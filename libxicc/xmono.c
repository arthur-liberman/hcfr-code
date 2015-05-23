/* 
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    2/7/00
 * Version: 1.00
 *
 * Copyright 2000 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Based on the old iccXfm class.
 */

/*
 * This module provides the expands icclib functionality
 * for monochrome profiles.
 * This file is #included in xicc.c, to keep its functions private.
 */

/*
 * TTBD:
 *       Some of the error handling is crude. Shouldn't use
 *       error(), should return status.
 *
 */

/* ============================================================= */
/* Forward and Backward Monochrome type conversion */
/* Return 0 on success, 1 if clipping occured, 2 on other error */

/* - - - - - - - - - - - - - - - - - - - - - */
/* Individual components of Fwd conversion: */
static int
icxLuMonoFwd_curve (
icxLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	return ((icmLuMono *)p->plu)->fwd_curve((icmLuMono *)p->plu, out, in);
}

static int
icxLuMonoFwd_map (
icxLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	return ((icmLuMono *)p->plu)->fwd_map((icmLuMono *)p->plu, out, in);
}

static int
icxLuMonoFwd_abs (
icxLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	rv |= ((icmLuMono *)p->plu)->fwd_abs((icmLuMono *)p->plu, out, in);

	if (p->pcs == icxSigJabData) {
		p->cam->XYZ_to_cam(p->cam, out, out);
	}
	return rv;
}


/* Overall Fwd conversion routine */
static int
icxLuMonoFwd_lookup (
icxLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icxLuMono *p = (icxLuMono *)pp;
	rv |= icxLuMonoFwd_curve(p, out, in);
	rv |= icxLuMonoFwd_map(p, out, out);
	rv |= icxLuMonoFwd_abs(p, out, out);
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given a relative XYZ or Lab PCS value, convert in the fwd direction into */ 
/* the nominated output PCS (ie. Absolute, Jab etc.) */
/* (This is used in generating gamut compression in B2A tables) */
void icxLuMono_fwd_relpcs_outpcs(
icxLuBase *pp,
icColorSpaceSignature is,		/* Input space, XYZ or Lab */
double *out, double *in) {
	icxLuMono *p = (icxLuMono *)pp;

	icmLab2XYZ(&icmD50, out, in);
	if (is == icSigLabData && p->natpcs == icSigXYZData) {
		icxLuMonoFwd_abs(p, out, out);
	} else if (is == icSigXYZData && p->natpcs == icSigLabData) {
		icxLuMonoFwd_abs(p, out, out);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - */
/* Individual components of Bwd conversion: */

static int
icxLuMonoBwd_abs (
icxLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;

	if (p->pcs == icxSigJabData) {
		p->cam->cam_to_XYZ(p->cam, out, in);
		rv |= ((icmLuMono *)p->plu)->bwd_abs((icmLuMono *)p->plu, out, out);
		/* Hack to prevent CAM02 weirdness being amplified by */
		/* any later per channel clipping. */
		/* Limit -Y to non-stupid values by scaling */
		if (out[1] < -0.1) {
			out[0] *= -0.1/out[1];
			out[2] *= -0.1/out[1];
			out[1] = -0.1;
		}
	} else {
		rv |= ((icmLuMono *)p->plu)->bwd_abs((icmLuMono *)p->plu, out, in);
	}
	return rv;
}

static int
icxLuMonoBwd_map (
icxLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	return ((icmLuMono *)p->plu)->bwd_map((icmLuMono *)p->plu, out, in);
}

static int
icxLuMonoBwd_curve (
icxLuMono *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	return ((icmLuMono *)p->plu)->bwd_curve((icmLuMono *)p->plu, out, in);
}

/* Overall Bwd conversion routine */
static int
icxLuMonoBwd_lookup (
icxLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	double temp[3];
	int rv = 0;
	icxLuMono *p = (icxLuMono *)pp;
	rv |= icxLuMonoBwd_abs(p, temp, in);
	rv |= icxLuMonoBwd_map(p, out, temp);
	rv |= icxLuMonoBwd_curve(p, out, out);
	return rv;
}

static void
icxLuMono_free(
icxLuBase *p
) {
	p->plu->del(p->plu);
	if (p->cam != NULL)
		p->cam->del(p->cam);
	free(p);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given a nominated output PCS (ie. Absolute, Jab etc.), convert it in the bwd */
/* direction into a relative XYZ or Lab PCS value */
/* (This is used in generating gamut compression in B2A tables) */
void icxLuMono_bwd_outpcs_relpcs(
icxLuBase *pp,
icColorSpaceSignature os,		/* Output space, XYZ or Lab */
double *out, double *in) {
	icxLuMono *p = (icxLuMono *)pp;

	if (os == icSigXYZData && p->natpcs == icSigLabData) {
		icxLuMonoFwd_abs(p, out, in);
		icmLab2XYZ(&icmD50, out, out);
	} else if (os == icSigXYZData && p->natpcs == icSigLabData) {
		icxLuMonoFwd_abs(p, out, in);
		icmXYZ2Lab(&icmD50, out, out);
	} else {
		icxLuMonoFwd_abs(p, out, in);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static gamut *icxLuMonoGamut(icxLuBase *plu, double detail);

static icxLuBase *
new_icxLuMono(
xicc                  *xicp,
int                   flags,		/* clip, merge flags */
icmLuBase             *plu,			/* Pointer to Lu we are expanding */
icmLookupFunc         func,			/* Functionality requested */
icRenderingIntent     intent,		/* Rendering intent */
icColorSpaceSignature pcsor,		/* PCS override (0 = def) */
icxViewCond           *vc,			/* Viewing Condition (NULL if pcsor is not CIECAM) */
int                   dir			/* 0 = fwd, 1 = bwd */
) {
	icxLuMono *p;

	/* Do the basic icxLuMono creation and initialisation */

	if ((p = (icxLuMono *) calloc(1,sizeof(icxLuMono))) == NULL)
		return NULL;

	p->pp                = xicp;
	p->plu               = plu;
	p->del               = icxLuMono_free;
	p->lutspaces         = icxLutSpaces;
	p->spaces            = icxLuSpaces;
	p->get_native_ranges = icxLu_get_native_ranges;
	p->get_ranges        = icxLu_get_ranges;
	p->efv_wh_bk_points  = icxLuEfv_wh_bk_points;
	p->get_gamut         = icxLuMonoGamut;
	p->fwd_relpcs_outpcs = icxLuMono_fwd_relpcs_outpcs;
	p->bwd_outpcs_relpcs = icxLuMono_bwd_outpcs_relpcs;
	p->nearclip = 0;				/* Set flag defaults */
	p->mergeclut = 0;
	p->noisluts = 0;
	p->noipluts = 0;
	p->nooluts = 0;
	p->intsep = 0;

	p->fwd_lookup = icxLuMonoFwd_lookup;
	p->fwd_curve  = icxLuMonoFwd_curve;
	p->fwd_map    = icxLuMonoFwd_map;
	p->fwd_abs    = icxLuMonoFwd_abs;
	p->bwd_lookup = icxLuMonoBwd_lookup;
	p->bwd_abs    = icxLuMonoFwd_abs;
	p->bwd_map    = icxLuMonoFwd_map;
	p->bwd_curve  = icxLuMonoFwd_curve;
	if (dir) {
		p->lookup     = icxLuMonoBwd_lookup;
		p->inv_lookup = icxLuMonoFwd_lookup;
	} else {
		p->lookup     = icxLuMonoFwd_lookup;
		p->inv_lookup = icxLuMonoBwd_lookup;
	}

	/* There are no mono specific flags */
	p->flags = flags;
	p->func = func;

	/* Get details of internal, native color space */
	plu->lutspaces(p->plu, &p->natis, NULL, &p->natos, NULL, &p->natpcs);

	/* Get other details of conversion */
	p->plu->spaces(p->plu, NULL, &p->inputChan, NULL, &p->outputChan, NULL, NULL, NULL, NULL, NULL);

	/* Init the CAM model */
	if (pcsor == icxSigJabData) {
		p->vc  = *vc;				/* Copy the structure */
		p->cam = new_icxcam(cam_default);
		p->cam->set_view(p->cam, vc->Ev, vc->Wxyz, vc->La, vc->Yb, vc->Lv, vc->Yf, vc->Yg, vc->Gxyz,
		XICC_USE_HK);
	} else 
		p->cam = NULL;

	/* Remember the effective intent */
	p->intent = intent;

	/* Get the effective spaces */
	plu->spaces(plu, &p->ins, NULL, &p->outs, NULL, NULL, NULL, NULL, &p->pcs, NULL);

	/* Override with pcsor */
	if (pcsor == icxSigJabData) {
		p->pcs = pcsor;		
		if (func == icmBwd || func == icmGamut || func == icmPreview)
			p->ins = pcsor;
		if (func == icmFwd || func == icmPreview)
			p->outs = pcsor;
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

	return (icxLuBase *)p;
}

/* ============================================================= */

/* Given an xicc lookup object, returm a gamut object. */
/* Note that the PCS must be Lab or Jab */
/* Return NULL on error, check errc+err for reason */
static gamut *icxLuMonoGamut(
icxLuBase   *plu,		/* this */
double       detail		/* gamut detail level, 0.0 = def */
) {
	gamut *xgam;
	xicc  *p = plu->pp;				/* parent xicc */

	p->errc = 1;
	sprintf(p->err,"Creating Mono gamut surface not supported yet.");
	plu->del(plu);
	xgam = NULL;

	return xgam;
}

