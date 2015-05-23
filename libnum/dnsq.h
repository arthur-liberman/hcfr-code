#ifndef DNSQ_H
#define DNSQ_H

/* dnsq non-linear equation solver public interface definition */
/*
 * This concatenation Copyright 1998 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#ifdef __cplusplus
	extern "C" {
#endif

/*

	return values from dnsq() and dnsqe():

 	0  Improper input parameters. 

	1  Relative error between two consecutive iterates is at
	   most XTOL. Normal sucess return value. 

	2  Number of calls to FCN has reached or exceeded MAXFEV. 

	3  XTOL is too small.  No further improvement in the
	   approximate solution X is possible. 

	4  Iteration is not making good progress, as measured by
	   the improvement from the last five Jacobian evaluations. 

	5  Iteration is not making good progress, as measured 
	   by the improvement from the last ten iterations. 
	   Return value if no zero can be found from this starting
       point.
 */

/* dnsq function */
int dnsq(
	void *fdata,	/* Opaque data pointer, passed to called functions */
	int (*fcn)(void *fdata, int n, double *x, double *fvec, int iflag),
					/* Pointer to function we are solving */
	int (*jac)(void *fdata, int n, double *x, double *fvec, double **fjac),
					/* Optional function to compute jacobian, NULL if not used */
	double **sjac,	/* Optional initial jacobian matrix/last jacobian matrix. NULL if not used.*/
	int startsjac,	/* Set to non-zero to use sjac as the initial jacobian */
	int n,			/* Number of functions and variables */
	double x[],		/* Initial solution estimate, RETURNs final solution */
	double fvec[],	/* Array that will be RETURNed with thefunction values at the solution */
	double dtol,	/* Desired tollerance of the solution */
	double tol,		/* Desired tollerance of root */
	int maxfev,		/* Set excessive Number of Function Evaluations */
	int ml,			/* number of subdiagonals within the band of the Jacobian matrix. */
	int mu, 		/* number of superdiagonals within the band of the Jacobian matrix. */
	double epsfcn,	/* determines suitable step for forward-difference approximation */
	double diag[],	/* Optional scaling factors, use NULL for internal scaling */
	double factor,	/* Determines the initial step bound */
	double maxstep, /* Determines the maximum subsequent step bound (0.0 for none) */
	int nprint, 	/* Turn on debugging printouts from func() every nprint itterations */
	int *nfev,		/* RETURNs the number of calls to fcn() */ 
	int *njev		/* RETURNs the number of calls to jac() */
);

/* User supplied functions */

/* calculate the functions at x[] */
int dnsq_fcn(		/* Return < 0 on abort */
	void *fdata,	/* Opaque data pointer */
	int n,			/* Dimenstionality */
	double *x,		/* Multivariate input values */
	double *fvec,	/* Multivariate output values */
	int iflag		/* Flag set to 0 to trigger debug output */
);

/* Calculate Jacobian at x[] */
int dnsq_jac(		/* Return < 0 on abort */
	void *fdata,	/* Opaque data pointer */
 	int n,			/* Dimensionality */
 	double *x,		/* Point to caluculate Jacobian at  (do not alter) */
 	double *fvec,	/* Function values at x (do not alter) */
 	double **fjac	/* Return n by n Jacobian in this array */
);

#define		M_LARGE 1.79e+308
#define		M_DIVER 2.22e-15

/* Simplified dnsq() */
int dnsqe(
	void *fdata,	/* Opaque pointer to pass to fcn() and jac() */
	int (*fcn)(void *fdata, int n, double *x, double *fvec, int iflag),
					/* Pointer to function we are solving */
	int (*jac)(void *fdata, int n, double *x, double *fvec, double **fjac),
					/* Optional function to compute jacobian, NULL if not used */
	int n,			/* Number of functions and variables */
	double x[],		/* Initial solution estimate, RETURNs final solution */
	double ss,		/* Initial search area */
	double fvec[],	/* Array that will be RETURNed with thefunction values at the solution */
	double dtol,	/* Desired tollerance of the solution */
	double tol,		/* Desired tollerance of root */
	int maxfev,		/* Maximum number of function calls. set to 0 for automatic */
	int nprint	 	/* Turn on debugging printouts from func() every nprint itterations */
);

#ifdef __cplusplus
	}
#endif

#endif /* DNSQ_H */










