
/* 
 * Argyll Color Correction System
 * Monotonic curve class for display calibration.
 *
 * Author: Graeme W. Gill
 * Date:   30/10/2005
 *
 * Copyright 2005 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * This is based on the monotonic curve equations used elsewhere,
 * but currently intended to support the display calibration process.
 * moncurve is not currently general, missing:
 *
 *  input scaling
 *  output scaling
 */

#undef DEBUG		/* Input points */
#undef DEBUG2		/* Detailed progress */

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

#define POWTOL 1e-5			/* Powell optimiser tollerance (was 1e-5 ?) */
#define MAXITS 10000

#undef TEST_PDE				/* Ckeck partial derivative calcs */

#define HW01		0.01	/* 0 & 1 harmonic weights */
#define HBREAK	    3		/* Harmonic that has HWBR */
#define HWBR        0.5		/* Base weight of harmonics HBREAK up */
#define HWINC       0.7		/* Increase in weight for each harmonic above HWBR */

static void mcv_del(mcv *p);
static void mcv_fit(mcv *p, int verb, int order, mcvco *d, int ndp, double smooth);
static void mcv_force_0(mcv *p, double target);
static void mcv_force_1(mcv *p, double target);
static void mcv_force_scale(mcv *p, double target);
static int mcv_get_params(mcv *p, double **rp);
static double mcv_interp(struct _mcv *p, double in);
static double mcv_inv_interp(struct _mcv *p, double in);

static double mcv_interp_p(struct _mcv *p, double *pms, double in);
static double mcv_shweight_p(mcv *p, double *v, double smooth);
double mcv_dinterp_p(mcv *p, double *pms, double *dv, double vv);
static double mcv_dshweight_p(mcv *p, double *v, double *dv, double smooth);

/* Create a new, uninitialised mcv that will fit with offset and scale */
/* (Note thate black and white points aren't allocated) */
mcv *new_mcv(void) {
	mcv *p;

	if ((p = (mcv *)calloc(1, sizeof(mcv))) == NULL)
		return NULL;

	/* Init method pointers */
	p->del         = mcv_del;
	p->fit         = mcv_fit;
	p->force_0     = mcv_force_0;
	p->force_1     = mcv_force_1;
	p->force_scale = mcv_force_scale;
	p->get_params  = mcv_get_params;
	p->interp      = mcv_interp;
	p->inv_interp  = mcv_inv_interp;
	p->interp_p    = mcv_interp_p;
	p->shweight_p  = mcv_shweight_p;
	p->dinterp_p   = mcv_dinterp_p;
	p->dshweight_p = mcv_dshweight_p;

	p->luord = 0;
	p->pms = NULL;
	return p;
}

/* Create a new, uninitialised mcv without offset and scale parameters. */
/* Note thate black and white points aren't allocated */
mcv *new_mcv_noos(void) {
	mcv *p;

	if ((p = new_mcv()) == NULL)
		return p;

	p->noos = 2;
	return p;
}

/* Create a new mcv initiated with the given curve parameters */
/* (Assuming parameters always includes offset and scale) */
mcv *new_mcv_p(double *pp, int np) {
	int i;
	mcv *p;

	if ((p = new_mcv()) == NULL)
		return p;

	p->luord = np;
	if ((p->pms = (double *)calloc(p->luord, sizeof(double))) == NULL)
		error ("Malloc failed");

	for (i = 0; i < np; i++)
		p->pms[i] = *pp++;

	return p;
}

/* Delete an mcv */
static void mcv_del(mcv *p) {
	if (p->pms != NULL)
		free(p->pms);
	free(p);
}

#ifdef TEST_PDE
#define mcv_opt_func mcv_opt_func_
#endif

/* Shaper+Matrix optimisation function handed to powell() */
static double mcv_opt_func(void *edata, double *v) {
	mcv *p = (mcv *)edata;
	double totw = 0.0;
	double ev = 0.0, rv, smv;
	double out;
	int i;

#ifdef DEBUG2
	printf("params =");
	for (i = 0; i < p->luord-p->noos; i++)
		printf(" %f",v[i]);
	printf("\n");
	printf("ndp = %d\n",p->ndp);
#endif

	/* For all our data points */
	for (i = 0; i < p->ndp; i++) {
		double del;

		/* Apply our function */
		out = p->interp_p(p, v, p->d[i].p);

		del = out - p->d[i].v;

		ev += p->d[i].w * del * del;
		totw += p->d[i].w;
	}

	/* Normalise error to be an average delta E squared */
	totw = (100.0 * 100.0)/(p->dra * p->dra * totw);
	ev *= totw;

	/* Sum with shaper parameters squared, to */
	/* minimise unsconstrained "wiggles" */
	smv = mcv_shweight_p(p, v, p->smooth);
	rv = ev + smv;

#ifdef DEBUG2
	printf("rv = %f (er %f + sm %f)\n",rv,ev,smv);
#endif
	return rv;
}

/* Shaper+Matrix optimisation function handed to conjgrad() */
static double mcv_dopt_func(void *edata, double *dv, double *v) {
	mcv *p = (mcv *)edata;
	double totw = 0.0;
	double ev = 0.0, rv, smv;
	double out;
	int i, j;

#ifdef DEBUG2
	printf("params =");
	for (i = 0; i < (p->luord-p->noos); i++)
		printf(" %f",v[i]);
	printf("\n");
#endif

	/* Zero the dv's */
	for (j = 0; j < (p->luord-p->noos); j++)
		dv[j] = 0.0;

	/* For all our data points */
	for (i = 0; i < p->ndp; i++) {
		double del;

		/* Apply our function with dv's */
		out = p->dinterp_p(p, v, p->dv, p->d[i].p);
//printf("~1 point %d: p %f, v %f, func %f\n",i,p->d[i].p,p->d[i].v,out); 

		del = out - p->d[i].v;
		ev += p->d[i].w * del * del;
//printf("~1 del %f, ev %f\n",del,ev);

		/* Sum the dv's */
		for (j = 0; j < (p->luord-p->noos); j++) {
			dv[j] += p->d[i].w * 2.0 * del * p->dv[j];
//printf("~1 dv[%d] = %f\n",j,dv[j]);
		}

		totw += p->d[i].w;
	}

//printf("~1 totw = %f, dra = %f\n",totw, p->dra);
	/* Normalise error to be an average delta E squared */
	totw = (100.0 * 100.0)/(p->dra * p->dra * totw);
	ev *= totw;
	for (j = 0; j < (p->luord-p->noos); j++) {
		dv[j] *= totw; 
//printf("~1 norm dv[%d] = %f\n",j,dv[j]);
	}

	/* Sum with shaper parameters squared, to */
	/* minimise unsconstrained "wiggles", */
	/* with partial derivatives */
	smv = mcv_dshweight_p(p, v, dv, p->smooth);
	rv = ev + smv;

#ifdef DEBUG2
	printf("drv = %f (er %f + sm %f)\n",rv,ev,smv);
#endif
	return rv;
}

#ifdef TEST_PDE
/* Check partial derivative function */

#undef mcv_opt_func

static double mcv_opt_func(void *edata, double *v) {
	mcv *p = (mcv *)edata;
	int i;
	double dv[500];
	double rv, drv;
	double trv;
	
	rv = mcv_opt_func_(edata, v);
	drv = mcv_dopt_func(edata, dv, v);

	if (fabs(rv - drv) > 1e-6)
		printf("######## RV MISMATCH is %f should be %f ########\n",rv,drv);

	/* Check each parameter delta */
	for (i = 0; i < (p->luord-p->noos); i++) {
		double del;

		v[i] += 1e-7;
		trv = mcv_opt_func_(edata, v);
		v[i] -= 1e-7;
		
		/* Check that del is correct */
		del = (trv - rv)/1e-7;
		if (fabs(dv[i] - del) > 0.04) {
//printf("~1 del = %f from (trv %f - rv %f)/0.1\n",del,trv,rv);
			printf("######## EXCESSIVE at v[%d] is %f should be %f ########\n",i,dv[i],del);
		}
	}
	return rv;
}
#endif	/* TEST_PDE */

/* Fit the curve to the given points */
static void mcv_fit(mcv *p,
	int verb,		/* Vebosity level, 0 = none */
	int order,		/* Number of curve orders, 1..MCV_MAXORDER */
	mcvco *d,		/* Array holding scattered initialisation data */
	int ndp,		/* Number of data points */
	double smooth	/* Degree of smoothing, 1.0 = normal */			
) {
	int i;
	double *sa;		/* Search area */
	double *pms;	/* Parameters to optimise */
	double min, max;

	p->verb = verb;
	p->smooth = smooth;
	p->luord = order+2;		/* Add two for offset and scale */

	if (p->pms != NULL)
		free(p->pms);
	if ((p->pms = (double *)calloc(p->luord, sizeof(double))) == NULL)
		error ("Malloc failed");
	if ((pms = (double *)calloc(p->luord, sizeof(double))) == NULL)
		error ("Malloc failed");
	if ((sa = (double *)calloc(p->luord, sizeof(double))) == NULL)
		error ("Malloc failed");
	if ((p->dv = (double *)calloc(p->luord, sizeof(double))) == NULL)
		error ("Malloc failed");

#ifdef DEBUG
	printf("mcv_fit with %d points (noos = %d)\n",ndp,p->noos);
#endif
	/* Establish the range of data values */
	min = 1e38;			/* Locate min, and make that offset */
	max = -1e38;			/* Locate max */
	for (i = 0; i < ndp; i++) {
		if (d[i].v < min)
			min = d[i].v;
		if (d[i].v > max)
			max = d[i].v;
#ifdef DEBUG
		printf("point %d is %f %f\n",i,d[i].p,d[i].v);
#endif
	}

	if (p->noos) {
		p->pms[0] = min = 0.0;
		p->pms[1] = max = 1.0;
	} else {
		/* Set offset and scale to reasonable values */
		p->pms[0] = min;
		p->pms[1] = max - min;
	}
	p->dra = max - min;
	if (p->dra <= 1e-12)
		error("Mcv max - min %e too small",p->dra);

	/* Use powell to minimise the sum of the squares of the */
	/* input points to the curvem, plus a parameter damping factor. */
	p->d = d;
	p->ndp = ndp;

	for (i = 0; i < p->luord; i++)
		sa[i] = 0.2;

#ifdef NEVER
	if (powell(&p->resid, p->luord-p->noos, p->pms+p->noos, sa+p->noos, POWTOL, MAXITS,
	                                          mcv_opt_func, (void *)p, NULL, NULL) != 0)
		error ("Mcv fit powell failed");
#else
	if (conjgrad(&p->resid, p->luord-p->noos, p->pms+p->noos, sa+p->noos, POWTOL, MAXITS,
	                              mcv_opt_func, mcv_dopt_func, (void *)p, NULL, NULL) != 0) {
#ifndef NEVER
	fprintf(stderr,"Mcv fit conjgrad failed with %d points:\n",ndp);
	for (i = 0; i < ndp; i++) {
		fprintf(stderr,"  %d: %f -> %f\n",i,d->p, d->v);
	}
#endif
		error ("Mcv fit conjgrad failed");
	}
#endif

	free(p->dv);
	p->dv = NULL;
	free(sa);
	free(pms);
}

/* The native values from the curve parameters are 0 - 1.0, */
/* then the scale is applied, then the offset added, so the */
/* output always ranges from (offset) to (offset + scale). */

/* Offset the the output so that the value for input 0.0, */
/* is the given value. Don't change the output for 1.0 */
void mcv_force_0(
	mcv *p,
	double target	/* Target output value */
) {
	if (p->luord > 0) {
		target -= p->pms[0];	/* Change */
		if (p->luord > 1)
			p->pms[1] -= target;	/* Adjust scale to leave 1.0 output untouched */
		p->pms[0] += target;	/* Adjust offset */
	}
}

/* Scale the the output so that the value for input 1.0, */
/* is the given target value. Don't change the output for 0.0 */
static void mcv_force_1(
	mcv *p,
	double target	/* Target output value */
) {
	if (p->luord > 1) {
		target -= p->pms[0];	/* Offset */
		p->pms[1] = target;	/* Scale */
	}
}

/* Scale the the output so that the value for input 1.0, */
/* is the given target value. Scale the value for 0 in proportion. */
static void mcv_force_scale(
	mcv *p,
	double target	/* Target output value */
) {
	if (p->luord > 1) {
		p->pms[0] *= target/(p->pms[0] + p->pms[1]);	/* Offset */
		p->pms[1] = target - p->pms[0];	/* Scale */
	}
}

/* Return the number of parameters and the parameters in */
/* an allocated array. free() when done. */
/* The parameters are the offset, scale, then all the other parameters */
static int mcv_get_params(mcv *p, double **rp) {
	double *pp;
	int np, i;

	np = p->luord;

	if ((pp = (double *)malloc(np * sizeof(double))) == NULL)
		error("mcb_get_params malloc failed");
	
	*rp = pp;
	
	for (i = 0; i < np; i++)
		*pp++ = p->pms[i];
		
	return np;
}

/* Translate a value through the curve */
/* using the currently set pms */
static double mcv_interp(struct _mcv *p,
	double vv	/* Input value */
) {
	return mcv_interp_p(p, p->pms + p->noos, vv);
}

/* Translate a value through backwards the curve */
static double mcv_inv_interp(struct _mcv *p,
	double vv	/* Input value */
) {
	double g;
	int ord;

	/* Process everything in reverse order to mcv_interp */

	if (p->noos == 0) {
		/* Do order 0 & 1, the offset and scale */
		if (p->luord > 0)
			vv -= p->pms[0];
	
		if (p->luord > 1)
			vv /= p->pms[1];
	}

	for (ord = p->luord-1; ord > 1; ord--) {
		int nsec;			/* Number of sections */
		double sec;			/* Section */

		g = -p->pms[ord];	/* Inverse parameter */

		nsec = ord-1;		/* Increase sections for each order */

		vv *= (double)nsec;

		sec = floor(vv);
		if (((int)sec) & 1)
			g = -g;			/* Alternate action in each section */
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

/* Translate a value through the curve */
/* using the given parameters */
static double mcv_interp_p(
	mcv *p,
	double *pms,	/* Parameters to use - may exclude offset and scale */
	double vv		/* Input value */
) {
	double g;
	int ord;

	/* Process all the shaper orders from low to high. */
	/* [These shapers were inspired by a Graphics Gem idea */
	/* (Gems IV, VI.3, "Fast Alternatives to Perlin's Bias and */
	/*  Gain Functions, pp 401). */
	/*  They have the nice properties that they are smooth, and */
	/*  are monotonic. The control parameter has been */
	/*  altered to have a range from -oo to +oo rather than 0.0 to 1.0 */
	/*  so that the search space is less non-linear. */
	for (ord = (2 - p->noos); ord < (p->luord - p->noos); ord++) {
		int nsec;			/* Number of sections */
		double sec;			/* Section */

		g = pms[ord];	/* Parameter */

		nsec = ord-1+p->noos;	/* Increase sections for each order */

		vv *= (double)nsec;

		sec = floor(vv);
		if (((int)sec) & 1)
			g = -g;			/* Alternate action in each section */
		vv -= sec;
		if (g >= 0.0) {
			vv = vv/(g - g * vv + 1.0);
		} else {
			vv = (vv - g * vv)/(1.0 - g * vv);
		}
		vv += sec;
		vv /= (double)nsec;
	}

	if (p->noos == 0) {
		/* Do order 0 & 1 */
		if (p->luord > 1)
			vv *= pms[1];	/* Scale */
	
		if (p->luord > 0)
			vv += pms[0];	/* Offset */
	}

	return vv;
}

/* Return the shaper parameters regularizing weight */
static double mcv_shweight_p(
mcv *p,
double *pms,	/* Parameters to use - may exclude offset and scale */
double smooth) {

	double smv;
	int i;

	/* Sum with shaper parameters squared, to */
	/* minimise unsconstrained "wiggles" */
	/* Note:- we start at 2, to skip offset and scale. */
	/* ?? Should these have a weight too ?? */
	smv = 0.0;
	for (i = (2-p->noos); i < (p->luord-p->noos); i++) {
		double w, tt;
		int cx;				/* Curve index (skips offset & scale) */

		cx = i - 2 + p->noos; 
		tt = pms[i];

		/* Weigh to suppress ripples */
		if (cx <= 1) {
			w = HW01;
		} else if (cx <= HBREAK) {
			double bl = (cx - 1.0)/(HBREAK - 1.0);
			w = (1.0 - bl) * HW01 + bl * HWBR * smooth;
		} else {
			w = HWBR + (cx-HBREAK) * HWINC * smooth;
		}
		tt *= tt;
		smv += w * tt;
	}
	return smv;
}

/* Transfer function with partial derivative */
/* with respect to the given parameters. */
double mcv_dinterp_p(mcv *p,
double *pms,		/* Parameters to use - may exclude offset and scale */
double *dv,			/* Return derivative wrt each parameter - may exclude offset and scale */
double vv			/* Source of value */
) {
	double g;
	int i, ord;

	/* Process all the shaper orders from low to high. */
	for (ord = (2-p->noos); ord < (p->luord-p->noos); ord++) {
		double dsv;		/* del for del in g */
		double ddv;		/* del for del in vv */
		int nsec;		/* Number of sections */
		double sec;		/* Section */

		g = pms[ord];			/* Parameter */

		nsec = ord-1+p->noos;	/* Increase sections for each order */

		vv *= (double)nsec;

		sec = floor(vv);
		if (((int)sec) & 1) {
			g = -g;				/* Alternate action in each section */
		}
		vv -= sec;
		if (g >= 0.0) {
			double tt = g - g * vv + 1.0;
			dsv = (vv * vv - vv)/(tt * tt);
			ddv = (g + 1.0)/(tt * tt);
			vv = vv/tt;
		} else {
			double tt = 1.0 - g * vv;
			dsv = (vv * vv - vv)/(tt * tt);
			ddv = (1.0 - g)/(tt * tt);
			vv = (vv - g * vv)/tt;
		}

		vv += sec;
		vv /= (double)nsec;
		dsv /= (double)nsec;
		if (((int)sec) & 1)
			dsv = -dsv;

		dv[ord] = dsv;
		for (i = ord - 1; i >= (2-p->noos); i--)
			dv[i] *= ddv;
	}

	if (p->noos == 0) {
		/* Do order 0, the scale */
		if (p->luord > 1) {
			dv[1] = vv;
			vv *= pms[1];
		}
		if (p->luord > 0) {
			dv[0] = 1.0;
			vv += pms[0];	/* Offset */
		}
	}

	return vv;
}

/* Return the shaper parameters regularizing weight, */
/* and add in partial derivatives. */
/* Weight error and derivatrive by wht */
static double mcv_dshweight_p(
mcv *p,
double *pms,	/* Parameters to use - may exclude offset and scale */
double *dpms,
double smooth) {
	double smv;
	int i;

	/* Sum with shaper parameters squared, to */
	/* minimise unsconstrained "wiggles", */
	/* with partial derivatives */
	smv = 0.0;
	for (i = (2-p->noos); i < (p->luord-p->noos); i++) {
		double w, tt;
		int cx;

		cx = i - 2 + p->noos; 
		tt = pms[i];

		/* Weigh to suppress ripples */
		if (cx <= 1) {			/* First or second curves */
			w = HW01;
		} else if (cx <= HBREAK) {	/* First or second curves */
			double bl = (cx - 1.0)/(HBREAK - 1.0);
			w = (1.0 - bl) * HW01 + bl * HWBR * smooth;
		} else {
			w = HWBR + (cx-HBREAK) * HWINC * smooth;
		}
		dpms[i] += w * 2.0 * tt;
		tt *= tt;
		smv += w * tt;
	}

	return smv;
}


