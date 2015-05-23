
/*
 * Author: Graeme Gill
 * Date:   30/10/2005
 *
 * Copyright 2005 Graeme W. Gill
 * Parts derived from rspl/c1.c, cv.c etc.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Test monocurve class.
 *
 */

#undef DIAG
#undef TEST_SYM

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#if defined(__IBMC__) && defined(_M_IX86)
#include <float.h>
#endif
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "moncurve.h"
#include "plot.h"
#include "ui.h"

double lin(double x, double xa[], double ya[], int n);
void usage(void);

#define TRIALS 30	/* Number of random trials */
#define SKIP 0		/* Number of random trials to skip */
#undef NORMONLY	/* Defined to use 0.0 - 1.0 limited curve */

#undef ORDER_STEP		/* Step orders from 2 to SHAPE_ORDERS */
//#define SHAPE_ORDS 30		/* Number of order to use */
#define SHAPE_ORDS 12		/* Number of order to use */

#define ABS_MAX_PNTS 100

#define MIN_PNTS 2
#define MAX_PNTS 20

#define MIN_RES 20
#define MAX_RES 500

double xa[ABS_MAX_PNTS];
double ya[ABS_MAX_PNTS];

#define XRES 100

#define TSETS 3
#define PNTS 11
#define GRES 100
int t1p[TSETS] = {
	4,
	11,
	11
};

double t1xa[TSETS][PNTS] = {
	{ 0.0, 0.2,  0.8,  1.0  },
	{ 0.0, 0.1,  0.2,  0.3,   0.4,   0.5,  0.6,  0.7,   0.8,   0.9,  1.0  },
	{ 0.0, 0.25, 0.30, 0.35,  0.40,  0.44, 0.48, 0.51,  0.64,  0.75, 1.0  }
};

double t1ya[TSETS][PNTS] = {
	{ 0.0, 0.5,  0.6,  1.0  },
	{ 0.0, 0.0,  0.5,  0.5,   0.5,   0.5,   0.5,  0.8,  1.0,   1.0,   1.0  },
	{ 0.0, 0.35, 0.4,  0.41,  0.42,  0.46, 0.5,  0.575, 0.48,  0.75,  1.0  }
};


mcvco test_points[ABS_MAX_PNTS];

double lin(double x, double xa[], double ya[], int n);

void
usage(void) {
	puts("usage: monctest");
	exit(1);
}

int main() {
	mcv *p;
	int i, n;
	double x, y;
	double xx[XRES];
	double y1[XRES];
	double y2[XRES];
	int np = SHAPE_ORDS;					/* Number of harmonics */

	error_program = "monctest";

#if defined(__IBMC__)
	_control87(EM_UNDERFLOW, EM_UNDERFLOW);
	_control87(EM_OVERFLOW, EM_OVERFLOW);
#endif

#ifdef NORMONLY
	if ((p = new_mcv_noos()) == NULL)
#else
	if ((p = new_mcv()) == NULL)
#endif
		error("new_mcv failed");

	for (n = 0; n < TRIALS; n++) {
		double lrand;		/* Amount of level randomness */
		int pnts;

#ifdef NEVER
		if (n < TSETS)	/* Standard versions */ {
			pnts = t1p[n];
			for (i = 0; i < pnts; i++) {
				xa[i] = t1xa[n][i];
				ya[i] = t1ya[n][i];
			}
		} else if (n == TSETS) {	/* Exponential function aproximation */
			double ex = 2.4;
			pnts = MAX_PNTS;

			printf("Trial %d, no points = %d, exponential %f\n",n,pnts,ex);

			/* Create X values */
			for (i = 0; i < pnts; i++)
				xa[i] = i/(pnts-1.0);

			for (i = 0; i < pnts; i++)
				ya[i] = pow(xa[i], ex);

#else	/* Put exponenial first */
		if (n == 0) {				/* Exponential function aproximation */
			double ex = 2.4;
			pnts = MAX_PNTS;

			printf("Trial %d, no points = %d, exponential %f\n",n,pnts,ex);

			/* Create X values */
			for (i = 0; i < pnts; i++)
				xa[i] = pow(i/(pnts-1.0), 1.0);

			for (i = 0; i < pnts; i++)
				ya[i] = pow(xa[i], ex);

		} else if (n == 1) {				/* inverse exponential function aproximation */
			double ex = 1.0/2.4;
			pnts = MAX_PNTS;

			printf("Trial %d, no points = %d, exponential %f\n",n,pnts,ex);

			/* Create X values */
			for (i = 0; i < pnts; i++)
				xa[i] = pow(i/(pnts-1.0), 3.0);

			for (i = 0; i < pnts; i++)
				ya[i] = pow(xa[i], ex);

#endif

		} else {	/* Random versions */
			double ymax;
			lrand = d_rand(0.0,0.2);		/* Amount of level randomness */
			lrand *= lrand;
			pnts = i_rand(MIN_PNTS,MAX_PNTS);

			printf("Trial %d, no points = %d, level randomness = %f\n",n,pnts,lrand);

			/* Create X values */
			xa[0] = 0.0;
			for (i = 1; i < pnts; i++)
				xa[i] = xa[i-1] + d_rand(0.5,1.0);
			for (i = 0; i < pnts; i++)	/* Divide out */
				xa[i] = (xa[i]/xa[pnts-1]);

			/* Create y values */
			ya[0] = xa[0] + d_rand(-0.2, 0.7);
			for (i = 1; i < pnts; i++)
				ya[i] = ya[i-1] + d_rand(0.1,1.0) + d_rand(-0.1,0.4) + d_rand(-0.4,0.5);

			ymax = d_rand(0.6, 10.2);		/* Scale target */
			for (i = 0; i < pnts; i++) {
				ya[i] = ymax * (ya[i]/ya[pnts-1]);
//				if (ya[i] < 0.0)
//					ya[i] = 0.0;
//				else if (ya[i] > 1.0)
//					ya[i] = 1.0;
			}
		}

		if (n < SKIP)
			continue;

		for (i = 0; i < pnts; i++) {
			test_points[i].p = xa[i];
			test_points[i].v = ya[i];
			test_points[i].w = 1.0;
		}
		/* Test weighting */
		test_points[pnts-1].w = 1.0;

#ifdef ORDER_STEP
		for (np = 2; np <= SHAPE_ORDS; np++) {
#else	/* Full number of orders */
		for (np = SHAPE_ORDS; np <= SHAPE_ORDS; np++) {
#endif

			/* Fit to scattered data */
			p->fit(p,
			    1,						/* Vebose */
			    np,						/* Number of parameters */
			    test_points,			/* Test points */
			    pnts,					/* Number of test points */
			    1.0						/* Smoothing */
			);
	
			printf("Residual = %f\n",p->resid);
			printf("Number params = %d\n",np);
			for (i = 0; i < p->luord; i++) {
				printf("Param %d = %f\n",i,p->pms[i]);
			}
	
			/* Display the result */
			for (i = 0; i < XRES; i++) {
				x = i/(double)(XRES-1);
				xx[i] = x;
				y1[i] = lin(x,xa,ya,pnts);
				y2[i] = p->interp(p, x);
				y = p->inv_interp(p, y2[i]);
				if (fabs(x - y) > 0.00001)
					printf("Inverse mismatch: %f -> %f -> %f\n",x,y2[i],y);
//				if (y2[i] < -0.2)
//					y2[i] = -0.2;
//				else if (y2[i] > 1.2)
//					y2[i] = 1.2;
			}
			do_plot(xx,y1,y2,NULL,XRES);
		}

	}	/* next trial */

	p->del(p);

	return 0;
}

double lin(
double x,
double xa[],
double ya[],
int n) {
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














