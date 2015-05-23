
/* 
 * Argyll Color Correction System
 * Multi-dimensional regularized splines data fitter
 *
 * Author: Graeme W. Gill
 * Date:   30/1/00
 *
 * Copyright 1996 - 2004 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* Version that a combination of relaxation and conjugate gradient */
/* solution techniques. */

/* TTBD:

   To save space, the full columns of A should only be allocated when needed ?
   (Does this actually save much space for a realistic data sample set ?)

   Get rid of error() calls - return status instead.
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
#undef DEBUGLU			/* Debug fwd interpolation */
#undef VERBOSE

#undef NEVER
#define ALWAYS

#ifdef DEBUGLU
# define DEBLU(xxxx) printf xxxx
#else
# define DEBLU(xxxx)
#endif

/* Implemeted in this file: */
rspl *new_rspl(int flags, int di, int fdi);
static void free_rspl(rspl *s);
static void init_grid(rspl *s);
static void free_grid(rspl *s);
static void get_in_range(rspl *s, double *min, double *max);
static void get_out_range(rspl *s, double *min, double *max);
static void get_out_range_points(rspl *s, int *minp, int *maxp);
static double get_out_scale(rspl *s);
static unsigned int get_next_touch(rspl *s);
static int within_restrictedsize(rspl *s);
static int interp_rspl_sx(rspl *s, co *pp);
static int part_interp_rspl_sx(rspl *s, co *p1, co *p2);
static int interp_rspl_nl(rspl *s, co *p);
int is_mono(rspl *s);
static int set_rspl(rspl *s, int flags, void *cbctx,
                        void (*func)(void *cbctx, double *out, double *in),
                        datai glow, datai ghigh, int gres[MXDI], datao vlow, datao vhigh);
static int re_set_rspl(struct _rspl *s, int flags, void *cbntx,
                        void (*func)(void *cbntx, double *out, double *in));
static void scan_rspl(struct _rspl *s, int flags, void *cbntx,
                        void (*func)(void *cbntx, double *out, double *in));
static int tune_value(struct _rspl *s, co *p);
static void filter_rspl(struct _rspl *s, int flags, void *cbctx,
                        void (*func)(void *cbntx, float **out, double *in, int cvi));

extern int add_rspl(rspl *s, int flags, co *d, int dno);
extern void init_data(rspl *s);
extern void free_data(rspl *s);

/* Implemented in rev.c: */
void init_rev(rspl *s);
void free_rev(rspl *s);

/* Implemented in gam.c: */
void init_gam(rspl *s);
void free_gam(rspl *s);

/* Implemented in spline.c: */
void init_spline(rspl *s);
void free_spline(rspl *s);

/* Implemented in opt.c: */
int opt_rspl_imp(struct _rspl *s, int flags, int tdi, int adi, double **vdata,
	double (*func)(void *fdata, double *inout, double *surav, int first, double *cw),
	void *fdata, datai glow, datai ghigh, int gres[MXDI], datao vlow, datao vhigh);


/* Convention is to use:
   i to index grid points u.a
   n to index data points d.a
   e to index position dimension di
   f to index output function dimension fdi
   j misc and cube corners
   k misc
 */

/* ================================ */
/* Allocate an empty rspl object. */
rspl *
new_rspl(
	int flags,
	int di,
	int fdi
) {
	rspl *s;

#ifdef DEBUG
	fprintf(stderr,"new_rspl with flags 0x%x, di %d, fdi %d\n",flags,di,fdi);
#endif
	/* Allocate a structure */
	if ((s = (rspl *) calloc(1, sizeof(rspl))) == NULL)
		error("rspl: malloc failed - main structure");

	/* Set our fundamental parameters */
	if (di < 1 || di > MXDI)
		error("rspl: can't handle input dimension %d",di);
	s->di = di;

	if (fdi < 1 || fdi > MXDO)
		error("rspl: can't handle output dimension %d",fdi);
	s->fdi = fdi;

	/* And appropriate flags */
	if (flags & RSPL_VERBOSE)	/* Turn on progress messages to stdout */
		s->verbose = 1;
	if (flags & RSPL_NOVERBOSE)	/* Turn off progress messages to stdout */
		s->verbose = 0;

	/* Allocate space for cube offset arrays */
	s->g.hi = s->g.a_hi;
	s->g.fhi = s->g.a_fhi;
	if ((1 << di) > DEF2MXDI) {
		if ((s->g.hi = (int *) malloc(sizeof(int) * (1 << di))) == NULL)
			error("rspl malloc failed - hi[]");
		if ((s->g.fhi = (int *) malloc(sizeof(int) * (1 << di))) == NULL)
			error("rspl malloc failed - fhi[]");
	}

	/* Init sub sections */
	init_data(s);
	init_grid(s);
	init_rev(s);
	init_gam(s);
	init_spline(s);

	if (flags & RSPL_FASTREVSETUP)
		s->rev.fastsetup = 1;
	else
		s->rev.fastsetup = 0;

	/* Set pointers to methods in this file */
	s->del           = free_rspl;
	s->interp        = interp_rspl_sx;	/* Default to simplex interp */
#ifdef NEVER
#define USING_INTERP_NL
printf("!!!! rspl.c using interp_rspl_nl !!!!");
	s->interp        = interp_rspl_nl;
#endif
	s->part_interp   = part_interp_rspl_sx;
	s->set_rspl      = set_rspl;
	s->scan_rspl     = scan_rspl;
	s->re_set_rspl   = re_set_rspl;
	s->tune_value    = tune_value;
	s->opt_rspl      = opt_rspl_imp;
	s->filter_rspl   = filter_rspl;
	s->get_in_range  = get_in_range;
	s->get_out_range = get_out_range;
	s->get_out_range_points = get_out_range_points;
	s->get_out_scale = get_out_scale;
	s->get_next_touch = get_next_touch;
	s->within_restrictedsize = within_restrictedsize;

	return s;
}

/* Free the rspl and all its contents */
static void free_rspl(rspl *s) {
	int e;

	/* Free everying contained */
	free_data(s);		/* Free any scattered data */
	free_rev(s);		/* Free any reverse lookup data */
	free_gam(s);		/* Free any grid data */
	free_grid(s);		/* Free any grid data */

	/* Free spline interpolation data ~~~~ */

	/* Free structure */
	for (e = 0; e < s->di; e++) {
		if (s->g.ipos[e] != NULL)
			free(s->g.ipos[e]);
	}
	if (s->g.hi != s->g.a_hi) {
		free(s->g.hi);
		free(s->g.fhi);
	}
	free((void *) s);
}

/* ======================================================== */
/* Allocate rspl grid data, and initialise grid associated stuff */
void
alloc_grid(rspl *s) {
	int di = s->di, fdi = s->fdi;
	int e,g,i;
	int gno;				/* Number of points in grid */
	ECOUNT(gc, MXDIDO, di, 0, s->g.res, 0);/* coordinates */
	float *gp;				/* Grid point pointer */

#ifdef DEBUG
	fprintf(stderr,"rspl allocating grid res %s\n",icmPiv(di, s->g.res));
#endif

	/* Compute total number of elements in the grid */
	for (gno = 1, e = 0; e < di; gno *= s->g.res[e], e++)
		;
	s->g.no = gno;

	s->g.pss = fdi+G_XTRA;	/* float for each output value + nme + flags */

	/* Compute index coordinate increments into linear grid for each dimension */
	/* ie. 1, gres, gres^2, gres^3 */
	for (s->g.ci[0] = 1, e = 1; e < di; e++)
		s->g.ci[e]  = s->g.ci[e-1] * s->g.res[e-1];		/* In grid points */
	for (e = 0; e < di; e++)
		s->g.fci[e] = s->g.ci[e] * s->g.pss;	/* In floats */

	/* Compute index offsets from base of cube to other corners. */
	for (s->g.hi[0] = e = 0, g = 1; e < di; g *= 2, e++) {
		for (i = 0; i < g; i++)
			s->g.hi[g+i] = s->g.hi[i] + s->g.ci[e];		/* In grid points */
	}
	/* same as hi, but in floats */
	for (i = 0; i < (1 << di); i++)
		s->g.fhi[i] = s->g.hi[i] * s->g.pss;	/* In floats */
	
	/* Allocate space for grid */
	if ((s->g.alloc = (float *) malloc(sizeof(float) * gno * s->g.pss)) == NULL)
		error("rspl malloc failed - grid points");
	s->g.a = s->g.alloc + G_XTRA;	/* make -1 be nme, and -2 be (unsigned int) flags */

	/* Set initial value of cell touch count */
	s->g.touch = 0;

	/* Init near edge flags, and touched flag */
	EC_INIT(gc);
	for (i = 0, gp = s->g.a; !EC_DONE(gc); i++, gp += s->g.pss) {
		gp[-1] = L_UNINIT;	/* Init Ink limit function value to -1e38 */
		I_FL(gp);			/* Init all flags to zero */
		for (e = 0; e < di; e++) {
			int e1,e2;
			e1 = gc[e];				/* Dist to bottom edge */
			e2 = (s->g.res[e]-1) - gc[e];	/* Dist to top edge */
			if (e2 < e1) {			/* Top edge is closer */
				if (e2 > 2)
					e2 = 2;			/* Max dist = 2 */
				S_FL(gp,e,e2);		/* Set flag value */
			} else {				/* Bot edge is closer */
				if (e1 > 2)
					e1 = 2;				/* Max dist = 2 */
				S_FL(gp,e,e1 | 0x4);	/* Set flag value */
			}
		}
		TOUCHF(gp) = 0;

		EC_INC(gc);
	}
	s->g.limitv_cached = 0;		/* No limit values are current cached */
}

/* Init grid related elements of rspl */
static void
init_grid(rspl *s) {
	s->g.alloc = NULL;
}

/* Free the grid allocation */
static void
free_grid(rspl *s) {
	if (s->g.alloc != NULL)
		free((void *)s->g.alloc);
}

/* ============================================ */
/* Return the range of possible input values that the grid can represent */
static void
get_in_range(
rspl *s,					/* Grid to search */
double *min, double *max	/* Return min/max values */
) {
	int e;
	for (e = 0; e < s->di; e++) {
		min[e] = s->g.l[e];
		max[e] = s->g.h[e];
	}
}

/* ============================================ */
/* Discover the range of possible output values */
static void
get_out_range(
rspl *s,					/* Grid to search */
double *min, double *max	/* Return min/max values */
) {
	float *gp,*ep;		/* Grid pointer */
	int f;

	if (s->g.fminmax_valid == 0) {	/* Not valid, so compute it */
		for (f = 0; f < s->fdi; f++) {
			s->g.fmin[f] = 1e30;
			s->g.fmax[f] = -1e30;
			s->g.fminx[f] = -1;
			s->g.fmaxx[f] = -1;
		}
	
		/* Scan the Grid points for min/max values */
		for (gp = s->g.a, ep = s->g.a + s->g.no * s->g.pss; gp < ep; gp += s->g.pss) {
			for (f = 0; f < s->fdi; f++) {
				if (s->g.fmin[f] > gp[f]) {
					 s->g.fmin[f] = gp[f];
					 s->g.fminx[f] = (gp - s->g.a)/s->g.pss;
				}
				if (s->g.fmax[f] < gp[f]) {
					 s->g.fmax[f] = gp[f];
					 s->g.fmaxx[f] = (gp - s->g.a)/s->g.pss;
				}
			}
		}

		/* Compute overall output scale */
		for (s->g.fscale = 0.0, f = 0; f < s->fdi; f++) {
			double tt = s->g.fmax[f] - s->g.fmin[f];
			s->g.fscale += tt * tt;
		}
		s->g.fscale = sqrt(s->g.fscale);
		s->g.fminmax_valid = 1;		/* Now is valid */
	}
	for (f = 0; f < s->fdi; f++) {
		if (min != NULL)
			min[f] = s->g.fmin[f];
		if (max != NULL)
			max[f] = s->g.fmax[f];
	}
}

/* ============================================ */
/* return the grid index of the grid values at the min & max output values */
static void get_out_range_points(rspl *s, int *minp, int *maxp) {
	int f;

	if (s->g.fminmax_valid == 0) /* Not valid, so compute it */
		get_out_range(s, NULL, NULL);

	for (f = 0; f < s->fdi; f++) {
		if (minp != NULL)
			minp[f] = s->g.fminx[f];
		if (maxp != NULL)
			maxp[f] = s->g.fmaxx[f];
	}
}

/* ============================================ */
/* Discover the csale of the output values */
static double
get_out_scale(rspl *s) {

	if (s->g.fminmax_valid == 0) /* Not valid, so compute it */
		get_out_range(s, NULL, NULL);

	return s->g.fscale;
}

/* ============================================ */
/* Return the next touched flag count value. */
/* Whenever this rolls over, all the flags in the grid array will be reset. */
/*                                                                       */
/* The touch flag is a way of some grid accessor (ie. rev()) making sure */
/* that it doesn't access cells more than once for a particular operation, */
/* without sorting a list of items to be accessed, or having to reset a */
/* binary flag on all the cells for each operation. */
/* If the value of a cells TOUCHF() is less than s->g.touch, then it hasn't */
/* been accessed yet. Once the cell has been acessed, then TOUCHF() should */
/* be set to s->g.touch. After 2^32 operations, the cell touch flags will */
/* have been set to values between 0 and 2^32-1, so it is time to reset */
/* all the flags. For that reason, the following method should be used */
/* to get the next touch generation value at the start of each operation. */
static unsigned int
get_next_touch(
rspl *s
) {
	unsigned int tg;
	float *gp,*ep;		/* Grid pointer */

	if ((tg = ++s->g.touch) == 0) {

		/* We have to reset all the cell flags to zero before we roll over */
		for (gp = s->g.a, ep = s->g.a + s->g.no * s->g.pss; gp < ep; gp += s->g.pss) {
			TOUCHF(gp) = 0;
		}
		tg = ++s->g.touch;		/* return 1 */
	}
	return tg;
}

/* ============================================ */
/* Return non-zero if this rspl can be */
/* used with Restricted Size functions. */
static int within_restrictedsize(
rspl *s
) {
	if (s->di <= MXRI && s->fdi <= MXRO)
		return 1;
	return 0;
}

/* ============================================ */
/* Do a forward interpolation using an simplex interpolation method. */
/* Return 0 if OK, 1 if input was clipped to grid */
/* Return 0 on success, 1 if clipping occured, 2 on other error */
// ~~999
//int rspldb = 0;

static int interp_rspl_sx(
rspl *s,
co *p			/* Input value and returned function value */
) {
	int e, di  = s->di;
	int f, fdi = s->fdi;
	double we[MXDI];		/* Coordinate offset within the grid cell */
	int    si[MXDI];		/* we[] Sort index, [0] = smallest */
	float *gp;				/* Pointer to grid cube base */
	int rv = 0;				/* Register clip */

	/* We are using a simplex (ie. tetrahedral for 3D input) interpolation. */

	DEBLU(("In %s\n", icmPdv(di, p->p)));

	/* Figure out which grid cell the point falls into */
	{
		gp = s->g.a;					/* Base of grid array */
		for (e = 0; e < di; e++) {
			int gres_1 = s->g.res[e]-1;
			double pe, t;
			int mi;
			pe = p->p[e];
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
			gp += mi * s->g.fci[e];		/* Add Index offset for grid cube base in dimen */
			we[e] = t - (double)mi;		/* 1.0 - weight */
//if (rspldb && di == 3) printf("~1 e = %d, ix = %d, we = %f\n", e, mi, we[e]);
		}
		DEBLU(("ix %d, we %s\n", (gp - s->g.a)/s->g.pss, icmPdv(di, p->p)));
	}

	/* Do selection sort on coordinates */
	{
		for (e = 0; e < di; e++)
			si[e] = e;						/* Initial unsorted indexes */
		for (e = 0; e < (di-1); e++) {
			double cosn;
			cosn = we[si[e]];				/* Current smallest value */
			for (f = e+1; f < di; f++) {	/* Check against rest */
				int tt;
				tt = si[f];
				if (cosn > we[tt]) {
					si[f] = si[e]; 			/* Exchange */
					si[e] = tt;
					cosn = we[tt];
				}
			}
		}
	}
	DEBLU(("si[] = %s\n", icmPiv(di, si)));

	/* Now compute the weightings, simplex vertices and output values */
	{
		double w;		/* Current vertex weight */

		w = 1.0 - we[si[di-1]];		/* Vertex at base of cell */
		for (f = 0; f < fdi; f++)
			p->v[f] = w * gp[f];

		DEBLU(("ix %d: w %f * val %s\n", (gp - s->g.a)/s->g.pss, w, icmPfv(fdi,gp)));

		for (e = di-1; e > 0; e--) {		/* Middle verticies */
			w = we[si[e]] - we[si[e-1]];
			gp += s->g.fci[si[e]];			/* Move to top of cell in next largest dimension */
			for (f = 0; f < fdi; f++)
				p->v[f] += w * gp[f];
			DEBLU(("ix %d: w %f * val %s\n", (gp - s->g.a)/s->g.pss, w, icmPfv(fdi,gp)));
		}

		w = we[si[0]];
		gp += s->g.fci[si[0]];		/* Far corner from base of cell */
		for (f = 0; f < fdi; f++)
			p->v[f] += w * gp[f];
		DEBLU(("ix %d: w %f * val %s\n", (gp - s->g.a)/s->g.pss, w, icmPfv(fdi,gp)));
		DEBLU(("Outval  %s\n", icmPdv(fdi, p->v)));
	}
	return rv;
}

/* ============================================ */
/* Do forward (partial) interpolation to allow input & output curves to be applied, */
/* and allow input delta E to be estimated from output delta E. */
/* Call with input value in p1[0].p[], */
/* In order smallest to largest weight: */
/* Return di+1 vertex values in p1[]].v[] and */
/* 0-1 sub-cell weight values as (p1[].p[0] - p1[].p[1]). */
/* Optionally in input channel order: */
/* Returns di+1 partial derivatives + base value in p2[].v[], */
/* with matching weight values for each in p2[].p[0] (last weight = 1)*/
/* Return 0 if OK, 1 if input was clipped to grid */
static int part_interp_rspl_sx(
struct _rspl *s,	/* this */
co *p1,
co *p2			/* optional - return partial derivatives for each input channel */
) {
	int e, di  = s->di;
	int f, fdi = s->fdi;
	double we[MXDI];		/* Coordinate offset within the grid cell */
	int    si[MXDI];		/* we[] Sort index, [0] = smallest */
	float *gp;				/* Pointer to grid cube base */
	int rv = 0;				/* Register clip */

	/* We are using a simplex (ie. tetrahedral for 3D input) interpolation. */

	/* Figure out which grid cell the point falls into */
	{
		gp = s->g.a;					/* Base of grid array */
		for (e = 0; e < di; e++) {
			int gres_1 = s->g.res[e]-1;
			double pe, t;
			int mi;
			pe = p1[0].p[e];
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
			gp += mi * s->g.fci[e];		/* Add Index offset for grid cube base in dimen */
			we[e] = t - (double)mi;		/* 1.0 - weight */
		}
	}

	/* Do selection sort on coordinates */
	{
		for (e = 0; e < di; e++)
			si[e] = e;						/* Initial unsorted indexes */
		for (e = 0; e < (di-1); e++) {
			double cosn;
			cosn = we[si[e]];				/* Current smallest value */
			for (f = e+1; f < di; f++) {	/* Check against rest */
				int tt;
				tt = si[f];
				if (cosn > we[tt]) {
					si[f] = si[e]; 			/* Exchange */
					si[e] = tt;
					cosn = we[tt];
				}
			}
		}
	}
	/* Now compute the vertex values that correspond */
	/* to the input faction weightings + fixed value */
	/* Scale the slopes + weights to make slopes */
	/* valid as partial derivative of input values */
	{

		p1[di].p[0] = 1.0;
		p1[di].p[1] = we[si[di-1]];		/* Vertex at base of cell */
		for (f = 0; f < fdi; f++)
			p1[di].v[f] = gp[f];

		if (p2 != NULL) {
			for (f = 0; f < fdi; f++)
				p2[di].v[f] = gp[f];	/* Constant term @ vertex base */
			p2[di].p[0] = 1.0;
		}
		
		for (e = di-1; e >= 0; e--) {	/* Middle verticies to far vertex from base */
			int ee = si[e];
			float *lgp = gp;			/* Last gp[] */

			gp += s->g.fci[ee];			/* Move to top of cell in next largest dimension */

			p1[e].p[0] = we[si[e]];
			p1[e].p[1] = e > 0 ? we[si[e-1]] : 0.0;
			for (f = 0; f < fdi; f++)
				p1[e].v[f] = gp[f];

			if (p2 != NULL) {
				for (f = 0; f < fdi; f++)
					p2[ee].v[f] = (gp[f] - lgp[f]) / s->g.w[ee];
				p2[ee].p[0] = we[ee] * s->g.w[ee];
			}
		}
	}
	return rv;
}

#ifdef NEVER
/* Test out part_interp_rspl_sx() */
/* Designed to test with a CMYK->Lab lookup */
static int interp_rspl_sx(
rspl *s,
co *p			/* Input value and returned function value */
) {
	int rv, rv2;
	int e, f, m;
	co p1[MXDI+1];
	co p2[MXDI+1];
	co p3;
	double v1[MXDO];
	double v2[MXDO];

	for (e = 0; e < s->di; e++) {
		p1[0].p[e] = p->p[e];
		p3.p[e] = p->p[e];
	}
	
	rv = _interp_rspl_sx(s, p);

	if ((s->di != 4 || s->fdi != 3)
	 && (s->di != 3 || s->fdi != 4))
		return rv;

	rv2 = part_interp_rspl_sx(s, p1, p2);

	/* Check interpolation values returned in p1 and p2 form */
	for (f = 0; f < s->fdi; f++)
		v1[f] = v2[f] = 0.0;

	for (e = 0; e <= s->di; e++) {
		for (f = 0; f < s->fdi; f++) {
			/* We could converts p1[].p[0] and p1[].p[1] through sub curve lookup, */
			/* and p1[].v[] though inverse output curve lookup, */
			/* then convert v1[] through output curve lookup. */
			v1[f] += p1[e].v[f] * (p1[e].p[0] - p1[e].p[1]);

			/* v2 is using base + partial derivatives */
			v2[f] += p2[e].v[f] * p2[e].p[0];
		}
	}

	if (s->di == 4) {
		printf("~1 %f %f %f %f ->\n",p->p[0], p->p[1], p->p[2], p->p[3]);
		printf("~1 ref    %d -> %f %f %f\n", rv, p->v[0], p->v[1], p->v[2]);
		printf("~1 check1 %d -> %f %f %f\n", rv2, v1[0], v1[1], v1[2]);
		printf("~1 check2 %d -> %f %f %f\n", rv2, v2[0], v2[1], v2[2]);
	} else {
		printf("~1 %f %f %f ->\n",p->p[0], p->p[1], p->p[2]);
		printf("~1 ref    %d -> %f %f %f %f\n", rv, p->v[0], p->v[1], p->v[2], p->v[3]);
		printf("~1 check1 %d -> %f %f %f %f\n", rv2, v1[0], v1[1], v1[2], v1[3]);
		printf("~1 check2 %d -> %f %f %f %f\n", rv2, v2[0], v2[1], v2[2], v2[3]);
	}

	/* Check partial derivs in p2 */
	for (m = 0; m < s->di; m++) {

		p3.p[m] += 1e-5;

		_interp_rspl_sx(s, &p3);
		for (f = 0; f < s->fdi; f++)
			p3.v[f] = (p3.v[f] - p->v[f])/1e-5;
	
		if (s->di == 4) {
			printf("~1 deriv %d:\n", m);
			printf("~1 ref del   %f %f %f\n", p3.v[0], p3.v[1], p3.v[2]);
			printf("~1 check del %f %f %f\n", p2[m].v[0], p2[m].v[1], p2[m].v[2]);
		} else {
			printf("~1 deriv %d:\n", m);
			printf("~1 ref del   %f %f %f %f\n", p3.v[0], p3.v[1], p3.v[2], p3.v[3]);
			printf("~1 check del %f %f %f %f\n", p2[m].v[0], p2[m].v[1], p2[m].v[2], p2[m].v[3]);
		}

		p3.p[m] -= 1e-5;
	}

	return rv;
}
#endif

/* ============================================ */

#ifdef USING_INTERP_NL
/* Alternate, not currently used */
/* Do a forward interpolation using an n-linear method. */
/* Return 0 if OK, 1 if input was clipped to grid */
/* Alternative to interp_rspl_sx */
static int interp_rspl_nl(
rspl *s,
co *p			/* Input value and returned function value */
) {
	int e, di  = s->di;
	int f, fdi = s->fdi;
	double we[MXDI];		/* 1.0 - Weight in each dimension */
	double *gw;				/* weight for each grid cube corner */
	double a_gw[DEF2MXDI];	/* Default space for gw */
	float *gp;				/* Pointer to grid cube base */
	int rv = 0;
	
	gw = a_gw;
	if ((1 << di) > DEF2MXDI) {
		if ((gw = (double *) malloc(sizeof(double) * (1 << di))) == NULL)
			error("rspl malloc failed - interp_rspl_nl");
	}
	/* Figure out which grid cell the point falls into */
	{
		gp = s->g.a;					/* Base of grid array */
		for (e = 0; e < di; e++) {
			int gres_1 = s->g.res[e]-1;
			double pe, t;
			int mi;
			pe = p->p[e];
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
			gp += mi * s->g.fci[e];		/* Add Index offset for grid cube base in dimen */
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

	/* Now compute the output values */
	{
		int i;
		double w = gw[0];
		float *d = gp + s->g.fhi[0];
		for (f = 0; f < fdi; f++)			/* Base of cube */
			p->v[f] = w * d[f];

		for (i = 1; i < (1 << di); i++) {	/* For all other corners of cube */
			double w = gw[i];				/* Strength reduce */
			float *d = gp + s->g.fhi[i];
			for (f = 0; f < fdi; f++)
				p->v[f] += w * d[f];
		}
		
	}

	if (gw != a_gw)
		free(gw);

	return rv;
}
#endif /* USING_INTERP_NL */

/* ============================================ */
/* Non-mono calculations */
/* Compute non-monotonicity factor for each grid point, and */
/* return non-zero if the overall grid is monotonic. */
/* (Note that this is not a true non-monotonicity test. */
/*  A true test has to deal with PCS combination values.) */
int
is_mono(
rspl *s
) {
	int f;
	int di  = s->di;
	int fdi = s->fdi;
	int *fci = s->g.fci;	/* Strength reduction */
	float *gp, *ep;
	double mcinc = MCINC/(s->g.mres-1);	/* Scaled version of MCINC */
	double min = 1e20;		/* Minimum clearance found */

	/* Find the minimum step between grid points */
	for (gp = s->g.a, ep = s->g.a + s->g.no * s->g.pss; gp < ep; gp += s->g.pss) {
		for (f = 0; f < fdi; f++) {
			int e;
			double e1,e2;		/* Smallest/largest surrounting point */
			double u;			/* Current output value we are considering */
			double ce;			/* nm error */
	
			/* Find smallest and largest surrounding points */
			/* In +/- 1 dimension directions */
			e1 = 1e20; e2 = -1e20;
			for (e = 0; e < di; e++) {
				int dof;	/* Double offset */
				float vv;

				if ((G_FL(gp,e) & 3) < 1)
					break;		/* Skip to next grid point if on edge */
				dof = fci[e];
				vv = gp[f + dof];
				if (vv < e1)
					e1 = vv;
				if (vv > e2)
					e2 = vv;
				vv = gp[f - dof];
				if (vv < e1)
					e1 = vv;
				if (vv > e2)
					e2 = vv;
			}
			if (e < di)		/* We broke because we are on the edge */
				continue;
	
			u = gp[f];

			e1 = u - e1;
			e2 = e2 - u;
			ce = (e1 < e2 ? e1 : e2);		/* Smallest step */

			if (ce < min)					/* Current smallest step */
				min = ce;
		}
	}
//if (min < mcinc) printf("~1 is_mono failed by %e < %e\n",min,mcinc);
	return min < mcinc;
}

/* ============================================ */
/* Initialize the grid from a provided function. By default the grid */
/* values are set to exactly the value returned by func(), unless the */
/* RSPL_SET_APXLS flag is set, in which case an attempt is made to have */
/* the grid points represent a least squares aproximation to the underlying */
/* surface, by using extra samples in the middle of grid cells. */
/* RSPL_SET_APXLS tends to improve the fit to the underlying function. */
/* Grid index values are supplied "under" in[] at *((int*)&iv[-e-1]), */
/* but if RSPL_SET_APXLS is set, the grid index will be the base of */
/* the cell the center point is sampled from every second sample. */
/* Return non-monotonic status */
static int set_rspl(
	struct _rspl *s,/* this */
	int flags,		/* Combination of flags */
	void *cbctx,	/* Opaque function context */
	void (*func)(void *cbctx, double *out, double *in),		/* Function to set from */
	datai glow,		/* Grid low scale - will expand to enclose data, NULL = default 0.0 */
	datai ghigh,	/* Grid high scale - will expand to enclose data, NULL = default 1.0 */
	int gres[MXDI],	/* Spline grid resolution for each dimension */
	datao vlow,		/* Data value low normalize, NULL = default 0.0 */
	datao vhigh		/* Data value high normalize - NULL = default 1.0 */
) {
	int e, f, j;
	rpsh counter;		/* Pseudo-hilbert counter */
	int gc[MXDI];		/* Grid index value */
	float *gp;			/* Pointer to grid data */
	float *cc = NULL;			/* Pointer to cell center data */
	double _iv[2 * MXDI], *iv = &_iv[MXDI];	/* Real index value/table value */
	double ov[MXDO];

	if (flags & RSPL_VERBOSE)	/* Turn on progress messages to stdout */
		s->verbose = 1;
	if (flags & RSPL_NOVERBOSE)	/* Turn off progress messages to stdout */
		s->verbose = 0;

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

		/* compute width of each grid cell */
		s->g.w[e] = (s->g.h[e] - s->g.l[e])/(double)(s->g.res[e]-1);

		/* ?? Should h be recomputed as (l + gres-1) * w ?? */
	}
	s->g.mres = pow(s->g.mres, 1.0/e);		/* geometric mean */

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

	/* Allocate the grid data */
	alloc_grid(s);

	/* Allocate space for cell center value lookup */
	if (flags & RSPL_SET_APXLS) {
		if ((cc = (float *)malloc(sizeof(float) * s->g.no * s->fdi)) == NULL)
			error("rspl malloc failed - center cell points");
	}

	/* Reset output min/max */
	for (f = 0; f < s->fdi; f++) {
		s->g.fmin[f] = 1e30;
		s->g.fmax[f] = -1e30;
		s->g.fminx[f] = -1;
		s->g.fmaxx[f] = -1;
	}

	/* Set the grid points value from the provided function */

	/* To make this clut function cache friendly, we use the pseudo-hilbert */
	/* count sequence. This keeps each point close to the last in the */
	/* multi-dimensional space. */ 
	rpsh_init(&counter, s->di, (unsigned int *)gres, gc);	/* Initialise counter */
	for (;;) {

		/* Compute grid pointer and input sample values */
		gp = s->g.a;	/* Base of grid data */
		for (e = 0; e < s->di; e++) { 				/* Input tables */
			gp += gc[e] * s->g.fci[e];				/* Grid value pointer */
			iv[e] = s->g.l[e] + gc[e] * s->g.w[e];	/* Input sample values */
			*((int *)&iv[-e-1]) = gc[e];			/* Trick to supply grid index in iv[] */
		}

		/* Apply incolor -> outcolor function we want to represent */
		func(cbctx, ov, iv);

		for (f = 0; f < s->fdi; f++) { 	/* Output chans */
			gp[f] = (float)ov[f];		/* Set output value */
			if (s->g.fmin[f] > gp[f]) {
				 s->g.fmin[f] = gp[f];
				 s->g.fminx[f] = (gp - s->g.a)/s->g.pss;
			}
			if (s->g.fmax[f] < gp[f]) {
				 s->g.fmax[f] = gp[f];
				 s->g.fmaxx[f] = (gp - s->g.a)/s->g.pss;
			}
		}

		/* For RSPL_SET_APXLS, get the center of the cell values as well. */
		if (cc != NULL) {
			float *ccp;

			ccp = cc;
			for (e = 0; e < s->di; e++) { 				/* Input tables */
				if (gc[e] >=  (gres[e]-1))
					break;								/* No center for outer row */
				iv[e] = s->g.l[e] + (gc[e] + 0.5) * s->g.w[e];	/* Input sample values */
				ccp += gc[e] * s->g.ci[e] * s->fdi;		/* cc location */
			}
	
			if (e >= s->di) {			/* Not outer row */
				/* Apply incolor -> outcolor function we want to represent */
				func(cbctx, ov, iv);
	
				for (f = 0; f < s->fdi; f++) { 	/* Output chans */
					ccp[f] = (float)ov[f];		/* Set output value */
				}
			}
		}

		/* Increment counter */
		if (rpsh_inc(&counter, gc))
			break;
	}

	/* For RSPL_SET_APXLS, deal with cell center value, aproximate least squares adjustment */
	if (cc != NULL) {
		int ee;
		double cw = 1.0/(double)(1 << s->di);		/* Weight for each cube corner */
		float *ccp;

		for (e = 0; e < s->di; e++)
			gc[e] = 0;	/* init coords */

		/* Compute linear interpolated error to actual cell center value */
		for (ee = 0; ee < s->di;) {

			gp = s->g.a;	/* Base of grid data */
			ccp = cc;		/* Base of center data */
			for (e = 0; e < s->di; e++) { 				/* Input tables */
				gp += gc[e] * s->g.fci[e];				/* Grid value pointer */
				ccp += gc[e] * s->g.ci[e] * s->fdi;		/* cc location */
			}

			for (f = 0; f < s->fdi; f++) { 	/* Output chans */
				double sum = 0.0;
		
				for (j = 0; j < (1 << s->di); j++) /* For corners of cube */
					sum += (gp + s->g.fhi[j])[f];
				sum *= cw;			/* Interpolated value */
				ccp[f] -= sum;		/* Correction to actual value */

				/* Average half the error to cube corners */
				ccp[f] *= 0.5 * cw;	/* Distribution fraction */
			}

			/* Increment coord */
			for (ee = 0; ee < s->di; ee++) {
				if (++gc[ee] < (gres[ee]-1))		/* Don't go through upper edge */
					break;	/* No carry */
				gc[ee] = 0;
			}
		}
		
		for (e = 0; e < s->di; e++)
			gc[e] = 0;	/* init coords */

		/* Distribute the center error to the cell corners */
		for (ee = 0; ee < s->di;) {

			gp = s->g.a;	/* Base of grid data */
			ccp = cc;		/* Base of center data */
			for (e = 0; e < s->di; e++) { 				/* Input tables */
				gp += gc[e] * s->g.fci[e];				/* Grid value pointer */
				ccp += gc[e] * s->g.ci[e] * s->fdi;		/* cc location */
			}

			for (j = 0; j < (1 << s->di); j++) { /* For corners of cube */
				double sc = 1.0;		/* Scale factor for non-edge nodes */

				/* Don't distribute error to edge nodes since there may */
				/* an expectation that they have precicely set values */
				/* (ie. white and black points) */
				for (e = 0; e < s->di; e++) {
					if ((gc[e] == 0 && (j & (1 << e)) == 0)
					 || (gc[e] == ((gres[e]-2)) && (j & (1 << e)) != 0))
						sc *= 0.0;
				}

				for (f = 0; f < s->fdi; f++) { 		/* Output chans */
					double vv;
					vv = (gp + s->g.fhi[j])[f];		/* Current value */
					vv += sc * cc[f];				/* Correction */
					(gp + s->g.fhi[j])[f] = vv;
					if (s->g.fmin[f] > vv) {
						s->g.fmin[f] = vv;
						s->g.fminx[f] = (gp + s->g.fhi[j] - s->g.a)/s->g.pss;
					}
					if (s->g.fmax[f] < vv) {
						s->g.fmax[f] = vv;
						s->g.fmaxx[f] = (gp + s->g.fhi[j] - s->g.a)/s->g.pss;
					}
				}
			}

			/* Increment coord */
			for (ee = 0; ee < s->di; ee++) {
				if (++gc[ee] < (gres[ee]-1))		/* Don't go through upper edge */
					break;	/* No carry */
				gc[ee] = 0;
			}
		}

		free((void *)cc);
	}

	/* Compute overall output scale */
	for (s->g.fscale = 0.0, f = 0; f < s->fdi; f++) {
		double tt = s->g.fmax[f] - s->g.fmin[f];
		s->g.fscale += tt * tt;
	}
	s->g.fscale = sqrt(s->g.fscale);

	s->g.fminmax_valid = 1;		/* Now is valid */

	/* Return non-mono check */
	return is_mono(s);
}

/* ============================================ */
/* Scan or change each grid point in the rspl. */
static int scan_set_rspl(
struct _rspl *s,	/* this */
int flags,		/* Combination of flags */
void *cbctx,	/* Opaque function context */
void (*func)(void *cbntx, double *out, double *in), /* Function to get/set from */
int change		/* Flag - nz means change values, 0 means scan values */
) {
	int e, f;
	rpsh counter;		/* Pseudo-hilbert counter */
	int gc[MXDI];		/* Grid index value */
	float *gp;			/* Pointer to grid data */
	double _iv[2 * MXDI], *iv = &_iv[MXDI];	/* Real index value/table value */
	double ov[MXDO];

	if (flags & RSPL_VERBOSE)	/* Turn on progress messages to stdout */
		s->verbose = 1;
	if (flags & RSPL_NOVERBOSE)	/* Turn off progress messages to stdout */
		s->verbose = 0;

	if (change) {
		/* Reset output min/max */
		for (f = 0; f < s->fdi; f++) {
			s->g.fmin[f] = 1e30;
			s->g.fmax[f] = -1e30;
			s->g.fminx[f] = -1;
			s->g.fmaxx[f] = -1;
		}
	}

	/* Set the grid points value from the provided function */
	/* Give the function both the grid position and the existing output values */

	/* To make this clut function cache friendly, we use the pseudo-hilbert */
	/* count sequence. This keeps each point close to the last in the */
	/* multi-dimensional space. */ 
	rpsh_init(&counter, s->di, (unsigned int *)s->g.res, gc);	/* Initialise counter */
	for (;;) {

		/* Compute grid pointer and input sample values */
		gp = s->g.a;	/* Base of grid data */
		for (e = 0; e < s->di; e++) { 				/* Input tables */
			gp += s->g.fci[e] * gc[e];				/* Grid value pointer */
			iv[e] = s->g.l[e] + gc[e] * s->g.w[e];	/* Input sample values */
			*((int *)&iv[-e-1]) = gc[e];			/* Trick to supply grid index in iv[] */
		}

		for (f = 0; f < s->fdi; f++) 	/* Output chans */
			ov[f] = gp[f];

		/* Let function scan the input and output values, or */
		/* Apply incolor -> outcolor, or oldoutcolor->outcolor function we want to represent */
		func(cbctx, ov, iv);

		if (change) {		/* Put new output values back */
			for (f = 0; f < s->fdi; f++) { 	/* Output chans */
				gp[f] = (float)ov[f];
				if (s->g.fmin[f] > gp[f]) {
					 s->g.fmin[f] = gp[f];
					 s->g.fminx[f] = (gp - s->g.a)/s->g.pss;
				}
				if (s->g.fmax[f] < gp[f]) {
					 s->g.fmax[f] = gp[f];
					 s->g.fmaxx[f] = (gp - s->g.a)/s->g.pss;
				}
			}
		}

		/* Increment counter */
		if (rpsh_inc(&counter, gc))
			break;
	}

	if (change == 0) {
		return 0;
	}

	/* Compute overall output scale */
	for (s->g.fscale = 0.0, f = 0; f < s->fdi; f++) {
		double tt = s->g.fmax[f] - s->g.fmin[f];
		s->g.fscale += tt * tt;
	}
	s->g.fscale = sqrt(s->g.fscale);

	s->g.fminmax_valid = 1;		/* Now is valid */

	/* Invalidate various things */
	free_data(s);		/* Free any scattered data */
	free_rev(s);		/* Free any reverse lookup data */

	/* Return non-mono check */
	return is_mono(s);
}

/* Re-initialize the grid from existing grid values, and the provided function */
/* Grid index values are supplied "under" in[] at *((int*)&iv[-e-1]) */
/* Return non-monotonic status. We assume that the ouput scale factors don't change. */
static int re_set_rspl(
struct _rspl *s,	/* this */
int flags,		/* Combination of flags */
void *cbctx,	/* Opaque function context */
void (*func)(void *cbntx, double *out, double *in) /* Function to set from */
) {
	return scan_set_rspl(s, flags, cbctx, func, 1);
}

/* Scan the rspl grid point locations and values. Grid index values are */
/* supplied "under" in[] *((int*)&iv[-e-1]) */
static void scan_rspl(
struct _rspl *s,	/* this */
int flags,		/* Combination of flags */
void *cbctx,	/* Opaque function context */
void (*func)(void *cbntx, double *out, double *in) /* Function to get from */
) {
	scan_set_rspl(s, flags, cbctx, func, 0);
}


/* ============================================ */
/* Tune a single value for simplex interpolation. */
/* Return 0 on success, 1 if input clipping occured, 2 if output clipping occured */
static int tune_value (
struct _rspl *s,				/* Pointer to Lut object */
co *p							/* Target value */
) {
	int e, di  = s->di;
	int f, fdi = s->fdi;
	double we[MXDI];		/* Coordinate offset within the grid cell */
	int    si[MXDI];		/* we[] Sort index, [0] = smallest */
	float *gp;				/* Pointer to grid cube base */
	int rv = 0;				/* Register clip */

	/* We are using a simplex (ie. tetrahedral for 3D input) interpolation. */

	DEBLU(("In %s\n", icmPdv(di, p->p)));

	/* Figure out which grid cell the point falls into */
	{
		gp = s->g.a;					/* Base of grid array */
		for (e = 0; e < di; e++) {
			int gres_1 = s->g.res[e]-1;
			double pe, t;
			int mi;
			pe = p->p[e];
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
			gp += mi * s->g.fci[e];		/* Add Index offset for grid cube base in dimen */
			we[e] = t - (double)mi;		/* 1.0 - weight */
//if (rspldb && di == 3) printf("~1 e = %d, ix = %d, we = %f\n", e, mi, we[e]);
		}
		DEBLU(("ix %d, we %s\n", (gp - s->g.a)/s->g.pss, icmPdv(di, p->p)));
	}

	/* Do selection sort on coordinates */
	{
		for (e = 0; e < di; e++)
			si[e] = e;						/* Initial unsorted indexes */
		for (e = 0; e < (di-1); e++) {
			double cosn;
			cosn = we[si[e]];				/* Current smallest value */
			for (f = e+1; f < di; f++) {	/* Check against rest */
				int tt;
				tt = si[f];
				if (cosn > we[tt]) {
					si[f] = si[e]; 			/* Exchange */
					si[e] = tt;
					cosn = we[tt];
				}
			}
		}
	}
	DEBLU(("si[] = %s\n", icmPiv(di, si)));

	/* Now compute the weightings, simplex vertices and output values */
	{
		double w, ww = 0.0;			/* Current vertex weight, sum of weights squared */
		double cout[MXDO];			/* Current output value */
		float *ogp = gp;			/* Pointer to grid cube base */

		w = 1.0 - we[si[di-1]];				/* Vertex at base of cell */
		ww += w * w;						/* Sum of weights squared */
		for (f = 0; f < fdi; f++)
			cout[f] = w * gp[f];

		DEBLU(("ix %d: w %f * val %s\n", (gp - s->g.a)/s->g.pss, w, icmPfv(fdi,gp)));

		for (e = di-1; e > 0; e--) {		/* Middle verticies */
			w = we[si[e]] - we[si[e-1]];
			ww += w * w;					/* Sum of weights squared */
			gp += s->g.fci[si[e]];			/* Move to top of cell in next largest dimension */
			for (f = 0; f < fdi; f++)
				cout[f] += w * gp[f];
			DEBLU(("ix %d: w %f * val %s\n", (gp - s->g.a)/s->g.pss, w, icmPfv(fdi,gp)));
		}

		w = we[si[0]];
		ww += w * w;					/* Sum of weights squared */
		gp += s->g.fci[si[0]];			/* Far corner from base of cell */
		for (f = 0; f < fdi; f++)
			cout[f] += w * gp[f];
		DEBLU(("ix %d: w %f * val %s\n", (gp - s->g.a)/s->g.pss, w, icmPfv(fdi,gp)));
		DEBLU(("cout  %s\n", icmPdv(fdi, cout)));

		/* We distribute the correction needed in proportion to the */
		/* interpolation weighting, so the biggest correction is to the */
		/* closest vertex. */
		for (f = 0; f < fdi; f++)
			cout[f] = (p->v[f] - cout[f])/ww;	/* Amount to distribute */

		gp = ogp;
		w = 1.0 - we[si[di-1]];		/* Vertex at base of cell */
		for (f = 0; f < fdi; f++) {
			gp[f] += w * cout[f];				/* Apply correction */
			if (gp[f] < s->g.fmin[f]) {
				gp[f] = s->g.fmin[f];
				rv |= 2;
			} else if (gp[f] > s->g.fmax[f]) {
				gp[f] = s->g.fmax[f];
				rv |= 2;
			}
		}

		for (e = di-1; e > 0; e--) {	/* Middle verticies */
			w = we[si[e]] - we[si[e-1]];
			gp += s->g.fci[si[e]];				/* Move to top of cell in next largest dimension */
			for (f = 0; f < fdi; f++) {
				gp[f] += w * cout[f];			/* Apply correction */
				if (gp[f] < s->g.fmin[f]) {
					gp[f] = s->g.fmin[f];
					rv |= 2;
				} else if (gp[f] > s->g.fmax[f]) {
					gp[f] = s->g.fmax[f];
					rv |= 2;
				}
			}
		}

		w = we[si[0]];
		gp += s->g.fci[si[0]];					/* Far corner from base of cell */
		for (f = 0; f < fdi; f++) {
			gp[f] += w * cout[f];				/* Apply correction */
			if (gp[f] < s->g.fmin[f]) {
				gp[f] = s->g.fmin[f];
				rv |= 2;
			} else if (gp[f] > s->g.fmax[f]) {
				gp[f] = s->g.fmax[f];
				rv |= 2;
			}
		}
	}
	return rv;
}

/* ============================================ */
/* Allow the grid values to be filtered. */
/* For each grid value, provide the input value and */
/* pointers to all the output values in a 3^di grid around */
/* the output value. Pointers will be NULL if neigbour is outside */
/* the grid. cvi is the index of the output value. */
/* Grid index values are supplied "under" in[] at *((int*)&iv[-e-1]) */
/* After all the grid values have been done, they will be updated */
/* with their new values. */
static void filter_rspl(
struct _rspl *s,	/* this */
int flags,		/* Combination of flags */
void *cbctx,	/* Opaque function context */
void (*func)(void *cbntx, float **out, double *in, int cvi) /* Function to set from */
) {
	int e, f;
	ECOUNT(gc, MXDIDO, s->di, 0, s->g.res, 0);	/* coordinates */
	DCOUNT(cc, MXDIDO, s->di, -1, -1, 2);	/* Surrounding cube counter */
	float *gp, *ep;		/* Pointer to grid data */
	float *tarry, *tp;	/* Temporary array of values */
	double _iv[2 * MXDI], *iv = &_iv[MXDI];	/* Real index value/table value */
	int cvi;				/* Center value index = 3^di-1)/2 */
	int pow3di = 1;
	float **svals;			/* Pointer to surrounding output values */
	float *a_svals[DEF3MXDI];/* default allocation for svals */

	if (flags & RSPL_VERBOSE)	/* Turn on progress messages to stdout */
		s->verbose = 1;
	if (flags & RSPL_NOVERBOSE)	/* Turn off progress messages to stdout */
		s->verbose = 0;

	/* Allocate svals array */
	svals = a_svals;
	for (e = 0; e < s->di; e++)
		pow3di *= 3;
	if (pow3di > DEF3MXDI) {
		if ((svals = (float **) malloc(sizeof(float *) * pow3di)) == NULL)
			error("rspl malloc failed - filter_rspl");
	}

	/* Compute the center value index */
	for (cvi = 1, e = 0; e < s->di; e++)
		cvi *= 3;
	cvi = (cvi-1)/2;

	/* Allocate a temporary array for the new output values */
	if ((tarry = (float *)malloc(sizeof(float) * s->g.no * s->fdi)) == NULL) {
		if (svals != a_svals)
			free(svals);
		error("rspl malloc failed - filter_rspl array");
	}

	/* Set the grid points value from the provided function */
	/* Give the function both the grid position and the existing output values */
	/* in the 3x3 surrounding grid */
	EC_INIT(gc);
	for (tp = tarry; !EC_DONE(gc); tp += s->fdi) {
		int i;
		
		/* Compute grid pointer and input sample values */
		for (e = 0; e < s->di; e++) {
			iv[e] = s->g.l[e] + gc[e] * s->g.w[e];	/* Input sample values */
			*((int *)&iv[-e-1]) = gc[e];			/* Trick to supply grid index in iv[] */
		}

		/* Set pointers to 3x3 surrounders */
		DC_INIT(cc)
		for (i = 0; !DC_DONE(cc); i++ ) {
			float *sp = s->g.a;

			for (e = 0; e < s->di; e++) { 				/* Input tables */
				int j;
				j = gc[e] + cc[e];
				if (j < 0 || j >= s->g.res[e]) {
					sp = NULL;			/* outside grid */
					break;
				}
				sp += s->g.fci[e] * j;	/* Compute pointer to surrounder */
			}

			svals[i] = sp;
			DC_INC(cc);
		}
		
		for (f = 0; f < s->fdi; f++) 	/* Set default no change new values */
			tp[f] = svals[cvi][f];
		svals[cvi] = tp;				/* Make sure output value goes into temp array */
		
		/* Apply incolor -> outcolor, or oldoutcolor->outcolor function we want to represent */
		func(cbctx, svals, iv, cvi);

		EC_INC(gc);
	}

	/* Reset output min/max */
	for (f = 0; f < s->fdi; f++) {
		s->g.fmin[f] = 1e30;
		s->g.fmax[f] = -1e30;
		s->g.fminx[f] = -1;
		s->g.fmaxx[f] = -1;
	}

	/* Now update all the values */
	for (tp = tarry, gp = s->g.a, ep = s->g.a + s->g.no * s->g.pss;
	     gp < ep; gp += s->g.pss, tp += s->fdi) {

		for (f = 0; f < s->fdi; f++) 	/* Output chans */
			gp[f] = tp[f];

		for (f = 0; f < s->fdi; f++) { 	/* Output chans */
			if (s->g.fmin[f] > gp[f]) {
				 s->g.fmin[f] = gp[f];
				 s->g.fminx[f] = (gp - s->g.a)/s->g.pss;
			}
			if (s->g.fmax[f] < gp[f]) {
				 s->g.fmax[f] = gp[f];
				 s->g.fmaxx[f] = (gp - s->g.a)/s->g.pss;
			}
		}
	}

	/* Compute overall output scale */
	for (s->g.fscale = 0.0, f = 0; f < s->fdi; f++) {
		double tt = s->g.fmax[f] - s->g.fmin[f];
		s->g.fscale += tt * tt;
	}
	s->g.fscale = sqrt(s->g.fscale);

	s->g.fminmax_valid = 1;		/* Now is valid */

	if (svals != a_svals)
		free(svals);
	free(tarry);

	/* Invalidate various things */
	free_data(s);		/* Free any scattered data */
	free_rev(s);		/* Free any reverse lookup data */
}


/* =============================================== */
/* Utility function */
/* Pseudo - Hilbert count sequencer */

/* Initialise, returns total usable count */
unsigned rpsh_init(
rpsh         *p,	/* Pointer to structure to initialise */
int           di,	/* Dimensionality */
unsigned int *res,	/* Size per coordinate */
int           co[]	/* Coordinates to initialise (May be NULL) */
) {
	int e;

	p->di = di;
	p->tbits = 0;
	for (e = 0; e < di; e++) {
		p->res[e] = res[e];

		/* Compute bits */
		for (p->bits[e] = 0; (1u << p->bits[e]) < res[e]; p->bits[e]++)
			;
		p->tbits += p->bits[e];
	}

	/* Compute the total count mask */
	p->tmask = ((((unsigned)1) << p->tbits)-1);

	/* Compute usable count */
	p->count = 1;
	for (e = 0; e < di; e++)
		p->count *= res[e];

	/* Reset the counter */
	p->ix = 0;

	if (co != NULL) {
		for (e = 0; e < di; e++)
			co[e] = 0;
	}
	return p->count;
}


/* Reset the counter */
void rpsh_reset(
rpsh *p	/* Pointer to structure */
) {
	p->ix = 0;
}


/* Increment pseudo-hilbert coordinates */
/* Return non-zero if count rolls over to 0 */
int rpsh_inc(
rpsh *p,	/* Pointer to structure */
int coa[]	/* Coordinates to return */
) {
	int di = p->di;
	int e;

	do {
		unsigned int b, tb;
		int gix;	/* Gray code index */
		
		p->ix = (p->ix + 1) & p->tmask;

		gix = p->ix ^ (p->ix >> 1);		/* Convert to gray code index */
	
		for (e = 0; e < di; e++) 
			coa[e] = 0;
		
		for (b = tb = 0; tb < p->tbits ; b++) {		/* Distribute bits */
			if (b & 1) {
				for (e = di-1; e >= 0; e--)  {		/* In reverse coord order */
					if (b < p->bits[e]) {
						coa[e] |= (gix & 1) << b;	/* ls bits of gix */
						gix >>= 1;
						tb++;
					}
				}
			} else {
				for (e = 0; e < di; e++)  {			/* In normal coord order */
					if (b < p->bits[e]) {
						coa[e] |= (gix & 1) << b;	/* ls bits of gix */
						gix >>= 1;
						tb++;
					}
				}
			}
		}

		/* Convert from Gray to binary coordinates */
		for (e = 0; e < di; e++)  {
			unsigned sh, tv;

			for(sh = 1, tv = coa[e];; sh <<= 1) {
				unsigned ptv = tv;
				tv ^= (tv >> sh);
				if (ptv <= 1 || sh == 16)
					break;
			}
			if (tv >= p->res[e])	/* Dumbo filter - increment again if outside cube range */
				break;
			coa[e] = tv;
		}

	} while (e < di);

	return (p->ix == 0);
}

/* =============================================== */
















