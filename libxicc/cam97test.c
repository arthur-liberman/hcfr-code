
/* 
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    1/5/01
 * Version: 1.20
 *
 * Copyright 2000-2004 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/*
 * This is some test code to test the CIECAM97s3 functionality. 
 */


#include <stdio.h>
#include <math.h>
#include "xcam.h"
#include "cam97s3.h"

#define DIAG		/* Print internal value diagnostics for each conversion */
#define SPOTTEST	/* Test known spot colors */
#undef INVTEST		/* Lab cube to XYZ to Jab to XYZ */
#undef INVTEST2	/* Jab cube to XYZ to Jab */
#define TRES 33
#define USE_HK 0	/* Use Helmholtz-Kohlraush */
#define NOCAMCLIP 1	/* Don't clip before XYZ2Jab */

#ifndef _isnan
#define _isnan(x) ((x) != (x))
#define _finite(x) ((x) == (x))
#endif

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

/* CIE XYZ to perceptual Lab */
static void
XYZ2Lab(double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double x,y,z,fx,fy,fz;

	x = X/0.9642;
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
		if (_isnan(tt))
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
		if (_isnan(tt))
			return tt;
		if (tt > rv)
			rv = tt;
	}
	return rv;
}

int
main(void) {
	int ok = 1;
	double white[6][3] = {
		{ 0.9505, 1.000, 1.0888 },
		{ 0.9505, 1.000, 1.0888 },
		{ 1.0985, 1.000, 0.3558 },
		{ 1.0985, 1.000, 0.3558 },
		{ 0.9505, 1.000, 1.0888 },
		{ 0.9642, 1.000, 0.8249 }	/* D50 for inversion tests */
	};
	double La[6] = { 318.31, 31.83, 318.31, 31.83, 318.31, 150.0 };
	double sample[5][3] = {
		{ 0.1901, 0.2000, 0.2178 },
		{ 0.5706, 0.4306, 0.3196 },
		{ 0.0353, 0.0656, 0.0214 },
		{ 0.1901, 0.2000, 0.2178 },
		{ 0.9505, 1.000, 1.0888 }		/* Check white */
	};

#ifdef CIECAM97S3_HK
#ifdef CIECAM97S3_SPLINE_E
	double correct[5][4] = {			/* Hue range is last two */
		{ 41.14,     0.06,     225.3, 251.9 },
		{ 69.04,     71.04,    19.4,  19.4 },
		{ 35.09,     87.15,   175.3, 175.3 },
		{ 55.64,     82.43,   252.5, 252.5 },
		{ 100.00000, 0.0,     0.0,   360.0 }
	};
#else
	double correct[5][4] = {			/* Hue range is last two */
		{ 41.14,     0.06,     225.3, 251.9 },
		{ 69.056048, 71.20763, 19.4,  19.4 },
		{ 35.365009, 88.65238, 175.3, 175.3 },
		{ 55.266184, 80.55128, 252.5, 252.5 },
		{ 100.00000, 0.0,      0.0,   360.0 }
	};
#endif
#else
	double correct[5][4] = {			/* Hue range is last two */
		{ 41.13,  0.05,   225.3, 251.9 },
		{ 64.14,  71.22,  19.4,  19.4 },
		{ 19.18,  88.64,  175.3, 175.3 },
		{ 39.11,  80.55,  252.5, 252.5 },
		{ 100.0,  0.00,   0.0,   360.0 }		/* Check white */
	};
#endif

	double Jab[3];
	double res[3];
	cam97s3 *cam;
	int c;

	cam = new_cam97s3();

#ifdef SPOTTEST
	for (c = 0; c < 5; c++) {

#ifdef DIAG
		printf("Case %d:\n",c);
#endif /* DIAG */
		cam->set_view(
			cam,
			vc_average,	/* Enumerated Viewing Condition */
			white[c],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
			La[c],		/* Adapting/Surround Luminance cd/m^2 */
			0.20,		/* Relative Luminance of Background to reference white */
			0.0,		/* Luminance of white in image - not used */
			0.00,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
			white[c],	/* The Flare color coordinates (typically the Ambient color) */
			USE_HK
		);
#ifdef DIAG
		printf("\n");
#endif /* DIAG */
	
		{
			double JCh[3], target[3];
			cam->XYZ_to_cam(cam, Jab, sample[c]);
	
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

			cam->cam_to_XYZ(cam, res, Jab);

			if (maxdiff(JCh, target) > 0.05) {
				printf("Spottest: Excesive error in conversion to CAM %f\n",maxdiff(JCh, target));
				ok = 0;
			}
			if (maxdiff(sample[c], res) > 1e-5) {
				printf("Spottest: Excessive error in inversion %f\n",maxdiff(sample[c], res));
				ok = 0;
			}
#ifdef DIAG
			printf("Jab is %f %f %f, Jch is %f %f %f\n",
			        Jab[0], Jab[1], Jab[2],
			        JCh[0], JCh[1], JCh[2]);

			printf("Error to expected value = %f\n",maxdiff(JCh, target));
		
			printf("XYZ is %f %f %f\n",res[0], res[1], res[2]);
			printf("Inversion error = %f\n",maxdiff(sample[c], res));
#endif /* DIAG */
		}
	}
#endif /* SPOTTEST */

#ifdef INVTEST
	for (c = 5; c < 6; c++) {
		/* Get the range of Jab values */
		double min[3] = { 1e6, 1e6, 1e6 };
		double max[3] = { -1e6, -1e6, -1e6 };

		cam->set_view(
			cam,
			vc_average,	/* Enumerated Viewing Condition */
			white[c],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) = D50 */
			34.0,		/* Adapting/Surround Luminance cd/m^2 */
			0.20,		/* Relative Luminance of Background to reference white */
			0.0,		/* Luminance of white in image - not used */
			0.01,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
			white[c],	/* The Flare color coordinates (typically the Ambient color) */
			USE_HK
		);
	
		{
			int co0, co1, co2;		/* (using co[3] triggers compiler bug) */
			double merr = 0.0;
			double xyz[3], Lab[3], Jab[3], checkxyz[3];


#ifdef NEVER
			/* Test case */
			Lab[0] = 0.0;
			Lab[1] = -180.0;
			Lab[2] = -180.0;
			Lab2XYZ(xyz, Lab);
			cam->XYZ_to_cam(cam, Jab, xyz);
			cam->cam_to_XYZ(cam, checkxyz, Jab);

#else /* !NEVER */

			/* itterate through Lab space */
			for (co0 = 0; co0 < TRES; co0++) {
				Lab[0] = co0/(TRES-1.0);
				Lab[0] = Lab[0] * 100.0;
				for (co1 = 0; co1 < TRES; co1++) {
					Lab[1] = co1/(TRES-1.0);
					Lab[1] = (Lab[1] - 0.5) * 256.0;
					for (co2 = 0; co2 < TRES; co2++) {
						double mxd;
						int i;
						Lab[2] = co2/(TRES-1.0);
						Lab[2] = (Lab[2] - 0.5) * 256.0;
		
						Lab2XYZ(xyz, Lab);
						cam->XYZ_to_cam(cam, Jab, xyz);
						for (i = 0; i < 3; i++) {
							if (Jab[i] < min[i])
								min[i] = Jab[i];
							if (Jab[i] > max[i])
								max[i] = Jab[i];
						}
						cam->cam_to_XYZ(cam, checkxyz, Jab);

						/* Check the result */
						mxd = maxxyzdiff(checkxyz, xyz);
						if (_finite(merr) && (_isnan(mxd) || mxd > merr))
							merr = mxd;
#ifdef DIAG
						if (_isnan(mxd) || mxd > 0.1) {
							printf("\n");
							printf("Lab = %f %f %f\n",Lab[0], Lab[1], Lab[2]);
							printf("%f %f %f -> %f %f %f -> %f %f %f [%f]\n",
							xyz[0],xyz[1],xyz[2],Jab[0],Jab[1],Jab[2],
							checkxyz[0],checkxyz[1],checkxyz[2],
							maxxyzdiff(checkxyz, xyz));
						}
#endif /* DIAG */
					}
				}
			}
			if (_isnan(merr) || merr > 0.1) {
				printf("Excessive error in inversion check %f\n",merr);
				ok = 0;
			}
#ifdef DIAG
			printf("Inversion check complete, peak error = %f\n",merr);
#endif /* !NEVER */

#endif /* DIAG */
			printf("\n");
			printf("Range of Jab values was:\n");
			printf("J:  %f -> %f\n", min[0], max[0]);
			printf("a:  %f -> %f\n", min[1], max[1]);
			printf("b:  %f -> %f\n", min[2], max[2]);
		}
	}
#endif /* INVTEST */

#ifdef INVTEST2
	for (c = 5; c < 6; c++) {
		cam->set_view(
			cam,
			vc_average,	/* Enumerated Viewing Condition */
			white[c],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) = D50 */
			34.0,		/* Adapting/Surround Luminance cd/m^2 */
			0.20,		/* Relative Luminance of Background to reference white */
			0.0,		/* Luminance of white in image - not used */
			0.01,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
			white[c],	/* The Flare color coordinates (typically the Ambient color) */
			USE_HK
		);
	
		{
			int co0, co1, co2;		/* (using co[3] triggers compiler bug) */
			double merr = 0.0;
			double xyz[3], Jab[3], checkJab[3];

#ifdef NEVER
			/* Test case */
#ifdef NEVER
			Jab[0] = 0.0;
			Jab[1] = -180.0;
			Jab[2] = -180.0;
			cam->cam_to_XYZ(cam, xyz, Jab);
			cam->XYZ_to_cam(cam, checkJab, xyz);
#else
			xyz[0] = 555.365115;
			xyz[1] = -518.091908;
			xyz[2] = -1591.731701;
			cam->XYZ_to_cam(cam, Jab, xyz);
			cam->cam_to_XYZ(cam, xyz, Jab);
#endif

#else /* !NEVER */

			/* itterate through Jab space */
			for (co0 = 0; co0 < TRES; co0++) {
				Jab[0] = co0/(TRES-1.0);
				Jab[0] = 0.0 + Jab[0] * 100.0;	
				for (co1 = 0; co1 < TRES; co1++) {
					Jab[1] = co1/(TRES-1.0);
					Jab[1] = (Jab[1] - 0.5) * 256.0;
					for (co2 = 0; co2 < TRES; co2++) {
						double mxd;
						Jab[2] = co2/(TRES-1.0);
						Jab[2] = (Jab[2] - 0.5) * 256.0;
		
						cam->cam_to_XYZ(cam, xyz, Jab);
						cam->XYZ_to_cam(cam, checkJab, xyz);

						/* Check the result */
						mxd = maxdiff(checkJab, Jab);
						if (_finite(merr) && (_isnan(mxd) || mxd > merr))
							merr = mxd;
#ifdef DIAG
						if (_isnan(mxd) || mxd > 0.1) {
							printf("\n");
							printf("Jab = %f %f %f\n",Jab[0], Jab[1], Jab[2]);
							printf("%f %f %f -> %f %f %f -> %f %f %f [%f]\n",
							Jab[0],Jab[1],Jab[2], xyz[0],xyz[1],xyz[2],
							checkJab[0],checkJab[1],checkJab[2],
							maxdiff(checkJab, Jab));
						}
#endif /* DIAG */
					}
				}
			}
			if (_isnan(merr) || merr > 1.0) {
				printf("Excessive error in inversion check 2 %f\n",merr);
				ok = 0;
			}
#ifdef DIAG
			printf("Inversion check 2 complete, peak error = %f\n",merr);
#endif /* DIAG */
#endif /* !NEVER */
		}
	}
#endif /* INVTEST2 */

	printf("\n");
	if (ok == 0) {
		printf("Cam testing FAILED\n");
	} else {
		printf("Cam testing OK\n");
	}

	return 0;
}

