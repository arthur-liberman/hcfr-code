
/* 
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    8/9/01
 * Version: 1.00
 *
 * Copyright 2001 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/* TTBD:
 *
 * Should switch this over to the linearisation used in
 * the current xlut.c (== xlut2.c), rather than linearise
 * w.r.t. the Y value.
 */

/*
 * This class handles the creation of device chanel linearisation
 * curves, given a callback function that maps the device chanels
 * to the value that should be linearised.
 *
 * This class is independent of other icc or icx classes.
 *
 * Its usual use is to create an Lab linerisation curve
 * for native XYZ profiles.
 */

#undef DEBUG			/* Plot 1d Luts */

#include <sys/types.h>
#ifdef __sun
#include <unistd.h>
#endif
#include "numlib.h"
#include "rspl.h"
#include "xdevlin.h"		/* definitions for this class */

#ifdef DEBUG
#include "plot.h"
#endif

/* Free up the xdevlin */
static void xdevlin_del(struct _xdevlin *p) {
	int e;
	for (e = 0; e < p->di; e++) {
		if (p->curves[e] != NULL)
			p->curves[e]->del(p->curves[e]);
	}
	free (p);
}

/* Return the linearisation values given the device values */
static void xdevlin_lin(
struct _xdevlin *p,		/* this */
double *out,			/* di input */
double *in				/* di output */
) {
	co tc;
	int e;
	for (e = 0; e < p->di; e++) {
		tc.p[0] = in[e];
		p->curves[e]->interp(p->curves[e], &tc);
		out[e] = tc.v[0];
	}
}

#define MAX_INVSOLN 5

/* Return the inverse linearisation */
static void xdevlin_invlin(
struct _xdevlin *p,		/* this */
double *out,			/* di input */
double *in				/* di output */
) {
	int i, j;
	int nsoln;				/* Number of solutions found */
	co pp[MAX_INVSOLN];		/* Room for all the solutions found */
	double cdir;

	for (i = 0; i < p->di; i++) {
		pp[0].p[0] = p->clipc[i];
		pp[0].v[0] = in[i];
		cdir = p->clipc[i] - in[i];	/* Clip towards output range */

		nsoln = p->curves[i]->rev_interp (
			p->curves[i], 		/* this */
			0,					/* No flags */
			MAX_INVSOLN,		/* Maximum number of solutions allowed for */
			NULL, 				/* No auxiliary input targets */
			&cdir,				/* Clip vector direction and length */
			pp);				/* Input and output values */

		nsoln &= RSPL_NOSOLNS;		/* Get number of solutions */

		if (nsoln == 1) { /* Exactly one solution */
			j = 0;
		} else if (nsoln == 0) {	/* Zero solutions. This is unexpected. */
			error("~~~1 Unexpected failure to find reverse solution for linearisation curve");
			return;
		} else {		/* Multiple solutions */
			/* Use a simple minded resolution - choose the one closest to the center */
			double bdist = 1e300;
			int bsoln = 0;
/* Don't expect this - 1D luts are meant to be monotonic */
printf("~~~1 got %d reverse solutions\n",nsoln);
printf("~~~1 solution 0 = %f\n",pp[0].p[0]);
printf("~~~1 solution 1 = %f\n",pp[1].p[0]);
			for (j = 0; j < nsoln; j++) {
				double tt;
				tt = pp[i].p[0] - p->clipc[i];
				tt *= tt;
				if (tt < bdist) {	/* Better solution */
					bdist = tt;
					bsoln = j;
				}
			}
			j = bsoln;
		}
		out[i] = pp[j].p[0];
	}
}

/* Callback function that is used to set each chanels grid value */
static void set_curve(void *cbntx, double *out, double *in) {
	xdevlin *p = (xdevlin *)cbntx;
	int e, ee = p->setch;
	double tin[MXDI], tout[MXDO];
	double tt;

	/* setup input value */
	for (e = 0; e < p->di; e++)
		tin[e] = p->pol ? p->max[e] : p->min[e];
	tin[ee] = in[0];

	p->lookup(p->lucntx, tout, tin);

	tt = (tout[0] - p->lmin)/(p->lmax - p->lmin);	/* Normalise from L */

	out[0] = tt * (p->max[ee] - p->min[ee]) + p->min[ee];	/* Back to device range */
}

/* Create an appropriate linearisation from the callback */
/* This code is very similar to that in xlut.c when creating */
/* a device profiles linearisation curves. */
xdevlin *new_xdevlin(
int di,				/* Device dimenstionality */
double *min, double *max,	/* Min & max range of device values, NULL = 0.0 - 1.0 */ 
void *lucntx,			/* Context for callback */
void (*lookup) (void *lucntx, double *lin, double *dev)
) {
	int ee, e;
	xdevlin *p;

	/* Do the basic class initialisation */
	if ((p = (xdevlin *) calloc(1,sizeof(xdevlin))) == NULL)
		return NULL;
	p->del      = xdevlin_del;
	p->lin      = xdevlin_lin;
	p->invlin   = xdevlin_invlin;

	/* And then set it up */
	p->di = di;
	p->lucntx = lucntx;
	p->lookup = lookup;

	/* Setup the clipping center */
	for (e = 0; e < p->di; e++) {
		p->min[e] = min[e];
		p->max[e] = max[e];
		p->clipc[e] = 0.5 * (min[e] + max[e]);
	}

	/* Determine what level to set the chanels we're not interested in */
	{
		double tin[MXDI], tout[MXDO];
		double l00, l01, l10, l11;		/* Resulting levels */

		for (e = 0; e < p->di; e++)
			tin[e] = min[e];
		lookup(lucntx, tout, tin);
		l00 = tout[0];					/* All minimum */

		tin[0] = max[0];
		lookup(lucntx, tout, tin);
		l01 = tout[0];					/* First chanel max, rest min */
		
		for (e = 0; e < p->di; e++)
			tin[e] = max[e];
		lookup(lucntx, tout, tin);
		l11 = tout[0];					/* All maximum */

		tin[0] = min[0];
		lookup(lucntx, tout, tin);
		l10 = tout[0];					/* First chanel min, rest max */
		
		if (fabs(l11 - l10) > fabs(l00 - l01))
			p->pol = 1;					/* Set other chanels to max */
		else
			p->pol = 0;					/* Set other chanels to min */
	}

	/* For each chanel, create an rspl */

	for (ee = 0; ee < p->di; ee++) {
		double tin[MXDI], tout[MXDO];
		int gres = 100;		// 4096
#ifdef DEBUG
	#define	XRES 100
	double xx[XRES];
	double y1[XRES];
#endif /* DEBUG */

		if ((p->curves[ee] = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
			error("Creation of rspl failed in xdevlin");
		}

		p->setch = ee;

		/* Figure the L min and max */
		for (e = 0; e < p->di; e++)
			tin[e] = p->pol ? max[e] : min[e];
		tin[ee] = min[ee];
		lookup(lucntx, tout, tin);
		p->lmin = tout[0];
		tin[ee] = max[ee];
		lookup(lucntx, tout, tin);
		p->lmax = tout[0];

		p->curves[ee]->set_rspl(
			p->curves[ee],
			0,
			p,			/* Opaque function context */
			set_curve,	/* Function to set from */
			min, max, 	/* Grid low scale, grid high scale */
			&gres,		/* Grid resolution */
			min, max 	/* Data value normalsie low and high */
		);

#ifdef DEBUG
		{
		int i;
		/* Display the result curve */
		for (i = 0; i < XRES; i++) {
			double x;
			co c;
			x = i/(double)(XRES-1);
			xx[i] = x;
			c.p[0] = x;
			p->curves[ee]->interp(p->curves[ee], &c);
			y1[i] = c.v[0];
			}
		do_plot(xx,y1,NULL,NULL,XRES);
		}
#endif /* DEBUG */

	}

	p->lookup = NULL;	/* Not valid after function return */
	return p;
}

































