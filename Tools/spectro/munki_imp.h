
#ifndef MUNKI_IMP_H

 /* X-Rite ColorMunki related defines */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   12/1/2009
 *
 * Copyright 2006 - 2010, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * (Base on i1pro_imp.h)
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

#ifdef __cplusplus
	extern "C" {
#endif

/* Implementation resources for munki driver */

/* -------------------------------------------------- */
/* Implementation class */

typedef int munki_code;		/* Type to use for error codes */

/* MUNKI mode state. This is implementation data that */
/* depends on the mode the instrument is in. */
/* Each mode has a separate calibration, and configured instrument state. */

typedef enum {
	mk_refl_spot      = 0,
	mk_refl_scan      = 1,
	mk_disp_spot      = 2,		
	mk_proj_spot      = 3,		
	mk_emiss_spot     = 4,
	mk_tele_spot      = 5,
	mk_emiss_scan     = 6,
	mk_amb_spot       = 7,
	mk_amb_flash      = 8,
	mk_trans_spot     = 9,
	mk_trans_scan     = 10,
	mk_no_modes       = 11
} mk_mode;

struct _munki_state {
	/* Just one of the following 3 must always be set */
	int emiss;			/* flag - Emissive mode */
	int trans;			/* flag - Transmissive mode */
	int reflective;		/* flag - Reflective mode */

	/* The following modify emiss */
	int ambient;		/* flag - Ambient position mode */
	int projector;		/* flag - Projector position (tele) mode */

	/* The following can be added to any of the 3: */
	int scan;			/* flag - Scanning mode */
	int adaptive;		/* flag - adaptive mode (emiss - adapt for each measurement) */

	/* The following can be added to scan: */
	int flash;			/* flag - Flash detection from scan mode */

	/* Configuration & state information */
	double targoscale;	/* Optimal reading scale factor <= 1.0 */
	int gainmode;		/* Gain mode, 0 = normal, 1 = high */
	double inttime;		/* Integration time */
	double invsampt;	/* Invalid sample time */

	double dpretime;	/* Target pre-read dark read time - sets no. of readings */
	double wpretime;	/* Target pre-read white/sample read time - sets no. of readings */

	double dcaltime;	/* Target dark calibration time - sets number of readings */
	double wcaltime;	/* Target white calibration time - sets number of readings (not used?) */

	double dreadtime;	/* Target dark on-the-fly cal time - sets number of readings */
	double wreadtime;	/* Target white/sample reading time - sets number of readings */

	double maxscantime;	/* Maximum scan time sets buffer size allocated */

	double min_wl;		/* Minimum wavelegth to report for this mode */

	/* calibration information for this mode */
	int dark_valid;			/* dark calibration factor valid */
	time_t ddate;			/* Date/time of last dark calibration */
	double dark_int_time;	/* Integration time used for dark data */
	double *dark_data;		/* [nraw] of dark level to subtract. Note that the dark value */
							/* depends on integration time and gain mode. */
	int dark_gain_mode;		/* Gain mode used for dark data */

	int cal_valid;			/* calibration factor valid */
	time_t cfdate;			/* Date/time of last cal factor calibration */
	double *cal_factor;		/* [nwav] of calibration scale factor for this mode */
	double *cal_factor1, *cal_factor2;	/* (Underlying tables for two resolutions) */
	double *white_data;		/* [nraw] linear absolute dark subtracted white data */
							/*        used to compute cal_factors (at reftemp) */
	double **iwhite_data;	/* [nraw][2] LED temperature data to interpolate white_data from */
	double reftemp;			/* Reference temperature to correct to */

	/* Adaptive emission/transparency black data */
	int idark_valid;		/* idark calibration factors valid */
	time_t iddate;			/* Date/time of last dark idark calibration */
	double idark_int_time[4];
	double **idark_data;	/* [4][nraw] of dark level for inttime/gains of : */
							/* 0.01 norm, 1.0 norm, 0.01 high, 1.0 high */
							/* then it's converted to base + increment with inttime */

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

}; typedef struct _munki_state munki_state;
 


/* MUNKI implementation class */
struct _munkiimp {
	munki *p;

	struct _mkdata *data;		/* EEProm data container */
	athread *th;				/* Switch monitoring thread (NULL if not used) */
	volatile int switch_count;	/* Incremented in thread */
	void *hcancel;				/* Context for canceling outstandin I/O */
	volatile int th_term;		/* Thread terminate on error rather than retry */
	volatile int th_termed;		/* Thread has terminated */
	inst_opt_mode trig;			/* Reading trigger mode */
	int trig_return;			/* Emit "\n" after trigger */
	int noautocalib;			/* Disable automatic calibration if not essential */
	int nosposcheck;			/* Disable checking the sensor position */
	int highres;				/* High resolution mode */
	int hr_inited;				/* High resolution has been initialized */

	/* Current settings */
	mk_mode mmode;					/* Current measurement mode selected */
	munki_state ms[mk_no_modes];	/* Mode state */
	int spec_en;				/* Enable reporting of spectral data */

	double intclkp;				/* Integration clock period (computed from tickdur) */

	/* Current state of hardware (~~99 are all these used ??) */
	double c_inttime;			/* Integration period (=inttime + deadtime) */
	int c_measmodeflags;		/* Measurement mode flags (set by trigger() */

	/* Information from the HW */
	int fwrev;					/* int - Firmware revision number, from getfirm() */
								/* Typically 0120 = V1.32 */
	unsigned char chipid[8];	/* HW serial number */
	char vstring[37];			/* Asciiz version string */
	int tickdur;				/* Tick duration (usec, converted to intclkp) */
	int minintcount;			/* Minimum integration tick count */
	int noeeblocks;				/* Number of EEPROM blocks */
	int eeblocksize;			/* Size of each block */

	/* Information from the EEProm */
	int calver;					/* Effective calibration version number */
	int prodno;					/* Production number */
	char serno[17];             /* serial number string */
	int adctype;				/* A/D converter type */

	double minsval;				/* Minimum sensor value target */
	double optsval;				/* Optimal sensor value target */
	double maxsval;				/* Maximum sensor value target */
	double satlimit;			/* Saturation limit */ 

	int ledholdtempdc;			/* LED Hold temparature duty cycle */
								/* Parameter used in measure instruction. [0] */
	double ledpreheattime;		/* LED Pre-heat time, Seconds [1.0] */
								/* Time to turn LED on before determining */
								/* integration time that achieves optimal sensor value. */
	double cal_int_time;		/* Calibration integration time in seconds. [0.018208] */
								/* Starting integration time use for achieving */
								/* the optimal sensor value. */
	double ledwaittime;			/* LED Wait time, Seconds [1.0] */
								/* Time to wait for LED to cool down */
								/* before determining temperature/output characteristic. */
	double calscantime;			/* Calibration scan time used for determining temp/output */
								/* characteristic of LED, Seconds. [3.0] */
	double refinvalidsampt;		/* Reflection invalid sample time in seconds. [0.1] */
								/* This sets the number of extra samples to add to a */
								/* reflective read, and then to discard from */
								/* the start of the measurements. This allows for LED */
								/* thermal warmup. */ 

	double min_int_time;	/* Minimum integration time (secs) (set from minintcount) */
	double max_int_time;	/* Maximum integration time (secs) (fixed in sw) */

	/* Underlying calibration information */
	int nraw;				/* Raw sample bands stored = 133 */
							/* Only 128 starting at offset 6 are actually valid. */
	// ~~99 change this to rnwav, rwl_short, rwl_long, enwav, ewl_short, ewl_long ?? */
	int nwav;				/* Current cooked spectrum bands stored, usually = 36 */
	double wl_short;		/* Cooked spectrum bands short wavelength, usually 380 */
	double wl_long;			/* Cooked spectrum bands short wavelength, usually 730 */

	unsigned int nwav1, nwav2;	/* Available bands for standard and high-res modes */
	double wl_short1, wl_short2, wl_long1, wl_long2;

							/* Reflection */
	int *rmtx_index;		/* [nwav] Matrix CCD sample starting index for each out wavelength */
	int *rmtx_nocoef; 		/* [nwav] Number of matrix cooeficients for each out wavelength */
	double *rmtx_coef;		/* [nwav * rmtx_nocoef] Matrix coeef's to compute each wavelength */
	int *rmtx_index1, *rmtx_index2;		/* Underlying arrays for the two resolutions */
	int *rmtx_nocoef1, *rmtx_nocoef2;	/* first [nwav1], second [nwav2] */
	double *rmtx_coef1, *rmtx_coef2;

							/* Emission */
	int *emtx_index;		/* [nwav] Matrix CCD sample starting index for each out wavelength */
	int *emtx_nocoef; 		/* [nwav] Number of matrix cooeficients for each out wavelength */
	double *emtx_coef;		/* [nwav * emtx_nocoef] Matrix coeef's to compute each wavelength */
	int *emtx_index1, *emtx_index2;		/* Underlying arrays for the two resolutions */
	int *emtx_nocoef1, *emtx_nocoef2;	/* first [nwav1], second [nwav2] */
	double *emtx_coef1, *emtx_coef2;

	unsigned int nlin0;		/* Number in array */
	double *lin0;			/* Array of linearisation polinomial factors, normal gain. */

	unsigned int nlin1;		/* Number in array */
	double *lin1;			/* Array of linearisation polinomial factors, high gain. */
		
	double *white_ref;		/* [nwav] White calibration tile reflectance values */
	double *emis_coef;		/* [nwav] Emission calibration coefficients */
	double *amb_coef;		/* [nwav] Ambient light cal values (compound with Emission) */
	double *proj_coef;		/* [nwav] Projector light cal values (compound with Emission) */
	double *white_ref1, *white_ref2;	/* Underlying tables for normal/high res modes */
	double *emis_coef1, *emis_coef2;
	double *amb_coef1, *amb_coef2;
	double *proj_coef1, *proj_coef2;

	double **straylight;		/* [nwav][nwav] Stray light convolution matrix */
	double **straylight1, **straylight2;	/* Underlying tables for normal/high res modes */

	double highgain;		/* High gain mode gain */
	double scan_toll_ratio;	/* Modifier of scan tollerance */

	/* Trigger houskeeping */
	int tr_t1, tr_t2, tr_t3, tr_t4, tr_t5, tr_t6, tr_t7;	/* Trigger/read timing diagnostics */
							/* 1->2 = time to execute trigger */
							/* 2->3 = time to between end trigger and start of first read */
							/* 3->4 = time to exectute first read */
							/* 6->5 = time between end of second last read and start of last read */
	int trig_se;			/* Delayed trigger icoms error */
	munki_code trig_rv;		/* Delayed trigger result */

}; typedef struct _munkiimp munkiimp;

/* Add an implementation structure */
munki_code add_munkiimp(munki *p);

/* Destroy implementation structure */
void del_munkiimp(munki *p);

/* ============================================================ */
/* Error codes returned from munki_imp */

/* Note: update munki_interp_error() and munki_interp_code() in munki.c */
/* if anything of these #defines are added or subtracted */

/* Fake Error codes */
#define MUNKI_INTERNAL_ERROR			0x71		/* Internal software error */
#define MUNKI_COMS_FAIL					0x72		/* Communication failure */
#define MUNKI_UNKNOWN_MODEL				0x73		/* Not an munki */
#define MUNKI_DATA_PARSE_ERROR  		0x74		/* Read data parsing error */
#define MUNKI_USER_ABORT			    0x75		/* User hit abort */
#define MUNKI_USER_TERM		    		0x76		/* User hit terminate */
#define MUNKI_USER_TRIG 			    0x77		/* User hit trigger */
#define MUNKI_USER_CMND		    		0x78		/* User hit command */

#define MUNKI_UNSUPPORTED		   		0x79		/* Unsupported function */
#define MUNKI_CAL_SETUP                 0x7A		/* Cal. retry with correct setup is needed */

/* Real error code */
#define MUNKI_OK   						0x00

/* EEprop parsing errors */
#define MUNKI_DATA_COUNT			    0x01		/* count unexpectedly small */
#define MUNKI_DATA_RANGE			    0x02		/* out of range of buffer */
#define MUNKI_DATA_MEMORY 				0x03		/* memory alloc failure */

/* HW errors */
#define MUNKI_HW_EE_SHORTREAD		    0x21		/* Read fewer EEProm bytes than expected */
#define MUNKI_HW_ME_SHORTREAD		    0x22		/* Read measurement bytes than expected */
#define MUNKI_HW_ME_ODDREAD			    0x23		/* Read measurement bytes was not mult 274 */
#define MUNKI_HW_CALIBVERSION		    0x24		/* calibration version is unknown */
#define MUNKI_HW_CALIBMATCH		        0x25		/* calibration doesn't match device */

/* Sample read operation errors */
#define MUNKI_RD_DARKREADINCONS		    0x30		/* Dark calibration reading inconsistent */
#define MUNKI_RD_SENSORSATURATED	    0x31		/* Sensor is saturated */
#define MUNKI_RD_DARKNOTVALID   	    0x32		/* Dark reading is not valid (too light) */
#define MUNKI_RD_NEEDS_CAL 		        0x33		/* Mode needs calibration */
#define MUNKI_RD_WHITEREADINCONS        0x34		/* White reference readings are inconsistent */
#define MUNKI_RD_WHITEREFERROR 	        0x35		/* White reference reading error */
#define MUNKI_RD_LIGHTTOOLOW 	        0x36		/* Light level is too low */
#define MUNKI_RD_LIGHTTOOHIGH 	        0x37		/* Light level is too high */
#define MUNKI_RD_SHORTMEAS              0x38		/* Measurment was too short */
#define MUNKI_RD_READINCONS             0x39		/* Reading is inconsistent */
#define MUNKI_RD_REFWHITENOCONV         0x3A		/* White calibration didn't converge */
#define MUNKI_RD_NOTENOUGHPATCHES       0x3B		/* Not enough patches */
#define MUNKI_RD_TOOMANYPATCHES         0x3C		/* Too many patches */
#define MUNKI_RD_NOTENOUGHSAMPLES       0x3D		/* Not enough samples per patch */
#define MUNKI_RD_NOFLASHES              0x3E		/* No flashes recognized */
#define MUNKI_RD_NOAMBB4FLASHES         0x3F		/* No ambient before flashes found */

#define MUNKI_SPOS_PROJ                 0x40		/* Sensor needs to be in projector position */
#define MUNKI_SPOS_SURF                 0x41		/* Sensor needs to be in surface position */
#define MUNKI_SPOS_CALIB                0x42		/* Sensor needs to be in calibration position */
#define MUNKI_SPOS_AMB                  0x43		/* Sensor needs to be in ambient position */

/* Internal errors */
#define MUNKI_INT_NO_COMS 		        0x50
#define MUNKI_INT_EEOUTOFRANGE 		    0x51		/* EEProm access is out of range */
#define MUNKI_INT_CALTOOSMALL 		    0x52		/* Calibration EEProm size is too small */
#define MUNKI_INT_CALTOOBIG 		    0x53		/* Calibration EEProm size is too big */
#define MUNKI_INT_CALBADCHSUM 		    0x54		/* Calibration has a bad checksum */
#define MUNKI_INT_ODDREADBUF 	        0x55		/* Measurment read buffer is not mult 274 */
#define MUNKI_INT_INTTOOBIG				0x56		/* Integration time is too big */
#define MUNKI_INT_INTTOOSMALL			0x57		/* Integration time is too small */
#define MUNKI_INT_ILLEGALMODE			0x58		/* Illegal measurement mode selected */
#define MUNKI_INT_ZEROMEASURES 			0x59		/* Number of measurements requested is zero */
#define MUNKI_INT_WRONGPATCHES 			0x5A		/* Number of patches to match is wrong */
#define MUNKI_INT_MEASBUFFTOOSMALL 		0x5B		/* Measurement read buffer is too small */
#define MUNKI_INT_NOTIMPLEMENTED 		0x5C		/* Support not implemented */
#define MUNKI_INT_NOTCALIBRATED 		0x5D		/* Unexpectedely invalid calibration */
#define MUNKI_INT_THREADFAILED 		    0x5E		/* Creation of thread failed */
#define MUNKI_INT_BUTTONTIMEOUT 	    0x5F		/* Switch status read timed out */
#define MUNKI_INT_CIECONVFAIL 	        0x60		/* Creating spectral to CIE converted failed */
#define MUNKI_INT_MALLOC                0x61		/* Error in mallocing memory */
#define MUNKI_INT_CREATE_EEPROM_STORE   0x62		/* Error in creating EEProm store */
#define MUNKI_INT_NEW_RSPL_FAILED       0x63		/* Creating RSPL object faild */
#define MUNKI_INT_CAL_SAVE              0x64		/* Unable to save calibration to file */
#define MUNKI_INT_CAL_RESTORE           0x65		/* Unable to restore calibration from file */

/* ============================================================ */
/* High level implementatation */

/* Initialise our software state from the hardware */
munki_code munki_imp_init(munki *p);

/* Return a pointer to the serial number */
char *munki_imp_get_serial_no(munki *p);

/* Set the measurement mode. It may need calibrating */
munki_code munki_imp_set_mode(
	munki *p,
	mk_mode mmode,		/* munki mode to use */
	int spec_en);		/* nz to enable reporting spectral */

/* Determine if a calibration is needed. */
inst_cal_type munki_imp_needs_calibration(munki *p);

/* Calibrate for the current mode. */
/* Request an instrument calibration of the current mode. */
munki_code munki_imp_calibrate(
munki *p,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[100]			/* Condition identifier (ie. white reference ID) */
);

/* Measure a patch or strip in the current mode. */
munki_code munki_imp_measure(
	munki *p,
	ipatch *val,		/* Pointer to array of instrument patch value */
	int nvals			/* Number of values */	
);

/* return nz if high res is supported */
int munki_imp_highres(munki *p);

/* Set to high resolution mode */
munki_code munki_set_highres(munki *p);

/* Set to standard resolution mode */
munki_code munki_set_stdres(munki *p);

/* Modify the scan consistency tollerance */
munki_code munki_set_scan_toll(munki *p, double toll_ratio);


/* Save the calibration for all modes, stored on local system */
munki_code munki_save_calibration(munki *p);

/* Restore the all modes calibration from the local system */
munki_code munki_restore_calibration(munki *p);


/* ============================================================ */
/* Intermediate routines  - composite commands/processing */

/* Take a dark reference measurement - part 1 */
munki_code munki_dark_measure_1(
	munki *p,
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	unsigned char *buf,		/* USB reading buffer to use */
	unsigned int bsize		/* Size of buffer */
);

/* Take a dark reference measurement - part 2 */
munki_code munki_dark_measure_2(
	munki *p,
	double *sens,			/* Return array [nraw] of sens values */
	int nummeas,			/* Number of readings to take */
	double inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	unsigned char *buf,		/* raw USB reading buffer to process */
	unsigned int bsize		/* Buffer size to process */
);

/* Take a dark measurement */
munki_code munki_dark_measure(
	munki *p,
	double *sens,			/* Return array [nraw] of sens values */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode			/* Gain mode to use, 0 = normal, 1 = high */
);

/* Take a white reference measurement */
/* (Subtracts black and processes into wavelenths) */
munki_code munki_whitemeasure(
	munki *p,
	double *absraw,			/* Return array [nraw] of absraw values (may be NULL) */
	double *optscale,		/* Factor to scale gain/int time by to make optimal (may be NULL) */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	double targoscale		/* Optimal reading scale factor */
);

/* Given an absraw white reference measurement, */
/* compute the wavelength equivalents. */
/* (absraw is usually ->white_data) */
/* (abswav1 is usually ->cal_factor1) */
/* (abswav2 is usually ->cal_factor2) */
munki_code munki_compute_wav_whitemeas(
	munki *p,
	double *abswav1,		/* Return array [nwav1] of abswav values (may be NULL) */
	double *abswav2,		/* Return array [nwav2] of abswav values (if hr_init, may be NULL) */
	double *absraw			/* Given array [nraw] of absraw values */
);

/* Take a reflective white reference measurement, */
/* subtracts black and decompose into base + LED temperature components */
munki_code munki_ledtemp_whitemeasure(
	munki *p,
	double *white,			/* Return [nraw] of temperature compensated white reference */
	double **iwhite,		/* Return array [nraw][2] of absraw base and scale values */
	double *reftemp,		/* Return a reference temperature to normalize to */
	int nummeas,			/* Number of readings to take */
	double inttime, 		/* Integration time to use */
	int gainmode			/* Gain mode to use, 0 = normal, 1 = high */
);

/* Given the ledtemp base and scale values, */
/* return a raw reflective white reference for the */
/* given temperature */ 
munki_code munki_ledtemp_white(
	munki *p,
	double *absraw,			/* Return array [nraw] of absraw base and scale values */
	double **iwhite,		/* ledtemp base and scale */
	double ledtemp			/* LED temperature value */
);

/* Given a set of absraw sensor readings and the corresponding temperature, */
/* compensate the readings to be at the nominated temperature. */
munki_code munki_ledtemp_comp(
	munki *p,
	double **absraw,		/* [nummeas][raw] measurements to compensate */
	double *ledtemp,		/* LED temperature for each measurement */
	int nummeas,			/* Number of measurements */
	double reftemp,			/* LED reference temperature to compensate to */
	double **iwhite			/* ledtemp base and scale information */
);

/* Heat the LED up for given number of seconds by taking a reading */
munki_code munki_heatLED(
	munki *p,
	double htime		/* Heat up time */
);

/* Process a single raw white reference measurement */
/* (Subtracts black and processes into wavelenths) */
munki_code munki_whitemeasure_buf(
	munki *p,
	double *abswav1,		/* Return array [nwav1] of abswav values (may be NULL) */
	double *abswav2,		/* Return array [nwav2] of abswav values (if hr_init, may be NULL) */
	double *absraw,			/* Return array [nraw] of absraw values */
	double inttime, 		/* Integration time to used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	unsigned char *buf		/* Raw buffer */
);

/* Take a measurement reading using the current mode, part 1 */
/* Converts to completely processed output readings. */
munki_code munki_read_patches_1(
	munki *p,
	int ninvmeas,			/* Number of extra invalid measurements at start */
	int minnummeas,			/* Minimum number of measurements to take */
	int maxnummeas,			/* Maximum number of measurements to allow for */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	int *nmeasuered,		/* Number actually measured (excluding ninvmeas) */
	unsigned char *buf,		/* Raw USB reading buffer */
	unsigned int bsize
);

/* Take a measurement reading using the current mode, part 2 */
/* Converts to completely processed output readings. */
munki_code munki_read_patches_2(
	munki *p,
	double *duration,		/* return flash duration (secs) */
	double **specrd,		/* Return array [numpatches][nwav] of spectral reading values */
	int numpatches,			/* Number of patches to return */
	double inttime, 		/* Integration time to used */
	int gainmode,			/* Gain mode useed, 0 = normal, 1 = high */
	int ninvmeas,			/* Number of extra invalid measurements at start */
	int nmeasuered,			/* Number actually measured */
	unsigned char *buf,		/* Raw USB reading buffer */
	unsigned int bsize
);

/* Take a trial measurement reading using the current mode. */
/* Used to determine if sensor is saturated, or not optimal */
munki_code munki_trialmeasure(
	munki *p,
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
/* The called then needs to call munki_readmeasurement() */
munki_code
munki_trigger_one_measure(
	munki *p,
	int nummeas,			/* Number of measurements to make */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	int calib_measure,		/* flag - nz if this is a calibration measurement */
	int dark_measure		/* flag - nz if this is a dark measurement */
);

/* ============================================================ */
/* lower level reading processing */

/* Take a buffer full of raw readings, and convert them to */
/* directly to floating point sensor values. Return nz if any is saturated */
int munki_meas_to_sens(
	munki *p,
	double **abssens,		/* Array of [nummeas-ninvalid][nraw] value to return */
	double *ledtemp,		/* Optional array [nummeas-ninvalid] LED temperature values to return */
	unsigned char *buf,		/* Raw measurement data must be 274 * nummeas */
	int nummeas,			/* Number of readings measured */
	int ninvalid,			/* Number of initial invalid readings to skip */
	double satthresh,		/* Sauration threshold in raw units */
	double *darkthresh		/* Return a dark threshold value */
);

/* Subtract the black from sensor values and convert to */
/* absolute (integration & gain scaled), zero offset based, */
/* linearized sensor values. */
void munki_sub_sens_to_abssens(
	munki *p,
	int nummeas,			/* Return number of readings measured */
	double inttime, 		/* Integration time used */
	int gainmode,			/* Gain mode, 0 = normal, 1 = high */
	double **abssens,		/* Source/Desination array [nraw] */
	double *sub,			/* Value to subtract [nraw] */
	double *trackmax, 		/* abssens values that should be offset the same as max */
	int ntrackmax,			/* Number of trackmax values */
	double *maxv			/* If not NULL, return the maximum value */
);

/* Average a set of sens measurements into one. */
/* Return zero if readings are consistent. */
/* Return nz if the readings are not consistent */
/* Return the overall average. */
int munki_average_sens_multimeas(
	munki *p,
	double *avg,			/* return average [nraw] */
	double **multimeas,		/* Array of [nummeas][nraw] value to average */
	int nummeas,			/* Return number of readings measured */
	double *poallavg,		/* If not NULL, return overall average of bands and measurements */
	double darkthresh		/* Dark threshold (used for consistency check scaling) */
);

/* Recognise the required number of ref/trans patch locations, */
/* and average the measurements within each patch. */
/* Return flags zero if readings are consistent. */
/* Return flags nz if the readings are not consistent */
munki_code munki_extract_patches_multimeas(
	munki *p,
	int *flags,             /* return flags */
	double **pavg,			/* return patch average [naptch][nraw] */
	int npatch,				/* number of patches to recognise */
	double **multimeas,		/* Array of [nummeas][nraw] value to extract from */
	int nummeas,			/* number of readings to recognise them from */
	double inttime			/* Integration time (used to adjust consistency threshold) */
);

/* Recognise any flashes in the readings, and */
/* and average their values together as well as summing their duration. */
/* Return nz on an error */
munki_code munki_extract_patches_flash(
	munki *p,
	int *flags,				/* return flags */
	double *duration,		/* return duration */
	double *pavg,			/* return patch average [nraw] */
	double **multimeas,		/* Array of [nummeas][nraw] value to extract from */
	int nummeas,			/* number of readings made */
	double inttime			/* Integration time (used to compute duration) */
);

/* Convert an abssens array from raw wavelengths to output wavelenths */
/* for the current resolution */
void munki_abssens_to_abswav(
	munki *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav] */
	double **abssens		/* Source array [nraw] */
);

/* Convert an abssens array from raw wavelengths to output wavelenths */
/* for the standard resolution */
void munki_abssens_to_abswav1(
	munki *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav1] */
	double **abssens		/* Source array [nraw] */
);

/* Convert an abssens array from raw wavelengths to output wavelenths */
/* for the high resolution */
void munki_abssens_to_abswav2(
	munki *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav2] */
	double **abssens		/* Source array [nraw] */
);

/* Convert an abswav array of output wavelengths to scaled output readings. */
void munki_scale_specrd(
	munki *p,
	double **outspecrd,		/* Destination */
	int numpatches,			/* Number of readings/patches */
	double **inspecrd		/* Source */
);

/* Convert from spectral to XYZ, and transfer to the ipatch array */
munki_code munki_conv2XYZ(
	munki *p,
	ipatch *vals,		/* Values to return */
	int nvals,			/* Number of values */
	double **specrd		/* Spectral readings */
);

/* Compute a calibration factor given the reading of the white reference. */
/* Return nz if any of the transmission wavelengths are low */
int munki_compute_white_cal(
	munki *p,
	double *cal_factor1,	/* [nwav1] Calibration factor to compute */
	double *white_ref1,		/* [nwav1] White reference to aim for, NULL for 1.0 */
	double *white_read1,	/* [nwav1] The white that was read */
	double *cal_factor2,	/* [nwav2] Calibration factor to compute */
	double *white_ref2,		/* [nwav2] White reference to aim for, NULL for 1.0 */
	double *white_read2		/* [nwav2] The white that was read */
);

/* For adaptive mode, compute a new integration time and gain mode */
/* in order to optimise the sensor values. */
munki_code munki_optimise_sensor(
	munki *p,
	double *pnew_int_time,
	int    *pnew_gain_mode,
	double cur_int_time,
	int    cur_gain_mode,
	int    permithg,		/* nz to permit switching to high gain mode */
	int    permitclip,		/* nz to permit clipping out of range int_time, else error */
	double *targoscale,		/* Optimising target scale ( <= 1.0) */
							/* (May be altered if integration time isn't possible) */
	double scale,			/* scale needed of current int time to reach optimum */
	double deadtime			/* Dead integration time (if any) */
);

/* Compute the number of measurements needed, given the target */
/* time and integration time. Will return 0 if target time is 0 */
int munki_comp_nummeas(
	munki *p,
	double meas_time,
	double int_time
);

/* Compute the rounded up number of measurements needed, */
/* given the target time and integration time. */
/* Will return 0 if target time is 0 */
int munki_comp_ru_nummeas(
	munki *p,
	double meas_time,
	double int_time
);

/* Convert the dark interpolation data to a useful state */
void munki_prepare_idark(munki *p);

/* Create the dark reference for the given integration time and gain */
/* by interpolating from the 4 readings taken earlier. */
munki_code munki_interp_dark(
	munki *p,
	double *result,		/* Put result of interpolation here */
	double inttime,
	int gainmode
);

/* Create high resolution mode references. */
/* Create Reflective if ref nz, else create Emissive */
munki_code munki_create_hr(munki *p, int ref);

/* Set the noautocalib mode */
void munki_set_noautocalib(munki *p, int v);

/* Set the trigger config */
void munki_set_trig(munki *p, inst_opt_mode trig);

/* Return the trigger config */
inst_opt_mode munki_get_trig(munki *p);

/* Set the trigger return */
void munki_set_trigret(munki *p, int val);

/* Switch thread handler */
int munki_switch_thread(void *pp);

/* ============================================================ */
/* Low level commands */

/* USB Commands */

/* Read from the EEProm */
munki_code
munki_readEEProm(
	struct _munki *p,
	unsigned char *buf,		/* Where to read it to */
	int addr,				/* Address in EEprom to read from */
	int size				/* Number of bytes to read (max 65535) */
);


/* Get the firmware parameters */
/* return pointers may be NULL if not needed. */
munki_code
munki_getfirm(
	munki *p,
	int *fwrev,			/* Return the formware version number as 8.8 */
	int *tickdur,		/* Tick duration */
	int *minintcount,	/* Minimum integration tick count */
	int *noeeblocks,	/* Number of EEPROM blocks */
	int *eeblocksize	/* Size of each block */
);

/* Get the Chip ID */
munki_code
munki_getchipid(
	munki *p,
	unsigned char chipid[8]
);

/* Get the Version String */
munki_code
munki_getversionstring(
    munki *p,
    char vstring[37]
);

/* Get the measurement state */
/* return pointers may be NULL if not needed. */
munki_code
munki_getmeasstate(
    munki *p,
    int *ledtrange,     /* LED temperature range */
    int *ledtemp,       /* LED temperature */
    int *dutycycle,     /* Duty Cycle */
    int *ADfeedback     /* A/D converter feedback */
);

/* Munki sensor positions */
typedef enum {
	mk_spos_proj  = 0x00,	/* Projector/Between detents */
	mk_spos_surf  = 0x01,	/* Surface */
	mk_spos_calib = 0x02,	/* Calibration tile */
	mk_spos_amb   = 0x03	/* Ambient */
} mk_spos;

/* Munki switch state */
typedef enum {
	mk_but_button_release = 0x00,	/* Button is released */
	mk_but_button_press   = 0x01	/* Button is pressed */
} mk_but;

/* Get the device status */
/* return pointers may be NULL if not needed. */
munki_code
munki_getstatus(
	munki *p,
	mk_spos *spos,		/* Return the sensor position */
	mk_but *but			/* Return Button state */
);

/* Set the indicator LED state */
munki_code
munki_setindled(
    munki *p,
    int ontime,			/* On time (msec) */
    int offtrime,		/* Off time (msec) */
    int transtime,		/* Transition time (msec) */
    int nopulses,		/* Number of pulses, -1 = max */
    int p5				/* Ignored ? */
);

/* Measuremend mode flags */
#define MUNKI_MMF_LAMP		0x01	/* Lamp mode, else no illumination of sample */
#define MUNKI_MMF_SCAN		0x02	/* Scan mode bit, else spot mode */
#define MUNKI_MMF_HIGHGAIN	0x04	/* High gain, else normal gain. */

/* Trigger a measurement with the given measurement parameters */
munki_code
munki_triggermeasure(
	munki *p,
	int intclocks,		/* Number of integration clocks */
	int nummeas,		/* Number of measurements to make */
	int measmodeflags,	/* Measurement mode flags */
	int holdtempduty	/* Hold temperature duty cycle */
);

/* Read a measurements results */
munki_code
munki_readmeasurement(
	munki *p,
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
munki_code
munki_setmcmode(
	munki *p,
	int mcmode	/* Measurement clock mode, 1..mxmcmode */
);

/* Munki event values, returned by event pipe, or */
/* parameter to simulate event */
typedef enum {
	mk_eve_none           = 0x0000,	/* No event */
	mk_eve_button_press   = 0x0001,	/* Button has been pressed */
	mk_eve_button_release = 0x0002,	/* Button has been released */
	mk_eve_spos_change    = 0x0100	/* Sensor position is being changed */
} mk_eve;

/* Simulating an event (use to terminate event thread) */
/* timestamp is msec since munki power up */
munki_code munki_simulate_event(munki *p, mk_eve ecode, int timestamp);

/* Wait for a reply triggered by a key press */
munki_code munki_waitfor_switch(munki *p, mk_eve *ecode, int *timest, double top);

/* Wait for a reply triggered by a key press (thread version) */
munki_code munki_waitfor_switch_th(munki *p, mk_eve *ecode, int *timest, double top);

/* -------------------------------------------------- */
/* EEProm parsing support. */

/* Initialise the calibration from the EEProm contents. */
/* (We're handed a buffer that's been rounded up to an even 32 bits by */
/* padding with zero's) */
munki_code munki_parse_eeprom(munki *p, unsigned char *buf, unsigned int len);

struct _mkdata {
  /* private: */
	munki *p;

	int verb;
	int debug;
	unsigned char *buf;		/* Buffer to parse */
	int len;				/* Length of buffer */
	
  /* public: */

	/* Return a pointer to an array of chars containing data from 8 bits. */
	/* If rv is NULL, the returned value will have been allocated, othewise */
	/* the rv will be returned. Return NULL if out of range. */
	unsigned char *(*get_8_char)(struct _mkdata *d, unsigned char *rv, int offset, int count);

	/* Return a pointer to an nul terminated string containing data from 8 bits. */
	/* If rv is NULL, the returned value will have been allocated, othewise */
	/* the rv will be returned. Return NULL if out of range. */
	/* An extra space and a nul terminator will be added to the eeprom data */
	char *(*get_8_asciiz)(struct _mkdata *d, char *rv, int offset, int count);


	/* Return a pointer to an array of ints containing data from 8 bits. */
	/* If rv is NULL, the returned value will have been allocated, othewise */
	/* the rv will be returned. Return NULL if out of range. */
	int *(*get_8_ints)(struct _mkdata *d, int *rv, int offset, int count);

	/* Return a pointer to an array of ints containing data from unsigned 8 bits. */
	/* If rv is NULL, the returned value will have been allocated, othewise */
	/* the rv will be returned. Return NULL if out of range. */
	int *(*get_u8_ints)(struct _mkdata *d, int *rv, int offset, int count);


	/* Return a pointer to an array of ints containing data from 16 bits. */
	/* If rv is NULL, the returned value will have been allocated, othewise */
	/* the rv will be returned. Return NULL if out of range. */
	int *(*get_16_ints)(struct _mkdata *d, int *rv, int offset, int count);

	/* Return a pointer to an array of ints containing data from unsigned 16 bits. */
	/* If rv is NULL, the returned value will have been allocated, othewise */
	/* the rv will be returned. Return NULL if out of range. */
	int *(*get_u16_ints)(struct _mkdata *d, int *rv, int offset, int count);


	/* Return a pointer to an array of ints containing data from 32 bits. */
	/* If rv is NULL, the returned value will have been allocated, othewise */
	/* the rv will be returned. Return NULL if out of range. */
	int *(*get_32_ints)(struct _mkdata *d, int *rv, int offset, int count);

	/* Return a pointer to an array of unsigned ints containing data from 32 bits. */
	/* If rv is NULL, the returned value will have been allocated, othewise */
	/* the rv will be returned. Return NULL if out of range. */
	unsigned int *(*get_u32_uints)(struct _mkdata *d, unsigned int *rv, int offset, int count);


	/* Return a pointer to an array of doubles containing data from 32 bits. */
	/* If rv is NULL, the returned value will have been allocated, othewise */
	/* the rv will be returned. Return NULL if out of range. */
	double *(*get_32_doubles)(struct _mkdata *d, double *rv, int offset, int count);

	/* Return a pointer to an array of doubles containing data from 32 bits, */
	/* with the array filled in reverse order. */
	/* If rv is NULL, the returned value will have been allocated, othewise */
	/* the rv will be returned. Return NULL if out of range. */
	double *(*rget_32_doubles)(struct _mkdata *d, double *rv, int offset, int count);

	/* Destroy ourselves */
	void (*del)(struct _mkdata *d);

}; typedef struct _mkdata mkdata;

/* Constructor. Construct from the EEprom calibration contents */
extern mkdata *new_mkdata(munki *p, unsigned char *buf, int len, int verb, int debug);

#ifdef __cplusplus
	}
#endif

#define MUNKI_IMP
#endif /* MUNKI_IMP */
