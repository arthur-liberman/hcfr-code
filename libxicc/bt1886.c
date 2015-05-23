
/* 
 * Author:  Graeme W. Gill
 * Date:    16/8/13
 * Version: 1.00
 *
 * Copyright 2013, 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUB LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/* BT.1886 stype input offset transfer curve, */
/* + general gamma + input + output offset curve support. */

#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#ifdef __sun
#include <unistd.h>
#endif
#if defined(__IBMC__) && defined(_M_IX86)
#include <float.h>
#endif
#include "numlib.h"
#include "icc.h"		/* definitions for this library */
#include "bt1886.h"		/* definitions for this library */

#undef DEBUG

/* BT.1886 support */
/* This is both a EOTF curve, and a white point */
/* adjustment. */

/* Compute technical gamma from effective gamma in BT.1886 style */

/* Info for optimization */
typedef struct {
	double wp;			/* 100% input target */ 
	double thyr;		/* 50% input target */
	double bp;			/* 0% input target */
} gam_fits;

/* gamma + input offset function handed to powell() */
static double gam_fit(void *dd, double *v) {
	gam_fits *gf = (gam_fits *)dd;
	double gamma = v[0];
	double a, b;
	double rv = 0.0;
	double t1, t2;

	if (gamma < 0.0) {
		rv += 100.0 * -gamma;
		gamma = 1e-4;
	}

	t1 = pow(gf->bp, 1.0/gamma);
	t2 = pow(gf->wp, 1.0/gamma);
	b = t1/(t2 - t1);						/* Offset */
	a = pow(t2 - t1, gamma);				/* Gain */

	/* Comput 50% output for this technical gamma */
	/* (All values are without output offset being added in) */
	t1 = a * pow((0.5 + b), gamma);
	t1 = t1 - gf->thyr;
	rv += t1 * t1;
	
	return rv;
}

/* Given the effective gamma and the output offset Y, */
/* return the technical gamma needed for the correct 50% response. */
static double xicc_tech_gamma(
	double egamma,			/* effective gamma needed */
	double off,				/* Output offset required */
	double outoprop			/* Prop. of offset to be accounted for on output */
) {
	gam_fits gf;
	double outo;
	double op[1], sa[1], rv;

	if (off <= 0.0) {
		return egamma;
	}

	/* We set up targets without outo being added */
	outo = off * outoprop; 		/* Offset acounted for in output */
	gf.bp = off - outo;			/* Black value for 0 % input */
	gf.wp = 1.0 - outo;			/* White value for 100% input */
	gf.thyr = pow(0.5, egamma) - outo;	/* Advetised 50% target */

	op[0] = egamma;
	sa[0] = 0.1;

	if (powell(&rv, 1, op, sa, 1e-6, 500, gam_fit, (void *)&gf, NULL, NULL) != 0)
		warning("Computing effective gamma and input offset is inaccurate");

	return op[0];
}

/* Set the bt1886_info to a default do nothing state */
void bt1886_setnop(bt1886_info *p) {
	icmXYZ2XYZ(p->w, icmD50);
	p->ingo = 0.0;
	p->outsc = 1.0;
	p->outo = 0.0;
	p->outL = 0.0;
	p->tab[1] = 0.0;
	p->tab[2] = 0.0;
}

/* Setup the bt1886_info for the given target black point, proportion of */
/* offset to be accounted for on output, and gamma. */
/* wp XYZ simply sets the L*a*b* reference */
/* Pure BT.1886 will have outopro = 0.0 and gamma = 2.4 */
void bt1886_setup(
bt1886_info *p,
icmXYZNumber *w,	/* wp used for L*a*b* conversion */
double *XYZbp,		/* normalised bp used for black offset and black point hue "bend" */
double outoprop,	/* 0..1 proportion of output black point compensation */
double gamma,		/* technical or effective gamma */
int effg			/* nz if effective gamma, z if technical gamma */
) {
	double Lab[3], ino, bkipow, wtipow;

	icmXYZ2XYZ(p->w, *w);

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886_setup wp.Y %f, bp.Y %f, outprop %f, gamma %f, effg %d", p->w.Y, XYZbp[1], outoprop, gamma, effg);
#endif

	if (effg) {
		p->gamma = xicc_tech_gamma(gamma, XYZbp[1], outoprop);
#ifdef DEBUG
		a1logd(g_log, 2, "bt1886_setup tgamma %f", p->gamma);
#endif
	} else {
		p->gamma = gamma;
	}

	icmXYZ2Lab(&p->w, Lab, XYZbp);

	p->outL   = Lab[0];		/* For bp blend comp. */
	p->tab[0] = Lab[1];		/* a* b* correction needed */
	p->tab[1] = Lab[2];
#ifdef DEBUG
	a1logd(g_log, 2, "bt1886_setup bend Lab = %f %f %f", p->outL, p->tab[0], p->tab[1]);
#endif

	if (XYZbp[1] < 0.0)
		XYZbp[1] = 0.0;


	p->outo = XYZbp[1] * outoprop; 		/* Offset acounted for in output */
	ino = XYZbp[1] - p->outo;			/* Balance of offset accounted for in input */

	bkipow = pow(ino, 1.0/p->gamma);				/* Input offset black to 1/pow */
	wtipow = pow((1.0 - p->outo), 1.0/p->gamma);	/* Input offset white to 1/pow */

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886_setup outo %f, ino %f, bkipow %f, wtipow %f", p->outo, ino, bkipow, wtipow);
#endif

	p->ingo = bkipow/(wtipow - bkipow);			/* non-linear Y that makes input offset */
												/* proportion of black point */
	p->outsc = pow(wtipow - bkipow, p->gamma);	/* Scale to make input of 1 map to */
												/* 1.0 - p->outo */

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886_setup ingo %f, outsc %f", p->ingo, p->outsc);
#endif
}

/* Apply BT.1886 eotf curve to the device RGB value */
/* to produce a linear light RGB. We pass xvYCC out of range values through. */
void bt1886_fwd_curve(bt1886_info *p, double *out, double *in) {
	int j;

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886    Dev RGB in %f %f %f, pow %f\n", in[0],in[1],in[2], p->gamma);
	a1logd(g_log, 2, "outo %f, outsc %f, pow %f\n", p->outo, p->outsc, 1.0/p->gamma);
	a1logd(g_log, 2, "ingo %f, pow %f, outsc %f, outo %f\n", p->ingo, p->gamma, p->outsc,p->outo);
#endif

	for (j = 0; j < 3; j++) {
		int neg = 0;
		double vv = in[j];
	
		if (vv < 0.0) {			/* Allow for xvYCC */
			neg = 1;
			vv = -vv;
		}
		/* Apply input offset */
		vv += p->ingo;
		
		/* Apply power and scale */
		if (vv > 0.0)
			vv = p->outsc * pow(vv, p->gamma);

		/* Apply output portion of offset */
		vv += p->outo;

		if (neg)
			vv = -vv;

		out[j] = vv;
	}

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886 linear RGB out %f %f %f\n", out[0],out[1],out[2]);
#endif
}

/* Apply inverse BT.1886 eotf curve to the linear light RGB to produce */
/* device RGB values. We pass xvYCC out of range values through. */
void bt1886_bwd_curve(bt1886_info *p, double *out, double *in) {
	int j;

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886 linear RGB in %f %f %f\n", in[0],in[1],in[2]);
	a1logd(g_log, 2, "outo %f, outsc %f, pow %f, ingo %f\n", p->outo, p->outsc, 1.0/p->gamma,p->ingo);
#endif

	for (j = 0; j < 3; j++) {
		int neg = 0;
		double vv = in[j];
	
		if (vv < 0.0) {			/* Allow for xvYCC */
			neg = 1;
			vv = -vv;
		}

		/* Un-apply output portion of offset */
		vv -= p->outo;

		/* Un-apply power and scale */
		if (vv > 0.0)
			vv = pow(vv/p->outsc, 1.0/p->gamma);

		/* Un-apply input offset */
		vv -= p->ingo;
		
		if (neg)
			vv = -vv;

		out[j] = vv;
	}

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886   Dev RGB out %f %f %f\n", in[0],in[1],in[2]);
#endif
}

/* Apply BT.1886 processing black point hue adjustment to the XYZ value */
void bt1886_wp_adjust(bt1886_info *p, double *out, double *in) {
	double vv;

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886 XYZ wp adj. in %f %f %f\n", in[0],in[1],in[2]);
#endif

	icmXYZ2Lab(&p->w, out, in);

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886 Lab wp adj. in %f %f %f\n", out[0],out[1],out[2]);
#endif

	/* Blend ab to required black point offset p->tab[] as L approaches black. */
	vv = (out[0] - p->outL)/(100.0 - p->outL);	/* 0 at bp, 1 at wp */
	vv = 1.0 - vv;
	if (vv < 0.0)
		vv = 0.0;
	else if (vv > 1.0)
		vv = 1.0;
	vv = pow(vv, 40.0);

	out[1] += vv * p->tab[0];
	out[2] += vv * p->tab[1];

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886 Lab after wp adj. %f %f %f\n", out[0],out[1],out[2]);
#endif

	icmLab2XYZ(&p->w, out, out);

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886 XYZ after wp adj. %f %f %f\n", out[0],out[1],out[2]);
#endif
}


/* Apply inverse BT.1886 processing black point hue adjustment to the XYZ value */
void bt1886_inv_wp_adjust(bt1886_info *p, double *out, double *in) {
	double vv;

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886 XYZ inv. wp adj. in %f %f %f\n", in[0],in[1],in[2]);
#endif

	icmXYZ2Lab(&p->w, out, in);

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886 Lab inv. wp adj. in %f %f %f\n", out[0],out[1],out[2]);
#endif

	/* Blend ab to required black point offset p->tab[] as L approaches black. */
	vv = (out[0] - p->outL)/(100.0 - p->outL);	/* 0 at bp, 1 at wp */
	vv = 1.0 - vv;
	if (vv < 0.0)
		vv = 0.0;
	else if (vv > 1.0)
		vv = 1.0;
	vv = pow(vv, 40.0);

	out[1] -= vv * p->tab[0];
	out[2] -= vv * p->tab[1];

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886 Lab after inv. wp adj. %f %f %f\n", out[0],out[1],out[2]);
#endif

	icmLab2XYZ(&p->w, out, out);

#ifdef DEBUG
	a1logd(g_log, 2, "bt1886 XYZ after inv. wp adj. %f %f %f\n", out[0],out[1],out[2]);
#endif
}






















