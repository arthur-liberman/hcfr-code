
/* 1 dimentional root finding code */
/* inspired by the Van Wijngaarden-Dekker-Brent */
/* method algorithm presented in */
/* "Numerical Recipes in C", by W.H.Press, */
/* B.P.Flannery, S.A.Teukolsky & W.T.Vetterling. */

/*
 * Copyright 2000 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#include "numsup.h"
#include "zbrent.h"

#undef DEBUG

#define ZBRACK_MAXTRY 40			/* Maximum tries to bracket */
#define ZBRACK_GOLD 1.618034		/* Golden ratio */

/* Bracket search function */
/* return  0 on sucess */
/*        -1 on no range */
/*        -2 on too many itterations */
int zbrac(
double *x1p,							/* Input and output bracket values */
double *x2p,							/* Min and Max */
double (*func)(void *fdata, double tp),	/* function to evaluate */
void *fdata								/* Opaque data pointer */
) {
	int i;
	double x1, x2;		/* Bracket under consideration */
	double f1, f2;		/* Function values at points x1 and x2 */

	x1 = *x1p;
	x2 = *x2p;
	if (x1 == x2)		/* Nowhere to go */
		return -1;

	f1 = (*func)(fdata, x1);		/* Initial function values */
	f2 = (*func)(fdata, x2);

	for (i = 0; i < ZBRACK_MAXTRY; i++) {
		if ((f1 * f2) < 0.0)	 {
			*x1p = x1;
			*x2p = x2;
			return 0;			/* If signs are opposite, we're done */
		}
		if (fabs(f2) > fabs(f1)) {	/* Move smaller in direction away from larger */
			x1 += ZBRACK_GOLD * (x1 - x2);
			f1 = (*func)(fdata, x1);
		} else {
			x2 += ZBRACK_GOLD * (x2 - x1);
			f2 = (*func)(fdata, x2);
		}
	}
	return -2;
}

#undef ZBRACK_GOLD
#undef ZBRACK_MAXTRY


#define ZBRENT_MAXIT 100

/* Root finder */
/* return  0 on sucess */
/*        -1 on root not bracketed */
/*        -2 on too many itterations */
int zbrent(
double *rv,								/* Return value */
double ax,								/* Bracket to search */
double bx,								/* (Min, Max) */
double tol,								/* Desired tollerance */
double (*func)(void *fdata, double tp),	/* function to evaluate */
void *fdata								/* Opaque data pointer */
) {
	int i;
	double         cx;			/* Trial points, bx = best current */
	double af ,bf, cf;			/* Function values at those points */

	af = (*func)(fdata, ax);
	bf = (*func)(fdata, bx);

	/* Sanity check bracketing */
	if (af * bf > 0.0)
		return -1;				/* No good */

	cx = bx;					/* Force bisection for first itter */
	cf = bf;
	for (i = 0; i < ZBRENT_MAXIT; i++) {
		double xdel;		/* Bisection delta to bx */
		double del = 1e80;	/* Delta to be applied to bx */
		double pdel = 1e80;	/* Last del from interpolation step */
		double tol1;		/* Minimum reasonable change in bx */

		/* Make bx and cx straddle root */
		if (bf * cf > 0.0) {		/* bx and cx don't straddle root */
			cx = ax;				/* ax must, so make cx = ax */
			cf = af;
			pdel = del = bx - ax;
		}

    	/* Make bx be point closest to solution */
		if (fabs(cf) < fabs(bf)) {
			ax = bx;				/* swap bx & cx, and make ax == new cx */
			af = bf;
			bx = cx;
			bf = cf;
			cx = ax;
			cf = af;
		}
		tol1 = (0.5 * tol) + (2.0 * DBL_EPSILON * fabs(bx));	/* Minimum tollerable bx move */
		xdel = 0.5 * (cx - bx);		/* Delta to bx for bisection move */

		if (bf == 0.0 || fabs(xdel) <= tol1) {	/* If exact soln, or last was min move */
			*rv = bx;
			return 0;
		}
		if (fabs(pdel) >= tol1 && fabs(af) > fabs(bf)) { /* Try inv. quadratic interpolation */
			double P, Q;

			if (ax == cx) {		/* Only have 2 points, use extrapolation */
				double R;
				R = bf / cf;
				P = (cx - bx) * R;
				Q = R - 1.0;
			} else {			/* Brent's interpolation of 3 points */
				double R, S, T;
				R = bf / cf;
				S = bf / af;
				T = af / cf;
				P = S * ((T * (R - T) * (cx - bx)) - ((1.0 - R) * (bx - ax)));
				Q = (T - 1.0) * (R - 1.0) * (S - 1.0);
			}
			if (P < 0.0)	/* Keep sign of P/Q with abs(P) */
				Q = -Q;
			P = fabs(P);
			{
				double min1, min2;
				min1 = (3.0 * xdel * Q) - (tol1 * fabs(Q));
				min2 = fabs(pdel * Q);
				if (min2 < min1)
					min1 = min2;
	
				if ((2.0 * P) < min1) {	/* Interpolation looks OK */
					pdel = del;			/* Remember last delta */
					del = P / Q;		/* Next delta */
				} else {
					pdel = del = xdel;	/* Use bisection */
				}
			}
		} else {
			pdel = del = xdel;			/* Use bisection */
		}
		ax = bx;						/* a keeps previous best point */
		af = bf;
		if (fabs(del) > tol1)			/* Delta looks reasonable */
			bx += del;
		else
			bx += (xdel > 0.0 ? tol1 : -tol1);	/* Do minimum move in direction of bisection */
		bf = (*func)(fdata, bx);
	}
	return -2;			/* Too many iterations */
}














