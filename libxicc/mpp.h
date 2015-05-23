#ifndef MPP_H
#define MPP_H

/* 
 * Argyll Color Correction System
 * Model Printer Profile object.
 *
 * Author: Graeme W. Gill
 * Date:   24/2/2002
 *
 * Copyright 2002 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/* This version (based on mpp_x1) has n * 2^(n-1) shape params, */
/* used for linear interpolation of the shaping correction. */

/*
 * This object provides a model based printer forward profile
 * functionality, to support forward profiling and optimised
 * separation of printing devices with more than 4 inks.
 */

/* ------------------------------------------------------------------------------ */

#define MPP_MXINKS 8		/* Would like to be ICX_MXINKS but need more dynamic allocation */
#define MPP_MXTCORD 20		/* Maximum shaper harmonic orders to use */
#define MPP_MXCCOMB (1 << MPP_MXINKS)	/* Maximum number of primary combinations */
#define MPP_MXPARMS (MPP_MXINKS * MPP_MXTCORD + (MPP_MXINKS * MPP_MXCCOMB/2) + MPP_MXCCOMB)
							/* Maximum total parameters for a band */
#define MPP_MXBANDS 61		/* Maximum number of spectral bands (enought for 10nm) */

/* A test patch value */
typedef struct {
  /* public: */
	double *nv;			/* [MPP_MXINKS] Device values */
	double *band;		/* [3+MPP_MXBANDS];	Target XYZ & Spectral reflectance values */

  /* private: */
	double w;			/* Weight for this pass */
	double *lband;		/* [3+MPP_MXBANDS];	L* scale Target XYZ & Spectral reflectance values */
	double Lab[3];		/* Target Lab value */
	double tpcnv, tpcnv1;	/* Intermediate values for band oba without channel och */
	double *tcnv;		/* [MPP_MXINKS] Transfer curve corrected device values */
	double *scnv;		/* [MPP_MXINKS] Ideal shape correction values for device input */
	double *pcnv;		/* [MPP_MXCCOMB] Primary combination values (pre or post shape) */
	double *fcnv;		/* [MPP_MXINKS * MPP_MXCCOMB/2] shape interpolation weights for och */
	double cXYZ[3];		/* Current model XYZ value */
	double err;			/* Delta E squared */
} mppcol;

struct _mpp {

  /* Public: */
	void (*del)(struct _mpp *p);


	/* Create the mpp from scattered data points */
	/* Returns nz on error */
	int (*create) (struct _mpp *p,
	               int verb,				/* Vebosity level, 0 = none */
	               int quality,				/* Profile quality, 0..3 */
	               int display,				/* non-zero if display device */
	               double limit,			/* Total ink limit (not %) (if not display) */
	               inkmask imask,			/* Inkmask describing device colorspace */
	               int    spec_n,			/* Number of spectral bands, 0 if not valid */
	               double spec_wl_short,	/* First reading wavelength in nm (shortest) */
	               double spec_wl_long,		/* Last reading wavelength in nm (longest) */
	               double norm,				/* Normalising scale value */
	               instType itype,			/* Spectral instrument type (if not display) */
	               int no,					/* Number of points */
	               mppcol *points);			/* Array of input points */

	int (*write_mpp)(struct _mpp *p, char *filename, int lab);	/* write to a CGATS .mpp file */
	int (*read_mpp)(struct _mpp *p, char *filename);	/* read from a CGATS .mpp file */

	/* Get various types of information about the mpp */
	void (*get_info) (struct _mpp *p,
	                  inkmask *imask,		/* Inkmask, describing device colorspace */
	                  int *nodchan,			/* Number of device channels */
	                  double *limit,		/* Total ink limit (0.0 .. devchan) */
	                  int *spec_n,			/* Number of spectral bands, 0 if none */
	                  double *spec_wl_short,/* First reading wavelength in nm (shortest) */
	                  double *spec_wl_long, /* Last reading wavelength in nm (longest) */
	                  instType *itype,		/* Instrument type */
					  int *display);		/* NZ if display type */

	/* Set an illuminant and observer to use spectral model */
	/* for CIE lookup with optional FWA. Set both to default for XYZ mpp model. */
	/* Return 0 on OK, 1 on spectral not supported */
	/* If the model is for a display, the illuminant will be ignored. */
	int (*set_ilob) (struct _mpp *p,
		icxIllumeType ilType,			/* Illuminant type (icxIT_default for none) */
		xspect        *custIllum,		/* Custom illuminant (NULL for none) */
		icxObserverType obType,			/* Observer type (icxOT_default for none) */	
		xspect        custObserver[3],	/* Custom observer (NULL for none)  */
		icColorSpaceSignature  rcs,		/* Return color space, icSigXYZData or icSigLabData */
		int           use_fwa			/* NZ to involke FWA. */
	);

	/* Get the white and black points for this profile (default XYZ) */
	void (*get_wb) (struct _mpp *p,
	                  double *white,
	                  double *black,
	                  double *kblack);


	/* Lookup an XYZ or Lab color (default XYZ) */
	/* [will use spectral and FWA if configured] */
	void (*lookup) (struct _mpp *p,
	               double *out,					/* Returned XYZ or Lab */
	               double *in);					/* Input device values */

	/* Lookup an XYZ or Lab color with its partial derivative. */
	/* [will ignore spectral and FWA even if configured] */
	void (*dlookup)(struct _mpp *p,
	               double *out,					/* Return the XYZ or Lab */
	               double **dout,				/* Return the partial derivative dout[3][n] */
	               double *dev);

	/* Lookup an XYZ value. (never FWA corrected) */
	void (*lookup_xyz) (struct _mpp *p,
	               double *out,					/* Returned XYZ value */
	               double *in);					/* Input device values */

	/* Lookup a spectral value. (never FWA corrected) */
	void (*lookup_spec) (struct _mpp *p,
	               xspect *out,					/* Returned spectral value */
	               double *in);					/* Input device values */

	/* Return a gamut object, return NULL on error */
	gamut *(*get_gamut)(struct _mpp *p, double detail);	/* detail level 0.0 = default */

  /* Private: */
	int verb;				/* Verbose */

	/* General model information */
	int    display;			/* Non-zero if display profile rather than output */
	inkmask imask;			/* Inkmask describing device space */
	double limit;			/* Total ink limit (If output device) */
	int    spec_n;			/* Number of spectral bands, 0 if not valid */
	double spec_wl_short;	/* First reading wavelength in nm (shortest) */
	double spec_wl_long;	/* Last reading wavelength in nm (longest) */
	double norm;			/* Normalising scale value (will be 1.0) */
	instType itype;			/* Spectral instrument type (If output device) */
	mppcol white, black;	/* White and black points */
	mppcol kblack;			/* K only black point */

	/* Foward model parameters */
	int n;					/* Number of chanels */
	int nn;					/* Number of primary combinations = 1 << n  */
	int nnn2;				/* Total shape combinations = n * nn/2 */

	int cord;				/* Device transfer curve order (must be 1 <= cord <= MPP_MXTCORD) */
	double tc[MPP_MXINKS][3+MPP_MXBANDS][MPP_MXTCORD];	/* Device transfer curve parameters */
	int useshape;			/* Flag, NZ if shape parameters are being used */
	double ***shape;		/* [MPP_MXINKS][MPP_MXCCOMB][3+MPP_MXBANDS] Extra shaping parameters */
	double pc[MPP_MXCCOMB][3+MPP_MXBANDS];	/* Primary overlay combinations color values */

	/* Model housekeeping */
	/* Translate sparse shape color combo to compacted parameters and back */
	int f2c[MPP_MXINKS][MPP_MXCCOMB];		/* Full to Compact */
	struct { int ink; int comb; } c2f[MPP_MXINKS * MPP_MXCCOMB/2];	/* Compact to Full */

	/* Optimisation state */
	mppcol *otp;			/* Optimisation test point */
	int oit, ott;			/* Optimisation itteration and total itterations */
	int och;				/* Optimisation device channel 0..n-1 */
	int oba;				/* Optimisation band, 0..2 are XYZ, 3..spec_n+3 are spectral */
	double lpca[MPP_MXCCOMB][MPP_MXBANDS];	/* Primary combinations anchor L* band values */
	int nodp;				/* Number of device data points */
	mppcol *cols;			/* List of test points */
	double spmax;			/* Maximum spectral value of any sample and band */

	/* Lookup */
	icColorSpaceSignature pcs;	/* PCS to return, XYZ, Lab */
	xsp2cie *spc;			/* Spectral to CIE converter (NULL if using XYZ model) */

	/* Houskeeping */
	int errc;				/* Error code */
	char err[200];			/* Error message */
}; typedef struct _mpp mpp;

/* Create a new, uninitialised mpp */
mpp *new_mpp(void);


/* - - - - - - - - - - - - - */
/* mppcol utility functions */
/* Allocate the data portion of an mppcol */
/* Return NZ if malloc error */
int new_mppcol(
mppcol *p,			/* mppcol to allocate */
int n,				/* Number of inks */
int nb				/* Number of spectral bands */
);

/* Free the data allocation of an mppcol */
void del_mppcol(
mppcol *p,			/* mppcol to free */
int n,				/* Number of inks */
int nb				/* Number of spectral bands */
);

/* Copy the contents of one mppcol to another */
void copy_mppcol(
	mppcol *d,			/* Destination */
	mppcol *s,			/* Source */
	int n,				/* Number of inks */
	int nb				/* Number of spectral bands */
);

/* Allocate an array of mppcol, */
/* Return NULL if malloc error */
mppcol *new_mppcols(
	int no,				/* Number in array */
	int n,				/* Number of inks */
	int nb				/* Number of spectral bands */
);

/* Free an array of mppcol */
void del_mppcols(
	mppcol *p,			/* mppcol array to be free'd */
	int no,				/* Number in array */
	int n,				/* Number of inks */
	int nb				/* Number of spectral bands */
);

#endif /* MPP_H */




































