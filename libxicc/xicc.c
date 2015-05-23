
/* 
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    2/7/00
 * Version: 1.00
 *
 * Copyright 2000, 2001, 2014 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Based on the old iccXfm class.
 */

/*
 * This module expands the basic icclib functionality,
 * providing more functionality in exercising.
 * The implementation for the three different types
 * of profile representation, are in their own source files.
 */

/*
 * TTBD:
 *       Some of the error handling is crude. Shouldn't use
 *       error(), should return status.
 *
 */

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
#include "counters.h"
#include "plot.h"
#include "../h/sort.h"
#include "xicc.h"		/* definitions for this library */

#define USE_CAM			/* Use CIECAM02 for clipping and gamut mapping, else use Lab */

#undef DEBUG			/* Plot 1d Luts */

#ifdef DEBUG
#include "plot.h"
#endif

#define MAX_INVSOLN 4

static void xicc_del(xicc *p);
icxLuBase * xicc_get_luobj(xicc *p, int flags, icmLookupFunc func, icRenderingIntent intent,
                           icColorSpaceSignature pcsor, icmLookupOrder order,
                           icxViewCond *vc, icxInk *ink);
static icxLuBase *xicc_set_luobj(xicc *p, icmLookupFunc func, icRenderingIntent intent,
                            icmLookupOrder order, int flags, int no, int nobw, cow *points,
							icxMatrixModel *skm,
                            double dispLuminance, double wpscale,
//							double *bpo,
                            double smooth, double avgdev,
							double demph, icxViewCond *vc, icxInk *ink, xcal *cal, int quality);
static void icxLutSpaces(icxLuBase *p, icColorSpaceSignature *ins, int *inn,
                         icColorSpaceSignature *outs, int *outn,
                         icColorSpaceSignature *pcs);
static void icxLuSpaces(icxLuBase *p, icColorSpaceSignature *ins, int *inn,
                        icColorSpaceSignature *outs, int *outn, 
                        icmLuAlgType *alg, icRenderingIntent *intt,
                        icmLookupFunc *fnc, icColorSpaceSignature *pcs);
static void icxLu_get_native_ranges (icxLuBase *p,
                              double *inmin, double *inmax, double *outmin, double *outmax);
static void icxLu_get_ranges (icxLuBase *p,
                              double *inmin, double *inmax, double *outmin, double *outmax);
static void icxLuEfv_wh_bk_points(icxLuBase *p, double *wht, double *blk, double *kblk);
int xicc_get_viewcond(xicc *p, icxViewCond *vc);

/* The different profile types are in their own source filesm */
/* and are included to keep their functions private. (static) */
#include "xmono.c"
#include "xmatrix.c"
#include "xlut.c"		/* New xfit3 in & out optimising based profiles */
//#include "xlut1.c"	/* Old xfit1 device curve based profiles */

#ifdef NT		/* You'd think there might be some standards.... */
# ifndef __BORLANDC__
#  define stricmp _stricmp
# endif
#else
# define stricmp strcasecmp
#endif
/* Utilities */

/* Return a string description of the given enumeration value */
const char *icx2str(icmEnumType etype, int enumval) {

	if (etype == icmColorSpaceSignature) {
		if (((icColorSpaceSignature)enumval) == icxSigJabData)
			return "Jab";
		else if (((icColorSpaceSignature)enumval) == icxSigJChData)
			return "JCh";
		else if (((icColorSpaceSignature)enumval) == icxSigLChData)
			return "LCh";
	} else if (etype == icmRenderingIntent) {
		if (((icRenderingIntent)enumval) == icxAppearance)
			return "icxAppearance";
		else if (((icRenderingIntent)enumval) == icxAbsAppearance)
			return "icxAbsAppearance";
		else if (((icRenderingIntent)enumval) == icxPerceptualAppearance)
			return "icxPerceptualAppearance";
		else if (((icRenderingIntent)enumval) == icxAbsPerceptualAppearance)
			return "icxAbsPerceptualAppearance";
		else if (((icRenderingIntent)enumval) == icxSaturationAppearance)
			return "icxSaturationAppearance";
		else if (((icRenderingIntent)enumval) == icxAbsSaturationAppearance)
			return "icxAbsSaturationAppearance";
	}
	return icm2str(etype, enumval);
}

/* Common xicc stuff */

/* Return information about the native lut in/out colorspaces. */
/* Any pointer may be NULL if value is not to be returned */
static void
icxLutSpaces(
	icxLuBase *p,					/* This */
	icColorSpaceSignature *ins,		/* Return input color space */
	int *inn,						/* Return number of input components */
	icColorSpaceSignature *outs,	/* Return output color space */
	int *outn,						/* Return number of output components */
	icColorSpaceSignature *pcs		/* Return PCS color space */
) {
	p->plu->lutspaces(p->plu, ins, inn, outs, outn, pcs);
}

/* Return information about the overall lookup in/out colorspaces, */
/* including allowance for any PCS override. */
/* Any pointer may be NULL if value is not to be returned */
static void
icxLuSpaces(
	icxLuBase *p,					/* This */
	icColorSpaceSignature *ins,		/* Return input color space */
	int *inn,						/* Return number of input components */
	icColorSpaceSignature *outs,	/* Return output color space */
	int *outn,						/* Return number of output components */
	icmLuAlgType *alg,				/* Return type of lookup algorithm used */
    icRenderingIntent *intt,		/* Return the intent implemented */
    icmLookupFunc *fnc,				/* Return the profile function being implemented */
	icColorSpaceSignature *pcs		/* Return the effective PCS */
) {
    icmLookupFunc function;
	icColorSpaceSignature npcs;		/* Native PCS */

	p->plu->spaces(p->plu, NULL, inn, NULL, outn, alg, NULL, &function, &npcs, NULL);

	if (intt != NULL)
		*intt = p->intent;

	if (fnc != NULL)
		*fnc = function;

	if (ins != NULL)
		*ins = p->ins;

	if (outs != NULL)
		*outs = p->outs;

	if (pcs != NULL)
		*pcs = p->pcs;
}

/* Return the native (internaly visible) colorspace value ranges */
static void
icxLu_get_native_ranges (
icxLuBase *p,
double *inmin, double *inmax,		/* Return maximum range of inspace values */
double *outmin, double *outmax		/* Return maximum range of outspace values */
) {
	int i;
	if (inmin != NULL) {
		for (i = 0; i < p->inputChan; i++)
			inmin[i] = p->ninmin[i];
	}
	if (inmax != NULL) {
		for (i = 0; i < p->inputChan; i++)
			inmax[i] = p->ninmax[i];
	}
	if (outmin != NULL) {
		for (i = 0; i < p->outputChan; i++)
			outmin[i] = p->noutmin[i];
	}
	if (outmax != NULL) {
		for (i = 0; i < p->outputChan; i++)
			outmax[i] = p->noutmax[i];
	}
}

/* Return the effective (externaly visible) colorspace value ranges */
static void
icxLu_get_ranges (
icxLuBase *p,
double *inmin, double *inmax,		/* Return maximum range of inspace values */
double *outmin, double *outmax		/* Return maximum range of outspace values */
) {
	int i;
	if (inmin != NULL) {
		for (i = 0; i < p->inputChan; i++)
			inmin[i] = p->inmin[i];
	}
	if (inmax != NULL) {
		for (i = 0; i < p->inputChan; i++)
			inmax[i] = p->inmax[i];
	}
	if (outmin != NULL) {
		for (i = 0; i < p->outputChan; i++)
			outmin[i] = p->outmin[i];
	}
	if (outmax != NULL) {
		for (i = 0; i < p->outputChan; i++)
			outmax[i] = p->outmax[i];
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Routine to figure out a suitable black point for CMYK */

/* Structure to hold optimisation information */
typedef struct {
	icmLuBase *p;
	int kch;				/* K channel, -1 if none */
	double tlimit, klimit;	/* Ink limit values */
	int inn;				/* Number of input channels */
	icColorSpaceSignature outs;		/* Output space */
	double p1[3];			/* white pivot point in abs Lab */
	double p2[3];			/* Point on vector towards black in abs Lab */
	double toll;			/* Tollerance of black direction */
} bpfind;

/* Optimise device values to minimise L, while remaining */
/* within the ink limit, and staying in line between p1 (white) and p2 (black dir) */
static double bpfindfunc(void *adata, double pv[]) {
	bpfind *b = (bpfind *)adata;
	double rv = 0.0;
	double Lab[3];
	double lr, ta, tb, terr;	/* L ratio, target a, target b, target error */
	double ovr = 0.0;
	int e;

	/* Compute amount outside total limit */
	if (b->tlimit >= 0.0) {
		double sum;
		for (sum = 0.0, e = 0; e < b->inn; e++)
			sum += pv[e];
		if (sum > b->tlimit) {
			ovr = sum - b->tlimit;
#ifdef DEBUG
	printf("~1 total ink ovr = %f\n",ovr);
#endif
		}
	}

	/* Compute amount outside black limit */
	if (b->klimit >= 0.0 && b->kch >= 0) {
		double kval = pv[b->kch] - b->klimit;
		if (kval > ovr) {
			ovr = kval;
#ifdef DEBUG
	printf("~1 black ink ovr = %f\n",ovr);
#endif
		}
	}
	/* Compute amount outside device value limits 0.0 - 1.0 */
	{
		double dval;
		for (dval = -1.0, e = 0; e < b->inn; e++) {
			if (pv[e] < 0.0) {
				if (-pv[e] > dval)
					dval = -pv[e];
			} else if (pv[e] > 1.0) {
				if ((pv[e] - 1.0) > dval)
					dval = pv[e] - 1.0;
			}
		}
		if (dval > ovr)
			ovr = dval;
	}

	/* Compute the Lab value: */
	b->p->lookup(b->p, Lab, pv);
	if (b->outs == icSigXYZData)
		icmXYZ2Lab(&icmD50, Lab, Lab);

#ifdef DEBUG
	printf("~1 p1 =  %f %f %f, p2 = %f %f %f\n",b->p1[0],b->p1[1],b->p1[2],b->p2[0],b->p2[1],b->p2[2]);
	printf("~1 device value %f %f %f %f, Lab = %f %f %f\n",pv[0],pv[1],pv[2],pv[3],Lab[0],Lab[1],Lab[2]);
#endif

	/* Primary aim is to minimise L value */
	rv = Lab[0];

	/* See how out of line from p1 to p2 we are */
	lr = (Lab[0] - b->p1[0])/(b->p2[0] - b->p1[0]);		/* Distance towards p2 from p1 */
	ta = lr * (b->p2[1] - b->p1[1]) + b->p1[1];			/* Target a value */
	tb = lr * (b->p2[2] - b->p1[2]) + b->p1[2];			/* Target b value */

	terr = (ta - Lab[1]) * (ta - Lab[1])
	     + (tb - Lab[2]) * (tb - Lab[2]);
	
	if (terr < b->toll)		/* Tollerance error doesn't count until it's over tollerance */
		terr = 0.0;
	
#ifdef DEBUG
	printf("~1 target error %f\n",terr);
#endif
	rv += XICC_BLACK_FIND_ABERR_WEIGHT * terr;	/* Make ab match more important than min. L */

#ifdef DEBUG
	printf("~1 out of range error %f\n",ovr);
#endif
	rv += 200 * ovr;

#ifdef DEBUG
	printf("~1 black find tc ret %f\n",rv);
#endif
	return rv;
}

/* Try and compute a real black point in XYZ given an iccLu, */
/* and also return the K only black or the normal black if the device doesn't have K */
/* black[] will be unchanged if black cannot be computed. */
/* Note that the black point will be in the space of the Lu */
/* converted to XYZ, so will have the Lu's intent etc. */
/* (Note that this is duplicated in xlut.c set_icxLuLut() !!!) */
static void icxLu_comp_bk_point(
icxLuBase *x,
int gblk,				/* If nz, compute black if possible. */
double *white,			/* XYZ Input, used for computing black */
double *black,			/* XYZ Input & Output. Set if gblk NZ and can be computed */
double *kblack			/* XYZ Output. Looked up if possible or set to black[] otherwise */
) {
	icmLuBase *p = x->plu;
	icmLuBase *op = p;			/* Original icmLu, in case we replace p */
	icc *icco = p->icp;
	icmHeader *h = icco->header;
	icColorSpaceSignature ins, outs;
	int inn, outn;
	icmLuAlgType alg;
	icRenderingIntent intt;
	icmLookupFunc fnc;
	icmLookupOrder ord;
	int kch = -1;
	double dblack[MAX_CHAN];	/* device black value */
	int e;

#ifdef DEBUG
	printf("~1 icxLu_comp_bk_point() called, gblk %d, white = %s, black = %s\n",gblk,icmPdv(3, white),icmPdv(3,black));
#endif
	/* Default return incoming black as K only black */
	kblack[0] = black[0];
	kblack[1] = black[1];
	kblack[2] = black[2];

	/* Get the effective characteristics of the Lu */
	p->spaces(p, &ins, &inn, &outs, &outn, &alg, &intt, &fnc, NULL, &ord);

	if (fnc == icmBwd) { /* Hmm. We've got PCS to device, and we want device to PCS. */

		/* Strictly speaking this is a dubious approach, since for a cLut profile */
		/* the B2A table could make the effective white and black points */
		/* anything it likes, and they don't have to match what the corresponding */
		/* A2B table does. In our usage it's probably OK, since we tend */
		/* to use colorimetric B2A */ 
#ifdef DEBUG
		printf("~1 getting icmFwd\n");
#endif
		if ((p = icco->get_luobj(icco, icmFwd, intt, ins, ord)) == NULL)
			error("icxLu_comp_bk_point: assert: getting Fwd Lookup failed!");

		p->spaces(p, &ins, &inn, &outs, &outn, &alg, &intt, &fnc, NULL, &ord);
	}

	if (outs != icSigXYZData && outs != icSigLabData) {
		error("icxLu_comp_bk_point: assert: icc Lu output is not XYZ or Lab!, outs = 0x%x, ");
	}

#ifdef DEBUG
	printf("~1 icxLu_comp_bk_point called for inn = %d, ins = %s\n", inn, icx2str(icmColorSpaceSignature,ins));
#endif

	switch (ins) {

    	case icSigXYZData:
    	case icSigLabData:
    	case icSigLuvData:
    	case icSigYxyData:
#ifdef DEBUG
			printf("~1 Assuming CIE colorspace black is 0.0\n");
#endif
			if (gblk) {
				for (e = 0; e < inn; e++)
					black[0] = 0.0;
			}
			kblack[0] = black[0];
			kblack[1] = black[1];
			kblack[2] = black[2];
			return;

		case icSigRgbData:
#ifdef DEBUG
			printf("~1 RGB:\n");
#endif
			for (e = 0; e < inn; e++)
				dblack[e] = 0.0;
			break;

		case icSigGrayData: {	/* Could be additive or subtractive */
			double dval[1];
			double minv[3], maxv[3];
#ifdef DEBUG
			printf("~1 Gray:\n");
#endif
			/* Check out 0 and 100% colorant */
			dval[0] = 0.0;
			p->lookup(p, minv, dval);
			if (outs == icSigXYZData)
				icmXYZ2Lab(&icmD50, minv, minv);
			dval[0] = 1.0;
			p->lookup(p, maxv, dval);
			if (outs == icSigXYZData)
				icmXYZ2Lab(&icmD50, maxv, maxv);

			if (minv[0] < maxv[0])
				dblack[0] = 0.0;
			else
				dblack[0] = 1.0;
			}
			break;

		case icSigCmyData:
			for (e = 0; e < inn; e++)
				dblack[e] = 1.0;
			break;

		case icSigCmykData:
#ifdef DEBUG
			printf("~1 CMYK:\n");
#endif
			kch = 3;
			dblack[0] = 0.0;
			dblack[1] = 0.0;
			dblack[2] = 0.0;
			dblack[3] = 1.0;
			if (alg == icmLutType) {
				icxLuLut *pp = (icxLuLut *)x;
		
				if (pp->ink.tlimit >= 0.0)
					dblack[kch] = pp->ink.tlimit;
			};
			break;

			/* Use a heursistic. */
			/* This duplicates code in icxGetLimits() :-( */
			/* Colorant guessing should go in icclib ? */
		case icSig2colorData:
    	case icSig3colorData:
    	case icSig4colorData:
    	case icSig5colorData:
    	case icSig6colorData:
    	case icSig7colorData:
    	case icSig8colorData:
    	case icSig9colorData:
    	case icSig10colorData:
    	case icSig11colorData:
    	case icSig12colorData:
    	case icSig13colorData:
    	case icSig14colorData:
    	case icSig15colorData:
    	case icSigMch5Data:
    	case icSigMch6Data:
    	case icSigMch7Data:
    	case icSigMch8Data: {
				double dval[MAX_CHAN];
				double ncval[3];
				double cvals[MAX_CHAN][3];
				int nlighter, ndarker;

				/* Decide if the colorspace is additive or subtractive */
#ifdef DEBUG
				printf("~1 N channel:\n");
#endif

				/* First the no colorant value */
				for (e = 0; e < inn; e++)
					dval[e] = 0.0;
				p->lookup(p, ncval, dval);
				if (outs == icSigXYZData)
					icmXYZ2Lab(&icmD50, ncval, ncval);

				/* Then all the colorants */
				nlighter = ndarker = 0;
				for (e = 0; e < inn; e++) {
					dval[e] = 1.0;
					p->lookup(p, cvals[e], dval);
					if (outs == icSigXYZData)
						icmXYZ2Lab(&icmD50, cvals[e], cvals[e]);
					dval[e] = 0.0;
					if (fabs(cvals[e][0] - ncval[0]) > 5.0) {
						if (cvals[e][0] > ncval[0])
							nlighter++;
						else
							ndarker++;
					}
				}
				if (ndarker == 0 && nlighter > 0) {		/* Assume additive */
					for (e = 0; e < inn; e++)
						dblack[e] = 0.0;
#ifdef DEBUG
					printf("~1 N channel is additive:\n");
#endif

				} else if (ndarker > 0 && nlighter == 0) {				/* Assume subtractive. */
					double pbk[3] = { 0.0,0.0,0.0 };	/* Perfect black */
					double smd = 1e10;			/* Smallest distance */

#ifdef DEBUG
					printf("~1 N channel is subtractive:\n");
#endif
					/* See if we can guess the black channel */
					for (e = 0; e < inn; e++) {
						double tt;
						tt = icmNorm33sq(pbk, cvals[e]);
						if (tt < smd) {
							smd = tt;	
							kch = e;
						}
					}
					/* See if the black seems sane */
					if (cvals[kch][0] > 40.0
					 || fabs(cvals[kch][1]) > 10.0
					 || fabs(cvals[kch][2]) > 10.0) {
						if (p != op)
							p->del(p);
#ifdef DEBUG
						printf("~1 black doesn't look sanem so assume nothing\n");
#endif
						return;		/* Assume nothing */
					}

					/* Chosen kch as black */
					for (e = 0; e < inn; e++)
						dblack[e] = 0.0;
					dblack[kch] = 1.0;
					if (alg == icmLutType) {
						icxLuLut *pp = (icxLuLut *)x;
			
						if (pp->ink.tlimit >= 0.0)
							dblack[kch] = pp->ink.tlimit;
					};
#ifdef DEBUG
					printf("~1 N channel K = chan %d\n",kch);
#endif
				} else {
					if (p != op)
						p->del(p);
#ifdef DEBUG
					printf("~1 can't figure if additive or subtractive, so assume nothing\n");
#endif
					return;			/* Assume nothing */
				}
			}
			break;

		default:
#ifdef DEBUG
			printf("~1 unhandled colorspace, so assume nothing\n");
#endif
			if (p != op)
				p->del(p);
			return;				/* Don't do anything */
	}

	/* Lookup the K only value */
	if (kch >= 0) {
		p->lookup(p, kblack, dblack);
		
		/* We always return XYZ */
		if (outs == icSigLabData)
			icmLab2XYZ(&icmD50, kblack, kblack);
	}

	if (gblk == 0) {		/* That's all we have to do */
#ifdef DEBUG
		printf("~1 gblk == 0, so only return kblack\n");
#endif
		if (p != op)
			p->del(p);
		return;
	}

	/* Lookup the device black or K only value as a default */
	p->lookup(p, black, dblack);		/* May be XYZ or Lab */
#ifdef DEBUG
	printf("~1 Got default lu black %f %f %f, kch = %d\n", black[0],black[1],black[2],kch);
#endif

	/* !!! Hmm. For CMY and RGB we are simply using the device */
	/* combination values as the black point. In reality we might */
	/* want to have the option of using a neutral black point, */
	/* just like CMYK ?? */

	if (kch >= 0) {	/* The space is subtractive with a K channel. */
		/* If XICC_NEUTRAL_CMYK_BLACK then locate the darkest */
		/* CMYK within limits with the same chromaticity as the white point, */
		/* otherwise locate the device value within the ink limits that is */
		/* in the direction of the K channel */
		bpfind bfs;					/* Callback context */
		double sr[MXDO];			/* search radius */
		double tt[MXDO];			/* Temporary */
		double rs0[MXDO], rs1[MXDO];	/* Random start candidates */
		int trial;
		double brv;

		/* Setup callback function context */
		bfs.p = p;
		bfs.inn = inn;
		bfs.outs = outs;

		bfs.kch = kch;
		bfs.tlimit = -1.0;
		bfs.klimit = -1.0;
		bfs.toll = XICC_BLACK_POINT_TOLL;

		if (alg == icmLutType) {
			icxLuLut *pp = (icxLuLut *)x;

			pp->kch = kch;
			bfs.tlimit = pp->ink.tlimit;
			bfs.klimit = pp->ink.klimit;
#ifdef DEBUG
			printf("~1 tlimit = %f, klimit = %f\n",bfs.tlimit,bfs.klimit);
#endif
		};
	
#ifdef XICC_NEUTRAL_CMYK_BLACK
#ifdef DEBUG
		printf("~1 Searching for neutral black\n");
#endif
		/* white has been given to us in XYZ */
		icmXYZ2Lab(&icmD50, bfs.p1, white);		/* pivot Lab */
		icmCpy3(bfs.p2, white);					/* temp white XYZ */
		icmScale3(bfs.p2, bfs.p2, 0.02);	 /* Scale white XYZ towards 0,0,0 */
		icmXYZ2Lab(&icmD50, bfs.p2, bfs.p2); /* Convert black direction to Lab */

#else /* Use K directin black */
#ifdef DEBUG
		printf("~1 Searching for K direction black\n");
#endif
		icmXYZ2Lab(&icmD50, bfs.p1, white);			/* Pivot */

		/* Now figure abs Lab value of K only, as the direction */
		/* to use for the rich black. */
		for (e = 0; e < inn; e++)
			dblack[e] = 0.0;
		if (bfs.klimit < 0.0)
			dblack[kch] = 1.0;
		else
			dblack[kch] = bfs.klimit;		/* K value */

		p->lookup(p, black, dblack);

		if (outs == icSigXYZData) {
			icmXYZ2Lab(&icmD50, bfs.p2, black);		/* K direction */
		} else {
			icmAry2Ary(bfs.p2, black);
		}
#endif

#ifdef DEBUG
		printf("~1 Lab pivot %f %f %f, Lab K direction %f %f %f\n",bfs.p1[0],bfs.p1[1],bfs.p1[2],bfs.p2[0],bfs.p2[1],bfs.p2[2]);
#endif
		/* Set the random start 0 location as 000K */
		/* and the random start 1 location as CMY0 */
		{
			double tt;

			for (e = 0; e < inn; e++)
				dblack[e] = rs0[e] = 0.0;
			if (bfs.klimit < 0.0)
				dblack[kch] = rs0[kch] = 1.0;
			else
				dblack[kch] = rs0[kch] = bfs.klimit;		/* K value */

			if (bfs.tlimit < 0.0)
				tt = 1.0;
			else
				tt = bfs.tlimit/(inn - 1.0);
			for (e = 0; e < inn; e++)
				rs1[e] = tt;
			rs1[kch] = 0.0;		/* K value */
		}

		/* Start with the K only as the current best value */
		brv = bpfindfunc((void *)&bfs, dblack);
#ifdef DEBUG
		printf("~1 initial brv for K only = %f\n",brv);
#endif

		/* Find the device black point using optimization */
		/* Do several trials to avoid local minima. */
		rand32(0x12345678);	/* Make trial values deterministic */
		for (trial = 0; trial < 200; trial++) {
			double rv;			/* Temporary */

			/* Start first trial at 000K */
			if (trial == 0) {
				for (e = 0; e < inn; e++) {
					tt[e] = rs0[e];
					sr[e] = 0.1;
				}

			} else {
				/* Base is random between 000K and CMY0: */
				if (trial < 100) {
					rv = d_rand(0.0, 1.0);
					for (e = 0; e < inn; e++) {
						tt[e] = rv * rs0[e] + (1.0 - rv) * rs1[e];
						sr[e] = 0.1;
					}
				/* Base on current best */
				} else {
					for (e = 0; e < inn; e++) {
						tt[e] = dblack[e];
						sr[e] = 0.1;
					}
				}
	
				/* Then add random start offset */
				for (rv = 0.0, e = 0; e < inn; e++) {
					tt[e] +=  d_rand(-0.5, 0.5);
					if (tt[e] < 0.0)
						tt[e] = 0.0;
					else if (tt[e] > 1.0)
						tt[e] = 1.0;
				}
			}

			/* Clip black */
			if (bfs.klimit >= 0.0 && tt[kch] > bfs.klimit)
				tt[kch] = bfs.klimit;	
			
			/* Compute amount outside total limit */
			if (bfs.tlimit >= 0.0) {
				for (rv = 0.0, e = 0; e < inn; e++)
					rv += tt[e];
		
				if (rv > bfs.tlimit) {
					rv /= (double)inn;
					for (e = 0; e < inn; e++)
						tt[e] -= rv;
				}
			}
	
			if (powell(&rv, inn, tt, sr, 0.000001, 1000, bpfindfunc,
			                      (void *)&bfs, NULL, NULL) == 0) {
#ifdef DEBUG
				printf("~1 trial %d, rv %f bp %f %f %f %f\n",trial,rv,tt[0],tt[1],tt[2],tt[3]);
#endif
				if (rv < brv) {
#ifdef DEBUG
					printf("~1   new best\n");
#endif
					brv = rv;
					for (e = 0; e < inn; e++)
						dblack[e] = tt[e];
				}
			}
		}
		if (brv > 1000.0)
			error("icxLu_comp_bk_point: Black point powell failed");

		for (e = 0; e < inn; e++) { /* Make sure device values are in range */
			if (dblack[e] < 0.0)
				dblack[e] = 0.0;
			else if (dblack[e] > 1.0)
				dblack[e] = 1.0;
		}
		/* Now have device black in dblack[] */
#ifdef DEBUG
		printf("~1 got device black %f %f %f %f\n",dblack[0], dblack[1], dblack[2], dblack[3]);
#endif

		p->lookup(p, black, dblack);		/* Convert to PCS */
	}

	if (p != op)
		p->del(p);

	/* We always return XYZ */
	if (outs == icSigLabData)
		icmLab2XYZ(&icmD50, black, black);

#ifdef DEBUG
	printf("~1 returning %f %f %f\n", black[0], black[1], black[2]);
#endif

	return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - */

/* Return the media white and black points */
/* in the xlu effective PCS colorspace. Pointers may be NULL. */
/* (ie. these will be relative values for relative intent etc.) */
static void icxLuEfv_wh_bk_points(
icxLuBase *p,
double *wht,
double *blk,
double *kblk		/* K only black */
) {
	double white[3], black[3], kblack[3];

	/* Get the Lu PCS converted to XYZ icc black and white points in XYZ */
	if (p->plu->lu_wh_bk_points(p->plu, white, black)) {
		/* Black point is assumed. We should determine one instead. */
		/* Lookup K only black too */
		icxLu_comp_bk_point(p, 1, white, black, kblack);

	} else {
		/* Lookup a possible K only black */
		icxLu_comp_bk_point(p, 0, white, black, kblack);
	}

//printf("~1 white %f %f %f, black %f %f %f, kblack %f %f %f\n",white[0],white[1],white[2],black[0],black[1],black[2],kblack[0],kblack[1],kblack[2]);
	/* Convert to possibl xicc override PCS */
	switch (p->pcs) {
		case icSigXYZData:
			break;								/* Don't have to do anyting */
		case icSigLabData:
			icmXYZ2Lab(&icmD50, white, white);	/* Convert from XYZ to Lab */
			icmXYZ2Lab(&icmD50, black, black);
			icmXYZ2Lab(&icmD50, kblack, kblack);
			break;
		case icxSigJabData:
			p->cam->XYZ_to_cam(p->cam, white, white);	/* Convert from XYZ to Jab */
			p->cam->XYZ_to_cam(p->cam, black, black);
			p->cam->XYZ_to_cam(p->cam, kblack, kblack);
			break;
		default:
			break;
	}

//printf("~1 icxLuEfv_wh_bk_points: pcsor %s White %f %f %f, Black %f %f %f\n", icx2str(icmColorSpaceSignature,p->pcs), white[0], white[1], white[2], black[0], black[1], black[2]);
	if (wht != NULL) {
		wht[0] = white[0];
		wht[1] = white[1];
		wht[2] = white[2];
	}

	if (blk != NULL) {
		blk[0] = black[0];
		blk[1] = black[1];
		blk[2] = black[2];
	}

	if (kblk != NULL) {
		kblk[0] = kblack[0];
		kblk[1] = kblack[1];
		kblk[2] = kblack[2];
	}
}

/* Create an instance of an xicc object */
xicc *new_xicc(
icc *picc		/* icc we are expanding */
) {
	xicc *p;
	if ((p = (xicc *) calloc(1,sizeof(xicc))) == NULL)
		return NULL;
	p->pp = picc;
	p->del           = xicc_del;
	p->get_luobj     = xicc_get_luobj;
	p->set_luobj     = xicc_set_luobj;
	p->get_viewcond  = xicc_get_viewcond;

	/* Create an xcal if there is the right tag in the profile */
	p->cal = xiccReadCalTag(p->pp);
	p->nodel_cal = 0;	/* We created it, we will delete it */

	return p;
}

/* Do away with the xicc (but not the icc!) */
static void xicc_del(
xicc *p
) {
	if (p->cal != NULL && p->nodel_cal == 0)
		p->cal->del(p->cal);
	free (p);
}


/* Return an expanded lookup object, initialised */
/* from the icc. */
/* Return NULL on error, check errc+err for reason. */
/* Set the pcsor & intent to consistent and values if */
/* Jab and/or icxAppearance has been requested. */
/* Create the underlying icm lookup object that is used */
/* to create and implement the icx one. The icm will be used */
/* to translate from native to effective PCS, unless the */
/* effective PCS is Jab, in which case the icm will be set to */
/* have an effective PCS of XYZ. Since native<->effecive PCS conversion */
/* is done at the to/from_abs() stage, none of this affects the individual */
/* conversion steps, which will all talk the native PCS (unless merged). */
icxLuBase *xicc_get_luobj(
xicc *p,					/* this */
int flags,					/* clip, merge flags */
icmLookupFunc func,			/* Functionality */
icRenderingIntent intent,	/* Intent */
icColorSpaceSignature pcsor,/* PCS override (0 = def) */
icmLookupOrder order,		/* Search Order */
icxViewCond *vc,			/* Viewing Condition (may be NULL if pcsor is not CIECAM) */
icxInk *ink					/* inking details (NULL for default) */
) {
	icmLuBase *plu;
	icxLuBase *xplu;
	icmLuAlgType alg;
	icRenderingIntent n_intent = intent;			/* Native Intent to request */
	icColorSpaceSignature n_pcs = icmSigDefaultData;	/* Native PCS to request */

//printf("~1 xicc_get_luobj got intent '%s' and pcsor '%s'\n",icx2str(icmRenderingIntent,intent),icx2str(icmColorSpaceSignature,pcsor));

	/* Ensure that appropriate PCS is slected for an appearance intent */
	if (intent == icxAppearance
	 || intent == icxAbsAppearance
	 || intent == icxPerceptualAppearance
	 || intent == icxAbsPerceptualAppearance
	 || intent == icxSaturationAppearance
	 || intent == icxAbsSaturationAppearance) {
		pcsor = icxSigJabData;
//printf("~1 pcsor = %s\n",tag2str(pcsor));

	/* Translate non-Jab intents to the equivalent appearance "intent" if pcsor == Jab. */
	/* This is how we get these when the UI's don't list all the apperances intents, */
	/* we select the analogous non-apperance intent with pcsor = Jab. */
	/* Note that Abs/non-abs selects between Apperance and AbsAppearance. */
	} else if (pcsor == icxSigJabData) {
		if (intent == icRelativeColorimetric)
			intent = icxAppearance;
		else if (intent == icAbsoluteColorimetric)
			intent = icxAbsAppearance;
		else if (intent == icPerceptual)
			intent = icxPerceptualAppearance;
		else if (intent == icmAbsolutePerceptual)
			intent = icxAbsPerceptualAppearance;
		else if (intent == icSaturation)
			intent = icxSaturationAppearance;
		else if (intent == icmAbsoluteSaturation)
			intent = icxAbsSaturationAppearance;
		else
			intent = icxAppearance;
	}
//printf("~1 intent = %s\n",tag2str(intent));

	/* Translate intent asked for into intent needed in icclib */
	if      (intent == icxAppearance
	      || intent == icxAbsAppearance)
		n_intent = icAbsoluteColorimetric;
	else if (intent == icxPerceptualAppearance
	      || intent == icxAbsPerceptualAppearance)
		n_intent = icmAbsolutePerceptual;
	else if (intent == icxSaturationAppearance
	      || intent == icxAbsSaturationAppearance)
		n_intent = icmAbsoluteSaturation;
//printf("~1 n_intent = %s\n",tag2str(n_intent));

	if (pcsor != icmSigDefaultData)
		n_pcs = pcsor;			/* There is an icclib override */

	if (pcsor == icxSigJabData)	/* xicc override */
		n_pcs = icSigXYZData;	/* Translate to XYZ */

//printf("~1 xicc_get_luobj processed intent %s and pcsor %s\n",icx2str(icmRenderingIntent,intent),icx2str(icmColorSpaceSignature,pcsor));
//printf("~1 xicc_get_luobj icclib intent %s and pcsor %s\n",icx2str(icmRenderingIntent,n_intent),icx2str(icmColorSpaceSignature,n_pcs));
	/* Get icclib lookup object */
	if ((plu = p->pp->get_luobj(p->pp, func, n_intent, n_pcs, order)) == NULL) {
		p->errc = p->pp->errc;		/* Copy error */
		strcpy(p->err, p->pp->err);
		return NULL;
	}

	/* Figure out what the algorithm is */
	plu->spaces(plu, NULL, NULL, NULL, NULL, &alg, NULL, NULL, &n_pcs, NULL);

	/* make sure its "Abs CAM" */
	if (vc!= NULL
	 && (intent == icxAbsAppearance
	  || intent == icxAbsPerceptualAppearance
	  || intent == icxAbsSaturationAppearance)) {	/* make sure its "Abs CAM" */
//printf("~1 xicc_get_luobj using absolute apperance space with white = D50\n");
		/* Set white point and flare color to D50 */
		/* (Hmm. This doesn't match what happens within collink with absolute intent!!) */
		vc->Wxyz[0] = icmD50.X/icmD50.Y;
		vc->Wxyz[1] = icmD50.Y/icmD50.Y;	// Normalise white reference to Y = 1 ?
		vc->Wxyz[2] = icmD50.Z/icmD50.Y;
	
		vc->Gxyz[0] = icmD50.X;
		vc->Gxyz[1] = icmD50.Y;
		vc->Gxyz[2] = icmD50.Z;
	}

	/* Call xiccLu wrapper creation */
	switch (alg) {
		case icmMonoFwdType:
			xplu = new_icxLuMono(p, flags, plu, func, intent, pcsor, vc, 0);
			break;
		case icmMonoBwdType:
			xplu = new_icxLuMono(p, flags, plu, func, intent, pcsor, vc, 1);
			break;
    	case icmMatrixFwdType:
			xplu = new_icxLuMatrix(p, flags, plu, func, intent, pcsor, vc, 0);
			break;
    	case icmMatrixBwdType:
			xplu = new_icxLuMatrix(p, flags, plu, func, intent, pcsor, vc, 1);
			break;
    	case icmLutType:
			xplu = new_icxLuLut(p, flags, plu, func, intent, pcsor, vc, ink);
			break;
		default:
			xplu = NULL;
			break;
	}

	return xplu;
}


/* Return an expanded lookup object, initialised */
/* from the icc, and then overwritten by a conversion */
/* created from the supplied scattered data points. */
/* The Lut is assumed to be a device -> native PCS profile. */
/* If the SET_WHITE and/or SET_BLACK flags are set, */
/* discover the white/black point, set it in the icc, */
/* and make the Lut relative to them. */
/* Return NULL on error, check errc+err for reason */
static icxLuBase *xicc_set_luobj(
xicc *p,					/* this */
icmLookupFunc func,			/* Functionality */
icRenderingIntent intent,	/* Intent */
icmLookupOrder order,		/* Search Order */
int flags,					/* white/black point, verbose flags etc. */
int no,						/* Number of points */
int nobw,					/* Number of points to look for white & black patches in */
cow *points,				/* Array of input points in target PCS space */
icxMatrixModel *skm,   		/* Optional skeleton model (used for input profiles) */
double dispLuminance,		/* > 0.0 if display luminance value and is known */
double wpscale,				/* > 0.0 if input white point is to be scaled */
//double *bpo,				/* != NULL for black point override XYZ */
double smooth,				/* RSPL smoothing factor, -ve if raw */
double avgdev,				/* reading Average Deviation as a proportion of the input range */
double demph,				/* dark emphasis factor for cLUT grid res. */
icxViewCond *vc,			/* Viewing Condition (NULL if not using CAM) */
icxInk *ink,				/* inking details (NULL for default) */
xcal *cal,                  /* Optional cal, will override any existing (not deleted with xicc)*/
int quality					/* Quality metric, 0..3 */
) {
	icmLuBase *plu;
	icxLuBase *xplu = NULL;
	icmLuAlgType alg;

	if (cal != NULL) {
		if (p->cal != NULL && p->nodel_cal == 0)
			p->cal->del(p->cal);
		p->cal = cal;
		p->nodel_cal = 1;	/* We were given it, so don't delete it */
	}

	if (func != icmFwd) {
		p->errc = 1;
		sprintf(p->err,"Can only create Device->PCS profiles from scattered data.");
		xplu = NULL;
		return xplu;
	}

	/* Get icclib lookup object */
	if ((plu = p->pp->get_luobj(p->pp, func, intent, 0, order)) == NULL) {
		p->errc = p->pp->errc;		/* Copy error */
		strcpy(p->err, p->pp->err);
		return NULL;
	}

	/* Figure out what the algorithm is */
	plu->spaces(plu, NULL, NULL, NULL, NULL, &alg, NULL, NULL, NULL, NULL);

	/* Call xiccLu wrapper creation */
	switch (alg) {
		case icmMonoFwdType:
			p->errc = 1;
			sprintf(p->err,"Setting Monochrome Fwd profile from scattered data not supported.");
			plu->del(plu);
			xplu = NULL;		/* Not supported yet */
			break;

    	case icmMatrixFwdType:
			if (smooth < 0.0)
				smooth = -smooth;
			xplu = set_icxLuMatrix(p, plu, flags, no, nobw, points, skm, dispLuminance, wpscale,
//			                       bpo,
			                       quality, smooth);
			break;

    	case icmLutType:
			/* ~~~ Should add check that it is a fwd profile ~~~ */
			xplu = set_icxLuLut(p, plu, func, intent, flags, no, nobw, points, skm, dispLuminance,
			                    wpscale,
//			                    bpo,
			                    smooth, avgdev, demph, vc, ink, quality);
			break;

		default:
			break;
	}

	return xplu;
}

/* ------------------------------------------------------ */
/* Viewing Condition Parameter stuff */

#ifdef NEVER	/* Not currently used */

/* Guess viewing parameters from the technology signature */
static void guess_from_techsig(
icTechnologySignature tsig,
double *Ybp
) {
double Yb = -1.0;

	switch (tsig) {
		/* These are all inputing either a representation of */
		/* a natural scene captured on another medium, or are assuming */
		/* that the medium is the original. A _good_ system would */
		/* let the user indicate which is the case. */
		case icSigReflectiveScanner:
		case icSigFilmScanner:
			Yb = 0.2;
			break;

		/* Direct scene to value devices. */
		case icSigDigitalCamera:
		case icSigVideoCamera:
			Yb = 0.2;
			break;

		/* Emmisive displays. */
		/* We could try tweaking the white point on the assumption */
		/* that the viewer will be adapted to a combination of both */
		/* the CRT white point, and the ambient light. */
		case icSigVideoMonitor:
		case icSigCRTDisplay:
		case icSigPMDisplay:
		case icSigAMDisplay:
			Yb = 0.2;
			break;

		/* Photo CD has its own viewing definitions */
		/* (It represents original scene colors) */
		case icSigPhotoCD:
			Yb = 0.2;
			break;

		/* Projection devices, either direct, or */
		/* via another intermediate medium. */
		case icSigProjectionTelevision:
			Yb = 0.1;		/* Assume darkened room, little background */
			break;
		case icSigFilmWriter:
			Yb = 0.0;		/* Assume a dark room - no background */
			break;

		/* Printed media devices. */
		case icSigInkJetPrinter:
		case icSigThermalWaxPrinter:
		case icSigElectrophotographicPrinter:
		case icSigElectrostaticPrinter:
		case icSigDyeSublimationPrinter:
		case icSigPhotographicPaperPrinter:
		case icSigPhotoImageSetter:
		case icSigGravure:
		case icSigOffsetLithography:
		case icSigSilkscreen:
		case icSigFlexography:
			Yb = 0.2;
			break;

		default:
			Yb = 0.2;
	}

	if (Ybp != NULL)
		*Ybp = Yb;
}

#endif /* NEVER */


/* See if we can read or guess the viewing conditions */
/* for an ICC profile. */
/* Return value 0 if it is well defined */
/* Return value 1 if it is a guess */
/* Return value 2 if it is not possible/appropriate */
int xicc_get_viewcond(
xicc *p,			/* Expanded profile we're working with */
icxViewCond *vc		/* Viewing parameters to return */
) {
	icc *pp = p->pp;	/* Base ICC */

	/* Numbers we're trying to find */
	ViewingCondition Ev = vc_none;
	double Wxyz[3] = {-1.0, -1.0, -1.0};	/* Adapting white color */
	double La = -1.0;						/* Adapting/Surround luminance */
	double Ixyz[3] = {-1.0, -1.0, -1.0};	/* Illuminant color */
	double Li = -1.0;						/* Illuminant luminance */
	double Lb = -1.0;		/* Backgrount luminance */
	double Yb = -1.0;		/* Background relative luminance to Lv */
	double Lve = -1.0;		/* Emissive device image luminance */
	double Lvr = -1.0;		/* Reflective device image luminance */
	double Lv = -1.0;		/* Device image luminance */
	double Yf = -1.0;		/* Flare relative luminance to Lv */
	double Yg = -1.0;		/* Glare relative luminance to La */
	double Gxyz[3] = {-1.0, -1.0, -1.0};	/* Glare color */
    icTechnologySignature tsig = icMaxEnumTechnology; /* Technology Signature */
    icProfileClassSignature devc = icMaxEnumClass;
	int trans = -1;		/* Set to 0 if not transparency, 1 if it is */

	/* Collect all the information we can find */

	/* Emmisive devices image white luminance */
	{
		icmXYZArray *luminanceTag;

		if ((luminanceTag = (icmXYZArray *)pp->read_tag(pp, icSigLuminanceTag)) != NULL
         && luminanceTag->ttype == icSigXYZType && luminanceTag->size >= 1) {
			Lve = luminanceTag->data[0].Y;	/* Copy structure */
		} 
	}

	/* Flare: */
	{
		icmMeasurement *ro;

		if ((ro = (icmMeasurement *)pp->read_tag(pp, icSigMeasurementTag)) != NULL
		 && ro->ttype == icSigMeasurementType) {
	
    		Yf = 0.0 * ro->flare;		// ?????
    		Yg = 1.0 * ro->flare;		// ?????
   			/* ro->illuminant ie D50, D65, D93, A etc. */
		}
	}

	/* Media White Point */
	{
		icmXYZArray *whitePointTag;

		if ((whitePointTag = (icmXYZArray *)pp->read_tag(pp, icSigMediaWhitePointTag)) != NULL
         && whitePointTag->ttype == icSigXYZType && whitePointTag->size >= 1) {
			Wxyz[0] = whitePointTag->data[0].X;
			Wxyz[1] = whitePointTag->data[0].Y;
			Wxyz[2] = whitePointTag->data[0].Z;
		}
	}

	/* ViewingConditions: */
	{
		icmViewingConditions *ro;

		if ((ro = (icmViewingConditions *)pp->read_tag(pp, icSigViewingConditionsTag)) != NULL
		 && ro->ttype == icSigViewingConditionsType) {
	
   			/* ro->illuminant.X */
   			/* ro->illuminant.Z */

    		Li = ro->illuminant.Y;

			/* Reflect illuminant off the media white */
    		Lvr = Li * Wxyz[1];

			/* Illuminant color */
			Ixyz[0] = ro->illuminant.X/ro->illuminant.Y;
			Ixyz[1] = 1.0;
			Ixyz[2] = ro->illuminant.Z/ro->illuminant.Y;
			
			/* Assume ICC surround is CICAM97 background */
    		/* ro->surround.X */
    		/* ro->surround.Z */
    		La = ro->surround.Y;

    		/* ro->stdIlluminant ie D50, D65, D93, A etc. */
		}
	}

	/* Stuff we might need */

	/* Technology: */
	{
		icmSignature *ro;

		/* Try and read the tag from the file */
		if ((ro = (icmSignature *)pp->read_tag(pp, icSigTechnologyTag)) != NULL
		 && ro->ttype != icSigSignatureType) {

		tsig = ro->sig;
		}
	}

    devc = pp->header->deviceClass;	/* Type of profile */
	if (devc == icSigLinkClass
	 || devc == icSigAbstractClass
	 || devc == icSigColorSpaceClass
	 || devc == icSigNamedColorClass)
		return 2;

	/*
    	icSigInputClass
    	icSigDisplayClass
    	icSigOutputClass
	*/

	if ((pp->header->flags & icTransparency) != 0)
		trans = 1;
	else
		trans = 0;


	/* figure Lv if we have the information */
	if (Lve >= 0.0)
		Lv = Lve;		/* Emmisive image white luminance */
	else
		Lv = Lvr;		/* Reflectance image white luminance */

	/* Fudge the technology signature */
	if (tsig == icMaxEnumTechnology) {
		if (devc == icSigDisplayClass)
			tsig = icSigCRTDisplay;
	}

#ifndef NEVER
	printf("Enumeration = %d\n", Ev);
	printf("Viewing Conditions:\n");
	printf("White adaptation color %f %f %f\n",Wxyz[0], Wxyz[1], Wxyz[2]);
	printf("Adapting Luminance La = %f\n",La);
	printf("Illuminant color %f %f %f\n",Ixyz[0], Ixyz[1], Ixyz[2]);
	printf("Illuminant Luminance Li = %f\n",Li);
	printf("Background Luminance Lb = %f\n",Lb);
	printf("Relative Background Yb = %f\n",Yb);
	printf("Emissive Image White Lve = %f\n",Lve);
	printf("Reflective Image White Lvr = %f\n",Lvr);
	printf("Device Image White Lv = %f\n",Lv);
	printf("Relative Flare Yf = %f\n",Yf);
	printf("Relative Glare Yg = %f\n",Yg);
	printf("Glare color %f %f %f\n",Gxyz[0], Gxyz[1], Gxyz[2]);
	printf("Technology = %s\n",tag2str(tsig));
	printf("deviceClass = %s\n",tag2str(devc));
	printf("Transparency = %d\n",trans);
#endif
	
	/* See if the viewing conditions are completely defined as ICC can do it */
	if (Wxyz[0] >= 0.0 && Wxyz[1] >= 0.0 && Wxyz[2] >= 0.0
	 && La >= 0.0
	 && Yb >= 0.0
	 && Lv >= 0.0
	 && Yf >= 0.0
	 && Yg >= 0.0
	 && Gxyz[0] >= 0.0 && Gxyz[1] >= 0.0 && Gxyz[2] >= 0.0) {

		vc->Ev = vc_none;
		vc->Wxyz[0] = Wxyz[0];
		vc->Wxyz[1] = Wxyz[1];
		vc->Wxyz[2] = Wxyz[2];
		vc->La = La;
		vc->Yb = Yb;
		vc->Lv = Lv;
		vc->Yf = Yf;
		vc->Yg = Yg;
		vc->Gxyz[0] = Gxyz[0];	
		vc->Gxyz[1] = Gxyz[1];	
		vc->Gxyz[2] = Gxyz[2];	
		return 0;
	}

	/* Hmm. We didn't get all the info an ICC can contain. */
	/* We will try to guess some reasonable defaults */

	/* Have we at least got an adaptation white point ? */
	if (Wxyz[0] < 0.0 || Wxyz[1] < 0.0 || Wxyz[2] < 0.0)
		return 2;						/* No */

	/* Have we got the technology ? */
	if (tsig == icMaxEnumTechnology)
		return 2;						/* Hopeless */

	/* Guess from the technology */
	switch (tsig) {

		/* This is inputing either a representation of */
		/* a natural scene captured on another a print medium, or */
		/* are is assuming that the medium is the original. */
		/* We will assume that the print is the original. */
		case icSigReflectiveScanner:
			{
				if (La < 0.0)	/* No adapting luminance */
					La = 34.0;	/* Use a practical print evaluation number */
				if (Yb < 0.0)	/* No background relative luminance */
					Yb = 0.2;	/* Assume grey world */
				if (Lv < 0.0)	/* No device image luminance */
					Ev = vc_average;	/* Assume average viewing conditions */
				if (Yf < 0.0)	/* No flare figure */
					Yf = 0.0;	/* Assume 0% flare */
				if (Yg < 0.0)	/* No glare figure */
					Yg = 0.01;	/* Assume 1% glare */
				if (Gxyz[0] < 0.0 || Gxyz[1] < 0.0 || Gxyz[2] < 0.0)	/* No flare color */
					Gxyz[0] = Wxyz[0], Gxyz[1] = Wxyz[1], Gxyz[2] = Wxyz[2];
				break;
			}

		/* This is inputing either a representation of */
		/* a natural scene captured on another a photo medium, or */
		/* are is assuming that the medium is the original. */
		/* We will assume a compromise media original, natural scene */
		case icSigFilmScanner:
			{
				if (La < 0.0)	/* No adapting luminance */
					La = 50.0;	/* Use bright indoors, dull outdoors */
				if (Yb < 0.0)	/* No background relative luminance */
					Yb = 0.2;	/* Assume grey world */
				if (Lv < 0.0)	/* No device image luminance */
					Ev = vc_average;	/* Assume average viewing conditions */
				if (Yf < 0.0)	/* No flare figure */
					Yf = 0.0;	/* Assume 0% flare */
				if (Yg < 0.0)	/* No glare figure */
					Yg = 0.01;	/* Assume 1% glare */
				if (Gxyz[0] < 0.0 || Gxyz[1] < 0.0 || Gxyz[2] < 0.0)	/* No flare color */
					Gxyz[0] = Wxyz[0], Gxyz[1] = Wxyz[1], Gxyz[2] = Wxyz[2];
				break;
			}

		/* Direct scene to value devices. */
		case icSigDigitalCamera:
		case icSigVideoCamera:
			{
				if (La < 0.0)	/* No adapting luminance */
					La = 110.0;	/* Use very bright indoors, usual outdoors */
				if (Yb < 0.0)	/* No background relative luminance */
					Yb = 0.2;	/* Assume grey world */
				if (Lv < 0.0)	/* No device image luminance */
					Ev = vc_average;	/* Assume average viewing conditions */
				if (Yf < 0.0)	/* No flare figure */
					Yf = 0.0;	/* Assume 0% flare */
				if (Yg < 0.0)	/* No glare figure */
					Yg = 0.01;	/* Assume 1% glare */
				if (Gxyz[0] < 0.0 || Gxyz[1] < 0.0 || Gxyz[2] < 0.0)	/* No flare color */
					Gxyz[0] = Wxyz[0], Gxyz[1] = Wxyz[1], Gxyz[2] = Wxyz[2];
				if (Gxyz[0] < 0.0 || Gxyz[1] < 0.0 || Gxyz[2] < 0.0)	/* No flare color */
					Gxyz[0] = Wxyz[0], Gxyz[1] = Wxyz[1], Gxyz[2] = Wxyz[2];
				break;
			}

		/* Emmisive displays. */
		/* Assume a video monitor is in a darker environment than a CRT */
		case icSigVideoMonitor:
			{
				if (La < 0.0)	/* No adapting luminance */
					La = 4.0;	/* Darkened work environment */
				if (Yb < 0.0)	/* No background relative luminance */
					Yb = 0.2;	/* Assume grey world */
				if (Lv < 0.0)	/* No device image luminance */
					Ev = vc_dim;	/* Assume dim viewing conditions */
				if (Yf < 0.0)	/* No flare figure */
					Yf = 0.0;	/* Assume 0% flare */
				if (Yg < 0.0)	/* No glare figure */
					Yg = 0.01;	/* Assume 1% glare */
				if (Gxyz[0] < 0.0 || Gxyz[1] < 0.0 || Gxyz[2] < 0.0)	/* No flare color */
					Gxyz[0] = Wxyz[0], Gxyz[1] = Wxyz[1], Gxyz[2] = Wxyz[2];
				break;
			}


		/* Assume a typical work environment */
		case icSigCRTDisplay:
		case icSigPMDisplay:
		case icSigAMDisplay:
			{
				if (La < 0.0)	/* No adapting luminance */
					La = 33.0;	/* Typical work environment */
				if (Yb < 0.0)	/* No background relative luminance */
					Yb = 0.2;	/* Assume grey world */
				if (Lv < 0.0)	/* No device image luminance */
					Ev = vc_average;	/* Assume average viewing conditions */
				if (Yf < 0.0)	/* No flare figure */
					Yf = 0.0;	/* Assume 0% flare */
				if (Yg < 0.0)	/* No glare figure */
					Yg = 0.01;	/* Assume 1% glare */
				if (Gxyz[0] < 0.0 || Gxyz[1] < 0.0 || Gxyz[2] < 0.0)	/* No flare color */
					Gxyz[0] = Wxyz[0], Gxyz[1] = Wxyz[1], Gxyz[2] = Wxyz[2];
				break;
			}

		/* Photo CD has its own viewing definitions */
		/* (It represents original scene colors) */
		case icSigPhotoCD:
			{
				if (La < 0.0)	/* No adapting luminance */
					La = 320.0;	/* Bright outdoors */
				if (Yb < 0.0)	/* No background relative luminance */
					Yb = 0.2;	/* Assume grey world */
				if (Lv < 0.0)	/* No device image luminance */
					Ev = vc_average;	/* Assume average viewing conditions */
				if (Yf < 0.0)	/* No flare figure */
					Yf = 0.0;	/* Assume 0% flare */
				if (Yg < 0.0)	/* No glare figure */
					Yg = 0.0;	/* Assume 0% glare */
				if (Gxyz[0] < 0.0 || Gxyz[1] < 0.0 || Gxyz[2] < 0.0)	/* No flare color */
					Gxyz[0] = Wxyz[0], Gxyz[1] = Wxyz[1], Gxyz[2] = Wxyz[2];
				break;
			}

		/* Projection devices, either direct, or */
		/* via another intermediate medium. */
		/* Assume darkened room, little background */
		case icSigProjectionTelevision:
			{
				if (La < 0.0)	/* No adapting luminance */
					La = 7.0;	/* Dark environment */
				if (Yb < 0.0)	/* No background relative luminance */
					Yb = 0.1;	/* Assume little background */
				if (Lv < 0.0)	/* No device image luminance */
					Ev = vc_dim;	/* Dim environment */
				if (Yf < 0.0)	/* No flare figure */
					Yf = 0.0;	/* Assume 0% flare */
				if (Yg < 0.0)	/* No glare figure */
					Yg = 0.01;	/* Assume 1% glare */
				if (Gxyz[0] < 0.0 || Gxyz[1] < 0.0 || Gxyz[2] < 0.0)	/* No flare color */
					Gxyz[0] = Wxyz[0], Gxyz[1] = Wxyz[1], Gxyz[2] = Wxyz[2];
				break;
			}
		/* Assume very darkened room, no background */
		case icSigFilmWriter:
			{
				if (La < 0.0)	/* No adapting luminance */
					La = 7.0;	/* Dark environment */
				if (Yb < 0.0)	/* No background relative luminance */
					Yb = 0.0;	/* Assume no background */
				if (Lv < 0.0)	/* No device image luminance */
					Ev = vc_dark;	/* Dark environment */
				if (Yf < 0.0)	/* No flare figure */
					Yf = 0.0;	/* Assume 0% flare */
				if (Yg < 0.0)	/* No glare figure */
					Yg = 0.01;	/* Assume 1% glare */
				if (Gxyz[0] < 0.0 || Gxyz[1] < 0.0 || Gxyz[2] < 0.0)	/* No flare color */
					Gxyz[0] = Wxyz[0], Gxyz[1] = Wxyz[1], Gxyz[2] = Wxyz[2];
				break;
			}

		/* Printed media devices. */
		/* Assume a normal print viewing environment */
		case icSigInkJetPrinter:
		case icSigThermalWaxPrinter:
		case icSigElectrophotographicPrinter:
		case icSigElectrostaticPrinter:
		case icSigDyeSublimationPrinter:
		case icSigPhotographicPaperPrinter:
		case icSigPhotoImageSetter:
		case icSigGravure:
		case icSigOffsetLithography:
		case icSigSilkscreen:
		case icSigFlexography:
			{
				if (La < 0.0)	/* No adapting luminance */
					La = 40.0;	/* Use a practical print evaluation number */
				if (Yb < 0.0)	/* No background relative luminance */
					Yb = 0.2;	/* Assume grey world */
				if (Lv < 0.0)	/* No device image luminance */
					Ev = vc_average;	/* Assume average viewing conditions */
				if (Yf < 0.0)	/* No flare figure */
					Yf = 0.0;	/* Assume 0% flare */
				if (Yg < 0.0)	/* No glare figure */
					Yg = 0.01;	/* Assume 1% glare */
				if (Gxyz[0] < 0.0 || Gxyz[1] < 0.0 || Gxyz[2] < 0.0)	/* No flare color */
					Gxyz[0] = Wxyz[0], Gxyz[1] = Wxyz[1], Gxyz[2] = Wxyz[2];
				break;
			}

		default:
			{
			return 2;
			}
	}

	return 1;
}

/* Write our viewing conditions to the underlying ICC profile, */
/* using a private tag. */
void xicc_set_viewcond(
xicc *p,			/* Expanded profile we're working with */
icxViewCond *vc		/* Viewing parameters to return */
) {
	//icc *pp = p->pp;	/* Base ICC */

	// ~~1	Not implemented yet
}



/* Return an enumerated viewing condition */
/* Return enumeration if OK, -999 if there is no such enumeration. */
/* xicc may be NULL if just the description is wanted, */
/* or an explicit white point is provided. */
int xicc_enum_viewcond(
xicc *p,			/* Expanded profile to get white point (May be NULL if desc NZ) */
icxViewCond *vc,	/* Viewing parameters to return, May be NULL if desc is nz */
int no,				/* Enumeration to return, -1 for default, -2 for none */
char *as,			/* String alias to number, NULL if none */
int desc,			/* NZ - Just return a description of this enumeration in vc */
double *wp			/* Provide white point if xicc is NULL */
) {

	if (desc == 0) {	/* We're setting the viewing condition */
		icc *pp;		/* Base ICC */
		icmXYZArray *whitePointTag;

		if (vc == NULL)
			return -999;

		if (p == NULL) {
			if (wp == NULL)
				return -999;
			vc->Wxyz[0] = wp[0];
			vc->Wxyz[1] = wp[1];
			vc->Wxyz[2] = wp[2];
		} else {
	
			pp = p->pp;
			if ((whitePointTag = (icmXYZArray *)pp->read_tag(pp, icSigMediaWhitePointTag)) != NULL
	   	      && whitePointTag->ttype == icSigXYZType && whitePointTag->size >= 1) {
				vc->Wxyz[0] = whitePointTag->data[0].X;
				vc->Wxyz[1] = whitePointTag->data[0].Y;
				vc->Wxyz[2] = whitePointTag->data[0].Z;
			} else {
				if (wp == NULL) { 
					sprintf(p->err,"Enum VC: Failed to read Media White point");
					p->errc = 2;
					return -999;
				}
				vc->Wxyz[0] = wp[0];
				vc->Wxyz[1] = wp[1];
				vc->Wxyz[2] = wp[2];
			}
		}

		/* Set a default Glare color */
		vc->Gxyz[0] = vc->Wxyz[0];
		vc->Gxyz[1] = vc->Wxyz[1];
		vc->Gxyz[2] = vc->Wxyz[2];
	}

	/*

	Typical adapting field luminances and white luminance in reflective setup:
	(Note that displays Lv is typically brighter under the same conditions)

	E = illuminance in Lux
	La = Adapting field luminance in cd/m^2, assuming 20% reflectance from surround
	Lv = White luminance assuming 100% reflectance
    
	    E     La     Lv   Condition 
	    11     0.7    4   Twilight
	    32     2     10   Subdued indoor lighting
	    64     4     20   Less than typical office light; sometimes recommended for
	                      display-only workplaces (sRGB)
	   350    22    111   Typical Office (sRGB annex D)
	   500    32    160   Practical print evaluationa (ISO-3664 P2)
	  1000    64    318   Good Print evaluation (CIE 116-1995)
	  1000    64    318   Television Studio lighting
	  1000    64    318   Overcast Outdoors
	  2000   127    637   Critical print evaluation (ISO-3664 P1)
	 10000   637   3183   Typical outdoors, full daylight 
	 50000  3185  15915   Bright summers day 

	Display numbers:

	SMPTE video standard white 100
	SMPTE cinema standard white 55

	Flare is image content dependent, and is typically 1% from factors
	including display self illumination and observer/camera internal
	stray light. Because image content is not static, using a 1% of white point
	flare results quite erronious appearance modelling for predominantly
	dark images. As a result, it is best to default to a Yf of 0%,
	and only introduce a higher number depending on the known image content.

	Glare is assumed to be from the ambient light reflecting from the display
	and also striking the observer directly, and is (typically) defaulted
	to 1% of ambient here.
	
	*/

	if (no == -1
	 || (as != NULL && stricmp(as,"d") == 0)) {

		no = -1;
		if (vc != NULL) {
			vc->desc = "  d - Default Viewing Condition";
			vc->Ev = vc_average;	/* Average viewing conditions */
			vc->La = 50.0;			/* Practical to Good lighting */
			vc->Lv = 250.0;			/* Average viewing conditions ratio */
			vc->Yb = 0.2;			/* Grey world */
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.01;			/* 1% glare */
		}
	}
	else if (no == 2
	 || (as != NULL && stricmp(as,"pc") == 0)) {

		no = 2;
		if (vc != NULL) {
			vc->desc = " pc - Critical print evaluation environment (ISO-3664 P1)";
			vc->Ev = vc_average;	/* Average viewing conditions */
			vc->La = 127.0;			/* 0.2 * Lv ? */
			vc->Lv = 2000.0/3.1415;	/* White of the image field */ 
			vc->Yb = 0.2;			/* Grey world */
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.01;			/* 1% glare */
		}
	}
	else if (no == 0
	 || (as != NULL && stricmp(as,"pp") == 0)) {

		no = 0;
		if (vc != NULL) {
			vc->desc = " pp - Practical Reflection Print (ISO-3664 P2)";
			vc->Ev = vc_none;		/* Use explicit La/Lv */
			vc->La = 32.0;			/* 0.2 * Lv ? */
			vc->Lv = 500.0/3.1415;	/* White of the image field */ 
			vc->Yb = 0.2;			/* Grey world */
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.01;			/* 1% glare */
		}
	}
	else if (no == 1
	 || (as != NULL && stricmp(as,"pe") == 0)) {

		no = 1;
		if (vc != NULL) {
			vc->desc = " pe - Print evaluation environment (CIE 116-1995)";
			vc->Ev = vc_none;		/* Use explicit La/Lv */
			vc->La = 30.0;			/* 0.2 * Lv ? */ 
			vc->Lv = 150.0;			/* White of the image field */ 
			vc->Yb = 0.2;			/* Grey world */
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.01;			/* 1% glare */
		}
	}
	else if (no == 4
	 || (as != NULL && stricmp(as,"mb") == 0)) {

		no = 4;
		if (vc != NULL) {
			vc->desc = " mb - Bright monitor in bright work environment";
			vc->Ev = vc_none;		/* Use explicit La/Lv */
			vc->La = 42.0;			/* Bright work environment */
			vc->Lv = 150.0;			/* White of the image field */ 
			vc->Yb = 0.2;			/* Grey world */
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.01;			/* 1% glare */
		}
	}
	else if (no == 3
	 || (as != NULL && stricmp(as,"mt") == 0)) {

		no = 3;
		if (vc != NULL) {
			vc->desc = " mt - Monitor in typical work environment";
			vc->Ev = vc_none;		/* Use explicit La/Lv */
			vc->La = 22.0;			/* Typical work environment */
			vc->Lv = 120.0;			/* White of the image field */ 
			vc->Yb = 0.2;			/* Grey world */
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.01;			/* 1% glare */
		}
	}
	else if (no == 5
	 || (as != NULL && stricmp(as,"md") == 0)) {

		no = 5;
		if (vc != NULL) {
			vc->desc = " md - Monitor in darkened work environment";
			vc->Ev = vc_none;		/* Use explicit La/Lv */
			vc->La = 10.0;			/* Darkened work environment */
			vc->Lv = 100.0;			/* White of the image field */ 
			vc->Yb = 0.2;			/* Grey world */
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.01;			/* 1% glare */
		}
	}
	else if (no == 6
	 || (as != NULL && stricmp(as,"jm") == 0)) {

		no = 6;
		if (vc != NULL) {
			vc->desc = " jm - Projector in dim environment";
			vc->Ev = vc_none;		/* Use explicit La/Lv */
			vc->La = 10.0;			/* Adaptation is from display */
			vc->Lv = 80.0;			/* White of the image field */ 
			vc->Yb = 0.2;			/* Grey world */
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.01;			/* 1% glare */
		}
	}
	else if (no == 7
	 || (as != NULL && stricmp(as,"jd") == 0)) {

		no = 7;
		if (vc != NULL) {
			vc->desc = " jd - Projector in dark environment";
			vc->Ev = vc_none;		/* Use explicit La/Lv */
			vc->La = 8.0;			/* Adaptation is from display */
			vc->Lv = 80.0;			/* White of the image field */ 
			vc->Yb = 0.2;			/* Grey world */
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.01;			/* 1% glare */
		}
	}
	else if (no == 8
	 || (as != NULL && stricmp(as,"tv") == 0)) {

		no = 8;
		if (vc != NULL) {
			vc->desc = " tv - Television/Film Studio";
			vc->Ev = vc_none;				/* Compute from La/Lv */
			vc->La = 0.2 * 1000.0/3.1415;	/* Adative/Surround */
			vc->Yb = 0.2;					/* Grey world */
			vc->Lv = 1000.0/3.1415;			/* White of the image field */ 
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.01;			/* 1% glare */
		}
	}
	else if (no == 9
	 || (as != NULL && stricmp(as,"pcd") == 0)) {

		no = 9;
		if (vc != NULL) {
			vc->desc = "pcd - Photo CD - original scene outdoors";
			vc->Ev = vc_average;	/* Average viewing conditions */
			vc->La = 320.0;			/* Typical outdoors, 1600 cd/m^2 */
			vc->Yb = 0.2;			/* Grey world */
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.0;			/* 0% glare - assumed to be compensated ? */
		}
	}
	else if (no == 10
	 || (as != NULL && stricmp(as,"ob") == 0)) {

		no = 10;
		if (vc != NULL) {
			vc->desc = " ob - Original scene - Bright Outdoors";
			vc->Ev = vc_average;	/* Average viewing conditions */
			vc->La = 2000.0;		/* Bright Outdoors */
			vc->Yb = 0.2;			/* Grey world */
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.0;			/* 0% glare - assumed to be compensated ? */
		}
	}
	else if (no == 11
	 || (as != NULL && stricmp(as,"cx") == 0)) {

		no = 11;
		if (vc != NULL) {
			vc->desc = " cx - Cut Sheet Transparencies on a viewing box";
			vc->Ev = vc_cut_sheet;	/* Cut sheet viewing conditions */
			vc->La = 53.0;			/* Dim, adapted to slide ? */
			vc->Yb = 0.2;			/* Grey world */
			vc->Yf = 0.0;			/* 0% flare */
			vc->Yg = 0.01;			/* 1% glare */
		}
	}
	else {
		if (p != NULL) {
			sprintf(p->err,"Enum VC: Unrecognised enumeration %d",no);
			p->errc = 1;
		}
		return -999;
	}

	return no;
}

/* Debug: dump a Viewing Condition to standard out */
void xicc_dump_viewcond(
icxViewCond *vc
) {
	printf("Viewing Condition:\n");
	if (vc->Ev == vc_dark)
		printf("  Surround to Image: Dark\n");
	else if (vc->Ev == vc_dim)
		printf("  Surround to Image: Dim\n");
	else if (vc->Ev == vc_average)
		printf("  Surround to Image: Average\n");
	else if (vc->Ev == vc_cut_sheet)
		printf("  Transparency on Light box\n");
	printf("  Adapted white = %f %f %f\n",vc->Wxyz[0], vc->Wxyz[1], vc->Wxyz[2]);
	printf("  Adapted luminance = %f cd/m^2\n",vc->La);
	printf("  Background to image ratio = %f\n",vc->Yb);
	if (vc->Ev == vc_none)
		printf("  Image luminance = %f cd/m^2\n",vc->Lv);
	printf("  Flare to image ratio = %f\n",vc->Yf);
	printf("  Glare to adapting/surround ratio = %f\n",vc->Yg);
	printf("  Flare color = %f %f %f\n",vc->Gxyz[0], vc->Gxyz[1], vc->Gxyz[2]);
}


/* Debug: dump an Inking setup to standard out */
void xicc_dump_inking(icxInk *ik) {
	printf("Inking settings:\n");
	if (ik->tlimit < 0.0)
		printf("No total limit\n");
	else
		printf("Total limit = %f%%\n",ik->tlimit * 100.0);

	if (ik->klimit < 0.0)
		printf("No black limit\n");
	else
		printf("Black limit = %f%%\n",ik->klimit * 100.0);

	if (ik->KonlyLmin)
		printf("K only black as locus Lmin\n");
	else
		printf("Normal black as locus Lmin\n");

	if (ik->k_rule == icxKvalue) {
		printf("Inking rule is a fixed K target\n");
	} if (ik->k_rule == icxKlocus) {
		printf("Inking rule is a fixed locus target\n");
	} if (ik->k_rule == icxKluma5 || ik->k_rule == icxKluma5k) {
		if (ik->k_rule == icxKluma5)
			printf("Inking rule is a 5 parameter locus function of L\n");
		else
			printf("Inking rule is a 5 parameter K function of L\n");
		printf("Ksmth = %f\n",ik->c.Ksmth);
		printf("Kskew = %f\n",ik->c.Kskew);
		printf("Kstle = %f\n",ik->c.Kstle);
		printf("Kstpo = %f\n",ik->c.Kstpo);
		printf("Kenpo = %f\n",ik->c.Kenpo);
		printf("Kenle = %f\n",ik->c.Kenle);
		printf("Kshap = %f\n",ik->c.Kshap);
	} if (ik->k_rule == icxKl5l || ik->k_rule == icxKl5lk) {
		if (ik->k_rule == icxKl5l)
			printf("Inking rule is a 2x5 parameter locus function of L and K aux\n");
		else
			printf("Inking rule is a 2x5 parameter K function of L and K aux\n");
		printf("Min Ksmth = %f\n",ik->c.Ksmth);
		printf("Min Kskew = %f\n",ik->c.Kskew);
		printf("Min Kstle = %f\n",ik->c.Kstle);
		printf("Min Kstpo = %f\n",ik->c.Kstpo);
		printf("Min Kenpo = %f\n",ik->c.Kenpo);
		printf("Min Kenle = %f\n",ik->c.Kenle);
		printf("Min Kshap = %f\n",ik->c.Kshap);
		printf("Max Ksmth = %f\n",ik->x.Ksmth);
		printf("Max Kskew = %f\n",ik->x.Kskew);
		printf("Max Kstle = %f\n",ik->x.Kstle);
		printf("Max Kstpo = %f\n",ik->x.Kstpo);
		printf("Max Kenpo = %f\n",ik->x.Kenpo);
		printf("Max Kenle = %f\n",ik->x.Kenle);
		printf("Max Kshap = %f\n",ik->x.Kshap);
	} 
}

/* ------------------------------------------------------ */
/* Gamut Mapping Intent stuff */

/* Return an enumerated gamut mapping intent */
/* Return enumeration if OK, icxIllegalGMIntent if there is no such enumeration. */
int xicc_enum_gmapintent(
icxGMappingIntent *gmi,	/* Gamut Mapping parameters to return */
int no,					/* Enumeration selected, icxNoGMIntent for none */
char *as				/* Alias string selector, NULL for none */
) {
#ifdef USE_CAM
	int colccas = 0x3;	/* Use abs. CAS for abs colorimetric intents */
	int perccas = 0x2;	/* Use CAS for other intents */
#else
	int colccas = 0x1;	/* Use abs. Lab for abs colorimetric intents */
	int perccas = 0x0;	/* Use Lab for other intents */
	fprintf(stderr,"!!!!!! Warning, USE_CAM is off in xicc.c !!!!!!\n");
#endif

	/* Assert default if no guidance given */
	if (no == icxNoGMIntent && as == NULL)
		no = icxDefaultGMIntent;

	if (no == 0
	 || no == icxAbsoluteGMIntent
	 || (as != NULL && stricmp(as,"a") == 0)) {
		/* Map Absolute appearance space Jab to Jab and clip out of gamut */
		no = 0;
		gmi->as = "a";
		gmi->desc = " a - Absolute Colorimetric (in Jab) [ICC Absolute Colorimetric]";
		gmi->icci = icAbsoluteColorimetric;
		gmi->usecas  = colccas;		/* Use absolute appearance space */
		gmi->usemap  = 0;			/* Don't use gamut mapping */
		gmi->greymf  = 0.0;
		gmi->glumwcpf = 0.0;
		gmi->glumwexf = 0.0;
		gmi->glumbcpf = 0.0;
		gmi->glumbexf = 0.0;
		gmi->glumknf = 0.0;
		gmi->bph = gmm_noBPadpt;	/* No BP adapation */
		gmi->gamcpf  = 0.0;
		gmi->gamexf  = 0.0;
		gmi->gamcknf  = 0.0;
		gmi->gamxknf  = 0.0;
		gmi->gampwf  = 0.0;
		gmi->gamswf  = 0.0;
		gmi->satenh  = 0.0;			/* No saturation enhancement */
	}
	else if (no == 1
	 || (as != NULL && stricmp(as,"aw") == 0)) {

		/* I'm not sure how often this intent is useful. It's less likely than */
		/* I though that a printer white point won't fit within the gamut */
		/* of a display profile, since the display white always has Y = 1.0, */
		/* and no paper has better than about 95% reflectance. */
		/* Perhaps it may be more useful for targeting printer profiles ? */

		/* Map Absolute Jab to Jab and scale source to avoid clipping the white point */
		no = 1;
		gmi->as = "aw";
		gmi->desc = "aw - Absolute Colorimetric (in Jab) with scaling to fit white point";
		gmi->icci = icAbsoluteColorimetric;
		gmi->usecas = 0x100 | colccas;	/* Absolute Appearance space with scaling */
									/* to avoid clipping the source white point */
		gmi->usemap  = 0;			/* Don't use gamut mapping */
		gmi->greymf  = 0.0;
		gmi->glumwcpf = 0.0;
		gmi->glumwexf = 0.0;
		gmi->glumbcpf = 0.0;
		gmi->glumbexf = 0.0;
		gmi->glumknf = 0.0;
		gmi->bph = gmm_noBPadpt;	/* No BP adapation */
		gmi->gamcpf  = 0.0;
		gmi->gamexf  = 0.0;
		gmi->gamcknf  = 0.0;
		gmi->gamxknf  = 0.0;
		gmi->gampwf  = 0.0;
		gmi->gamswf  = 0.0;
		gmi->satenh  = 0.0;			/* No saturation enhancement */
	}
	else if (no == 2
	 || (as != NULL && stricmp(as,"aa") == 0)) {

		/* Map appearance space Jab to Jab and clip out of gamut */
		no = 2;
		gmi->as = "aa";
		gmi->desc = "aa - Absolute Appearance";
		gmi->icci = icRelativeColorimetric;
		gmi->usecas  = perccas;		/* Appearance space */
		gmi->usemap  = 0;			/* Don't use gamut mapping */
		gmi->greymf  = 0.0;
		gmi->glumwcpf = 0.0;
		gmi->glumwexf = 0.0;
		gmi->glumbcpf = 0.0;
		gmi->glumbexf = 0.0;
		gmi->glumknf = 0.0;
		gmi->bph = gmm_noBPadpt;	/* No BP adapation */
		gmi->gamcpf  = 0.0;
		gmi->gamexf  = 0.0;
		gmi->gamcknf  = 0.0;
		gmi->gamxknf  = 0.0;
		gmi->gampwf  = 0.0;
		gmi->gamswf  = 0.0;
		gmi->satenh  = 0.0;			/* No saturation enhancement */
	}
	else if (no == 3
	 || no == icxRelativeGMIntent
	 || (as != NULL && stricmp(as,"r") == 0)) {

		/* Align neutral axes and linearly map white point, then */
		/* map appearance space Jab to Jab and clip out of gamut */
		no = 3;
		gmi->as = "r";
		gmi->desc = " r - White Point Matched Appearance [ICC Relative Colorimetric]";
		gmi->icci = icRelativeColorimetric;
		gmi->usecas  = perccas;		/* Appearance space */
		gmi->usemap  = 1;			/* Use gamut mapping */
		gmi->greymf  = 1.0;			/* Fully align grey axis */
		gmi->glumwcpf = 1.0;		/* Fully compress grey axis at white end */
		gmi->glumwexf = 1.0;		/* Fully expand grey axis at white end */
		gmi->glumbcpf = 0.0;		/* No compression at black end */
		gmi->glumbexf = 0.0;		/* No expansion at black end */
		gmi->glumknf = 0.0;
		gmi->bph = gmm_noBPadpt;	/* No BP adapation */
		gmi->gamcpf  = 0.0;
		gmi->gamexf  = 0.0;
		gmi->gamcknf  = 0.0;
		gmi->gamxknf  = 0.0;
		gmi->gampwf  = 0.0;
		gmi->gamswf  = 0.0;
		gmi->satenh  = 0.0;			/* No saturation enhancement */
	}
	else if (no == 4
	 || (as != NULL && stricmp(as,"la") == 0)) {

		/* Align neutral axes and linearly map white and black points, then */
		/* map appearance space Jab to Jab and clip out of gamut */
		no = 4;
		gmi->as = "la";
		gmi->desc = "la - Luminance axis matched Appearance";
		gmi->icci = icRelativeColorimetric;
		gmi->usecas  = perccas;		/* Appearance space */
		gmi->usemap  = 1;			/* Use gamut mapping */
		gmi->greymf  = 1.0;			/* Fully align grey axis */
		gmi->glumwcpf = 1.0;		/* Fully compress grey axis at white end */
		gmi->glumwexf = 1.0;		/* Fully expand grey axis at white end */
		gmi->glumbcpf = 1.0;		/* Fully compress grey axis at black end */
		gmi->glumbexf = 1.0;		/* Fully expand grey axis at black end */
		gmi->glumknf = 0.0;			/* No knee on grey mapping */
		gmi->bph = gmm_bendBP;		/* extent and bend */
		gmi->gamcpf  = 0.0;			/* No gamut compression */
		gmi->gamexf  = 0.0;			/* No gamut expansion */
		gmi->gamcknf  = 0.0;		/* No knee in gamut compress */
		gmi->gamxknf  = 0.0;		/* No knee in gamut expand */
		gmi->gampwf  = 0.0;			/* No Perceptual surface weighting factor */
		gmi->gamswf  = 0.0;			/* No Saturation surface weighting factor */
		gmi->satenh  = 0.0;			/* No saturation enhancement */
	}
	else if (no == 5
	 || no == icxDefaultGMIntent
	 || no == icxPerceptualGMIntent
	 || (as != NULL && stricmp(as,"p") == 0)) {

		/* Align neutral axes and perceptually map white and black points, */ 
		/* perceptually compress out of gamut and map appearance space Jab to Jab. */  
		no = 5;
		gmi->as = "p";
		gmi->desc = " p - Perceptual (Preferred) (Default) [ICC Perceptual]";
		gmi->icci = icPerceptual;
		gmi->usecas  = perccas;		/* Appearance space */
		gmi->usemap  = 1;			/* Use gamut mapping */
		gmi->greymf  = 1.0;			/* Fully align grey axis */
		gmi->glumwcpf = 1.0;		/* Fully compress grey axis at white end */
		gmi->glumwexf = 1.0;		/* Fully expand grey axis at white end */
		gmi->glumbcpf = 1.0;		/* Fully compress grey axis at black end */
		gmi->glumbexf = 1.0;		/* Fully expand grey axis at black end */
		gmi->glumknf = 1.0;			/* Sigma knee in grey compress/expand */
		gmi->bph = gmm_bendBP;		/* extent and bend */
		gmi->gamcpf  = 1.0;			/* Full gamut compression */
		gmi->gamexf  = 0.0;			/* No gamut expansion */
		gmi->gamcknf  = 0.9;		/* 0.9 High Sigma knee in gamut compress */
		gmi->gamxknf  = 0.0;		/* No knee in gamut expand */
		gmi->gampwf  = 1.0;			/* Full Perceptual surface weighting factor */
		gmi->gamswf  = 0.0;			/* No Saturation surface weighting factor */
		gmi->satenh  = 0.0;			/* No saturation enhancement */
	}
	else if (no == 6
	 || (as != NULL && stricmp(as,"pa") == 0)) {

		/* Don't align neutral axes, but perceptually compress out of gamut */
		/* and map appearance space Jab to Jab. */  
		no = 6;
		gmi->as = "pa";
		gmi->desc = "pa - Perceptual Apperance ";
		gmi->icci = icPerceptual;
		gmi->usecas  = perccas;		/* Appearance space */
		gmi->usemap  = 1;			/* Use gamut mapping */
		gmi->greymf  = 0.0;			/* Don't align grey axis */
		gmi->glumwcpf = 1.0;		/* Fully compress grey axis at white end */
		gmi->glumwexf = 1.0;		/* Fully expand grey axis at white end */
		gmi->glumbcpf = 1.0;		/* Fully compress grey axis at black end */
		gmi->glumbexf = 1.0;		/* Fully expand grey axis at black end */
		gmi->glumknf = 1.0;			/* Sigma knee in grey compress/expand */
		gmi->bph = gmm_bendBP;		/* extent and bend */
		gmi->gamcpf  = 1.0;			/* Full gamut compression */
		gmi->gamexf  = 0.0;			/* No gamut expansion */
		gmi->gamcknf  = 0.9;		/* 0.9 High Sigma knee in gamut compress */
		gmi->gamxknf  = 0.0;		/* No knee in gamut expand */
		gmi->gampwf  = 1.0;			/* Full Perceptual surface weighting factor */
		gmi->gamswf  = 0.0;			/* No Saturation surface weighting factor */
		gmi->satenh  = 0.0;			/* No saturation enhancement */
	}
	else if (no == 7
	 || (as != NULL && stricmp(as,"ms") == 0)) {

		/* Align neutral axes and perceptually map white and black points, */ 
		/* perceptually compress and expand to match gamuts and map Jab to Jab. */  
		no = 7;
		gmi->as = "ms";
		gmi->desc = "ms - Saturation";
		gmi->icci = icSaturation;
		gmi->usecas  = perccas;		/* Appearance space */
		gmi->usemap  = 1;			/* Use gamut mapping */
		gmi->greymf  = 1.0;			/* Fully align grey axis */
		gmi->glumwcpf = 1.0;		/* Fully compress grey axis at white end */
		gmi->glumwexf = 1.0;		/* Fully expand grey axis at white end */
		gmi->glumbcpf = 1.0;		/* Fully compress grey axis at black end */
		gmi->glumbexf = 1.0;		/* Fully expand grey axis at black end */
		gmi->glumknf = 1.0;			/* Sigma knee in grey compress/expand */
		gmi->bph = gmm_bendBP;		/* extent and bend */
		gmi->gamcpf  = 1.0;			/* Full gamut compression */
		gmi->gamexf  = 1.0;			/* Full gamut expansion  */
		gmi->gamcknf = 1.0;			/* High Sigma knee in gamut compress/expand */
		gmi->gamxknf = 0.4;			/* Moderate Sigma knee in gamut compress/expand */
		gmi->gampwf  = 0.2;			/* Slight perceptual surface weighting factor */
		gmi->gamswf  = 0.8;			/* Most saturation surface weighting factor */
		gmi->satenh  = 0.0;			/* No saturation enhancement */
	}
	else if (no == 8
	 || no == icxSaturationGMIntent
	 || (as != NULL && stricmp(as,"s") == 0)) {

		/* Same as "ms" but enhance saturation */
		no = 8;
		gmi->as = "s";
		gmi->desc = " s - Enhanced Saturation [ICC Saturation]";
		gmi->icci = icSaturation;
		gmi->usecas  = perccas;		/* Appearance space */
		gmi->usemap  = 1;			/* Use gamut mapping */
		gmi->greymf  = 1.0;			/* Fully align grey axis */
		gmi->glumwcpf = 1.0;		/* Fully compress grey axis at white end */
		gmi->glumwexf = 1.0;		/* Fully expand grey axis at white end */
		gmi->glumbcpf = 1.0;		/* Fully compress grey axis at black end */
		gmi->glumbexf = 1.0;		/* Fully expand grey axis at black end */
		gmi->glumknf = 1.0;			/* Sigma knee in grey compress/expand */
		gmi->bph = gmm_bendBP;		/* extent and bend */
		gmi->gamcpf  = 1.0;			/* Full gamut compression */
		gmi->gamexf  = 1.0;			/* Full gamut expansion */
		gmi->gamcknf = 1.0;			/* High sigma knee in gamut compress */
		gmi->gamxknf = 0.5;			/* Moderate sigma knee in gamut expand */
		gmi->gampwf  = 0.0;			/* No Perceptual surface weighting factor */
		gmi->gamswf  = 1.0;			/* Full Saturation surface weighting factor */
		gmi->satenh  = 0.9;			/* Medium saturation enhancement */
	}
	else if (no == 9
	 || (as != NULL && stricmp(as,"al") == 0)) {

		/* Map absolute L*a*b* to L*a*b* and clip out of gamut */
		no = 9;
		gmi->as = "al";
		gmi->desc = "al - Absolute Colorimetric (Lab)";
		gmi->icci = icAbsoluteColorimetric;
		gmi->usecas  = 0x1;			/* Don't use appearance space, use abs. L*a*b* */
		gmi->usemap  = 0;			/* Don't use gamut mapping */
		gmi->greymf  = 0.0;
		gmi->glumwcpf = 0.0;
		gmi->glumwexf = 0.0;
		gmi->glumbcpf = 0.0;
		gmi->glumbexf = 0.0;
		gmi->glumknf = 0.0;
		gmi->bph = gmm_noBPadpt;	/* No BP adapation */
		gmi->gamcpf  = 0.0;
		gmi->gamexf  = 0.0;
		gmi->gamcknf = 0.0;
		gmi->gamxknf = 0.0;
		gmi->gampwf  = 0.0;
		gmi->gamswf  = 0.0;
		gmi->satenh  = 0.0;			/* No saturation enhancement */
	}
	else if (no == 10
	 || (as != NULL && stricmp(as,"rl") == 0)) {

		/* Align neutral axes and linearly map white point, then */
		/* map L*a*b* to L*a*b* and clip out of gamut */
		no = 10;
		gmi->as = "rl";
		gmi->desc = "rl - White Point Matched Colorimetric (Lab)";
		gmi->icci = icRelativeColorimetric;
		gmi->usecas  = 0x0;			/* Don't use appearance space, use relative L*a*b* */
		gmi->usemap  = 1;			/* Use gamut mapping */
		gmi->greymf  = 1.0;			/* And linearly map white point */
		gmi->glumwcpf = 1.0;
		gmi->glumwexf = 1.0;
		gmi->glumbcpf = 0.0;
		gmi->glumbexf = 0.0;
		gmi->glumknf = 0.0;
		gmi->bph = gmm_noBPadpt;	/* No BP adapation */
		gmi->gamcpf  = 0.0;
		gmi->gamexf  = 0.0;
		gmi->gamcknf = 0.0;
		gmi->gamxknf = 0.0;
		gmi->gampwf  = 0.0;
		gmi->gamswf  = 0.0;
		gmi->satenh  = 0.0;			/* No saturation enhancement */
	}
	else {	/* icxIllegalGMIntent */
		return icxIllegalGMIntent;
	}

	return no;
}


/* Debug: dump a Gamut Mapping specification */
void xicc_dump_gmi(
icxGMappingIntent *gmi	/* Gamut Mapping parameters to return */
) {
	printf(" Gamut Mapping Specification:\n");
	if (gmi->desc != NULL)
		printf("  Description = '%s'\n",gmi->desc);
	printf("  Closest ICC intent = '%s'\n",icm2str(icmRenderingIntent,gmi->icci));

	if ((gmi->usecas & 0xff) == 0)
		printf("  Not using Color Apperance Space - using L*a*b*\n");
	else if ((gmi->usecas & 0xff) == 1)
		printf("  Not using Color Apperance Space - using Absoute L*a*b*\n");
	else if ((gmi->usecas & 0xff) == 2)
		printf("  Using Color Apperance Space\n");
	else if ((gmi->usecas & 0xff) == 3)
		printf("  Using Absolute Color Apperance Space\n");

	if ((gmi->usecas & 0x100) != 0)
		printf("  Scaling source to avoid white point clipping\n");

	if (gmi->usemap == 0)
		printf("  Not using Mapping\n");
	else {
		printf("  Using Mapping with parameters:\n");
		printf("  Grey axis alignment   factor %f\n", gmi->greymf);
		printf("  Grey axis white compression factor %f\n", gmi->glumwcpf);
		printf("  Grey axis white expansion   factor %f\n", gmi->glumwexf);
		printf("  Grey axis black compression factor %f\n", gmi->glumbcpf);
		printf("  Grey axis black expansion   factor %f\n", gmi->glumbexf);
		printf("  Grey axis knee        factor %f\n", gmi->glumknf);
		printf("  Black point algorithm: ");
		switch(gmi->bph) {
			case gmm_clipBP:	printf("Neutral axis no-adapt extend and clip\n"); break;
			case gmm_BPadpt:	printf("Neutral axis fully adapt\n"); break;
			case gmm_bendBP:	printf("Neutral axis no-adapt extend and bend\n"); break;
			case gmm_noBPadpt:	printf("Neutral axis no-adapt\n"); break;
		}
		printf("  Gamut compression factor %f\n", gmi->gamcpf);
		printf("  Gamut expansion   factor %f\n", gmi->gamexf);
		printf("  Gamut compression knee factor %f\n", gmi->gamcknf);
		printf("  Gamut expansion   knee factor %f\n", gmi->gamxknf);
		printf("  Gamut Perceptual mapping weighting factor %f\n", gmi->gampwf);
		printf("  Gamut Saturation mapping weighting factor %f\n", gmi->gamswf);
		printf("  Saturation enhancement factor %f\n", gmi->satenh);
	}
}

/* ------------------------------------------------------ */
/* Turn xicc xcal into limit calibration callback */

/* Given an icc profile, try and create an xcal */
/* Return NULL on error or no cal */
xcal *xiccReadCalTag(icc *p) {
	xcal *cal = NULL;
	icTagSignature sig = icmMakeTag('t','a','r','g');
	icmText *ro;
	int oi, tab;

//printf("~1 about to look for CAL in profile\n");
	if ((ro = (icmText *)p->read_tag(p, sig)) != NULL) { 
		cgatsFile *cgf;
		cgats *icg;

		if (ro->ttype != icSigTextType)
			return NULL;
	
//printf("~1 found 'targ' tag\n");
		if ((icg = new_cgats()) == NULL) {
			return NULL;
		}
		if ((cgf = new_cgatsFileMem(ro->data, ro->size)) != NULL) {
			icg->add_other(icg, "CTI3");
			oi = icg->add_other(icg, "CAL");

//printf("~1 created cgats object from 'targ' tag\n");
			if (icg->read(icg, cgf) == 0) {

				for (tab = 0; tab < icg->ntables; tab++) {
					if (icg->t[tab].tt == tt_other && icg->t[tab].oi == oi) {
						break;
					}
				}
				if (tab < icg->ntables) {
//printf("~1 found CAL table\n");
		
					if ((cal = new_xcal()) == NULL) {
						icg->del(icg);
						cgf->del(cgf);
						return NULL;
					}
					if (cal->read_cgats(cal, icg, tab, "'targ' tag") != 0)  {
#ifdef DEBUG
						printf("read_cgats on cal tag failed\n");
#endif
						cal->del(cal);
						cal = NULL;
					}
//else printf("~1 read CAL and creaded xcal object OK\n");
				}
			}
			cgf->del(cgf);
		}
		icg->del(icg);
	}
	return cal;
}

/* A callback that uses an xcal, that can be used with icc get_tac */
void xiccCalCallback(void *cntx, double *out, double *in) {
	xcal *cal = (xcal *)cntx;

	cal->interp(cal, out, in);
}

/* ---------------------------------------------- */

/* Utility function - given an open icc profile, */
/* guess which channel is the black. */
/* Return -1 if there is no black channel or it can't be guessed */
int icxGuessBlackChan(icc *p) {
	int kch = -1;

	switch (p->header->colorSpace) {
   	 	case icSigCmykData:
			kch = 3;
			break;

		/* Use a heuristic to detect the black channel. */
		/* This duplicates code in icxLu_comp_bk_point() :-( */ 
		/* Colorant guessing should go in icclib ? */
		case icSig2colorData:
		case icSig3colorData:
		case icSig4colorData:
		case icSig5colorData:
		case icSig6colorData:
		case icSig7colorData:
		case icSig8colorData:
		case icSig9colorData:
		case icSig10colorData:
		case icSig11colorData:
		case icSig12colorData:
		case icSig13colorData:
		case icSig14colorData:
		case icSig15colorData:
		case icSigMch5Data:
		case icSigMch6Data:
		case icSigMch7Data:
		case icSigMch8Data: {
				icmLuBase *lu;
				double dval[MAX_CHAN];
				double ncval[3];
				double cvals[MAX_CHAN][3];
				int inn, e, nlighter, ndarker;

				/* Grab a lookup object */
				if ((lu = p->get_luobj(p, icmFwd, icRelativeColorimetric, icSigLabData, icmLuOrdNorm)) == NULL)
					error("icxGetLimits: assert: getting Fwd Lookup failed!");

				lu->spaces(lu, NULL, &inn, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

				/* Decide if the colorspace is aditive or subtractive */

				/* First the no colorant value */
				for (e = 0; e < inn; e++)
					dval[e] = 0.0;
				lu->lookup(lu, ncval, dval);

				/* Then all the colorants */
				nlighter = ndarker = 0;
				for (e = 0; e < inn; e++) {
					dval[e] = 1.0;
					lu->lookup(lu, cvals[e], dval);
					dval[e] = 0.0;
					if (fabs(cvals[e][0] - ncval[0]) > 5.0) {
						if (cvals[e][0] > ncval[0])
							nlighter++;
						else
							ndarker++;
					}
				}

				if (ndarker > 0 && nlighter == 0) {		/* Assume subtractive. */
					double pbk[3] = { 0.0,0.0,0.0 };	/* Perfect black */
					double smd = 1e10;			/* Smallest distance */

					/* Guess the black channel */
					for (e = 0; e < inn; e++) {
						double tt;
						tt = icmNorm33sq(pbk, cvals[e]);
						if (tt < smd) {
							smd = tt;	
							kch = e;
						}
					}
					/* See if the black seems sane */
					if (cvals[kch][0] > 40.0
					 || fabs(cvals[kch][1]) > 10.0
					 || fabs(cvals[kch][2]) > 10.0) {
						kch = -1;
					}
				}
				lu->del(lu);
			}
			break;

		default:
			break;
	}

	return kch;
}

/* Utility function - given an open icc profile, */
/* estmate the total ink limit and black ink limit. */
/* Note that this is rather rough, because ICC profiles */
/* don't have a tag for this information, and ICC profiles */
/* don't have any straightforward way of identifying particular */
/* color channels for > 4 color. */
/* If there are no limits, or they are not discoverable or */
/* applicable, return values of -1.0 */

void icxGetLimits(
xicc *xp,
double *tlimit,
double *klimit 
) {
	icc *p = xp->pp;
	int nch;
	double max[MAX_CHAN];		/* Max of each channel */
	double total;

	total = p->get_tac(p, max, xp->cal != NULL ? xiccCalCallback : NULL, (void *)xp->cal);

	if (total < 0.0) {	/* Not valid */
		if (tlimit != NULL)
			*tlimit = -1.0;
		if (klimit != NULL)
			*klimit = -1.0;
		return;
	}

	nch = icmCSSig2nchan(p->header->colorSpace);

	/* No effective limit */
	if (tlimit != NULL) {
		if (total >= (double)nch) {
			*tlimit = -1.0;
		} else {
			*tlimit = total;
		}
	}

	if (klimit != NULL) {
		int kch;

		kch = icxGuessBlackChan(p);

		if (kch < 0 || max[kch] >= 1.0) {
			*klimit = -1.0;
		} else {
			*klimit = max[kch];
		}
	}
}

/* Replace a non-set limit (ie. < 0.0) with the heuristic from */
/* the given profile. */
void icxDefaultLimits(
xicc *xp,
double *tlout,
double tlin,
double *klout,
double klin
) {
	if (tlin < 0.0 || klin < 0.0) { 
		double tl, kl;

		icxGetLimits(xp, &tl, &kl);

		if (tlin < 0.0)
			tlin = tl;

		if (klin < 0.0)
			klin = kl;
	}

	if (tlout != NULL)
		*tlout = tlin;

	if (klout != NULL)
		*klout = klin;
}

/* Structure to hold optimisation information */
typedef struct {
	xcal *cal;
	double ilimit;
	double uilimit;
} ulimctx;

/* Callback to find equivalent underlying total limit */
/* and try and maximize it while remaining within gamut */
static double ulimitfunc(void *cntx, double pv[]) {
	ulimctx *cx = (ulimctx *)cntx;
	xcal *cal = cx->cal;
	int devchan = cal->devchan;
	int i;
	double dv, odv;
	double og = 0.0, rv = 0.0;

	double usum = 0.0, sum = 0.0;

	/* Comute calibrated sum of channels except last */
	for (i = 0; i < (devchan-1); i++) {
		double dv = pv[i];		/* Underlying (pre-calibration) device value */
		usum += dv;				/* Underlying sum */
		if (dv < 0.0) {
			og += -dv;
			dv = 0.0;
		} else if (dv > 1.0) {
			og += dv - 1.0;
			dv = 1.0;
		} else 
			dv = cal->interp_ch(cal, i, dv);	/* Calibrated device value */
		sum += dv;							/* Calibrated device sum */
	}
	/* Compute the omitted channel value */
	dv = cx->ilimit - sum;					/* Omitted calibrated device value */
	if (dv < 0.0) {
		og += -dv;
		dv = 0.0;
	} else if (dv > 1.0) {
		og += dv - 1.0;
		dv = 1.0;
	} else 
		dv = cal->inv_interp_ch(cal, i, dv);	/* Omitted underlying device value */
	usum += dv;								/* Underlying sum */
	cx->uilimit = usum; 
	
	rv = 10000.0 * og - usum;		/* Penalize out of gamut, maximize underlying sum */

//printf("~1 returning %f from %f %f %f %f\n",rv,pv[0],pv[1],pv[2],dv);
	return rv;
}

/* Given a calibrated total ink limit and an xcal, return the */
/* equivalent underlying (pre-calibration) total ink limit. */
/* This is the maximum equivalent, that makes sure that */
/* the calibrated limit is met or exceeded. */
double icxMaxUnderlyingLimit(xcal *cal, double ilimit) {
	ulimctx cx;
	int i;
	double dv[MAX_CHAN];
	double sr[MAX_CHAN];
	double rv;				/* Residual value */

	if (cal->devchan <= 1) {
		return cal->inv_interp_ch(cal, 0, ilimit);
	}

	cx.cal = cal;
	cx.ilimit = ilimit;

	for (i = 0; i < (cal->devchan-1); i++) {
		sr[i] = 0.05;
		dv[i] = 0.1;
	}
	if (powell(&rv, cal->devchan-1, dv, sr, 0.000001, 1000, ulimitfunc,
			                      (void *)&cx, NULL, NULL) != 0) {
		warning("icxUnderlyingLimit() failed for chan %d, ilimit %f\n",cal->devchan,ilimit);
		return ilimit;
	}
	ulimitfunc((void *)&cx, dv);

	return cx.uilimit;
}

/* ------------------------------------------------------ */
/* Conversion and deltaE formular that include partial */
/* derivatives, for use within fit parameter optimisations. */

/* CIE XYZ to perceptual Lab with partial derivatives. */
void icxdXYZ2Lab(icmXYZNumber *w, double *out, double dout[3][3], double *in) {
	double wp[3], tin[3], dtin[3];
	int i;

	wp[0] = w->X, wp[1] = w->Y, wp[2] = w->Z;

	for (i = 0; i < 3; i++) {
		tin[i] = in[i]/wp[i];
		dtin[i] = 1.0/wp[i];

		if (tin[i] > 0.008856451586) {
			dtin[i] *= pow(tin[i], -2.0/3.0) / 3.0;
			tin[i] = pow(tin[i],1.0/3.0);
		} else {
			dtin[i] *= 7.787036979;
			tin[i] = 7.787036979 * tin[i] + 16.0/116.0;
		}
	}

	out[0] = 116.0 * tin[1] - 16.0;
	dout[0][0] = 0.0;
	dout[0][1] = 116.0 * dtin[1];
	dout[0][2] = 0.0;

	out[1] = 500.0 * (tin[0] - tin[1]);
	dout[1][0] = 500.0 * dtin[0];
	dout[1][1] = 500.0 * -dtin[1];
	dout[1][2] = 0.0;

	out[2] = 200.0 * (tin[1] - tin[2]);
	dout[2][0] = 0.0;
	dout[2][1] = 200.0 * dtin[1];
	dout[2][2] = 200.0 * -dtin[2];
}


/* Return the normal Delta E squared, given two Lab values, */
/* including partial derivatives. */
double icxdLabDEsq(double dout[2][3], double *Lab0, double *Lab1) {
	double rv = 0.0, tt;

	tt = Lab0[0] - Lab1[0];
	dout[0][0] =  2.0 * tt;
	dout[1][0] = -2.0 * tt;
	rv += tt * tt;
	tt = Lab0[1] - Lab1[1];
	dout[0][1] =  2.0 * tt;
	dout[1][1] = -2.0 * tt;
	rv += tt * tt;
	tt = Lab0[2] - Lab1[2];
	dout[0][2] =  2.0 * tt;
	dout[1][2] = -2.0 * tt;
	rv += tt * tt;
	return rv;
}

/* Return the CIE94 Delta E color difference measure, squared */
/* including partial derivatives. */
double icxdCIE94sq(double dout[2][3], double Lab0[3], double Lab1[3]) {
	double desq, _desq[2][3];
	double dlsq;
	double dcsq, _dcsq[2][2];	/* == [x][1,2] */
	double c12,   _c12[2][2];	/* == [x][1,2] */
	double dhsq, _dhsq[2][2];	/* == [x][1,2] */
	double rv;

	{
		double dl, da, db;

		dl = Lab0[0] - Lab1[0];
		dlsq = dl * dl;		/* dl squared */
		da = Lab0[1] - Lab1[1];
		db = Lab0[2] - Lab1[2];


		/* Compute normal Lab delta E squared */
		desq = dlsq + da * da + db * db;
		_desq[0][0] =  2.0 * dl;
		_desq[1][0] = -2.0 * dl;
		_desq[0][1] =  2.0 * da;
		_desq[1][1] = -2.0 * da;
		_desq[0][2] =  2.0 * db;
		_desq[1][2] = -2.0 * db;
	}

	{
		double c1, c2, dc, tt;

		/* Compute chromanance for the two colors */
		c1 = sqrt(Lab0[1] * Lab0[1] + Lab0[2] * Lab0[2]);
		c2 = sqrt(Lab1[1] * Lab1[1] + Lab1[2] * Lab1[2]);
		c12 = sqrt(c1 * c2);	/* Symetric chromanance */

		tt = 0.5 * (pow(c2, 0.5) + 1e-12)/(pow(c1, 1.5) + 1e-12);
		_c12[0][0] = Lab0[1] * tt;
		_c12[0][1] = Lab0[2] * tt;
		tt = 0.5 * (pow(c1, 0.5) + 1e-12)/(pow(c2, 1.5) + 1e-12);
		_c12[1][0] = Lab1[1] * tt;
		_c12[1][1] = Lab1[2] * tt;

		/* delta chromanance squared */
		dc = c2 - c1;
		dcsq = dc * dc;
		if (c1 < 1e-12 || c2 < 1e-12) {
			c1 += 1e-12;
			c2 += 1e-12;
		}
		_dcsq[0][0] = -2.0 * Lab0[1] * (c2 - c1)/c1;
		_dcsq[0][1] = -2.0 * Lab0[2] * (c2 - c1)/c1;
		_dcsq[1][0] =  2.0 * Lab1[1] * (c2 - c1)/c2;
		_dcsq[1][1] =  2.0 * Lab1[2] * (c2 - c1)/c2;
	}

	/* Compute delta hue squared */
	dhsq = desq - dlsq - dcsq;
	if (dhsq >= 0.0) {
		_dhsq[0][0] = _desq[0][1] - _dcsq[0][0];
		_dhsq[0][1] = _desq[0][2] - _dcsq[0][1];
		_dhsq[1][0] = _desq[1][1] - _dcsq[1][0];
		_dhsq[1][1] = _desq[1][2] - _dcsq[1][1];
	} else {
		dhsq = 0.0;
		_dhsq[0][0] = 0.0;
		_dhsq[0][1] = 0.0;
		_dhsq[1][0] = 0.0;
		_dhsq[1][1] = 0.0;
	}

	{
		double sc, scsq, scf;
		double sh, shsq, shf;

		/* Weighting factors for delta chromanance & delta hue */
		sc = 1.0 + 0.048 * c12;
		scsq = sc * sc;
		
		sh = 1.0 + 0.014 * c12;
		shsq = sh * sh;
	
		rv = dlsq + dcsq/scsq + dhsq/shsq;

		scf = 0.048 * -2.0 * dcsq/(scsq * sc);
		shf = 0.014 * -2.0 * dhsq/(shsq * sh);
		dout[0][0] = _desq[0][0];
		dout[0][1] = _dcsq[0][0]/scsq + _c12[0][0] * scf
		           + _dhsq[0][0]/shsq + _c12[0][0] * shf;
		dout[0][2] = _dcsq[0][1]/scsq + _c12[0][1] * scf
		           + _dhsq[0][1]/shsq + _c12[0][1] * shf;
		dout[1][0] = _desq[1][0];
		dout[1][1] = _dcsq[1][0]/scsq + _c12[1][0] * scf
		           + _dhsq[1][0]/shsq + _c12[1][0] * shf;
		dout[1][2] = _dcsq[1][1]/scsq + _c12[1][1] * scf
		           + _dhsq[1][1]/shsq + _c12[1][1] * shf;
		return rv;
	}
}

// ~~99 not sure if these are correct:

/* Return the normal Delta E given two Lab values, */
/* including partial derivatives. */
double icxdLabDE(double dout[2][3], double *Lab0, double *Lab1) {
	double rv = 0.0, tt;

	tt = Lab0[0] - Lab1[0];
	dout[0][0] =  1.0 * tt;
	dout[1][0] = -1.0 * tt;
	rv += tt * tt;
	tt = Lab0[1] - Lab1[1];
	dout[0][1] =  1.0 * tt;
	dout[1][1] = -1.0 * tt;
	rv += tt * tt;
	tt = Lab0[2] - Lab1[2];
	dout[0][2] =  1.0 * tt;
	dout[1][2] = -1.0 * tt;
	rv += tt * tt;
	return sqrt(rv);
}

/* Return the CIE94 Delta E color difference measure */
/* including partial derivatives. */
double icxdCIE94(double dout[2][3], double Lab0[3], double Lab1[3]) {
	double desq, _desq[2][3];
	double dlsq;
	double dcsq, _dcsq[2][2];	/* == [x][1,2] */
	double c12,   _c12[2][2];	/* == [x][1,2] */
	double dhsq, _dhsq[2][2];	/* == [x][1,2] */
	double rv;

	{
		double dl, da, db;

		dl = Lab0[0] - Lab1[0];
		dlsq = dl * dl;		/* dl squared */
		da = Lab0[1] - Lab1[1];
		db = Lab0[2] - Lab1[2];


		/* Compute normal Lab delta E squared */
		desq = dlsq + da * da + db * db;
		_desq[0][0] =  1.0 * dl;
		_desq[1][0] = -1.0 * dl;
		_desq[0][1] =  1.0 * da;
		_desq[1][1] = -1.0 * da;
		_desq[0][2] =  1.0 * db;
		_desq[1][2] = -1.0 * db;
	}

	{
		double c1, c2, dc, tt;

		/* Compute chromanance for the two colors */
		c1 = sqrt(Lab0[1] * Lab0[1] + Lab0[2] * Lab0[2]);
		c2 = sqrt(Lab1[1] * Lab1[1] + Lab1[2] * Lab1[2]);
		c12 = sqrt(c1 * c2);	/* Symetric chromanance */

		tt = 0.5 * (pow(c2, 0.5) + 1e-12)/(pow(c1, 1.5) + 1e-12);
		_c12[0][0] = Lab0[1] * tt;
		_c12[0][1] = Lab0[2] * tt;
		tt = 0.5 * (pow(c1, 0.5) + 1e-12)/(pow(c2, 1.5) + 1e-12);
		_c12[1][0] = Lab1[1] * tt;
		_c12[1][1] = Lab1[2] * tt;

		/* delta chromanance squared */
		dc = c2 - c1;
		dcsq = dc * dc;
		if (c1 < 1e-12 || c2 < 1e-12) {
			c1 += 1e-12;
			c2 += 1e-12;
		}
		_dcsq[0][0] = -1.0 * Lab0[1] * (c2 - c1)/c1;
		_dcsq[0][1] = -1.0 * Lab0[2] * (c2 - c1)/c1;
		_dcsq[1][0] =  1.0 * Lab1[1] * (c2 - c1)/c2;
		_dcsq[1][1] =  1.0 * Lab1[2] * (c2 - c1)/c2;
	}

	/* Compute delta hue squared */
	dhsq = desq - dlsq - dcsq;
	if (dhsq >= 0.0) {
		_dhsq[0][0] = _desq[0][1] - _dcsq[0][0];
		_dhsq[0][1] = _desq[0][2] - _dcsq[0][1];
		_dhsq[1][0] = _desq[1][1] - _dcsq[1][0];
		_dhsq[1][1] = _desq[1][2] - _dcsq[1][1];
	} else {
		dhsq = 0.0;
		_dhsq[0][0] = 0.0;
		_dhsq[0][1] = 0.0;
		_dhsq[1][0] = 0.0;
		_dhsq[1][1] = 0.0;
	}

	{
		double sc, scsq, scf;
		double sh, shsq, shf;

		/* Weighting factors for delta chromanance & delta hue */
		sc = 1.0 + 0.048 * c12;
		scsq = sc * sc;
		
		sh = 1.0 + 0.014 * c12;
		shsq = sh * sh;
	
		rv = dlsq + dcsq/scsq + dhsq/shsq;

		scf = 0.048 * -1.0 * dcsq/(scsq * sc);
		shf = 0.014 * -1.0 * dhsq/(shsq * sh);
		dout[0][0] = _desq[0][0];
		dout[0][1] = _dcsq[0][0]/scsq + _c12[0][0] * scf
		           + _dhsq[0][0]/shsq + _c12[0][0] * shf;
		dout[0][2] = _dcsq[0][1]/scsq + _c12[0][1] * scf
		           + _dhsq[0][1]/shsq + _c12[0][1] * shf;
		dout[1][0] = _desq[1][0];
		dout[1][1] = _dcsq[1][0]/scsq + _c12[1][0] * scf
		           + _dhsq[1][0]/shsq + _c12[1][0] * shf;
		dout[1][2] = _dcsq[1][1]/scsq + _c12[1][1] * scf
		           + _dhsq[1][1]/shsq + _c12[1][1] * shf;
		return sqrt(rv);
	}
}

/* ------------------------------------------------------ */
/* A power-like function, based on Graphics Gems adjustment curve. */
/* Avoids "toe" problem of pure power. */
/* Adjusted so that "power" 2 and 0.5 agree with real power at 0.5 */

double icx_powlike(double vv, double pp) {
	double tt, g;

	if (pp >= 1.0) {
		g = 2.0 * (pp - 1.0);
		vv = vv/(g - g * vv + 1.0);
	} else {
		g = 2.0 - 2.0/pp;
		vv = (vv - g * vv)/(1.0 - g * vv);
	}

	return vv;
}

/* Compute the necessary aproximate power, to transform */
/* the given value from src to dst. They are assumed to be */
/* in the range 0.0 .. 1.0 */
double icx_powlike_needed(double src, double dst) {
	double pp, g;

	if (dst <= src) {
		g = -((src - dst)/(dst * src - dst));
		pp = (0.5 * g) + 1.0;
	} else {
		g = -((src - dst)/((dst - 1.0) * src));
		pp = 1.0/(1.0 - 0.5 * g);
	}

	return pp;
}

/* ------------------------------------------------------ */
/* Parameterized transfer/dot gain function. */
/* Used for device modelling. Including partial */
/* derivative for input and parameters. */

/* NOTE that clamping the input values seems to cause */
/* conjgrad() problems. */

/* Transfer function */
double icxTransFunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
) {
	double g;
	int ord;

	/* Process all the shaper orders from low to high. */
	/* [These shapers were inspired by a Graphics Gem idea */
	/* (Gems IV, VI.3, "Fast Alternatives to Perlin's Bias and */
	/*  Gain Functions, pp 401). */
	/*  They have the nice properties that they are smooth, and */
	/*  are monotonic. The control parameter has been */
	/*  altered to have a range from -oo to +oo rather than 0.0 to 1.0 */
	/*  so that the search space is less non-linear. */
	for (ord = 0; ord < luord; ord++) {
		int nsec;			/* Number of sections */
		double sec;			/* Section */

		g = v[ord];			/* Parameter */

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

	return vv;
}

/* Inverse transfer function */
double icxInvTransFunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
) {
	double g;
	int ord;

	/* Process the shaper orders in reverse from high to low. */
	/* [These shapers were inspired by a Graphics Gem idea */
	/* (Gems IV, VI.3, "Fast Alternatives to Perlin's Bias and */
	/*  Gain Functions, pp 401). */
	/*  They have the nice properties that they are smooth, and */
	/*  are monotonic. The control parameter has been */
	/*  altered to have a range from -oo to +oo rather than 0.0 to 1.0 */
	/*  so that the search space is less non-linear. */
	for (ord = luord-1; ord >= 0; ord--) {
		int nsec;			/* Number of sections */
		double sec;			/* Section */

		g = -v[ord];		/* Inverse parameter */

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

	return vv;
}

/* Transfer function with offset and scale */
double icxSTransFunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
) {
	max -= min;

	vv = (vv - min)/max;
	vv = icxTransFunc(v, luord, vv);
	vv = (vv * max) + min;
	return vv;
}

/* Inverse Transfer function with offset and scale */
double icxInvSTransFunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
) {
	max -= min;

	vv = (vv - min)/max;
	vv = icxInvTransFunc(v, luord, vv);
	vv = (vv * max) + min;
	return vv;
}

/* Transfer function with partial derivative */
/* with respect to the parameters. */
double icxdpTransFunc(
double *v,			/* Pointer to first parameter */
double *dv,			/* Return derivative wrt each parameter */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
) {
	double g;
	int i, ord;

	/* Process all the shaper orders from high to low. */
	for (ord = 0; ord < luord; ord++) {
		double dsv;		/* del for del in g */
		double ddv;		/* del for del in vv */
		int nsec;		/* Number of sections */
		double sec;		/* Section */

		g = v[ord];			/* Parameter */

		nsec = ord + 1;		/* Increase sections for each order */

		vv *= (double)nsec;

		sec = floor(vv);
		if (((int)sec) & 1) {
			g = -g;				/* Alternate action in each section */
		}
		vv -= sec;
		if (g >= 0.0) {
			double tt = g - g * vv + 1.0;
			dsv = (vv * vv - vv)/(tt * tt);
			ddv = (g + 1.0)/(tt * tt);
			vv = vv/tt;
		} else {
			double tt = 1.0 - g * vv;
			dsv = (vv * vv - vv)/(tt * tt);
			ddv = (1.0 - g)/(tt * tt);
			vv = (vv - g * vv)/tt;
		}

		vv += sec;
		vv /= (double)nsec;
		dsv /= (double)nsec;
		if (((int)sec) & 1)
			dsv = -dsv;

		dv[ord] = dsv;
		for (i = ord - 1; i >= 0; i--)
			dv[i] *= ddv;
	}

	return vv;
}

/* Transfer function with offset and scale, and */
/* partial derivative with respect to the parameters. */
double icxdpSTransFunc(
double *v,			/* Pointer to first parameter */
double *dv,			/* Return derivative wrt each parameter */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
) {
	int i;
	max -= min;

	vv = (vv - min)/max;
	vv = icxdpTransFunc(v, dv, luord, vv);
	vv = (vv * max) + min;
	for (i = 0; i < luord; i++)
		dv[i] *= max;
	return vv;
}


/* Transfer function with partial derivative */
/* with respect to the input value. */
double icxdiTransFunc(
double *v,			/* Pointer to first parameter */
double *pdin,		/* Return derivative wrt source value */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
) {
	double g, din;
	int ord;

#ifdef NEVER
	if (vv < 0.0 || vv > 1.0) {
		if (vv < 0.0)
			vv = 0.0;
	 	else
			vv = 1.0;

		*pdin = 0.0;
		return vv;
	}
#endif
	din = 1.0;

	/* Process all the shaper orders from high to low. */
	for (ord = 0; ord < luord; ord++) {
		double ddv;		/* del for del in vv */
		int nsec;		/* Number of sections */
		double sec;		/* Section */

		g = v[ord];			/* Parameter */

		nsec = ord + 1;		/* Increase sections for each order */

		vv *= (double)nsec;

		sec = floor(vv);
		if (((int)sec) & 1) {
			g = -g;				/* Alternate action in each section */
		}
		vv -= sec;
		if (g >= 0.0) {
			double tt = g - g * vv + 1.0;
			ddv = (g + 1.0)/(tt * tt);
			vv = vv/tt;
		} else {
			double tt = 1.0 - g * vv;
			ddv = (1.0 - g)/(tt * tt);
			vv = (vv - g * vv)/tt;
		}

		vv += sec;
		vv /= (double)nsec;
		din *= ddv;
	}

	*pdin = din;
	return vv;
}

/* Transfer function with offset and scale, and */
/* partial derivative with respect to the input value. */
double icxdiSTransFunc(
double *v,			/* Pointer to first parameter */
double *pdv,		/* Return derivative wrt source value */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
) {
	max -= min;

	vv = (vv - min)/max;
	vv = icxdiTransFunc(v, pdv, luord, vv);
	vv = (vv * max) + min;
	return vv;
}


/* Transfer function with partial derivative */
/* with respect to the parameters and the input value. */
double icxdpdiTransFunc(
double *v,			/* Pointer to first parameter */
double *dv,			/* Return derivative wrt each parameter */
double *pdin,		/* Return derivative wrt source value */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
) {
	double g, din;
	int i, ord;

#ifdef NEVER
	if (vv < 0.0 || vv > 1.0) {
		if (vv < 0.0)
			vv = 0.0;
	 	else
			vv = 1.0;

		for (ord = 0; ord < luord; ord++)
			dv[ord] = 0.0;
		*pdin = 0.0;
		return vv;
	}
#endif
	din = 1.0;

	/* Process all the shaper orders from high to low. */
	for (ord = 0; ord < luord; ord++) {
		double dsv;		/* del for del in g */
		double ddv;		/* del for del in vv */
		int nsec;		/* Number of sections */
		double sec;		/* Section */

		g = v[ord];			/* Parameter */

		nsec = ord + 1;		/* Increase sections for each order */

		vv *= (double)nsec;

		sec = floor(vv);
		if (((int)sec) & 1) {
			g = -g;				/* Alternate action in each section */
		}
		vv -= sec;
		if (g >= 0.0) {
			double tt = g - g * vv + 1.0;
			dsv = (vv * vv - vv)/(tt * tt);
			ddv = (g + 1.0)/(tt * tt);
			vv = vv/tt;
		} else {
			double tt = 1.0 - g * vv;
			dsv = (vv * vv - vv)/(tt * tt);
			ddv = (1.0 - g)/(tt * tt);
			vv = (vv - g * vv)/tt;
		}

		vv += sec;
		vv /= (double)nsec;
		dsv /= (double)nsec;
		if (((int)sec) & 1)
			dsv = -dsv;

		dv[ord] = dsv;
		for (i = ord - 1; i >= 0; i--)
			dv[i] *= ddv;
		din *= ddv;
	}

	*pdin = din;
	return vv;
}

/* Transfer function with offset and scale, and */
/* partial derivative with respect to the */
/* parameters and the input value. */
double icxdpdiSTransFunc(
double *v,			/* Pointer to first parameter */
double *dv,			/* Return derivative wrt each parameter */
double *pdin,		/* Return derivative wrt source value */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
) {
	int i;
	max -= min;

	vv = (vv - min)/max;
	vv = icxdpdiTransFunc(v, dv, pdin, luord, vv);
	vv = (vv * max) + min;
	for (i = 0; i < luord; i++)
		dv[i] *= max;
	return vv;
}

/* ------------------------------------------------------ */
/* Multi-plane interpolation, used for device modelling. */
/* Including partial derivative for input and parameters. */
/* A simple flat plane is used for each output. */

/* Multi-plane interpolation - uses base + di slope values.  */
/* Parameters are assumed to be fdi groups of di + 1 parameters. */
void icxPlaneInterp(
double *v,			/* Pointer to first parameter [fdi * (di + 1)] */
int    fdi,			/* Number of output channels */
int    di,			/* Number of input channels */
double *out,		/* Resulting fdi values */
double *in			/* Input di values */
) {
	int e, f;

	for (f = 0; f < fdi; f++) {
		for (out[f] = 0.0, e = 0; e < di; e++, v++) {
			out[f] += in[e] * *v;
		}
		out[f] += *v;
	}
}


/* Multii-plane interpolation with partial derivative */
/* with respect to the input and parameters. */
void icxdpdiPlaneInterp(
double *v,			/* Pointer to first parameter value [fdi * (di + 1)] */
double *dv,			/* Return [1 + di] deriv. wrt each parameter v */
double *din,		/* Return [fdi * di] deriv. wrt each input value */
int    fdi,			/* Number of output channels */
int    di,			/* Number of input channels */
double *out,		/* Resulting fdi values */
double *in			/* Input di values */
) {
	int e, ee, f, g;
	int dip2 = (di + 1);	/* Output dim increment through parameters */

	/* Compute the output values */
	for (f = 0; f < fdi; f++) {
		for (out[f] = 0.0, e = 0; e < di; e++)
			out[f] += in[e] * v[f * dip2 + e];
		out[f] += v[f * dip2 + e];
	}

	/* Since interpolation is verys simple, derivative are also simple */

	/* Copy del for parameter to return array */
	for (e = 0; e < di; e++)
		dv[e] = in[e];
	dv[e] = 1.0;

	/* Compute del of out[] from in[] */
	for (f = 0; f < fdi; f++) {
		for (e = 0; e < di; e++) {
			din[f * di + e] = v[f * dip2 + e];
		}
	}
}

/* ------------------------------------------------------ */
/* Matrix cube interpolation, used for device modelling. */
/* Including partial derivative for input and parameters. */

/* Matrix cube interpolation - interpolate between 2^di output corner values. */
/* Parameters are assumed to be fdi groups of 2^di parameters. */
void icxCubeInterp(
double *v,			/* Pointer to first parameter */
int    fdi,			/* Number of output channels */
int    di,			/* Number of input channels */
double *out,		/* Resulting fdi values */
double *in			/* Input di values */
) {
	int e, f, g;
	double gw[1 << MXDI];		/* weight for each matrix grid cube corner */

	/* Compute corner weights needed for interpolation */
	gw[0] = 1.0;
	for (e = 0, g = 1; e < di; e++, g *= 2) {
		int i; 
		for (i = 0; i < g; i++) {
			gw[g+i] = gw[i] * in[e];
			gw[i] *= (1.0 - in[e]);
		}
	}

	/* Now compute the output values */
	for (f = 0; f < fdi; f++) {
		out[f] = 0.0;						/* For each output value */
		for (e = 0; e < (1 << di); e++) {	/* For all corners of cube */
			out[f] += gw[e] * *v;
			v++;
		}
	}
}


/* Matrix cube interpolation. with partial derivative */
/* with respect to the input and parameters. */
void icxdpdiCubeInterp(
double *v,			/* Pointer to first parameter value [fdi * 2^di] */
double *dv,			/* Return [2^di] deriv. wrt each parameter v */
double *din,		/* Return [fdi * di] deriv. wrt each input value */
int    fdi,			/* Number of output channels */
int    di,			/* Number of input channels */
double *out,		/* Resulting fdi values */
double *in			/* Input di values */
) {
	int e, ee, f, g;
	int dip2 = (1 << di);
	double gw[1 << MXDI];			/* weight for each matrix grid cube corner */

	/* Compute corner weights needed for interpolation */
	gw[0] = 1.0;
	for (e = 0, g = 1; e < di; e++, g *= 2) {
		int i; 
		for (i = 0; i < g; i++) {
			gw[g+i] = gw[i] * in[e];
			gw[i] *= (1.0 - in[e]);
		}
	}

	/* Now compute the output values */
	for (f = 0; f < fdi; f++) {
		out[f] = 0.0;						/* For each output value */
		for (ee = 0; ee < dip2; ee++) {	/* For all corners of cube */
			out[f] += gw[ee] * v[f * dip2 + ee];
		}
	}

	/* Copy del for parameter to return array */
	for (ee = 0; ee < dip2; ee++) {	/* For all other corners of cube */
		dv[ee] = gw[ee];					/* del from parameter */
	}

	/* Compute del from in[] value we want */
	for (e = 0; e < di; e++) {				/* For input we want del wrt */

		for (f = 0; f < fdi; f++)
			din[f * di + e] = 0.0;			/* Zero del ready of accumulation */

		for (ee = 0; ee < dip2; ee++) {		/* For all corners of cube weights, */
			int e2;							/* accumulate del from in[] we want. */
			double vv = 1.0;

			/* Compute in[] weighted cube corners for all except del of in[] we want */
			for (e2 = 0; e2 < di; e2++) {			/* const from non del inputs */
				if (e2 == e)
					continue;
				if (ee & (1 << e2))
					vv *= in[e2];
				else
					vv *= (1.0 - in[e2]);
			}

			/* Accumulate contribution of in[] we want for corner to out[] we want */
			if (ee & (1 << e)) {
				for (f = 0; f < fdi; f++)
					din[f * di + e] += v[f * dip2 + ee] * vv;
			} else {
				for (f = 0; f < fdi; f++)
					din[f * di + e] -= v[f * dip2 + ee] * vv;
			}
		}
	}
}

/* ------------------------------------------------------ */
/* Matrix cube simplex interpolation, used for device modelling. */

/* Matrix cube simplex interpolation - interpolate between 2^di output corner values. */
/* Parameters are assumed to be fdi groups of 2^di parameters. */
void icxCubeSxInterp(
double *v,			/* Pointer to first parameter */
int    fdi,			/* Number of output channels */
int    di,			/* Number of input channels */
double *out,		/* Resulting fdi values */
double *in			/* Input di values */
) {
	int    si[MAX_CHAN];		/* in[] Sort index, [0] = smalest */

//{
//	double tout[MXDO];
//
//	icxCubeInterp(v, fdi, di, tout, in);
//printf("\n~1 Cube interp result = %f\n",tout[0]);
//}

//printf("~1 icxCubeSxInterp: %f %f %f\n", in[0], in[1], in[2]);
	/* Do insertion sort on coordinates, smallest to largest. */
	{
		int ff, vf;
		unsigned int e;
		double v;
		for (e = 0; e < di; e++)
			si[e] = e;						/* Initial unsorted indexes */

		for (e = 1; e < di; e++) {
			ff = e;
			v = in[si[ff]];
			vf = ff;
			while (ff > 0 && in[si[ff-1]] > v) {
				si[ff] = si[ff-1];
				ff--;
			}
			si[ff] = vf;
		}
	}
//printf("~1 sort order %d %d %d\n", si[0], si[1], si[2]);
//printf("         from %f %f %f\n", in[si[0]], in[si[1]], in[si[2]]);

	/* Now compute the weightings, simplex vertices and output values */
	{
		unsigned int e, f;
		double w;		/* Current vertex weight */

		w = 1.0 - in[si[di-1]];		/* Vertex at base of cell */
		for (f = 0; f < fdi; f++) {
			out[f] = w * v[f * (1 << di)];
//printf("~1 out[%d] = %f = %f * %f\n",f,out[f],w,v[f * (1 << di)]);
		}

		for (e = di-1; e > 0; e--) {	/* Middle verticies */
			w = in[si[e]] - in[si[e-1]];
			v += (1 << si[e]);				/* Move to top of cell in next largest dimension */
			for (f = 0; f < fdi; f++) {
				out[f] += w * v[f * (1 << di)];
//printf("~1 out[%d] = %f += %f * %f\n",f,out[f],w,v[f * (1 << di)]);
			}
		}

		w = in[si[0]];
		v += (1 << si[0]);		/* Far corner from base of cell */
		for (f = 0; f < fdi; f++) {
			out[f] += w * v[f * (1 << di)];
//printf("~1 out[%d] = %f += %f * %f\n",f,out[f],w,v[f * (1 << di)]);
		}
	}
}

/* ------------------------------------------------------ */
/* Matrix multiplication, used for device modelling. */
/* Including partial derivative for input and parameters. */


/* 3x3 matrix in 1D array multiplication */
void icxMulBy3x3Parm(
	double out[3],			/* Return input multiplied by matrix */
	double mat[9],			/* Matrix organised in [slow][fast] order */
	double in[3]			/* Input values */
) {
	double *v = mat, ov[3];
	int e, f;

	/* Compute the output values */
	for (f = 0; f < 3; f++) {
		ov[f] = 0.0;						/* For each output value */
		for (e = 0; e < 3; e++) {
			ov[f] += *v++ * in[e];
		}
	}
	out[0] = ov[0];
	out[1] = ov[1];
	out[2] = ov[2];
}


/* 3x3 matrix in 1D array multiplication, with partial derivatives */
/* with respect to just the input. */
void icxdpdiiMulBy3x3Parm(
	double out[3],			/* Return input multiplied by matrix */
	double din[3][3],		/* Return deriv for each [output] with respect to [input] */
	double mat[9],			/* Matrix organised in [slow][fast] order */
	double in[3]			/* Input values */
) {
	double *v, ov[3];
	int e, f;

	/* Compute the output values */
	v = mat;
	for (f = 0; f < 3; f++) {
		ov[f] = 0.0;						/* For each output value */
		for (e = 0; e < 3; e++) {
			ov[f] += *v++ * in[e];
		}
	}

	/* Compute deriv. with respect to the input values */
	/* This is pretty simple for a matrix ... */
	v = mat;
	for (f = 0; f < 3; f++)
		for (e = 0; e < 3; e++)
			din[f][e] = *v++;

	out[0] = ov[0];
	out[1] = ov[1];
	out[2] = ov[2];
}

/* 3x3 matrix in 1D array multiplication, with partial derivatives */
/* with respect to the input and parameters. */
void icxdpdiMulBy3x3Parm(
	double out[3],			/* Return input multiplied by matrix */
	double dv[3][9],		/* Return deriv for each [output] with respect to [param] */
	double din[3][3],		/* Return deriv for each [output] with respect to [input] */
	double mat[9],			/* Matrix organised in [slow][fast] order */
	double in[3]			/* Input values */
) {
	double *v, ov[3];
	int e, f;

	/* Compute the output values */
	v = mat;
	for (f = 0; f < 3; f++) {
		ov[f] = 0.0;						/* For each output value */
		for (e = 0; e < 3; e++) {
			ov[f] += *v++ * in[e];
		}
	}

	/* Compute deriv. with respect to the matrix parameter % 3 */
	/* This is pretty simple for a matrix ... */
	for (f = 0; f < 3; f++) {
		for (e = 0; e < 9; e++) {
			if (e/3 ==  f)
				dv[f][e] = in[e % 3];
			else
				dv[f][e] = 0.0;
		}
	}
	
	/* Compute deriv. with respect to the input values */
	/* This is pretty simple for a matrix ... */
	v = mat;
	for (f = 0; f < 3; f++)
		for (e = 0; e < 3; e++)
			din[f][e] = *v++;

	out[0] = ov[0];
	out[1] = ov[1];
	out[2] = ov[2];
}

/* - - - - - - - - - - */
#undef stricmp





















