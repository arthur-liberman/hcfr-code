#ifndef XICC_H
#define XICC_H
/* 
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    28/6/00
 * Version: 1.00
 *
 * Copyright 2000, 2011 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUB LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Based on the old iccXfm class.
 */

/*
 * This library expands the icclib functionality,
 * particularly in creating and exercising color transforms.
 *
 * For the moment, this support is concentrated around the
 * Lut and Matrix subsets of the ICC profile tranforms.
 *
 * It is intended to allow color transformation, storage and retrieval
 * of ICC profiles, while permitting inititialization from scattered
 * data, reversing of the transform, and specific manipulations of
 * the internal processing elements.
 *
 * This class bases much of it's functionality on that of the
 * underlying icclib icmLuBase.
 */

#include "icc.h"		/* icclib ICC definitions */ 
#include "cgats.h"		/* CGAT format */	
#include "rspl.h"		/* rspllib thin plate spline definitions */
#include "gamut.h"		/* gamut definitions */
#include "xutils.h"		/* Utility functions */
#include "xcam.h"		/* CAM definitions */
#include "xspect.h"		/* Spectral conversion object */
#include "xcolorants.h"	/* Known colorants support */
#include "insttypes.h"	/* Instrument type support */
#include "mpp.h"		/* model printer profile support */
#include "moncurve.h"	/* monotonic curve support */
						/* (more at the end) */

#define XICC_USE_HK 1	/* [Set] Set to 1 to use Helmholtz-Kohlraush in all CAM conversions */
#define XICC_NEUTRAL_CMYK_BLACK		/* Use neutral axis black, else use K direction black. */
#define XICC_BLACK_POINT_TOLL 0.5			/* Tollerance of CMYK black point location */ 
#define XICC_BLACK_FIND_ABERR_WEIGHT 10.0	/* Weight of ab error against min L in BP */

/* ------------------------------------------------------------------------------ */

/* The "effective" PCS colorspace is the one specified by the PCS override, and */
/* visible in the overal profile conversion. The "native" PCS colorspace is the one */
/* that the underlying tags actually represent, and the PCS that the individual */
/* stages within each profile type handle (unless the ICX_MERGED flag is set, in which */
/* case the clut == native PCS appears to be the effective one). The conversion between */
/* the native and effective PCS is generally done in the to/from_abs() conversions. */

/* ------------------------------------------------------------------------------ */

/* Pseudo intents, valid as parameter to xicc get_luobj(): */

/* Default Color Appearance Space, based on absolute colorimetric */
#define icxAppearance ((icRenderingIntent)994)

/* Represents icAbsoluteColorimetric, converted to Color Appearance space */
#define icxAbsAppearance ((icRenderingIntent)995)	/* Fixed D50 white point */

/* Special Color Appearance Space, based on "absolute" perceptual */
#define icxPerceptualAppearance ((icRenderingIntent)996)

/* Default Color Appearance Space, based on "absolute" saturation */
#define icxSaturationAppearance ((icRenderingIntent)997)

/* These two are for completeness, they are unlikely to be useful: */
/* Special Color Appearance Space, based on "absolute" perceptual */
#define icxAbsPerceptualAppearance ((icRenderingIntent)998)

/* Default Color Appearance Space, based on "absolute" saturation */
#define icxAbsSaturationAppearance ((icRenderingIntent)999)


/* Pseudo PCS colospace returned as PCS for intent icxAppearanceJab */
#define icxSigJabData ((icColorSpaceSignature) icmMakeTag('J','a','b',' '))

/* Pseudo PCS colospace returned as PCS for intent icxAppearanceJCh */
#define icxSigJChData ((icColorSpaceSignature) icmMakeTag('J','C','h',' '))

/* Pseudo PCS colospace returned as PCS */
#define icxSigLChData ((icColorSpaceSignature) icmMakeTag('L','C','h',' '))

/* Return a string description of the given enumeration value */
const char *icx2str(icmEnumType etype, int enumval);


/* ------------------------------------------------------------------------------ */

/* A 3x3 matrix model */
struct _icxMatrixModel {
	void *imp;		/* Opaque implementation */

	icc *picc;		/* ICC profile used to set cone space matrix, NULL for Bradford. */
	int isLab;		/* Convert lookup to Lab */

	void (*force) (struct _icxMatrixModel *p, double *targ, double *in);
	void (*lookup) (struct _icxMatrixModel *p, double *out, double *in);
	void (*del) (struct _icxMatrixModel *p);

}; typedef struct _icxMatrixModel icxMatrixModel;

/* Create a matrix model of a set of points, and return an object to lookup */
/* points from the model. Return NULL on error. */
icxMatrixModel *new_MatrixModel(
icc *picc,			/* ICC profile used to set cone space matrix, NULL for Bradford. */
int verb,			/* NZ if verbose */
int nodp,			/* Number of points */
cow *ipoints,		/* Array of input points in XYZ space */
int isLab,			/* nz if data points are Lab */
int quality,		/* Quality metric, 0..3 (-1 == 2 orders only) */
int isLinear,		/* NZ if pure linear, gamma = 1.0 */
int isGamma,		/* NZ if gamma rather than shaper */
int isShTRC,		/* NZ if shared TRCs */
int shape0gam,		/* NZ if zero'th order shaper should be gamma function */
int clipbw,			/* Prevent white > 1 and -ve black */
int clipprims,		/* Prevent primaries going -ve */
double smooth,		/* Smoothing factor (nominal 1.0) */
double scale		/* Scale device values */
);

/* ------------------------------------------------------------------------------ */
/* Basic class keeps extra state for an icc, and provides the */
/* expanded set of methods. */

/* Black generation rule */
/* The rule is all in terms of device K and L* values */
typedef enum {
    icxKvalue    = 0,	/* K is specified as output K target by PCS auxiliary */
    icxKlocus    = 1,	/* K is specified as proportion of K locus by PCS auxiliary */
    icxKluma5    = 2,	/* K is specified as locus by 5 parameters as a function of L */
    icxKluma5k   = 3,	/* K is specified as K value by 5 parameters as a function of L */
    icxKl5l      = 4,	/* K is specified as locus by 2x5 parameters as a function of L and K locus aux */
    icxKl5lk      = 5	/* K is specified as K value by 2x5 parameters as a function of L and K value aux */
} icxKrule;

/* Curve parameters */
typedef struct {
	double Ksmth;		/* K smoothing filter extent */
	double Kstle;		/* K start level at white end (0.0 - 1.0)*/
	double Kstpo;		/* K start point as prop. of L locus (0.0 - 1.0) */
	double Kenpo;		/* K end point as prop. of L locus (0.0 - 1.0) */
	double Kenle;		/* K end level at Black end (0.0 - 1.0) */
	double Kshap;		/* K transition shape, 0.0-1.0 concave, 1.0-2.0 convex */
	double Kskew;		/* K transition shape skew, 1.0 = even */
} icxInkCurve;

/* Default black generation smoothing value */
#define ICXINKDEFSMTH 0.09	/* Transition window of +/- ICXINKDEFSMTH */
#define ICXINKDEFSKEW 2.0	/* Curve shape skew (matches typical device behaviour) */

/* Structure to convey inking details */
typedef struct {
	double tlimit;		/* Total ink limit, > 0.0 to < inputChan, < 0.0 == off */
	double klimit;		/* Black limit, > 0.0 to < 1.0, < 0.0 == off */
	icxKrule k_rule;	/* type of K generation rule */
	int KonlyLmin;		/* Use K only black Locus Lmin */
	icxInkCurve c;		/* K curve, or locus minimum curve */
	icxInkCurve x;		/* locus maximum curve if icxKl5l */
} icxInk;

/* Structure to convey inverse lookup clip handling details */
struct _icxClip {
	int     nearclip;				/* Flag - use near clipping not vector */
	int     LabLike;				/* Flag Its an Lab like colorspace */
	int     fdi;					/* Dimentionality of clip vector */
	double  ocent[MXDO];			/* base of center of clut output gamut */
	double  ocentv[MXDO];			/* vector direction of clut output clip target line */
	double  ocentl;					/* clip target line length */
}; typedef struct _icxClip icxClip;

/* Structure to convey viewing conditions */
typedef struct {
	ViewingCondition Ev;/* Enumerated Viewing Condition */
	double Wxyz[3];		/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
	double La;			/* Adapting/Surround Luminance cd/m^2 */
	double Yb;			/* Relative Luminance of Background to reference white */
	double Lv;			/* Luminance of white in the Image/Scene/Viewing field (cd/m^2) */
						/* Ignored if Ev is set to other than vc_none */
	double Yf;			/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
	double Yg;			/* Glare as a fraction of the adapting/surround (Y range 0.0 .. 1.0) */
	double Gxyz[3];		/* The Glare white coordinates (ie the Ambient color) */
						/* will be taken from Wxyz if Gxyz <= 0.0 */
	char *desc;			/* Possible description of this VC */
} icxViewCond;

/* Method of black point adaptation */
typedef enum {
	gmm_BPadpt    = 0,		/* Adapt source black point to destination */
	gmm_noBPadpt  = 1,		/* Don't adapt black point to destination */ 
	gmm_bendBP    = 2,		/* Don't adapt black point, bend it to dest. at end */  
	gmm_clipBP    = 3		/* Don't adapt black point, clip it to dest. at end */  
} icx_BPmap;

/* Structure to convey gamut mapping intent */
typedef struct {
	int    usecas;			/* 0x0 Use relative Lab space */
							/* 0x1 Use Absolute Lab Space */
							/* 0x2 Use Color Appearance Space */
							/* 0x3 Use Absolute Color Appearance Space */
							/* 0x102 Use Color Appearance Space with luminence scaling */
	int    usemap;			/* NZ if Gamut mapping should be used, else clip */
	double greymf;			/* Grey axis hue matching factor, 0.0 - 1.0 */
	double glumwcpf;		/* Grey axis luminance white compression factor, 0.0 - 1.0 */
	double glumwexf;		/* Grey axis luminance white expansion factor, 0.0 - 1.0 */
	double glumbcpf;		/* Grey axis luminance black compression factor, 0.0 - 1.0 */
	double glumbexf;		/* Grey axis luminance black expansion factor, 0.0 - 1.0 */
	double glumknf;			/* Grey axis luminance knee factor, 0.0 - 1.0 */
	icx_BPmap bph;			/* Method of black point adapation */
	double gamcpf;			/* Gamut compression factor, 0.0 - 1.0 */
	double gamexf;			/* Gamut expansion factor, 0.0 - 1.0 */
	double gamcknf;			/* Gamut compression knee factor, 0.0 - 1.0 */
	double gamxknf;			/* Gamut expansion knee factor, 0.0 - 1.0 */
	double gampwf;			/* Gamut Perceptual Map weighting factor, 0.0 - 1.0 */
	double gamswf;			/* Gamut Saturation Map weighting factor, 0.0 - 1.0 */
	double satenh;			/* Saturation enhancement value, 0.0 - Inf */
	char *as;				/* Alias string (option name) */
	char *desc;				/* Possible description of this VC */
	icRenderingIntent icci;	/* Closest ICC intent */
} icxGMappingIntent;

struct _xicc {
	/* Private: */
	icc *pp;			/* ICC profile we expand */

	struct _xcal *cal;	/* Optional device cal, NULL if none */
	int nodel_cal;		/* Flag, nz if cal was provided externally and shouldn't be deleted */

	/* Public: */
	void                 (*del)(struct _xicc *p);

	/* "use" flags */
#define ICX_CLIP_VECTOR  0x0000		/* If clipping is needed, clip in vector direction (default) */
#define ICX_CLIP_NEAREST 0x0010		/* If clipping is needed, clip to nearest */
#define ICX_MERGE_CLUT   0x0020		/* Merge the output() and out_abs() into the clut(), */
									/* for improved performance on reading, and */
									/* clipping in effective output space on inverse lookup. */
									/* Reduces accuracy noticably though. */
									/* Output output() and out_abs() become NOPs */
#define ICX_CAM_CLIP     0x0100		/* Use CAM space during invfwd clipping lookup, */
									/* irrespective of the native or effective PCS. */
									/* Ignored if MERGE_CLUT is set or vector clip is used. */
									/* May halve the inverse lookup performance! */ 
#define ICX_INT_SEPARATE 0x0400		/* Handle 4 dimensional devices with fixed inking rules */
									/* with an optimised internal separation pass, rather */
									/* than a point by point inverse locus lookup . */
									/* NOT IMPLEMENTED YET */
#define ICX_FAST_SETUP   0x0800		/* Improve initial setup speed at the cost of throughput */
#define ICX_VERBOSE      0x8000		/* Turn on verboseness during creation */

	                                /* Returm a lookup object from the icc */
	struct _icxLuBase *  (*get_luobj) (struct _xicc *p,
	                                   int flags,				/* clip, merge flags */
	                                   icmLookupFunc func,		/* Functionality */
	                                   icRenderingIntent intent,/* Intent */
	                                   icColorSpaceSignature pcsor,	/* PCS override (0 = def) */
	                                   icmLookupOrder order,	/* Search Order */
	                                   icxViewCond *vc,			/* Viewing Condition - only */
	                                                            /* used if pcsor == CIECAM. */
																/* or ICX_CAM_CLIP flag. */
	                                   icxInk *ink);			/* inking details (NULL = def) */


										/* Set a xluobj and icc table from scattered data */
										/* Return appropriate lookup object */
										/* NULL on error, check errc+err for reason */
	/* "create" flags */
#define ICX_SET_WHITE       0x00010000	/* find, set and make relative to the white point */
#define ICX_SET_WHITE_US    0x00030000	/* find, set and make relative to the white point hue, */
										/* but not scale to W L value, to avoid input clipping */
#define ICX_SET_WHITE_C     0x00050000	/* find, set and make relative to the white point hue, */
										/* and clip any cLUT values over D50 to D50 */
#define ICX_SET_BLACK       0x00080000	/* find and set the black point */
#define ICX_WRITE_WBL       0x00100000	/* Matrix: write White, Black & Luminance tags */
#define ICX_CLIP_WB         0x00200000	/* Clip white and black to be < 1 and > 0 respectively */
#define ICX_CLIP_PRIMS      0x00400000	/* Clip matrix primaries to be > 0 */
#define ICX_NO_IN_SHP_LUTS  0x00800000	/* Lut/Mtx: Don't create input (Device) shaper curves. */
#define ICX_NO_IN_POS_LUTS  0x01000000	/* LuLut: Don't create input (Device) postion curves. */
#define ICX_NO_OUT_LUTS     0x02000000	/* LuLut: Don't create output (PCS) curves. */
//#define ICX_2PASSSMTH     0x04000000	/* If LuLut: Use Gaussian smoothing */
//#define ICX_EXTRA_FIT     0x08000000	/* If LuLut: Counteract scat data point errors. */
/* And  ICX_VERBOSE         			   Turn on verboseness during creation */
	struct _icxLuBase * (*set_luobj) (struct _xicc *p,
	                                  icmLookupFunc func,		/* Functionality to set */
	                                  icRenderingIntent intent,	/* Intent to set */
	                                  icmLookupOrder order,		/* Search Order */
	                                  int flags,				/* white/black point flags */
	                                  int no,					/* Total Number of points */
	                                  int nobw,					/* Number of points to look */
																/* for white & black patches in */
	                                  cow *points,				/* Array of input points */
	                                  icxMatrixModel *skm, 		/* Optional skeleton model */
	                                  double dispLuminance,		/* > 0.0 if display luminance */
	                                                                    /* value and is known */
	                                  double wpscale,			/* > 0.0 if input white pt is */
	                                                                    /* is to be scaled */
//									  double *bpo, 				/* != NULL black point override */
	                                  double smooth,			/* RSPL smoothing factor, */
	                                                                        /* -ve if raw */
	                                  double avgdev,			/* Avge Dev. of points */
	                                  double demph,				/* cLut dark emphasis factor */
	                                  icxViewCond *vc,			/* Viewing Condition - only */
	                                                            /* used if pcsor == CIECAM. */
																/* or ICX_CAM_CLIP flag. */
	                                  icxInk *ink,				/* inking details */
	                                  struct _xcal *cal,		/* Optional cal Will override any */
																/* existing, not deltd with xicc. */
	                                  int quality);				/* Quality metric, 0..3 */


								/* Return the devices viewing conditions. */
								/* Return value 0 if it is well defined */
								/* Return value 1 if it is a guess */
								/* Return value 2 if it is not possible/appropriate */
	int     (*get_viewcond)(struct _xicc *p, icxViewCond *vc);

	char             err[512];			/* Error message */
	int              errc;				/* Error code */
}; typedef struct _xicc xicc;

/* ~~~~~ */
/* Might be good to add a slow but very precise vector and closest "clip to gamut" */
/* function for use in setting white and black points. Use this in profile. */

xicc *new_xicc(icc *picc);

/* ------------------------------------------------------------------------------ */
/* Expanded lookup object support */
#define XLU_BASE_MEMBERS																\
	/* Private: */																		\
	int              trace;					/* Optional run time tracing flag */		\
	struct _xicc    *pp;					/* Pointer to XICC we're a part of */		\
	icmLuBase       *plu;					/* Pointer to icm Lu we are expanding */	\
	int              flags;					/* Flags passed to get_luobj */				\
	icmLookupFunc    func;					/* Function passed to get_luobj */			\
	icRenderingIntent intent;				/* Effective/External Intent */				\
											/* "in" and "out" are in reference to */	\
											/* the requested lookup direction. */		\
    icColorSpaceSignature ins;				/* Effective/External Clr space of input */	\
    icColorSpaceSignature outs;				/* Effective/External Clr space of output */\
	icColorSpaceSignature pcs;				/* Effective/External PCS */				\
    icColorSpaceSignature natis;			/* Native input Clr space */				\
    icColorSpaceSignature natos;			/* Native output Clr space */				\
    icColorSpaceSignature natpcs;			/* Native PCS Clr space */					\
    int	inputChan;      					/* Num of input channels */					\
    int	outputChan;     					/* Num of output channels */				\
	double ninmin[MXDI];					/* icc Native Input color space minimum */	\
	double ninmax[MXDI];					/* icc Native Input color space maximum */	\
	double noutmin[MXDO];					/* icc Native Output color space minimum */	\
	double noutmax[MXDO];					/* icc Native Output color space maximum */	\
	double inmin[MXDI];						/* icx Effective Input color space minimum */	\
	double inmax[MXDI];						/* icx Effective Input color space maximum */	\
	double outmin[MXDO];					/* icx Effective Output color space minimum */	\
	double outmax[MXDO];					/* icx Effective Output color space maximum */	\
	icxViewCond  vc;						/* Viewing Condition for CIECAM97s */		\
	icxcam      *cam;						/* CAM conversion */						\
																						\
	/* Attributes inhereted by ixcLu's */												\
	int noisluts;	/* Flag - If LuLut: Don't create input (Device) shaper curves. */			\
	int noipluts;	/* Flag - If LuLut: Don't create input (Device) position curves. */	\
	int nooluts;	/* Flag - If LuLut: Don't create output (PCS) curves. */			\
	int nearclip;	/* Flag - If clipping occurs, return the nearest solution, */		\
	int mergeclut;	/* Flag - If LuLut: Merge output() and out_abs() into clut(). */	\
	int camclip;	/* Flag - If LuLut: Use CIECAM for clut reverse lookup clipping */ \
	int intsep;		/* Flag - If LuLut: Do internal separation for 4d device */			\
	int fastsetup;	/* Flag - If LuLut: Do fast setup at cost of slower throughput */	\
																						\
	/* Public: */																		\
	void    (*del)(struct _icxLuBase *p);												\
																						\
								/* Return Internal native colorspaces */				\
	void    (*lutspaces) (struct _icxLuBase *p, icColorSpaceSignature *ins, int *inn,	\
	                                        icColorSpaceSignature *outs, int *outn,		\
	                                        icColorSpaceSignature *pcs);				\
																						\
								/* External effective colorspaces */       	    		\
	void    (*spaces) (struct _icxLuBase *p, icColorSpaceSignature *ins, int *inn,		\
	                                     icColorSpaceSignature *outs, int *outn,		\
	                                     icmLuAlgType *alg, icRenderingIntent *intt,	 \
	                                     icmLookupFunc *fnc, icColorSpaceSignature *pcs); \
																						\
	/* Get the Native input space and output space ranges */							\
	void (*get_native_ranges) (struct _icxLuBase *p,										\
		double *inmin, double *inmax,		/* Maximum range of inspace values */		\
		double *outmin, double *outmax);	/* Maximum range of outspace values */		\
																						\
																						\
	/* Get the Effective input space and output space ranges */							\
	void (*get_ranges) (struct _icxLuBase *p,											\
		double *inmin, double *inmax,		/* Maximum range of inspace values */		\
		double *outmin, double *outmax);	/* Maximum range of outspace values */		\
																						\
																						\
	/* Return the media white and black points */										\
	/* in the effective PCS colorspace. */												\
	/* (ie. these will be relative values for relative intent etc.) */					\
	void    (*efv_wh_bk_points)(struct _icxLuBase *p, double *wht, double *blk, double *kblk);	\
																						\
	/* Translate color values through profile */										\
	/* 0 = success */																	\
	/* 1 = warning: clipping occured */													\
	/* 2 = fatal: other error */														\
																						\
	/* (Note that clipping is not a reliable means of detecting out of gamut in the */	\
	/* lookup(bwd) call for clut based profiles, but is for inv_lookup() calls.) */		\
																						\
	int            (*lookup) (struct _icxLuBase *p, double *out, double *in);			\
											/* Requested conversion */					\
	int			   (*inv_lookup) (struct _icxLuBase *p, double *out, double *in);		\
											/* Inverse conversion */					\
																						\
	/* Given an xicc lookup object, returm a gamut object. */							\
	/* Note that the Effective PCS must be Lab or Jab */								\
	/* A icxLuLut type must be icmFwd or icmBwd, */										\
	/* and for icmFwd, the ink limit (if supplied) */									\
	/* will be applied. */																\
	/* Return NULL on error, check xicc errc+err for reason */							\
	gamut * (*get_gamut) (struct _icxLuBase *plu,	/* xicc lookup object */			\
	                      double detail);			/* gamut detail level, 0.0 = def */	\
																						\
	/* The following two functions expose the relative colorimetric native ICC PCS */	\
	/* <--> absolute/CAM space transform, so that CAM based gamut compression */		\
	/* can be applied in creating the ICC Lut tabls in profout.c. */					\
																						\
	/* Given a native ICC relative XYZ or Lab PCS value, convert in the fwd */			\
	/* direction into the nominated Effective output PCS (ie. Absolute, Jab etc.) */	\
	void (*fwd_relpcs_outpcs) (struct _icxLuBase *p, icColorSpaceSignature is,			\
	                                                   double *out, double *in);		\
																						\
	/* Given a nominated Effective output PCS (ie. Absolute, Jab etc.), convert it */	\
	/* in the bwd direction into a native ICC relative XYZ or Lab PCS value */			\
	void (*bwd_outpcs_relpcs) (struct _icxLuBase *p, icColorSpaceSignature os,			\
	                                                   double *out, double *in);		\
																						\


/* Base xlookup object */
struct _icxLuBase {
	XLU_BASE_MEMBERS
}; typedef struct _icxLuBase icxLuBase;

/* Monochrome  Fwd & Bwd type object */
struct _icxLuMono {
	XLU_BASE_MEMBERS

	/* Overall lookups */
	int (*fwd_lookup) (struct _icxLuBase *p, double *out, double *in);
	int (*bwd_lookup) (struct _icxLuBase *p, double *out, double *in);

	/* Components of Device to PCS lookup */
	int (*fwd_curve) (struct _icxLuMono *p, double *out, double *in);
	int (*fwd_map)   (struct _icxLuMono *p, double *out, double *in);
	int (*fwd_abs)   (struct _icxLuMono *p, double *out, double *in);

	/* Components of PCS to Device lookup */
	int (*bwd_abs)   (struct _icxLuMono *p, double *out, double *in);
	int (*bwd_map)   (struct _icxLuMono *p, double *out, double *in);
	int (*bwd_curve) (struct _icxLuMono *p, double *out, double *in);

}; typedef struct _icxLuMono icxLuMono;

/* 3D Matrix Fwd & Bwd type object */
struct _icxLuMatrix {
	XLU_BASE_MEMBERS

	/* Overall lookups */
	int (*fwd_lookup) (struct _icxLuBase *p, double *out, double *in);
	int (*bwd_lookup) (struct _icxLuBase *p, double *out, double *in);

	/* Components of Device to PCS lookup */
	int (*fwd_curve)  (struct _icxLuMatrix *p, double *out, double *in);
	int (*fwd_matrix) (struct _icxLuMatrix *p, double *out, double *in);
	int (*fwd_abs)    (struct _icxLuMatrix *p, double *out, double *in);

	/* Components of PCS to Device lookup */
	int (*bwd_abs)    (struct _icxLuMatrix *p, double *out, double *in);
	int (*bwd_matrix) (struct _icxLuMatrix *p, double *out, double *in);
	int (*bwd_curve)  (struct _icxLuMatrix *p, double *out, double *in);

}; typedef struct _icxLuMatrix icxLuMatrix;

/* Multi-D. Lut type object */
struct _icxLuLut {
	XLU_BASE_MEMBERS

	/* private: */
	icmLut *lut;							/* ICC Lut that is being used */
	rspl	        *inputTable[MXDI];		/* The input lookups */
	rspl	        *clutTable;				/* The multi dimention lookup */
	rspl	        *cclutTable;			/* Alternate multi dimention lookup in CAM space */
	rspl	        *outputTable[MXDO];		/* The output lookups */

	/* Inverted RSPLs used to speed ink limit calculation */
	/* input' -> input */
	rspl *revinputTable[MXDI];

	/* In/Out lookup flags used for rspl init. callback */
	int iol_out;	/* Non-zero if output lookup */
	int iol_ch;		/* Channel */

	/* In/Out inversion support */
	double inputClipc[MXDI];	/* The input lookups clip center */
	double outputClipc[MXDO];	/* The output lookups clip center */

	/* clut inversion support */
	double      icent[MXDI];		/* center of input gamut */
	double      licent[MXDI];		/* last icent value used */

	icxClip clip;					/* Clip setup information */

	int kch;						/* Black ink channel if discovered */
									/* -1 if not known or applicable */
									/* (Only set by icxLu_comp_bk_point()) */
	icxInk      ink;				/* inking details */
	double Lmin, Lmax;				/* L min/max for inking rule */

	/* Auxiliary parameter flags, non-zero for inputs that will be */
	/* used as auxiliary parameters the rspl input */
	/* dimensionality exceeds the output dimension (i.e. CMYK->Lab) */
	int auxm[MXDI];	

	/* Auxiliar linearization function - NULL if none */
 	/* Only the used auxiliary chanels need be calculated. */
	/* ~~ not implimented yet ~~~ */
//	void (*auxlinf)(void *auxlinf_ctx, double inout[MXDI]);

	/* Opaque context for auxlin */
//	void *auxlinf_ctx;

	/* Temporary icm fwd abs XYZ LuLut used for setting up icx clut */
	icmLuBase *absxyzlu;

	/* Optional function to compute the input chanel */
 	/* sum from the raw rspl input values. NULL if not used. */
	/* Use this to take account of any transformation beyond */
	/* the input space, or 6 color masquerading as 4 etc. */
	double (*limitf)(void *limitf_ctx, float in[MXDI]);		/* ~~ not implimented yet */

	/* Opaque context for limitf */
	void *limitf_ctx;

	/* Input space sum limit. Points with a limitf() over */
	/* this number will not be considered in gamut. Valid if gt 0 */
	double slimit;

	/* public: */

	/* Note that black inking rules are always defined and provided */
	/* in dev[] and pcs[] space, even for component functions */
	/* (ie. the implementation of the inking rule deals with */
	/*  the dev<->dev' and pcs<->pcs' conversions) */
	
	/* Requested direction component lookup */
	int (*in_abs)  (struct _icxLuLut *p, double *out, double *in);
	int (*matrix)  (struct _icxLuLut *p, double *out, double *in);
	int (*input)   (struct _icxLuLut *p, double *out, double *in);
	int (*clut)    (struct _icxLuLut *p, double *out, double *in);
	int (*clut_aux)(struct _icxLuLut *p, double *out, double *olimit,
	                                     double *auxv, double *in);
	int (*output)  (struct _icxLuLut *p, double *out, double *in);
	int (*out_abs) (struct _icxLuLut *p, double *out, double *in);

	/* Inverse direction component lookup (in reverse order) */
	int (*inv_out_abs) (struct _icxLuLut *p, double *out, double *in);
	int (*inv_output)  (struct _icxLuLut *p, double *out, double *in);
	int (*inv_clut)    (struct _icxLuLut *p, double *out, double *in);
	int (*inv_clut_aux)(struct _icxLuLut *p, double *out, double *auxv,
                 double *auxr, double *auxt, double *clipd, double *in);
	int (*inv_input)   (struct _icxLuLut *p, double *out, double *in);
	int (*inv_matrix)  (struct _icxLuLut *p, double *out, double *in);
	int (*inv_in_abs)  (struct _icxLuLut *p, double *out, double *in);

	/* Get locus information for a clut (see xlut.c for details) */
	int (*clut_locus)  (struct _icxLuLut *p, double *locus, double *out, double *in);

	/* Get various types of information about the LuLut */
	void (*get_info) (struct _icxLuLut *p, icmLut **lutp,
	                 icmXYZNumber *pcswhtp, icmXYZNumber *whitep,
	                 icmXYZNumber *blackp);

	/* Get the matrix contents */
	void (*get_matrix) (struct _icxLuLut *p, double m[3][3]);

}; typedef struct _icxLuLut icxLuLut;

/* ------------------------------------------------------------------------------ */
/* Utility declarations and functions */

/* Profile Creation Suplimental Information structure */
struct _profxinf {
    icmSig manufacturer;	/* Device manufacturer ICC Sig, 0 for default */
	char *deviceMfgDesc;	/* Manufacturer text description, NULL for none */

    icmSig model;			/* Device model ICC Sig, 0 for default */
	char *modelDesc;		/* Model text description, NULL for none */

    icmSig creator;			/* Profile creator ICC Sig, 0 for default */

	char *profDesc;			/* Text profile description, NULL for default */

	char *copyright;		/* Copyrigh text, NULL for default */

	/* Attribute flags */
	int transparency;		/* NZ for Trasparency, else Reflective */
	int matte;				/* NZ for Matte, else Glossy */
	int negative;			/* NZ for Negative, else Positive */
	int blackandwhite;		/* NZ for BlackAndWhite, else Color */

	/* Default intent */
	icRenderingIntent  default_ri;	/* Default rendering intent */

	/* Other stuff ICC ?? */

}; typedef struct _profxinf profxinf;

/* Set an icc's Lut tables, and take care of auxiliary continuity problems. */
/* Only useful if there are auxiliary device output chanels to be set. */
int icxLut_set_tables_auxfix(
icmLut *p,						/* Pointer to icmLut object */
void   *cbctx,							/* Opaque callback context pointer value */
icColorSpaceSignature insig, 			/* Input color space */
icColorSpaceSignature outsig, 			/* Output color space */
void (*infunc)(void *cbctx, double *out, double *in),
						/* Input transfer function, inspace->inspace' (NULL = default) */
double *inmin, double *inmax,			/* Maximum range of inspace' values */
										/* (NULL = default) */
void (*clutfunc)(void *cbntx, double *out, double *aux, double *auxr, double *pcs, double *in),
						/* inspace' -> outspace' transfer function, also */
						/* return the internal target PCS and the  (packed) auxiliary locus */
						/* range as [min0, max0, min1, max1...], and the actual auxiliary */
						/* target used. */
void (*clutpcsfunc)(void *cbntx, double *out, double *aux, double *pcs),
						/* Internal PCS + actual aux_target -> outspace' transfer function */
void (*clutipcsfunc)(void *cbntx, double *pcs, double *olimit, double *auxv, double *in),
						/* outspace' -> Internal PCS + auxv check function */
double *clutmin, double *clutmax,		/* Maximum range of outspace' values */
										/* (NULL = default) */
void (*outfunc)(void *cbntx, double *out, double *in)
						/* Output transfer function, outspace'->outspace (NULL = deflt) */
);

		
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
);

/* Debug: dump a Viewing Condition to standard out */
void xicc_dump_viewcond(icxViewCond *vc);

/* Debug: dump an Inking setup to standard out */
void xicc_dump_inking(icxInk *ik);

/* Return enumerated gamut mapping intents */
/* Return 0 if OK, 1 if there is no such enumeration. */
/* Note the following fixed numbers meanings: */
#define icxNoGMIntent -1
#define icxDefaultGMIntent -2
#define icxAbsoluteGMIntent -3
#define icxRelativeGMIntent -4
#define icxPerceptualGMIntent -5
#define icxSaturationGMIntent -6
#define icxIllegalGMIntent -999
int xicc_enum_gmapintent(icxGMappingIntent *gmi, int no, char *as);
void xicc_dump_gmi(icxGMappingIntent *gmi);

/* - - - - - - - - - - */
/* Utility functions: */

/* Given an open icc profile, */
/* guess which channel is the black. */
/* Return -1 if there is no black channel or it can't be guessed */
int icxGuessBlackChan(icc *p);

/* Given an icc profile, try and create an xcal */
/* Return NULL on error or no cal */
struct _xcal *xiccReadCalTag(icc *p);

/* A callback that uses an xcal, that can be used with icc get_tac */
void xiccCalCallback(void *cntx, double *out, double *in);

/* Given an xicc icc profile, estmate the total ink limit and black ink limit. */
void icxGetLimits(xicc *p, double *tlimit, double *klimit);

/* Using the above function, set default total and black ink values */
void icxDefaultLimits(xicc *p, double *tlout, double tlin, double *klout, double klin);

/* Given a calibrated total ink limit and an xcal, return the */
/* equivalent underlying (pre-calibration) total ink limit. */
/* This is the maximum equivalent, that makes sure that */
/* the calibrated limit is met or exceeded. */
double icxMaxUnderlyingLimit(struct _xcal *cal, double ilimit);

/* - - - - - - - - - - */

/* Utility function - compute the clip vector direction. */
/* return NULL if vector clip isn't used. */
double *icxClipVector(
icxClip *p,			/* Clipping setup information */
double *in,			/* Target point */
double *cdirv		/* Space for returned clip vector */
);

/* - - - - - - - - - - */

/* CIE XYZ to perceptual Lab with partial derivatives. */
void icxdXYZ2Lab(icmXYZNumber *w, double *out, double dout[3][3], double *in);

/* Return the normal Delta E squared, given two Lab values, */
/* including partial derivatives. */
double icxdLabDEsq(double dout[2][3], double *Lab0, double *Lab1);

/* Return the CIE94 Delta E color difference measure, squared */
/* including partial derivatives. */
double icxdCIE94sq(double dout[2][3], double Lab0[3], double Lab1[3]);

/* Return the normal Delta E given two Lab values, */
/* including partial derivatives. */
double icxdLabDE(double dout[2][3], double *Lab0, double *Lab1);

/* Return the CIE94 Delta E color difference measure */
/* including partial derivatives. */
double icxdCIE94(double dout[2][3], double Lab0[3], double Lab1[3]);

/* - - - - - - - - - - */
/* Power like function, based on Graphics Gems adjustment curve. */
/* Avoids "toe" problem of pure power. */
/* Adjusted so that "power" 2 and 0.5 agree with real power at 0.5 */
double icx_powlike(double vv, double pp);

/* Compute the necessary aproximate power, to transform */
/* the given value from src to dst. They are assumed to be */
/* in the range 0.0 .. 1.0 */
double icx_powlike_needed(double src, double dst);

/* - - - - - - - - - - */

/* Transfer function */
double icxTransFunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
);

/* Inverse Transfer function */
double icxInvTransFunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
);

/* Transfer function with scaling */
double icxSTransFunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
);

/* Inverse Transfer function with scaling */
double icxInvSTransFunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
);

/* Transfer function with partial derivative */
/* with respect to the parameters. */
double icxdpTransFunc(
double *v,			/* Pointer to first parameter */
double *dv,			/* Return derivative wrt each parameter [luord] */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
);

/* Transfer function with scaling and */
/* partial derivative with respect to the parameters. */
double icxdpSTransFunc(
double *v,			/* Pointer to first parameter */
double *dv,			/* Return derivative wrt each parameter [luord] */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
);

/* Transfer function with partial derivative */
/* with respect to the input value. */
double icxdiTransFunc(
double *v,			/* Pointer to first parameter */
double *pdv,		/* Return derivative wrt source value [1] */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
);

/* Transfer function with scaling and */
/* partial derivative with respect to the input value. */
double icxdiSTransFunc(
double *v,			/* Pointer to first parameter */
double *pdv,		/* Return derivative wrt source value [1] */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
);

/* Transfer function with partial derivative */
/* with respect to the parameters and the input value. */
double icxdpdiTransFunc(
double *v,			/* Pointer to first parameter */
double *dv,			/* Return derivative wrt each parameter [luord] */
double *pdin,		/* Return derivative wrt source value [1] */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
);

/* Transfer function with scaling and */
/* partial derivative with respect to the */
/* parameters and the input value. */
double icxdpdiSTransFunc(
double *v,			/* Pointer to first parameter */
double *dv,			/* Return derivative wrt each parameter [luord] */
double *pdin,		/* Return derivative wrt source value [1] */
int    luord,		/* Number of parameters */
double vv,			/* Source of value */
double min,			/* Scale values */
double max
);

/* Should add/move the spectro/moncurve stuff in here, */
/* since it has offset and scaling. */

/* - - - - - - - - - - */
/* Multi-plane interpolation - uses base + di slope values.  */
/* Parameters are assumed to be fdi groups of di + 1 parameters. */
void icxPlaneInterp(
double *v,			/* Pointer to first parameter [fdi * (di + 1)] */
int    fdi,			/* Number of output channels */
int    di,			/* Number of input channels */
double *out,		/* Resulting fdi values */
double *in			/* Input di values */
);

/* Matrix cube interpolation. with partial derivative */
/* with respect to the input and parameters. */
void icxdpdiPlaneInterp(
double *v,			/* Pointer to first parameter value [fdi * (di + 1)] */
double *dv,			/* Return [1 + di] deriv. wrt each parameter v */
double *din,		/* Return [fdi * di] deriv. wrt each input value */
int    fdi,			/* Number of output channels */
int    di,			/* Number of input channels */
double *out,		/* Resulting fdi values */
double *in			/* Input di values */
);

/* - - - - - - - - - - */

/* Matrix cube interpolation - interpolate between 2^di output corner values. */
/* Parameters are assumed to be fdi groups of 2^di parameters. */
void icxCubeInterp(
double *v,			/* Pointer to first parameter */
int    fdi,			/* Number of output channels */
int    di,			/* Number of input channels */
double *out,		/* Resulting fdi values */
double *in			/* Input di values */
);

/* Matrix cube interpolation. with partial derivative */
/* with respect to the input and parameters. */
void icxdpdiCubeInterp(
double *v,			/* Pointer to first parameter value */
double *dv,			/* Return [fdi * 2^di] deriv wrt each parameter v */
double *din,		/* Return [fdi * di] deriv wrt each input value */
int    fdi,			/* Number of output channels */
int    di,			/* Number of input channels */
double *out,		/* Resulting fdi values */
double *in			/* Input di values */
);

/* - - - - - - - - - - */

/* 3x3 matrix in 1D array multiplication */
void icxMulBy3x3Parm(
	double out[3],			/* Return input multiplied by matrix */
	double mat[9],			/* Matrix organised in [slow][fast] order */
	double in[3]			/* Input values */
);

/* 3x3 matrix in 1D array multiplication, with partial derivatives */
/* with respect to just the input. */
void icxdpdiiMulBy3x3Parm(
	double out[3],			/* Return input multiplied by matrix */
	double din[3][3],		/* Return deriv for each [output] with respect to [input] */
	double mat[9],			/* Matrix organised in [slow][fast] order */
	double in[3]			/* Input values */
);

/* 3x3 matrix in 1D array multiplication, with partial derivatives */
/* with respect to the input and parameters. */
void icxdpdiMulBy3x3Parm(
	double out[3],			/* Return input multiplied by matrix */
	double dv[3][9],		/* Return deriv for each [output] with respect to [param] */
	double din[3][3],		/* Return deriv for each [output] with respect to [input] */
	double mat[9],			/* Matrix organised in [slow][fast] order */
	double in[3]			/* Input values */
);

/* - - - - - - - - - - */

#include "xcal.h"

#endif /* XICC_H */



































