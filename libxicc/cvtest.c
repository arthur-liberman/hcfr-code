
/**********************************************************/
/* Investigate GEMS transfer curve function approximation */
/**********************************************************/

/* Standard test + Random testing */

/* Author: Graeme Gill
 * Date:   2003/12/1
 *
 * Copyright 2003 Graeme W. Gill
 * Parts derived from rspl/c1.c
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#undef DIAG
#undef TEST_SYM		/* Test forcing center to zero (a*, b* constraint) */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#if defined(__IBMC__) && defined(_M_IX86)
#include <float.h>
#endif
#include "numlib.h"
#include "plot.h"
#include "ui.h"

#define MAX_PARM 40		/* Make > SHAPE_ORDS */

#define POWTOL 1e-5
#define MAXITS 10000

/* SHAPE_BASE doesn't seem extremely critical.  It is centered in +/- 1 magnitude */
/* 10 x more filters out noise reasonably heaviliy, 10 x less gives noticable */
/* overshoot Range 0.00001 .. 0.001 */
/* SHAPE_HBASE is more critical. */
/* Range 0.00005 .. 0.001 */
//#define SHAPE_BASE  0.00001		/* 0 & 1 harmonic weight */
//#define SHAPE_HBASE 0.0002		/* 2nd and higher additional weight */
//#define SHAPE_ORDS 20
#define SHAPE_BASE  0.00001		/* 0 & 1 harmonic weight */
#define SHAPE_HBASE 0.0001		/* 2nd and higher additional weight */
#define SHAPE_ORDS 20

/* Interface coordinate value */
typedef struct {
	double p;		/* coordinate position */
	double v;		/* function values */
} co;

double lin(double x, double xa[], double ya[], int n);
static double tfunc(double *v, int    luord, double vv);
void fit(double *params, int np, co *test_points, int pnts);
void usage(void);

#define TRIALS 40		/* Number of random trials */
#define SKIP 0		/* Number of random trials to skip */

#define ABS_MAX_PNTS 100

#define MIN_PNTS 2
#define MAX_PNTS 20

#define MIN_RES 20
#define MAX_RES 500

double xa[ABS_MAX_PNTS];
double ya[ABS_MAX_PNTS];

#define XRES 100

#define TSETS 4
#define PNTS 11
#define GRES 100
int t1p[TSETS] = {
	4,
	11,
	11,
	11
};

double t1xa[TSETS][PNTS] = {
	{ 0.0, 0.2,  0.8,  1.0  },
	{ 0.0, 0.1,  0.2,  0.3,   0.4,   0.5,  0.6,  0.7,   0.8,   0.9,  1.0  },
	{ 0.0, 0.1,  0.2,  0.3,   0.4,   0.5,  0.6,  0.7,   0.8,   0.9,  1.0  },
	{ 0.0, 0.25, 0.30, 0.35,  0.40,  0.44, 0.48, 0.51,  0.64,  0.75, 1.0  }
};

double t1ya[TSETS][PNTS] = {
	{ 0.0, 0.5,  0.6,  1.0  },
	{ 0.0, 0.10, 0.22, 0.35,  0.52,  0.65,  0.78, 0.91, 1.0,   0.9,   0.85  },
	{ 0.0, 0.0,  0.5,  0.5,   0.5,   0.5,   0.5,  0.8,  1.0,   1.0,   1.0  },
	{ 0.0, 0.35, 0.4,  0.41,  0.42,  0.46, 0.5,  0.575, 0.48,  0.75,  1.0  }
};


co test_points[ABS_MAX_PNTS];

double lin(double x, double xa[], double ya[], int n);

int main(void) {
	int i, n;
	double x;
	double xx[XRES];
	double y1[XRES];
	double y2[XRES];
	int np = SHAPE_ORDS;					/* Number of harmonics */
	double params[MAX_PARM];	/* Harmonic parameters */

	error_program = "Curve1";

#if defined(__IBMC__)
	_control87(EM_UNDERFLOW, EM_UNDERFLOW);
	_control87(EM_OVERFLOW, EM_OVERFLOW);
#endif

	for (n = 0; n < TRIALS; n++) {
		double lrand;		/* Amount of level randomness */
		int pnts;

		if (n < TSETS)	/* Standard versions */ {
			pnts = t1p[n];
			for (i = 0; i < pnts; i++) {
				xa[i] = t1xa[n][i];
				ya[i] = t1ya[n][i];
			}
		} else if (n == TSETS) {	/* Exponential function aproximation */
			double ex = 2.2;
			pnts = MAX_PNTS;

			printf("Trial %d, no points = %d, exponential %f\n",n,pnts,ex);

			/* Create X values */
			for (i = 0; i < pnts; i++)
				xa[i] = i/(pnts-1.0);

			for (i = 0; i < pnts; i++)
				ya[i] = pow(xa[i], ex);

		} else if (n == (TSETS+1)) {	/* Exponential function aproximation */
			double ex = 1.0/2.2;
			pnts = MAX_PNTS;

			printf("Trial %d, no points = %d, exponential %f\n",n,pnts,ex);

			/* Create X values */
			for (i = 0; i < pnts; i++)
				xa[i] = i/(pnts-1.0);

			for (i = 0; i < pnts; i++)
				ya[i] = pow(xa[i], ex);

		} else {	/* Random versions */
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
			ya[0] = xa[0];
			for (i = 1; i < pnts; i++)
				ya[i] = ya[i-1] + d_rand(0.1,1.0) + d_rand(-0.1,0.4) + d_rand(-0.4,0.5);
			for (i = 0; i < pnts; i++) {	/* Divide out */
				ya[i] = (ya[i]/ya[pnts-1]);
				if (ya[i] < 0.0)
					ya[i] = 0.0;
				else if (ya[i] > 1.0)
					ya[i] = 1.0;
			}
		}

		if (n < SKIP)
			continue;

		for (i = 0; i < pnts; i++) {
			test_points[i].p = xa[i];
			test_points[i].v = ya[i];
		}

		for (np = 2; np <= SHAPE_ORDS; np++) {

			/* Fit to scattered data */
			fit(params,					/* Parameters to return */
			    np,						/* Number of parameters */
			    test_points,			/* Test points */
			    pnts					/* Number of test points */
			);
	
			printf("Number params = %d\n",np);
			for (i = 0; i < np; i++) {
				printf("Param %d = %f\n",i,params[i]);
			}
	
			/* Display the result */
			for (i = 0; i < XRES; i++) {
				x = i/(double)(XRES-1);
				xx[i] = x;
				y1[i] = lin(x,xa,ya,pnts);
				y2[i] = tfunc(params, np, x);
				if (y2[i] < -0.2)
					y2[i] = -0.2;
				else if (y2[i] > 1.2)
					y2[i] = 1.2;
			}
	
			do_plot(xx,y1,y2,NULL,XRES);
		}

	}	/* next trial */
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


/******************************************************************/
/* Error/debug output routines */
/******************************************************************/

void
usage(void) {
	puts("usage: curve");
	exit(1);
	}

/*******************************************************************/
/* Grapic gems based, monotonic 1D function transfer curve. */

/* Per transfer function */
static double tfunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Curve order n..MPP_MXTCORD */
double vv			/* Source of value */
) {
	double g;
	int ord;

	if (vv < 0.0)
		vv = 0.0;
	else if (vv > 1.0)
		vv = 1.0;

	/* Process all the shaper orders from high to low. */
	/* [These shapers were inspired by a Graphics Gem idea */
	/* (Gems IV, VI.3, "Fast Alternatives to Perlin's Bias and */
	/*  Gain Functions, pp 401). */
	/*  They have the nice properties that they are smooth, and */
	/*  can't be non-monotonic. The control parameter has been */
	/*  altered to have a range from -oo to +oo rather than 0.0 to 1.0 */
	/*  so that the search space is less non-linear. ] */
	for (ord = 0; ord < luord; ord++) {
		int nsec;			/* Number of sections */
		double sec;			/* Section */

		g = v[ord];			/* Parameter */

		nsec = ord + 1;		/* Increase sections for each order */

		vv *= (double)nsec;

		sec = floor(vv);
		if (((int)sec) & 1)
			g = -g;				/* Alternate action in each section */
		vv -= sec;
		if (g >= 0.0) {
			vv = vv/(g - g * vv + 1.0);
		} else {
			vv = (vv - g * vv)/(1.0 - g * vv);
		}
		vv += sec;
		vv /= (double)nsec;
	}

	return vv;
}

/* return the sum of the squares of the current shaper parameters */
static double shapmag(
double *v,			/* Pointer to first parameter */
int luord
) {
	double tt, w, tparam = 0.0;
	int f;

	for (f = 0; f < luord; f++) {
		tt = v[f];
		tt *= tt;

		/* Weigh to suppress ripples */
		if (f <= 1)
			w = SHAPE_BASE;
		else {
			w = SHAPE_BASE + (f-1) * SHAPE_HBASE;
		}

		tparam += w * tt;
	}
	return tparam;
}

/* Context for optimising function fit */
typedef struct {
	int luord;				/* shaper order */
	co *points;				/* List of test points   */
	int nodp;				/* Number of data points */
} luopt;

/* Shaper+Matrix optimisation function handed to powell() */
static double luoptfunc(void *edata, double *v) {
	luopt *p = (luopt *)edata;
	double rv = 0.0, smv;
	double out;
	int i;

	/* For all our data points */
	for (i = 0; i < p->nodp; i++) {
		double ev;

		/* Apply our function */
		out = tfunc(v, p->luord, p->points[i].p);

		ev = out - p->points[i].v;

#ifdef NEVER
		ev = fabs(ev);
		rv += ev * ev * ev;
#else
		rv += ev * ev;
#endif

	}

	/* Normalise error to be an average delta E squared */
	rv /= (double)p->nodp;

	/* Sum with shaper parameters squared, to */
	/* minimise unsconstrained "wiggles" */
	smv = shapmag(v, p->luord);
	rv += smv;

#ifdef TEST_SYM
	{
		double tt;
		tt = tfunc(v, p->luord, 0.5) - 0.5;
		tt *= tt;
		rv += 200.0 * tt;
	}
#endif

	printf("~1 rv = %f (%f)\n",rv,smv);
	return rv;
}

/* Fitting function */
void fit(
double *params,		/* Parameters to return */
int np,				/* Number of parameters */
co *test_points,	/* Test points */
int pnts			/* Number of test points */
) {
	int i;
	double sa[MAX_PARM];	/* Search area */
	luopt os;

	os.luord = np;
	os.points= test_points;
	os.nodp = pnts;

	for (i = 0; i < np; i++) {
		sa[i] = 0.5;
		params[i] = 0.0;
	}

	if (powell(NULL, np, params, sa, POWTOL, MAXITS, luoptfunc, (void *)&os, NULL, NULL) != 0)
		error ("Powell failed");
}

