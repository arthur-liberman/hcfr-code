
/* Multi-dimentional minizer using Powell or Conjugate Gradient methods */
/* This is good for smoother, well behaved functions. */

/* Code is an original expression of the algorithms decsribed in */
/* "Numerical Recipes in C", by W.H.Press, B.P.Flannery, */
/* S.A.Teukolsky & W.T.Vetterling. */

/*
 * Copyright 2000, 2006 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* TTBD:
   Fix error handling to return status (malloc, excessive itters)
   Create to "safe" library ?
   Make standalone - ie remove numsup ?
 */

/*

	Idea for improving progress accounting:

	count number of itterations already done (pitter)
	estimate number yet needed (fitter)
	progress = pitter/(pitter + fitter)

	Number yet needed estimated by progress of retval delta
	againsth threshold target. 

	ie  fitters = (lastdel - curdel)/(curdel - stopth)

*/

/* Note that all arrays are indexed from 0 */

#include "numsup.h"
#include "powell.h"

#undef SLOPE_SANITY_CHECK		/* exermental */
#undef ABSTOL					/* Make tollerance absolute */
#undef DEBUG					/* Some debugging printfs (not comprehensive) */

#ifdef DEBUG
#undef DBG
#define DBG(xxx) printf xxx ;
#else
#undef DBG
#define DBG(xxx) 
#endif

static double linmin(double p[], double xi[], int n, double ftol,
	double (*func)(void *fdata, double tp[]), void *fdata);

/* Standard interface for powell function */
/* return 0 on sucess, 1 on failure due to excessive itterions */
/* Result will be in cp */
int powell(
double *rv,				/* If not NULL, return the residual error */
int di,					/* Dimentionality */
double cp[],			/* Initial starting point */
double s[],				/* Size of initial search area */
#ifdef ABSTOL
double ftol,			/* Absolute tollerance of error change to stop on */
#else
double ftol,			/* Relative tollerance of error change to stop on */
#endif
int maxit,				/* Maximum iterations allowed */
double (*func)(void *fdata, double tp[]),		/* Error function to evaluate */
void *fdata,			/* Opaque data needed by function */
void (*prog)(void *pdata, int perc),		/* Optional progress percentage callback */
void *pdata				/* Opaque data needed by prog() */
) {
	int i;
	double **dmtx;			/* Direction vector */
	double *spt;			/* Sarting point before exploring all the directions */
	double *xpt;			/* Extrapolated point */
	double *svec;			/* Search vector */
	int    iter;
	double retv; 			/* Returned function value at p */
	double stopth;			/* Current stop threshold */
	double startdel = -1.0;	/* Initial change in function value */
	double curdel;			/* Current change in function value */
	int pc = 0;				/* Percentage complete */

	dmtx = dmatrixz(0, di-1, 0, di-1);	/* Zero filled */
	spt  = dvector(0,di-1);
	xpt  = dvector(0,di-1);
	svec = dvector(0,di-1);

	/* Create initial direction matrix by */
	/* placing search start on diagonal */
	for (i = 0; i < di; i++)
		dmtx[i][i] = s[i];
	
	/* Save the starting point */
	for (i = 0; i < di; i++)
		spt[i] = cp[i];

	if (prog != NULL)		/* Report initial progress */
		prog(pdata, pc);

	/* Initial function evaluation */
	retv = (*func)(fdata, cp);

//printf("~1 ### initial retv = %f\n",retv);
	/* Itterate untill we converge on a solution, or give up. */
	for (iter = 1; iter < maxit; iter++) {
		int j;
		double lretv;			/* Last function return value */
		int    ibig = 0;		/* Index of biggest delta */
		double del = 0.0;		/* Biggest function value decrease */
		double pretv;			/* Previous function return value */

		pretv = retv;			/* Save return value at top of itteration */

		/* Loop over all directions in the set */
		for (i = 0; i < di; i++) {

			DBG(("Looping over direction %d\n",i))

			for (j = 0; j < di; j++)	/* Extract this direction to make search vector */
				svec[j] = dmtx[j][i];

//printf("~1 ### chosen dir = %f %f\n", svec[0],svec[1]);
			/* Minimize in that direction */
			lretv = retv;
			retv = linmin(cp, svec, di, ftol, func, fdata);

			/* Record bigest function decrease, and dimension it occured on */
			if (fabs(lretv - retv) > del) {
				del = fabs(lretv - retv);
				ibig = i;
			}
		}

//printf("~1 ### biggest change was dir %d  by %f\n", ibig, del);

#ifdef ABSTOL
		stopth = ftol;				/* Absolute tollerance */
#else
		stopth = ftol * 0.5 * (fabs(pretv) + fabs(retv) + DBL_EPSILON);
#endif
		curdel = fabs(pretv - retv);
		if (startdel < 0.0) {
			startdel = curdel;
		} else {
			int tt;
			tt = (int)(100.0 * pow((log(curdel) - log(startdel))/(log(stopth) - log(startdel)), 4.0) + 0.5);
			if (tt > pc && tt < 100) {
				pc = tt;
				if (prog != NULL)		/* Report initial progress */
					prog(pdata, pc);
			}

		}
		/* If we have had at least one change of direction and */
		/* reached a suitable tollerance, then finish */
		if (iter > 1 && curdel <= stopth) {
//printf("~1 ### stopping on itter %d because curdel %f <= stopth %f\n",iter, curdel,stopth);
			DBG(("Reached stop tollerance because curdel %f <= stopth %f\n",curdel,stopth))
			break;
		}
		DBG(("Not stopping because curdel %f > stopth %f\n",curdel,stopth))

//printf("~1 ### recomputing direction\n");
		for (i = 0; i < di; i++) {
			svec[i] = cp[i] - spt[i];	/* Average direction moved after minimization round */
			xpt[i]  = cp[i] + svec[i];	/* Extrapolated point after round of minimization */
			spt[i]  = cp[i];			/* New start point for next round */
		}
//printf("~1 ### new dir = %f %f\n", svec[0],svec[1]);

		/* Function value at extrapolated point */
		lretv = (*func)(fdata, xpt);

		if (lretv < pretv) {			/* If extrapolation is an improvement */
			double t, t1, t2;

//printf("~1 ### extrap is improvement\n");
			t1 = pretv - retv - del;
			t2 = pretv - lretv;
			t = 2.0 * (pretv -2.0 * retv + lretv) * t1 * t1 - del * t2 * t2;
			if (t < 0.0) {
//printf("~1 ### move to min in new direction\n");
				/* Move to the minimum of the new direction */
				retv = linmin(cp, svec, di, ftol, func, fdata);

				for (i = 0; i < di; i++) { 		/* Save the new direction */
					dmtx[i][ibig] = svec[i];	/* by replacing best previous */
				}
			}
		}
	}

//printf("~1 iters = %d\n",iter);
	/* Free up all the temporary vectors and matrix */
	free_dvector(svec,0,di-1);
	free_dvector(xpt,0,di-1);
	free_dvector(spt,0,di-1);
	free_dmatrix(dmtx, 0, di-1, 0, di-1);

	if (prog != NULL)		/* Report final progress */
		prog(pdata, 100);

	if (rv != NULL)
		*rv = retv;

	if (iter < maxit)
		return 0;

	DBG(("powell: returning 1 due to excessive itterations\n"))
	return 1;		/* Failed due to execessive itterations */
}

/* -------------------------------------- */
/* Conjugate Gradient optimiser */
/* return 0 on sucess, 1 on failure due to excessive itterions */
/* Result will be in cp */
/* Note that we could use gradient in line minimiser, */
/* but haven't bothered yet. */
int conjgrad(
double *rv,				/* If not NULL, return the residual error */
int di,					/* Dimentionality */
double cp[],			/* Initial starting point */
double s[],				/* Size of initial search area */
#ifdef ABSTOL
double ftol,			/* Absolute tollerance of error change to stop on */
#else
double ftol,			/* Relative tollerance of error change to stop on */
#endif
int maxit,				/* Maximum iterations allowed */
double (*func)(void *fdata, double tp[]),		/* Error function to evaluate */
double (*dfunc)(void *fdata, double dp[], double tp[]),		/* Gradient function to evaluate */
void *fdata,			/* Opaque data needed by function */
void (*prog)(void *pdata, int perc),		/* Optional progress percentage callback */
void *pdata				/* Opaque data needed by prog() */
) {
	int i, iter;
	double *svec;			/* Search vector */
	double *gvec;			/* G direction vector */
	double *hvec;			/* H direction vector */
	double retv; 			/* Returned function value at p */
	double stopth;			/* Current stop threshold */
	double startdel = -1.0;	/* Initial change in function value */
	double curdel;			/* Current change in function value */
	double svec_sca;			/* initial svec scale factor */
	int pc = 0;				/* Percentage complete */

	svec = dvector(0,di-1);
	gvec  = dvector(0,di-1);
	hvec  = dvector(0,di-1);

	if (prog != NULL)		/* Report initial progress */
		prog(pdata, pc);

	/* Initial function evaluation */
	retv = (*dfunc)(fdata, svec, cp);

	/* svec[] seems to be large after this. */
	/* Rescale it to conform to maximum of s[] */
	for (svec_sca = 0.0, i = 0; i < di; i++) {
		if (fabs(svec[i]) > svec_sca)
			svec_sca = fabs(svec[i]);
	}
	/* set scale so largest <= 1 */
	if (svec_sca < 1e-12)
		svec_sca = 1.0;
	else
		svec_sca = 1.0/svec_sca;

//printf("~1 ### initial dir = %f %f\n", svec[0],svec[1]);
//printf("~1 ### initial retv = %f\n",retv);
	/* Initial vector setup */
	for (i = 0; i < di; i++) {
		gvec[i] = hvec[i] = -svec[i];			/* Inverse gradient */
		svec[i] = s[i] * -svec[i] * svec_sca;	/* Scale the search vector */
	}
//printf("~1 ### svec = %f %f\n", svec[0],svec[1]);

	/* Itterate untill we converge on a solution, or give up. */
	for (iter = 1; iter < maxit; iter++) {
		double gamden, gamnum, gam;
		double pretv;			/* Previous function return value */

		DBG(("conjrad: about to do linmin\n"))
		pretv = retv;
		retv = linmin(cp, svec, di, ftol, func, fdata);

#ifdef ABSTOL
		stopth = ftol;				/* Absolute tollerance */
#else
		stopth = ftol * 0.5 * (fabs(pretv) + fabs(retv) + DBL_EPSILON);		// Old code
#endif
		curdel = fabs(pretv - retv);
//printf("~1 ### this retv = %f, pretv = %f, curdel = %f\n",retv,pretv,curdel);
		if (startdel < 0.0) {
			startdel = curdel;
		} else {
			int tt;
			tt = (int)(100.0 * pow((log(curdel) - log(startdel))/(log(stopth) - log(startdel)), 4.0) + 0.5);
			if (tt > pc && tt < 100) {
				pc = tt;
				if (prog != NULL)		/* Report initial progress */
					prog(pdata, pc);
			}
		}

		/* If we have had at least one change of direction and */
		/* reached a suitable tollerance, then finish */
		if (iter > 1 && curdel <= stopth) {
//printf("~1 ### stopping on itter %d because curdel %f <= stopth %f\n",iter, curdel,stopth);
			break;
		}
//printf("~1 ### Not stopping on itter %d because curdel %f > stopth %f\n",iter, curdel,stopth);

		DBG(("conjrad: recomputing direction\n"))
//printf("~1 ### recomputing direction\n");
		(*dfunc)(fdata, svec, cp);		/* (Don't use retv as it wrecks stop test) */

//printf("~1 ### pderiv = %f %f\n", svec[0],svec[1]);
		/* Compute gamma */
		for (gamnum = gamden = 0.0, i = 0; i < di; i++) {
			gamnum += svec[i] * (gvec[i] + svec[i]);
			gamden += gvec[i] * gvec[i];
		}

//printf("~1 ### gamnum = %f, gamden = %f\n", gamnum,gamden);
		if (gamden == 0.0) {		/* Gradient is exactly zero */
			DBG(("conjrad: gradient is exactly zero\n"))
			break;
		}

		gam = gamnum/gamden;
		DBG(("conjrad: gamma = %f = %f/%f\n",gam,gamnum,gamden))
//printf("~1 ### gvec[] = %f %f, gamma = %f, hvec = %f %f\n", gvec[0],gvec[1],gam,hvec[0],hvec[1]);

		/* Adjust seach direction */
		for (i = 0; i < di; i++) {
			gvec[i] = -svec[i];
			svec[i] = hvec[i] = gvec[i] + gam * hvec[i];
		}

		/* svec[] seems to be large after this. */
		/* Rescale it to conform to maximum of s[] */
		for (svec_sca = 0.0, i = 0; i < di; i++) {
			if (fabs(svec[i]) > svec_sca)
				svec_sca = fabs(svec[i]);
		}
		/* set scale so largest <= 1 */
		if (svec_sca < 1e-12)
			svec_sca = 1.0;
		else
			svec_sca = 1.0/svec_sca;
		for (i = 0; i < di; i++)
			svec[i] = svec[i] * s[i] * svec_sca;
//printf("~1 ### svec = %f %f\n", svec[0],svec[1]);

	}
	/* Free up all the temporary vectors and matrix */
	free_dvector(hvec,0,di-1);
	free_dvector(gvec,0,di-1);
	free_dvector(svec,0,di-1);

	if (prog != NULL)		/* Report final progress */
		prog(pdata, 100);

	if (rv != NULL)
		*rv = retv;

//printf("~1 ### done\n");

	if (iter < maxit)
		return 0;

	return 1;		/* Failed due to execessive itterations */
}

/*------------------------------*/
#define POWELL_GOLD 1.618034
#define POWELL_CGOLD 0.3819660
#define POWELL_MAXIT 100

/* Line bracketing and minimisation routine. */
/* Return value at minimum. */
static double linmin(
double cp[],		/* Start point, and returned value */
double xi[],		/* Search vector */
int di,				/* Dimensionality */
#ifdef ABSTOL
double ftol,		/* Absolute tolerance to stop on */
#else
double ftol,		/* Relative tolerance to stop on */
#endif
double (*func)(void *fdata, double tp[]),		/* Error function to evaluate */
void *fdata)		/* Opaque data for func() */
{
	int i;
	double ax, xx, bx;	/* Search vector multipliers */
	double af, xf, bf;	/* Function values at those points */
	double *xt, XT[10];	/* Trial point */

	if (di <= 10)
		xt = XT;
	else
		xt = dvector(0, di-1);			/* Vector for trial point */

	/* -------------------------- */
	/* First bracket the solution */

	DBG(("linmin: Bracketing solution\n"))

	/* The line is measured as startpoint + offset * search vector. */
	/* (Search isn't symetric, but it seems to depend on cp being */
	/* best current solution ?) */
	ax = 0.0;
	for (i = 0; i < di; i++)
		xt[i] = cp[i] + ax * xi[i];
	af = (*func)(fdata, xt);

	/* xx being vector offset 0.618 */
	xx =  1.0/POWELL_GOLD;
	for (i = 0; i < di; i++)
		xt[i] = cp[i] + xx * xi[i];
	xf = (*func)(fdata, xt);

	DBG(("linmin: Initial points a:%f:%f -> b:%f:%f\n",ax,af,xx,xf))

	/* Fix it so that we are decreasing from point a -> x */
	if (xf > af) {
		double tt;
		tt = ax; ax = xx; xx = tt;
		tt = af; af = xf; xf = tt;
	}
	DBG(("linmin: Ordered Initial points a:%f:%f -> b:%f:%f\n",ax,af,xx,xf))

	bx = xx + POWELL_GOLD * (xx-ax);	/* Guess b beyond a -> x */
	for (i = 0; i < di; i++)
		xt[i] = cp[i] + bx * xi[i];
	bf = (*func)(fdata, xt);

	DBG(("linmin: Initial bracket a:%f:%f x:%f:%f b:%f:%f\n",ax,af,xx,xf,bx,bf))

#ifdef SLOPE_SANITY_CHECK
	/* If we're not seeing a slope indicitive of progress */
	/* of order ftol, give up straight away */
	if (2000.0 * fabs(xf - bf) <= ftol * (fabs(xf) + fabs(bf))
	 && 2000.0 * fabs(af - xf) <= ftol * (fabs(af) + fabs(xf))) {
		DBG(("linmin: giving up because slope is too shallow\n"))
		if (xt != XT)
			free_dvector(xt,0,di-1);

		if (bf < xf) {
			xf = bf;
			xx = bx;
		}

		/* Compute solution vector */
		for (i = 0; i < di; i++) 
			cp[i] += xx * xi[i];
		return xf;
	}
#endif /* SLOPE_SANITY_CHECK */

	/* While not bracketed */
	while (xf > bf) {
		double ulim, ux, uf;
		double tt, r, q;

//		DBG(("linmin: Not bracketed a:%f:%f x:%f%f b:%f:%f\n",ax,af,xx,xf,bx,bf))
		DBG(("linmin: Not bracketed because xf %f > bf %f\n",xf, bf))
		DBG(("        ax = %f, xx = %f, bx = %f\n",ax,xx,bx))

		/* Compute ux by parabolic interpolation from a, x & b */
		q = (xx - bx) * (xf - af);
		r = (xx - ax) * (xf - bf);
		tt = q - r;
		if (tt >= 0.0 && tt < 1e-20)				/* If +ve too small */
			tt = 1e-20;
		else if (tt <= 0.0 && tt > -1e-20)		/* If -ve too small */
			tt = -1e-20;
		ux = xx - ((xx - bx) * q - (xx - ax) * r) / (2.0 * tt);
		ulim = xx + 100.0 * (bx - xx);			/* Extrapolation limit */

//printf("~1 ux = %f, ulim = %f\n",ux,ulim);
		if ((xx - ux) * (ux - bx) > 0.0) {		/* u is between x and b */

			for (i = 0; i < di; i++)			/* Evaluate u */
				xt[i] = cp[i] + ux * xi[i];
			uf = (*func)(fdata, xt);

//printf("~1 u is between x and b, uf = %f\n",uf);

			if (uf < bf) {						/* Minimum is between x and b */
//printf("~1 min is between x and b\n");
				ax = xx; af = xf;
				xx = ux; xf = uf;
				break;
			} else if (uf > xf) {				/* Minimum is between a and u */
//printf("~1 min is between a and u\n");
				bx = ux; bf = uf;
				break;
			}

			/* Parabolic fit didn't work, look further out in direction of b */
			ux = bx + POWELL_GOLD * (bx-xx);
//printf("~1 parabolic fit didn't work,look further in direction of b (%f)\n",ux);

		} else if ((bx - ux) * (ux - ulim) > 0.0) {	/* u is between b and limit */
			for (i = 0; i < di; i++)			/* Evaluate u */
				xt[i] = cp[i] + ux * xi[i];
			uf = (*func)(fdata, xt);

//printf("~1 u is between b and limit uf = %f\n",uf);
			if (uf > bf) {						/* Minimum is between x and u */
//printf("~1 min is between x and uf\n");
				ax = xx; af = xf;
				xx = bx; xf = bf;
				bx = ux; bf = uf;
				break;
			}
			xx = bx; xf = bf;					/* Continue looking */
			bx = ux; bf = uf;
			ux = bx + POWELL_GOLD * (bx - xx);	/* Test beyond b */
//printf("~1 continue looking beyond b (%f)\n",ux);

		} else if ((ux - ulim) * (ulim - bx) >= 0.0) {	/* u is beyond limit */
			ux = ulim;
//printf("~1 use limit\n");
		} else {							/* u is to left side of x ? */
			ux = bx + POWELL_GOLD * (bx-xx);
//printf("~1 look gold beyond b (%f)\n",ux);
		}
		/* Evaluate u, and move into place at b */
		for (i = 0; i < di; i++)
			xt[i] = cp[i] + ux * xi[i];
		uf = (*func)(fdata, xt);
//printf("~1 lookup ux %f value uf = %f\n",ux,uf);
		ax = xx; af = xf;
		xx = bx; xf = bf;
		bx = ux; bf = uf;
//printf("~1 move along to the right (a<-x, x<-b, b-<u)\n");
	}
	DBG(("linmin: Got bracket a:%f:%f x:%f:%f b:%f:%f\n",ax,af,xx,xf,bx,bf))
	/* Got bracketed minimum between a -> x -> b */
//printf("~1 got bracketed minimum at %f (%f), %f (%f), %f (%f)\n",ax,af,xx,xf,bx,bf);

	/* --------------------------------------- */
	/* Now use brent minimiser bewteen a and b */
	{
		/* a and b bracket solution */
		/* x is best function value so far */
		/* w is second best function value so far */
		/* v is previous second best, or third best */
		/* u is most recently tested point */
		double wx, vx, ux;			/* Search vector multipliers */
		double wf, vf = 0.0, uf;	/* Function values at those points */
		int iter;
		double de = 0.0;	/* Distance moved on previous step */
		double e = 0.0;		/* Distance moved on 2nd previous step */

		/* Make sure a and b are in ascending order */
		if (ax > bx) {
			double tt;
			tt = ax; ax = bx; bx = tt;
			tt = af; af = bf; bf = tt;
		}

		wx = vx = xx;	/* Initial values of other center points */
		wf = xf = xf;

		for (iter = 1; iter <= POWELL_MAXIT; iter++) {
			double mx = 0.5 * (ax + bx);		/* m is center of bracket values */
#ifdef ABSTOL
			double tol1 = ftol;			/* Absolute tollerance */
#else
			double tol1 = ftol * fabs(xx) + 1e-10;
#endif
			double tol2 = 2.0 * tol1;

			DBG(("linmin: Got bracket a:%f:%f x:%f:%f b:%f:%f\n",ax,af,xx,xf,bx,bf))

			/* See if we're done */
//printf("~1 linmin check %f <= %f\n",fabs(xx - mx), tol2 - 0.5 * (bx - ax));
			if (fabs(xx - mx) <= (tol2 - 0.5 * (bx - ax))) {
				DBG(("linmin: We're done because %f <= %f\n",fabs(xx - mx), tol2 - 0.5 * (bx - ax)))
				break;
			}

			if (fabs(e) > tol1) {			/* Do a trial parabolic fit */
				double te, p, q, r;
				r = (xx-wx) * (xf-vf);
				q = (xx-vx) * (xf-wf);
				p = (xx-vx) * q - (xx-wx) * r;
				q = 2.0 * (q - r);
				if (q > 0.0)
					p = -p;
				else
					q = -q;
				te = e;				/* Save previous e value */
				e = de;				/* Previous steps distance moved */

				DBG(("linmin: Trial parabolic fit\n" ))

				if (fabs(p) >= fabs(0.5 * q * te) || p <= q * (ax-xx) || p >= q * (bx-xx)) {
					/* Give up on the parabolic fit, and use the golden section search */
					e = ((xx >= mx) ? ax-xx : bx-xx);	/* Override previous distance moved */
					de = POWELL_CGOLD * e;
					DBG(("linmin: Moving to golden section search\n" ))
				} else {	/* Use parabolic fit */
					de = p/q;			/* Change in xb */
					ux = xx + de;		/* Trial point according to parabolic fit */
					if ((ux - ax) < tol2 || (bx - ux) < tol2) {
						if ((mx - xx) > 0.0)	/* Don't use parabolic, use tol1 */
							de = tol1;			/* tol1 is +ve */
						else
							de = -tol1;
					}
					DBG(("linmin: Using parabolic fit\n" ))
				}
			} else {	/* Keep using the golden section search */
				e = ((xx >= mx) ? ax-xx : bx-xx);	/* Override previous distance moved */
				de = POWELL_CGOLD * e;
				DBG(("linmin: Continuing golden section search\n" ))
			}

			if (fabs(de) >= tol1) {		/* If de moves as much as tol1 would */
				ux = xx + de;			/* use it */
				DBG(("linmin: ux = %f = xx %f + de %f\n",ux,xx,de))
			} else {					/* else move by tol1 in direction de */
				if (de > 0.0) {
					ux = xx + tol1;
					DBG(("linmin: ux = %f = xx %f + tol1 %f\n",ux,xx,tol1))
				} else {
					ux = xx - tol1;
					DBG(("linmin: ux = %f = xx %f - tol1 %f\n",ux,xx,tol1))
				}
			}

			/* Evaluate function */
			for (i = 0; i < di; i++)
				xt[i] = cp[i] + ux * xi[i];
			uf = (*func)(fdata, xt);

			if (uf <= xf) {					/* Found new best solution */
				if (ux >= xx) {	
					ax = xx; af = xf;		/* New lower bracket */
				} else {
					bx = xx; bf = xf;		/* New upper bracket */
				}
				vx = wx; vf = wf;			/* New previous 2nd best solution */
				wx = xx; wf = xf;			/* New 2nd best solution from previous best */
				xx = ux; xf = uf;			/* New best solution from latest */
				DBG(("linmin: found new best solution\n"))
			} else {						/* Found a worse solution */
				if (ux < xx) {
					ax = ux; af = uf;		/* New lower bracket */
				} else {
					bx = ux; bf = uf;		/* New upper bracket */
				}
				if (uf <= wf || wx == xx) {	/* New 2nd best solution, or equal best */
					vx = wx; vf = wf;		/* New previous 2nd best solution */
					wx = ux; wf = uf;		/* New 2nd best from latest */
				} else if (uf <= vf || vx == xx || vx == wx) {	/* New 3rd best, or equal 1st & 2nd */
					vx = ux; vf = uf;		/* New previous 2nd best from latest */
				}
				DBG(("linmin: found new worse solution\n"))
			}
		}
		/* !!! should do something if iter > POWELL_MAXIT !!!! */
		/* Solution is at xx, xf */

		/* Compute solution vector */
		for (i = 0; i < di; i++) 
			cp[i] += xx * xi[i];
	}

	if (xt != XT)
		free_dvector(xt,0,di-1);
//printf("~~~ line minimizer returning %e\n",xf);
	return xf;
}

#undef POWELL_GOLD
#undef POWELL_CGOLD
#undef POWELL_MAXIT

/**************************************************/
