#ifndef XCOLORANTS_H
#define XCOLORANTS_H
/* 
 * International Color Consortium color transform expanded support
 * Known colorant definitions.
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

/* Some standard defines for known generic ink colors */

/*
   Note that we don't handle the issue of an arbitrary ink order,
   and we are only partially coping with multi-ink ICC profiles
   mapping to xcolorants and back. This is supported by mapping
   inkmask to ICC ColorantTable names and back (if needed),
   or by guessing colorants to match profile (icx_icc_cv_to_colorant_comb()).

   default xcolorant order matches the concrete ICC order, but
   ICC icSigXXcolorData could be in any order, and xcolorants
   currently doesn't handle it.

   One way of encapsulating things would be to change inkmask to:

	struct {
		unsigned int attr;							// Attributes (ie. Additive)
		unsigned int mask[(ICX_MXINKS + 31)/32];	// Ink masks
		int inks[MAX_CHAN];							// icx_ink_table[] index
	} inkmask;

   Then add appropriate macros & functions to replace current bit mask logic.

   Would it be easier to make all Argyll internal handling conform
   to the standard xcolorants order, and translate in the ICC file I/O ?
   MPP and .ti? files are assumed/made to conform to xcolorants order.
   
   Another change would be to make xcolorants an object,
   with dynamic colorant and colorant combination values.
   These would be initialised to defaults, but could then
   be added to at run time.

   Handling the "colorant" of non-device type color channels
   is also a challenge (ie., Lab, Hsv etc.)

 */ 

/* Maximum number of simultanious inks allowed for */
#define ICX_MXINKS 31

/* Note that new inks need to be added to icx_ink_table[] in xcolorants.c ! */

/* The type of the inkmask (allow for expansion) */
typedef unsigned int inkmask;

/* The ink mask enumeration */
#define ICX_ADDITIVE          0x80000000	/* Special flag indicating addive colorants */
#define ICX_INVERTED          0x40000000	/* Special flag indicating actual device is */
											/* the inverse of the perported additive/subtractive */
#define ICX_CYAN              0x00000001
#define ICX_MAGENTA           0x00000002
#define ICX_YELLOW            0x00000004
#define ICX_BLACK             0x00000008
#define ICX_ORANGE            0x00000010
#define ICX_RED               0x00000020
#define ICX_GREEN             0x00000040
#define ICX_BLUE              0x00000080
#define ICX_WHITE             0x00000100
#define ICX_LIGHT_CYAN        0x00010000
#define ICX_LIGHT_MAGENTA     0x00020000
#define ICX_LIGHT_YELLOW      0x00040000
#define ICX_LIGHT_BLACK       0x00080000
#define ICX_MEDIUM_CYAN       0x00100000
#define ICX_MEDIUM_MAGENTA    0x00200000
#define ICX_MEDIUM_YELLOW     0x00400000
#define ICX_MEDIUM_BLACK      0x00800000
#define ICX_LIGHT_LIGHT_BLACK 0x01000000

/* Character representation */
#define ICX_C_CYAN              "C"
#define ICX_C_MAGENTA           "M"
#define ICX_C_YELLOW            "Y"
#define ICX_C_BLACK             "K"
#define ICX_C_ORANGE            "O"
#define ICX_C_RED               "R"
#define ICX_C_GREEN             "G"
#define ICX_C_BLUE              "B"
#define ICX_C_WHITE             "W"
#define ICX_C_LIGHT_CYAN        "c"
#define ICX_C_LIGHT_MAGENTA     "m"
#define ICX_C_LIGHT_YELLOW      "y"
#define ICX_C_LIGHT_BLACK       "k"
#define ICX_C_MEDIUM_CYAN       "2c"
#define ICX_C_MEDIUM_MAGENTA    "2m"
#define ICX_C_MEDIUM_YELLOW     "2y"
#define ICX_C_MEDIUM_BLACK      "2k"
#define ICX_C_LIGHT_LIGHT_BLACK "1k"

/* Everyday String representation (max 31 chars) */
#define ICX_S_CYAN              "Cyan"
#define ICX_S_MAGENTA           "Magenta"
#define ICX_S_YELLOW            "Yellow"
#define ICX_S_BLACK             "Black"
#define ICX_S_ORANGE            "Orange"
#define ICX_S_RED               "Red"
#define ICX_S_GREEN             "Green"
#define ICX_S_BLUE              "Blue"
#define ICX_S_WHITE             "White"
#define ICX_S_LIGHT_CYAN        "Light Cyan"
#define ICX_S_LIGHT_MAGENTA     "Light Magenta"
#define ICX_S_LIGHT_YELLOW      "Light Yellow"
#define ICX_S_LIGHT_BLACK       "Light Black"
#define ICX_S_MEDIUM_CYAN       "Medium Cyan"
#define ICX_S_MEDIUM_MAGENTA    "Medium Magenta"
#define ICX_S_MEDIUM_YELLOW     "Medium Yellow"
#define ICX_S_MEDIUM_BLACK      "Medium Black"
#define ICX_S_LIGHT_LIGHT_BLACK "Light Light Black"

/* Postscript string representation */
#define ICX_PS_CYAN              "Cyan"
#define ICX_PS_MAGENTA           "Magenta"
#define ICX_PS_YELLOW            "Yellow"
#define ICX_PS_BLACK             "Black"
#define ICX_PS_ORANGE            "Orange"
#define ICX_PS_RED               "Red"
#define ICX_PS_GREEN             "Green"
#define ICX_PS_BLUE              "Blue"
#define ICX_PS_WHITE             "White"
#define ICX_PS_LIGHT_CYAN        "LightCyan"
#define ICX_PS_LIGHT_MAGENTA     "LightMagenta"
#define ICX_PS_LIGHT_YELLOW      "LightYellow"
#define ICX_PS_LIGHT_BLACK       "LightBlack"
#define ICX_PS_MEDIUM_CYAN       "MediumCyan"
#define ICX_PS_MEDIUM_MAGENTA    "MediumMagenta"
#define ICX_PS_MEDIUM_YELLOW     "MediumYellow"
#define ICX_PS_MEDIUM_BLACK      "MediumBlack"
#define ICX_PS_LIGHT_LIGHT_BLACK "LightLightBlack"

/* Common colorant combinations */
#define ICX_W						/* Video style "grey" */ \
	(ICX_ADDITIVE | ICX_WHITE)

#define ICX_RGB						/* Classic video RGB */ \
	(ICX_ADDITIVE | ICX_RED | ICX_GREEN | ICX_BLUE)

#define ICX_K						/* Printer style "grey" */ \
	(ICX_BLACK)

#define ICX_CMY						/* Classic printing CMY */ \
	(ICX_CYAN | ICX_MAGENTA | ICX_YELLOW)

#define ICX_IRGB					/* Fake printer RGB (== Inverted CMY) */ \
	(ICX_ADDITIVE | ICX_INVERTED | ICX_RED | ICX_GREEN | ICX_BLUE)

#define ICX_CMYK					/* Classic printing CMYK */ \
	(ICX_CYAN | ICX_MAGENTA | ICX_YELLOW | ICX_BLACK)

#define ICX_CMYKcm					/* Your bog standard "6 color" printers */ \
	(  ICX_CYAN | ICX_MAGENTA | ICX_YELLOW | ICX_BLACK \
	 | ICX_LIGHT_CYAN | ICX_LIGHT_MAGENTA)

#define ICX_CMYKcmk					/* A more unusual "7 color" printer */ \
	(  ICX_CYAN | ICX_MAGENTA | ICX_YELLOW | ICX_BLACK \
	 | ICX_LIGHT_CYAN | ICX_LIGHT_MAGENTA | ICX_LIGHT_BLACK)

#define ICX_CMYKcmk1k				/* A more unusual "8 color" printer */ \
	(  ICX_CYAN | ICX_MAGENTA | ICX_YELLOW | ICX_BLACK \
	 | ICX_LIGHT_CYAN | ICX_LIGHT_MAGENTA | ICX_LIGHT_BLACK \
	 | ICX_LIGHT_LIGHT_BLACK)

#define ICX_CMYKOG					/* A "hexachrome" style extended gamut printer */ \
	(  ICX_CYAN | ICX_MAGENTA | ICX_YELLOW | ICX_BLACK \
	 | ICX_ORANGE | ICX_GREEN)

#define ICX_CMYKRB					/* A 6 color printer with red and blue. */ \
	(  ICX_CYAN | ICX_MAGENTA | ICX_YELLOW | ICX_BLACK \
	 | ICX_RED | ICX_BLUE)

#define ICX_CMYKOGcm				/* An 8 color extended gamut printer */ \
	(  ICX_CYAN | ICX_MAGENTA | ICX_YELLOW | ICX_BLACK \
	 | ICX_ORANGE | ICX_GREEN | ICX_LIGHT_CYAN | ICX_LIGHT_MAGENTA)

#define ICX_CMYKcm2c2m				/* An 8 color printer that wishes it had variable dot size */ \
	(  ICX_CYAN | ICX_MAGENTA | ICX_YELLOW | ICX_BLACK \
	 | ICX_LIGHT_CYAN | ICX_LIGHT_MAGENTA | ICX_MEDIUM_CYAN | ICX_MEDIUM_MAGENTA)

/* ------------------------------------------ */

/* Given an ink combination mask, return the number of recognised inks in it */
int icx_noofinks(inkmask mask);

/* Given an ink combination mask, return the 1-2 character based string */
/* If winv is nz, include ICX_INVERTED indicator if set */
/* Return NULL on error. free() after use */
char *icx_inkmask2char(inkmask mask, int winv);

/* Given the 1-2 character based string, return the ink combination mask */
/* Note that ICX_ADDITIVE will be guessed */
/* Return 0 if unrecognised character in string */
inkmask icx_char2inkmask(char *chstring); 

/* Given an ink combination mask that may contain light inks, */
/* return the corresponding ink mask without light inks. */
/* Return 0 if ink combination not recognised. */
inkmask icx_ink2primary_ink(inkmask mask);


/* Given an ink combination mask and a single ink mask, */
/* return the index number for that ink. */
/* Return -1 if mask1 not in mask */
int icx_ink2index(inkmask mask, inkmask mask1);

/* Given an ink combination mask and a index number, */
/* return the single ink mask. */
/* Return 0 if there are no inks at that index */
inkmask icx_index2ink(inkmask mask, int ixno);


/* Given a single ink mask, */
/* return its string representation */
char *icx_ink2string(inkmask mask); 

/* Given a single ink mask, */
/* return its 1-2 character representation */
char *icx_ink2char(inkmask mask); 

/* Given a single ink mask, */
/* return its Postscript string representation */
char *icx_ink2psstring(inkmask mask); 


/* Return an enumerated single colorant description */
/* Return 0 if no such enumeration, single colorant mask if there is */
inkmask icx_enum_colorant(int no, char **desc);

/* Return an enumerated colorant combination inkmask and description */
/* Return 0 if no such enumeration, colorant combination mask if there is */
inkmask icx_enum_colorant_comb(int no, char **desc);


/* Given an colorant combination mask, */
/* check if it matches the given ICC colorspace signature. */ 
/* return NZ if it does. */
int icx_colorant_comb_match_icc(inkmask mask, icColorSpaceSignature sig);

/* Given an ICC colorspace signature, return the appropriate */
/* colorant combination mask. Return 0 if ambiguous signature. */
inkmask icx_icc_to_colorant_comb(icColorSpaceSignature sig, icProfileClassSignature deviceClass);

/* Given an ICC colorspace signature, and a matching list */
/* of the D50 L*a*b* colors of the colorants, return the best matching */
/* colorant combination mask. Return 0 if not applicable to colorspace. */
inkmask icx_icc_cv_to_colorant_comb(icColorSpaceSignature sig, icProfileClassSignature deviceClass,
                                    double cvals[][3]);

/* Given an colorant combination mask */
/* return the primary matching ICC colorspace signature. */ 
/* return 0 if there is no match */
icColorSpaceSignature icx_colorant_comb_to_icc(inkmask mask);

/* --------------------------------------------------------- */
/* An aproximate device colorant model object lookup object: */

struct _icxColorantLu {
/* Public: */
	void (*del)(struct _icxColorantLu *s);	/* We're done with it */

	/* Conversions */
	void (*dev_to_XYZ)(struct _icxColorantLu *s, double *out, double *in);	/* Absolute */
	void (*dev_to_rLab)(struct _icxColorantLu *s, double *out, double *in);	/* Relative */

/* Private: */
	inkmask mask;				/* Colorant mask for this instance */
	int di;						/* Dimensionality */
	int whix, bkix;				/* White and Black Indexes into icx_inkTable[] */
	icmXYZNumber wp;			/* White point XYZ value */
	int iix[ICX_MXINKS];		/* Device Indexes into icx_inkTable[] */
	double Ynorm;				/* Y normalisation factor for Additive */
}; typedef struct _icxColorantLu icxColorantLu;

/* Create a icxColorantLu conversion object */
icxColorantLu *new_icxColorantLu(inkmask mask);

#endif /* XCOLORANTS_H */



































