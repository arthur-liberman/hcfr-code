
/* 
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    2006/5/9
 * Version: 1.00
 *
 * Copyright 2006 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/*
 * This is some test code to test the Daylight and Plankian spectra, 
 * Correlated and Visual Color Temperatures, and CRI.
 */

#include <stdio.h>
#include <math.h>
#include "xspect.h"
#include "numlib.h"
#include "plot.h"
#include "ui.h"

#define PLANKIAN
#define XRES 500


#ifdef PLANKIAN
#define BBTYPE icxIT_Ptemp
#else
#define BBTYPE icxIT_Dtemp
#endif

static int do_spec(char *name, xspect *sp) {
	int i;
	double xyz[3];		/* Color temperature */
	double Yxy[3];
	double xx[XRES];
	double y1[XRES];
	double cct, vct;
	double cct_xyz[3], vct_xyz[3];
	double cct_lab[3], vct_lab[3];
	icmXYZNumber wp;
	double de;

	printf("\n");

	/* Compute XYZ of illuminant */
	if (icx_ill_sp2XYZ(xyz, icxOT_CIE_1931_2, NULL, icxIT_custom, 0, sp) != 0) 
		error ("icx_sp_temp2XYZ returned error");

	icmXYZ2Yxy(Yxy, xyz);

	printf("Type = %s\n",name);
	printf("XYZ = %f %f %f, x,y = %f %f\n", xyz[0], xyz[1], xyz[2], Yxy[1], Yxy[2]);
	
	/* Compute CCT */
	if ((cct = icx_XYZ2ill_ct(cct_xyz, BBTYPE, icxOT_CIE_1931_2, NULL, xyz, NULL, 0)) < 0)
		error ("Got bad cct\n");

	/* Compute VCT */
	if ((vct = icx_XYZ2ill_ct(vct_xyz, BBTYPE, icxOT_CIE_1931_2, NULL, xyz, NULL, 1)) < 0)
		error ("Got bad vct\n");

#ifdef PLANKIAN
	printf("CCT = %f, VCT = %f\n",cct, vct);
#else
	printf("CDT = %f, VDT = %f\n",cct, vct);
#endif

	{
		int invalid = 0;
		double cri;
		cri = icx_CIE1995_CRI(&invalid, sp);
		printf("CRI = %.1f%s\n",cri,invalid ? " (Invalid)" : "");
	}

	/* Use modern color difference - gives a better visual match */
	icmAry2XYZ(wp, vct_xyz);
	icmXYZ2Lab(&wp, cct_lab, cct_xyz);
	icmXYZ2Lab(&wp, vct_lab, vct_xyz);
	de = icmCIE2K(cct_lab, vct_lab);
	printf("CIEDE2000 Delta E = %f\n",de);

	/* Plot spectrum out */
	for (i = 0; i < XRES; i++) {
		double ww;

		ww = (sp->spec_wl_long - sp->spec_wl_short)
		   * ((double)i/(XRES-1.0)) + sp->spec_wl_short;
	
		xx[i] = ww;
		y1[i] = value_xspect(sp, ww);
	}
	do_plot(xx,y1,NULL,NULL,i);

	return 0;
}


void usage(void) {
	fprintf(stderr,"Plot spectrum and calculate CCT and VCT\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: ccttest [infile.sp]\n");
	fprintf(stderr," [infile.sp]      spectrum to plot\n");
	fprintf(stderr,"                  default is all built in illuminants\n");
	exit(1);
}

int
main(
	int argc,
	char *argv[]
) {
	int fa,nfa;				/* argument we're looking at */
	int k;
	int verb = 0;
	char in_name[100] = { '\000' };		/* Spectrum name */
	double temp;
	xspect sp;				/* Spectra */
	icxIllumeType ilType;
	char buf[200];

	error_program = argv[0];

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

			/* Verbosity */
			if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			} else {
				usage();
			}
		}
		else
			break;
	}

	if (fa < argc && argv[fa][0] != '-')
		strcpy(in_name,argv[fa]);

#ifndef NEVER /* hack test */
	{
#define NTESTS 5
		int i;
		double cct, vct;
		double de1, de2;
		double xyz[NTESTS][3] = {
			{ 92.250000, 97.750000, 89.650000 },	/* Rogers spotread verification result */
			{ 94.197915, 97.686362, 80.560411, },	/* Rogers target */
			{ 93.51,     97.12,     80.86 },	/* Rogers test ? */
			{ 75.31, 78.20, 64.59 },		/* Mac LCD test */
			{ 42.98, 44.62, 36.95 }			/* Hitachie CRT test */
		};
		double axyz[3];
		double lab[3], alab[3];

		for (i = 0; i < NTESTS; i++) {
			/* Compute CCT */
			if ((cct = icx_XYZ2ill_ct(axyz, icxIT_Ptemp, icxOT_CIE_1931_2, NULL, xyz[i], NULL, 0)) < 0)
				error ("Got bad cct\n");
	
			axyz[0] /= axyz[1];
			axyz[2] /= axyz[1];
			axyz[1] /= axyz[1];
			icmXYZ2Lab(&icmD50, alab, axyz);
			axyz[0] = xyz[i][0] / xyz[i][1];
			axyz[2] = xyz[i][2] / xyz[i][1];
			axyz[1] = xyz[i][1] / xyz[i][1];
			icmXYZ2Lab(&icmD50, lab, axyz);
			de1 = icmCIE2K(lab, alab);

			/* Compute VCT */
			if ((vct = icx_XYZ2ill_ct(axyz, icxIT_Ptemp, icxOT_CIE_1931_2, NULL, xyz[i], NULL, 1)) < 0)
				error ("Got bad vct\n");
			axyz[0] /= axyz[1];
			axyz[2] /= axyz[1];
			axyz[1] /= axyz[1];
			icmXYZ2Lab(&icmD50, alab, axyz);
			axyz[0] = xyz[i][0] / xyz[i][1];
			axyz[2] = xyz[i][2] / xyz[i][1];
			axyz[1] = xyz[i][1] / xyz[i][1];
			icmXYZ2Lab(&icmD50, lab, axyz);
			de2 = icmCIE2K(lab, alab);

			printf("XYZ %f %f %f, CCT = %f de %f, VCT = %f de %f\n",xyz[i][0], xyz[i][1], xyz[i][2], cct, de1, vct, de1);

			/* Compute CCT */
			if ((cct = icx_XYZ2ill_ct(axyz, icxIT_Dtemp, icxOT_CIE_1931_2, NULL, xyz[i], NULL, 0)) < 0)
				error ("Got bad cct\n");
			axyz[0] /= axyz[1];
			axyz[2] /= axyz[1];
			axyz[1] /= axyz[1];
			icmXYZ2Lab(&icmD50, alab, axyz);
			axyz[0] = xyz[i][0] / xyz[i][1];
			axyz[2] = xyz[i][2] / xyz[i][1];
			axyz[1] = xyz[i][1] / xyz[i][1];
			icmXYZ2Lab(&icmD50, lab, axyz);
			de1 = icmCIE2K(lab, alab);

			/* Compute VCT */
			if ((vct = icx_XYZ2ill_ct(axyz, icxIT_Dtemp, icxOT_CIE_1931_2, NULL, xyz[i], NULL, 1)) < 0)
				error ("Got bad vct\n");

			axyz[0] /= axyz[1];
			axyz[2] /= axyz[1];
			axyz[1] /= axyz[1];
			icmXYZ2Lab(&icmD50, alab, axyz);
			axyz[0] = xyz[i][0] / xyz[i][1];
			axyz[2] = xyz[i][2] / xyz[i][1];
			axyz[1] = xyz[i][1] / xyz[i][1];
			icmXYZ2Lab(&icmD50, lab, axyz);
			de2 = icmCIE2K(lab, alab);

			printf("XYZ %f %f %f, CDT = %f de %f, VDT = %f de %f\n",xyz[i][0], xyz[i][1], xyz[i][2], cct, de1, vct, de2);
		}
	}
#endif

	if (in_name[0] != '\000') {
		if (read_xspect(&sp, in_name) != 0)
			error ("Unable to read custom spectrum '%s'",in_name);

		sprintf(buf, "File '%s'",in_name);

		do_spec(buf, &sp);

	} else {

		/* For each standard illuminant */
		for (ilType = icxIT_A; ilType <= icxIT_F10; ilType++) {
			char *inm = NULL;
	
			switch (ilType) {
			    case icxIT_A:
					inm = "A"; break;
			    case icxIT_C:
					inm = "C"; break;
			    case icxIT_D50:
					inm = "D50"; break;
			    case icxIT_D50M2:
					inm = "D50M2"; break;
			    case icxIT_D65:
					inm = "D65"; break;
			    case icxIT_F5:
					inm = "F5"; break;
			    case icxIT_F8:
					inm = "F8"; break;
			    case icxIT_F10:
					inm = "F10"; break;
				default:
					inm = "Unknown"; break;
					break;
			}
	
			if (standardIlluminant(&sp, ilType, 0) != 0)
				error ("standardIlluminant returned error");
		
			do_spec(inm, &sp);
		}

		/* For each material and illuminant */
		for (temp = 2500; temp <= 9000; temp += 500) {
		
			for (k = 0; k < 2; k++) {
	
				ilType = k == 0 ? icxIT_Dtemp : icxIT_Ptemp;
	
				if (standardIlluminant(&sp, ilType, temp) != 0)
					error ("standardIlluminant returned error");
		
				sprintf(buf, "%s at %f", k == 0 ? "Daylight" : "Black body", temp);
	
				do_spec(buf, &sp);
			}
		}

	}
	return 0;
}








