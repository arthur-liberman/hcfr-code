/* 
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    2/7/00
 * Version: 1.00
 *
 * Copyright 2000, 2003 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Based on the old iccXfm class.
 */

/*
 * This module provides the expands icclib functionality
 * for matrix profiles.
 * This file is #included in xicc.c, to keep its functions private.
 */

/*
 * TTBD:
 *
 *      Some of the error handling is crude. Shouldn't use
 *      error(), should return status.
 *
 *		Should allow for offset in curves - this will greatly improve
 *		profile quality on non-calibrated displays. See spectro/dispcal.c
 *		spectro/moncurve.c. Use conjgrad() instead of powell() to speed things up.
 *      Note that if curves have scale, the scale will have to be
 *      normalized back to zero by scaling the matrix before storing
 *      the result in the ICC profile.
 *
 */



#define USE_CIE94_DE	/* Use CIE94 delta E measure when creating fit */

/* Weights in shaper parameters, to minimise unconstrained "wiggles" */
#define MXNORDERS 30			/* Maximum shaper harmonic orders to use */
#define XSHAPE_MAG  1.0			/* Overall shaper parameter magnitide */

#define XSHAPE_OFFG			0.1		/* Input offset weights when ord 0 is gamma */
#define XSHAPE_OFFS			1.0		/* Input offset weights when ord 0 is shaper */
#define XSHAPE_HW01		   0.01		/* 0 & 1 harmonic weights */
#define XSHAPE_HBREAK	   3		/* Harmonic that has HWBR */
#define XSHAPE_HWBR        0.5		/* Base weight of harmonics HBREAK up */
#define XSHAPE_HWINC       0.5		/* Increase in weight for each harmonic above HWBR */

#define XSHAPE_GAMTHR	   0.01		/* Input threshold for linear slope below gamma power */

#undef DEBUG			/* Extra printfs */
#undef DEBUG_PLOT		/* Plot curves */
#define G_DEB 0			/* g_deb default value */ 

/* ========================================================= */
/* Forward and Backward Matrix type conversion */
/* Return 0 on success, 1 if clipping occured, 2 on other error */

/* Individual components of Fwd conversion: */
static int
icxLuMatrixFwd_curve (
icxLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	return ((icmLuMatrix *)p->plu)->fwd_curve((icmLuMatrix *)p->plu, out, in);
}

static int
icxLuMatrixFwd_matrix (
icxLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	return ((icmLuMatrix *)p->plu)->fwd_matrix((icmLuMatrix *)p->plu, out, in);
}

static int
icxLuMatrixFwd_abs (
icxLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	rv |= ((icmLuMatrix *)p->plu)->fwd_abs((icmLuMatrix *)p->plu, out, in);

	if (p->pcs == icxSigJabData)
		p->cam->XYZ_to_cam(p->cam, out, out);
	return rv;
}


/* Overall Fwd conversion */
static int
icxLuMatrixFwd_lookup (
icxLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icxLuMatrix *p = (icxLuMatrix *)pp;
	rv |= icxLuMatrixFwd_curve(p, out, in);
	rv |= icxLuMatrixFwd_matrix(p, out, out);
	rv |= icxLuMatrixFwd_abs(p, out, out);
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Given a relative XYZ or Lab PCS value, convert in the fwd direction into */ 
/* the nominated output PCS (ie. Absolute, Jab etc.) */
/* (This is used in generating gamut compression in B2A tables) */
void icxLuMatrix_fwd_relpcs_outpcs(
icxLuBase *pp,
icColorSpaceSignature is,		/* Input space, XYZ or Lab */
double *out, double *in) {
	icxLuMatrix *p = (icxLuMatrix *)pp;

	if (is == icSigLabData && p->natpcs == icSigXYZData) {
		icmLab2XYZ(&icmD50, out, in);
		icxLuMatrixFwd_abs(p, out, out);
	} else if (is == icSigXYZData && p->natpcs == icSigLabData) {
		icmXYZ2Lab(&icmD50, out, in);
		icxLuMatrixFwd_abs(p, out, out);
	} else {
		icxLuMatrixFwd_abs(p, out, in);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - */
/* Individual components of Bwd conversion: */

static int
icxLuMatrixBwd_abs (
icxLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;

	if (p->pcs == icxSigJabData) {
		p->cam->cam_to_XYZ(p->cam, out, in);
		/* Hack to prevent CAM02 weirdness being amplified by */
		/* any later per channel clipping. */
		/* Limit -Y to non-stupid values by scaling */
		if (out[1] < -0.1) {
			out[0] *= -0.1/out[1];
			out[2] *= -0.1/out[1];
			out[1] = -0.1;
		}
		rv |= ((icmLuMatrix *)p->plu)->bwd_abs((icmLuMatrix *)p->plu, out, out);
	} else {
		rv |= ((icmLuMatrix *)p->plu)->bwd_abs((icmLuMatrix *)p->plu, out, in);
	}
	return rv;
}

static int
icxLuMatrixBwd_matrix (
icxLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	return ((icmLuMatrix *)p->plu)->bwd_matrix((icmLuMatrix *)p->plu, out, in);
}

static int
icxLuMatrixBwd_curve (
icxLuMatrix *p,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	return ((icmLuMatrix *)p->plu)->bwd_curve((icmLuMatrix *)p->plu, out, in);
}

/* Overall Bwd conversion */
static int
icxLuMatrixBwd_lookup (
icxLuBase *pp,		/* This */
double *out,		/* Vector of output values */
double *in			/* Vector of input values */
) {
	int rv = 0;
	icxLuMatrix *p = (icxLuMatrix *)pp;
	rv |= icxLuMatrixBwd_abs(p, out, in);
	rv |= icxLuMatrixBwd_matrix(p, out, out);
	rv |= icxLuMatrixBwd_curve(p, out, out);
	return rv;
}

static void
icxLuMatrix_free(
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
void icxLuMatrix_bwd_outpcs_relpcs(
icxLuBase *pp,
icColorSpaceSignature os,		/* Output space, XYZ or Lab */
double *out, double *in) {
	icxLuMatrix *p = (icxLuMatrix *)pp;

	icxLuMatrixFwd_abs(p, out, in);
	if (os == icSigXYZData && p->natpcs == icSigLabData) {
		icmLab2XYZ(&icmD50, out, out);
	} else if (os == icSigXYZData && p->natpcs == icSigLabData) {
		icmXYZ2Lab(&icmD50, out, out);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static gamut *icxLuMatrixGamut(icxLuBase *plu, double detail); 

/* Do the basic icxLuMatrix creation and initialisation */
static icxLuMatrix *
alloc_icxLuMatrix(
	xicc                  *xicp,
	icmLuBase             *plu,			/* Pointer to Lu we are expanding (ours) */
	int                   dir,			/* 0 = fwd, 1 = bwd */
	int                   flags			/* clip, merge flags */
) {
	icxLuMatrix *p;

	if ((p = (icxLuMatrix *) calloc(1,sizeof(icxLuMatrix))) == NULL)
		return NULL;

	p->pp                = xicp;
	p->plu               = plu;
	p->del               = icxLuMatrix_free;
	p->lutspaces         = icxLutSpaces;
	p->spaces            = icxLuSpaces;
	p->get_native_ranges = icxLu_get_native_ranges;
	p->get_ranges        = icxLu_get_ranges;
	p->efv_wh_bk_points  = icxLuEfv_wh_bk_points;
	p->get_gamut         = icxLuMatrixGamut;
	p->fwd_relpcs_outpcs = icxLuMatrix_fwd_relpcs_outpcs;
	p->bwd_outpcs_relpcs = icxLuMatrix_bwd_outpcs_relpcs;

	p->nearclip = 0;				/* Set flag defaults */
	p->mergeclut = 0;
	p->noisluts = 0;
	p->noipluts = 0;
	p->nooluts = 0;

	p->intsep = 0;

	p->fwd_lookup = icxLuMatrixFwd_lookup;
	p->fwd_curve  = icxLuMatrixFwd_curve;
	p->fwd_matrix = icxLuMatrixFwd_matrix;
	p->fwd_abs    = icxLuMatrixFwd_abs;
	p->bwd_lookup = icxLuMatrixBwd_lookup;
	p->bwd_abs    = icxLuMatrixBwd_abs;
	p->bwd_matrix = icxLuMatrixBwd_matrix;
	p->bwd_curve  = icxLuMatrixBwd_curve;

	if (dir) {		/* Bwd */
		p->lookup     = icxLuMatrixBwd_lookup;
		p->inv_lookup = icxLuMatrixFwd_lookup;
	} else {
		p->lookup     = icxLuMatrixFwd_lookup;
		p->inv_lookup = icxLuMatrixBwd_lookup;
	}

	/* There are no matrix specific flags */
	p->flags = flags;

	/* Get details of internal, native color space */
	p->plu->lutspaces(p->plu, &p->natis, NULL, &p->natos, NULL, &p->natpcs);

	/* Get other details of conversion */
	p->plu->spaces(p->plu, NULL, &p->inputChan, NULL, &p->outputChan, NULL, NULL, NULL, NULL, NULL);

	return p;
}

/* We setup valid fwd and bwd component conversions, */
/* but setup only the asked for overal conversion. */
static icxLuBase *
new_icxLuMatrix(
xicc                  *xicp,
int                   flags,		/* clip, merge flags */
icmLuBase             *plu,			/* Pointer to Lu we are expanding */
icmLookupFunc         func,			/* Functionality requested */
icRenderingIntent     intent,		/* Rendering intent */
icColorSpaceSignature pcsor,		/* PCS override (0 = def) */
icxViewCond           *vc,			/* Viewing Condition (NULL if pcsor is not CIECAM) */
int                   dir			/* 0 = fwd, 1 = bwd */
) {
	icxLuMatrix *p;

	/* Do basic creation and initialisation */
	if ((p = alloc_icxLuMatrix(xicp, plu, dir, flags)) == NULL)
		return NULL;

	p->func = func;

	/* Init the CAM model */
	if (pcsor == icxSigJabData) {
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

/* ========================================================== */
/* xicc creation code                                         */
/* ========================================================== */

/* Context for figuring input curves */
typedef struct {
	rspl *r;			/* Device -> PCS rspl */
	int linear;			/* Flag */
	double nmin, nmax;	/* PCS End points to linearise */
	double min, max;	/* device End points to linearise */
} mxinctx;

#define NPARMS (9 + 6 + 3 * MXNORDERS)

/* Context for optimising matrix */
typedef struct {
	int verb;				/* Verbose */
	int optdim;				/* Optimisation dimensions */
	int isLinear;			/* NZ if no curves, fixed Gamma = 1.0 */
	int isGamma;			/* NZ if gamma + matrix, else shaper */
	int isShTRC;			/* NZ if shared TRC */
	int shape0gam;			/* NZ if zero'th order shaper should be gamma function */
	int norders;			/* Number of shaper orders */
	int clipbw;				/* Prevent white > 1 and -ve black */
	int clipprims;			/* Prevent primaries going -ve */
	double smooth;			/* Shaper smoothing factor (nominal = 1.0) */ 
	double dscale;			/* Scale device values */
	double v[NPARMS];		/* Holder for parameters */
	double sa[NPARMS];		/* Initial search area */
							/* Rest are matrix : */
							/* 0  1  2   R   X   */
							/* 3  4  5 * G = Y   */
							/* 6  7  8   B   Z   */
							/* For Gamma: */
							/* 9, 10, 11 are gamma */
							/* Else for shaper only: */
							/* 9,  10, 11 are Input Offset */
							/* 12, 13, 14 are Output Offset */
							/* 15, 16, 17 are Gamma or 0th harmonics */
							/* 18, 19, 20 are 1st harmonics */
							/* 21, 22, 23 are 2nd harmonics */
							/* 24, 25, 26 etc. */
							/* For isShTRC there is only one set of offsets & harmonics */
	icmXYZNumber wp;		/* Assumed white point for Lab conversion */
	cow *points;			/* List of test points as dev->Lab */
	int nodp;				/* Number of data points */
} mxopt;

/* Per chanel function being optimised */
static void mxmfunc1(mxopt *p, int j, double *v, double *out, double *in) {
	double vv, g;
	int ps = 3;				/* Parameter spacing */

	vv = *in * p->dscale;
	
	if (p->isShTRC) {
		j = 0;
		ps = 1;				/* Only one channel */
	}

	if (p->isLinear) {		/* No per channel curve */
		*out = vv;
		return;
	}

	if (p->isGamma) {		/* Pure Gamma */
		/* Apply gamma */
		g = v[9 + j];
		if (g <= 0.0)
			vv = 1.0;
		else {
			if (vv >= 0.0) {
				vv = pow(vv, g);
			} else {
				vv = -pow(-vv, g);
			}
		}
	} else {		/* Add extra shaper parameters */
		int ord;

		if (p->shape0gam) {

			/* Apply input offset */
			g = v[9 + j];		/* Offset value */
	
			if (g >= 1.0) {
				vv = 1.0;
			} else {
				vv = g + ((1.0 - g) * vv);
			}

			/* Apply gamma as order 0 */
			g = v[9 + 2 * ps + j];
			if (g <= 0.0)
				vv = 1.0;
			else {
				/* Power with straight line at small values */
				if (vv >= XSHAPE_GAMTHR) {
					vv = pow(vv, g);
				} else {
					double slope, oth;
	
					oth = pow(XSHAPE_GAMTHR, g);				/* Output at input threshold */
					slope = g * pow(XSHAPE_GAMTHR, g - 1.0);	/* Slope at input threshold */
					vv = oth + (vv - XSHAPE_GAMTHR) * slope;	/* Straight line */
				}
			}
		}

		/* Process all the shaper orders from high to low. */
		/* [These shapers were inspired by a Graphics Gem idea */
		/* (Gems IV, VI.3, "Fast Alternatives to Perlin's Bias and */
		/*  Gain Functions, pp 401). */
		/*  They have the nice properties that they are smooth, and */
		/*  can't be non-monotonic. The control parameter has been */
		/*  altered to have a range from -oo to +oo rather than 0.0 to 1.0 */
		/*  so that the search space is less non-linear. ] */
		if (p->shape0gam)
			ord = 1;
		else
			ord = 0;
		for (; ord < p->norders; ord++)
		{
			int nsec;			/* Number of sections */
			double sec;			/* Section */
			g = v[9 + 2 * ps + ord * ps + j];	/* Parameter */
	
			nsec = ord + 1;		/* Increase sections for each order */
	
			vv *= (double)nsec;
	
			sec = floor(vv);
			if (((int)sec) & 1)
				g = -g;				/* Alternate action in each section */
			vv -= sec;
			if (g >= 0.0) {
				vv = vv/(g - g * vv + 1.0);
			} else {
				vv = (vv - g * vv)/(1.0 - g * vv);
			}
			vv += sec;
			vv /= (double)nsec;
		}

		/* (For extrapolation it helps to pin 0 & 1) */
		if (p->shape0gam) {
			/* Apply output offset */
			g = v[9 + 1 * ps + j];		/* Offset value */
	
			if (g >= 1.0) {
				vv = 1.0;
			} else if (g > 0.0) {
				vv = g + ((1.0 - g) * vv);
			}
		}
	}

	*out = vv;
}

/* Function being optimised */
static void mxmfunc(mxopt *p, double *v, double *xyz, double *in) {
	int j;
	double rgb[3];

	/* Apply per channel processing */
	for (j = 0; j < 3; j++)
		mxmfunc1(p, j, v, &rgb[j], &in[j]);

	/* Apply matrix */
	xyz[0] = v[0] * rgb[0] + v[1] * rgb[1] + v[2] * rgb[2];
	xyz[1] = v[3] * rgb[0] + v[4] * rgb[1] + v[5] * rgb[2];
	xyz[2] = v[6] * rgb[0] + v[7] * rgb[1] + v[8] * rgb[2];

}

/* return the sum of the squares of the current shaper parameters */
static double xshapmag(
mxopt  *p,			/* Base of optimisation structure */
double *v			/* Pointer to parameters */
) {
	double tt, w, tparam = 0.0;
	int f, g;

	if (p->isGamma) {		/* Pure Gamma only */
		return 0.0;
	}

	if (p->isShTRC) {
		/* Input offset value */
		if (p->shape0gam)
			w = XSHAPE_OFFG;
		else
			w = XSHAPE_OFFS;

		tt = v[9];
		tt *= tt;
		tparam += w * tt;

		/* Output offset value */
		tt = v[10];
		tt *= tt;
		tparam += w * tt;

		/* Shaper values */
		for (f = 0; f < p->norders; f++) {
			tt = v[11 + f];
			if (f == 0 && p->shape0gam)
				tt -= 1.0;			/* default is linear */
			/* Weigh to suppress ripples */
			if (f <= 1) {						/* Use XSHAPE_HW01 */
				w = XSHAPE_HW01;
			} else if (f <= XSHAPE_HBREAK) {	/* Blend from XSHAPE_HW01 to XSHAPE_HWBR * smooth */
				double bl = (f - 1.0)/(XSHAPE_HBREAK - 1.0);
				w = (1.0 - bl) * XSHAPE_HW01 + bl * XSHAPE_HWBR * p->smooth;
			} else {				/* Use XSHAPE_HWBR * smooth */
				w = XSHAPE_HWBR + (f-XSHAPE_HBREAK) * XSHAPE_HWINC * p->smooth;
			}
			tt *= tt;
			tparam += w * tt;
		}
		return XSHAPE_MAG * tparam;
	}

	/* Input offset value */
	if (p->shape0gam)
		w = XSHAPE_OFFG;
	else
		w = XSHAPE_OFFS;

	for (g = 0; g < 3; g++) {
		tt = v[9 + g];
		tt *= tt;
		tparam += w * tt;
	}
	/* Output ffset value */
	for (g = 0; g < 3; g++) {
		tt = v[12 + g];
		tt *= tt;
		tparam += w * tt;
	}
	/* Shaper values */
	for (f = 0; f < p->norders; f++) {
		/* Weigh to suppress ripples */
		if (f <= 1) {
			w = XSHAPE_HW01;
		} else if (f <= XSHAPE_HBREAK) {
			double bl = (f - 1.0)/(XSHAPE_HBREAK - 1.0);
			w = (1.0 - bl) * XSHAPE_HW01 + bl * XSHAPE_HWBR * p->smooth;
		} else {
			w = XSHAPE_HWBR + (f-XSHAPE_HBREAK) * XSHAPE_HWINC * p->smooth;
		}
		for (g = 0; g < 3; g++) {
			tt = v[15 + 3 * f + g];
			if (f == 0 && p->shape0gam)
				tt -= 1.0;			/* default is linear */
			tt *= tt;
			tparam += w * tt;
		}
	}
	return XSHAPE_MAG * tparam/3.0;
}

int g_deb = G_DEB;

/* Matrix optimisation function handed to powell() */
static double mxoptfunc(void *edata, double *v) {
	mxopt *p = (mxopt *)edata;
	double err = 0.0, rv = 0.0, smv;
	double xyz[3], lab[3];
	int i;

	if (g_deb) printf("\n");

	for (i = 0; i < p->nodp; i++) {

		/* Apply our function */
//printf("%f %f %f -> %f %f %f\n", p->points[i].p[0], p->points[i].p[1], p->points[i].p[2], xyz[0], xyz[1], xyz[2]);
		mxmfunc(p, v, xyz, p->points[i].p);
	
		/* Convert to Lab */
		icmXYZ2Lab(&p->wp, lab, xyz);
if (g_deb) printf("%d: %f %f %f -> %f %f %f, target %f %f %f, w %f\n", i, p->points[i].p[0], p->points[i].p[1], p->points[i].p[2], lab[0], lab[1], lab[2], p->points[i].v[0], p->points[i].v[1], p->points[i].v[2],p->points[i].w);
	
		/* Accumulate total delta E squared */
#ifdef USE_CIE94_DE
		rv += p->points[i].w * icmCIE94sq(lab, p->points[i].v);
#else
		rv += p->points[i].w * icmLabDEsq(lab, p->points[i].v);
#endif
	}

	/* Normalise error to be an average delta E squared */
	rv /= (double)p->nodp;

	/* Sum with shaper parameters squared, to */
	/* minimise unsconstrained "wiggles" */
	smv = xshapmag(p, v);
	rv += smv;

	/* Penalize if we have white > 1 or -ve black */ 
	if (p->clipbw) {
		double tp[3];

		tp[0] = tp[1] = tp[2] = 1.0;
		mxmfunc(p, v, xyz, tp);
		if ((xyz[1] - 1.0) > err)
			err = xyz[1] - 1.0;

		tp[0] = tp[1] = tp[2] = 0.0;
		mxmfunc(p, v, xyz, tp);
		for (i = 0; i < 3; i++) {
			if (-xyz[i] > err)
				err = -v[i];
		}
	}

	/* Penalize if we have -ve primaries */
	if (p->clipprims) {
		for (i = 0; i < 9; i++) {
			if (-v[i] > err)
				err = -v[i];
		}
	}
	rv += err * 1000.0;

#ifdef DEBUG
if (g_deb)
printf("~9(%f)mxoptfunc returning %f\n",smv,rv);
#endif

	return rv;
}

/* Matrix progress function handed to powell() */
static void mxprogfunc(void *pdata, int perc) {
	mxopt *p = (mxopt *)pdata;

	if (p->verb) {
		printf("%c% 3d%%",cr_char,perc); 
		if (perc == 100)
			printf("\n");
		fflush(stdout);
	}
}


/* Given a correction matrix, transform the matrix values */
static void mxtransform(mxopt *os, double mat[3][3]) { 
	double vec[3];
			
	vec[0] = os->v[0]; vec[1] = os->v[3]; vec[2] = os->v[6];
	icmMulBy3x3(vec, mat, vec);
	os->v[0] = vec[0]; os->v[3] = vec[1]; os->v[6] = vec[2];
			
	vec[0] = os->v[1]; vec[1] = os->v[4]; vec[2] = os->v[7];
	icmMulBy3x3(vec, mat, vec);
	os->v[1] = vec[0]; os->v[4] = vec[1]; os->v[7] = vec[2];
			
	vec[0] = os->v[2]; vec[1] = os->v[5]; vec[2] = os->v[8];
	icmMulBy3x3(vec, mat, vec);
	os->v[2] = vec[0]; os->v[5] = vec[1]; os->v[8] = vec[2];
}


/* Setup and then return the optimized matrix fit in the mxopt structure. */
/* Return 0 on sucess, error code on failure. */
static int
createMatrix(
char *err,			/* Return error message */
mxopt *os,			/* Optimisation information */
int verb,			/* NZ if verbose */
int nodp,			/* Number of points */
cow *ipoints,		/* Array of input points in XYZ space */
int isLab,			/* nz if data points are Lab */
int quality,		/* Quality metric, 0..3 (-1 == 2 orders only) */
int isLinear,		/* NZ if pure linear, gamma = 1.0 */
int isGamma,		/* NZ if gamma rather than shaper */
int isShTRC,		/* NZ if shared TRCs */
int shape0gam,      /* NZ if zero'th order shaper should be gamma function */
int clipbw,			/* Prevent white > 1 and -ve black */
int clipprims,		/* Prevent primaries going -ve */
double smooth,		/* Smoothing factor (nominal 1.0) */
double scale		/* Scale device values */
) {
	double nweight = 1.0;		/* Amount to weight neutral patches (make a parameter ?) */
	int inputChan = 3;			/* Must be RGB like */
	int outputChan = 3;			/* Must be the PCS */
	int rsplflags = 0;			/* Flags for scattered data rspl */
	int e, f, i, j;
	int maxits = 200;			/* Optimisation stop params */
	double stopon = 0.01;		/* Absolute delta E change to stop on */
	cow *points;				/* Lab copy of ipoints */
	double rerr;

#ifdef DEBUG_PLOT
	#define	XRES 100
	double xx[XRES];
	double y1[XRES];
#endif /* DEBUG_PLOT */

	if (verb)
		rsplflags |= RSPL_VERBOSE;

	/* Allocate the array passed to fit_rspl() */
	if ((points = (cow *)malloc(sizeof(cow) * nodp)) == NULL) {
		if (err != NULL)
			sprintf(err,"Allocation of scattered coordinate array failed");
		return 2;
	}

	/* Setup for optimising run */
	if (verb != 0)
		os->verb = verb;
	else
		os->verb = 0;
	os->points = points;
	os->nodp   = nodp;
	os->isShTRC = 0;
	os->shape0gam = shape0gam;
	os->smooth = smooth;
	os->clipbw = clipbw;
	os->clipprims = clipprims;
	os->dscale = scale;

	/* Set quality/effort  factors */
	if (quality >= 3) {			/* Ultra high */
		os->norders = 20;
		maxits = 50000;
		stopon = 1e-14;
	} else if (quality == 2) {	/* High */
		os->norders = 12;
		maxits = 5000;
		stopon = 5e-6;
	} else if (quality == 1) {	/* Medium */
		os->norders = 8;
		maxits = 2000;
		stopon = 5e-5;
	} else if (quality == 0) {  /* Low */
		os->norders = 4;
		maxits = 1000;
		stopon = 5e-4;
	} else {					/* Ultra Low */
		os->norders = 2;
		maxits = 1000;
		stopon = 5e-4;
	}
	if (os->norders > MXNORDERS)
		os->norders = MXNORDERS;
	
	/* Setup points ready for optimisation and do an initial Lab conversion */
	for (i = 0; i < nodp; i++) {
		for (e = 0; e < inputChan; e++)
			points[i].p[e] = ipoints[i].p[e];
		
		for (f = 0; f < outputChan; f++)
			points[i].v[f] = ipoints[i].v[f];

		points[i].w = ipoints[i].w;
	}

	/* Pick a white point for the real Lab conversion */
	{
		double wp[3];
		double wpy = -1e60;
		int wix = -1;

		/* We assume that the input target is well behaved, */
		/* and that it includes a white point patch, */
		/* and that it has an extreme L value */

		for (i = 0; i < nodp; i++) {
			double yv;

			/* Tilt things towards D50 neutral white patches */
			yv = points[i].v[0] - 0.3 * sqrt(points[i].v[1] * points[i].v[1]
			                                + points[i].v[2] * points[i].v[2]);
			if (yv > wpy) {
				wpy = yv;
				wix = i;
			}
		}
//printf("~1 picked point %d as white\n",wix);
		icmLab2XYZ(&icmD50, wp, points[wix].v);
		wp[0] /= wp[1];
		wp[2] /= wp[1];
		wp[1] = 1.0;
		icmAry2XYZ(os->wp, wp);

		/* We'll use this wp for delta E calculation when creating the matrix */
//		if (os->verb) printf("Switching to L*a*b* white point %f %f %f\n",os->wp.X,os->wp.Y,os->wp.Z);
		if (nweight < 1.0)		/* Sanity */
			nweight = 1.0;
		for (i = 0; i < nodp; i++) {
			double lch[3];
			if (isLab)
				icmLab2XYZ(&icmD50, points[i].v, points[i].v);
			icmXYZ2Lab(&os->wp, points[i].v, points[i].v);
			icmLab2LCh(lch, points[i].v);

			/* Apply any neutral weighting */
			if (lch[1] < 10.0) {
				double w = nweight;
				if (lch[1] > 5.0)
					w = 1.0 + (nweight - 1.0) * (10.0 - lch[1])/(10.0 - 5.0);
				points[i].w *= w;
			}
//printf("~1 patch %d = Lab %f %f %f, C = %f w = %f\n",i,points[i].v[0], points[i].v[1], points[i].v[2], lch[1],points[i].w);
		}

#if !defined(NOT_PRIVATE) && defined(HACK)
# pragma message("!!!!!!!!!!!!!!! xicc/xmatrix.c HACK code enabled !!!!!!!!!!!!!!!!!!")
		printf("!!!! HACK: setting white point ixt %d weight to zero\n",wix);
		points[wix].w = 0.0;
#endif
	}

	/* Set initial matrix optimisation values */
	os->v[0] = 0.4;  os->v[1] = 0.4;  os->v[2] = 0.2;		/* Matrix */
	os->v[3] = 0.2;  os->v[4] = 0.8;  os->v[5] = 0.1;
	os->v[6] = 0.02; os->v[7] = 0.15; os->v[8] = 1.3;

	/* We try and take a homomorphic approach here, in an attempt */
	/* to avoid getting trapped at a local minimum when a full */
	/* set of shaper parameters are in play. */
	
	/* Do a first pass just setting the matrix values */
	os->isLinear = 1;
	os->isGamma = 1;
	os->optdim = 9;
	os->v[9] = os->v[10] = os->v[11] = 1.0;					/* Linear */ 

	/* Set search area to starting values */
	for (j = 0; j < os->optdim; j++)
		os->sa[j] = 0.2;					/* Matrix, Gamma, Offsets, harmonics */

	if (os->verb)
		printf("Creating matrix...\n"); 

	if (powell(&rerr, os->optdim, os->v, os->sa, stopon, maxits,
	           mxoptfunc, (void *)os, mxprogfunc, (void *)os) != 0)
		warning("Powell failed to converge, residual error = %f",rerr);

#ifndef NEVER
	if (os->verb) {
		printf("Matrix = %f %f %f\n",os->v[0], os->v[1], os->v[2]);
		printf("         %f %f %f\n",os->v[3], os->v[4], os->v[5]);
		printf("         %f %f %f\n",os->v[6], os->v[7], os->v[8]);
	}
#endif /* NEVER */

	/* Now optimize again with shaper or gamma curves */
	if (!isLinear) {
		double scgamma;

		/* Start from linear, which is what was assumed for the matrix fit, */
		/* and fit with a single shared gamma curve. */
		os->isShTRC = 1;
		os->isLinear = 0;
		os->isGamma = 1;
		os->optdim = 10;
		os->v[9] = 1.0;		/* Linear */ 

		/* Set search area to starting values */
		for (j = 0; j < os->optdim; j++)
			os->sa[j] = 0.2;					/* Matrix, Gamma, Offsets, harmonics */

		if (os->verb)
			printf("Creating matrix and single gamma curve...\n"); 

		if (powell(&rerr, os->optdim, os->v, os->sa, stopon, maxits,
		           mxoptfunc, (void *)os, mxprogfunc, (void *)os) != 0)
			warning("Powell failed to converge, residual error = %f",rerr);

		scgamma = os->v[9];
		if (isShTRC && !isGamma) {
		
#ifndef NEVER
			if (os->verb) {
				printf("Matrix = %f %f %f\n",os->v[0], os->v[1], os->v[2]);
				printf("         %f %f %f\n",os->v[3], os->v[4], os->v[5]);
				printf("         %f %f %f\n",os->v[6], os->v[7], os->v[8]);
				printf("Gamma = %f\n",os->v[9]);
			}
#endif /* NEVER */

			/* Do final optimisation using full curve capability */
			/* and fit first with a single shared curve. */
			os->isShTRC = 1;
			os->isLinear = 0;
			os->isGamma = 0;
			os->optdim = 9 + 2 + os->norders;			/* Matrix, offset + orders */
			os->v[9] = 0.0;		/* Input offset */
			os->v[10] = 0.0;	/* Output offset */
			if (shape0gam)
				os->v[11] = 1.0; 	/* Gamma */
			else
				os->v[11] = 0.0; 	/* 0th Harmonic */
			for (i = 12; i < os->optdim; i++)
				os->v[i] = 0.0; 	/* Higher orders */

			/* Set search area to starting values */
			for (j = 0; j < os->optdim; j++)
				os->sa[j] = 0.2;					/* Matrix, Gamma, Offsets, harmonics */

			if (os->verb)
				printf("Creating matrix and single shaper curve...\n"); 

			if (powell(&rerr, os->optdim, os->v, os->sa, stopon, maxits,
			           mxoptfunc, (void *)os, mxprogfunc, (void *)os) != 0)
				warning("Powell failed to converge, residual error = %f",rerr);

			scgamma = os->v[9];

		}

		/* For multiple curves, continue fitting */
		if (!isShTRC) {
			double mcgamma[3];

#ifndef NEVER
			if (os->verb) {
				printf("Matrix = %f %f %f\n",os->v[0], os->v[1], os->v[2]);
				printf("         %f %f %f\n",os->v[3], os->v[4], os->v[5]);
				printf("         %f %f %f\n",os->v[6], os->v[7], os->v[8]);
				printf("Gamma = %f\n",os->v[9]);
			}
#endif /* NEVER */

			/* Fit matrix + multi gamma curves */
			os->isShTRC = 0;
			os->isLinear = 0;
			os->isGamma = 1;
			os->optdim = 12;
			os->v[9] = os->v[10] = os->v[11] = scgamma;	/* Single curve value */
	
			/* Set search area to starting values */
			for (j = 0; j < os->optdim; j++)
				os->sa[j] = 0.2;					/* Matrix, Gamma, Offsets, harmonics */
	
			if (os->verb)
				printf("Creating matrix and gamma curves...\n"); 
	
			if (powell(&rerr, os->optdim, os->v, os->sa, stopon, maxits,
			           mxoptfunc, (void *)os, mxprogfunc, (void *)os) != 0)
				warning("Powell failed to converge, residual error = %f",rerr);

		
			mcgamma[0] = os->v[9];
			mcgamma[1] = os->v[10];
			mcgamma[2] = os->v[11];

			if (!isGamma) {

#ifndef NEVER
				if (os->verb) {
					printf("Matrix = %f %f %f\n",os->v[0], os->v[1], os->v[2]);
					printf("         %f %f %f\n",os->v[3], os->v[4], os->v[5]);
					printf("         %f %f %f\n",os->v[6], os->v[7], os->v[8]);
					printf("Gamma = %f %f %f\n",os->v[9], os->v[10], os->v[11]);
				}
#endif /* NEVER */

				/* Do final curves */
				os->isShTRC = 0;
				os->isLinear = 0;
				os->isGamma = 0;
				os->optdim = 9 + 6 + 3 * os->norders;		/* Matrix, offset + orders */
				os->v[9] = os->v[10] = os->v[11] = 0.0;		/* Input offset */
				os->v[12] = os->v[13] = os->v[14] = 0.0;	/* Output offset */
				if (shape0gam) {
					os->v[15] = mcgamma[0];
					os->v[16] = mcgamma[1];
					os->v[17] = mcgamma[2];
				} else
					os->v[15] = os->v[16] = os->v[17] = 0.0; 	/* 0th Harmonic */
				for (i = 18; i < os->optdim; i++)
					os->v[i] = 0.0; 						/* Higher orders */
		
				/* Set search area to starting values */
				for (j = 0; j < os->optdim; j++)
					os->sa[j] = 0.1;					/* Matrix, Gamma, Offsets, harmonics */
		
				if (os->verb)
					printf("Creating matrix and curves...\n"); 
		
//g_deb = 1;
				if (powell(&rerr, os->optdim, os->v, os->sa, stopon, maxits,
				           mxoptfunc, (void *)os, mxprogfunc, (void *)os) != 0)
					warning("Powell failed to converge, residual error = %f",rerr);
			}
		}
	}
	if (os->clipprims) { /* Clip -ve primaries */
		for (i = 0; i < 9; i++) {
			if (os->v[i] < 0.0)
				os->v[i] = 0.0;
		}
	}

#ifndef NEVER
	if (os->verb) {
		printf("Matrix = %f %f %f\n",os->v[0], os->v[1], os->v[2]);
		printf("         %f %f %f\n",os->v[3], os->v[4], os->v[5]);
		printf("         %f %f %f\n",os->v[6], os->v[7], os->v[8]);
		if (!isLinear) {		/* Creating input curves */
			if (os->isGamma) {		/* Creating input curves */
				if (isShTRC) 
					printf("Gamma = %f\n",os->v[9]);
				else
					printf("Gamma = %f %f %f\n",os->v[9], os->v[10], os->v[11]);
			} else {		/* Creating input curves */
				if (isShTRC) { 
					printf("Input offset  = %f\n",os->v[9]);
					printf("Output offset = %f\n",os->v[10]);
				} else {
					printf("Input offset  = %f %f %f\n",os->v[9], os->v[10], os->v[11]);
					printf("Output offset = %f %f %f\n",os->v[12], os->v[13], os->v[14]);
				}
				for (j = 0; j < os->norders; j++) {
					if (isShTRC) { 
						if (shape0gam && j == 0)
							printf("gamma = %f\n", os->v[11 + j]);
						else
							printf("%d harmonics = %f\n",j, os->v[11 + j]);
					} else {
						if (shape0gam && j == 0)
							printf("%d gamma = %f %f %f\n",j, os->v[15 + j * 3],
							                    os->v[16 + j * 3], os->v[17 + j * 3]);
						else
							printf("%d harmonics = %f %f %f\n",j, os->v[15 + j * 3],
							                    os->v[16 + j * 3], os->v[17 + j * 3]);
					}
				}
			}
		}
	}
#endif /* NEVER */
#ifdef NEVER	/* Check DE of fit */
	{
		double xyz[3], txyz[3];

		for (i = 0; i < nodp; i++) {

			mxmfunc(os, os->v, xyz, ipoints[i].p);

			if (isLab)
				icmLab2XYZ(&icmD50, txyz, ipoints[i].v);
			else
				icmCpy3(txyz, ipoints[i].v);

			printf("~1 point %d DE %f\n", i, icmXYZLabDE(&icmD50, txyz, xyz));
		}
	}
#endif

	/* Free the coordinate lists */
	free(points);

	return 0;
}

/* Apply a chromatic transform to the matrix to force the given */
/* xyz value (typically white) to be exact */
static void icxMM_force_exact(icxMatrixModel *p, double *targ, double *rgb) {
	mxopt *os = (mxopt *)p->imp;
	double txyz[3], axyz[3];	/* Target & actual xyz */
	icmXYZNumber _tp, _ap;
	double cmat[3][3];			/* Model transform matrix */

	if (p->isLab)
		icmLab2XYZ(&icmD50, txyz, targ);
	else
		icmCpy3(txyz, targ);

	mxmfunc(os, os->v, axyz, rgb);

	icmAry2XYZ(_ap, axyz);
	icmAry2XYZ(_tp, txyz);
	if (p->picc != NULL)
		p->picc->chromAdaptMatrix(p->picc, ICM_CAM_NONE, _tp, _ap, cmat);
	else
		icmChromAdaptMatrix(ICM_CAM_BRADFORD, _tp, _ap, cmat);

	/* Apply correction to fine tune matrix. */
	mxtransform(os, cmat);
}

static void icxMM_lookup(icxMatrixModel *p, double *out, double *in) {
	mxopt *os = (mxopt *)p->imp;

	mxmfunc(os, os->v, out, in);

	if (p->isLab)
		icmXYZ2Lab(&icmD50, out, out);
}

static void icxMM_del(icxMatrixModel *p) {
	free(p->imp);
	free(p);
}

/* Create a matrix model of a set of points, and return an object to lookup */
/* points from the model. Return NULL on error. */
icxMatrixModel *new_MatrixModel(
icc *picc,			/* ICC profile used to set cone space matrix, NULL for Bradford. */
int verb,			/* NZ if verbose */
int nodp,			/* Number of points */
cow *ipoints,		/* Array of input points in XYZ space */
int isLab,			/* nz if data points are Lab */
int quality,		/* Quality metric, 0..3  (-1 == 2 orders only) */
int isLinear,		/* NZ if pure linear, gamma = 1.0 */
int isGamma,		/* NZ if gamma rather than shaper */
int isShTRC,		/* NZ if shared TRCs */
int shape0gam,      /* NZ if zero'th order shaper should be gamma function */
int clipbw,			/* Prevent white > 1 and -ve black */
int clipprims,		/* Prevent primaries going -ve */
double smooth,		/* Smoothing factor (nominal 1.0) */
double scale		/* Scale device values */
) {
	icxMatrixModel *p;

	if ((p = (icxMatrixModel *) calloc(1,sizeof(icxMatrixModel))) == NULL)
		return NULL;

	p->picc = picc;
	p->force = icxMM_force_exact;
	p->lookup = icxMM_lookup;
	p->del = icxMM_del;

	if ((p->imp = (void *) calloc(1,sizeof(mxopt))) == NULL) {
		free(p);
		return NULL;
	}

	if (createMatrix(NULL, (mxopt *)p->imp, verb, nodp, ipoints, isLab, quality,
	                 isLinear, isGamma, isShTRC, shape0gam,
		             clipbw, clipprims, smooth, scale) != 0) {
		free(p->imp);
		free(p);
		return NULL;
	}

	p->isLab = isLab;

	return p;
}

/* Create icxLuMatrix and undelying tone reproduction curves and */
/* colorant tags, initialised from the icc, and then overwritten */
/* by a conversion created from the supplied scattered data points. */

/* The scattered data is assumed to map Device -> native PCS (ie. dir = Fwd) */
/* NOTE:- in theory once this icxLuMatrix is setup, it can be */
/* called to translate color values. In practice I suspect */
/* that the icxLuMatrix hasn't been setup completely enough to allows this. */
/* Might be easier to close it and re-open it ? */
static icxLuBase *
set_icxLuMatrix(
xicc               *xicp,
icmLuBase          *plu,			/* Pointer to Lu we are expanding (ours) */	
int                flags,			/* white/black point flags */
int                nodp,			/* Number of points */
int                nodpbw,			/* Number of points to look for white  & black patches in */
cow                *ipoints,		/* Array of input points in XYZ space */
icxMatrixModel     *skm,    		/* Optional skeleton model (not used here) */
double             dispLuminance,	/* > 0.0 if display luminance value and is known */
double             wpscale,			/* > 0.0 if input white point is to be scaled */
//double             *bpo,			/* != NULL for XYZ black point override dev & XYZ */
int                quality,			/* Quality metric, 0..3 */
double             smooth			/* Curve smoothing, nominally 1.0 */
) {
	icxLuMatrix *p;						/* Object being created */
	icc *icco = xicp->pp;				/* Underlying icc object */
	icmLuMatrix *pmlu = (icmLuMatrix *)plu;	/* icc matrix lookup object */
	int luflags = 0;					/* icxLuMatrix alloc clip, merge flags */
	int isLinear = 0;					/* NZ if pure linear, gamma = 1.0 */
	int isGamma = 0;					/* NZ if gamma rather than shaper */
	int isShTRC = 0;					/* NZ if shared TRCs */
	int inputChan = 3;					/* Must be RGB like */
	int outputChan = 3;					/* Must be the PCS */
	icmHeader *h = icco->header;		/* Pointer to icc header */
	int rsplflags = 0;					/* Flags for scattered data rspl */
	int e, f, i, j;
	int maxits = 200;					/* Optimisation stop params */
	double stopon = 0.01;				/* Absolute delta E change to stop on */
	mxopt os;							/* Optimisation information */
	double rerr;
						/* If ICX_SET_WHITE | ICX_SET_BLACK: */
	double wp[3];		/* Absolute White point in XYZ */
	double bp[3];		/* Absolute Black point in XYZ */
	double dw[MXDI];	/* Device white value to adjust to be D50 */
	double db[MXDI];	/* Device balck value */
	double dgw[3];		/* Device space gamut boundary white for ICX_SET_WHITE_US */
	double fromAbs[3][3];	/* From abs to relative */
	double toAbs[3][3];		/* To abs from relative */
	cow *rpoints = NULL;	/* Aprox. relative in->output values */

#ifdef DEBUG_PLOT
	#define	XRES 100
	double xx[XRES];
	double y1[XRES];
#endif /* DEBUG_PLOT */

	if (flags & ICX_VERBOSE)
		rsplflags |= RSPL_VERBOSE;

	luflags = flags;		/* Transfer straight though ? */

	/* Check out some things about the profile */
	{
		icmCurve *wor, *wog, *wob;
		wor = pmlu->redCurve;
		wog = pmlu->greenCurve;
		wob = pmlu->blueCurve;

		if (wor == wog) {
			if (wog != wob) {
				xicp->errc = 1;
				sprintf(xicp->err,"icx_set_matrix: TRC sharing is inconsistent");
				return NULL;
			}
			isShTRC = 1;
		}
		if (wor->flag != wog->flag || wog->flag != wob->flag) {
			xicp->errc = 1;
			sprintf(xicp->err,"icx_set_matrix: TRC type is inconsistent");
			return NULL;
		}
		if (wor->flag == icmCurveGamma) {
			isGamma = 1;
		}

		if (flags & ICX_NO_IN_SHP_LUTS) {
			isLinear = 1;
		}
	}

	/* Do basic icxLu creation and initialisation */
	if ((p = alloc_icxLuMatrix(xicp, plu, 0, luflags)) == NULL) {
		xicp->errc = 1;
		sprintf(xicp->err,"icx_set_matrix: malloc failed");
		return NULL;
	}

	p->func = icmFwd;		/* Assumed by caller */

	/* Get the effective spaces of underlying icm, and set icx the same */
	plu->spaces(plu, &p->ins, NULL, &p->outs, NULL, NULL, &p->intent, NULL, &p->pcs, NULL);

	/* For set_icx the effective pcs has to be the same as the native pcs */

	/* Sanity check for matrix */
	if (p->pcs != icSigXYZData) {
		p->pp->errc = 1;
		sprintf(p->pp->err,"Can't create matrix profile with PCS of Lab !");
		p->del((icxLuBase *)p);
		return NULL;
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

	/* ------------------------------- */

	/* Choose a white and black point */
	if (flags & (ICX_SET_WHITE | ICX_SET_BLACK)) {

		if (flags & ICX_VERBOSE)
			printf("Find white & black points\n");

		/* Compute device white and black points as if */
		/* we are doing an Output or Display device */
		{
			switch (h->colorSpace) {
	
				case icSigCmyData:
					for (e = 0; e < p->inputChan; e++) {
						dw[e] = 0.0;
						db[e] = 1.0;
					}
					break;
				case icSigRgbData:
					for (e = 0; e < p->inputChan; e++) {
						dw[e] = 1.0;
						db[e] = 0.0;
					}
					break;
	
				default: {
					xicp->errc = 1;
					sprintf(xicp->err,"set_icxLuMatrix: can't handle color space %s",
					                           icm2str(icmColorSpaceSignature, h->colorSpace));
					p->del((icxLuBase *)p);
					return NULL;
					break;
				}
			}
		}

		/* dw is what we want for dgw[], used for XFIT_OUT_WP_REL_US */
		for (e = 0; e < p->inputChan; e++)
			dgw[e] = dw[e];

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
				icmXYZ2Lab(&icmD50, labv, ipoints[i].v);

#ifdef NEVER
				/* Choose Y */
				if (ipoints[i].v[1] > wpy) {
					wp[0] = ipoints[i].v[0];
					wp[1] = ipoints[i].v[1];
					wp[2] = ipoints[i].v[2];
					for (e = 0; e < p->inputChan; e++)
						dw[e] = ipoints[i].p[e];
					wpy = ipoints[i].v[1];
					wix = i;
				}
#else
				
				/* Tilt things towards D50 neutral white patches */
				yv = labv[0] - 0.3 * sqrt(labv[1] * labv[1] + labv[2] * labv[2]);
				if (yv > wpy) {
					wp[0] = ipoints[i].v[0];
					wp[1] = ipoints[i].v[1];
					wp[2] = ipoints[i].v[2];
					for (e = 0; e < p->inputChan; e++)
						dw[e] = ipoints[i].p[e];
					wpy = yv;
					wix = i;
				}
#endif
				if (ipoints[i].v[1] < bpy) {
					bp[0] = ipoints[i].v[0];
					bp[1] = ipoints[i].v[1];
					bp[2] = ipoints[i].v[2];
					for (e = 0; e < p->inputChan; e++)
						db[e] = ipoints[i].p[e];
					bpy = ipoints[i].v[1];
					bix = i;
				}
			}
			if (flags & ICX_VERBOSE) {
				printf("Picked white patch %d with dev = %s\n       XYZ = %s, Lab = %s\n",
				        wix+1, icmPdv(p->inputChan, dw), icmPdv(3, wp), icmPLab(wp));
				printf("Picked black patch %d with dev = %s\n       XYZ = %s, Lab = %s\n",
				        bix+1, icmPdv(p->inputChan, db), icmPdv(3, bp), icmPLab(bp));
			}

		} else {
			/* We assume that the display target is well behaved, */
			/* and that it includes a white point patch. */
			int nw = 0;

			wp[0] = wp[1] = wp[2] = 0.0;

			switch (h->colorSpace) {
	
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
					break;
	
				default:
					xicp->errc = 1;
					sprintf(xicp->err,"set_icxLuMatrix: can't handle color space %s",
					                           icm2str(icmColorSpaceSignature, h->colorSpace));
					p->del((icxLuBase *)p);
					return NULL;
					break;
			}

			if (nw == 0) {
				xicp->errc = 1;
				sprintf(xicp->err,"set_icxLuMatrix: can't handle test points without a white patch");
				p->del((icxLuBase *)p);
				return NULL;
			}
			wp[0] /= (double)nw;
			wp[1] /= (double)nw;
			wp[2] /= (double)nw;

			if (flags & ICX_VERBOSE) {
				printf("Initial white point = %f %f %f\n",wp[0],wp[1],wp[2]);
			}

			/* Need to lookup bp[] before we set the tag */
		}

		/* Create some abs<->rel chromatic conversions */
		{
			icmXYZNumber _wp;
			icmAry2XYZ(_wp, wp);
	
			/* Absolute->Aprox. Relative Adaptation matrix */
			icco->chromAdaptMatrix(icco, ICM_CAM_NONE, icmD50, _wp, fromAbs);
		
			/* Aproximate relative to absolute conversion matrix */
			icco->chromAdaptMatrix(icco, ICM_CAM_NONE, _wp, icmD50, toAbs);
		}

	} else {
		icmSetUnity3x3(fromAbs);
		icmSetUnity3x3(toAbs);
	}

	/* Create copy of input points with output converted to white relative */
	/* Allow one extra point for possible bpo value */
	if ((rpoints = (cow *)malloc((nodp+1) * sizeof(cow))) == NULL) {
		xicp->errc = 1;
		sprintf(xicp->err,"set_icxLuMatrix: malloc failed");
		p->del((icxLuBase *)p);
		return NULL;
	}
	for (i = 0; i < nodp; i++) {
		rpoints[i].w = ipoints[i].w;
		for (e = 0; e < inputChan; e++)
			rpoints[i].p[e] = ipoints[i].p[e];
		for (f = 0; f < outputChan; f++)
			rpoints[i].v[f] = ipoints[i].v[f];

		/* abs out -> aprox. rel out */
		icmMulBy3x3(rpoints[i].v, fromAbs, rpoints[i].v);
	}

#ifdef NEVER
	/* If black point override and shaper curves */
	if (bpo != NULL && !isLinear && !isGamma) {
		double tw = 0.0;		/* Total weight */

printf("Got bpo\n");
		/* Zero out any black data points, and sum up total weihting */
		for (i = 0; i < nodp; i++) {
			if (rpoints[i].p[0] < 0.001				/* We're assuming RGB */
			 && rpoints[i].p[1] < 0.001
			 && rpoints[i].p[2] < 0.001) {
				rpoints[i].w = 0.0;
printf("Zero'd point %d\n",i);
			}
			tw += rpoints[i].w;
		}
printf("Total weight = %f\n",tw);

		/* Add our override black point */
		/* and give it a dominant weighting */
		for (e = 0; e < inputChan; e++)
			rpoints[nodp].p[e] = 0.0;
		for (f = 0; f < outputChan; f++)
			rpoints[nodp].v[f] = bpo[f];
printf(" set black to %f %f %f\n", bpo[0], bpo[1], bpo[2]);

		/* abs out -> aprox. rel out */
		icmMulBy3x3(rpoints[nodp].v, fromAbs, rpoints[nodp].v);

		rpoints[nodp].w = 20.0 * tw;
printf(" set black %d w = %f\n", nodp,rpoints[nodp].w);

		nodp++;
	}
#endif // NEVER

	/* ------------------------------- */

	/* (Use a gamma curve as 0th order shape) */
	if ((p->pp->errc = createMatrix(p->pp->err, &os, flags & ICX_VERBOSE ? 1 : 0,  
	                               nodp, rpoints, 0, quality,
	                               isLinear, isGamma, isShTRC, 1,
		                           flags & ICX_CLIP_WB ? 1 : 0,
		                           flags & ICX_CLIP_PRIMS ? 1 : 0,
		                           smooth, 1.0)) != 0) {   
		free(rpoints);
		p->del((icxLuBase *)p);
		return NULL;
	}
	free(rpoints); rpoints = NULL;

#if !defined(NOT_PRIVATE) && defined(HACK)
# pragma message("!!!!!!!!!!!!!!! xicc/xmatrix.c HACK code enabled !!!!!!!!!!!!!!!!!!")
		printf("!!!! HACK: skipping white point fine tune\n");
#else
	/* The overall device to absolute conversion is now what we want */
	/* (as dictated by the points, weighting and best fit), */
	/* but we need to adjust the device to relative conversion */
	/* to make device white map exactly to D50, without touching */
	/* the overall absolute behaviour. */
	if (p->flags & ICX_SET_WHITE) {
		double aw[3];				/* aprox rel. white */
		icmXYZNumber _wp;			/* Uncorrected dw maps to _wp */ 
		double cmat[3][3];			/* Model correction matrix */

		if (flags & ICX_VERBOSE)
			printf("Doing White point fine tune:\n");


		/* See what the aprox. relative white point has turned out to be, */
		/* by looking up the device white in the current conversion */
		mxmfunc(&os, os.v, aw, dw);
	
		if (flags & ICX_VERBOSE) {
			printf("Before fine tune, rel WP = XYZ %s, Lab %s\n", icmPdv(3,aw), icmPLab(aw));
		}
	
		/* Matrix needed to correct aprox white to target D50 */
		icmAry2XYZ(_wp, aw);		/* Aprox relative target white point */
		icco->chromAdaptMatrix(icco, ICM_CAM_NONE, icmD50, _wp, cmat);	/* Correction */
	
		/* Compute the current absolute white point */
		icmMulBy3x3(wp, toAbs, aw);

		/* Apply correction to fine tune matrix. */
		mxtransform(&os, cmat);
	
		/* Fix relative conversions to leave absolute response unchanged. */
		icmAry2XYZ(_wp, wp);		/* Actual white point */
		icco->chromAdaptMatrix(icco, ICM_CAM_NONE, icmD50, _wp, fromAbs);
		icco->chromAdaptMatrix(icco, ICM_CAM_NONE, _wp, icmD50, toAbs);

		if (flags & ICX_VERBOSE) {
			double tw[3];
			mxmfunc(&os, os.v, tw, dw); /* Lookup white again */
			printf("After fine tune, rel WP = XYZ %s, Lab %s\n", icmPdv(3, tw), icmPLab(tw));
			printf("                 abs WP = XYZ %s, Lab %s\n", icmPdv(3, wp), icmPLab(wp));
		}
	}
#endif

	/* Create default wpscale */
	if (wpscale < 0.0) {
		wpscale = 1.0;
	} else {
		if (flags & ICX_VERBOSE) {
			printf("White manual point scale %f\n", wpscale);
		}
	}

	/* If we are going to auto scale the WP to avoid clipping */
	/* values above the WP: (not so important for matrix profiles ?) */
	if ((p->flags & ICX_SET_WHITE_US) == ICX_SET_WHITE_US) {
		double tw[3], bw[3];
		icmXYZNumber _wp;
		double uswpscale = 1.0;
		double mxd, mxY;
		double ndw[3];

		/* See what device space gamut boundary white (ie. 1,1,1) maps to */
		mxmfunc(&os, os.v, tw, dgw);
		icmMulBy3x3(tw, toAbs, tw);	/* Convert to absolute */

		mxY = tw[1];
		icmCpy3(bw, tw);
//printf("~1 1,1,1 Y = %f\n",tw[1]);

		/* See what the device white point value scaled to 1 produces */
		mxd = -1.0;
		for (e = 0; e < inputChan; e++) {
			if (dw[e] > mxd)
				mxd = dw[e];
		}
		for (e = 0; e < inputChan; e++)
			ndw[e] = dw[e]/mxd;

		mxmfunc(&os, os.v, tw, ndw);
		icmMulBy3x3(tw, toAbs, tw);	/* Convert to absolute */

//printf("~1 ndw = %f %f %f Y = %f\n",ndw[0],ndw[1],ndw[2],tw[1]);
		if (tw[1] > mxY) {
			mxY = tw[1];
			icmCpy3(bw, tw);
		}

		/* Compute WP scale factor needed to fit mxY */
		if (mxY > wp[1]) {
			uswpscale = mxY/wp[1];
			wpscale *= uswpscale;
			if (flags & ICX_VERBOSE) {
				printf("Dev boundary white XYZ %s, scale WP by %f, total WP scale %f\n",
				icmPdv(3, bw), uswpscale, wpscale);
			}
		}
	}

	/* If the scaled WP would have Y > 1.0, clip it to 1.0 */
	if (p->flags & ICX_CLIP_WB) {

		if ((wp[1] * wpscale) > 1.0) {
			wpscale = 1.0/wp[1];		/* Make wp Y = 1.0 */		
			if (flags & ICX_VERBOSE) {
				printf("WP Y would ve > 1.0. scale by %f to clip it\n",wpscale);
			}
		}
	}

	/* Apply our total wp scale factor */
	if (wpscale != 1.0) {
		icmXYZNumber _wp;
		double cmat[3][3];			/* Model correction matrix */

		/* Create inverse scaling matrix for relative rspl data */
		icmSetUnity3x3(cmat);
		icmScale3x3(cmat, cmat, 1.0/wpscale);

		/* Inverse scale the matrix */
		mxtransform(&os, cmat);

		/* Scale the WP */
		icmScale3(wp, wp, wpscale);

		/* Fix absolute conversions to leave absolute response unchanged. */
		icmAry2XYZ(_wp, wp);		/* Actual white point */
		icco->chromAdaptMatrix(icco, ICM_CAM_NONE, icmD50, _wp, fromAbs);
		icco->chromAdaptMatrix(icco, ICM_CAM_NONE, _wp, icmD50, toAbs);
	}

	/* Look up the actual black point */
	if (p->flags & ICX_SET_BLACK) {

		/* Look black point up in dev->rel model */
		mxmfunc(&os, os.v, bp, db);

		/* Convert from relative to Absolute colorimetric */
		icmMulBy3x3(bp, toAbs, bp);

		
		/* Got XYZ black point in bp[] */
		if (flags & ICX_VERBOSE) {
			printf("Black point XYZ = %s, Lab = %s\n", icmPdv(3,bp),icmPLab(bp));
		}

		if (flags & ICX_CLIP_WB) {
			if (bp[0] < 0.0 || bp[1] < 0.0 || bp[1] < 0.0) {
				if (bp[0] < 0.0)
					bp[0] = 0.0;
				if (bp[1] < 0.0)
					bp[1] = 0.0;
				if (bp[2] < 0.0)
					bp[2] = 0.0;
				if (flags & ICX_VERBOSE)
					printf("Black point clipped to XYZ = %s, Lab = %s\n",icmPdv(3,bp),icmPLab(bp));
			}
		}
	}

	if (flags & (ICX_SET_WHITE | ICX_SET_BLACK)) {

		/* If this is a display, adjust the absolute white point to be */
		/* exactly Y = 1.0, and compensate the matrix, dispLuminance */
		/* and black point accordingly. */
		if (h->deviceClass == icSigDisplayClass) {
			double cmat[3][3];			/* Model correction matrix */
			double scale = 1.0/wp[1];

			if (flags & ICX_VERBOSE)
				printf("Scaling White Point by %f to make Y = 1.0\n", scale);

			/* Scale the WP & BP*/
			icmScale3(wp, wp, scale);
			icmScale3(bp, bp, scale);

			/* Inverse scale the luminance */
			dispLuminance /= scale;
		}

		/* Absolute luminance tag */
		if (flags & ICX_WRITE_WBL
		 && h->deviceClass == icSigDisplayClass
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
			wo->data[0].Y = dispLuminance;
			wo->data[0].Z = 0.0;

			if (flags & ICX_VERBOSE)
				printf("Display Luminance = %f\n", wo->data[0].Y);
		}

		/* Write white and black tags */
		if ((flags & ICX_WRITE_WBL)
		 && (flags & ICX_SET_WHITE)) { /* White Point Tag: */
			icmXYZArray *wo;
			if ((wo = (icmXYZArray *)icco->read_tag(
			           icco, icSigMediaWhitePointTag)) == NULL)  {
				xicp->errc = 1;
				sprintf(xicp->err,"icx_set_white_black: couldn't find white tag");
				p->del((icxLuBase *)p);
				return NULL;
			}
			if (wo->ttype != icSigXYZArrayType) {
				xicp->errc = 1;
				sprintf(xicp->err,"icx_set_white_black: white tag has wrong type");
				p->del((icxLuBase *)p);
				return NULL;
			}

			wo->size = 1;
			wo->allocate((icmBase *)wo);	/* Allocate space */
			wo->data[0].X = wp[0];
			wo->data[0].Y = wp[1];
			wo->data[0].Z = wp[2];

			if (flags & ICX_VERBOSE)
				printf("White point XYZ = %f %f %f\n",wp[0],wp[1],wp[2]);
		}
		if ((flags & ICX_WRITE_WBL)
		 && (flags & ICX_SET_BLACK)) { /* Black Point Tag: */
			icmXYZArray *wo;
			if ((wo = (icmXYZArray *)icco->read_tag(
			           icco, icSigMediaBlackPointTag)) == NULL)  {
				xicp->errc = 1;
				sprintf(xicp->err,"icx_set_white_black: couldn't find black tag");
				p->del((icxLuBase *)p);
				return NULL;
				}
			if (wo->ttype != icSigXYZArrayType) {
				xicp->errc = 1;
				sprintf(xicp->err,"icx_set_white_black: black tag has wrong type");
				p->del((icxLuBase *)p);
				return NULL;
			}

			wo->size = 1;
			wo->allocate((icmBase *)wo);	/* Allocate space */
			wo->data[0].X = bp[0];
			wo->data[0].Y = bp[1];
			wo->data[0].Z = bp[2];

			if (flags & ICX_VERBOSE)
				printf("Black point XYZ = %f %f %f\n",bp[0],bp[1],bp[2]);
		}

		// ~~99
		if (flags & ICX_CLIP_PRIMS) {
			for (i = 0; i < 9; i++) {
				if (os.v[i] < 0.0)
					os.v[i] = 0.0;
			}
		}
	}

	if (flags & ICX_VERBOSE)
		printf("Done gamma/shaper and matrix creation\n");

	/* Write the gamma/shaper and matrix to the icc memory structures */
	if (!isGamma) {		/* Creating input curves */
		unsigned int ui;
		icmCurve *wor, *wog, *wob;
		wor = pmlu->redCurve;
		wog = pmlu->greenCurve;
		wob = pmlu->blueCurve;

		for (ui = 0; ui < wor->size; ui++) {
			double in, rgb[3];

			for (j = 0; j < 3; j++) {

				in = (double)ui / (wor->size - 1.0);
	
				mxmfunc1(&os, j, os.v, &rgb[j], &in);

				if (rgb[j] < 0.0)
					rgb[j] = 0.0;
				else if (rgb[j] > 1.0)
					rgb[j] = 1.0;
			}
			wor->data[ui] = rgb[0];	/* Curve values 0.0 - 1.0 */
			if (!isShTRC) {
				wog->data[ui] = rgb[1];
				wob->data[ui] = rgb[2];
			}
		}
#ifdef DEBUG_PLOT
		/* Display the result fit */
		for (j = 0; j < 3; j++) {
			for (i = 0; i < XRES; i++) {
				double x, y;
				xx[i] = x = i/(double)(XRES-1);
				mxmfunc1(&os, j, os.v, &y, &x);
				if (y < 0.0)
					y = 0.0;
				else if (y > 1.0)
					y = 1.0;
				y1[i] = y;
			}
			do_plot(xx,y1,NULL,NULL,XRES);
		}
#endif /* DEBUG_PLOT */


	} else {			/* Gamma */
		icmCurve *wor, *wog, *wob;
		wor = pmlu->redCurve;
		wog = pmlu->greenCurve;
		wob = pmlu->blueCurve;
		wor->data[0] = os.v[9];	/* Gamma values */
		if (!isShTRC) {
			wog->data[0] = os.v[10];
			wob->data[0] = os.v[11];
		}
	}

	/* Matrix values */
	{
		icmXYZArray *wor, *wog, *wob;
		double mat[3][3];
		wor = pmlu->redColrnt;
		wog = pmlu->greenColrnt;
		wob = pmlu->blueColrnt;

		/* Copy to mat[RGB][XYZ] */
		mat[0][0] = os.v[0];
		mat[0][1] = os.v[3];
		mat[0][2] = os.v[6];
		mat[1][0] = os.v[1];
		mat[1][1] = os.v[4];
		mat[1][2] = os.v[7];
		mat[2][0] = os.v[2];
		mat[2][1] = os.v[5];
		mat[2][2] = os.v[8];

		/* Make sure rounding doesn't wreck white point */
		quantizeRGBprimsS15Fixed16(mat);

		wor->data[0].X = mat[0][0]; wor->data[0].Y = mat[0][1]; wor->data[0].Z = mat[0][2];
		wog->data[0].X = mat[1][0]; wog->data[0].Y = mat[1][1]; wog->data[0].Z = mat[1][2];
		wob->data[0].X = mat[2][0]; wob->data[0].Y = mat[2][1]; wob->data[0].Z = mat[2][2];

		/* Load into pmlu matrix and inverse ??? */
	}

	if (flags & ICX_VERBOSE)
		printf("Profile done\n");

	return (icxLuBase *)p;
}

/* ========================================================= */

/* Given an xicc lookup object, returm a gamut object. */
/* Note that the PCS must be Lab or Jab */
/* Return NULL on error, check errc+err for reason */
static gamut *icxLuMatrixGamut(
icxLuBase   *plu,		/* this */
double       detail		/* gamut detail level, 0.0 = def */
) {
	xicc     *p = plu->pp;				/* parent xicc */
	icxLuMatrix *lumat = (icxLuMatrix *)plu;	/* Lookup xMatrix type object */
	icColorSpaceSignature pcs;
	icmLookupFunc func;
	double white[3], black[3], kblack[3];
	gamut *gam;
	int res;		/* Sample point resolution */
	int i, e;

	if (detail == 0.0)
		detail = 10.0;

	/* get some details */
	plu->spaces(plu, NULL, NULL, NULL, NULL, NULL, NULL, &func, &pcs);

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

	gam = new_gamut(detail, pcs == icxSigJabData, 0);

	/* Explore the gamut by itterating through */
	/* it with sample points in device space. */

	res = (int)(600.0/detail);	/* Establish an appropriate sampling density */

	if (res < 40)
		res = 40;

	/* Since matrix profiles can't be non-monotonic, */
	/* just itterate through the surface colors. */
	for (i = 0; i < 3; i++) {
		int co[3];
		int ep[3];
		int co_e = 0;

		for (e = 0; e < 3; e++) {
			co[e] = 0;
			ep[e] = res;
		}
		ep[i] = 2;

		while (co_e < 3) {
			double in[3];
			double out[3];
	
			for (e = 0; e < 3; e++) 		/* Convert count to input value */
				in[e] = co[e]/(ep[e]-1.0);
	
			/* Always use the device->PCS conversion */
			if (lumat->fwd_lookup((icxLuBase *)lumat, out, in) > 1)
				error ("%d, %s",p->errc,p->err);
	
			gam->expand(gam, out);
		
			/* Increment the counter */
			for (co_e = 0; co_e < 3; co_e++) {
				co[co_e]++;
				if (co[co_e] < ep[co_e])
					break;	/* No carry */
				co[co_e] = 0;
			}
		}
	}

#ifdef NEVER
	/* Try it twice */
	for (i = 0; i < 3; i++) {
		int co[3];
		int ep[3];
		int co_e = 0;

		for (e = 0; e < 3; e++) {
			co[e] = 0;
			ep[e] = res;
		}
		ep[i] = 2;

		while (co_e < 3) {
			double in[3];
			double out[3];
	
			for (e = 0; e < 3; e++) 		/* Convert count to input value */
				in[e] = co[e]/(ep[e]-1.0);
	
			/* Always use the device->PCS conversion */
			if (lumat->fwd_lookup((icxLuBase *)lumat, out, in) > 1)
				error ("%d, %s",p->errc,p->err);
	
			gam->expand(gam, out);
		
			/* Increment the counter */
			for (co_e = 0; co_e < 3; co_e++) {
				co[co_e]++;
				if (co[co_e] < ep[co_e])
					break;	/* No carry */
				co[co_e] = 0;
			}
		}
	}
#endif

#ifdef NEVER	// (doesn't seem to make much difference) 
	/* run along the primary ridges in more detail too */
	/* just itterate through the surface colors. */
	for (i = 0; i < 3; i++) {
		int j;
		double in[3];
		double out[3];
		
		res *= 4;

		for (j = 0; j < res; j++) {
			double vv = i/(res-1.0);

			in[0] = in[1] = in[2] = vv;
			in[i] = 0.0;

			if (lumat->fwd_lookup((icxLuBase *)lumat, out, in) > 1)
				error ("%d, %s",p->errc,p->err);
			gam->expand(gam, out);

			in[0] = in[1] = in[2] = 0.0;
			in[i] = vv;

			if (lumat->fwd_lookup((icxLuBase *)lumat, out, in) > 1)
				error ("%d, %s",p->errc,p->err);
			gam->expand(gam, out);
		}
	}
#endif

	/* Put the white and black points in the gamut */
	plu->efv_wh_bk_points(plu, white, black, kblack);
	gam->setwb(gam, white, black, kblack);

	/* set the cusp points by itterating through the 0 & 100% colorant combinations */
	{
		DCOUNT(co, 3, 3, 0, 0, 2);

		gam->setcusps(gam, 0, NULL);
		DC_INIT(co);
		while(!DC_DONE(co)) {
			int e;
			double in[3];
			double out[3];
	
			if (!(co[0] == 0 && co[1] == 0 && co[2] == 0)
			 && !(co[0] == 1 && co[1] == 1 && co[2] == 1)) {	/* Skip white and black */
				for (e = 0; e < 3; e++)
					in[e] = (double)co[e];
	
				/* Always use the device->PCS conversion */
				if (lumat->fwd_lookup((icxLuBase *)lumat, out, in) > 1)
					error ("%d, %s",p->errc,p->err);
				gam->setcusps(gam, 3, out);
			}

			DC_INC(co);
		}
		gam->setcusps(gam, 2, NULL);
	}

#ifdef NEVER		/* Not sure if this is a good idea ?? */
	gam->getwb(gam, NULL, NULL, white, black);	/* Get the actual gamut white and black points */
	gam->setwb(gam, white, black);				/* Put it back as colorspace one */
#endif

	return gam;
}

#ifdef DEBUG
#undef DEBUG
#endif
