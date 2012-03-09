#ifndef I1PRO_IMP_H

/* 
 * Argyll Color Correction System
 *
 * Gretag i1Pro implementation defines
 *
 * Author: Graeme W. Gill
 * Date:   20/12/2006
 *
 * Copyright 2006 - 2007 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
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

/* Implementation resources for i1pro driver */

/* -------------------------------------------------- */
/* Implementation class */

typedef int i1pro_code;		/* Type to use for error codes */

/* I1PRO mode state. This is implementation data that */
/* depends on the mode the instrument is in. */
/* Each mode has a separate calibration, and configured instrument state. */

typedef enum {
	i1p_refl_spot      = 0,
	i1p_refl_scan      = 1,
	i1p_disp_spot      = 2,
	i1p_emiss_spot     = 3,
	i1p_emiss_scan     = 4,
	i1p_amb_spot       = 5,
	i1p_amb_flash      = 6,
	i1p_trans_spot     = 7,
	i1p_trans_scan     = 8,
	i1p_no_modes       = 9		/* Number of modes */
} i1p_mode;

struct _i1pro_state {
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

	/* Configuration & state information */
	double targoscale;	/* Optimal reading scale factor <= 1.0 */
						/* Would determine scan sample rate, except we're not doing it that way! */ 
	int gainmode;		/* Gain mode, 0 = normal, 1 = high */
	double inttime;		/* Integration time */
	double lamptime;	/* Lamp turn on time */

	double dadaptime;	/* Target adaptive dark read time - sets number of readings */
	double wadaptime;	/* Target adaptive white/sample read time - sets number of readings */

	double dcaltime;	/* Target dark calibration time - sets number of readings */
	double wcaltime;	/* Target white calibration time - sets number of readings */

	double dreadtime;	/* Target dark on-the-fly cal time - sets number of readings */
	double wreadtime;	/* Target white/sample reading time - sets number of readings */

	double maxscantime;	/* Maximum scan time sets buffer size allocated */

	double min_wl;		/* Minimum wavelegth to report for this mode */

	/* calibration information for this mode */
	int dark_valid;			/* dark calibration factor valid */
	time_t ddate;			/* Date/time of last dark calibration */
	double dark_int_time;	/* Integration time used for dark data */
	double *dark_data;		/* [nraw] of dark level to subtract. Note that the dark value */
							/* depends on integration time. */
	int dark_gain_mode;		/* Gain mode used for dark data */

	int cal_valid;			/* calibration factor valid */
	time_t cfdate;			/* Date/time of last cal factor calibration */
	double *cal_factor;		/* [nwav] of calibration scale factor for this mode */
	double *white_data;		/* [nraw] linear absolute dark subtracted white data */
							/*        used to compute cal_factor */
	double *cal_factor1, *cal_factor2;	/* (Underlying tables for two resolutions) */

	/* Adaptive emission/transparency black data */
	int idark_valid;		/* idark calibration factors valid */
	time_t iddate;			/* Date/time of last dark idark calibration */
	double idark_int_time[4];
	double **idark_data;	/* [4][nraw] of dark level for inttime/gains of : */
							/* 0.01 norm, 1.0 norm, 0.01 high, 1.0 high */

	int need_calib;			/* White calibration needed anyway */
	int need_dcalib;		/* Dark Calibration needed anyway */

	/* Display mode calibration state (emmis && !scan && !adaptive) */
	int    dispswap;		/* 0 = default time, 1 = dark_int_time2, 2 = dark_int_time3 */
	double done_dintcal;	/* A display integration time cal has been done */
	double dcaltime2;		/* Target dark calibration time - sets number of readings */
	double dark_int_time2;	/* Integration time used for dark data 2 */
	double *dark_data2;		/* [nraw] of dark level to subtract for dark_int_time2. */
	double dcaltime3;		/* Target dark calibration time - sets number of readings */
	double dark_int_time3;	/* Integration time used for dark data 3 */
	double *dark_data3;		/* [nraw] of dark level to subtract for dark_int_time3. */

}; typedef struct _i1pro_state i1pro_state;
 


/* I1PRO implementation class */
struct _i1proimp {
	i1pro *p;

	struct _i1data *data;		/* EEProm data container */
	athread *th;				/* Switch monitoring thread (NULL if not used) */
	volatile int switch_count;	/* Incremented in thread */
	void *hcancel;				/* Handle to cancel the outstanding I/O */
	volatile int th_term;		/* Terminate thread on next return */
	volatile int th_termed;		/* Thread has terminated */
	inst_opt_mode trig;			/* Reading trigger mode */
	int trig_return;			/* Emit "\n" after trigger */
	int noautocalib;			/* Disable automatic calibration if not essential */
	int highres;				/* High resolution mode */
	int hr_inited;				/* High resolution has been initialized */

	/* Current settings */
	i1p_mode mmode;					/* Current measurement mode selected */
	i1pro_state ms[i1p_no_modes];	/* Mode state */
	int spec_en;				/* Enable reporting of spectral data */

	double intclkp;				/* Integration clock period */
	int subclkdiv;				/* Sub clock divider ratio */
	int subtmode;				/* Reading 127 subtract mode (version 301 or greater) */

	/* Current state of hardware, to avoid uncessary operations */
	double c_inttime;			/* Integration time */
	double l_inttime;			/* Last Integration time (for Rev A+/B quirk fix) */
	double c_lamptime;			/* Lamp turn on time */
	int c_mcmode;				/* special clock mode we're in (if rev >= 301) */
	int c_intclocks;			/* Number of integration clocks (set using setmeasparams() */
	int c_lampclocks;			/* Number of integration clocks (set using setmeasparams() */
	int c_nummeas;				/* Number of measurements (set using setmeasparams() */
	int c_measmodeflags;		/* Measurement mode flags (set using setmeasparams() */
	unsigned int slamponoff;	/* The second last time the lamp was switched from on to off */
	unsigned int llampoffon;	/* The last time the lamp was switched from off to on, in msec */
	unsigned int llamponoff;	/* The last time the lamp was switched from on to off, in msec */


	/* Values read from GetMisc() */
	int fwrev;					/* int - Firmware revision number, from getmisc() */
								/* Used for internal switching ?? */
								/* 101 = Rev A, 202 = Rev A update, 302 = Rev B, 502 = Rev D */

	int cpldrev;				/* int - CPLD revision number in EEProm */
								/* Not used internaly ???? */
								/* 101 = Rev A, 2 = Rev A update, 301 = Rev B, 999 = Rev D */

	int maxpve;					/* Maximum +ve value of Sensor Data + 1 */
	int powmode;				/* Power mode status, 0 = high, 8 = low */

	/* Values read from GetMeasureParameters() - are these needed ? */
	int r_intclocks;			/* Number of integration clocks (read from instrument) */
	int r_lampclocks;			/* Number of lamp turn on sub-clocks (read from instrument) */
	int r_nummeas;				/* Number of measurements (read from instrument) */
	int r_measmodeflags;		/* Measurement mode flags (read from instrument) */


	/* Information about the instrument from the EEprom */
	int serno;				/* serial number */
	char sserno[14];		/* serial number as string */
	int dom;				/* Date of manufacture DDMMYYYY ? */
	int capabilities;		/* Capabilities flag */
							/* Ambient capability if val & 0x6000 != 0 */
	int physfilt;			/* int - physical filter */
							/* 0x80 == no filter, 0x82 == UV filter */

	/* Underlying calibration information */
	int nraw;				/* Raw sample bands stored = 128 */
	int nwav;				/* Current cooked spectrum bands stored, usually = 36 */
	double wl_short;		/* Cooked spectrum bands short wavelength, usually 380 */
	double wl_long;			/* Cooked spectrum bands short wavelength, usually 730 */

	unsigned int nwav1, nwav2;	/* Available bands for standard and high-res modes */
	double wl_short1, wl_short2, wl_long1, wl_long2;

	unsigned int nlin0;		/* Number in array */
	double *lin0;			/* Array of linearisation polinomial factors, normal gain. */

	unsigned int nlin1;		/* Number in array */
	double *lin1;			/* Array of linearisation polinomial factors, high gain. */
		
	double min_int_time;	/* Minimum integration time (secs) */
	double max_int_time;	/* Maximum integration time (secs) */
	int *mtx_index;			/* [nwav] Matrix CCD sample starting index for each out wavelength */
	int *mtx_nocoef; 		/* [nwav] Number of matrix cooeficients for each out wavelength */
	double *mtx_coef;		/* [nwav * mtx_nocoef] Matrix cooeficients to compute each wavelength */

	int *mtx_index1, *mtx_index2;		/* Underlying arrays for the two resolutions */
	int *mtx_nocoef1, *mtx_nocoef2;		/* first [nwav1], second [nwav2] */
	double *mtx_coef1, *mtx_coef2;

	double *white_ref;		/* [nwav] White calibration tile reflectance values */
	double *emis_coef;		/* [nwav] Emission calibration coefficients */
	double *amb_coef;		/* [nwav] Ambient light cal values (compound with Emission) */
							/* NULL if ambient not supported */
	double *white_ref1, *white_ref2;	/* Underlying tables */
	double *emis_coef1, *emis_coef2;
	double *amb_coef1, *amb_coef2;
							/* NULL if ambient not supported */


	double highgain;		/* High gain mode gain */
	double scan_toll_ratio;	/* Modifier of scan tollerance */

	int sens_target;		/* sensor optimal target value */
	int sens_dark;			/* sensor dark reference threshold */
	int sens_sat0;			/* Normal gain sensor saturated threshold */
	int sens_sat1;			/* High gain sensor saturated threshold */

	/* log variables */
	int meascount;			/* Total Measure (Emis/Remis/Ambient/Trans/Cal) count */
							/* but not the pre-Remission dark calibration. */
	time_t caldate;			/* Remspotcal last calibration date */
	int calcount;			/* Remission spot measure count at last Remspotcal. */
	double rpinttime;		/* Last remision spot reading integration time */
	int rpcount;			/* Remission spot measure count */
	int acount;				/* Remission scan measure count (Or all scan ??) */
	double lampage;			/* Total lamp usage time in seconds (??) */

	/* Trigger houskeeping & diagnostics */
	int msec;				/* msec_time() at creation */
	athread *trig_thread;	/* Delayed trigger thread */
	int trig_delay;			/* Trigger delay in msec */
	int tr_t1, tr_t2, tr_t3, tr_t4, tr_t5, tr_t6, tr_t7;	/* Trigger/read timing diagnostics */
							/* 1->2 = time to execute trigger */
							/* 2->3 = time to between end trigger and start of first read */
							/* 3->4 = time to exectute first read */
							/* 6->5 = time between end of second last read and start of last read */
	int trig_se;			/* Delayed trigger icoms error */
	i1pro_code trig_rv;		/* Delayed trigger result */

}; typedef struct _i1proimp i1proimp;

/* Add an implementation structure */
i1pro_code add_i1proimp(i1pro *p);

/* Destroy implementation structure */
void del_i1proimp(i1pro *p);

/* ============================================================ */
/* Error codes returned from i1pro_imp */

/* Note: update i1pro_interp_error() and i1pro_interp_code() in i1pro.c */
/* if anything of these #defines are added or subtracted */

/* Fake Error codes */
#define I1PRO_INTERNAL_ERROR			0x71		/* Internal software error */
#define I1PRO_COMS_FAIL					0x72		/* Communication failure */
#define I1PRO_UNKNOWN_MODEL				0x73		/* Not an i1pro */
#define I1PRO_DATA_PARSE_ERROR  		0x74		/* Read data parsing error */
#define I1PRO_USER_ABORT			    0x75		/* User hit abort */
#define I1PRO_USER_TERM		    		0x76		/* User hit terminate */
#define I1PRO_USER_TRIG 			    0x77		/* User hit trigger */
#define I1PRO_USER_CMND		    		0x78		/* User hit command */

#define I1PRO_UNSUPPORTED		   		0x79		/* Unsupported function */
#define I1PRO_CAL_SETUP                 0x7A		/* Cal. retry with correct setup is needed */

/* Real error code */
#define I1PRO_OK   						0x00

#define I1PRO_DATA_COUNT			    0x01		/* count unexpectedly small */
#define I1PRO_DATA_BUFSIZE			    0x02		/* buffer too small */
#define I1PRO_DATA_MAKE_KEY				0x03		/* creating key failed */
#define I1PRO_DATA_MEMORY 				0x04		/* memory alloc failure */
#define I1PRO_DATA_KEYNOTFOUND			0x05		/* a key value wasn't found */
#define I1PRO_DATA_WRONGTYPE			0x06		/* a key is the wrong type */
#define I1PRO_DATA_KEY_CORRUPT		    0x07		/* key table seems to be corrupted */
#define I1PRO_DATA_KEY_COUNT		    0x08		/* key table count is too big or small */
#define I1PRO_DATA_KEY_UNKNOWN		    0x09		/* unknown key type */
#define I1PRO_DATA_KEY_MEMRANGE		    0x0a		/* key data is out of range of EEProm */
#define I1PRO_DATA_KEY_ENDMARK		    0x0b		/* And end section marker was missing */

/* HW errors */
#define I1PRO_HW_HIGHPOWERFAIL			0x20		/* Switch to high power mode failed */
#define I1PRO_HW_EE_SHORTREAD		    0x21		/* Read fewer EEProm bytes than expected */
#define I1PRO_HW_ME_SHORTREAD		    0x22		/* Read measurement bytes than expected */
#define I1PRO_HW_ME_ODDREAD			    0x23		/* Read measurement bytes was not mult 256 */
#define I1PRO_HW_CALIBINFO			    0x24		/* calibration info is missing or corrupted */

/* Sample read operation errors */
#define I1PRO_RD_DARKREADINCONS		    0x30		/* Dark calibration reading inconsistent */
#define I1PRO_RD_SENSORSATURATED	    0x31		/* Sensor is saturated */
#define I1PRO_RD_DARKNOTVALID   	    0x32		/* Dark reading is not valid (too light) */
#define I1PRO_RD_NEEDS_CAL 		        0x33		/* Mode needs calibration */
#define I1PRO_RD_WHITEREADINCONS        0x34		/* White reference readings are inconsistent */
#define I1PRO_RD_WHITEREFERROR 	        0x35		/* White reference reading error */
#define I1PRO_RD_LIGHTTOOLOW 	        0x36		/* Light level is too low */
#define I1PRO_RD_LIGHTTOOHIGH 	        0x37		/* Light level is too high */
#define I1PRO_RD_SHORTMEAS              0x38		/* Measurment was too short */
#define I1PRO_RD_READINCONS             0x39		/* Reading is inconsistent */
#define I1PRO_RD_TRANSWHITERANGE        0x3A		/* Transmission white reference is out of range */
#define I1PRO_RD_NOTENOUGHPATCHES       0x3B		/* Not enough patches */
#define I1PRO_RD_TOOMANYPATCHES         0x3C		/* Too many patches */
#define I1PRO_RD_NOTENOUGHSAMPLES       0x3D		/* Not enough samples per patch */
#define I1PRO_RD_NOFLASHES              0x3E		/* No flashes recognized */
#define I1PRO_RD_NOAMBB4FLASHES         0x3F		/* No ambient before flashes found */

/* Internal errors */
#define I1PRO_INT_NO_COMS 		        0x40
#define I1PRO_INT_EETOOBIG 		        0x41		/* EEProm read size is too big */
#define I1PRO_INT_ODDREADBUF 	        0x42		/* Measurment read buffer is not mult 256 */
#define I1PRO_INT_SMALLREADBUF 	        0x43		/* Measurment read buffer too small */
#define I1PRO_INT_INTTOOBIG				0x45		/* Integration time is too big */
#define I1PRO_INT_INTTOOSMALL			0x46		/* Integration time is too small */
#define I1PRO_INT_ILLEGALMODE			0x47		/* Illegal measurement mode selected */
#define I1PRO_INT_WRONGMODE  			0x48		/* In wrong mode for request */
#define I1PRO_INT_ZEROMEASURES 			0x49		/* Number of measurements requested is zero */
#define I1PRO_INT_WRONGPATCHES 			0x4A		/* Number of patches to match is wrong */
#define I1PRO_INT_MEASBUFFTOOSMALL 		0x4B		/* Measurement read buffer is too small */
#define I1PRO_INT_NOTIMPLEMENTED 		0x4C		/* Support not implemented */
#define I1PRO_INT_NOTCALIBRATED 		0x4D		/* Unexpectedely invalid calibration */
#define I1PRO_INT_NOINTERPDARK 		    0x4E		/* Need interpolated dark and don't have it */
#define I1PRO_INT_THREADFAILED 		    0x4F		/* Creation of thread failed */
#define I1PRO_INT_BUTTONTIMEOUT 	    0x50		/* Switch status read timed out */
#define I1PRO_INT_CIECONVFAIL 	        0x51		/* Creating spectral to CIE converted failed */
#define I1PRO_INT_PREP_LOG_DATA         0x52		/* Error in preparing log data */
#define I1PRO_INT_MALLOC                0x53		/* Error in mallocing memory */
#define I1PRO_INT_CREATE_EEPROM_STORE   0x54		/* Error in creating EEProm store */
#define I1PRO_INT_SAVE_SUBT_MODE        0x55		/* Can't save calibration if in subt mode */
#define I1PRO_INT_NO_CAL_TO_SAVE        0x56		/* No calibration data to save */
#define I1PRO_INT_EEPROM_DATA_MISSING   0x57		/* EEProm data is missing */
#define I1PRO_INT_NEW_RSPL_FAILED       0x58		/* Creating RSPL object faild */
#define I1PRO_INT_CAL_SAVE              0x59		/* Unable to save calibration to file */
#define I1PRO_INT_CAL_RESTORE           0x60		/* Unable to restore calibration from file */
#define I1PRO_INT_ADARK_INVALID         0x61		/* Adaptive dark calibration is invalid */

/* ============================================================ */
/* High level implementatation */

/* Initialise our software state from the hardware */
i1pro_code i1pro_imp_init(i1pro *p);

/* Return a pointer to the serial number */
char *i1pro_imp_get_serial_no(i1pro *p);

/* Return non-zero if capable of ambient mode */
int i1pro_imp_ambient(i1pro *p);

/* Set the measurement mode. It may need calibrating */
i1pro_code i1pro_imp_set_mode(
	i1pro *p,
	i1p_mode mmode,		/* i1pro mode to use */
	int spec_en);		/* nz to enable reporting spectral */

/* Determine if a calibration is needed. */
inst_cal_type i1pro_imp_needs_calibration(i1pro *p);

/* Calibrate for the current mode. */
/* Request an instrument calibration of the current mode. */
i1pro_code i1pro_imp_calibrate(
i1pro *p,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[100]			/* Condition identifier (ie. white reference ID) */
);

/* Measure a patch or strip in the current mode. */
i1pro_code i1pro_imp_measure(
	i1pro *p,
	ipatch *val,		/* Pointer to array of instrument patch value */
	int nvals			/* Number of values */	
);

/* return nz if high res is supported */
int i1pro_imp_highres(i1pro *p);

/* Set to high resolution mode */
i1pro_code i1pro_set_highres(i1pro *p);

/* Set to standard resolution mode */
i1pro_code i1pro_set_stdres(i1pro *p);

/* Modify the scan consistency tollerance */
i1pro_code i1pro_set_scan_toll(i1pro *p, double toll_ratio);


/* Update the single remission calibration and instrument usage log */
i1pro_code i1pro_update_log(i1pro *p);

/* Save the reflective spot calibration information to the EEPRom data object. */
/* Note we don't actually write to the EEProm here! */
static i1pro_code i1pro_set_log_data(i1pro *p);

/* Restore the reflective spot calibration information from the EEPRom */
/* Always returns success, even if the restore fails */
i1pro_code i1pro_restore_refspot_cal(i1pro *p);


/* Save the calibration for all modes, stored on local system */
i1pro_code i1pro_save_calibration(i1pro *p);

/* Restore the all modes calibration from the local system */
i1pro_code i1pro_restore_calibration(i1pro *p);

/* ============================================================ */
/* Intermediate routines  - composite commands/processing */

i1pro_code i1pro_establish_high_power(i1pro *p);

/* Take a dark reference measurement - part 1 */
i1pro_code i1pro_dark_measure_1(
	i1pro *p,
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	unsigned char *buf,		/* USB reading buffer to use */
	unsigned int bsize		/* Size of buffer */
);

/* Take a dark reference measurement - part 2 */
i1pro_code i1pro_dark_measure_2(
	i1pro *p,
	double *abssens,		/* Return array [nraw] of abssens values */
	int nummeas,			/* Number of readings to take */
	double inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	unsigned char *buf,		/* raw USB reading buffer to process */
	unsigned int bsize		/* Buffer size to process */
);

/* Take a dark measurement */
i1pro_code i1pro_dark_measure(
	i1pro *p,
	double *abssens,		/* Return array [nraw] of abssens values */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode			/* Gain mode to use, 0 = normal, 1 = high */
);

/* Take a white reference measurement - part 3 */
/* Average, check, and convert to output wavelengths */
i1pro_code i1pro_whitemeasure_3(
	i1pro *p,
	double *abswav1,		/* Return array [nwav1] of abswav values (may be NULL) */
	double *abswav2,		/* Return array [nwav2] of abswav values (if hr_init, may be NULL) */
	double *absraw,			/* Return array [nraw] of absraw values */
	double *optscale,		/* Factor to scale gain/int time by to make optimal (may be NULL) */
	int nummeas,			/* Number of readings to take */
	double inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	double targoscale,		/* Optimal reading scale factor */
	double **multimes		/* Multiple measurement results */
);

/* Take a white reference measurement */
/* (Subtracts black and processes into wavelenths) */
i1pro_code i1pro_whitemeasure(
	i1pro *p,
	double *abswav1,		/* Return array [nwav1] of abswav values (may be NULL) */
	double *abswav2,		/* Return array [nwav2] of abswav values (if hr_init, may be NULL) */
	double *absraw,			/* Return array [nraw] of absraw values */
	double *optscale,		/* Factor to scale gain/int time by to make optimal (may be NULL) */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	double targoscale,		/* Optimal reading scale factor */
	int ltocmode			/* 1 = Lamp turn on compensation mode */ 
);

/* Process a single raw white reference measurement */
/* (Subtracts black and processes into wavelenths) */
i1pro_code i1pro_whitemeasure_buf(
	i1pro *p,
	double *abswav1,		/* Return array [nwav1] of abswav values (may be NULL) */
	double *abswav2,		/* Return array [nwav2] of abswav values (if hr_init, may be NULL) */
	double *absraw,			/* Return array [nraw] of absraw values */
	double inttime, 		/* Integration time to used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	unsigned char *buf		/* Raw buffer */
);

/* Take a measurement reading using the current mode, part 1 */
/* Converts to completely processed output readings. */
i1pro_code i1pro_read_patches_1(
	i1pro *p,
	int minnummeas,			/* Minimum number of measurements to take */
	int maxnummeas,			/* Maximum number of measurements to allow for */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	int *nmeasuered,		/* Number actually measured */
	unsigned char *buf,		/* Raw USB reading buffer */
	unsigned int bsize
);

/* Take a measurement reading using the current mode, part 2 */
/* Converts to completely processed output readings. */
i1pro_code i1pro_read_patches_2(
	i1pro *p,
	double *duration,		/* return flash duration (secs) */
	double **specrd,		/* Return array [numpatches][nwav] of spectral reading values */
	int numpatches,			/* Number of patches to return */
	double inttime, 		/* Integration time to used */
	int gainmode,			/* Gain mode useed, 0 = normal, 1 = high */
	int nmeasuered,			/* Number actually measured */
	unsigned char *buf,		/* Raw USB reading buffer */
	unsigned int bsize
);

/* Take a measurement reading using the current mode. */
/* Converts to completely processed output readings. */
i1pro_code i1pro_read_patches(
	i1pro *p,
	double *duration,		/* Return flash duration */
	double **specrd,		/* Return array [numpatches][nwav] of spectral reading values */
	int numpatches,			/* Number of patches to return */
	int minnummeas,			/* Minimum number of measurements to take */
	int maxnummeas,			/* Maximum number of measurements to allow for */
	double *inttime, 		/* Integration time to use/used */
	int gainmode			/* Gain mode to use, 0 = normal, 1 = high */
);

/* Take a trial measurement reading using the current mode. */
/* Used to determine if sensor is saturated, or not optimal */
i1pro_code i1pro_trialmeasure(
	i1pro *p,
	int *saturated,			/* Return nz if sensor is saturated */
	double *optscale,		/* Factor to scale gain/int time by to make optimal */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	double targoscale		/* Optimal reading scale factor */
);

/* Trigger a single measurement cycle. This could be a dark calibration, */
/* a calibration, or a real measurement. Used to create the higher */
/* level "calibrate" and "take reading" functions. */
/* The setup for the operation is in the current mode state. */
/* The called then needs to call i1pro_readmeasurement() */
i1pro_code
i1pro_trigger_one_measure(
	i1pro *p,
	int nummeas,			/* Number of measurements to make */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	int calib_measure,		/* flag - nz if this is a calibration measurement */
	int dark_measure		/* flag - nz if this is a dark measurement */
);

/* ============================================================ */
/* lower level reading processing */

/* Take a buffer full of raw readings, and convert them to */
/* absolute linearised sensor values. */
void i1pro_meas_to_abssens(
	i1pro *p,
	double **abssens,		/* Array of [nummeas][nraw] value to return */
	unsigned char *buf,		/* Raw measurement data must be 256 * nummeas */
	int nummeas,			/* Return number of readings measured */
	double inttime, 		/* Integration time used */
	int gainmode			/* Gain mode, 0 = normal, 1 = high */
);

/* Take a raw sensor value, and convert it into an absolute sensor value */
double i1pro_raw_to_abssens(
	i1pro *p,
	double raw,				/* Input value */
	double inttime, 		/* Integration time used */
	int gainmode			/* Gain mode, 0 = normal, 1 = high */
);

/* Take a single set of absolute linearised sensor values and */
/* convert them back into raw reading values. */
i1pro_code i1pro_abssens_to_meas(
	i1pro *p,
	int meas[128],			/* Return raw measurement data */
	double abssens[128],	/* Array of [nraw] value to process */
	double inttime, 		/* Integration time used */
	int gainmode			/* Gain mode, 0 = normal, 1 = high */
);


/* Average a set of measurements into one. */
/* Return zero if readings are consistent and not saturated. */
/* Return nz with bit 1 set if the readings are not consistent */
/* Return nz with bit 2 set if the readings are saturated */
/* Return the highest individual element. */
/* Return the overall average. */
int i1pro_average_multimeas(
	i1pro *p,
	double *avg,			/* return average [nraw] */
	double **multimeas,		/* Array of [nummeas][nraw] value to average */
	int nummeas,			/* Return number of readings measured */
	double *phighest,		/* If not NULL, return highest value from all bands and msrmts. */
	double *poallavg,		/* If not NULL, return overall average of bands and measurements */
	double satthresh,		/* Sauration threshold, 0 for none */
	double darkthresh		/* Dark threshold (used for consistency check scaling) */
);

/* Recognise the required number of ref/trans patch locations, */
/* and average the measurements within each patch. */
/* Return flags zero if readings are consistent and not saturated. */
/* Return flags nz with bit 1 set if the readings are not consistent */
/* Return flags nz with bit 2 set if the readings are saturated */
/* Return the highest individual element. */
i1pro_code i1pro_extract_patches_multimeas(
	i1pro *p,
	int *flags,             /* return flags */
	double **pavg,			/* return patch average [naptch][nraw] */
	int npatch,				/* number of patches to recognise */
	double **multimeas,		/* Array of [nummeas][nraw] value to extract from */
	int nummeas,			/* number of readings to recognise them from */
	double *phighest,		/* If not NULL, return highest value from all bands and msrmts. */
	double satthresh,		/* Sauration threshold, 0 for none */
	double inttime			/* Integration time (used to adjust consistency threshold) */
);

/* Recognise any flashes in the readings, and */
/* and average their values together as well as summing their duration. */
/* Return nz on an error */
i1pro_code i1pro_extract_patches_flash(
	i1pro *p,
	int *flags,				/* return flags */
	double *duration,		/* return duration */
	double *pavg,			/* return patch average [nraw] */
	double **multimeas,		/* Array of [nummeas][nraw] value to extract from */
	int nummeas,			/* number of readings made */
	double inttime			/* Integration time (used to compute duration) */
);

/* Subtract one abssens array from another */
void i1pro_sub_abssens(
	i1pro *p,
	int nummeas,			/* Return number of readings measured */
	double **abssens,		/* Source/Desination array [nraw] */
	double *sub				/* Value to subtract [nraw] */
);

/* Convert an abssens array from raw wavelengths to output wavelenths */
/* for the current resolution */
void i1pro_abssens_to_abswav(
	i1pro *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav] */
	double **abssens		/* Source array [nraw] */
);

/* Convert an abssens array from raw wavelengths to output wavelenths */
/* for the standard resolution */
void i1pro_abssens_to_abswav1(
	i1pro *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav1] */
	double **abssens		/* Source array [nraw] */
);

/* Convert an abssens array from raw wavelengths to output wavelenths */
/* for the high resolution */
void i1pro_abssens_to_abswav2(
	i1pro *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav2] */
	double **abssens		/* Source array [nraw] */
);

/* Convert an abswav array of output wavelengths to scaled output readings. */
void i1pro_scale_specrd(
	i1pro *p,
	double **outspecrd,		/* Destination */
	int numpatches,			/* Number of readings/patches */
	double **inspecrd		/* Source */
);

/* Convert from spectral to XYZ, and transfer to the ipatch array */
i1pro_code i1pro_conv2XYZ(
	i1pro *p,
	ipatch *vals,		/* Values to return */
	int nvals,			/* Number of values */
	double **specrd		/* Spectral readings */
);

/* Check a reflective white measurement, and check that */
/* it seems reasonable. Return inst_ok if it is, error if not. */
i1pro_code i1pro_check_white_reference1(
	i1pro *p,
	double *abswav			/* Measurement to check */
);

/* Compute a calibration factor given the reading of the white reference. */
/* Return nz if any of the transmission wavelengths are low */
int i1pro_compute_white_cal(
	i1pro *p,
	double *cal_factor1,	/* [nwav1] Calibration factor to compute */
	double *white_ref1,		/* [nwav1] White reference to aim for, NULL for 1.0 */
	double *white_read1,	/* [nwav1] The white that was read */
	double *cal_factor2,	/* [nwav2] Calibration factor to compute */
	double *white_ref2,		/* [nwav2] White reference to aim for, NULL for 1.0 */
	double *white_read2		/* [nwav2] The white that was read */
);

/* For adaptive mode, compute a new integration time and gain mode */
/* in order to optimise the sensor values. */
i1pro_code i1pro_optimise_sensor(
	i1pro *p,
	double *pnew_int_time,
	int    *pnew_gain_mode,
	double cur_int_time,
	int    cur_gain_mode,
	int    permithg,		/* nz to permit switching to high gain mode */
	int    permitclip,		/* nz to permit clipping out of range int_time, else error */
	double targoscale,		/* Optimising target scale ( <= 1.0) */
	double scale			/* scale needed of current int time to reach optimum */
);

/* Compute the number of measurements needed, given the target */
/* time and integration time. Will return 0 if target time is 0 */
int i1pro_comp_nummeas(
	i1pro *p,
	double meas_time,
	double int_time
);

/* Convert the dark interpolation data to a useful state */
void i1pro_prepare_idark(i1pro *p);

/* Create the dark reference for the given integration time and gain */
/* by interpolating from the 4 readings taken earlier. */
i1pro_code i1pro_interp_dark(
	i1pro *p,
	double *result,		/* Put result of interpolation here */
	double inttime,
	int gainmode
);

/* Create high resolution mode references */
i1pro_code i1pro_create_hr(i1pro *p);

/* Set the noautocalib mode */
void i1pro_set_noautocalib(i1pro *p, int v);

/* Set the trigger config */
void i1pro_set_trig(i1pro *p, inst_opt_mode trig);

/* Return the trigger config */
inst_opt_mode i1pro_get_trig(i1pro *p);

/* Set the trigger return */
void i1pro_set_trigret(i1pro *p, int val);

/* Switch thread handler */
int i1pro_switch_thread(void *pp);

/* ============================================================ */
/* Low level commands */

/* USB Commands */

/* Reset the instrument */
i1pro_code
i1pro_reset(
	struct _i1pro *p,
	int mask	/* reset mask ?. Known values ar 0x1f, 0x07, 0x01 */
);

/* Read from the EEProm */
i1pro_code
i1pro_readEEProm(
	struct _i1pro *p,
	unsigned char *buf,		/* Where to read it to */
	int addr,				/* Address in EEprom to read from */
	int size				/* Number of bytes to read (max 65535) */
);

/* Write to the EEProm */
i1pro_code
i1pro_writeEEProm(
	i1pro *p,
	unsigned char *buf,		/* Where to write from */
	int addr,				/* Address in EEprom to write to */
	int size				/* Number of bytes to write (max 65535) */
);

/* Get the miscelanious status */
/* return pointers may be NULL if not needed. */
i1pro_code
i1pro_getmisc(
	i1pro *p,
	int *fwrev,		/* Return the hardware version number */
	int *unkn1,		/* Unknown status, set after doing a measurement */
	int *maxpve,	/* Maximum positive value in sensor readings */
	int *unkn3,		/* Unknown status, usually 1 */
	int *powmode	/* 0 = high power mode, 8 = low power mode */
);

/* Get the current measurement parameters */
/* return pointers may be NULL if not needed. */
i1pro_code
i1pro_getmeasparams(
	i1pro *p,
	int *intclocks,		/* Number of integration clocks */
	int *lampclocks,	/* Number of lamp turn on sub-clocks */
	int *nummeas,		/* Number of measurements */
	int *measmodeflags	/* Measurement mode flags */
);

#define I1PRO_MMF_SCAN		0x01	/* Scan mode bit, else spot mode */
#define I1PRO_MMF_NOLAMP	0x02	/* No lamp mode, else use illumination lamp */
#define I1PRO_MMF_GAINMODE	0x04	/* Normal gain mode, else high gain */
#define I1PRO_MMF_UNKN		0x08	/* Unknown. Not usually set */

/* Set the measurement parameters */
i1pro_code
i1pro_setmeasparams(
	i1pro *p,
	int intclocks,		/* Number of integration clocks */
	int lampclocks,		/* Number of lamp turn on sub-clocks */
	int nummeas,		/* Number of measurements to make */
	int measmodeflags	/* Measurement mode flags */
);

/* Trigger a measurement after the delay in msec. */
/* The actual return code will be in m->trig_rv after the delay */
i1pro_code
i1pro_triggermeasure(i1pro *p, int delay);


/* Read a measurements results */
i1pro_code
i1pro_readmeasurement(
	i1pro *p,
	int inummeas,			/* Initial number of measurements to expect */
	int scanflag,			/* NZ if in scan mode to continue reading */
	unsigned char *buf,		/* Where to read it to */
	int bsize,				/* Bytes available in buffer */
	int *nummeas,			/* Return number of readings measured */
	int calib_measure,		/* flag - nz if this is a calibration measurement */
	int dark_measure		/* flag - nz if this is a dark measurement */
);


/* Set the measurement clock mode */
/* Version >= 301 only */
i1pro_code
i1pro_setmcmode(
	i1pro *p,
	int mcmode	/* Measurement clock mode, 1..mxmcmode */
);


/* Get the current measurement clock mode */
/* Return pointers may be NULL if not needed. */
/* Version >= 301 only */
i1pro_code
i1pro_getmcmode(
	i1pro *p,
	int *maxmcmode,		/* mcmode must be < maxmcmode */
	int *mcmode,		/* readback current mcmode */
	int *subclkdiv,		/* Sub clock divider ratio */
	int *intclkusec,	/* Integration clock in usec */
	int *subtmode		/* Subtract mode on read using average of value 127 */
);

/* Wait for a reply triggered by a key press */
i1pro_code i1pro_waitfor_switch(i1pro *p, double top);

/* Wait for a reply triggered by a key press (thread version) */
i1pro_code i1pro_waitfor_switch_th(i1pro *p, double top);

/* Terminate switch handling ? */
i1pro_code i1pro_terminate_switch(i1pro *p);

/* -------------------------------------------------- */
/* Key/Value storage */

/* Calibration data storage class */
/* The i1pro stores all it's calibration information */
/* using a key/values arrangement. */
/* We provide a place to store and retrieve that information here. */

/* We haven't implemented a full set of functions - it's not possible */
/* to create the store from scratch, re-allocate key/value entries, */
/* resize entries or anything else of this sort. */


/* Data Key identifiers */

/* Note that array sizes are nominal. They could change with */
/* driver and instrument changes. */

/* "Log" data is keys 2710-2715, 271a-271d, 2724-2725 */

/* The log data seems largly devoted to the last remission spot calibration */
/* or reading, and some general statistics. */

typedef enum {
 
// Note 0x2710 = 10000
	key_meascount	= 0x2715,	/* int, Total Measure (Emis/Remis/Ambient/Trans/Cal) count */
								/* but not the pre-Remission dark calibration. */
	key_darkreading	= 0x271a,	/* int[128] Remspotcal Dark data */
	key_whitereading= 0x271b,	/* int[128] Remspotcal White data */
	key_gainmode	= 0x271c,	/* int - Remspotcal gain mode, Values 1 (normal) or 0 (high) */
	key_inttime		= 0x271d,	/* double - Remspotcal integration time */
	key_caldate		= 0x2724,	/* int date - Remspotcal last calibration date */
	key_calcount	= 0x2725,	/* int - Remission spot measure Count at last Remspotcal. */
	key_checksum	= 0x2710,	/* int - Log checksum */
	key_rpinttime	= 0x2711,	/* double - Last remision spot reading integration time */
	key_rpcount		= 0x2712,	/* int - Remission spot measure Count */
	key_acount		= 0x2713,	/* int - Remission scan measure Count (??) */
	key_lampage		= 0x2714,	/* double - Total lamp usage time (??) */

/* Duplicate of above, keys += 0x3E8 (+1000) */
// (0x2af8 = 11000)

	key_2logoff		= 0x03e8,	/* Offset from first to second copy of log keys */


/* Calibration parameters are 3e8-3ec, 44c-44e, 4b4-4b5, 4b7-4b8, 4bb-4bd, */
/* 4c5-4c6, bb9-bba, bbf-bc6, fa0 */

// Note 0x3e8 = 1000
//      0x44c = 1100
//      0x4b0 = 1200
//      0xbb8 = 3000
//      0xfa0 = 4000

/* Linearisation uses Polinomial equation, ie: y = c0 + c1 * x + c2 * x^2 + c3 * x^3 etc. */
/* and is applied to the raw (integer) sensor data. */

	key_ng_lin		= 0x03e8,	/* double[4] */
								/* Normal gain polinomial linearisation coefficients */

	key_hg_lin		= 0x03e9,	/* double[4] */
								/* High gain polinomial linearisation coefficients */

	key_min_int_time= 0x04c5,	/* double - Minumum integration time */
								/* default 8.84000025689601900e-003 in EEProm */
								/* Overwritten in MinilinoLowLevelDriver constructor: */
								/* Default to 8.84000025689601900e-003 if cpldrev == 101 Ver A */
								/* Default to 4.71600005403161050e-003 if cpldrev == 301 Ver B+ */

	key_max_int_time= 0x04c6,	/* double - Maximum integration time */
								/* Typically 4.4563798904418945 */

	key_mtx_index	= 0x03ea,	/* int[36] */
								/* Matrix CCD sample index for each out wavelength */
								/* 380 - 730nm */

	key_mtx_nocoef	= 0x03eb,	/* int[36] */
								/* Number of matrix cooeficients for each out wavelength */

	key_mtx_coef	= 0x03ec,	/* double[36 x 16] */
								/* Matrix cooeficients to compute each wavelength */

	key_0bb9		= 0x0bb9,	/* int - value typically -1*/
	key_0bba		= 0x0bba,	/* int - value typically -1 */

	key_white_ref	= 0x044c,	/* double[36] */
								/* White calibration tile reflectance values */

	key_emis_coef	= 0x044d,	/* double[36] */
								/* Emission calibration coefficients */

	key_amb_coef	= 0x044e,	/* double[36] */
								/* Ambient light calibration values (compound with Emission) */
								/* May be < 36, values -1.0 if Ambient is not supported */

	key_0fa0		= 0x0fa0,	/* int */
	key_0bbf		= 0x0bbf,	/* int */

	key_cpldrev		= 0x0bc0,	/* int - Firmware revision number */

	key_0bc1		= 0x0bc1,	/* int[5] */

	key_capabilities= 0x0bc2,	/* int */
								/* Capabilities flag ? */
								/* ie. has Ambient capability if val & 0x6000 != 0 */

	key_0bc3		= 0x0bc3,	/* int */

	key_physfilt	= 0x0bc4,	/* int - physical filter */
								/* 0x80 == no filter */
								/* 0x82 == UV filter */

	key_0bc5		= 0x0bc5,	/* int */

	key_0bc6		= 0x0bc6,	/* double */

	key_sens_target	= 0x04b4,	/* int - sensor optimal target value */
								/* typical value 37000 */

	key_sens_dark	= 0x04b5,	/* int - sensor dark reference threshold */
								/* typically value 150 */

	key_ng_sens_sat	= 0x04b7,	/* int */
								/* Normal gain sensor saturated threshold */
								/* typically value 45000 */

	key_hg_sens_sat	= 0x04b8,	/* int */
								/* High gain sensor saturated threshold */
								/* typically value 45000 */

	key_serno		= 0x04bb,	/* int - serial number */

	key_dom			= 0x04bc,	/* int - unknown */
								/* Possibly date of manufacture DDMMYYYY ? */
								/* ie., decimal 10072002 would be 10/7/2002 ? */

	key_hg_factor	= 0x04bd	/* double */
								/* High gain mode gain factor, ie 9.5572.. */

} i1key;


/* Data type */
typedef enum {
	i1_dtype_unknown = 0,
	i1_dtype_int     = 2,		/* 32 bit int */
	i1_dtype_double  = 3,		/* 64 bit double, serialized as 32 bit float */
	i1_dtype_section = 4		/* End of section marker */
} i1_dtype;

/* A key/value entry */
struct _i1keyv {
	void          *data;		/* Array of data */	
	unsigned int    count;		/* Count of data */
	i1_dtype        type;		/* Type of data */
	int             addr;		/* EEProm address */
	int             size;		/* Size in bytes */
	int             key;		/* 16 bit key */
	struct _i1keyv *next;		/* Link to next keyv */
}; typedef struct _i1keyv i1keyv;

struct _i1data {
  /* private: */
	i1pro *p;
	i1proimp *m;

	int verb;
	int debug;
	i1keyv *head;			/* Pointer to first in chain of keyv */
	i1keyv *last;			/* Pointer to last in chain of keyv */
	
  /* public: */

	/* Search the linked list for the given key */
	/* Return NULL if not found */
	i1keyv *(* find_key)(struct _i1data *d, i1key key);

	/* Search the linked list for the given key and */
	/* return it, or create the key if it doesn't exist. */
	/* Return NULL on error */ 
	i1keyv *(* make_key)(struct _i1data *d, i1key key);

	/* Return type of data associated with key. Return i1_dtype_unknown if not found */
	i1_dtype (*get_type)(struct _i1data *d, i1key key);

	/* Return the number of data items in a keyv. Return 0 if not found */
	unsigned int (*get_count)(struct _i1data *d, i1key key);

	/* Return a pointer to the int data for the key. */
	/* Optionally return the number of items too. */
	/* Return NULL if not found or wrong type */
	int *(*get_ints)(struct _i1data *d, unsigned int *count, i1key key);

	/* Return a pointer to the double data for the key. */
	/* Optionally return the number of items too. */
	/* Return NULL if not found or wrong type */
	double *(*get_doubles)(struct _i1data *d, unsigned int *count, i1key key);


	/* Return pointer to one of the int data for the key. */
	/* Return NULL if not found or wrong type or out of range index. */
	int *(*get_int)(struct _i1data *d, i1key key, unsigned int index);

	/* Return pointer to one of the double data for the key. */
	/* Return NULL if not found or wrong type or out of range index. */
	double *(*get_double)(struct _i1data *d, i1key key, double *data, unsigned int index);


	/* Un-serialize a char buffer into an i1key keyv */
	/* (Don't change addr if its is -1) */
	i1pro_code (*unser_ints)(struct _i1data *d, i1key key, int addr,
	                        unsigned char *buf, unsigned int size);

	/* Un-serialize a char buffer of floats into a double keyv */
	/* (Don't change addr if its is -1) */
	i1pro_code (*unser_doubles)(struct _i1data *d, i1key key, int addr,
	                           unsigned char *buf, unsigned int size);


	/* Serialize an i1key keyv into a char buffer. Error if it is outside the buffer */
	i1pro_code (*ser_ints)(struct _i1data *d, i1keyv *k, unsigned char *buf, unsigned int size);

	/* Serialize a double keyv as floats into a char buffer. Error if the buf is not big enough */
	i1pro_code (*ser_doubles)(struct _i1data *d, i1keyv *k, unsigned char *buf, unsigned int size);

	/* Initialise the data from the EEprom contents */
	i1pro_code (*parse_eeprom)(struct _i1data *d, unsigned char *buf, unsigned int len);


	/* Serialise all the keys up to the first marker into a buffer. */
	i1pro_code (*prep_section1)(struct _i1data *d, unsigned char **buf, unsigned int *len);

	/* Copy an array full of ints to the key (must be same size as existing) */ 
	i1pro_code (*add_ints)(struct _i1data *d, i1key key, int *data, unsigned int count);

	/* Copy an array full of doubles to the key (must be same size as existing) */ 
	i1pro_code (*add_doubles)(struct _i1data *d, i1key key, double *data, unsigned int count);


	/* Destroy ourselves */
	void (*del)(struct _i1data *d);

	/* Other utility methods */

	/* Return the data type for the given key identifier */
	i1_dtype (*det_type)(struct _i1data *d, i1key key);

	/* Given an index starting at 0, return the matching key code */
	/* for keys that get checksummed. Return 0 if outside range. */
	i1key (*chsum_keys)(struct _i1data *d, int index);

	/* Compute a checksum. */
	int (*checksum)(struct _i1data *d, i1key keyoffset); 

}; typedef struct _i1data i1data;

/* Constructor. Construct from the EEprom contents */
extern i1data *new_i1data(i1proimp *m, int verb, int debug);

#define I1PRO_IMP
#endif /* I1PRO_IMP */
