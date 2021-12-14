
/*
 * Argyll Color Management System
 *
 * Gretag i1Pro implementation functions
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


/*
	Notes:

	Naming of spectral values:

		sensor  - the 16 bit values from the sensor including any dummy/shielded/other values
		raw		- the floating point values of the spectral section 
		absraw	- raw after scaling for integration time and gain settings.


	For an RGB LED display, the i1Pro3 emissive sensitivity is
	aproximately 225 max raw counts per int. second per cd/m^2.
	Target is 45000 max, and saturation is about 53400.

	For an 'A' type source it is about 99.2

	cd/m^2 = count / (sensitivity * int_time)

	The current i1Pro3(LA) instruments appear to have identical emissive sensitivity
	as the i1Pro3(SA).

	Hi-resolution mode is almost certainly less accurate than standard resolution.
	For emissive measurement this is because the diffraction grating 
	has a rather bumpy spectral response, so that up-sampling the reference
	correction is not very accurate. Unlike the i1pro2 though, the illuminant
	(being LED) is also bumpy, and can't be uses as a smooth reference to
	correct these errors. (There is a static correction which will only be
	accurate for my specific instrument.)
	For reflective measurement a high-res result is unusable due to the
	complex and intricate calibration and measurement processing
	that is uses to compute M0, M1 and M2 responses. For this reason
	it is disabled in reflection measurement mode.
	(The reflection measurement could be re-worked along the lines
	 of ArgyllCMS FWA correction so as to support hi-res., but it would
	 almost certainly not exactly match the results of the manufacturers driver
	 for FWA/OBE papers.)

 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <math.h>
#if defined(UNIX)
# include <utime.h>
#else
# include <sys/utime.h>
#endif
#include <sys/stat.h>
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "rspl.h"
#else /* SALONEINSTLIB */
#include <fcntl.h>
#include "sa_config.h"
#include "numsup.h"
#include "rspl1.h"
#endif /* SALONEINSTLIB */
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "sort.h"

/* Configuration */
#define USE_THREAD				/* [def] Use thread to wait for events from the instrument */
#undef WAIT_FOR_DELAY_TRIGGER	/* [und] Hack to diagnose threading problems */
#define ENABLE_NONVCAL			/* [def] Enable saving calibration state to a file */
#define ENABLE_ZEBRA			/* [def] Enable Zebra ruler */
#define ENABLE_DYNBLKTC			/* [def] Enable dynamic black temperature compensation */
#define STRICT_IMITATE			/* [und] Imitate foibles of manufacturers driver */
#define HIGH_RES		/* [def] Enable high resolution spectral mode code. Disable */
						/* to break dependency on rspl library. */

#define CALTEMPLIM  10.0			/* [10.0] Calibration max temperature delta */
#define WLCALTOUT (24 * 60 * 60)	/* [24 Hrs] Wavelength calibration timeout in seconds */
#define DCALTOUT  ( 1 * 60 * 60)	/* [1 Hr] i1pro3 Dark Calibration timeout in seconds */
#define WCALTOUT  ( 1 * 60 * 60)	/* [1 Hr] White Calibration timeout in seconds */

#define MAXSCANTIME 30.0	/* [30] Maximum scan time in seconds */
#define SW_THREAD_TIMEOUT	(10 * 60.0) 	/* [10 Min] Switch read thread timeout */

#undef D_PLOT			/* [und] Use plots to show EE info for -D7 or higher */
#undef D_STRAYPLOT		/* [und] Use plots to show EE info for -D7 or higher */
#undef DEBUG			/* [und] Turn on debug printfs */
#undef PLOT_DEBUG		/* [und] Use plot to show readings & processing */
#undef PLOT_REFRESH 	/* [und] Plot refresh rate measurement info */
#undef PLOT_UPDELAY		/* [und] Plot data used to determine display update delay */
#undef DUMP_SCANV		/* [und] Dump scan readings to a file "i1pdump.txt" */
#undef DUMP_DARKM		/* [und] Append raw dark readings to file "i1pddump.txt" */
#undef APPEND_MEAN_EMMIS_VAL /* [und] Append averaged uncalibrated reading to file "i1pdump.txt" */
#undef TEST_DARK_INTERP    /* [und] Test out the dark interpolation (need DEBUG for plot) */
#undef PATREC_DEBUG			/* [und] Print & Plot patch/flash recognition information */
#undef PATREC_PLOT_ALLBANDS	/* [und] Plot all bands of scan */
#undef PATREC_SAVETRIMMED	/* [und] Saved trimmed raw to file "i1pro3_raw_trimed_N.csv */
#undef IGNORE_WHITE_INCONS	/* [und] Ignore define reference reading inconsistency */
#undef HIGH_RES_PLOT		/* [und] Plot created hi-res filters */
#undef HIGH_RES_PLOT_WAVFILT	/* [und] High resolution raw2wav filters */
#undef HIGH_RES_PLOT_STRAYL	/* [und] High resolution stray value upsample result */
#undef FAKE_EEPROM				/* Get [und] EEPROM data from i1pro3_fake_eeprom.h */

#define NAE_INTT 1.8			/* Default starting integration time for emis. non-adaptive */
								/* This sets the initial maximum to about 110 cd/m^2 */

#define MAX_INT_TIME 2.0		/* [2.0] Maximum integration time, sets adaptive max */
								/* (There are very slight improvements in S/N with longer max) */

/* High res mode settings */
#define HIGHRES_SHORT 380.0		/* [380] Same as std. res., since grating efficiency is poor */ 
#define HIGHRES_LONG  730.0		/* [730] outside this range. */
#define HIGHRES_WIDTH  (10.0/3.0) /* 3.3333 spacing seems a good choice */
#define HIGHRES_REF_MIN 380.0	  /* [380] Same as std. res. */


#define MX_NSEN 282				/* Maximum nsen value we can cope with */
#define MX_NRAW 128				/* Maximum nsen value we can cope with */
#define MX_NWAV 120				/* Maximum nsen value we can cope with (need 106) */

#if defined(DEBUG)					\
|| defined(D_PLOT)					\
|| defined(PLOT_DEBUG)				\
|| defined(PLOT_REFRESH)			\
|| defined(PLOT_UPDELAY)			\
|| defined(PATREC_DEBUG)			\
|| defined(HIGH_RES_PLOT)			\
|| defined(HIGH_RES_PLOT_WAVFILT)	\
|| defined(HIGH_RES_PLOT_STRAYL)		
# pragma message("######### i1pro3_imp.c DEBUG enabled !!!!! ########")
# include <plot.h>
#endif

/* - - - - - - - - - - - - - - - - - - */

#include "i1pro3.h"
#include "i1pro3_imp.h"
#include "xrga.h"

/* - - - - - - - - - - - - - - - - - - */
#define LED_OFF_TIME 1000		/* msec to allow LEDS to cool before illuminated measure ? */
#define LEDTURNONMEAS 77		/* Number of native reflective measurements to throw away */
								/* to allow the illumination LEDs to come up to temperature ? */

#define USE_RD_SYNC				/* [def] Use mutex syncronisation, else depend on TRIG_DELAY */
#define TRIG_DELAY 20			/* [20] Measure trigger delay to allow pending read, msec */

#define PATCH_CONS_THR 0.08		/* Default measurement consistency threshold */
#define MIN_SAMPLES 6			/* [6] Minimum number of scan samples in a patch */
#define POL_MIN_SAMPLES 4		/* [4] Be more generous due to lower sampling rate */

/* - - - - - - - - - - - - - - - - - - */

#if defined(DEBUG) || defined(PLOT_DEBUG) || defined(PATREC_DEBUG)
# include <plot.h>
#endif


#if defined(DEBUG) || defined(PLOT_DEBUG) || defined(HIGH_RES_PLOT) || defined(PATREC_DEBUG)
static int disdebplot = 0;

#define DISDPLOT disdebplot = 1;
#define ENDPLOT disdebplot = 0;

#else

#define DISDPLOT 
#define ENDPLOT 
	
#endif	/* DEBUG */

#if defined(DEBUG) || defined(PLOT_DEBUG) || defined(PATREC_DEBUG)
/* ============================================================ */
/* Debugging support */

/* Plot three CCD spectra */
static void plot_raw3(double *y1, double *y2, double *y3) {
	int i;
	double xx[128];

	if (disdebplot)
		return;

	for (i = 0; i < 128; i++)
		xx[i] = (double)i;
	do_plot(xx, y1, y2, y3, 128);
}

/* Plot a CCD spectra */
static void plot_raw(double *data) {
	plot_raw3(data, NULL, NULL);
}

/* Plot two CCD spectra */
static void plot_raw2(double *data1, double *data2) {
	plot_raw3(data1, data2, NULL);
}


/* Linear interpolate a wav. Return cv value on clip */
static double wav_lerp_cv(i1pro3imp *m, int hires, double *ary, double wl, double cv) {
	int jj;
	double wl0, wl1, bl;
	double rv;

	jj = (int)floor(XSPECT_DIX(m->wl_short[hires], m->wl_long[hires], m->nwav[hires], wl));
	if (jj < 0)
		jj = 0;
	else if (jj > (m->nwav[hires]-2))
		jj = m->nwav[hires]-2;

	wl0 = XSPECT_WL(m->wl_short[hires], m->wl_long[hires], m->nwav[hires], jj); 
	wl1 = XSPECT_WL(m->wl_short[hires], m->wl_long[hires], m->nwav[hires], jj+1); 

	bl = (wl - wl0)/(wl1 - wl0);
	if (bl < 0.0 || bl > 1.0)
		return cv;

	rv = (1.0 - bl) * ary[jj] + bl * ary[jj+1];

	return rv;
}

/* Plot a converted spectra */
static void plot_wav(i1pro3imp *m, int hires, double *data) {
	int i;
	double xx[128];
	double yy[128];

	if (disdebplot)
		return;

	for (i = 0; i < m->nwav[hires]; i++) {
		xx[i] = XSPECT_WL(m->wl_short[hires], m->wl_long[hires], m->nwav[hires], i);
		yy[i] = data[i];
	}
	do_plot(xx, yy, NULL, NULL, m->nwav[hires]);
}

/* Plot two converted spectra for the current res. mode */
void plot_wav2(i1pro3imp *m, int hires, double *data1, double *data2) {
	int i;
	double xx[128];
	double y1[128];
	double y2[128];

	if (disdebplot)
		return;

	for (i = 0; i < m->nwav[hires]; i++) {
		xx[i] = XSPECT_WL(m->wl_short[hires], m->wl_long[hires], m->nwav[hires], i);
		y1[i] = data1[i];
		y2[i] = data2[i];
	}
	do_plot(xx, y1, y2, NULL, m->nwav[hires]);
}

/* Plot three converted spectra for the current res. mode */
void plot_wav3(i1pro3imp *m, int hires, double *data1, double *data2, double *data3) {
	int i;
	double xx[128];
	double y1[128];
	double y2[128];
	double y3[128];

	if (disdebplot)
		return;

	for (i = 0; i < m->nwav[hires]; i++) {
		xx[i] = XSPECT_WL(m->wl_short[hires], m->wl_long[hires], m->nwav[hires], i);
		y1[i] = data1[i];
		y2[i] = data2[i];
		y3[i] = data3[i];
	}
	do_plot(xx, y1, y2, y3, m->nwav[hires]);
}

/* Plot N converted spectra for the current res. mode */
static void plot_wav_N(i1pro3imp *m, int hires, double **data, int nwav) {
	int i, j, k;
	double xx[128];
	double *yy[MXGPHS];

	if (disdebplot)
		return;

	for (j = 0; j < m->nwav[hires]; j++)
		xx[j] = XSPECT_WL(m->wl_short[hires], m->wl_long[hires], m->nwav[hires], j);

	for (i = 0; i < nwav; i += MXGPHS) {
		
		for (k = 0; k < MXGPHS; k++) {
			if ((i + k) >= nwav)
				yy[k] = NULL;
			else
				yy[k] = data[i+k];
		}
		do_plotNpwz(xx, yy, m->nwav[hires], NULL, NULL, 0, 1, 0);
	}
}

/* Plot std and high res wav spectra */
void plot_wav_sh(i1pro3imp *m, double *std, double *hi) {
		int j;
		double xx[MX_NWAV], y1[MX_NWAV], y2[MX_NWAV];

		for (j = 0; j < (m->nwav[1]); j++) {
			xx[j] = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], j);
			y1[j] = wav_lerp_cv(m, 0, std, xx[j], 0.0);
			y2[j] = hi[j];
		}
		do_plot(xx, y1, y2, NULL, m->nwav[1]);
	}

#endif	/* PLOT_DEBUG */


/* ============================================================ */

/* Implementation struct */

/* Add an implementation structure */
i1pro3_code add_i1pro3imp(i1pro3 *p) {
	i1pro3imp *m;

	if ((m = (i1pro3imp *)calloc(1, sizeof(i1pro3imp))) == NULL) {
		a1logd(p->log,1,"add_i1pro3imp malloc %ld bytes failed (1)\n",sizeof(i1pro3imp));
		return I1PRO3_INT_MALLOC;
	}
	m->p = p;

	m->imode = inst_mode_ref_spot;
	m->mmode = i1p3_refl_spot;

	amutex_init(m->lock);			/* USB control port lock */
	m->lo_secs = 2000000000;		/* A very long time */
	m->msec = msec_time();

	p->m = (void *)m;
	return I1PRO3_OK;
}

/* Shutdown instrument, and then destroy */
/* implementation structure */
void del_i1pro3imp(i1pro3 *p) {

	a1logd(p->log,5,"i1pro3_del called\n");

#ifdef ENABLE_NONVCAL
	/* Touch it so that we know when the instrument was last open */
	i1pro3_touch_calibration(p);
#endif /* ENABLE_NONVCAL */

	if (p->m != NULL) {
		int i, j;
		i1pro3imp *m = (i1pro3imp *)p->m;
		i1pro3_state *s;
		i1pro3_code ev;

		/* Terminate event monitor thread */
		if (m->th != NULL) {
			m->th_term = 1;			/* Tell thread to exit on error */
			i1pro3_terminate_event(p);
			
			for (i = 0; m->th_termed == 0 && i < 5; i++)
				msec_sleep(50);		/* Wait for thread to terminate */
			if (i >= 5) {
				a1logd(p->log,5,"i1pro3 event thread termination failed\n");
				m->th->terminate(m->th);	/* Try and force thread to terminate */
			}
			m->th->del(m->th);
			usb_uninit_cancel(&m->sw_cancel);		/* Don't need cancel token now */
			a1logd(p->log,5,"i1pro3 event thread terminated\n");
		}

		if (m->trig_thread != NULL) {
			m->trig_thread->del(m->trig_thread);
			a1logd(p->log,5,"i1pro3 trigger thread terminated\n");
		}
		usb_uninit_cancel(&m->rd_sync);			/* Don't need sync token now */
		usb_uninit_cancel(&m->rd_sync2);		/* Don't need sync token now */

		/* Free any per mode data */
		for (i = 0; i < i1p3_no_modes; i++) {
			s = &m->ms[i];

			free_dmatrix(s->idark_data, 0, 1, -1, m->nraw-1);  

			free_dvector(s->cal_factor[0], 0, m->nwav[0]-1);
			free_dvector(s->cal_factor[1], 0, m->nwav[1]-1);
			free_dvector(s->raw_white, -1, m->nraw-1);

			free_dvector(s->calsp_illcr[0][0], 0, m->nwav[0]-1);
			free_dvector(s->calsp_illcr[1][0], 0, m->nwav[1]-1);
			free_dvector(s->calsp_illcr[0][1], 0, m->nwav[0]-1);
			free_dvector(s->calsp_illcr[1][1], 0, m->nwav[1]-1);
			free_dvector(s->iavg2aillum[0], 0, m->nwav[0]-1);
			free_dvector(s->iavg2aillum[1], 0, m->nwav[1]-1);
			free_dvector(s->sc_calsp_nn_white[0], 0, m->nwav[0]-1);
			free_dvector(s->sc_calsp_nn_white[1], 0, m->nwav[1]-1);
			free_dvector(s->cal_l_uv_diff[0], 0, m->nwav[0]-1);
			free_dvector(s->cal_l_uv_diff[1], 0, m->nwav[1]-1);
			free_dvector(s->cal_s_uv_diff[0], 0, m->nwav[0]-1);
			free_dvector(s->cal_s_uv_diff[1], 0, m->nwav[1]-1);

			free_dvector(s->pol_calsp_white[0], 0, m->nwav[0]-1);
			free_dvector(s->pol_calsp_white[1], 0, m->nwav[1]-1);
		}

		/* Free all high res cal data */
		free_dvector(m->m0_fwa[1], 0, m->nwav[1]-1);
		free_dvector(m->m1_fwa[1], 0, m->nwav[1]-1);
		free_dvector(m->m2_fwa[1], 0, m->nwav[1]-1);
		free_dvector(m->fwa_cal[1], 0, m->nwav[1]-1);
		free_dvector(m->fwa_std[1], 0, m->nwav[1]-1);

		/* Free all high res raw2wav resampling filters */
		for (i = 0; i < 2; i++) {
			for (j = 0; j < 2; j++) {
				if (m->mtx[i][j].index != NULL)
					free(m->mtx[i][j].index);
				if (m->mtx[i][j].nocoef != NULL)
					free(m->mtx[i][j].nocoef);
				if (m->mtx[i][j].coef != NULL)
					free(m->mtx[i][j].coef);
			}
		}

		/* Free straylight arrays */
		for (i = 0; i < 2; i++) {
			if (m->straylight[i] != NULL)
				free_dmatrix(m->straylight[i], 0, m->nwav[i]-1, 0, m->nwav[i]-1);  
		}

		/* Free allocated eeprom data */
		free_dvector(m->white_ref[1], 0, m->nwav[1]-1);
		free_dvector(m->emis_coef[1], 0, m->nwav[1]-1);
		free_dvector(m->amb_coef[1], 0, m->nwav[1]-1);

		free(m);
		p->m = NULL;
	}
}

/* ============================================================ */
/* High level functions */

#ifdef FAKE_EEPROM
# pragma message("######### i1pro3_imp.c FAKE EEPROM compiled !!!!! ########")
# include "i1pro3_fake_eeprom.h"
#endif

#ifndef NEVER
void i1pro3_test_refl(i1pro3 *p);			/* test code */
void i1pro3_test_led_instr(i1pro3 *p);		/* test code */
void i1pro3_test_measure_instr(i1pro3 *p);	/* test code */
void i1pro3_test_emiscalib(i1pro3 *p);		/* test code */
#endif

/* Initialise our software state from the hardware */
i1pro3_code i1pro3_imp_init(i1pro3 *p) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_code ev = I1PRO3_OK;
	unsigned char *eeprom;	/* EEProm contents */
	char *envv;

	a1logd(p->log,5,"i1pro3_init:\n");


	m->native_calstd = xcalstd_xrga;
	m->target_calstd = xcalstd_native;		/* Default to native calibration */

	/* Honor Environment override */
	if ((envv = getenv("ARGYLL_XCALSTD")) != NULL) {
		if (strcmp(envv, "XRGA") == 0)
			m->target_calstd = xcalstd_xrga;
		else if (strcmp(envv, "XRDI") == 0)
			m->target_calstd = xcalstd_xrdi;
		else if (strcmp(envv, "GMDI") == 0)
			m->target_calstd = xcalstd_gmdi;
	}

	if (p->dtype != instI1Pro3)
		return I1PRO3_UNKNOWN_MODEL;

	m->trig = inst_opt_trig_user;
	m->scan_toll_ratio = 1.0;

	/* Take conservative approach to when the light was last on. */
	/* Assume it might have been on right before init was called again. */
	m->llamponoff = msec_time();

	usb_init_cancel(&m->sw_cancel);			/* Init event cancel token */
	usb_init_cancel(&m->rd_sync);			/* Init reading sync token */
	usb_init_cancel(&m->rd_sync2);			/* Init reading sync token */

	msec_sleep(100);

	/* Get the fw version */
	if ((ev = i1pro3_fwver(p, &m->fwver, m->fwvstr)) != I1PRO3_OK)
		return ev; 
	a1logd(p->log,2,"Firmware rev = %.2f, '%s'\n",m->fwver/100.0,m->fwvstr);

	/* Get other hw parameters */
	if ((ev = i1pro3_getparams(p, &m->minintclks, NULL, &m->intclkp)) != I1PRO3_OK)
		return ev; 
	a1logd(p->log,2,"Sub-clock divider = %d, integration clock = %f usec\n",
	                                          m->minintclks, 1e6 * m->intclkp);

/* Get EEPROM data from i1pro3_fake_eeprom.h */
#ifdef FAKE_EEPROM
	m->eesize = FAKE_EEPROM_SIZE;

	if ((eeprom = (unsigned char *)malloc(m->eesize)) == NULL) {
		a1logd(p->log,1,"Malloc %d bytes for eeprom failed\n",m->eesize);
		return I1PRO3_INT_MALLOC;
	}

	memcpy(eeprom, fake_eeprom_data, m->eesize);

#else
	/* Set the EEProm size (seems to be hard coded ?) */
	m->eesize = 16 * 1024;

	if ((eeprom = (unsigned char *)malloc(m->eesize)) == NULL) {
		a1logd(p->log,1,"Malloc %d bytes for eeprom failed\n",m->eesize);
		return I1PRO3_INT_MALLOC;
	}

	/* Read the EEProm */
	if ((ev = i1pro3_readEEProm(p, eeprom, 0, m->eesize)) != I1PRO3_OK) {
		free(eeprom);
		return ev;
	}
#endif /* FAKE_EEPROM */

	/* Get the Chip ID (This doesn't work until after reading the EEProm ?) */
	if ((ev = i1pro3_getchipid(p, m->hw_chipid)) != I1PRO3_OK) {
		free(eeprom);
		return ev; 
	}
	
	/* Parse the i1pro3 data */
	if ((ev = i1pro3_parse_eeprom(p, eeprom, m->eesize)) != I1PRO3_OK) {
		free(eeprom);
		return ev;
	}

	free(eeprom); eeprom = NULL;

	/* Make the default indicator state be off */
	if ((ev = i1pro3_indLEDoff(p)) != I1PRO3_OK)
		return ev; 

	/* Set the default LED currents */
	if ((ev = i1pro3_setledcurrents(p,
		m->ee_led_w_cur, m->ee_led_b_cur, m->ee_led_luv_cur, m->ee_led_suv_cur, m->ee_led_gwl_cur
	)) != I1PRO3_OK)
		return ev;
	
	/* Set up the current state of each mode */
	{
		int i, j;
		i1pro3_state *s;

		/* First set state to basic configuration */
		/* (We assume it's been zero'd) */
		for (i = 0; i < i1p3_no_modes; i++) {
			s = &m->ms[i];

			memset(s, 0, sizeof(i1pro3_state));		/* Default everything to zero */

			s->mode = i;

			/* Default to an emissive configuration */
			s->targoscale = 1.0;	/* Default full scale */

			s->want_wlcalib = 1;	/* Do an initial calibration */
			s->want_dcalib = 1;
			s->want_calib = 1;

			s->idark_data = dmatrixz(0, 1, -1, m->nraw-1);  

			s->idark_int_time[0] = m->min_int_time;
			s->idark_int_time[1] = MAX_INT_TIME > 4.0 ? 4.0 : MAX_INT_TIME;

			s->cal_factor[0] = dvectorz(0, m->nwav[0]-1);
			s->cal_factor[1] = dvectorz(0, m->nwav[1]-1);
			s->raw_white = dvectorz(-1, m->nraw-1);

			s->pol_calsp_white[0] = dvectorz(0, m->nwav[0]-1);
			s->pol_calsp_white[1] = dvectorz(0, m->nwav[1]-1);

			s->calsp_illcr[0][0] = dvectorz(0, m->nwav[0]-1);
			s->calsp_illcr[1][0] = dvectorz(0, m->nwav[1]-1);
			s->calsp_illcr[0][1] = dvectorz(0, m->nwav[0]-1);
			s->calsp_illcr[1][1] = dvectorz(0, m->nwav[1]-1);
			s->iavg2aillum[0] = dvectorz(0, m->nwav[0]-1);
			s->iavg2aillum[1] = dvectorz(0, m->nwav[1]-1);
			s->sc_calsp_nn_white[0] = dvectorz(0, m->nwav[0]-1);
			s->sc_calsp_nn_white[1] = dvectorz(0, m->nwav[1]-1);
			s->cal_l_uv_diff[0] = dvectorz(0, m->nwav[0]-1);
			s->cal_l_uv_diff[1] = dvectorz(0, m->nwav[1]-1);
			s->cal_s_uv_diff[0] = dvectorz(0, m->nwav[0]-1);
			s->cal_s_uv_diff[1] = dvectorz(0, m->nwav[1]-1);

			s->min_wl = HIGHRES_REF_MIN;		/* Same as std. res. */
		}

		/* Then add mode specific settings */
		for (i = 0; i < i1p3_no_modes; i++) {
			s = &m->ms[i];
			switch(i) {
				case i1p3_refl_spot:
					s->reflective = 1;
					s->dc_use = i1p3_dc_none;			/* Dark cal is on the fly */
					s->mc_use = i1p3_mc_reflective;		/* Reflective calibration block */

					s->inttime = m->min_int_time;	/* Maximize scan rate */

					{
						s->dcaltime  = 2 * 10 * m->min_int_time;
						s->wcaltime  = 2 * 22 * m->min_int_time;
						s->wscaltime = 2 * 323 *  m->min_int_time;	// First white calib is longer..
					}
					s->dreadtime = s->dcaltime;
					s->wreadtime = s->wcaltime;

					break;

				case i1p3_refl_spot_pol:
					s->reflective = 1;
					s->pol = 1;
					s->dc_use = i1p3_dc_none;			/* Dark cal is on the fly */
					s->mc_use = i1p3_mc_polreflective;	/* Pol. reflective calibration block */

					s->targoscale = 0.3;
					s->inttime = s->iinttime = 4.0 * m->min_int_time; /* Use 1/4 max scan rate */

					s->dcaltime  = 2 * 8 * 5 * m->min_int_time;		/* 8 measurements */
					s->wcaltime  = 2 * 17 * 5 * m->min_int_time;	/* 17 measurements */
					s->dreadtime = s->dcaltime;
					s->wreadtime = s->wcaltime;

					break;

				case i1p3_refl_scan:
					s->reflective = 1;
					s->scan = 1;
					s->dc_use = i1p3_dc_none;			/* Dark cal is on the fly */
					s->mc_use = i1p3_mc_reflective;		/* Reflective calibration block */

					s->inttime = m->min_int_time;	/* Maximize scan rate */

					{
						s->dcaltime  = 2 * 10 * m->min_int_time;
						s->wcaltime  = 2 * 22 * m->min_int_time;
						s->wscaltime = 2 * 323 *  m->min_int_time;	// First white calib is longer..
					}
					s->dreadtime = s->dcaltime;

					s->maxscantime = MAXSCANTIME;
					break;

				case i1p3_refl_scan_pol:
					s->reflective = 1;
					s->scan = 1;
					s->pol = 1;
					s->dc_use = i1p3_dc_none;			/* Dark cal is on the fly */
					s->mc_use = i1p3_mc_polreflective;	/* Pol. reflective calibration block */

					s->targoscale = 0.3;
					s->inttime = s->iinttime = 4.0 * m->min_int_time; /* Use 1/4 max scan rate */

					s->dcaltime  = 2 * 8 * 5 * m->min_int_time;		/* 8 measurements */
					s->wcaltime  = 2 * 17 * 5 * m->min_int_time;	/* 17 measurements */
					s->dreadtime = s->dcaltime;

					s->maxscantime = MAXSCANTIME;
					break;

				case i1p3_emiss_spot_na:			/* Emissive spot not adaptive */
					s->emiss = 1;
					s->adaptive = 0;
					s->dc_use = i1p3_dc_adaptive;
					s->mc_use = i1p3_mc_pcalfactor;

					vect_cpy(s->cal_factor[0], m->emis_coef[0], m->nwav[0]);
					vect_cpy(s->cal_factor[1], m->emis_coef[1], m->nwav[1]);
					s->cal_valid = 1;			/* Scale factor is valid */

					s->inttime = s->iinttime = NAE_INTT; /* Default disp integration time */

					s->dreadtime = 0.2;			/* Dark samples measurement time */  
					s->wreadtime = 2.0;			/* Minimum sample measurement time */
					s->dcaltime = 0.5;			/* Dark calibration (short) measurement time */
					s->dlcaltime = 4.0;			/* Dark calibration long measurement time */
					break;

				case i1p3_emiss_spot:			/* Emissive adaptive spot */
					s->emiss = 1;
					s->adaptive = 1;
					s->dc_use = i1p3_dc_adaptive;
					s->mc_use = i1p3_mc_pcalfactor;

					vect_cpy(s->cal_factor[0], m->emis_coef[0], m->nwav[0]);
					vect_cpy(s->cal_factor[1], m->emis_coef[1], m->nwav[1]);

					s->cal_valid = 1;			/* Scale factor is valid */

					s->inttime = m->min_int_time;

					s->dreadtime = 0.2;			/* Dark samples measurement time */  
					s->wreadtime = 2.0;			/* Minimum sample measurement time */
					s->dcaltime = 0.5;			/* Dark calibration (short) measurement time */
					s->dlcaltime = 4.0;			/* Dark calibration long measurement time */
					break;

				case i1p3_emiss_scan:
					s->emiss = 1;
					s->scan = 1;
					s->dc_use = i1p3_dc_adaptive;
					s->mc_use = i1p3_mc_pcalfactor;

					s->targoscale = 0.90;		/* Allow extra 10% margine for drift */

					vect_cpy(s->cal_factor[0], m->emis_coef[0], m->nwav[0]);
					vect_cpy(s->cal_factor[1], m->emis_coef[1], m->nwav[1]);
					s->cal_valid = 1;			/* Scale factor is valid */

					s->targoscale = 0.90;		/* Allow extra 10% margine for drift */

					s->inttime = s->iinttime = 2.0 * m->min_int_time;	/* Lower if needed */

					s->dreadtime = 0.2;			/* Dark samples measurement time */  
					s->dcaltime = 0.5;			/* Dark calibration (short) measurement time */
					s->dlcaltime = 4.0;			/* Dark calibration long measurement time */

					s->maxscantime = MAXSCANTIME;
					break;

				case i1p3_amb_spot:
					s->emiss = 1;
					s->ambient = 1;
					s->adaptive = 1;
					s->dc_use = i1p3_dc_adaptive;
					s->mc_use = i1p3_mc_pcalfactor;

					vect_cpy(s->cal_factor[0], m->emis_coef[0], m->nwav[0]);
					vect_mul(s->cal_factor[0], m->amb_coef[0], m->nwav[0]);
					vect_cpy(s->cal_factor[1], m->emis_coef[1], m->nwav[1]);
					vect_mul(s->cal_factor[1], m->amb_coef[1], m->nwav[1]);
					s->cal_valid = 1;			/* Scale factor is valid */

					s->inttime = m->min_int_time;

					s->dreadtime = 0.2;			/* Dark samples measurement time */  
					s->wreadtime = 2.0;			/* Minimum sample measurement time */
					s->dcaltime = 0.5;			/* Dark calibration (short) measurement time */
					s->dlcaltime = 4.0;			/* Dark calibration long measurement time */
					break;

				case i1p3_amb_flash:			/* This is intended for measuring flashes */
					s->emiss = 1;
					s->ambient = 1;
					s->scan = 1;
					s->flash = 1;
					s->dc_use = i1p3_dc_adaptive;
					s->mc_use = i1p3_mc_pcalfactor;

					vect_cpy(s->cal_factor[0], m->emis_coef[0], m->nwav[0]);
					vect_mul(s->cal_factor[0], m->amb_coef[0], m->nwav[0]);
					vect_cpy(s->cal_factor[1], m->emis_coef[1], m->nwav[1]);
					vect_mul(s->cal_factor[1], m->amb_coef[1], m->nwav[1]);
					s->cal_valid = 1;				/* Calibration is valid */

					s->inttime = m->min_int_time;	/* Maximize scan rate and max level */

					s->dreadtime = 0.2;			/* Dark samples measurement time */  
					s->dcaltime = 0.5;			/* Dark calibration (short) measurement time */
					s->dlcaltime = 4.0;			/* Dark calibration long measurement time */

					s->maxscantime = MAXSCANTIME;
					break;

				case i1p3_trans_spot:
					s->trans = 1;
					s->adaptive = 1;
					s->dc_use = i1p3_dc_adaptive;
					s->mc_use = i1p3_mc_mcalfactor;

					s->inttime = m->min_int_time;

					s->dreadtime = 0.2;			/* Dark samples measurement time */  
					s->wreadtime = 2.0;			/* Minimum sample measurement time */
					s->dcaltime = 0.5;			/* Dark calibration (short) measurement time */
					s->dlcaltime = 4.0;			/* Dark calibration long measurement time */
					break;

				case i1p3_trans_scan:
					s->trans = 1;
					s->scan = 1;
					s->adaptive = 0;
					s->dc_use = i1p3_dc_adaptive;
					s->mc_use = i1p3_mc_mcalfactor;

					s->targoscale = 0.90;		/* Allow extra 10% margine for drift */

					s->inttime = s->iinttime = 2.0 * m->min_int_time;	/* Lower if needed */

					s->dreadtime = 0.2;			/* Dark samples measurement time */  
					s->dcaltime = 0.5;			/* Dark calibration (short) measurement time */
					s->dlcaltime = 4.0;			/* Dark calibration long measurement time */

					s->maxscantime = MAXSCANTIME;
					break;
			}
		}
	}

#ifdef ENABLE_NONVCAL
	/* Restore the all modes calibration from the local system */
	i1pro3_restore_calibration(p);

	/* Touch it so that we know when the instrument was last opened */
	i1pro3_touch_calibration(p);
#endif
	

	/* Compute all the wavelength re-sampling filters. */
	/* (Incorporates wl_cal if it is valid) */
	if ((ev = i1pro3_compute_wav_filters(p, m->ms[m->mmode].wl_cal_raw_off,
		                                    m->ms[m->mmode].wl_cal_wav_off, 1)) != I1PRO3_OK) {
		a1logd(p->log,2,"i1pro3_compute_wav_filters() failed\n");
		return ev;
	}

	if (p->log->verb >= 1) {
		a1logv(p->log,1,"Instrument Type:   %s%s\n",inst_name(p->dtype),
		                                            m->aperture ? " Plus" : "");
		a1logv(p->log,1,"EE version:        %d\n",m->version);
		a1logv(p->log,1,"Serial Number:     %d\n",m->serno);
		a1logv(p->log,1,"Firmware version:  %d\n",m->fwver);
		a1logv(p->log,1,"Chip ID:           %02x-%02x%02x%02x%02x%02x%02x%02x\n",
	                           m->hw_chipid[0], m->hw_chipid[1], m->hw_chipid[2], m->hw_chipid[3],
	                           m->hw_chipid[4], m->hw_chipid[5], m->hw_chipid[6], m->hw_chipid[7]);
		a1logv(p->log,1,"Date manufactured: %d-%d-%d\n",
		                         m->ee_dom1 % 100, (m->ee_dom1/100) % 100, m->ee_dom1/10000);
		a1logv(p->log,1,"Aperture:          %s mm\n",m->aperture ? "8.0" : "4.5");

		a1logv(p->log,1,"Ambient Measurement ?   : %s\n",
		                                  m->capabilities & I1PRO3_CAP_AMBIENT ? "Yes" : "No");
		a1logv(p->log,1,"Wavelength Calibration ?: %s\n",
		                                  m->capabilities & I1PRO3_CAP_WL_LED ? "Yes" : "No");
		a1logv(p->log,1,"Zebra Ruler ?           : %s\n",
		                                  m->capabilities & I1PRO3_CAP_ZEB_RUL ? "Yes" : "No");
		a1logv(p->log,1,"Indicator LEDs ?        : %s\n",
		                                  m->capabilities & I1PRO3_CAP_IND_LED ? "Yes" : "No");
		a1logv(p->log,1,"Head Sensor ?           : %s\n",
		                                  m->capabilities & I1PRO3_CAP_HEAD_SENS ? "Yes" : "No");
		a1logv(p->log,1,"Polarized Measurement ? : %s\n",
		                                  m->capabilities & I1PRO3_CAP_POL ? "Yes" : "No");
	}

#ifdef USE_THREAD
	/* Setup the event monitoring thread */
	/* (If we start this too early, it wrecks instrument initialization, */
	/*  and we get the wrong scaling on the last 6 reflective measurement */
	/*  auxiliary values!) */
	if ((m->th = new_athread(i1pro3_event_thread, (void *)p)) == NULL)
		return I1PRO3_INT_THREADFAILED;
#endif

	return ev;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Return a pointer to the serial number */
char *i1pro3_imp_get_serial_no(i1pro3 *p) {
	i1pro3imp *m = (i1pro3imp *)p->m;

	return m->sserno; 
}

/* Set the measurement mode. It may need calibrating */
i1pro3_code i1pro3_imp_set_mode(
	i1pro3 *p,
	i1p3_mode mmode,	/* Operating mode */
	inst_mode imode		/* Full mode mask for options */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;

	a1logd(p->log,2,"i1pro3_imp_set_mode called with mode no %d and imask 0x%x\n",mmode,imode);
	switch(mmode) {
		case i1p3_refl_spot:
		case i1p3_refl_scan:
			break;
		case i1p3_refl_spot_pol:
		case i1p3_refl_scan_pol:
			if ((m->capabilities & I1PRO3_CAP_POL) == 0)	
				return I1PRO3_INT_ILLEGALMODE;
			break;
		case i1p3_emiss_spot_na:
		case i1p3_emiss_spot:
		case i1p3_emiss_scan:
			break;
		case i1p3_amb_spot:
		case i1p3_amb_flash:
			if ((m->capabilities & I1PRO3_CAP_AMBIENT) == 0)	
				return I1PRO3_INT_ILLEGALMODE;
			break;
		case i1p3_trans_spot:
		case i1p3_trans_scan:
			break;
		default:
			return I1PRO3_INT_ILLEGALMODE;
	}
	m->imode = imode;
	m->mmode = mmode;
	m->spec_en = (imode & inst_mode_spectral) != 0;

	if ((imode & inst_mode_highres) != 0) {
		i1pro3_code rv;
		if ((rv = i1pro3_set_highres(p)) != I1PRO3_OK)
			return rv;
	} else {
		i1pro3_set_stdres(p);	/* Ignore any error */
	}

	return I1PRO3_OK;
}

/* Check and ivalidate calibration */
i1pro3_code i1pro3_check_calib(i1pro3 *p) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *cs = &m->ms[m->mmode];
	double btemp;
	time_t curtime = time(NULL);
	i1pro3_code ev = I1PRO3_OK;

	a1logd(p->log,2,"i1pro3_check_calib: checking mode %d\n",m->mmode);

	if ((ev = i1pro3_getboardtemp(p, &btemp)) != I1PRO3_OK)
		return ev;

	/* Timout calibrations that are too old or temperature has changed too much */
	if (m->capabilities & I1PRO3_CAP_WL_LED) {
		if ((curtime - cs->wl_date) > WLCALTOUT) {
			a1logd(p->log,2,"Invalidating wavelength cal as %d secs from last cal\n",curtime - cs->wl_date);
			cs->wl_valid = 0;
		}
		if (fabs(btemp - cs->wl_temp) > CALTEMPLIM) {
			a1logd(p->log,2,"Invalidating wavelength cal as %d secs from last cal\n",btemp - cs->wl_temp);
			cs->wl_valid = 0;
		}
	}
	if (cs->dc_use != i1p3_dc_none && (curtime - cs->ddate) > DCALTOUT) {
		a1logd(p->log,2,"Invalidating dark cal as %d secs from last cal\n",curtime - cs->ddate);
		cs->dark_valid = 0;
	}
	if (cs->dc_use != i1p3_dc_none && fabs(btemp - cs->dtemp) > CALTEMPLIM) {
		a1logd(p->log,2,"Invalidating dark cal as %f degrees delta from last cal\n",btemp - cs->dtemp);
		cs->dark_valid = 0;
	}
	if (cs->mc_use != i1p3_mc_pcalfactor && (curtime - cs->cdate) > WCALTOUT) {
		a1logd(p->log,2,"Invalidating white cal as %d secs from last cal\n",curtime - cs->cdate);
		cs->cal_valid = 0;
	}

	if (p->log->debug >= 5) {
		a1logd(p->log,2,"i1pro3_check_calib result:\n");
		a1logd(p->log,1," reflective = %d, adaptive = %d, emiss = %d, trans = %d, scan = %d\n",
		cs->reflective, cs->adaptive, cs->emiss, cs->trans, cs->scan);
		a1logd(p->log,1," wl_valid = %d, dark_valid = %d, cal_valid = %d\n",
		                   cs->wl_valid, cs->dark_valid, cs->cal_valid); 
		a1logd(p->log,1," want_wlcalib = %d, want_calib = %d, want_dcalib = %d, noinitcalib = %d\n",
		                 cs->want_wlcalib, cs->want_calib,cs->want_dcalib, m->noinitcalib); 
	}

	return ev;
}

/* Return needed and available inst_cal_type's */
i1pro3_code i1pro3_imp_get_n_a_cals(i1pro3 *p, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *cs = &m->ms[m->mmode];
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
	i1pro3_code ev = I1PRO3_OK;

	if ((ev = i1pro3_check_calib(p)) != I1PRO3_OK)
		return ev;
	
	if (m->capabilities & I1PRO3_CAP_WL_LED) {
		if (!cs->wl_valid
		 || (cs->want_wlcalib && !m->noinitcalib)) {
			a1logd(p->log,2," wl calib is invalid or want calib\n");
			n_cals |= inst_calt_wavelength;
		}
		a_cals |= inst_calt_wavelength;
	}
	if (cs->reflective) {
		if (!cs->cal_valid
		 || (cs->want_calib && !m->noinitcalib)) {
			a1logd(p->log,2," reflective calib is invalid or want calib\n");
			n_cals |= inst_calt_ref_white;
		}
		a_cals |= inst_calt_ref_white;
	}
	if (cs->emiss) {
		if ((!cs->dark_valid)
		 || (cs->want_dcalib && !m->noinitcalib)) {
			a1logd(p->log,2," emissive dark calib is invalid or want calib\n");
			n_cals |= inst_calt_em_dark;
		}
		a_cals |= inst_calt_em_dark;
	}
	if (cs->emiss && !cs->adaptive && !cs->scan) {
		if (!cs->done_dintsel) {
			a1logd(p->log,2," non-adaptive emission int. time calib is invalid\n");
			n_cals |= inst_calt_emis_int_time;
		}
		a_cals |= inst_calt_emis_int_time;
	}
	if (cs->trans) {
		if ((!cs->dark_valid)
	     || (cs->want_dcalib && !m->noinitcalib)) {
			a1logd(p->log,2," transmissive dark calib is invalid or want calib\n");
			n_cals |= inst_calt_trans_dark;
		}
		a_cals |= inst_calt_trans_dark;

		if (!cs->cal_valid
	     || (cs->want_calib && !m->noinitcalib)) {
			a1logd(p->log,2," transmissive white calib is invalid or want calib\n");
			n_cals |= inst_calt_trans_vwhite;
		}
		a_cals |= inst_calt_trans_vwhite;
	}

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	a1logd(p->log,3,"i1pro3_imp_get_n_a_cals: returning n_cals 0x%x, a_cals 0x%x\n",n_cals, a_cals);

	return I1PRO3_OK;
}

/* - - - - - - - - - - - - - - - - */
/* Calibrate for the current mode. */
/* Request an instrument calibration of the current mode. */
i1pro3_code i1pro3_imp_calibrate(
	i1pro3 *p,
	inst_cal_type *calt,	/* Calibration type to do/remaining */
	inst_cal_cond *calc,	/* Current condition/desired condition */
	inst_calc_id_type *idtype,	/* Condition identifier type */
	char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	i1pro3_code ev = I1PRO3_OK;
	i1pro3imp *m = (i1pro3imp *)p->m;
	int mmode = m->mmode;
	i1pro3_state *cs = &m->ms[m->mmode];
	int sx1, sx2, sx3, sx;
	time_t cdate = time(NULL);	/* Current date to use */
	int nummeas = 0;
	int i, k;
    inst_cal_type needed, available;

	a1logd(p->log,2,"i1pro3_imp_calibrate called with calt 0x%x, calc 0x%x\n",*calt, *calc);

	if ((ev = i1pro3_imp_get_n_a_cals(p, &needed, &available)) != I1PRO3_OK)
		return ev;

	/* Translate inst_calt_all/needed into something specific */
	if (*calt == inst_calt_all
	 || *calt == inst_calt_needed
	 || *calt == inst_calt_available) {
		if (*calt == inst_calt_all) 
			*calt = (needed & inst_calt_n_dfrble_mask) | inst_calt_ap_flag;
		else if (*calt == inst_calt_needed)
			*calt = needed & inst_calt_n_dfrble_mask;
		else if (*calt == inst_calt_available)
			*calt = available & inst_calt_n_dfrble_mask;

		a1logd(p->log,4,"i1pro3_imp_calibrate: doing calt 0x%x\n",*calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return I1PRO3_OK;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		a1logd(p->log,4,"i1pro3_imp_calibrate: unsupported, calt 0x%x, available 0x%x\n",*calt,available);
		return I1PRO3_UNSUPPORTED;
	}

	if (*calt & inst_calt_ap_flag) {
		sx1 = 0; sx2 = sx3 = i1p3_no_modes;		/* Go through all the modes */
	} else {
		sx1 = m->mmode; sx2 = sx1 + 1;      /* Just current mode */
		sx3 = i1p3_no_modes;                /* no extra mode */
	}

	/* Go through the modes we are going to cover */
	for (sx = sx1; sx < sx2; (++sx >= sx2 && sx3 != i1p3_no_modes) ? sx = sx3, sx2 = sx+1, sx3 = i1p3_no_modes : 0) {
		i1pro3_state *s = &m->ms[sx];
		m->mmode = sx;				/* A lot of functions we call rely on this, so fake it */

		a1logd(p->log,2,"\nCalibrating mode %d\n", s->mode);

		/* Wavelength calibration: */
		if (s->wl_date != cdate
		 && (m->capabilities & I1PRO3_CAP_WL_LED)
		 && (*calt & (inst_calt_wavelength | inst_calt_ap_flag))
		 && ((*calc & inst_calc_cond_mask) == inst_calc_man_ref_white)) {
			double *wlraw;
			double optscale;
			double *abswav;

			a1logd(p->log,2,"\nDoing wavelength calibration\n");

			wlraw = dvectorz(-1, m->nraw-1);

			/* Make sure LEDs have cooled down for 1 second */
			i1pro3_delay_llampoff(p, LED_OFF_TIME);

			if ((ev = i1pro3_wl_measure(p, wlraw, &s->wl_temp)) != I1PRO3_OK) {
				a1logd(p->log,2,"i1pro3_wl_measure() failed\n");
				return ev;
			}

			// It's not cleare whether wl or cells is best offset to use...
			s->wl_cal_raw_off = s->wl_cal_wav_off = 0.0;

			/* Find the best fit of the measured values to the reference spectrum */
			if ((ev = i1pro3_match_wl_meas(p, &s->wl_cal_wav_off, NULL, wlraw)) != I1PRO3_OK)
//			if ((ev = i1pro3_match_wl_meas(p, NULL, &s->wl_cal_raw_off, wlraw)) != I1PRO3_OK)
			{
				a1logd(p->log,2,"i1pro3_match_wl_meas() failed\n");
				return ev;
			}
			free_dvector(wlraw, -1, m->nraw-1);


			/* Compute normal & hi-res wav filters for the value we just created */
			if ((ev = i1pro3_compute_wav_filters(p, s->wl_cal_raw_off, s->wl_cal_wav_off, 1))
				                                                           != I1PRO3_OK) {
				a1logd(p->log,2,"i1pro3_compute_wav_filters() failed\n");
				return ev;
			}


			s->want_wlcalib = 0;
			s->wl_valid = 1;
			s->wl_date = cdate;
			*calt &= ~inst_calt_wavelength;

			/* Save the calib to all modes */
			a1logd(p->log,5,"Saving wavelength calib to all modes\n");
			for (i = 0; i < i1p3_no_modes; i++) {
				i1pro3_state *ss = &m->ms[i];
				if (ss == cs)
					continue;
				ss->want_wlcalib = s->want_wlcalib;
				ss->wl_valid = s->wl_valid;
				ss->wl_date = s->wl_date;
				ss->wl_temp = s->wl_temp;
				ss->wl_cal_raw_off = s->wl_cal_raw_off;
				ss->wl_cal_wav_off = s->wl_cal_wav_off;
			}
		}

		/* Black calibration for all emissive or transmissive. */
		/* We do an adaptive calibration since this lets us track */
		/* temperature changes most effectively. */
		/* The black is interpolated from readings with two extreme integration times */
		if (s->ddate != cdate
		 && (*calt & (inst_calt_em_dark
		            | inst_calt_trans_dark
			        | inst_calt_ap_flag))
		 /* Any condition conducive to dark calib */
		 && ((*calc & inst_calc_cond_mask) == inst_calc_man_ref_white
		  || (*calc & inst_calc_cond_mask) == inst_calc_man_em_dark
		  || (*calc & inst_calc_cond_mask) == inst_calc_man_am_dark
		  || (*calc & inst_calc_cond_mask) == inst_calc_man_trans_dark)
		 /* Just emissive and transmissive */
		 && (s->emiss || s->trans)) {
			int refinst = 0;
			double ctemp;
			int i, j, k;
	
			a1logd(p->log,2,"\nDoing emis/trans black calibration\n");

			if ((ev = i1pro3_adapt_emis_cal(p, &ctemp)) != I1PRO3_OK) {
				a1logd(p->log,2,"i1pro3_refl_cal failed\n");
				return ev;
			}

			s->want_dcalib = 0;
			s->dark_valid = 1;
			s->dtemp = ctemp;
			s->ddate = cdate;
			*calt &= ~(inst_calt_em_dark
		             | inst_calt_trans_dark);
	
			/* Save the calib to all similar modes */
			a1logd(p->log,5,"Saving adaptive black calib to similar modes\n");
			for (i = 0; i < i1p3_no_modes; i++) {
				i1pro3_state *ss = &m->ms[i];
				if (ss == s || ss->ddate == s->ddate)
					continue;
				if (ss->emiss || ss->trans) {
					ss->want_dcalib = s->want_dcalib;
					ss->dark_valid = s->dark_valid;
					ss->dtemp = s->dtemp;
					ss->ddate = s->ddate;
					for (j = 0; j < 2; j++) {
						ss->idark_int_time[j] = s->idark_int_time[j];
						vect_cpy(ss->idark_data[j]-1, s->idark_data[j]-1, m->nraw+1);
					}
				}
			}
	
			a1logd(p->log,5,"Done adaptive interpolated black calibration\n");
		}
	
		/* Reflective white reference calibrate for spot or scan */
		if (s->cdate != cdate
		 && (*calt & (inst_calt_ref_white | inst_calt_ap_flag))
		 && ((*calc & inst_calc_cond_mask) == inst_calc_man_ref_white && s->reflective)) {
	
			a1logd(p->log,2,"\nDoing initial reflective white calibration\n");

			if (m->filt == inst_opt_filter_pol) {

				s->inttime = s->iinttime;		/* Start with default integration time */

				/* Do polarized reflectance calibration */
				if ((ev = i1pro3_pol_refl_cal(p)) != I1PRO3_OK) {
					a1logd(p->log,2,"i1pro3_refl_cal failed\n");
					return ev;
				}

			} else {
				/* Do reflectance calibration */
				if ((ev = i1pro3_refl_cal(p)) != I1PRO3_OK) {
					a1logd(p->log,2,"i1pro3_refl_cal failed\n");
					return ev;
				}
			}

			s->want_calib = 0;
			s->cal_valid = 1;
			s->cdate = cdate;
			*calt &= ~(inst_calt_ref_white);

			/* Save the calib to all similar modes */
			a1logd(p->log,5,"Saving reflection white calib to similar modes\n");
			for (i = 0; i < i1p3_no_modes; i++) {
				i1pro3_state *ss = &m->ms[i];
				if (ss == s || ss->cdate == s->cdate)
					continue;
				if (ss->reflective
				 && (s->pol == ss->pol)
				 && (s->dcaltime == ss->dcaltime)
				 && (s->wcaltime == ss->wcaltime)
				 && (s->wscaltime == ss->wscaltime)) {

					ss->want_calib = s->want_calib;
					ss->cal_valid = s->cal_valid;
					ss->cdate = s->cdate;

					if (s->pol) {
						vect_cpy(ss->pol_calraw_white, s->pol_calraw_white, m->nraw);
						vect_cpy(ss->pol_calsp_ledm, s->pol_calsp_ledm, m->nwav[0]);
						vect_cpy(ss->pol_calsp_white[0], s->pol_calsp_white[0], m->nwav[0]);
						vect_cpy(ss->pol_calsp_white[1], s->pol_calsp_white[1], m->nwav[1]);
					} else {
						vect_cpy(ss->calraw_white[0], s->calraw_white[0], m->nraw);
						vect_cpy(ss->calraw_white[1], s->calraw_white[1], m->nraw);
						vect_cpy(ss->calsp_ledm[0], s->calsp_ledm[0], m->nwav[0]);
						vect_cpy(ss->calsp_ledm[1], s->calsp_ledm[1], m->nwav[0]);
						vect_cpy(ss->calsp_illcr[0][0], s->calsp_illcr[0][0], m->nwav[0]);
						vect_cpy(ss->calsp_illcr[1][0], s->calsp_illcr[1][0], m->nwav[1]);
						vect_cpy(ss->calsp_illcr[0][1], s->calsp_illcr[0][1], m->nwav[0]);
						vect_cpy(ss->calsp_illcr[1][1], s->calsp_illcr[1][1], m->nwav[1]);
						vect_cpy(ss->iavg2aillum[0], s->iavg2aillum[0], m->nwav[0]);
						vect_cpy(ss->iavg2aillum[1], s->iavg2aillum[1], m->nwav[1]);
						vect_cpy(ss->sc_calsp_nn_white[0], s->sc_calsp_nn_white[0], m->nwav[0]);
						vect_cpy(ss->sc_calsp_nn_white[1], s->sc_calsp_nn_white[1], m->nwav[1]);
						vect_cpy(ss->cal_l_uv_diff[0], s->cal_l_uv_diff[0], m->nwav[0]);
						vect_cpy(ss->cal_l_uv_diff[1], s->cal_l_uv_diff[1], m->nwav[1]);
						vect_cpy(ss->cal_s_uv_diff[0], s->cal_s_uv_diff[0], m->nwav[0]);
						vect_cpy(ss->cal_s_uv_diff[1], s->cal_s_uv_diff[1], m->nwav[1]);
					}
				}
			}
		}
	
		/* If we are doing a transmissive white reference calibrate */
		if (s->cdate != cdate
		 && (*calt & (inst_calt_trans_vwhite | inst_calt_ap_flag))
		 && ((*calc & inst_calc_cond_mask) == inst_calc_man_trans_white && s->trans)) {
			int refinst = 0;
			int i, j, k;
	
			a1logd(p->log,2,"\nDoing transmission white calibration\n");

			if ((s->emiss || s->trans) && s->scan)
				s->inttime = s->iinttime;		/* Start with default integration time */

			ev = i1pro3_trans_cal(p);

			if (ev == I1PRO3_CAL_TRANSWHITEWARN) {
				m->transwarn |= 1;
				ev = I1PRO3_OK;
			}
			if (ev != I1PRO3_OK) {
				a1logd(p->log,2,"i1pro3_trans_cal failed\n");
				return ev;
			}
			s->cal_valid = 1;
			s->cdate = cdate;
			s->want_calib = 0;
			*calt &= ~(inst_calt_trans_vwhite);

			/* Save the calib to all similar modes */
			a1logd(p->log,5,"Saving transmission white calib to similar modes\n");
			for (i = 0; i < i1p3_no_modes; i++) {
				i1pro3_state *ss = &m->ms[i];
				if (ss == s || ss->cdate == s->cdate)
					continue;
				if (ss->trans) {
					ss->want_calib = s->want_calib;
					ss->cal_valid = s->cal_valid;
					ss->cdate = s->cdate;
					vect_cpy(ss->cal_factor[0], s->cal_factor[0], m->nwav[0]);
					vect_cpy(ss->cal_factor[1], s->cal_factor[1], m->nwav[1]);
					vect_cpy(ss->raw_white-1, s->raw_white-1, m->nraw+1);
				}
			}
	
			a1logd(p->log,5,"Done transmission white calibration\n");
		}
	
		/* Deal with a display integration time selection */
		if (s->diseldate != cdate
		 && (*calt & (inst_calt_emis_int_time | inst_calt_ap_flag))
		 && (*calc & inst_calc_cond_mask) == inst_calc_emis_white
		 && (s->emiss && !s->adaptive && !s->scan)) {
			double inttime = 0.05;
			double **raw_sample;
			int nummeas;
			double _psample[MX_NRAW+1], *psample = _psample + 1;
			double maxval;

			a1logd(p->log,2,"\nDoing display integration time calibration\n");
	
			s->inttime = s->iinttime;			/* Start with default integration time */

			/* returns raw, black subtracted, linearized */
			ev = i1pro3_spot_simple_emis_raw_meas(p, &raw_sample, &nummeas, &inttime, 0.25, 0);

			if (ev != I1PRO3_RD_SENSORSATURATED) {
				return ev;
			} else if (ev == I1PRO3_OK) {

				i1pro3_average_rawmmeas(p, psample, raw_sample, nummeas);
				maxval = vect_max(psample, m->nraw);

				a1logd(p->log,4," display int. calib. maxval %f\n",maxval);

				inttime = inttime * s->targoscale * m->sens_target/maxval;

				if (inttime < m->min_int_time)
					inttime = m->min_int_time;

				if (inttime < s->inttime) {		/* Decrease, no increase of na inttime */
					s->inttime = inttime;
					a1logd(p->log,5,"Display integration time reduced to %f\n",s->inttime);
				}

				i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeas);
			}

			s->done_dintsel = 1;
			s->diseldate = cdate;
			*calt &= ~inst_calt_emis_int_time;
	
			a1logd(p->log,5,"Done display integration time calibration\n");
		}

	}	/* Look at next mode */
	m->mmode = mmode;			/* Restore actual mode */

	/* Make sure there's the right condition for any remaining calibrations. */
	if (*calt & (inst_calt_ref_white)) {
		*idtype = inst_calc_id_ref_sn;
		sprintf(id, "%d",m->ee_serno);
		if ((*calc & inst_calc_cond_mask) != inst_calc_man_ref_white) {
			/* Calibrate using white tile */
			*calc = inst_calc_man_ref_white;
			return I1PRO3_CAL_SETUP;
		}
	} else if (*calt & inst_calt_wavelength) {		/* Wavelength calibration */
		*idtype = inst_calc_id_ref_sn;
		sprintf(id, "%d",m->ee_serno);
		if ((*calc & inst_calc_cond_mask) != inst_calc_man_ref_white) {
			/* Have to calibrate using white tile */
			*calc = inst_calc_man_ref_white;
			return I1PRO3_CAL_SETUP;
		}
	} else if (*calt & inst_calt_em_dark) {		/* Emissive Dark calib */
		*idtype = inst_calc_id_none;
		id[0] = '\000';
		if ((*calc & inst_calc_cond_mask) != inst_calc_man_em_dark) {
			/* Any sort of dark reference */
			*calc = inst_calc_man_em_dark;
			return I1PRO3_CAL_SETUP;
		}
	} else if (*calt & inst_calt_trans_dark) {	/* Transmissvice dark */
		*idtype = inst_calc_id_none;
		id[0] = '\000';
		if ((*calc & inst_calc_cond_mask) != inst_calc_man_trans_dark) {
			*calc = inst_calc_man_trans_dark;
			return I1PRO3_CAL_SETUP;
		}
	} else if (*calt & inst_calt_trans_vwhite) {/* Transmissvice white for emulated transmission */
		*idtype = inst_calc_id_none;
		id[0] = '\000';
		if ((*calc & inst_calc_cond_mask) != inst_calc_man_trans_white) {
			*calc = inst_calc_man_trans_white;
			return I1PRO3_CAL_SETUP;
		}
	} else if (*calt & inst_calt_emis_int_time) {
		*idtype = inst_calc_id_none;
		id[0] = '\000';
		if ((*calc & inst_calc_cond_mask) != inst_calc_emis_white) {
			*calc = inst_calc_emis_white;
			return I1PRO3_CAL_SETUP;
		}
	}

	/* Go around again if we've still got calibrations to do */
	if (*calt & inst_calt_all_mask) {
		return I1PRO3_CAL_SETUP;
	}

	/* We must be done */
	
#ifdef ENABLE_NONVCAL
	/* Save the calibration to a file */
	i1pro3_save_calibration(p);
#endif

	if (m->transwarn) {
		*calc = inst_calc_message;
		if (m->transwarn & 2) {
			*idtype = inst_calc_id_trans_low;
			strcpy(id, "Warning: Transmission light source is too low for accuracy!");
		} else {
			*idtype = inst_calc_id_trans_wl;
			strcpy(id, "Warning: Transmission light source is low at some wavelengths!");
		}
		m->transwarn = 0;
	}

	a1logd(p->log,2,"Finished cal with dark_valid = %d, cal_valid = %d\n",cs->dark_valid, cs->cal_valid);

	return I1PRO3_OK; 
}

/* Interpret an icoms error into a I1PRO3 error */
int icoms2i1pro3_err(int se) {
	if (se != ICOM_OK)
		return I1PRO3_COMS_FAIL;
	return I1PRO3_OK;
}

/* - - - - - - - - - - - - - - - - */
/* Measure a display update delay. It is assumed that */
/* white_stamp(init) has been called, and then a */
/* white to black change has been made to the displayed color, */
/* and this will measure the time it took for the update to */
/* be noticed by the instrument, up to 2.0 seconds. */
/* (It is assumed that white_change() will be called at the time the patch */
/* changes color.) */
/* inst_misread will be returned on failure to find a transition to black. */
#define NDMXTIME 2.0		/* Maximum time to take */
#define NDSAMPS 1000		/* Debug samples >= 2.0/0.0025 */

typedef struct {
	double sec;
	double rgb[3];
	double tot;
} i1rgbdsamp;

i1pro3_code i1pro3_imp_meas_delay(
i1pro3 *p,
int *pdispmsec,		/* Return display update delay in msec */
int *pinstmsec) {	/* Return instrument latency in msec */
	i1pro3_code ev = I1PRO3_OK;
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	int refinst = 0;
	int i, j, k, mm;
	double **multimeas;			/* Spectral measurements */
	int nummeas;
	double rgbw[3] = { 610.0, 520.0, 460.0 };
	double inttime;
	double rstart;
	i1rgbdsamp *samp;
	double stot, etot, del, thr;
	double stime, etime;
	int dispmsec, instmsec;
	int hr = m->highres;

	if (pinstmsec != NULL)
		*pinstmsec = 0; 

	if ((rstart = usec_time()) < 0.0) {
		a1loge(p->log, inst_internal_error, "i1pro3_imp_meas_delay: No high resolution timers\n");
		return inst_internal_error; 
	}

	/* Read the samples */
	inttime = m->min_int_time;
	nummeas = (int)(NDMXTIME/inttime + 0.5);

	multimeas = dmatrix(0, nummeas-1, -1, m->nwav[hr]-1);
	if ((samp = (i1rgbdsamp *)calloc(sizeof(i1rgbdsamp), nummeas)) == NULL) {
		a1logd(p->log, 1, "i1pro3_meas_delay: malloc failed\n");
		return I1PRO3_INT_MALLOC;
	}

	/* We rely on the measurement code setting m->trigstamp when the */
	/* trigger packet is sent to the instrument */
	if ((ev = i1pro3_spot_simple_emis_meas(p, multimeas, nummeas, &inttime, hr)) != inst_ok) {
		free_dmatrix(multimeas, 0, nummeas-1, 0, m->nwav[hr]-1);
		free(samp);
		return ev;
	}

	if (m->whitestamp < 0.0) {
		a1logd(p->log, 1, "i1pro3_meas_delay: White transition wasn't timestamped\n");
		return inst_internal_error; 
	}

	/* Convert the samples to RGB */
	/* Add 10 msec fudge factor */
	for (i = 0; i < nummeas; i++) {
		samp[i].sec = i * inttime + (m->trigstamp - m->whitestamp)/1000000.0 + 0.01;
		samp[i].rgb[0] = samp[i].rgb[1] = samp[i].rgb[2] = 0.0;
		for (j = 0; j < m->nwav[hr]; j++) {
			double wl = XSPECT_WL(m->wl_short[hr], m->wl_long[hr], m->nwav[hr], j);

//printf("~1 samp %d wl %d = %f\n",i, j, multimeas[i][j]);
			for (k = 0; k < 3; k++) {
				double tt = (double)(wl - rgbw[k]);
				tt = (50.0 - fabs(tt))/50.0;
				if (tt < 0.0)
					tt = 0.0;
				samp[i].rgb[k] += sqrt(tt) * multimeas[i][j];
			}
		}
		samp[i].tot = samp[i].rgb[0] + samp[i].rgb[1] + samp[i].rgb[2];
	}
	free_dmatrix(multimeas, 0, nummeas-1, 0, m->nwav[hr]-1);

	a1logd(p->log, 3, "i1pro3_meas_delay: Read %d samples for refresh calibration\n",nummeas);

	/* Over the first 100msec, locate the maximum value */
	stime = samp[0].sec;
	stot = -1e9;
	for (i = 0; i < nummeas; i++) {
		if (samp[i].tot > stot)
			stot = samp[i].tot;
		if ((samp[i].sec - stime) > 0.1)
			break;
	}

	/* Over the last 100msec, locate the maximum value */
	etime = samp[nummeas-1].sec;
	etot = -1e9;
	for (i = nummeas-1; i >= 0; i--) {
		if (samp[i].tot > etot)
			etot = samp[i].tot;
		if ((etime - samp[i].sec) > 0.1)
			break;
	}

	del = etot - stot;
	thr = stot + 0.30 * del;		/* 30% of transition threshold */

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "i1pro3_meas_delay: start tot %f end tot %f del %f, thr %f\n", stot, etot, del, thr);
#endif

#ifdef PLOT_UPDELAY
	/* Plot the raw sensor values */
	{
		double xx[NDSAMPS];
		double y1[NDSAMPS];
		double y2[NDSAMPS];
		double y3[NDSAMPS];
		double y4[NDSAMPS];

		for (i = 0; i < nummeas && i < NDSAMPS; i++) {
			xx[i] = samp[i].sec;
			y1[i] = samp[i].rgb[0];
			y2[i] = samp[i].rgb[1];
			y3[i] = samp[i].rgb[2];
			y4[i] = samp[i].tot;
//printf("%d: %f sec -> %f R+G+B\n",i,samp[i].sec, samp[i].tot);
		}
		printf("Display update delay measure sensor values and time (sec)\n");
		do_plot6(xx, y1, y2, y3, y4, NULL, NULL, nummeas);
	}
#endif

	/* Check that there has been a transition */
	if (del < 5.0) {
		free(samp);
		a1logd(p->log, 1, "i1pro3_meas_delay: can't detect change from black to white\n");
		return I1PRO3_RD_NOTRANS_FOUND; 
	}

	/* Working from the start, locate the time at which the level was above the threshold */
	for (i = 0; i < (nummeas-1); i++) {
		if (samp[i].tot > thr)
			break;
	}

	a1logd(p->log, 2, "i1pro3_meas_delay: stoped at sample %d time %f\n",i,samp[i].sec);

	/* Compute overall delay */
	dispmsec = (int)(samp[i].sec * 1000.0 + 0.5);				/* Display update time */
	instmsec = (int)((m->trigstamp - rstart)/1000.0 + 0.5);		/* Reaction time */

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "i1pro3_meas_delay: disp %d, trig %d msec\n",dispmsec,instmsec);
#else
	a1logd(p->log, 2, "i1pro3_meas_delay: disp %d, trig %d msec\n",dispmsec,instmsec);
#endif

	if (dispmsec < 0) 		/* This can happen if the patch generator delays it's return */
		dispmsec = 0;

	if (pdispmsec != NULL)
		*pdispmsec = dispmsec;

	if (pinstmsec != NULL)
		*pinstmsec = instmsec;

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "i1pro3_meas_delay: returning %d & %d msec\n",dispmsec,instmsec);
#else
	a1logd(p->log, 2, "i1pro3_meas_delay: returning %d & %d msec\n",dispmsec,instmsec);
#endif
	free(samp);

	return I1PRO3_OK;
}
#undef NDSAMPS
#undef NDMXTIME

/* Timestamp the white patch change during meas_delay() */
inst_code i1pro3_imp_white_change(i1pro3 *p, int init) {
	i1pro3imp *m = (i1pro3imp *)p->m;

	if (init)
		m->whitestamp = -1.0;
	else {
		if ((m->whitestamp = usec_time()) < 0.0) {
			a1loge(p->log, inst_internal_error, "i1pro3_imp_wite_change: No high resolution timers\n");
			return inst_internal_error; 
		}
	}

	return inst_ok;
}

/* - - - - - - - - - - - - - - - - */
/* Measure a patch or strip in the current mode. */
i1pro3_code i1pro3_imp_measure(
	i1pro3 *p,
	ipatch *vals,		/* Pointer to array of instrument patch value */
	int nvals,			/* Number of values */	
	instClamping clamp	/* Clamp XYZ/Lab to be +ve */
) {
	i1pro3_code ev = I1PRO3_OK;
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	double **specrd = NULL;		/* Cooked spectral patch values */
	double duration = 0.0;		/* Possible flash duration value */
	int user_trig = 0;
	int hr = m->highres;

	if (s->reflective)
		hr = 0;					/* hi-res doesn't work well for reflective... */

	a1logd(p->log,2,"i1pro3_imp_measure: Taking %d measurments in %s%s%s%s%s mode called\n", nvals,
		        s->emiss ? "Emission" : s->trans ? "Trans" : "Refl", 
		        s->emiss && s->ambient ? " Ambient" : "",
		        s->scan ? " Scan" : "",
		        s->flash ? " Flash" : "",
		        s->adaptive ? " Adaptive" : "");


	if ((ev = i1pro3_check_calib(p)) != I1PRO3_OK)	/* Check if calibration have expired */
		return ev;

	/* Is the used calibration state invalid ? */ 
	if ((s->dc_use != i1p3_dc_none && !s->dark_valid)
	 || !s->cal_valid) {
		a1logd(p->log,3,"dc_use %d dark_valid %d, cal_valid %d\n",s->dc_use,s->dark_valid,s->cal_valid);
		a1logd(p->log,3,"i1pro3_imp_measure need calibration\n");
		return I1PRO3_RD_NEEDS_CAL;
	}
		
	if (nvals <= 0
	 || (!s->scan && nvals > 1)) {
		a1logd(p->log,2,"i1pro3_imp_measure wrong number of patches\n");
		return I1PRO3_INT_WRONGPATCHES;
	}

	/* Buffer for returned measurements */
	specrd = dmatrix(0, nvals-1, 0, m->nwav[hr]-1);

	if (m->trig == inst_opt_trig_user_switch) {
		m->hide_event = 1;						/* Supress instrument events */

#ifdef USE_THREAD
		{
			int currcount = m->switch_count;		/* Variable set by thread */
			while (currcount == m->switch_count) {
				inst_code rc;
				int cerr;

				/* Don't trigger on user key if scan, only trigger */
				/* on instrument event */
				if (p->uicallback != NULL
				 && (rc = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
					if (rc == inst_user_abort) {
						ev = I1PRO3_USER_ABORT;
						break;						/* Abort */
					}
					if (!s->scan && rc == inst_user_trig) {
						ev = I1PRO3_USER_TRIG;
						user_trig = 1;
						break;						/* Trigger */
					}
				}
				msec_sleep(100);
			}
		}
#else
		/* Throw one away in case the event was pressed prematurely */
		i1pro3_waitfor_event_th(p, NULL, 0.01);

		for (;;) {
			inst_code rc;
			i1pro3_eve;
			int cerr;

			if ((ev = i1pro3_waitfor_event_th(p, &ecode, 0.1)) != I1PRO3_OK
			 && ev != I1PRO3_INT_BUTTONTIMEOUT)
				break;			/* Error */

			if (ev == I1PRO3_OK)
				break;			/* event triggered */

			/* Don't trigger on user key if scan, only trigger */
			/* on instrument event */
			if (p->uicallback != NULL
			 && (rc = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
				if (rc == inst_user_abort) {
					ev = I1PRO3_USER_ABORT;
					break;						/* Abort */
				}
				if (!s->scan && rc == inst_user_trig) {
					ev = I1PRO3_USER_TRIG;
					user_trig = 1;
					break;						/* Trigger */
				}
			}
		}
#endif
		a1logd(p->log,3,"############# triggered ##############\n");
		if (p->uicallback)	/* Notify of trigger */
			p->uicallback(p->uic_cntx, inst_triggered); 

		m->hide_event = 0;						/* Enable event events again */

	} else if (m->trig == inst_opt_trig_user) {
		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "hcfr: inst_opt_trig_user but no uicallback function set!\n");
			ev = I1PRO3_UNSUPPORTED;

		} else {

			for (;;) {
				inst_code rc;
				if ((rc = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
					if (rc == inst_user_abort) {
						ev = I1PRO3_USER_ABORT;	/* Abort */
						break;
					}
					if (rc == inst_user_trig) {
						ev = I1PRO3_USER_TRIG;
						user_trig = 1;
						break;						/* Trigger */
					}
				}
				msec_sleep(200);
			}
		}
		a1logd(p->log,3,"############# triggered ##############\n");
		if (p->uicallback)	/* Notify of trigger */
			p->uicallback(p->uic_cntx, inst_triggered); 

	/* Progromatic Trigger */
	} else {
		/* Check for abort */
		if (p->uicallback != NULL
		 && (ev = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort)
			ev = I1PRO3_USER_ABORT;	/* Abort */
	}

	if (ev != I1PRO3_OK && ev != I1PRO3_USER_TRIG) {
		free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[hr]-1);
		a1logd(p->log,2,"i1pro3_imp_measure user aborted, terminated, command, or failure\n");
		return ev;		/* User abort, term, command or failure */
	}

	/* Take a measurement reading using the current mode. */
	/* Converts to completely processed output readings. */

	a1logd(p->log,2,"Do main measurement reading\n");


	if (s->emiss || s->trans) {

		if (s->scan) {
			if ((ev = i1pro3_scan_emis_meas(p, &duration, specrd, nvals)) !=I1PRO3_OK) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[hr]-1);
				a1logd(p->log,2,"i1pro3_imp_measure failed at i1pro3_spot_adapt_emis_meas\n");
				return ev;
			}

		} else {
			if ((ev = i1pro3_spot_adapt_emis_meas(p, specrd)) !=I1PRO3_OK) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[hr]-1);
				a1logd(p->log,2,"i1pro3_imp_measure failed at i1pro3_spot_adapt_emis_meas\n");
				return ev;
			}
		}
	}

	if (s->reflective) {

		if (m->filt == inst_opt_filter_pol) {
			if (s->scan) {

				/* Do a pol strip reflectance measurement and return spectrum of each patch */
				if ((ev = i1pro3_pol_strip_refl_meas(p, specrd, nvals, hr)) !=I1PRO3_OK) {
					free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[hr]-1);
					a1logd(p->log,2,"i1pro3_imp_measure failed at i1pro3_pol_strip_refl_meas\n");
					return ev;
				}

			} else {

				/* Do a pol spot reflectance measurement and return spectrum */
				if ((ev = i1pro3_pol_spot_refl_meas(p, specrd, hr)) !=I1PRO3_OK) {
					free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[hr]-1);
					a1logd(p->log,2,"i1pro3_imp_measure failed at i1pro3_pol_spot_refl_meas\n");
					return ev;
				}
			}
		} else {
	
			if (s->scan) {

				/* Do a strip reflectance measurement and return spectrum of each patch */
				if ((ev = i1pro3_strip_refl_meas(p, specrd, nvals, hr)) !=I1PRO3_OK) {
					free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[hr]-1);
					a1logd(p->log,2,"i1pro3_imp_measure failed at i1pro3_strip_refl_meas\n");
					return ev;
				}
			} else {

				/* Do a spot reflectance measurement and return spectrum */
				if ((ev = i1pro3_spot_refl_meas(p, specrd, hr)) !=I1PRO3_OK) {
					free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[hr]-1);
					a1logd(p->log,2,"i1pro3_imp_measure failed at i1pro3_spot_refl_meas\n");
					return ev;
				}
			}
		}
	}

	/* Transfer spectral and convert to XYZ */
	if ((ev = i1pro3_conv2XYZ(p, vals, nvals, specrd, hr, clamp)) != I1PRO3_OK) {
		free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[hr]-1);
		a1logd(p->log,2,"i1pro3_imp_measure failed at i1pro3_conv2XYZ\n");
		return ev;
	}
	free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[hr]-1);
	
	if (nvals > 0)
		vals[0].duration = duration;	/* Possible flash duration */
	
	a1logd(p->log,3,"i1pro3_imp_measure sucessful return\n");
	if (user_trig)
		return I1PRO3_USER_TRIG;
	return ev; 
}

/* - - - - - - - - - - - - - - - - */
/*

	Determining the refresh rate for a refresh type display.

	This is easy when the max sample rate of the i1 is above
	the nyquist of the display, and will always be the case
	for the range we are prepared to measure (up to 100Hz)
	when using the i1Pro3, but is a problem for other
	instruments, so there is generic code here to 
	work around this problem by detecting when
	we are measuring an alias of the refresh rate, and
	average the aliasing corrected measurements.

	If there is no aparent refresh, or the refresh rate is not determinable,
	return a period of 0.0 and inst_ok;
*/

i1pro3_code i1pro3_measure_rgb(i1pro3 *p, double *inttime, double *rgb);

#ifndef PSRAND32L 
# define PSRAND32L(S) ((S) * 1664525L + 1013904223L)
#endif
#undef FREQ_SLOW_PRECISE	/* [und] Interpolate then autocorrelate, else autc & filter */
#define NFSAMPS 160		/* [80] Number of samples to read */
#define PBPMS 20		/* bins per msec */
#define PERMIN ((1000 * PBPMS)/40)	/* 40 Hz */
#define PERMAX ((1000 * PBPMS)/4)	/* 4 Hz*/
#define NPER (PERMAX - PERMIN + 1)
#define PWIDTH (8 * PBPMS)			/* 8 msec bin spread to look for peak in */
#define MAXPKS 20					/* Number of peaks to find */
#define TRIES 8	 					/* Number of different sample rates to try */

i1pro3_code i1pro3_imp_meas_refrate(
	i1pro3 *p,
	double *ref_rate
) {
	i1pro3_code ev = I1PRO3_OK;
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	int i, j, k, mm;
	double **multimeas;			/* Spectral measurements */
	int nummeas;
	double rgbw[3] = { 610.0, 520.0, 460.0 };
	double inttime;
	static unsigned int randn = 0x12345678;
	struct {
		double sec;
		double rgb[3];
	} samp[NFSAMPS * 2];
	int nfsamps;			/* Actual samples read */
	double minv[3];			/* Minimum reading */
	double maxv[3];			/* Maximum reading */
	double maxt;			/* Time range */
#ifdef FREQ_SLOW_PRECISE
	int nbins;
	double *bins[3];		/* PBPMS sample bins */
#else
	double tcorr[NPER];		/* Temp for initial autocorrelation */
	int ntcorr[NPER];		/* Number accumulated */
#endif
	double corr[NPER];		/* Filtered correlation for each period value */
	double mincv, maxcv;	/* Max and min correlation values */
	double crange;			/* Correlation range */
	double peaks[MAXPKS];	/* Peak wavelength */
	double peakh[MAXPKS];	/* Peak heighheight */
	int npeaks;				/* Number of peaks */
	double pval;			/* Period value */
	double rfreq[TRIES];	/* Computed refresh frequency for each try */
	double rsamp[TRIES];	/* Sampling rate used to measure frequency */
	int tix = 0;			/* try index */
	int hr = m->highres;

	a1logd(p->log,2,"i1pro3_imp_meas_refrate called\n");

	if (ref_rate != NULL)
		*ref_rate = 0.0;

	if (!s->emiss) {
		a1logd(p->log,2,"i1pro3_imp_meas_refrate not in emissive mode\n");
		return I1PRO3_UNSUPPORTED;
	}

	for (mm = 0; mm < TRIES; mm++) {
		rfreq[mm] = 0.0;
		npeaks = 0;			/* Number of peaks */
		nummeas = NFSAMPS;
		multimeas = dmatrix(0, nummeas-1, -1, m->nwav[hr]-1);

		if (mm == 0)
			inttime = m->min_int_time;
		else {
			double rval, dmm;
			randn = PSRAND32L(randn);
			rval = (double)randn/4294967295.0;
			dmm = ((double)mm + rval - 0.5)/(TRIES - 0.5);
			inttime = m->min_int_time * (1.0 + dmm * 0.80);
		}

		if ((ev = i1pro3_spot_simple_emis_meas(p, multimeas, nummeas, &inttime, hr)) != inst_ok) {
			free_dmatrix(multimeas, 0, nummeas-1, 0, m->nwav[hr]-1);
			return ev;
		} 

		rsamp[tix] = 1.0/inttime;

		/* Convert the samples to RGB */
		for (i = 0; i < nummeas && i < NFSAMPS; i++) {
			samp[i].sec = i * inttime;
			samp[i].rgb[0] = samp[i].rgb[1] = samp[i].rgb[2] = 0.0;
			for (j = 0; j < m->nwav[hr]; j++) {
				double wl = XSPECT_WL(m->wl_short[hr], m->wl_long[hr], m->nwav[hr], j);

//printf("~1 multimeas %d %d = %f\n",i, j, multimeas[i][j]);
				for (k = 0; k < 3; k++) {
					double tt = (double)(wl - rgbw[k]);
					tt = (40.0 - fabs(tt))/40.0;
					if (tt < 0.0)
						tt = 0.0;
					samp[i].rgb[k] += sqrt(tt) * multimeas[i][j];
				}
			}
		}
		free_dmatrix(multimeas, 0, nummeas-1, 0, m->nwav[hr]-1);
		nfsamps = i;

		a1logd(p->log, 3, "i1pro3_meas_refrate: Read %d samples for refresh calibration\n",nfsamps);

#ifdef NEVER
		/* Plot the raw sensor values */
		{
			double xx[NFSAMPS];
			double y1[NFSAMPS];
			double y2[NFSAMPS];
			double y3[NFSAMPS];

			for (i = 0; i < nfsamps; i++) {
				xx[i] = samp[i].sec;
				y1[i] = samp[i].rgb[0];
				y2[i] = samp[i].rgb[1];
				y3[i] = samp[i].rgb[2];
//			printf("%d: %f -> %f\n",i,samp[i].sec, samp[i].rgb[0]);
			}
			printf("Fast scan sensor values and time (sec)\n");
			do_plot6(xx, y1, y2, y3, NULL, NULL, NULL, nfsamps);
		}
#endif

		/* Locate the smallest values and maximum time */
		maxt = -1e6;
		minv[0] = minv[1] = minv[2] = 1e20;
		maxv[0] = maxv[1] = maxv[2] = -11e20;
		for (i = nfsamps-1; i >= 0; i--) {
			if (samp[i].sec > maxt)
				maxt = samp[i].sec;
			for (j = 0; j < 3; j++) {
				if (samp[i].rgb[j] < minv[j])
					minv[j] = samp[i].rgb[j]; 
				if (samp[i].rgb[j] > maxv[j])
					maxv[j] = samp[i].rgb[j]; 
			}
		}
		/* Re-zero the sample times, and normalise the readings */
		for (i = nfsamps-1; i >= 0; i--) {
			samp[i].sec -= samp[0].sec; 
			if (samp[i].sec > maxt)
				maxt = samp[i].sec;
			for (j = 0; j < 3; j++) {
				samp[i].rgb[j] -= minv[j];
			}
		}

#ifdef FREQ_SLOW_PRECISE	/* Interp then autocorrelate */

		/* Create PBPMS bins and interpolate readings into them */
		nbins = 1 + (int)(maxt * 1000.0 * PBPMS + 0.5);
		for (j = 0; j < 3; j++) {
			if ((bins[j] = (double *)calloc(sizeof(double), nbins)) == NULL) {
				a1loge(p->log, inst_internal_error, "i1pro3_meas_refrate: malloc failed\n");
				return I1PRO3_INT_MALLOC;
			}
		}

		/* Do the interpolation */
		for (k = 0; k < (nfsamps-1); k++) {
			int sbin, ebin;
			sbin = (int)(samp[k].sec * 1000.0 * PBPMS + 0.5);
			ebin = (int)(samp[k+1].sec * 1000.0 * PBPMS + 0.5);
			for (i = sbin; i <= ebin; i++) {
				double bl;
#if defined(__APPLE__) && defined(__POWERPC__)
				gcc_bug_fix(i);
#endif
				bl = (i - sbin)/(double)(ebin - sbin);	/* 0.0 to 1.0 */
				for (j = 0; j < 3; j++) {
					bins[j][i] = (1.0 - bl) * samp[k].rgb[j] + bl * samp[k+1].rgb[j];
				}
			} 
		}

#ifdef NEVER
		/* Plot interpolated values */
		{
			double *xx;
			double *y1;
			double *y2;
			double *y3;

			xx = malloc(sizeof(double) * nbins);
			y1 = malloc(sizeof(double) * nbins);
			y2 = malloc(sizeof(double) * nbins);
			y3 = malloc(sizeof(double) * nbins);

			if (xx == NULL || y1 == NULL || y2 == NULL || y3 == NULL) {
				a1loge(p->log, inst_internal_error, "i1pro3_meas_refrate: malloc failed\n");
				for (j = 0; j < 3; j++)
					free(bins[j]);
				return I1PRO3_INT_MALLOC;
			}
			for (i = 0; i < nbins; i++) {
				xx[i] = i / (double)PBPMS;			/* msec */
				y1[i] = bins[0][i];
				y2[i] = bins[1][i];
				y3[i] = bins[2][i];
			}
			printf("Interpolated fast scan sensor values and time (msec) for inttime %f\n",inttime);
			do_plot6(xx, y1, y2, y3, NULL, NULL, NULL, nbins);

			free(xx);
			free(y1);
			free(y2);
			free(y3);
		}
#endif /* PLOT_REFRESH */

		/* Compute auto-correlation at 1/PBPMS msec intervals */
		/* from 25 msec (40Hz) to 100msec (10 Hz) */
		mincv = 1e48, maxcv = -1e48;
		for (i = 0; i < NPER; i++) {
			int poff = PERMIN + i;		/* Offset to corresponding sample */

			corr[i] = 0;
			for (k = 0; (k + poff) < nbins; k++) {
				corr[i] += bins[0][k] * bins[0][k + poff]
				        +  bins[1][k] * bins[1][k + poff]
				        +  bins[2][k] * bins[2][k + poff];
			}
			corr[i] /= (double)k;		/* Normalize */

			if (corr[i] > maxcv)
				maxcv = corr[i];
			if (corr[i] < mincv)
				mincv = corr[i];
		}
		/* Free the bins */
		for (j = 0; j < 3; j++)
			free(bins[j]);

#else /* !FREQ_SLOW_PRECISE  Fast - autocorrellate then filter */

	/* Upsample by a factor of 2 */
	for (i = nfsamps-1; i >= 0; i--) {
		j = 2 * i;
		samp[j].sec = samp[i].sec;
		samp[j].rgb[0] = samp[i].rgb[0];
		samp[j].rgb[1] = samp[i].rgb[1];
		samp[j].rgb[2] = samp[i].rgb[2];
		if (i > 0) {
			j--;
			samp[j].sec = 0.5 * (samp[i].sec + samp[i-1].sec);
			samp[j].rgb[0] = 0.5 * (samp[i].rgb[0] + samp[i-1].rgb[0]);
			samp[j].rgb[1] = 0.5 * (samp[i].rgb[1] + samp[i-1].rgb[1]);
			samp[j].rgb[2] = 0.5 * (samp[i].rgb[2] + samp[i-1].rgb[2]);
		}
	}
	nfsamps = 2 * nfsamps - 1;

	/* Do point by point correllation of samples */
	for (i = 0; i < NPER; i++) {
		tcorr[i] = 0.0;
		ntcorr[i] = 0;
	}

	for (j = 0; j < (nfsamps-1); j++) {

		for (k = j+1; k < nfsamps; k++) {
			double del, cor;
			int bix;

			del = samp[k].sec - samp[j].sec;
			bix = (int)(del * 1000.0 * PBPMS + 0.5);
			if (bix < PERMIN)
				continue;
			if (bix > PERMAX)
				break;
			bix -= PERMIN;
		
			cor = samp[j].rgb[0] * samp[k].rgb[0]
	            + samp[j].rgb[1] * samp[k].rgb[1]
	            + samp[j].rgb[2] * samp[k].rgb[2];

//printf("~1 j %d k %d, del %f bix %d cor %f\n",j,k,del,bix,cor);
			tcorr[bix] += cor;
			ntcorr[bix]++;
		} 
	}
	/* Divide out count and linearly interpolate */
	j = 0;
	for (i = 0; i < NPER; i++) {
		if (ntcorr[i] > 0) {
			tcorr[i] /= ntcorr[i];
			if ((i - j) > 1) {
				if (j == 0) {
					for (k = j; k < i; k++)
						tcorr[k] = tcorr[i];

				} else {		/* Linearly interpolate from last value */
					double ww = (double)i-j;
					for (k = j+1; k < i; k++) {
						double bl = (k-j)/ww;
						tcorr[k] = (1.0 - bl) * tcorr[j] + bl * tcorr[i];
					}
				}
			}
			j = i;
		}
	}
	if (j < (NPER-1)) {
		for (k = j+1; k < NPER; k++) {
			tcorr[k] = tcorr[j];
		}
	}

#ifdef PLOT_REFRESH
	/* Plot unfiltered auto correlation */
	{
		double xx[NPER];
		double y1[NPER];

		for (i = 0; i < NPER; i++) {
			xx[i] = (i + PERMIN) / (double)PBPMS;			/* msec */
			y1[i] = tcorr[i];
		}
		printf("Unfiltered auto correlation (msec)\n");
		do_plot6(xx, y1, NULL, NULL, NULL, NULL, NULL, NPER);
	}
#endif /* PLOT_REFRESH */

	/* Apply a gausian filter */
#define FWIDTH 100
	{
		double gaus_[2 * FWIDTH * PBPMS + 1];
		double *gaus = &gaus_[FWIDTH * PBPMS];
		double bb = 1.0/pow(2, 5.0);
		double fw = inttime * 1000.0;
		int ifw;

//printf("~1 sc = %f = %f msec\n",1.0/inttime, fw);
//printf("~1 fw = %f, ifw = %d\n",fw,ifw);

		fw *= 0.9;
		ifw = (int)ceil(fw * PBPMS);
		if (ifw > FWIDTH * PBPMS)
			error("i1pro3: Not enough space for lanczos 2 filter");
		for (j = -ifw; j <= ifw; j++) {
			double x, y;
			x = j/(PBPMS * fw);
			if (fabs(x) > 1.0)
				y = 0.0;
			else
				y = 1.0/pow(2, 5.0 * x * x) - bb;
			gaus[j] = y;
//printf("~1 gaus[%d] = %f\n",j,y);
		}

		for (i = 0; i < NPER; i++) {
			double sum = 0.0;
			double wght = 0.0;
			
			for (j = -ifw; j <= ifw; j++) {
				double w;
				int ix = i + j;
				if (ix < 0)
					ix = -ix;
				if (ix > (NPER-1))
					ix = 2 * NPER-1 - ix;
				w = gaus[j];
				sum += w * tcorr[ix];
				wght += w;
			}
//printf("~1 corr[%d] wgt = %f\n",i,wght);
			corr[i] = sum / wght;
		}
	}

	/* Compute min & max */
	mincv = 1e48, maxcv = -1e48;
	for (i = 0; i < NPER; i++) {
		if (corr[i] > maxcv)
			maxcv = corr[i];
		if (corr[i] < mincv)
			mincv = corr[i];
	}

#endif /* !FREQ_SLOW_PRECISE  Fast - autocorrellate then filter */

		crange = maxcv - mincv;
		a1logd(p->log,3,"Correlation value range %f - %f = %f = %f%%\n",mincv, maxcv,crange, 100.0 * (maxcv-mincv)/maxcv);

#ifdef PLOT_REFRESH
		/* Plot this measuremnts auto correlation */
		{
			double xx[NPER];
			double y1[NPER];
		
			for (i = 0; i < NPER; i++) {
				xx[i] = (i + PERMIN) / (double)PBPMS;			/* msec */
				y1[i] = corr[i];
			}
			printf("Auto correlation (msec)\n");
			do_plot6(xx, y1, NULL, NULL, NULL, NULL, NULL, NPER);
		}
#endif /* PLOT_REFRESH */

#define PFDB 4	// normally 4
		/* If there is sufficient level and distict correlations */
		if (crange/maxcv >= 0.1) {

			a1logd(p->log,PFDB,"Searching for peaks\n");

			/* Locate all the peaks starting at the longest correllation */
			for (i = (NPER-1-PWIDTH); i >= 0 && npeaks < MAXPKS; i--) {
				double v1, v2, v3;
				v1 = corr[i];
				v2 = corr[i + PWIDTH/2];	/* Peak */
				v3 = corr[i + PWIDTH];

				if (fabs(v3 - v1)/crange < 0.05
				 && (v2 - v1)/crange > 0.025
				 && (v2 - v3)/crange > 0.025
				 && (v2 - mincv)/crange > 0.5) {
					double pkv;			/* Peak value */
					int pki;			/* Peak index */
					double ii, bl;

#ifdef PLOT_REFRESH
					a1logd(p->log,PFDB,"Max between %f and %f msec\n",
					       (i + PERMIN)/(double)PBPMS,(i + PWIDTH + PERMIN)/(double)PBPMS);
#endif

					/* Locate the actual peak */
					pkv = -1.0;
					pki = 0;
					for (j = i; j < (i + PWIDTH); j++) {
						if (corr[j] > pkv) {
							pkv = corr[j];
							pki = j;
						}
					}
#ifdef PLOT_REFRESH
					a1logd(p->log,PFDB,"Peak is at %f msec, %f corr\n", (pki + PERMIN)/(double)PBPMS, pkv);
#endif

					/* Interpolate the peak value for higher precision */
					/* j = bigest */
					if (corr[pki-1] > corr[pki+1])  {
						j = pki-1;
						k = pki+1;
					} else {
						j = pki+1;
						k = pki-1;
					}
					bl = (corr[pki] - corr[j])/(corr[pki] - corr[k]);
					bl = (bl + 1.0)/2.0;
					ii = bl * pki + (1.0 - bl) * j;
					pval = (ii + PERMIN)/(double)PBPMS;
#ifdef PLOT_REFRESH
					a1logd(p->log,PFDB,"Interpolated peak is at %f msec\n", pval);
#endif
					peaks[npeaks] = pval;
					peakh[npeaks] = corr[pki];
					npeaks++;

					i -= PWIDTH;
				}
#ifdef NEVER
				if (v2 > v1 && v2 > v3) {
					printf("Peak rehjected:\n");
					printf("(v3 - v1)/crange = %f < 0.05 ?\n",fabs(v3 - v1)/crange);
					printf("(v2 - v1)/crange = %f > 0.025 ?\n",(v2 - v1)/crange);
					printf("(v2 - v3)/crange = %f > 0.025 ?\n",(v2 - v3)/crange);
					printf("(v2 - mincv)/crange = %f > 0.5 ?\n",(v2 - mincv)/crange);
				}
#endif
			}
			a1logd(p->log,3,"Number of peaks located = %d\n",npeaks);

		} else {
			a1logd(p->log,3,"All rejected, crange/maxcv = %f < 0.06\n",crange/maxcv);
		}
#undef PFDB

		a1logd(p->log,3,"Number of peaks located = %d\n",npeaks);

		if (npeaks > 1) {		/* Compute aparent refresh rate */
			int nfails;
			double div, avg, ano;
			/* Try and locate a common divisor amongst all the peaks. */
			/* This is likely to be the underlying refresh rate. */
			for (k = 0; k < npeaks; k++) {
				for (j = 1; j < 25; j++) {
					avg = ano = 0.0;
					div = peaks[k]/(double)j;
					if (div < 5.0)
						continue;		/* Skip anything higher than 200Hz */ 
//printf("~1 trying %f Hz\n",1000.0/div);
					for (nfails = i = 0; i < npeaks; i++) {
						double rem, cnt;
	
						rem = peaks[i]/div;
						cnt = floor(rem + 0.5);
						rem = fabs(rem - cnt);
	
#ifdef PLOT_REFRESH
						a1logd(p->log, 3, "remainder for peak %d = %f\n",i,rem);
#endif
						if (rem > 0.06) {
							if (++nfails > 2)
								break;				/* Fail this divisor */
						} else {
							avg += peaks[i];		/* Already weighted by cnt */
							ano += cnt;
						}
					}
	
					if (nfails == 0 || (nfails <= 2 && npeaks >= 6))
						break;		/* Sucess */
					/* else go and try a different divisor */
				}
				if (j < 25)
					break;			/* Success - found common divisor */
			}
			if (k >= npeaks) {
				a1logd(p->log,3,"Failed to locate common divisor\n");
			
			} else {
				pval = 0.001 * avg/ano;
				if (pval < inttime) {
					a1logd(p->log,3,"Discarding frequency %f > sample rate %f\n",1.0/pval, 1.0/inttime);
				} else {
					pval = 1.0/pval;		/* Convert to frequency */
					rfreq[tix++] = pval;
					a1logd(p->log,3,"Located frequency %f sum %f dif %f\n",pval, pval + 1.0/inttime, fabs(pval - 1.0/inttime));
				}
			}
		}
	}

	if (tix >= 3) {

		for (mm = 0; mm < tix; mm++) {
			a1logd(p->log, 3, "Try %d, samp %f Hz, Meas %f Hz, Sum %f Hz, Dif %f Hz\n",mm,rsamp[mm],rfreq[mm], rsamp[mm] + rfreq[mm], fabs(rsamp[mm] - rfreq[mm]));
		}
	
		/* Decide if we are above the nyquist, or whether */
		/* we have aliases of the fundamental */
		{
			double brange = 1e38;
			double brate = 0.0;
			int bsplit = -1;
			double min, max, avg, range;
			int split, mul, niia;
			
			/* Compute fundamental and sub aliases at all possible splits. */
			/* Skip the reading at the split. */
			for (split = tix; split >= -1; split--) {
				min = 1e38; max = -1e38; avg = 0.0; niia = 0;
				for (mm = 0; mm < tix; mm++) {
					double alias;

					if (mm == split)
						continue;
					if (mm < split)
						alias = rfreq[mm];
					else
						alias = fabs(rsamp[mm] - rfreq[mm]);

					avg += alias;
					niia++;
	
					if (alias < min)
						min = alias;
					if (alias > max)
						max = alias;
				}
				avg /= (double)niia;
				range = (max - min)/(max + min);
//printf("~1 split %d avg = %f, range = %f\n",split,avg,range);
				if (range < brange) {
					brange = range;
					brate = avg;
					bsplit = split;
				}
			}

			/* Compute sub and add aliases at all possible splits */
			/* Skip the reading at the split. */
			for (split = tix; split >= -1; split--) {
				min = 1e38; max = -1e38; avg = 0.0; niia = 0;
				for (mm = 0; mm < tix; mm++) {
					double alias;

					if (mm == split)
						continue;
					if (mm < split)
						alias = fabs(rsamp[mm] - rfreq[mm]);
					else
						alias = rsamp[mm] + rfreq[mm];

					avg += alias;
					niia++;
	
					if (alias < min)
						min = alias;
					if (alias > max)
						max = alias;
				}
				avg /= (double)niia;
				range = (max - min)/(max + min);
//printf("~1 split %d avg = %f, range = %f\n",100 + split,avg,range);
				if (range < brange) {
					brange = range;
					brate = avg;
					bsplit = 100 + split;
				}
			}

			a1logd(p->log, 3, "Selected split %d range %f\n",bsplit,brange);
			
			/* Hmm. Could reject result and re-try if brange is too large ? ( > 0.005 ?) */
		
			if (brange > 0.05) {
				a1logd(p->log, 3, "Readings are too inconsistent (brange %.1f%%) - should retry ?\n",brange * 100.0);
			} else { 

				if (ref_rate != NULL)
					*ref_rate = brate;
		
				/* Error against my 85Hz CRT - GWG */
//				a1logd(p->log, 1, "Refresh rate %f Hz, error = %.4f%%\n",brate,100.0 * fabs(brate - 85.0)/(85.0));
				return I1PRO3_OK;
			}
		}
	} else {
		a1logd(p->log, 3, "Not enough tries suceeded to determine refresh rate\n");
	}

	return I1PRO3_RD_NOREFR_FOUND; 
}
#undef NFSAMPS 
#undef PBPMS
#undef PERMIN
#undef PERMAX
#undef NPER
#undef PWIDTH


/* - - - - - - - - - - - - - */
/* Save/Restore support code */

#ifdef ENABLE_NONVCAL

/* non-volatile save/checksum/restore state */
typedef struct {
	int op;					/* Operation, 0 = just checksum, 1 = write & checksum, 2 = read & checksum */
	i1pro3 *p;
	int ef;					/* Error flag, 1 = write or read failed, 2 = close failed, 3 = malloc */
	unsigned int chsum;		/* Checksum */
	int nbytes;				/* Number of bytes checksummed */
	unsigned char *buf;		/* Temporary buffer */
	unsigned int bsize;		/* Buffer size */
} i1pnonv;

static void update_chsum(i1pnonv *x, unsigned char *p, int nn) {
	int i;
	for (i = 0; i < nn; i++, p++)
		x->chsum = ((x->chsum << 5) | (((1 << 5)-1) & (x->chsum >> (32-5)))) + *p;
	x->nbytes += nn;
}

/* Just checksum, write & checksum, or read & checksum a chunk of data */
static void nv_op(i1pnonv *x, FILE *fp, unsigned char *bp, unsigned int size) {
	int i;

	if (x->ef != 0)
		return;

	if (x->op == 0) {			/* Read into dumy buffer */
		if (size > x->bsize) {
			if ((x->buf = realloc(x->buf, size)) == NULL) {
				a1logd(x->p->log,1,"nv_op: realloc size %u failed at line %d",size);
				x->ef = 3;
				return;
			}
			x->bsize = size;
		}
		bp = x->buf;
	}

	if (x->op == 1) {		/* Write */
		if (fwrite((void *)bp, 1, size, fp) != size) {
			x->ef = 1;
			return;
		}
	} else {				/* Read or just checksum */
		if (fread((void *)bp, 1, size, fp) != size) {
			x->ef = 1;
			return;
		}
	}

	for (i = 0; i < size; i++)
		x->chsum = ((x->chsum << 13) | (x->chsum >> (32-13))) + bp[i];
	x->nbytes += size;
}

/* Operate on an array of ints to/from the file. Set the error flag to nz on error */
static void nv_ints(i1pnonv *x, FILE *fp, int *dp, int n) {
	unsigned char *bp = (unsigned char *)dp;
	unsigned int size = sizeof(int) * n;

	nv_op(x, fp, bp, size);
}

/* Operate on an array of doubles to the file. Set the error flag to nz on error */
static void nv_doubles(i1pnonv *x, FILE *fp, double *dp, int n) {
	unsigned char *bp = (unsigned char *)dp;
	unsigned int size = sizeof(double) * n;

	nv_op(x, fp, bp, size);
}

/* Operate on an array of time_t's to the file. Set the error flag to nz on error */
/* (This will cause file checksum fail if different executables on the same */
/*  system have different time_t values) */
static void nv_time_ts(i1pnonv *x, FILE *fp, time_t *dp, int n) {
	unsigned char *bp = (unsigned char *)dp;
	unsigned int size = sizeof(time_t) * n;

	nv_op(x, fp, bp, size);
}

/* Write an array of ints to the file. Set the error flag to nz on error */
static void write_ints(i1pnonv *x, FILE *fp, int *dp, int n) {

	if (fwrite((void *)dp, sizeof(int), n, fp) != n) {
		x->ef = 1;
	}
}

/* Read an array of ints from the file. Set the error flag to nz on error */
static void read_ints(i1pnonv *x, FILE *fp, int *dp, int n) {

	if (fread((void *)dp, sizeof(int), n, fp) != n) {
		x->ef = 1;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Do a calibration checksum/write & checksum/read & checksum operation */
i1pro3_code i1pro3_nv_op(i1pro3 *p, i1pnonv *x, FILE *fp) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s;
	i1pro3_code ev = I1PRO3_OK;
	int op = x->op;
	int i;

	a1logd(p->log,2,"i1pro3_nv_op %d\n",x->op);

	/* Read the Argyll version and structure signature on just checksum */
	if (op == 0)
		x->op = 2;

	nv_ints(x, fp, &m->nv_av, 1);
	nv_ints(x, fp, &m->nv_ss, 1);
	nv_ints(x, fp, &m->nv_serno, 1);
	nv_ints(x, fp, &m->nv_nraw, 1);
	nv_ints(x, fp, &m->nv_nwav0, 1);
	nv_ints(x, fp, &m->nv_nwav1, 1);

	x->op = op;

	/* Do operation for each mode */
	for (i = 0; i < i1p3_no_modes; i++) {
		s = &m->ms[i];

		/* wl calibration is used by all modes */
		nv_ints(x, fp, &s->wl_valid, 1);
		nv_time_ts(x, fp, &s->wl_date, 1);
		nv_doubles(x, fp, &s->wl_temp, 1);
		nv_doubles(x, fp, &s->wl_cal_raw_off, 1);
		nv_doubles(x, fp, &s->wl_cal_wav_off, 1);

		/* Selected modes save/restore inttime */
		if (s->pol
		 || (s->emiss && !s->scan && s->adaptive)
		 || (s->emiss && s->scan)
		 || (s->trans && s->scan))
			nv_doubles(x, fp, &s->inttime, 1);

		/* Dark calibration */
		if (s->dc_use != i1p3_dc_none) {
			nv_ints(x, fp, &s->dark_valid, 1);
			nv_doubles(x, fp, &s->dtemp, 1);
			nv_time_ts(x, fp, &s->ddate, 1);

		}
		if (s->dc_use == i1p3_dc_adaptive) {
			nv_doubles(x, fp, s->idark_data[0]-1, m->nraw+1);
			nv_doubles(x, fp, s->idark_data[1]-1, m->nraw+1);
		}

		/* Main calibration is always used */
		nv_ints(x, fp, &s->cal_valid, 1);
		nv_time_ts(x, fp, &s->cdate, 1);

		if (s->mc_use == i1p3_mc_mcalfactor) {
			nv_doubles(x, fp, s->cal_factor[0], m->nwav[0]);
			nv_doubles(x, fp, s->cal_factor[1], m->nwav[1]);
		}
		if (s->mc_use == i1p3_mc_mcalfactor) {
			nv_doubles(x, fp, s->raw_white-1, m->nraw+1);
		}
		if (s->mc_use == i1p3_mc_reflective) {
			nv_doubles(x, fp, s->calraw_white[0], m->nraw);
			nv_doubles(x, fp, s->calraw_white[1], m->nraw);
			nv_doubles(x, fp, s->calsp_ledm[0], m->nwav[0]);
			nv_doubles(x, fp, s->calsp_ledm[1], m->nwav[0]);
			nv_doubles(x, fp, s->calsp_illcr[0][0], m->nwav[0]);
			nv_doubles(x, fp, s->calsp_illcr[1][0], m->nwav[1]);
			nv_doubles(x, fp, s->calsp_illcr[0][1], m->nwav[0]);
			nv_doubles(x, fp, s->calsp_illcr[1][1], m->nwav[1]);
			nv_doubles(x, fp, s->iavg2aillum[0], m->nwav[0]);
			nv_doubles(x, fp, s->iavg2aillum[1], m->nwav[1]);
			nv_doubles(x, fp, s->sc_calsp_nn_white[0], m->nwav[0]);
			nv_doubles(x, fp, s->sc_calsp_nn_white[1], m->nwav[1]);
			nv_doubles(x, fp, s->cal_l_uv_diff[0], m->nwav[0]);
			nv_doubles(x, fp, s->cal_l_uv_diff[1], m->nwav[1]);
			nv_doubles(x, fp, s->cal_s_uv_diff[0], m->nwav[0]);
			nv_doubles(x, fp, s->cal_s_uv_diff[1], m->nwav[1]);
		}
		if (s->mc_use == i1p3_mc_polreflective) {
			nv_doubles(x, fp, s->pol_calraw_white, m->nraw);
			nv_doubles(x, fp, s->pol_calsp_ledm, m->nwav[0]);
			nv_doubles(x, fp, s->pol_calsp_white[0], m->nwav[0]);
			nv_doubles(x, fp, s->pol_calsp_white[1], m->nwav[1]);
		}
	}

	return ev;
}

/* Save calibration for all modes, stored on local filesystem */
i1pro3_code i1pro3_save_calibration(i1pro3 *p) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_code ev = I1PRO3_OK;
	int i;
	char nmode[10];
	char cal_name[100];		/* Name */
	char **cal_paths = NULL;
	int no_paths = 0;
	FILE *fp;
	i1pnonv x = { 0 };

	strcpy(nmode, "w");
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	strcat(nmode, "b");
#endif

	/* Create the file name */
	sprintf(cal_name, "ArgyllCMS/.i1p3_%d.cal", m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_write, xdg_user, xdg_none,
		                                                                      cal_name)) < 1) {
		a1logd(p->log,1,"i1pro3_save_calibration xdg_bds returned no paths\n");
		return I1PRO3_INT_CAL_SAVE;
	}

	a1logd(p->log,2,"i1pro3_save_calibration saving to file '%s'\n",cal_paths[0]);

	if (create_parent_directories(cal_paths[0])
	 || (fp = fopen(cal_paths[0], nmode)) == NULL) {
		a1logd(p->log,2,"i1pro3_save_calibration failed to open file for writing\n");
		xdg_free(cal_paths, no_paths);
		return I1PRO3_INT_CAL_SAVE;
	}
	
	x.p = p;			/* Context */
	x.op = 1;			/* Write & checksum */
	x.ef = 0;
	x.chsum = 0;
	x.nbytes = 0;

	/* Setup id info */
	m->nv_av = ARGYLL_VERSION;
	m->nv_ss = sizeof(i1pro3_state) + sizeof(i1pro3imp); /* A crude structure signature */
	m->nv_serno = m->serno;
	m->nv_nraw  = m->nraw;
	m->nv_nwav0 = m->nwav[0];
	m->nv_nwav1 = m->nwav[1];

	/* Write data and compute checksum */
	i1pro3_nv_op(p, &x, fp);

	a1logd(p->log,3,"nbytes = %d, Checkum = 0x%x\n",x.nbytes,x.chsum);
	write_ints(&x, fp, (int *)&x.chsum, 1);

	if (x.ef == 0 && fclose(fp) != 0)
		x.ef = 2;

	if (x.ef != 0) {
		a1logd(p->log,2,"Writing calibration file failed with %d\n",x.ef);
		delete_file(cal_paths[0]);
		return I1PRO3_INT_CAL_SAVE;
	} else {
		a1logd(p->log,2,"Writing calibration file succeeded\n");
	}
	xdg_free(cal_paths, no_paths);

	return ev;
}

/* Restore the all modes calibration from the local system */
i1pro3_code i1pro3_restore_calibration(i1pro3 *p) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_code ev = I1PRO3_OK;
	int i, j;
	char nmode[10];
	char cal_name[100];		/* Name */
	char **cal_paths = NULL;
	int no_paths = 0;
	FILE *fp;
	i1pnonv x = { 0 };
	int chsum;

	strcpy(nmode, "r");
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	strcat(nmode, "b");
#endif
	/* Create the file name */
	sprintf(cal_name, "ArgyllCMS/.i1p3_%d.cal" SSEPS "color/.i1p3_%d.cal", m->serno, m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_read, xdg_user, xdg_none,
		                                                                     cal_name)) < 1) {
		a1logd(p->log,2,"i1pro3_restore_calibration xdg_bds failed to locate file'\n");
		return I1PRO3_INT_CAL_RESTORE;
	}

	a1logd(p->log,2,"i1pro3_restore_calibration restoring from file '%s'\n",cal_paths[0]);

	/* Check the last modification time */
	{
		struct sys_stat sbuf;

		if (sys_stat(cal_paths[0], &sbuf) == 0) {
			m->lo_secs = time(NULL) - sbuf.st_mtime;
			a1logd(p->log,2,"i1pro3_restore_calibration: %d secs from instrument last open\n",m->lo_secs);
		} else {
			a1logd(p->log,2,"i1pro3_restore_calibration: stat on file failed\n");
		}
	}

	if ((fp = fopen(cal_paths[0], nmode)) == NULL) {
		a1logd(p->log,2,"i1pro3_restore_calibration failed to open file for reading\n");
		xdg_free(cal_paths, no_paths);
		return I1PRO3_INT_CAL_RESTORE;
	}

	x.op = 0;			/* Checksum */
	x.ef = 0;
	x.chsum = 0;
	x.nbytes = 0;

	/* Compute checksum */
	i1pro3_nv_op(p, &x, fp);
	a1logd(p->log,3,"nbytes = %d, Checkum = 0x%x\n",x.nbytes,x.chsum);

	read_ints(&x, fp, (int *)&chsum, 1);

	/* Check the file identification */
	if (x.ef != 0
	 || m->nv_av != ARGYLL_VERSION
	 || m->nv_ss != (sizeof(i1pro3_state) + sizeof(i1pro3imp))
	 || m->nv_serno != m->serno
	 || m->nv_nraw  != m->nraw
	 || m->nv_nwav0 != m->nwav[0]
	 || m->nv_nwav1 != m->nwav[1]) {
		a1logd(p->log,2,"Identification didn't verify\n");
		goto reserr;
	}
	a1logd(p->log,3,"i1pro3_restore_calibration id is OK\n");
	
	/* Check the checksum */
	if (x.ef != 0
	 || x.chsum != chsum) {
		a1logd(p->log,2,"Checksum didn't verify, bytes %d, got 0x%x, expected 0x%x\n",x.nbytes,x.chsum, chsum);
		goto reserr;
	}
	a1logd(p->log,3,"i1pro3_restore_calibration checksum is OK\n");

	/* Now that we're happy, read the data. */
	free(x.buf);
	x.buf = NULL;
	x.bsize = 0;

	rewind(fp);

	x.op = 2;			/* Read */
	x.ef = 0;
	x.chsum = 0;
	x.nbytes = 0;

	i1pro3_nv_op(p, &x, fp);

	read_ints(&x, fp, (int *)&chsum, 1);

	/* Just to be sure... */
	if (x.ef != 0
	 || x.chsum != chsum) {
		error("i1pro3: Checksum didn't verify 2nd time, bytes %d, got 0x%x, expected 0x%x\n",x.nbytes,x.chsum, chsum);
	}

	a1logd(p->log,3,"i1pro3_restore_calibration done OK\n");
 reserr:;

	fclose(fp);
	xdg_free(cal_paths, no_paths);

	return ev;
}

i1pro3_code i1pro3_touch_calibration(i1pro3 *p) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_code ev = I1PRO3_OK;
	char cal_name[100];		/* Name */
	char **cal_paths = NULL;
	int no_paths = 0;
	int rv;

	/* Locate the file name */
	sprintf(cal_name, "ArgyllCMS/.i1p3_%d.cal" SSEPS "color/.i1p3_%d.cal", m->serno, m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_read, xdg_user, xdg_none,
		                                                                     cal_name)) < 1) {
		a1logd(p->log,2,"i1pro3_restore_calibration xdg_bds failed to locate file'\n");
		return I1PRO3_INT_CAL_TOUCH;
	}

	a1logd(p->log,2,"i1pro3_touch_calibration touching file '%s'\n",cal_paths[0]);

	if ((rv = sys_utime(cal_paths[0], NULL)) != 0) {
		a1logd(p->log,2,"i1pro3_touch_calibration failed with %d\n",rv);
		xdg_free(cal_paths, no_paths);
		return I1PRO3_INT_CAL_TOUCH;
	}
	xdg_free(cal_paths, no_paths);

	return ev;
}

#endif /* ENABLE_NONVCAL */

/* =============================================== */
/* High res support code                           */

#ifdef HIGH_RES

/* Upsample stray light */
void i1pro3_compute_hr_straylight(i1pro3 *p) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int refl;
	double twidth = HIGHRES_WIDTH;
	int i, j, k, cx, sx;
	double **slp;			/* 2D Array of stray light values */

	/* Then the 2D stray light using linear interpolation */
	slp = dmatrix(0, m->nwav[0]-1, 0, m->nwav[0]-1);

	/* Interpolate points in one dimension */
	for (i = 0; i < m->nwav[0]; i++) {		/* Output wavelength */
		for (j = 0; j < m->nwav[0]; j++) {	/* Input wavelength */

			slp[i][j] = m->straylight[0][i][j];

			/* Use interpolate/extrapolate for middle points */
			if (j == (i-1) || j == i || j == (i+1)) {
				int j0, j1;
				double w0, w1;
				if (j == (i-1)) {
					if (j <= 0)
						j0 = j+3, j1 = j+4;
					else if (j >= (m->nwav[0]-3))
						j0 = j-2, j1 = j-1;
					else
						j0 = j-1, j1 = j+3;
				} else if (j == i) {
					if (j <= 1)
						j0 = j+2, j1 = j+3;
					else if (j >= (m->nwav[0]-2))
						j0 = j-3, j1 = j-2;
					else
						j0 = j-2, j1 = j+2;
				} else if (j == (i+1)) {
					if (j <= 2)
						j0 = j+1, j1 = j+2;
					else if (j >= (m->nwav[0]-1))
						j0 = j-4, j1 = j-3;
					else
						j0 = j-3, j1 = j+1;
				}
				w1 = (j - j0)/(j1 - j0);
				w0 = 1.0 - w1;
				slp[i][j] = w0 * m->straylight[0][i][j0]
				          + w1 * m->straylight[0][i][j1];

			}
		}
	}

	/* Interpolate points in other dimension */
	for (i = 0; i < m->nwav[1]; i++) {		/* Output wavelength */
		for (j = 0; j < m->nwav[1]; j++) {	/* Input wavelength */
			double p0, p1;
			int x0, x1, y0, y1;
			double xx, yy, w0, w1, v0, v1;
			double vv;

			/* Do linear interp with clipping at ends */
			p0 = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], i);
			p1 = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], j);

			xx = (m->nwav[0]-1.0) * (p0 - m->wl_short[0])/(m->wl_long[0] - m->wl_short[0]);
			x0 = (int)floor(xx);
			if (x0 <= 0)
				x0 = 0;
			else if (x0 >= (m->nwav[0]-2))
				x0 = m->nwav[0]-2;
			x1 = x0 + 1;
			w1 = xx - (double)x0;
			w0 = 1.0 - w1;

			yy = (m->nwav[0]-1.0) * (p1 - m->wl_short[0])/(m->wl_long[0] - m->wl_short[0]);
			y0 = (int)floor(yy);
			if (y0 <= 0)
				y0 = 0;
			else if (y0 >= (m->nwav[0]-2))
				y0 = m->nwav[0]-2;
			y1 = y0 + 1;
			v1 = yy - (double)y0;
			v0 = 1.0 - v1;

			vv = w0 * v0 * slp[x0][y0] 
			   + w0 * v1 * slp[x0][y1] 
			   + w1 * v0 * slp[x1][y0] 
			   + w1 * v1 * slp[x1][y1]; 

			m->straylight[1][i][j] = vv * HIGHRES_WIDTH/10.0;
			if (m->straylight[1][i][j] > 0.0)
				m->straylight[1][i][j] = 0.0;
		}
	}

	/* Fix primary wavelength weight and neighbors */
	for (i = 0; i < m->nwav[1]; i++) {		/* Output wavelength */
		double sum;

		if (i > 0)
			m->straylight[1][i][i-1] = 0.0;
		m->straylight[1][i][i] = 0.0;
		if (i < (m->nwav[1]-1))
			m->straylight[1][i][i+1] = 0.0;

		for (sum = 0.0, j = 0; j < m->nwav[1]; j++)
			sum += m->straylight[1][i][j];

		m->straylight[1][i][i] = 1.0 - sum;		/* Total sum should be 1.0 */
	}

#ifdef HIGH_RES_PLOT_STRAYL
	/* Plot original and upsampled reference */
	{
		double *x1 = dvectorz(0, m->nwav[1]-1);
		double *y1 = dvectorz(0, m->nwav[1]-1);
		double *y2 = dvectorz(0, m->nwav[1]-1);
	
		for (i = 0; i < m->nwav[1]; i++) {		/* Output wavelength */
			double wli = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], i);
			int i1 = XSPECT_IX(m->wl_short[0], m->wl_long[0], m->nwav[0], wli);

			for (j = 0; j < m->nwav[1]; j++) {
				double wl = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], j);
				x1[j] = wl;
				y1[j] = m->straylight[1][i][j];
				if (y1[j] == 0.0)
					y1[j] = -8.0;
				else
					y1[j] = log10(fabs(y1[j]));
				if (wli < m->wl_short[0] || wli > m->wl_long[0]
				 || wl < m->wl_short[0] || wl > m->wl_long[0]) {
					y2[j] = -8.0;
				} else {
					double x, wl1, wl2;
					for (k = 0; k < (m->nwav[0]-1); k++) {
						wl1 = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], k);
						wl2 = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], k+1);
						if (wl >= wl1 && wl <= wl2)
							break;
					}
					x = (wl - wl1)/(wl2 - wl1);
					y2[j] = m->straylight[0][i1][k] + (m->straylight[0][i1][k+1]
					      - m->straylight[0][i1][k]) * x;
					if (y2[j] == 0.0)
						y2[j] = -8.0;
					else
						y2[j] = log10(fabs(y2[j]));
				}
			}
			do_plot(x1, y1, y2, NULL, m->nwav[1]);
		}
	
		free_dvector(x1, 0, m->nwav[1]-1);
		free_dvector(y1, 0, m->nwav[1]-1);
		free_dvector(y2, 0, m->nwav[1]-1);
	}
#endif /* HIGH_RES_PLOT_STRAYL */

	free_dmatrix(slp, 0, m->nwav[0]-1, 0, m->nwav[0]-1);
}

/* Linear spectra upsample */
static void linear_upsample(i1pro3 *p, double *hi, double *lo, int clip, char *debug) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	double _lo[36];
	int i;

	if (hi == lo) {					/* Allow for out == in */
		vect_cpy(_lo, lo, 36);
		lo = _lo;
	}

	/* For each output wavelength */
	for (i = 0; i < m->nwav[1]; i++) {
		double y[2], yw;
		double x[2], xw;
		int j, k;

		xw = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], i);

		/* Locate lowres index below it */
		j = (int)floor(XSPECT_DIX(m->wl_short[0], m->wl_long[0], m->nwav[0], xw));
		if (j < 0)
			j = 0;
		if (j > (m->nwav[0]-2))
			j = (m->nwav[0]-2);

		/* Setup the surrounding point values */
		for (k = 0; k < 2; k++) {
			x[k] = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], j+k);
			y[k] = lo[j+k];
		}

		/* Compute interpolated value using Lagrange: */
		yw = (y[1] * (xw - x[0]) + y[0] * (x[1] - xw))/(x[1] - x[0]);
		hi[i] = yw;
	}

	if (clip)
		vect_clip(hi, hi, 0.0, DBL_MAX, m->nwav[1]);

#ifdef HIGH_RES_PLOT
	if (debug != NULL) {
		printf("%s std res (black), high res(red)\n",debug);
		plot_wav_sh(m, lo, hi);
	}
#endif // HIGH_RES_PLOT
}

/* Simple/fast spectra upsample */
/* We use a point by point Lagrange interpolation */
static void fast_upsample(i1pro3 *p, double *hi, double *lo, int clip, char *debug) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	double _lo[36];
	int i;

	if (hi == lo) {					/* Allow for out == in */
		vect_cpy(_lo, lo, 36);
		lo = _lo;
	}

	/* For each output wavelength */
	for (i = 0; i < m->nwav[1]; i++) {
		double y[4], yw;
		double x[4], xw;
		int j, k;

		xw = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], i);

		/* Locate lowres index below it */
		j = (int)floor(XSPECT_DIX(m->wl_short[0], m->wl_long[0], m->nwav[0], xw)) -1;
		if (j < 0)
			j = 0;
		if (j > (m->nwav[0]-4))
			j = (m->nwav[0]-4);

		/* Setup the surrounding point values */
		for (k = 0; k < 4; k++) {
			x[k] = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], j+k);
			y[k] = lo[j+k];
		}

		/* Compute interpolated value using Lagrange: */
		yw = y[0] * (xw-x[1]) * (xw-x[2]) * (xw-x[3])
			      /((x[0]-x[1]) * (x[0]-x[2]) * (x[0]-x[3]))
		   + y[1] * (xw-x[0]) * (xw-x[2]) * (xw-x[3])
			      /((x[1]-x[0]) * (x[1]-x[2]) * (x[1]-x[3]))
		   + y[2] * (xw-x[0]) * (xw-x[1]) * (xw-x[3])
			      /((x[2]-x[0]) * (x[2]-x[1]) * (x[2]-x[3]))
		   + y[3] * (xw-x[0]) * (xw-x[1]) * (xw-x[2])
			      /((x[3]-x[0]) * (x[3]-x[1]) * (x[3]-x[2]));

		hi[i] = yw;
	}

	if (clip)
		vect_clip(hi, hi, 0.0, DBL_MAX, m->nwav[1]);

#ifdef HIGH_RES_PLOT
	if (debug != NULL) {
		printf("%s std res (black), high res(red)\n",debug);
		plot_wav_sh(m, lo, hi);
	}
#endif // HIGH_RES_PLOT
}

/* We super-sample to a common denominator of the current and target */
/* sampling interval, and then downfilter using triangular filtering. */
/* The super-sample interpolation uses a variation on 1D rspl */
/* in which there are 3 weighted fitting goals: */
/* 1) Smoothness */ 
/* 2) Fit to linear interpolated values */
/* 3) Fit of triangular filtered values to input values */
/* The balanance between 1) & 2) sets the smoothness/accuracy tradeoff, */
/* while 3) is given high weight to ensure fidelity to the orginal data. */

#define NN 2		/* [2] super-sampling down multiplier */

#undef UPSAMPLE_PLOT
#undef UPSAMPLE_DEBUG		/* Verbose output */

static void good_upsample(i1pro3 *p, double *hi, double *lo, int clip, char *debug) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int nnus, nnds = NN;		/* upsampling and down sampling multipliers */
	double vrange;				/* value range */
	int nig;					/* Number of super-samples */
	double ss_width, ss_short, ss_long;		/* ss range */
	double t1, t2;
	int ii;
	double cw;			/* Curvature weighting */
	double dw;			/* Data weight */
	int ndiag = 3;		/* Number of diagonals needed on A matrix */
	double **A;			/* A matrix of interpoint weights */
	double *b;			/* b vector for RHS of simultabeous equation */
	double *x;			/* Grid values */
	double tw;			/* Total triangle weight */
	double slo[MX_NWAV];	/* Scaled std. res. input values */
	double chlo[MX_NWAV], cher[MX_NWAV];
	double alo[MX_NWAV];/* Adjusted scaled std. res. input values */
	int j,n,i,k;
	int itter;
	double maxe = 1e38;	/* Itteration error */

	/* Compute up and down sampling ratios */
	t1 = (m->wl_width[0] * NN)/m->wl_width[1];
	nnus = (int)floor(t1 + 0.5);

	if (fabs(t1 - (double)nnus) > 1e-6)
		error("Assert in %s at line %d, hi-res is not multiple of std res\n",__FILE__,__LINE__);

	t1 = (m->wl_short[1] - m->wl_short[0])/m->wl_width[1];
	ii = floor(t1 + 0.5);
	if (fabs(t1 - (double)ii) > 1e-6)
		error("Assert in %s at line %d, hi-res is not aligned to std res\n",__FILE__,__LINE__);

	if (ndiag < (2 * nnus -1))
		ndiag = 2 * nnus - 1;

	/* Compute super-sample range */
	ss_width = m->wl_width[0]/(double)nnus;

	t1 = m->wl_short[0] - m->wl_width[0] + ss_width;
	t2 = m->wl_short[1] - m->wl_width[1] + ss_width;
	if (t2 < t1)
		t1 = t2;
	ss_short = t1;
	
	t1 = m->wl_long[0] + m->wl_width[0] - ss_width;
	t2 = m->wl_long[1] + m->wl_width[1] - ss_width;
	if (t2 > t1)
		t1 = t2;
	ss_long = t1;

	nig = (int)floor((ss_long - ss_short)/ss_width + 0.5);
	
#ifdef UPSAMPLE_DEBUG
	printf("nnus %d nnds %d ndiag %d\n",nnus, nnds, ndiag);
	printf("ss_width %f ss_short %f ss_long %f nig %d\n",ss_width, ss_short, ss_long, nig);
#endif

	/* Figure out the data range */
	t1 = DBL_MAX;
	t2 = -DBL_MAX;
	for (i = 0; i < m->nwav[0]; i++) {
		if (lo[i] < t1)
			t1 = lo[i];
		if (lo[i] > t2)
			t2 = lo[i];
	}
	vrange = 0.5 * (t2 - t1);

	/* Normalize curve weight to grid resolution. */
	cw = 5e-6 * pow((nig-1),4.0) / (nig - 2);	/* [5e-6] */
	
	dw = 1.0;

#ifdef UPSAMPLE_DEBUG
	printf("vrange = %f\n",vrange);
	printf("cw = %f dw = %f\n",cw,dw);
#endif

	x = dvectorz(0, nig);				/* Solution vector */
	A = dmatrix(0, nig, 0, ndiag-1);	/* Diagonal of the A matrix */
	b = dvector(0, nig);				/* RHS */

	for (i = 0; i < m->nwav[0]; i++) {
		slo[i] = lo[i]/vrange;
		alo[i] = slo[i];
	}

	/* Ensure fit to original data by itterating */
	for (itter = 0; itter < 30 && maxe > 0.005; itter++) {

		/* Clear setup matrix/vector */
		for (i = 0; i < nig; i++)
			vect_set(A[i], 0.0, ndiag);
		vect_set(b, 0.0, nig);

		/* Accumulate data dependent factors */
		tw = nnus * nnus;			/* Total weight */
		for (j = 0; j < m->nwav[0]; j++) {
			double tv = 0.0;
			double wl;
			int jj;

			/* super sample value that center of data point falls on */
			wl = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], j);
			jj = XSPECT_IX(ss_short, ss_long, nig, wl);

			/* For each partial derivative */
			for (ii = -nnus+1; ii < nnus; ii++) {
				double wii = (nnus - abs(ii))/tw;
		
				if ((jj + ii) < 0
				 || (jj + ii) >= nig) {
					continue;
				}

				b[jj + ii] -= 2.0 * -alo[j] * wii * dw;

				/* For each component of triangular integration */
				for (i = -nnus+1; i < nnus; i++) {
					double wi;

					if ((i - ii) < 0)
						continue;			/* Skip due to symetry */

					wi = (nnus - abs(i))/tw;
					A[jj + ii][i - ii] += 2.0 * wii * wi * dw;	
				}
			} 
		}

		/* Accumulate curvature dependent factors */
		for (i = 0; i < nig; i++) {
			double tcw = cw;

			if ((i-2) >= 0) {					/* Curvature of cell below */
				A[i][0] +=  2.0 * tcw;
			}

			if ((i-1) >= 0 && (i+1) < nig) {	/* Curvature of this cell */
				A[i][0] +=  8.0 * tcw;
				A[i][1] += -4.0 * tcw;
			}
			if ((i+2) < nig) {					/* Curvature of cell above */
				A[i][0] +=  2.0 * tcw;
				A[i][1] += -4.0 * tcw;
				A[i][2] +=  2.0 * tcw;
			}
		}

#ifdef UPSAMPLE_DEBUG
		printf("b, A matrix:\n");
		for (i = 0; i < nig; i++) {
			printf("b[%d] = %f\n",i,b[i]);
			for (k = 0; k < ndiag; k++)
				printf(" A[%d][%d] = %f\n",i,k,A[i][k]);
		}
#endif /* UPSAMPLE_DEBUG */

		/* Apply Cholesky decomposition to A[][] to create L[][] */
		for (i = 0; i < nig; i++) {
			double sm;
			for (n = 0; n < ndiag; n++) {
				sm = A[i][n];
				for (k = 1; (n+k) < ndiag && (i-k) >= 0; k++)
					sm -= A[i-k][n+k] * A[i-k][k];
				if (n == 0) {
					if (sm <= 0.0)
						error("Assert in %s at line %d, good_upsample loss of resolution\n",__FILE__,__LINE__);
					A[i][0] = sqrt(sm);
				} else {
					A[i][n] = sm/A[i][0];
				}
			}
		}

		/* Solve L . y = b, storing y in x */
		for (i = 0; i < nig; i++) {
			double sm;
			sm = b[i];
			for (k = 1; k < ndiag && (i-k) >= 0; k++)
				sm -= A[i-k][k] * x[i-k];
			x[i] = sm/A[i][0];
		}

		/* Solve LT . x = y */
		for (i = nig-1; i >= 0; i--) {
			double sm;
			sm = x[i];
			for (k = 1; k < ndiag && (i+k) < nig; k++)
				sm -= A[i][k] * x[i+k];
			x[i] = sm/A[i][0];
		}

#ifdef UPSAMPLE_DEBUG
		printf("Solution vector:\n");
		for (i = 0; i < nig; i++) {
			printf("x[%d] = %f\n",i,x[i]);
		}
#endif /* DEBUG */

		/* Compute low res. triangle integration values */
		tw = nnus * nnus;			/* Total weight */
		maxe = -1e38;
		for (j = 0; j < m->nwav[0]; j++) {
			double tv = 0.0;
			double wl, er;
			int jj;

			wl = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], j);
			jj = XSPECT_IX(ss_short, ss_long, nig, wl); 

			/* (This is an optimized triangle integration) */
			for (i = -nnus+1; i < nnus; i++) {
				double wi = (nnus - abs(i))/tw;
				tv += wi * x[jj + i];	
			}
			chlo[j] = tv;
		}
	
		vect_sub3(cher, slo, chlo, m->nwav[0]);
		maxe = vect_max_mag(cher, m->nwav[0]);
		vect_scaleadd(alo, cher, 1.70, m->nwav[0]);
	}
#ifdef UPSAMPLE_PLOT		/* Plot low res input vs. super sample */
	printf("itter %d: lowres in, lowres check (maxe %f):\n",itter,maxe);
	plot_wav2(m, 0, slo, chlo);
#endif

	free_dvector(b, 0, nig);
	free_dmatrix(A, 0, nig, 0, ndiag-1);

	/* Compute hires output values */
	tw = nnds * nnds;		/* Total weight */
	tw /= vrange;			/* but rescale to input range */
	for (j = 0; j < m->nwav[1]; j++) {
		double tv = 0.0;
		double wl;
		int jj;

		wl = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], j);
		jj = XSPECT_IX(ss_short, ss_long, nig, wl); 

		for (i = 0; i < nnds; i++) {
			double wi, wi1;

			wi = nnds - i;
			wi1 = nnds - i - 1;

			/* Area of trapezoid */
			tv += 0.5 * (wi * x[jj + i] + wi1 * x[jj + i + 1]);	/* +ve side of triangle */
			tv += 0.5 * (wi * x[jj - i] + wi1 * x[jj - i - 1]);	/* -ve side of triangle */
		}
		hi[j] = tv/tw;
	} 

#ifdef UPSAMPLE_PLOT		/* Plot low-res vs. super samplea v.s hi-res */
	{
		double xx[1000], y1[1000], y2[1000], y3[1000];
		xspect sp0, sp1;

		sp0.spec_n = m->nwav[0];
		sp0.spec_wl_short = m->wl_short[0];
		sp0.spec_wl_long = m->wl_long[0];
		sp0.norm = 1.0;
		vect_cpy(sp0.spec, lo, m->nwav[0]);

		sp1.spec_n = m->nwav[1];
		sp1.spec_wl_short = m->wl_short[1];
		sp1.spec_wl_long = m->wl_long[1];
		sp1.norm = 1.0;
		vect_cpy(sp1.spec, hi, m->nwav[1]);

		for (j = 0; j < nig; j++) {
			xx[j] = XSPECT_WL(ss_short, ss_long, nig, j);
			y1[j] = value_xspect_lin(&sp0, xx[j]);
			y2[j] = x[j] * vrange;
			y3[j] = value_xspect_lin(&sp1, xx[j]);
		}
		printf("low-res (bk) super sample (r) hi-res (g)\n");
//		do_plot(xx, y1, y2, y3, nig);
		do_plot(xx, y1, y2, NULL, nig);
	}
#endif

	free_dvector(x, 0, nig);			/* Solution vector */

	if (clip)
		vect_clip(hi, hi, 0.0, DBL_MAX, m->nwav[1]);

#ifdef HIGH_RES_PLOT
	if (debug != NULL) {
		printf("%s std res (black), high res(red)\n",debug);
		plot_wav_sh(m, lo, hi);
	}
#endif // HIGH_RES_PLOT
}

#undef NN
#undef UPSAMPLE_PLOT
#undef UPSAMPLE_DEBUG


/* Hack to correct for emissive bumpiness in calibration upsample. */
/* We correct by difference to smooth incendescent lamp spectra. */
/* This is a hack because it's not clear that a correction for one */
/* instance of an instrument is accurate for a different one... */
/* The alternative would be to add an optional calibration step. */
/* (We avoided this in the i1Pro2 by using its own incancescent source) */
void correct_emis_coef(i1pro3 *p, double *hi) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	xspect cor = { 106, 380.0, 730.0, 1.0, 
		{
			1.142277, 0.968452, 1.077103, 1.222850, 1.234457, 1.137349, 1.029250,
			0.987417, 0.979528, 0.997226, 1.007795, 1.022262, 1.029800, 1.028760,
			1.017607, 1.003028, 0.992135, 0.989818, 0.993231, 0.996651, 1.001012,
			1.004000, 1.006764, 1.009298, 1.010818, 1.012484, 1.013909, 1.012393,
			1.008904, 1.005176, 1.002034, 0.999666, 0.999555, 1.000680, 1.000192,
			0.999204, 0.999757, 0.999311, 0.999327, 1.002582, 1.003998, 1.001595,
			1.004868, 1.007035, 1.003921, 1.000437, 1.001987, 1.004170, 1.001211,
			0.998845, 1.001844, 1.001962, 0.993726, 1.002693, 1.000163, 1.000108,
			0.997594, 0.995878, 0.998601, 1.002356, 1.002480, 0.999815, 0.999394,
			1.001485, 1.003212, 1.002075, 1.000501, 0.999974, 0.999697, 0.999111,
			0.998460, 0.999582, 1.001289, 1.002462, 1.001140, 0.999102, 0.997950,
			0.998813, 1.000245, 1.000950, 1.000685, 1.000102, 0.999426, 0.998706,
			0.998156, 0.998011, 0.999170, 1.000538, 1.000933, 0.999226, 0.997238,
			0.996281, 0.998018, 1.001278, 1.003377, 1.001986, 0.997233, 0.992499,
			0.992259, 0.999155, 1.008205, 1.015060, 1.013560, 1.004068, 0.995199,
			1.000362 
		}
	};
	int i;


	for (i = 0; i < cor.spec_n; i++) {
		double wl = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], i);
		hi[i] *= value_xspect_poly(&cor, wl);
	}
}


/* Clear low entries of a spectrum */
void clear_low_wav(i1pro3 *p, double *wav, int sno, int hr) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	double cowl, hix;
	int hno;

	if (hr == 0) {
		vect_set(wav, 0.0, sno);
		return;
	}

	cowl = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], sno - 0.5);	/* Cuttoff */
	cowl -= 0.5 * m->wl_width[1];			/* Center at hr */
	hix = XSPECT_DIX(m->wl_short[1], m->wl_long[1], m->nwav[1], cowl);
	hno = 1 + (int)ceil(hix);				/* hr cuttof at or below sr cuttoff */

	vect_set(wav, 0.0, hno);
}

/* Clear low (short) entries of a spectrum with slope */
void clear_low_wav2(i1pro3 *p, double *wav, double swl, double lwl, int hr) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i;

	i = XSPECT_DIX(m->wl_short[hr], m->wl_long[hr], m->nwav[hr], lwl);
	for (; i >= 0; i--) {
		double wl = XSPECT_WL(m->wl_short[hr], m->wl_long[hr], m->nwav[hr], i);
		double ra = (wl - swl)/(lwl - swl);
		
		if (ra > 1.0)
			ra = 1.0;
		else if (ra < 0.0)
			ra = 0.0;

		wav[i] *= ra;
	}
}

/* Clear high entries of a spectrum */
void clear_high_wav(i1pro3 *p, double *wav, int sno, int hr) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	double cowl, hix;
	int hno;

	if (hr == 0) {
		vect_set(wav + sno, 0.0, m->nwav[0] - sno);
		return;
	}

	cowl = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], sno - 1.5);	/* Cuttoff */
	cowl += 0.5 * m->wl_width[1];			/* Center at hr */
	hix = XSPECT_DIX(m->wl_short[1], m->wl_long[1], m->nwav[1], cowl);
	hno = 1 + (int)floor(hix);				/* hr cuttof at or above sr cuttoff */

	vect_set(wav + hno, 0.0, m->nwav[1] - hno);
}

/* Clear high (long) entries of a spectrum with slope */
void clear_high_wav2(i1pro3 *p, double *wav, double swl, double lwl, int hr) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i;

	i = XSPECT_DIX(m->wl_short[hr], m->wl_long[hr], m->nwav[hr], swl);
	for (; i < m->nwav[hr]; i++) {
		double wl = XSPECT_WL(m->wl_short[hr], m->wl_long[hr], m->nwav[hr], i);
		double ra = (lwl - wl)/(lwl - swl);
		
		if (ra > 1.0)
			ra = 1.0;
		else if (ra < 0.0)
			ra = 0.0;

		wav[i] *= ra;
	}
}


/* Compute smooth edged sum over wl range */
double sum_wav2(i1pro3 *p, double *wav, double sx, double ss, double ee, double ex, int hr) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i, ix1, ix2;
	double sum = 0.0;

	ix1 = XSPECT_DIX(m->wl_short[hr], m->wl_long[hr], m->nwav[hr], sx);
	ix2 = XSPECT_DIX(m->wl_short[hr], m->wl_long[hr], m->nwav[hr], ex) + 1;

	for (i = ix1; i < ix2; i++) {
		double wl = XSPECT_WL(m->wl_short[hr], m->wl_long[hr], m->nwav[hr], i);
		double ra, rb;

		ra = (wl - sx)/(ss - sx);
		rb = (ex - wl)/(ex - ee);
		
		if (ra > 1.0)
			ra = 1.0;
		else if (ra < 0.0)
			ra = 0.0;

		if (rb > 1.0)
			rb = 1.0;
		else if (rb < 0.0)
			rb = 0.0;

		sum += ra * rb * wav[i];
	}

	return sum;
}


#endif /* HIGH_RES */


/* =============================================== */
/* i1Pro3 wavelength calibration                    */

/*
	The i1Pro3 has a wavelength reference LED/thin film filter and
	stores a reference raw spectrum of it in its
	calibrated state, together with two intepolation tables
	defining the raw bin no. to wavelength conversion,
	one for reflection mode, and one for emission mode.

	[ It's a puzzle as to why there are two tables. The emission
	  table is more linear, while by comparisson the reflection
	  mode has some subltle wiggles in the conversion. Possible
	  reasons for this could be:
	  * It's for backwards compatibility with previous X-Rite
	    instruments, which in the past used a flawed wavelength
	    reference.
	  * A different standard wavelength reference was used to
	    calibrate emission and reflection, and these references
	    have historical differences.
	  * There's subtle something going on in the physics of the
	    emission and reflection modes and the nature of
	    the diffraction grating that causes the wavelength
	    calibration to be different. ]

	By measuring the wavelength LED/filter and finding
	the best positional match against the reference
	spectrum, a CCD bin offset can be computed
	to compensate for any shift in the optical or
	physical alignment of spectrum against CCD.

	[ The X-Rite driver applies a correction in nm
	  rather than CCD bin shift. It's a puzzle why
	  they do this, since a first order assumption based
	  on the physics would be that the correction is best
	  done by CCD bin shift. ]

	To use the adjustment, the raw to wave subsampling
	filters need to be regenerated, and to ensure that
	the instrument returns readings very close to the
	manufacturers driver, the same underlying filter
	creation mathematics needs to be used.

	The manufacturers filter weights are the accumulated
	third order Lagrange polynomial weights of the
	integration of a 20 nm wide triange spectrum
	centered at each output wavelength, discretely
	integrated between the range of the middle two points
	of the Lagrange interpolator. The triangle response
	being integrated has an area of exactly 1.0.

 */

/* Invert a raw2wavlength table value. */
static double inv_raw2wav(double wl_cal[128], double inv) {
	double outv;

	outv = vect_rev_lerp(wl_cal, inv, 128);
	outv = 127.0 - 127.0 * outv;

	return outv;
}

/* Return the uncalibrated wavelength given a raw bin value index */
/* using reflective ee_wl_cal. */
static double i1pro3_raw2wav_runc(i1pro3 *p, double raw) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	double ov;

	raw = (127.0 - raw)/127.0;
	ov = vect_lerp(m->ee_wl_cal1, raw, 128);

	return ov;
} 

/* Return the uncalibrated wavelength given a raw bin value index */
/* using emissive ee_wl_cal. */
static double i1pro3_raw2wav_eunc(i1pro3 *p, double raw) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	double ov;

	raw = (127.0 - raw)/127.0;
	ov = vect_lerp(m->ee_wl_cal2, raw, 128);

	return ov;
} 

/* return the calibrated wavelength given a raw bin value for the given mode */
static double i1pro3_raw2wav(i1pro3 *p, int refl, double raw) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	double *wl_cal = refl ? m->ee_wl_cal1 : m->ee_wl_cal2;
	double ov;

	/* Correct for CCD offset and scale back to reference */
	raw = raw - m->wl_raw_off;

//printf("~1 i1pro3_raw2wav: in %f - wl_raw_off %f = %f\n", raw, m->wl_raw_off, raw - m->wl_raw_off);
	raw = (127.0 - raw)/127.0;		/* wl_cal[] expects +ve correlation ? */
	ov = vect_lerp(wl_cal, raw, 128);

	ov = ov - m->wl_wav_off;
//printf("~1 returning %f\n",ov);

	return ov;
}

/* Powell minimisation contxt for WL calibration */
typedef struct {
	double ref_max;		/* reference maximum level */
	double *wl_ref;		/* Wavlength reference samples */
	int wl_ref_n;		/* Number of wavelength references */
	double *wl_meas;	/* Wavelength measurement samples */
	int wl_meas_n;		/* Number of wavelength measurement samples */
	int plot;			/* Plot each try */
} wlcal_cx;

/* Powell minimisation callback function */
/* Parameters being optimized are offset and scale */ 
static double wlcal_opt1(void *vcx, double tp[]) {
#ifdef PLOT_DEBUG
	int pix = 0;
	double xx[1024];
	double y1[1024];		/* interpolate ref */
	double y2[1024];		/* Measurement */
	double y3[1024];		/* Error */
#endif
	wlcal_cx *cx = (wlcal_cx *)vcx;
	double vv, rv = 0.0;
	int si, i;

	si = (int)tp[1];

	/* i = Measurement index */
	for (i = si; i < cx->wl_meas_n; i++) {
		double xv;			/* offset & scaled measurement index */
		int ix;				/* Lagrange base offset */
		double yv;

		if (i < 0)
			continue;

		xv = ((double)i - tp[1]);	/* fitted measurement location in reference no scale */

		ix = ((int)xv) - 1;			/* Reference index of Lagrange for this xv */
		if (ix < 0)
			continue;
		if ((ix + 4) > cx->wl_ref_n)
			break;

		/* Compute interpolated value of reference using Lagrange: */
		yv =  cx->wl_ref[ix+0] * (xv-(ix+1)) * (xv-(ix+2)) * (xv-(ix+3))
		                                             /((0.0-1.0) * (0.0-2.0) * (0.0-3.0))
		   +  cx->wl_ref[ix+1] * (xv-(ix+0)) * (xv-(ix+2)) * (xv-(ix+3))
		                                             /((1.0-0.0) * (1.0-2.0) * (1.0-3.0))
		   +  cx->wl_ref[ix+2] * (xv-(ix+0)) * (xv-(ix+1)) * (xv-(ix+3))
		                                             /((2.0-0.0) * (2.0-1.0) * (2.0-3.0))
		   +  cx->wl_ref[ix+3] * (xv-(ix+0)) * (xv-(ix+1)) * (xv-(ix+2))
		                                             /((3.0-0.0) * (3.0-1.0) * (3.0-2.0));
		vv = yv - tp[0] * cx->wl_meas[i];

		/* Weight error linearly with magnitude, to emphasise peak error */
		/* rather than what's happening down in the noise */
		vv = vv * vv * (yv + 1.0)/(cx->ref_max+1.0);

#ifdef PLOT_DEBUG
		if (cx->plot) {
			xx[pix] = (double)i;
			y1[pix] = yv;
			y2[pix] = tp[0] * cx->wl_meas[i];
//			y3[pix] = 2000.0 * (0.02 + yv/cx->ref_max);		/* Weighting */
			y3[pix] = 0.5 * vv;		/* Error squared */
			pix++;
		}
#endif
		rv += vv;
	}
#ifdef PLOT_DEBUG
	if (cx->plot) {
		printf("Params %f %f -> err %f, Interp Ref (Bk), Meas samples (R), Error (G)\n", tp[0], tp[1], rv);
		do_plot(xx, y1, y2, y3, pix);
	}
#endif
//printf("~1 %f %f -> %f\n", tp[0], tp[1], rv);
	return rv;
}

#ifdef SALONEINSTLIB
/* Do a rudimetrary 2d optimization that uses exaustive */
/* search with hierarchical step sizes */
static int wloptimize(double *cparm,
	double *ss,
	double tol,
	double (*funk)(void *fdata, double tp[]),
	void *fdata
) {
	double range[2][2];			/* [dim][min/max] */
	double val[2];				/* Current test values */
	double bfit = 1e38;			/* Current best fit values */
	int dim;

	for (dim = 0; dim < 2; dim++) {
		range[dim][0] = cparm[dim] - ss[dim];
		range[dim][1] = cparm[dim] + ss[dim];
		val[dim] = cparm[dim];
	}

	/* Until we reach the tollerance */
	for (;;) {
		double mstep = 1e38;

		for (dim = 0; dim < 2; dim++) {
			double stepsz;
			stepsz = (range[dim][1] - range[dim][0])/10.0;
			if (stepsz < mstep)
				mstep = stepsz;

			/* Search in this dimension */
			for (val[dim] = range[dim][0]; val[dim] <= range[dim][1]; val[dim] += stepsz) {
				double fit;
				fit = funk(fdata, val);
				if (fit < bfit) {
					cparm[dim] = val[dim];
					bfit = fit;	
				}
			} 
			val[dim] = cparm[dim];
			range[dim][0] = val[dim] - stepsz;
			range[dim][1] = val[dim] + stepsz;
		}
		if (mstep <= tol)
			break;
	}
	return 0;
}
#endif /* SALONEINSTLIB */


/* Given a raw measurement of the wavelength LED, */
/* Compute the base offset that best fits it to the reference */
/* Returns both raw and wav offset, but only one should be used for correction */
/* (Also sets wl_refpeakloc & wl_refpeakwl) */
i1pro3_code i1pro3_match_wl_meas(i1pro3 *p, double *pwav_off, double *praw_off, double *wlraw) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_code ev = I1PRO3_OK;
	int i;
	int rploc, mploc;		/* Peak offset of ref. and meas. */
	int moff;				/* Base index */
	double lhalf, rhalf;
	double fwhm;			/* Measured half width */
	double rmax, mmax;
	double magscale;
	double raw_off, wav_off;

	/* Do simple match first - locate maximum */
	rmax = -1e6; 
	rploc = -1;
	for (i = 0; i < m->nraw; i++) {	
		if (m->wl_led_spec[i] > rmax) {
			rmax = m->wl_led_spec[i];			/* Max of reference */
			rploc = i;
		}
	}
 
	mmax = -1e6; 
	mploc = -1;
	for (i = 0; i < m->nraw; i++) {
		if (wlraw[i] > mmax) {
			mmax = wlraw[i];					/* Max of measurement */
			mploc = i;
		}
	}
 
	if (mploc < 0 || mploc >= m->nraw) {
		a1logd(p->log,1,"Couldn't locate WL measurement peak\n");
		return I1PRO3_WL_SHAPE; 
	}

	/* Check magnitude is sufficient (not sure this is right, typically 5900 > 882) */
	a1logd(p->log,2,"Measured WL level = %f, minimum needed = %f\n",mmax, m->wl_cal_min_level);
	if (mmax < m->wl_cal_min_level) {
		a1logd(p->log,1,"i1pro3_match_wl_meas peak magnitude too low\n");
		return I1PRO3_WL_TOOLOW;
	}
		
	/* Locate the half peak values */
	for (i = 1; i < mploc; i++) {
		if (wlraw[i] > (mmax/2.0)) {	/* Use linear interp */
			lhalf = (wlraw[i] - mmax/2.0)/(wlraw[i] - wlraw[i-1]);
			lhalf = lhalf * (i-1.0) + (1.0 - lhalf) * (double)i;
			break;
		}
	}
	if (i >= mploc) {
		a1logd(p->log,1,"Couldn't locate WL left half level\n");
		return I1PRO3_WL_SHAPE; 
	}
	for (; i < m->nraw; i++) {
		if (wlraw[i] < (mmax/2.0)) {	/* Use linear interp */
			rhalf = (mmax/2.0 - wlraw[i])/(wlraw[i-1] - wlraw[i]);
			rhalf = rhalf * (i-1.0) + (1.0 - rhalf) * (double)i;
			break;
		}
	}
	if (i >= m->nraw) {
		a1logd(p->log,1,"Couldn't locate WL righ half level\n");
		return I1PRO3_WL_SHAPE; 
	}

	a1logd(p->log,5,"WL half levels at %f (%f nm) and %f (%f nm)\n",lhalf, i1pro3_raw2wav_runc(p, lhalf), rhalf, i1pro3_raw2wav_runc(p, rhalf));

	fwhm = i1pro3_raw2wav_runc(p, lhalf) - i1pro3_raw2wav_runc(p, rhalf);

	a1logd(p->log,3, "WL spectrum fwhm = %f\n",fwhm);
	if (fwhm < (m->wl_cal_fwhm - m->wl_cal_fwhm_tol) 
	 || fwhm > (m->wl_cal_fwhm + m->wl_cal_fwhm_tol)) {
		a1logd(p->log,1,"WL fwhm %f is out of range %f .. %f\n",fwhm,m->wl_cal_fwhm - m->wl_cal_fwhm_tol,m->wl_cal_fwhm + m->wl_cal_fwhm_tol);
		return I1PRO3_WL_SHAPE; 
	}

	moff = mploc - rploc;			/* rough measured raw offset */

	a1logd(p->log,3, "Preliminary WL peak match at ref base offset %d into measurement\n", moff);
	
	magscale = rmax/mmax;		/* Initial scale to make them match */

#ifdef PLOT_DEBUG
	/* Plot the match */
	{
		double xx[1024];
		double y1[1024];
		double y2[1024];

		for (i = 0; i < m->nraw; i++) {
			xx[i] = (double)i;
			y1[i] = 0.0;
			if (i >= moff && (i - moff) < m->nraw) {
				y1[i] = m->wl_led_spec[i- moff];
			}
			y2[i] = wlraw[i] * magscale;
		}
		printf("Simple WL match, ref = black, meas = red:\n");
		do_plot(xx, y1, y2, NULL, m->nraw);
	}
#endif

	/* Now do a good match */
	/*
		Do Lagrange interpolation on the reference curve,
		and use a minimizer to find the best fit (minimum weighted y error)
		by optimizing the magnitude, offset and scale.
	 */

	{
		wlcal_cx cx;
		double cparm[2];		/* fit parameters */
		double ss[2];			/* Search range */
		double athresh;

		cparm[0] = magscale;
		ss[0] = 0.2;
		cparm[1] = (double)moff;
		ss[1] = 4.0;		/* == +- 12 nm */

		cx.ref_max = rmax;
		cx.wl_ref = m->wl_led_spec;
		cx.wl_ref_n = m->nraw;
		cx.wl_meas = wlraw; 
		cx.wl_meas_n = m->nraw;
//		cx.plot = 1;		/* Plot each trial */

		/* We could use the scale to adjust the whole CCD range, */
		/* but the manufacturers driver doesn't seem to do this, */
		/* and it may be making the calibration sensitive to any */
		/* changes in the WL LED/filter spectrum shape. Instead we */
		/* minimize the error weighted for the peak of the shape. */

#ifdef SALONEINSTLIB
		if (wloptimize(cparm, ss, 1e-7, wlcal_opt1, &cx))
			a1logw(p->log,"wlcal_opt1 failed\n");
#else
		if (powell(NULL, 2, cparm, ss, 1e-6, 1000, wlcal_opt1, &cx, NULL, NULL))
			a1logw(p->log,"wlcal_opt1 failed\n");
#endif
		a1logd(p->log,3,"WL best fit parameters: %f %f\n", cparm[0], cparm[1]);

		raw_off = cparm[1];

#ifdef PLOT_DEBUG
		/* Plot the final result */
		printf("Best WL match, ref = black, meas = red, err = green:\n");
		cx.plot = 1;
		wlcal_opt1(&cx, cparm);
#endif

		/* If we have calibrated on the ambient cap, correct */
		/* for the emissive vs. reflective raw2wav scaling factor */

		athresh = 15000.0;
		if (m->aperture)			/* 8mm reduces brightness */
			athresh *= 0.316;

		if (m->filt == inst_opt_filter_pol)		/* Polarizer reduces brightness */
			athresh *= 0.333;
			
		if (mmax < athresh) {
			raw_off += 0.1549;
			a1logd(p->log,3,"Adjusted raw correction to %f to account for measurement using ambient cap\n",raw_off);
		}

		/* Check that the correction in nm is not excessive. */
		/* (Use emissive nm lookup since it is more linear in its conversion) */
		m->wl_refpeakloc = rploc;		/* For later conversions from wl off to raw off */
		m->wl_refpeakwl = i1pro3_raw2wav_eunc(p, rploc);
//printf(" wl_refpeakloc %d wl_refpeakwl %f\n",m->wl_refpeakloc,m->wl_refpeakwl);
		wav_off = i1pro3_raw2wav_eunc(p, rploc + raw_off) - m->wl_refpeakwl;
		a1logd(p->log,2, "Final WL raw offset = %f, wav offset %f nm\n",raw_off, wav_off);
		if (fabs(wav_off)> m->wl_err_max) {
			a1logd(p->log,1,"Final WL correction of %f nm is too big\n",wav_off);
			return I1PRO3_WL_ERR2BIG;
		}

		/* Do a verification plot */
		/* Plot the measurement against calibrated wavelength, */
		/* and reference measurement verses reference wavelength */
	
#ifdef PLOT_DEBUG
		{
			double xx[1024];
			double y1[1024];		/* interpolate ref */
			double y2[1024];		/* Measurement */
			int ii;

			/* i = index into measurement */
			for (ii = 0, i = 0; i < m->nraw; i++) {
				double raw;
				double mwl;			/* Measurment wavelength */
				double rraw;		/* Reference raw value */
				int ix;				/* Lagrange base offset */
				int k;
				double yv;

				raw = (double)i;

				raw = raw - raw_off;
				mwl = i1pro3_raw2wav_runc(p, raw);		// Using m->ee_wl_cal1
				xx[ii] = mwl;
				y1[ii] = cparm[0] * wlraw[i];
				y2[ii] = 0.0;

				/* Compute the reference index corresponding to this wavelength */
				rraw = inv_raw2wav(m->ee_wl_cal1, mwl);

				/* Use Lagrange to interpolate the reference level for this wavelength */
				ix = ((int)rraw) - 1;	/* Reference index of Lagrange for this xv */
				if (ix < 0)
					continue;
				if ((ix + 3) >= m->nraw)
					break;

				/* Compute interpolated value of reference using Lagrange: */
				yv =  m->wl_led_spec[ix+0] * (rraw-(ix+1)) * (rraw-(ix+2)) * (rraw-(ix+3))
				                                             /((0.0-1.0) * (0.0-2.0) * (0.0-3.0))
				   +  m->wl_led_spec[ix+1] * (rraw-(ix+0)) * (rraw-(ix+2)) * (rraw-(ix+3))
				                                             /((1.0-0.0) * (1.0-2.0) * (1.0-3.0))
				   +  m->wl_led_spec[ix+2] * (rraw-(ix+0)) * (rraw-(ix+1)) * (rraw-(ix+3))
				                                             /((2.0-0.0) * (2.0-1.0) * (2.0-3.0))
				   +  m->wl_led_spec[ix+3] * (rraw-(ix+0)) * (rraw-(ix+1)) * (rraw-(ix+2))
				                                             /((3.0-0.0) * (3.0-1.0) * (3.0-2.0));
				y2[ii] = yv;
				ii++;
			}
			printf("Verification fit in nm:\n");
			do_plot(xx, y1, y2, NULL, ii);
		}
#endif

		if (praw_off != NULL)
			*praw_off = raw_off;
		if (pwav_off != NULL)
			*pwav_off = wav_off;
	}

	return ev; 
}

/* Recompute normal & hi-res wav filters using the values given, */
/* if they are sufficiently different from current or force. */
i1pro3_code i1pro3_compute_wav_filters(i1pro3 *p, double wl_raw_off, double wl_wav_off, int force) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_code ev = I1PRO3_OK;
	int ref, hr;
	double rdiff, wdiff;

	a1logd(p->log,3,"i1pro3_compute_wav_filter() raw %f wav %f force %d\n",wl_raw_off, wl_wav_off, force);

	/* Check them */
	rdiff = fabs(m->wl_raw_off - wl_raw_off);
	wdiff = fabs(m->wl_wav_off - wl_wav_off);

	if (!force && rdiff < 0.03 && wdiff < 0.09999) {
		a1logd(p->log,3,"i1pro3_compute_wav_filter() ignored because rdiff %f wdiff %f\n",rdiff,wdiff);
		return I1PRO3_OK;
	}

	/* Use them */
	m->wl_raw_off = wl_raw_off;
	m->wl_wav_off = wl_wav_off;

	/* Compute the emissive, reflective standard and hi-res raw->wav sampling filters */
	for (hr = 0; hr < 2; hr++) {
		for (ref = 0; ref < 2; ref++) {
			if ((ev = i1pro3_compute_wav_filter(p, hr, ref)) != I1PRO3_OK) {
				a1logd(p->log,2,"i1pro3_compute_wav_filter() failed\n");
				return ev;
			}
		}
	}

	return I1PRO3_OK;
}

/* Set the current wl_raw/wav_off for the given board temperature and then */
/* recompute normal & hi-res wav filters if the wl offset has changed. */
i1pro3_code i1pro3_recompute_wav_filters_for_temp(i1pro3 *p, double temp) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	double wl_raw_off = 0.0, wl_wav_off = 0.0;

	a1logd(p->log,2,"i1pro3_recompute_wav_filters_for_temp got del temp %f\n",temp - s->wl_temp);

	if (m->wl_tempcoef == 0.0 || (temp - s->wl_temp) == 0.0)
		return I1PRO3_OK;

	if (s->wl_cal_raw_off != 0.0) {
		double delwl = - (temp - s->wl_temp) * m->wl_tempcoef;
		double delraw = inv_raw2wav(m->ee_wl_cal2, m->wl_refpeakwl + delwl) - m->wl_refpeakloc;
		wl_raw_off = s->wl_cal_raw_off + delraw;
		a1logd(p->log,2," del wl offset %f del raw %f\n",delwl,delraw);

#ifdef NEVER 	// verify raw offset in wl
		delwl = i1pro3_raw2wav_eunc(p, m->wl_refpeakloc + delraw)
		      - i1pro3_raw2wav_eunc(p, m->wl_refpeakloc);

		a1logd(p->log,2," verify del wl %f\n",delwl);
#endif
	}  else {
		double delwl = - (temp - s->wl_temp) * m->wl_tempcoef;
		wl_wav_off = s->wl_cal_wav_off + delwl;

		a1logd(p->log,2," del wl offset %f\n",delwl);
	}

	return i1pro3_compute_wav_filters(p, wl_raw_off, wl_wav_off, 0);
}


/* Compute standard/high res. downsampling filters for the current mode */
/* given the current wl_raw_off/wl_wav_off, and set them as current, */
/* using triangular filters of the lagrange interpolation of the */
/* CCD values (i.e. the same type of filter used by the OEM driver) */
/* [ Interestingly, the resulting filter shape is a bit like lanczos2, */
/*   but not identical. ] */
i1pro3_code i1pro3_compute_wav_filter(i1pro3 *p, int hr, int refl) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	double twidth;		/* Target filter width */
	int six, eix;		/* raw starting index and one past end index */ 
	int wlix;			/* current wavelenght index */
	double *wlcop;		/* This wavelength base filter coefficient pointer */
	double trh, trx;	/* Triangle height and triangle equation x weighting */
	int i, j, k;
	int r2wt = refl;		/* raw2wav lookup table to use */

	// printf("i1pro3_compute_wav_filter called hr %d refl %d with correction %f raw %f wav\n",hr,refl,m->wl_raw_off, m->wl_wav_off);

	a1logd(p->log,2,"i1pro3_compute_wav_filter called hr %d refl %d with correction %f raw %f wav\n",hr,refl,m->wl_raw_off,m->wl_wav_off);

	twidth = (m->wl_long[hr] - m->wl_short[hr])/(m->nwav[hr] - 1.0);	/* Filter width */

	trh = 1.0/twidth;		/* Triangle height */
	trx = trh/twidth;		/* Triangle equation x weighting */

	/* Allocate space for the filter coefficients */
	if (m->mtx[hr][refl].index == NULL) {

		if ((m->mtx[hr][refl].index = (int *)calloc(m->nwav[hr], sizeof(int))) == NULL) {
			a1logd(p->log,1,"i1pro3: malloc index failed!\n");
			return I1PRO3_INT_MALLOC;
		}

		if ((m->mtx[hr][refl].nocoef = (int *)calloc(m->nwav[hr], sizeof(int))) == NULL) {
			a1logd(p->log,1,"i1pro3: malloc nocoef failed!\n");
			return I1PRO3_INT_MALLOC;
		}

		if ((m->mtx[hr][refl].coef = (double *)calloc(16 * m->nwav[hr], sizeof(double)))
		                                                                        == NULL) {
			a1logd(p->log,1,"i1pro3: malloc coef failed!\n");
			return I1PRO3_INT_MALLOC;
		}
	}

	/* For each output wavelength */
	wlcop = m->mtx[hr][refl].coef;
	for (wlix = 0; wlix < m->nwav[hr]; wlix++) {
		double owl = wlix/(m->nwav[hr]-1.0) * (m->wl_long[hr] - m->wl_short[hr]) + m->wl_short[hr];
		int lip;	/* Lagrange interpolation position */

		//printf("Generating filter for %.1f nm width %.1f nm\n",owl, twidth);
		//printf("~1 coefix = %d\n",wlcop - m->mtx[hr][refl].coef);

		/* The filter is based on a triangle centered at owl and extending */
		/* from owl - twidth to owl + twidth. We therefore need to locate the */
		/* raw values that will overlap this range */

		/* Do a dumb search from high to low nm to find raw index range */

		for (six = 0; six < m->nraw; six++) {
			//printf("~1 (raw2wav ix %d -> %f <? (owl %f + twidth %f) %f\n",six,i1pro3_raw2wav(p, r2wt, (double)six),owl,twidth,owl + twidth);
			if (i1pro3_raw2wav(p, r2wt, (double)six) < (owl + twidth))
				break;
		}

		if (six < 2 || six >= m->nraw) {
			a1loge(p->log,1,"i1pro3: compute_wav_filters() six %d, exceeds raw range to cover output filter %.1f nm width %.1f nm\n",six, owl, twidth);
			return I1PRO3_INT_ASSERT;
		}
		eix = six;
		six -= 2;		/* Outside */

		/* Continue search for other edge */
		for (; eix < m->nraw; eix++) {
			if (i1pro3_raw2wav(p, r2wt, (double)eix) <= (owl - twidth))
				break;
		}
		if (eix > (m->nraw - 2) ) {
			a1loge(p->log,1,"i1pro3: compute_wav_filters() eix %d, exceeds raw range to cover output filter %.1f nm width %.1f nm\n",eix, owl, twidth);
			return I1PRO3_INT_ASSERT;
		}
		eix += 2;		/* Outside */

		//for (j = six; j < eix; j++) printf("Using raw %d @ %.1f nm del %.1f\n",j, i1pro3_raw2wav(p, r2wt, (double)j), i1pro3_raw2wav(p, r2wt, (double)j) - owl);

		/* Set start index for this wavelength */
		m->mtx[hr][refl].index[wlix] = six;

		/* Set number of filter coefficients */
		m->mtx[hr][refl].nocoef[wlix] = eix - six;

		if (m->mtx[hr][refl].nocoef[wlix] > 16) {
			a1loge(p->log,1,"i1pro3: compute_wav_filters() too many filter %d\n",
			                                      m->mtx[hr][refl].nocoef[wlix]);
			return I1PRO3_INT_ASSERT;
		}

		/* Start with zero filter weightings */
		for (i = 0; i < m->mtx[hr][refl].nocoef[wlix]; i++)
			wlcop[i] = 0.0;
		
		/* For each Lagrange interpolation position (adjacent CCD locations) */
		/* create the Lagrange and then acumulate the integral of the convolution */
		/* of the overlap of the central region, with the triangle of our */
		/* underlying re-sampling filter. */
		/* (If we were to run out of enough source points for the Lagrange to */
		/* encompas the region, then in theory we could use the Lagrange to */
		/* extrapolate beyond the end from points within.) */
		for (lip = six; (lip + 3) < eix; lip++) {
			double rwav[4];		/* Relative wavelength of these Lagrange points */
			double den[4];		/* Denominator values for points */
			double num[4][4];	/* Numerator polinomial components x^3, x^2, x, 1 */
			double ilow, ihigh;	/* Integration points */
		
			/* Relative wavelengths to owl of each basis point */
			for (i = 0; i < 4; i++)
				rwav[i] = i1pro3_raw2wav(p, r2wt, (double)lip + i) - owl;

			//printf("\n~1 rwav = %f %f %f %f\n", rwav[0], rwav[1], rwav[2], rwav[3]);

			/* Compute each basis points Lagrange denominator values */
			den[0] = (rwav[0]-rwav[1]) * (rwav[0]-rwav[2]) * (rwav[0]-rwav[3]);
			den[1] = (rwav[1]-rwav[0]) * (rwav[1]-rwav[2]) * (rwav[1]-rwav[3]);
			den[2] = (rwav[2]-rwav[0]) * (rwav[2]-rwav[1]) * (rwav[2]-rwav[3]);
			den[3] = (rwav[3]-rwav[0]) * (rwav[3]-rwav[1]) * (rwav[3]-rwav[2]);
			//printf("~1 denominators = %f %f %f %f\n", den[0], den[1], den[2], den[3]);
	
			/* Compute each basis points Langrange numerator components. */
			/* We make the numerator have polinomial form, so that it is easy */
			/* to compute the integral equation from it. */
			num[0][0] = 1.0;
			num[0][1] = -rwav[1] - rwav[2] - rwav[3];
			num[0][2] = rwav[1] * rwav[2] + rwav[1] * rwav[3] + rwav[2] * rwav[3];
			num[0][3] = -rwav[1] * rwav[2] * rwav[3];
			num[1][0] = 1.0;
			num[1][1] = -rwav[0] - rwav[2] - rwav[3];
			num[1][2] = rwav[0] * rwav[2] + rwav[0] * rwav[3] + rwav[2] * rwav[3];
			num[1][3] = -rwav[0] * rwav[2] * rwav[3];
			num[2][0] = 1.0;
			num[2][1] = -rwav[0] - rwav[1] - rwav[3];
			num[2][2] = rwav[0] * rwav[1] + rwav[0] * rwav[3] + rwav[1] * rwav[3];
			num[2][3] = -rwav[0] * rwav[1] * rwav[3];
			num[3][0] = 1.0;
			num[3][1] = -rwav[0] - rwav[1] - rwav[2];
			num[3][2] = rwav[0] * rwav[1] + rwav[0] * rwav[2] + rwav[1] * rwav[2];
			num[3][3] = -rwav[0] * rwav[1] * rwav[2];

			//printf("~1 num %d = %f %f %f %f\n", 0, num[0][0], num[0][1], num[0][2], num[0][3]);
			//printf("~1 num %d = %f %f %f %f\n", 1, num[1][0], num[1][1], num[1][2], num[1][3]);
			//printf("~1 num %d = %f %f %f %f\n", 2, num[2][0], num[2][1], num[2][2], num[2][3]);
			//printf("~1 num %d = %f %f %f %f\n", 3, num[3][0], num[3][1], num[3][2], num[3][3]);

			/* Now compute the integral difference between the two middle points */
			/* of the Lagrange over the triangle shape, and accumulate the resulting */
			/* Lagrange weightings to the filter coefficients. */

			/* For high and then low side of the triangle. */
			for (k = 0; k < 2; k++) {

				ihigh = rwav[1]; 
				ilow = rwav[2]; 

				/* Over just the central portion, if it overlaps the triangle. */
				if ((k == 0 && ilow <= twidth && ihigh >= 0.0)		/* Portion is +ve side */
				 || (k == 1 && ilow <= 0.0 && ihigh >= -twidth)) {	/* Portion is -ve side */

					if (k == 0) {
						if (ilow < 0.0)
							ilow = 0.0;
						if (ihigh > twidth)
							ihigh = twidth;
						//printf("~1 doing +ve triangle between %f %f\n",ilow,ihigh);
					} else {
						if (ilow < -twidth)
							ilow = -twidth;
						if (ihigh > 0.0)
							ihigh = 0.0;
						//printf("~1 doing -ve triangle between %f %f\n",ilow,ihigh);
					}

					/* For each Lagrange point */
					for (i = 0; i < 4; i++) {
						double xnum[5];			/* Expanded numerator components */
						double nvall, nvalh;	/* Numerator low and high values */

						/* Because the y value is a function of x, we need to */
						/* expand the Lagrange 3rd order polinomial into */
						/* a 4th order polinomial using the triangle edge equation */
						/* y = trh +- trx * x */
						for (j = 0; j < 4; j++)
							xnum[j] = (k == 0 ? -trx : trx) * num[i][j];
						xnum[j] = 0.0;
						for (j = 0; j < 4; j++)
							xnum[j+1] += trh * num[i][j];

						/* The 4th order equation becomes a 5th order one */
						/* when we convert it to an integral, ie. x^4 becomes x^5/5 etc. */
						for (j = 0; j < 4; j++)
							xnum[j] /= (5.0 - (double)j);	/* Integral denom. */

						/* Compute ihigh integral as 5th order polynomial */
						nvalh = xnum[0];
						nvalh = nvalh * ihigh + xnum[1];
						nvalh = nvalh * ihigh + xnum[2];
						nvalh = nvalh * ihigh + xnum[3];
						nvalh = nvalh * ihigh + xnum[4];
						nvalh = nvalh * ihigh;

						/* Compute ilow integral as 5th order polynomial */
						nvall = xnum[0];
						nvall = nvall * ilow + xnum[1];
						nvall = nvall * ilow + xnum[2];
						nvall = nvall * ilow + xnum[3];
						nvall = nvall * ilow + xnum[4];
						nvall = nvall * ilow;

						/* Compute ihigh - ilow and add to filter weightings */
						wlcop[lip -six + i] += (nvalh - nvall)/den[i];
						//printf("~1 k = %d, comp %d weight += %e den %e now %e\n",k,lip-six+i,(nvalh - nvall)/den[i], den[i], wlcop[lip-six+i]);
					}
				}
			}
		}
		//printf("~1 Weightings for %.1f nm are:\n",owl);
		//for (i = 0; i < m->mtx[hr][refl].nocoef[wlix]; i++)
		//printf("~1 comp %d rix %d weight %g\n",i,m->mtx[0][refl].index[wlix] + i, wlcop[i]);
		//printf("\n\n");

		wlcop += m->mtx[hr][refl].nocoef[wlix];		/* Next group of weightings */
	}

#ifdef HIGH_RES_PLOT_WAVFILT
	/* Plot standard res. raw->wav re-sampling filters */
	{
		int cx;
		double *xx, *ss;
		double **yy;

		xx = dvectorz(-1, m->nraw-1);		/* X index */
		yy = dmatrixz(0, 5, -1, m->nraw-1);	/* Curves distributed amongst 5 graphs */
		
		for (i = 0; i < m->nraw; i++)
			xx[i] = i;

		/* For each output wavelength */
		for (cx = j = 0; j < m->nwav[0]; j++) {
			int sx;

			i = j % 5;

			/* For each matrix value */
			sx = m->mtx[0][refl].index[j];		/* Starting raw index */
			for (k = 0; k < m->mtx[0][refl].nocoef[j]; k++, cx++, sx++) {
				yy[5][sx] += 0.5 * m->mtx[0][refl].coef[cx];	/* Sum of coefs */
				yy[i][sx] = m->mtx[0][refl].coef[cx];
			}
		}

		printf("raw->wav filter curves %s:\n",refl ? "refl" : "emis");
		do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
		free_dvector(xx, -1, m->nraw-1);
		free_dmatrix(yy, 0, 2, -1, m->nraw-1);
	}
#endif /* PLOT_DEBUG */

	return ev;
}

/* Dump the contents of a raw->wav filter to stdout */
i1pro3_code i1pro3_dump_wav_filters(i1pro3 *p, int hr, int refl) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	int cx;
	int j, k;
	i1pro3_code ev = I1PRO3_OK;

	/* For each output wavelength */
	for (cx = j = 0; j < m->nwav[hr]; j++) {

		/* For each matrix value */
		for (k = 0; k < m->mtx[hr][refl].nocoef[j]; k++, cx++) {
			printf("wl %d ix %d coef %.15e\n", j, k, m->mtx[hr][refl].coef[cx]);
		}
	}
	return ev;
}


/* =============================================== */

/* return nz if high res is supported at all */
int i1pro3_imp_highres(i1pro3 *p) {
#ifdef HIGH_RES
	return 1;
#else
	return 0;
#endif /* HIGH_RES */
}

/* Set to high resolution mode */
/* Note that it's important to set mode first... */
i1pro3_code i1pro3_set_highres(i1pro3 *p) {
	int i;
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;

#ifdef HIGH_RES
	if (!s->reflective)
		m->highres = 1;
	else
		ev = I1PRO3_UNSUPPORTED;
#else
	ev = I1PRO3_UNSUPPORTED;
#endif /* HIGH_RES */

	return ev;
}

/* Set to standard resolution mode */
i1pro3_code i1pro3_set_stdres(i1pro3 *p) {
	int i;
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_code ev = I1PRO3_OK;

#ifdef HIGH_RES
	m->highres = 0;
#else
	ev = I1PRO3_UNSUPPORTED;
#endif /* HIGH_RES */

	return ev;
}

/* =============================================== */

/* Modify the scan consistency tolerance */
i1pro3_code i1pro3_set_scan_toll(i1pro3 *p, double toll_ratio) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_code ev = I1PRO3_OK;

	if (toll_ratio < 1e-6)
		toll_ratio = 1e-6;

	m->scan_toll_ratio = toll_ratio;

	return I1PRO3_OK;
}


/* Optical adjustment weights */
static double opt_adj_weights[21] = {
	1.4944496665144658e-282, 2.0036175483913455e-070, 1.2554893022685038e+232,
	2.3898157055642966e+190, 1.5697625128432372e-076, 6.6912978722191457e+281,
	1.2369092402930559e+277, 1.4430907501246712e-153, 3.0017439193018232e+238,
	1.2978311824382444e+161, 5.5068703318775818e-311, 7.7791723264455314e-260,
	6.4560484084110176e+170, 8.9481529920968425e+165, 1.3565405878488529e-153,
	2.0835868791190880e-076, 5.4310198502711138e+241, 4.8689849775675438e+275,
	9.2709981544886391e+122, 3.7958270103353899e-153, 7.1366083837501666e-154
};

/* Convert from spectral to XYZ, and transfer to the ipatch array. */
/* Apply XRGA conversion if needed */
i1pro3_code i1pro3_conv2XYZ(
	i1pro3 *p,
	ipatch *vals,		/* Values to return */
	int nvals,			/* Number of values */
	double **specrd,	/* Spectral readings */
	int hr,				/* 0 for std. res., 1 for high-res */
	instClamping clamp	/* Clamp XYZ/Lab to be +ve */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	xsp2cie *conv;	/* Spectral to XYZ conversion object */
	int i, j, k;
	int six = 0;		/* Starting index */
	int nwl = m->nwav[hr];	/* Number of wavelength */
	double wl_short = m->wl_short[hr];	/* Starting wavelength */
	double sms;			/* Weighting */

	if (s->emiss)
		conv = new_xsp2cie(icxIT_none, 0.0, NULL, icxOT_CIE_1931_2, NULL, icSigXYZData, (icxClamping)clamp);
	else
		conv = new_xsp2cie(icxIT_D50, 0.0, NULL, icxOT_CIE_1931_2, NULL, icSigXYZData, (icxClamping)clamp);
	if (conv == NULL)
		return I1PRO3_INT_CIECONVFAIL;


	/* Don't report any wavelengths below the minimum for this mode */
	if ((s->min_wl-1e-3) > wl_short) {
		double wl = 0.0;
		for (j = 0; j < m->nwav[hr]; j++) {
			wl = XSPECT_WL(m->wl_short[hr], m->wl_long[hr], m->nwav[hr], j);
			if (wl >= (s->min_wl-1e-3))
				break;
		}
		six = j;
		wl_short = wl;
		nwl -= six;
	}

	a1logd(p->log,5,"i1pro3_conv2XYZ got wl_short %f, wl_long %f, nwav %d, min_wl %f\n",
	                m->wl_short[hr], m->wl_long[hr], m->nwav[hr], s->min_wl);
	a1logd(p->log,5,"      after skip got wl_short %f, nwl = %d\n", wl_short, nwl);

	for (sms = 0.0, i = 1; i < 21; i++)
		sms += opt_adj_weights[i];
	sms *= opt_adj_weights[0];

	for (i = 0; i < nvals; i++) {
		vals[i].loc[0] = '\000';
		vals[i].mtype = inst_mrt_none;
		vals[i].mcond = inst_mrc_none;
		vals[i].XYZ_v = 0;
		vals[i].sp.spec_n = 0;
		vals[i].duration = 0.0;
	
		vals[i].sp.spec_n = nwl;
		vals[i].sp.spec_wl_short = wl_short;
		vals[i].sp.spec_wl_long = m->wl_long[hr];

		if (s->emiss) {
			/* Leave spectral values as mW/m^2 */
			for (j = six, k = 0; j < m->nwav[hr]; j++, k++) {
				vals[i].sp.spec[k] = specrd[i][j] * sms;
			}
			vals[i].sp.norm = 1.0;

			/* Set the XYZ */
			conv->convert(conv, vals[i].XYZ, &vals[i].sp);
			vals[i].XYZ_v = 1;

			if (s->ambient) {
				if (s->flash)
					vals[i].mtype = inst_mrt_ambient_flash;
				else
					vals[i].mtype = inst_mrt_ambient;
			} else {
				if (s->flash)
					vals[i].mtype = inst_mrt_emission_flash;
				else
					vals[i].mtype = inst_mrt_emission;
			}

		} else {
			/* Scale spectral values to percentage reflectance/transmission */
			for (j = six, k = 0; j < m->nwav[hr]; j++, k++)
				vals[i].sp.spec[k] = 100.0 * specrd[i][j] * sms;
			vals[i].sp.norm = 100.0;

			/* Set the XYZ */
			conv->convert(conv, vals[i].XYZ, &vals[i].sp);
			vals[i].XYZ_v = 1;
			vals[i].XYZ[0] *= 100.0;
			vals[i].XYZ[1] *= 100.0;
			vals[i].XYZ[2] *= 100.0;

			if (s->trans)
				vals[i].mtype = inst_mrt_transmissive;
			else {
				vals[i].mtype = inst_mrt_reflective;
				if (m->filt == inst3_filter_D50)
					vals[i].mcond = inst_mrc_D50;
				else if (m->filt == inst3_filter_UVCut) 
					vals[i].mcond = inst_mrc_uvcut;
				else if (m->filt == inst3_filter_pol) 
					vals[i].mcond = inst_mrc_pol;
			}
		}

		/* Don't return spectral if not asked for */
		if (!m->spec_en) {
			vals[i].sp.spec_n = 0;
		}

	}

	conv->del(conv);

	/* Apply any XRGA conversion */
	ipatch_convert_xrga(vals, nvals, xcalstd_nonpol, m->target_calstd, m->native_calstd, clamp);

	/* Apply custom filter compensation */
	if (m->custfilt_en)
		ipatch_convert_custom_filter(vals, nvals, &m->custfilt, clamp);

	return I1PRO3_OK;
}

/* Compute the number of measurements needed, given the target */
/* measurement time and integration time. Will return a minimum, of 1 */
int i1pro3_comp_nummeas(
	i1pro3 *p,
	double meas_time,
	double int_time
) {
	int nmeas;
	if (int_time <= 0.0 || meas_time <= 0.0)
		return 1;
	nmeas = (int)floor(meas_time/int_time + 0.5);
	if (nmeas < 1)
		nmeas = 1;
	return nmeas;
}

/* Set the noinitcalib mode */
void i1pro3_set_noinitcalib(i1pro3 *p, int v, int losecs) {
	i1pro3imp *m = (i1pro3imp *)p->m;

	/* Ignore disabling init calib if more than losecs since instrument was open */
	if (v && losecs != 0 && m->lo_secs >= losecs) {
		a1logd(p->log,3,"initcalib disable ignored because %d >= %d secs\n",m->lo_secs,losecs);
		return;
	}
	m->noinitcalib = v;
}

/* Set the trigger config */
void i1pro3_set_trig(i1pro3 *p, inst_opt_type trig) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	m->trig = trig;
}

/* Return the trigger config */
inst_opt_type i1pro3_get_trig(i1pro3 *p) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	return m->trig;
}

/* Switch thread handler */
int i1pro3_event_thread(void *pp) {
	int nfailed = 0;
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_code rv = I1PRO3_OK; 
	a1logd(p->log,3,"Switch thread started\n");

//	for (nfailed = 0;nfailed < 5;)
	/* Try indefinitely, in case instrument is put to sleep */
	for (;;) {
		i1pro3_eve ecode;

		rv = i1pro3_waitfor_event_th(p, &ecode, SW_THREAD_TIMEOUT);
		a1logd(p->log,8,"Event handler triggered with rv %d, th_term %d\n",rv,m->th_term);
		if (m->th_term) {
			m->th_termed = 1;
			break;
		}
		if (rv == I1PRO3_INT_BUTTONTIMEOUT) {
			nfailed = 0;
			continue;
		}
		if (rv != I1PRO3_OK) {
			nfailed++;
			a1logd(p->log,3,"Event thread failed with 0x%x\n",rv);
			continue;
		}
		if (ecode == i1pro3_eve_switch_press) {
			m->switch_count++;
			if (!m->hide_event && p->eventcallback != NULL) {
				p->eventcallback(p->event_cntx, inst_event_switch);
			}
		} else if (ecode == i1pro3_eve_adapt_change) {
			if (p->eventcallback != NULL) {
				p->eventcallback(p->event_cntx, inst_event_mconf);
			}
		}
	}
	a1logd(p->log,3,"Switch thread returning\n");
	return rv;
}

/* ============================================================ */
/* Low level i1pro3 commands */

/* USB Instrument commands */

/* Get the firmware version number and/or string */
i1pro3_code
i1pro3_fwver(
	i1pro3 *p,
	int *no,			/* If !NULL, return version * 100 */
	char str[50]		/* If !NULL, return version string */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char pbuf[57] = { 0 };			/* Recieve buffer */
	int slen = 56;			/* Buffer length */
	int rlen = 0;			/* Receved message length */
	int se, rv = I1PRO3_OK;
	int stime = 0;
	int majv, minv, strl;

	a1logd(p->log,2,"\ni1pro3_fwver: @ %d msec\n",(stime = msec_time()) - m->msec);


	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_get_firm_ver, 0, 0, pbuf, slen, &rlen, 2.0);
	amutex_unlock(m->lock);

	/* We expect a short read error */
	if ((se & ICOM_SHORT) && rlen >= 21)
		se &= ~ICOM_SHORT;

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_getfwrev: failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
		return rv;
	}

	majv = pbuf[0];
	minv = pbuf[1];
	strl = read_ORD32_le(&pbuf[2]);

	if (strl >= 50)
		strl = 49;
	if (strl >= rlen - 6)
		strl = (rlen -6);

	if (no != NULL) {
		*no = majv * 100 + minv;
	}

	if (str != NULL) {
		strncpy(str, (char *)&pbuf[6], strl);
		str[strl] = '\000';  
	}

	a1logd(p->log,2, "i1pro3_fwver: FW Ver. = %d.%d str = '%s', ICOM err 0x%x (%d msec)\n",
	                                               majv, minv, &pbuf[6], se, msec_time()-stime);

	return rv;
}

/* Get hw parameters */
/* Return pointers may be NULL if not needed. */
i1pro3_code
i1pro3_getparams(
	i1pro3 *p,
	unsigned int *minintclks,	/* Sub clock divider ratio ??? Or min_int_time ? */
	unsigned int *eesize,		/* EE size, but not used/wrong ?? */
	double *intclkp				/* Integration clock period ??? */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char pbuf[12];	/* reply bytes read */
	unsigned int _minintclks;
	unsigned int _eesize;
	double _intclkp;
	int se, rv = I1PRO3_OK;
	int stime = 0;

	a1logd(p->log,2,"\ni1pro3_getparams: @ %d msec\n", (stime = msec_time()) - m->msec);


	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_getparams, 0, 0, pbuf, 12, NULL, 2.0);
	amutex_unlock(m->lock);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_getparams: failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
		return rv;
	}

	_minintclks = read_ORD32_le(&pbuf[0]);
	_eesize    = read_ORD32_le(&pbuf[4]);		/* Doesn't seem to be valid */
	_intclkp   = read_FLT32_le(&pbuf[8]);

	a1logd(p->log,2,"i1pro3_getparams: returning %u, %u, %.17g ICOM err 0x%x (%d msec)\n",
	                   _minintclks, _eesize, _intclkp, se, msec_time()-stime);

	if (minintclks != NULL) *minintclks = _minintclks;		/* 165 */
	if (eesize != NULL) *eesize = _eesize;
	if (intclkp != NULL) *intclkp = _intclkp;			/* 1.5277777492883615e-5 */

	return rv;
}

/* Read from the EEProm */
i1pro3_code
i1pro3_readEEProm(
	i1pro3 *p,
	unsigned char *buf,		/* Where to read it to */
	int addr,				/* Address in EEprom to read from */
	int size				/* Number of bytes to read (max 65535) */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int rwbytes;			/* Data bytes read or written */
	unsigned char pbuf[8];	/* Write EEprom parameters */
	int len = 8;			/* Message length */
	int se = 0, rv = I1PRO3_OK;
	int stime;

	if (size >= 0x10000)
		return I1PRO3_INT_EETOOBIG;

	a1logd(p->log,2,"\ni1pro3_readEEProm: address 0x%x size 0x%x @ %d msec\n",
	                           addr, size, (stime = msec_time()) - m->msec);


	write_INR32_le(&pbuf[0], addr);
	write_INR16_le(&pbuf[4], size);
	pbuf[6] = pbuf[7] = 0;		/* Ignored */

	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_read_EE, 0, 0, pbuf, len, NULL, 2.0);
	amutex_unlock(m->lock);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_readEEProm: read failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
		return rv;
	}

	/* Now read the bytes */
	se = p->icom->usb_read(p->icom, NULL, 0x81, buf, size, &rwbytes, 5.0);
	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_readEEProm: read failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
		return rv;
	}

	if (rwbytes != size) {
		a1logd(p->log,1,"i1pro3_readEEProm: 0x%x bytes, short read error\n",rwbytes);
		return I1PRO3_HW_EE_SHORTREAD;
	}



	if (p->log->debug >= 6) {
		a1logd(p->log,6,"i1pro3_readEEProm: EE data:\n");
		adump_bytes(p->log,"    ",buf, 0, size);
	}

	a1logd(p->log,2,"i1pro3_readEEProm: 0x%x bytes, ICOM err 0x%x (%d msec)\n",
	                                          rwbytes, se, msec_time()-stime);

	return rv;
}


/* Get the Chip ID */
/* (It returns all zero's unless you've read the EEProm first ?) */
i1pro3_code
i1pro3_getchipid(
	i1pro3 *p,
	unsigned char chipid[8]
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int se, rv = I1PRO3_OK;

	a1logd(p->log,2,"\ni1pro3_getchipid: called\n");


	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_get_chipid, 0, 0, chipid, 8, NULL, 2.0);
	amutex_unlock(m->lock);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_getchipid: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	a1logd(p->log,2,"i1pro3_getchipid: returning %02X-%02X%02X%02X%02X%02X%02X%02X ICOM err 0x%x\n",
                           chipid[0], chipid[1], chipid[2], chipid[3],
                           chipid[4], chipid[5], chipid[6], chipid[7], se);
	return rv;
}

/* Set the measurement illumination LEDs currents. */
/* All values are 0..255 */
/* Assume these are: Green wl, white 1, white 2, blue, UV in some order... */
i1pro3_code
i1pro3_setledcurrents(
	i1pro3 *p,
	int led0c,	/* */
	int led1c,	/* */
	int led2c,	/* */
	int led3c,	/* */
	int led4c	/* */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char pbuf[5];
	int se, rv = I1PRO3_OK;
	int stime = 0;

	a1logd(p->log,2,"\ni1pro3_setledcurrents: %d, %d, %d, %d, %d  @ %d msec\n",
	                   led0c, led1c, led2c, led3c, led4c,
	                   (stime = msec_time()) - m->msec);



	write_ORD8(&pbuf[0], led0c);
	write_ORD8(&pbuf[1], led1c);
	write_ORD8(&pbuf[2], led2c);
	write_ORD8(&pbuf[3], led3c);
	write_ORD8(&pbuf[4], led4c);

	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_set_led_currents, 0, 0, pbuf, 5, NULL, 2.0);
	amutex_unlock(m->lock);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_setledcurrents: failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
		return rv;
	}

	a1logd(p->log,2,"i1pro3_setledcurrents: returning ICOM err 0x%x (%d msec)\n",
	                                                     se,msec_time()-stime);
	return rv;
}

/* Get Adapter Type */
i1pro3_code
i1pro3_getadaptype(
  i1pro3 *p,
    i1p3_adapter *atype
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char pbuf[1];
	int se, rv = I1PRO3_OK;
	int _atype;

	a1logd(p->log,2,"\ni1pro3_getadaptype: called\n");

	if ((m->capabilities & I1PRO3_CAP_HEAD_SENS) == 0) {
		a1logd(p->log,2,"i1pro3_getadaptype: not supported by instrument\n");
		return I1PRO3_OK;
	}


	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_get_adap_type, 0, 0, pbuf, 1, NULL, 2.0);
	amutex_unlock(m->lock);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_getadaptype: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	_atype = read_ORD8(pbuf);


	a1logd(p->log,2,"i1pro3_getadaptype: returning %d ICOM err 0x%x\n", _atype, se);

	if (atype != NULL)
		*atype = (i1p3_adapter)_atype;

	return rv;
}

/* Get the board temperature */
/* Return pointers may be NULL if not needed. */
i1pro3_code
i1pro3_getboardtemp(
	i1pro3 *p,
	double *btemp		/* Return temperature in degrees C */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char pbuf[4];	/* reply bytes read */
	double _btemp;
	int se, rv = I1PRO3_OK;
	int stime = 0;

	a1logd(p->log,2,"\ni1pro3_geboardtemp: @ %d msec\n", (stime = msec_time()) - m->msec);


	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_get_board_temp, 0, 0, pbuf, 4, NULL, 2.0);
	amutex_unlock(m->lock);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_geboardtemp: failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
		return rv;
	}

	_btemp = read_FLT32_le(&pbuf[0]);

	a1logd(p->log,2,"i1pro3_geboardtemp: returning %g ICOM err 0x%x (%d msec)\n",
	                   _btemp, se, msec_time()-stime);

	if (btemp != NULL) *btemp = _btemp;

	return rv;
}


/* Set the scan start indicator parameters */
/* This works with any measurement type (emis/refl, timed or manual length) */
// Default is 0,0 or 1,1.
i1pro3_code
i1pro3_setscanstartind(
	i1pro3 *p,
	int starttime,	/* 2 x min_int_time units after measure command to turn Green indicator on */
	int endtime		/* 2 x min_int_time units after measure command to turn Green indicator off */
					/* endtime must be > starttime or command is ignored */
					/* endtime == 255 keeps green on until measure ends */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char pbuf[2];
	int se, rv = I1PRO3_OK;
	int stime = 0;

	a1logd(p->log,2,"\ni1pro3_setscanparams: %d, %d @ %d msec\n",
	                   starttime, endtime, (stime = msec_time()) - m->msec);

	if ((m->capabilities & I1PRO3_CAP_IND_LED) == 0) {
		a1logd(p->log,2,"i1pro3_setscanparams: not supported by instrument\n");
		return inst_ok;
	}


	write_ORD8(&pbuf[0], starttime);
	write_ORD8(&pbuf[1], endtime);

	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_set_scan_ind, 0, 0, pbuf, 2, NULL, 2.0);
	amutex_unlock(m->lock);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_setscanparams: failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
		return rv;
	}

	a1logd(p->log,2,"i1pro3_setscanparams: returning ICOM err 0x%x (%d msec)\n",
	                                                     se,msec_time()-stime);
	return rv;
}


/* Set the tint multiplier */
/*
	For the emissive command:

	This multiplies the reflective integration time if >= 2.
	The value is rounded down to the nearest even value internally. 
	If == 2 then every odd sample carries the value,
	if == 4 every fourth sample carries the value, etc.
	Not very useful ? (change int time instead).

	For the  reflective command:

	This multiplies the reflective integration time if >= 2.
	The value is rounded down to the nearest even value internally. 
	If == 2 the even sample is zero and the x2 int value is delivered in
	the odd side of each sensor data. If == 4, the x4 integration sample
	is delivered in the odd side of every second sensor data, etc. 
	Doesn't affect the UV Led muxing.
	Maybe useful with polarization filter to improve S/N ratio ?
*/
i1pro3_code
i1pro3_settintmult(
	i1pro3 *p,
	int tintm	/* */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char pbuf[1];
	int se, rv = I1PRO3_OK;
	int stime = 0;
	unsigned int irrc = 0;

	a1logd(p->log,2,"\ni1pro3_settintmult: %d @ %d msec\n",
	                   tintm, (stime = msec_time()) - m->msec);


	write_ORD8(&pbuf[0], tintm);

	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_set_tint_mul, 0, 0, pbuf, 1, NULL, 2.0);
	amutex_unlock(m->lock);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_settintmult: failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
		return rv;
	}

	/* Get last error code. What to do with it ?? */
	i1pro3_getlasterr(p, &irrc);

	a1logd(p->log,2,"i1pro3_settintmult: irrc 0x%x returning ICOM err 0x%x (%d msec)\n",
	                                                     irrc, se,msec_time()-stime);
	return rv;
}

/* Delayed trigger implementation, called from thread below */
/* We assume that the measurement parameters have been set in */
/* the i1pro3imp structure c_* values */
static int
i1pro3_delayed_trigger(
	void *pp
) {
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char pbuf[12];	/* 8 or 12 bytes to write */
	int se, rv = I1PRO3_OK;
	int stime;


	if (m->c_refinst) {
		write_ORD32_le(pbuf + 0, m->c_nummeas); 
		write_ORD32_le(pbuf + 4, m->c_measflags); 
	} else {
		write_ORD32_le(pbuf + 0, m->c_nummeas); 
		write_ORD32_le(pbuf + 4, m->c_intclocks); 
		write_ORD32_le(pbuf + 8, m->c_measflags); 
	}

#ifdef USE_RD_SYNC
	a1logd(p->log,7,"\ni1pro3_delayed_trigger: waiting for meas. sync 0x%x\n",&m->rd_sync);
	p->icom->usb_wait_io(p->icom, &m->rd_sync);		/* Wait for meas or zebra read to start */
	a1logd(p->log,7,"i1pro3_delayed_trigger: got meas. sync\n");
#else
	/* Delay the trigger */
	a1logd(p->log,2,"\ni1pro3_delayed_trigger: start sleep @ %d msec\n",
	                                                 msec_time() - m->msec);
	msec_sleep(m->trig_delay);
#endif

	m->tr_t1 = msec_time();		/* Diagnostic */


	a1logd(p->log,2,"i1pro3_delayed_trigger: trigger @ %d msec\n",
	                                     (stime = msec_time()) - m->msec);
	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	m->trigstamp = usec_time();
	if (m->c_refinst) {
		se = p->icom->usb_control(p->icom,
			               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
		                   i1p3cc_meas_refl, 0, 0, pbuf, 8, NULL, 2.0);
	} else {
		se = p->icom->usb_control(p->icom,
			               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
		                   i1p3cc_meas_emis, 0, 0, pbuf, 12, NULL, 2.0);
	}
	amutex_unlock(m->lock);

	m->tr_t2 = msec_time();		/* Diagnostic */

	m->trig_se = se;
	m->trig_rv = icoms2i1pro3_err(se);

	a1logd(p->log,2,"i1pro3_delayed_trigger: done ICOM err 0x%x (%d msec)\n",
	                                                  se,msec_time()-stime);
	return 0;
}

/* Trigger either an "emissive" or "reflective" measurement after the delay in msec. */
/* The actual return code will be in m->trig_rv after the delay */
/* This allows us to start the measurement read before the trigger, */
/* ensuring that process scheduling latency can't cause the read to fail. */
/* An "emissive" measurement returns 268 bytes per measurement, composed of */
/* 128 live values and 6 dummy/covered values. */
/* A "reflective" measurement returns 564 bytes per measurement, composed */
/* of two 268 sub-measurements + illuminent LED tracking information. */
/* These two measurements are for two different illuminants, thereby multiplexing */
/* the with/without UV measurement values. */
i1pro3_code
i1pro3_trigger_measure(
	i1pro3 *p,
	int refinst,		/* NZ to trigger a "reflective" measurement */
	int zebra,			/* NZ if zebra ruler read is being used as well */
	int nummeas,		/* Number of measurements to make, 0 for infinite ? */
	int intclocks,		/* Number of integration clocks (ignored but used if refinst) */
	int flags,			/* Measurement mode flags. Differs with refinst */
	int delay			/* Delay before triggering command in msec */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int rv = I1PRO3_OK;

	a1logd(p->log,2,"\ni1pro3_trigger_measure: triggering %s measurement with nummeas %d intclocks %d flags 0x%x after %dmsec "
	   "delay @ %d msec\n", refinst ? "reflective" : "emissive", nummeas, intclocks, flags, delay, msec_time() - m->msec);

	/* NOTE := would be better here to create a reusable thread once and retrigger ! */
	if (m->trig_thread != NULL)
		m->trig_thread->del(m->trig_thread);

	m->c_refinst = refinst;
	m->c_nummeas = nummeas;
	m->c_intclocks = intclocks;
	m->c_measflags = flags;


    m->tr_t1 = m->tr_t2 = m->tr_t3 = m->tr_t4 = m->tr_t5 = m->tr_t6 = m->tr_t7 = 0;
	m->trig_delay = delay;

	if ((m->trig_thread = new_athread(i1pro3_delayed_trigger, (void *)p)) == NULL) {
		a1logd(p->log,1,"i1pro3_trigger_measure: creating delayed trigger Rev E thread failed\n");
		return I1PRO3_INT_THREADFAILED;
	}

#ifdef WAIT_FOR_DELAY_TRIGGER	/* hack to diagnose threading problems */
	while (m->tr_t2 == 0) {
		Sleep(1);
	}
#endif
	a1logd(p->log,2,"i1pro3_trigger_measure: scheduled triggering OK\n");

	return rv;
}

/* Gather a measurements results. */
/* A buffer full of bytes is returned. */
/* It appears that the read can be pending before triggering though. */
/* Scan reads will also terminate if there is too great a delay beteween each read ? */
static i1pro3_code
i1pro3_gathermeasurement(
	i1pro3 *p,
	int refinst,		/* NZ if we are gathering a measure reflective else emissive */
	int zebra,			/* NZ if zebra ruler read is being used as well */
	int scanflag,		/* NZ if in scan mode to continue reading */
	int xmeas,			/* Expected number of measurements, ignored if scanflag */
	unsigned char *buf,	/* Where to read it to */
	int bsize,			/* Bytes available in buffer */
	int *nummeas		/* Return number of readings measured */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char *ibuf = buf;	/* Incoming buffer */
	int nsen = refinst ? m->nsen2 : m->nsen1;		/* Number of sensor 16 bit words */
	double top, extra;			/* Time out period */
	int ixsize;					/* Total expected bytes to read */
	int xsize;					/* Remaining expected bytes to read */
	int rwbytes;				/* Data bytes read or written */
	int ameas = 0;				/* actual measurements */
	int asize = 0;				/* actual size measured */
	int stime = 0;
	int se, rv = I1PRO3_OK;
	unsigned int irrc = 0;

	if ((bsize % (2 * nsen)) != 0) {
		a1logd(p->log,1,"i1pro3_gathermeasurement: buffer was not a multiple of sens size\n");
		return I1PRO3_INT_ODDREADBUF;
	}

	a1logd(p->log,2,"\ni1pro3_gathermeasurement: xmeas %d, refinst %d, scanflag %d, address %p bsize 0x%x "
	          "@ %d msec\n",xmeas, refinst, scanflag, buf, bsize, (stime = msec_time()) - m->msec);

	extra = 2.0;		/* Extra timeout margin */

	if (scanflag == 0) {
		ixsize = xmeas * 2 * nsen; 
	} else {
		ixsize = bsize;
		xmeas = bsize / (2 * nsen);
	}


	for (xsize = ixsize; xsize > 0;) {
		int size;		/* number of bytes to attempt to read */

		size = xsize;

		if (size > 0x10000)		/* Read max at once */
			size = 0x10000;

		if (size > bsize) {		/* oops, no room for read */
			unsigned char tbuf[MX_NSEN * 2];

			/* One sample at a time.. */
			top = extra + (m->c_refinst ? 2.0 : 1.0) * m->c_intclocks * m->intclkp;

			/* We need to clean up, so soak up all the data and throw it away */
			while ((se = p->icom->usb_read(p->icom, NULL, 0x81, tbuf, 2 * nsen, &rwbytes, top)) == ICOM_OK)
				;
			a1logd(p->log,1,"i1pro3_gathermeasurement: buffer was too short for scan\n");
			return I1PRO3_INT_MEASBUFFTOOSMALL;
		}

		m->tr_t6 = msec_time();	/* Diagnostic, start of subsequent reads */
		if (m->tr_t3 == 0) m->tr_t3 = m->tr_t6;		/* Diagnostic, start of first read */

		top = extra + size/(2.0 * nsen) * (m->c_refinst ? 2.0 : 1.0) * m->c_intclocks * m->intclkp;
		a1logd(p->log,7,"i1pro3_gathermeasurement: size %d timeout set to %f secs\n",size,top);

		/* Tell trigger command or zebra read when we have started.. */
		se = p->icom->usb_read(p->icom,
		      xsize == ixsize ? (zebra ? &m->rd_sync2 : &m->rd_sync) : NULL,
		                                          0x81, buf, size, &rwbytes, top);

		m->tr_t5 = m->tr_t7;
		m->tr_t7 = msec_time();	/* Diagnostic, end of subsequent reads */
		if (m->tr_t4 == 0) {
			m->tr_t5 = m->tr_t2;
			m->tr_t4 = m->tr_t7;	/* Diagnostic, end of first read */
		}

		a1logd(p->log,7,"i1pro3_gathermeasurement: returned @ %d msec\n",msec_time());

		if (se == ICOM_SHORT) {		/* Expect this to terminate scan reading */
			a1logd(p->log,2,"i1pro3_gathermeasurement: short read, read %d bytes, asked for %d\n",
			                                                                     rwbytes,size);
			a1logd(p->log,2,"i1pro3_gathermeasurement: trig & rd times %d %d %d %d)\n",
			   m->tr_t2-m->tr_t1, m->tr_t3-m->tr_t2, m->tr_t4-m->tr_t3, m->tr_t6-m->tr_t5);

		} else if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
			if (m->trig_rv != I1PRO3_OK) {
				a1logd(p->log,1,"i1pro3_gathermeasurement: trigger failed, ICOM err 0x%x\n",
				                                                             m->trig_se);
				return m->trig_rv;
			}
			if (se & ICOM_TO)
				a1logd(p->log,1,"i1pro3_gathermeasurement: timed out with top = %f\n",top);
			a1logd(p->log,1,"i1pro3_gathermeasurement: failed, bytes read 0x%x, ICOM err 0x%x\n",
			                                                                     rwbytes, se);
			return rv;
		}
 
		/* Track where we're up to */
		buf   += rwbytes;
		bsize -= rwbytes;
		xsize -= rwbytes;
		asize += rwbytes;

		/* Either we're scanning and expect to get a short read at the end of the scan, */
		/* or something went wrong with a non-scan measurement. */
		if (rwbytes != size) {
			break;
		}
	}

	/* Get last error code. What to do with it ?? */
	i1pro3_getlasterr(p, &irrc);

	/* Not scanning, so expect to read exactly what we asked for */
	if (scanflag == 0) {
		if (asize != ixsize) {
			a1logd(p->log,1,"i1pro3_gathermeasurement: unexpected length read, got %d expected %d\n"
			                                                                     ,asize,ixsize);
			return I1PRO3_HW_ME_SHORTREAD;
		}

	/* Scanning, but expect bytes to be a multiple of measurement */
	} else {

		if ((asize % (2 * nsen)) != 0) {
			a1logd(p->log,1,"i1pro3_gathermeasurement: unexpected length read, got %d expected %d\n"
			                                                      ,asize,(asize/(2 * nsen) + 1) * 2 * nsen);
			return I1PRO3_HW_ME_SHORTREAD;
		}
	}

	ameas = asize / (2 * nsen);


	if (p->log->debug >= 6) {
		a1logd(p->log,6,"i1pro3_gathermeasurement: measurement data:\n");
		adump_bytes(p->log,"    ",ibuf, 0, asize);
	}

	a1logd(p->log,2,"i1pro3_gathermeasurement: read %d readings %d bytes, irrc 0x%x ICOM err 0x%x (%d msec)\n",
	                                                   ameas, asize, irrc, se, msec_time()-stime);
	a1logd(p->log,2,"i1pro3_gathermeasurement: (trig & rd times %d %d %d %d)\n",
	    m->tr_t2-m->tr_t1, m->tr_t3-m->tr_t2, m->tr_t4-m->tr_t3, m->tr_t6-m->tr_t5);

	if (nummeas != NULL) *nummeas = ameas;

	return rv;
}

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
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char *ibuf = buf;	/* Incoming buffer */
	double top, extra;			/* Time out period */
	int ixsize;					/* Total expected bytes to read */
	int xsize;					/* Remaining expected bytes to read */
	int rwbytes;				/* Data bytes read or written */
	int ameas = 0;				/* actual measurements */
	int asize = 0;				/* actual size measured */
	int stime = 0;
	int se, rv = I1PRO3_OK;

	a1logd(p->log,2,"\ni1pro3_gatherzebra: bsize 0x%x "
	          "@ %d msec\n",bsize, (stime = msec_time()) - m->msec);

	extra = 2.0;		/* Extra timeout margin */


	ixsize = bsize;	

	for (xsize = bsize; xsize > 0;) {
		int size;		/* number of bytes to attempt to read */

		size = xsize;

		if (size > 65536)		/* Read max at once */
			size = 65536;

		if (size > bsize) {		/* oops, no room for read */
			unsigned char tbuf[1024];

			/* 1024 bytes at a time.. */
			top = extra + m->intclkp * 4 * 1024;

			/* We need to clean up, so soak up all the data and throw it away */
			while ((se = p->icom->usb_read(p->icom, NULL, 0x84, tbuf, 1024, &rwbytes, top)) == ICOM_OK)
				;
			a1logd(p->log,1,"i1pro3_gatherzebra: buffer was too short for scan\n");
			return I1PRO3_INT_MEASBUFFTOOSMALL;
		}

		top = extra + m->intclkp * 4 * size;
		a1logd(p->log,7,"i1pro3_gatherzebra: size %d timeout set to %f secs\n",size,top);

		/* Tell trigger command when we have started.. */
		se = p->icom->usb_read(p->icom, xsize == ixsize ? &m->rd_sync : NULL, 
		                       0x84, buf, size, &rwbytes, top);

		if (se == ICOM_SHORT) {		/* Expect this to terminate scan reading */
			a1logd(p->log,2,"i1pro3_gatherzebra: short read, read %d bytes, asked for %d\n",
			                                                                     rwbytes,size);
		} else if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
			if (se & ICOM_TO)
				a1logd(p->log,1,"i1pro3_gatherzebra: timed out with top = %f\n",top);
			a1logd(p->log,1,"i1pro3_gatherzebra: failed, bytes read 0x%x, ICOM err 0x%x\n",
			                                                                     rwbytes, se);
			return rv;
		}
 
		/* Track where we're up to */
		buf   += rwbytes;
		bsize -= rwbytes;
		xsize -= rwbytes;
		asize += rwbytes;

		/* We expect to get a short read at the end of the scan */
		if (rwbytes != size) {
			break;
		}
	}
	if (p->log->debug >= 6) {
		a1logd(p->log,6,"i1pro3_gatherzebra: zebra data:\n");
		adump_bytes(p->log,"    ",ibuf, 0, asize);
	}

	a1logd(p->log,2,"i1pro3_gatherzebra: read %d bytes ICOM err 0x%x (%d msec)\n",
	                                                   asize, se, msec_time()-stime);

	if (bread != NULL) *bread = asize;

	return rv;
}

/* Delayed simulated event implementation, called from thread below */
static int
i1pro3_delayed_simulate_event(
	void *pp
) {
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char buf[1];	/* 1 byte to write */
	int se, rv = I1PRO3_OK;

	a1logd(p->log,2,"\ni1pro3_delayed_simulate_event: 0x%x, delay %d msec\n",
	                                           m->seve_code, m->seve_delay);

	msec_sleep(m->seve_delay);

	write_ORD8(&buf[0], m->seve_code);

	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_sim_event, 0, 0, buf, 1, NULL, 2.0);
	amutex_unlock(m->lock);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK)
		a1logd(p->log,1,"i1pro3_delayed_simulate_event: event 0x%x failed with ICOM err 0x%x\n",
		                                                                        m->seve_code,se);
	else 
		a1logd(p->log,2,"i1pro3_delayed_simulate_event: 0x%x done, ICOM err 0x%x\n",m->seve_code,se);

	m->seve_se = se;
	m->seve_rv = icoms2i1pro3_err(se);

	return 0;
}

/* Simulating an event. If delay > 0, send simulated event after delay msec */
i1pro3_code i1pro3_simulate_event(i1pro3 *p, i1pro3_eve ecode, int delay) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char buf[1];	/* 1 byte to write */
	int se, rv = I1PRO3_OK;

	a1logd(p->log,2,"\ni1pro3_simulate_event: 0x%x\n",ecode);


	if (delay > 0) {
		m->seve_delay = delay;
		m->seve_code = ecode;
		m->seve_se = 0;
		m->seve_rv = 0;

		if ((m->seve_thread = new_athread(i1pro3_delayed_simulate_event, (void *)p)) == NULL) {
			a1logd(p->log,1,"i1pro3_simulate_event: creating delayed eevent  thread failed\n");
			return I1PRO3_INT_THREADFAILED;
		}

		return rv;
	}

	write_ORD8(&buf[0], ecode);

	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_sim_event, 0, 0, buf, 1, NULL, 2.0);
	amutex_unlock(m->lock);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK)
		a1logd(p->log,1,"i1pro3_simulate_event: event 0x%x failed with ICOM err 0x%x\n",ecode,se);
	else 
		a1logd(p->log,2,"i1pro3_simulate_event: 0x%x done, ICOM err 0x%x\n",ecode,se);

	return rv;
}


/* Wait for a reply triggered by an instrument event */
i1pro3_code i1pro3_waitfor_event(i1pro3 *p, i1pro3_eve *ecode, double top) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int rwbytes;			/* Data bytes read */
	unsigned char buf[1];	/* Result  */
	int se, rv = I1PRO3_OK;
	int stime = 0;
	i1pro3_eve _ecode;

	a1logd(p->log,2,"\ni1pro3_waitfor_event: read 1 byte from event hit port @ %d msec\n",
	                                                     (stime = msec_time()) - m->msec);


	/* Send this only once ? */
	if (!m->get_events_sent) {
		/* Maybe the parameter is an event mask (Typical value ix 0x7) ? */
		write_ORD8(&buf[0], m->ee_button_bytes);

		amutex_lock(m->lock);
		msec_sleep(1);		// ??
		se = p->icom->usb_control(p->icom,
			               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
		                   i1p3cc_get_events, 0, 0, buf, 1, NULL, 2.0);
		amutex_unlock(m->lock);

		if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
			a1logd(p->log,1,"i1pro3_waitfor_event_th: failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
			return rv;
		}

		m->get_events_sent = 1;
	}

	/* Now read bytes */
	se = p->icom->usb_read(p->icom, NULL, 0x83, buf, 1, &rwbytes, top);

	if (se & ICOM_TO) {
		a1logd(p->log,2,"i1pro3_waitfor_event: read 0x%x bytes, timed out (%d msec)\n",
		                   rwbytes,msec_time()-stime);
		return I1PRO3_INT_BUTTONTIMEOUT;
	}

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_waitfor_event: failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
		return rv;
	}

	if (rwbytes != 1) {
		a1logd(p->log,1,"i1pro3_waitfor_event: read 0x%x bytes, short read error (%d msec)\n",
		                                                          rwbytes,msec_time()-stime);
		return I1PRO3_HW_SW_SHORTREAD;
	}

	_ecode  = (i1pro3_eve) read_ORD8(&buf[0]);
	
	if (p->log->debug >= 2) {
		char sbuf[100];
		if (_ecode == i1pro3_eve_none)
			strcpy(sbuf, "None");
		else if (_ecode == i1pro3_eve_switch_press)
			strcpy(sbuf, "Button press");
		else if (_ecode == i1pro3_eve_switch_release)
			strcpy(sbuf, "Button release");
		else if (_ecode == i1pro3_eve_adapt_change)
			strcpy(sbuf, "Adaptor change");
		else
			sprintf(sbuf,"Unknown 0x%x",_ecode);

		a1logd(p->log,2,"i1pro3_waitfor_event: Event %s @ %d msec ICOM err 0x%x\n",
		                                 sbuf, (stime = msec_time()) - m->msec, se);
	}

	return rv; 
}

/* Wait for a reply triggered by an instrument event (thread version) */
/* Returns I1PRO3_OK if the switch has been pressed or some other event such */
/* as an adapter type change, or I1PRO3_INT_BUTTONTIMEOUT if */
/* no event has occured before the time expired, */
/* or some other error. */
i1pro3_code i1pro3_waitfor_event_th(i1pro3 *p, i1pro3_eve *ecode, double top) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int rwbytes;			/* Data bytes read */
	unsigned char buf[1];	/* Result  */
	int se, rv = I1PRO3_OK;
	int stime = 0;
	i1pro3_eve _ecode;

	a1logd(p->log,2,"\ni1pro3_waitfor_event_th: read 1 byte from event hit port @ %d msec\n",
	                                                     (stime = msec_time()) - m->msec);

	/* Send this only once ? */
	if (!m->get_events_sent) {
		/* Maybe the parameter is an event mask (Typical value ix 0x7) ? */
		write_ORD8(&buf[0], m->ee_button_bytes);

		amutex_lock(m->lock);
		msec_sleep(1);		// ??
		se = p->icom->usb_control(p->icom,
			               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
		                   i1p3cc_get_events, 0, 0, buf, 1, NULL, 2.0);
		amutex_unlock(m->lock);

		if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
			a1logd(p->log,1,"i1pro3_waitfor_event_th_th: failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
			return rv;
		}

		m->get_events_sent = 1;
	}

	/* Now read bytes */
	se = p->icom->usb_read(p->icom, &m->sw_cancel, 0x83, buf, 1, &rwbytes, top);

	if (se & ICOM_TO) {
		a1logd(p->log,2,"i1pro3_waitfor_event_th: read 0x%x bytes, timed out (%d msec)\n",
		                   rwbytes,msec_time()-stime);
		return I1PRO3_INT_BUTTONTIMEOUT;
	}

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_waitfor_event_th: failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
		return rv;
	}

	if (rwbytes != 1) {
		a1logd(p->log,1,"i1pro3_waitfor_event_th: read 0x%x bytes, short read error (%d msec)\n",
		                                                          rwbytes,msec_time()-stime);
		return I1PRO3_HW_SW_SHORTREAD;
	}

	_ecode = (i1pro3_eve) read_ORD8(&buf[0]);
	
	if (p->log->debug >= 2) {
		char sbuf[100];
		if (_ecode == i1pro3_eve_none)
			strcpy(sbuf, "None");
		else if (_ecode == i1pro3_eve_switch_press)
			strcpy(sbuf, "Button press");
		else if (_ecode == i1pro3_eve_switch_release)
			strcpy(sbuf, "Button release");
		else if (_ecode == i1pro3_eve_adapt_change)
			strcpy(sbuf, "Adaptor change");
		else
			sprintf(sbuf,"Unknown 0x%x",_ecode);

		a1logd(p->log,2,"i1pro3_waitfor_event_th: Event %s @ %d msec ICOM err 0x%x\n",
		                                 sbuf, (stime = msec_time()) - m->msec, se);
	}

	if (ecode != NULL)
		*ecode = _ecode;

	return rv; 
}

/* Terminate event handling by cancelling the thread i/o */
i1pro3_code
i1pro3_terminate_event(
	i1pro3 *p
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char pbuf[8];	/* 8 bytes to write */
	int se, rv = I1PRO3_OK;

	a1logd(p->log,2,"i1pro3_terminate_event: called\n");

	if (m->th_termed == 0) {
		a1logd(p->log,3,"i1pro3 terminate event thread failed, canceling I/O\n");
		p->icom->usb_cancel_io(p->icom, &m->sw_cancel);
	}
	
	return rv;
}

/* Get last error code */
/* Return pointers may be NULL if not needed. */
i1pro3_code
i1pro3_getlasterr(
	i1pro3 *p,
	unsigned int *errc
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned char pbuf[4];	/* reply bytes read */
	unsigned int _errc;
	int se, rv = I1PRO3_OK;
	int stime = 0;

	a1logd(p->log,2," i1pro3_getlasterr: @ %d msec\n", (stime = msec_time()) - m->msec);


	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_get_last_err, 0, 0, pbuf, 4, NULL, 2.0);
	amutex_unlock(m->lock);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1," i1p3cc_get_last_err: failed with ICOM err 0x%x (%d msec)\n",se, msec_time()-stime);
		return rv;
	}

	_errc = read_ORD32_le(&pbuf[0]);

	a1logd(p->log,2," i1p3cc_get_last_err: returning %u ICOM err 0x%x (%d msec)\n",
	                   _errc, se, msec_time()-stime);

	if (errc != NULL) *errc = _errc;

	return rv;
}

/* Send a raw indicator LED sequence.

	The instruction sends a 32 bit le length of the data to follow.

	The data is sent to e.p. 2.

	Usually groups of 3 bytes.
	If < 3 bytes, indicator is turned off ?

	First byte = color & pulse type

					Off:
	1xxx xxxx			??
	0xx0 00 xx		Off ??

					Red:
	0xxx 11x1			Blink once, ignore timing & repeats
	0xxx 1110			Pulse up & down using timing & repeats

					Green:
	0xxx 10x1			Blink once, ignore timing & repeats
	0xxx 1010			Pulse up & down using timing & repeats

					White:
	0xx1 00x1			Blink once, ignore timing & repeats
	0xx1 0010			Pulse up & down using timing & repeats

	Second byte = timing:

		Blink:
			ms 4 bits sets blink time:
				0 .. 8 = time, number-1 of 75 msec periods.
				>= 9 = infinity

		Pulse: ls 4 bits = cycle period divider out of 11 seconds
				ie. 1 = 11 s, 2 = 5.5 s, 5 = 2.2 s. 0xA = 1.1 s, 14 = 0.5 sec

	Last byte is count

		Blink:
			Ignored (always 1)

		Pulse:
			Number of pulses, 0 = infinity.

	Examples seen in traces:

		11 fa 00
		01 fa 04  12 05 00		White blink then white pulse ?
		0a 14 02  12 05 00		Two fast green pulse then white pulses
		06 05 01  11 fa 00

	Functions found:

		05 01 11 fa 00
		fa 00

		14 04 11 fa 00
		14 01 01 fa 19 12 05 00
		14 02 80 fa 01
		14 02 12 05 00
		14 01 06 06 14 02 12 05
		00 01
		14 01 10 14 01 80 fa
		fa 04 12 05 00

 */

static int
i1pro3_indLEDseq(void *pp, unsigned char *buf, int size) {
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3imp *m = (i1pro3imp *)p->m;
	int rwbytes;			/* Data bytes written */
	unsigned char pbuf[4];	/* Number of bytes being sent */
	int se, rv = I1PRO3_OK;

	write_INR32_le(pbuf, size); 

	a1logd(p->log,2,"\ni1pro3_indLEDseq: length %d bytes\n", size);

	if ((m->capabilities & I1PRO3_CAP_IND_LED) == 0) {
		a1logd(p->log,2,"i1pro3_indLEDseq: not supported by instrument\n");
		return inst_ok;
	}
	

	amutex_lock(m->lock);
	msec_sleep(1);		// ??
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   i1p3cc_set_ind, 0, 0, pbuf, 4, NULL, 2.0);
	amutex_unlock(m->lock);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_indLEDseq: failed with ICOM err 0x%x\n",rv);
		return rv;
	}

	a1logd(p->log,2,"i1pro3_geteesize: command got ICOM err 0x%x\n", se);

	/* Now write the bytes */
	se = p->icom->usb_write(p->icom, NULL, 0x02, buf, size, &rwbytes, 5.0);

	if ((rv = icoms2i1pro3_err(se)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_indLEDseq: data write failed with ICOM err 0x%x\n",se);
		return rv;
	}

	if (rwbytes != size) {
		a1logd(p->log,1,"i1pro3_indLEDseq: wrote 0x%x bytes, short write error\n",rwbytes);
		return I1PRO3_HW_LED_SHORTWRITE;
	}

	a1logd(p->log,2,"i1pro3_indLEDseq: wrote 0x%x bytes LED sequence, ICOM err 0x%x\n", size, rv);

	return rv;
}

/* Turn indicator LEDs off */
static int
i1pro3_indLEDoff(void *pp) {
	i1pro3 *p = (i1pro3 *)pp;
	int rv = I1PRO3_OK;
	unsigned char seq[] = { 0x00 };

	a1logd(p->log,2,"i1pro3_indLEDoff: called\n");
	rv = i1pro3_indLEDseq(p, seq, sizeof(seq));
	a1logd(p->log,2,"i1pro3_indLEDoff: returning ICOM err 0x%x\n",rv);

	return rv;
}

/* ============================================================ */
/* Parse the EEProm contents */

/* Initialise the calibration from the EEProm contents. */
i1pro3_code i1pro3_parse_eeprom(i1pro3 *p, unsigned char *buf, unsigned int len) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1data3 *d;
	int rv = I1PRO3_OK;
	ORD32 crc;
	int boff;						/* Block start offset */
	int i, j;

	a1logd(p->log,2,"i1pro3_parse_eeprom: called with %d bytes\n",len);

	/* Create class to handle EEProm parsing */
	if ((d = new_i1data3(p, buf, len)) == NULL)
		{ d->del(d); return I1PRO3_INT_CREATE_EEPROM_STORE; }

	/* - - - - - - - - - - - - - - - - - - */
	/* Parse block 0, the base information */
	boff = 0;
	d->init_crc(d);

	if (d->get_32_ints(d, &m->ee_crc_0, boff + 0x0, 1, 0) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }

	if (d->get_32_ints(d, &m->ee_version, boff + 0x4, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	a1logd(p->log,2,"EE version = %u\n",m->ee_version);

	if (m->ee_version == 0)
		warning("Operation of i1Pro3 Rev 0 will not be accurate");
	if (m->ee_version > 1)
		warning("Operation of i1Pro3 Rev %d has not be verified");
	/* Hmm. we could do this.. */
//	{ d->del(d); return  I1PRO3_HW_EE_VERSION; }	

	if (d->get_32_ints(d, &m->ee_unk01, boff + 0x8, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	/* Block offsets for the remaining blocks */

	if (d->get_16_ints(d, &m->ee_bk1_off, boff + 0xc, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, &m->ee_bk2_off, boff + 0xe, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, &m->ee_bk3_off, boff + 0x10, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, &m->ee_bk4_off, boff + 0x12, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, &m->ee_bk5_off, boff + 0x14, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, &m->ee_bk6_off, boff + 0x16, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	

	/* Check the CRC */
	crc = d->get_crc(d);
	a1logd(p->log,3,"i1pro3_parse_eeprom: block 0 crc = 0x%x, should be 0x%x - %s\n",
	                             crc, m->ee_crc_0, crc == m->ee_crc_0 ? "OK": "BAD");
	if (crc != m->ee_crc_0)
		{ d->del(d); return I1PRO3_HW_EE_CHKSUM; }

#ifdef NEVER		/* We're not using or maintaining the EEProm last cal. log */
					/* (We also don't compute the checksum if we do) */ 
# pragma message("######### i1pro3_imp.c reading EEProm last cal log ########")

	/* Block 1 */
	boff = m->ee_bk1_off;
	d->init_crc(d);

	if (d->get_32_ints(d, &m->ee_logA_crc, boff + 0x0, 1, 0) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logA_remspotcalc, boff + 0x4, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logA_scanmeasc, boff + 0x8, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_logA_lamptime, boff + 0xc, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logA_emspotcalc, boff + 0x10, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logA_emscancalc, boff + 0x14, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, m->ee_logA_darkreading, boff + 0x18, 128, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, m->ee_logA_whitereading, boff + 0x218, 128, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logA_caldate, boff + 0x418, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logA_calcount, boff + 0x41c, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logA_caldate2, boff + 0x420, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logA_calcount2, boff + 0x424, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (m->ee_version >= 1) {
		if (d->get_32_ints(d, m->ee1_logA_unk02, boff + 0x428, 128, 1) == NULL)
			{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	}
	crc = d->get_crc(d);
	a1logd(p->log,3,"i1pro3_parse_eeprom: block 1 crc = 0x%x, should be 0x%x - %s\n",
	                             crc, m->ee_logA_crc, crc == m->ee_logA_crc ? "OK": "BAD");

	/* Block 2 */
	boff = m->ee_bk2_off;
	d->init_crc(d);

	if (d->get_32_ints(d, &m->ee_logB_crc, boff + 0x0, 1, 0) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logB_remspotcalc, boff + 0x4, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_scanmeasc, boff + 0x8, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_logB_lamptime, boff + 0xc, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logB_emspotcalc, boff + 0x10, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logB_emscancalc, boff + 0x14, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, m->ee_logB_darkreading, boff + 0x18, 128, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, m->ee_logB_whitereading, boff + 0x218, 128, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logB_caldate, boff + 0x418, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logB_calcount, boff + 0x41c, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logB_caldate2, boff + 0x420, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_logB_calcount2, boff + 0x424, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (m->ee_version >= 1) {
		if (d->get_32_ints(d, m->ee1_logB_unk02, boff + 0x428, 128, 1) == NULL)
			{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	}

	crc = d->get_crc(d);
	a1logd(p->log,3,"i1pro3_parse_eeprom: block 2 crc = 0x%x, should be 0x%x - %s\n",
	                             crc, m->ee_logB_crc, crc == m->ee_logB_crc ? "OK": "BAD");
#endif /* NEVER */

	/* Block 3 */
	boff = m->ee_bk3_off;
	d->init_crc(d);

	if (d->get_32_ints(d, &m->ee_crc_3, boff + 0x0, 1, 0) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk03, boff + 0x4, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk04, boff + 0x8, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->rget_32_doubles(d, m->ee_lin, boff + 0xc, 4, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ee_white_ref, boff + 0x1c, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ee_uv_white_ref, boff + 0xac, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ee_emis_coef, boff + 0x13c, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ee_amb_coef, boff + 0x1cc, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk05, boff + 0x25c, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk06, boff + 0x260, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk07, boff + 0x264, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk08, boff + 0x268, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_wl_cal_min_level, boff + 0x26c, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk09, boff + 0x270, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk10, boff + 0x274, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk11, boff + 0x278, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk12, boff + 0x27c, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk13, boff + 0x280, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_wlcal_intt, boff + 0x284, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_sens_sat, boff + 0x288, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_serno, boff + 0x28c, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_dom1, boff + 0x290, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_dom2, boff + 0x294, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, m->ee_unk14, boff + 0x298, 5, 0) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk15, boff + 0x2ac, 1, 0) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_devtype, boff + 0x2b0, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk16, boff + 0x2b4, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_8_char(d, m->ee_chipid, boff + 0x2b8, 8, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_capabilities, boff + 0x2c0, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk17, boff + 0x2c4, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_bk_v_limit, boff + 0x2c8, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_bk_f_limit, boff + 0x2cc, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_unk18, boff + 0x2d0, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_unk19, boff + 0x2d4, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_unk20, boff + 0x2d8, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_unk21, boff + 0x2dc, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_unk22, boff + 0x2e0, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_unk23, boff + 0x2e4, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_unk24, boff + 0x2e8, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_unk25, boff + 0x2ec, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, m->ee_wlcal_spec, boff + 0x2f0, 128, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, m->ee_uv_wlcal_spec, boff + 0x3f0, 128, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_wlcal_max, boff + 0x4f0, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_fwhm, boff + 0x4f4, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_fwhm_tol, boff + 0x4f8, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_unk35, boff + 0x4fc, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, m->ee_straylight, boff + 0x500, 1296, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_straylight_scale, boff + 0xf20, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ee_wl_cal1, boff + 0xf24, 128, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ee_wl_cal2, boff + 0x1124, 128, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_unk26, boff + 0x1324, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, &m->ee_led_w_cur, boff + 0x1328, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, &m->ee_led_b_cur, boff + 0x132a, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, &m->ee_led_luv_cur, boff + 0x132c, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, &m->ee_led_suv_cur, boff + 0x132e, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, &m->ee_led_gwl_cur, boff + 0x1330, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_16_ints(d, &m->ee_button_bytes, boff + 0x1332, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	/* Load LED drift model directly into organized array */
	if (d->get_32_doubles(d, m->ledm_poly[1][2][0], boff + 0x1334, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[1][2][1], boff + 0x13c4, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[1][2][2], boff + 0x1454, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }

	vect_set(m->ledm_poly[1][2][3], 0.0, 36);
	
	if (d->get_32_doubles(d, m->ledm_poly[1][3][0], boff + 0x1478, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[1][3][1], boff + 0x1508, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[1][3][2], boff + 0x1598, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }

	vect_set(m->ledm_poly[1][3][3], 0.0, 36);
	
	if (d->get_32_doubles(d, m->ledm_poly[1][4][0], boff + 0x15bc, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[1][4][1], boff + 0x164c, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[1][4][2], boff + 0x16dc, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }

	vect_set(m->ledm_poly[1][4][3], 0.0, 36);
	
	if (d->get_32_doubles(d, m->ledm_poly[1][5][0], boff + 0x1700, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[1][5][1], boff + 0x1790, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[1][5][2], boff + 0x1820, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	vect_set(m->ledm_poly[1][5][3], 0.0, 36);

	if (d->get_32_doubles(d, m->ledm_poly[1][6][0], boff + 0x1844, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[1][6][1], boff + 0x18d4, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[1][6][2], boff + 0x1964, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }

	vect_set(m->ledm_poly[1][6][3], 0.0, 36);
	
	if (d->get_32_doubles(d, m->ledm_poly[1][7][0], boff + 0x1988, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[1][7][1], boff + 0x1a18, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[1][7][2], boff + 0x1aa8, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }

	vect_set(m->ledm_poly[1][7][3], 0.0, 36);
	
	if (d->get_32_doubles(d, m->ledm_poly[1][1][0], boff + 0x1acc, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[1][1][1], boff + 0x1b5c, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[1][1][2], boff + 0x1bec, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[1][1][3], boff + 0x1c7c, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[1][0][0], boff + 0x1ca0, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[1][0][1], boff + 0x1d30, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[1][0][2], boff + 0x1dc0, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[1][0][3], boff + 0x1e50, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[0][2][0], boff + 0x1e74, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[0][2][1], boff + 0x1f04, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[0][2][2], boff + 0x1f94, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }

	vect_set(m->ledm_poly[0][2][3], 0.0, 36);
	
	if (d->get_32_doubles(d, m->ledm_poly[0][3][0], boff + 0x1fb8, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[0][3][1], boff + 0x2048, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[0][3][2], boff + 0x20d8, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }

	vect_set(m->ledm_poly[0][3][3], 0.0, 36);
	
	if (d->get_32_doubles(d, m->ledm_poly[0][4][0], boff + 0x20fc, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[0][4][1], boff + 0x218c, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[0][4][2], boff + 0x221c, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }

	vect_set(m->ledm_poly[0][4][3], 0.0, 36);
	
	if (d->get_32_doubles(d, m->ledm_poly[0][5][0], boff + 0x2240, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[0][5][1], boff + 0x22d0, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[0][5][2], boff + 0x2360, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }

	vect_set(m->ledm_poly[0][5][3], 0.0, 36);
	
	if (d->get_32_doubles(d, m->ledm_poly[0][6][0], boff + 0x2384, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[0][6][1], boff + 0x2414, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[0][6][2], boff + 0x24a4, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }

	vect_set(m->ledm_poly[0][6][3], 0.0, 36);
	
	if (d->get_32_doubles(d, m->ledm_poly[0][7][0], boff + 0x24c8, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[0][7][1], boff + 0x2558, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[0][7][2], boff + 0x25e8, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }

	vect_set(m->ledm_poly[0][7][3], 0.0, 36);
	
	if (d->get_32_doubles(d, m->ledm_poly[0][1][0], boff + 0x260c, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[0][1][1], boff + 0x269c, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[0][1][2], boff + 0x272c, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[0][1][3], boff + 0x27bc, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[0][0][0], boff + 0x27e0, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[0][0][1], boff + 0x2870, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ledm_poly[0][0][2], boff + 0x2900, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_u8_doubles(d, m->ledm_poly[0][0][3], boff + 0x2990, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }


	if (d->get_32_doubles_padded(d, m->ee_m0_fwa, boff + 0x29b4, 18, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles_padded(d, m->ee_m1_fwa, boff + 0x29fc, 18, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles_padded(d, m->ee_m2_fwa, boff + 0x2a44, 18, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_suv_inttarg, boff + 0x2a8c, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_luv_inttarg, boff + 0x2a90, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, &m->ee_sluv_bl, boff + 0x2a94, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles_padded(d, m->ee_fwa_cal, boff + 0x2a98, 18, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles_padded(d, m->ee_fwa_std, boff + 0x2ae0, 18, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ee_ref_nn_illum, boff + 0x2b28, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_doubles(d, m->ee_ref_uv_illum, boff + 0x2bb8, 36, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (m->ee_version >= 1) {
		if (d->get_16_ints(d, &m->ee1_pol_led_luv_cur, boff + 0x2c48, 1, 1) == NULL)
			{ d->del(d); return I1PRO3_HW_EE_RANGE; }
		
		if (d->get_16_ints(d, &m->ee1_pol_led_suv_cur, boff + 0x2c4a, 1, 1) == NULL)
			{ d->del(d); return I1PRO3_HW_EE_RANGE; }
		
		if (d->get_32_doubles(d, &m->ee1_wltempcoef, boff + 0x2c4c, 1, 1) == NULL)
			{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	}

	/* Check the CRC */
	crc = d->get_crc(d);
	a1logd(p->log,3,"i1pro3_parse_eeprom: block 3 crc = 0x%x, should be 0x%x - %s\n",
	                             crc, m->ee_crc_3, crc == m->ee_crc_3 ? "OK": "BAD");
	if (crc != m->ee_crc_3)
		{ d->del(d); return I1PRO3_HW_EE_CHKSUM; }

	
	/* Block 4 */
	boff = m->ee_bk4_off;
	d->init_crc(d);

	if (d->get_32_ints(d, m->ee_unk27, boff + 0x0, 4, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, m->ee_unk28, boff + 0x10, 4, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk29, boff + 0x20, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk30, boff + 0x24, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, &m->ee_unk31, boff + 0x28, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	/* Block 5 */
	boff = m->ee_bk5_off;
	d->init_crc(d);

	if (d->get_8_asciiz(d, m->ee_supplier, boff + 0x0, 16, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, m->ee_unk32, boff + 0x10, 4, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	if (d->get_32_ints(d, m->ee_unk33, boff + 0x20, 256, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	/* Block 6 */
	boff = m->ee_bk6_off;
	d->init_crc(d);

	if (d->get_16_ints(d, &m->ee_unk34, boff + 0x0, 1, 1) == NULL)
		{ d->del(d); return I1PRO3_HW_EE_RANGE; }
	
	d->del(d); d = NULL;

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* Organize EEProm/calibration info: */
	
	/* Convert stray light compensation from ints to floats */
	m->straylight[0] = dmatrixz(0, 35, 0, 35);  
	for (i = 0; i < 36; i++) {
		for (j = 0; j < 36; j++) {
			m->straylight[0][i][j] = m->ee_straylight_scale * m->ee_straylight[i * 36 + j]/32767.0;
			if (i == j)
				m->straylight[0][i][j] += 1.0;
		}
	}

	/* Because the reference illumination spectrum level */
	/* is inversely proportional to the aperure area, */
	/* we could heuristically estimate the aperture size:

		double sum = 0.0, aperture;
		for (i = 0; i < 36; i++)
			sum += m->ee_ref_nn_illum[i];
		aperture = sqrt(6.9e9/sum);
	*/

	if (m->ee_devtype == 0x20)
		m->aperture = 1;

	/* Copy (some) ee values to working values */
	m->version = m->ee_version;
	m->serno = m->ee_serno;
	m->capabilities = m->ee_capabilities;

	/* Set nature of spectrometer system */
	m->nsen1 = 134; 		// emiss/non-mux: 6 extra samples over CCD
	m->nsen2 = 282; 		// refl/muxed:   2 x 134 + 14 extra
	m->nraw = 128;			// CCD 

	m->nwav[0] = 36; 
	m->wl_width[0] = 10.0;		/* Standard res. width */
	m->wl_short[0] = 380.0;		/* Standard res. range */
	m->wl_long[0] = 730.0;

	/* Fill high res in too */
	m->wl_width[1] = HIGHRES_WIDTH;
	m->wl_short[1] = HIGHRES_SHORT;
	m->wl_long[1] = HIGHRES_LONG;
	m->nwav[1] = (int)((m->wl_long[1]-m->wl_short[1])/HIGHRES_WIDTH + 0.5) + 1;	

	if (m->nsen2 > MX_NSEN)
		error("Assert in %s at line %d nsen2 %d > MX_NSEN %d\n",__FILE__,__LINE__,m->nsen2,MX_NSEN);
	if (m->nraw > MX_NRAW)
		error("Assert in %s at line %d nraw %d > MX_NRAW %d\n",__FILE__,__LINE__,m->nraw,MX_NRAW);
	if (m->nwav[1] > MX_NWAV)
		error("Assert in %s at line %d nwav[1] %d > MX_NWAV %d\n",__FILE__,__LINE__,m->nwav[1],MX_NWAV);

	/* Set standard res. references and create high res. */
	m->white_ref[0] = m->ee_white_ref;
	m->white_ref[1] = dvector(0, m->nwav[1]-1);
	good_upsample(p, m->white_ref[1], m->white_ref[0], 1, "White ref");

	m->emis_coef[0] = m->ee_emis_coef;
	m->emis_coef[1] = dvector(0, m->nwav[1]-1);
	good_upsample(p, m->emis_coef[1], m->emis_coef[0], 1, "Emis ref");
	correct_emis_coef(p, m->emis_coef[1]);		/* Hacky... */

	m->amb_coef[0] = m->ee_amb_coef;
	m->amb_coef[1] = dvector(0, m->nwav[1]-1);
	good_upsample(p, m->amb_coef[1], m->amb_coef[0], 1, "Amb ref");

	m->straylight[1] = dmatrixz(0, m->nwav[1]-1, 0, m->nwav[1]-1);  
	i1pro3_compute_hr_straylight(p);

	m->m0_fwa[0] = m->ee_m0_fwa;
	m->m0_fwa[1] = dvector(0, m->nwav[1]-1);
	good_upsample(p, m->m0_fwa[1], m->m0_fwa[0], 1, "M0 FWA");
	clear_low_wav(p, m->m0_fwa[1], 2, 1);

	m->m1_fwa[0] = m->ee_m1_fwa;
	m->m1_fwa[1] = dvector(0, m->nwav[1]-1);
	fast_upsample(p, m->m1_fwa[1], m->m1_fwa[0], 1, "M1 FWA");
	clear_low_wav(p, m->m1_fwa[1], 2, 1);

	m->m2_fwa[0] = m->ee_m2_fwa;
	m->m2_fwa[1] = dvector(0, m->nwav[1]-1);
	fast_upsample(p, m->m2_fwa[1], m->m2_fwa[0], 1, "M2 FWA");
	clear_low_wav(p, m->m2_fwa[1], 2, 1);

	m->fwa_cal[0] = m->ee_fwa_cal;
	m->fwa_cal[1] = dvector(0, m->nwav[1]-1);
	fast_upsample(p, m->fwa_cal[1], m->fwa_cal[0], 1, "FWA cal");

	m->fwa_std[0] = m->ee_fwa_std;
	m->fwa_std[1] = dvector(0, m->nwav[1]-1);
	fast_upsample(p, m->fwa_std[1], m->fwa_std[0], 1, "FWA std");
	clear_low_wav(p, m->fwa_std[1], 1, 1);

	/* Assumed minimum integration time */
	/* minintclks   = 165 */
	/* intclkp      = 1.5277777492883615e-5 */
	/* min_int_time = 2.5208332863257965e-3 */
	m->min_int_time = m->minintclks * m->intclkp;

	/* This isn't set in the EEProm. intclk count is 32 bit, so not limited by HW. */
	/* so pick a number... */
	m->max_int_time = MAX_INT_TIME;

	m->sens_sat = m->ee_sens_sat;	
	m->sens_target = 0.843 * m->sens_sat; 		/* (OEM has it hard coded as 45000) */

	a1logd(p->log,2,"sens_target %d, sens_sat %d\n", m->sens_target, m->sens_sat);

	/* Wavelength measurement parameters */
	m->wl_cal_inttime = m->ee_wlcal_intt;
	m->wl_cal_min_level = 500.0;
	m->wl_cal_fwhm = m->ee_fwhm;
	m->wl_cal_fwhm_tol = m->ee_fwhm_tol;
	for (i = 0; i < 128; i++)
		m->wl_led_spec[i] = (double)m->ee_wlcal_spec[i];
	m->wl_err_max = m->ee_wlcal_max;
	if (m->ee_version >= 1)
		m->wl_tempcoef = m->ee1_wltempcoef;		/* 0.0 if unknown */
	m->wl_refpeakloc = 73;				/* default - will be updated by i1pro3_match_wl_meas() */ 
	m->wl_refpeakwl = 527.5;			/* default - will be updated by i1pro3_match_wl_meas() */ 

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* Process, check and be verbose about the EEProm/calibration info */

	a1logd(p->log,2,"Serial number = %u\n",m->ee_serno);
	sprintf(m->sserno,"%u",m->ee_serno);

	a1logd(p->log,2,"Supplier = '%s'\n",m->ee_supplier);

	a1logd(p->log,2,"HW Id = %02x-%02x%02x%02x%02x%02x%02x%02x\n",
		                      m->hw_chipid[0], m->hw_chipid[1], m->hw_chipid[2], m->hw_chipid[3],
		                      m->hw_chipid[4], m->hw_chipid[5], m->hw_chipid[6], m->hw_chipid[7]);

	a1logd(p->log,2,"EE Id = %02x-%02x%02x%02x%02x%02x%02x%02x\n",
		                      m->ee_chipid[0], m->ee_chipid[1], m->ee_chipid[2], m->ee_chipid[3],
		                      m->ee_chipid[4], m->ee_chipid[5], m->ee_chipid[6], m->ee_chipid[7]);

	if (memcmp(m->hw_chipid, m->ee_chipid, 8) != 0) {
		a1logd(p->log,1,"i1pro3_parse_eeprom: HW ChipId doesn't match EE ChipID\n");
		return I1PRO3_HW_EE_CHIPID;
	}
	
	a1logd(p->log,2, "Date of manufacture = %d-%d-%d\n",
		                      m->ee_dom1 % 100, (m->ee_dom1/100) % 100, m->ee_dom1/10000);

	a1logd(p->log,2, "Non-linearity factors: %s\n",debPdvf(4, "%g", m->ee_lin));

#ifdef D_PLOT			/* [und] Use plots to show EE info -D value is high enough */
# pragma message("######### i1pro3_imp.c D_PLOT defined !!!!! ########")
	/* White tile, emissive/ambient calibrations */
	if (p->log->debug >= 7) {
		int i;
		double xx[128];
		double y1[128], y2[128];
	
		printf("ee_white_ref - White tile (Black) and ee_uv_white_ref (Red)\n");
		for (i = 0; i < 36; i++) {
			xx[i] = XSPECT_WL(380.0, 730.0, 36, i);
			y1[i] = m->ee_white_ref[i];
			y2[i] = m->ee_uv_white_ref[i];
		}
		do_plot(xx, y1, y2, NULL, 36);

		printf("ee_emis_coef (Black)\n");
		for (i = 0; i < 36; i++) {
			xx[i] = XSPECT_WL(380.0, 730.0, 36, i);
			y1[i] = m->ee_emis_coef[i];
		}
		do_plot(xx, y1, NULL, NULL, 36);

		printf("ee_amb_coef (Black)\n");
		for (i = 0; i < 36; i++) {
			xx[i] = XSPECT_WL(380.0, 730.0, 36, i);
			y1[i] = m->ee_amb_coef[i];
		}
		do_plot(xx, y1, NULL, NULL, 36);

		printf("ee_wlcal_spec (Black), ee_uv_wlcal_spec (red)\n");
		for (i = 0; i < 128; i++) {
			xx[i] = i;
			y1[i] = m->ee_wlcal_spec[i];
			y2[i] = m->ee_uv_wlcal_spec[i];
		}
		do_plot(xx, y1, y2, NULL, 128);
	}
#endif

#ifdef D_STRAYPLOT			/* Use plots to show EE info -D value is high enough */
	/* Stray light values */
	if (p->log->debug >= 7) {
		int i, j;
		double xx[36];
		double *yy[36];
	
		for (i = 0; i < 36; i++)
			xx[i] = XSPECT_WL(380.0, 730.0, 36, i);

		for (j = 0; j < 36; j += MXGPHS) {
			double w1, w2;
			int n = MXGPHS;

			if ((j + n) > 36)
				n = 36 - j;
 
			w1 = XSPECT_WL(380.0, 730.0, 36, j);
			w2 = XSPECT_WL(380.0, 730.0, 36, j + n -1);

			for (i = 0; i < n; i++)
				yy[i] = m->straylight[0][j+i];
			for (; i < 36; i++)
				yy[i] = NULL;

			a1logd(p->log,7,"Stray Light matrix %f - %f: \n",w1, w2);

			do_plotNpwz(xx, yy, 36, NULL, NULL, 0, 1, 0);
		}
	}
#else
	/* Stray light values */
	if (p->log->debug >= 9) {
		int i;

		for(i = 0; i < 36; i++) {
			double sum = 0.0;
			for (j = 0; j < 36; j++) {
				sum += m->straylight[0][i][j];
				a1logd(p->log,7,"  Wt %d = %f\n",j, m->straylight[0][i][j]);
			}
			a1logd(p->log,7,"  Sum = %f\n",sum);
		}
	}
#endif

#ifdef D_PLOT			/* [und] Use plots to show EE info -D value is high enough */
	/* raw to wav conversion graphs */
	if (p->log->debug >= 7) {
		int i;
		double xx[128];
		double y1[128], y2[128];
	
		printf(" raw -> wl refl (Black) emis (Red)\n");
		for (i = 0; i < 128; i++) {
			xx[i] = i;
			y1[i] = m->ee_wl_cal1[i];
			y2[i] = m->ee_wl_cal2[i];
		}
		do_plot(xx, y1, y2, NULL, 128);
	}
#endif

#ifdef D_PLOT			/* [und] Use plots to show EE info -D value is high enough */
	if (p->log->debug >= 7) {
		int i;
		double xx[36];
		double y1[36], y2[36], y3[36];
	
		printf("M0:m0_fwa (Black) M1:m1_fwa (Red) M2:m2_fwa (Green) FWA values\n");
		for (i = 0; i < 18; i++) {
			xx[i] = XSPECT_WL(380.0, 730.0, 36, i);
			y1[i] = m->ee_m0_fwa[i];
			y2[i] = m->ee_m1_fwa[i];
			y3[i] = m->ee_m2_fwa[i];
		}
		do_plot(xx, y1, y2, y3, 18);

		printf("ee_fwa_cal UV cal ?\n");
		for (i = 0; i < 18; i++) {
			xx[i] = XSPECT_WL(380.0, 730.0, 36, i);		// ???
			y1[i] = m->ee_fwa_cal[i];
		}
		do_plot(xx, y1, NULL, NULL, 18);

		printf("ee_fwa_std FWA standard ?\n");
		for (i = 0; i < 18; i++) {
			xx[i] = XSPECT_WL(380.0, 730.0, 36, i);	
			y1[i] = m->ee_fwa_std[i];
		}
		do_plot(xx, y1, NULL, NULL, 18);

		printf("ee_ref_nn_illum (Black) ee_ref_uv_illum (Red)\n");
		for (i = 0; i < 36; i++) {
			xx[i] = XSPECT_WL(380.0, 730.0, 36, i);	
			y1[i] = m->ee_ref_nn_illum[i];
			y2[i] = m->ee_ref_uv_illum[i];
		}
		do_plot(xx, y1, y2, NULL, 36);
	}
#endif

	return inst_ok;
}

/* ============================================================ */
/* EEProm contents parsing support. */
/* Unlike the USB protocol, this is all big endian. */

/* Generate the crc-32-c lookup table */
static void init_crc32c_table(ORD32 *table) {
	int ix, i;
	ORD32 mask;
	ORD32 val;
	
	/* Note that 0x82f63b78 is the reverse of Poly 0x1edc6f41 */
	for (ix = 0; ix < 256; ix++) {
		for (val = 0, mask = 1; mask < 0x100; mask <<= 1) {

			if ((ix & mask) != 0)
				val ^= 1;

			if (val & 1)
				val = (val >> 1) ^ 0x82f63b78;
			else
				val >>= 1;
		}
		table[ix] = val;
	}
}

/* Compute incremental CRC-32-C. */
/* For complete result start with 0xffffffff, and complement the final value */
static ORD32 comp_crc32c(ORD8 *buf, int len, ORD32 crc) {
	static ORD32 table[256] = { 0 };
	int i;

	if (table[1] == 0)
		init_crc32c_table(table);

	for (i = 0; i < len; i++) 
		crc = (crc >> 8) ^ table[(crc ^ buf[i]) & 0xff];

	return crc;
}

/* Return a pointer to an array of chars containing data from 8 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range. */
static unsigned char *i1data3_get_8_char(i1data3 *d, unsigned char *rv, int off, int count, int c) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 1) > d->len)
		return NULL;

	if (c) {
		d->crc = comp_crc32c(d->buf + off, 1 * count, d->crc);
	}

	if (rv == NULL) {
		if ((rv = (unsigned char *)malloc(sizeof(int) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 1) {
		rv[i] = d->buf[off];
	}
	return rv;
}

/* Return a pointer to an nul terminated string containing data from 8 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range. */
/* An extra space and a nul terminator will be added to the eeprom data */
static char *i1data3_get_8_asciiz(i1data3 *d, char *rv, int off, int count, int c) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 1) > d->len)
		return NULL;

	if (c) d->crc = comp_crc32c(d->buf + off, 1 * count, d->crc);

	if (rv == NULL) {
		if ((rv = (char *)malloc(sizeof(int) * (count + 1))) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 1) {
		rv[i] = (char)d->buf[off];
	}
	rv[i] = '\000';

	return rv;
}

/* Return a pointer to an array of ints containing data from 8 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range. */
static int *i1data3_get_8_ints(i1data3 *d, int *rv, int off, int count, int c) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 1) > d->len)
		return NULL;

	if (c) d->crc = comp_crc32c(d->buf + off, 1 * count, d->crc);

	if (rv == NULL) {
		if ((rv = (int *)malloc(sizeof(int) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 1) {
		rv[i] = ((signed char *)d->buf)[off];
	}
	return rv;
}

/* Return a pointer to an array of ints containing data from unsigned 8 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range. */
static int *i1data3_get_u8_ints(i1data3 *d, int *rv, int off, int count, int c) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 1) > d->len)
		return NULL;

	if (c) d->crc = comp_crc32c(d->buf + off, 1 * count, d->crc);

	if (rv == NULL) {
		if ((rv = (int *)malloc(sizeof(int) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 1) {
		rv[i] = d->buf[off];
	}
	return rv;
}

/* Return a pointer to an array of ints containing data from 16 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range. */
static int *i1data3_get_16_ints(i1data3 *d, int *rv, int off, int count, int c) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 2) > d->len)
		return NULL;

	if (c) d->crc = comp_crc32c(d->buf + off, 2 * count, d->crc);

	if (rv == NULL) {
		if ((rv = (int *)malloc(sizeof(int) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 2) {
		rv[i] = read_INR16_be(d->buf + off);
	}
	return rv;
}

/* Return a pointer to an array of ints containing data from unsigned 16 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range. */
static int *i1data3_get_u16_ints(i1data3 *d, int *rv, int off, int count, int c) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 2) > d->len)
		return NULL;

	if (c) d->crc = comp_crc32c(d->buf + off, 2 * count, d->crc);

	if (rv == NULL) {
		if ((rv = (int *)malloc(sizeof(int) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 2) {
		rv[i] = read_ORD16_be(d->buf + off);
	}
	return rv;
}

/* Return a pointer to an array of ints containing data from 32 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range. */
static int *i1data3_get_32_ints(i1data3 *d, int *rv, int off, int count, int c) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 4) > d->len)
		return NULL;

	if (c) d->crc = comp_crc32c(d->buf + off, 4 * count, d->crc);

	if (rv == NULL) {
		if ((rv = (int *)malloc(sizeof(int) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 4) {
		rv[i] = read_INR32_be(d->buf + off);
	}
	return rv;
}

/* Return a pointer to an array of unsigned ints containing data from unsigned 32 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range. */
static unsigned int *i1data3_get_u32_uints(i1data3 *d, unsigned int *rv, int off, int count, int c) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 4) > d->len)
		return NULL;

	if (c) d->crc = comp_crc32c(d->buf + off, 4 * count, d->crc);

	if (rv == NULL) {
		if ((rv = (unsigned int *)malloc(sizeof(unsigned int) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 4) {
		rv[i] = read_ORD32_be(d->buf + off);
	}
	return rv;
}

/* Return a pointer to an array of doubles containing data from 32 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range or malloc failure. */
static double *i1data3_get_32_doubles(i1data3 *d, double *rv, int off, int count, int c) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 4) > d->len)
		return NULL;

	if (c) d->crc = comp_crc32c(d->buf + off, 4 * count, d->crc);

	if (rv == NULL) {
		if ((rv = (double *)malloc(sizeof(double) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 4) {
		rv[i] = read_FLT32_be(d->buf + off);
	}
	return rv;
}

/* Return a pointer to an array of doubles containing data from 32 bits, */
/* with the array filled in reverse order. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range or malloc failure. */
static double *i1data3_rget_32_doubles(i1data3 *d, double *rv, int off, int count, int c) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 4) > d->len)
		return NULL;

	if (c) d->crc = comp_crc32c(d->buf + off, 4 * count, d->crc);

	if (rv == NULL) {
		if ((rv = (double *)malloc(sizeof(double) * count)) == NULL)
			return NULL;
	}

	for (i = count-1; i >= 0; i--, off += 4) {
		rv[i] = read_FLT32_be(d->buf + off);
	}
	return rv;
}

/* Return a pointer to an array of doubles containing data from 32 bits with zero padding */
static double *i1data3_get_32_doubles_padded(
i1data3 *d, double *rv, int off, int count, int retcount, int c) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 4) > d->len)
		return NULL;

	if (c) d->crc = comp_crc32c(d->buf + off, 4 * count, d->crc);

	if (rv == NULL) {
		if ((rv = (double *)malloc(sizeof(double) * retcount)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 4) {
		rv[i] = read_FLT32_be(d->buf + off);
	}

	for (; i < retcount; i++, off += 4) {
		rv[i] = 0.0;
	}

	return rv;
}

/* Return a pointer to an array of doubles containing data from u8 bits/255.0. */
static double *i1data3_get_u8_doubles(i1data3 *d, double *rv, int off, int count, int c) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 1) > d->len)
		return NULL;

	if (c) d->crc = comp_crc32c(d->buf + off, 1 * count, d->crc);

	if (rv == NULL) {
		if ((rv = (double *)malloc(sizeof(double) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 1) {
		rv[i] = d->buf[off]/255.0;
	}
	return rv;
}


/* Init the crc and checksum value */
void i1data3_init_crc(i1data3 *d) {
	d->crc = 0xffffffff;
	d->chsum = 0x0;
}

/* Return the crc value */
ORD32 i1data3_get_crc(i1data3 *d) {
	return ~d->crc;
}

/* Return the checksum value */
ORD32 i1data3_get_chsum(i1data3 *d) {
	return d->chsum;
}

/* Destroy ourselves */
static void i1data3_del(i1data3 *d) {
	del_a1log(d->log);		/* Unref it */
	free(d);
}

/* Constructor for i1data3 */
i1data3 *new_i1data3(i1pro3 *p, unsigned char *buf, int len) {
	i1data3 *d;
	if ((d = (i1data3 *)calloc(1, sizeof(i1data3))) == NULL) {
		a1loge(p->log, 1, "new_i1data3: malloc failed!\n");
		return NULL;
	}

	d->p = p;

	d->log = new_a1log_d(p->log);	/* Take reference */

	d->buf = buf;
	d->len = len;

	d->get_8_char      = i1data3_get_8_char;
	d->get_8_asciiz    = i1data3_get_8_asciiz;
	d->get_8_ints      = i1data3_get_8_ints;
	d->get_u8_ints     = i1data3_get_u8_ints;
	d->get_16_ints     = i1data3_get_16_ints;
	d->get_u16_ints    = i1data3_get_u16_ints;
	d->get_32_ints     = i1data3_get_32_ints;
	d->get_u32_uints   = i1data3_get_u32_uints;
	d->get_32_doubles  = i1data3_get_32_doubles;
	d->rget_32_doubles = i1data3_rget_32_doubles;
	d->get_32_doubles_padded = i1data3_get_32_doubles_padded;
	d->get_u8_doubles  = i1data3_get_u8_doubles;

	d->init_crc   = i1data3_init_crc;
	d->get_crc    = i1data3_get_crc;
	d->get_chsum  = i1data3_get_chsum;

	d->del        = i1data3_del;

	return d;
}

/* =======================================================================*/
/* Higher level measurement operations */
/* =======================================================================*/

/* Delay the given number of msec from last time the LEDs were */
/* turned off during reflective measurement */
void i1pro3_delay_llampoff(i1pro3 *p, unsigned int delay) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	unsigned int timssinceoff;

	if ((timssinceoff = (msec_time() - m->llamponoff)) < delay) {
		a1logd(p->log,3,"i1pro3_delay_llampoff: sleep %d msec\n",delay - timssinceoff);
		msec_sleep(delay - timssinceoff);
	}
}

/* Take a wavelength reference measurement */
/* (Measure and subtracts black, linearizes) */
i1pro3_code i1pro3_wl_measure(
	i1pro3 *p,
	double *raw,			/* Return array [nraw] of raw values */
	double *temp			/* Return the board temperature */
) {
	i1pro3_code ev = I1PRO3_OK;
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1p3_adapter atype;
	int nummeas;
	double inttime;
	double **raw_black, **raw_green;
	double *raw_wlmeas, maxval;

	nummeas = 1;
	inttime = m->wl_cal_inttime;		/* 0.1 secs */

	a1logd(p->log,3,"i1pro3_wl_measure called\n");

	if (m->capabilities & I1PRO3_CAP_HEAD_SENS) {
		if ((ev = i1pro3_getadaptype(p, &atype)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_getadaptype failed\n");
			return ev;
		}
	
		a1logd(p->log,4," adapter type = %d\n",atype);
	
		/* Are we in correct state for calibration ? */
		if (atype != i1p3_ad_cal
		 && atype != i1p3_ad_m3cal) {
			a1logd(p->log,1,"i1pro3_wl_measure: Need to be on calibration tile\n");
			return I1PRO3_SPOS_CAL;
		}
	}
	
	/* Read emissive black */
	if ((ev = i1pro3_do_measure(p, i1p3mm_em, &raw_black, &nummeas, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black failed\n");
		return ev;
	}

	a1logd(p->log,4," Got nummeas %d inttim %f from black\n",nummeas,inttime);

	/* Read emissive wl with same params */
	if ((ev = i1pro3_do_measure(p, i1p3mm_em_wl, &raw_green, &nummeas, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of green failed\n");
		i1pro3_free_raw(p, i1p3mm_em, raw_black, nummeas);
	}

	a1logd(p->log,4," Got nummeas %d inttim %f from green\n",nummeas,inttime);

	/* Compute black subtracted green */
	vect_sub3(raw, raw_green[0], raw_black[0], m->nraw);

	/* Check if green is saturated */
	maxval = vect_max(raw, m->nraw);
	if (maxval > m->sens_sat) {
		a1logd(p->log,1," green measure %f is saturated\n",maxval);
		i1pro3_free_raw(p, i1p3mm_em, raw_black, nummeas);
		i1pro3_free_raw(p, i1p3mm_em_wl, raw_green, nummeas);
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Check if we have sufficient signal */
	if (maxval < m->wl_cal_min_level) {
		a1logd(p->log,1," green measure %f is too small\n",maxval);
		i1pro3_free_raw(p, i1p3mm_em, raw_black, nummeas);
		i1pro3_free_raw(p, i1p3mm_em_wl, raw_green, nummeas);
		return I1PRO3_WL_TOOLOW;
	}

	/* Linearize */
	i1pro3_vect_lin(p, raw);
	
	/* Record the board temperature we calibrated at */
	if ((ev = i1pro3_getboardtemp(p, temp)) != I1PRO3_OK) {
		i1pro3_free_raw(p, i1p3mm_em, raw_black, nummeas);
		i1pro3_free_raw(p, i1p3mm_em_wl, raw_green, nummeas);
		return ev;
	}

#ifdef PLOT_DEBUG
	printf(" raw WL, black subtr, linearized:\n");
	plot_raw(raw);
#endif

	i1pro3_free_raw(p, i1p3mm_em, raw_black, nummeas);
	i1pro3_free_raw(p, i1p3mm_em_wl, raw_green, nummeas);

	return ev;
}

/* Do reflectance calibration */
i1pro3_code i1pro3_refl_cal(
	i1pro3 *p
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	int nummeasB, nummeasW;
	double inttime;
	double **raw_black1, **raw_black2, **raw_white;
	double *avg_black;
	double **wav_white;
	double **wav_ledm;
	double **wav_cor_ill;
	double **wav_avg_white;			/* Averaged, drift corrected */
	double **wav_avg_sl_white;		/* Averaged, drift corrected, possibly straylight */
	double sum_wav_avg_sl_white;
	double sum_ref_nn_illum;
	double sum_diffvals;
	int hr = 0;

	a1logd(p->log,3,"i1pro3_refl_cal\n");

	if (m->capabilities & I1PRO3_CAP_HEAD_SENS) {
		i1p3_adapter atype;

		if ((ev = i1pro3_getadaptype(p, &atype)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_getadaptype failed\n");
			return ev;
		}
	
		a1logd(p->log,4," adapter type = %d\n",atype);
	
		/* Are we in correct state for calibration ? */
		if (atype != i1p3_ad_cal) {
			a1logd(p->log,1,"Need to be on calibration tile\n");
			return I1PRO3_SPOS_CAL;
		}
	}

	/* Make sure LEDs have cooled down for 1 second */
	i1pro3_delay_llampoff(p, LED_OFF_TIME);

	/* Correct wl calibration for any temperature change */
	if (m->ee_version >= 1) {
		double temp;

		if ((ev = i1pro3_getboardtemp(p, &temp)) != I1PRO3_OK) {
			error(" i1pro3_getboardtemp failed\n");
			return ev;
		}
		if ((ev = i1pro3_recompute_wav_filters_for_temp(p, temp)) != I1PRO3_OK) {
			error(" i1pro3_recompute_wav_filters_for_temp failed\n");
			return ev;
		}
	}

	/* - - - - - - - - - - - - - - - - - - - - - */
	a1logd(p->log,3," i1pro3_refl_cal part 1\n");

	/* This first measurement is only used to establish a */
	/* reference led model for latter measurements - calsp_ledm. */
	/* The actual white measurement is not used for anything */
	/* (except we use it for patch recognition processing purposes.) */
	/* This whole thing could be thrown away and the next measurement */
	/* used instead... */

	inttime = s->inttime;	

	/* Read reflective black */
	nummeasB = i1pro3_comp_nummeas(p, s->dcaltime, inttime);
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black1, &nummeasB, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black failed\n");
		return ev;
	}

	/* Read reflective white using all LEDs */
	nummeasW = i1pro3_comp_nummeas(p, s->wscaltime, inttime);
	if ((ev =i1pro3_do_measure(p, i1p3mm_rf_wh, &raw_white, &nummeasW, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of white failed\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return ev;
	}

	a1logd(p->log,4," Got nummeas %d inttim %f from white\n",nummeasW,inttime);

	/* Check black */
	if (i1pro3_multimeas_check_black(p, raw_black1, nummeasB, inttime)) {  
		a1logd(p->log,1," black is too bright\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return I1PRO3_RD_DARKNOTVALID;
	}

	/* Average the black measurements together */
	avg_black = dvector(-1, m->nraw-1);  
	i1pro3_average_rawmmeas(p, avg_black, raw_black1, nummeasB);
	i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);

	/* Subtract black from white */
	i1pro3_multimeas_sub_black(p, raw_white, nummeasW, avg_black);
	free_dvector(avg_black, -1, m->nraw-1);  

	/* Check if white is saturated */
	if (i1pro3_multimeas_check_sat(p, NULL, raw_white, nummeasW)) {
		a1logd(p->log,1," white is saturated\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Check if white is consistent */
	if (i1pro3_multimeas_check_raw_consistency_x(p, raw_white, nummeasW)) {
		a1logd(p->log,1," white is inconsistent\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);
		return I1PRO3_RD_WHITEREADINCONS;
	}

	/* Linearize white */
	i1pro3_multimeas_lin(p, raw_white, nummeasW);

	/* Save average odd & even raw white measure for patch recognition reflection calc. */
	i1pro3_average_eorawmmeas(p, s->calraw_white, raw_white, nummeasW);

	/* Separate the odd and even. */
	/* Even will be 0..nummeasW/2, odd nummeasW/2 .. nummeasW */
	if (i1pro3_unshuffle(p, raw_white, nummeasW))
		return I1PRO3_INT_MALLOC;

	/* Compute LED model for non-uv and uv patches */
	wav_ledm = dmatrix(0, nummeasW-1, 0, 35);
	i1pro3_comp_ledm(p, wav_ledm, raw_white, nummeasW/2, 0);
	i1pro3_comp_ledm(p, wav_ledm + nummeasW/2, raw_white + nummeasW/2, nummeasW/2, 1);
	i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);

	/* Compute average of led models */
	i1pro3_average_wavmmeas(p, s->calsp_ledm[0], wav_ledm, nummeasW/2, 0);
	i1pro3_average_wavmmeas(p, s->calsp_ledm[1], wav_ledm + nummeasW/2, nummeasW/2, 0);

	free_dmatrix(wav_ledm, 0, nummeasW-1, 0, 35);

	/* - - - - - - - - - - - - - - - - - - - - - */
	a1logd(p->log,3," i1pro3_refl_cal part 2\n");

	/* Read reflective black 1 */
	nummeasB = i1pro3_comp_nummeas(p, s->dcaltime, inttime);
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black1, &nummeasB, &inttime, NULL, NULL))
		                                                                 != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black 1failed\n");
		return ev;
	}

	/* Read reflective white using all LEDs */
	nummeasW = i1pro3_comp_nummeas(p, s->wcaltime, inttime);
	if ((ev =i1pro3_do_measure(p, i1p3mm_rf_wh, &raw_white, &nummeasW, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of white failed\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return ev;
	}

	/* Read reflective black 2 */
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black2, &nummeasB, &inttime, NULL, NULL))
		                                                                 != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black 2 failed\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);
		return ev;
	}

	/* Check blacks */
	if (i1pro3_multimeas_check_black(p, raw_black1, nummeasB, inttime)
	 || i1pro3_multimeas_check_black(p, raw_black2, nummeasB, inttime)) {  
		a1logd(p->log,1," black is too bright\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return I1PRO3_RD_DARKNOTVALID;
	}

	/* Average the blacks together */
	avg_black = dvector(-1, m->nraw-1);  
	i1pro3_average_rawmmeas_2(p, avg_black, raw_black1, nummeasB, raw_black2, nummeasB);
	i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
	i1pro3_free_raw(p, i1p3mm_rf_black, raw_black2, nummeasB);

	/* Subtract black from white */
	i1pro3_multimeas_sub_black(p, raw_white, nummeasW, avg_black);
	free_dvector(avg_black, -1, m->nraw-1);  

	/* Check if white is saturated */
	if (i1pro3_multimeas_check_sat(p, NULL, raw_white, nummeasW)) {
		a1logd(p->log,1," white is saturated\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Check if white is consistent */
	if (i1pro3_multimeas_check_raw_consistency_x(p, raw_white, nummeasW)) {
		a1logd(p->log,1," white is iconsistent\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);
		return I1PRO3_RD_WHITEREADINCONS;
	}

	/* Linearize white and normalize to integration time */
	i1pro3_multimeas_lin(p, raw_white, nummeasW);
	i1pro3_normalize_rawmmeas(p, raw_white, nummeasW, inttime);

	/* Separate the odd and even. */
	/* Even will be 0..nummeasW/2, odd nummeasW/2 .. nummeasW */
	if (i1pro3_unshuffle(p, raw_white, nummeasW))
		return I1PRO3_INT_MALLOC;

	/* Compute LED model for non-uv and uv patches from first samples (?) */
	// ~~~~ this doesn't seem correct. Shouldn't we compute the model for all 
	// ~~~~ and then average them ??
	wav_ledm = dmatrix(0, 1, 0, MX_NWAV-1);	/* Allow space for upsample */
	i1pro3_comp_ledm(p, &wav_ledm[0], raw_white, 1, 0);
	i1pro3_comp_ledm(p, &wav_ledm[1], raw_white + nummeasW/2, 1, 1);

	/* Convert these into drift correction factors */
	vect_div3_safe(wav_ledm[0], s->calsp_ledm[0], wav_ledm[0], 36); 
	vect_div3_safe(wav_ledm[1], s->calsp_ledm[1], wav_ledm[1], 36); 

	for (hr = 0; hr < 2; hr++) {

		/* Convert from raw to wav nn & uv */
		wav_white = dmatrix(-9, nummeasW-1, -9, m->nwav[hr]-1);
		i1pro3_absraw_to_abswav(p, hr, 1, wav_white, raw_white, nummeasW);
 
		wav_avg_white = dmatrix(0, 1, 0, m->nwav[hr]-1);

		/* Compute average of white measurement */
		i1pro3_average_wavmmeas(p, wav_avg_white[0], wav_white, nummeasW/2, hr);
		i1pro3_average_wavmmeas(p, wav_avg_white[1], wav_white + nummeasW/2, nummeasW/2, hr);

		free_dmatrix(wav_white, -9, nummeasW-1, -9, m->nwav[hr]-1);

		if (hr) {	/* Second time around upsample drift correction */
			fast_upsample(p, wav_ledm[0], wav_ledm[0], 0, "nn drift corn");
			fast_upsample(p, wav_ledm[1], wav_ledm[1], 0, "uv drift corn");
		}
		/* Apply LED drift correction to avg whites */ 
		vect_mul(wav_avg_white[0], wav_ledm[0], m->nwav[hr]); 
		vect_mul(wav_avg_white[1], wav_ledm[1], m->nwav[hr]); 
		clear_low_wav(p, wav_avg_white[0], 4, hr);

		/* Make a copy of avg_white and apply stray light to copy, and zero short wl's */
		wav_avg_sl_white = dmatrix(0, 1, 0, m->nwav[hr]-1);
		vect_cpy(wav_avg_sl_white[0], wav_avg_white[0], m->nwav[hr]);
		vect_cpy(wav_avg_sl_white[1], wav_avg_white[1], m->nwav[hr]);
		i1pro3_straylight(p, hr, wav_avg_sl_white, 2);
		clear_low_wav(p, wav_avg_sl_white[0], 4, hr);

		if (hr == 0) {

			/* Correction factor from measured white illumination to reference measured */
			/* illumination with straylight correction */
			vect_div3(s->calsp_illcr[0][0], m->ee_ref_nn_illum, wav_avg_sl_white[0], 36); 
			vect_div3(s->calsp_illcr[0][1], m->ee_ref_uv_illum, wav_avg_sl_white[1], 36); 

			vect_set(s->calsp_illcr[0][0], 1.0, 4);		/* Reduce upsample artefacts */

			/* Create upsampled versions of illuminant correction */
			fast_upsample(p, s->calsp_illcr[1][0], s->calsp_illcr[0][0], 0, "nn illum corn");
			fast_upsample(p, s->calsp_illcr[1][1], s->calsp_illcr[0][1], 0, "uv illum corn");

			/* Clear short wl's */
			vect_set(s->calsp_illcr[0][0], 0.0, 4);
			clear_low_wav(p, s->calsp_illcr[1][0], 4, 1);
		}

		wav_cor_ill = dmatrix(0, 1, 0, m->nwav[hr]-1);

		/* Apply illumination correction to non-sl light corrected */
		vect_mul3(wav_cor_ill[0], s->calsp_illcr[hr][0], wav_avg_white[0], m->nwav[hr]); 
		vect_mul3(wav_cor_ill[1], s->calsp_illcr[hr][1], wav_avg_white[1], m->nwav[hr]); 

		/* Stray light correct illum corrected and zero short wl's */
		i1pro3_straylight(p, hr, wav_cor_ill, 2);
		clear_low_wav(p, wav_cor_ill[0], 4, hr);

		/* The use of the value wav_cor_ill[] is a bit doubtful! */
		/* It's going to be extremely close to ee_ref_nn/uv_illum, */
		/* and in fact is different by only 35 parts per million... */

		/* white_tile/(nn + UV corrected absolute illumination) */
		/* = 1.0/(2 x average LED illumination as if measured by sensor) */
		/* (Note that hi-res is overridden below with upsample) */
		vect_add3(s->iavg2aillum[hr], wav_cor_ill[0], wav_cor_ill[1], m->nwav[hr]);
		vect_div3(s->iavg2aillum[hr], m->white_ref[hr], s->iavg2aillum[hr], m->nwav[hr]);

		free_dmatrix(wav_cor_ill, 0, 1, 0, m->nwav[hr]-1);

		/* Scale compute scale so that integral of it matches the nn reference illuminant */
		sum_wav_avg_sl_white = vect_sum(wav_avg_sl_white[0], m->nwav[hr]);
		sum_ref_nn_illum = vect_sum(m->ee_ref_nn_illum, 36);

		if (hr)		/* Allow for discrete integral difference.. */
			sum_wav_avg_sl_white *= m->wl_width[1]/m->wl_width[0];

		vect_scale(s->sc_calsp_nn_white[hr], wav_avg_sl_white[0],
		            sum_ref_nn_illum/sum_wav_avg_sl_white, m->nwav[hr]);

		free_dmatrix(wav_avg_sl_white, 0, 1, 0, m->nwav[hr]-1);
		free_dmatrix(wav_avg_white, 0, 1, 0, m->nwav[hr]-1);
	}
	free_dmatrix(wav_ledm, 0, 1, 0, MX_NWAV-1);
	i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);

	/* - - - - - - - - - - - - - - - - - - - - - */
	a1logd(p->log,3," i1pro3_refl_cal part 3\n");

	/* Read reflective black 1 */
	nummeasB = i1pro3_comp_nummeas(p, s->dcaltime, inttime);
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black1, &nummeasB, &inttime, NULL, NULL))
		                                                                 != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black 1failed\n");
		return ev;
	}

	/* Read reflective white using all LEDs except short UV */
	/* i.e. we're reading whit + long UV */
	nummeasW = i1pro3_comp_nummeas(p, s->wcaltime, inttime);
	if ((ev =i1pro3_do_measure(p, i1p3mm_rf_whs, &raw_white, &nummeasW, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of white failed\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return ev;
	}

	/* Read reflective black 2 */
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black2, &nummeasB, &inttime, NULL, NULL))
		                                                                 != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black 2 failed\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);
		return ev;
	}

	/* Check blacks */
	if (i1pro3_multimeas_check_black(p, raw_black1, nummeasB, inttime)
	 || i1pro3_multimeas_check_black(p, raw_black2, nummeasB, inttime)) {  
		a1logd(p->log,1," black is too bright\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return I1PRO3_RD_DARKNOTVALID;
	}

	/* Average the blacks together */
	avg_black = dvector(-1, m->nraw-1);  
	i1pro3_average_rawmmeas_2(p, avg_black, raw_black1, nummeasB, raw_black2, nummeasB);
	i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
	i1pro3_free_raw(p, i1p3mm_rf_black, raw_black2, nummeasB);

	/* Subtract black from white */
	i1pro3_multimeas_sub_black(p, raw_white, nummeasW, avg_black);
	free_dvector(avg_black, -1, m->nraw-1);  

	/* Check if white is saturated */
	if (i1pro3_multimeas_check_sat(p, NULL, raw_white, nummeasW)) {
		a1logd(p->log,1," white is saturated\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Check if white is consistent */
	if (i1pro3_multimeas_check_raw_consistency_x(p, raw_white, nummeasW)) {
		a1logd(p->log,1," white is iconsistent\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);
		return I1PRO3_RD_WHITEREADINCONS;
	}

	/* Linearize white and normalize */
	i1pro3_multimeas_lin(p, raw_white, nummeasW);
	i1pro3_normalize_rawmmeas(p, raw_white, nummeasW, inttime);

	/* Separate the odd and even. */
	/* Even will be 0..nummeasW/2, odd nummeasW/2 .. nummeasW */
	if (i1pro3_unshuffle(p, raw_white, nummeasW))
		return I1PRO3_INT_MALLOC;

	/* Compute LED model for non-uv and uv patches from first samples (?) */
	wav_ledm = dmatrix(0, 1, 0, MX_NWAV-1);	/* Allow space for upsample */
	i1pro3_comp_ledm(p, &wav_ledm[0], raw_white, 1, 0);
	i1pro3_comp_ledm(p, &wav_ledm[1], raw_white + nummeasW/2, 1, 1);

	/* Convert these into drift correction factors */
	vect_div3_safe(wav_ledm[0], s->calsp_ledm[0], wav_ledm[0], 36); 
	vect_div3_safe(wav_ledm[1], s->calsp_ledm[1], wav_ledm[1], 36); 

	for (hr = 0; hr < 2; hr++) {
		
		/* Convert from raw to wav nn & uv */
		wav_white = dmatrix(-9, nummeasW-1, -9, m->nwav[hr]-1);
		i1pro3_absraw_to_abswav(p, hr, 1, wav_white, raw_white, nummeasW);
 
		/* Compute average of white measurement */
		wav_avg_white = dmatrix(0, 1, 0, m->nwav[hr]-1);
		i1pro3_average_wavmmeas(p, wav_avg_white[0], wav_white, nummeasW/2, hr);
		i1pro3_average_wavmmeas(p, wav_avg_white[1], wav_white + nummeasW/2, nummeasW/2, hr);

		free_dmatrix(wav_white, -9, nummeasW-1, -9, m->nwav[hr]-1);

		if (hr) {	/* Second time around upsample drift correction */
			fast_upsample(p, wav_ledm[0], wav_ledm[0], 0, "nn drift corn");
			fast_upsample(p, wav_ledm[1], wav_ledm[1], 0, "uv drift corn");
		}

		/* Apply LED drift correction to avg whites */ 
		vect_mul(wav_avg_white[0], wav_ledm[0], m->nwav[hr]); 
		vect_mul(wav_avg_white[1], wav_ledm[1], m->nwav[hr]); 

		/* Apply stray light correction and clear nn short wl's */
		i1pro3_straylight(p, hr, wav_avg_white, 2);
		if (hr)
			clear_low_wav2(p, wav_avg_white[0], 405.0, 415.0, hr);
		else
			vect_set(wav_avg_white[0], 0.0, 4);

		/* Compute difference between uv and nn */
		vect_sub3(s->cal_l_uv_diff[hr], wav_avg_white[1], wav_avg_white[0], m->nwav[hr]);

		/* Clear all except short wavelength values */
		if (hr)
			clear_high_wav2(p, s->cal_l_uv_diff[hr], 485.0, 505.0, hr);
		else
			vect_set(s->cal_l_uv_diff[hr] + 12, 0.0, 36 - 12);

		sum_diffvals = vect_sum(s->cal_l_uv_diff[hr], m->nwav[hr]);
		if (hr)		/* Allow for discrete integral difference.. */
			sum_diffvals *= m->wl_width[1]/m->wl_width[0];
		vect_scale1(s->cal_l_uv_diff[hr], m->ee_luv_inttarg/sum_diffvals, m->nwav[hr]);

		free_dmatrix(wav_avg_white, 0, 1, 0, m->nwav[hr]-1);
	}
	free_dmatrix(wav_ledm, 0, 1, 0, MX_NWAV-1);
	i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);

	/* - - - - - - - - - - - - - - - - - - - - - */
	a1logd(p->log,3," i1pro3_refl_cal part 4\n");

	/* Read reflective black 1 */
	nummeasB = i1pro3_comp_nummeas(p, s->dcaltime, inttime);
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black1, &nummeasB, &inttime, NULL, NULL))
		                                                                 != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black 1failed\n");
		return ev;
	}

	/* Read reflective white using all LEDs except long UV */
	/* i.e. we're reading whit + short UV */
	nummeasW = i1pro3_comp_nummeas(p, s->wcaltime, inttime);
	if ((ev =i1pro3_do_measure(p, i1p3mm_rf_whl, &raw_white, &nummeasW, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of white failed\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return ev;
	}

	/* Read reflective black 2 */
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black2, &nummeasB, &inttime, NULL, NULL))
		                                                                 != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black 2 failed\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);
		return ev;
	}

	/* Check blacks */
	if (i1pro3_multimeas_check_black(p, raw_black1, nummeasB, inttime)
	 || i1pro3_multimeas_check_black(p, raw_black2, nummeasB, inttime)) {  
		a1logd(p->log,1," black is too bright\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return I1PRO3_RD_DARKNOTVALID;
	}

	/* Average the blacks together */
	avg_black = dvector(-1, m->nraw-1);  
	i1pro3_average_rawmmeas_2(p, avg_black, raw_black1, nummeasB, raw_black2, nummeasB);
	i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
	i1pro3_free_raw(p, i1p3mm_rf_black, raw_black2, nummeasB);

	/* Subtract black from white */
	i1pro3_multimeas_sub_black(p, raw_white, nummeasW, avg_black);
	free_dvector(avg_black, -1, m->nraw-1);  

	/* Check if white is saturated */
	if (i1pro3_multimeas_check_sat(p, NULL, raw_white, nummeasW)) {
		a1logd(p->log,1," white is saturated\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Check if white is consistent */
	if (i1pro3_multimeas_check_raw_consistency_x(p, raw_white, nummeasW)) {
		a1logd(p->log,1," white is iconsistent\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);
		return I1PRO3_RD_WHITEREADINCONS;
	}

	/* Linearize white and normalize */
	i1pro3_multimeas_lin(p, raw_white, nummeasW);
	i1pro3_normalize_rawmmeas(p, raw_white, nummeasW, inttime);

	/* Separate the odd and even. */
	/* Even will be 0..nummeasW/2, odd nummeasW/2 .. nummeasW */
	if (i1pro3_unshuffle(p, raw_white, nummeasW))
		return I1PRO3_INT_MALLOC;

	/* Compute LED model for non-uv and uv patches from first samples (?) */
	wav_ledm = dmatrix(0, 1, 0, MX_NWAV-1);	/* Allow space for upsample */
	i1pro3_comp_ledm(p, &wav_ledm[0], raw_white, 1, 0);
	i1pro3_comp_ledm(p, &wav_ledm[1], raw_white + nummeasW/2, 1, 1);

	/* Convert these into drift correction factors */
	vect_div3_safe(wav_ledm[0], s->calsp_ledm[0], wav_ledm[0], 36); 
	vect_div3_safe(wav_ledm[1], s->calsp_ledm[1], wav_ledm[1], 36); 

	for (hr = 0; hr < 2; hr++) {
		
		/* Convert from raw to wav nn & uv */
		wav_white = dmatrix(-9, nummeasW-1, -9, m->nwav[hr]-1);
		i1pro3_absraw_to_abswav(p, hr, 1, wav_white, raw_white, nummeasW);
 
		/* Compute average of white measurement */
		wav_avg_white = dmatrix(0, 1, 0, m->nwav[hr]-1);
		i1pro3_average_wavmmeas(p, wav_avg_white[0], wav_white, nummeasW/2, hr);
		i1pro3_average_wavmmeas(p, wav_avg_white[1], wav_white + nummeasW/2, nummeasW/2, hr);

		free_dmatrix(wav_white, -9, nummeasW-1, -9, m->nwav[hr]-1);

		if (hr) {	/* Second time around upsample drift correction */
			fast_upsample(p, wav_ledm[0], wav_ledm[0], 0, "nn drift corn");
			fast_upsample(p, wav_ledm[1], wav_ledm[1], 0, "uv drift corn");
		}

		/* Apply LED drift correction to avg whites */ 
		vect_mul(wav_avg_white[0], wav_ledm[0], m->nwav[hr]); 
		vect_mul(wav_avg_white[1], wav_ledm[1], m->nwav[hr]); 

		/* Apply stray light correction and clear nn short wl's */
		i1pro3_straylight(p, hr, wav_avg_white, 2);
		if (hr)
			clear_low_wav2(p, wav_avg_white[0], 405.0, 415.0, hr);
		else 
			vect_set(wav_avg_white[0], 0.0, 4);

		/* Compute difference between uv and nn */
		vect_sub3(s->cal_s_uv_diff[hr], wav_avg_white[1], wav_avg_white[0], m->nwav[hr]);

		/* Clear all except short wavelength values */
		if (hr)
			clear_high_wav2(p, s->cal_s_uv_diff[hr], 485.0, 505.0, hr);
		else
			vect_set(s->cal_s_uv_diff[hr] + 12, 0.0, 36 - 12);

		sum_diffvals = vect_sum(s->cal_s_uv_diff[hr], m->nwav[hr]);
		if (hr)		/* Allow for discrete integral difference.. */
			sum_diffvals *= m->wl_width[1]/m->wl_width[0];
		vect_scale1(s->cal_s_uv_diff[hr], m->ee_suv_inttarg/sum_diffvals, m->nwav[hr]);

		free_dmatrix(wav_avg_white, 0, 1, 0, m->nwav[hr]-1);
	}
	free_dmatrix(wav_ledm, 0, 1, 0, MX_NWAV-1);
	i1pro3_free_raw(p, i1p3mm_rf_wh, raw_white, nummeasW);

	return ev; 
}

/* Do a spot reflectance measurement */
i1pro3_code i1pro3_spot_refl_meas(
	i1pro3 *p,
	double **specrd,	/* Cooked spectral patch value to return */
	int hr				/* 1 for high-res. */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	int nummeasB, nummeasW;
	double inttime;
	double **raw_black1, **raw_sample;	/* raw measurements */
	double *raw_avg_black;				/* raw average black */
	double **proc_sample;				/* processed sample values */
	double **eproc_sample;				/* eproc_sample[enumsample][-9, nwav] */
	int enummeas;						/* Number of even samples */
	double **oproc_sample;				/* oproc_sample[onumsample][-9, nwav] */
	int onummeas;						/* Number of odd samples */
	double *m0 = NULL, *m1 = NULL, *m2 = NULL;
	int i;

	a1logd(p->log,3,"i1pro3_spot_refl_meas\n");

	if (m->capabilities & I1PRO3_CAP_HEAD_SENS) {
		i1p3_adapter atype;

		if ((ev = i1pro3_getadaptype(p, &atype)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_getadaptype failed\n");
			return ev;
		}
	
		/* Do we have a reflection adapter fitted ? */
		if ((atype & i1p3_ad_standard) == 0) {
			a1logd(p->log,1,"Expect a standard measurement adapter\n");
			return I1PRO3_SPOS_STD;
		}
	}

	/* Make sure LEDs have cooled down for 1 second */
	i1pro3_delay_llampoff(p, LED_OFF_TIME);

	/* Correct wl calibration for any temperature change */
	if (m->ee_version >= 1) {
		double temp;

		if ((ev = i1pro3_getboardtemp(p, &temp)) != I1PRO3_OK) {
			error(" i1pro3_getboardtemp failed\n");
			return ev;
		}
		if ((ev = i1pro3_recompute_wav_filters_for_temp(p, temp)) != I1PRO3_OK) {
			error(" i1pro3_recompute_wav_filters_for_temp failed\n");
			return ev;
		}
	}

	inttime = s->inttime;

	/* Read reflective black */
	nummeasB = i1pro3_comp_nummeas(p, s->dreadtime, inttime);
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black1, &nummeasB, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black failed\n");
		return ev;
	}

	/* Read reflective sample using all LEDs */
	nummeasW = i1pro3_comp_nummeas(p, s->wreadtime, inttime);
	if ((ev =i1pro3_do_measure(p, i1p3mm_rf_wh, &raw_sample, &nummeasW, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of sample failed\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return ev;
	}

	/* Don't check black for measurement */

	/* Average the black together */
	raw_avg_black = dvector(-1, m->nraw-1);  
	i1pro3_average_rawmmeas(p, raw_avg_black, raw_black1, nummeasB);
	i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);

//dump_dvector(stdout, "raw_avg_black", " ", raw_avg_black, 128);
//printf("Black:\n"); plot_raw(raw_avg_black);

	/* Subtract black from sample */
	i1pro3_multimeas_sub_black(p, raw_sample, nummeasW, raw_avg_black);
	free_dvector(raw_avg_black, -1, m->nraw-1);  

	/* Check if sample is saturated */
	if (i1pro3_multimeas_check_sat(p, NULL, raw_sample, nummeasW)) {
		a1logd(p->log,1," sample is saturated\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_sample, nummeasW);
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Check if sample is consistent */
	if (i1pro3_multimeas_check_raw_consistency_x(p, raw_sample, nummeasW)) {
		a1logd(p->log,1," sample is inconsistent\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_sample, nummeasW);
		return I1PRO3_RD_READINCONS;
	}

	/* Linearize sample and normalize to inttime */
	i1pro3_multimeas_lin(p, raw_sample, nummeasW);
	i1pro3_normalize_rawmmeas(p, raw_sample, nummeasW, inttime);

	/* Convert from raw to wav */
	proc_sample = dmatrix(0, nummeasW-1, -9, m->nwav[hr]-1);
	i1pro3_absraw_to_abswav(p, hr, 1, proc_sample, raw_sample, nummeasW);
	i1pro3_free_raw(p, i1p3mm_rf_wh, raw_sample, nummeasW);

	/* Split into odd and even wav samples */
	/* (For scan with zebra this will be much more complex...) */
	enummeas = nummeasW/2;
	eproc_sample = dmatrix(0, enummeas, -9, m->nwav[hr]-1); 
	onummeas = nummeasW/2;
	oproc_sample = dmatrix(0, onummeas, -9, m->nwav[hr]-1);

	for (i = 0; i < nummeasW; i += 2) {
		vect_cpy(eproc_sample[i/2]-9, proc_sample[i + 0]-9, 9+m->nwav[hr]);
		vect_cpy(oproc_sample[i/2]-9, proc_sample[i + 1]-9, 9+m->nwav[hr]);
	}
	free_dmatrix(proc_sample, 0, nummeasW-1, -9, m->nwav[hr]-1);

	if (m->filt == inst_opt_filter_none)
		m0 = specrd[0];
	else if (m->filt == inst_opt_filter_D50)
		m1 = specrd[0];
	else if (m->filt == inst_opt_filter_UVCut)
		m2 = specrd[0];
	else {
		a1logd(p->log,1," wrong filter 0x%x\n",m->filt);
		free_dmatrix(eproc_sample, 0, enummeas, -9, m->nwav[hr]-1); 
		free_dmatrix(oproc_sample, 0, onummeas, -9, m->nwav[hr]-1);
		return ev;
	}
	
	/* Convert raw samples to patch reflective spectral values.*/
	if ((ev = i1pro3_comp_refl_value(
		p,
		m0, m1, m2,
		eproc_sample, enummeas,
		oproc_sample, onummeas,
		hr)) != I1PRO3_OK) {

		a1logd(p->log,1," conversion to calibrated spectral failed\n");
		free_dmatrix(eproc_sample, 0, enummeas, -9, m->nwav[hr]-1); 
		free_dmatrix(oproc_sample, 0, onummeas, -9, m->nwav[hr]-1);
		return ev;
	}

	free_dmatrix(eproc_sample, 0, enummeas, -9, m->nwav[hr]-1); 
	free_dmatrix(oproc_sample, 0, onummeas, -9, m->nwav[hr]-1);

	return ev;
}

/* Do a strip reflectance measurement */
i1pro3_code i1pro3_strip_refl_meas(
	i1pro3 *p,
	double **specrd,	/* Cooked spectral patches values to return */
	int nvals,			/* Number of patches expected */
	int hr				/* 1 for high-res. */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	int nummeasB, nummeasW;
	double inttime;
	double **raw_black1, **raw_sample;	/* raw measurements */
	double **wav_sample;			/* processed wav measurements */
	double **ewav_sample;			/* ewav_sample[enumsample][-9, nwav] */
	double **owav_sample;			/* owav_sample[onumsample][-9, nwav] */
	int **p2m = NULL;				/* zebra list of sample indexes in mm. order */
	int npos = 0;					/* Number of position slots */
	double **filt_sample;			/* filtered raw measurements */
	int nfiltsamp;					/* Number of filtered raw measurements */
	double *raw_avg_black;			/* raw average black */
	i1pro3_patch *patch;			/* List of patch locations within scan data */
	double *m0 = NULL, *m1 = NULL, *m2 = NULL;
	int i, j, k;

	a1logd(p->log,3,"i1pro3_strip_refl_meas\n");

	if (m->capabilities & I1PRO3_CAP_HEAD_SENS) {
		i1p3_adapter atype;

		if ((ev = i1pro3_getadaptype(p, &atype)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_getadaptype failed\n");
			return ev;
		}
	
		/* Do we have a reflection adapter fitted ? */
		if ((atype & i1p3_ad_standard) == 0) {
			a1logd(p->log,1,"Expect a standard measurement adapter\n");
			return I1PRO3_SPOS_STD;
		}
	}

	/* Make sure LEDs have cooled down for 1 second */
	i1pro3_delay_llampoff(p, LED_OFF_TIME);

	inttime = s->inttime;

	/* Correct wl calibration for any temperature change */
	if (m->ee_version >= 1) {
		double temp;

		if ((ev = i1pro3_getboardtemp(p, &temp)) != I1PRO3_OK) {
			error(" i1pro3_getboardtemp failed\n");
			return ev;
		}
		if ((ev = i1pro3_recompute_wav_filters_for_temp(p, temp)) != I1PRO3_OK) {
			error(" i1pro3_recompute_wav_filters_for_temp failed\n");
			return ev;
		}
	}

	/* Read reflective black */
	nummeasB = i1pro3_comp_nummeas(p, s->dreadtime, inttime);
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black1, &nummeasB, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black failed\n");
		return ev;
	}

	/* Read strip reflective sample using all LEDs. Read zebra ruler if possible too. */
	nummeasW = 0;
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_wh, &raw_sample, &nummeasW, &inttime, &p2m, &npos)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of sample failed\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return ev;
	}

	a1logd(p->log,2," i1pro3_do_measure strip returned %d nummeas\n",nummeasW);
	
	/* Don't check black for measurement */

	/* Average the black together */
	raw_avg_black = dvector(-1, m->nraw-1);  
	i1pro3_average_rawmmeas(p, raw_avg_black, raw_black1, nummeasB);
	i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);

//dump_dvector(stdout, "raw_avg_black", " ", raw_avg_black, 128);
//printf("Black:\n"); plot_raw(raw_avg_black);

	/* Subtract black from samples */
	i1pro3_multimeas_sub_black(p, raw_sample, nummeasW, raw_avg_black);
	free_dvector(raw_avg_black, -1, m->nraw-1);  

	/* Check if samples are saturated */
	if (i1pro3_multimeas_check_sat(p, NULL, raw_sample, nummeasW)) {
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_sample, nummeasW);
		a1logd(p->log,1," sample is saturated\n");
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Linearize samples */
	i1pro3_multimeas_lin(p, raw_sample, nummeasW);

	/* Create copy of samples to filter */
	nfiltsamp = nummeasW;
	filt_sample = dmatrix(0, nfiltsamp-1, -9, m->nraw-1);
	copy_dmatrix(filt_sample, raw_sample, 0, nummeasW-1, -9, m->nraw-1);

	/* convert to reflection and filter UV even/odd mux noise out */
	/* for use in patch recognition. */
	i1pro3_filter_uvmux(p, filt_sample, nfiltsamp, hr);

#undef PLOT_FILLINS

	/* Convert time based patch recognition proxy into position based one */
	if (p2m != NULL) {
		double **pos_sample;			/* Position based raw samples */
#ifdef PLOT_FILLINS
		double **plot = dmatrixz(0, 2, 0, npos-1);  
#endif

		pos_sample = dmatrix(0, npos-1, 0, m->nraw-1);

		/* Average samples values into position slots */
		for (i = 0; i < npos; i++) {
			vect_set(pos_sample[i], 0.0, m->nraw);

			if (p2m[i][-1] > 0) {
				for (j = 0; j < p2m[i][-1]; j++)
					vect_add(pos_sample[i], filt_sample[p2m[i][j]], m->nraw);
				vect_scale1(pos_sample[i], 1.0/(double)j, m->nraw);
			}
		}

#ifdef PLOT_FILLINS
		for (i = 0; i < npos; i++) {
			plot[0][i] = (double)i;
			plot[1][i] = pos_sample[i][50];
		}
#endif

		/* Now interpolate any empty slots */
		for (i = 0; i < npos; i++) {
			double bf;

			if (p2m[i][-1] != 0)
				continue;

			for (j = i-1; j >= 0; j--)		/* Search for lower non-empty */
				if (p2m[j][-1] != 0)
					break;

			for (k = i+1; k < npos; k++)		/* Search for upper non-empty */
				if (p2m[k][-1] != 0)
					break;

			if (j < 0) {
				j = k;
				bf = 0.5;
			} else if (k >= npos) {
				k = j;
				bf = 0.5;
			} else {
				bf = (i - j) / (double)(k - j);
			}
			vect_blend(pos_sample[i], pos_sample[j], pos_sample[k], bf, m->nraw);
		}

#ifdef PLOT_FILLINS
		for (i = 0; i < npos; i++) 
			plot[2][i] = pos_sample[i][50];
		printf("Fill-ins on band 50:\n");
		do_plot(plot[0], plot[1], plot[2], NULL, npos);
#endif

		/* Switch position proxy for time one */
		free_dmatrix(filt_sample, 0, nfiltsamp-1, -9, m->nraw-1);
		filt_sample = pos_sample;
		nfiltsamp = npos;
	}

	/* Room for returned patch information */
	if ((patch = malloc(sizeof(i1pro3_patch) * nvals)) == NULL) {
		if (p2m != NULL) del_zebix_list(p2m, npos);
		free_dmatrix(filt_sample, 0, nfiltsamp-1, -9, m->nraw-1);
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_sample, nummeasW); raw_sample = NULL;
		a1logd(p->log,1,"i1pro3_strip_refl_meas malloc %ld bytes failed\n",sizeof(i1pro3_patch) * nvals);
		return I1PRO3_INT_MALLOC;
	}
 
	/* Locate patch boundaries */
	if ((ev = i1pro3_locate_patches(p, patch, nvals, filt_sample, nfiltsamp, p2m)) != I1PRO3_OK) {
		if (p2m != NULL) del_zebix_list(p2m, npos);
		free(patch);
		free_dmatrix(filt_sample, 0, nfiltsamp-1, -9, m->nraw-1);
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_sample, nummeasW); raw_sample = NULL;
		a1logd(p->log,1," i1pro3_do_measure of sample failed\n");
		return ev;
	}

#ifdef PATREC_DEBUG 
	for (i = 0; i < nvals; i++) {
		int acount = patch[i].no;

		if (p2m != NULL) {		/* Count underlying samples in patch */
			for (acount = k = 0; k < patch[i].no; k++)
				acount += p2m[patch[i].ss + k][-1];
		}
	
		printf("patch %d: @ %d len %d (%d)\n",i,patch[i].ss,patch[i].no,acount);
	}
#endif // NEVER

	free_dmatrix(filt_sample, 0, nfiltsamp-1, -9, m->nraw-1);

	/* Allocate array of pointers to even/odd samples of max possible length */
	if ((ewav_sample = malloc(sizeof(double *) * (nummeasW/2+1))) == NULL) {
		if (p2m != NULL) del_zebix_list(p2m, npos);
		free(patch);
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_sample, nummeasW); raw_sample = NULL;
		a1logd(p->log,1,"i1pro3_strip_refl_meas malloc %ld bytes failed\n",sizeof(double *) * (nummeasW/2+1));
		return I1PRO3_INT_MALLOC;
	}
	if ((owav_sample = malloc(sizeof(double *) * (nummeasW/2+1))) == NULL) {
		if (p2m != NULL) del_zebix_list(p2m, npos);
		free(ewav_sample);
		free(patch);
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_sample, nummeasW); raw_sample = NULL;
		a1logd(p->log,1,"i1pro3_strip_refl_meas malloc %ld bytes failed\n",sizeof(double *) * (nummeasW/2+1));
		return I1PRO3_INT_MALLOC;
	}

	/* Normalize raw samples */
	i1pro3_normalize_rawmmeas(p, raw_sample, nummeasW, inttime);

	/* Convert samples from raw to wav */
	wav_sample = dmatrix(0, nummeasW-1, -9, m->nwav[hr]-1);
	i1pro3_absraw_to_abswav(p, hr, 1, wav_sample, raw_sample, nummeasW);
	i1pro3_free_raw(p, i1p3mm_rf_wh, raw_sample, nummeasW); raw_sample = NULL;

	/* Process each patch into spectral values */
	for (i = 0; i < nvals; i++) {
		int eix, enummeas;			/* Number of even samples */
		int oix, onummeas;			/* Number of odd samples */

		/* Split into odd and even wav samples */
		if (p2m != NULL) {

			/* Copy list of patches in p2m list */
			for (eix = oix = j = 0; j < patch[i].no; j++) {
				int *plist = p2m[patch[i].ss + j];	/* List of patches in this slot */

				for (k = 0; k < plist[-1]; k++) {
					int ix = plist[k];
					if ((ix & 1) == 0)
						ewav_sample[eix++] = wav_sample[ix];
					else
						owav_sample[oix++] = wav_sample[ix];
				}
			}
			enummeas = eix;
			onummeas = oix;

		} else {
			/* Count number of even/odd samples in patch */
			enummeas = patch[i].no/2;			/* Round down */
			onummeas = patch[i].no/2;
			if ((patch[i].no & 1) != 0) {		/* Assign any odd sample */
				if ((patch[i].ss & 1) != 0) 
					onummeas++;
				else
					enummeas++;
			}

			/* Copy even/odd samples pointers to wav values */
			for (eix = oix = j = 0; j < patch[i].no; j++) {
				if (((patch[i].ss + j) & 1) == 0)
					ewav_sample[eix++] = wav_sample[j];
				else
					owav_sample[oix++] = wav_sample[j];
			}
		}

#ifdef NEVER	/* Check we're not mixing up odd & even samples.. */
		printf("%d Even samples:\n",enummeas);
		plot_wav_N(m, hr, ewav_sample, enummeas);
		printf("%d Odd samples:\n",onummeas);
		plot_wav_N(m, hr, owav_sample, onummeas);
#endif // NEVER

		/* Check if each patch is consistent */
		if (i1pro3_multimeas_check_wav_consistency2(p, hr, inttime, ewav_sample, enummeas,
			                                                        owav_sample, onummeas)) {
			if (p2m != NULL) del_zebix_list(p2m, npos);
			free(ewav_sample);
			free(owav_sample);
			free(patch);
			free_dmatrix(wav_sample, 0, nummeasW-1, -9, m->nwav[hr]-1);
			a1logd(p->log,1," sample is inconsistent\n");
			return I1PRO3_RD_READINCONS;
		}

		/* Convert raw samples to patch reflective spectral values.*/
		if (m->filt == inst_opt_filter_none)
			m0 = specrd[i];
		else if (m->filt == inst_opt_filter_D50)
			m1 = specrd[i];
		else if (m->filt == inst_opt_filter_UVCut)
			m2 = specrd[i];
		else {
			a1logd(p->log,1," wrong filter 0x%x\n",m->filt);
			if (p2m != NULL) del_zebix_list(p2m, npos);
			free(ewav_sample);
			free(owav_sample);
			free(patch);
			free_dmatrix(wav_sample, 0, nummeasW-1, -9, m->nwav[hr]-1);
			return ev;
		}
	
		if ((ev = i1pro3_comp_refl_value(
			p,
			m0, m1, m2,
			ewav_sample, enummeas,
			owav_sample, onummeas,
			hr)) != I1PRO3_OK) {

			a1logd(p->log,1," conversion to calibrated spectral failed\n");
			if (p2m != NULL) del_zebix_list(p2m, npos);
			free(ewav_sample);
			free(owav_sample);
			free(patch);
			free_dmatrix(wav_sample, 0, nummeasW-1, -9, m->nwav[hr]-1);
			return ev;
		}
	}
	if (p2m != NULL)
		del_zebix_list(p2m, npos);
	free(ewav_sample);
	free(owav_sample);
	free(patch);
	free_dmatrix(wav_sample, 0, nummeasW-1, -9, m->nwav[hr]-1);

	return ev;
}

/* - - - - - - - - - - - - - - - - - - - */

/* Do polarized reflectance calibration */
i1pro3_code i1pro3_pol_refl_cal(
	i1pro3 *p
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	int nummeasB, nummeasW;
	double inttime;
	double **raw_black1, **raw_white;
	double *avg_black, *raw_avg_white;
#ifdef STRICT_IMITATE
	double refillumsum[36];			/* Sum of nn & uv illumination references */
	double nnweight[36];			/* nn ref illum. blend weighting */
	double uvweight[36];			/* uv ref illum. blend weighting */
#endif
	int hr;
	int i;

	a1logd(p->log,3,"i1pro3_pol_refl_cal\n");

	if (m->capabilities & I1PRO3_CAP_HEAD_SENS) {
		i1p3_adapter atype;

		if ((ev = i1pro3_getadaptype(p, &atype)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_getadaptype failed\n");
			return ev;
		}
	
		a1logd(p->log,4," adapter type = %d\n",atype);
	
		/* Are we in correct state for polarized calibration ? */
		if (atype != i1p3_ad_m3cal) {
			a1logd(p->log,1,"Need polarizer and to be on calibration tile\n");
			return I1PRO3_SPOS_POLCAL;
		}
	}

	/* Make sure LEDs have cooled down for 1 second */
	i1pro3_delay_llampoff(p, LED_OFF_TIME);

	/* Correct wl calibration for any temperature change */
	if (m->ee_version >= 1) {
		double temp;

		if ((ev = i1pro3_getboardtemp(p, &temp)) != I1PRO3_OK) {
			error(" i1pro3_getboardtemp failed\n");
			return ev;
		}
		if ((ev = i1pro3_recompute_wav_filters_for_temp(p, temp)) != I1PRO3_OK) {
			error(" i1pro3_recompute_wav_filters_for_temp failed\n");
			return ev;
		}
	}

	/* Check the integration time is suitable for the levels returned in this mode */
	{
		double meastime;
		double *avg_white;
		double maxval;
		int no_minints;

		inttime = m->min_int_time;	
		meastime = 2 * 10 * m->min_int_time;

		/* Read reflective black */
		nummeasB = i1pro3_comp_nummeas(p, meastime, inttime);
		if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black1, &nummeasB, &inttime, NULL, NULL)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_do_measure of black failed\n");
			return ev;
		}

		/* Read reflective white using all LEDs but with pol UV levels */
		nummeasW = i1pro3_comp_nummeas(p, meastime, inttime);
		if ((ev = i1pro3_do_measure(p, i1p3mm_rf_whp, &raw_white, &nummeasW, &inttime, NULL, NULL)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_do_measure of white failed\n");
			i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
			return ev;
		}

		/* Check black */
		if (i1pro3_multimeas_check_black(p, raw_black1, nummeasB, inttime)) {  
			a1logd(p->log,1," black is too bright\n");
			i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
			return I1PRO3_RD_DARKNOTVALID;
		}

		/* Average the black measurements together */
		avg_black = dvector(-1, m->nraw-1);  
		i1pro3_average_rawmmeas(p, avg_black, raw_black1, nummeasB);
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);

		/* Average the white measurements together */
		avg_white = dvector(-1, m->nraw-1);  
		i1pro3_average_rawmmeas(p, avg_white, raw_white, nummeasW);
		i1pro3_free_raw(p, i1p3mm_rf_whp, raw_white, nummeasW);

		/* Subtract black from white */
		vect_sub(avg_white, avg_black, m->nraw);

		/* Highest raw value achieved */
		maxval = vect_max(avg_white, m->nraw);

		a1logd(p->log,5," i1pro3_pol_refl_cal: maxval %f target %f\n",
		                maxval, s->targoscale * m->sens_target);

		free_dvector(avg_white, -1, m->nraw-1);  
		free_dvector(avg_black, -1, m->nraw-1);  

		/* Compute an integration time that achieves targoscale */ 
		inttime = inttime * s->targoscale * m->sens_target/maxval;
		
		/* Round up to even multiple of min_inttime. */
		/* (Polarizing filter reduces sensitivity by about 5.9) */
		no_minints = 2 * (int)ceil(0.5 * inttime/m->min_int_time);
		a1logd(p->log,5," i1pro3_pol_refl_cal: raw no_minints %d\n",no_minints);
		if (no_minints < 2)
			no_minints = 2;
		else if (no_minints > 10)
			no_minints = 10;

		inttime = no_minints * m->min_int_time;
		a1logd(p->log,5," i1pro3_pol_refl_cal: no_minints %d inttime %f\n",no_minints,inttime);

		s->inttime = inttime;
	}

	inttime = s->inttime;	

	/* Read reflective black */
	nummeasB = i1pro3_comp_nummeas(p, s->dcaltime, inttime);
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black1, &nummeasB, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black failed\n");
		return ev;
	}
	a1logd(p->log,4," Got nummeas %d inttime %f from black\n",nummeasB,inttime);

	/* Read reflective white using all LEDs but with pol UV levels */
	nummeasW = i1pro3_comp_nummeas(p, s->wcaltime, inttime);
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_whp, &raw_white, &nummeasW, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of white failed\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return ev;
	}

	a1logd(p->log,4," Got nummeas %d inttim %f from white\n",nummeasW,inttime);

	/* Check black */
	if (i1pro3_multimeas_check_black(p, raw_black1, nummeasB, inttime)) {  
		a1logd(p->log,1," black is too bright\n");
		i1pro3_free_raw(p, i1p3mm_rf_whp, raw_white, nummeasW);
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return I1PRO3_RD_DARKNOTVALID;
	}

	/* Average the black measurements together */
	avg_black = dvector(-1, m->nraw-1);  
	i1pro3_average_rawmmeas(p, avg_black, raw_black1, nummeasB);
	i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);

	/* Subtract black from white */
	i1pro3_multimeas_sub_black(p, raw_white, nummeasW, avg_black);
	free_dvector(avg_black, -1, m->nraw-1);  

	/* Check if white is saturated */
	if (i1pro3_multimeas_check_sat(p, NULL, raw_white, nummeasW)) {
		a1logd(p->log,1," white is saturated\n");
		i1pro3_free_raw(p, i1p3mm_rf_whp, raw_white, nummeasW);
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Check if white is consistent */
	if (i1pro3_multimeas_check_raw_consistency_x(p, raw_white, nummeasW)) {
		a1logd(p->log,1," white is inconsistent\n");
		i1pro3_free_raw(p, i1p3mm_rf_whp, raw_white, nummeasW);
		return I1PRO3_RD_WHITEREADINCONS;
	}

	/* Linearize white */
	i1pro3_multimeas_lin(p, raw_white, nummeasW);

	/* Save average raw white measure for patch recognition reflection calc. */
	raw_avg_white = dvector(-1, m->nraw-1);  
	i1pro3_average_rawmmeas(p, raw_avg_white, raw_white, nummeasW);
	vect_cpy(s->pol_calraw_white, raw_avg_white, m->nraw);
	free_dvector(raw_avg_white, -1, m->nraw-1);  

#ifdef STRICT_IMITATE
	/* Compute reference illumination blend weightings. Weights sum to 1.0, */
	/* and are equal above 450nm, with full weight to the UV region in uvweight */
	/* and zero weight in nnweight. */ 
	vect_add3(refillumsum, m->ee_ref_nn_illum, m->ee_ref_uv_illum, 36);
	vect_div3(nnweight, m->ee_ref_nn_illum, refillumsum, 36);
	vect_div3(uvweight, m->ee_ref_uv_illum, refillumsum, 36);
#endif

//printf("nn and uv blend weightings:\n");
//plot_wav2(m, 0, nnweight, uvweight);

	/* Accumulate average sum of led model illumination */
	vect_set(s->pol_calsp_ledm, 0.0, 36);
	for (i = 0; i < nummeasW; i++) {
		double nn_ledm[36], *pnn_ledm[1] = { nn_ledm };	/* nn led model for this measurement */
		double uv_ledm[36], *puv_ledm[1] = { uv_ledm };	/* uv led model for this measurement */
//double ouv_ledm[36];
		
		/* Compute nn and uv models for the given paux data */
		i1pro3_comp_ledm(p, pnn_ledm, raw_white + i, 1, 0);
		i1pro3_comp_ledm(p, puv_ledm, raw_white + i, 1, 1);
//vect_cpy(ouv_ledm, uv_ledm, 36);

#ifdef STRICT_IMITATE
		/* Scale them by the same reference blending ratio. */
		/* Hmm. So we end up with the average of the models above 450 nm */
		/* and the uv portion of the uv led model. */
		/* This seems pointless because the result is almost */
		/* identical to the uv_ledm, the only slight discrepancy */
		/* being in the blend crossover region 410 - 440nm. It also doesn't */
		/* seem to correspond to the reality that the effective illumination */
		/* is the average of the mux'd nn and uv led spectrum. */
		/* So we SHOULD actually use the mean of the nn and uv models */
		vect_mul(uv_ledm, uvweight, 36); 
		vect_mul(nn_ledm, nnweight, 36); 

		/* This measurements led model */
		vect_add(uv_ledm, nn_ledm, 36);
#else
		/* Use (presumed) correct average of nn & uv as illum cal reference */
		vect_blend(uv_ledm, uv_ledm, nn_ledm, 0.5, 36);
#endif

//printf("Computed this meas led model, uv_ledm\n");
//plot_wav(m, 0, uv_ledm);

		/* Sum to calibration ledm reference */
		vect_add(s->pol_calsp_ledm, uv_ledm, 36);
	}
	/* Complete average ledm calculations */
	vect_scale1(s->pol_calsp_ledm, 1.0/nummeasW, 36);	

//printf(" avg ledm:\n");
//plot_wav(m, 0, s->pol_calsp_ledm);
//dump_dvector_fmt(stdout, "pol_calsp_ledm", " ", s->pol_calsp_ledm, 36, "%g");

	/* Compute white reference in std and hi-res */
	for (hr = 0; hr < 2; hr++) {
		double **wav_white;

		/* Convert from raw to wav */
		wav_white = dmatrix(-9, nummeasW-1, -9, m->nwav[hr]-1);
		i1pro3_absraw_to_abswav(p, hr, 1, wav_white, raw_white, nummeasW);

		/* Apply stray light compensation */
		i1pro3_straylight(p, hr, wav_white, nummeasW);
 
		/* Accumulate average sums of white mesured */
		vect_set(s->pol_calsp_white[hr], 0.0, m->nwav[hr]);

		for (i = 0; i < nummeasW; i++) {
			/* Sum white */
			vect_add(s->pol_calsp_white[hr], wav_white[i], m->nwav[hr]);
		}
		/* Complete average white calculations */
		vect_scale1(s->pol_calsp_white[hr], 1.0/nummeasW, m->nwav[hr]);

//printf(" avg white hr %d:\n",hr);
//plot_wav(m, hr, s->pol_calsp_white[hr]);
//dump_dvector_fmt(stdout, "pol_calsp_white", " ", s->pol_calsp_white[hr], m->nwav[hr], "%g");

		free_dmatrix(wav_white, -9, nummeasW-1, -9, m->nwav[hr]-1);
	}
	i1pro3_free_raw(p, i1p3mm_rf_whp, raw_white, nummeasW);

	return ev; 
}

/* Do a polarized spot reflectance measurement */
i1pro3_code i1pro3_pol_spot_refl_meas(
	i1pro3 *p,
	double **specrd,    /* Cooked spectral patch value to return */
	int hr				/* 1 for high-res. */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	int nummeasB, nummeasW;
	double inttime;
	double **raw_black1, **raw_sample;	/* raw measurements */
	double *raw_avg_black;				/* raw average black */
	double **proc_sample;				/* processed sample values */
	int i;

	a1logd(p->log,3,"i1pro3_pol_spot_refl_meas\n");

	if (m->capabilities & I1PRO3_CAP_HEAD_SENS) {
		i1p3_adapter atype;

		if ((ev = i1pro3_getadaptype(p, &atype)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_getadaptype failed\n");
			return ev;
		}
	
		/* Do we have a polarized filter fitted ? */
		if ((atype & i1p3_ad_m3) == 0) {
			a1logd(p->log,1,"Expect a polarization filter\n");
			return I1PRO3_SPOS_POL;
		}
	}

	/* Make sure LEDs have cooled down for 1 second */
	i1pro3_delay_llampoff(p, LED_OFF_TIME);

	/* Correct wl calibration for any temperature change */
	if (m->ee_version >= 1) {
		double temp;

		if ((ev = i1pro3_getboardtemp(p, &temp)) != I1PRO3_OK) {
			error(" i1pro3_getboardtemp failed\n");
			return ev;
		}
		if ((ev = i1pro3_recompute_wav_filters_for_temp(p, temp)) != I1PRO3_OK) {
			error(" i1pro3_recompute_wav_filters_for_temp failed\n");
			return ev;
		}
	}

	inttime = s->inttime;	

	/* Read reflective black */
	nummeasB = i1pro3_comp_nummeas(p, s->dreadtime, inttime);
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black1, &nummeasB, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black failed\n");
		return ev;
	}

	/* Read reflective sample using all LEDs */
	nummeasW = i1pro3_comp_nummeas(p, s->wreadtime, inttime);
	if ((ev =i1pro3_do_measure(p, i1p3mm_rf_whp, &raw_sample, &nummeasW, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of sample failed\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return ev;
	}

	/* Don't check black for measurement */

	/* Average the black together */
	raw_avg_black = dvector(-1, m->nraw-1);  
	i1pro3_average_rawmmeas(p, raw_avg_black, raw_black1, nummeasB);
	i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);

	/* Subtract black from sample */
	i1pro3_multimeas_sub_black(p, raw_sample, nummeasW, raw_avg_black);
	free_dvector(raw_avg_black, -1, m->nraw-1);  

	/* Check if sample is saturated */
	if (i1pro3_multimeas_check_sat(p, NULL, raw_sample, nummeasW)) {
		a1logd(p->log,1," sample is saturated\n");
		i1pro3_free_raw(p, i1p3mm_rf_whp, raw_sample, nummeasW);
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Check if sample is consistent */
	if (i1pro3_multimeas_check_raw_consistency_x(p, raw_sample, nummeasW)) {
		a1logd(p->log,1," sample is inconsistent\n");
		i1pro3_free_raw(p, i1p3mm_rf_whp, raw_sample, nummeasW);
		return I1PRO3_RD_READINCONS;
	}

	/* Linearize sample */
	i1pro3_multimeas_lin(p, raw_sample, nummeasW);

	/* Convert from raw to wav */
	proc_sample = dmatrix(-9, nummeasW-1, -9, m->nwav[hr]-1);
	i1pro3_absraw_to_abswav(p, hr, 1, proc_sample, raw_sample, nummeasW);
	i1pro3_free_raw(p, i1p3mm_rf_whp, raw_sample, nummeasW);

	/* Convert to calibrated reflectance */
	if ((ev = i1pro3_comp_pol_refl_value(p, specrd[0], proc_sample, nummeasW, hr)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_comp_pol_refl_value failed\n");
		free_dmatrix(proc_sample, -9, nummeasW-1, -9, m->nwav[hr]-1);
		return ev;
	}

	free_dmatrix(proc_sample, -9, nummeasW-1, -9, m->nwav[hr]-1);

	return ev;
}

/* Do a polarized strip reflectance measurement */
i1pro3_code i1pro3_pol_strip_refl_meas(
	i1pro3 *p,
	double **specrd,	/* Cooked spectral patches values to return */
	int nvals,			/* Number of patches expected */
	int hr				/* 1 for hr */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	int nummeasB, nummeasW;
	double inttime;
	double **raw_black1, **raw_sample;	/* raw measurements */
	double **wav_sample;			/* processed wav measurements */
	double **pwav_sample;			/* pointer to a patches wav values */
	int **p2m = NULL;				/* zebra list of sample indexes in mm. order */
	int npos = 0;					/* Number of position slots */
	double **filt_sample;			/* filtered raw measurements */
	int nfiltsamp;					/* Number of filtered raw measurements */
	double *raw_avg_black;			/* raw average black */
	i1pro3_patch *patch;			/* List of patch locations within scan data */
	double *m0 = NULL, *m1 = NULL, *m2 = NULL;
	int i, j, k;

	a1logd(p->log,3,"i1pro3_pol_strip_refl_meas\n");

	if (m->capabilities & I1PRO3_CAP_HEAD_SENS) {
		i1p3_adapter atype;

		if ((ev = i1pro3_getadaptype(p, &atype)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_getadaptype failed\n");
			return ev;
		}
	
		/* Do we have a polarized filter fitted ? */
		if ((atype & i1p3_ad_m3) == 0) {
			a1logd(p->log,1,"Expect a polarization filter\n");
			return I1PRO3_SPOS_POL;
		}
	}

	/* Make sure LEDs have cooled down for 1 second */
	i1pro3_delay_llampoff(p, LED_OFF_TIME);

	inttime = s->inttime;	

	/* Correct wl calibration for any temperature change */
	if (m->ee_version >= 1) {
		double temp;

		if ((ev = i1pro3_getboardtemp(p, &temp)) != I1PRO3_OK) {
			error(" i1pro3_getboardtemp failed\n");
			return ev;
		}
		if ((ev = i1pro3_recompute_wav_filters_for_temp(p, temp)) != I1PRO3_OK) {
			error(" i1pro3_recompute_wav_filters_for_temp failed\n");
			return ev;
		}
	}

	/* Read reflective black */
	nummeasB = i1pro3_comp_nummeas(p, s->dreadtime, inttime);
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_black, &raw_black1, &nummeasB, &inttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of black failed\n");
		return ev;
	}

	/* Read strip reflective sample using all LEDs. Read zebra ruler if possible too. */
	nummeasW = 0;
	if ((ev = i1pro3_do_measure(p, i1p3mm_rf_whp, &raw_sample, &nummeasW, &inttime, &p2m, &npos)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of sample failed\n");
		i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);
		return ev;
	}

	a1logd(p->log,2," i1pro3_do_measure strip returned %d nummeas\n",nummeasW);
	
	/* Don't check black for measurement */

	/* Average the black together */
	raw_avg_black = dvector(-1, m->nraw-1);  
	i1pro3_average_rawmmeas(p, raw_avg_black, raw_black1, nummeasB);
	i1pro3_free_raw(p, i1p3mm_rf_black, raw_black1, nummeasB);

//dump_dvector(stdout, "raw_avg_black", " ", raw_avg_black, 128);
//printf("Black:\n"); plot_raw(raw_avg_black);

	/* Subtract black from samples */
	i1pro3_multimeas_sub_black(p, raw_sample, nummeasW, raw_avg_black);
	free_dvector(raw_avg_black, -1, m->nraw-1);  

	/* Check if samples are saturated */
	if (i1pro3_multimeas_check_sat(p, NULL, raw_sample, nummeasW)) {
		i1pro3_free_raw(p, i1p3mm_rf_whp, raw_sample, nummeasW);
		a1logd(p->log,1," sample is saturated\n");
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Linearize samples */
	i1pro3_multimeas_lin(p, raw_sample, nummeasW);

	/* Create copy of samples for patch recognition */
	nfiltsamp = nummeasW;
	filt_sample = dmatrix(0, nfiltsamp-1, -9, m->nraw-1);
	copy_dmatrix(filt_sample, raw_sample, 0, nummeasW-1, -9, m->nraw-1);

	/* Spectrally normalize recognition samples */
	for (i = 0; i < nummeasW; i++)
		vect_div3(filt_sample[i], raw_sample[i], s->pol_calraw_white, m->nraw);

#undef PLOT_FILLINS

	/* Convert time based patch recognition proxy into position based one */
	if (p2m != NULL) {
		double **pos_sample;			/* Position based raw samples */
#ifdef PLOT_FILLINS
		double **plot = dmatrixz(0, 2, 0, npos-1);  
#endif

		pos_sample = dmatrix(0, npos-1, 0, m->nraw-1);

		/* Average samples values into position slots */
		for (i = 0; i < npos; i++) {
			vect_set(pos_sample[i], 0.0, m->nraw);

			if (p2m[i][-1] > 0) {
				for (j = 0; j < p2m[i][-1]; j++)
					vect_add(pos_sample[i], filt_sample[p2m[i][j]], m->nraw);
				vect_scale1(pos_sample[i], 1.0/(double)j, m->nraw);
			}
		}

#ifdef PLOT_FILLINS
		for (i = 0; i < npos; i++) {
			plot[0][i] = (double)i;
			plot[1][i] = pos_sample[i][50];
		}
#endif

		/* Now interpolate any empty slots */
		for (i = 0; i < npos; i++) {
			double bf;

			if (p2m[i][-1] != 0)
				continue;

			for (j = i-1; j >= 0; j--)		/* Search for lower non-empty */
				if (p2m[j][-1] != 0)
					break;

			for (k = i+1; k < npos; k++)		/* Search for upper non-empty */
				if (p2m[k][-1] != 0)
					break;

			if (j < 0) {
				j = k;
				bf = 0.5;
			} else if (k >= npos) {
				k = j;
				bf = 0.5;
			} else {
				bf = (i - j) / (double)(k - j);
			}
			vect_blend(pos_sample[i], pos_sample[j], pos_sample[k], bf, m->nraw);
		}

#ifdef PLOT_FILLINS
		for (i = 0; i < npos; i++) 
			plot[2][i] = pos_sample[i][50];
		printf("Fill-ins on band 50:\n");
		do_plot(plot[0], plot[1], plot[2], NULL, npos);
#endif

		/* Switch position proxy for time one */
		free_dmatrix(filt_sample, 0, nfiltsamp-1, -9, m->nraw-1);
		filt_sample = pos_sample;
		nfiltsamp = npos;
	}

	/* Room for returned patch information */
	if ((patch = malloc(sizeof(i1pro3_patch) * nvals)) == NULL) {
		if (p2m != NULL) del_zebix_list(p2m, npos);
		free_dmatrix(filt_sample, 0, nfiltsamp-1, -9, m->nraw-1);
		i1pro3_free_raw(p, i1p3mm_rf_whp, raw_sample, nummeasW); raw_sample = NULL;
		a1logd(p->log,1,"i1pro3_pol_strip_refl_meas malloc %ld bytes failed\n",sizeof(i1pro3_patch) * nvals);
		return I1PRO3_INT_MALLOC;
	}
 
	/* Locate patch boundaries */
	if ((ev = i1pro3_locate_patches(p, patch, nvals, filt_sample, nfiltsamp, p2m)) != I1PRO3_OK) {
		if (p2m != NULL) del_zebix_list(p2m, npos);
		free(patch);
		free_dmatrix(filt_sample, 0, nfiltsamp-1, -9, m->nraw-1);
		i1pro3_free_raw(p, i1p3mm_rf_whp, raw_sample, nummeasW); raw_sample = NULL;
		a1logd(p->log,1," i1pro3_do_measure of sample failed\n");
		return ev;
	}

#ifdef PATREC_DEBUG 
	for (i = 0; i < nvals; i++) {
		int nosamp = patch[i].no;

		if (p2m != NULL) {		/* Count underlying samples in patch */
			for (nosamp = k = 0; k < patch[i].no; k++)
				nosamp += p2m[patch[i].ss + k][-1];
		}
	
		printf("patch %d: @ %d len %d (%d)\n",i,patch[i].ss,patch[i].no,nosamp);
	}
#endif // NEVER

	free_dmatrix(filt_sample, 0, nfiltsamp-1, -9, m->nraw-1);

	/* Allocate array of pointers to samples of max possible length */
	if ((pwav_sample = malloc(sizeof(double *) * nummeasW)) == NULL) {
		if (p2m != NULL) del_zebix_list(p2m, npos);
		free(patch);
		i1pro3_free_raw(p, i1p3mm_rf_whp, raw_sample, nummeasW); raw_sample = NULL;
		a1logd(p->log,1,"i1pro3_pol_strip_refl_meas malloc %ld bytes failed\n",sizeof(double *) * (nummeasW/2+1));
		return I1PRO3_INT_MALLOC;
	}

	/* Convert samples from raw to wav */
	wav_sample = dmatrix(0, nummeasW-1, -9, m->nwav[hr]-1);
	i1pro3_absraw_to_abswav(p, hr, 1, wav_sample, raw_sample, nummeasW);
	i1pro3_free_raw(p, i1p3mm_rf_whp, raw_sample, nummeasW); raw_sample = NULL;

	/* Process each patch into spectral values */
	for (i = 0; i < nvals; i++) {
		int nummeas;			/* Number of samples */

		/* Split into odd and even wav samples */
		if (p2m != NULL) {

			/* Copy list of patches in p2m list */
			for (nummeas = j = 0; j < patch[i].no; j++) {
				int *plist = p2m[patch[i].ss + j];	/* List of patches in this slot */

				for (k = 0; k < plist[-1]; k++, nummeas++)
					pwav_sample[nummeas] = wav_sample[plist[k]];
			}

		} else {
			/* Copy samples pointers to wav values */
			for (nummeas = j = 0; j < patch[i].no; j++, nummeas++)
				pwav_sample[nummeas] = wav_sample[j];
		}

		/* Check if each patch is consistent */
		if (i1pro3_multimeas_check_wav_consistency(p, hr, inttime, pwav_sample, nummeas)) {
			if (p2m != NULL) del_zebix_list(p2m, npos);
			free(pwav_sample);
			free(patch);
			free_dmatrix(wav_sample, 0, nummeasW-1, -9, m->nwav[hr]-1);
			a1logd(p->log,1," sample is inconsistent\n");
			return I1PRO3_RD_READINCONS;
		}

		/* Convert raw samples to patch reflective spectral values.*/
		if ((ev = i1pro3_comp_pol_refl_value(p, specrd[i], pwav_sample, nummeas, hr)) != I1PRO3_OK) {
			a1logd(p->log,1," conversion to calibrated spectral failed\n");
			if (p2m != NULL) del_zebix_list(p2m, npos);
			free(pwav_sample);
			free(patch);
			free_dmatrix(wav_sample, 0, nummeasW-1, -9, m->nwav[hr]-1);
			return ev;
		}
	}
	if (p2m != NULL)
		del_zebix_list(p2m, npos);
	free(pwav_sample);
	free(patch);
	free_dmatrix(wav_sample, 0, nummeasW-1, -9, m->nwav[hr]-1);

	return ev;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Do an adaptive emissive/ambient/transmissive calibration */
i1pro3_code i1pro3_adapt_emis_cal(
	i1pro3 *p,
	double *btemp			/* Return the board temperature */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	double inttime1, inttime2;
	int nummeas1, nummeas2;
	double **raw_black1, **raw_black2, **raw_black3;		/* raw measurements */
	double *avg_black1, *avg_black2;		/* raw average black */

	a1logd(p->log,3,"i1pro3_adapt_emis_cal\n");

	if (m->capabilities & I1PRO3_CAP_HEAD_SENS) {
		i1p3_adapter atype;

		if ((ev = i1pro3_getadaptype(p, &atype)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_getadaptype failed\n");
			return ev;
		}
	
		a1logd(p->log,4," adapter type = %d\n",atype);
	
		/* Are we in correct state for calibration ? */
		if (atype != i1p3_ad_cal) {
			a1logd(p->log,1,"Need to be on calibration tile\n");
			return I1PRO3_SPOS_CAL;
		}
	}

	if ((ev = i1pro3_getboardtemp(p, btemp)) != I1PRO3_OK) {
		error(" i1pro3_getboardtemp failed\n");
		return ev;
	}

	/* Correct wl calibration for any temperature change */
	if (m->ee_version >= 1) {
		if ((ev = i1pro3_recompute_wav_filters_for_temp(p, *btemp)) != I1PRO3_OK) {
			error(" i1pro3_recompute_wav_filters_for_temp failed\n");
			return ev;
		}
	}

	/* Read short reflective black */
	{
		inttime1 = s->idark_int_time[0];
		nummeas1 = i1pro3_comp_nummeas(p, s->dcaltime, inttime1);
	}
	a1logd(p->log,2,"\nDoing adaptive interpolated black calibration, nummeas %d of int_time %f\n", nummeas1, s->idark_int_time[0]);
	if ((ev = i1pro3_do_measure(p, i1p3mm_em, &raw_black1, &nummeas1, &inttime1, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure of short adaptive black failed\n");
		return ev;
	}

	/* Read long reflective black */
	{
		inttime2 = s->idark_int_time[1];
		nummeas2 = i1pro3_comp_nummeas(p, s->dlcaltime, inttime2);
	}
	a1logd(p->log,2,"\nDoing adaptive interpolated black calibration %d of int_time %f\n", nummeas2, s->idark_int_time[1]);
	if ((ev = i1pro3_do_measure(p, i1p3mm_em, &raw_black2, &nummeas2, &inttime2, NULL, NULL)) != I1PRO3_OK) {
		i1pro3_free_raw(p, i1p3mm_em, raw_black1, nummeas1);
		a1logd(p->log,1," i1pro3_do_measure of long adaptive black failed\n");
		return ev;
	}

	/* Read short reflective black again */
	{
		a1logd(p->log,2,"\nDoing adaptive interpolated black calibration, nummeas %d of int_time %f\n", nummeas1, s->idark_int_time[0]);
		if ((ev = i1pro3_do_measure(p, i1p3mm_em, &raw_black3, &nummeas1, &inttime1, NULL, NULL)) != I1PRO3_OK) {
			i1pro3_free_raw(p, i1p3mm_em, raw_black1, nummeas1);
			i1pro3_free_raw(p, i1p3mm_em, raw_black2, nummeas2);
			a1logd(p->log,1," i1pro3_do_measure of short adaptive 2 black failed\n");
			return ev;
		}
	}

	/* Check black */
	if (i1pro3_multimeas_check_black(p, raw_black1, nummeas1, inttime1)
	 || i1pro3_multimeas_check_black(p, raw_black2, nummeas2, inttime2)
	 || i1pro3_multimeas_check_black(p, raw_black3, nummeas1, inttime1)) {  
		a1logd(p->log,1," black is too bright\n");
		i1pro3_free_raw(p, i1p3mm_em, raw_black1, nummeas1);
		i1pro3_free_raw(p, i1p3mm_em, raw_black2, nummeas2);
		if (raw_black1 != raw_black3) i1pro3_free_raw(p, i1p3mm_em, raw_black3, nummeas1);
		return I1PRO3_RD_DARKNOTVALID;
	}

	/* Average the black measurements together */
	avg_black1 = dvector(-1, m->nraw-1);
	avg_black2 = dvector(-1, m->nraw-1);
	i1pro3_average_rawmmeas_2(p, avg_black1, raw_black1, nummeas1, raw_black3, nummeas1);
	i1pro3_average_rawmmeas(p, avg_black2, raw_black2, nummeas2);
	i1pro3_free_raw(p, i1p3mm_em, raw_black1, nummeas1);
	i1pro3_free_raw(p, i1p3mm_em, raw_black2, nummeas2);
	if (raw_black1 != raw_black3) i1pro3_free_raw(p, i1p3mm_em, raw_black3, nummeas1);

	/* Compute black ref dynamic value */
	vect_sub3(s->idark_data[1]-1, avg_black2-1, avg_black1-1, m->nraw+1);
	vect_scale1(s->idark_data[1]-1, 1.0/(inttime2 - inttime1), m->nraw+1);

	/* Compute black ref static value */
	vect_scale(s->idark_data[0]-1, s->idark_data[1]-1, inttime1, m->nraw+1);
	vect_sub3(s->idark_data[0]-1, avg_black1-1, s->idark_data[0]-1, m->nraw+1);

	free_dvector(avg_black1, -1, m->nraw-1);  
	free_dvector(avg_black2, -1, m->nraw-1);  

	return ev;
}

/* Do a tranmissive white calibration */
/* Return I1PRO3_CAL_TRANSWHITEWARN if any of the transmission wavelengths are low. */
i1pro3_code i1pro3_trans_cal(
	i1pro3 *p
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	double btemp;
	double **raw_white;
	double *raw_avg_white;
	int nummeas;
	int hr;

	a1logd(p->log,3,"i1pro3_trans_cal\n");

	if (m->capabilities & I1PRO3_CAP_HEAD_SENS) {
		i1p3_adapter atype;

		if ((ev = i1pro3_getadaptype(p, &atype)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_getadaptype failed\n");
			return ev;
		}
	
		a1logd(p->log,4," adapter type = %d\n",atype);
	
		/* Are we in correct state for calibration ? */
		if ((atype & i1p3_ad_standard) == 0) {
			a1logd(p->log,1,"Expect a standard measurement adapter\n");
			return I1PRO3_SPOS_CAL;
		}
	}

	if ((ev = i1pro3_getboardtemp(p, &btemp)) != I1PRO3_OK) {
		error(" i1pro3_getboardtemp failed\n");
		return ev;
	}

	/* Correct wl calibration for any temperature change */
	if (m->ee_version >= 1) {
		if ((ev = i1pro3_recompute_wav_filters_for_temp(p, btemp)) != I1PRO3_OK) {
			error(" i1pro3_recompute_wav_filters_for_temp failed\n");
			return ev;
		}
	}

	/* Do all the hard work here... */
	if ((ev = i1pro3_spot_adapt_emis_raw_meas(p, &raw_white, &nummeas)) != I1PRO3_OK) { 
		return ev;
	}

	/* Save raw white for pattern recognition normalization */
	raw_avg_white = dvector(-1, m->nraw-1);  
	i1pro3_average_rawmmeas(p, raw_avg_white, raw_white, nummeas);
	vect_cpy(s->raw_white, raw_avg_white, m->nraw);
	free_dvector(raw_avg_white, -1, m->nraw-1);  

	/* Do std res. and high res. */
	for (hr = 0; hr < 2; hr++) {
		double avgwh;
		double **proc_sample;
		double wav[MX_NWAV];
		int j;

		/* Convert from raw to wav, apply straylight and average */
		proc_sample = dmatrix(0, nummeas-1, -1, m->nwav[hr]-1);
		i1pro3_absraw_to_abswav(p, hr, 0, proc_sample, raw_white, nummeas);
		i1pro3_straylight(p, hr, proc_sample, nummeas);
		i1pro3_average_wavmmeas(p, wav, proc_sample, nummeas, hr);

		avgwh = vect_avg(wav, m->nwav[hr]);

		if (avgwh < 5000.0) {
			free_dmatrix(proc_sample, 0, nummeas-1, -1, m->nwav[hr]-1);
			i1pro3_free_raw(p, i1p3mm_em, raw_white, nummeas);
			return I1PRO3_RD_TRANSWHITELEVEL;
		}

		/* Check white and safely invert */
		for (j = 0; j < m->nwav[hr]; j++) {
			/* If reference is < 0.4% of average */
			if (wav[j]/avgwh < 0.004) {
				s->cal_factor[hr][j] = 1.0/(0.004 * avgwh);
				ev = I1PRO3_CAL_TRANSWHITEWARN;
			} else {
				s->cal_factor[hr][j] = 1.0/wav[j];	
			}
		}
//if (ev == I1PRO3_CAL_TRANSWHITEWARN) plot_wav(m, hr, wav);

		free_dmatrix(proc_sample, 0, nummeas-1, -1, m->nwav[hr]-1);
	}

	i1pro3_free_raw(p, i1p3mm_em, raw_white, nummeas);

	return ev;
}


/* Do an adaptive spot emission/ambient/transmissive measurement */
i1pro3_code i1pro3_spot_adapt_emis_meas(
	i1pro3 *p,
	double **specrd    /* Cooked spectral patch value to return */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	int nummeas;
	double **raw_sample;	/* raw measurements */
	double **proc_sample;	/* processed sample values */
	int hr = m->highres;	/* High res. */
	int i;

	if (m->capabilities & I1PRO3_CAP_HEAD_SENS) {
		i1p3_adapter atype;

		if ((ev = i1pro3_getadaptype(p, &atype)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_getadaptype failed\n");
			return ev;
		}
	
		if (s->ambient) {
			/* Do we have ambient adapter fitted ? */
			if (atype != i1p3_ad_ambient) {
				a1logd(p->log,1,"Expect ambient measurement adapter\n");
				return I1PRO3_SPOS_AMB;
			}
		} else {	/* Emision or Transmission */
			/* Do we have normal adapter fitted ? */
			if ((atype & i1p3_ad_standard) == 0) {
				a1logd(p->log,1,"Expect an standard measurement adapter\n");
				return I1PRO3_SPOS_STD;
			}
		}
	}

	/* Do all the hard work here, so that it is reusable. */
	if ((ev = i1pro3_spot_adapt_emis_raw_meas(p, &raw_sample, &nummeas)) != I1PRO3_OK) { 
		return ev;
	}

	/* Convert from raw to wav, apply straylight and average */
	proc_sample = dmatrix(0, nummeas-1, -1, m->nwav[hr]-1);
	i1pro3_absraw_to_abswav(p, hr, 0, proc_sample, raw_sample, nummeas);
	i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeas);
	i1pro3_straylight(p, hr, proc_sample, nummeas);
	i1pro3_average_wavmmeas(p, specrd[0], proc_sample, nummeas, hr);

	/* Scale by calibration factor */
	vect_mul(specrd[0], s->cal_factor[hr], m->nwav[hr]);

	free_dmatrix(proc_sample, 0, nummeas-1, -1, m->nwav[hr]-1);

	return ev;
}

/* Do scan emission/ambient/transmissive measurement */
/* Emissive scan is assumed to be a patch strip. */
/* Transmissive scan is assumed to be a patch strip. */
/* Ambient scan is assumed to be a flash. */
i1pro3_code i1pro3_scan_emis_meas(
	i1pro3 *p,
	double *duration,	/* If flash, return duration */
	double **specrd,    /* Cooked spectral patch values to return */
	int nvals			/* Number of patches expected */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	double btemp;
	double binttime, inttime;
	int nummeasB, nummeasW;
	double **raw_black1, **raw_black2, **raw_sample;	/* raw measurements */
	double black[MX_NRAW], _psample[MX_NRAW+1], *psample = _psample + 1;
	double maxval;
	int **p2m = NULL;		/* zebra list of sample indexes in mm. order */
	int npos = 0;			/* Number of position slots */
	int hr = m->highres;	/* High res. */
	int i;

	if (m->capabilities & I1PRO3_CAP_HEAD_SENS) {
		i1p3_adapter atype;

		if ((ev = i1pro3_getadaptype(p, &atype)) != I1PRO3_OK) {
			a1logd(p->log,1," i1pro3_getadaptype failed\n");
			return ev;
		}
	
		if (s->ambient) {
			/* Do we have ambient adapter fitted ? */
			if (atype != i1p3_ad_ambient) {
				a1logd(p->log,1,"Expect ambient measurement adapter\n");
				return I1PRO3_SPOS_AMB;
			}
		} else {	/* Emision or Transmission */
			/* Do we have normal adapter fitted ? */
			if ((atype & i1p3_ad_standard) == 0) {
				a1logd(p->log,1,"Expect an standard measurement adapter\n");
				return I1PRO3_SPOS_STD;
			}
		}
	}

	if ((ev = i1pro3_getboardtemp(p, &btemp)) != I1PRO3_OK) {
		error(" i1pro3_getboardtemp failed\n");
		return ev;
	}

	/* Correct wl calibration for any temperature change */
	if (m->ee_version >= 1) {
		if ((ev = i1pro3_recompute_wav_filters_for_temp(p, btemp)) != I1PRO3_OK) {
			error(" i1pro3_recompute_wav_filters_for_temp failed\n");
			return ev;
		}
	}

	/* Read short int time, and check/adjust measurement integration time */
	binttime = m->min_int_time;
	nummeasB = i1pro3_comp_nummeas(p, s->dreadtime, binttime);
	if ((ev = i1pro3_do_measure(p, i1p3mm_em, &raw_black1, &nummeasB, &binttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure failed\n");
		return ev;
	}

	/* If we are assuming a strip, then we should be starting over the media */
	/* which should be white, so use this to adjust the integration time a little */
	if (!s->flash) {
		i1pro3_average_rawmmeas(p, psample, raw_black1, nummeasB);
		i1pro3_comp_simple_emis_black(p, black, raw_black1, nummeasB, binttime); 
		vect_sub(psample, black, m->nraw);	/* Subtract black from psample */
		i1pro3_vect_lin(p, psample);		/* Linearize */
		maxval = vect_max(psample, m->nraw);

		a1logd(p->log,4," emis/trans short meas maxval %f\n",maxval);

		if (maxval > m->sens_sat) {
			a1logd(p->log,1," sample is saturated\n");
			i1pro3_free_raw(p, i1p3mm_em, raw_black1, nummeasB);
			return I1PRO3_RD_SENSORSATURATED;
		} else {
			double ainttime;

			if (maxval <= 0.0)
				maxval = 1.0;
			ainttime = binttime * s->targoscale * m->sens_target/maxval;
			if (ainttime < m->min_int_time)
				ainttime = m->min_int_time;

			/* Only lower integration time */
			if (ainttime < s->inttime) {
				s->inttime = ainttime;
				a1logd(p->log,3," adjusted trans/emis scan inttime to %f\n",s->inttime);
			}
		}
	}

	inttime = s->inttime;

	/* Main measure */
	nummeasW = 0;
	a1logd(p->log,2,"\nDoing emissive scan measure int_time %f\n", inttime);

	if ((ev = i1pro3_do_measure(p, i1p3mm_em, &raw_sample, &nummeasW, &inttime, &p2m, &npos)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure failed\n");
		i1pro3_free_raw(p, i1p3mm_em, raw_black1, nummeasB);
		return ev;
	}

	/* Read 2nd short int time */
	if ((ev = i1pro3_do_measure(p, i1p3mm_em, &raw_black2, &nummeasB, &binttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure failed\n");
		if (p2m != NULL) del_zebix_list(p2m, npos);
		i1pro3_free_raw(p, i1p3mm_em, raw_black1, nummeasB);
		i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeasW);
		return ev;
	}

	/* Construct our black */
	i1pro3_comp_emis_black(p, black, raw_black1, raw_black2, nummeasB, binttime,
	                       raw_sample, nummeasW, inttime, btemp);

	i1pro3_free_raw(p, i1p3mm_em, raw_black1, nummeasB);
	i1pro3_free_raw(p, i1p3mm_em, raw_black2, nummeasB);

//printf("~1 raw black, sample:\n");
//i1pro3_average_rawmmeas(p, psample, raw_sample, nummeasW);
//plot_raw2(black, psample);

	/* Subtract black from sample */
	i1pro3_multimeas_sub_black(p, raw_sample, nummeasW, black);

	/* Check if sample is saturated */
	if (i1pro3_multimeas_check_sat(p, NULL, raw_sample, nummeasW)) {
		a1logd(p->log,1," sample is saturated\n");
		if (p2m != NULL) del_zebix_list(p2m, npos);
		i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeasW);
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Linearize and int time normalize sample */
	i1pro3_multimeas_lin(p, raw_sample, nummeasW);
	i1pro3_normalize_rawmmeas(p, raw_sample, nummeasW, inttime);

	/* If not flash, deal wth patch recognition */
	if (!s->flash) {
		double **filt_sample;	/* spetral normalize raw measurements */
		int nfiltsamp;			/* Number of normalized raw measurements */
		double *norm;			/* Normalizing spectrum */
		i1pro3_patch *patch;	/* List of patch locations within scan data */
		double **wav_sample;	/* processed wav measurements */
		double **pwav_sample;	/* pointer to a patches wav values */

		/* If not transmission, construct a normalizing white */
		if (!s->trans) {
			vect_set(psample, 0.0, m->nraw);
		
			for (i = 0; i < nummeasW; i++)
				vect_max_elem(psample, raw_sample[i], m->nraw);

			norm = psample;
		} else {
			norm = s->raw_white;	/* Transmission white raw reference */
		}

		nfiltsamp = nummeasW;
		filt_sample = dmatrix(0, nfiltsamp-1, -9, m->nraw-1);

		/* Spectrally normalize recognition samples */
		for (i = 0; i < nummeasW; i++)
			vect_div3(filt_sample[i], raw_sample[i], norm, m->nraw);

		/* Convert time based patch recognition proxy into position based one */
		if (p2m != NULL) {
			double **pos_sample;			/* Position based raw samples */
			int j, k;

			pos_sample = dmatrix(0, npos-1, 0, m->nraw-1);

			/* Average samples values into position slots */
			for (i = 0; i < npos; i++) {
				vect_set(pos_sample[i], 0.0, m->nraw);

				if (p2m[i][-1] > 0) {
					for (j = 0; j < p2m[i][-1]; j++)
						vect_add(pos_sample[i], filt_sample[p2m[i][j]], m->nraw);
					vect_scale1(pos_sample[i], 1.0/(double)j, m->nraw);
				}
			}

			/* Now interpolate any empty slots */
			for (i = 0; i < npos; i++) {
				double bf;

				if (p2m[i][-1] != 0)
					continue;

				for (j = i-1; j >= 0; j--)		/* Search for lower non-empty */
					if (p2m[j][-1] != 0)
						break;

				for (k = i+1; k < npos; k++)		/* Search for upper non-empty */
					if (p2m[k][-1] != 0)
						break;

				if (j < 0) {
					j = k;
					bf = 0.5;
				} else if (k >= npos) {
					k = j;
					bf = 0.5;
				} else {
					bf = (i - j) / (double)(k - j);
				}
				vect_blend(pos_sample[i], pos_sample[j], pos_sample[k], bf, m->nraw);
			}

			/* Switch position proxy for time one */
			free_dmatrix(filt_sample, 0, nfiltsamp-1, -9, m->nraw-1);
			filt_sample = pos_sample;
			nfiltsamp = npos;
		}

		/* Room for returned patch information */
		if ((patch = malloc(sizeof(i1pro3_patch) * nvals)) == NULL) {
			if (p2m != NULL) del_zebix_list(p2m, npos);
			free_dmatrix(filt_sample, 0, nfiltsamp-1, -9, m->nraw-1);
			i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeasW); raw_sample = NULL;
			a1logd(p->log,1,"i1pro3_scan_emis_meas malloc %ld bytes failed\n",sizeof(i1pro3_patch) * nvals);
			return I1PRO3_INT_MALLOC;
		}
 
		/* Locate patch boundaries */
		if ((ev = i1pro3_locate_patches(p, patch, nvals, filt_sample, nfiltsamp, p2m)) != I1PRO3_OK) {
			if (p2m != NULL) del_zebix_list(p2m, npos);
			free(patch);
			free_dmatrix(filt_sample, 0, nfiltsamp-1, -9, m->nraw-1);
			i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeasW); raw_sample = NULL;
			a1logd(p->log,1," i1pro3_scan_emis_meas patch recognition failed\n");
			return ev;
		}

		free_dmatrix(filt_sample, 0, nfiltsamp-1, -9, m->nraw-1);

		/* Allocate array of pointers to samples of max possible length */
		if ((pwav_sample = malloc(sizeof(double *) * nummeasW)) == NULL) {
			if (p2m != NULL) del_zebix_list(p2m, npos);
			free(patch);
			i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeasW); raw_sample = NULL;
			a1logd(p->log,1,"i1pro3_scan_emis_meas malloc %ld bytes failed\n",sizeof(double *) * (nummeasW/2+1));
			return I1PRO3_INT_MALLOC;
		}

		/* Convert all samples from raw to wav */
		wav_sample = dmatrix(0, nummeasW-1, -9, m->nwav[hr]-1);
		i1pro3_absraw_to_abswav(p, hr, 0, wav_sample, raw_sample, nummeasW);
		i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeasW); raw_sample = NULL;
		i1pro3_straylight(p, hr, wav_sample, nummeasW);

		/* Process each patch into spectral values */
		for (i = 0; i < nvals; i++) {
			int j, nummeas;

			if (p2m != NULL) {
				int k, ix;

				/* Copy list of patches in p2m list */
				for (ix = j = 0; j < patch[i].no; j++) {
					int *plist = p2m[patch[i].ss + j];	/* List of patches in this slot */

					for (k = 0; k < plist[-1]; k++) {
						pwav_sample[ix++] = wav_sample[plist[k]];
					}
				}
				nummeas = ix;

			} else {
				/* Copy samples pointers to wav values */
				for (j = 0; j < patch[i].no; j++)
					pwav_sample[j] = wav_sample[patch[i].ss + j];
				nummeas = patch[i].no;
			}

			/* Check if each patch is consistent */
			if (i1pro3_multimeas_check_wav_consistency(p, hr, inttime, pwav_sample, nummeas)) {
				if (p2m != NULL) del_zebix_list(p2m, npos);
				free(pwav_sample);
				free(patch);
				free_dmatrix(wav_sample, 0, nummeasW-1, -9, m->nwav[hr]-1);
				a1logd(p->log,1," sample is inconsistent\n");
				return I1PRO3_RD_READINCONS;
			}

			/* Average samples into patch value */
			i1pro3_average_wavmmeas(p, specrd[i], pwav_sample, nummeas, hr);

			/* Scale by calibration factor */
			vect_mul(specrd[i], s->cal_factor[hr], m->nwav[hr]);
		}
		if (p2m != NULL)
			del_zebix_list(p2m, npos);
		free(pwav_sample);
		free(patch);
		free_dmatrix(wav_sample, 0, nummeasW-1, -9, m->nwav[hr]-1);

	/* Flash recognition */
	} else {
		double raw_avg[MX_NRAW], *praw_avg[1] = { raw_avg };
		double **wav_sample;	/* processed wav measurements */

		if ((ev = i1pro3_extract_patches_flash(p, duration, raw_avg, raw_sample, nummeasW, inttime)) != I1PRO3_OK) {
			i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeasW); raw_sample = NULL;
			a1logd(p->log,1,"i1pro3_scan_emis_meas extract_patches_flash failed\n");
			return ev;
		} 
		i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeasW);

		/* Convert from raw to wav, apply straylight */
		wav_sample = dmatrix(0, 0, -1, m->nwav[hr]-1);
		i1pro3_absraw_to_abswav(p, hr, 0, wav_sample, praw_avg, 1);
		i1pro3_straylight(p, hr, wav_sample, 1);
	
		/* Scale by calibration factor */
		vect_mul(wav_sample[0], s->cal_factor[hr], m->nwav[hr]);
		vect_cpy(specrd[0], wav_sample[0], m->nwav[hr]);
		free_dmatrix(wav_sample, 0, 0, -1, m->nwav[hr]-1);
	}

	return ev;
}

/* =======================================================================*/
/* Medium level support functions */
/* =======================================================================*/

/* Number of window size tries to use */
#define WIN_TRIES 20

/* Range of raw bands to detect transitions */
#define BL 20	/* Start */
#define BH 101	/* End+1 */
#define NFB 7	/* [7] Number of filtered bands (must be odd and < 10) */		

/* Locate the required number of ref/emis/trans patch locations, */
/* and return a list of the patch boundaries */
i1pro3_code i1pro3_locate_patches(
	i1pro3 *p,
	i1pro3_patch *patches,	/* Return patches[tnpatch] patch locations */
	int tnpatch,			/* Target number of patches to recognise */
	double **rawmeas,		/* Array of [nummeas][nraw] value to locate in */
	int nummeas,			/* number of raw samples */
	int **p2m				/* If not NULL, contains nummeas zebra ruler patch indexes */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	int i, j, k, pix;
	double *maxval;				/* Maximum input value for each wavelength */
	int fbands[NFB][2];			/* Start & end+1 raw indexes of filter bands */
	double **fraw;				/* NFB filtered raw bands */
	double *slope;				/* Accumulated absolute difference between i and i+1 */
	double *fslope;				/* Filtered slope */
	i1pro3_patch *pat;			/* Possible patch information */
	int npat, apat = 0, fpat;	/* Number of potential, number allocated, number found */
	double fmaxslope = 0.0;
	double maxslope = 0.0;
	double minslope =  1e38;
	double thresh = 0.4;		/* Slope threshold */
	int minsamples = s->pol ? POL_MIN_SAMPLES : MIN_SAMPLES; 
	int try;					/* Thresholding try */
	double avglength;			/* Average length of patches */
	double maxlength;			/* Max length of of patches */
	int *sizepop;				/* Size popularity of potential patches */
	int msthr;					/* Median search threshold */
	double median;				/* median potential patch width */
	double window;				/* +/- around median to accept */
	double highest = -1e6;
	double white_avg;			/* Average of (aproximate) white data */
	i1pro3_code rv = I1PRO3_OK;
#ifdef PATREC_DEBUG
	double *pplot[10];
	double **plot;
#endif

	a1logd(p->log,2,"i1pro3_locate_patches looking for %d patches out of %d samples\n",tnpatch,nummeas);

	maxval = dvectorz(0, m->nraw-1);  

	/* Discover the maximum input value for normalisation */
	for (j = 0; j < m->nraw; j++) {
		for (i = 0; i < nummeas; i++) {
			if (rawmeas[i][j]
			                   > maxval[j])
				maxval[j] = rawmeas[i][j];
		}
		if (maxval[j] < 1.0)
			maxval[j] = 1.0;
	}

#ifdef PATREC_DEBUG
	plot = dmatrixz(0, 10, 0, nummeas-1);			/* Up to 11 values */  
#ifdef PATREC_PLOT_ALLBANDS
	for (j = 0; j < (m->nraw-10); j += 10) 			/* Plot all the bands */
#else
	for (j = 24; j < (111-10); j += 30) 			/* Do some of the bands */
#endif
	{
		for (k = 0; k < 10; k ++) {
			if (j + k >= m->nraw) {
				pplot[k] = NULL;
				continue;
			}
			for (i = 0; i < nummeas; i++)
				plot[k][i] = rawmeas[i][j+k]/maxval[j+k];
			pplot[k] = plot[k];
		}
		for (i = 0; i < nummeas; i++)
			plot[10][i] = (double)i;
		printf("Raw Bands %d - %d\n",j,j+9);
		do_plot10(plot[10], pplot[0], pplot[1], pplot[2], pplot[3], pplot[4], pplot[5], pplot[6], pplot[7], pplot[8], pplot[9], nummeas, 0);
	}
#endif	/* PATREC_DEBUG */

	/* Compute the range of each filter band */
	{
		double fbwidth;				/* Number of raw in each band */
		double st;

		fbwidth = (BH - BL)/(double)(NFB/2+1);

		for (st = BL, i = 0; i < NFB; i++) {
			fbands[i][0] = (int)(floor(st)); 
			fbands[i][1] = (int)(floor(st + fbwidth));
			st += 0.5 * fbwidth;
		}
//printf("fbwidth = %f\n",fbwidth);
//for (i = 0; i < NFB; i++) printf("~1 band %d is %d - %d\n",i,fbands[i][0],fbands[i][1]);
	}

	fraw = dmatrixz(0, nummeas-1, 0, NFB-1); 

	/* Weighted box average raw to create filtered bands */
	for (i = 0; i < nummeas; i++) {
		for (j = 0; j < NFB; j++) {
			fraw[i][j] = 0.0;
			for (k = fbands[j][0]; k < fbands[j][1]; k++) {
				fraw[i][j] += rawmeas[i][k]/maxval[k];
			}
			fraw[i][j] /= (double)(fbands[j][1] - fbands[j][0]);
		}
	}

	slope = dvectorz(0, nummeas-1);  

	/* Compute sliding window average and deviation that contains */
	/* our output point, and chose the average with the minimum deviation. */
#define FW 5		/* [5] Number of delta's to average */
	for (i = FW-1; i < (nummeas-FW); i++) {		/* Samples */
		double basl, bdev;		/* Best average slope, Best deviation */
		double sl[2 * FW -1];
		double asl[FW], dev[FW];		
		int slopen = 0;
		double slopeth = 0.0;
		int m, pp;

		for (pp = 0; pp < 2; pp++) { 			/* For each pass */

			for (j = 0; j < NFB; j++) {			/* For each band */

				/* Compute differences for the range of our windows */
				for (k = 0; k < (2 * FW -1); k++)
					sl[k] = fraw[i+k-FW+1][j] - fraw[i+k+-FW+2][j];

				/* For each window offset, compute average and deviation squared */
				bdev = 1e38;
				for (k = 0; k < FW; k++) { 

					/* Compute average of this window offset */
					asl[k] = 0.0;
					for (m = 0; m < FW; m++)	/* For slope in window */
						asl[k] += sl[k+m];
					asl[k] /= (double)FW;

					/* Compute deviation squared */
					dev[k] = 0.0;
					for (m = 0; m < FW; m++) {
						double tt;
						tt = sl[k+m] - asl[k];
						dev[k] += tt * tt;
					}
					if (dev[k] < bdev)
						bdev = dev[k];
				}

				/* Weight the deviations with a triangular weighting */
				/* to skew slightly towards the center */
				for (k = 0; k < FW; k++) { 
					double wt;
					wt = fabs(2.0 * k - (FW -1.0))/(FW-1.0);
					dev[k] += wt * bdev;
				}

				/* For each window offset, choose the one to use. */
				bdev = 1e38;
				basl = 0.0;
				for (k = 0; k < FW; k++) { 

					/* Choose window average with smallest deviation squared */
					if (dev[k] < bdev) {
						bdev = dev[k];
						basl = fabs(asl[k]);
					}
				}

				if (pp == 0) {		/* First pass, compute average slope over bands */
					slope[i] += basl;

				} else {			/* Second pass, average slopes of bands over threshold */
					if (basl > slopeth) {
						slope[i] += basl;
						slopen++;
					}
				}
			}	/* Next band */

			if (pp == 0) {
				slopeth = 1.0 * slope[i]/j;		/* Compute threshold */
				slope[i] = 0.0;
			} else {
				if (slopen > 0)
					slope[i] /= slopen;			/* Compute average of those over threshold */
			}
		}		/* Next pass */
	}
#undef FW

	/* Normalise the slope values */
	/* Locate the minumum and maximum values */
	maxslope = 0.0;
	minslope = 1e38;
	for (i = 4; i < (nummeas-4); i++) {
		double avs;

		if (slope[i] > maxslope)
			maxslope = slope[i];

		/* Simple moving average for min comp. */
		avs = 0.0;
		for (j = -2; j <= 2; j++)
			avs += slope[i+j];
		avs /= 5.0;
		if (avs < minslope)
			minslope = avs;
	}

	/* Normalise the slope */
	maxslope *= 0.5;
	minslope *= 3.0;
	for (i = 0; i < nummeas; i++) {
		slope[i] = (slope[i] - minslope) / (maxslope - minslope);
		if (slope[i] < 0.0)
			slope[i] = 0.0;
		else if (slope[i] > 1.0)
			slope[i] = 1.0;
	}

	fslope = dvectorz(0, nummeas-1);  

	/* "Automatic Gain control" the raw slope information. */
#define LFW 40		/* [40] Half width of triangular filter */
	for (i = 0; i < nummeas; i++) {
		double sum, twt;
		
		sum = twt = 0.0;
		for (j = -LFW; j <= LFW; j++) {
			double wt;
			if ((i+j) < 0 || (i+j) >= nummeas)
				continue;

			wt = ((LFW-abs(j))/(double)LFW);

			sum += wt * slope[i+j];
			twt += wt;
		}
		fslope[i] = sum/twt;
		if (fslope[i] > fmaxslope)
			fmaxslope = fslope[i];
	}
#undef LFW

	free_dvector(fslope, 0, nummeas-1); fslope = NULL;

	/* Locate the minumum and maximum slope values */
	maxslope = 0.0;
	minslope = 1e38;
	for (i = 4; i < (nummeas-4); i++) {
		double avs;

		if (slope[i] > maxslope)
			maxslope = slope[i];

		/* Simple moving average for min comp. */
		avs = 0.0;
		for (j = -2; j <= 2; j++)
			avs += slope[i+j];
		avs /= 5.0;
		if (avs < minslope)
			minslope = avs;
	}

	/* Normalise the slope again */
	maxslope *= 0.3;
	minslope *= 3.0;
	for (i = 0; i < nummeas; i++) {
		slope[i] = (slope[i] - minslope) / (maxslope - minslope);
		if (slope[i] < 0.0)
			slope[i] = 0.0;
		else if (slope[i] > 1.0)
			slope[i] = 1.0;
	}

#ifdef PATREC_DEBUG
	printf("Slope filter output + filtered raw:\n");
	for (j = 0; j < NFB; j++) {
		for (i = 0; i < nummeas; i++)
			plot[j][i] = fraw[i][j];
		pplot[j] = plot[j];
	}
	for (; j < 10; j++)
		pplot[j] = NULL;

	for (i = 0; i < nummeas; i++)
		plot[10][i] = (double)i;
	do_plot10(plot[10], slope, pplot[0], pplot[1], pplot[2], pplot[3], pplot[4], pplot[5], pplot[6], pplot[7], pplot[8], nummeas, 0);
#endif	/* PATREC_DEBUG */

	sizepop = ivectorz(0, nummeas-1);

	/* Now threshold the measurements into possible patches */
	apat = 2 * nummeas;
	if ((pat = (i1pro3_patch *)malloc(sizeof(i1pro3_patch) * apat)) == NULL) {
		free_dmatrix(fraw, 0, nummeas-1, 0, NFB-1);
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, 0, m->nraw-1);  
		a1logd(p->log, 1, "i1pro3: malloc of patch structures failed!\n");
		return I1PRO3_INT_MALLOC;
	}

	avglength = maxlength = 0.0;
	for (npat = i = 0; i < (nummeas-1); i++) {
		if (slope[i] > thresh)
			continue;

		/* Start of a new patch */
		if (npat >= apat) {
			apat *= 2;
			if ((pat = (i1pro3_patch *)realloc(pat, sizeof(i1pro3_patch) * apat)) == NULL) {
				free_dmatrix(fraw, 0, nummeas-1, 0, NFB-1);
				free_ivector(sizepop, 0, nummeas-1);
				free_dvector(slope, 0, nummeas-1);  
				free_dvector(maxval, 0, m->nraw-1);  
				a1logd(p->log, 1, "i1pro3: reallloc of patch structures failed!\n");
				return I1PRO3_INT_MALLOC;
			}
		}
		pat[npat].ss = i;
		pat[npat].no = 2;
		pat[npat].use = 0;
		for (i++; i < (nummeas-1); i++) {
			if (slope[i] > thresh)
				break;
			pat[npat].no++;
		}

		avglength += (double) pat[npat].no;
		if (pat[npat].no > maxlength)
			maxlength = pat[npat].no;
		npat++;
	}
	a1logd(p->log,7,"Number of patches = %d\n",npat);

	/* We don't count the first and last patches, as we assume they are white leader & tail */
	if (npat < (tnpatch + 2)) {
		free_dmatrix(fraw, 0, nummeas-1, 0, NFB-1);
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, 0, m->nraw-1);  
		free(pat);
		a1logd(p->log,2,"Patch recog failed - unable to detect enough possible patches (%d < %d)\n",npat, tnpatch + 2);
		return I1PRO3_RD_NOTENOUGHPATCHES;
	} else if (npat >= (5 * tnpatch + 2)) {
		free_dmatrix(fraw, 0, nummeas-1, 0, NFB-1);
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, 0, m->nraw-1);  
		free(pat);
		a1logd(p->log,2,"Patch recog failed - detecting too many possible patches (%d >= %d)\n",npat , 5 * tnpatch + 2);
		return I1PRO3_RD_TOOMANYPATCHES;
	}
	avglength /= (double)npat;

	for (i = 0; i < npat; i++) {
		a1logd(p->log,7,"Raw patch %d, start %d, length %d\n",i, pat[i].ss, pat[i].no);
	}

	/* Accumulate popularity ccount of possible patches */
	for (i = 1; i < (npat-1); i++)
		sizepop[pat[i].no]++;

	/* Locate the median potential patch width */
	msthr = npat - tnpatch;			/* Excess number of patches */
	if (msthr > 2)
		msthr = 2;					/* Skip possible header/tail patches */
	msthr += tnpatch/2;				/* Expected median largest patch no. */	

	/* Search from largest to smallest for median */
	for (j = 0, i = maxlength; i > 0; i--) {
		j += sizepop[i];
		if (j >= msthr)
			break;
	}
	median = (double)i;

	a1logd(p->log,7,"Median patch width %f\n",median);

	/* Now decide which patches to use. */
	/* Try a widening window around the median. */
	for (window = 0.1, try = 0; try < WIN_TRIES; window *= 1.3, try++) {
		int bgcount = 0, bgstart = 0;
		int gcount, gstart;
		double wmin = median/(1.0 + window);
		double wmax = median * (1.0 + window);

		a1logd(p->log,7,"Window = %f - %f\n",wmin, wmax);

		/* Track which is the largest contiguous group that */
		/* is within our window */
		gcount = gstart = 0;
		for (i = 1; i < npat; i++) {
			if (i < (npat-1) && pat[i].no <= wmax) {		/* Small enough */
				if (pat[i].no >= wmin) {	/* And big enough */
					if (gcount == 0) {		/* Start of new group */
						gcount++;
						gstart = i;
						a1logd(p->log,7,"Start group at %d\n",gstart);
					} else {
						gcount++;			/* Continuing new group */
						a1logd(p->log,7,"Continue group at %d, count %d\n",gstart,gcount);
					}
				}
			} else {	/* Too big or end of patches, end this group */
				a1logd(p->log,7,"Terminating group group at %d, count %d\n",gstart,gcount);
				if (gcount > bgcount) {		/* New biggest group */
					bgcount = gcount;
					bgstart = gstart;
					a1logd(p->log,7,"New biggest\n");
				}
				gcount = gstart = 0;		/* End this group */
			}
		}
		a1logd(p->log,7,"Biggest group is at %d, count %d\n",bgstart,bgcount);

		/* Tag the patches that we would use */
		for (fpat = 0, i = bgstart; i < npat; i++) {
			if (pat[i].no <= wmax && pat[i].no >= wmin) {
				pat[i].use = 1;
				fpat++;
			}
		}

		if (bgcount == tnpatch) {			/* We're done */
			break;
		}

		if (bgcount > tnpatch) {
			a1logd(p->log,2,"Patch recog failed - detected too many consistent patches\n");
			rv = I1PRO3_RD_TOOMANYPATCHES;
			break;
		}
	}
	if (try >= WIN_TRIES) {
		a1logd(p->log,2,"Patch recog failed - unable to find enough consistent patches\n");
		rv = I1PRO3_RD_NOTENOUGHPATCHES;
	}

	if (p->log->debug >= 7) {
		a1logd(p->log,7,"Got %d patches out of potential %d, want %d:\n",fpat, npat, tnpatch);
		a1logd(p->log,7,"Average patch length %f\n",avglength);
	
		for (i = 1; i < (npat-1); i++) {
			int nosamp = pat[i].no;
	
			if (!pat[i].use)
				continue;
	
			if (p2m != NULL) {		/* Count underlying samples in patch */
				for (nosamp = k = 0; k < pat[i].no; k++)
					nosamp += p2m[pat[i].ss + k][-1];
			}
	
			a1logd(p->log,7,"Patch %d, start %d, length %d (%d):\n",i, pat[i].ss, pat[i].no, nosamp);
		}
	}

	/* Check every patch has enough samples before trimming */
	for (k = 1; k < (npat-1); k++) {
		int nosamp = pat[k].no;

		if (!pat[k].use)
			continue;

		if (p2m != NULL) {		/* Count underlying samples in patch */
			for (nosamp = i = 0; i < pat[k].no; i++)
				nosamp += p2m[pat[k].ss + i][-1];
		}
		if (nosamp < minsamples) {
			a1logd(p->log,2,"Patch recog failed - too few samples (%d < %d)\n",nosamp, minsamples);
			rv = I1PRO3_RD_NOTENOUGHSAMPLES;
		}
	}

	/* Now trim the patches by shrinking their windows */
	for (k = 1; k < (npat-1); k++) {
		int nno, trim;

		if (pat[k].use == 0)
			continue;
		
//		nno = (pat[k].no * 3 + 0)/4;		/*       Trim to 75% & round down */
		nno = (pat[k].no * 2 + 0)/3;		/* [def] Trim to 66% & round down */
//		nno = (pat[k].no * 2 + 0)/4;		/*       Trim to 50% & round down */
		trim = (pat[k].no - nno + 1)/2;

		pat[k].ss += trim;
		pat[k].no = nno;
	}

#ifdef PATREC_SAVETRIMMED			/* Save debugging file */
	{
		static int filen = 0;		/* Debug file index */
		char fname[100];
		FILE *fp;

		sprintf(fname, "i1pro3_raw_trimed_%d.csv",filen++);

		if ((fp = fopen(fname, "w")) == NULL)
			error("Unable to open debug output file '%'",fname);

		/* Create fake "slope" value that marks patches */
		for (i = 0; i < nummeas; i++) 
			slope[i] = 1.0;
		for (k = 1; k < (npat-1); k++) {
			if (pat[k].use == 0)
				continue;
			for (i = pat[k].ss; i < (pat[k].ss + pat[k].no); i++)
				slope[i] = 0.0;
		}

		for (i = 0; i < nummeas; i++) { 
			fprintf(fp, "%f\t",slope[i]);
			for (j = 0; j < m->nraw; j++)
				fprintf(fp, "%f\t", rawmeas[i][j]/maxval[j]);
			fprintf(fp, "\n");
		}
		fclose(fp);
	}
#endif

#ifdef PATREC_DEBUG
	printf("After trimming got:\n");
	for (j = 0, i = 1; i < (npat-1); i++) {
		int acount = pat[i].no;

		if (!pat[i].use)
			continue;

		if (p2m != NULL) {		/* Count underlying samples in patch */
			for (acount = k = 0; k < pat[i].no; k++)
				acount += p2m[pat[i].ss + k][-1];
		}
		printf("Patch %d (%d), start %d, length %d (%d):\n",i, j, pat[i].ss, pat[i].no, acount);
		j++;
	}

	/* Create fake "slope" value that marks patches */
	for (i = 0; i < nummeas; i++) 
		slope[i] = 1.0;
	for (k = 1; k < (npat-1); k++) {
		if (pat[k].use == 0)
			continue;
		for (i = pat[k].ss; i < (pat[k].ss + pat[k].no); i++)
			slope[i] = 0.0;
	}

	printf("Trimmed output:\n");
#ifdef PATREC_PLOT_ALLBANDS
	for (j = 0; j < (m->nraw-9); j += 9) 			/* Plot all the bands */
#else
	for (j = 24; j < (111-9); j += 30) 			/* Do some of the bands */
#endif
	{
		for (k = 0; k < 9; k ++) {
			if (j + k >= m->nraw) {
				pplot[k] = NULL;
				continue;
			}
			for (i = 0; i < nummeas; i++)
				plot[k][i] = rawmeas[i][j+k]/maxval[j+k];
			pplot[k] = plot[k];
		}
		for (i = 0; i < nummeas; i++)
			plot[10][i] = (double)i;
		printf("Raw Bands %d - %d\n",j,j+8);
		do_plot10(plot[10], slope, pplot[0], pplot[1], pplot[2], pplot[3], pplot[4], pplot[5], pplot[6], pplot[7], pplot[8], nummeas, 0);
	}

	free_dmatrix(plot, 0, 10, 0, nummeas-1);  
#endif	/* PATREC_DEBUG */

	free_dmatrix(fraw, 0, nummeas-1, 0, NFB-1);
	free_dvector(slope, 0, nummeas-1);  
	free_ivector(sizepop, 0, nummeas-1);
	free_dvector(maxval, 0, m->nraw-1);  

	if (rv != I1PRO3_OK) {
		free(pat);
		return rv;
	}

	/* Now copy the patch locates to return value */
	for (pix = 0, k = 1; k < (npat-1) && pix < tnpatch; k++) {
		double maxavg = -1e38;	/* Track min and max averages of readings for consistency */
		double minavg = 1e38;
		double avgoverth = 0.0;	/* Average over saturation threshold */
		double cons;			/* Consistency */
		int nosamp = pat[k].no;

		if (!pat[k].use)
			continue;

		/* Check there are enough samples after trimming */
		if (p2m != NULL) {		/* Count underlying samples in patch */
			for (nosamp = i = 0; i < pat[k].no; i++)
				nosamp += p2m[pat[k].ss + i][-1];
		}
		if (nosamp < minsamples) {
			free(pat);
			a1logd(p->log,2,"Patch recog failed - too few trimmed samples (%d, need %d)\n",nosamp,MIN_SAMPLES);
			return I1PRO3_RD_NOTENOUGHSAMPLES;
		}
		patches[pix++] = pat[k];		/* struct copy */
	}

	free(pat);

	return I1PRO3_OK;
}

#undef BL
#undef BH
#undef BW
#undef WIN_TRIES
#undef MIN_SAMPLES

/* Turn a raw set of reflective samples into one calibrated patch spectral value */
/* Samples have been black subtracted and linearized */
i1pro3_code i1pro3_comp_refl_value(
	i1pro3 *p,
	double *m0_spec,		/* Return m0 spectra ('A' illuminant) if not NULL */
	double *m1_spec,		/* Return m1 spectra ('D50' illuminant) if not NULL */
	double *m2_spec,		/* Return m2 spectra (UV cut) if not NULL */
	double **eproc_sample,	/* eproc_sample[enumsample][-9, nwav] */
	int enummeas,			/* Number of even samples */
	double **oproc_sample,	/* oproc_sample[onumsample][-9, nwav] */
	int onummeas,			/* Number of odd samples */
	int hr					/* High res flag */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	double **edriftcorr;		/* led model, drift compensation corrections */
	double **odriftcorr;		/* led model, drift compensation corrections */
	double **avg_nd_sample;		/* average nn/uv not drift corrected samples */
	double **avg_sample;		/* average nn/uv samples, progresively corrected */
	double *avg_uv_drcor;		/* Average uv drift correction values */
	double **matA_3x36, **imatA_36x3;
	double c12timesc7[36];
	double _sumc0c1c2[MX_NWAV], *sumc0c1c2[1] = { _sumc0c1c2 };
	double **uv_weight, uvbl_val;
	double fwa_edi_resp[MX_NWAV];
	double fwa_e_resp[MX_NWAV];
	double sc_avg_sample[MX_NWAV];	/* Scaled avg_sample */
	double cv_A[MX_NWAV];
	double uv_ratio_scale, uv_ratio;
	double cv_B[MX_NWAV];
	double cv_C[MX_NWAV];
	double cv_BmC[MX_NWAV];
	double cv_D[MX_NWAV];
	double cv_E[MX_NWAV];
	double cv_F[MX_NWAV];
	double cv_G[MX_NWAV];
	double cv_H[MX_NWAV];
	double cv_I[MX_NWAV];
	double cv_J[MX_NWAV];
	double cv_K[MX_NWAV];
	double cv_L[MX_NWAV];
	double cv_M[MX_NWAV];
	double cv_O[MX_NWAV];
	double cv_P[MX_NWAV];
	double cv_Q[MX_NWAV];
	double m0[MX_NWAV], m1[MX_NWAV], m2[MX_NWAV];
	int i;

	a1logd(p->log,3,"i1pro3_comp_refl_value\n");

	/* Compute average of sample measurement with no drift correction */
	avg_nd_sample = dmatrix(0, 1, 0, m->nwav[hr]-1);
	i1pro3_average_wavmmeas(p, avg_nd_sample[0], eproc_sample, enummeas, hr);
	i1pro3_average_wavmmeas(p, avg_nd_sample[1], oproc_sample, onummeas, hr);

	/* Compute LED model for each measurement */
	edriftcorr = dmatrix(0, enummeas-1, -9, MX_NWAV);
	odriftcorr = dmatrix(0, onummeas-1, -9, MX_NWAV);
	i1pro3_comp_ledm(p, edriftcorr, eproc_sample, enummeas, 0);
	i1pro3_comp_ledm(p, odriftcorr, oproc_sample, onummeas, 1);

	/* Average uv drift compensation values at std res and then hr res */
	avg_uv_drcor = dvectorz(0, MX_NWAV-1);  

	/* Compute drift correction for each measurement */
	/* and apply it to each measurement */ 
	for (i = 0; i < enummeas; i++) {	/* For nn */
		vect_div3_safe(edriftcorr[i], s->calsp_ledm[0], edriftcorr[i], 36); 
		if (hr)
			fast_upsample(p, edriftcorr[i], edriftcorr[i], 0, "nn drift corn");
		vect_mul(eproc_sample[i], edriftcorr[i], m->nwav[hr]);
	}

	for (i = 0; i < onummeas; i++) {	/* For uv */
		vect_div3_safe(odriftcorr[i], s->calsp_ledm[1], odriftcorr[i], 36);
		vect_add(avg_uv_drcor, odriftcorr[i], 36);
		if (hr)
			fast_upsample(p, odriftcorr[i], odriftcorr[i], 0, "nn drift corn");
		vect_mul(oproc_sample[i], odriftcorr[i], m->nwav[hr]); 
	}

	/* Compute std res avg_uv_drcor */
	vect_scale1(avg_uv_drcor, 1.0/(double)onummeas, 36);

	/* Compute average of drift corrected sample */
	avg_sample = dmatrix(0, 1, 0, m->nwav[hr]-1);
	i1pro3_average_wavmmeas(p, avg_sample[0], eproc_sample, enummeas, hr);
	i1pro3_average_wavmmeas(p, avg_sample[1], oproc_sample, onummeas, hr);

	/* Scale average drift corrected sample by illumination correction */
	vect_mul(avg_sample[0], s->calsp_illcr[hr][0], m->nwav[hr]);
	vect_mul(avg_sample[1], s->calsp_illcr[hr][1], m->nwav[hr]);

	free_dmatrix(edriftcorr, 0, enummeas-1, -9, MX_NWAV);
	free_dmatrix(odriftcorr, 0, onummeas-1, -9, MX_NWAV);

	/* Stray light correct average drift and illumination corrected sample values */
	i1pro3_straylight(p, hr, avg_sample, 2);
	clear_low_wav(p, avg_sample[0], 4, hr);		/* Zero first 4 nn samples */

	/* Now that we have the averaged values, we can compute the rest of the conversion */

	/* Like ledm, do matrix computations leading to uvbl_val at std res. */
	/* Setup matrix */
	matA_3x36 = dmatrix(0, 2, 0, 36-1);
	vect_cpy(matA_3x36[0], s->sc_calsp_nn_white[0], 36);	/* nn white sum scaled to ref illum */
	vect_cpy(matA_3x36[1], s->cal_l_uv_diff[0], 36);		/* long UV diff spectrum */
	vect_cpy(matA_3x36[2], s->cal_s_uv_diff[0], 36);		/* short UV diff spectrum */

	/* Sum of the three components of the matrix */
	vect_add3(_sumc0c1c2, matA_3x36[0], matA_3x36[1], 36);
	vect_add(_sumc0c1c2, matA_3x36[2], 36);

	/* Normalize it by 1.0/(drift correction * uv illum correction)  */
	vect_mul3(c12timesc7, avg_uv_drcor, s->calsp_illcr[0][1], 36); 
	vect_div(matA_3x36[0], c12timesc7, 36);
	vect_div(matA_3x36[1], c12timesc7, 36);
	vect_div(matA_3x36[2], c12timesc7, 36);

	/* Pseudo-Invert using lu decomposition */
	imatA_36x3 = dmatrix(0, 36-1, 0, 2);
	if (lu_psinvert(imatA_36x3, matA_3x36, 3, 36)) {
		a1logd(p->log,1,"i1pro3_do_measure: invert failed at line %d\n",__LINE__);
		return I1PRO3_INT_ASSERT;
	}

	/* Compute weighting of (inverse drift/illum corrected) c0, c1, c2 */
	/* that best approximates the sum of c0, c1, c2. */
	/* These will all be close to 1.0 ... */
	uv_weight = dmatrix(0, 0, 0, 2);
    if (matrix_mult(uv_weight,  1,  3,
	                            sumc0c1c2, 1, 36, imatA_36x3, 36, 3)) {
		a1logd(p->log,1,"i1pro3_do_measure: matrix_mult failed at line %d\n",__LINE__);
		return I1PRO3_INT_ASSERT;
	}

	free_dmatrix(imatA_36x3, 0, 35, 0, 2);
	free_dmatrix(matA_3x36, 0, 2, 0, 36-1);

	/* Weighted blend of long and short UV inverse correction factors. */
	/* (Could we fix above to work using inverse correction factor, to */
	/*  eliminate inverses here ?) */ 
	uvbl_val = 1.0/((m->ee_sluv_bl/uv_weight[0][2] + (1.0 - m->ee_sluv_bl)/uv_weight[0][1]));

	/* now back to the samples .... */

	/* FWA response due to UV */

	vect_sub3(fwa_edi_resp, avg_sample[1], avg_sample[0], m->nwav[hr]);

	/* Converted to emission calibrated spectrum */
	vect_mul(fwa_edi_resp, m->emis_coef[hr], m->nwav[hr]);

	/* - - - - - - - */
	/* Non Drift and Illuminant corrected samples: */
	/* Stray light correct nn/uv !drift & !illum sample values */
	i1pro3_straylight(p, hr, avg_nd_sample, 2);
	clear_low_wav(p, avg_nd_sample[0], 4, hr);		/* Zero first 4 nn samples */

	/* FWA response due to UV */
	vect_sub3(fwa_e_resp, avg_nd_sample[1], avg_nd_sample[0], m->nwav[hr]);
	free_dmatrix(avg_nd_sample, 0, 1, 0, m->nwav[hr]-1);

	/* Converted to emission calibrated spectrum */
	vect_mul(fwa_e_resp, m->emis_coef[hr], m->nwav[hr]);

	/* - - - - - - - - */
	/* 2/(2 x average actual nn & uv illumination)
	   x average of drift & ill & stray corrd, normalized nn sample */
	vect_mul3(sc_avg_sample, avg_sample[0], s->iavg2aillum[hr], m->nwav[hr]); 
	vect_scale1(sc_avg_sample, 2.0, m->nwav[hr]); 

	/*    sc_avg_sample
	    x drift & stray corrd long uv - nn, scaled to have ee_luv_inttarg sum
		x Emissive calibration factor / White reference tile reflectivity
	*/

	/* cv_A */
	vect_mul3(cv_A, sc_avg_sample, m->emis_coef[hr], m->nwav[hr]);
	vect_div(cv_A, m->white_ref[hr], m->nwav[hr]);
	vect_mul(cv_A, s->cal_l_uv_diff[hr], m->nwav[hr]);

	/* We're subtracting the reflected long uv response from measured */
	/* difference between uv and nn */
	if (hr) {
		double wl1, wl2;
		double wl3, wl4;
		double sr = m->wl_width[1]/m->wl_width[0];

		wl1 = 442.0;		/* [441] */
		wl2 = 450.0;		/* [450] */

		wl3 = 540.0;		/* [540] */
		wl4 = 548.0;		/* [549] */

		uv_ratio = (  sr * sum_wav2(p, fwa_e_resp, wl1, wl2, wl3, wl4, 1)
		            - sr * sum_wav2(p, cv_A, wl1, wl2, wl3, wl4, 1)/uv_weight[0][1])
		           * uvbl_val;
	} else {
		uv_ratio = (  vect_sum(fwa_e_resp + 7, 16 - 7)
		            - vect_sum(cv_A + 7, 16 - 7)/uv_weight[0][1])
		           * uvbl_val;
	}

	free_dmatrix(uv_weight, 0, 0, 0, 2);

	/* Threshold the ratio below 10, and linearly interpolated */
	/* transition from the 10 to 30 */
	uv_ratio_scale = 1.0;
	if (uv_ratio < 10.0)
		uv_ratio_scale = 0.0;
	else if (uv_ratio < 30.0)
		uv_ratio_scale = (uv_ratio - 10.0)/20.0;
	uv_ratio *= uv_ratio_scale;

	/* Scale spectrum by ratio */
	vect_scale(cv_B, m->fwa_std[hr], uv_ratio, m->nwav[hr]); 

	//cv_C = avg_uv_drcor * calsp_illcr[1] * cv_B / uvbl_val
	if (hr)
		fast_upsample(p, avg_uv_drcor, avg_uv_drcor, 0, "avg_uv_drcor");
	vect_mul3(cv_C, avg_uv_drcor, s->calsp_illcr[hr][1], m->nwav[hr]);
	vect_mul3(cv_C, cv_C, cv_B, m->nwav[hr]);
	vect_scale1(cv_C, 1.0/uvbl_val, m->nwav[hr]);

	// cv_D = fwa_edi_resp + cv_B - cv_C
	vect_sub3(cv_BmC, cv_B, cv_C, m->nwav[hr]);
	vect_add3(cv_D, fwa_edi_resp, cv_BmC, m->nwav[hr]);

	// cv_E = avg_sample nn + uv 
	vect_add3(cv_E, avg_sample[1], avg_sample[0], m->nwav[hr]);
	free_dmatrix(avg_sample, 0, 1, 0, m->nwav[hr]);

	// cv_F = iavg2aillum * (cv_E + (cv_B - cv_C)/emis_coef) 
	vect_div3(cv_F, cv_BmC, m->emis_coef[hr], m->nwav[hr]);
	vect_add(cv_F, cv_E, m->nwav[hr]);
	vect_mul(cv_F, s->iavg2aillum[hr], m->nwav[hr]);

	// cv_G = <m2_fwa> * (cv_D - <fwa_cal> * cv_F)
	vect_mul3(cv_G, m->fwa_cal[hr], cv_F, m->nwav[hr]); 


	vect_sub3(cv_G, cv_D, cv_G, m->nwav[hr]); 

	vect_mul(cv_G, m->m2_fwa[hr], m->nwav[hr]); 

	// cv_H = 1.0
	// cv_I = cv_H - <m2_fwa> * <fwa_cal>
	vect_mul3(cv_I, m->m2_fwa[hr], m->fwa_cal[hr], m->nwav[hr]);
	vect_set(cv_H, 1.0, m->nwav[hr]);
	vect_sub3(cv_I, cv_H, cv_I, m->nwav[hr]);

	// cv_J = clip+ve(cv_G / cv_I) * uv_ratio_scale
	vect_div3(cv_J, cv_G, cv_I, m->nwav[hr]);
	vect_clip(cv_J, cv_J, 0.0, DBL_MAX, m->nwav[hr]);
	vect_scale1(cv_J, uv_ratio_scale, m->nwav[hr]);

	// Result: clip(m2 = cv_F - CV_J)
	// Note m2 is uv-excluded. m0 & m1 are based on top of this. 
	vect_sub3(m2, cv_F, cv_J, m->nwav[hr]);
	vect_clip(m2, m2, 0.0, DBL_MAX, m->nwav[hr]);

	if (m2_spec != NULL) {
		vect_cpy(m2_spec, m2, m->nwav[hr]);
	}

	if (m1_spec != NULL) {
		// m1: D50 illum.
		// cv_L = <m1_fwa> x <fwa_cal> x m2
		vect_mul3(cv_L, m->m1_fwa[hr], m->fwa_cal[hr], m->nwav[hr]);
		vect_mul(cv_L, m2, m->nwav[hr]);

		// cv_K = <m1_fwa> * cv_D
		vect_mul3(cv_K, m->m1_fwa[hr], cv_D, m->nwav[hr]);

		// cv_M = cvlip+ve(cv_K - cv_L) * uv_ratio_scale
		vect_sub3(cv_M, cv_K, cv_L, m->nwav[hr]);
		vect_clip(cv_M, cv_M, 0.0, DBL_MAX, m->nwav[hr]);
		vect_scale1(cv_M, uv_ratio_scale, m->nwav[hr]);

		// Result: clip(m1 = cv_M + m2) */
		vect_add3(m1, cv_M, m2, m->nwav[hr]);
		vect_clip(m1, m1, 0.0, DBL_MAX, m->nwav[hr]);
		vect_cpy(m1_spec, m1, m->nwav[hr]);
	}

	if (m0_spec != NULL) {
		// m0: legacy, tungsten illum.
		// cv_P = <m0_fwa> x <fwa_cal> x m2
		vect_mul3(cv_P, m->m0_fwa[hr], m->fwa_cal[hr], m->nwav[hr]);
		vect_mul(cv_P, m2, m->nwav[hr]);

		// cv_O = <m0_fwa> x cv_D */
		vect_mul3(cv_O, m->m0_fwa[hr], cv_D, m->nwav[hr]);

		// cv_Q = cvlip+ve(cv_O - cv_P) * uv_ratio_scale
		vect_sub3(cv_Q, cv_O, cv_P, m->nwav[hr]);
		vect_clip(cv_Q, cv_Q, 0.0, DBL_MAX, m->nwav[hr]);
		vect_scale1(cv_Q, uv_ratio_scale, m->nwav[hr]);

		// Result: clip(m0 = cv_Q + m2) */
		vect_add3(m0, cv_Q, m2, m->nwav[hr]);
		vect_clip(m0, m0, 0.0, DBL_MAX, m->nwav[hr]);
		vect_cpy(m0_spec, m0, m->nwav[hr]);
	}

	free_dvector(avg_uv_drcor, 0, MX_NWAV-1);  

	return ev;
}

/* Turn a set of polarized reflective samples into one calibrated patch spectral value */
/* Samples have been black subtracted, linearized and converted to wav */
i1pro3_code i1pro3_comp_pol_refl_value(
	i1pro3 *p,
	double *spec,			/* Return calibrated spectra */
	double **proc_sample,	/* proc_sample[enumsample][-9, nwav] */
	int nummeas,			/* Number of samples */
	int hr
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
#ifdef STRICT_IMITATE
	double refillumsum[36];				/* Sum of nn & uv illumination references */
	double nnweight[36];				/* nn ref illum. blend weighting */
	double uvweight[36];				/* uv ref illum. blend weighting */
#endif
	int i;

	a1logd(p->log,3,"i1pro3_comp_pol_refl_value\n");

#ifdef STRICT_IMITATE
	/* Compute reference illumination blend weightings. Weights sum to 1.0, */
	/* and are equal above 450nm, with full weight to the UV region in uvweight */
	/* and zero weight in nnweight. */ 
	vect_add3(refillumsum, m->ee_ref_nn_illum, m->ee_ref_uv_illum, 36);
	vect_div3(nnweight, m->ee_ref_nn_illum, refillumsum, 36);
	vect_div3(uvweight, m->ee_ref_uv_illum, refillumsum, 36);
#endif

	/* Compute reflectance for each sample and accumulate */
	vect_set(spec, 0.0, m->nwav[hr]);
	for (i = 0; i < nummeas; i++) {
		double nn_ledm[36], *pnn_ledm[1] = { nn_ledm };	/* nn led model for this measurement */
		double uv_ledm[MX_NWAV], *puv_ledm[1] = { uv_ledm };/* uv led model for this measurement */

		i1pro3_comp_ledm(p, pnn_ledm, proc_sample + i, 1, 0);
		i1pro3_comp_ledm(p, puv_ledm, proc_sample + i, 1, 1);

#ifdef STRICT_IMITATE
		/* Same calculation as pol calibration - see comments there */
		vect_mul(uv_ledm, uvweight, 36); 
		vect_mul(nn_ledm, nnweight, 36); 

		/* This measurements led model */
		vect_add(uv_ledm, nn_ledm, 36);
#else
		/* Use (presumed) correct average of nn & uv as illum cal reference */
		vect_blend(uv_ledm, uv_ledm, nn_ledm, 0.5, 36);
#endif

		/* Compute led drift compensation value */
		vect_div3(nn_ledm, s->pol_calsp_ledm, uv_ledm, 36);

		if (hr)
			fast_upsample(p, nn_ledm, nn_ledm, 0, "pol ledm compensation");

		/* Illumination compensated value */
		vect_mul(proc_sample[i], nn_ledm, m->nwav[hr]);

		i1pro3_straylight(p, hr, proc_sample + i, 1);

		/* Convert to calibrated reflectance value */
		vect_muldiv(proc_sample[i], m->white_ref[hr], s->pol_calsp_white[hr], m->nwav[hr]);

		/* Sum to average reflectance of sample */
		vect_add(spec, proc_sample[i], m->nwav[hr]);
	}
	/* Complete average calculations of return value */
	vect_scale1(spec, 1.0/nummeas, m->nwav[hr]);

	return ev;
}

/* Filter dynamic black shield value to smooth out randomness */
double i1pro3_dynsh_filt(
	i1pro3 *p,
	time_t meastime,
	double temp,
	double inttime,
	double sv
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int ix = m->dynsh_ix = (m->dynsh_ix + 1) % N_DYNSHVAL;
	int i, nav;
	double twt, rsv;
	double tlimit = 2.0;

	/* Add the new measurement */
	m->dynsh[ix].meastime = meastime;
	m->dynsh[ix].temp = temp;
	m->dynsh[ix].inttime = inttime;
	m->dynsh[ix].sv = sv;

	/* Compute weighted average. We weight samples with long integration times */ 
	/* and close temperatures. */
	rsv = twt = 0.0;
	for (nav = i = 0; i < N_DYNSHVAL; i++) {
		double tdel, wt;
		if ((meastime - m->dynsh[i].meastime) > DCALTOUT)
			continue;			/* Too old */
		tdel = fabs(temp - m->dynsh[i].temp);
		if (tdel >= tlimit)
			continue;
		wt = m->dynsh[i].inttime * (tlimit - tdel);
		rsv += wt * m->dynsh[i].sv;
		twt += wt;
		nav++;
	}
	rsv /= twt;

	a1logd(p->log,8," i1pro3_dynsh_filt got sv %f returning %f from avg of %d\n",sv,rsv,nav);
	
	return rsv;
} 

/* Construct a black to subtract from an emissive measurement. */
void i1pro3_comp_emis_black(
	i1pro3 *p,
	double *black,			/* Return raw black to subtract */
	double **raw_black1,	/* Before and after short measurements */
	double **raw_black2,
	int nummeasB,			/* Number of samples in short measurements */
	double sinttime, 		/* Integration time of short measurements (typicall min_int) */
	double **raw_sample,	/* Longer sample measurement */
	int nummeasS,			/* Number of samples in longer measurement */
	double linttime,		/* Integration time of longer measurement */
	double btemp			/* Current board temperature */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
//	double _psample[MX_NRAW_1], *psample = _psample + 1;
	double sb, sv;			/* Shield base and variable values */
	double sa, da, bl;		/* Dynamic adjustment, blend factor */
	int i;

	/* Average the short shielded values */
	sb = 0.0;
	for (i = 0; i < nummeasB; i++) {
		sb += raw_black1[i][-1];
		sb += raw_black2[i][-1];
	}
	sb /= (2.0 * nummeasB);

	/* Average the long shield values */
	sv = 0.0;
	for (i = 0; i < nummeasS; i++)
		sv += raw_sample[i][-1];
	sv /= (double) nummeasS;

	/* Compute shield variable and base values */
	if (linttime > (20.0 * sinttime)) {
		sv = (sv - sb)/(linttime - sinttime);
		/* The sv value is quite noisy, so filter it using any recent measurements */
		sv = i1pro3_dynsh_filt(p, time(NULL), btemp, linttime, sv);
	} else {
		sv = s->idark_data[1][-1];		/* Use calibration value so adj == 1.0 */
	}

	sb = sb - sv * sinttime;

	sa = sb/s->idark_data[0][-1];		/* Base static temp. adjustment */
	da = sv/s->idark_data[1][-1];		/* Base dynamic temp. adustment */

#ifdef ENABLE_DYNBLKTC
	/* The dynamic adjustment may make the short term */
	/* repeatability worse by a factor of about 10 without filtering. */
	/* But it significantly improves the medium and long term stability. */
	if (linttime < 0.05)
		bl = 0.0;
	else if (linttime > 0.5)
		bl = 1.0;
	else
		bl = (linttime - 0.05)/(0.5 - 0.05);
	da = (1.0 - bl) * 1.0 + bl * da;

#else
	da = 1.0;
#endif

	/* Construct our black */
	vect_scale(black, s->idark_data[0], sa, m->nraw);
	vect_scaleadd(black, s->idark_data[1], linttime * da, m->nraw);

//printf("~1 raw black, sample:\n");
//i1pro3_average_rawmmeas(p, psample, raw_sample, nummeasW);
//plot_raw2(black, psample);
}

/* Do an adaptive emission type measurement and return the processed raw values. */
/* We don't check the adapter type. */
/* It is checked for consistency. */
/* It is scaled for integration time. */
/* To compute to calibrated wav need to:
	convert from raw to wav
	apply straylight
	apply wav calibration factors
	possibly average together
*/
i1pro3_code i1pro3_spot_adapt_emis_raw_meas(
	i1pro3 *p,
	double ***praw,		/* Return raw measurements. */
						/* Call i1pro3_free_raw(p, i1p3mm_em, raw, nummeas) when done */
	int *pnummeas		/* Return number of raw measurements */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	double btemp;
	double binttime, ainttime;
	int nummeasB, nummeasW;
	double **raw_black1, **raw_black2, **raw_sample;	/* raw measurements */
	double black[MX_NRAW], _psample[MX_NRAW+1], *psample = _psample+1;
	double maxval;
	int i;

	if ((ev = i1pro3_getboardtemp(p, &btemp)) != I1PRO3_OK) {
		error(" i1pro3_getboardtemp failed\n");
		return ev;
	}

	/* Correct wl calibration for any temperature change */
	if (m->ee_version >= 1) {
		if ((ev = i1pro3_recompute_wav_filters_for_temp(p, btemp)) != I1PRO3_OK) {
			error(" i1pro3_recompute_wav_filters_for_temp failed\n");
			return ev;
		}
	}

	/* Preliminary measure to calculate target integration time */
	/* An integration time of 0.05 should be good up to about */
	/* 5000 cd/m^2 for typical displays, and we fall back */
	/* to min_int sample if level is higher than this. */
	ainttime = 0.05;
	nummeasW = i1pro3_comp_nummeas(p, 0.25, ainttime);
	if ((ev = i1pro3_do_measure(p, i1p3mm_em, &raw_sample, &nummeasW, &ainttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure failed\n");
		return ev;
	}

	i1pro3_average_rawmmeas(p, psample, raw_sample, nummeasW);
	i1pro3_comp_simple_emis_black(p, black, raw_sample, nummeasW, ainttime); 
	i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeasW);
	vect_sub(psample, black, m->nraw);	/* Subtract black from psample */
	i1pro3_vect_lin(p, psample);		/* Linearize */
	maxval = vect_max(psample, m->nraw);

	a1logd(p->log,4," adaptive pmeas maxval %f\n",maxval);

	if (maxval > m->sens_sat) {
		ainttime = 0.0;
	} else {
		if (maxval <= 0.0)
			maxval = 1.0;
		ainttime = ainttime * s->targoscale * m->sens_target/maxval;
		if (ainttime < m->min_int_time)
			ainttime = m->min_int_time;
		else if (ainttime > m->max_int_time)
			ainttime = m->max_int_time;
	}

	a1logd(p->log,3," adaptive inttime %f\n",ainttime);

	{
		/* Read short int time */
		binttime = m->min_int_time;
		nummeasB = i1pro3_comp_nummeas(p, s->dreadtime, binttime);
	}
	if ((ev = i1pro3_do_measure(p, i1p3mm_em, &raw_black1, &nummeasB, &binttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure failed\n");
		return ev;
	}

	/* If preliminary measure was saturated, so use short measure to compute ainttime */
	if (ainttime == 0.0) {
		i1pro3_average_rawmmeas(p, psample, raw_black1, nummeasB);
		i1pro3_comp_simple_emis_black(p, black, raw_black1, nummeasB, binttime); 
		vect_sub(psample, black, m->nraw);	/* Subtract black from psample */
		i1pro3_vect_lin(p, psample);		/* Linearize */
		maxval = vect_max(psample, m->nraw);

		a1logd(p->log,4," short meas maxval %f\n",maxval);

		if (maxval > m->sens_sat) {
			a1logd(p->log,1," sample is saturated\n");
			i1pro3_free_raw(p, i1p3mm_em, raw_black1, nummeasB);
			return I1PRO3_RD_SENSORSATURATED;
		} else {
			if (maxval <= 0.0)
				maxval = 1.0;
			ainttime = binttime * s->targoscale * m->sens_target/maxval;
			if (ainttime < m->min_int_time)
				ainttime = m->min_int_time;
			else if (ainttime > m->max_int_time)
				ainttime = m->max_int_time;
		}
		a1logd(p->log,3," adaptive inttime #2 %f\n",ainttime);
	}

	/* For non-adaptive we should only reduce integration time */
	if (!s->adaptive) {
		if (ainttime < s->inttime) {
			s->inttime = ainttime;
			a1logd(p->log,5,"Reduced display integration time to %f\n",s->inttime);
		}
		ainttime = s->inttime;
	}

	/* Main measure */
	nummeasW = i1pro3_comp_nummeas(p, s->wreadtime, ainttime);
	a1logd(p->log,2,"\nDoing adaptive measure nummeas %d of int_time %f\n", nummeasW, ainttime);

	if ((ev = i1pro3_do_measure(p, i1p3mm_em, &raw_sample, &nummeasW, &ainttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure failed\n");
		i1pro3_free_raw(p, i1p3mm_em, raw_black1, nummeasB);
		return ev;
	}

	/* Read 2nd short int time */
	if ((ev = i1pro3_do_measure(p, i1p3mm_em, &raw_black2, &nummeasB, &binttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure failed\n");
		i1pro3_free_raw(p, i1p3mm_em, raw_black1, nummeasB);
		i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeasW);
		return ev;
	}

	/* Construct our black */
	i1pro3_comp_emis_black(p, black, raw_black1, raw_black2, nummeasB, binttime,
	                       raw_sample, nummeasW, ainttime, btemp);

	i1pro3_free_raw(p, i1p3mm_em, raw_black1, nummeasB);
	i1pro3_free_raw(p, i1p3mm_em, raw_black2, nummeasB);

//printf("~1 raw black, sample:\n");
//i1pro3_average_rawmmeas(p, psample, raw_sample, nummeasW);
//plot_raw2(black, psample);

	/* Subtract black from sample */
	i1pro3_multimeas_sub_black(p, raw_sample, nummeasW, black);

	/* Check if sample is saturated */
	if (i1pro3_multimeas_check_sat(p, NULL, raw_sample, nummeasW)) {
		a1logd(p->log,1," sample is saturated\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_sample, nummeasW);
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Check if sample is consistent */
	if (i1pro3_multimeas_check_raw_consistency(p, raw_sample, nummeasW)) {
		a1logd(p->log,1," sample is inconsistent\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_sample, nummeasW);
		return I1PRO3_RD_READINCONS;
	}

	/* Linearize and normalize integration time */
	i1pro3_multimeas_lin(p, raw_sample, nummeasW);
	i1pro3_normalize_rawmmeas(p, raw_sample, nummeasW, ainttime);

	/* Return result */
	if (praw != NULL)
		*praw = raw_sample;
	if (pnummeas != NULL)
		*pnummeas = nummeasW;

	return ev;
}

/* Construct a simple (staticaly) adjusted black to subtract from an emissive measurement. */
void i1pro3_comp_simple_emis_black(
	i1pro3 *p,
	double *black,			/* Return raw black to subtract */
	double **raw_sample,	/* Sample measurements */
	int nummeas,			/* Number of samples */
	double inttime			/* Integration time */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	double sb;		/* Shield base values */
	double sa;		/* Dynamic adjustment, blend factor */
	int i;

	/* Average the shielded values */
	sb = 0.0;
	for (i = 0; i < nummeas; i++)
		sb += raw_sample[i][-1];
	sb /= (double) nummeas;

	sb = sb - s->idark_data[1][-1] * inttime;	/* Zero int. time base shield value */

	sa = sb/s->idark_data[0][-1];				/* Base static temp. adjustment */

	/* Construct our black */
	vect_scale(black, s->idark_data[0], sa, m->nraw);
	vect_scaleadd(black, s->idark_data[1], inttime, m->nraw);
}

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
	double *pinttime,	/* Integration time to use */
	double meastime,	/* Measurement time to use */
	int donorm			/* Normalize for integration time */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	int nummeas;
	double **raw_sample;	/* raw measurements */
	double black[MX_NRAW];
	int i;

	nummeas = i1pro3_comp_nummeas(p, meastime, *pinttime);
	a1logd(p->log,2,"\nDoing simple measure nummeas %d of int_time %f\n", nummeas, *pinttime);

	if ((ev = i1pro3_do_measure(p, i1p3mm_em, &raw_sample, &nummeas, pinttime, NULL, NULL)) != I1PRO3_OK) {
		a1logd(p->log,1," i1pro3_do_measure failed\n");
		return ev;
	}

	/* Construct simple black */
	i1pro3_comp_simple_emis_black(p, black, raw_sample, nummeas, *pinttime);

	/* Subtract black from sample */
	i1pro3_multimeas_sub_black(p, raw_sample, nummeas, black);

	/* Check if sample is saturated */
	if (i1pro3_multimeas_check_sat(p, NULL, raw_sample, nummeas)) {
		a1logd(p->log,1," sample is saturated\n");
		i1pro3_free_raw(p, i1p3mm_rf_wh, raw_sample, nummeas);
		return I1PRO3_RD_SENSORSATURATED;
	}

	/* Linearize and normalize samples */
	i1pro3_multimeas_lin(p, raw_sample, nummeas);
	if (donorm)
		i1pro3_normalize_rawmmeas(p, raw_sample, nummeas, *pinttime);

	/* Return result */
	if (praw != NULL)
		*praw = raw_sample;
	if (pnummeas != NULL)
		*pnummeas = nummeas;

	return ev;
}

/* Do a simple emission measurement and return the processed wav values */
i1pro3_code i1pro3_spot_simple_emis_meas(
	i1pro3 *p,
	double **specrd,		/* Return array [numpatches][nwav] of spectral reading values */
	int numpatches,			/* Number of sample to measure */
	double *inttime, 		/* Integration time to use/used */
	int hr					/* High resolution flag */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	i1pro3_code ev = I1PRO3_OK;
	double **raw_sample;
	int nummeas = numpatches;
	double meastime = nummeas * *inttime;
	int i;

	if (!s->emiss || s->scan) {
		a1logd(p->log,1,"i1pro3_spot_simple_emis_meas in unexpected mode\n");
		return I1PRO3_INT_WRONGMODE;
	}

	if ((ev = i1pro3_spot_simple_emis_raw_meas(p, &raw_sample, &nummeas, inttime, meastime, 1)) != I1PRO3_OK) {
		return ev;
	}

	if (nummeas != numpatches)	/* Hmm. */
		error("Assert in %s at line %d nummeas %d != numpatches %d\n",__FILE__,__LINE__,nummeas, numpatches);

	i1pro3_absraw_to_abswav(p, hr, 2, specrd, raw_sample, nummeas);
	i1pro3_free_raw(p, i1p3mm_em, raw_sample, nummeas);
	i1pro3_straylight(p, hr, specrd, nummeas);

	/* Scale by calibration factor */
	for (i = 0; i < nummeas; i++)
		vect_mul(specrd[i], s->cal_factor[hr], m->nwav[hr]);

	return ev;
}

/* Recognise any flashes in the readings, and */
/* and average their raw values together as well as summing their duration. */
/* Return nz on an error */
i1pro3_code i1pro3_extract_patches_flash(
	i1pro3 *p,
	double *duration,		/* return duration */
	double *pavg,			/* return patch average [nraw] */
	double **multimeas,		/* Array of [nummeas][nraw] value to extract from */
	int nummeas,			/* number of readings made */
	double inttime			/* Integration time (used to compute duration) */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i, j, k, pix;
	double minval, maxval;		/* min and max input value at wavelength of maximum input */
	double mean;				/* Mean of the max wavelength band */
	int maxband;				/* Band of maximum value */
	double thresh;				/* Level threshold */
	int fsampl;					/* Index of the first sample over the threshold */
	int nsampl;					/* Number of samples over the threshold */
	double *aavg;				/* ambient average [-1 nraw] */
	double finttime;			/* Flash integration time */
#ifdef PATREC_DEBUG
	double **plot;
#endif

	a1logd(p->log,2,"i1pro3_extract_patches_flash looking for flashes in %d measurements\n",nummeas);

	/* Discover the maximum input value for flash dection */
	maxval = -1e6;
	maxband = 0;
	for (j = 0; j < m->nraw; j ++) {
		for (i = 0; i < nummeas; i++) {
			if (multimeas[i][j] > maxval) {
				maxval = multimeas[i][j];
				maxband = j;
			}
		}
	}

	if (maxval <= 0.0) {
		a1logd(p->log,2,"No flashes found in measurement\n");
		return I1PRO3_RD_NOFLASHES;
	}

	minval = 1e6;
	mean = 0.0;
	for (i = 0; i < nummeas; i++) {
		mean += multimeas[i][maxband];
		if (multimeas[i][maxband] < minval)
			minval = multimeas[i][maxband];
	}
	mean /= (double)nummeas;

	/* Set the threshold at 5% from mean towards max */
	thresh = (3.0 * mean + maxval)/4.0;
	a1logd(p->log,7,"i1pro3_extract_patches_flash band %d minval %f maxval %f, mean = %f, thresh = %f\n",maxband,minval,maxval,mean, thresh);

#ifdef PATREC_DEBUG
	/* Plot out 6 lots of 6 values each */ 
	plot = dmatrixz(0, 6, 0, nummeas-1);  
	for (j = maxband -3; j>= 0 && j < (m->nraw-6); j += 100)		/* Do one set around max */
	{
		for (k = 0; k < 6; k ++) {
			for (i = 0; i < nummeas; i++) { 
				plot[k][i] = multimeas[i][j+k]/maxval;
			}
		}
		for (i = 0; i < nummeas; i++)
			plot[6][i] = (double)i;
		printf("Bands %d - %d\n",j,j+5);
		do_plot6(plot[6], plot[0], plot[1], plot[2], plot[3], plot[4], plot[5], nummeas);
	}
	free_dmatrix(plot,0,6,0,nummeas-1);
#endif	/* PATREC_DEBUG */

#ifdef PATREC_DEBUG
	/* Plot just the pulses */
	{
		int start, end;

		plot = dmatrixz(0, 6, 0, nummeas-1);  

		for(j = 0, start = -1, end = 0;;) {

			for (start = -1, i = end; i < nummeas; i++) {
				if (multimeas[i][maxband] >= thresh) {
					if (start < 0)
						start = i;
				} else if (start >= 0) {
					end = i;
					break;
				}
			}
			if (start < 0)
				break;
			start -= 3;
			if (start < 0)
				start = 0;
			end += 4;
			if (end > nummeas)
				end = nummeas;
	
			for (i = start; i < end; i++, j++) { 
				int q;

				plot[6][j] = (double)j;
#ifdef NEVER	/* Plot +/-3 around maxband */
				for (q = 0, k = maxband -3; k < (maxband+3) && k >= 0 && k < m->nraw; k++, q++) {
					plot[q][j] = multimeas[i][k]/maxval;
				}
#else
				/* plot max of bands in 6 segments */
				for (q = 0; q < 6; q++) {
					int ss, ee;

					plot[q][j] = -1e60;
					ss = q * (m->nraw/6);
					ee = (q+1) * (m->nraw/6);
					for (k = ss; k < ee; k++) {
						if (multimeas[i][k]/maxval > plot[q][j])
							plot[q][j] = multimeas[i][k]/maxval;
					} 
				}
#endif
			}
		}
		do_plot6(plot[6], plot[0], plot[1], plot[2], plot[3], plot[4], plot[5], j);
		free_dmatrix(plot,0,6,0,nummeas-1);
	}
#endif

	/* Locate the first sample over the threshold, and the */
	/* total number of samples in the pulses. */
	fsampl = -1;
	for (nsampl = i = 0; i < nummeas; i++) {
		for (j = 0; j < m->nraw; j++) {
			if (multimeas[i][j] >= thresh)
				break;
		}
		if (j < m->nraw) {
			if (fsampl < 0)
				fsampl = i;
			nsampl++;
		}
	}
	a1logd(p->log,7,"Number of flash patches = %d\n",nsampl);
	if (nsampl == 0)
		return I1PRO3_RD_NOFLASHES;

	/* See if there are as many samples before the first flash */
	if (nsampl < 6)
		nsampl = 6;

	/* Average nsample samples of ambient */
	i = (fsampl-3-nsampl);
	if (i < 0)
		return I1PRO3_RD_NOAMBB4FLASHES;
	a1logd(p->log,7,"Ambient samples %d to %d \n",i,fsampl-3);
	aavg = dvectorz(-1, m->nraw-1);  
	for (nsampl = 0; i < (fsampl-3); i++) {
		for (j = 0; j < m->nraw; j++)
			aavg[j] += multimeas[i][j];
		nsampl++;
	}

	/* Integrate all the values over the threshold, */
	/* and also one either side of flash */
	for (j = 0; j < m->nraw; j++)
		pavg[j] = 0.0;

	for (k = 0, i = 1; i < (nummeas-1); i++) {
		int sample = 0;
		for (j = 0; j < m->nraw; j++) {
			if (multimeas[i-1][j] >= thresh) {
				sample = 1;
				break;
			}
			if (multimeas[i][j] >= thresh) {
				sample = 1;
				break;
			}
			if (multimeas[i+1][j] >= thresh) {
				sample = 1;
				break;
			}
		}
		if (j < m->nraw) {
			a1logd(p->log,7,"Integrating flash sample no %d \n",i);
			for (j = 0; j < m->nraw; j++)
				pavg[j] += multimeas[i][j];
			k++;
		}
	}
	for (j = 0; j < m->nraw; j++)
		pavg[j] = pavg[j]/(double)k - aavg[j]/(double)nsampl;

	a1logd(p->log,7,"Number of flash patches integrated = %d\n",k);

	finttime = inttime * (double)k;
	if (duration != NULL)
		*duration = finttime;

	/* Convert to cd/m^2 seconds */
	for (j = 0; j < m->nraw; j++)
		pavg[j] *= finttime;

	free_dvector(aavg, -1, m->nraw-1);  

	return I1PRO3_OK;
}

/* =======================================================================*/
/* Lower level reading processing */
/* =======================================================================*/

/* Check a raw black for sanity */
/* Returns nz if not sane */
int i1pro3_multimeas_check_black(
	i1pro3 *p,
	double **rawmmeas,		/* Array of [nummeas][-1 nraw] values to check */
	int nummeas,
	double inttime
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i, j;
	double avg = 0.0;		/* Average raw value */
	double avgd = 0.0;		/* Average dummy value */
	double limit;
	
	if (rawmmeas == NULL)
		return 0;

	for (i = 0; i < nummeas; i++) {
		avgd += rawmmeas[i][-1];
		for (j = 0; j < m->nraw; j++) {
			avg += rawmmeas[i][j];
		}
	}

	avgd /= (double)nummeas;
	avg /= (double)nummeas * m->nraw;
	limit = avgd + m->ee_bk_f_limit + m->ee_bk_v_limit * inttime;

	a1logd(p->log,4,"i1pro3_multimeas_check_black %d meas: avgd %f avg %f limit %f\n",nummeas,avgd,avg,limit);

	if (avg >= limit)
		return 1;
	return 0;
}

/* Subtract black from a raw unscaled multimeas */
void i1pro3_multimeas_sub_black(
	i1pro3 *p,
	double **rawmmeas,		/* Array of [nummeas][nraw] values to subtract from */
	int nummeas,
	double *black			/* Black [nraw] to subtract */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i;
	
	for (i = 0; i < nummeas; i++)
		vect_sub(rawmmeas[i], black, m->nraw);
}

/* Check for saturation of a raw unscaled multimeas */
/* This should be after black subtraction but before linearization. */
/* Returns nz if saturated */
int i1pro3_multimeas_check_sat(
	i1pro3 *p,
	double *pmaxval,		/* If not NULL, return the maximum value */
	double **rawmmeas,		/* Array of [nummeas][nraw] values to check */
	int nummeas
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i, j;
	double maxval = -1e9;
	int satcount = 0;
	
	for (i = 0; i < nummeas; i++) {
		for (j = 0; j < m->nraw; j++) {
			if (rawmmeas[i][j] > maxval)
				maxval = rawmmeas[i][j];
			if (rawmmeas[i][j] > m->sens_sat)
				satcount++;
		}
	}

	a1logd(p->log,6,"i1pro3_multimeas_check_sat: maxval %f satcount %d\n",maxval,satcount);

	if (pmaxval != NULL)
		*pmaxval = maxval;

	/* Count of 10 is hard coded */
	if (satcount > (10 * nummeas))
		return 1;
	return 0;
}

/* Check for consistency of a raw unscaled multimeas */
/* (Don't use on black subtracted black measurement.) */
/* Returns nz if inconsistent */
int i1pro3_multimeas_check_raw_consistency(
	i1pro3 *p,
	double **rawmmeas,		/* Array of [nummeas][nraw] values to check */
	int nummeas
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i;
	double *avgs, aavg, aavgd, maxdev;
	int maxi;
	double limit = PATCH_CONS_THR * m->scan_toll_ratio;
	int rv = 0;

	/* Compute the average raw value of each measurement */
	avgs = dvector(0, nummeas-1);
	for (i = 0; i < nummeas; i++)
		avgs[i] = vect_avg(rawmmeas[i], m->nraw);

	/* Average of averages */
	aavgd = aavg = vect_avg(avgs, nummeas);

	/* Can't expect consistency for very low levels due to noise */
	if (aavgd < 40.0)
		aavgd = 40.0;

	maxdev = 0;
	for (i = 0; i < nummeas; i++) {
		double dev = fabs(avgs[i] - aavg)/aavgd;
		if (dev > maxdev) {
			maxdev = dev;
			maxi = i;
		}
	}

	if (maxdev > limit) {
		a1logd(p->log,1,"i1pro3_multimeas_check_raw_consistency aavg %f aavg %f maxdev %f > %f\n",aavg,aavg,maxdev,limit);
		rv = 1;
	} else {
		a1logd(p->log,5,"i1pro3_multimeas_check_raw_consistency aavg %f aavg %f maxdev %f limit %f\n",aavg,aavg,maxdev,limit);
	}

	free_dvector(avgs, 0, nummeas-1);

	return rv;
}

/* Check for consistency of a raw unscaled multimeas with UV muxed values */
/* Returns nz if inconsistent */
int i1pro3_multimeas_check_raw_consistency_x(
	i1pro3 *p,
	double **rawmmeas,		/* Array of [nummeas][nraw] values to check */
	int nummeas
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i;
	double eavg, oavg, hdif;
	int neavg, noavg;
	double *avgs, aavg, aavgd, maxdev;
	int maxi;
	double limit = PATCH_CONS_THR * m->scan_toll_ratio;
	int rv = 0;

	/* Compute the average raw value of each measurement */
	avgs = dvector(0, nummeas-1);
	for (i = 0; i < nummeas; i++)
		avgs[i] = vect_avg(rawmmeas[i], m->nraw);

	/* Compute even/odd averages */
	eavg = oavg = 0.0;
	neavg = noavg = 0;
	for (i = 0; i < nummeas; i++) {
		if ((i & 1) == 0) {
			eavg += avgs[i];
			neavg++;
		} else {
			oavg += avgs[i];
			noavg++;
		}
	}
	if (neavg == 0 || noavg == 0) {
		a1logd(p->log,1,"i1pro3_multimeas_check_raw_consistency_x: too few patches (%d, %d)\n",neavg, noavg);
		free_dvector(avgs, 0, nummeas-1);
		return 1;
	}
	eavg /= (double)neavg;
	oavg /= (double)noavg;
	hdif = 0.5 * (eavg - oavg);		/* Half even above odd */

	/* Correct averages for average even/odd difference */
	for (i = 0; i < nummeas; i++) {
		if ((i & 1) == 0)
			avgs[i] -= hdif;	
		else
			avgs[i] += hdif;	
	}

	/* Average of averages */
	aavgd = aavg = vect_avg(avgs, nummeas);

	/* Can't expect consistency for very low levels due to noise */
	if (aavgd < 40.0)
		aavgd = 40.0;

	/* Maximum difference from average */
	maxdev = 0;
	for (i = 0; i < nummeas; i++) {
		double dev = fabs(avgs[i] - aavg)/aavgd;
		if (dev > maxdev) {
			maxdev = dev;
			maxi = i;
		}
	}

	if (maxdev > limit) {
		a1logd(p->log,1,"i1pro3_multimeas_check_raw_consistency_x aavg %f aavg %f maxdev %f > %f\n",aavg,aavg,maxdev,limit);
		rv = 1;
	} else {
		a1logd(p->log,5,"i1pro3_multimeas_check_raw_consistency_x aavg %f aavg %f maxdev %f limit %f\n",aavg,aavg,maxdev,limit);
	}

	free_dvector(avgs, 0, nummeas-1);

	return rv;
}

/* Check for consistency of wav values. */
/* Returns nz if inconsistent */
int i1pro3_multimeas_check_wav_consistency(
	i1pro3 *p,
	int highres,		/* 0 for std res, 1 for high res */ 
	double inttime,
	double **wav,		/* Array of [nwav1m][nwav] values to check */
	int nummeas
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i;
	double *avgs, aavg, aavgd, maxdev;
	int nwav = m->nwav[highres];
	int maxi;
	double limit = PATCH_CONS_THR * m->scan_toll_ratio;
	int rv = 0;

	/* Compute the average wav scale wav value of each measurement */
	avgs = dvector(0, nummeas-1);
	for (i = 0; i < nummeas; i++)
		avgs[i] = vect_avg(wav[i], nwav) * inttime;

	/* Average of averages */
	aavgd = aavg = vect_avg(avgs, nummeas);

	/* Can't expect consistency for very low levels due to noise */
	if (aavgd < 40.0)
		aavgd = 40.0;

	maxdev = 0;
	for (i = 0; i < nummeas; i++) {
		double dev = fabs(avgs[i] - aavg)/aavgd;
		if (dev > maxdev) {
			maxdev = dev;
			maxi = i;
		}
	}

	if (maxdev > limit) {
		a1logd(p->log,1,"wav_consistency2 aavg %f aavg %f maxdev %f > %f\n",aavg,aavg,maxdev,limit);
		rv = 1;
	} else {
		a1logd(p->log,6,"wav_consistency2 aavg %f aavg %f maxdev %f limit %f\n",aavg,aavg,maxdev,limit);
	}

	free_dvector(avgs, 0, nummeas-1);

	return rv;
}

/* Check for consistency of a even/odd wav values. */
/* Returns nz if inconsistent */
int i1pro3_multimeas_check_wav_consistency2(
	i1pro3 *p,
	int highres,		/* 0 for std res, 1 for high res */ 
	double inttime,
	double **wav1,		/* Array of [nwav1m][nwav] values to check */
	int nwav1m,
	double **wav2,		/* Array of [nwav2m][nwav] values to check */
	int nwav2m
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i;
	double eavg, oavg, hdif;
	double *avgs, aavg, aavgd, maxdev;
	int nwav = m->nwav[highres];
	int nummeas = nwav1m + nwav2m;
	int maxi;
	double limit = PATCH_CONS_THR * m->scan_toll_ratio;
	int rv = 0;

	if (nwav1m == 0 || nwav2m == 0) {
		a1logd(p->log,1,"i1pro3_multimeas_check_wav_consistency2: too few patches (%d, %d)\n",nwav1m, nwav2m);
		return 1;
	}

	/* Compute the average raw scale wav value of each measurement */
	avgs = dvector(0, nummeas-1);
	for (i = 0; i < nwav1m; i++)
		avgs[i] = vect_avg(wav1[i], nwav) * inttime;
	for (i = 0; i < nwav2m; i++)
		avgs[nwav1m + i] = vect_avg(wav2[i], nwav) * inttime;

	/* Compute even/odd averages */
	eavg = oavg = 0.0;
	for (i = 0; i < nwav1m; i++)
		eavg += avgs[i];
	for (i = 0; i < nwav2m; i++)
		oavg += avgs[nwav1m + i];

	eavg /= (double)nwav1m;
	oavg /= (double)nwav2m;
	hdif = 0.5 * (eavg - oavg);		/* Half even above odd */

	/* Correct averages for average even/odd difference */
	for (i = 0; i < nwav1m; i++)
		avgs[i] -= hdif;
	for (i = 0; i < nwav2m; i++)
		avgs[nwav1m + i] += hdif;

	/* Average of averages */
	aavgd = aavg = vect_avg(avgs, nummeas);

	/* Can't expect consistency for very low levels due to noise */
	if (aavgd < 40.0)
		aavgd = 40.0;

	maxdev = 0;
	for (i = 0; i < nummeas; i++) {
		double dev = fabs(avgs[i] - aavg)/aavgd;
		if (dev > maxdev) {
			maxdev = dev;
			maxi = i;
		}
	}

	if (maxdev > limit) {
		a1logd(p->log,1,"wav_consistency2 aavg %f aavg %f maxdev %f > %f\n",aavg,aavg,maxdev,limit);
		rv = 1;
	} else {
		a1logd(p->log,6,"wav_consistency2 aavg %f aavg %f maxdev %f limit %f\n",aavg,aavg,maxdev,limit);
	}

	free_dvector(avgs, 0, nummeas-1);

	return rv;
}

/* Take a set of raw, black subtracted, unscaled values */
/* and linearize them. */ 
void i1pro3_vect_lin(
	i1pro3 *p,
	double *raw
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	double *polys = m->ee_lin;
	int j;
	
	for (j = 0; j < m->nraw; j++) {
		double fval, lval;

		fval = raw[j];
		lval = ((polys[0] * fval + polys[1]) * fval + polys[2]) * fval + polys[3];
		raw[j] = lval;
	}
} 

/* Take a set of raw, black subtracted, unscaled multimeas values */
/* and linearize them. */ 
void i1pro3_multimeas_lin(
	i1pro3 *p,
	double **rawmmeas,		/* Array of [nummeas][nraw] values to linearize */
	int nummeas
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	double *polys = m->ee_lin;
	int i, j;

	for (i = 0; i < nummeas; i++) {
		for (j = 0; j < m->nraw; j++) {
			double fval, lval;

			fval = rawmmeas[i][j];
			lval = ((polys[0] * fval + polys[1]) * fval + polys[2]) * fval + polys[3];
			rawmmeas[i][j] = lval;
		}
	}
}

/* Normalize a set of raw measurements by dividing by integration time */
void i1pro3_normalize_rawmmeas(
	i1pro3 *p,
	double **rawmmeas,		/* raw measurements to correct array [nummeas][nraw] */
	int nummeas,			/* Number of measurements */
	double int_time			/* Integration time */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i;
	double dd = 1.0/int_time;
	
	/* For each measurement */
	for (i = 0; i < nummeas; i++)
		vect_scale1(rawmmeas[i], dd, m->nraw);
}

/* Average a raw multimeas into a single vector */
/* NOTE averages auxv value too! */
void i1pro3_average_rawmmeas(
	i1pro3 *p,
	double *avg,			/* return average [-1 nraw] */
	double **rawmmeas,		/* Array of [nummeas][-1 nraw] value to average */
	int nummeas
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i;

	vect_set(avg-1, 0.0, m->nraw+1);

	for (i = 0; i < nummeas; i++)
		vect_add(avg-1, rawmmeas[i]-1, m->nraw+1);
		
	vect_scale1(avg-1, 1.0/nummeas, m->nraw+1);
}

/* Average two raw multimeas into a single vector */
void i1pro3_average_rawmmeas_2(
	i1pro3 *p,
	double *avg,			/* return average [-1 nraw] */
	double **rawmmeas1,		/* Array of [nummeas][-1 nraw] value to average */
	int nummeas1,
	double **rawmmeas2,		/* Array of [nummeas][-1 nraw] value to average */
	int nummeas2
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i;

	vect_set(avg-1, 0.0, m->nraw+1);

	for (i = 0; i < nummeas1; i++)
		vect_add(avg-1, rawmmeas1[i]-1, m->nraw+1);

	for (i = 0; i < nummeas2; i++)
		vect_add(avg-1, rawmmeas2[i]-1, m->nraw+1);
		
	vect_scale1(avg-1, 1.0/(nummeas1 + nummeas2), m->nraw+1);
}

/* Average a raw multimeas into odd & even single vectors */
/* (Doesn't average aux values) */
void i1pro3_average_eorawmmeas(
	i1pro3 *p,
	double avg[2][128],		/* return average [2][nraw] */
	double **rawmmeas,		/* Array of [nummeas][nraw] raw values to average */
	int nummeas				/* (must be even) */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i;

	if (nummeas & 1)
		error("i1pro3_average_eorawmmeas: odd nummeas in %s line %d\n",__FILE__,__LINE__);

	vect_set(avg[0], 0.0, m->nraw);
	vect_set(avg[1], 0.0, m->nraw);

	for (i = 0; i < nummeas; i += 2) {
		vect_add(avg[0], rawmmeas[i + 0], m->nraw);
		vect_add(avg[1], rawmmeas[i + 1], m->nraw);
	}
	vect_scale1(avg[0], 0.5/nummeas, m->nraw);
	vect_scale1(avg[1], 0.5/nummeas, m->nraw);
}


/* Average a wav multimeas into a single vector */
void i1pro3_average_wavmmeas(
	i1pro3 *p,
	double *avg,			/* return average [nwav[]] */
	double **wavmmeas,		/* Array of [nummeas][nwav[]] raw values to average */
	int nummeas,
	int hr					/* 0 if standard res., 1 if hires */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i;

	vect_set(avg, 0.0, m->nwav[hr]);

	for (i = 0; i < nummeas; i++)
		vect_add(avg, wavmmeas[i], m->nwav[hr]);
		
	vect_scale1(avg, 1.0/nummeas, m->nwav[hr]);
}

/* Convert an absraw array from raw wavelengths to output wavelenths */
/* for a given [std res, high res] and [emis/tras, reflective] mode */
/* (Copies aux data from raw to wav if refl >= 0) */
void i1pro3_absraw_to_abswav(
	i1pro3 *p,
	int hr,				/* 0 for std res, 1 for high res */ 
	int refl,			/* 0 for emis/trans, 1 for reflective,  2 or 3 for no aux copy. */ 
	double **abswav,	/* Desination array [nummeas] [-x nwav[]] */
	double **absraw,	/* Source array [nummeas] [-x nraw] */
	int nummeas			/* Number of measurements */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i, j, k, cx, sx;
	int refix = refl & 1;
	
	/* For each measurement */
	for (i = 0; i < nummeas; i++) {

		/* For each output wavelength */
		for (cx = j = 0; j < m->nwav[hr]; j++) {
			double oval = 0.0;

			/* For each matrix value */
			sx = m->mtx[hr][refix].index[j];		/* Starting index */
			for (k = 0; k < m->mtx[hr][refix].nocoef[j]; k++, cx++, sx++) {
				oval += m->mtx[hr][refix].coef[cx] * absraw[i][sx];
			}
			abswav[i][j] = oval;
		}

		/* Copy auxiliary data */
		if ((refl & 2) == 0) {
			if (refix)
				vect_cpy(&abswav[i][-9], &absraw[i][-9], 9);
			else 
				vect_cpy(&abswav[i][-1], &absraw[i][-1], 1);
		}
	}
}

/* Apply stray light correction to a wavmmeas */
void i1pro3_straylight(
	i1pro3 *p,
	int hr,				/* 0 for std res, 1 for high res */ 
	double **wavmmeas,	/* wav measurements to correct array [nummeas][nwav[]] */
	int nummeas			/* Number of measurements */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i, j, k;
	double *tm;			/* Temporary array */
	
	tm = dvector(0, m->nwav[hr]-1);

	/* For each measurement */
	for (i = 0; i < nummeas; i++) {

		vect_cpy(tm, wavmmeas[i], m->nwav[hr]);

		for (j = 0; j < m->nwav[hr]; j++) {
			double oval = 0.0;
	
			/* For each matrix value */
			for (k = 0; k < m->nwav[hr]; k++)
				oval += m->straylight[hr][j][k] * tm[k];
			wavmmeas[i][j] = oval;
		}
	}

	free_dvector(tm, 0, m->nwav[hr]-1);
}

/* Take a set of odd/even raw or wav measurements and reshuffle them into */
/* first half block and second half block. */
/* Return nz if malloc failed */
int i1pro3_unshuffle(
	i1pro3 *p,
	double **mmeas,		/* Array of [nummeas][xxx] values to unshuffle */
	int nummeas			/* Must be even */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int s, d;
	double **sp;

	if (nummeas <= 2)
		return 0;			/* Nothing to do */

	if (nummeas & 1)
		error("i1pro3_unshuffle: odd nummeas in %s line %d\n",__FILE__,__LINE__);

	/* We take advantage of our matrix construction to swap pointers. */

	if ((sp = (double **)malloc(sizeof(double *) * nummeas)) == NULL) {
		a1logd(p->log,1,"i1pro3_unshuffle malloc %ld bytes failed\n",sizeof(double *) * nummeas);
		return 1;
	}

	memmove((char *)sp, (char *)mmeas, sizeof(double *) * nummeas);
	
	for (s = 0; s < nummeas; s++) {
		d = (s/2) + ((s & 1) != 0 ? nummeas/2 : 0); /* Slot to put s in */
		mmeas[d] = sp[s];
	}

	free(sp);

	return 0;
}


/* Compute i1pro3 calibration LED model given auxiliary information */
/* from the spectral and other sensors. */
/* We always use std res. for led modelling. */
void i1pro3_comp_ledm(
	i1pro3 *p,
	double **wavmmeas_ledm,		/* Array of [nummeas][36] values to return */
	double **wavmmeas_aux,		/* Array of [nummeas][-9] aux values to use */
	int nummeas,				/* Number of measurements */
	int uv						/* 0 for non-uv, 1 for uv-illuminated LED model */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int i, j, k;

	for (i = 0; i < nummeas; i++) {

		for (j = 0; j < 36; j++) {
			wavmmeas_ledm[i][j] = 0.0;

			/* Each band result is the weighted sum of */
			/* a polynomial model driven by each auxililay value */
			for (k = 0; k < 8; k++) {
				double auxv = wavmmeas_aux[i][-9 + k];
				double val;

				if (k < 2) {
					val = m->ledm_poly[uv][k][0][j]
					    + auxv * m->ledm_poly[uv][k][1][j]
					    + auxv * auxv * m->ledm_poly[uv][k][2][j];
					val *= m->ledm_poly[uv][k][3][j];
				} else {
					val = m->ledm_poly[uv][k][0][j]
					    + auxv * m->ledm_poly[uv][k][1][j];
					val *= m->ledm_poly[uv][k][2][j];
				}
				wavmmeas_ledm[i][j] += val;
			}
		}
	}
}

/* Take a set of odd/even raw measurements, convert to approx. reflectance */
/* and filter out any UV multiplexing noise using a windowed comb filter. */

#define CWIDTH 3		/* [3] +/- comb width */
#undef DEBUG_UVFILT

void i1pro3_filter_uvmux(
	i1pro3 *p,
	double **raw,		/* Array of [nummeas][-9, nraw] values to filter */
	int nummeas,		/* Must be even */
	int hr
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = &m->ms[m->mmode];
	double *tres;			/* Temporary results */
#ifdef DEBUG_UVFILT
	double avgs[2][128];	/* Debug plot */
#endif
	int i, j;

	a1logd(p->log,4,"i1pro3_filter_uvmux called with %d samples\n",nummeas);

	if (nummeas < 2)		/* Hmm. */
		return;
	
	/* Convert to approximate reflection values */
	for (i = 0; i < nummeas; i += 2) {
		vect_div(raw[i + 0], s->calraw_white[0], m->nraw);
		vect_div(raw[i + 1], s->calraw_white[1], m->nraw);
	}

#ifdef DEBUG_UVFILT
	printf("Unfiltered average even, odd:\n");
	i1pro3_average_eorawmmeas(p, avgs, raw, nummeas);
	plot_raw2(avgs[0], avgs[1]);
#endif

	/* Do the filtering */
	tres = dvector(0, nummeas-1);
	if (nummeas >= (2 * CWIDTH + 1)) {				/* If comb filter will fit */
		for (j = 60; j < m->nraw; j++) {			/* For needed bands */
			for (i = 0; i < nummeas; i ++) {		/* Filter each entry in band */
				double eavg = 0.0, oavg = 0.0;
				int neavg = 0, noavg = 0;
				int ii = i - CWIDTH; 		/* Start */
				int jj = i + CWIDTH + 1;	/* End + 1 */
				int kk;

				if (ii < 0)						/* Clip comb */
					ii = 0;
				if (jj > nummeas)
					jj = nummeas;

				for (kk = ii; kk < jj; kk++) {		/* Compute window even/odd averages */
					if ((kk & 1) == 0) {
						eavg += raw[kk][j];
						neavg++;
					} else {
						oavg += raw[kk][j];
						noavg++;
					}
				}
				eavg /= (double)neavg;
				oavg /= (double)noavg;

				if ((i & 1) == 0)					/* Subtract even/odd difference */
					tres[i] = raw[i][j] - 0.0 * (eavg - oavg);
				else
					tres[i] = raw[i][j] - 1.0 * (oavg - eavg);
			}
			for (i = 0; i < nummeas; i++)			/* Transfer result */
				raw[i][j] = tres[i];
		}
	}
	free_dvector(tres, 0, nummeas-1);

#ifdef DEBUG_UVFILT
	printf("Filtered average even, odd:\n");
	i1pro3_average_eorawmmeas(p, avgs, raw, nummeas);
	plot_raw2(avgs[0], avgs[1]);
#endif
}

#undef CWIDTH

/* ----------------------------------------------------------------- */

/* Zebra ruler read handler. */
/* Caller will have setup p->zebra_* values.. */
static int i1pro3_zebra_thread(void *pp) {
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3imp *m = (i1pro3imp *)p->m;

	/* We need to start zebra read after the measurement read, or it */
	/* won't happen at all.. */
#ifdef USE_RD_SYNC
	a1logd(p->log,7,"\ni1pro3_zebra_thread: waiting for measure sync2 0x%x 0x%x\n",&m->rd_sync2,m->rd_sync2.condx);
	p->icom->usb_wait_io(p->icom, &m->rd_sync2);	/* Wait for meas read to start */
	a1logd(p->log,7,"i1pro3_zebra_thread: got measure sync\n");
#else
	msec_sleep(m->trig_delay/2);
#endif

	m->zebra_rv = i1pro3_gatherzebra(p, m->zebra_buf, m->zebra_bsize, &m->zebra_bread);

	return 0;
}

/* Process a buffer of raw zebra values. */
static i1pro3_code i1pro3_zebra_proc( 
	i1pro3 *p,
	int ***p_p2m,		/* If valid, return list of sample indexes in even position order */
	int *p_npos,		/* Number of position slots */
	unsigned char *zeb,	/* Array [numzeb] bytes of zebra ruler samples (0,1,2,3) values */
	int numzeb,			/* Number of zebra ruler samples/bytes.*/
	double inttime,		/* Corresponding measurement integration time */
	int nummeas			/* Number of measures */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int dval[16] = {	/* Convert sensor change into delta value */
		/* 0, 0 */	0,	/* Either change in count or 9 for illegal */
		/* 0, 1 */	-1,
		/* 0, 2 */	+1,
		/* 0, 3 */	9,
		/* 1, 0 */ +1,
		/* 1, 1 */  0,
		/* 1, 2 */  9,
		/* 1, 3 */ -1,
		/* 2, 0 */ -1,
		/* 2, 1 */  9,
		/* 2, 2 */  0,
		/* 2, 3 */ +1,
		/* 3, 0 */  9,
		/* 3, 1 */ +1,
		/* 3, 2 */ -1,
		/* 3, 3 */  0
	};
	int ntrans, nilleg;
	double transpsec;
	int count, mincount, maxcount;
	double maxdist;
	double *t2p_t;		/* Time to position, time [ntrans]  */
	double *t2p_p;		/* Time to position, position [ntrans]  */
#define POSRES 0.3		/* [0.2] Position slot width in mm */
	int **p2m;			/* Array of pointers to sample lists */
						/* list[-2] = index allocation length (ie. not counting first 2 values) */
						/* list[-1] = number of indexes length */
						/* list[0] = First measurement index */
						/* list[0+n] = n'th measurement index */
	int npos;
	int i;

	a1logd(p->log,6,"\ni1pro3_zebra_proc: processing %d zebra values\n",numzeb);
	
	/* First pass to count transitions and check validity */
	count = ntrans = nilleg = 0;
	mincount = maxcount = 0;
	for (i = 0; i < (numzeb-1); i++) {
		int trv = (zeb[i] << 2) | zeb[i+1];
		int vv = dval[trv];
	
		if (vv == 9)
			nilleg++;
		else if (vv != 0) {
			ntrans++;
			count += vv;
			if (count < mincount)
				mincount = count; 
			if (count > maxcount)
				maxcount = count; 
		}
	}

	/* Assume that if there is less than 1 transition per second, that it is invalid */
	transpsec = ntrans/(numzeb * m->intclkp);

	a1logd(p->log,6,"i1pro3_zebra_proc: nilleg %d ntrans %d transpsec %f\n",nilleg,ntrans,transpsec);
	if (nilleg != 0 || ntrans < 10 || transpsec < 1.0) {
		a1logd(p->log,2,"i1pro3_zebra_proc: invalid zebra ruler read\n");
		if (p_p2m != NULL)
			*p_p2m = NULL;
		if (p_npos != NULL)
			*p_npos = 0;
		return I1PRO3_OK;
	}

	a1logd(p->log,6,"i1pro3_zebra_proc: mincount %d maxcount %d\n",mincount,maxcount);

	t2p_t = dvector(0, ntrans);		/* Time to position mapping, time values */
	t2p_p = dvector(0, ntrans);		/* Time to position mapping, position */

	/* Convert so that overall +ve distance is in the direction */
	/* first moved, and that the distances are all +ve */
	t2p_t[0] = 0.0;
	t2p_p[0] = -mincount * 0.5;

	count = ntrans = 0;
	for (i = 0; i < (numzeb-1); i++) {
		int trv = (zeb[i] << 2) | zeb[i+1];
		int vv = dval[trv];
	
		if (vv == 0 || vv == 9)
			continue;

		ntrans++;
		count += vv;

		t2p_t[ntrans] = i * m->intclkp;	/* Time */
		t2p_p[ntrans] = (count - mincount) * 0.5;		/* Position in mm */
	}
	ntrans++;

	maxdist = (maxcount - mincount) * 0.5;

	if ((t2p_p[9] - t2p_p[0]) < 0.0) {		/* If we started moving backwards */
		for (i = 0; i < ntrans; i++)
			t2p_p[i] = maxdist - t2p_p[i];	/* Make distances +ve */
	}

	a1logd(p->log,6,"i1pro3_zebra_proc: maxdist %f\n",maxdist);

	if (p->log->debug >= 6) {
		a1logd(p->log,6,"t2p ix, position, time:\n");
		for (i = 0; i < ntrans; i++)
			a1logd(p->log,6," %d %f %f\n",i,t2p_t[i],t2p_p[i]);
	}

#ifdef PATREC_DEBUG
	printf("Zebra time vs. position:\n");
	do_plot(t2p_t, t2p_p, NULL, NULL, ntrans);
#endif // NEVER

	/* Creae position to time mapping. We create a list in */
	/* increments of 0.2 mm and then add measurement samples */
	/* that fall into each slot. */

	npos = (int)floor(maxdist/POSRES + 1.5);

	if ((p2m = (int **)malloc(sizeof(int *) * npos)) == NULL) {
		a1logd(p->log,1,"i1pro3_zebra_proc malloc %d bytes failed\n",sizeof(int *) * npos);
		return I1PRO3_INT_MALLOC;
	}

	/* Allocate enough space for 10 measurements per 0.5 mm */
	for (i = 0; i < npos; i++) {
		int *ary;
		if ((ary = (int *)malloc(sizeof(int) * (2 + 10))) == NULL) {
			a1logd(p->log,1,"i1pro3_zebra_proc malloc %d bytes failed\n",sizeof(int) * 12);
			return I1PRO3_INT_MALLOC;
		}
		p2m[i] = ary+2;
		p2m[i][-2] = 10;
		p2m[i][-1] = 0;
	}

	/* For each measurement sample, compute center time, */
	/* then interpolate distance */
	for (i = 0; i < nummeas; i++) {
		double tm;
		double ps;	/* Position in mm */
		int slot;

		tm = (i + 0.5) * inttime;	/* Time at center of integration period */
		ps = vect_lerp2(t2p_t, t2p_p, tm, ntrans);
		slot = (int)floor(ps/POSRES + 0.5);

		if (slot < 0 || slot >= npos)
			error("Assert in %s at line %d slot %d outside 0 .. %d\n",__FILE__,__LINE__,slot,npos-1);
		//fprintf(stderr,"Sample %d time %f position %f slot %d\n",i,tm,ps,slot);
		if (p2m[slot][-1] >= p2m[slot][-2]) {
			int *ary;
			int size = 2 * p2m[slot][-2];
			if ((ary = (int *)realloc(p2m[slot]-2, sizeof(int) * (2 + size))) == NULL) {
				a1logd(p->log,1,"1pro3_zebra_proc realloc %d bytes failed\n",sizeof(int) * (2 + size));
				return I1PRO3_INT_MALLOC;
			}
			p2m[slot] = ary+2;
			p2m[slot][-2] = size;
		} 
		p2m[slot][p2m[slot][-1]++] = i;		/* Add to slot list */
	}

	free_dvector(t2p_t, 0, ntrans);
	free_dvector(t2p_p, 0, ntrans);

	/* Dump slot info */
	if (p->log->debug >= 9) {
		a1logd(p->log,9,"Patch zebra slot info:\n");
		for (i = 0; i < npos; i++) {
			int j;
			a1logd(p->log,9," %d:",i); 
			for (j = 0; j < p2m[i][-1]; j++)
				a1logd(p->log,9," %d",p2m[i][j]); 
			a1logd(p->log,9,"\n"); 
		}
	}

	if (p_p2m != NULL)
		*p_p2m = p2m;
	if (p_npos != NULL)
		*p_npos = npos;

	return I1PRO3_OK;
}

#undef POSRES

/* Free up a p2m list returned by i1pro3_zebra_proc() */
void del_zebix_list(int **p2m, int npos) {
	int i;

	for (i = 0; i < npos; i++) {
		if (p2m[i] != NULL)
			free(p2m[i] - 2);
	}
	free(p2m);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Take a measurement and return floating point raw values. */
/* This is core measurement function */
i1pro3_code i1pro3_do_measure(
	i1pro3 *p,
	i1p3_mmode mm,			/* Measurement mode */
	double ***praw,			/* Return array [nummeas][-1, nraw] for emissive, */
							/* Return array [nummeas][-9, nraw] for reflective, */
	int *pnummeas,			/* Number of measurements to make/returned, 0 for scan. */
	double *pinttime, 		/* Integration time to use/used */
	int ***p_p2m,			/* If valid, return zeb list of sample indexes in mm. order */
	int *p_npos				/* Number of position slots */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	int nummeas = *pnummeas;	/* Requested number of measurements */
	double inttime = *pinttime;	/* Requested integration time */
	int intclocks = 0;			/* Integration clocks for emissive */
	int tint = 1;				/* tint for reflective */
	int nnmeas;					/* Number of native measurements */
	int nnskip = 0;				/* Number of native measurements to skip */
	int nsen;					/* Number of 16 bit sample per native measurement */ 
	unsigned char *buf = NULL;	/* Where to read data to */
	int bsize;					/* Bytes available in buffer */
	unsigned char *zbuf = NULL;	/* Where to read zebra ruler data to */
	int zbsize;					/* Bytes read into zebra ruler buffer */
	athread *zebra_th = NULL;	/* Zebra ruler read thread */
	int refmode;
	int scanmode = 0;
	int flags = 0;				/* Command flags */
	int anummeas = 0;			/* Actual number of native measurements taken */
	double **raw;
	unsigned char *zeb;			/* Zebra ruler values to return */
	int numzeb;					/* Number of zebra ruler samples */
	unsigned int errc;
	int i, j;
	i1pro3_code ev = I1PRO3_OK;

	a1logd(p->log,4,"\ni1pro3_do_measure: mm %d nummeas %d inttime %f\n",mm,*pnummeas,*pinttime);

#ifndef ENABLE_ZEBRA
	p_p2m = NULL;
#endif

	if (inttime < m->min_int_time)
		inttime = m->min_int_time; 

	/* Reflective mode */
	if (mm & i1p3cm_refl) {
		refmode = 1;

		nsen = m->nsen2;

		/* Compute integration multiplier */
		/* Valid values are 1, 2, 4, 6, 8, etc. */
		tint = (int)floor(inttime / m->min_int_time + 0.5);
		if (tint > 1)
			tint = 2 * (int)floor(inttime / (2.0 * m->min_int_time) + 0.5);
		else if (tint < 1)
			tint = 1;

		inttime = m->min_int_time * tint;  
		intclocks = (int)floor(inttime/m->intclkp + 0.5);

		a1logd(p->log,4,"i1pro3_do_measure: refl, tint %d inttime %f\n",tint,inttime);

		if (tint == 1 && (nummeas & 1) != 0) {
			error("i1pro3_do_measure: nummeas must be even (tint %d nummeas %d)",tint,nummeas);
		}

		if ((ev = i1pro3_settintmult(p, tint)) != I1PRO3_OK)
			error("i1pro3_settintmult failed with %d\n",ev);

		/* set the illuminating LED currents */
		if (mm & i1p3cm_ill) {
			int led_luv_cur = m->ee_led_luv_cur;
			int led_suv_cur = m->ee_led_suv_cur;

			if (mm == i1p3mm_rf_whl)	/* No long UV */
				led_luv_cur = 0;

			if (mm == i1p3mm_rf_whs)	/* No short UV */
				led_suv_cur = 0;

			if (mm == i1p3mm_rf_whp) {	/* Polarized UV levels */
				if (m->ee_version >= 1) {
					led_luv_cur = m->ee1_pol_led_luv_cur; 
					led_suv_cur = m->ee1_pol_led_suv_cur;
				}
			}

			a1logd(p->log,4,"i1pro3_do_measure: led currents %d %d %d %d %d\n",
			       m->ee_led_w_cur, m->ee_led_b_cur, led_luv_cur, led_suv_cur, m->ee_led_gwl_cur);

			if ((ev = i1pro3_setledcurrents(p,
				m->ee_led_w_cur, m->ee_led_b_cur, led_luv_cur, led_suv_cur, m->ee_led_gwl_cur
			)) != I1PRO3_OK)
				return ev;

			nnskip = LEDTURNONMEAS;	/* Hard coded LED turn on delay in native meas count */

			/* Quantize up when tint */
			if (tint > 1)
				nnskip = (tint/2) * ((nnskip + (tint/2)-1) / (tint/2)); 

			a1logd(p->log,4,"i1pro3_do_measure: nnskip %d\n",nnskip);
		}

		if (mm == i1p3mm_rf_whp) {			/* Polarized ILL flag */
			flags |= I1PRO3_RMF_CILL;
		} else if (mm & i1p3cm_ill)
			flags |= I1PRO3_RMF_ILL;

		if (p_p2m != NULL)
			flags |= I1PRO3_RMF_ZEBRA;

	/* Emissive mode */
	} else {
		refmode = 0;
		nsen = m->nsen1;

		intclocks = (int)floor(inttime/m->intclkp + 0.5);
		inttime = intclocks * m->intclkp;

		if (mm == i1p3mm_em_wl)
			flags |= I1PRO3_EMF_WL;

		a1logd(p->log,4,"i1pro3_do_measure: emis, inttime %f\n",inttime);

		if (p_p2m != NULL)
			flags |= I1PRO3_EMF_ZEBRA;
	}

	/* Allow MAXSCANTIME for a scan */
	if (nummeas == 0) {
		int delay = TRIG_DELAY;		/* Scan start reaction time in msec */
		int dmeas, omeas;			/* Delay and On LED indicator params */

		scanmode = 1;

		nummeas = (int)floor(MAXSCANTIME/inttime + 0.5);
		if (nummeas < 1)
			nummeas = 1;
	
		/* Compute reaction time from trigger_measure() to actial measurement */
		if (mm & i1p3cm_ill) {
			delay += ceil(1000.0 * nnskip * 2.0 * m->min_int_time);
		}
		
		a1logd(p->log,4,"i1pro3_do_measure: trig delay %d msec, reaction delay %d msec\n",
		                                                  TRIG_DELAY, delay); 

		/* Indicate to the user with a (typically) audible indication. */
		if (p->eventcallback != NULL) {
			issue_scan_ready((inst *)p, delay);
		} else {
			msec_beep(delay, 1000, 200);	/* delay then 1KHz for 200 msec */
		}
	
		/* Also use built in LED indicators */
		dmeas = (int)ceil(0.001 * delay/(2.0 * m->min_int_time));	/* Reaction delay */
		omeas = dmeas + (int)ceil(0.2/(2.0 * m->min_int_time));		/* 0.2 sec LED on time */
		if ((ev = i1pro3_setscanstartind(p, dmeas, omeas)) != I1PRO3_OK)
			return ev;

		a1logd(p->log,4,"i1pro3_do_measure: LED ind. dmeas %d omeas %d\n",dmeas, omeas);

	} else {	/* Disable the measure indicator */
		if ((ev = i1pro3_setscanstartind(p, 0, 0)) != I1PRO3_OK)
			return ev;
	}

	/* Number of native measurements */
	if (refmode) {
		/* Two measures per native measure if tint == 1 */
		nnmeas = nummeas * tint / 2;
		nnmeas += nnskip;
	} else {
		nnmeas = nummeas;
	}

	/* Allocate the byte buffer space */
	bsize = 2 * nsen * nnmeas ;

	a1logd(p->log,4,"i1pro3_do_measure: nummeas %d nnmeas %d bsize %d\n",nummeas,nnmeas,bsize);

	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"i1pro3_do_measure malloc %d bytes failed\n",bsize);
		return I1PRO3_INT_MALLOC;
	}

	if (p_p2m != NULL) {
		int mxnzsamp;

		mxnzsamp = nnmeas * (refmode ? 2 : 1) * intclocks;	/* Total number of clocks */ 

		zbsize = (mxnzsamp + 3)/4;		/* 2 bits per clock to bytes */
	
		a1logd(p->log,4,"i1pro3_do_measure: zbsize %d\n",zbsize);
	
		if ((zbuf = (unsigned char *)malloc(sizeof(unsigned char) * zbsize)) == NULL) {
			a1logd(p->log,1, "i1pro3_do_measure malloc %d bytes failed\n",zbsize);
			free(buf);
			return I1PRO3_INT_MALLOC;
		}
	}

	/* Do measurement */
	{
		a1logd(p->log,1,"i1pro3_trigger_measure about to call usb_reinit_cancel()\n");
		usb_reinit_cancel(&m->rd_sync);			/* Prepare to sync rd and trigger */
		a1logd(p->log,1,"i1pro3_trigger_measure done usb_reinit_cancel()\n");
		if (p_p2m != NULL) {
			a1logd(p->log,1,"i1pro3_trigger_measure about to call usb_reinit_cancel(2)\n");
			usb_reinit_cancel(&m->rd_sync2);	/* Prepare to sync rd and trigger */
			a1logd(p->log,1,"i1pro3_trigger_measure done usb_reinit_cancel(2)\n");
		}
	}

	/* Start thread that will trigger measure when signaled by gather */
	if ((ev = i1pro3_trigger_measure(p, refmode, p_p2m != NULL ? 1 : 0,
		        scanmode ? 0 : nnmeas, intclocks, flags, TRIG_DELAY)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_trigger_measure failed with %d\n",ev);
		free(buf);
		return ev;
	}

	/* Start zebra stripe read - this releases measurement trigger. */
	if (p_p2m != NULL) {
		m->zebra_buf = zbuf;
		m->zebra_bsize = zbsize;
		m->zebra_bread = 0;

		if ((zebra_th = new_athread(i1pro3_zebra_thread, (void *)p)) == NULL) {
			a1logd(p->log,1,"i1pro3_trigger_measure new_athread() failed\n");
			free(buf);
			return I1PRO3_INT_THREADFAILED;
		}
	}

	/* Start measurement gather - this releases the zebra read/trigger command... */
	if ((ev = i1pro3_gathermeasurement(p, refmode, p_p2m != NULL ? 1 : 0,
		        scanmode, nnmeas, buf, bsize, &anummeas)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_gathermeasurement failed with %d\n",ev);
		zebra_th->wait(zebra_th);
		zebra_th->del(zebra_th);
		free(buf);
		return ev;
	}

	/* Should do this ? */
	if ((ev = i1pro3_getlasterr(p, &errc)) != I1PRO3_OK) {
		a1logd(p->log,1,"i1pro3_getlasterr failed with %d\n",ev);
		zebra_th->wait(zebra_th);
		zebra_th->del(zebra_th);
		free(buf);
		return ev;
	}

	/* If we had turned the LEDs on, record when they were turned off */
	if (mm == i1p3mm_em_wl
	 || (mm & i1p3cm_ill)) { 
		m->llamponoff = msec_time();
	}

	if (refmode) {
		if (tint > 1)	/* Round down to give number of whole measurements */
			nummeas = (anummeas - nnskip)/(tint / 2);
		else
			nummeas = 2 * (anummeas - nnskip);

		if (nummeas < 0) {
			zebra_th->wait(zebra_th);
			zebra_th->del(zebra_th);
			return I1PRO3_RD_SHORTMEAS;
		}
			
		raw = dmatrix(0, nummeas-1, -9, m->nraw-1);
	} else {
		nummeas = anummeas; 
		raw = dmatrix(0, nummeas-1, -1, m->nraw-1);
	}

	a1logd(p->log,4,"i1pro3_do_measure: nummeas actually done %d\n",nummeas);

	/* Copy all the measurement data */
	for (j = 0; j < nummeas; j++) {
		unsigned char *b;
		double tt;

		if (refmode) {

			/* Just use the odd (second) value from each measurement buffer. */
			if (tint > 1) {
				b = buf + 2 * nsen * (nnskip + j * tint/2 + tint/2-1); 

				/* This illum. auxiliary could be some sort of voltage. */
				/* This is updated with each measurement */
				tt = (double)read_ORD16_le(b + 2 * 0);
				raw[j][-9] = (tt/13107.0 + 6.5)/2.988;

				/* This one looks like a 3490 K NTC resistor */
				/* This is updated with each measurement */
				tt = (double)read_ORD16_le(b + 2 * 2);
				tt = 65535.0/tt - 1.0;
				tt = log(1.0/(tt * exp(-11.70551735703505)));
				raw[j][-8] = 3490.0/tt -273.15;

				/* These seem to be the output of a spectral sensor, i.e. AS7262/3 ? AS7341 ? */
				/* These are updates every 11th min-int period, i.e. about 27.9 msec. */
				for (i = 0; i < 6; i++)
					raw[j][-7+i] = (double)read_ORD16_le(b + 2 * (276 + i));

				/* Only odd measurement is valid */

				/* Average 5 covered cell values. Skip 6th, as it may not be completely covered. */
				raw[j][-1] = 0.0;
				for (i = 0; i < 5; i++)
					raw[j][-1] += (double)read_ORD16_le(b + 2 * (142 + i));
				raw[j][-1] /= 5.0;
			
				/* 128 sensor values */
				for (i = 0; i < m->nraw; i++)
					raw[j][i] = (double)read_ORD16_le(b + 2 * (142 + 6 + i));

			/* Each measurement buffer produces two spectra, the even (first) being */
			/* UV-excluded, and the odd (second) being UV included */
			} else {
				b = buf + 2 * nsen * (nnskip + j/2); 

				/* Even measurement, UV excluded */

				/* This illum. auxiliary could be some sort of voltage. */
				/* This is updated with each measurement */
				tt = (double)read_ORD16_le(b + 2 * 0);
				raw[j][-9] = (tt/13107.0 + 6.5)/2.988;

				/* This one looks like a 3490 K NTC resistor */
				/* This is updated with each measurement */
				tt = (double)read_ORD16_le(b + 2 * 2);
				tt = 65535.0/tt - 1.0;
				tt = log(1.0/(tt * exp(-11.70551735703505)));
				raw[j][-8] = 3490.0/tt -273.15;

				/* These seem to be the output of a spectral sensor, i.e. AS7262/3 ? AS7341 ? */
				/* These are updates every 11th min-int period, i.e. about 27.9 msec. */
				for (i = 0; i < 6; i++)
					raw[j][-7+i] = (double)read_ORD16_le(b + 2 * (276 + i));

				/* Average 5 covered cell values. Skip 6th, as it may not be completely covered. */
				raw[j][-1] = 0.0;
				for (i = 0; i < 5; i++)
					raw[j][-1] += (double)read_ORD16_le(b + 2 * (6 + i));
				raw[j][-1] /= 5.0;
			
				/* 128 sensor values */
				for (i = 0; i < m->nraw; i++) {
					raw[j][i] = (double)read_ORD16_le(b + 2 * (6 + 6 + i));
				}

				/* Odd measurement, UV included */
				j++;

				/* Illum. aux values are the same */
				for (i = -9; i < -1; i++)
					raw[j][+i] = raw[j-1][+i];

				/* Average 5 covered cell values. Skip 6th, as it may not be completely covered. */
				raw[j][-1] = 0.0;
				for (i = 0; i < 5; i++)
					raw[j][-1] += (double)read_ORD16_le(b + 2 * (142 + i));
				raw[j][-1] /= 5.0;
			
				/* 128 sensor values */
				for (i = 0; i < m->nraw; i++) {
					raw[j][i] = (double)read_ORD16_le(b + 2 * (142 + 6 + i));
				}
			}
	
		/* Emissive measurement - no skip */
		} else {
			b = buf + j * 2 * nsen; 

			/* Average 5 covered cell values. Skip 6th, as it may not be completely covered. */
			raw[j][-1] = 0.0;
			for (i = 0; i < 5; i++)
				raw[j][-1] += (double)read_ORD16_le(b + 2 * i);
			raw[j][-1] /= 5.0;
			
			/* 128 sensor values */
			for (i = 0; i < m->nraw; i++)
				raw[j][i] = (double)read_ORD16_le(b + 2 * (6 + i));
		}
	}

	/* Copy and process the zebra ruler data */
	if (p_p2m != NULL) {
		int zskip;
		int ii, jj;

		zebra_th->wait(zebra_th);
		zebra_th->del(zebra_th);

		if (m->zebra_rv != I1PRO3_OK) {
			a1logd(p->log,1,"i1pro3_zebra_thread failed with %d\n",m->zebra_rv);
			return m->zebra_rv;
		}

		/* Allocate a byte for each zebra ruler value */
		/* We expect one zebra bit per clock cycle, */
		/* but in tint mode we can get extra zebra bits after the last */
		/* valid measurement, which we discard. */
		numzeb = nummeas * intclocks;

		if ((zeb = (unsigned char *)malloc(sizeof(unsigned char) * numzeb)) == NULL) {
			a1logd(p->log,1,"i1pro3_do_measure malloc %d bytes failed\n",numzeb);
			free(buf);
			return I1PRO3_INT_MALLOC;
		}

		/* Number of z samples to skip */
		zskip = nnskip * (refmode ? 2 : 1) * m->minintclks;

		/* Copy zebra ruler bits into bytes */
		for (jj = ii = i = 0; i < m->zebra_bread; i++) {
			unsigned int ival, oval;
			int k;

			ival = zbuf[i];
			for (k = 0; k < 4; k++, ii++) {
				oval = ival & 3;
				ival >>= 2;

				if (ii >= zskip) {
					if (jj >= numzeb)
						break;
					zeb[jj] = oval;
					jj++;
				}
			}
			if (k < 4) {
				i++;
				break;
			}
		}
		free(zbuf);

		if (jj != numzeb || i > m->zebra_bread || i < (m->zebra_bread - (2 * intclocks)/8)) {
			error("Assert in %s at %d: zebra/meas mismatch: jj %d != numzeb %d or i %d != bread %d\n",__FILE__,__LINE__,jj,numzeb,i,m->zebra_bread);
			}

		if (p->log->debug >= 9) {
			a1logd(p->log,9,"zebra raw data:\n");
			adump_bytes(p->log, "", zeb, 0, numzeb);
		}

		/* Process the raw data into a list of meas patches at each 0.2 mm location */
		if ((ev = i1pro3_zebra_proc(p, p_p2m, p_npos, zeb, numzeb, inttime, nummeas)) != I1PRO3_OK)
			return ev;

		free(zeb);
	}

	free(buf);

	*praw = raw;				/* Return measurement data we allocated */
	*pinttime = inttime;
	*pnummeas = nummeas;

	return ev;
}

/* Free up the raw matrix returned by i1pro3_do_measure */
void i1pro3_free_raw(
	i1pro3 *p,
	i1p3_mmode mm,			/* Measurement mode */
	double **raw, 			/* Matrix to free */
	int nummeas				/* Number of measurements */
) {
	i1pro3imp *m = (i1pro3imp *)p->m;

	if (raw == NULL)
		return;

	if (mm & i1p3cm_refl) {
		free_dmatrix(raw, 0, nummeas-1, -9, m->nraw-1);
	} else {
		free_dmatrix(raw, 0, nummeas-1, -1, m->nraw-1);
	}
}


