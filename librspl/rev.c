
/* 
 * Argyll Color Correction System
 * Multi-dimensional regularized spline data structure
 *
 * Reverse interpolation support code.
 *
 * Author: Graeme W. Gill
 * Date:   30/1/00
 *
 * Copyright 1999 - 2008 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Latest simplex/linear equation version.
 */

/* TTBD:

    Should fix the clipping case so that a direction weighting
	funtion can be applied. This should be used just like
	the perceptual case to increase L* constance for dark
    colors. This would entail large scale changes though,
	since a lot of code assumes minimal euclidean distance
	goal, from the cell selection structure [ See fill_nncell(),
	fill_nncell() and users of calc_fwd_nn_cell_list() ] and
	the within cell computation [ ie. See  nnearest_clip_solve(),
	clipn_setsort() etc. ]
	XYZ PCS couldn't work with a simple weighting - it would have
	to be a position dependent weighting.
	The SVD least squares computation case makes this hard to change ?
	Would have to feed in a weighting function, or can it be general ?
	-
	Can this be solved some other way, ie. by using gamut
	mapping type look up ? Problem is precision.
	-
	Vector clip could be used (if intent can be turned
	into computable vector clip direction), but it is slow,
	because it search all cells from source until it
	hits surface.

	Allow function callback to set auxiliary values for 
	flag RSPL_AUXLOCUS. 
	How to pass enough info back to aux_compute() ?

	Should auxil return multiple solutions if it finds them ???

 */

/* TTBD:
	Get rid of error() calls - return status instead

	Need to add a hefty overview and explanation of
	how all this works, before I forget it !

	ie:

	  Basic function requirements:  exact, auxil, locus, clip

	  Fwd cell - reverse cell list lookup

	  Basic layout di -> fdi + auxils + ink limit

	  Basic search strategy

	  Sub Simplex decomposition & properties

	  How each type of function finds solutions
		Sub-simplex dimensionality & dof + target dim & dof
		Linear algebra choices.
		
	  How final solutions are chosen

 */

/* PROBLEMS:

	Sometimes the aux locus doesn't correspond exactly to
	the inversion :- ie. one locus segment is returned,
	yet the inversion can't return a solution with
	a particular aux target that lies within that segment.
	(1150 near black, k ~= 0.4).


 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <memory.h>
#include <time.h>

#ifdef NT 
# ifdef WINVER
#  undef WINVER
# endif
# define WINVER 0x0500		/* We need 2k features */
# include <windows.h>
#else
# include <unistd.h>
# ifdef __APPLE__
#  include <fcntl.h>
#  include <sys/types.h>
#  include <sys/sysctl.h>
# endif
#endif

#define INKSCALE 5000.0	/* For ink limit weighting to fudge SVD least squares solution */

#include "rspl_imp.h"
#include "numlib.h"
#include "sort.h"		/* Heap sort */
#include "counters.h"	/* Counter macros */

//#define DMALLOC_GLOBALS
//#include "dmalloc.h"
//#undef DMALLOC_GLOBALS

#undef DEBUG1			/* Higher level code */
#undef DEBUG2			/* Lower level code */

/* Debug memory usage accounting */
#ifdef NEVER
#ifdef NEVER
int thissz, lastsz = -1;
#define INCSZ(s, bbb) {						\
					(s)->rev.sz += (bbb);	\
					(s)->rev.thissz = (s)->rev.sz/1000000;		\
                    if ((s)->rev.thissz != (s)->rev.lastsz) fprintf(stderr,"~1 0x%x: %s, %d: rev size = %d Mbytes, delta %d, limit %d\n",((int)(s) >> 8) & 0xf, __FILE__, __LINE__,(s)->rev.thissz,(bbb),(s)->rev.max_sz/1000000);	\
					(s)->rev.lastsz = (s)->rev.thissz;	\
					}
#define DECSZ(s, bbb) {						\
					(s)->rev.sz -= (bbb);	\
					(s)->rev.thissz = (s)->rev.sz/1000000;		\
                    if ((s)->rev.thissz != (s)->rev.lastsz) fprintf(stderr,"~1 0x%x: %s, %d: rev size = %d Mbytes, delta %d, limit %d\n",((int)(s) >> 8) & 0xf, __FILE__, __LINE__,(s)->rev.thissz,-(bbb),(s)->rev.max_sz/1000000);	\
					(s)->rev.lastsz = (s)->rev.thissz;	\
					}
#else
#define INCSZ(s, bbb) (s)->rev.sz += (bbb);	\
                     fprintf(stderr,"%s, %d: rev.sz += %d\n",__FILE__, __LINE__, bbb)
#define DECSZ(s, bbb) (s)->rev.sz -= (bbb);	\
                     fprintf(stderr,"%s, %d: rev.sz -= %d\n",__FILE__, __LINE__, bbb)
#endif
#else
#define INCSZ(s, bbb) (s)->rev.sz += (bbb)
#define DECSZ(s, bbb) (s)->rev.sz -= (bbb)
#endif

/* Set STATS in rev.h */

#define DOSORT				/* Cell sort */

/* Print a vectors value */
#define DBGVI(text, dim, out, vec, end)			\
{	int pveci;									\
	printf("%s",text);							\
	for (pveci = 0 ; pveci < (dim); pveci++)		\
		printf(out,(vec)[pveci]);				\
	printf(end);								\
}

/* Print a matrix value */
#define DBGMI(text, rows, cols, out, mat, end)		\
{	int pveci, pvecr;								\
	printf("%s",text);								\
	for (pvecr = 0 ; pvecr < (rows); pvecr++) {		\
		for (pveci = 0 ; pveci < (cols); pveci++)		\
			printf(out,(mat)[pvecr][pveci]);		\
	if ((pvecr+1) < (rows))							\
		printf("\n");								\
	}												\
	printf(end);									\
}

/* Do an arbitrary printf */
#define DBGI(text) printf text ;

#undef DEBUG
#undef DBG
#undef DBGV
#undef DBGM

#undef NEVER
#define ALWAYS

#ifdef DEBUG1
#undef DBGS
#undef DBG
#undef DBGV
#undef DBGM
#define DEBUG
#define DBGS(xxx) xxx
#define DBG(xxx) DBGI(xxx)
#define DBGV(xxx) DBGVI xxx
#define DBGM(xxx) DBGMI xxx
#else
#undef DEBUG
#undef DBGS
#undef DBG
#undef DBGV
#undef DBGM
#define DBGS(xxx) 
#define DBG(xxx) 
#define DBGV(xxx) 
#define DBGM(xxx) 
#endif

/* Debug string routines */
static char *pcellorange(cell *c);

/* Convention is to use:
   i to index grid points u.a
   n to index data points d.a
   e to index position dimension di
   f to index output function dimension fdi
   j misc and cube corners
   k misc
 */

#define	EPS (2e-6)			/* 2e-6 Allowance for numeric error */

static void make_rev(rspl *s);
static void init_revaccell(rspl *s);

static cell *get_rcell(schbase *b, int ix, int force);
static void uncache_rcell(revcache *r, cell *cp);
#define unget_rcell(r, cp) uncache_rcell(r, cp)		/* These are the same */
static void invalidate_revaccell(rspl *s);
static int decrease_revcache(revcache *rc);

/* ====================================================== */

static schbase *init_search(rspl *s, int flags, double *av, int *auxm,
                        double *v, double *cdir, co *cpp, int mxsoln, enum ops op);
static void adjust_search(rspl *s, int flags, double *av, enum ops op);
static schbase *set_search_limit(rspl *s, double (*limit)(void *vcntx, double *in),
                                 void *lcntx, double limitv);
static void set_lsearch(rspl *s, int e);
static void free_search(schbase *b);

static int *calc_fwd_cell_list(rspl *s, double *v);

static int *calc_fwd_nn_cell_list(rspl *s, double *v);

static void init_line_eq(schbase *b, double st[MXRO], double de[MXRO]);
static int *init_line(rspl *s, line *l, double st[MXRO], double de[MXRO]);
static int *next_line_cell(line *l);

static void search_list(schbase *b, int *rip, unsigned int tcount);

static void clear_limitv(rspl *s);

static double get_limitv(schbase *b, int ix,	float *fcb, double *p);

#ifdef STATS
static char *opnames[6] = { "exact", "clipv", "clipn", "auxil", "locus" };
#endif /* STATS */

#define INF_DIST 1e38		/* Stands for infinite "current best" distance */

/* ====================================================== */
/* Globals that track overall usage of reverse cache to aportion memory */
/* This is incremented for rspl with di > 1 when rev.rev_valid != 0 */
size_t g_avail_ram = 0;			/* Total maximum memory to be used */
size_t g_test_ram = 0;			/* Amount of memory that has been tested to be allocatable */
int g_no_rev_cache_instances = 0;
rev_struct *g_rev_instances = NULL;

/* ------------------------------------------------------ */
/* Retry allocation routines - if the malloc fails,       */
/* try reducing the cache size and trying again */
/* (This won't catch the problem if it occurs in a malloc outside rev) */

/* When a malloc fails, reduce the maximum cache to */
/* it's current allocation minus the given size. */
static void rev_reduce_cache(size_t size) {
	rev_struct *rsi;
	size_t ram;

	/* Compute how much ram is currently allocated */
	for (ram = 0, rsi = g_rev_instances; rsi != NULL; rsi = rsi->next)
		ram += rsi->sz;

	if (size > ram)
		error("rev_reduce_cache: run out of rev  virtual memory!");

//printf("~1 size = %d, g_test_ram = %d\n",size,g_test_ram);
//printf("~1 rev: Reducing cache because alloc of %d bytes failed. Reduced from %d to %d MB\n",
//size, g_avail_ram/1000000, (ram - size)/1000000);
	ram = g_avail_ram = ram - size;

	/* Aportion the memory, and reduce the cache allocation to match */
	ram /= g_no_rev_cache_instances; 
	for (rsi = g_rev_instances; rsi != NULL; rsi = rsi->next) {
		revcache *rc = rsi->cache;

		rsi->max_sz = ram;
		while (rc->nunlocked > 0 && rsi->sz > rsi->max_sz) {
			if (decrease_revcache(rc) == 0)
				break;
		}
//printf("~1 rev instance ram = %d MB\n",rsi->sz/1000000);
	}
//fprintf(stdout, "%c~~1 There %s %d rev cache instance%s with %d Mbytes limit\n",
//              cr_char,
//				g_no_rev_cache_instances > 1 ? "are" : "is",
//                   g_no_rev_cache_instances,
//				g_no_rev_cache_instances > 1 ? "s" : "",
//                   ram/1000000);
}

/* Check that the requested allocation plus 20 M Bytes */
/* can be allocated, and if not, reduce the rev-cache limit. */
/* This is so as to detect running out of VM before */
/* we actually run out and (on OS X) avoid emitting a warning. */
static void rev_test_vram(size_t size) {
	char *a1;
#ifdef __APPLE__
	int old_stderr, new_stderr;

	/* OS X malloc() blabs about a malloc failure. This */
	/* will confuse users, so we temporarily redirect stdout */
	fflush(stderr);
	old_stderr = dup(fileno(stderr));
	new_stderr = open("/dev/null", O_WRONLY | O_APPEND);
	dup2(new_stderr, fileno(stderr));
#endif
	size += 20 * 1024 * 1024;	/* This depends on the VM region allocation size */
	if ((a1 = malloc(size)) == NULL) {
		rev_reduce_cache(size);
	} else {
		free(a1);
	}
	g_test_ram = size/2;		/* Allow for twice as much VM to be used for each allocation */
#ifdef __APPLE__
	fflush(stderr);
	dup2(old_stderr, fileno(stderr));	/* Restore stderr */
	close(new_stderr);
	close(old_stderr);
#endif
}

static void *rev_malloc(rspl *s, size_t size) {
	void *rv;

	if ((size + 1 * 1024 * 1204) > g_test_ram)
		rev_test_vram(size);
	if ((rv = malloc(size)) == NULL) {
		rev_reduce_cache(size);
		rv = malloc(size);
	}
	if (rv != NULL)
		g_test_ram -= size;

	return rv;
}

static void *rev_calloc(rspl *s, size_t num, size_t size) {
	void *rv;

	if (((num * size) + 1 * 1024 * 1204) > g_test_ram)
		rev_test_vram(size);
	if ((rv = calloc(num, size)) == NULL) {
		rev_reduce_cache(num * size);
		rv = calloc(num, size);
	}
	if (rv != NULL)
		g_test_ram -= size;

	return rv;
}

static void *rev_realloc(rspl *s, void *ptr, size_t size) {
	void *rv;

	if ((size + 1 * 1024 * 1204) > g_test_ram)
		rev_test_vram(size);
	if ((rv = realloc(ptr, size)) == NULL) {
		rev_reduce_cache(size);		/* approximation */
		rv = realloc(ptr, size);
	}
	if (rv != NULL)
		g_test_ram -= size;

	return rv;
}


/* ====================================================== */
/* Set the ink limit information for any reverse interpolation. */
/* Calling this will clear the reverse interpolaton cache and acceleration structures. */
static void
rev_set_limit_rspl(
	rspl *s,		/* this */
	double (*limit)(void *lcntx, double *in),	/* Optional input space limit function. Function */
					/* should evaluate in[0..di-1], and return number that is not to exceed */
					/* limitv. NULL if not used */
	void *lcntx,	/* Context passed to limit() */
	double limitv	/* Value that limit() is not to exceed */
) {
	schbase *b;

	DBG(("rev: setting ink limit function 0x%x and limit %f\n",limit,limitv));
	/* This is a restricted size function */
	if (s->di > MXRI)
		error("rspl: rev_set_limit can't handle di = %d",s->di);
	if (s->fdi > MXRO)
		error("rspl: rev_set_limit can't handle fdi = %d",s->fdi);

	b = set_search_limit(s, limit, lcntx, limitv);	/* Init and set limit info */

	if (s->rev.inited) {		/* If cache and acceleration has been allocated */
		invalidate_revaccell(s);		/* Invalidate the reverse cache */
	}

	/* Invalidate any ink limit values cached with the fwd grid data */
	clear_limitv(s);
}

/* Get the ink limit information for any reverse interpolation. */
static void
rev_get_limit_rspl(
	rspl *s,		/* this */
	double (**limitf)(void *lcntx, double *in),	/* Return pointer to function of NULL if not set */
	void **lcntx,	/* return context pointer */
	double *limitv	/* Return limit value */
) {
	schbase *b = s->rev.sb;

	/* This is a restricted size function */
	if (s->di > MXRI)
		error("rspl: rev_get_limit can't handle di = %d",s->di);
	if (s->fdi > MXRO)
		error("rspl: rev_get_limit can't handle fdi = %d",s->fdi);

	if (b == NULL) {
		*limitf = NULL;
		*lcntx = NULL;
		*limitv = 0.0;
	} else {
		*limitf = s->limitf;
		*lcntx = s->lcntx;
		*limitv = s->limitv/INKSCALE;
	}
}

#define RSPL_CERTAIN 0x80000000 						/* WILLCLIP hint is certain */
#define RSPL_WILLCLIP2 (RSPL_CERTAIN | RSPL_WILLCLIP)	/* Clipping will certainly be needed */

/* Do reverse interpolation given target output values and (optional) auxiliary target */
/* input values. Return number of results and clipping flag. If return value == mxsoln, */
/* then there might be more results. The target values returned will correspond to the */
/* actual (posssibly clipped) point. The return value is the number of solutions + */
/* a clipped flag. Properly set hint flags improve performance, but a correct result should */
/* be returned if the RSPL_NEARCLIP is set, even if they are not set correctly. */
static int
rev_interp_rspl(
	rspl *s,		/* this */
	int flags,		/* Hint flag */
	int mxsoln,		/* Maximum number of solutions allowed for */
	int *auxm,		/* Array of di mask flags, !=0 for valid auxliaries (NULL if no auxiliaries) */
	double cdir[MXRO],	/* Clip vector direction wrt to cpp[0].v and length - NULL if not used */
	co *cpp			/* Given target output space value in cpp[0].v[] +  */
					/* target input space auxiliaries in cpp[0].p[], return */
					/* input space solutions in cpp[0..retval-1].p[], and */
) {
	int e, di = s->di;
	int fdi = s->fdi;
	int i, *rip = NULL;
	schbase *b = NULL;		/* Base search information */
	double auxv[MXRI];		/* Locus proportional auxiliary values */
	int didclip = 0;		/* flag - set if we clipped the target */
	
	DBGV(("\nrev interp called with out targets", fdi, " %f", cpp[0].v, "\n"));

	/* This is a restricted size function */
	if (di > MXRI)
		error("rspl: rev_interp can't handle di = %d",di);
	if (fdi > MXRO)
		error("rspl: rev_interp can't handle fdi = %d",fdi);

	if (auxm != NULL) {
		double ax[MXRI];
		for (i = 0; i < di; i++) {
			if (auxm[i] != 0)
				ax[i] = cpp[0].p[i];
			else
				ax[i] = 0.0;
		}
		DBGV(("                  auxiliaries mask", di, " %d", auxm, "\n"));
		DBGV(("                auxiliaries values", di, " %f", ax, "\n"));
	}
	DBG(("di = %d, fdi = %d\n",di, fdi));
	DBG(("flags = 0x%x\n",flags));

	mxsoln &= RSPL_NOSOLNS;		/* Prevent silliness */

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* Auxiliary is proportion of locus, so we need to find locus extent */	
	if (flags & RSPL_AUXLOCUS) {
		DBG(("rev interp has aux targets as proportion of locus\n"));

		flags &= ~RSPL_WILLCLIP;		/* Reset hint flag, as we will figure it out */

		/* For each valid auxiliary */
		for (e = 0; e < di; e++) {
			if (auxm[e] == 0)
				continue;			/* Skip unsused auxiliaries */
	
			/* Do search for min and max */
			DBG(("rev locus searching for aux %d min/max\n", e));
			if (b == NULL) {
				b = init_search(s, flags, cpp[0].p, auxm, cpp[0].v, cdir, cpp, mxsoln, locus);
#ifdef STATS
				s->rev.st[b->op].searchcalls++;
#endif	/* STATS */
			} else
				set_lsearch(s, e);		/* Reset locus search for next auxiliary */

			if (rip == NULL) {		/* Not done this yet */
				rip = calc_fwd_cell_list(s, cpp[0].v); /* Reverse grid index for out target */
				if (rip == NULL) {
					DBG(("Got NULL list (point outside range) for auxiliary locus search\n"));
					flags |= RSPL_WILLCLIP2;
					break;
				}
			}
	
			search_list(b, rip, s->get_next_touch(s)); /* Setup, sort and search the list */
	
			if (b->min > b->max) {			/* Failed to find locus */
				DBG(("rev interp failed to find locus for aux %d, so expect clip\n",e));
				flags |= RSPL_WILLCLIP2;
				break;
			}
			auxv[e] = (cpp[0].p[e] * (b->max - b->min)) + b->min;
		}

		DBG(("rev interp got all locuses, so expect exact result\n",e));
		if (!(flags & RSPL_WILLCLIP)) {
			flags |= RSPL_EXACTAUX;				/* Got locuses, so expect exact result */
		}
	}

	/* Init the search information */
	if (b == NULL)
		b = init_search(s, flags, cpp[0].p, auxm, cpp[0].v, cdir, cpp, mxsoln, exact);
	else
		adjust_search(s, flags, auxv, exact);		/* Using proportion of locus aux */
	
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* If hinted that we will not need to clip, look for exact solution. */
	if (!(flags & RSPL_WILLCLIP)) {
		DBG(("Hint we won't clip, so trying exact search\n"));

		/* First do an exact search (init will select auxil if requested) */
		adjust_search(s, flags, NULL, exact);
	
		/* Figure out the reverse grid index appropriate for this request */
		if (rip == NULL)	/* Not done this yet */
			rip = calc_fwd_cell_list(s, cpp[0].v);
	
#ifdef STATS
			s->rev.st[b->op].searchcalls++;
#endif	/* STATS */
		if (rip != NULL) {
			/* Setup, sort and search the list */
			search_list(b, rip, s->get_next_touch(s));
		} else {
			DBG(("Got NULL list (point outside range) for first exact reverse cell\n"));
		}
	
		/* If we selected exact aux, but failed to find a solution, relax expectation */
		if (b->nsoln == 0 && b->naux > 0 && (flags & RSPL_EXACTAUX)) {
//printf("~1 relaxing notclip expactation when nsoln == %d, naux = %d, falgs & RSPL_EXACTAUX = 0x%x\n", b->nsoln,b->naux,flags & RSPL_EXACTAUX);
			DBG(("Searching for exact match to auxiliary target failed, so try again\n"));
			adjust_search(s, flags & ~RSPL_EXACTAUX, NULL, exact);

#ifdef STATS
			s->rev.st[b->op].searchcalls++;
#endif	/* STATS */
			/* Candidate cell list should be the same */
			if (rip != NULL) {
				/* Setup, sort and search the list */
				search_list(b, rip, s->get_next_touch(s));
			} else {
				DBG(("Got NULL list (point outside range) for nearest search reverse cell\n"));
			}
		}
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* If the exact search failed, and we should look for a nearest solution */
	if (b->nsoln == 0 && (flags & RSPL_NEARCLIP)) {
		DBG(("Trying nearest search\n"));

#ifdef STATS
		s->rev.st[b->op].searchcalls++;
#endif	/* STATS */

		/* We get returned a list of cube base indexes of all cubes that have */
		/* the closest valid vertex value to the target value. */
		/* (This may not result in the true closest point if the geometry of */
		/* the vertex values is localy non-smooth or self intersecting, */
		/* but seems to return a good result in most realistic situations ?) */

		adjust_search(s, flags, NULL, clipn);

		/* Get list of cells enclosing nearest vertex */
		if ((rip = calc_fwd_nn_cell_list(s, cpp[0].v)) != NULL) {
			search_list(b, rip, s->get_next_touch(s)); /* Setup, sort and search the list */
		} else {
			DBG(("Got NULL list! (point inside gamut \?\?) for nearest search\n"));
		}

		if (b->nsoln > 0)
			didclip = RSPL_DIDCLIP;
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* If we still don't have a solution, do a vector direction clip */
	if (b->nsoln == 0 && b->canvecclip) {
		/* Find clipping solution in vector direction */
		line ln;				/* Structure to hold line context */
		unsigned int tcount;	/* grid touch count for this opperation */

		DBG(("Starting a clipping vector search now!!\n"));

		adjust_search(s, flags, NULL, clipv);

		tcount = s->get_next_touch(s);		/* Get next grid touched generation count */

#ifdef STATS
		s->rev.st[b->op].searchcalls++;
#endif	/* STATS */
		init_line_eq(b, b->v, cdir);				/* Init the implicit line equation */
		rip = init_line(s, &ln, cpp[0].v, cdir);	/* Init the line cell dda */
//~~1 HACK!!! should be <= 1.0 !!!
		for (; ln.t <= 2.0; rip = next_line_cell(&ln)) {
			if (rip == NULL) {
				DBG(("Got NULL list for this reverse cell\n"));
				continue;
			}

			/* Setup, sort and search the list */
			search_list(b, rip, tcount);

			/* If we have found a solution, then abort the search - */
			/* this line will be taking us away from the best solution. */
			if (b->nsoln > 0)
				break;
		}
		if (b->nsoln > 0)
			didclip = RSPL_DIDCLIP;
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* If the clipped solution seems to have been jumping to conclusions, */
	/* search for an exact solution. */
	if (didclip && (flags & RSPL_WILLCLIP && !(flags & RSPL_CERTAIN))
	 && (b->cdist/s->get_out_scale(s)) < 0.002) {
		co c_cpp       = b->cpp[0];	/* Save clip solution in case we want it */
		double c_idist = b->idist;	
		int c_iabove   = b->iabove;	
		int c_nsoln    = b->nsoln;
		int c_pauxcell = b->pauxcell;
		double c_cdist = b->cdist;
		int c_iclip    = b->iclip;

		DBG(("Trying exact search again\n"));

		/* Do an exact search (init will select auxil if requested) */
		adjust_search(s, flags & ~RSPL_WILLCLIP, NULL, exact);
	
		/* Figure out the reverse grid index appropriate for this request */
		rip = calc_fwd_cell_list(s, cpp[0].v);
	
#ifdef STATS
		s->rev.st[b->op].searchcalls++;
#endif	/* STATS */
		if (rip != NULL) {
			/* Setup, sort and search the list */
			search_list(b, rip, s->get_next_touch(s));
		} else {
			DBG(("Got NULL list (point outside range) for first exact reverse cell\n"));
		}
	
		/* If we selected exact aux, but failed to find a solution, relax expectation */
		if (b->nsoln == 0 && b->naux > 0 && (flags & RSPL_EXACTAUX)) {
			DBG(("Searching for exact match to auxiliary target failed, so try again\n"));
//printf("~1 relaxing didclip expactation when nsoln == %d, naux = %d, flags & RSPL_EXACTAUX = 0x%x\n", b->nsoln,b->naux,flags & RSPL_EXACTAUX);
			adjust_search(s, flags & ~RSPL_EXACTAUX, NULL, exact);

#ifdef STATS
			s->rev.st[b->op].searchcalls++;
#endif	/* STATS */
			/* Candidate cell list should be the same */
			if (rip != NULL) {
				/* Setup, sort and search the list */
				search_list(b, rip, s->get_next_touch(s));
			} else {
				DBG(("Got NULL list (point outside range) for nearest search reverse cell\n"));
			}
		}

		/* If we did get an exact solution */
		if (b->nsoln > 0) {
			DBG(("Deciding to return exact solution after finding clipped\n"));
			didclip = 0;		/* Reset did-clip and return exact solution */

		} else {
			DBG(("keeping clipped solution\n"));
			/* Restore the clipped solution */
			b->cpp[0] = c_cpp;
			b->idist = c_idist;	
			b->iabove = c_iabove;	
			b->nsoln = c_nsoln;
			b->pauxcell = c_pauxcell;
			b->cdist = c_cdist;
			b->iclip = c_iclip;
		}
	}

	if (b->nsoln > 0) {
		DBGV(("rev interp returning 1st soln: ",di," %f", cpp[0].p, "\n"));
	}
	DBG(("rev interp returning %d solutions%s\n",b->nsoln, didclip ? " [clip]" : ""));

	return b->nsoln | didclip;
}

/* ------------------------------------------------------------------------------------ */
/* Do reverse search for the auxiliary min/max ranges of the solution locus for the */
/* given target output values. */
/* Return number of locus segments found, up to mxsoln. 0 will be returned if no solutions */
/* are found. */

static int
rev_locus_segs_rspl (
	rspl *s,		/* this */
	int *auxm,		/* Array of di mask flags, !=0 for valid auxliaries (NULL if no auxiliaries) */
	co *cpp,		/* Input value in cpp[0].v[] */
	int mxsoln,		/* Maximum number of solutions allowed for */
	double min[][MXRI],	/* Array of min[MXRI] to hold return segment minimum values. */
	double max[][MXRI]	/* Array of max[MXRI] to hold return segment maximum values. */
) {
	int e, di = s->di;
	int f, fdi = s->fdi;
	int six;		/* solution index */
	int *rip = NULL;
	int rv = 1;				/* Return value */
	schbase *b = NULL;		/* Base search information */
	
	DBGV(("rev locus called with out targets", fdi, " %f", cpp[0].v, "\n"));
	
	/* This is a restricted size function */
	if (di > MXRI)
		error("rspl: rev_locus_segs can't handle di = %d",di);
	if (fdi > MXRO)
		error("rspl: rev_locus_segs can't handle fdi = %d",fdi);

	if (mxsoln < 1) {
		return 0;			/* Guard against silliness */
	}

	if (auxm != NULL) {
		int i;
		double ax[MXRI];
		for (i = 0; i < di; i++) {
			if (auxm[i] != 0)
				ax[i] = cpp[0].p[i];
			else
				ax[i] = 0.0;
		}
		DBGV(("                  auxiliaries mask", di, " %d", auxm, "\n"));
		DBGV(("                auxiliaries values", di, " %f", ax, "\n"));
	}

	/* Init default return values */
	for (six = 0; six < mxsoln; six++) {
		for (e = 0; e < di; e++) {
			if (auxm[e] == 0) {
				min[six][e] = max[six][e] = 0;	/* Return 0 for unused auxiliaries */
			} else {
				min[six][e] = 1.0;			/* max < min indicates invalid range */
				max[six][e] = 0.0;
			}
		}
	}

	/* For each valid auxiliary */
	for (e = 0; e < di; e++) {
		if (auxm[e] == 0)
			continue;			/* Skip unsused auxiliaries */

		/* Do search for min and max */
		DBG(("rev locus searching for aux %d min/max\n", e));
		if (b == NULL)
			b = init_search(s, 0, cpp[0].p, auxm, cpp[0].v, NULL, cpp, mxsoln, locus);
		else
			set_lsearch(s, e);		/* Reset locus search for next auxiliary */

		if (rip == NULL) {		/* Not done this yet */
			rip = calc_fwd_cell_list(s, cpp[0].v); /* Reverse grid index for this request */
			if (rip == NULL) {
				DBG(("Got NULL list (point outside range) for auxiliary locus search\n"));
				rv = 0;
				break;
			}
		}

		search_list(b, rip, s->get_next_touch(s)); /* Setup, sort and search the list */

		if (b->min > b->max) {
			rv = 0;				/* Failed to find a result */
			break;
		}

		if (b->asegs == 0) {		/* Overall min max only */

			min[0][e] = b->min;		/* Save single result */
			max[0][e] = b->max;

		} else {				/* Tracking auxiliary segments */
			int si;					/* Start i */
			int i, j, ff;

			/* Sort the segment list */
#define 	HEAP_COMPARE(A,B) (A.xval < B.xval)
			HEAPSORT(axisec, b->axisl, b->axisln)
#undef 		HEAP_COMPARE

#ifdef NEVER
for (i = 0; i < b->axisln; i++) {
printf("~2 xval = %f, verts = ",b->axisl[i].xval);
for (f = 0; f < b->axisl[i].nv; f++)
printf(" %d", b->axisl[i].vix[f]);
printf("\n");
}
#endif
			/* Find the segments by finding common verticies */
			six = si = i = 0;

			min[six][e] = b->axisl[i].xval;

			for (i++; i < (b->axisln-1); i++) {
				/* Check if any i and i-1 to j are connected */
				for (j = i-1; j >= si; j--) {
					for (f = 0; f < b->axisl[j].nv; f++) {
						for (ff = 0; ff < b->axisl[i].nv; ff++) {
							if (b->axisl[j].vix[f] == b->axisl[i].vix[ff])
								break;		/* Found a link */
						}
						if (ff < b->axisl[i].nv)
							break;
					}
					if (f < b->axisl[j].nv)
						break;
				}
				if (j < si) {	/* Wasn't linked */
					int ii, jj;
					/* Think we found a break. Check that all the rest of */
					/* the entries don't have any links to the previous group */

					/* This could be rather a slow way of checking ! (On^2) */
					for (ii = i+1; ii < (b->axisln); ii++) {
						for (jj = i-1; jj >= si; jj--) {
							for (f = 0; f < b->axisl[jj].nv; f++) {
								for (ff = 0; ff < b->axisl[ii].nv; ff++) {
									if (b->axisl[jj].vix[f] == b->axisl[ii].vix[ff])
										break;		/* Found a link */
								}
								if (ff < b->axisl[ii].nv)
									break;
							}
							if (f < b->axisl[jj].nv)
								break;
						}
						if (jj >= si)
							break;
					}
					if (ii >= b->axisln) {	/* Wasn't forward linked */
						/* Nothing ahead links to last group */
						max[six][e] = b->axisl[i-1].xval;

						/* If we run out of solution space */
						/* merge the last segments */
						if ((six+1) < mxsoln) {
							six++;
							min[six][e] = b->axisl[i].xval;
						}
					}
				}
			}
			max[six++][e] = b->axisl[i].xval;

			if (six > rv)
				rv = six;
		}
	}

#ifdef STATS
	s->rev.st[b->op].searchcalls++;
#endif	/* STATS */
	if (rv) {
		for (six = 0; six < rv; six++) {
			DBG(("rev locus returning:\n"));
			DBGV(("     min", di, " %f", min[six], "\n"));
			DBGV(("     max", di, " %f", max[six], "\n"));
		}
	}

	DBG(("rev locus returning status %d\n",rv));
	return rv;
}

/* ------------------------------------------------------------------------------------ */
typedef double mxdi_ary[MXRI];

/* Do reverse search for the locus of the auxiliary input values given a target output. */
/* Return 1 on finding a valid solution, and 0 if no solutions are found. */
static int
rev_locus_rspl(
	rspl *s,		/* this */
	int *auxm,		/* Array of di mask flags, !=0 for valid auxliaries (NULL if no auxiliaries) */
	co *cpp,		/* Input value in cpp[0].v[] */
	double min[MXRI],/* Return minimum auxiliary values */
	double max[MXRI] /* Return maximum auxiliary values */
) {

	/* Use segment routine to compute oveall locus */
	return rev_locus_segs_rspl (s, auxm, cpp, 1, (mxdi_ary *)min, (mxdi_ary *)max);
}

/* ------------------------------------------------------------------------------------ */

#ifdef DEBUG2
#define DEBUG
#undef DBG
#undef DBGV
#undef DBGM
#define DBG(xxx) DBGI(xxx)
#define DBGV(xxx) DBGVI xxx
#define DBGM(xxx) DBGMI xxx
#else
#undef DEBUG
#undef DBG
#undef DBGV
#undef DBGM
#define DBG(xxx) 
#define DBGV(xxx) 
#define DBGM(xxx) 
#endif

/* ------------------------------------------------ */
/* subroutines of top level reverse lookup routine */

static int exact_setsort(schbase *b, cell *c);
static int exact_compute(schbase *b, simplex *x);

static int auxil_setsort(schbase *b, cell *c);
static int auxil_check(schbase *b, cell *c);
static int auxil_compute(schbase *b, simplex *x);

static int locus_setsort(schbase *b, cell *c);
static int locus_check(schbase *b, cell *c);
static int locus_compute(schbase *b, simplex *x);

static int clipv_setsort(schbase *b, cell *c);
static int clipv_check(schbase *b, cell *c);
static int clipv_compute(schbase *b, simplex *x);

static int clipn_setsort(schbase *b, cell *c);
static int clipn_check(schbase *b, cell *c);
static int clipn_compute(schbase *b, simplex *x);

/* Allocate the search base structure */
static schbase *
alloc_sb(rspl *s) {
	schbase *b;
	if ((b = s->rev.sb = (schbase *)rev_calloc(s, 1, sizeof(schbase))) == NULL)
		error("rspl malloc failed - rev.sb structure");
	INCSZ(s, sizeof(schbase));

	b->s     = s;				/* rsp */
	b->pauxcell =				/* Previous solution cell indexes */
	b->plmaxcell = 
	b->plmincell = -1;

	return b;
}

/* Free the search base structure */
static void
free_sb(schbase *b) {
	DECSZ(b->s, sizeof(schbase));
	free(b);
}

/* Do the basic search type independent initialization */
static schbase *	/* Return pointer to base search information */
init_search(
	rspl *s,		/* rsp; */
	int flags,		/* Hint flag */

	double *av,		/* Auxiliary input values - may be NULL */
	int *auxm,		/* Array of di mask flags, !=0 for valid auxliaries (NULL if no auxiliaries) */
					/* Locus search will search for max/min of first valid auxlilary */
	double *v,		/* Output value target, NULL if none */
	double *cdir,	/* Clip vector direction, NULL if none */
	co *cpp,		/* Array that hold solutions, NULL if none. */
	int mxsoln,		/* Maximum number of solutions allowed for */
	enum ops op		/* Type of reverse search operation requested */
) {
	schbase *b = NULL;		/* Pointer to search base information structure */
	int e, di = s->di;
	int f, fdi = s->fdi;

	DBG(("Initializing search\n"));

	if (s->rev.inited == 0) 	/* Compute reverse info if it doesn't exist */
		make_rev(s);

	/* If first time initialisation (Fourth section init) */
	if ((b = s->rev.sb) == NULL) {
		b = alloc_sb(s);
	}

	/* Init some basic search info */
	b->op    = op;				/* operation */
	b->flags = flags;			/* hint flags */
	b->canvecclip = 0;			/* Assume invalid clip direction */

	b->ixc = (1<<di)-1;			/* Cube index of corner that holds maximum input values */

	/* Figure out if auxiliaries have been requested */
	b->naux = 0;
	b->auxbm = 0;
	if (auxm != NULL) {
		unsigned bm;

		if (mxsoln > 1)
			b->asegs = 1;		/* Find all segments */
		else
			b->asegs = 0;		/* Find only overall aux locus range */

		for (e = di-1, bm = 1 << e; e >= 0; e--, bm >>= 1) {	/* Record auxiliary mask bits */
			if (av != NULL)
				b->av[e] = av[e];	/* Auxiliary target values */
			b->auxm[e] = auxm[e];	/* Auxiliary mask */
			if (auxm[e] != 0) {
				b->auxbm |= bm;			/* Auxiliary bit mask */
				b->auxi[b->naux++] = e;	/* Index of next auxiliary input to be used */
				/* Auxiliary locus extent */
				b->lxi = e;			/* Assume first one */
				b->max = -INF_DIST;	/* In case searching for max */
				b->min =  INF_DIST;	/* In case searching for minimum */
				b->axisln = 0;		/* No intersects in list */
			}
		}
	}

	/* Figure out if the clip direction is meaningfull */
	/* Check that the clip vector makes sense */
	if (cdir != NULL) {	/* Clip vector is specified */
		double ss;
		for (ss = 0.0, f = 0; f < fdi; f++) {
			double tt = cdir[f];
			b->cdir[f] = tt;
			ss += tt * tt;
		}

		if (ss > 1e-6) {
			b->canvecclip = 1;	/* It has a non-zero length */
			ss = sqrt(ss);
			/* Compute normalised clip vector direction */
			for (f = 0; f < fdi; f++) {
				b->ncdir[f] = b->cdir[f]/ss;
			}
		}
	}

	if (di <= fdi)		/* Only allow auxiliaries if di > fdi */
		b->naux = 0;

	/* Switch to appropriate operation */
	if (b->op == exact && (b->naux > 0 || di != fdi)) {
		b->op = auxil;
	} else if (b->op == auxil && b->naux == 0 && di == fdi) {
		b->op = exact;
	}

	/* Set appropriate functions for type of operation */
	switch (b->op) {
		case exact:
			b->setsort = exact_setsort;
			b->check   = NULL;
			b->compute = exact_compute;
			b->snsdi = b->ensdi = di;	/* Search full dimension simplex, expect point soln. */
			break;
		case auxil:
			b->setsort = auxil_setsort;
			b->check   = auxil_check;
			b->compute = auxil_compute;
			b->snsdi = di;				/* Start here DOF = di-fdi locus solutions */
			b->ensdi = fdi;				/* End with DOF = 0 for point solutions */
			break;
		case locus:
			b->setsort = locus_setsort;
			b->check   = locus_check;
			b->compute = locus_compute;
			b->snsdi = b->ensdi = fdi;	/* Search for point solutions */
			break;
		case clipv:
			b->setsort = clipv_setsort;
			b->check   = clipv_check;
			b->compute = clipv_compute;
											/* Clip vector 1 dimension in output space, */
			b->snsdi = b->ensdi = fdi-1;	/* search planes for combined point solution */
			break;
		case clipn:
			b->setsort = clipn_setsort;
			b->check   = clipn_check;
			b->compute = clipn_compute;
			b->snsdi = 0;				/* Start with DOF = 0 for point solutions */
			b->ensdi = fdi-1;			/* End on DOF = di-fdi-1 on surfaces of simplexes */
			break;
		default:
			error("init_search: Unknown operation %d\n",b->op);
	}

	if (v != NULL) {
		for (f = 0; f < fdi; f++)	/* Record target output values */
			b->v[f] = v[f];
		b->v[fdi] = s->limitv;		/* Limitvalue is output target for limit clip subsimplexes */
	}

	b->mxsoln = mxsoln;				/* Allow solutions to be returned */
	b->cpp    = cpp;				/* Put solutions here */
	b->nsoln = 0;					/* No solutions at present */
	b->iclip = 0;					/* Default solution isn't above ink limit */

	if (flags & RSPL_EXACTAUX)		/* Expect to be able to match auxiliary target exactly */
		b->idist = 2.0 * EPS;		/* Best input distance to beat - helps sort/triage */
	else
		b->idist = INF_DIST;		/* Best input distance to beat. */
	b->iabove = 0;					/* Best isn't known to be above (yet) */

	b->cdist = INF_DIST;			/* Best clip distance to beat. */

	DBG(("Search initialized\n"));

	return b;
}

/* Adjust the search */
static void
adjust_search(
	rspl *s,		/* rsp; */
	int flags,		/* Hint flag */
	double *av,		/* Auxiliary input values - may be NULL */
	enum ops op		/* Type of reverse search operation requested */
) {
	schbase *b = s->rev.sb;		/* Pointer to search base information structure */
	int e, di = s->di;
	int fdi = s->fdi;

	DBG(("Adjusting search\n"));

	b->op    = op;				/* operation */
	b->flags = flags;			/* hint flags */

	/* Switch from exact to aux if we need to */
	if (b->op == exact && (b->naux > 0 || di != fdi)) {
		b->op = auxil;
	} else if (b->op == auxil && b->naux == 0 && di == fdi) {
		b->op = exact;
	}

	/* Update auxiliary target values */
	if (av != NULL) {
		for (e = 0; e < b->naux; e++) {
			int ee = b->auxi[e];
			b->av[ee] = av[ee];
		}
	}

	/* Set appropriate functions for type of operation */
	switch (b->op) {
		case exact:
			b->setsort = exact_setsort;
			b->check   = NULL;
			b->compute = exact_compute;
			b->snsdi = b->ensdi = di;		/* Expect point solution */
			break;
		case auxil:
			b->setsort = auxil_setsort;
			b->check   = auxil_check;
			b->compute = auxil_compute;
			b->snsdi = di;				/* Start here DOF = di-fdi locus solutions */
			b->ensdi = fdi;				/* End with DOF = 0 for point solutions, */
			break;						/* will early exit DOF if good soln found. */
		case locus:
			b->setsort = locus_setsort;
			b->check   = locus_check;
			b->compute = locus_compute;
			b->snsdi = b->ensdi = fdi;	/* Search for point solutions */
			break;
		case clipv:
			b->setsort = clipv_setsort;
			b->check   = clipv_check;
			b->compute = clipv_compute;
											/* Clip vector 1 dimension in output space, */
			b->snsdi = b->ensdi = fdi-1;	/* so the intersection with the simplex is a point. */
			break;
		case clipn:
			b->setsort = clipn_setsort;
			b->check   = clipn_check;
			b->compute = clipn_compute;
			b->snsdi = 0;				/* Start with DOF = 0 for point solutions */
			b->ensdi = fdi-1;			/* End on DOF = di-fdi-1 on surfaces of simplexes */
			break;						/* Will go through all DOF */
		default:
			error("init_search: Unknown operation %d\n",b->op);
	}

	b->nsoln = 0;					/* No solutions at present */

	if (flags & RSPL_EXACTAUX)		/* Expect to be able to match auxiliary target exactly */
		b->idist = 2.0 * EPS;		/* Best input distance to beat - helps sort/triage */
	else
		b->idist = INF_DIST;		/* Best input distance to beat. */
	b->iabove = 0;					/* Best isn't known to be above (yet) */

	b->cdist = INF_DIST;			/* Best clip distance to beat. */

	DBG(("Search adjusted\n"));
}

/* Adjust existing locus search for a different auxiliary */
static void
set_lsearch(
rspl *s,
int e			/* Next auxiliary */
) {
	schbase *b = s->rev.sb;		/* Pointer to search base information structure */

	b->lxi = e;			/* Assume first one */
	b->max = -INF_DIST;	/* In case searching for max */
	b->min =  INF_DIST;	/* In case searching for minimum */
	b->axisln = 0;		/* No intersects in list */
}

/* Set the limit search information */
/* Note this doesn't create or init the main rev information. */
static schbase *	/* Return pointer to base search information */
set_search_limit(
	rspl *s,		/* rsp; */
	double (*limitf)(void *vcntx, double *in),	/* Optional input space limit function. Function */
					/* should evaluate in[0..di-1], and return number that is not to exceed */
					/* limitv. NULL if not used */
	void *lcntx,	/* Context passed to limit() */
	double limitv	/* Value that limit() is not to exceed */
) {
	schbase *b = NULL;		/* Pointer to search base information structure */

	/* If sb info needs initialising (Fourth section init) */
	if ((b = s->rev.sb) == NULL) {
		b = alloc_sb(s);
	}

	s->limitf = limitf;			/* Input limit function */
	s->lcntx  = lcntx; 			/* Context passed to limit() */
	s->limitv= INKSCALE * limitv; 	/* Context passed to values not to be exceedded by limit() */
	if (limitf != NULL) {
		s->limiten = 1;				/* enable limiting by default */
	} else
		s->limiten = 0;				/* No limit function, so limiting not enabled. */

	return b;
}

/* Free any search specific data, plus the search base. */
static void
free_search(
schbase *b	/* Base search information */
) {
	DBG(("Freeing search\n"));

	/* Clip line implicit equation (incuding space for ink target) */
	if (b->cla != NULL) {
		int fdi = b->s->fdi;
		free_dmatrix(b->cla, 0, fdi-1, 0, fdi);
		b->cla = NULL;
	}

	/* Auxiliary segment list */
	if (b->axislz > 0) {
		free(b->axisl);
		DECSZ(b->s, b->axislz * sizeof(axisec));
		b->axisl = NULL;
		b->axislz = 0;
		b->axisln = 0;
	}

	/* Sorted cell list */
	if (b->lclistz > 0) {
		free(b->lclist);
		DECSZ(b->s, b->lclistz * sizeof(cell *));
		b->lclist = NULL;
		b->lclistz = 0;
	}

	/* Simplex filter list */
	if (b->lsxfilt > 0) {
		free(b->sxfilt);
		DECSZ(b->s, b->lsxfilt * sizeof(char));
		b->sxfilt = NULL;
		b->lsxfilt = 0;
	}

	free_sb(b);
}

/* Return the pointer to the list of fwd cells given */
/* the target output values. The pointer will be to the first */
/* index in the list (ie. list address + 3) */
/* Return NULL if none in list (out of gamut). */
static int *
calc_fwd_cell_list(
	rspl *s,		/* this */
	double *v		/* Output values */
) {
	int f, fdi = s->fdi;
	int **rpp;
	int rgres_1 = s->rev.res - 1;

	if (s->rev.rev_valid == 0)
		init_revaccell(s);
		
	for (rpp = s->rev.rev, f = 0; f < fdi; f++) {
		int mi;
		double t = (v[f] - s->rev.gl[f])/s->rev.gw[f];
		mi = (int)floor(t);				/* Grid coordinate */
		if (mi < 0 || mi > rgres_1) { 	/* If outside valid reverse range */
			return NULL;
		}
		rpp += mi * s->rev.coi[f];	/* Accumulate reverse grid pointer */
	}
	if (*rpp == NULL)
		return NULL;
	return (*rpp) + 3;
}

void alloc_simplexes(cell *c, int nsdi);

/* Given a pointer to a list of fwd cells, cull cells that */
/* cannot contain or improve the solution, sort the list, */
/* and then compute the final best solution. */
static void
search_list(
schbase *b,				/* Base search information */
int     *rip,			/* Pointer to first index in cell list */
unsigned int tcount		/* grid touch count for this operation */
) {
	rspl *s = b->s;
	int nsdi;
	int i;
	int nilist;			/* Number in cell list */
	unsigned int stouch;	/* Simplex touch count */
	
	DBG(("search_list called\n"));

	/* (rip[-3] contains allocation for fwd cells in the list) */
	/* (rip[-2] contains the index of the next free entry in the list) */
	/* (rip[-1] contains the reference count for the list) */
	if (b->lclistz < rip[-3]) {	/* Allocate more space if needed */

		if (b->lclistz > 0) {	/* Free old space before allocating new */
			free(b->lclist);
			DECSZ(b->s, b->lclistz * sizeof(cell *));
		}
		b->lclistz = 0;
		/* Allocate enough space for all the candidate cells */
		if ((b->lclist = (cell **)rev_malloc(s, rip[-3] * sizeof(cell *))) == NULL)
			error("rev: malloc failed - candidate cell list, count %d",rip[-3]);
		b->lclistz = rip[-3];	/* Current allocated space */
		INCSZ(b->s, b->lclistz * sizeof(cell *));
	}
		
	/* Get the next simplex touch count, so that we don't search shared */
	/* face simplexes more than once in this pass through the cells. */
	if ((stouch = ++s->rev.stouch) == 0) {	/* If touch count rolls over */
		cell *cp;
		stouch = s->rev.stouch = 1;

		DBG(("touch has rolled over, resetting it\n"));
		/* For all of the cells */
		for (cp = s->rev.cache->mrubot; cp != NULL; cp = cp->mruup) {
			int nsdi;
	
			if (cp->s == NULL)	/* Cell has never been used */
				continue;

			/* For all the simplexes in the cell */
			for (nsdi = 0; nsdi <= s->di; nsdi++) {
				if (cp->sx[nsdi] != NULL) {
					int si;

					for (si = 0; si < cp->sxno[nsdi]; si++) {
						cp->sx[nsdi][si]->touch = 0;
					}
				}
			}
		}
	}

	/* For each chunk of the list that we can fit in the rcache: */
	for(; *rip != -1;)  {

		/* Go through all the candidate fwd cells, and build up the list of search cells */
		for(nilist = 0; *rip != -1; rip++)  {
			int ix = *rip;				/* Fwd cell index */
			float *fcb = s->g.a + ix * s->g.pss;	/* Pointer to base float of fwd cell */
			cell *c;

			if (TOUCHF(fcb) >= tcount) {	/* If we have visited this cell before */
				DBG((" Already touched cell index %d\n",ix));
				continue;
			}
			/* Get pointers to cells from cache, and lock it in the cache */
			if ((c = get_rcell(b, ix, nilist == 0 ? 1 : 0)) == NULL) {
				static int warned = 0;
				if (!warned) {
					warning("%cWarning - Reverse Cell Cache exausted, processing in chunks",cr_char);
					warned = 1;
				}
				DBG(("revcache is exausted, do search in chunks\n"));
				if (nilist == 0) {
					/* This should never happen, because nz force should prevent it */
					revcache *rc = s->rev.cache;
					cell *cp;
					int nunlk = 0;
					/* Double check that there are no unlocked cells */
					for (cp = rc->mrubot; cp != NULL && cp->refcount > 0; cp = cp->mruup) {
						if (cp->refcount == 0)
							nunlk++;
					}
					fprintf(stdout,"Diagnostic: rev.sz = %lu, rev.max_sz = %lu, numlocked = %d, nunlk = %d\n",
					               (unsigned long)rc->s->rev.sz, (unsigned long)rc->s->rev.max_sz,
					               rc->nunlocked, nunlk);
					error("Not enough memory to process in chunks");
				}
				break;		/* cache has run out of room, so abandon, and do it next time */
			}

			DBG(("checking out cell %d range %s\n",ix,pcellorange(c)));
			TOUCHF(fcb) = tcount;			/* Touch it */

			/* Check mandatory conditions, and compute search key */
			if (!b->setsort(b, c)) {
				DBG(("cell %d rejected from list\n",ix));
				unget_rcell(s->rev.cache, c);
				continue;
			}
			DBG(("cell %d accepted into list\n",ix));

			b->lclist[nilist++] = c; /* Cell is accepted as recursion candidate */
		}

		if (nilist == 0) {
			DBG(("List was empty\n"));
		}

#ifdef DOSORT
		/* If appropriate, sort child cells into best order */
		/* == sort key smallest to largest */
		switch (b->op) {
			case locus:
				{	/* Special case, adjust sort values */
					double min = INF_DIST, max = -INF_DIST;
					for (i = 0; i < nilist; i++) {
						cell *c = b->lclist[i];
						if (c->sort < min)
							min = c->sort;
						if (c->sort > max)
							max = c->sort;
					}
					max = min + max;	/* Total of min/max */
					min = 0.5 * max;	/* Average sort value */
					for (i = 0; i < nilist; i++) {
						cell *c = b->lclist[i];
						if (c->ix == b->plmincell || c->ix == b->plmaxcell) {
							c->sort = -1.0;		/* Put previous solution cells at head of list */
						} else if (c->sort > min) {
							c->sort = max - c->sort;	/* Reflect about average */
						}
					}
				}
				/* Fall through to sort */
			case auxil:
			case clipv:
			case clipn:
#define 	HEAP_COMPARE(A,B) (A->sort < B->sort)
				HEAPSORT(cell *,b->lclist, nilist)
#undef 		HEAP_COMPARE
				break;
			default:
				break;
		}
#endif /* DOSORT */

		DBG(("List sorted, about to search\n"));
#ifdef NEVER
		printf("\n~1 Op = %s, Cell sort\n",opnames[b->op]);
		for (i = 0; i < nilist; i++) {
			printf("~1 List %d, cell %d, sort = %f\n",i,b->lclist[i]->ix,b->lclist[i]->sort);
		}
#endif /* NEVER */

		/* 
			Tried reversing the "for each cell" and "for each level" loops,
			but it made a negligible difference to the performance.
			We choose to have cell on the outer so that we can unlock
			them as we go, so that they may be freed, even though
			this is a couple of percent slower (?).
		 */

		/* For each cell in the list */
		for (i = 0; i < nilist; i++) {
			cell *c = b->lclist[i];

#ifdef STATS
			s->rev.st[b->op].csearched++;
#endif /* STATS */

			/* For each dimensionality of sub-simplexes, in given order */
			DBG(("Searching from level %d to level %d\n",b->snsdi, b->ensdi));
			for (nsdi = b->snsdi;;) {
				int j, nospx;					/* Number of simplexes in cell */

				DBG(("\n******************\n"));
				DBG(("Searching level %d\n",nsdi));

				/* For those searches that have an optimisation goal, */
				/* re-check the cell to see if the goal can still improve on. */
				if (b->check != NULL && !b->check(b, c))
					break;

				if (c->sx[nsdi] == NULL) {
					alloc_simplexes(c, nsdi);	/* Do level 1 initialisation for nsdi */
				}

				/* For each simplex in a cell */
				nospx = c->sxno[nsdi];			/* Number of nsdi simplexes */
				for (j = 0; j < nospx; j++) {
					simplex *x = c->sx[nsdi][j];

					if (x->touch >= stouch) {
						continue;						/* We've already seen this one */
					}

					if (s->limiten == 0) {
						if (x->flags & SPLX_CLIPSX)		/* If limiting is disabled, we're */
							continue;					/* not interested in clip plane simplexes */
					}
#ifdef STATS
					s->rev.st[b->op].ssearched++;
#endif /* STATS */
					if (b->compute(b, x)) {
						DBG(("search aborted by compute\n"));
						break;					/* Found enough solutions */
					}
					x->touch = stouch;			/* Don't look at it again */

				}	/* Next Simplex */

				if (nsdi == b->ensdi)
					break;						/* We're done with levels */

				/* Next Simplex dimensionality */
				if (b->ensdi < b->snsdi) {
					if (nsdi == b->snsdi && b->nsoln > 0
					 && (b->op != auxil || b->idist <= 2.0 * EPS))
						break; 		/* Don't continue though decreasing */
									/* sub-simplex dimensions if we found a solution at */
									/* the highest dimension level. */
					nsdi--;
				} else if (b->ensdi > b->snsdi) {
					nsdi++;				/* Continue through increasing sub-simplex dimenionality */
				}						/* until we get to the top. */
			}
			/* Unlock the cache cell now that we're done with it */
			unget_rcell(s->rev.cache, b->lclist[i]);
		}	/* Next cell */

	}	/* Next chunk */

	DBG(("search_list complete\n"));
	return;
}

/* ------------------------------------- */
/* Vector search in output space support */

/* Setup the line, and fetch the first cell */
/* Return the pointer to the list of fwd cells, NULL if none in list. */
static int *
init_line(
	rspl *s,			/* this */
	line *l,			/* line structure */
	double st[MXRO],	/* start of line */
	double de[MXRO]		/* line direction and length */
) {
	int f, fdi = s->fdi;
	int **rpp;
	int rgres_1 = s->rev.res - 1;
	int nvalid = 0;		/* Flag set if outside reverse grid range */

	DBGV(("Line from ", fdi, " %f", st, "\n"));
	DBGV(("In dir    ", fdi, " %f", de, "\n"));
	DBGV(("gl        ", fdi, " %f", s->rev.gl, "\n"));
	DBGV(("gh        ", fdi, " %f", s->rev.gh, "\n"));
	DBGV(("gw        ", fdi, " %f", s->rev.gw, "\n"));
	
	/* Init */
	l->s = s;
	for (f = 0; f < fdi; f++) {
		l->st[f] = st[f] - s->rev.gl[f];
		l->de[f] = de[f];
		if (de[f] > 0.0)
			l->di[f] = 1;	/* Axis increments */
		else if (de[f] < 0.0)
			l->di[f] = -1;
		else
			l->di[f] = 0;
	}
	l->t = 0.0;
	DBGV(("increments =", fdi, " %d", l->di, "\n"));

	/* Figure out the starting cell */
	for (rpp = s->rev.rev, f = 0; f < fdi; f++) {
		double t = l->st[f]/s->rev.gw[f];
		l->ci[f] = (int)floor(t);					/* Grid coordinate */
		if (l->ci[f] < 0 || l->ci[f] > rgres_1) 	/* If outside valid reverse range */
			nvalid = 1;
		rpp += l->ci[f] * s->rev.coi[f];	/* Accumulate reverse grid pointer */
	}
	DBGV(("current line cell = ", fdi, " %d", l->ci, "")); DBG((",  t = %f, nvalid = %d\n",l->t,nvalid));
#ifdef DEBUG
{
int ii;
double tt;
printf("Current cell = ");
for (ii = 0; ii < fdi; ii++) {
	tt = l->ci[ii] * s->rev.gw[ii] + s->rev.gl[ii];
	printf(" %f - %f",tt,tt+s->rev.gw[ii]);
}
printf("\n");
}
#endif	/* DEBUG */
	if (nvalid)
		return NULL;
	if (*rpp == NULL)
		return NULL;
	return *rpp + 3;
}

/* Get the next cell on the line. */
/* Return the pointer to the list of fwd cells, NULL if none in list. */
static int *
next_line_cell(
	line *l		/* line structure */
) {
	rspl *s = l->s;
	int bf = 0, f, fdi = s->fdi;
	int **rpp;
	int rgres_1 = s->rev.res - 1;
	double bt = 100.0;	/* Best (smalest +ve) parameter value to move */

	/* See which axis cell crossing we will hit next */
	for (f = 0; f < fdi; f++) {
		double t;
		if (l->de[f] != 0) {
			t = ((l->ci[f] + l->di[f]) * s->rev.gw[f] - l->st[f])/l->de[f];
			DBG(("t for dim %d = %f\n",f,t));
			if (t < bt) {
				bt = t;
				bf = f;		/* Best direction to move */
			}
		}
	}

	/* Move to the next reverse grid coordinate */
	l->ci[bf] += l->di[bf];
	l->t = bt;

	DBGV(("current line cell =", fdi, " %d", l->ci, "")); DBG((",  t = %f\n",l->t));

#ifdef DEBUG
{
int ii;
double tt;
printf("Current cell = ");
for (ii = 0; ii < fdi; ii++) {
	tt = l->ci[ii] * s->rev.gw[ii] + s->rev.gl[ii];
	printf(" %f - %f",tt,tt+s->rev.gw[ii]);
}
printf("\n");
}
#endif	/* DEBUG */

	/* Compute reverse cell index */
	for (rpp = s->rev.rev, f = 0; f < fdi; f++) {
		if (l->ci[f] < 0 || l->ci[f] > rgres_1) { 	/* If outside valid reverse range */
			DBG(("Outside list on dim %d, 0 <= %d <= %d\n", f, l->ci[f],rgres_1));
			return NULL;
		}
		rpp += l->ci[f] * s->rev.coi[f];	/* Accumulate reverse grid pointer */
	}
	if (*rpp == NULL)
		return NULL;
	return *rpp + 3;
}

/* ------------------------------------- */
/* Clip nearest support. */

/* Track candidate cells nearest and furthest */
struct _nncell_nf{
	double n, f;
}; typedef struct _nncell_nf nncell_nf;

/* Given and empty nnrev index, create a list of */ 
/* the forward cells that may contain the nearest value by */
/* using and exaustive search. This is used for faststart. */
static void fill_nncell(
	rspl *s,
	int *co,	/* Integer coords of cell to be filled */
	int ix		/* Index of cell to be filled */
) {
	int i;
	int e, di = s->di;
	int f, fdi = s->fdi;
	double cc[MXDO];	/* Cell center */
	double rr = 0.0;	/* Cell radius */
	int **rpp, *rp;
	int gno = s->g.no;
	float *gp;			/* Pointer to fwd grid points */
	nncell_nf *nf;		/* cloase and far distances corresponding to list */
	double clfu = 1e38;	/* closest furthest distance in list */
	
	rpp = s->rev.nnrev + ix;
	rp = *rpp;

	/* Compute the center location and radius of the target cell */
	for (f = 0; f < fdi; f++) {
		cc[f] = s->rev.gw[f] * (co[f] + 0.5) + s->rev.gl[f];
		rr += 0.25 * s->rev.gw[f] * s->rev.gw[f];
	}
	rr = sqrt(rr);
//printf("~1 fill_nncell() cell ix %d, coord %d %d %d, cent %f %f %f, rad %f\n",
//ix, co[0], co[1], co[2], cc[0], cc[1], cc[2], rr);
//printf("~1 total of %d fwd cells\n",gno);

	/* For all the forward cells: */
	for (gp = s->g.a, i = 0; i < gno; gp += s->g.pss, i++) {
		int ee;
		int uil;			/* One is under the ink limit */
		double dn, df;		/* Nearest and farthest distance of fwd cell values */

		/* Skip cubes that are on the outside edge of the grid */
		for (e = 0; e < di; e++) {
			if(G_FL(gp, e) == 0)		/* At the top edge */
				break;
		}
		if (e < di) {	/* Top edge - skip this cube */
			continue;
		}

		/* Compute the closest and furthest distances of nodes of current cell */
		dn = 1e38, df = 0.0;
		for (uil = ee = 0; ee < (1 << di); ee++) { /* For all grid points in the cube */
			double r;
			float *gt = gp + s->g.fhi[ee];	/* Pointer to cube vertex */
			
			if (!s->limiten || gt[-1] <= s->limitv)
				uil = 1;

			/* Update bounding box for this grid point */
			for (r = 0.0, f = 0; f < fdi; f++) {
				double tt = cc[f] - (double)gt[f];
				r += tt * tt;	
			}
//printf("~1 grid location %f %f %f rad %f\n",gt[0],gt[1],gt[2],sqrt(r));
			if (r < dn)
				dn = r;
			if (r > df)
				df = r;
		}
		/* Skip any fwd cells that are over the ink limit */
		if (!uil)
			continue;

		dn = sqrt(dn) - rr;
		df = sqrt(df) + rr;

//printf("~1 checking cell %d, near %f, far %f\n",i,dn,df);

		/* Skip any that have a closest distance larger that the lists */
		/* closest furthest distance. */
		if (dn > clfu) {
//printf("~1 skipping cell %d, near %f, far %f clfu %f\n",i,dn,df,clfu);
			continue;
		}

//printf("~1 adding cell %d\n",i);
		if (rp == NULL) {
			if ((nf = (nncell_nf *) rev_malloc(s, 6 * sizeof(nncell_nf))) == NULL)
				error("rspl malloc failed - nncell_nf list");
			INCSZ(s, 6 * sizeof(nncell_nf));
			if ((rp = (int *) rev_malloc(s, 6 * sizeof(int))) == NULL)
				error("rspl malloc failed - rev.grid entry");
			INCSZ(s, 6 * sizeof(int));
			*rpp = rp;
			rp[0] = 6;		/* Allocation */
			rp[1] = 4;		/* Next empty cell */
			rp[2] = 1;		/* Reference count */
			rp[3] = i;
			nf[3].n = dn;
			nf[3].f = df;
			rp[4] = -1;
		} else {
			int z = rp[1], ll = rp[0];
			if (z >= (ll-1)) {			/* Not enough space */
				INCSZ(s, ll * sizeof(nncell_nf));
				INCSZ(s, ll * sizeof(int));
				ll *= 2;
				if ((nf = (nncell_nf *) rev_realloc(s, nf, sizeof(nncell_nf) * ll)) == NULL)
					error("rspl realloc failed - nncell_nf list");
				if ((rp = (int *) rev_realloc(s, rp, sizeof(int) * ll)) == NULL)
					error("rspl realloc failed - rev.grid entry");
				*rpp = rp;
				rp[0] = ll;
			}
			rp[z] = i;
			nf[z].n = dn;
			nf[z++].f = df;
			rp[z] = -1;
			rp[1] = z;
		}

		if (df < clfu)
			clfu = df;
	}
//printf("~1 Current list is:\n");
//for (e = 3; rp[e] != -1; e++)
//printf(" %d: Cell %d near %f far %f\n",e,rp[e],nf[e].n,nf[e].f);

	/* Now filter out any cells that have a closest point that is further than */
	/* closest furthest point */ 
	{
		int z, w, ll = rp[0];

		/* For all the cells in the current list: */
		for (w = z = 3; rp[z] != -1; z++) {

			/* If the new cell nearest is greater than the existing cell closest, */
			/* then don't omit existing cell from the list. */
			if (clfu >= nf[z].n) {
				rp[w] = rp[z];
				nf[w].n = nf[z].n;
				nf[w].f = nf[z].f;
				w++;
			}
//else printf("~1 deleting cell %d because %f >= %f\n",rp[z],clfu, nf[z].f);
		}
		rp[w] = rp[z];
	}
//printf("~1 Current list is:\n");
//for (e = 3; rp[e] != -1; e++)
//printf(" %d: Cell %d near %f far %f\n",e,rp[e],nf[e].n,nf[e].f);
	free(nf);
//printf("~1 Done\n");
}

/* Return the pointer to the list of nearest fwd cells given */
/* the target output values. The pointer will be to the first */
/* index in the list (ie. list address + 3) */
/* Return NULL if none in list (out of gamut). */
static int *
calc_fwd_nn_cell_list(
	rspl *s,		/* this */
	double *v		/* Output values */
) {
	int f, fdi = s->fdi, ix;
	int **rpp;
	int rgres_1 = s->rev.res - 1;
	int mi[MXDO];

	if (s->rev.rev_valid == 0)
		init_revaccell(s);

	for (ix = 0, f = 0; f < fdi; f++) {
		double t = (v[f] - s->rev.gl[f])/s->rev.gw[f];
		mi[f] = (int)floor(t);			/* Grid coordinate */
		if (mi[f] < 0) 				/* Clip to reverse range, so we always return a result  */
			mi[f] = 0;
		else if (mi[f] > rgres_1)
			mi[f] = rgres_1;
		ix += mi[f] * s->rev.coi[f];	/* Accumulate reverse grid index */
	}
	rpp = s->rev.nnrev + ix;
	if (*rpp == NULL) {
		if (s->rev.fastsetup)
			fill_nncell(s, mi, ix);
		if (*rpp == NULL)
			rpp = s->rev.rev + ix;		/* fall back to in-gamut lookup */ 
	}
	if (*rpp == NULL)
		return NULL;
	return (*rpp) + 3;
}

/* =================================================== */
/* The cell and simplex solver top level routines */

static int add_lu_svd(simplex *x);
static int add_locus(schbase *b, simplex *x);
static int add_auxil_lu_svd(schbase *b, simplex *x);
static int within_simplex(simplex *x, double *p);
static void simplex_to_abs(simplex *x, double *in, double *out);

static int auxil_solve(schbase *b, simplex *x, double *xp);

/* ---------------------- */
/* Exact search functions */
/* Return non-zero if cell is acceptable */
static int exact_setsort(schbase *b, cell *c) {
	rspl *s = b->s;
	int f, fdi = s->fdi;
	double ss;

	DBG(("Reverse exact search, evaluate and set sort key on cell\n"));

	/* Check that the target lies within the cell bounding sphere */
	for (ss = 0.0, f = 0; f < fdi; f++) {
		double tt = c->bcent[f] - b->v[f];
		ss += tt * tt;
	}
	if (ss > c->bradsq) {
		DBG(("Cell rejected - %s outside sphere c %s rad %f\n",icmPdv(fdi,b->v),icmPdv(fdi,c->bcent),sqrt(c->bradsq)));
		return 0;
	}

	if (s->limiten != 0 && c->limmin > s->limitv) {
		DBG(("Cell is rejected - ink limit, min = %f, limit = %f\n",c->limmin,s->limitv));
		return 0;
	}

	/* Sort can't be used, because we return all solutions */
	c->sort = 0.0;

	DBG(("Cell is accepted\n"));

	return 1;
}

/* Compute a solution for a given sub-simplex (if there is one) */
/* Return 1 if search should be aborted */
static int exact_compute(schbase *b, simplex *x) {
	rspl *s     = b->s;
	int e, di = s->di, sdi  = x->sdi;
	int f, fdi  = s->fdi;
	int i;
	datai xp;	/* solution in simplex relative coord order */
	datai p;	/* absolute solution */
	int wsrv;	/* Within simplex return value */

	DBG(("\nExact: computing possible solution\n"));

#ifdef DEBUG
	/* Sanity check */
	if (sdi != fdi || sdi != di || x->efdi != fdi) {
		printf("di = %d, fdi = %d\n",di,fdi);
		printf("sdi = %d, efdi = %d\n",sdi,x->efdi);
		error("rspl exact reverse interp called with sdi != fdi, sdi != di, efdi != fdi");
		/* !!! could switch to SVD solution if di != fdi ?? !!! */
	}
#endif

	/* This may not be worth it here since it may not filter out */
	/* many more simplexes than the cube check did. */
	/* This is due to full dimension simplexes all sharing the main */
	/* diagonal axis. */

	/* Check that the target lies within the simplex bounding cube */
	for (f = 0; f < fdi; f++) {
		if (b->v[f] < x->min[f] || b->v[f] > x->max[f]) {
			DBG(("Simplex is rejected - bounding cube\n"));
			return 0;
		}
	}

	/* Create the LU decomp needed to exactly solve */
	if (add_lu_svd(x)) {
		DBG(("LU decomp was singular, skip simplex\n"));
		return 0;
	}

	/* Init the RHS B[] vector (note di == fdi) */
	for (f = 0; f < fdi; f++) {
		xp[f] = b->v[f] - x->v[di][f];
	}

	/* Compute the solution (in simplex space) */
	lu_backsub(x->d_u, sdi, (int *)x->d_w, xp);

	/* Check that the solution is within the simplex */
	if ((wsrv = within_simplex(x, xp)) == 0) {
		DBG(("Solution rejected because not in simplex\n"));
		return 0;
	}

	/* Convert solution from simplex relative to absolute space */
	simplex_to_abs(x, p, xp);

	/* Check if a very similiar input solution has been found before */
	for (i = 0; i < b->nsoln; i++) {
		double tt;
		for (e = 0; e < di; e++) {
			tt = b->cpp[i].p[e] - p[e];
			if (fabs(tt) > (2 * EPS))
				break;	/* Mismatch */
		}
		if (e >= di)	/* Found good match */
			break;
	}

	/* Probably alias caused by solution lying close to a simplex boundary */
	if (i < b->nsoln) {
		DBG(("Another solution has been found before - index %d\n",i));
		return 0;		/* Skip this, since betters been found before */
	}

	/* Check we haven't overflowed space */
	if (i >= b->mxsoln) {
		DBG(("Run out of space for new solution\n"));
		return 1;		/* Abort */
	}

	DBG(("######## Accepting new solution\n"));

	/* Put solution in place */
	for (e = 0; e < di; e++)
		b->cpp[i].p[e] = p[e];
	for (f = 0; f < fdi; f++)
		b->cpp[i].v[f] = b->v[f];	/* Assumed to be an exact solution */
	if (i == b->nsoln)
		b->nsoln++;
	if (wsrv == 2)					/* Is above (disabled) ink limit */
		b->iclip = 1;
	return 0;
}

/* -------------------------- */
/* Auxiliary search functions */
static int auxil_setsort(schbase *b, cell *c) {
	rspl *s = b->s;
	int f, fdi  = b->s->fdi;
	int ee, ixc = b->ixc;
	double ss, sort, nabove;

	DBG(("Reverse auxiliary search, evaluate and set sort key on cell\n"));

	if (b->s->di <= fdi) {	/* Assert */
		error("rspl auxiliary reverse interp called with di <= fdi (%d %d)", b->s->di, fdi);
	}

	/* Check that the target lies within the cell bounding sphere */
	for (ss = 0.0, f = 0; f < fdi; f++) {
		double tt = c->bcent[f] - b->v[f];
		ss += tt * tt;
	}
	if (ss > c->bradsq) {
		DBG(("Cell rejected - %s outside sphere c %s rad %f\n",icmPdv(fdi,b->v),icmPdv(fdi,c->bcent),sqrt(c->bradsq)));
		return 0;
	}

	if (s->limiten != 0 && c->limmin > s->limitv) {
		DBG(("Cell is rejected - ink limit, min = %f, limit = %f\n",c->limmin,s->limitv));
		return 0;
	}

	/* Check if this cell could possible improve b->idist */
	/* and compute sort key as the distance to auxilliary target */
	/* (We may have a non INF_DIST idist before commencing the */
	/* search if we already know that the auxiliary target is */
	/* within gamut - the usual usage case!) */
	for (sort = 0.0, nabove = ee = 0; ee < b->naux; ee++) {
		int ei = b->auxi[ee];
		double tt = (c->p[0][ei] + c->p[ixc][ei]) - b->av[ei];
		sort += tt * tt;
		if (c->p[ixc][ei] >= (b->av[ei] - EPS))		/* Could be above */
			nabove++;
	}

	if (b->flags & RSPL_MAXAUX && nabove < b->iabove) {
		DBG(("Doesn't contain solution that has as many aux above auxiliary goal\n"));
		return 0;
	}
	if (!(b->flags & RSPL_MAXAUX) || nabove == b->iabove) {
		for (ee = 0; ee < b->naux; ee++) {
			int ei = b->auxi[ee];
			if (c->p[0][ei]   >= (b->av[ei] + b->idist)
			 || c->p[ixc][ei] <= (b->av[ei] - b->idist)) {
				DBG(("Doesn't contain solution that will be closer to auxiliary goal\n"));
				return 0;
			}
		}
	}
	c->sort = sort + 0.01 * ss;

	if (c->ix == b->pauxcell)
		c->sort = -1.0;			/* Put previous calls solution cell at top of sort list */

	DBG(("Cell is accepted\n"));
	return 1;
}

/* Re-check whether it's worth searching cell */
static int auxil_check(schbase *b, cell *c) {
	int ee, ixc = b->ixc, nabove;

	DBG(("Reverse auxiliary search, re-check cell\n"));

	/* Check if this cell could possible improve b->idist */
	/* and compute sort key as the distance to auxilliary target */

	for (nabove = ee = 0; ee < b->naux; ee++) {
		int ei = b->auxi[ee];
		if (c->p[ixc][ei] >= (b->av[ei] - EPS))		/* Could be above */
			nabove++;
	}

	if (b->flags & RSPL_MAXAUX && nabove < b->iabove) {
		DBG(("Doesn't contain solution that has as many aux above auxiliary goal\n"));
		return 0;
	}
	if (!(b->flags & RSPL_MAXAUX) || nabove == b->iabove) {
		for (ee = 0; ee < b->naux; ee++) {
			int ei = b->auxi[ee];
			if (c->p[0][ei]   >= (b->av[ei] + b->idist)
			 || c->p[ixc][ei] <= (b->av[ei] - b->idist)) {
				DBG(("Doesn't contain solution that will be closer to auxiliary goal\n"));
				return 0;
			}
		}
	}
	DBG(("Cell is still ok\n"));
	return 1;
}

/* Compute a solution for a given simplex (if there is one) */
/* Return 1 if search should be aborted */
static int auxil_compute(schbase *b, simplex *x) {
	rspl *s     = b->s;
	int e, di   = s->di;
	int f, fdi  = s->fdi;
	datai xp;		/* solution in simplex relative coord order */
	datai p;		/* absolute solution */
	double idist;	/* Auxiliary input distance */
	int wsrv;		/* Within simplex return value */
	int nabove;		/* Number above aux target */

	DBG(("\nAuxil: computing possible solution\n"));

#ifdef DEBUG
	{
	unsigned int sum = 0;
	for (f = 0; f <= x->sdi; f++)
		sum += x->vix[f];
	printf("Simplex of cell ix %d, sum 0x%x, sdi = %d, efdi = %d\n",x->ix, sum, x->sdi, x->efdi);
	printf("Target val %s\n",icmPdv(fdi, b->v));
	for (f = 0; f <= x->sdi; f++) {
		int ix = x->vix[f], i;
		float *fcb = s->g.a + ix * s->g.pss;	/* Pointer to base float of fwd cell */
		printf("Simplex vtx %d [cell ix %d] val %s\n",f,ix,icmPfv(fdi, fcb));
	}
	}
#endif

	/* Check that the target lies within the simplex bounding cube */
	for (f = 0; f < fdi; f++) {
		if (b->v[f] < x->min[f] || b->v[f] > x->max[f]) {
			DBG(("Simplex is rejected - bounding cube\n"));
			return 0;
		}
	}

	/* Check if this cell could possible improve b->idist */
	for (nabove = e = 0; e < b->naux; e++) {
		int ei = b->auxi[e];					/* pmin/max[] is indexed in input space */
		if (x->pmax[ei] >= (b->av[ei] - EPS))	/* Could be above */
			nabove++;
	}
	if ((b->flags & RSPL_MAXAUX) && nabove < b->iabove) {
		DBG(("Simplex doesn't contain solution that has as many aux above auxiliary goal\n"));
		return 0;
	}
	if (!(b->flags & RSPL_MAXAUX) || nabove == b->iabove) {
		for (nabove = e = 0; e < b->naux; e++) {
			int ei = b->auxi[e];					/* pmin/max[] is indexed in input space */
			if (x->pmin[ei] >= (b->av[ei] + b->idist)
			 || x->pmax[ei] <= (b->av[ei] - b->idist)) {
				DBG(("Simplex doesn't contain solution that will be closer to auxiliary goal\n"));
				return 0;
			}
		}
	}

//printf("~~ About to create svd decomp\n");
	/* Create the SVD or LU decomp needed to compute solution or locus */
	if (add_lu_svd(x)) {
		DBG(("SVD decomp failed, skip simplex\n"));
		return 0;
	}

//printf("~~ About to solve locus for aux target\n");
	/* Now solve for locus parameter that minimises */
	/* distance to auxliary target. */
	if ((wsrv = auxil_solve(b, x, xp)) == 0) {
		DBG(("Target auxiliary along locus is outside simplex,\n"));
		DBG(("or computation failed, skip simplex\n"));
		return 0;
	}

//printf("~~ About to convert solution to absolute space\n");
	/* Convert solution from simplex relative to absolute space */
	simplex_to_abs(x, p, xp);

	DBG(("Got solution at %s\n", icmPdv(di,p)));

//printf("~~ soln = %f %f %f %f\n",p[0],p[1],p[2],p[3]);
//printf("~~ About to compute auxil distance\n");
	/* Compute distance to auxiliary target */
	for (idist = 0.0, nabove = e = 0; e < b->naux; e++) {
		int ei = b->auxi[e];
		double tt = b->av[ei] - p[ei];
		idist += tt * tt;
		if (p[ei] >= (b->av[ei] - EPS))
			nabove++;
	}
	idist = sqrt(idist);
//printf("~1 idist %f, nabove %d\n",idist, nabove);
//printf("~1 best idist %f, best iabove %d\n",b->idist, b->iabove);

	/* We want the smallest error from auxiliary target */
	if (b->flags & RSPL_MAXAUX) {
		if (nabove < b->iabove || (nabove == b->iabove && idist >= b->idist)) {
			DBG(("nsoln %d, nabove %d, iabove %d, idist = %f, better solution has been found before\n",b->nsoln, nabove, b->iabove, idist));
			return 0;
		}
	} else {
		if (idist >= b->idist) {	/* Equal or worse auxiliary solution */
			DBG(("nsoln %d, idist = %f, better solution has been found before\n",b->nsoln,idist));
			return 0;
		}
	}

	/* Solution is accepted */
	DBG(("######## Accepting new solution with nabove %d <= iabove %d and idist %f <= %f\n",nabove,b->iabove,idist,b->idist));
	for (e = 0; e < di; e++)
		b->cpp[0].p[e] = p[e];
	for (f = 0; f < fdi; f++)
		b->cpp[0].v[f] = b->v[f];	/* Assumed to be an exact solution */
	b->idist = idist;
	b->iabove = nabove;
	b->nsoln = 1;
	b->pauxcell = x->ix;
	if (wsrv == 2)					/* Is above (disabled) ink limit */
		b->iclip = 1;

	return 0;
}

/* ------------------------------------ */
/* Locus range search functions */

static int locus_setsort(schbase *b, cell *c) {
	rspl *s = b->s;
	int f, fdi  = s->fdi;
	int lxi = b->lxi;	/* Auxiliary we are finding min/max of */
	int ixc = b->ixc;
	double sort, ss;

	DBG(("Reverse locus evaluate and set sort key on cell\n"));

#ifdef DEBUG
	if (b->s->di <= fdi) {	/* Assert ~1 */
		error("rspl auxiliary locus interp called with di <= fdi");
	}
#endif /* DEBUG */

	/* Check that the target lies within the cell bounding sphere */
	for (ss = 0.0, f = 0; f < fdi; f++) {
		double tt = c->bcent[f] - b->v[f];
		ss += tt * tt;
	}
	if (ss > c->bradsq) {
		DBG(("Cell rejected - %s outside sphere c %s rad %f\n",icmPdv(fdi,b->v),icmPdv(fdi,c->bcent),sqrt(c->bradsq)));
		return 0;
	}

	if (s->limiten != 0 && c->limmin > s->limitv) {
		DBG(("Cell is rejected - ink limit, min = %f, limit = %f\n",c->limmin,s->limitv));
		return 0;
	}

	/* Check if this cell could possible improve the locus min/max */
	if (b->asegs == 0) {	/* If we aren't find all segments of the locus */
		if (c->p[0][lxi] >= b->min && c->p[ixc][lxi] <= b->max ) {
			DBG(("Doesn't contain solution that will expand the locus\n"));
			return 0;
		}
	}

	/* Compute sort index from average of auxiliary values */
	sort = (c->p[0][b->lxi] + c->p[ixc][b->lxi]);
	
	c->sort = sort + 0.01 * ss;

	DBG(("Cell is accepted\n"));
	return 1;
}

/* Re-check whether it's worth searching simplexes */
static int locus_check(schbase *b, cell *c) {
	int lxi = b->lxi;	/* Auxiliary we are finding min/max of */
	int ixc = b->ixc;

	DBG(("Reverse locus re-check\n"));

	/* Check if this cell could possible improve the locus min/max */
	if (b->asegs == 0) {	/* If we aren't find all segments of the locus */
		if (c->p[0][lxi] >= b->min && c->p[ixc][lxi] <= b->max ) {
			DBG(("Doesn't contain solution that will expand the locus\n"));
			return 0;
		}
	}

	DBG(("Cell is still ok\n"));
	return 1;
}

static int auxil_locus(schbase *b, simplex *x);

/* We expect to be given a sub-simplex with no DOF, to give an exact solution */
static int locus_compute(schbase *b, simplex *x) {
	rspl *s  = b->s;
	int f, fdi  = s->fdi;
	int lxi  = b->lxi;	/* Auxiliary we are finding min/max of */

	DBG(("\nLocus: computing possible solution\n"));

#ifdef DEBUG
	{
	unsigned int sum = 0;
	for (f = 0; f <= x->sdi; f++)
		sum += x->vix[f];
	printf("Simplex of cell ix %d, sum 0x%x, sdi = %d, efdi = %d\n",x->ix, sum, x->sdi, x->efdi);
	printf("Target val %s\n",icmPdv(fdi, b->v));
	for (f = 0; f <= x->sdi; f++) {
		int ix = x->vix[f], i;
		float *fcb = s->g.a + ix * s->g.pss;	/* Pointer to base float of fwd cell */
		double v[MXDO];
		printf("Simplex vtx %d [cell ix %d] val %s\n",f,ix,icmPfv(fdi, fcb));
	}
	}
#endif

	/* Check that the target lies within the simplex bounding cube */
	for (f = 0; f < fdi; f++) {
		if (b->v[f] < x->min[f] || b->v[f] > x->max[f]) {
			DBG(("Simplex is rejected - bounding cube\n"));
			return 0;
		}
	}

	/* Check if simplex could possible improve the locus min/max */
	if (b->asegs == 0) {	/* If we aren't find all segments of the locus */
		if (x->pmin[lxi] >= b->min && x->pmax[lxi] <= b->max ) {
			DBG(("Simplex doesn't contain solution that will expand the locus\n"));
			return 0;
		}
	}

//printf("~~ About to create svd decomp\n");
	/* Create the SVD decomp needed to compute solution extreme points */
	if (add_lu_svd(x)) {
		DBG(("SVD decomp failed, skip simplex\n"));
		return 0;
	}

//printf("~~ About to solve locus for aux extremes\n");
	/* Now solve for locus parameter that are at the extremes */
	/* of the axiliary we are interested in. */
	if (!auxil_locus(b, x)) {
		DBG(("Target auxiliary is outside simplex,\n"));
		DBG(("or computation failed, skip simplex\n"));
		return 0;
	}

	return 0;
}

/* ------------------- */
/* Vector clipping search functions */
static int clipv_setsort(schbase *b, cell *c) {
	rspl *s = b->s;
	int f, fdi  = s->fdi;
	double ss, dp;

	DBG(("Reverse clipping search evaluate cell\n"));

//printf("~~sphere center = %f %f %f, radius %f\n",c->bcent[0],c->bcent[1],c->bcent[2],sqrt(c->bradsq));
	/* Check if the clipping line intersects the bounding sphere */
	/* First compute dot product cdir . (bcent - v) */
	/* == distance to center of sphere in direction of clip vector */
	for (dp = 0.0, f = 0; f < fdi; f++) {
		dp += b->ncdir[f] * (c->bcent[f] - b->v[f]);
	}

	if (s->limiten != 0 && c->limmin > s->limitv) {
		DBG(("Cell is rejected - ink limit, min = %f, limit = %f\n",c->limmin,s->limitv));
		return 0;
	}

//printf("~~ dot product = %f\n",dp);
	/* Now compute closest distance to sphere center */
	for (ss = 0.0, f = 0; f < fdi; f++) {
		double tt = b->v[f] + dp * b->ncdir[f] - c->bcent[f];
		ss += tt * tt;
	}

//printf("~~ distance to sphere center = %f\n",sqrt(ss));
	if (ss > c->bradsq) {
		DBG(("Cell is rejected - wrong direction or bounding sphere\n"));
		return 0;
	}
	c->sort = dp;		/* May be -ve if beyond clip target point ? */

	DBG(("Cell is accepted\n"));
	return 1;
}

/* Clipping check functions */
/* Note that we don't bother with this check in setsort(), */
/* because we assume that nothing will set a small cdist */
/* before the search commences (unlike auxil). */
/* Note that line search loop exits on finding any solution. */
static int clipv_check(schbase *b, cell *c) {

	DBG(("Reverse clipping re-check\n"));

	if (b->cdist < INF_DIST) {	/* If some clip solution has been found */
		int f, fdi = b->s->fdi;
		double dist;
		/* Compute a conservative "best possible solution clip distance" */
		for (dist = 0.0, f = 0; f < fdi ; f++) {
			double tt = (c->bcent[f] - b->v[f]);
			dist += tt * tt;
		}
		dist = sqrt(dist); /* Target distance to bounding */

		if (dist >= (c->brad + b->cdist)) {	/* Equal or worse clip solution */
			DBG(("Cell best possible solution worse than current\n"));
			return 0;
		}
	}

	DBG(("Cell is still ok\n"));
	return 1;
}

static int vnearest_clip_solve(schbase *b, simplex *x, double *xp, double *xv, double *err);

/* Compute a clip solution */
static int clipv_compute(schbase *b, simplex *x) {
	rspl   *s  = b->s;
	int f, fdi = s->fdi;
	datai p;				/* Input space solution */
	datao v;				/* Output space solution */
	double err;				/* output error of solution */
	int wsrv;	/* Within simplex return value */

	DBG(("Clips: computing possible solution\n"));

	/* Compute a solution value */
	if ((wsrv = vnearest_clip_solve(b, x, p, v, &err)) == 0) {
		DBG(("Doesn't contain a solution\n"));
		return 0;
	}

	/* We want the smallest clip error */
	/* (Should we reject points in -ve vector direction ??) */
	if (err >= b->cdist) {	/* Equal or worse clip solution */
		DBG(("better solution has been found before\n"));
		return 0;
	}

	simplex_to_abs(x, b->cpp[0].p, p);	/* Convert to abs. space & copy */

	DBG(("######## Accepting new clipv solution with error %f\n",err));
#ifdef DEBUG
	if (s->limiten != 0) {
		DBG(("######## Ink value = %f, limit %f\n",get_limitv(b, x->ix, NULL, b->cpp[0].p), s->limitv));
	}
#endif

	/* Put solution in place */
	for (f = 0; f < fdi; f++)
		b->cpp[0].v[f] = v[f];
	b->cdist = err;
	b->nsoln = 1;
	if (wsrv == 2)					/* Is above (disabled) ink limit */
		b->iclip = 1;

	return 0;
}

/* ------------------- */
/* Nearest clipping search functions */
static int clipn_setsort(schbase *b, cell *c) {
	rspl *s = b->s;
	int f, fdi  = s->fdi;
	double ss;

	DBG(("Reverse nearest clipping search evaluate cell\n"));

	/* Compute a conservative "best possible solution clip distance" */
	for (ss = 0.0, f = 0; f < fdi ; f++) {
		double tt = (c->bcent[f] - b->v[f]);
		ss += tt * tt;
	}
	ss = sqrt(ss); /* Target distance to bounding sphere */
	ss -= c->brad;
	if (ss < 0.0)
		ss = 0.0;

	/* Check that the cell could possibly improve the solution */
	if (b->cdist < INF_DIST) {	/* If some clip solution has been found */
		if (ss >= b->cdist) {	/* Equal or worse clip solution */
			DBG(("Cell best possible solution worse than current\n"));
			return 0;
		}
	}

	if (s->limiten != 0 && c->limmin > s->limitv) {
		DBG(("Cell is rejected - ink limit, min = %f, limit = %f\n",c->limmin,s->limitv));
		return 0;
	}

	c->sort = ss;		/* May be -ve if beyond clip target point ? */

	DBG(("Cell is accepted\n"));
	return 1;
}

/* Clipping check functions */
static int clipn_check(schbase *b, cell *c) {

	DBG(("Reverse nearest clipping re-check\n"));

	if (b->cdist < INF_DIST) {	/* If some clip solution has been found */
		/* re-use sort value, best possible distance to solution */
		if (c->sort >= b->cdist) {	/* Equal or worse clip solution */
			DBG(("Cell best possible solution worse than current\n"));
			return 0;
		}
	}

	DBG(("Cell is still ok\n"));
	return 1;
}

static int nnearest_clip_solve(schbase *b, simplex *x, double *xp, double *xv, double *err);

/* Compute a clip solution */
static int clipn_compute(schbase *b, simplex *x) {
	rspl   *s  = b->s;
	int f, fdi = s->fdi;
	datai p;				/* Input space solution */
	datao v;				/* Output space solution */
	double err;				/* output error of solution */
	int wsrv;	/* Within simplex return value */

	DBG(("Clipn: computing possible solution  simplex %d, sdi = %d, efdi = %d\n",x->si,x->sdi,x->efdi));

	/* Compute a solution value */
	if ((wsrv = nnearest_clip_solve(b, x, p, v, &err)) == 0) {
		DBG(("Doesn't contain a solution\n"));
		return 0;
	}

	/* We want the smallest clip error */
	if (err >= b->cdist) {	/* Equal or worse clip solution */
		DBG(("better solution has been found before\n"));
		return 0;
	}

	DBG(("######## Accepting new clipn solution with error %f\n",err));

	simplex_to_abs(x, b->cpp[0].p, p);	/* Convert to abs. space & copy */

	/* Put solution in place */
	for (f = 0; f < fdi; f++)
		b->cpp[0].v[f] = v[f];
	b->cdist = err;
	b->nsoln = 1;
	if (wsrv == 2)					/* Is above (disabled) ink limit */
		b->iclip = 1;

	return 0;
}

/* -------------------------------------------------------- */
/* Cell/simplex solver middle level code */

/* Find the point on this sub-simplexes solution locus that is */
/* closest to the target auxiliary values, and return it in xp[] */
/* Return zero if this point canot be calculated, */
/* or it lies outside the simplex. */
/* Return 1 normally, and 2 if the solution would be over the ink limit */
static int
auxil_solve(
schbase *b,
simplex *x,
double *xp		/* Return solution xp[sdi] */
) {
	rspl *s = b->s;
	int ee, e, di = s->di, sdi = x->sdi; 
	int f, efdi = x->efdi; 
	int dof = sdi-efdi;			 /* Degree of freedom of simplex locus */
	int *icomb = x->psxi->icomb; /* abs -> simplex coordinate translation */
	double auxt[MXRI];			/* Simplex relative auxiliary targets */
	double bb[MXRI];
	int wsrv;	/* Within simplex return value */

	DBG(("axuil_solve called\n"));

	if (dof < 0)
		error("Error - auxil_solve got sdi < efdi (%d < %d) - don't know how to handle this",sdi, efdi);

	/* If there is no locus, compute an exact solution */
	if (dof == 0) {
		DBG(("axuil_solve dof = zero\n"));

		/* Init the RHS B[] vector (note sdi == efdi) */
		for (f = 0; f < efdi; f++) {
			xp[f] = b->v[f] - x->v[sdi][f];
		}

		/* Compute the solution (in simplex space) */
		lu_backsub(x->d_u, sdi, (int *)x->d_w, xp);

		if ((wsrv = within_simplex(x, xp)) != 0) {
			DBG(("Got solution at %s\n", icmPdv(sdi,xp)));
			return wsrv;				/* OK, got solution */
		}

		DBG(("No solution (not within simplex)\n"));
		return 0;
	}

	/* There is a locus, so find solution nearest auxiliaries */

	/* Compute locus for target function values (if sdi > efdi) */
	if (add_locus(b, x)) {
		DBG(("Locus computation failed, skip simplex\n"));
		return 0;
	}

	/* Convert aux targets from absolute space to simplex relative */
	for (e = 0; e < di; e++) {	/* For abs coords */
		int ei = icomb[e];		/* Simplex coord */

		if (ei >= 0 &&  b->auxm[e] != 0) {
			auxt[ei] = (b->av[e] - x->p0[e])/s->g.w[e];	/* Only sets those needed */
		}
	}

	if (dof == 1 && b->naux == 1) {		/* Special case, because it's common and easy! */
		int ei = icomb[b->auxi[0]];		/* Simplex relative auxiliary index */
		double tt;

		DBG(("axuil_solve dof = naux = 1\n"));
		if (ei < 0)
			return 0;					/* Not going to find solution */
		if ((tt = x->lo_l[ei][0]) == 0.0)
			return 0;
		tt = (auxt[ei] - x->lo_bd[ei])/tt;	/* Parameter solution for target auxiliary */

		/* Back substitute parameter */
		for (e = 0; e < sdi; e++) {
			xp[e] = x->lo_bd[e] + tt * x->lo_l[e][0];
		}
		if ((wsrv = within_simplex(x, xp)) != 0) {
			DBG(("Got solution %s\n",icmPdv(di,xp)));
			return wsrv;				/* OK, got solution */
		}
		DBG(("No solution (not within simplex)\n"));
		return 0;
	}

	/* Compute the locus decompositions needed (info #5) */
	if (add_auxil_lu_svd(b, x)) {	/* Will set x->naux */
		DBG(("LU/SVD decomp failed\n"));
		return 0;
	}

	/* Setup B[], equation RHS  */
	for (e = ee = 0; ee < b->naux; ee++) {
		int ei = icomb[b->auxi[ee]];		/* Simplex relative auxiliary index */
		if (ei >= 0)						/* Usable auxiliary on this sub simplex */ 
			bb[e++] = auxt[ei] - x->lo_bd[ei];
	}
	if (e != x->naux)	/* Assert */
		error("Internal error - auxil_solve got mismatching number of auxiliaries");

	if (x->naux == dof) {			/* Use LU decomp to solve */
		DBG(("axuil_solve using LU\n"));
		lu_backsub(x->ax_u, dof, (int *)x->ax_w, bb);

	} else if (x->naux > 0) {	/* Use SVD to solve least squares */
		DBG(("axuil_solve using SVD\n"));
		svdbacksub(x->ax_u, x->ax_w, x->ax_v, bb, bb, x->naux, dof);

	} else {	/* x->naux == 0 */
		DBG(("axuil_solve  naux = 0\n"));
		for (f = 0; f < dof; f++)
			bb[f] = 0.0;		/* Use base solution ?? */
	}

	/* Now back substitute the locus parameters */
	/* to calculate the solution point (in simplex space) */
	for (e = 0; e < sdi; e++) {
		double tt;
		for (tt = 0.0, f = 0; f < dof; f++) {
			tt += bb[f] * x->lo_l[e][f];
		}
		xp[e] = x->lo_bd[e] + tt;
	}

	if ((wsrv = within_simplex(x, xp)) != 0) {
		DBG(("Got solution %s\n",icmPdv(di,xp)));
		return wsrv;				/* OK, got solution */
	}
	DBG(("No solution (not within simplex)\n"));
	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the min/max values for the current auxiliary of interest. */
/* Return zero if this point canot be calculated, */
/* or it lies outside the simplex. */
/* Return 1 normally, 2 if it would be outside the simplex if limting was enabled */
/* We expect to get a sub-simplex that will give an exact solution. */
static int
auxil_locus(
schbase *b,
simplex *x
) {
	rspl *s = b->s;
	int sdi = x->sdi; 
	int f, efdi = x->efdi; 
	double pp[MXRI];
	int wsrv;	/* Within simplex return value */

	DBG(("axuil_locus called\n"));

	if (sdi != efdi)
		warning("Internal error - auxil_locus got sdi != efdi (%d < %d)",sdi, efdi);

	/* Init the RHS B[] vector (note sdi == efdi) */
	for (f = 0; f < efdi; f++) {
		pp[f] = b->v[f] - x->v[sdi][f];
	}

	/* Compute the solution (in simplex space) */
	lu_backsub(x->d_u, sdi, (int *)x->d_w, pp);

	/* Check that the solution is within the simplex */
	if ((wsrv = within_simplex(x, pp)) != 0) {
		double xval;
		int lxi = b->lxi;	/* Auxiliary we are finding min/max of (Abs space) */
		int xlxi = x->psxi->icomb[lxi];	/* Auxiliary we are finding min/max of (simplex space) */

		DBG(("Got locus solution within simplex\n"));

		/* Compute auxiliary value for this solution (absolute space) */
		xval = x->p0[lxi];
		if (xlxi >= 0)				/* Simplex param value */
			xval += s->g.w[lxi] * pp[xlxi];
		else if (xlxi == -2)		/* 1 value */
			xval += s->g.w[lxi];
									/* Else 0 value */

		if (b->asegs != 0) {		/* Tracking auxiliary segments */
			if (b->axisln >= b->axislz) {	/* Need some more space in list */
				if (b->axislz == 0) {
					b->axislz = 10;
					if ((b->axisl = (axisec *)rev_malloc(s, b->axislz * sizeof(axisec))) == NULL)
						error("rev: malloc failed - Auxiliary intersect list size %d",b->axislz);
					INCSZ(b->s, b->axislz * sizeof(axisec));
				} else {
					INCSZ(b->s, b->axislz * sizeof(axisec));
					b->axislz *= 2;
					if ((b->axisl = (axisec *)rev_realloc(s, b->axisl, b->axislz * sizeof(axisec)))
					    == NULL)
						error("rev: realloc failed - Auxiliary intersect list size %d",b->axislz);
				}
			}
			b->axisl[b->axisln].xval = xval;
			b->axisl[b->axisln].nv = x->sdi + 1;
			for (f = 0; f <= x->sdi; f++) {
				b->axisl[b->axisln].vix[f] = x->vix[f];
			}
			b->axisln++;
		}

#ifdef DEBUG
		if (xval >= b->min && xval <= b->max)
			DBG(("auxil_locus: solution %f doesn't improve on min %f, max %f\n",xval,b->min,b->max));
#endif
		/* If this solution is expands the min or max, save it */
		if (xval < b->min) {
			DBG(("######## Improving minimum to %f\n",xval));
			b->min = xval;
			b->plmincell = x->ix;
		}
		if (xval > b->max) {
			DBG(("######## Improving maximum to %f\n",xval));
			b->max = xval;
			b->plmaxcell = x->ix;
		}
	} else {
		DBG(("Solution wasn't within the simplex\n"));
		return 0;
	}

	return wsrv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - */
/* Find the point on the clip line locus and simplexes */
/* valid surface, that is closest to the target output value. */
/* We expect to be given a sub simplex with sdi = fdi-1, and efdi = fdi */
/* or a limit sub-simplex with sdi = fdi, and efdi = fdi+1 */
/* Return zero if solution canot be calculated, */
/* return 1 normally, 2 if solution would be above the (disabled) ink limit */
static int
vnearest_clip_solve(
schbase *b,
simplex *x,
double *xp,		/* Return solution (simplex parameter space) */
double *xv,		/* Return solution (output space) */
double *err		/* Output error distance at solution point */
) {
	rspl *s = b->s;
	int e, sdi = x->sdi; 
	int f, fdi = s->fdi, efdi = x->efdi; 
	int g;
	int wsrv;	/* Within simplex return value */

	double *ta[MXRO], TA[MXRO][MXRO];
	double tb[MXRO];

	DBG(("Vector nearest clip solution called, cell %d, splx %d\n", x->ix, x->si));

	/* Setup temporary matricies */
	for (f = 0; f < sdi; f++) {
		ta[f] = TA[f];
	}

	/* Substitute simplex equation for output values V */
	/* in terms of sub-simplex parameters P, */
	/* into  clip line implicit equation in V, to give */
	/* clip line simplex implicit equation in terms of P (simplex input space) */
	/* If this is a limit sub-simlex, the ink limit part of the clip vector */
	/* equations will be used. */

	/* LHS: ta[sdi][sdi] = cla[sdi][efdi] * vv[efdi][sdi] */
	/* RHS: tb[sdi] = clb[sdi] - cla[sdi][efdi] * vv_di[efdi] */
	for (f = 0; f < sdi; f++) {
		double tt;
		for (e = 0; e < sdi; e++) {
			for (tt = 0.0, g = 0; g < efdi; g++)
				tt += b->cla[f][g] * (x->v[e][g] - x->v[e+1][g]);
			ta[f][e] = tt;
		}
		for (tt = 0.0, g = 0; g < efdi; g++)
			tt += b->cla[f][g] * x->v[sdi][g];
		tb[f] = b->clb[f] - tt;
	}

	/* Compute the solution */
	if (gen_solve_se(ta, tb, sdi, sdi)) {
		DBG(("Equation solution failed!\n"));
		return 0;		/* No solution */
	}

	/* Check that the solution is within the simplex */
	if ((wsrv = within_simplex(x, tb)) != 0) {
		double dist;				/* distance to clip target */

		DBG(("Got solution within simplex %s\n", icmPdv(sdi,tb)));

		/* Compute the output space solution point */
		for (f = 0; f < fdi; f++) {
			double tt = 0.0;
			for (e = 0; e < sdi; e++) {
				tt += (x->v[e][f] - x->v[e+1][f]) * tb[e];
			}
			xv[f] = tt + x->v[sdi][f];
		}

		/* Copy to return array */
		for (e = 0; e < sdi; e++)
			xp[e] = tb[e];

		/* Compute distance to clip target */
		for (dist = 0.0, f = 0; f < fdi ; f++) {
			double tt = (b->v[f] - xv[f]);
			dist += tt * tt;
		}
		DBGV(("Vector clip output soln: ",fdi," %f", xv, "\n"));

		/* Return the solution in xp[]m xv[] and *err */
		*err = sqrt(dist);

		DBG(("Vector clip returning a solution with error %f\n",*err));
		return wsrv;
	}

	DBG(("Vector clip solution not in simplex\n"));
	return 0;		/* No solution */
}

/* - - - - - - - - - - - - - - - - - - - - - - - */
/* Find the point on the simplexes valid surface, that is closest */
/* to the target output value. */
/* We expect to be given a sub simplex with sdi = fdi-1, and efdi = fdi */
/* or a limit sub-simplex with sdi = fdi, and efdi = fdi+1 */
/* Return zero if solution canot be calculated, */
/* return 1 normally, 2 if solution would be above the (disabled) ink limit */
static int
nnearest_clip_solve(
schbase *b,
simplex *x,
double *xp,		/* Return solution (simplex parameter space) */
double *xv,		/* Return solution (output space) */
double *err		/* Output error distance at solution point */
) {
	rspl *s = b->s;
	int e, sdi = x->sdi; 
	int f, fdi = s->fdi, efdi = x->efdi; 
	double tb[MXRO];		/* RHS & Parameter solution */
	double dist;			/* distance to clip target */
	int wsrv = 0;			/* Within simplex return value */

	DBG(("Nearest clip solution called, cell %d, splx %d\n", x->ix, x->si));

	if (sdi == 0) {		/* Solution is vertex */
		wsrv = 1;
		for (f = 0; f < efdi; f++)
			xv[f] = x->v[sdi][f]; 		/* Copy vertex value */
		if (x->v[sdi][fdi] > s->limitv) {
			if (s->limiten) 			/* Needed when limiten == 0 */
				return 0;				/* Over ink limit - no good */
			wsrv = 2;					/* Would be over */
		}
		DBG(("Got assumed vertex solution\n"));
	} else {
#ifdef NEVER	/* Don't specialise ink limit version - use INKSCALE fudge instead */
		if (!(x->flags & SPLX_CLIPSX)) {	/* Not an ink limited plane simplex */
		
#endif
			/* Create the SVD decomp needed for least squares solution */
			if (add_lu_svd(x)) {
				DBG(("SVD decomp failed, skip simplex\n"));
				return 0;
			}
		
			/* Setup RHS to solve */
			for (f = 0; f < efdi; f++)
				tb[f] = b->v[f] - x->v[sdi][f]; 

			/* Find least squares solution */
			svdbacksub(x->d_u, x->d_w, x->d_v, tb, tb, efdi, sdi);
	
			/* Check that the solution is within the simplex */
			if ((wsrv = within_simplex(x, tb)) == 0) {
				DBG(("Nearest clip solution not in simplex\n"));
				return 0;		/* No solution */
			}
	
			DBG(("Got solution within simplex %s\n",icmPdv(sdi,tb)));
	
			/* Compute the output space solution point */
			for (f = 0; f < fdi; f++) {
				double tt = 0.0;
				for (e = 0; e < sdi; e++) {
					tt += (x->v[e][f] - x->v[e+1][f]) * tb[e];
				}
				xv[f] = tt + x->v[sdi][f];
			}
#ifdef NEVER /* ~~1 Haven't figured out equations to make this a special case. */
			 /* Content to use INKSCALE fudge and rely on SVD least squares. */
		} else {
			/* We can't use the given equations, because we want the solution */
			/* to lie exactly on the ink limit plane, and be least squares to the */
			/* other target parameters. */
			/* Extract the ink limit parameters, and transform them into */
			/* a parameterised surface for this simplex. */
			/* Substitute the ink plane equation into the remaining target */
			/* parameter equations, and solve for least squares. */

		}
#endif
	}

	/* Copy to return array */
	for (e = 0; e < sdi; e++)
		xp[e] = tb[e];

	/* Compute distance to clip target */
	for (dist = 0.0, f = 0; f < fdi ; f++) {
		double tt = (b->v[f] - xv[f]);
		dist += tt * tt;
	}
	DBGV(("Nearest clip output soln: ",fdi," %f", xv, "\n"));

	/* Return the solution in xp[]m xv[] and *err */
	*err = sqrt(dist);

	DBG(("Nearest clip returning a solution with error %f\n",*err));
	return wsrv;
}


#ifdef NEVER
/* Utility to convert an implicit ink limit plane equation */
/* (held at the end of the simplex output value equations), */
/* into a parameterized surface equation. */
static void
compute_param_limit_surface(
schbase *b,
simplex *x
) {
	rspl *s = b->s;
	int ff, f, fdi = s->fdi;
	int i, p;
	double lgst;

double st[MXRO],	/* Start point */
double de[MXRO]		/* Delta */
	DBG(("Computing clipping line implicit equation, dim = %d\n", fdi));
	
	/* Pick a pivot element - the smallest */
	for (lgst = -1.0, p = -1, f = 0; f < fdi; f++) {
		double tt = de[f];
		b->cdir[f] = tt;		/* Stash this away */
		tt = fabs(tt);
		if (tt > lgst) {
			lgst = tt;
			p = f;
		}
	}
	if (p < 0)	/* Shouldn't happen */
		error("rspl rev, internal, trying to cope with zero length clip line\n");
	
	if (b->cla == NULL)
		b->cla = dmatrix(0, fdi-1, 0, fdi);	/* Allow for ink limit supliment */

	for (i = ff = 0;  ff < fdi; ff++) {	/* For the input rows */
		if (ff == p) {
			continue;					/* Skip pivot row */
		}
		for (f = 0; f < fdi; f++) {		/* For input & output columns */
			if (f == p) {
				b->cla[i][f] = -de[ff];	/* Last column is -ve delta value */
			} else if (f == ff) {
				b->cla[i][f] = de[p];	/* Diagonal is pivot value */
			} else {
				b->cla[i][f] = 0.0;		/* Else zero */
			}
		}
		b->clb[i] = de[p] * st[ff] - de[ff] * st[p];
		i++;
	}

	/* Add ink limit target equation - */
	/* interpolated ink value == target */
	if (s->limitf != NULL) {
		for (i = 0;  i < (fdi-1); i++)
			b->cla[i][fdi] = 0.0;

		for (f = 0; f < fdi; f++) 
			b->cla[fdi-1][f] = 0.0;
		
		b->cla[fdi-1][fdi] = 1.0;
		b->clb[fdi-1] = s->limitv;
	}

#ifdef NEVER
/* Verify that the implicit equation is correct */
{
	double pnt[MXRO], v[MXRO];
	double pa;	/* Parameter */
	for (pa = 0.0; pa <= 1.0; pa += 0.125) {
		for (f = 0; f < fdi; f++) {
			pnt[f] = st[f] + pa * de[f];
		}

		/* Verify the implicit equation */
		for (ff = 0; ff < (fdi-1); ff++) {
			v[ff] = 0.0;
			for (f = 0; f < fdi; f++) {
				v[ff] += b->cla[ff][f] * pnt[f];
			}
			v[ff] -= b->clb[ff];
			if (v[ff] < 0.0)
				v[ff] = -v[ff];
			if (v[ff] > 0.000001) {
				printf("Point on clip line = %f %f %f\n",pnt[0],pnt[1],pnt[2]);
				printf("Implicit %d error of = %f\n",ff, v[ff]);
			}
		}
	}
}
#endif /* NEVER */

}

#endif




/* -------------------------------------------------------- */
/* Cell/simplex object lower level code */

/* Utility to get or calculate a vertexes ink limit value */
static double get_limitv(
schbase *b,			/* Base search information */
int ix,				/* fwd index of cell */
float *fcb,			/* Pointer to base of vertex value array (ix is used if NULL) */
double *p			/* Array of input values (can be NULL to compute) */
) {
	rspl *s = b->s;
	float *base = fcb;
	double lv;
	if (base == NULL)
		base = s->g.a + ix * s->g.pss;
	lv = base[-1];					/* Fetch existing ink limit function value */
	if ((float)lv == L_UNINIT) {			/* Not been computed yet */
		if (p != NULL) {
			lv = INKSCALE * s->limitf(s->lcntx, p);	/* Do it */
			base[-1] = (float)lv;
		} else {
			int e, di = s->di;
			double pp[MXRI];			/* Copy from float to double */
			int tix;					/* Temp fwd cell index */

			for (tix = ix, e = 0; e < di; e++) {
				int dix;
				dix = tix % s->g.res[e];
				tix /= s->g.res[e];
				pp[e] = s->g.l[e] + (double)dix * s->g.w[e];	/* Base point */
			}
			lv = INKSCALE * s->limitf(s->lcntx, pp);	/* Do it */
			base[-1] = (float)lv;
		}
		s->g.limitv_cached = 1;			/* At least one limit value is cached */
	}
	return lv;
}

/* Utility to invalidate all the ink limit values */
/* cached in the main rspl array */
static void clear_limitv(
rspl *s
) {
	int i;
	float *gp;		/* Grid point pointer */

	if (s->g.limitv_cached != 0) {	/* If any have been set */
		/* Unset them all */
		for (i = 0, gp = s->g.a; i < s->g.no; i++, gp += s->g.pss) {
			gp[-1] = L_UNINIT;
		}
		s->g.limitv_cached = 0;
	}
}

/* Cell code */

static void free_cell_contents(cell *c);
static cell *cache_rcell(revcache *r, int ix, int force);
static void uncache_rcell(revcache *r, cell *cp);

/* Return a pointer to an appropriate reverse cell */
/* cache structure. None of the sub simplex lists will */
/* be initialised. */
/* NOTE: must unget_cell() (== uncache_rcell()) when cell */
/* is no longer needed */
/* Return NULL if we ran out of room in the cache. */
static cell *get_rcell(
schbase *b,			/* Base search information */
int ix,				/* fwd index of cell */
int force			/* if nz, force memory allocation, so that we have at least one cell */
) {
	rspl *s = b->s;
	int ee, e, di = s->di;
	int p2di = (1<<di);
	int ff, f, fdi = s->fdi;
	cell *c;

	c = cache_rcell(s->rev.cache, ix, force);	/* Fetch it from the cache and lock it */
	if (c == NULL)
		return NULL;

	if (!(c->flags & CELL_FLAG_1)) {			/* Have to (re)initialize cell & simplexes */
		int tix;								/* Temp fwd cell index */
		float *fcb = s->g.a + ix * s->g.pss;	/* Pointer to base float of fwd cell */

		/* Compute basic Cell info and vertex output values */
		for (ee = 0; ee < p2di; ee++) {
			float *vp = fcb + s->g.fhi[ee];
			for (f = 0; f < fdi; f++)		/* Transfer cell verticy values from grid */
				c->v[ee][f] = vp[f];

			/* ~~ reset any other cell info that will be stale */
		}

		/* Convert from cell index, to absolute fwd coord base values */
		c->limmin = INF_DIST;				/* and min/max values */
		c->limmax = -INF_DIST;
		for (tix = ix, e = 0; e < di; e++) {
			int dix;
			dix = tix % s->g.res[e];
			tix /= s->g.res[e];
			c->p[0][e] = s->g.l[e] + (double)dix * s->g.w[e];	/* Base point */
		}
		if (s->limitf != NULL) {			/* Compute ink limit values at base verticy */
			double lv = get_limitv(b, ix, fcb, c->p[0]); /* Fetch or generate limit value */
			c->v[0][fdi] = lv;
			if (lv < c->limmin)	/* And min/max for this cell */
				c->limmin = lv;
			if (lv > c->limmax)
				c->limmax = lv;
		}
			
		/* Setup cube verticy input position values, and ink limit values */
		for (ee = 1; ee < p2di; ee++) {
			for (e = 0; e < di; e++) {
				c->p[ee][e] = c->p[0][e];
				if (ee & (1 << e))
					c->p[ee][e] += s->g.w[e];		/* In input space offset */
			}
			if (s->limitf != NULL) {			/* Compute ink limit values at cell verticies */
				double lv = get_limitv(b, ix, fcb + s->g.fhi[ee], c->p[ee]);
				c->v[ee][fdi] = lv;
				if (lv < c->limmin)	/* And min/max for this cell */
					c->limmin = lv;
				if (lv > c->limmax)
					c->limmax = lv;
			}
		}
		
		/* Compute the output bounding sphere for fast rejection testing */
		{
			double *min[MXRO], *max[MXRO];	/* Pointers to points with min/max values */
			double radsq = -1.0;			/* Span/radius squared */
			double rad;
			int spf = 0;
			
			/* Find verticies of cell that have min and max values in output space */
			for (f = 0; f < fdi; f++)
				min[f] = max[f] = NULL;

			for (ee = 0; ee < p2di; ee++) {
				double *vp = c->v[ee];
				for (f = 0; f < fdi; f++) {
					if (min[f] == NULL || min[f][f] > vp[f])
						min[f] = vp;
					if (max[f] == NULL || max[f][f] < vp[f])
						max[f] = vp;
				}
			}

			/* Find the pair of points with the largest span (diameter) in output space */
			for (ff = 0; ff < fdi; ff++) {
				double ss;
				for (ss = 0.0, f = 0; f < fdi; f++) {
					double tt;
					tt = max[ff][f] - min[ff][f];
					ss += tt * tt;
				}
				if (ss > radsq) {
					radsq = ss;
					spf = ff;		/* Output dimension max was in */
				}
			}

			/* Set initial bounding sphere */
			for (f = 0; f < fdi; f++) {
				c->bcent[f] = (max[spf][f] + min[spf][f])/2.0;
			}
			radsq /= 4.0;			/* diam^2 -> rad^2 */
			c->bradsq = radsq;
			rad = c->brad = sqrt(radsq);
			
			/* Go though all the points again, expanding sphere if necessary */
			for (ee = 0; ee < p2di; ee++) {
				double ss;
				double *vp = c->v[ee];

				/* Compute distance squared of point to bounding shere */
				for (ss = 0.0, f = 0; f < fdi; f++) {
					double tt = vp[f] - c->bcent[f];
					ss += tt * tt;
				}
				if (ss > radsq) {
					double tt;
					/* DBG(("Expanding bounding sphere by %f\n",sqrt(ss) - rad)); */

					ss = sqrt(ss) + EPS;			/* Radius to point */
					rad = (rad + ss)/2.0;
					c->bradsq = radsq = rad * rad;
					tt = ss - rad;
					for (f = 0; f < fdi; f++) {
						c->bcent[f] = (rad * c->bcent[f] + tt * vp[f])/ss;
					}

				} else {
					/* DBG(("Bounding sphere encloses by %f\n",rad - sqrt(ss))); */
				}
			}
			c->bradsq += EPS;
		}
		c->flags = CELL_FLAG_1;
	}

	return c;
}

void free_simplex_info(cell *c, int dof);

/* Free up any allocated simplexes in a cell, */
/* and set the pointers to NULL. */
/* Nothing else is changed (ie. it's NOT removed from */
/* the cache index or unthrheaded from the mru list). */
static void
free_cell_contents(
cell *c
) {
	int nsdi;
	
	/* Free up all the simplexes */
	if (c->s != NULL) {
		for (nsdi = 0; nsdi <= c->s->di; nsdi++) {
			if (c->sx[nsdi] != NULL) {
				free_simplex_info(c, nsdi);
				c->sx[nsdi] = NULL;
			}
		}
	}
	/* ~~ free any other cell information */
}

/* - - - - - -  */
/* Simplex code */

/* Simplex and Cell hash index size increments */
int primes[] = {
	367,
	853,
	1489,
	3373,
	3373,
	6863,
	12919,
	23333,
	43721,
	97849,
	146221,
	254941,
	-1
};

/* Compute a simplex hash index */
unsigned int simplex_hash(revcache *rc, int sdi, int efdi, int *vix) {
	unsigned int hash = 0;
	int i;

	for (i = 0; i <= sdi; i++)
		hash = hash * 17 + vix[i];
	hash = hash * 17 + sdi;
	hash = hash * 17 + efdi;

	hash %= rc->spx_hash_size;
	return hash;
}

/* Allocate and do the basic initialisation for a DOF list of simplexes */
void alloc_simplexes(
cell *c,
int nsdi			/* Non limited sub simplex dimensionality */
) {
	rspl *s = c->s;
	schbase *b = s->rev.sb;
	revcache *rc = s->rev.cache;
	int ee, e, di = s->di;
	int f, fdi = s->fdi;
	int lsdi;			/* Ink limited Sub-simplex sdi */
	int tsxno;			/* Total number of DOF simplexes */
	int nsxno;			/* Number of non-ink limited DOF simplexes */
	int si, so;			/* simplex index in and out */

	DBG(("Allocating level %d sub simplexes in cell %d\n",nsdi,c->ix));
	if (c->sx[nsdi] != NULL)
		error("rspl rev, internal, trying allocate already allocated simplexes\n");

	/* Figure out how many simplexes will be at this nsdi */
	lsdi = nsdi + 1;	/* Ink limit simplexes sdi */

	tsxno = nsxno = s->rev.sspxi[nsdi].nospx;

	if (s->limitf != NULL && lsdi <= di)
		tsxno += s->rev.sspxi[lsdi].nospx;		/* Second set with extra input dimension */

	/* Make sure there is enough space in temp simplex filter list */
	if (b->lsxfilt < tsxno) {	/* Allocate more space if needed */

		if (b->lsxfilt > 0) {	/* Free old space before allocating new */
			free(b->sxfilt);
			DECSZ(b->s, b->lsxfilt * sizeof(char));
		}
		b->lsxfilt = 0;
		/* Allocate enough space for all the candidate cells */
		if ((b->sxfilt = (char *)rev_malloc(s, tsxno * sizeof(char))) == NULL)
			error("rev: malloc failed - temp simplex filter list, count %d",tsxno);
		b->lsxfilt = tsxno;	/* Current allocated space */
		INCSZ(b->s, b->lsxfilt * sizeof(char));
	}
		
	/* Figure out the number of simplexes that will actually be needed */
	for (si = so = 0; si < tsxno; si++) {
		psxinfo *psxi = NULL;
		int *icomb, *offs;
		int sdi = nsdi;
		int efdi = fdi;
		int ssi = si;
		int isclip = 0;
		if (si >= nsxno) {				/* If limit boundary simplex */
			sdi++;						/* One more dimension */
			efdi++;						/* One more constraint */
			ssi -= nsxno;				/* In second half of list */
			isclip++;					/* Limit clipped simplex */
		}
		psxi = &s->rev.sspxi[sdi].spxi[ssi];
		icomb = psxi->icomb;
		offs  = psxi->offs;

		b->sxfilt[si] = 0;				/* Assume simplex won't be used */

		/* Check if simplex should be discared due to the ink limit */
		if (s->limitf != NULL) {
			double max = -INF_DIST;
			double min =  INF_DIST;

			/* Find the range of ink limit values covered by simplex */
			for (e = 0; e <= sdi; e++) {		/* For all the simplex verticies */
				int i = offs[e];
				double vv = c->v[i][fdi];		/* Ink limit value */
				if (vv < min)
					min = vv;
				if (vv > max)
					max = vv;
			}
			
//if ((max - min) > EPS) printf("~1 Found simplex sdi %d, efdi %d, min = %f, max = %f, limitv = %f\n", sdi, efdi, min,max,s->limitv);
			if (isclip) {	/* Limit clipped simplex */
				/* (Make sure it straddles the limit boundary) */
				if (max < s->limitv || min > s->limitv)
					continue;		/* Discard this simplex - it can't straddle the ink limit */
//printf("~1 using sub simplex sdi %d, efdi %d, min = %f, max = %f, limitv = %f\n", sdi, efdi, min,max,s->limitv);
			} else {
				if (min > s->limitv)
					continue;		/* Discard this simplex - it is above the ink limit */
			}
		}

		b->sxfilt[si] |= 1;		/* This cell will be OK */
		so++;
	}

	DBG(("There are %d level %d sub simplexes\n",so, nsdi));
	/* Allocate space for all the DOF simplexes that will be used */
	if (so > 0) {
		if ((c->sx[nsdi] = (simplex **) rev_calloc(s, so, sizeof(simplex *))) == NULL)
			error("rspl malloc failed - reverse cell simplexes - list of pointers");
		INCSZ(s, so * sizeof(simplex *));
	}

	/* Setup SPLX_FLAG_1 level information in the simplex */
	for (si = so = 0; si < tsxno; si++) {
		simplex *x;
		psxinfo *psxi = NULL;
		int *icomb;
		int sdi, efdi;
		int ssi;
		int vix[MXRI+1];            /* fwd cell vertex indexes of this simplex [sdi+1] */

		if (b->sxfilt[si] == 0)		/* Decided not to use this one */
			continue;

#ifdef STATS
		s->rev.st[b->op].sinited++;
#endif /* STATS */

		sdi = nsdi;
		efdi = fdi;
		ssi = si;
		if (si >= nsxno) {				/* If limit boundary simplex */
			sdi++;						/* One more dimension */
			efdi++;						/* One more constraint */
			ssi -= nsxno;				/* In second half of list */
		}

		psxi = &s->rev.sspxi[sdi].spxi[ssi];
		icomb = psxi->icomb;

		/* Compute simplex vertexes so we can match it in the cache */
		for (e = 0; e <= sdi; e++) 
			vix[e] = c->ix + s->g.hi[psxi->offs[e]];

		x = c->sx[nsdi][so];

		/* If this is a shared simplex, see if we already have it in another cell */
		if (x == NULL && psxi->face) {
			unsigned int hash;
//printf("~1 looking for existing simplex nsdi = %d\n",nsdi);
			hash = simplex_hash(rc, sdi, efdi, vix);
			for (x = rc->spxhashtop[hash]; x != NULL; x = x->hlink) {
				if (x->sdi != sdi
				 || x->efdi != efdi)
					continue;			/* miss */
				for (e = 0; e <= sdi; e++) {
					if (x->vix[e] != vix[e])
						break;			/* miss */
				}
				if (e > sdi)
					break;				/* hit */
			}
			if (x != NULL) {
				x->refcount++;
//printf("~1 found hit in simplex face list hash %d, refcount = %d\n",hash,x->refcount);
			}
		}
		/* Doesn't already exist */
		if (x == NULL) {
			if ((x = (simplex *) rev_calloc(s, 1, sizeof(simplex))) == NULL)
				error("rspl malloc failed - reverse cell simplexes - base simplex %d bytes",sizeof(simplex));
			INCSZ(s, sizeof(simplex));
			x->refcount = 1;
			x->touch = s->rev.stouch-1;
			x->flags = 0;

			if (si >= nsxno) {				/* If limit boundary simplex */
				x->flags |= SPLX_CLIPSX;	/* Limit clipped simplex */
			}

			/* Fill in the other simplex details */
			x->s    = s;					/* Parent rspl */
			x->ix   = c->ix;				/* Construction cube base index */
			for (e = 0; e <= sdi; e++)		/* Indexs of fwd verticies that make up this simplex */
				x->vix[e] = vix[e];
			x->psxi = psxi;					/* Pointer to constant per simplex info */
//printf("~1 set simplex 0x%x psxi = 0x%x\n",x,x->psxi);
			x->si   = so;					/* Diagnostic, simplex offset in list */
			x->sdi  = sdi;					/* Copy of simplex dimensionaity */
			x->efdi = efdi;					/* Copy of effective output dimensionality */

			/* Copy cell simplex vertex output and limit values */
			for (e = 0; e <= sdi; e++) {		/* For all the simplex verticies */
				int i = x->psxi->offs[e];

				for (f = 0; f <= fdi; f++)		/* Copy vertex value + ink sum */
					x->v[e][f] = c->v[i][f];

				/* Setup output bounding box values (the hard way) */
				if (e == 0) {						/* Init to first vertex of simplex */
					for (f = 0; f <= fdi; f++)		/* Output space */
						x->min[f] = x->max[f] = c->v[i][f];
				} else {
					for (f = 0; f <= fdi; f++) {	/* Output space + ink sum */
						double vv;
//						if (f == fdi && s->limit == NULL)
//							continue;			/* Skip ink */
						vv = c->v[i][f];
						if (vv < x->min[f])
							x->min[f] = vv;
						else if (vv > x->max[f])
							x->max[f] = vv;
					}
				}
			}
			/* Add a margin */
			for (f = 0; f <= fdi; f++) {	/* Output space + ink sum */
				x->min[f] -= EPS;
				x->max[f] += EPS;
			}

			/* Setup input bounding box value pointers (the easy way) */
			for (ee = 0; ee < di; ee++) {
				x->p0[ee]   = c->p[0][ee];		/* Construction base cube origin */
				x->pmin[ee] = c->p[x->psxi->pmino[ee]][ee] - EPS;
				x->pmax[ee] = c->p[x->psxi->pmaxo[ee]][ee] + EPS;
			}

			x->flags |= SPLX_FLAG_1;		/* vv & iv done, nothing else */

			x->aloc2 = x->aloc5 = NULL;		/* Matrix allocations not done yet */

			/* Add it to the face shared simplex hash index */
			if (x->psxi->face) {
				unsigned int hash;
				int i;
				/* See if we should re-size the simplex hash index */
				if (++rc->nspx > (HASH_FILL_RATIO * rc->spx_hash_size)) {
					for (i = 0; primes[i] > 0 && primes[i] <= rc->spx_hash_size; i++)
						;
					if (primes[i] > 0) {
						int spx_hash_size = rc->spx_hash_size;	/* Old */
						simplex **spxhashtop = rc->spxhashtop;

						rc->spx_hash_size = primes[i];

						DBG(("Increasing face simplex hash index to %d\n",spx_hash_size));
//printf("~1 increasing simplex hash index size to %d\n",spx_hash_size);
						/* Allocate a new index */
						if ((rc->spxhashtop = (simplex **) rev_calloc(s, rc->spx_hash_size,
						                                   sizeof(simplex *))) == NULL)
							error("rspl malloc failed - reverse simplex cache index");
						INCSZ(s, rc->spx_hash_size * sizeof(simplex *));

						/* Transfer all the simplexes to the new index */
						for (i = 0; i < spx_hash_size; i++) {
							simplex *x, *nx;
							for (x = spxhashtop[i]; x != NULL; x = nx) {
								nx = x->hlink;
								hash = simplex_hash(rc, x->sdi, x->efdi, x->vix);	/* New hash */
								x->hlink = rc->spxhashtop[hash];	/* Add to new hash index */
								rc->spxhashtop[hash] = x;
							}
						}
						free(spxhashtop); /* Done with old index */
						DECSZ(s, spx_hash_size * sizeof(simplex *));
					}
				}
				hash = simplex_hash(rc, sdi, efdi, vix);

				/* Add this to hash index */
				x->hlink = rc->spxhashtop[hash];
				rc->spxhashtop[hash] = x;
//printf("~1 Added simplex to hash %d, rc->nspx = %d\n",hash,rc->nspx);
			}

//if (rc->nunlocked == 0 && rc->s->rev.sz > rc->s->rev.max_sz)
//printf("~1 unable to decrease_revcache 1\n");

			/* keep memory in check */
			while (rc->nunlocked > 0 && rc->s->rev.sz > rc->s->rev.max_sz) {
				if (decrease_revcache(rc) == 0)
					break;
			}
		}
		c->sx[nsdi][so] = x;
		so++;
	}
	c->sxno[nsdi] = so;				/* Record actual number in list */
	c->flags |= CELL_FLAG_2;		/* Note that cell now has simplexes */
}

/* Free up any allocated for a list of sub-simplexes */
void
free_simplex_info(
cell *c,
int nsdi			/* non limit sub simplex dimensionaity */
) {
	int si, sxno = c->sxno[nsdi];	/* Number of simplexes */

	for (si = 0; si < sxno; si++) { /* For all the simplexes */
		simplex *x = c->sx[nsdi][si];
		int dof = x->sdi - x->efdi;

//printf("~1 freeing simplex, refcount = %d\n",x->refcount);
		if (--x->refcount <= 0) {		/* Last reference to this simplex */

//printf("~1 freeing simplex 0x%x psxi = 0x%x\n",x,x->psxi);
			if (x->psxi->face) {
				unsigned int hash;
				revcache *rc = c->s->rev.cache;
				
				hash = simplex_hash(rc, x->sdi, x->efdi, x->vix);

				/* Free it from the hash list */
				if (rc->spxhashtop[hash] == x) {
					rc->spxhashtop[hash] = x->hlink;
					rc->nspx--;
//printf("~1 removed simplex from hash %d, nspx now = %d\n",hash,rc->nspx);
				} else {
					simplex *xx;
					for (xx = rc->spxhashtop[hash]; xx != NULL && xx->hlink != x; xx = xx->hlink)
						;
					if (xx != NULL) {		/* Found it */
						xx->hlink = x->hlink;
						rc->nspx--;
//printf("~1 removed simplex from hash %d, nspx now = %d\n",hash,rc->nspx);
					}
//else
//printf("~1 warning, failed to find face simplex hash %d, sdi = %d in cache index (nspx = %d)!!\n",hash,x->sdi,rc->nspx);
				}
			}
			if (x->aloc2 != NULL) {
				int adof = dof >= 0 ? dof : 0;		/* Allocation dof */
				int asize;
				if (dof == 0)
					asize = sizeof(double) * (x->efdi * x->sdi)
				          + sizeof(double *) * x->efdi 
				          + sizeof(int) * x->sdi;
				else
					asize = sizeof(double) * (x->sdi * (x->efdi + x->sdi + adof + 2) + x->efdi)
				          + sizeof(double *) * (x->efdi + 2 * x->sdi);
				free(x->aloc2);
				DECSZ(x->s, asize);
			}

			if (x->aloc5 != NULL) {
				int asize;
				if (x->naux == dof)
					asize = sizeof(double *) * x->naux
				          + sizeof(double) * (x->naux * dof)
				          + sizeof(int) * dof;
				else
					asize = sizeof(double *) * (x->naux + dof) 
					      + sizeof(double) * (dof * (x->naux + dof + 1));
				free(x->aloc5);
				DECSZ(x->s, asize);
			}

			/* ~~ free any other simplex information */

			free(x);
			DECSZ(c->s, sizeof(simplex));
			c->sx[nsdi][si] = NULL;
		}
	}
	free(c->sx[nsdi]);
	DECSZ(c->s, c->sxno[nsdi] * sizeof(simplex *));
	c->sx[nsdi] = NULL;
	c->sxno[nsdi] = 0;

	/* ~~ free any other cell information */
}

/* - - - - - - - - - - - - */
/* Check that an input space vector is within a given simplex, */
/* and that it meets any ink limit. */
/* Return zero if outside the simplex, */
/* 1 normally if within the simplex, */
/* and 2 if it would be over the ink limit if limit was enabled. */
static int
within_simplex(
simplex *x,				/* Simplex */
double *p				/* Input coords in simplex space */
) {
	rspl *s = x->s;
	schbase *b = s->rev.sb;
	int    fdi = s->fdi;
	int e, sdi = x->sdi;		/* simplex dimensionality */
	double cp, lp;
	int rv = 1;
	/* EPS is allowance for numeric error */
	/* (Don't want solutions falling down */
	/* the numerical cracks between the simplexes) */

	/* Check we are within baricentric limits */
	for (lp = 0.0, e = 0; e < sdi; e++) {
		cp = p[e];
		if ((cp+EPS) < lp) 		/* Outside baricentric or not in correct */
			return 0;			/* order for this simplex  */
		lp = cp;
	}
	if ((1.0+EPS) < lp)  		/* outside baricentric range */
		return 0;

	/* Compute limit using interp. - assume simplex would have been trivially rejected */
	if (s->limitf != NULL) {
		double sum = 0.0;			/* Might be over the limit */
		for (e = 0; e < sdi; e++)
			sum += p[e] * (x->v[e][fdi] - x->v[e+1][fdi]);
		sum += x->v[sdi][fdi];
		if (sum > s->limitv) {
			if (s->limiten != 0)
	 			return 0;			/* Exceeds ink limit */
			else
				rv = 2;				/* would have exceeded limit */
		}
	}

#ifdef NEVER
	/* Constrain to legal values */
	/* (Is this needed ?????) */
	for (e = 0; e < sdi; e++) {
		cp = p[e];
		if (cp < 0.0)
			p[e] = 0.0;
		else if (cp > 1.0)
			p[e] = 1.0;
	}
#endif
	return rv;
}

/* Convert vector from simplex space to absolute cartesian space */
static void simplex_to_abs(
simplex *x,
double *out,	/* output in absolute space */
double *in		/* Input in simplex space */
) {
	rspl *s     = x->s;
	int e, di   = s->di;
	int *icomb  = x->psxi->icomb;	/* Coord combination order */

	for (e = 0; e < di; e++) {		/* For each absolute coord */
		double ov = x->p0[e];		/* Base value */
		int ee = icomb[e];			/* Simplex param index */
		if (ee >= 0)				/* Simplex param value */
			ov += s->g.w[e] * in[ee];
		else if (ee == -2)			/* 1 value */
			ov += s->g.w[e];
									/* Else 0 value */
		out[e] = ov;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given the parametric clip line equation, compute the */
/* implicit equation in terms of the absolute output space. */
/* Pad equation with target ink limit in case it is use */
/* with CLIPSX sub-simplexes. */
/* Note that no line equation values are returned if fdi = 1, */
/* since there is no such thing as an implicit line equation. */
static void
init_line_eq(
schbase *b,
double st[MXRO],	/* Start point */
double de[MXRO]		/* Delta */
) {
	rspl *s = b->s;
	int ff, f, fdi = s->fdi;
	int i, p;
	double lgst;

	DBG(("Computing clipping line implicit equation, dim = %d\n", fdi));
	
	/* Pick a pivot element */
	for (lgst = -1.0, p = -1, f = 0; f < fdi; f++) {
		double tt = de[f];
		b->cdir[f] = tt;		/* Stash this away */
		tt = fabs(tt);
		if (tt > lgst) {
			lgst = tt;
			p = f;
		}
	}
	if (p < 0)	/* Shouldn't happen */
		error("rspl rev, internal, trying to cope with zero length clip line\n");
	
	if (b->cla == NULL)
		b->cla = dmatrix(0, fdi-1, 0, fdi);	/* Allow for ink limit supliment */

	for (i = ff = 0;  ff < fdi; ff++) {	/* For the input rows */
		if (ff == p) {
			continue;					/* Skip pivot row */
		}
		for (f = 0; f < fdi; f++) {		/* For input & output columns */
			if (f == p) {
				b->cla[i][f] = -de[ff];	/* Last column is -ve delta value */
			} else if (f == ff) {
				b->cla[i][f] = de[p];	/* Diagonal is pivot value */
			} else {
				b->cla[i][f] = 0.0;		/* Else zero */
			}
		}
		b->clb[i] = de[p] * st[ff] - de[ff] * st[p];
		i++;
	}

	/* Add ink limit target equation - */
	/* interpolated ink value == target */
	if (s->limitf != NULL) {
		for (i = 0;  i < (fdi-1); i++)
			b->cla[i][fdi] = 0.0;

		for (f = 0; f < fdi; f++) 
			b->cla[fdi-1][f] = 0.0;
		
		b->cla[fdi-1][fdi] = 1.0;
		b->clb[fdi-1] = s->limitv;
	}

#ifdef NEVER
/* Verify that the implicit equation is correct */
{
	double pnt[MXRO], v[MXRO];
	double pa;	/* Parameter */
	for (pa = 0.0; pa <= 1.0; pa += 0.125) {
		for (f = 0; f < fdi; f++) {
			pnt[f] = st[f] + pa * de[f];
		}

		/* Verify the implicit equation */
		for (ff = 0; ff < (fdi-1); ff++) {
			v[ff] = 0.0;
			for (f = 0; f < fdi; f++) {
				v[ff] += b->cla[ff][f] * pnt[f];
			}
			v[ff] -= b->clb[ff];
			if (v[ff] < 0.0)
				v[ff] = -v[ff];
			if (v[ff] > 0.000001) {
				printf("Point on clip line = %f %f %f\n",pnt[0],pnt[1],pnt[2]);
				printf("Implicit %d error of = %f\n",ff, v[ff]);
			}
		}
	}
}
#endif /* NEVER */

}

/* - - - - - -  */
/* Simpex solution info #2 */

/* Create the LU or SVD decomp needed to compute solution or locus. */
/* Return non-zero if it cannot be created */
static int
add_lu_svd(simplex *x) {

	if (x->flags & SPLX_FLAG_2F) {		/* Previously failed */
		return 1;
	}
	if (!(x->flags & SPLX_FLAG_2)) {
		int ee, e, sdi = x->sdi; 
		int f, efdi = x->efdi; 
		int dof = sdi-efdi;		/* Degree of freedom of locus, or -ve over specification */
		int adof = dof >= 0 ? dof : 0;		/* Allocation dof */
		int i;

		if (x->aloc2 == NULL) {	/* Allocate space for matricies and arrays */
			/* Do this in one hit to minimise malloc overhead */
			if (dof == 0) {
				int i;
				char *mem;
				int asize = sizeof(double) * (efdi * sdi)
				          + sizeof(double *) * efdi 
				          + sizeof(int) * sdi;

				if ((x->aloc2 = mem = (char *) rev_malloc(x->s, asize)) == NULL)
					error("rspl malloc failed - reverse cell sub-simplex matricies");
				INCSZ(x->s, asize);

				/* Allocate biggest to smallest (double, pointers, ints) */
				/* to make sure that items lie on the natural boundaries. */

				/* Reserve matrix doubles */
				mem += efdi * sdi * sizeof(double);

				/* Allocate pointers */
				x->d_u = (double **)mem, mem += efdi * sizeof(double *);

				/* Allocate ints */
				x->d_w = (double *)mem, mem += sdi * sizeof(int);

#ifdef DEBUG
				if (mem != (x->aloc2 + asize))
					error("~1 aloc2a assert failed! Is %d, should be %d\n",mem - x->aloc2,asize);
#endif /* DEBUG */

				/* Reset and allocate matrix doubles */
				mem = x->aloc2; 
				for (i = 0; i < efdi; i++)
					x->d_u[i] = (double *)mem,	mem += sdi * sizeof(double);

			} else {
				int i;
				char *mem;
				int asize = sizeof(double) * (sdi * (efdi + sdi + adof + 2) + efdi)
				          + sizeof(double *) * (efdi + 2 * sdi);

				if ((x->aloc2 = mem = (char *) rev_malloc(x->s, asize)) == NULL)
					error("rspl malloc failed - reverse cell sub-simplex matricies");
				INCSZ(x->s, asize);

				/* Allocate biggest to smallest (double, pointers, ints) */
				/* to make sure that items lie on the natural boundaries. */

				/* Reserve matrix doubles */
				mem += sdi * (efdi + sdi + adof) * sizeof(double);

				/* Allocate doubles */
				x->lo_xb = (double *)mem, mem += efdi * sizeof(double);
				x->lo_bd = (double *)mem; mem += sdi * sizeof(double);
				x->d_w = (double *)mem, mem += sdi * sizeof(double);

				/* Allocate pointers */
				x->d_u = (double **)mem, mem += efdi * sizeof(double *);
				x->d_v = (double **)mem, mem += sdi * sizeof(double *);
				x->lo_l = (double **)mem, mem += sdi * sizeof(double *);

#ifdef DEBUG
				if (mem != (x->aloc2 + asize))
					error("~1 aloc2b assert failed! Is %d, should be %d\n",mem - x->aloc2,asize);
#endif /* DEBUG */

				/* Reset and allocate matrix doubles */
				mem = x->aloc2;
				for (i = 0; i < efdi; i++)
					x->d_u[i] = (double *)mem,	mem += sdi * sizeof(double);
				for (i = 0; i < sdi; i++)
					x->d_v[i] = (double *)mem,	mem += sdi * sizeof(double);
				for (i = 0; i < sdi; i++)
					x->lo_l[i] = (double *)mem,	mem += adof * sizeof(double);

				/* Init any values that will be read before being written to. */
				for (f = 0; f < efdi; f++)
					x->lo_xb[f] = 1e100;		/* Silly value */
			}
		}

		/* Setup matrix from vertex values */
		for (f = 0; f < efdi; f++)
			for (e = 0; e < sdi; e++)
				x->d_u[f][e] = x->v[e][f] - x->v[e+1][f];

		if (dof == 0) {	/* compute LU */
			double rip;
#ifdef STATS
			x->s->rev.st[x->s->rev.sb->op].sinited2a++;
#endif /* STATS */
			if (lu_decomp(x->d_u, sdi, (int *)x->d_w, &rip)) {
				x->flags |= SPLX_FLAG_2F;	/* Failed */
				return 1;
			}
		} else {
//printf("~~ Creating SVD decomp, sdi = %d, efdi = %d\n", sdi, efdi);

#ifdef STATS
			x->s->rev.st[x->s->rev.sb->op].sinited2b++;
#endif /* STATS */
			if (svdecomp(x->d_u, x->d_w, x->d_v, efdi, sdi)) {
				x->flags |= SPLX_FLAG_2F;	/* Failed */
				return 1;
			}
	
			/* Threshold the singular values W[] */ 
			svdthresh(x->d_w, sdi);
	
			if (dof >= 0) {		/* If we expect a locus */
//printf("~~ got dif %d locus from SVD\n",dof);
				/* copy the locus direction coefficients out */
				for (i = e = 0; e < sdi; e++) {
					if (x->d_w[e] == 0.0) {		/* Found a zero W[] */
						if (i < dof) {
							for (ee = 0; ee < sdi; ee++) {	/* Copy column of V[][] */
								x->lo_l[ee][i] = x->d_v[ee][e];
							}
						}
						i++;
					}
				}
				if (i != dof) {
//printf("~~ got unexpected dof in svd\n");
					x->flags |= SPLX_FLAG_2F;	/* Failed */
					return 1;					/* Didn't get expected d.o.f. */
				}
			}
		}
		x->flags |= SPLX_FLAG_2;	/* Set flag so that it isn't attempted again */

//if (x->s->rev.cache->nunlocked == 0 && x->s->rev.sz > x->s->rev.max_sz)
//printf("~1 unable to decrease_revcache 2\n");

		/* keep memory in check */
		while (x->s->rev.cache->nunlocked > 0 && x->s->rev.sz > x->s->rev.max_sz) {
			if (decrease_revcache(x->s->rev.cache) == 0)
				break;
		}
	}
	return 0;
}

/* - - - - - -  */
/* Simplex solution info #4 */

/* Calculate the solution locus equation for this simplex and target */
/* (The direction was calculated by add_svd(), but now calculate */
/* the base solution point for this particular reverse lookup) */
/* Return non-zero if this point canot be calculated */
/* We are assuming that sdi > efdi */
static int
add_locus(
schbase *b,
simplex *x
) {
	int sdi = x->sdi; 
	int f, efdi = x->efdi; 
	int doback = 0;

#ifdef STATS
	x->s->rev.st[x->s->rev.sb->op].sinited4++;
#endif /* STATS */
	/* Use output of svdcmp() to solve overspecified and/or */
	/* singular equation A.x = b */

	/* Init the RHS B[] vector, and check if it doesn't match */
	/* that used to compute base value last time. */
	for (f = 0; f < efdi; f++) {
		double xb = b->v[f] - x->v[sdi][f];
		if (x->lo_xb[f] != xb) {
			x->lo_xb[f] = xb;
			doback = 1;			/* RHS differs, so re-compute */
		}
	}
	
#ifdef STATS
	if (doback && (x->flags & SPLX_FLAG_4))
		x->s->rev.st[x->s->rev.sb->op].sinited4i++;
#endif /* STATS */

	/* Compute locus */
	if (doback || !(x->flags & SPLX_FLAG_4))
		svdbacksub(x->d_u, x->d_w, x->d_v, x->lo_xb, x->lo_bd, efdi, sdi);
	
	x->flags |= SPLX_FLAG_4;

//if (x->s->rev.cache->nunlocked == 0 && x->s->rev.sz > x->s->rev.max_sz)
//printf("~1 unable to decrease_revcache 3\n");

	/* keep memory in check */
	while (x->s->rev.cache->nunlocked > 0 && x->s->rev.sz > x->s->rev.max_sz) {
		if (decrease_revcache(x->s->rev.cache) == 0)
			break;
	}

	return 0;
}

/* - - - - - -  */
/* Simplex solution info #5 */

/* Compute LU or SVD decomp of lo_l */
/* Allocates the memory for the various matricies */
/* Return non-zero if this canot be calculated. */
static int
add_auxil_lu_svd(
schbase *b,
simplex *x
) {
	int ee, sdi = x->sdi; 
	int f, efdi = x->efdi; 
	int dof = sdi-efdi;		/* Degree of freedom of locus */
	int naux = b->naux;		/* Number of auxiliaries actually available */

#ifdef STATS
	if (x->aaux != b->naux || x->auxbm != b->auxbm)
		x->s->rev.st[x->s->rev.sb->op].sinited5i++;
#endif /* STATS */

	if (x->aaux != b->naux) {	/* Number of auxiliaries has changed */
		if (x->aloc5 != NULL) {
			int asize;
			if (x->naux == dof)
				asize = sizeof(double *) * x->naux
			          + sizeof(double) * (x->naux * dof)
			          + sizeof(int) * dof;
			else
				asize = sizeof(double *) * (x->naux + dof) 
				      + sizeof(double) * (dof * (x->naux + dof + 1));
			free(x->aloc5);
			x->aloc5 = NULL;
			DECSZ(x->s, asize);
		}
		x->flags &= ~(SPLX_FLAG_5 | SPLX_FLAG_5F);	/* Force recompute */
	}
	
	if (x->auxbm != b->auxbm) {	/* Different selection of auxiliaries */
		x->flags &= ~(SPLX_FLAG_5 | SPLX_FLAG_5F);	/* Force recompute */
	}

	if (x->flags & SPLX_FLAG_5F) {		/* Previously failed */
		return 1;
	}
	if (!(x->flags & SPLX_FLAG_5)) {
		int *icomb = x->psxi->icomb; /* abs -> simplex coordinate translation */

		if (x->aloc5 == NULL) {	/* Allocate space for matricies and arrays */
			/* Do this in one hit to minimise malloc overhead */
			if (naux == dof) {
				int i;
				char *mem;
				int asize = sizeof(double *) * naux
				          + sizeof(double) * (naux * dof)
				          + sizeof(int) * dof;

				if ((x->aloc5 = mem = (char *) rev_malloc(x->s, asize)) == NULL)
					error("rspl malloc failed - reverse cell sub-simplex matricies");
				INCSZ(x->s, asize);

				/* Allocate biggest to smallest (double, pointers, ints) */
				/* to make sure that items lie on the natural boundaries. */

				/* Reserve matrix doubles */
				mem += naux * dof * sizeof(double);

				/* Allocate pointers and ints */
				x->d_u = (double **)mem, mem += naux * sizeof(double *);
				x->d_w = (double *)mem, mem += dof * sizeof(int);

#ifdef DEBUG
				if (mem != (x->aloc5 + asize))
					error("aloc5a assert failed! Is %d, should be %d\n",mem - x->aloc5,asize);
#endif /* DEBUG */

				/* Reset and allocate matrix doubles */
				mem = x->aloc5;
				for (i = 0; i < naux; i++)
					x->d_u[i] = (double *)mem,	mem += dof * sizeof(double);
			} else {
				int i;
				char *mem;
				int asize = sizeof(double *) * (naux + dof) 
				          + sizeof(double) * (dof * (naux + dof + 1));

				if ((x->aloc5 = mem = (char *) rev_malloc(x->s, asize)) == NULL)
					error("rspl malloc failed - reverse cell sub-simplex matricies");
				INCSZ(x->s, asize);

				/* Allocate biggest to smallest (double, pointers, ints) */
				/* to make sure that items lie on the natural boundaries. */

				/* Reserve matrix doubles */
				mem += dof * (naux + dof) * sizeof(double);

				/* Allocate doubles */
				x->ax_w = (double *)mem, mem += dof * sizeof(double);

				/* Allocate pointers, ints */
				x->ax_u = (double **)mem, mem += naux * sizeof(double *);
				x->ax_v = (double **)mem, mem += dof * sizeof(double *);

#ifdef DEBUG
				if (mem != (x->aloc5 + asize))
					error("aloc5b assert failed! Is %d, should be %d\n",mem - x->aloc5,asize);
#endif /* DEBUG */

				/* Reset and allocate matrix doubles */
				mem = x->aloc5;
				for (i = 0; i < naux; i++)
					x->ax_u[i] = (double *)mem,	mem += dof * sizeof(double);
				for (i = 0; i < dof; i++)
					x->ax_v[i] = (double *)mem, mem += dof * sizeof(double);
			}
			x->aaux = naux;				/* Number of auxiliaries allocated for */
		}
	
		/* Setup A[][] matrix to decompose, and figure number of auxiliaries actually needed */
		for (ee = naux = 0; ee < b->naux; ee++) {
			int ei = icomb[b->auxi[ee]];		/* Simplex relative auxiliary index */
			if (ei < 0)
				continue;		/* aux corresponds with fixed input value for this simplex */
			for (f = 0; f < dof; f++)
				x->ax_u[naux][f] = x->lo_l[ei][f];
			naux++;
		}
		x->naux = naux;					/* Number of auxiliaries actually available */
		x->auxbm = b->auxbm;			/* Mask of auxiliaries used */

		if (naux == dof) {				/* Use LU decomp to solve exactly */
			double rip;

#ifdef STATS
			x->s->rev.st[x->s->rev.sb->op].sinited5a++;
#endif /* STATS */
			if (lu_decomp(x->ax_u, dof, (int *)x->ax_w, &rip)) {
				x->flags |= SPLX_FLAG_5F;
				return 1;
			}

		} else if (naux > 0) {			/* Use SVD to solve least squares */

#ifdef STATS
			x->s->rev.st[x->s->rev.sb->op].sinited5b++;
#endif /* STATS */
			if (svdecomp(x->ax_u, x->ax_w, x->ax_v, naux, dof)) {
				x->flags |= SPLX_FLAG_5F;
				return 1;
			}
	
			/* Threshold the singular values W[] */ 
			svdthresh(x->ax_w, dof);
		} /* else naux == 0, don't setup anything */

		x->flags |= SPLX_FLAG_5;

//if (x->s->rev.cache->nunlocked == 0 && x->s->rev.sz > x->s->rev.max_sz)
//printf("~1 unable to decrease_revcache 4\n");

		/* keep memory in check */
		while (x->s->rev.cache->nunlocked > 0 && x->s->rev.sz > x->s->rev.max_sz) {
			if (decrease_revcache(x->s->rev.cache) == 0)
				break;
		}
	}
	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Initialise a static sub-simplex verticy information table */
void
rspl_init_ssimplex_info(
rspl *s,
ssxinfo *xip,				/* Pointer to sub-simplex info structure to init. */
int sdi						/* Sub-simplex dimensionality (range 0 - di) */
) {
	int e, di = s->di;		/* Dimensionality */
	int vi, nospx;			/* Number of sub-simplexes */
	XCOMBO(vcmb, MXDI, sdi+1, 1 << di);/* Simplex dimension sdi out of cube dimension di counter */

	DBG(("init_ssimplex_info called with sdi = %d\n",sdi));
	/* First count the number of sub-simplexes */
	nospx = 0;
	XCB_INIT(vcmb);
	while (!XCB_DONE(vcmb)) {
		nospx++;
		XCB_INC(vcmb);
	}

	xip->sdi = sdi;
	xip->nospx = nospx;
	if ((xip->spxi = (psxinfo *) rev_calloc(s, nospx, sizeof(psxinfo))) == NULL)
		error("rspl malloc failed - reverse cell sub-simplex info array");
	INCSZ(s, nospx * sizeof(psxinfo));
	
	DBG(("Number of subsimplex = %d\n",nospx));
	/* For all sub-simplexes */
	XCB_INIT(vcmb);
	for (vi = 0; vi < nospx; vi++) {
		psxinfo *x = &xip->spxi[vi];
		int i;
		int andm, orm;

		/* XCOMB generates verticies in order from max to min offset */

		/* Compute Absolute -> Parameter mapping */
		for (e = 0; e < di; e++) {				/* For each absolute axis */

			if ((vcmb[sdi] & (1<<e)) != 0) {
				x->icomb[e] = -2;	/* This abs is always '1' */

			} else if ((vcmb[0] & (1<<e)) == 0) {
				x->icomb[e] = -1;	/* This abs is always '0' */

			} else {
				for (i = 0; i < sdi; i++) {	/* For each verticy in large to small order (!first) */
					if ((vcmb[i]   & (1<<e)) != 0 && 
					    (vcmb[i+1] & (1<<e)) == 0) {/* Transition from offset 1 to 0 */
						x->icomb[e] = i;	/* This is parameter */
						break;
					}
				}
			}
		}
		
		/* Compute fwd grid offsets for each simplex vertex in baricentric order */
		for (i = 0; i <= sdi; i++) {	/* For each verticy */
			int pmin[MXRI], pmax[MXRI];
			x->offs[i]  = vcmb[i];
			x->goffs[i] = s->g.hi[vcmb[i]];
			x->foffs[i] = s->g.fhi[vcmb[i]];

			/* Setup input coordinate bounding box value offsets */
			if (i == 0) {								/* Init to first vertex of simplex */
				for (e = 0; e < di; e++) {				/* Input space */
					x->pmino[e] = x->pmaxo[e] = vcmb[i];
					pmin[e] = pmax[e] = vcmb[i] & (1<<e);
				}
			} else {
				for (e = 0; e < di; e++) {			/* Input space */
					int vv = vcmb[i] & (1<<e);
					if (vv < pmin[e]) {				/* Adjust min/max offsets */
						x->pmino[e] = vcmb[i];
						pmin[e] = vv;
					} else if (vv > pmax[e]) {
						x->pmaxo[e] = vcmb[i];
						pmax[e] = vv;
					}
				}
			}
		}

		/* See if the sub-simplex lies on a cube face */
		andm = ~0;
		orm = 0;
		for (i = 0; i <= sdi; i++) {	/* For each verticy */
			andm &= vcmb[i];
			orm  |= vcmb[i];
		}
		/* If one coordinate is common (all 0 or all 1) to the verticies, */
		/* they must all be on the same cube face. */
		if (andm != 0 || orm != ((1 << di)-1))
			x->face = 1;
		else
			x->face = 0;

#ifdef DEBUG
		printf("Verticies   = ");
		for (i = 0; i <= sdi; i++)
			printf("%d ",vcmb[i]);
		printf("\n");
		
		printf("Face        = %s\n",x->face ? "True" : "False");
		
		printf("Abs -> Parm = ");
		for (e = 0; e < di; e++)
			printf("%d ",x->icomb[e]);
		printf("\n");
		
		printf("Grid Offset      = ");
		for (e = 0; e <= sdi; e++)
			printf("%d ",x->goffs[e]);
		printf("Float Offset      = ");
		for (e = 0; e <= sdi; e++)
			printf("%d ",x->foffs[e]);
		printf("\n");
		printf("\n");
#endif /* DEBUG */

		/* Increment the counter value */
		XCB_INC(vcmb);
	}
}

/* Free the given sub-simplex verticy information */
void
rspl_free_ssimplex_info(
rspl *s,
ssxinfo *xip		/* Pointer to sub-simplex info structure */
) {
	if (xip == NULL)	/* Assert */
		return;

	free(xip->spxi);
	DECSZ(s, xip->nospx * sizeof(psxinfo));
	xip->spxi = NULL;
}

/* ====================================================== */
/* Reverse cell cache code                                */

/* Allocate and initialise the reverse cell cache */
static revcache *
alloc_revcache(
rspl *s
) {
	revcache *rc;

	DBG(("alloc_revcache called\n"));
	if ((rc = (revcache *) rev_calloc(s, 1, sizeof(revcache))) == NULL)
		error("rspl malloc failed - reverse cell cache");
	INCSZ(s, sizeof(revcache));
	
	rc->s = s;		/* For stats */

	/* Allocate an initial cell hash index */
	rc->cell_hash_size = primes[0];

	if ((rc->hashtop = (cell **) rev_calloc(s, rc->cell_hash_size, sizeof(cell *))) == NULL)
		error("rspl malloc failed - reverse cell cache index");
	INCSZ(s, rc->cell_hash_size * sizeof(cell *));

	/* Allocate an initial simplex face match hash index */
	rc->spx_hash_size = primes[0];

	if ((rc->spxhashtop = (simplex **) rev_calloc(s, rc->spx_hash_size, sizeof(simplex *))) == NULL)
		error("rspl malloc failed - reverse simplex cache index");
	INCSZ(s, rc->spx_hash_size * sizeof(simplex *));

	return rc;
}

/* Free the reverse cell cache */
static void
free_revcache(revcache *rc) {
	int i;
	cell *cp, *ncp;

	/* Free any stuff allocated in the cell contents, and the cell itself. */
	for (cp = rc->mrubot; cp != NULL; cp = ncp) {
		ncp = cp->mruup;
		free_cell_contents(cp);
		free(cp);
		DECSZ(rc->s, sizeof(cell));
	}

	/* Free the hash indexes */
	free(rc->hashtop);
	DECSZ(rc->s, rc->cell_hash_size * sizeof(cell *));
	free(rc->spxhashtop);
	DECSZ(rc->s, rc->spx_hash_size * sizeof(simplex *));

	DECSZ(rc->s, sizeof(revcache));
	free(rc);
}

/* Invalidate the whole cache */
static void
invalidate_revcache(
revcache *rc)
{
	int i;
	cell *cp;

	rc->nunlocked = 0;

	/* Free any stuff allocated in the cell contents */
	for (cp = rc->mrubot; cp != NULL; cp = cp->mruup) {
		free_cell_contents(cp);
		cp->refcount = 0;		/* Make sure they can now be reused */
		cp->ix = 0;
		cp->flags = 0;			/* Contents needs re-initializing */
		rc->nunlocked++;
	}

	/* Clear the hash table so they can't be hit */
	for (i = 0; i < rc->cell_hash_size; i++) {
		rc->hashtop[i] = NULL;
	}

}

#define HASH(xx, yy) ((yy) % xx->cell_hash_size)

/* Allocate another cell, and add it to the cache. */
/* This may re-size the hash index too. */
/* Return the pointer to the new cell. */
/* (Note it's not our job here to honour the memory limit) */
static cell *
increase_revcache(
revcache *rc
) {
	cell *nxcell;		/* Newly allocated cell */
	int i;

	DBG(("Adding another chunk of cells to cache\n"));

#ifdef NEVER	/* We may be called with force != 0 */
	if (rc->s->rev.sz >= rc->s->rev.max_sz)
		return NULL;
#endif

	if ((nxcell = (cell *) rev_calloc(rc->s, 1, sizeof(cell))) == NULL)
		error("rspl malloc failed - reverse cache cells");
	INCSZ(rc->s, sizeof(cell));

	nxcell->s = rc->s;

	/* Add cell to the bottom of the cache mru linked list */
	if (rc->mrutop == NULL)					/* List was empty */
		rc->mrutop = nxcell;
	else {
		rc->mrubot->mrudown = nxcell;	/* Splice into bottom */
		nxcell->mruup = rc->mrubot;
	}
	rc->mrubot = nxcell;
	rc->nacells++;
	rc->nunlocked++;

	DBG(("cache is now %d cells\n",rc->nacells));

	/* See if the hash index should be re-sized */
	if (rc->nacells > (HASH_FILL_RATIO * rc->cell_hash_size)) {
		for (i = 0; primes[i] > 0 && primes[i] <= rc->cell_hash_size; i++)
			;
		if (primes[i] > 0) {
			int cell_hash_size = rc->cell_hash_size;	/* Old */
			cell **hashtop = rc->hashtop;

			rc->cell_hash_size = primes[i];

			DBG(("Increasing cell cache hash index to %d\n",cell_hash_size));
			/* Allocate a new index */
			if ((rc->hashtop = (cell **) rev_calloc(rc->s, rc->cell_hash_size, sizeof(cell *))) == NULL)
				error("rspl malloc failed - reverse cell cache index");
			INCSZ(rc->s, rc->cell_hash_size * sizeof(cell *));

			/* Transfer all the cells to the new index */
			for (i = 0; i < cell_hash_size; i++) {
				cell *c, *nc;
				for (c = hashtop[i]; c != NULL; c = nc) {
					int hash;
					nc = c->hlink;
					hash = HASH(rc, c->ix); 		/* New hash */
					c->hlink = rc->hashtop[hash];	/* Add to new hash index */
					rc->hashtop[hash] = c;
				}
			}

			/* Done with old index */
			free(hashtop);
			DECSZ(rc->s, cell_hash_size * sizeof(cell *));
		}
	}
	
	return nxcell;
}

/* Reduce the cache memory usage by freeing the least recently used unlocked cell. */
/* Return nz if we suceeeded in freeing some memory. */
static int decrease_revcache(
revcache *rc		/* Reverse cache structure */
) {
	int hit = 0;
	int hash;
	cell *cp;
	
	DBG(("Decreasing cell cache memory allocation by freeing a cell\n"));

	/* Use the least recently used unlocked cell */
	for (cp = rc->mrubot; cp != NULL && cp->refcount > 0; cp = cp->mruup)
		;

	/* Run out of unlocked cells */
	if (cp == NULL) {
		DBG(("Failed to find unlocked cell to free\n"));
//printf("~1 failed to decrease memory\n");
		return 0;
	}
	
	/* If it has been used before, free up the simplexes */
	free_cell_contents(cp);

	/* Remove from current hash index (if it is in it) */
	hash = HASH(rc,cp->ix);			/* Old hash */
	if (rc->hashtop[hash] == cp) {
		rc->hashtop[hash] = cp->hlink;
	} else {
		cell *c;
		for (c = rc->hashtop[hash]; c != NULL && c->hlink != cp; c = c->hlink)
			;
		if (c != NULL)
			c->hlink = cp->hlink;
	}

	/* Free up this cell - Remove it from LRU list */
	if (rc->mrutop == cp)
		rc->mrutop = cp->mrudown;
	if (rc->mrubot == cp)
		rc->mrubot = cp->mruup;
	if (cp->mruup != NULL)
		cp->mruup->mrudown = cp->mrudown;
	if (cp->mrudown != NULL)
		cp->mrudown->mruup = cp->mruup;
	cp->mruup = cp->mrudown = NULL;
	free(cp);
	DECSZ(rc->s, sizeof(cell));
	rc->nacells--;
	rc->nunlocked--;

	DBG(("Freed a rev cache cell\n"));
	return 1;
}

/* Return a pointer to an appropriate reverse cell */
/* cache structure. cell->flags will be 0 if the cell */
/* has been reallocated. cell contents will be 0 if */
/* never used before. */
/* The cell reference count is incremented, so that it */
/* can't be thrown out of the cache. The cell must be */
/* released with uncache_rcell() when it's no longer needed. */
/* return NULL if we ran out of room in the cache */
static cell *cache_rcell(
revcache *rc,		/* Reverse cache structure */
int ix,				/* fwd index of cell */
int force			/* if nz, force memory allocation, so that we have at least one cell */
) {
	int hit = 0;
	int hash;
	cell *cp;
	
	/* keep memory in check - fail if we're out of memory and can't free any */
	/* (Doesn't matter if it might be a hit, it will get picked up the next time) */
	if (!force && rc->s->rev.sz > rc->s->rev.max_sz && rc->nunlocked <= 0) {
		return NULL;
	}

//if (rc->nunlocked == 0 && rc->s->rev.sz > rc->s->rev.max_sz)
//printf("~1 unable to decrease_revcache 5\n");

	/* Free up memory to get below threshold */
	while (rc->nunlocked > 0 && rc->s->rev.sz > rc->s->rev.max_sz) {
		if (decrease_revcache(rc) == 0)
			break;
	}

	hash = HASH(rc,ix);		/* Compute hash of fwd cell index */

	/* See if we get a cache hit */
	for (cp = rc->hashtop[hash]; cp != NULL; cp = cp->hlink) {
		if (ix == cp->ix) {	/* Hit */
			hit = 1;
#ifdef STATS
			rc->s->rev.st[rc->s->rev.sb->op].chits++;
#endif /* STATS */
			break;
		}
	}
	if (!hit) {			/* No hit, use new cell or the least recently used cell */
		int ohash;

		/* If we haven't used all our memory, or if we are forced and have */
		/* no cell we can re-use, then noallocate another cell */
		if (rc->s->rev.sz < rc->s->rev.max_sz || (force && rc->nunlocked == 0)) {
			cp = increase_revcache(rc);
			hash = HASH(rc,ix);			/* Re-compute hash in case hash size changed */
//printf("~1 using new cell\n");
		} else {
//printf("~1 memory limit has been reached, using old cell\n");

			for (;;) {
				/* Use the least recently used unlocked cell */
				for (cp = rc->mrubot; cp != NULL && cp->refcount > 0; cp = cp->mruup)
					;
	
				/* Run out of unlocked cells */
				if (cp == NULL) {
//printf("~1 none available\n");
					return NULL;
				}
	
				/* If it has been used before, free up the simplexes */
				free_cell_contents(cp);

				/* Remove from current hash index (if it is in it) */
				ohash = HASH(rc,cp->ix);			/* Old hash */
				if (rc->hashtop[ohash] == cp) {
					rc->hashtop[ohash] = cp->hlink;
				} else {
					cell *c;
					for (c = rc->hashtop[ohash]; c != NULL && c->hlink != cp; c = c->hlink)
						;
					if (c != NULL)
						c->hlink = cp->hlink;
				}

				/* If we're now under the memory limit, use this cell */
				if (rc->s->rev.sz < rc->s->rev.max_sz) {
					break;
				}

//printf("~1 freeing a cell\n");
				/* Free up this cell and look for another one */
				/* Remove it from LRU list */
				if (rc->mrutop == cp)
					rc->mrutop = cp->mrudown;
				if (rc->mrubot == cp)
					rc->mrubot = cp->mruup;
				if (cp->mruup != NULL)
					cp->mruup->mrudown = cp->mrudown;
				if (cp->mrudown != NULL)
					cp->mrudown->mruup = cp->mruup;
				cp->mruup = cp->mrudown = NULL;
				free(cp);
				DECSZ(rc->s, sizeof(cell));
				rc->nacells--;
				rc->nunlocked--;
			}
		}

#ifdef STATS
		rc->s->rev.st[rc->s->rev.sb->op].cmiss++;
#endif /* STATS */

		/* Add this cell to hash index */
		cp->hlink = rc->hashtop[hash];
		rc->hashtop[hash] = cp;	/* Add to hash table and list */

		cp->ix = ix;
		cp->flags = 0;			/* Contents needs re-initializing */
//printf("~1 returning fresh cell\n");
	}
	
	/* Move slected cell to the top of the mru list */
	if (cp->mruup != NULL) {		/* This one wasn't already at top */
		cp->mruup->mrudown = cp->mrudown;
		if (cp->mrudown == NULL)	/* This was bottom */
			rc->mrubot = cp->mruup;	/* New bottom */
		else
			cp->mrudown->mruup = cp->mruup;
		/* Put this one at the top */
		rc->mrutop->mruup = cp;
		cp->mrudown = rc->mrutop;
		rc->mrutop = cp;
		cp->mruup = NULL;
	}
	if (cp->refcount == 0) {
		rc->nunlocked--;
	}

	cp->refcount++;

	return cp;
}

/* Tell the cache that we aren't using this cell anymore, */
/* but to keep it in case it is needed again. */
static void uncache_rcell(
revcache *rc,		/* Reverse cache structure */
cell *cp
) {
	if (cp->refcount > 0) {
		cp->refcount--;
		if (cp->refcount == 0) {
			rc->nunlocked++;
		}
	} else
		warning("rspl cell cache assert: refcount overdecremented!");
}

/* ====================================================== */
/* Reverse rspl setup functions                           */

/* Called by rspl initialisation */
/* Note that reverse cell lookup tables are not */
/* allocated & created until the first call */
/* to a reverse interpolation function. */
void
init_rev(rspl *s) {

	/* First section */
	s->rev.inited = 0;
	s->rev.res = 0;
	s->rev.no = 0;
	s->rev.rev = NULL;

	/* Second section */
	s->rev.rev_valid = 0;
	s->rev.nnrev = NULL;

	/* Third section */
	s->rev.cache = NULL;

	/* Fourth section */
	s->rev.sb = NULL;

	/* Methods */
	s->rev_set_limit   = rev_set_limit_rspl;
	s->rev_get_limit   = rev_get_limit_rspl;
	s->rev_interp      = rev_interp_rspl;
	s->rev_locus       = rev_locus_rspl;
	s->rev_locus_segs  = rev_locus_segs_rspl;
}

/* Free up all the reverse interpolation info */
void free_rev(
rspl *s		/* Pointer to rspl grid */
) {
	int e, di = s->di;
	int **rpp, *rp;
		
#ifdef STATS
	{
		int i, totcalls = 0;
		for (i = 0; i < 5; i++) {
			totcalls += s->rev.st[i].searchcalls;
		}

		printf("\n===============================\n");
		printf("di = %d, do = %d\n",s->di, s->fdi);
		for (i = 0; i < 5; i++) {
			int calls = s->rev.st[i].searchcalls;
			if (calls == 0) 
				continue;
			printf("\n- - - - - - - - - - - - - - - -\n");
			printf("Operation %s\n",opnames[i]);
			printf("Search calls = %d = %f%%\n",s->rev.st[i].searchcalls,
			100.0 * s->rev.st[i].searchcalls/totcalls);
			printf("Cells searched/call = %f\n",s->rev.st[i].csearched/(double)calls);
			printf("Simplexes searched/call = %f\n",s->rev.st[i].ssearched/(double)calls);
			printf("Simplexes inited level 1/call = %f\n",s->rev.st[i].sinited/(double)calls);
			printf("Simplexes inited level 2 (LU)/call = %f\n",s->rev.st[i].sinited2a/(double)calls);
			printf("Simplexes inited level 2 (SVD)/call = %f\n",s->rev.st[i].sinited2b/(double)calls);
			printf("Simplexes invalidated level 4/call = %f\n",s->rev.st[i].sinited4i/(double)calls);
			printf("Simplexes inited level 4/call = %f\n",s->rev.st[i].sinited4/(double)calls);
			printf("Simplexes invalidated level 5/call = %f\n",s->rev.st[i].sinited5i/(double)calls);
			printf("Simplexes inited level 5 (LU)/call = %f\n",s->rev.st[i].sinited5a/(double)calls);
			printf("Simplexes inited level 5 (SVD)/call = %f\n",s->rev.st[i].sinited5b/(double)calls);
			if ((s->rev.st[i].chits + s->rev.st[i].cmiss) == 0)
				printf("No cache calls\n");
			else
				printf("Cell hit rate = %f%%\n",
					100.0 * s->rev.st[i].chits/(double)(s->rev.st[i].chits + s->rev.st[i].cmiss));
		}
		printf("\n===============================\n");
	}
#endif /* STATS */

	/* Free up Fourth section */
	if (s->rev.sb != NULL) {
		free_search(s->rev.sb);
		s->rev.sb = NULL;
	}
	/* Free up the Third section */
	if (s->rev.cache != NULL) {
		free_revcache(s->rev.cache);	/* Reverse cell cache */
		s->rev.cache = NULL;
	}

	/* Free up the Second section */
	if (s->rev.nnrev != NULL) {
		/* Free arrays at grid points, taking care of reference count */
		for (rpp = s->rev.nnrev; rpp < (s->rev.nnrev + s->rev.no); rpp++) {
			if ((rp = *rpp) != NULL && --rp[2] <= 0) {
				DECSZ(s, rp[0] * sizeof(int));
				free(*rpp);
				*rpp = NULL;
			}
		}
		free(s->rev.nnrev);
		DECSZ(s, s->rev.no * sizeof(int *));
		s->rev.nnrev = NULL;
	}

	if (di > 1 && s->rev.rev_valid) {
		rev_struct *rsi, **rsp;
		size_t ram_portion = g_avail_ram;

		/* Remove it from the linked list */
		for (rsp = &g_rev_instances; *rsp != NULL; rsp = &((*rsp)->next)) {
			if (*rsp == &s->rev) {
				*rsp = (*rsp)->next;
				break;
			}
		}

		/* Aportion the memory */
		g_no_rev_cache_instances--;

		if (g_no_rev_cache_instances > 0) {
			ram_portion /= g_no_rev_cache_instances; 
			for (rsi = g_rev_instances; rsi != NULL; rsi = rsi->next)
				rsi->max_sz = ram_portion;
			if (s->verbose)
				fprintf(stdout, "%cThere %s %d rev cache instance%s with %lu Mbytes limit\n",
				                cr_char,
								g_no_rev_cache_instances > 1 ? "are" : "is",
			                    g_no_rev_cache_instances,
								g_no_rev_cache_instances > 1 ? "s" : "",
			                    (unsigned long)ram_portion/1000000);
		}
	}

	s->rev.rev_valid = 0;

	if (s->rev.rev != NULL) {
		/* Free arrays at grid points, taking care of reference count */
		for (rpp = s->rev.rev; rpp < (s->rev.rev + s->rev.no); rpp++) {
			if ((rp = *rpp) != NULL && --rp[2] <= 0) {
				DECSZ(s, rp[0] * sizeof(int));
				free(*rpp);
				*rpp = NULL;
			}
		}
		free(s->rev.rev);
		DECSZ(s, s->rev.no * sizeof(int *));
		s->rev.rev = NULL;
	}

	/* If first section has been initialised */
	if (s->rev.inited != 0)	 {

		/* Sub-simplex information */
		for (e = 0; e <= di; e++) {
			rspl_free_ssimplex_info(s, &s->rev.sspxi[e]);
		}
		s->rev.res = 0;
		s->rev.no = 0;
		s->rev.inited = 0;
	}
	DBG(("rev allocation left after free = %d bytes\n",s->rev.sz));
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef NEVER	/* Test code */
/* Reverse closest find using exaustive pseudo hilbert search */
static void debug_find_closest_rev(
rspl *s,
double *out,
double *in
) {
	double best = 1e38;
	int e, f;
	rpsh counter;		/* Pseudo-hilbert counter */
	int gc[MXDI];		/* Grid index value */
	double iv[MXDI];
	float *gp;			/* Pointer to grid data */

	rpsh_init(&counter, s->di, (unsigned int *)s->g.res, gc);	/* Initialise counter */
	for (;;) {
		double dist;

		/* Compute grid pointer and input sample values */
		gp = s->g.a;	/* Base of grid data */
		for (e = 0; e < s->di; e++) { 				/* Input tables */
			gp += s->g.fci[e] * gc[e];				/* Grid value pointer */
			iv[e] = s->g.l[e] + gc[e] * s->g.w[e];	/* Input sample values */
		}

		dist = 0.0;
		for (f = 0; f < s->fdi; f++) {
			double tt = in[f] - (double)gp[f];
			dist += tt * tt;
		}
		if (dist < best) {
			best = dist;
			for (e = 0; e < s->di; e++)
				out[e] = iv[e];
		}

		/* Increment counter */
		if (rpsh_inc(&counter, gc))
			break;
	}
}
#endif /* NEVER */

/* ========================================================== */
/* reverse lookup acceleration structure initialisation code */

/* The reverse lookup relies on a search of the fwd interpolation tables. */
/* To eliminate out of gamut points quickly, to provide a starting point for */
/* the search, and to guarantee that all possible reverse solutions are discovered, */
/* a spatial indexing structure is used to provide a list of starting candidate */
/* forward indexes for a given output value. (rev.rev[]) */
/* The reverse structure contains an fdi dimensional cell grid, each element of the */
/* cell grid holding the indexes of the forward interpolation grid, which intersect */
/* that ranges of output values. A reverse cell will be empty if there is no */
/* potential exact solution. */
/* Note that unlike the forward grid which is composed of verticies, */
/* this grid is composed of cells (there is an extra row allocated */
/* during construction using verticies, that are not used when converted. */
/* to cells) */
/* For accelleration of the nearest lookup, a parallel reverse grid is */
/* constructed that holds lists of forward grid cells that may hold the */
/* nearest point within the gamut. These lists may be empty if we are within */
/* gamut - ie. the rev.nnrev[] array is the complement of the rev.rev[] array. */
/* During construction of rev.nnrev[], it is initially filled with lists for */
/* the potential nearest cell list for each vertex (hence the extra rows allocated */
/* for rev[] and nnrev[]), and these are then merged down to form the list */
/* for each cell extent. The nnrev[] array is filled using a seed fill algorithm, */
/* starting from the edges of the filled cells in rev[]. */
/* Since many of the cells map to the same surface region, many of the fwd cell lists */
/* are shared. */

/* NOTE: that the nnrev accuracy doesn't seem as good as fill_nncell() ! */
/* Could we fix this with better geometry calculations ??? */

/* rev.nnrev[] cache entry record */
struct _nncache{
	int min[MXRO];			/* bwd vertex extent covered by this list */
	int max[MXRO];		
	int *rip;				/* Fwd cell list */
	struct _nncache *next;	/* Link list for this cache key */
}; typedef struct _nncache nncache;

/* Structure to hold prime seed vertex information */
struct _primevx{
	int ix;						/* Index of prime seed */
	int gc[MXRO];				/* coordinate of the prime seed vertex */
	struct _primevx *next;		/* Linked list for final conversion */
	int *clist;					/* Cell list generated for prime cell */
}; typedef struct _primevx primevx;

/* Structure to hold temporary nn reverse vertex propogation information */
struct _propvx{
	int ix;						/* Index of this secondary seed */
	int gc[MXRO];				/* coordinate of this secondary seed */
	int cix;					/* Index of the closest surface nnrev vertex */
	double dsq;					/* Distance to the closest point squared */
	int pass;					/* Propogation pass */
	struct _propvx *next;		/* Linked list for next seeds */
}; typedef struct _propvx propvx;

/* Initialise the rev Second section acceleration information. */
/* This is called when it is discovered on a call that s->rev.rev_valid == 0 */
static void init_revaccell(
rspl *s
) {
	int i, j;		/* Index of fwd grid point */
	int e, f, ee, ff;
	int di = s->di;
	int fdi = s->fdi;
	int gno = s->g.no;
	int rgno = s->rev.no;
	int argres = s->rev.ares;		/* Allocation rgres, = no bwd cells +1 */
	int rgres = s->rev.res;			/* no bwd cells */
	int rgres_1 = rgres-1;			/* rgres -1 == maximum base coord value */

	schbase *b = s->rev.sb;		/* Base search information */
	char *vflag = NULL;			/* Per vertex flag used during construction of nnrev */
	float *gp;					/* Pointer to fwd grid points */
	primevx *plist = NULL, *ptail = NULL;	/* Prime seed list for last pass */
	propvx *alist = NULL;				/* Linked list of active secondary seeds */
	propvx *nlist = NULL;				/* Linked list of next secondary seeds */
	DCOUNT(gg, MXRO, fdi, 0, 0, argres);/* Track the prime seed coordinate */
	DCOUNT(cc, MXRO, fdi, -1, -1, 2);	/* Neighborhood offset counter */
	int nn[MXRO];						/* Neighbor coordinate */
	int pass = 0;						/* Propogation pass */
	int nncsize;	/* Size of the rev.nnrev construction cache index */
	nncache **nnc;	/* nn cache index, used during construction of nnrev */
	unsigned hashk;							/* Hash key */
	nncache *ncp;	/* Hash entry pointer */ 
	int nskcells = 0;					/* Number of skiped cells (debug) */
#ifdef DEBUG
	int cellinrevlist = 0;
	int fwdcells = 0;
#endif

	DBG(("init_revaccell called, di = %d, fdi = %d, mgres = %d\n",di,fdi,(int)s->g.mres));

	if (!s->rev.fastsetup) {
		/* Temporary per bwd vertex/cell flag */
		if ((vflag = (char *) calloc(rgno, sizeof(char))) == NULL)
			error("rspl malloc failed - rev.vflag points");
		INCSZ(s, rgno * sizeof(char));
	}

	/*
	 * The rev[] and nnrev[] grids contain pointers to lists of grid cube base indexes.
	 * If the pointer is NULL, then there are no base indexes in that list.
	 * A non NULL list uses element [0] to indicate the alocation size of the list,
	 * [1] contains the index of the next free location, [2] contains the reference
     * count (lists may be shared), the list starts at [3]. The last entry is marked with -1.
	 */

	/* We won't include any fwd cells that are over the ink limit, */
	/* so makes sure that the fwd cell nodes all have an ink limit value. */ 
	if (b != NULL && s->limiten) {
		ECOUNT(gc, MXDIDO, s->di, 0, s->g.res, 0);    /* coordinates */
		double iv[MXDI];				/* Input value corresponding to grid */

		DBG(("Looking up fwd vertex ink limit values\n"));
		/* Calling the limit function for each fwd vertex could be bad */
		/* if the limit function is slow. Maybe an octree type algorithm */
		/* could be used if this is a problem ? */
		EC_INIT(gc);
		for (i = 0, gp = s->g.a; i < s->g.no; i++, gp += s->g.pss) {
			if (gp[-1] == L_UNINIT) {
				for (e = 0; e < di; e++)
					iv[e] = s->g.l[e] + gc[e] * s->g.w[e];  /* Input sample values */
				gp[-1] = (float)(INKSCALE * s->limitf(s->lcntx, iv));
			}
			EC_INC(gc);
		}
		s->g.limitv_cached = 1;
	}

	/* We then fill in the in-gamut reverse grid lookups, */
	/* and identify nnrev prime seed verticies */

	DBG(("filling in rev.rev[] grid\n"));
	
	/* To create rev.rev[], for all fwd grid points, form the cube with that */
	/* point at its base, and determine the bounding box of the output values */
	/* that could intersect that cube. */
	/* As a start for creating rev.nnrevp[], flag which bwd verticies are */
	/* covered by the fwd grid output range. */
	for (gp = s->g.a, i = 0; i < gno; gp += s->g.pss, i++) {
		datao min, max;
		int imin[MXRO], imax[MXRO], gc[MXRO];
		int uil;			/* One is under the ink limit */

//printf("~1 i = %d/%d\n",i,gno);
		/* Skip grid points on the upper edge of the grid, since there */
		/* is no further grid point to form a cube range with. */
		for (e = 0; e < di; e++) {
			if(G_FL(gp, e) == 0)		/* At the top edge */
				break;
		}
		if (e < di) {	/* Top edge - skip this cube */
			continue;
		}

		/* Find the output value bounding box values for this grid cell */
		uil = 0;
		for (f = 0; f < fdi; f++)	/* Init output min/max */
			min[f] = max[f] = gp[f];
		if (b == NULL || !s->limiten || gp[-1] <= s->limitv)
			uil = 1;
	
		/* For all other grid points in the cube */
		for (ee = 1; ee < (1 << di); ee++) {
			float *gt = gp + s->g.fhi[ee];	/* Pointer to cube vertex */
			
			if (b == NULL || !s->limiten || gt[-1] <= s->limitv)
				uil = 1;

			/* Update bounding box for this grid point */
			for (f = 0; f < fdi; f++) {
				if (min[f] > gt[f])	
					 min[f] = gt[f];
				if (max[f] < gt[f])
					 max[f] = gt[f];
			}
		}

		/* Skip any fwd cells that are over the ink limit */
		if (!uil) {
			nskcells++;
			continue;
		}

		/* Figure out intersection range in reverse grid */
		for (f = 0; f < fdi; f++) {
			double t;
			int mi;
			double gw = s->rev.gw[f];
			double gl = s->rev.gl[f];
			t = (min[f] - gl)/gw;
			mi = (int)floor(t);			/* Grid coordinate */
			if (mi < 0)					/* Limit to valid cube base index range */
				mi = 0;
			else if (mi > rgres_1)
				mi = rgres_1;
			imin[f] = mi;	
			t = (max[f] - gl)/gw;
			mi = (int)floor(t);			/* Grid coordinate */
			if (mi < 0)					/* Limit to valid cube base index range */
				mi = 0;
			else if (mi > rgres_1)
				mi = rgres_1;
			imax[f] = mi;	
		}

//printf("Scanning over grid:\n");
//for (f = 0; f < fdi; f++)
//printf("Min[%d] = %d -> Max[%d] = %d\n",f,imin[f],f,imax[f]);

		/* Now create forward index and vector with all the reverse grid cells */
		for (f = 0; f < fdi; f++)
			gc[f] = imin[f];	/* init coords */

		for (f = 0; f < fdi;) {	/* For all of intersect cube */
			int **rpp, *rp;
			
			/* Compute pointer to grid cell */
			for (rpp = s->rev.rev, f = 0; f < fdi; f++)
				rpp += gc[f] * s->rev.coi[f];
			rp = *rpp;

//printf("Currently at grid:\n");
//for (f = 0; f < fdi; f++)
//printf("gc[%d] = %d\n",f,gc[f]);

			if (rp == NULL) {
				if ((rp = (int *) rev_malloc(s, 6 * sizeof(int))) == NULL)
					error("rspl malloc failed - rev.grid entry");
				INCSZ(s, 6 * sizeof(int));
				*rpp = rp;
				rp[0] = 6;		/* Allocation */
				rp[1] = 4;		/* Next free Cell */
				rp[2] = 1;		/* Reference count */
				rp[3] = i;
				rp[4] = -1;		/* End marker */
			} else {
				int z = rp[1], ll = rp[0];
				if (z >= (ll-1)) {			/* Not enough space */
					INCSZ(s, ll * sizeof(int));
					ll *= 2;
					if ((rp = (int *) rev_realloc(s, rp, sizeof(int) * ll)) == NULL)
						error("rspl realloc failed - rev.grid entry");
					*rpp = rp;
					rp[0] = ll;
				}
				rp[z++] = i;
				rp[z] = -1;
				rp[1] = z;
			}
			/* Increment index */
			for (f = 0; f < fdi; f++) {
				gc[f]++;
				if (gc[f] <= imax[f])
					break;	/* No carry */
				gc[f] = imin[f];
			}
		}	/* Next reverse grid point in intersecting cube */

		if (s->rev.fastsetup)
			continue;           /* Skip nnrev setup */


		/* Now also register which grid points are in-gamut and are part of cells */
		/* than have a rev.rev[] list. */

		/* Figure out intersection range in reverse nn (construction) vertex grid */
		/* This range may be empty if a grid isn't stradled by the fwd cell output */
		/* range. */
		for (f = 0; f < fdi; f++) {
			double t;
			int mi;
			double gw = s->rev.gw[f];
			double gl = s->rev.gl[f];
			t = (min[f] - gl)/gw;
			mi = (int)ceil(t);			/* Grid coordinate */
			if (mi < 0)					/* Limit to valid cube base index range */
				mi = 0;
			else if (mi >= argres)
				mi = rgres;
			imin[f] = mi;	
			t = (max[f] - gl)/gw;
			mi = (int)floor(t);			/* Grid coordinate */
			if (mi < 0)					/* Limit to valid cube base index range */
				mi = 0;
			else if (mi >= argres)
				mi = rgres;
			imax[f] = mi;	
			if (imax[f] < imin[f])
				break;					/* Doesn't straddle any verticies */
		}

		if (f >= fdi) {					/* There are seed verticies to mark */

//printf("~1 marking prime seed vertex %d\n",i);

			/* Mark an initial seed point nnrev vertex, and */
			/* create a surface point propogation record for it */
			for (f = 0; f < fdi; f++)
				gc[f] = imin[f];	/* init coords */

			for (f = 0; f < fdi;) {	/* For all of intersect cube */
				int **rpp, *rp;
				char *fpp;
				
				/* Compute pointer to grid cell */
				for (rpp = s->rev.nnrev, fpp = vflag, f = 0; f < fdi; f++) {
					int inc = gc[f] * s->rev.coi[f];
					rpp += inc;
					fpp += inc;
				}
				rp = *rpp;

				*fpp = 3;		/* Initial seed point */
	
				/* Increment index */
				for (f = 0; f < fdi; f++) {
					gc[f]++;
					if (gc[f] <= imax[f])
						break;	/* No carry */
					gc[f] = imin[f];
				}
			}
		}
	}	/* Next base grid point */

	DBG(("We skipped %d cells that were over the limit\n",nskcells));

	/* Setup the nnrev array if we are not doing a fast setup. */
	/* (fastsetup will instead fill the nnrev array on demand, */
	/* using an exaustive search.) */
	if (!s->rev.fastsetup) {

		/* The next step is to use all the prime seed grid points to set and propogate */
		/* the index of the closest fwd vertex through the revnn[] array. */
		/* (This doesn't work perfectly. Sometimes a vertex is not linked to it's closest */
		/*  prime. I'm not sure if this is due to a bug here, or is a quirk of geometry */
		/*  that a prime that is closest to a vertex isn't closest for any of its neighbors.) */
		DBG(("filling in rev.nnrev[] grid\n"));

		/* For all the primary seed points */
		DC_INIT(gg);
		for (i = 0; i < rgno; i++) {
			int **rpp;
			primevx *prime= NULL;				/* prime cell information structure */

			if (vflag[i] != 3) {			/* Not a prime seed point */
				goto next_seed_point;
			}
			
			rpp = s->rev.nnrev + i;

//printf("~1 potential rev.nnrev[] prime seed %d, about to scan neibors\n",i);
			/* For all the neigbors of this seed */
			DC_INIT(cc);
			while (!DC_DONE(cc)) {
				propvx *prop;					/* neighor cell propogation structure */
				int nix = 0;					/* Neighbor cell index */
				char *fpp = vflag;
				int **nrpp = s->rev.nnrev;
				double dsq;
		
				for (f = 0; f < fdi; f++) {
					nn[f] = gg[f] + cc[f];
					if (nn[f] < 0 || nn[f] >= argres)
						break;					/* Out of bounds */
					nix += nn[f] * s->rev.coi[f];
				}
				fpp = vflag + nix;

				/* If neighbor out of bounds, or is a prime seed point, skip it */
				if (f < fdi || *fpp == 3) {
					goto next_neighbor;
				}

//printf("~1 identified prime seed %d with neighbor %d\n",i,nix);
				/* We now know that this prime seed will propogate, */
				/* so get/create the temporary information record for it */
				if (prime == NULL) {

					/* If this prime seed hasn't be setup before */
					if (*rpp != NULL) {
						prime = *((primevx **)rpp);
					} else {
						/* Allocate a primevx if there isn't one */
						if ((prime = (primevx *) calloc(1, sizeof(primevx))) == NULL)
							error("rspl malloc failed - rev.nnrev prime info structs");
						*((primevx **)rpp) = prime;
						prime->ix = i;
						for (f = 0; f < fdi; f++)
							prime->gc[f] = gg[f];
//if (fdi > 1) printf("~1 setting prime %d, gc = %d, %d, %d\n", i, prime->gc[0], prime->gc[1], prime->gc[2]);
					}
				}

				/* Pointer to nnrev vertex neighbor point */
				nrpp = s->rev.nnrev + nix;

				/* Compute the distance squared from this prime seed to this neighbor */
				for (dsq = 0.0, f = 0; f < fdi; f++) {
					double tt = (gg[f] - nn[f]) * s->rev.gw[f];
					dsq += tt * tt;
				}

				/* Get or allocate a prop structure for it */
				if (*nrpp != NULL) {
					prop = *((propvx **)nrpp);
					if ((dsq + 1e-6) < prop->dsq) {		/* This prime is closer than previous */
						prop->cix = i;			/* The index of the closest prime */
						prop->dsq = dsq;		/* Distance squared to closest prime */
					}
				} else {
					if ((prop = (propvx *) calloc(1, sizeof(propvx))) == NULL)
						error("rspl malloc failed - rev.nnrev propogation structs");
					*((propvx **)nrpp) = prop;
					prop->ix = nix;
					for (f = 0; f < fdi; f++)
						prop->gc[f] = nn[f];	/* This neighbors coord */
					prop->cix = i;
					prop->dsq = dsq;
					prop->pass = pass;
					prop->next = nlist;			/* Add new seed to list of next seeds */
					nlist = prop;
					*fpp = 1;

				}
				next_neighbor:;
				DC_INC(cc);
			}

			next_seed_point:;
			DC_INC(gg);
		}

//printf("~1 about to propogate secondary seeds\n");
		/* Now we propogate the secondary seed points until there are no more left */
		while(nlist != NULL) {
			propvx *next;
			propvx *tlp;

			if ((pass += 2) < 0)
				error("Assert rev: excessive propogation passes");
//printf("~1 about to do a round of propogation pass %d\n",(pass+2)/2);

			/* Mark all seed points on the current list with pass-1 */
			for (tlp = nlist; tlp != NULL; tlp = tlp->next) {
				*(vflag + tlp->ix) = 2;
				tlp->pass = pass-1;
			}

			/* Go through each secondary seed in the active list, propogating them */
			for (alist = nlist, nlist = NULL; alist != NULL; alist = next) {
				int **rpp;
				primevx *prime= NULL;			/* prime cell information structure */

				next = alist->next;				/* Next unless we re-insert one */
				
				/* Grab this seed points coodinate and index */
				for (i = f = 0; f < fdi; f++) {
					gg[f] = alist->gc[f];
					i += gg[f] * s->rev.coi[f];
				}

//printf("\n~1 propogating from seed %d\n",i);
				/* rpp = s->rev.nnrev + i; */
				
				/* Grab the corresponding prime seed information record */
				prime = *((primevx **)(s->rev.nnrev + alist->cix));

				/* For all the neigbors of this seed */
				DC_INIT(cc);
				while (!DC_DONE(cc)) {
					propvx *prop;				/* neighor cell propogation structure */
					int nix;					/* Neighbor cell index */
					char *fpp = vflag;
					int **nrpp = s->rev.nnrev;
					double dsq;
		
					for (nix = f = 0; f < fdi; f++) {
						nn[f] = gg[f] + cc[f];
						if (nn[f] < 0 || nn[f] >= argres)
							break;					/* Out of bounds */
						nix += nn[f] * s->rev.coi[f];
					}
					fpp = vflag + nix;
//printf("~1 neighbor ix %d, flag %d\n",nix,*fpp);

					/* If neighbor out of bounds, current vertex or prime, skip it */
					if (f < fdi || i == nix || *fpp >= 3) {
//printf("~1 skipping neighbour %d\n",nix);
						goto next_neighbor2;
					}

					/* Pointer to nnrev vertex neighbor point */
					nrpp = s->rev.nnrev + nix;

					/* Compute the distance squared from the prime seed to this neighbor */
					for (dsq = 0.0, f = 0; f < fdi; f++) {
						double tt = (prime->gc[f] - nn[f]) * s->rev.gw[f];
						dsq += tt * tt;
					}

					/* Get or allocate a prop structure for it */
					if (*nrpp != NULL) {
						prop = *((propvx **)nrpp);
//if (prop->ix != nix) error ("Assert: prop index %d doesn't match index %d",prop->ix, nix);

						if ((dsq + 1e-6) < prop->dsq) {		/* This prime is closer than previous */
//printf("~1 updating %d to prime %d, dsq = %f from %f\n",nix, prime->ix, dsq, prop->dsq);
							prop->cix = prime->ix;	/* The index of the new closest prime */
							prop->dsq = dsq;		/* Distance squared to closest prime */
							/* If this is a vertex from previous pass that has changed, */
							/* and it's not ahead of us in the current list, */
							/* put it next on the current list. */
							if (*fpp == 2 && prop->pass != (pass-1)) {
//printf("~1 re-shedule %d (%d) for next propogate\n",nix,prop->ix);
//if (next == NULL)
//printf("Before insert, next = NULL\n");
//else
//printf("Before insert, next = %d\n",next->ix);
								prop->pass = pass-1;	/* Re-shedule once only */
								prop->next = next;
								next = prop;
							}
						}
					} else {
						if ((prop = (propvx *) calloc(1, sizeof(propvx))) == NULL)
							error("rspl malloc failed - rev.nnrev propogation structs");
						*((propvx **)nrpp) = prop;
						prop->ix = nix;
						for (f = 0; f < fdi; f++)
							prop->gc[f] = nn[f];	/* This neighbors coord */
						prop->cix = prime->ix;
						prop->dsq = dsq;
//printf("~1 propogating to new, %d, dsq = %f, prime %d\n",nix, dsq, prime->ix);
						prop->pass = pass;
						prop->next = nlist;		 /* Add new seed to list of next seeds */
						nlist = prop;
						*fpp = 1;
					}

					next_neighbor2:;
					DC_INC(cc);
				}
				alist->pass = pass;
			}
		}

#ifdef DEBUG
		DBG(("checking that every vertex is now touched\n"));
		for (i = 0; i < rgno; i++) {
			if (vflag[i] < 2) {
				printf("~1 problem: vertex %d flag = %d\n",i, vflag[i]);
			}
			if (vflag[i] == 2 && *(s->rev.nnrev + i) == NULL) {
				printf("~1 problem: vertex %d flag = %d and struct = NULL\n",i, vflag[i]);
			}
		}
#endif /* DEBUG */

#ifdef NEVER		/* Check that all cells are closest to their primes than any other */
DC_INIT(gg);
for (i = 0; i < rgno; i++) {		/* For all the verticies */
		if (vflag[i] == 2) {
			propvx *prop = (propvx *) *(s->rev.nnrev + i);
			for (j = 0; j < rgno; j++) {	/* For all the primes */
				if (vflag[j] == 3) {
					primevx *prime = (primevx *) *(s->rev.nnrev + j);
					double dsq;
					if (prime == NULL)
						continue;
					for (dsq = 0.0, f = 0; f < fdi; f++) {
						double tt = (prime->gc[f] - prop->gc[f]) * s->rev.gw[f];
						dsq += tt * tt;
					}
					if ((dsq + 1e-6) < prop->dsq) {
						warning("~1 vertex %d prime %d, dsq = %f, is closer to prime %d, dsq %f\n", i,prop->cix, prop->dsq, j, dsq);
						/* See if any of the neighbors have the closer prime */
						DC_INIT(cc); /* For all the neigbors of this seed */
						while (!DC_DONE(cc)) {
							propvx *nprop;				/* neighor cell propogation structure */
							int nix;					/* Neighbor cell index */
							char *fpp = vflag;
							int **nrpp = s->rev.nnrev;
							double dsq;
				
							for (nix = f = 0; f < fdi; f++) {
								nn[f] = gg[f] + cc[f];
								if (nn[f] < 0 || nn[f] >= argres)
									break;					/* Out of bounds */
								nix += nn[f] * s->rev.coi[f];
							}
							fpp = vflag + nix;
//printf("~1 neighbor ix %d, flag %d\n",nix,*fpp);

							/* If neighbor out of bounds, current vertex or prime, skip it */
							if (f < fdi || i == nix || *fpp != 2) {
//printf("~1 skipping neighbour %d\n",nix);
								goto next_neighbor3;
							}

							/* Pointer to nnrev vertex neighbor point */
							nrpp = s->rev.nnrev + nix;
							if ((nprop = *((propvx **)nrpp)) != NULL) {
//printf("~1 neighbor %d %d %d has prime %d dsq %f\n",cc[0],cc[1],cc[2],nprop->cix,nprop->dsq);
								if (nprop->cix == j) {
//warning("~1 but neighbor has this prime point!\n");

								}
							}
							next_neighbor3:;
							DC_INC(cc);
						}
//					prop->cix = j;			/* Fix it and see what happens */
//					prop->dsq = dsq;
					}
				}
			}
		}
		DC_INC(gg);
}

#endif /* NEVER */


		DBG(("about to do convert vertex values to cell lists\n"));
		/* Setup a cache for the fwd cell lists created, so that we can */
		/* avoid the list creation and memory allocation for identical lists */
		nncsize = s->rev.ares * s->rev.ares;
		if ((nnc = (nncache **) calloc(nncsize, sizeof(nncache *))) == NULL)
			error("rspl malloc failed - rev.nnc cache entries");

		/* Now convert the nnrev secondary vertex points to pointers to fwd cell lists */
		/* Do this in order, so that we don't need the verticies after */
		/* they are converted to cell lists. */
		DC_INIT(gg);
		for (i = 0; i < rgno; i++) {
			int **rpp, *rp;
			propvx *prop = NULL;			/* vertex information structure */
			primevx *prime= NULL;			/* prime cell information structure */
			int imin[MXRO], imax[MXRO];		/* Prime vertex range for each axis */
			double rmin[MXRO], rmax[MXRO];	/* Float prime vertex value range */
			unsigned int tcount;			/* grid touch count for this opperation */
			datao min, max;					/* Fwd cell output range */
			int lpix;						/* Last prime index seen */

//if (fdi > 1) printf("~1 converting vertex %d\n",i);
//if (fdi > 1) printf("~1 coord %d %d %d\n",gg[0],gg[1],gg[2]); 

			rpp = s->rev.nnrev + i;
			if (vflag[i] == 3) {			/* Cell base is prime */
				prime = (primevx *) *rpp;

				if (prime != NULL) {			/* It's a propogating prime */
					/* Add prime to the end of the ptime linked list */
					prime->next = NULL;
					if (plist == NULL) {
						plist = ptail = prime;
					} else {
						ptail->next = prime;
						ptail = prime;
					}
				}
			} else if (vflag[i] == 2) {		/* Cell base is secondary */
				prop = (propvx *)*rpp;
			} else {								/* Hmm */
				/* This seems to happen if the space explored is not really 3D ? */
				if (s->rev.primsecwarn == 0) {
					warning("rev: bwd vertex %d is not prime or secondary (vflag = %d)"
					"(Check that your measurement data is sane!)",i,vflag[i]);
					s->rev.primsecwarn = 1;
				}
				fill_nncell(s, gg, i);		/* Is this valid to do ?? */
				continue;
			}
		
			/* Setup to scan of cube corners, and check that base is within cube grid */
			for (f = 0; f < fdi; f++) {
				if (gg[f] > rgres_1) {		/* Vertex outside bwd cell range, */
					if (prop != NULL && prime == NULL) {
						free(prop);
						*rpp = NULL;
					}
//printf("~1 done vertex %d because its out of cell bounds\n",i);
					goto next_vertex;
				}
				imin[f] = 0x7fffffff;
				imax[f] = -1;
			}

			/* For all the vertex points in the nnrev cube starting at this base (i), */
			/* Check if any of them are secondary seed points */
			for (ff = 0; ff < (1 << fdi); ff++) {
				if (vflag[i + s->rev.hoi[ff]] == 2)
					break;
			}

			/* If not a cell that we want to create a nearest fwd cell list for */
			if (ff >= (1 << fdi)) {
				/* Don't free anything, because we leave a prime in place, */
				/* and it can't be a prop. */
				goto next_vertex;
			}

			/* For all the vertex points in the nnrev cube starting at this base (i), */
			/* accumulate the range they cover */
			lpix = -1;
			for (f = 0; f < fdi; f++) {
				imin[f] = 0x7fffffff;
				imax[f] = -1;
			}
			for (ff = 0; ff < (1 << fdi); ff++) {
				int ii = i + s->rev.hoi[ff];	/* cube vertex index */
				primevx *tprime= NULL;
				
				/* Grab vertex info and corresponding prime vertex info */
				if (vflag[ii] == 3) {								/* Corner is a prime */
					tprime = (primevx *) *(s->rev.nnrev + ii);		/* Use itself */
					if (tprime == NULL)
						continue;				/* Not a propogated in-gamut vertex */
				} else if (vflag[ii] == 2) {
					propvx *tprop = (propvx *) *(s->rev.nnrev + ii);	/* Use propogated prime */
					tprime = (primevx *) *(s->rev.nnrev + tprop->cix);
				} else {
					continue;		/* Hmm */
				}
				if (tprime->ix == lpix)
					continue;		/* Don't waste time */

//if (fdi > 1) printf("~1 corner %d, ix %d, prime %d gc = %d, %d, %d\n", ff, ii, tprime->ix, tprime->gc[0], tprime->gc[1], tprime->gc[2]);

				/* Update bounding box for this prime grid point */
				for (f = 0; f < fdi; f++) {
					if (tprime->gc[f] < imin[f])
						 imin[f] = tprime->gc[f];
					if (tprime->gc[f] > imax[f])
						 imax[f] = tprime->gc[f];
				}
				lpix = tprime->ix;
			}

//if (fdi > 1) printf("~1 prime vertex index range = %d - %d, %d - %d, %d - %d\n", imin[0], imax[0], imin[1], imax[1], imin[2], imax[2]);

			/* See if a list matching this range is in the cache */
			hashk = 0;
			for (hashk = f = 0; f < fdi; f++)
				hashk = hashk * 97 + imin[f] + 43 * (imax[f] - imin[f]);
			hashk = hashk % nncsize;
//if (fdi > 1) printf("~1 hashk = %d from %d - %d %d - %d %d - %d\n", hashk, imin[0], imax[0], imin[1], imax[1], imin[2], imax[2]);

			/* See if we can locate an existing list for this range */
			for (ncp = nnc[hashk]; ncp != NULL; ncp = ncp->next) {
//if (fdi > 1) printf("~1 checking %d - %d %d - %d %d - %d\n", ncp->min[0], ncp->max[0], ncp->min[1], ncp->max[1], ncp->min[2], ncp->max[2]);
				for (f = 0; f < fdi; f++) {
					if (ncp->min[f] != imin[f]
					 || ncp->max[f] != imax[f]) {
//if (fdi > 1) printf("~1 not a match\n");
						break;
					}
				}
				if (f >= fdi) {
//if (fdi > 1) printf("~1 got a match\n");
					break;			/* Found a matching cache entry */
				}
			}

			if (ncp != NULL) {
				rp = ncp->rip;
//if (fdi > 1) printf("~1 got cache hit hashk = %d, with ref count %d\n\n",hashk, rp[1]);
				rp[2]++;			/* Increase reference count */

			} else {
				/* This section seems to be the most time consuming part of the nnrev setup. */

				/* Allocate a cache entry and place it */
				if ((ncp = (nncache *)calloc(1, sizeof(nncache))) == NULL)
					error("rspl malloc failed - rev.nn cach record");

				for (f = 0; f < fdi; f++) {
					ncp->min[f] = imin[f];
					ncp->max[f] = imax[f];
				}
				ncp->next = nnc[hashk];
				nnc[hashk] = ncp;

				/* Convert the nn destination vertex range into an output value range. */
				for (f = 0; f < fdi; f++) {
					double gw = s->rev.gw[f];
					double gl = s->rev.gl[f];
					rmin[f] = gl + imin[f] * gw;
					rmax[f] = gl + imax[f] * gw;
				}

				/* Do any adjustment of the range needed to acount for the inacuracies */
				/* caused by the vertex quantization. */
				/* (I don't really understand the need for the extra avggw expansion, */
				/*  but there are artefacts without this. This size of this sampling */
				/*  expansion has a great effect on the performance.) */
				{
					double avggw = 0.0;
					for (f = 0; f < fdi; f++) 
						avggw += s->rev.gw[f];
					avggw /= (double)fdi;
					for (f = 0; f < fdi; f++) {		/* Quantizing range plus extra */
						double gw = s->rev.gw[f];
						rmin[f] -= (0.5 * gw + 0.99 * avggw);
						rmax[f] += (0.5 * gw + 0.99 * avggw);
					}
				}
//if (fdi > 1) printf("~1 prime vertex value adjusted range = %f - %f, %f - %f, %f - %fn", rmin[0], rmax[0], rmin[1], rmax[1], rmin[2], rmax[2]);

				/* computue the rev.rev cell grid range we will need to cover to */
				/* get all the cell output ranges that could touch our nn reverse range */
				for (f = 0; f < fdi; f++) {
					double gw = s->rev.gw[f];
					double gl = s->rev.gl[f];
					imin[f] = (int)floor((rmin[f] - gl)/gw);
					if (imin[f] < 0)
						imin[f] = 0;
					else if (imin[f] > rgres_1)
						 imin[f] = rgres_1;
					imax[f] = (int)floor((rmax[f] - gl)/gw);
					if (imax[f] < 0)
						imax[f] = 0;
					else if (imax[f] > rgres_1)
						 imax[f] = rgres_1;
					cc[f] = imin[f];				/* Set grid starting value */
				}
				tcount = s->get_next_touch(s);		/* Get next grid touched generation count */

//if (fdi > 1) printf("~1 Cells to scan = %d - %d, %d - %d, %d - %d\n", imin[0], imax[0], imin[1], imax[1], imin[2], imax[2]);

				rp = NULL;							/* We always allocate a new list initially */
				for (f = 0; f < fdi;) {		/* For all the cells in the min/max range */
					int ii;
					int **nrpp, *nrp;	/* Pointer to base of cell list, entry 0 = allocated space */

					/* Get pointer to rev.rev[] cell list */
					for (nrpp = s->rev.rev, f = 0; f < fdi; f++)
						nrpp += cc[f] * s->rev.coi[f];

					if ((nrp = *nrpp) == NULL)
						goto next_range_list;		/* This rev.rev[] cell is empty */


//if (fdi > 1) printf("~1 adding list from cell %d, list length %d\n",nrpp - s->rev.rev, nrp[0]);
					/* For all the fwd cells in the rev.rev[] list */
					for(nrp += 3; *nrp != -1; nrp++)  {
						int ix = *nrp;			/* Fwd cell index */
						float *fcb = s->g.a + ix * s->g.pss; /* Pntr to base float of fwd cell */

						if (TOUCHF(fcb) >= tcount) {	/* If we seen visited this fwd cell before */
//if (fdi > 1) printf("~1 skipping cell %d because we alread have it\n",ix);
							continue;
						}
						TOUCHF(fcb) = tcount;			/* Touch it so we will skip it next time */

						/* Compute the range of output values this cell covers */
						for (f = 0; f < fdi; f++)	/* Init output min/max */
							min[f] = max[f] = fcb[f];

						/* For all other grid points in the fwd cell cube */
						for (ee = 1; ee < (1 << di); ee++) {
							float *gt = fcb + s->g.fhi[ee];	/* Pointer to cube vertex */
							
							/* Update bounding box for this grid point */
							for (f = 0; f < fdi; f++) {
								if (min[f] > gt[f])	
									 min[f] = gt[f];
								if (max[f] < gt[f])
									 max[f] = gt[f];
							}
						}

//if (fdi > 1) printf("~1 cell %d range = %f - %f, %f - %f, %f - %f\n", ix, min[0], max[0], min[1], max[1], min[2], max[2]);

						/* See if this fwd cell output values overlaps our region of interest */
						for (f = 0; f < fdi; f++) {
							if (min[f] > rmax[f]
							 || max[f] < rmin[f]) {
								break;				/* Doesn't overlap */
							}
						}

						if (f < fdi) {
//if (fdi > 1) printf("~1 skipping cell %d because we doesn't overlap\n",ix);
							continue;				/* It doesn't overlap */
						}

//if (fdi > 1) printf("~1 adding fwd index %d to list\n",ix);
//if (fdi > 1) printf("~1 cell %d range = %f - %f, %f - %f, %f - %f\n", ix, min[0], max[0], min[1], max[1], min[2], max[2]);
#ifdef DEBUG
						fwdcells++;
#endif
						/* It does, add it to our new list */
						if (rp == NULL) {
							if ((rp = (int *) rev_malloc(s, 6 * sizeof(int))) == NULL)
								error("rspl malloc failed - rev.nngrid entry");
							INCSZ(s, 6 * sizeof(int));
							rp[0] = 6;		/* Allocation */
							rp[1] = 4;		/* Next free Cell */
							rp[2] = 1;		/* reference count */
							rp[3] = ix;
							rp[4] = -1;
						} else {
							int z = rp[1], ll = rp[0];
							if (z >= (ll-1)) {			/* Not enough space */
								INCSZ(s, ll * sizeof(int));
								ll *= 2;
								if ((rp = (int *) rev_realloc(s, rp, sizeof(int) * ll)) == NULL)
									error("rspl realloc failed - rev.grid entry");
								rp[0] = ll;
							}
							rp[z++] = ix;
							rp[z] = -1;
							rp[1] = z;
						}
					}			/* Next fwd cell in list */

					/* Increment index */
					next_range_list:;
					for (f = 0; f < fdi; f++) {
						if (++cc[f] <= imax[f])
							break;	/* No carry */
						cc[f] = imin[f];
					}
				}

				ncp->rip = rp;		/* record nnrev cell in cache */
#ifdef DEBUG
				cellinrevlist++;
#endif
//if (fdi > 1) printf("~1 adding cache entry with hashk = %d\n\n",hashk);
			}

			/* Put the resulting list in place */
			if (prime != NULL)
				prime->clist = rp;	/* Save it untill we get rid of the primes */
			else
				*rpp = rp;

//if (*rpp == NULL) printf("~1 problem: we ended up with no list or prime struct at cell %d\n",i);

#ifdef NEVER
/* Sanity check the list, to see if the list cells corner contain an output value */
/* that is at least closer to the target than the prime. */
if (prop != NULL) {
		int *tp = rp;
		double bdist = 1e60;
		double bdist2 = 1e60;
		double vx[MXRO];		/* Vertex location */
		double px[MXRO];		/* Prime location */
		double cl[MXRO];		/* Closest output value from list */
		double acl[MXRO];		/* Absolute closest output value */
		double dst;				/* Distance to prime */
		int ti;

		primevx *prm = (primevx *) *(s->rev.nnrev + prop->cix);
		for (f = 0; f < fdi; f++) {
		double gw = s->rev.gw[f];
		double gl = s->rev.gl[f];
		vx[f] = gl + prop->gc[f] * gw;
		px[f] = gl + prm->gc[f] * gw;
		}

		for(tp++; *tp != -1; tp++)  {
		int ix = *tp;			/* Fwd cell index */
		float *fcb = s->g.a + ix * s->g.pss; /* Pntr to base float of fwd cell */

		for (ee = 0; ee < (1 << di); ee++) {
			double ss;
			float *gt = fcb + s->g.fhi[ee];	/* Pointer to cube vertex */
						
			for (ss = 0.0, f = 0; f < fdi; f++) {
				double tt = vx[f] - gt[f];
				ss += tt * tt;
			}
			if (ss < bdist) {
				bdist = ss;
				for (f = 0; f < fdi; f++)
					cl[f] = gt[f];
			}
		}
		}
		bdist = sqrt(bdist);
		dst = sqrt(prop->dsq);

		/* Lookup best distance to any output value */
		if (dst < bdist) {
		float *gt;
		for (ti = 0, gt = s->g.a; ti < s->g.no; ti++, gt += s->g.pss) {
			double ss;
						
			for (ss = 0.0, f = 0; f < fdi; f++) {
				double tt = vx[f] - gt[f];
				ss += tt * tt;
			}
			if (ss < bdist2) {
				bdist2 = ss;
				for (f = 0; f < fdi; f++)
					acl[f] = gt[f];
			}
		}
		}
		bdist2 = sqrt(bdist2);

		if (dst < bdist) {
		printf("~1 vertex %d has worse distance to values than prime\n",i);
		printf("~1 vertex loc %f %f %f\n", vx[0], vx[1], vx[2]);
		printf("~1 prime loc %f %f %f, dist %f\n", px[0], px[1], px[2],dst);
		printf("~1 closest loc %f %f %f, dist %f\n", cl[0], cl[1], cl[2],bdist);
		printf("~1 abs clst loc %f %f %f, dist %f\n", acl[0], acl[1], acl[2], bdist2);
		}
}
#endif // NEVER

			if (prop != NULL && prime == NULL) {
				free(prop);
			}

			next_vertex:;
			DC_INC(gg);
		}

		DBG(("freeing up the prime seed structurs\n"));
		/* Finaly convert all the prime verticies to cell lists */
		/* Free up all the prime seed structures */
		for (;plist != NULL; ) {
			primevx *prime, *next = plist->next;
			int **rpp;

			rpp = s->rev.nnrev + plist->ix;
			if ((prime = (primevx *)(*rpp)) != NULL) {
				if (prime->clist != NULL) 		/* There is a nn list for this cell */
					*rpp = prime->clist;
				else
					*rpp = NULL;
				free(prime); 
			} else {
				error("assert, prime cell %d was empty",plist->ix);
			}
			plist = next;
		}

#ifdef DEBUG
		DBG(("sanity check that all rev accell cells are filled\n"));
		DC_INIT(gg);
		for (i = 0; i < rgno; i++) {
			for (f = 0; f < fdi; f++) {
				if (gg[f] > rgres_1) {	/* Vertex outside bwd cell range, */
					goto next_vertex3;
				}
			}

			if (*(s->rev.nnrev + i) == NULL
			 && *(s->rev.rev + i) == NULL) {
//			printf("~1 warning, cell %d [ %d %d %d] has a NULL list\n",i, gg[0],gg[1],gg[2]);
				error("cell %d has a NULL list\n",i);
			}
			next_vertex3:;
			DC_INC(gg);
		}
#endif /* DEBUG */

		/* Free up flag array used for construction */
		if (vflag != NULL) {
			DECSZ(s, rgno * sizeof(char));
			free(vflag);
		}

		/* Free up nn list cache indexing structure used in construction */
		if (nnc != 0) {
			for (i = 0; i < nncsize; i++) {
				nncache *nncp;
				/* Run through linked list freeing entries */
				for (ncp = nnc[i]; ncp != NULL; ncp = nncp) {
					nncp = ncp->next;
					free(ncp);
				}
			}
			free(nnc);
			nnc = NULL;
		}
	}

	if (s->rev.rev_valid == 0 && di > 1) {
		rev_struct *rsi;
		size_t ram_portion = g_avail_ram;

		/* Add into linked list */
		s->rev.next = g_rev_instances;
		g_rev_instances = &s->rev;

		/* Aportion the memory, and reduce cache if it is over new limit. */
		g_no_rev_cache_instances++;
		ram_portion /= g_no_rev_cache_instances; 
		for (rsi = g_rev_instances; rsi != NULL; rsi = rsi->next) {
			revcache *rc = rsi->cache;

			rsi->max_sz = ram_portion;
			while (rc->nunlocked > 0 && rsi->sz > rsi->max_sz) {
				if (decrease_revcache(rc) == 0)
					break;
			}
//printf("~1 rev instance ram = %d MB\n",rsi->sz/1000000);
		}
		
		if (s->verbose)
			fprintf(stdout, "%cThere %s %d rev cache instance%s with %lu Mbytes limit\n",
			                    cr_char,
								g_no_rev_cache_instances > 1 ? "are" : "is",
			                    g_no_rev_cache_instances,
								g_no_rev_cache_instances > 1 ? "s" : "",
			                    (unsigned long)ram_portion/1000000);
	}
	s->rev.rev_valid = 1;

#ifdef DEBUG
	if (fdi > 1) printf("%d cells in rev nn list\n",cellinrevlist);
	if (fdi > 1) printf("%d fwd cells in rev nn list\n",fwdcells);
	if (cellinrevlist > 1) printf("Avg list size = %f\n",(double)fwdcells/cellinrevlist);
#endif

	DBG(("init_revaccell finished\n"));
}

/* Invalidate the reverse acceleration structures (section Two) */
static void invalidate_revaccell(
rspl *s		/* Pointer to rspl grid */
) {
	int e, di = s->di;
	int **rpp, *rp;

	/* Invalidate the whole rev cache (Third section) */
	invalidate_revcache(s->rev.cache);

	/* Free up the contents of rev.rev[] and rev.nnrev[] */
	if (s->rev.rev != NULL) {
		for (rpp = s->rev.rev; rpp < (s->rev.rev + s->rev.no); rpp++) {
			if ((rp = *rpp) != NULL && --rp[2] <= 0) {
				DECSZ(s, rp[0] * sizeof(int));
				free(*rpp);
				*rpp = NULL;
			}
		}
	}
	if (s->rev.nnrev != NULL) {
		for (rpp = s->rev.nnrev; rpp < (s->rev.nnrev + s->rev.no); rpp++) {
			if ((rp = *rpp) != NULL && --rp[2] <= 0) {
				DECSZ(s, rp[0] * sizeof(int));
				free(*rpp);
				*rpp = NULL;
			}
		}
	}

	if (di > 1 && s->rev.rev_valid) {
		rev_struct *rsi, **rsp;
		size_t ram_portion = g_avail_ram;

		/* Remove it from the linked list */
		for (rsp = &g_rev_instances; *rsp != NULL; rsp = &((*rsp)->next)) {
			if (*rsp == &s->rev) {
				*rsp = (*rsp)->next;
				break;
			}
		}

		/* Aportion the memory */
		g_no_rev_cache_instances--;

		if (g_no_rev_cache_instances > 0) {
			ram_portion /= g_no_rev_cache_instances; 
			for (rsi = g_rev_instances; rsi != NULL; rsi = rsi->next)
				rsi->max_sz = ram_portion;
			if (s->verbose)
				fprintf(stdout, "%cThere %s %d rev cache instance%s with %lu Mbytes limit\n",
				                cr_char,
								g_no_rev_cache_instances > 1 ? "are" : "is",
			                    g_no_rev_cache_instances,
								g_no_rev_cache_instances > 1 ? "s" : "",
			                    (unsigned long)ram_portion/1000000);
		}
	}
	s->rev.rev_valid = 0;
}

/* ====================================================== */

/* Initialise the rev First section, basic information that doesn't change */
/* This is called on initial setup when s->rev.inited == 0 */
static void make_rev_one(
rspl *s
) {
	int i, j;		/* Index of fwd grid point */
	int e, f, ee, ff;
	int di = s->di;
	int fdi = s->fdi;
	int rgno, gno = s->g.no;
	int argres;		/* Allocation rgres, = no cells +1 */
	int rgres;
	int rgres_1;	/* rgres -1 == maximum base coord value */
	datao rgmin, rgmax;

	DBG(("make_rev_one called, di = %d, fdi = %d, mgres = %d\n",di,fdi,(int)s->g.mres));

//printf("~1 nnb = %d\n",nnb);

	s->get_out_range(s, rgmin, rgmax);	/* overall output min/max */

	/* Expand out range to encompass declared range */
	/* The declared range is assumed to be the range over which */
	/* we may want an reasonably accurate nearest reverse lookup. */
	for (f = 0; f < fdi; f++) {
		if ((s->d.vl[f] + s->d.vw[f]) > rgmax[f])
				rgmax[f] = s->d.vl[f] + s->d.vw[f];
		if (s->d.vl[f] < rgmin[f])
				rgmin[f] = s->d.vl[f];
	}

	/* Expand out range slightly to allow for out of gamut points */
	for (f = 0; f < fdi; f++) {
		double del = (rgmax[f] - rgmin[f]) * 0.10;	/* Expand by +/- 10% */
		rgmax[f] += del;
		rgmin[f] -= del;
	}
//printf("~~got output range\n");

	/* Heuristic - reverse grid acceleration resolution ? */
	/* Should this really be adapted to be constant in output space ? */
	/* (ie. make the gw aprox equal ?) Would complicate code rev accell */
	/* indexing though. */
	{
		char *ev;
		double gresmul = REV_ACC_GRES_MUL;		/* Typically 2.0 */

		if ((gresmul * s->g.mres) > (double)REV_ACC_GRES_LIMIT) {
			gresmul = (double)REV_ACC_GRES_LIMIT/s->g.mres;		/* Limit target res to typ. 43. */
		}

		/* Allow the user to override if it causes memory consumption problems */
		/* or to speed things up if more memory is available */
		if ((ev = getenv("ARGYLL_REV_ACC_GRID_RES_MULT")) != NULL) {
			double mm;
			mm = atof(ev);
			if (mm > 0.1 && mm < 20.0)
				gresmul *= mm;
		}
		/* Less than 4 is not functional */
		if ((rgres = (int) gresmul * s->g.mres) < 4)
			rgres = 4;
	}
	argres = rgres+1;
	s->rev.ares = argres;		/* == number of verticies per side, used for construction */
	s->rev.res = rgres;			/* == number of cells per side */
	rgres_1 = rgres-1;

	/* Number of elements in the rev.grid, including construction extra rows */
	for (rgno = 1, f = 0; f < fdi; f++, rgno *= argres);
	s->rev.no = rgno;

//printf("~1 argres = %d\n",argres);
	/* Compute coordinate increments */
	s->rev.coi[0] = 1;
//printf("~1 coi[0] = %d\n",s->rev.coi[0]);
	for (f = 1; f < fdi; f++) {
		s->rev.coi[f] = s->rev.coi[f-1] * argres;
//printf("~1 coi[%d] = %d\n",f,s->rev.coi[f]);
	}

	/* Compute index offsets from base of cube to other corners. */

	for (s->rev.hoi[0] = f = 0, j = 1; f < fdi; j *= 2, f++) {
		for (i = 0; i < j; i++)
			s->rev.hoi[j+i] = s->rev.hoi[i] + s->rev.coi[f];	/* In grid points */
	}
//for (ff = 0; ff < (1 << fdi); ff++)
//printf("~1 hoi[%d] = %d\n",ff,s->rev.hoi[ff]);

	/* Conversion from output value to cell indexes */
	for (f = 0; f < fdi; f++) {
		s->rev.gl[f] = rgmin[f];
		s->rev.gh[f] = rgmax[f];
		s->rev.gw[f] = (rgmax[f] - rgmin[f])/(double)rgres;
	}

	if ((s->rev.rev = (int **) rev_calloc(s, rgno, sizeof(int *))) == NULL)
		error("rspl malloc failed - rev.grid points");
	INCSZ(s, rgno * sizeof(int *));

	if ((s->rev.nnrev = (int **) rev_calloc(s, rgno, sizeof(int *))) == NULL)
		error("rspl malloc failed - rev.nngrid points");
	INCSZ(s, rgno * sizeof(int *));

	s->rev.inited = 1;

	s->rev.stouch = 1;

	DBG(("make_rev_one finished\n"));
}

/* ====================================================== */

/* First section of rev_struct init. */
/* Initialise the reverse cell cache, sub simplex information */
/* and reverse lookup acceleration structures. */
/* This is called by a reverse interpolation call */
/* that discovers that the reverse index list haven't */
/* been initialised. */
static void make_rev(
rspl *s
) {
	int e, di = s->di;
	char *ev;
	size_t avail_ram = 256 * 1024 * 1024;	/* Default assumed RAM in the system */
	size_t ram1, ram2;						/* First Gig and rest */
	static int repsr = 0;					/* Have we reported system RAM size ? */
	size_t max_vmem = 0;

	DBG(("make_rev called, di = %d, fdi = %d, mgres = %d\n",di,s->fdi,(int)s->g.mres));

	/* Figure out how much RAM we can use for the rev cache. */
	/* (We compute this for each rev instance, to account for any VM */
	/* limit changes due to intervening allocations) */
	if (di > 1 || g_avail_ram == 0) {
	#ifdef NT 
		{
			BOOL (WINAPI* pGlobalMemoryStatusEx)(MEMORYSTATUSEX *) = NULL;
			MEMORYSTATUSEX mstat;
	
			pGlobalMemoryStatusEx = (BOOL (WINAPI*)(MEMORYSTATUSEX *))
			                        GetProcAddress(LoadLibrary("KERNEL32"), "GlobalMemoryStatusEx");
	
			if (pGlobalMemoryStatusEx == NULL)
				error("Unable to link to GlobalMemoryStatusEx()");
			mstat.dwLength = sizeof(MEMORYSTATUSEX);
			if ((*pGlobalMemoryStatusEx)(&mstat) != 0) {
				if (sizeof(avail_ram) < 8 && mstat.ullTotalPhys > 0xffffffffL)
					mstat.ullTotalPhys = 0xffffffffL;
				avail_ram = mstat.ullTotalPhys;
			} else {
				warning("%cWarning - Unable to get system memory size",cr_char);
			}
		}
	#else
	#ifdef __APPLE__
		{
			long long memsize;
			size_t memsize_sz = sizeof(long long);
			if (sysctlbyname("hw.memsize", &memsize, &memsize_sz, NULL, 0) == 0) {
				if (sizeof(avail_ram) < 8 && memsize > 0xffffffffL)
					memsize = 0xffffffff;
				avail_ram = memsize;
			} else {
				warning("%cWarning - Unable to get system memory size",cr_char);
			}
			
		}
	#else	/* Linux */
		{
			long long total;
			total = (long long)sysconf(_SC_PAGESIZE) * (long long)sysconf(_SC_PHYS_PAGES);
			if (sizeof(avail_ram) < 8 && total > 0xffffffffL)
				total = 0xffffffffL;
			avail_ram = total;
		}
	#endif
	#endif
		DBG(("System RAM = %d Mbytes\n",avail_ram/1000000));
	
		/* Make it sane */
		if (avail_ram < (256 * 1024 * 1024)) {
			warning("%cWarning - System RAM size seems very small (%d MBytes),"
			        " assuming 256Mb instead",cr_char,avail_ram/1000000);
			avail_ram = 256 * 1024 * 1024;
		}
		// avail_ram = -1;		/* Fake 4GB of RAM. This will swap! */
	
		ram1 = avail_ram;
		ram2 = 0;
		if (ram1 > (1024 * 1024 * 1024)) {
			ram1 = 1024 * 1024 * 1024;
			ram2 = avail_ram - ram1;
		}
	
		/* Default maximum reverse memory (typically 50% of the first Gig, 75% of the rest) */
		g_avail_ram = (size_t)(REV_MAX_MEM_RATIO * ram1
		            +          REV_MAX_MEM_RATIO2 * ram2);
	
		/* Many 32 bit systems have a virtual memory limit, so we'd better stay under it. */
		/* This is slightly dodgy though, since we don't know how much memory other */
		/* software will need to malloc. A more sophisticated approach would be to */
		/* replace all malloc/calloc/realloc calls in the exe with a version that on failure, */
		/* sets the current memory usage as the new limit, and then */
		/* frees up some rev cache space before re-trying. This is a non-trivial change */
		/* to the source code though, and really has to include all user mode */
		/* libraries we're linked to, making implementation problematic. */ 
		/* Instead we do a simple test to see what the maximum allocation is, and */
		/* then use 75% of that for cache, and free cache and retry if */
		/* malloc failes in rev.c. Too bad if 25% isn't enough, and a malloc fails */
		/* outside rev.c... */
		if (sizeof(avail_ram) < 8) {
			char *alocs[4 * 1024];
			size_t safe_max_vmem = 0;
			int i; 
	
#ifdef __APPLE__
			int old_stderr, new_stderr;

			/* OS X malloc() blabs about a malloc failure. This */
			/* will confuse users, so we temporarily redirect stdout */
			fflush(stderr);
			old_stderr = dup(fileno(stderr));
			new_stderr = open("/dev/null", O_WRONLY | O_APPEND);
			dup2(new_stderr, fileno(stderr));
#endif
			for (i = 0; (i < 4 * 1024);i++) {
				if ((alocs[i] = malloc(1024 * 1024)) == NULL) {
					break;
				}
				max_vmem = (i+1) * 1024 * 1024;
			}
			for (--i; i >= 0; i--) {
				free(alocs[i]);
			}
#ifdef __APPLE__
			fflush(stderr);
			dup2(old_stderr, fileno(stderr));	/* Restore stderr */
			close(new_stderr);
			close(old_stderr);
#endif
			/* To compute a true value, we need to allow for any VM already */
			/* used by any rev instances. */
			{
				rev_struct *rsi;

				for (rsi = g_rev_instances; rsi != NULL; rsi = rsi->next)
					max_vmem += rsi->sz;
			}
			
//fprintf(stdout,"~ Abs max VM = %d Mbytes\n",max_vmem/1000000);
			safe_max_vmem = (size_t)(0.85 * max_vmem);
			if (g_avail_ram > safe_max_vmem) {
				g_avail_ram = safe_max_vmem;
				if (s->verbose && repsr == 0)
					fprintf(stdout,"%cTrimmed maximum cache RAM to %lu Mbytes to allow for VM limit\n",cr_char,(unsigned long)g_avail_ram/1000000);
			}
		}
	
		/* Check for environment variable tweak  */
		if ((ev = getenv("ARGYLL_REV_CACHE_MULT")) != NULL) {
			double mm, gg;
			mm = atof(ev);
			if (mm < 0.01)			/* Make it sane */
				mm = 0.01;
			else if (mm > 100.0)
				mm = 100.0;
			gg = g_avail_ram * mm + 0.5;
			if (gg > (double)(((size_t)0)-1))
				gg  = (double)(((size_t)0)-1);
			g_avail_ram = (size_t)(gg);
		}
		if (max_vmem != 0 && g_avail_ram > max_vmem && repsr == 0) {
			g_avail_ram = (size_t)(0.95 * max_vmem);
			fprintf(stdout,"%cARGYLL_REV_CACHE_MULT * RAM trimmed to %lu Mbytes to allow for VM limit\n",cr_char,(unsigned long)g_avail_ram/1000000);
		}
	}

	/* Default - this will get aportioned as more instances appear */
	s->rev.max_sz = g_avail_ram;

	DBG(("reverse cache max memory = %d Mbytes\n",s->rev.max_sz/1000000));
	if (s->verbose && repsr == 0) {
		fprintf(stdout, "%cRev cache RAM = %lu Mbytes\n",cr_char,(unsigned long)g_avail_ram/1000000);
		repsr = 1;
	}

	/* Sub-simplex information for each sub dimension */
	for (e = 0; e <= di; e++) {
		if (s->rev.sspxi[e].spxi != NULL)	/* Assert */
			error("rspl rev, internal, init_ssimplex_info called on already init'd\n");

		rspl_init_ssimplex_info(s, &s->rev.sspxi[e], e);
	}

	make_rev_one(s);

	/* Reverse cell cache allocation */
	s->rev.cache = alloc_revcache(s);

	DBG(("make_rev finished\n"));
}

/* ====================================================== */

#if defined(DEBUG1) || defined(DEBUG2)

/* Utility - return a string containing a cells output value range */
static char *pcellorange(cell *c) {
	static char buf[5][300];
	static ix = 0;
	char *bp;
	rspl *s = c->s;
	int di = s->di, fdi = s->fdi;
	int ee, e, f;
	
	datao min, max;

//	double p[POW2MXRI][MXRI]; /* Vertex input positions for this cube. */
//	double v[POW2MXRI][MXRO+1]; /* Vertex data for this cube. Copied to x->v[] */
//							/* v[][fdi] is the ink limit values, if relevant */

	for (f = 0; f < fdi; f++) {
		min[f] = 1e60;
		max[f] = -1e60; 
	}

	/* For all other grid points in the cube */
	for (ee = 0; ee < (1 << di); ee++) {
		
		/* Update bounding box for this grid point */
		for (f = 0; f < fdi; f++) {
			if (min[f] > c->v[ee][f])	
				 min[f] = c->v[ee][f];
			if (max[f] < c->v[ee][f])
				 max[f] = c->v[ee][f];
		}
	}
	if (++ix >= 5)
		ix = 0;
	bp = buf[ix];

	for (e = 0; e < fdi; e++) {
		if (e > 0)
			*bp++ = ' ';
		sprintf(bp, "%f:%f", min[e],max[e]); bp += strlen(bp);
	}
	return buf[ix];
}

#endif
/* ====================================================== */

#undef DEBUG
#undef DBGV
#undef DBG
#define DBGV(xxx)
#define DBG(xxx) 






