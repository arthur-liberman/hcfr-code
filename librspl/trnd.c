
/************************************************/
/* Test RSPL reverse lookup in 3/4D */
/************************************************/

/* Author: Graeme Gill
 * Date:   31/10/96
 * Derived from tnd.c
 * Copyright 1999 - 2000 Graeme W. Gill
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */


#undef DEBUG
#undef DETAILED

#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include "rspl.h"
#include "numlib.h"

#ifdef NEVER
FILE *verbose_out = stdout;
int verbose_level = 6;			/* Current verbosity level */
								/* 0 = none */
								/* !0 = diagnostics */
#endif /* NEVER */

#define GRES 10			/* Grid resolution */
#define DI 4			/* Dimensions in */
#define FDI 3			/* Function (out) Dimensions */
#define DOLIMIT
#define LIMITV 1.50

/* Fwd function approximated by rspl */
void func(
void *cbctx,
double *out,
double *in) {
	double tt[4];

#ifdef NEVER
printf(" Got input %f %f\n",in[0],in[1]);
	if (in[1] < 0.5 && in[0] < 0.5) { /* 0,0 */
		out[0] =  0.1;
		out[1] =  0.5;
	}
	if (in[1] < 0.5 && in[0] > 0.5) { /* 0,1 */
		out[0] =  0.5;
		out[1] =  0.0;
	}
	if (in[1] > 0.5 && in[0] < 0.5) { /* 0,1 */
		out[0] =  0.9;
		out[1] =  0.8;
	}
	if (in[1] > 0.5 && in[0] > 0.5) { /* 0,1 */
		out[0] =  0.9;
		out[1] =  0.2;
	}
#endif	/* NEVER */

#if DI >= 3
	tt[0] = 0.7 * in[0] + 0.2 * in[1] + 0.1 * in[2];
	tt[0] = tt[0] * tt[0];
	tt[1] = 0.2 * in[0] + 0.8 * in[1] - 0.1 * in[2];
	tt[1] = tt[1] * tt[1];
	tt[2] = 0.3 * in[0] - 0.2 * in[1] + 0.9 * in[2];
	tt[2] = tt[2] * tt[2];
#endif

#if DI == 4
	tt[0] *= in[3];
	tt[1] *= in[3];
	tt[2] *= in[3];
#endif

	tt[3] = 0.3 * in[0] + 0.4 * in[1] + 0.3 * in[2];

#if FDI > 0
	out[0] = tt[0];
#endif
#if FDI > 1
	out[1] = tt[1];
#endif
#if FDI > 2
	out[2] = tt[2];
#endif
#if FDI > 3
	out[3] = tt[3];
#endif
}

/* Simplex ink limit function */
double limitf(
void *lcntx,
double *in
) {
	int i;
	double ov;

	for (ov = 0.0, i = 0; i < DI; i++) {
		ov += in[i];
	} 
	return ov;
}

int
main(
int argc,
char *argv[]
) {
	int e;
	rspl *rss;	/* Multi-resolution regularized spline structure */
	int gres[MXDI];

	for (e = 0; e < DI; e++)
		gres[e] = GRES;

	printf("Started test\n");
	/* Create the object */
	rss =  new_rspl(RSPL_NOFLAGS, DI, FDI);

	printf("Rspl allocated\n");
	rss->set_rspl(rss, 0, (void *)NULL, func, NULL, NULL, gres, NULL, NULL);
//	rss->set_rspl(rss, RSPL_SET_APXLS, (void *)NULL, func, NULL, NULL, gres, NULL, NULL);

	printf("Rspl set\n");

	{
#define NIP 10			/* Number of solutions allowed */
	int i, r, cl;
	double v[MXDO];		/* Target output value */
	co tp[NIP], chp;	/* Test point, check point */
	double cvec[4];		/* Text clip vector */
	int auxm[4];		/* Auxiliary target value valid flag */
	double lmin[4], lmax[4];	/* Locus min/max values */

	/* Output value being looked for */
/*
	tp[0].v[0] = v[0] = 1.5;
	tp[0].v[1] = v[1] = 0.9;
	tp[0].v[2] = v[2] = 1.2;
	tp[0].v[3] = v[3] = 0.0;
*/
	tp[0].v[0] = v[0] = 0.3;
	tp[0].v[1] = v[1] = 0.4;
	tp[0].v[2] = v[2] = 0.26;
	tp[0].v[3] = v[3] = 0.0;

	/* Set auxiliary target */
	auxm[0] = 0;
	auxm[1] = 0;
	auxm[2] = 0;
	auxm[3] = 1;
	tp[0].p[0] = 0;
	tp[0].p[1] = 0;
	tp[0].p[2] = 0.0;
	tp[0].p[3] = 0.87;

	for (i = 1; i < NIP; i++) {	/* Make sure we can see changes */
		tp[i].p[0] = -1.0;
		tp[i].p[1] = -1.0;
		tp[i].p[2] = -1.0;
		tp[i].p[3] = -1.0;
	}

	/* Clip center */
	cvec[0] = 0.1 - tp[0].v[0];
	cvec[1] = 0.1 - tp[0].v[1];
	cvec[2] = 0.1 - tp[0].v[2];
	cvec[3] = 0.1 - tp[0].v[3];

#ifdef DOLIMIT
	/* Setup limit */
	rss->rev_set_limit(rss,
		limitf,		/* limit function */
		NULL,		/* Function context */
		LIMITV		/* limit maximum value */
	);
#endif /* DOLIMIT */
	
#if DI > FDI
	/* Check out the locus size */
	if ((r = rss->rev_locus(rss,
		auxm, 	/* auxm Auxiliary mask flags */
		tp,		/* Input and auxiliary values */
		lmin,	/* Locus min/max return values */
		lmax
		)) > 0) {
	
		printf("Total of %d Results\n",r);
		for (i = 0; i < FDI; i++) {
			if (auxm[i] == 0)
				continue;
			printf("Auxiliary %d min = %f, max = %f\n",i, lmin[i], lmax[i]);
		}
	} else {
		printf("Failed to find gamut range\n");
	}
	printf("\n\n");
#endif

	/* Do reverse interpolation ~~~1 */
	if ((r = rss->rev_interp(rss,
		0 	/* RSPL_EXACTAUX */,		/* Hint flags */
		NIP,	/* Number of solutions allowed */
		auxm, 	/* auxm Auxiliary mask flags */
		cvec, 	/* cvec Clip vector direction & length */
		tp)		/* Input and output values */
		) > 0) {
	
		printf("Total of %d Results\n", r &= RSPL_NOSOLNS);

		printf("Target output   %f, %f, %f, %f\n", v[0], v[1], v[2], v[3]);

		cl = r & RSPL_DIDCLIP;	/* clipped flag */
		r &= RSPL_NOSOLNS;		/* Get number of solutions */

		/* Check test result */
		for (i = 0; i < r; i++) {
			chp.p[0] = tp[i].p[0];
			chp.p[1] = tp[i].p[1];
			chp.p[2] = tp[i].p[2];
			chp.p[3] = tp[i].p[3];
			chp.v[0] = -1.0;
			chp.v[1] = -1.0;
			chp.v[2] = -1.0;
			chp.v[3] = -1.0;
			printf("Result %d = inp: %f, %f, %f, %f\n",i, tp[i].p[0],tp[i].p[1],tp[i].p[2],tp[i].p[3]);
			printf("           out: %f, %f, %f, %f\n",tp[i].v[0],tp[i].v[1],tp[i].v[2],tp[i].v[3]);

			if (rss->interp(rss, &chp))
				printf("Fwd check %d failed!\n",i);
			else {
				int p;
				double er = 0.0;
				for (p = 0; p < FDI; p++)
					er += (v[p] - chp.v[p]) *  (v[p] - chp.v[p]);
				er = sqrt(er);
				printf("Fwd check error %d = %f\n",i,er);

				if (cl != 0) {
					/* Check if clipped result us reasonable */
					/* ~~~ */
				}
			}
			{
				printf("Output limit = %f\n",limitf(NULL, tp[i].p));
			}
		}
	} else
		printf("Rev lookup result returned none\n");
	}
	rss->del(rss);
	return 0;
}


#ifdef NEVER
/* Standard interface for powell function */
/* return err on sucess, -1.0 on failure */
/* Result will be in cp */
double powell(
int di,					/* Dimensionality */
double cp[],			/* Initial starting point */
double s[],				/* Size of initial search area */
double ftol,			/* Tollerance of error change to stop on */
int maxit,				/* Maximum iterations allowed */
double (*func)(void *fdata, double tp[]),		/* Error function to evaluate */
void *fdata				/* Opaque data needed by function */
) {
#endif
