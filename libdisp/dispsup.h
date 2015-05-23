
#ifndef DISPSUP_H

/* 
 * Argyll Color Correction System
 * Common display patch reading support.
 *
 * Author: Graeme W. Gill
 * Date:   2/11/2005
 *
 * Copyright 1998 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* Helper functions to handle presenting a display test patch */

/* Struct used for calibration */
struct _disp_win_info {
	int webdisp;			/* nz if web display is to be used */
	ccast_id *ccid;	 		/* non-NULL for ChromeCast */
#ifdef NT
	int madvrdisp;			/* NZ for MadVR display */
#endif
	disppath *disp;			/* display to calibrate. */
	int out_tvenc;			/* 1 = use RGB Video Level encoding */
	int blackbg;			/* NZ if whole screen should be filled with black */
	int override;			/* Override_redirect on X11 */
	double hpatsize;		/* Size of dispwin */
	double vpatsize;		/* Size of dispwin */
	double ho, vo;			/* Position of dispwin */
	dispwin *dw;			/* Display window if already open */
	dispwin *_dw;			/* Privare window if not already open */
}; typedef struct _disp_win_info disp_win_info;

/* A defauult callback that can be provided as an argument to */
/* inst_handle_calibrate() to handle the display part of a */
/* calibration callback. */
/* Call this again with calc = inst_calc_none to cleanup afterwards. */
inst_code setup_display_calibrate(
	inst *p,
	inst_cal_cond calc,		/* Current condition. inst_calc_none for not setup */
	disp_win_info *dwi		/* Information to be able to open a display test patch */
);

/* User requested calibration of the display instrument */
int disprd_calibration(
icompath *ipath,	/* Instrument path to open, &icomFakeDevice == fake */
flow_control fc,	/* Serial flow control */
int dtype,			/* Display type, 0 = unknown, 1 = CRT, 2 = LCD */
int sdtype,			/* Spectro dtype, use dtype if -1 */
int docbid,			/* NZ to only allow cbid dtypes */
int tele,			/* NZ for tele mode */
int nadaptive,		/* NZ for non-adaptive mode */
int noinitcal,		/* NZ to disable initial instrument calibration */
disppath *screen,	/* Screen to calibrate. */
int webdisp,		/* If nz, port number for web display */
ccast_id *ccid,		/* non-NULL for ChromeCast */
#ifdef NT
int madvrdisp,		/* NZ for MadVR display */
#endif
int out_tvenc,		/* 1 = use RGB Video Level encoding */
int blackbg,		/* NZ if whole screen should be filled with black */
int override,		/* Override_redirect on X11 */
double hpatsize,	/* Size of dispwin */
double vpatsize,
double ho, double vo,	/* Position of dispwin */
a1log *log			/* Verb, debug & error log */
);


/* A color structure to return values with. */
/* This can hold all representations simultaniously */
typedef struct {
	double r,g,b;
	char *id;			/* Id string */

	inst_meas_type mtype;	/* Measurement type */

	int    XYZ_v;
	double XYZ[3];		/* Colorimeter readings */

	xspect sp;			/* Spectrum. sp.spec_n > 0 if valid */

	double duration;	/* Total duration in seconds (flash measurement) */

	int serno;			/* Reading serial number */
	unsigned int msec;	/* Timestamp */
} col;


/* Maximum number of entries to setup for calibration */
#define MAX_CAL_ENT 4096

/* Display reading context */
struct _disprd {

/* private: */
	a1log *log;			/* Verb, debug & error log */
	int fake;			/* Fake display/instrument flag */
	int fake2;			/* Flag to apply extra matrix to fake response */
	char *fake_name;	/* Fake profile name */
	icmFile *fake_fp;
	icc *fake_icc;		/* NZ if ICC profile is being used for fake */
	int native;			/* X0 = use current per channel calibration curve */
						/* X1 = set native linear output and use ramdac high precn. */
						/* 0X = use current color management cLut (MadVR) */
						/* 1X = disable color management cLUT (MadVR) */
	double cal[3][MAX_CAL_ENT];	/* Calibration being worked through (cal[0][0] < 0.0 or NULL if not used) */
	int ncal;			/* Number of entries used in cal[] */
	icmLuBase *fake_lu;
	char *mcallout;		/* fake instrument shell callout */
//	char *scallout;		/* measurement XYZ value callout */
	icompath *ipath;	/* Instrument path to open, &icomFakeDevice == fake */
	baud_rate br;
	flow_control fc;
	inst *it;			/* Instrument */
	int dtype;			/* Display type, 0 = unknown, 1 = CRT, 2 = LCD */
	int sdtype;			/* Spectro dtype */
	int docbid;			/* NZ to only allow cbid dtypes */
	int refrmode;		/* Refresh display mode, -1 if unknow, 0 = if no, 1 if yes */
	int cbid;			/* The current Calibration Base display mode ID, 0 if unknown */
	int tele;			/* NZ for tele mode */
	int nadaptive;		/* NZ for non-adaptive mode */
	int highres;		/* Use high res mode if available */
	double refrate;		/* If != 0.0, set display refresh rate calibration */
	int update_delay_set;	/* NZ if we've calibrated the disp. update delay, or tried and failed */
	disptech cc_dtech;	/* Display tech used with ccmtx or sets */
	int cc_cbid;		/* cbid to be used with ccmtx * or sets */
	double (*ccmtx)[3];	/* Colorimeter Correction Matrix, NULL if none */
	icxObserverType obType;	/* CCSS Observer */
	xspect *custObserver;	/* CCSS Optional custom observer */
	xspect *sets;		/* CCSS Set of sample spectra, NULL if none  */
	int no_sets;	 	/* CCSS Number on set, 0 if none */
	int spectral;		/* 1 = Generate spectral info flag, 2 = don't print error if not capable */
	icxObserverType observ;		/* Compute XYZ from spectral if spectral and != icxOT_none */
	xsp2cie *sp2cie;	/* Spectral to XYZ conversion */
	int bdrift;			/* Flag, nz for black drift compensation */
	int wdrift;			/* Flag, nz for white drift compensation */
	int noinitcal;		/* No initial instrument calibration */
	int noinitplace;	/* Don't wait for user to place instrument on screen */
	dispwin *dw;		/* Window */

	int serno;			/* Reading serial number */
	col ref_bw[2];   	/* Reference black and white readings for drift comp. */
	int ref_bw_v;		/* Reference valid flag */
	col last_bw[2];   	/* Last black and white readings for drift comp. */
	int last_bw_v;		/* Last valid flag */
	col targ_w;   		/* Target white to normalise to. last_bw[1] for batch, first white for */
						/* non-batch, but latter can be reset. */
	int targ_w_v;		/* target_w valid flag */

/* public: */

	/* Destroy ourselves */
	void (*del)(struct _disprd *p);

	/* Take a series of readings from the display */
	/* return nz on fail/abort */
	/* 1 = user aborted */
	/* 2 = instrument access failed */
	/* 3 = window access failed */ 
	/* 4 = user hit terminate key */
	/* 5 = system error */
	int (*read)(struct _disprd *p,
		col *cols,		/* Array of patch colors to be tested */
		int npat, 		/* Number of patches to be tested */
		int spat,		/* Start patch index for "verb", 0 if not used */
		int tpat,		/* Total patch index for "verb", 0 if not used */
		int acr,		/* If nz, do automatic final carriage return */
		int tc,			/* If nz, termination key */
		instClamping clamp	/* NZ if clamp XYZ/Lab to be +ve */
	);

	/* Return the display type information */
	void (*get_disptype)(struct _disprd *p, int *refrmode, int *cbid);

	/* Reset the white drift target white value, for non-batch */
	/* readings when white drift comp. is enabled */
	void (*reset_targ_w)(struct _disprd *p);

	/* Change the black/white drift compensation options */
	/* Note that this simply invalidates any reference readings, */
	/* and therefore will not make for good black compensation */
	/* if it is done a long time since the instrument calibration. */
	void (*change_drift_comp)(struct _disprd *p,
		int bdrift,			/* Flag, nz for black drift compensation */
		int wdrift			/* Flag, nz for white drift compensation */
	);

	/* Take an ambient reading if the instrument has the capability. */
	/* return nz on fail/abort */
	/* 1 = user aborted */
	/* 2 = instrument access failed */
	/* 3 = no ambient capability */ 
	/* 4 = user hit terminate key */
	/* 5 = system error */
	/* 8 = no ambient capability */
	int (*ambient)(struct _disprd *p,
		double *ambient,	/* return ambient in cd/m^2 */
		int tc				/* If nz, termination key */
	);

}; typedef struct _disprd disprd;


/* Create a display reading object. */
/* Return NULL if error */
/* Set *errc to code: */
/* 0 = no error */
/* 1 = user aborted */
/* 2 = instrument access failed */
/* 3 = window access failed */
/* 4 = user hit terminate key */
/* 5 = system error */
/* 6 = system error */
/* 7 = CRT or LCD must be selected */
/* 9 = spectral conversion failed */
/* 10 = no ccmx support */
/* 11 = no ccss support */
/* 12 = out_tvenc & native = 0 && no cal but current RAMDAC is not linear */
/* 13 = out_tvenc for MadVR window */
/* 14 = no set refresh rate support */
/* 15 = unknown calibration/display type */
/* 16 = non based calibration/display type */
/* Use disprd_err() to interpret errc */
disprd *new_disprd(
int *errc,			/* Error code. May be NULL */ 
icompath *ipath,	/* Instrument path to open, &icomFakeDevice == fake */
flow_control fc,	/* Serial flow control */
int dtype,			/* Display type, 0 = unknown, 1 = CRT, 2 = LCD */
int sdtype,			/* Spectro dtype, use dtype if -1 */
int docbid,			/* NZ to only allow cbid dtypes */
int tele,			/* NZ for tele mode */
int nadaptive,		/* NZ for non-adaptive mode */
int noinitcal,		/* No initial instrument calibration */
int noinitplace,	/* Don't wait for user to place instrument on screen */
int highres,		/* Use high res mode if available */
double refrate,		/* If != 0.0, set display refresh rate calibration */
int native,			/* X0 = use current per channel calibration curve */
					/* X1 = set native linear output and use ramdac high precn. */
					/* 0X = use current color management cLut (MadVR) */
					/* 1X = disable color management cLUT (MadVR) */
int *noramdac,		/* Return nz if no ramdac access. native is set to X0 */
int *nocm,			/* Return nz if no CM cLUT access. native is set to 0X */
double cal[3][MAX_CAL_ENT],	/* Calibration set (cal = NULL or cal[0][0] < 0.0 if not to be used) */
int ncal,			/* number of entries use in cal */
disppath *screen,	/* Screen to calibrate. */
int out_tvenc,		/* 1 = use RGB Video Level encoding */
int blackbg,		/* NZ if whole screen should be filled with black */
int override,		/* Override_redirect on X11 */
int webdisp,		/* If nz, port number for web display */
ccast_id *ccid,		/* non-NULL for ChromeCast */
#ifdef NT
int madvrdisp,		/* NZ for MadVR display */
#endif
char *ccallout,		/* Shell callout on set color */
char *mcallout,		/* Shell callout on measure color (forced fake) */
//char *scallout,		/* Shell callout on results of measure color */
double hpatsize,	/* Size of dispwin */
double vpatsize,
double ho,			/* Horizontal offset */
double vo,			/* Vertical offset */
disptech cc_dtech,	/* Display tech to go with ccmtx or sets, disptech_unknown if not used */
int cc_cbid,		/* cbid to go with ccmtx or sets, 0 if not used */
double ccmtx[3][3],	/* Colorimeter Correction matrix, NULL if none */
xspect *sets,		/* CCSS Set of sample spectra, NULL if none  */
int no_sets,	 	/* CCSS Number on set, 0 if none */
int spectral,		/* 1 = Generate spectral info flag, 2 = don't print error if not capable */
icxObserverType obType,	/* Use alternate observer if spectral or CCSS and != icxOT_none */
xspect custObserver[3],	/* Optional custom observer */
int bdrift,			/* Flag, nz for black drift compensation */
int wdrift,			/* Flag, nz for white drift compensation */
char *fake_name,	/* Name of profile to use as a fake device */
a1log *log			/* Verb, debug & error log */
);
/* Return a string describing the error code */
char * disprd_err(int en);

#define DISPSUP_H
#endif /* DISPSUP_H */

