
 /* Single dimension regularized spline data structure */

/* 
 * Argyll Color Correction System
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "numsup.h"
#include "rspl1.h"

#undef DEBUG

#ifdef DEBUG
#ifdef DBG
#undef DBG
#undef dbgo
#endif
#define dbgo stderr
#define DBG(aaa) fprintf aaa
#else
#define DBG(aaa) 
#endif

/* Do an interpolation based on the grid */
/* Use a linear interp between grid points. */
/* If the input is outside the grid range, it will */
/* be clamped to the nearest grid point. */
static int interp(
rspl *t,
co *p
) {
	int rv = 0;
	double x, y, xx, w1;
	int i;

	x = p->p[0];

	if (x < t->gl) {
		x = t->gl;
		rv = 1;
	} else if (x > t->gh) {
		x = t->gh;
		rv = 1;
	}

	xx = (x - t->gl)/t->gw;			/* Grid location of point */
	i = (int)floor(xx);				/* Lower grid of point */
	if (i >= (t->nig-2))
		i = t->nig-2;

	w1 = xx - (double)i;			/* Weight to upper grid point */

	y = ((1.0 - w1) * t->x[i]) + (w1 * t->x[i+1]);

	p->v[0] = y * t->vw + t->vl;	/* Rescale the data */

	return rv;
}

/* Destructor */
static void del_rspl(rspl *t) {
	if (t != NULL) {
		if (t->x != NULL)
			free_dvector(t->x, 0, t->nig);
	free(t);
	}
}

/* Initialise the regular spline from scattered data */
/* Return nz on error */
static int fit_rspl_imp(
	struct _rspl *t,/* this */
	int flags,		/* (Not used) */
	void *d,		/* Array holding position and function values of data points */
	int dtp,		/* Flag indicating data type, 0 = (co *), 1 = (cow *), 2 = (coww *) */
	int ndp,		/* Number of data points */
	datai glow,		/* Grid low scale - will expand to enclose data, NULL = default 0.0 */
	datai ghigh,	/* Grid high scale - will expand to enclose data, NULL = default 1.0 */
	int    *gres,	/* Spline grid resolution, ncells = gres-1 */
	datao vlow,		/* Data value low normalize, NULL = default 0.0 */
	datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
	double smooth,	/* Smoothing factor, 0.0 = default 1.0 */
	double *avgdev, /* (Not used) */
	double **ipos 	/* (not used) */
) {
	int n;
	double cw;

	DBG((dbgo, "rspl1:fit_rspl_imp() with %d points called, dtp = %d\n",ndp,dtp));

	/* Allocate space for interpolation grid */
	t->nig = *gres;

	if ((t->x   = dvector(0, t->nig)) == NULL) {
		DBG((dbgo, "rspl1:Malloc of vector x failed\n"));
		return 1;
	}

	/* Normalize curve weight to grid resolution. */
	cw = 0.0000005 * smooth * pow((t->nig-1),4.0) / (t->nig - 2);
	DBG((dbgo, "rspl1:cw = %e\n",cw));

	/* cw is multiplied by the sum of grid curvature errors squared to keep */
	/* the same ratio with the sum of data position errors squared */

	/* Determine the data range */
	t->xl = 1e300;
	t->xh = -1e300;
	t->dl = 1e300;
	t->dh = -1e300;
	if (dtp == 0) {
		co *dd = (co *)d;

		for (n = 0; n < ndp; n++) {
			if (dd[n].p[0] < t->xl)
				t->xl = dd[n].p[0];
			if (dd[n].p[0] > t->xh)
				t->xh = dd[n].p[0];
			if (dd[n].v[0] < t->dl)
				t->dl = dd[n].v[0];
			if (dd[n].v[0] > t->dh)
				t->dh = dd[n].v[0];

			DBG((dbgo, "rspl1:Point %d = %f, %f\n",n,dd[n].p[0],dd[n].v[0]));
		}
	} else if (dtp == 1) {
		cow *dd = (cow *)d;

		for (n = 0; n < ndp; n++) {
			if (dd[n].p[0] < t->xl)
				t->xl = dd[n].p[0];
			if (dd[n].p[0] > t->xh)
				t->xh = dd[n].p[0];
			if (dd[n].v[0] < t->dl)
				t->dl = dd[n].v[0];
			if (dd[n].v[0] > t->dh)
				t->dh = dd[n].v[0];
			DBG((dbgo, "rspl1:Point %d = %f, %f (%f)\n",n,dd[n].p[0],dd[n].v[0],dd[n].w));
		}
	} else {
		DBG((dbgo, "rspl1:Internal error, unknown dtp value %d\n",dtp));
		return 1;
	}

	t->gl = glow != NULL ? *glow : 0.0;
	t->gh = ghigh != NULL ? *ghigh : 1.0;

	/* adjust input ranges to encompass data */ 
	if (t->xl < t->gl)
		t->gl = t->xl;
	if (t->xh > t->gh)
		t->gh = t->xh;

	/* Set the input and output scaling */
	t->gw  = (t->gh - t->gl)/(double)(t->nig-1);

	t->vl  = vlow != NULL ? *vlow : 0.0;
	t->vw  = ((vhigh != NULL ? *vhigh : 1.0) - t->vl);

	DBG((dbgo, "rspl1:gl %f, gh %f, gw %f, vl %f, vw %f\n",t->gl,t->gh,t->gw,t->vl,t->vw));

	/* create smoothed grid data */
	{
		int n,i,k;
		double **A;		/* A matrix of interpoint weights */
		double *b;		/* b vector for RHS of simultabeous equation */

		/* We just store the diagonal of the A matrix */
		if ((A = dmatrix(0, t->nig, 0, 2)) == NULL) {
			DBG((dbgo, "rspl1:Malloc of matrix A failed\n"));
			return 1;
		}

		if ((b = dvector(0,t->nig)) == NULL) {
			free_dvector(b,0,t->nig);
			DBG((dbgo, "rspl1:Malloc of vector b failed\n"));
			return 1;
		}

		/* Initialize the A and b matricies */
		for (i = 0; i < t->nig; i++) {
			for (k = 0; k < 3; k++) 
				A[i][k] = 0.0;
			t->x[i] = b[i] = 0.0;
		}
	
		/* Accumulate data dependent factors */
		for (n = 0; n < ndp; n++) {
			double bf, cbf;
			double xv, yv, wv;

			if (dtp == 0) {
				co *dd = (co *)d;

				xv = dd[n].p[0];
				yv = dd[n].v[0];
				wv = 1.0;
			} else if (dtp == 1) {
				cow *dd = (cow *)d;

				xv = dd[n].p[0];
				yv = dd[n].v[0];
				wv = dd[n].w;
			} else {
				DBG((dbgo, "rspl1:Internal error, unknown dtp value %d\n",dtp));
				return 1;
			}
			yv = (yv - t->vl)/t->vw;	/* Normalize the value */

			/* Figure out which grid cell data is in */
			i = (int)((xv - t->gl)/t->gw);	/* Index of next lowest data point */

			bf = ((((double)(i+1) * t->gw) + t->gl) - xv)/t->gw; /* weight to lower grid point */
			cbf = 1.0 - bf;						/* weight to upper grid point */

			b[i]    -= 2.0 * bf * -yv * wv;			/* dui component due to dn */
			A[i][0] += 2.0 * bf * bf * wv;			/* dui component due to ui */
			A[i][1] += 2.0 * bf * cbf * wv;			/* dui component due to ui+1 */

			if ((i+1) < t->nig) {
				b[i+1]     -= 2.0 * cbf * -yv * wv;	/* dui component due to dn */
				A[i+1][0]  += 2.0 * cbf * cbf * wv;	/* dui component due to ui */
			}
		}
	
		/* Accumulate curvature dependent factors */
		for (i = 0; i < t->nig; i++) {

			if ((i-2) >= 0) {					/* Curvature of cell below */
				A[i][0] +=  2.0 * cw;
			}

			if ((i-1) >= 0 && (i+1) < t->nig) {	/* Curvature of t cell */
				A[i][0] +=  8.0 * cw;
				A[i][1] += -4.0 * cw;
			}
			if ((i+2) < t->nig) {					/* Curvature of cell above */
				A[i][0] +=  2.0 * cw;
				A[i][1] += -4.0 * cw;
				A[i][2] +=  2.0 * cw;
			}
		}

#ifdef DEBUG
		fprintf(dbgo, "A matrix:\n");
		for (i = 0; i < t->nig; i++) {
			for (k = 0; k < 3; k++) {
				fprintf(dbgo, "A[%d][%d] = %f\n",i,k,A[i][k]);
			}
		}
		fprintf(dbgo, "b vector:\n");
		for (i = 0; i < t->nig; i++) {
			fprintf(dbgo, "b[%d] = %f\n",i,b[i]);
		}
#endif /* DEBUG */

		/* Apply Cholesky decomposition to A[][] to create L[][] */
		for (i = 0; i < t->nig; i++) {
			double sm;
			for (n = 0; n < 3; n++) {
				sm = A[i][n];
				for (k = 1; (n+k) < 3 && (i-k) >=0; k++) {
					sm -= A[i-k][n+k] * A[i-k][k];
				}
				if (n == 0) {
					if (sm <= 0.0) {
						free_dvector(b,0,t->nig);
						free_dmatrix(A,0,t->nig,0,2);
						DBG((dbgo, "rspl1:Sum is -ve - loss of accuracy ?\n"));
						return 1;
					}
					A[i][0] = sqrt(sm);
				} else {
					A[i][n] = sm/A[i][0];
				}
			}
		}

		/* Solve L . y = b, storing y in x */
		for (i = 0; i < t->nig; i++) {
			double sm;
			sm = b[i];
			for (k = 1; k < 3 && (i-k) >= 0; k++) {
				sm -= A[i-k][k] * t->x[i-k];
			}
			t->x[i] = sm/A[i][0];
		}

		/* Solve LT . x = y */
		for (i = t->nig-1; i >= 0; i--) {
			double sm;
			sm = t->x[i];
			for (k = 1; k < 3 && (i+k) < t->nig; k++) {
				sm -= A[i][k] * t->x[i+k];
			}
			t->x[i] = sm/A[i][0];
		}
#ifdef DEBUG
		fprintf(dbgo, "Solution vector:\n");
		for (i = 0; i < t->nig; i++) {
			fprintf(dbgo, "x[%d] = %f\n",i,t->x[i]);
		}
#endif /* DEBUG */

		free_dvector(b,0,t->nig);
		free_dmatrix(A,0,t->nig,0,2);
	}
	return 0;
}

/* Initialise from scattered data. */
/* Return nz on error */
static int fit_rspl(
	struct _rspl *t,/* this */
	int flags,		/* (Not used) */
	co *d,			/* Array holding position and function values of data points */
	int ndp,		/* Number of data points */
	datai glow,		/* Grid low scale - will expand to enclose data, NULL = default 0.0 */
	datai ghigh,	/* Grid high scale - will expand to enclose data, NULL = default 1.0 */
	int *gres,		/* Spline grid resolution, ncells = gres-1 */
	datao vlow,		/* Data value low normalize, NULL = default 0.0 */
	datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
	double smooth,	/* Smoothing factor, 0.0 = default 1.0 */
	double *avgdev, /* (Not used) */
	double **ipos 	/* (not used) */
) {
	/* Call implementation with (co *) data */
	return fit_rspl_imp(t, flags, (void *)d, 0, ndp, glow, ghigh, gres, vlow, vhigh,
	                    smooth, avgdev, ipos);
}

/* Initialise the regular spline from scattered data with weights */
/* Return nz on error */
static int
fit_rspl_w(
	rspl *t,		/* this */
	int flags,		/* Combination of flags */
	cow *d,			/* Array holding position, function and weight values of data points */
	int dno,		/* Number of data points */
	ratai glow,		/* Grid low scale - will be expanded to enclose data, NULL = default 0.0 */
	ratai ghigh,	/* Grid high scale - will be expanded to enclose data, NULL = default 1.0 */
	int *gres,		/* Spline grid resolution */
	ratao vlow,		/* Data value low normalize, NULL = default 0.0 */
	ratao vhigh,	/* Data value high normalize - NULL = default 1.0 */
	double smooth,	/* Smoothing factor, 0.0 = default 1.0 */
	double *avgdev, /* (Not used) */
	double **ipos 	/* (not used) */
) {
	/* Call implementation with (cow *) data */
	return fit_rspl_imp(t, flags, (void *)d, 1, dno, glow, ghigh, gres, vlow, vhigh,
	                    smooth, avgdev, ipos);
}

/* Construct an empty rspl1 */
/* Return NULL if something goes wrong. */
rspl *new_rspl(int flags, int di, int fdi) {
	rspl *t;	/* this */

	if (flags != RSPL_NOFLAGS || di != 1 || fdi != 1) {
		DBG((dbgo, "rspl1:Can't handle general rspl: flags %d, di %d, do %d\n",flags,di,fdi));
		return NULL;
	}

	if ((t = (rspl *)calloc(1, sizeof(rspl))) == NULL) {
		DBG((dbgo, "rspl1:Malloc of structure failed\n"));
		return NULL;
	}

	/* Initialise the classes methods */
	t->interp     = interp;
	t->fit_rspl   = fit_rspl; 
	t->fit_rspl_w = fit_rspl_w; 
	t->del        = del_rspl;

	return t;
}





