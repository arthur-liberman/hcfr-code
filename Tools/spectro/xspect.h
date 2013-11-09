
#ifndef XSPECT_H
#define XSPECT_H

/* 
 * Author:  Graeme W. Gill
 * Date:    21/6/01
 * Version: 1.00
 *
 * Copyright 2000 - 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/*
 * This class supports converting spectral samples
 * into CIE XYZ or D50 Lab tristimulous values.
 */

/*
 * TTBD:
 *
 */

#ifndef SALONEINSTLIB
#include "icc.h"		/* icclib ICC definitions */ 
#else /* SALONEINSTLIB */
#include "conv.h"		/* fake icclib ICC definitions */ 
#endif /* SALONEINSTLIB */

#ifdef __cplusplus
	extern "C" {
#endif

/* ------------------------------------------------------------------------------ */

/* Structure for conveying spectral information */

/* NOTE :- should ditch norm, and replace it by */
/* "units", ie. reflectance/transmittance 0..1, 0..100%, */
/* W/nm/m^2 or mW/nm/m^2 */
#define XSPECT_MAX_BANDS 601		/* Enought for 1nm from 300 to 900 */

typedef struct {
	int    spec_n;					/* Number of spectral bands, 0 if not valid */
	double spec_wl_short;			/* First reading wavelength in nm (shortest) */
	double spec_wl_long;			/* Last reading wavelength in nm (longest) */
	double norm;					/* Normalising scale value */
	double spec[XSPECT_MAX_BANDS];	/* Spectral value, shortest to longest */
} xspect;

/* Some helpful macro's: */

/* Copy everything except the spectral values */
#define XSPECT_COPY_INFO(PDST, PSRC) 						\
 (PDST)->spec_n = (PSRC)->spec_n,							\
 (PDST)->spec_wl_short = (PSRC)->spec_wl_short,				\
 (PDST)->spec_wl_long = (PSRC)->spec_wl_long,				\
 (PDST)->norm = (PSRC)->norm

/* Given an index and the sampling ranges, compute the sample wavelength */
#define XSPECT_WL(SHORT, LONG, N, IX) \
((SHORT) + (double)(IX) * ((LONG) - (SHORT))/((N)-1.0))

/* Given the address of an xspect and an index, compute the sample wavelegth */
#define XSPECT_XWL(PXSP, IX) \
(((PXSP)->spec_wl_short) + (double)(IX) * (((PXSP)->spec_wl_long) - ((PXSP)->spec_wl_short))/(((PXSP)->spec_n)-1.0))

/* Given a wavelength and the sampling ranges, compute the double index */
#define XSPECT_DIX(SHORT, LONG, N, WL) \
(((N)-1.0) * ((WL) - (SHORT))/((LONG) - (SHORT)))

/* Given the wavelength and address of an xspect, compute the double index */
#define XSPECT_XDIX(PXSP, WL) \
(((PXSP)->spec_n-1.0) * ((WL) - ((PXSP)->spec_wl_short))/(((PXSP)->spec_wl_long) - ((PXSP)->spec_wl_short)))

/* Given a wavelength and the sampling ranges, compute the nearest index */
#define XSPECT_IX(SHORT, LONG, N, WL) \
((int)floor(XSPECT_DIX(SHORT, LONG, N, WL) + 0.5))

/* Given a wavelength and address of an xspect, compute the nearest index */
#define XSPECT_XIX(PXSP, WL) \
((int)floor(XSPECT_XDIX(PXSP, WL) + 0.5))

#ifndef SALONEINSTLIB

/* Single spectrum utility functions. Return NZ if error */
int write_xspect(char *fname, xspect *s);
int read_xspect(xspect *sp, char *fname);

/* CMF utility functions. Return NZ if error */
int write_cmf(char *fname, xspect cmf[3]);
int read_cmf(xspect cmf[3], char *fname);

/* Save a set of nspec spectrum to a CGATS file. Return NZ if error */
/* type 0 = SPECT, 1 = CMF */
int write_nxspect(char *fname, xspect *sp, int nspec, int type);

/* Restore a set of up to nspec spectrum from a CGATS file. Return NZ if error */
/* type  = any, 1 = SPECT, 2 = CMF, 3 = both */
int read_nxspect(xspect *sp, char *fname, int *nret, int off, int nspec, int type);

#endif /* !SALONEINSTLIB*/

/* Get interpolated value at wavelenth (not normalised) */
double value_xspect(xspect *sp, double wl);

/* De-normalize and set normalisation factor to 1.0 */
void xspect_denorm(xspect *sp);

#ifndef SALONEINSTLIB
/* Convert from one xspect type to another */
void xspect2xspect(xspect *dst, xspect *targ, xspect *src);
#endif /* !SALONEINSTLIB*/

/* ------------------------------------------------------------------------------ */
/* Class for converting between spectral and CIE */

/* We build in some useful spectra */

/* Type of illumination */
typedef enum {
    icxIT_default	 = 0,	/* Default illuminant (usually D50) */
    icxIT_none		 = 1,	/* No illuminant - self luminous spectrum */
    icxIT_custom	 = 2,	/* Custom illuminant spectrum */
    icxIT_A			 = 3,	/* Standard Illuminant A */
	icxIT_C          = 4,	/* Standard Illuminant C */
    icxIT_D50		 = 5,	/* Daylight 5000K */
    icxIT_D50M2		 = 6,	/* Daylight 5000K, UV filtered (M2) */
    icxIT_D65		 = 7,	/* Daylight 6500K */
    icxIT_E		     = 8,	/* Equal Energy */
#ifndef SALONEINSTLIB
    icxIT_F5		 = 9,	/* Fluorescent, Standard, 6350K, CRI 72 */
    icxIT_F8		 = 10,	/* Fluorescent, Broad Band 5000K, CRI 95 */
    icxIT_F10		 = 11,	/* Fluorescent Narrow Band 5000K, CRI 81 */
	icxIT_Spectrocam = 12,	/* Spectrocam Xenon Lamp */
    icxIT_Dtemp		 = 13,	/* Daylight at specified temperature */
#endif /* !SALONEINSTLIB*/
    icxIT_Ptemp		 = 14	/* Planckian at specified temperature */
} icxIllumeType;

/* Fill in an xpsect with a standard illuminant spectrum */
/* return 0 on sucecss, nz if not matched */
int standardIlluminant(
xspect *sp,					/* Xspect to fill in */
icxIllumeType ilType,		/* Type of illuminant */
double temp);				/* Optional temperature in degrees kelvin, For Dtemp and Ptemp */

/* Given an emission spectrum, set the UV output to the given level. */
/* The shape of the UV is taken from FWA1_stim, and the level is */
/* with respect to the average of the input spectrum. */
void xsp_setUV(xspect *out, xspect *in, double uvlevel);


/* Type of observer */
typedef enum {
    icxOT_default			= 0,	/* Default observer (usually CIE_1931_2) */
    icxOT_none			    = 1,	/* No observer - (don't compute XYZ) */
    icxOT_custom			= 2,	/* Custom observer type weighting */
    icxOT_CIE_1931_2		= 3,	/* Standard CIE 1931 2 degree */
    icxOT_CIE_1964_10		= 4,	/* Standard CIE 1964 10 degree */
//#ifndef SALONEINSTLIB
    icxOT_Stiles_Burch_2	= 5,	/* Stiles & Burch 1955 2 degree */
    icxOT_Judd_Voss_2		= 6,	/* Judd & Voss 1978 2 degree */
    icxOT_CIE_1964_10c		= 7,	/* Standard CIE 1964 10 degree, 2 degree compatible */
    icxOT_Shaw_Fairchild_2	= 8,		/* Shaw & Fairchild 1997 2 degree */
    icxOT_Stockman_Sharpe_2006_2	= 9		/* Current CIE based on Stockman & Sharp 2006 2 degree */
//#endif /* !SALONEINSTLIB*/
} icxObserverType;

/* Return pointers to three xpsects with a standard observer weighting curves */
/* return 0 on sucecss, nz if not matched */
int standardObserver(xspect *sp[3], icxObserverType obType);

/* Return a string describing the standard observer */
char *standardObserverDescription(icxObserverType obType);

/* Clamping state */
typedef enum {
    icxNoClamp			= 0,	/* Don't clamp XYZ/Lab to +ve */
    icxClamp			= 1,	/* Clamp XYZ/Lab to +ve */
} icxClamping;

/* The conversion object */
struct _xsp2cie {
	/* Private: */
	xspect illuminant;			/* Lookup conversion/observer illuminant */
	int isemis;					/* nz if we are doing an emission conversion */
	xspect observer[3];
	int doLab;					/* Return D50 Lab result */
	icxClamping clamp;			/* Clamp XYZ and Lab to be +ve */

#ifndef SALONEINSTLIB
	/* FWA compensation */
	double bw;		/* Integration bandwidth */
	xspect iillum;	/* Y = 1 Normalised instrument illuminant spectrum */
	xspect imedia;	/* Instrument measured media */
	xspect emits;	/* Estimated FWA emmission spectrum */
	xspect media;	/* Estimated base media (ie. minus FWA) */
	xspect tillum;	/* Y = 1 Normalised target/simulated instrument illuminant spectrum */
	xspect oillum;	/* Y = 1 Normalised observer illuminant spectrum */
	double Sm;		/* FWA Stimulation level for emits contribution */
	double FWAc;	/* FWA content (informational) */
	int    insteqtarget;	/* iillum == tillum, bypass FWA */
#endif /* !SALONEINSTLIB*/

	/* Public: */
	void (*del)(struct _xsp2cie *p);

	/* Convert (and possibly fwa correct) reflectance spectrum */
	/* Note that the input spectrum normalisation value is used. */
	/* Note that the returned XYZ is 0..1 range for reflectanc. */
	/* Emissive spectral values are assumed to be in mW/nm, and sampled */
	/* rather than integrated if they are not at 1nm spacing. */
	void (*convert) (struct _xsp2cie *p,	/* this */
	                 double *out,			/* Return XYZ or D50 Lab value */
	                 xspect *in				/* Spectrum to be converted, normalised by norm */
	                );

	/* Convert and also return (possibly corrected) reflectance spectrum */
	/* Spectrum will be same wlength range and readings as input spectrum */
	/* Note that the returned XYZ is 0..1 range for reflectanc. */
	/* Emissive spectral values are assumed to be in mW/nm, and sampled */
	/* rather than integrated if they are not at 1nm spacing. */
	void (*sconvert) (struct _xsp2cie *p,	/* this */
	                 xspect *sout,			/* Return corrected refl. spectrum (may be NULL) */
	                 double *out,			/* Return XYZ or D50 Lab value (may be NULL) */
	                 xspect *in				/* Spectrum to be converted, normalised by norm */
	                );

#ifndef SALONEINSTLIB
	/* Set Media White. This enables extracting and applying the */
	/* colorant reflectance value from/to the meadia. */
	/* return NZ if error */
	int (*set_mw) (struct _xsp2cie *p,	/* this */
	                xspect *white		/* Spectrum of plain media */
	                );

	/* Set Fluorescent Whitening Agent compensation */
	/* return NZ if error */
	int (*set_fwa) (struct _xsp2cie *p,	/* this */
					xspect *iillum,		/* Spectrum of instrument illuminant */
					xspect *tillum,		/* Spectrum of target/simulated instrument illuminant, */
										/* NULL to use observer illuminant. */
	                xspect *white		/* Spectrum of plain media */
	                );

	/* Set FWA given updated conversion illuminant. */
	/* (We assume that xsp2cie_set_fwa has been called first) */
	/* return NZ if error */
	int (*update_fwa_custillum) (struct _xsp2cie *p,
					xspect *tillum,		/* Spectrum of target/simulated instrument illuminant, */
										/* NULL to use set_fwa() value. */
	                xspect *custIllum	/* Spectrum of observer illuminant */
	                );	

	/* Get Fluorescent Whitening Agent compensation information */
	/* return NZ if error */
	void (*get_fwa_info) (struct _xsp2cie *p,	/* this */
					double *FWAc		/* FWA content as a ratio. */
	                );

	/* Extract the colorant reflectance value from the media. Takes FWA */
	/* into account if set. Media white or FWA must be set. */
	/* return NZ if error */
	int (*extract) (struct _xsp2cie *p,	/* this */
	                 xspect *out,			/* Extracted colorant refl. spectrum */
	                 xspect *in				/* Spectrum to be converted, normalised by norm */
	                );


	/* Apply the colorant reflectance value from the media. Takes FWA */
	/* into account if set. DOESN'T convert to FWA target illumination! */
	/* FWA must be set. */
	int (*apply) (struct _xsp2cie *p,	/* this */
	                 xspect *out,			/* Applied refl. spectrum */
	                 xspect *in				/* Colorant reflectance to be applied */
	                );
#endif /* !SALONEINSTLIB*/

}; typedef struct _xsp2cie xsp2cie;

xsp2cie *new_xsp2cie(
	icxIllumeType ilType,			/* Observer Illuminant to use */
	xspect        *custIllum,

	icxObserverType obType,			/* Observer */
	xspect        custObserver[3],
	icColorSpaceSignature  rcs,		/* Return color space, icSigXYZData or icSigLabData */
									/* ** Must be icSigXYZData if SALONEINSTLIB ** */
	icxClamping clamp				/* NZ to clamp XYZ/Lab to be +ve */
);

#ifndef SALONEINSTLIB

/* --------------------------- */
/* Given a choice of temperature dependent illuminant (icxIT_Dtemp or icxIT_Ptemp), */
/* return the closest correlated color temperature to the XYZ. */
/* An observer type can be chosen for interpretting the spectrum of the input and */
/* the illuminant. */
/* Return -1.0 on erorr */
double icx_XYZ2ill_ct2(
double txyz[3],			/* If not NULL, return the XYZ of the locus temperature */
icxIllumeType ilType,	/* Type of illuminant, icxIT_Dtemp or icxIT_Ptemp */
icxObserverType obType,	/* Observer, CIE_1931_2 or CIE_1964_10 */
double xyz[3],			/* Input XYZ value */
int viscct				/* nz to use visual CIEDE2000, 0 to use CCT CIE 1960 UCS. */
);

/* --------------------------- */
/* Spectrum locus              */

/* Return the spectrum locus range for the given observer. */
/* return 0 on sucecss, nz if observer not known */
int icx_spectrum_locus_range(double *min_wl, double *max_wl, icxObserverType obType);

/* Return an XYZ that is on the spectrum locus for the given observer. */
/* wl is the input wavelength in the range icx_chrom_locus_range(), */
/* and return clipped result if outside this range. */
/* Return nz if observer unknown. */
int icx_spectrum_locus(double xyz[3], double in, icxObserverType obType);

/* - - - - - - - - - - - - - - */
/* Chromaticity locus support */

typedef struct _xslpoly xslpoly;

typedef enum {
    icxLT_none	     = 0,	
    icxLT_spectral	 = 1,	
    icxLT_daylight	 = 2,
    icxLT_plankian	 = 3
} icxLocusType;

/* Return a pointer to the chromaticity locus object */
/* return NULL on failure. */
xslpoly *chrom_locus_poligon(icxLocusType locus_type, icxObserverType obType, int cspace);


/* --------------------------- */
/* Density and other functions */

/* Given a reflectance or transmition spectral product, */
/* return status T CMY + V density values */
void xsp_Tdensity(double *out,			/* Return CMYV density */
                 xspect *in				/* Spectral product to be converted */
                );

/* Given a reflectance or transmission XYZ value, */
/* return approximate status T CMYV log10 density values */
void icx_XYZ2Tdens(
double *out,			/* Return aproximate CMYV log10 density */
double *in				/* Input XYZ values */
);

/* Given a reflectance or transmission XYZ value, */
/* return log10 XYZ density values */
void icx_XYZ2dens(
double *out,			/* Return log10 XYZ density */
double *in				/* Input XYZ values */
);

/* Given an XYZ value, */
/* return sRGB values */
void icx_XYZ2sRGB(
double *out,			/* Return sRGB value */
double *wp,				/* Input XYZ white point (may be NULL) */
double *in				/* Input XYZ values */
);

/* Given an XYZ value, return approximate RGB value */
/* Desaurate to white by the given amount */
void icx_XYZ2RGB_ds(
double *out,			/* Return approximate sRGB values */
double *in,				/* Input XYZ */
double desat			/* 0.0 = full saturation, 1.0 = white */
);

/* Given a wavelengthm return approximate RGB value */
/* Desaurate to white by the given amount */
void icx_wl2RGB_ds(
double *out,			/* Return approximate sRGB values */
double wl,				/* Input wavelength in nm */
double desat			/* 0.0 = full saturation, 1.0 = white */
);


/* Given an illuminant definition and an observer model, return */
/* the normalised XYZ value for that spectrum. */
/* Return 0 on sucess, 1 on error */
int icx_ill_sp2XYZ(
double xyz[3],			/* Return XYZ value with Y == 1 */
icxObserverType obType,	/* Observer */
xspect custObserver[3],	/* Optional custom observer */
icxIllumeType ilType,	/* Type of illuminant */
double temp,			/* Input temperature in degrees K */
xspect *custIllum);		/* Optional custom illuminant */


/* Given a choice of temperature dependent illuminant (icxIT_Dtemp or icxIT_Ptemp), */
/* return the closest correlated color temperature to the given spectrum or XYZ. */
/* An observer type can be chosen for interpretting the spectrum of the input and */
/* the illuminant. */
/* Return -1 on erorr */
double icx_XYZ2ill_ct(
double txyz[3],			/* If not NULL, return the XYZ of the black body temperature */
icxIllumeType ilType,	/* Type of illuminant, icxIT_Dtemp or icxIT_Ptemp */
icxObserverType obType,	/* Observer */
xspect custObserver[3],	/* Optional custom observer */
double xyz[3],			/* Input XYZ value, NULL if spectrum intead */
xspect *insp0,			/* Input spectrum value, NULL if xyz[] instead */
int viscct);			/* nz to use visual CIEDE2000, 0 to use CCT CIE 1960 UCS. */

/* Compute the CIE1995 CRI: Ra */
/* Return < 0.0 on error */
/* If invalid is not NULL, set it to nz if CRI */
/* is invalid because the sample is not white enough. */
double icx_CIE1995_CRI(
int *invalid,			/* if not NULL, set to nz if invalid */
xspect *sample			/* Illuminant sample to compute CRI of */
);


/* Return the maximum 24 hour exposure in seconds. */
/* Limit is 8 hours */
/* Returns -1 if the source sample doesn't go down to at least 350 nm */
double icx_ARPANSA_UV_exp(
xspect *sample			/* Illuminant sample to compute UV_exp of */
);

#endif /* !SALONEINSTLIB*/

#ifdef __cplusplus
	}
#endif

#endif /* XSPECTFM_H */






































