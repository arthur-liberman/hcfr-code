
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
# include "icc.h"		/* icclib ICC definitions */ 
#else /* SALONEINSTLIB */
# include "sa_conv.h"		/* fake icclib ICC definitions */ 
#endif /* SALONEINSTLIB */

#ifdef __cplusplus
	extern "C" {
#endif

/* ------------------------------------------------------------------------------ */

/* Type of measurement result */
typedef enum {						/* XYZ units,      Spectral units */
	inst_mrt_none           = 0,	/* Not set */
	inst_mrt_emission       = 1,	/* cd/m^2,         mW/(m^2.sr.nm) */
	inst_mrt_ambient        = 2,	/* Lux             mW/(m^2.nm) */
	inst_mrt_emission_flash = 3,	/* cd.s/m^2,       mW.s/(m^2.sr.nm) */
	inst_mrt_ambient_flash  = 4,	/* Lux.s           mW.s/(m^2.nm) */
	inst_mrt_reflective     = 5,	/* %,              %/nm */
	inst_mrt_transmissive   = 6,	/* %,              %/nm */
	inst_mrt_sensitivity    = 7,	/* %,              %/nm (i.e. CMF) */
	inst_mrt_frequency      = 8		/* Hz */
} inst_meas_type;

/* Return a string describing the inst_meas_type */
char *meas_type2str(inst_meas_type mt);

/* Reflective measurement conditions */
typedef enum {
	inst_mrc_none           = 0,	/* M0 - Default or N/A */
	inst_mrc_D50            = 1,	/* M1 - D50 illuminant */
	inst_mrc_D65            = 2,	/*      D65 Illuminant */
	inst_mrc_uvcut          = 3,	/* M2 - U.V. Cut */ 
	inst_mrc_pol            = 4,	/* M3 - Polarized */
	inst_mrc_custom         = 5  	/*      Custom */
} inst_meas_cond;

/* Return a string describing the inst_meas_type */
char *meas_cond2str(inst_meas_cond mc);

/* ------------------------------------------------------------------------------ */

/* Structure for conveying spectral information */

/* NOTE :- should ditch norm, and replace it by */
/* "units", ie. reflectance/transmittance 0..1, 0..100%, */
/* W/nm/m^2 or mW/nm/m^2 */

/* NOTE :- there is an assumption in the .sp file format that */
/* resolution is always >= 1nm. Realistically this could only be */
/* lifted by making the spec[] allocation dynamic, which would involve */
/* a huge number of code changes. */

#define XSPECT_MAX_BANDS 601		/* Enought for 1nm from 300 to 900 */

typedef struct {
	int    spec_n;					/* Number of spectral bands, 0 if not valid */
	double spec_wl_short;			/* First reading wavelength in nm (shortest) */
	double spec_wl_long;			/* Last reading wavelength in nm (longest) */
	double norm;					/* Normalising scale value, ie. 1, 100 etc. */
	double spec[XSPECT_MAX_BANDS];	/* Spectral value, shortest to longest */
} xspect;

/* Some helpful macro's: */

/* Set the spectrums parameters */
#define XSPECT_SET_INFO(PDST, N, SHORT, LONG, NORM) \
 (PDST)->spec_n = (N),								\
 (PDST)->spec_wl_short = (SHORT),					\
 (PDST)->spec_wl_long = (LONG),						\
 (PDST)->norm = (NORM)

/* Copy everything except the spectral values */
#define XSPECT_COPY_INFO(PDST, PSRC) 						\
 (PDST)->spec_n = (PSRC)->spec_n,							\
 (PDST)->spec_wl_short = (PSRC)->spec_wl_short,				\
 (PDST)->spec_wl_long = (PSRC)->spec_wl_long,				\
 (PDST)->norm = (PSRC)->norm

/* True if the two xspecs match */
#define XSPECT_SAME_INFO(PDST, PSRC)	 					\
 ((PDST)->spec_n == (PSRC)->spec_n							\
 && (PDST)->spec_wl_short == (PSRC)->spec_wl_short			\
 && (PDST)->spec_wl_long == (PSRC)->spec_wl_long			\
 && (PDST)->norm == (PSRC)->norm)

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

/* Given the address of an xspect, compute the wavelegth interval */
#define XSPECT_WLI(PXSP) \
((((PXSP)->spec_wl_long) - ((PXSP)->spec_wl_short))/(((PXSP)->spec_n)-1.0))

int write_xspect(char *fname, inst_meas_type mt, inst_meas_cond mc, xspect *s);
/* mt, mc may be NULL */
int read_xspect(xspect *sp, inst_meas_type *mt, inst_meas_cond *mc, char *fname);

int write_C_xspect(char *fname, xspect *s);

#ifndef SALONEINSTLIB

/* Single spectrum utility functions. Return NZ if error */
/* Two step write & read spectrum, to be able to write & read extra kewords values */

/* Prepare to write spectrum, and return cgats */
int write_xspect_1(cgats **ocgp, inst_meas_type mt, inst_meas_cond mc, xspect *s);

/* Complete writing spectrum */
int write_xspect_2(cgats *ocg, char *fname);

/* Read spectrum and return cgats as well */
int read_xspect_1(cgats **picg, xspect *sp, inst_meas_type *mt, inst_meas_cond *mc, char *fname);

/* Complete reading spectrum */
int read_xspect_2(cgats *icg);

/* Save a set of nspec spectrum to a CGATS file. Return NZ if error */
/* type 0 = SPECT, 1 = CMF */
int write_nxspect(char *fname, inst_meas_type mt, inst_meas_cond mc, xspect *sp,
                  int nspec, int type);

/* Restore a set of up to nspec spectrum from a CGATS file. Return NZ if error */
/* type  = any, 1 = SPECT, 2 = CMF, 3 = both */
int read_nxspect(xspect *sp, inst_meas_type *mt, inst_meas_cond *mc,
	             char *fname, int *nret, int off, int nspec, int type);

/* Two step write & read n spectrum, to be able to write & read extra kewords values */
/* (Alloactes cgats * on _1, free's it on _2) */
int write_nxspect_1(cgats **pocg, inst_meas_type mt, inst_meas_cond mc, xspect *sp,
                    int nspec, int type);
int write_nxspect_2(cgats *ocg, char *fname);
int read_nxspect_1(cgats **picg, xspect *sp, inst_meas_type *mt, inst_meas_cond *mc, char *fname,
                 int *nret, int off, int nspec, int type);
int read_nxspect_2(cgats *icg);

#endif /* !SALONEINSTLIB*/

/* CMF utility functions. Return NZ if error */
/* (See cmf/pcmf.h for write/read pcmf) */
int write_cmf(char *fname, xspect cmf[3]);
int read_cmf(xspect cmf[3], char *fname);


/* Get a (normalised) linearly or poly interpolated spectrum value. */
/* Return NZ if value is valid, Z and last valid value */
/* if outside the range */
int getval_xspec(xspect *sp, double *rv, double wl) ;

/* Get linear or poly interpolated value at wavelenth (not normalised) */
double value_xspect(xspect *sp, double wl);

/* Get linear interpolated value at wavelenth (not normalised) */
double value_xspect_lin(xspect *sp, double wl);

/* Get poly interpolated value at wavelenth (not normalised) */
double value_xspect_poly(xspect *sp, double wl);


/* De-normalize and set normalisation factor to 1.0 */
void xspect_denorm(xspect *sp);

/* Scale the spectral values (don't alter norm) */
void xspect_scale(xspect *sp, double scale);

#ifndef SALONEINSTLIB
/* Convert from one xspect type to another with a wl offset from source */
void xspect2xspect_wloff(xspect *dst, xspect *targ, xspect *src, double wloff);

/* Convert from one xspect type to another */
void xspect2xspect(xspect *dst, xspect *targ, xspect *src);

/* Dump a spectra to stdout */
void xspect_dump(xspect *sp);

/* Plot up to 3 spectra */
void xspect_plot_w(xspect *sp1, xspect *sp2, xspect *sp3, int wait);

/* Plot up to 3 spectra & wait for key */
void xspect_plot(xspect *sp1, xspect *sp2, xspect *sp3);

/* Plot up to 12 spectra in an array & wait for key */
void xspect_plotN(xspect *sp, int n);

/* Plot up to 12 spectra pointed to by an array & wait for key */
void xspect_plotNp(xspect *sp[MXGPHS], int n);

/* Plot up to 12 spectra pointed to by an array */
void xspect_plotNp_w(xspect *sp[MXGPHS], int n, int wait);

/* Plot up to 12 spectra pointed to by an array over a wl range */
//void xspect_plotNp_w(xspect *sp[MXGPHS], int n, double shwl, double lowl, int wait);

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
    icxIT_D55		 = 7,	/* Daylight 5500K (use specified temperature) */
    icxIT_D65		 = 8,	/* Daylight 6500K */
    icxIT_D75		 = 9,	/* Daylight 7500K (uses specified temperature) */
    icxIT_E		     = 10,	/* Equal Energy = flat = 1.0 */
#ifndef SALONEINSTLIB
    icxIT_F5		 = 11,	/* Fluorescent, Standard, 6350K, CRI 72 */
    icxIT_F8		 = 12,	/* Fluorescent, Broad Band 5000K, CRI 95 */
    icxIT_F10		 = 13,	/* Fluorescent Narrow Band 5000K, CRI 81 */
	icxIT_Spectrocam = 14,	/* Spectrocam Xenon Lamp */
#endif /* !SALONEINSTLIB*/
    icxIT_ODtemp	 = 15,	/* Old daylight at specified temperature */
    icxIT_Dtemp		 = 16,	/* CIE 15.2004 Appendix C daylight at specified temperature */
    icxIT_OPtemp     = 17,	/* Old planckian at specified temperature */
    icxIT_Ptemp		 = 18	/* CIE 15.2004 Planckian at specified temperature */
} icxIllumeType;

/* Fill in an xpsect with a standard illuminant spectrum */
/* return 0 on sucecss, nz if not matched */
int standardIlluminant(
xspect *sp,					/* Xspect to fill in */
icxIllumeType ilType,		/* Type of illuminant */
double temp);				/* Optional temperature in degrees kelvin, For Dtemp and Ptemp */

/* Return a string describing the standard illuminant */
/* (Returns static buffer for temp based) */
char *standardIlluminant_name(
icxIllumeType ilType,		/* Type of illuminant */
double temp);				/* Optional temperature in degrees kelvin, For Dtemp and Ptemp */

/* Given an emission spectrum, set the UV output to the given level. */
/* The shape of the UV is taken from FWA1_stim, and the level is */
/* with respect to the average of the input spectrum. */
void xsp_setUV(xspect *out, xspect *in, double uvlevel);

/* General temperature Planckian (black body) spectra using CIE 15:2004 (exp 1.4388e-2) */
/* Fill in the given xspect with the specified Planckian illuminant */
/* normalised so that 560nm = 100. */
/* Return nz if temperature is out of range */
int planckian_il_sp(xspect *sp, double ct);

/* General temperature Planckian (black body) spectra using CIE 15:2004 (exp 1.4388e-2) */
/* Set the xspect to 300-830nm and fill with the specified Planckian illuminant */
/* normalised so that 560nm = 100. */
/* Return nz if temperature is out of range */
static int planckian_il(xspect *sp, double ct);

/* Type of observer */
typedef enum {
    icxOT_default			= 0,	/* Default observer (usually CIE_1931_2) */
    icxOT_none			    = 1,	/* No observer - (don't compute XYZ) */
    icxOT_custom			= 2,	/* Custom observer type weighting */
    icxOT_CIE_1931_2		= 3,	/* Standard CIE 1931 2 degree */
    icxOT_CIE_1964_10		= 4,	/* Standard CIE 1964 10 degree */
    icxOT_CIE_2012_2		= 5,	/* Proposed Standard CIE 2012 2 degree */
    icxOT_CIE_2012_10		= 6,	/* Proposed Standard CIE 2012 10 degree */
#ifndef SALONEINSTLIB
    icxOT_Stiles_Burch_2	= 7,	/* Stiles & Burch 1955 2 degree */
    icxOT_Judd_Voss_2		= 8,	/* Judd & Voss 1978 2 degree */
    icxOT_CIE_1964_10c		= 9,	/* Standard CIE 1964 10 degree, 2 degree compatible */
    icxOT_Shaw_Fairchild_2	= 10,	/* Shaw & Fairchild 1997 2 degree */
    icxOT_EBU_2012	        = 11	/* EBU standard camera curves 2012 */
#endif /* !SALONEINSTLIB*/
} icxObserverType;

/* Return pointers to three xpsects with a standard observer weighting curves */
/* return 0 on sucecss, nz if not matched */
int standardObserver(xspect *sp[3], icxObserverType obType);

/* Return a string describing the standard observer */
char *standardObserverDescription(icxObserverType obType);

/* Type of density values */
typedef enum {
    icxDT_none	= 0,	/* No density */
    icxDT_ISO	= 1,	/* ISO Visual + Type 1 + Type 2 */
    icxDT_A		= 2,	/* Status A */
    icxDT_M		= 3,	/* Status M */
    icxDT_T		= 4,	/* Status T */
    icxDT_E		= 5
} icxDensityType;

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
	int doLab;					/* 1 == Return D50 Lab result, 2 == D50 Lpt */
	icxClamping clamp;			/* Clamp XYZ and Lab to be +ve */

	/* Integration range and steps - set to observer range and 1nm by default.  */
	int    spec_bw;				/* Integration bandwidth (nm) */
	double spec_wl_short;		/* Start wavelength (nm) */
	double spec_wl_long;		/* End wavelength (nm) */

#ifndef SALONEINSTLIB
	/* FWA compensation */
	double fwa_bw;	/* Integration bandwidth */
	xspect iillum;	/* Y = 1 Normalised instrument illuminant spectrum */
	xspect imedia;	/* Instrument measured media */
	xspect emits;	/* Estimated FWA emmission spectrum */
	xspect media;	/* Estimated base media (ie. minus FWA) */
	xspect tillum;	/* Y = 1 Normalised target/simulated instrument illuminant spectrum */
					/* Use oillum if tillum spec_n = 0 */
	xspect oillum;	/* Y = 1 Normalised observer illuminant spectrum */
	double Sm;		/* FWA Stimulation level for emits contribution */
	double FWAc;	/* FWA content (informational) */
	int    insteqtarget;	/* iillum == tillum, bypass FWA */
#endif /* !SALONEINSTLIB*/

	/* Public: */
	void (*del)(struct _xsp2cie *p);

	/* Override the integration wavelength range and step size */
	void (*set_int_steps)(struct _xsp2cie *p,	/* this */
	                      double bw,		/* Integration step size (nm) */
	                      double shortwl,	/* Starting nm */
	                      double longwl		/* Ending nm */
	                     );

	/* Convert spectrum from photometric to radiometric. */
	/* Note that the input spectrum normalisation value is used. */
	/* Emissive spectral values are assumed to be in mW/nm, and sampled */
	/* rather than integrated if they are not at 1nm spacing. */
	void (*photo2rad)(struct _xsp2cie *p, 	/* this */
					 double *rout,		/* Return total lumens */
					 double *pout,		/* Return total mW */
					 xspect *sout,		/* Return input spectrum converted to lm/nm */
					 xspect *in			/* Spectrum to be converted */
				  	);

#ifndef SALONEINSTLIB
	/* Convert (and possibly fwa correct) reflectance spectrum */
#else
	/* Convert reflectance spectrum */
#endif
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
	/* Note that the returned XYZ is 0..1 range for reflectance. */
	/* Emissive spectral values are assumed to be in mW/nm, and sampled */
	/* rather than integrated if they are not at 1nm spacing. */
	void (*sconvert) (struct _xsp2cie *p,	/* this */
	                 xspect *sout,			/* Return corrected refl. spectrum (may be NULL) */
	                 double *out,			/* Return XYZ or D50 Lab value (may be NULL) */
	                 xspect *in				/* Spectrum to be converted, normalised by norm */
	                );

	/* Get the XYZ of the illuminant being used to compute the CIE XYZ */
	/* value. */
	void (*get_cie_il)(struct _xsp2cie *p,	/* this */
	                   double *xyz			/* Return the XYZ */
	                   );

#ifndef SALONEINSTLIB
	/* Set Fluorescent Whitening Agent compensation */
	/* return NZ if error */
	int (*set_fwa) (struct _xsp2cie *p,	/* this */
					xspect *iillum,		/* Spectrum of instrument illuminant */
					xspect *tillum,		/* Spectrum of target/simulated instrument illuminant, */
										/* NULL to use observer illuminant. */
	                xspect *white		/* Spectrum of plain media */
	                );

	/* Set FWA given updated conversion illuminants. */
	/* (We assume that xsp2cie_set_fwa has been called first) */
	/* return NZ if error */
	int (*update_fwa_custillum) (struct _xsp2cie *p,
					xspect *tillum,		/* Spectrum of target/simulated instrument illuminant, */
										/* NULL to use set_fwa() value. */
	                xspect *custIllum	/* Spectrum of observer illuminant */
										/* NULL to use new_xsp2cie() value. */
	                );	

	/* Get Fluorescent Whitening Agent compensation information */
	/* return NZ if error */
	void (*get_fwa_info) (struct _xsp2cie *p,	/* this */
					double *FWAc		/* FWA content as a ratio. */
	                );


	/* Set Media White. This enables extracting and applying the */
	/* colorant reflectance value from/to the meadia. */
	/* return NZ if error */
	int (*set_mw) (struct _xsp2cie *p,	/* this */
	                xspect *white		/* Spectrum of plain media */
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
	double        temp,				/* Optional temp. in degrees kelvin, if ilType = Dtemp etc */
	xspect        *custIllum,		/* Custom illuminant if ilType == icxIT_custom */

	icxObserverType obType,			/* Observer */
	xspect        custObserver[3],	/* Custom observer if obType == icxOT_custom */
	icColorSpaceSignature  rcs,		/* Return color space, icSigXYZData or D50 icSigLabData */
									/* or D50 icmSigLptData */
									/* ** Must be icSigXYZData if SALONEINSTLIB ** */
	icxClamping clamp				/* NZ to clamp XYZ/Lab to be +ve */
);

/* ------------------------------------------------- */

/* Given a reflectance/transmittance spectrum, */
/* an illuminant definition and an observer model, return */
/* the XYZ value for that spectrum. */
/* Return 0 on sucess, 1 on error */
/* (One shot version of xsp2cie etc.) */
int icx_sp2XYZ(
double xyz[3],			/* Return XYZ value */
icxObserverType obType,	/* Observer */
xspect custObserver[3],	/* Optional custom observer */
icxIllumeType ilType,	/* Type of illuminant, icxIT_[O]Dtemp or icxIT_[O]Ptemp */
double ct,				/* Input temperature in degrees K */
xspect *custIllum,		/* Optional custom illuminant */
xspect *sp				/* Spectrum to be converted */
);

/* Given an illuminant definition and an observer model, return */
/* the normalised XYZ value for that spectrum. */
/* Return 0 on sucess, 1 on error */
/* (One shot version of xsp2cie etc.) */
int icx_ill_sp2XYZ(
double xyz[3],			/* Return XYZ value with Y == 1 */
icxObserverType obType,	/* Observer */
xspect custObserver[3],	/* Optional custom observer */
icxIllumeType ilType,	/* Type of illuminant */
double temp,			/* Input temperature in degrees K */
xspect *custIllum,		/* Optional custom illuminant */
int abs					/* If nz return absolute value in cd/m^2 or Lux */
						/* else return Y = 1 normalised value */
);

#ifndef SALONEINSTLIB

/* --------------------------- */
/* Given a choice of temperature dependent illuminant (icxIT_[O]Dtemp or icxIT_[O]Ptemp), */
/* return the closest correlated color temperature to the XYZ. */
/* An observer type can be chosen for interpretting the spectrum of the input and */
/* the illuminant. */
/* Return -1.0 on erorr */
/* (Faster, slightly less accurate version of icx_XYZ2ill_ct()) */
double icx_XYZ2ill_ct2(
double txyz[3],			/* If not NULL, return the XYZ of the locus temperature */
icxIllumeType ilType,	/* Type of illuminant, icxIT_[O]Dtemp or icxIT_[O]Ptemp */
icxObserverType obType,	/* Observer, CIE_1931_2 or CIE_1964_10 */
double xyz[3],			/* Input XYZ value */
int viscct				/* nz to use visual CIEDE2000, 0 to use CCT CIE 1960 UCS. */
);

/* Given a choice of temperature dependent illuminant (icxIT_[O]Dtemp or icxIT_[O]Ptemp), */
/* a color temperature and a Y value, return the corresponding XYZ */
/* An observer type can be chosen for interpretting the spectrum of the input and */
/* the illuminant. */
/* Return xyz[0] = -1.0 on erorr */
/* (Faster, slightly less accrurate alternative to standardIlluminant()) */
void icx_ill_ct2XYZ(
double xyz[3],			/* Return the XYZ value */
icxIllumeType ilType,	/* Type of illuminant, icxIT_[O]Dtemp or icx[O]IT_Ptemp */
icxObserverType obType,	/* Observer, CIE_1931_2 or CIE_1964_10 */
int viscct,				/* nz to use visual CIEDE2000, 0 to use CCT CIE 1960 UCS. */
double tin,				/* Input temperature */
double Yin				/* Input Y value */
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

/* Return a pointer to the (static) chromaticity locus object */
/* return NULL on failure. */
xslpoly *chrom_locus_poligon(icxLocusType locus_type, icxObserverType obType, int cspace);


/* Determine whether the given XYZ is outside the chromaticity locus */
/* Return 0 if within locus */
/* Return 1 if outside locus */
int icx_outside_spec_locus(xslpoly *p, double xyz[3]);

/* ------------------------------------------------- */
/* Color temperature and CRI */

/* Given a choice of temperature dependent illuminant (icxIT_[O]Dtemp or icxIT_[O]Ptemp), */
/* return the closest correlated color temperature to the given spectrum or XYZ. */
/* An observer type can be chosen for interpretting the spectrum of the input and */
/* the illuminant. */
/* Return -1 on erorr */
double icx_XYZ2ill_ct(
double txyz[3],			/* If not NULL, return the XYZ of the black body temperature */
icxIllumeType ilType,	/* Type of illuminant, icxIT_[O]Dtemp or icxIT_[O]Ptemp */
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
double cris[14],		/* If not NULL, return the TCS01-14 CRI's */
xspect *sample			/* Illuminant sample to compute CRI of */
);

/* Compute the EBU TLCI-2012 Qa */
/* Return < 0.0 on error */
/* If invalid is not NULL, set it to nz if TLCI */
/* is invalid because the sample is not white enough. */
double icx_EBU2012_TLCI(
int *invalid,			/* if not NULL, set to nz if invalid */
xspect *sample			/* Illuminant sample to compute TLCI of */
);

#include "tm3015.h"		/* IES TM-30-15 */

/* Return the maximum 24 hour exposure in seconds. */
/* Limit is 8 hours */
/* Returns -1 if the source sample doesn't go down to at least 350 nm */
double icx_ARPANSA_UV_exp(
xspect *sample			/* Illuminant sample to compute UV_exp of */
);

/* Return a polinomial aproximation of CCT */
double aprox_CCT(double xyz[3]);

/* Aproximate x,y from CCT using Kim et al's cubic spline. */
/* Invalid < 1667 and > 25000 */
/* (Doesn't set Yxy[0]) */
void aprox_plankian(double Yxy[3], double ct);

/* --------------------------- */
/* Density and other functions */

/* Given a reflectance or transmition spectral product, (Relative */
/* to the scale factor), return CMYV log10 density values */
void xsp_density(
double out[4],				/* Return CMYV density */
	xspect *in,				/* Spectral product to be converted */
	icxDensityType dt		/* Density type */
	);

/* Return a string describing the type of density */
char *xsp_density_desc(icxDensityType dt);

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
double *wp,				/* Input XYZ white point (D65 used if NULL) */
double *in				/* Input XYZ values */
);

/* Given an RGB value, return XYZ values. */
/* This is a little slow */
void icx_sRGB2XYZ(
double *out,			/* Return XYZ values */
double *wp,				/* Output XYZ white point (D65 used if NULL) */
double *in				/* Input sRGB values */
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

#endif /* !SALONEINSTLIB*/

#ifdef __cplusplus
	}
#endif

#endif /* XSPECTFM_H */






































