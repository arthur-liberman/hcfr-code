
/* 
 * Argyll Color Correction System
 * Multi-dimensional regularized splines
 * optimiser based initialiser.
 *
 * Author: Graeme W. Gill
 * Date:   2001/5/16
 *
 * Copyright 1996 - 2001 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* This file contains an rspl initialiser that */
/* works from an optimisation function callback. */
/* It is intended to support the creation of optimised */
/* color separations, although this usage is not hard coded */
/* here. */

/* TTBD:
 *
 * !!! fix so that this can also be used for smoothed
 *     inversion, ie. PCS -> DevN, as well as
 *     separation PseudoCMY/K -> DevN.
 *
 * Plan:
 *		Have additional callback function used for invert,
 *		called at grid initialisation that initialised
 *		the target values to fixed PCS values.
 *		For separation, these are dynamic, and adjusted by
 *		the usual optimisation callback.
 *		(Or can the usual callback figure out when the
 *		 initial initialisation is needed ?)
 *
 *		Need to return average/extrapolated surround values
 *		on the edge, just like fit, so smoothness can be
 *		evaluated in inversion.
 *		Provide another mechanism for sep to know
 *		it is on the edge of the grid, and should expand
 *		gamut if possible.
 *
 * Get rid of error() calls - return status instead
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#if defined(__IBMC__) && defined(_M_IX86)
#include <float.h>
#endif

#include "rspl_imp.h"
#include "numlib.h"
#include "counters.h"	/* Counter macros */

#undef DEBUG

/* Tuning parameters */
#define TOL 1e-6		/* Tollerance of result */
#define GRATIO 1.7		/* Multi-grid ratio */
#define SMOOTH 80.0		/* Set nominal smoothing (1.0) */

#undef NEVER
#define ALWAYS

/* Implemented in rspl.c: */
extern void alloc_grid(rspl *s);

extern int is_mono(rspl *s);

/* Convention is to use:
   i to index grid points u.a
   n to index data points d.a
   e to index position dimension di
   f to index output function dimension fdi
   j misc and cube corners
   k misc
 */

/* ================================================= */
/* Structure to hold temporary data for multi-grid caliculations */
/* Only used in this file. */
struct _omgtp {
	rspl *s;	/* Associated rspl */

	/* Configuration data */
	int tdi;	/* Target guide values dimensionality (must be <= MXDI) */
				/* (Typically the Lab aim values corresponding to this pseudo device value) */
	int adi;	/* Additional grid point data allowance (must be <= 2 * MXDI) */
				/* (Typically black locus range) */

	double (*func)(void *fdata, double *inout, double *surav, int first, double *cw);
					/* Optimisation function */
	void *fdata;	/* Pointer to opaque data needed by callback function */

	struct {
		double cw[MXDI];	/* Curvature weight factor for each dimension */
	} sf;

	/* Grid points data */
	struct {
		int res[MXDI];	/* Single dimension grid resolution for each dimension */
		int bres, brix;	/* Biggest resolution and its index */
		double mres;	/* Geometric mean res[] */
		int no;			/* Total number of points in grid = res ^ di */
		datai l,h,w;	/* Grid low, high, grid cell width */

	
		double *a;		/* Grid point data */
						/* Array is res ^ di entries double[fdi+tdi+adi] */
						/* The output values start at offset 0, the */
						/* target data values start at offset fdi, and */
						/* the additional data starts at offset fdi+tdi. */
		int pss;		/* Grid point structure size = fdi+tdi */

		/* Grid array offset lookups */
		int ci[MXDI];		/* Grid coordinate increments for each dimension */
		int fci[MXDI];		/* Grid coordinate increments for each dimension in doubles */
		int *hi;			/* 2^di Combination offset for sequence through cube. */
		int *fhi;			/* Combination offset for sequence through cube of */
							/* 2^di points, starting at base, in floats */
		int a_hi[DEF2MXDI];	/* Default allocation for *hi */
		int a_fhi[DEF2MXDI];/* Default allocation for *fhi */
	} g;

}; typedef struct _omgtp omgtp;

/* ================================================= */
static omgtp *new_omgtp(rspl *s, int tdi, int adi, int mxres,
	double (*func)(void *fdata, double *inout, double *surav, int first, double *cw),
	void *fdata);
static void free_omgtp(omgtp *m);
static void solve_gres(omgtp *m, double tol);
static void init_soln(omgtp  *m1, omgtp  *m2);
static void init_fsoln(omgtp *m, double **vdata);

/* Initialise the regular spline from the optimisation callback function. */
/* The target data is auxiliary data used to "target" the optimisation */
/* callback function. */
/* The callback function arguments are as follows:
 *	void *fdata,
 *	double *inout,	Pointers to fdi+tdi+adi values for the grid point being optimised.
 *	double *surav,	Pointers to fdi+tdi values which are the average of the
 *					neighbors of this grid point. Pointer will NULL if this
 *					is a surface grid point.
 *	int first,		Flag, NZ if this is the first optimisation of this point.
 *	double *cw		the (grid resolution) curvature weighting factor for each dimension 
 *	
 *	Returns value is the "error" for this point.
 */

int
opt_rspl_imp(
	rspl *s,		/* this */
	int flags,		/* Combination of flags */
	int tdi,		/* Dimensionality of target data */
	int adi,		/* Additional per grid point data allocation */
	double **vdata,	/* di^2 array of function, target and additional values to init */
					/* array corners with. Corners are ordered with lowest index */
					/* dimension changing most rapidly. */
	double (*func)(void *fdata, double *inout, double *surav, int first, double *cw),
					/* Optimisation function */
	void *fdata,	/* Opaque data needed by function */
	datai glow,		/* Grid low scale - NULL = default 0.0 */
	datai ghigh,	/* Grid high scale - NULL = default 1.0 */
	int gres[MXDI],	/* Spline grid resolution for each dimension */
	datao vlow,		/* Data value low normalize, NULL = default 0.0 */
	datao vhigh		/* Data value high normalize - NULL = default 1.0 */
) {
//	int di = s->di
	int fdi = s->fdi;
	int i, e, f;
//	int n;

#if defined(__IBMC__) && defined(_M_IX86)
	_control87(EM_UNDERFLOW, EM_UNDERFLOW);
#endif
	/* set debug level */
	s->debug = (flags >> 24);

	if (flags & RSPL_VERBOSE)	/* Turn on progress messages to stdout */
		s->verbose = 1;
	if (flags & RSPL_NOVERBOSE)	/* Turn off progress messages to stdout */
		s->verbose = 0;

	s->symdom = (flags & RSPL_SYMDOMAIN) ? 1 : 0;	/* Turn on symetric smoothness with gres */

	if (tdi >= MXDI)
		error("rspl, opt: tdi %d > MXDI %d",tdi,MXDI);

	if (adi >= (2 * MXDI))
		error("rspl, opt: adi %d > 2 * MXDI %d",adi,2 * MXDI);

	/* transfer desired grid range to structure */
	s->g.mres = 1.0;
	s->g.bres = 0;
	for (e = 0; e < s->di; e++) {
		if (gres[e] < 2)
			error("rspl: grid res must be >= 2!");
		s->g.res[e] = gres[e];	/* record the desired resolution of the grid */
		s->g.mres *= gres[e];
		if (gres[e] > s->g.bres) {
			s->g.bres = gres[e];
			s->g.brix = e;
		}

		if (glow == NULL)
			s->g.l[e] = 0.0;
		else
			s->g.l[e] = glow[e];

		if (ghigh == NULL)
			s->g.h[e] = 1.0;
		else
			s->g.h[e] = ghigh[e];
	}
	s->g.mres = pow(s->g.mres, 1.0/e);		/* geometric mean */

	/* compute width of each grid cell */
	for (e = 0; e < s->di; e++) {
		s->g.w[e] = (s->g.h[e] - s->g.l[e])/(double)(gres[e]-1);
	}

	/* record low and width data normalizing factors */
	for (f = 0; f < s->fdi; f++) {
		if (vlow == NULL)
			s->d.vl[f] = 0.0;
		else
			s->d.vl[f] = vlow[f];

		if (vhigh == NULL)
			s->d.vw[f] = 1.0 - s->d.vl[f];
		else
			s->d.vw[f] = vhigh[f] - s->d.vl[f];
	}

	/* Do optimisation of data points */
	{
		int nn, res, sres;
		double fres, gratio = GRATIO;
		float *gp;		/* rspl grid pointer */
		double *mgp;	/* Temp muligrid pointer */
		omgtp *m, *om = NULL;

		sres = 4;		/* Start at initial grid res of 4 */
		if (sres > s->g.bres)
			sres = s->g.bres;	/* Drop to target resolution */

		/* Calculate the resolution scaling ratio */
		if (((double)s->g.bres/(double)sres) <= gratio) {
			gratio = (double)s->g.bres/(double)sres;
			nn = 1;
		} else {	/* More than one needed */
			nn = (int)((log((double)s->g.bres) - log((double)sres))/log(gratio) + 0.5);
			gratio = exp((log((double)s->g.bres) - log((double)sres))/(double)nn);
		}

		/* Do each grid resolution in turn */
		for (fres = (double)sres, res = sres;;) {
			m = new_omgtp(s, tdi, adi, res, func, fdata);

			if (om == NULL) {
				init_fsoln(m, vdata);	/* Set the initial targets & values from corners */
			} else {
				init_soln(m, om);	/* Scale targets & values from from previous resolution */
				free_omgtp(om);		/* Free previous grid res solution */
			}
			solve_gres(m, TOL * s->g.mres/res);	/* Use itterative */

			if (res >= s->g.mres)
				break;	/* Done */

			fres *= gratio;
			res = (int)(fres + 0.5);
			if ((res + 1) >= s->g.mres)	/* If close enough */
				res = (int)s->g.mres;
			om = m;
		}

		/* Allocate the final rspl grid data */
		alloc_grid(s);
	
		/* Transfer result in x[] to appropriate grid point value */
		for (gp = s->g.a, mgp = m->g.a, i = 0; i < s->g.no; gp += s->g.pss, mgp += m->g.pss, i++)
			for (f = 0; f < fdi; f++)
				gp[f] = (float)mgp[f];
		free_omgtp(m);
	}

	/* Return non-mono check */
	return is_mono(s);
}

/* - - - - - - - - - - - - - - - - - - - - - - - -*/
/* omgtp routines */

/* Create a new omgtp. */
/* Grid data will be uninitialised */
static omgtp *new_omgtp(
	rspl *s,		/* associated rspl */
	int tdi,		/* Target dimensions */
	int adi,		/* Additional per grid point data allocation */
	int mxres,		/* maximum resolution to create */
	double (*func)(void *fdata, double *inout, double *surav, int first, double *cw),
					/* Optimisation function */
	void *fdata		/* Opaque data needed by function */
) {
	omgtp *m;
	int di = s->di, fdi = s->fdi;
//	int dno = s->d.no;
	int gno;
	int e, g, i;
//	int f, n, j, k;

	/* Allocate a structure */
	if ((m = (omgtp *) calloc(1, sizeof(omgtp))) == NULL)
		error("rspl: malloc failed - omgtp");

	/* Allocate space for cube offset arrays */
	m->g.hi = m->g.a_hi;
	m->g.fhi = m->g.a_fhi;
	if ((1 << di) > DEF2MXDI) {
		if ((m->g.hi = (int *) malloc(sizeof(int) * (1 << di))) == NULL)
			error("rspl omgtp malloc failed - hi[]");
		if ((m->g.fhi = (int *) malloc(sizeof(int) * (1 << di))) == NULL)
			error("rspl omgtp malloc failed - fhi[]");
	}

	/* General stuff */
	m->s = s;
	m->tdi = tdi;
	m->adi = adi;
	m->func = func;
	m->fdata = fdata;

	/* Grid related */
	m->g.mres = 1.0;
	m->g.bres = 0;
	for (gno = 1, e = 0; e < di; e++) {
		if (mxres >= s->g.res[e])			/* Shoose smaller of gres and target res */
			m->g.res[e] = s->g.res[e];
		else
			m->g.res[e] = mxres;

		m->g.mres *= m->g.res[e];
		if (m->g.res[e] > m->g.bres) {
			m->g.bres = m->g.res[e];
			m->g.brix = e;
		}
		gno *= m->g.res[e];
	}
	m->g.mres = pow(m->g.mres, 1.0/e);		/* geometric mean */
	m->g.no = gno;

	m->g.pss = fdi+tdi+adi;	/* doubles for each output value + target data + additional data */

	/* record high, low limits, and width of each grid cell */
	for (e = 0; e < s->di; e++) {
		m->g.l[e] = s->g.l[e];
		m->g.h[e] = s->g.h[e];
		m->g.w[e] = (s->g.h[e] - s->g.l[e])/(double)(m->g.res[e]-1);
	}

	/* Compute index coordinate increments into linear grid for each dimension */
	/* ie. 1, gres, gres^2, gres^3 */
	for (m->g.ci[0] = 1, e = 1; e < di; e++) {
		m->g.ci[e] = m->g.ci[e-1] * m->g.res[e-1];	/* In grid points */
		m->g.fci[e] = m->g.ci[e] * m->g.pss;		/* In doubles */
	}

	/* Compute index offsets from base of cube to other corners */
	for (m->g.hi[0] = 0, e = 0, g = 1; e < di; g *= 2, e++) {
		for (i = 0; i < g; i++) {
			m->g.hi[g+i] = m->g.hi[i] + m->g.ci[e];		/* In grid points */
			m->g.fhi[g+i] = m->g.hi[g+i] * m->g.pss;	/* In doubles */
		}
	}

	/* Allocate space for grid */
	if ((m->g.a = (double *) malloc(sizeof(double) * gno * m->g.pss)) == NULL)
		error("rspl malloc failed - multi-grid points");

	/* Compute curvature weighting for matching intermediate resolutions. */
	/* cw[] is multiplied by the grid curvature_errors_squared[] to keep */
	/* the same ratio with the sum of data position errors squared. */
	for (e = 0; e < di; e++) {
		double rsm;				/* Resolution smoothness factor */
		if (s->symdom)
			rsm = m->g.res[e]-1.0;	/* Relative final grid size  */
		else
			rsm = m->g.mres-1.0;	/* Relative mean final grid size */
		rsm =    pow(rsm,8.0/di);
		rsm /= pow(200.0,8.0/di)/pow(200.0, 4.0);	/* (Scale factor to adjust power) */

		m->sf.cw[e] = (s->smooth * SMOOTH)/(rsm * (double)di);
	}

	return m;
}

/* Completely free an omgtp */
static void free_omgtp(omgtp  *m) {

	free((void *)m->g.a);

	/* Free structure */
	if (m->g.hi != m->g.a_hi) {
		free(m->g.hi);
		free(m->g.fhi);
	}
	free((void *)m);
}

/* Set the first targets & values from the corner values. */
static void init_fsoln(
omgtp *m,			/* Destination */
double **vdata		/* di^2 array of function and target values to init array corners with. */
					/* Corners are ordered with lowest index dimension changing most rapidly. */
					/* (Function data at index 0, target data at index fdi) */
) {
	rspl *s = m->s;
	int di  = s->di;
	int fdi = s->fdi;
	int gno = m->g.no;
	int gres_1[MXDI];
	int e, n;
	double *gp;				/* Pointer to dest g.a[] grid cube base */
	ECOUNT(gc, MXDIDO, di, 0, m->g.res, 0);	/* Counter for output points */
	double *gw;				/* weight for each grid cube corner */
	double a_gw[DEF2MXDI];	/* default allocation for gw */

	gw = a_gw;
	if ((1 << di) > DEF2MXDI) {
		if ((gw = (double *) malloc(sizeof(double) * (1 << di))) == NULL)
			error("rspl malloc failed - interp_rspl_nl");
	}

	for (e = 0; e < di; e++)
		gres_1[e] = m->g.res[e]-1;

	/* For all output grid points (could skip non-surface points ?) */
	EC_INIT(gc);
	for (n = 0, gp = m->g.a; n < gno; n++, gp += m->g.pss) {
		double we[MXDI];		/* 1.0 - Weight in each dimension */
		
		/* Figure out the pointer to the grid data and its weighting */
		{
			gp = m->g.a;					/* Base of output array */
			for (e = 0; e < di; e++)
				we[e] = (double)gc[e]/gres_1[e]; /* 1.0 - weight */
		}

		/* Compute corner weights needed for interpolation */
		{
			int i, g;
			gw[0] = 1.0;
			for (e = 0, g = 1; e < di; g *= 2, e++) {
				for (i = 0; i < g; i++) {
					gw[g+i] = gw[i] * we[e];
					gw[i] *= (1.0 - we[e]);
				}
			}
		}

		/* Compute the output values */
		{
			int i, f;
			double w = gw[0];
			double *d = vdata[0];

			for (f = 0; f < m->g.pss; f++)		/* Base of cube */
				gp[f] = w * d[f];

			for (i = 1; i < (1 << di); i++) {	/* For all other corners of cube */
				w = gw[i];						/* Strength reduce */
				d = vdata[i];
				for (f = 0; f < fdi; f++)
					gp[f] += w * d[f];
			}
		
		}

		EC_INC(gc);
	}

	if (gw != a_gw)
		free(gw);
}


/* Transfer a device and target values solution from one omgtp to another. */
/* (We assume that they are for the same problem) */
static void init_soln(
	omgtp  *m1,		/* Destination */
	omgtp  *m2		/* Source */
) {
	rspl *s = m1->s;
	int di  = s->di;
	int gno = m1->g.no;
	int gres1_1[MXDI];
	int gres2_1[MXDI];
	int e, n;
	double *a;				/* Pointer to dest g.a[] grid cube base */
	ECOUNT(gc, MXDIDO, di, 0, m1->g.res, 0);	/* Counter for output points */
	double *gw;				/* weight for each grid cube corner */
	double a_gw[DEF2MXDI];	/* default allocation for gw */

	gw = a_gw;
	if ((1 << di) > DEF2MXDI) {
		if ((gw = (double *) malloc(sizeof(double) * (1 << di))) == NULL)
			error("rspl malloc failed - interp_rspl_nl");
	}

	for (e = 0; e < di; e++) {
		gres1_1[e] = m1->g.res[e]-1;
		gres2_1[e] = m2->g.res[e]-1;
	}

	/* For all output grid points */
	EC_INIT(gc);
	for (n = 0, a = m1->g.a; n < gno; n++, a += m1->g.pss) {
		double we[MXDI];		/* 1.0 - Weight in each dimension */
		double *gp;				/* Pointer to source g.a[] grid cube base */
		
		/* Figure out which grid cell the point falls into */
		{
			double t;
			int mi;
			gp = m2->g.a;					/* Base of solution array */
			for (e = 0; e < di; e++) {
				t = (double)gc[e] * gres2_1[e]/gres1_1[e];
				mi = (int)floor(t);			/* Grid coordinate */
				if (mi < 0)					/* Limit to valid cube base index range */
					mi = 0;
				else if (mi >= gres2_1[e])
					mi = gres2_1[e]-1;
				gp += mi * m2->g.fci[e];	/* Add Index offset for grid cube base in dimen */
				we[e] = t - (double)mi;		/* 1.0 - weight */
			}
		}

		/* Compute corner weights needed for interpolation */
		{
			int i, g;
			gw[0] = 1.0;
			for (e = 0, g = 1; e < di; g *= 2, e++) {
				for (i = 0; i < g; i++) {
					gw[g+i] = gw[i] * we[e];
					gw[i] *= (1.0 - we[e]);
				}
			}
		}

		/* Compute the output values */
		{
			int i, f;
			double w = gw[0];
			double *d = gp + m2->g.fhi[0];

			for (f = 0; f < m1->g.pss; f++)			/* Base of cube */
				a[f] = w * d[f];

			for (i = 1; i < (1 << di); i++) {	/* For all other corners of cube */
				w = gw[i];						/* Strength reduce */
				d = gp + m2->g.fhi[i];
				for (f = 0; f < m1->g.pss; f++)
					a[f] += w * d[f];
			}
		
		}
		EC_INC(gc);
	}

	if (gw != a_gw)
		free(gw);
}

/* - - - - - - - - - - - - - - - - - - - -*/
static double one_itter(omgtp *m, int first);

/* Itterate the optimisation functions until we are happy things have settled */
static void
solve_gres(
omgtp *m,
double tol
) {
	int i;
	double dtol = tol * 0.1;	/* Delta tol limit */
	double ltt, tt;

	ltt = 1.0;
	tt = tol * 10.0;

	for (i = 0; i < 500; i++) {
		if (i == 0)
			tt = one_itter(m, 1);

		ltt = tt;
		tt = one_itter(m, 0);

		if (tt < tol || (ltt - tt) < dtol)	/* Get within 0.1 % */
			break;
	}
}

/* Optimise the points values and (optionally) targets */
/* Use Red/Black order, return total error after this itteration. */
/* Return the total optimisation error */
static double
one_itter(
omgtp *m,
int first		/* Flag, NZ if this is the first pass at this resolution */
) {
	int di = m->s->di, fdi = m->s->fdi;
	int tdi = m->tdi;
	int i, e, f;
	int gc[MXDI];
	int *gres = m->g.res;
	int gres_1[MXDI];
	DCOUNT(cc, MXDIDO, di, -1, -1, 2);	/* Surrounding cube counter */
	double *gpp;				/* Current grid point pointer */
	double ssum[MXDO+MXDI+2*MXDI];	/* Pointer to surrounding average values */
	double *surav;				/* Surrounding average values */
	double awt;					/* Average weight */
	double terr = 0.0;			/* Total error */
	int surf;					/* This point is on the surface */

	for (e = 0; e < di; e++) {
		gc[e] = 0;	/* init coords */
		gres_1[e] = gres[e] - 1;
	}

	/* Until done */
	for (;;) {

		/* See if we are on the surface */
		surf = 0;
		gpp = m->g.a;
		for (e = 0; e < di; e++) {
			gpp += m->g.fci[e] * gc[e];	/* Compute pointer to current point */

			if (gc[e] == 0 || gc[e] == gres_1[e])
				surf = 1;
		}

		surav = NULL;
		if (!surf) {

			for (f = 0; f < (fdi + tdi); f++)
				ssum[f] = 0.0;
			awt = 0.0;
 
			/* Average the 3x3 surrounders */
			DC_INIT(cc)
			for (i = 0; !DC_DONE(cc); i++ ) {
				double *gp = m->g.a;

				for (e = 0; e < di; e++) {
					int j;
					j = gc[e] + cc[e];
					if (j < 0 && j > gres_1[e]) {	/* outside */
						break;
					}
					gp += m->g.fci[e] * j;	/* Compute pointer to surrounder */
				}
				if (e >= di) {	/* We have a valid point */
					for (f = 0; f < (fdi + tdi); f++)
						ssum[f] += gp[f];
					awt += 1.0;
				}
				DC_INC(cc);
			}
			if (awt > 0.0) {	/* Compute the average */
				for (f = 0; f < (fdi + tdi); f++)
					ssum[f] /= awt;
				surav = ssum;
			}
		}
		
		/* Call optimisation function */
		terr += m->func(m->fdata, gpp, surav, first, m->sf.cw);

		/* Increment index in red/black order */
		for (e = 0; e < di; e++) {
			if (e == 0) {
				gc[0] += 2;	/* Inc coordinate by 2 */
			} else {
				gc[e] += 1;		/* Inc coordinate */
			}
			if (gc[e] < gres[e])
				break;	/* No carry */
			gc[e] -= gres[e];			/* Reset coord */

			if ((gres[e] & 1) == 0) {	/* Compensate for odd grid */
				gc[0] ^= 1; 			/* XOR lsb */
			}
		}
		/* Stop on reaching 0 */
		for(e = 0; e < di; e++)
			if (gc[e] != 0)
				break;
		if (e == di)
			break;		/* Finished */
	}

	return terr;
}













