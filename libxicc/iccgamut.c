
/* 
 * iccgamut
 *
 * Produce color surface gamut of an ICC profile.
 *
 * Author:  Graeme W. Gill
 * Date:    19/3/00
 * Version: 1.00
 *
 * Copyright 2000 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */


/*
 * TTBD:
 *       To support CIACAM02 properly, need to cope with viewing parameters ?
 */

#define SURFACE_ONLY
#define GAMRES 10.0		/* Default surface resolution */

#define USE_CAM_CLIP_OPT		/* Use CAM space to clip in */

#define RGBRES 33	/* 33 */
#define CMYKRES 17	/* 17 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "icc.h"
#include "xicc.h"
#include "gamut.h"
#include "counters.h"
#include "vrml.h"
#include "ui.h"

static void diag_gamut(icxLuBase *p, double detail, int doaxes,
                       double tlimit, double klimit, char *outname);

void usage(char *diag) {
	int i;
	fprintf(stderr,"Create Lab/Jab gamut plot Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: iccgamut [options] profile\n");
	if (diag != NULL)
		fprintf(stderr,"Diagnostic: %s\n",diag);
	fprintf(stderr," -v            Verbose\n");
	fprintf(stderr," -d sres       Surface resolution details 1.0 - 50.0\n");
	fprintf(stderr," -w            emit %s %s file as well as CGATS .gam file\n",vrml_format(),vrml_ext());
	fprintf(stderr," -n            Don't add %s axes or white/black point\n",vrml_format());
	fprintf(stderr," -k            Add %s markers for prim. & sec. \"cusp\" points\n",vrml_format());
	fprintf(stderr,"               (Set env. ARGYLL_3D_DISP_FORMAT to VRML, X3D or X3DOM to change format)\n");
	fprintf(stderr," -f function   f = forward*, b = backwards\n");
	fprintf(stderr," -i intent     p = perceptual, r = relative colorimetric,\n");
	fprintf(stderr,"               s = saturation, a = absolute (default), d = profile default\n");
//  fprintf(stderr,"               P = absolute perceptual, S = absolute saturation\n");
	fprintf(stderr," -p oride      l = Lab_PCS (default), j = %s Appearance Jab\n",icxcam_description(cam_default));
	fprintf(stderr," -o order      n = normal (priority: lut > matrix > monochrome)\n");
	fprintf(stderr,"               r = reverse (priority: monochrome > matrix > lut)\n");
	fprintf(stderr," -l tlimit     set total ink limit, 0 - 400%% (estimate by default)\n");
	fprintf(stderr," -L klimit     set black ink limit, 0 - 100%% (estimate by default)\n");
	fprintf(stderr," -c viewcond   set viewing conditions for %s,\n",icxcam_description(cam_default));
	fprintf(stderr,"               either an enumerated choice, or a series of parameter:value changes\n");
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
    fprintf(stderr," -s                    Create special cube surface topology plot\n");
	fprintf(stderr,"\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	char prof_name[MAXNAMEL+1];
	char *xl, out_name[MAXNAMEL+4+1];
	icmFile *fp;
	icc *icco;
	xicc *xicco;
	gamut *gam;
	int verb = 0;
	int rv = 0;
	int vrml = 0;
	int doaxes = 1;
	int docusps = 0;
	double gamres = GAMRES;		/* Surface resolution */
	int special = 0;			/* Special surface plot */
	int fl = 0;					/* luobj flags */
	icxInk ink;					/* Ink parameters */
	int tlimit = -1;			/* Total ink limit as a % */
	int klimit = -1;			/* Black ink limit as a % */
	icxViewCond vc;				/* Viewing Condition for CIECAM */
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

	icxLuBase *luo;

	/* Lookup parameters */
	icmLookupFunc     func   = icmFwd;				/* Default */
	icRenderingIntent intent = -1;					/* Default */
	icColorSpaceSignature pcsor = icSigLabData;		/* Default */
	icmLookupOrder    order  = icmLuOrdNorm;		/* Default */
	
	error_program = argv[0];

	if (argc < 2)
		usage("Too few parameters");

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
				usage(NULL);

			/* function */
			else if (argv[fa][1] == 'f' || argv[fa][1] == 'F') {
				fa = nfa;
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
					default:
						usage("Unrecognised parameter after flag -f");
				}
			}

			/* Intent */
			else if (argv[fa][1] == 'i' || argv[fa][1] == 'I') {
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -i");
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
						usage("Unrecognised parameter after flag -i");
				}
			}

			/* Search order */
			else if (argv[fa][1] == 'o' || argv[fa][1] == 'O') {
				fa = nfa;
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
						usage("Unrecognised parameter after flag -o");
				}
			}

			/* PCS override */
			else if (argv[fa][1] == 'p' || argv[fa][1] == 'P') {
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -p");
    			switch (na[0]) {
					case 'l':
						pcsor = icSigLabData;
						break;
					case 'j':
						pcsor = icxSigJabData;
						break;
					default:
						usage("Unrecognised parameter after flag -p");
				}
			}

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}
			/* VRML output */
			else if (argv[fa][1] == 'w' || argv[fa][1] == 'W') {
				vrml = 1;
			}
			/* No axis output in vrml */
			else if (argv[fa][1] == 'n' || argv[fa][1] == 'N') {
				doaxes = 0;
			}
			/* Do cusp markers in vrml */
			else if (argv[fa][1] == 'k' || argv[fa][1] == 'K') {
				docusps = 1;
			}
			/* Special */
			else if (argv[fa][1] == 's' || argv[fa][1] == 'S') {
				special = 1;
			}
			/* Ink limit */
			else if (argv[fa][1] == 'l') {
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -l");
				tlimit = atoi(na);
			}

			else if (argv[fa][1] == 'L') {
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -L");
				klimit = atoi(na);
			}


			/* Surface Detail */
			else if (argv[fa][1] == 'd' || argv[fa][1] == 'D') {
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -d");
				gamres = atof(na);
				if (gamres < 0.1 || gamres > 50.0)
					usage("Parameter after flag -d seems out of range");
			}

			/* Viewing conditions */
			else if (argv[fa][1] == 'c' || argv[fa][1] == 'C') {
				fa = nfa;
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
						vc_s = vc_none;		/* Automatic from Lv */
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
			}
			else 
				usage("Unknown flag");
		} else
			break;
	}

	if (intent == -1) {
		if (pcsor == icxSigJabData)
			intent = icRelativeColorimetric;	/* Default to icxAppearance */
		else
			intent = icAbsoluteColorimetric;	/* Default to icAbsoluteColorimetric */
	}

	if (fa >= argc || argv[fa][0] == '-') usage("Expected profile name");
	strncpy(prof_name, argv[fa],MAXNAMEL); prof_name[MAXNAMEL] = '\000';

	/* Open up the profile for reading */
	if ((fp = new_icmFileStd_name(prof_name,"r")) == NULL)
		error ("Can't open file '%s'",prof_name);

	if ((icco = new_icc()) == NULL)
		error ("Creation of ICC object failed");

	if ((rv = icco->read(icco,fp,0)) != 0)
		error ("%d, %s",rv,icco->err);

	if (verb) {
		icmFile *op;
		if ((op = new_icmFileStd_fp(stdout)) == NULL)
			error ("Can't open stdout");
		icco->header->dump(icco->header, op, 1);
		op->del(op);
	}

	/* Wrap with an expanded icc */
	if ((xicco = new_xicc(icco)) == NULL)
		error ("Creation of xicc failed");

	/* Set the ink limits */
	icxDefaultLimits(xicco, &ink.tlimit, tlimit/100.0, &ink.klimit, klimit/100.0);

	if (verb) {
		if (ink.tlimit >= 0.0)
			printf("Total ink limit assumed is %3.0f%%\n",100.0 * ink.tlimit);
		if (ink.klimit >= 0.0)
			printf("Black ink limit assumed is %3.0f%%\n",100.0 * ink.klimit);
	}

	/* Setup a safe ink generation (not used) */
	ink.KonlyLmin = 0;		/* Use normal black Lmin for locus */
	ink.k_rule = icxKluma5k;
	ink.c.Ksmth = ICXINKDEFSMTH;	/* Default smoothing */
	ink.c.Kskew = ICXINKDEFSKEW;	/* default curve skew */
	ink.c.Kstle = 0.0;		/* Min K at white end */
	ink.c.Kstpo = 0.0;		/* Start of transition is at white */
	ink.c.Kenle = 1.0;		/* Max K at black end */
	ink.c.Kenpo = 1.0;		/* End transition at black */
	ink.c.Kshap = 1.0;		/* Linear transition */

	/* Setup the default viewing conditions */
	if (xicc_enum_viewcond(xicco, &vc, -1, NULL, 0, NULL) == -2)
		error ("%d, %s",xicco->errc, xicco->err);

	if (vc_e != -1)
		if (xicc_enum_viewcond(xicco, &vc, vc_e, NULL, 0, NULL) == -2)
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

	fl |= ICX_CLIP_NEAREST;		/* Don't setup rev uncessarily */

#ifdef USE_CAM_CLIP_OPT
	 fl |= ICX_CAM_CLIP;
#endif

#ifdef NEVER
	printf("~1 output space flags = 0x%x\n",fl);
	printf("~1 output space intent = %s\n",icx2str(icmRenderingIntent,intent));
	printf("~1 output space pcs = %s\n",icx2str(icmColorSpaceSignature,pcsor));
	printf("~1 output space viewing conditions =\n"); xicc_dump_viewcond(&vc);
	printf("~1 output space inking =\n"); xicc_dump_inking(&ink);
#endif

	strcpy(out_name, prof_name);
	if ((xl = strrchr(out_name, '.')) == NULL)	/* Figure where extention is */
		xl = out_name + strlen(out_name);

	strcpy(xl,".gam");

	/* Get a expanded color conversion object */
	if ((luo = xicco->get_luobj(xicco, fl, func, intent, pcsor, order, &vc, &ink)) == NULL)
		error ("%d, %s",xicco->errc, xicco->err);

	if (special) {
		if (func != icmFwd)
			error("Must be forward direction for special plot");
		xl[0] = '\000';			/* remove extension */
		diag_gamut(luo, gamres, doaxes, tlimit/100.0, klimit/100.0, out_name); 
	} else {
		/* Creat a gamut surface */
		if ((gam = luo->get_gamut(luo, gamres)) == NULL)
			error ("%d, %s",xicco->errc, xicco->err);

		if (gam->write_gam(gam, out_name))
			error ("write gamut failed on '%s'",out_name);

		if (vrml) {
			xl[0] = '\000';			/* remove extension */
			if (gam->write_vrml(gam,out_name, doaxes, docusps))
				error ("write vrml failed on '%s%s'",out_name, vrml_ext());
		}

		if (verb) {
			printf("Total volume of gamut is %f cubic colorspace units\n",gam->volume(gam));
		}
		gam->del(gam);
	}

	luo->del(luo);			/* Done with lookup object */

	xicco->del(xicco);		/* Expansion wrapper */
	icco->del(icco);		/* Icc */
	fp->del(fp);


	return 0;
}

/* -------------------------------------------- */
/* Code for special gamut surface plot */

#define GAMUT_LCENT 50

/* Create a diagnostic gamut, illustrating */
/* device space "fold-over" */
static void diag_gamut(
icxLuBase *p,		/* Lookup object */
double detail,		/* Gamut resolution detail */
int doaxes,			/* Do Lab axes */
double tlimit,		/* Total ink limit */
double klimit,		/* K ink limit */
char *outname		/* Output VRML/X3D file (no extension) */
) {
	int i, j;
	vrml *wrl;
	int vix;						/* Vertex index */
	DCOUNT(coa, MXDI, p->inputChan, 0, 0, 2);

	double col[1 << MXDI][3];		/* Color asigned to each major vertex */
	int res;

	if (tlimit < 0.0)
		tlimit = p->inputChan;
	if (klimit < 0.0)
		klimit = 1.0;

	/* Asign some colors to the combination nodes */
	for (i = 0; i < (1 << p->inputChan); i++) {
		int a, b, c, j;
		double h;

		j = (i ^ 0x5a5a5a5a) % (1 << p->inputChan);
		h = (double)j/((1 << p->inputChan)-1);

		/* Make fully saturated with chosen hue */
		if (h < 1.0/3.0) {
			a = 0;
			b = 1;
			c = 2;
		} else if (h < 2.0/3.0) {
			a = 1;
			b = 2;
			c = 0;
			h -= 1.0/3.0;
		} else {
			a = 2;
			b = 0;
			c = 1;
			h -= 2.0/3.0;
		}
		h *= 3.0;

		col[i][a] = (1.0 - h);
		col[i][b] = h;
		col[i][c] = d_rand(0.0, 1.0);
	}

	if (detail > 0.0)
		res = (int)(100.0/detail);	/* Establish an appropriate sampling density */
	else
		res = 4;

	if (res < 2)
		res = 2;

	if ((wrl = new_vrml(outname, doaxes, vrml_lab)) == NULL)
		error("Error creating wrl output '%s%s'",outname,vrml_ext());

	wrl->start_line_set(wrl, 0);		/* Start set 0 */

	/* Itterate over all the faces in the device space */
	/* generating the vertx positions. */
	DC_INIT(coa);
	vix = 0;
	while(!DC_DONE(coa)) {
		int e, m1, m2;
		double in[MXDI], xb, yb;
		double inl[MXDI];
		double out[3];
		double sum;

		/* Scan only device surface */
		for (m1 = 0; m1 < p->inputChan; m1++) {
			if (coa[m1] != 0)
				continue;

			for (m2 = m1 + 1; m2 < p->inputChan; m2++) {
				int x, y;

				if (coa[m2] != 0)
					continue;

				for (e = 0; e < p->inputChan; e++)
					in[e] = (double)coa[e];		/* Base value */

				/* Scan over 2D device space face */
				for (x = 0; x < res; x++) {				/* step over surface */
					xb = in[m1] = x/(res - 1.0);
					for (y = 0; y < res; y++) {
						double rgb[3], rgb2[3];
						int v0, v1, v2, v3;
						int ix[4];

						yb = in[m2] = y/(res - 1.0);

						/* Check ink limit */
						for (sum = 0.0, e = 0; e < p->inputChan; e++)
							sum += inl[e] = in[e];
						if (sum >= tlimit) {
							for (e = 0; e < p->inputChan; e++)
								inl[e] *= tlimit/sum;
						}
						if (p->inputChan >= 3 && inl[3] >= klimit)
							inl[3] = klimit;

						/* Lookup L*a*b* value */
						p->lookup(p, out, inl);

						/* Compute color */
						for (v0 = 0, e = 0; e < p->inputChan; e++)
							v0 |= coa[e] ? (1 << e) : 0;		/* Binary index */

						v1 = v0 | (1 << m2);				/* Y offset */
						v2 = v0 | (1 << m2) | (1 << m1);	/* X+Y offset */
						v3 = v0 | (1 << m1);				/* Y offset */

						/* Linear interp between the main verticies */
						for (j = 0; j < 3; j++) {
							rgb[j] = (1.0 - yb) * (1.0 - xb) * col[v0][j]
							       +        yb  * (1.0 - xb) * col[v1][j]
							       + (1.0 - yb) *        xb  * col[v3][j]
							       +        yb  *        xb  * col[v2][j];
						}
						/* re-order the color */
						rgb2[0] = rgb[1];
						rgb2[1] = rgb[2];
						rgb2[2] = rgb[0];
						wrl->add_col_vertex(wrl, 0, out, rgb2);

						/* Add the quad vertexes */
						if (x < (res-1) && y < (res-1)) {
							ix[0] = vix;
							ix[1] = vix + 1;
							ix[2] = vix + 1 + res;
							ix[3] = vix + res;
							wrl->add_quad(wrl, 0, ix);
						}
						vix++;
					}
				}
			}
		}
		/* Increment index within block */
		DC_INC(coa);
	}

	wrl->make_quads_vc(wrl, 0, 0.0);	/* Make set 0 with color per vertex and 0 transparence */

	wrl->del(wrl);
}

