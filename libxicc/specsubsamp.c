
/* 
 * Author:  Graeme W. Gill
 * Date:    2007/3/21
 * Version: 1.00
 *
 * Copyright 2007 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/*
 * This is some utility code to subsample the 1 and 5nm spectral
 * data in xspect to wider spacings using ANSI-CGATS the recommended
 * triangular filter.
 */

#include <stdio.h>
#include <math.h>
#include "xspect.h"
#include "numlib.h"
#include "ui.h"

void usage(void) {
	fprintf(stderr,"Downsample spectral data\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: specsubsamp -options\n");
	fprintf(stderr," -i illum        Choose illuminant for print/transparency spectral data:\n");
	fprintf(stderr,"                 A, C, D50, D50M2, D65, F5, F8, F10 or file.sp\n");
	fprintf(stderr," -o observ       Choose CIE Observer for spectral data:\n");
	fprintf(stderr,"                 1931_2, 1964_10, S&B 1955_2, shaw, J&V 1978_2\n");
	fprintf(stderr," -w st,en,sp     Output start, end and spacing nm\n");
	fprintf(stderr," -5 		     Commenf output wavelegths every 5\n");
	exit(1);
}

int
main(
	int argc,
	char *argv[]
) {
	int fa,nfa;				/* argument we're looking at */
	int verb = 0;
	icxIllumeType illum = icxIT_D50;	/* Spectral defaults */
	xspect cust_illum;			/* Custom illumination spectrum */
	icxObserverType observ = icxOT_CIE_1931_2;
	int obs = 0;				/* If nz output observer */			
	double wl_short = 380.0, wl_long = 730.0, wl_width = 10.0;
	int wl_n = 0;
	int evy5 = 0;

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

			/* Spectral Illuminant type */
			if (argv[fa][1] == 'i' || argv[fa][1] == 'I') {
				fa = nfa;
				if (na == NULL) usage();
				if (strcmp(na, "A") == 0) {
					illum = icxIT_A;
				} else if (strcmp(na, "C") == 0) {
					illum = icxIT_C;
				} else if (strcmp(na, "D50") == 0) {
					illum = icxIT_D50;
				} else if (strcmp(na, "D50M2") == 0) {
					illum = icxIT_D50M2;
				} else if (strcmp(na, "D65") == 0) {
					illum = icxIT_D65;
				} else if (strcmp(na, "F5") == 0) {
					illum = icxIT_F5;
				} else if (strcmp(na, "F8") == 0) {
					illum = icxIT_F8;
				} else if (strcmp(na, "F10") == 0) {
					illum = icxIT_F10;
				} else {	/* Assume it's a filename */
					illum = icxIT_custom;
					if (read_xspect(&cust_illum, na) != 0)
						usage();
				}
			}

			/* Spectral Observer type */
			else if (argv[fa][1] == 'o' || argv[fa][1] == 'O') {
				fa = nfa;
				if (na == NULL) usage();
				if (strcmp(na, "1931_2") == 0) {			/* Classic 2 degree */
					obs = 1;
					observ = icxOT_CIE_1931_2;
				} else if (strcmp(na, "1964_10") == 0) {	/* Classic 10 degree */
					obs = 1;
					observ = icxOT_CIE_1964_10;
				} else if (strcmp(na, "1955_2") == 0) {		/* Stiles and Burch 1955 2 degree */
					obs = 1;
					observ = icxOT_Stiles_Burch_2;
				} else if (strcmp(na, "1978_2") == 0) {		/* Judd and Voss 1978 2 degree */
					obs = 1;
					observ = icxOT_Judd_Voss_2;
				} else if (strcmp(na, "shaw") == 0) {		/* Shaw and Fairchilds 1997 2 degree */
					obs = 1;
					observ = icxOT_Shaw_Fairchild_2;
				} else
					usage();
			}

			else if (argv[fa][1] == 'w' || argv[fa][1] == 'W') {
				fa = nfa;
				if (na == NULL) usage();
				if (sscanf(na, " %lf,%lf,%lf ",&wl_short,&wl_long,&wl_width) != 3)

				if (wl_short > wl_long)
					usage();
			}

			else if (argv[fa][1] == '5') {
				evy5 = 1;
			}

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			} else {
				usage();
			}
		}
		else
			break;
	}

	wl_n = (int)((wl_long - wl_short)/wl_width + 1.5);

	if ((fabs(wl_short + (wl_n - 1.0) * wl_width) - wl_long) > 0.001)
		error("Not an integer number of output samples");


	if (obs) {
		int i, j, k;
		xspect *sp[3];

		if (standardObserver(sp, observ) != 0)
			error ("standardObserver returned error");
		printf("/* %f - %f, %f spacing, %d samples */\n",wl_short,wl_long,wl_width,wl_n);
		printf("{\n");
		for (k = 0; k < 3; k++) {

			printf("	{\n");
			for (i = 0; i < wl_n; i++) {
				double cwl, swl, sw, tw, outv;
				int ns;

				cwl = XSPECT_WL(wl_short, wl_long, wl_n, i);
				ns = (int)(wl_width/0.1 + 0.5);
				tw = outv = 0.0;
				for (j = -ns; j <= ns; j++) {
					swl = cwl + (j * wl_width)/ns;
					sw = (ns - abs(j))/(double)ns;
					tw += sw;
					outv += sw * value_xspect(sp[k], swl);
				}
				outv /= tw;
				if (!evy5 || i % 5 == 0)
					printf("		/* %3.1f */	%1.10f%s\n",cwl,outv, i < (wl_n-1) ? "," : "");
				else
					printf("		            %1.10f%s\n",outv, i < (wl_n-1) ? "," : "");
			}
			printf("	}%s\n",k < (3-1) ? "," : "");
		}
		printf("}\n");

	} else {
		int i, j;
		xspect sp;

		if (illum == icxIT_custom)
			sp = cust_illum;
		else {
			if (standardIlluminant(&sp, illum, 0) != 0)
				error ("standardIlluminant returned error");
		}

		printf("/* %f - %f, %f spacing, %d samples */\n",wl_short,wl_long,wl_width,wl_n);
		printf("{\n");
		for (i = 0; i < wl_n; i++) {
			double cwl, swl, sw, tw, outv;
			int ns;

			cwl = XSPECT_WL(wl_short, wl_long, wl_n, i);
//printf("~1 output %f\n",cwl);
			ns = (int)(wl_width/0.1 + 0.5);
			tw = outv = 0.0;
			for (j = -ns; j <= ns; j++) {
				swl = cwl + (j * wl_width)/ns;
				sw = (ns - abs(j))/(double)ns;
				tw += sw;
//printf("~1 sample %f weight %f\n",swl,sw);
				outv += sw * value_xspect(&sp, swl);
			}
			outv /= tw;
			if (!evy5 || i % 5 == 0)
				printf("	/* %3.1f */	%1.10f%s\n",cwl,outv, i < (wl_n-1) ? "," : "");
			else
				printf("	            %1.10f%s\n",outv, i < (wl_n-1) ? "," : "");
		}
		printf("}\n");
	}
	return 0;
}








