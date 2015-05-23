
/* 
 * Create a visualisation of the gamut of a TIFF or JPEG file.
 *
 * Author:  Graeme W. Gill
 * Date:    00/9/20
 * Version: 1.00
 *
 * Copyright 2000, 2002, 2012 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/*
 * TTBD:
 *
 *		Should record abs/relative intent, PCS space used together
 *		with viewing conditions, so that a mismatch within
 *		icclink can be detected or allowed for.
 *
 *      Need to cope with profile not having black point.
 *
 *		How to cope with no profile, therefore no white or black point ?
 *
 *		Should we have a median filter option, to ignore small groups
 *		of extreme pixel values, rathar than total small numbers using -f ?
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <setjmp.h>
#include <string.h>
#include <math.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "tiffio.h"
#include "jpeglib.h"
#include "iccjpeg.h"
#include "icc.h"
#include "gamut.h"
#include "xicc.h"
#include "sort.h"
#include "vrml.h"
#include "ui.h"

#undef NOCAMGAM_CLIP		/* No clip to CAM gamut before CAM lookup */
#undef DEBUG				/* Dump filter cell contents */

#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif

void set_fminmax(double min[3], double max[3]);
void reset_filter();
void add_fpixel(double val[3]);
void flush_filter(int verb, gamut *gam, double filtperc);
void del_filter();

void usage(void) {
	int i;
	fprintf(stderr,"Create gamut surface of a TIFF or JPEG, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: tiffgamut [-v level] [profile.icm | embedded.tif/jpg] infile1.tif/jpg [infile2.tif/jpg ...] \n");
	fprintf(stderr," -v            Verbose\n");
	fprintf(stderr," -d sres       Surface resolution details 1.0 - 50.0\n");
	fprintf(stderr," -w            emit %s %s file as well as CGATS .gam file\n",vrml_format(),vrml_ext());
	fprintf(stderr," -n            Don't add %s axes or white/black point\n",vrml_format());
	fprintf(stderr," -k            Add %s markers for prim. & sec. \"cusp\" points\n",vrml_format());
	fprintf(stderr,"               (set env. ARGYLL_3D_DISP_FORMAT to VRML, X3D or X3DOM to change format)\n");
	fprintf(stderr," -f perc       Filter by popularity, perc = percent to use\n");
	fprintf(stderr," -i intent     p = perceptual, r = relative colorimetric,\n");
	fprintf(stderr,"               s = saturation, a = absolute (default), d = profile default\n");
//  fprintf(stderr,"               P = absolute perceptual, S = absolute saturation\n");
	fprintf(stderr," -p oride      l = Lab_PCS (default), j = %s Appearance Jab\n",icxcam_description(cam_default));
	fprintf(stderr," -o order      n = normal (priority: lut > matrix > monochrome)\n");
	fprintf(stderr,"               r = reverse (priority: monochrome > matrix > lut)\n");
	fprintf(stderr," -c viewcond   set appearance mode and viewing conditions for %s,\n",icxcam_description(cam_default));
	fprintf(stderr,"               either an enumerated choice, or a parameter:value changes\n");
	for (i = 0; ; i++) {
		icxViewCond vc;
		if (xicc_enum_viewcond(NULL, &vc, i, NULL, 1, NULL) == -999)
			break;

		fprintf(stderr,"           %s\n",vc.desc);
	}
	fprintf(stderr,"         s:surround    n = auto, a = average, m = dim, d = dark,\n");
	fprintf(stderr,"                       c = transparency (default average)\n");
	fprintf(stderr,"         w:X:Y:Z       Adapted white point as XYZ (default media white)\n");
	fprintf(stderr,"         w:x:y         Adapted white point as x, y\n");
	fprintf(stderr,"         a:adaptation  Adaptation luminance in cd.m^2 (default 50.0)\n");
	fprintf(stderr,"         b:background  Background %% of image luminance (default 20)\n");
	fprintf(stderr,"         l:imagewhite  Image white in cd.m^2 if surround = auto (default 250)\n");
	fprintf(stderr,"         f:flare       Flare light %% of image luminance (default 0)\n");
	fprintf(stderr,"         g:glare       Flare light %% of ambient (default 1)\n");
	fprintf(stderr,"         g:X:Y:Z       Flare color as XYZ (default media white, Abs: D50)\n");
	fprintf(stderr,"         g:x:y         Flare color as x, y\n");
	fprintf(stderr," -O outputfile Override the default output filename.\n");
	exit(1);
}

#define GAMRES 10.0		/* Default surface resolution */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Conversion functions from direct binary 0..n^2-1 == 0.0 .. 1.0 range */
/* to ICC luo input range. */
/* It is assumed that the binary has been sign corrected to be */
/* contiguous (ie CIELab). */

/* TIFF 8 bit CIELAB to standard L*a*b* */
/* Assume that a & b have been converted from signed to offset */
static void cvt_CIELAB8_to_Lab(double *out, double *in) {
	out[0] = in[0] * 100.0;
	out[1] = in[1] * 255.0 - 128.0;
	out[2] = in[2] * 255.0 - 128.0;
}

/* TIFF 16 bit CIELAB to standard L*a*b* */
/* Assume that a & b have been converted from signed to offset */
static void cvt_CIELAB16_to_Lab(double *out, double *in) {
	out[0] = in[0] * 100.0;
	out[1] = (in[1] - 32768.0/65535.0) * 256.0;
	out[2] = (in[2] - 32768.0/65535.0) * 256.0;
}

/* TIFF 8 bit ICCLAB to standard L*a*b* */
static void cvt_ICCLAB8_to_Lab(double *out, double *in) {
	out[0] = in[0] * 100.0;
	out[1] = (in[1] * 255.0) - 128.0;
	out[2] = (in[2] * 255.0) - 128.0;
}

/* TIFF 16 bit ICCLAB to standard L*a*b* */
static void cvt_ICCLAB16_to_Lab(double *out, double *in) {
	out[0] = in[0] * (100.0 * 65535.0)/65280.0;
	out[1] = (in[1] * (255.0 * 65535.0)/65280) - 128.0;
	out[2] = (in[2] * (255.0 * 65535.0)/65280) - 128.0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Convert a TIFF Photometric tag to an ICC colorspace. */
/* return 0 if not possible or applicable. */
icColorSpaceSignature 
TiffPhotometric2ColorSpaceSignature(
void (**icvt)(double *out, double *in),	/* Return read conversion function, NULL if none */
int *smsk,			/* Return signed handling mask, 0x0 if none */
int pmtc,			/* Input TIFF photometric */
int bps,			/* Input Bits per sample */
int spp,			/* Input Samples per pixel */
int extra			/* Extra Samples per pixel, if any */
) {
	if (icvt != NULL)
		*icvt = NULL;		/* Default return values */
	if (smsk != NULL)
		*smsk = 0x0;

	switch (pmtc) {
		case PHOTOMETRIC_MINISWHITE:	/* Subtractive Gray */
			return icSigGrayData;

		case PHOTOMETRIC_MINISBLACK:	/* Additive Gray */
			return icSigGrayData;

		case PHOTOMETRIC_RGB:
			return icSigRgbData;

		case PHOTOMETRIC_PALETTE:
			return 0x0;

		case PHOTOMETRIC_MASK:
			return 0x0;

		case PHOTOMETRIC_SEPARATED:
			/* Should look at the colorant names to figure out if this is CMY, CMYK */
			/* Should at least return both Cmy/3 or Cmyk/4 ! */
			switch(spp) {
				case 2:
					return icSig2colorData;
				case 3:
//					return icSig3colorData;
					return icSigCmyData;
				case 4:
//					return icSig4colorData;
					return icSigCmykData;
				case 5:
					return icSig5colorData;
				case 6:
					return icSig6colorData;
				case 7:
					return icSig7colorData;
				case 8:
					return icSig8colorData;
				case 9:
					return icSig9colorData;
				case 10:
					return icSig10colorData;
				case 11:
					return icSig11colorData;
				case 12:
					return icSig12colorData;
				case 13:
					return icSig13colorData;
				case 14:
					return icSig14colorData;
				case 15:
					return icSig15colorData;
			}

		case PHOTOMETRIC_YCBCR:
			return icSigYCbCrData;

		case PHOTOMETRIC_CIELAB:
			if (bps == 8) {
				if (icvt != NULL)
					*icvt = cvt_CIELAB8_to_Lab;
			} else {
				if (icvt != NULL)
					*icvt = cvt_CIELAB16_to_Lab;
			}
			*smsk = 0x6;				/* Treat a & b as signed */
			return icSigLabData;

		case PHOTOMETRIC_ICCLAB:
			if (bps == 8) {
				if (icvt != NULL)
					*icvt = cvt_ICCLAB8_to_Lab;
			} else {
				if (icvt != NULL)
					*icvt = cvt_ICCLAB16_to_Lab;
			}
			return icSigLabData;

		case PHOTOMETRIC_ITULAB:
			return 0x0;					/* Could add this with a conversion function */
										/* but have to allow for variable ITU gamut */
										/* (Tag 433, "Decode") */

		case PHOTOMETRIC_LOGL:
			return 0x0;					/* Could add this with a conversion function */

		case PHOTOMETRIC_LOGLUV:
			return 0x0;					/* Could add this with a conversion function */
	}
	return 0x0;
}


char *
Photometric2str(
int pmtc
) {
	static char buf[80];
	switch (pmtc) {
		case PHOTOMETRIC_MINISWHITE:
			return "Subtractive Gray";
		case PHOTOMETRIC_MINISBLACK:
			return "Additive Gray";
		case PHOTOMETRIC_RGB:
			return "RGB";
		case PHOTOMETRIC_PALETTE:
			return "Indexed";
		case PHOTOMETRIC_MASK:
			return "Transparency Mask";
		case PHOTOMETRIC_SEPARATED:
			return "Separated";
		case PHOTOMETRIC_YCBCR:
			return "YCbCr";
		case PHOTOMETRIC_CIELAB:
			return "CIELab";
		case PHOTOMETRIC_ICCLAB:
			return "ICCLab";
		case PHOTOMETRIC_ITULAB:
			return "ITULab";
		case PHOTOMETRIC_LOGL:
			return "CIELog2L";
		case PHOTOMETRIC_LOGLUV:
			return "CIELog2Luv";
	}
	sprintf(buf,"Unknown Photometric Tag %d",pmtc);
	return buf;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* JPEG error information */
typedef struct {
	jmp_buf env;		/* setjmp/longjmp environment */
	char message[JMSG_LENGTH_MAX];
} jpegerrorinfo;

/* JPEG error handler */
static void jpeg_error(j_common_ptr cinfo) {  
	jpegerrorinfo *p = (jpegerrorinfo *)cinfo->client_data;
	(*cinfo->err->format_message) (cinfo, p->message);
	longjmp(p->env, 1);
}

static char *
JPEG_cspace2str(
J_COLOR_SPACE cspace
) {
	static char buf[80];
	switch (cspace) {
		case JCS_UNKNOWN:
			return "Unknown";
		case JCS_GRAYSCALE:
			return "Monochrome";
		case JCS_RGB:
			return "RGB";
		case JCS_YCbCr:
			return "YCbCr";
		case JCS_CMYK:
			return "CMYK";
		case JCS_YCCK:
			return "YCCK";
	}
	sprintf(buf,"Unknown JPEG colorspace %d",cspace);
	return buf;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
int
main(int argc, char *argv[]) {
	int fa,nfa;					/* argument we're looking at */
	int ffa, lfa;				/* First, last input file argument */
	char prof_name[MAXNAMEL+1] = { '\000' };	/* ICC profile name, "" if none */
	char in_name[MAXNAMEL+1];			/* TIFF input file */
	char *xl = NULL, out_name[MAXNAMEL+4+1] = { '\000' };	/* VRML/X3D output file */
	int verb = 0;
	int vrml = 0;
	int doaxes = 1;
	int docusps = 0;
	int filter = 0;
	double filtperc = 100.0;
	int rv = 0;

	icc *icco = NULL;
	xicc *xicco = NULL;
	icxcam *cam = NULL;			/* Separate CAM used for Lab TIFF files */
	icxViewCond vc;				/* Viewing Condition for CIECAM */
	int vc_e = -1;				/* Enumerated viewing condition */
	int vc_s = -1;				/* Surround override */
	double vc_wXYZ[3] = {-1.0, -1.0, -1.0};	/* Adapted white override in XYZ */
	double vc_wxy[2] = {-1.0, -1.0};		/* Adapted white override in x,y */
	double vc_a = -1.0;			/* Adapted luminance */
	double vc_b = -1.0;			/* Background % overid */
	double vc_l = -1.0;			/* Scene luminance override */
	double vc_f = -1.0;			/* Flare % overide */
	double vc_g = -1.0;			/* Glare % overide */
	double vc_gXYZ[3] = {-1.0, -1.0, -1.0};	/* Glare color override in XYZ */
	double vc_gxy[2] = {-1.0, -1.0};		/* Glare color override in x,y */
	icxLuBase *luo = NULL;					/* Generic lookup object */
	icColorSpaceSignature ins = icSigLabData, outs;	/* Type of input and output spaces */
	int inn, outn;						/* Number of components */
	icmLuAlgType alg;					/* Type of lookup algorithm */
	icmLookupFunc     func   = icmFwd;				/* Must be */
	icRenderingIntent intent = -1;					/* Default */
	icColorSpaceSignature pcsor = icSigLabData;		/* Default */
	icmLookupOrder    order  = icmLuOrdNorm;		/* Default */

	/* TIFF */
	TIFFErrorHandler olderrh, oldwarnh;
	TIFFErrorHandlerExt olderrhx, oldwarnhx;
	TIFF *rh = NULL;
	int x, y, width, height;					/* Size of image */
	uint16 samplesperpixel, bitspersample;
	uint16 pconfig, photometric, pmtc;
	tdata_t *inbuf;
	int inbpix = 0;               					/* Number of pixels in jpeg in buf */
	void (*cvt)(double *out, double *in) = NULL;	/* TIFF conversion function, NULL if none */
	icColorSpaceSignature tcs = 0;				/* TIFF colorspace */
	uint16 extrasamples = 0;					/* Extra "alpha" samples */
	uint16 *extrainfo = NULL;					/* Info about extra samples */
	int sign_mask = 0;							/* Handling of encoding sign */

	/* JPEG */
	jpegerrorinfo jpeg_rerr;
	FILE *rf = NULL;
	struct jpeg_decompress_struct rj;
	struct jpeg_error_mgr jerr;
	int iinv;							/* Invert the input */

	double gamres = GAMRES;				/* Surface resolution */
	gamut *gam;

	double apcsmin[3], apcsmax[3];		/* Actual PCS range */

	error_program = argv[0];

	if (argc < 2)
		usage();

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')	{	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage();

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}

			/* Intent */
			else if (argv[fa][1] == 'i' || argv[fa][1] == 'I') {
				fa = nfa;
				if (na == NULL) usage();
    			switch (na[0]) {
					case 'd':
						intent = icmDefaultIntent;
						break;
					case 'a':
						intent = icAbsoluteColorimetric;
						break;
					case 'p':
						intent = icPerceptual;
						break;
					case 'r':
						intent = icRelativeColorimetric;
						break;
					case 's':
						intent = icSaturation;
						break;
					/* Argyll special intents to check spaces underlying */
					/* icxPerceptualAppearance & icxSaturationAppearance */
					case 'P':
						intent = icmAbsolutePerceptual;
						break;
					case 'S':
						intent = icmAbsoluteSaturation;
						break;
					default:
						usage();
				}
			}

			/* Search order */
			else if (argv[fa][1] == 'o') {
				fa = nfa;
				if (na == NULL) usage();
    			switch (na[0]) {
					case 'n':
					case 'N':
						order = icmLuOrdNorm;
						break;
					case 'r':
					case 'R':
						order = icmLuOrdRev;
						break;
					default:
						usage();
				}
			}

			/* PCS override */
			else if (argv[fa][1] == 'p' || argv[fa][1] == 'P') {
				fa = nfa;
				if (na == NULL) usage();
    			switch (na[0]) {
					case 'l':
						pcsor = icSigLabData;
						break;
					case 'j':
						pcsor = icxSigJabData;
						break;
					default:
						usage();
				}
			}

			/* Viewing conditions */
			else if (argv[fa][1] == 'c' || argv[fa][1] == 'C') {
				fa = nfa;
				if (na == NULL) usage();

				/* Switch to Jab automatically */
				pcsor = icxSigJabData;

				/* Set the viewing conditions */
				if (na[1] != ':') {
					if ((vc_e = xicc_enum_viewcond(NULL, NULL, -2, na, 1, NULL)) == -999)
						usage();
				} else if (na[0] == 's' || na[0] == 'S') {
					if (na[1] != ':')
						usage();
					if (na[2] == 'n' || na[2] == 'N') {
						vc_s = vc_none;		/* Automatic */
					} else if (na[2] == 'a' || na[2] == 'A') {
						vc_s = vc_average;
					} else if (na[2] == 'm' || na[2] == 'M') {
						vc_s = vc_dim;
					} else if (na[2] == 'd' || na[2] == 'D') {
						vc_s = vc_dark;
					} else if (na[2] == 'c' || na[2] == 'C') {
						vc_s = vc_cut_sheet;
					} else
						usage();
				} else if (na[0] == 'w' || na[0] == 'W') {
					double x, y, z;
					if (sscanf(na+1,":%lf:%lf:%lf",&x,&y,&z) == 3) {
						vc_wXYZ[0] = x; vc_wXYZ[1] = y; vc_wXYZ[2] = z;
					} else if (sscanf(na+1,":%lf:%lf",&x,&y) == 2) {
						vc_wxy[0] = x; vc_wxy[1] = y;
					} else
						usage();
				} else if (na[0] == 'a' || na[0] == 'A') {
					if (na[1] != ':')
						usage();
					vc_a = atof(na+2);
				} else if (na[0] == 'b' || na[0] == 'B') {
					if (na[1] != ':')
						usage();
					vc_b = atof(na+2);
				} else if (na[0] == 'l' || na[0] == 'L') {
					if (na[1] != ':')
						usage();
					vc_l = atof(na+2);
				} else if (na[0] == 'f' || na[0] == 'F') {
					if (na[1] != ':')
						usage();
					vc_f = atof(na+2);
				} else if (na[0] == 'g' || na[0] == 'G') {
					double x, y, z;
					if (sscanf(na+1,":%lf:%lf:%lf",&x,&y,&z) == 3) {
						vc_gXYZ[0] = x; vc_gXYZ[1] = y; vc_gXYZ[2] = z;
					} else if (sscanf(na+1,":%lf:%lf",&x,&y) == 2) {
						vc_gxy[0] = x; vc_gxy[1] = y;
					} else if (sscanf(na+1,":%lf",&x) == 1) {
						vc_g = x;
					} else
						usage();
				} else
					usage();
			}

			/* VRML/X3D output */
			else if (argv[fa][1] == 'w' || argv[fa][1] == 'W') {
				vrml = 1;
			}
			/* No axis output */
			else if (argv[fa][1] == 'n' || argv[fa][1] == 'N') {
				doaxes = 0;
			}
			/* Do cusp markers */
			else if (argv[fa][1] == 'k' || argv[fa][1] == 'K') {
				docusps = 1;
			}
			/* Surface Detail */
			else if (argv[fa][1] == 'd' || argv[fa][1] == 'D') {
				fa = nfa;
				if (na == NULL) usage();
				gamres = atof(na);
			}
			/* Filtering */
			else if (argv[fa][1] == 'f' || argv[fa][1] == 'F') {
				fa = nfa;
				if (na == NULL) usage();
				filtperc = atof(na);
				if (filtperc < 0.0 || filtperc> 100.0)
					usage();
				filter = 1;
			}

			/* Output file name */
			else if (argv[fa][1] == 'O') {
				fa = nfa;
				if (na == NULL) usage();
				strncpy(out_name,na,MAXNAMEL); out_name[MAXNAMEL] = '\000';
			}

			else 
				usage();
		} else
			break;
	}

	if (fa >= argc || argv[fa][0] == '-') usage();
	if (fa < (argc-1))
		strncpy(prof_name,argv[fa++],MAXNAMEL); prof_name[MAXNAMEL] = '\000';

	for (ffa = fa; fa < argc; fa++)
		if (fa >= argc || argv[fa][0] == '-') usage();
	lfa = fa-1;
	if (verb)
		printf("There are %d raster files to process\n",lfa - ffa + 1);

	if (out_name[0] == '\000') {
		strncpy(out_name,argv[lfa],MAXNAMEL); out_name[MAXNAMEL] = '\000';
		if ((xl = strrchr(out_name, '.')) == NULL)	/* Figure where extention is */
			xl = out_name + strlen(out_name);
		strcpy(xl,".gam");
	} else {
		if ((xl = strrchr(out_name, '.')) == NULL)	/* Figure where extention is */
			xl = out_name + strlen(out_name);
	}

	if (verb)
		printf("Gamut output files is '%s'\n",out_name);

	if (intent == -1) {
		if (pcsor == icxSigJabData)
			intent = icRelativeColorimetric;	/* Default to icxAppearance */
		else
			intent = icAbsoluteColorimetric;	/* Default to icAbsoluteColorimetric */
	}

	/* - - - - - - - - - - - - - - - - */
	/* If we were provided an ICC profile to use */
	if (prof_name[0] != '\000') {

		/* Open up the profile or TIFF embedded profile for reading */
		if ((icco = read_embedded_icc(prof_name)) == NULL)
			error ("Can't open profile in file '%s'",prof_name);
	
		if (verb) {
			icmFile *op;
			if ((op = new_icmFileStd_fp(stdout)) == NULL)
				error ("Can't open stdout");
			icco->header->dump(icco->header, op, 1);
			op->del(op);
		}
	
		/* Check that the profile is appropriate */
		if (icco->header->deviceClass != icSigInputClass
		 && icco->header->deviceClass != icSigDisplayClass
		 && icco->header->deviceClass != icSigOutputClass
		 && icco->header->deviceClass != icSigColorSpaceClass)
			error("Profile type isn't device or colorspace");
	
		/* Wrap with an expanded icc */
		if ((xicco = new_xicc(icco)) == NULL)
			error ("Creation of xicc failed");
	
		/* Setup the default viewing conditions */
		if (xicc_enum_viewcond(xicco, &vc, -1, NULL, 0, NULL) == -999)
			error ("%d, %s",xicco->errc, xicco->err);
	
		if (vc_e != -1)
			if (xicc_enum_viewcond(xicco, &vc, vc_e, NULL, 0, NULL) == -999)
				error ("%d, %s",xicco->errc, xicco->err);
		if (vc_s >= 0)
			vc.Ev = vc_s;
		if (vc_wXYZ[1] > 0.0) {
			/* Normalise it to current media white */
			vc.Wxyz[0] = vc_wXYZ[0]/vc_wXYZ[1] * vc.Wxyz[1];
			vc.Wxyz[2] = vc_wXYZ[2]/vc_wXYZ[1] * vc.Wxyz[1];
		} 
		if (vc_wxy[0] >= 0.0) {
			double x = vc_wxy[0];
			double y = vc_wxy[1];	/* If Y == 1.0, then X+Y+Z = 1/y */
			double z = 1.0 - x - y;
			vc.Wxyz[0] = x/y * vc.Wxyz[1];
			vc.Wxyz[2] = z/y * vc.Wxyz[1];
		}
		if (vc_a >= 0.0)
			vc.La = vc_a;
		if (vc_b >= 0.0)
			vc.Yb = vc_b/100.0;
		if (vc_l >= 0.0)
			vc.Lv = vc_l;

		if (vc_f >= 0.0)
			vc.Yf = vc_f/100.0;
		if (vc_g >= 0.0)
			vc.Yg = vc_g/100.0;
		if (vc_gXYZ[1] > 0.0) {
			/* Normalise it to current media white */
			vc.Gxyz[0] = vc_gXYZ[0]/vc_gXYZ[1] * vc.Gxyz[1];
			vc.Gxyz[2] = vc_gXYZ[2]/vc_gXYZ[1] * vc.Gxyz[1];
		}
		if (vc_gxy[0] >= 0.0) {
			double x = vc_gxy[0];
			double y = vc_gxy[1];	/* If Y == 1.0, then X+Y+Z = 1/y */
			double z = 1.0 - x - y;
			vc.Gxyz[0] = x/y * vc.Gxyz[1];
			vc.Gxyz[2] = z/y * vc.Gxyz[1];
		}
	
		/* Get a expanded color conversion object */
		if ((luo = xicco->get_luobj(xicco, ICX_CLIP_NEAREST
#ifdef NOCAMGAM_CLIP
		             | ICX_CAM_NOGAMCLIP
#endif
		           , func, intent, pcsor, order, &vc, NULL)) == NULL)
			error ("%d, %s",xicco->errc, xicco->err);
	
		luo->spaces(luo, &ins, &inn, &outs, &outn, &alg, NULL, NULL, NULL);

	/* Else if the image is in Lab space, and we want Jab */
	} else if (pcsor == icxSigJabData) {
		double wp[3];		/* D50 white point */

		wp[0] = icmD50.X;
		wp[1] = icmD50.Y;
		wp[2] = icmD50.Z;

		/* Setup the default viewing conditions */
		if (xicc_enum_viewcond(NULL, &vc, -1, NULL, 0, wp) == -999)
			error("Failed to locate a default viewing condition");
	
		if (vc_e != -1)
			if (xicc_enum_viewcond(NULL, &vc, vc_e, NULL, 0, wp) == -999)
				error("Failed to enumerate viewing condition %d",vc_e);
		if (vc_s >= 0)
			vc.Ev = vc_s;
		if (vc_wXYZ[1] > 0.0) {
			/* Normalise it to current media white */
			vc.Wxyz[0] = vc_wXYZ[0]/vc_wXYZ[1] * vc.Wxyz[1];
			vc.Wxyz[2] = vc_wXYZ[2]/vc_wXYZ[1] * vc.Wxyz[1];
		} 
		if (vc_wxy[0] >= 0.0) {
			double x = vc_wxy[0];
			double y = vc_wxy[1];	/* If Y == 1.0, then X+Y+Z = 1/y */
			double z = 1.0 - x - y;
			vc.Wxyz[0] = x/y * vc.Wxyz[1];
			vc.Wxyz[2] = z/y * vc.Wxyz[1];
		}
		if (vc_a >= 0.0)
			vc.La = vc_a;
		if (vc_b >= 0.0)
			vc.Yb = vc_b/100.0;
		if (vc_f >= 0.0)
			vc.Yf = vc_f/100.0;
		if (vc_g >= 0.0)
			vc.Yg = vc_g/100.0;
		if (vc_gXYZ[1] > 0.0) {
			/* Normalise it to current media white */
			vc.Gxyz[0] = vc_gXYZ[0]/vc_gXYZ[1] * vc.Gxyz[1];
			vc.Gxyz[2] = vc_gXYZ[2]/vc_gXYZ[1] * vc.Gxyz[1];
		}
		if (vc_gxy[0] >= 0.0) {
			double x = vc_gxy[0];
			double y = vc_gxy[1];	/* If Y == 1.0, then X+Y+Z = 1/y */
			double z = 1.0 - x - y;
			vc.Gxyz[0] = x/y * vc.Gxyz[1];
			vc.Gxyz[2] = z/y * vc.Gxyz[1];
		}
	
		if ((cam = new_icxcam(cam_default)) == NULL)
			error("new_icxcam failed");

		cam->set_view(cam, vc.Ev, vc.Wxyz, vc.La, vc.Yb, vc.Lv, vc.Yf, vc.Yg, vc.Gxyz,
		              XICC_USE_HK);
	}

	/* Establish the PCS range if we are filtering */
	if (filter) {
		double pcsmin[3], pcsmax[3];		/* PCS range for filter stats array */
	
		if (luo) {
			gamut *csgam;

			if ((csgam = luo->get_gamut(luo, 20.0)) == NULL)
				error("Getting the gamut of the source colorspace failed");
			
			csgam->getrange(csgam, pcsmin, pcsmax);
			csgam->del(csgam);
		} else {
			pcsmin[0] = -10.0;
			pcsmax[0] = 110.0;
			pcsmin[1] = -138.0;
			pcsmax[1] = 138.0;
			pcsmin[2] = -138.0;
			pcsmax[2] = 138.0;

#ifdef NEVER	/* Does this make any sense ?? */
			if (cam) {
				icmLab2XYZ(&icmD50, pcsmin, pcsmin);
				cam->XYZ_to_cam(cam, pcsmin, pcsmin);

				icmLab2XYZ(&icmD50, pcsmax, pcsmax);
				cam->XYZ_to_cam(cam, pcsmax, pcsmax);
			}
#endif
		}

		if (verb)
			printf("PCS range = %f..%f, %f..%f. %f..%f\n\n", pcsmin[0], pcsmax[0], pcsmin[1], pcsmax[1], pcsmin[2], pcsmax[2]);

		/* Allocate and initialize the filter */
		set_fminmax(pcsmin, pcsmax);
	}

	/* - - - - - - - - - - - - - - - */
	/* Creat a raster gamut surface */
	gam = new_gamut(gamres, pcsor == icxSigJabData, 1);

	apcsmin[0] = apcsmin[1] = apcsmin[2] = 1e6;
	apcsmax[0] = apcsmax[1] = apcsmax[2] = -1e6;

	/* Process all the tiff files */
	for (fa = ffa; fa <= lfa; fa++) {

		/* - - - - - - - - - - - - - - - */
		/* Open up input tiff/jpeg file ready for reading */
		/* Got arguments, so setup to process the file */
		strncpy(in_name,argv[fa],MAXNAMEL); in_name[MAXNAMEL] = '\000';

		olderrh = TIFFSetErrorHandler(NULL);
		oldwarnh = TIFFSetWarningHandler(NULL);
		olderrhx = TIFFSetErrorHandlerExt(NULL);
		oldwarnhx = TIFFSetWarningHandlerExt(NULL);
		
		if ((rh = TIFFOpen(in_name, "r")) != NULL) {
			TIFFSetErrorHandler(olderrh);
			TIFFSetWarningHandler(oldwarnh);
			TIFFSetErrorHandlerExt(olderrhx);
			TIFFSetWarningHandlerExt(oldwarnhx);

			TIFFGetField(rh, TIFFTAG_IMAGEWIDTH,  &width);
			TIFFGetField(rh, TIFFTAG_IMAGELENGTH, &height);

			TIFFGetField(rh, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
			TIFFGetField(rh, TIFFTAG_BITSPERSAMPLE, &bitspersample);
			if (bitspersample != 8 && bitspersample != 16)
				error("TIFF Input file must be 8 or 16 bits/channel");

			TIFFGetFieldDefaulted(rh, TIFFTAG_EXTRASAMPLES, &extrasamples, &extrainfo);
			TIFFGetField(rh, TIFFTAG_PHOTOMETRIC, &photometric);

			if (luo != NULL) {
				if (inn != (samplesperpixel-extrasamples))
					error("TIFF Input file has %d input chanels and is mismatched to colorspace '%s'",
					       samplesperpixel, icm2str(icmColorSpaceSignature, ins));
			}

			if ((tcs = TiffPhotometric2ColorSpaceSignature(&cvt, &sign_mask, photometric,
			                                     bitspersample, samplesperpixel, extrasamples)) == 0)
				error("Can't handle TIFF file photometric %s", Photometric2str(photometric));

			if (tcs != ins) {
				if (luo != NULL)
					error("TIFF photometric '%s' doesn't match ICC input colorspace '%s' !",
					      Photometric2str(photometric), icm2str(icmColorSpaceSignature,ins));
				else
					error("No profile provided and TIFF photometric '%s' isn't Lab !",
					      Photometric2str(photometric));
			}

			TIFFGetField(rh, TIFFTAG_PLANARCONFIG, &pconfig);
			if (pconfig != PLANARCONFIG_CONTIG)
				error("TIFF Input file must be planar");

			if (verb) {
				printf("Input TIFF file '%s'\n",in_name);
				printf("TIFF file photometric is %s\n",Photometric2str(photometric));
				printf("TIFF file colorspace is %s\n",icm2str(icmColorSpaceSignature,tcs));
				printf("File size %d x %d pixels\n",width,height);
				printf("\n");
			}

		} else {
			TIFFSetErrorHandler(olderrh);
			TIFFSetWarningHandler(oldwarnh);
			TIFFSetErrorHandlerExt(olderrhx);
			TIFFSetWarningHandlerExt(oldwarnhx);

			/* We cope with the horrible ijg jpeg library error handling */
			/* by using a setjmp/longjmp. */
			jpeg_std_error(&jerr);
			jerr.error_exit = jpeg_error;
			if (setjmp(jpeg_rerr.env)) {
				jpeg_destroy_decompress(&rj);
				fclose(rf);
				error("Unable to read JPEG file '%s'\n",in_name);
			}

			rj.err = &jerr;
			rj.client_data = &jpeg_rerr;
			jpeg_create_decompress(&rj);

#if defined(O_BINARY) || defined(_O_BINARY)
		    if ((rf = fopen(in_name,"rb")) == NULL)
#else
		    if ((rf = fopen(in_name,"r")) == NULL)
#endif
			{
				jpeg_destroy_decompress(&rj);
				error("Unable to open input file '%s'\n",in_name);
			}
			
			jpeg_stdio_src(&rj, rf);
			jpeg_read_header(&rj, TRUE); /* we'll longjmp on error */

			bitspersample = rj.data_precision;
			if (bitspersample != 8 && bitspersample != 16) {
				error("JPEG Input file must be 8 or 16 bit/channel");
			}

			/* No extra samples */
			extrasamples = 0;
			iinv = 0;
				
			switch (rj.jpeg_color_space) {
				case JCS_GRAYSCALE:
					rj.out_color_space = JCS_GRAYSCALE;
					tcs = icSigGrayData;
					samplesperpixel = 1;
					break;

				case JCS_YCbCr:		/* get libjpg to convert to RGB */
					rj.out_color_space = JCS_RGB;
					tcs = icSigRgbData;
					samplesperpixel = 3;
					break;

				case JCS_RGB:
					rj.out_color_space = JCS_RGB;
					tcs = icSigRgbData;
					samplesperpixel = 3;
					break;

				case JCS_YCCK:		/* libjpg to convert to CMYK */
					rj.out_color_space = JCS_CMYK;
					tcs = icSigCmykData;
					samplesperpixel = 4;
					if (rj.saw_Adobe_marker)
						iinv = 1;
					break;

				case JCS_CMYK:
					rj.out_color_space = JCS_CMYK;
					tcs = icSigCmykData;
					samplesperpixel = 4;
					if (rj.saw_Adobe_marker)	/* Adobe inverts CMYK */
						iinv = 1;
					break;

				default:
					error("Can't handle JPEG file '%s' colorspace 0x%x", in_name, rj.jpeg_color_space);
			}

			if (luo != NULL) {
				if (inn != samplesperpixel)
					error ("JPEG Input file has %d input chanels and is mismatched to colorspace '%s'",
					       samplesperpixel, icm2str(icmColorSpaceSignature, ins));
			}

			if (tcs != ins) {
				if (luo != NULL)
					error("JPEG colorspace '%s' doesn't match ICC input colorspace '%s' !",
					      icm2str(icmColorSpaceSignature, tcs), icm2str(icmColorSpaceSignature,ins));
				else
					error("No profile provided and JPEG colorspace '%s' isn't Lab !",
					      icm2str(icmColorSpaceSignature, tcs));
			}
			jpeg_calc_output_dimensions(&rj);
			width = rj.output_width;
			height = rj.output_height;

			if (verb) {
				printf("Input JPEG file '%s'\n",in_name);
				printf("JPEG file original colorspace is %s\n",JPEG_cspace2str(rj.jpeg_color_space));
				printf("JPEG file colorspace is %s\n",icm2str(icmColorSpaceSignature,tcs));
				printf("File size %d x %d pixels\n",width,height);
				printf("\n");
			}
			jpeg_start_decompress(&rj);
		}

		/* - - - - - - - - - - - - - - - */
		/* Process colors to translate */
		/* (Should fix this to process a group of lines at a time ?) */

		if (rh != NULL)
			inbuf  = _TIFFmalloc(TIFFScanlineSize(rh));
		else {
			inbpix = rj.output_width * rj.num_components;
			if ((inbuf = (tdata_t *)malloc(inbpix)) == NULL)
				error("Malloc failed on input line buffer");

			if (setjmp(jpeg_rerr.env)) {
				/* Something went wrong with reading the file */
				jpeg_destroy_decompress(&rj);
				error("failed to read JPEG line of file '%s' [%s]",in_name, jpeg_rerr.message);
			}
		}

		for (y = 0; y < height; y++) {

			/* Read in the next line */
			if (rh) {
				if (TIFFReadScanline(rh, inbuf, y, 0) < 0)
					error ("Failed to read TIFF line %d",y);
			} else {
				jpeg_read_scanlines(&rj, (JSAMPARRAY)&inbuf, 1);
				if (iinv) {
					unsigned char *cp, *ep = (unsigned char *)inbuf + inbpix;
					for (cp = (unsigned char *)inbuf; cp < ep; cp++)
						*cp = ~*cp;
				}
			}

			/* Do floating point conversion */
			for (x = 0; x < width; x++) {
				int i;
				double in[MAX_CHAN], out[MAX_CHAN];
				
//printf("~1 location %d,%d\n",x,y);
				if (bitspersample == 8) {
					for (i = 0; i < samplesperpixel; i++) {
						int v = ((unsigned char *)inbuf)[x * samplesperpixel + i];
//printf("~1 v[%d] = %d\n",i,v);
						if (sign_mask & (1 << i))		/* Treat input as signed */
							v = (v & 0x80) ? v - 0x80 : v + 0x80;
//printf("~1 signed v[%d] = %d\n",i,v);
						in[i] = v/255.0;
//printf("~1 in[%d] = %f\n",i,in[i]);
					}
				} else {
					for (i = 0; i < samplesperpixel; i++) {
						int v = ((unsigned short *)inbuf)[x * samplesperpixel + i];
//printf("~1 v[%d] = %d\n",i,v);
						if (sign_mask & (1 << i))		/* Treat input as signed */
							v = (v & 0x8000) ? v - 0x8000 : v + 0x8000;
//printf("~1 signed v[%d] = %d\n",i,v);
						in[i] = v/65535.0;
//printf("~1 in[%d] = %f\n",i,in[i]);
					}
				}
				if (cvt != NULL) {	/* Undo TIFF encoding */
					cvt(in, in);
				}
				/* ICC profile to convert RGB to Lab or Jab */
				if (luo != NULL) {
//printf("~1 RGB in value = %f %f %f\n",in[0],in[1],in[2]);
					if ((rv = luo->lookup(luo, out, in)) > 1)
						error ("%d, %s",icco->errc,icco->err);
//printf("~1 after luo = %f %f %f\n",out[0],out[1],out[2]);
					
					if (outs == icSigXYZData) {	/* Convert to Lab */
						icmXYZ2Lab(&icco->header->illuminant, out, out);
//printf("~1 after XYZ2Lab = %f %f %f\n",out[0],out[1],out[2]);
					}
				/* Lab TIFF - may need to convert to Jab */
				} else if (cam != NULL) {
//printf("~1 Lab in value = %f %f %f\n",in[0],in[1],in[2]);
					icmLab2XYZ(&icmD50, out, in);
//printf("~1 XYZ = %f %f %f\n",out[0],out[1],out[2]);
					cam->XYZ_to_cam(cam, out, out);
//printf("~1 Jab = %f %f %f\n",out[0],out[1],out[2]);

				} else {
//printf("~1 Lab value = %f %f %f\n",in[0],in[1],in[2]);
					for (i = 0; i < samplesperpixel; i++)
						out[i] = in[i];
				}

				for (i = 0; i < 3; i++) {
					if (out[i] < apcsmin[i]) {
//printf("~1 new min %f\n",out[i]);
						apcsmin[i] = out[i];
					}
					if (out[i] > apcsmax[i]) {
//printf("~1 new max %f\n",out[i]);
						apcsmax[i] = out[i];
					}
				}
				if (filter)
					add_fpixel(out);
				else
					gam->expand(gam, out);
			}
		}

		/* Release buffers and close files */
		if (rh != NULL) {
			if (inbuf != NULL)
				_TIFFfree(inbuf);
			TIFFClose(rh); /* Close Input file */
		} else {
			jpeg_finish_decompress(&rj);
			jpeg_destroy_decompress(&rj);
			if (inbuf != NULL)
				free(inbuf);
			if (fclose(rf))
				error("Error closing JPEG input file '%s'\n",in_name);
		}

		/* If filtering, flush filtered points to the gamut */
		if (filter) {
			flush_filter(verb, gam, filtperc);
		}
	}

	if (verb)
		printf("Actual PCS range = %f..%f, %f..%f. %f..%f\n\n", apcsmin[0], apcsmax[0], apcsmin[1], apcsmax[1], apcsmin[2], apcsmax[2]);

	if (filter)
		del_filter();
	
	/* Get White and Black points from the profile, and set them in the gamut. */
	if (luo != NULL) {
		double wp[3], bp[3], kp[3];

		luo->efv_wh_bk_points(luo, wp, bp,kp);
		gam->setwb(gam, wp, bp, kp);
	} else {
		double wp[3] = { 100.0, 0.0, 0.0 };
		double bp[3] = {   0.0, 0.0, 0.0 };
		gam->setwb(gam, wp, bp, bp);
	}

	/* Done with lookup object */
	if (luo != NULL) {
		luo->del(luo);
		xicco->del(xicco);		/* Expansion wrapper */
		icco->del(icco);		/* Icc */
	}
	if (cam != NULL) {
		cam->del(cam);
	}

	if (verb)
		printf("Output Gamut file '%s'\n",out_name);

	/* Create the VRML/X3D file */
	if (gam->write_gam(gam,out_name))
		error ("write gamut failed on '%s'",out_name);

	if (vrml) {
		xl[0] = '\000';
		printf("Output %s file '%s%s'\n",vrml_format(),out_name,vrml_ext());
		if (gam->write_vrml(gam,out_name, doaxes, docusps))
			error ("write vrml failed on '%s'",out_name);
	}

	if (verb) {
		printf("Total volume of gamut is %f cubic colorspace units\n",gam->volume(gam));
	}
	gam->del(gam);

	return 0;
}


/* ============================================================================= */
/* A pixel value filter module. We quantize the pixel values and keep statistics */
/* on them, so as to filter out low frequency colors. */

#define FILTBITS 6		/* Total = 2 ^ (3 * FILTBITS) entries  = 33Mbytes*/
#define FILTSIZE (1 << FILTBITS)

/* A filtered cell entry */
typedef struct {
	int   count;		/* Count of pixels that fall in this cell */
	float pcs[3];		/* Most extreme PCS value that fell in this cell */
} fent;

struct _ffilter {
	double min[3], max[3];		/* PCS range */

	fent cells[FILTSIZE][FILTSIZE][FILTSIZE];		/* Quantized pixels stats */
	fent *scells[FILTSIZE * FILTSIZE * FILTSIZE];	/* Sorted order */
	
}; typedef struct _ffilter ffilter;



/* Use a global object */
ffilter *ff = NULL;

/* Set the min and max values and init the filter */
void set_fminmax(double min[3], double max[3]) {

	if (ff == NULL) {
	    if ((ff = (ffilter *) calloc(1,sizeof(ffilter))) == NULL)
	        error("ffilter: calloc failed");
	}

	ff->min[0] = min[0];
	ff->min[1] = min[1];
	ff->min[2] = min[2];
	ff->max[0] = max[0];
	ff->max[1] = max[1];
	ff->max[2] = max[2];
}

/* Add another pixel to the filter */
void add_fpixel(double val[3]) {
	int j;
	int qv[3];
	fent *fe;
	double cent[3] = { 50.0, 0.0, 0.0 }; 	/* Assumed center */
	double tt, cdist, ndist;

	if (ff == NULL)
		error("ffilter not initialized");

	/* Quantize the values */
	for (j = 0; j < 3; j++) {
		double vv;

		vv = (val[j] - ff->min[j])/(ff->max[j] - ff->min[j]);
		qv[j] = (int)(vv * (FILTSIZE - 1) + 0.5);
		if (qv[j] < 0)
			qv[j] = 0;
		else if (qv[j] >= FILTSIZE)
			qv[j] = FILTSIZE-1; 
	}

//printf("~1 color %f %f %f -> Cell %d %d %d\n", val[0], val[1], val[2], qv[0], qv[1], qv[2]);

	/* Find the appropriate cell */
	fe = &ff->cells[qv[0]][qv[1]][qv[2]];

	/* See if this is the new most distant value in this cell */
	for (cdist = ndist = 0.0, j = 0; j < 3; j++) {
		tt = fe->pcs[j] - cent[j];
		cdist += tt * tt;
		tt = val[j] - cent[j];
		ndist += tt * tt;
	}
	if (fe->count == 0 || ndist > cdist) {
		fe->pcs[0] = val[0];
		fe->pcs[1] = val[1];
		fe->pcs[2] = val[2];
//printf("Updated pcs to %f %f %f\n", val[0],val[1],val[2]);
	}
	fe->count++;
//printf("Cell count = %d\n",fe->count);
}

/* Flush the filter contents to the gamut, and then reset it */
void flush_filter(int verb, gamut *gam, double filtperc) {
	int i, j;
	int totcells = FILTSIZE * FILTSIZE * FILTSIZE;
	int used, hasone;
	double cuml, avgcnt;

	if (ff == NULL)
		error("ffilter not initialized");

	/* Sort the cells by popularity from most to least */
	for (used = hasone = avgcnt = i = 0; i < totcells; i++) {
		ff->scells[i] = (fent *)ff->cells + i;
		if (ff->scells[i]->count > 0) {
			used++;
			if (ff->scells[i]->count == 1)
				hasone++;
			avgcnt += ff->scells[i]->count;
		}
	}
	avgcnt /= used; 

#define HEAP_COMPARE(A,B) A->count > B->count 
	HEAPSORT(fent *,ff->scells, totcells)

#ifdef DEBUG
	for (i = 0; i < totcells; i++) {
		if (ff->scells[i]->count > 0) {
			printf("Cell %d at %f %f %f count %d\n",
			        i,
			        ff->scells[i]->pcs[0],
			        ff->scells[i]->pcs[1],
			        ff->scells[i]->pcs[2],
			        ff->scells[i]->count);
		}
	}
#endif
	if (verb) {
		printf("Total of %d cells out of %d were hit (%.1f%%)\n",used,totcells,used * 100.0/totcells);
		printf("%.1f%% have a count of 1\n",hasone * 100.0/used);
		printf("Average cell count = %f\n",avgcnt);
		printf("\n");
	}

	/* Add the populated cells in order */
	filtperc /= 100.0;
	for (cuml = 0.0, i = j = 0; cuml < filtperc && i < totcells; i++) {
		
		if (ff->scells[i]->count > 0) {
			double val[3];
			val[0] = ff->scells[i]->pcs[0];	/* float -> double */
			val[1] = ff->scells[i]->pcs[1];
			val[2] = ff->scells[i]->pcs[2];
#ifdef DEBUG
			printf("Adding %d: %f %f %f count %d to gamut\n", i, val[0], val[1], val[2],ff->scells[i]->count);
#endif
			gam->expand(gam, val);
			j++;
			cuml = j/(used-1.0);
		}

	}

	/* Reset it to empty */
	{
		double min[3], max[3];

		for (j = 0; j < 3; j++) {
			min[j] = ff->min[j];
			max[j] = ff->max[j];
		}
		memset(ff, 0, sizeof(ffilter));
		for (j = 0; j < 3; j++) {
			ff->min[j] = min[j];
			ff->max[j] = max[j];
		}
	}
}

/* Free up the filter structure */
void del_filter() {

	free(ff);
}
