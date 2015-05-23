
/* 
 * Very simple, dumb plot of .ti3 data.
 *
 * Author:  Graeme W. Gill
 * Date:    2005/10/12
 * Version: 1.0
 *
 * Copyright 2000 - 2005 Graeme W. Gill
 * Please refer to License.txt file for details.
 */

/* TTBD:
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "numlib.h"
#include "icc.h"
#include "cgats.h"
#include "xcolorants.h"
#include "sort.h"
#include "plot.h"
#include "ui.h"

void usage(void) {
	fprintf(stderr,"Simple 2D plot of CGATS .ti3 data\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: cgatssplot [-v] infile\n");
	fprintf(stderr," -v        verbose\n");
	fprintf(stderr," -0 .. -9  Choose channel to plot against\n");
	exit(1);
}

/* Patch value type */
typedef struct {
	double d[ICX_MXINKS];	/* Device values */
	double v[3];			/* CIE value */
} pval;


int
main(
	int argc,
	char *argv[]
) {
	int fa,nfa;				/* argument we're looking at */
	int verb = 0;
	int chan = 0;			/* Chosen channel to plot against */
	char in_name[100];

	char *buf, *outc;
	int ti;
	cgats *cgf = NULL;			/* cgats file data */
	int isLab = 0;				/* cgats output is Lab, else XYZ */
	char *xyzfname[3] = { "XYZ_X", "XYZ_Y", "XYZ_Z" };
	char *labfname[3] = { "LAB_L", "LAB_A", "LAB_B" };
	int npat;					/* Number of patches */ 
	inkmask nmask;				/* Device inkmask */
	int nchan;					/* Number of input chanels */
	char *bident;				/* Base ident */
	int chix[ICX_MXINKS];	/* Device chanel indexes */
	int pcsix[3];	/* Device chanel indexes */
	pval *pat;					/* patch values */
	int i, j;
	
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

			/* Verbosity */
			if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}

			else if (argv[fa][1] >= '0' && argv[fa][1] <= '9') {
				chan = argv[fa][1] - '0';
			}
			else if (argv[fa][1] == '?')
				usage();
			else 
				usage();
		}
		else
			break;
	}

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(in_name,argv[fa]);

	/* Open CIE target values */
	cgf = new_cgats();			/* Create a CGATS structure */
	cgf->add_other(cgf, "CTI3");/* our special input type is Calibration Target Information 3 */
	if (cgf->read_name(cgf, in_name))
		error("CGATS file read error %s on file '%s'",cgf->err, in_name);

	if (cgf->ntables == 0 || cgf->t[0].tt != tt_other || cgf->t[0].oi != 0)
		error ("Profile file '%s' isn't a CTI3 format file",in_name);

	if (cgf->ntables < 1)
		error ("Input file '%s' doesn't contain at least one table",in_name);

	if ((ti = cgf->find_kword(cgf, 0, "COLOR_REP")) < 0)
		error("Input file doesn't contain keyword COLOR_REPS");
	
	if ((buf = strdup(cgf->t[0].kdata[ti])) == NULL)
		error("Malloc failed");
	
	/* Split COLOR_REP into device and PCS space */
	if ((outc = strchr(buf, '_')) == NULL)
		error("COLOR_REP '%s' invalid", cgf->t[0].kdata[ti]);
	*outc++ = '\000';
	
	if (strcmp(outc, "XYZ") == 0) {
		isLab = 0;
	} else if (strcmp(outc, "LAB") == 0) {
		isLab = 1;
	} else
		error("COLOR_REP '%s' invalid (Neither XYZ nor LAB)", cgf->t[0].kdata[ti]);

	if ((nmask = icx_char2inkmask(buf)) == 0) {
		error ("File '%s' keyword COLOR_REPS has unknown device value '%s'",in_name,buf);
	}

	free(buf);

	nchan = icx_noofinks(nmask);
	bident = icx_inkmask2char(nmask, 0);		/* Base ident (No possible 'i') */ 

	/* Find device fields */
	for (j = 0; j < nchan; j++) {
		int ii, imask;
		char fname[100];

		imask = icx_index2ink(nmask, j);
		sprintf(fname,"%s_%s",nmask == ICX_W || nmask == ICX_K ? "GRAY" : bident,
		                      icx_ink2char(imask));

		if ((ii = cgf->find_field(cgf, 0, fname)) < 0)
			error ("Input file doesn't contain field %s",fname);
		if (cgf->t[0].ftype[ii] != r_t)
			error ("Field %s is wrong type",fname);
		chix[j] = ii;
	}

	/* Find PCS fields */
	for (j = 0; j < 3; j++) {
		int ii;

		if ((ii = cgf->find_field(cgf, 0, isLab ? labfname[j] : xyzfname[j])) < 0)
			error ("Input file doesn't contain field %s",isLab ? labfname[j] : xyzfname[j]);
		if (cgf->t[0].ftype[ii] != r_t)
			error ("Field %s is wrong type",isLab ? labfname[j] : xyzfname[j]);
		pcsix[j] = ii;
	}

	npat = cgf->t[0].nsets;		/* Number of patches */

	if (npat <= 0)
		error("No sets of data in file '%s'",in_name);

	/* Allocate arrays to hold test patch input and output values */
	if ((pat = (pval *)malloc(sizeof(pval) * npat)) == NULL)
		error("Malloc failed - pat[]");

	/* Grab all the values */
	for (i = 0; i < npat; i++) {
		pat[i].v[0] = *((double *)cgf->t[0].fdata[i][pcsix[0]]);
		pat[i].v[1] = *((double *)cgf->t[0].fdata[i][pcsix[1]]);
		pat[i].v[2] = *((double *)cgf->t[0].fdata[i][pcsix[2]]);
		if (!isLab) {
			pat[i].v[0] /= 100.0;		/* Normalise XYZ to range 0.0 - 1.0 */
			pat[i].v[1] /= 100.0;
			pat[i].v[2] /= 100.0;
		}
		if (!isLab) { /* Convert test patch result XYZ to PCS (D50 Lab) */
			icmXYZ2Lab(&icmD50, pat[i].v, pat[i].v);
		}
		for (j = 0; j < nchan; j++) {
			pat[i].d[j] = *((double *)cgf->t[0].fdata[i][chix[j]]);
		}
	}

	/* Sort by the selected channel */
#define HEAP_COMPARE(A,B) (A.d[chan] < B.d[chan])
	HEAPSORT(pval, pat, npat);
#undef HEAP_COMPARE

	/* Create the plot */
	{
		int i;
		double *xx;
		double *y0;
		double *y1;
		double *y2;

		if ((xx = (double *)malloc(sizeof(double) * npat)) == NULL)
			error("Malloc failed - xx[]");
		if ((y0 = (double *)malloc(sizeof(double) * npat)) == NULL)
			error("Malloc failed - y0[]");
		if ((y1 = (double *)malloc(sizeof(double) * npat)) == NULL)
			error("Malloc failed - y1[]");
		if ((y2 = (double *)malloc(sizeof(double) * npat)) == NULL)
			error("Malloc failed - y2[]");
		
		for (i = 0; i < npat; i++) {
			xx[i] = pat[i].d[chan];
			y0[i] = pat[i].v[0];
			y1[i] = 50 + pat[i].v[1]/2.0;
			y2[i] = 50 + pat[i].v[2]/2.0;

//			printf("~1 %d: xx = %f, y = %f %f %f\n",i,xx[i],y0[i],y1[i],y2[i]);
		}
		do_plot6(xx,y0,y1,NULL,NULL,y2,NULL,npat);

		free(y2);
		free(y1);
		free(y0);
		free(xx);
	}

	free(pat);
	cgf->del(cgf);

	return 0;
}
