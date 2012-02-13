
/* 
 * Argyll Color Correction System
 * Display callibrator.
 *
 * Author: Graeme W. Gill
 * Date:   14/10/2005
 *
 * Copyright 1996 - 2007 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* This program displays test patches, and takes readings from a display device */
/* in order to create a RAMDAC calibration curve (usually stored in the ICC vcgt tag) */

/* This is the third version of the program. */

/* TTBD

	Add a white point option that makes the target the
	closest temperature to the native one of the display :-
	ie. it moves the display to the closest point on the
	chosen locus to RGB 1,1,1.
     ie. should it do this if "-t" or "-T"
	with no specific teperature is chosen ?


	Add bell at end of calibration ?

	The verify (-E) may not be being done correctly.
	Like update, shouldn't it read the .cal file to set what's
	being calibrated agaist ? (This would fix missing ambient value too!)

	What about the "Read the base test set" - aren't
	there numbers then used to tweak the black aim point
	in "Figure out the black point target" - Yes they are !!
	Verify probably shouldn't work this way.


	Add option to plot graph of native and calibrated RGB ?


	Add a "delta E" number to the interactive adjustments,
	so the significance of the error can be judged ?

	Need to add flare measure/subtract, to improve
	projector calibration ? - need to add to dispread too.

	It may be an improvement to automatically set the black
	ajustment level (-k) using a heuristic based on the black level
	and the change in brightness that would result from 100%
	black point grey adjustment.

 */

#ifdef __MINGW32__
# define WINVER 0x0500
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#if defined (NT)
#include <conio.h>
#endif
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "xicc.h"
#include "xspect.h"
#include "xcolorants.h"
#include "ccmx.h"
#include "ccss.h"
#include "cgats.h"
#include "insttypes.h"
#include "icoms.h"
#include "inst.h"
#include "conv.h"
#include "dispwin.h"
#include "dispsup.h"
#include "rspl.h"
#include "moncurve.h"
#include "targen.h"
#include "ofps.h"
#include "icc.h"
#include "sort.h"
#include "spyd2setup.h"			/* Enable Spyder 2 access */

#undef DEBUG
#undef DEBUG_OFFSET			/* Keep test window out of the way */
#undef DEBUG_PLOT			/* Plot curve each time around */
#undef CHECK_MODEL			/* Do readings to check the accuracy of our model */
#undef SHOW_WINDOW_ONFAKE	/* Display a test window up for a fake device */

/* Invoke with -dfake for testing with a fake device. */
/* Will use a fake.icm/.icc profile if present, or a built in fake */
/* device behaviour if not. */

#define COMPORT 1			/* Default com port 1..4 */
#define REFINE_GAIN 0.80	/* Refinement correction damping/gain */
#define MAX_RPTS 12			/* Maximum tries at making a sample meet the current threshold */
#define VER_RES 100			/* Verification resolution */
#define NEUTRAL_BLEND_RATE 4.0		/* Default rate of transition for -k factor < 1.0 (power) */
#define ADJ_JACOBIAN		/* Adjust the Jacobian predictor matrix each time */
#define JAC_COR_FACT 0.4	/* Amount to correct Jacobian by (to filter noise) */
#define REMEAS_JACOBIAN		/* Re-measure Jacobian */
#define MOD_DIST_POW 1.6	/* Power used to distribute test samples for model building */
#define REFN_DIST_POW 1.6	/* Power used to distribute test samples for grey axis refinement */
#define CHECK_DIST_POW 1.6	/* Power used to distribute test samples for grey axis checking */
#define THRESH_SCALE_POW 0.5 /* Amount to loosen threshold for first itterations */
#define CAL_RES 256			/* Resolution of calibration table to produce. */
#define CLIP				/* Clip RGB during refinement */
#define RDAC_SMOOTH 0.3		/* RAMDAC curve fitting smoothness */
#define MEAS_RES			/* Measure the RAMNDAC entry size */ 

#define VERBOUT stdout

#ifdef DEBUG_PLOT
#include "plot.h"
#endif

#if defined(DEBUG)

#define DBG(xxx) fprintf xxx ;
#define dbgo stderr
#else
#define DBG(xxx) 
#endif	/* DEBUG */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Sample points used in initial device model optimisation */

typedef struct {
	double dev[3];		/* Device values */
	double lab[3];		/* Read value */
	double w;			/* Weighting */
} optref;

/* - - - - - - - - - - - - - - - - - - - */
/* device RGB inverse solution code */

/* Selected transfer curve */
typedef enum {
	gt_power     = 0,	/* A simple power */
	gt_Lab       = 1,	/* The L* curve */
	gt_sRGB      = 2,	/* The sRGB curve */
	gt_Rec709    = 3,	/* REC 709 video standard */
	gt_SMPTE240M = 4	/* SMTPE 240M video standard */
} gammatype;

/* Context for calibration solution */
typedef struct {
	double wh[3];		/* White absolute XYZ value */
	double bk[3];		/* Black absolute XYZ value */

	/* Target model */
	gammatype gammat;	/* Transfer curve type */
	double egamma;		/* Effective Gamma target */
	double oofff;		/* proportion of output offset vs input offset (default 1.0) */
	double gioff;		/* Gamma curve input zero offset */
	double gooff;		/* Target output offset (normalised to Y max of 1.0) */
	int nat;			/* Flag - nz if native white target */
	double nbrate;		/* Neutral blend weight (power) */

	/* Viewing  conditions adjustment */
	int vc;				/* Flag, nz to enable viewing conditions adjustment */
	icxcam *svc;		/* Source viewing conditions */
	icxcam *dvc;		/* Destination viewing conditions */
	double vn0, vn1;	/* Normalisation values */

	double nwh[3];		/* Target white normalised XYZ value (Y = 1.0) */
	double twh[3];		/* Target white absolute XYZ value */
	icmXYZNumber twN;	/* Same as above as XYZNumber */

	double tbk[3];		/* Target black point color */
	icmXYZNumber tbN;	/* Same as above as XYZNumber */

	double fm[3][3];	/* Forward, aprox. linear RGB -> XYZ */
	double bm[3][3];	/* Backwards, aprox. XYZ -> linear RGB */
	mcv *dcvs[3];		/* Device RGB channel to linearised RGB curves */

	mcv *rdac[3];		/* Current RGB to RGB ramdac curves */

	double xyz[3];		/* Target xyz value */

	/* optimisation information */
	int np;				/* Total number of optimisation parameters */
	int co[3];			/* Offset in the parameters to each curve offset */
	int nc[3];			/* Number of for each curve */
	int nrp;			/* Total number of reference points */
	optref *rp;			/* reference points */
	double *dtin_iv;	/* Temporary array :- dp for input curves */
} cctx;

/* - - - - - - - - - - - - - - - - - - - */
/* Ideal target curve definitions */

/* Convert ideal device (0..1) to target Y value (0..1) */
static double dev2Y(cctx *x, double egamma, double vv) {

	switch(x->gammat) {
		case gt_power: {
			vv = pow(vv, egamma);
			break;
		}
		case gt_Lab: {
			vv = icmL2Y(vv * 100.0);
			break;
		}
		case gt_sRGB: {
			if (vv <= 0.03928)
				vv = vv/12.92;
			else
				vv = pow((0.055 + vv)/1.055, 2.4);
			break;
		}
		case gt_Rec709: {
			if (vv <= 0.081)
				vv = vv/4.5;
			else
				vv = pow((0.099 + vv)/1.099, 1.0/0.45);
			break;
		}
		case gt_SMPTE240M: {
			if (vv <= 0.0913)
				vv = vv/4.0;
			else
				vv = pow((0.1115 + vv)/1.1115, 1.0/0.45);
			break;
		} 
		default:
			error("Unknown gamma type");
	}
	return vv;
}

/* Convert target Y value (0..1) to ideal device (0..1) */
static double Y2dev(cctx *x, double egamma, double vv) {

	switch(x->gammat) {
		case gt_power: {
			vv = pow(vv, 1.0/egamma);
			break;
		}
		case gt_Lab: {
			vv = icmY2L(vv) * 0.01;
			break;
		}
		case gt_sRGB: {
			if (vv <= 0.00304)
				vv = vv * 12.92;
			else
				vv = pow(vv, 1.0/2.4) * 1.055 - 0.055;
			break;
		}
		case gt_Rec709: {
			if (vv <= 0.018)
				vv = vv * 4.5;
			else
				vv = pow(vv, 0.45) * 1.099 - 0.099;
			break;
		}
		case gt_SMPTE240M: {
			if (vv <= 0.0228)
				vv = vv * 4.0;
			else
				vv = pow(vv, 0.45) * 1.1115 - 0.1115;
			break;
		} 
		default:
			error("Unknown gamma type");
	}
	return vv;
}

/* - - - - - - - - - - - - - - - - - - - */
/* Compute a viewing environment Y transform */

static double view_xform(cctx *x, double in) {
	double out = in;

	if (x->vc != 0) {
		double xyz[3], Jab[3];

		xyz[0] = in * x->nwh[0];				/* Compute value on neutral axis */
		xyz[1] = in * x->nwh[1];
		xyz[2] = in * x->nwh[2];
		x->svc->XYZ_to_cam(x->svc, Jab, xyz);
		x->dvc->cam_to_XYZ(x->dvc, xyz, Jab);

		out = xyz[1] * x->vn1 + x->vn0;			/* Apply scaling factors */
	}
	return out;
}

/* - - - - - - - - - - - - - - - - - - - */

/* Info for optimization */
typedef struct {
	double thyr;		/* 50% input target */
	double roo;			/* 0% input target */
} gam_fits;

/* gamma + input offset function handed to powell() */
static double gam_fit(void *dd, double *v) {
	gam_fits *gf = (gam_fits *)dd;
	double gamma = v[0];
	double ioff = v[1];
	double rv = 0.0;
	double tt;

	if (gamma < 0.0) {
		rv += 100.0 * -gamma;
		gamma = 0.0;
	}
	if (ioff < 0.0) {
		rv += 100.0 * -ioff;
		ioff = 0.0;
	} else if (ioff > 0.999) {
		rv += 100.0 * (ioff - 0.999);
		ioff = 0.999;
	}
	tt = gf->roo - pow(ioff, gamma);
	rv += tt * tt;
	tt = gf->thyr - pow(0.5 + (1.0 - 0.5) * ioff, gamma);
	rv += tt * tt;
	
//printf("~1 gam_fit %f %f returning %f\n",ioff,gamma,rv);
	return rv;
}


/* Given the advertised gamma and the output offset, compute the */
/* effective gamma and input offset needed. */
/* Return the expected output value for 50% input. */
/* (It's assumed that gooff is normalised the target brightness) */
static double tech_gamma(
	cctx *x,
	double *pegamma,		/* return effective gamma needed */
	double *pooff,			/* return output offset needed */
	double *pioff,			/* return input offset needed */
	double egamma,			/* effective gamma needed (> 0.0 if valid, overrides gamma) */
	double gamma,			/* advertised gamma needed */
	double tooff			/* Total ouput offset needed */
) {
	int i;
	double rv;
	double gooff = 0.0;		/* The output offset applied */
	double gioff = 0.0;		/* The input offset applied */
	double roo;				/* Remaining output offset accounted for by input offset */

	/* Compute the output offset that will be applied */
	gooff = tooff * x->oofff;
	roo = (tooff - gooff)/(1.0 - gooff);

//printf("~1 gooff = %f, roo = %f\n",gooff,roo);

	/* Now compute the input offset that will be needed */
	if (x->gammat == gt_power && egamma <= 0.0) {
		gam_fits gf;
		double op[2], sa[2], rv;

		gf.thyr = pow(0.5, gamma);					/* Advetised 50% target */
		gf.thyr = (gf.thyr - gooff)/(1.0 - gooff);	/* Target before gooff is added */
		gf.roo = roo;

		op[0] = gamma;
		op[1] = pow(roo, 1.0/gamma);
		sa[0] = 0.1;
		sa[1] = 0.01;

		if (powell(&rv, 2, op, sa, 1e-6, 500, gam_fit, (void *)&gf, NULL, NULL) != 0)
			warning("Computing effective gamma and input offset is inaccurate");

		if (rv > 1e-5) {
			warning("Computing effective gamma and input offset is inaccurate (%f)",rv);
		}
		egamma = op[0];
		gioff = op[1];  

//printf("~1 Result gioff %f, gooff %f, egamma %f\n",gioff, gooff, egamma);
//printf("~1 Verify 0.0 in -> out = %f, tooff = %f\n",gooff + dev2Y(x, egamma, gioff) * (1.0 - gooff),tooff);
//printf("~1 Verify 0.5 out = %f, target %f\n",gooff + dev2Y(x, egamma, gioff + 0.5 * (1.0 - gioff)) * (1.0 - gooff), pow(0.5, gamma));

	} else {
		gioff = Y2dev(x, egamma, roo);
//printf("~1 Result gioff %f, gooff %f\n",gioff, gooff);
//printf("~1 Verify 0.0 in -> out = %f, tooff = %f\n",gooff + dev2Y(x, egamma, gioff) * (1.0 - gooff),tooff);
	}

	/* Compute the 50% output value */
	rv = gooff + dev2Y(x, egamma, gioff + 0.5 * (1.0 - gioff)) * (1.0 - gooff);

	if (pegamma != NULL)
		*pegamma = egamma;
	if (pooff != NULL)
		*pooff = gooff;
	if (pioff != NULL)
		*pioff = gioff;
	return rv;
}

/* Compute approximate advertised gamma from black/50% grey/white readings, */
/* (assumes a zero based gamma curve shape) */
static double pop_gamma(double bY, double gY, double wY) {
	int i;
	double grat, brat, gioff, gvv, gamma;

	grat = gY/wY;
	brat = bY/wY;
	
	gamma = log(grat) / log(0.5);
	return gamma;
}

/* - - - - - - - - - - - - - - - - - - - */

/* Return the xyz that is predicted by our aproximate device model */
/* by the given device RGB. */
static void fwddev(cctx *x, double xyz[3], double rgb[3]) {
	double lrgb[3];
	int j;

//printf("~1 fwddev called with rgb %f %f %f\n",rgb[0],rgb[1],rgb[2]);

	/* Convert device RGB into linear light RGB via curves */
	for (j = 0; j < 3; j++)
		lrgb[j] = x->dcvs[j]->interp(x->dcvs[j], rgb[j]);

//printf("~1 fwddev got linear RGB %f %f %f\n",lrgb[0],lrgb[1],lrgb[2]);

	/* Convert linear light RGB into XYZ via the matrix */
	icmMulBy3x3(xyz, x->fm, lrgb);

//printf("~1 fwddev got final xyz %f %f %f\n",xyz[0],xyz[1],xyz[2]);
}

/* Return the closest device RGB predicted by our aprox. device model */
/* to generate the given xyz. */
/* Return > 0 if clipped */
static double invdev(cctx *x, double rgb[3], double xyz[3]) {
	double lrgb[3];
	double clip = 0.0;
	int j;

//printf("~1 invdev called with xyz %f %f %f\n",xyz[0],xyz[1],xyz[2]);

	/* Convert XYZ to linear light RGB via the inverse matrix */
	icmMulBy3x3(lrgb, x->bm, xyz);
//printf("~1 invdev; lin light rgb = %f %f %f\n",lrgb[0],lrgb[1],lrgb[2]);

	/* Convert linear light RGB to device RGB via inverse curves */
	for (j = 0; j < 3; j++) {
		lrgb[j] = x->dcvs[j]->inv_interp(x->dcvs[j], lrgb[j]);
		if (lrgb[j] < 0.0) {
			if (-lrgb[j] > clip)
				clip = -lrgb[j];
#ifdef CLIP
			lrgb[j] = 0.0;
#endif
		} else if (lrgb[j] > 1.0) {
			if ((lrgb[j]-1.0) > clip)
				clip = (lrgb[j]-1.0);
#ifdef CLIP
			lrgb[j] = 1.0;
#endif
		}
	}
//printf("~1 invdev; inverse curves rgb = %f %f %f\n",lrgb[0],lrgb[1],lrgb[2]);
	if (rgb != NULL) {
		rgb[0] = lrgb[0];
		rgb[1] = lrgb[1];
		rgb[2] = lrgb[2];
	}
	return clip;
}

/* Overall optimisation support */

/* Set the optimsation parameter number and offset values in cctx, */
/* and return an array filled in with the current parameters. */
/* Allocate temporary arrays */
static double *dev_get_params(cctx *x) {
	double *p, *tp;
	int i, j;

	x->np = 9;
	for (i = 0; i < 3; i++)
		x->np += x->dcvs[i]->luord;

	if ((p = (double *)malloc(x->np * sizeof(double))) == NULL)
		error("dev_params malloc failed");
	
	tp = p;
	
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			*tp++ = x->fm[i][j];

	for (i = 0; i < 3; i++) {
		x->co[i] = tp - p;				/* Offset to start */
		for (j = 0; j < x->dcvs[i]->luord; j++)
			*tp++ = x->dcvs[i]->pms[j];
		x->nc[i] = (tp - p) - x->co[i];	/* Number */
	}
		
	if ((x->dtin_iv = (double *)malloc(x->np * sizeof(double))) == NULL)
		error("dev_params malloc failed");

	return p;
}

/* Given a set of parameters, put them back into the model */
/* Normalize them so that the curve maximum is 1.0 too. */
static void dev_put_params(cctx *x, double *p) {
	int i, j;
	double scale[3];

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			x->fm[i][j] = *p++;
	for (i = 0; i < 3; i++)
		for (j = 0; j < x->dcvs[i]->luord; j++)
			x->dcvs[i]->pms[j] = *p++;

	/* Figure out how we have to scale the curves */
	for (j = 0; j < 3; j++) {
		scale[j] = x->dcvs[j]->interp(x->dcvs[j], 1.0);
		x->dcvs[j]->force_scale(x->dcvs[j], 1.0);
	}

	/* Scale the matrix to compensate */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			x->fm[i][j] *= scale[j];
}

/* Device model optimisation function handed to powell() */
static double dev_opt_func(void *edata, double *v) {
	cctx *x = (cctx *)edata;
	int i, j;
	double tw = 0.0;
	double rv, smv;

#ifdef NEVER
	printf("params =");
	for (i = 0; i < x->np; i++)
		printf(" %f",v[i]);
	printf("\n");
#endif

	/* For all our data points */
	rv = 0.0;
	for (i = 0; i < x->nrp; i++) {
		double lrgb[3];		/* Linear light RGB */
		double xyz[3], lab[3];
		double de;

		/* Convert through device curves */
		for (j = 0; j < 3; j++)
			lrgb[j] = x->dcvs[j]->interp_p(x->dcvs[j], v + x->co[j], x->rp[i].dev[j]);

		/* Convert linear light RGB into XYZ via the matrix */
		icxMulBy3x3Parm(xyz, v, lrgb);

		/* Convert to Lab */
		icmXYZ2Lab(&x->twN, lab, xyz);
	
		/* Compute delta E squared */
		de = icmCIE94sq(lab, x->rp[i].lab) * x->rp[i].w;
#ifdef NEVER
	printf("point %d DE %f, Lab is %f %f %f, should be %f %f %f\n",
i, sqrt(de), lab[0], lab[1], lab[2], x->rp[i].lab[0], x->rp[i].lab[1], x->rp[i].lab[2]);
#endif
		rv += de;
		tw += x->rp[i].w;
	}

	/* Normalise error to be a weighted average delta E squared and scale smoothing */
	rv /= (tw * 5.0);

	/* Sum with shaper parameters squared, to */
	/* minimise unsconstrained "wiggles" */
	smv = 0.0;
	for (j = 0; j < 3; j++)
		smv += x->dcvs[j]->shweight_p(x->dcvs[j], v + x->co[j], 1.0);
	rv += smv;

#ifdef NEVER
	printf("rv = %f (%f)\n",rv, smv);
#endif
	return rv;
}


/* Device model optimisation function handed to conjgrad() */
static double dev_dopt_func(void *edata, double *dv, double *v) {
	cctx *x = (cctx *)edata;
	int i, j, k;
	int f, ee, ff, jj;
	double tw = 0.0;
	double rv, smv;

	double dmato_mv[3][9];		/* Del in mat out due to del in matrix param vals */
	double dmato_tin[3][3];		/* Del in mat out due to del in matrix input values */
	double dout_lab[3][3];		/* Del in out due to XYZ to Lab conversion */
	double de_dout[2][3];		/* Del in delta E due to input Lab values */

#ifdef NEVER
	printf("params =");
	for (i = 0; i < x->np; i++)
		printf(" %f",v[i]);
	printf("\n");
#endif

	/* Zero the accumulated partial derivatives */
	for (i = 0; i < x->np; i++)
		dv[i] = 0.0;

	/* For all our data points */
	rv = 0.0;
	for (i = 0; i < x->nrp; i++) {
		double lrgb[3];		/* Linear light RGB */
		double xyz[3], lab[3];

		/* Apply the input channel curves */
		for (j = 0; j < 3; j++)
			lrgb[j] = x->dcvs[j]->dinterp_p(x->dcvs[j], v + x->co[j],
			                         x->dtin_iv + x->co[j], x->rp[i].dev[j]);

		/* Convert linear light RGB into XYZ via the matrix */
		icxdpdiMulBy3x3Parm(xyz, dmato_mv, dmato_tin, v, lrgb);
		
		/* Convert to Lab */
		icxdXYZ2Lab(&x->twN, lab, dout_lab, xyz);
	
		/* Compute delta E squared */
//printf("~1 point %d: Lab is %f %f %f, should be %f %f %f\n",
//i, lab[0], lab[1], lab[2], x->rp[i].lab[0], x->rp[i].lab[1], x->rp[i].lab[2]);
		rv += icxdCIE94sq(de_dout, lab, x->rp[i].lab) * x->rp[i].w;
		de_dout[0][0] *= x->rp[i].w;
		de_dout[0][1] *= x->rp[i].w;
		de_dout[0][2] *= x->rp[i].w;
		tw += x->rp[i].w;

		/* Compute and accumulate partial difference values for each parameter value */

		/* Input channel curves */
		for (ee = 0; ee < 3; ee++) {				/* Parameter input chanel */
			for (k = 0; k < x->nc[ee]; k++) {		/* Param within channel */
				double vv = 0.0;
				jj = x->co[ee] + k;					/* Overall input curve param */

				for (ff = 0; ff < 3; ff++) {		/* Lab channels */
					for (f = 0; f < 3; f++) {		/* XYZ channels */
						vv += de_dout[0][ff] * dout_lab[ff][f]
						    * dmato_tin[f][ee] * x->dtin_iv[jj];
					}
				}
				dv[jj] += vv;
			}
		}

		/* Matrix parameters */
		for (k = 0; k < 9; k++) {				/* Matrix parameter */
			double vv = 0.0;

			for (ff = 0; ff < 3; ff++) {		/* Lab channels */
				for (f = 0; f < 3; f++) {		/* XYZ channels */
					vv += de_dout[0][ff] * dout_lab[ff][f]
					    * dmato_mv[f][k];
				}
			}
			dv[k] += vv;
		}
	}

	/* Normalise error to be a weighted average delta E squared and scale smoothing */
	rv /= (tw * 1200.0);
	for (i = 0; i < x->np; i++)
		dv[i] /= (tw * 900.0);

	/* Sum with shaper parameters squared, to */
	/* minimise unsconstrained "wiggles" */
	smv = 0.0;
	for (j = 0; j < 3; j++)
		smv += x->dcvs[j]->dshweight_p(x->dcvs[j], v + x->co[j], x->dtin_iv + x->co[j], 1.0);
	rv += smv;

#ifdef NEVER
	printf("drv = %f (%f)\n",rv, smv);
#endif
	return rv;
}

#ifdef NEVER
/* Check partial derivative function within dev_opt_func() using powell() */

static double dev_opt_func(void *edata, double *v) {
	cctx *x = (cctx *)edata;
	int i;
	double dv[2000];
	double rv, drv;
	double trv;
	
	rv = dev_opt_func_(edata, v);
	drv = dev_dopt_func(edata, dv, v);

	if (fabs(rv - drv) > 1e-6) {
		printf("######## RV MISMATCH is %f should be %f ########\n",drv, rv);
		exit(0);
	}

	/* Check each parameter delta */
	for (i = 0; i < x->np; i++) {
		double del;

		v[i] += 1e-7;
		trv = dev_opt_func_(edata, v);
		v[i] -= 1e-7;
		
		/* Check that del is correct */
		del = (trv - rv)/1e-7;
		if (fabs(dv[i] - del) > 1.0) {
//printf("~1 del = %f from (trv %f - rv %f)/0.1\n",del,trv,rv);
			printf("######## EXCESSIVE at v[%d] is %f should be %f ########\n",i,dv[i],del);
			exit(0);
		}
	}
	return rv;
}
#endif

/* =================================================================== */

/* White point brightness optimization function handed to powell. */
/* Maximize brigness while staying within gamut */
static double wp_opt_func(void *edata, double *v) {
	cctx *x = (cctx *)edata;
	double wxyz[3], rgb[3];
	int j;
	double rv = 0.0;

	wxyz[0] = v[0] * x->twh[0];
	wxyz[1] = v[0] * x->twh[1];
	wxyz[2] = v[0] * x->twh[2];

//printf("~1 wp_opt_func got scale %f, xyz = %f %f %f\n",
//v[0],wxyz[0],wxyz[1],wxyz[2]);

	if ((rv = invdev(x, rgb, wxyz)) > 0.0) {		/* Out of gamut */
		rv *= 1e5;
//printf("~1 out of gamut %f %f %f returning %f\n", rgb[0], rgb[1], rgb[2], rv);
		return rv;
	}
	if (v[0] < 0.00001)
		rv = 1.0/0.00001;
	else
		rv = 1.0/v[0];
	
//printf("~1 %f %f %f returning %f\n", rgb[0], rgb[1], rgb[2], rv);
	return rv;
}

/* =================================================================== */
/* Structure to save aproximate model readings in */
typedef struct {
	double v;			/* Input value */
	double xyz[3];		/* Reading */
} sxyz;

/* ------------------------------------------------------------------- */
#if defined(__APPLE__) && defined(__POWERPC__)

/* Workaround for a ppc gcc 3.3 optimiser bug... */
/* It seems to cause a segmentation fault instead of */
/* converting an integer loop index into a float, */
/* when there are sufficient variables in play. */
static int gcc_bug_fix(int i) {
	static int nn;
	nn += i;
	return nn;
}
#endif	/* APPLE */


/* =================================================================== */
/* Calibration sample point support. This allows the successive */
/* refinement of our neutral sample points */

/* A sample point */
typedef struct {
	double v;			/* Desired input value */
	double rgb[3];		/* Input value through calibration curves */
	double tXYZ[3];		/* Target XYZ */
	double XYZ[3];		/* Read XYZ */
	double deXYZ[3];	/* Delta XYZ wanted to target */
	double de;			/* Delta Lab to neutral target */
	double dc;			/* Delta XYZ to neutral target */
	double peqde;		/* Delta Lab to previous point value (last pass) */
	double hde;			/* Hybrid de composed of de and peqde */

	double pXYZ[3];		/* Previous measured XYZ */
	double pdXYZ[3];	/* Delta XYZ intended from previous measure */
	double pdrgb[3];	/* Delta rgb made to previous */

	double dXYZ[3];		/* Actual delta XYZ resulting from previous delta rgb */

	double j[3][3];		/* Aproximate Jacobian (del RGB -> XYZ) */
	double ij[3][3];	/* Aproximate inverse Jacobian (del XYZ-> del RGB) */
	double fb_ij[3][3];	/* Copy of initial inverse Jacobian, used as a fallback */
} csp;

/* All the sample points */
typedef struct {
	int no;				/* Number of samples */
	int _no;			/* Allocation */
	csp *s;			/* List of samples */
} csamp;

static void free_alloc_csamp(csamp *p) {
	if (p->s != NULL)
		free(p->s);
	p->s = NULL;
}

/* Initialise v values */
static void init_csamp_v(csamp *p, cctx *x, int psrand) {
	int i, j;
	sobol *so = NULL;

	if (psrand != 0) {	/* Use pseudo random distribution for verification */
		if ((so = new_sobol(1)) == NULL)
			error("New sobol failed");
	}
	
	/* Generate the sample points */
	for (i = 0; i < p->no; i++) {
		double vv;

#if defined(__APPLE__) && defined(__POWERPC__)
		gcc_bug_fix(i);
#endif
		if (so != NULL) {
			if (i == 0)
				vv = 1.0;
			else if (i == 1)
				vv = 0.0;
			else
				so->next(so, &vv);
		} else
			vv = i/(p->no - 1.0);
		vv = pow(vv, REFN_DIST_POW);	/* Skew sample points to be slightly perceptual */
		p->s[i].v = vv;
	}

	if (so != NULL) {
		/* Sort it so white is last */
#define HEAP_COMPARE(A,B) (A.v < B.v) 
	HEAPSORT(csp,p->s,p->no)
#undef HEAP_COMPARE
		so->del(so);
	}
}

/* Initialise txyz values from v values */
static void init_csamp_txyz(csamp *p, cctx *x, int fixdev) {
	int i, j;
	double tbL[3];		/* tbk as Lab */

	/* Convert target black from XYZ to Lab here, */
	/* in case twN has changed at some point. */
	icmXYZ2Lab(&x->twN, tbL, x->tbk);

	/* Set the sample points targets */
	for (i = 0; i < p->no; i++) {
		double y, vv;
		double XYZ[3];			/* Existing XYZ value */
		double Lab[3];
		double bl;

		vv = p->s[i].v;

		/* Compute target relative Y value for this device input. */
		/* We allow for any input and/or output offset */
		y = x->gooff + dev2Y(x, x->egamma, x->gioff + vv * (1.0 - x->gioff)) * (1.0 - x->gooff);

		/* Add viewing environment transform */
		y = view_xform(x, y);

		/* Convert Y to L* */
		Lab[0] = icmY2L(y);
		Lab[1] = Lab[2] = 0.0;	/* Target is neutral */

		/* Compute blended neutral target a* b* */
		bl = pow((1.0 - vv), x->nbrate);		/* Crossover near the black */
		Lab[1] = bl * tbL[1];
		Lab[2] = bl * tbL[2];

		icmAry2Ary(XYZ, p->s[i].tXYZ);				/* Save the existing values */
		icmLab2XYZ(&x->twN, p->s[i].tXYZ, Lab);		/* New XYZ Value to aim for */

#ifdef DEBUG
		printf("%d: target XYZ %.2f %.2f %.2f, Lab %.2f %.2f %.2f\n",i, p->s[i].tXYZ[0],p->s[i].tXYZ[1],p->s[i].tXYZ[2], Lab[0],Lab[1],Lab[2]);
#endif
	}
}


/* Allocate the sample points and initialise them with the */
/* target device and XYZ values, and first cut device values. */
static void init_csamp(csamp *p, cctx *x, int doupdate, int verify, int psrand, int no) {
	int i, j;
	
	p->_no = p->no = no;

	if ((p->s = (csp *)malloc(p->_no * sizeof(csp))) == NULL)
		error("csamp malloc failed");

	/* Compute v and txyz */
	init_csamp_v(p, x, psrand);
	init_csamp_txyz(p, x, 0);

	/* Generate the sample points */
	for (i = 0; i < no; i++) {
		double dd, vv;

#if defined(__APPLE__) && defined(__POWERPC__)
		gcc_bug_fix(i);
#endif
		vv = p->s[i].v;

		if (verify == 2) {		/* Verifying through installed curve */
			/* Make RGB values the input value */
			p->s[i].rgb[0] = p->s[i].rgb[1] = p->s[i].rgb[2] = vv;

		} else if (doupdate) {	/* Start or verify through current cal curves */
			for (j = 0; j < 3; j++) {
				p->s[i].rgb[j] = x->rdac[j]->interp(x->rdac[j], vv);
#ifdef CLIP
				if (p->s[i].rgb[j] < 0.0)
					p->s[i].rgb[j] = 0.0;
				else if (p->s[i].rgb[j] > 1.0)
					p->s[i].rgb[j] = 1.0;
#endif
			}
		} else {		/* we have model */
			/* Lookup an initial device RGB for that target by inverting */
			/* the approximate forward device model */
			p->s[i].rgb[0] = p->s[i].rgb[1] = p->s[i].rgb[2] = vv;
			invdev(x, p->s[i].rgb, p->s[i].tXYZ);
		}
		/* Force white to be native if native flag set */
		if (x->nat && i == (no-1)) {
//printf("~1 Forcing white rgb to be 1,1,1\n");
			p->s[i].rgb[0] = p->s[i].rgb[1] = p->s[i].rgb[2] = 1.0;
		}
		
//printf("~1 Inital point %d rgb %f %f %f\n",i,p->s[i].rgb[0],p->s[i].rgb[1],p->s[i].rgb[2]);

		/* Compute the approximate inverse Jacobian at this point */
		/* by taking the partial derivatives wrt to each device */
		/* channel of our aproximate forward model */
		if (verify != 2) {
			double refXYZ[3], delXYZ[3];
			fwddev(x, refXYZ, p->s[i].rgb);
			if (vv < 0.5)
				dd = 0.02;
			else
				dd = -0.02;
			/* Matrix organization is J[XYZ][RGB] for del RGB->del XYZ*/
			for (j = 0; j < 3; j++) {
				p->s[i].rgb[j] += dd;
				fwddev(x, delXYZ, p->s[i].rgb);
				p->s[i].j[0][j] = (delXYZ[0] - refXYZ[0]) / dd;
				p->s[i].j[1][j] = (delXYZ[1] - refXYZ[1]) / dd;
				p->s[i].j[2][j] = (delXYZ[2] - refXYZ[2]) / dd;
				p->s[i].rgb[j] -= dd;
			}
			if (icmInverse3x3(p->s[i].ij, p->s[i].j)) {
				error("dispcal: inverting Jacobian failed (1)");
			}
			/* Make a copy of this Jacobian in case we get an invert failure later */
			icmCpy3x3(p->s[i].fb_ij, p->s[i].ij);
		}
	}
}

/* Return a linear XYZ interpolation */
static void csamp_interp(csamp *p, double xyz[3], double v) {
	int i, j;
	double b;

	if (p->no < 2)
		error("Calling csamp_interp with less than two existing samples");

	/* Locate the pair surrounding our input value */
	for (i = 0; i < (p->no-1); i++) {
		if (v >= p->s[i].v && v <= p->s[i+1].v)
			break;
	}
	if (i >= (p->no-1))
		error("csamp_interp out of range");
	
	b = (v - p->s[i].v)/(p->s[i+1].v - p->s[i].v);

	for (j = 0; j < 3; j++) {
		xyz[j] = b * p->s[i+1].XYZ[j] + (1.0 - b) * p->s[i].XYZ[j];
	}
}

/* Re-initialise a CSP with a new number of points. */
/* Interpolate the device values and jacobian. */
/* Set the current rgb from the current RAMDAC curves if not verifying */
static void reinit_csamp(csamp *p, cctx *x, int verify, int psrand, int no) {
	csp *os;			/* Old list of samples */
	int ono;			/* Old number of samples */
	int i, j, k, m;
	
	if (no == p->no)
		return;			/* Nothing has changed */

	os = p->s;			/* Save the existing per point information */
	ono = p->no;

	init_csamp(p, x, 0, 2, psrand, no);

	p->_no = p->no = no;

	/* Interpolate the current device values */
	for (i = 0; i < no; i++) {
		double vv, b;

		vv = p->s[i].v;

		/* Locate the pair surrounding our target value */
		for (j = 0; j < ono-1; j++) {
			if (vv >= os[j].v && vv <= os[j+1].v)
				break;
		}
		if (j >= (ono-1))
			error("csamp interp. out of range");
		
		b = (vv - os[j].v)/(os[j+1].v - os[j].v);
	
		for (k = 0; k < 3; k++) {
			if (verify == 2) {

				p->s[i].rgb[k] = b * os[j+1].rgb[k] + (1.0 - b) * os[j].rgb[k];

			} else {	/* Lookup rgb from current calibration curves */
				for (m = 0; m < 3; m++) {
					p->s[i].rgb[m] = x->rdac[m]->interp(x->rdac[m], vv);
#ifdef CLIP
					if (p->s[i].rgb[m] < 0.0)
						p->s[i].rgb[m] = 0.0;
					else if (p->s[i].rgb[m] > 1.0)
						p->s[i].rgb[m] = 1.0;
#endif
				}
			}
			p->s[i].XYZ[k] = b * os[j+1].XYZ[k] + (1.0 - b) * os[j].XYZ[k];
			p->s[i].deXYZ[k] = b * os[j+1].deXYZ[k] + (1.0 - b) * os[j].deXYZ[k];
			p->s[i].pXYZ[k] = b * os[j+1].pXYZ[k] + (1.0 - b) * os[j].pXYZ[k];
			p->s[i].pdrgb[k] = b * os[j+1].pdrgb[k] + (1.0 - b) * os[j].pdrgb[k];
			p->s[i].dXYZ[k] = b * os[j+1].dXYZ[k] + (1.0 - b) * os[j].dXYZ[k];
#ifdef INTERP_JAC
			for (m = 0; m < 3; m++) 
				p->s[i].j[k][m] = b * os[j+1].j[k][m] + (1.0 - b) * os[j].j[k][m];
#endif

		}
#ifndef INTERP_JAC
		/* Create a Jacobian at this location from our forward model */
		{
			double dd, refXYZ[3], delXYZ[3];
			fwddev(x, refXYZ, p->s[i].rgb);
			if (vv < 0.5)
				dd = 0.02;
			else
				dd = -0.02;
			/* Matrix organization is J[XYZ][RGB] for del RGB->del XYZ*/
			for (j = 0; j < 3; j++) {
				p->s[i].rgb[j] += dd;
				fwddev(x, delXYZ, p->s[i].rgb);
				p->s[i].j[0][j] = (delXYZ[0] - refXYZ[0]) / dd;
				p->s[i].j[1][j] = (delXYZ[1] - refXYZ[1]) / dd;
				p->s[i].j[2][j] = (delXYZ[2] - refXYZ[2]) / dd;
				p->s[i].rgb[j] -= dd;
			}
		}
#endif
		if (icmInverse3x3(p->s[i].ij, p->s[i].j)) {
			error("dispcal: inverting Jacobian failed (2)");
		}
		/* Make a copy of this Jacobian in case we get an invert failure later */
		icmCpy3x3(p->s[i].fb_ij, p->s[i].ij);

		/* Compute expected delta XYZ using new Jacobian */
		icmMulBy3x3(p->s[i].pdXYZ, p->s[i].j, p->s[i].pdrgb);

		p->s[i].de = b * os[j+1].de + (1.0 - b) * os[j].de;
		p->s[i].dc = b * os[j+1].dc + (1.0 - b) * os[j].dc;
	}

	free(os);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef NEVER
/* Do a linear interp of the ramdac */
static void interp_ramdac(double cal[CAL_RES][3], double drgb[3], double rgb[3]) {
	int i, j;
	int gres = CAL_RES;
	double w;

	/* For r,g & b */
	for (j = 0; j < 3; j++) {
		int mi, gres_1 = gres-1;
		double t, vv = rgb[j];
		t = gres * vv;
		mi = (int)floor(t);			/* Grid coordinate */
		if (mi < 0)					/* Limit to valid cube base index range */
			mi = 0;
		else if (mi >= gres_1)
			mi = gres_1-1;
		w = t - (double)mi;	 		/* 1.0 - weight */

		drgb[j] = (1.0 - w) * cal[mi][j] + w * cal[mi+1][j];
	}
}
#endif	/* NEVER */

/* Given an XYZ, compute the color temperature and the delta E 2K to the locus */
static double comp_ct(
	double *de,		/* If non-NULL, return CIEDE2000 to locus */
	double lxyz[3],	/* If non-NULL, return normalised XYZ on locus */
	int plank,		/* NZ if Plankian locus, 0 if Daylight locus */
	int dovct,		/* NZ if visual match, 0 if traditional correlation */
	icxObserverType obType,		/* If not default, set a custom observer */
	double xyz[3]	/* Color to match */
) {
	double ct_xyz[3];		/* XYZ on locus */
	double nxyz[3];			/* Normalised input color */
	double ct, ctde;		/* Color temperature & delta E to Black Body locus */
	icmXYZNumber wN;
	

	if (obType == icxOT_default)
		obType = icxOT_CIE_1931_2;

	if ((ct = icx_XYZ2ill_ct(ct_xyz, plank != 0 ? icxIT_Ptemp : icxIT_Dtemp,
	                         obType, NULL, xyz, NULL, dovct)) < 0)
		error ("Got bad color temperature conversion\n");

	if (de != NULL) {
		icmAry2XYZ(wN, ct_xyz);
		icmAry2Ary(nxyz, xyz);
		nxyz[0] /= xyz[1];
		nxyz[2] /= xyz[1];
		nxyz[1] /= xyz[1];
		ctde = icmXYZCIE2K(&wN, nxyz, ct_xyz);
		*de = ctde;
	}
	if (lxyz != NULL) {
		icmAry2Ary(lxyz, ct_xyz);
	}
	return ct;
}
	
/* =================================================================== */

/* Default gamma */
double g_def_gamma = 2.4;

void usage(iccss *cl, char *diag, ...) {
	int i;
	disppath **dp;
	icoms *icom;

	fprintf(stderr,"Calibrate a Display, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	if (setup_spyd2() == 2)
		fprintf(stderr,"WARNING: This file contains a proprietary firmware image, and may not be freely distributed !\n");
	if (diag != NULL) {
		va_list args;
		fprintf(stderr,"Diagnostic: ");
		va_start(args, diag);
		vfprintf(stderr, diag, args);
		va_end(args);
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"usage: dispcal [options] outfile\n");
	fprintf(stderr," -v [n]               Verbose mode\n");
#if defined(UNIX) && !defined(__APPLE__)
	fprintf(stderr," -display displayname Choose X11 display name\n");
	fprintf(stderr," -d n[,m]             Choose the display n from the following list (default 1)\n");
	fprintf(stderr,"                      Optionally choose different display m for VideoLUT access\n"); 
#else
	fprintf(stderr," -d n                 Choose the display from the following list (default 1)\n");
#endif
//	fprintf(stderr," -d fake              Use a fake display device for testing, fake%s if present\n",ICC_FILE_EXT);
	dp = get_displays();
	if (dp == NULL || dp[0] == NULL)
		fprintf(stderr,"    ** No displays found **\n");
	else {
		int i;
		for (i = 0; ; i++) {
			if (dp[i] == NULL)
				break;
			fprintf(stderr,"    %d = '%s'\n",i+1,dp[i]->description);
		}
	}
	free_disppaths(dp);
	fprintf(stderr," -c listno            Set communication port from the following list (default %d)\n",COMPORT);
	if ((icom = new_icoms()) != NULL) {
		icompath **paths;
		if ((paths = icom->get_paths(icom)) != NULL) {
			int i;
			for (i = 0; ; i++) {
				if (paths[i] == NULL)
					break;
				if (strlen(paths[i]->path) >= 8
				  && strcmp(paths[i]->path+strlen(paths[i]->path)-8, "Spyder2)") == 0
				  && setup_spyd2() == 0)
					fprintf(stderr,"    %d = '%s' !! Disabled - no firmware !!\n",i+1,paths[i]->path);
				else
					fprintf(stderr,"    %d = '%s'\n",i+1,paths[i]->path);
			}
		} else
			fprintf(stderr,"    ** No ports found **\n");
		icom->del(icom);
	}
	fprintf(stderr," -r                   Report on the calibrated display then exit\n");
	fprintf(stderr," -R                   Report on the uncalibrated display then exit\n");
	fprintf(stderr," -m                   Skip adjustment of the monitor controls\n");
	fprintf(stderr," -o [profile%s]     Create fast matrix/shaper profile [different filename to outfile%s]\n",ICC_FILE_EXT,ICC_FILE_EXT);
	fprintf(stderr," -O \"description\"     Fast ICC Profile Description string (Default \"outfile\")\n");
	fprintf(stderr," -u                   Update previous calibration and (if -o used) ICC profile VideoLUTs\n");
	fprintf(stderr," -q [lmh]             Quality - Low, Medium (def), High\n");
	fprintf(stderr," -p                   Use projector mode (if available)\n");
	fprintf(stderr," -y c|l               Display type, c = CRT, l = LCD\n");
	fprintf(stderr," -t [temp]            White Daylight locus target, optional target temperaturee in deg. K (deflt.)\n");
	fprintf(stderr," -T [temp]            White Black Body locus target, optional target temperaturee in deg. K\n");
	fprintf(stderr," -w x,y        	      Set the target white point as chromaticity coordinates\n");
#ifdef NEVER	/* Not worth confusing people about this ? */
	fprintf(stderr," -L                   Show CCT/CDT rather than VCT/VDT during native white point adjustment\n");
#endif
	fprintf(stderr," -b bright            Set the target white brightness in cd/m^2\n");
	fprintf(stderr," -g gamma             Set the target response curve advertised gamma (Def. %3.1f)\n",g_def_gamma);
	fprintf(stderr,"                      Use \"-gl\" for L*a*b* curve\n");
	fprintf(stderr,"                      Use \"-gs\" for sRGB curve\n");
	fprintf(stderr,"                      Use \"-g709\" for REC 709 curve (should use -a as well!)\n");
	fprintf(stderr,"                      Use \"-g240\" for SMPTE 240M curve (should use -a as well!)\n");
	fprintf(stderr," -G gamma             Set the target response curve actual technical gamma\n");
	fprintf(stderr," -f [degree]          Amount of black level accounted for with output offset (default all output offset)\n");
	fprintf(stderr," -a ambient           Use viewing condition adjustment for ambient in Lux\n");
	fprintf(stderr," -k factor            Amount to try and correct black point hue. Default 1.0, LCD default 0.0\n");
	fprintf(stderr," -A rate              Rate of blending from neutral to black point. Default %.1f\n",NEUTRAL_BLEND_RATE);
	fprintf(stderr," -B blkbright         Set the target black brightness in cd/m^2\n");
	fprintf(stderr," -e [n]               Run n verify passes on final curves\n");
	fprintf(stderr," -E                   Run only verify pass on installed calibration curves\n");
	fprintf(stderr," -P ho,vo,ss          Position test window and scale it\n");
	fprintf(stderr,"                      ho,vi: 0.0 = left/top, 0.5 = center, 1.0 = right/bottom etc.\n");
	fprintf(stderr,"                      ss: 0.5 = half, 1.0 = normal, 2.0 = double etc.\n");
	fprintf(stderr," -F                   Fill whole screen with black background\n");
	fprintf(stderr," -K                   Don't use VideoLUT to set high precision test values\n");
#if defined(UNIX) && !defined(__APPLE__)
	fprintf(stderr," -n                   Don't set override redirect on test window\n");
#endif
	fprintf(stderr," -J                   Run instrument calibration first (used rarely)\n");
	fprintf(stderr," -N                   Disable auto calibration of instrument\n");
	fprintf(stderr," -H                   Use high resolution spectrum mode (if available)\n");
	fprintf(stderr," -V                   Use adaptive measurement mode (if available)\n");
	fprintf(stderr," -X file.ccmx         Apply Colorimeter Correction Matrix\n");
	fprintf(stderr," -X file.ccss         Use Colorimeter Calibration Spectral Samples for calibration\n");
	for (i = 0; cl != NULL && cl[i].desc != NULL; i++) {
		if (i == 0)
			fprintf(stderr," -X N                  0: %s\n",cl[i].desc); 
		else
			fprintf(stderr,"                       %d: %s\n",i,cl[i].desc); 
	}
	free_iccss(cl);
	fprintf(stderr," -Q observ            Choose CIE Observer for spectrometer or CCSS colorimeter data:\n");
	fprintf(stderr,"                      1931_2 (def), 1964_10, S&B 1955_2, shaw, J&V 1978_2, 1964_10c\n");
	fprintf(stderr," -I b|w               Drift compensation, Black: -Ib, White: -Iw, Both: -Ibw\n");
	fprintf(stderr," -C \"command\"         Invoke shell \"command\" each time a color is set\n");
	fprintf(stderr," -M \"command\"         Invoke shell \"command\" each time a color is measured\n");
	fprintf(stderr," -W n|h|x             Override serial port flow control: n = none, h = HW, x = Xon/Xoff\n");
	fprintf(stderr," -D [level]           Print debug diagnostics to stderr\n");
	fprintf(stderr," inoutfile            Base name for created or updated .cal and %s output files\n",ICC_FILE_EXT);
	exit(1);
}

int main(int argc, char *argv[]) {
	int i, j, k;
	int fa, nfa, mfa;					/* current argument we're looking at */
	disppath *disp = NULL;				/* Display being used */
	double patsize = 100.0;				/* size of displayed color test patch */
	double patscale = 1.0;				/* scale factor for test patch size */
	double ho = 0.0, vo = 0.0;			/* Test window offsets, -1.0 to 1.0 */
	int blackbg = 0;            		/* NZ if whole screen should be filled with black */
	int verb = 0;
	int debug = 0;
	int fake = 0;						/* Use the fake device for testing */
	int override = 1;					/* Override redirect on X11 */
	int docalib = 0;					/* Do a manual instrument calibration */
	int doreport = 0;					/* 1 = Report the current uncalibrated display response */
										/* 2 = Report the current calibrated display response */
	int docontrols = 1;					/* Do adjustment of the display controls */
	int doprofile = 0;					/* Create/update ICC profile */
	char *profDesc = NULL;				/* Created profile description string */
	char *copyright = NULL;				/* Copyright string */
	char *deviceMfgDesc = NULL;			/* Device manufacturer string */
	char *modelDesc = NULL;				/* Device model description string */
	int doupdate = 0;				    /* Do an update rather than a fresh calbration */
	int comport = COMPORT;				/* COM port used */
	flow_control fc = fc_nc;			/* Default flow control */
	instType itype = instUnknown;		/* Default target instrument - none */
	int dtype = 0;						/* Display kind, 0 = default, 1 = CRT, 2 = LCD */
	int proj  = 0;						/* nz if projector mode */
	int nocal = 0;						/* Disable auto calibration */
	int highres = 0;					/* Use high res mode if available */
	int adaptive = 0;					/* Use adaptive mode if available */
	int bdrift = 0;						/* Flag, nz for black drift compensation */
	int wdrift = 0;						/* Flag, nz for white drift compensation */
	double temp = 0.0;					/* Color temperature (0 = native) */
	int planckian = 0;					/* 0 = Daylight, 1 = Planckian color locus */
	int dovct = 1;						/* Show VXT rather than CXT for adjusting white point */
	double wpx = 0.0, wpy = 0.0;		/* White point xy (native) */
	double tbright = 0.0;				/* Target white brightness ( 0.0 == max)  */
	double gamma = 0.0;					/* Advertised Gamma target */
	double egamma = 0.0;				/* Effective Gamma target, NZ if set */
	double ambient = 0.0;				/* NZ if viewing cond. adjustment to be used (cd/m^2) */
	double bkcorrect = -1.0;			/* Level of black point correction */ 
	double bkbright = 0.0;				/* Target black brightness ( 0.0 == min)  */
	int quality = -99;					/* Quality level, -2 = v, -1 = l, 0 = m, 1 = h, 2 = u */
	int isteps = 22;					/* Initial measurement steps/3 (medium) */
	int rsteps = 64;					/* Refinement measurement steps (medium) */
	double errthr = 1.5;				/* Error threshold for refinement steps (medium) */
	int thrfail = 0;					/* Set to NZ if failed to meet threshold target */
	double failerr = 0.0;				/* Delta E of worst failed target */
	int mxits = 3;						/* maximum iterations (medium) */
	int verify = 0;						/* Do a verify after last refinement, 2 = do only verify. */
	int nver = 0;						/* Number of verify passes after refinement */
	char *ccallout = NULL;				/* Change color Shell callout */
	char *mcallout = NULL;				/* Measure color Shell callout */
	char outname[MAXNAMEL+1] = { 0 };	/* Output cgats file base name */
	char iccoutname[MAXNAMEL+1] = { 0 };/* Output icc file base name */
	char ccxxname[MAXNAMEL+1] = "\000";  /* CCMX or CCSS file name */
	iccss *cl = NULL;					/* List of installed CCSS files */
	int ncl = 0;						/* Number of them */
	ccmx *cmx = NULL;					/* Colorimeter Correction Matrix */
	ccss *ccs = NULL;					/* Colorimeter Calibration Spectral Samples */
	int spec = 0;						/* Want spectral data from instrument */
	icxObserverType obType = icxOT_default;
	disprd *dr = NULL;					/* Display patch read object */
	csamp asgrey;						/* Main calibration loop test points */
	double dispLum = 0.0;				/* Display luminence reading */
	int it;								/* verify & refine iteration */
	int rv;
	int fitord = 30;					/* More seems to make curves smoother */
	int native = 1;						/* 0 = use current current or given calibration curve */
										/* 1 = set native linear op and use ramdac high prec'n */
										/* 2 = set native linear output */
	int errc;							/* Return value from new_disprd() */
	cctx x;								/* Context for calibration solution */

	set_exe_path(argv[0]);				/* Set global exe_path and error_program */
	check_if_not_interactive();

#if defined(__APPLE__)
	{
		SInt32 MacVers;

		/* Hmm. Maybe this should actually be 1.72 ?? */
		g_def_gamma = 1.8;

		/* OS X 10.6 uses a nominal gamma of 2.2 */
		if (Gestalt(gestaltSystemVersion, &MacVers) == noErr) {
			if (MacVers >= 0x1060) {
				g_def_gamma = 2.4;
			}
		}
	}
#else
	g_def_gamma = 2.4;		/* Typical CRT gamma */
#endif
	gamma = g_def_gamma;

	setup_spyd2();					/* Load firware if available */

	x.gammat = gt_power ;				/* Default gamma type */
	x.egamma = 0.0;						/* Default effective gamma none */
	x.oofff = 1.0;						/* Default is all output ofset */
	x.vc = 0;							/* No viewing conditions adjustment */
	x.svc = NULL;
	x.dvc = NULL;
	x.nbrate = NEUTRAL_BLEND_RATE;		/* Rate of blending from black point to neutral axis */

#ifdef DEBUG_OFFSET
	ho = 0.8;
	vo = -0.8;
#endif

#if defined(DEBUG) || defined(DEBUG_OFFSET) || defined(DEBUG_PLOT)
	printf("!!!!!! Debug turned on !!!!!!\n");
#endif

	/* Get a list of installed CCSS files */
	cl = list_iccss(&ncl);

	if (argc <= 1)
		usage(cl,"Too few arguments");

	/* Process the arguments */
	mfa = 1;        /* Minimum final arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')		/* Look for any flags */
			{
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1+mfa) < argc) {
					if (argv[fa+1][0] != '-')
						{
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?' || argv[fa][1] == '-') {
				usage(cl,"Usage requested");

			} else if (argv[fa][1] == 'v') {
				verb = 1;
				if (na != NULL && na[0] >= '0' && na[0] <= '9') {
					verb = atoi(na);
					fa = nfa;
				}

			/* Display number */
			} else if (argv[fa][1] == 'd') {
#if defined(UNIX) && !defined(__APPLE__)
				int ix, iv;

				if (strcmp(&argv[fa][2], "isplay") == 0 || strcmp(&argv[fa][2], "ISPLAY") == 0) {
					if (++fa >= argc || argv[fa][0] == '-') usage(cl,"Parameter expected following -display");
					setenv("DISPLAY", argv[fa], 1);
				} else {
					if (na == NULL) usage(cl,"Parameter expected following -d");
					fa = nfa;
					if (strcmp(na,"fake") == 0) {
						fake = 1;
					} else {
						if (sscanf(na, "%d,%d",&ix,&iv) != 2) {
							ix = atoi(na);
							iv = 0;
						}
						if (disp != NULL)
							free_a_disppath(disp);
						if ((disp = get_a_display(ix-1)) == NULL)
							usage(cl,"-d parameter %d out of range",ix);
						if (iv > 0)
							disp->rscreen = iv-1;
					}
				}
#else
				int ix;
				if (na == NULL) usage(cl,"Parameter expected following -d");
				fa = nfa;
				if (strcmp(na,"fake") == 0) {
					fake = 1;
				} else {
					ix = atoi(na);
					if (disp != NULL)
						free_a_disppath(disp);
					if ((disp = get_a_display(ix-1)) == NULL)
						usage(cl,"-d parameter %d out of range",ix);
				}
#endif

			} else if (argv[fa][1] == 'J') {
				docalib = 1;

			} else if (argv[fa][1] == 'N') {
				nocal = 1;

			/* High res mode */
			} else if (argv[fa][1] == 'H') {
				highres = 1;

			/* Adaptive mode */
			} else if (argv[fa][1] == 'V') {
				adaptive = 1;

			/* Colorimeter Correction Matrix */
			/* or Colorimeter Calibration Spectral Samples */
			} else if (argv[fa][1] == 'X') {
				int ix;
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expected following -X");
				if (ncl > 0 && sscanf(na, " %d ", &ix) == 1) {
					if (ix < 0 || ix > ncl)
						usage(cl, "-X ccss selection is out of range");
					strncpy(ccxxname,cl[ix].path,MAXNAMEL-1); ccxxname[MAXNAMEL-1] = '\000';
				} else {
					strncpy(ccxxname,na,MAXNAMEL-1); ccxxname[MAXNAMEL-1] = '\000';
				}

			/* Drift Compensation */
			} else if (argv[fa][1] == 'I') {
				fa = nfa;
				if (na == NULL || na[0] == '\000') usage(cl,"Parameter expected after -I");
				for (i=0; ; i++) {
					if (na[i] == '\000')
						break;
					if (na[i] == 'b' || na[i] == 'B')
						bdrift = 1;
					else if (na[i] == 'w' || na[i] == 'W')
						wdrift = 1;
					else
						usage(cl,"-I parameter '%c' not recognised",na[i]);
				}

			/* Spectral Observer type */
			} else if (argv[fa][1] == 'Q') {
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expecte after -Q");
				if (strcmp(na, "1931_2") == 0) {			/* Classic 2 degree */
					obType = icxOT_CIE_1931_2;
				} else if (strcmp(na, "1964_10") == 0) {	/* Classic 10 degree */
					obType = icxOT_CIE_1964_10;
				} else if (strcmp(na, "1964_10c") == 0) {	/* 10 degree corrected */
					obType = icxOT_CIE_1964_10c;
				} else if (strcmp(na, "1955_2") == 0) {		/* Stiles and Burch 1955 2 degree */
					obType = icxOT_Stiles_Burch_2;
				} else if (strcmp(na, "1978_2") == 0) {		/* Judd and Voss 1978 2 degree */
					obType = icxOT_Judd_Voss_2;
				} else if (strcmp(na, "shaw") == 0) {		/* Shaw and Fairchilds 1997 2 degree */
					obType = icxOT_Shaw_Fairchild_2;
				} else
					usage(cl,"-Q parameter '%s' not recognised",na);

			/* Change color callout */
			} else if (argv[fa][1] == 'C') {
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expected after -C");
				ccallout = na;

			/* Measure color callout */
			} else if (argv[fa][1] == 'M') {
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expected after -M");
				mcallout = na;

			/* Serial port flow control */
			} else if (argv[fa][1] == 'W') {
				fa = nfa;
				if (na == NULL) usage(cl,"Paramater expected following -W");
				if (na[0] == 'n' || na[0] == 'N')
					fc = fc_none;
				else if (na[0] == 'h' || na[0] == 'H')
					fc = fc_Hardware;
				else if (na[0] == 'x' || na[0] == 'X')
					fc = fc_XonXOff;
				else
					usage(cl,"-W parameter '%c' not recognised",na[0]);

			/* Debug coms */
			} else if (argv[fa][1] == 'D') {
				debug = 1;
				if (na != NULL && na[0] >= '0' && na[0] <= '9') {
					debug = atoi(na);
					fa = nfa;
				}
				callback_ddebug = 1;		/* dispwin global */

			/* Black point correction amount */
			} else if (argv[fa][1] == 'k') {
				fa = nfa;
				if (na == NULL) usage(cl,"Paramater expected following -k");
				bkcorrect = atof(na);
				if (bkcorrect < 0.0 || bkcorrect > 1.0) usage(cl,"-k parameter must be between 0.0 and 1.0");
			/* Neutral blend rate (power) */
			} else if (argv[fa][1] == 'A') {
				fa = nfa;
				if (na == NULL) usage(cl,"Paramater expected following -A");
				x.nbrate = atof(na);
				if (x.nbrate < 0.05 || x.nbrate > 20.0) usage(cl,"-A parameter must be between 0.05 and 20.0");
			/* Black brightness */
			} else if (argv[fa][1] == 'B') {
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expected after -B");
				bkbright = atof(na);
				if (bkbright <= 0.0 || bkbright > 100000.0) usage(cl,"-B parameter %f out of range",bkbright);

			/* Number of verify passes */
			} else if (argv[fa][1] == 'e') {
				verify = 1;
				nver = 1;
				if (na != NULL && na[0] >= '0' && na[0] <= '9') {
					nver = atoi(na);
					fa = nfa;
				}

			} else if (argv[fa][1] == 'E') {
				verify = 2;
				mfa = 0;

#if defined(UNIX) && !defined(__APPLE__)
			} else if (argv[fa][1] == 'n') {
				override = 0;
#endif /* UNIX */
			/* COM port  */
			} else if (argv[fa][1] == 'c') {
				fa = nfa;
				if (na == NULL) usage(cl,"Paramater expected following -c");
				comport = atoi(na);
				if (comport < 1 || comport > 50) usage(cl,"-c parameter %d out of range",comport);

			} else if (argv[fa][1] == 'p') {
				proj = 1;
				
			} else if (argv[fa][1] == 'r' || argv[fa][1] == 'R') {
				if (argv[fa][1] == 'R')
					doreport = 1;		/* raw */
				else
					doreport = 2;		/* Calibrated */
				mfa = 0;

			} else if (argv[fa][1] == 'm' || argv[fa][1] == 'M') {
				docontrols = 0;

			/* Output/update ICC profile [optional different name] */
			} else if (argv[fa][1] == 'o') {
				doprofile = 1;

				if (na != NULL) {	/* Found an optional icc profile name */
					fa = nfa;
					strncpy(iccoutname,na,MAXNAMEL); iccoutname[MAXNAMEL] = '\000';
				}

			/* Fast Profile Description */
			} else if (argv[fa][1] == 'O') {
				fa = nfa;
				if (na == NULL) usage(cl,"Expect argument to profile description flag -O");
				profDesc = na;

			/* Update calibration and (optionally) profile */
			} else if (argv[fa][1] == 'u') {
				doupdate = 1;
				docontrols = 0;

			/* Quality */
			} else if (argv[fa][1] == 'q') {
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expected following -q");
    			switch (na[0]) {
					case 'L':			/* Test value */
						quality = -3;
						break;
					case 'v':
						quality = -2;
						break;
					case 'l':
						quality = -1;
						break;
					case 'm':
					case 'M':
						quality = 0;
						break;
					case 'h':
					case 'H':
						quality = 1;
						break;
					case 'u':
					case 'U':
						quality = 2;
						break;
					default:
						usage(cl,"-q parameter '%c' not recognised",na[0]);
				}

			/* Display type */
			} else if (argv[fa][1] == 'y') {
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expected after -y");
				if (na[0] == 'c' || na[9] == 'C')
					dtype = 1;
				else if (na[0] == 'l' || na[0] == 'L')
					dtype = 2;
				else
					usage(cl,"-y parameter '%c' not recognised",na[0]);

			/* Daylight color temperature */
			} else if (argv[fa][1] == 't' || argv[fa][1] == 'T') {
				if (argv[fa][1] == 'T')
					planckian = 1;
				else
					planckian = 0;
				if (na != NULL) {
					fa = nfa;
					temp = atof(na);
					if (temp < 1000.0 || temp > 15000.0) usage(cl,"-%c parameter %f out of range",argv[fa][1], temp);
				}

			/* White point as x, y */
			} else if (argv[fa][1] == 'w') {
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expected after -w");
				if (sscanf(na, " %lf,%lf ", &wpx, &wpy) != 2)
					usage(cl,"-w parameter '%s' not recognised",na);

			/* Show CXT rather than VXT when adjusting native white point */
			} else if (argv[fa][1] == 'L') {
				dovct = 0;

			/* White brightness */
			} else if (argv[fa][1] == 'b') {
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expected after -b");
				tbright = atof(na);
				if (tbright <= 0.0 || tbright > 100000.0) usage(cl,"-b parameter %f out of range",tbright);

			/* Target transfer curve */
			} else if (argv[fa][1] == 'g') {
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expected after -g");
				if ((na[0] == 'l' || na[0] == 'L') && na[1] == '\000')
					x.gammat = gt_Lab;
				else if ((na[0] == 's' || na[0] == 'S') && na[1] == '\000')
					x.gammat = gt_sRGB;
				else if (strcmp(na, "709") == 0)
					x.gammat = gt_Rec709;
				else if (strcmp(na, "240") == 0)
					x.gammat = gt_SMPTE240M;
				else {
					gamma = atof(na);
					if (gamma <= 0.0 || gamma > 10.0) usage(cl,"-g parameter %f out of range",gamma);
					x.gammat = gt_power;
				}

			/* Effective gamma power */
			} else if (argv[fa][1] == 'G') {
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expected after -G");
				egamma = atof(na);
				if (egamma <= 0.0 || egamma > 10.0) usage(cl,"-G parameter %f out of range",egamma);
				x.gammat = gt_power;

			/* Degree of output offset */
			} else if (argv[fa][1] == 'f') {
				fa = nfa;
				if (na == NULL) {
					x.oofff = 0.0;
				} else {
					x.oofff = atof(na);
					if (x.oofff < 0.0 || x.oofff > 1.0)
						usage(cl,"-f parameter %f out of range",x.oofff);
				}

			/* Ambient light level */
			} else if (argv[fa][1] == 'a') {
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expected after -a");
				ambient = atof(na);
				if (ambient < 0.0)
					usage(cl,"-a parameter %f out of range",ambient);
				ambient /= 3.141592654;	/* Convert from Lux to cd/m^2 */

			/* Test patch offset and size */
			} else if (argv[fa][1] == 'P') {
				fa = nfa;
				if (na == NULL) usage(cl,"Parameter expected after -P");
				if (sscanf(na, " %lf,%lf,%lf ", &ho, &vo, &patscale) != 3)
					usage(cl,"-P parameter '%s' not recognised",na);
				if (ho < 0.0 || ho > 1.0
				 || vo < 0.0 || vo > 1.0
				 || patscale <= 0.0 || patscale > 50.0)
					usage(cl,"-P parameters %f %f %f out of range",ho,vo,patscale);
				ho = 2.0 * ho - 1.0;
				vo = 2.0 * vo - 1.0;

			/* Black background */
			} else if (argv[fa][1] == 'F') {
				blackbg = 1;

			/* Don't use VideoLUT to set high precision test values */
			} else if (argv[fa][1] == 'K') {
				native = 2;

			} else 
				usage(cl,"Flag '-%c' not recognised",argv[fa][1]);
		} else
			break;
	}

	if (bkcorrect < 0.0) {
		if (dtype == 2)			/* LCD */
			bkcorrect = 0.0;	/* Default to no black point correction for LCD */
		else
			bkcorrect = 1.0;	/* Default to full black point correction for other displays */
	}

	patsize *= patscale;

	/* No explicit display has been set */
	if (
#ifndef SHOW_WINDOW_ONFAKE
	!fake && 
#endif
	 disp == NULL) {
		int ix = 0;
#if defined(UNIX) && !defined(__APPLE__)
		char *dn, *pp;

		if ((dn = getenv("DISPLAY")) != NULL) {
			if ((pp = strrchr(dn, ':')) != NULL) {
				if ((pp = strchr(pp, '.')) != NULL) {
					if (pp[1] != '\000')
						ix = atoi(pp+1);
				}
			}
		}
#endif
		if ((disp = get_a_display(ix)) == NULL)
			error("Unable to open the default display");
	}

	/* See if there is an environment variable ccxx */
	if (ccxxname[0] == '\000') {
		char *na;
		if ((na = getenv("ARGYLL_COLMTER_CAL_SPEC_SET")) != NULL) {
			strncpy(ccxxname,na,MAXNAMEL-1); ccxxname[MAXNAMEL-1] = '\000';

		} else if ((na = getenv("ARGYLL_COLMTER_COR_MATRIX")) != NULL) {
			strncpy(ccxxname,na,MAXNAMEL-1); ccxxname[MAXNAMEL-1] = '\000';
		}
	}

	/* Load up CCMX or CCSS */
	if (ccxxname[0] != '\000') {
		if ((cmx = new_ccmx()) == NULL
		  || cmx->read_ccmx(cmx, ccxxname)) {
			if (cmx != NULL) {
				cmx->del(cmx);
				cmx = NULL;
			}
			
			/* CCMX failed, try CCSS */
			if ((ccs = new_ccss()) == NULL
			  || ccs->read_ccss(ccs, ccxxname)) {
				if (ccs != NULL) {
					ccs->del(ccs);
					ccs = NULL;
				}
			}
		}
	}

	if (docalib) {
		if ((rv = disprd_calibration(itype, comport, fc, dtype, proj, adaptive, nocal, disp,
		                             blackbg, override, patsize, ho, vo, verb, debug)) != 0) {
			error("docalibration failed with return value %d\n",rv);
		}
	}

	if (verify != 2 && doreport == 0) {
		/* Get the file name argument */
		if (fa >= argc || argv[fa][0] == '-') usage(cl,"Output filname parameter not found");
		strncpy(outname,argv[fa],MAXNAMEL-4); outname[MAXNAMEL-4] = '\000';
		strcat(outname,".cal");
		if (iccoutname[0] == '\000') {
			strncpy(iccoutname,argv[fa++],MAXNAMEL-4); iccoutname[MAXNAMEL-4] = '\000';
			strcat(iccoutname,ICC_FILE_EXT);
		}
	}

	if (verify == 2) {
		if (doupdate)
			warning("Update flag ignored because we're doing a verify only");
		doupdate = 0;
		docontrols = 0;
	}

	if (doreport != 0) {
		if (verify == 2)
			warning("Verify flag ignored because we're doing a report only");
		verify = 0;
	}

	/* Normally calibrate against native response */
	if (verify == 2 || doreport == 2)
		native = 0;	/* But measure calibrated response of verify or report calibrated */ 

	/* Get ready to do some readings */
	if ((dr = new_disprd(&errc, itype, fake ? -99 : comport, fc, dtype, proj, adaptive, nocal,
	                     highres, native, NULL, 0, 0, disp, blackbg, override, ccallout, mcallout,
	                     patsize, ho, vo,
	                     cmx != NULL ? cmx->matrix : NULL,
	                     ccs != NULL ? ccs->samples : NULL, ccs != NULL ? ccs->no_samp : 0,
	                     spec, obType, NULL, bdrift, wdrift, verb, VERBOUT, debug,
	                     "fake" ICC_FILE_EXT)) == NULL)
		error("new_disprd() failed with '%s'\n",disprd_err(errc));

	if (cmx != NULL)
		cmx->del(cmx);
	if (ccs != NULL)
		ccs->del(ccs);

	free_iccss(cl); cl = NULL; ncl = 0;

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	if (doreport) {
		col tcols[3] = {	/* Base set of test colors */
			{ 0.0, 0.0, 0.0 },
			{ 0.5, 0.5, 0.5 },
			{ 1.0, 1.0, 1.0 }
		};
		double cct, cct_de;		/* Color temperatures and DE 2K */
		double cdt, cdt_de;
		double vct, vct_de;
		double vdt, vdt_de;
		double cgamma, w[3], wp[2];
		int sigbits = 0;	/* Number of significant bits in VideoLUT/display/instrument */

		if ((rv = dr->read(dr, tcols, 3, 1, 3, 1, 0)) != 0) {
			dr->del(dr);
			error("display read failed with '%s'\n",disprd_err(rv));
		} 

//printf("~1 Got black = %f, half = %f, white = %f\n",tcols[0].aXYZ[1],tcols[1].aXYZ[1],tcols[2].aXYZ[1]);
		/* Normalised XYZ white point */
		w[0] = tcols[2].aXYZ[0]/tcols[2].aXYZ[1];
		w[1] = tcols[2].aXYZ[1]/tcols[2].aXYZ[1];
		w[2] = tcols[2].aXYZ[2]/tcols[2].aXYZ[1];

		/* White point chromaticity coordinates */
		wp[0] = w[0]/(w[0] + w[1] + w[2]);
		wp[1] = w[1]/(w[0] + w[1] + w[2]);

		cct = comp_ct(&cct_de, NULL, 1, 0, obType, w);	/* Compute CCT */
		cdt = comp_ct(&cdt_de, NULL, 0, 0, obType, w);	/* Compute CDT */
		vct = comp_ct(&vct_de, NULL, 1, 1, obType, w);	/* Compute VCT */
		vdt = comp_ct(&vdt_de, NULL, 0, 1, obType, w);	/* Compute VDT */

		/* Compute advertised current gamma - use the gross curve shape for robustness */
		cgamma = pop_gamma(tcols[0].aXYZ[1], tcols[1].aXYZ[1], tcols[2].aXYZ[1]);

#ifdef MEAS_RES
		/* See if we can detect what sort of precision the LUT entries */
		/* have. Our ability to detect this may be limited by the instrument */
		/* (ie. Huey and Spyder 2) */
		if (doreport == 1) {
#define MAX_RES_SAMPS 24
			col ttt[MAX_RES_SAMPS];
			int res_samps = 6;
			double a0, a1, a2, dd;
			int n;
			int issig = 0;

			if (verb)
				printf("Measuring VideoLUT table entry precision.\n");

			/* Run a small state machine until we come to a conclusion */
			sigbits = 8;
			for (issig = 0; issig < 2; ) {

				DBG((dbgo,"Trying %d bits\n",sigbits));
				/* Do the test */
				for (n = 0; n < res_samps; n++) {
					double v;
#if defined(__APPLE__) && defined(__POWERPC__)
					gcc_bug_fix(sigbits);
#endif
					/* Notional test value */
					v = (5 << (sigbits-3))/((1 << sigbits) - 1.0);
					/* And -1, 0 , +1 bit test values */
					if ((n % 3) == 2)
						v += 1.0/((1 << sigbits) - 1.0);
					else if ((n % 3) == 1)
						v += 0.0/((1 << sigbits) - 1.0);
					else
						v += -1.0/((1 << sigbits) - 1.0);
					ttt[n].r = ttt[n].g = ttt[n].b = v; 
				}
				if ((rv = dr->read(dr, ttt, res_samps, 1, res_samps, 1, 0)) != 0) {
					dr->del(dr);
					error("display read failed with '%s'\n",disprd_err(rv));
				} 
				/* Average the readings for each test value */
				a0 = a1 = a2 = 0.0;
				for (n = 0; n < res_samps; n++) {
					double v = ttt[n].aXYZ[1];
					if ((n % 3) == 2) {
						a2 += v;
					} else if ((n % 3) == 1) {
						a1 += v;
					} else {
						a0 += v;
					}
				}
				a0 /= (res_samps / 3.0);
				a1 /= (res_samps / 3.0);
				a2 /= (res_samps / 3.0);
				/* Judge significance of any differences */
				dd = 0.0;
				for (n = 0; n < res_samps; n++) {
					double tt;
					if ((n % 3) == 2)
						tt = fabs(a2 - ttt[n].aXYZ[1]);
					else if ((n % 3) == 1)
						tt = fabs(a1 - ttt[n].aXYZ[1]);
					else
						tt = fabs(a0 - ttt[n].aXYZ[1]);
					dd += tt * tt;
				}
				dd /= res_samps;
				dd = sqrt(dd);
				if (fabs(a1 - a0) > (2.0 * dd) && fabs(a2 - a1) > (2.0 * dd))
					issig = 1;		/* Noticable difference */
				else
					issig = 0;		/* No noticable difference */
				DBG((dbgo,"Bits %d: Between = %f, %f within = %f, sig = %s\n",sigbits, fabs(a1 - a0), fabs(a2 - a1),dd, issig ? "yes" : "no"));

				switch(sigbits) {
					case 8:				/* Do another trial */
						if (issig) {
							sigbits = 10;
							res_samps = 6;
						} else {
							sigbits = 6;
						}
						break;
					case 6:				/* Do another trial or give up */
						if (issig) {
							sigbits = 7;
						} else {
							sigbits = 0;
							issig = 2;	/* Give up */
						}
						break;
					case 7:				/* Terminal */
						if (!issig)
							sigbits = 6;
						issig = 2;		/* Stop here */
						break;
					case 10:			/* Do another trial */
						if (issig) {
							sigbits = 12;
							res_samps = 9;
						} else {
							sigbits = 9;
						}
						break;
					case 12:			/* Do another trial or give up */
						if (issig) {
							issig = 2;	/* Stop here */
						} else {
							sigbits = 11;
						}
						break;
					case 11:			/* Terminal */
						if (!issig)
							sigbits = 10;
						issig = 2;		/* Stop here */
						break;
					case 9:				/* Terminal */
						if (!issig)
							sigbits = 8;
						issig = 2;		/* Stop here */
						break;

					default:
						error("Unexpected number of bits in depth test (bits)",sigbits);
				}
			}
		}
#endif	/* MEAS_RES */

		if (doreport == 2)
			printf("Current calibration response:\n");
		else
			printf("Uncalibrated response:\n");
		printf("Black level = %.2f cd/m^2\n",tcols[0].aXYZ[1]);
		printf("White level = %.2f cd/m^2\n",tcols[2].aXYZ[1]);
		printf("Aprox. gamma = %.2f\n",cgamma);
		printf("Contrast ratio = %.0f:1\n",tcols[2].aXYZ[1]/tcols[0].aXYZ[1]);
		printf("White chromaticity coordinates %.4f, %.4f\n",wp[0],wp[1]);
		printf("White    Correlated Color Temperature = %.0fK, DE 2K to locus = %4.1f\n",cct,cct_de);
		printf("White Correlated Daylight Temperature = %.0fK, DE 2K to locus = %4.1f\n",cdt,cdt_de);
		printf("White        Visual Color Temperature = %.0fK, DE 2K to locus = %4.1f\n",vct,vct_de);
		printf("White     Visual Daylight Temperature = %.0fK, DE 2K to locus = %4.1f\n",vdt,vdt_de);
#ifdef	MEAS_RES
		if (doreport == 1) {
			if (sigbits == 0) {
				warning("Unable to determine LUT entry bit depth - problems ?");
			} else {
				printf("Effective LUT entry depth seems to be %d bits\n",sigbits);
			}
		}
#endif	/* MEAS_RES */
		dr->del(dr);
		exit(0);
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* If we're updating, retrieve the previously used settings, */
	/* and device model */
	if (doupdate) {
		cgats *icg;						/* output cgats structure */
		int nsamp;
		mcvco *rdv[3];					/* Scattered data for ramdac curves */
		int fi;							/* Field index */
		int si[4];						/* Set fields */

		if (verb) {
			if (doprofile)
				printf("Updating previous calibration and profile\n");
			else
				printf("Updating previous calibration\n");
		}

		icg = new_cgats();				/* Create a CGATS structure */
		icg->add_other(icg, "CAL"); 	/* our special type is Calibration file */
		
		if (icg->read_name(icg, outname)) {
			dr->del(dr);
			error("Can't update '%s' - read error : %s",outname, icg->err);
		}

		if (icg->ntables == 0
		 || icg->t[0].tt != tt_other || icg->t[0].oi != 0
		 || icg->t[1].tt != tt_other || icg->t[1].oi != 0) {
			dr->del(dr);
			error("Can't update '%s' - wrong type of file",outname);
		}
		if (icg->ntables < 2) {
			dr->del(dr);
			error("Can't update '%s' - there aren't two tables",outname);
		}

//printf("~1 reading previous cal, got 2 tables\n");

		/* Read in the setup, user and model values */

		dtype = 0;		/* Override any user setting */
		if ((fi = icg->find_kword(icg, 0, "DEVICE_TYPE")) >= 0) {
			if (strcmp(icg->t[0].kdata[fi], "CRT") == 0)
				dtype = 1;
			else if (strcmp(icg->t[0].kdata[fi], "LCD") == 0)
				dtype = 2;
			else {
				dr->del(dr);
				error ("Can't update '%s' - field 'DISPLAY_TYPE' has unrecognised value '%s'",
				        outname,icg->t[0].kdata[fi]);
			}
		}
//printf("~1 dealt with device type\n");
	
		if ((fi = icg->find_kword(icg, 0, "TARGET_WHITE_XYZ")) < 0) {
			dr->del(dr);
			error ("Can't update '%s' - can't find field 'TARGET_WHITE_XYZ'",outname);
		}
		if (sscanf(icg->t[0].kdata[fi], "%lf %lf %lf", &x.twh[0], &x.twh[1], &x.twh[2]) != 3) {
			dr->del(dr);
			error ("Can't update '%s' - reading field 'TARGET_WHITE_XYZ' failed",outname);
		}
		x.nwh[0] = x.twh[0] / x.twh[1];
		x.nwh[1] = x.twh[1] / x.twh[1];
		x.nwh[2] = x.twh[2] / x.twh[1];

		if ((fi = icg->find_kword(icg, 0, "NATIVE_TARGET_WHITE")) >= 0) {
			wpx = wpy = 0.0;
			temp = 0.0;
			tbright = 0.0;
		} else {
			wpx = wpy = 0.0001;
			temp = 1.0;
			tbright = 1.0;
		}
//printf("~1 dealt with target white\n");

		if ((fi = icg->find_kword(icg, 0, "TARGET_GAMMA")) < 0) {
			dr->del(dr);
			error ("Can't update '%s' - can't find field 'TARGET_GAMMA'",outname);
		}
		if (strcmp(icg->t[0].kdata[fi], "L_STAR") == 0)
			x.gammat = gt_Lab;
		else if (strcmp(icg->t[0].kdata[fi], "sRGB") == 0)
			x.gammat = gt_sRGB;
		else if (strcmp(icg->t[0].kdata[fi], "REC709") == 0)
			x.gammat = gt_Rec709;
		else if (strcmp(icg->t[0].kdata[fi], "SMPTE240M") == 0)
			x.gammat = gt_SMPTE240M;
		else {
			x.gammat = 0;
			gamma = atof(icg->t[0].kdata[fi]);
			
			if (fabs(gamma) < 0.1 || fabs(gamma) > 5.0) {
				dr->del(dr);
				error ("Can't update '%s' - field 'TARGET_GAMMA' has bad value %f",outname,fabs(gamma));
			}
			if (gamma < 0.0) {	/* Effective gamma = actual power value */
				egamma = -gamma;
			}
		}
		if ((fi = icg->find_kword(icg, 0, "DEGREE_OF_BLACK_OUTPUT_OFFSET")) < 0) {
			/* Backward compatibility if value is not present */
			if (x.gammat == gt_Lab || x.gammat == gt_sRGB)
				x.oofff = 1.0;
			else
				x.oofff = 0.0;
		} else {
			x.oofff = atof(icg->t[0].kdata[fi]);
		}
		if ((fi = icg->find_kword(icg, 0, "TARGET_BLACK_BRIGHTNESS")) < 0) {
			bkbright = 0.0;		/* Native */
		} else {
			bkbright = atof(icg->t[0].kdata[fi]);
		}


		if ((fi = icg->find_kword(icg, 0, "BLACK_POINT_CORRECTION")) < 0) {
			dr->del(dr);
			error ("Can't update '%s' - can't find field 'BLACK_POINT_CORRECTION'",outname);
		}
		bkcorrect = atof(icg->t[0].kdata[fi]);
		if (bkcorrect < 0.0 || bkcorrect > 1.0) {
			dr->del(dr);
			error ("Can't update '%s' - field 'BLACK_POINT_CORRECTION' has bad value %f",outname,bkcorrect);
		}

		if ((fi = icg->find_kword(icg, 0, "BLACK_NEUTRAL_BLEND_RATE")) < 0) {
			x.nbrate = 8.0;		/* Backwards compatibility value */
		} else {
			x.nbrate = atof(icg->t[0].kdata[fi]);
			if (x.nbrate < 0.05 || x.nbrate > 20.0) {
				dr->del(dr);
				error ("Can't update '%s' - field 'BLACK_NEUTRAL_BLEND_RATE' has bad value %f",outname,x.nbrate);
			}
		}

		if ((fi = icg->find_kword(icg, 0, "QUALITY")) < 0) {
			dr->del(dr);
			error ("Can't update '%s' - can't find field 'QUALITY'",outname);
		}
		if (quality < -50) {	/* User hasn't overridden quality */
			if (strcmp(icg->t[0].kdata[fi], "ultra low") == 0)
				quality = -3;
			else if (strcmp(icg->t[0].kdata[fi], "very low") == 0)
				quality = -2;
			else if (strcmp(icg->t[0].kdata[fi], "low") == 0)
				quality = -1;
			else if (strcmp(icg->t[0].kdata[fi], "medium") == 0)
				quality = 0;
			else if (strcmp(icg->t[0].kdata[fi], "high") == 0)
				quality = 1;
			else if (strcmp(icg->t[0].kdata[fi], "ultra high") == 0)
				quality = 2;
			else {
				dr->del(dr);
				error ("Can't update '%s' - field 'QUALITY' has unrecognised value '%s'",
				        outname,icg->t[0].kdata[fi]);
			}
		}
//printf("~1 dealt with quality\n");

		/* Read in the last set of calibration curves used */
		if ((nsamp = icg->t[0].nsets) < 2) {
			dr->del(dr);
			error("Can't update '%s' - %d not enough data points in calibration curves",
			        outname,nsamp);
		}
//printf("~1 got %d points in calibration curves\n",nsamp);

		for (k = 0; k < 3; k++) {
			if ((x.rdac[k] = new_mcv()) == NULL) {
				dr->del(dr);
				error("new_mcv x.rdac[%d] failed",k);
			}
			if ((rdv[k] = malloc(sizeof(mcvco) * nsamp)) == NULL) {
				dr->del(dr);
				error ("Malloc of scattered data points failed");
			}
		}
//printf("~1 allocated calibration curve objects\n");

		/* Read the current calibration curve points (usually CAL_RES of them) */
		for (k = 0; k < 4; k++) {
			char *fnames[4] = { "RGB_I", "RGB_R", "RGB_G", "RGB_B" };

			if ((si[k] = icg->find_field(icg, 0, fnames[k])) < 0) {
				dr->del(dr);
				error ("Can't updata '%s' - can't find field '%s'",outname,fnames[k]);
			}
			if (icg->t[0].ftype[si[k]] != r_t) {
				dr->del(dr);
				error ("Can't updata '%s' - field '%s' is wrong type",outname,fnames[k]);
			}
		}
//printf("~1 Found calibration curve fields\n");

		for (i = 0; i < nsamp; i++) {
			rdv[0][i].p =
			rdv[1][i].p =
			rdv[2][i].p =
			                *((double *)icg->t[0].fdata[i][si[0]]);
			for (k = 0; k < 3; k++) {		/* RGB */
				rdv[k][i].v = *((double *)icg->t[0].fdata[i][si[k + 1]]);
			}
			rdv[0][i].w = rdv[1][i].w = rdv[2][i].w = 1.0;
		}
//printf("~1 Read calibration curve data points\n");
		for (k = 0; k < 3; k++) {
			x.rdac[k]->fit(x.rdac[k], 0, fitord, rdv[k], nsamp, RDAC_SMOOTH);
			free (rdv[k]);
		}
//printf("~1 Fitted calibration curves\n");

		/* Read in the per channel forward model curves */
		for (k = 0; k < 3; k++) {
			char *fnames[3] = { "R_P", "G_P", "B_P" };
			double *pp;

//printf("~1 Reading device curve channel %d\n",k);
			if ((si[k] = icg->find_field(icg, 1, fnames[k])) < 0) {
				dr->del(dr);
				error ("Can't updata '%s' - can't find field '%s'",outname,fnames[k]);
			}
			if (icg->t[1].ftype[si[k]] != r_t) {
				dr->del(dr);
				error ("Can't updata '%s' - field '%s' is wrong type",outname,fnames[k]);
			}
			/* Create the model curves */
			if ((pp = (double *)malloc(icg->t[1].nsets * sizeof(double))) == NULL) {
				dr->del(dr);
				error ("Malloc of device curve parameters");
			}
			for (i = 0; i < icg->t[1].nsets; i++)
				pp[i] = *((double *)icg->t[1].fdata[i][si[k]]);

			if ((x.dcvs[k] = new_mcv_p(pp, icg->t[1].nsets)) == NULL) {
				dr->del(dr);
				error("new_mcv x.dcvs[%d] failed",k);
			}
			free(pp);
		}
		
		icg->del(icg);
//printf("~1 read in previous settings and device model\n");
	}

	/* Be nice - check we can read the iccprofile before calibrating the display */
	if (verify != 2 && doupdate && doprofile) {
		icmFile *ic_fp;
		icc *icco;

		if ((icco = new_icc()) == NULL) {
			dr->del(dr);
			error ("Creation of ICC object to read profile '%s' failed",iccoutname);
		}

		/* Open up the profile for reading */
		if ((ic_fp = new_icmFileStd_name(iccoutname,"r")) == NULL) {
			dr->del(dr);
			error ("Can't open file '%s'",iccoutname);
		}

		/* Read header etc. */
		if ((rv = icco->read(icco,ic_fp,0)) != 0) {
			dr->del(dr);
			error ("Reading profile '%s' failed with %d, %s",iccoutname, rv,icco->err);
		}

		ic_fp->del(ic_fp);

		if (icco->find_tag(icco, icSigVideoCardGammaTag) != 0) {
			dr->del(dr);
			error("Can't find VideoCardGamma tag in file '%s': %d, %s",
			      iccoutname, icco->errc,icco->err);
		}
		icco->del(icco);
	}
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* Convert quality level to iterations etc. */
	/* Note that final tolerance is often double the */
	/* final errth, because one more corrections is always */
	/* performed after the last reading. */
    switch (quality) {
		case -3:				/* Test value */
			isteps = 3;
			rsteps = 9;
			mxits = 1;
			errthr = 2.0;
			break;
		case -2:				/* Very low */
			isteps = 10;
			rsteps = 16;
			errthr = 1.5;
			if (doupdate)
				mxits = 1;
			else
				mxits = 1;
			break;
		case -1:				/* Low */
			if (verify != 2 && doprofile && !doupdate)
				isteps = 24;	/* Use more steps if we're creating a profile */
			else
				isteps = 12;
			rsteps = 32;
			errthr = 0.9;
			if (doupdate)
				mxits = 1;
			else
				mxits = 2;
			break;
		default:
		case 0:					/* Medum */
			quality = 0;		/* In case it wasn't set */
			if (verify != 2 && doprofile && !doupdate)
				isteps = 32;	/* Use more steps if we're creating a profile */
			else
				isteps = 16;
			rsteps = 64;
			errthr = 0.6;
			if (doupdate)
				mxits = 1;
			else
				mxits = 3;
			break;
		case 1:					/* High */
			if (verify != 2 && doprofile && !doupdate)
				isteps = 40;	/* Use more steps if we're creating a profile */
			else
				isteps = 20;
			rsteps = 96;
			errthr = 0.4;
			if (doupdate)
				mxits = 1;
			else
				mxits = 4;
			break;
		case 2:					/* Ultra */
			if (verify != 2 && doprofile && !doupdate)
				isteps = 48;	/* Use more steps if we're creating a profile */
			else
				isteps = 24;
			rsteps = 128;
			errthr = 0.25;
			if (doupdate)
				mxits = 1;
			else
				mxits = 5;
			break;
	}

	/* Set native white target flag in cctx so that other things can play the game.. */
	if (wpx == 0.0 && wpy == 0.0 && temp == 0.0 && tbright == 0.0)
		x.nat = 1;
	else
		x.nat = 0;

	if (verb) {
		if (dtype == 1)
			printf("Display type is CRT\n");
		else if (dtype == 2)
			printf("Display type is LCD\n");

		if (doupdate) {
			if (x.nat)
				printf("Target white = native white point & brightness\n");
			else
				printf("Target white = XYZ %f %f %f\n",
				       x.twh[0], x.twh[1], x.twh[2]);
		} else {
			if (wpx > 0.0 || wpy > 0.0)
				printf("Target white = xy %f %f\n",wpx,wpy);
			else if (temp > 0.0) {
				if (planckian)
					printf("Target white = %f degrees kelvin Planckian (black body) spectrum\n",temp);
				else
					printf("Target white = %f degrees kelvin Daylight spectrum\n",temp);
			} else
				printf("Target white = native white point\n");
	
			if (tbright > 0.0)
				printf("Target white brightness = %f cd/m^2\n",tbright);
			else
				printf("Target white brightness = native brightness\n");
			if (bkbright > 0.0)
				printf("Target black brightness = %f cd/m^2\n",bkbright);
			else
				printf("Target black brightness = native brightness\n");
		}

		switch(x.gammat) {
			case gt_power:
				if (egamma > 0.0)
					printf("Target effective gamma = %f\n",egamma);
				else
					printf("Target advertised gamma = %f\n",gamma);
				break;
			case gt_Lab:
				printf("Target gamma = L* curve\n");
				break;
			case gt_sRGB:
				printf("Target gamma = sRGB curve\n");
				break;
			case gt_Rec709:
				printf("Target gamma = REC 709 curve\n");
				break;
			case gt_SMPTE240M:
				printf("Target gamma = SMPTE 240M curve\n");
				break;
			default:
				error("Unknown gamma type");
		}
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* Go through the procedure of adjusting monitor controls */
	if (docontrols) {
		int rgbch = 0;			/* Got RBG Yxy ? */
		double rgbXYZ[3][3];	/* The RGB XYZ */

		/* Make sure drift comp. is off for interactive adjustment */
		dr->change_drift_comp(dr, 0, 0);

		/* Until the user is done */
		printf("\nDisplay adjustment menu:");
		for (;;) {
			int c;

			/* Print the menue of adjustments */
			printf("\nPress 1 .. 7\n");
			printf("1) Black level (CRT: Offset/Brightness)\n");
			printf("2) White point (Color temperature, R,G,B, Gain/Contrast)\n");
			printf("3) White level (CRT: Gain/Contrast, LCD: Brightness/Backlight)\n");
			printf("4) Black point (R,G,B, Offset/Brightness)\n");
			printf("5) Check all\n");
			printf("6) Measure and set ambient for viewing condition adjustment\n");
			printf("7) Continue on to calibration\n");
			printf("8) Exit\n");

			empty_con_chars();
			c = next_con_char();

			/* Black level adjustment */
			/* Due to the possibility of the channel offsets not being even, */
			/* we use the largest of the XYZ values after they have been */
			/* scaled to be even acording to the white XYZ balance. */
			/* It's safer to set the black level a bit low, and then the */
			/* calibration curves can bump the low ones up. */
			if (c == '1') {
				col tcols[3] = {	/* Base set of test colors */
					{ 0.0, 0.0, 0.0 },
					{ 0.5, 0.5, 0.5 },		/* And 1% values */
					{ 1.0, 1.0, 1.0 }
				};
				int ff;
				double mgamma, tar1, dev1;
		
				printf("Doing some initial measurements\n");
				/* Do an initial set of readings to set 1% output mark */
				if ((rv = dr->read(dr, tcols, 3, 0, 0, 1, 0)) != 0) {
					dr->del(dr);
					error("display read failed with '%s'\n",disprd_err(rv));
				} 

				if (verb) {
					printf("Black = XYZ %6.2f %6.2f %6.2f\n",tcols[0].aXYZ[0],
					                                         tcols[0].aXYZ[1], tcols[0].aXYZ[2]);
					printf("Grey  = XYZ %6.2f %6.2f %6.2f\n",tcols[1].aXYZ[0],
					                                         tcols[1].aXYZ[1], tcols[1].aXYZ[2]);
					printf("White = XYZ %6.2f %6.2f %6.2f\n",tcols[2].aXYZ[0],
					                                         tcols[2].aXYZ[1], tcols[2].aXYZ[2]);
				}

				/* Advertised Gamma - Gross curve shape */
				mgamma = pop_gamma(tcols[0].aXYZ[1], tcols[1].aXYZ[1], tcols[2].aXYZ[1]);

				dev1 = pow(0.01, 1.0/mgamma);
//printf("~1 device level for 1%% output = %f\n",dev1);
				tcols[1].r = tcols[1].g = tcols[1].b = dev1;
				tar1 = 0.01 * tcols[2].aXYZ[1];

				printf("\nAdjust CRT brightness to get target level. Press space when done.\n");
				printf("   Target %.2f\n",tar1);
				for (ff = 0;; ff ^= 1) {
					double dir;			/* Direction to adjust brightness */
					double sv[3], val1;				/* Scaled values */
					if ((rv = dr->read(dr, tcols+1, 1, 0, 0, 1, ' ')) != 0) {
						if (rv == 4)
							break;			/* User is done with this adjustment */
						dr->del(dr);
						error("display read failed with '%s'\n",disprd_err(rv));
					} 
					/* Scale 1% values by ratio of Y to white XYZ */
					sv[0] = tcols[1].aXYZ[0] * tcols[2].aXYZ[1]/tcols[2].aXYZ[0];
					sv[1] = tcols[1].aXYZ[1];
					sv[2] = tcols[1].aXYZ[2] * tcols[2].aXYZ[1]/tcols[2].aXYZ[2];
//printf("~1 scaled readings = %f %f %f\n",sv[0],sv[1],sv[2]);
					val1 = sv[1];
					if (sv[0] > val1)
						val1 = sv[0];
					if (sv[2] > val1)
						val1 = sv[2];
					dir = tar1 - val1;
					if (fabs(dir) < 0.01)
						dir = 0.0;
					printf("%c%c Current %.2f  %c",
					       cr_char,
					       ff == 0 ? '/' : '\\',
					       val1,
					       dir < 0.0 ? '-' : dir > 0.0 ? '+' : '=');
					fflush(stdout);
					c = poll_con_char();
					if (c == ' ')
						break;
				}
				printf("\n");

			/* White point adjustment */
			} else if (c == '2') {
				int nat = 0;		/* NZ if using native white as target */
				col tcols[1] = {	/* Base set of test colors */
					{ 1.0, 1.0, 1.0 }
				};
				int ff;
				double tYxy[3];				/* Target white chromaticities */
				icmXYZNumber tXYZ;			/* Target white as XYZ */
				double tLab[3];				/* Target white as Lab or UCS */
				double tarw;				/* Target brightness */
				double Lab[3];				/* Last measured point Lab or UCS */
				double ct = 0.0, ct_de;		/* Color temperature & delta E to white locus */
		
				printf("Doing some initial measurements\n");

				if (rgbch == 0) {	/* Figure the RGB chromaticities */
					col ccols[3] = {
						{ 1.0, 0.0, 0.0 },
						{ 0.0, 1.0, 0.0 },
						{ 0.0, 0.0, 1.0 }
					};
					if ((rv = dr->read(dr, ccols, 3, 0, 0, 1, 0)) != 0) {
						dr->del(dr);
						error("display read failed with '%s'\n",disprd_err(rv));
					}
					if (verb) {
						printf("Red   = XYZ %6.2f %6.2f %6.2f\n",ccols[0].aXYZ[0],
						                       ccols[0].aXYZ[1], ccols[0].aXYZ[2]);
						printf("Green = XYZ %6.2f %6.2f %6.2f\n",ccols[1].aXYZ[0],
						                       ccols[1].aXYZ[1], ccols[1].aXYZ[2]);
						printf("Blue  = XYZ %6.2f %6.2f %6.2f\n",ccols[2].aXYZ[0],
						                       ccols[2].aXYZ[1], ccols[2].aXYZ[2]);
					}
					for (i = 0; i < 3; i++)
						icmAry2Ary(rgbXYZ[i], ccols[i].aXYZ);
					rgbch = 1;
				}
				/* Do an initial set of readings to set full output mark */
				if ((rv = dr->read(dr, tcols, 1, 0, 0, 1, 0)) != 0) {
					dr->del(dr);
					error("display read failed with '%s'\n",disprd_err(rv));
				}
				if (verb) {
					printf("White = XYZ %6.2f %6.2f %6.2f\n",tcols[0].aXYZ[0],
					                       tcols[0].aXYZ[1], tcols[0].aXYZ[2]);
				}
				
				/* Figure out the target white chromaticity */
				if (wpx > 0.0 || wpy > 0.0) {	/* xy coordinates */
					tYxy[0] = 1.0;
					tYxy[1] = wpx;
					tYxy[2] = wpy;
				} else if (temp > 0.0) {		/* Daylight color temperature */
					double XYZ[3];
					if (planckian)
						rv = icx_ill_sp2XYZ(XYZ, icxOT_default, NULL, icxIT_Ptemp, temp, NULL);
					else
						rv = icx_ill_sp2XYZ(XYZ, icxOT_default, NULL, icxIT_Dtemp, temp, NULL);
					if (rv != 0)
						error("Failed to compute XYZ of target color temperature %f\n",temp);
					icmXYZ2Yxy(tYxy, XYZ);
				} else {						/* Native white */
					icmXYZ2Yxy(tYxy, tcols[0].aXYZ);
					nat = 1;
				}

				/* Figure out the target white brightness */
				/* Note we're not taking the device gamut into account here */
				if (tbright > 0.0) {			/* Given brightness */
					tarw = tbright;
//printf("~1 set tarw %f from tbright\n",tarw);
				} else {						/* Native/maximum brightness */
					tarw = tcols[0].aXYZ[1];
//printf("~1 set tarw %f from tcols[1]\n",tarw);
				}

				if (!nat) {	/* Target is a specified white */
					printf("\nAdjust R,G & B gain to get target x,y. Press space when done.\n");
					printf("   Target Br %.2f, x %.4f , y %.4f \n",
					        tarw, tYxy[1],tYxy[2]);

				} else {	/* Target is native white */
					printf("\nAdjust R,G & B gain to desired white point. Press space when done.\n");
					/* Compute the CT and delta E to white locus of target */
					ct = comp_ct(&ct_de, NULL, planckian, dovct, obType, tcols[0].aXYZ);
					printf("  Initial Br %.2f, x %.4f , y %.4f , %c%cT %4.0fK DE 2K %4.1f\n",
					        tarw, tYxy[1],tYxy[2],
				            dovct ? 'V' : 'C', planckian ? 'C' : 'D', ct,ct_de);
				}
				for (ff = 0;; ff ^= 1) {
					double dir;			/* Direction to adjust brightness */
					double Yxy[3];		/* Yxy of current reading */
					double rgbdir[3];	/* Direction to adjust RGB */
					double rgbxdir[3];	/* Biggest to move */
					double bdir, terr;
					int bx = 0;
					
					if ((rv = dr->read(dr, tcols, 1, 0, 0, 1, ' ')) != 0) {
						if (rv == 4)
							break;			/* User is done with this adjustment */
						dr->del(dr);
						error("display read failed with '%s'\n",disprd_err(rv));
					} 
					dir = tarw - tcols[0].aXYZ[1];
					if (fabs(dir) < 0.01)
						dir = 0.0;

					icmXYZ2Yxy(Yxy, tcols[0].aXYZ);

					if (!nat) {	/* Target is a specified white */
						/* Compute values we need for delta E and RGB direction */
						icmYxy2XYZ(tLab, tYxy);
						tLab[0] /= tLab[1];
						tLab[2] /= tLab[1];
						tLab[1] /= tLab[1];
						icmAry2XYZ(tXYZ, tLab);					/* Lab white reference */
						icmXYZ2Lab(&tXYZ, tLab, tLab);			/* Target Lab */
	
						icmAry2Ary(Lab, tcols[0].aXYZ);
						Lab[0] /= Lab[1];
						Lab[2] /= Lab[1];
						Lab[1] /= Lab[1];
						icmXYZ2Lab(&tXYZ, Lab, Lab);			/* Current Lab */

					} else {	/* Target is native white */
						double lxyz[3];	/* Locus XYZ */
						ct = comp_ct(&ct_de, lxyz, planckian, dovct, obType, tcols[0].aXYZ);

						icmXYZ2Yxy(tYxy, lxyz);
						/* lxyz is already normalised */
						icmAry2XYZ(tXYZ, lxyz);					/* Lab white reference */
						if (dovct)
							icmXYZ2Lab(&tXYZ, tLab, lxyz);		/* Target Lab */
						else
							icmXYZ21960UCS(tLab, lxyz);			/* Target UCS */
	
						icmAry2Ary(Lab, tcols[0].aXYZ);
						Lab[0] /= Lab[1];
						Lab[2] /= Lab[1];
						Lab[1] /= Lab[1];
						if (dovct)
							icmXYZ2Lab(&tXYZ, Lab, Lab);		/* Current Lab */
						else
							icmXYZ21960UCS(Lab, Lab);			/* Current UCS */
					}

					/* Compute dot products */
					bdir = 0.0;
					for (i = 0; i < 3; i++) {
						double rgbLab[3];
						
						if (dovct)
							icmXYZ2Lab(&tXYZ, rgbLab, rgbXYZ[i]);
						else
							icmXYZ21960UCS(rgbLab, rgbXYZ[i]);
						rgbdir[i] = (tLab[1] - Lab[1]) * (rgbLab[1] - Lab[1])
						          + (tLab[2] - Lab[2]) * (rgbLab[2] - Lab[2]);
						rgbxdir[i] = 0.0;
						if (fabs(rgbdir[i]) > fabs(bdir)) {
							bdir = rgbdir[i];
							bx = i;
						}
					}

					/* See how close to the target we are */
					terr = sqrt((tLab[1] - Lab[1]) * (tLab[1] - Lab[1])
					          + (tLab[2] - Lab[2]) * (tLab[2] - Lab[2]));
					if (terr < 0.1)
						rgbdir[0] = rgbdir[1] = rgbdir[2] = 0.0;
					rgbxdir[bx] = rgbdir[bx];

	
					if (!nat) {
						printf("%c%c Current Br %.2f, x %.4f%c, y %.4f%c  DE %4.1f  R%c%c G%c%c B%c%c ",
						   cr_char,
					       ff == 0 ? '/' : '\\',
					       tcols[0].aXYZ[1],
					       Yxy[1],
						   Yxy[1] > tYxy[1] ? '-' : Yxy[1] < tYxy[1] ? '+' : '=',
					       Yxy[2],
						   Yxy[2] > tYxy[2] ? '-' : Yxy[2] < tYxy[2] ? '+' : '=',
					       icmCIE2K(tLab, Lab),
					       rgbdir[0] < 0.0 ? '-' : rgbdir[0] > 0.0 ? '+' : '=',
					       rgbxdir[0] < 0.0 ? '-' : rgbxdir[0] > 0.0 ? '+' : ' ',
					       rgbdir[1] < 0.0 ? '-' : rgbdir[1] > 0.0 ? '+' : '=',
					       rgbxdir[1] < 0.0 ? '-' : rgbxdir[1] > 0.0 ? '+' : ' ',
					       rgbdir[2] < 0.0 ? '-' : rgbdir[2] > 0.0 ? '+' : '=',
					       rgbxdir[2] < 0.0 ? '-' : rgbxdir[2] > 0.0 ? '+' : ' ');
					} else {
						printf("%c%c Current Br %.2f, x %.4f%c, y %.4f%c  %c%cT %4.0fK DE 2K %4.1f  R%c%c G%c%c B%c%c ",
						   cr_char,
					       ff == 0 ? '/' : '\\',
					       tcols[0].aXYZ[1],
					       Yxy[1],
						   Yxy[1] > tYxy[1] ? '-': Yxy[1] < tYxy[1] ? '+' : '=',
					       Yxy[2],
						   Yxy[2] > tYxy[2] ? '-': Yxy[2] < tYxy[2] ? '+' : '=',
				           dovct ? 'V' : 'C', planckian ? 'C' : 'D', ct,ct_de,
					       rgbdir[0] < 0.0 ? '-' : rgbdir[0] > 0.0 ? '+' : '=',
					       rgbxdir[0] < 0.0 ? '-' : rgbxdir[0] > 0.0 ? '+' : ' ',
					       rgbdir[1] < 0.0 ? '-' : rgbdir[1] > 0.0 ? '+' : '=',
					       rgbxdir[1] < 0.0 ? '-' : rgbxdir[1] > 0.0 ? '+' : ' ',
					       rgbdir[2] < 0.0 ? '-' : rgbdir[2] > 0.0 ? '+' : '=',
					       rgbxdir[2] < 0.0 ? '-' : rgbxdir[2] > 0.0 ? '+' : ' ');
					}
					fflush(stdout);
					c = poll_con_char();
					if (c == ' ')
						break;
				}
				printf("\n");

			/* White level adjustment */
			} else if (c == '3') {
				col tcols[1] = {	/* Base set of test colors */
					{ 1.0, 1.0, 1.0 }
				};
				int ff;
				double tarw;
		
				printf("Doing some initial measurements\n");
				/* Do an initial set of readings to set full output mark */
				if ((rv = dr->read(dr, tcols, 1, 0, 0, 1, 0)) != 0) {
					dr->del(dr);
					error("display read failed with '%s'\n",disprd_err(rv));
				}
				if (verb) {
					printf("White = XYZ %6.2f %6.2f %6.2f\n",tcols[0].aXYZ[0],
					                       tcols[0].aXYZ[1], tcols[0].aXYZ[2]);
				}
				
				/* Figure out the target white brightness */
				/* Note we're not taking the device gamut into account here */
				if (tbright > 0.0)			/* Given brightness */
					tarw = tbright;
				else						/* Native/maximum brightness */
					tarw = tcols[0].aXYZ[1];

				if (tbright > 0.0) {
					printf("\nAdjust CRT Contrast or LCD Brightness to get target level. Press space when done.\n");
					printf("   Target %.2f\n", tarw);
				} else {
					printf("\nAdjust CRT Contrast or LCD Brightness to desired level. Press space when done.\n");
					printf("  Initial %.2f\n", tarw);
				}
				for (ff = 0;; ff ^= 1) {
					double dir;			/* Direction to adjust brightness */
					
					if ((rv = dr->read(dr, tcols, 1, 0, 0, 1, ' ')) != 0) {
						if (rv == 4)
							break;			/* User is done with this adjustment */
						dr->del(dr);
						error("display read failed with '%s'\n",disprd_err(rv));
					} 
					dir = tarw - tcols[0].aXYZ[1];
					if (fabs(dir) < 0.01)
						dir = 0.0;

					if (tbright > 0.0)
						printf("%c%c Current %.2f  %c",
						   cr_char,
					       ff == 0 ? '/' : '\\',
					       tcols[0].aXYZ[1],
					       dir < 0.0 ? '-' : dir > 0.0 ? '+' : '=');
					else
						printf("%c%c Current %.2f   ",
						   cr_char,
					       ff == 0 ? '/' : '\\',
					       tcols[0].aXYZ[1]);
					fflush(stdout);
					c = poll_con_char();
					if (c == ' ')
						break;
				}
				printf("\n");

			/* Black point adjustment */
			} else if (c == '4') {
				col tcols[3] = {	/* Base set of test colors */
					{ 0.0, 0.0, 0.0 },
					{ 0.5, 0.5, 0.5 },		/* And 1% values */
					{ 1.0, 1.0, 1.0 }
				};
				int ff;
				double tYxy[3];				/* Target white chromaticities */
				icmXYZNumber tXYZ;			/* Target white as XYZ */
				double tLab[3];				/* Target white as Lab or UCS */
				double mgamma, tar1, dev1;
				double Lab[3];				/* Last measured point Lab */
		
				printf("Doing some initial measurements\n");

				if (rgbch == 0) {	/* Figure the RGB chromaticities */
					col ccols[3] = {
						{ 1.0, 0.0, 0.0 },
						{ 0.0, 1.0, 0.0 },
						{ 0.0, 0.0, 1.0 }
					};
					if ((rv = dr->read(dr, ccols, 3, 0, 0, 1, 0)) != 0) {
						dr->del(dr);
						error("display read failed with '%s'\n",disprd_err(rv));
					}
					if (verb) {
						printf("Red   = XYZ %6.2f %6.2f %6.2f\n",ccols[0].aXYZ[0],
						                       ccols[0].aXYZ[1], ccols[0].aXYZ[2]);
						printf("Green = XYZ %6.2f %6.2f %6.2f\n",ccols[1].aXYZ[0],
						                       ccols[1].aXYZ[1], ccols[1].aXYZ[2]);
						printf("Blue  = XYZ %6.2f %6.2f %6.2f\n",ccols[2].aXYZ[0],
						                       ccols[2].aXYZ[1], ccols[2].aXYZ[2]);
					}
					for (i = 0; i < 3; i++)
						icmAry2Ary(rgbXYZ[i], ccols[i].aXYZ);
					rgbch = 1;
				}
				/* Do an initial set of readings to set 1% output mark */
				if ((rv = dr->read(dr, tcols, 3, 0, 0, 1, 0)) != 0) {
					dr->del(dr);
					error("display read failed with '%s'\n",disprd_err(rv));
				}
				if (verb) {
					printf("Black = XYZ %6.2f %6.2f %6.2f\n",tcols[0].aXYZ[0],
					                                         tcols[0].aXYZ[1], tcols[0].aXYZ[2]);
					printf("Grey  = XYZ %6.2f %6.2f %6.2f\n",tcols[1].aXYZ[0],
					                                         tcols[1].aXYZ[1], tcols[1].aXYZ[2]);
					printf("White = XYZ %6.2f %6.2f %6.2f\n",tcols[2].aXYZ[0],
					                                         tcols[2].aXYZ[1], tcols[2].aXYZ[2]);
				}
				
				/* Advertised Gamma - Gross curve shape */
				mgamma = pop_gamma(tcols[0].aXYZ[1], tcols[1].aXYZ[1], tcols[2].aXYZ[1]);

				dev1 = pow(0.01, 1.0/mgamma);
				tcols[1].r = tcols[1].g = tcols[1].b = dev1;
				tar1 = 0.01 * tcols[2].aXYZ[1];

				/* Figure out the target white chromaticity */
				if (wpx > 0.0 || wpy > 0.0) {	/* xy coordinates */
					tYxy[0] = 1.0;
					tYxy[1] = wpx;
					tYxy[2] = wpy;
		
				} else if (temp > 0.0) {		/* Daylight color temperature */
					double XYZ[3];
					if (planckian)
						rv = icx_ill_sp2XYZ(XYZ, icxOT_default, NULL, icxIT_Ptemp, temp, NULL);
					else
						rv = icx_ill_sp2XYZ(XYZ, icxOT_default, NULL, icxIT_Dtemp, temp, NULL);
					if (rv != 0)
						error("Failed to compute XYZ of target color temperature %f\n",temp);
					icmXYZ2Yxy(tYxy, XYZ);
				} else {						/* Native white */
					icmXYZ2Yxy(tYxy, tcols[2].aXYZ);
				}

				printf("\nAdjust R,G & B offsets to get target x,y. Press space when done.\n");
				printf("   Target Br %.2f, x %.4f , y %.4f \n", tar1, tYxy[1],tYxy[2]);
				for (ff = 0;; ff ^= 1) {
					double dir;			/* Direction to adjust brightness */
					double sv[3], val1;				/* Scaled values */
					double Yxy[3];		/* Yxy of current reading */
					double rgbdir[3];	/* Direction to adjust RGB */
					double rgbxdir[3];	/* Biggest to move */
					double bdir, terr;
					int bx = 0;
					
					if ((rv = dr->read(dr, tcols+1, 1, 0, 0, 1, ' ')) != 0) {
						if (rv == 4)
							break;			/* User is done with this adjustment */
						dr->del(dr);
						error("display read failed with '%s'\n",disprd_err(rv));
					} 
				
					/* Scale 1% values by ratio of Y to white XYZ */
					sv[0] = tcols[1].aXYZ[0] * tcols[2].aXYZ[1]/tcols[2].aXYZ[0];
					sv[1] = tcols[1].aXYZ[1];
					sv[2] = tcols[1].aXYZ[2] * tcols[2].aXYZ[1]/tcols[2].aXYZ[2];
					val1 = sv[1];
					if (sv[0] > val1)
						val1 = sv[0];
					if (sv[2] > val1)
						val1 = sv[2];

					/* Compute 1% direction */
					dir = tar1 - val1;
					if (fabs(dir) < 0.01)
						dir = 0.0;

					/* Compute numbers for black point error and direction */
					icmYxy2XYZ(tLab, tYxy);
					tLab[0] /= tLab[1];
					tLab[2] /= tLab[1];
					tLab[1] /= tLab[1];
					icmAry2XYZ(tXYZ, tLab);				/* Lab white reference */
					icmXYZ2Lab(&tXYZ, tLab, tLab);

					icmXYZ2Yxy(Yxy, tcols[1].aXYZ);
					icmAry2Ary(Lab, tcols[1].aXYZ);
					Lab[0] /= Lab[1];
					Lab[2] /= Lab[1];
					Lab[1] /= Lab[1];
					icmXYZ2Lab(&tXYZ, Lab, Lab);
	
					/* Compute dot products */
					bdir = 0.0;
					for (i = 0; i < 3; i++) {
						double rgbLab[3];
						
						icmXYZ2Lab(&tXYZ, rgbLab, rgbXYZ[i]);
						rgbdir[i] = (tLab[1] - Lab[1]) * (rgbLab[1] - Lab[1])
						          + (tLab[2] - Lab[2]) * (rgbLab[2] - Lab[2]);
						rgbxdir[i] = 0.0;
						if (fabs(rgbdir[i]) > fabs(bdir)) {
							bdir = rgbdir[i];
							bx = i;
						}
					}

					/* See how close to the target we are */
					terr = sqrt((tLab[1] - Lab[1]) * (tLab[1] - Lab[1])
					          + (tLab[2] - Lab[2]) * (tLab[2] - Lab[2]));
					if (terr < 0.1)
						rgbdir[0] = rgbdir[1] = rgbdir[2] = 0.0;
					rgbxdir[bx] = rgbdir[bx];

			 		printf("%c%c Current Br %.2f, x %.4f%c, y %.4f%c  DE %4.1f  R%c%c G%c%c B%c%c ",
					       cr_char,
					       ff == 0 ? '/' : '\\',
					       val1,
					       Yxy[1],
						   Yxy[1] > tYxy[1] ? '-': Yxy[1] < tYxy[1] ? '+' : '=',
					       Yxy[2],
						   Yxy[2] > tYxy[2] ? '-': Yxy[2] < tYxy[2] ? '+' : '=',
					       icmCIE2K(tLab, Lab),
					       rgbdir[0] < 0.0 ? '-' : rgbdir[0] > 0.0 ? '+' : '=',
					       rgbxdir[0] < 0.0 ? '-' : rgbxdir[0] > 0.0 ? '+' : ' ',
					       rgbdir[1] < 0.0 ? '-' : rgbdir[1] > 0.0 ? '+' : '=',
					       rgbxdir[1] < 0.0 ? '-' : rgbxdir[1] > 0.0 ? '+' : ' ',
					       rgbdir[2] < 0.0 ? '-' : rgbdir[2] > 0.0 ? '+' : '=',
					       rgbxdir[2] < 0.0 ? '-' : rgbxdir[2] > 0.0 ? '+' : ' ');
					fflush(stdout);
					c = poll_con_char();
					if (c == ' ')
						break;
				}
				printf("\n");

			/* Report on how well we current meet the targets */
			} else if (c == '5') {
				int nat = 0;		/* NZ if using native white as target */
				col tcols[4] = {	/* Set of test colors */
					{ 0.0, 0.0, 0.0 },
					{ 0.5, 0.5, 0.5 },
					{ 1.0, 1.0, 1.0 },
					{ 0.0, 0.0, 0.0 }	/* 1% test value */
				};
				double tYxy[3];				/* Target white chromaticities */
				double sv[3], val1;			/* Scaled values */
				double mgamma, tarw, tar1, dev1, tarh;
				double gooff;				/* Aproximate output offset needed */
				icmXYZNumber tXYZ;
				double tLab[3], wYxy[3], wLab[3], bYxy[3], bLab[3];
				double ct, ct_de;			/* Color temperature & delta E to white locus */
		
				printf("Doing check measurements\n");

				/* Do an initial set of readings to set 1% output mark */
				if ((rv = dr->read(dr, tcols, 3, 0, 0, 1, 0)) != 0) {
					dr->del(dr);
					error("display read failed with '%s'\n",disprd_err(rv));
				}
				if (verb) {
					printf("Black = XYZ %6.2f %6.2f %6.2f\n",tcols[0].aXYZ[0],
					                                         tcols[0].aXYZ[1], tcols[0].aXYZ[2]);
					printf("Grey  = XYZ %6.2f %6.2f %6.2f\n",tcols[1].aXYZ[0],
					                                         tcols[1].aXYZ[1], tcols[1].aXYZ[2]);
					printf("White = XYZ %6.2f %6.2f %6.2f\n",tcols[2].aXYZ[0],
					                                         tcols[2].aXYZ[1], tcols[2].aXYZ[2]);
				}
				
				/* Approximate Gamma - use the gross curve shape for robustness */
				mgamma = pop_gamma(tcols[0].aXYZ[1], tcols[1].aXYZ[1], tcols[2].aXYZ[1]);

				dev1 = pow(0.01, 1.0/mgamma);
				tcols[3].r = tcols[3].g = tcols[3].b = dev1;
				tar1 = 0.01 * tcols[2].aXYZ[1];

				/* Read the 1% value */
				if ((rv = dr->read(dr, tcols+3, 1, 0, 0, 1, 0)) != 0) {
					dr->del(dr);
					error("display read failed with '%s'\n",disprd_err(rv));
				}
				if (verb) {
					printf("1%%    = XYZ %6.2f %6.2f %6.2f\n",tcols[3].aXYZ[0],
					                                         tcols[3].aXYZ[1], tcols[3].aXYZ[2]);
				}
				
				/* Scale 1% values by ratio of Y to white XYZ */
				/* (Note we're assuming -k1 here, which may not be true...) */
				sv[0] = tcols[3].aXYZ[0] * tcols[2].aXYZ[1]/tcols[2].aXYZ[0];
				sv[1] = tcols[3].aXYZ[1];
				sv[2] = tcols[3].aXYZ[2] * tcols[2].aXYZ[1]/tcols[2].aXYZ[2];
				val1 = sv[1];
				if (sv[0] > val1)
					val1 = sv[0];
				if (sv[2] > val1)
					val1 = sv[2];

				/* Figure out the target white brightness */
				/* Note we're not taking the device gamut into account here */
				if (tbright > 0.0)			/* Given brightness */
					tarw = tbright;
				else						/* Native/maximum brightness */
					tarw = tcols[2].aXYZ[1];

				/* Figure out the target white chromaticity */
				if (wpx > 0.0 || wpy > 0.0) {	/* xy coordinates */
					tYxy[0] = 1.0;
					tYxy[1] = wpx;
					tYxy[2] = wpy;
		
				} else if (temp > 0.0) {		/* Daylight color temperature */
					double XYZ[3];
					if (planckian)
						rv = icx_ill_sp2XYZ(XYZ, icxOT_default, NULL, icxIT_Ptemp, temp, NULL);
					else
						rv = icx_ill_sp2XYZ(XYZ, icxOT_default, NULL, icxIT_Dtemp, temp, NULL);
					if (rv != 0)
						error("Failed to compute XYZ of target color temperature %f\n",temp);
					icmXYZ2Yxy(tYxy, XYZ);
				} else {						/* Native white */
					icmXYZ2Yxy(tYxy, tcols[2].aXYZ);
					nat = 1;
				}

				/* Figure out the target 50% device output value */
				gooff = tcols[0].aXYZ[1]/tcols[2].aXYZ[1];	/* Aprox. normed black output offset */

				/* Use tech_gamma() to do the hard work */
				tarh = tech_gamma(&x, NULL, NULL, NULL, egamma, gamma, gooff);

				/* Convert from Y fraction to absolute Y */
				tarh = tarh * tcols[2].aXYZ[1];

				/* Compute various white point values */
				icmYxy2XYZ(tLab, tYxy);
				tLab[0] /= tLab[1];
				tLab[2] /= tLab[1];
				tLab[1] /= tLab[1];
				icmAry2XYZ(tXYZ, tLab);
				icmXYZ2Lab(&tXYZ, tLab, tLab);

				icmXYZ2Yxy(wYxy, tcols[2].aXYZ);
				icmAry2Ary(wLab, tcols[2].aXYZ);
				wLab[0] /= wLab[1];
				wLab[2] /= wLab[1];
				wLab[1] /= wLab[1];
				icmXYZ2Lab(&tXYZ, wLab, wLab);

				icmXYZ2Yxy(bYxy, tcols[3].aXYZ);
				icmAry2Ary(bLab, tcols[3].aXYZ);
				bLab[0] /= bLab[1];
				bLab[2] /= bLab[1];
				bLab[1] /= bLab[1];
				icmXYZ2Lab(&tXYZ, bLab, bLab);

				/* And color temperature */
				ct = comp_ct(&ct_de, NULL, planckian, dovct, obType, tcols[2].aXYZ);

				printf("\n");

				if (tbright > 0.0)			/* Given brightness */
					printf("  Target Brightness = %.2f, Current = %5.2f, error = % .1f%%\n",
				       tarw, tcols[2].aXYZ[1], 
				       100.0 * (tcols[2].aXYZ[1] - tarw)/tarw);
				else
					printf("  Current Brightness = %.2f\n", tcols[2].aXYZ[1]);
				
				printf("  Target 50%% Level  = %.2f, Current = %5.2f, error = % .1f%%\n",
				       tarh, tcols[1].aXYZ[1], 
				       100.0 * (tcols[1].aXYZ[1] - tarh)/tarw);
				
				printf("  Target Near Black = %5.2f, Current = %5.2f, error = % .1f%%\n",
				       tar1, val1, 
				       100.0 * (val1 - tar1)/tarw);

				if (!nat)
					printf("  Target white = x %.4f, y %.4f, Current = x %.4f, y %.4f, error = %5.2f DE\n",
				       tYxy[1], tYxy[2], wYxy[1], wYxy[2], icmCIE2K(tLab, wLab));
				else
					printf("  Current white = x %.4f, y %.4f, %c%cT %4.0fK DE 2K %4.1f\n",
				       wYxy[1], wYxy[2], dovct ? 'V' : 'C', planckian ? 'C' : 'D', ct,ct_de);

				printf("  Target black = x %.4f, y %.4f, Current = x %.4f, y %.4f, error = %5.2f DE\n",
				       tYxy[1], tYxy[2], bYxy[1], bYxy[2], icmCIE2K(tLab, bLab));

			
			/* Measure and set ambient for viewing condition adjustment */
			} else if (c == '6') {
				if ((rv = dr->ambient(dr, &ambient, 1)) != 0) {
					if (rv == 8) {
						printf("Instrument doesn't have an ambient reading capability\n");
					} else {
						dr->del(dr);
						error("ambient measure failed with '%s'\n",disprd_err(rv));
					}
				} else {
					printf("Measured ambient level = %.1f Lux\n",ambient * 3.141592654);
				}

			} else if (c == '7') {
				if (!verb) {		/* Tell user command has been accepted */
					if (verify == 2)
						printf("Commencing device verification\n");
					else
						printf("Commencing device calibration\n");
				}
				break;
			} else if (c == '8' || c == 0x03 || c == 0x1b) {
				printf("Exiting\n");
				dr->del(dr);
				exit(0);
			}
		}

		/* Make sure drift comp. is set to the command line options */
		dr->change_drift_comp(dr, bdrift, wdrift);
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	/* Take a small number of readings, and compute basic */
	/* informations such as black & white, white target, */
	/* aproximate matrix based display forward and reverse model. */

	/* Read the base test set */
	{
		icmXYZNumber mrd;		/* Number for matrix */
		icmXYZNumber mgn;
		icmXYZNumber mbl;
		icmXYZNumber mwh;
		ramdac *or = NULL;

		col base[6] = {	/* Base set of test colors */
			{ 0.0, 0.0, 0.0 },		/* 0 - Black */
			{ 1.0, 0.0, 0.0 },		/* 1 - Red */
			{ 0.0, 1.0, 0.0 },		/* 2 - Green */
			{ 0.0, 0.0, 1.0 },		/* 3 - Blue */
			{ 1.0, 1.0, 1.0 },		/* 4 - White */
			{ 0.0, 0.0, 0.0 }		/* 5 - Black */
		};

		if (verb) {
			if (verify == 2)
				printf("Commencing device verification\n");
			else
				printf("Commencing device calibration\n");
		}

		/* Switch to native for this, so the black calc is realistic. */
		/* (Should we really get black aim from previous .cal though ???) */
		if (verify == 2) {
			if ((or = dr->dw->get_ramdac(dr->dw)) != NULL) {
				ramdac *r;
				if (verb) printf("Switching to native response for base measurements\n");
				r = or->clone(or);
				r->setlin(r);
				dr->dw->set_ramdac(dr->dw, r, 0);
				r->del(r);
			}
		}

		if ((rv = dr->read(dr, base, 6, 1, 6, 1, 0)) != 0) {
			dr->del(dr);
			error("display read failed with '%s'\n",disprd_err(rv));
		} 

		/* Restore the cal we're verifying */
		if (verify == 2 && or != NULL) {
			if (verb) printf("Switching back to calibration being verified\n");
			dr->dw->set_ramdac(dr->dw, or, 0);
			or->del(or);
		}

		if (base[0].aXYZ_v == 0) {
			dr->del(dr);
			error("Failed to get an absolute XYZ value from the instrument!\n");
		}

		/* Average black relative from 2 readings */
		x.bk[0] = 0.5 * (base[0].aXYZ[0] + base[5].aXYZ[0]);
		x.bk[1] = 0.5 * (base[0].aXYZ[1] + base[5].aXYZ[1]);
		x.bk[2] = 0.5 * (base[0].aXYZ[2] + base[5].aXYZ[2]);

		if (verb) {
			printf("Black = XYZ %6.2f %6.2f %6.2f\n",x.bk[0],x.bk[1],x.bk[2]);
			printf("Red   = XYZ %6.2f %6.2f %6.2f\n",base[1].aXYZ[0], base[1].aXYZ[1], base[1].aXYZ[2]);
			printf("Green = XYZ %6.2f %6.2f %6.2f\n",base[2].aXYZ[0], base[2].aXYZ[1], base[2].aXYZ[2]);
			printf("Blue  = XYZ %6.2f %6.2f %6.2f\n",base[3].aXYZ[0], base[3].aXYZ[1], base[3].aXYZ[2]);
			printf("White = XYZ %6.2f %6.2f %6.2f\n",base[4].aXYZ[0], base[4].aXYZ[1], base[4].aXYZ[2]);
		}

		/* Copy other readings into place */
		dispLum = base[4].aXYZ[1];				/* White Y */
		icmAry2Ary(x.bk, base[0].aXYZ);
		icmAry2Ary(x.wh, base[4].aXYZ);
		icmAry2XYZ(x.twN, x.wh);	/* Use this as Lab reference white until we establish target */

		icmAry2XYZ(mrd, base[1].aXYZ);
		icmAry2XYZ(mgn, base[2].aXYZ);
		icmAry2XYZ(mbl, base[3].aXYZ);
		icmAry2XYZ(mwh, base[4].aXYZ);

		/* Setup forward matrix */
		if (icmRGBprim2matrix(mwh, mrd, mgn, mbl, x.fm)) {
			dr->del(dr);
			error("Aprox. fwd matrix unexpectedly singular\n");
		}

#ifdef DEBUG
		if (verb) {
			printf("Forward matrix is:\n");
			printf("%f %f %f\n", x.fm[0][0], x.fm[0][1], x.fm[0][2]);
			printf("%f %f %f\n", x.fm[1][0], x.fm[1][1], x.fm[1][2]);
			printf("%f %f %f\n", x.fm[2][0], x.fm[2][1], x.fm[2][2]);
		}
#endif

		/* Compute bwd matrix */
		if (icmInverse3x3(x.bm, x.fm)) {
			dr->del(dr);
			error("Inverting aprox. fwd matrix failed");
		}
	}

	/* Now do some more readings, to compute the basic per channel */
	/* transfer characteristics, and then a device model. */
	if (verify != 2 && !doupdate) {
		col *cols;				/* Read 4 x isteps patches from display */
		sxyz *asrgb[4];			/* samples for r, g, b & w */

		if ((cols = (col *)malloc(isteps * 4 * sizeof(col))) == NULL) {
			dr->del(dr);
			error ("Malloc of array of readings failed");
		}
		for (j = 0; j < 4; j++) {
			if ((asrgb[j] = (sxyz *)malloc(isteps * sizeof(sxyz))) == NULL) {
				free(cols);
				dr->del(dr);
				error ("Malloc of array of readings failed");
			}
		}

		/* Set the device colors to read */
		for (i = 0; i < isteps; i++) {
			double vv;

#if defined(__APPLE__) && defined(__POWERPC__)
			gcc_bug_fix(i);
#endif
			vv = i/(isteps - 1.0);
			vv = pow(vv, MOD_DIST_POW);
			for (j = 0; j < 4; j++) {
				cols[i * 4 + j].r = cols[i * 4 + j].g = cols[i * 4 + j].b = 0.0;
				if (j == 0)
					cols[i * 4 + j].r = vv;
				else if (j == 1)
					cols[i * 4 + j].g = vv;
				else if (j == 2)
					cols[i * 4 + j].b = vv;
				else
					cols[i * 4 + j].r = cols[i * 4 + j].g = cols[i * 4 + j].b = vv;
			}
		}

		/* Read the patches */
		if ((rv = dr->read(dr, cols, isteps * 4, 1, isteps * 4, 1, 0)) != 0) {
			free(cols); free(asrgb[0]); free(asrgb[1]); free(asrgb[2]); free(asrgb[3]);
			dr->del(dr);
			error("display read failed with '%s'\n",disprd_err(rv));
		} 

		/* Transfer readings to asrgb[] */
		for (i = 0; i < isteps; i++) {
			double vv =  cols[i * 4 + 0].r;
			for (j = 0; j < 4; j++) {
//printf("~1 R = %f, G = %f, B = %f, XYZ = %f %f %f\n",
//cols[i * 4 + j].r, cols[i * 4 + j].g, cols[i * 4 + j].b, cols[i * 4 + j].aXYZ[0], cols[i * 4 + j].aXYZ[1], cols[i * 4 + j].aXYZ[2]);
				asrgb[j][i].v = vv;
				asrgb[j][i].xyz[0] = cols[i * 4 + j].aXYZ[0];
				asrgb[j][i].xyz[1] = cols[i * 4 + j].aXYZ[1];
				asrgb[j][i].xyz[2] = cols[i * 4 + j].aXYZ[2];
			}
		}

		/* Convert RGB channel samples to curves */
		{
			mcvco *sdv;		/* Points used to create cvs[], RGB */
			double blrgb[3];
			double *op;		/* Parameters to optimise */
			double *sa;		/* Search area */
			double re;		/* Residual error */

			/* Transform measured black back to linearised RGB values */
			icmMulBy3x3(blrgb, x.bm, x.bk);
//printf("~1 model black should be %f %f %f\n", x.bk[0], x.bk[1], x.bk[2]);
//printf("~1 linearised RGB should be %f %f %f\n", blrgb[0], blrgb[1], blrgb[2]);

			if ((sdv = malloc(sizeof(mcvco) * isteps)) == NULL) {
				free(cols); free(asrgb[0]); free(asrgb[1]); free(asrgb[2]); free(asrgb[3]);
				dr->del(dr);
				error ("Malloc of scattered data points failed");
			}
			for (k = 0; k < 3; k++) {	/* Create the model curves */
				for (i = 0; i < isteps; i++) {
					sdv[i].p = asrgb[k][i].v;
					sdv[i].v = ICMNORM3(asrgb[k][i].xyz);
					sdv[i].w = 1.0;
//printf("~1 chan %d, entry %d, p = %f, v = %f from XYZ %f %f %f\n",
//k,i,x.sdv[k][i].p,x.sdv[k][i].v, asrgb[k][i].xyz[0], asrgb[k][i].xyz[1], asrgb[k][i].xyz[2]);
				}
				if ((x.dcvs[k] = new_mcv()) == NULL) {
					free(cols); free(asrgb[0]); free(asrgb[1]); free(asrgb[2]); free(asrgb[3]);
					dr->del(dr);
					error("new_mcv x.dcvs[%d] failed",k);
				}
				x.dcvs[k]->fit(x.dcvs[k], 0, fitord, sdv, isteps, 5.0);

				/* Scale the whole curve so the output is scaled to 1.0 */
				x.dcvs[k]->force_scale(x.dcvs[k], 1.0);

				/* Force curves to produce this lrgb for 0.0 */
				x.dcvs[k]->force_0(x.dcvs[k], blrgb[k]);
			}
			free(sdv);

			/* Setup list of reference points ready for optimisation */
			x.nrp = 4 * isteps;
			if ((x.rp = (optref *)malloc(sizeof(optref) * x.nrp)) == NULL) {
				free(cols); free(asrgb[0]); free(asrgb[1]); free(asrgb[2]); free(asrgb[3]);
				dr->del(dr);
				error ("Malloc of measurement reference points failed");
			}
			for (k = 0; k < 4; k++) {
				for (i = 0; i < isteps; i++) {
					int ii = k * isteps + i;
					double v[3];
				
					v[0] = v[1] = v[2] = 0.0;
					if (k == 0)
						v[k] = asrgb[k][i].v;
					else if (k == 1)
						v[k] = asrgb[k][i].v;
					else if (k == 2)
						v[k] = asrgb[k][i].v;
					else
						v[0] = v[1] = v[2] = asrgb[k][i].v;
					icmAry2Ary(x.rp[ii].dev, v);
					icmXYZ2Lab(&x.twN, x.rp[ii].lab, asrgb[k][i].xyz);
					if (k == 3)		/* White */
						x.rp[ii].w = 0.5;
					else
						x.rp[ii].w = 0.16667;
				}
			}

			/* Get parameters and setup for optimisation */
			op = dev_get_params(&x);
			if ((sa = malloc(x.np * sizeof(double))) == NULL) {
				free(cols); free(asrgb[0]); free(asrgb[1]); free(asrgb[2]); free(asrgb[3]);
				dr->del(dr);
				error ("Malloc of scattered data points failed");
			}
			
			for (i = 0; i < x.np; i++)
				sa[i] = 0.1;

			/* Do optimisation */
#ifdef NEVER
			if (powell(&re, x.np, op, sa, 1e-5, 3000, dev_opt_func, (void *)&x, NULL, NULL) != 0) {
				free(cols); free(asrgb[0]); free(asrgb[1]); free(asrgb[2]); free(asrgb[3]);
				dr->del(dr);
				error ("Model powell failed, re = %f",re);
			}
#else
			if (conjgrad(&re, x.np, op, sa, 1e-5, 3000,
			                         dev_opt_func, dev_dopt_func, (void *)&x, NULL, NULL) != 0) {
				if (re > 1e-2) {
					free(cols); free(asrgb[0]); free(asrgb[1]); free(asrgb[2]); free(asrgb[3]);
					dr->del(dr);
					error("Model conjgrad failed, residual error = %f",re);
				} else
					warning("Model conjgrad failed, residual error = %f",re);
			}
#endif

			/* Put optimised parameters in place */
			dev_put_params(&x, op);

			free(x.rp);
			x.rp = NULL;
			x.nrp = 0;
			free(x.dtin_iv);		/* Free temporary arrays */
			x.dtin_iv = NULL;
			free(sa);
			free(op);
		}

#ifdef DEBUG_PLOT
		/* Plot the current calc curves */
		{
			#define	XRES 256
			double xx[XRES];
			double yy[3][XRES];
			double xyz[3];
			for (i = 0; i < XRES; i++) {
				xx[i] = i/(XRES-1.0);
				for (j = 0; j < 3; j++)
					yy[j][i] = x.dcvs[j]->interp(x.dcvs[j], xx[i]);
			}
			printf("Channel curves\n");
			do_plot(xx,yy[0],yy[1],yy[2],XRES);
			#undef XRES
		}
#endif

		/* We're done with cols[] and asrgb[] */
		free(cols); free(asrgb[0]); free(asrgb[1]); free(asrgb[2]); free(asrgb[3]);
	}

#ifdef CHECK_MODEL
	/* Check how well our fwd model agrees with the device */
	if (verify != 2) {
		col set[3];					/* Variable to read up to 3 values from the display */
		int nn = 27;
		double alab[3], mxyz[3], mlab[3];	/* Actual and model Lab */
		double mnerr;		/* Maximum neutral error */
		double mnv;			/* Value where maximum error is */
		double anerr;		/* Average neutral error */

		mnerr = anerr = 0.0;
		/* !!! Should change this to single batch to work better with drift comp. !!! */
		for (i = 0; i < nn; i++) {
			double vv, v[3];
			double de;

			vv = i/(nn - 1.0);
			vv = pow(vv, CHECK_DIST_POW);
			v[0] = v[1] = v[2] = vv;
			set[0].r = set[0].g = set[0].b = vv;

			if ((rv = dr->read(dr, set, 1, i+1, nn, 1, 0)) != 0) {
				dr->del(dr);
				error("display read failed with '%s'\n",disprd_err(rv));
			} 
			icmXYZ2Lab(&x.twN, alab, set[0].aXYZ);
		
			fwddev(&x, mxyz, v);
			icmXYZ2Lab(&x.twN, mlab, mxyz);

			de = icmCIE2K(mlab, alab);
			if (de > mnerr) {
				mnerr = de;
				mnv = vv;
			}
			anerr += de;
			
			printf("RGB %.3f -> XYZ %.2f %.2f %.2f, model %.2f %.2f %.2f\n",
			vv, set[0].aXYZ[0], set[0].aXYZ[1], set[0].aXYZ[2], mxyz[0], mxyz[1], mxyz[2]);

			printf("RGB %.3f -> Lab %.2f %.2f %.2f, model %.2f %.2f %.2f, DE %f\n",
			vv, alab[0], alab[1], alab[2], mlab[0], mlab[1], mlab[2],de);
		}
		anerr /= (double)nn;
		printf("Model maximum error (@ %f) = %f deltaE\n",mnv, mnerr);
		printf("Model average error = %f deltaE\n",anerr);
	}
#endif /* CHECK_MODEL */

	/* Figure out our calibration curve parameter targets */
	if (!doupdate) {

		/* Figure out the target white point */
		if (wpx > 0.0 || wpy > 0.0) {	/* xy coordinates */
			double Yxy[3];
			Yxy[0] = 1.0;
			Yxy[1] = wpx;
			Yxy[2] = wpy;
			icmYxy2XYZ(x.twh, Yxy);

		} else if (temp > 0.0) {		/* Daylight color temperature */
			if (planckian)
				rv = icx_ill_sp2XYZ(x.twh, icxOT_default, NULL, icxIT_Ptemp, temp, NULL);
			else
				rv = icx_ill_sp2XYZ(x.twh, icxOT_default, NULL, icxIT_Dtemp, temp, NULL);
			if (rv != 0)
				error("Failed to compute XYZ of target color temperature %f\n",temp);
//printf("~1 Raw target from temp %f XYZ = %f %f %f\n",temp,x.twh[0],x.twh[1],x.twh[2]);
		} else {						/* Native white */
			x.twh[0] = x.wh[0]/x.wh[1];
			x.twh[1] = x.wh[1]/x.wh[1];
			x.twh[2] = x.wh[2]/x.wh[1];
		}
		x.nwh[0] = x.twh[0];
		x.nwh[1] = x.twh[1];
		x.nwh[2] = x.twh[2];

		/* Convert it to absolute white target */
		if (tbright > 0.0) {			/* Given brightness */
			x.twh[0] *= tbright;
			x.twh[1] *= tbright;
			x.twh[2] *= tbright;
		} else {						/* Native/maximum brightness */
			x.twh[0] *= x.wh[1];
			x.twh[1] *= x.wh[1];
			x.twh[2] *= x.wh[1];
			if (verb)
				printf("Initial native brightness target = %f cd/m^2\n", x.twh[1]);
		}

		/* Now make sure the target white will fit in gamut. */
		if (verify != 2 &&
		   ((tbright > 0.0 && invdev(&x, NULL, x.twh) > 0.0) /* Defined brightness and clips */
		 || (tbright <= 0.0 && x.nat == 0))) {		/* Max non-native white */
			double rgb[3];
			double scale = 0.5;
			double sa = 0.1;

			if (powell(NULL, 1, &scale, &sa, 1e-5, 200, wp_opt_func, (void *)&x, NULL, NULL) != 0)
				error ("WP scale powell failed");

			x.twh[0] *= scale;
			x.twh[1] *= scale;
			x.twh[2] *= scale;
			invdev(&x, rgb, x.twh);
			if (verb) {
				printf("Had to scale brightness from %f to %f to fit within gamut,\n",x.twh[1]/scale, x.twh[1]);
				printf("corresponding to RGB %f %f %f\n",rgb[0],rgb[1],rgb[2]);
			}
		}

		if (verb)
			printf("Target white value is XYZ %f %f %f\n",x.twh[0],x.twh[1],x.twh[2]);
	}

	/* Need this for Lab conversions */
	icmAry2XYZ(x.twN, x.twh);

	/* Figure out the black point target */
	{
		double tbL[3];
		double tbkLab[3]; 

		icmXYZ2Lab(&x.twN, tbkLab, x.bk);	/* Convert measured black to Lab */
	
//printf("~1 black point Lab = %f %f %f\n", tbkLab[0], tbkLab[1], tbkLab[2]);

		/* Now blend the a* b* with that of the target white point */
		tbL[0] = tbkLab[0];
		tbL[1] = bkcorrect * 0.0 + (1.0 - bkcorrect) * tbkLab[1];
		tbL[2] = bkcorrect * 0.0 + (1.0 - bkcorrect) * tbkLab[2];

//printf("~1 blended black Lab = %f %f %f\n", tbL[0], tbL[1], tbL[2]);
	
		if (bkbright > 0.0 && bkbright <= x.bk[1] || (2.0 * bkbright) >= x.twh[1])
			warning("Black brigtness ignored because it is out of range");
		else if (bkbright > 0.0) {
			double bkbxyz[3], bkbLab[3], vv, bl;
			/* Figure out the L value of the target brightness */
			bkbxyz[0] = 0.0;
			bkbxyz[1] = bkbright;
			bkbxyz[2] = 0.0;
			icmXYZ2Lab(&x.twN, bkbLab, bkbxyz);
			
			/* Do crossover to white neutral */
			vv = bkbLab[0] / (100.0 - tbL[0]);
			bl = pow((1.0 - vv), x.nbrate);		/* Crossover near the black */
			tbL[0] = bkbLab[0];
			tbL[1] = (1.0 - bl) * 0.0 + bl * tbL[1];
			tbL[2] = (1.0 - bl) * 0.0 + bl * tbL[2];
//printf("~1 brighted black Lab = %f %f %f\n", tbL[0], tbL[1], tbL[2]);
		}

		/* And make this the black hue to aim for */
		icmLab2XYZ(&x.twN, x.tbk, tbL);
		icmAry2XYZ(x.tbN, x.tbk);
		if (verb)
			printf("Adjusted target black XYZ %.2f %.2f %.2f, Lab %.2f %.2f %.2f\n",
	        x.tbk[0], x.tbk[1], x.tbk[2], tbL[0], tbL[1], tbL[2]);
	}

	/* Figure out the gamma curve black offset value */
	/* that will give us the black level we actually have. */
	{
		double yy, tby;			/* Target black y */
		
		/* Make target black Y as high as necessary */
		/* to get the black point hue */
		/* ????? should do this by increasing L* until XYZ > x.bk ????? */
		tby = x.bk[1];
//printf("Target Y from Y = %f\n",tby);
		yy = x.bk[0] * x.tbk[1]/x.tbk[0];
//printf("Target Y from X = %f\n",yy);
		if (yy > tby)
			tby = yy;
		yy = x.bk[2] * x.tbk[1]/x.tbk[2];
//printf("Target Y from Z = %f\n",yy);
		if (yy > tby)
			tby = yy;

		if (x.tbk[1] > tby)		/* If target is already high enough */
			tby = x.tbk[1];

		if (verb) {
			double tbp[3], tbplab[3];
	        tbp[0] = x.tbk[0] * tby/x.tbk[1];
			tbp[1] = tby;
			tbp[2] = x.tbk[2] * tby/x.tbk[1];
			icmXYZ2Lab(&x.twN, tbplab, tbp);
			printf("Target black after min adjust: XYZ %.3f %.3f %.3f, Lab %.3f %.3f %.3f\n",
			        tbp[0], tbp[1], tbp[2], tbplab[0], tbplab[1], tbplab[2]);
		}

		/* Figure out the x.gioff and egamma needed to get this x.gooff and gamma */
		x.gooff = tby / x.twh[1];			/* Convert to relative */

		/* tech_gamma() does the hard work */
		tech_gamma(&x, &x.egamma, &x.gooff, &x.gioff, egamma, gamma, x.gooff);
	}

	if (verb)
		printf("Gamma curve input offset = %f, output offset = %f, power = %f\n",x.gioff,x.gooff,x.egamma);

	/* For ambient light compensation, we make use of CIECAM02 */
	if (ambient > 0.0) {
		double xyz[3], Jab[3];
		double t1, t0, a1, a0;

		/* Setup default source viewing conditions */
		if ((x.svc = new_icxcam(cam_default)) == NULL
		 || (x.dvc = new_icxcam(cam_default)) == NULL) {
			error("Failed to create source and destination CAMs");
		}
	
		switch(x.gammat) {
			case gt_power:	/* There's nothing obvious for these cases, */
			case gt_Lab:	/* So default to a computerish source viewing condition */

			case gt_sRGB:	/* sRGB standard viewing conditions */
				x.svc->set_view(x.svc, vc_none,
					x.nwh,					/* Display normalised white point */
					0.2 * 80.0,				/* Adapting luminence, 20% of display 80 cd/m^2 */
					0.2,					/* Background relative to reference white */
					80.0,					/* Display is 80 cd/m^2 */
			        0.01, x.nwh,			/* 1% flare same white point */
					0);
				break;

			case gt_Rec709:
			case gt_SMPTE240M:	/* Television studio conditions */
				x.svc->set_view(x.svc, vc_none,
					x.nwh,					/* Display normalised white point */
					0.2 * 1000.0/3.1415,	/* Adapting luminence, 20% of 1000 lux in cd/m^2 */
					0.2,					/* Background relative to reference white */
					1000.0/3.1415,			/* Luminance of white in the Image field (cd/m^2) */
			        0.01, x.nwh,			/* 1% flare same white point */
					0);
				break;

			default:
				error("Unknown gamma type");
		}
		/* The display we're calibratings situation */
		x.dvc->set_view(x.dvc, vc_none,
			x.nwh,				/* Display normalised white point */
			0.2 * ambient,		/* Adapting luminence, 20% of ambient in cd/m^2 */
			0.2,				/* Background relative to reference white */
			x.twh[1],			/* Target white level (cd/m^2) */
	        0.01, x.nwh,		/* 1% flare same white point */
			0);

		/* Compute the normalisation values */
		x.svc->XYZ_to_cam(x.svc, Jab, x.nwh);		/* Relative white point */
		x.dvc->cam_to_XYZ(x.dvc, xyz, Jab);
		t1 = x.nwh[1];
		a1 = xyz[1];

		xyz[0] = x.tbk[1]/x.twh[1] * x.nwh[0];
		xyz[1] = x.tbk[1]/x.twh[1] * x.nwh[1];
		xyz[2] = x.tbk[1]/x.twh[1] * x.nwh[2];
		t0 = xyz[1];
		x.svc->XYZ_to_cam(x.svc, Jab, xyz);		/* Relative black Y */
		x.dvc->cam_to_XYZ(x.dvc, xyz, Jab);
		a0 = xyz[1];

//printf("~1 t1 = %f, t0 = %f\n",t1,t0);
//printf("~1 a1 = %f, a0 = %f\n",a1,a0);
		x.vn1 = (t1 - t0)/(a1 - a0);		/* Scale factor */
		x.vn0 = t0 - (a0 * x.vn1);			/* Then offset */
//printf("~1 vn1 = %f, vn0 = %f\n",x.vn1, x.vn0);
//printf("~1 fix a1 = %f, should be = %f\n",a1 * x.vn1 + x.vn0, t1);
//printf("~1 fix a0 = %f, should be = %f\n",a0 * x.vn1 + x.vn0, t0);

		x.vc = 1;

		/* Compute aproximate power of viewing transform */
		if (verb) {
			double v;
			v = view_xform(&x, 0.5);
			v = log(v) / log(0.5);
			printf("Viewing conditions adjustment aprox. power = %f\n",v);
		}
#ifdef NEVER
{
	int i;

	printf("~1 viewing xtranform:\n");
	for (i = 0; i <= 10; i++) {
		double w, v = i/10.0;
		
		w = view_xform(&x, v);
		printf("~1 in %f -> %f\n",v,w); 
	}
}
#endif /* NEVER */
	}

	/* - - - - - - - - - - - - - - - - - - - - - */
	/* Start with a scaled down number of test points and refine threshold, */
	/* and double/halve these on each iteration. */
	if (verb && verify != 2)
		printf("Total Iteration %d, Final Samples = %d Final Repeat threshold = %f\n",
		        mxits, rsteps, errthr);
	if (verify == 2) {
		rsteps = VER_RES;
		errthr = 0.0;
	} else {
		rsteps /= (1 << (mxits-1));
		errthr *= pow((double)(1 << (mxits-1)), THRESH_SCALE_POW);
	}

	/* Setup the initial calibration test point values */
	init_csamp(&asgrey, &x, doupdate, verify, verify == 2 ? 1 : 0, rsteps);
	
	/* Calculate the initial calibration curve values */
	if (verify != 2 && !doupdate) {
		int nsamp = 128;
		mcvco *sdv[3];				/* Scattered data for creating mcv */

		for (j = 0; j < 3; j++) {
			if ((x.rdac[j] = new_mcv()) == NULL) {
				dr->del(dr);
				error("new_mcv x.rdac[%d] failed",j);
			}
		}

		for (j = 0; j < 3; j++) {
			if ((sdv[j] = malloc(sizeof(mcvco) * rsteps)) == NULL) {
				dr->del(dr);
				error ("Malloc of scattered data points failed");
			}
		}

		if (verb)
			printf("Creating initial calibration curves...\n");

		/* Copy the sample points */
		for (i = 0; i < rsteps; i++) {
			for (j = 0; j < 3; j++) {
				sdv[j][i].p = asgrey.s[i].v;
				sdv[j][i].v = asgrey.s[i].rgb[j];
				sdv[j][i].w = 1.0;
			}
		}
		if (x.nat)		/* Make curve go thought white if possible */
			sdv[0][rsteps-1].w = sdv[1][rsteps-1].w = sdv[2][rsteps-1].w = 50.0;

		/* Create an initial set of RAMDAC curves */
		for (j = 0; j < 3; j++)
			x.rdac[j]->fit(x.rdac[j], 0, fitord, sdv[j], rsteps, RDAC_SMOOTH);

		/* Make sure that if we are using native brightness and white point, */
		/* that the curves go to a perfect 1.0 ... */
		if (x.nat) {
			for (j = 0; j < 3; j++)
				x.rdac[j]->force_1(x.rdac[j], 1.0);
		}

		for (j = 0; j < 3; j++)
			free (sdv[j]);
	}

#ifdef DEBUG_PLOT
	/* Plot the initial curves */
	if (verify != 2) {
		#define	XRES 255
		double xx[XRES];
		double y1[XRES];
		double y2[XRES];
		double y3[XRES];
		double rgb[3];
		for (i = 0; i < XRES; i++) {
			double drgb[3], rgb[3];
			xx[i] = i/(XRES-1.0);
			rgb[0] = rgb[1] = rgb[2] = xx[i];
			for (j = 0; j < 3; j++)
				drgb[j] = x.rdac[j]->interp(x.rdac[j], rgb[j]);
			y1[i] = drgb[0];
			y2[i] = drgb[1];
			y3[i] = drgb[2];
		}
		printf("Initial ramdac curves\n");
		do_plot(xx,y1,y2,y3,XRES);
		#undef XRES
	}
#endif

	dr->reset_targ_w(dr);			/* Reset white drift target at start of main cal. */

	/* Now we go into the main verify & refine loop */
	for (it = verify == 2 ? mxits : 0; it < mxits || verify != 0;
	     rsteps *= 2, errthr /= (it < mxits) ? pow(2.0,THRESH_SCALE_POW) : 1.0, it++)  {
		int totmeas = 0;		/* Total number of measurements in this pass */
		col set[3];				/* Variable to read one to three values from the display */

		/* Verify pass */
		if (it >= mxits)
			rsteps = VER_RES;		/* Fixed verification resolution */
		else
			thrfail = 0; /* Not verify pass */


		/* re-init asgrey if the number of test points has changed */
		reinit_csamp(&asgrey, &x, verify, (verify == 2 || it >= mxits) ? 1 : 0, rsteps);

		if (verb) {
			if (it >= mxits)
				printf("Doing verify pass with %d sample points\n",rsteps);
			else
				printf("Doing iteration %d with %d sample points and repeat threshold of %f DE\n",
				                                                             it+1,rsteps, errthr);
		}
		/* Read and adjust each step */
		/* Do this white to black to track drift in native white point */
		for (i = rsteps-1; i >= 0; i--) {
			double rpt;
			double peqXYZ[3];		/* Previous steps equivalent aim point */
			double bestrgb[3];		/* In case we fail */
			double bestxyz[3];
			double prevde = 1e7;
			double bestde = 1e7;
			double bestdc = 1e7;
			double bestpeqde = 1e7;
			double besthde = 1e7;
			double rgain = REFINE_GAIN;	/* Scale down if lots of repeats */
			int mjac = 0;				/* We measured the Jacobian */

			/* Setup a second termination threshold chriteria based on */
			/* the delta E to the previous step point for the last pass. */
			if (i == (rsteps-1) || it < (mxits-1)) {
				icmAry2Ary(peqXYZ, asgrey.s[i].tXYZ);	/* Its own aim point */
			} else {
				double Lab1[3], Lab2[3], Lab3[3];
				icmXYZ2Lab(&x.twN, Lab1, asgrey.s[i+1].XYZ);
				icmXYZ2Lab(&x.twN, Lab2, asgrey.s[i].tXYZ);
				Lab3[0] = Lab2[0];
				Lab3[1] = 0.5 * (Lab1[1] + Lab2[1]);	/* Compute aim point between actual target */
				Lab3[2] = 0.5 * (Lab1[2] + Lab2[2]);	/* and previous point. */
				icmLab2XYZ(&x.twN, asgrey.s[i].tXYZ, Lab3);
				Lab1[0] = Lab2[0];		/* L of current target with ab of previous as 2nd threshold */
				icmLab2XYZ(&x.twN, peqXYZ, Lab1);
			}

			/* Until we meet the necessary accuracy or give up */
			for (rpt = 0;rpt < MAX_RPTS; rpt++) {
				int gworse = 0;		/* information flag */
				double wde;			/* informational */

				set[0].r = asgrey.s[i].rgb[0];
				set[0].g = asgrey.s[i].rgb[1];
				set[0].b = asgrey.s[i].rgb[2];
				set[0].id = NULL;

				/* Read patches (no auto cr in case we repeat last patch) */
				if ((rv = dr->read(dr, set, 1, rsteps-i, rsteps, 0, 0)) != 0) {
					dr->del(dr);
					error("display read failed with '%s'\n",disprd_err(rv));
				} 
				totmeas++;
	
				icmAry2Ary(asgrey.s[i].pXYZ, asgrey.s[i].XYZ);	/* Remember previous XYZ */
				icmAry2Ary(asgrey.s[i].XYZ, set[0].aXYZ);		/* Transfer current reading */

				/* If native white and we've just measured it, */
				/* and we're not doing a verification, */
				/* adjust all the other point targets txyz to track the white. */
				if (x.nat && i == (rsteps-1) && it < mxits && asgrey.s[i].v == 1.0) {

					icmAry2Ary(x.twh, asgrey.s[i].XYZ);		/* Set current white */
					icmAry2XYZ(x.twN, x.twh);				/* Need this for Lab conversions */
					init_csamp_txyz(&asgrey, &x, 1);		/* Recompute txyz's */
					icmAry2Ary(peqXYZ, asgrey.s[i].tXYZ);	/* Fix peqXYZ */
//printf("~1 Just reset target white to native white\n");
					if (wdrift) {	/* Make sure white drift is reset on next read. */
						dr->reset_targ_w(dr);			/* Reset this */
					}
				}

				/* Compute the current change wanted to hit target */
				icmSub3(asgrey.s[i].deXYZ, asgrey.s[i].tXYZ, asgrey.s[i].XYZ);
				asgrey.s[i].de = icmXYZLabDE(&x.twN, asgrey.s[i].tXYZ, asgrey.s[i].XYZ);
				asgrey.s[i].peqde = icmXYZLabDE(&x.twN, peqXYZ, asgrey.s[i].XYZ);
				asgrey.s[i].hde = 0.8 * asgrey.s[i].de + 0.2 * asgrey.s[i].peqde;
				/* Eudclidian difference of XYZ, because this doesn't always track Lab */
				asgrey.s[i].dc = icmLabDE(asgrey.s[i].tXYZ, asgrey.s[i].XYZ);

				/* Compute change from last XYZ */
				icmSub3(asgrey.s[i].dXYZ, asgrey.s[i].XYZ, asgrey.s[i].pXYZ);

#ifdef DEBUG
				printf("\n\nTest point %d, v = %f\n",rsteps - i,asgrey.s[i].v);
				printf("Current rgb %f %f %f -> XYZ %f %f %f, de %f, dc %f\n", 
				asgrey.s[i].rgb[0], asgrey.s[i].rgb[1], asgrey.s[i].rgb[2],
				asgrey.s[i].XYZ[0], asgrey.s[i].XYZ[1], asgrey.s[i].XYZ[2],
				asgrey.s[i].de, asgrey.s[i].dc);
				printf("Target XYZ %f %f %f, delta needed %f %f %f\n", 
				asgrey.s[i].tXYZ[0], asgrey.s[i].tXYZ[1], asgrey.s[i].tXYZ[2],
				asgrey.s[i].deXYZ[0], asgrey.s[i].deXYZ[1], asgrey.s[i].deXYZ[2]);
				if (rpt > 0) {
					printf("Intended XYZ change %f %f %f, actual change %f %f %f\n", 
					asgrey.s[i].pdXYZ[0], asgrey.s[i].pdXYZ[1], asgrey.s[i].pdXYZ[2],
					asgrey.s[i].dXYZ[0], asgrey.s[i].dXYZ[1], asgrey.s[i].dXYZ[2]);
				}
#endif

				if (it < mxits) {		/* Not verify, apply correction */
					int impj = 0;		/* We improved the Jacobian */
					int dclip = 0;		/* We clipped the new RGB */
#ifdef ADJ_JACOBIAN
					int isclipped = 0;

#ifndef CLIP		/* Check for cliping */
					/* Don't try and update the Jacobian if the */	
					/* device values are going out of gamut, */
					/* and being clipped without Jac correction being aware. */
					for (j = 0; j < 3; j++) {
						if (asgrey.s[i].rgb[j] <= 0.0 || asgrey.s[i].rgb[j] >= 1.0) {
							isclipped = 1;
							break;
						}
					}
#endif /* !CLIP */

#ifdef REMEAS_JACOBIAN
					/* If the de hasn't improved, try and measure the Jacobian */
					if (it < (rsteps-1) && mjac == 0 && asgrey.s[i].de > (0.8 * prevde)) {
						double dd;
						if (asgrey.s[i].v < 0.5)
							dd = 0.05;
						else
							dd= -0.05;
						set[0].r = asgrey.s[i].rgb[0] + dd;
						set[0].g = asgrey.s[i].rgb[1];
						set[0].b = asgrey.s[i].rgb[2];
						set[0].id = NULL;
						set[1].r = asgrey.s[i].rgb[0];
						set[1].g = asgrey.s[i].rgb[1] + dd;
						set[1].b = asgrey.s[i].rgb[2];
						set[1].id = NULL;
						set[2].r = asgrey.s[i].rgb[0];
						set[2].g = asgrey.s[i].rgb[1];
						set[2].b = asgrey.s[i].rgb[2] + dd;
						set[2].id = NULL;

						if ((rv = dr->read(dr, set, 1, rsteps-i, rsteps, 0, 0)) != 0
						 || (rv = dr->read(dr, set+1, 1, rsteps-i, rsteps, 0, 0)) != 0
						 || (rv = dr->read(dr, set+2, 1, rsteps-i, rsteps, 0, 0)) != 0) {
							dr->del(dr);
							error("display read failed with '%s'\n",disprd_err(rv));
						} 
						totmeas += 3;

						/* Matrix organization is J[XYZ][RGB] for del RGB->del XYZ*/
						for (j = 0; j < 3; j++) {
							asgrey.s[i].j[0][j] = (set[j].aXYZ[0] - asgrey.s[i].XYZ[0]) / dd;
							asgrey.s[i].j[1][j] = (set[j].aXYZ[1] - asgrey.s[i].XYZ[1]) / dd;
							asgrey.s[i].j[2][j] = (set[j].aXYZ[2] - asgrey.s[i].XYZ[2]) / dd;
						}
						if (icmInverse3x3(asgrey.s[i].ij, asgrey.s[i].j)) {
							/* Should repeat with bigger dd ? */
//printf("~1 matrix =\n");
//printf("~1    %f %f %f\n", asgrey.s[i].j[0][0], asgrey.s[i].j[0][1], asgrey.s[i].j[0][2]);
//printf("~1    %f %f %f\n", asgrey.s[i].j[1][0], asgrey.s[i].j[1][1], asgrey.s[i].j[1][2]);
//printf("~1    %f %f %f\n", asgrey.s[i].j[2][0], asgrey.s[i].j[2][1], asgrey.s[i].j[2][2]);
							if (verb)
								printf("dispcal: inverting Jacobian failed (3) - falling back\n");

							/* Revert to the initial Jacobian */
							icmCpy3x3(asgrey.s[i].ij, asgrey.s[i].fb_ij);
						}
						/* Restart at the best we've had */
						if (asgrey.s[i].hde > besthde) {
							asgrey.s[i].de = bestde;
							asgrey.s[i].dc = bestdc;
							asgrey.s[i].peqde = bestpeqde;
							asgrey.s[i].hde = besthde;
							asgrey.s[i].rgb[0] = bestrgb[0];
							asgrey.s[i].rgb[1] = bestrgb[1];
							asgrey.s[i].rgb[2] = bestrgb[2];
							asgrey.s[i].XYZ[0] = bestxyz[0];
							asgrey.s[i].XYZ[1] = bestxyz[1];
							asgrey.s[i].XYZ[2] = bestxyz[2];
							icmSub3(asgrey.s[i].deXYZ, asgrey.s[i].tXYZ, asgrey.s[i].XYZ);
						}
						mjac = 1;
						impj = 1;
					}
#endif /* REMEAS_JACOBIAN */

					/* Compute a correction to the Jacobian if we can. */
					/* (Don't do this unless we have a solid previous */
					/* reading for this patch) */ 
					if (impj == 0 && rpt > 0 && isclipped == 0) {
						double nsdrgb;			/* Norm squared of pdrgb */
						double spdrgb[3];		/* Scaled previous delta rgb */
						double dXYZerr[3];		/* Error in previous prediction */
						double jadj[3][3];		/* Adjustment to Jacobian */
						double tj[3][3];		/* Temp Jacobian */
						double itj[3][3];		/* Temp inverse Jacobian */

						/* Use Broyden's Formula */
						icmSub3(dXYZerr, asgrey.s[i].dXYZ, asgrey.s[i].pdXYZ);
//printf("~1 Jacobian error = %f %f %f\n", dXYZerr[0], dXYZerr[1], dXYZerr[2]);
						nsdrgb = icmNorm3sq(asgrey.s[i].pdrgb);
						/* If there was sufficient change in device values */
						/* to be above any noise: */ 
						if (nsdrgb >= (0.005 * 0.005)) {
							icmScale3(spdrgb, asgrey.s[i].pdrgb, 1.0/nsdrgb);
							icmTensMul3(jadj, dXYZerr, spdrgb);

#ifdef DEBUG
							/* Check that new Jacobian predicts previous delta XYZ */
							{
								double eXYZ[3];
	
								/* Make a full adjustment to temporary Jac */
								icmAdd3x3(tj, asgrey.s[i].j, jadj);
								icmMulBy3x3(eXYZ, tj, asgrey.s[i].pdrgb);
								icmSub3(eXYZ, eXYZ, asgrey.s[i].dXYZ);
								printf("Jac check resid %f %f %f\n", eXYZ[0], eXYZ[1], eXYZ[2]);
							}
#endif	/* DEBUG */

							/* Add part of our correction to actual Jacobian */
							icmScale3x3(jadj, jadj, JAC_COR_FACT);
							icmAdd3x3(tj, asgrey.s[i].j, jadj);
							if (icmInverse3x3(itj, tj) == 0) {		/* Invert OK */
								icmCpy3x3(asgrey.s[i].j, tj);		/* Use adjusted */
								icmCpy3x3(asgrey.s[i].ij, itj);
								impj = 1;
							}
//else printf("~1 ij failed\n");
						}
//else printf("~1 nsdrgb was below threshold\n");
					}
//else if (isclipped) printf("~1 no j update: rgb is clipped\n");
#endif	/* ADJ_JACOBIAN */

					/* Track the best solution we've found */
					if (asgrey.s[i].hde <= besthde) {
						bestde = asgrey.s[i].de;
						bestdc = asgrey.s[i].dc;
						bestpeqde = asgrey.s[i].peqde;
						besthde = asgrey.s[i].hde;
						bestrgb[0] = asgrey.s[i].rgb[0];
						bestrgb[1] = asgrey.s[i].rgb[1];
						bestrgb[2] = asgrey.s[i].rgb[2];
						bestxyz[0] = asgrey.s[i].XYZ[0];
						bestxyz[1] = asgrey.s[i].XYZ[1];
						bestxyz[2] = asgrey.s[i].XYZ[2];

					} else if (asgrey.s[i].dc > bestdc) {
						/* we got worse in Lab and XYZ ! */

						wde = asgrey.s[i].de;

						/* If we've wandered too far, return to best we found */
						if (asgrey.s[i].hde > (3.0 * besthde)) {
							asgrey.s[i].de = bestde;
							asgrey.s[i].dc = bestdc;
							asgrey.s[i].peqde = bestpeqde;
							asgrey.s[i].hde = besthde;
							asgrey.s[i].rgb[0] = bestrgb[0];
							asgrey.s[i].rgb[1] = bestrgb[1];
							asgrey.s[i].rgb[2] = bestrgb[2];
							asgrey.s[i].XYZ[0] = bestxyz[0];
							asgrey.s[i].XYZ[1] = bestxyz[1];
							asgrey.s[i].XYZ[2] = bestxyz[2];
							icmSub3(asgrey.s[i].deXYZ, asgrey.s[i].tXYZ, asgrey.s[i].XYZ);
						}

						/* If the Jacobian hasn't changed, moderate the gain */
						if (impj == 0)
							rgain *= 0.8;		/* We might be overshooting */
						gworse = 1;
					}

					/* See if we need to repeat */
					if (asgrey.s[i].de <= errthr && asgrey.s[i].peqde < errthr) {	
						if (verb > 1)
							if (it < (mxits-1))
								printf("Point %d Delta E %f, OK\n",rsteps - i,asgrey.s[i].de);
							else
								printf("Point %d Delta E %f, peqDE %f, OK\n",rsteps - i,asgrey.s[i].de, asgrey.s[i].peqde);
						break;	/* No more retries */
					}
					if ((rpt+1) >= MAX_RPTS) {
						asgrey.s[i].de = bestde;			/* Restore to best we found */
						asgrey.s[i].dc = bestdc;
						asgrey.s[i].peqde = bestpeqde;		/* Restore to best we found */
						asgrey.s[i].hde = besthde;			/* Restore to best we found */
						asgrey.s[i].rgb[0] = bestrgb[0];
						asgrey.s[i].rgb[1] = bestrgb[1];
						asgrey.s[i].rgb[2] = bestrgb[2];
						asgrey.s[i].XYZ[0] = bestxyz[0];
						asgrey.s[i].XYZ[1] = bestxyz[1];
						asgrey.s[i].XYZ[2] = bestxyz[2];
						if (verb > 1)
							if (it < (mxits-1))
								printf("Point %d Delta E %f, Fail\n",rsteps - i,asgrey.s[i].de);
							else
								printf("Point %d Delta E %f, peqDE %f, Fail\n",rsteps - i,asgrey.s[i].de,asgrey.s[i].peqde);
						thrfail = 1;						/* Failed to meet target */
						if (bestde > failerr)
							failerr = bestde;				/* Worst failed delta E */
						break;	/* No more retries */
					}
					if (verb > 1) {
						if (gworse)
							if (it < (mxits-1))
								printf("Point %d Delta E %f, Repeat (got worse)\n", rsteps - i, wde);
							else
								printf("Point %d Delta E %f, peqDE %f, Repeat (got worse)\n", rsteps - i, wde,asgrey.s[i].peqde);
						else
							if (it < (mxits-1))
								printf("Point %d Delta E %f, Repeat\n", rsteps - i,asgrey.s[i].de);
							else
								printf("Point %d Delta E %f, peqDE %f, Repeat\n", rsteps - i,asgrey.s[i].de,asgrey.s[i].peqde);
					}
				
					/* Compute refinement of rgb */
					icmMulBy3x3(asgrey.s[i].pdrgb, asgrey.s[i].ij, asgrey.s[i].deXYZ);
//printf("~1 delta needed %f %f %f -> delta RGB %f %f %f\n",
//asgrey.s[i].deXYZ[0], asgrey.s[i].deXYZ[1], asgrey.s[i].deXYZ[2],
//asgrey.s[i].pdrgb[0], asgrey.s[i].pdrgb[1], asgrey.s[i].pdrgb[2]);

					/* Gain scale */
					icmScale3(asgrey.s[i].pdrgb, asgrey.s[i].pdrgb, rgain);
//printf("~1 delta RGB after gain scale %f %f %f\n",
//asgrey.s[i].pdrgb[0], asgrey.s[i].pdrgb[1], asgrey.s[i].pdrgb[2]);

#ifdef CLIP
					/* Component wise clip */
					for (j = 0; j < 3; j++) {		/* Check for clip */
						if ((-asgrey.s[i].pdrgb[j]) > asgrey.s[i].rgb[j]) {
							asgrey.s[i].pdrgb[j] = -asgrey.s[i].rgb[j];
							dclip = 1;
						}
						if (asgrey.s[i].pdrgb[j] > (1.0 - asgrey.s[i].rgb[j])) {
							asgrey.s[i].pdrgb[j] = (1.0 - asgrey.s[i].rgb[j]);
							dclip = 1;
						}
					}
#ifdef DEBUG
					if (dclip) printf("delta RGB after clip %f %f %f\n",
					       asgrey.s[i].pdrgb[0], asgrey.s[i].pdrgb[1], asgrey.s[i].pdrgb[2]);
#endif	/* DEBUG */
#endif	/* CLIP */
					/* Compute next on the basis of this one RGB */
					icmAdd3(asgrey.s[i].rgb, asgrey.s[i].rgb, asgrey.s[i].pdrgb);

					/* Save expected change in XYZ */
					icmMulBy3x3(asgrey.s[i].pdXYZ, asgrey.s[i].j, asgrey.s[i].pdrgb);
#ifdef DEBUG
					printf("New rgb %f %f %f from expected del XYZ %f %f %f\n",
					       asgrey.s[i].rgb[0], asgrey.s[i].rgb[1], asgrey.s[i].rgb[2],
					       asgrey.s[i].pdXYZ[0], asgrey.s[i].pdXYZ[1], asgrey.s[i].pdXYZ[2]);
#endif
				} else {	/* Verification, so no repeat */
					break;
				}

				prevde = asgrey.s[i].de;
			}	/* Next repeat */
		}		/* Next resolution step */
		if (verb)
			printf("\n");			/* Final return for patch count */

#ifdef DEBUG_PLOT
		/* Plot the measured response XYZ */
		{
			#define	XRES 256
			double xx[XRES];
			double yy[3][XRES];
			double xyz[3];
			for (i = 0; i < XRES; i++) {
				xx[i] = i/(XRES-1.0);
				csamp_interp(&asgrey, xyz, xx[i]);
				for (j = 0; j < 3; j++)
					yy[j][i] = xyz[j];
			}
			printf("Measured neutral axis XYZ\n",k);
			do_plot(xx,yy[0],yy[1],yy[2],XRES);
			#undef XRES
		}
#endif

		/* Check out the accuracy of the results: */
		{
			double ctwh[3];		/* Current target white */
			icmXYZNumber ctwN;	/* Same as above as XYZNumber */
			double brerr;		/* Brightness error */
			double cterr;		/* Color temperature delta E */
			double mnerr;		/* Maximum neutral error */
			double mnv = 0.0;	/* Value where maximum error is */
			double anerr;		/* Average neutral error */
			double lab1[3], lab2[3];
			
			/* Brightness */
			brerr = asgrey.s[asgrey.no-1].XYZ[1] - x.twh[1];
		
			/* Compensate for brightness error */
			for (j = 0; j < 3; j++)
				ctwh[j] = x.twh[j] * asgrey.s[asgrey.no-1].XYZ[1]/x.twh[1];
			icmAry2XYZ(ctwN, ctwh);		/* Need this for Lab conversions */
			
			/* Color temperature error */
			icmXYZ2Lab(&ctwN, lab1, ctwh);		/* Should be 100,0,0 */
			icmXYZ2Lab(&ctwN, lab2, asgrey.s[asgrey.no-1].XYZ);
			cterr = icmLabDE(lab1, lab2);

			/* check delta E of all the sample points */
			/* We're checking against our given brightness and */
			/* white point target. */
			mnerr = anerr = 0.0;
			init_csamp_txyz(&asgrey, &x, 0);		/* In case the targets were tweaked */
			for (i = 0; i < asgrey.no; i++) {
				double err;

				/* Re-compute de in case last pass had tweaked targets */
				asgrey.s[i].de = icmXYZLabDE(&x.twN, asgrey.s[i].tXYZ, asgrey.s[i].XYZ);
				err = asgrey.s[i].de;
//printf("RGB %.3f -> Lab %.2f %.2f %.2f, target %.2f %.2f %.2f, DE %f\n",
//asgrey.s[i].v, lab2[0], lab2[1], lab2[2], lab1[0], lab1[1], lab1[2], err);
				if (err > mnerr) {
					mnerr = err;
					mnv = asgrey.s[i].v;
				}
				anerr += err;
			}
			anerr /= (double)asgrey.no;

			if (verb || it >= mxits) {
				if (it >= mxits)
					printf("Verification results:\n");
				printf("Brightness error = %f cd/m^2 (is %f, should be %f)\n",brerr,asgrey.s[asgrey.no-1].XYZ[1],x.twh[1]);
				printf("White point error = %f deltaE\n",cterr);
				printf("Maximum neutral error (@ %f) = %f deltaE\n",mnv, mnerr);
				printf("Average neutral error = %f deltaE\n",anerr);
				if (it < mxits && thrfail)
					printf("Failed to meet target %f delta E, got worst case %f\n",errthr,failerr);
				printf("Number of measurements taken = %d\n",totmeas);
			}
		}

		/* Verify loop exit */
		if (it >= (mxits + nver -1)) {
			break;
		}

		/* Convert our test points into calibration curves. */
		/* The call to reinit_csamp() will then convert the */
		/* curves back to current test point values. */
		/* This applies some level of cohesion between the test points, */
		/* as well as forcing monotomicity */
		if (it < mxits) {
			mcvco *sdv[3];				/* Scattered data for mcv */

			for (j = 0; j < 3; j++) {
				if ((sdv[j] = malloc(sizeof(mcvco) * asgrey.no)) == NULL) {
					dr->del(dr);
					error ("Malloc of scattered data points failed");
				}
			}

			if (verb)
				printf("Computing update to calibration curves...\n");

			for (i = 0; i < asgrey.no; i++) {
				/* Use fixed rgb's */
				for (j = 0; j < 3; j++) {
					sdv[j][i].p = asgrey.s[i].v;
					sdv[j][i].v = asgrey.s[i].rgb[j];
					sdv[j][i].w = 1.0;
				}
			}
			if (x.nat)		/* Make curve go thought white if possible */
				sdv[0][rsteps-1].w = sdv[1][rsteps-1].w = sdv[2][rsteps-1].w = 10.0;

			for (j = 0; j < 3; j++)
				x.rdac[j]->fit(x.rdac[j], 0, fitord, sdv[j], asgrey.no, RDAC_SMOOTH);

			/* Make sure that if we are using native brightness and white point, */
			/* that the curves go to a perfect 1.0 ... */
			if (x.nat) {
				for (j = 0; j < 3; j++)
					x.rdac[j]->force_1(x.rdac[j], 1.0);
			}

			for (j = 0; j < 3; j++)
				free(sdv[j]);
#ifdef DEBUG_PLOT
			/* Plot the current curves */
			{
				#define	XRES 255
				double xx[XRES];
				double y1[XRES];
				double y2[XRES];
				double y3[XRES];
				double rgb[3];
				for (i = 0; i < XRES; i++) {
					double drgb[3], rgb[3];
					xx[i] = i/(XRES-1.0);
					rgb[0] = rgb[1] = rgb[2] = xx[i];
					for (j = 0; j < 3; j++)
						drgb[j] = x.rdac[j]->interp(x.rdac[j], rgb[j]);
					y1[i] = drgb[0];
					y2[i] = drgb[1];
					y3[i] = drgb[2];
				}
				printf("Current ramdac curves\n");
				do_plot(xx,y1,y2,y3,XRES);
				#undef XRES
			}
#endif
		}
	}	/* Next refine/verify loop */

	free_alloc_csamp(&asgrey);		/* We're done with test points */
	dr->del(dr);	/* Now we're done with test window */

	/* Write out the resulting calibration file */
	if (verify != 2) {
		int calres = CAL_RES;			/* steps in calibration table saved */
		cgats *ocg;						/* output cgats structure */
		time_t clk = time(0);
		struct tm *tsp = localtime(&clk);
		char *atm = asctime(tsp);		/* Ascii time */
		cgats_set_elem *setel;			/* Array of set value elements */
		int ncps;						/* Number of curve parameters */
		double *cps[3];					/* Arrays of curve parameters */
		char *bp = NULL, buf[100];		/* Buffer to sprintf into */

		ocg = new_cgats();				/* Create a CGATS structure */
		ocg->add_other(ocg, "CAL"); 	/* our special type is Calibration file */

		ocg->add_table(ocg, tt_other, 0);	/* Add a table for RAMDAC values */
		ocg->add_kword(ocg, 0, "DESCRIPTOR", "Argyll Device Calibration Curves",NULL);
		ocg->add_kword(ocg, 0, "ORIGINATOR", "Argyll dispcal", NULL);
		atm[strlen(atm)-1] = '\000';	/* Remove \n from end */
		ocg->add_kword(ocg, 0, "CREATED",atm, NULL);

		ocg->add_kword(ocg, 0, "DEVICE_CLASS","DISPLAY", NULL);
		ocg->add_kword(ocg, 0, "COLOR_REP","RGB", NULL);

		/* Put the target parameters in the CGATS file too */
		if (dtype != 0)
			ocg->add_kword(ocg, 0, "DEVICE_TYPE", dtype == 1 ? "CRT" : "LCD", NULL);
		
		if (wpx == 0.0 && wpy == 0.0 && temp == 0.0 && tbright == 0.0)
			ocg->add_kword(ocg, 0, "NATIVE_TARGET_WHITE","", NULL);

		sprintf(buf,"%f %f %f", x.twh[0], x.twh[1], x.twh[2]);
		ocg->add_kword(ocg, 0, "TARGET_WHITE_XYZ",buf, NULL);

		switch(x.gammat) {
			case gt_power:
				if (egamma > 0.0)
					sprintf(buf,"%f", -egamma);
				else
					sprintf(buf,"%f", gamma);
				break;
			case gt_Lab:
				strcpy(buf,"L_STAR");
				break;
			case gt_sRGB:
				strcpy(buf,"sRGB");
				break;
			case gt_Rec709:
				strcpy(buf,"REC709");
				break;
			case gt_SMPTE240M:
				strcpy(buf,"SMPTE240M");
				break;
			default:
				error("Unknown gamma type");
		}
		ocg->add_kword(ocg, 0, "TARGET_GAMMA",buf, NULL);

		sprintf(buf,"%f", x.oofff);
		ocg->add_kword(ocg, 0, "DEGREE_OF_BLACK_OUTPUT_OFFSET",buf, NULL);
		
		sprintf(buf,"%f", bkcorrect);
		ocg->add_kword(ocg, 0, "BLACK_POINT_CORRECTION", buf, NULL);

		sprintf(buf,"%f", x.nbrate);
		ocg->add_kword(ocg, 0, "BLACK_NEUTRAL_BLEND_RATE", buf, NULL);

		if (bkbright > 0.0) {
			sprintf(buf,"%f", bkbright);
			ocg->add_kword(ocg, 0, "TARGET_BLACK_BRIGHTNESS",buf, NULL);
		}

		/* Write rest of setup */
	    switch (quality) {
			case -3:				/* Test value */
				bp = "ultra low";
				break;
			case -2:				/* Very low */
				bp = "very low";
				break;
			case -1:				/* Low */
				bp = "low";
				break;
			case 0:					/* Medum */
				bp = "medium";
				break;
			case 1:					/* High */
				bp = "high";
				break;
			case 2:					/* Ultra */
				bp = "ultra high";
				break;
			default:
				error("unknown quality level %d",quality);
		}
		ocg->add_kword(ocg, 0, "QUALITY",bp, NULL);

		ocg->add_field(ocg, 0, "RGB_I", r_t);
		ocg->add_field(ocg, 0, "RGB_R", r_t);
		ocg->add_field(ocg, 0, "RGB_G", r_t);
		ocg->add_field(ocg, 0, "RGB_B", r_t);

		if ((setel = (cgats_set_elem *)malloc(sizeof(cgats_set_elem) * 4)) == NULL)
			error("Malloc failed!");

		/* Write the video lut curve values */
		for (i = 0; i < calres; i++) {
			double vv, rgb[3];

#if defined(__APPLE__) && defined(__POWERPC__)
			gcc_bug_fix(i);
#endif
			vv = i/(calres-1.0);
			for (j = 0; j < 3; j++) {
				double cc;
				cc = x.rdac[j]->interp(x.rdac[j], vv);
				if (cc < 0.0)
					cc = 0.0;
				else if (cc > 1.0)
					cc = 1.0;
				rgb[j] = cc;
			}

			setel[0].d = vv;
			setel[1].d = rgb[0];
			setel[2].d = rgb[1];
			setel[3].d = rgb[2];

			ocg->add_setarr(ocg, 0, setel);
		}

		free(setel);

		/* Write some of the device model information to a second */
		/* table, so that we can update the calibration latter on without */
		/* having to read R,G & B curves. */

		ocg->add_table(ocg, tt_other, 0);	/* Add a second table for setup and model */
		ocg->add_kword(ocg, 1, "DESCRIPTOR", "Argyll Calibration options and model",NULL);
		ocg->add_kword(ocg, 1, "ORIGINATOR", "Argyll dispcal", NULL);
		atm[strlen(atm)-1] = '\000';	/* Remove \n from end */
		ocg->add_kword(ocg, 1, "CREATED",atm, NULL);


		/* Write device model curves */
		ocg->add_field(ocg, 1, "R_P", r_t);
		ocg->add_field(ocg, 1, "G_P", r_t);
		ocg->add_field(ocg, 1, "B_P", r_t);

		if ((setel = (cgats_set_elem *)malloc(sizeof(cgats_set_elem) * 3)) == NULL)
			error("Malloc failed!");

		ncps = -1;
		for (i = 0; i < 3; i++) {
			int nn;
			nn = x.dcvs[i]->get_params(x.dcvs[i], &cps[i]);
			if (ncps != -1 && ncps != nn)
				error("Expect device model linearisation curves to have the same order");
			ncps = nn;
		}

		for (i = 0; i < ncps; i++) {
			setel[0].d = cps[0][i];
			setel[1].d = cps[1][i];
			setel[2].d = cps[2][i];
			ocg->add_setarr(ocg, 1, setel);
		}

		for (i = 0; i < 3; i++)
			free(cps[i]);
		free(setel);

		if (ocg->write_name(ocg, outname))
			error("Write error : %s",ocg->err);

		if (verb)
			printf("Written calibration file '%s'\n",outname);

		ocg->del(ocg);		/* Clean up */

	}

	/* Update the ICC file with the new LUT curves */
	if (verify != 2 && doupdate && doprofile) {
		icmFile *ic_fp;
		icc *icco;
		int j, i;
		icmVideoCardGamma *wo;

		if ((icco = new_icc()) == NULL)
			error ("Creation of ICC object to read profile '%s' failed",iccoutname);

		/* Open up the profile for reading */
		if ((ic_fp = new_icmFileStd_name(iccoutname,"r")) == NULL)
			error ("Can't open file '%s'",iccoutname);

		/* Read header etc. */
		if ((rv = icco->read(icco,ic_fp,0)) != 0)
			error ("Reading profile '%s' failed with %d, %s",iccoutname, rv,icco->err);

		/* Read every tag */
		if (icco->read_all_tags(icco) != 0) {
			error("Unable to read all tags from '%s': %d, %s",iccoutname, icco->errc,icco->err);
		}

		ic_fp->del(ic_fp);

		wo = (icmVideoCardGamma *)icco->read_tag(icco, icSigVideoCardGammaTag);
		if (wo == NULL)
			error("Can't find VideoCardGamma tag in file '%s': %d, %s",
			      iccoutname, icco->errc,icco->err);

		wo->tagType = icmVideoCardGammaTableType;
		wo->u.table.channels = 3;			/* rgb */
		wo->u.table.entryCount = CAL_RES;	/* full lut */
		wo->u.table.entrySize = 2;			/* 16 bits */
		wo->allocate((icmBase*)wo);
		for (j = 0; j < 3; j++) {
			for (i = 0; i < CAL_RES; i++) {
				double cc, vv = i/(CAL_RES-1.0);
				cc = x.rdac[j]->interp(x.rdac[j], vv);
				if (cc < 0.0)
					cc = 0.0;
				else if (cc > 1.0)
					cc = 1.0;
				((unsigned short*)wo->u.table.data)[CAL_RES * j + i] = (int)(cc * 65535.0 + 0.5);
			}
		}

		/* Open up the profile again writing */
		if ((ic_fp = new_icmFileStd_name(iccoutname,"w")) == NULL)
			error ("Can't open file '%s' for writing",iccoutname);

		if ((rv = icco->write(icco,ic_fp,0)) != 0)
			error ("Write to file '%s' failed: %d, %s",iccoutname, rv,icco->err);

		if (verb)
			printf("Updated profile '%s'\n",iccoutname);

		ic_fp->del(ic_fp);
		icco->del(icco);

	/* Create a fast matrix/shaper profile */
	/*
	     [ Another way of doing this would be to run all the
	     measured points through the calibration curves, and
	     then re-fit the curve/matrix to the calibrated points.
	     This might be more accurate ?]
	 */

	} else if (verify != 2 && doprofile) {
		icmFile *wr_fp;
		icc *wr_icco;
		double uwp[3];		/* Absolute Uncalibrated White point in XYZ */
		double wp[3];		/* Absolute White point in XYZ */
		double bp[3];		/* Absolute Black point in XYZ */
		double mat[3][3];	/* Device to XYZ matrix */

		/* Lookup white and black points */
		{
			int j;
			double rgb[3];

			rgb[0] = rgb[1] = rgb[2] = 1.0;

			fwddev(&x, uwp, rgb);

			/* Through calibration */
			for (j = 0; j < 3; j++) {
				rgb[j] = x.rdac[j]->interp(x.rdac[j], rgb[j]);
				if (rgb[j] < 0.0)
					rgb[j] = 0.0;
				else if (rgb[j] > 1.0)
					rgb[j] = 1.0;
			}
			fwddev(&x, wp, rgb);

			rgb[0] = rgb[1] = rgb[2] = 0.0;

			/* Through calibration */
			for (j = 0; j < 3; j++) {
				rgb[j] = x.rdac[j]->interp(x.rdac[j], rgb[j]);
				if (rgb[j] < 0.0)
					rgb[j] = 0.0;
				else if (rgb[j] > 1.0)
					rgb[j] = 1.0;
			}
			fwddev(&x, bp, rgb);
		}

		/* Fix matrix to be relative to D50 white point, rather than absolute */
		{
			icmXYZNumber swp;
			icmAry2XYZ(swp, wp);

			/* Transfer from parameter to matrix */
			icmCpy3x3(mat, x.fm);

			/* Adapt matrix */
			icmChromAdaptMatrix(ICM_CAM_MULMATRIX | ICM_CAM_BRADFORD, icmD50, swp, mat);
		}
		
		/* Open up the file for writing */
		if ((wr_fp = new_icmFileStd_name(iccoutname,"w")) == NULL)
			error("Write: Can't open file '%s'",iccoutname);

		if ((wr_icco = new_icc()) == NULL)
			error("Write: Creation of ICC object failed");

		/* Add all the tags required */

		/* The header: */
		{
			icmHeader *wh = wr_icco->header;

			/* Values that must be set before writing */
			wh->deviceClass     = icSigDisplayClass;
			wh->colorSpace      = icSigRgbData;				/* Display is RGB */
			wh->pcs         = icSigXYZData;					/* XYZ for matrix based profile */
			wh->renderingIntent = icRelativeColorimetric;	/* For want of something */

			wh->manufacturer = str2tag("????");
	    	wh->model        = str2tag("????");
#ifdef NT
			wh->platform = icSigMicrosoft;
#endif
#ifdef __APPLE__
			wh->platform = icSigMacintosh;
#endif
#if defined(UNIX) && !defined(__APPLE__)
			wh->platform = icmSig_nix;
#endif
		}

		/* Profile Description Tag: */
		{
			icmTextDescription *wo;
			char *dst, dstm[200];			/* description */

			if (profDesc != NULL)
				dst = profDesc;
			else {
				dst = iccoutname;
			}

			if ((wo = (icmTextDescription *)wr_icco->add_tag(
			           wr_icco, icSigProfileDescriptionTag,	icSigTextDescriptionType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->errc,wr_icco->err);

			wo->size = strlen(dst)+1; 	/* Allocated and used size of desc, inc null */
			wo->allocate((icmBase *)wo);/* Allocate space */
			strcpy(wo->desc, dst);		/* Copy the string in */
		}
		/* Copyright Tag: */
		{
			icmText *wo;
			char *crt;

			if (copyright != NULL)
				crt = copyright;
			else
				crt = "Copyright, the creator of this profile";

			if ((wo = (icmText *)wr_icco->add_tag(
			           wr_icco, icSigCopyrightTag,	icSigTextType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->errc,wr_icco->err);

			wo->size = strlen(crt)+1; 	/* Allocated and used size of text, inc null */
			wo->allocate((icmBase *)wo);/* Allocate space */
			strcpy(wo->data, crt);		/* Copy the text in */
		}
		/* Device Manufacturers Description Tag: */
		if (deviceMfgDesc != NULL) {
			icmTextDescription *wo;
			char *dst = deviceMfgDesc;

			if ((wo = (icmTextDescription *)wr_icco->add_tag(
			           wr_icco, icSigDeviceMfgDescTag,	icSigTextDescriptionType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->errc,wr_icco->err);

			wo->size = strlen(dst)+1; 	/* Allocated and used size of desc, inc null */
			wo->allocate((icmBase *)wo);/* Allocate space */
			strcpy(wo->desc, dst);		/* Copy the string in */
		}
		/* Model Description Tag: */
		if (modelDesc != NULL) {
			icmTextDescription *wo;
			char *dst = modelDesc;

			if ((wo = (icmTextDescription *)wr_icco->add_tag(
			           wr_icco, icSigDeviceModelDescTag,	icSigTextDescriptionType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->errc,wr_icco->err);

			wo->size = strlen(dst)+1; 	/* Allocated and used size of desc, inc null */
			wo->allocate((icmBase *)wo);/* Allocate space */
			strcpy(wo->desc, dst);		/* Copy the string in */
		}
		/* Luminance tag */
		{
			icmXYZArray *wo;;
	
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigLuminanceTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->errc,wr_icco->err);

			wo->size = 1;
			wo->allocate((icmBase *)wo);	/* Allocate space */
			wo->data[0].X = 0.0;
			wo->data[0].Y = wp[1] * dispLum/uwp[1];	/* Compensate for model error */
			wo->data[0].Z = 0.0;

			if (verb)
				printf("Luminance XYZ = %f %f %f\n", wo->data[0].X, wo->data[0].Y, wo->data[0].Z);
		}
		/* White Point Tag: */
		{
			icmXYZArray *wo;
			/* Note that tag types icSigXYZType and icSigXYZArrayType are identical */
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaWhitePointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->errc,wr_icco->err);

			wo->size = 1;
			wo->allocate((icmBase *)wo);	/* Allocate space */
			wo->data[0].X = wp[0] * 1.0/wp[1];
			wo->data[0].Y = wp[1] * 1.0/wp[1];
			wo->data[0].Z = wp[2] * 1.0/wp[1];

			if (verb)
				printf("White point XYZ = %f %f %f\n", wo->data[0].X, wo->data[0].Y, wo->data[0].Z);
		}
		/* Black Point Tag: */
		{
			icmXYZArray *wo;
			if ((wo = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigMediaBlackPointTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",wr_icco->errc,wr_icco->err);

			wo->size = 1;
			wo->allocate((icmBase *)wo);	/* Allocate space */
			wo->data[0].X = bp[0] * 1.0/wp[1];
			wo->data[0].Y = bp[1] * 1.0/wp[1];
			wo->data[0].Z = bp[2] * 1.0/wp[1];

			if (verb)
				printf("Black point XYZ = %f %f %f\n", wo->data[0].X, wo->data[0].Y, wo->data[0].Z);
		}

		/* vcgt tag */
		{
			int j, i;
			icmVideoCardGamma *wo;
			wo = (icmVideoCardGamma *)wr_icco->add_tag(wr_icco,
			                           icSigVideoCardGammaTag, icSigVideoCardGammaType);
			if (wo == NULL)
				error("add_tag failed: %d, %s",wr_icco->errc,wr_icco->err);

			wo->tagType = icmVideoCardGammaTableType;
			wo->u.table.channels = 3;				/* rgb */
			wo->u.table.entryCount = CAL_RES;		/* full lut */
			wo->u.table.entrySize = 2;				/* 16 bits */
			wo->allocate((icmBase*)wo);
			for (j = 0; j < 3; j++) {
				for (i = 0; i < CAL_RES; i++) {
					double cc, vv = i/(CAL_RES-1.0);
					cc = x.rdac[j]->interp(x.rdac[j], vv);
					if (cc < 0.0)
						cc = 0.0;
					else if (cc > 1.0)
						cc = 1.0;
					((unsigned short*)wo->u.table.data)[CAL_RES * j + i] = (int)(cc * 65535.0 + 0.5);
				}
			}
		}

		/* Red, Green and Blue Colorant Tags: */
		{
			icmXYZArray *wor, *wog, *wob;
			if ((wor = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigRedColorantTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->err);
			if ((wog = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigGreenColorantTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->err);
			if ((wob = (icmXYZArray *)wr_icco->add_tag(
			           wr_icco, icSigBlueColorantTag, icSigXYZArrayType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->err);

			wor->size = wog->size = wob->size = 1;
			wor->allocate((icmBase *)wor);	/* Allocate space */
			wog->allocate((icmBase *)wog);
			wob->allocate((icmBase *)wob);

			wor->data[0].X = mat[0][0]; wor->data[0].Y = mat[1][0]; wor->data[0].Z = mat[2][0];
			wog->data[0].X = mat[0][1]; wog->data[0].Y = mat[1][1]; wog->data[0].Z = mat[2][1];
			wob->data[0].X = mat[0][2]; wob->data[0].Y = mat[1][2]; wob->data[0].Z = mat[2][2];
		}

		/* Red, Green and Blue Tone Reproduction Curve Tags: */
		{
			icmCurve *wor, *wog, *wob;
			int ui;

			if ((wor = (icmCurve *)wr_icco->add_tag(
			           wr_icco, icSigRedTRCTag, icSigCurveType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->err);
			if ((wog = (icmCurve *)wr_icco->add_tag(
			           wr_icco, icSigGreenTRCTag, icSigCurveType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->err);
			if ((wob = (icmCurve *)wr_icco->add_tag(
			           wr_icco, icSigBlueTRCTag, icSigCurveType)) == NULL) 
				error("add_tag failed: %d, %s",rv,wr_icco->err);

			wor->flag = wog->flag = wob->flag = icmCurveSpec; 
			wor->size = wog->size = wob->size = 256;			/* Number of entries */
			wor->allocate((icmBase *)wor);	/* Allocate space */
			wog->allocate((icmBase *)wog);
			wob->allocate((icmBase *)wob);
	
			for (ui = 0; ui < wor->size; ui++) {
				double in, rgb[3];
	
				for (j = 0; j < 3; j++) {
#if defined(__APPLE__) && defined(__POWERPC__)
					gcc_bug_fix(ui);
#endif
					in = (double)ui / (wor->size - 1.0);
		
					/* Lookup RAMDAC value */
					in = x.rdac[j]->interp(x.rdac[j], in);

					if (in < 0.0)
						in = 0.0;
					else if (in > 1.0)
						in = 1.0;

					/* Lookup device model */
					in = x.dcvs[j]->interp(x.dcvs[j], in);
					if (in < 0.0)
						in = 0.0;
					else if (in > 1.0)
						in = 1.0;
					rgb[j] = in;
				}
				wor->data[ui] = rgb[0];	/* Curve values 0.0 - 1.0 */
				wog->data[ui] = rgb[1];
				wob->data[ui] = rgb[2];
			}
		}

		/* Write the file (including all tags) out */
		if ((rv = wr_icco->write(wr_icco,wr_fp,0)) != 0) {
			error("Write file: %d, %s",rv,wr_icco->err);
		}

		if (verb)
			printf("Created fast shaper/matrix profile '%s'\n",iccoutname);

		/* Close the file */
		wr_icco->del(wr_icco);
		wr_fp->del(wr_fp);
	}

	if (verify != 2) {
		for (j = 0; j < 3; j++)
			x.rdac[j]->del(x.rdac[j]);
	
		for (k = 0; k < 3; k++)
			x.dcvs[k]->del(x.dcvs[k]);
	}

	if (x.svc != NULL) {
		x.svc->del(x.svc);
		x.svc = NULL;
	}
	if (x.dvc != NULL) {
		x.dvc->del(x.svc);
		x.dvc = NULL;
	}

	free_a_disppath(disp);

	return 0;
}


