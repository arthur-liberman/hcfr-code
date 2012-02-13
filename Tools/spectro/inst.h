
#ifndef INST_H

 /* Abstract base class for common color instrument interface */
 /* and other common instrument stuff.                        */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   15/3/2001
 *
 * Copyright 2001 - 2010 Graeme W. Gill
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
#include "icoms.h"			/* libinst Includes this functionality */
#include "conv.h"

#ifdef __cplusplus
	extern "C" {
#endif

/* ------------------------------------------------- */
/* Structure for holding an instrument patch reading */

#define ICOM_MAX_LOC_LEN 10

struct _ipatch {
	char loc[ICOM_MAX_LOC_LEN];	/* patch location */

	int XYZ_v;			/* XYZ valid */
	double XYZ[3];		/* XYZ values, 0.0 .. 100.0 */

	int aXYZ_v;			/* Absolute XYZ valid */
	double aXYZ[3];		/* XYZ values in cd/m^2 or Lux/PI */
						/* or cd/m^2 seconds for flash. */

	int Lab_v;			/* Lab valid */
	double Lab[3];		/* Lab value */

	xspect sp;			/* Spectrum. sp.spec_n > 0 if valid */
						/* Reflectance/Transmittance 0.0 .. 100.0, norm = 100.0 */
						/* or mW/m^2, norm = 1.0  */

	double duration;	/* Apparent total duration in seconds (flash measurement) */

}; typedef struct _ipatch ipatch;

/* ---------------------------------------- */
/* Instrument interface abstract base class */

/* Abstract return codes in ms byte. */
/* Machine dependant codes in ls byte. */
/* Note :- update interp_code() in inst.c if anything here is changed. */
/* and also check all the instrument specific XXX_interp_code() routines too. */
typedef enum {
	inst_ok                = 0x0000,
	inst_notify            = 0x0100,	/* A Notification */
	inst_warning           = 0x0200,	/* A Warning */
	inst_internal_error    = 0x0300,	/* Internal software error */
	inst_coms_fail         = 0x0400,	/* Communication failure */
	inst_unknown_model     = 0x0500,	/* Not the expected instrument */
	inst_protocol_error    = 0x0600, 	/* Read or Write protocol error */
	inst_user_abort        = 0x0700,	/* User hit escape */
	inst_user_term         = 0x0800,	/* User hit terminate key */
	inst_user_trig         = 0x0900,	/* User hit trigger key */
	inst_user_cmnd         = 0x0A00,	/* User hit command key */
	inst_misread           = 0x0B00,	/* Bad reading, or strip misread */
	inst_nonesaved         = 0x0C00,	/* No saved data to read */
	inst_nochmatch         = 0x0D00,	/* Chart doesn't match */
	inst_needs_cal         = 0x0E00,	/* Instrument needs calibration, and read retried */
	inst_cal_setup         = 0x0F00,	/* Calibration retry with correct setup is needed */
	inst_wrong_sensor_pos  = 0x1000,	/* Reading retry with correct sensor position is needed */
	inst_unsupported       = 0x1100,	/* Unsupported function */
	inst_unexpected_reply  = 0x1200,	/* Unexpected Reply */
	inst_wrong_config      = 0x1300,    /* Configuration is wrong */
	inst_hardware_fail     = 0x1400,    /* Hardware failure */
	inst_bad_parameter     = 0x1500,	/* Bad parameter value */
	inst_other_error       = 0x1600,	/* Some other error */
	inst_mask              = 0xff00,	/* inst_code mask value */
	inst_imask             = 0x00ff		/* instrument specific mask value */
} inst_code;

/* Instrument capabilities */
/* (Some instrument capabilities may be mode dependent) */
typedef enum {
	inst_unknown            = 0x00000000, /* Capabilities can't be determined */

	inst_ref_spot           = 0x00000001, /* Capable of reflection spot measurement */
	inst_ref_strip          = 0x00000002, /* Capable of reflection strip measurement */
	inst_ref_xy             = 0x00000004, /* Capable of reflection X-Y page measurement */
	inst_ref_chart          = 0x00000008, /* Capable of reflection chart measurement */
	inst_reflection         = 0x0000000F, /* Capable of general reflection measurements */

	inst_s_ref_spot         = 0x00000010, /* Capable of saved reflection spot measurement */
	inst_s_ref_strip        = 0x00000020, /* Capable of saved reflection strip measurement */
	inst_s_ref_xy           = 0x00000040, /* Capable of saved reflection X-Y page measurement */
	inst_s_ref_chart        = 0x00000080, /* Capable of saved reflection chart measurement */
	inst_s_reflection       = 0x000000F0, /* Capable of general saved reflection measurements */
 
	inst_trans_spot         = 0x00000100, /* Capable of transmission spot measurement */
	inst_trans_strip        = 0x00000200, /* Capable of transmission strip measurement */
	inst_trans_xy           = 0x00000400, /* Capable of transmission X-Y measurement */
	inst_trans_chart        = 0x00000800, /* Capable of transmission chart measurement */
	inst_transmission       = 0x00000F00, /* Capable of general transmission measurements */

	inst_emis_spot          = 0x00001000, /* Capable of emission spot measurement */
	inst_emis_strip         = 0x00002000, /* Capable of emission strip measurement */
	inst_emis_disp          = 0x00004000, /* Capable of display emission measurement */
	inst_emis_disp_crt      = 0x00008000, /* Has a CRT display mode */
	inst_emis_disp_lcd      = 0x00010000, /* Has an LCD display mode */
	inst_emis_proj          = 0x00020000, /* Capable of projector emission measurement */
	inst_emis_proj_crt      = 0x00040000, /* Has a CRT display mode */
	inst_emis_proj_lcd      = 0x00080000, /* Has an LCD display mode */
	inst_emis_tele          = 0x00100000, /* Capable of telephoto emission measurement */
	inst_emis_ambient       = 0x00200000, /* Capable of ambient measurement */
	inst_emis_ambient_flash = 0x00400000, /* Capable of ambient flash measurement */
	inst_emis_ambient_mono  = 0x00800000, /* The ambient measurement is monochrome */
	inst_emission           = 0x00FFF000, /* Capable of general emission measurements */

	inst_colorimeter        = 0x01000000, /* Colorimetric capability */
	inst_spectral           = 0x02000000, /* Spectral capability */
	inst_highres            = 0x04000000, /* High Resolution Spectral mode */
	inst_ccmx               = 0x08000000, /* Colorimeter Correction Matrix capability */
	inst_ccss               = 0x10000000  /* Colorimeter Calibration Spectral Set capability */

} inst_capability;

/* Instrument capabilities 2 */
typedef enum {
	inst2_unknown           = 0x00000000, /* Capabilities can't be determined */

	inst2_xy_holdrel        = 0x00000001, /* Needs paper hold/release between each sheet */
	inst2_xy_locate         = 0x00000002, /* Needs user to locate patch locations */
	inst2_xy_position       = 0x00000004, /* Can be positioned at a given location */

	inst2_cal_ref_white     = 0x00000010, /* Uses a reflective white/dark calibration */
	inst2_cal_ref_dark      = 0x00000020, /* Uses a reflective dark calibration */
	inst2_cal_trans_white   = 0x00000040, /* Uses a transmissive white reference calibration */
	inst2_cal_trans_dark    = 0x00000080, /* Uses a transmissive dark reference calibration */
	inst2_cal_disp_offset   = 0x00000100, /* Uses a display offset/black calibration */
	inst2_cal_disp_ratio    = 0x00000200, /* Uses a display ratio calibration */
	inst2_cal_disp_int_time = 0x00000400, /* Uses a display integration time calibration */
	inst2_cal_proj_offset   = 0x00000800, /* Uses a display offset/black calibration */
	inst2_cal_proj_ratio    = 0x00001000, /* Uses a display ratio calibration */
	inst2_cal_proj_int_time = 0x00002000, /* Uses a display integration time calibration */
	inst2_cal_crt_freq      = 0x00004000, /* Uses a display CRT scan frequency calibration */

	inst2_prog_trig         = 0x00010000, /* Progromatic trigger measure capability */
	inst2_keyb_trig         = 0x00020000, /* Keyboard trigger measure capability */
	inst2_switch_trig       = 0x00040000, /* Inst. switch trigger measure capability */
	inst2_keyb_switch_trig  = 0x00080000, /* keyboard or switch trigger measure capability */

	inst2_bidi_scan         = 0x00100000, /* Try and recognise patches scanned from either dir. */
	inst2_cal_using_switch  = 0x00200000, /* DTP22 special - use switch triggered calibration */
	inst2_has_scan_toll     = 0x00400000, /* Instrument will honour modified scan tollerance */
	inst2_no_feedback       = 0x00800000, /* Instrument doesn't give any user feedback */

	inst2_has_leds          = 0x01000000, /* Instrument has some user viewable indicator LEDs */
	inst2_has_sensmode      = 0x02000000, /* Instrument can report it's sensors mode */

	inst2_has_battery       = 0x04000000  /* Instrument is battery powered */

} inst2_capability;

/* Instrument modes and sub-modes */
/* We assume that we only want to be in one measurement mode at a time */
typedef enum {
	inst_mode_unknown            = 0x0000,	/* Mode not specified */

	/* Sub modes that compose operating modes: */

	/* Mode of light measurement */
	inst_mode_reflection         = 0x0001,	/* General reflection mode */
	inst_mode_s_reflection       = 0x0002,	/* General saved reflection mode */
	inst_mode_transmission       = 0x0003,	/* General transmission mode */
	inst_mode_emission           = 0x0004,	/* General emission mode */
	inst_mode_illum_mask         = 0x000f,	/* Mask of sample illumination sub mode */

	/* Access mode of measurement */
	inst_mode_spot               = 0x0010,	/* General spot measurement mode */
	inst_mode_strip              = 0x0020,	/* General strip measurement mode */
	inst_mode_xy                 = 0x0030,	/* General X-Y measurement mode */
	inst_mode_chart              = 0x0040,	/* General chart measurement mode */
	inst_mode_ambient            = 0x0050,	/* General ambient measurement mode */
	inst_mode_ambient_flash      = 0x0060,	/* General ambient flash measurement mode */
	inst_mode_tele               = 0x0070,	/* General telephoto measurement mode */
	inst_mode_sub_mask           = 0x00f0,	/* Mask of sub-mode */

	/* Measurement modifiers */
	inst_mode_disp               = 0x0100,	/* Display device (non-adaptive/refresh) mode */
	inst_mode_mod_mask           = 0x0f00,	/* Mask of measurement modifiers */

	/* Combined operating modes (from above): */
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
	                             | inst_mode_transmission, 
	inst_mode_trans_strip        = inst_mode_strip	/* Transmission strip measurement mode */
	                             | inst_mode_transmission, 
	inst_mode_trans_xy           = inst_mode_xy		/* Transmission X-Y measurement mode */
	                             | inst_mode_transmission, 
	inst_mode_trans_chart        = inst_mode_chart	/* Transmission chart measurement mode */
	                             | inst_mode_transmission, 

	inst_mode_emis_spot          = inst_mode_spot	/* Spot emission measurement mode */
	                             | inst_mode_emission,
	inst_mode_emis_strip         = inst_mode_strip	/* Strip emission measurement mode */
	                             | inst_mode_emission,
	inst_mode_emis_disp          = inst_mode_disp	/* Display emission measurement mode */
	                             | inst_mode_spot
	                             | inst_mode_emission,
	inst_mode_emis_proj          = inst_mode_disp	/* Projector emission measurement mode */
	                             | inst_mode_tele
	                             | inst_mode_emission,
	inst_mode_emis_tele          = inst_mode_tele	/* Telephoto emission measurement mode */
	                             | inst_mode_emission,
	inst_mode_emis_ambient       = inst_mode_ambient	/* Ambient emission measurement mode */
	                             | inst_mode_emission,
	inst_mode_emis_ambient_flash = inst_mode_ambient_flash	/* Ambient emission flash measurement */
	                             | inst_mode_emission,

	inst_mode_measurement_mask   = inst_mode_mod_mask	/* Mask of exclusive measurement modes */
	                             | inst_mode_sub_mask
	                             | inst_mode_illum_mask,

	/* Independent extra modes */
	inst_mode_colorimeter        = 0x1000,	/* Colorimetric mode */
	inst_mode_spectral           = 0x2000	/* Spectral mode */

} inst_mode;

/* Instrument options for set_opt_mode() */
typedef enum {
	inst_opt_unknown            = 0x0000,	/* Option not specified */

	inst_opt_autocalib          = 0x0001,	/* Enable auto calibration (default) [No args] */
	inst_opt_noautocalib        = 0x0002,	/* Disable auto calibration [No args] */

	inst_opt_disp_crt           = 0x0003,	/* CRT display technology [No args] */
	inst_opt_disp_lcd           = 0x0004,	/* LCD display technology [No args] */
	inst_opt_proj_crt           = 0x0005,	/* CRT display technology [No args] */
	inst_opt_proj_lcd           = 0x0006,	/* LCD display technology [No args] */

	inst_opt_set_filter         = 0x0007,	/* Set a filter configuration */
											/* [1 argument type inst_opt_filter] */

	inst_opt_trig_prog          = 0x0008,	/* Trigger progromatically [No args] */
	inst_opt_trig_keyb          = 0x0009,	/* Trigger from keyboard (default) [No args] */
	inst_opt_trig_switch        = 0x000A,	/* Trigger using instrument switch [No args] */
	inst_opt_trig_keyb_switch   = 0x000B,	/* Trigger using keyboard or switch (def) [No args] */

	inst_opt_trig_return        = 0x000C,	/* Hack - emit "\n" after switch/kbd trigger [No args] */
	inst_opt_trig_no_return     = 0x000D,	/* Hack - don't emit "\n" after trigger (def) [No args] */

	inst_opt_highres            = 0x000E,	/* Enable high resolution spectral mode */
	inst_opt_stdres             = 0x000F,	/* Revert to standard resolution spectral mode */

	inst_opt_scan_toll          = 0x0010,	/* Modify the patch scan recognition tollnce [double] */

	inst_opt_get_gen_ledmask    = 0x0011,	/* Get the bitmask for general indication LEDs [*int] */
											/* (More specialized indicator masks go here) */
	inst_opt_set_led_state      = 0x0012,	/* Set the current LED state. 0 = off, 1 == on [int] */
	inst_opt_get_led_state      = 0x0013,	/* Get the current LED state. 0 = off, 1 == on [*int] */

	inst_opt_get_pulse_ledmask  = 0x0014,	/* Get the bitmask for pulseable ind. LEDs [*int] */
	inst_opt_set_led_pulse_state= 0x0015,	/* Set the current LED state. [double period_in_secs, */
	                                        /* double on_time_prop, double trans_time_prop] */
	inst_opt_get_led_pulse_state= 0x0016	/* Get the current pulse LED state. [*double period, */

} inst_opt_mode;

/* Instrument status commands for get_status() */
typedef enum {
	inst_stat_unknown           = 0x0000,	/* Status type not specified */

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

	inst_stat_sensmode          = 0x0007,	/* Return sensor mode */
											/* [1 argument type *inst_stat_smode ] */

	inst_stat_get_filter        = 0x0008	/* Set a filter configuration */
											/* [1 argument type *inst_opt_filter ] */
} inst_status_type;


/* Optional filter fitted to instrument (for inst_opt_set_filter) */
typedef enum {
	inst_opt_filter_unknown  = 0xffff,	/* Unspecified filter */
	inst_opt_filter_none     = 0x0000,	/* No filters fitted */
	inst_opt_filter_pol      = 0x0001,	/* Polarising filter */ 
	inst_opt_filter_D65      = 0x0002,	/* D65 Illuminant filter */
	inst_opt_filter_UVCut    = 0x0004,	/* U.V. Cut filter */
	inst_opt_filter_Custom   = 0x0008	/* Custom Filter */
} inst_opt_filter;

/* Off-line pending readings available (status) */
typedef enum {
	inst_stat_savdrd_none    = 0x00,		/* No saved readings */
	inst_stat_savdrd_spot    = 0x01,		/* There are saved spot readings available */
	inst_stat_savdrd_strip   = 0x02,		/* There are saved strip readings available */
	inst_stat_savdrd_xy      = 0x04,		/* There are saved page readings available */
	inst_stat_savdrd_chart   = 0x08			/* There are saved chart readings available */
} inst_stat_savdrd;

/* Sensor mode/position (status) */
/* Note that this is a mask, as the same position may be suitable */
/* for several different types of measurement. */
typedef enum {
	inst_stat_smode_unknown = 0x00,	/* Unknown mode */
	inst_stat_smode_calib   = 0x01,	/* Calibration tile */
	inst_stat_smode_ref     = 0x02,	/* Reflective */
	inst_stat_smode_disp    = 0x04,	/* Display */
	inst_stat_smode_proj    = 0x08,	/* Projector */
	inst_stat_smode_amb     = 0x10	/* Ambient */
} inst_stat_smode;

/* Type of user interaction */
typedef enum {
	inst_verb              = 0x00,	/* verbose message */
	inst_question          = 0x01	/* A question requiring answers */
} inst_uiact;

/* Type of calibration needed/requested - corresponds to capabilities */
typedef enum {
	/* Response to needs_calibration() */
	inst_calt_unknown        = 0x00, 	/* Unknown whether calibration is needed */
	inst_calt_none           = 0x01, 	/* No callibration is needed */

	/* Type to prompt all needed calibrations */
	inst_calt_all            = 0x10, 	/* All applicable calibrations */

	/* Specific type of calibration - corresponds to capabilities */
	inst_calt_ref_white      = 0x20, 	/* Reflective white/emissive dark calibration */
	inst_calt_ref_dark       = 0x30, 	/* Reflective dark calibration (in dark) */
	inst_calt_disp_offset    = 0x40, 	/* Display offset/black calibration (dark surface) */
	inst_calt_disp_ratio     = 0x50, 	/* Display ratio calibration */
	inst_calt_proj_offset    = 0x60, 	/* Display offset/black calibration (dark surface) */
	inst_calt_proj_ratio     = 0x70, 	/* Display ratio calibration */
	inst_calt_crt_freq       = 0x80, 	/* Display CRT scan frequency calibration */
	inst_calt_disp_int_time  = 0x90, 	/* Display integration time */
	inst_calt_proj_int_time  = 0xA0, 	/* Display integration time */
	inst_calt_em_dark        = 0xB0, 	/* Emissive dark calibration (in dark) */
	inst_calt_trans_white    = 0xC0,	/* Transmissive white reference calibration */
	inst_calt_trans_dark     = 0xD0,	/* Transmissive dark reference calibration */

	inst_calt_needs_cal_mask = 0xF0		/* One of the calibrations in needed */

} inst_cal_type;

/* Calibration conditions. */
/* This is how the instrument communicates to the calling program */
/* about how to facilitate a calibration */
typedef enum {
	inst_calc_none             = 0x00000000, /* No particular calibration setup, or unknown */

	inst_calc_uop_ref_white    = 0x00000001, /* user operated reflective white calibration */
	inst_calc_uop_trans_white  = 0x00000002, /* user operated tranmissive white calibration */
	inst_calc_uop_trans_dark   = 0x00000003, /* user operated tranmissive dark calibration */
	inst_calc_uop_mask         = 0x0000000F, /* user operated calibration mask */

	inst_calc_man_ref_white    = 0x00000010, /* place instrument on reflective white reference */
	inst_calc_man_ref_whitek   = 0x00000020, /* click instrument on reflective white reference */
	inst_calc_man_ref_dark     = 0x00000030, /* place instrument in dark, not close to anything */
	inst_calc_man_em_dark      = 0x00000040, /* place cap on instrument, put on dark surface or white ref. */
	inst_calc_man_cal_smode    = 0x00000050, /* Put instrument sensor in calibration position */

	inst_calc_man_trans_white  = 0x00000060, /* place instrument on transmissive white reference */
	inst_calc_man_trans_dark   = 0x00000070, /* place instrument on transmissive dark reference */
	inst_calc_man_man_mask     = 0x000000F0, /* user configured calibration mask */ 

	inst_calc_disp_white       = 0x00000100, /* Provide a white display test patch */
	inst_calc_disp_grey        = 0x00000200, /* Provide a grey display test patch */
	inst_calc_disp_grey_darker = 0x00000300, /* Provide a darker grey display test patch */
	inst_calc_disp_grey_ligher = 0x00000400, /* Provide a darker grey display test patch */
	inst_calc_disp_mask        = 0x00000F00, /* Display provided reference patch */

	inst_calc_proj_white       = 0x00001000, /* Provide a white projector test patch */
	inst_calc_proj_grey        = 0x00002000, /* Provide a grey projector test patch */
	inst_calc_proj_grey_darker = 0x00003000, /* Provide a darker grey projector test patch */
	inst_calc_proj_grey_ligher = 0x00004000, /* Provide a darker grey projector test patch */
	inst_calc_proj_mask        = 0x0000F000, /* Projector provided reference patch */

	inst_calc_change_filter    = 0x00010000, /* Filter needs changing on device - see id[] */
	inst_calc_message          = 0x00020000  /* Issue a message. - see id[] */
} inst_cal_cond;

#define CALIDLEN 200

/* Color instrument interface base object */
#define INST_OBJ_BASE															\
																				\
	int debug;		/* debug level, 1..9 */										\
	int verb;		/* Verbosity level */                                       \
	instType  prelim_itype;	/* Instrument type determined by cons/USB */		\
	instType  itype;	/* Instrument type determined by driver */				\
	icoms *icom;	/* Instrument coms object */								\
	int gotcoms;	/* Coms established flag */                                 \
	int inited;		/* Initialised flag */                                      \
	double cal_gy_level; /* Display calibration test window state */			\
	int cal_gy_count;	/* Display calibration test window state */				\
																				\
	/* Establish communications at the indicated baud rate. */					\
	/* (Serial parameters are ignored for USB instrument) */					\
	/* Timout in to seconds, and return non-zero error code */					\
	inst_code (*init_coms)(														\
        struct _inst *p,														\
        int comport,		/* icom communication port number */				\
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
	/* This may not be valid until after init_inst() */							\
	instType (*get_itype)(  													\
        struct _inst *p);														\
																				\
	/* Return the instrument serial number. */									\
	/* (This will be an empty string if there is no serial no) */               \
	char *(*get_serial_no)(  													\
        struct _inst *p);														\
																				\
	/* Return the instrument capabilities. */									\
	/* Note that these may change with the mode. */								\
	inst_capability (*capabilities)(struct _inst *p);							\
	inst2_capability (*capabilities2)(struct _inst *p);							\
																				\
    /* Set the device measurement mode */                                       \
	/* Note that this may change the capabilities. */							\
    inst_code (*set_mode)(														\
        struct _inst *p,														\
        inst_mode m);		/* Requested mode */								\
																				\
    /* Set or reset an option */												\
    inst_code (*set_opt_mode)(													\
        struct _inst *p,														\
        inst_opt_mode m,	/* Requested option mode */							\
		...);				/* Option parameters */                             \
																				\
    /* Get a (dynamic) status */												\
    inst_code (*get_status)(													\
        struct _inst *p,														\
        inst_status_type m,	/* Requested status type */							\
		...);				/* Status parameters */                             \
																				\
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
	/* Obeys the trigger mode set, and may return user trigger code */			\
	/* Return the inst error code */											\
	inst_code (*read_strip)(													\
		struct _inst *p,														\
		char *name,			/* Strip name (up to 7 chars) */					\
		int npatch,			/* Number of patches in each pass */				\
		char *pname,		/* Pass name (3 chars) */							\
		int sguide,			/* Guide number (decrements by 5) */				\
		double pwid,		/* Patch width in mm (For DTP20/DTP41) */			\
		double gwid,		/* Gap width in mm  (For DTP20/DTP41) */			\
		double twid,		/* Trailer width in mm  (For DTP41T) */				\
		ipatch *vals);		/* Pointer to array of values */					\
																				\
																				\
	/* Read a single sample (applicable to spot instruments) */					\
	/* Obeys the trigger mode set, and may return user trigger code */			\
	/* Values are in XYZ 0..100 for reflective transmissive, */					\
	/* aXYZ in cd/m^2 for emissive, amd Lux/3.1415926 for ambient. */			\
	/* Spectral will be analogous to the XYZ/aXYZ. */							\
	/* Return the inst error code */											\
	inst_code (*read_sample)(													\
		struct _inst *p,														\
		char *name,			/* Patch identifier (up to 7 chars) */				\
		ipatch *val);		/* Pointer to value to be returned */				\
																				\
	/* Determine if a calibration is needed. Returns inst_calt_none if not, */  \
	/* inst_calt_unknown if it is unknown, or inst_calt_XXX if needs calibration, */ \
	/* and the first type of calibration needed. */								\
	inst_cal_type (*needs_calibration)(											\
		struct _inst *p);														\
																				\
	/* Request an instrument calibration. */									\
	/* This is use if the user decides they want to do a calibration */			\
	/* in anticipation of a calibration (needs_calibration()) to avoid */		\
	/* requiring one during measurement, or in response to measuring */			\
	/* returning inst_needs_cal, before retrying the measurement. */			\
																				\
	/* An initial inst_cal_cond of inst_calc_none may be used to do the */		\
	/* default or necessary calibration, or a specific calibration supported */	\
	/* by the instrument may be chosen. */										\
																				\
	/* If no error is returned to the first call to calibrate() with */			\
	/* inst_calc_none, then the instrument was capable of calibrating */		\
	/* without user or application intervention. If on the other hand */		\
	/* calibrate() returns inst_cal_setup, then the appropriate action */		\
	/* indicated by the value returned in *calc should be taken by the */		\
	/* user or application, before retrying calibration() with the */			\
	/* current setup indicated by *calc. If several different calibrations */	\
	/* are being performed, then several retries may be needed with */			\
	/* different setups, untill calibrate() returns inst_ok. */					\
																				\
	/* DOESN'T use the trigger mode */											\
	/* Return inst_unsupported if calibration type is not appropriate. */		\
	inst_code (*calibrate)(														\
		struct _inst *p,														\
		inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */ \
		inst_cal_cond *calc,	/* Current condition/desired condition */		\
		char id[CALIDLEN]);		/* Condition identifier (ie. white reference ID, */	\
		                        /* filter ID) */								\
																				\
	/* Insert a compensation filter in the instrument readings */				\
	/* This is typically needed if an adapter is being used, that alters */     \
	/* the spectrum of the light reaching the instrument */                     \
	/* To remove the filter, pass NULL for the filter filename */               \
	inst_code (*comp_filter)(											        \
		struct _inst *p,														\
		char *filtername);		/* File containing compensating filter */		\
																				\
	/* Insert a colorimetric correction matrix in the instrument XYZ readings */ \
	/* This is only valid for colorimetric instruments. */						\
	/* To remove the matrix, pass NULL for the matrix */						\
	inst_code (*col_cor_mat)(											        \
		struct _inst *p,														\
		double mtx[3][3]);		/* XYZ matrix */								\
																				\
	/* Use a Colorimeter Calibration Spectral Set (ccss) to set the */			\
	/* instrumen calibration. This will affect emissive readings. */			\
	/* An alternate observer may also be set, and this will affect both */		\
	/* emmissive and ambient readings. */										\
	/* This is only valid for colorimetric instruments. */						\
	/* To set calibration back to default, pass NULL for sets, and */			\
	/* icxOT_default for the observer. */										\
	inst_code (*col_cal_spec_set)(												\
		struct _inst *p,														\
		icxObserverType obType,	/* Observer */									\
		xspect custObserver[3],	/* Optional custom observer */					\
		xspect *sets,			/* Set of sample spectra */						\
		int no_sets);	 		/* Number on set */								\
																				\
	/* Send a message to the user. */											\
	inst_code (*message_user)(struct _inst *p, char *fmt, ...);					\
																				\
	/* Poll for a user abort, terminate, trigger or command. */					\
	/* Wait for a key rather than polling, if wait != 0 */						\
	/* Return: */																\
	/* inst_ok if no key has been hit, */										\
	/* inst_user_abort if User abort has been hit, */							\
	/* inst_user_term if User terminate has been hit. */						\
	/* inst_user_trig if User trigger has been hit */							\
	/* inst_user_cmnd if User command has been hit */							\
	inst_code (*poll_user)(struct _inst *p, int wait);							\
																				\
																				\
	/* Generic inst error codes interpretation */								\
	char * (*inst_interp_error)(struct _inst *p, inst_code ec);					\
																				\
	/* Instrument specific error codes interpretation */						\
	char * (*interp_error)(struct _inst *p, int ec);							\
																				\
	/* Return the last communication error code */								\
	int (*last_comerr)(struct _inst *p);										\
																				\
	/* Destroy ourselves */														\
	void (*del)(struct _inst *p);												\

/* The base object type */
struct _inst {
	INST_OBJ_BASE
	}; typedef struct _inst inst;

/* Constructor */
extern inst *new_inst(
	int comport,		/* icom communication port number */
	instType itype,		/* Usually instUnknown to auto detect type of instrument */
	int debug,			/* Debug level, 0 = off */
	int verb			/* Verbose level, 0  = off */
);

/* ======================================================================= */

/* Opaque type as far as inst.h is concerned. */
typedef struct _disp_win_info disp_win_info;

/* A default calibration user interaction handler using the console. */
/* This handles both normal and display based calibration interaction */
/* with the instrument, if a disp_setup function and pointer to disp_win_info */
/* is provided. */
inst_code inst_handle_calibrate(
	inst *p,
	inst_cal_type calt,		/* type of calibration to do. inst_calt_all for all */
	inst_cal_cond calc,		/* Current condition. inst_calc_none for not setup */
	inst_code (*disp_setup) (inst *p, inst_cal_cond calc, disp_win_info *dwi),
							/* Callback for handling a display calibration - May be NULL */
	disp_win_info *dwi		/* Information to be able to open a display test patch - May be NULL */
);

#ifdef __cplusplus
	}
#endif

#define INST_H
#endif /* INST_H */
