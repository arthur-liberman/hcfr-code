#ifndef I1PRO3_IMP_H

/* 
 * Argyll Color Management System
 *
 * Gretag i1Pro implementation defines
 */

/*
 * Author: Graeme W. Gill
 * Date:   14/9/2020
 *
 * Copyright 2006 - 2021 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#ifdef __cplusplus
	extern "C" {
#endif

/* 
   If you make use of the instrument driver code here, please note
   that it is the author(s) of the code who are responsibility
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

/* Implementation resources for i1pro3 driver */

/* -------------------------------------------------- */
/* Implementation class */

typedef int i1pro3_code;		/* Type to use for i1pro3 error codes */

/* I1PRO3 mode state. This is implementation data that */
/* depends on the measurement mode the instrument is in. */
/* Each mode has a separate calibration, and configured instrument state, */
/* although the data may be copied between the states. */

typedef enum {
	i1p3_refl_spot      = 0,
	i1p3_refl_spot_pol  = 1,
	i1p3_refl_scan      = 2,
	i1p3_refl_scan_pol  = 3,
	i1p3_emiss_spot_na  = 4,
	i1p3_emiss_spot     = 5,
	i1p3_emiss_scan     = 6,
	i1p3_amb_spot       = 7,
	i1p3_amb_flash      = 8,
	i1p3_trans_spot     = 9,
	i1p3_trans_scan     = 10,
	i1p3_no_modes       = 11	/* Number of modes */
} i1p3_mode;

/* A modes dark calib usage */
typedef enum {
	i1p3_dc_none       = 0,			/* Doesn't use black calibration */
	i1p3_dc_adaptive   = 1			/* Single black calibration type */
} i1p3_dc_usage;

/* A modes main calib usage */
typedef enum {
	i1p3_mc_pcalfactor    = 1,		/* Preset calibration factor */
	i1p3_mc_mcalfactor    = 2,		/* Measured calibration factor */
	i1p3_mc_reflective    = 3,		/* Reflective calibration data */
	i1p3_mc_polreflective = 4		/* Polarized reflective calibration data */
} i1p3_mc_usage;

struct _i1pro3_state {
	i1p3_mode mode;		/* i1pro3 mode number */

	/* Just one of the following 3 must always be set */
	int emiss;			/* flag - Emissive mode */
	int trans;			/* flag - Transmissive mode */
	int reflective;		/* flag - Reflective mode */

	/* The following can be added to emiss */
	int ambient;		/* flag - Ambient mode */

	/* The following can be added to any of the 3: */
	int scan;			/* flag - Scanning mode */
	int adaptive;		/* flag - adaptive mode */

	/* The following can be added to scan: */
	int flash;			/* flag - Flash detection from scan mode */

	/* The following can be added to reflective */
	int pol;			/* flag - Polarized filter mode */

	/* (Only some combinations are supported) */

	/* Configuration & state information */
	double targoscale;	/*   Optimal reading scale factor <= 1.0 */
	double iinttime;	/*   Initial calibration integration time to use for this mode */
	double inttime;		/* # Integration time to use for this mode */
						/*   Save/rest. for pol, emiss & !scan & adaptive, (emiss | trans) & scan */
	double dcaltime;	/*   Target dark calibration time - sets number of readings */
	double dlcaltime;	/*   Target dark long calibration time - sets number of readings */
	double wcaltime;	/*   Target white calibration time - sets number of readings */
	double wscaltime;	/*   Target scan first white calibration time - sets number of readings */

	double dreadtime;	/*   Target dark/white on-the-fly cal time - sets number of readings */
	double wreadtime;	/*   Target white/sample reading time - sets number of readings */

	double maxscantime;	/*   Maximum scan time sets buffer size allocated */

	double min_wl;		/*   Minimum wavelength to report for this mode */

	/* Calibration state for this mode. */
	/* Each mode has storage for all possible calibration state, but only */
	/* set, uses, saves and restores the parts that are relevant to that mode. */
	/* State that should be saved/restored marked with # */
	/* (Could use union for mode exclusive state) */

	/* Wavelength calibration - all modes use this */
	/* [ Should we move this to common data to simplify things ?? ] */ 
	int want_wlcalib;		/*   Want wl calib at start */
	int wl_valid;			/* # wavelength calibration factor valid */
	time_t wl_date;			/* # Date/time of last wavelength calibration */
	double wl_temp;			/* # Board temperature when wl cal was done */
	double wl_cal_raw_off;	/* # Wavelength LED reference spectrum measured offset (raw cells) */
	double wl_cal_wav_off;	/* # Wavelength LED reference spectrum measured offset (wavlength) */

	/* Dark calibration */
	i1p3_dc_usage dc_use;	/*   What usage is made of dark calbration state if any */
	int want_dcalib;		/*   Want this modes dark Calibration at start */
	int dark_valid;			/* # dark calibration factor valid */
	double dtemp;			/* # Board temperature of last dark calibration */
	time_t ddate;			/* # Date/time of last dark calibration */

	/* dc_use: i1p3_dc_adaptive: Adaptive emission, ambient, transparency black cal. */
	double idark_int_time[2];    /* Integration times of idark_data */
	double **idark_data;	/* # [2][-1 nraw] adaptive static & variable black */

	/* Non adaptive state */
	double done_dintsel;	/*   A display integration time selection has been done */
	time_t diseldate;		/*   Date/time of last selection */

	/* Main calibration */
	i1p3_mc_usage mc_use;	/*   What usage is made of main calbration state if any */
	int want_calib;			/*   Want this modes main calibration at start */
	int cal_valid;			/* # Main calibration valid */
	time_t cdate;			/* # Date/time of last main cal calibration */

	/* mc_use: i1p3_mc_mcalfactor:	Transmission modes: */
	double *cal_factor[2];	/* # [s,h][nwav] main calibration scale factor */
	double *raw_white;		/* # [-1 nraw] linear normalized dark sub. white data (patch recog.) */

	/* mc_use: i1p3_mc_reflective:	Reflective calibration information */
	double calraw_white[2][128];	/* # [nn, uv] raw white reference measurement (patch recog.) */
	double calsp_ledm[2][36];		/* # [nn, uv] Reference LED model */
	double *calsp_illcr[2][2];		/* # [s,h][nn, uv] Illumination correction */
	double *iavg2aillum[2];			/* # [s,h] 1.0/(2 x avg. nn & uv LED illumination @ sensor)  */
	double *sc_calsp_nn_white[2];	/* # [s,h] nn drift & stray corrected., */
									/* scaled to ref. nn illum. integrated magnitude */
	double *cal_l_uv_diff[2];		/* # [s,h] Scaled diff. between with/without long UV */
	double *cal_s_uv_diff[2];		/* # [s,h] Scaled diff. between with/without short UV */

	/* mc_use: i1p3_mc_polreflective:	Polarized reflective calibration information */
	double pol_calraw_white[128];	/* # raw white reference measurement (patch recog.) */
	double pol_calsp_ledm[36];		/* # Reference LED model */
	double *pol_calsp_white[2];		/* # [s,h] white reference measurement */

}; typedef struct _i1pro3_state i1pro3_state;
 
/* Pointers to the three tables that allow a raw to wave filter conversion */
typedef struct {
	int *index;			/* [nwav] Matrix CCD sample starting index for each out wavelength */
	int *nocoef; 		/* [nwav] Number of matrix cooeficients for each out wavelength */
	double *coef;		/* [nwav * mtx_nocoef] Matrix cooeficients to compute each wavelength */
} i1pro3_r2wtab;

/* Data about an emissive dynamic black shielded value, used for smoothing */
typedef struct {
	time_t meastime;	/* Time of measurement */ 
	double temp;		/* Board temperature at time of measurement */
	double inttime;		/* Integration time of measurement */ 
	double sv;			/* Variable shield value (normalized to 1.0 seconds int time) */
} i1pro3_dynshval;

#define N_DYNSHVAL 5	/* Number of i1pro3_dynshval to average */

/* Capability bits stored in capabilities ?? */
#define I1PRO3_CAP_AMBIENT		0x01		/* Has ambient measurement capability */	
#define I1PRO3_CAP_WL_LED		0x02		/* Has wavelenght LED */	
#define I1PRO3_CAP_ZEB_RUL		0x04		/* Has zerbra ruler sensor */	
#define I1PRO3_CAP_IND_LED		0x08		/* Has user indicator LEDs */
#define I1PRO3_CAP_HEAD_SENS	0x10		/* Has a head sensor */
#define I1PRO3_CAP_POL			0x20		/* Has polarizer */

/* I1PRO3 implementation class */
struct _i1pro3imp {
	i1pro3 *p;

	/* Misc. and top level */
	amutex lock;                /* USB control port access lock */
	athread *th;				/* Switch monitoring thread (NULL if not used) */
	volatile int switch_count;	/* Switch down count, incremented in thread */
	volatile int hide_event;	/* Set to supress event event during read */
	usb_cancelt sw_cancel;		/* Token to allow cancelling event I/O */
	volatile int th_term;		/* Terminate thread on next return */
	volatile int th_termed;		/* Thread has terminated */
	usb_cancelt rd_sync;		/* Token to allow meas. read to synchronize trigger */
	usb_cancelt rd_sync2;		/* Token to allow meas. 2nd read to synchronize trigger */
	inst_opt_type trig;			/* Reading trigger mode */
	int noinitcalib;			/* Disable initial calibration if not essential */
	int highres;				/* High resolution mode */

	/* Current settings */
	inst_mode imode;			/* Current inst mode mask */
	i1p3_mode mmode;			/* Corresponding measurement mode selected */
	i1pro3_state ms[i1p3_no_modes];	/* Mode state */
	int spec_en;				/* NZ to enable reporting of spectral data */

	xcalstd native_calstd;		/* Instrument native calibration standard */
	xcalstd target_calstd;		/* Returned calibration standard */

	inst_opt_filter filt;		 /* Reflective measurement filter currently active */

	int custfilt_en;			/* Custom filter enabled */
	xspect custfilt;			/* Custom filter */

	/* Current state of hardware, to avoid uncessary operations */
	unsigned int llamponoff;	/* The last time the LEDs were switched from on to off, in msec */

	/* Values from i1pro3_getmeaschar() */
	double intclkp;				/* Integration clock period (typically 15.3 usec, half Rev E) */
	unsigned int minintclks;	/* Minimum integration clocks (typically 165, same as Rev E) */

	/* Delayed measurement trigger parameters */
	double c_inttime;			/* Integration time */
	int c_refinst;				/* Reflective mode trigger */
	unsigned int c_nummeas;		/* Number of measurements */
	unsigned int c_intclocks;	/* Number of integration clocks */
	unsigned int c_measflags;	/* Measurement flags */

	/* Values read from Getfwver() */
	int fwver;					/* int - Firmware version number * 100 */
	char fwvstr[50];			/* Firmware version string */

	/* Value from getchipid() */
	unsigned char hw_chipid[8];	/* HW serial number */

	int eesize;					/* EEProm size in bytes (hard coded) */

	/* Information from the EEProm */
	int version;			/* EEProm layout version. 0, 1 are known. */
	int serno;				/* Serial number */
	int aperture;			/* 0 = 4.5 mm, 1 = 8 mm */
	char sserno[14];		/* serial number as string */

	/* Information about the instrument from the EEprom */
	int capabilities;		/* Capabilities flags - see I1PRO3_CAP_* #define */

	/* Underlying calibration information */
	int nsen1;				/* Raw + extra sample bands read with emisssion command. */
							/* i1pro3 has 134, of which 128 are measurements. */
							/* The first 6 are from covered cells */

	int nsen2;				/* Raw + extra sample bands read with reflective command. */
							/* 282 samples. structured as 6 + 134 + 2 + 134 + 6 */
							/* each pair being without/with UV. */
							/* Data gets shuffled into odd/even sense data + 8 common */
							/* values to the odd & even, with 6 values ingnored. */
							/* The 134 values are as per emissive. */

	int nraw;				/* Raw sample bands stored = 128 (Must be signed!) */
	unsigned int nwav[2];	/* [std res, high res] cooked spectrum bands stored, ie = 36, 108 */
	double wl_width[2];		/* [std res, high res] cooked spectrum bands width in nm, ie 10, 3.33 */
	double wl_short[2];		/* [std res, high res] cooked spectrum bands short wavelength, ie 380 */
	double wl_long[2];		/* [std res, high res] cooked spectrum bands short wavelength, ie 730 */

	double min_int_time;	/* Minimum integration time (secs) */
	double max_int_time;	/* Maximum integration time (secs) */

	double wl_raw_off;		/* Wavelength LED reference spectrum current offset (raw cells) */
	double wl_wav_off;		/* Wavelength LED reference spectrum current offset (wavlength) */
							/* Should use wl_raw_off or wl_wav_off but not both. */
	i1pro3_r2wtab mtx[2][2];/* Raw to wav filters [normal res, high res][emis/trans, reflective] */
							/* These are all pointers to tables allocated below */

	double *white_ref[2];	/* [s,h][nwav] White cal tile reflectance values */
	double *emis_coef[2];	/* [s,h][nwav] Emission cal coefficients */
	double *amb_coef[2];	/* [s,h][nwav] Ambient light cal values */
							/* (compound with Emission), NULL if ambient not supported */
	double **straylight[2];	/* [nwav][nwav] Stray light convolution matrix as doubles */

	double *m0_fwa[2];		/* [s,h][nwav] m0 FWA coefficient ?? */
	double *m1_fwa[2];		/* [s,h][nwav] m1 FWA coefficient ?? */
	double *m2_fwa[2];		/* [s,h][nwav] m2 FWA coefficient ?? */

	double *fwa_cal[2];		/* [s,h][nwav] FWA spectra, calibration ??  */
	double *fwa_std[2];		/* [s,h][nwav] FWA spectra, standard ?? */

	double scan_toll_ratio;	/* Modifier of scan tollerance */

	int sens_sat;			/* sensor saturated threshold (after black subtraction) */
	int sens_target;		/* sensor optimal target value (after black subtraction) */

	double wl_cal_inttime;	/* Wavelength calibration integration time */
	double wl_cal_min_level;/* Normalized wavelength calibration minumum peak level */
	double wl_cal_fwhm;		/* Wavelength cal expected FWHM (nm) */
	double wl_cal_fwhm_tol;	/* Wavelength cal expected FWHM tollerance (nm) */
	double wl_led_spec[128];/* Wavelength LED processed reference spectrum [128] */
	double wl_err_max;		/* Wavelength error maximum value (nm) */
	double wl_tempcoef;		/* Wavelength temperature coefficient, 0.0 if unknown */
	int wl_refpeakloc;		/* Nominal wavelength reference peak raw location */
	double wl_refpeakwl;	/* Nominal wavelength reference peak wl */

	/* LED drift model (points to corresponding ee data) */
	/* aux is voltage, temperature, spectral 0..5 */
	/* [nn, uv][aux][3 or 4] */
	/* (Loaded directly from eeprom) */
	double ledm_poly[2][8][4][36];

	/* Emissive black dynamic shield value history for smoothing */
	int dynsh_ix;			/* Index of latest value */
	i1pro3_dynshval dynsh[N_DYNSHVAL];

	/* Trigger houskeeping & diagnostics */
	int transwarn;			/* Transmission calibration warning state */
	int lo_secs;			/* Seconds since last opened (from calibration file mod time) */ 
	int msec;				/* msec_time() at creation */
	athread *trig_thread;	/* Delayed trigger thread */
	int trig_delay;			/* Trigger delay in msec */
	int tr_t1, tr_t2, tr_t3, tr_t4, tr_t5, tr_t6, tr_t7;	/* Trigger/read timing diagnostics */
							/* 1->2 = time to execute trigger */
							/* 2->3 = time to between end trigger and start of first read */
							/* 3->4 = time to exectute first read */
							/* 6->5 = time between end of second last read and start of last read */
	int trig_se;			/* Delayed trigger icoms error */
	i1pro3_code trig_rv;	/* Delayed trigger result */

	int get_events_sent;	/* nz if i1p3cc_get_events command sent */

	athread *seve_thread;	/* Simulated delayed event thread */
	int seve_delay;			/* Simulated event delay in msec */
	int seve_code;			/* Simulated event code to send */
	int seve_se;			/* Simulated event icoms error */
	i1pro3_code seve_rv;	/* Simulated event result */

	volatile double whitestamp;	/* meas_delay() white timestamp */
	volatile double trigstamp;	/* meas_delay() trigger timestamp */

	/* Variables used by zebra read thread */
	unsigned char *zebra_buf;	/* Where to read data to */
	int zebra_bsize;			/* Bytes available in buffer */
	int zebra_bread;			/* Return number of bytes read */
	int zebra_rv;				/* Return value */

	/* Temporaries used by save/restore calibration */
	int nv_av;				/* Argyll version */
	int nv_ss;				/* Structure signature */
	int nv_serno;
	int nv_nraw;
	int nv_nwav0;
	int nv_nwav1;

	/* - - - - - - - - - - - - - - - - - - - - - - - - */
	/* Parsed data from EEProm. Not all of it is used. */
	// Some values are copied to above.

	/* Block 0 */
	int ee_crc_0;		/* */
	int ee_version;		/* EEProm layout version. 0, 1 are known. */
	int ee_unk01;		/* */
	int ee_bk1_off;		/* Offset to block 1 */
	int ee_bk2_off;		/* Offset to block 2 */
	int ee_bk3_off;		/* Offset to block 3 */
	int ee_bk4_off;		/* Offset to block 4 */
	int ee_bk5_off;		/* Offset to block 5 */
	int ee_bk6_off;		/* Offset to block 6 */

#ifndef NEVER			/* We're not using or maintaining the EEProm last cal. log */
	/* Block 1 */
	int ee_logA_crc;
	int ee_logA_remspotcalc;
	int ee_logA_scanmeasc;
	double ee_logA_lamptime;
	int ee_logA_emspotcalc;
	int ee_logA_emscancalc;
	int ee_logA_darkreading[128];
	int ee_logA_whitereading[128];
	int ee_logA_caldate;
	int ee_logA_calcount;
	int ee_logA_caldate2;
	int ee_logA_calcount2;
	int ee1_logA_unk02[128];

	/* Block 2 */
	int ee_logB_crc;
	int ee_logB_remspotcalc;
	int ee_scanmeasc;
	double ee_logB_lamptime;
	int ee_logB_emspotcalc;
	int ee_logB_emscancalc;
	int ee_logB_darkreading[128];
	int ee_logB_whitereading[128];
	int ee_logB_caldate;
	int ee_logB_calcount;
	int ee_logB_caldate2;
	int ee_logB_calcount2;
	int ee1_logB_unk02[128];
#endif /* NEVER */

	/* Block 3 */
	int ee_crc_3;
	int ee_unk03;
	int ee_unk04;
	double ee_lin[4];			/* Linearity correction poly cooefs. */
	double ee_white_ref[36];	/* Reflectivity of white calibration tile */
	double ee_uv_white_ref[36];	/* Not used */
	double ee_emis_coef[36];	/* Scale needed for flat emissive response */	
	double ee_amb_coef[36];		/* Scale needed on top of emissive for ambient adapter */
	int ee_unk05;
	int ee_unk06;
	int ee_unk07;
	int ee_unk08;
	int ee_wl_cal_min_level;	/* 500 Not used */
	int ee_unk09;
	int ee_unk10;
	int ee_unk11;
	int ee_unk12;
	int ee_unk13;
	double ee_wlcal_intt;	/* Ref. wavelength LED integration time */
	int ee_sens_sat;		/* Saturation detect level (after subtracting black) */
	int ee_serno;			/* serial number */
	int ee_dom1;
	int ee_dom2;
	int ee_unk14[5];
	int ee_unk15;
	int ee_devtype;	/* SA/LA Device type flag */
	int ee_unk16;
	unsigned char ee_chipid[8];
	int ee_capabilities;	/* Capabilities flags */
	int ee_unk17;
	double ee_bk_v_limit;	/* Black integration time limit factor (100) */
	double ee_bk_f_limit;	/* Black fixed limit factor (30) */
	double ee_unk18;
	double ee_unk19;
	double ee_unk20;
	double ee_unk21;
	double ee_unk22;
	double ee_unk23;
	double ee_unk24;
	double ee_unk25;
	int ee_wlcal_spec[128];	/* Reference sensor values of green wavelength LED. */
	int ee_uv_wlcal_spec[128];		/* Not used */
	double ee_wlcal_max;
	double ee_fwhm;
	double ee_fwhm_tol;
	double ee_unk35;
	int ee_straylight[1296];	/* Raw stray light matrix */
	double ee_straylight_scale;
	double ee_wl_cal1[128];	/* Sensor bin -> calibrated wavelength, reflective */
	double ee_wl_cal2[128];	/* Sensor bin -> calibrated wavelength, emissive */
	double ee_unk26;

			/* This is the current settings for the 5 possible illumination LEDs */
	int ee_led_w_cur;	/* Illuminating White LED current */
	int ee_led_b_cur;	/* Illuminating Blue LED current */
	int ee_led_luv_cur;	/* Illuminating Long UV LED current */
	int ee_led_suv_cur;	/* Illuminating Short UV LED current */
	int ee_led_gwl_cur;	/* Illuminating Green Wavelength LED current */

	int ee_button_bytes;	/* i1p3cc_send_events parameter value */

	/* LED model polynomial cooeficients are in ledm_poly[] above */

	/* FWA related */
	double ee_m0_fwa[36];	/* M0 ('A') FWA peak ? */
	double ee_m1_fwa[36];	/* M1 (D50) FWA peak ? */
	double ee_m2_fwa[36];	/* M2 (UVcut) FWA peak ? */

	double ee_suv_inttarg;	/* short UV integral target */
	double ee_luv_inttarg;	/* long UV integral target */
	double ee_sluv_bl;	/* Blend between long and short UV Leds */

	double ee_fwa_cal[36];	/* FWA cal factor ? Maybe UV absorbtion ???? */
	double ee_fwa_std[36];	/* WA standard ? */

	double ee_ref_nn_illum[36];	/* Reference reflected LED illumination ex. UV */
		/* LED no uv with black, linear & stray corrected reflected spectrum from white tile ? */ 
	double ee_ref_uv_illum[36];	/* Reference reflected LED illumination inc. UV */
		/* LED w. uv with black, linear & stray corrected reflected spectrum from white tile ? */ 

	int ee1_pol_led_luv_cur;	/* Polarized illuminating Long UV LED current */
	int ee1_pol_led_suv_cur;	/* Polarized illuminating Short UV LED current */

	double ee1_wltempcoef;	/* wl/delta_temperature adjustment factor */
							/* adjust by - (current_board_temp - cal_board_temp) * ee1_2f88 */

	/* Block 4 */
	int ee_unk27[4];
	int ee_unk28[4];
	int ee_unk29;
	int ee_unk30;
	int ee_unk31;

	/* Block 5 */
	char ee_supplier[17];
	int ee_unk32[4];
	int ee_unk33[256];

	/* Block 6 */
	int ee_unk34;

}; typedef struct _i1pro3imp i1pro3imp;

/* Add an implementation structure */
i1pro3_code add_i1pro3imp(i1pro3 *p);

/* Destroy implementation structure */
void del_i1pro3imp(i1pro3 *p);

/* ============================================================ */
/* Error codes returned from i1pro3_imp, type is i1pro3_code */

/* Note: update i1pro3_interp_error() and i1pro3_interp_code() in i1pro3.c */
/* if anything of these #defines are added or subtracted */

/* Fake Error codes */
#define I1PRO3_INTERNAL_ERROR			0x71		/* Internal software error */
#define I1PRO3_COMS_FAIL				0x72		/* Communication failure */
#define I1PRO3_UNKNOWN_MODEL			0x73		/* Not an i1pro3 */
#define I1PRO3_DATA_PARSE_ERROR  		0x74		/* Read data parsing error */

#define I1PRO3_USER_ABORT		    	0x75		/* uicallback returned abort */
#define I1PRO3_USER_TRIG 		    	0x76		/* uicallback retuned trigger */

#define I1PRO3_UNSUPPORTED		   		0x79		/* Unsupported function */
#define I1PRO3_CAL_SETUP                0x7A		/* Cal. retry with correct setup is needed */
#define I1PRO3_CAL_TRANSWHITEWARN       0x7B		/* Transmission white ref wl are low */

/* Real error code */
#define I1PRO3_OK   					0x00

/* HW errors */
#define I1PRO3_HW_EE_VERSION	        0x01		/* EEProm version is unknown */
#define I1PRO3_HW_EE_CHKSUM		        0x02		/* EEProm has bad checksum */
#define I1PRO3_HW_EE_RANGE			    0x03		/* EEPfom attempt to read out of buffer range */
#define I1PRO3_HW_EE_FORMAT		        0x04		/* EEProm seems corrupt */
#define I1PRO3_HW_EE_CHIPID		        0x05		/* HW 8 EE ChipId don't match */
#define I1PRO3_HW_EE_SHORTREAD		    0x06		/* Read fewer EEProm bytes than expected */
#define I1PRO3_HW_ME_SHORTREAD		    0x07		/* Read measurement bytes than expected */
#define I1PRO3_HW_SW_SHORTREAD          0x09		/* Read less bytes for Switch read than expected */
#define I1PRO3_HW_LED_SHORTWRITE        0x0C		/* Wrote fewer LED seq. bytes than expected */
#define I1PRO3_WL_TOOLOW                0x0D		/* WL calibration measurement too low */
#define I1PRO3_WL_SHAPE                 0x0E		/* WL calibration measurement shape is wrong */
#define I1PRO3_WL_ERR2BIG               0x0F		/* WL calibration correction is too big */

/* User errors */
#define I1PRO3_SPOS_CAL			 		0x20		/* Need to be on cal. tile */
#define I1PRO3_SPOS_STD			 		0x21		/* Need standard/surface adapter */
#define I1PRO3_SPOS_AMB		 			0x22		/* Need ambient adapter */
#define I1PRO3_SPOS_POLCAL			 	0x23		/* Need have polarizer filter and on cal */
#define I1PRO3_SPOS_POL		 			0x24		/* Need polarization filter */

/* Meas. sample read operation errors */
#define I1PRO3_RD_SENSORSATURATED	    0x31		/* Sensor is saturated */
#define I1PRO3_RD_DARKNOTVALID   	    0x32		/* Dark reading is not valid (too light) */
#define I1PRO3_RD_NEEDS_CAL 		    0x33		/* Mode needs calibration */
#define I1PRO3_RD_WHITEREADINCONS       0x34		/* White reference readings are inconsistent */
#define I1PRO3_RD_SHORTMEAS             0x38		/* Measurment was too short */
#define I1PRO3_RD_READINCONS            0x39		/* Reading is inconsistent */
#define I1PRO3_RD_TRANSWHITELEVEL       0x3A		/* Transmission white reference is too dark */
#define I1PRO3_RD_NOTENOUGHPATCHES      0x3B		/* Not enough patches */
#define I1PRO3_RD_TOOMANYPATCHES        0x3C		/* Too many patches */
#define I1PRO3_RD_NOTENOUGHSAMPLES      0x3D		/* Not enough samples per patch */
#define I1PRO3_RD_NOFLASHES             0x3E		/* No flashes recognized */
#define I1PRO3_RD_NOAMBB4FLASHES        0x3F		/* No ambient before flashes found */
#define I1PRO3_RD_NOREFR_FOUND          0x40		/* Unable to measure refresh rate */
#define I1PRO3_RD_NOTRANS_FOUND         0x41		/* Unable to measure delay transition */

/* Internal errors */
#define I1PRO3_INT_NO_COMS 		        0x50
#define I1PRO3_INT_EETOOBIG 		    0x51		/* EEProm read size is too big */
#define I1PRO3_INT_ODDREADBUF 	        0x53		/* Measurment read buffer is not expeted mult */
#define I1PRO3_INT_ILLEGALMODE			0x57		/* Illegal measurement mode selected */
#define I1PRO3_INT_WRONGMODE  			0x58		/* In wrong mode for request */
#define I1PRO3_INT_ZEROMEASURES 		0x59		/* Number of measurements requested is zero */
#define I1PRO3_INT_WRONGPATCHES 		0x5A		/* Number of patches to match is wrong */
#define I1PRO3_INT_MEASBUFFTOOSMALL 	0x5B		/* Measurement read buffer is too small */
#define I1PRO3_INT_NOTIMPLEMENTED 		0x5C		/* Support not implemented */
#define I1PRO3_INT_NOTCALIBRATED 		0x5D		/* Unexpectedely invalid calibration */
#define I1PRO3_INT_THREADFAILED 		0x5F		/* Creation of thread failed */
#define I1PRO3_INT_BUTTONTIMEOUT 	    0x60		/* Switch status read timed out */
#define I1PRO3_INT_CIECONVFAIL 	        0x61		/* Creating spectral to CIE converted failed */
#define I1PRO3_INT_MALLOC               0x63		/* Error in mallocing memory */
#define I1PRO3_INT_CREATE_EEPROM_STORE  0x64		/* Error in creating EEProm store */
#define I1PRO3_INT_CAL_SAVE             0x69		/* Unable to save calibration to file */
#define I1PRO3_INT_CAL_RESTORE          0x6A		/* Unable to restore calibration from file */
#define I1PRO3_INT_CAL_TOUCH            0x6B		/* Unable to touch calibration file */
#define I1PRO3_INT_ASSERT               0x6F		/* Internal assert */

int icoms2i1pro3_err(int se);

/* ============================================================ */
/* High level implementatation */

/* Initialise our software state from the hardware */
i1pro3_code i1pro3_imp_init(i1pro3 *p);

/* Return a pointer to the serial number */
char *i1pro3_imp_get_serial_no(i1pro3 *p);

/* Return non-zero if capable of ambient mode */
int i1pro3_imp_ambient(i1pro3 *p);

/* Check and ivalidate calibration */
i1pro3_code i1pro3_check_calib(i1pro3 *p);

/* Set the measurement mode. It may need calibrating */
i1pro3_code i1pro3_imp_set_mode(
	i1pro3 *p,
	i1p3_mode mmode,		/* i1pro3 mode to use */
	inst_mode m);		/* full mode mask */

/* Implement get_n_a_cals */
i1pro3_code i1pro3_imp_get_n_a_cals(i1pro3 *p, inst_cal_type *pn_cals, inst_cal_type *pa_cals);

/* Calibrate for the current mode. */
/* Request an instrument calibration of the current mode. */
i1pro3_code i1pro3_imp_calibrate(
	i1pro3 *p,
	inst_cal_type *calt,	/* Calibration type to do/remaining */
	inst_cal_cond *calc,	/* Current condition/desired condition */
	inst_calc_id_type *idtype,	/* Condition identifier type */
	char id[100]			/* Condition identifier (ie. white reference ID) */
);

/* Measure a patch or strip in the current mode. */
i1pro3_code i1pro3_imp_measure(
	i1pro3 *p,
	ipatch *val,		/* Pointer to array of instrument patch value */
	int nvals,			/* Number of values */	
	instClamping clamp	/* Clamp XYZ/Lab to be +ve */
);

/* Measure the emissive refresh rate */
i1pro3_code i1pro3_imp_meas_refrate(
	i1pro3 *p,
	double *ref_rate
);

/* Measure the display update delay */
i1pro3_code i1pro3_imp_meas_delay(
	i1pro3 *p,
	int *pdispmsec,
	int *pinstmsec);

/* Timestamp the white patch change during meas_delay() */
inst_code i1pro3_imp_white_change(i1pro3 *p, int init);

/* Recompute normal & hi-res wav filters using the current wl_raw/wav_off */
i1pro3_code i1pro3_compute_wav_filters(i1pro3 *p, double wl_raw_off, double wl_wav_off, int force);

/* Set the current wl_raw/wav_off for the given board temperature and then */
/* recompute normal & hi-res wav filters */
i1pro3_code i1pro3_recompute_wav_filters_for_temp(i1pro3 *p, double temp);


/* Given a raw measurement of the wavelength LED, */
/* Compute the base offset that best fits it to the reference */
/* Returns both raw and wav offset, but only one should be used for correction */
i1pro3_code i1pro3_match_wl_meas(i1pro3 *p, double *pwav_off, double *praw_off, double *wlraw);

/* Compute downsampling filters using the default filters. */
/* mtx_index1, mtx_nocoef1, mtx_coef1 given the */
/* current wl_raw_off */
i1pro3_code i1pro3_compute_wav_filter(i1pro3 *p, int hires, int reflective);

/* return nz if high res is supported */
int i1pro3_imp_highres(i1pro3 *p);

/* Set to high resolution mode */
i1pro3_code i1pro3_set_highres(i1pro3 *p);

/* Set to standard resolution mode */
i1pro3_code i1pro3_set_stdres(i1pro3 *p);

/* Modify the scan consistency tollerance */
i1pro3_code i1pro3_set_scan_toll(i1pro3 *p, double toll_ratio);

/* Save the calibration for all modes, stored on local system */
i1pro3_code i1pro3_save_calibration(i1pro3 *p);

/* Restore the all modes calibration from the local system */
i1pro3_code i1pro3_restore_calibration(i1pro3 *p);

/* Update the modification time on the file, so we can */
/* track when the instrument was last open. */
i1pro3_code i1pro3_touch_calibration(i1pro3 *p);

/* ============================================================ */

/* Set the noinitcalib mode */
void i1pro3_set_noinitcalib(i1pro3 *p, int v, int losecs);

/* Set the trigger config */
void i1pro3_set_trig(i1pro3 *p, inst_opt_type trig);

/* Return the trigger config */
inst_opt_type i1pro3_get_trig(i1pro3 *p);

/* Set the trigger return */
void i1pro3_set_trigret(i1pro3 *p, int val);

/* Switch thread handler */
int i1pro3_event_thread(void *pp);

i1pro3_code i1pro3_conv2XYZ(
	i1pro3 *p,
	ipatch *vals,		/* Values to return */
	int nvals,			/* Number of values */
	double **specrd,	/* Spectral readings */
	int hr,				/* 1 for hr */
	instClamping clamp	/* Clamp XYZ/Lab to be +ve */
);

/* ============================================================ */
/* Intermediate routines  - composite commands/processing */

/* Compute the number of measurements needed, given the target */
/* time and integration time. Will return a minimum, of 1 */
int i1pro3_comp_nummeas(
	i1pro3 *p,
	double meas_time,
	double int_time
);

/* Delay the given number of msec from last time the LEDs were */
/* turned off during reflective measurement */
void i1pro3_delay_llampoff(i1pro3 *p, unsigned int delay);

/* Take a wavelength reference measurement */
/* (Measure and subtracts black, linearizes) */
i1pro3_code i1pro3_wl_measure(
	i1pro3 *p,
	double *raw,	/* Return array [nraw] of raw values */
	double *temp	/* Return the board temperature */
);

/* Do a reflectance calibration */
i1pro3_code i1pro3_refl_cal(
	i1pro3 *p
);

/* Do a spot reflectance measurement */
i1pro3_code i1pro3_spot_refl_meas(
	i1pro3 *p,
	double **specrd,	/* Cooked spectral patch value to return */
	int hr				/* 1 for high-res. */
);

/* Do a strip reflectance measurement */
i1pro3_code i1pro3_strip_refl_meas(
	i1pro3 *p,
	double **specrd,	/* Cooked spectral patches values to return */
	int nvals,			/* Number of patches expected */
	int hr				/* 1 for high-res. */
);

/* Do a polarized reflectance calibration */
i1pro3_code i1pro3_pol_refl_cal(
	i1pro3 *p
);

/* Do a polarized spot reflectance measurement */
i1pro3_code i1pro3_pol_spot_refl_meas(
	i1pro3 *p,
	double **specrd,	/* Cooked spectral patch value to return */
	int hr				/* 1 for high-res. */
);

/* Do a polarized strip reflectance measurement */
i1pro3_code i1pro3_pol_strip_refl_meas(
	i1pro3 *p,
	double **specrd,	/* Cooked spectral patches values to return */
	int nvals,			/* Number of patches expected */
	int hr				/* 1 for high-res. */
);

/* Do an adaptive emissive/ambient/tranmissive calibration */
i1pro3_code i1pro3_adapt_emis_cal(
	i1pro3 *p,
	double *btemp		/* Return the board temperature */
);

/* Do an adaptive spot emission/ambient/transmissive measurement */
i1pro3_code i1pro3_spot_adapt_emis_meas(
	i1pro3 *p,
	double **specrd     /* Cooked spectral patch value to return */
);

i1pro3_code i1pro3_trans_cal(
	i1pro3 *p
);

/* Do scan emission/ambient/transmissive measurement */
i1pro3_code i1pro3_scan_emis_meas(
	i1pro3 *p,
	double *duration,	/* If flash, return duration */
	double **specrd,    /* Cooked spectral patch values to return */
	int nvals			/* Number of patches expected */
);

/* ============================================================ */
/* Medium level support functions */

/* Recognized patch location */
typedef struct {
	int ss;				/* Start sample index */
	int no;				/* Number of samples */
	int use;			/* nz if patch is to be used */
} i1pro3_patch;

/* Locate the required number of ref/trans patch locations, */
/* and return a list of the patch boundaries */
i1pro3_code i1pro3_locate_patches(
	i1pro3 *p,
	i1pro3_patch *patches,	/* Return patches[tnpatch] patch locations */
	int tnpatch,			/* Target number of patches to recognise */
	double **rawmeas,		/* Array of [nummeas][nraw] value to locate in */
	int nummeas,			/* number of raw samples */
	int **p2m				/* If not NULL, contains nummeas zebra ruler patch indexes */
);

/* Turn a raw set of reflective samples into one calibrated patch spectral value */
/* Samples have been black subtracted, linearized and converted to wav */
i1pro3_code i1pro3_comp_refl_value(
	i1pro3 *p,
	double *m0_spec,		/* Return m0 spectra if not NULL */
	double *m1_spec,		/* Return m1 spectra if not NULL */
	double *m2_spec,		/* Return m2 spectra if not NULL */
	double **eproc_sample,	/* eproc_sample[enumsample][-9, nwav] */
	int enummeas,			/* Number of even samples */
	double **oproc_sample,	/* oproc_sample[onumsample][-9, nwav] */
	int onummeas,			/* Number of odd samples */
	int hr					/* High res flag */
);

/* Turn a raw set of polarized reflective samples into one calibrated patch spectral value */
/* Samples have been black subtracted, linearized and converted to wav */
i1pro3_code i1pro3_comp_pol_refl_value(
	i1pro3 *p,
	double *spec,			/* Return calibrated spectra */
	double **proc_sample,	/* proc_sample[enumsample][-9, nwav] */
	int nummeas,			/* Number of samples */
	int hr					/* High res flag */
);

/* Filter dynamic black shield value to smooth out randomness */
double i1pro3_dynsh_filt(
	i1pro3 *p,
	time_t meastime,
	double temp,
	double inttime,
	double sv
);

/* Construct a black to subtract from an emissive measurement. */
void i1pro3_comp_emis_black(
	i1pro3 *p,
	double *black,			/* Return raw black to subtract */
	double **raw_black1,	/* Before and after short measurements */
	double **raw_black2,
	int nummeasB,			/* Number of samples in short measurements */
	double binttime, 		/* Integration time of short measurements */
	double **raw_sample,	/* Longer sample measurement */
	int nummeasS,			/* Number of samples in longer measurement */
	double sinttime,		/* Integration time of longer measurement */
	double btemp			/* Current board temperature */
);

/* Do an non/adaptive emission type measurement and return the processed raw values */
/* We don't check the adapter type. */
/* It is checked for consistency */
/* To compute to calibrated wav need to:
	convert from raw to wav
	apply straylight
	apply wav calibration factors
	average together
*/
i1pro3_code i1pro3_spot_adapt_emis_raw_meas(
	i1pro3 *p,
	double ***praw,		/* Return raw measurements. */
						/* Call i1pro3_free_raw(p, i1p3mm_em, raw, nummeas) when done */
	int *pnummeas		/* Return number of raw measurements */
);

/* Construct a simple (staticly) adjusted black to subtract from an emissive measurement. */
void i1pro3_comp_simple_emis_black(
	i1pro3 *p,
	double *black,			/* Return raw black to subtract */
	double **raw_sample,	/* Sample measurements */
	int nummeas,			/* Number of samples */
	double inttime			/* Integration time */
);

/* Do a simple emission measurement and return the processed raw values */
/* We don't check the adapter type. */
/* To compute to calibrated wav values need to:
	convert from raw to wav
	apply straylight
	apply wav calibration factors
*/
i1pro3_code i1pro3_spot_simple_emis_raw_meas(
	i1pro3 *p,
	double ***praw,		/* Return raw measurements. */
						/* Call i1pro3_free_raw(p, i1p3mm_em, raw, nummeas) when done */
	int *pnummeas,		/* Return number of raw measurements */
	double *inttime,	/* Integration time to use */
	double meastime,	/* Measurement time to use */
	int donorm			/* Normalize for integration time */
);

/* Do a simple emission measurement and return the processed and calibrated wav values */
i1pro3_code i1pro3_spot_simple_emis_meas(
	i1pro3 *p,
	double **specrd,		/* Return array [numpatches][nwav] of spectral reading values */
	int numpatches,			/* Number of sample to measure */
	double *inttime, 		/* Integration time to use/used */
	int hr					/* High resolution flag */
);

/* Recognise any flashes in the readings, and */
/* and average their values together as well as summing their duration. */
/* Return nz on an error */
i1pro3_code i1pro3_extract_patches_flash(
	i1pro3 *p,
	double *duration,		/* return duration */
	double *pavg,			/* return patch average [nraw] */
	double **multimeas,		/* Array of [nummeas][nraw] value to extract from */
	int nummeas,			/* number of readings made */
	double inttime			/* Integration time (used to compute duration) */
);

/* ============================================================ */
/* Lower level reading processing */

/* Check a raw black for sanity */
/* Returns nz if not sane */
int i1pro3_multimeas_check_black(
	i1pro3 *p,
	double **rawmmeas,		/* Array of [nummeas][-1 nraw] values to check */
	int nummeas,
	double inttime
);

/* Subtract black from a raw unscaled multimeas */
void i1pro3_multimeas_sub_black(
	i1pro3 *p,
	double **rawmmeas,		/* Array of [nummeas][nraw] values to subtract from */
	int nummeas,
	double *black			/* Black to subtract */
);

/* Check for saturation of a raw unscaled multimeas */
/* This should be after black subtraction but before linearization. */
/* Returns nz if saturated */
int i1pro3_multimeas_check_sat(
	i1pro3 *p,
	double *pmaxval,		/* If not NULL, return the maximum value */
	double **rawmmeas,		/* Array of [nummeas][nraw] values to check */
	int nummeas
);

/* Check for consistency of a raw unscaled multimeas */
/* (Don't use on black subtracted black measurement) */
/* Returns nz if inconsistent */
int i1pro3_multimeas_check_raw_consistency(
	i1pro3 *p,
	double **rawmmeas,		/* Array of [nummeas][nraw] values to check */
	int nummeas
);

/* Check for consistency of a raw unscaled multimeas with UV muxing */
/* Returns nz if inconsistent */
int i1pro3_multimeas_check_raw_consistency_x(
	i1pro3 *p,
	double **rawmmeas,		/* Array of [nummeas][nraw] values to check */
	int nummeas
);

/* Check for consistency of wav values. */
/* Returns nz if inconsistent */
int i1pro3_multimeas_check_wav_consistency(
	i1pro3 *p,
	int hr,				/* 0 for std res, 1 for high res */ 
	double inttime,
	double **wav,		/* Array of [nwav1m][nwav] values to check */
	int nummeas
);

/* Check for consistency of a raw unscaled multimeas with UV muxing */
/* Returns nz if inconsistent */
int i1pro3_multimeas_check_raw_consistency_x(
	i1pro3 *p,
	double **rawmmeas,		/* Array of [nummeas][nraw] values to check */
	int nummeas
);

/* Check for consistency of a even/odd wav values. */
/* Returns nz if inconsistent */
int i1pro3_multimeas_check_wav_consistency2(
	i1pro3 *p,
	int hr,				/* 0 for std res, 1 for high res */ 
	double inttime,		/* Integration time */
	double **wav1m,		/* Array of [nwav1m][nwav] values to check */
	int nwav1,
	double **wav2m,		/* Array of [nwav2m][nwav] values to check */
	int nwav2
);

/* Take a set of raw, black subtracted, unscaled values and linearize them. */ 
void i1pro3_vect_lin(
	i1pro3 *p,
	double *raw
);

/* Normalize a set of raw measurements by dividing by integration time */
void i1pro3_normalize_rawmmeas(
	i1pro3 *p,
	double **rawmmeas,		/* raw measurements to correct array [nummeas][nraw] */
	int nummeas,			/* Number of measurements */
	double int_time			/* Integration time */
);

/* Take a set of raw, black subtracted, unscaled multimeas values */
/* and linearize them. */ 
void i1pro3_multimeas_lin(
	i1pro3 *p,
	double **rawmmeas,		/* Array of [nummeas][nraw] values to linearize */
	int nummeas
);

/* Average a raw multimeas into a single vector */
void i1pro3_average_rawmmeas(
	i1pro3 *p,
	double *avg,			/* return average [-1 nraw] */
	double **rawmmeas,		/* Array of [nummeas][-1 nraw] raw values to average */
	int nummeas
);

/* Average two raw multimeas into a single vector */
void i1pro3_average_rawmmeas_2(
	i1pro3 *p,
	double *avg,			/* return average [-1 nraw] */
	double **rawmmeas1,		/* Array of [nummeas][-1 nraw] value to average */
	int nummeas1,
	double **rawmmeas2,		/* Array of [nummeas][-1 nraw] value to average */
	int nummeas2
);

/* Average a raw multimeas into odd & even single vectors */
void i1pro3_average_eorawmmeas(
	i1pro3 *p,
	double avg[2][128],		/* return average [2][nraw] */
	double **rawmmeas,		/* Array of [nummeas][nraw] raw values to average */
	int nummeas				/* (must be even) */
);

/* Average a wav multimeas into a single vector */
void i1pro3_average_wavmmeas(
	i1pro3 *p,
	double *avg,			/* return average [nwav] */
	double **wavmmeas,		/* Array of [nummeas][nwav[]] raw values to average */
	int nummeas,
	int hires				/* 0 if standard res., 1 if hires */
);

/* Convert an absraw array from raw wavelengths to output wavelenths */
/* for a given [std res, high res] and [emis/tras, reflective] mode */
/* (Copies aux data from raw to wav if refl & 2 == 0) */
void i1pro3_absraw_to_abswav(
	i1pro3 *p,
	int hr,					/* 0 for std res, 1 for high res, -1 for no aux copy */ 
	int refl,				/* 0 for emis/trans, 1 for reflective, 2 or 3 for no aux copy */ 
	double **abswav,		/* Desination array [nummeas] [-x nwav[]] */
	double **absraw,		/* Source array [nummeas] [-x nraw] */
	int nummeas				/* Number of measurements */
);

/* Apply stray light correction to a wavmmeas */
void i1pro3_straylight(
	i1pro3 *p,
	int hr,					/* 0 for std res, 1 for high res */ 
	double **wavmmeas,		/* wav measurements to correct array [nummeas] [nwav] */
	int nummeas				/* Number of measurements */
);

/* Take a set of odd/even raw or wav measurements and reshuffle them into */
/* first half block and second half block */
/* return nz if malloc failed */
int i1pro3_unshuffle(
	i1pro3 *p,
	double **mmeas,			/* Array of [nummeas][xx] values to unshuffle */
	int nummeas				/* Must be even */
);

/* Compute i1pro3 calibration LED model given auxiliary information */
/* from the spectral and other sensors. */
void i1pro3_comp_ledm(
	i1pro3 *p,
	double **wavmmeas_ledm,		/* Array of [nummeas][36] values to return */
	double **wavmmeas_aux,		/* Array of [nummeas][-9] aux values to use */
	int nummeas,				/* Number of measurements */
	int uv						/* 0 for non-uv, 1 for uv-illuminated LED model */
);

/* Take a set of odd/even raw measurements, convert to approx. reflectance */
/* and filter out any UV multiplexing noise */
void i1pro3_filter_uvmux(
	i1pro3 *p,
	double **raw,		/* Array of [nummeas][-9 nraw] values to filter */
	int nummeas,		/* Must be even */
	int hr
);

/* return the calibrated wavelength given a raw bin value for the given mode */
static double i1pro3_raw2wav(i1pro3 *p, int refl, double raw);

/* Invert a raw2wavlength table value. */
static double inv_raw2wav(double wl_cal[128], double inv);

/* Process a buffer of raw zebra values. */
/* Return nz if invalid */
static i1pro3_code i1pro3_zebra_proc( 
	i1pro3 *p,
	int ***p_p2m,		/* If valid, return list of sample indexes in even position order */
	int *p_npos,		/* Number of position slots */
	unsigned char *zeb,	/* Array [numzeb] bytes of zebra ruler samples (0,1,2,3) values */
	int numzeb,			/* Number of zebra ruler samples/bytes.*/
	double inttime,		/* Corresponding measurement integration time */
	int nummeas			/* Number of measures */
);

/* Free up a p2m list returned by i1pro3_zebra_proc() */
void del_zebix_list(int **p2m, int npos);

/* ============================================================ */
/* Lower level measurement code */

/* Measurement mode. */
typedef enum {
	/* Construction masks */
	i1p3cm_refl			= 0x10,					/* Reflection mode measurement */
	i1p3cm_ill			= 0x20,					/* Reflective illumination used */ 
	i1p3cm_illref		= i1p3cm_refl | i1p3cm_ill,

	/* Modes */
	i1p3mm_em			= 0x00,			/* Emission mode measurement */
	i1p3mm_em_wl		= 0x01,			/* Emmission mode with wl led on */
	i1p3mm_rf_black		= i1p3cm_refl,	/* Reflection mode with no illumination */
	i1p3mm_rf_wh		= i1p3cm_illref | 0x02,	/* Reflection mode with all LEDs on */
	i1p3mm_rf_whl		= i1p3cm_illref | 0x03,	/* Refl. mode with LEDs but no l_UV */
	i1p3mm_rf_whs		= i1p3cm_illref | 0x04,	/* Refl. mode with LEDs but no s_UV */
	i1p3mm_rf_whp		= i1p3cm_refl | i1p3cm_ill | 0x05	/* Refl. mode with pol UV levels */
} i1p3_mmode;

/* Take a measurement and return floating point raw values. */
/* Converts to completely processed output readings. */
i1pro3_code i1pro3_do_measure(
	i1pro3 *p,
	i1p3_mmode mm,			/* Measurement mode */
	double ***praw,			/* Return array [nummeas][-1, nraw] for emissive, */
							/* Return array [nummeas][-9, nraw] for reflective, */
	int *pnummeas,			/* Number of measurements to make/returned, 0 for scan. */
	double *pinttime, 		/* Integration time to use/used */
	int ***p_p2m,			/* If valid, return zeb list of sample indexes in mm. order */
	int *p_npos				/* Number of position slots */
);

/* Free up the raw matrix returned by i1pro3_do_measure */
void i1pro3_free_raw(
	i1pro3 *p,
	i1p3_mmode mm,			/* Measurement mode */
	double **raw, 			/* Matrix to free */
	int nummeas				/* Number of measurements */
);

/* ============================================================ */
/* Low level i1pro3 commands */

/* USB Command Codes used. */

typedef enum {
	i1p3cc_get_firm_ver     = 0x20,		/* Get Firmware Version */	
	i1p3cc_getparams        = 0x21,		/* Get Parameters */
	i1p3cc_read_EE          = 0x24,		/* Read EEProm/cal data */
	i1p3cc_get_chipid       = 0x26,		/* Get Chip Id */
	i1p3cc_set_led_currents = 0x2D,		/* Set LED Currents */
	i1p3cc_get_adap_type    = 0x38,		/* Get Adapter Type */
	i1p3cc_get_board_temp   = 0x2C,		/* Get Board Temperature */
	i1p3cc_set_scan_ind     = 0x3B,		/* Set Scan Start Indicator Timing */
	i1p3cc_set_tint_mul     = 0x3A,		/* Set Tint Multiplier */
	i1p3cc_meas_emis        = 0x27,		/* Measure Emissive/Passive */
	i1p3cc_meas_refl        = 0x22,		/* Measure Reflective/Active */
	i1p3cc_get_events       = 0x66,		/* Return events on pipe 0x83 */
	i1p3cc_sim_event        = 0x25,		/* Simulate Event */
	i1p3cc_set_ind          = 0x29,		/* Set LED Indicator */
	i1p3cc_get_last_err     = 0x2B		/* Get Last Error */


} i1Pro3CC;

/* USB Commands */

/* Get the firmware version number and/or string */
i1pro3_code
i1pro3_fwver(
	i1pro3 *p,
	int *no,			/* If !NULL, return version * 100 */
	char str[50]		/* If !NULL, return version string */
);

/* Get hw parameters */
/* Return pointers may be NULL if not needed. */
i1pro3_code
i1pro3_getparams(
	i1pro3 *p,
	unsigned int *subclkdiv,	/* Sub clock divider ratio ??? */
	unsigned int *eesize,		/* EE size, but not used/wrong ?? */
	double *intclkp				/* Integration clock period ??? */
);

/* Read from the EEProm */
i1pro3_code
i1pro3_readEEProm(
	struct _i1pro3 *p,
	unsigned char *buf,		/* Where to read it to */
	int addr,				/* Address in EEprom to read from */
	int size				/* Number of bytes to read (max 65535) */
);

/* Get the Chip ID */
i1pro3_code
i1pro3_getchipid(
    i1pro3 *p,
    unsigned char chipid[8]
);

/* Set the illumination LEDs currents. */
/* All values are 0..255 */
i1pro3_code
i1pro3_setledcurrents(
	i1pro3 *p,
	int led_w,		/* 0: White LED current */
	int led_b,		/* 1: Blue LED current */
	int led_luv,	/* 2: Longer wl UV LED current */
	int led_suv,	/* 3: Shorted wl UV LED current */
	int led_gwl		/* 4: Green wl reference LED current */
);

/* Get Adapter Type. This uses the 3 optical sensors. */
typedef enum {
	i1p3_ad_none     = 0,	/* No adapter fitted */
	i1p3_ad_standard = 1,	/* Standard reflective/emissive adapter */
	i1p3_ad_m3       = 2,	/* "M3" polarizing adapter */
	i1p3_ad_ambient  = 5,	/* Ambient adapter = 1 + 4 */
	i1p3_ad_cal      = 5,	/* Calibration tile = 1 + 4 */
	i1p3_ad_m3cal    = 6	/* Calibration tile = 2 + 4 ?? */
} i1p3_adapter;

i1pro3_code
i1pro3_getadaptype(
  i1pro3 *p,
  i1p3_adapter *atype
);

/* Get the board temperature */
/* Used to track need wl calibration change with temperature. */
/* Return pointers may be NULL if not needed. */
i1pro3_code
i1pro3_getboardtemp(
	i1pro3 *p,
	double *btemp		/* Return temperature in degrees C */
);

/* Set the scan start indicator parameters */
i1pro3_code
i1pro3_setscanstartind(
	i1pro3 *p,
	int starttime,	/* Time in 5msec units after measure command to turn Green indicator on */
	int endtime		/* Time in 5msec units after measure command to turn Green indicator off */
					/* endtime must be > starttime or command is ignored */
					/* endtime == 255 keeps green on until measure ends */
);

/* Set the tint multiplier. (use getlasterr) */
/*
	For the emissive command:

	 This multiplies the reflective integration time if >= 2.
	 The value is rounded down to the nearest even value internally. 
	 If == 2 then every odd sample carries the value,
	 if == 4 every fourth sample carries the value, etc.
	 Not useful - change int time instead.

	For the  reflective command:

	 This multiplies the reflective integration time if >= 2.
	 The value is rounded down to the nearest even value internally. 
	 If == 2 the even sample is zero and the x2 int value is delivered in
	 the odd side of each sensor data. If == 4, the x4 integration sample
	 is delivered in the odd side of every second sensor data, etc. 
	 Doesn't affect the UV Led muxing, so we get the average of with & without UV.
	 Is used with polarization filter to improve S/N ratio.
*/

i1pro3_code
i1pro3_settintmult(
	i1pro3 *p,
	int tintm	/* */
);

/* Emission mode flags */
#define I1PRO3_EMF_BLACK	0x00	/* No illumination */
#define I1PRO3_EMF_ZEBRA	0x10	/* Read zebra stripe while measuring */
#define I1PRO3_EMF_WL		0x40	/* Illuminate with green wavelength LED. */
									/* nummeas is ignored, and 1 measurement is always taken. */
#define I1PRO3_EMF_UNK80	0x80	/* Unknown. Flashes white, but before start of integration. */
									/* A small amount of the light is captured. */
									/* Could be used to check LED turn off delay ???? */

/* Reflection mode flags */
/* (Note that other values are aliases) */
/* It's not clear that the CILL version has any effect - there's no difference in the */
/* illumination level or spectra compared to ILL. */
#define I1PRO3_RMF_BLACK	0x00	/* No illumination */
#define I1PRO3_RMF_ILL		0x03	/* Illuminate */
#define I1PRO3_RMF_CILL		0x04	/* Illuminate for Polarized */
#define I1PRO3_RMF_ZEBRA	0x10	/* Read zebra stripe while measuring */

/* Trigger either an "emissive" or "reflective" measurement after the delay in msec. */
/* The actual return code will be in m->trig_rv after the delay. */
/* This allows us to start the measurement read before the trigger, */
/* ensuring that process scheduling latency can't cause the read to fail. */
/* (use getlasterr after gather) */

/*
	If nummeas == 0: Button must be pressed and held before command,
	and scan continues until button is released. If button is not
	pressed when command is issued, one measurement is taken.
	Simulate event can't be substituted for an actual button press :-(

	Emis mode has a variable integration time given as a parameter,
	and returns a stream of measurements. Only the green wl LED can
	be turned on for this mode.

	Data format is 134 samples total:
		Samples 0..5		= 6 x Dummy cell values
		Samples 6..133		= 128 x raw cel values 

	Refl mode integration time canot be set by this instruction,
	and the default is minimum integration time (Aprox. 200 Hz x 2 with muliplexing)
	Data is in the form of even/odd measurements, with the first (even)
	being just the white + blue LEDs, and the second (odd) being all 4 LEDs -
	White + Blue + Long UV + Short UV. This is augmented with information
	to track the LEDs output.

	Data format is 282 samples total, 6 of which are not used:
		Samples 0, 2, 276..281		= 8 x LED tracking values common to both samples
		Samples 6..139				= 134 x (even) emissive values from Wh + Bl
		Samples 142..275			= 134 x (odd) emissive values from Wh + Bl + lUV + sUV 
*/

i1pro3_code
i1pro3_trigger_measure(
	i1pro3 *p,
	int refinst,		/* NZ to trigger a "reflective" measurement */
	int zebra,			/* NZ if zebra ruler read is being used as well */
	int nummeas,		/* Number of measurements to make, 0 for 1/infinite */
	int intclocks,		/* Number of integration clocks (ignored if refinst) */
	int flags,			/* Measurement mode flags. Differs with refinst */
	int delay			/* Delay before triggering command in msec */
);

/* Gather a measurements results. */
/* A buffer full of bytes is returned. */
/* It appears that the read can be pending before triggering though. */
/* Scan reads will also terminate if there is too great a delay beteween each read ? */
static i1pro3_code
i1pro3_gathermeasurement(
	i1pro3 *p,
	int refinst,			/* NZ if we are gathering a measure reflective else emissive */
	int zebra,				/* NZ if zebra ruler read is being used as well */
	int scanflag,			/* NZ if in scan mode to continue reading */
	int xmeas,				/* Expected number of measurements, ignored if scanflag */
	unsigned char *buffer,	/* Where to read it to */
	int bsize,				/* Bytes available in buffer */
	int *nummeas			/* Return number of readings measured */
);

/* Gather a zebra stripe results. */
/* A buffer full of bytes is returned. */
/* This is called in a thread. */
/* This is assumed to be in scan mode */
static i1pro3_code
i1pro3_gatherzebra(
	i1pro3 *p,
	unsigned char *buf,		/* Where to read it to */
	int bsize,				/* Bytes available in buffer */
	int *bread				/* Return number of bytes read */
);

/* i1Pro3 event values, returned by event pipe, or */
/* parameter to simulate event */
typedef enum {
	i1pro3_eve_none           = 0x00,	/* No event */
	i1pro3_eve_switch_press   = 0x01,	/* Button has been pressed */
	i1pro3_eve_switch_release = 0x02,	/* Button has been released */
	i1pro3_eve_adapt_change   = 0x03	/* Adapaptor/calibration tile changed */
} i1pro3_eve;


/* Simulate an event after given delay */
i1pro3_code i1pro3_simulate_event(i1pro3 *p, i1pro3_eve ecode, int delay);


/* Wait for a reply triggered by an instrument event press */
i1pro3_code i1pro3_waitfor_event(i1pro3 *p, i1pro3_eve *ecode, double top);

/* Wait for a reply triggered by an instrument event (thread version) */
/* Returns I1PRO3_OK if the switch has been pressed or some other event such */
/* as an adapter type change, or I1PRO3_INT_BUTTONTIMEOUT if */
/* no event has occured before the time expired, */
/* or some other error. */
i1pro3_code i1pro3_waitfor_event_th(i1pro3 *p, i1pro3_eve *ecode, double top);

/* Terminate event handling by cancelling the thread i/o */
i1pro3_code i1pro3_terminate_event(i1pro3 *p);


/* Get last instrument HW error code */
/* Return pointers may be NULL if not needed. */
i1pro3_code i1pro3_getlasterr(i1pro3 *p, unsigned int *errc);

/* Send a raw indicator LED sequence. */
static int i1pro3_indLEDseq(void *pp, unsigned char *buf, int size);

/* Turn indicator LEDs off */
static int i1pro3_indLEDoff(void *pp);


/* -------------------------------------------------- */
/* EEProm parsing support. */

/* The EEProm layout is hard coded rather than having an i1Pro1/2 type directory structure, */
/* although the key/value store is still used within the OEM driver code. */

/* Initialise the calibration from the EEProm contents. */
i1pro3_code i1pro3_parse_eeprom(i1pro3 *p, unsigned char *buf, unsigned int len);

struct _i1data3 {
  /* private: */
	i1pro3 *p;

	a1log *log;
	unsigned char *buf;		/* Buffer to parse */
	int len;				/* Length of buffer */

	ORD32 crc;				/* crc-32-c of read key values */
	ORD32 chsum;			/* checksum of read key values */
	
  /* public: */

	/* In all these functions if rv is supplied then it is assumed */
	/* to be large enough to hold the data, or if it is NULL */
	/* the returned value will have been allocated. */
	/* if c is nz, combine data into the current crc */
	/* Return NULL if out of range of the read EE data. */

	/* Return a pointer to an array of chars containing data from 8 bits. */
	unsigned char *(*get_8_char)(struct _i1data3 *d, unsigned char *rv, int offset, int count, int c);

	/* Return a pointer to an nul terminated string containing data from 8 bits. */
	/* An extra space and a nul terminator will be added to the eeprom data */
	char *(*get_8_asciiz)(struct _i1data3 *d, char *rv, int offset, int count, int c);


	/* Return a pointer to an array of ints containing data from 8 bits. */
	/* the rv will be returned. Return NULL if out of range. */
	int *(*get_8_ints)(struct _i1data3 *d, int *rv, int offset, int count, int c);

	/* Return a pointer to an array of ints containing data from unsigned 8 bits. */
	/* the rv will be returned. Return NULL if out of range. */
	int *(*get_u8_ints)(struct _i1data3 *d, int *rv, int offset, int count, int c);


	/* Return a pointer to an array of ints containing data from 16 bits. */
	int *(*get_16_ints)(struct _i1data3 *d, int *rv, int offset, int count, int c);

	/* Return a pointer to an array of ints containing data from unsigned 16 bits. */
	int *(*get_u16_ints)(struct _i1data3 *d, int *rv, int offset, int count, int c);


	/* Return a pointer to an array of ints containing data from 32 bits. */
	int *(*get_32_ints)(struct _i1data3 *d, int *rv, int offset, int count, int c);

	/* Return a pointer to an array of unsigned ints containing data from 32 bits. */
	unsigned int *(*get_u32_uints)(struct _i1data3 *d, unsigned int *rv, int offset, int count, int c);


	/* Return a pointer to an array of doubles containing data from 32 bits. */
	double *(*get_32_doubles)(struct _i1data3 *d, double *rv, int offset, int count, int c);

	/* Return a pointer to an array of doubles containing data from 32 bits, */
	/* with the array filled in reverse order. */
	double *(*rget_32_doubles)(struct _i1data3 *d, double *rv, int offset, int count, int c);

	/* Return a pointer to an array of doubles containing data from 32 bits with zero padding */
	double *(*get_32_doubles_padded)(struct _i1data3 *d, double *rv, int offset, int count, int retcount, int c);

	/* Return a pointer to an array of doubles containing data from u8 bits/255.0. */
	double *(*get_u8_doubles)(struct _i1data3 *d, double *rv, int offset, int count, int c);

	/* Init the crc value */
	void (*init_crc)(struct _i1data3 *d);

	/* Return the crc value */
	ORD32 (*get_crc)(struct _i1data3 *d);

	/* Return the checksum value */
	ORD32 (*get_chsum)(struct _i1data3 *d);

	/* Destroy ourselves */
	void (*del)(struct _i1data3 *d);

}; typedef struct _i1data3 i1data3;

/* Constructor. Construct from the EEprom calibration contents */
extern i1data3 *new_i1data3(i1pro3 *p, unsigned char *buf, int len);

#ifdef __cplusplus
	}
#endif

#define I1PRO3_IMP
#endif /* I1PRO3_IMP */
