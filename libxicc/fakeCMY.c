
/* 
 * Create a synthetic .ti3 file for a CMY "device",
 * that exactly maps to the surface points and neutral
 * axis of the CMYK profile it is derived from.
 *
 * Author:  Graeme W. Gill
 * Date:    2004/6/27
 * Version: 1.00
 *
 * Copyright 2004 Graeme W. Gill
 * Please refer to License.txt file for details.
 */

/* TTBD:
 *
 *	Add a flag to create RGB instead, by simply inverting the CMY values
 *	when writing to the .ti3 file.
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
#include "icc.h"
#include "xicc.h"
#include "cgats.h"
#include "targen.h"
#include "ui.h"

#define EXTRA_NEUTRAL 50		/* Crude way of increasing weighting */

/* ---------------------------------------- */

void usage(char *diag) {
	fprintf(stderr,"Create a fake CMY data file from a CMYK profile, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: fakeCMY [option] profile.icm fake.ti3\n");
	if (diag != NULL)
		fprintf(stderr,"Diagnostic: %s\n",diag);
	fprintf(stderr," -v          verbose\n");
	fprintf(stderr," -r res      set surface point resolution (default 3)\n");
	fprintf(stderr," -l tlimit    set total ink limit, 0 - 400%% (estimate by default)\n");
	fprintf(stderr," -L klimit    set black ink limit, 0 - 100%% (estimate by default)\n");
	exit(1);
}

/* Fake up a vector direction clip in Lab space */
static void visect(
	icxLuBase *luo,		/* xicc lookup object */
	double *pout,		/* PCS result (may be NULL) */
	double *dout,		/* Device result (may be NULL) */
	double *orig,		/* Origin of vector (PCS) */
	double *vec			/* Input vector (PCS) */
) {
	int i;
	int rv;
	double sum, step, inc[3];	/* 1 Delta E increment */
	double try[3];				/* Trial PCS */
	double res[4];				/* Trial result device values */

	for (sum = 0.0, i = 0; i < 3; i++) {
		double tt = vec[i] - orig[i];
		sum += tt * tt;
	}
	sum = sqrt(sum);
	for (i = 0; i < 3; i++) {
		inc[i] = (vec[i] - orig[i])/sum;
	}
//printf("~1 orig %f %f %f, vec %f %f %f\n",orig[0],orig[1],orig[2],vec[0],vec[1],vec[3]);
//printf("~1 inc %f %f %f\n",inc[0],inc[1],inc[2]);
	
	/* Increment in vector direction until we clip */
	for (i = 0; i < 3; i++)
		try[i] = 0.5 * (orig[i] + vec[i]);

	for (step = 20.0;;) {
		rv = luo->inv_lookup(luo, res, try);
		if (rv > 1)
			error ("inv_lookup failed");
//printf("~1 trial %f %f %f returned %d\n",try[0],try[1],try[2],rv);
		if (rv == 1) {
			if (step <= 0.1)
				break;
			/* Back off, and use smaller steps */
			for (i = 0; i < 3; i++)
				try[i] -= step * inc[i];
			step *= 0.5;
		}
		for (i = 0; i < 3; i++)
			try[i] += step * inc[i];
	}

	if (dout != NULL)
		for (i = 0; i < 3; i++)
			dout[i] = res[i];

	/* Lookup the clipped PCS */
	if (pout != NULL)
		luo->lookup(luo, pout, res);
}

int
main(
	int argc,
	char *argv[]
) {
	int fa,nfa;				/* argument we're looking at */
	int verb = 0;
	double tlimit = -1.0;	/* Total ink limit */
	double klimit = -1.0;	/* Black ink limit */
	int tres = 3;			/* Resolution of suface grid */
	int gres = tres+2;		/* Resolution of grey points */
	char in_name[100];
	char out_name[100];
	icmFile *rd_fp;
	icc *rd_icco;
	int rv = 0;
	icColorSpaceSignature ins, outs;	/* Type of input and output spaces */
	xicc *xicco;
	icxLuBase *luo;
	icxInk ink;				/* Ink parameters */
	fxpos *fxlist = NULL;	/* Fixed point list for full spread */
	int fxlist_a = 0;		/* Fixed point list allocation */
	int fxno = 0;			/* The number of fixed points */
	int gc[MXTD];			/* Grid coordinate */

	cgats *ocg;				/* output cgats structure */
	time_t clk = time(0);
	struct tm *tsp = localtime(&clk);
	char *atm = asctime(tsp); /* Ascii time */

	int i, j;
	
	error_program = argv[0];

	if (argc < 2)
		usage("Not enough arguments");

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
			}

			/* Ink limits */
			else if (argv[fa][1] == 'l') {
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -l");
				tlimit = atoi(na)/100.0;
			}

			else if (argv[fa][1] == 'L') {
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -L");
				klimit = atoi(na)/100.0;
			}

			/* Resolution */
			else if (argv[fa][1] == 'r' || argv[fa][1] == 'R') {
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -r");
				tres = atoi(na);
			}

			else if (argv[fa][1] == '?')
				usage(NULL);
			else 
				usage("Unknown flag");
		}
		else
			break;
	}

	if (fa >= argc || argv[fa][0] == '-') usage("Expected input profile name");
	strcpy(in_name,argv[fa++]);

	if (fa >= argc || argv[fa][0] == '-') usage("Expected output .ti3 name");
	strcpy(out_name,argv[fa++]);

	/* Open up the file for reading */
	if ((rd_fp = new_icmFileStd_name(in_name,"r")) == NULL)
		error ("Read: Can't open file '%s'",in_name);

	if ((rd_icco = new_icc()) == NULL)
		error ("Read: Creation of ICC object failed");

	/* Read the header and tag list */
	if ((rv = rd_icco->read(rd_icco,rd_fp,0)) != 0)
		error ("Read: %d, %s",rv,rd_icco->err);


	/* Wrap with an expanded icc */
	if ((xicco = new_xicc(rd_icco)) == NULL)
		error ("Creation of xicc failed");
	
	/* Setup ink limit */

	/* Set the default ink limits if not set on command line */
	icxDefaultLimits(xicco, &ink.tlimit, tlimit, &ink.klimit, klimit);

	if (verb) {
		if (ink.tlimit >= 0.0)
			printf("Total ink limit assumed is %3.0f%%\n",100.0 * ink.tlimit);
		if (ink.klimit >= 0.0)
			printf("Black ink limit assumed is %3.0f%%\n",100.0 * ink.klimit);
	}

	ink.c.Ksmth = ICXINKDEFSMTH;	/* Default smoothing */
	ink.c.Kskew = ICXINKDEFSKEW;	/* default curve skew */
	ink.c.Kstle = 0.5;		/* Min K at white end */
	ink.c.Kstpo = 0.5;		/* Start of transition is at white */ 	
	ink.c.Kenle = 0.5;		/* Max K at black end */
	ink.c.Kenpo = 0.5;		/* End transition at black */
	ink.c.Kshap = 1.0;		/* Linear transition */

	gres = tres + 2;

	/* Get a Device to PCS conversion object */
	if ((luo = xicco->get_luobj(xicco, ICX_CLIP_NEAREST, icmFwd, icAbsoluteColorimetric, icSigLabData, icmLuOrdNorm, NULL, &ink)) == NULL) {
		error ("%d, %s",rd_icco->errc, rd_icco->err);
	}
	/* Get details of conversion */
	luo->spaces(luo, &ins, NULL, &outs, NULL, NULL, NULL, NULL, NULL);

	if (ins != icSigCmykData) {
		error("Expecting CMYK device");
	}
		
	/* Make a default allocation */
	fxlist_a = 4;
	if ((fxlist = (fxpos *)malloc(sizeof(fxpos) * fxlist_a)) == NULL)
		error ("Failed to malloc fxlist");

	/* init coords */
	for (j = 0; j < 3; j++)
		gc[j] = 0;
			
	for (;;) {	/* For all grid points */

		/* Check if this position is on the CMY surface */
		for (j = 0; j < 3; j++) {
			if (gc[j] == 0 || gc[j] == (tres-1))
				break;
		}
		if (j < 3) {		/* On the surface */

			/* Make sure there is room */
			if (fxno >= fxlist_a) {
				fxlist_a *= 2;
				if ((fxlist = (fxpos *)realloc(fxlist, sizeof(fxpos) * fxlist_a)) == NULL)
						error ("Failed to malloc fxlist");
			}
			for (j = 0; j < 3; j++)
				fxlist[fxno].p[j] = gc[j]/(tres-1.0);
			fxlist[fxno].p[3] = 0.0;
			fxno++;
		}

		/* Increment grid index and position */
		for (j = 0; j < 3; j++) {
			gc[j]++;
			if (gc[j] < tres)
				break;	/* No carry */
			gc[j] = 0;
		}
		if (j >= 3)
			break;		/* Done grid */
	}

	for (i = 1; i < ((EXTRA_NEUTRAL * gres)-1); i++) {	/* For all grey axis points */

		/* Make sure there is room */
		if (fxno >= fxlist_a) {
			fxlist_a *= 2;
			if ((fxlist = (fxpos *)realloc(fxlist, sizeof(fxpos) * fxlist_a)) == NULL)
					error ("Failed to malloc fxlist");
		}
		for (j = 0; j < 3; j++)
			fxlist[fxno].p[j] = i/((EXTRA_NEUTRAL * gres)-1.0);
		fxlist[fxno].p[3] = 0.0;
		fxno++;
	}

	/* Convert CMY values into CMY0 Lab values */
	for (i = 0; i < fxno; i++) {
		if ((rv = luo->lookup(luo, fxlist[i].v, fxlist[i].p)) > 1)
			error ("%d, %s",rd_icco->errc,rd_icco->err);
//printf("~1 initial lookup %f %f %f -> %f %f %f\n", fxlist[i].p[0], fxlist[i].p[1], fxlist[i].p[2], fxlist[i].v[0], fxlist[i].v[1], fxlist[i].v[2]);
	}

	/* Figure out the general scale at the black end */
	{
		double dval[4];
		double wt[3] = { 100.0, 0, 0 };		/* Approximate white PCS */
		double ac[3] = { 50.0, 0, 0 };		/* Adjustment center */

		double lab_wt[3];		/* Media Whitepoint PCS */
		double xyz_wt[3];		/* Media Whitepoint PCS */
		icmXYZNumber XYZ_WT;
		double k_bk[3];			/* K black PCS */
		double cmyk_bk[3];		/* CMYK black PCS */
		double toAbs[3][3];
		double fromAbs[3][3];
	
		/* Discover the Lab of the CMYK media */
		for (j = 0; j < 4; j++)
			dval[j] = 0.0;
		luo->lookup(luo, lab_wt, dval);
		icmLab2XYZ(&icmD50, xyz_wt, lab_wt);
		icmAry2XYZ(XYZ_WT, xyz_wt);

		/* Create the XYZ chromatic transform from relative to absolute and back */
		icmChromAdaptMatrix(ICM_CAM_BRADFORD, XYZ_WT, icmD50, toAbs);
		icmChromAdaptMatrix(ICM_CAM_BRADFORD, icmD50, XYZ_WT,  fromAbs);

		/* Discover the Lab of the CMY black and CMYK black */
		for (j = 0; j < 3; j++)
			dval[j] = 0.0;
		dval[j] = 1.0;
		luo->lookup(luo, k_bk, dval);
		visect(luo, cmyk_bk, NULL, wt, k_bk);
		if (verb)
			printf("K black %f %f %f, CMYK black %f %f %f\n", k_bk[0], k_bk[1], k_bk[2], cmyk_bk[0], cmyk_bk[1], cmyk_bk[2]);

		/* Scale the CMY values that will be influenced by the K component to a darker */
		/* black. */

		for (i = 0; i < fxno; i++) {

			/* Treat grey axis points differently */
			if (fxlist[i].p[0] != 0.0
			 && fxlist[i].p[0] == fxlist[i].p[1]
			 && fxlist[i].p[0] == fxlist[i].p[2]) {
				double xyz[3];			/* Temporary */
				double lab[3];			/* Temporary */

//printf("~1 scaled neutral L value from %f",fxlist[i].v[0]);

				/* Scale L value from K to CMYK black */
				fxlist[i].v[0] = lab_wt[0] + (fxlist[i].v[0] - lab_wt[0]) *
					                         (cmyk_bk[0] - lab_wt[0])/(k_bk[0] - lab_wt[0]);
//printf(" to %f\n",fxlist[i].v[0]);

#ifdef NEVER
				/* Make a & b values same as CMYK black */
				/* Pivot around white point */
				fxlist[i].v[1] = wt[1] + (cmyk_bk[1] - wt[1]) *
					                         (fxlist[i].v[0] - wt[0])/(cmyk_bk[0] - wt[0]);
				fxlist[i].v[2] = wt[2] + (cmyk_bk[2] - wt[2]) *
					                         (fxlist[i].v[0] - wt[0])/(cmyk_bk[0] - wt[0]);
#else
				/* Convert absolute target Lab to relative Lab */
				icmLab2XYZ(&icmD50, xyz, fxlist[i].v);
				icmMulBy3x3(xyz, fromAbs, xyz);
				icmXYZ2Lab(&icmD50, lab, xyz);

				/* Make sure the equivalent relative value is neutral */
				lab[1] = lab[2] = 0.0;

				/* Convert back to absolute Lab */
				icmLab2XYZ(&icmD50, xyz, lab);
				icmMulBy3x3(xyz, toAbs, xyz);
				icmXYZ2Lab(&icmD50, lab, xyz);

//printf("~1 corrected neutral value from Lab %f %f %f to %f %f %f\n", fxlist[i].v[0], fxlist[i].v[1], fxlist[i].v[2], lab[0], lab[1], lab[2]);

				for (j = 0; j < 3; j++)
					fxlist[i].v[j] = lab[j];
#endif
			
			} else {

				for (j = 0; j < 3; j++) {
					if (fxlist[i].p[j] == 0.0)
						break;				/* Not a dark color */
				}
				if (j >= 3) {
					/* Scale by CMYK/K black vector */
					for (j = 0; j < 3; j++) {
						fxlist[i].v[j] = lab_wt[j] + (fxlist[i].v[j] - lab_wt[j]) *
						                         (cmyk_bk[j] - lab_wt[j])/(k_bk[j] - lab_wt[j]);
					}
				}

//printf("[%4.2f %4.2f %4.2f] %f %f %f becomes",
//fxlist[i].p[0], fxlist[i].p[1], fxlist[i].p[2], fxlist[i].v[0], fxlist[i].v[1], fxlist[i].v[2]);
				/* Now clip non-neutrals to the gamut surface */
				visect(luo, fxlist[i].v, NULL, ac, fxlist[i].v);
//printf("%f %f %f\n", fxlist[i].v[0], fxlist[i].v[1], fxlist[i].v[2]);
			}
		}
	}

	if (verb) {
		for (i = 0; i < fxno; i++) {
			printf("Point %d = %f %f %f -> %f %f %f\n",i,
			        fxlist[i].p[0], fxlist[i].p[1], fxlist[i].p[2],
			        fxlist[i].v[0], fxlist[i].v[1], fxlist[i].v[2]);
		}
	}

	/* Setup output cgats file */
	{
		char buf[1000];
		cgats_set_elem *setel;  /* Array of set value elements */

		ocg = new_cgats();				/* Create a CGATS structure */
		ocg->add_other(ocg, "CTI3"); 	/* our special type is Calibration Target Information 3 */
		ocg->add_table(ocg, tt_other, 0);	/* Start the first table */
	
		ocg->add_kword(ocg, 0, "DESCRIPTOR", "Argyll Calibration Target chart information 3",NULL);
		ocg->add_kword(ocg, 0, NULL, NULL, "Fake CMY device data for CMY->CMYK link");
		ocg->add_kword(ocg, 0, "ORIGINATOR", "Argyll fakeCMY", NULL);
		atm[strlen(atm)-1] = '\000';	/* Remove \n from end */
		ocg->add_kword(ocg, 0, "CREATED",atm, NULL);
	
		ocg->add_kword(ocg, 0, "DEVICE_CLASS","OUTPUT", NULL);
	
		ocg->add_kword(ocg, 0, "COLOR_REP","CMY_LAB", NULL);
	
		if (tlimit >= 0) {
			sprintf(buf,"%.0f",tlimit);
			ocg->add_kword(ocg, 0, "TOTAL_INK_LIMIT",buf, NULL);
		}

		if (klimit >= 0) {
			sprintf(buf,"%.0f",klimit);
			ocg->add_kword(ocg, 0, "BLACK_INK_LIMIT",buf, NULL);
		}

		/* Fields we want */
		ocg->add_field(ocg, 0, "SAMPLE_ID", nqcs_t);
		ocg->add_field(ocg, 0, "CMY_C", r_t);
		ocg->add_field(ocg, 0, "CMY_M", r_t);
		ocg->add_field(ocg, 0, "CMY_Y", r_t);
		ocg->add_field(ocg, 0, "LAB_L", r_t);
		ocg->add_field(ocg, 0, "LAB_A", r_t);
		ocg->add_field(ocg, 0, "LAB_B", r_t);

		if ((setel = (cgats_set_elem *)malloc(sizeof(cgats_set_elem) * 7)) == NULL)
			error("Malloc failed!");

		/* Write out test values. */
		for (i = 0; i < fxno; i++) {
			
			sprintf(buf, "%d", i+1);
			setel[0].c = buf;
			
			setel[1].d = 100.0 * fxlist[i].p[0]; 
			setel[2].d = 100.0 * fxlist[i].p[1];
			setel[3].d = 100.0 * fxlist[i].p[2];
			setel[4].d = fxlist[i].v[0];
			setel[5].d = fxlist[i].v[1];
			setel[6].d = fxlist[i].v[2];

			ocg->add_setarr(ocg, 0, setel);
		}

		free(setel);
		if (ocg->write_name(ocg, out_name))
			error("Write error : %s",ocg->err);
		ocg->del(ocg);
	}

	luo->del(luo);
	xicco->del(xicco);
	rd_icco->del(rd_icco);
	rd_fp->del(rd_fp);

	return 0;
}

