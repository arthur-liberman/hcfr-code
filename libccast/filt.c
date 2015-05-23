
/* 
 * Argyll Color Correction System
 * ChromCast up filter test code.
 *
 * Author: Graeme W. Gill
 * Date:   28/8/2014
 *
 * Copyright 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License2.txt file for licencing details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include "copyright.h"
#include "aconfig.h"
#ifndef SALONEINSTLIB
#include "numlib.h"
#else
#include "numsup.h"
#endif
#include "yajl.h"
#include "conv.h"
#include "base64.h"
#include "ccmdns.h"
#include "ccpacket.h"
#include "ccmes.h"
#include "yajl.h"

#define DO_WEIGHTING
#define SUM_CONSTRAINT

//#define VERT 1		/* 1 for vertical */

#ifndef DBL_PI
# define DBL_PI         3.1415926535897932384626433832795
#endif

double lanczos3(double wi, double x) {
	double y;

	x = fabs(1.0 * x/wi);
	if (x >= 3.0)
		return 0.0; 
	if (x < 1e-6)
		return 1.0;
	y = sin(DBL_PI * x)/(DBL_PI * x) * sin(DBL_PI * x/3.0)/(DBL_PI * x/3.0);

	return y;
}

double lanczos2(double wi, double x) {
	double y;

	x = fabs(1.0 * x/wi);
	if (x >= 2.0)
		return 0.0; 
	if (x < 1e-6)
		return 1.0;
	y = sin(DBL_PI * x)/(DBL_PI * x) * sin(DBL_PI * x/2.0)/(DBL_PI * x/2.0);

	return y;
}

double in[2][72] = {
{	// Horizontal 
255, 0, 0, 0, 0, 0, 0, 0, 0,
255, 0, 0, 0, 0, 0, 0, 0, 0,
254, 0, 0, 0, 0, 0, 0, 0, 0,
254, 0, 0, 0, 0, 0, 0, 0, 0,
253, 0, 0, 0, 0, 0, 0, 0, 0,
253, 0, 0, 0, 0, 0, 0, 0, 0,
252, 0, 0, 0, 0, 0, 0, 0, 0,
252, 0, 0, 0, 0, 0, 0, 0, 0
}, { // Vertical slice input target
255, 0, 0, 0, 0, 0, 0, 0, 0,
255, 0, 0, 0, 0, 0, 0, 0, 0,
254, 0, 0, 0, 0, 0, 0, 0, 0,
254, 0, 0, 0, 0, 0, 0, 0, 0,
253, 0, 0, 0, 0, 0, 0, 0, 0,
253, 0, 0, 0, 0, 0, 0, 0, 0,
252, 0, 0, 0, 0, 0, 0, 0, 0,
252, 0, 0, 0, 0, 0, 0, 0, 0
} };
 
#ifdef NEVER
double out[2][] = {
{	// Horizontal
170, 110, 23, 5, 15, 17, 16, 16, 16, 17, 10, 6, 57,
151, 153, 61, 7, 10, 16, 16, 16, 16, 17, 15, 5, 22,
107, 169, 109, 23, 5, 15, 17, 16, 16, 16, 17, 10, 6, 57,
150, 152, 61, 7, 10, 16, 16, 16, 16, 17, 15, 5, 22,
107, 168, 109, 23, 5, 15, 17, 16, 16, 16, 17, 10, 6, 56,
149, 151, 61, 7, 10, 16, 16, 16, 16, 17, 15, 6, 22,
106, 167, 108, 23, 5, 15, 17, 16, 16, 16, 17, 10, 6, 56,
148, 150
},
{ // Vertical slice target output
235, 100, 0, 16, 16, 16, 16, 16, 16, 16, 20, 1, 13,
191, 194, 19, 0, 20, 16, 16, 16, 16, 16, 16, 16, 0, 95,
234, 99, 0, 16, 16, 16, 16, 16, 16, 16, 20, 1, 13,
190, 194, 19, 0, 20, 16, 16, 16, 16, 16, 16, 16, 0, 94,
233, 99, 0, 16, 16, 16, 16, 16, 16, 16, 20, 1, 13,
189, 193, 19, 0, 20, 16, 16, 16, 16, 16, 16, 16, 0, 94,
232, 99, 0, 16, 16, 16, 16, 16, 16, 16, 20, 1, 13,
188, 192
}
};
#else
//#define N1 -9
//#define N2 -8
#define N1 0
#define N2 0

//#define N3 1
#define N4 0

double out[2][96] = {
{	// Horozontal
170, 110, 23, 5, 15, 17, 16, 16, 16, 17, 10, 6, 57,
151, 153, 61, 7, 10, 16, 16, 16, 16, 17, 15, 5, 22,
107, 169, 109, 23, 5, 15, 17, 16, 16, 16, 17, 10, 6, 57,
150, 152, 61, 7, 10, 16, 16, 16, 16, 17, 15, 5, 22,
107, 168, 109, 23, 5, 15, 17, 16, 16, 16, 17, 10, 6, 56,
149, 151, 61, 7, 10, 16, 16, 16, 16, 17, 15, 6, 22,
106, 167, 108, 23, 5, 15, 17, 16, 16, 16, 17, 10, 6, 56,
148, 150
},
{	// Vertical slice target output
235, 100, N2, 16, 16, 16, 16, 16, 16, 16, 20,  1,  13,
191, 194, 19, N4,  20, 16, 16, 16, 16, 16, 16, 16, N1, 95,
234, 99,  N2, 16, 16, 16, 16, 16, 16, 16, 20,  1,  13,
190, 194, 19, N4,  20, 16, 16, 16, 16, 16, 16, 16, N1, 94,
233, 99,  N2, 16, 16, 16, 16, 16, 16, 16, 20,  1,  13,
189, 193, 19, N4,  20, 16, 16, 16, 16, 16, 16, 16, N1, 94,
232, 99,  N2, 16, 16, 16, 16, 16, 16, 16, 20,  1,  13,
188, 192
}
};
#endif

// Computed the input to output filters.
// There will be two phases, depending on whether
// the input pixel has an even or odd address.
// Although in this case they seem like they
// almost interleave, they are actually mirror
// images with offset, so it's easier to keep them
// separate, rather than trying to figure the offset out.
// The index is the output pixel around the closest one
// to the scaled input pixel - ie. floor(in * 1.5 + 0.5)
#define FWIDTH 6
#define NWIDTH (2 * FWIDTH + 1)

double filt_v[2][2][NWIDTH];
double filt_vx[2][2][NWIDTH];			/* max */
double filt_vn[2][2][NWIDTH];			/* min */

double *filt[2][2] = { { &filt_v[0][0][FWIDTH], &filt_v[0][1][FWIDTH] },
                       { &filt_v[1][0][FWIDTH], &filt_v[1][1][FWIDTH] } };

double *filtx[2][2] = { { &filt_vx[0][0][FWIDTH], &filt_vx[0][1][FWIDTH] },
                        { &filt_vx[1][0][FWIDTH], &filt_vx[1][1][FWIDTH] } };

double *filtn[2][2] = { { &filt_vn[0][0][FWIDTH], &filt_vn[0][1][FWIDTH] },
                        { &filt_vn[1][0][FWIDTH], &filt_vn[1][1][FWIDTH] } };

//int fneg[2] = { -5, -4 };		/* Negative index range (inclusive) */
//int fpos[2] = {  5,  3 };		/* Positive index range (inclusive) */
int fneg[2][2] = { { -4, -4 },		/* Negative index range (inclusive) */
                   { -4, -4 } };
int fpos[2][2] = { {  4,  3 },		/* Positive index range (inclusive) */
                   {  4,  3 } };

/* Weightings [horiz/vert][phase] */
double filtw_v[2][2][NWIDTH] = {
	  /* -6,  -5,  -4,  -3,  -2,  -1,     0,     +1,  +2,   +3,  +4,  +5,  +6 */
//	{ {  1.0, 1.0, 1.0, 5.0, 3.0, 42.0,  80.0,  43.0, 3.0, 5.0, 1.0, 1.0, 1.0 },
//	  {  1.0, 1.0, 3.0, 4.0, 19.0, 62.0,  62.0, 21.0,  4.0, 3.0, 1.0, 1.0, 1.0 } },
	{ {  1.0, 1.0, 1.0, 1.0, 1.0, 1.0,  1.0,  1.0, 1.0, 1.0, 1.0, 1.0, 1.0 },
	  {  1.0, 1.0, 1.0, 1.0, 1.0, 1.0,  1.0, 1.0,  1.0, 1.0, 1.0, 1.0, 1.0 } },
	  /* -6,  -5,  -4,  -3,  -2,  -1,     0,     +1,  +2,   +3,  +4,  +5,  +6 */
//	{ {  1.0, 1.0, 1.0, 3.0, 17.0, 36.0, 100.0, 40.0, 15.0, 1.0, 3.0, 1.0, 1.0 },
//	  {  1.0, 1.0, 1.0, 8.0, 2.0,  80.0,  81.0,  2.0,  8.0, 2.0, 1.0, 1.0, 1.0 } }
	{ {  1.0, 1.0, 1.0, 1.0, 1.0, 2.0,  4.0,  2.0, 1.0, 1.0, 1.0, 1.0, 1.0 },
	  {  1.0, 1.0, 1.0, 1.0, 1.0, 3.0,  3.0, 1.0,  1.0, 1.0, 1.0, 1.0, 1.0 } },
};
double *filtw[2][2] = { { &filtw_v[0][0][FWIDTH], &filtw_v[0][1][FWIDTH] },
                        { &filtw_v[1][0][FWIDTH], &filtw_v[1][1][FWIDTH] } };

#define MX 2.0
#define MN -0.5 

// Compute the filter shapes
// vv = 0 for horizontal, 1 for vertical
static void compute(int vv) {
	int ph, ii, i, jj, j;
	double iv, ov; 
	int fcount[2];
	int niv = sizeof(in[vv])/sizeof(double);
	int nov = sizeof(out[vv])/sizeof(double);

	// Clear the filters
	for (ph = 0; ph < 2; ph++) {
		for (j = -FWIDTH; j <= FWIDTH; j++) {
			filt[vv][ph][j] = 0.0;
			filtx[vv][ph][j] = MX;
			filtn[vv][ph][j] = MN;
		}
		fcount[ph] = 0;
	}

	// Discover an input value
	for (ii = 0; ii < niv; ii++) {
		double rgb[3], ycc[3];
		double prop, propx, propn;

		if (in[ii] == 0)
			continue;

		iv = in[vv][ii]; 
		rgb[0] = rgb[1] = rgb[2] = iv;
		ccast2YCbCr(NULL, ycc, rgb);
		iv = ycc[0];

		ph = ii & 1;
		jj = (int)floor(ii * 1.5 + 0.5);

		for (j = -FWIDTH; j <= FWIDTH; j++) {
			int k = jj + j;
			if (k < 0 || k >= nov)
				continue;
			ov = out[vv][k];

			prop = ((ov - 16.0)/219.0)/((iv - 16.0)/219.0);
			propx = ((ov - 16.0)/219.0)/((iv - 0.5 - 16.0)/219.0);
			propn = ((ov - 16.0)/219.0)/((iv + 0.5 - 16.0)/219.0);

			if (propx < propn) {
				double tt = propn;
				propn = propx;
				propx = tt;
			}

//printf("~1 phase %d, off %d, iv %f ov %f, prop %f\n",ph,j,iv,ov,prop);
			filt[vv][ph][j] += prop;
			if (propx < filtx[vv][ph][j])
				filtx[vv][ph][j] = propx;
			if (propn > filtn[vv][ph][j])
				filtn[vv][ph][j] = propn;
		}
		fcount[ph]++;
	}

	// Compute average values
	for (ph = 0; ph < 2; ph++) {
		for (j = -FWIDTH; j <= FWIDTH; j++)
			filt[vv][ph][j] /= (double)fcount[ph];
	}
}

#define FCO2IX(bank, off) (bank ? fpos[vv][0] - fneg[vv][0] + 1 + off - fneg[vv][1] : off - fneg[vv][0])

// Compute the filter shapes using SVD
// vv = 0 for horizontal, 1 for vertical
static void compute2(int vv) {
	int niv = sizeof(in[vv])/sizeof(double);		/* Number of input values */
	int nov = sizeof(out[vv])/sizeof(double);		/* Number of output values */
	int novextra = 0;
	int nfc = fpos[vv][0] - fneg[vv][0] + 1	
	        + fpos[vv][1] - fneg[vv][1] + 1;		/* Number of filter coeficients */
	double **A, *b;
	int oe;					/* Even or odd */
	int j, i;

#ifdef SUM_CONSTRAINT
	novextra = 3;
#endif

	nov += novextra;			/* Extra constraint of sum */

	/* We assume nov > nfc */
	A = dmatrixz(0, nov-1, 0, nfc-1);
	b = dvectorz(0, nov-1);

	/* For each output value */
	for (j = 0; j < (nov-novextra); j++) {
		double ww = 1.0;

#ifdef DO_WEIGHTING
		/* Figure out the weighting */
		for (oe = 0; oe < 2; oe++) {
			int fix;			/* Filter index */

			/* For offset range of filter */
			for (fix = fneg[vv][oe]; fix <= fpos[vv][oe]; fix++) {
				int ocx, icx, ph;

				ocx = j - fix;					/* Output center of filter */
				if (ocx < 0 || ocx >= nov)
					continue;					/* Filter would never get applied */
				if (((2 * ocx) % 3) == 2)
					continue;					/* Would never get applied */
				icx = (int)floor(ocx / 1.5);	/* Input center index for this output */
				if (icx < 0 || icx >= niv)
					continue;					/* Filter would never get applied */
				ph = icx & 1;					/* Phase of filter */
				if (ph != oe)					/* Not a filter that would appear at this ouput */
					continue;
				if (in[vv][icx] >= 200.0)
					ww = filtw[vv][ph][fix];
			}
		}
#endif /* DO_WEIGHTING */

		/* For even and odd filters */
		for (oe = 0; oe < 2; oe++) {
			int fix;			/* Filter index */

			/* For offset range of filter */
			for (fix = fneg[vv][oe]; fix <= fpos[vv][oe]; fix++) {
				double rgb[3], ycc[3];
				int ocx, icx, ph;

				ocx = j - fix;					/* Output center of filter */
				if (ocx < 0 || ocx >= nov)
					continue;					/* Filter would never get applied */
				if (((2 * ocx) % 3) == 2)
					continue;					/* Would never get applied */
				icx = (int)floor(ocx / 1.5);	/* Input center index for this output */
				if (icx < 0 || icx >= niv)
					continue;					/* Filter would never get applied */
				ph = icx & 1;					/* Phase of filter */
				if (ph != oe)					/* Not a filter that would appear at this ouput */
					continue;
//printf("j = %d/%d, k = %d/%d, ix = %d/%d\n",j,nov,oe * NWIDTH + FWIDTH + fix,nfc,ix,niv);
				rgb[0] = rgb[1] = rgb[2] = in[vv][icx];
				ccast2YCbCr(NULL, ycc, rgb);
				A[j][FCO2IX(oe, fix)] += ww * ycc[0];
//printf("A[%d][%d] = %f\n",j,FCO2IX(oe, fix),A[j][FCO2IX(oe, fix)]);
			}
		}
		b[j] = ww * out[vv][j];
//printf("b[%d] = %f\n",j,b[j]);
	}

#ifdef SUM_CONSTRAINT
	/* Add sum constraints */
	/* For 3 repeating output slots */
	for (j = nov-novextra; j < nov; j++) {
		double ww = 10000.0;
		int jj = j - (nov-novextra);

		b[j] = ww;

		/* For even and odd filters */
		for (oe = 0; oe < 2; oe++) {
			int fix;			/* Filter index */

			/* For offset range of filter */
			for (fix = fneg[vv][oe]; fix <= fpos[vv][oe]; fix++) {
				double rgb[3], ycc[3];
				int ocx, ocx2, icx, ph;

				ocx = j - fix;					/* Output center of filter */
				ocx2 = 2 * ocx;
				
				while (ocx2 < 0)
					ocx2 += 3;
				while (ocx2 >= 3)
					ocx2 -= 3;

				if (ocx2 == 2)
					continue;					/* Would never get applied */

				while (ocx < 0)
					ocx += 3;
				while (ocx >= 3)
					ocx -= 3;

				icx = (int)floor(ocx / 1.5);	/* Input center index for this output */
				ph = icx & 1;					/* Phase of filter */
				if (ph != oe)					/* Not a filter that would appear at this ouput */
					continue;
				A[j][FCO2IX(oe, fix)] = ww;
printf("A[%d][%d] = %f\n",j,FCO2IX(oe, fix),A[j][FCO2IX(oe, fix)]);
			}
		}
	}
#endif /* SUM_CONSTRAINT */

	/* Solve the equation A.x = b using SVD */
	/* (The w[] values are thresholded for best accuracy) */
	/* Return non-zero if no solution found */
	if (svdsolve(A, b, nov, nfc))
		error("svdsolve failed");

	/* Print the filter shape */
	/* and copy to the filter */
	printf("SVD computed for %s:\n", vv ? "vertical" : "horizontal");
	for (oe = 0; oe < 2; oe++) {
		int fix;			/* Filter index */
		double sum = 0.0;
	
		printf("Phase %d\n",oe);
//		for (fix = -FWIDTH; fix <= FWIDTH; fix++) {
		for (fix = fneg[vv][oe]; fix <= fpos[vv][oe]; fix++) {
			printf(" %d -> %f\n",fix, b[FCO2IX(oe, fix)]);
			sum += b[FCO2IX(oe, fix)];
			filt[vv][oe][fix] = b[FCO2IX(oe, fix)];
		}
		printf("sum = %f\n",sum);
	}
}

void check(int vv) {
	double *chout;
	int niv = sizeof(in[vv])/sizeof(double);			/* Number of input values */
	int nov = sizeof(out[vv])/sizeof(double);		/* Number of output values */

	int range, i, ii, j;
	double xc, x, iv, tw, w, y;
	double cout, terr = 0.0;

printf("~1 nov = %d\n",nov);
	if ((chout = (double *)malloc(sizeof(double) * nov)) == NULL)
		error("Malloc failed");

	// Clear the output
	for (i = 0; i < nov; i++)
		chout[i] = 0.0;

	// For  all the input value
	for (ii = 0; ii < niv; ii++) {
		int ph, jj;
		double rgb[3], ycc[3];
		double prop;

		iv = in[vv][ii]; 
		rgb[0] = rgb[1] = rgb[2] = iv;
		ccast2YCbCr(NULL, ycc, rgb);
		iv = ycc[0];

		ph = ii & 1;
		jj = (int)floor(ii * 1.5 + 0.5);

		for (j = -FWIDTH; j <= FWIDTH; j++) {
			int k = jj + j;
			if (k < 0 || k >= nov)
				continue;

//if ((jj + j) == 4) printf("[%d] += w %f * iv %f\n",jj + j,  filt[ph][j], iv);
			chout[k] += filt[vv][ph][j] * iv;
		}
	}

	for (i = 0; i < nov; i++) {
		double ov, ee;
		ov = chout[i];
		ov = floor(ov + 0.5);
#ifdef NEVER
		if (ov < 0.0)
			ov = 0.0;
		else if (ov > 255.0)
			ov = 255.0;
#endif
		ee = ov - out[vv][i]; 
		terr += ee * ee;
		printf("out %d = %f should be %f err %f\n",i,chout[i],out[vv][i],ee);
	}

	printf("Total err = %f RMS\n",sqrt(terr));
}

int
main(int argc,
	char *argv[]
) {
	int ph, vv, j;

	double err;
	double cp[2];			/* Initial starting point */
	double s[2];			/* Size of initial search area */

	printf("Hi there\n");

#ifdef NEVER
	compute(1);

	// Print filter shape
	printf("Directly computed:\n");
	for (ph = 0; ph < 2; ph++) {
		printf("Phase %d\n",ph);
		for (j = -FWIDTH; j <= FWIDTH; j++)
			printf(" %d -> min %f, avg %f, max %f\n",j,filtn[1][ph][j],filt[1][ph][j],filtx[1][ph][j]);
	}
#endif

	for (vv = 0; vv < 2; vv++) {
		compute2(vv);
		check(vv);
	}

	/* Output code to stdout */
	for (ph = 0; ph < 2; ph++) {
		fprintf(stderr,"/* Weightings [horiz/vert] */\n");
		fprintf(stderr,"double filt_v_%s[2][%s_WIDTH] = {\n",ph ? "od" : "ev", ph ? "OD" : "EV");

		for (vv = 0; vv < 2; vv++) {
			int fix;			/* Filter index */
		
			fprintf(stderr,"{ ");
			for (fix = fneg[vv][ph]; fix <= fpos[vv][ph]; fix++) {
				if (fix > fneg[vv][ph])
				fprintf(stderr,", ");
				fprintf(stderr,"%f", filt[vv][ph][fix]);
			}
			fprintf(stderr," }%s\n",vv == 0 ? "," : "");
		}
		fprintf(stderr,"};\n\n");
	}

	return 0;
}

