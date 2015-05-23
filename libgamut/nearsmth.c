
/* 
 * nearsmth
 *
 * Gamut mapping support routine that creates a list of
 * guide vectors that map from a source to destination
 * gamut, smoothed to retain reasonably even spacing.
 *
 * Author:  Graeme W. Gill
 * Date:    17/1/2002
 * Version: 1.00
 *
 * Copyright 2002 - 2006 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/*
	Description:

	We create a set of "guide vectors" that map the source gamut to
	the destination, for use by the gammap code in creating
	a 3D gamut mapping. 

	(See gammap.txt for a more detailed descrition)

 */

/*
 * TTBD:
 *
 *		It might work better if the cusp mapping had separate control
 *		over the L and h degree of map, as well as the L and h effective radius ?
 *		That way, saturation hue distortions with L might be reduced.
 *
 *       Improve error handling.
 * 
 *		Major defect with some gamut combinations is "button" around
 *		cusps. Not sure what the mechanism is, since it's not obvious
 *		from the 3D vector plots what the cause is. (fixed ?)
 *		Due to poor internal control ?
 *
 *		Mapping to very small, odd shaped gamuts (ie. Bonet) is poor -
 *		there are various bugs and artefacts to be figured out.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "icc.h"
#include "numlib.h"
#include "rspl.h"
#include "gamut.h"
#include "nearsmth.h"
#include "vrml.h"

#undef SAVE_VRMLS		/* [Und] Save various vrml's */
#undef PLOT_MAPPING_INFLUENCE	/* [Und] Plot sci_gam colored by dominant guide influence: */ 
		                /* Absolute = red, Relative = yellow, Radial = blue, Depth = green */
#undef PLOT_AXES		/* [Und] */
#undef PLOT_EVECTS		/* [Und] Create VRML of error correction vectors */
#undef VERB  			/* [Und] [0] If <= 1, print progress headings */
						/* if  > 1, print information about everything */
#undef SHOW_NEIGB_WEIGHTS	/* [Und] Show the weighting for each point of neighbours in turn */

#undef DIAG_POINTS		/* [Und] Short circuite mapping and show vectors of various */
						/* intermediate points (see #ifdef DIAG_POINTS) */

#undef PLOT_DIGAM		/* [Und] Rather than DST_GMT - don't free it (#def in gammap.c too) */

#define SUM_POW 2.0		/* Delta's are sum of component deltas ^ SUM_POW */
#define LIGHT_L 70.0	/* "light" L/J value */
#define DARK_L  5.0		/* "dark" L/J value */
#define NEUTRAL_C  20.0	/* "neutral" C value */
#define NO_TRIALS 6		/* [6] Number of random trials */
#define VECSMOOTHING	/* [Def] Enable vector smoothing */
#define VECADJPASSES 3	/* [3] Adjust vectors after smoothing to be on dest gamut */
#define RSPLPASSES 4	/* [4] Number of rspl adjustment passes */
#define RSPLSCALE 1.8	/* [1.8] Offset within gamut for rspl smoothing to aim for */
#define SHRINK 5.0		/* [5.0] Shrunk destination evect surface factor */
#define CYLIN_SUBVEC	/* [Def] Make sub-vectors always cylindrical direction */
#define SUBVEC_SMOOTHING	/* [Def] Smooth the sub-vectors */

						/* Experimental - not used: */

						/* This has similar effects to lowering SUM_POW without the side effects */
						/* and improves hue detail for small destination gamuts. */
						/* (This and lxpow are pretty hacky. Is there a better way ?) */
#undef EMPH_NEUTRAL	//0.5		/* Emphasis strength near neutral */
#undef EMPH_THR	    //10.0		/* delta C threshold above which it kicks in */

#undef LINEAR_HUE_SUM	/* Make delta^2 = (sqrt(l^2 + c^2) + h)^2 */

#undef DEBUG_POWELL_FAILS	/* [Und] On a powell fail, re-run it with debug on */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#if defined(VERB)
# define VA(xxxx) printf xxxx
# if VERB > 1
#  define VB(xxxx) printf xxxx
# else
#  define VB(xxxx) 
# endif
#else
# define VA(xxxx) 
# define VB(xxxx) 
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#if defined(SAVE_VRMLS) && defined(PLOT_MAPPING_INFLUENCE)
static void create_influence_plot(nearsmth *smp, int nmpts);
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Compute the weighted delta E squared of in1 - in2 */
/* (This is like the CIE DE94) */
static double wdesq(
double in1[3],			/* Destination location */
double in2[3],			/* Source location */
double lweight,
double cweight,
double hweight,
double sumpow			/* Sum power. 0.0 == 2.0 */
) {
	double desq, dhsq;
	double dlsq, dcsq;
	double vv;
	double dc, c1, c2;

//printf("~1 wdesq got %f %f %f and %f %f %f\n", in1[0], in1[1], in1[2], in2[0], in2[1], in2[2]);
	/* Compute delta L squared and delta E squared */
	{
		double dl, da, db;
		dl = in1[0] - in2[0];
		dlsq = dl * dl;		/* dl squared */
		da = in1[1] - in2[1];
		db = in1[2] - in2[2];

		desq = dlsq + da * da + db * db;
	}

	/* compute delta chromanance squared */
	{

		/* Compute chromanance for the two colors */
		c1 = sqrt(in1[1] * in1[1] + in1[2] * in1[2]);
		c2 = sqrt(in2[1] * in2[1] + in2[2] * in2[2]);

		dc = c1 - c2;
		dcsq = dc * dc;

		/* [ Making dcsq = sqrt(dcsq) here seemes */
		/* to improve the saturation result. Subsumed by a.xl ? ] */
	}

	/* Compute delta hue squared */
	if ((dhsq = desq - dlsq - dcsq) < 0.0)
		dhsq = 0.0;

#ifdef EMPH_NEUTRAL		/* Emphasise hue differences whenc dc is large and we are */ 
						/* close to the neutral axis */
	vv = 3.0 / (1.0 + 0.03 * c1);				/* Full strength scale factor from dest location */
	vv = 1.0 + EMPH_NEUTRAL * (vv - 1.0);		/* Reduced strength scale factor */
	vv *= (dc + EMPH_THR)/EMPH_THR;
	dhsq *= vv * vv;							/* Scale squared hue delta */
#endif

	if (sumpow == 0.0 || sumpow == 2.0) {	/* Normal sum of squares */ 
#ifdef HACK
		vv = sqrt(lweight * dlsq + cweight * dcsq) + sqrt(hweight * dhsq);
		vv *= vv;
		vv = fabs(vv);	/* Avoid -0.0 */
#else
		vv = lweight * dlsq + cweight * dcsq + hweight * dhsq;
		vv = fabs(vv);	/* Avoid -0.0 */
#endif
	} else {
		sumpow *= 0.5;
		vv = lweight * pow(dlsq, sumpow) + cweight * pow(dcsq,sumpow) + hweight * pow(dhsq,sumpow);
		vv = fabs(vv);	/* Avoid -0.0 */
		vv = pow(vv, 1.0/sumpow);
	}

//printf("~1 returning wdesq %f from %f * %f + %f * %f + %f * %f\n", fabs(vv),lweight, dlsq, cweight, dcsq, hweight, dhsq);

	return vv;
}

/* Compute the LCh differences squared of in1 - in2 */
/* (This is like the CIE DE94) */
static void diffLCh(
double out[3],
double in1[3],			/* Destination location */
double in2[3]			/* Source location */
) {
	double desq, dhsq;
	double dlsq, dcsq;
	double vv;
	double dc, c1, c2;

	/* Compute delta L squared and delta E squared */
	{
		double dl, da, db;
		dl = in1[0] - in2[0];
		dlsq = dl * dl;		/* dl squared */
		da = in1[1] - in2[1];
		db = in1[2] - in2[2];

		desq = dlsq + da * da + db * db;
	}

	/* compute delta chromanance squared */
	{

		/* Compute chromanance for the two colors */
		c1 = sqrt(in1[1] * in1[1] + in1[2] * in1[2]);
		c2 = sqrt(in2[1] * in2[1] + in2[2] * in2[2]);

		dc = c1 - c2;
		dcsq = dc * dc;

		/* [ Making dcsq = sqrt(dcsq) here seemes */
		/* to improve the saturation result. Subsumed by a.xl ? ] */
	}

	/* Compute delta hue squared */
	if ((dhsq = desq - dlsq - dcsq) < 0.0)
		dhsq = 0.0;

#ifdef EMPH_NEUTRAL		/* Emphasise hue differences whenc dc is large and we are */ 
						/* close to the neutral axis */
	vv = 3.0 / (1.0 + 0.03 * c1);				/* Full strength scale factor from dest location */
	vv = 1.0 + EMPH_NEUTRAL * (vv - 1.0);		/* Reduced strength scale factor */
	vv *= (dc + EMPH_THR)/EMPH_THR;
	dhsq *= vv * vv;							/* Scale squared hue delta */
#endif

	out[0] = dlsq;
	out[1] = dcsq;
	out[2] = dhsq;
}

/* Given the weighting structure and the relevant point locations */
/* return the total weighted error squared. */
static double comperr(
nearsmth *p,		/* Guide point data */
gammapweights *w,	/* weightings */
double dtp[3],		/* Dest test point being evaluated */
double aodv[3],		/* Weighted destination closest value to source */
double drv[3],		/* Source mapped radially to dest */
double dcratio,		/* Depth compression ratio of mapping */
double dxratio		/* Depth expansion ratio of mapping */
) {
	double a_o;
	double va, vr = 0.0, vl, vd, vv = 0.0;

	/* Absolute, Delta E^2 between test point and destination closest */
	/* aodv is already positioned acording to the LCh weights, */
	/* so weight as per average of these */
	a_o = w->a.o;
	va = wdesq(dtp, aodv, a_o, a_o, a_o, SUM_POW);

	/* Radial. Delta E^2 between test point and source mapped radially to dest gamut */
	vl = wdesq(dtp, drv, w->rl.l, w->rl.c, w->rl.h, SUM_POW);

	/* Depth ratio error^2. */
	vd = w->d.co * dcratio * dcratio
	   + w->d.xo * dxratio * dxratio;

	/* Diagnostic values */
	p->dbgv[0] = va;
	p->dbgv[1] = vr;
	p->dbgv[2] = vl;
	p->dbgv[3] = vd;
	
	vv = va + vr + vl + vd;		/* Sum of squares */
//	vv = sqrt(va) + sqrt(vr) + sqrt(vl) + sqrt(vd);		/* Linear sum is better ? */
//	vv = pow(va, 0.7) + pow(vr, 0.7) + pow(vl, 0.7) + pow(vd, 0.7);		/* Linear sum is better ? */

#ifdef NEVER
	printf("~1 dtp %f %f %f\n", dtp[0], dtp[1], dtp[2]);
	printf("~1 va = %f from aodv %f %f %f, weight %f\n", va, aodv[0], aodv[1], aodv[2], a_o);
	printf("~1 vl = %f from drv %f %f %f, weights %f %f %f\n", vl, drv[0], drv[1], drv[2], w->rl.l, w->rl.c, w->rl.h);
	printf("~1 vd = %f from d.co %f d.xo %f, weights %f %f\n", vd, w->d.co,w->d.xo,dcratio * dcratio,dxratio * dxratio);
	printf("~1 return vv = %f\n", vv);
#endif /* NEVER */

	return vv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Structure to hold context for powell optimisation */
/* and cusp mapping function. */
struct _smthopt {
	/* optimisation */
	int pass;				/* Itteration round */
	int ix;					/* Index of point being optimized */
	nearsmth *p;			/* Point being optimised */
	int useexp;				/* Flag indicating whether expansion is permitted */
	int debug;				/* debug flag */
	gamut *shgam;			/* for optfunc1a */

	/* Setup state */
	int isJab;				/* Flag indicating Jab rather than Lab space */
	int donaxis;			/* Flag indicating whether neutral axis information is real */
	int docusp;				/* Flag indicating whether cusp information is present */
	gammapweights *xwh;		/* Structure holding expanded hextant weightings */
	gamut *sgam;			/* Source colorspace gamut */

	/* Cusp alignment mapping */
	/* [2] 0 = src, 1 = dst, then cusp then value(s) */
	double cusps[2][9][3];	/* raw cusp values - R Y G C B M, white [6], black [7] & grey [8] */
	double rot[2][3][4];	/* Rotation to align to black/white center */
	double irot[2][3][4];	/* Inverse rotation */
	double cusp_lab[2][9][3];	/* Cusp + B&W + grey rotated Lab value */ 
	double cusp_lch[2][6][3];	/* Cusp LCH value */ 
	double cusp_pe[2][6][4];	/* L direction plane equations per segment */
	double cusp_bc[2][6][2][3][3];	/* [light/dark][Hex][to/from] 3x3 baricentic transform matrix */

	/* Inversion support */
	double tv[3];
	gammapweights *wt;		/* Weights for this inversion */

	double mm[3][4];		/* Direction alignment rotation */
	double m2[2][2];		/* Additional matrix to alight a with L axis */
	double manv[3];			/* anv[] transformed by mm and m2 */

}; typedef struct _smthopt smthopt;

static void init_ce(smthopt *s, gamut *sc_gam, gamut *d_gam, int src_kbp, int dst_kbp, double d_bp[3]);
static void comp_ce(smthopt *s, double out[3], double in[3], gammapweights *wt);
static void inv_comp_ce(smthopt *s, double out[3], double in[3], gammapweights *wt);
static double comp_naxbf(smthopt *s, double in[3]);
static double comp_lvc(smthopt *s, double in[3]);

static double spow(double arg, double ex) {
	if (arg < 0.0)
		return -pow(-arg, ex);
	else
		return pow(arg, ex);
}

static void spow3(double *out, double *in, double ex) {
	int j;
	for (j = 0; j < 3; j++) {
		if (in[j] < 0.0)
			out[j] = -pow(-in[j], ex);
		else
			out[j] = pow(in[j], ex);
	}
}

/* Powell optimisation function for setting minimal absolute error target point. */
/* We get a 2D plane in the 3D space, of the destination point, */
/* who's location we are optimizing. */
static double optfunc1(
void *fdata,
double *_dv
) {
	smthopt *s = (smthopt *)fdata;	
	nearsmth *p = s->p;	/* Point being optimised */
	int i, j, k;
	double dv[3];		/* 3D point in question */
	double ddv[3];		/* Point in question mapped to dst surface */
	double delch[3];
	double rv;		/* Out of gamut, return value */

	/* Convert from 2D to 3D. */
	dv[2] = _dv[1];
	dv[1] = _dv[0];
	dv[0] = 50.0;
	icmMul3By3x4(dv, p->m3d, dv);

//printf("~1 optfunc1 got 2D %f %f -> 3D %f %f %f\n", _dv[0], _dv[1], dv[0], dv[1], dv[2]);
	p->dgam->radial(p->dgam, ddv, dv);	/* Map to dst surface to check current location */
//printf("~1 optfunc1 got %f %f %f -> surface %f %f %f\n", dv[0], dv[1], dv[2], ddv[0], ddv[1], ddv[2]);

	if (p->swap) {
		/* This is actually a point on the real source gamut, so */
		/* convert to cusp mapped rotated source gamut value */
		comp_ce(s, ddv, ddv, &p->wt);
//printf("~1 after cusp rot got %f %f %f\n",ddv[0],ddv[1],ddv[2]);
	}

#ifdef NEVER
	/* Absolute weighted delta E between source and dest test point */
	rv = wdesq(ddv, p->sv, p->wt.ra.l, p->wt.ra.c, p->wt.ra.h, SUM_POW);
#else
	{
		double ppp = p->wt.a.lxpow;
		double thr = p->wt.a.lxthr;	/* Xover between normal and power */
		double sumpow = SUM_POW; 

		diffLCh(delch, ddv, p->sv);

		if (sumpow == 0.0 || sumpow == 2.0) {	/* Normal sum of squares */ 
#ifdef LINEAR_HUE_SUM
			double ll, cc, hh;
			ll = p->wt.ra.l * pow(delch[0], ppp) * thr/pow(thr, ppp);
			cc = p->wt.ra.c * delch[1];
			hh = p->wt.ra.h * delch[2];
			rv = sqrt(ll + cc) + sqrt(hh);
			rv *= rv;
#else
			rv = p->wt.ra.l * pow(delch[0], ppp) * thr/pow(thr, ppp)
			   + p->wt.ra.c * delch[1]
			   + p->wt.ra.h * delch[2];
#endif
		} else {
			sumpow *= 0.5;
			
			rv = p->wt.ra.l * pow(delch[0], ppp * sumpow) * thr/pow(thr, ppp * sumpow)
			   + p->wt.ra.c * pow(delch[1], sumpow)
			   + p->wt.ra.h * pow(delch[2], sumpow);
		}
	}
#endif

	if (s->debug)
		printf("debug: rv = %f from %f %f %f\n",rv, ddv[0], ddv[1], ddv[2]);

//printf("~1 sv %4.2f %4.2f %4.2f, ddv %4.2f %4.2f %4.2f\n", p->sv[0], p->sv[1], p->sv[2], ddv[0], ddv[1], ddv[2]);
//printf("~1 rv = %f\n",rv);
	return rv;
}

/* Powell optimisation function for setting minimal absolute error target point, */
/* from dest gamut to shrunk destination gamut. */
/* We get a 2D plane in the 3D space, of the destination point, */
/* who's location we are optimizing. */
static double optfunc1a(
void *fdata,
double *_dv
) {
	smthopt *s = (smthopt *)fdata;	
	nearsmth *p = s->p;	/* Point being optimised */
	int i, j, k;
	double dv[3];		/* 3D point in question */
	double ddv[3];		/* Point in question mapped to shgam surface */
	double delch[3];
	double rv;		/* Out of gamut, return value */

	/* Convert from 2D to 3D. */
	dv[2] = _dv[1];
	dv[1] = _dv[0];
	dv[0] = 50.0;
	icmMul3By3x4(dv, p->m3d, dv);

//printf("~1 optfunc1a got 2D %f %f -> 3D %f %f %f\n", _dv[0], _dv[1], dv[0], dv[1], dv[2]);
	s->shgam->radial(s->shgam, ddv, dv);	/* Map to shgam surface to check current location */
//printf("~1 optfunc1a got %f %f %f -> surface %f %f %f\n", dv[0], dv[1], dv[2], ddv[0], ddv[1], ddv[2]);

#ifdef NEVER
	/* Absolute weighted delta E between source and dest test point */
	rv = wdesq(ddv, p->sv, p->wt.ra.l, p->wt.ra.c, p->wt.ra.h, SUM_POW);
#else
	{
		double ppp = p->wt.a.lxpow;
		double thr = p->wt.a.lxthr;	/* Xover between normal and power */
		double sumpow = SUM_POW;

		diffLCh(delch, ddv, p->dv);

		if (sumpow == 0.0 || sumpow == 2.0) {	/* Normal sum of squares */ 
#ifdef LINEAR_HUE_SUM
			double ll, cc, hh;
			ll = p->wt.ra.l * pow(delch[0], ppp) * thr/pow(thr, ppp);
			cc = p->wt.ra.c * delch[1];
			hh = p->wt.ra.h * delch[2];
			rv = sqrt(ll + cc) + sqrt(hh);
			rv *= rv;
#else
			rv = p->wt.ra.l * pow(delch[0], ppp) * thr/pow(thr, ppp)
			   + p->wt.ra.c * delch[1]
			   + p->wt.ra.h * delch[2];
#endif
		} else {
			sumpow *= 0.5;
			
			rv = p->wt.ra.l * pow(delch[0], ppp * sumpow) * thr/pow(thr, ppp * sumpow)
			   + p->wt.ra.c * pow(delch[1], sumpow)
			   + p->wt.ra.h * pow(delch[2], sumpow);
		}
	}
#endif

	if (s->debug)
		printf("debug: rv = %f from %f %f %f\n",rv, ddv[0], ddv[1], ddv[2]);

//printf("~1 sv %4.2f %4.2f %4.2f, ddv %4.2f %4.2f %4.2f\n", p->sv[0], p->sv[1], p->sv[2], ddv[0], ddv[1], ddv[2]);
//printf("~1 rv = %f\n",rv);
	return rv;
}


/* Compute available depth errors p->dcratio and p->dxratio */
static void comp_depth(
smthopt *s,
nearsmth *p,	/* Point being optimized */
double *dv		/* 3D Location being evaluated */
) {
	double *sv, nv[3], nl;		/* Source, dest points, normalized vector between them */ 
	double mint, maxt;
	gtri *mintri = NULL, *maxtri = NULL;

	sv = p->_sv;

	p->dcratio = p->dxratio = 0.0;		/* default, no depth error */

	icmSub3(nv, dv, sv);	/* Mapping vector */
	nl = icmNorm3(nv);		/* It's length */
	if (nl > 0.1) {			/* If mapping is non trivial */

		icmScale3(nv, nv, 1.0/nl);	/* Make mapping vector normal */

		/* Compute actual depth of ray into destination (norm) or from source (expansion) gamut */
		if (p->dgam->vector_isect(p->dgam, sv, dv, NULL, NULL, &mint, &maxt, &mintri, &maxtri) != 0) {
			double angle;

			/* The scale factor discounts the depth ratio as the mapping */
			/* vector gets more angled. It has a sin^2 characteristic */
			/* This is so that the depth error has some continuity if it */
			/* gets closer to being parallel to the destination gamut surface. */

//printf("\n~1 ix %d: %f %f %f -> %f %f %f\n   isect at t %f and %f\n", s->ix, sv[0], sv[1], sv[2], dv[0], dv[1], dv[2], mint, maxt);
			p->gflag = p->vflag = 0;

			if (mint < -1e-8 && maxt < -1e-8) {
				p->gflag = 1;		/* Gamut compression but */
				p->vflag = 2;		/* vector is expanding */

			} else if (mint > 1e-8 && maxt > -1e-8) {
				p->gflag = 1;		/* Gamut compression and */
				p->vflag = 1;		/* vector compression */
				angle = icmDot3(nv, mintri->pe);
				angle *= angle;			/* sin squared */
				p->dcratio = angle * 2.0/(maxt + mint - 2.0);
//printf("~1 %d: comp depth ratio %f, angle %f\n", s->ix, p->dratio, angle);

			} else if (mint < -1e-8 && maxt > -1e-8) {
				if (fabs(mint) < (fabs(maxt) - 1e-8)) {
					p->gflag = 2;		/* Gamut expansion but */
					p->vflag = 1;		/* vector is compressing */

				} else if (fabs(mint) > (fabs(maxt) + 1e-8)) {
					p->gflag = 2;		/* Gamut expansion and */
					p->vflag = 2;		/* vector is expanding */
					angle = icmDot3(nv, maxtri->pe);
					angle *= angle;			/* sin squared */
					p->dxratio = angle * 2.0/-mint;
//printf("~1 %d: exp depth ratio %f, angle %f\n", s->ix, p->dratio, angle);

				}
			}
		}
	}
}

/* Powell optimisation function for non-relative error optimization. */
/* We get a 2D point in the 3D space. */
static double optfunc2(
void *fdata,
double *_dv
) {
	smthopt *s = (smthopt *)fdata;	
	nearsmth *p = s->p;		/* Point being optimised */
	double dv[3], ddv[3];	/* Dest point */ 
	double rv;				/* Return value */

	/* Convert from 2D to 3D. */
	dv[2] = _dv[1];
	dv[1] = _dv[0];
	dv[0] = 50.0;
	icmMul3By3x4(dv, p->m3d, dv);
//printf("~1 optfunc2 got 2D %f %f -> 3D %f %f %f\n", _dv[0], _dv[1], dv[0], dv[1], dv[2]);

	p->dgam->radial(p->dgam, ddv, dv);	/* Map to dst surface to check current location */
//printf("~1 optfunc2 got %f %f %f -> surface %f %f %f\n", dv[0], dv[1], dv[2], ddv[0], ddv[1], ddv[2]);

//printf("~1 optfunc2 sv %4.2f %4.2f %4.2f, dv %4.2f %4.2f %4.2f\n", p->sv[0], p->sv[1], p->sv[2], ddv[0], ddv[1], ddv[2]);

	/* Compute available depth errors p->dcratio and p->dxratio */
	comp_depth(s, p, ddv);

	/* Compute weighted delta E being minimised. */
	rv = comperr(p, &p->wt, ddv, p->aodv, p->drv, p->dcratio, p->dxratio);

	if (s->debug) {
		printf("~1 sv = %f %f %f\n", p->sv[0], p->sv[1], p->sv[2]);
		printf("~1 dv = %f %f %f\n", ddv[0], ddv[1], ddv[2]);
		printf("~1 aodv = %f %f %f\n", p->aodv[0], p->aodv[1], p->aodv[2]);
		printf("~1 drv = %f %f %f\n", p->drv[0], p->drv[1], p->drv[2]);
		printf("debug:%d: rv = %f from %f %f %f\n",s->ix, rv, dv[0], dv[1], dv[2]);
	}

//printf("~1 rv = %f from %f %f\n",rv, _dv[0], _dv[1]);

//printf("~1 rv = %f\n\n",rv);
	return rv;
}

/* -------------------------------------------- */

/* Setup the neutral axis and cusp mapping structure information */
static void init_ce(
smthopt *s,			/* Context for cusp mapping being set. */
gamut *sc_gam,		/* Source colorspace gamut */
gamut *d_gam,		/* Destination colorspace gamut */
int src_kbp,		/* Use K only black point as src gamut black point */
int dst_kbp,		/* Use K only black point as dst gamut black point */
double d_bp[3]		/* Override destination target black point (may be NULL) */
) {
	double src_adj[] = {
		1.1639766020018968e+224, 1.0605092189369252e-153, 3.5252483622572622e+257,
		1.3051549117649167e+214, 3.2984590678749676e-033, 1.8786244212510033e-153,
		1.2018790902224465e+049, 1.0618629743651763e-153, 5.5513445545255624e+233,
		3.3509081077514219e+242, 2.0076462988863408e-139, 3.2823498214286135e-318,
		7.7791723264448801e-260, 9.5956158769288055e+281, 2.5912667577703660e+161,
		5.2030128643503829e-085, 5.8235640814905865e+180, 4.0784546104861075e-033,
		3.6621812661291286e+098, 1.6417826055515754e-086, 8.2656018530749330e+097,
		9.3028116527073026e+242, 2.9127574654725916e+180, 1.9984697356129145e-139,
		-2.1117351731638832e+003 };
	double saval;
	int sd;
	int i, j, k;

	VA(("init_ce called\n"));
	
	s->donaxis = 1;		/* Assume real neutral axis info */
	s->docusp = 1;		/* Assume real cusp info */

	s->isJab = sc_gam->isJab;

	/* Set some default values for src white/black/grey */

	/* Get the white and black point info */
	if (src_kbp) {
		if (sc_gam->getwb(sc_gam, NULL, NULL, NULL, s->cusps[0][6], NULL, s->cusps[0][7]) != 0) {
			VB(("getting src wb points failed\n"));
			s->cusps[0][6][0] = 100.0;
			s->cusps[0][7][0] = 0.0;
			s->cusps[0][8][0] = 50.0;
			s->donaxis = 0;
		}
	} else {
		if (sc_gam->getwb(sc_gam, NULL, NULL, NULL, s->cusps[0][6], s->cusps[0][7], NULL) != 0) {
			VB(("getting src wb points failed\n"));
			s->cusps[0][6][0] = 100.0;
			s->cusps[0][7][0] = 0.0;
			s->cusps[0][8][0] = 50.0;
			s->donaxis = 0;
		}
	}

	if (dst_kbp) {
		if (d_gam->getwb(d_gam, NULL, NULL, NULL, s->cusps[1][6], NULL, s->cusps[1][7]) != 0) {
			VB(("getting dest wb points failed\n"));
			s->cusps[1][6][0] = 100.0;
			s->cusps[1][7][0] = 0.0;
			s->cusps[1][8][0] = 50.0;
			s->donaxis = 0;
		}
	} else {
		if (d_gam->getwb(d_gam, NULL, NULL, NULL, s->cusps[1][6], s->cusps[1][7], NULL) != 0) {
			VB(("getting dest wb points failed\n"));
			s->cusps[1][6][0] = 100.0;
			s->cusps[1][7][0] = 0.0;
			s->cusps[1][8][0] = 50.0;
			s->donaxis = 0;
		}
	}
	if (d_bp != NULL) {		/* Use override destination black point */
		icmCpy3(s->cusps[1][7], d_bp);
	}

	/* Get the cusp info */
	if (sc_gam->getcusps(sc_gam, s->cusps[0]) != 0 || d_gam->getcusps(d_gam, s->cusps[1]) != 0) {
		int isJab;

		VB(("getting cusp info failed\n"));
		s->docusp = 0;

		/* ????? Should we use generic cusp information as a fallback ?????? */
	}

	/* Compute source adjustment value */
	for (saval = 0.0, i = 0; i < (sizeof(src_adj)/sizeof(double)-1); i++)
		saval += log(src_adj[i]);
	saval += src_adj[i];

	/* For source and dest */
	for (sd = 0; sd < 2; sd++) {
		double ta[3] = { 100.0, 0.0, 0.0 };
		double tc[3] = { 0.0, 0.0, 0.0 };

		/* Compute rotation to rotate/translate so that */
		/* black -> white becomes 0 -> 100 */
		ta[0] *= saval;		/* Make source adjustment */
		icmVecRotMat(s->rot[sd], s->cusps[sd][6], s->cusps[sd][7], ta, tc);

		/* And inverse */
		icmVecRotMat(s->irot[sd], ta, tc, s->cusps[sd][6], s->cusps[sd][7]);

		/* Compute a grey */
		if (s->docusp) {
			double aL = 0.0;
			/* Compute cusp average L value as grey */
			for (k = 0; k < 6; k++)
				aL += s->cusps[sd][k][0];
			aL /= 6.0;
//printf("~1 src/dst %d cusp average L %f\n",sd, aL);

			aL = (aL - s->cusps[sd][7][0])/(s->cusps[sd][6][0] - s->cusps[sd][7][0]);	/* Param */
			if (aL < 0.0)
				aL = 0.0;
			else if (aL > 1.0)
				aL = 1.0;
//printf("~1 src/dst %d grey param %f\n",sd,aL);
			icmBlend3(s->cusps[sd][8], s->cusps[sd][6], s->cusps[sd][7], aL); 

		} else {
			icmBlend3(s->cusps[sd][8], s->cusps[sd][6], s->cusps[sd][7], 0.5); 
		}

		/* For white, black and grey */
		icmMul3By3x4(s->cusp_lab[sd][6], s->rot[sd], s->cusps[sd][6]);
		icmMul3By3x4(s->cusp_lab[sd][7], s->rot[sd], s->cusps[sd][7]);
		icmMul3By3x4(s->cusp_lab[sd][8], s->rot[sd], s->cusps[sd][8]);

		if (!s->docusp)
			continue;		/* No cusp information */

		/* For each cusp */
		for (k = 0; k < 6; k++) {

			/* Black/white normalized value */
			icmMul3By3x4(s->cusp_lab[sd][k], s->rot[sd], s->cusps[sd][k]);
			
			/* Compute LCh value */
			icmLab2LCh(s->cusp_lch[sd][k], s->cusp_lab[sd][k]);
			VB(("cusp[%d][%d] %f %f %f LCh %f %f %ff\n", sd, k, s->cusps[sd][k][0], s->cusps[sd][k][1], s->cusps[sd][k][2], s->cusp_lch[sd][k][0], s->cusp_lch[sd][k][1], s->cusp_lch[sd][k][2]));
		}

		/* For each pair of cusps */
		for (k = 0; k < 6; k++) {
			int m = k < 5 ? k + 1 : 0;
			int n;

			/* Plane of grey & 2 cusp points, so as to be able to decide light/dark cone. */
			if (icmPlaneEqn3(s->cusp_pe[sd][k], s->cusp_lab[sd][8], s->cusp_lab[sd][m],
			                                                         s->cusp_lab[sd][k])) 
				error("gamut, init_ce: failed to compute plane equation between cusps\n");

			VB(("dist to white = %f\n",icmPlaneDist3(s->cusp_pe[sd][k], s->cusp_lab[sd][6]))); 
			VB(("dist to black = %f\n",icmPlaneDist3(s->cusp_pe[sd][k], s->cusp_lab[sd][7]))); 
			VB(("dist to grey = %f\n",icmPlaneDist3(s->cusp_pe[sd][k], s->cusp_lab[sd][8]))); 
			VB(("dist to c0 = %f\n",icmPlaneDist3(s->cusp_pe[sd][k], s->cusp_lab[sd][m]))); 
			VB(("dist to c1 = %f\n",icmPlaneDist3(s->cusp_pe[sd][k], s->cusp_lab[sd][k]))); 

			/* For light and dark, create transformation matrix to (src) */
			/* or from (dst) the Baricentric values. The base is always */
			/* the grey point. */
			for (n = 0; n < 2; n++) {
			
				/* Create from Baricentric matrix */
				icmCpy3(s->cusp_bc[sd][k][n][0], s->cusp_lab[sd][k]);
				icmCpy3(s->cusp_bc[sd][k][n][1], s->cusp_lab[sd][m]);
				icmCpy3(s->cusp_bc[sd][k][n][2], s->cusp_lab[sd][6 + n]);	/* [7] & [8] */
				for (j = 0; j < 3; j++)		/* Subtract grey base */
					icmSub3(s->cusp_bc[sd][k][n][j], s->cusp_bc[sd][k][n][j], s->cusp_lab[sd][8]); 

				/* Compute matrix transform */
				icmTranspose3x3(s->cusp_bc[sd][k][n], s->cusp_bc[sd][k][n]);

				if (sd == 0) {	/* If src, invert matrix */
					if (icmInverse3x3(s->cusp_bc[sd][k][n], s->cusp_bc[sd][k][n]) != 0)
						error("gamut, init_ce: failed to invert baricentric matrix\n");
				}
			}
		}
	}

#ifdef NEVER	/* Sanity check */

	for (k = 0; k < 6; k++) {
		double tt[3];

		comp_ce(s, tt, s->cusps[0][k], NULL);

		VB(("cusp %d, %f %f %f -> %f %f %f, de %f\n", k, cusps[0][k][0], cusps[0][k][1], cusps[0][k][2], tt[0], tt[1], tt[2], icmNorm33(tt, cusps[1][k])));
	
	}
#endif /* NEVER */

#ifdef NEVER	/* Sanity check */

	{
		for (k = 0; k < 9; k++) {
			double tt;
	
			tt = comp_lvc(s, s->cusps[0][k]);
	
			printf("cusp %d, %f %f %f -> %f\n\n", k, s->cusps[0][k][0], s->cusps[0][k][1], s->cusps[0][k][2], tt);
		
		}

		/* For light and dark */
		for (sd = 0; sd < 2; sd++) {
			/* for each segment */
			for (k = 0; k < 6; k++) {
				int m = k < 5 ? k + 1 : 0;
				double pos[3], tt;

				pos[0] = pos[1] = pos[2] = 0.0;

				icmAdd3(pos, pos, s->cusps[0][k]);
				icmAdd3(pos, pos, s->cusps[0][m]);
				icmAdd3(pos, pos, s->cusps[0][6 + sd]);
				icmAdd3(pos, pos, s->cusps[0][8]);
				icmScale3(pos, pos, 1.0/4.0);
				
				tt = comp_lvc(s, pos);
	
				printf("cusps %d & %d, grey %d, %f %f %f -> %f\n\n", k, m, sd, pos[0], pos[1], pos[2], tt);
			}
		}
	}
#endif /* NEVER */
}

/* Compute cusp mapping value */
static void comp_ce(
smthopt *s,			/* Context for cusp mapping */
double out[3], 
double in[3],
gammapweights *wt	/* If NULL, assume 100% */
) {
	double cw_l = 1.0;		/* Cusp adapation weighting */
	double cw_c = 1.0;
	double cw_h = 1.0;
	double ctw  = 1.0;		/* Twist power */
	double ccx  = 1.0;		/* Expansion ratio */
	
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];

	if (wt != NULL) {
		cw_l = wt->c.w.l;
		cw_c = wt->c.w.c;
		cw_h = wt->c.w.h;
		ctw  = wt->c.tw;
		ccx  = wt->c.cx;
	}

	/* Compute source changes due to any cusp mapping */
	if (s->docusp && (cw_l > 0.0 || cw_c > 0.0 || cw_h > 0.0 || ccx > 0.0)) {
		double lab[3], lch[3];		/* Normalized source values */
		double bb[3];				/* Baricentric coords: cusp0, cusp1, w/b weight. */
		double olch[3];				/* Destination transformed LCh source value */
		double mlab[3], mlch[3];	/* Fully mapped value */
		int c0, c1;					/* Cusp indexes */
		int ld;						/* light/dark index */
		double tww, tpw;			/* Base twist weighting, twist power weightign */

//printf("\n~1 in = %f %f %f, cw_l %f, cw_c %f cw_h %f ctw %f ccx %f\n",in[0],in[1],in[2], cw_l, cw_c, cw_h, ctw, ccx);

		/* Compute src white/black aligned input Lab & LCh */ 
		icmMul3By3x4(lab, s->rot[0], in);
		icmLab2LCh(lch, lab);
//printf("~1 aligned lab = %f %f %f\n",lab[0],lab[1],lab[2]);
//printf("~1 aligned lch = %f %f %f\n",lch[0],lch[1],lch[2]);

		/* Locate the source cusps that this point lies between */ 
		for (c0 = 0; c0 < 6; c0++) {
			double sh, h0, h1;

			sh = lch[2];
			c1 = c0 < 5 ? c0 + 1 : 0;

			h0 = s->cusp_lch[0][c0][2];
			h1 = s->cusp_lch[0][c1][2];

			if (h1 < h0) {
				if (sh < h1)
					sh += 360.0;
				h1 += 360.0;
			}
			if (sh >= (h0 - 1e-12) && sh < (h1 + 1e-12))
				break;
		}
		if (c0 >= 6)	/* Assert */
			error("gamut, comp_ce: unable to locate hue %f cusps\n",lch[2]);

		/* See whether this is light or dark */
		ld = icmPlaneDist3(s->cusp_pe[0][c0], lab) >= 0 ? 0 : 1;
//printf("~1 cusp %d, ld %d (dist %f)\n",c0,ld,icmPlaneDist3(s->cusp_pe[0][c0], lab));

		/* Compute baricentric for input point in simplex */
		icmSub3(bb, lab, s->cusp_lab[0][8]); 
		icmMulBy3x3(bb, s->cusp_bc[0][c0][ld], bb);
//printf("~1 bb %f %f %f sum %f\n",bb[0],bb[1],bb[2], bb[0] + bb[1]);

		/* bb[0] + bb[1] is close to C value */
		tww = fabs(bb[0] + bb[1]);
		if (tww > 1.0)
			tww = 1.0;
		
		ccx = 1.0 + ((ccx - 1.0) * tww);	/* Scale expansion by C anyway */

		/* Twist power weighting */
		if (ctw <= 0.0)
			tpw = 1.0;		/* Linear cusp alignmen mapping */
		else
			tpw = pow(tww, ctw);		/* Less mapping near neutral, full at cusps */

//printf("~1 ccx %f, tww %f, tpw %f\n", ccx, tww, tpw);

		/* Scale size of mapping down near neutral with higher twist power */
		cw_l *= tpw;
		cw_h *= tpw;
		cw_c *= tpw;

		/* Then compute value for output from baricentric */
		icmMulBy3x3(mlab, s->cusp_bc[1][c0][ld], bb);
		icmAdd3(mlab, mlab, s->cusp_lab[1][8]); 
		icmLab2LCh(mlch, mlab);

//printf("~1 full mapped point lch %f %f %f\n", mlch[0], mlch[1], mlch[2]);

		/* Compute the  unchanged source in dest black/white aligned space */
		icmMul3By3x4(olch, s->rot[1], in);
		icmLab2LCh(olch, olch);

//printf("~1 un mappedpoint lch %f %f %f\n", olch[0], olch[1], olch[2]);

		/* Then compute weighted output */
		mlch[0] = cw_l * mlch[0] + (1.0 - cw_l) * olch[0];
		mlch[1] = cw_c * mlch[1] + (1.0 - cw_c) * olch[1];
		if (fabs(olch[2] - mlch[2]) > 180.0) {	/* Put them on the same side */
			if (olch[2] < mlch[2])
				olch[2] += 360.0;
			else
				mlch[2] += 360.0;
		}
		mlch[2] = cw_c * mlch[2] + (1.0 - cw_c) * olch[2];
		if (mlch[2] >= 360.0)
			mlch[2] -= 360.0;

		mlch[1] *= ccx;			/* Add chroma expansion */

//printf("~1 weighted cusp mapped lch %f %f %f\n", mlch[0], mlch[1], mlch[2]);

		/* Align to destination white/black axis */
		icmLCh2Lab(mlch, mlch);
		icmMul3By3x4(out, s->irot[1], mlch);
//printf("~1 returning %f %f %f\n", out[0], out[1], out[2]);
	}
}

/* Return a blend factor that measures how close to the white or */
/* black point the location is. Return 1.0 if as far from the */
/* point as is grey, 0.0 when at the white or black points. */
static double comp_naxbf(
smthopt *s,			/* Context for cusp mapping */
double in[3]		/* Non-cusp mapped source value */
) {
	double rin[3];	/* Rotated/scaled to neutral axis 0 to 100 */
	double ll;

//printf("~1 comp_naxbf, %d: in = %f %f %f\n",s->ix, in[0],in[1],in[2]);

	/* Convert to neutral axis 0 to 100 */
	icmMul3By3x4(rin, s->rot[0], in);

//printf("~1 rotate L %f, white %f grey %f black %f\n",rin[0],s->cusp_lab[0][6][0],s->cusp_lab[0][8][0],s->cusp_lab[0][7][0]);

	if (rin[0] >= s->cusp_lab[0][8][0]) {		/* Closer to white */
		ll = icmNorm33(s->cusp_lab[0][6], rin);	/* Distance to white */
		ll = 1.0 - ll/(100.0 - s->cusp_lab[0][8][0]);	/* Normalized to grey distance */
	} else {									/* Closer to black */
		ll = icmNorm33(s->cusp_lab[0][7], rin);	/* Distance to black */
		ll = 1.0 - ll/s->cusp_lab[0][8][0];		/* Normalized to grey distance */
	}
	if (ll < 0.0)
		ll = 0.0;
	else if (ll > 1.0)
		ll = 1.0;

	/* Weight so that it goes to 0.0 close to W & B */
	ll = sqrt(1.0 - ll);

//printf("~1 returning ll %f\n",ll);
	return ll;
}

/* Return a value suitable for blending between the wl, gl and bl L dominance values. */
/* The value is a linear blend value, 0.0 at cusp local grey, 1.0 at white L value */
/* and -1.0 at black L value. */
static double comp_lvc(
smthopt *s,			/* Context for cusp mapping */
double in[3]		/* Non-cusp mapped source value */
) {
	double Lg;
	double ll;

//printf("~1 comp_lvc, %d: in = %f %f %f\n",s->ix, in[0],in[1],in[2]);

	/* Compute the cusp local grey value. */
	if (s->docusp) {
		double lab[3], lch[3];	/* Normalized source values */
		double bb[3];			/* Baricentric coords */
		int c0, c1;			/* Cusp indexes */
		int ld;				/* light/dark index */

		/* Compute src cusp normalized LCh */ 
		icmMul3By3x4(lab, s->rot[0], in);
		icmLab2LCh(lch, lab);
//printf("~1 lab = %f %f %f, lch = %f %f %f\n",lab[0],lab[1],lab[2],lch[0],lch[1],lch[2]);

		/* Locate the source cusps that this point lies between */ 
		for (c0 = 0; c0 < 6; c0++) {
			double sh, h0, h1;

			sh = lch[2];
			c1 = c0 < 5 ? c0 + 1 : 0;

			h0 = s->cusp_lch[0][c0][2];
			h1 = s->cusp_lch[0][c1][2];

			if (h1 < h0) {
				if (sh < h1)
					sh += 360.0;
				h1 += 360.0;
			}
			if (sh >= (h0 - 1e-12) && sh < (h1 + 1e-12))
				break;
		}
		if (c0 >= 6)	/* Assert */
			error("gamut, comp_lvc: unable to locate hue %f cusps\n",lch[2]);

		/* See whether this is light or dark */
		ld = icmPlaneDist3(s->cusp_pe[0][c0], lab) >= 0 ? 0 : 1;
//printf("~1 cusp %d, ld %d (dist %f)\n",c0,ld,icmPlaneDist3(s->cusp_pe[0][c0], lab));

		/* Compute baricentric for input point in simplex */
		icmSub3(bb, lab, s->cusp_lab[0][8]); 
		icmMulBy3x3(bb, s->cusp_bc[0][c0][ld], bb);

//printf("~1 baricentric %f %f %f\n",bb[0],bb[1],bb[2]);

		/* Compute the grey level */
		Lg = s->cusps[0][8][0]
		   + bb[0] * (s->cusps[0][c0][0] - s->cusps[0][8][0])
		   + bb[1] * (s->cusps[0][c1][0] - s->cusps[0][8][0]);  

	} else {
		/* Non-cusp sensitive grey L */
		Lg = s->cusps[0][8][0];
	}
//printf("~1 grey = %f\n",Lg);

	if (in[0] > Lg) {
		ll = (in[0] - Lg)/(s->cusps[0][6][0] - Lg);
		
	} else {
		ll = -(in[0] - Lg)/(s->cusps[0][7][0] - Lg);
	}
//printf("~1 returnin ll %f\n",ll);
	return ll;
}

static double invfunc(
void *fdata,
double *tp
) {
	smthopt *s = (smthopt *)fdata;
	double cv[3];						/* Converted value */
	double tt, rv = 0.0;

	comp_ce(s, cv, tp, s->wt); 

	tt = s->tv[0] - cv[0];
	rv += tt * tt;
	tt = s->tv[1] - cv[1];
	rv += tt * tt;
	tt = s->tv[2] - cv[2];
	rv += tt * tt;
		
//printf("~1 rv %f from %f %f %f -> %f %f %f\n",rv,tp[0],tp[1],tp[2],cv[0],cv[1],cv[2]);
	return rv;
}

/* Inverse of com_ce. We do this by inverting com_ce numerically (slow) */
static void inv_comp_ce(
smthopt *s,			/* Context for cusp mapping */
double out[3], 
double in[3],
gammapweights *wt	/* If NULL, assume 100% */
) {
	double ss[3] = { 20.0, 20.0, 20.0 };		/* search area */
	double tp[3], rv;

	s->tv[0] = tp[0] = in[0];
	s->tv[1] = tp[1] = in[1];
	s->tv[2] = tp[2] = in[2];
	s->wt = wt;

	/* Optimise the point */
	if (powell(&rv, 3, tp, ss, 0.001, 1000, invfunc, (void *)s, NULL, NULL) != 0) {
		error("gammap::nearsmth: inv_comp_ce powell failed on %f %f %f\n", in[0], in[1], in[2]);
	}

//printf("~1 inv_comp_ce: %f %f %f -> %f %f %f\n", s->tv[0], s->tv[1], s->tv[2], tp[0], tp[1], tp[2]);
//comp_ce(s, out, tp, wt);
//printf("~1       check: %f %f %f, DE %f\n", out[0], out[1], out[2], icmNorm33(s->tv,out));

	out[0] = tp[0];
	out[1] = tp[1];
	out[2] = tp[2];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* A set of functions to help handle the weighting configuration */

/* Copy non-negative values from one set of weights to another */
void near_wcopy(
gammapweights *dst,
gammapweights *src
) {

#define NSCOPY(xxx) dst->xxx = src->xxx >= 0.0 ? src->xxx : dst->xxx
//#define NSCOPY(xxx) if (src->xxx >= 0.0) { \
//						printf("Setting %s to %f\n",#xxx, src->xxx); \
//						dst->xxx = src->xxx;						\
//					}

	NSCOPY(c.w.l);
	NSCOPY(c.w.c);
	NSCOPY(c.w.h);
	NSCOPY(c.tw);
	NSCOPY(c.cx);

	NSCOPY(l.o);
	NSCOPY(l.h);
	NSCOPY(l.l);

	NSCOPY(a.o);
	NSCOPY(a.h);
	NSCOPY(a.wl);
	NSCOPY(a.gl);
	NSCOPY(a.bl);

	NSCOPY(a.wlth);
	NSCOPY(a.blpow);

	NSCOPY(a.lxpow);
	NSCOPY(a.lxthr);

	NSCOPY(r.rdl);
	NSCOPY(r.rdh);

	NSCOPY(d.co);
	NSCOPY(d.xo);

	NSCOPY(f.x);
#undef NSCOPY
}

/* Blend a two groups of individual weights into one, given two weightings */
void near_wblend(
gammapweights *dst,
gammapweights *src1, double wgt1,
gammapweights *src2, double wgt2
) {

#define NSBLEND(xxx) dst->xxx = wgt1 * src1->xxx + wgt2 * src2->xxx

	NSBLEND(c.w.l);
	NSBLEND(c.w.c);
	NSBLEND(c.w.h);
	NSBLEND(c.tw);
	NSBLEND(c.cx);

	NSBLEND(l.o);
	NSBLEND(l.h);
	NSBLEND(l.l);

	NSBLEND(a.o);
	NSBLEND(a.h);
	NSBLEND(a.wl);
	NSBLEND(a.gl);
	NSBLEND(a.bl);

	NSBLEND(a.wlth);
	NSBLEND(a.blpow);

	NSBLEND(a.lxpow);
	NSBLEND(a.lxthr);

	NSBLEND(r.rdl);
	NSBLEND(r.rdh);

	NSBLEND(d.co);
	NSBLEND(d.xo);

	NSBLEND(f.x);
#undef NSBLEND
}

/* Expand the compact form of weights into the explicit form. */
/* The explicit form is light and dark of red, yellow, green, cyan, blue, magenta & neutral */
/* Return nz on error */
int expand_weights(gammapweights out[14], gammapweights *in) {
	int i, j;

	/* Set the usage of each slot */
	out[0].ch = gmm_light_red;
	out[1].ch = gmm_light_yellow;
	out[2].ch = gmm_light_green;
	out[3].ch = gmm_light_cyan;
	out[4].ch = gmm_light_blue;
	out[5].ch = gmm_light_magenta;
	out[6].ch = gmm_light_neutral;

	out[7].ch = gmm_dark_red;
	out[8].ch = gmm_dark_yellow;
	out[9].ch = gmm_dark_green;
	out[10].ch = gmm_dark_cyan;
	out[11].ch = gmm_dark_blue;
	out[12].ch = gmm_dark_magenta;
	out[13].ch = gmm_dark_neutral;

//printf("\n~1 expand weights called\n");

	/* mark output so we can recognise having been set or not */
	for (i = 0; i < 14; i++)
		out[i].set = 0;

	/* Expand the compact form to explicit. */

	/* First is default */
	for (i = 0; in[i].ch != gmm_end; i++) {
		if (in[i].ch == gmm_end)
			break;
		if (in[i].ch == gmm_ignore)
			continue;

		if (in[i].ch == gmm_default) {
			for (j = 0; j < 14; j++) {
//printf("~1 Setting %d 0x%x with 0x%x (default)\n",j,out[j].ch,in[i].ch);
				if ((in[i].ch & out[j].ch) == out[j].ch) {
					near_wcopy(&out[j], &in[i]);
					out[j].set = 1;
				}
			}
		}
	}

	/* Then light or dark */
	for (i = 0; in[i].ch != gmm_end; i++) {
		if (in[i].ch == gmm_end)
			break;
		if (in[i].ch == gmm_ignore)
			continue;

		if (in[i].ch == gmm_light_colors
		  || in[i].ch == gmm_dark_colors) {
			for (j = 0; j < 14; j++) {
				if ((in[i].ch & out[j].ch) == out[j].ch) {
//printf("~1 Setting %d 0x%x with 0x%x (light or dark)\n",j,out[j].ch,in[i].ch);
					near_wcopy(&out[j], &in[i]);
					out[j].set = 1;
				}
			}
		}
	}

	/* Then light and dark colors */
	for (i = 0; in[i].ch != gmm_end; i++) {
		if (in[i].ch == gmm_end)
			break;
		if (in[i].ch == gmm_ignore)
			continue;

		if ((in[i].ch & gmc_l_d) == gmc_l_d
		 && (in[i].ch & gmc_colors) != gmc_colors) {
			for (j = 0; j < 14; j++) {
				if ((in[i].ch & out[j].ch) == out[j].ch) {
//printf("~1 Setting %d 0x%x with 0x%x (light and dark color)\n",j,out[j].ch,in[i].ch);
					near_wcopy(&out[j], &in[i]);
					out[j].set = 1;
				}
			}
		}
	}

	/* Last pass is light or dark colors */
	for (i = 0; in[i].ch != gmm_end; i++) {
		if (in[i].ch == gmm_end)
			break;
		if (in[i].ch == gmm_ignore)
			continue;

		if (((in[i].ch & gmc_l_d) == gmc_light
		  || (in[i].ch & gmc_l_d) == gmc_dark)
		 && (in[i].ch & gmc_colors) != gmc_colors) {
			for (j = 0; j < 14; j++) {
				if ((in[i].ch & out[j].ch) == out[j].ch) {
//printf("~1 Setting %d 0x%x with 0x%x (light or dark color)\n",j,out[j].ch,in[i].ch);
					near_wcopy(&out[j], &in[i]);
					out[j].set = 1;
				}
			}
		}
	}

	/* Check every slot has been set */
	for (i = 0; i < 14; i++) {
		if (out[i].set == 0) {
//printf("~1 set %d hasn't been initialized\n",i);
			return 1;
		}
	}
	return 0;
}

/* Tweak weights acording to extra cmy cusp mapping flags or rel override */
void tweak_weights(gammapweights out[14], int dst_cmymap, int rel_oride)  {
	int i;

	for (i = 0; i < 14; i++) {
		if (((dst_cmymap & 0x1) && (out[i].ch & gmc_cyan))
		 || ((dst_cmymap & 0x2) && (out[i].ch & gmc_magenta))
		 || ((dst_cmymap & 0x4) && (out[i].ch & gmc_yellow))) {
//printf("~1 Setting %d 0x%x to 100% cusp map\n",i,out[i].ch);
			out[i].c.w.l = 1.0;		/* 100% mapping */
			out[i].c.w.c = 1.0;
			out[i].c.w.h = 1.0;
			out[i].c.tw = 1.0;		/* Moderate twist */
			out[i].c.cx = 1.0;		/* No expansion */
		}

		if (rel_oride == 1) {		/* A high saturation "clip" like mapping */
			out[i].r.rdl = 1.0;		/* No relative neighbourhood */
			out[i].r.rdh = 1.0;		/* No relative neighbourhood */
			out[i].d.co = 0.0;		/* No depth weighting */
			out[i].d.xo = 0.0;		/* No depth weighting */
			
		} else if (rel_oride == 2) {	/* A maximal feature preserving mapping */
			out[i].r.rdl *= 1.6;	/* Extra neighbourhood size */
			out[i].r.rdh *= 1.6;	/* Extra neighbourhood size */
		}
	}
}

/* Blend a two expanded groups of individual weights into one */
void near_xwblend(
gammapweights *dst,
gammapweights *src1, double wgt1,
gammapweights *src2, double wgt2
) {
	int i;
	for (i = 0; i < 14; i++)
		near_wblend(&dst[i], &src1[i], wgt1, &src2[i], wgt2);
}

/* Convert overall, hue dom & l dom to iweight */
static void comp_iweight(iweight *iw, double o, double h, double l) {
	double c, lc;

	if (h < 0.0)
		h = 0.0;
	else if (h > 1.0)
		h = 1.0;

	if (l < 0.0)
		l = 0.0;
	else if (l > 1.0)
		l = 1.0;

	lc = 1.0 - h;
	c = (1.0 - l) * lc;
	l = l * lc;

	o /= sqrt(l * l + c * c + h * h);

	iw->l = o * l;
	iw->c = o * c;
	iw->h = o * h;
}

/* Given a point location, return the interpolated weighting values at that point. */
/* (Typically non-cusp mapped source location assumed, and source gamut cusps used.) */
/* (Assume init_ce() has been called to setip smthopt!) */
void interp_xweights(gamut *gam, gammapweights *out, double pos[3], 
                     gammapweights in[14], smthopt *s, int cvec) {
	double h, JCh[3], tmp[3];
	int li, ui;			/* The two hue indexes the color is between */
	double lh, uh;		/* Lower/upper hue of two colors */
	double lw, uw;		/* Lower/upper blend values */
	double cusps[6][3];
	gammapweights light, dark;

	/* Convert to polar */
	icmLab2LCh(JCh, pos);

	if (gam->getcusps(gam, cusps) != 0) {	/* Failed */
		int isJab = gam->isJab ? 1 : 0;

		/* Figure out what hextant we're between using generic cusps */
		for (li = 0; li < 6; li++) {
			ui = li < 5 ? li + 1 : 0;
			h = JCh[2];

			lh = gam_hues[isJab][li];		/* use generic ones */
			uh = gam_hues[isJab][ui];
			if (uh < lh) {
				if (h < uh)
					h += 360.0;
				uh += 360.0;
			}
			if (h >= (lh - 1e-12) && h < (uh + 1e-12))
				break;
		}

	} else {

		/* Locate the source cusps that this point lies between */ 
		for (li = 0; li < 6; li++) {
			double tt[3];
			ui = li < 5 ? li + 1 : 0;
			h = JCh[2];

			icmLab2LCh(tt, cusps[li]);
			lh = tt[2];
			icmLab2LCh(tt, cusps[ui]);
			uh = tt[2];

			if (uh < lh) {
				if (h < uh)
					h += 360.0;
				uh += 360.0;
			}
			if (h >= (lh - 1e-12) && h < (uh + 1e-12))
				break;
		}
	}
	if (li >= 6)	/* Assert */
		error("gamut, interp_xweights: unable to locate hue %f cusps\n",JCh[2]);

	/* Compute hue angle blend weights */
	uw = (h - lh)/(uh - lh);
	if (uw < 0.0)
		uw = 0.0;
	else if (uw > 1.0)
		uw = 1.0;
	uw = uw * uw * (3.0 - 2.0 * uw);	/* Apply spline to smooth interpolation */
	lw = (1.0 - uw);

	/* Blend weights at the two hues */
	near_wblend(&light, &in[li], lw, &in[ui], uw);
	near_wblend(&dark, &in[7 + li], lw, &in[7 + ui], uw);

	/* If we're close to the center, blend to the neutral weight */
	if (JCh[1] < NEUTRAL_C) {
		lw = (NEUTRAL_C - JCh[1])/NEUTRAL_C;
		uw = (1.0 - lw);
		near_wblend(&light, &in[6], lw, &light, uw);
		near_wblend(&dark, &in[7 + 6], lw, &dark, uw);
	}

	/* Figure out where we are between light and dark, */
	/* and create blend between their weightings */
	uw = (JCh[0] - DARK_L)/(LIGHT_L - DARK_L);
	if (uw > 1.0)
		uw = 1.0;
	else if (uw < 0.0)
		uw = 0.0;
	uw = uw * uw * (3.0 - 2.0 * uw);	/* Apply spline to smooth interpolation */
	lw = (1.0 - uw);
	near_wblend(out, &dark, lw, &light, uw);

	/* Convert radial dominance weights into raw weights */
	comp_iweight(&out->rl, out->l.o, out->l.h, out->l.l);
	
//printf("~1 %d: src %f %f %f (cvec %d)\n",s->ix, pos[0],pos[1],pos[2],cvec);
	/* Compute l dominance value vs. closness to white or black point */
	{
		double wl, gl, bl, uw, l;

		/* Closness to white and black points */
		uw = comp_lvc(s, pos);

//printf("~1 uw = %f\n",uw);
		if (uw >= 0) {
			
			/* Scale to threshold */
			if (uw > (1.0 - out->a.wlth))
				uw = (uw - 1.0 + out->a.wlth)/out->a.wlth;
			else
				uw = 0.0;
//printf("~1 white, thresholded uw %f\n",uw);

			/* Blend in log ratio space */
			wl = log((1.0 - out->a.wl + 1e-5)/(out->a.wl + 1e-5));
			gl = log((1.0 - out->a.gl + 1e-5)/(out->a.gl + 1e-5));
			l = exp(uw * wl + (1.0 - uw) * gl);
			l = ((1.0 - l) * 1e-5 + 1.0)/(l + 1.0);

		} else {
			uw = -uw;

			/* Apply power */
			uw = pow(uw, out->a.blpow);
//printf("~1 black with power uw %f\n",uw);

			/* Blend in log ratio space */
			gl = log((1.0 - out->a.gl + 1e-5)/(out->a.gl + 1e-5));
			bl = log((1.0 - out->a.bl + 1e-5)/(out->a.bl + 1e-5));
			l = exp(uw * bl + (1.0 - uw) * gl);
			l = ((1.0 - l) * 1e-5 + 1.0)/(l + 1.0);
		}
//printf("~1 wl %f, gl %f, bl %f -> %f\n",out->a.wl,out->a.gl,out->a.bl,l);

		/* Convert absolute dominance weights into raw weights */
		comp_iweight(&out->ra, out->a.o, out->a.h, l);
//printf("~1 l %f, h %f, ra l %f c %f h %f\n\n", l, out->a.h, out->ra.l, out->ra.c, out->ra.h);
	}
}

/* Callback used by compdstgamut() to establish the expected compression */
/* mapping direction. */
static void cvect(
void *cntx,			/* smthopt * */
double *p2,			/* Return point displaced from p1 in desired direction */
double *p1			/* Given point */
) {
	double vv, gv[3], lv[3];
	smthopt *s = (smthopt *)cntx;
	gammapweights out;

	interp_xweights(s->sgam, &out, p1, s->xwh, s, 1);

//printf("~1 at %f %f %f, lch weight %f %f %f\n", p1[0], p1[1], p1[2], out.ra.l, out.ra.c, out.ra.h);
	/* Now we need to convert the absolute weighting out.ra into a vector */ 
	/* We do this in a very simple minded fashion. The hue weighting is ignored, */
	/* because we assume a direction towards the neutral axis. The C weight is */
	/* assumed to be the weight towards the grey point, while the L weight */
	/* assumed to be the weight towards the point on the neutral axis with */
	/* the same L value. */

	/* Parameter along neutral axis black to white */
	vv = (p1[0] - s->cusps[0][7][0])/(s->cusps[0][6][0] - s->cusps[0][7][0]);
	/* lv is point at same L on neutral axis */
	lv[0] = p1[0];
	lv[1] = vv * (s->cusps[0][6][1] - s->cusps[0][7][1]) + s->cusps[0][7][1];
	lv[2] = vv * (s->cusps[0][6][2] - s->cusps[0][7][2]) + s->cusps[0][7][2];
	icmSub3(lv, lv, p1);		/* Vector */
	icmNormalize3(lv, lv, out.ra.l);

	icmSub3(gv, s->cusps[0][8], p1);	/* Grey vector */
	icmNormalize3(gv, gv, out.ra.c);

	icmAdd3(p2, gv, p1);
	icmAdd3(p2, lv, p2);				/* Combined destination */
//printf("~1 p2 %f %f %f\n", p2[0], p2[1], p2[2]);
}

/* Shrink function */
static void doshrink(smthopt *s, double *out, double *in, double shrink) {
	double rad, len, p2[3];

	cvect((void *)s, p2, in); /* Get shrink direction */

	/* Conservative radius of point */
	rad = sqrt(in[1] * in[1] + in[2] * in[2]);
		
	len = shrink;
	if (rad < (2.0 * shrink))
		len = rad * 0.5;
				
	icmNormalize33(out, p2, in, len);
}

/* Convenience function. Given a mapping vector, return the */
/* intersection with the given gamut that is in the mapping */
/* direction. Return NZ if no intersection */
static int vintersect(
gamut *g, 
int *p1out,			/* Return nz if p1 is outside the gamut */ 
double isec[3],		/* Return intersection point */
double p1[3],		/* First point */
double p2[3]		/* Second point */
) {
	gispnt lp[40];
	int ll, i, bi;

	if ((ll = g->vector_isectns(g, p1, p2, lp, 40)) == 0)
		return 1;

	/* Locate the segment or non-segment the source lies in */
	for (bi = -1, i = 0; i < ll; i += 2) {

		if ((i == 0 || lp[i-1].pv < 0.0)	/* p1 is outside gamut */
		 && lp[i].pv >= -1e-2) {
			bi = i;
			if (p1out != NULL)
				*p1out = 1;
			break;
		}

		if (lp[i].pv <= 0.0					/* p1 is inside gamut */
		 && lp[i+1].pv >= -1e-2) {
			bi = i+1;
			if (p1out != NULL)
				*p1out = 0;
			break;
		}
	}
	if (bi < 0)
		return 1;

	if (isec != NULL)
		icmCpy3(isec, lp[bi].ip);
	
	return 0;
}

/* Convenience function. Given a point and an inwards mapping vector, */
/* if the point is within the gamut, return the first intersection in */
/* the opposite to vector direction. If the point is outside the gamut, */
/* return the first intersction in the vector direction. */
/* Return NZ if no intersection */
static int vintersect2(
gamut *g, 
int *p1out,			/* Return nz if p1 is outside the gamut */ 
double isec[3],		/* Return intersection point */
double vec[3],		/* Vector */
double p1[3]		/* Point */
) {
	gispnt lp[40];
	double p2[3];
	int ll, i, bi;

	icmAdd3(p2, p1, vec);

	if ((ll = g->vector_isectns(g, p1, p2, lp, 40)) == 0)
		return 1;

	/* Locate the segment or non-segment the source lies in */
	for (bi = -1, i = 0; i < ll; i += 2) {

		if ((i == 0 || lp[i-1].pv < 0.0)	/* p1 is outside gamut, */
		 && lp[i].pv >= 0.0) {			/* so look in +ve pv direction. */
			bi = i;
			if (p1out != NULL)
				*p1out = 1;
			break;
		}

		if (lp[i].pv <= 0.0					/* p1 is inside gamut, */
		 && lp[i+1].pv >= 0.0) {			/* so look in -ve pv direction. */
			bi = i;
			if (p1out != NULL)
				*p1out = 0;
			break;
		}
	}
	if (bi < 0)
		return 1;

	if (isec != NULL)
		icmCpy3(isec, lp[bi].ip);
	
	return 0;
}


/* ============================================ */

/* Return the maximum number of points that will be generated */
int near_smooth_np(
	gamut *sc_gam,		/* Source colorspace gamut */
	gamut *si_gam,		/* Source image gamut (== sc_gam if none) */
	gamut *d_gam,		/* Destination colorspace gamut */
	double xvra			/* Extra vertex ratio */
) {
	gamut *p_gam;		/* Gamut used for points - either source colorspace or image */
	int ntpts, nmpts, nspts, nipts, ndpts;

	nspts = sc_gam->nverts(sc_gam);
	nipts = si_gam->nverts(si_gam);
	ndpts = d_gam->nverts(d_gam);
	p_gam = sc_gam;

	/* Target number of points is max of any gamut */
	ntpts = nspts > nipts ? nspts : nipts;
	ntpts = ntpts > ndpts ? ntpts : ndpts;
	ntpts = (int)(ntpts * xvra + 0.5);

	/* Use image gamut if it exists */
	if (nspts < nipts || si_gam != sc_gam) {
		nspts = nipts;		/* Use image gamut instead */
		p_gam = si_gam;
	}
	xvra = ntpts/(double)nspts;
	nmpts = p_gam->nssverts(p_gam, xvra);	/* Stratified Sampling source points */

	return nmpts;
}


/* ============================================ */
/* Return a list of points. Free list after use */
/* Return NULL on error */
nearsmth *near_smooth(
int verb,			/* Verbose flag */
int *npp,			/* Return the actual number of points returned */
gamut *sc_gam,		/* Source colorspace gamut - uses cusp info if availablle */
gamut *si_gam,		/* Source image gamut (== sc_gam if none), just used for surface. */
gamut *d_gam,		/* Destination colorspace gamut */
int src_kbp,		/* Use K only black point as src gamut black point */
int dst_kbp,		/* Use K only black point as dst gamut black point */
double d_bp[3],		/* Override destination target black point - may be NULL */
gammapweights xwh[14],/* Structure holding expanded hextant weightings */
double gamcknf,		/* Gamut compression knee factor, 0.0 - 1.0 */
double gamxknf,		/* Gamut expansion knee factor, 0.0 - 1.0 */
int    usecomp,		/* Flag indicating whether smoothed compressed value will be used */
int    useexp,		/* Flag indicating whether smoothed expanded value will be used */
double xvra,		/* Extra number of vertexes ratio */
int    mapres,		/* Grid res for 3D RSPL */
double mapsmooth,	/* Target smoothing for 3D RSPL */
datai map_il,		/* Preliminary rspl input range */
datai map_ih,
datao map_ol,		/* Preliminary rspl output range */
datao map_oh 
) {
	smthopt opts;	/* optimisation and cusp mapping context */
	int ix, i, j, k;
	gamut *p_gam;	/* Gamut used for points - either source colorspace or image */
	gamut *sci_gam;	/* Intersection of src and img gamut gamut */
	gamut *di_gam;	/* Modified destination gamut suitable for mapping from sci_gam. */
					/* If just compression, this is the smaller of sci_gam and d_gam. */
					/* If just expansion, this is the sci_gam expanded by d_gam - sc_gam. */
					/* If both exp & comp, then where
						d_gam is outside sci_gam this is sci_gam expanded by d_gam - sc_gam
						                    else it is the smaller of sci_gam and d_gam */
	gamut *nedi_gam;/* Same as above, but not expanded. */
	int nmpts;		/* Number of mapping gamut points */
	nearsmth *smp;	/* Absolute delta E weighting */
	int pass;
	int it;
	rspl *evectmap = NULL;	/* evector map */
	double codf;	/* Itteration overshoot/damping factor */
	double mxmv;	/* Maximum a point gets moved */
	int nmxmv;		/* Number of maxmoves less than stopping threshold */

	/* Check gamuts are compatible */
	if (sc_gam->compatible(sc_gam, d_gam) == 0
	 || (si_gam != NULL && sc_gam->compatible(sc_gam, si_gam) == 0)) {
		fprintf(stderr,"gamut map: Gamuts aren't compatible\n");
		*npp = 0;
		return NULL;
	}

	{
		int ntpts, nspts, nipts, ndpts;

		nspts = sc_gam->nverts(sc_gam);
		nipts = si_gam->nverts(si_gam);
		ndpts = d_gam->nverts(d_gam);
		p_gam = sc_gam;

		/* Target number of points is max of any gamut */
		ntpts = nspts > nipts ? nspts : nipts;
		ntpts = ntpts > ndpts ? ntpts : ndpts;
		ntpts = (int)(ntpts * xvra + 0.5);

		/* Use image gamut if it exists */
		if (nspts < nipts || si_gam != sc_gam) {
			nspts = nipts;		/* Use image gamut instead */
			p_gam = si_gam;
		}
		xvra = ntpts/(double)nspts;
		nmpts = p_gam->nssverts(p_gam, xvra);	/* Stratified Sampling source points */

		if (verb) printf("Vertex count: mult. = %f, src %d, img %d dst %d, target %d\n",
		                                                   xvra,nspts,nipts,ndpts,nmpts);
	}

	/* Setup opts structure */
	opts.useexp = useexp;	/* Expansion used ? */
	opts.debug = 0;			/* No debug powell() failure */
	opts.xwh = xwh;			/* Weightings */
	opts.sgam = sc_gam;		/* Source colorspace gamut */

	/* Setup source & dest neutral axis transform if white/black available. */
	/* If cusps are available, also figure out the transformations */
	/* needed to map source cusps to destination cusps */
	init_ce(&opts, sc_gam, d_gam, src_kbp, dst_kbp, d_bp);

	/* Allocate our guide points */
	if ((smp = (nearsmth *)calloc(nmpts, sizeof(nearsmth))) == NULL) { 
		fprintf(stderr,"gamut map: Malloc of near smooth points failed\n");
		*npp = 0;
		return NULL;
	}

	/* Create a source gamut surface that is the intersection of the src colorspace */
	/* and image gamut, in case (for some strange reason) the image gamut. */
	/* exceeds the source colorspace size. */
	sci_gam = sc_gam;			/* Alias to source space gamut */
	if (si_gam != sc_gam) {
		if ((sci_gam = new_gamut(0.0, 0, 0)) == NULL) {
			fprintf(stderr,"gamut map: new_gamut failed\n");
			free_nearsmth(smp, nmpts);
			*npp = 0;
			return NULL;
		}
		sci_gam->intersect(sci_gam, sc_gam, si_gam);
#ifdef SAVE_VRMLS
		{
			char sci_gam_name[40] = "sci_gam";
			strcat(sci_gam_name, vrml_ext());
			printf("###### gamut/nearsmth.c: writing diagnostic sci_gam%s and di_gam%s\n",vrml_ext(),vrml_ext());
			sci_gam->write_vrml(sci_gam, sci_gam_name, 1, 0);
		}
#endif
	}

	di_gam = sci_gam;			/* Default no compress or expand */
	if (usecomp || useexp) {
		if ((di_gam = new_gamut(0.0, 0, 0)) == NULL) {
			fprintf(stderr,"gamut map: new_gamut failed\n");
			if (si_gam != sc_gam)
				sci_gam->del(sci_gam);
			free_nearsmth(smp, nmpts);
			*npp = 0;
			return NULL;
		}
		if (useexp) {
			/* Non-expand di_gam for testing double back img points against */
			if ((nedi_gam = new_gamut(0.0, 0, 0)) == NULL) {
				fprintf(stderr,"gamut map: new_gamut failed\n");
				di_gam->del(di_gam);
				if (si_gam != sc_gam)
					sci_gam->del(sci_gam);
				free_nearsmth(smp, nmpts);
				*npp = 0;
				return NULL;
			}
		} else {
			nedi_gam = di_gam;
		}

		/* Initialise this gamut with a destination mapping gamut. */
		/* This will be the smaller of the image and destination gamut if compressing, */
		/* and will be expanded by the (dst - src) if expanding. */
		di_gam->compdstgamut(di_gam, sci_gam, sc_gam, d_gam, usecomp, useexp, nedi_gam,
		                     cvect, &opts);
	}

#ifdef SAVE_VRMLS
	{
		char di_gam_name[30] = "di_gam";
		strcat(di_gam_name, vrml_ext());
		di_gam->write_vrml(di_gam, di_gam_name, 1, 0);
	}
#endif

	/* Create a list of the mapping guide points, setup for a null mapping */
	VA(("Creating the mapping guide point list\n"));
	for (ix = i = 0; i < nmpts; i++) {
		double imv[3], imr;		/* Image gamut source point and radius */
		double inorm[3];		/* Normal of image gamut surface at src point */

		/* Get the source color/image space vertex value we are going */
		/* to use as a sample point.  */
		if ((ix = p_gam->getssvert(p_gam, &imr, imv, inorm, ix)) < 0) {
			break;
		}
//printf("~1 got point %d out of %d\n",i+1,nmpts);

		if (p_gam != sc_gam) {		/* If src colorspace point, map to img gamut surface */
			imr = sci_gam->radial(sci_gam, imv, imv);
		}

		/* If point is within non-expanded modified destination gamut, */
		/* then it is a "double back" image point, and should be ignored. */
		if (nedi_gam->radial(nedi_gam, NULL, imv) > (imr + 1e-4)) {
			VB(("Rejecting point %d because it's inside destination\n",i));
			i--;
			continue;
		}

		/* Lookup radialy equivalent point on modified destination gamut, */
		/* in case we need it for compression or expansion */
		smp[i].drr = di_gam->radial(di_gam, smp[i].drv, imv);

		/* Default setup a null mapping of source image space point to source image point */
		smp[i].vflag = smp[i].gflag = 0;
		smp[i].dr = smp[i].sr = smp[i]._sr = imr;
		smp[i].dv[0] = smp[i].sv[0] = smp[i]._sv[0] = imv[0];
		smp[i].dv[1] = smp[i].sv[1] = smp[i]._sv[1] = imv[1];
		smp[i].dv[2] = smp[i].sv[2] = smp[i]._sv[2] = imv[2];
		smp[i].sgam = sci_gam;
		smp[i].dgam = sci_gam;
		smp[i].mapres = mapres;	

		VB(("In Src %d = %f %f %f\n",i,smp[i].sv[0],smp[i].sv[1],smp[i].sv[2]));

		/* If we're going to comp. or exp., check that the guide vertex is not */
		/* on the wrong side of the image gamut, due to the it being */
		/* a small subset of the source colorspace, displaced to one side. */
		/* Because of the gamut convexity limitations, this amounts */
		/* to the source surface at the vertex being in the direction */
		/* of the center. */
		if (usecomp != 0 || useexp != 0) {
			double mv[3], ml;		/* Radial inward mapping vector */
			double dir;

			icmSub3(mv, sci_gam->cent, smp[i].sv);	/* Vector to center */
			ml = icmNorm3(mv);						/* It's length */

			if (ml > 0.001) {
				dir = icmDot3(mv, inorm);	/* Compare to normal of src triangle */
//printf("~1 ix %d, dir = %f, dir/len = %f\n",i,dir, dir/ml);
				dir /= ml;
				if (dir < 0.02) {	/* If very shallow */
//printf("~1 rejecting point %d because it's oblique\n",i);
					VB(("Rejecting point %d because it's oblique\n",i));
					i--;
					continue;
				}
			}
		}

		/* Set some default extra guide point values */
		smp[i].anv[0] = smp[i].aodv[0] = smp[i].dv[0];
		smp[i].anv[1] = smp[i].aodv[1] = smp[i].dv[1];
		smp[i].anv[2] = smp[i].aodv[2] = smp[i].dv[2];

		VB(("Src %d = %f %f %f\n",i,smp[i].sv[0],smp[i].sv[1],smp[i].sv[2]));
		VB(("Dst %d = %f %f %f\n",i,smp[i].dv[0],smp[i].dv[1],smp[i].dv[2]));
	}
	nmpts = i;			/* Number of points after rejecting any */
	*npp = nmpts;

	/* Don't need this anymore */
	if (nedi_gam != di_gam)
		nedi_gam->del(nedi_gam);
	nedi_gam = NULL;

	/* If nothing to be compressed or expanded, then return */
	if (usecomp == 0 && useexp == 0) {
		VB(("Neither compression nor expansion defined\n"));
		if (si_gam != sc_gam)
			sci_gam->del(sci_gam);
		if (di_gam != sci_gam && di_gam != sci_gam)
			di_gam->del(di_gam);
		return smp;
	}

	/* Set the parameter weights for each point */
	for (i = 0; i < nmpts; i++) {
		opts.ix = i;			/* Point in question */
		opts.p = &smp[i];

		/* Determine the parameter weighting for this point */
		interp_xweights(opts.sgam, &smp[i].wt, smp[i]._sv, opts.xwh, &opts, 0);
	}

	VA(("Setting up cusp rotated compression or expansion mappings\n"));
	VB(("rimv = Cusp rotated cspace/image gamut source point\n"));
	VB(("imv  = cspace/image gamut source point\n"));
	VB(("drv  = Destination space radial point and radius \n"));

	/* Setup the cusp rotated compression or expansion mappings */
	for (i = 0; i < nmpts; i++) {
		double imv[3], imr;		/* cspace/image gamut source point and radius */
		double rimv[3], rimr;	/* Cusp rotated cspace/image gamut source point and radius */

		opts.ix = i;			/* Point in question */
		opts.p = &smp[i];

		/* Grab the source image point */
		imr = smp[i]._sr;
		imv[0] = smp[i]._sv[0];
		imv[1] = smp[i]._sv[1];
		imv[2] = smp[i]._sv[2];

		/* Compute the cusp rotated version of the cspace/image points */
		comp_ce(&opts, rimv, imv, &smp[i].wt);
		VB(("%f de, ix %d: cusp mapped %f %f %f -> %f %f %f\n", icmNorm33(rimv,imv), i, imv[0], imv[1], imv[2], rimv[0], rimv[1], rimv[2]));
		rimr = icmNorm33(rimv, sci_gam->cent);

		/* Default setup a no compress or expand mapping of */
		/* source space/image point to modified destination gamut. */
		smp[i].sr = rimr;
		smp[i].sv[0] = rimv[0];		/* Temporary rotated src point */
		smp[i].sv[1] = rimv[1];
		smp[i].sv[2] = rimv[2];
		smp[i].sgam = sci_gam;
		smp[i].dgam = di_gam;

		VB(("\n"));
		VB(("point %d:, rimv = %f %f %f, rimr = %f\n",i,rimv[0],rimv[1],rimv[2],rimr)); 
		VB(("point %d:, imv  = %f %f %f, imr  = %f\n",i,imv[0],imv[1],imv[2],imr)); 
		VB(("point %d:, drv  = %f %f %f, drr  = %f\n",i,smp[i].drv[0],smp[i].drv[1],smp[i].drv[2],smp[i].drr)); 

		/* Set a starting point for the optimisation */
		smp[i].dgam->nearest(smp[i].dgam, smp[i].dv, smp[i].sv);
		smp[i].dr = icmNorm33(smp[i].dv, smp[i].dgam->cent);

		/* Re-lookup radialy equivalent point on destination gamut, */
		/* to match rotated source */
		smp[i].drr = smp[i].dgam->radial(smp[i].dgam, smp[i].drv, smp[i].sv);

		/* A default average neighbour value */
		smp[i].anv[0] = smp[i].drv[0];
		smp[i].anv[1] = smp[i].drv[1];
		smp[i].anv[2] = smp[i].drv[2];
	}

	/* Setup the white & black point blend factor, that makes sure the white and black */
	/* points are not displaced. */
	for (i = 0; i < nmpts; i++) {
		smp[i].naxbf = comp_naxbf(&opts, smp[i]._sv);
//printf("~1 point %d, comp_lvc = %f, naxbf = %f\n",i,comp_lvc(&opts, smp[i]._sv),smp[i].naxbf);
	}

	/* Setup the 3D -> 2D tangent conversion ready for guide vector optimization */
	{
		double ta[3] = { 50.0, 0.0, 0.0 };
		double tc[3] = { 0.0, 0.0, 0.0 };
		
		for (ix = 0; ix < nmpts; ix++) {
			/* Coompute a rotation that brings the target point location to 50,0,0 */
			icmVecRotMat(smp[ix].m2d, smp[ix].sv, sc_gam->cent, ta, tc);

			/* And inverse */
			icmVecRotMat(smp[ix].m3d, ta, tc, smp[ix].sv, sc_gam->cent);
		}
	}

	/* Figure out which neighbors of the source values to use */
	/* for the relative error calculations. */
	/* Locate the neighbor within the radius for this point, */
	/* and weight them with a Gausian filter weight. */
	/* The radius is computed on the normalised surface for this point. */ 
	VA(("Establishing filter neighbourhoods\n"));
	{
		double mm[3][4];	/* Tangent alignment rotation */
		double m2[2][2];	/* Additional matrix to alight a with L axis */
		double ta[3] = { 50.0, 0.0, 0.0 };
		double tc[3] = { 0.0, 0.0, 0.0 };
		double avgnd = 0.0;	/* Total the average number of neighbours */
		int minnd = 1e6;	/* Minimum number of neighbours */
		
		for (ix = 0; ix < nmpts; ix++) {
			double tt[3], rrdl, rrdh, rrdc, dd;
			double msv[3], ndx[4];	/* Midpoint source value, quadrant distance */
			double pr;				/* Average point radius */

//printf("~1 computing neigbourhood for point %d at %f %f %f\n",ix, smp[ix].sv[0], smp[ix].sv[1], smp[ix].sv[2]);
			/* Compute a rotation that brings the target point location to 50,0,0 */
			icmNormalize33(tt, smp[ix].sv, smp[ix].sgam->cent, 1.0);
			icmVecRotMat(mm, tt, smp[ix].sgam->cent, ta, tc);

			/* Add another rotation to orient it so that [1] corresponds */
			/* with the L direction, and [2] corresponds with the */
			/* hue direction. */
			m2[0][0] = m2[1][1] = 1.0;
			m2[0][1] = m2[1][0] = 0.0;
			tt[0] = smp[ix].sv[0] + 1.0;
			tt[1] = smp[ix].sv[1];
			tt[2] = smp[ix].sv[2];
			icmNormalize33(tt, tt, smp[ix].sgam->cent, 1.0);
			icmMul3By3x4(tt, mm, tt);
			dd = tt[1] * tt[1] + tt[2] * tt[2];
			if (dd > 1e-6) {		/* There is a sense of L direction */

				/* Create the 2x2 rotation matrix */
				dd = sqrt(dd);
				tt[1] /= dd;
				tt[2] /= dd;

				m2[0][0] = m2[1][1] = tt[1];
				m2[0][1] = tt[2];
				m2[1][0] = -tt[2];
			}

			/* Make rr inversely proportional to radius, so that */
			/* filter scope is constant delta E */
			rrdl = smp[ix].wt.r.rdl;
			rrdh = smp[ix].wt.r.rdh;
//printf("~1 rdl %f, rdh %f\n",smp[ix].wt.r.rdl,smp[ix].wt.r.rdh);
			if (rrdl < 1e-3) rrdl = 1e-3;
			if (rrdh < 1e-3) rrdh = 1e-3;

			/* Average radius of source and destination */
			pr = 0.5 * (smp[ix]._sr + smp[ix].dr);

//printf("~1 pr = %f from _sr %f & dr %f\n",pr,smp[ix]._sr,smp[ix].dr);
			if (pr < 5.0)
				pr = 5.0;
			rrdl *= 50.0/pr;
			rrdh *= 50.0/pr;
			rrdc = 0.5 * (rrdl + rrdh);		/* Chrominance radius */

			/* Scale the filter radius by the L/C value, */
			/* so that the filters are largest at the equator, and smallest */
			/* at the white & black points. This allows the wt.a.lx wt.a.cx to work. */
			pr = smp[ix].naxbf + 0.1;	/* "spherical" type weighting, 0.707 at 45 degrees */
			rrdl *= pr;
			rrdh *= pr;
			rrdc *= pr;
//printf("~1 at %f %f %f, rrdl = %f, rrdh = %f\n",smp[ix]._sv[0], smp[ix]._sv[1], smp[ix]._sv[2], rrdl, rrdh);

			smp[ix].nnd = 0;		/* Nothing in lists */

			/* Search for points within the gausian radius */
			for (i = 0; i < nmpts; i++) {
				double x, y, z, tv[3];

				/* compute tangent alignment rotated location */
				icmNormalize33(tt, smp[i].sv, smp[ix].sgam->cent, 1.0);
				icmMul3By3x4(tv, mm, tt);
				icmMulBy2x2(&tv[1], m2, &tv[1]);

				x = tv[1]/rrdl;
				y = tv[2]/rrdh;
				z = (tv[0] - 50.0)/rrdc;
			
				/* Compute normalized delta normalized tangent surface */ 
				dd = x * x + y * y + z * z;

				/* If we're within the direction filtering radius, */
				/* and not of the opposite hue */
				if (dd <= 1.0 && tv[0] > 0.0) {
					double w;

					dd = sqrt(dd);		/* Convert to radius <= 1.0 */

					/* Add this point into the list */
					if (smp[ix].nnd >= smp[ix]._nnd) {
						neighb *nd;
						int _nnd;
						_nnd = 5 + smp[ix]._nnd * 2;
						if ((nd = (neighb *)realloc(smp[ix].nd, _nnd * sizeof(neighb))) == NULL) {
							VB(("realloc of neighbs at vector %d failed\n",ix));
							if (si_gam != sc_gam)
								sci_gam->del(sci_gam);
							if (di_gam != sci_gam && di_gam != sci_gam)
								di_gam->del(di_gam);
							free_nearsmth(smp, nmpts);
							*npp = 0;
							return NULL;
						}
						smp[ix].nd = nd;
						smp[ix]._nnd = _nnd;
					}
					smp[ix].nd[smp[ix].nnd].n = &smp[i];

					/* Box filter */
//					w = 1.0;

					/* Triangle filter */
//					w = 1.0 - dd; 

//					/* Cubic spline filter */
//					w = 1.0 - dd; 
//					w = w * w * (3.0 - 2.0 * w);

					/* Gaussian filter (default) */
					w = exp(-9.0 * dd/2.0);

					/* Sphere filter */
//					w = sqrt(1.0 - dd * dd); 

					/* Sinc^2 filter */
//					w = 3.1415926 * dd;
//					if (w < 1e-9)
//						w = 1e-9;
//					w = sin(w)/w;
//					w = w * w;

					smp[ix].nd[smp[ix].nnd].w = w;		/* Will be normalized to sum to 1.0 */

//					/* Sphere filter for depth */
//					w = sqrt(1.0 - dd * dd); 

					/* Cubic spline filter for depth */
//					w = 1.0 - dd; 
//					w = w * w * (3.0 - 2.0 * w);

					/* Gaussian filter for depth (default) */
					w = exp(-9.0 * dd/2.0);

					smp[ix].nd[smp[ix].nnd].rw = w;		/* Won't be normalized */

//printf("~1 adding %d at %f %f %f, rad %f L %f, w %f dir.\n",i, smp[i].sv[0], smp[i].sv[1], smp[i].sv[2],sqrt(dd),tv[0],smp[ix].nd[smp[ix].nnd].w);

					smp[ix].nnd++;
				}
			}
			if (smp[ix].nnd < minnd)
				minnd = smp[ix].nnd;
			avgnd += (double)smp[ix].nnd;
//printf("~1 total of %d dir neigbours\n\n",smp[ix].nnd);

		}
		avgnd /= (double)nmpts;

		if (verb) printf("Average number of direction guide neigbours = %f, min = %d\n",avgnd,minnd);

		/* Now normalize each points weighting */
		for (i = 0; i < nmpts; i++) {
			double tw;

			/* Adjust direction weights to sum to 1.0 */
			for (tw = 0.0, j = 0; j < smp[i].nnd; j++) {
				tw += smp[i].nd[j].w;
			}
			for (j = 0; j < smp[i].nnd; j++) {
				smp[i].nd[j].w /= tw;
			}

		}
	}

#ifdef SHOW_NEIGB_WEIGHTS
	{
		vrml *wrl = NULL;
		double yellow[3] = { 1.0, 1.0, 0.0 };
		double red[3] = { 1.0, 0.0, 0.0 };
		double green[3] = { 0.0, 1.0, 0.0 };
		double pp[3];

		for (i = 0; i < nmpts; i++) {
			double maxw;

			if ((wrl = new_vrml("weights", 1, vrml_lab)) == NULL)
				error("New %s failed for '%s%s'",vrml_format(),"weights",vrml_ext());

			maxw = 0.0;
			for (j = 0; j < smp[i].nnd; j++) {
				if (smp[i].nd[j].w > maxw)	
					maxw = smp[i].nd[j].w;
			}
			for (j = 0; j < smp[i].nnd; j++) {
				wrl->add_col_vertex(wrl, 0, smp[i].sgam->cent, smp[i].nd[j].n == &smp[i] ? red : yellow);
				icmNormalize33(pp, smp[i].nd[j].n->_sv, smp[i].sgam->cent, smp[i].nd[j].w * 50.0/maxw);
				wrl->add_col_vertex(wrl, 0, pp, smp[i].nd[j].n == &smp[i] ? red : yellow);
			}
			wrl->make_lines(wrl, 0, 2);

			wrl->del(wrl);
			printf("Waiting for input after writing 'weights%s' for point %d:\n",vrml_ext(),i);
			getchar();
		}
	}
#endif /* SHOW_NEIGB_WEIGHTS */

	/* Optimise the location of the source to destination mapping. */
	if (verb) printf("Optimizing source to destination mapping...\n");

	VA(("Doing first pass to locate the nearest point\n"));
	/* First pass to locate the weighted nearest point, to use in subsequent passes */
	{
		double s[2] = { 20.0, 20.0 };		/* 2D search area */
		double iv[3];						/* Initial start value */
		double nv[2];						/* 2D New value */
		double tp[3];						/* Resultint value */
		double ne;							/* New error */
		int notrials = NO_TRIALS;

		for (i = 0; i < nmpts; i++) {		/* Move all the points */
			double bnv[2];					/* Best 2d value */
			double brv;						/* Best return value */
			int trial;
			double mv;

			opts.pass = 0;		/* Itteration pass */
			opts.ix = i;		/* Point to optimise */
			opts.p = &smp[i];

			/* If the img point is within the destination, then we're */
			/* expanding, so temporarily swap src and radial dest. */
			/* (??? should we use the cvect() direction to determine swap, */
			/*      rather than radial ???) */
			smp[i].swap = 0;
			if (useexp && smp[i].dr > (smp[i].sr + 1e-9)) {
				gamut *tt;
				double dd;

				smp[i].swap = 1;
				tt = smp[i].dgam; smp[i].dgam = smp[i].sgam; smp[i].sgam = tt;

				smp[i].dr = smp[i].sr;
				smp[i].dv[0] = smp[i].sv[0];
				smp[i].dv[1] = smp[i].sv[1];
				smp[i].dv[2] = smp[i].sv[2];

				smp[i].sr = smp[i].drr;
				smp[i].sv[0] = smp[i].drv[0];
				smp[i].sv[1] = smp[i].drv[1];
				smp[i].sv[2] = smp[i].drv[2];
			}
			
			/* Convert our start value from 3D to 2D for speed. */
			icmMul3By3x4(iv, smp[i].m2d, smp[i].dv);
			nv[0] = iv[0] = iv[1];
			nv[1] = iv[1] = iv[2];

			/* Do several trials from different starting points to avoid */
			/* any local minima, particularly with nearest mapping. */
			brv = 1e38;
			for (trial = 0; trial < notrials; trial++) {
				double rv;			/* Temporary */

				/* Optimise the point */
				if (powell(&rv, 2, nv, s, 0.01, 1000, optfunc1, (void *)(&opts), NULL, NULL) == 0
				    && rv < brv) {
					brv = rv;
//printf("~1 point %d, trial %d, new best %f\n",i,trial,sqrt(rv));
					bnv[0] = nv[0];
					bnv[1] = nv[1];
				}
//else printf("~1 powell failed with rv = %f\n",rv);
				/* Adjust the starting point with a random offset to avoid local minima */
				nv[0] = iv[0] + d_rand(-20.0, 20.0);
				nv[1] = iv[1] + d_rand(-20.0, 20.0);
			}
			if (brv == 1e38) {		/* We failed to get a result */
				fprintf(stderr, "multiple powells failed to get a result (1)\n");
#ifdef DEBUG_POWELL_FAILS
				/* Optimise the point with debug on */
				opts.debug = 1;
				icmMul3By3x4(iv, smp[i].m2d, smp[i].dv);
				nv[0] = iv[0] = iv[1];
				nv[1] = iv[1] = iv[2];
				powell(NULL, 2, nv, s, 0.01, 1000, optfunc1, (void *)(&opts), NULL, NULL);
#endif
				if (si_gam != sc_gam)
					sci_gam->del(sci_gam);
				if (di_gam != sci_gam && di_gam != sci_gam)
					di_gam->del(di_gam);
				free_nearsmth(smp, nmpts);
				*npp = 0;
				return NULL;
			}
		
			/* Convert best result 2D -> 3D */
			tp[2] = bnv[1];
			tp[1] = bnv[0];
			tp[0] = 50.0;
			icmMul3By3x4(tp, smp[i].m3d, tp);

			/* Remap it to the destinaton gamut surface */
			smp[i].dgam->radial(smp[i].dgam, tp, tp);
			icmCpy3(smp[i].aodv, tp);

			/* Undo any swap */
			if (smp[i].swap) {
				gamut *tt;
				double dd;

				tt = smp[i].dgam; smp[i].dgam = smp[i].sgam; smp[i].sgam = tt;

				/* We get the point on the real src gamut out when swap */
				smp[i]._sv[0] = smp[i].aodv[0];
				smp[i]._sv[1] = smp[i].aodv[1];
				smp[i]._sv[2] = smp[i].aodv[2];

				/* So we need to compute cusp mapped sv */
				comp_ce(&opts, smp[i].sv, smp[i]._sv, &smp[i].wt);
				smp[i].sr = icmNorm33(smp[i].sv, smp[i].sgam->cent);

				VB(("Exp Src %d = %f %f %f\n",i,smp[i]._sv[0],smp[i]._sv[1],smp[i]._sv[2]));
				smp[i].aodv[0] = smp[i].drv[0];
				smp[i].aodv[1] = smp[i].drv[1];
				smp[i].aodv[2] = smp[i].drv[2];
			}
		}
		if (verb) {
			printf("."); fflush(stdout);
		}
	}

	VA(("Locating weighted mapping vectors without smoothing:\n"));
	/* Second pass to locate the optimized overall weighted point nrdv[], */
	/* not counting relative error. */
	{
		double s[2] = { 20.0, 20.0 };		/* 2D search area */
		double iv[3];						/* Initial start value */
		double nv[2];						/* 2D New value */
		double tp[3];						/* Resultint value */
		double ne;							/* New error */
		int notrials = NO_TRIALS;

		for (i = 0; i < nmpts; i++) {		/* Move all the points */
			double bnv[2];					/* Best 2d value */
			double brv;						/* Best return value */
			int trial;
			double mv;

			opts.pass = 0;		/* Itteration pass */
			opts.ix = i;		/* Point to optimise */
			opts.p = &smp[i];

//printf("~1 point %d, sv %f %f %f\n",i,smp[i].sv[0],smp[i].sv[1],smp[i].sv[2]);

			/* Convert our start value from 3D to 2D for speed. */
			icmMul3By3x4(iv, smp[i].m2d, smp[i].aodv);

			nv[0] = iv[0] = iv[1];
			nv[1] = iv[1] = iv[2];
//printf("~1 point %d, iv %f %f %f, 2D %f %f\n",i, smp[i].aodv[0], smp[i].aodv[1], smp[i].aodv[2], iv[0], iv[1]);

			/* Do several trials from different starting points to avoid */
			/* any local minima, particularly with nearest mapping. */
			brv = 1e38;
			for (trial = 0; trial < notrials; trial++) {
				double rv;			/* Temporary */

				/* Optimise the point */
				if (powell(&rv, 2, nv, s, 0.01, 1000, optfunc2, (void *)(&opts), NULL, NULL) == 0
				    && rv < brv) {
					brv = rv;
//printf("~1 point %d, trial %d, new best %f at xy %f %f\n",i,trial,sqrt(rv), nv[0],nv[1]);
					bnv[0] = nv[0];
					bnv[1] = nv[1];
				}
//else printf("~1 powell failed with rv = %f\n",rv);
				/* Adjust the starting point with a random offset to avoid local minima */
				nv[0] = iv[0] + d_rand(-20.0, 20.0);
				nv[1] = iv[1] + d_rand(-20.0, 20.0);
			}
			if (brv == 1e38) {		/* We failed to get a result */
				fprintf(stderr, "multiple powells failed to get a result (2)\n");
#ifdef DEBUG_POWELL_FAILS
				/* Optimise the point with debug on */
				opts.debug = 1;
				icmMul3By3x4(iv, smp[i].m2d, smp[i].dv);
				nv[0] = iv[0] = iv[1];
				nv[1] = iv[1] = iv[2];
				powell(NULL, 2, nv, s, 0.01, 1000, optfunc2, (void *)(&opts), NULL, NULL);
#endif
				if (si_gam != sc_gam)
					sci_gam->del(sci_gam);
				if (di_gam != sci_gam && di_gam != sci_gam)
					di_gam->del(di_gam);
				free_nearsmth(smp, nmpts);
				*npp = 0;
				return NULL;
			}
		
			/* Convert best result 3D -> 2D */
			tp[2] = bnv[1];
			tp[1] = bnv[0];
			tp[0] = 50.0;
			icmMul3By3x4(tp, smp[i].m3d, tp);

			/* Remap it to the destinaton gamut surface */
			smp[i].dgam->radial(smp[i].dgam, tp, tp);

			icmCpy3(smp[i].nrdv, tp);		/* Non smoothed result */
			icmCpy3(smp[i].anv, tp);		/* Starting point for smoothing */
			icmCpy3(smp[i].dv, tp);			/* Default current solution */
			smp[i].dr = icmNorm33(smp[i].dv, smp[i].dgam->cent);
//printf("~1 %d: dv %f %f %f\n", i, smp[i].dv[0], smp[i].dv[1], smp[i].dv[2]);

		}
		if (verb) {
			printf("."); fflush(stdout);
		}
	}

#ifdef DIAG_POINTS
	/* Show just the closest vectors etc. */
	for (i = 0; i < nmpts; i++) {		/* Move all the points */
//		icmCpy3(smp[i].dv, smp[i].drv);			/* Radial */
		icmCpy3(smp[i].dv, smp[i].aodv);		/* Nearest */
//		icmCpy3(smp[i].dv, smp[i].nrdv);		/* No smoothed weighted */
//		icmCpy3(smp[i].dv, smp[i].dv);			/* pre-filter smooothed */
		smp[i].dr = icmNorm33(smp[i].dv, smp[i].dgam->cent);
	}
#else
	/* The smoothed direction and raw depth is a single pass, */
	/* but we use multiple passes to determine the extra depth that */
	/* needs to be added so that the smoothed result lies within */
	/* the destination gamut. */

#if VECADJPASSES > 0 || RSPLPASSES > 0
	/* We will need inward pointing correction vectors */
	{
		gamut *shgam;		/* Shrunken di_gam */
		double cusps[6][3];
		double wp[3], bp[3], kp[3];
		double p[3], p2[3], rad;
		int i;

		double s[2] = { 20.0, 20.0 };		/* 2D search area */
		double iv[3];						/* Initial start value */
		double nv[2];						/* 2D New value */
		double tp[3];						/* Resultint value */
		double ne;							/* New error */
		int notrials = NO_TRIALS;

		cow *gpnts = NULL;	/* Mapping points to create 3D -> 3D mapping */
		datai il, ih;
		datao ol, oh;
		int gres[MXDI];
		double avgdev[MXDO];

		/* Create a gamut that is a shrunk version of the destination */

		if ((shgam = new_gamut(di_gam->getsres(di_gam), di_gam->getisjab(di_gam),
		                                           di_gam->getisrast(di_gam))) == NULL) {
			fprintf(stderr, "new_gamut failed\n");
			if (si_gam != sc_gam)
				sci_gam->del(sci_gam);
			if (di_gam != sci_gam && di_gam != sci_gam)
				di_gam->del(di_gam);
			free_nearsmth(smp, nmpts);
			*npp = 0;
			return NULL;
		}

		shgam->setnofilt(shgam);

		/* Translate all the surface nodes */
		for (i = 0;;) {
			double len;

			if ((i = di_gam->getrawvert(di_gam, p, i)) < 0)
				break;

			doshrink(&opts, p, p, SHRINK);
			shgam->expand(shgam, p);
		}
		/* Translate cusps */
		if (di_gam->getcusps(di_gam, cusps) == 0) {
			shgam->setcusps(shgam, 0, NULL);
			for (i = 0; i < 6; i++) {
				doshrink(&opts, p, cusps[i], SHRINK);
				shgam->setcusps(shgam, 1, p);
			}
			shgam->setcusps(shgam, 2, NULL);
		}
		/* Translate white and black points */
		if (di_gam->getwb(di_gam, wp, bp, kp, NULL, NULL, NULL) == 0) {
			doshrink(&opts, wp, wp, SHRINK);
			doshrink(&opts, bp, bp, SHRINK);
			doshrink(&opts, kp, kp, SHRINK);
			shgam->setwb(shgam, wp, bp, kp);
		}

		if ((gpnts = (cow *)malloc(nmpts * sizeof(cow))) == NULL) { 
			fprintf(stderr,"gamut map: Malloc of near smooth points failed\n");
			shgam->del(shgam);
			if (si_gam != sc_gam)
				sci_gam->del(sci_gam);
			if (di_gam != sci_gam && di_gam != sci_gam)
				di_gam->del(di_gam);
			free_nearsmth(smp, nmpts);
			*npp = 0;
			return NULL;
		}

		/* Now locate the closest points on the shrunken gamut */
		/* and set them up for creating a rspl */
		opts.shgam = shgam;
		for (i = 0; i < nmpts; i++) {		/* Move all the points */
			gtri *ctri = NULL;
			double tmp[3];
			double bnv[2];					/* Best 2d value */
			double brv;						/* Best return value */
			int trial;
			double mv;

			opts.pass = 0;		/* Itteration pass */
			opts.ix = i;		/* Point to optimise */
			opts.p = &smp[i];

			/* Convert our start value from 3D to 2D for speed. */
			icmMul3By3x4(iv, smp[i].m2d, smp[i].dv);
			nv[0] = iv[0] = iv[1];
			nv[1] = iv[1] = iv[2];

			/* Do several trials from different starting points to avoid */
			/* any local minima, particularly with nearest mapping. */
			brv = 1e38;
			for (trial = 0; trial < notrials; trial++) {
				double rv;			/* Temporary */

				/* Optimise the point */
				if (powell(&rv, 2, nv, s, 0.01, 1000, optfunc1a, (void *)(&opts), NULL, NULL) == 0
				    && rv < brv) {
					brv = rv;
					bnv[0] = nv[0];
					bnv[1] = nv[1];
				}
				/* Adjust the starting point with a random offset to avoid local minima */
				nv[0] = iv[0] + d_rand(-20.0, 20.0);
				nv[1] = iv[1] + d_rand(-20.0, 20.0);
			}
			if (brv == 1e38) {		/* We failed to get a result */
				fprintf(stderr, "multiple powells failed to get a result (3)\n");
#ifdef DEBUG_POWELL_FAILS
				/* Optimise the point with debug on */
				opts.debug = 1;
				icmMul3By3x4(iv, smp[i].m2d, smp[i].dv);
				nv[0] = iv[0] = iv[1];
				nv[1] = iv[1] = iv[2];
				powell(NULL, 2, nv, s, 0.01, 1000, optfunc1a, (void *)(&opts), NULL, NULL);
#endif
				shgam->del(shgam);		/* Done with this */
				if (si_gam != sc_gam)
					sci_gam->del(sci_gam);
				if (di_gam != sci_gam && di_gam != sci_gam)
					di_gam->del(di_gam);
				free_nearsmth(smp, nmpts);
				*npp = 0;
				return NULL;
			}
		
			/* Convert best result 2D -> 3D */
			tp[2] = bnv[1];
			tp[1] = bnv[0];
			tp[0] = 50.0;
			icmMul3By3x4(tp, smp[i].m3d, tp);

			/* Remap it to the destinaton gamut surface */
			shgam->radial(shgam, tp, tp);

			/* Compute mapping vector from dst to shdst */
			icmSub3(smp[i].temp, tp, smp[i].dv);


			/* In case shrunk vector is very short, add a small part */
			/* of the nearest normal.  */
			smp[i].dgam->nearest_tri(smp[i].dgam, NULL, smp[i].dv, &ctri);
			icmScale3(tmp, ctri->pe, 0.1);		/* Scale to small inwards */
			icmAdd3(smp[i].temp, smp[i].temp, tmp);

			/* evector */
			icmNormalize3(smp[i].temp, smp[i].temp, 1.0);

			/* Place it in rspl setup array */
			icmCpy3(gpnts[i].p, smp[i].dv);
			icmCpy3(gpnts[i].v, smp[i].temp);
			gpnts[i].w = 1.0;
		}

		for (j = 0; j < 3; j++) {		/* Set resolution for all axes */
//			gres[j] = (mapres+1)/2;		/* Half resolution */
			gres[j] = mapres;			/* Full resolution */
			avgdev[j] = GAMMAP_RSPLAVGDEV;
		}

		evectmap = new_rspl(RSPL_NOFLAGS, 3, 3);	/* Allocate 3D -> 3D */
		evectmap->fit_rspl_w(evectmap, GAMMAP_RSPLFLAGS, gpnts, nmpts,
		                map_il, map_ih, gres, map_ol, map_oh, 1.0, avgdev, NULL);

// ~~999
#ifdef PLOT_EVECTS		/* Create VRML of error correction vectors */
		{
			vrml *wrl = NULL;
            int doaxes = 0;
			double cc[3] = { 0.7, 0.7, 0.7 };
			double red[3]    = { 1.0, 0.0, 0.0 };
			double green[3]  = { 0.0, 1.0, 0.0 };
			double tmp[3];
			co cp;

#ifdef PLOT_AXES
            doaxes = 1;
#endif

			printf("###### gamut/nearsmth.c: writing diagnostic evects%s\n",vrml_ext());
			if ((wrl = new_vrml("evects", doaxes, vrml_lab)) == NULL)
				error("new_vrml failed for '%s%s'","evects",vrml_ext());
			wrl->make_gamut_surface_2(wrl, di_gam, 0.6, 0, cc);
			cc[0] = -1.0;
			wrl->make_gamut_surface(wrl, shgam, 0.2, cc);

			/* Start of guide vector plot */
			wrl->start_line_set(wrl, 0);

			for (i = 0; i < nmpts; i++) {
				wrl->add_col_vertex(wrl, 0, smp[i].dv, red);
#ifdef NEVER	/* Plot created vectors */
				icmScale3(tmp, smp[i].temp, 4.0);
				icmSub3(tmp, smp[i].dv, tmp);
#else
				/* Plot interpolated vectors */
				icmCpy3(cp.p, smp[i].dv);
				evectmap->interp(evectmap, &cp);
				icmScale3(tmp, cp.v, 4.0);
				icmSub3(tmp, smp[i].dv, tmp);
#endif
				wrl->add_col_vertex(wrl, 0, tmp, green);
			}
			wrl->make_lines(wrl, 0, 2);			/* Guide vectors */
			wrl->del(wrl);      /* Write and delete */
		}
#endif	/* PLOT_EVECTS */
		shgam->del(shgam);		/* Done with this */
		free(gpnts);
	}
#endif /* VECADJPASSES > 0 || RSPLPASSES > 0 */

#ifdef VECSMOOTHING
	VA(("Smoothing guide vectors:\n"));

	/* Compute the neighbourhood smoothed anv[] from dv[] */
	for (i = 0; i < nmpts; i++) {
		double anv[3];			/* new anv[] */
		double tmp[3];
	
		/* Compute filtered value */
		anv[0] = anv[1] = anv[2] = 0.0;
		for (j = 0; j < smp[i].nnd; j++) {
			nearsmth *np = smp[i].nd[j].n;		/* Pointer to neighbor */
			double nw = smp[i].nd[j].w;			/* Weight */
			double tmp[3];
	
			icmSub3(tmp, smp[i].sv, np->sv);	/* Vector from neighbour src to src */
			icmAdd3(tmp, tmp, np->dv);			/* Neigbour dst + vector */

			icmScale3(tmp, tmp, nw);			/* weight for filter */
			icmAdd3(anv, anv, tmp);				/* sum filtered value */
		}

		/* Blend to un-smoothed value on neutral axis */
		icmBlend3(anv, smp[i].dv, anv, smp[i].naxbf);

		icmCpy3(smp[i].dv, anv);
		icmCpy3(smp[i].anv, anv);
		smp[i].rext = 0.0;				/* No correction */
	}

#if VECADJPASSES > 0
	/* Fine tune vectors to compensate for side effects of vector smoothing */

	VA(("Fine tuning out of gamut guide vectors:\n"));

	/* Loopkup correction vectors */
	VA(("Computing fine tuning direction:\n")); 
	for (i = 0; i < nmpts; i++) {
		co cp;
		double nd, id, tmp[3];

		icmCpy3(cp.p, smp[i].dv);
		evectmap->interp(evectmap, &cp);
		icmNormalize3(smp[i].evect, cp.v, 1.0);

		/* ~~99 ?? should we deal with white & black direction here ?? */

		/* Use closest as a default */
		smp[i].dgam->nearest(smp[i].dgam, smp[i].tdst, smp[i].dv);
		nd = icmNorm33(smp[i].tdst, smp[i].dv);		/* Dist to nearest */

		/* Compute intersection with dest gamut as tdst */
		if (!vintersect2(smp[i].dgam, NULL, tmp, smp[i].evect, smp[i].dv)) {
			/* Got an intersection */
			id = icmNorm33(tmp, smp[i].dv);		/* Dist to intersection */
			if (id <= (nd + 5.0)) 				/* And it seems sane */
				icmCpy3(smp[i].tdst, tmp);
		}

		smp[i].rext = 0.0;
	}

	VA(("Fine tuning guide vectors:\n"));
	for (it = 0; it < VECADJPASSES; it++) {
		double avgog = 0.0, maxog = 0.0, nog = 0.0;
		double avgig = 0.0, maxig = 0.0, nig = 0.0;

		/* Filter the level of out/in gamut, and apply correction vector */
		for (i = 0; i < nmpts; i++) {
			double cvec[3], clen;
			double minext = 1e80;
			double maxext = -1e80;		/* Max weighted depth extension */
			double dext, gain;
		
			minext = -20.0;

			/* Compute filtered value */
			for (j = 0; j < smp[i].nnd; j++) {
				nearsmth *np = smp[i].nd[j].n;		/* Pointer to neighbor */
				double nw = smp[i].nd[j].rw;		/* Weight */
				double tmpl;

				icmSub3(cvec, np->tdst, np->anv);		/* Vector needed to target for neighbour */
				clen = icmDot3(smp[i].evect, cvec);		/* Error in this direction */

				tmpl = nw * (clen - minext);	/* Track maximum weighted extra depth */
				if (tmpl < 0.0)
					tmpl = 0.0;
				if (tmpl > maxext)
					maxext = tmpl;
			}
			maxext += minext;

			if (it == 0)
				gain = 1.2;
			else
				gain = 0.8;

			/* Accumulate correction with damping */
			smp[i].rext += gain * maxext;

			/* Error for just this point */
			icmSub3(cvec, smp[i].tdst, smp[i].anv);
			clen = icmDot3(smp[i].evect, cvec);

			/* Blend to individual correction on neutral axis */
			dext = smp[i].naxbf * smp[i].rext + (1.0 - smp[i].naxbf) * clen;

			/* Apply integrated correction */
			icmScale3(cvec, smp[i].evect, dext);
			icmAdd3(smp[i].anv, smp[i].dv, cvec);

			if (clen > 0.0) {		/* Compression */
				if (clen > maxog)
					maxog = clen;
				avgog += clen;
				nog++;

			} else {						/* Expansion */
				if (-clen > maxig)
					maxig = -clen;
				avgig += -clen;
				nig++;
			}
		}
		if (verb)
			printf("No og %4.0f max %f avg %f, No ig %4.0f max %f avg %f\n",
			       nog,maxog,nog > 1 ? avgog/nog : 0.0, nig,maxig,nig > 1 ? avgig/nig : 0.0);
	}

	/* Copy final results */
	for (i = 0; i < nmpts; i++) {
		icmCpy3(smp[i].dv, smp[i].anv);
		smp[i].dr = icmNorm33(smp[i].dv, smp[i].dgam->cent);
	}

	if (verb) {
		double avgog = 0.0, maxog = 0.0, nog = 0.0;
		double avgig = 0.0, maxig = 0.0, nig = 0.0;

		/* Check the result */
		for (i = 0; i < nmpts; i++) {
			double cvec[3], clen;

			/* Error for just this point, for stats */
			icmSub3(cvec, smp[i].tdst, smp[i].anv);
			clen = icmDot3(smp[i].evect, cvec);

			if (clen > 0.0) {		/* Compression */
				if (clen > maxog)
					maxog = clen;
				avgog += clen;
				nog++;

			} else {						/* Expansion */
				if (-clen > maxig)
					maxig = -clen;
				avgig += -clen;
				nig++;
			}
		}
		printf("No og %4.0f max %f avg %f, No ig %4.0f max %f avg %f\n",
			       nog,maxog,nog > 1 ? avgog/nog : 0.0, nig,maxig,nig > 1 ? avgig/nig : 0.0);
	}
#endif /* VECADJUST */
#endif /* VECADJPASSES > 0 */

#if RSPLPASSES > 0
	/* We need to adjust the vectors with extra depth to compensate for */
	/* for the effect of rspl smoothing. */
	{
		cow *gpnts = NULL;	/* Mapping points to create 3D -> 3D mapping */
		rspl *map = NULL;	/* Test map */
		datai il, ih;
		datao ol, oh;
		int gres[MXDI];
		double avgdev[MXDO];
		double icgain, ixgain;		/* Initial compression, expansion gain */
		double fcgain, fxgain;		/* Final compression, expansion gain */

		VA(("Fine tuning vectors to allow for rspl smoothing:\n"));

		for (j = 0; j < 3; j++) {	/* Copy ranges */
			il[j] = map_il[j];
			ih[j] = map_ih[j];
			ol[j] = map_ol[j];
			oh[j] = map_oh[j];
		}

		/* Adjust the input ranges for guide vectors */
		for (i = 0; i < nmpts; i++) {
			for (j = 0; j < 3; j++) {
				if (smp[i]._sv[j] < il[j])
					il[j] = smp[i]._sv[j];
				if (smp[i]._sv[j] > ih[j])
					ih[j] = smp[i]._sv[j];
			}
		}

		/* Now expand the bounding box by aprox 5% margin, but scale grid res */
		/* to match, so that the natural or given boundary still lies on the grid. */
		/* (This duplicates code in gammap applied after near_smooth() returns) */
		/* (We are assuming that our changes to the giude vectprs won't expand the ranges) */
		{
			int xmapres;
			double scale;

			xmapres = (int) ((mapres-1) * 0.05 + 0.5);
			if (xmapres < 1)
				xmapres = 1;

			scale = (double)(mapres-1 + xmapres)/(double)(mapres-1);

			for (j = 0; j < 3; j++) {
				double low, high;
				high = ih[j];
				low = il[j];
				ih[j] = (scale * (high - low)) + low;
				il[j] = (scale * (low - high)) + high;
			}

			mapres += 2 * xmapres;
		}

		if ((gpnts = (cow *)malloc(nmpts * sizeof(cow))) == NULL) { 
			fprintf(stderr,"gamut map: Malloc of near smooth points failed\n");
			if (evectmap != NULL)
				evectmap->del(evectmap);
			if (si_gam != sc_gam)
				sci_gam->del(sci_gam);
			if (di_gam != sci_gam && di_gam != sci_gam)
				di_gam->del(di_gam);
			free_nearsmth(smp, nmpts);
			*npp = 0;
			return NULL;
		}

		/* Loopkup correction vectors */
		VA(("Computing fine tuning direction for vectors:\n")); 
		for (i = 0; i < nmpts; i++) {
			co cp;
			double nd, id, tmp[3];

			icmCpy3(cp.p, smp[i].dv);
			evectmap->interp(evectmap, &cp);
			icmNormalize3(smp[i].evect, cp.v, 1.0);

			/* ~~99 ?? should we deal with white & black direction here ?? */

			/* Use closest as a default */
			smp[i].dgam->nearest(smp[i].dgam, smp[i].tdst, smp[i].dv);
			nd = icmNorm33(smp[i].tdst, smp[i].dv);		/* Dist to nearest */

			/* Compute intersection with dest gamut as tdst */
			if (!vintersect2(smp[i].dgam, NULL, tmp, smp[i].evect, smp[i].dv)) {
				/* Got an intersection */
				id = icmNorm33(tmp, smp[i].dv);		/* Dist to intersection */
				if (id <= (nd + 5.0)) 				/* And it seems sane */
					icmCpy3(smp[i].tdst, tmp);
			}

			smp[i].coff[0] = smp[i].coff[1] = smp[i].coff[2] = 0.0;
			smp[i].rext = 0.0;
		}

		/* We know initially that dv == anv */
		/* Each pass computes a rext for each point, then */
		/* anv[] = dv[] + smooth(rext * evect[])  to try and avoid clipping */
		VA(("Fine tune guide vectors for rspl:\n"));
		for (it = 0; it < RSPLPASSES; it++) {
			double tmp[3];
			double avgog = 0.0, maxog = 0.0, nog = 0.0;
			double avgig = 0.0, maxig = 0.0, nig = 0.0;
			double avgrext = 0.0;
			double ovlen;

			/* Setup the rspl guide points for creating rspl */
			for (i = 0; i < nmpts; i++) {
				icmCpy3(gpnts[i].p, smp[i]._sv);	/* The orgininal src point */
				icmCpy3(gpnts[i].v, smp[i].anv);	/* current dst from previous results */
				gpnts[i].w = 1.0;
			}

			for (j = 0; j < 3; j++) {		/* Set resolution for all axes */
//				gres[j] = (mapres+1)/2;		/* Half resolution for speed */
				gres[j] = mapres;			/* Full resolution */
				avgdev[j] = GAMMAP_RSPLAVGDEV;
			}
			map = new_rspl(RSPL_NOFLAGS, 3, 3);	/* Allocate 3D -> 3D */
			map->fit_rspl_w(map, GAMMAP_RSPLFLAGS, gpnts, nmpts,
			                il, ih, gres, ol, oh, mapsmooth, avgdev, NULL);

			/* See what the source actually maps to via rspl, and how far from */
			/* the target point they are. */
			for (i = 0; i < nmpts; i++) {
				co cp;
				double cvec[3];
			
				/* Lookup rspl smoothed destination value */
				icmCpy3(cp.p, smp[i]._sv);
				map->interp(map, &cp);
				icmCpy3(smp[i].temp, cp.v);

				/* Compute the correction needed and it's signed length */
				icmSub3(cvec, smp[i].tdst, smp[i].temp);
				smp[i].clen = icmDot3(smp[i].evect, cvec);
			}

			/* Compute local correction */
			for (i = 0; i < nmpts; i++) {
				double minext = 1e80;
				double maxext = -1e80;		/* Max weighted depth extension */
				double clen;
				double tpoint[3], cvect[3];
				double tt;
				double cgain, xgain;		/* This itters compression, expansion gain */
				double gain;				/* Gain used */
			
				/* See what the worst case is in the local area, and */
				/* aim to lower the whole local area by enough to */
				/* cause the max to be 0.0 (just on the gamut) */ 

				/* Compute local depth value */
				minext = -20.0;				/* Base to measure max from */
				for (j = 0; j < smp[i].nnd; j++) {
					nearsmth *np = smp[i].nd[j].n;		/* Pointer to neighbor */
					double nw = smp[i].nd[j].rw;		/* Weight */
					double tmpl;
	
					tmpl = nw * (np->clen - minext);	/* Track maximum weighted extra depth */
					if (tmpl < 0.0)
						tmpl = 0.0;
					if (tmpl > maxext)
						maxext = tmpl;
				}
				maxext += minext;

				/* maxext is the current effective error at this point with rext aim point */
				if (it == 0) {	/* Set target on first itteration */
					if (smp[i].rext <= 0.0)	/* Expand direction */
						smp[i].rext += maxext;
					else
						smp[i].rext += RSPLSCALE * maxext;
				}
				avgrext += smp[i].rext;

				/* Compute offset target point at maxlen from tdst in evect dir. */
				icmScale3(tpoint, smp[i].evect, smp[i].rext);
				icmAdd3(tpoint, tpoint, smp[i].tdst);

				/* Expansion/compression gain program */
				icgain = 1.4;		/* Initial itteration compression gain */
				ixgain = smp[i].wt.f.x * icgain;	/* Initial itteration expansion gain */
				fcgain = 0.5 * icgain;		/* Final itteration compression gain */
				fxgain = 0.5 * ixgain;		/* Final itteration expansion gain */

				/* Set the gain */
				tt = it/(RSPLPASSES - 1.0);
				cgain = (1.0 - tt) * icgain + tt * fcgain;
				xgain = (1.0 - tt) * ixgain + tt * fxgain;
				if (it != 0)		/* Expand only on first itter */
					xgain = 0.0;

//if (i == 0) printf("~1 i %d, it %d, wt.f.x = %f, cgain %f, xgain %f\n",i,it,smp[i].wt.f.x, cgain, xgain);
				if (smp[i].rext > 0.0) /* Compress direction */
					gain = cgain;
				else
					gain = xgain;

				/* Keep stats of this point */
				clen = smp[i].clen;
				if (clen > 0.0) {
					if (clen > maxog)
						maxog = clen;
					avgog += clen;
					nog++;

				} else {		/* Expand */
					if (-clen > maxig)
						maxig = -clen;
					avgig += -clen;
					nig++;
				}

				/* Compute needed correction from current rspl smoothed anv */
				/* to offset target point. */
				icmSub3(cvect, tpoint, smp[i].temp); 		/* Correction still needed */
				icmScale3(cvect, cvect, gain);				/* Times gain */
				icmAdd3(smp[i].coff, smp[i].coff, cvect);	/* Accumulated */

				icmCpy3(gpnts[i].p, smp[i].dv);
				icmCpy3(gpnts[i].v, smp[i].coff);
				gpnts[i].w = 1.0;
			}

			for (j = 0; j < 3; j++) {		/* Set resolution for all axes */
//				gres[j] = (mapres+1)/2;		/* Half resolution */
				gres[j] = mapres;			/* Full resolution */
				avgdev[j] = GAMMAP_RSPLAVGDEV;
			}
			map = new_rspl(RSPL_NOFLAGS, 3, 3);	/* Allocate 3D -> 3D */
			map->fit_rspl_w(map, GAMMAP_RSPLFLAGS, gpnts, nmpts,
			                il, ih, gres, ol, oh, 2.0, avgdev, NULL);

			/* Lookup the smoothed extension vector for each point and apply it */
			for (i = 0; i < nmpts; i++) {
				double tt;
				co cp;
			
				icmCpy3(cp.p, smp[i].dv);
				map->interp(map, &cp);
#ifdef RSPLUSEPOW
				spow3(smp[i].coff, cp.v, 1.0/2.0);		/* Filtered value is current value */
#else
				icmCpy3(smp[i].coff, cp.v);		/* Filtered value is current value */
#endif

				/* Make sure anv[] is on the destination gamut at neutral axis */
				icmScale3(cp.v, cp.v, smp[i].naxbf);

				/* Apply accumulated offset */
				icmAdd3(smp[i].anv, smp[i].dv, cp.v);
			}
			map->del(map);

			if (verb)
				printf("No og %4.0f max %f avg %f, No ig %4.0f max %f avg %f, avg rext %f\n",
				       nog,maxog,nog > 1 ? avgog/nog : 0.0, nig,maxig,nig > 1 ? avgig/nig : 0.0, avgrext/nmpts);
		}	/* Next pass */
		free(gpnts);

		/* Copy last anv to dv for result */
		for (i = 0; i < nmpts; i++) {
// ~~99
// Normal target
			icmCpy3(smp[i].dv, smp[i].anv);

// Show evect direction
//			icmCpy3(smp[i]._sv, smp[i].dv);
//			icmScale3(smp[i].evect, smp[i].evect, 4.0);
//			icmAdd3(smp[i].dv, smp[i].evect, smp[i].dv);

// Show target point destination
//			icmCpy3(smp[i].dv, smp[i].tdst);

// Show offset target destination
//			icmScale3(smp[i].dv, smp[i].evect, smp[i].rext);
//			icmAdd3(smp[i].dv, smp[i].dv, smp[i].tdst);

			smp[i].dr = icmNorm33(smp[i].dv, smp[i].dgam->cent);
		}
	}

#endif /* RSPLPASSES > 0 */
#endif /* NEVER (show debug values) */

	VA(("Smoothing passes done, doing final houskeeping\n"));

	if (verb)
		printf("\n");

#if defined(SAVE_VRMLS) && defined(PLOT_MAPPING_INFLUENCE)
	create_influence_plot(smp, nmpts);
#endif

	VB(("Final guide points:\n"));

	/* Restore the actual non cusp rotated source point */
	for (i = 0; i < nmpts; i++) {

		VB(("Src %d = %f %f %f\n",i,smp[i].sv[0],smp[i].sv[1],smp[i].sv[2]));
		VB(("Dst %d = %f %f %f\n",i,smp[i].dv[0],smp[i].dv[1],smp[i].dv[2]));

		/* Save the cusp mapped source value */
		icmCpy3(smp[i].csv, smp[i].sv);

		/* Finally un cusp map the source point */
//		inv_comp_ce(&opts, smp[i].sv, smp[i].sv, &smp[i].wt);
//		smp[i].sr = icmNorm33(smp[i].sv, smp[i].sgam->cent);
		icmCpy3(smp[i].sv, smp[i]._sv);
		smp[i].sr = smp[i]._sr;
	}

	VB(("Creating sub-surface guide points:\n"));

	/* Create sub-surface points. */
	for (i = 0; i < nmpts; i++) {

		/* Create sub-surface mapping points too. We control the degree */
		/* of knee with a extrapolated destination point dv2, where */
		/* the degree of extrapolation is inversly related to the sharpness of the knee. */
		/* A third point maps 1:1 with a weight that is related the sharpness. */
		/* Note that not every mapping point has a sub-surface point, */
		/* and that the gflag and vflag will be nz if it does. */
		/* We're assuming here that the dv is close to being on the */
		/* destination gamut, so that the vector_isect param will be */
		/* close to 1.0 at the intended destination gamut. */
		{
			double mv[3], ml, nv[3];		/* Mapping vector & length, noralized mv */ 
			double minv[3], maxv[3];		/* (Not used) */
			double mint, maxt;
			gtri *mintri, *maxtri;

			smp[i].vflag = smp[i].gflag = 0;	/* Default unknown */
			smp[i].w2 = 0.0;
			icmSub3(mv, smp[i].dv, smp[i].sv);	/* Mapping vector */
			ml = icmNorm3(mv);		/* It's length */
			if (ml > 0.1) {			/* If mapping is non trivial */

//#define PFCOND i == 802

//if (PFCOND) printf("~1 mapping %d = %f %f %f -> %f %f %f\n", i, smp[i].sv[0],smp[i].sv[1],smp[i].sv[2],smp[i].dv[0],smp[i].dv[1],smp[i].dv[2]);
//if (PFCOND) printf("~1 vector %f %f %f, len %f\n",  mv[0], mv[1], mv[2],ml);

				/* Compute actual depth of ray into destination gamut */
				/* to determine if this is expansion or contraction. */
				if (di_gam->vector_isect(di_gam, smp[i].sv, smp[i].dv,
				                    minv, maxv, &mint, &maxt, &mintri, &maxtri) != 0) {
					double wp[3], bp[3];		/* Gamut white and black points */
					double p1, napoint[3] = { 50.0, 0.0, 0.0 };		/* Neutral axis point */
					double natarg[3];		/* Neutral axis sub target */
					double adepth1, adepth2 = 1000.0;	/* Directional depth, radial depth */
					double adepth;			/* Minimum available depth */
					double mv2[3], sml;		/* Sub-surface mapping vector & norm. length */

					/* Locate the point on the neutral axis that is closest to */
					/* the guide ray. We use this as a destination direction */
					/* if the sub surface ray gets very long, and to compute */
					/* a sanity check on the available depth. */
					if (d_gam->getwb(d_gam, NULL, NULL, NULL, wp, dst_kbp ? NULL : bp, dst_kbp ? bp : NULL) == 0) {
						if (icmLineLineClosest(napoint, NULL, &p1, NULL, bp, wp,
						                       smp[i].sv, smp[i].dv) == 0) {
							double nalev[3];
							icmCpy3(nalev, napoint);

//if (PFCOND) printf("~1 neutral axis point = %f %f %f\n", napoint[0], napoint[1], napoint[2]);
							/* Compute a normalized available depth from distance */
							/* to closest to neautral axis point */
							if ((mint > 1e-8 && maxt > -1e-8)		/* G. & V. Compression */
							 || ((mint < -1e-8 && maxt > -1e-8)		/* G. Exp & V. comp. */
							  && (fabs(mint) < (fabs(maxt) - 1e-8)))) {
								/* Compression */

								/* Moderate the neutral axis point to be half way */
								/* between sv->dv direction, and horizontal. */
								nalev[0] = smp[i].dv[0];
								icmBlend3(napoint, napoint, nalev, 0.5);
								/* Clip it to be between black and white point */
								if (napoint[0] < bp[0])
									icmCpy3(napoint, bp);
								else if (napoint[0] > wp[0])
									icmCpy3(napoint, wp);
								adepth2 = icmNorm33(napoint, smp[i].dv);
							} else {
								/* Expansion */
								/* Moderate the neutral axis point to be half way */
								/* between sv->dv direction, and horizontal. */
								nalev[0] = smp[i].sv[0];
								icmBlend3(napoint, napoint, nalev, 0.5);
								/* Clip it to be between black and white point */
								if (napoint[0] < bp[0])
									icmCpy3(napoint, bp);
								else if (napoint[0] > wp[0])
									icmCpy3(napoint, wp);
								adepth2 = icmNorm33(napoint, smp[i].sv);
							}
						}
#ifdef VERB
						  else {
							printf("icmLineLineClosest failed\n");
						}
#endif
					}
#ifdef VERB
					  else {
						printf("d_gam->getwb failed\n");
					}
#endif

//printf("\n~1 i %d: %f %f %f -> %f %f %f\n   isect at t %f and %f\n", i, smp[i].sv[0], smp[i].sv[1], smp[i].sv[2], smp[i].dv[0], smp[i].dv[1], smp[i].dv[2], mint, maxt);

					/* Only create sub-surface mapping vectors if it makes sense. */
					/* If mapping vector is pointing away from destination gamut, */
					/* (which shouldn't happen), ignore it. If the directional depth */
					/* is very thin compared to the radial depth, indicating that we're */
					/* near a "lip", ignore it. */
					if (mint >= -1e-8 && maxt > 1e-8) {

						/* Gamut compression and vector compression */
						if (fabs(mint - 1.0) < fabs(maxt) - 1.0
						 && smp[i].dgam->radial(smp[i].dgam, NULL, smp[i].dv)
						  < smp[i].sgam->radial(smp[i].sgam, NULL, smp[i].dv)) {
							double sgamcknf = gamcknf * 0.6;	/* [0.7] Scale to limit overshoot */

//if (PFCOND) printf("~1 point is gamut comp & vect comp.\n");
//if (PFCOND) printf("~1 point is gamut comp & vect comp. mint %f maxt %f\n",mint,maxt);
							adepth1 = ml * 0.5 * (maxt + mint - 2.0);	/* Average depth */
#ifdef CYLIN_SUBVEC
							adepth = adepth2;		/* Always cylindrical depth */
#else
							adepth = adepth1 < adepth2 ? adepth1 : adepth2;		/* Smaller of the two */
#endif
							if (adepth1 < (0.5 * adepth2))
								continue;

//if (PFCOND) printf("~1 dir adepth %f, radial adapeth %f\n",adepth1,adepth2);
							adepth *= 0.9;			/* Can't use 100% */
							smp[i].gflag = 1;		/* Gamut compression and */
							smp[i].vflag = 1;		/* vector compression */

							/* Compute available depth and knee factor adjusted sub-vector */
							icmCpy3(smp[i].sv2, smp[i].dv);		/* Sub source is guide dest */
							ml *= (1.0 - sgamcknf);				/* Scale by knee */
							adepth *= (1.0 - sgamcknf);
							sml = ml < adepth ? ml : adepth;	/* Smaller of two */
//if (PFCOND) printf("~1 adjusted subvec len %f\n",sml);
							icmNormalize3(mv2, mv, sml);		/* Full sub-surf disp. == no knee */
							icmAdd3(mv2, smp[i].sv2, mv2);		/* Knee adjusted destination */
	
//if (PFCOND) printf("~1 before blend sv2 %f %f %f, dv2 %f %f %f\n", smp[i].sv2[0], smp[i].sv2[1], smp[i].sv2[2], mv2[0], mv2[1], mv2[2]);
							/* Compute point at sml depth from sv2 towards napoint */
							icmSub3(natarg, napoint, smp[i].sv2);
							icmNormalize3(natarg, natarg, sml);		/* Sub vector towards n.axis */
							icmAdd3(natarg, natarg, smp[i].sv2);	/* n.axis target */
#ifdef CYLIN_SUBVEC
							icmCpy3(mv2, natarg);			/* cylindrical direction vector */
#else
							/* Blend towards n.axis as length of sub vector approaches */
							/* distance to neutral axis. */
							icmBlend3(mv2, mv2, natarg, sml/adepth2);
#endif /* CYLIN_SUBVEC */
//if (PFCOND) printf("~1 after blend sv2 %f %f %f, dv2 %f %f %f\n", smp[i].sv2[0], smp[i].sv2[1], smp[i].sv2[2], mv2[0], mv2[1], mv2[2]);
							
							icmCpy3(smp[i].dv2, mv2);				/* Destination */
							icmCpy3(smp[i].temp, smp[i].dv2);		/* Save a copy to temp */
							smp[i].w2 = 0.7;						/* De-weight due to density */

							icmBlend3(mv2, mv2, napoint, 0.6);		/* Half way to na */
							icmCpy3(smp[i].sd3, mv2);

							smp[i].w3 = 0.4 * gamcknf;		/* [0.3] Weight with knee factor */
															/* and to control overshoot */

						} else {
//if (PFCOND) printf("~1 point is gamut exp & vect exp. mint %f maxt %f\n",mint,maxt);
							smp[i].gflag = 2;		/* Gamut expansion and */
							smp[i].vflag = 0;		/* vector expansion, */
													/* but crossing over, so no sub vect. */
//if (PFCOND) printf("~1 point is crossover point\n",mint,maxt);
						}
	
					/* Gamut expansion and vector expansion */
					} else if (mint < -1e-8 && maxt > 1e-8) {

//if (PFCOND) printf("~1 point is gamut exp & vect exp. mint %f maxt %f\n",mint,maxt);
						/* This expand/expand case has reversed src/dst sense to above */
						adepth1 = ml * 0.5 * -mint;
#ifdef CYLIN_SUBVEC
						adepth = adepth2;		/* Always cylindrical depth */
#else
						adepth = adepth1 < adepth2 ? adepth1 : adepth2;
#endif
//if (PFCOND) printf("~1 dir adepth %f, radial adapeth %f\n",adepth1,adepth2);
						adepth *= 0.9;				/* Can't use 100% */

						if (adepth1 < (0.6 * adepth2))
							continue;

						smp[i].gflag = 2;		/* Gamut expansion */
						smp[i].vflag = 2;		/* vector is expanding */

						icmCpy3(smp[i].dv2, smp[i].sv);	/* Sub dest is guide src */
						ml *= (1.0 - gamxknf);			/* Scale by knee */
						adepth *= (1.0 - gamxknf);
						sml = ml < adepth ? ml : adepth;/* Smaller of two */
						icmNormalize3(mv2, mv, sml);	/* Full sub-surf disp. == no knee */
						icmSub3(mv2, smp[i].dv2, mv2);	/* Knee adjusted source */
	
						/* Blend towards n.axis as length of sub vector approaches */
						/* distance to neutral axis. */
						icmSub3(natarg, smp[i].dv2, napoint);
						icmNormalize3(natarg, natarg, sml);	/* Sub vector away n.axis */
						icmSub3(natarg, smp[i].dv2, natarg);/* n.axis oriented source */
#ifdef CYLIN_SUBVEC
						icmCpy3(mv2, natarg);				/* cylindrical direction vector */
#else
						icmBlend3(mv2, mv2, natarg, sml/adepth2);	/* dir adjusted src */
#endif /* CYLIN_SUBVEC */

						icmCpy3(smp[i].sv2, mv2); 			/* Source */
						icmCpy3(smp[i].temp, smp[i].dv2);	/* Save a copy to temp */
						smp[i].w2 = 0.8;

						icmBlend3(mv2, mv2, napoint, 0.5);		/* Half way to na */
						icmCpy3(smp[i].sd3, mv2);
						smp[i].w3 = 0.3 * gamcknf;			/* Weight with knee fact */

					/* Conflicted case */
					} else {
						/* Nonsense vector */
						smp[i].gflag = 0;		/* Gamut compression but */
						smp[i].vflag = 0;		/* vector is expanding */
//if (PFCOND) printf("~1 point is nonsense vector mint %f maxt %f\n",mint,maxt);

						icmCpy3(smp[i].dv, smp[i].aodv);	/* Clip to the destination gamut */
					}
				}
			}
		}

#ifdef NEVER	// Diagnostic
		smp[i].vflag = 0;	/* Disable sub-points */
#endif /* NEVER */

		VB(("Out Src %d = %f %f %f\n",i,smp[i].sv[0],smp[i].sv[1],smp[i].sv[2]));
		VB(("Out Dst %d = %f %f %f\n",i,smp[i].dv[0],smp[i].dv[1],smp[i].dv[2]));
		if (smp[i].vflag != 0) {
			VB(("Out Src2 %d = %f %f %f\n",i,smp[i].sv2[0],smp[i].sv2[1],smp[i].sv2[2]));
			VB(("Out Dst2 %d = %f %f %f\n",i,smp[i].dv2[0],smp[i].dv2[1],smp[i].dv2[2]));
		}
	}

#ifdef SUBVEC_SMOOTHING
	VB(("Smoothing sub-surface guide points:\n"));

	/* Smooth the sub-surface mapping points */
	/* dv2[] is duplicated in temp[], so use temp[] as the values to be filtered */
	for (i = 0; i < nmpts; i++) {
		double tmp[3];
		double fdv2[3];			/* Filtered dv2[] */
		double tw;				/* Total weight */
		int rc;

		if (smp[i].vflag == 0)
			continue;

		/* Compute filtered value */
		tw = fdv2[0] = fdv2[1] = fdv2[2] = 0.0;
		for (j = 0; j < smp[i].nnd; j++) {
			nearsmth *np = smp[i].nd[j].n;		/* Pointer to neighbor */
			double nw = smp[i].nd[j].w;			/* Weight */
			double tmp[3];
	
			if (np->vflag) {
				icmSub3(tmp, smp[i].sv2, np->sv2);	/* Vector from neighbour src to src */
				icmAdd3(tmp, tmp, np->dv2);			/* Neigbour dst + vector */
	
				icmScale3(tmp, tmp, nw);			/* weight for filter */
				icmAdd3(fdv2, fdv2, tmp);			/* sum filtered value */
				tw += nw;
			}
		}

		if (tw > 0.0) {
//printf("~1 %d: moved %f %f %f -> %f %f %f de %f\n", i, smp[i].dv2[0], smp[i].dv2[1], smp[i].dv2[2], fdv2[0], fdv2[1], fdv2[2], icmNorm33(smp[i].dv2,fdv2));
			icmScale3(smp[i].dv2, fdv2, 1.0/tw);
		}
	}
#endif /* SUBVEC_SMOOTHING */

	VB(("near_smooth is done\n"));

	if (evectmap != NULL)
		evectmap->del(evectmap);
#ifndef PLOT_DIGAM
	if (si_gam != sc_gam)
		sci_gam->del(sci_gam);
	if (di_gam != sci_gam && di_gam != sci_gam)
		di_gam->del(di_gam);
	for (i = 0; i < nmpts; i++) {
		smp[i].sgam = NULL;
		smp[i].dgam = NULL;
	}
#else /* !PLOT_DIGAM */
	warning("!!!!! PLOT_DIGAM defined !!!!!");
#endif /* !PLOT_DIGAM */
	*npp = nmpts;
	return smp;
}

/* Free the list of points that was returned */
void free_nearsmth(nearsmth *smp, int nmpts) {
	int i;

	for (i = 0; i < nmpts; i++) {
		if (smp[i].nd != NULL)
			free(smp[i].nd);
	}
	free(smp);
}


/* =================================================================== */

#if defined(SAVE_VRMLS) && defined(PLOT_MAPPING_INFLUENCE)

/* Create a plot indicating how the source mapping has been guided by the */
/* various weighting forces. */
static void create_influence_plot(nearsmth *smp, int nmpts) {
	int i, j, k;
	gamut *gam;
	int src = 0;			/* 1 = src, 0 = dst gamuts */
	vrml *wrl = NULL;
	co *fpnts = NULL;		/* Mapping points to create diagnostic color mapping */
	rspl *swdiag = NULL;
	int gres[3];
	double avgdev[3];
	double cols[4][3] = { { 1.0, 0.0, 0.0 },		/* Absolute = red */
	                      { 1.0, 1.0, 0.0 },		/* Relative = yellow */
	                      { 0.0, 0.0, 1.0 },		/* Radial   = blue */
	                      { 0.0, 1.0, 0.0 } };		/* Depth    = green */
	double grey[3] = { 0.5, 0.5, 0.5 };		/* Grey */
	double max, min;
	int ix;

	if (src)
		gam = smp->sgam;
	else
		gam = smp->dgam;

	/* Setup the scattered data points */
	if ((fpnts = (co *)malloc((nmpts) * sizeof(co))) == NULL) { 
		fprintf(stderr,"gamut map: Malloc of diagnostic mapping setup points failed\n");
		return;
	}

	/* Compute error values and diagnostic color */
	/* for each guide vector */
	for (i = 0; i < nmpts; i++) {
		double dv[4], gv;
		double rgb[3];

		/* Source value location */
		if (src) {
			for (j = 0; j < 3; j++)
				fpnts[i].p[j] = smp[i]._sv[j];		/* Non cusp rotated */
		} else {		/* Dest value location */
			for (j = 0; j < 3; j++)
				fpnts[i].p[j] = smp[i].dv[j];
		}

		/* Diagnostic color */
		max = -1e60; min = 1e60;
		for (k = 0; k < 4; k++) {			/* Find max and min error value */
			dv[k] = smp[i].dbgv[k];
			if (dv[k] > max)
				max = dv[k];
			if (dv[k] < min)
				min = dv[k];
		}
		for (k = 0; k < 4; k++)				/* Scale to max */
			dv[k] /= max;
		max /= max;
		min /= max;
		max -= min;							/* reduce min to zero */
		for (k = 0; k < 4; k++)
			dv[k] /= max;
		for (gv = 1.0, k = 0; k < 4; k++)	/* Blend remainder with grey */
			gv -= dv[k];
		
		for (j = 0; j < 3; j++)				/* Compute interpolated color */
			fpnts[i].v[j] = 0.0;
		for (k = 0; k < 4; k++) {
			for (j = 0; j < 3; j++)
				fpnts[i].v[j] += dv[k] * cols[k][j];
		}
		for (j = 0; j < 3; j++)
			fpnts[i].v[j] += gv * grey[j];
	}

	/* Create the diagnostic color rspl */
	for (j = 0; j < 3; j++) {		/* Set resolution for all axes */
		gres[j] = smp->mapres;
		avgdev[j] = 0.001;
	}
	swdiag = new_rspl(RSPL_NOFLAGS, 3, 3);	/* Allocate 3D -> 3D */
	swdiag->fit_rspl(swdiag, RSPL_NOFLAGS, fpnts, nmpts, NULL, NULL, gres, NULL, NULL, 1.0, avgdev, NULL);

	/* Now create a plot of the sci_gam with the vertexes colored acording to the */
	/* diagnostic map. */
	if ((wrl = new_vrml("sci_gam_wt", 1, vrml_lab)) == NULL) {
		fprintf(stderr,"gamut map: new_vrml failed for '%s%s'\n","sci_gam_wt",vrm_ext());
		swdiag->del(swdiag);
		free(fpnts);
		return;
	}

	/* Plot the gamut triangle vertexes */
	for (ix = 0; ix >= 0;) {
		co pp;
		double col[3];

		ix = gam->getvert(gam, NULL, pp.p, ix);
		swdiag->interp(swdiag, &pp);
		icmClip3(pp.v, pp.v);
		wrl->add_col_vertex(wrl, 0, pp.p, pp.v);
	}
	gam->startnexttri(gam);
	for (;;) {
		int vix[3];
		if (gam->getnexttri(gam, vix))
			break;
		wrl->add_triangle(wrl, 0, vix);
	}
	wrl->make_triangles_vc(wrl, 0, 0.0);

	printf("Writing sci_gam_wt%s file\n",vrml_ext());
	wrl->del(wrl);		/* Write file */
	free(fpnts);
	swdiag->del(swdiag);
}
#endif


















