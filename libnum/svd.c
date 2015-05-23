
/*
 * Singular Value Decomposition,
 * from the Burton S. Garbow's EISPACK FORTRAN code,
 * based on the algorithm of Golub and Reinsch in
 * "Handbook of Automatic Computation",
 * with some guidance from R. B Davie's newmat09.
 *
 * Copyright 2000 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#include "numsup.h"
#include "svd.h"

#ifndef NEVER
/* Compute the pythagorian distance sqrt(a^2 + b^2) */
/* taking care of under or overflow. */
double pythag(
double a,
double b) {
	double aba, abb;

	aba = fabs(a);
	abb = fabs(b);

	if (aba > abb) {
		double boa;
		boa = abb/aba;
		return aba * sqrt(1.0 + boa * boa);
	} else {
		double aob;
		if (abb == 0.0)
			return 0.0;
		aob = aba/abb;
		return abb * sqrt(1.0 + aob * aob);
	}
}
#else	/* Quicker, but less robust */
#define pythag(a, b) sqrt((a) * (a) + (b) * (b))
#endif

#define MAXITS 30

/* Compute Singular Value Decomposition of A = U.W.Vt */
/* Return status value: */
/* 0 - no error */
/* 1 - Too many itterations */
int svdecomp(
double **a,		/* A[0..m-1][0..n-1], return U[][] */
double  *w,		/* return W[0..n-1] */
double **v,		/* return V[0..n-1][0..n-1] (not transpose!) */
int      m,		/* Number of equations */
int      n		/* Number of unknowns */
) {	
	double eps = DBL_EPSILON;		/* 1.0 + eps = 1.0 */
	double tol = DBL_MIN/eps;		/* Minumum +ve value/eps */
	double *rv1, RV1[10];
	double anm;
	int i, j, k;
	int its;

	if (n <= 10) 
		rv1 = RV1;					/* Allocate fast off stack */
	else
		rv1 = dvector(0, n-1);

	/* Housholder reduction of A to bidiagonal form */
	anm = 0.0;
	rv1[0] = 0.0;		/* Will always == 0.0 */
	for (i = 0; i < n; i++) {	/* For each element in the diagonal of A */
		int ip1 = i + 1;

		/* Deal with lower column at i */
		w[i] = 0.0;
		if (i < m) {			/* If it makes sense to go from row i .. m-1 */ 
			double ss, ff = 0.0;

			for (ss = 0.0, k = m-1; k >= i; k--) {	/* Sum of squares of column */
				ff = a[k][i];
				ss += ff * ff;
			}						/* Note ff = A[i][i] */
			if (ss >= tol) {
				double gg, hh;
				gg = sqrt(ss);
				w[i] = gg = ff < 0.0 ? gg : -gg;	/* gg has -sign of ff */
				hh = ff * gg - ss;
				a[i][i] = ff - gg;

				/* For all lower columns to the right of this one */
				for (j = ip1; j < n; j++) {				/* Column j */
					double tt;
					for (ss = 0.0, k = i; k < m; k++)
						ss += a[k][j] * a[k][i];		/* Sum of products of i and j columns */
					tt = ss / hh;
					for (k = i; k < m; k++)
						a[k][j] += tt * a[k][i];		/* Add sumprod/hh to column j */
				}
			}
		}

		/* Deal with upper super row at i */
		if (ip1 < n) {				/* If it makes sense to go from column i+1 .. n-1 */
			rv1[ip1] = 0.0;
			if (i < m) {			/* If it makes sense to process row i */
				double ss, ff = 0.0;
				for (ss = 0.0, k = n-1; k >= ip1; k--) {	/* Sum of squares of row */
					ff = a[i][k];
					ss += ff * ff;
				}					/* Note ff = A[i][ip1] */
				if (ss >= tol) {
					double gg, hh;
					gg = sqrt(ss);
					rv1[ip1] = gg = ff < 0.0 ? gg : -gg;	/* gg has -sign of ff */
					hh = ff * gg - ss;
					a[i][ip1] = (ff - gg);

					/* For all upper rows below this one */
					for (j = ip1; j < m; j++) {
						double tt;
						for (ss = 0.0, k = ip1; k < n; k++)	/* Sum of products of i and j rows */
							ss += a[j][k] * a[i][k];
						tt = ss / hh;
						for (k = ip1; k < n; k++)
							a[j][k] += tt * a[i][k];		/* Add sumprod/hh to row j */
					}
				}
			}
		}
		{
			double tt;
			tt = fabs(w[i]) + fabs(rv1[i]);
			if (tt > anm)
				anm = tt;
		}
	}

	/* Accumulation of right hand transformations */
	for (i = n-1; i >= 0; i--) {
		int ip1 = i + 1;
		if (ip1 < n) {
			double gg;
			gg = rv1[ip1];
			if (gg != 0.0) {
				gg = 1.0 / gg;
				for (j = ip1; j < n; j++)
					v[j][i] = (a[i][j] / a[i][ip1]) * gg; /* Double division to avoid underflow */
				for (j = ip1; j < n; j++) {
					double ss;
					for (ss = 0.0, k = ip1; k < n; k++)
						ss += a[i][k] * v[k][j];
					for (k = ip1; k < n; k++)
						v[k][j] += ss * v[k][i];
				}
			}
			for (j = ip1; j < n; j++)
				v[i][j] = v[j][i] = 0.0;
		}
		v[i][i] = 1.0;
	}
	/* Accumulation of left hand transformations */
	for (i = n < m ? n-1 : m-1; i >= 0; i--) {
		int ip1 = i + 1;
		double gg = w[i];
		if (ip1 < n)
			for (j = ip1; j < n; j++)
				a[i][j] = 0.0;
		if (gg == 0.0) {
			for (j = i; j < m; j++)
				a[j][i] = 0.0;
		} else {
			gg = 1.0 / gg;
			if (ip1 < n) {
				for (j = ip1; j < n; j++) {
					double ss, ff;
					for (ss = 0.0, k = ip1; k < m; k++)
						ss += a[k][i] * a[k][j];
					ff = (ss / a[i][i]) * gg;		/* Double division to avoid underflow */
					for (k = i; k < m; k++)
						a[k][j] += ff * a[k][i];
				}
			}
			for (j = i; j < m; j++)
				a[j][i] *= gg;
		}
		a[i][i] += 1.0;
	}

	eps *= anm;

	/* Fully diagonalize bidiagonal result, by */
	/* successive QR rotations. */
	for (k = (n-1); k >= 0; k--) {					/* For all the singular values */
		for (its = 0;; its++) {
			int flag;
			int lm1 = 0;
			int ll;
			double zz;

			/* Test for splitting */
			for (flag = 1, ll = k; ll >= 0; ll--) {
				lm1 = ll - 1;
				if (fabs(rv1[ll]) <= eps) {	/* Note always stops at 0 because rv1[0] = 0.0 */
					flag = 0;
					break;
				}
				if (fabs(w[lm1]) <= eps)
					break;
			}
			if (flag != 0) {
				double cc = 0.0;
				double ss = 1.0;
				for (i = ll; i <= k; i++) {
					double ff, gg, hh;
					gg = rv1[i];
					rv1[i] = cc * gg;
					ff = ss * gg;
					if (fabs(ff) <= eps)
						break;						/* Found acceptable solution */
					gg = w[i];
					w[i] = hh = pythag(ff, gg);
					hh = 1.0 / hh;
					cc =  gg * hh;
					ss = -ff * hh;

					/* Apply rotation */
					for (j = 0; j < m; j++) {
						double y1, z1;
						y1 = a[j][lm1];
						z1 = a[j][i];
						a[j][lm1] = y1 * cc + z1 * ss;
						a[j][i]   = z1 * cc - y1 * ss;
					}
				}
			}
			zz = w[k];
			if (k == ll) {				/* Convergence */
				if (zz < 0.0) {
					w[k] = -zz;			/* Make singular value non-negative */
					for (j = 0; j < n; j++)
						v[j][k] = (-v[j][k]);
				}
				break;
			}
			if (its == MAXITS) {
/*				fprintf(stderr,"No convergence in %d SVDCMP iterations",MAXITS); */
				if (rv1 != RV1)
					free_dvector(rv1, 0, n-1);
				return 1;
			}
			{
				double ff, gg, hh, cc, ss, xx, yy;
				int km1;

				km1 = k - 1;
				xx = w[ll];
				yy = w[km1];
				gg = rv1[km1];
				hh = rv1[k];
				ff = ((yy - zz) * (yy + zz) + (gg - hh) * (gg + hh)) / (2.0 * hh * yy);
				gg = pythag(ff, 1.0);
				gg = ff < 0.0 ? -gg : gg;
				ff = ((xx - zz) * (xx + zz) + hh * ((yy / (ff + gg)) - hh)) / xx;
				cc = ss = 1.0;

				for (j = ll; j <= km1; j++) {
					double f2, g2, y2, h2, z2;
					int jp1 = j + 1;
					g2 = rv1[jp1];
					y2 = w[jp1];
					h2 = ss * g2;
					g2 = cc * g2;
					rv1[j] = z2 = pythag(ff, h2);
					cc = ff / z2;
					ss = h2 / z2;
					f2 = xx * cc + g2 * ss;
					g2 = g2 * cc - xx * ss;
					h2 = y2 * ss;
					y2 = y2 * cc;

					/* Apply rotation */
					for (i = 0; i < n; i++) {
						double x1, z1;
						x1 = v[i][j];
						z1 = v[i][jp1];
						v[i][j]   = x1 * cc + z1 * ss;
						v[i][jp1] = z1 * cc - x1 * ss;
					}
					w[j] = z2 = pythag(f2, h2);
					if (z2 != 0.0) {			/* Rotation can be arbitrary */
						z2 = 1.0 / z2;
						cc = f2 * z2;
						ss = h2 * z2;
					}
					ff = (cc * g2) + (ss * y2);
					xx = (cc * y2) - (ss * g2);

					/* Apply rotation */
					for (i = 0; i < m; i++) {
						double y1, z1;
						y1 = a[i][j];
						z1 = a[i][jp1];
						a[i][j]   = y1 * cc + z1 * ss;
						a[i][jp1] = z1 * cc - y1 * ss;
					}
				}
				rv1[ll] = 0.0;
				rv1[k]  = ff;
				w[k] = xx;
			}
		}
	}
	if (rv1 != RV1)
		free_dvector(rv1, 0, n-1);

	return 0;
}

/* --------------------------- */
/* Threshold the singular values W[] */ 
void svdthresh(
double w[],		/* Singular values */
int      n		/* Number of unknowns */
) {
	int i;
	double maxw;

	/* Threshold the w[] values */
	for (maxw = 0.0, i = 0; i < n; i++) {
		if (w[i] > maxw)
			maxw = w[i];
	}
	maxw *= 1.0e-12;
	for (i = 0; i < n; i++) {
		if (w[i] < maxw)
			w[i] = 0.0;
	}
}

/* --------------------------- */
/* Threshold the singular values W[] to give */
/* a specific degree of freedom. */ 
void svdsetthresh(
double w[],		/* Singular values */
int      n,		/* Number of unknowns */
int      dof	/* Expected degree of freedom */
) {
	int i, j;

	/* Set the dof smallest elements to zero */
	/* (This algorithm is simple but not quick) */
	for (j = 0; j < dof;) {
		int k;
		double minv = 1e38;
		for (k = j = i = 0; i < n; i++) {
			if (w[i] == 0.0) {
				j++;
				continue;
			}
			if (w[i] < minv) {
				minv = w[i];
				k = i;
			}
		}
		if (j < dof)	/* Zero next smallest */
			w[k] = 0.0;
	}
}

/* --------------------------- */
/* Use output of svdcmp() to solve overspecified and/or */
/* singular equation A.x = b */
int svdbacksub(
double **u,		/* U[0..m-1][0..n-1] U, W, V SVD decomposition of A[][] */
double  *w,		/* W[0..n-1] */
double **v,		/* V[0..n-1][0..n-1] (not transpose!) */
double b[],		/* B[0..m-1]  Right hand side of equation */
double x[],		/* X[0..n-1]  Return solution. (May be the same as b[]) */
int      m,		/* Number of equations */
int      n		/* Number of unknowns */
) {
	int i, j;
	double *tmp, TMP[10]; /* Intermediate value of B . U-1 . W-1 */

	if (n <= 10)
		tmp = TMP;
	else
		tmp = dvector(0, n-1);

	/* A . X = B == U . W . Vt . X = B */
	/* and U, W, and Vt are trivialy invertable */

	/* Compute B . U-1 . W-1 */
	for (j = 0; j < n; j++) {
		if (w[j]) {
			double s;
			for (s = 0.0, i = 0; i < m; i++)
				s += b[i] * u[i][j];
			s /= w[j];
			tmp[j] = s;
		} else {
			tmp[j] = 0.0;
		}
	}
	/* Compute T. V-1 */
	for (j = 0; j < n; j++) {
		double s;
		for (s = 0.0, i = 0; i < n; i++)
			s += v[j][i] * tmp[i];
		x[j] = s;
	}
	if (tmp != TMP)
		free_dvector(tmp, 0, n-1);
	return 0;
}


/* --------------------------- */
/* Solve the equation A.x = b using SVD */
/* (The w[] values are thresholded for best accuracy) */
/* Return non-zero if no solution found */
int svdsolve(
double **a,		/* A[0..m-1][0..n-1] input A[][], will return U[][] */
double b[],		/* B[0..m-1]  Right hand side of equation, return solution */
int      m,		/* Number of equations */
int      n		/* Number of unknowns */
) {
	int i;
	double *w, W[8];
	double **v, *VP[8], V[8][8];
	double maxw;

	if (n <= 8) {
		w = W;
		VP[0] = V[0]; VP[1] = V[1]; VP[2] = V[2]; VP[3] = V[3];
		VP[4] = V[4]; VP[5] = V[5]; VP[6] = V[6]; VP[7] = V[7];
		v = VP;
	} else {
		w = dvector(0, n-1);
		v = dmatrix(0, n-1, 0, n-1);
	}

	/* Singular value decompose */
	if (svdecomp(a, w, v, m, n)) {
		if (w != W) {
			free_dvector(w, 0, n-1);
			free_dmatrix(v, 0, n-1, 0, n-1);
		}
		return 1;
	}

	/* Threshold the w[] values */
	for (maxw = 0.0, i = 0; i < n; i++) {
		if (w[i] > maxw)
			maxw = w[i];
	}
	maxw *= 1.0e-12;
	for (i = 0; i < n; i++) {
		if (w[i] < maxw)
			w[i] = 0.0;
	}

	/* Back substitute to solve the equation */
	svdbacksub(a, w, v, b, b, m, n);

	if (w != W) {
		free_dvector(w, 0, n-1);
		free_dmatrix(v, 0, n-1, 0, n-1);
	}
	return 0;
}


/* --------------------------- */
/* Solve the equation A.x = b using SVD */
/* The top s out of n singular values will be used */
/* Return non-zero if no solution found */
int svdsolve_s(
double **a,		/* A[0..m-1][0..n-1] input A[][], will return U[][] */
double b[],		/* B[0..m-1]  Right hand side of equation, return solution */
int      m,		/* Number of equations */
int      n,		/* Number of unknowns */
int      s		/* Number of singular values */
) {
	int i, j;
	double *w, W[8];
	int *sw, SW[8];
	double **v, *VP[8], V[8][8];
	double maxw;

	if (n <= 8) {
		w = W;
		sw = SW;
		VP[0] = V[0]; VP[1] = V[1]; VP[2] = V[2]; VP[3] = V[3];
		VP[4] = V[4]; VP[5] = V[5]; VP[6] = V[6]; VP[7] = V[7];
		v = VP;
	} else {
		w = dvector(0, n-1);
		sw = ivector(0, n-1);
		v = dmatrix(0, n-1, 0, n-1);
	}

	/* Singular value decompose */
	if (svdecomp(a, w, v, m, n)) {
		if (w != W) {
			free_dvector(w, 0, n-1);
			free_dmatrix(v, 0, n-1, 0, n-1);
		}
		return 1;
	}

	/* Create sorted index of w[] */
	for (maxw = 0.0, i = 0; i < n; i++) {
		sw[i] = i;
		if (w[i] > maxw)
			maxw = w[i];
	}
	maxw *= 1.0e-12;

	/* Really dumb exchange sort.. */
	for (i = 0; i < (n-1); i++) {
		for (j = i+1; j < n; j++) {
			if (w[sw[i]] > w[sw[j]]) {
				int tt = sw[i];
				sw[i] = sw[j];
				sw[j] = tt;
			}
		}
	}

	/* Set the (n - s) smallest values to zero */
	s = n - s;
	if (s < 0)
		s = 0;
	if (s > n)
		s = n;
	for (i = 0; i < s; i++)
		w[sw[i]] = 0.0;

	/* And threshold them too */
	for (maxw = 0.0, i = 0; i < n; i++) {
		if (w[i] < maxw)
			w[i] = 0.0;
	}

	/* Back substitute to solve the equation */
	svdbacksub(a, w, v, b, b, m, n);

	if (w != W) {
		free_dvector(w, 0, n-1);
		free_ivector(sw, 0, n-1);
		free_dmatrix(v, 0, n-1, 0, n-1);
	}
	return 0;
}


/* --------------------------- */
/* Solve the equation A.x = b using Direct calculation, LU or SVD as appropriate */
/* Return non-zero if no solution found */

#include "ludecomp.h"

int gen_solve_se(
double **a,		/* A[0..m-1][0..n-1] input A[][], will return U[][] */
double b[],		/* B[0..m-1]  Right hand side of equation, return solution */
int      m,		/* Number of equations */
int      n		/* Number of unknowns */
) {
	if (n == m) {
		if (n == 1) {	/* So simple, solve it directly */
			double tt = a[0][0];
			if (fabs(tt) <= DBL_MIN)
				return 1;
			b[0] = b[0]/tt;
			return 0;
		} else {
			return solve_se(a, b, n);
		}
	} else {
		return svdsolve(a, b, m, n);
	}
}

























