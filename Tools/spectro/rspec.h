#ifndef RSPEC_H

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   20015
 *
 * Copyright 2006 - 2015 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 * 
 * Derived from i1pro_imp.c & munki_imp.c
 */

/*
 * A library for processing raw spectrometer values.
 *
 * Currently this is setup for the EX1 spectrometer,
 * but the longer term plan is to expand the functionality
 * so that it becomes more generic, and can replace a lot 
 * of common code in i1pro_imp.c & munki_imp.c.
 */

#ifdef __cplusplus
	extern "C" {
#endif

#define RSPEC_MAXSAMP 2048

/* - - - - - - - - - - - - - */
/* Collection of raw samples */
typedef enum {
	rspec_sensor,		/* Includes shielded/temperature values */
	rspec_raw,			/* Potential light values */
	rspec_wav			/* Valid wavelength values */
} rspec_type;

/* The order the state is changed in, is device workflow dependent */
typedef enum {
	rspec_none = 0x0000,	/* No processing */
	rspec_shld = 0x0002,	/* Shielded cell corrected */
	rspec_dcal = 0x0004,	/* Dark calibration subtracted */
	rspec_lin  = 0x0010,	/* Linearized */
	rspec_int  = 0x0020,	/* Integration time adjusted */
	rspec_temp = 0x0001,	/* Temperature corrected */
	rspec_cal  = 0x0040		/* Calibrated */
} rspec_state;

struct _rspec {
	struct _rspec_inf *inf;		/* Base information */

	rspec_type     stype;	/* Spectral type - sensor, raw, cooked */
	inst_meas_type mtype;	/* Measurement type (emis, ambient, reflective etc.) */
	rspec_state    state;	/* Processing state */

	double inttime;		/* Integration time */

	int nmeas;			/* Number of measurements */
	int nsamp;			/* Number of sensor/wavelength samples */
	double **samp;		/* [mesa][samples] allocated using numlib. */

}; typedef struct _rspec rspec;


/* - - - - - - - - - - - - - */
/* Base information about characteristics in a given mode */

/* Wavelength resample kernel type */
typedef enum {
	rspec_triangle,
	rspec_gausian,
	rspec_lanczos2,
	rspec_lanczos3,
	rspec_cubicspline
} rspec_kernel;

/* Group of values */
typedef struct {
	int off;		/* Offset to start of group */
	int num;		/* Number in group */
} rspec_group;

struct _rspec_inf {
	a1log *log;

	int nsen;					/* Number of sensor values */
	int nshgrps;				/* Number of shielded sensor groups */
	rspec_group shgrps[2];		/* Shielded group definition */

	int nilltkgrps;				/* Number of illuminant level tracking groups */
	rspec_group illtkgrps[2];	/* illuminant level tracking groups definition */

	rspec_group lightrange;		/* Range of sensor potential light values transferred to raw */ 

	int nraw;					/* Number of raw light sensor values */
	rspec_group rawrange;		/* Valid range of raw values for filter to wav */

	rspec_kernel ktype;			/* Re-sampling kernel type */
	int nwav;					/* Cooked spectrum bands */
	double wl_space;			/* Wavelength spacing */
	double wl_short;			/* Cooked spectrum bands short wavelength nm */
	double wl_long;				/* Cooked spectrum bands short wavelength nm */


								/* (Stray light is not currently implemented) */
	rspec_type straytype;		/* Stray light spectral type - sensor, raw, cooked */
	double **straylight;		/* [][] Stray light convolution matrix (size ??) */

	/* raw index to wavelength polynomial */
	unsigned int nwlcal;		/* Number in array */
	double *wlcal;				/* Array of wavelenght cal polinomial factors. */

	/* raw index to wavelength re-sampling filters */
	int *findex;				/* [nwav] raw starting index for each out wavelength */
	int *fnocoef; 				/* [nwav] Number of matrix cooeficients for each out wavelength */
	double *fcoef;				/* [nwav * nocoef] Packed cooeficients to compute each wavelength */

	unsigned int nlin;			/* Number in array */
	double *lin;				/* Array of linearisation polinomial factors. */
	int lindiv;					/* nz if polinomial result should be divided */

	/* Black calibration */
	rspec *idark[2];			/* Adaptive dark cal for two integration times */

	/* Emission calibration */
	rspec_type ecaltype;		/* Emissioni calibration type - sensor, raw, cooked */
	double *ecal;				/* Emission calibration values */

}; typedef struct _rspec_inf rspec_inf;

/* - - - - - - - - - - - - - */

/* Completely clear an rspec_inf. */
void clear_rspec_inf(rspec_inf *inf);

/* Completely free contents of rspec_inf. */
void free_rspec_inf(rspec_inf *inf);

/* return the number of samples for the given spectral type */
int rspec_typesize(rspec_inf *inf, rspec_type ty);

/* Compute the valid raw range from the calibration information */
void rspec_comp_raw_range_from_ecal(rspec_inf *inf);

/* Convert a raw index to nm */
double rspec_raw2nm(rspec_inf *inf, double rix);

/* Convert a cooked index to nm */
double rspec_wav2nm(rspec_inf *inf, double ix);

/* Create the wavelength resampling filters */
void rspec_make_resample_filters(rspec_inf *inf);


/* Plot the first rspec */
void plot_rspec1(rspec *p);

/* Plot the first rspec of 2 */
void plot_rspec2(rspec *p1, rspec *p2);

/* Plot the wave resampling filters */
void plot_resample_filters(rspec_inf *inf);

/* Plot the calibration curves */
void plot_ecal(rspec_inf *inf);

/* - - - - - - - - - - - - - */

/* Create a new rspec from scratch */
/* This always succeeds (i.e. application bombs if malloc fails) */
rspec *new_rspec(rspec_inf *inf, rspec_type ty, int nmeas);

/* Create a new rspec based on an existing prototype */ 
/* If nmes == 0, create space for the same number or measurements */
rspec *new_rspec_proto(rspec *rs, int nmeas);

/* Create a new rspec by cloning an existing one */
rspec *new_rspec_clone(rspec *rs);

/* Free a rspec */
void del_rspec(rspec *rs);

/* - - - - - - - - - - - - - */
/* Return the largest value */
double largest_val_rspec(int *pmix, int *psix, rspec *raw);

/* return a raw rspec from a sensor rspec */
rspec *extract_raw_from_sensor_rspec(rspec *sens);

/* Return an interpolated dark reference value from idark */
double ex1_interp_idark_val(rspec_inf *inf, int mix, int six, double inttime);

/* Return an interpolated dark reference from idark */
rspec *ex1_interp_idark(rspec_inf *inf, double inttime);

/* Subtract the adaptive black */
void subtract_idark_rspec(rspec *raw);

/* Apply non-linearity to a single value */
double linearize_val_rspec(rspec_inf *inf, double ival);

/* Invert non-linearity of a single value */
double inv_linearize_val_rspec(rspec_inf *inf, double targv);

/* Correct non-linearity */
void linearize_rspec(rspec *raw);

/* Apply the emsissive calibration */
void emis_calibrate_rspec(rspec *sens);

/* Scale to the integration time */
void inttime_calibrate_rspec(rspec *sens);

/* return a wav rspec from a raw rspec */
rspec *convert_wav_from_raw_rspec(rspec *sens);


/* - - - - - - - - - - - - - */
/* Calibration file support */

typedef struct {
	a1log *log;
	int lo_secs;			/* Seconds since last opened (from file mod time) */ 
	FILE *fp;
	int rd;					/* 0 = dummy read, 1 = real read */
	int ef;					/* Error flag, 1 = write failed, 2 = close failed */
	unsigned int chsum;		/* Checksum */
	int nbytes;				/* Number of bytes checksummed */

	char *buf;				/* Dummy read buffer */
	size_t bufsz;			/* Size of dumy read buffer */
} calf;

int calf_open(calf *x, a1log *log, char *fname, int wr);
void calf_rewind(calf *x);
int calf_touch(a1log *log, char *fname);
int calf_done(calf *x);

void calf_wints(calf *x, int *dp, int n);
void calf_wdoubles(calf *x, double *dp, int n);
void calf_wtime_ts(calf *x, time_t *dp, int n);
void calf_wstrz(calf *x, char *dp);

void calf_rints(calf *x, int *dp, int n);
void calf_rints2(calf *x, int *dp, int n);
void calf_rdoubles(calf *x, double *dp, int n);
void calf_rtime_ts(calf *x, time_t *dp, int n);
void calf_rstrz(calf *x, char **dp);
void calf_rstrz2(calf *x, char **dp);

/* Save a rspec to a calibration file */
void calf_wrspec(calf *x, rspec *dp);

/* Restore a rspec from a calibration file */
void calf_rrspec(calf *x, rspec **dp, rspec_inf *inf);

#ifdef __cplusplus
	}
#endif

#define RSPEC_H
#endif /* RSPEC_H */
