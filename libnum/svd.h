#ifndef SVD_H
#define SVD_H

/*
 * Copyright 2000 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#ifdef __cplusplus
	extern "C" {
#endif

/* Compute Singular Value Decomposition of A = U.W.Vt */
/* Return status value: */
/* 0 - no error */
/* 1 - m < n error */
int svdecomp(
double **a,		/* A[0..m-1][0..n-1], return U[][] */
double  *w,		/* return W[0..n-1] */
double **v,		/* return V[0..n-1][0..n-1] (not transpose!) */
int      m,		/* Number of equations */
int      n		/* Number of unknowns */
);

/* Threshold the singular values W[] */ 
void svdthresh(
double w[],		/* Singular values */
int      n		/* Number of unknowns */
);

/* Threshold the singular values W[] to give a specific dof */ 
void svdsetthresh(
double w[],		/* Singular values */
int      n,		/* Number of unknowns */
int      dof	/* Expected degree of freedom */
);

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
);

/* Solve the equation A.x = b using SVD */
/* (The w[] values are thresholded for best accuracy) */
/* Return non-zero if no solution found */
/* !!! Note that A[][] will be changed !!! */
int svdsolve(
double **a,		/* A[0..m-1][0..n-1] input A[][], will return U[][] */
double b[],		/* B[0..m-1]  Right hand side of equation, return solution */
int      m,		/* Number of equations */
int      n		/* Number of unknowns */
);

/* Solve the equation A.x = b using SVD */
/* The top s out of n singular values will be used */
/* Return non-zero if no solution found */
/* !!! Note that A[][] will be changed !!! */
int svdsolve_s(
double **a,		/* A[0..m-1][0..n-1] input A[][], will return U[][] */
double b[],		/* B[0..m-1]  Right hand side of equation, return solution */
int      m,		/* Number of equations */
int      n,		/* Number of unknowns */
int      s		/* Number of unknowns */
);

/* Solve the equation A.x = b using Direct calculation, LU or SVD as appropriate */
/* Return non-zero if no solution found */
/* !!! Note that A[][] will be changed !!! */
int gen_solve_se(
double **a,		/* A[0..m-1][0..n-1] input A[][], will return U[][] */
double b[],		/* B[0..m-1]  Right hand side of equation, return solution */
int      m,		/* Number of equations */
int      n		/* Number of unknowns */
);

#ifdef __cplusplus
	}
#endif

#endif /* SVD_H */
