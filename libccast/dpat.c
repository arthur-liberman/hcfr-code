
/* 
 * Argyll Color Correction System
 * ChromCast dither pattern code
 *
 * Author: Graeme W. Gill
 * Date:   11/12/2014
 *
 * Copyright 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License2.txt file for licencing details.
 *
 */

/*
	* Locates minimal list of surrounders
	* Find initial weight for them using minimizer
	Itterates over
	 * Locate cell changes that match desired correction
       with increasing randomness of choosing a poor match
     * Reset to starting pattern when accumulated error over
       best current is exceeded.


	Problem is this is unreliable is locating good matches
 	for some cases.

	Problem is that it doesn't seem to work - ChromeCast doesn't
	decode the way we model it :-(

 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include "../h/aconfig.h"
#include "../h/counters.h"
#ifndef SALONEINSTLIB
#include "numlib.h"
#else
#include "numsup.h"
#endif
#include "conv.h"
#include "base64.h"
#include "yajl.h"
#include "ccmdns.h"
#include "ccpacket.h"
#include "ccmes.h"
#include "ccast.h"

#ifdef STANDALONE_TEST
# define DIAGNOSTICS
#endif

#undef TEST_POINT

/* Even/Odd filter filter index ranges (inclusive) and total size */
#define EV_NEG -4
#define EV_POS  4
#define EV_WIDTH (-(EV_NEG) + 1 + EV_POS)
#define OD_NEG -4
#define OD_POS  3
#define OD_WIDTH (-(OD_NEG) + 1 + OD_POS)

/* Weightings [horiz/vert] */
double filt_v_ev[2][EV_WIDTH] = {
{ -0.003467, -0.048341, 0.028688, 0.418873, 0.704674, 0.427551, 0.032480, -0.050146, -0.003793 },
{ -0.013477, 0.000081, -0.087119, 0.357627, 1.000252, 0.378473, -0.083496, -0.000155, -0.009850 }
};

/* Weightings [horiz/vert] */
double filt_v_od[2][OD_WIDTH] = {
{ -0.026810, -0.045190, 0.186825, 0.614579, 0.623721, 0.206987, -0.040217, -0.026419 },
{ 0.008396, -0.078987, -0.013733, 0.796594, 0.813578, 0.013555, -0.086466, 0.004781 }
};


/* [horiz/vert][phase] */
double *filt[2][2] = { { &filt_v_ev[0][-(EV_NEG)], &filt_v_od[0][-(OD_NEG)] },
                       { &filt_v_ev[1][-(EV_NEG)], &filt_v_od[1][-(OD_NEG)] } };

/* [phase] */
int fneg[2] = { EV_NEG, OD_NEG };
int fpos[2] = { EV_POS, OD_POS };

/* Dither input size */
#ifdef TEST_POINT
# define DISIZE 8
#else
# define DISIZE CCDITHSIZE
#endif
#define DOSIZE ((DISIZE * 3)/2)

#ifdef DIAGNOSTICS
int vv = 0;
#endif

/* Upsample ipat to opat */
/* And return the average RGB value */
static void upsample(double ret[3], double iipat[DISIZE][DISIZE][3]) {
	double ipat[DISIZE][DISIZE][3];
	double tpat[DOSIZE][DISIZE][3]; /* Intermediate pattern - horizontal up-sampled */
	double opat[DOSIZE][DOSIZE][3]; /* Output pattern */
	int x, y;
	int i, j;

//printf("~1 doing conversion\n");
	/* Convert from RGB to YCbCr */
	for (x = 0; x < DISIZE; x++) {
		for (y = 0; y < DISIZE; y++) {
			ccast2YCbCr(NULL, ipat[x][y], iipat[x][y]);
//			ipat[x][y][0] = iipat[x][y][0];
//			ipat[x][y][1] = iipat[x][y][1];
//			ipat[x][y][2] = iipat[x][y][2];
//printf("[%d][%d] %f %f %f -> %f %f %f\n",x,y, iipat[x][y][0], iipat[x][y][1], iipat[x][y][2], ipat[x][y][0], ipat[x][y][1], ipat[x][y][2]);
		}
	}

//printf("~1 doing horiz\n");
	/* Up-sample in horizontal direction */
	for (y = 0; y < DISIZE; y++) {

		/* Zero the intermediate pattern */
		for (i = 0; i < DOSIZE; i++) {
			tpat[i][y][0] = 0.0;
			tpat[i][y][1] = 0.0;
			tpat[i][y][2] = 0.0;
		}

		/* Distribute the input according to the filter */
		for (x = 0; x < DISIZE; x++) {
			int ph, ii, k;

			ph = x & 1;
			ii = (int)floor(x * 1.5 + 0.5);

			/* For all the filter cooeficients */
			for (k = fneg[ph]; k <= fpos[ph]; k++) {
				i = (ii + k);
				while (i < 0)
					i += DOSIZE;
				while (i >= DOSIZE)
					i -= DOSIZE;
				tpat[i][y][0] += filt[0][ph][k] * ipat[x][y][0];
				tpat[i][y][1] += filt[0][ph][k] * ipat[x][y][1];
				tpat[i][y][2] += filt[0][ph][k] * ipat[x][y][2];
			}
		}
	}

//printf("~1 doing vert\n");
	/* Up-sample in vertical direction */
	for (i = 0; i < DOSIZE; i++) {

		/* Zero the output pattern */
		for (j = 0; j < DOSIZE; j++) {
			opat[i][j][0] = 0.0;
			opat[i][j][1] = 0.0;
			opat[i][j][2] = 0.0;
		}

		/* Distribute the input according to the filter */
		for (y = 0; y < DISIZE; y++) {
			int ph, jj, k;

			ph = y & 1;
			jj = (int)floor(y * 1.5 + 0.5);

			/* For all the filter cooeficients */
			for (k = fneg[ph]; k <= fpos[ph]; k++) {
				j = (jj + k);
				while (j < 0)
					j += DOSIZE;
				while (j >= DOSIZE)
					j -= DOSIZE;
				opat[i][j][0] += filt[1][ph][k] * tpat[i][y][0];
				opat[i][j][1] += filt[1][ph][k] * tpat[i][y][1];
				opat[i][j][2] += filt[1][ph][k] * tpat[i][y][2];
			}
		}
	}
//printf("~1 doing conversion\n");

	/* Convert from YCbCr to RGB */
	for (i = 0; i < DOSIZE; i++) {
		for (j = 0; j < DOSIZE; j++) {
			YCbCr2ccast(NULL, opat[i][j], opat[i][j]);
		}
	}
//printf("~1 done\n");

	if (ret != NULL) {
		ret[0] = ret[1] = ret[2] = 0.0;
		for (j = 0; j < DOSIZE; j++) {
			for (i = 0; i < DOSIZE; i++) {
				ret[0] += opat[i][j][0];
				ret[1] += opat[i][j][1];
				ret[2] += opat[i][j][2];
			}
		}
		ret[0] /= (double)(DOSIZE * DOSIZE);
		ret[1] /= (double)(DOSIZE * DOSIZE);
		ret[2] /= (double)(DOSIZE * DOSIZE);
	}

#ifdef DIAGNOSTICS
	if (vv) {
		printf("Result\n");
		for (j = 0; j < DOSIZE; j++) {
			for (i = 0; i < DOSIZE; i++) {
				if (i > 0)
					printf(", ");
				printf("% 5.1f % 5.1f % 5.1f",opat[i][j][0],opat[i][j][1], opat[i][j][2]);
			}
			printf("\n");
		}
	}
#endif
}

/* ------------------------------------------------------------- */

/* Given a quantized RGB target, return a quantized RGB that either exactly */
/* maps to it through YCbCr conversion, or is the closest in the */
/* direction away from the target value, or the closest clipped value */
static void quant_rgb(int n, double out[3], double rgb[3]) {
	double ycc[3], base[3];
	double tmp[3], chval[3];
	double  bdist = 1e6;
//	double brgb[3], borgb[3];
	int ix, k;

//printf("Quant RGB %f %f %f n %d\n", rgb[0], rgb[1], rgb[2], n);

	/* Search current rgb and surround in the same direction */
	/* it is from the target value, for a quantized rgb */
	/* that is in that direction */
	for (k = 0; k < 3; k++)
		base[k] = rgb[k];

	for (ix = 0; ix < 8; ix++) {
		double dist;

		/* Comp trial RGB */
		for (k = 0; k < 3; k++) {
			tmp[k] = base[k]; 
			if (ix & (1 << k)) {
				if (n & (1 << k))				/* Move in direction base point is in */ 
					tmp[k] += 1.0; 
				else
					tmp[k] -= 1.0; 
				if (tmp[k] < 0.0 || tmp[k] > 255.0)
					break;
			}
		}
		if (k < 3)			/* Trial is out of gamut */
			continue;

		/* Quantize it */
		ccast2YCbCr(NULL, ycc, tmp);
		YCbCr2ccast(NULL, chval, ycc);
//printf("Trial RGB   %f %f %f\n", tmp[0], tmp[1], tmp[2]);
//printf("     result %f %f %f\n", chval[0], chval[1], chval[2]);

		/* It's OK if it is eual or greater in the desired */
		/* direction than the input rgb. */
		/* Best would be closest to input. */
		/* Least worst would be what ? */

		dist = 0.0;
		for (k = 0; k < 3; k++) {
			double tt;
			if (n & (1 << k)) {
				tt = chval[k] - base[k];
			} else {
				tt = base[k] - chval[k];
			}
			if (tt >= 0.0)
				dist += 0.1 * tt;
			else
				dist += 2.0 * -tt;
		}
//printf("     dist %f\n", dist);

		/* Pick it if it is at least in the right direction */
		if (dist < bdist) {
			for (k = 0; k < 3; k++) {
				rgb[k] = tmp[k];
				out[k] = chval[k];
			}
			bdist = dist;
		}
    }

#ifdef NEVER
	if (bdist > 0.0
	 && rgb[0] != 0.0 && rgb[0] != 1.0 && rgb[0] != 254.0 && rgb[0] != 255.0
	 && rgb[1] != 0.0 && rgb[1] != 1.0 && rgb[1] != 254.0 && rgb[1] != 255.0
	 && rgb[2] != 0.0 && rgb[2] != 1.0 && rgb[2] != 254.0 && rgb[2] != 255.0) {
		printf("quant_rgb failed with bdist %f\n",bdist);
		printf("        rgb in %f %f %f\n",base[0],base[1],base[2]);
		printf(" returning rgb %f %f %f\n",rgb[0],rgb[1],rgb[2]);
		printf("        resrgb %f %f %f\n",out[0],out[1],out[2]);
	}
#endif /* NEVER */
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct optcntx {
	int di;
	double *val;				/* Target */
	double (*ressur)[8][3];		/* resulting RGB surrounding values */
} optcntx;

static double optfunc(void *fdata, double tp[]) {
	optcntx *cntx = (optcntx *)fdata;
	int i, k;
	double iw, tmp[3];
	double err = 0.0;

	/* Compute interpolated result */
	for (k = 0; k < 3; k++)
		tmp[k] = 0.0;

	iw = 1.0;
	for (i = 0; i < cntx->di; i++) {
		if (tp[i] < 0.0)
			err += 1000.0 * tp[i] * tp[i];
		else if (tp[i] > 1.0)
			err += 1000.0 * (tp[i] - 1.0) * (tp[i] - 1.0);

		for (k = 0; k < 3; k++)
			tmp[k] += tp[i] * (*cntx->ressur)[i][k];
		iw -= tp[i];
	}
	for (k = 0; k < 3; k++)
		tmp[k] += iw * (*cntx->ressur)[i][k];

	/* Compute error */
	for (k = 0; k < 3; k++) {
		double tt = tmp[k] - cntx->val[k];
		err += tt * tt;
	}
//printf("Returning %f from %f %f\n",err,tp[0],tp[1]);
	return err;
}

/* Compute pattern */
/* return the delta to the target */
double get_ccast_dith(double ipat[DISIZE][DISIZE][3], double val[3]) {
	double itpat[DISIZE][DISIZE][3], tpat[DISIZE][DISIZE][3];
	double irtpat[DISIZE][DISIZE][3], rtpat[DISIZE][DISIZE][3];	/* resulting for tpat */
	double berr = 0.0;
	int n, k;
	int x, y;
	int i, j;
	int ii;
	struct {
    	int x, y;
	} order[16] = {
// Dispersed:
	    { 3, 1 },{ 1, 3 },{ 1, 1 },{ 3, 0 },{ 3, 3 },{ 1, 2 },{ 3, 2 },{ 2, 0 },
		{ 1, 0 },{ 0, 3 },{ 0, 1 },{ 2, 1 },{ 2, 2 },{ 0, 0 },{ 0, 2 },{ 2, 3 }
// Clustered:
//	    { 0, 0 },{ 0, 1 },{ 1, 1 },{ 1, 0 },{ 2, 0 },{ 3, 0 },{ 3, 1 },{ 2, 1 },
//		{ 2, 2 },{ 3, 2 },{ 3, 3 },{ 2, 3 },{ 1, 3 },{ 1, 2 },{ 0, 2 },{ 0, 3 }
	};
	int cix = 0;
	int nsur = 8, ncomb = 4;
	double sur[8][3];			/* RGB surrounding values to use */
	double ressur[8][3];		/* resulting RGB surrounding values */
	int  bcc[8];				/* Best combination */
	double bw[8];				/* Best weight */
	int biw[8];					/* Best integer weight/16 */
	double dval[3];				/* Dithered value */
	double err[3], werr;
	double errxs;
	/* Tuning params */
	int nitters = 300;			/* No itters */
	int rand_count = 150;
	double rand_start = 4.5;	/* Ramp with itters */
	double rand_end = 0.2;
	double rand_pow = 1.5;
	double mxerrxs = 2.5;		/* accumulated error reset threshold */
	unsigned int randv = 0x1234;

	/* 32 bit pseudo random sequencer based on XOR feedback */
	/* generates number between 1 and 4294967295 */
#define PSRAND32(S) (((S) & 0x80000000) ? (((S) << 1) ^ 0xa398655d) : ((S) << 1))

	/* Locate the 8 surrounding RGB verticies */
	for (n = 0; n < 8; n++) {
		for (k = 0; k < 3; k++) {
			if (n & (1 << k))
				sur[n][k] = ceil(val[k]);
			else
				sur[n][k] = floor(val[k]);
		}

//printf("Input sur %d: %f %f %f\n",n,sur[n][0], sur[n][1], sur[n][2], sur[n][3]);
		/* Figure out what RGB values to use to surround the point, */
		/* and what actual RGB values to expect from using them. */
		quant_rgb(n, ressur[n], sur[n]);
//printf("Quant sur %d: %f %f %f\n",n,sur[n][0], sur[n][1], sur[n][2], sur[n][3]);
//printf("   ressur %d: %f %f %f\n",n,ressur[n][0], ressur[n][1], ressur[n][2], ressur[n][3]);
//		printf("\n");
	}

	/* Reduce this to unique surrounders */
	for (nsur = 0, i = 0; i < 8; i++) {

		/* Check if the i'th entry is already in the list */
		for (j = 0; j < nsur; j++) {
			for (k = 0; k < 3; k++) {
				if (ressur[i][k] != ressur[j][k])
					break;
			}
			if (k < 3)
				continue;	/* Unique */
			break;			/* Duplicate */
		}
		if (j < nsur)		/* Duplicate */
			continue;
		
		/* Copy i'th to nsur */
		for (k = 0; k < 3; k++) {
			sur[nsur][k]    = sur[i][k];
			ressur[nsur][k] = ressur[i][k];
		}
		nsur++;
	}

#ifdef DIAGNOSTICS
	if (vv) {
		printf("There are %d unique surrounders:\n",nsur);
		for (n = 0; n < nsur; n++) {
			printf("sur %f %f %f\n",ressur[n][0], ressur[n][1], ressur[n][2], ressur[n][3]);
		}
	}
#endif

	/* Use an optimzer to set the initial values using all the unique */
	/* surrounders. */
	{
		double s[8]; 
		optcntx cntx;

		ncomb = nsur;
		for (n = 0; n < nsur; n++) {
			bcc[n] = n;
			bw[n] = 1.0/nsur;
			s[n] = 0.1;
		}

		cntx.di = nsur-1;
		cntx.val = val;
		cntx.ressur = &ressur;

		powell(NULL, cntx.di, bw, s, 1e-4, 1000, optfunc, &cntx, NULL, NULL); 

		/* Compute baricentric values */
		bw[nsur-1] = 0.0;
		for (n = 0; n < (nsur-1); n++) {
			if (bw[n] < 0.0)
				bw[n] = 0.0;
			else if (bw[n] > 1.0)
				bw[n] = 1.0;
			bw[nsur-1] += bw[n];
		}
		if (bw[nsur-1] > 1.0) {		/* They summed to over 1.0 */
			for (n = 0; n < (nsur-1); n++)
				bw[n] *= 1.0/bw[nsur-1];	/* Scale them down */
			bw[nsur-1] = 1.0;
		}
		bw[nsur-1] = 1.0 - bw[nsur-1];		/* Remainder */
	}

	/* Check the result */
#ifdef DIAGNOSTICS
	if (vv) {
		double tmp[3], err;

		/* Compute interpolated result */
		for (k = 0; k < 3; k++)
			tmp[k] = 0.0;
		for (n = 0; n < ncomb; n++) {
			for (k = 0; k < 3; k++)
				tmp[k] += bw[n] * ressur[bcc[n]][k];
		}
		/* Compute error */
		err = 0.0;
		for (k = 0; k < 3; k++) {
			tmp[k] -= val[k];
			err += tmp[k] * tmp[k];
		}
		err = sqrt(err);
		for (n = 0; n < ncomb; n++)
			printf("Comb %d weight %f rgb %f %f %f\n",bcc[n],bw[n],ressur[bcc[n]][0],ressur[bcc[n]][1],ressur[bcc[n]][2]);
		printf("Error %f %f %f rms %f\n",tmp[0], tmp[1], tmp[2], err);
		printf("\n");
	}
#endif

	/* Compute the number of pixels for each surounder value */
	{
		int sw[8], rem;

		/* Sort the weightings from smallest to largest */
		for (n = 0; n < 8; n++)
			sw[n] = n;

		for (i = 0; i < (ncomb-1); i++) {
			for (j = i+1; j < ncomb; j++) {
				if (bw[sw[j]] < bw[sw[i]]) {
					int tt = sw[i];		/* Swap them */
					sw[i] = sw[j];
					sw[j] = tt;
				} 
			}
		}

		/* Compute the nearest integer weighting out of 16 */
		rem = 16;
		for (i = 0; i < (ncomb-1) && rem > 0; i++) {
			n = sw[i];
			biw[n] = (int)(16.0 * bw[n] + 0.5);
			rem -= biw[n];
			if (rem <= 0)
				rem = 0;
		}
		for (; i < ncomb; i++) {
			n = sw[i];
			biw[n] = rem;
		}
#ifdef DIAGNOSTICS
		if (vv) {
			for (n = 0; n < ncomb; n++)
				printf("Comb %d iweight %i rgb %f %f %f\n",bcc[n],biw[n],ressur[bcc[n]][0],ressur[bcc[n]][1],ressur[bcc[n]][2]);
		}
#endif
	}

	/* Set the initial pattern according to the integer weighting */
	for (cix = 0, n = 0; n < ncomb; n++) {
		for (i = 0; i < biw[n]; i++) {
			x = order[cix].x;
			y = order[cix].y;
			cix++;
			for (k = 0; k < 3; k++) {
				tpat[x][y][k] = itpat[x][y][k]  = sur[bcc[n]][k];
				rtpat[x][y][k] = irtpat[x][y][k] = ressur[bcc[n]][k];
			}
		}
	}

#ifdef DIAGNOSTICS
	/* Check initial pattern error */
	if (vv) {
		printf("Input pat:\n");
		for (x = 0; x < DISIZE; x++) {
			for (y = 0; y < DISIZE; y++) {
				if (y > 0)
					printf(", ");
				printf("%3.0f %3.0f %3.0f",rtpat[x][y][0], rtpat[x][y][1], rtpat[x][y][2]);
			}
			printf("\n");
		}
	}
#endif

	upsample(dval, rtpat);

	werr = 0.0;
	for (k = 0; k < 3; k++) {
		err[k] = dval[k] - val[k]; 
		if (fabs(err[k]) > werr)
			werr = fabs(err[k]);
	}

#ifdef DIAGNOSTICS
	if (vv) {
		printf("Target %f %f %f -> %f %f %f\n", val[0], val[1], val[2], dval[0], dval[1], dval[2]);
		printf("Error %f %f %f werr %f\n", err[0], err[1], err[2], werr);
	}
#endif

	berr = werr;
	for (x = 0; x < DISIZE; x++) {
		for (y = 0; y < DISIZE; y++) {
			for (k = 0; k < 3; k++)
				ipat[x][y][k] = tpat[x][y][k];
		}
	}

	/* Improve fit if needed */
	/* This is a bit stocastic */
	errxs = 0.0;
	for (ii = 0; ii < nitters; ii++) {		/* Until we give up */
		double corr[3];		/* Correction direction needed */
		double bdot;
		int bx, by;

//		double  mm;
//		double cell[3];		/* Cell being modified value */
//		double ccell[3];	/* Corrected cell */
//		double pcell[3];		/* Proposed new cell value */

		for (k = 0; k < 3; k++)
			corr[k] = val[k] - dval[k];
#ifdef DIAGNOSTICS
		if (vv)
			printf("corr needed %f %f %f\n", corr[0], corr[1], corr[2]);
#endif

		/* Scale it and limit it */
		for (k = 0; k < 3; k++) {
			double dd = 16.0 * corr[k];
			if (dd >= 1.0)
				dd = 1.0;
			else if (dd <= -1.0)
				dd = -1.0;
			else
				dd = 0.0;
			corr[k] = dd;
		}

		if (corr[0] == 0.0 && corr[1] == 0 && corr[2] == 0.0) {
#ifdef DIAGNOSTICS
			if (vv)
				printf("No correction possible - done\n");
#endif
			break;
		}
			
#ifdef DIAGNOSTICS
		if (vv)
			printf("scaled corr %f %f %f\n", corr[0], corr[1], corr[2]);
#endif

		/* Search dither cell and surrounder for a combination */
		/* that is closest to the change we want to make. */
		bdot = 1e6;
		bx = by = n = 0;
		for (x = 0; x < DISIZE; x++) {
			double rlevel = rand_start + (rand_end - rand_start)
			                * pow((ii % rand_count)/rand_count, rand_pow);

			for (y = 0; y < DISIZE; y++) {
				for (i = 0; i < ncomb; i++) {
					double dot = 0.0;
					for (k = 0; k < 3; k++)
						dot += (ressur[bcc[i]][k] - rtpat[x][y][k]) * corr[k];

					/* Ramp the randomness up */
//					dot += d_rand(0.0, 0.1 + (2.5-0.1) * ii/nitters);
//					dot += d_rand(-rlevel, rlevel);
					/* use a deterministic random element, so that */
					/* the dither patterns are repeatable. */
					randv = PSRAND32(randv);
					dot += rlevel * 2.0 * ((randv - 1)/4294967294.0 - 0.5);

					if (dot <= 0.0)
						dot = 1e7;
					else {
						dot = (dot - 1.0) * (dot - 1.0);
					}
//printf("dot %f from sur %f %f %f to pat %f %f %f\n", dot, ressur[bcc[i]][0], ressur[bcc[i]][1], ressur[bcc[i]][2], rtpat[x][y][0], rtpat[x][y][1], rtpat[x][y][2]);

					if (dot < bdot) {
						bdot = dot;
						bx = x;
						by = y;
						n = i;
					}
				}
			}
		}

#ifdef DIAGNOSTICS
		if (vv) {
			printf("Changing cell [%d][%d] %f %f %f with dot %f\n",bx,by,rtpat[bx][by][0],rtpat[bx][by][1],rtpat[bx][by][2],bdot);
			printf("           to sur %d: %f %f %f\n",n, ressur[bcc[n]][0], ressur[bcc[n]][1], ressur[bcc[n]][2]);
		}
#endif

		/* Substitute the best correction for this cell */
		for (k = 0; k < 3; k++) {
			tpat[bx][by][k] = sur[bcc[n]][k];
			rtpat[bx][by][k] = ressur[bcc[n]][k];
		}

#ifdef DIAGNOSTICS
		if (vv) {
			printf("Input pat:\n");
			for (x = 0; x < DISIZE; x++) {
				for (y = 0; y < DISIZE; y++) {
					if (y > 0)
						printf(", ");
					printf("%3.0f %3.0f %3.0f",rtpat[x][y][0], rtpat[x][y][1], rtpat[x][y][2]);
				}
				printf("\n");
			}
		}
#endif

		upsample(dval, rtpat);
	
		werr = 0.0;
		for (k = 0; k < 3; k++) {
			err[k] = dval[k] - val[k]; 
			if (fabs(err[k]) > werr)
				werr = fabs(err[k]);
		}

		if (werr > berr) {
			errxs += werr - berr;
		}

		/* New best */
		if (werr < berr) {
			berr = werr;
			errxs = 0.0;
			for (x = 0; x < DISIZE; x++) {
				for (y = 0; y < DISIZE; y++) {
					for (k = 0; k < 3; k++) {
						ipat[x][y][k] = tpat[x][y][k];

						itpat[x][y][k] = tpat[x][y][k];
						irtpat[x][y][k] = rtpat[x][y][k];
					}
				}
			}
		}

#ifdef DIAGNOSTICS
		if (vv) {
			printf("Target %f %f %f -> %f %f %f\n", val[0], val[1], val[2], dval[0], dval[1], dval[2]);
			printf("Error %f %f %f werr %f\n", err[0], err[1], err[2], werr);
		}
#endif

		if (berr < 0.11) {
#ifdef DIAGNOSTICS
			if (vv)
				printf("best error %f < 0.11 - give up\n",berr);
#endif
			break;
		}

		/* If we're not making progress, reset to the last best */
		if (errxs > mxerrxs) {
#ifdef DIAGNOSTICS
			if (vv)
				printf("Restarting at ii %d \n",ii); 
#endif
			errxs = 0.0;
			for (x = 0; x < DISIZE; x++) {
				for (y = 0; y < DISIZE; y++) {
					for (k = 0; k < 3; k++) {
						tpat[x][y][k] = itpat[x][y][k];
						rtpat[x][y][k] = irtpat[x][y][k];
					}
				}
			}
		}
	}

#ifdef DIAGNOSTICS
	if (vv) {
		printf("Returning best error %f pat:\n",berr);
		for (y = 0; y < DISIZE; y++) {
			for (x = 0; x < DISIZE; x++) {
				if (x > 0)
					printf(",  ");
				printf("%3.0f %3.0f %3.0f",ipat[x][y][0], ipat[x][y][1], ipat[x][y][2]);
			}
			printf("\n");
		}
	}
#endif

	return berr;
}

/* ====================================================================================== */

#ifdef STANDALONE_TEST

int
main(int argc,
	char *argv[]
) {
	double val[3], out[3], err;
	double ipat[DISIZE][DISIZE][3];
	double aerr, acount, xerr;
	int x, y;
	int i, j;
	int k, nn;

	printf("Hi there\n");

	rand32(time(NULL));

#ifdef TEST_POINT
	for (x = 0; x < DISIZE; x++) {
		for (y = 0; y < DISIZE; y++) {
			ipat[x][y][0] = 0.0;
			ipat[x][y][1] = 0.0;
			ipat[x][y][2] = 0.0;
		}
	}

	ipat[5][4][0] = 255.0;
	ipat[5][4][1] = 255.0;
	ipat[5][4][2] = 255.0;

	upsample(NULL, ipat);

#else

#ifdef NEVER

//	val[0] = 201.500000;
//	val[1] = 115.533403;
//	val[2] = 76.300000;

//	val[0] = 255.000000;
//	val[1] = 115.533403;
//	val[2] = 255.000000;

//	val[0] = 221.689875;
//	val[1] = 29.593255;
//	val[2] = 140.820878;

//	val[0] = 212.377797;
//	val[1] = 228.338903;
//	val[2] = 70.153296;

//	val[0] = 231.554511;
//	val[1] = 0.000000;
//	val[2] = 51.958048;

//	val[0] = 255.000000;
//	val[1] = 144.768052;
//	val[2] = 179.737212;

//	val[0] = 194.854956;
//	val[1] = 41.901887;
//	val[2] = 20.434793;

//	val[0] = 250.100121; 
//	val[1] = 83.484217;
//	val[2] = 42.867603;

//	val[0] = 255.000000;
//	val[1] = 255.000000;
//	val[2] = 228.534759;

	// 1.71 -> 1.58, rand 2.2
//	val[0] = 255.000000;
//	val[1] = 176.894769;
//	val[2] = 8.932806;

	// 1.54 -> 0.762592, rand 0.3 
//	val[0] = 216.873703;
//	val[1] = 250.908094;

	// 1.05
//	val[0] = 167.284458;
//	val[1] = 248.945210;
//	val[2] = 199.023452;

	// 1.07
//	val[0] = 211.045184;
//	val[1] = 27.825141;
//	val[2] = 63.883148;

	// 1.928
//	val[0] = 255.000000;
//	val[1] = 0.439284;
//	val[2] = 210.928135;

	// 1.278
//	val[0] = 218.693614;
//	val[1] = 222.890101;
//	val[2] = 174.779727;

	// 1.334501
//	val[0] = 253.931573;
//	val[1] = 230.278945;
//	val[2] = 185.677389;

//	printf("In RGB %f %f %f\n",val[0],val[1],val[2]);
//	ccast2YCbCr(NULL, out, val);
//	printf("YCbCr %f %f %f\n",out[0],out[1],out[2]);
//	YCbCr2ccast(NULL, out, out);
//	printf("RGB %f %f %f\n",out[0],out[1],out[2]);
	

	vv = 1;
	err = get_ccast_dith(ipat, val);

	printf("Got pat with err %f\n",err);

#else
	aerr = 0.0;
	acount = 0.0;
	xerr = 0.0;
	for (nn = 0; nn < 200000; nn++) {

		for (k = 0; k < 3; k++) {
			val[k] = d_rand(-5.0, 255.0 + 5.0);
			if (val[k] < 0.0)
				val[k] = 0.0;
			else if (val[k] > 255.0)
				val[k] = 255.0;
		}

		err = get_ccast_dith(ipat, val);

		if (err >= 1.5 || 
		 (  err >= 1.0
		 && val[0] != 0.0 && val[0] != 255.0
		 && val[1] != 0.0 && val[1] != 255.0
		 && val[2] != 0.0 && val[2] != 255.0)) {
			printf("Target RGB %f %f %f, err %f\n", val[0], val[1], val[2],err);
//			vv = 1;
//			comput_pat(ipat, val);
//			break;
		}

		aerr += err;
		acount++;
		if (err > xerr)
			xerr = err;
	}

	aerr /= acount;
	printf("After %d trials, aerr = %f, maxerr = %f\n",nn,aerr,xerr); 
#endif
#endif

	return 0;
}

#endif /* STANDALONE_TEST */
