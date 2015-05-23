
/* 
 * cam97s3hk
 *
 * Color Appearance Model, based on
 * CIECAM97, "Revision for Practical Applications"
 * by Mark D. Fairchild, with the addition of the Viewing Flare
 * model described on page 487 of "Digital Color Management",
 * by Edward Giorgianni and Thomas Madden, and the
 * Helmholtz-Kohlraush effect, using the equation
 * the Bradford-Hunt 96C model as detailed in Mark Fairchilds
 * book "Color Appearance Models". 
 *
 * Author:  Graeme W. Gill
 * Date:    5/10/00
 * Version: 1.20
 *
 * Copyright 2000, 2002 Graeme W. Gill
 * Please refer to COPYRIGHT file for details.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* Note that XYZ values are normalised to 1.0 consistent */
/* with the ICC convention (not 100.0 as assumed by the CIECAM spec.) */
/* Note that all whites are assumed to be normalised (ie. Y = 1.0) */

/* Various changes have been made to allow the CAM conversions to */
/* function over a much greater range of XYZ and Jab values that */
/* the functions are described in the above references. This is */
/* because such values arise in the process of gamut mapping, and */
/* in scanning through the grid of PCS values needed to fill in */
/* the A2B table of an ICC profile. Such values have no correlation */
/* to a real color value, but none the less need to be handled without */
/* causing an exception, in a geometrically consistent and reversible */
/* fashion. */ 

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "xcam.h"
#include "cam97s3.h"

#undef DIAG			/* Print internal value diagnostics for each conversion */

#define CAM_PI 3.14159265359

/* Utility function */
/* Return a viewing condition enumeration from the given Ambient and */
/* Adapting/Surround Luminance. */
static ViewingCondition cam97_Ambient2VC(
double La,		/* Ambient Luminance (cd/m^2) */
double Lv		/* Luminance of white in the Viewing/Scene/Image field (cd/m^2) */
) {
	double r;

	if (fabs(La) < 1e-10) 		/* Hmm. */
		r = 1.0;
	else
		r = La/Lv;

	if (r < 0.01)
		return vc_dark;
	if (r < 0.2)
		return vc_dim;
	return vc_average;
}

static void cam_free(cam97s3 *s);
static int set_view(struct _cam97s3 *s, ViewingCondition Ev, double Wxyz[3],
                    double La, double Yb, double Lv, double Yf, double Fxyz[3],
					int hk);
static int XYZ_to_cam(struct _cam97s3 *s, double *Jab, double *xyz);
static int cam_to_XYZ(struct _cam97s3 *s, double *xyz, double *Jab);

/* Create a cam97s3 conversion object, with default viewing conditions */
cam97s3 *new_cam97s3(void) {
	cam97s3 *s;
//	double D50[3] = { 0.9642, 1.0000, 0.8249 };

	if ((s = (cam97s3 *)calloc(1, sizeof(cam97s3))) == NULL) {
		fprintf(stderr,"cam97s3: malloc failed allocating object\n");
		exit(-1);
	}
	
	/* Initialise methods */
	s->del      = cam_free;
	s->set_view = set_view;
	s->XYZ_to_cam = XYZ_to_cam;
	s->cam_to_XYZ = cam_to_XYZ;

	/* Set a default viewing condition ?? */
	/* set_view(s, vc_average, D50, 33.0, 0.2, 0.0, 0.0, D50, 0); */

	return s;
}

static void cam_free(cam97s3 *s) {
	if (s != NULL)
		free(s);
}

/* A version of the pow() function that preserves the */
/* sign of its first argument. */
static double spow(double x, double y) {
	return x < 0.0 ? -pow(-x,y) : pow(x,y);
}

static int set_view(
cam97s3 *s,
ViewingCondition Ev,	/* Enumerated Viewing Condition */
double Wxyz[3],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
double La,		/* Adapting/Surround Luminance cd/m^2 */
double Yb,		/* Relative Luminance of Background to reference white */
double Lv,		/* Luminance of white in the Viewing/Scene/Image field (cd/m^2) */
				/* Ignored if Ev is set to other than vc_none */
double Yf,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
double Fxyz[3],	/* The Flare white coordinates (typically the Ambient color) */
int hk			/* Flag, NZ to use Helmholtz-Kohlraush effect */
) {
	double tt;

	if (Ev == vc_none)		/* Compute enumerated viewing condition */
		Ev = cam97_Ambient2VC(La, Lv);
	/* Transfer parameters to the object */
	s->Ev = Ev;
	s->Wxyz[0] = Wxyz[0];
	s->Wxyz[1] = Wxyz[1];
	s->Wxyz[2] = Wxyz[2];
	s->Yb = Yb > 0.005 ? Yb : 0.005;	/* Set minimum to avoid divide by 0.0 */
	s->La = La;
	s->Yf = Yf;
	s->Fxyz[0] = Fxyz[0];
	s->Fxyz[1] = Fxyz[1];
	s->Fxyz[2] = Fxyz[2];
	s->hk = hk;

	/* Compute the internal parameters by category */
	switch(s->Ev) {
		case vc_dark:
			s->C = 0.525;
			s->Nc = 0.8;
			s->F = 0.9;
			break;
		case vc_dim:
			s->C = 0.59;
			s->Nc = 0.95;
			s->F = 0.9;
			break;
		case vc_cut_sheet:
			s->C = 0.41;
			s->Nc = 0.8;
			s->F = 0.9;
			break;
		default:	/* average */
			s->C = 0.69;
			s->Nc = 1.0;
			s->F = 1.0;
			break;
	}

	/* Compute values that only change with viewing parameters */

	/* Figure out the Flare contribution to the flareless XYZ input */
	tt = s->Yf * s->Wxyz[1]/s->Fxyz[1];
	s->Fsxyz[0] = tt * s->Fxyz[0];
	s->Fsxyz[1] = tt * s->Fxyz[1];
	s->Fsxyz[2] = tt * s->Fxyz[2];

	/* Rescale so that the sum of the flare and the input doesn't exceed white */
	s->Fsc = s->Wxyz[1]/(s->Fsxyz[1] + s->Wxyz[1]);
	s->Fsxyz[0] *= s->Fsc;
	s->Fsxyz[1] *= s->Fsc;
	s->Fsxyz[2] *= s->Fsc;
	s->Fisc = 1.0/s->Fsc;

	/* Sharpened cone response white values */
	s->rgbW[0] =  0.8562 * s->Wxyz[0] + 0.3372 * s->Wxyz[1] - 0.1934 * s->Wxyz[2];
	s->rgbW[1] = -0.8360 * s->Wxyz[0] + 1.8327 * s->Wxyz[1] + 0.0033 * s->Wxyz[2];
	s->rgbW[2] =  0.0357 * s->Wxyz[0] - 0.0469 * s->Wxyz[1] + 1.0112 * s->Wxyz[2];
	
	/* Degree of chromatic adaptation */
	s->D = s->F - (s->F / (1.0 + 2.0 * pow(s->La, 0.25) + s->La * s->La / 300.0) );

	/* Chromaticaly transformed white value */
	s->rgbcW[0] = (s->D * (1.0/s->rgbW[0]) + 1.0 - s->D ) * s->rgbW[0];
	s->rgbcW[1] = (s->D * (1.0/s->rgbW[1]) + 1.0 - s->D ) * s->rgbW[1];
	s->rgbcW[2] = (s->D * (1.0/s->rgbW[2]) + 1.0 - s->D ) * s->rgbW[2];
	
	/* Transform from spectrally sharpened, to Hunt-Pointer_Estevez cone space */
	s->rgbpW[0] =  0.6962394300923847 * s->rgbcW[0]
	            +  0.2492311682812913 * s->rgbcW[1]
	            +  0.0545394016263241 * s->rgbcW[2];
	s->rgbpW[1] =  0.3054822636273227 * s->rgbcW[0]
	            +  0.5921282520433844 * s->rgbcW[1]
	            +  0.1023894843292929 * s->rgbcW[2];
	s->rgbpW[2] = -0.0139683251072516 * s->rgbcW[0]
	            +  0.0278065725014340 * s->rgbcW[1]
	            +  0.9861617526058175 * s->rgbcW[2];

	/* Background induction factor */
    s->n = s->Yb/ s->Wxyz[1];
	s->nn = pow((1.64 - pow(0.29, s->n)), 1.41);	/* Pre computed value */

	/* Lightness contrast factor ?? */
	{
		double k;

		k = 1.0 / (5.0 * s->La + 1.0);
		s->Fl = 0.2 * pow(k , 4.0) * 5.0 * s->La
		      + 0.1 * pow(1.0 - pow(k , 4.0) , 2.0) * pow(5.0 * s->La , 1.0/3.0);
	}

	/* Background and Chromatic brightness induction factors */
	s->Nbb   = 0.725 * pow(1.0/s->n, 0.2);
	s->Ncb   = s->Nbb;

	/* Base exponential nonlinearity */
	s->z = 1.0 + pow(s->n , 0.5);

	/* Post-adapted cone response of white */
	tt = pow(s->Fl * s->rgbpW[0], 0.73);
	s->rgbaW[0] = (40.0 * tt / (tt + 2.0)) + 1.0;
	tt = pow(s->Fl * s->rgbpW[1], 0.73);
	s->rgbaW[1] = (40.0 * tt / (tt + 2.0)) + 1.0;
	tt = pow(s->Fl * s->rgbpW[2], 0.73);
	s->rgbaW[2] = (40.0 * tt / (tt + 2.0)) + 1.0;

	/* Achromatic response of white */
    s->Aw = (2.0 * s->rgbaW[0] + s->rgbaW[1] + (1.0/20.0) * s->rgbaW[2] - 3.05) * s->Nbb;

#ifdef DIAG
	printf("Scene parameters:\n");
	printf("Viewing condition Ev = %d\n",s->Ev);
	printf("Ref white Wxyz = %f %f %f\n", s->Wxyz[0], s->Wxyz[1], s->Wxyz[2]);
	printf("Relative liminance of background Yb = %f\n", s->Yb);
	printf("Adapting liminance La = %f\n", s->La);
	printf("Flare Yf = %f\n", s->Yf);
	printf("Flare color Fxyz = %f %f %f\n", s->Fxyz[0], s->Fxyz[1], s->Fxyz[2]);

	printf("Internal parameters:\n");
	printf("Surround Impact C = %f\n", s->C);
	printf("Chromatic Induction Nc = %f\n", s->Nc);
	printf("Adaptation Degree F = %f\n", s->F);

	printf("Pre-computed values\n");
	printf("Sharpened cone white rgbW = %f %f %f\n", s->rgbW[0], s->rgbW[1], s->rgbW[2]);
	printf("Degree of chromatic adaptation D = %f\n", s->D);
	printf("Chromatically transformed white rgbcW = %f %f %f\n", s->rgbcW[0], s->rgbcW[1], s->rgbcW[2]);
	printf("Hunter-P-E cone response white rgbpW = %f %f %f\n", s->rgbpW[0], s->rgbpW[1], s->rgbpW[2]);
	printf("Background induction factor n = %f\n", s->n);
	printf("Lightness contrast factor Fl = %f\n", s->Fl);
	printf("Background brightness induction factor Nbb = %f\n", s->Nbb);
	printf("Chromatic brightness induction factor Ncb = %f\n", s->Ncb);
	printf("Base exponential nonlinearity z = %f\n", s->z);
	printf("Post adapted cone response white rgbaW = %f %f %f\n", s->rgbaW[0], s->rgbaW[1], s->rgbaW[2]);
	printf("Achromatic response of white Aw = %f\n", s->Aw);
#endif
	return 0;
}

/* Conversions */
static int XYZ_to_cam(
struct _cam97s3 *s,
double Jab[3],
double XYZ[3]
) {
	int i;
	double xyz[3], rgb[3], rgbp[3], rgba[3], rgbc[3];
	double a, b, nab, J, C, h, e, A, ss;
	double ttd, tt;

	/* Add in flare */
	xyz[0] = s->Fsc * XYZ[0] + s->Fsxyz[0];
	xyz[1] = s->Fsc * XYZ[1] + s->Fsxyz[1];
	xyz[2] = s->Fsc * XYZ[2] + s->Fsxyz[2];

	/* Spectrally sharpened cone responses */
	rgb[0] =  0.8562 * xyz[0] + 0.3372 * xyz[1] - 0.1934 * xyz[2];
	rgb[1] = -0.8360 * xyz[0] + 1.8327 * xyz[1] + 0.0033 * xyz[2];
	rgb[2] =  0.0357 * xyz[0] - 0.0469 * xyz[1] + 1.0112 * xyz[2];
	
	/* Chromaticaly transformed sample value */
	rgbc[0] = (s->D * (1.0/s->rgbW[0]) + 1.0 - s->D ) * rgb[0];
	rgbc[1] = (s->D * (1.0/s->rgbW[1]) + 1.0 - s->D ) * rgb[1];
	rgbc[2] = (s->D * (1.0/s->rgbW[2]) + 1.0 - s->D ) * rgb[2];
	
	/* Transform from spectrally sharpened, to Hunt-Pointer_Estevez cone space */
	rgbp[0] =  0.6962394300923847 * rgbc[0]
	        +  0.2492311682812913 * rgbc[1]
	        +  0.0545394016263241 * rgbc[2];
	rgbp[1] =  0.3054822636273227 * rgbc[0]
	        +  0.5921282520433844 * rgbc[1]
	        +  0.1023894843292929 * rgbc[2];
	rgbp[2] = -0.0139683251072516 * rgbc[0]
	        +  0.0278065725014340 * rgbc[1]
	        +  0.9861617526058175 * rgbc[2];

	/* Post-adapted cone response of sample. */
	/* rgba[] has a minimum value of 1.0 for XYZ[] = 0 and no flare. */
	/* We add linear segments at the ends of this conversion to */
	/* allow numerical handling of a wider range of values */
	for (i = 0; i < 3; i++) {
		if (rgbp[i] < 0.0) {
			tt = pow(s->Fl * -rgbp[i], 0.73);
			if (tt < 78.0)
				rgba[i] = (2.0 - 39.0 * tt) / (tt + 2.0);
			else 
				rgba[i] = (2.0 - tt) / 2.0;

		} else {
			tt = pow(s->Fl * rgbp[i], 0.73);
			if (tt < 78.0)
				rgba[i] = (41.0 * tt + 2.0) / (tt + 2.0);
			else
				rgba[i] = (tt + 2.0) / 2.0;
		}
	}

	/* Preliminary red-green & yellow-blue opponent dimensions */
	a     = rgba[0] - 12.0 * rgba[1]/11.0 + rgba[2]/11.0;
    b     = (1.0/9.0) * (rgba[0] + rgba[1] - 2.0 * rgba[2]);
	nab   = sqrt(a * a + b * b);		/* Normalised a, b */

	/* Hue angle */
    h = (180.0/CAM_PI) * atan2(b,a);
	h = (h < 0.0) ? h + 360.0 : h;

	/* Eccentricity factor */
	{
		double r, e1, e2, h1, h2;
		
		if (h <= 20.14)
			e1 = 0.8565, e2 = 0.8, h1 = 0.0, h2 = 20.14;
		else if (h <= 90.0) 
			e1 = 0.8, e2 = 0.7, h1 = 20.14, h2 = 90.0;
		else if (h <= 164.25)
			e1 = 0.7, e2 = 1.0, h1 = 90.0, h2 = 164.25;
		else if (h <= 237.53)
			e1 = 1.0, e2 = 1.2, h1 = 164.25, h2 = 237.53;
		else
			e1 = 1.2, e2 = 0.8565, h1 = 237.53, h2 = 360.0;
	
		r = (h-h1)/(h2-h1);
#ifdef CIECAM97S3_SPLINE_E
		r = r * r * (3.0 - 2.0 * r);
#endif
		e = e1 + r * (e2-e1);
	}

	/* Achromatic response */
	/* Note that the minimum values of rgba[] for XYZ = 0 is 1.0, */
	/* hence magic 3.05 below comes from the following weighting of rgba[], */
	/* to base A at 0.0 */
	A = (2.0 * rgba[0] + rgba[1] + (1.0/20.0) * rgba[2] - 3.05) * s->Nbb;

	/* Lightness */
	J = spow(A/s->Aw, s->C * s->z);		/* J/100  - keep Sign */

	/* Saturation */
	/* Note that the minimum values for rgba[] for XYZ = 0 is 1.0 */
	/* Hence magic 3.05 below comes from the following weighting of rgba[] */
	ttd = rgba[0] + rgba[1] + (21.0/20.0) * rgba[2];
	ttd = fabs(ttd);
	if (ttd < 3.05) {	/* If not physically realisable, limit denominator */
		ttd = 3.05;		/* hence limit max ss value */
	}
	ss = (50000.0/13.0 * s->Nc * s->Ncb * nab * e) / ttd;

	/* Chroma - Keep C +ve and make sure J doesn't force it to 0  */
	tt = fabs(J);
	if (tt < 0.01)
		tt = 0.01;
	C = 0.7487 * pow(ss, 0.973) * pow(tt, 0.945 * s->n) * s->nn;
	
 	/* Helmholtz-Kohlraush effect */
	if (s->hk) {
		double kk = C/300.0 * sin(CAM_PI * fabs(0.5 * (h - 90.0))/180.0);
		if (kk > 0.9)		/* Limit kk to a reasonable range */
			kk = 0.9;
		J = J + (1.0 - J) * kk;
	}

	J *= 100.0;		/* Scale J */

	/* Compute Jab value */
	Jab[0] = J;
	if (nab > 1e-10) {
		Jab[1] = C * a/nab;
		Jab[2] = C * b/nab;
	} else {
		Jab[1] = 0.0;
		Jab[2] = 0.0;
	}

#ifdef DIAG
	printf("Processing:\n");
	printf("XYZ = %f %f %f\n", XYZ[0], XYZ[1], XYZ[2]);
	printf("Including flare XYZ = %f %f %f\n", xyz[0], xyz[1], xyz[2]);
	printf("Sharpened cone sample rgb = %f %f %f\n", rgb[0], rgb[1], rgb[2]);
	printf("Chromatically transformed sample value rgbc = %f %f %f\n", rgbc[0], rgbc[1], rgbc[2]);
	printf("Hunt-P-E cone space rgbp = %f %f %f\n", rgbp[0], rgbp[1], rgbp[2]);
	printf("Post adapted cone response rgba = %f %f %f\n", rgba[0], rgba[1], rgba[2]);
	printf("Prelim red green a = %f, b = %f\n", a, b);
	printf("Hue angle h = %f\n", h);
	printf("Eccentricity factor e = %f\n", e);
	printf("Achromatic response A = %f\n", A);
	printf("Lightness J = %f\n", J);
	printf("Saturation ss = %f\n", ss);
	printf("Chroma C = %f\n", C);
	printf("Jab = %f %f %f\n", Jab[0], Jab[1], Jab[2]);
#endif
	return 0;
}

static int cam_to_XYZ(
struct _cam97s3 *s,
double XYZ[3],
double Jab[3]
) {
	int i;
	double xyz[3], rgb[3], rgbp[3], rgba[3], rgbc[3];
	double ja, jb, aa, ab, a, b, J, C, h, e, A, ss;
	double tt, ttA, tte;

	J = Jab[0] * 0.01;	/* J/100 */
	ja = Jab[1];
	jb = Jab[2];

	/* Compute hue angle */
    h = (180.0/CAM_PI) * atan2(jb, ja);
	h = (h < 0.0) ? h + 360.0 : h;
	
	/* Compute chroma value */
	C = sqrt(ja * ja + jb * jb);		/* Must be Always +ve */

 	/* Helmholtz-Kohlraush effect */
	if (s->hk) {
		double kk = C/300.0 * sin(CAM_PI * fabs(0.5 * (h - 90.0))/180.0);
		if (kk > 0.9)		/* Limit kk to a reasonable range */
			kk = 0.9;
		J = (J - kk)/(1.0 - kk);
	}

	/* Eccentricity factor */
	{
		double r, e1, e2, h1, h2;
		
		if (h <= 20.14)
			e1 = 0.8565, e2 = 0.8, h1 = 0.0, h2 = 20.14;
		else if (h <= 90.0) 
			e1 = 0.8, e2 = 0.7, h1 = 20.14, h2 = 90.0;
		else if (h <= 164.25)
			e1 = 0.7, e2 = 1.0, h1 = 90.0, h2 = 164.25;
		else if (h <= 237.53)
			e1 = 1.0, e2 = 1.2, h1 = 164.25, h2 = 237.53;
		else
			e1 = 1.2, e2 = 0.8565, h1 = 237.53, h2 = 360.0;
	
		r = (h-h1)/(h2-h1);
#ifdef CIECAM97S3_SPLINE_E
		r = r * r * (3.0 - 2.0 * r);
#endif
		e = e1 + r * (e2-e1);
	}

	/* Achromatic response */
	A = spow(J, 1.0/(s->C * s->z)) * s->Aw;				/* Keep sign of J */

	/* Saturation - keep +ve and make sure J = 0 doesn't blow it up. */
	tt = fabs(J);
	if (tt < 0.01)
		tt = 0.01;
	ss = pow(C/(0.7487 * pow(tt, 0.945 * s->n) * s->nn), 1.0/0.973);	/* keep +ve */

	/* Compute a & b, taking care of numerical problems */
	aa = fabs(ja);
	ab = fabs(jb);
    ttA = (A/s->Nbb)+3.05;						/* Common factor */
	tte = 50000.0/13.0 * e * s->Nc * s->Ncb;	/* Common factor */

	if (aa < 1e-10 && ab < 1e-10) {
		a = ja;
		b = jb;
	} else if (aa > ab) {
		double tanh = jb/ja;
		double sign = (h > 90.0 && h <= 270.0) ? -1.0 : 1.0;
	
		if (ttA < 0.0)
			sign = -sign;

		a = (ss * ttA)
		  / (sign * sqrt(1.0 + tanh * tanh) * tte + (ss * (11.0/23.0 + (108.0/23.0) * tanh)));
		b = a * tanh;

	} else {	/* ab > aa */
		double itanh = ja/jb;
		double sign = (h > 180.0 && h <= 360.0) ? -1.0 : 1.0;

		if (ttA < 0.0)
			sign = -sign;

		b = (ss * ttA)
		  / (sign * sqrt(1.0 + itanh * itanh) * tte + (ss * (108.0/23.0 + (11.0/23.0) * itanh)));
		a = b * itanh;
	}

	{ 	/* Check if we have a limited saturation because it is non-realisable */
		double tts;
		double nab  = sqrt(a * a + b * b);	/* Normalised a, b */
		tts = (nab * tte) / 3.05;	/* Limited saturation number */
		if (tts < ss) {		/* Saturation exceeds it anyway so must have limited denom. */
			a *= ss/tts;	/* Rescale a & b to account for extra ss */
			b *= ss/tts;	/* even though denom was limited (since nab was in numerator). */
		}
	} 

	/* Post-adapted cone response of sample */
    rgba[0] = (20.0/61.0) * ttA
	        + ((41.0 * 11.0)/(61.0 * 23.0)) * a
	        + ((288.0 * 1.0)/(61.0 * 23.0)) * b;
    rgba[1] = (20.0/61.0) * ttA
	        - ((81.0 * 11.0)/(61.0 * 23.0)) * a
	        - ((261.0 * 1.0)/(61.0 * 23.0)) * b;
    rgba[2] = (20.0/61.0) * ttA
	        - ((20.0 * 11.0)/(61.0 * 23.0)) * a
	        - ((20.0 * 315.0)/(61.0 * 23.0)) * b;

	/* Hunt-Pointer_Estevez cone space */
	/* (with linear segments at the ends0 */
	tt = 1.0/s->Fl;
	for (i = 0; i < 3; i++) {
		if (rgba[i] < 1.0) {
			double ta = rgba[i] > -38.0 ? rgba[i] : -38.0;
			rgbp[i] = -tt * pow((2.0 - 2.0 * rgba[i] )/(39.0+ ta), 1.0/0.73);
		} else {
			double ta = rgba[i] < 40.0 ? rgba[i] : 40.0;
			rgbp[i] =  tt * pow((2.0 * rgba[i] -2.0)/(41.0 - ta), 1.0/0.73);
		}
	}

	/* Chromaticaly transformed sample value */
	rgbc[0] =  1.7605948990728097 * rgbp[0]
	        -  0.7400833814121892 * rgbp[1]
	        -  0.0205291236096116 * rgbp[2];
	rgbc[1] = -0.9170843265341294 * rgbp[0]
	        +  2.0826033118941054 * rgbp[1]
	        -  0.1655098145167107 * rgbp[2];
	rgbc[2] =  0.0507964678367941 * rgbp[0]
	        -  0.0692054676442407 * rgbp[1]
	        +  1.0184084918427683 * rgbp[2];

	/* Spectrally sharpened cone responses */
	rgb[0]  =  rgbc[0]/(s->D * (1.0/s->rgbW[0]) + 1.0 - s->D);
	rgb[1]  =  rgbc[1]/(s->D * (1.0/s->rgbW[1]) + 1.0 - s->D);
	rgb[2]  =  rgbc[2]/(s->D * (1.0/s->rgbW[2]) + 1.0 - s->D);

	/* XYZ values */
	xyz[0] =  0.9873999149199270 * rgb[0]
	       -  0.1768250198556842 * rgb[1]
	       +  0.1894251049357572 * rgb[2];
	xyz[1] =  0.4504351090445316 * rgb[0]
	       +  0.4649328977527109 * rgb[1]
	       +  0.0846319932027575 * rgb[2];
	xyz[2] = -0.0139683251072516 * rgb[0]
	       +  0.0278065725014340 * rgb[1]
	       +  0.9861617526058175 * rgb[2];

	/* Subtract flare */
	XYZ[0] = s->Fisc * (xyz[0] - s->Fsxyz[0]);
	XYZ[1] = s->Fisc * (xyz[1] - s->Fsxyz[1]);
	XYZ[2] = s->Fisc * (xyz[2] - s->Fsxyz[2]);

#ifdef DIAG
	printf("Processing:\n");
	printf("Jab = %f %f %f\n", Jab[0], Jab[1], Jab[2]);
	printf("Chroma C = %f\n", C);
	printf("Saturation ss = %f\n", ss);
	printf("Lightness J = %f\n", J * 100.0);
	printf("Achromatic response A = %f\n", A);
	printf("Eccentricity factor e = %f\n", e);
	printf("Hue angle h = %f\n", h);
	printf("Prelim red green a = %f, b = %f\n", a, b);
	printf("Post adapted cone response rgba = %f %f %f\n", rgba[0], rgba[1], rgba[2]);
	printf("Hunt-P-E cone space rgbp = %f %f %f\n", rgbp[0], rgbp[1], rgbp[2]);
	printf("Chromatically transformed sample value rgbc = %f %f %f\n", rgbc[0], rgbc[1], rgbc[2]);
	printf("Sharpened cone sample rgb = %f %f %f\n", rgb[0], rgb[1], rgb[2]);
	printf("Including flare XYZ = %f %f %f\n", xyz[0], xyz[1], xyz[2]);
	printf("XYZ = %f %f %f\n", XYZ[0], XYZ[1], XYZ[2]);
#endif
	return 0;
}




