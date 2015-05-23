
/* 
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    18/1/2004
 * Version: 1.00
 *
 * Copyright 2000-2011 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/*
 * This is some test code to test the CIECAM02 functionality. 
 */


#include <stdio.h>
#include <math.h>
#include "icc.h"
#include "xcam.h"
#include "cam02.h"
#include "numlib.h"
#include "cam02ref.h"

					/* ** = usual tests */

#undef DIAG			/* Print internal value diagnostics for each spot test conversion */
					/* and print diagnostics for excessive errors, nans etc. */
#undef VERBOSE		/* Print diagnostic values for every conversion */
#define SPOTTEST		/* ** Test known spot colors */
#undef TROUBLE		/* Test trouble spot colors XYZ -> Jab -> XYZ */
#undef TROUBLE2		/* Test trouble spot colors Jab -> XYZ -> Jab */
#undef SPECIAL		/* Special exploration code */

#undef LOCUSTEST	/* [def] ** Test spectrum locus points */
#undef LOCUSRAND	/* [def] ** Test random spectrum locus points */

#define INVTEST		/* [def] ** -100 -> 100 XYZ cube to Jab to XYZ */
#undef INVTEST1		/* [undef] Single value */
#undef INVTEST2		/* [undef] Powell search for invers */

#define TESTINV		/* [def] ** Jab cube to XYZ to Jab */
#undef TESTINV1		/* [undef] Single Jab value */
#undef TESTINV2		/* [undef] J = 0 test values */

//#define TRES 41		/* Grid resolution */
#define TRES 17		/* Grid resolution */
#define USE_HK 0	/* Use Helmholtz-Kohlraush in testing */
#define EXIT_ON_ERROR	/* and also trace */

//#define MAX_SPOT_ERR 0.05
//#define MAX_REF_ERR 0.1	/* Maximum permitted error to reference transform in delta Jab */
/* The blue fix throws this out */
#define MAX_SPOT_ERR 2.0
#define MAX_REF_ERR 2.0	/* Maximum permitted error to reference transform in delta Jab */

#ifndef _isnan
#define _isnan(x) ((x) != (x))
#define _finite(x) ((x) == 0.0 || (x) * 1.0000001 != (x))
#endif


#ifdef INVTEST1
static void
Lab2XYZ(double *out, double *in) {
	double L = in[0], a = in[1], b = in[2];
	double x,y,z,fx,fy,fz;

	if (L > 8.0) {
		fy = (L + 16.0)/116.0;
		y = pow(fy,3.0);
	} else {
		y = L/903.2963058;
		fy = 7.787036979 * y + 16.0/116.0;
	}

	fx = a/500.0 + fy;
	if (fx > 24.0/116.0)
		x = pow(fx,3.0);
	else
		x = (fx - 16.0/116.0)/7.787036979;

	fz = fy - b/200.0;
	if (fz > 24.0/116.0)
		z = pow(fz,3.0);
	else
		z = (fz - 16.0/116.0)/7.787036979;

	out[0] = x * 0.9642;
	out[1] = y * 1.0000;
	out[2] = z * 0.8249;
}
#endif /* INVTEST1 */

/* CIE XYZ to perceptual Lab */
static void
XYZ2Lab(double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double x,y,z,fx,fy,fz;

	x = X/0.9642;		/* D50 ? */
	y = Y/1.0000;
	z = Z/0.8249;

	if (x > 0.008856451586)
		fx = pow(x,1.0/3.0);
	else
		fx = 7.787036979 * x + 16.0/116.0;

	if (y > 0.008856451586)
		fy = pow(y,1.0/3.0);
	else
		fy = 7.787036979 * y + 16.0/116.0;

	if (z > 0.008856451586)
		fz = pow(z,1.0/3.0);
	else
		fz = 7.787036979 * z + 16.0/116.0;

	out[0] = 116.0 * fy - 16.0;
	out[1] = 500.0 * (fx - fy);
	out[2] = 200.0 * (fy - fz);
}


/* Return maximum difference */
double maxdiff(double in1[3], double in2[3]) {
	int i;
	double tt, rv = 0.0;

	/* Deal with the possibility that we have nan's */
	for (i = 0; i < 3; i++) {
		tt = fabs(in1[i] - in2[i]);
		if (!_finite(tt))
			return tt;
		if (tt > rv)
			rv = tt;
	}
	return rv;
}

/* Return absolute difference */
double absdiff(double in1[3], double in2[3]) {
	double tt, rv = 0.0;
	tt = in1[0] - in2[0];
	rv += tt * tt;
	tt = in1[1] - in2[1];
	rv += tt * tt;
	tt = in1[2] - in2[2];
	rv += tt * tt;
	return sqrt(rv);
}

/* Return maximum Lab difference of XYZ */
double maxxyzdiff(double i1[3], double i2[3]) {
	int i;
	double tt, rv = 0.0;
	double in1[3], in2[3];

	XYZ2Lab(in1, i1);
	XYZ2Lab(in2, i2);

	/* Deal with the possibility that we have nan's */
	for (i = 0; i < 3; i++) {
		tt = fabs(in1[i] - in2[i]);
		if (!_finite(tt))
			return tt;
		if (tt > rv)
			rv = tt;
	}
	return rv;
}

#ifdef INVTEST2

/* Powell callback function and struct */
typedef struct {
	cam02 *cam;
	double Jab[3];
} cntx;

static double opt1(void *fdata, double tp[]) {
	cntx *x = (cntx *)fdata;
	double Jab[3];
	double tt, rv = 0.0;

	x->cam->XYZ_to_cam(x->cam, Jab, tp);
	
	tt = Jab[0] - x->Jab[0];
	rv += tt * tt;
	tt = Jab[1] - x->Jab[1];
	rv += tt * tt;
	tt = Jab[2] - x->Jab[2];
	rv += tt * tt;
	return rv;
}

#endif /* INVTEST2 */

/* 2 degree spectrum locus in xy coordinates */
/* nm, x, y, Y CMC */
double sl[65][4] = {
	{ 380, 0.1741, 0.0050, 0.000039097450 },
	{ 385, 0.1740, 0.0050, 0.000065464490 },
	{ 390, 0.1738, 0.0049, 0.000121224052 },
	{ 395, 0.1736, 0.0049, 0.000221434140 },
	{ 400, 0.1733, 0.0048, 0.000395705080 },
	{ 405, 0.1730, 0.0048, 0.000656030940 },
	{ 410, 0.1726, 0.0048, 0.001222776600 },
	{ 415, 0.1721, 0.0048, 0.002210898200 },
	{ 420, 0.1714, 0.0051, 0.004069952000 },
	{ 425, 0.1703, 0.0058, 0.007334133400 },
	{ 430, 0.1689, 0.0069, 0.011637600000 },
	{ 435, 0.1669, 0.0086, 0.016881322000 },
	{ 440, 0.1644, 0.0109, 0.023015402000 },
	{ 445, 0.1611, 0.0138, 0.029860866000 },
	{ 450, 0.1566, 0.0177, 0.038072300000 },
	{ 455, 0.1510, 0.0227, 0.048085078000 },
	{ 460, 0.1440, 0.0297, 0.060063754000 },
	{ 465, 0.1355, 0.0399, 0.074027114000 },
	{ 470, 0.1241, 0.0578, 0.091168598000 },
	{ 475, 0.1096, 0.0868, 0.112811680000 },
	{ 480, 0.0913, 0.1327, 0.139122260000 },
	{ 485, 0.0686, 0.2007, 0.169656160000 },
	{ 490, 0.0454, 0.2950, 0.208513180000 },
	{ 495, 0.0235, 0.4127, 0.259083420000 },
	{ 500, 0.0082, 0.5384, 0.323943280000 },
	{ 505, 0.0039, 0.6548, 0.407645120000 },
	{ 510, 0.0139, 0.7502, 0.503483040000 },
	{ 515, 0.0389, 0.8120, 0.608101540000 },
	{ 520, 0.0743, 0.8338, 0.709073280000 },
	{ 525, 0.1142, 0.8262, 0.792722560000 },
	{ 530, 0.1547, 0.8059, 0.861314320000 },
	{ 535, 0.1929, 0.7816, 0.914322820000 },
	{ 540, 0.2296, 0.7543, 0.953482260000 },
	{ 545, 0.2658, 0.7243, 0.979818740000 },
	{ 550, 0.3016, 0.6923, 0.994576720000 },
	{ 555, 0.3373, 0.6589, 0.999604300000 },
	{ 560, 0.3731, 0.6245, 0.994513460000 },
	{ 565, 0.4087, 0.5896, 0.978204680000 },
	{ 570, 0.4441, 0.5547, 0.951588260000 },
	{ 575, 0.4788, 0.5202, 0.915060800000 },
	{ 580, 0.5125, 0.4866, 0.869647940000 },
	{ 585, 0.5448, 0.4544, 0.816076000000 },
	{ 590, 0.5752, 0.4242, 0.756904640000 },
	{ 595, 0.6029, 0.3965, 0.694818180000 },
	{ 600, 0.6270, 0.3725, 0.630997820000 },
	{ 605, 0.6482, 0.3514, 0.566802360000 },
	{ 610, 0.6658, 0.3340, 0.503096860000 },
	{ 615, 0.6801, 0.3197, 0.441279360000 },
	{ 620, 0.6915, 0.3083, 0.380961920000 },
	{ 625, 0.7006, 0.2993, 0.321156580000 },
	{ 630, 0.7079, 0.2920, 0.265374180000 },
	{ 635, 0.7140, 0.2859, 0.217219520000 },
	{ 640, 0.7190, 0.2809, 0.175199900000 },
	{ 645, 0.7230, 0.2770, 0.138425720000 },
	{ 650, 0.7260, 0.2740, 0.107242628000 },
	{ 655, 0.7283, 0.2717, 0.081786794000 },
	{ 660, 0.7300, 0.2700, 0.061166218000 },
	{ 665, 0.7311, 0.2689, 0.044729418000 },
	{ 670, 0.7320, 0.2680, 0.032160714000 },
	{ 675, 0.7327, 0.2673, 0.023307860000 },
	{ 680, 0.7334, 0.2666, 0.017028548000 },
	{ 685, 0.7340, 0.2660, 0.011981432000 },
	{ 690, 0.7344, 0.2656, 0.008259734600 },
	{ 695, 0.7346, 0.2654, 0.005758363200 },
	{ 700, 0.7347, 0.2653, 0.004117206200 }
};

int
main(void) {
	int ok = 1;

#define NO_WHITES 9
	double white[9][3] = {
		{ 0.964242, 1.000000, 0.825124 },		/* 0: D50 */
		{ 1.098494, 1.000000, 0.355908 },		/* 1: A */
		{ 0.908731, 1.000000, 0.987233 },		/* 2: F5 */
		{ 1.128981, 1.000000, 0.305862 },		/* 3: D25 */
		{ 0.950471, 1.000000, 1.088828 },		/* 4: D65 */
		{ 0.949662, 1.000000, 1.160438 },		/* 5: D80 */
		{ 0.949662, 1.000000, 1.160438 },		/* 6: D80 */
		{ 0.951297, 1.000000, 1.338196 },		/* 7: D85 */
		{ 0.952480, 1.000000, 1.386693 }		/* 8: D90 */
	};
#define NO_LAS 6
	double La[6] = { 10.0, 31.83, 100.0, 318.31, 1000.83, 3183.1 };

#define NO_VCS 4

	ViewingCondition Vc[4] = {
		vc_average,
		vc_dark,
		vc_dim,
		vc_cut_sheet
	};

#ifdef SPOTTEST
#define NO_SPOTS 5
	double sp_white[6][3] = {
		{ 0.9505, 1.000, 1.0888 },
		{ 0.9505, 1.000, 1.0888 },
		{ 1.0985, 1.000, 0.3558 },
		{ 1.0985, 1.000, 0.3558 },
		{ 0.9505, 1.000, 1.0888 },
		{ 0.9642, 1.000, 0.8249 }	/* D50 for inversion tests */
	};
	double sp_La[6] = { 318.31, 31.83, 318.31, 31.83, 318.31, 150.0 };

	double sample[5][3] = {
		{ 0.1901, 0.2000, 0.2178 },
		{ 0.5706, 0.4306, 0.3196 },
		{ 0.0353, 0.0656, 0.0214 },
		{ 0.1901, 0.2000, 0.2178 },
		{ 0.9505, 1.000, 1.0888 }		/* Check white */
	};

	/* Reference values (NOT correct anymore, as HK params have changed!!!) */
#if USE_HK == 1
	double correct[5][4] = {			/* Hue range is last two */
		{ 41.75,  0.10,    217.0, 219.0 },
		{ 69.11,  48.60,   19.9,  19.91 },
		{ 30.22,  46.94,   177.1, 177.2 },
		{ 52.64,  53.07,   249.5, 249.6 },
		{ 100.00, 0.14,    0.0,   360.0 }
	};
#else
	double correct[5][4] = {			/* Hue range is last two */
		{ 41.73,  0.10,   217.0, 219.0 },
		{ 65.94,  48.60,  19.90, 19.91 },
		{ 21.79,  46.94,  177.1, 177.2 },
		{ 42.66,  53.07,  249.5, 249.6 },
		{ 100.0,  0.14,   0.0,   360.0 }		/* Check white */
	};
#endif	/* USE_HK */
// Original values
//#if USE_HK == 1
//	double correct[5][4] = {			/* Hue range is last two */
//		{ 41.75,  0.10,    217.0, 219.0 },
//		{ 69.13,  48.57,   19.56, 19.56 },
//		{ 30.22,  46.94,   177.1, 177.1 },
//		{ 52.31,  51.92,   248.9, 248.9 },
//		{ 100.00, 0.14,    0.0,   360.0 }
//	};
#//else
//	double correct[5][4] = {			/* Hue range is last two */
//		{ 41.73,  0.10,   217.0, 219.0 },
//		{ 65.96,  48.57,  19.6,  19.6 },
//		{ 21.79,  46.94,  177.1, 177.1 },
//		{ 42.53,  51.92,  248.9, 248.9 },
//		{ 100.0,  0.14,   0.0,   360.0 }		/* Check white */
//	};
//#endif	/* USE_HK */
#endif	/* SPOTTEST */

#if defined(TROUBLE) || defined(TROUBLE2) || defined(SPECIAL)
#define NO_TROUBLE 1
	double twhite[3][3] = {
		{ 0.9642, 1.000, 0.8249 },	/* D50 */
		{ 0.949662, 1.000000, 1.160438 },		/* 5: D80 */
		{ 0.9642, 1.000, 0.8249 }	/* D50 */
	};
	ViewingCondition tVc[3] = { vc_average, vc_average, vc_average };
	double tLa[3] = { 10.0, 3183.1, 50.0 };
	double tFlair[3] = { 0.00, 0.01, 0.01 };
	double tsample[4][3] = {
		{ -1.0, -1.0, -1.0 },	/* Inv Problem */
		{ 34.485160, 0.982246, 164.621489 },	/* Inv Problem */
		{ 0.031339, 0.000000, 0.824895 },		/* ProPhoto Blue - OK */
		{ 0.041795, 0.006211, 0.652597 }		/* ProPhoto nearly Blue - fails */
	};
	double tsample2[1][3] = {
		{ 3.625000, -121.600000, -64.000000, }	/* Inv Problem */
	};
#endif	/* TROUBLE || TROUBLE2 */

	cam02ref *camr;
	cam02 *cam;
	int c, d, e;

	cam = new_cam02();
	camr = new_cam02ref();
	camr->range = 1;		/* Return on range error */
#ifdef VERBOSE
	cam->trace = 1;
	camr->trace = 1;
#endif

	/* ================= Trouble colors ===================== */
#if defined(TROUBLE) || defined(TROUBLE2) || defined(SPECIAL)
	for (c = 0; c < NO_TROUBLE; c++) {

#ifdef DIAG
		printf("Case %d:\n",c);
#endif /* DIAG */
		camr->set_view(
			camr,
			tVc[c],		/* Enumerated Viewing Condition */
			twhite[c],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
			tLa[c],		/* Adapting/Surround Luminance cd/m^2 */
			0.20,		/* Relative Luminance of Background to reference white */
			0.0,		/* Luminance of white in image - not used */
			tFlair[c],	/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
			0.00,		/* Glare as a fraction of the ambient (Y range 0.0 .. 1.0) */
			twhite[c],	/* The Flare color coordinates (typically the Ambient color) */
			USE_HK		/* use Helmholtz-Kohlraush flag */ 
		);
		cam->set_view(
			cam,
			tVc[c],		/* Enumerated Viewing Condition */
			twhite[c],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
			tLa[c],		/* Adapting/Surround Luminance cd/m^2 */
			0.20,		/* Relative Luminance of Background to reference white */
			0.0,		/* Luminance of white in image - not used */
			tFlair[c],	/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
			0.00,		/* Glare as a fraction of the ambient (Y range 0.0 .. 1.0) */
			twhite[c],	/* The Flare color coordinates (typically the Ambient color) */
			USE_HK		/* use Helmholtz-Kohlraush flag */ 
		);
		camr->nldlimit = cam->nldlimit;
		camr->jlimit = cam->jlimit;

#ifdef DIAG
		printf("\n");
#endif /* DIAG */
	
#ifdef SPECIAL
	/* See what happens as we approach a neutral color */
	{
		double j1[3] = { 50, +0.1, +0.1 }, x1[3];
		double j2[3] = { 0, +0.1, +0.1 }, x2[3];
		double Jab[3], xyz[3];
		double rr = 1.0;
		int i;

		/* Establish the XYZ */
		cam->cam_to_XYZ(cam, x1, j1);
		cam->cam_to_XYZ(cam, x2, j2);

		cam->trace = 1;

		/* Approach it by halves */
		for (i = 0; i < 100; i++, rr *= 0.5) {
			xyz[0] = rr * x1[0] + (1.0 - rr) * x2[0];
			xyz[1] = rr * x1[1] + (1.0 - rr) * x2[1];
			xyz[2] = rr * x1[2] + (1.0 - rr) * x2[2];

			cam->XYZ_to_cam(cam, Jab, xyz);

			printf("XYZ %f %f %f -> Jab is %f %f %f\n",
			        xyz[0], xyz[1], xyz[2],
			        Jab[0], Jab[1], Jab[2]);
		}
		exit(0);
	}
#endif /* SPECIAL */

#ifdef TROUBLE
		{
			double Jab[3], jabr[3], xyz[3];

			cam->XYZ_to_cam(cam, Jab, tsample[c]);

			if (camr->XYZ_to_cam(camr, jabr, tsample[c])) {
#ifdef DIAG
				printf("Reference XYZ2Jab returned error from XYZ %f %f %f\n",tsample[c][0],tsample[c][1],tsample[c][2]);
#endif /* DIAG */
			} else if (maxdiff(Jab, jabr) > MAX_REF_ERR) {
				printf("Spottest: Excessive error to reference for %f %f %f\n",tsample[c][0],tsample[c][1],tsample[c][2]);
				printf("Spottest: Error %f, Got %f %f %f\n                      expected %f %f %f\n", maxdiff(Jab, jabr), Jab[0], Jab[1], Jab[2], jabr[0], jabr[1], jabr[2]);
				ok = 0;
#ifdef EXIT_ON_ERROR
				fflush(stdout);
				exit(-1);
#endif /* EXIT_ON_ERROR */
			}


#ifdef DIAG
			printf("XYZ %f %f %f -> Jab is %f %f %f\n",
			        tsample[c][0], tsample[c][1], tsample[c][2],
			        Jab[0], Jab[1], Jab[2]);
#endif /* DIAG */

			cam->cam_to_XYZ(cam, xyz, Jab);

#ifdef DIAG
			printf("XYZ %f %f %f -> Jab is %f %f %f\n",
			        Jab[0], Jab[1], Jab[2],
			        xyz[0], xyz[1], xyz[2]);
#endif /* DIAG */

			if (maxdiff(tsample[c], xyz) > 1e-5) {
				printf("Spottest trouble 1: Excessive error in inversion %f\n",maxdiff(tsample[c], xyz));
				printf("Is        %f %f %f\n",xyz[0],xyz[1],xyz[2]);
				printf("Should be %f %f %f\n",tsample[c][0],tsample[c][1],tsample[c][2]);
				ok = 0;
#ifdef EXIT_ON_ERROR
				fflush(stdout);
				exit(-1);
#endif /* EXIT_ON_ERROR */
			}
		}
#endif /* TROUBLE */

#ifdef TROUBLE2
		{
			double Jab[3], xyz[3];

			cam->cam_to_XYZ(cam, xyz, tsample2[c]);

#ifdef DIAG
			printf("Lab %f %f %f -> XYZ is %f %f %f\n",
			        tsample2[c][0], tsample2[c][1], tsample2[c][2],
			        xyz[0], xyz[1], xyz[2]);
#endif /* DIAG */

			cam->XYZ_to_cam(cam, Jab, xyz);

#ifdef DIAG
			printf("XYZ %f %f %f -> Jab is %f %f %f\n",
			        xyz[0], xyz[1], xyz[2],
			        Jab[0], Jab[1], Jab[2]);
#endif /* DIAG */

			if (maxdiff(tsample2[c], Jab) > 1e-3) {
				printf("Spottest trouble2: Excessive error in inversion %f\n",maxdiff(tsample2[c], Jab));
				printf("Is        %f %f %f\n",Jab[0],Jab[1],Jab[2]);
				printf("Should be %f %f %f\n",tsample2[c][0],tsample2[c][1],tsample2[c][2]);
				ok = 0;
#ifdef EXIT_ON_ERROR
				fflush(stdout);
				exit(-1);
#endif /* EXIT_ON_ERROR */
			}
		}
#endif /* TROUBLE2 */
	}
#endif /* TROUBLE || TROUBLE 2 */
	/* =============================================== */

	/* ================= SpotTest ===================== */
#ifdef SPOTTEST
	{
	double mrerr = 0.0, mxerr = 0.0;
	for (c = 0; c < NO_SPOTS; c++) {

#ifdef DIAG
		printf("Case %d:\n",c);
#endif /* DIAG */
		camr->set_view(
			camr,
			vc_average,	/* Enumerated Viewing Condition */
			sp_white[c],/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
			sp_La[c],	/* Adapting/Surround Luminance cd/m^2 */
			0.20,		/* Relative Luminance of Background to reference white */
			0.0,		/* Luminance of white in image - not used */
			0.00,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
			0.00,		/* Glare as a fraction of the ambient (Y range 0.0 .. 1.0) */
			sp_white[c],/* The Glare color coordinates (typically the Ambient color) */
			USE_HK		/* use Helmholtz-Kohlraush flag */ 
		);

		cam->set_view(
			cam,
			vc_average,	/* Enumerated Viewing Condition */
			sp_white[c],/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
			sp_La[c],	/* Adapting/Surround Luminance cd/m^2 */
			0.20,		/* Relative Luminance of Background to reference white */
			0.0,		/* Luminance of white in image - not used */
			0.00,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
			0.00,		/* Glare as a fraction of the ambient (Y range 0.0 .. 1.0) */
			sp_white[c],/* The Glare color coordinates (typically the Ambient color) */
			USE_HK		/* use Helmholtz-Kohlraush flag */ 
		);

		camr->nldlimit = cam->nldlimit;
		camr->jlimit = cam->jlimit;

#ifdef DIAG
		printf("\n");
#endif /* DIAG */
	
		{
			double Jab[3], JCh[3], jabr[3], res[3], target[3];
			double mde;

			cam->XYZ_to_cam(cam, Jab, sample[c]);

			if (camr->XYZ_to_cam(camr, jabr, sample[c]))
				printf("Reference XYZ2Jab returned error\n");
			else if ((mde = maxdiff(Jab, jabr)) > MAX_REF_ERR) {
				printf("Spottest: Excessive error to reference for %f %f %f\n",sample[c][0],sample[c][1],sample[c][2]);
				printf("Spottest: Error %f, Got %f %f %f\n                     expected %f %f %f\n",maxdiff(Jab, jabr),
				                               Jab[0], Jab[1], Jab[2], jabr[0], jabr[1], jabr[2]);
				ok = 0;
#ifdef EXIT_ON_ERROR
				camr->trace = 1;
				camr->XYZ_to_cam(camr, jabr, sample[c]);
				camr->trace = 0;
				cam->trace = 1;
				cam->XYZ_to_cam(cam, Jab, sample[c]);
				cam->trace = 0;
				fflush(stdout);
				fflush(stdout);
				exit(-1);
#endif /* EXIT_ON_ERROR */
			}
			if (mde > mrerr)
				mrerr = mde;

			/* Convert to JCh for checking */
			JCh[0] = Jab[0];

			/* Compute hue angle */
    		JCh[2] = (180.0/3.14159265359) * atan2(Jab[2], Jab[1]);
			JCh[2] = (JCh[2] < 0.0) ? JCh[2] + 360.0 : JCh[2];
	
			/* Compute chroma value */
			JCh[1] = sqrt(Jab[1] * Jab[1] + Jab[2] * Jab[2]);

			target[0] = correct[c][0];
			target[1] = correct[c][1];
			if (JCh[2] >= correct[c][2] && JCh[2] <= correct[c][3]) {
				target[2] = JCh[2];
			} else {
				if (fabs(JCh[2] - correct[c][2]) < fabs(JCh[2] - correct[c][3]))
					target[2] = correct[c][2];
				else
					target[2] = correct[c][3];
			}

			if ((mde = maxdiff(JCh, target)) > MAX_SPOT_ERR) {
				printf("Spottest: Excessive error for %f %f %f\n",sample[c][0],sample[c][1],sample[c][2]);
				printf("Spottest: Excessive error in conversion to CAM %f\n",maxdiff(JCh, target));
				printf("Jab is %f %f %f\nJch is        %f %f %f\n",
				        Jab[0], Jab[1], Jab[2],
				        JCh[0], JCh[1], JCh[2]);
	
				printf("JCh should be %f %f %f - %f\n",
				correct[c][0], correct[c][1], correct[c][2], correct[c][3]);
		
				ok = 0;
#ifdef EXIT_ON_ERROR
				fflush(stdout);
				exit(-1);
#endif /* EXIT_ON_ERROR */
			}
			if (mde > mxerr)
				mxerr = mde;

			cam->cam_to_XYZ(cam, res, Jab);

			if (maxdiff(sample[c], res) > 1e-5) {
				printf("Spottest: Excessive error in inversion %f\n",maxdiff(sample[c], res));
				ok = 0;
#ifdef EXIT_ON_ERROR
				cam->trace = 1;
				cam->XYZ_to_cam(cam, Jab, sample[c]);
				cam->cam_to_XYZ(cam, res, Jab);
				cam->trace = 0;
				fflush(stdout);
				exit(-1);
#endif /* EXIT_ON_ERROR */
			}
#ifdef DIAG
			printf("Jab is %f %f %f\nJch is        %f %f %f\n",
			        Jab[0], Jab[1], Jab[2],
			        JCh[0], JCh[1], JCh[2]);

			printf("JCh should be %f %f %f - %f\n",
			correct[c][0], correct[c][1], correct[c][2], correct[c][3]);
		
			printf("XYZ is %f %f %f\n",res[0], res[1], res[2]);
			printf("Inversion error = %f\n",maxdiff(sample[c], res));
			printf("\n\n");
#endif /* DIAG */
		}
	}
	if (ok == 0) {
		printf("Spottest testing FAILED\n");
	} else {
		printf("Spottest testing OK\n");
		printf("Spottest maximum DE to ref = %f, to expected = %f\n",mrerr,mxerr);
	}
	}
#endif /* SPOTTEST */
	/* =============================================== */

	/* ================= LocusTest ===================== */
#ifdef LOCUSTEST
#define LT_YRES 30
	{
	double mxlocde = -100.0;		/* maximum locus test delta E */

	for (c = 0; c < NO_WHITES; c++) {
	printf("Locus: next white reference\n");
	for (d = 0; d < NO_LAS; d++) {
	for (e = 0; e < NO_VCS; e++) {
		int i, j;
		double Yxy[3], xyz[3], Jab[3], jabr[3], checkxyz[3];
		double mxd;		/* Maximum delta of Lab any component */
		double ltde;	/* Locus test delta E */

		camr->set_view(
			camr,
			Vc[e],		/* Enumerated Viewing Condition */
			white[c],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) = D50 */
			La[d],		/* Adapting/Surround Luminance cd/m^2 */
			0.20,		/* Relative Luminance of Background to reference white */
			0.0,		/* Luminance of white in image - not used */
			0.00,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
			0.00,		/* Glare as a fraction of the ambient (Y range 0.0 .. 1.0) */
			white[c],	/* The Glare color coordinates (typically the Ambient color) */
			USE_HK		/* use Helmholtz-Kohlraush flag */ 
		);
		
		cam->set_view(
			cam,
			Vc[e],		/* Enumerated Viewing Condition */
			white[c],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) = D50 */
			La[d],		/* Adapting/Surround Luminance cd/m^2 */
			0.20,		/* Relative Luminance of Background to reference white */
			0.0,		/* Luminance of white in image - not used */
			0.00,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
			0.00,		/* Glare as a fraction of the ambient (Y range 0.0 .. 1.0) */
			white[c],	/* The Glare color coordinates (typically the Ambient color) */
			USE_HK		/* use Helmholtz-Kohlraush flag */ 
		);
		
		/* Make reference return error where it's going to disagree with implementation */
		camr->nldlimit = cam->nldlimit;
		camr->jlimit = cam->jlimit;

		/* The monochromic boundary with a unit energy monochromic source */
		for (j = 0; j < LT_YRES; j++) {
			double Y = j/(LT_YRES - 1.0);
	
			Y = Y * Y * 0.999 + 0.001;
	
			for (i = 0; i < 65; i++) {
				Yxy[0] = Y * sl[i][3];
				Yxy[1] = sl[i][1];
				Yxy[2] = sl[i][2];
	
				icmYxy2XYZ(xyz, Yxy);

				cam->XYZ_to_cam(cam, Jab, xyz);

				if (camr->XYZ_to_cam(camr, jabr, xyz)) {
#ifdef DIAG
					printf("Reference XYZ2Jab returned error from XYZ %f %f %f\n",xyz[0],xyz[1],xyz[2]);
#endif /* DIAG */
				} else if (maxdiff(Jab, jabr) > MAX_REF_ERR) {
					printf("Locustest1: Excessive error to reference for %f %f %f\n",xyz[0],xyz[1],xyz[2]);
					printf("Locustest1: Error %f, Got %f %f %f\n              expected %f %f %f\n",maxdiff(Jab, jabr),
					                               Jab[0], Jab[1], Jab[2], jabr[0], jabr[1], jabr[2]);
					ok = 0;
#ifdef EXIT_ON_ERROR
					fflush(stdout);
					exit(-1);
#endif /* EXIT_ON_ERROR */
				}

				cam->cam_to_XYZ(cam, checkxyz, Jab);

#ifdef DIAG
				printf("Inversion error = %f\n",maxdiff(checkxyz, xyz));
#endif /* DIAG */

				/* Check the result */
				ltde = icmXYZLabDE(&icmD50, checkxyz, xyz);
				if (ltde > mxlocde)
					mxlocde = ltde;
				mxd = maxxyzdiff(checkxyz, xyz);
				if (mxd > 0.05) {
					printf("XYZ %f %f %f\n ->",xyz[0],xyz[1],xyz[2]);
					printf("Jab %f %f %f\n ->",Jab[0],Jab[1],Jab[2]);
					printf("XYZ %f %f %f\n",checkxyz[0],checkxyz[1],checkxyz[2]);
					printf("Locustest1: Excessive roundtrip error %f\n",mxd);
					printf("c = %d, d = %d, e = %d\n",c,d,e);
					ok = 0;
#ifdef EXIT_ON_ERROR
					fflush(stdout);
					exit(-1);
#endif /* EXIT_ON_ERROR */
				}
			}
	
			/* Do the purple line */
			for (i = 0; i < 20; i++) {
				double b = i/(20 - 1.0);
	
				Yxy[0] = (b * sl[0][3] + (1.0 - b) * sl[64][3]) * Y;
				Yxy[1] = b * sl[0][1] + (1.0 - b) * sl[64][1];
				Yxy[2] = b * sl[0][2] + (1.0 - b) * sl[64][2];
				icmYxy2XYZ(xyz, Yxy);

				cam->XYZ_to_cam(cam, Jab, xyz);

				if (camr->XYZ_to_cam(camr, jabr, xyz)) {
#ifdef DIAG
					printf("Reference XYZ2Jab returned error from XYZ %f %f %f\n",xyz[0],xyz[1],xyz[2]);
#endif /* DIAG */
				} else if (maxdiff(Jab, jabr) > MAX_REF_ERR) {
					printf("Locustest2: Excessive error to reference for %f %f %f\n",xyz[0],xyz[1],xyz[2]);
					printf("Locustest2: Error %f, Got %f %f %f,\n                 expected %f %f %f\n",maxdiff(Jab, jabr),
					                               Jab[0], Jab[1], Jab[2], jabr[0], jabr[1], jabr[2]);
					ok = 0;
#ifdef EXIT_ON_ERROR
					fflush(stdout);
					exit(-1);
#endif /* EXIT_ON_ERROR */
				}

				cam->cam_to_XYZ(cam, checkxyz, Jab);

#ifdef DIAG
				printf("Inversion error = %f\n",maxdiff(checkxyz, xyz));
#endif /* DIAG */

				/* Check the result */
				ltde = icmXYZLabDE(&icmD50, checkxyz, xyz);
				if (ltde > mxlocde)
					mxlocde = ltde;
				mxd = maxxyzdiff(checkxyz, xyz);
				if (mxd > 0.05) {
					printf("XYZ %f %f %f\n ->",xyz[0],xyz[1],xyz[2]);
					printf("Jab %f %f %f\n ->",Jab[0],Jab[1],Jab[2]);
					printf("XYZ %f %f %f\n",checkxyz[0],checkxyz[1],checkxyz[2]);
					printf("Locustest2: Excessive roundtrip error %f\n",mxd);
					printf("c = %d, d = %d, e = %d\n",c,d,e);
					ok = 0;
#ifdef EXIT_ON_ERROR
					fflush(stdout);
					exit(-1);
#endif /* EXIT_ON_ERROR */
				}
			}
		}

#ifdef LOCUSRAND
		for (i = 0; i < 20000 ; i++) {
//		for (i = 0; i < 1000000 ; i++) {
			int i1, i2, i3;
			double Y, bb1, bb2, bb3;

			i1 = i_rand(0, 64);
			i2 = i_rand(0, 64);
			i3 = i_rand(0, 64);
			bb1 = d_rand(0.0, 1.0);
			bb2 = d_rand(0.0, 1.0 - bb1);
			bb3 = 1.0 - bb1 - bb2;
			Y = d_rand(0.001, 1.0);
//printf("~1 i1 = %d, i2 = %d, i3 = %d\n",i1,i2,i3);
//printf("~1 bb1 = %f, bb2 = %f, bb3 = %f\n",bb1,bb2,bb3);
//printf("~1 Y = %f\n",Y);

			Yxy[0] = bb1 * sl[i1][3] + bb2 * sl[i2][3] + bb3 * sl[i3][3];
			Yxy[1] = bb1 * sl[i1][1] + bb2 * sl[i2][1] + bb3 * sl[i3][1];
			Yxy[2] = bb1 * sl[i1][2] + bb2 * sl[i2][2] + bb3 * sl[i3][2];
			Yxy[0] = Y;		/* ??? */
			icmYxy2XYZ(xyz, Yxy);

			Yxy[0] = (bb1 * sl[i1][3] + bb2 * sl[i2][3] + bb3 * sl[i3][3]) * Y;
//printf("~1 xyz = %f %f %f\n", xyz[0], xyz[1], xyz[2]);

			cam->XYZ_to_cam(cam, Jab, xyz);

			if (camr->XYZ_to_cam(camr, jabr, xyz)) {
#ifdef DIAG
				printf("Reference XYZ2Jab returned error from XYZ %f %f %f\n",xyz[0],xyz[1],xyz[2]);
#endif /* DIAG */
			} else if (maxdiff(Jab, jabr) > MAX_REF_ERR) {
				printf("Locustest3: Excessive error to reference for %f %f %f\n",xyz[0],xyz[1],xyz[2]);
				printf("Locustest3: Error %f, Got %f %f %f, expected %f %f %f\n",maxdiff(Jab, jabr),
				                               Jab[0], Jab[1], Jab[2], jabr[0], jabr[1], jabr[2]);
				printf("c = %d, d = %d, e = %d\n",c,d,e);
				ok = 0;
#ifdef EXIT_ON_ERROR
				fflush(stdout);
				exit(-1);
#endif /* EXIT_ON_ERROR */
			}

			cam->cam_to_XYZ(cam, checkxyz, Jab);

#ifdef DIAG
			printf("Inversion error = %f\n",maxdiff(checkxyz, xyz));
#endif /* DIAG */

			/* Check the result */
			ltde = icmXYZLabDE(&icmD50, checkxyz, xyz);
			if (ltde > mxlocde)
				mxlocde = ltde;
			mxd = maxxyzdiff(checkxyz, xyz);
			if (mxd > 0.05) {
				printf("XYZ %f %f %f\n ->",xyz[0],xyz[1],xyz[2]);
				printf("Jab %f %f %f\n ->",Jab[0],Jab[1],Jab[2]);
				printf("XYZ %f %f %f\n",checkxyz[0],checkxyz[1],checkxyz[2]);
				printf("Locustest3: Excessive roundtrip error %f\n",mxd);
				printf("c = %d, d = %d, e = %d\n",c,d,e);
				ok = 0;
#ifdef EXIT_ON_ERROR
				fflush(stdout);
				exit(-1);
#endif /* EXIT_ON_ERROR */
			}
		}
#endif

	}	/* Viewing condition */
	}	/* Luninence */
	}	/* Whites */
	if (ok == 0) {
		printf("Locustest FAILED\n");
	} else {
		printf("Locustest OK, max DE = %e\n",mxlocde);
	}
	}
#endif /* LOCUSTEST */
	/* =============================================== */

	/* =================  XYZ->Jab->XYZ ================== */
#if defined(INVTEST) || defined(INVTEST1) || defined(INVTEST2)
	{
		/* Get the range of Jab values */
		double xmin[3] = { 1e38, 1e38, 1e38 };
		double xmax[3] = { -1e38, -1e38, -1e38 };
		double jmin[3] = { 1e38, 1e38, 1e38 };
		double jmax[3] = { -1e38, -1e38, -1e38 };
		double merr = 0.0;
		for (c = 0; c < 6; c++) {
			int co0, co1, co2;		/* (using co[3] triggers compiler bug) */
			double xyz[3], Lab[3], Jab[3], checkxyz[3];
			double mxd;


			cam->set_view(
				cam,
				vc_average,	/* Enumerated Viewing Condition */
				white[c],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) = D50 */
				34.0,		/* Adapting/Surround Luminance cd/m^2 */
				0.20,		/* Relative Luminance of Background to reference white */
				0.0,		/* Luminance of white in image - not used */
				0.0,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
				0.0,		/* Glare as a fraction of the ambient (Y range 0.0 .. 1.0) */
				white[c],	/* The Glare color coordinates (typically the Ambient color) */
				USE_HK		/* use Helmholtz-Kohlraush flag */ 
			);
		
#ifdef INVTEST1
			/* Test case */
			Lab[0] = 0.0; Lab[1] = -128.0; Lab[2] = 36.571429;
			Lab2XYZ(xyz, Lab);
			cam->XYZ_to_cam(cam, Jab, xyz);
			cam->cam_to_XYZ(cam, checkxyz, Jab);

			/* Check the result */
			mxd = maxxyzdiff(checkxyz, xyz);
			if (_finite(merr) && (!_finite(mxd) || mxd > merr))
				merr = mxd;
#ifdef DIAG
#ifndef VERBOSE
			if (!_finite(mxd) || mxd > 0.1)
#endif /* VERBOSE */
			{
				printf("\n");
				printf("c = %d, #### Lab = %f %f %f\n",c, Lab[0], Lab[1], Lab[2]);
				printf("%f %f %f -> %f %f %f -> %f %f %f [%f]\n",
				xyz[0],xyz[1],xyz[2],Jab[0],Jab[1],Jab[2],
				checkxyz[0],checkxyz[1],checkxyz[2],
				maxxyzdiff(checkxyz, xyz));
#ifdef EXIT_ON_ERROR
				if (!_finite(mxd) || mxd > 0.1) {
					fflush(stdout);
					exit(-1);
				}
#endif /* EXIT_ON_ERROR */
			}
#endif /* DIAG */

#endif /* INVTEST1 */

#ifdef INVTEST2
			{
				int rv;
				cntx x;
				double xyz[3], ss[3], Jab[3];

				x.cam = cam;
				x.Jab[0] = 0.0;
				x.Jab[1] = -50.0;
				x.Jab[2] = -50.0;

				xyz[0] = -0.3;
				xyz[1] = -0.3;
				xyz[2] = -0.3;

				ss[0] = 0.3;
				ss[1] = 0.3;
				ss[2] = 0.3;

				rv = powell(NULL, 3, xyz, ss, 1e-12, 10000, opt1, (void *)&x);
				if (rv)
					printf("Powell failed\n");
				else {
					printf("XYZ %f %f %f\n",xyz[0], xyz[1], xyz[2]);
				}
				cam->cam_to_XYZ(cam, xyz, x.Jab);
				printf("Inverted XYZ %f %f %f\n",xyz[0], xyz[1], xyz[2]);
				cam->XYZ_to_cam(cam, Jab, xyz);
				printf("Check Jab %f %f %f\n",Jab[0], Jab[1], Jab[2]);
				c = 6;
			}
#endif /* INVTEST2 */

#ifdef INVTEST
			/* itterate through -20 to +120 XYZ cube space */
			for (co0 = 0; co0 < TRES; co0++) {
				xyz[0] = co0/(TRES-1.0);
				xyz[0] = xyz[0] * 1.4 - 0.2;
				for (co1 = 0; co1 < TRES; co1++) {
					xyz[1] = co1/(TRES-1.0);
					xyz[1] = xyz[1] * 1.4 - 0.2;
					for (co2 = 0; co2 < TRES; co2++) {
						int i;
						xyz[2] = co2/(TRES-1.0);
						xyz[2] = xyz[2] * 1.4 - 0.2;
		
						for (i = 0; i < 3; i++) {
							if (xyz[i] < xmin[i])
								xmin[i] = xyz[i];
							if (xyz[i] > xmax[i])
								xmax[i] = xyz[i];
						}
						cam->XYZ_to_cam(cam, Jab, xyz);

						for (i = 0; i < 3; i++) {
							if (Jab[i] < jmin[i])
								jmin[i] = Jab[i];
							if (Jab[i] > jmax[i])
								jmax[i] = Jab[i];
						}
						cam->cam_to_XYZ(cam, checkxyz, Jab);

						/* Check the result */
						mxd = maxxyzdiff(checkxyz, xyz);
						if (_finite(merr) && (!_finite(mxd) || mxd > merr))
							merr = mxd;

#if defined(DIAG) || defined(VERBOSE) || defined(EXIT_ON_ERROR)

#if defined(DIAG) || defined(EXIT_ON_ERROR)
						if (!_finite(mxd) || mxd > 0.5)		/* Delta E */
#endif /* DIAG || EXIT_ON_ERROR */
						{
							double oLab[3];

							printf("\n");
							printf("#### XYZ = %f %f %f -> %f %f %f\n",
							xyz[0], xyz[1], xyz[2], checkxyz[0], checkxyz[1], checkxyz[2]);
							printf("%f %f %f -> %f %f %f -> %f %f %f [%f]\n",
							xyz[0],xyz[1],xyz[2],Jab[0],Jab[1],Jab[2],
							checkxyz[0],checkxyz[1],checkxyz[2],
							maxxyzdiff(checkxyz, xyz));
							printf("c = %d\n",c);
#ifdef EXIT_ON_ERROR
							if (!_finite(mxd) || mxd > 0.1) {
								cam->trace = 1;
								cam->XYZ_to_cam(cam, Jab, xyz);
								cam->cam_to_XYZ(cam, checkxyz, Jab);
								cam->trace = 0;
								fflush(stdout);
								exit(-1);
							}
#endif /* EXIT_ON_ERROR */
						}
#endif /* DIAG || VERBOSE || EXIT_ON_ERROR */
					}
				}
			}
#endif /* INVTEST */
		}
		if (!_finite(merr) || merr > 0.15) {
			printf("INVTEST: Excessive error in roundtrip check %f DE\n",merr);
			ok = 0;
		}
		printf("\n");
		printf("XYZ -> Jab -> XYZ\n");
		printf("Inversion check complete, peak error = %e DE\n",merr);
		printf("Range of XYZ values was:\n");
		printf("X:  %f -> %f\n", xmin[0], xmax[0]);
		printf("Y:  %f -> %f\n", xmin[1], xmax[1]);
		printf("Z:  %f -> %f\n", xmin[2], xmax[2]);
		printf("Range of Jab values was:\n");
		printf("J:  %f -> %f\n", jmin[0], jmax[0]);
		printf("a:  %f -> %f\n", jmin[1], jmax[1]);
		printf("b:  %f -> %f\n", jmin[2], jmax[2]);
	}
#endif /*  INVTEST || INVTEST1 || INVTEST2 */
	/* =============================================== */

	/* ===================Jab->XYX->Jab ============================ */
	/* Note that this is expected to fail if BLUECOMPR is enabled */
#if defined(TESTINV) || defined(TESTINV1) || defined(TESTINV2)
	{
		/* Get the range of XYZ values */
		double xmin[3] = { 1e38, 1e38, 1e38 };
		double xmax[3] = { -1e38, -1e38, -1e38 };
		double jmin[3] = { 1e38, 1e38, 1e38 };
		double jmax[3] = { -1e38, -1e38, -1e38 };
		double merr = 0.0;

		for (c = 0; c < 6; c++) {
			int i, j;
			int co0, co1, co2;		/* (using co[3] triggers compiler bug) */
			double xyz[3], Jab[3], checkJab[3];
			double mxd;

			cam->set_view(
				cam,
				vc_average,	/* Enumerated Viewing Condition */
				white[c],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) = D50 */
				34.0,		/* Adapting/Surround Luminance cd/m^2 */
				0.20,		/* Relative Luminance of Background to reference white */
				0.0,		/* Luminance of white in image - not used */
				0.0,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
				0.0,		/* Glare as a fraction of the ambient (Y range 0.0 .. 1.0) */
				white[c],	/* The Glare color coordinates (typically the Ambient color) */
				USE_HK		/* use Helmholtz-Kohlraush flag */ 
			);
		
#ifdef TESTINV1
			/* Double sample test case */
			for (j = 0; j < 2; j++) {
				if (j == 0) {
//					Jab[0] = -2.5; Jab[1] = 128.0; Jab[2] = 128.0;
					Jab[0] = 0.0; Jab[1] = -7.683075; Jab[2] = -7.683075;
				} else {
//					Jab[0] = -2.5; Jab[1] = -128.0; Jab[2] = -128.0;
					Jab[0] = 0.0; Jab[1] = -128.0; Jab[2] = -128.0;
				}
//				Jab[0] = 21.25; Jab[1] = 0.0; Jab[2] = -64.0;
//				Jab[0] = 1.05; Jab[1] = 0.0; Jab[2] = -64.0;
//				Jab[0] = 0.0; Jab[1] = -128.0; Jab[2] = -128.0;

				for (i = 0; i < 3; i++) {
					if (Jab[i] < jmin[i])
						jmin[i] = Jab[i];
					if (Jab[i] > jmax[i])
						jmax[i] = Jab[i];
				}
				cam->cam_to_XYZ(cam, xyz, Jab);
				for (i = 0; i < 3; i++) {
					if (xyz[i] < xmin[i])
						xmin[i] = xyz[i];
					if (xyz[i] > xmax[i])
						xmax[i] = xyz[i];
				}
				cam->XYZ_to_cam(cam, checkJab, xyz);

				/* Check the result */
				mxd = maxdiff(checkJab, Jab);
				if (_finite(merr) && (!_finite(mxd) || mxd > merr))
					merr = mxd;
#ifdef DIAG
#ifndef VERBOSE
				if (!_finite(mxd) || mxd > 0.1)
#endif /* VERBOSE */
				{
					printf("\n");
					printf("#### Jab = %f %f %f\n",Jab[0], Jab[1], Jab[2]);
					printf("%f %f %f -> %f %f %f -> %f %f %f [%f]\n",
					Jab[0],Jab[1],Jab[2], xyz[0],xyz[1],xyz[2],
					checkJab[0],checkJab[1],checkJab[2],
					maxdiff(checkJab, Jab));
					printf("c = %d\n",c);
#ifdef EXIT_ON_ERROR
					if (!_finite(mxd) || mxd > 0.1) {
						fflush(stdout);
						exit(-1);
					}
#endif /* EXIT_ON_ERROR */
				}
#endif /* DIAG */
			}
#endif /* TESTINV1 */

#ifdef TESTINV2
			{	/* J = 0 test cases */
				int i;
				for (i = 1; i < 128; i++) {
					Jab[0] = 0.0;
					Jab[1] = (double)i/2;
					Jab[2] = (double)i;
					cam->cam_to_XYZ(cam, xyz, Jab);

					Jab[0] = 0.0;
					Jab[1] = (double)-i/2;
					Jab[2] = (double)i;
					cam->cam_to_XYZ(cam, xyz, Jab);

					Jab[0] = 0.0;
					Jab[1] = (double)i/2;
					Jab[2] = (double)-i;
					cam->cam_to_XYZ(cam, xyz, Jab);

					Jab[0] = 0.0;
					Jab[1] = (double)-i/2;
					Jab[2] = (double)-i;
					cam->cam_to_XYZ(cam, xyz, Jab);

					Jab[0] = 0.0;
					Jab[1] = (double)i;
					Jab[2] = (double)i/2;
					cam->cam_to_XYZ(cam, xyz, Jab);

					Jab[0] = 0.0;
					Jab[1] = (double)-i;
					Jab[2] = (double)i/2;
					cam->cam_to_XYZ(cam, xyz, Jab);

					Jab[0] = 0.0;
					Jab[1] = (double)i;
					Jab[2] = (double)-i/2;
					cam->cam_to_XYZ(cam, xyz, Jab);

					Jab[0] = 0.0;
					Jab[1] = (double)-i;
					Jab[2] = (double)-i/2;
					cam->cam_to_XYZ(cam, xyz, Jab);
				}
			}
#endif	/* TESTINV2 */

#ifdef TESTINV
			/* itterate through Jab space */
			for (co0 = 0; co0 < TRES; co0++) {
				Jab[0] = co0/(TRES-1.0);
				Jab[0] = -50.0 + Jab[0] * 165.0;	
				for (co1 = 0; co1 < TRES; co1++) {
					Jab[1] = co1/(TRES-1.0);
					Jab[1] = (Jab[1] - 0.5) * 256.0;
					for (co2 = 0; co2 < TRES; co2++) {
						int i;
						Jab[2] = co2/(TRES-1.0);
						Jab[2] = (Jab[2] - 0.5) * 256.0;
		
						for (i = 0; i < 3; i++) {
							if (Jab[i] < jmin[i])
								jmin[i] = Jab[i];
							if (Jab[i] > jmax[i])
								jmax[i] = Jab[i];
						}
						cam->cam_to_XYZ(cam, xyz, Jab);
						for (i = 0; i < 3; i++) {
							if (xyz[i] < xmin[i])
								xmin[i] = xyz[i];
							if (xyz[i] > xmax[i])
								xmax[i] = xyz[i];
						}
						cam->XYZ_to_cam(cam, checkJab, xyz);

						/* Check the result */
						mxd = maxdiff(checkJab, Jab);
						if (_finite(merr) && (!_finite(mxd) || mxd > merr))
							merr = mxd;
#if defined(DIAG) || defined(VERBOSE) || defined(EXIT_ON_ERROR)

#if defined(DIAG) || defined(EXIT_ON_ERROR)
						if (!_finite(mxd) || mxd > 0.1)
#endif /* DIAG || EXIT_ON_ERROR */
						{
							printf("\n");
							printf("#### Jab = %f %f %f\n",Jab[0], Jab[1], Jab[2]);
							printf("%f %f %f -> %f %f %f -> %f %f %f [%f]\n",
							Jab[0],Jab[1],Jab[2], xyz[0],xyz[1],xyz[2],
							checkJab[0],checkJab[1],checkJab[2],
							maxdiff(checkJab, Jab));
							printf("c = %d\n",c);
#ifdef EXIT_ON_ERROR
							if (!_finite(mxd) || mxd > 0.1) {
								cam->trace = 1;
								cam->cam_to_XYZ(cam, xyz, Jab);
								cam->XYZ_to_cam(cam, checkJab, xyz);
								cam->trace = 0;
								fflush(stdout);
								exit(-1);
							}
#endif /* EXIT_ON_ERROR */
						}
#endif /* DIAG || VERBOSE || EXIT_ON_ERROR */
					}
				}
			}
#endif /* TESTINV */
		}
		if (!_finite(merr) || merr > 1.0) {
			printf("TESTINV: Excessive error in roundtrip check %f\n",merr);
			ok = 0;
		}
		printf("\n");
		printf("Jab -> XYX -> Jab\n");
		printf("Inversion check 2 complete, peak error = %e DE Jab\n",merr);
		printf("Range of Jab values was:\n");
		printf("J:  %f -> %f\n", jmin[0], jmax[0]);
		printf("a:  %f -> %f\n", jmin[1], jmax[1]);
		printf("b:  %f -> %f\n", jmin[2], jmax[2]);
		printf("Range of XYZ values was:\n");
		printf("X:  %f -> %f\n", xmin[0], xmax[0]);
		printf("Y:  %f -> %f\n", xmin[1], xmax[1]);
		printf("Z:  %f -> %f\n", xmin[2], xmax[2]);
	}
#endif /* TESTINV || TESTINV1 TESTINV2 */
	/* =============================================== */

	printf("\n");
	if (ok == 0) {
		printf("Cam testing FAILED\n");
	} else {
		printf("Cam testing OK\n");
	}

	cam->del(cam);

	return 0;
}

