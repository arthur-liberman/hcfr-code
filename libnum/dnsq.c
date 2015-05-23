
/* Concatenated dnsq code */

/*
 * This concatenation, translation to C and modifications,
 * Copyright 1998 Graeme Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#include "numsup.h"
#include "dnsq.h"		/* Public interface definitions */

#undef DEBUG

typedef long int bool;
#ifndef TRUE
# define FALSE (0)
# define TRUE (!FALSE)
#endif

#ifndef min
#define min(a,b) ((a) <= (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) ((a) >= (b) ? (a) : (b))
#endif
#ifndef fabs
#define fabs(x) ((x) >= 0.0 ? (x) : -(x))
#endif

/* Forward difference jacobian approximation */
static int dfdjc1(
	void *fdata,
	int (*fcn)(void *fdata, int n, double *x, double *fvec, int iflag),
	int n, double *x, double * fvec, double *fjac,
	int ldfjac, int ml, int mu,
	double epsfcn, double *wa1, double *wa2);

/* QR factorization */
static int dqrfac(int m, int n, double *a, int lda,
	bool pivot, int *ipvt, double *sigma,
	double *acnorm, double *wa);

/* Use QR decomposition to form the orthogonal matrix */
static int dqform(int m, int n, double *q, int ldq, double *wa);

/* QR decomposition update */
static int d1updt(int m, int n, double *s,
	double *u, double *v, double *w);

/* Jacobian rotation, called by QR update */
static int d1mpyq(int m, int n, double *a, int lda,
	double *v, double *w);

/* Compute norm of a vector */
static double denorm(int n, double *x);

/* Line search */
static int ddoglg(int n, double *r, double *diag, double *qtb,
	double delta, double *x, double *wa1, double *wa2);

/***************************************************************/

/*
 * A simplified interface to dnsq().
 */

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
	double dtol,	/* Desired delta tollerance of the solution */
	double atol,	/* Desired absolute tollerance of the solution */
	int maxfev,		/* Maximum number of function calls. set to 0 for automatic */
	int nprint	 	/* Turn on debugging printouts from func() every nprint itterations */
) {
	int info = 0;		/* Return status */

	int nfev, njev;
	int index, ml, lr, mu;
	double epsfcn = ss * ss;		/* Jacobian estimate step */
	double factor = ss;		/* Initial step size */
	double maxstep = 0.0;	/* Subsequent step size (not working ??) */

	if (maxfev <= 0) {
		maxfev = (n + 1) * 100;
		if (jac == NULL)
			maxfev <<= 1;
	}
	ml = n - 1;		/* number of subdiagonals within the band of the Jacobian matrix. */
	mu = n - 1;		/* number of superdiagonals within the band of the Jacobian matrix. */

	lr = n * (n + 1) / 2;
	index = n * 6 + lr;

	/* Call dnsq. */
	info = dnsq(fdata, fcn, jac, NULL, 0,
			n, &x[0], &fvec[0], dtol, atol,
			maxfev, ml, mu, epsfcn, NULL, factor, maxstep, nprint, 
			&nfev, &njev);

	if (info == 5)
		info = 4;
	if (info == 0)
		warning("dnsqe: invalid input parameter.");
	return info;
} /* dnsqe */



/***************************************************************/
/*
 * Library:  SLATEC 
 * Category: F2A 
 * Type:     Double precision (SNSQE-S, DNSQE-D) 
 * Keywords  easy-to-use, nonlinear square system, 
 *			 powell hybrid method, zeros 
 * Author:   Hiebert, K. L. (SNLA) 
 * Translated to C by f2c and Graeme W. Gill
 *
 * 1. Purpose. 
 *
 * The purpose of DNSQ is to find a zero of a system of N nonlinear 
 * functions in N variables by a modification of the Powell 
 * hybrid method.  The user must provide a subroutine which 
 * calculates the functions.  The user has the option of either to 
 * provide a subroutine which calculates the Jacobian or to let the 
 * code calculate it by a forward-difference approximation. 
 * This code is the combination of the MINPACK codes (Argonne) 
 * HYBRD and HYBRDJ. 
 *
 * 2. Subroutine and Type Statements. 
 *
 *	   SUBROUTINE DNSQ(FCN,JAC,N,X,FVEC,FJAC,LDFJAC,XTOL,MAXFEV, 
 *	  				 ML,MU,EPSFCN,DIAG,MODE,FACTOR,NPRINT,INFO,NFEV, 
 *	  				 NJEV,R,LR,QTF,WA1,WA2,WA3,WA4) 
 *	   INTEGER N,MAXFEV,ML,MU,MODE,NPRINT,INFO,NFEV,LDFJAC,NJEV,LR 
 *	   DOUBLE PRECISION XTOL,EPSFCN,FACTOR 
 *	   DOUBLE PRECISION 
 *	   X(N),FVEC(N),DIAG(N),FJAC(LDFJAC,N),R(LR),QTF(N), 
 *	  	 WA1(N),WA2(N),WA3(N),WA4(N) 
 *	   EXTERNAL FCN,JAC 
 *
 * 3. Parameters. 
 *
 *	   Parameters designated as input parameters must be specified on 
 *	   entry to DNSQ and are not changed on exit, while parameters 
 *	   designated as output parameters need not be specified on entry 
 *	   and are set to appropriate values on exit from DNSQ. 
 *
 *	   fcn() is the name of the user-supplied subroutine which calculates 
 *		 the functions.  fcn() must be declared in an external statement 
 *		 in the user calling program, and should be written as follows. 
 *
 *	int fcn(
 *      void *fdata,
 *		int n,
 *		double *x,
 * 		double *fvec,
 *		int iflag);
 *	{
 *		 calculate the functions at x and 
 *		 return this vector in fvec. 
 *       print out vector if iflag == 0
 *		 return 0 (or < 0 to abort)
 *	}
 *		 The value 0 should be returned fcn() unless the 
 *		 user wants to terminate execution of DNSQ.  In this case
 *		 return a negative integer. 
 *
 *	   jac() is the name of the user-supplied subroutine which calculates 
 *		 the Jacobian.  If jac!=NULL, then jac() must be declared in an 
 *		 external statement in the user calling program, and should be 
 *		 written as follows. 
 *
 *	int jac(
 *      void *fdata,
 *		int n,
 *		double *x,
 *		double *fvec,
 *		double **fjac,
 *	{
 *		 Calculate the Jacobian at x and return this 
 *		 matrix in fjac.  fvec contains the function 
 *		 values at x and should not be altered. 
 *		 return 0 (or < 0 to abort)
 *	}
 *		 The value 0 should be returned jac() unless the 
 *		 user wants to terminate execution of DNSQ.  In this case
 *		 return a negative integer. 
 *
 *		 If jac == NULL, then the code will approximate the
 *       Jacobian by forward-differencing. 
 *
 *	double **sjac;
 *		sjac is an optional n by n matrix that can hold an initial
 *		jacobian matrix that will be used in preference to calling the jac()
 *		function, or to using forward differencing. If the array is provided,
 *		it will also contain the last jacobian matrix used before exiting.
 * 		If this array is not used, it should be set to NULL.
 *
 *	int startsjac;
 *		styatsjac is a flag, that when set to non-zero, will cause the
 *		sjac[] array to be used as the initial jacobian matrix, in preference
 *		to calling the jac() function, or to using forward differencing.
 *
 *	   n is a positive integer input variable set to the number of 
 *		 functions and variables. 
 *
 *	   x is an array of length n.  On input x must contain an initial 
 *		 estimate of the solution vector.  On output x contains the 
 *		 final estimate of the solution vector. 
 *
 *	   fvec is an output array of length n which contains the functions 
 *		 evaluated at the output x. 
 *
 *	   fjac is an output N by N array which contains the orthogonal 
 *		 matrix Q produced by the QR factorization of the final 
 *		 approximate Jacobian. 
 *
 *	   ldfjac is a positive integer input variable not less than n 
 *		 which specifies the leading dimension of the array fjac. 
 *
 *	   dtol is a nonnegative input variable.  Termination occurs when 
 *		 the relative error between two consecutive iterates is at most 
 *		 dtol.  Therefore, dtol measures the relative error desired in 
 *		 the approximate solution.  Section 4 contains more details 
 *		 about dtol. 
 *
 *	   tol is a nonnegative input variable.  Termination occurs when 
 *		 the value of all the function values falls below this threshold.
 *       Termination occurs when either the dtol or tol condition is met.
 *
 *	   maxfev is a positive integer input variable.  Termination occurs 
 *		 when the number of calls to fcn is at least maxfev by the end 
 *		 of an iteration. 
 *
 *	   ml is a nonnegative integer input variable which specifies the 
 *		 number of subdiagonals within the band of the Jacobian matrix. 
 *		 If the Jacobian is not banded or jac!=null, set ml to at 
 *		 least n - 1. 
 *
 *	   mu is a nonnegative integer input variable which specifies the 
 *		 number of superdiagonals within the band of the Jacobian 
 *		 matrix.  If the Jacobian is not banded or jac!=null, set mu to at 
 *		 least n - 1. 
 *
 *	   epsfcn is an input variable used in determining a suitable step 
 *		 for the forward-difference approximation.  This approximation 
 *		 assumes that the relative errors in the functions are of the 
 *		 order of epsfcn.  If epsfcn is less than the machine 
 *		 precision, it is assumed that the relative errors in the 
 *		 functions are of the order of the machine precision.  If 
 *		 jac!=null, then epsfcn can be ignored (treat it as a dummy 
 *		 argument). 
 *
 *	   diag is an array of length n.  If MODE = 1 (see below), diag is 
 *		 internally set.  If mode = 2, diag must contain positive 
 *		 entries that serve as implicit (multiplicative) scale factors 
 *		 for the variables. 
 *
 *	   mode is an integer input variable.  If mode = 1, the variables 
 *		 will be scaled internally.  If mode = 2, the scaling is 
 *		 specified by the input diag.  Other values of mode are 
 *		 equivalent to mode = 1. 
 *
 *	   factor is a positive input variable used in determining the 
 *		 initial step bound.  This bound is set to the product of 
 *		 factor and the Euclidean norm of diag*x if nonzero, or else to 
 *		 factor itself.  In most cases factor should lie in the 
 *		 interval (.1,100.).  100. is a generally recommended value. 
 *
 *	   nprint is an integer input variable that enables controlled 
 *		 printing of iterates if it is positive.  In this case, fcn is 
 *		 called with iflag = 0 at the beginning of the first iteration 
 *		 and every nprint iterations thereafter and immediately prior 
 *		 to return, with x and fvec available for printing. Appropriate 
 *		 print statements must be added to fcn(see example).  If nprint 
 *		 is not positive, no special calls of fcn with iflag = 0 are 
 *		 made. 
 *
 *	   info is an integer output variable.  If the user has terminated 
 *		 execution, info is set to the (negative) value of iflag.  See 
 *		 description of fcn and jac. Otherwise, INFO is set as follows. 
 *
 *		 INFO = 0  Improper input parameters. 
 *
 *		 INFO = 1  Relative error between two consecutive iterates is 
 *				   at most XTOL. Normal sucessful return value. 
 *
 *		 INFO = 2  Number of calls to FCN has reached or exceeded 
 *				   MAXFEV. 
 *
 *		 INFO = 3  XTOL is too small.  No further improvement in the 
 *				   approximate solution X is possible. 
 *
 *		 INFO = 4  Iteration is not making good progress, as measured 
 *				   by the improvement from the last five Jacobian 
 *				   evaluations. 
 *
 *		 INFO = 5  Iteration is not making good progress, as measured 
 *				   by the improvement from the last ten iterations. 
 *                 Return value if no zero can be found from this starting
 *                 point.
 *
 *		 Sections 4 and 5 contain more details about INFO. 
 *
 *	   nfev is an integer output variable set to the number of calls to 
 *		 fcn. 
 *
 *	   njev is an integer output variable set to the number of calls to 
 *		 jac. (If jac==null, then njev is set to zero.) 
 *
 * 4. Successful completion. 
 *
 *	   The accuracy of DNSQ is controlled by the convergence parameter 
 *	   XTOL.  This parameter is used in a test which makes a comparison 
 *	   between the approximation X and a solution XSOL.  DNSQ 
 *	   terminates when the test is satisfied.  If the convergence 
 *	   parameter is less than the machine precision (as defined by the 
 *	   function D1MACH(4)), then DNSQ only attempts to satisfy the test 
 *	   defined by the machine precision.  Further progress is not 
 *	   usually possible. 
 *
 *	   The test assumes that the functions are reasonably well behaved, 
 *	   and, if the Jacobian is supplied by the user, that the functions 
 *	   and the Jacobian are coded consistently.  If these conditions 
 *	   are not satisfied, then DNSQ may incorrectly indicate 
 *	   convergence.  The coding of the Jacobian can be checked by the 
 *	   subroutine DCKDER. If the Jacobian is coded correctly or JAC==NULL, 
 *	   then the validity of the answer can be checked, for example, by 
 *	   rerunning DNSQ with a tighter tolerance. 
 *
 *	   Convergence Test.  If DENORM(Z) denotes the Euclidean norm of a 
 *		 vector Z and D is the diagonal matrix whose entries are 
 *		 defined by the array DIAG, then this test attempts to 
 *		 guarantee that 
 *
 *			   DENORM(D*(X-XSOL)) .LE. XTOL*DENORM(D*XSOL). 
 *
 *		 If this condition is satisfied with XTOL = 10**(-K), then the 
 *		 larger components of D*X have K significant decimal digits and 
 *		 INFO is set to 1.  There is a danger that the smaller 
 *		 components of D*X may have large relative errors, but the fast 
 *		 rate of convergence of DNSQ usually avoids this possibility. 
 *
 *		 Unless high precision solutions are required, the recommended 
 *		 value for XTOL is the square root of the machine precision. 
 *
 *
 * 5. Unsuccessful Completion. 
 *
 *	   Unsuccessful termination of DNSQ can be due to improper input 
 *	   parameters, arithmetic interrupts, an excessive number of 
 *	   function evaluations, or lack of good progress. 
 *
 *	   Improper Input Parameters.  INFO is set to 0 if
 *		 N .LE. 0, or LDFJAC .LT. N, or 
 *		 XTOL .LT. 0.E0, or MAXFEV .LE. 0, or ML .LT. 0, or MU .LT. 0, 
 *		 or FACTOR .LE. 0.E0, or LR .LT. (N*(N+1))/2. 
 *
 *	   Arithmetic Interrupts.  If these interrupts occur in the FCN 
 *		 subroutine during an early stage of the computation, they may 
 *		 be caused by an unacceptable choice of X by DNSQ.  In this 
 *		 case, it may be possible to remedy the situation by rerunning 
 *		 DNSQ with a smaller value of FACTOR. 
 *
 *	   Excessive Number of Function Evaluations.  A reasonable value 
 *		 for MAXFEV is 100*(N+1) for JAC!=NULL and 200*(N+1) for JAC==NULL. 
 *
 *		 If the number of calls to FCN reaches MAXFEV, then this 
 *		 indicates that the routine is converging very slowly as 
 *		 measured by the progress of FVEC, and INFO is set to 2. This 
 *
 *		 situation should be unusual because, as indicated below, lack 
 *		 of good progress is usually diagnosed earlier by DNSQ, 
 *		 causing termination with info = 4 or INFO = 5. 
 *
 *	   Lack of Good Progress.  DNSQ searches for a zero of the system 
 *
 *		 by minimizing the sum of the squares of the functions.  In so 
 *		 doing, it can become trapped in a region where the minimum 
 *		 does not correspond to a zero of the system and, in this 
 *		 situation, the iteration eventually fails to make good 
 *		 progress.  In particular, this will happen if the system does 
 *		 not have a zero.  If the system has a zero, rerunning DNSQ 
 *		 from a different starting point may be helpful. 
 *
 *
 * 6. Characteristics of The Algorithm. 
 *
 *	   DNSQ is a modification of the Powell Hybrid method.  Two of its 
 *	   main characteristics involve the choice of the correction as a 
 *	   convex combination of the Newton and scaled gradient directions, 
 *	   and the updating of the Jacobian by the rank-1 method of 
 *	   Broyden.  The choice of the correction guarantees (under 
 *	   reasonable conditions) global convergence for starting points 
 *	   far from the solution and a fast rate of convergence.  The 
 *	   Jacobian is calculated at the starting point by either the 
 *	   user-supplied subroutine or a forward-difference approximation, 
 *	   but it is not recalculated until the rank-1 method fails to 
 *	   produce satisfactory progress. 
 *
 *	   Timing.  The time required by DNSQ to solve a given problem 
 *		 depends on N, the behavior of the functions, the accuracy 
 *		 requested, and the starting point.  The number of arithmetic 
 *		 operations needed by DNSQ is about 11.5*(N**2) to process 
 *		 each evaluation of the functions (call to FCN) and 1.3*(N**3) 
 *		 to process each evaluation of the Jacobian (call to JAC, 
 *		 if JAC!=NULL).  Unless FCN and JAC can be evaluated quickly, 
 *		 the timing of DNSQ will be strongly influenced by the time 
 *		 spent in FCN and JAC. 
 *
 *	   Storage.  DNSQ requires (3*N**2 + 17*N)/2 single precision 
 *		 storage locations, in addition to the storage required by the 
 *		 program.  There are no internally declared storage arrays. 
 *
 * References:   M. J. D. Powell, A hybrid method for nonlinear equa- 
 *				 tions. In Numerical Methods for Nonlinear Algebraic 
 *				 Equations, P. Rabinowitz, Editor.  Gordon and Breach, 
 *				 1988. 
 *
 * Routines called: D1MPYQ, D1UPDT, DDOGLG, DENORM, DFDJC1, DQFORM, DQRFAC
 *
 * Revision history: (YYMMDD) 
 *   800301  DATE WRITTEN 
 *   890531  Changed all specific intrinsics to generic.  (WRB) 
 *   890831  Modified array declarations.  (WRB) 
 *   890831  REVISION DATE from Version 3.2 
 *   891214  Prologue converted to Version 4.0 format.  (BAB) 
 *   900315  CALLs to XERROR changed to CALLs to XERMSG.  (THJ) 
 *   920501  Reformatted the REFERENCES section.  (WRB) 
 *   960510  Translated to C and features added. (GWG)
 */

/* Returns status. 0 = OK. */
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
	double dtol,	/* Desired delta tollerance of the solution */
	double atol,	/* Desired tollerance of the root (stops on dtol or tol) */
	int maxfev,		/* Set excessive Number of Function Evaluations */
	int ml,			/* number of subdiagonals within the band of the Jacobian matrix. */
	int mu, 		/* number of superdiagonals within the band of the Jacobian matrix. */
	double epsfcn,	/* determines suitable step for forward-difference approximation */
	double diag[],	/* Optional scaling factors, use NULL for internal scaling */
	double factor,	/* Determines the initial step bound */
	double maxstep,	/* Determines the maximum subsequent step bound (0.0 for none) */
					/* maxstep is NOT WORKING !!! ??? */
	int nprint, 	/* Turn on debugging printouts from func() every nprint itterations */
	int *nfev,		/* RETURNs the number of calls to fcn() */ 
	int *njev		/* RETURNs the number of calls to jac() */
) {
	int info = 0;	/* Return status  - invalid argument */
	int smode = 0;	/* Scaling mode, 1 = internal */

	/* Internal working arrays */
	double *fjac = NULL;	/* n by n array which contains the orthogonal matrix Q */
					/* produced by the QR factorization of the final approximate Jacobian. */ 
	int ldfjac = n; /* stride of 2d array */
	double **jjac = NULL;	/* NR style pointers to fjac */

	double *r = NULL;  	/* Array of length (n*(n+1))/2 which contains the upper  */
					/* triangular matrix produced by the QR factorization of the  */
					/* final approximate Jacobian, stored rowwise. */
	double *qtf = NULL;	/* Array of length n which contains the vector (q transpose)*fvec. */

	double *wa1 = NULL; 	/* Four working arrays length n */
	double *wa2 = NULL;
	double *wa3 = NULL;
	double *wa4 = NULL;

	int iwa[1];		/* Pivot swap array (only one element used) */

	/* Local variables */
	bool jeval;
	int iter;
	int i, j, k, l, iflag;
	int qrflag;					/* Set when a valid Q is in fjac[], and R is in r[] */
	int ncsuc;
	int nslow1, nslow2, ncfail;
	double temp;
	double delta = 0.0;
	double ratio, fnorm, pnorm;
	double xnorm = 0.0;
	double fnorm1;
	double actred, prered;
	double sum;

	/* Allocate out working arrays */
	if (diag == NULL) {	/* Internal scaling */
		smode = 1;
		diag = dvector(0,n-1);
	}
	fjac = dvector(0,(n * n)-1);
	jjac = convert_dmatrix(fjac,0,n-1,0,ldfjac-1);
	r    = dvector(0,((n * (n+1))/2)-1);
	qtf  = dvector(0,n-1);
	wa1  = dvector(0,n-1);
	wa2  = dvector(0,n-1);
	wa3  = dvector(0,n-1);
	wa4  = dvector(0,n-1);

	qrflag = 0;
	iflag = 0;
	*nfev = 0;
	*njev = 0;

	/* Check the input parameters for errors. */
	if (n <= 0 || dtol < 0.0 || maxfev <= 0
		|| ml < 0 || mu < 0 || factor <= 0.0 || maxstep < 0.0
	    || (sjac == NULL && startsjac != 0)) {
		goto func_exit;
	}
	if (!smode) {	/* Check the scaling array we were given */
		for (j = 0; j < n; ++j) {
			if (diag[j] <= 0.0) {
				goto func_exit;
			}
		}
	}

	/* Evaluate the function at the starting point */
	/* and calculate its norm. */
	*nfev = 1;
	if ((iflag = (*fcn)(fdata, n, &x[0], &fvec[0], 1)) < 0)
		goto func_exit;
	fnorm = denorm(n, &fvec[0]);

	/* Initialize iteration counter and monitors. */

	iter = 1;
	ncsuc = 0;
	ncfail = 0;
	nslow1 = 0;
	nslow2 = 0;

	/*	Beginning of the outer loop. */
	for (;;) {
		jeval = TRUE;

		/* initialize the jacobian matrix. */
		if (startsjac) {	/* User provided array */
			for (j = 0; j < n; j++) {
				for (i = 0; i < n; i++) 
					fjac[j * ldfjac + i] = sjac[j][i];
			}
		} else if (jac == NULL) { /* Code approximates the jacobian */
			int ti;
			iflag = dfdjc1(fdata, fcn, n, &x[0], &fvec[0], &fjac[0], ldfjac,
				ml, mu, epsfcn, &wa1[0], &wa2[0]);
			ti = ml + mu + 1;
			*nfev += min(ti,n);
		} else { /* User supplies jacobian function */
			iflag = (*jac)(fdata, n, &x[0], &fvec[0], jjac);
			++(*njev);
		}
#ifdef DEBUG
printf("DNSQ: Jacobian initialized\n");
#endif

		if (iflag < 0)
			goto func_exit;

		/* Compute the qr factorization of the jacobian. */
		dqrfac(n, n, &fjac[0], ldfjac, FALSE, iwa, &wa1[0], &wa2[0], &wa3[0]);

		/* On the first iteration and if internal scaling mode set, scale */
		/* according to the norms of the columns of the initial jacobian. */
		/* (wa2[] will contain norms) */
		/* Do this on subsequent itterations too, if a maxstep is set. */
		if (iter == 1 || maxstep > 0.0) {
			if (smode) {
				for (j = 0; j < n; ++j) {
					diag[j] = wa2[j];
					if (wa2[j] == 0.0) {
						diag[j] = 1.0;
					}
				}
			}
	
			/* On the first iteration, calculate the norm of the scaled */
			/* x[], and initialize the step bound delta. */
			for (j = 0; j < n; ++j)
				wa3[j] = diag[j] * x[j];
	
			xnorm = denorm(n, &wa3[0]);
			if (iter == 1) {
				delta = factor * xnorm;
				if (delta == 0.0)
					delta = factor;
#ifdef DEBUG
				printf("Initial Delta = %f\n",delta);
#endif /* DEBUG */
			} else {
				delta = maxstep * xnorm;
				if (delta == 0.0)
					delta = maxstep;
#ifdef DEBUG
				printf("Subsequent Delta = %f\n",delta);
#endif /* DEBUG */
			}
		}

		/* Form (q transpose)*fvec and store in qtf. */
		for (i = 0; i < n; ++i)
			qtf[i] = fvec[i];

		for (j = 0; j < n; ++j) {
			if (fjac[j + j * ldfjac] != 0.0) {
				sum = 0.0;
				for (i = j; i < n; ++i)
					sum += fjac[i + j * ldfjac] * qtf[i];
				temp = -sum / fjac[j + j * ldfjac];
	
				for (i = j; i < n; ++i)
					qtf[i] += fjac[i + j * ldfjac] * temp;
			}
		}

		/* Copy the triangular factor of the qr factorization into r. */
		for (j = 0; j < n; ++j) {
			l = j;
			if (j >= 1) {
				for (i = 0; i < j; ++i) {
					r[l] = fjac[i + j * ldfjac];
					l += (n-1) - i;
				}
			}
			r[l] = wa1[j];
		}

		/* Accumulate the orthogonal factor Q in fjac. */
		dqform(n, n, &fjac[0], ldfjac, &wa1[0]);

		qrflag = 1;

		/* Rescale if necessary. */
		if (smode) {
			for (j = 0; j < n; ++j) {
				diag[j] = max(diag[j],wa2[j]);
			}
		}

		/* Beginning of the inner loop. */
		for (;;) {
			/* If requested, call fcn to enable printing of iterates. */
			if (nprint > 0) {
				if ((iter - 1) % nprint == 0)
					if ((iflag = (*fcn)(fdata, n, &x[0], &fvec[0], 0)) < 0)
						goto func_exit;
			}

#ifdef DEBUG
			/* If the user supplied an array, and there is a valid Q in */
			/* fjac[], and R is in r[], recover the most recent Jacobian */
			/* matrix by multiplying Q by R */
			if (qrflag && sjac) {
				for (i = 0; i < n; ++i)  {
					for (j = 0; j < n; ++j) {
						double temp = 0.0;
						l = j;
						for (k = 0; k <= j; ++k) {
							temp += fjac[k * ldfjac + i] * r[l];
							l += (n-1-k);
						}
						sjac[j][i] = temp;
					}
				}
				printf("Current Jacobian = \n");
				for (j = 0; j < n; ++j) {
					for (i = 0; i < n; ++i)  {
						printf("%f  ",sjac[j][i]);
					}
				printf("\n");
				}
			}
#endif /* DEBUG */

			/* Determine the direction p. */
			ddoglg(n, &r[0], &diag[0], &qtf[0], delta, &wa1[0], &wa2[0], &wa3[0]);

			/* Store the direction p and x + p. calculate the norm of p. */
			/* (wa1[] is X[] output array from ddoglg()) */
			for (j = 0; j < n; ++j) {
				wa1[j] = -wa1[j];
				wa2[j] = x[j] + wa1[j];
				wa3[j] = diag[j] * wa1[j];
			}
			pnorm = denorm(n, &wa3[0]);

			/* On the first iteration, adjust the initial step bound. */
			/* Do this on subsequent itterations too, if maxstep is set. */
			if (iter == 1 || maxstep > 0.0) {
				delta = min(delta,pnorm);
#ifdef DEBUG
				if (iter == 1)
					printf("First itter Delta = %f\n",delta);
				else
					printf("Subsequent itter Delta = %f\n",delta);
#endif /* DEBUG */
			}

			/* Evaluate the function at x + p and calculate its norm. */
			++(*nfev);
			if ((iflag = (*fcn)(fdata, n, &wa2[0], &wa4[0], 1)) < 0)
				goto func_exit;
			fnorm1 = denorm(n, &wa4[0]);

			/* Compute the scaled actual reduction. */
			actred = -1.0;				/* Assume norm is made worse */
			if (fnorm1 < fnorm) {		/* There was a reduction in the norm */
				double td;
				td = fnorm1 / fnorm;
				actred = 1.0 - td * td;
			}

			/* Compute the scaled predicted reduction. */
			l = 0;
			for (i = 0; i < n; ++i) {
				sum = 0.0;
				for (j = i; j < n; ++j) {
					sum += r[l] * wa1[j];
					++l;
				}
				wa3[i] = qtf[i] + sum;
			}

			temp = denorm(n, &wa3[0]);
			prered = 0.0;
			if (temp < fnorm) {
				double td;
				td = temp / fnorm;
				prered = 1.0 - td * td;
			}

			/* Compute the ratio of the actual to the predicted reduction. */
			ratio = 0.0;		/* Assume no improvement */ 
			if (prered > 0.0)
				ratio = actred / prered;
#ifdef DEBUG
printf("DNSQ: actual/predicted ratio = %f\n",ratio);
#endif /* DEBUG */

			/* Update the step bound. */
			if (ratio < 0.1) {
				ncsuc = 0;
				++ncfail;		/* Forces jacobian recalc when ncfail == 2 */
				delta = 0.5 * delta;
#ifdef DEBUG
printf("ratio < 0.1 bad, Delta = %f, ncfail = %d\n",delta,ncfail);
#endif /* DEBUG */
			} else {
				ncfail = 0;		/* Pospone jacobian recalc */
				++ncsuc;
				if (ratio >= 0.5 || ncsuc > 1) {
					delta = max(delta, pnorm / 0.5);
#ifdef DEBUG
printf("ratio > 0.1 good, Delta = %f, ncsuc = %d\n",delta,ncsuc);
#endif /* DEBUG */
				}
				if (fabs(ratio - 1.0) <= 0.1) {
					delta = 2.0 * pnorm;
#ifdef DEBUG
printf("abs(ratio -1.0) <= 0.1 Delta = %f\n",delta);
#endif /* DEBUG */
				}
			}

			/* Test for progressing iteration. */
			if (ratio > 0.0001) {
#ifdef DEBUG
printf("Successful itter\n");
#endif /* DEBUG */
				/* Successful iteration. update x, fvec, and their norms. */
				for (j = 0; j < n; ++j) {
					x[j] = wa2[j];
					wa2[j] = diag[j] * x[j];
					fvec[j] = wa4[j];
				}
				xnorm = denorm(n, &wa2[0]);
				fnorm = fnorm1;
				++iter;
			}

#ifdef DEBUG
printf("DNSQ: actual = %f\n",actred);
#endif /* DEBUG */
			/* Determine the progress of the iteration. */
			++nslow1;
			if (fabs(actred) >= 0.001)
				nslow1 = 0;
			if (jeval)
				++nslow2;
			if (fabs(actred) >= 0.1)
				nslow2 = 0;

			/* Test for convergence. */
			if (delta <= dtol * xnorm || fnorm == 0.0) {
#ifdef DEBUG
printf("DNSQ: delta %f <= dtol * xnorm %f || fnorm == %f\n",delta,dtol * xnorm,fnorm);
#endif /* DEBUG */
				info = 1;
				goto func_exit;
			}
			/* Test for root meeting tolerance (GWG) */
			for (j = 0; j < n; ++j) {
				if (fabs(fvec[j]) > atol)
					break;
			}
			if (j >= n) {		/* All were below tollerance */
#ifdef DEBUG
printf("DNSQ: fvecs are all <= atol %f\n",atol);
#endif /* DEBUG */
				info = 1;
				goto func_exit;
			}

			/* Tests for termination and stringent tolerances. */
			if (*nfev >= maxfev)
				info = 2;
			if (0.1 * max(0.1 * delta, pnorm) <= M_DIVER * xnorm)
				info = 3;
			if (nslow2 == 5)
				info = 4;
			if (nslow1 == 10)
				info = 5;
			if (info != 0)
				goto func_exit;

			/* Criterion for recalculating jacobian */
			if (ncfail == 2) {
				break;	/* Break out of inner loop */
			}

			/* Calculate the rank one modification to the jacobian */
			/* and update qtf if necessary. */
			for (j = 0; j < n; ++j) {
				sum = 0.0;
				for (i = 0; i < n; ++i) {
					sum += fjac[i + j * ldfjac] * wa4[i];
				}
				wa2[j] = (sum - wa3[j]) / pnorm;
				wa1[j] = diag[j] * (diag[j] * wa1[j] / pnorm);
				if (ratio >= 1e-4) {
					qtf[j] = sum;
				}
			}

			/* Compute the qr factorization of the updated jacobian. */
			d1updt(n, n, &r[0], &wa1[0], &wa2[0], &wa3[0]);
			d1mpyq(n, n, &fjac[0], ldfjac, &wa2[0], &wa3[0]);
			d1mpyq(1, n, &qtf[0], 1, &wa2[0], &wa3[0]);

			jeval = FALSE;
		} /* End of the inner loop. */
	} /* End of the outer loop. */


	/* Termination, either normal or user imposed. */
func_exit:

	/* If the user supplied an array, and there is a valid Q in */
	/* fjac[], and R is in r[], recover the most recent Jacobian */
	/* matrix by multiplying Q by R */
	if (qrflag && sjac) {
		for (i = 0; i < n; ++i)  {
			for (j = 0; j < n; ++j) {
				double temp = 0.0;
				l = j;
				for (k = 0; k <= j; ++k) {
					temp += fjac[k * ldfjac + i] * r[l];
					l += (n-1-k);
				}
				sjac[j][i] = temp;
			}
		}
	}

	/* Free our working arrays */
	if (smode)
		free_dvector(diag,0,n-1);
	free_dvector(fjac,0,(n * n)-1);
	free_convert_dmatrix(jjac,0,n-1,0,ldfjac-1);
	free_dvector(r,0,((n * (n+1))/2)-1);
	free_dvector(qtf,0,n-1);
	free_dvector(wa1,0,n-1);
	free_dvector(wa2,0,n-1);
	free_dvector(wa3,0,n-1);
	free_dvector(wa4,0,n-1);


	if (iflag < 0)
		info = iflag;
	if (nprint > 0)
		(*fcn)(fdata, n, &x[0], &fvec[0], 0);
#ifdef DEBUG
	if (info < 0)
		printf("dnsq: Execution terminated because user set iflag negative\n");
	if (info == 0)
		printf("dnsq: Invalid input parameter\n");
	if (info == 2)
		printf("dnsq: Too many function evaluations\n");
	if (info == 3)
		printf("dnsq: dtol too small. no further improvement possible\n");
	if (info > 4)
		printf("dnsq: Iteration not making good progress\n");
#endif /* DEBUG */
	return info;
} /* dnsq */

/***************************************************************/
/***************************************************************/

/*
 *	 Given an M by N matrix A, this subroutine computes A*Q where 
 *	 Q is the product of 2*(N - 1) transformations                
 *                                                                  
 *		   GV(N-1)*...*GV(1)*GW(1)*...*GW(N-1) 
 *                                                                  
 *	 and GV(I), GW(I) are Givens rotations in the (I,N) plane which 
 *	 eliminate elements in the I-th and N-th planes, respectively. 
 *	 Q itself is not given, rather the information to recover the 
 *	 GV, GW rotations is supplied. 
 *                                                                  
 */

static int d1mpyq(
	int m,		/* Number of rows of A */
	int n,		/* Number of columns of A */
	double *a,	/* M by N array */
	int lda,	/* stride for a[][] */
    double *v,	/* Input array */
	double *w)	/* Input array */
{
	/* Local variables */
	int i, j;
	int nm1 = n - 1;
	double temp;
	double cos_ = 0.0, sin_ = 0.0;

	/* Apply the first set of givens rotations to a. */
	if (nm1 >= 1) {
		for (j = n-2; j >= 0; --j) {
			if (fabs(v[j]) > 1.0) 
				cos_ = 1.0 / v[j];
			if (fabs(v[j]) > 1.0) { /* Computing 2nd power */
				sin_ = sqrt(1.0 - cos_ * cos_);
			}
			if (fabs(v[j]) <= 1.0) {
				sin_ = v[j];
			}
			if (fabs(v[j]) <= 1.0) { /* Computing 2nd power */
				cos_ = sqrt(1.0 - sin_ * sin_);
			}
			for (i = 0; i < m; ++i) {
				temp = cos_ * a[i + j * lda] - sin_ * a[i + nm1 * lda];
				a[i + nm1 * lda] = sin_ * a[i + j * lda] + cos_ * a[i + nm1 * lda];
				a[i + j * lda] = temp;
			}
		}

		/* Apply the second set of givens rotations to a. */
		for (j = 0; j < nm1; ++j) {
			if (fabs(w[j]) > 1.0) 
				cos_ = 1.0 / w[j];
			if (fabs(w[j]) > 1.0) { /* Computing 2nd power */
				sin_ = sqrt(1.0 - cos_ * cos_);
			}
			if (fabs(w[j]) <= 1.0)
				sin_ = w[j];
			if (fabs(w[j]) <= 1.0) { /* Computing 2nd power */
				cos_ = sqrt(1.0 - sin_ * sin_);
			}
			for (i = 0; i < m; ++i) {
				temp = cos_ * a[i + j * lda] + sin_ * a[i + nm1 * lda];
				a[i + nm1 * lda] = -sin_ * a[i + j * lda] + cos_ * a[i + nm1 * lda];
				a[i + j * lda] = temp;
			}
		}
	}
	return 0;
}

/***************************************************************/
/***************************************************************/

/*
 *	 Given an M by N lower trapezoidal matrix S, an M-vector U, 
 *	 and an N-vector V, the problem is to determine an 
 *	 orthogonal matrix Q such that 
 *
 *				   t 
 *		   (S + U*V )*Q 
 *
 *	 is again lower trapezoidal. 
 *
 *	 This subroutine determines Q as the product of 2*(N - 1) 
 *	 transformations 
 *
 *		   GV(N-1)*...*GV(1)*GW(1)*...*GW(N-1) 
 *
 *	 where GV(I), GW(I) are Givens rotations in the (I,N) plane 
 *	 which eliminate elements in the I-th and N-th planes, 
 *	 respectively. Q itself is not accumulated, rather the 
 *	 information to recover the GV, GW rotations is returned. 
 *
 */

static int d1updt(
	int m,			/* Number of rows of S */
	int n,			/* Number of columns of S */
	double *s,		/* Array of length (n*(2*m-n+1))/2 containing the lower trapezoid matrix */
	double *u,		/* U vector lenghth m */
	double *v,		/* V vector length n */
	double *w)		/* Output array W length m */
{
	int i, j, l;
	int jj;
	int nm1 = n - 1;
	int nmj;
	double temp;
	double giant, cotan;
	double tan_;
	double cos_, sin_, tau;

	/* Giant is the largest magnitude. */
	giant = M_LARGE;

	/* Initialize the diagonal element pointer. */
	jj = n * ((m << 1) - n + 1) / 2 - (m - n) - 1;

	/* Move the nontrivial part of the last column of s into w. */
	l = jj;
	for (i = nm1; i < m; ++i) {
		w[i] = s[l];
		++l;
	}

	/* Rotate the vector v into a multiple of the n-th unit vector */
	/* in such a way that a spike is introduced into w. */
	if (nm1 >= 1) {
		for (nmj = 1; nmj <= nm1; ++nmj) {
			j = n - nmj - 1;
			jj -= m - j;
			w[j] = 0.0;
			if (v[j] == 0.0)
				continue;

			/* Determine a givens rotation which eliminates the */
			/* j-th element of v. */
			if (fabs(v[nm1]) < fabs(v[j])) {
				cotan = v[nm1] / v[j];
				sin_ = 0.5 / sqrt(0.25 + 0.25 * (cotan * cotan));
				cos_ = sin_ * cotan;
				tau = 1.0;
				if (fabs(cos_) * giant > 1.0) {
					tau = 1.0 / cos_;
				}
			} else {
				tan_ = v[j] / v[nm1];
				cos_ = 0.5 / sqrt(0.25 + 0.25 * (tan_ * tan_));
				sin_ = cos_ * tan_;
				tau = sin_;
			}

			/* Apply the transformation to v and store the information */
			/* necessary to recover the givens rotation. */
			v[nm1] = sin_ * v[j] + cos_ * v[nm1];
			v[j] = tau;

			/* Apply the transformation to s and extend the spike in w. */
			l = jj;
			for (i = j; i < m; ++i) {
				temp = cos_ * s[l] - sin_ * w[i];
				w[i] = sin_ * s[l] + cos_ * w[i];
				s[l] = temp;
				++l;
			}
		}
	}

	/* Add the spike from the rank 1 update to w. */
	for (i = 0; i < m; ++i)
		w[i] += v[nm1] * u[i];

	/* Eliminate the spike. */
	if (nm1 >= 1) {
		for (j = 0; j < nm1; ++j) {
			if (w[j] != 0.0) {
				/* Determine a givens rotation which eliminates the */
				/* j-th element of the spike. */
				if (fabs(s[jj]) < fabs(w[j])) {
					cotan = s[jj] / w[j];
					sin_ = 0.5 / sqrt(0.25 + 0.25 * (cotan * cotan));
					cos_ = sin_ * cotan;
					tau = 1.0;
					if (fabs(cos_) * giant > 1.0) {
						tau = 1.0 / cos_;
					}
				} else {
					tan_ = w[j] / s[jj];
					cos_ = 0.5 / sqrt(0.25 + 0.25 * (tan_ * tan_));
					sin_ = cos_ * tan_;
					tau = sin_;
				}

				/* Apply the transformation to s and reduce the spike in w. */
				l = jj;
				for (i = j; i < m; ++i) {
					temp = cos_ * s[l] + sin_ * w[i];
					w[i] = -sin_ * s[l] + cos_ * w[i];
					s[l] = temp;
					++l;
				}

				/* Store the information necessary to recover the givens rotation. */
				w[j] = tau;
			}
			jj += m - j;
		}
	}

	/* Move w back into the last column of the output s. */
	l = jj;
	for (i = nm1; i < m; ++i) {
		s[l] = w[i];
		++l;
	}
	return 0;
}

/***************************************************************/
/***************************************************************/

/*
 *	 Given an M by N matrix A, an N by N nonsingular diagonal 
 *	 matrix D, an M-vector B, and a positive number DELTA, the 
 *	 problem is to determine the convex combination X of the 
 *	 Gauss-Newton and scaled gradient directions that minimizes 
 *	 (A*X - B) in the least squares sense, subject to the 
 *	 restriction that the Euclidean norm of D*X be at most DELTA. 
 *
 *	 This subroutine completes the solution of the problem 
 *	 if it is provided with the necessary information from the 
 *	 QR factorization of A. That is, if A = Q*R, where Q has 
 *	 orthogonal columns and R is an upper triangular matrix, 
 *	 then DDOGLG expects the full upper triangle of R and 
 *	 the first N components of (Q transpose)*B. 
 *
 */

static int ddoglg(
	int n,			/* Order of r[] */
	double r[],		/* Array of length (n*(n+!))/2 containing upper triangular matrix */
	double diag[],	/* Array of length n containing the diagonal elements of the matrix d[] */
	double qtb[],	/* Array of length n containing first n elements of vector (Q transpose)*B */
	double delta,	/* Upper bound on the Euclidean norm of D*X  */
	double x[],		/* output array of length N containing the desired direction */
	double wa1[],	/* Working arrays length n */
	double wa2[])
{
	int i, j, k, l;
	int jj;
	int jp1;
	int nm1 = n-1;
	double temp;
	double alpha, gnorm, qnorm;
	double epsmch;
	double sgnorm;
	double sum;

	/* Epsmch is the machine precision. */
	epsmch = M_DIVER;

	/* First, calculate the gauss-newton direction. */
	jj = n * (n + 1) / 2;
	for (k = 0; k < n; ++k) {
		j = nm1 - k;
		jp1 = j + 1;
		jj -= (k+1);
		l = jj + 1;
		sum = 0.0;
		if (n >= (jp1+1)) {
			for (i = jp1; i < n; ++i) {
				sum += r[l] * x[i];
				++l;
			}
		}

		temp = r[jj];
		if (temp == 0.0) {
			l = j;
			for (i = 0; i <= j; ++i) { /* Computing MAX */
				double dt;
				dt = fabs(r[l]);
				temp = max(temp,dt);
				l += nm1 - i;
			}
			temp = epsmch * temp;
			if (temp == 0.0) {
				temp = epsmch;
			}
		}
		x[j] = (qtb[j] - sum) / temp;
	}


	/* Test whether the gauss-newton direction is acceptable. */
	for (j = 0; j < n; ++j) {
		wa1[j] = 0.0;
		wa2[j] = diag[j] * x[j];
	}
	qnorm = denorm(n, &wa2[0]);
	if (qnorm <= delta) {
		return 0;		/* Done - use this direction */
	}

	/* The gauss-newton direction is not acceptable. */
	/* Next, calculate the scaled gradient direction. */
	l = 0;
	for (j = 0; j < n; ++j) {
		temp = qtb[j];
		for (i = j; i < n; ++i) {
			wa1[i] += r[l] * temp;
			++l;
		}
		wa1[j] /= diag[j];
	}

	/* Calculate the norm of the scaled gradient and test for */
	/* The special case in which the scaled gradient is zero. */
	gnorm = denorm(n, &wa1[0]);
	sgnorm = 0.0;
	alpha = delta / qnorm;
	if (gnorm != 0.0) {
		/* Calculate the point along the scaled gradient */
		/* at which the quadratic is minimized. */
		for (j = 0; j < n; ++j)
			wa1[j] = wa1[j] / gnorm / diag[j];
		l = 0;
		for (j = 0; j < n; ++j) {
			sum = 0.0;
			for (i = j; i < n; ++i) {
				sum += r[l] * wa1[i];
				++l;
			}
			wa2[j] = sum;
		}
		temp = denorm(n, &wa2[0]);
		sgnorm = gnorm / temp / temp;

		/* Test whether the scaled gradient direction is acceptable. */
		alpha = 0.0;
		if (sgnorm < delta) {
			double d0,d1,d2,d3,d4,
			/* The scaled gradient direction is not acceptable. */
			/* Finally, calculate the point along the dogleg */
			/* at which the quadratic is minimized. */
			bnorm = denorm(n, &qtb[0]);
			d0 = bnorm / gnorm * (bnorm / qnorm) * (sgnorm / delta);
			d1 = sgnorm / delta;
			d2 = d0 - delta / qnorm;
			d3 = delta / qnorm;
			d4 = sgnorm / delta;
			d0 = d0 - delta / qnorm * (d1 * d1)
			   + sqrt(d2 * d2 + (1.0 - d3 * d3) * (1.0 - d4 * d4));
			d1 = sgnorm / delta;
			alpha = delta / qnorm * (1.0 - d1 * d1) / d0;
		}
	}

	/* Form appropriate convex combination of the gauss-newton */
	/* direction and the scaled gradient direction. */
	temp = (1.0 - alpha) * min(sgnorm,delta);
	for (j = 0; j < n; ++j)
		x[j] = temp * wa1[j] + alpha * x[j];

	return 0;
} /* ddoglg */


/***************************************************************/
/***************************************************************/

/*
 *	 Given an N-vector X, this function calculates the 
 *	 Euclidean norm of X. 
 *
 *	 The Euclidean norm is computed by accumulating the sum of 
 *	 squares in three different sums. The sums of squares for the 
 *	 small and large components are scaled so that no overflows 
 *	 occur. Non-destructive underflows are permitted. Underflows 
 *	 and overflows do not occur in the computation of the unscaled 
 *	 sum of squares for the intermediate components. 
 *	 The definitions of small, intermediate and large components 
 *	 depend on two constants, RDWARF and RGIANT. The main 
 *	 restrictions on these constants are that RDWARF**2 not 
 *	 underflow and RGIANT**2 not overflow. The constants 
 *	 given here are suitable for every known computer. 
 *
 */

static double denorm(
	int n,			/* Size of x[] */
	double x[])		/* Input vector */
{
	if (n < 8) {	/* Make it simple and fast */
		double ss = 0.0;
		switch (n) {
			case 8:
				ss += x[7] * x[7];
			case 7:
				ss += x[6] * x[6];
			case 6:
				ss += x[5] * x[5];
			case 5:
				ss += x[4] * x[4];
			case 4:
				ss += x[3] * x[3];
			case 3:
				ss += x[2] * x[2];
			case 2:
				ss += x[1] * x[1];
			case 1:
				ss += x[0] * x[0];
		}
		return sqrt(ss);
	} else {
		/* Initialized data */
		static double rdwarf = 3.834e-20;
		static double rgiant = 1.304e19;
	
		/* Local variables */
		static double xabs, x1max, x3max;
		static int i;
		static double s1, s2, s3, agiant, floatn;
		double ret_val, td;
	
		s1 = 0.0;	/* Large component */
		s2 = 0.0;	/* Intermedate component */
		s3 = 0.0;	/* Small component */
		x1max = 0.0;
		x3max = 0.0;
		floatn = (double) (n + 1);
		agiant = rgiant / floatn;
		for (i = 0; i < n; i++) {
			xabs = (td = x[i], fabs(td));
	
			/* Sum for intermediate components. */
			if (xabs > rdwarf && xabs < agiant) {
				td = xabs;				 	/* Computing 2nd power */
				s2 += td * td;
	
			/* Sum for small components. */
			} else if (xabs <= rdwarf) {
				if (xabs <= x3max) {
					if (xabs != 0.0) {		/* Computing 2nd power */
					td = xabs / x3max;
					s3 += td * td;
					}
				} else { /* Computing 2nd power */
					td = x3max / xabs;
					s3 = 1.0 + s3 * (td * td);
					x3max = xabs;
				}
	
			/* Sum for large components. */
			} else {
				if (xabs <= x1max) {		/* Computing 2nd power */
					td = xabs / x1max;
					s1 += td * td;
				} else {					/* Computing 2nd power */
					td = x1max / xabs;
					s1 = 1.0 + s1 * (td * td);
					x1max = xabs;
				}
			}
		}
	
		/* Calculation of norm. */
		if (s1 != 0.0) {		/* Large is present */
			ret_val = x1max * sqrt(s1 + s2 / x1max / x1max);
		} else {				/* Medium and small are present */
			if (s2 == 0.0) {
				ret_val = x3max * sqrt(s3);		/* Small only */
			} else {
				if (s2 >= x3max) {		/* Medium larger than small */
					ret_val = sqrt(s2 * (1.0 + x3max / s2 * (x3max * s3)));
				} else {				/* Small large than medium */
					ret_val = sqrt(x3max * (s2 / x3max + x3max * s3));
				}
			}
		}
		return ret_val;
	}
}

/***************************************************************/
/***************************************************************/

/*
 *	 This subroutine computes a forward-difference approximation 
 *	 to the N by N Jacobian matrix associated with a specified 
 *	 problem of N functions in N variables. If the Jacobian has 
 *	 a banded form, then function evaluations are saved by only 
 *	 approximating the nonzero terms. 
 *
 */

static int dfdjc1(	/* Return < 0 if fcn() aborts */
	void *fdata,	/* Opaque data pointer to pass to fcn() */
	int (*fcn)(void *fdata, int n, double *x, double *fvec, int iflag),
					/* Pointer to function we are solving */
	int n,			/* Number of functions and variables  */
	double x[],		/* Input array size n */
	double fvec[],	/* array of length n which must contain the functions evaluated at x[]  */
	double fjac[],	/* output n by n array containing approximation to the Jacobian matrix at x[] */
	int ldfjac,		/* stride of fjac[] */
	int ml, 		/* Number of subdiagonals within the band of the Jacobian matrix */
	int mu, 		/* Number of superdiagonals within the band of the Jacobian matrix */
	double epsfcn,	/* Step length for the forward-difference approximation */
	double *wa1,	/* Working arrays of length n */
	double *wa2)
{
	/* Local variables */
	int iflag = 0;
	double temp;
	int msum;
	double h;
	int i, j, k;
	double eps;
	int nm1 = n-1;

	/* M_DIVER is the machine precision. */
	eps = sqrt((max(epsfcn,M_DIVER)));
	msum = ml + mu + 1;
	if (msum >= n) {
		/* Computation of dense approximate jacobian. */
		for (j = 0; j < n; ++j) {
			temp = x[j];
			h = eps * fabs(temp);
			if (h == 0.0)
				h = eps;
			x[j] = temp + h;
			if ((iflag = (*fcn)(fdata,n, &x[0], &wa1[0], 1)) < 0)
				break;
			x[j] = temp;
			for (i = 0; i < n; ++i)
				fjac[i + j * ldfjac] = (wa1[i] - fvec[i]) / h;
		}
	} else {
		/* Computation of banded approximate jacobian. */
		for (k = 0; k < msum; ++k) {
			for (j = k; msum < 0 ? j >= nm1 : j <= nm1; j += msum) {
				wa2[j] = x[j];
				h = eps * fabs(wa2[j]);
				if (h == 0.0)
					h = eps;
				x[j] = wa2[j] + h;
			}
			if ((iflag = (*fcn)(fdata, n, &x[0], &wa1[0], 1)) < 0)
				break;
			for (j = k; msum < 0 ? j >= nm1 : j <= nm1; j += msum) {
				x[j] = wa2[j];
				h = eps * fabs(wa2[j]);
				if (h == 0.0)
					h = eps;
				for (i = 0; i < n; ++i) {
					fjac[i + j * ldfjac] = 0.0;
					if (i >= j - mu && i <= j + ml)
						fjac[i + j * ldfjac] = (wa1[i] - fvec[i]) / h;
				}
			}
		}
	}
	return iflag;
} /* dfdjc1_ */

/***************************************************************/
/***************************************************************/

/*
 *	 This subroutine proceeds from the computed QR factorization of 
 *	 an M by N matrix A to accumulate the M by M orthogonal matrix 
 *	 Q from its factored form. 
 *
 */

static int dqform(
	int m,		/* No of rows of A and the order of Q. */
	int n, 		/* No of columns of A.  */
	double *q,	/* m by m array */
	int ldq,	/* stride of q[][] */
	double *wa)	/* Working aray length m */
{
	int i, j, k, l, minmn;
	double sum;

	/* Zero out upper triangle of q in the first min(m,n) columns. */
	minmn = min(m,n);
	if (minmn >= 2) {
		for (j = 1; j < minmn; ++j) {
			for (i = 0; i < j; ++i)
				q[i + j * ldq] = 0.0;
		}
	}

	/* Initialize remaining columns to those of the identity matrix. */
	if (m > n) {
		for (j = n; j < m; ++j) {
			for (i = 0; i < m; ++i) {
				q[i + j * ldq] = 0.0;
			}
			q[j + j * ldq] = 1.0;
		}
	}

	/* Accumulate q from its factored form. */
	for (l = 0; l < minmn; ++l) {
		k = minmn - l - 1;
		for (i = k; i < m; ++i) {
			wa[i] = q[i + k * ldq];
			q[i + k * ldq] = 0.0;
		}
		q[k + k * ldq] = 1.0;
		if (wa[k] != 0.0) {
			for (j = k; j < m; ++j) {
				double temp;
				sum = 0.0;
				for (i = k; i < m; ++i)
					sum += q[i + j * ldq] * wa[i];
				temp = sum / wa[k];
				for (i = k; i < m; ++i)
					q[i + j * ldq] -= temp * wa[i];
			}
		}
	}
	return 0;
} /* dqform_ */

/***************************************************************/
/***************************************************************/

/*
 *	 This subroutine uses Householder transformations with column 
 *	 pivoting (optional) to compute a QR factorization of the 
 *	 M by N matrix A. That is, DQRFAC determines an orthogonal 
 *	 matrix Q, a permutation matrix P, and an upper trapezoidal 
 *	 matrix R with diagonal elements of nonincreasing magnitude, 
 *	 such that A*P = Q*R. The Householder transformation for 
 *	 column K, K = 1,2,...,MIN(M,N), is of the form 
 *
 *						   T 
 *		   I - (1/U(K))*U*U 
 *
 *	 where U has zeros in the first K-1 positions. The form of 
 *	 this transformation and the method of pivoting first 
 *	 appeared in the corresponding LINPACK subroutine. 
 *
 */

static int dqrfac(
	int m,		/* Number of rows of a[] */
	int n,		/* Number of columns of a[] */
	double *a,	/* m by n array */
	int lda,	/* stride of a[][] */
	bool pivot,	/* TRUE to enforce column pivoting */
	int *ipvt,	/* Pivot output array, size n */
	double *sigma,	/* Output diagonal elements of R, length n */
	double *acnorm,	/* Output norms of A, length n */
	double *wa)		/* Working array size n */
{
	/* Local variables */
	int kmax;
	int i, j, k, minmn;
	double ajnorm;
	int jp1;
	double sum;

	/* Compute the initial column norms and initialize several arrays. */
	for (j = 0; j < n; ++j) {
		acnorm[j] = denorm(m, &a[j * lda]);
		sigma[j] = acnorm[j];
		wa[j] = sigma[j];
		if (pivot)
			ipvt[j] = j;
	}

	/* Reduce a to r with householder transformations. */
	minmn = min(m,n);
	for (j = 0; j < minmn; ++j) {
		if (pivot) {
			/* Bring the column of largest norm into the pivot position. */
			kmax = j;
			for (k = j; k < n; ++k) {
				if (sigma[k] > sigma[kmax]) {
					kmax = k;
				}
			}
			if (kmax != j) {
				for (i = 0; i < m; ++i) {
					double temp;
					temp = a[i + j * lda];
					a[i + j * lda] = a[i + kmax * lda];
					a[i + kmax * lda] = temp;
				}
				sigma[kmax] = sigma[j];
				wa[kmax] = wa[j];
				k = ipvt[j];
				ipvt[j] = ipvt[kmax];
				ipvt[kmax] = k;
			}
		}

		/* Compute the householder transformation to reduce the */
		/* j-th column of a to a multiple of the j-th unit vector. */
		ajnorm = denorm(m - j, &a[j + j * lda]);
		if (ajnorm != 0.0) {
			if (a[j + j * lda] < 0.0)
				ajnorm = -ajnorm;
			for (i = j; i < m; ++i)
				a[i + j * lda] /= ajnorm;
			a[j + j * lda] += 1.0;

			/* Apply the transformation to the remaining columns */
			/* and update the norms. */
			jp1 = j + 1;
			if (n > jp1) {
				for (k = jp1; k < n; ++k) {
					double temp;
					sum = 0.0;
					for (i = j; i < m; ++i)
						sum += a[i + j * lda] * a[i + k * lda];
					temp = sum / a[j + j * lda];
					for (i = j; i < m; ++i)
						a[i + k * lda] -= temp * a[i + j * lda];
					if (pivot && sigma[k] != 0.0) {
						temp = a[j + k * lda] / sigma[k];
						temp = 1.0 - temp * temp;
						sigma[k] *= sqrt((max(0.0,temp)));
						temp = sigma[k] / wa[k];
						if (0.05 * (temp * temp) <= M_DIVER) {
							sigma[k] = denorm(m - jp1, &a[jp1 + k * lda]);
							wa[k] = sigma[k];
						}
					}
				}
			}
		}
		sigma[j] = -ajnorm;
	}
	return 0;
} /* dqrfac_ */

/***************************************************************/
/***************************************************************/

