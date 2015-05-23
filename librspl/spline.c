
/* 
 * Argyll Color Correction System
 * Multi-dimensional regularized spline data structure
 *
 * Spline forward interpolation support.
 *
 * Author: Graeme W. Gill
 * Date:   12/10/98
 *
 * Copyright 1998, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* TTBD:
	Get rid of error() calls - return status instead
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
//#if defined(__IBMC__) && defined(_M_IX86)
//#include <float.h>
//#endif

#include "rspl_imp.h"
#include "numlib.h"

int spline_interp_rspl(rspl *ss, co *cp);

#undef DEBUG

#undef NEVER
#define ALWAYS

/* Convention is to use:
   i to index grid points u.a
   n to index data points d.a
   e to index position dimension di
   f to index output function dimension fdi
   j misc and cube corners
   k misc
 */

/* ====================================================== */

/* Init spline elements in rspl */
void
init_spline(rspl *s) {
	s->spline.nm = 0;
	s->spline.spline = 0;
	s->spline.magic = NULL;

	s->spline_interp = spline_interp_rspl;
}

/* Free up the spline interpolation info */
void free_spline(
rspl *s		/* Pointer to rspl grid */
) {
	if (s->spline.magic != NULL) {
		free(s->spline.magic);
	}
	s->spline.nm = 0;
	s->spline.spline = 0;
}

/* ====================================================== */
/* Setup functions first: */

/* Hermite spline, magic matrix */
/* Indexes are: param powers 0, 1, 2, 3; Offset from base vertex 0,1; Dimension mask 0,1 */
static double hmagic[4][2][2] = {
	{ { 1.0,  0.0}, { 0.0,  0.0} },
	{ { 0.0,  1.0}, { 0.0,  0.0} },
	{ {-3.0, -2.0}, { 3.0, -1.0} },
	{ { 2.0,  1.0}, {-2.0,  1.0} }
};

/* Allocate and initialize tangency information for each grid point */
static void make_tang(
rspl *s		/* Pointer to rspl grid */
) {
	int i,p,j;
	int di  = s->di;
	int fdi = s->fdi;
	int nig = s->g.no;
	float *tp;		/* Pointer to tangent values */
	int nim, mix;	/* Number in magic, magic index */
	float *tang_alloc, *tang;	/* Tangency info */
	float *gt;		/* Working grid point */
	
//printf("~~make_tang called\n");
	/* Organized as: tang[[grid]][di combs.][fdi] */
	/* Allocate space for tangency info */
	if ((tang_alloc = (float *) malloc(sizeof(float) * nig * (((1 << di) * fdi)+G_XTRA))) == NULL)
		error("rspl malloc failed - tangecy points");
	tang = tang_alloc + G_XTRA;	/* Offset for flags and non-mono error */

	/* For all grid points */
	for (tp = tang, gt = s->g.a, i = 0; i < nig; i++, gt += s->g.pss, tp += G_XTRA) {
		int ee;
/* printf("\n~~ grid point %d\n",i); */

		/* Look at surrounding grid points in combinations of +- 1 all dimensions */
		for (ee = 0; ee < (1 << di); ee++) {
			double av[MXRO];	/* average */
			int nia = 0;		/* Number in average */
			int f, ec;

/* printf("Dim combo %d\n",ee); */
			/* special case - base value */
			if (ee == 0) { 
				*((int *)(tp-2)) = *((int *)(gt-2));	/* Copy flags */
				tp[-1] = gt[-1];						/* Copy ink limit function value */
				for (f = 0; f < fdi; f++) {
					*tp++ = gt[f];
/* printf("Tang value out %d = %f\n",f,tp[-1]); */
				}
				continue;
			}
			for (f = 0; f < fdi; f++)
				av[f] = 0.0;	/* Init average */
			/* For all surroundin grid points in this combination */
			for (ec = 0; ec < (1 << di); ec++) {
				int xo, io, sgn, e, ex;
/* printf("~~checking out surrounding combo %d\n",ec); */
				if (ec & ~ee) {
/* printf("~~being skipped\n"); */
					continue;	/* Skip invalid combo */
				}
				xo = io = 0;		/* Grid float offset */
				sgn = 1;			/* Sign */
				ex = 0;				/* Flag - No extrapolation */
				for (e = 0; e < di; e++) {	/* For each dimension */
/* printf("~~checking  dimension %d\n",e); */
					if (!(ee & (1 << e))) {
/* printf("~~dimension not active\n"); */
						continue;	/* Dimension is not active */
					}
					if (ec & (1 << e)) {
						/* If + dimension is valid */
						if (((G_FL(gt,e) & 3) > 0) || (G_FL(gt,e) & 0x4)) {
							int to = s->g.fci[e];	/* +1 in dimension */
							io += to;				/* real/pivot point */
							xo += to;				/* reflected point */
						} else {
							ex = 1;					/* Use extrapolation */
							xo -= s->g.fci[e];		/* -1 in dimension */
						}
					} else {
						sgn = -sgn;			/* Reverse sign */
						/* If - dimension is valid */
						if (((G_FL(gt,e) & 3) > 0) || !(G_FL(gt,e) & 0x4)) {
							int to = -s->g.fci[e];	/* -1 in dimension */
							io += to;				/* real/pivot point */
							xo += to;				/* reflected point */
						} else {
							ex = 1;					/* Use extrapolation */
							xo += s->g.fci[e];		/* +1 in dimension */
						}
					}
				}
				/* Add surrounding grid points value into the average */
				if (!ex) {
					for (f = 0; f < fdi; f++) 
						av[f] += (double)sgn * gt[io + f];
				} else {	/* Extrapolate point beyond edge */
							/* Use an extrapolation that tries to maintain curvature */
					for (f = 0; f < fdi; f++)  {
						double v0,v1,v2;
						v0 = gt[io + f];			/* Pivot point */
						v1 = gt[xo + f];			/* Reflection of target in pivot */
						v2 = gt[2 * xo - io + f];	/* Reflection +2 */
						av[f] += (double)sgn * (3.0 * (v0 - v1) + v2);
					}
				}
				nia++;
			}
			for (f = 0; f < fdi; f++) {
				*tp++ = (float)(av[f]/(double)nia);
/* printf("Tang value out %d = %f, average of %d\n",f,tp[-1],nia); */
			}
		}	/* Next dimension combination */
	}	/* Next grid point */

	/* Create a full sized hermite magic matrix */
	/* Organized as: magic[4^di][2^di][2^di] */
	/* = [param power combos][cube vertex index][di combos], */
	/* but then only store non-zero weight values. */
	for (i = 0, nim = 1; i < di; nim *= 10, i++);	/* Number of entries needed */
	if (s->spline.magic == NULL) { /* Allocate space for magic matrix info */
		if ((s->spline.magic = (magic_data *) malloc(sizeof(magic_data) * nim)) == NULL)
			error("rspl malloc failed - hermite magic matrix data");
	}

	mix = 0;
	for (p = 0; p < (1 << (2 * di)); p++) {		/* For all combinations of parameter powers */
		for (i = 0; i < (1 << di); i++)	{		/* For all corners of cube */
			for (j = 0; j < (1 << di); j++)	{	/* For all dimension combinations */
				int ii;
				double wgt = 1.0;
				for (ii = 0; ii < di; ii++) {
					wgt *= hmagic[3&(p>>(2*ii))][1&(i>>ii)][1&(j>>ii)];
				}
				if (wgt != 0.0) {	/* record non-zero weight value */
					s->spline.magic[mix].p = p;
					s->spline.magic[mix].i = i;
					s->spline.magic[mix].j = fdi * j;	/* Pre-scale */
					s->spline.magic[mix].wgt = (float)wgt;
					mix++;
				}
			}
		}
	}
	/* mix should == nim! */
	s->spline.nm = nim;

	/* Free basic grid info, and substitute tangency enhanced version */
	/* ~~~~!! need to free any other structures in rspl that depend on */
	/* ~~~~!! g.pss size, ie. rev stuff ??? */
	if (s->g.alloc != NULL)
		free((void *)s->g.alloc);

	s->g.alloc  = tang_alloc;
	s->g.a      = tang;

	/* Adjust index tables */
	s->g.pss = (1 << di) * fdi + G_XTRA;
	for (i = 0; i < di; i++)
		s->g.fci[i] = s->g.ci[i] * s->g.pss;	/* In floats */
	for (i = 0; i < (1 << di); i++)
		s->g.fhi[i] = s->g.hi[i] * s->g.pss;	/* In floats */

	s->spline.spline = 1;

//printf("~~make_tang finished\n");
}

/* Do a Hermite spline smooth interpolation based on the finest grid */
/* (To do this more accurately, the data point interpolation within */
/*  the grid itteration should be of the same order. This increases */
/*  itteration complexity quite a bit, so we won't bother for the moment.) */
/* This code is not optimised for speed. */
/* Return 0 if OK, 1 if input was clipped to grid */
int spline_interp_rspl(
rspl *s,
co *cp			/* Input value and returned function value */
) {
	int e,f,p,i;
	int di  = s->di;
	int fdi = s->fdi;
	double ppw[MXRI][4];	/* Parameter powers of 0, 1, 2, 3 */
	float  *ga[POW2MXRI];	/* Pointers to grid cubes data in tang[] */
	magic_data *tp;			/* Pointer to items in magic matrix */
	int rv = 0;
	
/* printf("~~smooth interp called\n"); */

	/* This is a restricted size function */
	if (di > MXRI)
		error("rspl: spline can't handle di = %d",di);
	if (fdi > MXRO)
		error("rspl: spline can't handle fdi = %d",fdi);

	if (s->spline.spline == 0) 	/* Compute tangent info if it doesn't exist */
		make_tang(s);

	/* Locate grid base point, and position with base cube */
	ga[0] = s->g.a;	/* Base pointer of cube */
	for (e = 0; e < di; e++) {
		double t, pe;
		int mi, gres_1 = s->g.res[e]-1;

		pe = cp->p[e];
		if (pe < s->g.l[e]) {		/* Clip to grid */
			pe = s->g.l[e];
			rv = 1;
		}
		if (pe > s->g.h[e]) {
			pe = s->g.h[e];
			rv = 1;
		}
		t = (pe - s->g.l[e])/s->g.w[e];
		mi = (int)floor(t);			/* Grid coordinate */
		if (mi < 0)					/* Limit to valid cube base index range */
			mi = 0;
		else if (mi >= gres_1)
			mi = gres_1-1;
		ga[0] += s->g.fci[e] * mi;	/* Add offset in dimen */
		t = t - (double)mi;;		/* sub-cube offset = parameter in dimension e */
		ppw[e][0] = 1.0;			/* Powers of parameter */
		ppw[e][1] = t;
		ppw[e][2] = t * t;
		ppw[e][3] = t * t * t; 
	}

	/* Compute indexes into cube corners in tangent array */
	for (i = 1; i < (1 << di); i++)
		ga[i] = ga[0] + s->g.fhi[i];

	/* Now compute the output values */
	for (f = 0; f < fdi; f++)		/* Zero output value sums */
		cp->v[f] = 0.0;
	
	/* For all non-zero combinations of parameter powers */
	{
		double ppc = -1000.0;			/* Parameter power combination */
		for (tp = s->spline.magic, p = -1; tp < &s->spline.magic[s->spline.nm]; tp++) {
			double wgt;			/* Magic matrix weight */
			float *gp;			/* Pointer to vertex data */

			if (p != tp->p) {		/* Param power needs re-calculating */
				int pp;
				p = tp->p;
				for (ppc = 1.0, pp = 0; pp < di; pp++)
					ppc *= ppw[pp][3&(p>>(2*pp))];		/* comb. of param powers value */
			}

			wgt = tp->wgt * ppc;		/* matrix times parameter */
			gp = ga[tp->i] + tp->j;		/* Point to base of vertex data */
			for (f = 0; f < fdi; f++) 	/* For all output values */
				cp->v[f] += wgt * gp[f];
		}
	}
/* printf("~~smooth interp finished\n"); */
	return rv;
}



















