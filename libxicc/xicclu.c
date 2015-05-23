
/* 
 * xicc lookup/test utility
 *
 * This program is the analog of icclu, but allows reverse lookup
 * of transforms by making use of xicc interpolation code, + other
 * more advanced features.
 * (Based on the old xfmlu.c)
 *
 * Author:  Graeme W. Gill
 * Date:    8/7/00
 * Version: 1.00
 *
 * Copyright 1999, 2000 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* TTBD:

	Add HSV as alternative to RGB ?

	Can -ff and -fif be made to work with device link files ?

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
#include "xicc.h"
#include "plot.h"
#include "ui.h"

#undef SPTEST				/* Test rspl gamut surface code */

#define USE_NEARCLIP		/* Our usual expectation */
#define XRES 128			/* Plotting resolution */

void usage(char *diag) {
	int i;
	fprintf(stderr,"Lookup ICC or CAL colors, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: xicclu [-options] profile_or_cal\n");
	if (diag != NULL)
		fprintf(stderr,"Diagnostic: %s\n",diag);
	fprintf(stderr," -v level       Verbosity level 0 - 2 (default = 1)\n");
	fprintf(stderr," -g             Plot slice instead of looking colors up. (Default white to black)\n");
	fprintf(stderr," -G s:L:a:b    	Override plot slice start with Lab or Jab co-ordinate \n");
	fprintf(stderr," -G e:L:a:b    	Override plot slice end with Lab or Jab co-ordinate \n");
	fprintf(stderr," -f function    f = forward, b = backwards, g = gamut, p = preview\n");
	fprintf(stderr,"                if = inverted forward, ib = inverted backwards\n");
	fprintf(stderr," -i intent      a = absolute, r = relative colorimetric\n");
	fprintf(stderr,"                p = perceptual, s = saturation\n");
//  fprintf(stderr,"                P = absolute perceptual, S = absolute saturation\n");
	fprintf(stderr," -o order       n = normal (priority: lut > matrix > monochrome)\n");
	fprintf(stderr,"                r = reverse (priority: monochrome > matrix > lut)\n");
	fprintf(stderr," -p oride       x = XYZ_PCS, X = XYZ * 100, l = Lab_PCS, L = LCh, y = Yxy\n");
	fprintf(stderr,"                j = %s Appearance Jab, J = %s Appearance JCh\n",icxcam_description(cam_default),icxcam_description(cam_default));
	fprintf(stderr," -s scale       Scale device range 0.0 - scale rather than 0.0 - 1.0\n");
	fprintf(stderr," -e flag        Video encode device input as:\n");
	fprintf(stderr," -E flag        Video decode device output as:\n");
	fprintf(stderr,"     n           normal 0..1 full range RGB levels (default)\n");
	fprintf(stderr,"     t           (16-235)/255 \"TV\" RGB levels\n");
	fprintf(stderr,"     6           Rec601 YCbCr SD (16-235,240)/255 \"TV\" levels\n");
	fprintf(stderr,"     7           Rec709 1125/60Hz YCbCr HD (16-235,240)/255 \"TV\" levels\n");
	fprintf(stderr,"     5           Rec709 1250/50Hz YCbCr HD (16-235,240)/255 \"TV\" levels\n");
	fprintf(stderr,"     2           Rec2020 YCbCr UHD (16-235,240)/255 \"TV\" levels\n");
	fprintf(stderr,"     C           Rec2020 Constant Luminance YCbCr UHD (16-235,240)/255 \"TV\" levels\n");
	fprintf(stderr," -k [zhxrlv]    Black value target: z = zero K,\n");
	fprintf(stderr,"                h = 0.5 K, x = max K, r = ramp K (def.)\n");
	fprintf(stderr,"                l = extra PCS input is portion of K locus\n");
	fprintf(stderr,"                v = extra PCS input is K target value\n");
	fprintf(stderr," -k p stle stpo enpo enle shape\n");
	fprintf(stderr,"                stle: K level at White 0.0 - 1.0\n");
	fprintf(stderr,"                stpo: start point of transition Wh 0.0 - Bk 1.0\n");
	fprintf(stderr,"                enpo: End point of transition Wh 0.0 - Bk 1.0\n");
	fprintf(stderr,"                enle: K level at Black 0.0 - 1.0\n");
	fprintf(stderr,"                shape: 1.0 = straight, 0.0-1.0 concave, 1.0-2.0 convex\n");
	fprintf(stderr," -k q stle0 stpo0 enpo0 enle0 shape0 stle2 stpo2 enpo2 enle2 shape2\n");
	fprintf(stderr,"                Transfer extra PCS input to dual curve limits\n");
	fprintf(stderr," -K parameters  Same as -k, but target is K locus rather than K value itself\n");
	fprintf(stderr," -l tlimit      set total ink limit, 0 - 400%% (estimate by default)\n");
	fprintf(stderr," -L klimit      set black ink limit, 0 - 100%% (estimate by default)\n");
	fprintf(stderr," -a             show actual target values if clipped\n");
	fprintf(stderr," -u             warn if output PCS is outside the spectrum locus\n");
	fprintf(stderr," -m             merge output processing into clut\n");
	fprintf(stderr," -b             use CAM Jab for clipping\n");
//	fprintf(stderr," -S             Use internal optimised separation for inverse 4d [NOT IMPLEMENTED]\n");

#ifdef SPTEST
	fprintf(stderr," -w             special gamut surface test PCS space\n");
	fprintf(stderr," -W             special gamut surface test, PCS' space\n");
#endif
	fprintf(stderr," -c viewcond    set viewing conditions for CIECAM97s,\n");
	fprintf(stderr,"                either an enumerated choice, or a parameter:value changes\n");
	for (i = 0; ; i++) {
		icxViewCond vc;
		if (xicc_enum_viewcond(NULL, &vc, i, NULL, 1, NULL) == -999)
			break;

		fprintf(stderr,"            %s\n",vc.desc);
	}

	fprintf(stderr,"         s:surround    n = auto, a = average, m = dim, d = dark,\n");
	fprintf(stderr,"                       c = transparency (default average)\n");
	fprintf(stderr,"         w:X:Y:Z       Adapted white point as XYZ (default media white, Abs: D50)\n");
	fprintf(stderr,"         w:x:y         Adapted white point as x, y\n");
	fprintf(stderr,"         a:adaptation  Adaptation luminance in cd.m^2 (default 50.0)\n");
	fprintf(stderr,"         b:background  Background %% of image luminance (default 20)\n");
	fprintf(stderr,"         l:imagewhite  Image white in cd.m^2 if surround = auto (default 250)\n");
	fprintf(stderr,"         f:flare       Flare light %% of image luminance (default 0)\n");
	fprintf(stderr,"         g:glare       Flare light %% of ambient (default 1)\n");
	fprintf(stderr,"         g:X:Y:Z       Flare color as XYZ (default media white, Abs: D50)\n");
	fprintf(stderr,"         g:x:y         Flare color as x, y\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"    The colors to be translated should be fed into standard in,\n");
	fprintf(stderr,"    one input color per line, white space separated.\n");
	fprintf(stderr,"    A line starting with a # will be ignored.\n");
	fprintf(stderr,"    A line not starting with a number will terminate the program.\n");
	fprintf(stderr,"    Use -v0 for just output colors.\n");
	exit(1);
}

#ifdef SPTEST

#pragma message("!!!!!!!!!!!! Experimental gamut boundary test  !!!!!!!!!")

/* Output curve function */
void spoutf(void *cbntx, double *out, double *in) {
	icxLuLut *clu = (icxLuLut *)cbntx;
	
	clu->output(clu, out, in);
	clu->out_abs(clu, out, out);
}

/* Inverse output curve function */
void spioutf(void *cbntx, double *out, double *in) {
	icxLuLut *clu = (icxLuLut *)cbntx;

	clu->inv_out_abs(clu, out, in);
	clu->inv_output(clu, out, out);
}


#endif /* SPTEST */

int
main(int argc, char *argv[]) {
	int fa, nfa, mfa;				/* argument we're looking at */
	char prof_name[MAXNAMEL+1];
	icmFile *fp = NULL;
	icc *icco = NULL;
	xicc *xicco = NULL;
	xcal *cal = NULL;			/* If .cal rather than .icm/.icc, not NULL */
	int doplot = 0;				/* Do grey axis plot */
	double pstart[3] = { -1000.0 };		/* Plot Lab/Jab PCS start point = white */ 
	double pend[3]   = { -1000.0 };		/* Plot Lab/Jab PCS end point = black */ 
	icxInk ink;					/* Ink parameters */
	double tlimit = -1.0;		/* Total ink limit */
	double klimit = -1.0;		/* Black ink limit */
	int intsep = 0;
	icxViewCond vc;				/* Viewing Condition for CIECAM97s */
	int vc_e = -1;				/* Enumerated viewing condition */
	int vc_s = -1;				/* Surround override */
	double vc_wXYZ[3] = {-1.0, -1.0, -1.0};	/* Adapted white override in XYZ */
	double vc_wxy[2] = {-1.0, -1.0};		/* Adapted white override in x,y */
	double vc_a = -1.0;			/* Adapted luminance */
	double vc_b = -1.0;			/* Background % overide */
	double vc_l = -1.0;			/* Scene luminance override */
	double vc_f = -1.0;			/* Flare % overide */
	double vc_g = -1.0;			/* Glare % overide */
	double vc_gXYZ[3] = {-1.0, -1.0, -1.0};	/* Glare color override in XYZ */
	double vc_gxy[2] = {-1.0, -1.0};		/* Glare color override in x,y */
	int verb = 1;
	int actual = 0;
	int slocwarn = 0;
	int merge = 0;
	int camclip = 0;
	int repYxy = 0;			/* Report Yxy */
	int repJCh = 0;			/* Report JCh */
	int repLCh = 0;			/* Report LCh */
	int repXYZ100 = 0;		/* Scale XYZ by 10 */
	double scale = 0.0;		/* Device value scale factor */
	int in_tvenc = 0;		/* 1 to use RGB Video Level encoding, 2 = Rec601, 3 = Rec709 YCbCr */
	int out_tvenc = 0;		/* 1 to use RGB Video Level encoding, 2 = Rec601, 3 = Rec709 YCbCr */
	int rv = 0;
	char buf[200];
	double uin[MAX_CHAN], in[MAX_CHAN], out[MAX_CHAN], uout[MAX_CHAN];

	icxLuBase *luo = NULL, *aluo = NULL;
	icColorSpaceSignature ins, outs;	/* Type of input and output spaces */
	int inn, outn;						/* Number of components */
	icmLuAlgType alg;					/* Type of lookup algorithm */

	/* Lookup parameters */
	icmLookupFunc     func   = icmFwd;				/* Default */
	icRenderingIntent intent = icmDefaultIntent;	/* Default */
	icColorSpaceSignature pcsor = icmSigDefaultData;	/* Default */
	icmLookupOrder    order  = icmLuOrdNorm;		/* Default */
	int               inking = 3;					/* Default is ramp */
	int               locus = 0;					/* Default is K value */
													/* K curve params */
	double Kstle = 0.0, Kstpo = 0.0, Kenle = 0.0, Kenpo = 0.0, Kshap = 0.0;
													/* K curve params (max) */
	double Kstle1 = 0.0, Kstpo1 = 0.0, Kenle1 = 0.0, Kenpo1 = 0.0, Kshap1 = 0.0;
	int               invert = 0;
	
	xslpoly *chlp = NULL;

#ifdef SPTEST
	int sptest = 0;
	warning("xicc/xicclu.c !!!! special rspl gamut sest code is compiled in !!!!\n");
#endif

	error_program = argv[0];

	if (argc < 2)
		usage("Too few arguments");

	/* Process the arguments */
	mfa = 1;        /* Minimum final arguments */
	for (fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')	{	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1+mfa) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage("Requested usage");

			/* Verbosity */
			else if (argv[fa][1] == 'v') {
				if (na == NULL) {
					verb = 2;
				} else {
					if (na[0] == '0')
						verb = 0;
					else if (na[0] == '1')
						verb = 1;
					else if (na[0] == '2')
						verb = 2;
					else
						usage("Illegal verbosity level");
					fa = nfa;
				}
			}

			/* Locus plot */
			else if (argv[fa][1] == 'g') {
				doplot = 1;
			}
			/* Plot start or end override */
			else if (argv[fa][1] == 'G') {
				if (na == NULL) usage("No parameter after flag -G");
				if (na[0] == 's' || na[0] == 'S') {
					if (sscanf(na+1,":%lf:%lf:%lf",&pstart[0],&pstart[1],&pstart[2]) != 3)
						usage("Unrecognised parameters after -Gs");
				} else if (na[0] == 'e' || na[0] == 'E') {
					if (sscanf(na+1,":%lf:%lf:%lf",&pend[0],&pend[1],&pend[2]) != 3)
						usage("Unrecognised parameters after -Ge");
				} else
					usage("Unrecognised parameters after -G");
				fa = nfa;
			}
			/* Actual target values */
			else if (argv[fa][1] == 'a') {
				actual = 1;
			}
			/* Warn if output is outside the spectrum locus */
			else if (argv[fa][1] == 'u') {
				slocwarn = 1;
			}
			/* Merge output */
			else if (argv[fa][1] == 'm') {
				merge = 1;
			}
			/* Use CAM Jab for clipping on reverse lookup */
			else if (argv[fa][1] == 'b') {
				camclip = 1;
			}
			/* Use optimised internal separation */
			else if (argv[fa][1] == 'S') {
				intsep = 1;
			}
			/* Device scale */
			else if (argv[fa][1] == 's') {
				if (na == NULL) usage("No parameter after flag -s");
				scale = atof(na);
				if (scale <= 0.0) usage("Illegal scale value");
				fa = nfa;
			}
			/* Video RGB encoding */
			else if (argv[fa][1] == 'e'
			      || argv[fa][1] == 'E') {
				int enc;
				if (na == NULL) usage("Video encodong flag (-e/E) needs an argument");
    			switch (na[0]) {
					case 'n':				/* Normal */
						enc = 0;
						break;
					case 't':				/* TV 16 .. 235 */
						enc = 1;
						break;
					case '6':				/* Rec601 YCbCr */
						enc = 2;
						break;
					case '7':				/* Rec709 1150/60/2:1 YCbCr */
						enc = 3;
						break;
					case '5':				/* Rec709 1250/50/2:1 YCbCr (HD) */
						enc = 4;
						break;
					case '2':				/* Rec2020 Non-constant Luminance YCbCr (UHD) */
						enc = 5;
						break;
					case 'C':				/* Rec2020 Constant Luminance YCbCr (UHD) */
						enc = 6;
						break;
					default:
						usage("Video encoding (-E) argument not recognised");
				}
				if (argv[fa][1] == 'e')
					in_tvenc = enc;
				else
					out_tvenc = enc;
				fa = nfa;
			}

			/* function */
			else if (argv[fa][1] == 'f') {
				if (na == NULL) usage("No parameter after flag -f");
    			switch (na[0]) {
					case 'f':
					case 'F':
						func = icmFwd;
						break;
					case 'b':
					case 'B':
						func = icmBwd;
						break;
					case 'g':
					case 'G':
						func = icmGamut;
						break;
					case 'p':
					case 'P':
						func = icmPreview;
						break;
					case 'i':
					case 'I':
						invert = 1;
						if (na[1] == 'f' || na[1] == 'F')
							func = icmFwd;
						else if (na[1] == 'b' || na[1] == 'B')
							func = icmBwd;
						else
							usage("Unknown parameter after flag -fi");
						break;
					default:
						usage("Unknown parameter after flag -f");
				}
				fa = nfa;
			}

			/* Intent */
			else if (argv[fa][1] == 'i') {
				if (na == NULL) usage("No parameter after flag -i");
    			switch (na[0]) {
					case 'p':
						intent = icPerceptual;
						break;
					case 'r':
						intent = icRelativeColorimetric;
						break;
					case 's':
						intent = icSaturation;
						break;
					case 'a':
						intent = icAbsoluteColorimetric;
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
						usage("Unknown parameter after flag -i");
				}
				fa = nfa;
			}

			/* PCS override */
			else if (argv[fa][1] == 'p') {
				if (na == NULL) usage("No parameter after flag -i");
    			switch (na[0]) {
					case 'x':
						pcsor = icSigXYZData;
						repYxy = 0;
						repLCh = 0;
						repJCh = 0;
						repXYZ100 = 0;
						break;
					case 'X':
						pcsor = icSigXYZData;
						repYxy = 0;
						repLCh = 0;
						repJCh = 0;
						repXYZ100 = 1;
						break;
					case 'l':
						pcsor = icSigLabData;
						repYxy = 0;
						repLCh = 0;
						repJCh = 0;
						repXYZ100 = 0;
						break;
					case 'L':
						pcsor = icSigLabData;
						repYxy = 0;
						repLCh = 1;
						repJCh = 0;
						repXYZ100 = 0;
						break;
					case 'y':
					case 'Y':
						pcsor = icSigXYZData;
						repYxy = 1;
						repLCh = 0;
						repJCh = 0;
						repXYZ100 = 0;
						break;
					case 'j':
						pcsor = icxSigJabData;
						repYxy = 0;
						repLCh = 0;
						repJCh = 0;
						repXYZ100 = 0;
						break;
					case 'J':
						pcsor = icxSigJabData;
						repYxy = 0;
						repLCh = 0;
						repJCh = 1;
						repXYZ100 = 0;
						break;
					default:
						usage("Unknown parameter after flag -i");
				}
				fa = nfa;
			}

			/* Search order */
			else if (argv[fa][1] == 'o') {
				if (na == NULL) usage("No parameter after flag -o");
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
						usage("Unknown parameter after flag -o");
				}
				fa = nfa;
			}

			/* Inking rule */
			else if (argv[fa][1] == 'k'
			      || argv[fa][1] == 'K') {
				if (na == NULL) usage("No parameter after flag -k");
				if (argv[fa][1] == 'k')
					locus = 0;			/* K value target */
				else
					locus = 1;			/* K locus target */
    			switch (na[0]) {
					case 'z':
					case 'Z':
						inking = 0;		/* Use minimum k */
						break;
					case 'h':
					case 'H':
						inking = 1;		/* Use 0.5 k */
						break;
					case 'x':
					case 'X':
						inking = 2;		/* Use maximum k */
						break;
					case 'r':
					case 'R':
						inking = 3;		/* Use ramping K */
						break;
					case 'l':
					case 'L':
						inking = 4;		/* Extra param is locus */
						break;
					case 'v':
					case 'V':
						inking = 5;		/* Extra param is K target */
						break;
					case 'p':
					case 'P':
					case 'q':
					case 'Q':
						inking = 6;		/* Use curve parameter */

						++fa;
						if (fa >= argc) usage("Inking rule (-kp) expects more parameters");
						Kstle = atof(argv[fa]);

						++fa;
						if (fa >= argc) usage("Inking rule (-kp) expects more parameters");
						Kstpo = atof(argv[fa]);

						++fa;
						if (fa >= argc || argv[fa][0] == '-') usage("Inking rule (-kp) expects more parameters");
						Kenpo = atof(argv[fa]);

						++fa;
						if (fa >= argc || argv[fa][0] == '-') usage("Inking rule (-kp) expects more parameters");
						Kenle = atof(argv[fa]);

						++fa;
						if (fa >= argc || argv[fa][0] == '-') usage("Inking rule (-kp) expects more parameters");
						Kshap = atof(argv[fa]);

						if (na[0] == 'q' || na[0] == 'Q') {
							inking = 7;		/* Use transfer to dual curve parameter */

							++fa;
							if (fa >= argc) usage("Inking rule (-kq) expects more parameters");
							Kstle1 = atof(argv[fa]);
	
							++fa;
							if (fa >= argc) usage("Inking rule (-kq) expects more parameters");
							Kstpo1 = atof(argv[fa]);
	
							++fa;
							if (fa >= argc || argv[fa][0] == '-') usage("Inking rule (-kq) expects more parameters");
							Kenpo1 = atof(argv[fa]);
	
							++fa;
							if (fa >= argc) usage("Inking rule (-kq) expects more parameters");
							Kenle1 = atof(argv[fa]);
	
							++fa;
							if (fa >= argc || argv[fa][0] == '-') usage("Inking rule (-kq) expects more parameters");
							Kshap1 = atof(argv[fa]);
	
						}
						break;
					default:
						usage("Unknown parameter after flag -k");
				}
				fa = nfa;
			}

			else if (argv[fa][1] == 'l') {
				if (na == NULL) usage("No parameter after flag -l");
				tlimit = atoi(na)/100.0;
				fa = nfa;
			}

			else if (argv[fa][1] == 'L') {
				if (na == NULL) usage("No parameter after flag -L");
				klimit = atoi(na)/100.0;
				fa = nfa;
			}

#ifdef SPTEST
			else if (argv[fa][1] == 'w') {
				sptest = 1;
			}
			else if (argv[fa][1] == 'W') {
				sptest = 2;
			}
#endif
			/* Viewing conditions */
			else if (argv[fa][1] == 'c') {
				if (na == NULL) usage("No parameter after flag -c");
#ifdef NEVER
				if (na[0] >= '0' && na[0] <= '9') {
					vc_e = atoi(na);
				} else
#endif
				if (na[1] != ':') {
					if ((vc_e = xicc_enum_viewcond(NULL, NULL, -2, na, 1, NULL)) == -999)
						usage("Urecognised Enumerated Viewing conditions");
				} else if (na[0] == 's' || na[0] == 'S') {
					if (na[1] != ':')
						usage("Unrecognised parameters after -cs");
					if (na[2] == 'n' || na[2] == 'N') {
						vc_s = vc_none;		/* Automatic using Lv */
					} else if (na[2] == 'a' || na[2] == 'A') {
						vc_s = vc_average;
					} else if (na[2] == 'm' || na[2] == 'M') {
						vc_s = vc_dim;
					} else if (na[2] == 'd' || na[2] == 'D') {
						vc_s = vc_dark;
					} else if (na[2] == 'c' || na[2] == 'C') {
						vc_s = vc_cut_sheet;
					} else
						usage("Unrecognised parameters after -cs:");
				} else if (na[0] == 'w' || na[0] == 'W') {
					double x, y, z;
					if (sscanf(na+1,":%lf:%lf:%lf",&x,&y,&z) == 3) {
						vc_wXYZ[0] = x; vc_wXYZ[1] = y; vc_wXYZ[2] = z;
					} else if (sscanf(na+1,":%lf:%lf",&x,&y) == 2) {
						vc_wxy[0] = x; vc_wxy[1] = y;
					} else
						usage("Unrecognised parameters after -cw");
				} else if (na[0] == 'a' || na[0] == 'A') {
					if (na[1] != ':')
						usage("Unrecognised parameters after -ca");
					vc_a = atof(na+2);
				} else if (na[0] == 'b' || na[0] == 'B') {
					if (na[1] != ':')
						usage("Unrecognised parameters after -cb");
					vc_b = atof(na+2);
				} else if (na[0] == 'l' || na[0] == 'L') {
					if (na[1] != ':')
						usage("Viewing conditions (-cl) missing ':'");
					vc_l = atof(na+2);
				} else if (na[0] == 'f' || na[0] == 'F') {
					if (na[1] != ':')
						usage("Viewing conditions (-cf) missing ':'");
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
						usage("Unrecognised parameters after -cg");
				} else
					usage("Unrecognised parameters after -c");
				fa = nfa;
			}

			else 
				usage("Unknown flag");
		} else
			break;
	}

	if (fa >= argc || argv[fa][0] == '-') usage("Expecting profile file name");
	strncpy(prof_name,argv[fa],MAXNAMEL); prof_name[MAXNAMEL] = '\000';

	if (slocwarn) {
		if ((chlp = chrom_locus_poligon(0, icxOT_CIE_1931_2, 0)) == NULL)
			error("chrom_locus_poligon failed");
	}

	/* Open up the profile for reading */
	if ((fp = new_icmFileStd_name(prof_name,"r")) == NULL)
		error ("Can't open file '%s'",prof_name);

	if ((icco = new_icc()) == NULL)
		error ("Creation of ICC object failed");

	if ((rv = icco->read(icco,fp,0)) == 0) {		/* ICC profile */

		if (doplot) {
	
			/* Force PCS to be Lab or Jab */
			repJCh = 0;
			repLCh = 0;
			if (pcsor != icxSigJabData)
				pcsor = icSigLabData;
	
			if ((invert == 0 && func != icmBwd)
			 || (invert != 0 && func != icmFwd))
				error("Must use -fb or -fif for grey axis plot");
		}

		if (icco->header->cmmId == str2tag("argl"))
			icco->allowclutPoints256 = 1;
				
		if (doplot) {
			if (icco->header->deviceClass != icSigInputClass
			 && icco->header->deviceClass != icSigDisplayClass
			 && icco->header->deviceClass != icSigOutputClass)
				error("Profile must be a device profile to plot neutral axis");
		}

		/* Wrap with an expanded icc */
		if ((xicco = new_xicc(icco)) == NULL)
			error ("Creation of xicc failed");

		/* Set the default ink limits if not set on command line */
		icxDefaultLimits(xicco, &ink.tlimit, tlimit, &ink.klimit, klimit);

		if (verb > 1) {
			if (ink.tlimit >= 0.0)
				printf("Total ink limit assumed is %3.0f%%\n",100.0 * ink.tlimit);
			if (ink.klimit >= 0.0)
				printf("Black ink limit assumed is %3.0f%%\n",100.0 * ink.klimit);
		}

		ink.KonlyLmin = 0;		/* Use normal black as locus Lmin */

		ink.c.Ksmth = ICXINKDEFSMTH;	/* Default curve smoothing */
		ink.c.Kskew = ICXINKDEFSKEW;	/* default curve skew */
		ink.x.Ksmth = ICXINKDEFSMTH;
		ink.x.Kskew = ICXINKDEFSKEW;

		if (inking == 0) {			/* Use minimum */
			ink.k_rule = locus ? icxKluma5 : icxKluma5k;	/* Locus or value target */
			ink.c.Kstle = 0.0;
			ink.c.Kstpo = 0.0;
			ink.c.Kenpo = 1.0;
			ink.c.Kenle = 0.0;
			ink.c.Kshap = 1.0;
		} else if (inking == 1) {	/* Use 0.5  */
			ink.k_rule = locus ? icxKluma5 : icxKluma5k;	/* Locus or value target */
			ink.c.Kstle = 0.5;
			ink.c.Kstpo = 0.0;
			ink.c.Kenpo = 1.0;
			ink.c.Kenle = 0.5;
			ink.c.Kshap = 1.0;
		} else if (inking == 2) {	/* Use maximum  */
			ink.k_rule = locus ? icxKluma5 : icxKluma5k;	/* Locus or value target */
			ink.c.Kstle = 1.0;
			ink.c.Kstpo = 0.0;
			ink.c.Kenpo = 1.0;
			ink.c.Kenle = 1.0;
			ink.c.Kshap = 1.0;
		} else if (inking == 3) {	/* Use ramp  */
			ink.k_rule = locus ? icxKluma5 : icxKluma5k;	/* Locus or value target */
			ink.c.Kstle = 0.0;
			ink.c.Kstpo = 0.0;
			ink.c.Kenpo = 1.0;
			ink.c.Kenle = 1.0;
			ink.c.Kshap = 1.0;
		} else if (inking == 4) {	/* Use locus  */
			ink.k_rule = icxKlocus;
		} else if (inking == 5) {	/* Use K target  */
			ink.k_rule = icxKvalue;
		} else if (inking == 6) {	/* Use specified curve */
			ink.k_rule = locus ? icxKluma5 : icxKluma5k;	/* Locus or value target */
			ink.c.Kstle = Kstle;
			ink.c.Kstpo = Kstpo;
			ink.c.Kenpo = Kenpo;
			ink.c.Kenle = Kenle;
			ink.c.Kshap = Kshap;
		} else {				/* Use dual curves */
			ink.k_rule = locus ? icxKl5l : icxKl5lk;	/* Locus or value target */
			ink.c.Kstle = Kstle;
			ink.c.Kstpo = Kstpo;
			ink.c.Kenpo = Kenpo;
			ink.c.Kenle = Kenle;
			ink.c.Kshap = Kshap;
			ink.x.Kstle = Kstle1;
			ink.x.Kstpo = Kstpo1;
			ink.x.Kenpo = Kenpo1;
			ink.x.Kenle = Kenle1;
			ink.x.Kshap = Kshap1;
		}

		/* Setup the viewing conditions in case we need them */
		if (xicc_enum_viewcond(xicco, &vc, -1, NULL, 0, NULL) == -999) {
			if (camclip || pcsor == icxSigJabData)		/* If it will be needed */
				error("%d, %s",xicco->errc, xicco->err);
		}

//xicc_dump_viewcond(&vc);
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
//xicc_dump_viewcond(&vc);

		/* Get a expanded color conversion object */
		if ((luo = xicco->get_luobj(xicco, 0
#ifdef USE_NEARCLIP
		   | ICX_CLIP_NEAREST
#endif
		   | (intsep ? ICX_INT_SEPARATE : 0)
		   | (merge ? ICX_MERGE_CLUT : 0)
		   | (camclip ? ICX_CAM_CLIP : 0)
		   | ICX_FAST_SETUP
		                                  , func, intent, pcsor, order, &vc, &ink)) == NULL)
			error ("%d, %s",xicco->errc, xicco->err);

		/* Get details of conversion (Arguments may be NULL if info not needed) */
		if (invert)
			luo->spaces(luo, &outs, &outn, &ins, &inn, &alg, NULL, NULL, NULL);
		else
			luo->spaces(luo, &ins, &inn, &outs, &outn, &alg, NULL, NULL, NULL);

		/* If we can do check on clipped values */
		if (actual != 0) {
			if (invert == 0) {
				if (func == icmFwd || func == icmBwd) {
					if ((aluo = xicco->get_luobj(xicco, ICX_CLIP_NEAREST,
					     func == icmFwd ? icmBwd : icmFwd, intent, pcsor, order, &vc, &ink)) == NULL)
					error ("%d, %s",xicco->errc, xicco->err);
				}
			} else {
				aluo = luo;		/* We can use the same one */
			}
		}

		/* More information */
		if (verb > 1) {
			int j;
			double inmin[MAX_CHAN], inmax[MAX_CHAN];
			double outmin[MAX_CHAN], outmax[MAX_CHAN];

			luo->get_native_ranges(luo, inmin, inmax, outmin,outmax);
			printf("Internal input value range: ");
			for (j = 0; j < inn; j++) {
				if (j > 0)
					fprintf(stdout," %f..%f",inmin[j], inmax[j]);
				else
					fprintf(stdout,"%f..%f",inmin[j], inmax[j]);
			}
			printf("\nInternal output value range: ");
			for (j = 0; j < outn; j++) {
				if (j > 0)
					fprintf(stdout," %f..%f",outmin[j], outmax[j]);
				else
					fprintf(stdout,"%f..%f",outmin[j], outmax[j]);
			}

			luo->get_ranges(luo, inmin, inmax, outmin,outmax);
			printf("\nInput value range: ");
			for (j = 0; j < inn; j++) {
				if (j > 0)
					fprintf(stdout," %f..%f",inmin[j], inmax[j]);
				else
					fprintf(stdout,"%f..%f",inmin[j], inmax[j]);
			}
			printf("\nOutput value range: ");
			for (j = 0; j < outn; j++) {
				if (j > 0)
					fprintf(stdout," %f..%f",outmin[j], outmax[j]);
				else
					fprintf(stdout,"%f..%f",outmin[j], outmax[j]);
			}
			printf("\n");
		}

		if (repYxy) {	/* report Yxy rather than XYZ */
			if (ins == icSigXYZData)
				ins = icSigYxyData; 
			if (outs == icSigXYZData)
				outs = icSigYxyData; 
		}

		if (repJCh) {	/* report JCh rather than Jab */
			if (ins == icxSigJabData)
				ins = icxSigJChData; 
			if (outs == icxSigJabData)
				outs = icxSigJChData; 
		}
		if (repLCh) {	/* report LCh rather than Lab */
			if (ins == icSigLabData)
				ins = icxSigLChData; 
			if (outs == icSigLabData)
				outs = icxSigLChData; 
		}

#ifdef SPTEST
		if (sptest) {
			icxLuLut *clu;
			double cent[3] = { 50.0, 0.0, 0.0 };

			if (luo->plu->ttype != icmLutType)
				error("Special test only works on CLUT profiles");

			clu = (icxLuLut *)luo;

			clu->clutTable->comp_gamut(clu->clutTable, cent, NULL, spoutf, clu, spioutf, clu);
			rspl_gam_plot(clu->clutTable, "sp_test.wrl", sptest-1);
			exit(0);
		}
#endif

	} else {	/* See if it's a .cal */
		fp->del(fp);
		fp = NULL;

		if ((cal = new_xcal()) == NULL)
			error("new_xcal failed");

		if ((cal->read(cal, prof_name)) != 0) {
			error ("File '%s' is not an ICC or .cal file",prof_name);
		}

		/* Get details of conversion (Arguments may be NULL if info not needed) */
		outs = ins = cal->colspace;
		outn = inn = cal->devchan;
	}

	if (verb > 1 && icco != NULL) {
		icmFile *op;
		if ((op = new_icmFileStd_fp(stdout)) == NULL)
			error ("Can't open stdout");
		icco->header->dump(icco->header, op, 1);
		op->del(op);
	}


	if (doplot) {
		int i, j;
		double xx[XRES];
		double yy[6][XRES];
		double start[3], end[3];

		if (cal != NULL) {
			for (i = 0; i < XRES; i++) {
				double ival = (double)i/(XRES-1.0);

				for (j = 0; j < inn; j++)
					in[j] = ival;

				/* Do the conversion */
				if (func == icmBwd || invert) {
					if ((rv = cal->inv_interp(cal, out, in)) != 0)
						error ("%d, %s",cal->errc,cal->err);
				} else {
					cal->interp(cal, out, in);
				}

				xx[i] = 100.0 * ival; 
				for (j = 0; j < outn; j++)
					yy[j][i] = 100.0 * out[j];
			}
		} else {
			/* Plot from white to black by default */
			luo->efv_wh_bk_points(luo, start, end, NULL);

			if (pstart[0] == -1000.0)
				icmCpy3(pstart, start);

			if (pend[0] == -1000.0)
				icmCpy3(pend, end);

			if (verb) {
				printf("Plotting from white %f %f %f to black %f %f %f\n",
				        pstart[0], pstart[1], pstart[2], pend[0], pend[1], pend[2]);
			}
			for (i = 0; i < XRES; i++) {
				double ival = (double)i/(XRES-1.0);

				/* Input is always Jab or Lab */
				in[0] = ival * pend[0] + (1.0 - ival) * pstart[0];
				in[1] = ival * pend[1] + (1.0 - ival) * pstart[1];
				in[2] = ival * pend[2] + (1.0 - ival) * pstart[2];
//in[1] = in[2] = 0.0;

				/* Do the conversion */
				if (invert) {
					if ((rv = luo->inv_lookup(luo, out, in)) > 1)
						error ("%d, %s",xicco->errc,xicco->err);
//printf("~1 %f: %f %f %f -> %f %f %f %f\n", ival, in[0], in[1], in[2], out[0], out[1], out[2], out[3]);
				} else {
					if ((rv = luo->lookup(luo, out, in)) > 1)
						error ("%d, %s",xicco->errc,xicco->err);
				}

				xx[i] = 100.0 * ival; 
				for (j = 0; j < outn; j++)
					yy[j][i] = 100.0 * out[j];
			}
//fflush(stdout);
		}

		/* plot order: Black Red Green Blue Yellow Purple */
		if (outs == icSigRgbData) {
			do_plot6(xx, NULL, yy[0], yy[1], yy[2], NULL, NULL, XRES);

		} else if (outs == icSigCmykData) {
			do_plot6(xx, yy[3], yy[1], NULL, yy[0], yy[2], NULL, XRES);

		} else {
		
			switch(outn) {
				case 1:
					do_plot6(xx, yy[0], NULL, NULL, NULL, NULL, NULL, XRES);
					break;
				case 2:
					do_plot6(xx, yy[0], yy[1], NULL, NULL, NULL, NULL, XRES);
					break;
				case 3:
					do_plot6(xx, yy[0], yy[1], yy[2], NULL, NULL, NULL, XRES);
					break;
				case 4:
					do_plot6(xx, yy[0], yy[1], yy[2], yy[3], NULL, NULL, XRES);
					break;
				case 5:
					do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], NULL, XRES);
					break;
				case 6:
					do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], XRES);
					break;
			}
		}

	} else {

		if (slocwarn && outs != icSigXYZData
		             && outs != icSigYxyData
		             && outs != icSigLabData
		             && outs != icxSigLChData) {
			error("Can't warn if outside spectrum locus unless XYZ like space");
		}

		/* Process colors to translate */
		for (;;) {
			int i,j;
			char *bp, *nbp;
			int outsloc = 0;

			/* Read in the next line */
			if (fgets(buf, 200, stdin) == NULL)
				break;
			if (buf[0] == '#') {
				if (verb > 0)
					fprintf(stdout,"%s\n",buf);
				continue;
			}
			/* For each input number */
			for (nbp = buf, i = 0; i < MAX_CHAN; i++) {
				bp = nbp;
				uout[i] = out[i] = in[i] = uin[i] = strtod(bp, &nbp);
				if (nbp == bp)
					break;			/* Failed */
			}
			if (i == 0)
				break;

			/* If device data and scale */
			if( ins != icxSigJabData
			 && ins != icxSigJChData
			 && ins != icSigXYZData
			 && ins != icSigLabData
			 && ins != icxSigLChData
			 && ins != icSigLuvData
			 && ins != icSigYCbCrData
			 && ins != icSigYxyData
			 && ins != icSigHsvData
			 && ins != icSigHlsData) {
				if (scale > 0.0) {
					for (i = 0; i < MAX_CHAN; i++)
						in[i] /= scale;
				}
				if (inn == 3 && in_tvenc != 0) {
					if (in_tvenc == 1) {			/* Video 16-235 range */
						icmRGB_2_VidRGB(in, in);
					} else if (in_tvenc == 2) {		/* Rec601 YCbCr */
						icmRec601_RGBd_2_YPbPr(in, in);
						icmRecXXX_YPbPr_2_YCbCr(in, in);
					} else if (in_tvenc == 3) {		/* Rec709 YCbCr */
						icmRec709_RGBd_2_YPbPr(in, in);
						icmRecXXX_YPbPr_2_YCbCr(in, in);
					} else if (out_tvenc == 4) {		/* Rec709 1250/50/2:1 YCbCr */
						icmRec709_50_RGBd_2_YPbPr(in, in);
						icmRecXXX_YPbPr_2_YCbCr(in, in);
					} else if (out_tvenc == 5) {		/* Rec2020 Non-constant Luminance YCbCr */
						icmRec2020_NCL_RGBd_2_YPbPr(in, in);
						icmRecXXX_YPbPr_2_YCbCr(in, in);
					} else if (out_tvenc == 6) {		/* Rec2020 Non-constant Luminance YCbCr */
						icmRec2020_CL_RGBd_2_YPbPr(in, in);
						icmRecXXX_YPbPr_2_YCbCr(in, in);
					}
				}
			}

			if (repXYZ100 && ins == icSigXYZData) {
				in[0] /= 100.0;
				in[1] /= 100.0;
				in[2] /= 100.0;
			}

			if (repYxy && ins == icSigYxyData) {
				icmYxy2XYZ(in, in);
			}

			/* JCh -> Jab & LCh -> Lab */
			if ((repJCh && ins == icxSigJChData) 
			 || (repLCh && ins == icxSigLChData)) {
				double C = in[1];
				double h = in[2];
				in[1] = C * cos(3.14159265359/180.0 * h);
				in[2] = C * sin(3.14159265359/180.0 * h);
			}

			/* Do conversion */
			if (cal != NULL) {	/* .cal */
				if (func == icmBwd || invert) {
					if ((rv = cal->inv_interp(cal, out, in)) != 0)
						error ("%d, %s",cal->errc,cal->err);
				} else {
					cal->interp(cal, out, in);
					rv = 0;
				}

			} else {	/* ICC */
				if (invert) {
					for (j = 0; j < MAX_CHAN; j++)
						out[j] = in[j];		/* Carry any auxiliary value to out for lookup */
					if ((rv = luo->inv_lookup(luo, out, in)) > 1)
						error ("%d, %s",xicco->errc,xicco->err);
				} else {
					if ((rv = luo->lookup(luo, out, in)) > 1)
						error ("%d, %s",xicco->errc,xicco->err);
				}
			}

			if (slocwarn) {
				double xyz[3];

				if (outs == icSigLabData || outs == icxSigLChData)
					icmLab2XYZ(&icmD50, out, xyz);	
				else
					icmCpy3(xyz, out);

				outsloc = icx_outside_spec_locus(chlp, xyz);
			}

			/* Copy conversion out value so that we can create user values */
			for (i = 0; i < MAX_CHAN; i++)
				uout[i] = out[i];

			if (repXYZ100 && outs == icSigXYZData) {
				uout[0] *= 100.0;
				uout[1] *= 100.0;
				uout[2] *= 100.0;
			}

			if (repYxy && outs == icSigYxyData) {
				icmXYZ2Yxy(uout, uout);
			}

			/* Jab -> JCh and Lab -> LCh */
			if ((repJCh && outs == icxSigJChData) 
			 || (repLCh && outs == icxSigLChData)) {
				double a = uout[1];
				double b = uout[2];
				uout[1] = sqrt(a * a + b * b);
			    uout[2] = (180.0/3.14159265359) * atan2(b, a);
				uout[2] = (uout[2] < 0.0) ? uout[2] + 360.0 : uout[2];
			}

			/* If device data and scale */
			if( outs != icxSigJabData
			 && outs != icxSigJChData
			 && outs != icSigXYZData
			 && outs != icSigLabData
			 && outs != icxSigLChData
			 && outs != icSigLuvData
			 && outs != icSigYCbCrData
			 && outs != icSigYxyData
			 && outs != icSigHsvData
			 && outs != icSigHlsData) {
				if (outn == 3 && out_tvenc != 0) {
					if (out_tvenc == 1) {				/* Video 16-235 range */
						icmVidRGB_2_RGB(uout, uout);
					} else if (out_tvenc == 2) {		/* Rec601 YCbCr */
						icmRecXXX_YCbCr_2_YPbPr(uout, uout);
						icmRec601_YPbPr_2_RGBd(uout, uout);
					} else if (out_tvenc == 3) {		/* Rec709 1150/60/2:1 YCbCr */
						icmRecXXX_YCbCr_2_YPbPr(uout, uout);
						icmRec709_YPbPr_2_RGBd(uout, uout);
					} else if (out_tvenc == 4) {		/* Rec709 1250/50/2:1 YCbCr */
						icmRecXXX_YCbCr_2_YPbPr(uout, uout);
						icmRec709_50_YPbPr_2_RGBd(uout, uout);
					} else if (out_tvenc == 5) {		/* Rec2020 Non-constant Luminance YCbCr */
						icmRecXXX_YCbCr_2_YPbPr(uout, uout);
						icmRec2020_NCL_YPbPr_2_RGBd(uout, uout);
					} else if (out_tvenc == 6) {		/* Rec2020 Non-constant Luminance YCbCr */
						icmRecXXX_YCbCr_2_YPbPr(uout, uout);
						icmRec2020_CL_YPbPr_2_RGBd(uout, uout);
					}
				}
				if (scale > 0.0) {
					for (i = 0; i < MAX_CHAN; i++)
						uout[i] *= scale;
				}
			}

			/* Output the results */
			if (verb > 0) {
				for (j = 0; j < inn; j++) {
					if (j > 0)
						fprintf(stdout," %f",uin[j]);
					else
						fprintf(stdout,"%f",uin[j]);
				}
				if (cal != NULL)
					printf(" [%s] -> ", icx2str(icmColorSpaceSignature, ins));
				else
					printf(" [%s] -> %s -> ", icx2str(icmColorSpaceSignature, ins),
					                          icm2str(icmLuAlg, alg));
			}

			for (j = 0; j < outn; j++) {
				if (j > 0)
					fprintf(stdout," %f",uout[j]);
				else
					fprintf(stdout,"%f",uout[j]);
			}
			if (verb > 0)
				printf(" [%s]", icx2str(icmColorSpaceSignature, outs));

			if (verb > 0 && tlimit >= 0) {
				double tot;	
				for (tot = 0.0, j = 0; j < outn; j++) {
					tot += out[j];
				}
				printf(" Lim %f",tot);
			}
			if (outsloc)
				fprintf(stdout,"(Imaginary)");

			if (verb == 0 || rv == 0)
				fprintf(stdout,"\n");
			else {
				fprintf(stdout," (clip)\n");

				/* This probably isn't right - we need to convert */
				/* in[] to Lab to Jab if it is not in that space, */
				/* so we can do a delta E on it. */
				if (actual && aluo != NULL) {
					double cin[MAX_CHAN], de;
					if ((rv = aluo->lookup(aluo, cin, out)) > 1)
						error ("%d, %s",xicco->errc,xicco->err);

					for (de = 0.0, j = 0; j < inn; j++) {
						de += (cin[j] - in[j]) * (cin[j] - in[j]);
					}
					de = sqrt(de);
					printf("[Actual ");
					for (j = 0; j < inn; j++) {
						if (j > 0)
							fprintf(stdout," %f",cin[j]);
						else
							fprintf(stdout,"%f",cin[j]);
					}
					printf(", deltaE %f]\n",de);
				}
			}
		}
	}

	/* Done with lookup object */
	if (aluo != NULL && aluo != luo)
		luo->del(aluo);
	if (luo != NULL)
		luo->del(luo);
	if (cal != NULL)
		cal->del(cal);
	if (xicco != NULL)
		xicco->del(xicco);		/* Expansion wrapper */
	if (icco != NULL)
		icco->del(icco);		/* Icc */
	if (fp != NULL)
		fp->del(fp);

	return 0;
}

