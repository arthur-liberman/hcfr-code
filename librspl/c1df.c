
/************************************************/
/* Investigate various curve approximations     */
/************************************************/

/* Discrete regularized spline versions */
/* Standard test  with weak default function */

/* Author: Graeme Gill
 * Date:   20/11/2005
 *
 * Copyright 1995, 1996, 2005 Graeme W. Gill
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#undef DIAG
#undef DIAG2
#undef GLOB_CHECK
#define RES2			/* Do multiple test at various resolutions */
#undef EXTRAFIT			/* Test extra fitting effort */
#define SMOOTH 1.0
#define AVGDEV 0.0

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

#define TRIALS 15	/* Number of random trials */
#define SKIP 0		/* Number of random trials to skip */

#define MIN_PNTS 1
#define MAX_PNTS 7

#define MIN_RES 20
#define MAX_RES 300

double xa[MAX_PNTS];
double ya[MAX_PNTS];
double wa[MAX_PNTS];

#define XRES 100

#define PNTS 2
#define GRES 200
//double t1xa[PNTS] = { 0.325, 0.625  };
//double t1ya[PNTS] = { 0.4,   0.70  };
double t1xa[PNTS] = { 0.325, 0.625  };
double t1ya[PNTS] = { 0.5,   0.8  };
double t1wa[PNTS] = { 1.0,   1.0  };
cow test_points[MAX_PNTS];

double lin(double x, double xa[], double ya[], int n);

/* Weak default function */
static void wfunc(void *cbntx, double *out, double *in) {
	out[0] = in[0];
}

void usage(void) {
	fprintf(stderr,"Test 1D rspl interpolation with weak default function\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: c1df [options]\n");
	fprintf(stderr," -w wweight    Set weak default function weight (default 1.0)\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	int i,j, n;
	double x;
	double xx[XRES];
	double yy[6][XRES];
	rspl *rss;		/* incremental solution version */
	datai low,high;
	int gres[MXDI];
	double avgdev[MXDO];
	double wweight = 1.0;

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

			if (argv[fa][1] == '?') {
				usage();

			} else if (argv[fa][1] == 'w' || argv[fa][1] == 'W') {
				fa = nfa;
				if (na == NULL) usage();
				wweight = atof(na);
			} else 
				usage();
		} else
			break;
	}

	low[0] = 0.0;
	high[0] = 1.0;
	avgdev[0] = AVGDEV;

	error_program = "Curve1";

	for (n = 0; n < TRIALS; n++) {
		double lrand = 0.0;	/* Amount of level randomness */
		int pnts;
		int fres;

		if (n == 0) {	/* Standard versions */
			pnts = PNTS;
			fres = GRES; 
			for (i = 0; i < pnts; i++) {
				xa[i] = t1xa[i];
				ya[i] = t1ya[i];
				wa[i] = t1wa[i];
			}
			printf("Trial %d, points = %d, res = %d, level randomness = %f\n",n,pnts,fres,lrand);
		} else {	/* Random versions */
			double xmx;
			lrand = d_rand(0.0,0.1);		/* Amount of level randomness */
			pnts = i_rand(MIN_PNTS,MAX_PNTS);
			fres = i_rand(MIN_RES,MAX_RES);

			printf("Trial %d, points = %d, res = %d, level randomness = %f\n",n,pnts,fres,lrand);

			/* Create X values */
			xa[0] = d_rand(0.3, 0.5);
			for (i = 1; i < pnts; i++)
				xa[i] = xa[i-1] + d_rand(0.2,0.7);
			xmx = d_rand(0.6, 0.9);
			for (i = 0; i < pnts; i++)	/* Divide out */
				xa[i] *= (xmx/xa[pnts-1]);

			/* Create y values */
			for (i = 0; i < pnts; i++) {
				ya[i] = xa[i] + d_rand(-lrand,lrand);
				wa[i] = 1.0;
			}
		}

		if (n < SKIP)
			continue;

		/* Create the object */
		rss =  new_rspl(RSPL_NOFLAGS, 1,				/* di */
		                  1);				/* fdi */

		for (i = 0; i < pnts; i++) {
			test_points[i].p[0] = xa[i];
			test_points[i].v[0] = ya[i];
			test_points[i].w = wa[i];
		}
		gres[0] = fres;

#ifdef RES2
		if (n != 0) {
#endif
		/* Fit to scattered data */
		rss->fit_rspl_w_df(rss,
#ifdef EXTRAFIT
		           RSPL_EXTRAFIT |	/* Extra fit flag */
#endif
		           0,
		           test_points,			/* Test points */
		           pnts,	/* Number of test points */
		           low, high, gres,		/* Low, high, resolution of grid */
		           low, high,			/* Data scale */
		           SMOOTH,				/* Smoothing */
		           avgdev,				/* Average deviation */
		           NULL,				/* iwidth */
                   wweight,				/* weak function weight */
				   NULL,				/* No context */
		           wfunc				/* Weak function */
		);

		/* Display the result */
		for (i = 0; i < XRES; i++) {
			co tp;	/* Test point */
			x = i/(double)(XRES-1);
			xx[i] = x;
			yy[0][i] = lin(x,xa,ya,pnts);
			tp.p[0] = x;
			rss->interp(rss, &tp);
			yy[1][i] = tp.v[0];
			if (yy[1][i] < -0.2)
				yy[1][i] = -0.2;
			else if (yy[1][i] > 1.2)
				yy[1][i] = 1.2;
		}
		
		do_plot(xx,yy[0],yy[1],NULL,XRES);

#ifdef RES2
		} else {	/* Multiple resolution version */
			int gresses[5];
			for (j = 0; j < 5; j++) {
#ifndef NEVER
				if (j == 0)
					gres[0] = fres/8;
				else if (j == 1)
					gres[0] = fres/4;
				else if (j == 2)
					gres[0] = fres/2;
				else if (j == 3)
					gres[0] = fres;
				else 
					gres[0] = fres * 2;
#else 	/* Check sensitivity to griding of data points */
				if (j == 0)
					gres[0] = 192;
				else if (j == 1)
					gres[0] = 193;
				else if (j == 2)
					gres[0] = 194;
				else if (j == 3)
					gres[0] = 195;
				else 
					gres[0] = 196;
#endif
				gresses[j] = gres[0];
	
				rss->fit_rspl_w_df(rss,
#ifdef EXTRAFIT
		           RSPL_EXTRAFIT |		/* Extra fit flag */
#endif
			           0,
			           test_points,			/* Test points */
			           pnts,	/* Number of test points */
			           low, high, gres,		/* Low, high, resolution of grid */
			           low, high,			/* Data scale */
			           SMOOTH,				/* Smoothing */
			           avgdev,				/* Average deviation */
			           NULL,				/* iwidth */
	                   wweight,				/* weak function weight */
					   NULL,				/* No context */
			           wfunc				/* Weak function */
			);
	
				/* Get the result */
				for (i = 0; i < XRES; i++) {
					co tp;	/* Test point */
					x = i/(double)(XRES-1);
					xx[i] = x;
					yy[0][i] = lin(x,xa,ya,pnts);
					tp.p[0] = x;
					rss->interp(rss, &tp);
					yy[1+j][i] = tp.v[0];
					if (yy[1+j][i] < -0.2)
						yy[1+j][i] = -0.2;
					else if (yy[1+j][i] > 1.2)
						yy[1+j][i] = 1.2;
				}
			}
	
		printf("Black = lin, Red = %d, Green = %d, Blue = %d, Yellow = %d, Purple = %d\n",
		       gresses[0], gresses[1], gresses[2], gresses[3], gresses[4]);
		do_plot6(xx,yy[0],yy[1],yy[2],yy[3],yy[4],yy[5],XRES);
	}
#endif /* RES2 */
	}	/* next trial */
	return 0;
}


double
lin(
double x,
double xa[],
double ya[],
int n)
	{
	int i;
	double y;

	if (x < xa[0])
		return ya[0];
	else if (x > xa[n-1])
		return ya[n-1];

	for (i = 0; i < (n-1); i++)
		if (x >=xa[i] && x <= xa[i+1])
			break;

	x = (x - xa[i])/(xa[i+1] - xa[i]);

	y = ya[i] + (ya[i+1] - ya[i]) * x;
	
	return y;
	}


