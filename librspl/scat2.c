
/* 
 * Argyll Color Correction System
 * Multi-dimensional multilevel spline data fitter
 * mlbs base version.
 *
 * Author: Graeme W. Gill
 * Date:   2000/11/10
 *
 * Copyright 1996 - 2000 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* This file contains the scattered data solution specific code */

/* TTBD:
 *
 * mlbs code doesn't work. Results are rubbish.
 *
 * Fix bugs ?
 * merge stest.c into this file.
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
#include "mlbs.h"

extern void error(char *fmt, ...), warning(char *fmt, ...);

#undef DEBUG

#undef NEVER
#define ALWAYS

/* Implemented in rspl.c: */
extern void alloc_grid(rspl *s);

extern int is_mono(rspl *s);

/* ============================================ */
void set_from_mlbs(void *cbctx, double *out, double *in) {
	mlbs *p = (mlbs *)cbctx;
	co tp;
	int i;

	for (i = 0; i < p->di; i++)
		tp.p[i] = in[i];

	if (p->lookup(p, &tp))
		error("Internal, set_from_mlbs failed!");

	for (i = 0; i < p->di; i++)
		out[i] = tp.v[i];
}

/* Initialise the regular spline from scattered data */
/* Return non-zero if non-monotonic */
static int
fit_rspl_imp(
	rspl *s,		/* this */
	int flags,		/* Combination of flags */
	void *d,		/* Array holding position and function values of data points */
	int dtp,		/* Flag indicating data type, 0 = (co *), 1 = (cow *) */
	int dno,		/* Number of data points */
	datai glow,		/* Grid low scale - will be expanded to enclose data, NULL = default 0.0 */
	datai ghigh,	/* Grid high scale - will be expanded to enclose data, NULL = default 1.0 */
	int gres,		/* Spline grid resolution */
	datao vlow,		/* Data value low normalize, NULL = default 0.0 */
	datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
	double smooth	/* Smoothing factor, nominal = 1.0 */
) {
	int di = s->di, fdi = s->fdi;
	int i, n, e, f;
	int rv;
	int nigc;
	int bres;
	mlbs *p;

#if defined(__IBMC__) && defined(_M_IX86)
	_control87(EM_UNDERFLOW, EM_UNDERFLOW);
#endif
	/* set debug level */
	s->debug = (flags >> 24);

	/* Init other flags */
	if (flags & RSPL_NONMON)		/* Enable elimination of non-monoticities */
		s->nm = 1;
	else
		s->nm = 0;

	if (flags & RSPL_VERBOSE)		/* Turn on progress messages to stdout */
		s->verbose = 1;
	else
		s->verbose = 0;

	/* Save smoothing factor */
	s->smooth = smooth;

	/* Stash the data points away */
	s->d.no = dno;			/* Number of data points */

	/* Allocate the scattered data space */
	if ((s->d.a = (dpnts *) malloc(sizeof(dpnts) * s->d.no)) == NULL)
		error("rspl malloc failed - data points");

	if (dtp == 0) {			/* Default weight */
		co *dp = (co *)d;

		/* Copy the list into data points */
		for (n = 0; n < s->d.no; n++) {
			for (e = 0; e < s->di; e++)
				s->d.a[n].p[e] = dp[n].p[e];
			for (f = 0; f < s->fdi; f++)
				s->d.a[n].v[f] = dp[n].v[f];
			s->d.a[n].k = 1.0;		/* Assume all data points have same weight */
		}
	} else {				/* Per data point weight */
		cow *dp = (cow *)d;

		/* Copy the list into data points */
		for (n = 0; n < s->d.no; n++) {
			for (e = 0; e < s->di; e++)
				s->d.a[n].p[e] = dp[n].p[e];
			for (f = 0; f < s->fdi; f++)
				s->d.a[n].v[f] = dp[n].v[f];
			s->d.a[n].k = dp[n].w; /* Weight specified */
		}
	}

	/* Compute target B-Spline resolution */
	/* Make it worst case half the target rspl resolution */
	for (bres = 2; (2 * bres) < gres; bres = 2 * bres -1)
		;

	/* Create multilevel B-Spline fit */
	p = new_mlbs(di, fdi, bres, s->d.a, s->d.no, glow, ghigh, smooth);
	if (p == NULL)
		error("new_mlbs() failed");

	/* Create rspl grid points by looking up the B-Spline values */
	rv = s->set_rspl(s, 0, (void *)p, set_from_mlbs, glow, ghigh, gres, vlow, vhigh); 

	/* Don't need B-Spline any more */
	p->del(p);

	return rv;
}

/* Initialise the regular spline from scattered data */
/* Return non-zero if non-monotonic */
int
fit_rspl(
	rspl *s,		/* this */
	int flags,		/* Combination of flags */
	co *d,			/* Array holding position and function values of data points */
	int dno,		/* Number of data points */
	datai glow,		/* Grid low scale - will be expanded to enclose data, NULL = default 0.0 */
	datai ghigh,	/* Grid high scale - will be expanded to enclose data, NULL = default 1.0 */
	int gres,		/* Spline grid resolution */
	datao vlow,		/* Data value low normalize, NULL = default 0.0 */
	datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
	double smooth	/* Smoothing factor, nominal = 1.0 */
) {
	/* Call implementation with (co *) data */
	return fit_rspl_imp(s, flags, (void *)d, 0, dno, glow, ghigh, gres, vlow, vhigh, smooth);
}

/* Initialise the regular spline from scattered data with weights */
/* Return non-zero if non-monotonic */
int
fit_rspl_w(
	rspl *s,		/* this */
	int flags,		/* Combination of flags */
	cow *d,			/* Array holding position, function and weight values of data points */
	int dno,		/* Number of data points */
	datai glow,		/* Grid low scale - will be expanded to enclose data, NULL = default 0.0 */
	datai ghigh,	/* Grid high scale - will be expanded to enclose data, NULL = default 1.0 */
	int gres,		/* Spline grid resolution */
	datao vlow,		/* Data value low normalize, NULL = default 0.0 */
	datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
	double smooth	/* Smoothing factor, nominal = 1.0 */
) {
	/* Call implementation with (cow *) data */
	return fit_rspl_imp(s, flags, (void *)d, 1, dno, glow, ghigh, gres, vlow, vhigh, smooth);
}

/* Init scattered data elements in rspl */
void
init_data(rspl *s) {
	s->d.no = 0;
	s->d.a = NULL;
	s->fit_rspl = fit_rspl;
	s->fit_rspl_w = fit_rspl_w;
}

/* Free the scattered data allocation */
void
free_data(rspl *s) {
	if (s->d.a != NULL) {
		free((void *)s->d.a);
		s->d.a = NULL;
	}
}

/* ============================================ */















