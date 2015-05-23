
/* 
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    11/11/2004
 * Version: 1.00
 *
 * Copyright 2000-2004 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/*
 * This is some test code to test the CIECAM02 continuity. 
 * This test creates file "cam02plot.tif" containing values that
 * have been converted to and back from CIECAM02 space. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "icc.h"
#include "xcam.h"
#include "cam02.h"
#include "tiffio.h"
#include "cam02ref.h"

#define DEFRES 100		/* Default cube resolution */


#define NORMAL_XYZ2RGB
#undef NIEVE_XYZ2RGB
#undef COMPLEX_XYZ2RGB

#define SCALE 1.0		/* Compress RGB by scale towards grey */

#ifndef _isnan
#define _isnan(x) ((x) != (x))
#define _finite(x) ((x) == 0.0 || (x) * 1.0000001 != (x))
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

#ifdef NORMAL_XYZ2RGB

static double sign(double x) {
	if (x < 0.0)
		return -1.0;
	else
		return 1.0;
}

/* Version with reasonable clipping. Converts to Lab, */
/* clips the Lab an then converts to sRGB */
/* Convert from XYZ to sRGB */
void XYZ2sRGB(double *out, double *in, int trace) {
	double lab[3];
	double tmp[3];
	double scale = SCALE;		/* Scale RGB towards grey */
	double mat[3][3] = 
		{
			{  3.2410, -1.5374, -0.4986 },
			{ -0.9692,  1.8760,  0.0416 },
			{  0.0556, -0.2040,  1.0570 }
		};
	int i;

// ~~999
	trace = 0;

	/* Copy from input */
	for(i = 0; i < 3; i++)
		tmp[i] = in[i];

	if (trace) printf("XYZ to RGB\n");
	if (trace) printf("input XYZ %f %f %f\n",tmp[0],tmp[1],tmp[2]);
	/* Clip to reasonable range */
//	if (tmp[1] <= 0.0)
//		tmp[0] = tmp[2] = 0.0;
	XYZ2Lab(lab, tmp);
	if (trace) printf("Lab %f %f %f\n",tmp[0],tmp[1],tmp[2]);
	icmClipLab(lab, lab);
	if (trace) printf("Clipped Lab %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	/* We're going to assume that L == 0 is always black */
	if (lab[0] <= 0.0) {
		lab[1] = lab[2] = 0.0;
	}
	if (trace) printf("Clipped Lab2 %f %f %f\n",tmp[0],tmp[1],tmp[2]);
	Lab2XYZ(tmp, lab);
	if (trace) printf("XYZ %f %f %f\n",tmp[0],tmp[1],tmp[2]);
	icmClipXYZ(tmp, tmp);
	if (trace) printf("clipped XYZ %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	/* Now convert to sRGB assuming D50 (??) white point */
	icmMulBy3x3(tmp, mat, tmp);

	/* Scale */
	for(i = 0; i < 3; i++) {
		tmp[i] = ((tmp[i] - 0.5) * scale) + 0.5;
	}

	if (trace) printf("raw RGB %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	/* Apply gamma */
	for(i = 0; i < 3; i++) {
		if (tmp[i] < 0.00304) {
			tmp[i] = tmp[i] * 12.92;
		} else {
			tmp[i] = 1.055 * pow(tmp[i], 1.0/2.4) - 0.055;
		}
	}

	if (trace) printf("gamma corrected RGB %f %f %f\n",tmp[0],tmp[1],tmp[2]);

#ifndef NEVER
	/* Clip to nearest */
	for(i = 0; i < 3; i++) {
		if (tmp[i] < 0.0)
			tmp[i] = 0.0;
		else if (tmp[i] > 1.0)
			tmp[i] = 1.0;
	}
#else
	/* Clip towards center */
	{
		double biggest = -100.0;
		for(i = 0; i < 3; i++) {
			tmp[i] -= 0.5;
			if (fabs(tmp[i]) > biggest)
				biggest = fabs(tmp[i]);
		}
		for(i = 0; i < 3; i++) {
			if (biggest > 0.5)
				tmp[i] *= 0.5/biggest; 
			tmp[i] += 0.5;
		}
	}
#endif
	if (trace) printf("output clipped RGB %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	/* Copy to output */
	for(i = 0; i < 3; i++)
		out[i] = tmp[i];

}

#endif /* NORMAL_XYZ2RGB */

#ifdef NIEVE_XYZ2RGB

/* Nieve version with RGB clipping */
/* Convert from XYZ to sRGB */
void XYZ2sRGB(double *out, double *in, int trace) {
	double tmp[3];
	double scale = SCALE;		/* Scale RGB towards grey */
	double mat[3][3] = 
		{
			{  3.2410, -1.5374, -0.4986 },
			{ -0.9692,  1.8760,  0.0416 },
			{  0.0556, -0.2040,  1.0570 }
		};
	int i;

	if (trace) printf("XYZ to RGB\n");
	if (trace) printf("input XYZ %f %f %f\n",in[0],in[1],in[2]);

	/* Now convert to sRGB assuming D50 (??) white point */
	icmMulBy3x3(tmp, mat, in);

	/* Scale */
	for(i = 0; i < 3; i++) {
		tmp[i] = ((tmp[i] - 0.5) * scale) + 0.5;
	}

	if (trace) printf("raw RGB %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	/* Clip to nearest */
	for(i = 0; i < 3; i++) {
		if (tmp[i] < 0.0)
			tmp[i] = 0.0;
		else if (tmp[i] > 1.0)
			tmp[i] = 1.0;
	}

	if (trace) printf("clipped RGB %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	/* Apply gamma */
	for(i = 0; i < 3; i++) {
		if (tmp[i] < 0.00304) {
			tmp[i] = tmp[i] * 12.92;
		} else {
			tmp[i] = 1.055 * pow(tmp[i], 1.0/2.4) - 0.055;
		}
	}

	if (trace) printf("output gamma corrected RGB %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	/* Copy to output */
	for(i = 0; i < 3; i++)
		out[i] = tmp[i];
}

#endif /* NIEVE_XYZ2RGB */

#ifdef COMPLEX_XYZ2RGB

/* Complex version using powell to ensure good clipping */
/* Callback context */
typedef struct {
	double scale;
	double lab[3];
	double imat[3][3];
} ctx;

double clipf(void *fdata, double tp[]) {
	ctx *x = (ctx *)fdata;
	double lab[3], tmp[3], tt;
	int k;
	double rv = 0.0;

//printf("\n");
//printf("~1 clipf got %f %f %f\n",tp[0],tp[1],tp[2]);

	/* Penalize for being out of gamut */
	for (k = 0; k < 3; k++) {
		if (tp[k] < 0.0) {
			tt = 100000.0 * (0.0 - tp[k]);
			rv += tt * tt;
		} else if (tp[k] > 1.0) {
			tt = 100000.0 * (tp[k] - 1.0);
			rv += tt * tt;
		}
	}
//printf("~1 rv out of gamut = %f\n",rv);

	/* Unscale it */
	for(k = 0; k < 3; k++)
		tmp[k] = ((tp[k] - 0.5) / x->scale) + 0.5;
//printf("~1 unscale %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	icmMulBy3x3(tmp, x->imat, tmp);		/* To XYZ */
//printf("~1 xyz %f %f %f\n",tmp[0],tmp[1],tmp[2]);
	XYZ2Lab(lab, tmp);					/* To Lab */
//printf("~1 lab    = %f %f %f\n",lab[0],lab[1],lab[2]);
//printf("~1 target = %f %f %f\n",x->lab[0],x->lab[1],x->lab[2]);

	/* Compute an error to the goal */
	tt = 10000.0 * (lab[0] - x->lab[0]);
	rv += tt * tt;

	tt = 1.0 * (lab[1] - x->lab[1]);
	rv += tt * tt;
	tt = 1.0 * (lab[2] - x->lab[2]);
	rv += tt * tt;

//printf("~1 rv = %f\n",rv);

	return rv;
}

/* Convert from XYZ to sRGB */
void XYZ2sRGB(double *out, double *in, int trace) {
	double lab[3], tmp[3];
	double scale = SCALE;		/* Scale RGB towards grey */
	double mat[3][3] = 
		{
			{  3.2410, -1.5374, -0.4986 },
			{ -0.9692,  1.8760,  0.0416 },
			{  0.0556, -0.2040,  1.0570 }
		};
	int i;

	/* Copy from input */
	for(i = 0; i < 3; i++)
		tmp[i] = in[i];

	if (trace) printf("XYZ to RGB\n");

	if (trace) printf("input XYZ %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	XYZ2Lab(lab, tmp);

	if (trace) printf("Lab %f %f %f\n",lab[0],lab[1],lab[2]);

	icmClipLab(lab, lab);

	if (trace) printf("Clipped Lab %f %f %f\n",lab[0],lab[1],lab[2]);

	/* Now convert to sRGB assuming D50 (??) white point */
	Lab2XYZ(tmp, lab);
	icmMulBy3x3(tmp, mat, tmp);

	/* Scale */
	for(i = 0; i < 3; i++) {
		tmp[i] = ((tmp[i] - 0.5) * scale) + 0.5;
	}

	if (trace) printf("raw RGB %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	/* See if it need clipping */
	for(i = 0; i < 3; i++) {
		if (tmp[i] < 0.0 || tmp[i] > 1.0)
			break;
	}
	/* It needs more clipping */
	if (i < 3) {
		ctx x;
		double ss[3] = { 0.1, 0.1, 0.1 };
		x.lab[0] = lab[0];
		x.lab[1] = lab[1];
		x.lab[2] = lab[2];
		x.scale = scale;
		icmInverse3x3(x.imat, mat);

		if (powell(NULL, 3, tmp, ss, 1e-6, 1000, clipf, (void *)&x) < 0.0)
			error("RGB clip failed");
	}
	if (trace) printf("RGB clip %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	/* Clip to nearest */
	for(i = 0; i < 3; i++) {
		if (tmp[i] < 0.0)
			tmp[i] = 0.0;
		else if (tmp[i] > 1.0)
			tmp[i] = 1.0;
	}
	if (trace) printf("clip to nearest RGB %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	/* Apply gamma */
	for(i = 0; i < 3; i++) {
		if (tmp[i] < 0.00304) {
			tmp[i] = tmp[i] * 12.92;
		} else {
			tmp[i] = 1.055 * pow(tmp[i], 1.0/2.4) - 0.055;
		}
	}
	if (trace) printf("gamma corrected RGB %f %f %f\n",tmp[0],tmp[1],tmp[2]);

	/* Copy to output */
	for(i = 0; i < 3; i++)
		out[i] = tmp[i];

}
#endif 	/* COMPLEX_XYZ2RGB */

void usage(char *diag) {
	fprintf(stderr,"Create a 3D slice plot of XYZ/Lab <-> Jab, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	if (diag != NULL)
		fprintf(stderr,"Diagnostic '%s'\n",diag);
	fprintf(stderr,"usage: cam02plot [-options] x y\n");
	fprintf(stderr,"'cam02plot -l -o' shows problem best\n");
	fprintf(stderr," -v level Verbosity level 0 - 2 (default = 1)\n");
	fprintf(stderr," -n       Don't use Helmholtz-Kohlraush flag\n"); 
	fprintf(stderr," -f       Use the reference CIECAM transform\n"); 
	fprintf(stderr," -r res   Resolution (default %d)\n",DEFRES); 
	fprintf(stderr," -l       Lab source cube (default XYZ source cube)\n");
	fprintf(stderr," -i       XYZ/Lab -> Jab (default XYZ/Lab -> Lab)\n");
	fprintf(stderr," -o       Jab -> XYZ -> RGB (defult Lab -> XYZ -> RGB\n");
	fprintf(stderr," -s       XYZ/Lab -> ab scale as RGB\n");
	fprintf(stderr," -x       Cross section plots\n");
	fprintf(stderr,"    x y   Return input color for this coordinate\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	char *tiffname = "cam02plot.tif";
	TIFF *wh = NULL;
	uint16 resunits = RESUNIT_INCH;
	float resx = 75.0, resy = 75.0;
	tdata_t *obuf;
	unsigned char *ob;
	int verb = 0;
	int transl = 0;				/* Translate x,y into input color */
	int tx = 0, ty = 0;			/* coordinate to translate */
	int use_hk = 1;				/* Use Helmholtz-Kohlraush flag */
	int use_ref = 0;			/* Use reference transform rather than working code */
	int lab_plot = 0;			/* Lab <-> Jab plot, else XYZ <-> Jab */
	int to_jab = 0;				/* Convert to Jab, else convert to Lab */
	int from_jab = 0;			/* Convert from Jab, else convert from Lab */
	int to_ss = 0;				/* Convert from XYZ/Lab to abs scale ss */
	int xsect = 0;				/* Cross section plots */
	
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

#ifdef NEVER
	double La[6] = { 10.0, 31.83, 100.0, 318.31, 1000.83, 3183.1 };

	ViewingCondition Vc[4] = {
		vc_average,
		vc_dark,
		vc_dim,
		vc_cut_sheet
	};
#endif /* NEVER */

	int i, j, k, m;
	double xyz[3], Jab[3], rgb[3];
	cam02 *cam1, *cam2;

	int res = DEFRES;	/* Resolution of scan through space */
	int ares;		/* Array of 2d sub-rasters */
	int w, h;		/* Width and height of raster */
	int x, y;
	int sp = 5;			/* Spacing beween resxres sub-images */

	error_program = argv[0];

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')	{	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage("Requested usage");

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				fa = nfa;
				if (na == NULL)
					verb = 2;
				else {
					if (na[0] == '0')
						verb = 0;
					else if (na[0] == '1')
						verb = 1;
					else if (na[0] == '2')
						verb = 2;
					else
						usage("Illegal verbosity level");
				}
			}

			else if (argv[fa][1] == 'r' || argv[fa][1] == 'R') {
				fa = nfa;
				if (na == NULL)
					usage("Need resolution after -r");
				res = atoi(na);
				if (res < 2 || res > 1000)
					usage("Resolution is out of range");
			}
			else if (argv[fa][1] == 'n' || argv[fa][1] == 'N') {
				use_hk = 0;
			}
			else if (argv[fa][1] == 'f' || argv[fa][1] == 'F') {
				use_ref = 1;
			}
			else if (argv[fa][1] == 'l' || argv[fa][1] == 'L') {
				lab_plot = 1;
			}
			else if (argv[fa][1] == 'i' || argv[fa][1] == 'I') {
				to_jab = 1;
			}
			else if (argv[fa][1] == 'o' || argv[fa][1] == 'O') {
				from_jab = 1;
			}
			else if (argv[fa][1] == 's' || argv[fa][1] == 'S') {
				to_ss = 1;
			}
			else if (argv[fa][1] == 'x' || argv[fa][1] == 'X') {
				xsect = 1;
			}
			else 
				usage("Unknown flag");
		} else
			break;
	}

	if ((fa+1) < argc) {
		tx = atoi(argv[fa]);
		ty = atoi(argv[fa+1]);
		transl = 1;
	}


	/* Setup cam to convert to Jab */
	if (use_ref)
		cam1 = new_cam02ref();
	else
		cam1 = new_cam02();
	cam1->set_view(
		cam1,
		vc_average,	/* Enumerated Viewing Condition */
		white[4],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
		1000.0,		/* Adapting/Surround Luminance cd/m^2 */
		0.20,		/* Relative Luminance of Background to reference white */
		0.0,		/* Luminance of white in image - not used */
		0.00,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
		0.00,		/* Glare as a fraction of the ambient (Y range 0.0 .. 1.0) */
		white[4],	/* The Glare color coordinates (typically the Ambient color) */
		use_hk		/* use Helmholtz-Kohlraush flag */ 
	);
	
	/* Setup cam to convert from Jab */
	if (use_ref)
		cam2 = new_cam02ref();
	else
		cam2 = new_cam02();
	cam2->set_view(
		cam2,
		vc_average,	/* Enumerated Viewing Condition */
		white[4],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
		1000.0,		/* Adapting/Surround Luminance cd/m^2 */
		0.20,		/* Relative Luminance of Background to reference white */
		0.0,		/* Luminance of white in image - not used */
		0.00,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
		0.00,		/* Glare as a fraction of the ambient (Y range 0.0 .. 1.0) */
		white[4],	/* The Glare color coordinates (typically the Ambient color) */
		use_hk		/* use Helmholtz-Kohlraush flag */ 
	);
	
	/* Figure out the size of the raster */
	ares = (int)ceil(sqrt((double)res));
	if (verb)
		printf("For res %d, ares = %d\n",res,ares);
	
	w = ares * res + sp * (ares+1);
	h = ares * res + sp * (ares+1);

	if (to_ss) {
		cam1->retss = 1;
	}

	if (transl) {
		int ix,iy,iz;
		double dx,dy,dz;
		double tmp[3], lab[3];

		cam1->trace = 1;
		cam2->trace = 1;

//printf("~1 translate %d %d\n",tx,ty);
		tx -= sp;		/* Border at base */
		ty -= sp;

//printf("~1 offset %d %d\n",tx,ty);
		ix = tx % (res + sp);
		iy = ty % (res + sp);
		iz = (ty / (res + sp)) * ares + (tx / (res + sp));
		
//printf("~1 x y z = %d %d %d\n", ix,iy,iz);

		dx = ix/(res-1.0);
		dy = iy/(res-1.0);
		dz = iz/((ares * ares)-1.0);
		
//printf("~1 X Y Z = %f %f %f\n", dx,dy,dz);

		/* Source range is extended L*a*b* type */
		if (lab_plot) {
			if (xsect) {
				lab[0] = 180.0 * dz - 40.0;
				lab[1] = 300.0 * dy - 150.0;
				lab[2] = 300.0 * dx - 150.0;
			} else {
				lab[0] = 180.0 * dx - 40.0;
				lab[1] = 300.0 * dy - 150.0;
				lab[2] = 300.0 * dz - 150.0;
			}
//printf("~1 Lab = %f %f %f\n", lab[0],lab[1],lab[2]);
			Lab2XYZ(tmp, lab);
			printf("======================================================\n");
			printf("%d,%d -> Lab        %f %f %f\n", tx+sp, ty+sp, lab[0],lab[1],lab[2]);
			printf("      == XYZ        %f %f %f\n", tmp[0],tmp[1],tmp[2]);

		/* Else source range is extended XYZ type */
		} else {
			if (xsect) {
				tmp[0] = 1.4 * dz - 0.2;
				tmp[1] = 1.4 * dy - 0.2;
				tmp[2] = 1.6 * dx - 0.2;
			} else {
				tmp[0] = 1.4 * dx - 0.2;
				tmp[1] = 1.4 * dy - 0.2;
				tmp[2] = 1.6 * dz - 0.2;
			}
			printf("%d,%d -> Input color XYZ %f %f %f\n", tx, ty, tmp[0],tmp[1],tmp[2]);
		}

		if (to_ss) {
			cam1->XYZ_to_cam(cam1, Jab, tmp);
			printf("         XYZ -> ss %f %f %f\n", Jab[0],Jab[1],Jab[2]);

		/* Convert XYZ through cam and back, then to RGB */
		} else if (to_jab) {
			cam1->XYZ_to_cam(cam1, Jab, tmp);
			if (to_ss) {
				printf("         XYZ -> ss %f %f %f\n", Jab[0],Jab[1],Jab[2]);
			} else {
				printf("         XYZ -> ss %f %f %f\n", Jab[0],Jab[1],Jab[2]);
			}

		/* Else convert XYZ to L*a*b* */
		} else {
			XYZ2Lab(Jab, tmp);
			printf("         XYZ -> Lab %f %f %f\n", Jab[0],Jab[1],Jab[2]);
		}

		/* Convert from Jab back to XYZ */
		if (from_jab) {
			cam2->cam_to_XYZ(cam2, rgb, Jab);
			printf("         Jab -> XYZ %f %f %f\n", rgb[0],rgb[1],rgb[2]);

		/* Else convert from Lab back to XYZ */
		} else {
			Lab2XYZ(rgb, Jab);
			printf("         Lab -> XYZ %f %f %f\n", rgb[0],rgb[1],rgb[2]);
		}

		XYZ2Lab(tmp, rgb);
		printf("      == Lab        %f %f %f\n", tmp[0],tmp[1],tmp[2]);

		/* Interpret XYZ as RGB color */
		XYZ2sRGB(rgb, rgb, 1);
		printf("         XYZ -> RGB %f %f %f\n", rgb[0],rgb[1],rgb[2]);

		cam1->trace = 0;
		cam2->trace = 0;

		printf("\n");
		exit(0);
	}

	if (verb)
		printf("Raster width = %d, height = %d\n",w,h);

	/* Setup the tiff file */
	if ((wh = TIFFOpen(tiffname, "w")) == NULL)
		error("Can\'t create TIFF file '%s'!",tiffname);
	
	TIFFSetField(wh, TIFFTAG_IMAGEWIDTH,  w);
	TIFFSetField(wh, TIFFTAG_IMAGELENGTH, h);
	TIFFSetField(wh, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(wh, TIFFTAG_SAMPLESPERPIXEL, 3);
	TIFFSetField(wh, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(wh, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(wh, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	TIFFSetField(wh, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	TIFFSetField(wh, TIFFTAG_RESOLUTIONUNIT, resunits);
	TIFFSetField(wh, TIFFTAG_XRESOLUTION, resx);
	TIFFSetField(wh, TIFFTAG_YRESOLUTION, resy);
	TIFFSetField(wh, TIFFTAG_IMAGEDESCRIPTION, "cam02plot");

	obuf = _TIFFmalloc(TIFFScanlineSize(wh));
	ob = (unsigned char *)obuf;

	y = 0;

	/* Fill sp lines with black */
	for (x = 0, m = 0; m < w; m++) {
		ob[x] = ob[x+1] = ob[x+2] = 0;
		x += 3;
	}
	for (j = 0; j < sp; j++, y++) {
		TIFFWriteScanline(wh, ob, y, 0);
	}

	for (i = 0; i < ares; i++) {				/* Vertical blocks (rows) */
		for (j = 0; j < res; j++, y++) {		/* Vertical in block (y = Y/a*) */
			xyz[1] = j/(res-1.0);
			x = 0;

			/* Fill sp pixels with black */
			for (m = 0; m < sp; m++) {
				ob[x] = ob[x+1] = ob[x+2] = 0;
				x += 3;
			}
			for (k = 0; k < ares; k++) {		/* Horizontal blocks (columns) */
				int zv = i * ares + k;
				if (zv >= res) {
					/* Fill res pixels with black */
					for (m = 0; m < res; m++) {
						ob[x] = ob[x+1] = ob[x+2] = 0;
						x += 3;
					}
				} else {
					xyz[2] = zv/(res-1.0);		/* z = block = Z/b* */
					for (m = 0; m < res; m++) {	/* Horizontal in block (x = X/L*) */
						double tmp[3], xyz2[3];
						xyz[0] = m/(res-1.0);

						if (lab_plot) {
							if (xsect) {
								tmp[0] = 180.0 * xyz[2] - 40.0;
								tmp[1] = 300.0 * xyz[1] - 150.0;
								tmp[2] = 300.0 * xyz[0] - 150.0;
							} else {
								tmp[0] = 180.0 * xyz[0] - 40.0;
								tmp[1] = 300.0 * xyz[1] - 150.0;
								tmp[2] = 300.0 * xyz[2] - 150.0;
							}
							Lab2XYZ(tmp, tmp);

						} else {
							if (xsect) {
								tmp[0] = 1.4 * xyz[2] - 0.2;
								tmp[1] = 1.4 * xyz[1] - 0.2;
								tmp[2] = 1.6 * xyz[0] - 0.2;
							} else {
								tmp[0] = 1.4 * xyz[0] - 0.2;
								tmp[1] = 1.4 * xyz[1] - 0.2;
								tmp[2] = 1.6 * xyz[2] - 0.2;
							}
						}

						/* Convert XYZ through cam and back, then to RGB */
						if (to_jab || to_ss)
							cam1->XYZ_to_cam(cam1, Jab, tmp);
						else
							XYZ2Lab(Jab, tmp);

						if (to_ss) {
							rgb[0] = Jab[0];
							rgb[1] = Jab[1];
							rgb[2] = Jab[2];
						} else {
							if (from_jab)
								cam2->cam_to_XYZ(cam2, xyz2, Jab);
							else
								Lab2XYZ(xyz2, Jab);
	
							XYZ2sRGB(rgb, xyz2, 0);
						}

						/* Fill with pixel value */
						ob[x+0] = (int)(rgb[0] * 255.0 + 0.5);
						ob[x+1] = (int)(rgb[1] * 255.0 + 0.5);
						ob[x+2] = (int)(rgb[2] * 255.0 + 0.5);
						x += 3;
					}
				}
				/* Fill sp pixels with black */
				for (m = 0; m < sp; m++) {
					ob[x] = ob[x+1] = ob[x+2] = 0;
					x += 3;
				}
			}
			TIFFWriteScanline(wh, ob, y, 0);
		}
		/* Fill sp lines with black */
		for (x = m = 0; m < w; m++) {
			ob[x] = ob[x+1] = ob[x+2] = 0;
			x += 3;
		}
		for (j = 0; j < sp; j++, y++) {
			TIFFWriteScanline(wh, ob, y, 0);
		}
	}

	cam1->del(cam1);
	cam2->del(cam2);

	/* Write TIFF file */
	TIFFClose(wh);

	return 0;
}

