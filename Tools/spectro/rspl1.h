
#ifndef _RSPL1_H_

/* Single dimension regularized spline data structure */

/* 
 * Argyll Color Management System
 * Author: Graeme W. Gill
 * Date:   2000/10/29
 *
 * Copyright 1996 - 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * This is a simple 1D version of rspl, useful for standalone purposes.
 *
 */

/*
 * Might be nice to add support for simple rev lookup, so that
 * standalone xcal can use it ??
 */


#ifdef __cplusplus
extern "C" {
#endif

/* Make up for possible lack of rspl.h */
#ifndef MXDI
# define MXDI 10			/* Maximum input dimensionality */
# define MXDO 10			/* Maximum output dimensionality (Is not fully tested!!!) */
#endif

/* General data point position/value structure */
typedef double datai[1];
typedef double datao[1];
typedef double ratai[1];
typedef double ratao[1];

/* Interface coordinate value */
typedef struct {
    double p[1];	/* coordinate position */
    double v[1];	/* function values */
} co;

/* Interface coordinate value */
typedef struct {
    double p[1];	/* coordinate position */
    double v[1];	/* function values */
	double w;		/* Weight to give this point, nominally 1.0 */
} cow;

#define RSPL_NOFLAGS 0

struct _rspl {
	
  /* Private: */
	int nig;		/* number in interpolation grid */
	double gl,gh,gw;/* Interpolation grid scale low, high, grid cell width */
	double vl,vw;	/* low & range */ 
	double xl,xh;	/* Actual X data exremes low, high */
	double dl,dh;	/* Actual Y Data scale low, high */
	double *x;		/* Array of nig grid point scaled y values */

  /* Public: */

	/* destructor */
	void (*del)(struct _rspl *t);

	/* Initialise from scattered data. */
	/* Returns nz on error */
	int
	(*fit_rspl)(
		struct _rspl *s,	/* this */
		int flags,		/* (Not used) */
		co *d,			/* Array holding position and function values of data points */
		int ndp,		/* Number of data points */
		datai glow,		/* Grid low scale - will expand to enclose data, NULL = default 0.0 */
		datai ghigh,	/* Grid high scale - will expand to enclose data, NULL = default 1.0 */
		int    *gres,	/* Spline grid resolution, ncells = gres-1 */
		datao vlow,		/* Data value low normalize, NULL = default 0.0 */
		datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
		double smooth,	/* Smoothing factor, 0.0 = default 1.0 */
		double *avgdev, /* (Not used) */
		double **ipos 	/* (not used) */
	); 

	/* Initialise from scattered data with weighting. */
	/* Returns nz on error */
	int
	(*fit_rspl_w)(
		struct _rspl *s,	/* this */
		int flags,		/* (Not used) */
		cow *d,			/* Array holding position and function values of data points */
		int ndp,		/* Number of data points */
		datai glow,		/* Grid low scale - will expand to enclose data, NULL = default 0.0 */
		datai ghigh,	/* Grid high scale - will expand to enclose data, NULL = default 1.0 */
		int    *gres,	/* Spline grid resolution, ncells = gres-1 */
		datao vlow,		/* Data value low normalize, NULL = default 0.0 */
		datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
		double smooth,	/* Smoothing factor, 0.0 = default 1.0 */
		double *avgdev, /* (Not used) */
		double **ipos 	/* (not used) */
	); 

	/* Initialize the grid from a provided function. */
	/* Grid index values are supplied "under" in[] at *((int*)&in[-e-1]) */
	int
	(*set_rspl)(
		struct _rspl *s,	/* this */
		int flags,		/* (Not used) */
		void *cbntx,	/* Opaque function context */
		void (*func)(void *cbntx, double *out, double *in),		/* Function to set from */
		datai glow,		/* Grid low scale, NULL = default 0.0 */
		datai ghigh,	/* Grid high scale, NULL = default 1.0 */
		int gres[MXDI],	/* Spline grid resolution */
		datao vlow,		/* Data value low normalize, NULL = default 0.0 */
		datao vhigh		/* Data value high normalize - NULL = default 1.0 */
	);

	/* Do forward interpolation */
	/* Return 0 if OK, 1 if input was clipped to grid */
	int (*interp)(
		struct _rspl *s,	/* this */
		co *p);				/* Input and output values */

	/* Return a pointer to the resolution array */
	int *(*get_res)(struct _rspl *s);

}; typedef struct _rspl rspl;

/* Create a new, empty rspl object */
rspl *new_rspl(int flags, int di, int fdi);

#ifdef __cplusplus
}
#endif

#define _RSPL1_H_
#endif /* _RSPL1_H_ */


