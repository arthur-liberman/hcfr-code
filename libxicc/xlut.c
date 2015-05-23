
/*
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    2/7/00
 * Version: 1.00
 *
 * Copyright 2000, 2001, 2014 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * This is the third major version of xlut.c (originally called xlut2.c)
 * Based on the old xlut.c code (preserved as xlut1.c)
 * This version uses xfit.c to do the curve and rspl fitting.
 */

/*
 * This module provides the expanded icclib functionality
 * for lut based profiles.
 * This file is #included in xicc.c, to keep its functions private.
 *
 * This version creates both input and output 1D luts by
 * optimising the accuracy of the profile for a linear clut.
 *
 */

/*
	TTBD:

		See gamut/gammap.c for notes on the desirability of
		a non-minimum delta E colorimetric intent as the default.

		Should fix gamut's so that they have enough information
		to spefify a gamut mapping without the need for
		a source colorspace profile, and fix the code
		to work with just a gamut. This would take us further
		towards supporting the PRMG reference gamut interoperability
		as an option.

		Should the input profile white point determination
		be made a bit smarter about determining the chromaticity ?
		ie. not take on the tint of the whitest patch, but an
		average of neutral patches ?

		Should xlutfix.c be revived (also adding ICM_CLUT_SET_APXLS support), 
		to improve "bumpy black" problem ?
 */

/*

	NOTE :- an alternative to the way display profile absolute is handled here
	would be to always chromatically adapt the illuminant to D50, and encode
	that in the Chromatic adapation tag. To make absolute colorimetric
	do anything useful though, the chromatic adapation tag would
    have to be used for absolute intent.
	This may be the way of improving compatibility with other systems,
	and is needed for V4, but would break compatibility with existing
	Argyll profiles, unless special measures are taken:

	ie. 

		1) if (display profile & using chromatic adaptation tag)
			Create Bradford chromatic adapation matrix and store it in tag
			Adapt all the readings using Bradford
			Create white point and store it in tag (white point will be D50)
			Adapt all the readings to the white point using wrong Von-Kries (== NOOP)	
			Store relative colorimetric cLUT 
			Set version >= 2.4

		else
			2) if (display scheme A or using Argyll historical printer scheme) 
				Create white point and store it in tag
				Adapt all the readings to the white point using Bradford
				Store relative colorimetric tag
				Set version < 2.4 for V2 profile
				Add private Absolute Transform Matrix (labels V4 profile)

			3) else (display scheme B or strict ICC printer compatibility)
				Create white point and store it in tag
				Adapt all the readings to the white point using Wrong Von-Kries
				Store relative colorimetric tag
				Set version >= 2.4


	Argyll Processing for each type

		1) if display and chromatic adapation matrix
			Un-adapt matrix or cLUT using wrong Von-Kries from white point
			Un-adapt matrix or cLUT using chromatic matrix
			Un-adapt apparant white point & black point using chromatic transform

			if (not absolute intent)
				Create Bradford transfor from white to PCS D50
				Adapt all matrix or cLUT
		else
			2) if (display scheme A or using Argyll < V2.4. profile
			       or find Absolute Transform Matrix) 
				if (absolute intent)
					Un-adapt matrix or cLUT using Bradford from white point
			
			3) else (display scheme B or !Argyll profile or ( >= V2.4 profile
			       and !Absolute Transform Matrix)) 
				Un-adapt matrix or cLUT using wrong Von-Kries from white point
	
				if (not absolute intent)
					Create Bradford transfor from white to PCS D50
					Adapt all matrix or cLUT to white


	The problem with this is that it wouldn't do the right thing on old Argyll
	type profiles that weren't labeled or recognized.

	Is there a way of recognizing Bradford Absolute transform Matricies if
	the color chromaticities are given ?

 */

/* 
	A similar condrum is that it seems that an unwritten convention for
 	V2 profiles is to scale the black point of the perceptual and
	saturation tables to 0 (Part of the V4 spec is to scale to Y = 3.1373).

	To get better gamut mapping we should therefore unscale the perceptual
	and saturation A2B table to have the same black point as the colorimetric
	table before computing the gamut mapping, and then apply the opposite
	transform to our perceptual B2A and A2B tables.

 */

/*
	There is interesting behaviour for Jab 0,0,0 in, in that
	it gets mapped to (something like) Lab -1899.019855 -213.574625 -231.914285
	which after per-component clipping of the inv out tables looks like
	0, -128, -128, which may be mapped to (say) RGB 0.085455 1.000000 0.936951,
	which is not black.

 */

#include "xfit.h"

#undef USE_CIE94_DE				/* [Undef] Use CIE94 delta E measure when creating in/out curves */
								/* Don't use CIE94 because it makes peak error worse ? */

#undef DEBUG 					/* [Undef] Verbose debug information */
#undef CNDTRACE					/* [Undef] Enable xicc->trace conditional verbosity */
#undef DEBUG_OLUT 				/* [Undef] Print steps in overall fwd & rev lookups */
#undef DEBUG_RLUT 				/* [Undef] Print values being reverse lookup up */
#undef DEBUG_SPEC 				/* [Undef] Debug some specific cases */
#undef INK_LIMIT_TEST			/* [Undef] Turn off input tables for ink limit testing */
#undef CHECK_ILIMIT				/* [Undef] Do sanity checks on meeting ink limit */
#undef WARN_CLUT_CLIPPING		/* [Undef] Print warning if setting clut clips */
#undef DISABLE_KCURVE_FILTER	/* [Undef] don't filter the Kcurve */
#undef REPORT_LOCUS_SEGMENTS    /* [Undef[ Examine how many segments there are in aux inversion */

#define XYZ_EXTRA_SMOOTH 20.0		/* Extra smoothing factor for XYZ profiles */
									/* !!! Note this is mainly due to smoothing being */
									/* scaled by data range in rspl code !!! */
#define SHP_SMOOTH 1.0	/* Input shaper curve smoothing */
#define OUT_SMOOTH1 1.0	/* Output shaper curve smoothing for L*, X,Y,Z */
#define OUT_SMOOTH2 1.0	/* Output shaper curve smoothing for a*, b* */

#define CAMCLIPTRANS 1.0		/* [1.0] Cam clipping transition region Delta E */
								/* Should this be smaller ? */
#undef USECAMCLIPSPLINE			/* [Und] use spline blend between PCS and Jab */

#define CCJSCALE	2.0			/* [2.0] Amount to emphasize J delta E in in computing clip */

/*
 * TTBD:
 *
 *       Reverse lookup of Lab
 *       Make NEARCLIP the default ??
 *
 *       XYZ vector clipping isn't implemented.
 *
 *       Some of the error handling is crude. Shouldn't use
 *       error(), should return status.
 */

static double icxLimitD(icxLuLut *p, double *in);		/* For input' */
#define icxLimitD_void ((double (*)(void *, double *))icxLimitD)	/* Cast with void 1st arg */
static double icxLimit(icxLuLut *p, double *in);		/* For input */
static int icxLuLut_init_clut_camclip(icxLuLut *p);

/* Debug overall lookup */
#ifdef DEBUG_OLUT
#undef DBOL
#ifdef CNDTRACE
#define DBOL(xxx) if (p->trace) printf xxx ;
#else
#define DBOL(xxx) printf xxx ;
#endif
#else
#undef DBOL
#define DBOL(xxx) 
#endif

/* Debug reverse lookup */
#ifdef DEBUG_RLUT
#undef DBR
#ifdef CNDTRACE
#define DBR(xxx) if (p->trace) printf xxx ;
#else
#define DBR(xxx) printf xxx ;
#endif
#else
#undef DBR
#define DBR(xxx) 
#endif

/* Debug some specific cases (fwd_relpcs_outpcs, bwd_outpcs_relpcs) */
#ifdef DEBUG_SPEC
# undef DBS
# ifdef CNDTRACE
#  define DBS(xxx) if (p->trace) printf xxx ;
# else
#  define DBS(xxx) printf xxx ;
# endif
#else
# undef DBS
# define DBS(xxx) 
#endif

/* ========================================================== */
/* xicc lookup code.                                          */
/* ========================================================== */

/* Forward and Backward Multi-Dimensional Interpolation type conversion */
/* Return 0 on success, 1 if clipping occured, 2 on other error */

/* Components of overall lookup, in order */

int icxLuLut_in_abs(icxLuLut *p, double *out, double *in) {
	int rv = 0;

	if (p->ins == icxSigJabData) {
		DBOL(("xlut in_abs: CAM in = %s\n", icmPdv(p->inputChan, in)));
		p->cam->cam_to_XYZ(p->cam, out, in);
		DBOL(("xlut in_abs: XYZ = %s\n", icmPdv(p->inputChan, out)));
		/* Hack to prevent CAM02 weirdness being amplified by inv_abs() */
		/* or any later per channel clipping. */
		/* Limit -Y to non-stupid values by scaling */
		if (out[1] < -0.1) {
			out[0] *= -0.1/out[1];
			out[2] *= -0.1/out[1];
			out[1] = -0.1;
			DBOL(("xlut in_abs: after clipping -Y %s\n",icmPdv(p->outputChan, out)));
		}
		rv |= ((icmLuLut *)p->plu)->in_abs((icmLuLut *)p->plu, out, out);
		DBOL(("xlut in_abs: XYZ out = %s\n", icmPdv(p->inputChan, out)));
	} else {
		DBOL(("xlut in_abs: PCS in = %s\n", icmPdv(p->inputChan, in)));
		rv |= ((icmLuLut *)p->plu)->in_abs((icmLuLut *)p->plu, out, in);
		DBOL(("xlut in_abs: PCS out = %s\n", icmPdv(p->inputChan, out)));
	}

	return rv;
}

/* Possible matrix lookup */
/* input->input (not distinguishing matrix altered input values) */
int icxLuLut_matrix(icxLuLut *p, double *out, double *in) {
	int rv = 0;
	rv |= ((icmLuLut *)p->plu)->matrix((icmLuLut *)p->plu, out, in);
	return rv;
}

/* Do input -> input' lookup */
int icxLuLut_input(icxLuLut *p, double *out, double *in) {
#ifdef NEVER
	return ((icmLuLut *)p->plu)->input((icmLuLut *)p->plu, out, in);
#else
	int rv = 0;
	co tc;
	int i;
	for (i = 0; i < p->inputChan; i++) {
		tc.p[0] = in[i];
		rv |= p->inputTable[i]->interp(p->inputTable[i], &tc);
		out[i] = tc.v[0];
	}
	return rv;
#endif
}

/* Do input'->output' lookup, with aux' return */
/* (The aux' is just extracted from the in' values) */
int icxLuLut_clut_aux(icxLuLut *p,
double *out,	/* output' value */
double *oink,	/* If not NULL, return amount input is over the ink limit, 0 if not */
double *auxv,	/* If not NULL, return aux value used (packed) */
double *in		/* input' value */
) {
	int rv = 0;
	co tc;
	int i;

	for (i = 0; i < p->inputChan; i++)
		tc.p[i] = in[i];
	rv |= p->clutTable->interp(p->clutTable, &tc);
	for (i = 0; i < p->outputChan; i++)
		out[i] = tc.v[i];

	if (auxv != NULL) {
		int ee = 0;
		for (i = 0; i < p->clutTable->di; i++) {
			double v = in[i];
			if (p->auxm[i] != 0) {
				auxv[ee] = v;
				ee++;
			}
		}
	}

	if (oink != NULL) {
		double lim = 0.0;

		if (p->ink.tlimit >= 0.0 || p->ink.klimit >= 0.0) {
			lim = icxLimitD(p, in);
			if (lim < 0.0)
				lim = 0.0;
		}
		*oink = lim;
	}

	return rv;
}

/* Do input'->output' lookup */
int icxLuLut_clut(icxLuLut *p, double *out, double *in) {
#ifdef NEVER
	return ((icmLuLut *)p->plu)->clut((icmLuLut *)p->plu, out, in);
#else
	return icxLuLut_clut_aux(p, out, NULL, NULL, in);
#endif
}

/* Do output'->output lookup */
int icxLuLut_output(icxLuLut *p, double *out, double *in) {
	int rv = 0;

	if (p->mergeclut == 0) {
#ifdef NEVER
		rv = ((icmLuLut *)p->plu)->output((icmLuLut *)p->plu, out, in);
#else
		co tc;
		int i;
		for (i = 0; i < p->outputChan; i++) {
			tc.p[0] = in[i];
			rv |= p->outputTable[i]->interp(p->outputTable[i], &tc);
			out[i] = tc.v[0];
		}
#endif
	} else {
		int i;
		for (i = 0; i < p->outputChan; i++)
			out[i] = in[i];
	}
	return rv;
}

/* Relative to absolute conversion + PCS to PCS override (Effective PCS) conversion */
int icxLuLut_out_abs(icxLuLut *p, double *out, double *in) {
	int rv = 0;
	if (p->mergeclut == 0) {
		DBOL(("xlut out_abs: PCS in = %s\n", icmPdv(p->outputChan, in)));

		rv |= ((icmLuLut *)p->plu)->out_abs((icmLuLut *)p->plu, out, in);

		DBOL(("xlut out_abs: ABS PCS out = %s\n", icmPdv(p->outputChan, out)));

		if (p->outs == icxSigJabData) {
			p->cam->XYZ_to_cam(p->cam, out, out);

			DBOL(("xlut out_abs: CAM out = %s\n", icmPdv(p->outputChan, out)));
		}
	} else {
		int i;
		for (i = 0; i < p->outputChan; i++)
			out[i] = in[i];
	}

	return rv;
}

/* Overall lookup */
static int
icxLuLut_lookup (
icxLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	icxLuLut *p = (icxLuLut *)pp;
	int rv = 0;
	double temp[MAX_CHAN];

	DBOL(("xicclu: in = %s\n", icmPdv(p->inputChan, in)));
	rv |= p->in_abs  (p, temp, in);
	DBOL(("xicclu: after abs = %s\n", icmPdv(p->inputChan, temp)));
	rv |= p->matrix  (p, temp, temp);
	DBOL(("xicclu: after matrix = %s\n", icmPdv(p->inputChan, temp)));
	rv |= p->input   (p, temp, temp);
	DBOL(("xicclu: after inout = %s\n", icmPdv(p->inputChan, temp)));
	rv |= p->clut    (p, out,  temp);
	DBOL(("xicclu: after clut = %s\n", icmPdv(p->outputChan, out)));
	if (p->mergeclut == 0) {
		rv |= p->output  (p, out,  out);
		DBOL(("xicclu: after output = %s\n", icmPdv(p->outputChan, out)));
		rv |= p->out_abs (p, out,  out);
		DBOL(("xicclu: after outabs = %s\n", icmPdv(p->outputChan, out)));
	}
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given a relative XYZ or Lab PCS value, convert in the fwd direction into */ 
/* the nominated output PCS (ie. Absolute, Jab etc.) */
/* (This is used in generating gamut compression in B2A tables) */
void icxLuLut_fwd_relpcs_outpcs(
icxLuBase *pp,
icColorSpaceSignature is,		/* Input space, XYZ or Lab */
double *out, double *in) {
	icxLuLut *p = (icxLuLut *)pp;


	/* Convert to the ICC PCS */
	if (is == icSigLabData && p->natpcs == icSigXYZData) {
		DBS(("fwd_relpcs_outpcs: Lab in = %s\n", icmPdv(p->inputChan, in)));
		icmLab2XYZ(&icmD50, out, in);
		DBS(("fwd_relpcs_outpcs: XYZ = %s\n", icmPdv(p->inputChan, out)));
	} else if (is == icSigXYZData && p->natpcs == icSigLabData) {
		DBS(("fwd_relpcs_outpcs: XYZ in = %s\n", icmPdv(p->inputChan, in)));
		icmXYZ2Lab(&icmD50, out, in);
		DBS(("fwd_relpcs_outpcs: Lab = %s\n", icmPdv(p->inputChan, out)));
	} else {
		DBS(("fwd_relpcs_outpcs: PCS in = %s\n", icmPdv(p->inputChan, in)));
		icmCpy3(out, in);
	}

	/* Convert to absolute */
	((icmLuLut *)p->plu)->out_abs((icmLuLut *)p->plu, out, out);

	DBS(("fwd_relpcs_outpcs: abs PCS = %s\n", icmPdv(p->inputChan, out)));

	if (p->outs == icxSigJabData) {

		/* Convert to CAM */
		p->cam->XYZ_to_cam(p->cam, out, out);

		DBS(("fwd_relpcs_outpcs: Jab = %s\n", icmPdv(p->inputChan, out)));
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Components of inverse lookup, in order */

/* Utility function - compute the clip vector direction. */
/* return NULL if vector clip isn't used. */
double *icxClipVector(
icxClip *p,			/* Clipping setup information */
double *in,			/* Target point */
double *cdirv		/* Space for returned clip vector */
) {
	int f;
	if (p->nearclip != 0)
		return NULL;			/* Doing nearest clipping, not vector */

	/* Default is simple vector clip */
	for (f = 0; f < p->fdi; f++)
		cdirv[f] = p->ocent[f] - in[f];	/* Clip towards output gamut center */

	if (p->ocentl != 0.0) {			/* Graduated vector clip */
		double cvl, nll;

		/* Compute (negative) clip vector length */
		for (cvl = 0.0, f = 0; f < p->fdi; f++) {
			cvl += cdirv[f] * cdirv[f];
		}
		cvl = sqrt(cvl);
		if (cvl > 1e-8) {
			/* Dot product of clip vector and clip center line */
			for (nll = 0.0, f = 0; f < p->fdi; f++)
				nll -= cdirv[f] * p->ocentv[f];	/* (Fix -ve clip vector) */
			nll /= (p->ocentl * p->ocentl);	/* Normalised location along line */

			/* Limit to line */
			if (nll < 0.0)
				nll = 0.0;
			else if (nll > 1.0)
				nll = 1.0;

			if (p->LabLike) {
				/* Aim more towards center for saturated targets */
				double sat = sqrt(in[1] * in[1] + in[2] * in[2]);
				nll += sat/150.0 * (0.5 - nll);
			}

			/* Compute target clip direction */
			for (f = 0; f < p->fdi; f++)
				cdirv[f] = p->ocent[f] + nll * p->ocentv[f] - in[f];
		}
	}

	return cdirv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Typically inv is used to invert a device->PCS table, */
/* so it is doing a PCS->device conversion. */
/* This doesn't always have to be the case though. */

/* PCS override (Effective PCS) to PCS conversion + absolute to relative conversion */
int icxLuLut_inv_out_abs(icxLuLut *p, double *out, double *in) {
	int rv = 0;

	DBR(("\nicxLuLut_inv_out_abs got PCS %s\n",icmPdv(p->outputChan, in)));

	if (p->mergeclut == 0) {
		if (p->outs == icxSigJabData) {
			p->cam->cam_to_XYZ(p->cam, out, in);
			DBR(("icxLuLut_inv_out_abs after cam2XYZ %s\n",icmPdv(p->outputChan, out)));
			/* Hack to prevent CAM02 weirdness being amplified by inv_out_abs() */
			/* or per channel clipping. */
			/* Limit -Y to non-stupid values by scaling */
			if (out[1] < -0.1) {
				out[0] *= -0.1/out[1];
				out[2] *= -0.1/out[1];
				out[1] = -0.1;
				DBR(("icxLuLut_inv_out_abs after clipping -Y %s\n",icmPdv(p->outputChan, out)));
			}
			rv |= ((icmLuLut *)p->plu)->inv_out_abs((icmLuLut *)p->plu, out, out);
			DBR(("icxLuLut_inv_out_abs after icmLut inv_out_abs %s\n",icmPdv(p->outputChan, out)));
		} else {
			rv |= ((icmLuLut *)p->plu)->inv_out_abs((icmLuLut *)p->plu, out, in);
			DBR(("icxLuLut_inv_out_abs after icmLut inv_out_abs %s\n",icmPdv(p->outputChan, out)));
		}
	} else {
		int i;
		for (i = 0; i < p->outputChan; i++)
			out[i] = in[i];
	}
	DBR(("icxLuLut_inv_out_abs returning PCS %f %f %f\n",out[0],out[1],out[2]))
	return rv;
}

/* Do output->output' inverse lookup */
int icxLuLut_inv_output(icxLuLut *p, double *out, double *in) {
	int rv = 0;
	DBR(("icxLuLut_inv_output got PCS %f %f %f\n",in[0],in[1],in[2]))
	if (p->mergeclut == 0) {
#ifdef NEVER
		rv = ((icmLuLut *)p->plu)->inv_output((icmLuLut *)p->plu, out, in);
#else
		int i,j;
		int nsoln;				/* Number of solutions found */
		co pp[MAX_INVSOLN];		/* Room for all the solutions found */
		double cdir;

		for (i = 0; i < p->outputChan; i++) {
			pp[0].p[0] = p->outputClipc[i];
			pp[0].v[0] = in[i];
			cdir = p->outputClipc[i] - in[i];	/* Clip towards output range */

			nsoln = p->outputTable[i]->rev_interp (
				p->outputTable[i], 	/* this */
				RSPL_NEARCLIP,		/* Clip to nearest (faster than vector) */
				MAX_INVSOLN,		/* Maximum number of solutions allowed for */
				NULL, 				/* No auxiliary input targets */
				&cdir,				/* Clip vector direction and length */
				pp);				/* Input and output values */

			if (nsoln & RSPL_DIDCLIP)
				rv = 1;

			nsoln &= RSPL_NOSOLNS;		/* Get number of solutions */

			if (nsoln == 1) { /* Exactly one solution */
				j = 0;
			} else if (nsoln == 0) {	/* Zero solutions. This is unexpected. */
				error("xlut: Unexpected failure to find reverse solution for output table");
				return 2;
			} else {		/* Multiple solutions */
				/* Use a simple minded resolution - choose the one closest to the center */
				double bdist = 1e300;
				int bsoln = 0;
				/* Don't expect this - 1D luts are meant to be monotonic */
				warning("1D lut inversion got %d reverse solutions\n",nsoln);
				warning("solution 0 = %f\n",pp[0].p[0]);
				warning("solution 1 = %f\n",pp[1].p[0]);
				for (j = 0; j < nsoln; j++) {
					double tt;
					tt = pp[i].p[0] - p->outputClipc[i];
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

#endif /* NEVER */
	} else {
		int i;
		for (i = 0; i < p->outputChan; i++)
			out[i] = in[i];
	}
	DBR(("icxLuLut_inv_output returning PCS' %f %f %f\n",out[0],out[1],out[2]))
	return rv;
}

/* Ink limit+gamut limit calculation function for xLuLut. */
/* Returns < 0.0 if input value is within limits, */
/* > 0.0 if outside limits. Value is proportinal to distance to limits. */
/* We implement device gamut check to improve utility outside rspl, */
/* in optimisation routines. */
/* The limits are assumed to be post calibrated device values (ie. real */
/* final device values) */ 
static double icxLimit(
icxLuLut *p,
double *in
) {
	double cin[MAX_CHAN];	/* Calibrated input values */
	double tlim, klim;
	double ovr, val;
	int e;

	if (p->pp->cal != NULL) {	/* We have device calibration information */
		p->pp->cal->interp(p->pp->cal, cin, in);
	} else {
		for (e = 0; e < p->inputChan; e++)
			cin[e] = in[e];
	}

	if ((tlim = p->ink.tlimit) < 0.0)
		tlim = (double)p->inputChan;	/* Default is no limit */

	if ((klim = p->ink.klimit) < 0.0)
		klim = 1.0;

	/* Compute amount outside total limit */
	{					/* No calibration */
		double sum;
		for (sum = 0.0, e = 0; e < p->inputChan; e++)
			sum += cin[e];
		val = sum - tlim;
	}

	/* Compute amount outside black limit */
	if (p->ink.klimit >= 0.0) {
		double kval = 0.0;
		switch(p->natis) {
			case icSigCmykData:
				kval = cin[3] - klim;
				break;
			default:
				/* NOTE !!! p->kch isn't being initialized !!! */
				if (p->kch >= 0) {
					kval = cin[p->kch] - klim;
				} else {
					error("xlut: Unknown colorspace when black limit specified");
				}
		}
		if (kval > val)
			val = kval;
	}
	/* Compute amount outside device value limits 0.0 - 1.0 */
	for (ovr = -1.0, e = 0; e < p->inputChan; e++) {
		if (in[e] < 0.0) {				/* ! Not cin[] */
			if (-in[e] > ovr)
				ovr = -in[e];
		} else if (in[e] > 1.0) {
			if ((in[e] - 1.0) > ovr)
				ovr = in[e] - 1.0;
		}
	}
	if (ovr > val)
		val = ovr;

	return val;
}

/* Same as above, but works with input' values */
/* (If an ink limit is being used we assume that the */
/*  input space is not PCS, hence inv_in_abs() is doing nothing) */
static double icxLimitD(
icxLuLut *p,
double *ind
) {
	double in[MAX_CHAN];
	co tc;
	int e;

	/* Convert input' to input through revinput Luts (for speed) */
	for (e = 0; e < p->inputChan; e++) {
		tc.p[0] = ind[e];
		p->revinputTable[e]->interp(p->revinputTable[e], &tc);
		in[e] = tc.v[0];
	}

	return icxLimit(p, in);
}

/* Ink limit+gamut limit clipping function for xLuLut (CMYK). */
/* Return nz if there was clipping */
static int icxDoLimit(
icxLuLut *p,
double *out,
double *in
) {
	double tlim, klim = -1.0;
	double sum;
	int clip = 0, e;
	int kch = -1;

	for (e = 0; e < p->inputChan; e++)
		out[e] = in[e];

	if ((tlim = p->ink.tlimit) < 0.0)
		tlim = (double)p->inputChan;

	if ((klim = p->ink.klimit) < 0.0)
		klim = 1.0;

	/* Clip black */
	if (p->natis == icSigCmykData)
		kch = 3;
	else
		kch = p->kch;

	if (kch >= 0) {
		if (out[p->kch] > klim) {
			out[p->kch] = klim;	
			clip = 1;
		}
	}

	/* Compute amount outside total limit */
	for (sum = 0.0, e = 0; e < p->inputChan; e++)
		sum += out[e];

	if (sum > tlim) {
		clip = 1;
		sum /= (double)p->inputChan;
		for (e = 0; e < p->inputChan; e++)
			out[e] -= sum;
	}
	return clip;
}

#ifdef NEVER
#undef DBK
#define DBK(xxx) printf xxx ;
#else
#undef DBK
#define DBK(xxx) 
#endif

/* helper function that creates our standard K locus curve value, */
/* given the curve parameters, and the normalised L 0.0 - 1.0 value. */
/* No filtering version. */
/* !!! Should add K limit in here so that smoothing takes it into account !!! */
static double icxKcurveNF(double L, icxInkCurve *c) {
	double Kstpo, Kenpo, Kstle, Kenle;
	double rv;

	DBK(("icxKcurve got L = %f, smth %f skew %f, Parms %f %f %f %f %f\n",L, c->Ksmth, c->Kskew, c->Kstle, c->Kstpo, c->Kenpo, c->Kenle, c->Kshap));

	/* Invert sense of L, so that 0.0 = white, 1.0 = black */
	L = 1.0 - L;

	/* Clip L, just in case */
	if (L < 0.0) {
		L = 0.0;
	} else if (L > 1.0) {
		L = 1.0;
	}
	DBK(("Clipped inverted L = %f\n",L));

	/* Make sure breakpoints are ordered */
	if (c->Kstpo < c->Kenpo) {
		Kstle = c->Kstle;
		Kstpo = c->Kstpo;
		Kenpo = c->Kenpo;
		Kenle = c->Kenle;
	} else {	/* They're swapped */
		Kstle = c->Kenle;
		Kstpo = c->Kenpo;
		Kenpo = c->Kstpo;
		Kenle = c->Kstle;
	}

	if (L <= Kstpo) {
		/* We are at white level */
		rv = Kstle;
		DBK(("At white level %f\n",rv));
	} else if (L >= Kenpo) {
		/* We are at black level */
		rv = Kenle;
		DBK(("At black level %f\n",rv));
	} else {
		double Lp, g;
		/* We must be on the curve from start to end levels */

		Lp = (L - Kstpo)/(Kenpo - Kstpo);

		DBK(("Curve position %f\n",Lp));

		Lp = pow(Lp, c->Kskew);

		DBK(("Skewed curve position %f\n",Lp));

		g = c->Kshap/2.0;
		DBK(("Curve bf %f, g %g\n",Lp,g));

		/* A value of 0.5 will be tranlated to g */
		Lp = Lp/((1.0/g - 2.0) * (1.0 - Lp) + 1.0);

		DBK(("Skewed shaped %f\n",Lp));

		Lp = pow(Lp, 1.0/c->Kskew);

		DBK(("Shaped %f\n",Lp));

		/* Transition between start end end levels */
		rv = Lp * (Kenle - Kstle) + Kstle;

		DBK(("Scaled to start and end levele %f\n",rv));
	}

	DBK(("Returning %f\n",rv));
	return rv;
}

#ifdef DBK
#undef DBK
#define DBK(xxx) 
#endif


#ifdef NEVER
#undef DBK
#define DBK(xxx) printf xxx ;
#else
#undef DBK
#define DBK(xxx) 
#endif

/* Same as above, but implement transition filters accross inflection points. */
/* (The convolultion filter previously used could be */
/* re-instituted if something was done about compressing */
/* the filter at the boundaries so that the levels are met.) */
static double icxKcurve(double L, icxInkCurve *c) {

#ifdef DISABLE_KCURVE_FILTER
	return icxKcurveNF(L, c);

#else /* !DISABLE_KCURVE_FILTER */

	double Kstpo, Kenpo, Kstle, Kenle, Ksmth;
	double rv;

	DBK(("icxKcurve got L = %f, smth %f skew %f, Parms %f %f %f %f %f\n",L, c->Ksmth, c->Kskew, c->Kstle, c->Kstpo, c->Kenpo, c->Kenle, c->Kshap));

	/* Invert sense of L, so that 0.0 = white, 1.0 = black */
	L = 1.0 - L;

	/* Clip L, just in case */
	if (L < 0.0) {
		L = 0.0;
	} else if (L > 1.0) {
		L = 1.0;
	}
	DBK(("Clipped inverted L = %f\n",L));

	/* Make sure breakpoints are ordered */
	if (c->Kstpo < c->Kenpo) {
		Kstle = c->Kstle;
		Kstpo = c->Kstpo;
		Kenpo = c->Kenpo;
		Kenle = c->Kenle;
	} else {	/* They're swapped */
		Kstle = c->Kenle;
		Kstpo = c->Kenpo;
		Kenpo = c->Kstpo;
		Kenle = c->Kstle;
	}
	Ksmth = c->Ksmth;

	/* Curve value at point */
	rv = icxKcurveNF(1.0 - L, c);

	DBK(("Raw output at iL = %f, rv\n",L));

	/* Create filtered value */
	{
		double wbs, wbe;		/* White transitioin start, end */
		double wbl, wfv;		/* White blend factor, value at filter band */

		double mt;				/* Middle of the two transitions */

		double bbs, bbe;		/* Black transitioin start, end */
		double bbl, bfv;		/* Black blend factor, value at filter band */

		wbs = Kstpo - Ksmth;
		wbe = Kstpo + Ksmth;

		bbs = Kenpo - 1.0 * Ksmth;
		bbe = Kenpo + 1.0 * Ksmth;

		mt = 0.5 * (wbe + bbs);

		/* Make sure that the whit & black transition regions */
		/* don't go out of bounts or overlap */
		if (wbs < 0.0) {
			wbe += wbs;   
			wbs = 0.0;
		}
		if (bbe > 1.0) {
			bbs += (bbe - 1.0);
			bbe = 1.0;
		}

		if (wbe > mt) {
			wbs += (wbe - mt);
			wbe = mt;
		}

		if (bbs < mt) {
			bbe += (mt - bbs);   
			bbs = mt;
		}

		DBK(("Transition windows %f - %f, %f - %f\n",wbs, wbe, bbw, bbe));
		if (wbs < wbe) {
			wbl = (L - wbe)/(wbs - wbe);

			if (wbl > 0.0 && wbl < 1.0) {
				wfv = icxKcurveNF(1.0 - wbe, c);
				DBK(("iL = %f, wbl = %f, wfv = %f\n",L,Kstpo,wbl,wfv));
	
				wbl = 1.0 - pow(1.0 - wbl, 2.0);
				rv = wbl * Kstle + (1.0 - wbl) * wfv;
			}
		}
		if (bbs < bbe) {
			bbl = (L - bbe)/(bbs - bbe);

			if (bbl > 0.0 && bbl < 1.0) {
				bfv = icxKcurveNF(1.0 - bbs, c);
				DBK(("iL = %f, bbl = %f, bfv = %f\n",L,Kstpo,bbl,bfv));
	
				bbl = pow(bbl, 2.0);
				rv = bbl * bfv + (1.0 - bbl) * Kenle;
			}
		}
	}

	/* To be safe */
	if (rv < 0.0)
		rv = 0.0;
	else if (rv > 1.0)
		rv = 1.0;

	DBK(("Returning %f\n",rv));
	return rv;
#endif /* !DISABLE_KCURVE_FILTER */
}

#ifdef DBK
#undef DBK
#define DBK(xxx) 
#endif

/* Do output'->input' lookup with aux details. */
/* Note that out[] will be used as the inking value if icxKrule is */
/* icxKvalue, icxKlocus, icxKl5l or icxKl5lk, and that the auxiliar values, PCS ranges */
/* and icxKrule value will all be evaluated in output->input space (not ' space). */
/* Note that the ink limit will be computed after converting input' to input, auxt */
/* will override the inking rule, and auxr[] reflects the available auxiliary range */
/* that the locus was to choose from, and auxv[] was the actual auxiliary used. */
/* Returns clip status. */
int icxLuLut_inv_clut_aux(
icxLuLut *p,
double *out,	/* Function return values, plus aux value or locus target input if auxt == NULL */
double *auxv,	/* If not NULL, return aux value used (packed) */
double *auxr,	/* If not NULL, return aux locus range (packed, 2 at a time) */
double *auxt,	/* If not NULL, specify the aux target for this lookup (override ink) */
double *clipd,	/* If not NULL, return DE to gamut on clipi, 0 for not clip */
double *in		/* Function input values to invert (== clut output' values) */
) {
	co pp[MAX_INVSOLN];		/* Room for all the solutions found */
	int nsoln;			/* Number of solutions found */
	double *cdir, cdirv[MXDO];	/* Clip vector direction and length */
	int e,f,i;
	int fdi = p->clutTable->fdi;
	int flags = 0;		/* reverse interp flags */
	int xflags = 0;		/* extra clip/noclip flags */
	double tin[MXDO];	/* PCS value to be inverted */
	double cdist = 0.0;	/* clip DE */
	int crv = 0;		/* Return value - set to 1 if clipped */

	if (p->nearclip != 0)
		flags |= RSPL_NEARCLIP;			/* Use nearest clipping rather than clip vector */

	DBR(("inv_clut_aux input is %f %f %f\n",in[0], in[1], in[2]))

	if (auxr != NULL) {		/* Set a default locus range */
		int ee = 0;
		for (e = 0; e < p->clutTable->di; e++) {
			if (p->auxm[e] != 0) {
				auxr[ee++] = 1e60;
				auxr[ee++] = -1e60;
			}
		}
	}

	/* Setup for reverse lookup */
	for (f = 0; f < fdi; f++)
		pp[0].v[f] = in[f];				/* Target value */

	/* Compute clip vector, if any */
	cdir = icxClipVector(&p->clip, in, cdirv);

	if (p->clutTable->di > fdi) {	/* ie. CMYK->Lab, there will be ambiguity */
		double min[MXDI], max[MXDI];	/* Auxiliary locus range */

#ifdef REPORT_LOCUS_SEGMENTS	/* Examine how many segments there are */
		{	/* ie. CMYK->Lab, there will be ambiguity */
			double smin[10][MXRI], smax[10][MXRI];	/* Auxiliary locus segment ranges */
			double min[MXRI], max[MXRI];	/* Auxiliary min and max locus range */

			nsoln = p->clutTable->rev_locus_segs(
				p->clutTable,	/* rspl object */
				p->auxm,		/* Auxiliary mask */
				pp,				/* Input target and output solutions */
				10,				/* Maximum number of solutions to return */
				smin, smax);		/* Returned locus of valid auxiliary values */

			if (nsoln != 0) {
				/* Convert the locuses from input' -> input space */
				/* and get overall min/max locus range */
				for (e = 0; e < p->clutTable->di; e++) {
					co tc;
					/* (Is speed more important than precision ?) */
					if (p->auxm[e] != 0) {
						for (i = 0; i < nsoln; i++) {
							tc.p[0] = smin[i][e];
							p->revinputTable[e]->interp(p->revinputTable[e], &tc);
							smin[i][e] = tc.v[0];
							tc.p[0] = smax[i][e];
							p->revinputTable[e]->interp(p->revinputTable[e], &tc);
							smax[i][e] = tc.v[0];
							printf("  Locus seg %d:[%d] %f -> %f\n",i, e, smin[i][e], smax[i][e]);
						}
					}
				}
			}
		}
#endif /* REPORT_LOCUS_SEGMENTS */

		/* Compute auxiliary locus on the fly. This is in dev' == input' space. */
		nsoln = p->clutTable->rev_locus(
			p->clutTable,	/* rspl object */
			p->auxm,		/* Auxiliary mask */
			pp,				/* Input target and output solutions */
			min, max);		/* Returned locus of valid auxiliary values */
		
		if (nsoln == 0) {
			xflags |= RSPL_WILLCLIP;	/* No valid locus, so we expect to have to clip */
#ifdef DEBUG_RLUT
			printf("inv_clut_aux: no valid locus, expect clip\n");
#endif
			/* Make sure that the auxiliar value is initialized, */
			/* even though it won't have any effect. */
			for (e = 0; e < p->clutTable->di; e++) {
				if (p->auxm[e] != 0) {
					pp[0].p[e] = 0.5;
				}
			}

		} else {  /* Got a valid locus */

			/* Convert the locuses from input' -> input space */
			for (e = 0; e < p->clutTable->di; e++) {
				co tc;
				/* (Is speed more important than precision ?) */
				if (p->auxm[e] != 0) {
					tc.p[0] = min[e];
					p->revinputTable[e]->interp(p->revinputTable[e], &tc);
					min[e] = tc.v[0];
					tc.p[0] = max[e];
					p->revinputTable[e]->interp(p->revinputTable[e], &tc);
					max[e] = tc.v[0];
				}
			}

			if (auxr != NULL) {		/* Report the locus range */
				int ee = 0;
				for (e = 0; e < p->clutTable->di; e++) {
					if (p->auxm[e] != 0) {
						auxr[ee++] = min[e];
						auxr[ee++] = max[e];
					}
				}
			}

			if (auxt != NULL) {		/* overiding auxiliary target */
				int ee = 0;
				for (e = 0; e < p->clutTable->di; e++) {
					if (p->auxm[e] != 0) {
						double iv = auxt[ee++];
						if (iv < min[e])
							iv = min[e];
						else if (iv > max[e])
							iv = max[e];
						pp[0].p[e] = iv;
					}
				}
				DBR(("inv_clut_aux: aux %f from auxt[] %f\n",pp[0].p[3],auxt[0]))
			} else if (p->ink.k_rule == icxKvalue) {
				/* Implement the auxiliary inking rule */
				/* Target auxiliary values are provided in out[] K value */
				for (e = 0; e < p->clutTable->di; e++) {
					if (p->auxm[e] != 0) {
						double iv = out[e];		/* out[] holds aux target value */
						if (iv < min[e])
							iv = min[e];
						else if (iv > max[e])
							iv = max[e];
						pp[0].p[e] = iv;
					}
				}
				DBR(("inv_clut_aux: aux %f from out[0] K target %f min %f max %f\n",pp[0].p[3],out[3],min[3],max[3]))
			} else if (p->ink.k_rule == icxKlocus) {
				/* Set target auxliary input values from values in out[] and locus */
				for (e = 0; e < p->clutTable->di; e++) {
					if (p->auxm[e] != 0) {
						double ii, iv;
						ii = out[e];							/* Input ink locus */
						iv = min[e] + ii * (max[e] - min[e]);	/* Output ink from locus */
						if (iv < min[e])
							iv = min[e];
						else if (iv > max[e])
							iv = max[e];
						pp[0].p[e] = iv;
					}
				}
				DBR(("inv_clut_aux: aux %f from out[0] locus %f min %f max %f\n",pp[0].p[3],out[3],min[3],max[3]))
			} else { /* p->ink.k_rule == icxKluma5 || icxKluma5k || icxKl5l || icxKl5lk */
				/* Auxiliaries are driven by a rule and the output values */
				double rv, L;

				/* If we've got a mergeclut, then the PCS' is the same as the */
				/* effective PCS, and we need to convert to native PCS */
				if (p->mergeclut) {
					p->mergeclut = 0;					/* Hack to be able to use inv_out_abs() */
					icxLuLut_inv_out_abs(p, tin, in);
					p->mergeclut = 1;

				} else {
					/* Convert native PCS' to native PCS values */
					p->output(p, tin, in);	
				}

				/* Figure out Luminance number */
				if (p->natos == icSigXYZData) {
					icmXYZ2Lab(&icmD50, tin, tin);
				} else if (p->natos != icSigLabData) {	/* Hmm. that's unexpected */
					error("Assert: xlut K locus, unexpected native pcs of 0x%x\n",p->natos);
				}
				L = 0.01 * tin[0];
				DBR(("inv_clut_aux: aux from Luminance, raw L = %f\n",L));

				/* Normalise L to its possible range from min to max */
				L = (L - p->Lmin)/(p->Lmax - p->Lmin);
				DBR(("inv_clut_aux: Normalize L = %f\n",L));

				/* Convert L to curve value */
				rv = icxKcurve(L, &p->ink.c);
				DBR(("inv_clut_aux: icxKurve lookup returns = %f\n",rv));

				if (p->ink.k_rule == icxKluma5) {	/* Curve is locus value */

					/* Set target black as K fraction within locus */

					for (e = 0; e < p->clutTable->di; e++) {
						if (p->auxm[e] != 0) {
							pp[0].p[e] = min[e] + rv * (max[e] - min[e]);
						}
					}
					DBR(("inv_clut_aux: aux %f from locus %f min %f max %f\n",pp[0].p[3],rv,min[3],max[3]))

				} else if (p->ink.k_rule == icxKluma5k) {	/* Curve is K value */

					for (e = 0; e < p->clutTable->di; e++) {
						if (p->auxm[e] != 0) {
							double iv = rv;
							if (iv < min[e])			/* Clip to locus */
								iv = min[e];
							else if (iv > max[e])
								iv = max[e];
							pp[0].p[e] = iv;
						}
					}
					DBR(("inv_clut_aux: aux %f from out[0] K target %f min %f max %f\n",pp[0].p[3],rv,min[3],max[3]))

				} else { /* icxKl5l || icxKl5lk */
					/* Create second curve, and use input locus to */
					/* blend between */

					double rv2;		/* Upper limit */

					/* Convert L to max curve value */
					rv2 = icxKcurve(L, &p->ink.x);

					if (rv2 < rv) {		/* Ooops - better swap. */
						double tt;
						tt = rv;
						rv = rv2;
						rv2 = tt;
					}

					for (e = 0; e < p->clutTable->di; e++) {
						if (p->auxm[e] != 0) {
							if (p->ink.k_rule == icxKl5l) {
								double ii;
								ii = out[e];				/* Input K locus */
								if (ii < 0.0)
									ii = 0.0;
								else if (ii > 1.0)
									ii = 1.0;
								ii = (1.0 - ii) * rv + ii * rv2;/* Blend between locus rule curves */
								/* Out ink from output locus */
								pp[0].p[e] = min[e] + ii * (max[e] - min[e]);
							} else {
								double iv;
								iv = out[e];				/* Input K level */
								if (iv < rv)				/* Constrain to curves */
									iv = rv;
								else if (iv > rv2)
									iv = rv2;
								pp[0].p[e] = iv; 
							}
						}
					}
					DBR(("inv_clut_aux: aux %f from 2 curves\n",pp[0].p[3]))
				}
			}

			/* Convert to input/dev aux target to input'/dev' space for rspl inversion */
			for (e = 0; e < p->clutTable->di; e++) {
				double tv, bv = 0.0, bd = 1e6;
				co tc;
				if (p->auxm[e] != 0)  {
					tv = pp[0].p[e];
					/* Clip to overall locus range (belt and braces) */
					if (tv < min[e])
						tv = min[e];
					if (tv > max[e])
						tv = max[e];
					tc.p[0] = tv;
					p->inputTable[e]->interp(p->inputTable[e], &tc);
					pp[0].p[e] = tc.v[0];
				}
			}

			xflags |= RSPL_EXACTAUX;	/* Since we confine aux to locus */

#ifdef DEBUG_RLUT
			printf("inv_clut_aux computed aux values ");
			for (e = 0; e < p->clutTable->di; e++) {
				if (p->auxm[e] != 0)
					printf("%d: %f ",e,pp[0].p[e]);
			}
			printf("\n");
#endif /* DEBUG_RLUT */
		}

		if (clipd != NULL) {	/* Copy pp.v[] to compute clip DE */
			for (f = 0; f < fdi; f++)
				tin[f] = pp[0].v[f];
		}

		/* Find reverse solution with target auxiliaries */
		/* We choose the closest aux at or above the target */
		/* to try and avoid glitches near black due to */
		/* possible forked black locuses. */
		nsoln = p->clutTable->rev_interp(
			p->clutTable, 	/* rspl object */
			RSPL_MAXAUX | flags | xflags,	/* Combine all the flags */
			MAX_INVSOLN, 	/* Maxumum solutions to return */
			p->auxm, 		/* Auxiliary input chanel mask */
			cdir,			/* Clip vector direction and length */
			pp);			/* Input target and output solutions */
							/* returned solutions in pp[0..retval-1].p[] */

	} else {
		DBR(("inv_clut_aux needs no aux value\n"))

		if (clipd != NULL) {	/* Copy pp.v[] to compute clip DE */
			for (f = 0; f < fdi; f++)
				tin[f] = pp[0].v[f];
		}

		/* Color spaces don't need auxiliaries to choose from solution locus */
		nsoln = p->clutTable->rev_interp(
			p->clutTable, 	/* rspl object */
			flags,			/* No extra flags */
			MAX_INVSOLN, 	/* Maxumum solutions to return */
			NULL, 			/* No auxiliary input targets */
			cdir,			/* Clip vector direction and length */
			pp);			/* Input target and output solutions */
							/* returned solutions in pp[0..retval-1].p[] */
	}
	if (nsoln & RSPL_DIDCLIP)
		crv = 1;			/* Clipped on PCS inverse lookup */

	if (crv && clipd != NULL) {


		/* Compute the clip DE */
		for (cdist = 0.0, f = 0; f < fdi; f++) {
			double tt;
			tt = pp[0].v[f] - tin[f];
			cdist += tt * tt;
		}
		cdist = sqrt(cdist);
		DBR(("Targ PCS %f %f %f Clipped %f %f %f cdist %f\n",tin[0],tin[1],tin[2],pp[0].v[0],pp[0].v[1],pp[0].v[2],cdist))
	}

	nsoln &= RSPL_NOSOLNS;		/* Get number of solutions */

	DBR(("inv_clut_aux got %d rev_interp solutions, clipflag = %d\n",nsoln,crv))

	/* If we clipped and we should clip in CAM Jab space, compute reverse */
	/* clip solution using our additional CAM space. */
	/* (Note that we don't support vector clip in CAM space at the moment) */
	if (crv != 0 && p->camclip && p->nearclip) {
		co cpp;				/* Alternate CAM space solution */
		double bf;			/* Blend factor */

		DBR(("inv_clut_aux got clip, compute CAM clip\n"))

		if (nsoln != 1) {	/* This would be unexpected */
			error("Unexpected failure to return 1 solution on clip for input to output table");
		}

		if (p->cclutTable == NULL) {	/* we haven't created this yet, so do so */
			if (icxLuLut_init_clut_camclip(p))
				error("Creating CAM rspl for camclip failed");
		}

		/* Setup for reverse lookup */
		DBR(("inv_clut_aux cam clip PCS in %f %f %f\n",in[0],in[1],in[2]))

		/* Convert from PCS' to (XYZ) PCS */
		((icmLuLut *)p->absxyzlu)->output((icmLuLut *)p->absxyzlu, tin, in);
		DBR(("inv_clut_aux cam clip PCS' -> PCS %f %f %f\n",tin[0],tin[1],tin[2]))

		((icmLuLut *)p->absxyzlu)->out_abs((icmLuLut *)p->absxyzlu, tin, tin);
		DBR(("inv_clut_aux cam clip abs XYZ PCS %f %f %f\n",tin[0],tin[1],tin[2]))

		p->cam->XYZ_to_cam(p->cam, tin, tin);
		DBR(("inv_clut_aux cam clip PCS after XYZtoCAM %f %f %f\n",tin[0],tin[1],tin[2]))

		for (f = 0; f < fdi; f++)	/* Transfer CAM targ */
			cpp.v[f] = tin[f];
		cpp.v[0] *= CCJSCALE;	

		/* Make sure that the auxiliar value is initialized, */
		/* even though it shouldn't have any effect, since should clipp. */
		for (e = 0; e < p->clutTable->di; e++) {
			if (p->auxm[e] != 0) {
				cpp.p[e] = 0.5;
			}
		}
		if (p->clutTable->di > fdi) {	/* ie. CMYK->Lab, there will be ambiguity */

			nsoln = p->cclutTable->rev_interp(
				p->cclutTable, 	/* rspl object */
				flags | xflags | RSPL_WILLCLIP,	/* Combine all the flags + clip ?? */
				1, 				/* Maximum solutions to return */
				p->auxm, 		/* Auxiliary input chanel mask */
				cdir,			/* Clip vector direction and length */
				&cpp);			/* Input target and output solutions */

		} else {
			nsoln = p->cclutTable->rev_interp(
				p->cclutTable, 	/* rspl object */
				flags | RSPL_WILLCLIP,	/* Because we know it will clip ?? */
				1, 				/* Maximum solutions to return */
				NULL, 			/* No auxiliary input targets */
				cdir,			/* Clip vector direction and length */
				&cpp);			/* Input target and output solutions */
		}

		nsoln &= RSPL_NOSOLNS;		/* Get number of solutions */

		if (nsoln != 1) {	/* This would be unexpected */
			error("Unexpected failure to return 1 solution on CAM clip for input to output table");
		}

		/* Compute the CAM clip distances */
		cpp.v[0] /= CCJSCALE;
		for (cdist = 0.0, f = 0; f < fdi; f++) {
			double tt;
			tt = cpp.v[f] - tin[f];
			cdist += tt * tt;
		}
		cdist = sqrt(cdist);
		DBR(("Targ CAM %f %f %f Clipped %f %f %f cdist %f\n",tin[0],tin[1],tin[2],cpp.v[0],cpp.v[1],cpp.v[2],cdist))

		/* Use magic number to set blend distance, and compute a blend factor. */
		/* Blend over 1 delta E, otherwise the Lab & CAM02 divergence can result */
		/* in reversals. */
		bf = cdist/CAMCLIPTRANS;		/* 0.0 for PCS result, 1.0 for CAM result */
		if (bf > 1.0)
			bf = 1.0;
#ifdef USECAMCLIPSPLINE
		bf = bf * bf * (3.0 - 2.0 * bf);	/* Convert to spline blend */
#endif
		DBR(("cdist %f, spline blend %f\n",cdist,bf))

		/* Blend between solution values for PCS and CAM clip result. */
		/* We're hoping that the solutions are close, and expect them to be */
		/* that way when we're close to the gamut surface (since the cell */
		/* vertex values should be exact, irrespective of which interpolation */
		/* space they're in), but weird things could happen away from the surface. */
		/* Weird things can happen anyway with "clip to nearest", since this is not */
		/* guaranteed to be a smooth function, depending on the gamut surface */
		/* geometry, without taking some precaution such as clipping to a */
		/* convex hull "wrapper" to create a clip vector, which we're not */
		/* current doing. */
		DBR(("Clip blend between device:\n"))
		DBR(("Lab: %s & ",icmPdv(p->clutTable->di, pp[0].p)))
		DBR(("Jab: %s ",icmPdv(p->clutTable->di, cpp.p)))

		for (e = 0; e < p->clutTable->di; e++) {
			out[e] = (1.0 - bf) * pp[0].p[e] + bf * cpp.p[e];
		}
		DBR((" = %s\n",icmPdv(p->clutTable->di, out)))

	/* Not CAM clip case */
	} else {

		if (nsoln == 1) { /* Exactly one solution */
			i = 0;
		} else if (nsoln == 0) {	/* Zero solutions. This is unexpected. */
			double in_v[MXDO];
			p->output(p, in_v, pp[0].v);		/* Get ICC inverse input values */
			p->out_abs(p, in_v, in_v);
			error("Unexpected failure to find reverse solution for input to output table for value %f %f %f (ICC input %f %f %f)",pp[0].v[0],pp[0].v[1],pp[0].v[2], in_v[0], in_v[1], in_v[2]);
			return 2;
		} else {		/* Multiple solutions */
			/* Use a simple minded resolution - choose the one closest to the center */
			double bdist = 1e300;
			int bsoln = 0;
			DBR(("got multiple reverse solutions\n"));
			for (i = 0; i < nsoln; i++) {
				double ss;

				DBR(("Soln %d: %s\n",i, icmPdv(p->clutTable->di, pp[i].p)))
				for (ss = 0.0, e = 0; e < p->clutTable->di; e++) {
					double tt;
					tt = pp[i].p[e] - p->licent[e];
					tt *= tt;
					if (tt < bdist) {	/* Better solution */
						bdist = tt;
						bsoln = i;
					}
				}
			}
#ifndef NEVER
			// ~~99 average them
			for (i = 1; i < nsoln; i++) {
				for (e = 0; e < p->clutTable->di; e++)
					pp[0].p[e] += pp[i].p[e];
			}
			for (e = 0; e < p->clutTable->di; e++)
				pp[0].p[e] /= (double)nsoln;
			bsoln = 0;
#endif
//printf("~1 chose %d\n",bsoln);
			i = bsoln;
		}
		for (e = 0; e < p->clutTable->di; e++) {
			/* Save solution as atractor for next one, on the basis */
			/* that it might have better continuity given pesudo-hilbert inversion path. */
			p->licent[e] = out[e] = pp[i].p[e];			/* Solution */
		}
	}

	/* Sanitise auxiliary locus range and auxiliary value return */
	if (auxr != NULL || auxv != NULL) {
		int ee = 0;
		for (e = 0; e < p->clutTable->di; e++) {
			double v = out[e];			/* Solution */
			if (p->auxm[e] != 0) {
				if (auxr != NULL) {			/* Make sure locus encloses actual value */
					if (auxr[2 * ee] > v)
						auxr[2 * ee] = v;
					if (auxr[2 * ee + 1] < v)
						auxr[2 * ee + 1] = v;
				}
				if (auxv != NULL) {
					auxv[ee] = v;
				}
				ee++;
			}
		}
	}

#ifdef CHECK_ILIMIT	/* Do sanity checks on meeting ink limit */
if (p->ink.tlimit >= 0.0 || p->ink.klimit >= 0.0) {
	double sum = icxLimitD(p, out);
	if (sum > 0.0)
		printf("xlut assert%s: icxLuLut_inv_clut returned outside limits by %f > tlimit %f\n",crv ? " (clip)" : "", sum, p->ink.tlimit);
}
#endif

	if (clipd != NULL) {
		*clipd = cdist;
		DBR(("inv_clut_aux returning clip DE %f\n",cdist))
	}

	DBR(("inv_clut_aux returning %f %f %f %f\n",out[0],out[1],out[2],out[3]))
	return crv;
}

/* Do output'->input' lookup, simple version */
/* Note than out[] will carry inking value if icxKrule is icxKvalue of icxKlocus */
/* and that the icxKrule value will be in the input (NOT input') space. */
/* Note that the ink limit will be computed after converting input' to input */
int icxLuLut_inv_clut(icxLuLut *p, double *out, double *in) {
	return icxLuLut_inv_clut_aux(p, out, NULL, NULL, NULL, NULL, in);
}

/* Given the proposed auxiliary input values in in[di], */
/* and the target output' (ie. PCS') values in out[fdi], */
/* return the auxiliary input (NOT input' space) values as a proportion of their */
/* possible locus in locus[di]. */
/* This is generally used on a source CMYK profile to convey the black intent */
/* to destination CMYK profile. */
int icxLuLut_clut_aux_locus(icxLuLut *p, double *locus, double *out, double *in) {
	co pp[1];		/* Room for all the solutions found */
	int nsoln;		/* Number of solutions found */
	int e,f;

	if (p->clutTable->di > p->clutTable->fdi) {	/* ie. CMYK->Lab, there will be ambiguity */
		double min[MXDI], max[MXDI];	/* Auxiliary locus range */

		/* Setup for auxiliary locus lookup */
		for (f = 0; f < p->clutTable->fdi; f++) {
			pp[0].v[f] = out[f];			/* Target output' (i.e. PCS) value */
		}
	
		/* Compute auxiliary locus */
		nsoln = p->clutTable->rev_locus(
			p->clutTable,	/* rspl object */
			p->auxm,		/* Auxiliary mask */
			pp,				/* Input target and output solutions */
			min, max);		/* Returned locus of valid auxiliary values */
		
		if (nsoln == 0) {
			for (e = 0; e < p->clutTable->di; e++)
				locus[e] = 0.0;		/* Return some safe values */
		} else {  /* Got a valid locus */

			/* Convert the locus from input' -> input space */
			for (e = 0; e < p->clutTable->di; e++) {
				co tc;
				/* (Is speed more important than precision ?) */
				if (p->auxm[e] != 0) {
					tc.p[0] = min[e];
					p->revinputTable[e]->interp(p->revinputTable[e], &tc);
					min[e] = tc.v[0];
					tc.p[0] = max[e];
					p->revinputTable[e]->interp(p->revinputTable[e], &tc);
					max[e] = tc.v[0];
				}
			}

			/* Figure out the proportion of the locus */
			for (e = 0; e < p->clutTable->di; e++) {
				if (p->auxm[e] != 0) {
					double iv = in[e];
					if (iv <= min[e])
						locus[e] = 0.0;
					else if (iv >= max[e])
						locus[e] = 1.0;
					else {
						double lpl = max[e] - min[e];	/* Locus path length */
						if (lpl > 1e-6)
							locus[e] = (iv - min[e])/lpl;
						else
							locus[e] = 0.0;
					}
				}
			}
		}
	} else {
		/* There should be no auxiliaries */
		for (e = 0; e < p->clutTable->di; e++)
			locus[e] = 0.0;		/* Return some safe values */
	}
	return 0;
}

/* Do input' -> input inverse lookup */
int icxLuLut_inv_input(icxLuLut *p, double *out, double *in) {
#ifdef NEVER
	return ((icmLuLut *)p->plu)->inv_input((icmLuLut *)p->plu, out, in);
#else
	int rv = 0;
	int i,j;
	int nsoln;				/* Number of solutions found */
	co pp[MAX_INVSOLN];		/* Room for all the solutions found */
	double cdir;

	DBR(("inv_input got DEV' %f %f %f %f\n",in[0],in[1],in[2],in[3]))

	for (i = 0; i < p->inputChan; i++) {
		pp[0].p[0] = p->inputClipc[i];
		pp[0].v[0] = in[i];
		cdir = p->inputClipc[i] - in[i];	/* Clip towards output range */

		nsoln = p->inputTable[i]->rev_interp (
			p->inputTable[i], 	/* this */
			RSPL_NEARCLIP,		/* Clip to nearest (faster than vector) */
			MAX_INVSOLN,		/* Maximum number of solutions allowed for */
			NULL, 				/* No auxiliary input targets */
			&cdir,				/* Clip vector direction and length */
			pp);				/* Input and output values */

		if (nsoln & RSPL_DIDCLIP)
			rv = 1;

		nsoln &= RSPL_NOSOLNS;		/* Get number of solutions */

		if (nsoln == 1) { /* Exactly one solution */
			j = 0;
		} else if (nsoln == 0) {	/* Zero solutions. This is unexpected. */
			error("Unexpected failure to find reverse solution for input table");
			return 2;
		} else {		/* Multiple solutions */
			/* Use a simple minded resolution - choose the one closest to the center */
			double bdist = 1e300;
			int bsoln = 0;
			/* Don't expect this - 1D luts are meant to be monotonic */
			warning("1D lut inversion got %d reverse solutions\n",nsoln);
			warning("solution 0 = %f\n",pp[0].p[0]);
			warning("solution 1 = %f\n",pp[1].p[0]);
			for (j = 0; j < nsoln; j++) {
				double tt;
				tt = pp[i].p[0] - p->inputClipc[i];
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

	DBR(("inv_input returning DEV %f %f %f %f\n",out[0],out[1],out[2],out[3]))
	return rv;
#endif /* NEVER */
}

/* Possible inverse matrix lookup */
/* (Will do nothing if input is device space) */
int icxLuLut_inv_matrix(icxLuLut *p, double *out, double *in) {
	int rv = 0;
	rv |= ((icmLuLut *)p->plu)->inv_matrix((icmLuLut *)p->plu, out, in);
	return rv;
}

/* Inverse input absolute intent conversion */
/* (Will do nothing if input is device space) */
int icxLuLut_inv_in_abs(icxLuLut *p, double *out, double *in) {
	int rv = 0;
	rv |= ((icmLuLut *)p->plu)->inv_in_abs((icmLuLut *)p->plu, out, in);

	if (p->ins == icxSigJabData) {
		p->cam->XYZ_to_cam(p->cam, out, out);
	}

	return rv;
}

/* Overall inverse lookup */
/* Note that all auxiliary values are in input (NOT input') space */
static int
icxLuLut_inv_lookup(
icxLuBase *pp,		/* This */
double *out,		/* Vector of output values/input auxiliary values */
double *in			/* Vector of input values */
) {
	icxLuLut *p = (icxLuLut *)pp;
	int rv = 0;
	int i;
	double temp[MAX_CHAN];

	DBOL(("xiccilu: input            = %s\n", icmPdv(p->outputChan, in)));
	if (p->mergeclut == 0) {		/* Do this if it's not merger with clut */
		rv |= p->inv_out_abs (p, temp, in);
		DBOL(("xiccilu: after inv abs    = %s\n", icmPdv(p->outputChan, temp)));
		rv |= p->inv_output  (p, temp, temp);
		DBOL(("xiccilu: after inv out    = %s\n", icmPdv(p->outputChan, temp)));
	} else {
		for (i = 0; i < p->outputChan; i++)
			temp[i] = in[i];
	}
	DBOL(("xiccilu: aux targ   = %s\n", icmPdv(p->inputChan,out)));
	rv |= p->inv_clut    (p, out, temp);
	DBOL(("xiccilu: after inv clut   = %s\n", icmPdv(p->inputChan,out)));
	rv |= p->inv_input   (p, out, out);
	DBOL(("xiccilu: after inv input  = %s\n", icmPdv(p->inputChan,out)));
	rv |= p->inv_matrix  (p, out, out);
	DBOL(("xiccilu: after inv matrix = %s\n", icmPdv(p->inputChan,out)));
	rv |= p->inv_in_abs  (p, out, out);
	DBOL(("xiccilu: after inv abs    = %s\n", icmPdv(p->inputChan,out)));
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given a nominated output PCS (ie. Absolute, Jab etc.), convert it in the bwd */
/* direction into a relative XYZ or Lab PCS value */
/* (This is used in generating gamut compression in B2A tables) */
void icxLuLut_bwd_outpcs_relpcs(
icxLuBase *pp,
icColorSpaceSignature os,		/* Output space, XYZ or Lab */
double *out, double *in) {
	icxLuLut *p = (icxLuLut *)pp;

	if (p->outs == icxSigJabData) {
		DBS(("bwd_outpcs_relpcs: Jab in = %s\n", icmPdv(3, in)));
		p->cam->cam_to_XYZ(p->cam, out, in);
		DBS(("bwd_outpcs_relpcs: abs XYZ = %s\n", icmPdv(3, out)));
		/* Hack to prevent CAM02 weirdness being amplified by */
		/* per channel clipping. */
		/* Limit -Y to non-stupid values by scaling */
		if (out[1] < -0.1) {
			out[0] *= -0.1/out[1];
			out[2] *= -0.1/out[1];
			out[1] = -0.1;
			DBS(("bwd_outpcs_relpcs: after clipping -Y %s\n",icmPdv(p->outputChan, out)));
		}
	} else {
		DBS(("bwd_outpcs_relpcs: abs PCS in = %s\n", icmPdv(3, out)));
		icmCpy3(out, in);
	}

	((icmLuLut *)p->plu)->inv_out_abs((icmLuLut *)p->plu, out, out);
	DBS(("bwd_outpcs_relpcs: rel PCS = %s\n", icmPdv(3, out)));

	if (os == icSigXYZData && p->natpcs == icSigLabData) {
		icmLab2XYZ(&icmD50, out, out);
		DBS(("bwd_outpcs_relpcs: rel XYZ = %s\n", icmPdv(3, out)));
	} else if (os == icSigXYZData && p->natpcs == icSigLabData) {
		icmXYZ2Lab(&icmD50, out, out);
		DBS(("bwd_outpcs_relpcs: rel Lab = %s\n", icmPdv(3, out)));
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Return LuLut information */
static void icxLuLut_get_info(
	icxLuLut     *p,		/* this */
	icmLut       **lutp,	/* Pointer to icc lut type return value */
	icmXYZNumber *pcswhtp,	/* Pointer to profile PCS white point return value */
	icmXYZNumber *whitep,	/* Pointer to profile absolute white point return value */
	icmXYZNumber *blackp	/* Pointer to profile absolute black point return value */
) {
	((icmLuLut *)p->plu)->get_info((icmLuLut *)p->plu, lutp, pcswhtp, whitep, blackp);
}

/* Return the underlying Lut matrix */
static void
icxLuLut_get_matrix (
	icxLuLut *p,
	double m[3][3]
) {
	((icmLuLut *)p->plu)->get_matrix((icmLuLut *)p->plu, m);
}

static void
icxLuLut_free(
icxLuBase *pp
) {
	icxLuLut *p = (icxLuLut *)pp;
	int i;

	for (i = 0; i < p->inputChan; i++) {
		if (p->inputTable[i] != NULL)
			p->inputTable[i]->del(p->inputTable[i]);
		if (p->revinputTable[i] != NULL)
			p->revinputTable[i]->del(p->revinputTable[i]);
	}

	if (p->clutTable != NULL)
		p->clutTable->del(p->clutTable);

	if (p->cclutTable != NULL)
		p->cclutTable->del(p->cclutTable);

	for (i = 0; i < p->outputChan; i++) {
		if (p->outputTable[i] != NULL)
			p->outputTable[i]->del(p->outputTable[i]);
	}

	if (p->plu != NULL)
		p->plu->del(p->plu);

	if (p->cam != NULL)
		p->cam->del(p->cam);

	if (p->absxyzlu != NULL)
		p->absxyzlu->del(p->absxyzlu);

	free(p);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

static gamut *icxLuLutGamut(icxLuBase *plu, double detail); 

/* Do the basic icxLuLut creation and initialisation */
static icxLuLut *
alloc_icxLuLut(
	xicc                  *xicp,
	icmLuBase             *plu,			/* Pointer to Lu we are expanding (ours) */
	int                   flags			/* clip, merge flags */
) {
	icxLuLut *p;						/* Object being created */
	icmLuLut *luluto = (icmLuLut *)plu;	/* Lookup Lut type object */

	if ((p = (icxLuLut *) calloc(1,sizeof(icxLuLut))) == NULL)
		return NULL;

	p->pp                = xicp;
	p->plu               = plu;
	p->del               = icxLuLut_free;
	p->lutspaces         = icxLutSpaces;
	p->spaces            = icxLuSpaces;
	p->get_native_ranges = icxLu_get_native_ranges;
	p->get_ranges        = icxLu_get_ranges;
	p->efv_wh_bk_points  = icxLuEfv_wh_bk_points;
	p->get_gamut         = icxLuLutGamut;
	p->fwd_relpcs_outpcs = icxLuLut_fwd_relpcs_outpcs;
	p->bwd_outpcs_relpcs = icxLuLut_bwd_outpcs_relpcs;
	p->nearclip = 0;				/* Set flag defaults */
	p->mergeclut = 0;
	p->noisluts = 0;
	p->noipluts = 0;
	p->nooluts = 0;
	p->intsep = 0;

	p->lookup   = icxLuLut_lookup;
	p->in_abs   = icxLuLut_in_abs;
	p->matrix   = icxLuLut_matrix;
	p->input    = icxLuLut_input;
	p->clut     = icxLuLut_clut;
	p->clut_aux = icxLuLut_clut_aux;
	p->output   = icxLuLut_output;
	p->out_abs  = icxLuLut_out_abs;

	p->inv_lookup   = icxLuLut_inv_lookup;
	p->inv_in_abs   = icxLuLut_inv_in_abs;
	p->inv_matrix   = icxLuLut_inv_matrix;
	p->inv_input    = icxLuLut_inv_input;
	p->inv_clut     = icxLuLut_inv_clut;
	p->inv_clut_aux = icxLuLut_inv_clut_aux;
	p->inv_output   = icxLuLut_inv_output;
	p->inv_out_abs  = icxLuLut_inv_out_abs;

	p->clut_locus   = icxLuLut_clut_aux_locus;

	p->get_info   = icxLuLut_get_info;
	p->get_matrix = icxLuLut_get_matrix;

	/* Setup all the rspl analogs of the icc Lut */
	/* NOTE: We assume that none of this relies on the flag settings, */
	/* since they will be set on our return. */

	/* Get details of underlying, native icc color space */
	p->plu->lutspaces(p->plu, &p->natis, NULL, &p->natos, NULL, &p->natpcs);

	/* Get other details of conversion */
	p->plu->spaces(p->plu, NULL, &p->inputChan, NULL, &p->outputChan, NULL, NULL, NULL, NULL, NULL);

	/* Remember flags */
	p->flags = flags;

	/* Sanity check */
	if (p->inputChan > MXDI) {
		sprintf(p->pp->err,"xicc can only handle input channels of %d or less",MXDI);
		p->inputChan = MXDI;		/* Avoid going outside array bounds */
		p->pp->errc = 1;
		p->del((icxLuBase *)p);
		return NULL;
	}
	if (p->outputChan > MXDO) {
		sprintf(p->pp->err,"xicc can only handle output channels of %d or less",MXDO);
		p->outputChan = MXDO;		/* Avoid going outside array bounds */
		p->pp->errc = 1;
		p->del((icxLuBase *)p);
		return NULL;
	}

	/* Get pointer to icmLut */
	luluto->get_info(luluto, &p->lut, NULL, NULL, NULL);

	return p;
}

/* Initialise the clut ink limiting and black */
/* generation information. */
/* return 0 or error code */
static int
setup_ink_icxLuLut(
icxLuLut *p,			/* Object being initialised */
icxInk   *ink,			/* inking details (NULL for default) */
int       setLminmax	/* Figure the L locus for inking rule */
) {
	int devchan = p->func == icmFwd ? p->inputChan : p->outputChan;

	if (ink) {
		p->ink = *ink;	/* Copy the structure */
	} else {
		p->ink.tlimit = 3.0;			/* default ink limit of 300% */
		p->ink.klimit = -1.0;			/* default no black limit */
		p->ink.KonlyLmin = 0;			/* default don't use K only black as Locus Lmin */
		p->ink.k_rule = icxKluma5;		/* default K generation rule */
		p->ink.c.Ksmth = ICXINKDEFSMTH;	/* Default smoothing */
		p->ink.c.Kskew = ICXINKDEFSKEW;	/* Default shape skew */
		p->ink.c.Kstle = 0.0;		/* Min K at white end */
		p->ink.c.Kstpo = 0.0;		/* Start of transition is at white */
		p->ink.c.Kenle = 1.0;		/* Max K at black end */
		p->ink.c.Kenpo = 1.0;		/* End transition at black */
		p->ink.c.Kshap = 1.0;		/* Linear transition */
	}

	/* Normalise total and black ink limits */
    if (p->ink.tlimit <= 1e-4 || p->ink.tlimit >= (double)devchan)
    	p->ink.tlimit = -1.0;		/* Turn ink limit off if not effective */
    if (devchan < 4 || p->ink.klimit < 0.0 || p->ink.klimit >= 1.0)
    	p->ink.klimit = -1.0;		/* Turn black limit off if not effective */
	
	/* Set the ink limit information for any reverse interpolation. */
	/* Calling this will clear the reverse interpolaton cache. */
	p->clutTable->rev_set_limit(
		p->clutTable,		/* this */
		p->ink.tlimit >= 0.0 || p->ink.klimit >= 0.0 ? icxLimitD_void : NULL,
		                    /* Optional input space limit() function. */
		                	/* Function should evaluate in[0..di-1], and return */
		                	/* number that is not to exceed 0.0. NULL if not used. */
		(void *)p,			/* Context passed to limit() */
		0.0					/* Value that limit() is not to exceed */
	);

	/* Duplicate in the CAM clip rspl if it exists */
	if (p->cclutTable != NULL) {
		p->cclutTable->rev_set_limit(
			p->cclutTable,		/* this */
			p->ink.tlimit >= 0.0 || p->ink.klimit >= 0.0 ? icxLimitD_void : NULL,
			                    /* Optional input space limit() function. */
			                	/* Function should evaluate in[0..di-1], and return */
			                	/* number that is not to exceed 0.0. NULL if not used. */
			(void *)p,			/* Context passed to limit() */
			0.0					/* Value that limit() is not to exceed */
		);
	}

	/* Figure Lmin and Lmax for icxKluma5 curve basis */
	if (setLminmax
	 && p->clutTable->di > p->clutTable->fdi) {	/* If K generation makes sense */
		double wh[3], bk[3], kk[3];
		int mergeclut;			/* Save/restore mergeclut value */

		/* Get white/black in effective xlu PCS space */
		p->efv_wh_bk_points((icxLuBase *)p, wh, bk, kk);

		/* Convert from effective PCS (ie. Jab) to native XYZ or Lab PCS */
		mergeclut = p->mergeclut;			/* Hack to be able to use inv_out_abs() */
		p->mergeclut = 0;					/* if mergeclut is active. */
		icxLuLut_inv_out_abs(p, wh, wh);
		icxLuLut_inv_out_abs(p, bk, bk);
		icxLuLut_inv_out_abs(p, kk, kk);
		p->mergeclut = mergeclut;			/* Restore */

		/* Convert to Lab PCS */
		if (p->natos == icSigXYZData) {	/* Always do K rule in L space */
			icmXYZ2Lab(&icmD50, wh, wh);
			icmXYZ2Lab(&icmD50, bk, bk);
			icmXYZ2Lab(&icmD50, kk, kk);
		}
		p->Lmax = 0.01 * wh[0];
		if (p->ink.KonlyLmin != 0)
			p->Lmin = 0.01 * kk[0];
		else
			p->Lmin = 0.01 * bk[0];
	} else {

		/* Some sane defaults */
		p->Lmax = 1.0;
		p->Lmin = 0.0;
	}

	return 0;
}

	
/* Initialise the clut clipping information, ink limiting */
/* and auxiliary parameter settings for all the rspl. */
/* return 0 or error code */
static int
setup_clip_icxLuLut(
icxLuLut *p			/* Object being initialised */
) {
	double tmin[MXDIDO], tmax[MXDIDO]; 
	int i;

	/* Setup for inversion of multi-dim clut */

	p->kch = -1;		/* Not known yet */

	/* Set auxiliaries */
	for (i = 0; i < p->inputChan; i++)
		p->auxm[i] = 0;

	if (p->inputChan > p->outputChan) {
		switch(p->natis) {
			case icSigCmykData:
									/* Should fix icm/xicc to remember K channel */
				p->auxm[3] = 1;		/* K is the auxiliary channel */
				break;
			default:
				if (p->kch >= 0)	/* It's been discovered */
					p->auxm[p->kch] = 1;
				else {
					p->pp->errc = 2;
					sprintf(p->pp->err,"Unknown colorspace %s when setting auxliaries",
					                icm2str(icmColorSpaceSignature, p->natis));
					return p->pp->errc;
				}
				break;
		}
	}

//	p->auxlinf = NULL;		/* Input space auxiliary linearisation function  - not implemented */
//	p->auxlinf_ctx = NULL;	/* Opaque context for auxliliary linearisation function */

	/* Aproximate center of clut input gamut - used for */
	/* resolving multiple reverse solutions. */
	p->clutTable->get_in_range(p->clutTable, tmin, tmax);
	for (i = 0; i < p->clutTable->di; i++) {
		p->licent[i] = p->icent[i] = (tmin[i] + tmax[i])/2.0;
	}

	/* Compute clip setup information relating to clut output gamut. */
	if (p->nearclip != 0			/* Near clip requested */
	 || p->inputChan == 1) {		/* or vector clip won't work */
		p->clip.nearclip = 1;

	} else {		/* Vector clip */
		/* !!!! NOTE NOTE NOTE !!!! */
		/* We should re-write this to avoid calling p->clutTable->rev_interp(), */
		/* since this sets up all the rev acceleration tables for two calls, */
		/* if this lut is being setup from scattered data, and never used */
		/* for rev lookup */
		icColorSpaceSignature clutos = p->natos;

		fprintf(stderr, "!!!!! setup_clip_icxLuLut with vector clip - possibly unnecessary rev setup !!!!\n");
		p->clip.nearclip = 0;
		p->clip.LabLike = 0;
		p->clip.fdi = p->clutTable->fdi;

		switch(clutos) {
			case icxSigJabData:
			case icSigLabData: {
				co pp;				/* Room for all the solutions found */
				int nsoln;			/* Number of solutions found */
				double cdir[MXDO];	/* Clip vector direction and length */

				p->clip.LabLike = 1;

				/* Find high clip target */
				for (i = 0; i < p->inputChan; i++)
					pp.p[i] = 0.0;					/* Set aux values */
				pp.v[0] = 105.0; pp.v[1] = pp.v[2] = 0.0; 	/* PCS Target value */
				cdir[0] = cdir[1] = cdir[2] = 0.0;	/* Clip Target */

				p->inv_output(p, pp.v, pp.v);				/* Compensate for output curve */
				p->inv_output(p, cdir, cdir);
			
				cdir[0] -= pp.v[0];							/* Clip vector */
				cdir[1] -= pp.v[1];
				cdir[2] -= pp.v[2];

				/* PCS -> Device with clipping */
				nsoln = p->clutTable->rev_interp(
					p->clutTable, 	/* rspl object */
					0,				/* No hint flags - might be in gamut, might vector clip */
					1,			 	/* Maxumum solutions to return */
					p->auxm, 		/* Auxiliary input targets */
					cdir,			/* Clip vector direction and length */
					&pp);			/* Input target and output solutions */
									/* returned solutions in pp[0..retval-1].p[] */
				nsoln &= RSPL_NOSOLNS;	/* Get number of solutions */

				if (nsoln != 1) {
					p->pp->errc = 2;
					sprintf(p->pp->err,"Failed to find high clip target for Lab space");
					return p->pp->errc;
				}

				p->clip.ocent[0] = pp.v[0] - 0.001;					/* Got main target */
				p->clip.ocent[1] = pp.v[1];
				p->clip.ocent[2] = pp.v[2];

				/* Find low clip target */
				pp.v[0] = -5.0; pp.v[1] = pp.v[2] = 0.0; 	/* PCS Target value */
				cdir[0] = 100.0; cdir[1] = cdir[2] = 0.0;	/* Clip Target */

				p->inv_output(p, pp.v, pp.v);				/* Compensate for output curve */
				p->inv_output(p, cdir, cdir);
				cdir[0] -= pp.v[0];							/* Clip vector */
				cdir[1] -= pp.v[1];
				cdir[2] -= pp.v[2];

				/* PCS -> Device with clipping */
				nsoln = p->clutTable->rev_interp(
					p->clutTable, 	/* rspl object */
					RSPL_WILLCLIP,	/* Since there was no locus, we expect to have to clip */
					1,			 	/* Maxumum solutions to return */
					NULL, 			/* No auxiliary input targets */
					cdir,			/* Clip vector direction and length */
					&pp);			/* Input target and output solutions */
									/* returned solutions in pp[0..retval-1].p[] */
				nsoln &= RSPL_NOSOLNS;		/* Get number of solutions */
				if (nsoln != 1) {
					p->pp->errc = 2;
					sprintf(p->pp->err,"Failed to find low clip target for Lab space");
					return p->pp->errc;
				}

				p->clip.ocentv[0] = pp.v[0] + 0.001 - p->clip.ocent[0];	/* Raw line vector */
				p->clip.ocentv[1] = pp.v[1] - p->clip.ocent[1];
				p->clip.ocentv[2] = pp.v[2] - p->clip.ocent[2];

				/* Compute vectors length */
				for (p->clip.ocentl = 0.0, i = 0; i < 3; i++)
					p->clip.ocentl += p->clip.ocentv[i] * p->clip.ocentv[i];
				p->clip.ocentl = sqrt(p->clip.ocentl);
				if (p->clip.ocentl <= 1e-8)
					p->clip.ocentl = 0.0;

				break;
				}
			case icSigXYZData:
				// ~~~~~~1 need to add this.

			default:
				/* Do a crude approximation, that may not work. */
				p->clutTable->get_out_range(p->clutTable, tmin, tmax);
				for (i = 0; i < p->clutTable->fdi; i++) {
					p->clip.ocent[i] = (tmin[i] + tmax[i])/2.0;
				}
				p->clip.ocentl = 0.0;
				break;
		}
	}
	return 0;
}

/* Function to pass to rspl to set secondary input/output transfer functions */
static void
icxLuLut_inout_func(
	void *pp,			/* icxLuLut */
	double *out,		/* output value */
	double *in			/* inut value */
) {
	icxLuLut *p      = (icxLuLut *)pp;			/* this */
	icmLuLut *luluto = (icmLuLut *)p->plu;		/* Get icmLuLut object */
	double tin[MAX_CHAN];
	double tout[MAX_CHAN];
	int i;

	if (p->iol_out == 0) {			/* fwd input */
#ifdef INK_LIMIT_TEST
		tout[p->iol_ch] = in[0];
#else
		for (i = 0; i < p->inputChan; i++)
			tin[i] = 0.0;
		tin[p->iol_ch] = in[0];
		luluto->input(luluto, tout, tin);
#endif
	} else if (p->iol_out == 1) {	/* fwd output */
		for (i = 0; i < p->outputChan; i++)
			tin[i] = 0.0;
		tin[p->iol_ch] = in[0];
		luluto->output(luluto, tout, tin);
	} else {						/* bwd input */
#ifdef INK_LIMIT_TEST
		tout[p->iol_ch] = in[0];
#else
		for (i = 0; i < p->inputChan; i++)
			tin[i] = 0.0;
		tin[p->iol_ch] = in[0];
		luluto->inv_input(luluto, tout, tin);
		/* This won't be valid if matrix is used or there is a PCS conversion !!! */
		/* Shouldn't be a problem because this is only used for inverting dev->PCS ? */
		luluto->inv_in_abs(luluto, tout, tout);
#endif
	}
	out[0] = tout[p->iol_ch];
}

/* Function to pass to rspl to set clut up, when mergeclut is set */
static void
icxLuLut_clut_merge_func(
	void *pp,			/* icxLuLut */
	double *out,		/* output value */
	double *in			/* input value */
) {
	icxLuLut *p      = (icxLuLut *)pp;			/* this */
	icmLuLut *luluto = (icmLuLut *)p->plu;		/* Get icmLuLut object */

	luluto->clut(luluto, out, in);
	luluto->output(luluto, out, out);
	luluto->out_abs(luluto, out, out);

	if (p->outs == icxSigJabData) {
		p->cam->XYZ_to_cam(p->cam, out, out);
	}
}

/* Implimenation of Lut create from icc. */
/* Note that xicc_get_luobj() will have set the pcsor & */
/* intent to consistent values if Jab and/or icxAppearance */
/* has been requested. */
/* It will also have created the underlying icm lookup object */
/* that is used to create and implement the icx one. The icm */
/* will be used to translate from native to effective PCS, unless */
/* the effective PCS is Jab, in which case the icm will be set to */
/* have an effective PCS of XYZ. Since native<->effecive PCS conversion */
/* is done at the to/from_abs() stage, none of it affects the individual */
/* conversion steps, which will all talk the native PCS (unless merged). */
static icxLuBase *
new_icxLuLut(
xicc                  *xicp,
int                   flags,		/* clip, merge flags */
icmLuBase             *plu,			/* Pointer to Lu we are expanding (ours) */
icmLookupFunc         func,			/* Functionality requested */
icRenderingIntent     intent,		/* Rendering intent */
icColorSpaceSignature pcsor,		/* PCS override (0 = def) */
icxViewCond           *vc,			/* Viewing Condition (NULL if not using CAM) */
icxInk                *ink			/* inking details (NULL for default) */
) {
	icxLuLut *p;						/* Object being created */
	icmLuLut *luluto = (icmLuLut *)plu;	/* Lookup Lut type object */

	int i;

	/* Do basic creation and initialisation */
	if ((p = alloc_icxLuLut(xicp, plu, flags)) == NULL)
		return NULL;

	p->func = func;

	/* Set LuLut "use" specific creation flags: */
	if (flags & ICX_CLIP_NEAREST)
		p->nearclip = 1;

	if (flags & ICX_MERGE_CLUT)
		p->mergeclut = 1;

	if (flags & ICX_FAST_SETUP)
		p->fastsetup = 1;

	/* We're only implementing this under specific conditions. */
	if (flags & ICX_CAM_CLIP
	 && func == icmFwd
	 && !(p->mergeclut != 0 && pcsor == icxSigJabData))		/* Don't need camclip if merged Jab */
		p->camclip = 1;

	if (flags & ICX_INT_SEPARATE) {
fprintf(stderr,"~1 Internal optimised 4D separations not yet implemented!\n");
		p->intsep = 1;
	}

	/* Init the CAM model if it will be used */
	if (pcsor == icxSigJabData || p->camclip) {
		if (vc != NULL)		/* One has been provided */
			p->vc  = *vc;		/* Copy the structure */
		else
			xicc_enum_viewcond(xicp, &p->vc, -1, NULL, 0, NULL);	/* Use a default */
		p->cam = new_icxcam(cam_default);
		p->cam->set_view(p->cam, p->vc.Ev, p->vc.Wxyz, p->vc.La, p->vc.Yb, p->vc.Lv,
		                 p->vc.Yf, p->vc.Yg, p->vc.Gxyz, XICC_USE_HK);
	} else {
		p->cam = NULL;
	}
	
	/* Remember the effective intent */
	p->intent = intent;

	/* Get the effective spaces of underlying icm */
	plu->spaces(plu, &p->ins, NULL, &p->outs, NULL, NULL, NULL, NULL, &p->pcs, NULL);

	/* Override with pcsor */
	/* We assume that any profile that has a CIE color as a "device" color */
	/* intends it to stay that way, and not be overridden. */
	if (pcsor == icxSigJabData) {
		p->pcs = pcsor;		

		if (xicp->pp->header->deviceClass == icSigAbstractClass) {
				p->ins = pcsor;
				p->outs = pcsor;

		} else if (xicp->pp->header->deviceClass != icSigLinkClass) {
			if (func == icmBwd || func == icmGamut || func == icmPreview)
				p->ins = pcsor;
			if (func == icmFwd || func == icmPreview)
				p->outs = pcsor;
		}
	}

	/* In general the native and effective ranges of the icx will be the same as the */
	/* underlying icm lookup object. */
	p->plu->get_lutranges(p->plu, p->ninmin, p->ninmax, p->noutmin, p->noutmax);
	p->plu->get_ranges(p->plu, p->inmin,  p->inmax,  p->outmin,  p->outmax);

	/* If we have a Jab PCS override, reflect this in the effective icx range. */
	/* Note that the ab ranges are nominal. They will exceed this range */
	/* for colors representable in L*a*b* PCS */
	if (p->ins == icxSigJabData) {
		p->inmin[0] = 0.0;		p->inmax[0] = 100.0;
		p->inmin[1] = -128.0;	p->inmax[1] = 128.0;
		p->inmin[2] = -128.0;	p->inmax[2] = 128.0;
	} else if (p->outs == icxSigJabData) {
		p->outmin[0] = 0.0;		p->outmax[0] = 100.0;
		p->outmin[1] = -128.0;	p->outmax[1] = 128.0;
		p->outmin[2] = -128.0;	p->outmax[2] = 128.0;
	} 

	/* If we have a merged clut, reflect this in the icx native PCS range. */
	/* Merging merges output processing (irrespective of whether we are using */
	/* the forward or backward cluts) */
	if (p->mergeclut != 0) {
		int i;
		for (i = 0; i < p->outputChan; i++) {
			p->noutmin[i] = p->outmin[i];
			p->noutmax[i] = p->outmax[i];
		}
	}

	/* ------------------------------- */
	/* Create rspl based input lookups */
	for (i = 0; i < p->inputChan; i++) {
		if ((p->inputTable[i] = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
			p->pp->errc = 2;
			sprintf(p->pp->err,"Creation of input table rspl failed");
			p->del((icxLuBase *)p);
			return NULL;
		}
		p->iol_out = 0;		/* Input lookup */
		p->iol_ch = i;		/* Chanel */
		p->inputTable[i]->set_rspl(p->inputTable[i], RSPL_NOFLAGS,
		           (void *)p, icxLuLut_inout_func,
		           &p->ninmin[i], &p->ninmax[i], (int *)&p->lut->inputEnt, &p->ninmin[i], &p->ninmax[i]);
	}

	/* Setup center clip target for inversion */
	for (i = 0; i < p->inputChan; i++) {
		p->inputClipc[i] = (p->ninmin[i] + p->ninmax[i])/2.0;
	}

	/* Create rspl based reverse input lookups used in ink limit and locus range functions. */
	for (i = 0; i < p->inputChan; i++) {
		int gres = p->inputTable[i]->g.mres;	/* Same res as curve being inverted */
		if (gres < 256)
			gres = 256;
		if ((p->revinputTable[i] = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
			p->pp->errc = 2;
			sprintf(p->pp->err,"Creation of reverse input table rspl failed");
			p->del((icxLuBase *)p);
			return NULL;
		}
		p->iol_out = 2;		/* Input lookup */
		p->iol_ch = i;		/* Chanel */
		p->revinputTable[i]->set_rspl(p->revinputTable[i], RSPL_NOFLAGS,
		           (void *)p, icxLuLut_inout_func,
		           &p->ninmin[i], &p->ninmax[i], &gres, &p->ninmin[i], &p->ninmax[i]);
	}

	/* ------------------------------- */
	{ 
		int gres[MXDI];

		for (i = 0; i < p->inputChan; i++)
			gres[i] = p->lut->clutPoints;

		/* Create rspl based multi-dim table */
		if ((p->clutTable = new_rspl((p->fastsetup ? RSPL_FASTREVSETUP : RSPL_NOFLAGS)
		                             | (flags & ICX_VERBOSE ? RSPL_VERBOSE : RSPL_NOFLAGS),
		                             p->inputChan, p->outputChan)) == NULL) {
			p->pp->errc = 2;
			sprintf(p->pp->err,"Creation of clut table rspl failed");
			p->del((icxLuBase *)p);
			return NULL;
		}

		if (p->mergeclut == 0) {	/* Do this if it's not merged with clut, */
			p->clutTable->set_rspl(p->clutTable, RSPL_NOFLAGS,
			           (void *)luluto, (void (*)(void *, double *, double *))luluto->clut,
		               p->ninmin, p->ninmax, gres, p->noutmin, p->noutmax);

		} else {	/* If mergeclut */
			p->clutTable->set_rspl(p->clutTable, RSPL_NOFLAGS,
			           (void *)p, icxLuLut_clut_merge_func,
		               p->ninmin, p->ninmax, gres, p->noutmin, p->noutmax);

		}

		/* clut clipping is setup separately */
	}

	/* ------------------------------- */
	/* Create rspl based output lookups */
	for (i = 0; i < p->outputChan; i++) {
		if ((p->outputTable[i] = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
			p->pp->errc = 2;
			sprintf(p->pp->err,"Creation of output table rspl failed");
			p->del((icxLuBase *)p);
			return NULL;
		}
		p->iol_out = 1;		/* Output lookup */
		p->iol_ch = i;		/* Chanel */
		p->outputTable[i]->set_rspl(p->outputTable[i], RSPL_NOFLAGS,
		           (void *)p, icxLuLut_inout_func,
		           &p->noutmin[i], &p->noutmax[i], (int *)&p->lut->outputEnt, &p->noutmin[i], &p->noutmax[i]);
	}

	/* Setup center clip target for inversion */
	for (i = 0; i < p->outputChan; i++) {
		p->outputClipc[i] = (p->noutmin[i] + p->noutmax[i])/2.0;
	}

	/* ------------------------------- */

	/* Setup all the clipping, ink limiting and auxiliary stuff, */
	/* in case a reverse call is used. Only do this if we know */
	/* the reverse stuff isn't going to fail due to channel limits. */
	if (p->clutTable->within_restrictedsize(p->clutTable)) {

		if (setup_ink_icxLuLut(p, ink, 1) != 0) {
			p->del((icxLuBase *)p);
			return NULL;
		}
	
		if (setup_clip_icxLuLut(p) != 0) {
			p->del((icxLuBase *)p);
			return NULL;
		}
	}

	return (icxLuBase *)p;
}


/* Function to pass to rspl to set clut up, when camclip is going to be used. */
/* We use the temporary icm fwd absolute xyz lookup, then convert to CAM Jab. */
static void
icxLuLut_clut_camclip_func(
	void *pp,			/* icxLuLut */
	double *out,		/* output value */
	double *in			/* inut value */
) {
	icxLuLut *p      = (icxLuLut *)pp;			/* this */
	icmLuLut *luluto = (icmLuLut *)p->absxyzlu;

	luluto->clut(luluto, out, in);
	luluto->output(luluto, out, out);
	luluto->out_abs(luluto, out, out);
	p->cam->XYZ_to_cam(p->cam, out, out);
	out[0] *= CCJSCALE;
}

/* Initialise the additional CAM space clut rspl, used to compute */
/* reverse lookup CAM clipping results when the camclip flag is set. */
/* We scale J by CCJSCALE, to give a more L* preserving clip direction */
/* return error code. */
static int
icxLuLut_init_clut_camclip(
icxLuLut *p) {
	int e, gres[MXDI];

	/* Setup so clut contains transform to CAM Jab */
	/* (camclip is only used in fwd or invfwd direction lookup) */
	double cmin[3], cmax[3];
	cmin[0] = 0.0;		cmax[0] = CCJSCALE * 100.0;	/* Nominal Jab output ranges */
	cmin[1] = -128.0;	cmax[1] = 128.0;
	cmin[2] = -128.0;	cmax[2] = 128.0;

	/* Get icm lookup we need for seting up and using CAM icx clut */
	if ((p->absxyzlu = p->pp->pp->get_luobj(p->pp->pp, icmFwd, icAbsoluteColorimetric,
	                                    icSigXYZData, icmLuOrdNorm)) == NULL) {
		p->pp->errc = p->pp->pp->errc;		/* Copy error to xicc */
		strcpy(p->pp->err, p->pp->pp->err);
		return p->pp->errc;
	}

	/* Create CAM rspl based multi-dim table */
	if ((p->cclutTable = new_rspl((p->fastsetup ? RSPL_FASTREVSETUP : RSPL_NOFLAGS)
		                          | (p->flags & ICX_VERBOSE ? RSPL_VERBOSE : RSPL_NOFLAGS),
	                              p->inputChan, p->outputChan)) == NULL) {
		p->pp->errc = 2;
		sprintf(p->pp->err,"Creation of clut table rspl failed");
		return p->pp->errc;
	}

	for (e = 0; e < p->inputChan; e++)
		gres[e] = p->lut->clutPoints;

	/* Setup our special CAM space rspl */
	p->cclutTable->set_rspl(p->cclutTable, RSPL_NOFLAGS, (void *)p,
	           icxLuLut_clut_camclip_func,
               p->ninmin, p->ninmax, gres, cmin, cmax);

	/* Duplicate the ink limit information for any reverse interpolation. */
	p->cclutTable->rev_set_limit(
		p->cclutTable,		/* this */
		p->ink.tlimit >= 0.0 || p->ink.klimit >= 0.0 ? icxLimitD_void : NULL,
		                    /* Optional input space limit() function. */
		                	/* Function should evaluate in[0..di-1], and return */
		                	/* number that is not to exceed 0.0. NULL if not used. */
		(void *)p,			/* Context passed to limit() */
		0.0					/* Value that limit() is not to exceed */
	);
	return 0;
}

/* ========================================================== */
/* xicc creation code                                         */
/* ========================================================== */

/* Callback for computing delta E squared for two output (PCS) */
/* values, passed as a callback to xfit */
static double xfit_to_de2(void *cntx, double *in1, double *in2) {
	icxLuLut *p = (icxLuLut *)cntx;
	double rv;

	if (p->pcs == icSigLabData) {
#ifdef USE_CIE94_DE
		rv = icmCIE94sq(in1, in2);
#else
		rv = icmLabDEsq(in1, in2);
#endif
	} else {
		double lab1[3], lab2[3];
		icmXYZ2Lab(&icmD50, lab1, in1);
		icmXYZ2Lab(&icmD50, lab2, in2);
#ifdef USE_CIE94_DE
		rv = icmCIE94sq(lab1, lab2);
#else
		rv = icmLabDEsq(lab1, lab2);
#endif
	}
	return rv;
}

/* Same as above plus partial derivatives */
static double xfit_to_dde2(void *cntx, double dout[2][MXDIDO], double *in1, double *in2) {
	icxLuLut *p = (icxLuLut *)cntx;
	double rv;

	if (p->pcs == icSigLabData) {
		int j,k;
		double tdout[2][3];
#ifdef USE_CIE94_DE
		rv = icxdCIE94sq(tdout, in1, in2);
#else
		rv = icxdLabDEsq(tdout, in1, in2);
#endif
		for (k = 0; k < 2; k++) {
			for (j = 0; j < 3; j++)
				dout[k][j] = tdout[k][j];
		}
	} else {
		double lab1[3], lab2[3];
		double dout12[2][3][3];
		double tdout[2][3];
		int i,j,k;

		icxdXYZ2Lab(&icmD50, lab1, dout12[0], in1);
		icxdXYZ2Lab(&icmD50, lab2, dout12[1], in2);
#ifdef USE_CIE94_DE
		rv = icxdCIE94sq(tdout, lab1, lab2);
#else
		rv = icxdLabDEsq(tdout, lab1, lab2);
#endif
		/* Compute partial derivative (is this correct ??) */
		for (k = 0; k < 2; k++) {
			for (j = 0; j < 3; j++) {
				dout[k][j] = 0.0;
				for (i = 0; i < 3; i++) {
					dout[k][j] += tdout[k][i] * dout12[k][i][j];
				}
			}
		}
	}
	return rv;
}

#ifdef NEVER
/* Check partial derivative function within xfit_to_dde2() */

static double _xfit_to_dde2(void *cntx, double dout[2][MXDIDO], double *in1, double *in2) {
	icxLuLut *pp = (icxLuLut *)cntx;
	int k, i;
	double rv, drv;
	double trv;
	
	rv = xfit_to_de2(cntx, in1, in2);
	drv = xfit_to_dde2(cntx, dout, in1, in2);

	if (fabs(rv - drv) > 1e-6)
		printf("######## DDE2: RV MISMATCH is %f should be %f ########\n",rv,drv);

	/* Check each parameter delta */
	for (k = 0; k < 2; k++) {
		for (i = 0; i < 3; i++) {
			double *in;
			double del;
	
			if (k == 0)
				in = in1;
			else
				in = in2;

			in[i] += 1e-9;
			trv = xfit_to_de2(cntx, in1, in2);
			in[i] -= 1e-9;
			
			/* Check that del is correct */
			del = (trv - rv)/1e-9;
			if (fabs(dout[k][i] - del) > 0.04) {
				printf("######## DDE2: EXCESSIVE at in[%d][%d] is %f should be %f ########\n",k,i,dout[k][i],del);
			}
		}
	}
	return rv;
}

#define xfit_to_dde2 _xfit_to_dde2

#endif

/* Context for rspl setting input and output curves */
typedef struct {
	int iix;
	int oix;
	xfit *xf;		/* Optimisation structure */
} curvectx;

/* Function to pass to rspl to set input and output */
/* transfer function for xicc lookup function */
static void
set_linfunc(
	void *cc,			/* curvectx structure */
	double *out,		/* Device output value */
	double *in			/* Device input value */
) {
	curvectx *c = (curvectx *)cc;		/* this */
	xfit *p = c->xf;

	if (c->iix >= 0) {				/* Input curve */
		*out = p->incurve(p, *in, c->iix);
	} else if (c->oix >= 0) {		/* Output curve */
		*out = p->outcurve(p, *in, c->oix);
	}
}

/* Function to pass to rspl to set inverse input transfer function, */
/* used for ink limiting calculation. */
static void
icxLuLut_invinput_func(
	void *cc,			/* curvectx structure */
	double *out,		/* Device output value */
	double *in			/* Device input value */
) {
	curvectx *c = (curvectx *)cc;		/* this */
	xfit *p = c->xf;

	*out = p->invincurve(p, *in, c->iix);
}


/* Functions to pass to icc settables() to setup icc A2B Lut: */

/* Input table */
static void set_input(void *cntx, double *out, double *in) {
	icxLuLut *p = (icxLuLut *)cntx;

	if (p->noisluts != 0 && p->noipluts != 0) {	/* Input table must be linear */
		int i;
		for (i = 0; i < p->inputChan; i++)
			out[i] = in[i];
	} else {
		if (p->input(p, out, in) > 1)
			error ("%d, %s",p->pp->errc,p->pp->err);
	}
}

/* clut */
static void set_clut(void *cntx, double *out, double *in) {
	icxLuLut *p = (icxLuLut *)cntx;
	int f;

	if (p->clut(p, out, in) > 1)
		error ("%d, %s",p->pp->errc,p->pp->err);

	/* Convert from efective pcs to natural pcs */
	if (p->pcs != p->plu->icp->header->pcs) {
		if (p->pcs == icSigLabData)
			icmLab2XYZ(&icmD50, out, out);
		else
			icmXYZ2Lab(&icmD50, out, out);
	}
}

/* output */
static void set_output(void *cntx, double *out, double *in) {
	icxLuLut *p = (icxLuLut *)cntx;

	if (p->nooluts != 0) {	/* Output table must be linear */
		int i;
		for (i = 0; i < p->outputChan; i++)
			out[i] = in[i];
	} else {
		if (p->output(p, out, in) > 1)
			error ("%d, %s",p->pp->errc,p->pp->err);
	}
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Routine to figure out a suitable black point for CMYK */

/* Structure to hold optimisation information */
typedef struct {
	icxLuLut *p;			/* Object being created */
	double toAbs[3][3];		/* To abs from aprox relative */
	double p1[3];			/* white pivot point in abs Lab */
	double p2[3];			/* Point on vector towards black */
	double toll;			/* Tollerance of black direction */
} bfinds;

/* Optimise device values to minimise L, while remaining */
/* within the ink limit, and staying in line between p1 (white) and p2 (black dir) */
static double bfindfunc(void *adata, double pv[]) {
	bfinds *b = (bfinds *)adata;
	double rv = 0.0;
	double tt[3], Lab[3];
	co bcc;
	double lr, ta, tb, terr;	/* L ratio, target a, target b, target error */
	double ovr;

//printf("~1 bfindfunc got %f %f %f %f\n", pv[0], pv[1], pv[2], pv[3]);
	/* See if over ink limit or outside device range */
	ovr = icxLimit(b->p, pv);		/* > 0.0 if outside device gamut or ink limit */
	if (ovr < 0.0)
		ovr = 0.0;
//printf("~1 bfindfunc got ovr %f\n", ovr);

	/* Compute the absolute Lab value: */
	b->p->input(b->p, bcc.p, pv);						/* Through input tables */
	b->p->clutTable->interp(b->p->clutTable, &bcc);		/* Through clut */
	b->p->output(b->p, bcc.v, bcc.v);					/* Through the output tables */

	if (b->p->pcs != icSigXYZData) 	/* Convert PCS to XYZ */
		icmLab2XYZ(&icmD50, bcc.v, bcc.v);

	/* Convert from relative to Absolute colorimetric */
	icmMulBy3x3(tt, b->toAbs, bcc.v);
	icmXYZ2Lab(&icmD50, Lab, tt);	/* Convert to Lab */

#ifdef DEBUG
printf("~1 p1 =  %f %f %f, p2 = %f %f %f\n",b->p1[0],b->p1[1],b->p1[2],b->p2[0],b->p2[1],b->p2[2]);
printf("~1 device value %f %f %f %f, Lab = %f %f %f\n",pv[0],pv[1],pv[2],pv[3],Lab[0],Lab[1],Lab[2]);
#endif

	/* Primary aim is to minimise L value */
	rv = Lab[0];

	/* See how out of line from p1 to p2 we are */
	lr = (Lab[0] - b->p1[0])/(b->p2[0] - b->p1[0]);		/* Distance towards p2 from p1 */
	ta = lr * (b->p2[1] - b->p1[1]) + b->p1[1];			/* Target a value */
	tb = lr * (b->p2[2] - b->p1[2]) + b->p1[2];			/* Target b value */

	terr = (ta - Lab[1]) * (ta - Lab[1])
	     + (tb - Lab[2]) * (tb - Lab[2]);

	if (terr < b->toll)		/* Tollerance error doesn't count until it's over tollerance */
		terr = 0.0;
	
#ifdef DEBUG
printf("~1 target error %f\n",terr);
#endif
	rv += XICC_BLACK_FIND_ABERR_WEIGHT * terr;		/* Make ab match  more important than min. L */

#ifdef DEBUG
printf("~1 out of range error %f\n",ovr);
#endif
	rv += 200 * ovr;

#ifdef DEBUG
printf("~1 black find tc ret %f\n",rv);
#endif
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Create icxLuLut and underlying fwd Lut from scattered data */
/* The scattered data is assumed to map Device -> native PCS */
/* NOTE:- in theory once this icxLuLut is setup, it can be */
/* called to translate color values. In practice I suspect */
/* that the icxLuLut hasn't been setup completely enough to allows this. */
/* Might be easier to close it and re-open it ? */
static icxLuBase *
set_icxLuLut(
xicc               *xicp,
icmLuBase          *plu,			/* Pointer to Lu we are expanding (ours) */	
icmLookupFunc      func,			/* Functionality requested */
icRenderingIntent  intent,			/* Rendering intent */
int                flags,			/* white/black point flags etc. */
int                nodp,			/* Number of points */
int                nodpbw,			/* Number of points to look for white & black patches in */
cow                *ipoints,		/* Array of input points (Lab or XYZ normalized to 1.0) */
icxMatrixModel     *skm,    		/* Optional skeleton model (used for input profiles) */
double             dispLuminance,	/* > 0.0 if display luminance value and is known */
double             wpscale,			/* > 0.0 if white point is to be scaled */
//double            *bpo,				/* != NULL for XYZ black point override dev & XYZ */
double             smooth,			/* RSPL smoothing factor, -ve if raw */
double             avgdev,			/* reading Average Deviation as a prop. of the input range */
double             demph,			/* dark emphasis factor for cLUT grid res. */
icxViewCond        *vc,				/* Viewing Condition (NULL if not using CAM) */
icxInk             *ink,			/* inking details (NULL for default) */
int                quality			/* Quality metric, 0..3 */
) {
	icxLuLut *p;						/* Object being created */
	icc *icco = xicp->pp;				/* Underlying icc object */
	int luflags = 0;					/* icxLuLut alloc clip, merge flags */
	int pcsy;							/* Effective PCS L or Y chanel index */
	double pcsymax;						/* Effective PCS L or Y maximum value */
	icmHeader *h = icco->header;		/* Pointer to icc header */
	int maxchan;						/* max(inputChan, outputChan) */
	int rsplflags = RSPL_NOFLAGS;		/* Flags for scattered data rspl */
	int e, f, i, j;
	double dwhite[MXDI], dblack[MXDI];	/* Device white and black values */
	double wp[3];			/* Absolute White point in XYZ */
	double bp[3];			/* Absolute Black point in XYZ */
	double dgwhite[MXDI];	/* Device space gamut boundary white (ie. RGB 1,1,1) */
	double oavgdev[MXDO];	/* Average output value deviation */
	int gres[MXDI];			/* RSPL/CLUT resolution */
	xfit *xf = NULL;		/* Curve fitting class instance */
//	co bpop;				/* bpo dev + XYZ value */

	if (flags & ICX_VERBOSE)
		rsplflags |= RSPL_VERBOSE;

//	if (flags & ICX_2PASSSMTH)
//		rsplflags |= RSPL_2PASSSMTH;	/* Smooth data using Gaussian */

//	if (flags & ICX_EXTRA_FIT)
//		rsplflags |= RSPL_EXTRAFIT2;	/* Try extra hard to fit data */

	luflags = flags;		/* Transfer straight though ? */

	/* Do basic creation and initialisation */
	if ((p = alloc_icxLuLut(xicp, plu, luflags)) == NULL)
		return NULL;

	p->func = func;

	/* Set LuLut "use" specific creation flags: */
	if (flags & ICX_CLIP_NEAREST)
		p->nearclip = 1;

	/* Set LuLut "create" specific flags: */
	if (flags & ICX_NO_IN_SHP_LUTS)
		p->noisluts = 1;

	if (flags & ICX_NO_IN_POS_LUTS)
		p->noipluts = 1;

	if (flags & ICX_NO_OUT_LUTS)
		p->nooluts = 1;

	/* Get the effective spaces of underlying icm, and set icx the same */
	plu->spaces(plu, &p->ins, NULL, &p->outs, NULL, NULL, &p->intent, NULL, &p->pcs, NULL);

	/* For set_icx the effective pcs has to be the same as the native pcs */

	if (p->pcs == icSigXYZData) {
		pcsy = 1;	/* Y of XYZ */
		pcsymax = 1.0;
	} else {
		pcsy = 0;	/* L or Lab */
		pcsymax = 100.0;
	}

	maxchan = p->inputChan > p->outputChan ? p->inputChan : p->outputChan;

	/* Translate overall average deviation into output channel deviation */
	/* (This is for rspl scattered data fitting smoothness adjustment) */
	/* (This could do with more tuning) */

	/* XYZ display models are under-smoothed, because the mapping is typically */
	/* very "straight", and the lack of tension reduces any noise reduction effect. */
	/* !!! This probably means that we should switch to 3rd order smoothness criteria !! */
	/* We apply an arbitrary correction here */
	/* !!!! There is also a bug in the rspl code, where smoothness is */
	/* scaled by data range. This is making Lab smoothing ~100 times */
	/* more than XYZ smoothing. Fix this with SMOOTH2 changes ?? */
	if (p->pcs == icSigXYZData) {
		oavgdev[0] = XYZ_EXTRA_SMOOTH * 0.70 * avgdev;
		oavgdev[1] = XYZ_EXTRA_SMOOTH * 1.00 * avgdev;
		oavgdev[2] = XYZ_EXTRA_SMOOTH * 0.70 * avgdev;
	} else if (p->pcs == icSigLabData) {
		oavgdev[0] = 1.00 * avgdev;
		oavgdev[1] = 0.70 * avgdev;
		oavgdev[2] = 0.70 * avgdev;
	} else {	/* Hmmm */
		for (f = 0; f < p->outputChan; f++)
			oavgdev[f] = avgdev;
	}

	/* In general the native and effective ranges of the icx will be the same as the */
	/* underlying icm lookup object. */
	p->plu->get_lutranges(p->plu, p->ninmin, p->ninmax, p->noutmin, p->noutmax);
	p->plu->get_ranges(p->plu, p->inmin,  p->inmax,  p->outmin,  p->outmax);

	/* ??? Does this ever happen with set_icxLuLut() ??? */
	/* If we have a Jab PCS override, reflect this in the effective icx range. */
	/* Note that the ab ranges are nominal. They will exceed this range */
	/* for colors representable in L*a*b* PCS */
	if (p->ins == icxSigJabData) {
		p->inmin[0] = 0.0;		p->inmax[0] = 100.0;
		p->inmin[1] = -128.0;	p->inmax[1] = 128.0;
		p->inmin[2] = -128.0;	p->inmax[2] = 128.0;
	} else if (p->outs == icxSigJabData) {
		p->outmin[0] = 0.0;		p->outmax[0] = 100.0;
		p->outmin[1] = -128.0;	p->outmax[1] = 128.0;
		p->outmin[2] = -128.0;	p->outmax[2] = 128.0;
	} 

	/* ------------------------------- */

	if (flags & ICX_VERBOSE)
		printf("Estimating white point\n");

	icmXYZ2Ary(wp, icmD50);		/* Set a default value - D50 */
	icmXYZ2Ary(bp, icmBlack);	/* Set a default value - absolute black */

	{
		/* Figure out as best we can the device white and black points */

		/* Compute device white and black points as if */
		/* we are doing an Output or Display device */
		{
			switch (h->colorSpace) {
	
				case icSigCmykData:
					for (e = 0; e < p->inputChan; e++) {
						dwhite[e] = 0.0;
						dblack[e] = 1.0;
					}
					break;
				case icSigCmyData:
					for (e = 0; e < p->inputChan; e++) {
						dwhite[e] = 0.0;
						dblack[e] = 1.0;
					}
					break;
				case icSigRgbData:
					for (e = 0; e < p->inputChan; e++) {
						dwhite[e] = 1.0;
						dblack[e] = 0.0;
					}
					break;
	
				case icSigGrayData: {	/* Could be additive or subtractive */
					double maxY, minY;		/* Y min and max */
					double maxd, mind;		/* Corresponding device values */

					maxY = -1e8, minY = 1e8;

					/* Figure out if it's additive or subtractive */
					for (i = 0; i < nodpbw; i++) {
						double Y;
						if (p->pcs != icSigXYZData) { 	/* Convert white point to XYZ */
							double xyz[3];
							icmLab2XYZ(&icmD50, xyz, ipoints[i].v);
							Y = xyz[1];
						} else {
							Y = ipoints[i].v[1];
						}
						
						if (Y > maxY) {
							maxY = Y;
							maxd = ipoints[i].p[0];
						}
						if (Y < minY) {
							minY = Y;
							mind = ipoints[i].p[0];
						}
					}
					if (maxd < mind) {			/* Subtractive */
						for (e = 0; e < p->inputChan; e++) {
							dwhite[e] = 0.0;
							dblack[e] = 1.0;
						}
					} else {					/* Additive */
						for (e = 0; e < p->inputChan; e++) {
							dwhite[e] = 1.0;
							dblack[e] = 0.0;
						}
					}
					break;
				}
				default:
					xicp->errc = 1;
					sprintf(xicp->err,"set_icxLuLut: can't handle color space %s",
					                           icm2str(icmColorSpaceSignature, h->colorSpace));
					p->del((icxLuBase *)p);
					return NULL;
					break;
			}
		}
		/* dwhite is what we want for dgwhite[], used for XFIT_OUT_WP_REL_US */
		for (e = 0; e < p->inputChan; e++)
			dgwhite[e] = dwhite[e];

		/* If this is actuall an input device, lookup wp & bp */
		/* and override dwhite & dblack */
		if (h->deviceClass == icSigInputClass) {
			double wpy = -1e60, bpy = 1e60;
			int wix = -1, bix = -1;
			/* We assume that the input target is well behaved, */
			/* and that it includes a white and black point patch, */
			/* and that they have the extreme L/Y values */

			/*
				NOTE that this may not be the best approach !
				It may be better to average the chromaticity
				of all the neutral seeming patches, since
				the whitest patch may have (for instance)
				a blue tint.
			 */

			/* Discover the white and black patches */
			for (i = 0; i < nodpbw; i++) {
				double labv[3], yv;

				/* Create D50 Lab to allow some chromatic sensitivity */
				/* in picking the white point */
				if (p->pcs == icSigXYZData)
					icmXYZ2Lab(&icmD50, labv, ipoints[i].v);
				else
					icmCpy3(labv,ipoints[i].v);

#ifdef NEVER
				/* Choose largest Y or L* */
				if (ipoints[i].v[pcsy] > wpy) {
					wp[0] = ipoints[i].v[0];
					wp[1] = ipoints[i].v[1];
					wp[2] = ipoints[i].v[2];
					for (e = 0; e < p->inputChan; e++)
						dwhite[e] = ipoints[i].p[e];
					wpy = ipoints[i].v[pcsy];
					wix = i;
				}
#else
				
				/* Tilt things towards D50 neutral white patches */
				yv = labv[0] - 0.3 * sqrt(labv[1] * labv[1] + labv[2] * labv[2]);
//printf("~1 patch %d Lab = %s, yv = %f\n",i+1,icmPdv(3,labv),yv);
				if (yv > wpy) {
//printf("~1 best so far\n");
					wp[0] = ipoints[i].v[0];
					wp[1] = ipoints[i].v[1];
					wp[2] = ipoints[i].v[2];
					for (e = 0; e < p->inputChan; e++)
						dwhite[e] = ipoints[i].p[e];
					wpy = yv;
					wix = i;
				}
#endif
				if (ipoints[i].v[pcsy] < bpy) {
					bp[0] = ipoints[i].v[0];
					bp[1] = ipoints[i].v[1];
					bp[2] = ipoints[i].v[2];
					for (e = 0; e < p->inputChan; e++)
						dblack[e] = ipoints[i].p[e];
					bpy = ipoints[i].v[pcsy];
					bix = i;
				}
			}
			/* Make sure values are XYZ */
			if (p->pcs != icSigXYZData) {
				icmLab2XYZ(&icmD50, wp, wp);
				icmLab2XYZ(&icmD50, bp, bp);
			}
			
			if (flags & ICX_VERBOSE) {
				printf("Picked white patch %d with dev = %s\n       XYZ = %s, Lab = %s\n",
				        wix+1, icmPdv(p->inputChan, dwhite), icmPdv(3, wp), icmPLab(wp));
				printf("Picked black patch %d with dev = %s\n       XYZ = %s, Lab = %s\n",
				        bix+1, icmPdv(p->inputChan, dblack), icmPdv(3, bp), icmPLab(bp));
			}

		/* else Output or Display device */
		} else {
			/* We assume that the output target is well behaved, */
			/* and that it includes a white point patch. */
			int nw = 0;

			wp[0] = wp[1] = wp[2] = 0.0;

			switch (h->colorSpace) {
	
				case icSigCmykData:
					for (i = 0; i < nodpbw; i++) {
						if (ipoints[i].p[0] < 0.001
						 && ipoints[i].p[1] < 0.001
						 && ipoints[i].p[2] < 0.001
						 && ipoints[i].p[3] < 0.001) {
							wp[0] += ipoints[i].v[0];
							wp[1] += ipoints[i].v[1];
							wp[2] += ipoints[i].v[2];
							nw++;
						}
					}
					break;
				case icSigCmyData:
					for (i = 0; i < nodpbw; i++) {
						if (ipoints[i].p[0] < 0.001
						 && ipoints[i].p[1] < 0.001
						 && ipoints[i].p[2] < 0.001) {
							wp[0] += ipoints[i].v[0];
							wp[1] += ipoints[i].v[1];
							wp[2] += ipoints[i].v[2];
							nw++;
						}
					}
					break;
				case icSigRgbData:
					for (i = 0; i < nodpbw; i++) {
						if (ipoints[i].p[0] > 0.999
						 && ipoints[i].p[1] > 0.999
						 && ipoints[i].p[2] > 0.999) {
							wp[0] += ipoints[i].v[0];
							wp[1] += ipoints[i].v[1];
							wp[2] += ipoints[i].v[2];
							nw++;
						}
					}
					/* Setup bpo device value in case we need it */
//					bpop.p[0] = bpop.p[1] = bpop.p[2] = 0.0;
					break;
	
				case icSigGrayData: {	/* Could be additive or subtractive */
					double minwp[3], maxwp[3];
					int nminwp = 0, nmaxwp = 0;

					minwp[0] = minwp[1] = minwp[2] = 0.0;
					maxwp[0] = maxwp[1] = maxwp[2] = 0.0;

					/* Look for both */
					for (i = 0; i < nodpbw; i++) {
						if (ipoints[i].p[0] < 0.001)
							minwp[0] += ipoints[i].v[0];
							minwp[1] += ipoints[i].v[1];
							minwp[2] += ipoints[i].v[2]; {
							nminwp++;
						}
						if (ipoints[i].p[0] > 0.999)
							maxwp[0] += ipoints[i].v[0];
							maxwp[1] += ipoints[i].v[1];
							maxwp[2] += ipoints[i].v[2]; {
							nmaxwp++;
						}
					}
					if (nminwp > 0) {			/* Subtractive */
						wp[0] = minwp[0];
						wp[1] = minwp[1];
						wp[2] = minwp[2];
						nw = nminwp;
						if (minwp[pcsy]/nminwp < (0.5 * pcsymax))
							nw = 0;					/* Looks like a mistake */
//						bpop.p[0] = 1.0;
					}
					if (nmaxwp > 0				/* Additive */
					 && (nminwp == 0 || maxwp[pcsy]/nmaxwp > minwp[pcsy]/nminwp)) {
						wp[0] = maxwp[0];
						wp[1] = maxwp[1];
						wp[2] = maxwp[2];
						nw = nmaxwp;
						if (maxwp[pcsy]/nmaxwp < (0.5 * pcsymax))
							nw = 0;					/* Looks like a mistake */
//						bpop.p[0] = 0.0;
					}
					break;
				}

				default:
					xicp->errc = 1;
					sprintf(xicp->err,"set_icxLuLut: can't handle color space %s",
					                           icm2str(icmColorSpaceSignature, h->colorSpace));
					p->del((icxLuBase *)p);
					return NULL;
					break;
			}

			if (nw == 0) {
				xicp->errc = 1;
				sprintf(xicp->err,"set_icxLuLut: can't handle test points without a white patch");
				p->del((icxLuBase *)p);
				return NULL;
			}
			wp[0] /= (double)nw;
			wp[1] /= (double)nw;
			wp[2] /= (double)nw;
			if (p->pcs != icSigXYZData) 	/* Convert white point to XYZ */
				icmLab2XYZ(&icmD50, wp, wp);

//			if (bpo != NULL) {			/* Copy black override XYZ value */
//				bpop.v[0] = bpo[0];
//				bpop.v[1] = bpo[1];
//				bpop.v[2] = bpo[2];
//			}
		}

		if (flags & ICX_VERBOSE) {
			printf("Approximate White point XYZ = %s, Lab = %s\n", icmPdv(3,wp),icmPLab(wp));
		}
	}

	if (h->colorSpace == icSigGrayData) {	/* Don't use device or PCS curves for monochrome */
		p->noisluts = p->noipluts = p->nooluts = 1;
	}

	if ((flags & ICX_VERBOSE) && (p->noisluts == 0 || p->noipluts == 0 || p->nooluts == 0))
		printf("Creating optimised per channel curves\n");

	/* Set the target CLUT grid resolution so in/out curves can be optimised for it */
	for (e = 0; e < p->inputChan; e++)
		gres[e] = p->lut->clutPoints;

	/* Setup and then create xfit object that does most of the work */
	{
		int xfflags = 0;		/* xfit flags */
		double in_min[MXDI];	/* Input value scaling minimum */
		double in_max[MXDI];	/* Input value scaling maximum */
		double out_min[MXDO];	/* Output value scaling minimum */
		double out_max[MXDO];	/* Output value scaling maximum */
		int iluord, sluord, oluord;
		int iord[MXDI];			/* Input curve orders */
		int sord[MXDI];			/* Input sub-grid curve orders */
		int oord[MXDO];			/* Output curve orders */
		double shp_smooth[MXDI];/* Smoothing factors for each curve, nom = 1.0 */
		double out_smooth[MXDO];

		optcomb tcomb = oc_ipo;	/* Create all by default */

		if ((xf = new_xfit(icco)) == NULL) {
			p->pp->errc = 2;
			sprintf(p->pp->err,"Creation of xfit object failed");
			p->del((icxLuBase *)p);
			return NULL;
		}
			
		/* Setup for optimising run */
		if (p->noisluts)
			tcomb &= ~oc_i;

		if (p->noipluts)
			tcomb &= ~oc_p;

		if (p->nooluts)
			tcomb &= ~oc_o;

		if (flags & ICX_VERBOSE)
			xfflags |= XFIT_VERB;

		if (flags & ICX_SET_WHITE) {

			xfflags |= XFIT_OUT_WP_REL;
			if ((flags & ICX_SET_WHITE_C) == ICX_SET_WHITE_C) {
				xfflags |= XFIT_OUT_WP_REL_C;
			}
			else if ((flags & ICX_SET_WHITE_US) == ICX_SET_WHITE_US)
				xfflags |= XFIT_OUT_WP_REL_US;

			if (p->pcs != icSigXYZData)
				xfflags |= XFIT_OUT_LAB;
		}

		/* If asked, clip the absolute white point to have Y <= 1.0 ? */
		if (flags & ICX_CLIP_WB)
			xfflags |= XFIT_CLIP_WP;

		/* With current B2A code, make sure a & b curves */
		/* pass through zero. */
		if (p->pcs == icSigLabData) {
			xfflags |= XFIT_OUT_ZERO;
		}

		/* Let xfit create the clut */
		xfflags |= XFIT_MAKE_CLUT;

		/* Set the curve orders for input (device) and output (PCS) */
		if (quality >= 3) {				/* Ultra high */
			iluord = 25;			
			sluord = 4;			
			oluord = 25;			
		} else if (quality == 2) {		/* High */
			iluord = 20;			
			sluord = 3;			
			oluord = 20;			
		} else if (quality == 1) {		/* Medium */
			iluord = 17;			
			sluord = 2;			
			oluord = 17;			
		} else {						/* Low */
			iluord = 10;			
			sluord = 1;			
			oluord = 10;			
		}
		for (e = 0; e < p->inputChan; e++) {
			iord[e] = iluord;
			sord[e] = sluord;
			in_min[e] = p->inmin[e];
			in_max[e] = p->inmax[e];
			shp_smooth[e] = SHP_SMOOTH;
		}

		for (f = 0; f < p->outputChan; f++) {
			oord[f] = oluord;
			out_min[f] = p->outmin[f];
			out_max[f] = p->outmax[f];

			/* Hack to prevent a convex L curve pushing */
			/* the clut L values above the maximum value */
			/* that can be represented, causing clipping. */
			/* Do this by making sure that the L curve pivots */
			/* through 100.0 to 100.0 */
			if (f == 0 && p->pcs == icSigLabData) {
				if (out_min[f] < 0.0001 && out_max[f] > 100.0) {
					out_max[f] = 100.0;	
				}
			}
			out_smooth[f] = OUT_SMOOTH1;
			if (f != 0 && p->pcs == icSigLabData)	/* a* & b* */
				out_smooth[f] = OUT_SMOOTH2;
		}

//printf("~1 wp before xfit %f %f %f\n", wp[0], wp[1], wp[2]);
		/* Create input, sub and output per channel curves (if configured), */
		/* adjust for white point to make relative (if configured), */
		/* and create clut rspl using xfit class. */
		/* The true white point for the returned curves and rspl is returned. */
		if (xf->fit(xf, xfflags, p->inputChan, p->outputChan,
			rsplflags, wp, dwhite, wpscale, dgwhite, 
		    ipoints, nodp, skm, in_min, in_max, gres, out_min, out_max,
//			bpo != NULL ? &bpop : NULL,
		    smooth, oavgdev, demph, iord, sord, oord, shp_smooth, out_smooth, tcomb,
		   (void *)p, xfit_to_de2, xfit_to_dde2) != 0) {
			p->pp->errc = 2;
			sprintf(p->pp->err,"xfit fitting failed");
			xf->del(xf);
			p->del((icxLuBase *)p);
			return NULL;
		}
//printf("~1 wp after xfit %f %f %f\n", wp[0], wp[1], wp[2]);

		/* - - - - - - - - - - - - - - - */
		/* Set the xicc input curve rspl */
		for (e = 0; e < p->inputChan; e++) {
			curvectx cx;
	
			cx.xf = xf;
			cx.oix = -1;
			cx.iix = e;

			if ((p->inputTable[e] = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
				p->pp->errc = 2;
				sprintf(p->pp->err,"Creation of input table rspl failed");
				xf->del(xf);
				p->del((icxLuBase *)p);
				return NULL;
			}

			p->inputTable[e]->set_rspl(p->inputTable[e], RSPL_NOFLAGS,
			           (void *)&cx, set_linfunc,
    			       &p->ninmin[e], &p->ninmax[e],
			           (int *)&p->lut->inputEnt,
			           &p->ninmin[e], &p->ninmax[e]);
		}

		/* - - - - - - - - - - - - - - - */
		/* Set the xicc output curve rspl */
		for (f = 0; f < p->outputChan; f++) {
			curvectx cx;

			cx.xf = xf;
			cx.iix = -1;
			cx.oix = f;

			if ((p->outputTable[f] = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
				p->pp->errc = 2;
				sprintf(p->pp->err,"Creation of output table rspl failed");
				xf->del(xf);
				p->del((icxLuBase *)p);
				return NULL;
			}

			p->outputTable[f]->set_rspl(p->outputTable[f], RSPL_NOFLAGS,
		                (void *)&cx, set_linfunc,
			            &p->noutmin[f], &p->noutmax[f],
			            (int *)&p->lut->outputEnt,
			            &p->noutmin[f], &p->noutmax[f]);

		}
	}

	if (flags & ICX_VERBOSE)
		printf("Creating fast inverse input lookups\n");

	/* Create rspl based reverse input lookups used in ink limit function. */
	for (e = 0; e < p->inputChan; e++) {
		int res = 256;
		curvectx cx;

		cx.xf = xf;
		cx.oix = -1;
		cx.iix = e;

		if ((p->revinputTable[e] = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
			p->pp->errc = 2;
			sprintf(p->pp->err,"Creation of reverse input table rspl failed");
			xf->del(xf);
			p->del((icxLuBase *)p);
			return NULL;
		}
		p->revinputTable[e]->set_rspl(p->revinputTable[e], RSPL_NOFLAGS,
		           (void *)&cx, icxLuLut_invinput_func,
		           &p->ninmin[e], &p->ninmax[e], &res, &p->ninmin[e], &p->ninmax[e]);
	}


	/* ------------------------------- */
	/* Set clut lookup table from xfit */
	p->clutTable = xf->clut;
	xf->clut = NULL;

	/* Setup all the clipping, ink limiting and auxiliary stuff, */
	/* in case a reverse call is used. Need to avoid relying on inking */
	/* stuff that makes use of the white/black points, since they haven't */
	/* been set up properly yet. */
	if (setup_ink_icxLuLut(p, ink, 0) != 0) {
		xf->del(xf);
		p->del((icxLuBase *)p);
		return NULL;
	}

	/* Deal with finalizing white/black points */
	{

		if ((flags & ICX_SET_WHITE) && (flags & ICX_VERBOSE)) {
			printf("White point XYZ = %s, Lab = %s\n", icmPdv(3,wp),icmPLab(wp));
		}

		/* Lookup the black point */
		{ /* Black Point Tag: */
			co bcc;

			if (flags & ICX_VERBOSE)
				printf("Find black point\n");

			/* !!! Hmm. For CMY and RGB we are simply using the device */
			/* combination values as the black point. In reality we might */
			/* want to have the option of using a neutral black point, */
			/* just like CMYK ?? */

			/* For CMYK devices, we choose a black that has minumum L within */
			/* the ink limits, and if XICC_NEUTRAL_CMYK_BLACK it will in the direction */
			/* that has the same chromaticity as the white point, else choose the same */
			/* Lab vector direction as K, with the minimum L value. */
			/* (Note this is duplicated in xicc.c icxLu_comp_bk_point() !!!) */
			if (h->deviceClass != icSigInputClass
			 && h->colorSpace == icSigCmykData) {
				bfinds bfs;					/* Callback context */
				double sr[MXDO];			/* search radius */
				double tt[MXDO];			/* Temporary */
				double rs0[MXDO], rs1[MXDO];	/* Random start candidates */
				int trial;
				double brv;
				int kch = 3;

				/* Setup callback function context */
				bfs.p = p;
				bfs.toll = XICC_BLACK_POINT_TOLL;

				/* !!! we should use an accessor funcion of xfit !!! */
				for (i = 0; i < 3; i++) {
					for (j = 0; j < 3; j++) {
						bfs.toAbs[i][j] = xf->toAbs[i][j];
					}
				}

				/* Lookup abs Lab value of white point */
				icmXYZ2Lab(&icmD50, bfs.p1, wp);

#ifdef XICC_NEUTRAL_CMYK_BLACK
				icmScale3(tt, wp, 0.02);		/* Scale white XYZ towards 0,0,0 */
				icmXYZ2Lab(&icmD50, bfs.p2, tt); /* Convert black direction to Lab */
				if (flags & ICX_VERBOSE)
					printf("Neutral black direction (Lab) =                     %f %f %f\n",bfs.p2[0], bfs.p2[1], bfs.p2[2]);

#else /* K direction black */
				/* Now figure abs Lab value of K only, as the direction */
				/* to use for the rich black. */
				for (e = 0; e < p->inputChan; e++)
					bcc.p[e] = 0.0;
				if (p->ink.klimit <= 0.0)
					bcc.p[kch] = 1.0;
				else
					bcc.p[kch] = p->ink.klimit;		/* K value */

				p->input(p, bcc.p, bcc.p);			/* Through input tables */
				p->clutTable->interp(p->clutTable, &bcc);	/* Through clut */
				p->output(p, bcc.v, bcc.v);		/* Through the output tables */

				if (p->pcs != icSigXYZData) 	/* Convert PCS to XYZ */
					icmLab2XYZ(&icmD50, bcc.v, bcc.v);

				/* Convert from relative to Absolute colorimetric */
				icmMulBy3x3(tt, xf->toAbs, bcc.v);
				icmXYZ2Lab(&icmD50, bfs.p2, tt); /* Convert K only black point to Lab */

				if (flags & ICX_VERBOSE)
					printf("K only black direction (Lab) =                      %f %f %f\n",bfs.p2[0], bfs.p2[1], bfs.p2[2]);
#endif
				/* Set the random start 0 location as 000K */
				/* and the random start 1 location as CMY0 */
				{
					double tv;

					for (e = 0; e < p->inputChan; e++)
						rs0[e] = 0.0;
					if (p->ink.klimit <= 0.0)
						rs0[kch] = 1.0;
					else
						rs0[kch] = p->ink.klimit;		/* K value */

					if (p->ink.tlimit <= 0.0)
						tv = 1.0;
					else
						tv = p->ink.tlimit/(p->inputChan - 1.0);
					for (e = 0; e < p->inputChan; e++)
						rs1[e] = tv;
					rs1[kch] = 0.0;		/* K value */
				}

				/* Start with the K only as the current best value */
				for (e = 0; e < p->inputChan; e++)
					bcc.p[e] = rs0[e];
				brv = bfindfunc((void *)&bfs, bcc.p);
//printf("~1 initial brv for K only = %f\n",brv);

				/* Find the device black point using optimization */
				/* Do several trials to avoid local minima. */
				rand32(0x12345678);	/* Make trial values deterministic */
				for (trial = 0; trial < 200; trial++) {
					double rv;			/* Temporary */

					/* Start first trial at 000K */
					if (trial == 0) {
						for (j = 0; j < p->inputChan; j++) {
							tt[j] = rs0[j];
							sr[j] = 0.1;
						}

					} else {
						/* Base is random between 000K and CMY0: */
						if (trial < 100) {
							rv = d_rand(0.0, 1.0);
							for (j = 0; j < p->inputChan; j++) {
								tt[j] = rv * rs0[j] + (1.0 - rv) * rs1[j];
								sr[j] = 0.1;
							}
						/* Base on current best */
						} else {
							for (j = 0; j < p->inputChan; j++) {
								tt[j] = bcc.p[j];
								sr[j] = 0.1;
							}
						}
	
						/* Then add random start offset */
						for (rv = 0.0, j = 0; j < p->inputChan; j++) {
							tt[j] += d_rand(-0.5, 0.5);
							if (tt[j] < 0.0)
								tt[j] = 0.0;
							else if (tt[j] > 1.0)
								tt[j] = 1.0;
						}
					}

					/* Clip to be within ink limit */
					icxDoLimit(p, tt, tt);

					if (powell(&rv, p->inputChan, tt, sr, 0.000001, 1000,
					           bfindfunc, (void *)&bfs, NULL, NULL) == 0) {
//printf("~1 trial %d, rv %f bp %f %f %f %f\n",trial,rv,tt[0],tt[1],tt[2],tt[3]);
						if (rv < brv) {
//printf("~1   new best\n");
							brv = rv;
							for (j = 0; j < p->inputChan; j++)
								bcc.p[j] = tt[j];
						}
					}
				}
				if (brv > 1000.0)
					error ("set_icxLuLut: Black point powell failed");

				/* Make sure resulting device values are strictly in range */
				for (j = 0; j < p->inputChan; j++) {
					if (bcc.p[j] < 0.0)
						bcc.p[j] = 0.0;
					else if (bcc.p[j] > 1.0)
						bcc.p[j] = 1.0;
				}
				/* Now have device black in bcc.p[] */
//printf("~1 Black point is CMYK %f %f %f %f\n", bcc.p[0], bcc.p[1], bcc.p[2], bcc.p[3]);

			/* Else not a CMYK output device, */
			/* use the previously determined device black value. */
			} else {
				for (e = 0; e < p->inputChan; e++)
					bcc.p[e] = dblack[e];
			}

			/* Lookup the PCS for the device black: */
			p->input(p, bcc.p, bcc.p);			/* Through input tables */
			p->clutTable->interp(p->clutTable, &bcc);	/* Through clut */
			p->output(p, bcc.v, bcc.v);		/* Through the output tables */

			if (p->pcs != icSigXYZData) 	/* Convert PCS to XYZ */
				icmLab2XYZ(&icmD50, bcc.v, bcc.v);

			/* Convert from relative to Absolute colorimetric */
			icmMulBy3x3(bp, xf->toAbs, bcc.v);

			/* Got XYZ black point in bp[] */
			if (flags & ICX_VERBOSE) {
				printf("Black point XYZ = %s, Lab = %s\n", icmPdv(3,bp),icmPLab(bp));
			}

			/* Some ICC sw gets upset if the bp is at all -ve. */
			/* Clip it if this is the case */
			/* (Or we could get xfit to rescale the rspl instead ??) */
			if ((flags & ICX_CLIP_WB)
			 && (bp[0] < 0.0 || bp[1] < 0.0 || bp[2] < 0.0)) {
				if (bp[0] < 0.0)
					bp[0] = 0.0;
				if (bp[1] < 0.0)
					bp[1] = 0.0;
				if (bp[2] < 0.0)
					bp[2] = 0.0;
				if (flags & ICX_VERBOSE) {
					printf("Black point clipped to XYZ = %s, Lab = %s\n",icmPdv(3,bp),icmPLab(bp));
				}
			}
		}

		/* If this is a display, adjust the white point to be */
		/* exactly Y = 1.0, and compensate the dispLuminance */
		/* and black point accordingly. The Lut is already set to */
		/* assume device white maps to perfect PCS white. */
		if (h->deviceClass == icSigDisplayClass) {
			double scale = 1.0/wp[1];
			int i;

			dispLuminance /= scale;

			for (i = 0; i < 3; i++) {
				wp[i] *= scale;
				bp[i] *= scale;
			}

//			if (bpo != NULL) {
//				bp[0] = bpo[0];
//				bp[1] = bpo[1];
//				bp[2] = bpo[2];
//				printf("Overide Black point XYZ = %s, Lab = %s\n", icmPdv(3,bp),icmPLab(bp));
//			}
		}

		if (h->deviceClass == icSigDisplayClass
		 && dispLuminance > 0.0) {
			icmXYZArray *wo;
			if ((wo = (icmXYZArray *)icco->read_tag(
			           icco, icSigLuminanceTag)) == NULL)  {
				xicp->errc = 1;
				sprintf(xicp->err,"icx_set_luminance: couldn't find luminance tag");
				p->del((icxLuBase *)p);
				return NULL;
			}
			if (wo->ttype != icSigXYZArrayType) {
				xicp->errc = 1;
				sprintf(xicp->err,"luminance: tag has wrong type");
				p->del((icxLuBase *)p);
				return NULL;
			}

			wo->size = 1;
			wo->allocate((icmBase *)wo);	/* Allocate space */
			wo->data[0].X = 0.0;
			wo->data[0].Y = dispLuminance * wp[1];
			wo->data[0].Z = 0.0;

			if (flags & ICX_VERBOSE)
				printf("Display Luminance = %f\n", wo->data[0].Y);
		}

		/* Write white and black points */
		if (flags & ICX_SET_WHITE) { /* White Point Tag: */
			icmXYZArray *wo;
			if ((wo = (icmXYZArray *)icco->read_tag(
			           icco, icSigMediaWhitePointTag)) == NULL)  {
				xicp->errc = 1;
				sprintf(xicp->err,"icx_set_white_black: couldn't find white tag");
				xf->del(xf);
				p->del((icxLuBase *)p);
				return NULL;
			}
			if (wo->ttype != icSigXYZArrayType) {
				xicp->errc = 1;
				sprintf(xicp->err,"icx_set_white_black: white tag has wrong type");
				xf->del(xf);
				p->del((icxLuBase *)p);
				return NULL;
			}

			wo->size = 1;
			wo->allocate((icmBase *)wo);	/* Allocate space */
			wo->data[0].X = wp[0];
			wo->data[0].Y = wp[1];
			wo->data[0].Z = wp[2];
		}
		if (flags & ICX_SET_BLACK) { /* Black Point Tag: */
			icmXYZArray *wo;
			if ((wo = (icmXYZArray *)icco->read_tag(
			           icco, icSigMediaBlackPointTag)) == NULL)  {
				xicp->errc = 1;
				sprintf(xicp->err,"icx_set_white_black: couldn't find black tag");
				xf->del(xf);
				p->del((icxLuBase *)p);
				return NULL;
				}
			if (wo->ttype != icSigXYZArrayType) {
				xicp->errc = 1;
				sprintf(xicp->err,"icx_set_white_black: black tag has wrong type");
				xf->del(xf);
				p->del((icxLuBase *)p);
				return NULL;
			}

			wo->size = 1;
			wo->allocate((icmBase *)wo);	/* Allocate space */
			wo->data[0].X = bp[0];
			wo->data[0].Y = bp[1];
			wo->data[0].Z = bp[2];
		}
		if ((flags & ICX_SET_WHITE) || (flags & ICX_SET_BLACK)) {
			/* Make sure ICC white/black point lookup notices the new white and black points */
			p->plu->init_wh_bk(p->plu);
		}

		/* Setup the clut clipping, ink limiting and auxiliary stuff again */
		/* since re_set_rspl will have invalidated */
		if (setup_ink_icxLuLut(p, ink, 1) != 0) {
			xf->del(xf);
			p->del((icxLuBase *)p);
			return NULL;
		}
	}

	/* Done with xfit now */
	xf->del(xf);
	xf = NULL;

	if (setup_clip_icxLuLut(p) != 0) {
		p->del((icxLuBase *)p);
		return NULL;
	}

	/* ------------------------------- */

//Debugging clipping of clut
//printf("~1 xlut.c: noutmin = %f %f %f\n", p->noutmin[0], p->noutmin[1], p->noutmin[2]);
//printf("~1 xlut.c: noutmax = %f %f %f\n", p->noutmax[0], p->noutmax[1], p->noutmax[2]);
//printf("~1 xlut.c: outmin = %f %f %f\n", p->outmin[0], p->outmin[1], p->outmin[2]);
//printf("~1 xlut.c: outmax = %f %f %f\n", p->outmax[0], p->outmax[1], p->outmax[2]);

	/* Do a specific test for out of Lab encoding range RGB primaries */
	/* (A more general check seems to get false positives - why ??) */
	if (h->pcs == icSigLabData 
	 && (   h->deviceClass == icSigDisplayClass
	     || h->deviceClass == icSigOutputClass)
	 && h->colorSpace == icSigRgbData) {
		double dev[3] = { 0.0, 0.0, 0.0 };
		double pcs[3];
		double clip = 0;

		for (i = 0; i < 3; i++) {
			dev[i] = 1.0;

			if (p->clut(p, pcs, dev) > 1)
				error ("%d, %s",p->pp->errc,p->pp->err);
		
			/* Convert from efective pcs to natural pcs */
			if (p->pcs != icSigLabData)
				icmXYZ2Lab(&icmD50, pcs, pcs);

			if (pcs[1] < -128.0 || pcs[1] > 128.0
			 || pcs[2] < -128.0 || pcs[2] > 128.0) {
				warning("\n    *** %s primary value can't be encoded in L*a*b* PCS (%f %f %f)",
				        i == 0 ? "Red" : i == 1 ? "Green" : "Blue", pcs[0],pcs[1],pcs[2]);
				clip = 1;
			}
			dev[i] = 0.0;
		}
		if (clip)
			a1logw(g_log,"   *** Try switching to XYZ PCS ***\n");
	}

	/* Use our rspl's to set the icc Lut AtoB table values. */
	/* Use helper function to do the hard work. */
	if (p->lut->set_tables(p->lut, ICM_CLUT_SET_EXACT, (void *)p,
			h->colorSpace, 				/* Input color space */
			h->pcs,						/* Output color space */
			set_input,					/* Input transfer function, Dev->Dev' */
			NULL, NULL,					/* Use default Maximum range of Dev' values */
			set_clut,					/* Dev' -> PCS' transfer function */
			NULL, NULL,					/* Use default Maximum range of PCS' values */
			set_output,					/* Linear output transform PCS'->PCS */
			NULL, NULL					/* No APXLS */
	) != 0)
		error("Setting 16 bit %s->%s Lut failed: %d, %s",
			     icm2str(icmColorSpaceSignature, h->colorSpace),
			     icm2str(icmColorSpaceSignature, h->pcs),
		         p->pp->pp->errc,p->pp->pp->err);

#ifdef WARN_CLUT_CLIPPING
	if (p->pp->pp->warnc) {
		warning("Values clipped in setting A2B LUT!");
		if (p->pp->pp->warnc == 2 && h->pcs == icSigLabData) {
			a1logw(g_log,"*** Try switching to XYZ PCS ***\n");
		}
	}
#endif /* WARN_CLUT_CLIPPING */

	/* Init a CAM model in case it will be used (ie. in profile with camclip flag) */
	if (vc != NULL)			/* One has been provided */
		p->vc  = *vc;		/* Copy the structure */
	else
		xicc_enum_viewcond(xicp, &p->vc, -1, NULL, 0, NULL);	/* Use a default */
	p->cam = new_icxcam(cam_default);
	p->cam->set_view(p->cam, p->vc.Ev, p->vc.Wxyz, p->vc.La, p->vc.Yb, p->vc.Lv,
	                 p->vc.Yf, p->vc.Yg, p->vc.Gxyz, XICC_USE_HK);
	
	if (flags & ICX_VERBOSE)
		printf("Done A to B table creation\n");

	return (icxLuBase *)p;
}

/* ========================================================== */
/* Gamut boundary code.                                       */
/* ========================================================== */


/* Context for creating gamut boundary points fro, xicc */
typedef struct {
	gamut *g;				/* Gamut being created */
	icxLuLut *x;			/* xLut we are working from */
	icxLuBase *flu;			/* Forward xlookup */
	double in[MAX_CHAN];	/* Device input values */
} lutgamctx;

/* Function to hand to zbrent to find a clut input' value at the ink limit */
/* Returns value < 0.0 when within gamut, > 0.0 when out of gamut */
static double icxLimitFind(void *fdata, double tp) {
	int i;
	double in[MAX_CHAN];
	lutgamctx *p = (lutgamctx *)fdata;
	double tt;

	for (i = 0; i < p->x->inputChan; i++) {
		in[i] = tp * p->in[i];		/* Scale given input value */
	}
	
	tt = icxLimitD(p->x, in);

	return tt;
}

/* Function to pass to rspl to create gamut boundary from */
/* forward xLut transform grid points. */
static void
lutfwdgam_func(
	void *pp,			/* lutgamctx structure */
	double *out,		/* output' value at clut grid point (ie. PCS' value) */
	double *in			/* input' value at clut grid point (ie. device' value) */
) {
	lutgamctx *p    = (lutgamctx *)pp;
	double pcso[3];	/* PCS output value */

	/* Figure if we are over the ink limit. */
	if (   (p->x->ink.tlimit >= 0.0 || p->x->ink.klimit >= 0.0)
	    && icxLimitD(p->x, in) > 0.0) {
		int i;
		double sf;

		/* We are, so use the bracket search to discover a scale */
		/* for the clut input' value that will put us on the ink limit. */

		for (i = 0; i < p->x->inputChan; i++)
			p->in[i] = in[i];

		if (zbrent(&sf, 0.0, 1.0, 1e-4, icxLimitFind, pp) != 0) {
			return;		/* Give up */
		}

		/* Compute ink limit value */
		for (i = 0; i < p->x->inputChan; i++)
			p->in[i] = sf * in[i];
		
		/* Compute the clut output for this clut input */
		p->x->clut(p->x, pcso, p->in);	
		p->x->output(p->x, pcso, pcso);	
		p->x->out_abs(p->x, pcso, pcso);	
	} else {	/* No ink limiting */
		/* Convert the clut PCS' values to PCS output values */
		p->x->output(p->x, pcso, out);
		p->x->out_abs(p->x, pcso, pcso);	
	}

	/* Expand the gamut surface with this point */
	p->g->expand(p->g, pcso);

	/* Leave out[] unchanged */
}


/* Function to pass to rspl to create gamut boundary from */
/* backwards Lut transform. This is called for every node in the */
/* B2A grid. */
static void
lutbwdgam_func(
	void *pp,			/* lutgamctx structure */
	double *out,		/* output value */
	double *in			/* input value */
) {
	lutgamctx *p    = (lutgamctx *)pp;
	double devo[MAX_CHAN];	/* Device output value */
	double pcso[3];	/* PCS output value */

	/* Convert the clut values to device output values */
	p->x->output(p->x, devo, out);		/* (Device never uses out_abs()) */

	/* Convert from device values to PCS values */
	p->flu->lookup(p->flu, pcso, devo);

	/* Expand the gamut surface with this point */
	p->g->expand(p->g, pcso);

	/* Leave out[] unchanged */
}

/* Given an xicc lookup object, return a gamut object. */
/* Note that the PCS must be Lab or Jab */
/* An icxLuLut type must be icmFwd or icmBwd, */
/* and for icmFwd, the ink limit (if supplied) will be applied. */
/* Return NULL on error, check errc+err for reason */
static gamut *icxLuLutGamut(
icxLuBase   *plu,		/* this */
double       detail		/* gamut detail level, 0.0 = def */
) {
	xicc     *p = plu->pp;				/* parent xicc */
	icxLuLut *luluto = (icxLuLut *)plu;	/* Lookup xLut type object */
	icColorSpaceSignature ins, pcs, outs;
	icmLookupFunc func;
	icRenderingIntent intent;
	double white[3], black[3], kblack[3];
	int inn, outn;
	gamut *gam;

	/* get some details */
	plu->spaces(plu, &ins, &inn, &outs, &outn, NULL, &intent, &func, &pcs);

	if (func != icmFwd && func != icmBwd) {
		p->errc = 1;
		sprintf(p->err,"Creating Gamut surface for anything other than Device <-> PCS is not supported.");
		return NULL;
	}

	if (pcs != icSigLabData && pcs != icxSigJabData) {
		p->errc = 1;
		sprintf(p->err,"Creating Gamut surface PCS of other than Lab or Jab is not supported.");
		return NULL;
	}

	if (func == icmFwd) {
		lutgamctx cx;

		cx.g = gam = new_gamut(detail, pcs == icxSigJabData, 0);
		cx.x = luluto;

		/* Scan through grid. */
		/* (Note this can give problems for a strange input space - ie. Lab */
		/* and a low grid resolution - ie. 2) */
		luluto->clutTable->scan_rspl(
			luluto->clutTable,	/* this */
			RSPL_NOFLAGS,		/* Combination of flags */
			(void *)&cx,		/* Opaque function context */
			lutfwdgam_func		/* Function to set from */
		);

		/* Make sure the white and point goes in too, if it isn't in the grid */
		plu->efv_wh_bk_points(plu, white, NULL, NULL);
		gam->expand(gam, white);

		if (detail == 0.0)
			detail = 10.0;

		/* If the gamut is more than cursary, add some more detail surface points */
		if (detail < 20.0 || luluto->clutTable->g.mres < 4) {
			int res;
			DCOUNT(co, MAX_CHAN, inn, 0, 0, 2);
		
			res = (int)(500.0/detail);	/* Establish an appropriate sampling density */
			if (res < 10)
				res = 10;

			/* Itterate over all the faces in the device space */
			DC_INIT(co);
			while(!DC_DONE(co)) {		/* Count through the corners of hyper cube */
				int e, m1, m2;
				double in[MAX_CHAN];
				double out[3];
		
				for (e = 0; e < inn; e++) {	/* Base value */
					in[e] = (double)co[e];      /* Base value */
					in[e] = in[e] * (luluto->inmax[e] - luluto->inmin[e])
					       + luluto->inmin[e];
				}

   				/* Figure if we are over the ink limit. */
				if ((luluto->ink.tlimit >= 0.0 || luluto->ink.klimit >= 0.0)
			        && icxLimit(luluto, in) > 0.0) {
					DC_INC(co);
					continue;		/* Skip points over limit */
				}

				/* Scan only device surface */
				for (m1 = 0; m1 < inn; m1++) {		/* Choose first coord to scan */
					if (co[m1] != 0)
						continue;					/* Not at lower corner */
					for (m2 = m1 + 1; m2 < inn; m2++) {	/* Choose second coord to scan */
						int x, y;
		
						if (co[m2] != 0)
							continue;					/* Not at lower corner */
		
						for (x = 0; x < res; x++) {				/* step over surface */
							in[m1] = x/(res - 1.0);
							in[m1] = in[m1] * (luluto->inmax[m1] - luluto->inmin[m1])
							       + luluto->inmin[m1];
							for (y = 0; y < res; y++) {
								in[m2] = y/(res - 1.0);
								in[m2] = in[m2] * (luluto->inmax[m2] - luluto->inmin[m2])
								       + luluto->inmin[m2];

				   				/* Figure if we are over the ink limit. */
								if (   (luluto->ink.tlimit >= 0.0 || luluto->ink.klimit >= 0.0)
							        && icxLimit(luluto, in) > 0.0) {
									continue;		/* Skip points over limit */
								}
		
								luluto->lookup((icxLuBase *)luluto, out, in);
								gam->expand(gam, out);
							}
						}
					}
				}
				/* Increment index within block */
				DC_INC(co);
			}
		}

		/* Now set the cusp points by itterating through colorant 0 & 100% combinations */
		/* If we know what sort of space it is: */
		if (ins == icSigRgbData || ins == icSigCmyData || ins == icSigCmykData) {
			DCOUNT(co, 3, 3, 0, 0, 2);

			gam->setcusps(gam, 0, NULL);
			DC_INIT(co);
			while(!DC_DONE(co)) {
				int e;
				double in[MAX_CHAN];
				double out[3];
		
				if (!(co[0] == 0 && co[1] == 0 && co[2] == 0)
				 && !(co[0] == 1 && co[1] == 1 && co[2] == 1)) {	/* Skip white and black */
					for (e = 0; e < 3; e++)
						in[e] = (double)co[e];
					in[e] = 0;					/* K */
		
					/* Always use the device->PCS conversion */
					if (luluto->lookup((icxLuBase *)luluto, out, in) > 1)
						error ("%d, %s",p->errc,p->err);
					gam->setcusps(gam, 3, out);
				}

				DC_INC(co);
			}
			gam->setcusps(gam, 2, NULL);

		/* Do all ink combinations and hope we can sort it out */
		/* (This may not be smart, since bodgy cusp values will cause gamut mapping to fail...) */
		} else if (ins != icSigXYZData
		        && ins != icSigLabData
		        && ins != icSigLuvData
		        && ins != icSigYxyData) {
			DCOUNT(co, MAX_CHAN, inn, 0, 0, 2);

			gam->setcusps(gam, 0, NULL);
			DC_INIT(co);
			while(!DC_DONE(co)) {
				int e;
				double in[MAX_CHAN];
				double out[3];
		
				for (e = 0; e < inn; e++) {
					in[e] = (double)co[e];
					in[e] = in[e] * (luluto->inmax[e] - luluto->inmin[e])
					       + luluto->inmin[e];
				}
	
	   			/* Figure if we are over the ink limit. */
				if ((luluto->ink.tlimit >= 0.0 || luluto->ink.klimit >= 0.0)
			        && icxLimit(luluto, in) > 0.0) {
					DC_INC(co);
					continue;		/* Skip points over limit */
				}
	
				luluto->lookup((icxLuBase *)luluto, out, in);
				gam->setcusps(gam, 1, out);

				DC_INC(co);
			}
			gam->setcusps(gam, 2, NULL);
		}

	} else { /* Must be icmBwd */
		lutgamctx cx;
	
		/* Get an appropriate device to PCS conversion for the fwd conversion */
		/* we use after bwd conversion in lutbwdgam_func() */
		switch (intent) {
			/* If it is relative */
			case icmDefaultIntent:					/* Shouldn't happen */
			case icPerceptual:
			case icRelativeColorimetric:
			case icSaturation:
				intent = icRelativeColorimetric;	/* Choose relative */
				break;
			/* If it is absolute */
			case icAbsoluteColorimetric:
			case icxAppearance:
			case icxAbsAppearance:
				break;								/* Leave unchanged */
			default:
				break;
		}
		if ((cx.flu = p->get_luobj(p, ICX_CLIP_NEAREST , icmFwd, intent, pcs, icmLuOrdNorm,
		                              &plu->vc, NULL)) == NULL) {
			return NULL;	/* oops */
		}

		cx.g = gam = new_gamut(detail, pcs == icxSigJabData, 0);

		cx.x = luluto;

		luluto->clutTable->scan_rspl(
			luluto->clutTable,	/* this */
			RSPL_NOFLAGS,		/* Combination of flags */
			(void *)&cx,		/* Opaque function context */
			lutbwdgam_func		/* Function to set from */
		);

		/* Now set the cusp points by using the fwd conversion and */
		/* itterating through colorant 0 & 100% combinations. */
		/* If we know what sort of space it is: */
		if (outs == icSigRgbData || outs == icSigCmyData || outs == icSigCmykData) {
			DCOUNT(co, 3, 3, 0, 0, 2);

			gam->setcusps(gam, 0, NULL);
			DC_INIT(co);
			while(!DC_DONE(co)) {
				int e;
				double in[MAX_CHAN];
				double out[3];
		
				if (!(co[0] == 0 && co[1] == 0 && co[2] == 0)
				 && !(co[0] == 1 && co[1] == 1 && co[2] == 1)) {	/* Skip white and black */
					for (e = 0; e < 3; e++)
						in[e] = (double)co[e];
					in[e] = 0;					/* K */
		
					/* Always use the device->PCS conversion */
					if (cx.flu->lookup((icxLuBase *)cx.flu, out, in) > 1)
						error ("%d, %s",p->errc,p->err);
					gam->setcusps(gam, 3, out);
				}

				DC_INC(co);
			}
			gam->setcusps(gam, 2, NULL);

		/* Do all ink combinations and hope we can sort it out */
		/* (This may not be smart, since bodgy cusp values will cause gamut mapping to fail...) */
		} else if (ins != icSigXYZData
		        && ins != icSigLabData
		        && ins != icSigLuvData
		        && ins != icSigYxyData) {
			DCOUNT(co, MAX_CHAN, inn, 0, 0, 2);

			gam->setcusps(gam, 0, NULL);
			DC_INIT(co);
			while(!DC_DONE(co)) {
				int e;
				double in[MAX_CHAN];
				double out[3];
		
				for (e = 0; e < inn; e++) {
					in[e] = (double)co[e];
					in[e] = in[e] * (luluto->inmax[e] - luluto->inmin[e])
					       + luluto->inmin[e];
				}
	
	   			/* Figure if we are over the ink limit. */
				if ((luluto->ink.tlimit >= 0.0 || luluto->ink.klimit >= 0.0)
			        && icxLimit(luluto, in) > 0.0) {
					DC_INC(co);
					continue;		/* Skip points over limit */
				}
	
				cx.flu->lookup((icxLuBase *)cx.flu, out, in);
				gam->setcusps(gam, 1, out);

				DC_INC(co);
			}
			gam->setcusps(gam, 2, NULL);
		}
		cx.flu->del(cx.flu);	/* Done with the fwd conversion */
	}

	/* Set the white and black points */
	plu->efv_wh_bk_points(plu, white, black, kblack);
	gam->setwb(gam, white, black, kblack);

//printf("~1 icxLuLutGamut: set black %f %f %f and kblack %f %f %f\n", black[0], black[1], black[2], kblack[0], kblack[1], kblack[2]);

#ifdef NEVER	/* Not sure if this is a good idea ?? */
	/* Since we know this is a profile, force the space and gamut points to be the same */
	gam->getwb(gam, NULL, NULL, white, black, kblack);	/* Get the actual gamut white and black points */
	gam->setwb(gam, white, black, kblack);				/* Put it back as colorspace one */
#endif

	return gam;
}

/* ----------------------------------------------- */
#ifdef DEBUG
#undef DEBUG 	/* Limit extent to this file */
#endif






