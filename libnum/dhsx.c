
/* This is better for stocastic optimisation, where the function */
/* being evaluated may have a random component, or is not smooth. */

/*
 * Copyright 1999 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* A general purpose downhill simplex multivariate optimser, */
/* based on the Nelder and Mead algorithm. */
/* Code is an original expression of the algorithms decsribed in */
/* "Numerical Recipes in C", by W.H.Press, B.P.Flannery, */
/* S.A.Teukolsky & W.T.Vetterling. */

#include "numsup.h"

#undef DEBUG

static void simplexinit(int di, double *cp, double *r,double **p);
static double trypoint(int di,double *cp, double **p, double *y, int hix, double hpfac,
                double (*funk)(void *fdata, double *tp), void *fdata, double *tryp);

#ifdef NEVER	/* Experimental */

#define ALPHA 0.7			/* Extrapolate hight point through oposite face factor */
#define GAMMA 1.4			/* Aditional extrapolation if ALPHA is good */ 
#define BETA 0.4			/* One dimensional contraction factor */
#define NONEXP 2			/* non expanding passes */

#else			/* Standard tuning values */

#define ALPHA 1.0			/* Extrapolate hight point through oposite face factor */
#define GAMMA 2.0			/* Aditional extrapolation if ALPHA is good */ 
#define BETA 0.5			/* One dimensional contraction factor */
#define NONEXP 2			/* non expanding passes */

#endif


/* Down hill simplex function */
/* return 0 on sucess, 1 on failure due to excessive itterations */
/* Result will be in cp */
/* Arrays start at 0 */
int dhsx(
double *rv,				/* If not NULL, return the residual error */
int di,					/* Dimentionality */
double *cp,				/* Initial starting point */
double *s,				/* Size of initial search area */
double ftol,			/* Finishing tollerance of error change */
double atol,			/* Absolute return value tollerance */
int maxit,				/* Maximum iterations allowed */
double (*funk)(void *fdata, double *tp),		/* Error function to evaluate */
void *fdata				/* Data needed by function */
) {
	int i, j;
	int lox, hix, nhix;	/* Lowest point index, highest point, next highest point */
	int nsp = di+1;		/* Number of simplex verticy points */
	int nit;			/* Number of iterations */
	double tryy, ysave;
	double tol;
	double **p;			/* Simplex array */
	double *y;			/* values of func at verticies */
	double *tryp;		/* Temporary used by trypoint() */

	/* Allocate array arrays */
	y = dvector(0, di);				/* Value of function at verticies */
	tryp = dvector(0, di-1);		
	p = dmatrix(0, di+1, 0, di);	/* Vertex array of dimentions */
	
	/* Init the search simplex */
	simplexinit(di, cp, s, p);

	/* Compute current center point position */
	for (j = 0; j < di; j++) {	/* For all dimensions */
		double sum;
		for (i = 0, sum = 0.0; i < nsp; i++)	/* For all verticies */
			sum += p[i][j];
		cp[j] = sum;
	}

	/* Compute initial y (function) values at verticies */
	for (i = 0; i < nsp; i++)		/* For all verticies */
		y[i] = (*funk)(fdata,p[i]);	/* Compute error function */

	/* Untill we find a solution or give up */
	for (nit = 0; nit < maxit; nit++) {
		/* Find highest, next highest and lowest vertex */

		lox = nhix = hix = 0;
		for (i = 0; i < nsp; i++) {
			if (y[i] < y[lox]) 
				lox = i;
			if (y[i] > y[hix]) {
				nhix = hix;
				hix = i;
			} else if (y[i] > y[nhix]) {
				nhix = i;
			}
		}

		tol = y[hix] - y[lox];

#ifdef DEBUG	/* 2D */
		printf("Current vs = %f,%f  %f,%f  %f,%f\n",
		        p[0].c[0],p[0].c[1],p[1].c[0],p[1].c[1],p[2].c[0],p[2].c[1]);
		printf("Current errs = %e    %e    %e\n",y[0],y[1],y[2]);
		printf("Current sxs = %d    %d    %d\n",sy[0]->no,sy[1]->no,sy[2]->no);
		printf("Current y[hix] = %e\n",y[hix]);
#endif /* DEBUG */

		if (tol < ftol && y[lox] < atol) {	/* Found an adequate solution */
							/* (allow 10 x range for disambiguator) */
			/* set cp[] to best point, and return error value of that point */
			tryy = 1.0/(di+1);
			for (j = 0; j < di; j++)		/* For all dimensions */
				cp[j] *= tryy;				/* Set cp to center point of simplex */
#ifdef DEBUG
			printf("C point = %f,%f\n",cp[0],cp[1]);
#endif
			tryy = (*funk)(fdata,cp);		/* Compute error function */

			if (tryy > y[lox]) {			/* Center point is not the best */
				tryy = y[lox];
				for (j = 0; j < di; j++)
					cp[j] = p[lox][j];
			}
			free_dmatrix(p, 0, di+1, 0, di);
			free_dvector(tryp, 0, di-1);
			free_dvector(y, 0, di);
printf("~1 itterations = %d\n",nit);
			if (rv != NULL)
				*rv = tryy;
			return 0;
		}

		if (nit > NONEXP) {	/* Only try expanding after a couple of iterations */
			/* Try moving the high point through the oposite face by ALPHA */
#ifdef DEBUG
			printf("dhsx: try moving high point %d through oposite face",hix);
#endif
			tryy = trypoint(di, cp, p, y, hix, -ALPHA, funk, fdata, tryp);
		}

		if (nit > NONEXP && tryy <= y[lox]) {
#ifdef DEBUG
			verbose(4,"dhsx: moving high through oposite face worked");
#endif
			/* Gave good result, so continue on in that direction */
			tryy=trypoint(di,cp,p,y,hix,GAMMA,funk,fdata,tryp);


		} else if (nit <= NONEXP || tryy >= y[nhix]) {

			/* else if ALPHA move made things worse, do a one dimensional */
			/* contraction by a factor BETA */
#ifdef DEBUG
			verbose(4,"dhsx: else try moving contracting point %d, y[ini] = %f",hix,y[hix]);
#endif
			ysave = y[hix];
			tryy = trypoint(di, cp, p, y, hix, BETA, funk, fdata, tryp);
			
			if (tryy >= ysave) {
#ifdef DEBUG
				verbose(4,"dhsx: contracting didn't work, try contracting other points to low");
#endif
				/* That still didn't help us, so move all the */
				/* other points towards the high point */
				for (i = 0; i < nsp; i++) {	/* For all verts except low */
					if (i != lox) {
						for (j = 0; j < di; j++) {	/* For all dimensions */
							cp[j] = 0.5 * (p[i][j] + p[lox][j]);
							p[i][j] = cp[j];
						}
						/* Compute function value for new point */
						y[i] = (*funk)(fdata,p[i]);		/* Compute error function */
					}
				}
				/* Re-compute current center point value */
				for (j = 0; j < di; j++) {
					double sum;
					for (i = 0,sum = 0.0;i<nsp;i++)
						sum += p[i][j];
					cp[j] = sum;
				}
			} else {
#ifdef DEBUG
				verbose(4,"dhsx: contracting point %d worked, tryy = %e, ysave = %e",hix,tryy,ysave);
#endif
			}
		}
	}
	free_dmatrix(p, 0, di+1, 0, di);
	free_dvector(tryp, 0, di-1);
	free_dvector(y, 0, di);
	if (rv != NULL)
		*rv = tryy;
	return 1;	/* Failed */
}

/* Try moving the high point through the opposite face */
/* by a factor of fac, and replaces the high point if */
/* that proves to be better.                          */
static double trypoint(
int di,					/* Dimentionality */
double *cp,				/* nsp * center coord/Returned coordinate */
double **p,				/* Starting/Current simplex (modified by dhsx) */
double *y,				/* values of func at verticies */
int hix,				/* Index of high point we are moving */
double hpfac,			/* factor to move high point */
double (*funk)(void *fdata, double tp[]),		/* Error function to evaluate */
void *fdata,			/* Data needed by function */
double *tryp			/* temporary array of size di-1 */
) {
	int j;
	double tt, tryy;

	/* Compute trial high point */
	tt = (1.0 - hpfac)/di;
	for (j = 0; j < di; j++) 
		tryp[j] = cp[j] * tt - p[hix][j] * (tt - hpfac);

	/* Evaluate trial point */
	tryy = (*funk)(fdata, tryp);	/* Compute error function */

	/* If new high point pos. is better */
	if (tryy < y[hix]) {
#ifdef DEBUG
		printf("Try gave improved %e from sx %d",tryy,lsxp->last_fwd->no);
#endif
		y[hix] = tryy;		/* Replace func val of hi with trial */

		for (j = 0; j < di; j++) {
			cp[j] += tryp[j] - p[hix][j];	/* Recompute cp */
			p[hix][j] = tryp[j];	/* Replace co-ords of hi with trial */
		}
	} else {
#ifdef DEBUG
		verbose(4,"Try gave worse %e from sx %d",tryy,
		               lsxp->last_fwd == NULL ? -999 : lsxp->last_fwd->no);
#endif
	}
	return tryy;		/* Function value of trial point */
}


/* Make up an initial simplex for dhsx routine */
static void
simplexinit(
int di,			/* Dimentionality */
double *cp,		/* Initial solution values */
double *s,		/* initial radius for each dimention */
double **p		/* Simplex to initialize */
) {
	double bb;
	double hh = 0.5;			/* Constant */
	double rr = sqrt(3.0)/2.0;	/* Constant */
	int i,j;
	for (i = 0; i <= di; i++) {	/* For each vertex */
		/* The bounding points form a equalateral simplex */
		/* whose vertexes are on a spehere about the data */
		/* point center. The coordinate sequence is: */
		/*  B = sphere radius */
		/*	H = 0.5         */
		/*	R = sqrt(3)/2   */
		/*   0  0  0 +B     */
		/*   0  0  0 -B     */

		/*   0  0   0  +B   */
		/*   0  0  +RB -HB  */
		/*   0  0  -RB -HB  */

		/*   0  0      0   +B   */
		/*   0  0     +RB  -HB  */
		/*   0  +RRb  -HRB -HB  */
		/*   0  -RRb  -HRB -HB  */

		/*   0       0      0   +B   */
		/*   0       0     +RB  -HB  */
		/*   0      +RRb   -HRB -HB  */
		/*   +RRRb  -HRRb  -HRB -HB  */
		/*   -RRRb  -HRRb  -HRB -HB  */

		/*      etc.     */
		bb = 1.0;		/* Initial unscaled radius */
		for (j = 0; j < di; j++) {	/* For each coordinate in vertex */
			if (j > i)
				p[i][j] = cp[j] + s[j] * 0.0;		/* If beyond last */
			else if (j == i)		/* If last non-zero */
				p[i][j] = cp[j] + s[j] * bb;
			else if (i == di && j == (di-1)) /* If last of last */
				p[i][j] = cp[j] + s[j] * -1.0 * bb;
			else					/* If before last */
				p[i][j] = cp[j] + s[j] * -hh * bb;
			bb *= rr;
		}
	}
}

