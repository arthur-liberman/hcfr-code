
/* 
 * Argyll Color Correction System
 *
 * Scattered Data Interpolation with multilevel B-splines library.
 * This can be used by rspl, or independently by any other routine.
 *
 * Author: Graeme W. Gill
 * Date:   2001/1/1
 *
 * Copyright 2000 - 2001 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/*
 * This is from the paper
 * "Scattered Data Interpolation with Multilevel B-Splines"
 * by Seungyong Lee, George Wolberg and Sung Yong Shin,
 * IEEE Transactions on Visualisation and Computer Graphics
 * Vol. 3, No. 3, July-September 1997, pp 228.
 */

#include "rspl.h"		/* Define some common elements */

/* Neighborhood latice cache data */
typedef struct {
	int c[MXDI];		/* Coordinate */
	int xo;				/* Offset into slbs latice */
	double w;			/* B-spline basis weight */
} neigh;

/* Structure that represents a resolution level of B-splines */
struct _slbs {
	struct _mlbs *p;		/* Parent structure */
	int res;				/* Basic resolution */
	int coi[MXDI];			/* Double increment for each input dimension into latice */
	double *lat;			/* Control latice, extending from +/- 1 from 0..res-1 */
	double *_lat;			/* Allocation base of lat */
	int lsize, loff;		/* Number of doubles in _lat, offset of lat from _lat */
	double w[MXDI];			/* Input data cell width */
	neigh *n;				/* Neighborhood latice cache */
	int nsize;				/* Number of n entries */
}; typedef struct _slbs slbs;

/* Structure that represents the whole scattered interpolation state */
struct _mlbs {
	int di;		/* Input dimensions */
	int fdi;	/* Output dimensions */
	int tres;	/* Target resolution */
	double smf;	/* Smoothing factor */
	int npts;	/* Number of data points */
	dpnts *pts;	/* Coordinate points and weights (valid while creating) */
	double l[MXDI], h[MXDI];	/* Input data range, cell width */
	slbs *s;	/* Current B-spline latice */

	int (*lookup)(struct _mlbs *p, co *c);

	void (*del)(struct _mlbs *p);
	
}; typedef struct _mlbs mlbs;


/* Create a new empty mlbs */
mlbs *new_mlbs(
int di,		/* Input dimensionality */
int fdi,	/* Output dimesionality */
int res,	/* Minimum final resolution */
dpnts  *pts,	/* scattered data points and weights */
int  npts,	/* number of scattered data points */
double *l,	/* Input data range, low  (May be NULL) */
double *h,	/* Input data range, high (May be NULL) */
double smf	/* Smoothing factor */
); 

