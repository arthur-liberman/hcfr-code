
/* 
 * Create a gamut mapping locus set from a TIFF file.
 *
 * Author:  Graeme W. Gill
 * Date:    08/10/14
 * Version: 1.00
 *
 * Copyright 2000, 2008 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Derived from tiffgamut.c
 *
 * This program creates a locus of test values that spans the
 * volume of colors ocupied by the pixels of the TIFF file.
 * The direction that spans the greatest distance is
 * turned into a line of source test points, that can
 * then be used by gammap to illustrate how the gamut mapping
 * alters the locus.
 */

/*
 * TTBD:
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "tiffio.h"
#include "icc.h"
#include "gamut.h"
#include "xicc.h"
#include "vrml.h"
#include "sort.h"
#include "ui.h"

#define DE_SPACE 3		/* Delta E of spacing for output points */
#undef DEBUG_PLOT

void usage(void) {
	int i;
	fprintf(stderr,"Create locus of test points that spans the range of colors a TIFF, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: tiffgmts [-v level] [profile.icm | embedded.tif] infile.tif\n");
	fprintf(stderr," -v            Verbose\n");
	fprintf(stderr," -w            emit %s %s file as well as CGATS .ts file\n",vrml_format(),vrml_ext());
	fprintf(stderr," -n            Don't add %s axes or white/black point\n",vrml_format());
	fprintf(stderr," -i intent     p = perceptual, r = relative colorimetric,\n");
	fprintf(stderr,"               s = saturation, a = absolute (default), d = profile default\n");
//  fprintf(stderr,"               P = absolute perceptual, S = absolute saturation\n");
	fprintf(stderr," -p oride      l = Lab_PCS (default), j = %s Appearance Jab\n",icxcam_description(cam_default),icxcam_description(cam_default));
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
	fprintf(stderr,"         s:surround    a = average, m = dim, d = dark,\n");
	fprintf(stderr,"                       c = transparency (default average)\n");
	fprintf(stderr,"         w:X:Y:Z       Adapted white point as XYZ (default media white)\n");
	fprintf(stderr,"         w:x:y         Adapted white point as x, y\n");
	fprintf(stderr,"         a:adaptation  Adaptation luminance in cd.m^2 (default 50.0)\n");
	fprintf(stderr,"         b:background  Background %% of image luminance (default 20)\n");
	fprintf(stderr,"         f:flare       Flare light %% of image luminance (default 0)\n");
	fprintf(stderr,"         g:glare       Flare light %% of ambient (default 1)\n");
	fprintf(stderr,"         g:X:Y:Z       Flare color as XYZ (default media white, Abs: D50)\n");
	fprintf(stderr,"         g:x:y         Flare color as x, y\n");
	fprintf(stderr," -V L,a,b       Overide normal vector direction for span\n");
	fprintf(stderr," -O outputfile  Override the default output filename (locus.ts)\n");
	fprintf(stderr," infile.tif            File to create test value from\n");
	exit(1);
}

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

void set_fminmax(double min[3], double max[3]);
void reset_filter();
void add_fpixel(double val[3]);
int flush_filter(int verb, double filtperc);
void get_filter(co *inp);
void del_filter();

int
main(int argc, char *argv[]) {
	int fa,nfa;					/* argument we're looking at */
	char prof_name[MAXNAMEL+1] = { '\000' };	/* ICC profile name, "" if none */
	char in_name[MAXNAMEL+1];			/* TIFF input file */
	char *xl = NULL, out_name[MAXNAMEL+4+1] = "locus.ts";	/* locus output file */
	int verb = 0;
	int dovrml = 0;
	int doaxes = 1;
	int usevec = 0;
	double vec[3];
	int rv = 0;

	icc *icco = NULL;
	xicc *xicco = NULL;
	icxViewCond vc;				/* Viewing Condition for CIECAM */
	int vc_e = -1;				/* Enumerated viewing condition */
	int vc_s = -1;				/* Surround override */
	double vc_wXYZ[3] = {-1.0, -1.0, -1.0};	/* Adapted white override in XYZ */
	double vc_wxy[2] = {-1.0, -1.0};		/* Adapted white override in x,y */
	double vc_a = -1.0;			/* Adapted luminance */
	double vc_b = -1.0;			/* Background % overid */
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

	TIFF *rh = NULL;
	int x, y, width, height;					/* Size of image */
	uint16 samplesperpixel, bitspersample;
	uint16 pconfig, photometric, pmtc;
	uint16 resunits;
	float resx, resy;
	tdata_t *inbuf;
	void (*cvt)(double *out, double *in);		/* TIFF conversion function, NULL if none */
	icColorSpaceSignature tcs;					/* TIFF colorspace */
	uint16 extrasamples;						/* Extra "alpha" samples */
	uint16 *extrainfo;							/* Info about extra samples */
	int sign_mask;								/* Handling of encoding sign */

	int i, j;
	int nipoints = 0;					/* Number of raster sample points */ 
	co *inp = NULL;						/* Input point values */
	double tdel = 0.0;					/* Total delta along locus */
	rspl *rr = NULL;
	int nopoints = 0;					/* Number of raster sample points */ 
	co *outp = NULL;

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
			else if (argv[fa][1] == 'v') {
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
					if (na[2] == 'a' || na[2] == 'A') {
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
				} else if (na[0] == 'f' || na[0] == 'F') {
					vc_f = atof(na+2);
						usage();
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
				dovrml = 1;
			}
			/* No axis output */
			else if (argv[fa][1] == 'n' || argv[fa][1] == 'N') {
				doaxes = 0;
			}
			/* Vector direction for span */
			else if (argv[fa][1] == 'V') {
				usevec = 1;
				if (na == NULL) usage();
				fa = nfa;
				if (sscanf(na, " %lf , %lf , %lf ",&vec[0], &vec[1], &vec[2]) != 3)
						usage();
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

	if (fa >= argc || argv[fa][0] == '-') usage();
	strncpy(in_name,argv[fa],MAXNAMEL); in_name[MAXNAMEL] = '\000';

	if ((xl = strrchr(out_name, '.')) == NULL)	/* Figure where extention is */
		xl = out_name + strlen(out_name);

	if (verb) {
		printf("Profile     = '%s'\n",prof_name);
		printf("Input TIFF  = '%s'\n",in_name);
		printf("Output file = '%s'\n",out_name);
	}

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
		           , func, intent, pcsor, order, &vc, NULL)) == NULL)
			error ("%d, %s",xicco->errc, xicco->err);
	
		luo->spaces(luo, &ins, &inn, &outs, &outn, &alg, NULL, NULL, NULL);
	
	}

	/* Establish the PCS range if we are filtering */
	{
		double pcsmin[3], pcsmax[3];		/* PCS range for filter stats array */
	
		if (luo) {
			gamut *csgam;

			if ((csgam = luo->get_gamut(luo, 20.0)) == NULL)
				error("Getting the gamut of the source colorspace failed");
			
			csgam->getrange(csgam, pcsmin, pcsmax);
			csgam->del(csgam);
		} else {
			pcsmin[0] = 0.0;
			pcsmax[0] = 100.0;
			pcsmin[1] = -128.0;
			pcsmax[1] = 128.0;
			pcsmin[2] = -128.0;
			pcsmax[2] = 128.0;
		}

		if (verb)
			printf("PCS range = %f..%f, %f..%f. %f..%f\n\n", pcsmin[0], pcsmax[0], pcsmin[1], pcsmax[1], pcsmin[2], pcsmax[2]);

		/* Allocate and initialize the filter */
		set_fminmax(pcsmin, pcsmax);
	}

	/* - - - - - - - - - - - - - - - */
	/* Open up input tiff file ready for reading */
	/* Got arguments, so setup to process the file */

	if ((rh = TIFFOpen(in_name, "r")) == NULL)
		error("error opening read file '%s'",in_name);

	TIFFGetField(rh, TIFFTAG_IMAGEWIDTH,  &width);
	TIFFGetField(rh, TIFFTAG_IMAGELENGTH, &height);

	TIFFGetField(rh, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
	TIFFGetField(rh, TIFFTAG_BITSPERSAMPLE, &bitspersample);
	if (bitspersample != 8 && bitspersample != 16)
		error("TIFF Input file must be 8 bit/channel");

	TIFFGetFieldDefaulted(rh, TIFFTAG_EXTRASAMPLES, &extrasamples, &extrainfo);
	TIFFGetField(rh, TIFFTAG_PHOTOMETRIC, &photometric);

	if (inn != (samplesperpixel-extrasamples))
		error ("TIFF Input file has %d input chanels mismatched to colorspace '%s'",
		       samplesperpixel, icm2str(icmColorSpaceSignature, ins));

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
		error ("TIFF Input file must be planar");

	TIFFGetField(rh, TIFFTAG_RESOLUTIONUNIT, &resunits);
	TIFFGetField(rh, TIFFTAG_XRESOLUTION, &resx);
	TIFFGetField(rh, TIFFTAG_YRESOLUTION, &resy);

	if (verb) {
		printf("Input TIFF file '%s'\n",in_name);
		printf("TIFF file colorspace is %s\n",icm2str(icmColorSpaceSignature,tcs));
		printf("TIFF file photometric is %s\n",Photometric2str(photometric));
		printf("\n");
	}

	/* - - - - - - - - - - - - - - - */
	/* Process colors to translate */
	/* (Should fix this to process a group of lines at a time ?) */

	nipoints = width * height;

//	if ((inp = malloc(sizeof(co) * nipoints)) == NULL)
//		error("Unable to allocate co array");

	inbuf  = _TIFFmalloc(TIFFScanlineSize(rh));

	for (i = y = 0; y < height; y++) {

		/* Read in the next line */
		if (TIFFReadScanline(rh, inbuf, y, 0) < 0)
			error ("Failed to read TIFF line %d",y);

		/* Do floating point conversion */
		for (x = 0; x < width; x++) {
			int e;
			double in[MAX_CHAN], out[MAX_CHAN];
			
			if (bitspersample == 8) {
				for (e = 0; e < samplesperpixel; e++) {
					int v = ((unsigned char *)inbuf)[x * samplesperpixel + e];
					if (sign_mask & (1 << i))		/* Treat input as signed */
						v = (v & 0x80) ? v - 0x80 : v + 0x80;
					in[e] = v/255.0;
				}
			} else {
				for (e = 0; e < samplesperpixel; e++) {
					int v = ((unsigned short *)inbuf)[x * samplesperpixel + e];
					if (sign_mask & (1 << i))		/* Treat input as signed */
						v = (v & 0x8000) ? v - 0x8000 : v + 0x8000;
					in[e] = v/65535.0;
				}
			}
			if (cvt != NULL) {	/* Undo TIFF encoding */
				cvt(in, in);
			}
			if (luo != NULL) {
				if ((rv = luo->lookup(luo, out, in)) > 1)
					error ("%d, %s",icco->errc,icco->err);
				
				if (outs == icSigXYZData)	/* Convert to Lab */
					icmXYZ2Lab(&icco->header->illuminant, out, out);
			} else {
				for (e = 0; e < samplesperpixel; e++)
					out[e] = in[e];
			}

//printf("~1 %f %f %f -> %f %f %f\n", in[0], in[1], in[2], out[0], out[1], out[2]);

			add_fpixel(out);

#ifdef NEVER
	 		/* Store PCS value in array */
			inp[i].v[0] = out[0];
			inp[i].v[1] = out[1];
			inp[i].v[2] = out[2];
			i++;
#endif
		}
	}

	_TIFFfree(inbuf);

	TIFFClose(rh);		/* Close Input file */

	/* Done with lookup object */
	if (luo != NULL) {
		luo->del(luo);
		xicco->del(xicco);		/* Expansion wrapper */
		icco->del(icco);		/* Icc */
	}


	nipoints = flush_filter(verb, 80.0);

	if ((inp = malloc(sizeof(co) * nipoints)) == NULL)
		error("Unable to allocate co array");

	get_filter(inp);

printf("~1 There are %d points\n",nipoints);
//for (i = 0; i < nipoints; i++)
//printf("~1 point %d = %f %f %f\n", i, inp[i].v[0], inp[i].v[1], inp[i].v[2]);

	del_filter();

	/* Create the locus */
	{
		double s0[3], s1[3];
		double t0[3], t1[3];
		double mm[3][4];
		double im[3][4];
		int gres[MXDI] = { 256 } ;

		if (usevec) {
			double max = -1e6; 
			double min = 1e6; 
			double dist;
					
			icmScale3(vec, vec, 1.0/icmNorm3(vec));

			/* Locate the two furthest distant points measured along the vector */
			for (i = 0; i < nipoints; i++) {
				double tt;
				tt = icmDot3(vec, inp[i].v);
				if (tt > max) {
					max = tt;
					icmAry2Ary(s1, inp[i].v);
				}
				if (tt < min) {
					min = tt;
					icmAry2Ary(s0, inp[i].v);
				}
			}
			dist = icmNorm33sq(s0, s1);

printf("~1 most distant in vector %f %f %f = %f %f %f -> %f %f %f dist %f\n",
vec[0], vec[1], vec[2], s0[0], s0[1], s0[2], s1[0], s1[1], s1[2], sqrt(dist));

			t0[0] = 0.0;
			t0[1] = 0.0;
			t0[2] = 0.0;
			t1[0] = sqrt(dist);
			t1[1] = 0.0;
			t1[2] = 0.0;

		} else {
			double dist = 0.0;

			/* Locate the two furthest distant points (brute force) */
			for (i = 0; i < (nipoints-1); i++) {
				for (j = i+1; j < nipoints; j++) {
					double tt;
					if ((tt = icmNorm33sq(inp[i].v, inp[j].v)) > dist) {
						dist = tt;
						icmAry2Ary(s0, inp[i].v);
						icmAry2Ary(s1, inp[j].v);
					}
				}
			}
printf("~1 most distant = %f %f %f -> %f %f %f dist %f\n",
s0[0], s0[1], s0[2], s1[0], s1[1], s1[2], sqrt(dist));

			t0[0] = 0.0;
			t0[1] = 0.0;
			t0[2] = 0.0;
			t1[0] = sqrt(dist);
			t1[1] = 0.0;
			t1[2] = 0.0;
		}

		/* Transform our direction vector to the L* axis, and create inverse too */
		icmVecRotMat(mm, s1, s0, t1, t0);
		icmVecRotMat(im, t1, t0, s1, s0);

		/* Setup for rspl to create smoothed locus */
		for (i = 0; i < nipoints; i++) {
			icmMul3By3x4(inp[i].v, mm, inp[i].v);
			inp[i].p[0] = inp[i].v[0];
			inp[i].v[0] = inp[i].v[1];
			inp[i].v[1] = inp[i].v[2];
//printf("~1 point %d = %f -> %f %f\n", i, inp[i].p[0], inp[i].v[0], inp[i].v[1]);
		}

		/* Create rspl */
		if ((rr = new_rspl(RSPL_NOFLAGS, 1, 2)) == NULL)
			error("Creating rspl failed");

		rr->fit_rspl(rr, RSPL_NOFLAGS,inp, nipoints, NULL, NULL, gres, NULL, NULL, 5.0, NULL, NULL);       
#ifdef DEBUG_PLOT
		{
#define	XRES 100
			double xx[XRES];
			double y1[XRES];
			double y2[XRES];

			for (i = 0; i < XRES; i++) {
				co pp;
				double x;
				x = i/(double)(XRES-1);
				xx[i] = x * (t1[0] - t0[0]);
				pp.p[0] = xx[i];
				rr->interp(rr, &pp);
				y1[i] = pp.v[0];
				y2[i] = pp.v[1];
			}
			do_plot(xx,y1,y2,NULL,XRES);
		}
#endif /* DEBUG_PLOT */

		free(inp);

		nopoints = t1[0] / DE_SPACE;
		if (nopoints < 2)
			nopoints = 2;

		/* Create the output points */
		if ((outp = malloc(sizeof(co) * nopoints)) == NULL)
			error("Unable to allocate co array");

		/* Setup initial division of locus */
		for (i = 0; i < nopoints; i++) {
			double xx;

			xx = i/(double)(nopoints-1);
			xx *= (t1[0] - t0[0]);
			
			outp[i].p[0] = xx;
//printf("~1 div %d = %f\n",i,outp[i].p[0]);
		}
		for (i = 0; i < (nopoints-1); i++) {
			outp[i].p[1] = outp[i+1].p[0] - outp[i].p[0];
//printf("~1 del div  %d = %f\n",i,outp[i].p[1]);
		}

		/* Itterate until the delta between samples is even */
		for (j = 0; j < 10; j++) {
			double alen, minl, maxl;
			double tdiv;
			
			alen = 0.0;
			minl = 1e38;
			maxl = -1.0;
			for (i = 0; i < nopoints; i++) {
				rr->interp(rr, &outp[i]);
				outp[i].v[2] = outp[i].v[1];
				outp[i].v[1] = outp[i].v[0];
				outp[i].v[0] = outp[i].p[0];
				icmMul3By3x4(outp[i].v, im, outp[i].v);

//printf("~1 locus pnt %d = %f %f %f\n", i,outp[i].v[0],outp[i].v[1],outp[i].v[1]);

				if (i > 0) {
					double tt[3], len;
					icmSub3(tt, outp[i].v, outp[i-1].v);
					len = icmNorm3(tt);
					outp[i-1].p[2] = len;
					if (len > maxl)
						maxl = len;
					if (len < minl)
						minl = len;
					alen += len;
				}
			}
			alen /= (nopoints-1.0);
printf("~1 itter %d, alen = %f, minl = %f, maxl = %f\n",j,alen,minl,maxl);

			/* Adjust spacing */
			tdiv = 0.0;
			for (i = 0; i < (nopoints-1); i++) {
				outp[i].p[1] *= pow(alen/outp[i].p[2], 1.0);
				tdiv += outp[i].p[1];
			}
//printf("~1 tdiv = %f\n",tdiv);
			for (i = 0; i < (nopoints-1); i++) {
				outp[i].p[1] *= (t1[0] - t0[0])/tdiv;
//printf("~1 del div %d = %f\n",i,outp[i].p[1]);
			}
			tdiv = 0.0;
			for (i = 0; i < (nopoints-1); i++) {
				tdiv += outp[i].p[1];
			}
//printf("~1 tdiv now = %f\n",tdiv);
			for (i = 1; i < nopoints; i++) {
				outp[i].p[0] = outp[i-1].p[0] + outp[i-1].p[1];
//printf("~1 div %d = %f\n",i,outp[i].p[0]);
			}
		}

		/* Write the CGATS file */
		{
			time_t clk = time(0);
			struct tm *tsp = localtime(&clk);
			char *atm = asctime(tsp); /* Ascii time */
			cgats *pp;

			pp = new_cgats();	/* Create a CGATS structure */
			pp->add_other(pp, "TS"); 	/* Test Set */

			pp->add_table(pp, tt_other, 0);	/* Add the first table for target points */
			pp->add_kword(pp, 0, "DESCRIPTOR", "Argyll Test Point set",NULL);
			pp->add_kword(pp, 0, "ORIGINATOR", "Argyll tiffgmts", NULL);
			atm[strlen(atm)-1] = '\000';	/* Remove \n from end */
			pp->add_kword(pp, 0, "CREATED",atm, NULL);

			pp->add_field(pp, 0, "SAMPLE_ID", cs_t);
			pp->add_field(pp, 0, "LAB_L", r_t);
			pp->add_field(pp, 0, "LAB_A", r_t);
			pp->add_field(pp, 0, "LAB_B", r_t);

			for (i = 0; i < nopoints; i++) {
				char buf[100];
				cgats_set_elem ary[1 + 3];

				sprintf(buf,"%d",i+1);
				ary[0].c = buf;
				ary[1 + 0].d = outp[i].v[0];
				ary[1 + 1].d = outp[i].v[1];
				ary[1 + 2].d = outp[i].v[2];
		
				pp->add_setarr(pp, 0, ary);
			}

			if (pp->write_name(pp, out_name))
			    error("Write error : %s",pp->err);
		}

		/* Create the VRML/X3D file */
		if (dovrml) {
			vrml *vv;
			
			xl[0] = '\000';			/* remove extension */
			printf("Output %s file '%s%s'\n",vrml_format(),out_name,vrml_ext());
			if ((vv = new_vrml(out_name, doaxes, 0)) == NULL)
				error ("Creating %s object %s%s failed",vrml_format(),out_name,vrml_ext());

#ifdef NEVER
			vv->start_line_set(vv);
			for (i = 0; i < nopoints; i++) {
				vv->add_vertex(vv, outp[i].v);
			}
			vv->make_lines(vv, nopoints);
#else
			for (i = 1; i < nopoints; i++) {
				vv->add_cone(vv, outp[i-1].v, outp[i].v, NULL, 0.5);
			}
#endif

			vv->del(vv);		/* Write file */
		}
		free(outp);
	}

	rr->del(rr);

	return 0;
}

/* ------------------------------------------ */
/* A pixel value filter module. We quantize the pixel values and keep statistics */
/* on them, so as to filter out low frequency colors. */

#define FILTBITS 5		/* Total = 2 ^ (3 * FILTBITS) entries  = 33Mbytes*/
#define FILTSIZE (1 << FILTBITS)

/* A filtered cell entry */
typedef struct {
	int   count;		/* Count of pixels that fall in this cell */
	float pcs[3];		/* Sum of PCS value that fall in this cell */
} fent;

struct _ffilter {
	double min[3], max[3];		/* PCS range */
	double filtperc;
	int used;

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
	}

//printf("~1 color %f %f %f -> Cell %d %d %d\n", val[0], val[1], val[2], qv[0], qv[1], qv[2]);

	/* Find the appropriate cell */
	fe = &ff->cells[qv[0]][qv[1]][qv[2]];

	fe->pcs[0] += val[0];
	fe->pcs[1] += val[1];
	fe->pcs[2] += val[2];
//printf("Updated pcs to %f %f %f\n", val[0],val[1],val[2]);
	fe->count++;
//printf("Cell count = %d\n",fe->count);
}

/* Flush the filter contents, and return the number of filtered values */
int flush_filter(int verb, double filtperc) {
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
			ff->scells[i]->pcs[0] /= ff->scells[i]->count;
			ff->scells[i]->pcs[1] /= ff->scells[i]->count;
			ff->scells[i]->pcs[2] /= ff->scells[i]->count;
			j++;
			cuml = j/(used-1.0);
		}
	}
	ff->used = used;
	ff->filtperc = filtperc;

	return j;
}

/* Add the points to the array */
void get_filter(co *inp) {
	int i, j;
	int totcells = FILTSIZE * FILTSIZE * FILTSIZE;
	double cuml, filtperc;
	int used;

	used = ff->used;
	filtperc = ff->filtperc;

	for (cuml = 0.0, i = j = 0; cuml < filtperc && i < totcells; i++) {
		
		if (ff->scells[i]->count > 0) {
			double val[3];
			inp[j].v[0] = ff->scells[i]->pcs[0];	/* float -> double */
			inp[j].v[1] = ff->scells[i]->pcs[1];
			inp[j].v[2] = ff->scells[i]->pcs[2];
//printf("~1 adding %f %f %f to gamut\n", val[0], val[1], val[2]);
			j++;
			cuml = j/(used-1.0);
		}

	}
}

/* Free up the filter structure */
void del_filter() {

	free(ff);
}

