
/* 
 * International Color Consortium color transform expanded support
 * Known colorants support.
 *
 * Author:  Graeme W. Gill
 * Date:    24/2/2002
 * Version: 1.00
 *
 * Copyright 2002 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/* TTBD:
		Would like to have some short string way of defining whether
		the ink combination is additive or subtractive, instead
		of just guessing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "icc.h"
#include "xcolorants.h"
#include "sort.h"

/* Colorant table for N color device characterisation. */
/* This is ordered to match the ICC colorspace convention. */
/* NOTE:- need to keep these in same order as ink mask */
/* enumeration (lsb to msb), or strange things result! */
static struct {
	inkmask m;		/* Mask value */
	char *c;		/* 1/2 Character name */
	char *s;		/* Everyday name */
	char *ps;		/* Postscript colorant name */
	double aXYZ[3];	/* Rough XYZ values (0..1) for that additive colorant */
	double sXYZ[3];	/* Rough XYZ values (0..1) for that subtractive colorant */
} icx_ink_table[] = {
	{ ICX_CYAN,           ICX_C_CYAN,           ICX_S_CYAN,           ICX_PS_CYAN,
      { 0.12, 0.18, 0.48 },	
      { 0.12, 0.18, 0.48 } },	
	{ ICX_MAGENTA,        ICX_C_MAGENTA,        ICX_S_MAGENTA,        ICX_PS_MAGENTA,
	  { 0.38, 0.19, 0.20 },	
	  { 0.38, 0.19, 0.20 } },	
	{ ICX_YELLOW,         ICX_C_YELLOW,         ICX_S_YELLOW,         ICX_PS_YELLOW,
	  { 0.76, 0.81, 0.11 },	
	  { 0.76, 0.81, 0.11 } },	
	{ ICX_BLACK,          ICX_C_BLACK,          ICX_S_BLACK,          ICX_PS_BLACK,
	  { 0.01, 0.01, 0.01 },	
	  { 0.04, 0.04, 0.04 } },	
	{ ICX_ORANGE,         ICX_C_ORANGE,         ICX_S_ORANGE,         ICX_PS_ORANGE,
	  { 0.59, 0.41, 0.03 },	
	  { 0.59, 0.41, 0.05 } },	
	{ ICX_RED,            ICX_C_RED,            ICX_S_RED,            ICX_PS_RED,
	  { 0.412414, 0.212642, 0.019325 },
	  { 0.40, 0.21, 0.05 } },	
	{ ICX_GREEN,          ICX_C_GREEN,          ICX_S_GREEN,          ICX_PS_GREEN,
	  { 0.357618, 0.715136, 0.119207 },
	  { 0.11, 0.27, 0.21 } },	
	{ ICX_BLUE,           ICX_C_BLUE,           ICX_S_BLUE,           ICX_PS_BLUE,
	  { 0.180511, 0.072193, 0.950770 },
	  { 0.11, 0.27, 0.47 } },	
	{ ICX_WHITE,          ICX_C_WHITE,          ICX_S_WHITE,          ICX_PS_WHITE,
	  { 0.950543, 1.0,  1.089303 },		/* D65 ? */
	  { 0.9642,   1.00, 0.8249 } },		/* D50 */
	{ ICX_LIGHT_CYAN,     ICX_C_LIGHT_CYAN,     ICX_S_LIGHT_CYAN,     ICX_PS_LIGHT_CYAN,
	  { 0.76, 0.89, 1.08 },	
	  { 0.76, 0.89, 1.08 } },	
	{ ICX_LIGHT_MAGENTA,  ICX_C_LIGHT_MAGENTA,  ICX_S_LIGHT_MAGENTA,  ICX_PS_LIGHT_MAGENTA,
	  { 0.83, 0.74, 1.02 },	
	  { 0.83, 0.74, 1.02 } },	
	{ ICX_LIGHT_YELLOW,   ICX_C_LIGHT_YELLOW,   ICX_S_LIGHT_YELLOW,   ICX_PS_LIGHT_YELLOW,
	  { 0.88, 0.97, 0.72 },	
	  { 0.88, 0.97, 0.72 } },	
	{ ICX_LIGHT_BLACK,    ICX_C_LIGHT_BLACK,    ICX_S_LIGHT_BLACK,    ICX_PS_LIGHT_BLACK,
	  { 0.56, 0.60, 0.65 },	
	  { 0.56, 0.60, 0.65 } },	
	{ ICX_MEDIUM_CYAN,    ICX_C_MEDIUM_CYAN,    ICX_S_MEDIUM_CYAN,    ICX_PS_MEDIUM_CYAN,
	  { 0.61, 0.81, 1.07 },	
	  { 0.61, 0.81, 1.07 } },	
	{ ICX_MEDIUM_MAGENTA, ICX_C_MEDIUM_MAGENTA, ICX_S_MEDIUM_MAGENTA, ICX_PS_MEDIUM_MAGENTA,
	  { 0.74, 0.53, 0.97 },	
	  { 0.74, 0.53, 0.97 } },	
	{ ICX_MEDIUM_YELLOW,  ICX_C_MEDIUM_YELLOW,  ICX_S_MEDIUM_YELLOW,  ICX_PS_MEDIUM_YELLOW,
	  { 0.82, 0.93, 0.40 },	
	  { 0.82, 0.93, 0.40 } },	
	{ ICX_MEDIUM_BLACK,   ICX_C_MEDIUM_BLACK,   ICX_S_MEDIUM_BLACK,   ICX_PS_MEDIUM_BLACK,
	  { 0.27, 0.29, 0.31 },	
	  { 0.27, 0.29, 0.31 } },	
	{ ICX_LIGHT_LIGHT_BLACK,   ICX_C_LIGHT_LIGHT_BLACK,   ICX_S_LIGHT_LIGHT_BLACK,   ICX_PS_LIGHT_LIGHT_BLACK,
	  { 0.76, 0.72, 0.65 },			/* Very rough - should substiture real numbers */
	  { 0.76, 0.72, 0.65 } },	
	{ 0, "", "", "", { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 } }
};


/* Colorant table for N color device characterisation */
static struct {
	inkmask m;					/* Mask combination */
	inkmask rm;					/* Light ink reduced mask combination */
	icColorSpaceSignature psig;	/* Appropriate primary ICC signature */
	icColorSpaceSignature ssig;	/* Appropriate secondary ICC signature */
	char *desc;					/* Description */
} icx_colcomb_table[] = {
	{ ICX_K,		  ICX_K,           icSigGrayData,   icSigGrayData,
	                                                    "Print grey" },
	{ ICX_W, 		  ICX_W,           icSigGrayData,   icSigGrayData,
	                                                    "Video grey" },
	{ ICX_IRGB,		  ICX_IRGB,        icSigRgbData,    icSigRgbData,
	                                                    "Print RGB" },
	{ ICX_RGB, 		  ICX_RGB,         icSigRgbData,    icSigRgbData,
	                                                    "Video RGB" },
	{ ICX_CMYK,       ICX_CMYK,        icSigCmykData,   icSigCmykData,
	                                                    "CMYK" },
	{ ICX_CMY, 		  ICX_CMY,         icSigCmyData,    icSigCmyData,
	                                                    "CMY" },
	{ ICX_CMYKcm,     ICX_CMYK,        icSig6colorData, icSigMch6Data,
	                                                    "CMYK + Light CM" },
	{ ICX_CMYKcmk,    ICX_CMYK,        icSig7colorData, icSig7colorData,
	                                                    "CMYK + Light CMK" },
	{ ICX_CMYKRB,     ICX_W,           icSig6colorData, icSigMch6Data,
	                                                    "CMYK + Red + Blue" },
	{ ICX_CMYKOG,     ICX_W,           icSig6colorData, icSigMch6Data,
	                                                    "CMYK + Orange + Green" },
	{ ICX_CMYKcmk1k,  ICX_CMYK,        icSig8colorData, icSigMch8Data,
	                                                    "CMYK + Light CMK + Light Light K" },
	{ ICX_CMYKOGcm,   ICX_CMYKOG,      icSig8colorData, icSigMch8Data,
	                                                    "CMYK + Orange + Green + Light CM" },
	{ ICX_CMYKcm2c2m, ICX_CMYK,        icSig8colorData, icSigMch8Data,
	                                                    "CMYK + Light CM + Medium CM" },
	{ 0, 0, 0, 0, "" }
};


/* Given an ink combination mask, return the number of recognised inks in it */
int icx_noofinks(inkmask mask) {
	int i, count = 0;

	for (i = 0; icx_ink_table[i].m != 0; i++) {
		if (mask & icx_ink_table[i].m) {
			count++;
		}
	}
	return count;
}

/* Given an ink combination mask, return the 1-2 character based string */
/* If winv is nz, include ICX_INVERTED indicator if set */
/* Return NULL on error. free() after use */
char *icx_inkmask2char(inkmask mask, int winv) {
	int i;
	char *rv;

	if ((rv = malloc(2 * ICX_MXINKS + 1)) == NULL)
		return NULL;

	*rv = '\000';
	
	if (winv && (mask & ICX_INVERTED))
		strcat(rv, "i");

	for (i = 0; icx_ink_table[i].m != 0; i++) {
		if (mask & icx_ink_table[i].m) {
			strcat(rv, icx_ink_table[i].c);
		}
	}

	return rv;
}

/* Given the 1-2 character string, return the ink combination mask */
/* Note that ICX_ADDITIVE will be guessed */
/* Return 0 if unrecognised character in string */
inkmask icx_char2inkmask(char *chstring)  {
	inkmask mask = 0;
	int k, i;
	char *cp;

	cp = chstring;

	for (k = 0; ; k++) {

		if (*cp == '\000') {
			break;
		}

		/* "Inverted" prefix ? */
		if (k == 0 && *cp == 'i') {
			mask |= ICX_INVERTED;
			cp++;
			continue;
		}
		/* Colorant ? */
		for (i = 0; icx_ink_table[i].m != 0; i++) {

			if (strncmp(cp, icx_ink_table[i].c, strlen(icx_ink_table[i].c)) == 0) {
				mask |= icx_ink_table[i].m;
				cp += strlen(icx_ink_table[i].c);
				break;
			}
		}
		if (icx_ink_table[i].m == 0) {		/* oops - unrecognised character */
			return 0;
		}
	}

	/* Check it out againts known combinations to guess additive */
	for (i = 0; icx_colcomb_table[i].m != 0; i++) {
		if ((icx_colcomb_table[i].m & ~ICX_ADDITIVE) == mask) {
			mask = icx_colcomb_table[i].m;
			break;
		}
	}

	return mask;
}

/* Given an ink combination mask that may contain light inks, */
/* return the corresponding ink mask without light inks. */
/* Return 0 if ink combination not recognised. */
inkmask icx_ink2primary_ink(inkmask mask) {
	int i;
	for (i = 0; icx_colcomb_table[i].m != 0; i++) {
	 	if (mask == icx_colcomb_table[i].m) {
			 return icx_colcomb_table[i].rm;
		}
	}
	return 0;
}


/* Given an ink combination mask and a single ink mask, */
/* return the index number for that ink. */
/* Return -1 if mask1 not in mask */
int icx_ink2index(inkmask mask, inkmask mask1) {
	int i, count = 0;

	if ((mask1 & mask) == 0)
		return -1;

	for (i = 0; icx_ink_table[i].m != 0; i++) {
		if (mask1 == icx_ink_table[i].m) {
			return count;
		}
		if (mask & icx_ink_table[i].m) {
			count++;
		}
	}

	return -1;
}

/* Given an ink combination mask and a index number, */
/* return the single ink mask. */
/* Return 0 if there are no inks at that index */
inkmask icx_index2ink(inkmask mask, int ixno) {
	int i, count = 0;

	for (i = 0; icx_ink_table[i].m != 0; i++) {
		if (mask & icx_ink_table[i].m) {
			if (ixno == count)
				return icx_ink_table[i].m;
			count++;
		}
	}
	return 0;
}


/* Given a single ink mask, */
/* return its string representation */
char *icx_ink2string(inkmask mask) {
	int i;

	for (i = 0; icx_ink_table[i].m != 0; i++) {
		if (mask == icx_ink_table[i].m)
			return icx_ink_table[i].s;
	}
	return NULL;
}

/* Given a single ink mask, */
/* return its 1-2 character representation */
char *icx_ink2char(inkmask mask) {
	int i;

	for (i = 0; icx_ink_table[i].m != 0; i++) {
		if (mask == icx_ink_table[i].m)
			return icx_ink_table[i].c;
	}
	return NULL;
}

/* Given a single ink mask, */
/* return its Postscript string representation */
char *icx_ink2psstring(inkmask mask) {
	int i;

	for (i = 0; icx_ink_table[i].m != 0; i++) {
		if (mask == icx_ink_table[i].m)
			return icx_ink_table[i].ps;
	}
	return NULL;
}


/* Return an enumerated single colorant description */
/* Return 0 if no such enumeration, single colorant mask if there is */
inkmask icx_enum_colorant(
int no,			/* Enumeration mask index */
char **desc		/* Return enumeration description */
) {
	int i;

	for (i = 0; icx_ink_table[i].m != 0; i++) {
		if (i == no) {
			if (desc != NULL)
				*desc = icx_ink_table[i].s;
			return icx_ink_table[i].m;
		}
	}
	return 0;
}

/* Return an enumerated colorant combination */
/* Return 0 if no such enumeration, colorant combination mask if there is */
inkmask icx_enum_colorant_comb(
int no,			/* Enumeration mask index */
char **desc		/* Return enumeration description */
) {
	int i;

	for (i = 0; icx_colcomb_table[i].m != 0; i++) {
		if (i == no) {
			if (desc != NULL)
				*desc = icx_colcomb_table[i].desc;
			return icx_colcomb_table[i].m;
		}
	}
	return 0;
}


/* Given an colorant combination mask, */
/* check if it matches the given ICC colorspace signature. */ 
/* return NZ if it does. */
/* (We don't check colorant colors for multi-colorant devices though) */
int icx_colorant_comb_match_icc(
inkmask mask,				/* Colorant combination mask */
icColorSpaceSignature sig	/* ICC signature */
) {
	int i;
	for (i = 0; icx_colcomb_table[i].m != 0; i++) {
	 	if (mask == icx_colcomb_table[i].m) {
			if (sig == icx_colcomb_table[i].psig
			 || sig == icx_colcomb_table[i].ssig) {
				return 1;
			} else {
				return 0;
			}
		}
	}
	return 0;
}

/* Given an ICC colorspace signature, return the appropriate */
/* colorant combination mask. Return 0 if ambiguous signature. */
inkmask icx_icc_to_colorant_comb(icColorSpaceSignature sig, icProfileClassSignature deviceClass) {
	switch (sig) {
		case icSigGrayData:
			if (deviceClass == icSigOutputClass)
				return ICX_K;
			return ICX_W;

		case icSigCmyData:
			return ICX_CMY;

		case icSigRgbData:
			if (deviceClass == icSigOutputClass)
				return ICX_IRGB;
			return ICX_RGB;

		case icSigCmykData:
			return ICX_CMYK;

		default:
			break;
	}
	return 0;
}

/* Given an ICC colorspace signature, and a matching list */
/* of the D50 L*a*b* colors of the colorants, return the best matching */
/* colorant combination mask. Return 0 if not an applicable colorspace. */
/* (Note we're not dealing with colorant order here.) */
inkmask icx_icc_cv_to_colorant_comb(
icColorSpaceSignature sig,				/* Input ICC colorspace signature */ 
icProfileClassSignature deviceClass,	/* Device class */
double cvals[][3]						/* Input L*a*b* colorant values */
) {
	int i, j;
	int imask;
	int ninks, ncol;
	double slab[ICX_MXINKS][3];
	double alab[ICX_MXINKS][3];
	typedef struct { int x; double v; } mchstr;
	mchstr mch[MAX_CHAN][ICX_MXINKS];	/* match index */
	int used[ICX_MXINKS];
	int co[ICX_MXINKS];		/* Combination counter */
	double cmv;				/* Combination match value */
	int bco[ICX_MXINKS];	/* Best Combinat */
	double bcmv;			/* Best Match value */
	int order[MAX_CHAN];	/* Place holder, not currently used - return ink order */

	switch (sig) {
    	case icSigXYZData:
    	case icSigLabData:
    	case icSigLuvData:
    	case icSigYCbCrData:
    	case icSigYxyData:
    	case icSigHsvData:
    	case icSigHlsData:
		case icSigNamedData:
			return 0;

		case icSigGrayData:
			if (deviceClass == icSigOutputClass)
				return ICX_K;
			return ICX_W;

		case icSigCmyData:
			return ICX_CMY;

		case icSigRgbData:
			if (deviceClass == icSigOutputClass)
				return ICX_IRGB;
			return ICX_RGB;

		case icSigCmykData:
			return ICX_CMYK;

		default:
			break;

	}
#ifdef NEVER	/* Rely on device class (above) */
	if (sig == icSigGrayData) {
		/* This only works reliably if we got the Lab data from a non-ICC */
		/* conformant monochrome profile (one that doesn't force device 0 */
		/* input of the grayTRTC to black), or uses a LUT based monochrome */
		/* profile. */
		/* It also won't work if someone uses a monochrome profile for a */
		/* device with a non grey colorant. */
		/* Really ICC should have icSigWhiteData, icSigBlackData, icSig1colorData ? */
		if (cvals[0][0] > 50.0) {
			return ICX_W;
		} else {
			return ICX_K;
		} 
	}
#endif /* NEVER */

	/* Compute Lab values of stock inks, and count them */
	for (ninks = 0; ninks < ICX_MXINKS; ninks++) {
		if (icx_ink_table[ninks].m == 0)
			break;
		icmXYZ2Lab(&icmD50, slab[ninks], icx_ink_table[ninks].sXYZ);
		icmXYZ2Lab(&icmD50, alab[ninks], icx_ink_table[ninks].aXYZ);
	}

	ncol = icmCSSig2nchan(sig);	/* Number of colorants */

	/* Compute ideal matching of device colorants to stock inks */
	for (i = 0; i < ncol; i++) {
		for (j = 0; j < ninks; j++) {
			double tt;
			mch[i][j].x = j;
			mch[i][j].v = icmCIE94sq(cvals[i], slab[j]);
			tt = icmCIE94sq(cvals[i], alab[j]);
			if (tt < mch[i][j].v)
				mch[i][j].v = tt;
		}
		/* Sort the matches for this colorant */
#define HEAP_COMPARE(A,B) (A.v < B.v)
		HEAPSORT(mchstr, mch[i], ninks)
#undef HEAP_COMPARE

//printf("\n~1 Colorant %d has best matches\n",i);
//for (j = 0; j < ninks; j++)
//printf("~1 ix %d color = '%s' dE %f\n",j, icx_ink_table[mch[i][j].x].s, mch[i][j].v);
	} 

	/* Do exaustive combination search, using early */
	/* out to keep combination count down.*/
	
	/* Reset the combination counter */
	for (j = 0; j < ninks; j++)	/* Reset ink used flag */
		used[j] = 0;
	cmv = 0.0;
	for (i = ncol-1; i >= 0; i--) {
		for (j = 0; j < ninks; j++) {			/* Set to lowest usable number */
			if (used[mch[i][j].x] == 0) {		/* Can use this one */
				used[mch[i][j].x] = 1;			/* Now it's matched */
				cmv += mch[i][j].v;
				co[i] = j;
				break;							/* we assume ncol < ninks */
			}
		}
	} 

//printf("\n~1 Initial combination has cmv = %f\n",cmv);
//for (i = 0; i < ncol; i++)
//printf("~1 Chan %d color = '%s'\n",i, icx_ink_table[mch[i][co[i]].x].s);

	/* Set this as the best initial match */
	for (i = 0; i < ncol; i++)
		bco[i] = co[i];
	bcmv = cmv;
	
	/* Now go through all other combinations */
	for (;;) {

		/* Increment counter */
		for (i = 0;;) {

			/* Work up the digits, incrementing each one */
			for (; i < ncol; i++) {
				j = co[i];
				used[mch[i][j].x] = 0;
				cmv -= mch[i][j].v;
				while (++j < ninks && (used[mch[i][j].x] != 0 || (cmv + mch[i][j].v) >= bcmv)) {
				}; 
				if (j < ninks) {	/* No carry */
					used[mch[i][j].x] = 1;
					cmv += mch[i][j].v;
					co[i] = j;
					break;
				}
				/* Carry to next colorant */
			}
			if (i >= ncol)
				break;				/* Run out of digits to increment */

			/* Now work down again, resetting digit */
			for (--i; i >= 0; i--) {
				for (j = 0; j < ninks; j++) {		/* Set to lowest usable number */
					if (used[mch[i][j].x] == 0 && (cmv + mch[i][j].v) < bcmv) {
						/* Can use this one */
						used[mch[i][j].x] = 1;		/* Now it's matched */
						cmv += mch[i][j].v;
						co[i] = j;
						break;
					}
				}
				if (j >= ninks) {		/* No combination is feasible */
					i++;				/* Go back to incrementing next highest digit */
					break;
				}
			}
			if (i < 0)					/* We reset all the lower digits */
				break;					/* We're at a good combination, so done. */
		}
		if (i >= ncol)
			break;				/* We're done all combinations */

//printf("\n~1 New combination has cmv = %f\n",cmv);
//for (i = 0; i < ncol; i++)
//printf("~1 Chan %d color = '%s'\n",i, icx_ink_table[mch[i][co[i]].x].s);

		/* See if this is better (it always should be !) */
		if (cmv < bcmv) {
			for (i = 0; i < ncol; i++)
				bco[i] = co[i];
			bcmv = cmv;
		}
	}

	/* Compile the result */
	for (imask = 0, i = 0; i < ncol; i++) {
		imask |= icx_ink_table[mch[i][bco[i]].x].m;
		order[i] = bco[i];		/* icx ink_table index corresponding to channel */
	}

	/* Slight hack to recognise some additive combinations */
	if (imask == ICX_WHITE)
		imask = ICX_W;
	else if (imask == (ICX_RED | ICX_GREEN | ICX_BLUE))
		imask = ICX_RGB;

	return imask;
}

/* Given a colorant combination mask */
/* return the primary matching ICC colorspace signature. */ 
/* return 0 if there is no match */
icColorSpaceSignature icx_colorant_comb_to_icc(
inkmask mask			/* Colorant combination mask */
) {
	int i;

	for (i = 0; icx_colcomb_table[i].m != 0; i++) {
	 	if (mask == icx_colcomb_table[i].m)
			return icx_colcomb_table[i].psig;
	}
	return 0;
}



/* - - - - - - - - - - - - - - - - - */
/* Approximate device colorant model */

/* Given device values, return an estimate of the XYZ value for that color. */
static void icxColorantLu_to_XYZ(
icxColorantLu *s,
double XYZ[3],			/* Output */
double d[ICX_MXINKS]	/* Input */
) {
	int e, j;
 
	if (s->mask & ICX_ADDITIVE ) {
		/* We assume a simple additive model with gamma */

		XYZ[0] = XYZ[1] = XYZ[2] = 0.0;

		for (e = 0; e < s->di; e++) {
			double v = d[e];
				
			if (v < 0.0)
				v = 0.0;
			else if (v > 1.0)
				v = 1.0;
			if (v <= 0.03928)
				v /= 12.92;
			else
				v = pow((0.055 + v)/1.055, 2.4);		/* Gamma */

			for (j = 0; j < 3; j++)
				XYZ[j] += v * icx_ink_table[s->iix[e]].aXYZ[j];
		}

		/* Normalise Y to 1.0, & add black glare */
		for (j = 0; j < 3; j++) {
			XYZ[j] *= s->Ynorm;
			XYZ[j] = XYZ[j] * (1.0 - icx_ink_table[s->bkix].aXYZ[j]) + icx_ink_table[s->bkix].aXYZ[j];
		}

	} else {
		/* We assume a simple screened subtractive filter model, with dot gain */

		/* start with white */
		XYZ[0] = icx_ink_table[s->whix].sXYZ[0];
		XYZ[1] = icx_ink_table[s->whix].sXYZ[1];
		XYZ[2] = icx_ink_table[s->whix].sXYZ[2];

		/* And filter it out for each component */
		for (e = 0; e < s->di; e++) {
			double v = d[e];
				
			if (v < 0.0)
				v = 0.0;
			else if (v > 1.0)
				v = 1.0;
			v = 1.0 - pow(1.0 - v, 2.2); /* Compute dot gain */

			for (j = 0; j < 3; j++) {
				double fv;

				/* Normalise filtering effect of this colorant */
				fv = icx_ink_table[s->iix[e]].aXYZ[j]/icx_ink_table[s->whix].sXYZ[j];

				/* Compute screened filtering effect */
				fv = (1.0 - v) + v * fv;

				/* Apply filter to our current value */
				XYZ[j] *= fv;
			}
		}
	}
}

/* Given device values, return an estimate of the */
/* relative Lab value for that color. */
static void icxColorantLu_to_rLab(
icxColorantLu *s,
double Lab[3],			/* Output */
double d[ICX_MXINKS]	/* Input */
) {

	icxColorantLu_to_XYZ(s, Lab, d);	/* Compute XYZ */
	icmXYZ2Lab(&s->wp, Lab, Lab);		/* Convert from XYZ to Lab */
}

/* We're done with aproximate device model */
static void icxColorantLu_del(icxColorantLu *s) {

	if (s != NULL) {
		free(s);
	}
}

/* Given an ink definition, return an aproximate */
/* device to CIE color converted object. */
icxColorantLu *new_icxColorantLu(inkmask mask) {
	int i, e;
	icxColorantLu *s;
 
	if ((s = (icxColorantLu *)malloc(sizeof(icxColorantLu))) == NULL) {
		fprintf(stderr,"icxColorantLu: malloc failed allocating object\n");
		exit(-1);
	}
	
	/* Initialise methods */
	s->del      = icxColorantLu_del;
	s->dev_to_XYZ = icxColorantLu_to_XYZ;
	s->dev_to_rLab = icxColorantLu_to_rLab;

	/* Init */
	s->mask = mask;

	for (e = i = 0; icx_ink_table[i].m != 0; i++) {
		if (ICX_WHITE == icx_ink_table[i].m)
			s->whix = i;
		if (ICX_BLACK == icx_ink_table[i].m)
			s->bkix = i;
		if (mask & icx_ink_table[i].m)
			s->iix[e++] = i;
	}
	s->di = e;


	s->Ynorm = 0.0;
	if (mask & ICX_ADDITIVE ) {
		for (e = 0; e < s->di; e++)
			s->Ynorm += icx_ink_table[s->iix[e]].aXYZ[1];
		s->Ynorm = 1.0/s->Ynorm;
		icmAry2XYZ(s->wp, icx_ink_table[s->whix].aXYZ);
	} else {
		icmAry2XYZ(s->wp, icx_ink_table[s->whix].sXYZ);
	}

	return s;
}





















