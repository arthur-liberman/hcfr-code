
/************************************************/
/* Investigate various curve approximations     */
/************************************************/

/* Discrete regularized spline versions */
/* Test variable grid spacing in 1D */

/* Author: Graeme Gill
 * Date:   4/10/95
 * Date:   5/4/96
 *
 * Copyright 1995, 2013 Graeme W. Gill
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#undef DIAG
#undef DIAG2
#undef GLOB_CHECK
#define AVGDEV 0.0		/* Average deviation of function data */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "rspl.h"
#include "plot.h"
#include "ui.h"

double lin();
void usage(void);

#define TRIALS 1	/* Number of random trials */
#define XRES 100		/* Plotting res */


#define PNTS1 10
#define GRES1 100

/* Function that we're approximating */
static double func(double val) {
	double out;
	int sgn = 0;

	if (val < 0.0) {
		sgn = 1;
		val = -val;
	}

	out = pow(val, 2.5);

	if (sgn)
		out = -out;

	return out;
}

/* Grid spacing power */
#define GRIDPOW 1.5

/* Scattered data sample spacing power */
#define SAMPPOW 1.3


co test_points[PNTS1];

double ipos[GRES1];
double *iposes[1] = { ipos };

double powlike(double vv, double pp);

void usage(void) {
	fprintf(stderr,"Test 1D rspl interpolation variable grid spacing\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: c1 [options]\n");
	fprintf(stderr," -s smooth     Use given smoothness (default 1.0)\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	int i,j, n;
	double x;
	double xx1[XRES];
	double yy1[6][XRES];
	double xx2[XRES];
	double yy2[6][XRES];
	rspl *rss;		/* incremental solution version */
	datai low,high;
	int gres[MXDI];
	double smooth = 1.0;
	double avgdev[MXDO];

	low[0] = 0.0;
	high[0] = 1.0;
	avgdev[0] = AVGDEV;

	error_program = "c1";
	check_if_not_interactive();

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

			/* smoothness */
			else if (argv[fa][1] == 's' || argv[fa][1] == 'S') {
				fa = nfa;
				if (na == NULL) usage();
				smooth = atof(na);
			}
			else 
				usage();
		} else
			break;
	}

	/* Only one trial */
	for (n = 0; n < TRIALS; n++) {
		double lrand = 0.0;		/* Amount of level randomness */
		int pnts;
		int fres;

		pnts = PNTS1;
		fres = GRES1; 
		gres[0] = fres;

		/* Create the object */
		rss =  new_rspl(RSPL_NOFLAGS,
		                1,				/* di */
		                1);				/* fdi */

		/* For each grid spacing test */
		for (j = 0; j < 2; j++) {

			for (i = 0; i < pnts; i++) {
				double val, out;

				val = i/(pnts - 1.0);

				if (j == 0) {			/* Linear */
//					out = func(val + d_rand(-0.001, 0.001));
					out = func(val);
					test_points[i].p[0] = val;	
					test_points[i].v[0] = out;

				} else {					/* power 2.0 grid spacing */
					val = pow(val, SAMPPOW);	/* sample point spacing */ 

//					out = func(val + d_rand(-0.001, 0.001));
					out = func(val);
					val = powlike(val, 1.0/GRIDPOW);	/* To grid spacing */ 
					test_points[i].p[0] = val;
					test_points[i].v[0] = out;
				}
			}

			/* Set grid position info */
			for (i = 0; i < gres[0]; i++) {
				double val, out;

				val = i/(gres[0] - 1.0);
				if (j == 1)
					val = powlike(val, 1.0/GRIDPOW);	/* To grid spacing */ 
				ipos[i] = val; 
			}
			
			rss->fit_rspl(rss,
		           0,
		           test_points,			/* Test points */
		           pnts,	/* Number of test points */
		           low, high, gres,		/* Low, high, resolution of grid */
		           NULL, NULL,			/* Default data scale */
		           smooth,				/* Smoothing */
		           avgdev,				/* Average deviation */
		           j == 0 ? NULL : iposes);				/* iwidth */

			/* Show the result full scale */
			for (i = 0; i < XRES; i++) {
				co tp;	/* Test point */
				x = i/(double)(XRES-1);
				xx1[i] = x;
				yy1[0][i] = func(x);
				tp.p[0] = x;
				if (j == 1)
					tp.p[0] = powlike(tp.p[0], 1.0/GRIDPOW);	/* Grid spacing input */ 
				rss->interp(rss, &tp);
				yy1[1+j][i] = tp.v[0];
				if (yy1[1+j][i] < -0.2)
					yy1[1+j][i] = -0.2;
				else if (yy1[1+j][i] > 1.2)
					yy1[1+j][i] = 1.2;
			}

			/* Show the result magnified */
			for (i = 0; i < XRES; i++) {
				co tp;	/* Test point */
				x = i/(double)(XRES-1);
				x *= 0.1;
				xx2[i] = x;
				yy2[0][i] = func(x);
				tp.p[0] = x;
				if (j == 1)
					tp.p[0] = powlike(tp.p[0], 1.0/GRIDPOW);	/* Grid spacing input */ 
				rss->interp(rss, &tp);
				yy2[1+j][i] = tp.v[0];
				if (yy2[1+j][i] < -0.2)
					yy2[1+j][i] = -0.2;
				else if (yy2[1+j][i] > 1.2)
					yy2[1+j][i] = 1.2;
			}
		}
	
		printf("Full scale: Black = ref, Red = even, Green = pow2\n");
		do_plot6(xx1,yy1[0],yy1[1],yy1[2],NULL,NULL,NULL,XRES);

		printf("Magnified: Black = ref, Red = even, Green = pow2\n");
		do_plot6(xx2,yy2[0],yy2[1],yy2[2],NULL,NULL,NULL,XRES);
	}
	return 0;
}


/* A power-like function, based on Graphics Gems adjustment curve. */
/* Avoids "toe" problem of pure power. */
/* Adjusted so that "power" 2 and 0.5 agree with real power at 0.5 */

double powlike(double vv, double pp) {
	double tt, g;

	if (pp >= 1.0) {
		g = 2.0 * (pp - 1.0);
		vv = vv/(g - g * vv + 1.0);
	} else {
		g = 2.0 - 2.0/pp;
		vv = (vv - g * vv)/(1.0 - g * vv);
	}

	return vv;
}


/******************************************************************/
/* Error/debug output routines */
/******************************************************************/

/* Next u function done with optimization */

/* Structure to hold data for optimization function */
struct _edatas {
	rspl *rss;
	int j;
	}; typedef struct _edatas edatas;

#ifdef GLOB_CHECK
/* Overall Global optimization method */
/* Definition of the optimization function handed to powell() */
double efunc2(void *edata, double p[])
	{
	int j;
	double rv;
	rspl *rss = (rspl *)edata;
	for (j = 0; j < rss->nig; j++)	/* Ugg */
		rss->u[j].v = p[j];
	rv = rss->efactor(rss);
#ifdef DIAG2
	/* printf("%c%e",cr_char,rv); */
	printf("%e\n",rv);
#endif
	return rv;
	}

solveu(rss)
rspl *rss;
	{
	int j;
	double *cp;
	double *s;

	cp = dvector(0,rss->nig);
	s = dvector(0,rss->nig);
	for (j = 0; j < rss->nig; j++)	/* Ugg */
		{
		cp[j] = rss->u[j].v;
		s[j] = 0.1;
		}
	powell(rss->nig,cp,s,1e-7,1000,efunc2,(void *)rss);
	}
#endif /* GLOB_CHECK */
