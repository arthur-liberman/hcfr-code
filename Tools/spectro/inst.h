
#ifndef INST_H

/*
 * instlib API definition.
 *
 * See spotread.c, chartread.c, illumread.c & ccxxmake.c for examples of
 * the API usage.
 *
 * Abstract base class for common color instrument interface
 * and other common instrument stuff.
 */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   15/3/2001
 *
 * Copyright 2001 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 */

/* 
   If you make use of the instrument driver code here, please note
   that it is the author(s) of the code who take responsibility
   for its operation. Any problems or queries regarding driving
   instruments with the Argyll drivers, should be directed to
   the Argyll's author(s), and not to any other party.

   If there is some instrument feature or function that you
   would like supported here, it is recommended that you
   contact Argyll's author(s) first, rather than attempt to
   modify the software yourself, if you don't have firm knowledge
   of the instrument communicate protocols. There is a chance
   that an instrument could be damaged by an incautious command
   sequence, and the instrument companies generally cannot and
   will not support developers that they have not qualified
   and agreed to support.
 */

#include "insttypes.h"		/* libinst Includes this functionality */
#include "dev.h"			/* Base device class */
#include "disptechs.h"		/* libinst Includes this functionality */
#include "icoms.h"			/* libinst Includes this functionality */
#include "conv.h"

#ifdef __cplusplus
	extern "C" {
#endif

/* ------------------------------------------------- */
/* aprox. debug level guide:

	1,2  Applications, internal errors
	2,3  High level instrument drivers
	4,5  High level instrument communications
	6,7  High level serial/USB communications
	8,9  Low level serial/USB communications

*/

/* ------------------------------------------------- */
/* Structure for holding an instrument patch reading */

#ifdef NEVER	/* Declared in xicc/xspect.h */

// ~~~ should add absorbance mode -
//     i.e. modified transmissive mode where units are
//     log10(incident/transmitted) = Beer-Lambert Law

/* Note that flash modes subtract background light */
/* It's not clear if that is the best thing to do in terms */
/* of either exposure or color temperature. */

/* Type of measurement result */
typedef enum {						/* XYZ units,      Spectral units */
	inst_mrt_none           = 0,	/* Not set */
	inst_mrt_emission       = 1,	/* cd/m^2,         mW/(m^2.sr.nm) */
	inst_mrt_ambient        = 2,	/* Lux             mW/(m^2.nm) */
	inst_mrt_emission_flash = 3,	/* cd.s/m^2,       mW.s/(m^2.sr.nm) */
	inst_mrt_ambient_flash  = 4,	/* Lux.s           mW.s/(m^2.nm) */
	inst_mrt_reflective     = 5,	/* %,              %/nm */
	inst_mrt_transmissive   = 6,	/* %,              %/nm */
	inst_mrt_frequency      = 7		/* Hz */
} inst_meas_type;

#endif // NEVER

#define ICOM_MAX_LOC_LEN 10

struct _ipatch {
	char loc[ICOM_MAX_LOC_LEN];	/* patch location */

	inst_meas_type mtype;	/* Measurement type */
	
	int XYZ_v;				/* XYZ valid */
	double XYZ[3];			/* XYZ values */

	xspect sp;			/* Spectrum. sp.spec_n > 0 if valid */
						/* Reflectance/Transmittance 0.0 .. 100.0%, norm = 100.0 */
						/* or mW/nm/m^2, norm = 1.0, etc.  */

	double duration;	/* Apparent total duration in seconds (flash measurement) */
						/* Typicall limited to sampling rate of instrument. */

}; typedef struct _ipatch ipatch;

/* ---------------------------------------- */
/* Instrument interface abstract base class */

/* Abstract return codes in ms 8bits. */
/* Instrument dependant codes in ls 16 bits. */
/* Note :- update inst_interp_error() in inst.c if anything here is changed. */
/* and also check all the instrument specific XXX_interp_code() routines too. */
typedef enum {
	inst_ok                = 0x000000,
	inst_notify            = 0x010000,	/* A Notification */
	inst_warning           = 0x020000,	/* A Warning */
	inst_no_coms           = 0x030000,	/* init_coms() hasn't been called yet */
	inst_no_init           = 0x040000,	/* init_inst() hasn't been called yet */
	inst_unsupported       = 0x050000,	/* Unsupported function */
	inst_internal_error    = 0x060000,	/* Internal software error */
	inst_coms_fail         = 0x070000,	/* Communication failure */
	inst_unknown_model     = 0x080000,	/* Not the expected instrument */
	inst_protocol_error    = 0x090000, 	/* Read or Write protocol error */
	inst_user_abort        = 0x0A0000,	/* User hit escape */
	inst_user_trig         = 0x0C0000,	/* User hit trigger key */
	inst_misread           = 0x0E0000,	/* Bad reading, or strip misread */
	inst_nonesaved         = 0x0F0000,	/* No saved data to read */
	inst_nochmatch         = 0x100000,	/* Chart doesn't match */
	inst_needs_cal         = 0x110000,	/* Instrument needs calibration, and read retried */
	inst_cal_setup         = 0x120000,	/* Calibration retry with correct setup is needed */
	inst_wrong_config      = 0x130000,	/* Retry with correct inst. config./sensor posn. needed */
	inst_unexpected_reply  = 0x140000,	/* Unexpected Reply */
	inst_wrong_setup       = 0x150000,	/* Setup is wrong or conflicting */
	inst_hardware_fail     = 0x160000,	/* Hardware failure */
	inst_system_error      = 0x170000,	/* System call (ie malloc) fail */
	inst_bad_parameter     = 0x180000,	/* Bad parameter value */
	inst_other_error       = 0x190000,	/* Some other error */
	inst_mask              = 0xff0000,	/* inst_code mask value */
	inst_imask             = 0x00ffff	/* instrument specific mask value */
} inst_code;


/*
	possible UV modes:

	Do the reflective measurement with UV rather than normal illuminant
	[ Should this be reflectivity against 'A', or absolute ?? ]
	(ie. spot, strip, xy or chart). inst_mode_ref_uv

	Do a white & UV measurement at the start of each strip reading.
	Return result with special call after each strip read.
	                                inst_mode_ref_uv_strip_1

	Do a dual white & UV measurement
	[ Can do in one hit for spot, but how should two strip passes be handled ?
	  ie. two separate strip reads of phase 1 & then 2 ? ]
	(ie. spot, strip, xy or chart). inst_mode_ref_uv_2pass 

	Get normal illuminant spectrum.
	
	Get UV spectrum.

	                                get_meas_illum_spectrum(mode);
 */

/* Instrument capabilities & modes */
/* Note that due to the binary combinations, capabilities is not definititive */
/* as to valid modes. check_mode() is definitive. */
/* #defines are for saving modes in a version independent way. */
/* Note :- update inst_mode_sym[] table in inst.c if anything here is changed. */
typedef enum {
	inst_mode_none               = 0x00000000, /* No capability or mode */

	/* Light measurement mode */
	inst_mode_reflection         = 0x00000001,	/* General reflection mode */
#   define inst_mode_reflection_sym             "REFL"
	inst_mode_s_reflection       = 0x00000002,	/* General saved reflection mode */
#   define inst_mode_s_reflection_sym           "SRFL"
	inst_mode_transmission       = 0x00000004,	/* General transmission mode */
#   define inst_mode_transmission_sym           "TRAN"
	inst_mode_emission           = 0x00000008,	/* General emission mode */
#   define inst_mode_emission_sym               "EMIS"
	inst_mode_illum_mask         = 0x0000000f,	/* Mask of sample illumination sub mode */

	/* Action of measurement */
	inst_mode_spot               = 0x00000010,	/* General spot measurement mode */
#   define inst_mode_spot_sym                   "SPOT"
	inst_mode_strip              = 0x00000020,	/* General strip measurement mode */
#   define inst_mode_strip_sym                  "STRP"
	inst_mode_xy                 = 0x00000040,	/* General X-Y measurement mode */
#   define inst_mode_xy_sym                     "CHXY"
	inst_mode_chart              = 0x00000080,	/* General chart measurement mode */
#   define inst_mode_chart_sym                  "CHRT"
	inst_mode_ambient            = 0x00000100,	/* General ambient measurement mode */
#   define inst_mode_ambient_sym                "AMBI"
	inst_mode_ambient_flash      = 0x00000200,	/* General ambient flash measurement mode */
#   define inst_mode_ambient_flash_sym          "ABFL"
	inst_mode_tele               = 0x00000400,	/* General telephoto measurement mode */
#   define inst_mode_tele_sym                   "TELE"
	// Hmm. Should there be a tele_flash mode ????
	inst_mode_sub_mask           = 0x000007f0,	/* Mask of sub-mode */

	/* Basic mode */
	inst_mode_basic_mask         = inst_mode_illum_mask | inst_mode_sub_mask,

	/* Extra dependent modes */
	inst_mode_emis_nonadaptive   = 0x00000800,	/* Emissom Non-adaptive mode */
#   define inst_mode_emis_nonadaptive_sys       "EMNA"
	inst_mode_ref_uv             = 0x00001000,	/* Ultra Violet measurement mode */
#   define inst_mode_ref_uv_sym                 "REUV"
	inst_mode_emis_refresh_ovd   = 0x00002000,	/* Emissom Refresh mode override */
#   define inst_mode_emis_refresh_ovd_sym       "EMRO"
	inst_mode_emis_norefresh_ovd = 0x00006000,	/* Emissom Non-refresh mode override */
#   define inst_mode_emis_norefresh_ovd_sym     "ENRO"
	inst_mode_dep_extra_mask     = 0x00007800,	/* Mask of measurement modifiers */

	/* Extra independent modes */
	inst_mode_colorimeter        = 0x00004000,	/* Colorimetric mode */
#   define inst_mode_colorimeter_sym            "COLI"
	inst_mode_spectral           = 0x00008000,	/* Spectral mode */
#   define inst_mode_spectral_sym               "SPEC"
	inst_mode_highres            = 0x00010000,	/* High Resolution Spectral mode */
#   define inst_mode_highres_sym                "HIRZ"
	inst_mode_extra_mask         = 0x0001c000,	/* Mask of extra modes */

	/* Configured for calibration & capable of returning it from inst_mode_calibration */
	inst_mode_calibration        = 0x80000000,	/* Configured for calibration */
#   define inst_mode_calibration_sym            "CALB"

	/* Combined operating modes (from above): */
	/* These mode capabilities are also use to set the mode */
	/* Test for a mode should be IMODETST(flags, mode) */
	inst_mode_ref_spot           = inst_mode_spot	/* Reflection spot measurement mode */
	                             | inst_mode_reflection,
	inst_mode_ref_strip          = inst_mode_strip	/* Reflection strip measurement mode */
	                             | inst_mode_reflection,
	inst_mode_ref_xy             = inst_mode_xy		/* Reflection X-Y measurement mode */
	                             | inst_mode_reflection,
	inst_mode_ref_chart          = inst_mode_chart	/* Reflection Chart measurement mode */
	                             | inst_mode_reflection,

	inst_mode_s_ref_spot         = inst_mode_spot	/* Saved reflection spot measurement mode */
	                             | inst_mode_s_reflection,
	inst_mode_s_ref_strip        = inst_mode_strip	/* Saved reflection strip measurement mode */
	                             | inst_mode_s_reflection,
	inst_mode_s_ref_xy           = inst_mode_xy		/* Saved reflection X-Y measurement mode */
	                             | inst_mode_s_reflection,
	inst_mode_s_ref_chart        = inst_mode_chart	/* Saved reflection Chart measurement mode */
	                             | inst_mode_s_reflection,

	inst_mode_trans_spot         = inst_mode_spot	/* Transmission spot measurement mode */
	                             | inst_mode_transmission, 	/* Normal Diffuse/90 */
	inst_mode_trans_spot_a       = inst_mode_ambient /* Transmission spot measurement mode */
	                             | inst_mode_transmission, /* Alternate 90/diffuse */
	inst_mode_trans_strip        = inst_mode_strip	/* Transmission strip measurement mode */
	                             | inst_mode_transmission, 
	inst_mode_trans_xy           = inst_mode_xy		/* Transmission X-Y measurement mode */
	                             | inst_mode_transmission, 
	inst_mode_trans_chart        = inst_mode_chart	/* Transmission chart measurement mode */
	                             | inst_mode_transmission, 

	inst_mode_emis_spot          = inst_mode_spot	/* Spot emission measurement mode */
	                             | inst_mode_emission,
	inst_mode_emis_tele          = inst_mode_tele	/* Telephoto emission measurement mode */
	                             | inst_mode_emission,
	inst_mode_emis_ambient       = inst_mode_ambient	/* Ambient emission measurement mode */
	                             | inst_mode_emission,
	inst_mode_emis_ambient_flash = inst_mode_ambient_flash	/* Ambient emission flash measurement */
	                             | inst_mode_emission,

	inst_mode_emis_strip         = inst_mode_strip	/* Strip emission measurement mode */
	                             | inst_mode_emission,

	inst_mode_measurement_mask   = inst_mode_illum_mask	/* Mask of exclusive measurement modes */
	                             | inst_mode_sub_mask
	                             | inst_mode_dep_extra_mask
} inst_mode;

/* Test for a specific mode */
/* (This isn't foolproof - it->check_mode() is better for modes */
/*  composed of more than one bit) */
#define IMODETST(mbits, mode) (((mbits) & (mode)) == (mode))

/* Test for a specific mode in capability and mode */
#define IMODETST2(mcap, mbits, mode) (IMODETST(mcap, mode) && IMODETST(mbits, mode))

#define MAX_INST_MODE_SYM_SZ (32 * (4 + 1))		/* Each bit sym is 4 chars */

/* Return a string with a symbolic encoding of the mode flags */
void inst_mode_to_sym(char sym[MAX_INST_MODE_SYM_SZ], inst_mode mode);

/* Return a set of mode flags that correspond to the symbolic encoding */
/* Return nz if a symbol wasn't recognized */
int sym_to_inst_mode(inst_mode *mode, const char *sym);


/* Instrument capabilities 2 */
/* (Available capabilities may be mode dependent) */
typedef enum {
	inst2_none              = 0x00000000, /* No capabilities */

	inst2_xy_holdrel        = 0x00000001, /* Needs paper hold/release between each sheet */
	inst2_xy_locate         = 0x00000002, /* Needs user to locate patch locations */
	inst2_xy_position       = 0x00000004, /* Can be positioned at a given location */

	inst2_meas_disp_update  = 0x00000010, /* Is able to measure display update delay */

	inst2_get_refresh_rate  = 0x00000020, /* Is able to get the calibrated refresh rate */
	inst2_set_refresh_rate  = 0x00000040, /* Is able to set the calibrated refresh rate */

	inst2_emis_refr_meas    = 0x00000080, /* Has an emissive refresh rate measurement func. */

	inst2_prog_trig         = 0x00000100, /* Progromatic trigger measure capability */
	inst2_user_trig         = 0x00000200, /* User trigger measure capability, */
										  /* i.e. via user button and uicallback. */
	inst2_switch_trig       = 0x00000400, /* Inst. switch trigger measure capability, */
										  /* i.e. instrument directly starts measurement */
										  /* via mechanical switch. */
	inst2_user_switch_trig  = 0x00000800, /* User or switch trigger measure capability */

	inst2_bidi_scan         = 0x00001000, /* Try and recognise patches scanned from either dir. */
	inst2_cal_using_switch  = 0x00002000, /* DTP22 special - use switch triggered calibration */
	inst2_has_scan_toll     = 0x00004000, /* Instrument will honour modified scan tollerance */
	inst2_no_feedback       = 0x00008000, /* Instrument doesn't give any user feedback */

	inst2_opt_calibs        = 0x00010000, /* Instrument has optional calibrations */

	inst2_has_leds          = 0x00200000, /* Instrument has some user viewable indicator LEDs */
	inst2_has_target        = 0x00400000, /* Instrument has aiming target */
	inst2_has_sensmode      = 0x00800000, /* Instrument can report it's sensors mode */

	inst2_has_battery       = 0x01000000, /* Instrument is battery powered */

	inst2_disptype          = 0x02000000, /* Has a display/calibration type selector */
										  /* (ie. get_disptypesel(), set_disptype */

	inst2_ccmx              = 0x04000000, /* Colorimeter Correction Matrix capability */
	inst2_ccss              = 0x08000000, /* Colorimeter Cal. Spectral Set capability */

	inst2_ambient_mono      = 0x10000000, /* The ambient measurement is monochrome */

	inst2_ambient_cardioid  = 0x20000000, /* The ambient measurement has a cardioid response */

	inst2_get_min_int_time  = 0x40000000, /* Is able to get the minimum integration time */
	inst2_set_min_int_time  = 0x80000000  /* Is able to set the minimum integration time */

} inst2_capability;

/* Instrument capabilities 3 (room for expansion) */
/* (Available capabilities may be mode dependent) */
typedef enum {
	inst3_none              = 0x00000000, /* No capabilities */

	inst3_average           = 0x00000001  /* Can set to average multiple measurements into 1 */
										  /* See inst_opt_set_averages */

} inst3_capability;

/* - - - - - - - - - - - - - - - - - - - */

/* API ideas for supporting calibration management:

	functions to:

	 return number of used and free builtin slots, + password required flag.
	
	 write builtin calibration or ccss/ccmx file

	 write builtin calibration or ccss/ccmx file

	 delete builtin calibration or ccss/ccmx file

	Things to be allowed for:

		optional password
		configuration specific calibrations. How are they marked ?


	Also for ccss capable instruments:

	 write corresponding ccmx for given ccss.
*/

typedef enum {
	inst_dtflags_none    = 0x0000,			/* no flags - assume builtin calibration */
	inst_dtflags_mtx     = 0x0001,			/* matrix read from instrument */
	inst_dtflags_ccss    = 0x0002,			/* ccss file */
	inst_dtflags_ccmx    = 0x0004,			/* ccmx file */
	inst_dtflags_custom  = 0x0010,			/* custom (i.e. not built in or OEM) */
	inst_dtflags_wr      = 0x0020,			/* Writable slot */
	inst_dtflags_ld      = 0x0040,			/* mtx/ccss/ccmx is loaded */
	inst_dtflags_default = 0x1000,			/* Default calibration to use */
	inst_dtflags_end     = 0x8000			/* end marker */
} inst_dtflags;

#define INST_DTYPE_SEL_LEN 10
#define INST_DTYPE_DESC_LEN 100

/* Structure used to return display type selection information. */
/* Calibrations may be implicit (based on ix and driver code), or */
/* explicit mtx/ccss/ccmx information contained in structure. */
typedef struct _inst_disptypesel {

  /* Public: */
	inst_dtflags flags;		/* Attribute flags */
	int cbid;				/* Calibration base ID. NZ if valid ccmx calibration base. */
							/* Should remain constant between releases */
	char sel[INST_DTYPE_SEL_LEN];	/* String of selector character aliases */
	char desc[INST_DTYPE_DESC_LEN];	/* Textural description */
	int  refr;				/* Refresh mode flag */

	disptech dtech;			/* display techology */

  /* Private: */
	int ix;					/* Internal index,  */
	char isel[INST_DTYPE_SEL_LEN];	/* String of potential selector characters */

	// Stuff for ccss & ccmx
	char *path;				/* Path to ccss or ccmx. NULL if not valid */
	int cc_cbid;			/* cbid that matrix requires */
	double mat[3][3];		/* ccmx or instrument matrix */
	xspect *sets;   		/* ccss set of sample spectra. NULL if not valid */
	int no_sets;    		/* ccs number of sets. 0 if not valid */

} inst_disptypesel;

/* - - - - - - - - - - - - - - - - - - - */

/* Instrument options for get_set_opt() */
typedef enum {
	inst_opt_unknown            = 0x0000,	/* Option not specified */

	inst_stat_saved_readings    = 0x0001,	/* Return status of saved reading values */ 
											/* [1 argument type *inst_stat_savdrd] */
	inst_stat_s_spot            = 0x0002,	/* Return number of saved spot readings */ 
											/* [1 argument type *int] */
	inst_stat_s_strip           = 0x0003,	/* Return saved strip details */
											/* [args: int *no_patches, int *no_rows */
	                                        /*        int *pat_per_row] */
	inst_stat_s_xy              = 0x0004,	/* Return saved strip details */
											/* [args: int *no_sheets, int *no_patches, */
	                                        /*        int *no_rows, int *pat_per_row] */
	inst_stat_s_chart           = 0x0005,	/* Return number saved chart details */
											/* [args: int *no_patches, int *no_rows    */
	                                        /*        int *pat_per_row, int *chart_id, */
											/*        int *missing_row ]               */

	inst_stat_battery           = 0x0006,	/* Return charged status of battery */
											/* [1 argument type *double : range 0.0 - 1.0 ] */

	inst_stat_get_filter        = 0x0007,	/* Get a instrument filter configuration */
											/* [1 argument type *inst_opt_filter ] */

	inst_stat_get_custom_filter = 0x0008,	/* Get a custom filter configuration. */
											/* Will return NULL if none. */
											/* [1 argument type xspect *] */

	inst_opt_initcalib          = 0x0009,	/* Enable initial calibration (default) [No args] */
	inst_opt_noinitcalib        = 0x000A,	/* Disable initial calibration if < losecs since last */											/* opened, or losecs == 0 [int losecs] */

	inst_opt_askcalib           = 0x000B,	/* Ask before proceeding with calibration (default) */
											/* [ No args] */
	inst_opt_noaskcalib         = 0x000C,	/* Proceed with calibration immediately if possible */
											/* [ No args] */

	inst_opt_set_ccss_obs       = 0x000D,	/* Set the observer used with ccss device types - */
											/* Not applicable to any other type of instrument. */
											/* [args: icxObserverType obType,*/
	                                        /*        xspect custObserver[3] */

	inst_opt_set_filter         = 0x000E,	/* Set an instrument filter configuration */
											/* Set this before calling init_coms() */
											/* [1 argument type inst_opt_filter] */

	inst_opt_set_custom_filter  = 0x000F,	/* Set a custom filter configuration. */
											/* Emmissive mmnts. are divided by filter values. */
											/* Can be set any time. */
											/* Independent of inst_opt_set_filter. */
											/* Set to NULL to dispable. */
											/* [1 argument type xspect *] */

	inst_opt_trig_prog          = 0x0010,	/* Trigger progromatically [No args] */
	inst_opt_trig_user          = 0x0011,	/* Trigger from user via uicallback [No args] */
	inst_opt_trig_switch        = 0x0012,	/* Trigger directly using instrument switch [No args] */
	inst_opt_trig_user_switch   = 0x0013,	/* Trigger from user via uicallback or switch (def) [No args] */

	inst_opt_highres            = 0x0014,	/* Enable high res spectral mode indep. of set_mode() */
	inst_opt_stdres             = 0x0015,	/* Revert to standard resolution spectral mode */

	inst_opt_scan_toll          = 0x0016,	/* Modify the patch scan recognition tollnce [double] */

	inst_opt_get_gen_ledmask    = 0x0017,	/* Get the bitmask for general indication LEDs [*int] */
											/* (More specialized indicator masks go here) */
	inst_opt_set_led_state      = 0x0018,	/* Set the current LED state. 0 = off, 1 == on [int] */
	inst_opt_get_led_state      = 0x0019,	/* Get the current LED state. 0 = off, 1 == on [*int] */
	inst_opt_get_pulse_ledmask  = 0x001A,	/* Get the bitmask for pulseable ind. LEDs [*int] */
	inst_opt_set_led_pulse_state= 0x001B,	/* Set the current LED state. [double period_in_secs, */
	                                        /* double on_time_prop, double trans_time_prop] */
	inst_opt_get_led_pulse_state= 0x001C,	/* Get the current pulse LED state. [*double period] */
	inst_opt_get_target_state   = 0x001D,	/* Get the aiming target state 0 = off, 1 == on [*int] */
	inst_opt_set_target_state   = 0x001E,	/* Set the aiming target state 0 = off, 1 == on, 2 = toggle [int] */

	inst_opt_get_min_int_time   = 0x001F,	/* Get the minimum integration time [*double time] */
	inst_opt_set_min_int_time   = 0x0020,	/* Set the minimum integration time [double time] */

	inst_opt_opt_calibs_valid   = 0x0021,	/* Are optional (white/black/gloss etc.) calibrations */
	                                        /* valid?                     [*int valid] */
	inst_opt_clear_opt_calibs   = 0x0022,	/* Clear all optional calibrations. */

	inst_opt_get_cal_tile_sp    = 0x0023,	/* Return refl. white tile reference spectrum. */
	                                        /* for current instrument filter. [*xspect tile] */

	inst_opt_set_xcalstd        = 0x0024,	/* Set the X-Rite reflective calibration standard */
											/*                             [xcalstd standard] */
	inst_opt_get_xcalstd        = 0x0025,	/* Get the effective X-Rite ref. cal. standard */
											/*                             [xcalstd *standard] */
	inst_opt_lamp_remediate     = 0x0026,	/* Remediate i1Pro lamp           [double seconds] */

	inst_opt_set_averages       = 0x0027	/* Set the number of measurements to average [int] */
											/* 0 for default */


} inst_opt_type;

/* Optional manufacturers instrument filter fitted to instrument (for inst_opt_set_filter) */
/* Set this before calling init_coms() */
typedef enum {
	inst_opt_filter_unknown  = 0xffff,	/* Unspecified filter */
	inst_opt_filter_none     = 0x0000,	/* No filters fitted */
	inst_opt_filter_pol      = 0x0001,	/* Polarising filter */ 
	inst_opt_filter_D65      = 0x0002,	/* D65 Illuminant filter */
	inst_opt_filter_UVCut    = 0x0004,	/* U.V. Cut filter */
	inst_opt_filter_Custom   = 0x0008	/* Manufacturers custom Filter */
} inst_opt_filter;

/* Off-line pending readings available (status) */
typedef enum {
	inst_stat_savdrd_none    = 0x00,		/* No saved readings */
	inst_stat_savdrd_spot    = 0x01,		/* There are saved spot readings available */
	inst_stat_savdrd_strip   = 0x02,		/* There are saved strip readings available */
	inst_stat_savdrd_xy      = 0x04,		/* There are saved page readings available */
	inst_stat_savdrd_chart   = 0x08			/* There are saved chart readings available */
} inst_stat_savdrd;

/* Type of calibration needed/available/requested - corresponds to capabilities */
/* [ inst_calt_trans_vwhite is "variable" white transmission calibration, needed */
/*   where transmission mode is being emulated. ] */
/* Remember to update calt2str() */ 
typedef enum {
	/* Response to needs_calibration() */
	inst_calt_none           = 0x00000000, 	/* No calibration or unknown */

	/* Psudo-calibration types */
	inst_calt_all            = 0x00000001, 	/* Do required non-deferable cals for mode, but also */
										    /* do all possible calibrations for all other modes */
										    /* using the calibration conditions. This may be slow */
	/* Hmm. We don't have an "calt_all_needed" - do all needed cals of all possible modes. */
	/* This might be more useful than inst_calt_all ? */
	inst_calt_needed         = 0x00000002, 	/* Do all required non-deferable cals for c.m. */
	inst_calt_available      = 0x00000003, 	/* Do all available non-deferable cals for c.m. */

	/* Specific type of calibration - corresponds to capabilities  */
	inst_calt_wavelength     = 0x00000010, 	/* Wavelength calibration using refl. cal. surface */ 
	inst_calt_ref_white      = 0x00000020, 	/* Reflective white/emissive dark calibration */
	inst_calt_ref_dark       = 0x00000040, 	/* Reflective dark calibration (light trap) */
	inst_calt_ref_dark_gl    = 0x00000080, 	/* Reflective gloss calibration (black gloss surface) */
	inst_calt_emis_offset    = 0x00000100, 	/* Emissive offset/black calibration (dark surface) */
	inst_calt_emis_ratio     = 0x00000200, 	/* Emissive ratio calibration */
	inst_calt_em_dark        = 0x00000400, 	/* Emissive dark calibration (in dark) */
	inst_calt_trans_white    = 0x00000800,	/* Transmissive white reference calibration */
	inst_calt_trans_vwhite   = 0x00001000,	/* Transmissive variable white reference calibration */
	inst_calt_trans_dark     = 0x00002000,	/* Transmissive dark reference calibration */

	inst_calt_n_dfrble_mask  = 0x0000fff0,	/* Mask of non-deferrable calibrations */

	/* Calibrations that might be deferred until measurement */
	inst_calt_emis_int_time  = 0x00100000, 	/* Emissive measurement range (integration time) */
	inst_calt_ref_freq       = 0x00200000, 	/* Display refresh frequency calibration */

	inst_calt_dfrble_mask    = 0x00f00000, 	/* Mask of deferrable calibrations */

	inst_calt_all_mask       = 0x00f0fff0, 	/* Mask of all specific calibrations */

	inst_calt_ap_flag        = 0x80000000	/* Implementation flag indicating do all possible */

} inst_cal_type;

/* Return a description of the first calibration type */ 
char *calt2str(inst_cal_type calt);

/* Calibration conditions. */
/* This is how the instrument communicates to the calling program */
/* about how to facilitate a calibration, or what it's current measurement */
/* configuration provides. */
/* [There is no provission for explictly indicating calibrations that can be */
/* performed automatically and transparently by the instrument - for instance */
/* in the case of the spectroscan, since the required condition can be obtained */
/* without the users interaction. ] */
typedef enum {
	inst_calc_none             = 0x00000000, /* Not suitable for calibration */
	inst_calc_unknown          = 0xffffffff, /* Unknown calibration setup */

							/* uop means that user has to trigger the within instrument */
							/* calibration using its "front panel" or other direct keys */
	inst_calc_uop_ref_white    = 0x00000001, /* user operated reflective white calibration */
	inst_calc_uop_trans_white  = 0x00000002, /* user operated tranmissive white calibration */
	inst_calc_uop_trans_dark   = 0x00000003, /* user operated tranmissive dark calibration */
	inst_calc_uop_mask         = 0x0000000F, /* user operated calibration mask */

							/* Man means that the user has to manualy configure the instrument */
							/* to be on the correct reference for the software triggered cal. */
	inst_calc_man_ref_white    = 0x00000010, /* place instrument on reflective white reference */
	inst_calc_man_ref_whitek   = 0x00000020, /* click instrument on reflective white reference */
	inst_calc_man_ref_dark     = 0x00000030, /* place instrument on light trap */
	inst_calc_man_dark_gloss   = 0x00000040, /* place instrument on gloss black reference */
	inst_calc_man_em_dark      = 0x00000050, /* place cap on instrument, put on dark surface or white ref. */
	inst_calc_man_am_dark      = 0x00000060, /* Place cap over ambient sensor (wl calib capable) */
	inst_calc_man_cal_smode    = 0x00000070, /* Put instrument sensor in calibration position */

	inst_calc_man_trans_white  = 0x00000080, /* place instrument on transmissive white reference */
	inst_calc_man_trans_dark   = 0x00000090, /* place instrument on transmissive dark reference */
	inst_calc_man_man_mask     = 0x000000F0, /* user configured calibration mask */ 

	inst_calc_emis_white       = 0x00000100, /* Provide a white test patch */
	inst_calc_emis_80pc        = 0x00000200, /* Provide an 80% white test patch */
	inst_calc_emis_grey        = 0x00000300, /* Provide a grey test patch */
	inst_calc_emis_grey_darker = 0x00000400, /* Provide a darker grey test patch */
	inst_calc_emis_grey_ligher = 0x00000500, /* Provide a darker grey test patch */

	inst_calc_emis_mask        = 0x00000F00, /* Emmissive/display provided reference patch */

	inst_calc_change_filter    = 0x00010000, /* Filter needs changing on device - see id[] */
	inst_calc_message          = 0x00020000, /* Issue a message. - see id[] */

	inst_calc_cond_mask        = 0x0fffffff, /* Mask for conditions (i.e. remove flags) */

	inst_calc_optional_flag    = 0x80000000  /* Flag indicating calibration can be skipped */

} inst_cal_cond;

/* Condition identifier message type. This can be useful in internationalizing the */
/* string returned in id[] from calibrate() */
typedef enum {
	inst_calc_id_none      = 0x00000000,	/* No id */
	inst_calc_id_ref_sn    = 0x00000001,	/* Calibration reference (ie. tile) serial number */

	inst_calc_id_trans_low = 0x00010000,	/* Trans. Ref. light is too low for accuracy warning */
	inst_calc_id_trans_wl  = 0x00020000,	/* Trans. Ref. light is low at some wavelengths warning */
	inst_calc_id_filt_unkn = 0x00100000,	/* Request unknown filter */
	inst_calc_id_filt_none = 0x00200000,	/* Request no filter */
	inst_calc_id_filt_pol  = 0x00300000,	/* Request polarizing filter */
	inst_calc_id_filt_D65  = 0x00400000,	/* Request D65 filter */
	inst_calc_id_filt_UV   = 0x00500000,	/* Request UV cut filter */
	inst_calc_id_filt_cust = 0x00600000		/* Request instrument custom filter */
} inst_calc_id_type;

/* Clamping state */
typedef enum {
    instNoClamp			= 0,	/* Don't clamp XYZ/Lab to +ve */
    instClamp			= 1,	/* Clamp XYZ/Lab to +ve */
} instClamping;

/* User interaction callback (uicallback()) function purpose */
typedef enum {
    inst_negcoms,		/* Negotiating communications - can abort */
    inst_armed,			/* Armed and waiting for a measurement trigger - can wait, trigger, abort */
    inst_triggered,		/* Measurement triggered by switch or user (not progromatic) - notice */
						/* (Also triggered by switch driven calibration,i.e. DTP41) */
    inst_measuring		/* Busy measuring - can abort */
} inst_ui_purp;

/* Asynchronous event callback type */
typedef enum {
    inst_event_switch,		/* Instrument measure/calibrate switch pressed. */
    inst_event_mconf,		/* Change in measurement configuration (ie. sensor position) */
    inst_event_scan_ready	/* Ready for manual strip scan (i.e. issue audible prompt) */
} inst_event_type;

/* Instrument configuration/sensor position*/
typedef enum {
    inst_conf_unknown,
    inst_conf_projector,
    inst_conf_surface,	
    inst_conf_emission,	
	inst_conf_calibration,
	inst_conf_ambient
} inst_config;

# define EXTRA_INST_OBJ

/* Off-line pending readings available (status) */
#define CALIDLEN 200	/* Maxumum length of calibration tile ID string */

/* Color instrument interface base object */
/* Note that some methods work after creation, while many */
/* will return an error if communications hasn't been established and */
/* the instrument initialised. Some may change their response before and */
/* after initialisation. */
#define INST_OBJ_BASE															\
																				\
	DEV_OBJ_BASE																\
	                                                                            \
	EXTRA_INST_OBJ																\
	                                                                            \
	double cal_gy_level; /* Display calibration test window state */			\
	int cal_gy_count;	/* Display calibration test window state */				\
	inst_code (*uicallback)(void *cntx, inst_ui_purp purp);						\
	void *uic_cntx;		/* User interaction callback function */				\
	void (*eventcallback)(void *cntx, inst_event_type event);					\
	void *event_cntx;	/* Asynchronous event callback function */				\
	athread *scan_ready_thread;	/* msec_scan_ready() support */					\
	int scan_ready_delay;		/* msec_scan_ready() support */					\
																				\
	/* Virtual delete. Cleans up things done by new_inst(). */					\
	inst_code (*vdel)(															\
        struct _inst *p);														\
																				\
	/* Establish communications at the indicated baud rate. */					\
	/* (Serial parameters are ignored for USB instrument) */					\
	/* Timout in to seconds, and return non-zero error code */					\
	inst_code (*init_coms)(														\
        struct _inst *p,														\
        baud_rate br,		/* Baud rate */										\
        flow_control fc,	/* Flow control */									\
        double tout);		/* Timeout */										\
																				\
	/* Initialise or re-initialise the INST */									\
	/* return non-zero on an error, with inst error code */						\
	inst_code (*init_inst)(  													\
        struct _inst *p);														\
																				\
	/* Return the instrument type */											\
	/* (this could concievably change after init_inst()) */						\
	/* Can be called before init */												\
	instType (*get_itype)(  													\
        struct _inst *p);														\
																				\
	/* Return the instrument serial number. */									\
	/* (This will be an empty string if there is no serial no) */               \
	char *(*get_serial_no)(  													\
        struct _inst *p);														\
																				\
	/* Return the avilable instrument modes and capabilities. */				\
	/* Can be called before init, but may be different to */					\
	/* what's returned after initilisation. */									\
	/* Note that these may change with the mode. */								\
	/* Arguments may be NULL */													\
	void (*capabilities)(struct _inst *p,										\
	        inst_mode *cap1,													\
	        inst2_capability *cap2,												\
	        inst3_capability *cap3);											\
																				\
	/* Return current or given configuration available measurement modes. */	\
	/* Most instruments have only one detectable configuration. */				\
	/* If conf_ix == NULL or *conf_ix is an invalid configuration index, */		\
	/* then the current configuration modes are returned. */					\
	/* Otherwise the given configuration index is returned. */					\
	/* The i1d3 has 2, the Munki has 4, one being calibration. */				\
	/* *cconds is valid if *mmodes = inst_mode_calibration */					\
	inst_code (*meas_config)(struct _inst *p,									\
			inst_mode *mmodes,	/* Return all the valid measurement modes */	\
								/* for the current configuration */				\
			inst_cal_cond *cconds,	/* Return the valid calibration conditions */		\
			int *conf_ix);		/* Request mode for given configuration, and */			\
	                            /* return the index of the configuration returned */	\
																				\
    /* Check that the particular device measurement mode is valid, */           \
	/* since it's not possible to be 100% sure from capabilities */				\
    inst_code (*check_mode)(													\
        struct _inst *p,														\
        inst_mode m);		/* Requested mode */								\
																				\
    /* Set the device measurement mode */                                       \
	/* Note that this may change the capabilities. */							\
    inst_code (*set_mode)(														\
        struct _inst *p,														\
        inst_mode m);		/* Requested mode */								\
																				\
	/* Return array of display type selectors */								\
	inst_code (*get_disptypesel)(												\
        struct _inst *p,														\
		int *no_selectors,		/* Return number of display types */			\
		inst_disptypesel **sels,/* Return the array of display types */			\
		int allconfig,			/* nz to return list for all configs, not just current. */	\
		int recreate);			/* nz to re-check for new ccmx & ccss files */	\
																				\
	/* Set the display type or calibration mode. */								\
	/* index is into the inst_disptypesel[] returned */   						\
	/* returned by get_disptypesel(). clears col_cor_mat() and */				\
	/* col_cal_spec_set(). */													\
	inst_code (*set_disptype)(													\
        struct _inst *p,														\
		int index);                                                             \
	                                                                            \
	/* Get the disptech and other corresponding info for the current */		    \
	/* selected display type or calibration mode. Returns disptype_unknown */	\
	/* by default. Because refrmode can be overridden, it may not */			\
	/* match the refrmode of the dtech. */										\
	/* (Pointers may be NULL if not needed) */									\
	inst_code (*get_disptechi)(													\
		struct _inst *p,														\
		disptech *dtech,														\
		int *refrmode,															\
		int *cbid);																\
	                                                                            \
    /* Get a status or get or set an option */                                  \
	/* option state. */															\
	/* Some options can be set before init */									\
	/* See inst_opt_type typedef for list of mode types */						\
    inst_code (*get_set_opt)(													\
        struct _inst *p,														\
        inst_opt_type m,	/* Requested option mode */							\
		...);				/* Option parameters */                             \
																				\
	/* Read a full test chart composed of multiple sheets */					\
	/* DOESN'T use the trigger mode */											\
	/* Return the inst error code */											\
	inst_code (*read_chart)(													\
		struct _inst *p,														\
		int npatch,			/* Total patches/values in chart */					\
		int pich,			/* Passes (rows) in chart */						\
		int sip,			/* Steps in each pass (patches in each row) */		\
		int *pis,			/* Passes in each strip (rows in each sheet) */		\
		int chid,			/* Chart ID number */								\
		ipatch *vals);		/* Pointer to array of values */					\
																				\
																				\
	/* For an xy instrument, release the paper */								\
	/* (if cap has inst_xy_holdrel) */											\
	/* Return the inst error code */											\
	inst_code (*xy_sheet_release)(												\
		struct _inst *p);														\
																				\
	/* For an xy instrument, hold the paper */									\
	/* (if cap has inst_xy_holdrel) */											\
	/* Return the inst error code */											\
	inst_code (*xy_sheet_hold)(													\
		struct _inst *p);														\
																				\
	/* For an xy instrument, allow the user to locate a point */				\
	/* (if cap has inst_xy_locate) */											\
	/* Return the inst error code */											\
	inst_code (*xy_locate_start)(												\
		struct _inst *p);														\
																				\
	/* For an xy instrument, read back the location */							\
	/* (if cap has inst_xy_locate) */											\
	/* Return the inst error code */											\
	inst_code (*xy_get_location)(												\
		struct _inst *p,														\
		double *x, double *y);													\
																				\
	/* For an xy instrument, end allowing the user to locate a point */			\
	/* (if cap has inst_xy_locate) */											\
	/* Return the inst error code */											\
	inst_code (*xy_locate_end)(													\
		struct _inst *p);														\
																				\
	/* For an xy instrument, move the measurement point */						\
	/* (if cap has inst_xy_position) */											\
	/* Return the inst error code */											\
	inst_code (*xy_position)(													\
		struct _inst *p,														\
		int measure,	/* nz for measure point, z for locate point */	        \
		double x, double y);													\
																				\
	/* For an xy instrument, try and clear the table after an abort */			\
	/* Return the inst error code */											\
	inst_code (*xy_clear)(														\
		struct _inst *p);														\
																				\
	/* Read a sheet full of patches using xy mode */							\
	/* DOESN'T use the trigger mode */											\
	/* Return the inst error code */											\
	inst_code (*read_xy)(														\
		struct _inst *p,														\
		int pis,			/* Passes in strips (letters in sheet) */			\
		int sip,			/* Steps in pass (numbers in sheet) */				\
		int npatch,			/* Total patches in strip (skip in last pass) */	\
		char *pname,		/* Starting pass name (' A' to 'ZZ') */             \
		char *sname,		/* Starting step name (' 1' to '99') */             \
		double ox, double oy,	/* Origin of first patch */						\
		double ax, double ay,	/* pass increment */							\
		double aax, double aay,	/* pass offset for odd patches */				\
		double px, double py,	/* step/patch increment */						\
		ipatch *vals);		/* Pointer to array of values */					\
																				\
																				\
	/* Read a set of strips (applicable to strip reader) */						\
	/* Obeys the trigger mode set, and may return user trigger code, */			\
	/* to hint that a user command may be available. */							\
	/* Return the inst error code */											\
	inst_code (*read_strip)(													\
		struct _inst *p,														\
		char *name,			/* Strip name (up to 7 chars, for DTP51) */			\
		int npatch,			/* Number of patches in each pass */				\
		char *pname,		/* Pass name (3 chars, for DTP51) */				\
		int sguide,			/* Guide number (for DTP51, decrements by 5) */		\
		double pwid,		/* Patch width in mm (For DTP20/DTP41) */			\
		double gwid,		/* Gap width in mm  (For DTP20/DTP41) */			\
		double twid,		/* Trailer width in mm  (For DTP41T) */				\
		ipatch *vals);		/* Pointer to array of values */					\
																				\
																				\
	/* Read a single sample (applicable to spot instruments) */					\
	/* Obeys the trigger mode set, and may return user trigger code */			\
	/* Values are in XYZ % (0..100) for reflective & transmissive, */			\
	/* cd/m^2 for emissive, and Lux for ambient. */								\
	/* Spectral will be analogous to the XYZ (see inst_meas_type). */			\
	/* By default values may be -ve due to noise (depending on instrument) */	\
	/* Return the inst error code */											\
	inst_code (*read_sample)(													\
		struct _inst *p,														\
		char *name,				/* Patch identifier (up to 7 chars) */			\
		ipatch *val,			/* Pointer to value to be returned */			\
		instClamping clamp);	/* NZ to clamp XYZ to be +ve */					\
																				\
	/* Measure the emissive refresh rate in Hz. */								\
	/* (Available if cap2 & inst2_emis_refr_meas) */ 							\
	/* Returns: */																\
	/* inst_unsupported - if this instrument doesn't suport this measuremet */	\
	/*                    or not in an emissive measurement mode */				\
	/* inst_misread     - if no refresh rate could be determined */				\
	/* inst_ok          - on returning a valid reading */						\
	inst_code (*read_refrate)(											        \
		struct _inst *p,														\
		double *ref_rate);		/* Return the Hz */								\
																				\
	/* Determine if a calibration is needed. Returns inst_calt_none if not */	\
	/* or unknown, or a mask of the needed calibrations. */						\
	/* This call checks if calibrations are invalid or have timed out. */		\
	/* With the exception of instruments with automated calibration */			\
	/* (ie. SpectroScan), an instrument will typically */						\
	/* not check for calibration timout any other way. */						\
	/* [What's returned is the same as get_n_a_cals() [ needed_calibrations.] */\
	inst_cal_type (*needs_calibration)(											\
		struct _inst *p);														\
																				\
	/* Return combined mask of needed and available inst_cal_type's */			\
	/* for the current mode. */													\
	inst_code (*get_n_a_cals)(													\
		struct _inst *p,														\
		inst_cal_type *needed_calibrations,										\
		inst_cal_type *available_calibrations);									\
																				\
	/* Request an instrument calibration. */									\
	/* This is use if the user decides they want to do a calibration */			\
	/* in anticipation of a calibration (needs_calibration()) to avoid */		\
	/* requiring one during measurement, or in response to measuring */			\
	/* returning inst_needs_cal before retrying the measurement, */				\
	/* or to do one or more re-calibrations. */									\
																				\
	/* *calt should contain the mask of calibrations to be performed, */		\
	/* with *calc set to the current calibration condition. */					\
	/* Alternately, one of the psudo-calibration types inst_calt_all, */		\
	/* inst_calt_needed or inst_calt_available can be used, */			\
	/* and/or the *calc of inst_calc_none to get calibrate() */					\
	/* to determine the required calibration types and conditions. */			\
	/* (The corresponding calibration types will be used & returned. */			\
																				\
	/* If no error is returned to the first call to calibrate() */				\
	/* then the instrument was capable of calibrating without user or */		\
	/* application intervention. If on the other hand calibrate() returns */	\
	/* inst_cal_setup, then the appropriate action indicated by the value */	\
	/* returned in *calc should be taken by the user or application, */			\
	/* before retrying calibration() with the current setup indicated */		\
	/* by *calc. If more than one calibration type is requested, then */		\
	/* several retries may be needed with different calibration conditions. */	\
 	/* Each call to calibrate() will update *calt to reflect the remaining */	\
	/* calibration to be performed.  calibrate() returns inst_ok when no */		\
	/* more calibrations remain. */												\
																				\
	/* If the calc has the inst_calc_optional_flag flag set, */					\
	/* then the user should be offered the option of skipping the */			\
	/* calibration. If they decide to skip it, return a calc with */			\
	/* inst_calc_optional_flag set, and if they want to proceed, */				\
	/* make sure the inst_calc_optional_flag is cleared in the returned */		\
	/* calc. */																	\
																				\
	/* DOESN'T use the trigger mode */											\
	/* Return inst_unsupported if *calt is not appropriate, */					\
	/* inst_cal_setup if *calc is not appropriate. */							\
	inst_code (*calibrate)(														\
		struct _inst *p,														\
		inst_cal_type *calt,	/* Calibration type to do/remaining */			\
		inst_cal_cond *calc,	/* Current condition/desired condition */		\
		inst_calc_id_type *idtype,	/* Condition identifier type */				\
		char id[CALIDLEN]);		/* Condition identifier (ie. white */			\
								/* reference ID, filter ID, message string, */	\
								/* etc.) */										\
																				\
	/* Measure a display update, and instrument reaction time. It is */			\
	/* assumed that a white to black change will be made to the */				\
	/* displayed color during this call, and this is used to measure */			\
	/* the time it took for the update to be noticed by the instrument, */		\
	/* up to 1.0 second. */												   		\
	/* The instrument reaction time accounts for the time between */			\
	/* when the measure function is called and the samples actually being */	\
	/* taken. This value may be negative if there is a filter delay. */			\
	/* The method white_change() should be called with init=1 before */			\
	/* calling meas_delay, and then with init=0 during the meas_delay() */	\
	/* call to timestamp the transition. */										\
	/* Note that a default instmsec will be returned even on error. */			\
	inst_code (*meas_delay)(											        \
		struct _inst *p,														\
		int *dispmsec,		/* Return display update delay in msec */			\
		int *instmsec);		/* Return instrument reaction time in msec */		\
																				\
	/* Call used by other thread to timestamp the patch transition. */			\
	inst_code (*white_change)(											        \
		struct _inst *p,														\
		int init);			/* nz to init time stamp, z to mark transition */	\
																				\
	/* Return the last calibrated refresh rate in Hz. Returns: */				\
	/* (Available if cap2 & inst2_get_refresh_rate) */ 							\
	/* inst_unsupported - if this instrument doesn't suport a refresh mode */	\
	/*                    or is unable to retrieve the refresh rate */			\
	/* inst_needs_cal   - if the refresh rate value is not valid */				\
	/* inst_misread     - if no refresh rate could be determined */				\
	/* inst_ok          - on returning a valid reading */						\
	inst_code (*get_refr_rate)(											        \
		struct _inst *p,														\
		double *ref_rate);		/* Return the Hz */								\
																				\
	/* Set the calibrated refresh rate in Hz. */								\
	/* (Available if cap2 & inst2_set_refresh_rate) */ 							\
	/* Set refresh rate to 0.0 to mark it as invalid */							\
	/* Rates outside the range 5.0 to 150.0 Hz will return an error */			\
	/* Note that not all instruments that can return a refresh rate, */			\
	/* will permit one to be set (ie., DTP92) */								\
	inst_code (*set_refr_rate)(											        \
		struct _inst *p,														\
		double ref_rate);		/* Rate in Hz */								\
																				\
	/* Insert a colorimetric correction matrix in the instrument XYZ readings */ \
	/* This is only valid for colorimetric instruments, and can only be */		\
	/* applied over a base calibration display type. Setting a display */		\
	/* type will clear the matrix. */											\
	/* To clear the matrix, pass NULL for the matrix */							\
	inst_code (*col_cor_mat)(											        \
		struct _inst *p,														\
		disptech dtech,		/* Use disptech_unknown if not known */				\
		int cbid,       	/* Calibration display type base ID, needed 1 if unknown */\
		double mtx[3][3]);	/* XYZ matrix */								    \
																				\
	/* Use a Colorimeter Calibration Spectral Set (ccss) to set the */			\
	/* instrumen calibration. This will affect emissive readings. */			\
	/* An alternate observer may also be set, and this will affect both */		\
	/* emissive and ambient readings. */										\
	/* This is only valid for colorimetric instruments. */						\
	/* To set calibration back to default, pass NULL for sets, and */			\
	/* icxOT_default for the observer. */										\
	inst_code (*col_cal_spec_set)(												\
		struct _inst *p,														\
		disptech dtech,		/* Use disptech_unknown if not known */				\
		xspect *sets,		/* Set of sample spectra */							\
		int no_sets); 		/* Number on set */									\
																				\
	/* Supply a user interaction callback function.								\
	 * This is called for one of three different purposes:						\
	 *	To signal that the instrument measurement has been triggered.			\
	 *	To poll for an abort while waiting to trigger.							\
	 *	To poll for a user abort during measurement.							\
	 *																			\
	 * The callback function will have the purpose paramater appropriately.		\
	 *																			\
     * For inst_negcoms, the return value of inst_user_abort					\
	 * will abort the communication negotiation.								\
	 *																			\
	 * For inst_armed return value should be one of: 							\
	 * inst_ok to do nothing, inst_user_abort to abort the measurement,			\
	 * or inst_user_trig to trigger the measurement.							\
	 *																			\
	 * For inst_triggered, the return value of the callback is ignored.			\
	 *																			\
	 * For inst_measuring the return value should be one of:					\
	 * inst_ok to do nothing, inst_user_abort to abort the measurement.			\
	 *																			\
	 * NULL can be set to disable the callback.									\
	 */																			\
	void (*set_uicallback)(struct _inst *p,										\
		inst_code (*uicallback)(void *cntx, inst_ui_purp purp), 				\
		void *cntx);															\
																				\
	/* Supply an aynchronous event callback function.							\
	 * This is called from a different thread with the following possible events:		\
	 *																			\
	 * inst_event_switch: Instrument measure/calibrate switch pressed 			\
	 * inst_event_mconf: The measurement configuration has changed (ie. sensor position)	\
     * inst_event_scan_ready: Ready for manual strip scan (i.e. issue audible prompt)	\
	 *																			\
	 * NULL can be set to disable the callback.									\
	 */																			\
	void (*set_event_callback)(struct _inst *p,									\
		void (*eventcallback)(void *cntx, inst_event_type event),				\
		void *cntx);															\
																				\
	/* Generic inst error codes interpretation */								\
	char * (*inst_interp_error)(struct _inst *p, inst_code ec);					\
																				\
	/* Instrument specific error codes interpretation */						\
	char * (*interp_error)(struct _inst *p, int ec);							\
																				\
	/* Convert instrument specific inst_wrong_config error to inst_config enum */	\
	inst_config (*config_enum)(struct _inst *p, int ec);							\
																				\
	/* Return the last serial communication error code */						\
	/* (This is used for deciding fallback/retry strategies) */					\
	int (*last_scomerr)(struct _inst *p);										\
																				\
	/* Destroy ourselves */														\
	void (*del)(struct _inst *p);												\

/* The base object type */
struct _inst {
	INST_OBJ_BASE
	}; typedef struct _inst inst;

/* Virtual constructor. */
/* Return NULL for unknown instrument, */
/* or serial instrument if nocoms == 0. */
/* (Doesn't copy icompaths log!) */
/* If uicallback is provided, it will be set in the resulting inst */
extern inst *new_inst(
	icompath *path,		/* Device path this instrument */
	int nocoms,			/* Don't open if communications are needed to establish inst type */
	a1log *log,			/* Log to use */
	inst_code (*uicallback)(void *cntx, inst_ui_purp purp),		/* optional uicallback */
	void *cntx			/* Context for callback */
);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Implementation functions used by instrument drivers */
/* (Client code should not call these) */

/* Get a status or set or get an option (default implementation) */
inst_code inst_get_set_opt_def(
inst *p,
inst_opt_type m,	/* Option type */
va_list args);		/* Option parameters */                             

/* - - - - - - - - - - - - - - - - - - -- */

/* Create the display type list */
inst_code inst_creat_disptype_list(inst *p,
int *pndtlist,					/* Number in returned list */
inst_disptypesel **pdtlist,		/* Returned list */
inst_disptypesel *sdtlist,		/* Static list */
int doccss,						/* Add installed ccss files */
int doccmx						/* Add matching installed ccmx files */
);

/* Free a display type list */
void inst_del_disptype_list(inst_disptypesel *list, int no);

/* - - - - - - - - - - - - - - - - - - -- */

/* Issue an inst_event_scan_ready event after a delay */
void issue_scan_ready(inst *p, int delay);

/* - - - - - - - - - - - - - - - - - - -- */
/* CCMX support - ccmx instrument proxy */

typedef struct {
	char *path;			/* Path to the file */
	char *desc;			/* Technology + display description */
	disptech dtech;		/* Display Technology enumeration (optional if disp) */
	int cc_cbid;       	/* Calibration display type base ID required */
	int  refr;			/* Refresh mode flag */
	char *sel;			/* UI selector characters (may be NULL) */
	int oem;			/* nz if oem origin */
	double mat[3][3];	/* The matrix values */
} iccmx;

/* return a list of installed ccmx files. */
/* if itype != instUnknown, return those that match the given instrument. */
/* The list is sorted by description and terminated by a NULL entry. */
/* If no is != NULL, return the number in the list */
/* Return NULL and -1 if there is a malloc error */
iccmx *list_iccmx(instType itype, int *no);

/* Free up a iccmx list */
void free_iccmx(iccmx *list);

/* - - - - - - - - - - - - - - - - - - -- */
/* CCSS support - ccss instrument proxy */

typedef struct {
	char *path;			/* Path to the file */
	char *desc;			/* Technology + display description */
	disptech dtech;		/* Display Technology enumeration (optional if disp) */
	int  refr;			/* Refresh mode flag */
	char *sel;			/* UI selector characters (may be NULL) */
	int oem;			/* nz if oem origin */
	xspect *sets;		/* Set of sample spectra */
	int no_sets;		/* Number on set */
} iccss;

/* return a list of installed ccss files. */
/* The list is sorted by description and terminated by a NULL entry. */
/* If no is != NULL, return the number in the list */
/* Return NULL and -1 if there is a malloc error */
iccss *list_iccss(int *no);

/* Free up a iccss list */
void free_iccss(iccss *list);

/* - - - - - - - - - - - - - - - - - - -- */
/* Custom filter support */

/* Apply a custom filer to an array of ipatch's */
/* Spetral values are divided by filter spectrum and XYZ recomputed */
void ipatch_convert_custom_filter(ipatch *vals, int nvals, xspect *filt, int clamp);

/* - - - - - - - - - - - - - - - - - - -- */

#ifdef __cplusplus
	}
#endif


#define INST_H
#endif /* INST_H */
