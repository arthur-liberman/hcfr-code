
/*
 * Argyll Color Correction System
 *
 * Gretag i1Pro implementation functions
 *
 * Author: Graeme W. Gill
 * Date:   24/11/2006
 *
 * Copyright 2006 - 2014 Graeme W. Gill
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

/* TTBD:

	Some things probably aren't quite correct:
	The way the sensor saturation and optimal target is
	computed probably doesn't account for the dark level
	correctly, since the targets are in sensor value,
	but the comparison is done after subtracting black ??
	See the Munki implementation for an approach to fix this ??

	It should be possible to add a refresh-display calibration
	routine based on an emissive scan + the auto-correlation
	(see i1d3.c). Whether this will noticably improve repeatibility
	remains to be seen.
*/

/*
	Notes:

	Naming of spectral values:

		sensor  - the 16 bit values from the sensor including any dummy/shielded values
		raw		- the floating point values of the spectral section 
		absraw	- raw after scaling for integration time and gain settings.

	The Rev D seems to die if it is ever given a GET_STATUS. This is why
	the WinUSB driver can't be used with it.

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
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "sort.h"

/* Configuration */
#define ENABLE_2		/* [Def] Enable i1pro2/Rev E driver code, else treat i1pro2 as i1pro */
#undef USE_HIGH_GAIN_MODE /* [Und] Make use of high gain mode in Rev A-D mode */ 
#define USE_THREAD		/* Need to use thread, or there are 1.5 second internal */
						/* instrument delays ! */
#undef WAIT_FOR_DELAY_TRIGGER	/* [Und] Hack to diagnose threading problems */
#undef ENABLE_WRITE		/* [Und] Enable writing of calibration and log data to the EEProm */
#define ENABLE_NONVCAL	/* [Def] Enable saving calibration state between program runs in a file */
#define ENABLE_NONLINCOR	/* [Def] Enable non-linear correction */
#define ENABLE_BKDRIFTC	/* [Def] Enable Emis. Black drift compensation using sheilded cell values */
#define HEURISTIC_BKDRIFTC	/* [Def] Enable heusristic black drift correction */

#define WLCALTOUT (24 * 60 * 60) /* [24 Hrs] Wavelength calibration timeout in seconds */
#define DCALTOUT  (     30 * 60) /* [30 Minutes] Dark Calibration timeout in seconds */
#define DCALTOUT2 ( 1 * 60 * 60) /* [1 Hr] i1pro2 Dark Calibration timeout in seconds */
#define WCALTOUT  ( 1 * 60 * 60) /* [1 Hr] White Calibration timeout in seconds */

#define MAXSCANTIME 20.0	/* [20] Maximum scan time in seconds */
#define SW_THREAD_TIMEOUT	(10 * 60.0) 	/* [10 Min] Switch read thread timeout */

#define SINGLE_READ		/* [Def] Use a single USB read for scan to eliminate latency issues. */
#define HIGH_RES		/* [Def] Enable high resolution spectral mode code. Dissable */
						/* to break dependency on rspl library. */
# undef FAST_HIGH_RES_SETUP	/* Slightly better accuracy ? */

/* Debug [Und] */
#undef DEBUG			/* Turn on debug printfs */
#undef PLOT_DEBUG		/* Use plot to show readings & processing */
#undef PLOT_REFRESH 	/* Plot refresh rate measurement info */
#undef PLOT_UPDELAY		/* Plot data used to determine display update delay */
#undef DUMP_SCANV		/* Dump scan readings to a file "i1pdump.txt" */
#undef DUMP_DARKM		/* Append raw dark readings to file "i1pddump.txt" */
#undef APPEND_MEAN_EMMIS_VAL /* Append averaged uncalibrated reading to file "i1pdump.txt" */
#undef TEST_DARK_INTERP    /* Test out the dark interpolation (need DEBUG for plot) */
#undef PATREC_DEBUG			/* Print & Plot patch/flash recognition information */
#undef PATREC_ALLBANDS		/* Plot all bands of scan */
#undef IGNORE_WHITE_INCONS	/* Ignore define reference reading inconsistency */
#undef HIGH_RES_DEBUG
#undef HIGH_RES_PLOT
#undef ANALIZE_EXISTING		/* Analize the manufacturers existing filter shape */
#undef PLOT_BLACK_SUBTRACT	/* Plot temperature corrected black subtraction */
#undef FAKE_AMBIENT		/* Fake the ambient mode for a Rev A */

#define MATCH_SPOT_OMD			/* [Def] Match Original Manufacturers Driver. Reduce */
								/* integration time and lamp turn on time */

#define DISP_INTT 2.0			/* Seconds per reading in display spot mode */
								/* More improves repeatability in dark colors, but limits */
								/* the maximum brightness level befor saturation. */
								/* A value of 2.0 seconds has a limit of about 110 cd/m^2 */
#define DISP_INTT2 0.8			/* High brightness display spot mode seconds per reading, */
								/* Should be good up to 275 cd/m^2 */
#define DISP_INTT3 0.3			/* High brightness display spot mode seconds per reading, */
								/* Should be good up to 700 cd/m^2 */

#define ADARKINT_MAX 2.0		/* Max cal time for adaptive dark cal */
#define ADARKINT_MAX2 4.0		/* Max cal time for adaptive dark cal Rev E or no high gain */

#define EMIS_SCALE_FACTOR 1.0	/* Emission mode scale factor */ 
#define AMB_SCALE_FACTOR 1.0	/* Ambient mode scale factor for Lux */
//#define AMB_SCALE_FACTOR (1.0/3.141592654)	/* Ambient mode scale factor - convert */ 
//								/* from Lux to Lux/PI */
//								/* These factors get the same behaviour as the GMB drivers. */

#define NSEN_MAX 140			/* Maximum nsen value we can cope with */

/* High res mode settings */
#define HIGHRES_SHORT 370.0		/* i1pro2 uses more of the CCD, */
#define HIGHRES_LONG  730.0		/* leaving less scope for extenion */
#define HIGHRES_WIDTH  (10.0/3.0) /* (The 3.3333 spacing and lanczos2 seems a good combination) */
#define HIGHRES_REF_MIN 375.0	  /* Too much stray light below this in reflective mode */

#include "i1pro.h"
#include "i1pro_imp.h"

/* - - - - - - - - - - - - - - - - - - */
#define LAMP_OFF_TIME 1500		/* msec to make sure lamp is dark for dark measurement */
#define PATCH_CONS_THR 0.1		/* Dark measurement consistency threshold */

#define USE_RD_SYNC				/* Use mutex syncronisation, else depend on TRIG_DELAY */
#define TRIG_DELAY 10			/* Measure trigger delay to allow pending read, msec */

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

/* Plot a CCD spectra */
void plot_raw(double *data) {
	int i;
	double xx[NSEN_MAX];
	double yy[NSEN_MAX];

	if (disdebplot)
		return;

	for (i = 0; i < 128; i++) {
		xx[i] = (double)i;
		yy[i] = data[i];
	}
	do_plot(xx, yy, NULL, NULL, 128);
}

/* Plot two CCD spectra */
void plot_raw2(double *data1, double *data2) {
	int i;
	double xx[128];
	double y1[128];
	double y2[128];

	if (disdebplot)
		return;

	for (i = 0; i < 128; i++) {
		xx[i] = (double)i;
		y1[i] = data1[i];
		y2[i] = data2[i];
	}
	do_plot(xx, y1, y2, NULL, 128);
}

/* Plot a converted spectra */
void plot_wav(i1proimp *m, int hires, double *data) {
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
void plot_wav_2(i1proimp *m, int hires, double *data1, double *data2) {
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

#endif	/* PLOT_DEBUG */

/* ============================================================ */

/* Return a linear interpolated spectral value. Clip to ends */
static double wav_lerp(i1proimp *m, int hires, double *ary, double wl) {
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
	if (bl < 0.0)
		bl = 0;
	else if (bl > 1.0)
		bl = 1.0;

	rv = (1.0 - bl) * ary[jj] + bl * ary[jj+1];

	return rv;
}

/* Same as above, but return cv value on clip */
static double wav_lerp_cv(i1proimp *m, int hires, double *ary, double wl, double cv) {
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

/* ============================================================ */

/* Implementation struct */

/* Add an implementation structure */
i1pro_code add_i1proimp(i1pro *p) {
	i1proimp *m;

	if ((m = (i1proimp *)calloc(1, sizeof(i1proimp))) == NULL) {
		a1logd(p->log,1,"add_i1proimp malloc %ld bytes failed (1)\n",sizeof(i1proimp));
		return I1PRO_INT_MALLOC;
	}
	m->p = p;

	/* EEProm data store */
	if ((m->data = new_i1data(m)) == NULL)
		return I1PRO_INT_CREATE_EEPROM_STORE;
		
	m->lo_secs = 2000000000;		/* A very long time */
	m->msec = msec_time();

	p->m = (void *)m;
	return I1PRO_OK;
}

/* Shutdown instrument, and then destroy */
/* implementation structure */
void del_i1proimp(i1pro *p) {

	a1logd(p->log,5,"i1pro_del called\n");

#ifdef ENABLE_NONVCAL
	/* Touch it so that we know when the instrument was last open */
	i1pro_touch_calibration(p);
#endif /* ENABLE_NONVCAL */

	if (p->m != NULL) {
		int i, j;
		i1proimp *m = (i1proimp *)p->m;
		i1pro_state *s;
		i1pro_code ev;

		if (p->itype != instI1Pro2 && (ev = i1pro_update_log(p)) != I1PRO_OK) {
			a1logd(p->log,2,"i1pro_update_log: Updating the cal and log parameters to"
			                                              " EEProm failed failed\n");
		}

		/* i1pro_terminate_switch() seems to fail on a rev A & Rev C ?? */
		if (m->th != NULL) {		/* Terminate switch monitor thread */
			m->th_term = 1;			/* Tell thread to exit on error */
			i1pro_terminate_switch(p);
			
			for (i = 0; m->th_termed == 0 && i < 5; i++)
				msec_sleep(50);		/* Wait for thread to terminate */
			if (i >= 5) {
				a1logd(p->log,5,"i1pro switch thread termination failed\n");
			}
			m->th->del(m->th);
			usb_uninit_cancel(&m->sw_cancel);		/* Don't need cancel token now */
			usb_uninit_cancel(&m->rd_sync);			/* Don't need sync token now */
			a1logd(p->log,5,"i1pro switch thread terminated\n");
		}

		if (m->trig_thread != NULL) {
			m->trig_thread->del(m->trig_thread);
			a1logd(p->log,5,"i1pro trigger thread terminated\n");
		}

		/* Free any per mode data */
		for (i = 0; i < i1p_no_modes; i++) {
			s = &m->ms[i];

			free_dvector(s->dark_data, -1, m->nraw-1);  
			free_dvector(s->dark_data2, -1, m->nraw-1);  
			free_dvector(s->dark_data3, -1, m->nraw-1);  
			free_dvector(s->white_data, -1, m->nraw-1);
			free_dmatrix(s->idark_data, 0, 3, -1, m->nraw-1);  

			free_dvector(s->cal_factor[0], 0, m->nwav[0]-1);
			free_dvector(s->cal_factor[1], 0, m->nwav[1]-1);
		}

		/* Free EEProm key data */
		if (m->data != NULL)
			m->data->del(m->data);

		/* Free all Rev E and high res raw2wav filters */
		for (i = 0; i < 2; i++) {
			for (j = 0; j < 2; j++) {
				if (m->mtx_c[i][j].index != NULL)
					free(m->mtx_c[i][j].index);
				if (m->mtx_c[i][j].nocoef != NULL)
					free(m->mtx_c[i][j].nocoef);
				if (m->mtx_c[i][j].coef != NULL)
					free(m->mtx_c[i][j].coef);
			}
		}

		/* Free RevE straylight arrays */
		for (i = 0; i < 2; i++) {
			if (m->straylight[i] != NULL)
				free_dmatrix(m->straylight[i], 0, m->nwav[i]-1, 0, m->nwav[i]-1);  
		}

		/* RevA-D high res. recal support */
		if (m->raw2wav != NULL)
			m->raw2wav->del(m->raw2wav);

		free(m);
		p->m = NULL;
	}
}

/* ============================================================ */
/* High level functions */

/* Initialise our software state from the hardware */
i1pro_code i1pro_imp_init(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code ev = I1PRO_OK;
	unsigned char *eeprom;	/* EEProm contents, i1pro = half, i1pro2 = full */
	int len = 8192;

	a1logd(p->log,5,"i1pro_init:\n");

	/* Revert to i1pro if i1pro2 driver is not enabled */
	if (p->itype == instI1Pro2
#ifdef ENABLE_2
	 && getenv("ARGYLL_DISABLE_I1PRO2_DRIVER") != NULL	/* Disabled by environment */
#endif
	) {
		p->itype = instI1Pro;
	}

	if (p->itype != instI1Monitor
	 && p->itype != instI1Pro
	 && p->itype != instI1Pro2)
		return I1PRO_UNKNOWN_MODEL;

	m->trig = inst_opt_trig_user;
	m->scan_toll_ratio = 1.0;

	/* Take conservative approach to when the light was last on. */
	/* Assume it might have been on right before init was called again. */
	m->slamponoff = msec_time();
	m->llampoffon = msec_time();
	m->llamponoff = msec_time();

	if ((ev = i1pro_reset(p, 0x1f)) != I1PRO_OK)
		return ev;

	usb_init_cancel(&m->sw_cancel);			/* Init switch cancel token */
	usb_init_cancel(&m->rd_sync);			/* Init reading sync token */

#ifdef USE_THREAD
	/* Setup the switch monitoring thread */
	if ((m->th = new_athread(i1pro_switch_thread, (void *)p)) == NULL)
		return I1PRO_INT_THREADFAILED;
#endif

	/* Get the current misc. status, fwrev etc */
	if ((ev = i1pro_getmisc(p, &m->fwrev, NULL, &m->maxpve, NULL, &m->powmode)) != I1PRO_OK)
		return ev; 
	a1logd(p->log,2,"Firmware rev = %d, max +ve value = 0x%x\n",m->fwrev, m->maxpve);

	if (p->itype == instI1Pro2 && m->fwrev < 600) {		/* Hmm */
		a1logd(p->log,2, "Strange, firmware isn't up to i1pro2 but has extra pipe..revert to i1pro driver\n",m->fwrev);
		p->itype = instI1Pro;
	}

	/* Get the EEProm size */
	m->eesize = 8192;		/* Rev A..D */
	if (p->itype == instI1Pro2) {
#ifdef NEVER
// ~~99		Hmm. makes it worse. Why ???
//		/* Make sure LED sequence is finished, because it interferes with EEProm read! */
//		if ((ev = i1pro2_indLEDoff(p)) != I1PRO_OK)
//			return ev; 
#endif

		if ((ev = i1pro2_geteesize(p, &m->eesize)) != I1PRO_OK) {
			return ev; 
		}

	}
	
	if (m->eesize < 8192) {
		a1logd(p->log,2,"Strange, EEProm size is < 8192!\n",m->fwrev);
		return I1PRO_HW_EE_SIZE;
	}
	
	if ((eeprom = (unsigned char *)malloc(m->eesize)) == NULL) {
		a1logd(p->log,1,"Malloc %d bytes for eeprom failed\n",m->eesize);
		return I1PRO_INT_MALLOC;
	}

	/* Read the EEProm */
	if ((ev = i1pro_readEEProm(p, eeprom, 0, m->eesize)) != I1PRO_OK) {
		free(eeprom);
		return ev;
	}

	if (p->itype == instI1Pro2) {
		/* Get the Chip ID (This doesn't work until after reading the EEProm !) */
		if ((ev = i1pro2_getchipid(p, m->chipid)) != I1PRO_OK) {
			free(eeprom);
			return ev; 
		}
	}
	
	/* Parse the i1pro data */
	if ((ev = m->data->parse_eeprom(m->data, eeprom, m->eesize, 0)) != I1PRO_OK) {
		free(eeprom);
		return ev;
	}

	/* Parse the i1pro2 extra data */
	if (p->itype == instI1Pro2) {
		if ((ev = m->data->parse_eeprom(m->data, eeprom, m->eesize, 1)) != I1PRO_OK) {
			free(eeprom);
			return ev;
		}
	}
	free(eeprom); eeprom = NULL;

	/* Setup various calibration parameters from the EEprom */
	{
		int *ip, i, xcount;
		unsigned int count;
		double *dp;

		/* Information about the instrument */

		if ((ip = m->data->get_ints(m->data, &count, key_serno)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->serno = ip[0];
		a1logd(p->log,2,"Serial number = %d\n",m->serno);
		sprintf(m->sserno,"%ud",m->serno);

		if ((ip = m->data->get_ints(m->data, &count, key_dom)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->dom = ip[0];
		a1logd(p->log,2, "Date of manufactur = %d-%d-%d\n",
		                          m->dom/1000000, (m->dom/10000) % 100, m->dom % 10000);

		if ((ip = m->data->get_ints(m->data, &count, key_cpldrev)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->cpldrev = ip[0];
		a1logd(p->log,2,"CPLD rev = %d\n",m->cpldrev);

		if ((ip = m->data->get_ints(m->data, &count, key_capabilities)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->capabilities = ip[0];
		if (m->capabilities & 0x6000)		/* Has ambient */
			m->capabilities2 |= I1PRO_CAP2_AMBIENT;	/* Mimic in capabilities2 */
		a1logd(p->log,2,"Capabilities flag = 0x%x\n",m->capabilities);
		if (m->capabilities & 0x6000)
			a1logd(p->log,2," Can read ambient\n");

		if ((ip = m->data->get_ints(m->data, &count, key_physfilt)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->physfilt = ip[0];
		if (m->physfilt == 0x82)
			m->capabilities2 |= I1PRO_CAP2_UV_FILT;	/* Mimic in cap 2 */
		a1logd(p->log,2,"Physical filter flag = 0x%x\n",m->physfilt);
		if (m->physfilt == 0x80)
			a1logd(p->log,2," No filter fitted\n");
		else if (m->physfilt == 0x81)
			a1logd(p->log,2," Emission only ??\n");
		else if (m->physfilt == 0x82)
			a1logd(p->log,2," Is fitted with Ultra Violet Filter\n");

		/* Underlying calibration information */

		m->nsen = 128;
		m->nraw = 128;
		if (p->itype == instI1Pro2) {
			int clkusec, subdiv, xraw, nraw;
			if ((ev = i1pro2_getmeaschar(p, &clkusec, &xraw, &nraw, &subdiv)) != I1PRO_OK)
				return ev;
			m->intclkp2 = clkusec * 1e-6;	/* Rev E integration clock period, ie. 36 usec */
			m->subclkdiv2 = subdiv;			/* Rev E sub clock divider, ie. 136 */

			m->nsen = nraw + xraw;
			if (clkusec != 36 || xraw != 6 || nraw != 128 || subdiv != 136)
				return I1PRO_HW_UNEX_SPECPARMS;

			if (m->nsen > NSEN_MAX)		/* Static allocation assumed */
				return I1PRO_HW_UNEX_SPECPARMS;
		}
		if (m->data->get_ints(m->data, &m->nwav[0], key_mtx_index) == 0)
			return I1PRO_HW_CALIBINFO;
		if (m->nwav[0] != 36)
			return I1PRO_HW_CALIBINFO;
		m->wl_short[0] = 380.0;		/* Normal res. range */
		m->wl_long[0] = 730.0;

		/* Fill high res in too */
		m->wl_short[1] = HIGHRES_SHORT;
		m->wl_long[1] = HIGHRES_LONG;
		m->nwav[1] = (int)((m->wl_long[1]-m->wl_short[1])/HIGHRES_WIDTH + 0.5) + 1;	

		if ((dp = m->data->get_doubles(m->data, &count, key_hg_factor)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->highgain = dp[0];
		a1logd(p->log,2,"High gain         = %.10f\n",m->highgain);

		if ((m->lin0 = m->data->get_doubles(m->data, &m->nlin0, key_ng_lin)) == NULL
		                                                               || m->nlin0 < 1)
			return I1PRO_HW_CALIBINFO;

		if ((m->lin1 = m->data->get_doubles(m->data, &m->nlin1, key_hg_lin)) == NULL
		                                                               || m->nlin1 < 1)
			return I1PRO_HW_CALIBINFO;

		if (p->log->debug >= 2) {
			char oline[200] = { '\000' }, *bp = oline;
	
			bp += sprintf(bp,"Normal non-lin    =");
			for(i = 0; i < m->nlin0; i++)
				bp += sprintf(bp," %1.10f",m->lin0[i]);
			bp += sprintf(bp,"\n");
			a1logd(p->log,2,oline);
	
			bp = oline;
			bp += sprintf(bp,"High Gain non-lin =");
			for(i = 0; i < m->nlin1; i++)
				bp += sprintf(bp," %1.10f",m->lin1[i]);
			bp += sprintf(bp,"\n");
			a1logd(p->log,2,oline);
		}

		if ((dp = m->data->get_doubles(m->data, &count, key_min_int_time)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->min_int_time = dp[0];

		/* And then override it */
		if (p->itype == instI1Pro2) {
			m->min_int_time = m->subclkdiv2 * m->intclkp2;		/* 0.004896 */
		} else {
			if (m->fwrev >= 301)
				m->min_int_time = 0.004716;
			else
				m->min_int_time = 0.00884;		/* == 1 sub clock */
		}

		if ((dp = m->data->get_doubles(m->data, &count, key_max_int_time)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->max_int_time = dp[0];


		if ((m->mtx_o.index = m->data->get_ints(m->data, &count, key_mtx_index)) == NULL
		                                                                   || count != m->nwav[0])
			return I1PRO_HW_CALIBINFO;

		if ((m->mtx_o.nocoef = m->data->get_ints(m->data, &count, key_mtx_nocoef)) == NULL
		                                                                   || count != m->nwav[0])
			return I1PRO_HW_CALIBINFO;

		for (xcount = i = 0; i < m->nwav[0]; i++)	/* Count number expected in matrix coeffs */
			xcount += m->mtx_o.nocoef[i];

		if ((m->mtx_o.coef = m->data->get_doubles(m->data, &count, key_mtx_coef)) == NULL
		                                                                    || count != xcount)
			return I1PRO_HW_CALIBINFO;

		if ((m->white_ref[0] = m->data->get_doubles(m->data, &count, key_white_ref)) == NULL
		                                                                   || count != m->nwav[0]) {
			if (p->itype != instI1Monitor)
				return I1PRO_HW_CALIBINFO;
			m->white_ref[0] = NULL;
		}

		if ((m->emis_coef[0] = m->data->get_doubles(m->data, &count, key_emis_coef)) == NULL
		                                                                   || count != m->nwav[0])
			return I1PRO_HW_CALIBINFO;

		if ((m->amb_coef[0] = m->data->get_doubles(m->data, &count, key_amb_coef)) == NULL
		                                                                   || count != m->nwav[0]) {
			if (p->itype != instI1Monitor
			 && m->capabilities & 0x6000)		/* Expect ambient calibration */
				return I1PRO_HW_CALIBINFO;
			m->amb_coef[0] = NULL;
		}
		/* Default to original EEProm raw to wav filters values*/
		m->mtx[0][0] = m->mtx_o;	/* Std res reflective */
		m->mtx[0][1] = m->mtx_o;	/* Std res emissive */

		if ((ip = m->data->get_ints(m->data, &count, key_sens_target)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->sens_target = ip[0];

		if ((ip = m->data->get_ints(m->data, &count, key_sens_dark)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->sens_dark = ip[0];

		if ((ip = m->data->get_ints(m->data, &count, key_ng_sens_sat)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->sens_sat0 = ip[0];

		if ((ip = m->data->get_ints(m->data, &count, key_hg_sens_sat)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->sens_sat1 = ip[0];

		a1logd(p->log,2,"sens_target %d, sens_dark %d, sens_sat0 %d, sens_sat1 %d\n",
		                      m->sens_target, m->sens_dark, m->sens_sat0, m->sens_sat1);

		/* Then read the log data. Don't fatal error if there is a problem with this. */
		for (;;) {

			/* Total Measure (Emis/Remis/Ambient/Trans/Cal) count */
			if ((ip = m->data->get_ints(m->data, &count, key_meascount)) == NULL || count < 1)
				break;
			m->meascount = ip[0];

			/* Remspotcal last calibration date */
			if ((ip = m->data->get_ints(m->data, &count, key_caldate)) == NULL || count < 1)
				break;
			m->caldate = ip[0];

			/* Remission spot measure count at last Remspotcal. */
			if ((ip = m->data->get_ints(m->data, &count, key_calcount)) == NULL || count < 1)
				break;
			m->calcount = ip[0];

			/* Last remision spot reading integration time */
			if ((dp = m->data->get_doubles(m->data, &count, key_rpinttime)) == NULL || count < 1)
				break;
			m->rpinttime = dp[0];

			/* Remission spot measure count */
			if ((ip = m->data->get_ints(m->data, &count, key_rpcount)) == NULL || count < 1)
				break;
			m->rpcount = ip[0];

			/* Remission scan measure count (??) */
			if ((ip = m->data->get_ints(m->data, &count, key_acount)) == NULL || count < 1)
				break;
			m->acount = ip[0];

			/* Total lamp usage time in seconds (??) */
			if ((dp = m->data->get_doubles(m->data, &count, key_lampage)) == NULL || count < 1)
				break;
			m->lampage = dp[0];
			a1logd(p->log,3,"Read log information OK\n");

			break;
		}
	}

	/* Read Rev E specific keys */
	if (p->itype == instI1Pro2) {
		int i, j;
		double *dp;
		int *sip;
		unsigned int count;
		int *ip;

		/* Capability bits */
		if ((ip = m->data->get_ints(m->data, &count, key2_capabilities)) == NULL
		                                                                  || count != 1)
			return I1PRO_HW_CALIBINFO;
		m->capabilities2 = *ip;
		if (p->log->debug >= 2) {
			a1logd(p->log,2,"Capabilities2 flag = 0x%x\n",m->capabilities2);
			if (m->capabilities2 & I1PRO_CAP2_AMBIENT)
				a1logd(p->log,2," Can read ambient\n");
			if (m->capabilities2 & I1PRO_CAP2_WL_LED)
				a1logd(p->log,2," Has Wavelength Calibration LED\n");
			if (m->capabilities2 & I1PRO_CAP2_UV_LED)
				a1logd(p->log,2," Has Ultra Violet LED\n");
			if (m->capabilities2 & I1PRO_CAP2_ZEB_RUL)
				a1logd(p->log,2," Has Zebra Ruler sensor\n");
			if (m->capabilities2 & I1PRO_CAP2_IND_LED)
				a1logd(p->log,2," Has user indicator LEDs\n");
			if (m->capabilities2 & I1PRO_CAP2_UV_FILT)
				a1logd(p->log,2," Is fitted with Ultra Violet Filter\n");
		}

		if (m->capabilities2 & I1PRO_CAP2_WL_LED) {
			/* wavelength LED calibration integration time (0.56660) */
			if ((dp = m->data->get_doubles(m->data, &count, key2_wlcal_intt)) == NULL
			                                                                  || count != 1)
				return I1PRO_HW_CALIBINFO;
			m->wl_cal_inttime = *dp;
	
			/* Wavelength calibration minimum level */
			if ((ip = m->data->get_ints(m->data, &count, key2_wlcal_minlev)) == NULL
			                                                                  || count != 1)
				return I1PRO_HW_CALIBINFO;
			/* Normalize it to 1.0 seconds (ie. 500/0.56660) */
			m->wl_cal_min_level = (double)(*ip) / m->wl_cal_inttime;
	
			/* wavelength LED measurement expected FWHM in nm */
			if ((dp = m->data->get_doubles(m->data, &count, key2_wlcal_fwhm)) == NULL
			                                                                  || count != 1)
				return I1PRO_HW_CALIBINFO;
			m->wl_cal_fwhm = *dp;
	
			/* wavelength LED measurement FWHM tollerance in nm */
			if ((dp = m->data->get_doubles(m->data, &count, key2_wlcal_fwhm_tol)) == NULL
			                                                                  || count != 1)
				return I1PRO_HW_CALIBINFO;
			m->wl_cal_fwhm_tol = *dp;
	
			/* wavelength LED reference spectrum */
			if ((m->wl_led_spec = m->data->get_doubles(m->data, &m->wl_led_count, key2_wlcal_spec)) == NULL)
				return I1PRO_HW_CALIBINFO;
	
			/* wavelength LED spectraum reference offset */
			if ((ip = m->data->get_ints(m->data, &count, key2_wlcal_ooff)) == NULL
			                                                                  || count != 1)
				return I1PRO_HW_CALIBINFO;
			m->wl_led_ref_off = *ip;
			/* Hmm. this is odd, but it doesn't work correctly otherwise... */
			m->wl_led_ref_off--;
	
			/* wavelength calibration maximum error */
			if ((dp = m->data->get_doubles(m->data, &count, key2_wlcal_max)) == NULL
			                                                                  || count != 1)
				return I1PRO_HW_CALIBINFO;
			m->wl_err_max = *dp;
		}

		/* CCD bin to wavelength polinomial */
		if ((m->wlpoly1 = m->data->get_doubles(m->data, &count, key2_wlpoly_1)) == NULL || count != 4)
			return I1PRO_HW_CALIBINFO;

		if ((m->wlpoly2 = m->data->get_doubles(m->data, &count, key2_wlpoly_2)) == NULL || count != 4)
			return I1PRO_HW_CALIBINFO;

		/* Stray light compensation. Note that 16 bit numbers are signed. */
		if ((sip = m->data->get_shorts(m->data, &count, key2_straylight)) == NULL
		                                                          || count != (36 * 36))
			return I1PRO_HW_CALIBINFO;

		/* stray light scale factor */
		if ((dp = m->data->get_doubles(m->data, &count, key2_straylight_scale)) == NULL
		                                                                  || count != 1)
			return I1PRO_HW_CALIBINFO;

		/* Convert from ints to floats */
		m->straylight[0] = dmatrixz(0, 35, 0, 35);  
		for (i = 0; i < 36; i++) {
			for (j = 0; j < 36; j++) {
				m->straylight[0][i][j] = *dp * sip[i * 36 + j]/32767.0;
				if (i == j)
					m->straylight[0][i][j] += 1.0;
			}
		
		}

		if (p->log->debug >= 7) {
			a1logd(p->log,7,"Stray Light matrix:\n");
			for(i = 0; i < 36; i++) {
				double sum = 0.0;
				for (j = 0; j < 36; j++) {
					sum += m->straylight[0][i][j];
					a1logd(p->log,7,"  Wt %d = %f\n",j, m->straylight[0][i][j]);
				}
				a1logd(p->log,7,"  Sum = %f\n",sum);
			}
		}

#ifdef NEVER
	/* Plot raw2wav polinomials for Rev E */
	{
		double *xx;
		double *y1, *y2;		/* Rev E poly1 and poly2 */
		double d1, d2;
		int i, k;

		xx = dvector(0, m->nraw);		/* X index = raw bin */
		y1 = dvector(0, m->nraw);		/* Y = nm */
		y2 = dvector(0, m->nraw);		/* Y = nm */
		
		d1 = d2 = 0.0;
		for (i = 0; i < m->nraw; i++) {
			double iv, v1, v2;
			xx[i] = i;

			iv = (double)(128-i);
	
			for (v1 = m->wlpoly1[4-1], k = 4-2; k >= 0; k--)
				v1 = v1 * iv + m->wlpoly1[k];
			y1[i] = v1;
			d1 += fabs(y2[i] - yy[i]);
	
			for (v2 = m->wlpoly2[4-1], k = 4-2; k >= 0; k--)
				v2 = v2 * iv + m->wlpoly2[k];
			y2[i] = v2;
			d2 += fabs(y3[i] - yy[i]);

//			printf("ix %d, poly1 %f, poly2 %f, del12 %f\n",i, y1[i], y2[i], y2[i] - y1[i]);
		}
		printf("Avge del of poly1 = %f, poly2 = %f\n",d1/128.0, d2/128.0);

		printf("CCD bin to wavelength mapping of RevE polinomial:\n");
		do_plot6(xx, y1, y2, NULL, NULL, NULL, NULL, m->nraw);
		free_dvector(xx, 0, m->nraw);
		free_dvector(y1, 0, m->nraw);
		free_dvector(y2, 0, m->nraw);
	}
#endif

	}

	/* Set up the current state of each mode */
	{
		int i, j;
		i1pro_state *s;

		/* First set state to basic configuration */
		/* (We assume it's been zero'd) */
		for (i = 0; i < i1p_no_modes; i++) {
			s = &m->ms[i];

			s->mode = i;

			/* Default to an emissive configuration */
			s->targoscale = 1.0;	/* Default full scale */
			s->targmaxitime = 2.0; /* Maximum integration time to aim for */
			s->targoscale2 = 0.15;	 /* Proportion of targoscale to meed etargmaxitime2 (!higain) */
			s->gainmode = 0;		/* Normal gain mode */

			s->inttime = 0.5;		/* Integration time */
			s->lamptime = 0.50;		/* Lamp turn on time (up to 1.0 sec is better, */

			s->wl_valid = 0;
			s->wl_led_off = m->wl_led_ref_off;

			s->dark_valid = 0;		/* Dark cal invalid */
			s->dark_data = dvectorz(-1, m->nraw-1);  
			s->dark_data2 = dvectorz(-1, m->nraw-1);  
			s->dark_data3 = dvectorz(-1, m->nraw-1);  

			s->cal_valid = 0;		/* Scale cal invalid */
			s->cal_factor[0] = dvectorz(0, m->nwav[0]-1);
			s->cal_factor[1] = dvectorz(0, m->nwav[1]-1);
			s->white_data = dvectorz(-1, m->nraw-1);

			s->idark_valid = 0;		/* Dark cal invalid */
			s->idark_data = dmatrixz(0, 3, -1, m->nraw-1);  

			s->min_wl = 0.0;		/* No minimum by default */

			s->dark_int_time  = DISP_INTT;	/* 2.0 */
			s->dark_int_time2 = DISP_INTT2;	/* 0.8 */
			s->dark_int_time3 = DISP_INTT3;	/* 0.3 */

			s->idark_int_time[0] = s->idark_int_time[2] = m->min_int_time;
			if (p->itype == instI1Pro2) {
				s->idark_int_time[1] = s->idark_int_time[3] = ADARKINT_MAX2; /* 4.0 */
			} else {
#ifdef USE_HIGH_GAIN_MODE
				s->idark_int_time[1] = s->idark_int_time[3] = ADARKINT_MAX; /* 2.0 */
#else
				s->idark_int_time[1] = s->idark_int_time[3] = ADARKINT_MAX2; /* 4.0 */
#endif
			}

			s->want_calib = 1;		/* Do an initial calibration */
			s->want_dcalib = 1;
		}

		/* Then add mode specific settings */
		for (i = 0; i < i1p_no_modes; i++) {
			s = &m->ms[i];
			switch(i) {
				case i1p_refl_spot:
					s->targoscale = 1.0;		/* Optimised sensor scaling to full */
					s->reflective = 1;
					s->adaptive = 1;
					s->inttime = 0.02366;		/* Should get this from the log ?? */ 
					s->dark_int_time = s->inttime;

					s->dadaptime = 0.10;
					s->wadaptime = 0.10;
#ifdef MATCH_SPOT_OMD
					s->lamptime = 0.18;			/* Lamp turn on time, close to OMD */
												/* (Not ideal, but partly compensated by calib.) */
												/* (The actual value the OMD uses is 0.20332) */
					s->dcaltime = 0.05;			/* Longer than the original driver for lower */
												/* noise, and lamptime has been reduces to */
												/* compensate. (OMD uses 0.014552) */
					s->wcaltime = 0.05;
					s->dreadtime = 0.05;
					s->wreadtime = 0.05;
#else
					s->lamptime = 0.5;			/* This should give better accuracy, and better */
					s->dcaltime = 0.5;			/* match the scan readings. Difference is about */
					s->wcaltime = 0.5;			/* 0.1 DE though, but would be lost in the */
					s->dreadtime = 0.5;			/* repeatability noise... */
					s->wreadtime = 0.5;
#endif
					s->maxscantime = 0.0;
					s->min_wl = HIGHRES_REF_MIN;/* Too much stray light below this */
												/* given low illumination < 375nm */
					break;
				case i1p_refl_scan:
					s->reflective = 1;
					s->scan = 1;
					s->adaptive = 1;
					s->inttime = m->min_int_time;	/* Maximize scan rate */
					s->dark_int_time = s->inttime;
					if (m->fwrev >= 301)			/* (We're not using scan targoscale though) */
						s->targoscale = 0.25;
					else
						s->targoscale = 0.5;

					s->lamptime = 0.5;		/* Lamp turn on time - lots to match scan */
					s->dadaptime = 0.10;
					s->wadaptime = 0.10;
					s->dcaltime = 0.5;
					s->wcaltime = 0.5;
					s->dreadtime = 0.10;
					s->wreadtime = 0.10;
					s->maxscantime = MAXSCANTIME;
					s->min_wl = HIGHRES_REF_MIN;	/* Too much stray light below this */
					break;

				case i1p_emiss_spot_na:			/* Emissive spot not adaptive */
					s->targoscale = 0.90;		/* Allow extra 10% margine for drift */
					for (j = 0; j < m->nwav[0]; j++)
						s->cal_factor[0][j] = EMIS_SCALE_FACTOR * m->emis_coef[0][j];
					s->cal_valid = 1;
					s->emiss = 1;
					s->adaptive = 0;

					s->inttime = DISP_INTT;		/* Default disp integration time (ie. 2.0 sec) */
					s->lamptime = 0.20;			/* ???? */
					s->dark_int_time = s->inttime;
					s->dark_int_time2 = DISP_INTT2;	/* Alternate disp integration time (ie. 0.8) */
					s->dark_int_time3 = DISP_INTT3;	/* Alternate disp integration time (ie. 0.3) */

					s->dadaptime = 0.0;
					s->wadaptime = 0.10;
					s->dcaltime = DISP_INTT;		/* ie. determines number of measurements */
					s->dcaltime2 = DISP_INTT2 * 2;	/* Make it 1.6 seconds (ie, 2 x 0.8 seconds) */
					s->dcaltime3 = DISP_INTT3 * 3;	/* Make it 0.9 seconds (ie, 3 x 0.3 seconds) */
					s->wcaltime = 0.0;
					s->dreadtime = 0.0;
					s->wreadtime = DISP_INTT;
					s->maxscantime = 0.0;
					break;
				case i1p_emiss_spot:
					s->targoscale = 0.90;		/* Allow extra 10% margine for drift */
					for (j = 0; j < m->nwav[0]; j++)
						s->cal_factor[0][j] = EMIS_SCALE_FACTOR * m->emis_coef[0][j];
					s->cal_valid = 1;
					s->emiss = 1;
					s->adaptive = 1;

					s->lamptime = 0.20;			/* ???? */
					s->dadaptime = 0.0;
					s->wadaptime = 0.10;
					s->dcaltime = 1.0;
					s->wcaltime = 0.0;
					s->dreadtime = 0.0;
					s->wreadtime = 1.0;
					s->maxscantime = 0.0;
					break;
				case i1p_emiss_scan:
					for (j = 0; j < m->nwav[0]; j++)
						s->cal_factor[0][j] = EMIS_SCALE_FACTOR * m->emis_coef[0][j];
					s->cal_valid = 1;
					s->emiss = 1;
					s->scan = 1;
					s->adaptive = 1;			/* ???? */
					s->inttime = m->min_int_time;	/* Maximize scan rate */
					s->lamptime = 0.20;			/* ???? */
					s->dark_int_time = s->inttime;
					if (m->fwrev >= 301)
						s->targoscale = 0.25;		/* (We're not using scan targoscale though) */
					else
						s->targoscale = 0.5;

					s->dadaptime = 0.0;
					s->wadaptime = 0.10;
					s->dcaltime = 1.0;
					s->wcaltime = 0.0;
					s->dreadtime = 0.0;
					s->wreadtime = 0.10;
					s->maxscantime = MAXSCANTIME;
					break;

				case i1p_amb_spot:
#ifdef FAKE_AMBIENT
					for (j = 0; j < m->nwav[0]; j++)
						s->cal_factor[0][j] = EMIS_SCALE_FACTOR * m->emis_coef[0][j];
					s->cal_valid = 1;
#else
					if (m->amb_coef[0] != NULL) {
						for (j = 0; j < m->nwav[0]; j++)
							s->cal_factor[0][j] = AMB_SCALE_FACTOR * m->emis_coef[0][j] * m->amb_coef[0][j];
						s->cal_valid = 1;
					}
#endif
					s->emiss = 1;
					s->ambient = 1;
					s->adaptive = 1;

					s->lamptime = 0.20;			/* ???? */
					s->dadaptime = 0.0;
					s->wadaptime = 0.10;
					s->dcaltime = 1.0;
					s->wcaltime = 0.0;
					s->dreadtime = 0.0;
					s->wreadtime = 1.0;
					s->maxscantime = 0.0;
					break;
				case i1p_amb_flash:
				/* This is intended for measuring flashes */
#ifdef FAKE_AMBIENT
					for (j = 0; j < m->nwav[0]; j++)
						s->cal_factor[0][j] = EMIS_SCALE_FACTOR * m->emis_coef[0][j];
					s->cal_valid = 1;
#else
					if (m->amb_coef[0] != NULL) {
						for (j = 0; j < m->nwav[0]; j++)
							s->cal_factor[0][j] = AMB_SCALE_FACTOR * m->emis_coef[0][j] * m->amb_coef[0][j];
						s->cal_valid = 1;
					}
#endif
					s->emiss = 1;
					s->ambient = 1;
					s->scan = 1;
					s->adaptive = 0;
					s->flash = 1;

					s->inttime = m->min_int_time;	/* Maximize scan rate */
					s->lamptime = 0.20;			/* ???? */
					s->dark_int_time = s->inttime;
					if (m->fwrev >= 301)
						s->targoscale = 0.25;		/* (We're not using scan targoscale though) */
					else
						s->targoscale = 0.5;

					s->dadaptime = 0.0;
					s->wadaptime = 0.10;
					s->dcaltime = 1.0;
					s->wcaltime = 0.0;
					s->dreadtime = 0.0;
					s->wreadtime = 0.12;
					s->maxscantime = MAXSCANTIME;
					break;

				case i1p_trans_spot:
					s->trans = 1;
					s->adaptive = 1;

					s->lamptime = 0.20;			/* ???? */
					s->dadaptime = 0.10;
					s->wadaptime = 0.10;
					s->dcaltime = 1.0;
					s->wcaltime = 1.0;
					s->dreadtime = 0.0;
					s->wreadtime = 1.0;
					s->maxscantime = 0.0;
					s->min_wl = HIGHRES_REF_MIN;	/* Too much stray light below this */
					break;
				case i1p_trans_scan:
					s->trans = 1;
					s->scan = 1;
					s->adaptive = 0;
					s->inttime = m->min_int_time;	/* Maximize scan rate */
					s->dark_int_time = s->inttime;
					if (m->fwrev >= 301)			/* (We're not using scan targoscale though) */
						s->targoscale = 0.25;
					else
						s->targoscale = 0.5;

					s->lamptime = 0.20;			/* ???? */
					s->dadaptime = 0.10;
					s->wadaptime = 0.10;
					s->dcaltime = 1.0;
					s->wcaltime = 1.0;
					s->dreadtime = 0.00;
					s->wreadtime = 0.10;
					s->maxscantime = MAXSCANTIME;
					s->min_wl = HIGHRES_REF_MIN;	/* Too much stray light below this */
					break;
			}
		}
	}

	if (p->itype != instI1Monitor		/* Monitor doesn't have reflective cal */
	 && p->itype != instI1Pro2) {		/* Rev E mode has different calibration */
		/* Restore the previous reflective spot calibration from the EEProm */
		/* Get ready to operate the instrument */
		if ((ev = i1pro_restore_refspot_cal(p)) != I1PRO_OK)
			return ev; 
	}

#ifdef ENABLE_NONVCAL
	/* Restore the all modes calibration from the local system */
	i1pro_restore_calibration(p);
	/* Touch it so that we know when the instrument was last opened */
	i1pro_touch_calibration(p);
#endif
	
	/* Get ready to operate the instrument */
	if ((ev = i1pro_establish_high_power(p)) != I1PRO_OK)
		return ev; 

	/* Get the current measurement parameters (why ?) */
	if ((ev = i1pro_getmeasparams(p, &m->r_intclocks, &m->r_lampclocks, &m->r_nummeas, &m->r_measmodeflags)) != I1PRO_OK)
		return ev; 

	if (p->log->verb >= 1) {
		a1logv(p->log,1,"Instrument Type:   %s\n",inst_name(p->itype));
		a1logv(p->log,1,"Serial Number:     %d\n",m->serno);
		a1logv(p->log,1,"Firmware version:  %d\n",m->fwrev);
		a1logv(p->log,1,"CPLD version:      %d\n",m->cpldrev);
		if (p->itype == instI1Pro2)
			a1logv(p->log,1,"Chip ID:           %02x-%02x%02x%02x%02x%02x%02x%02x\n",
			                           m->chipid[0], m->chipid[1], m->chipid[2], m->chipid[3],
			                           m->chipid[4], m->chipid[5], m->chipid[6], m->chipid[7]);
		a1logv(p->log,1,"Date manufactured: %d-%d-%d\n",
		                                 m->dom/1000000, (m->dom/10000) % 100, m->dom % 10000);
		// Hmm. physfilt == 0x81 for instI1Monitor ???
		a1logv(p->log,1,"U.V. filter ?:     %s\n",m->physfilt == 0x82 ? "Yes" : "No");
		a1logv(p->log,1,"Measure Ambient ?: %s\n",m->capabilities & 0x6000 ? "Yes" : "No");

		a1logv(p->log,1,"Tot. Measurement Count:           %d\n",m->meascount);
		a1logv(p->log,1,"Remission Spot Count:             %d\n",m->rpcount);
		a1logv(p->log,1,"Remission Scan Count:             %d\n",m->acount);
		a1logv(p->log,1,"Date of last Remission spot cal:  %s",ctime(&m->caldate));
		a1logv(p->log,1,"Remission Spot Count at last cal: %d\n",m->calcount);
		a1logv(p->log,1,"Total lamp usage:                 %f\n",m->lampage);
	}

#ifdef NEVER
// ~~99 play with LED settings
	if (p->itype == instI1Pro2) {

		/* Makes it white */
		unsigned char b2[] = {
			0x00, 0x00, 0x00, 0x02,

			0x00, 0x00, 0x00, 0x0a,
			0x00, 0x00, 0x00, 0x01,
			0x00, 0x36, 0x00,
			0x00, 0x00, 0x01,

			0x00, 0x00, 0x00, 0x0a,
			0xff, 0xff, 0xff, 0xff,
			0x3f, 0x36, 0x40,
			0x00, 0x00, 0x01
		};

		printf("~1 send led sequence length %d\n",sizeof(b2));
		if ((ev = i1pro2_indLEDseq(p, b2, sizeof(b2))) != I1PRO_OK)
			return ev;
	}
	/* Make sure LED sequence is finished, because it interferes with EEProm read! */
	if ((ev = i1pro2_indLEDoff(p)) != I1PRO_OK)
		return ev; 
#endif

	return ev;
}

/* Return a pointer to the serial number */
char *i1pro_imp_get_serial_no(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;

	return m->sserno; 
}

/* Return non-zero if capable of ambient mode */
int i1pro_imp_ambient(i1pro *p) {

	if (p->inited) {
		i1proimp *m = (i1proimp *)p->m;
		if (m->capabilities & 0x6000)		/* Expect ambient calibration */
			return 1;
#ifdef FAKE_AMBIENT
		return 1;
#endif
		return 0;

	} else {
		return 0;
	}
}

/* Set the measurement mode. It may need calibrating */
i1pro_code i1pro_imp_set_mode(
	i1pro *p,
	i1p_mode mmode,		/* Operating mode */
	inst_mode mode		/* Full mode mask for options */
) {
	i1proimp *m = (i1proimp *)p->m;

	a1logd(p->log,2,"i1pro_imp_set_mode called with mode no %d and mask 0x%x\n",mmode,m);
	switch(mmode) {
		case i1p_refl_spot:
		case i1p_refl_scan:
			if (p->itype == instI1Monitor)
				return I1PRO_INT_ILLEGALMODE;		/* i1Monitor can't do reflection */
			break;
		case i1p_emiss_spot_na:
		case i1p_emiss_spot:
		case i1p_emiss_scan:
			break;
		case i1p_amb_spot:
		case i1p_amb_flash:
			if (!i1pro_imp_ambient(p))
				return I1PRO_INT_ILLEGALMODE;
			break;
		case i1p_trans_spot:
		case i1p_trans_scan:
			break;
		default:
			return I1PRO_INT_ILLEGALMODE;
	}
	m->mmode = mmode;
	m->spec_en = (mode & inst_mode_spectral) != 0;

	if ((mode & inst_mode_highres) != 0) {
		i1pro_code rv;
		if ((rv = i1pro_set_highres(p)) != I1PRO_OK)
			return rv;
	} else {
		i1pro_set_stdres(p);	/* Ignore any error */
	}

	m->uv_en = 0;

	if (mmode == i1p_refl_spot
	 || mmode == i1p_refl_scan)
		m->uv_en = (mode & inst_mode_ref_uv) != 0;

	return I1PRO_OK;
}

/* Return needed and available inst_cal_type's */
i1pro_code i1pro_imp_get_n_a_cals(i1pro *p, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *cs = &m->ms[m->mmode];
	time_t curtime = time(NULL);
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
	int wl_valid = cs->wl_valid;			/* Locally timed out versions of valid state */
	int idark_valid = cs->idark_valid;
	int dark_valid = cs->dark_valid;
	int cal_valid = cs->cal_valid;

	a1logd(p->log,2,"i1pro_imp_get_n_a_cals: checking mode %d\n",m->mmode);

	/* Timout calibrations that are too old */
	if (m->capabilities2 & I1PRO_CAP2_WL_LED) {
		if ((curtime - cs->wldate) > WLCALTOUT) {
			a1logd(p->log,2,"Invalidating wavelength cal as %d secs from last cal\n",curtime - cs->wldate);
			wl_valid = 0;
		}
	}
	if ((curtime - cs->iddate) > ((p->itype == instI1Pro2) ? DCALTOUT2 : DCALTOUT)) {
		a1logd(p->log,2,"Invalidating adaptive dark cal as %d secs from last cal\n",curtime - cs->iddate);
		idark_valid = 0;
	}
	if ((curtime - cs->ddate) > ((p->itype == instI1Pro2) ? DCALTOUT2 : DCALTOUT)) {
		a1logd(p->log,2,"Invalidating dark cal as %d secs from last cal\n",curtime - cs->ddate);
		dark_valid = 0;
	}
	if (!cs->emiss && (curtime - cs->cfdate) > WCALTOUT) {
		a1logd(p->log,2,"Invalidating white cal as %d secs from last cal\n",curtime - cs->cfdate);
		cal_valid = 0;
	}

#ifdef NEVER
	printf("~1 reflective = %d, adaptive = %d, emiss = %d, trans = %d, scan = %d\n",
	cs->reflective, cs->adaptive, cs->emiss, cs->trans, cs->scan);
	printf("~1 idark_valid = %d, dark_valid = %d, cal_valid = %d\n",
	idark_valid,dark_valid,cal_valid); 
	printf("~1 want_calib = %d, want_dcalib = %d, noinitcalib = %d\n",
	cs->want_calib,cs->want_dcalib, m->noinitcalib); 
#endif /* NEVER */

	if (m->capabilities2 & I1PRO_CAP2_WL_LED) {
		if (!wl_valid
		 || (cs->want_dcalib && !m->noinitcalib))	// ?? want_dcalib ??
			n_cals |= inst_calt_wavelength;
		a_cals |= inst_calt_wavelength;
	}
	if (cs->reflective) {
		if (!dark_valid
		 || (cs->want_dcalib && !m->noinitcalib))
			n_cals |= inst_calt_ref_dark;
		a_cals |= inst_calt_ref_dark;

		if (!cal_valid
		 || (cs->want_calib && !m->noinitcalib))
			n_cals |= inst_calt_ref_white;
		a_cals |= inst_calt_ref_white;
	}
	if (cs->emiss) {
		if ((!cs->adaptive && !dark_valid)
		 || (cs->adaptive && !idark_valid)
		 || (cs->want_dcalib && !m->noinitcalib))
			n_cals |= inst_calt_em_dark;
		a_cals |= inst_calt_em_dark;
	}
	if (cs->trans) {
		if ((!cs->adaptive && !dark_valid)
		 || (cs->adaptive && !idark_valid)
	     || (cs->want_dcalib && !m->noinitcalib))
			n_cals |= inst_calt_trans_dark;
		a_cals |= inst_calt_trans_dark;

		if (!cal_valid
	     || (cs->want_calib && !m->noinitcalib))
			n_cals |= inst_calt_trans_vwhite;
		a_cals |= inst_calt_trans_vwhite;
	}
	if (cs->emiss && !cs->adaptive && !cs->scan) {
		if (!cs->done_dintsel)
			n_cals |= inst_calt_emis_int_time;
		a_cals |= inst_calt_emis_int_time;
	}

	/* Special case high res. emissive cal fine calibration, */
	/* needs reflective cal. */
	/* Hmmm. Should we do this every time for emission, in case */
	/* we switch to hires mode ??? */
	if ((cs->emiss || cs->trans)			/* We're in an emissive mode */
	 && m->hr_inited						/* and hi-res has been setup */
	 && (!m->emis_hr_cal || (n_cals & inst_calt_em_dark)) /* and the emis cal hasn't been */
											/* fine tuned or we will be doing a dark cal */
	 && p->itype != instI1Monitor) {		/* i1Monitor doesn't have reflective cal capability */
		n_cals |= inst_calt_ref_white;		/* Need a reflective white calibration */
		a_cals |= inst_calt_ref_white;
	}

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	a1logd(p->log,3,"i1pro_imp_get_n_a_cals: returning n_cals 0x%x, a_cals 0x%x\n",n_cals, a_cals);

	return I1PRO_OK;
}

/* - - - - - - - - - - - - - - - - */
/* Calibrate for the current mode. */
/* Request an instrument calibration of the current mode. */
i1pro_code i1pro_imp_calibrate(
	i1pro *p,
	inst_cal_type *calt,	/* Calibration type to do/remaining */
	inst_cal_cond *calc,	/* Current condition/desired condition */
	char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	int mmode = m->mmode;
	i1pro_state *cs = &m->ms[m->mmode];
	int sx1, sx2, sx3, sx;
	time_t cdate = time(NULL);
	int nummeas = 0;
	int ltocmode = 0;			/* 1 = Lamp turn on compensation mode */
	int i, k;
    inst_cal_type needed, available;

	a1logd(p->log,2,"i1pro_imp_calibrate called with calt 0x%x, calc 0x%x\n",*calt, *calc);

	if ((ev = i1pro_imp_get_n_a_cals(p, &needed, &available)) != I1PRO_OK)
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

		a1logd(p->log,4,"i1pro_imp_calibrate: doing calt 0x%x\n",*calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return I1PRO_OK;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		a1logd(p->log,4,"i1pro_imp_calibrate: unsupported, calt 0x%x, available 0x%x\n",*calt,available);
		return I1PRO_UNSUPPORTED;
	}

	if (*calt & inst_calt_ap_flag) {
		sx1 = 0; sx2 = sx3 = i1p_no_modes;		/* Go through all the modes */
	} else {
		/* Special case - doing reflective cal. to fix emiss hires */
		if ((cs->emiss || cs->trans)			/* We're in an emissive mode */
		 && (*calt & inst_calt_ref_white)) {		/* but we're asked for a ref white cal */
			sx1 = m->mmode; sx2 = sx1 + 1;		/* Just current mode */
			sx3 = i1p_refl_spot;				/* no extra mode */
		} else {
			sx1 = m->mmode; sx2 = sx1 + 1;		/* Just current mode */
			sx3 = i1p_no_modes;					/* no extra mode */
		}
	}

	/* Go through the modes we are going to cover */
	for (sx = sx1; sx < sx2; (++sx >= sx2 && sx3 != i1p_no_modes) ? sx = sx3, sx2 = sx+1, sx3 = i1p_no_modes : 0) {
		i1pro_state *s = &m->ms[sx];
		m->mmode = sx;				/* A lot of functions we call rely on this */

		a1logd(p->log,2,"\nCalibrating mode %d\n", s->mode);

		/* Sanity check scan mode settings, in case something strange */
		/* has been restored from the persistence file. */
		if (s->scan && s->inttime > (2.1 * m->min_int_time)) {
			s->inttime = m->min_int_time;	/* Maximize scan rate */
		}
	
		/* Wavelength calibration: */
		if ((m->capabilities2 & I1PRO_CAP2_WL_LED)
		 && (*calt & (inst_calt_wavelength | inst_calt_ap_flag))
		 && (*calc == inst_calc_man_ref_white
		  || *calc == inst_calc_man_am_dark)) {
			double *wlraw;
			double optscale;
			double *abswav;

			a1logd(p->log,2,"\nDoing wavelength calibration\n");

			wlraw = dvectorz(-1, m->nraw-1);

			if ((ev = i1pro2_wl_measure(p, wlraw, &optscale, &m->wl_cal_inttime, 1.0)) != I1PRO_OK) {
				a1logd(p->log,2,"i1pro2_wl_measure() failed\n");
				return ev;
			}

			/* Find the best fit of the measured values to the reference spectrum */
			if ((ev = i1pro2_match_wl_meas(p, &cs->wl_led_off, wlraw)) != I1PRO_OK) {
				a1logd(p->log,2,"i1pro2_match_wl_meas() failed\n");
				return ev;
			}

			free_dvector(wlraw, -1, m->nraw-1);

			/* Compute normal res. emissive/transmissive wavelength corrected filters */
			if ((ev = i1pro_compute_wav_filters(p, 0, 0)) != I1PRO_OK) {
				a1logd(p->log,2,"i1pro_compute_wav_filters() failed\n");
				return ev;
			}

			/* Compute normal res. reflective wavelength corrected filters */
			if ((ev = i1pro_compute_wav_filters(p, 0, 1)) != I1PRO_OK) {
				a1logd(p->log,2,"i1pro_compute_wav_filters() failed\n");
				return ev;
			}

			/* Re-compute high res. wavelength corrected filters */
			if (m->hr_inited && (ev = i1pro_create_hr(p)) != I1PRO_OK) {
				a1logd(p->log,2,"i1pro_create_hr() failed\n");
				return ev;
			}

			cs->wl_valid = 1;
			cs->wldate = cdate;
			*calt &= ~inst_calt_wavelength;

			/* Save the calib to all modes */
			a1logd(p->log,5,"Saving wavelength calib to similar modes\n");
			for (i = 0; i < i1p_no_modes; i++) {
				i1pro_state *ss = &m->ms[i];
				if (ss == cs)
					continue;
				ss->wl_valid = cs->wl_valid;
				ss->wldate = cs->wldate;
				ss->wl_led_off = cs->wl_led_off;
			}
		}

		/* Fixed int. time black calibration: */
		/* Reflective uses on the fly black, even for adaptive. */
		/* Emiss and trans can use single black ref only for non-adaptive */
		/* using the current inttime & gainmode, while display mode */
		/* does an extra fallback black cal for bright displays. */
		if ((*calt & (inst_calt_ref_dark
		            | inst_calt_em_dark
		            | inst_calt_trans_dark | inst_calt_ap_flag))
		 && (*calc == inst_calc_man_ref_white		/* Any condition conducive to dark calib */
		  || *calc == inst_calc_man_em_dark
		  || *calc == inst_calc_man_am_dark
		  || *calc == inst_calc_man_trans_dark)
		 && ( s->reflective
		  || (s->emiss && !s->adaptive && !s->scan)
		  || (s->trans && !s->adaptive))) {
			int stm;
			int usesdct23 = 0;			/* Is a mode that uses dcaltime2 & 3 */

			if (s->emiss && !s->adaptive && !s->scan)
				usesdct23 = 1;
	
			nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->inttime);
	
			a1logd(p->log,2,"\nDoing initial black calibration with dcaltime %f, int_time %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode);
			stm = msec_time();
			if ((ev = i1pro_dark_measure(p, s->dark_data,
				                               nummeas, &s->inttime, s->gainmode)) != I1PRO_OK) {
				m->mmode = mmode;			/* Restore actual mode */
				return ev;
			}
			a1logd(p->log,2,"Execution time of dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);
	
			/* Special display mode alternate integration time black measurement */
			if (usesdct23) {
				nummeas = i1pro_comp_nummeas(p, s->dcaltime2, s->dark_int_time2);
				a1logd(p->log,2,"Doing 2nd initial black calibration with dcaltime2 %f, dark_int_time2 %f, nummeas %d, gainmode %d\n", s->dcaltime2, s->dark_int_time2, nummeas, s->gainmode);
				stm = msec_time();
				if ((ev = i1pro_dark_measure(p, s->dark_data2,
					                       nummeas, &s->dark_int_time2, s->gainmode)) != I1PRO_OK) {
					m->mmode = mmode;			/* Restore actual mode */
					return ev;
				}
				a1logd(p->log,2,"Execution time of 2nd dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);
	
				nummeas = i1pro_comp_nummeas(p, s->dcaltime3, s->dark_int_time3);
				a1logd(p->log,2,"Doing 3rd initial black calibration with dcaltime3 %f, dark_int_time3 %f, nummeas %d, gainmode %d\n", s->dcaltime3, s->dark_int_time3, nummeas, s->gainmode);
				nummeas = i1pro_comp_nummeas(p, s->dcaltime3, s->dark_int_time3);
				stm = msec_time();
				if ((ev = i1pro_dark_measure(p, s->dark_data3,
					                       nummeas, &s->dark_int_time3, s->gainmode)) != I1PRO_OK) {
					m->mmode = mmode;			/* Restore actual mode */
					return ev;
				}
				a1logd(p->log,2,"Execution time of 3rd dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);
			}
			s->dark_valid = 1;
			s->want_dcalib = 0;
			s->ddate = cdate;
			s->dark_int_time = s->inttime;
			s->dark_gain_mode = s->gainmode;
			*calt &= ~(inst_calt_ref_dark
		             | inst_calt_em_dark
		             | inst_calt_trans_dark);

			/* Save the calib to all similar modes */
			for (i = 0; i < i1p_no_modes; i++) {
				i1pro_state *ss = &m->ms[i];
				if (ss == s || ss->ddate == cdate)
					continue;
				if (( s->reflective
				  || (ss->emiss && !ss->adaptive && !ss->scan)
				  || (ss->trans && !ss->adaptive))
				 && ss->dark_int_time == s->dark_int_time
				 && ss->dark_gain_mode == s->dark_gain_mode) {

					ss->dark_valid = s->dark_valid;
					ss->want_dcalib = s->want_dcalib;
					ss->ddate = s->ddate;
					ss->dark_int_time = s->dark_int_time;
					ss->dark_gain_mode = s->dark_gain_mode;
					for (k = -1; k < m->nraw; k++)
						ss->dark_data[k] = s->dark_data[k];
					/* If this is a mode with dark_data2/3, tranfer it too */
					if (usesdct23 && ss->emiss && !ss->adaptive && !ss->scan) {
						ss->dark_int_time2 = s->dark_int_time2;
						ss->dark_int_time3 = s->dark_int_time2;
						for (k = -1; k < m->nraw; k++) {
							ss->dark_data2[k] = s->dark_data2[k];
							ss->dark_data3[k] = s->dark_data3[k];
						}
					}
				}
			}
		}
	
		/* Emissive scan black calibration: */
		/* Emsissive scan (flash) uses the fastest possible scan rate (??) */
		if ((*calt & (inst_calt_em_dark | inst_calt_ap_flag))
		 && (*calc == inst_calc_man_ref_white		/* Any condition conducive to dark calib */
		  || *calc == inst_calc_man_em_dark
		  || *calc == inst_calc_man_am_dark
		  || *calc == inst_calc_man_trans_dark)
		 && (s->emiss && !s->adaptive && s->scan)) {
			int stm;
	
			nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->inttime);
	
			a1logd(p->log,2,"\nDoing emissive (flash) black calibration with dcaltime %f, int_time %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode);
			stm = msec_time();
			if ((ev = i1pro_dark_measure(p, s->dark_data,
				                                 nummeas, &s->inttime, s->gainmode)) != I1PRO_OK) {
				m->mmode = mmode;			/* Restore actual mode */
				return ev;
			}
			a1logd(p->log,2,"Execution time of dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);
	
			s->dark_valid = 1;
			s->want_dcalib = 0;
			s->ddate = cdate;
			s->dark_int_time = s->inttime;
			s->dark_gain_mode = s->gainmode;
			*calt &= ~inst_calt_em_dark;
	
			/* Save the calib to all similar modes */
			/* We're assuming they have the same int times */
			a1logd(p->log,5,"Saving emissive scan black calib to similar modes\n");
			for (i = 0; i < i1p_no_modes; i++) {
				i1pro_state *ss = &m->ms[i];
				if (ss == s || ss->ddate == s->ddate)
					continue;
				if (ss->emiss && !ss->adaptive && ss->scan) {
					ss->dark_valid = s->dark_valid;
					ss->want_dcalib = s->want_dcalib;
					ss->ddate = s->ddate;
					ss->dark_int_time = s->dark_int_time;
					ss->dark_gain_mode = s->dark_gain_mode;
					for (k = -1; k < m->nraw; k++)
						ss->dark_data[k] = s->dark_data[k];
				}
			}
		}
	
		/* Adaptive black calibration: */
		/* Deal with an emmissive/transmissive black reference */
		/* in non-scan mode, where the integration time and gain may vary. */
		/* The black is interpolated from readings with two extreme integration times */
		if ((*calt & (inst_calt_ref_dark
		            | inst_calt_em_dark
		            | inst_calt_trans_dark | inst_calt_ap_flag))
		 && (*calc == inst_calc_man_ref_white		/* Any condition conducive to dark calib */
		  || *calc == inst_calc_man_em_dark
		  || *calc == inst_calc_man_am_dark
		  || *calc == inst_calc_man_trans_dark)
		 && ((s->emiss && s->adaptive && !s->scan)
		  || (s->trans && s->adaptive && !s->scan))) {
			int i, j, k;
	
			a1logd(p->log,2,"\nDoing emis/trans adapative black calibration\n");

			/* Adaptive where we can't measure the black reference on the fly, */
			/* so bracket it and interpolate. */
			/* The black reference is probably temperature dependent, but */
			/* there's not much we can do about this. */
	
			nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->idark_int_time[0]);
			a1logd(p->log,2,"\nDoing adaptive interpolated black calibration, dcaltime %f, idark_int_time[0] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[0], nummeas, 0);
			if ((ev = i1pro_dark_measure(p, s->idark_data[0],
				                          nummeas, &s->idark_int_time[0], 0)) != I1PRO_OK) {
				m->mmode = mmode;			/* Restore actual mode */
				return ev;
			}
		
			nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->idark_int_time[1]);
			a1logd(p->log,2,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[1] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[1], nummeas, 0);
			if ((ev = i1pro_dark_measure(p, s->idark_data[1],
				                          nummeas, &s->idark_int_time[1], 0)) != I1PRO_OK) {
				m->mmode = mmode;			/* Restore actual mode */
				return ev;
			}
		
#ifdef USE_HIGH_GAIN_MODE
			if (p->itype != instI1Pro2) {	/* Rev E doesn't have high gain mode */
				nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->idark_int_time[2]);
				a1logd(p->log,2,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[2] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[2], nummeas, 1);
				if ((ev = i1pro_dark_measure(p, s->idark_data[2],
					                       nummeas, &s->idark_int_time[2], 1)) != I1PRO_OK) {
					m->mmode = mmode;			/* Restore actual mode */
					return ev;
				}
			
				a1logd(p->log,2,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[3] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[3], nummeas, 1);
				nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->idark_int_time[3]);
				if ((ev = i1pro_dark_measure(p, s->idark_data[3],
					                        nummeas, &s->idark_int_time[3], 1)) != I1PRO_OK) {
					m->mmode = mmode;			/* Restore actual mode */
					return ev;
				}
			}
#endif /* USE_HIGH_GAIN_MODE */
		
			i1pro_prepare_idark(p);
			s->idark_valid = 1;
			s->iddate = cdate;
		
			if ((ev = i1pro_interp_dark(p, s->dark_data, s->inttime, s->gainmode)) != I1PRO_OK) {
				m->mmode = mmode;			/* Restore actual mode */
				return ev;
			}
			s->dark_valid = 1;
			s->want_dcalib = 0;
			s->ddate = s->iddate;
			s->dark_int_time = s->inttime;
			s->dark_gain_mode = s->gainmode;
			*calt &= ~(inst_calt_ref_dark
		            | inst_calt_em_dark
		            | inst_calt_trans_dark);
	
			/* Save the calib to all similar modes */
			/* We're assuming they have the same int times */
			a1logd(p->log,5,"Saving adaptive black calib to similar modes\n");
			for (i = 0; i < i1p_no_modes; i++) {
				i1pro_state *ss = &m->ms[i];
				if (ss == s || ss->iddate == s->iddate)
					continue;
				if ((ss->emiss || ss->trans) && ss->adaptive && !ss->scan) {
					ss->idark_valid = s->idark_valid;
					ss->want_dcalib = s->want_dcalib;
					ss->iddate = s->iddate;
					ss->dark_int_time = s->dark_int_time;
					ss->dark_gain_mode = s->dark_gain_mode;
#ifdef USE_HIGH_GAIN_MODE
					for (j = 0; j < (p->itype != instI1Pro2) ? 4 : 2; j++)
#else
					for (j = 0; j < 2; j++)
#endif
					{
						ss->idark_int_time[j] = s->idark_int_time[j];
						for (k = -1; k < m->nraw; k++)
							ss->idark_data[j][k] = s->idark_data[j][k];
					}
				}
			}
	
			a1logd(p->log,5,"Done adaptive interpolated black calibration\n");
	
			/* Test accuracy of dark level interpolation */
#ifdef TEST_DARK_INTERP
			{
				double tinttime;
				double ref[128], interp[128];
				
	//			fprintf(stderr,"Normal gain offsets:\n");
	//			plot_raw(s->idark_data[0]);
	//			fprintf(stderr,"Normal gain multiplier:\n");
	//			plot_raw(s->idark_data[1]);
				
#ifdef DUMP_DARKM
				extern int ddumpdarkm;
				ddumpdarkm = 1;
#endif
				for (tinttime = m->min_int_time; ; tinttime *= 2.0) {
					if (tinttime >= m->max_int_time)
						tinttime = m->max_int_time;
					nummeas = i1pro_comp_nummeas(p, s->dcaltime, tinttime);
					if ((ev = i1pro_dark_measure(p, ref, nummeas, &tinttime, 0)) != I1PRO_OK) {
						m->mmode = mmode;			/* Restore actual mode */
						return ev;
					}
					i1pro_interp_dark(p, interp, tinttime, 0);
#ifdef DEBUG
					fprintf(stderr,"Low gain ref vs. interp dark offset for inttime %f:\n",tinttime);
					plot_raw2(ref, interp);
#endif
					if ((tinttime * 1.1) > m->max_int_time)
						break;
				}
#ifdef DUMP_DARKM
				ddumpdarkm = 0;
#endif
				
#ifdef USE_HIGH_GAIN_MODE
				if (p->itype != instI1Pro2) {	/* Rev E doesn't have high gain mode */
	//				fprintf(stderr,"High gain offsets:\n");
	//				plot_raw(s->idark_data[2]);
	//				fprintf(stderr,"High gain multiplier:\n");
	//				plot_raw(s->idark_data[3]);
				
					for (tinttime = m->min_int_time; ; tinttime *= 2.0) {
						if (tinttime >= m->max_int_time)
							tinttime = m->max_int_time;
						nummeas = i1pro_comp_nummeas(p, s->dcaltime, tinttime);
						if ((ev = i1pro_dark_measure(p, ref, nummeas, &tinttime, 1)) != I1PRO_OK) {
							m->mmode = mmode;			/* Restore actual mode */
							return ev;
						}
						i1pro_interp_dark(p, interp, tinttime, 1);
#ifdef DEBUG
						fprintf(stderr,"High gain ref vs. interp dark offset for inttime %f:\n",tinttime);
						plot_raw2(ref, interp);
#endif
						if ((tinttime * 1.1) > m->max_int_time)
							break;
					}
				}
#endif /* USE_HIGH_GAIN_MODE */
			}
#endif	/* NEVER */
	
		}
	
		/* Deal with an emissive/transmisive adaptive black reference */
		/* when in scan mode. */
		if ((*calt & (inst_calt_ref_dark
		            | inst_calt_em_dark
		            | inst_calt_trans_dark | inst_calt_ap_flag))
		 && (*calc == inst_calc_man_ref_white		/* Any condition conducive to dark calib */
		  || *calc == inst_calc_man_em_dark
		  || *calc == inst_calc_man_am_dark
		  || *calc == inst_calc_man_trans_dark)
		 && ((s->emiss && s->adaptive && s->scan)
		  || (s->trans && s->adaptive && s->scan))) {
			int j;

			a1logd(p->log,2,"\nDoing emis/trans adapative scan mode black calibration\n");

			/* We know scan is locked to the minimum integration time, */
			/* so we can measure the dark data at that integration time, */
			/* but we don't know what gain mode will be used, so measure both, */
			/* and choose the appropriate one on the fly. */
	
			s->idark_int_time[0] = s->inttime;
			nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->idark_int_time[0]);
			a1logd(p->log,2,"\nDoing adaptive scan black calibration, dcaltime %f, idark_int_time[0] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[0], nummeas, s->gainmode);
			if ((ev = i1pro_dark_measure(p, s->idark_data[0],
				                                  nummeas, &s->idark_int_time[0], 0)) != I1PRO_OK) {
				m->mmode = mmode;			/* Restore actual mode */
				return ev;
			}
		
#ifdef USE_HIGH_GAIN_MODE
			if (p->itype != instI1Pro2) {	/* Rev E doesn't have high gain mode */
				s->idark_int_time[2] = s->inttime;
				nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->idark_int_time[2]);
				a1logd(p->log,2,"Doing adaptive scan black calibration, dcaltime %f, idark_int_time[2] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[2], nummeas, s->gainmode);
				if ((ev = i1pro_dark_measure(p, s->idark_data[2],
					                           nummeas, &s->idark_int_time[2], 1)) != I1PRO_OK) {
					m->mmode = mmode;			/* Restore actual mode */
					return ev;
				}
			}
#endif
		
			s->idark_valid = 1;
			s->iddate = cdate;
	
#ifdef USE_HIGH_GAIN_MODE
		   	if (s->gainmode) {
				for (j = -1; j < m->nraw; j++)
					s->dark_data[j] = s->idark_data[2][j];
			} else
#endif
			{
				for (j = -1; j < m->nraw; j++)
					s->dark_data[j] = s->idark_data[0][j];
			}
			s->dark_valid = 1;
			s->want_dcalib = 0;
			s->ddate = s->iddate;
			s->dark_int_time = s->inttime;
			s->dark_gain_mode = s->gainmode;
			*calt &= ~(inst_calt_ref_dark
		            | inst_calt_em_dark
		            | inst_calt_trans_dark);
	
			a1logd(p->log,2,"Done adaptive scan black calibration\n");
	
			/* Save the calib to all similar modes */
			/* We're assuming they have the same int times */
			a1logd(p->log,5,"Saving adaptive scan black calib to similar modes\n");
			for (i = 0; i < i1p_no_modes; i++) {
				i1pro_state *ss = &m->ms[i];
				if (ss == s || ss->iddate == s->iddate)
					continue;
				if ((ss->emiss || ss->trans) && ss->adaptive && s->scan) {
					ss->idark_valid = s->idark_valid;
					ss->want_dcalib = s->want_dcalib;
					ss->iddate = s->iddate;
					ss->dark_int_time = s->dark_int_time;
					ss->dark_gain_mode = s->dark_gain_mode;
#ifdef USE_HIGH_GAIN_MODE
					for (j = 0; j < (p->itype != instI1Pro2) ? 4 : 2; j += 2)
#else
					for (j = 0; j < 2; j += 2)
#endif
					{
						ss->idark_int_time[j] = s->idark_int_time[j];
						for (k = -1; k < m->nraw; k++)
							ss->idark_data[j][k] = s->idark_data[j][k];
					}
				}
			}
		}
	
		/* If we are doing a white reference calibrate */
		if ((*calt & (inst_calt_ref_white
		            | inst_calt_trans_vwhite | inst_calt_ap_flag))
		 && ((*calc == inst_calc_man_ref_white && s->reflective)
		  || (*calc == inst_calc_man_trans_white && s->trans))) {
			double scale;
	
			a1logd(p->log,2,"\nDoing initial white calibration with current inttime %f, gainmode %d\n",
			                                                               s->inttime, s->gainmode);
			nummeas = i1pro_comp_nummeas(p, s->wcaltime, s->inttime);
			ev = i1pro_whitemeasure(p, s->cal_factor[0], s->cal_factor[1], s->white_data, &scale, nummeas,
			           &s->inttime, s->gainmode, s->scan ? 1.0 : s->targoscale, 0);
			if (ev == I1PRO_RD_SENSORSATURATED) {
				scale = 0.0;			/* Signal it this way */
				ev = I1PRO_OK;
			}
			if (ev != I1PRO_OK) {
				m->mmode = mmode;			/* Restore actual mode */
				return ev;
			}
	
			/* For non-scan modes, we adjust the integration time to avoid saturation, */
			/* and to try and match the target optimal sensor value */
			if (!s->scan) {
				/* If it's adaptive and not good, or if it's not adaptive and even worse, */
				/* or if we're using lamp dynamic compensation for reflective scan, */
				/* change the parameters until the white is optimal. */
				if ((s->adaptive && (scale < 0.95 || scale > 1.05))
				 || (scale < 0.3 || scale > 2.0)) {
			
					/* Need to have done adaptive black measure to change inttime/gain params */
					if (*calc != inst_calc_man_ref_white && !s->idark_valid) {
						m->mmode = mmode;			/* Restore actual mode */
						return I1PRO_RD_TRANSWHITERANGE;
					}
	
					if (scale == 0.0) {		/* If sensor was saturated */
						s->inttime = m->min_int_time;
						s->gainmode = 0;
						s->dark_valid = 0;
						if (!s->emiss)
							s->cal_valid = 0;
	
						if (*calc == inst_calc_man_ref_white) {
							nummeas = i1pro_comp_nummeas(p, s->dadaptime, s->inttime);
							a1logd(p->log,2,"Doing another black calibration with dadaptime %f, min inttime %f, nummeas %d, gainmode %d\n", s->dadaptime, s->inttime, nummeas, s->gainmode);
							if ((ev = i1pro_dark_measure(p, s->dark_data,
								             nummeas, &s->inttime, s->gainmode)) != I1PRO_OK) {
								m->mmode = mmode;			/* Restore actual mode */
								return ev;
							}
	
						} else if (s->idark_valid) {
							/* compute interpolated dark refence for chosen inttime & gainmode */
							a1logd(p->log,2,"Interpolate dark calibration reference\n");
							if ((ev = i1pro_interp_dark(p, s->dark_data,
								                       s->inttime, s->gainmode)) != I1PRO_OK) {
								m->mmode = mmode;			/* Restore actual mode */
								return ev;
							}
							s->dark_valid = 1;
							s->ddate = s->iddate;
							s->dark_int_time = s->inttime;
							s->dark_gain_mode = s->gainmode;
						} else {
							m->mmode = mmode;			/* Restore actual mode */
							return I1PRO_INT_NOINTERPDARK;
						}
						a1logd(p->log,2,"Doing another white calibration with min inttime %f, gainmode %d\n",
						                                                  s->inttime,s->gainmode);
						nummeas = i1pro_comp_nummeas(p, s->wadaptime, s->inttime);
						if ((ev = i1pro_whitemeasure(p, s->cal_factor[0], s->cal_factor[1], s->white_data,
						   &scale, nummeas, &s->inttime, s->gainmode, s->targoscale, 0))
							                                                != I1PRO_OK) {
							m->mmode = mmode;			/* Restore actual mode */
							return ev;
						}
					}
	
					/* Compute a new integration time and gain mode */
					/* in order to optimise the sensor values. Error if can't get */
					/* scale we want. */
					if ((ev = i1pro_optimise_sensor(p, &s->inttime, &s->gainmode, 
					         s->inttime, s->gainmode, s->trans, 0, s->targoscale, scale)) != I1PRO_OK) {
						m->mmode = mmode;			/* Restore actual mode */
						return ev;
					}
					a1logd(p->log,2,"Computed optimal white inttime %f and gainmode %d\n",
					                                                      s->inttime,s->gainmode);
				
					if (*calc == inst_calc_man_ref_white) {
						nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->inttime);
						a1logd(p->log,2,"Doing final black calibration with dcaltime %f, opt inttime %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode);
						if ((ev = i1pro_dark_measure(p, s->dark_data,
								                   nummeas, &s->inttime, s->gainmode)) != I1PRO_OK) {
							m->mmode = mmode;			/* Restore actual mode */
							return ev;
						}
						s->dark_valid = 1;
						s->ddate = cdate;
						s->dark_int_time = s->inttime;
						s->dark_gain_mode = s->gainmode;
	
					} else if (s->idark_valid) {
						/* compute interpolated dark refence for chosen inttime & gainmode */
						a1logd(p->log,2,"Interpolate dark calibration reference\n");
						if ((ev = i1pro_interp_dark(p, s->dark_data,
							                             s->inttime, s->gainmode)) != I1PRO_OK) {
							m->mmode = mmode;			/* Restore actual mode */
							return ev;
						}
						s->dark_valid = 1;
						s->ddate = s->iddate;
						s->dark_int_time = s->inttime;
						s->dark_gain_mode = s->gainmode;
					} else {
						m->mmode = mmode;			/* Restore actual mode */
						return I1PRO_INT_NOINTERPDARK;
					}
				
					a1logd(p->log,2,"Doing final white calibration with opt int_time %f, gainmode %d\n",
					                                                 s->inttime,s->gainmode);
					nummeas = i1pro_comp_nummeas(p, s->wcaltime, s->inttime);
					if ((ev = i1pro_whitemeasure(p, s->cal_factor[0], s->cal_factor[1], s->white_data,
					  &scale, nummeas, &s->inttime, s->gainmode, s->targoscale, ltocmode)) != I1PRO_OK) {
						m->mmode = mmode;			/* Restore actual mode */
						return ev;
					}
				}
	
			/* For scan we take a different approach. We try and use the minimum possible */
			/* integration time so as to maximize sampling rate, and adjust the gain */
			/* if necessary. */
			} else if (s->adaptive) {
				int j;
				if (scale == 0.0) {		/* If sensor was saturated */
					a1logd(p->log,3,"Scan illuminant is saturating sensor\n");
					if (s->gainmode == 0) {
						m->mmode = mmode;			/* Restore actual mode */
						return I1PRO_RD_SENSORSATURATED;		/* Nothing we can do */
					}
					a1logd(p->log,3,"Switching to low gain mode\n");
					s->gainmode = 0;
					/* Measure white again with low gain */
					nummeas = i1pro_comp_nummeas(p, s->wcaltime, s->inttime);
					if ((ev = i1pro_whitemeasure(p, s->cal_factor[0], s->cal_factor[1], s->white_data,
					   &scale, nummeas, &s->inttime, s->gainmode, 1.0, 0)) != I1PRO_OK) {
						m->mmode = mmode;			/* Restore actual mode */
						return ev;
					}
				} else if (p->itype != instI1Pro2 && s->gainmode == 0 && scale > m->highgain) {
#ifdef USE_HIGH_GAIN_MODE
					a1logd(p->log,3,"Scan signal is so low we're switching to high gain mode\n");
					s->gainmode = 1;
					/* Measure white again with high gain */
					nummeas = i1pro_comp_nummeas(p, s->wcaltime, s->inttime);
					if ((ev = i1pro_whitemeasure(p, s->cal_factor[0], s->cal_factor[1], s->white_data,
					   &scale, nummeas, &s->inttime, s->gainmode, 1.0, 0)) != I1PRO_OK) {
						m->mmode = mmode;			/* Restore actual mode */
						return ev;
					}
#endif /* USE_HIGH_GAIN_MODE */
				}
	
				a1logd(p->log,2,"After scan gain adaption scale = %f\n",scale);
				if (scale > 6.0) {
					m->transwarn |= 2;
					a1logd(p->log,2, "scan white reference is not bright enough by %f\n",scale);
				}
	
				if (*calc == inst_calc_man_ref_white) {
					nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->inttime);
					a1logd(p->log,2,"Doing final black calibration with dcaltime %f, opt inttime %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode);
					if ((ev = i1pro_dark_measure(p, s->dark_data,
						              nummeas, &s->inttime, s->gainmode)) != I1PRO_OK) {
						m->mmode = mmode;			/* Restore actual mode */
						return ev;
					}
					s->dark_valid = 1;
					s->ddate = cdate;
					s->dark_int_time = s->inttime;
					s->dark_gain_mode = s->gainmode;
	
				} else if (s->idark_valid) {
					/* compute interpolated dark refence for chosen inttime & gainmode */
					a1logd(p->log,2,"Interpolate dark calibration reference\n");
					if (s->gainmode) {
						for (j = -1; j < m->nraw; j++)
							s->dark_data[j] = s->idark_data[2][j];
					} else {
						for (j = -1; j < m->nraw; j++)
							s->dark_data[j] = s->idark_data[0][j];
					}
					s->dark_valid = 1;
					s->ddate = s->iddate;
					s->dark_int_time = s->inttime;
					s->dark_gain_mode = s->gainmode;
				} else {
					m->mmode = mmode;			/* Restore actual mode */
					return I1PRO_INT_NOINTERPDARK;
				}
				a1logd(p->log,2,"Doing final white calibration with opt int_time %f, gainmode %d\n",
				                                                 s->inttime,s->gainmode);
			}
	
			/* We've settled on the inttime and gain mode to get a good white reference. */
			if (s->reflective) {	/* We read the white reference - check it */
				/* Check a reflective white measurement, and check that */
				/* it seems reasonable. Return I1PRO_OK if it is, error if not. */
				/* (Using cal_factor[] as temp.) */
				a1logd(p->log,2,"Checking white reference\n");
				if ((ev = i1pro_check_white_reference1(p, s->cal_factor[0])) != I1PRO_OK) {
					m->mmode = mmode;			/* Restore actual mode */
					return ev;
				}
				/* Compute a calibration factor given the reading of the white reference. */
				ev = i1pro_compute_white_cal(p,
				                s->cal_factor[0], m->white_ref[0], s->cal_factor[0],
				                s->cal_factor[1], m->white_ref[1], s->cal_factor[1],
								!s->scan);		/* Use this for emis hires fine tune if not scan */
				if (ev == I1PRO_RD_TRANSWHITEWARN)		/* Shouldn't happen ? */
					ev = I1PRO_OK;
				if (ev != I1PRO_OK) {
					m->mmode = mmode;			/* Restore actual mode */
					return ev;
				}
	
			} else {	/* transmissive */
				/* Compute a calibration factor given the reading of the white reference. */
				ev = i1pro_compute_white_cal(p, s->cal_factor[0], NULL, s->cal_factor[0],
				                                s->cal_factor[1], NULL, s->cal_factor[1], 0);
				if (ev == I1PRO_RD_TRANSWHITEWARN) {
					m->transwarn |= 1;
					ev = I1PRO_OK;
				}
				if (ev != I1PRO_OK) {
					m->mmode = mmode;			/* Restore actual mode */
					return ev;
				}
			}
			s->cal_valid = 1;
			s->cfdate = cdate;
			s->want_calib = 0;
			*calt &= ~(inst_calt_ref_white
			         | inst_calt_trans_vwhite);
		}
	
		/* Deal with a display integration time selection */
		if ((*calt & (inst_calt_emis_int_time | inst_calt_ap_flag))
		 && *calc == inst_calc_emis_white
		 && (s->emiss && !s->adaptive && !s->scan)) {
			double scale;
			double *data;
			double *tt, tv;
	
			data = dvectorz(-1, m->nraw-1);
	
			a1logd(p->log,2,"\nDoing display integration time calibration\n");
	
			/* Undo any previous swaps */
			if (s->dispswap == 1) {
				tv = s->inttime; s->inttime = s->dark_int_time2; s->dark_int_time2 = tv;
				tt = s->dark_data; s->dark_data = s->dark_data2; s->dark_data2 = tt;
			} else if (s->dispswap == 2) {
				tv = s->inttime; s->inttime = s->dark_int_time3; s->dark_int_time3 = tv;
				tt = s->dark_data; s->dark_data = s->dark_data3; s->dark_data3 = tt;
			}
			s->dispswap = 0;

			/* Simply measure the full display white, and if it's close to */
			/* saturation, switch to the alternate display integration time */
			nummeas = i1pro_comp_nummeas(p, s->wreadtime, s->inttime);
			ev = i1pro_whitemeasure(p, NULL, NULL, data , &scale, nummeas,
			           &s->inttime, s->gainmode, s->targoscale, 0);
			/* Switch to the alternate if things are too bright */
			/* We do this simply by swapping the alternate values in. */
			if (ev == I1PRO_RD_SENSORSATURATED || scale < 1.0) {
				a1logd(p->log,2,"Switching to alternate display integration time %f seconds\n",s->dark_int_time2);
				tv = s->inttime; s->inttime = s->dark_int_time2; s->dark_int_time2 = tv;
				tt = s->dark_data; s->dark_data = s->dark_data2; s->dark_data2 = tt;
				s->dispswap = 1;
	
				/* Do another measurement of the full display white, and if it's close to */
				/* saturation, switch to the 3rd alternate display integration time */
				nummeas = i1pro_comp_nummeas(p, s->wreadtime, s->inttime);
				ev = i1pro_whitemeasure(p, NULL, NULL, data , &scale, nummeas,
				           &s->inttime, s->gainmode, s->targoscale, 0);
				/* Switch to the 3rd alternate if things are too bright */
				/* We do this simply by swapping the alternate values in. */
				if (ev == I1PRO_RD_SENSORSATURATED || scale < 1.0) {
					a1logd(p->log,2,"Switching to 3rd alternate display integration time %f seconds\n",s->dark_int_time3);
					/* Undo previous swap */
					tv = s->inttime; s->inttime = s->dark_int_time2; s->dark_int_time2 = tv;
					tt = s->dark_data; s->dark_data = s->dark_data2; s->dark_data2 = tt;
					/* swap in 2nd alternate */
					tv = s->inttime; s->inttime = s->dark_int_time3; s->dark_int_time3 = tv;
					tt = s->dark_data; s->dark_data = s->dark_data3; s->dark_data3 = tt;
					s->dispswap = 2;
				}
			}
			free_dvector(data, -1, m->nraw-1);
			if (ev != I1PRO_OK) {
				m->mmode = mmode;			/* Restore actual mode */
				return ev;
			}
			s->done_dintsel = 1;
			s->diseldate = cdate;
			*calt &= ~inst_calt_emis_int_time;
	
			a1logd(p->log,5,"Done display integration time calibration\n");
		}

	}	/* Look at next mode */
	m->mmode = mmode;			/* Restore actual mode */

	/* Make sure there's the right condition for any remaining calibrations. */
	/* Do ref_white first in case we are doing a high res fine tune. */

	if (*calt & (inst_calt_ref_dark | inst_calt_ref_white)) {
		sprintf(id, "Serial no. %d",m->serno);
		if (*calc != inst_calc_man_ref_white) {
			*calc = inst_calc_man_ref_white;	/* Calibrate using white tile */
			return I1PRO_CAL_SETUP;
		}
	} else if (*calt & inst_calt_wavelength) {		/* Wavelength calibration */
		if (cs->emiss && cs->ambient) {
			id[0] = '\000';
			if (*calc != inst_calc_man_am_dark) {
				*calc = inst_calc_man_am_dark;	/* Calibrate using ambient adapter */
				return I1PRO_CAL_SETUP;
			}
		} else {
			sprintf(id, "Serial no. %d",m->serno);
			if (*calc != inst_calc_man_ref_white) {
				*calc = inst_calc_man_ref_white;	/* Calibrate using white tile */
				return I1PRO_CAL_SETUP;
			}
		}
	} else if (*calt & inst_calt_em_dark) {		/* Emissive Dark calib */
		id[0] = '\000';
		if (*calc != inst_calc_man_em_dark) {
			*calc = inst_calc_man_em_dark;		/* Any sort of dark reference */
			return I1PRO_CAL_SETUP;
		}
	} else if (*calt & inst_calt_trans_dark) {	/* Transmissvice dark */
		id[0] = '\000';
		if (*calc != inst_calc_man_trans_dark) {
			*calc = inst_calc_man_trans_dark;
			return I1PRO_CAL_SETUP;
		}
	} else if (*calt & inst_calt_trans_vwhite) {/* Transmissvice white for emulated transmission */
		id[0] = '\000';
		if (*calc != inst_calc_man_trans_white) {
			*calc = inst_calc_man_trans_white;
			return I1PRO_CAL_SETUP;
		}
	} else if (*calt & inst_calt_emis_int_time) {
		id[0] = '\000';
		if (*calc != inst_calc_emis_white) {
			*calc = inst_calc_emis_white;
			return I1PRO_CAL_SETUP;
		}
	}

	/* Go around again if we've still got calibrations to do */
	if (*calt & inst_calt_all_mask) {
		return I1PRO_CAL_SETUP;
	}

	/* We must be done */

	/* Update and write the EEProm log if the is a refspot calibration */
	if (cs->reflective && !cs->scan && cs->dark_valid && cs->cal_valid) {
		m->calcount = m->rpcount;
		m->caldate = cdate;
		if ((ev = i1pro_update_log(p)) != I1PRO_OK) {
			a1logd(p->log,2,"i1pro_update_log: Updating the cal and log parameters"
			                                               " to EEProm failed\n");
		}
	}
	
#ifdef ENABLE_NONVCAL
	/* Save the calibration to a file */
	i1pro_save_calibration(p);
#endif

	if (m->transwarn) {
		*calc = inst_calc_message;
		if (m->transwarn & 2)
			strcpy(id, "Warning: Transmission light source is too low for accuracy!");
		else
			strcpy(id, "Warning: Transmission light source is low at some wavelengths!");
		m->transwarn = 0;
	}

	a1logd(p->log,2,"Finished cal with dark_valid = %d, cal_valid = %d\n",cs->dark_valid, cs->cal_valid);

	return I1PRO_OK; 
}

/* Interpret an icoms error into a I1PRO error */
int icoms2i1pro_err(int se) {
	if (se != ICOM_OK)
		return I1PRO_COMS_FAIL;
	return I1PRO_OK;
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
#define NDSAMPS 500			/* Debug samples */

typedef struct {
	double sec;
	double rgb[3];
	double tot;
} i1rgbdsamp;

i1pro_code i1pro_imp_meas_delay(
i1pro *p,
int *pdispmsec,		/* Return display update delay in msec */
int *pinstmsec) {	/* Return instrument latency in msec */
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
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

	if (pinstmsec != NULL)
		*pinstmsec = 0; 

	if ((rstart = usec_time()) < 0.0) {
		a1loge(p->log, inst_internal_error, "i1pro_imp_meas_delay: No high resolution timers\n");
		return inst_internal_error; 
	}

	/* Read the samples */
	inttime = m->min_int_time;
	nummeas = (int)(NDMXTIME/inttime + 0.5);
	multimeas = dmatrix(0, nummeas-1, -1, m->nwav[m->highres]-1);
	if ((samp = (i1rgbdsamp *)calloc(sizeof(i1rgbdsamp), nummeas)) == NULL) {
		a1logd(p->log, 1, "i1pro_meas_delay: malloc failed\n");
		return I1PRO_INT_MALLOC;
	}

	/* We rely on the measurement code setting m->trigstamp when the */
	/* trigger packet is sent to the instrument */
	if ((ev = i1pro_read_patches_all(p, multimeas, nummeas, &inttime, 0)) != inst_ok) {
		free_dmatrix(multimeas, 0, nummeas-1, 0, m->nwav[m->highres]-1);
		free(samp);
		return ev;
	}

	if (m->whitestamp < 0.0) {
		a1logd(p->log, 1, "i1d3_meas_delay: White transition wasn't timestamped\n");
		return inst_internal_error; 
	}

	/* Convert the samples to RGB */
	/* Add 10 msec fudge factor */
	for (i = 0; i < nummeas; i++) {
		samp[i].sec = i * inttime + (m->trigstamp - m->whitestamp)/1000000.0 + 0.01;
		samp[i].rgb[0] = samp[i].rgb[1] = samp[i].rgb[2] = 0.0;
		for (j = 0; j < m->nwav[m->highres]; j++) {
			double wl = XSPECT_WL(m->wl_short[m->highres], m->wl_long[m->highres], m->nwav[m->highres], j);

//printf("~1 multimeas %d %d = %f\n",i, j, multimeas[i][j]);
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
	free_dmatrix(multimeas, 0, nummeas-1, 0, m->nwav[m->highres]-1);

	a1logd(p->log, 3, "i1pro_meas_delay: Read %d samples for refresh calibration\n",nummeas);

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
	a1logd(p->log, 0, "i1pro_meas_delay: start tot %f end tot %f del %f, thr %f\n", stot, etot, del, thr);
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
//printf("%d: %f -> %f\n",i,samp[i].sec, samp[i].tot);
		}
		printf("Display update delay measure sensor values and time (sec)\n");
		do_plot6(xx, y1, y2, y3, y4, NULL, NULL, nummeas);
	}
#endif

	/* Check that there has been a transition */
	if (del < 5.0) {
		free(samp);
		a1logd(p->log, 1, "i1pro_meas_delay: can't detect change from black to white\n");
		return I1PRO_RD_NOTRANS_FOUND; 
	}

	/* Working from the start, locate the time at which the level was above the threshold */
	for (i = 0; i < (nummeas-1); i++) {
		if (samp[i].tot > thr)
			break;
	}

	a1logd(p->log, 2, "i1pro_meas_delay: stoped at sample %d time %f\n",i,samp[i].sec);

	/* Compute overall delay */
	dispmsec = (int)(samp[i].sec * 1000.0 + 0.5);				/* Display update time */
	instmsec = (int)((m->trigstamp - rstart)/1000.0 + 0.5);		/* Reaction time */

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "i1pro_meas_delay: disp %d, trig %d msec\n",dispmsec,instmsec);
#else
	a1logd(p->log, 2, "i1pro_meas_delay: disp %d, trig %d msec\n",dispmsec,instmsec);
#endif

	if (dispmsec < 0) 		/* This can happen if the patch generator delays it's return */
		dispmsec = 0;

	if (pdispmsec != NULL)
		*pdispmsec = dispmsec;

	if (pinstmsec != NULL)
		*pinstmsec = instmsec;

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "i1pro_meas_delay: returning %d & %d msec\n",dispmsec,instmsec);
#else
	a1logd(p->log, 2, "i1pro_meas_delay: returning %d & %d msec\n",dispmsec,instmsec);
#endif
	free(samp);

	return I1PRO_OK;
}
#undef NDSAMPS
#undef NDMXTIME

/* Timestamp the white patch change during meas_delay() */
inst_code i1pro_imp_white_change(i1pro *p, int init) {
	i1proimp *m = (i1proimp *)p->m;

	if (init)
		m->whitestamp = -1.0;
	else {
		if ((m->whitestamp = usec_time()) < 0.0) {
			a1loge(p->log, inst_internal_error, "i1pro_imp_wite_change: No high resolution timers\n");
			return inst_internal_error; 
		}
	}

	return inst_ok;
}

/* - - - - - - - - - - - - - - - - */
/* Measure a patch or strip in the current mode. */
/* To try and speed up the reaction time between */
/* triggering a scan measurement and being able to */
/* start moving the instrument, we pre-allocate */
/* all the buffers and arrays, and pospone processing */
/* until after the scan is complete. */
i1pro_code i1pro_imp_measure(
	i1pro *p,
	ipatch *vals,		/* Pointer to array of instrument patch value */
	int nvals,			/* Number of values */	
	instClamping clamp	/* Clamp XYZ/Lab to be +ve */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	unsigned char *buf = NULL;	/* Raw USB reading buffer for reflection dark cal */
	unsigned int bsize;
	unsigned char *mbuf = NULL;	/* Raw USB reading buffer for measurement */
	unsigned int mbsize;
	int nummeas = 0, maxnummeas = 0;
	int nmeasuered = 0;			/* Number actually measured */
	double **specrd = NULL;		/* Cooked spectral patch values */
	double duration = 0.0;		/* Possible flash duration value */
	int user_trig = 0;

	a1logd(p->log,2,"i1pro_imp_measure: Taking %d measurments in %s%s%s%s%s%s mode called\n", nvals,
		        s->emiss ? "Emission" : s->trans ? "Trans" : "Refl", 
		        s->emiss && s->ambient ? " Ambient" : "",
		        s->scan ? " Scan" : "",
		        s->flash ? " Flash" : "",
		        s->adaptive ? " Adaptive" : "",
		        m->uv_en ? " UV" : "");


	if ((s->emiss && s->adaptive && !s->idark_valid)
	 || ((!s->emiss || !s->adaptive) && !s->dark_valid)
	 || !s->cal_valid) {
		a1logd(p->log,3,"emis %d, adaptive %d, idark_valid %d\n",s->emiss,s->adaptive,s->idark_valid);
		a1logd(p->log,3,"dark_valid %d, cal_valid %d\n",s->dark_valid,s->cal_valid);
		a1logd(p->log,3,"i1pro_imp_measure need calibration\n");
		return I1PRO_RD_NEEDS_CAL;
	}
		
	if (nvals <= 0
	 || (!s->scan && nvals > 1)) {
		a1logd(p->log,2,"i1pro_imp_measure wrong number of patches\n");
		return I1PRO_INT_WRONGPATCHES;
	}

	/* Notional number of measurements, before adaptive and not counting scan */
	nummeas = i1pro_comp_nummeas(p, s->wreadtime, s->inttime);

	/* Allocate buf for pre-measurement dark calibration */
	if (s->reflective) {
		bsize = m->nsen * 2 * nummeas;
		if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
			a1logd(p->log,1,"i1pro_imp_measure malloc %d bytes failed (5)\n",bsize);
			return I1PRO_INT_MALLOC;
		}
	}

	/* Allocate buffer for measurement */
	maxnummeas = i1pro_comp_nummeas(p, s->maxscantime, s->inttime);
	if (maxnummeas < nummeas)
		maxnummeas = nummeas;
	mbsize = m->nsen * 2 * maxnummeas;
	if ((mbuf = (unsigned char *)malloc(sizeof(unsigned char) * mbsize)) == NULL) {
		if (buf != NULL)
			free(buf);
		a1logd(p->log,1,"i1pro_imp_measure malloc %d bytes failed (6)\n",mbsize);
		return I1PRO_INT_MALLOC;
	}
	specrd = dmatrix(0, nvals-1, 0, m->nwav[m->highres]-1);

	if (m->trig == inst_opt_trig_user_switch) {
		m->hide_switch = 1;						/* Supress switch events */

#ifdef USE_THREAD
		{
			int currcount = m->switch_count;		/* Variable set by thread */
			while (currcount == m->switch_count) {
				inst_code rc;
				int cerr;

				/* Don't trigger on user key if scan, only trigger */
				/* on instrument switch */
				if (p->uicallback != NULL
				 && (rc = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
					if (rc == inst_user_abort) {
						ev = I1PRO_USER_ABORT;
						break;						/* Abort */
					}
					if (!s->scan && rc == inst_user_trig) {
						ev = I1PRO_USER_TRIG;
						user_trig = 1;
						break;						/* Trigger */
					}
				}
				msec_sleep(100);
			}
		}
#else
		/* Throw one away in case the switch was pressed prematurely */
		i1pro_waitfor_switch_th(p, 0.01);

		for (;;) {
			inst_code rc;
			int cerr;

			if ((ev = i1pro_waitfor_switch_th(p, 0.1)) != I1PRO_OK
			 && ev != I1PRO_INT_BUTTONTIMEOUT)
				break;			/* Error */

			if (ev == I1PRO_OK)
				break;			/* switch triggered */

			/* Don't trigger on user key if scan, only trigger */
			/* on instrument switch */
			if (p->uicallback != NULL
			 && (rc = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
				if (rc == inst_user_abort) {
					ev = I1PRO_USER_ABORT;
					break;						/* Abort */
				}
				if (!s->scan && rc == inst_user_trig) {
					ev = I1PRO_USER_TRIG;
					user_trig = 1;
					break;						/* Trigger */
				}
			}
		}
#endif
		a1logd(p->log,3,"############# triggered ##############\n");
		if (p->uicallback)	/* Notify of trigger */
			p->uicallback(p->uic_cntx, inst_triggered); 

		m->hide_switch = 0;						/* Enable switch events again */

	} else if (m->trig == inst_opt_trig_user) {
		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "hcfr: inst_opt_trig_user but no uicallback function set!\n");
			ev = I1PRO_UNSUPPORTED;

		} else {

			for (;;) {
				inst_code rc;
				if ((rc = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
					if (rc == inst_user_abort) {
						ev = I1PRO_USER_ABORT;	/* Abort */
						break;
					}
					if (rc == inst_user_trig) {
						ev = I1PRO_USER_TRIG;
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
			ev = I1PRO_USER_ABORT;	/* Abort */
	}

	if (ev != I1PRO_OK && ev != I1PRO_USER_TRIG) {
		free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
		free(mbuf);
		if (buf != NULL)
			free(buf);
		a1logd(p->log,2,"i1pro_imp_measure user aborted, terminated, command, or failure\n");
		return ev;		/* User abort, term, command or failure */
	}

	if (s->emiss && !s->scan && s->adaptive) {
		int saturated = 0;
		double optscale = 1.0;
		s->inttime = 0.25;
		s->gainmode = 0;
		s->dark_valid = 0;

		a1logd(p->log,2,"Trial measure emission with inttime %f, gainmode %d\n",s->inttime,s->gainmode);

		/* Take a trial measurement reading using the current mode. */
		/* Used to determine if sensor is saturated, or not optimal */
//		nummeas = i1pro_comp_nummeas(p, s->wreadtime, s->inttime);
		nummeas = 1;
		if ((ev = i1pro_trialmeasure(p, &saturated, &optscale, nummeas, &s->inttime, s->gainmode,
		                                                            s->targoscale)) != I1PRO_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
			free(mbuf);
			a1logd(p->log,2,"i1pro_imp_measure trial measure failed\n");
			return ev;  
		}

		if (saturated) {
			s->inttime = m->min_int_time;

			a1logd(p->log,2,"2nd trial measure emission with inttime %f, gainmode %d\n",
			                                         s->inttime,s->gainmode);
			/* Take a trial measurement reading using the current mode. */
			/* Used to determine if sensor is saturated, or not optimal */
			nummeas = i1pro_comp_nummeas(p, 0.25, s->inttime);
			if ((ev = i1pro_trialmeasure(p, &saturated, &optscale, nummeas, &s->inttime, 
			                                          s->gainmode, s->targoscale)) != I1PRO_OK) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
				free(mbuf);
				a1logd(p->log,2,"i1pro_imp_measure trial measure failed\n");
				return ev;  
			}
		}

		a1logd(p->log,2,"Compute optimal integration time\n");
		/* For adaptive mode, compute a new integration time and gain mode */
		/* in order to optimise the sensor values. */
		if ((ev = i1pro_optimise_sensor(p, &s->inttime, &s->gainmode, 
		         s->inttime, s->gainmode, 1, 1, s->targoscale, optscale)) != I1PRO_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
			free(mbuf);
			a1logd(p->log,2,"i1pro_imp_measure optimise sensor failed\n");
			return ev;
		}
		a1logd(p->log,2,"Computed optimal emiss inttime %f and gainmode %d\n",s->inttime,s->gainmode);

		a1logd(p->log,2,"Interpolate dark calibration reference\n");
		if ((ev = i1pro_interp_dark(p, s->dark_data, s->inttime, s->gainmode)) != I1PRO_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
			free(mbuf);
			a1logd(p->log,2,"i1pro_imp_measure interplate dark ref failed\n");
			return ev;
		}
		s->dark_valid = 1;
		s->dark_int_time = s->inttime;
		s->dark_gain_mode = s->gainmode;

		/* Recompute number of measurements and realloc measurement buffer */
		free(mbuf);
		nummeas = i1pro_comp_nummeas(p, s->wreadtime, s->inttime);
		maxnummeas = i1pro_comp_nummeas(p, s->maxscantime, s->inttime);
		if (maxnummeas < nummeas)
			maxnummeas = nummeas;
		mbsize = m->nsen * 2 * maxnummeas;
		if ((mbuf = (unsigned char *)malloc(sizeof(unsigned char) * mbsize)) == NULL) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
			a1logd(p->log,1,"i1pro_imp_measure malloc %d bytes failed (7)\n",mbsize);
			return I1PRO_INT_MALLOC;
		}

	} else if (s->reflective) {

		DISDPLOT

		a1logd(p->log,2,"Doing on the fly black calibration_1 with nummeas %d int_time %f, gainmode %d\n",
		                                                   nummeas, s->inttime, s->gainmode);

		if ((ev = i1pro_dark_measure_1(p, nummeas, &s->inttime, s->gainmode, buf, bsize))
		                                                                           != I1PRO_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
			free(buf);
			free(mbuf);
			a1logd(p->log,2,"i1pro_imp_measure dak measure 1 failed\n");
			return ev;
		}

		ENDPLOT
	}
	/* Take a measurement reading using the current mode. */
	/* Converts to completely processed output readings. */

	a1logd(p->log,2,"Do main measurement reading\n");

	/* Indicate to the user that they can now scan the instrument, */
	/* after a little delay that allows for the instrument reaction time. */
	if (s->scan) {
		/* 500msec delay, 1KHz for 200 msec */
		msec_beep(200 + (int)(s->lamptime * 1000.0 + 0.5), 1000, 200);
	}

	/* Retry loop for certaing cases */
	for (;;) {

		/* Trigger measure and gather raw readings */
		if ((ev = i1pro_read_patches_1(p, nummeas, maxnummeas, &s->inttime, s->gainmode,
		                                       &nmeasuered, mbuf, mbsize)) != I1PRO_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
			if (buf != NULL)
				free(buf);
			free(mbuf);
			a1logd(p->log,2,"i1pro_imp_measure failed at i1pro_read_patches_1\n");
			return ev;
		}

		/* Complete reflective black reference measurement */
		if (s->reflective) {
			a1logd(p->log,3,"Calling black calibration_2 calc with nummeas %d, inttime %f, gainmode %d\n", nummeas, s->inttime,s->gainmode);
			if ((ev = i1pro_dark_measure_2(p, s->dark_data,
				            nummeas, s->inttime, s->gainmode, buf, bsize)) != I1PRO_OK) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
				free(buf);
				free(mbuf);
				a1logd(p->log,2,"i1pro_imp_measure failed at i1pro_dark_measure_2\n");
				return ev;
			}
			s->dark_valid = 1;
			s->dark_int_time = s->inttime;
			s->dark_gain_mode = s->gainmode;
			free(buf);
		}

		/* Process the raw measurement readings into final spectral readings */
		ev = i1pro_read_patches_2(p, &duration, specrd, nvals, s->inttime, s->gainmode,
		                                              nmeasuered, mbuf, mbsize);
		/* Special case display mode read. If the sensor is saturated, and */
		/* we haven't already done so, switch to the alternate integration time */
		/* and try again. */
		if (s->emiss && !s->scan && !s->adaptive
		 && ev == I1PRO_RD_SENSORSATURATED
		 && s->dispswap < 2) {
			double *tt, tv;

			if (s->dispswap == 0) {
				a1logd(p->log,2,"Switching to alternate display integration time %f seconds\n",s->dark_int_time2);
				tv = s->inttime; s->inttime = s->dark_int_time2; s->dark_int_time2 = tv;
				tt = s->dark_data; s->dark_data = s->dark_data2; s->dark_data2 = tt;
				s->dispswap = 1;
			} else if (s->dispswap == 1) {
				a1logd(p->log,2,"Switching to 2nd alternate display integration time %f seconds\n",s->dark_int_time3);
				/* Undo first swap */
				tv = s->inttime; s->inttime = s->dark_int_time2; s->dark_int_time2 = tv;
				tt = s->dark_data; s->dark_data = s->dark_data2; s->dark_data2 = tt;
				/* Do 2nd swap */
				tv = s->inttime; s->inttime = s->dark_int_time3; s->dark_int_time3 = tv;
				tt = s->dark_data; s->dark_data = s->dark_data3; s->dark_data3 = tt;
				s->dispswap = 2;
			}
			/* Recompute number of measurements and realloc measurement buffer */
			free(mbuf);
			nummeas = i1pro_comp_nummeas(p, s->wreadtime, s->inttime);
			maxnummeas = i1pro_comp_nummeas(p, s->maxscantime, s->inttime);
			if (maxnummeas < nummeas)
				maxnummeas = nummeas;
			mbsize = m->nsen * 2 * maxnummeas;
			if ((mbuf = (unsigned char *)malloc(sizeof(unsigned char) * mbsize)) == NULL) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
				a1logd(p->log,1,"i1pro_imp_measure malloc %d bytes failed (7)\n",mbsize);
				return I1PRO_INT_MALLOC;
			}
			continue;			/* Do the measurement again */
		}

		if (ev != I1PRO_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
			free(mbuf);
			a1logd(p->log,2,"i1pro_imp_measure failed at i1pro_read_patches_2\n");
			return ev;
		}
		break;		/* Don't repeat */
	}
	free(mbuf);

	/* Transfer spectral and convert to XYZ */
	if ((ev = i1pro_conv2XYZ(p, vals, nvals, specrd, clamp)) != I1PRO_OK) {
		free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
		a1logd(p->log,2,"i1pro_imp_measure failed at i1pro_conv2XYZ\n");
		return ev;
	}
	free_dmatrix(specrd, 0, nvals-1, 0, m->nwav[m->highres]-1);
	
	if (nvals > 0)
		vals[0].duration = duration;	/* Possible flash duration */
	
	/* Update log counters */
	if (s->reflective) {
		if (s->scan)
			m->acount++;
		else {
			m->rpinttime = s->inttime;
			m->rpcount++;
		}
	}
	
	a1logd(p->log,3,"i1pro_imp_measure sucessful return\n");
	if (user_trig)
		return I1PRO_USER_TRIG;
	return ev; 
}

/* - - - - - - - - - - - - - - - - */
/*

	Determining the refresh rate for a refresh type display.

	This is easy when the max sample rate of the i1 is above
	the nyquist of the display, and will always be the case
	for the range we are prepared to measure (up to 100Hz)
	when using an Rev B, D or E, but is a problem for the
	rev A and ColorMunki, which can only sample at 113Hz.

	We work around this problem by detecting when
	we are measuring an alias of the refresh rate, and
	average the aliasing corrected measurements.

	If there is no aparent refresh, or the refresh rate is not determinable,
	return a period of 0.0 and inst_ok;
*/

i1pro_code i1pro_measure_rgb(i1pro *p, double *inttime, double *rgb);

#ifndef PSRAND32L 
# define PSRAND32L(S) ((S) * 1664525L + 1013904223L)
#endif
#undef FREQ_SLOW_PRECISE	/* [und] Interpolate then autocorrelate, else autc & filter */
#define NFSAMPS 80		/* Number of samples to read */
#define NFMXTIME 6.0	/* Maximum time to take (2000 == 6) */
#define PBPMS 20		/* bins per msec */
#define PERMIN ((1000 * PBPMS)/40)	/* 40 Hz */
#define PERMAX ((1000 * PBPMS)/4)	/* 4 Hz*/
#define NPER (PERMAX - PERMIN + 1)
#define PWIDTH (8 * PBPMS)			/* 8 msec bin spread to look for peak in */
#define MAXPKS 20					/* Number of peaks to find */
#define TRIES 8	 					/* Number of different sample rates to try */

i1pro_code i1pro_imp_meas_refrate(
	i1pro *p,
	double *ref_rate
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
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

	a1logd(p->log,2,"i1pro_imp_meas_refrate called\n");

	if (ref_rate != NULL)
		*ref_rate = 0.0;

	if (!s->emiss) {
		a1logd(p->log,2,"i1pro_imp_meas_refrate not in emissive mode\n");
		return I1PRO_UNSUPPORTED;
	}

	for (mm = 0; mm < TRIES; mm++) {
		rfreq[mm] = 0.0;
		npeaks = 0;			/* Number of peaks */
		nummeas = NFSAMPS;
		multimeas = dmatrix(0, nummeas-1, -1, m->nwav[m->highres]-1);

		if (mm == 0)
			inttime = m->min_int_time;
		else {
			double rval, dmm;
			randn = PSRAND32L(randn);
			rval = (double)randn/4294967295.0;
			dmm = ((double)mm + rval - 0.5)/(TRIES - 0.5);
			inttime = m->min_int_time * (1.0 + dmm * 0.80);
		}

		if ((ev = i1pro_read_patches_all(p, multimeas, nummeas, &inttime, 0)) != inst_ok) {
			free_dmatrix(multimeas, 0, nummeas-1, 0, m->nwav[m->highres]-1);
			return ev;
		} 

		rsamp[tix] = 1.0/inttime;

		/* Convert the samples to RGB */
		for (i = 0; i < nummeas && i < NFSAMPS; i++) {
			samp[i].sec = i * inttime;
			samp[i].rgb[0] = samp[i].rgb[1] = samp[i].rgb[2] = 0.0;
			for (j = 0; j < m->nwav[m->highres]; j++) {
				double wl = XSPECT_WL(m->wl_short[m->highres], m->wl_long[m->highres], m->nwav[m->highres], j);

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
		free_dmatrix(multimeas, 0, nummeas-1, 0, m->nwav[m->highres]-1);
		nfsamps = i;

		a1logd(p->log, 3, "i1pro_meas_refrate: Read %d samples for refresh calibration\n",nfsamps);

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
				a1loge(p->log, inst_internal_error, "i1pro_meas_refrate: malloc failed\n");
				return I1PRO_INT_MALLOC;
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
				a1loge(p->log, inst_internal_error, "i1pro_meas_refrate: malloc failed\n");
				for (j = 0; j < 3; j++)
					free(bins[j]);
				return I1PRO_INT_MALLOC;
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
			error("i1pro: Not enough space for lanczos 2 filter");
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
				return I1PRO_OK;
			}
		}
	} else {
		a1logd(p->log, 3, "Not enough tries suceeded to determine refresh rate\n");
	}

	return I1PRO_RD_NOREFR_FOUND; 
}
#undef NFSAMPS 
#undef PBPMS
#undef PERMIN
#undef PERMAX
#undef NPER
#undef PWIDTH

/* - - - - - - - - - - - - - - - - - - - - - - */
/* i1 refspot calibration/log stored on instrument */
/* RevA..D only! */

/* Restore the reflective spot calibration information from the EEPRom */
/* Always returns success, even if the restore fails, */
/* which may happen for an instrument that's never been used or had calibration */
/* written to its EEProm */
/* RevA..D only! */
i1pro_code i1pro_restore_refspot_cal(i1pro *p) {
	int chsum1, *chsum2;
	int *ip, i;
	unsigned int count;
	double *dp;
	unsigned char buf[256];
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[i1p_refl_spot];		/* NOT current mode, refspot mode */
	i1key offst = 0;							/* Offset to copy to use */
	i1pro_code ev = I1PRO_OK;
	int o_nsen;									/* Actual nsen data */

	a1logd(p->log,2,"Doing Restoring reflective spot calibration information from the EEProm\n");

	chsum1 = m->data->checksum(m->data, 0);
	if ((chsum2 = m->data->get_int(m->data, key_checksum, 0)) == NULL || chsum1 != *chsum2) {
		offst = key_2logoff;
		chsum1 = m->data->checksum(m->data, key_2logoff);
		if ((chsum2 = m->data->get_int(m->data, key_checksum + key_2logoff, 0)) == NULL
		     || chsum1 != *chsum2) {
			a1logd(p->log,2,"Neither EEPRom checksum was valid\n");
			return I1PRO_OK;
		}
	}

	/* Get the calibration gain mode */
	if ((ip = m->data->get_ints(m->data, &count, key_gainmode + offst)) == NULL || count < 1) {
		a1logd(p->log,2,"Failed to read calibration gain mode from EEPRom\n");
		return I1PRO_OK;
	}
	if (ip[0] == 0) {
#ifdef USE_HIGH_GAIN_MODE
		s->gainmode = 1;
#else
		s->gainmode = 0;
		a1logd(p->log,2,"Calibration gain mode was high, and high gain not compiled in\n");
		return I1PRO_OK;
#endif /* !USE_HIGH_GAIN_MODE */
	} else
		s->gainmode = 0;

	/* Get the calibration integration time */
	if ((dp = m->data->get_doubles(m->data, &count, key_inttime + offst)) == NULL || count < 1) {
		a1logd(p->log,2,"Failed to read calibration integration time from EEPRom\n");
		return I1PRO_OK;
	}
	s->inttime = dp[0];
	if (s->inttime < m->min_int_time)	/* Hmm. EEprom is occasionaly screwed up */
		s->inttime = m->min_int_time;

	/* Get the dark data */
	if ((ip = m->data->get_ints(m->data, &count, key_darkreading + offst)) == NULL
	                                                              || count != 128) {
		a1logv(p->log,1,"Failed to read calibration dark data from EEPRom\n");
		return I1PRO_OK;
	}

	/* Convert back to a single raw big endian instrument readings */
	for (i = 0; i < 128; i++) {
		buf[i * 2 + 0] = (ip[i] >> 8) & 0xff;
		buf[i * 2 + 1] = ip[i] & 0xff;
	}

	/* Convert to calibration data */
	a1logd(p->log,3,"Calling black calibration_2 calc with nummeas %d, inttime %f, gainmode %d\n", 1, s->inttime,s->gainmode);
	o_nsen = m->nsen;
	m->nsen = 128;		/* Assume EEprom cal data is <= Rev D format */
	if ((ev = i1pro_dark_measure_2(p, s->dark_data, 1, s->inttime, s->gainmode,
		                                                 buf, 256)) != I1PRO_OK) {
		a1logd(p->log,2,"Failed to convert EEProm dark data to calibration\n");
		m->nsen = o_nsen;
		return I1PRO_OK;
	}

	/* We've sucessfully restored the dark calibration */
	s->dark_valid = 1;
	s->ddate = m->caldate;

	/* Get the white calibration data */
	if ((ip = m->data->get_ints(m->data, &count, key_whitereading + offst)) == NULL
	                                                               || count != 128) {
		a1logd(p->log,2,"Failed to read calibration white data from EEPRom\n");
		m->nsen = o_nsen;
		return I1PRO_OK;
	}

	/* Convert back to a single raw big endian instrument readings */
	for (i = 0; i < 128; i++) {
		buf[i * 2 + 0] = (ip[i] >> 8) & 0xff;
		buf[i * 2 + 1] = ip[i] & 0xff;
	}

	/* Convert to calibration data */
	m->nsen = 128;		/* Assume EEprom cal data is <= Rev D format */
	if ((ev = i1pro_whitemeasure_buf(p, s->cal_factor[0], s->cal_factor[1], s->white_data,
	                s->inttime, s->gainmode, buf)) != I1PRO_OK) {
		/* This may happen for an instrument that's never been used */
		a1logd(p->log,2,"Failed to convert EEProm white data to calibration\n");
		m->nsen = o_nsen;
		return I1PRO_OK;
	}
	m->nsen = o_nsen;

	/* Check a reflective white measurement, and check that */
	/* it seems reasonable. Return I1PRO_OK if it is, error if not. */
	/* (Using cal_factor[] as temp.) */
	if ((ev = i1pro_check_white_reference1(p, s->cal_factor[0])) != I1PRO_OK) {
		/* This may happen for an instrument that's never been used */
		a1logd(p->log,2,"Failed to convert EEProm white data to calibration\n");
		return I1PRO_OK;
	}
	/* Compute a calibration factor given the reading of the white reference. */
	ev = i1pro_compute_white_cal(p, s->cal_factor[0], m->white_ref[0], s->cal_factor[0],
	                               s->cal_factor[1], m->white_ref[1], s->cal_factor[1], 1);
	if (ev != I1PRO_RD_TRANSWHITEWARN && ev != I1PRO_OK) {
		a1logd(p->log,2,"i1pro_compute_white_cal failed to convert EEProm data to calibration\n");
		return I1PRO_OK;
	}

	/* We've sucessfully restored the calibration */
	s->cal_valid = 1;
	s->cfdate = m->caldate;

	return I1PRO_OK;
}

/* Save the reflective spot calibration information to the EEPRom data object. */
/* Note we don't actually write to the EEProm here! */
/* For RevA..D only! */
static i1pro_code i1pro_set_log_data(i1pro *p) {
	int *ip, i;
	unsigned int count;
	double *dp;
	double absmeas[128];
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[i1p_refl_spot];		/* NOT current mode, refspot mode */
	i1key offst = 0;							/* Offset to copy to use */
	i1pro_code ev = I1PRO_OK;

	a1logd(p->log,3,"i1pro_set_log_data called\n");

	if (s->dark_valid == 0 || s->cal_valid == 0)
		return I1PRO_INT_NO_CAL_TO_SAVE;

	/* Set the calibration gain mode */
	if ((ip = m->data->get_ints(m->data, &count, key_gainmode + offst)) == NULL || count < 1) {
		a1logd(p->log,2,"Failed to access calibration gain mode from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	if (s->gainmode == 0)
		ip[0] = 1;
	else
		ip[0] = 0;

	/* Set the calibration integration time */
	if ((dp = m->data->get_doubles(m->data, &count, key_inttime + offst)) == NULL || count < 1) {
		a1logd(p->log,2,"Failed to read calibration integration time from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	dp[0] = s->inttime;

	/* Set the dark data */
	if ((ip = m->data->get_ints(m->data, &count, key_darkreading + offst)) == NULL
	                                                              || count != 128) {
		a1logd(p->log,2,"Failed to access calibration dark data from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}

	/* Convert abs dark_data to raw data */
	if ((ev = i1pro_absraw_to_meas(p, ip, s->dark_data, s->inttime, s->gainmode)) != I1PRO_OK)
		return ev;

	/* Add back black level to white data */
	for (i = 0; i < 128; i++)
		absmeas[i] = s->white_data[i] + s->dark_data[i];

	/* Get the white data */
	if ((ip = m->data->get_ints(m->data, &count, key_whitereading + offst)) == NULL
	                                                               || count != 128) {
		a1logd(p->log,2,"Failed to access calibration white data from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}

	/* Convert abs white_data to raw data */
	if ((ev = i1pro_absraw_to_meas(p, ip, absmeas, s->inttime, s->gainmode)) != I1PRO_OK)
		return ev;

	/* Set all the log counters */

	/* Total Measure (Emis/Remis/Ambient/Trans/Cal) count */
	if ((ip = m->data->get_ints(m->data, &count, key_meascount)) == NULL || count < 1) {
		a1logd(p->log,2,"Failed to access meascount log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	ip[0] = m->meascount;

	/* Remspotcal last calibration date */
	if ((ip = m->data->get_ints(m->data, &count, key_caldate)) == NULL || count < 1) {
		a1logd(p->log,2,"Failed to access caldate log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	ip[0] = m->caldate;

	/* Remission spot measure count at last Remspotcal. */
	if ((ip = m->data->get_ints(m->data, &count, key_calcount)) == NULL || count < 1) {
		a1logd(p->log,2,"Failed to access calcount log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	ip[0] = m->calcount;

	/* Last remision spot reading integration time */
	if ((dp = m->data->get_doubles(m->data, &count, key_rpinttime)) == NULL || count < 1) {
		a1logd(p->log,2,"Failed to access rpinttime log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	dp[0] = m->rpinttime;

	/* Remission spot measure count */
	if ((ip = m->data->get_ints(m->data, &count, key_rpcount)) == NULL || count < 1) {
		a1logd(p->log,2,"Failed to access rpcount log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	ip[0] = m->rpcount;

	/* Remission scan measure count (??) */
	if ((ip = m->data->get_ints(m->data, &count, key_acount)) == NULL || count < 1) {
		a1logd(p->log,2,"Failed to access acount log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	ip[0] = m->acount;

	/* Total lamp usage time in seconds (??) */
	if ((dp = m->data->get_doubles(m->data, &count, key_lampage)) == NULL || count < 1) {
		a1logd(p->log,2,"Failed to access lampage log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	dp[0] = m->lampage;

	a1logd(p->log,5,"i1pro_set_log_data done\n");

	return I1PRO_OK;
}

/* Update the single remission calibration and instrument usage log */
/* For RevA..D only! */
i1pro_code i1pro_update_log(i1pro *p) {
	i1pro_code ev = I1PRO_OK;
#ifdef ENABLE_WRITE
	i1proimp *m = (i1proimp *)p->m;
	unsigned char *buf;		/* Buffer to write to EEProm */
	unsigned int len;

	a1logd(p->log,5,"i1pro_update_log:\n");

	/* Copy refspot calibration and log data to EEProm data store */ 
	if ((ev = i1pro_set_log_data(p)) != I1PRO_OK) {
		a1logd(p->log,2,"i1pro_update_log i1pro_set_log_data failed\n");
		return ev;
	}

	/* Compute checksum and serialise into buffer ready to write */
	if ((ev = m->data->prep_section1(m->data, &buf, &len)) != I1PRO_OK) {
		a1logd(p->log,2,"i1pro_update_log prep_section1 failed\n");
		return ev;
	}

	/* First copy of log */
	if ((ev = i1pro_writeEEProm(p, buf, 0x0000, len)) != I1PRO_OK) {
		a1logd(p->log,2,"i1pro_update_log i1pro_writeEEProm 0x0000 failed\n");
		return ev;
	}
	/* Second copy of log */
	if ((ev = i1pro_writeEEProm(p, buf, 0x0800, len)) != I1PRO_OK) {
		a1logd(p->log,2,"i1pro_update_log i1pro_writeEEProm 0x0800 failed\n");
		return ev;
	}
	free(buf);

	a1logd(p->log,5,"i1pro_update_log done\n");
#else
	a1logd(p->log,5,"i1pro_update_log: skipped as EPRom write is disabled\n");
#endif

	return ev;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Save the calibration for all modes, stored on local system */

#ifdef ENABLE_NONVCAL

/* non-volatile save/restor state */
typedef struct {
	int ef;					/* Error flag, 1 = write failed, 2 = close failed */
	unsigned int chsum;		/* Checksum */
	int nbytes;				/* Number of bytes checksummed */
} i1pnonv;

static void update_chsum(i1pnonv *x, unsigned char *p, int nn) {
	int i;
	for (i = 0; i < nn; i++, p++)
		x->chsum = ((x->chsum << 13) | (x->chsum >> (32-13))) + *p;
	x->nbytes += nn;
}

/* Write an array of ints to the file. Set the error flag to nz on error */
static void write_ints(i1pnonv *x, FILE *fp, int *dp, int n) {

	if (fwrite((void *)dp, sizeof(int), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(int));
	}
}

/* Write an array of doubles to the file. Set the error flag to nz on error */
static void write_doubles(i1pnonv *x, FILE *fp, double *dp, int n) {

	if (fwrite((void *)dp, sizeof(double), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(double));
	}
}

/* Write an array of time_t's to the file. Set the error flag to nz on error */
/* (This will cause file checksum fail if different executables on the same */
/*  system have different time_t values) */
static void write_time_ts(i1pnonv *x, FILE *fp, time_t *dp, int n) {

	if (fwrite((void *)dp, sizeof(time_t), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(time_t));
	}
}

/* Read an array of ints from the file. Set the error flag to nz on error */
static void read_ints(i1pnonv *x, FILE *fp, int *dp, int n) {

	if (fread((void *)dp, sizeof(int), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(int));
	}
}

/* Read an array of doubles from the file. Set the error flag to nz on error */
static void read_doubles(i1pnonv *x, FILE *fp, double *dp, int n) {

	if (fread((void *)dp, sizeof(double), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(double));
	}
}

/* Read an array of time_t's from the file. Set the error flag to nz on error */
/* (This will cause file checksum fail if different executables on the same */
/*  system have different time_t values) */
static void read_time_ts(i1pnonv *x, FILE *fp, time_t *dp, int n) {

	if (fread((void *)dp, sizeof(time_t), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(time_t));
	}
}

i1pro_code i1pro_save_calibration(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code ev = I1PRO_OK;
	i1pro_state *s;
	int i;
	char nmode[10];
	char cal_name[100];		/* Name */
	char **cal_paths = NULL;
	int no_paths = 0;
	FILE *fp;
	i1pnonv x;
	int ss;
	int argyllversion = ARGYLL_VERSION;

	strcpy(nmode, "w");
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	strcat(nmode, "b");
#endif

	/* Create the file name */
	sprintf(cal_name, "ArgyllCMS/.i1p_%d.cal", m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_write, xdg_user, cal_name)) < 1) {
		a1logd(p->log,1,"i1pro_save_calibration xdg_bds returned no paths\n");
		return I1PRO_INT_CAL_SAVE;
	}

	a1logd(p->log,2,"i1pro_save_calibration saving to file '%s'\n",cal_paths[0]);

	if (create_parent_directories(cal_paths[0])
	 || (fp = fopen(cal_paths[0], nmode)) == NULL) {
		a1logd(p->log,2,"i1pro_save_calibration failed to open file for writing\n");
		xdg_free(cal_paths, no_paths);
		return I1PRO_INT_CAL_SAVE;
	}
	
	x.ef = 0;
	x.chsum = 0;
	x.nbytes = 0;

	/* A crude structure signature */
	ss = sizeof(i1pro_state) + sizeof(i1proimp);

	/* Some file identification */
	write_ints(&x, fp, &argyllversion, 1);
	write_ints(&x, fp, &ss, 1);
	write_ints(&x, fp, &m->serno, 1);
	write_ints(&x, fp, (int *)&m->nraw, 1);
	write_ints(&x, fp, (int *)&m->nwav[0], 1);
	write_ints(&x, fp, (int *)&m->nwav[1], 1);

	/* For each mode, save the calibration if it's valid */
	for (i = 0; i < i1p_no_modes; i++) {
		s = &m->ms[i];

		/* Mode identification */
		write_ints(&x, fp, &s->emiss, 1);
		write_ints(&x, fp, &s->trans, 1);
		write_ints(&x, fp, &s->reflective, 1);
		write_ints(&x, fp, &s->scan, 1);
		write_ints(&x, fp, &s->flash, 1);
		write_ints(&x, fp, &s->ambient, 1);
		write_ints(&x, fp, &s->adaptive, 1);

		/* Configuration calibration is valid for */
		write_ints(&x, fp, &s->gainmode, 1);
		write_doubles(&x, fp, &s->inttime, 1);

		/* Calibration information */
		write_ints(&x, fp, &s->wl_valid, 1);
		write_time_ts(&x, fp, &s->wldate, 1);
		write_doubles(&x, fp, &s->wl_led_off, 1);
		write_ints(&x, fp, &s->dark_valid, 1);
		write_time_ts(&x, fp, &s->ddate, 1);
		write_doubles(&x, fp, &s->dark_int_time, 1);
		write_doubles(&x, fp, s->dark_data-1, m->nraw+1);
		write_doubles(&x, fp, &s->dark_int_time2, 1);
		write_doubles(&x, fp, s->dark_data2-1, m->nraw+1);
		write_doubles(&x, fp, &s->dark_int_time3, 1);
		write_doubles(&x, fp, s->dark_data3-1, m->nraw+1);
		write_ints(&x, fp, &s->dark_gain_mode, 1);

		if (!s->emiss) {
			write_ints(&x, fp, &s->cal_valid, 1);
			write_time_ts(&x, fp, &s->cfdate, 1);
			write_doubles(&x, fp, s->cal_factor[0], m->nwav[0]);
			write_doubles(&x, fp, s->cal_factor[1], m->nwav[1]);
			write_doubles(&x, fp, s->white_data-1, m->nraw+1);
		}
		
		write_ints(&x, fp, &s->idark_valid, 1);
		write_time_ts(&x, fp, &s->iddate, 1);
		write_doubles(&x, fp, s->idark_int_time, 4);
		write_doubles(&x, fp, s->idark_data[0]-1, m->nraw+1);
		write_doubles(&x, fp, s->idark_data[1]-1, m->nraw+1);
		write_doubles(&x, fp, s->idark_data[2]-1, m->nraw+1);
		write_doubles(&x, fp, s->idark_data[3]-1, m->nraw+1);
	}

	a1logd(p->log,3,"nbytes = %d, Checkum = 0x%x\n",x.nbytes,x.chsum);
	write_ints(&x, fp, (int *)&x.chsum, 1);

	if (fclose(fp) != 0)
		x.ef = 2;

	if (x.ef != 0) {
		a1logd(p->log,2,"Writing calibration file failed with %d\n",x.ef);
		delete_file(cal_paths[0]);
		return I1PRO_INT_CAL_SAVE;
	} else {
		a1logd(p->log,2,"Writing calibration file succeeded\n");
	}
	xdg_free(cal_paths, no_paths);

	return ev;
}

/* Restore the all modes calibration from the local system */
i1pro_code i1pro_restore_calibration(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code ev = I1PRO_OK;
	i1pro_state *s, ts;
	int i, j;
	char nmode[10];
	char cal_name[100];		/* Name */
	char **cal_paths = NULL;
	int no_paths = 0;
	FILE *fp;
	i1pnonv x;
	int argyllversion;
	int ss, serno, nraw, nwav0, nwav1, nbytes, chsum1, chsum2;

	strcpy(nmode, "r");
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	strcat(nmode, "b");
#endif
	/* Create the file name */
	sprintf(cal_name, "ArgyllCMS/.i1p_%d.cal" SSEPS "color/.i1p_%d.cal", m->serno, m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_read, xdg_user, cal_name)) < 1) {
		a1logd(p->log,2,"i1pro_restore_calibration xdg_bds failed to locate file'\n");
		return I1PRO_INT_CAL_RESTORE;
	}

	a1logd(p->log,2,"i1pro_restore_calibration restoring from file '%s'\n",cal_paths[0]);

	/* Check the last modification time */
	{
		struct sys_stat sbuf;

		if (sys_stat(cal_paths[0], &sbuf) == 0) {
			m->lo_secs = time(NULL) - sbuf.st_mtime;
			a1logd(p->log,2,"i1pro_restore_calibration: %d secs from instrument last open\n",m->lo_secs);
		} else {
			a1logd(p->log,2,"i1pro_restore_calibration: stat on file failed\n");
		}
	}

	if ((fp = fopen(cal_paths[0], nmode)) == NULL) {
		a1logd(p->log,2,"i1pro_restore_calibration failed to open file for reading\n");
		xdg_free(cal_paths, no_paths);
		return I1PRO_INT_CAL_RESTORE;
	}

	x.ef = 0;
	x.chsum = 0;
	x.nbytes = 0;

	/* Check the file identification */
	read_ints(&x, fp, &argyllversion, 1);
	read_ints(&x, fp, &ss, 1);
	read_ints(&x, fp, &serno, 1);
	read_ints(&x, fp, &nraw, 1);
	read_ints(&x, fp, &nwav0, 1);
	read_ints(&x, fp, &nwav1, 1);
	if (x.ef != 0
	 || argyllversion != ARGYLL_VERSION
	 || ss != (sizeof(i1pro_state) + sizeof(i1proimp))
	 || serno != m->serno
	 || nraw != m->nraw
	 || nwav0 != m->nwav[0]
	 || nwav1 != m->nwav[1]) {
		a1logd(p->log,2,"Identification didn't verify\n");
		goto reserr;
	}

	/* Do a dummy read to check the checksum */
	for (i = 0; i < i1p_no_modes; i++) {
		int di;
		double dd;
		time_t dt;
		int emiss, trans, reflective, ambient, scan, flash, adaptive;

		s = &m->ms[i];

		/* Mode identification */
		read_ints(&x, fp, &emiss, 1);
		read_ints(&x, fp, &trans, 1);
		read_ints(&x, fp, &reflective, 1);
		read_ints(&x, fp, &scan, 1);
		read_ints(&x, fp, &flash, 1);
		read_ints(&x, fp, &ambient, 1);
		read_ints(&x, fp, &adaptive, 1);

		/* Configuration calibration is valid for */
		read_ints(&x, fp, &di, 1);
		read_doubles(&x, fp, &dd, 1);

		/* Calibration information */
		read_ints(&x, fp, &di, 1);
		read_time_ts(&x, fp, &dt, 1);
		read_doubles(&x, fp, &dd, 1);

		read_ints(&x, fp, &di, 1);
		read_time_ts(&x, fp, &dt, 1);
		read_doubles(&x, fp, &dd, 1);
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
		read_doubles(&x, fp, &dd, 1);
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
		read_doubles(&x, fp, &dd, 1);
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
		read_ints(&x, fp, &di, 1);

		if (!s->emiss) {
			read_ints(&x, fp, &di, 1);
			read_time_ts(&x, fp, &dt, 1);
			for (j = 0; j < m->nwav[0]; j++)
				read_doubles(&x, fp, &dd, 1);
			for (j = 0; j < m->nwav[1]; j++)
				read_doubles(&x, fp, &dd, 1);
			for (j = -1; j < m->nraw; j++)
				read_doubles(&x, fp, &dd, 1);
		}
		
		read_ints(&x, fp, &di, 1);
		read_time_ts(&x, fp, &dt, 1);
		for (j = 0; j < 4; j++)
			read_doubles(&x, fp, &dd, 1);
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
	}

	chsum1 = x.chsum;
	nbytes = x.nbytes;
	read_ints(&x, fp, &chsum2, 1);
	
	if (x.ef != 0
	 || chsum1 != chsum2) {
		a1logd(p->log,2,"Checksum didn't verify, bytes %d, got 0x%x, expected 0x%x\n",nbytes,chsum1, chsum2);
		goto reserr;
	}

	rewind(fp);
	x.ef = 0;
	x.chsum = 0;
	x.nbytes = 0;

	/* Allocate space in temp structure */

	ts.dark_data = dvectorz(-1, m->nraw-1);  
	ts.dark_data2 = dvectorz(-1, m->nraw-1);  
	ts.dark_data3 = dvectorz(-1, m->nraw-1);  
	ts.cal_factor[0] = dvectorz(0, m->nwav[0]-1);
	ts.cal_factor[1] = dvectorz(0, m->nwav[1]-1);
	ts.white_data = dvectorz(-1, m->nraw-1);
	ts.idark_data = dmatrixz(0, 3, -1, m->nraw-1);  

	/* Read the identification */
	read_ints(&x, fp, &argyllversion, 1);
	read_ints(&x, fp, &ss, 1);
	read_ints(&x, fp, &m->serno, 1);
	read_ints(&x, fp, (int *)&m->nraw, 1);
	read_ints(&x, fp, (int *)&m->nwav[0], 1);
	read_ints(&x, fp, (int *)&m->nwav[1], 1);

	/* For each mode, restore the calibration if it's valid */
	for (i = 0; i < i1p_no_modes; i++) {
		s = &m->ms[i];

		/* Mode identification */
		read_ints(&x, fp, &ts.emiss, 1);
		read_ints(&x, fp, &ts.trans, 1);
		read_ints(&x, fp, &ts.reflective, 1);
		read_ints(&x, fp, &ts.scan, 1);
		read_ints(&x, fp, &ts.flash, 1);
		read_ints(&x, fp, &ts.ambient, 1);
		read_ints(&x, fp, &ts.adaptive, 1);

		/* Configuration calibration is valid for */
		read_ints(&x, fp, &ts.gainmode, 1);
		read_doubles(&x, fp, &ts.inttime, 1);

		/* Calibration information: */

		/* Wavelength */
		read_ints(&x, fp, &ts.wl_valid, 1);
		read_time_ts(&x, fp, &ts.wldate, 1);
		read_doubles(&x, fp, &ts.wl_led_off, 1);

		/* Static Dark */
		read_ints(&x, fp, &ts.dark_valid, 1);
		read_time_ts(&x, fp, &ts.ddate, 1);
		read_doubles(&x, fp, &ts.dark_int_time, 1);
		read_doubles(&x, fp, ts.dark_data-1, m->nraw+1);
		read_doubles(&x, fp, &ts.dark_int_time2, 1);
		read_doubles(&x, fp, ts.dark_data2-1, m->nraw+1);
		read_doubles(&x, fp, &ts.dark_int_time3, 1);
		read_doubles(&x, fp, ts.dark_data3-1, m->nraw+1);
		read_ints(&x, fp, &ts.dark_gain_mode, 1);

		if (!ts.emiss) {
			/* Reflective */
			read_ints(&x, fp, &ts.cal_valid, 1);
			read_time_ts(&x, fp, &ts.cfdate, 1);
			read_doubles(&x, fp, ts.cal_factor[0], m->nwav[0]);
			read_doubles(&x, fp, ts.cal_factor[1], m->nwav[1]);
			read_doubles(&x, fp, ts.white_data-1, m->nraw+1);
		}
		
		/* Adaptive Dark */
		read_ints(&x, fp, &ts.idark_valid, 1);
		read_time_ts(&x, fp, &ts.iddate, 1);
		read_doubles(&x, fp, ts.idark_int_time, 4);
		read_doubles(&x, fp, ts.idark_data[0]-1, m->nraw+1);
		read_doubles(&x, fp, ts.idark_data[1]-1, m->nraw+1);
		read_doubles(&x, fp, ts.idark_data[2]-1, m->nraw+1);
		read_doubles(&x, fp, ts.idark_data[3]-1, m->nraw+1);

		/* If the configuration for this mode matches */
		/* that of the calibration, restore the calibration */
		/* for this mode. */
		if (x.ef == 0				/* No read error */
		 &&	s->emiss == ts.emiss
		 && s->trans == ts.trans
		 && s->reflective == ts.reflective
		 && s->scan == ts.scan
		 && s->flash == ts.flash
		 && s->ambient == ts.ambient
		 && s->adaptive == ts.adaptive
		 && (s->adaptive || fabs(s->inttime - ts.inttime) < 0.01)
		 && (s->adaptive || fabs(s->dark_int_time - ts.dark_int_time) < 0.01)
		 && (s->adaptive || fabs(s->dark_int_time2 - ts.dark_int_time2) < 0.01)
		 && (s->adaptive || fabs(s->dark_int_time3 - ts.dark_int_time3) < 0.01)
		 && (!s->adaptive || fabs(s->idark_int_time[0] - ts.idark_int_time[0]) < 0.01)
		 && (!s->adaptive || fabs(s->idark_int_time[1] - ts.idark_int_time[1]) < 0.01)
		 && (!s->adaptive || fabs(s->idark_int_time[2] - ts.idark_int_time[2]) < 0.01)
		 && (!s->adaptive || fabs(s->idark_int_time[3] - ts.idark_int_time[3]) < 0.01)
		) {
			/* Copy all the fields read above */
			s->emiss = ts.emiss;
			s->trans = ts.trans;
			s->reflective = ts.reflective;
			s->scan = ts.scan;
			s->flash = ts.flash;
			s->ambient = ts.ambient;
			s->adaptive = ts.adaptive;

			s->gainmode = ts.gainmode;
			s->inttime = ts.inttime;

			s->wl_valid = ts.wl_valid;
			s->wldate = ts.wldate;
			s->wl_led_off = ts.wl_led_off;

			s->dark_valid = ts.dark_valid;
			s->ddate = ts.ddate;
			s->dark_int_time = ts.dark_int_time;
			for (j = -1; j < m->nraw; j++)
				s->dark_data[j] = ts.dark_data[j];
			s->dark_int_time2 = ts.dark_int_time2;
			for (j = -1; j < m->nraw; j++)
				s->dark_data2[j] = ts.dark_data2[j];
			s->dark_int_time3 = ts.dark_int_time3;
			for (j = -1; j < m->nraw; j++)
				s->dark_data3[j] = ts.dark_data3[j];
			s->dark_gain_mode = ts.dark_gain_mode;
			if (!ts.emiss) {
				s->cal_valid = ts.cal_valid;
				s->cfdate = ts.cfdate;
				for (j = 0; j < m->nwav[0]; j++)
					s->cal_factor[0][j] = ts.cal_factor[0][j];
				for (j = 0; j < m->nwav[1]; j++)
					s->cal_factor[1][j] = ts.cal_factor[1][j];
				for (j = -1; j < m->nraw; j++)
					s->white_data[j] = ts.white_data[j];
			}
			s->idark_valid = ts.idark_valid;
			s->iddate = ts.iddate;
			for (j = 0; j < 4; j++)
				s->idark_int_time[j] = ts.idark_int_time[j];
			for (j = -1; j < m->nraw; j++)
				s->idark_data[0][j] = ts.idark_data[0][j];
			for (j = -1; j < m->nraw; j++)
				s->idark_data[1][j] = ts.idark_data[1][j];
			for (j = -1; j < m->nraw; j++)
				s->idark_data[2][j] = ts.idark_data[2][j];
			for (j = -1; j < m->nraw; j++)
				s->idark_data[3][j] = ts.idark_data[3][j];

		} else {
			a1logd(p->log,2,"Not restoring cal for mode %d since params don't match:\n",i);
			a1logd(p->log,2,"emis = %d : %d, trans = %d : %d, ref = %d : %d\n",s->emiss,ts.emiss,s->trans,ts.trans,s->reflective,ts.reflective);
			a1logd(p->log,2,"scan = %d : %d, flash = %d : %d, ambi = %d : %d, adapt = %d : %d\n",s->scan,ts.scan,s->flash,ts.flash,s->ambient,ts.ambient,s->adaptive,ts.adaptive);
			a1logd(p->log,2,"inttime = %f : %f\n",s->inttime,ts.inttime);
			a1logd(p->log,2,"darkit1 = %f : %f, 2 = %f : %f, 3 = %f : %f\n",s->dark_int_time,ts.dark_int_time,s->dark_int_time2,ts.dark_int_time2,s->dark_int_time3,ts.dark_int_time3);
			a1logd(p->log,2,"idarkit0 = %f : %f, 1 = %f : %f, 2 = %f : %f, 3 = %f : %f\n",s->idark_int_time[0],ts.idark_int_time[0],s->idark_int_time[1],ts.idark_int_time[1],s->idark_int_time[2],ts.idark_int_time[2],s->idark_int_time[3],ts.idark_int_time[3]);
		}
	}

	/* Free up temporary space */
	free_dvector(ts.dark_data, -1, m->nraw-1);  
	free_dvector(ts.dark_data2, -1, m->nraw-1);  
	free_dvector(ts.dark_data3, -1, m->nraw-1);  
	free_dvector(ts.white_data, -1, m->nraw-1);
	free_dmatrix(ts.idark_data, 0, 3, -1, m->nraw-1);  

	free_dvector(ts.cal_factor[0], 0, m->nwav[0]-1);
	free_dvector(ts.cal_factor[1], 0, m->nwav[1]-1);

	a1logd(p->log,5,"i1pro_restore_calibration done\n");
 reserr:;

	fclose(fp);
	xdg_free(cal_paths, no_paths);

	return ev;
}

i1pro_code i1pro_touch_calibration(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code ev = I1PRO_OK;
	char cal_name[100];		/* Name */
	char **cal_paths = NULL;
	int no_paths = 0;
	int rv;

	/* Locate the file name */
	sprintf(cal_name, "ArgyllCMS/.i1p_%d.cal" SSEPS "color/.i1p_%d.cal", m->serno, m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_read, xdg_user, cal_name)) < 1) {
		a1logd(p->log,2,"i1pro_restore_calibration xdg_bds failed to locate file'\n");
		return I1PRO_INT_CAL_TOUCH;
	}

	a1logd(p->log,2,"i1pro_touch_calibration touching file '%s'\n",cal_paths[0]);

	if ((rv = sys_utime(cal_paths[0], NULL)) != 0) {
		a1logd(p->log,2,"i1pro_touch_calibration failed with %d\n",rv);
		xdg_free(cal_paths, no_paths);
		return I1PRO_INT_CAL_TOUCH;
	}
	xdg_free(cal_paths, no_paths);

	return ev;
}

#endif /* ENABLE_NONVCAL */

/* ============================================================ */
/* Intermediate routines  - composite commands/processing */

/* Some sort of configuration needed get instrument ready. */
/* Does it have a sleep mode that we need to deal with ?? */
/* Note this always does a reset. */
i1pro_code
i1pro_establish_high_power(i1pro *p) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	int i;

	/* Get the current misc. status */
	if ((ev = i1pro_getmisc(p, &m->fwrev, NULL, &m->maxpve, NULL, &m->powmode)) != I1PRO_OK)
		return ev; 
	
	if (m->powmode != 8) {		/* In high power mode */
		if ((ev = i1pro_reset(p, 0x1f)) != I1PRO_OK)
			return ev;

		return I1PRO_OK;
	}

	a1logd(p->log,4,"Switching to high power mode\n");

	/* Switch to high power mode */
	if ((ev = i1pro_reset(p, 1)) != I1PRO_OK)
		return ev;

	/* Wait up to 1.5 seconds for it return high power indication */
	for (i = 0; i < 15; i++) {

		/* Get the current misc. status */
		if ((ev = i1pro_getmisc(p, &m->fwrev, NULL, &m->maxpve, NULL, &m->powmode)) != I1PRO_OK)
			return ev; 
		
		if (m->powmode != 8) {		/* In high power mode */
			if ((ev = i1pro_reset(p, 0x1f)) != I1PRO_OK)
				return ev;
	
			return I1PRO_OK;
		}

		msec_sleep(100);
	}

	/* Failed to switch into high power mode */
	return I1PRO_HW_HIGHPOWERFAIL;
}

/* Take a dark reference measurement - part 1 */
i1pro_code i1pro_dark_measure_1(
	i1pro *p,
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	unsigned char *buf,		/* USB reading buffer to use */
	unsigned int bsize		/* Size of buffer */
) {
	i1pro_code ev = I1PRO_OK;

	if (nummeas <= 0)
		return I1PRO_INT_ZEROMEASURES;
		
	if ((ev = i1pro_trigger_one_measure(p, nummeas, inttime, gainmode, i1p_dark_cal)) != I1PRO_OK)
		return ev;

	if ((ev = i1pro_readmeasurement(p, nummeas, 0, buf, bsize, NULL, i1p_dark_cal)) != I1PRO_OK)
		return ev;

	return ev;
}

/* Take a dark reference measurement - part 2 */
i1pro_code i1pro_dark_measure_2(
	i1pro *p,
	double *absraw,			/* Return array [-1 nraw] of absraw values */
	int nummeas,			/* Number of readings to take */
	double inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	unsigned char *buf,		/* raw USB reading buffer to process */
	unsigned int bsize		/* Buffer size to process */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	double **multimes;		/* Multiple measurement results */
	double sensavg;			/* Overall average of sensor readings */
	double satthresh;		/* Saturation threshold */
	double darkthresh;		/* Dark threshold */
	int rv;

	multimes = dmatrix(0, nummeas-1, -1, m->nraw-1);	/* -1 is RevE shielded cells values */

	if (gainmode == 0)
		satthresh = m->sens_sat0;
	else
		satthresh = m->sens_sat1;

	darkthresh = m->sens_dark + inttime * 900.0;
	if (gainmode)
		darkthresh *= m->highgain;

	/* Take a buffer full of raw readings, and convert them to */
	/* absolute linearised sensor values. */
	if ((ev = i1pro_sens_to_absraw(p, multimes, buf, nummeas, inttime, gainmode, &darkthresh))
		                                                                          != I1PRO_OK) {
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		return ev;
	}

	satthresh = i1pro_raw_to_absraw(p, satthresh, inttime, gainmode);  
	darkthresh = i1pro_raw_to_absraw(p, darkthresh, inttime, gainmode);  

	/* Average a set of measurements into one. */
	/* Return zero if readings are consistent and not saturated. */
	/* Return nz with bit 1 set if the readings are not consistent */
	/* Return nz with bit 2 set if the readings are saturated */
	/* Return the highest individual element. */
	/* Return the overall average. */
	rv = i1pro_average_multimeas(p, absraw, multimes, nummeas, NULL, &sensavg,
	                                                       satthresh, darkthresh);     
	free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);

#ifdef PLOT_DEBUG
	printf("Average absolute sensor readings, average = %f, satthresh %f:\n",sensavg, satthresh);
	plot_raw(absraw);
#endif

	if (rv & 1)
		return I1PRO_RD_DARKREADINCONS;

	if (rv & 2)
		return I1PRO_RD_SENSORSATURATED;

	a1logd(p->log,3,"Dark threshold = %f\n",darkthresh);

	if (sensavg > (2.0 * darkthresh))
		return I1PRO_RD_DARKNOTVALID;

	return ev;
}

#ifdef DUMP_DARKM
int ddumpdarkm = 0;
#endif

/* Take a dark reference measurement (combined parts 1 & 2) */
i1pro_code i1pro_dark_measure(
	i1pro *p,
	double *absraw,			/* Return array [-1 nraw] of absraw values */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode			/* Gain mode to use, 0 = normal, 1 = high */
) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code ev = I1PRO_OK;
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;

	bsize = m->nsen * 2  * nummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"i1pro_dark_measure malloc %d bytes failed (8)\n",bsize);
		return I1PRO_INT_MALLOC;
	}

	if ((ev = i1pro_dark_measure_1(p, nummeas, inttime, gainmode, buf, bsize)) != I1PRO_OK) {
		free(buf);
		return ev;
	}

	if ((ev = i1pro_dark_measure_2(p, absraw,
		                 nummeas, *inttime, gainmode, buf, bsize)) != I1PRO_OK) {
		free(buf);
		return ev;
	}
	free(buf);

#ifdef DUMP_DARKM
	/* Dump raw dark readings to a file "i1pddump.txt" */
	if (ddumpdarkm) {
		int j;
		FILE *fp;
		
		if ((fp = fopen("i1pddump.txt", "a")) == NULL)
			a1logw(p->log,"Unable to open debug file i1pddump.txt\n");
		else {

			fprintf(fp, "\nDark measure: nummeas %d, inttime %f, gainmode %d, darkcells %f\n",nummeas,*inttime,gainmode, absraw[-1]);
			fprintf(fp,"\t\t\t{ ");
			for (j = 0; j < (m->nraw-1); j++)
				fprintf(fp, "%f, ",absraw[j]);
			fprintf(fp, "%f },\n",absraw[j]);
			fclose(fp);
		}
	}
#endif

	return ev;
}


/* Take a white reference measurement */
/* (Subtracts black and processes into wavelenths) */
i1pro_code i1pro_whitemeasure(
	i1pro *p,
	double *abswav0,		/* Return array [nwav[0]] of abswav values (may be NULL) */
	double *abswav1,		/* Return array [nwav[1]] of abswav values (if hr_init, may be NULL) */
	double *absraw,			/* Return array [-1 nraw] of absraw values */
	double *optscale,		/* Factor to scale gain/int time by to make optimal (may be NULL) */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	double targoscale,		/* Optimal reading scale factor */
	int ltocmode			/* 1 = Lamp turn on compensation mode */ 
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	double **multimes;		/* Multiple measurement results */
	double darkthresh;		/* Consitency threshold scale limit */
	int rv;

	a1logd(p->log,3,"i1pro_whitemeasure called \n");

	darkthresh = m->sens_dark + *inttime * 900.0;		/* Default */
	if (gainmode)
		darkthresh *= m->highgain;

	if (nummeas <= 0)
		return I1PRO_INT_ZEROMEASURES;
		
	/* Allocate temporaries up front to avoid delay between trigger and read */
	bsize = m->nsen * 2 * nummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"i1pro_whitemeasure malloc %d bytes failed (10)\n",bsize);
		return I1PRO_INT_MALLOC;
	}
	multimes = dmatrix(0, nummeas-1, -1, m->nraw-1);

	a1logd(p->log,3,"Triggering measurement cycle, nummeas %d, inttime %f, gainmode %d\n",
	                                              nummeas, *inttime, gainmode);

	if ((ev = i1pro_trigger_one_measure(p, nummeas, inttime, gainmode, i1p_cal)) != I1PRO_OK) {
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free(buf);
		return ev;
	}

	a1logd(p->log,4,"Gathering readings\n");

	if ((ev = i1pro_readmeasurement(p, nummeas, 0, buf, bsize, NULL, i1p_cal)) != I1PRO_OK) {
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free(buf);
		return ev;
	}

	/* Take a buffer full of raw readings, and convert them to */
	/* absolute linearised sensor values. */
	if ((ev = i1pro_sens_to_absraw(p, multimes, buf, nummeas, *inttime, gainmode, &darkthresh))
		                                                                          != I1PRO_OK) {
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free(buf);
		return ev;
	}

#ifdef PLOT_DEBUG
	printf("Dark data:\n");
	plot_raw(s->dark_data);
#endif

	/* Subtract the black level */
	i1pro_sub_absraw(p, nummeas, *inttime, gainmode, multimes, s->dark_data);

	/* Convert linearised white value into output wavelength white reference */
	ev = i1pro_whitemeasure_3(p, abswav0, abswav1, absraw, optscale, nummeas,
	                                      *inttime, gainmode, targoscale, multimes, darkthresh);

	free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
	free(buf);

	return ev;
}

/* Process a single raw white reference measurement */
/* (Subtracts black and processes into wavelenths) */
/* Used for restoring calibration from the EEProm */
i1pro_code i1pro_whitemeasure_buf(
	i1pro *p,
	double *abswav0,		/* Return array [nwav[0]] of abswav values (may be NULL) */
	double *abswav1,		/* Return array [nwav[1]] of abswav values (if hr_init, may be NULL) */
	double *absraw,			/* Return array [-1 nraw] of absraw values */
	double inttime, 		/* Integration time to used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	unsigned char *buf		/* Raw buffer */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	double *meas;			/* Multiple measurement results */
	double darkthresh;		/* Consitency threshold scale limit */

	a1logd(p->log,3,"i1pro_whitemeasure_buf called \n");

	meas = dvector(-1, m->nraw-1);
	
	darkthresh = m->sens_dark + inttime * 900.0;	/* Default */
	if (gainmode)
		darkthresh *= m->highgain;

	/* Take a buffer full of raw readings, and convert them to */
	/* absolute linearised sensor values. */
	if ((ev = i1pro_sens_to_absraw(p, &meas, buf, 1, inttime, gainmode, &darkthresh))
		                                                                != I1PRO_OK) {
		return ev;
	}

	/* Subtract the black level */
	i1pro_sub_absraw(p, 1, inttime, gainmode, &meas, s->dark_data);

	/* Convert linearised white value into output wavelength white reference */
	ev = i1pro_whitemeasure_3(p, abswav0, abswav1, absraw, NULL, 1, inttime, gainmode,
	                                                           0.0, &meas, darkthresh);

	free_dvector(meas, -1, m->nraw-1);

	return ev;
}

/* Take a white reference measurement - part 3 */
/* Average, check, and convert to output wavelengths */
i1pro_code i1pro_whitemeasure_3(
	i1pro *p,
	double *abswav0,		/* Return array [nwav[0]] of abswav values (may be NULL) */
	double *abswav1,		/* Return array [nwav[1]] of abswav values (if hr_init, may be NULL) */
	double *absraw,			/* Return array [-1 nraw] of absraw values */
	double *optscale,		/* Factor to scale gain/int time by to make optimal (may be NULL) */
	int nummeas,			/* Number of readings to take */
	double inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	double targoscale,		/* Optimal reading scale factor */
	double **multimes,		/* Multiple measurement results */
	double darkthresh		/* Raw dark threshold */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	double highest;			/* Highest of sensor readings */
	double sensavg;			/* Overall average of sensor readings */
	double satthresh;		/* Saturation threshold */
	double opttarget;		/* Optimal sensor target */
	int rv;

	a1logd(p->log,3,"i1pro_whitemeasure_3 called \n");

	if (gainmode == 0)
		satthresh = m->sens_sat0;
	else
		satthresh = m->sens_sat1;
	satthresh = i1pro_raw_to_absraw(p, satthresh, inttime, gainmode);  

	darkthresh = i1pro_raw_to_absraw(p, darkthresh, inttime, gainmode);  

	/* Average a set of measurements into one. */
	/* Return zero if readings are consistent and not saturated. */
	/* Return nz with bit 1 set if the readings are not consistent */
	/* Return nz with bit 2 set if the readings are saturated */
	/* Return the highest individual element. */
	/* Return the overall average. */
	rv = i1pro_average_multimeas(p, absraw, multimes, nummeas, &highest, &sensavg,
	                                                           satthresh, darkthresh);     
#ifdef PLOT_DEBUG
	printf("Average absolute sensor readings, average = %f, satthresh %f:\n",sensavg, satthresh);
	plot_raw(absraw);
#endif

#ifndef IGNORE_WHITE_INCONS
	if (rv & 1) {
		return I1PRO_RD_WHITEREADINCONS;
	}
#endif /* IGNORE_WHITE_INCONS */

	if (rv & 2) {
		return I1PRO_RD_SENSORSATURATED;
	}

	/* Convert an absraw array from raw wavelengths to output wavelenths */
	if (abswav0 != NULL) {
		i1pro_absraw_to_abswav(p, 0, s->reflective, 1, &abswav0, &absraw);

#ifdef PLOT_DEBUG
		printf("Converted to wavelengths std res:\n");
		plot_wav(m, 0, abswav0);
#endif
	}

#ifdef HIGH_RES
	if (abswav1 != NULL && m->hr_inited) {
		i1pro_absraw_to_abswav(p, 1, s->reflective, 1, &abswav1, &absraw);

#ifdef PLOT_DEBUG
		printf("Converted to wavelengths high res:\n");
		plot_wav(m, 1, abswav1);
#endif
	}
#endif /* HIGH_RES */

	if (optscale != NULL) {
		double lhighest = highest;

		if (lhighest < 1.0)
			lhighest = 1.0;

		/* Compute correction factor to make sensor optimal */
		opttarget = i1pro_raw_to_absraw(p, (double)m->sens_target, inttime, gainmode);  
		opttarget *= targoscale;
	
	
		a1logd(p->log,3,"Optimal target = %f, amount to scale = %f\n",opttarget, opttarget/lhighest);

		*optscale = opttarget/lhighest; 
	}

	return ev;
}

/* Take a wavelength reference measurement */
/* (Measure and subtracts black and convert to absraw) */
i1pro_code i1pro2_wl_measure(
	i1pro *p,
	double *absraw,			/* Return array [-1 nraw] of absraw values */
	double *optscale,		/* Factor to scale gain/int time by to make optimal (may be NULL) */
	double *inttime, 		/* Integration time to use/used */
	double targoscale		/* Optimal reading scale factor */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	int nummeas = 1;		/* Number of measurements to take */
	int gainmode = 0;		/* Gain mode to use, 0 = normal, 1 = high */
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	double *dark;			/* Dark reading */
	double **multimes;		/* Measurement results */
	double darkthresh;		/* Consitency threshold scale limit/reading dark cell values */
	double highest;			/* Highest of sensor readings */
	double sensavg;			/* Overall average of sensor readings */
	double satthresh;		/* Saturation threshold */
	double opttarget;		/* Optimal sensor target */
	int rv;

	a1logd(p->log,3,"i1pro2_wl_measure called \n");

	/* Allocate temporaries up front to avoid delay between trigger and read */
	bsize = m->nsen * 2;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"i1pro2_wl_measure malloc %d bytes failed (10)\n",bsize);
		return I1PRO_INT_MALLOC;
	}

	/* Do a dark reading at our integration time */
	dark = dvector(-1, m->nraw-1);
	multimes = dmatrix(0, nummeas-1, -1, m->nraw-1);

	if ((ev = i1pro_dark_measure(p, dark, nummeas, inttime, gainmode)) != I1PRO_OK) {
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free_dvector(dark, -1, m->nraw-1);
		free(buf);
		return ev;
	}

#ifdef PLOT_DEBUG
	printf("Absraw dark data:\n");
	plot_raw(dark);
#endif

	a1logd(p->log,3,"Triggering wl measurement cycle, inttime %f\n", *inttime);

	if ((ev = i1pro_trigger_one_measure(p, nummeas, inttime, gainmode, i1p2_wl_cal)) != I1PRO_OK) {
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free_dvector(dark, -1, m->nraw-1);
		free(buf);
		return ev;
	}

	a1logd(p->log,4,"Gathering readings\n");

	if ((ev = i1pro_readmeasurement(p, nummeas, 0, buf, bsize, NULL, i1p2_wl_cal)) != I1PRO_OK) {
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free_dvector(dark, -1, m->nraw-1);
		free(buf);
		return ev;
	}

	/* Take a buffer full of raw readings, and convert them to */
	/* absolute linearised sensor values. */
	if ((ev = i1pro_sens_to_absraw(p, multimes, buf, nummeas, *inttime, gainmode, &darkthresh))
		                                                                           != I1PRO_OK) {
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free_dvector(dark, -1, m->nraw-1);
		free(buf);
		return ev;
	}

	/* Convert satthresh and darkthresh/dark_cell values to abs */
	if (gainmode == 0)
		satthresh = m->sens_sat0;
	else
		satthresh = m->sens_sat1;
	satthresh = i1pro_raw_to_absraw(p, satthresh, *inttime, gainmode);  
	darkthresh = i1pro_raw_to_absraw(p, darkthresh, *inttime, gainmode);  

#ifdef PLOT_DEBUG
	printf("Absraw WL data:\n");
	plot_raw(multimes[0]);
#endif

	/* Subtract the black level */
	i1pro_sub_absraw(p, nummeas, *inttime, gainmode, multimes, dark);

#ifdef PLOT_DEBUG
	printf("Absraw WL - black data:\n");
	plot_raw(multimes[0]);
#endif

	/* Average a set of measurements into one. */
	/* Return zero if readings are consistent and not saturated. */
	/* Return nz with bit 1 set if the readings are not consistent */
	/* Return nz with bit 2 set if the readings are saturated */
	/* Return the highest individual element. */
	/* Return the overall average. */
	rv = i1pro_average_multimeas(p, absraw, multimes, 1, &highest, &sensavg,
	                                                           satthresh, darkthresh);     
#ifdef PLOT_DEBUG
	printf("Average absolute sensor readings, average = %f, satthresh %f, absraw WL result:\n",sensavg, satthresh);
	plot_raw(absraw);
#endif

#ifndef IGNORE_WHITE_INCONS
	if (rv & 1) {
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free_dvector(dark, -1, m->nraw-1);
		free(buf);
		return I1PRO_RD_WHITEREADINCONS;
	}
#endif /* IGNORE_WHITE_INCONS */

	if (rv & 2) {
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free_dvector(dark, -1, m->nraw-1);
		free(buf);
		return I1PRO_RD_SENSORSATURATED;
	}

	if (optscale != NULL) {
		double lhighest = highest;

		if (lhighest < 1.0)
			lhighest = 1.0;

		/* Compute correction factor to make sensor optimal */
		opttarget = i1pro_raw_to_absraw(p, (double)m->sens_target, *inttime, gainmode);  
		opttarget *= targoscale;
	
	
		a1logd(p->log,3,"Optimal target = %f, amount to scale = %f\n",opttarget, opttarget/lhighest);

		*optscale = opttarget/lhighest; 
	}

	free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
	free_dvector(dark, -1, m->nraw-1);
	free(buf);

	return ev;
}

/* Take a measurement reading using the current mode, part 1 */
/* Converts to completely processed output readings. */
/* (NOTE:- this can't be used for calibration, as it implements uv mode) */
i1pro_code i1pro_read_patches_1(
	i1pro *p,
	int minnummeas,			/* Minimum number of measurements to take */
	int maxnummeas,			/* Maximum number of measurements to allow for */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	int *nmeasuered,		/* Number actually measured */
	unsigned char *buf,		/* Raw USB reading buffer */
	unsigned int bsize
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	i1p_mmodif mmod = i1p_norm;
	int rv = 0;

	if (minnummeas <= 0)
		return I1PRO_INT_ZEROMEASURES;
	if (minnummeas > maxnummeas)
		maxnummeas = minnummeas;
		
	if (m->uv_en)
		mmod = i1p2_UV;

	a1logd(p->log,3,"Triggering & gathering cycle, minnummeas %d, inttime %f, gainmode %d\n",
	                                              minnummeas, *inttime, gainmode);

	if ((ev = i1pro_trigger_one_measure(p, minnummeas, inttime, gainmode, mmod)) != I1PRO_OK) {
		return ev;
	}

	if ((ev = i1pro_readmeasurement(p, minnummeas, m->c_measmodeflags & I1PRO_MMF_SCAN,
	                                             buf, bsize, nmeasuered, mmod)) != I1PRO_OK) {
		return ev;
	}

	return ev;
}

/* Take a measurement reading using the current mode, part 2 */
/* Converts to completely processed output readings. */
i1pro_code i1pro_read_patches_2(
	i1pro *p,
	double *duration,		/* Return flash duration */
	double **specrd,		/* Return array [numpatches][nwav] of spectral reading values */
	int numpatches,			/* Number of patches to return */
	double inttime, 		/* Integration time to used */
	int gainmode,			/* Gain mode useed, 0 = normal, 1 = high */
	int nmeasuered,			/* Number actually measured */
	unsigned char *buf,		/* Raw USB reading buffer */
	unsigned int bsize
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	double **multimes;		/* Multiple measurement results [maxnummeas|nmeasuered][-1 nraw]*/
	double **absraw;		/* Linearsised absolute sensor raw values [numpatches][-1 nraw]*/
	double satthresh;		/* Saturation threshold */
	double darkthresh;		/* Dark threshold for consistency scaling limit */
	int rv = 0;

	if (duration != NULL)
		*duration = 0.0;	/* default value */

	darkthresh = m->sens_dark + inttime * 900.0;		/* Default */
	if (gainmode)
		darkthresh *= m->highgain;

	/* Allocate temporaries */
	multimes = dmatrix(0, nmeasuered-1, -1, m->nraw-1);
	absraw = dmatrix(0, numpatches-1, -1, m->nraw-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* absolute linearised sensor values. */
	if ((ev = i1pro_sens_to_absraw(p, multimes, buf, nmeasuered, inttime, gainmode, &darkthresh))
		                                                                             != I1PRO_OK) {
		free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
		free_dmatrix(multimes, 0, nmeasuered-1, -1, m->nraw-1);
		return ev;
	}

	/* Subtract the black level */
	i1pro_sub_absraw(p, nmeasuered, inttime, gainmode, multimes, s->dark_data);

#ifdef DUMP_SCANV
	/* Dump raw scan readings to a file "i1pdump.txt" */
	{
		int i, j;
		FILE *fp;
		
		if ((fp = fopen("i1pdump.txt", "w")) == NULL)
			a1logw(p->log,"Unable to open debug file i1pdump.txt\n");
		else {
			for (i = 0; i < nmeasuered; i++) {
				fprintf(fp, "%d	",i);
				for (j = 0; j < m->nraw; j++) {
					fprintf(fp, "%f	",multimes[i][j]);
				}
				fprintf(fp,"\n");
			}
			fclose(fp);
		}
	}
#endif
	if (gainmode == 0)
		satthresh = m->sens_sat0;
	else
		satthresh = m->sens_sat1;
	satthresh = i1pro_raw_to_absraw(p, satthresh, inttime, gainmode);  

	darkthresh = i1pro_raw_to_absraw(p, darkthresh, inttime, gainmode);  

	if (!s->scan) {
		if (numpatches != 1) {
			free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
			free_dmatrix(multimes, 0, nmeasuered-1, -1, m->nraw-1);
			a1logd(p->log,2,"i1pro_read_patches_2 spot read failed because numpatches != 1\n");
			return I1PRO_INT_WRONGPATCHES;
		}

		/* Average a set of measurements into one. */
		/* Return zero if readings are consistent and not saturated. */
		/* Return nz with bit 1 set if the readings are not consistent */
		/* Return nz with bit 2 set if the readings are saturated */
		/* Return the highest individual element. */
		/* Return the overall average. */
		rv = i1pro_average_multimeas(p, absraw[0], multimes, nmeasuered, NULL, NULL,
	                                                            satthresh, darkthresh);     
	} else {
		if (s->flash) {

			if (numpatches != 1) {
				free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
				free_dmatrix(multimes, 0, nmeasuered-1, -1, m->nraw-1);
				a1logd(p->log,2,"i1pro_read_patches_2 spot read failed because numpatches != 1\n");
				return I1PRO_INT_WRONGPATCHES;
			}
			if ((ev = i1pro_extract_patches_flash(p, &rv, duration, absraw[0], multimes,
			                                                 nmeasuered, inttime)) != I1PRO_OK) {
				free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
				free_dmatrix(multimes, 0, nmeasuered-1, -1, m->nraw-1);
				a1logd(p->log,2,"i1pro_read_patches_2 spot read failed at i1pro_extract_patches_flash\n");
				return ev;
			}

		} else {
			a1logd(p->log,3,"Number of patches measured = %d\n",nmeasuered);

			/* Recognise the required number of ref/trans patch locations, */
			/* and average the measurements within each patch. */
			if ((ev = i1pro_extract_patches_multimeas(p, &rv, absraw, numpatches, multimes,
			                            nmeasuered, NULL, satthresh, inttime)) != I1PRO_OK) {
				free_dmatrix(multimes, 0, nmeasuered-1, -1, m->nraw-1);
				free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
				a1logd(p->log,2,"i1pro_read_patches_2 spot read failed at i1pro_extract_patches_multimeas\n");
				return ev;
			}
		}
	}
	free_dmatrix(multimes, 0, nmeasuered-1, -1, m->nraw-1);

	if (rv & 1) {
		free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
		a1logd(p->log,3,"i1pro_read_patches_2 spot read failed with inconsistent readings\n");
		return I1PRO_RD_READINCONS;
	}

	if (rv & 2) {
		free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
		a1logd(p->log,3,"i1pro_read_patches_2 spot read failed with sensor saturated\n");
		return I1PRO_RD_SENSORSATURATED;
	}

	/* Convert an absraw array from raw wavelengths to output wavelenths */
	i1pro_absraw_to_abswav(p, m->highres, s->reflective, numpatches, specrd, absraw);
	free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);

#ifdef APPEND_MEAN_EMMIS_VAL
	/* Append averaged emission reading to file "i1pdump.txt" */
	{
		int i, j;
		FILE *fp;
		
		/* Create wavelegth label */
		if ((fp = fopen("i1pdump.txt", "r")) == NULL) {
			if ((fp = fopen("i1pdump.txt", "w")) == NULL)
				a1logw(p->log,"Unable to reate debug file i1pdump.txt\n");
			else {
				for (j = 0; j < m->nwav[m->highres]; j++)
					fprintf(fp,"%f ",XSPECT_WL(m->wl_short[m->highres], m->wl_long[m->highres], m->nwav[m->highres], j));
				fprintf(fp,"\n");
				fclose(fp);
			}
		}
		if ((fp = fopen("i1pdump.txt", "a")) == NULL) {
			a1logw(p->log,"Unable to open debug file i1pdump.txt\n");
		else {
			for (j = 0; j < m->nwav[m->highres]; j++)
				fprintf(fp, "%f	",specrd[0][j] * m->emis_coef[j]);
			fprintf(fp,"\n");
			fclose(fp);
		}
	}
#endif

#ifdef PLOT_DEBUG
	printf("Converted to wavelengths:\n");
	plot_wav(m, m->highres, specrd[0]);
#endif

	/* Scale to the calibrated output values */
	i1pro_scale_specrd(p, specrd, numpatches, specrd);

#ifdef PLOT_DEBUG
	printf("Calibrated measuerment spectra:\n");
	plot_wav(m, m->highres, specrd[0]);
#endif

	return ev;
}

/* Take a measurement reading using the current mode, part 2a */
/* Converts to completely processed output readings, */
/* but don't average together or extract patches or flash. */
/* (! Note that we aren't currently detecting saturation here!) */
i1pro_code i1pro_read_patches_2a(
	i1pro *p,
	double **specrd,		/* Return array [numpatches][nwav] of spectral reading values */
	int numpatches,			/* Number of patches measured and to return */
	double inttime, 		/* Integration time to used */
	int gainmode,			/* Gain mode useed, 0 = normal, 1 = high */
	unsigned char *buf,		/* Raw USB reading buffer */
	unsigned int bsize
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	double **absraw;		/* Linearsised absolute sensor raw values [numpatches][-1 nraw]*/
	double satthresh;		/* Saturation threshold */
	double darkthresh;		/* Dark threshold for consistency scaling limit */

	darkthresh = m->sens_dark + inttime * 900.0;		/* Default */
	if (gainmode)
		darkthresh *= m->highgain;

	/* Allocate temporaries */
	absraw = dmatrix(0, numpatches-1, -1, m->nraw-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* absolute linearised sensor values. */
	if ((ev = i1pro_sens_to_absraw(p, absraw, buf, numpatches, inttime, gainmode, &darkthresh))
		                                                                           != I1PRO_OK) {
		free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
		return ev;
	}

	/* Subtract the black level */
	i1pro_sub_absraw(p, numpatches, inttime, gainmode, absraw, s->dark_data);

	if (gainmode == 0)
		satthresh = m->sens_sat0;
	else
		satthresh = m->sens_sat1;
	satthresh = i1pro_raw_to_absraw(p, satthresh, inttime, gainmode);  

	darkthresh = i1pro_raw_to_absraw(p, darkthresh, inttime, gainmode);  

	a1logd(p->log,3,"Number of patches measured = %d\n",numpatches);

	/* Convert an absraw array from raw wavelengths to output wavelenths */
	i1pro_absraw_to_abswav(p, m->highres, s->reflective, numpatches, specrd, absraw);
	free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);

#ifdef PLOT_DEBUG
	printf("Converted to wavelengths:\n");
	plot_wav(m, m->highres, specrd[0]);
#endif

	/* Scale to the calibrated output values */
	i1pro_scale_specrd(p, specrd, numpatches, specrd);

#ifdef PLOT_DEBUG
	printf("Calibrated measuerment spectra:\n");
	plot_wav(m, m->highres, specrd[0]);
#endif

	return ev;
}

/* Take a measurement reading using the current mode (combined parts 1 & 2) */
/* Converts to completely processed output readings. */
/* (NOTE:- this can't be used for calibration, as it implements uv mode) */
i1pro_code i1pro_read_patches(
	i1pro *p,
	double *duration,		/* Return flash duration */
	double **specrd,		/* Return array [numpatches][nwav] of spectral reading values */
	int numpatches,			/* Number of patches to return */
	int minnummeas,			/* Minimum number of measurements to take */
	int maxnummeas,			/* Maximum number of measurements to allow for */
	double *inttime, 		/* Integration time to use/used */
	int gainmode			/* Gain mode to use, 0 = normal, 1 = high */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	int nmeasuered;			/* Number actually measured */
	int rv = 0;

	if (minnummeas <= 0)
		return I1PRO_INT_ZEROMEASURES;
	if (minnummeas > maxnummeas)
		maxnummeas = minnummeas;
		
	/* Allocate temporaries */
	bsize = m->nsen * 2 * maxnummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"i1pro_read_patches malloc %d bytes failed (11)\n",bsize);
		return I1PRO_INT_MALLOC;
	}

	/* Trigger measure and gather raw readings */
	if ((ev = i1pro_read_patches_1(p, minnummeas, maxnummeas, inttime, gainmode,
	                                       &nmeasuered, buf, bsize)) != I1PRO_OK) { 
		free(buf);
		return ev;
	}

	/* Process the raw readings */
	if ((ev = i1pro_read_patches_2(p, duration, specrd, numpatches, *inttime, gainmode,
	                                              nmeasuered, buf, bsize)) != I1PRO_OK) {
		free(buf);
		return ev;
	}
	free(buf);
	return ev;
}

/* Take a measurement reading using the current mode (combined parts 1 & 2a) */
/* Converts to completely processed output readings, without averaging or extracting */
/* sample patches. */
/* (NOTE:- this can't be used for calibration, as it implements uv mode) */
i1pro_code i1pro_read_patches_all(
	i1pro *p,
	double **specrd,		/* Return array [numpatches][nwav] of spectral reading values */
	int numpatches,			/* Number of sample to measure */
	double *inttime, 		/* Integration time to use/used */
	int gainmode			/* Gain mode to use, 0 = normal, 1 = high */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	int rv = 0;

	bsize = m->nsen * 2 * numpatches;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"i1pro_read_patches malloc %d bytes failed (11)\n",bsize);
		return I1PRO_INT_MALLOC;
	}

	/* Trigger measure and gather raw readings */
	if ((ev = i1pro_read_patches_1(p, numpatches, numpatches, inttime, gainmode,
	                                       NULL, buf, bsize)) != I1PRO_OK) { 
		free(buf);
		return ev;
	}

	/* Process the raw readings without averaging or extraction */
	if ((ev = i1pro_read_patches_2a(p, specrd, numpatches, *inttime, gainmode,
	                                                 buf, bsize)) != I1PRO_OK) {
		free(buf);
		return ev;
	}
	free(buf);
	return ev;
}

/* Take a trial measurement reading using the current mode. */
/* Used to determine if sensor is saturated, or not optimal */
/* in adaptive emission mode. */
i1pro_code i1pro_trialmeasure(
	i1pro *p,
	int *saturated,			/* Return nz if sensor is saturated */
	double *optscale,		/* Factor to scale gain/int time by to make optimal (may be NULL) */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	double targoscale		/* Optimal reading scale factor */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	double **multimes;		/* Multiple measurement results */
	double *absraw;		/* Linearsised absolute sensor raw values */
	int nmeasuered;			/* Number actually measured */
	double highest;			/* Highest of sensor readings */
	double sensavg;			/* Overall average of sensor readings */
	double satthresh;		/* Saturation threshold */
	double darkthresh;		/* Dark threshold */
	double opttarget;		/* Optimal sensor target */
	int rv;

	if (nummeas <= 0)
		return I1PRO_INT_ZEROMEASURES;
		
	darkthresh = m->sens_dark + *inttime * 900.0;
	if (gainmode)
		darkthresh *= m->highgain;

	/* Allocate up front to avoid delay between trigger and read */
	bsize = m->nsen * 2 * nummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"i1pro_trialmeasure malloc %d bytes failed (12)\n",bsize);
		return I1PRO_INT_MALLOC;
	}
	multimes = dmatrix(0, nummeas-1, -1, m->nraw-1);
	absraw = dvector(-1, m->nraw-1);

	a1logd(p->log,3,"Triggering measurement cycle, nummeas %d, inttime %f, gainmode %d\n",
	                                              nummeas, *inttime, gainmode);

	if ((ev = i1pro_trigger_one_measure(p, nummeas, inttime, gainmode, i1p_cal)) != I1PRO_OK) {
		free_dvector(absraw, -1, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free(buf);
		return ev;
	}

	a1logd(p->log,4,"Gathering readings\n");
	if ((ev = i1pro_readmeasurement(p, nummeas, m->c_measmodeflags & I1PRO_MMF_SCAN,
	                                             buf, bsize, &nmeasuered, i1p_cal)) != I1PRO_OK) {
		free_dvector(absraw, -1, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free(buf);
		return ev;
	}

	/* Take a buffer full of raw readings, and convert them to */
	/* absolute linearised sensor values. */
	if ((ev = i1pro_sens_to_absraw(p, multimes, buf, nmeasuered, *inttime, gainmode, &darkthresh))
		                                                                            != I1PRO_OK) {
		free_dvector(absraw, -1, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free(buf);
		return ev;
	}

	/* Compute dark subtraction for this trial's parameters */
	if ((ev = i1pro_interp_dark(p, s->dark_data,
		                                  s->inttime, s->gainmode)) != I1PRO_OK) {
		free_dvector(absraw, -1, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free(buf);
		a1logd(p->log,2,"i1pro_trialmeasure interplate dark ref failed\n");
		return ev;
	}

	/* Subtract the black level */
	i1pro_sub_absraw(p, nummeas, *inttime, gainmode, multimes, s->dark_data);

	if (gainmode == 0)
		satthresh = m->sens_sat0;
	else
		satthresh = m->sens_sat1;
	satthresh = i1pro_raw_to_absraw(p, satthresh, *inttime, gainmode);  

	darkthresh = i1pro_raw_to_absraw(p, darkthresh, *inttime, gainmode);  

	/* Average a set of measurements into one. */
	/* Return zero if readings are consistent and not saturated. */
	/* Return nz with bit 1 set if the readings are not consistent */
	/* Return nz with bit 2 set if the readings are saturated */
	/* Return the highest individual element. */
	/* Return the overall average. */
	rv = i1pro_average_multimeas(p, absraw, multimes, nmeasuered, &highest, &sensavg,
	                                                            satthresh, darkthresh);     
#ifdef PLOT_DEBUG
	printf("Average absolute sensor readings, average = %f, satthresh %f:\n",sensavg, satthresh);
	plot_raw(absraw);
#endif

	if (saturated != NULL) {
		*saturated = 0;
		if (rv & 2)
			*saturated = 1;
	}

	/* Compute correction factor to make sensor optimal */
	opttarget = (double)m->sens_target * targoscale;
	opttarget = i1pro_raw_to_absraw(p, opttarget, *inttime, gainmode);  

	if (optscale != NULL) {
		double lhighest = highest;

		if (lhighest < 1.0)
			lhighest = 1.0;

		*optscale = opttarget/lhighest; 
	}

	free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
	free_dvector(absraw, -1, m->nraw-1);
	free(buf);

	return ev;
}

/* Trigger a single measurement cycle. This could be a dark calibration, */
/* a calibration, or a real measurement. This is used to create the */
/* higher level "calibrate" and "take reading" functions. */
/* The setup for the operation is in the current mode state. */
/* Call i1pro_readmeasurement() to collect the results */
i1pro_code
i1pro_trigger_one_measure(
	i1pro *p,
	int nummeas,			/* Minimum number of measurements to make */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	i1p_mmodif mmodif		/* Measurement modifier enum */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	unsigned int timssinceoff;	/* time in msec since lamp turned off */
	double dintclocks;
	int intclocks;		/* Number of integration clocks */
	double dlampclocks;
	int lampclocks;		/* Number of lamp turn on sub-clocks */
	int measmodeflags;	/* Measurement mode command flags */
	int measmodeflags2;	/* Rev E Measurement mode command flags */

	/* The Rev E measure mode has it's own settings */
	if (p->itype == instI1Pro2) {
		m->intclkp = m->intclkp2;		/* From i1pro2_getmeaschar() ? */
		m->subclkdiv = m->subclkdiv2;
		m->subtmode = 0;

	} else {
		/* Set any special hardware up for this sort of read */
		if (*inttime != m->c_inttime) {		/* integration time is different */
			int mcmode, maxmcmode;
			int intclkusec;
			int subtmodeflags;

			/* Setting for fwrev < 301 */
			/* (This is what getmcmode() returns for mcmode = 1 on fwrev >= 301) */
			m->intclkp = 68.0e-6;
			m->subclkdiv = 130;
			m->subtmode = 0;

			if (m->fwrev >= 301) {	/* Special hardware in latter versions of instrument */

#ifdef DEBUG
				/* Show all the available clock modes */
				for (mcmode = 1;; mcmode++) {
					int rmcmode, subclkdiv;

					if ((ev = i1pro_setmcmode(p, mcmode)) != I1PRO_OK)
						break;
	
					if ((ev = i1pro_getmcmode(p, &maxmcmode, &rmcmode, &subclkdiv,
					                             &intclkusec, &subtmodeflags) ) != I1PRO_OK)
						break;
	
					fprintf(stderr,"getcmode %d: maxcmode %d, mcmode %d, subclkdif %d, intclkusec %d, subtmodeflags 0x%x\n",mcmode,maxmcmode,rmcmode,subclkdiv,intclkusec,subtmodeflags);
					if (mcmode >= maxmcmode)
						break;
				}
#endif
				/* Configure a clock mode that gives us an optimal integration time ? */
				for (mcmode = 1;; mcmode++) {
					if ((ev = i1pro_setmcmode(p, mcmode)) != I1PRO_OK)
						return ev;
	
					if ((ev = i1pro_getmcmode(p, &maxmcmode, &mcmode, &m->subclkdiv,
					                             &intclkusec, &subtmodeflags) ) != I1PRO_OK)
						return ev;
	
					if ((*inttime/(intclkusec * 1e-6)) > 65535.0) {
						return I1PRO_INT_INTTOOBIG;
					}
	
					if (*inttime >= (intclkusec * m->subclkdiv * 1e-6 * 0.99))
						break;			/* Setting is optimal */
	
					/* We need to go around again */
					if (mcmode >= maxmcmode) {
						return I1PRO_INT_INTTOOSMALL;
					}
				}
				m->c_mcmode = mcmode;
				m->intclkp = intclkusec * 1e-6;
				a1logd(p->log,3,"Switched to perfect mode, subtmode flag = 0x%x, intclk = %f Mhz\n",subtmodeflags & 0x01, 1.0/intclkusec);
				if (subtmodeflags & 0x01)
					m->subtmode = 1;	/* Last reading subtract mode */
			}
		}
	}
	a1logd(p->log,3,"Integration clock period = %f ussec\n",m->intclkp * 1e6);

	/* Compute integration clocks */
	dintclocks = floor(*inttime/m->intclkp + 0.5);
	if (p->itype == instI1Pro2) {
		if (dintclocks > 4294967296.0)		/* This is probably not the actual limit */
			return I1PRO_INT_INTTOOBIG;
	} else {
		if (dintclocks > 65535.0)
			return I1PRO_INT_INTTOOBIG;
	}
	intclocks = (int)dintclocks;
	*inttime = (double)intclocks * m->intclkp;		/* Quantized integration time */

	if (s->reflective && (mmodif & 0x10)) {
		dlampclocks = floor(s->lamptime/(m->subclkdiv * m->intclkp) + 0.5);
		if (dlampclocks > 256.0)		/* Clip - not sure why. Silly value anyway */
			dlampclocks = 256.0;
		lampclocks = (int)dlampclocks;
		s->lamptime = dlampclocks * m->subclkdiv * m->intclkp;	/* Quantized lamp time */
	} else {
		dlampclocks = 0.0;
		lampclocks = 0;
	}

	if (nummeas > 65535)
		nummeas = 65535;		/* Or should we error ? */

	/* Create measurement mode flag values for this operation for both */
	/* legacy and Rev E mode. Other code will examine legacy mode flags */
	measmodeflags = 0;
	if (s->scan && !(mmodif & 0x20)) 			/* Never scan on a calibration */
		measmodeflags |= I1PRO_MMF_SCAN;
	if (!s->reflective || !(mmodif & 0x10))
		measmodeflags |= I1PRO_MMF_NOLAMP;		/* No lamp if not reflective or dark measure */
	if (gainmode == 0)
		measmodeflags |= I1PRO_MMF_LOWGAIN;		/* Normal gain mode */

	if (p->itype == instI1Pro2) {
		measmodeflags2 = 0;
		if (s->scan && !(mmodif & 0x20))			/* Never scan on a calibration */
			measmodeflags2 |= I1PRO2_MMF_SCAN;

		if (mmodif == i1p2_UV)
			measmodeflags2 |= I1PRO2_MMF_UV_LED;	/* UV LED illumination measurement */
		else if (mmodif == i1p2_wl_cal)
			measmodeflags2 |= I1PRO2_MMF_WL_LED;	/* Wavelegth illumination cal */
		else if (s->reflective && (mmodif & 0x10))
			measmodeflags2 |= I1PRO2_MMF_LAMP;		/* lamp if reflective and mmodif possible */

		if (gainmode != 0)
			return I1PRO_INT_NO_HIGH_GAIN;
	}

	a1logd(p->log,2,"i1pro: Int time %f msec, delay %f msec, no readings %d, expect %f msec\n",
		*inttime * 1000.0,
		((measmodeflags & I1PRO_MMF_NOLAMP) ? 0.0 : s->lamptime) * 1000.0,
		nummeas,
		(nummeas * *inttime + ((measmodeflags & I1PRO_MMF_NOLAMP) ? 0.0 : s->lamptime)) * 1000.0);
	
	/* Do a setmeasparams */
#ifdef NEVER
	if (intclocks != m->c_intclocks				/* If any parameters have changed */
	 || lampclocks != m->c_lampclocks
	 || nummeas != m->c_nummeas
	 || measmodeflags != m->c_measmodeflags
	 || measmodeflags2 != m->c_measmodeflags2)
#endif /* NEVER */
	{

		if (p->itype != instI1Pro2) {			/* Rev E sets the params in the measure command */
			/* Set the hardware for measurement */
			if ((ev = i1pro_setmeasparams(p, intclocks, lampclocks, nummeas, measmodeflags)) != I1PRO_OK)
				return ev;
		} else {
			a1logd(p->log,2,"\ni1pro: SetMeasureParam2 %d, %d, %d, 0x%04x @ %d msec\n",
	                   intclocks, lampclocks, nummeas, measmodeflags2,
	                   msec_time() - m->msec);
		}
	
		m->c_intclocks = intclocks;
		m->c_lampclocks = lampclocks;
		m->c_nummeas = nummeas;
		m->c_measmodeflags = measmodeflags;
		m->c_measmodeflags2 = measmodeflags2;

		m->c_inttime = *inttime;		/* Special harware is configured */
		m->c_lamptime = s->lamptime;
	}

	/* If the lamp needs to be off, make sure at least 1.5 seconds */
	/* have elapsed since it was last on, to make sure it's dark. */
	if ((measmodeflags & I1PRO_MMF_NOLAMP)
	 && (timssinceoff = (msec_time() - m->llamponoff)) < LAMP_OFF_TIME) {
		a1logd(p->log,3,"Sleep %d msec for lamp cooldown\n",LAMP_OFF_TIME - timssinceoff);
		msec_sleep(LAMP_OFF_TIME - timssinceoff);	/* Make sure time adds up to 1.5 seconds */
	}

	/* Trigger a measurement */
	usb_reinit_cancel(&m->rd_sync);			/* Prepare to sync rd and trigger */
	if (p->itype != instI1Pro2) {
		if ((ev = i1pro_triggermeasure(p, TRIG_DELAY)) != I1PRO_OK)
			return ev;
	} else {
		if ((ev = i1pro2_triggermeasure(p, TRIG_DELAY)) != I1PRO_OK)
			return ev;
	}

	return ev;
}

/* ============================================================ */
/* Big endian wire format conversion routines */

/* Take an int, and convert it into a byte buffer big endian */
static void int2buf(unsigned char *buf, int inv) {
	buf[0] = (inv >> 24) & 0xff;
	buf[1] = (inv >> 16) & 0xff;
	buf[2] = (inv >> 8) & 0xff;
	buf[3] = (inv >> 0) & 0xff;
}

/* Take a short, and convert it into a byte buffer big endian */
static void short2buf(unsigned char *buf, int inv) {
	buf[0] = (inv >> 8) & 0xff;
	buf[1] = (inv >> 0) & 0xff;
}

/* Take a word sized buffer, and convert it to an int */
static int buf2int(unsigned char *buf) {
	int val;
	val = buf[0];	/* Hmm. should this be sign extended ?? */
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[3]));
	return val;
}

/* Take a short sized buffer, and convert it to an int */
static int buf2short(unsigned char *buf) {
	int val;
	val = *((signed char *)buf);		/* Sign extend */
	val = ((val << 8) + (0xff & buf[1]));
	return val;
}

/* Take a unsigned short sized buffer, and convert it to an int */
static int buf2ushort(unsigned char *buf) {
	int val;
	val = (0xff & buf[0]);
	val = ((val << 8) + (0xff & buf[1]));
	return val;
}

/* ============================================================ */
/* lower level reading processing and computation */

/* Take a buffer full of sensor readings, and convert them to */
/* absolute raw values. Linearise if Rev A..D */
/* If RevE, fill in the [-1] value with the shielded cell values */
/* Note the rev E darkthresh returned has NOT been converted to an absolute raw value */
i1pro_code i1pro_sens_to_absraw(
	i1pro *p,
	double **absraw,		/* Array of [nummeas][-1 nraw] value to return */
	unsigned char *buf,		/* Raw measurement data must be nsen * nummeas */
	int nummeas,			/* Return number of readings measured */
	double inttime, 		/* Integration time used */
	int gainmode,			/* Gain mode, 0 = normal, 1 = high */
	double *pdarkthresh     /* Return a raw dark threshold value (Rev E) */
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k;
	unsigned char *bp;
	unsigned int maxpve = m->maxpve;	/* maximum +ve sensor value + 1 */ 
	double avlastv = 0.0;
	double darkthresh = 0.0;			/* Rev E calculated values */
	double ndarkthresh = 0.0;
	double gain;
	int npoly;			/* Number of linearisation coefficients */
	double *polys;		/* the coeficients */
	double scale;		/* Absolute scale value */
	int sskip = 0;		/* Bytes to skip at start */
	int eskip = 0;		/* Bytes to skip at end */

	if (gainmode) {
		gain = m->highgain;
		npoly = m->nlin1;
		polys = m->lin1;
	} else {
		gain = 1.0;
		npoly = m->nlin0;
		polys = m->lin0;
	}
	scale = 1.0/(inttime * gain);

	/* Now process the buffer values */
	if (m->nsen > m->nraw) {	/* It's a Rev E, so we have extra values, */
								/* and don't linearize here. */
		sskip = 6 * 2;		/* 6 dark reading values */
		eskip = 0 * 2;		/* none to skip at end */

		if ((sskip + m->nraw * 2 + eskip) != (m->nsen * 2)) {
			a1loge(p->log,1,"i1pro Rev E - sskip %d + nraw %d + eskip %d != nsen %d\n"
			                                 ,sskip, m->nraw * 2, eskip, m->nsen * 2);
			return I1PRO_INT_ASSERT;
		}

		for (bp = buf, i = 0; i < nummeas; i++, bp += eskip) {
			unsigned int rval;
			double fval;
			
			/* The first 6 readings (xraw from i1pro2_getmeaschar()) are shielded cells, */
			/* and we use them as an estimate of the dark reading consistency, as well as for */
			/* compensating the dark level calibration for any temperature changes. */

			/* raw average of all measurement shielded cell values */
			for (k = 0; k < 6; k++) {
				darkthresh += (double)buf2ushort(bp + k * 2);
				ndarkthresh++;
			}

			/* absraw of shielded cells per reading */
			absraw[i][-1] = 0.0;
			for (k = 0; k < 6; k++) {
				rval = buf2ushort(bp + k * 2);
				fval = (double)(int)rval;

				/* And scale to be an absolute sensor reading */
				absraw[i][-1] += fval * scale;
			}
			absraw[i][-1] /= 6.0;
		
			for (bp += sskip, j = 0; j < m->nraw; j++, bp += 2) {
				rval = buf2ushort(bp);
				a1logd(p->log,9,"% 3d:rval 0x%x, ",j, rval);
				a1logd(p->log,9,"srval 0x%x, ",rval);
				fval = (double)(int)rval;
				a1logd(p->log,9,"fval %.0f, ",fval);

				/* And scale to be an absolute sensor reading */
				absraw[i][j] = fval * scale;
				a1logd(p->log,9,"absval %.1f\n",fval * scale);
			}
		}
		darkthresh /= ndarkthresh;
		if (pdarkthresh != NULL)
			*pdarkthresh = darkthresh;
		a1logd(p->log,3,"i1pro_sens_to_absraw: Dark threshold = %f\n",darkthresh);

	} else {
		/* if subtmode is set, compute the average last reading raw value. */
		/* Could this be some sort of temperature compensation offset ??? */
		/* (Rev A produces a value that is quite different to a sensor value, */
		/* ie. 1285 = 0x0505, while RevD and RevE in legacy mode have a value of 0 */
		/* I've not seen anything actually use subtmode - maybe this is Rev B only ?) */
		/* The 0 band seens to contain values similar to band 1, so it's not clear */
		/* why the manufacturers driver appears to be discarding it ? */

		/* (Not sure if it's reasonable to extend the sign and then do this */
		/* computation, or whether it makes any difference, since I've never */
		/* seen this mode triggered. */
		if (m->subtmode) {
			for (bp = buf + 254, i = 0; i < nummeas; i++, bp += (m->nsen * 2)) {
				unsigned int lastv;
				lastv = buf2ushort(bp);
				if (lastv >= maxpve) {
					lastv -= 0x00010000;	/* Convert to -ve */
				}
				avlastv += (double)lastv;
			}
			avlastv /= (double)nummeas;
			a1logd(p->log,3,"subtmode got avlastv = %f\n",avlastv);
		}

		for (bp = buf, i = 0; i < nummeas; i++) {
			absraw[i][-1] = 1.0;		/* Not used in RevA-D */

			for (j = 0; j < 128; j++, bp += 2)  {
				unsigned int rval;
				double fval, lval;

				rval = buf2ushort(bp);
				a1logd(p->log,9,"% 3d:rval 0x%x, ",j, rval);
				if (rval >= maxpve)
					rval -= 0x00010000;	/* Convert to -ve */
				a1logd(p->log,9,"srval 0x%x, ",rval);
				fval = (double)(int)rval;
				a1logd(p->log,9,"fval %.0f, ",fval);
				fval -= avlastv;
				a1logd(p->log,9,"fval-av %.0f, ",fval);

#ifdef ENABLE_NONLINCOR	
				/* Linearise */
				for (lval = polys[npoly-1], k = npoly-2; k >= 0; k--)
					lval = lval * fval + polys[k];
#else
				lval = fval;
#endif
				a1logd(p->log,9,"lval %.1f, ",lval);

				/* And scale to be an absolute sensor reading */
				absraw[i][j] = lval * scale;
				a1logd(p->log,9,"absval %.1f\n",lval * scale);
				// a1logd(p->log,3,"Meas %d band %d raw = %f\n",i,j,fval);
			}

			/* Duplicate last values in buffer to make up to 128 */
			absraw[i][0] = absraw[i][1];
			absraw[i][127] = absraw[i][126];
		}
	}

	return I1PRO_OK;
}

/* Take a raw value, and convert it into an absolute raw value. */
/* Note that linearisation is ignored, since it is assumed to be insignificant */
/* to the black threshold and saturation values. */
double i1pro_raw_to_absraw(
	i1pro *p,
	double raw,				/* Input value */
	double inttime, 		/* Integration time used */
	int gainmode			/* Gain mode, 0 = normal, 1 = high */
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k;
	double gain;
	double scale;		/* Absolute scale value */
	double fval;

	if (gainmode) {
		gain = m->highgain;
	} else {
		gain = 1.0;
	}
	scale = 1.0/(inttime * gain);

	return raw * scale;
}


/* Invert a polinomial equation. */
/* Since the linearisation is nearly a straight line, */
/* a simple Newton inversion will suffice. */
static double inv_poly(double *polys, int npoly, double inv) {
	double outv = inv, lval, del = 100.0;
	int i, k;

	for (i = 0; i < 200 && fabs(del) > 1e-7; i++) {
		for (lval = polys[npoly-1], k = npoly-2; k >= 0; k--) {
			lval = lval * outv + polys[k];
		}
		del = (inv - lval);
		outv += 0.99 * del;
	}

	return outv;
}

/* Take a single set of absolute linearised sensor values and */
/* convert them back into Rev A..D raw reading values. */
/* This is used for saving a calibration to the EEProm */
i1pro_code i1pro_absraw_to_meas(
	i1pro *p,
	int *meas,				/* Return raw measurement data */
	double *absraw,			/* Array of [-1 nraw] value to process */
	double inttime, 		/* Integration time used */
	int gainmode			/* Gain mode, 0 = normal, 1 = high */
) {
	i1proimp *m = (i1proimp *)p->m;
	unsigned int maxpve = m->maxpve;	/* maximum +ve sensor value + 1 */ 
	int i, j, k;
	double avlastv = 0.0;
	double gain;
	int npoly;			/* Number of linearisation coefficients */
	double *polys;		/* the coeficients */
	double scale;		/* Absolute scale value */

	if (m->subtmode) {
		a1logd(p->log,1,"i1pro_absraw_to_meas subtmode set\n");
		return I1PRO_INT_MALLOC;
	}

	if (gainmode) {
		gain = m->highgain;
		npoly = m->nlin1;
		polys = m->lin1;
	} else {
		gain = 1.0;
		npoly = m->nlin0;
		polys = m->lin0;
	}
	scale = 1.0/(inttime * gain);

	for (j = 0; j < 128; j++)  {
		double fval, lval;
		unsigned int rval;

		/* Unscale from absolute sensor reading */
		lval = absraw[j] / scale;

#ifdef ENABLE_NONLINCOR	
		/* Un-linearise */
		fval = inv_poly(polys, npoly, lval);
#else
		fval = lval;
#endif

		if (fval < (double)((int)maxpve-65536))
			fval = (double)((int)maxpve-65536);
		else if (fval > (double)(maxpve-1))
			fval = (double)(maxpve-1);

		rval = (unsigned int)(int)floor(fval + 0.5);
		meas[j] = rval;
	}
	return I1PRO_OK;
}

/* Average a set of measurements into one. */
/* Return zero if readings are consistent and not saturated. */
/* Return nz with bit 1 set if the readings are not consistent */
/* Return nz with bit 2 set if the readings are saturated */
/* Return the highest individual element. */
/* Return the overall average. */
int i1pro_average_multimeas(
	i1pro *p,
	double *avg,			/* return average [-1 nraw] */
	double **multimeas,		/* Array of [nummeas][-1 nraw] value to average */
	int nummeas,			/* number of readings to be averaged */
	double *phighest,		/* If not NULL, return highest value from all bands and msrmts. */
	double *poallavg,		/* If not NULL, return overall average of bands and measurements */
	double satthresh,		/* Sauration threshold, 0 for none */
	double darkthresh		/* Dark threshold (used for consistency check scaling) */
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j;
	double highest = -1e6;
	double oallavg = 0.0;
	double avgoverth = 0.0;		/* Average over threshold */
	double maxavg = -1e38;		/* Track min and max averages of readings */
	double minavg = 1e38;
	double norm;
	int rv = 0;
	
	a1logd(p->log,3,"i1pro_average_multimeas %d readings\n",nummeas);

	for (j = -1; j < 128; j++) 
		avg[j] = 0.0;

	/* Now process the buffer values */
	for (i = 0; i < nummeas; i++) {
		double measavg = 0.0;
		int k;

		for (j = k = 0; j < m->nraw; j++) {
			double val;

			val = multimeas[i][j];

			avg[j] += val;		/* Per value average */

			/* Skip 0 and 127 cell values for RevA-D */
			if (m->nsen == m->nraw && (j == 0 || j == 127))
				continue;

			if (val > highest)
				highest = val;
			if (val > satthresh)
				avgoverth++;
			measavg += val;
			k++;
		}
		measavg /= (double)k;
		oallavg += measavg;
		if (measavg < minavg)
			minavg = measavg;
		if (measavg > maxavg)
			maxavg = measavg;

		/* and shielded values */
		avg[-1] += multimeas[i][-1];
	}

	for (j = -1; j < 128; j++) 
		avg[j] /= (double)nummeas;
	oallavg /= (double)nummeas;
	avgoverth /= (double)nummeas;

	if (phighest != NULL)
		*phighest = highest;

	if (poallavg != NULL)
		*poallavg = oallavg;

	if (satthresh > 0.0 && avgoverth > 0.0)
		rv |= 2;

	norm = fabs(0.5 * (maxavg+minavg));
	a1logd(p->log,4,"norm = %f, dark thresh = %f\n",norm,darkthresh);
	if (norm < (2.0 * darkthresh))
		norm = 2.0 * darkthresh;

	a1logd(p->log,4,"overall avg = %f, minavg = %f, maxavg = %f, variance %f, shielded avg %f\n",
	                   oallavg,minavg,maxavg,(maxavg - minavg)/norm, avg[-1]);
	if ((maxavg - minavg)/norm > PATCH_CONS_THR) {
		a1logd(p->log,2,"Reading is inconsistent: (maxavg %f - minavg %f)/norm %f = %f > thresh %f, darkthresh %f\n",maxavg,minavg,norm,(maxavg - minavg)/norm,PATCH_CONS_THR, darkthresh);
		rv |= 1;
	}
	return rv;
}

/* Minimum number of scan samples in a patch */
#define MIN_SAMPLES 3

/* Range of bands to detect transitions */
#define BL 5	/* Start */
#define BH 105	/* End */
#define BW 5	/* Width */

/* Record of possible patch */
typedef struct {
	int ss;				/* Start sample index */
	int no;				/* Number of samples */
	int use;			/* nz if patch is to be used */
} i1pro_patch;

/* Recognise the required number of ref/trans patch locations, */
/* and average the measurements within each patch. */
/* *flags returns zero if readings are consistent and not saturated. */
/* *flags returns nz with bit 1 set if the readings are not consistent */
/* *flags returns nz with bit 2 set if the readings are saturated */
/* *phighest returns the highest individual element. */
/* (Doesn't extract [-1] shielded values, since they have already been used) */
i1pro_code i1pro_extract_patches_multimeas(
	i1pro *p,
	int *flags,				/* return flags */
	double **pavg,			/* return patch average [naptch][-1 nraw] */
	int tnpatch,			/* Target number of patches to recognise */
	double **multimeas,		/* Array of [nummeas][-1 nraw] value to extract from */
	int nummeas,			/* number of readings made */
	double *phighest,		/* If not NULL, return highest value from all bands and msrmts. */
	double satthresh,		/* Sauration threshold, 0 for none */
	double inttime			/* Integration time (used to adjust consistency threshold) */
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k, pix;
	double **sslope;			/* Signed difference between i and i+1 */
	double *slope;				/* Accumulated absolute difference between i and i+1 */
	double *fslope;				/* Filtered slope */
	i1pro_patch *pat;			/* Possible patch information */
	int npat, apat = 0;
	double *maxval;				/* Maximum input value for each wavelength */
	double fmaxslope = 0.0;
	double maxslope = 0.0;
	double minslope =  1e38;
	double thresh = 0.4;		/* Slope threshold */
	int try;					/* Thresholding try */
	double avglegth;			/* Average length of patches */
	int *sizepop;				/* Size popularity of potential patches */
	double median;				/* median potential patch width */
	double window;				/* +/- around median to accept */
	double highest = -1e6;
	double white_avg;			/* Average of (aproximate) white data */
	int b_lo = BL;				/* Patch detection low band */
	int b_hi = BH;				/* Patch detection low band */
	int rv = 0;
	double patch_cons_thr = PATCH_CONS_THR * m->scan_toll_ratio;
#ifdef PATREC_DEBUG
	double **plot;
#endif

	a1logd(p->log,2,"i1pro_extract_patches_multimeas looking for %d patches out of %d samples\n",tnpatch,nummeas);

	/* Adjust bands if UV mode */
	/* (This is insufficient for useful patch recognition) */
	if (m->uv_en) {
		b_lo = 91;
		b_hi = 117;
	}

	maxval = dvectorz(-1, m->nraw-1);  

	/* Loosen consistency threshold for short integation time, */
	/* to allow for extra noise */
	if (inttime < 0.012308)		/* Smaller than Rev A minimum int. time */
		patch_cons_thr *= sqrt(0.012308/inttime);

	/* Discover the maximum input value for normalisation */
	for (j = 0; j < m->nraw; j ++) {
		for (i = 0; i < nummeas; i++) {
			if (multimeas[i][j] > maxval[j])
				maxval[j] = multimeas[i][j];
		}
		if (maxval[j] < 1.0)
			maxval[j] = 1.0;
	}

#ifdef PATREC_DEBUG
	/* Plot out 6 lots of 8 values each */ 
	plot = dmatrixz(0, 8, 0, nummeas-1);  
//	for (j = 45; j <= (m->nraw-8); j += 100) 		/* Do just one band */
#ifdef PATREC_ALLBANDS
	for (j = 0; j <= (m->nraw-8); j += 8) 			/* Plot all the bands */
#else
	for (j = 5; j <= (m->nraw-8); j += 30) 			/* Do four bands */
#endif
	{
		for (k = 0; k < 8; k ++) {
			for (i = 0; i < nummeas; i++) { 
				plot[k][i] = multimeas[i][j+k]/maxval[j+k];
			}
		}
		for (i = 0; i < nummeas; i++)
			plot[8][i] = (double)i;
		printf("Bands %d - %d\n",j,j+7);
		do_plot10(plot[8], plot[0], plot[1], plot[2], plot[3], plot[4], plot[5], plot[6], plot[7], NULL, NULL, nummeas, 0);
	}
#endif	/* PATREC_DEBUG */

	sslope = dmatrixz(0, nummeas-1, -1, m->nraw-1);  
	slope = dvectorz(0, nummeas-1);  
	fslope = dvectorz(0, nummeas-1);  
	sizepop = ivectorz(0, nummeas-1);

#ifndef NEVER		/* Good with this on */
	/* Average bands together */
	for (i = 0; i < nummeas; i++) {
		for (j = b_lo + BW; j < (b_hi - BW); j++) {
			for (k = -b_lo; k <= BW; k++)		/* Box averaging filter over bands */
				 sslope[i][j] += multimeas[i][j + k]/maxval[j];
		}
	}
#else
	/* Don't average bands */
	for (i = 0; i < nummeas; i++) {
		for (j = 0; j < m->nraw; j++) {
			sslope[i][j] = multimeas[i][j]/maxval[j];
		}
	}
#endif

	/* Compute slope result over readings and bands */
	/* Compute signed slope result over readings and bands */

#ifdef NEVER		/* Works well for non-noisy readings */
	/* Median of 5 differences from 6 points */
	for (i = 2; i < (nummeas-3); i++) {
		for (j = b_lo; j < b_hi; j++) {
			double sl, asl[5];
			int r, s;
			asl[0] = fabs(sslope[i-2][j] - sslope[i-1][j]);
			asl[1] = fabs(sslope[i-1][j] - sslope[i-0][j]);
			asl[2] = fabs(sslope[i-0][j] - sslope[i+1][j]);
			asl[3] = fabs(sslope[i+1][j] - sslope[i+2][j]);
			asl[4] = fabs(sslope[i+2][j] - sslope[i+3][j]);

			/* Sort them */
			for (r = 0; r < (5-1); r++) {
				for (s = r+1; s < 5; s++) {
					if (asl[s] < asl[r]) {
						double tt;
						tt = asl[s];
						asl[s] = asl[r];
						asl[r] = tt;
					}
				}
			}
			/* Pick middle one */
			sl = asl[2];
			if (sl > slope[i])
				slope[i] = sl;
		}
	}

#else	/* Works better for noisy readings */

	/* Compute sliding window average and deviation that contains */
	/* our output point, and chose the average with the minimum deviation. */
#define FW 3		/* Number of delta's to average */
	for (i = FW-1; i < (nummeas-FW); i++) {		/* Samples */
		double basl, bdev;		/* Best average slope, Best deviation */
		double sl[2 * FW -1];
		double asl[FW], dev[FW];		
		int slopen = 0;
		double slopeth = 0.0;
		int m, pp;

		for (pp = 0; pp < 2; pp++) { 			/* For each pass */

			for (j = b_lo; j < b_hi; j++) {				/* Bands */

				/* Compute differences for the range of our windows */
				for (k = 0; k < (2 * FW -1); k++)
					sl[k] = sslope[i+k-FW+1][j] - sslope[i+k+-FW+2][j];

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

#ifndef NEVER
				/* Weight the deviations with a triangular weighting */
				/* to skew slightly towards the center */
				for (k = 0; k < FW; k++) { 
					double wt;

					wt = fabs(2.0 * k - (FW -1.0))/(FW-1.0);
					dev[k] += wt * bdev;
				}
#endif

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
#endif

#ifndef NEVER		/* Good with this on */
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

	/* "Automatic Gain control" the raw slope information. */
#define LFW 20		/* Half width of triangular filter */
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

#ifdef NEVER		/* Better with the off, for very noisy samples */
	/* Apply AGC with limited gain */
	for (i = 0; i < nummeas; i++) {
		if (fslope[i] > fmaxslope/4.0)
			slope[i] = slope[i]/fslope[i];
		else
			slope[i] = slope[i] * 4.0/fmaxslope;
	}
#endif
#endif /* NEVER */

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

#ifndef NEVER		/* Good with this on */
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
#endif

#ifdef PATREC_DEBUG
	printf("Slope filter output\n");
	for (i = 0; i < nummeas; i++) { 
		int jj;
		for (jj = 0, j = b_lo; jj < 6 && j < b_hi; jj++, j += ((b_hi-b_lo)/6)) {
			double sum = 0.0;
			for (k = -b_lo; k <= BW; k++)		/* Box averaging filter over bands */
				sum += multimeas[i][j + k];
			plot[jj][i] = sum/((2.0 * b_lo + 1.0) * maxval[j+k]);
		}
	}
	for (i = 0; i < nummeas; i++)
		plot[6][i] = (double)i;
	do_plot6(plot[6], slope, plot[0], plot[1], plot[2], plot[3], plot[4], nummeas);
#endif	/* PATREC_DEBUG */

	free_dvector(fslope, 0, nummeas-1);  
	free_dmatrix(sslope, 0, nummeas-1, -1, m->nraw-1);  

	/* Now threshold the measurements into possible patches */
	apat = 2 * nummeas;
	if ((pat = (i1pro_patch *)malloc(sizeof(i1pro_patch) * apat)) == NULL) {
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, -1, m->nraw-1);  
		a1logd(p->log, 1, "i1pro: malloc of patch structures failed!\n");
		return I1PRO_INT_MALLOC;
	}

	avglegth = 0.0;
	for (npat = i = 0; i < (nummeas-1); i++) {
		if (slope[i] > thresh)
			continue;

		/* Start of a new patch */
		if (npat >= apat) {
			apat *= 2;
			if ((pat = (i1pro_patch *)realloc(pat, sizeof(i1pro_patch) * apat)) == NULL) {
				free_ivector(sizepop, 0, nummeas-1);
				free_dvector(slope, 0, nummeas-1);  
				free_dvector(maxval, -1, m->nraw-1);  
				a1logd(p->log, 1, "i1pro: reallloc of patch structures failed!\n");
				return I1PRO_INT_MALLOC;
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
		avglegth += (double) pat[npat].no;
		npat++;
	}
	a1logd(p->log,7,"Number of patches = %d\n",npat);

	/* We don't count the first and last patches, as we assume they are white leader */
	if (npat < (tnpatch + 2)) {
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, -1, m->nraw-1);  
		free(pat);
		a1logd(p->log,2,"Patch recog failed - unable to detect enough possible patches\n");
		return I1PRO_RD_NOTENOUGHPATCHES;
	} else if (npat >= (2 * tnpatch) + 2) {
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, -1, m->nraw-1);  
		free(pat);
		a1logd(p->log,2,"Patch recog failed - detecting too many possible patches\n");
		return I1PRO_RD_TOOMANYPATCHES;
	}
	avglegth /= (double)npat;

	for (i = 0; i < npat; i++) {
		a1logd(p->log,7,"Raw patch %d, start %d, length %d\n",i, pat[i].ss, pat[i].no);
	}

	/* Accumulate popularity ccount of possible patches */
	for (i = 1; i < (npat-1); i++)
		sizepop[pat[i].no]++;

	/* Locate the median potential patch width */
	for (j = 0, i = 0; i < nummeas; i++) {
		j += sizepop[i];
		if (j >= ((npat-2)/2))
			break;
	}
	median = (double)i;

	a1logd(p->log,7,"Median patch width %f\n",median);

	/* Now decide which patches to use. */
	/* Try a widening window around the median. */
	for (window = 0.2, try = 0; try < 15; window *= 1.4, try++) {
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

		if (bgcount == tnpatch) {			/* We're done */
			for (i = bgstart, j = 0; i < npat && j < tnpatch; i++) {
				if (pat[i].no <= wmax && pat[i].no >= wmin) {
					pat[i].use = 1;
					j++;
					if (pat[i].no < MIN_SAMPLES) {
						a1logd(p->log,7,"Too few samples\n");
						free_ivector(sizepop, 0, nummeas-1);
						free_dvector(slope, 0, nummeas-1);  
						free_dvector(maxval, -1, m->nraw-1);  
						free(pat);
						a1logd(p->log,2,"Patch recog failed - patches sampled too sparsely\n");
						return I1PRO_RD_NOTENOUGHSAMPLES;
					}
				}
			}
			break;

		} else if (bgcount > tnpatch) {
			a1logd(p->log,7,"Too many patches\n");
			free_ivector(sizepop, 0, nummeas-1);
			free_dvector(slope, 0, nummeas-1);  
			free_dvector(maxval, -1, m->nraw-1);  
			free(pat);
			a1logd(p->log,2,"Patch recog failed - detected too many consistent patches\n");
			return I1PRO_RD_TOOMANYPATCHES;
		}
	}
	if (try >= 15) {
		a1logd(p->log,7,"Not enough patches\n");
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, -1, m->nraw-1);  
		free(pat);
		a1logd(p->log,2,"Patch recog failed - unable to find enough consistent patches\n");
		return I1PRO_RD_NOTENOUGHPATCHES;
	}

	a1logd(p->log,7,"Got %d patches out of potential %d:\n",tnpatch, npat);
	a1logd(p->log,7,"Average patch legth %f\n",avglegth);
	for (i = 1; i < (npat-1); i++) {
		if (pat[i].use == 0)
			continue;
		a1logd(p->log,7,"Patch %d, start %d, length %d:\n",i, pat[i].ss, pat[i].no, pat[i].use);
	}

	/* Now trim the patches simply by shrinking their windows */
	for (k = 1; k < (npat-1); k++) {
		int nno, trim;

		if (pat[k].use == 0)
			continue;

		
		nno = (pat[k].no * 3)/4;
		trim = (pat[k].no - nno)/2;

		pat[k].ss += trim;
		pat[k].no = nno;
	}

#ifdef PATREC_DEBUG
	a1logd(p->log,7,"After trimming got:\n");
	for (i = 1; i < (npat-1); i++) {
		if (pat[i].use == 0)
			continue;
		printf("Patch %d, start %d, length %d:\n",i, pat[i].ss, pat[i].no, pat[i].use);
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
	for (i = 0; i < nummeas; i++) { 
		int jj;
		for (jj = 0, j = b_lo; jj < 6 && j < b_hi; jj++, j += ((b_hi-b_lo)/6)) {
			double sum = 0.0;
			for (k = -b_lo; k <= BW; k++)		/* Box averaging filter over bands */
				sum += multimeas[i][j + k];
			plot[jj][i] = sum/((2.0 * b_lo + 1.0) * maxval[j+k]);
		}
	}
	for (i = 0; i < nummeas; i++)
		plot[6][i] = (double)i;
	do_plot6(plot[6], slope, plot[0], plot[1], plot[2], plot[3], plot[4], nummeas);
#endif	/* PATREC_DEBUG */

#ifdef PATREC_DEBUG
	free_dmatrix(plot, 0, 6, 0, nummeas-1);  
#endif	/* PATREC_DEBUG */

	/* Compute average of (aproximate) white */
	white_avg = 0.0;
	for (j = 1; j < (m->nraw-1); j++) 
		white_avg += maxval[j];
	white_avg /= (m->nraw - 2.0);

	/* Now process the buffer values */
	for (i = 0; i < tnpatch; i++) {
		for (j = 0; j < m->nraw; j++) 
			pavg[i][j] = 0.0;
	}

	for (pix = 0, k = 1; k < (npat-1); k++) {
		double maxavg = -1e38;	/* Track min and max averages of readings for consistency */
		double minavg = 1e38;
		double avgoverth = 0.0;	/* Average over saturation threshold */
		double cons;			/* Consistency */

		if (pat[k].use == 0)
			continue;

		if (pat[k].no <= MIN_SAMPLES) {
			a1logd(p->log,7,"Too few samples (%d, need %d)\n",pat[k].no,MIN_SAMPLES);
			free_dvector(slope, 0, nummeas-1);  
			free_ivector(sizepop, 0, nummeas-1);
			free_dvector(maxval, -1, m->nraw-1);  
			free(pat);
			a1logd(p->log,2,"Patch recog failed - patches sampled too sparsely\n");
			return I1PRO_RD_NOTENOUGHSAMPLES;
		}

		/* Measure samples that make up patch value */
		for (i = pat[k].ss; i < (pat[k].ss + pat[k].no); i++) {
			double measavg = 0.0;

			for (j = 1; j < m->nraw-1; j++) {
				double val;

				val = multimeas[i][j];

				if (val > highest)
					highest = val;
				if (val > satthresh)
					avgoverth++;
				measavg += val;
				pavg[pix][j] += val;
			}
			measavg /= (m->nraw-2.0);
			if (measavg < minavg)
				minavg = measavg;
			if (measavg > maxavg)
				maxavg = measavg;

			/* and the duplicated values at the end */
			pavg[pix][0]   += multimeas[i][0];
			pavg[pix][127] += multimeas[i][127];
		}

		for (j = 0; j < m->nraw; j++) 
			pavg[pix][j] /= (double)pat[k].no;
		avgoverth /= (double)pat[k].no;

		if (satthresh > 0.0 && avgoverth >= 10.0)
			rv |= 2;

		cons = (maxavg - minavg)/white_avg;
		a1logd(p->log,7,"Patch %d: consistency = %f%%, thresh = %f%%\n",pix,100.0 * cons, 100.0 * patch_cons_thr);
		if (cons > patch_cons_thr) {
			a1logd(p->log,2,"Patch recog failed - patch %d is inconsistent (%f%% > %f)\n",pix,cons, patch_cons_thr);
			rv |= 1;
		}
		pix++;
	}

	if (phighest != NULL)
		*phighest = highest;
	if (flags != NULL)
		*flags = rv;

	free_dvector(slope, 0, nummeas-1);  
	free_ivector(sizepop, 0, nummeas-1);
	free_dvector(maxval, -1, m->nraw-1);  
	free(pat);

	if (rv & 2)
		a1logd(p->log,2,"Patch recog failed - some patches are saturated\n");

	a1logd(p->log,2,"i1pro_extract_patches_multimeas done, sat = %s, inconsist = %s\n",
	                  rv & 2 ? "true" : "false", rv & 1 ? "true" : "false");

	return I1PRO_OK;
}
#undef BL
#undef BH
#undef BW


/* Recognise any flashes in the readings, and */
/* and average their values together as well as summing their duration. */
/* Return nz on an error */
/* (Doesn't extract [-1] shielded values, since they have already been used) */
i1pro_code i1pro_extract_patches_flash(
	i1pro *p,
	int *flags,				/* return flags */
	double *duration,		/* return duration */
	double *pavg,			/* return patch average [-1 nraw] */
	double **multimeas,		/* Array of [nummeas][-1 nraw] value to extract from */
	int nummeas,			/* number of readings made */
	double inttime			/* Integration time (used to compute duration) */
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k, pix;
	double minval, maxval;		/* min and max input value at wavelength of maximum input */
	double mean;				/* Mean of the max wavelength band */
	int maxband;				/* Band of maximum value */
	double thresh;				/* Level threshold */
	int fsampl;					/* Index of the first sample over the threshold */
	int nsampl;					/* Number of samples over the threshold */
	double *aavg;				/* ambient average [-1 nraw] */
	double finttime;			/* Flash integration time */
	int rv = 0;
#ifdef PATREC_DEBUG
	double **plot;
#endif

	a1logd(p->log,2,"i1pro_extract_patches_flash looking for flashes in %d measurements\n",nummeas);

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
		return I1PRO_RD_NOFLASHES;
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
	a1logd(p->log,7,"i1pro_extract_patches_flash band %d minval %f maxval %f, mean = %f, thresh = %f\n",maxband,minval,maxval,mean, thresh);

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
		for (j = 0; j < m->nraw-1; j++) {
			if (multimeas[i][j] >= thresh)
				break;
		}
		if (j < m->nraw-1) {
			if (fsampl < 0)
				fsampl = i;
			nsampl++;
		}
	}
	a1logd(p->log,7,"Number of flash patches = %d\n",nsampl);
	if (nsampl == 0)
		return I1PRO_RD_NOFLASHES;

	/* See if there are as many samples before the first flash */
	if (nsampl < 6)
		nsampl = 6;

	/* Average nsample samples of ambient */
	i = (fsampl-3-nsampl);
	if (i < 0)
		return I1PRO_RD_NOAMBB4FLASHES;
	a1logd(p->log,7,"Ambient samples %d to %d \n",i,fsampl-3);
	aavg = dvectorz(-1, m->nraw-1);  
	for (nsampl = 0; i < (fsampl-3); i++) {
		for (j = 0; j < m->nraw-1; j++)
			aavg[j] += multimeas[i][j];
		nsampl++;
	}

	/* Integrate all the values over the threshold, */
	/* and also one either side of flash */
	for (j = 0; j < m->nraw-1; j++)
		pavg[j] = 0.0;

	for (k = 0, i = 1; i < (nummeas-1); i++) {
		int sample = 0;
		for (j = 0; j < m->nraw-1; j++) {
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
		if (j < m->nraw-1) {
			a1logd(p->log,7,"Integrating flash sample no %d \n",i);
			for (j = 0; j < m->nraw-1; j++)
				pavg[j] += multimeas[i][j];
			k++;
		}
	}
	for (j = 0; j < m->nraw-1; j++)
		pavg[j] = pavg[j]/(double)k - aavg[j]/(double)nsampl;

	a1logd(p->log,7,"Number of flash patches integrated = %d\n",k);

	finttime = inttime * (double)k;
	if (duration != NULL)
		*duration = finttime;

	/* Convert to cd/m^2 seconds */
	for (j = 0; j < m->nraw-1; j++)
		pavg[j] *= finttime;

	if (flags != NULL)
		*flags = rv;

	free_dvector(aavg, -1, m->nraw-1);  

	return I1PRO_OK;
}


/* Subtract the black level. */
/* If Rev E, also adjust according to shielded cells, and linearise. */
void i1pro_sub_absraw(
	i1pro *p,
	int nummeas,			/* Return number of readings measured */
	double inttime,			/* Integration time used */
	int gainmode,			/* Gain mode, 0 = normal, 1 = high */
	double **absraw,		/* Source/Desination array [-1 nraw] */
	double *sub				/* Black value to subtract [-1 nraw] */
) {
	i1proimp *m = (i1proimp *)p->m;
	double gain;
	int npoly;			/* Number of linearisation coefficients */
	double *polys;		/* the coeficients */
	double scale;		/* Absolute scale value */
	double submax = -1e6;	/* Subtraction value maximum */
	int i, j;
	
	if (gainmode) {
		gain = m->highgain;
		npoly = m->nlin1;
		polys = m->lin1;
	} else {
		gain = 1.0;
		npoly = m->nlin0;
		polys = m->lin0;
	}
	scale = 1.0/(inttime * gain);	/* To scale RevE linearity */

	/* Adjust black to allow for temperature change by using the */
	/* shielded cell values as a reference. */
	/* We use a heuristic to compute a zero based scale for adjusting the */
	/* black. It's not clear why it works best this way, or how */
	/* dependent on the particular instrument the magic numbers are, */
	/* but it reduces the black level error from over 10% to about 0.3% */
	if (p->itype == instI1Pro2) {
//		double xx[NSEN_MAX], in[NSEN_MAX], res[NSEN_MAX];
		double asub[NSEN_MAX];
		double avgscell, zero;

		/* Locate largest of black */
		for (j = 0; j < m->nraw; j++) {
			if (sub[j] > submax)
				submax = sub[j];
		}

		/* Average the shielded cell value of all the readings */
		avgscell = 0.0;
		for (i = 0; i < nummeas; i++)
			avgscell += absraw[i][-1];
		avgscell /= (double)nummeas;

		/* Compute scaling zero */
		zero = 1.144 * 0.5 * (avgscell + sub[-1]);

		/* make sure that the zero point is above any black value */
		if (zero < (1.01 * avgscell))
			zero = 1.01 * avgscell;
		if (zero < (1.01 * sub[-1]))
			zero = 1.01 * sub[-1];
		if (zero < (1.01 * submax))
			zero = 1.01 * submax;

		a1logd(p->log,2,"Black shielded value = %f, Reading shielded value = %f\n",sub[-1], avgscell);
		/* Compute the adjusted black */
		/* [ Unlike the ColorMunki, using the black drift comp. for reflective */
		/*   seems to be OK and even beneficial. ] */
		for (j = 0; j < m->nraw; j++) {
#ifdef ENABLE_BKDRIFTC
# ifdef HEURISTIC_BKDRIFTC
			/* heuristic scaled correction */
			asub[j] = zero - (zero - sub[j]) * (zero - avgscell)/(zero - sub[-1]);
# else
			/* simple additive correction */
#  pragma message("######### i1pro2 Simple shielded cell temperature correction! ########")
			asub[j] = sub[j] + avgscell - sub[-1];
# endif
#else
#  pragma message("######### i1pro2 No shielded cell temperature correction! ########")
			asub[j] = sub[j];		/* Just use the calibration dark data */
#endif
		}
	
		/* Subtract the black */
		for (i = 0; i < nummeas; i++) {
			for (j = 0; j < m->nraw; j++) {
//				xx[j] = j, in[j] = absraw[i][j];

				absraw[i][j] -= asub[j]; 		/* Subtract adjusted black */

//				 res[j] = absraw[i][j] + (double)((int)(avgscell/20.0)) * 20.0;
#ifdef ENABLE_NONLINCOR	
				/* Linearise */
				{
					int k;
					double fval, lval;

					fval = absraw[i][j] / scale;		/* Scale back to sensor value range */

					for (lval = polys[npoly-1], k = npoly-2; k >= 0; k--)
						lval = lval * fval + polys[k];

					absraw[i][j] = scale * lval;		/* Rescale back to absolute range */
				}
#endif
			}
#ifdef PLOT_BLACK_SUBTRACT	/* Plot black adjusted levels */
			printf("black = meas, red = black, green = adjuste black, blue = result\n"); 
			do_plot6(xx, in, sub, adjsub, res, NULL, NULL, m->nraw);
#endif
		}

	/* Rev A-D don't have shielded reference cells */
	} else {
	
		/* For each measurement */
		for (i = 0; i < nummeas; i++) {
			for (j = -1; j < m->nraw; j++) {
				absraw[i][j] -= sub[j];
			}
		}
	}
}

/* Convert an absraw array from raw wavelengths to output wavelenths */
/* for a given [std res, high res] and [emis/tras, reflective] mode */
void i1pro_absraw_to_abswav(
	i1pro *p,
	int highres,			/* 0 for std res, 1 for high res */ 
	int refl,				/* 0 for emis/trans, 1 for reflective */ 
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav] */
	double **absraw			/* Source array [-1 nraw] */
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k, cx, sx;
	double *tm;			/* Temporary array */
	
	tm = dvector(0, m->nwav[highres]-1);
	
	/* For each measurement */
	for (i = 0; i < nummeas; i++) {

		/* For each output wavelength */
		for (cx = j = 0; j < m->nwav[highres]; j++) {
			double oval = 0.0;
	
			/* For each matrix value */
			sx = m->mtx[highres][refl].index[j];		/* Starting index */
			for (k = 0; k < m->mtx[highres][refl].nocoef[j]; k++, cx++, sx++) {
				oval += m->mtx[highres][refl].coef[cx] * absraw[i][sx];
			}
			abswav[i][j] = tm[j] = oval;
		}

		if (p->itype == instI1Pro2) {
			/* Now apply stray light compensation */ 
			/* For each output wavelength */
			for (j = 0; j < m->nwav[highres]; j++) {
				double oval = 0.0;
		
				/* For each matrix value */
				for (k = 0; k < m->nwav[highres]; k++)
					oval += m->straylight[highres][j][k] * tm[k];
				abswav[i][j] = oval;
			}
#ifdef PLOT_DEBUG
			printf("Before & after stray light correction:\n");
			plot_wav_2(m, highres, tm, abswav[i]);
#endif /* PLOT_DEBUG */
		}
	}
	free_dvector(tm, 0, m->nwav[highres]-1);
}

/* Convert an abswav array of output wavelengths to scaled output readings. */
void i1pro_scale_specrd(
	i1pro *p,
	double **outspecrd,		/* Destination */
	int numpatches,			/* Number of readings/patches */
	double **inspecrd		/* Source */
) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	int i, j;
	
	/* For each measurement */
	for (i = 0; i < numpatches; i++) {

		/* For each output wavelength */
		for (j = 0; j < m->nwav[m->highres]; j++) {
			outspecrd[i][j] = inspecrd[i][j] * s->cal_factor[m->highres][j];
		}
	}
}


/* =============================================== */
/* Rev E wavelength calibration                    */

/*
	The Rev E has a wavelength reference LED amd
	stores a reference raw spectrum of it in its
	calibrated state, together with an polinomial
	defining the raw bin no. to wavelength conversion.

	By measuring the wavelength LED and finding
	the best positional match against the reference
	spectrum, a CCD bin offset can be computed
	to compensate for any shift in the optical or
	physical alignment of spectrum against CCD.

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

/* Invert a raw2wavlength polinomial equation. */
/* Use simple Newton inversion will suffice. */
static double inv_raw2wav(double *polys, int npoly, double inv) {
	double outv = 560.0, lval, del = 100.0;
	int i, k;

	for (i = 0; i < 200 && fabs(del) > 1e-7; i++) {
		for (lval = polys[npoly-1], k = npoly-2; k >= 0; k--) {
			lval = lval * outv + polys[k];
		}
		del = (inv - lval);
		outv += 0.4 * del;
	}

	return 128.0 - outv;
}

/* return the uncalibrated wavelength given a raw bin value */
/* (Always uses reflective RevE wav2cal) */
static double i1pro_raw2wav_uncal(i1pro *p, double raw) {
	i1proimp *m = (i1proimp *)p->m;
	double ov;
	int k;

	if (p->itype == instI1Pro2) {
		raw = 128.0 - raw;		/* Quadratic expects +ve correlation */
		
		/* Compute polinomial */
		for (ov = m->wlpoly1[4-1], k = 4-2; k >= 0; k--)
			ov = ov * raw + m->wlpoly1[k];
	} else {
		co pp;

		if (m->raw2wav == NULL) {
			a1loge(p->log,1,"i1pro_raw2wav_uncal called when hi-res not inited\n");
			return I1PRO_INT_ASSERT;
		}

		pp.p[0] = raw;
		m->raw2wav->interp(m->raw2wav, &pp);
		ov = pp.v[0];
	}

	return ov;
} 

/* return the calibrated wavelength given a raw bin value for the given mode */
static double i1pro_raw2wav(i1pro *p, int refl, double raw) {
	i1proimp *m = (i1proimp *)p->m;
	double ov;
	int k;

	if (p->itype == instI1Pro2) {
		i1pro_state *s = &m->ms[m->mmode];

		/* Correct for CCD offset and scale back to reference */
		raw = raw - s->wl_led_off + m->wl_led_ref_off;

		raw = 128.0 - raw;		/* Quadratic expects +ve correlation */
		
		/* Compute polinomial */
		if (refl) {
			for (ov = m->wlpoly1[4-1], k = 4-2; k >= 0; k--)
				ov = ov * raw + m->wlpoly1[k];
		} else {
			for (ov = m->wlpoly2[4-1], k = 4-2; k >= 0; k--)
				ov = ov * raw + m->wlpoly2[k];
		}
	} else {
		co pp;

		/* If not RevE there is no WL calibration */
		if (m->raw2wav == NULL) {
			a1loge(p->log,1,"i1pro_raw2wav_uncal called when hi-res not inited\n");
			return I1PRO_INT_ASSERT;
		}

		pp.p[0] = raw;
		m->raw2wav->interp(m->raw2wav, &pp);
		ov = pp.v[0];
	}

	return ov;
}

/* Powell minimisation contxt for WL calibration */
typedef struct {
	double ref_max;			/* reference maximum level */
	double *wl_ref;		/* Wavlength reference samples */
	int wl_ref_n;		/* Number of wavelength references */
	double *wl_meas;	/* Wavelength measurement samples */
	int wl_meas_n;		/* Number of wavelength measurement samples */
	int plot;			/* Plot each try */
} wlcal_cx;

/* Powell minimisation callback function */
/* Parameters being optimized are magnitude, offset and scale */ 
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
		printf("Params %f %f -> err %f\n", tp[0], tp[1], rv);
		do_plot(xx, y1, y2, y3, pix);
	}
#endif
//printf("~1 %f %f -> %f\n", tp[0], tp[1], rv);
	return rv;
}

#ifdef SALONEINSTLIB
/* Do a rudimetrary 2d optimization that uses exaustive */
/* search with hierarchical step sizes */
int wloptimize(double *cparm,
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
i1pro_code i1pro2_match_wl_meas(i1pro *p, double *pled_off, double *wlraw) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code ev = I1PRO_OK;
	int i;
	int rpoff, mpoff;		/* Peak offset */
	int roff, moff;			/* Base index */
	double lhalf, rhalf;
	double fwhm;			/* Measured half width */
	double rmax, mmax;
	double magscale;
	double led_off, off_nm;

	/* Do simple match first - locate maximum */
	rmax = -1e6; 
	rpoff = -1;
	for (i = 0; i < m->wl_led_count; i++) {	
		if (m->wl_led_spec[i] > rmax) {
			rmax = m->wl_led_spec[i];			/* Max of reference */
			rpoff = i;
		}
	}
 
	mmax = -1e6; 
	mpoff = -1;
	for (i = 0; i < m->nraw; i++) {
		if (wlraw[i] > mmax) {
			mmax = wlraw[i];					/* Max of measurement */
			mpoff = i;
		}
	}
 
	if (mpoff < 0 || mpoff >= m->nraw) {
		a1logd(p->log,1,"Couldn't locate WL measurement peak\n");
		return I1PRO_WL_SHAPE; 
	}

	/* Check magnitude is sufficient (not sure this is right, typically 5900 > 882) */
	a1logd(p->log,2,"Measured WL level = %f, minimum needed = %f\n",mmax, m->wl_cal_min_level);
	if (mmax < m->wl_cal_min_level) {
		a1logd(p->log,1,"i1pro2_match_wl_meas peak magnitude too low\n");
		return I1PRO_WL_TOOLOW;
	}
		
	/* Locate the half peak values */
	for (i = 1; i < mpoff; i++) {
		if (wlraw[i] > (mmax/2.0)) {	/* Use linear interp */
			lhalf = (wlraw[i] - mmax/2.0)/(wlraw[i] - wlraw[i-1]);
			lhalf = lhalf * (i-1.0) + (1.0 - lhalf) * (double)i;
			break;
		}
	}
	if (i >= mpoff) {
		a1logd(p->log,1,"Couldn't locate WL left half level\n");
		return I1PRO_WL_SHAPE; 
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
		return I1PRO_WL_SHAPE; 
	}
	a1logd(p->log,5,"WL half levels at %f (%f nm) and %f (%f nm)\n",lhalf, i1pro_raw2wav_uncal(p, lhalf), rhalf, i1pro_raw2wav_uncal(p, rhalf));
	fwhm = i1pro_raw2wav_uncal(p, lhalf) - i1pro_raw2wav_uncal(p, rhalf);
	a1logd(p->log,3, "WL spectrum fwhm = %f\n",fwhm);
	if (fwhm < (m->wl_cal_fwhm - m->wl_cal_fwhm_tol) 
	 || fwhm > (m->wl_cal_fwhm + m->wl_cal_fwhm_tol)) {
		a1logd(p->log,1,"WL fwhm %f is out of range %f .. %f\n",fwhm,m->wl_cal_fwhm - m->wl_cal_fwhm_tol,m->wl_cal_fwhm + m->wl_cal_fwhm_tol);
		return I1PRO_WL_SHAPE; 
	}

	roff = m->wl_led_ref_off;		/* reference raw offset */
	moff = mpoff - rpoff;			/* rough measured raw offset */

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
			if (i >= moff && (i - moff) < m->wl_led_count) {
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

		cparm[0] = magscale;
		ss[0] = 0.2;
		cparm[1] = (double)moff;
		ss[1] = 4.0;		/* == +- 12 nm */

		cx.ref_max = rmax;
		cx.wl_ref = m->wl_led_spec;
		cx.wl_ref_n = m->wl_led_count;
		cx.wl_meas = wlraw; 
		cx.wl_meas_n = m->nraw;
//		cx.plot = 1;		/* Plot each trial */

		/* We could use the scale to adjust the whole CCD range, */
		/* but the manufacturers driver doesn't seem to do this, */
		/* and it may be making the calibration sensitive to any */
		/* changes in the WL LED spectrum shape. Instead we minimize */
		/* the error weighted for the peak of the shape. */

#ifdef SALONEINSTLIB
		if (wloptimize(cparm, ss, 1e-7, wlcal_opt1, &cx))
			a1logw(p->log,"wlcal_opt1 failed\n");
#else
		if (powell(NULL, 2, cparm, ss, 1e-6, 1000, wlcal_opt1, &cx, NULL, NULL))
			a1logw(p->log,"wlcal_opt1 failed\n");
#endif
		a1logd(p->log,3,"WL best fit parameters: %f %f\n", cparm[0], cparm[1]);

		led_off = cparm[1];

#ifdef PLOT_DEBUG
		/* Plot the final result */
		printf("Best WL match, ref = black, meas = red, err = green:\n");
		cx.plot = 1;
		wlcal_opt1(&cx, cparm);
#endif

		/* If we have calibrated on the ambient cap, correct */
		/* for the emissive vs. reflective raw2wav scaling factor */
		if (mmax < 2500.0) {
			double wlraw2 = m->wl_led_ref_off + (double)rpoff;
			double raw, wlnm, wlraw1, refnm;
			int k;

			/* Convert from raw to wavelength using poly2 (emission) */
			raw = 128.0 - wlraw2;		/* Quadratic expects +ve correlation */
			for (wlnm = m->wlpoly2[4-1], k = 4-2; k >= 0; k--)
				wlnm = wlnm * raw + m->wlpoly2[k];

			/* Convert from wavelength to raw using poly1 (reflectance) */
			wlraw1 = inv_raw2wav(m->wlpoly1, 4, wlnm);
//printf("emiss raw %f -> ref raw %f\n",wlraw2, wlraw1);

			/* Adjust the raw correction to account for measuring it in emissive mode */
			led_off = led_off + wlraw2 - wlraw1;

			/* Hmm. This is rather suspect. The difference between the white reference */
			/* calibrated wavelength offset and the ambient cap one is about -0.2788 raw. */
			/* This is not explained by the poly1 vs. poly2 difference at the WL LED peak */
			/* at 550 nm. (see above), which amounts to about +0.026, leaving 0.2528 */
			/* unexplained. It appears the CCD wavelength has a dependence on the */
			/* angle that the light enters the optics ?? */

			led_off += 0.2528;	/* Hack to make ambient cap correction == white tile correction */

			a1logd(p->log,3,"Adjusted raw correction by %f to account for measurement using ambient cap\n",wlraw2 - wlraw1 + 0.2528);
		}

		/* Check that the correction is not excessive */
		off_nm = i1pro_raw2wav_uncal(p, led_off) - i1pro_raw2wav_uncal(p, m->wl_led_ref_off);
		a1logd(p->log,2, "Final WL offset = %f, correction %f nm\n",led_off, off_nm);
		if (fabs(off_nm)> m->wl_err_max) {
			a1logd(p->log,1,"Final WL correction of %f nm is too big\n",off_nm);
			return I1PRO_WL_ERR2BIG;
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
			for (ii = 0, i = m->wl_led_ref_off; i < (m->wl_led_ref_off + m->wl_led_count); i++) {
				double raw;
				double mwl;			/* Measurment wavelength */
				double rraw;		/* Reference raw value */
				int ix;				/* Lagrange base offset */
				int k;
				double yv;

				raw = (double)i;
				raw = raw - led_off + m->wl_led_ref_off;
				raw = 128.0 - raw;		/* Quadratic expects +ve correlation */
				if (mmax < 2500.0) {
					for (mwl = m->wlpoly2[4-1], k = 4-2; k >= 0; k--)
						mwl = mwl * raw + m->wlpoly2[k];
				} else {
					for (mwl = m->wlpoly1[4-1], k = 4-2; k >= 0; k--)
						mwl = mwl * raw + m->wlpoly1[k];
				}
				xx[ii] = mwl;
				y1[ii] = cparm[0] * wlraw[i];
				y2[ii] = 0.0;

				/* Compute the reference index corresponding to this wavelength */
				rraw = inv_raw2wav(m->wlpoly1, 4, mwl) - (double)m->wl_led_ref_off;

				/* Use Lagrange to interpolate the reference level for this wavelength */
				ix = ((int)rraw) - 1;	/* Reference index of Lagrange for this xv */
				if (ix < 0)
					continue;
				if ((ix + 3) >= m->wl_led_count)
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

		if (pled_off != NULL)
			*pled_off = led_off;
	}

	return ev; 
}

/* Compute standard/high res. downsampling filters for the given mode */
/* given the current wl_led_off, and set them as current, */
/* using triangular filters of the lagrange interpolation of the */
/* CCD values. */
i1pro_code i1pro_compute_wav_filters(i1pro *p, int hr, int refl) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	i1pro_code ev = I1PRO_OK;
	double twidth;		/* Target filter width */
	int six, eix;		/* raw starting index and one past end index */ 
	int wlix;			/* current wavelenght index */
	double *wlcop;		/* This wavelength base filter coefficient pointer */
	double trh, trx;	/* Triangle height and triangle equation x weighting */
	int i, j, k;

	a1logd(p->log,2,"i1pro_compute_wav_filters called with correction %f raw\n",s->wl_led_off - m->wl_led_ref_off);

	twidth = (m->wl_long[hr] - m->wl_short[hr])/(m->nwav[hr] - 1.0);	/* Filter width */

	trh = 1.0/twidth;		/* Triangle height */
	trx = trh/twidth;		/* Triangle equation x weighting */

	/* Allocate separate space for the calibrated versions, so that the */
	/* original eeprom values are preserved */
	if (m->mtx_c[hr][refl].index == NULL) {

		if ((m->mtx_c[hr][refl].index = (int *)calloc(m->nwav[hr], sizeof(int))) == NULL) {
			a1logd(p->log,1,"i1pro: malloc ndex1 failed!\n");
			return I1PRO_INT_MALLOC;
		}

		if ((m->mtx_c[hr][refl].nocoef = (int *)calloc(m->nwav[hr], sizeof(int))) == NULL) {
			a1logd(p->log,1,"i1pro: malloc nocoef failed!\n");
			return I1PRO_INT_MALLOC;
		}

		if ((m->mtx_c[hr][refl].coef = (double *)calloc(16 * m->nwav[hr], sizeof(double)))
		                                                                        == NULL) {
			a1logd(p->log,1,"i1pro: malloc coef failed!\n");
			return I1PRO_INT_MALLOC;
		}
	}

	/* For each output wavelength */
	wlcop = m->mtx_c[hr][refl].coef;
	for (wlix = 0; wlix < m->nwav[hr]; wlix++) {
		double owl = wlix/(m->nwav[hr]-1.0) * (m->wl_long[hr] - m->wl_short[hr]) + m->wl_short[hr];
		int lip;	/* Lagrange interpolation position */

//		printf("Generating filter for %.1f nm width %.1f nm\n",owl, twidth);

		/* The filter is based on a triangle centered at owl and extending */
		/* from owl - twidth to owl + twidth. We therefore need to locate the */
		/* raw values that will overlap this range */

		/* Do a dumb search from high to low nm */
		for (six = 0; six < m->nraw; six++) {
//printf("~1 (raw2wav (six %d) %f <? (owl %f + twidth %f) %f\n",six,i1pro_raw2wav(p, refl, (double)six),owl,twidth,owl + twidth);

			if (i1pro_raw2wav(p, refl, (double)six) < (owl + twidth))
				break;
		}

		if (six < 2 || six >= m->nraw) {
			a1loge(p->log,1,"i1pro: compute_wav_filters() six %d, exceeds raw range to cover output filter %.1f nm width %.1f nm\n",six, owl, twidth);
			return I1PRO_INT_ASSERT;
		}
		eix = six;
		six -= 2;		/* Outside */

		for (; eix < m->nraw; eix++) {
			if (i1pro_raw2wav(p, refl, (double)eix) <= (owl - twidth))
				break;
		}
		if (eix > (m->nraw - 2) ) {
			a1loge(p->log,1,"i1pro: compute_wav_filters() eix %d, exceeds raw range to cover output filter %.1f nm width %.1f nm\n",eix, owl, twidth);
			return I1PRO_INT_ASSERT;
		}
		eix += 2;		/* Outside */

//		for (j = six; j < eix; j++)
//			printf("Using raw %d @ %.1f nm\n",j, i1pro_raw2wav(p, refl, (double)j));

		/* Set start index for this wavelength */
		m->mtx_c[hr][refl].index[wlix] = six;

		/* Set number of filter coefficients */
		m->mtx_c[hr][refl].nocoef[wlix] = eix - six;

		if (m->mtx_c[hr][refl].nocoef[wlix] > 16) {
			a1loge(p->log,1,"i1pro: compute_wav_filters() too many filter %d\n",m->mtx_c[hr][refl].nocoef[wlix]);
			return I1PRO_INT_ASSERT;
		}

		/* Start with zero filter weightings */
		for (i = 0; i < m->mtx_c[hr][refl].nocoef[wlix]; i++)
			wlcop[i] = 0.0;
		
		/* for each Lagrange interpolation position (adjacent CCD locations) */
		for (lip = six; (lip + 3) < eix; lip++) {
			double rwav[4];		/* Relative wavelength of these Lagrange points */
			double den[4];		/* Denominator values for points */
			double num[4][4];	/* Numerator polinomial components x^3, x^2, x, 1 */
			double ilow, ihigh;	/* Integration points */
		
			/* Relative wavelengths to owl of each basis point */
			for (i = 0; i < 4; i++)
				rwav[i] = i1pro_raw2wav(p, refl, (double)lip + i) - owl;
//			printf("\n~1 rwav = %f %f %f %f\n", rwav[0], rwav[1], rwav[2], rwav[3]);

			/* Compute each basis points Lagrange denominator values */
			den[0] = (rwav[0]-rwav[1]) * (rwav[0]-rwav[2]) * (rwav[0]-rwav[3]);
			den[1] = (rwav[1]-rwav[0]) * (rwav[1]-rwav[2]) * (rwav[1]-rwav[3]);
			den[2] = (rwav[2]-rwav[0]) * (rwav[2]-rwav[1]) * (rwav[2]-rwav[3]);
			den[3] = (rwav[3]-rwav[0]) * (rwav[3]-rwav[1]) * (rwav[3]-rwav[2]);
//			printf("~1 denominators = %f %f %f %f\n", den[0], den[1], den[2], den[3]);
	
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

//			printf("~1 num %d = %f %f %f %f\n", 0, num[0][0], num[0][1], num[0][2], num[0][3]);
//			printf("~1 num %d = %f %f %f %f\n", 1, num[1][0], num[1][1], num[1][2], num[1][3]);
//			printf("~1 num %d = %f %f %f %f\n", 2, num[2][0], num[2][1], num[2][2], num[2][3]);
//			printf("~1 num %d = %f %f %f %f\n", 3, num[3][0], num[3][1], num[3][2], num[3][3]);

			/* Now compute the integral difference between the two middle points */
			/* of the Lagrange over the triangle shape, and accumulate the resulting */
			/* Lagrange weightings to the filter coefficients. */

			/* For high and then low side of the triangle. */
			for (k = 0; k < 2; k++) {

				ihigh = rwav[1]; 
				ilow = rwav[2]; 

				if ((k == 0 && ilow <= twidth && ihigh >= 0.0)		/* Portion is +ve side */
				 || (k == 1 && ilow <= 0.0 && ihigh >= -twidth)) {	/* Portion is -ve side */

					if (k == 0) {
						if (ilow < 0.0)
							ilow = 0.0;
						if (ihigh > twidth)
							ihigh = twidth;
//						printf("~1 doing +ve triangle between %f %f\n",ilow,ihigh);
					} else {
						if (ilow < -twidth)
							ilow = -twidth;
						if (ihigh > 0.0)
							ihigh = 0.0;
//						printf("~1 doing -ve triangle between %f %f\n",ilow,ihigh);
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
//						printf("~1 k = %d, comp %d weight += %e now %e\n",k,lip-six+i,(nvalh - nvall)/den[i], wlcop[lip-six+i]);
					}
				}
			}
		}
//		printf("~1 Weightings for for %.1f nm are:\n",owl);
//		for (i = 0; i < m->mtx_c[hr][refl].nocoef[wlix]; i++)
//			printf("~1 comp %d weight %e\n",i,wlcop[i]);

		wlcop += m->mtx_c[hr][refl].nocoef[wlix];		/* Next group of weightings */
	}
#ifdef DEBUG 
	/* Check against orginal filters */
	if (!hr) {
		int ix1, ix1c;
		double aerr = 0.0;

		a1logd(p->log,2,"Checking gemertated tables against EEProm table\n"); 
		ix1 = ix1c = 0;
		for (i = 0; i < m->nwav[0]; i++) {
			double err;
			int six, eix;

			if (m->mtx_o.index[i] < m->mtx_o.index[i])
				six = m->mtx_o.index[i];
			else
				six = m->mtx_o.index[i];

			if ((m->mtx_o.index[i] + m->mtx_o.nocoef[i]) > (m->mtx_o.index[i] + m->mtx_o.nocoef[i]))
				eix = m->mtx_o.index[i] + m->mtx_o.nocoef[i];
			else
				eix = m->mtx_o.index[i] + m->mtx_o.nocoef[i];
//			printf("~1 filter %d from %d to %d\n",i,six,eix);

			err = 0.0;
			for (j = six; j < eix; j++) {
		 		double w1, w1c;	

				if (j < m->mtx_o.index[i] || j >= (m->mtx_o.index[i] + m->mtx_o.nocoef[i]))
					w1 = 0.0;
				else
					w1 = m->mtx_o.coef[ix1 + j - m->mtx_o.index[i]];
				if (j < m->mtx_c[0][refl].index[i]
				 || j >= (m->mtx_c[0][refl].index[i] + m->mtx_c[0][refl].nocoef[i]))
					w1c = 0.0;
				else
					w1c = m->mtx_c[0][refl].coef[ix1c + j - m->mtx_c[0][refl].index[i]];

				err += fabs(w1 - w1c);
//				printf("Weight %d, %e should be %e\n", j, w1c, w1);
			}
//			printf("Filter %d average weighting error = %f\n",i, err/j);
			aerr += err/j;

			ix1 += m->mtx_o.nocoef[i];
			ix1c += m->mtx_c[0][refl].nocoef[i];
		}
		a1logd(p->log,2,"Overall average filter weighting change = %f\n",aerr/m->nwav[0]);
	}
#endif /* DEBUG */

	/* Switch normal res. to use wavelength calibrated version */
	m->mtx[hr][refl] = m->mtx_c[hr][refl];

	return ev;
}
	

/* =============================================== */
#ifdef HIGH_RES

/*
	It turns out that using the sharpest possible resampling filter
	may make accuracy worse (particularly on the RevE), because it
	enhances bumps in the raw response that mightn't be there
	after calibrating for the instrument spectral sensitivity.
	A better scheme (which we could sythesise using the hi-res
	emissive calibration logic) would be to calibrate the raw CCD
	values and then resample with possible sharpening.
	Another approach would be to sharpen after filtering with
	non-sharpening resampling filters.
	The bottom line is that it's best to use a gausian hi-res
	filter to avoid sharpening in non calibrated spectral space. 
 */

/* High res congiguration */
/* Pick one of these: */
#undef USE_TRI_LAGRANGE		/* [und] Use normal res filter shape */
#undef USE_LANCZOS2			/* [und] Use lanczos2 filter shape */
#undef USE_LANCZOS3			/* [und] Use lanczos3 filter shape */
#undef USE_DECONV			/* [und] Use deconvolution curve */
#undef USE_BLACKMAN			/* [und] Use Blackman windowed sinc shape */
#define USE_GAUSSIAN		/* [def] Use gaussian filter shape*/
#undef USE_CUBIC			/* [und] Use cubic spline filter */

#define DO_CCDNORM			/* [def] Normalise CCD values to original */
#define DO_CCDNORMAVG		/* [und] Normalise averages rather than per CCD bin */
							/* (We relly on fine cal & white cal to fix it) */

#undef COMPUTE_DISPERSION	/* Compute slit & optics dispersion from red laser data */

#ifdef NEVER
/* Plot the matrix coefficients */
static void i1pro_debug_plot_mtx_coef(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k, cx, sx;
	double *xx, *ss;
	double **yy;

	xx = dvectorz(-1, m->nraw-1);		/* X index */
	yy = dmatrixz(0, 5, -1, m->nraw-1);	/* Curves distributed amongst 5 graphs */
	
	for (i = 0; i < m->nraw; i++)
		xx[i] = i;

	/* For each output wavelength */
	for (cx = j = 0; j < m->nwav; j++) {
		i = j % 5;

//		printf("Out wave = %d\n",j);
		/* For each matrix value */
		sx = m->mtx_index[j];		/* Starting index */
//		printf("start index = %d, nocoef %d\n",sx,m->mtx_nocoef[j]);
		for (k = 0; k < m->mtx_nocoef[j]; k++, cx++, sx++) {
//			printf("offset %d, coef ix %d val %f from ccd %d\n",k, cx, m->mtx_coef[cx], sx);
			yy[5][sx] += 0.5 * m->mtx_coef[cx];
			yy[i][sx] = m->mtx_coef[cx];
		}
	}

	do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
	free_dvector(xx, -1, m->nraw-1);
	free_dmatrix(yy, 0, 2, -1, m->nraw-1);
}
#endif	/* NEVER */

#ifdef COMPUTE_DISPERSION

/* Gausian filter implementation */
/* parameters are amplidude [0], center wavelength [1], std. dev. [2] */
static double gaussf(double tp[], double x) {
	double y;

	x = (x - tp[1])/(sqrt(2.0) * tp[2]);
	y = tp[0] * exp(-(x * x));

	return y;
}

/* Gausian integral implementatation */
/* parameters are amplidude [0], center wavelength [1], std. dev. [2] */
/* return an aproximation to the intergral between w1 and w2 */
static double gaussint(double tp[], double w1, double w2) {
	int j, nn;
	double lw, ll, vv;

	/* Intergate in 0.1 nm increments */
	nn = (int)(fabs(w2 - w1)/0.1 + 0.5);

	lw = w1;
	ll = gaussf(tp, lw);
	vv = 0.0;
	for (j = 0; j < nn; j++) { 
		double cw, cl;
		cw = w1 + (j+1)/(nn +1.0) * (w2 - w1);
		cl = gaussf(tp, cw);
		vv += 0.5 * (cl + ll) * (lw - cw);
		ll = cl;
		lw = cw;
	}
	return fabs(vv);
}

/* Powell minimisation context */
typedef struct {
	double nsp;			/* Number of samples of dispersion data */
	double *llv;		/* [nsamp] laser values */
	double *lwl;		/* [nsamp+1] CCD boundary wavelegths */
} hropt_cx;

/* Powell minimisation callback function */
/* to match dispersion data */
static double hropt_opt1(void *vcx, double tp[]) {
	hropt_cx *cx = (hropt_cx *)vcx;
	double rv = 0.0;
	int i, j;

	/* For each CCD sample */
	for (i = 0; i < cx->nsp; i++) {
		double vv;

		/* Actual CCD integrated value */
		vv = cx->llv[i] * (cx->lwl[i] - cx->lwl[i+1]);
		/* Computed intergral with current curve */
		vv -= gaussint(tp, cx->lwl[i], cx->lwl[i+1]);
		rv += vv * vv;
	}
//	printf("~1 params %f %f %f, rv = %f\n", tp[0],tp[1],tp[2],rv);
	return rv;
}

#endif /* COMPUTE_DISPERSION */

/* Filter shape point */
typedef	struct {
	double wl, we;
} i1pro_fs;

/* Filter cooeficient values */
typedef struct {
	int ix;				/* Raw index */
	double we;			/* Weighting */
} i1pro_fc;

/* Wavelenth calibration crossover point information */
typedef struct {
	double wav;				/* Wavelegth of point */
	double raw;				/* Raw index of point */		
	double wei;				/* Weigting of the point */
} i1pro_xp;

/* Linearly interpolate the filter shape */
static double lin_fshape(i1pro_fs *fsh, int n, double x) {
	int i;
	double y;

	if (x <= fsh[0].wl)
		return fsh[0].we;
	else if (x >= fsh[n-1].wl)
		return fsh[n-1].we;

	for (i = 0; i < (n-2); i++)
		if (x >= fsh[i].wl && x <= fsh[i+1].wl)
			break;

	x = (x - fsh[i].wl)/(fsh[i+1].wl - fsh[i].wl);
	y = fsh[i].we + (fsh[i+1].we - fsh[i].we) * x;
	
	return y;
}

/* Generate a sample from a lanczos2 filter shape */
/* wi is the width of the filter */
static double lanczos2(double wi, double x) {
	double y = 0.0;

#ifdef USE_DECONV
	/* For 3.333, created by i1deconv.c */
	static i1pro_fs fshape[49] = {
		{ -7.200000, 0.0 },
		{ -6.900000, 0.013546 },
		{ -6.600000, 0.035563 },
		{ -6.300000, 0.070500 },
		{ -6.000000, 0.106543 },
		{ -5.700000, 0.148088 },
		{ -5.400000, 0.180888 },
		{ -5.100000, 0.186637 },
		{ -4.800000, 0.141795 },
		{ -4.500000, 0.046101 },
		{ -4.200000, -0.089335 },
		{ -3.900000, -0.244652 },
		{ -3.600000, -0.391910 },
		{ -3.300000, -0.510480 },
		{ -3.000000, -0.573177 },
		{ -2.700000, -0.569256 },
		{ -2.400000, -0.489404 },
		{ -2.100000, -0.333957 },
		{ -1.800000, -0.116832 },
		{ -1.500000, 0.142177 },
		{ -1.200000, 0.411639 },
		{ -0.900000, 0.658382 },
		{ -0.600000, 0.851521 },
		{ -0.300000, 0.967139 },
		{ 0.000000, 1.000000 },
		{ 0.300000, 0.967139 },
		{ 0.600000, 0.851521 },
		{ 0.900000, 0.658382 },
		{ 1.200000, 0.411639 },
		{ 1.500000, 0.142177 },
		{ 1.800000, -0.116832 },
		{ 2.100000, -0.333957 },
		{ 2.400000, -0.489404 },
		{ 2.700000, -0.569256 },
		{ 3.000000, -0.573177 },
		{ 3.300000, -0.510480 },
		{ 3.600000, -0.391910 },
		{ 3.900000, -0.244652 },
		{ 4.200000, -0.089335 },
		{ 4.500000, 0.046101 },
		{ 4.800000, 0.141795 },
		{ 5.100000, 0.186637 },
		{ 5.400000, 0.180888 },
		{ 5.700000, 0.148088 },
		{ 6.000000, 0.106543 },
		{ 6.300000, 0.070500 },
		{ 6.600000, 0.035563 },
		{ 6.900000, 0.013546 },
		{ 7.200000, 0.0 }
	};

	return lin_fshape(fshape, 49, x);
#endif

#ifdef USE_GAUSSIAN
	/* gausian */
	wi = wi/(2.0 * sqrt(2.0 * log(2.0)));	/* Convert width at half max to std. dev. */
    x = x/(sqrt(2.0) * wi);
//    y = 1.0/(wi * sqrt(2.0 * DBL_PI)) * exp(-(x * x));		/* Unity area */
    y = exp(-(x * x));											/* Center at 1.0 */
#endif

#ifdef USE_LANCZOS2
	/* lanczos2 */
	x = fabs(1.0 * x/wi);
	if (x >= 2.0)
		return 0.0; 
	if (x < 1e-6)
		return 1.0;
	y = sin(DBL_PI * x)/(DBL_PI * x) * sin(DBL_PI * x/2.0)/(DBL_PI * x/2.0);
#endif

#ifdef USE_LANCZOS3
	/* lanczos3 */
	x = fabs(1.0 * x/wi);
	if (x >= 3.0)
		return 0.0; 
	if (x < 1e-6)
		return 1.0;
	y = sin(DBL_PI * x)/(DBL_PI * x) * sin(DBL_PI * x/3.0)/(DBL_PI * x/3.0);
#endif

#ifdef USE_BLACKMAN		/* Use Blackman windowed sinc shape */
	double xx = x, w;
	double a0, a1, a2, a3;
	double bb, cc;

	xx = fabs(1.0 * x/wi);
	if (xx >= 2.0)
		return 0.0; 
	if (xx < 1e-5)
		return 1.0;
	y = sin(DBL_PI * xx)/(DBL_PI * xx);	/* sinc */

	/* gausian window */
//	wi *= 1.5;
//	wi = wi/(2.0 * sqrt(2.0 * log(2.0)));	/* Convert width at half max to std. dev. */
//	x = x/(sqrt(2.0) * wi);
//	w = exp(-(x * x));

	xx = (xx/4.0 + 0.5);		/* Convert to standard window cos() range */

	/* Hamming window */
//	a0 = 0.54; a1 = 0.46;
//	w = a0 - a1 * cos(2.0 * DBL_PI * xx);

	/* Blackman window */
	a0 = 7938.0/18608.0; a1 = 9240.0/18608.0; a2 = 1430.0/18608.0;
	w = a0 - a1 * cos(2.0 * DBL_PI * xx) + a2 * cos(4.0 * DBL_PI * xx);

	/* Nuttall window */
//	a0 = 0.355768; a1=0.487396; a2=0.144232; a3=0.012604;
//	w = a0 - a1 * cos(2.0 * DBL_PI * xx) + a2 * cos(4.0 * DBL_PI * xx) - a3 * cos(6.0 * DBL_PI * xx);

	/* Blackman Harris window */
//	a0=0.35875; a1=0.48829; a2=0.14128; a3=0.01168;
//	w = a0 - a1 * cos(2.0 * DBL_PI * xx) + a2 * cos(4.0 * DBL_PI * xx) - a3 * cos(6.0 * DBL_PI * xx);

	/* Blackman Nuttall window */
//	a0=0.3635819; a1=0.4891775; a2=0.1365995; a3=0.0106411;
//	w = a0 - a1 * cos(2.0 * DBL_PI * xx) + a2 * cos(4.0 * DBL_PI * xx) - a3 * cos(6.0 * DBL_PI * xx);

	y *= w;
#endif
#ifdef USE_CUBIC		/* Use cubic sline */
	double xx = x;
	double bb, cc;

	xx = fabs(1.0 * x/wi);

//	bb = cc = 1.0/3.0;		/* Mitchell */
	bb = 0.5;
	cc = 0.5;
	xx *= 1.2;

	if (xx < 1.0) {
		y = ( 12.0 -  9.0 * bb - 6.0 * cc) * xx * xx * xx
		  + (-18.0 + 12.0 * bb + 6.0 * cc) * xx * xx
		  + (  6.0 -  2.0 * bb);
		y /= (6.0 - 2.0 * bb);
	} else if (xx < 2.0) {
		y = ( -1.0 * bb -  6.0 * cc) * xx * xx * xx
		  + (  6.0 * bb + 30.0 * cc) * xx * xx
		  + (-12.0 * bb - 48.0 * cc) * xx 
		  + (  8.0 * bb + 24.0 * cc);
		y /= (6.0 - 2.0 * bb);
	} else {
		y = 0.0;
	}
#endif
	return y;
}

#if defined(__APPLE__) && defined(__POWERPC__)

/* Workaround for a ppc gcc 3.3 optimiser bug... */
static int gcc_bug_fix(int i) {
	static int nn;
	nn += i;
	return nn;
}
#endif	/* APPLE */

/* Re-create calibration factors for hi-res */
/* Set emisonly to only recompute emissive factors */
i1pro_code i1pro_create_hr_calfactors(i1pro *p, int eonly) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code ev = I1PRO_OK;
	int i, j;

	/* Generate high res. per mode calibration factors. */
	if (m->hr_inited) {

		for (i = 0; i < i1p_no_modes; i++) {
			i1pro_state *s = &m->ms[i];

			if (s->cal_factor[1] == NULL)
				s->cal_factor[1] = dvectorz(0, m->nwav[1]-1);

			switch(i) {
				case i1p_refl_spot:
				case i1p_refl_scan:
					if (eonly)
						continue;
					if (s->cal_valid) {
						/* (Using cal_factor[] as temp. for i1pro_absraw_to_abswav()) */
#ifdef NEVER
						printf("~1 regenerating calibration for reflection\n");
						printf("~1 raw white data:\n");
						plot_raw(s->white_data);
#endif	/* NEVER */
						i1pro_absraw_to_abswav(p, 0, s->reflective, 1, &s->cal_factor[0], &s->white_data);
#ifdef NEVER
						printf("~1 Std res intmd. cal_factor:\n");
						plot_wav(m, 0, s->cal_factor[0]);
#endif	/* NEVER */
						i1pro_absraw_to_abswav(p, 1, s->reflective, 1, &s->cal_factor[1], &s->white_data);
#ifdef NEVER
						printf("~1 High intmd. cal_factor:\n");
						plot_wav(m, 1, s->cal_factor[1]);
						printf("~1 Std res white ref:\n");
						plot_wav(m, 0, m->white_ref[0]);
						printf("~1 High res white ref:\n");
						plot_wav(m, 1, m->white_ref[1]);
#endif	/* NEVER */
						ev = i1pro_compute_white_cal(p,
						                   s->cal_factor[0], m->white_ref[0], s->cal_factor[0],
						                   s->cal_factor[1], m->white_ref[1], s->cal_factor[1],
						                   i == i1p_refl_spot);
						if (ev == I1PRO_RD_TRANSWHITEWARN)		/* Shouldn't happen ? */
							ev = I1PRO_OK;
						if (ev != I1PRO_OK) {
							return ev;
						}
#ifdef NEVER
						printf("~1 Std res final cal_factor:\n");
						plot_wav(m, 0, s->cal_factor[0]);
						printf("~1 High final cal_factor:\n");
						plot_wav(m, 1, s->cal_factor[1]);
#endif	/* NEVER */
					}
					break;

				case i1p_emiss_spot_na:
				case i1p_emiss_spot:
				case i1p_emiss_scan:
					for (j = 0; j < m->nwav[1]; j++)
						s->cal_factor[1][j] = EMIS_SCALE_FACTOR * m->emis_coef[1][j];
					break;

				case i1p_amb_spot:
				case i1p_amb_flash:
#ifdef FAKE_AMBIENT
					for (j = 0; j < m->nwav[1]; j++)
						s->cal_factor[1][j] = EMIS_SCALE_FACTOR * m->emis_coef[1][j];
					s->cal_valid = 1;
#else

					if (m->amb_coef[0] != NULL) {
						for (j = 0; j < m->nwav[1]; j++)
							s->cal_factor[1][j] = AMB_SCALE_FACTOR * m->emis_coef[1][j] * m->amb_coef[1][j];
						s->cal_valid = 1;
					}
#endif
					break;
				case i1p_trans_spot:
				case i1p_trans_scan:
					if (eonly)
						continue;
					if (s->cal_valid) {
						/* (Using cal_factor[] as temp. for i1pro_absraw_to_abswav()) */
						i1pro_absraw_to_abswav(p, 0, s->reflective, 1, &s->cal_factor[0], &s->white_data);
						i1pro_absraw_to_abswav(p, 1, s->reflective, 1, &s->cal_factor[1], &s->white_data);
						ev = i1pro_compute_white_cal(p, s->cal_factor[0], NULL, s->cal_factor[0],
			                                            s->cal_factor[1], NULL, s->cal_factor[1], 0);
						if (ev == I1PRO_RD_TRANSWHITEWARN)		/* Ignore this ? */
							ev = I1PRO_OK;
						if (ev != I1PRO_OK) {
							return ev;
						}
					}
					break;
			}
		}
	}
	return ev;
}

#ifdef SALONEINSTLIB
# define ONEDSTRAYLIGHTUS
#endif

/* Create or re-create high resolution mode references */
i1pro_code i1pro_create_hr(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code ev = I1PRO_OK;
	int refl;
	double twidth = HIGHRES_WIDTH;
	int i, j, k, cx, sx;
	
	/* If we don't have any way of converting raw2wav (ie. RevE polinomial equations), */
	/* use the orginal filters to figure this out. */
	if (m->raw2wav == NULL
#ifndef ANALIZE_EXISTING
	 && p->itype != instI1Pro2
#endif
	) {
		i1pro_fc coeff[100][16];	/* Existing filter cooefficients */
		i1pro_xp xp[101];			/* Crossover points each side of filter */
		i1pro_fs fshape[100 * 16];  /* Existing filter shape */
		int ncp = 0;				/* Number of shape points */

		/* Convert the native filter cooeficient representation to */
		/* a 2D array we can randomly index. */
		for (cx = j = 0; j < m->nwav[0]; j++) { /* For each output wavelength */
			if (j >= 100) {	/* Assert */
				a1loge(p->log,1,"i1pro: number of output wavelenths is > 100\n");
				return I1PRO_INT_ASSERT;
			}

			/* For each matrix value */
			sx = m->mtx_o.index[j];		/* Starting index */
			for (k = 0; k < m->mtx_o.nocoef[j]; k++, cx++, sx++) {
				if (k >= 16) {	/* Assert */
					a1loge(p->log,1,"i1pro: number of filter coeefs is > 16\n");
					return I1PRO_INT_ASSERT;
				}

				coeff[j][k].ix = sx;
				coeff[j][k].we = m->mtx_o.coef[cx];
//				printf("Output %d, filter %d weight = %e\n",j,k,coeff[j][k].we);
			}
		}

#ifdef HIGH_RES_PLOT
		/* Plot original re-sampling curves */
		{
			double *xx, *ss;
			double **yy;

			xx = dvectorz(-1, m->nraw-1);		/* X index */
			yy = dmatrixz(0, 5, -1, m->nraw-1);	/* Curves distributed amongst 5 graphs */
			
			for (i = 0; i < m->nraw; i++)
				xx[i] = i;

			/* For each output wavelength */
			for (j = 0; j < m->nwav[0]; j++) {
				i = j % 5;

				/* For each matrix value */
				for (k = 0; k < m->mtx_o.nocoef[j]; k++) {
					yy[5][coeff[j][k].ix] += 0.5 * coeff[j][k].we;
					yy[i][coeff[j][k].ix] = coeff[j][k].we;
				}
			}

			printf("Original wavelength sampling curves:\n");
			do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
			free_dvector(xx, -1, m->nraw-1);
			free_dmatrix(yy, 0, 2, -1, m->nraw-1);
		}
#endif /* HIGH_RES_PLOT */

//		a1logd(p->log,3,"computing crossover points\n");
		/* Compute the crossover points between each filter */
		for (i = 0; i < (m->nwav[0]-1); i++) {
			double den, y1, y2, y3, y4, yn, xn;	/* Location of intersection */
			double eps = 1e-6;			/* Numerical tollerance */
			double besty = -1e6;

			/* between filter i and i+1, we want to find the two */
			/* raw indexes where the weighting values cross over */
			/* Do a brute force search to avoid making assumptions */
			/* about the raw order. */
			for (j = 0; j < (m->mtx_o.nocoef[i]-1); j++) {
				for (k = 0; k < (m->mtx_o.nocoef[i+1]-1); k++) {
					if (coeff[i][j].ix == coeff[i+1][k].ix
					 && coeff[i][j+1].ix == coeff[i+1][k+1].ix) {

//						a1logd(p->log,3,"got it at %d, %d: %d = %d, %d = %d\n",j,k, coeff[i][j].ix, coeff[i+1][k].ix, coeff[i][j+1].ix, coeff[i+1][k+1].ix);

						/* Compute the intersection of the two line segments */
						y1 = coeff[i][j].we;
						y2 = coeff[i][j+1].we;
						y3 = coeff[i+1][k].we;
						y4 = coeff[i+1][k+1].we;
//						a1logd(p->log,3,"y1 %f, y2 %f, y3 %f, y4 %f\n",y1, y2, y3, y4);
						den = -y4 + y3 + y2 - y1;
						if (fabs(den) < eps)
							continue; 
						yn = (y2 * y3 - y1 * y4)/den;
						xn = (y3 - y1)/den;
						if (xn < -eps || xn > (1.0 + eps))
							continue;
//						a1logd(p->log,3,"den = %f, yn = %f, xn = %f\n",den,yn,xn);
						if (yn > besty) {
							xp[i+1].wav = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], i + 0.5);
							xp[i+1].raw = (1.0 - xn) * coeff[i][j].ix + xn * coeff[i][j+1].ix;
							xp[i+1].wei = yn;
							besty = yn;
//						a1logd(p->log,3,"Intersection %d: wav %f, raw %f, wei %f\n",i+1,xp[i+1].wav,xp[i+1].raw,xp[i+1].wei);
//						a1logd(p->log,3,"Found new best y %f\n",yn);
						}
//					a1logd(p->log,3,"\n");
					}
				}
			}
			if (besty < 0.0) {	/* Assert */
				a1logw(p->log,"i1pro: failed to locate crossover between resampling filters\n");
				return I1PRO_INT_ASSERT;
			}
//			a1logd(p->log,3,"\n");
		}

		/* Add the two points for the end filters */
		{
			double x5, x6, y5, y6;				/* Points on intesecting line */
			double den, y1, y2, y3, y4, yn, xn;	/* Location of intersection */

			x5 = xp[1].raw;
			y5 = xp[1].wei;
			x6 = xp[2].raw;
			y6 = xp[2].wei;

			/* Search for possible intersection point with first curve */
			/* Create equation for line from next two intersection points */
			for (j = 0; j < (m->mtx_o.nocoef[0]-1); j++) {
				/* Extrapolate line to this segment */
				y3 = y5 + (coeff[0][j].ix - x5)/(x6 - x5) * (y6 - y5);
				y4 = y5 + (coeff[0][j+1].ix - x5)/(x6 - x5) * (y6 - y5);
				/* This segment of curve */
				y1 = coeff[0][j].we;
				y2 = coeff[0][j+1].we;
				if ( ((  y1 >= y3 && y2 <= y4)		/* Segments overlap */
				   || (  y1 <= y3 && y2 >= y4))
				  && ((    coeff[0][j].ix < x5 && coeff[0][j].ix < x6
				        && coeff[0][j+1].ix < x5 && coeff[0][j+1].ix < x6)
				   || (    coeff[0][j+1].ix > x5 && coeff[0][j+1].ix > x6
					    && coeff[0][j].ix > x5 && coeff[0][j].ix > x6))) {
					break;
				}
			}
			if (j >= m->mtx_o.nocoef[0]) {	/* Assert */
				a1loge(p->log,1,"i1pro: failed to find end crossover\n");
				return I1PRO_INT_ASSERT;
			}
	
			den = -y4 + y3 + y2 - y1;
			yn = (y2 * y3 - y1 * y4)/den;
			xn = (y3 - y1)/den;
//			printf("~1 den = %f, yn = %f, xn = %f\n",den,yn,xn);
			xp[0].wav = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], -0.5);
			xp[0].raw = (1.0 - xn) * coeff[0][j].ix + xn * coeff[0][j+1].ix;
			xp[0].wei = yn;
//			printf("End 0 intersection %d: wav %f, raw %f, wei %f\n",0,xp[0].wav,xp[0].raw,xp[0].wei);
//			printf("\n");

			x5 = xp[m->nwav[0]-2].raw;
			y5 = xp[m->nwav[0]-2].wei;
			x6 = xp[m->nwav[0]-1].raw;
			y6 = xp[m->nwav[0]-1].wei;

//			printf("~1 x5 %f, y5 %f, x6 %f, y6 %f\n",x5,y5,x6,y6);
			/* Search for possible intersection point with first curve */
			/* Create equation for line from next two intersection points */
			for (j = 0; j < (m->mtx_o.nocoef[0]-1); j++) {
				/* Extrapolate line to this segment */
				y3 = y5 + (coeff[m->nwav[0]-1][j].ix - x5)/(x6 - x5) * (y6 - y5);
				y4 = y5 + (coeff[m->nwav[0]-1][j+1].ix - x5)/(x6 - x5) * (y6 - y5);
				/* This segment of curve */
				y1 = coeff[m->nwav[0]-1][j].we;
				y2 = coeff[m->nwav[0]-1][j+1].we;
				if ( ((  y1 >= y3 && y2 <= y4)		/* Segments overlap */
				   || (  y1 <= y3 && y2 >= y4))
				  && ((    coeff[m->nwav[0]-1][j].ix < x5 && coeff[m->nwav[0]-1][j].ix < x6
				        && coeff[m->nwav[0]-1][j+1].ix < x5 && coeff[m->nwav[0]-1][j+1].ix < x6)
				   || (    coeff[m->nwav[0]-1][j+1].ix > x5 && coeff[m->nwav[0]-1][j+1].ix > x6
					    && coeff[m->nwav[0]-1][j].ix > x5 && coeff[m->nwav[0]-1][j].ix > x6))) {
					break;
				}
			}
			if (j >= m->mtx_o.nocoef[m->nwav[0]-1]) {	/* Assert */
				a1loge(p->log,1,"i1pro: failed to find end crossover\n");
				return I1PRO_INT_ASSERT;
			}
			den = -y4 + y3 + y2 - y1;
			yn = (y2 * y3 - y1 * y4)/den;
			xn = (y3 - y1)/den;
//			printf("~1 den = %f, yn = %f, xn = %f\n",den,yn,xn);
			xp[m->nwav[0]].wav = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], m->nwav[0]-0.5);
			xp[m->nwav[0]].raw = (1.0 - xn) * coeff[m->nwav[0]-1][j].ix + xn * coeff[m->nwav[0]-1][j+1].ix;
			xp[m->nwav[0]].wei = yn;
//			printf("End 36 intersection %d: wav %f, raw %f, wei %f\n",m->nwav[0]+1,xp[m->nwav[0]].wav,xp[m->nwav[0]].raw,xp[m->nwav[0]].wei);
//			printf("\n");
		}

#ifdef HIGH_RES_DEBUG
		/* Check to see if the area of each filter curve is the same */
		/* (yep, width times 2 * xover height is close to 1.0, and the */
		/*  sum of the weightings is exactly 1.0) */
		for (i = 0; i < m->nwav[0]; i++) {
			double area1, area2;
			area1 = fabs(xp[i].raw - xp[i+1].raw) * (xp[i].wei + xp[i+1].wei);

			area2 = 0.0;
			for (j = 0; j < (m->mtx_o.nocoef[i]); j++)
			    area2 += coeff[i][j].we;

		printf("Area of curve %d = %f, %f\n",i,area1, area2);
		}
#endif /* HIGH_RES_DEBUG */

		/* From our crossover data, create a rspl that maps raw CCD index */
		/* value into wavelegth. */
		{
			co sd[101];				/* Scattered data points */
			datai glow, ghigh;
			datao vlow, vhigh;
			int gres[1];
			double avgdev[1];

			if ((m->raw2wav = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
				a1logd(p->log,1,"i1pro: creating rspl for high res conversion failed\n");
				return I1PRO_INT_NEW_RSPL_FAILED;
			}

			vlow[0] = 1e6;
			vhigh[0] = -1e6;
			for (i = 0; i < (m->nwav[0]+1); i++) {
				sd[i].p[0] = xp[i].raw;
				sd[i].v[0] = xp[i].wav;
				
				if (sd[i].v[0] < vlow[0])
					vlow[0] = sd[i].v[0];
				if (sd[i].v[0] > vhigh[0])
					vhigh[0] = sd[i].v[0];
			}
			glow[0] = 0.0;
			ghigh[0] = 127.0;
			gres[0] = 128;
			avgdev[0] = 0.0;
			
			m->raw2wav->fit_rspl(m->raw2wav, 0, sd, m->nwav[0]+1, glow, ghigh, gres, vlow, vhigh, 0.5, avgdev, NULL);
		}

#ifdef HIGH_RES_PLOT
		/* Plot raw to wav lookup */
		{
			double *xx, *yy, *y2;

			xx = dvector(0, m->nwav[0]+1);		/* X index = raw bin */
			yy = dvector(0, m->nwav[0]+1);		/* Y = nm */
			y2 = dvector(0, m->nwav[0]+1);		/* Y = nm */
			
			for (i = 0; i < (m->nwav[0]+1); i++) {
				co pp;
				double iv, v1, v2;
				xx[i] = xp[i].raw;
				yy[i] = xp[i].wav;

				pp.p[0] = xp[i].raw;
				m->raw2wav->interp(m->raw2wav, &pp);
				y2[i] = pp.v[0];
			}

			printf("CCD bin to wavelength mapping of original filters + rspl:\n");
			do_plot6(xx, yy, y2, NULL, NULL, NULL, NULL, m->nwav[0]+1);
			free_dvector(xx, 0, m->nwav[0]+1);
			free_dvector(yy, 0, m->nwav[0]+1);
			free_dvector(y2, 0, m->nwav[0]+1);
		}
#endif /* HIGH_RES_PLOT */

#ifdef ANALIZE_EXISTING
		/* Convert each weighting curves values into normalized values and */
		/* accumulate into a single curve. */
		if (!m->hr_inited) {
			for (i = 0; i < m->nwav[0]; i++) {
				double cwl;		/* center wavelegth */
				double weight = 0.0;

				for (j = 0; j < (m->mtx_o.nocoef[i]); j++) {
					double w1, w2, cellw;

					/* Translate CCD cell boundaries index to wavelength */
					w1 = i1pro_raw2wav_uncal(p, (double)coeff[i][j].ix - 0.5);

					w2 = i1pro_raw2wav_uncal(p, (double)coeff[i][j].ix + 0.5);

					cellw = fabs(w2 - w1);

					cwl = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], i);

					/* Translate CCD index to wavelength */
					fshape[ncp].wl = i1pro_raw2wav_uncal(p, (double)coeff[i][j].ix) - cwl;
					fshape[ncp].we = coeff[i][j].we / (0.09 * cellw);
					ncp++;
				}
			}

			/* Now sort by wavelength */
#define HEAP_COMPARE(A,B) (A.wl < B.wl)
			HEAPSORT(i1pro_fs, fshape, ncp)
#undef HEAP_COMPARE

			/* Strip out leading zero's */
			for (i = 0; i < ncp; i++) {
				if (fshape[i].we != 0.0)
					break;
			}
			if (i > 1 && i < ncp) {
				memmove(&fshape[0], &fshape[i-1], sizeof(i1pro_fs) * (ncp - i + 1));
				ncp = ncp - i + 1;
				for (i = 0; i < ncp; i++) {
					if (fshape[i].we != 0.0)
						break;
				}
			}

#ifdef HIGH_RES_PLOT
			/* Plot the shape of the accumulated curve */
			{
				double *x1 = dvectorz(0, ncp-1);
				double *y1 = dvectorz(0, ncp-1);

				for (i = 0; i < ncp; i++) {
					double x;
					x1[i] = fshape[i].wl;
					y1[i] = fshape[i].we;
				}
				printf("Original accumulated curve:\n");
				do_plot(x1, y1, NULL, NULL, ncp);

				free_dvector(x1, 0, ncp-1);
				free_dvector(y1, 0, ncp-1);
			}
#endif /* HIGH_RES_PLOT */

#ifdef HIGH_RES_DEBUG
			/* Check that the orginal filter sums to a constant */
			{
				double x, sum;

				for (x = 0.0; x < 10.0; x += 0.2) {
					sum = 0;
					sum += lin_fshape(fshape, ncp, x - 30.0);
					sum += lin_fshape(fshape, ncp, x - 20.0);
					sum += lin_fshape(fshape, ncp, x - 10.0);
					sum += lin_fshape(fshape, ncp, x -  0.0);
					sum += lin_fshape(fshape, ncp, x + 10.0);
					sum += lin_fshape(fshape, ncp, x + 20.0);
					printf("Offset %f, sum %f\n",x, sum);
				}
			}
#endif /* HIGH_RES_DEBUG */
		}
#endif /* ANALIZE_EXISTING */
	}	/* End of compute wavelength cal from existing filters */

#ifdef COMPUTE_DISPERSION
	if (!m->hr_inited) {
		/* Fit our red laser CCD data to a slit & optics Gaussian dispersion model */
		{
			double spf[3];				/* Spread function parameters */

			/* Measured CCD values of red laser from CCD indexes 29 to 48 inclusive */
			/* (It would be nice to have similar data from a monochromic source */
			/*  at other wavelegths such as green and blue!) */
			double llv[20] = {
				53.23,
				81.3,
				116.15,
				176.16,
				305.87,
				613.71,
				8500.52,
				64052.0,
				103134.13,
				89154.03,
				21742.89,
				1158.86,
				591.44,
				369.75,
				241.01,
				166.48,
				126.79,
				97.76,
				63.88,
				46.46
			};
			double lwl[21];		/* Wavelegth of boundary between CCD cells */ 
			double ccd;
			hropt_cx cx;
			double ss[3];

			/* Add CCD boundary wavelengths to dispersion data */
			for (ccd = 29.0 - 0.5, i = 0; i < 21; i++, ccd += 1.0) {
				/* Translate CCD cell boundaries index to wavelength */
				lwl[i] = i1pro_raw2wav_uncal(p, ccd);
			}

			/* Fit a gausian to it */
			cx.nsp = 20;
			cx.llv = llv;
			cx.lwl = lwl;

			/* parameters are amplidude [0], center wavelength [1], std. dev. [2] */
			spf[0] = 115248.0;
			spf[1] = 653.78;
			spf[2] = 3.480308;
			ss[0] = 500.0;
			ss[1] = 0.5;
			ss[2] = 0.5;

			if (powell(NULL, 3, spf, ss, 1e-5, 2000, hropt_opt1, &cx))
				a1logw(p->log,"hropt_opt1 failed\n");

#ifdef HIGH_RES_PLOT
			/* Plot dispersion spectra */
			{
				double xx[200];
				double y1[200];
				double y2[200];
				double w1, w2;

				w1 = lwl[0] + 5.0;
				w2 = lwl[20] - 5.0;
				for (i = 0; i < 200; i++) {
					double wl;
					wl = w1 + (i/199.0) * (w2-w1);
					xx[i] = wl;
					for (j = 0; j < 20; j++) {
						if (lwl[j] >= wl && wl >= lwl[j+1])
							break; 
					}
					if (j < 20)
						y1[i] = llv[j];
					else
						y1[i] = 0.0;
					y2[i] = gaussf(spf, wl);
				}
				printf("Gauss Parameters %f %f %f\n",spf[0],spf[1],spf[2]);
				printf("Red laser dispersion data:\n");
				do_plot(xx, y1, y2, NULL, 200);
			}
#endif /* HIGH_RES_PLOT */

			/* Normalize the gausian to have an area of 1 */
			spf[0] *= 1.0/(spf[0] * spf[2] * sqrt(2.0 * DBL_PI));

//			printf("~1 Normalized intergral = %f\n",gaussint(spf, spf[1] - 30.0, spf[1] + 30.0));
//			printf("~1 Half width = %f\n",2.0 * sqrt(2.0 * log(2.0)) * spf[2]);
		}
	}
#endif /* COMPUTE_DISPERSION */

	/* Compute the upsampled calibration references */
	if (!m->hr_inited) {
		rspl *trspl;			/* Upsample rspl */
		cow sd[40 * 40];		/* Scattered data points of existing references */
		datai glow, ghigh;
		datao vlow, vhigh;
		int gres[2];
		double avgdev[2];
		int ix, ii;
		co pp;

		if ((trspl = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
			a1logd(p->log,1,"i1pro: creating rspl for high res conversion failed\n");
			return I1PRO_INT_NEW_RSPL_FAILED;
		}
		
		/* For ref, emis, ambient */
		for (ii = 0; ii < 3; ii++) {
			double **ref2, *ref1;
			double smooth = 1.0;

			if (ii == 0) {
				ref1 = m->white_ref[0];
				ref2 = &m->white_ref[1];
//				smooth = 0.5;
				smooth = 1.5;
			} else if (ii == 1) {
				/* Don't create high res. from low res., if there is */
				/* a better, calibrated cal read from .cal file */
				if (m->emis_coef[1] != NULL) {
					if (m->emis_hr_cal) 
						continue;
					free(m->emis_coef[1]);		/* Regenerate it anyway */
				}
				ref1 = m->emis_coef[0];
				ref2 = &m->emis_coef[1];
				m->emis_hr_cal = 0;
				smooth = 10.0;
			} else {
				if (m->amb_coef[0] == NULL)
					break;
				ref1 = m->amb_coef[0];
				ref2 = &m->amb_coef[1];
				smooth = 1.0;
			}

			if (ref1 == NULL)
				continue;		/* The instI1Monitor doesn't have a reflective cal */

			vlow[0] = 1e6;
			vhigh[0] = -1e6;
			for (ix = i = 0; i < m->nwav[0]; i++) {

				sd[ix].p[0] = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], i);
				sd[ix].v[0] = ref1[i];
				sd[ix].w = 1.0;

				if (sd[ix].v[0] < vlow[0])
					vlow[0] = sd[ix].v[0];
				if (sd[ix].v[0] > vhigh[0])
				    vhigh[0] = sd[ix].v[0];
				ix++;
			}

			/* Our upsampling is OK for reflective and ambient cal's, */
			/* but isn't so good for the emissive cal., especially */
			/* on the i1pro2 which has a rather bumpy diffraction */
			/* grating/sensor. We'll get an opportunity to fix it */
			/* when we do a reflective calibration, by using the */
			/* smoothness of the lamp as a reference. */

			/* Add inbetween points to restrain dips and peaks in interp. */
			for (i = 0; i < (m->nwav[0]-1); i++) {

				/* Use linear interp extra points */
				double wt = 0.05;

				sd[ix].p[0] = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], i + 0.3333);
				sd[ix].v[0] = (2.0 * ref1[i] + ref1[i+1])/3.0;
				sd[ix].w = wt;

				if (sd[ix].v[0] < vlow[0])
					vlow[0] = sd[ix].v[0];
				if (sd[ix].v[0] > vhigh[0])
				    vhigh[0] = sd[ix].v[0];
				ix++;

				sd[ix].p[0] = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], i + 0.66667);
				sd[ix].v[0] = (ref1[i] + 2.0 * ref1[i+1])/3.0;
				sd[ix].w = wt;

				if (sd[ix].v[0] < vlow[0])
					vlow[0] = sd[ix].v[0];
				if (sd[ix].v[0] > vhigh[0])
				    vhigh[0] = sd[ix].v[0];
				ix++;
			}

			glow[0] = m->wl_short[1];
			ghigh[0] = m->wl_long[1];
			gres[0] = 3 * m->nwav[1];
			avgdev[0] = 0.0;
			
			trspl->fit_rspl_w(trspl, 0, sd, ix, glow, ghigh, gres, vlow, vhigh, smooth, avgdev, NULL);
			if ((*ref2 = (double *)calloc(m->nwav[1], sizeof(double))) == NULL) {
				trspl->del(trspl);
				a1logw(p->log, "i1pro: malloc ref2 failed!\n");
				return I1PRO_INT_MALLOC;
			}

			for (i = 0; i < m->nwav[1]; i++) {
				pp.p[0] = m->wl_short[1]
				        + (double)i * (m->wl_long[1] - m->wl_short[1])/(m->nwav[1]-1.0);
				trspl->interp(trspl, &pp);
				(*ref2)[i] = pp.v[0];
			}

#ifdef HIGH_RES_PLOT
			/* Plot original and upsampled reference */
			{
				double *x1 = dvectorz(0, m->nwav[1]-1);
				double *y1 = dvectorz(0, m->nwav[1]-1);
				double *y2 = dvectorz(0, m->nwav[1]-1);
	
				for (i = 0; i < m->nwav[1]; i++) {
					double wl = m->wl_short[1] + (double)i * (m->wl_long[1] - m->wl_short[1])/(m->nwav[1]-1.0);
					x1[i] = wl;
					y1[i] = wav_lerp_cv(m, 0, ref1, wl, 0.0);
					y2[i] = (*ref2)[i];
				}
				printf("Original and up-sampled ");
				if (ii == 0) {
					printf("Reflective cal. curve:\n");
				} else if (ii == 1) {
					printf("Emission cal. curve:\n");
				} else {
					printf("Ambient cal. curve:\n");
				}
				do_plot(x1, y1, y2, NULL, m->nwav[1]);
	
				free_dvector(x1, 0, m->nwav[1]-1);
				free_dvector(y1, 0, m->nwav[1]-1);
				free_dvector(y2, 0, m->nwav[1]-1);
			}
#endif /* HIGH_RES_PLOT */
		}
		trspl->del(trspl);

		/* Upsample stray light */
		if (p->itype == instI1Pro2) {
#ifdef ONEDSTRAYLIGHTUS
			double **slp;			/* 2D Array of stray light values */

			/* Then the 2D stray light using linear interpolation */
			slp = dmatrix(0, m->nwav[0]-1, 0, m->nwav[0]-1);

			/* Set scattered points */
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
#else	/* !ONEDSTRAYLIGHTUS */
			/* Then setup 2D stray light using rspl */
			if ((trspl = new_rspl(RSPL_NOFLAGS, 2, 1)) == NULL) {
				a1logd(p->log,1,"i1pro: creating rspl for high res conversion failed\n");
				return I1PRO_INT_NEW_RSPL_FAILED;
			}
			
			/* Set scattered points */
			for (i = 0; i < m->nwav[0]; i++) {		/* Output wavelength */
				for (j = 0; j < m->nwav[0]; j++) {	/* Input wavelength */
					int ix = i * m->nwav[0] + j;

					sd[ix].p[0] = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], i);
					sd[ix].p[1] = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], j);
					sd[ix].v[0] = m->straylight[0][i][j];
					sd[ix].w = 1.0;
					if (j == (i-1) || j == i || j == (i+1))
						sd[ix].w = 0.0;
				}
			}

			glow[0] = m->wl_short[1];
			glow[1] = m->wl_short[1];
			ghigh[0] = m->wl_long[1];
			ghigh[1] = m->wl_long[1];
			gres[0] = m->nwav[1];
			gres[1] = m->nwav[1];
			avgdev[0] = 0.0;
			avgdev[1] = 0.0;
			
			trspl->fit_rspl_w(trspl, 0, sd, m->nwav[0] * m->nwav[0], glow, ghigh, gres, NULL, NULL, 0.5, avgdev, NULL);
#endif	/* !ONEDSTRAYLIGHTUS */

			m->straylight[1] = dmatrixz(0, m->nwav[1]-1, 0, m->nwav[1]-1);  

			/* Create upsampled version */
			for (i = 0; i < m->nwav[1]; i++) {		/* Output wavelength */
				for (j = 0; j < m->nwav[1]; j++) {	/* Input wavelength */
					double p0, p1;
					p0 = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], i);
					p1 = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], j);
#ifdef ONEDSTRAYLIGHTUS
					/* Do linear interp with clipping at ends */
					{
						int x0, x1, y0, y1;
						double xx, yy, w0, w1, v0, v1;

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

						pp.v[0] = w0 * v0 * slp[x0][y0] 
						        + w0 * v1 * slp[x0][y1] 
						        + w1 * v0 * slp[x1][y0] 
						        + w1 * v1 * slp[x1][y1]; 
					}
#else /* !ONEDSTRAYLIGHTUS */
					pp.p[0] = p0;
					pp.p[1] = p1;
					trspl->interp(trspl, &pp);
#endif /* !ONEDSTRAYLIGHTUS */
					m->straylight[1][i][j] = pp.v[0] * HIGHRES_WIDTH/10.0;
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
#endif /* HIGH_RES_PLOT */

#ifdef ONEDSTRAYLIGHTUS
			free_dmatrix(slp, 0, m->nwav[0]-1, 0, m->nwav[0]-1);
#else /* !ONEDSTRAYLIGHTUS */
			trspl->del(trspl);
#endif /* !ONEDSTRAYLIGHTUS */
		}
	}

	/* Create or re-create the high resolution filters */
	for (refl = 0; refl < 2; refl++) {	/* for emis/trans and reflective */
#define MXNOWL 500		/* Max hires bands */
#define MXNOFC 64		/* Max hires coeffs */

#ifndef USE_TRI_LAGRANGE				/* Use decimation filter */
		int super = 0;					/* nz if we're super sampling */
		double fshmax;					/* filter shape max wavelength from center */
		i1pro_fc coeff2[MXNOWL][MXNOFC];	/* New filter cooefficients */

		/* Construct a set of filters that uses more CCD values */

		if (twidth < 3.0)
			super = 1;

		if (m->nwav[1] > MXNOWL) {		/* Assert */
			a1loge(p->log,1,"High res filter has too many bands\n");
			return I1PRO_INT_ASSERT;
		}

		if (super) {
			fshmax = 5.0;
		} else {
			/* Use a simple means of determining width */
			for (fshmax = 50.0; fshmax >= 0.0; fshmax -= 0.1) {
				if (fabs(lanczos2(twidth, fshmax)) > 0.0001) {
					fshmax += 0.1;
					break;
				}
			}
			if (fshmax <= 0.0) {
				a1logw(p->log, "i1pro: fshmax search failed\n");
				return I1PRO_INT_ASSERT;
			}
		}
	
//		printf("~1 fshmax = %f\n",fshmax);

#ifdef HIGH_RES_DEBUG
		/* Check that the filter sums to a constant */
		{
			double x, sum;
	
			for (x = 0.0; x < 5.0; x += 0.1) {
				sum = 0;
				sum += lin_fshape(fshape, ncp, x - 15.0);
				sum += lin_fshape(fshape, ncp, x - 10.0);
				sum += lin_fshape(fshape, ncp, x -  5.0);
				sum += lin_fshape(fshape, ncp, x -  0.0);
				sum += lin_fshape(fshape, ncp, x +  5.0);
				sum += lin_fshape(fshape, ncp, x + 10.0);
				printf("Offset %f, sum %f\n",x, sum);
			}
		}
#endif /* HIGH_RES_DEBUG */

		/* Create all the filters */
		if (m->mtx_c[1][refl].nocoef != NULL)
			free(m->mtx_c[1][refl].nocoef);
		if ((m->mtx_c[1][refl].nocoef = (int *)calloc(m->nwav[1], sizeof(int))) == NULL) {
			a1logw(p->log, "i1pro: malloc nocoef failed!\n");
			return I1PRO_INT_MALLOC;
		}

		if (super) {	/* Use linear interpolation */

			/* For all the useful CCD bands */
			for (i = 1; i < (127-1); i++) {
				double wl, wh;

				/* Translate CCD centers to calibrated wavelength */
				wh = i1pro_raw2wav(p, refl, (double)(i+0));
				wl = i1pro_raw2wav(p, refl, (double)(i+1));

				/* For each filter */
				for (j = 0; j < m->nwav[1]; j++) {
					double cwl;		/* Center wavelength */
					double we;		/* Weighting */
					double wwwe = 1.0;

					cwl = m->wl_short[1] + (double)j * (m->wl_long[1] - m->wl_short[1])
					                                  /(m->nwav[1]-1.0);
//printf("~1 wl %f, wh %f, cwl %f\n",wl,wh,cwl);
					if (cwl < (wl - 1e-6) || cwl > (wh + 1e-6))
						continue;		/* Doesn't fall into this filter */

					if ((m->mtx_c[1][refl].nocoef[j]+1) >= MXNOFC) {
						a1logw(p->log, "i1pro: run out of high res filter space\n");
						return I1PRO_INT_ASSERT;
					}

					we = (cwl - wl)/(wh - wl);
//					wwwe = 3.3/(wh - wl);

					coeff2[j][m->mtx_c[1][refl].nocoef[j]].ix = i;
					coeff2[j][m->mtx_c[1][refl].nocoef[j]++].we = wwwe * we; 
//printf("~1 filter %d, cwl %f, ix %d, we %f, nocoefs %d\n",j,cwl,i,we,m->mtx_c[1][refl].nocoef[j]);
					coeff2[j][m->mtx_c[1][refl].nocoef[j]].ix = i+1;
					coeff2[j][m->mtx_c[1][refl].nocoef[j]++].we = wwwe * (1.0 - we); 
//printf("~1 filter %d, cwl %f, ix %d, we %f, nocoefs %d\n",j,cwl,i+1,1.0-we,m->mtx_c[1][refl].nocoef[j]);
				}
			}

		} else {
			/* For all the useful CCD bands */
			for (i = 1; i < 127; i++) {
				double w1, wl, w2;

				/* Translate CCD center and boundaries to calibrated wavelength */
				wl = i1pro_raw2wav(p, refl, (double)i);
				w1 = i1pro_raw2wav(p, refl, (double)i - 0.5);
				w2 = i1pro_raw2wav(p, refl, (double)i + 0.5);

//			printf("~1 CCD %d, w1 %f, wl %f, w2 %f\n",i,w1,wl,w2);

				/* For each filter */
				for (j = 0; j < m->nwav[1]; j++) {
					double cwl, rwl;		/* center, relative wavelegth */
					double we;

					cwl = m->wl_short[1] + (double)j * (m->wl_long[1] - m->wl_short[1])
					                                  /(m->nwav[1]-1.0);
					rwl = wl - cwl;			/* relative wavelength to filter */

					if (fabs(w1 - cwl) > fshmax && fabs(w2 - cwl) > fshmax)
						continue;		/* Doesn't fall into this filter */

					/* Integrate in 0.05 nm increments from filter shape */
					/* using triangular integration. */
					{
						int nn;
						double lw, ll;
#ifdef FAST_HIGH_RES_SETUP
# define FINC 0.2
#else
# define FINC 0.05
#endif
						nn = (int)(fabs(w2 - w1)/0.2 + 0.5);		/* Number to integrate over */
				
						lw = w1;				/* start at lower boundary of CCD cell */
						ll = lanczos2(twidth, w1- cwl);
						we = 0.0;
						for (k = 0; k < nn; k++) { 
							double cw, cl;

#if defined(__APPLE__) && defined(__POWERPC__)
							gcc_bug_fix(k);
#endif
							cw = w1 + (k+1.0)/(nn +1.0) * (w2 - w1);	/* wl to sample */
							cl = lanczos2(twidth, cw- cwl);
							we += 0.5 * (cl + ll) * (lw - cw);			/* Area under triangle */
							ll = cl;
							lw = cw;
						}
					}

					if (m->mtx_c[1][refl].nocoef[j] >= MXNOFC) {
						a1logw(p->log, "i1pro: run out of high res filter space\n");
						return I1PRO_INT_ASSERT;
					}

					coeff2[j][m->mtx_c[1][refl].nocoef[j]].ix = i;
					coeff2[j][m->mtx_c[1][refl].nocoef[j]++].we = we; 
//				printf("~1 filter %d, cwl %f, rwl %f, ix %d, we %f, nocoefs %d\n",j,cwl,rwl,i,we,m->mtx_c[1][refl].nocoef[j]);
				}
			}
		}

#ifdef HIGH_RES_PLOT
		/* Plot resampled curves */
		{
			double *xx, *ss;
			double **yy;

			xx = dvectorz(-1, m->nraw-1);		/* X index */
			yy = dmatrixz(0, 5, -1, m->nraw-1);	/* Curves distributed amongst 5 graphs */
			
			for (i = 0; i < m->nraw; i++)
				xx[i] = i;

			/* For each output wavelength */
			for (j = 0; j < m->nwav[1]; j++) {
				i = j % 5;

				/* For each matrix value */
				for (k = 0; k < m->mtx_c[1][refl].nocoef[j]; k++) {
					yy[5][coeff2[j][k].ix] += 0.5 * coeff2[j][k].we;
					yy[i][coeff2[j][k].ix] = coeff2[j][k].we;
				}
			}

			printf("Hi-Res wavelength sampling curves %s:\n",refl ? "refl" : "emis");
			do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
			free_dvector(xx, -1, m->nraw-1);
			free_dmatrix(yy, 0, 2, -1, m->nraw-1);
		}
#endif /* HIGH_RES_PLOT */

		/* Convert hires filters into runtime format */
		{
			int xcount;

			/* Allocate or reallocate high res filter tables */
			if (m->mtx_c[1][refl].index != NULL)
				free(m->mtx_c[1][refl].index);
			if (m->mtx_c[1][refl].coef != NULL)
				free(m->mtx_c[1][refl].coef);

			if ((m->mtx_c[1][refl].index = (int *)calloc(m->nwav[1], sizeof(int))) == NULL) {
				a1logw(p->log, "i1pro: malloc index failed!\n");
				return I1PRO_INT_MALLOC;
			}
	
			xcount = 0;
			for (j = 0; j < m->nwav[1]; j++) {
				m->mtx_c[1][refl].index[j] = coeff2[j][0].ix;
				xcount += m->mtx_c[1][refl].nocoef[j];
			}
	
			if ((m->mtx_c[1][refl].coef = (double *)calloc(xcount, sizeof(double))) == NULL) {
				a1logw(p->log, "i1pro: malloc coef failed!\n");
				return I1PRO_INT_MALLOC;
			}

			for (i = j = 0; j < m->nwav[1]; j++)
				for (k = 0; k < m->mtx_c[1][refl].nocoef[j]; k++, i++)
					m->mtx_c[1][refl].coef[i] = coeff2[j][k].we;

			/* Set high res tables to new allocations */
			m->mtx[1][refl] = m->mtx_c[1][refl];
		}

#else	/* USE_TRI_LAGRANGE, use triangle over lagrange interp */

		/* Compute high res. reflective wavelength corrected filters */
		if ((ev = i1pro_compute_wav_filters(p, 1, refl)) != I1PRO_OK) {
			a1logd(p->log,2,"i1pro_compute_wav_filters() failed\n");
			return ev;
		}
#endif	/* USE_TRI_LAGRANGE */

		/* Normalise the filters area in CCD space, while maintaining the */
		/* total contribution of each CCD at the target too. */
		/* Hmm. This will wreck super-sample. We should fix it */
#ifdef DO_CCDNORM			/* Normalise CCD values to original */
		{
			double x[4], y[4];
			double avg[2], max[2];
			double ccdsum[2][128];			/* Target weight/actual for each CCD */
			double dth[2];

			avg[0] = avg[1] = 0.0;
			max[0] = max[1] = 0.0;
			for (j = 0; j < 128; j++) {
				ccdsum[0][j] = 0.0;
				ccdsum[1][j] = 0.0;
			}

			/* Compute the weighting of each CCD value in the normal output */
			for (cx = j = 0; j < m->nwav[0]; j++) { /* For each wavelength */

				/* For each matrix value */
				sx = m->mtx_o.index[j];		/* Starting index */
				for (k = 0; k < m->mtx_o.nocoef[j]; k++, cx++, sx++) {
					ccdsum[0][sx] += m->mtx_o.coef[cx];
//printf("~1 Norm CCD [%d] %f += [%d] %f\n",sx,ccdsum[0][sx],cx, m->mtx_o.coef[cx]);
				}
			}

			/* Compute the weighting of each CCD value in the hires output */
			for (cx = j = 0; j < m->nwav[1]; j++) { /* For each wavelength */

				/* For each matrix value */
				sx = m->mtx_c[1][refl].index[j];		/* Starting index */
				for (k = 0; k < m->mtx_c[1][refl].nocoef[j]; k++, cx++, sx++) {
					ccdsum[1][sx] += m->mtx_c[1][refl].coef[cx];
//printf("~1 HiRes CCD [%d] %f += [%d] %f\n",sx,ccdsum[1][sx],cx, m->mtx_c[1][refl].coef[cx]);
				}
			}

			/* Figure valid range and extrapolate to edges */
			dth[0] = 0.0;		/* ref */
//			dth[1] = 0.007;		/* hires */
			dth[1] = 0.004;		/* hires */

			for (k = 0; k < 2; k++) {

				for (i = 0; i < 128; i++) {
					if (ccdsum[k][i] > max[k])
						max[k] = ccdsum[k][i];
				}

//printf("~1 max[%d] = %f\n",k, max[k]);
				/* Figure out the valid range */
				for (i = 64; i >= 0; i--) {
					if (ccdsum[k][i] > (0.8 * max[k])) {
						x[0] = (double)i;
					} else {
						break;
					}
				}
				for (i = 64; i < 128; i++) {
					if (ccdsum[k][i] > (0.8 * max[k])) {
						x[3] = (double)i;
					} else {
						break;
					}
				}
				/* Space off the last couple of entries */
				x[0] += (3.0 + 3.0);
				x[3] -= (3.0 + 3.0);
				x[1] = floor((2 * x[0] + x[3])/3.0);
				x[2] = floor((x[0] + 2 * x[3])/3.0);

				for (i = 0; i < 4; i++) {
					y[i] = 0.0;
					for (j = -3; j < 4; j++) {
						y[i] += ccdsum[k][(int)x[i]+j];
					}
					y[i] /= 7.0;
				}

//printf("~1 extrap nodes %f, %f, %f, %f\n",x[0],x[1],x[2],x[3]);
//printf("~1 extrap value %f, %f, %f, %f\n",y[0],y[1],y[2],y[3]);

				for (i = 0; i < 128; i++) {
					double xw, yw;

					xw = (double)i;
	
					/* Compute interpolated value using Lagrange: */
					yw = y[0] * (xw-x[1]) * (xw-x[2]) * (xw-x[3])
						      /((x[0]-x[1]) * (x[0]-x[2]) * (x[0]-x[3]))
					   + y[1] * (xw-x[0]) * (xw-x[2]) * (xw-x[3])
						      /((x[1]-x[0]) * (x[1]-x[2]) * (x[1]-x[3]))
					   + y[2] * (xw-x[0]) * (xw-x[1]) * (xw-x[3])
						      /((x[2]-x[0]) * (x[2]-x[1]) * (x[2]-x[3]))
					   + y[3] * (xw-x[0]) * (xw-x[1]) * (xw-x[2])
						      /((x[3]-x[0]) * (x[3]-x[1]) * (x[3]-x[2]));
	
					if ((xw < x[0] || xw > x[3])
					 && fabs(ccdsum[k][i] - yw)/yw > dth[k]) {
						ccdsum[k][i] = yw;
					}
					avg[k] += ccdsum[k][i];
				}
				avg[k] /= 128.0;
			}

#ifdef HIGH_RES_PLOT
			/* Plot target CCD values */
			{
				double xx[128], y1[128], y2[128];
	
				for (i = 0; i < 128; i++) {
					xx[i] = i;
					y1[i] = ccdsum[0][i]/avg[0];
					y2[i] = ccdsum[1][i]/avg[1];
				}
	
				printf("Target and actual CCD weight sums:\n");
				do_plot(xx, y1, y2, NULL, 128);
			}
#endif

#ifdef DO_CCDNORMAVG	/* Just correct by average */
			for (cx = j = 0; j < m->nwav[1]; j++) { /* For each wavelength */

				/* For each matrix value */
				sx = m->mtx_c[1][refl].index[j];		/* Starting index */
				for (k = 0; k < m->mtx_c[1][refl].nocoef[j]; k++, cx++, sx++) {
					m->mtx_c[1][refl].coef[cx] *= 10.0/twidth * avg[0]/avg[1];
				}
			}

#else			/* Correct by CCD bin */

			/* Correct the weighting of each CCD value in the hires output */
			for (i = 0; i < 128; i++) {
				ccdsum[1][i] = 10.0/twidth * ccdsum[0][i]/ccdsum[1][i];		/* Correction factor */
			}
			for (cx = j = 0; j < m->nwav[1]; j++) { /* For each wavelength */

				/* For each matrix value */
				sx = m->mtx_c[1][refl].index[j];		/* Starting index */
				for (k = 0; k < m->mtx_c[1][refl].nocoef[j]; k++, cx++, sx++) {
					m->mtx_c[1][refl].coef[cx] *= ccdsum[1][sx];
				}
			}
#endif
		}
#endif /* DO_CCDNORM */

#ifdef HIGH_RES_PLOT
		{
			static i1pro_fc coeff2[MXNOWL][MXNOFC];	
			double *xx, *ss;
			double **yy;

			/* Convert the native filter cooeficient representation to */
			/* a 2D array we can randomly index. */
			for (cx = j = 0; j < m->nwav[1]; j++) { /* For each output wavelength */
				if (j >= MXNOWL) {	/* Assert */
					a1loge(p->log,1,"i1pro: number of hires output wavelenths is > %d\n",MXNOWL);
					return I1PRO_INT_ASSERT;
				}

				/* For each matrix value */
				sx = m->mtx[1][refl].index[j];		/* Starting index */
				for (k = 0; k < m->mtx[1][refl].nocoef[j]; k++, cx++, sx++) {
					if (k >= MXNOFC) {	/* Assert */
						a1loge(p->log,1,"i1pro: number of hires filter coeefs is > %d\n",MXNOFC);
						return I1PRO_INT_ASSERT;
					}
					coeff2[j][k].ix = sx;
					coeff2[j][k].we = m->mtx[1][refl].coef[cx];
//				printf("Output %d, filter %d weight = %e\n",j,k,coeff2[j][k].we);
				}
			}
	
			xx = dvectorz(-1, m->nraw-1);		/* X index */
			yy = dmatrixz(0, 5, -1, m->nraw-1);	/* Curves distributed amongst 5 graphs */
			
			for (i = 0; i < m->nraw; i++)
				xx[i] = i;

			/* For each output wavelength */
			for (j = 0; j < m->nwav[1]; j++) {
				i = j % 5;

				/* For each matrix value */
				for (k = 0; k < m->mtx_c[1][refl].nocoef[j]; k++) {
					yy[5][coeff2[j][k].ix] += 0.5 * coeff2[j][k].we;
					yy[i][coeff2[j][k].ix] = coeff2[j][k].we;
				}
			}

			printf("Normalized Hi-Res wavelength sampling curves: %s\n",refl ? "refl" : "emis");
			do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
			free_dvector(xx, -1, m->nraw-1);
			free_dmatrix(yy, 0, 2, -1, m->nraw-1);
		}
#endif /* HIGH_RES_PLOT */
#undef MXNOWL
#undef MXNOFC
	}	/* Do next filter */

	/* Hires has been initialised */
	m->hr_inited = 1;

	/* Generate high res. per mode calibration factors. */
	if ((ev = i1pro_create_hr_calfactors(p, 0)) != I1PRO_OK)
		return ev;

	return ev;
}

#endif /* HIGH_RES */


/* return nz if high res is supported */
int i1pro_imp_highres(i1pro *p) {
#ifdef HIGH_RES
	return 1;
#else
	return 0;
#endif /* HIGH_RES */
}

/* Set to high resolution mode */
i1pro_code i1pro_set_highres(i1pro *p) {
	int i;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code ev = I1PRO_OK;

#ifdef HIGH_RES
	if (m->hr_inited == 0) {
		if ((ev = i1pro_create_hr(p)) != I1PRO_OK)
			return ev;
	}
	m->highres = 1;
#else
	ev = I1PRO_UNSUPPORTED;
#endif /* HIGH_RES */

	return ev;
}

/* Set to standard resolution mode */
i1pro_code i1pro_set_stdres(i1pro *p) {
	int i;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code ev = I1PRO_OK;

#ifdef HIGH_RES
	m->highres = 0;
#else
	ev = I1PRO_UNSUPPORTED;
#endif /* HIGH_RES */

	return ev;
}

/* =============================================== */

/* Modify the scan consistency tolerance */
i1pro_code i1pro_set_scan_toll(i1pro *p, double toll_ratio) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code ev = I1PRO_OK;

	m->scan_toll_ratio = toll_ratio;

	return I1PRO_OK;
}


/* Optics adjustment weights */
static double opt_adj_weights[21] = {
	1.4944496665144658e-282, 2.0036175483913455e-070, 1.2554893022685038e+232,
	2.3898157055642966e+190, 1.5697625128432372e-076, 6.6912978722191457e+281,
	1.2369092402930559e+277, 1.4430907501246712e-153, 3.0017439193018232e+238,
	1.2978311824382444e+161, 5.5068703318775818e-311, 7.7791723264455314e-260,
	6.4560484084110176e+170, 8.9481529920968425e+165, 1.3565405878488529e-153,
	2.0835868791190880e-076, 5.4310198502711138e+241, 4.8689849775675438e+275,
	9.2709981544886391e+122, 3.7958270103353899e-153, 7.1366083837501666e-154
};

/* Convert from spectral to XYZ, and transfer to the ipatch array */
i1pro_code i1pro_conv2XYZ(
	i1pro *p,
	ipatch *vals,		/* Values to return */
	int nvals,			/* Number of values */
	double **specrd,	/* Spectral readings */
	instClamping clamp	/* Clamp XYZ/Lab to be +ve */
) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	xsp2cie *conv;	/* Spectral to XYZ conversion object */
	int i, j, k;
	int six = 0;		/* Starting index */
	int nwl = m->nwav[m->highres];	/* Number of wavelegths */
	double wl_short = m->wl_short[m->highres];	/* Starting wavelegth */
	double sms;			/* Weighting */

	if (s->emiss)
		conv = new_xsp2cie(icxIT_none, NULL, icxOT_CIE_1931_2, NULL, icSigXYZData, (icxClamping)clamp);
	else
		conv = new_xsp2cie(icxIT_D50, NULL, icxOT_CIE_1931_2, NULL, icSigXYZData, (icxClamping)clamp);
	if (conv == NULL)
		return I1PRO_INT_CIECONVFAIL;

	/* Don't report any wavelengths below the minimum for this mode */
	if ((s->min_wl-1e-3) > wl_short) {
		double wl = 0.0;
		for (j = 0; j < m->nwav[m->highres]; j++) {
			wl = XSPECT_WL(m->wl_short[m->highres], m->wl_long[m->highres], m->nwav[m->highres], j);
			if (wl >= (s->min_wl-1e-3))
				break;
		}
		six = j;
		wl_short = wl;
		nwl -= six;
	}

	a1logd(p->log,5,"i1pro_conv2XYZ got wl_short %f, wl_long %f, nwav %d, min_wl %f\n",
	                m->wl_short[m->highres], m->wl_long[m->highres], m->nwav[m->highres], s->min_wl);
	a1logd(p->log,5,"      after skip got wl_short %f, nwl = %d\n", wl_short, nwl);

	for (sms = 0.0, i = 1; i < 21; i++)
		sms += opt_adj_weights[i];
	sms *= opt_adj_weights[0];

	for (i = 0; i < nvals; i++) {

		vals[i].loc[0] = '\000';
		vals[i].mtype = inst_mrt_none;
		vals[i].XYZ_v = 0;
		vals[i].sp.spec_n = 0;
		vals[i].duration = 0.0;
	
		vals[i].sp.spec_n = nwl;
		vals[i].sp.spec_wl_short = wl_short;
		vals[i].sp.spec_wl_long = m->wl_long[m->highres];

		if (s->emiss) {
			/* Leave spectral values as mW/m^2 */
			for (j = six, k = 0; j < m->nwav[m->highres]; j++, k++) {
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
			/* Scale spectral values to percentage reflectance */
			for (j = six, k = 0; j < m->nwav[m->highres]; j++, k++) {
				vals[i].sp.spec[k] = 100.0 * specrd[i][j] * sms;
			}
			vals[i].sp.norm = 100.0;

			/* Set the XYZ */
			conv->convert(conv, vals[i].XYZ, &vals[i].sp);
			vals[i].XYZ_v = 1;
			vals[i].XYZ[0] *= 100.0;
			vals[i].XYZ[1] *= 100.0;
			vals[i].XYZ[2] *= 100.0;

			if (s->trans)
				vals[i].mtype = inst_mrt_transmissive;
			else
				vals[i].mtype = inst_mrt_reflective;
		}

		/* Don't return spectral if not asked for */
		if (!m->spec_en) {
			vals[i].sp.spec_n = 0;
		}

	}

	conv->del(conv);
	return I1PRO_OK;
}

/* Check a reflective white reference measurement to see if */
/* it seems reasonable. Return I1PRO_OK if it is, error if not. */
i1pro_code i1pro_check_white_reference1(
	i1pro *p,
	double *abswav			/* [nwav[0]] Measurement to check */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	double *emiswav, normfac;
	double avg01, avg2227;
	int j;

	emiswav = dvector(-1, m->nraw-1);

	/* Convert from absolute wavelength converted sensor reading, */
	/* to calibrated emission wavelength spectrum. */

	/* For each output wavelength */
	for (j = 0; j < m->nwav[0]; j++) {
		emiswav[j] = m->emis_coef[0][j] * abswav[j];
	}
#ifdef PLOT_DEBUG
	printf("White ref read converted to emissive spectrum:\n");
	plot_wav(m, 0, emiswav);
#endif

	/* Normalise the measurement to the reflectance of the 17 wavelength */
	/* of the white reference (550nm), as well as dividing out the */
	/* reference white. This should leave us with the iluminant spectrum, */
	normfac = m->white_ref[0][17]/emiswav[17];
	
	for (j = 0; j < m->nwav[0]; j++) {
		emiswav[j] *= normfac/m->white_ref[0][j];
	}

#ifdef PLOT_DEBUG
	printf("normalised to reference white read:\n");
	plot_wav(m, 0, emiswav);
#endif

	/* Compute two sample averages of the illuminant spectrum. */
	avg01 = 0.5 * (emiswav[0] + emiswav[1]);

	for (avg2227 = 0, j = 22; j < 28; j++) {
		avg2227 += emiswav[j];
	}
	avg2227 /= (double)(28 - 22);

	free_dvector(emiswav, -1, m->nraw-1);


	/* And check them against tolerance for the illuminant. */
	if (m->physfilt == 0x82) {		/* UV filter */
		a1logd(p->log,2,"Checking white reference (UV): 0.0 < avg01 %f < 0.05, 1.2 < avg2227 %f < 1.76\n",avg01,avg2227);
		if (0.0 < avg01 && avg01 < 0.05
		 && 1.2 < avg2227 && avg2227 < 1.76) {
			return I1PRO_OK;
		}

	} else {						/* No filter */
		a1logd(p->log,2,"Checking white reference: 0.11 < avg01 %f < 0.22, 1.35 < avg2227 %f < 1.6\n",avg01,avg2227);
		if (0.11 < avg01 && avg01 < 0.22
		 && 1.35 < avg2227 && avg2227 < 1.6) {
			return I1PRO_OK;
		}
	}
	a1logd(p->log,2,"Checking white reference failed - out of tollerance");
	return I1PRO_RD_WHITEREFERROR;
}

/* Compute a mode calibration factor given the reading of the white reference. */
/* We will also calibrate & smooth hi-res emis_coef[1] if they are present. */
/* Return I1PRO_RD_TRANSWHITEWARN if any of the transmission wavelengths are low. */
/* May return some other error (malloc) */
i1pro_code i1pro_compute_white_cal(
	i1pro *p,
	double *cal_factor0,	/* [nwav[0]] Calibration factor to compute */
	double *white_ref0,		/* [nwav[0]] White reference to aim for, NULL for 1.0 */
	double *white_read0,	/* [nwav[0]] The white that was read */
	double *cal_factor1,	/* [nwav[1]] Calibration factor to compute */
	double *white_ref1,		/* [nwav[1]] White reference to aim for, NULL for 1.0 */
	double *white_read1,	/* [nwav[1]] The white that was read */
	int do_emis_ft			/* Do emission hires fine tune with this info. */
) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	int j, warn = I1PRO_OK;;
	
#ifdef HIGH_RES
	/* If we need to, fine calibrate the emission */
	/* calibration coefficients, using the reflectance cal. */
	/* illuminant as an (assumed) smooth light source reference. */
	/* (Do this first, befor white_read0/cal_factor0 is overwritten by white cal.) */
	if (do_emis_ft && m->hr_inited != 0 && white_ref1 != NULL) {
		i1pro_code ev = I1PRO_OK;
		int i;
		double *lincal;
		double *fudge;
		xspect illA;
#ifdef HIGH_RES_PLOT
		double *targ_smth;		/* Hires target in smooth space */
		double *old_emis;
		double avgl;

		if ((targ_smth = (double *)calloc(m->nwav[1], sizeof(double))) == NULL) {
			return I1PRO_INT_MALLOC;
		}
		if ((old_emis = (double *)calloc(m->nwav[1], sizeof(double))) == NULL) {
			return I1PRO_INT_MALLOC;
		}
		for (i = 0; i < m->nwav[1]; i++)
			old_emis[i] = m->emis_coef[1][i];
#endif

		if ((lincal = (double *)calloc(m->nwav[0], sizeof(double))) == NULL) {
			return I1PRO_INT_MALLOC;
		}
		if ((fudge = (double *)calloc(m->nwav[0], sizeof(double))) == NULL) {
			return I1PRO_INT_MALLOC;
		}

		/* Fill in an xpsect with a standard illuminant spectrum */
		if (standardIlluminant(&illA, icxIT_Ptemp, 2990.0)) {
			a1loge(p->log,1,"i1pro_compute_white_cal: standardIlluminant() failed");
			return I1PRO_INT_ASSERT;
		}

		for (j = 0; j < m->nwav[0]; j++) {
			double wl = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], j); 

			lincal[j] = white_read0[j] * m->emis_coef[0][j]
			          / (white_ref0[j] * value_xspect(&illA, wl));
		}

#ifndef NEVER
		/* Generate the hires emis_coef by interpolating lincal */
		/* using rspl, and reversing computation through hi-res readings */
		{
			rspl *trspl;			/* Upsample rspl */
			cow sd[40];		/* Scattered data points of existing references */
			datai glow, ghigh;
			datao vlow, vhigh;
			int gres[1];
			double avgdev[1];
			int ix;
			co pp;

			if ((trspl = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
				a1logd(p->log,1,"i1pro: creating rspl for high res conversion failed\n");
				return I1PRO_INT_NEW_RSPL_FAILED;
			}
			
			vlow[0] = 1e6;
			vhigh[0] = -1e6;
			for (ix = i = 0; i < m->nwav[0]; i++) {

				sd[ix].p[0] = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], i);
				sd[ix].v[0] = lincal[i];
				sd[ix].w = 1.0;

				if (sd[ix].v[0] < vlow[0])
					vlow[0] = sd[ix].v[0];
				if (sd[ix].v[0] > vhigh[0])
				    vhigh[0] = sd[ix].v[0];
				ix++;
			}

			glow[0] = m->wl_short[1];
			ghigh[0] = m->wl_long[1];
			gres[0] = 6 * m->nwav[1];
			avgdev[0] = 0.0;
		
			/* The smoothness factor of 0.02 seems critical in tuning the RevE */
			/* hires accuracy, as measured on an LCD display */
			trspl->fit_rspl_w(trspl, 0, sd, ix, glow, ghigh, gres, vlow, vhigh, 0.05, avgdev, NULL);

			/* Create a linear interp fudge factor to place interp at low */
			/* res. exactly on lowres linear curve. This compensates */
			/* if the rspl moves the target at the lowres points */
			for (j = 0; j < m->nwav[0]; j++) {
				double wl = XSPECT_WL(m->wl_short[0], m->wl_long[0], m->nwav[0], j); 
	
				pp.p[0] = wl;
				trspl->interp(trspl, &pp);

				fudge[j] = lincal[j] / pp.v[0];
			}

			/* Compute hires emis_coef */
			for (i = 0; i < m->nwav[1]; i++) {
				double wl = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], i);
				double ff;

				pp.p[0] = wl;
				trspl->interp(trspl, &pp);

#ifdef HIGH_RES_PLOT
				targ_smth[i] = pp.v[0];
#endif
				/* Invert lincal at high res */
				ff = wav_lerp(m, 0, fudge, wl);
				m->emis_coef[1][i] = (ff * pp.v[0] * white_ref1[i] * value_xspect(&illA, wl))
				                     /white_read1[i];
			}
			trspl->del(trspl);
		}

#else
		/* Generate the hires emis_coef by interpolating lincal */
		/* using lagrange, and reversing computation through hi-res readings */
		for (i = 0; i < m->nwav[1]; i++) {
			int k;
			double x[4], xw;
			double y[4], yw;

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
				y[k] = lincal[j+k];
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

			targ_smth[i] = yw;

			/* Invert lincal at high res */
			m->emis_coef[1][i] = (yw  * white_ref1[i] * value_xspect(&illA, xw))/white_read1[i];
		}
#endif /* NEVER */

#ifdef HIGH_RES_PLOT
		/* Plot linear target curve and measured curve */
		{
			double *xx, *y1, *y2, *y3, *y4;

			xx = dvector(0, m->nwav[1]);		/* X  wl */
			y1 = dvector(0, m->nwav[1]);
			y2 = dvector(0, m->nwav[1]);
			y3 = dvector(0, m->nwav[1]);
			y4 = dvector(0, m->nwav[1]);
			
			for (j = 0; j < (m->nwav[1]); j++) {
				xx[j] = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], j);
				y1[j] = wav_lerp_cv(m, 0, lincal, xx[j], 0.0);
				y2[j] = targ_smth[j];
				y3[j] = white_read1[j] * wav_lerp(m, 0, m->emis_coef[0], xx[j])
				        /(white_ref1[j] * value_xspect(&illA, xx[j]));
				y4[j] = white_read1[j] * old_emis[j]
				        /(white_ref1[j] * value_xspect(&illA, xx[j]));
			}

			printf("stdres interp targ (bk), smoothed targ (rd), measured hires resp. (gn), previous cal resp.(bu):\n");
			do_plot6(xx, y1, y2, y3, y4, NULL, NULL, m->nwav[1]);
			free_dvector(xx, 0, m->nwav[1]);
			free_dvector(y1, 0, m->nwav[1]);
			free_dvector(y2, 0, m->nwav[1]);
			free_dvector(y4, 0, m->nwav[1]);
		}
		/* Plot target and achieved smooth space responses */
		{
			double *xx, *y2;

			xx = dvector(0, m->nwav[1]);
			y2 = dvector(0, m->nwav[1]);
			
			for (j = 0; j < (m->nwav[1]); j++) {
				xx[j] = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], j);
				y2[j] = white_read1[j] * m->emis_coef[1][j]
				       /(white_ref1[j] * value_xspect(&illA, xx[j]));
			}

			printf("target smooth curve, achived smooth curve:\n");
			do_plot6(xx, targ_smth, y2, NULL, NULL, NULL, NULL, m->nwav[1]);

			free_dvector(xx, 0, m->nwav[1]);
			free_dvector(y2, 0, m->nwav[1]);
		}
		/* Plot lowres and hires lamp response */
		{
			double *xx, *y1, *y2;

			xx = dvector(0, m->nwav[1]);
			y1 = dvector(0, m->nwav[1]);
			y2 = dvector(0, m->nwav[1]);
			
			for (j = 0; j < (m->nwav[1]); j++) {
				xx[j] = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], j);
				y1[j] = wav_lerp(m, 0, white_read0, xx[j]) * wav_lerp(m, 0, m->emis_coef[0], xx[j])
				        /wav_lerp(m, 0, white_ref0, xx[j]);
				y2[j] = white_read1[j] * m->emis_coef[1][j]
				       /white_ref1[j];
			}

			printf("lowres, high res lamp response:\n");
			do_plot6(xx, y1, y2, NULL, NULL, NULL, NULL, m->nwav[1]);

			free_dvector(xx, 0, m->nwav[1]);
			free_dvector(y1, 0, m->nwav[1]);
			free_dvector(y2, 0, m->nwav[1]);
		}
		/* Plot hires emis calibration */
		{
			double *xx;

			xx = dvector(0, m->nwav[1]);		/* X  wl */
			
			for (j = 0; j < (m->nwav[1]); j++) {
				xx[j] = XSPECT_WL(m->wl_short[1], m->wl_long[1], m->nwav[1], j);
			}

			printf("orig upsampled + smoothed hires emis_coef:\n");
			do_plot6(xx, old_emis, m->emis_coef[1], NULL, NULL, NULL, NULL, m->nwav[1]);
			free_dvector(xx, 0, m->nwav[1]);
		}
		free(old_emis);
		free(targ_smth);
#endif /* HIGH_RES_PLOT */
		free(fudge);
		free(lincal);

		m->emis_hr_cal = 1;		/* We don't have to do a reflective calibration */

		/* Make sure these are updated */
		if ((ev = i1pro_create_hr_calfactors(p, 1)) != I1PRO_OK)
			return ev;
	}
#endif /* HIGH_RES */

	if (white_ref0 == NULL) {		/* transmission white reference */
		double avgwh = 0.0;

		/* Compute average white reference reading */
		for (j = 0; j < m->nwav[0]; j++)
			avgwh += white_read0[j];
		avgwh /= (double)m->nwav[0];

		/* For each wavelength */
		for (j = 0; j < m->nwav[0]; j++) {
			/* If reference is < 0.4% of average */
			if (white_read0[j]/avgwh < 0.004) {
				cal_factor0[j] = 1.0/(0.004 * avgwh);
				warn = I1PRO_RD_TRANSWHITEWARN;
			} else {
				cal_factor0[j] = 1.0/white_read0[j];	
			}
		}

	} else {					/* Reflection white reference */

		/* For each wavelength */
		for (j = 0; j < m->nwav[0]; j++) {
			if (white_read0[j] < 1000.0)
				cal_factor0[j] = white_ref0[j]/1000.0;	
			else
				cal_factor0[j] = white_ref0[j]/white_read0[j];	
		}
	}

#ifdef HIGH_RES
	if (m->hr_inited == 0)
		return warn;

	if (white_ref1 == NULL) {		/* transmission white reference */
		double avgwh = 0.0;

		/* Compute average white reference reading */
		for (j = 0; j < m->nwav[1]; j++)
			avgwh += white_read1[j];
		avgwh /= (double)m->nwav[1];

		/* For each wavelength */
		for (j = 0; j < m->nwav[1]; j++) {
			/* If reference is < 0.4% of average */
			if (white_read1[j]/avgwh < 0.004) {
				cal_factor1[j] = 1.0/(0.004 * avgwh);
				warn = I1PRO_RD_TRANSWHITEWARN;
			} else {
				cal_factor1[j] = 1.0/white_read1[j];	
			}
		}

	} else {					/* Reflection white reference */

		/* For each wavelength */
		for (j = 0; j < m->nwav[1]; j++) {
			if (white_read1[j] < 1000.0)
				cal_factor1[j] = white_ref1[j]/1000.0;	
			else
				cal_factor1[j] = white_ref1[j]/white_read1[j];	
		}
#endif /* HIGH_RES */
	}
	return warn;
}

/* For adaptive mode, compute a new integration time and gain mode */
/* in order to optimise the sensor values. Note that the Rev E doesn't have */
/* a high gain mode. */
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
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	double new_int_time;
	int    new_gain_mode;

	a1logd(p->log,3,"i1pro_optimise_sensor called, inttime %f, gain mode %d, targ scale %f, scale %f\n",cur_int_time,cur_gain_mode, targoscale, scale);

	/* Compute new normal gain integration time */
	if (cur_gain_mode)		/* If high gain */
		new_int_time = cur_int_time * scale * m->highgain;
	else
		new_int_time = cur_int_time * scale;
	new_gain_mode = 0;

	a1logd(p->log,3,"target inttime %f, gain mode %d\n",new_int_time,new_gain_mode);

	/* Adjust to low light situation by increasing the integration time. */
	if (new_int_time > s->targmaxitime) {	/* Exceeding target integration time */
		if (s->targmaxitime/new_int_time > s->targoscale2) {	/* But within range */
			/* Compromise sensor target value to maintain targmaxitime */
			new_int_time = s->targmaxitime;
			a1logd(p->log,3,"Using targmaxitime with compromise sensor target\n");
		} else {
			/* Target reduced sensor value to give improved measurement time and continuity */
			new_int_time *= s->targoscale2;
			a1logd(p->log,3,"Using compromse sensor target\n");
		}
#ifdef USE_HIGH_GAIN_MODE
		/* !! Should change this so that it doesn't affect int. time, */
		/* but that we simply switch to high gain mode when the */
		/* expected level is < target_level/gain */
		/* Hmm. It may not be a good idea to use high gain mode if it compromises */
		/* the longer integration time which reduces noise. */
		if (p->itype != instI1Pro2 && new_int_time > m->max_int_time && permithg) {
			new_int_time /= m->highgain;
			new_gain_mode = 1;
			a1logd(p->log,3,"Switching to high gain mode\n");
		}
#endif
	}
	a1logd(p->log,3,"after low light adjust, inttime %f, gain mode %d\n",new_int_time,new_gain_mode);

	/* Deal with still low light */
	if (new_int_time > m->max_int_time) {
		if (permitclip)
			new_int_time = m->max_int_time;
		else
			return I1PRO_RD_LIGHTTOOLOW;
	}
	a1logd(p->log,3,"after low light clip, inttime %f, gain mode %d\n",new_int_time,new_gain_mode);

	/* Adjust to high light situation */
	if (new_int_time < m->min_int_time && targoscale < 1.0) {
		new_int_time /= targoscale;			/* Aim for non-scaled sensor optimum */
		if (new_int_time > m->min_int_time)	/* But scale as much as possible */
			new_int_time = m->min_int_time;
	}
	a1logd(p->log,3,"after high light adjust, inttime %f, gain mode %d\n",new_int_time,new_gain_mode);

	/* Deal with still high light */
	if (new_int_time < m->min_int_time) {
		if (permitclip)
			new_int_time = m->min_int_time;
		else
			return I1PRO_RD_LIGHTTOOHIGH;
	}
	a1logd(p->log,3,"after high light clip, returning inttime %f, gain mode %d\n",new_int_time,new_gain_mode);

	if (pnew_int_time != NULL)
		*pnew_int_time = new_int_time;

	if (pnew_gain_mode != NULL)
		*pnew_gain_mode = new_gain_mode;

	return I1PRO_OK;
}

/* Compute the number of measurements needed, given the target */
/* measurement time and integration time. Will return 0 if target time is 0 */
int i1pro_comp_nummeas(
	i1pro *p,
	double meas_time,
	double int_time
) {
	int nmeas;
	if (meas_time <= 0.0)
		return 0;
	nmeas = (int)floor(meas_time/int_time + 0.5);
	if (nmeas < 1)
		nmeas = 1;
	return nmeas;
}

/* Convert the dark interpolation data to a useful state */
/* (also allow for interpolating the shielded cell values) */
void
i1pro_prepare_idark(
	i1pro *p
) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	int i, j;

	/* For normal and high gain */
	for (i = 0; i < 4; i+= 2) {
		for (j = -1; j < m->nraw; j++) {
			double d01, d1;
			d01 = s->idark_data[i+0][j] * s->idark_int_time[i+0];
			d1 = s->idark_data[i+1][j]  * s->idark_int_time[i+1];
	
			/* Compute increment */
			s->idark_data[i+1][j] = (d1 - d01)/(s->idark_int_time[i+1] - s->idark_int_time[i+0]);
	
			/* Compute base */
			s->idark_data[i+0][j] = d01 - s->idark_data[i+1][j] * s->idark_int_time[i+0];
		}
		if (p->itype == instI1Pro2) 	/* Rev E doesn't have high gain mode */
			break;
	}
}

/* Create the dark reference for the given integration time and gain */
/* by interpolating from the 4 readings taken earlier. */
i1pro_code
i1pro_interp_dark(
	i1pro *p,
	double *result,		/* Put result of interpolation here */
	double inttime,
	int gainmode
) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	int i, j;

	if (!s->idark_valid)
		return I1PRO_INT_NOTCALIBRATED;
		
	i = 0;
#ifdef USE_HIGH_GAIN_MODE
	if (gainmode)
		i = 2;
#endif

	for (j = -1; j < m->nraw; j++) {
		double tt;
		tt = s->idark_data[i+0][j] + inttime * s->idark_data[i+1][j];
		tt /= inttime;
		result[j] = tt;
	}
	return I1PRO_OK;
}

/* Set the noinitcalib mode */
void i1pro_set_noinitcalib(i1pro *p, int v, int losecs) {
	i1proimp *m = (i1proimp *)p->m;

	/* Ignore disabling init calib if more than losecs since instrument was open */
	if (v && losecs != 0 && m->lo_secs >= losecs) {
		a1logd(p->log,3,"initcalib disable ignored because %d >= %d secs\n",m->lo_secs,losecs);
		return;
	}
	m->noinitcalib = v;
}

/* Set the trigger config */
void i1pro_set_trig(i1pro *p, inst_opt_type trig) {
	i1proimp *m = (i1proimp *)p->m;
	m->trig = trig;
}

/* Return the trigger config */
inst_opt_type i1pro_get_trig(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;
	return m->trig;
}

/* Switch thread handler */
int i1pro_switch_thread(void *pp) {
	int nfailed = 0;
	i1pro *p = (i1pro *)pp;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code rv = I1PRO_OK; 
	a1logd(p->log,3,"Switch thread started\n");
//	for (nfailed = 0;nfailed < 5;)
	/* Try indefinitely, in case instrument is put to sleep */
	for (;;) {
		rv = i1pro_waitfor_switch_th(p, SW_THREAD_TIMEOUT);
		a1logd(p->log,8,"Switch handler triggered with rv %d, th_term %d\n",rv,m->th_term);
		if (m->th_term) {
			m->th_termed = 1;
			break;
		}
		if (rv == I1PRO_INT_BUTTONTIMEOUT) {
			nfailed = 0;
			continue;
		}
		if (rv != I1PRO_OK) {
			nfailed++;
			a1logd(p->log,3,"Switch thread failed with 0x%x\n",rv);
			continue;
		}
		m->switch_count++;
		if (!m->hide_switch && p->eventcallback != NULL) {
			p->eventcallback(p->event_cntx, inst_event_switch);
		}
	}
	a1logd(p->log,3,"Switch thread returning\n");
	return rv;
}

/* ============================================================ */
/* Low level i1pro commands */

/* USB Instrument commands */

/* Reset the instrument */
i1pro_code
i1pro_reset(
	i1pro *p,
	int mask	/* reset mask ?. Known values ar 0x1f, 0x07, 0x01 */
				/* 0x1f = normal resent */
				/* 0x01 = establish high power mode */
) {
	i1proimp *m = (i1proimp *)p->m;
	unsigned char pbuf[2];	/* 1 or 2 bytes to write */
	int len = 1;			/* Message length */
	int se, rv = I1PRO_OK;
	int stime = 0;

	a1logd(p->log,2,"i1pro_reset: reset with mask 0x%02x @ %d msec\n",
	                          mask,(stime = msec_time()) - m->msec);

	pbuf[0] = mask;

	if (p->itype == instI1Pro2) {
		pbuf[1] = 0;		/* Not known what i1pro2 second byte is for */
		len = 2;
	}

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xCA, 0, 0, pbuf, len, 2.0);

	rv = icoms2i1pro_err(se);

	a1logd(p->log,2,"i1pro_reset: complete, ICOM err 0x%x (%d msec)\n",se,msec_time()-stime);

	/* Allow time for hardware to stabalize */
	msec_sleep(100);

	/* Make sure that we re-initialize the measurement mode */
	m->c_intclocks = 0;
	m->c_lampclocks = 0;
	m->c_nummeas = 0;
	m->c_measmodeflags = 0;

	return rv;
}

/* Read from the EEProm */
i1pro_code
i1pro_readEEProm(
	i1pro *p,
	unsigned char *buf,		/* Where to read it to */
	int addr,				/* Address in EEprom to read from */
	int size				/* Number of bytes to read (max 65535) */
) {
	i1proimp *m = (i1proimp *)p->m;
	int rwbytes;			/* Data bytes read or written */
	unsigned char pbuf[8];	/* Write EEprom parameters */
	int len = 8;			/* Message length */
	int se, rv = I1PRO_OK;
	int stime;

	if (size >= 0x10000)
		return I1PRO_INT_EETOOBIG;

	a1logd(p->log,2,"i1pro_readEEProm: address 0x%x size 0x%x @ %d msec\n",
	                           addr, size, (stime = msec_time()) - m->msec);

	int2buf(&pbuf[0], addr);
	short2buf(&pbuf[4], size);
	pbuf[6] = pbuf[7] = 0;		/* Ignored */

	if (p->itype == instI1Pro2)
		len = 6;

  	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xC4, 0, 0, pbuf, len, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro_readEEProm: read failed with ICOM err 0x%x\n",se);
		return rv;
	}

	/* Now read the bytes */
	se = p->icom->usb_read(p->icom, NULL, 0x82, buf, size, &rwbytes, 5.0);
	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro_readEEProm: read failed with ICOM err 0x%x\n",se);
		return rv;
	}

	if (rwbytes != size) {
		a1logd(p->log,1,"i1pro_readEEProm: 0x%x bytes, short read error\n",rwbytes);
		return I1PRO_HW_EE_SHORTREAD;
	}

	if (p->log->debug >= 7) {
		int i;
		char oline[100], *bp = oline;
		for (i = 0; i < size; i++) {
			if ((i % 16) == 0)
				bp += sprintf(bp,"    %04x:",i);
			bp += sprintf(bp," %02x",buf[i]);
			if ((i+1) >= size || ((i+1) % 16) == 0) {
				bp += sprintf(bp,"\n");
				a1logd(p->log,7,oline);
				bp = oline;
			}
		}
	}

	a1logd(p->log,2,"i1pro_readEEProm: 0x%x bytes, ICOM err 0x%x (%d msec)\n",
	                                          rwbytes, se, msec_time()-stime);

	return rv;
}

/* Write to the EEProm */
i1pro_code
i1pro_writeEEProm(
	i1pro *p,
	unsigned char *buf,		/* Where to write from */
	int addr,				/* Address in EEprom to write to */
	int size				/* Number of bytes to write (max 65535) */
) {
	i1proimp *m = (i1proimp *)p->m;
	int rwbytes;			/* Data bytes read or written */
	unsigned char pbuf[8];	/* Write EEprom parameters */
	int len = 8;			/* Message length */
	int se = 0, rv = I1PRO_OK;
	int i;
	int stime;

	/* Don't write over fixed values, as the instrument could become unusable.. */ 
	if (addr < 0 || addr > 0x1000 || (addr + size) >= 0x1000)
		return I1PRO_INT_EETOOBIG;

	a1logd(p->log,2,"i1pro_writeEEProm: address 0x%x size 0x%x @ %d msec\n",
	                             addr,size, (stime = msec_time()) - m->msec);

	if (p->log->debug >= 6) {
		int i;
		char oline[100], *bp = oline;
		for (i = 0; i < size; i++) {
			if ((i % 16) == 0)
				bp += sprintf(bp,"    %04x:",i);
			bp += sprintf(bp," %02x",buf[i]);
			if ((i+1) >= size || ((i+1) % 16) == 0) {
				bp += sprintf(bp,"\n");
				a1logd(p->log,6,oline);
				bp = oline;
			}
		}
	}

#ifdef ENABLE_WRITE
	int2buf(&pbuf[0], addr);
	short2buf(&pbuf[4], size);
	short2buf(&pbuf[6], 0x100);		/* Might be accidental, left over from getmisc.. */

	if (p->itype == instI1Pro2)
		len = 6;

  	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xC3, 0, 0, pbuf, len, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,2,"i1pro_writeEEProm: write failed with ICOM err 0x%x\n",se);
		return rv;
	}

	/* Now write the bytes */
	se = p->icom->usb_write(p->icom, NULL, 0x03, buf, size, &rwbytes, 5.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro_writeEEProm: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	if (rwbytes != size) {
		a1logd(p->log,1,"i1pro_writeEEProm: 0x%x bytes, short write error\n",rwbytes);
		return I1PRO_HW_EE_SHORTWRITE;
	}

	/* Now we write two separate bytes of 0 - confirm write ?? */
	for (i = 0; i < 2; i++) {
		pbuf[0] = 0;

		/* Now write the bytes */
		se = p->icom->usb_write(p->icom, NULL, 0x03, pbuf, 1, &rwbytes, 5.0);

		if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
			a1logd(p->log,1,"i1pro_writeEEProm: write failed with ICOM err 0x%x\n",se);
			return rv;
		}

		if (rwbytes != 1) {
			a1logd(p->log,1,"i1pro_writeEEProm: 0x%x bytes, short write error\n",rwbytes);
			return I1PRO_HW_EE_SHORTWRITE;
		}
	}
	a1logd(p->log,2,"i1pro_writeEEProm: 0x%x bytes, ICOM err 0x%x (%d msec)\n",
	                   size, se, msec_time()-stime);

	/* The instrument needs some recovery time after a write */
	msec_sleep(50);

#else /* ENABLE_WRITE */

	a1logd(p->log,2,"i1pro_writeEEProm: (NOT) 0x%x bytes, ICOM err 0x%x\n",size, se);

#endif /* ENABLE_WRITE */

	return rv;
}

/* Get the miscellaneous status */
/* return pointers may be NULL if not needed. */
i1pro_code
i1pro_getmisc(
	i1pro *p,
	int *fwrev,		/* Return the hardware version number */
	int *unkn1,		/* Unknown status, set after doing a measurement */
	int *maxpve,	/* Maximum positive value in sensor readings */
	int *unkn3,		/* Unknown status, usually 1 */
	int *powmode	/* 0 = high power mode, 8 = low power mode */
) {
	i1proimp *m = (i1proimp *)p->m;
	unsigned char pbuf[8];	/* status bytes read */
	int _fwrev;
	int _unkn1;
	int _maxpve;
	int _unkn3;
	int _powmode;
	int se, rv = I1PRO_OK;
	int stime = 0;

	a1logd(p->log,2,"i1pro_getmisc: @ %d msec\n",(stime = msec_time()) - m->msec);

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xC9, 0, 0, pbuf, 8, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro_getmisc: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	_fwrev   = buf2ushort(&pbuf[0]);
	_unkn1   = buf2ushort(&pbuf[2]);	/* Value set after each read. Average ?? */
	_maxpve  = buf2ushort(&pbuf[4]);
	_unkn3   = pbuf[6];		/* Flag values are tested, but don't seem to be used ? */
	_powmode = pbuf[7];

	a1logd(p->log,2,"i1pro_getmisc: returning %d, 0x%04x, 0x%04x, 0x%02x, 0x%02x ICOM err 0x%x (%d msec)\n",
	                   _fwrev, _unkn1, _maxpve, _unkn3, _powmode, se, msec_time()-stime);

	if (fwrev != NULL) *fwrev = _fwrev;
	if (unkn1 != NULL) *unkn1 = _unkn1;
	if (maxpve != NULL) *maxpve = _maxpve;
	if (unkn3 != NULL) *unkn3 = _unkn3;
	if (powmode != NULL) *powmode = _powmode;

	return rv;
}

/* Get the current measurement parameters */
/* Return pointers may be NULL if not needed. */
i1pro_code
i1pro_getmeasparams(
	i1pro *p,
	int *intclocks,		/* Number of integration clocks */
	int *lampclocks,	/* Number of lamp turn on sub-clocks */
	int *nummeas,		/* Number of measurements */
	int *measmodeflags	/* Measurement mode flags */
) {
	i1proimp *m = (i1proimp *)p->m;
	unsigned char pbuf[8];	/* status bytes read */
	int _intclocks;
	int _lampclocks;
	int _nummeas;
	int _measmodeflags;
	int se, rv = I1PRO_OK;
	int stime = 0;

	a1logd(p->log,2,"i1pro_getmeasparams: @ %d msec\n", (stime = msec_time()) - m->msec);

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xC2, 0, 0, pbuf, 8, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro_getmeasparams: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	_intclocks     = buf2ushort(&pbuf[0]);
	_lampclocks    = buf2ushort(&pbuf[2]);
	_nummeas       = buf2ushort(&pbuf[4]);
	_measmodeflags = pbuf[6];

	a1logd(p->log,2,"i1pro_getmeasparams: returning %d, %d, %d, 0x%02x ICOM err 0x%x (%d msec)\n",
	                   _intclocks, _lampclocks, _nummeas, _measmodeflags, se, msec_time()-stime);

	if (intclocks != NULL) *intclocks = _intclocks;
	if (lampclocks != NULL) *lampclocks = _lampclocks;
	if (nummeas != NULL) *nummeas = _nummeas;
	if (measmodeflags != NULL) *measmodeflags = _measmodeflags;

	return rv;
}

/* Set the current measurement parameters */
/* Return pointers may be NULL if not needed. */
/* Quirks:

	Rev. A upgrade:
	Rev. B:
		Appears to have a bug where the measurement time
		is the sum of the previous measurement plus the current measurement.
		It doesn't seem to alter the integration time though.
		There is no obvious way of fixing this (ie. reseting the instrument
		doesn't work).

	Rev. D:
		It appears that setting intclocks to 0, toggles to/from
		a half clock speed mode. (?)
*/

i1pro_code
i1pro_setmeasparams(
	i1pro *p,
	int intclocks,		/* Number of integration clocks */
	int lampclocks,		/* Number of lamp turn on sub-clocks */
	int nummeas,		/* Number of measurements */
	int measmodeflags	/* Measurement mode flags */
) {
	i1proimp *m = (i1proimp *)p->m;
	unsigned char pbuf[8];	/* command bytes written */
	int se, rv = I1PRO_OK;
	int stime = 0;

	a1logd(p->log,2,"i1pro_setmeasparams: %d, %d, %d, 0x%02x @ %d msec\n",
	                   intclocks, lampclocks, nummeas, measmodeflags,
	                   (stime = msec_time()) - m->msec);

	short2buf(&pbuf[0], intclocks);
	short2buf(&pbuf[2], lampclocks);
	short2buf(&pbuf[4], nummeas);
	pbuf[6] = measmodeflags;
	pbuf[7] = 0;

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xC1, 0, 0, pbuf, 8, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro_setmeasparams: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	a1logd(p->log,2,"i1pro_setmeasparams: returning ICOM err 0x%x (%d msec)\n",
	                                                     se,msec_time()-stime);
	return rv;
}

/* Delayed trigger implementation, called from thread */
static int
i1pro_delayed_trigger(void *pp) {
	i1pro *p = (i1pro *)pp;
	i1proimp *m = (i1proimp *)p->m;
	int se, rv = I1PRO_OK;
	int stime = 0;

	if ((m->c_measmodeflags & I1PRO_MMF_NOLAMP) == 0) {		/* Lamp will be on for measurement */
		m->llampoffon = msec_time();						/* Record when it turned on */
//		printf("~1 got lamp off -> on at %d (%f)\n",m->llampoffon, (m->llampoffon - m->llamponoff)/1000.0);

	}

	a1logd(p->log,2,"i1pro_delayed_trigger: start sleep @ %d msec\n", msec_time() - m->msec);

#ifdef USE_RD_SYNC
	p->icom->usb_wait_io(p->icom, &m->rd_sync);		/* Wait for read to start */
#else
	/* Delay the trigger */
	msec_sleep(m->trig_delay);
#endif

	m->tr_t1 = msec_time();		/* Diagnostic */

	a1logd(p->log,2,"i1pro_delayed_trigger: trigger @ %d msec\n",(stime = msec_time()) - m->msec);

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xC0, 0, 0, NULL, 0, 2.0);
	m->trigstamp = usec_time();
	m->tr_t2 = msec_time();		/* Diagnostic */

	m->trig_se = se;
	m->trig_rv = icoms2i1pro_err(se);

	a1logd(p->log,2,"i1pro_delayed_trigger: returning ICOM err 0x%x (%d msec)\n",
	                                                       se,msec_time()-stime);

	return 0;
}

/* Trigger a measurement after the nominated delay */
/* The actual return code will be in m->trig_rv after the delay. */
/* This allows us to start the measurement read before the trigger, */
/* ensuring that process scheduling latency can't cause the read to fail. */
i1pro_code
i1pro_triggermeasure(i1pro *p, int delay) {
	i1proimp *m = (i1proimp *)p->m;
	int rv = I1PRO_OK;

	a1logd(p->log,2,"i1pro_triggermeasure: trigger after %dmsec delay @ %d msec\n",
	                                                 delay, msec_time() - m->msec);

	/* NOTE := would be better here to create thread once, and then trigger it */
	/* using a condition variable. */
	if (m->trig_thread != NULL) {
		m->trig_thread->del(m->trig_thread);
		m->trig_thread = NULL;
	}

    m->tr_t1 = m->tr_t2 = m->tr_t3 = m->tr_t4 = m->tr_t5 = m->tr_t6 = m->tr_t7 = 0;
	m->trig_delay = delay;

	if ((m->trig_thread = new_athread(i1pro_delayed_trigger, (void *)p)) == NULL) {
		a1logd(p->log,1,"i1pro_triggermeasure: creating delayed trigger thread failed\n");
		return I1PRO_INT_THREADFAILED;
	}

#ifdef WAIT_FOR_DELAY_TRIGGER	/* hack to diagnose threading problems */
	while (m->tr_t2 == 0) {
		Sleep(1);
	}
#endif
	a1logd(p->log,2,"i1pro_triggermeasure: scheduled triggering OK\n");

	return rv;
}

/* Read a measurements results. */
/* A buffer full of bytes is returned. */
/* (This will fail on a Rev. A if there is more than about a 40 msec delay */
/* between triggering the measurement and starting this read. */
/* It appears that the read can be pending before triggering though. */
/* Scan reads will also terminate if there is too great a delay beteween each read.) */
static i1pro_code
i1pro_readmeasurement(
	i1pro *p,
	int inummeas,			/* Initial number of measurements to expect */
	int scanflag,			/* NZ if in scan mode to continue reading */
	unsigned char *buf,		/* Where to read it to */
	int bsize,				/* Bytes available in buffer */
	int *nummeas,			/* Return number of readings measured */
	i1p_mmodif mmodif		/* Measurement modifier enum */
) {
	i1proimp *m = (i1proimp *)p->m;
	unsigned char *ibuf = buf;	/* Incoming buffer */
	int nmeas;					/* Number of measurements for this read */
	double top, extra;			/* Time out period */
	int rwbytes;				/* Data bytes read or written */
	int se, rv = I1PRO_OK;
	int treadings = 0;
	int stime = 0;
//	int gotshort = 0;			/* nz when got a previous short reading */

	if ((bsize % (m->nsen * 2)) != 0) {
		return I1PRO_INT_ODDREADBUF;
	}

	a1logd(p->log,2,"i1pro_readmeasurement: inummeas %d, scanflag %d, address %p bsize 0x%x "
	          "@ %d msec\n",inummeas, scanflag, buf, bsize, (stime = msec_time()) - m->msec);

	extra = 2.0;		/* Extra timeout margin */

	/* Deal with Rev A+ & Rev B quirk: */
	if ((m->fwrev >= 200 && m->fwrev < 300)
	 || (m->fwrev >= 300 && m->fwrev < 400))
		extra += m->l_inttime;
	m->l_inttime = m->c_inttime;

#ifdef SINGLE_READ
	if (scanflag == 0)
		nmeas = inummeas;
	else
		nmeas = bsize / (m->nsen * 2);		/* Use a single large read */
#else
	nmeas = inummeas;				/* Smaller initial number of measurements */
#endif

	top = extra + m->c_inttime * nmeas;
	if ((m->c_measmodeflags & I1PRO_MMF_NOLAMP) == 0)	/* Lamp is on */
		top += m->c_lamptime;

	/* NOTE :- for a scan on Rev. A, if we don't read fast enough the Eye-One will */
	/* assume it should stop sending, even though the user has the switch pressed. */
	/* For the rev A, this is quite a small margin (aprox. 1 msec ?) */
	/* The Rev D has a lot more buffering, and is quite robust. */
	/* By using the delayed trigger and a single read, this problem is usually */
	/* eliminated. */
	/* An unexpected short read seems to lock the instrument up. Not currently */
	/* sure what sequence would recover it for a retry of the read. */
	for (;;) {
		int size;		/* number of bytes to read */

		size = m->nsen * 2 * nmeas;

		if (size > bsize) {		/* oops, no room for read */
			a1logd(p->log,1,"i1pro_readmeasurement: buffer was too short for scan\n");
			return I1PRO_INT_MEASBUFFTOOSMALL;
		}

		m->tr_t6 = msec_time();	/* Diagnostic, start of subsequent reads */
		if (m->tr_t3 == 0) m->tr_t3 = m->tr_t6;		/* Diagnostic, start of first read */

		se = p->icom->usb_read(p->icom, &m->rd_sync, 0x82, buf, size, &rwbytes, top);

		m->tr_t5 = m->tr_t7;
		m->tr_t7 = msec_time();	/* Diagnostic, end of subsequent reads */
		if (m->tr_t4 == 0) {
			m->tr_t5 = m->tr_t2;
			m->tr_t4 = m->tr_t7;	/* Diagnostic, end of first read */
		}

#ifdef NEVER		/* Use short + timeout to terminate scan */
		if (gotshort != 0 && se == ICOM_TO) {	/* We got a timeout after a short read. */ 
			a1logd(p->log,2,"i1pro_readmeasurement: timed out in %f secs after getting short read\n",top);
			a1logd(p->log,2,"i1pro_readmeasurement: trig & rd times %d %d %d %d)\n",
			    m->tr_t2-m->tr_t1, m->tr_t3-m->tr_t2, m->tr_t4-m->tr_t3, m->tr_t6-m->tr_t5);
			break;		/* We're done */
		} else
#endif
		  if (se == ICOM_SHORT) {		/* Expect this to terminate scan reading */
			a1logd(p->log,2,"i1pro_readmeasurement: short read, read %d bytes, asked for %d\n",
			                                                                     rwbytes,size);
			a1logd(p->log,2,"i1pro_readmeasurement: trig & rd times %d %d %d %d)\n",
			   m->tr_t2-m->tr_t1, m->tr_t3-m->tr_t2, m->tr_t4-m->tr_t3, m->tr_t6-m->tr_t5);
		} else if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
			if (m->trig_rv != I1PRO_OK) {
				a1logd(p->log,1,"i1pro_readmeasurement: trigger failed, ICOM err 0x%x\n",
				                                                             m->trig_se);
				return m->trig_rv;
			}
			if (se & ICOM_TO)
				a1logd(p->log,1,"i1pro_readmeasurement: timed out with top = %f\n",top);
			a1logd(p->log,1,"i1pro_readmeasurement: failed, bytes read 0x%x, ICOM err 0x%x\n",
			                                                                     rwbytes, se);
			return rv;
		}
 
		/* If we didn't read a multiple of m->nsen * 2, we've got problems */
		if ((rwbytes % (m->nsen * 2)) != 0) {
			a1logd(p->log,1,"i1pro_readmeasurement: read 0x%x bytes, odd read error\n",rwbytes);
			return I1PRO_HW_ME_ODDREAD;
		}

		/* Track where we're up to */
		bsize -= rwbytes;
		buf   += rwbytes;
		treadings += rwbytes/(m->nsen * 2);

		if (scanflag == 0) {	/* Not scanning */

			/* Expect to read exactly what we asked for */
			if (rwbytes != size) {
				a1logd(p->log,1,"i1pro_readmeasurement: unexpected short read, got %d expected %d\n"
				                                                                     ,rwbytes,size);
				return I1PRO_HW_ME_SHORTREAD;
			}
			break;	/* And we're done */
		}

#ifdef NEVER		/* Use short + timeout to terminate scan */
		/* We expect to get a short read at the end of a scan, */
		/* or we might have the USB transfer truncated by somethinge else. */
		/* Note the short read, and keep reading until we get a time out */
		if (rwbytes != size) {
			gotshort = 1;
		} else {
			gotshort = 0;
		}
#else				/* Use short to terminate scan */
		/* We're scanning and expect to get a short read at the end of the scan. */
		if (rwbytes != size) {
			break;
		}
#endif

		if (bsize == 0) {		/* oops, no room for more scanning read */
			unsigned char tbuf[NSEN_MAX * 2];

			/* We need to clean up, so soak up all the data and throw it away */
			while ((se = p->icom->usb_read(p->icom, NULL, 0x82, tbuf, m->nsen * 2, &rwbytes, top)) == ICOM_OK)
				;
			a1logd(p->log,1,"i1pro_readmeasurement: buffer was too short for scan\n");
			return I1PRO_INT_MEASBUFFTOOSMALL;
		}

		/* Read a bunch more readings until the read is short or times out */
		nmeas = bsize / (m->nsen * 2);
		if (nmeas > 64)
			nmeas = 64;
		top = extra + m->c_inttime * nmeas;
	}

	if ((m->c_measmodeflags & I1PRO_MMF_NOLAMP) == 0) {		/* Lamp was on for measurement */
		m->slamponoff = m->llamponoff;						/* remember second last */
		m->llamponoff = msec_time();						/* Record when it turned off */
//		printf("~1 got lamp on -> off at %d (%f)\n",m->llamponoff, (m->llamponoff - m->llampoffon)/1000.0);
		m->lampage += (m->llamponoff - m->llampoffon)/1000.0;	/* Time lamp was on */
	}
	/* Update log values */
	if (mmodif != i1p_dark_cal)
		m->meascount++;

	/* Must have timed out in initial readings */
	if (treadings < inummeas) {
		a1logd(p->log,1,"i1pro_readmeasurement: read failed, bytes read 0x%x, ICOM err 0x%x\n",
		                                                                          rwbytes, se);
		return I1PRO_RD_SHORTMEAS;
	} 

	if (p->log->debug >= 6) {
		int i, size = treadings * m->nsen * 2;
		char oline[100], *bp = oline;
		for (i = 0; i < size; i++) {
			if ((i % 16) == 0)
				bp += sprintf(bp,"    %04x:",i);
			bp += sprintf(bp," %02x",ibuf[i]);
			if ((i+1) >= size || ((i+1) % 16) == 0) {
				bp += sprintf(bp,"\n");
				a1logd(p->log,6,oline);
				bp = oline;
			}
		}
	}

	a1logd(p->log,2,"i1pro_readmeasurement: read %d readings, ICOM err 0x%x (%d msec)\n",
	                                                   treadings, se, msec_time()-stime);
	a1logd(p->log,2,"i1pro_readmeasurement: (trig & rd times %d %d %d %d)\n",
	    m->tr_t2-m->tr_t1, m->tr_t3-m->tr_t2, m->tr_t4-m->tr_t3, m->tr_t6-m->tr_t5);

	if (nummeas != NULL) *nummeas = treadings;

	return rv;
}

/* Set the measurement clock mode */
/* Firmware Version >= 301 only */
i1pro_code
i1pro_setmcmode(
	i1pro *p,
	int mcmode	/* Measurement clock mode, 1..mxmcmode */
) {
	i1proimp *m = (i1proimp *)p->m;
	unsigned char pbuf[1];	/* 1 bytes to write */
	int se, rv = I1PRO_OK;
	int stime = 0;

	a1logd(p->log,2,"i1pro_setmcmode: mode %d @ %d msec\n",
	                   mcmode, (stime = msec_time()) - m->msec);

	pbuf[0] = mcmode;
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xCF, 0, 0, pbuf, 1, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro_setmcmode: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	a1logd(p->log,2,"i1pro_setmcmode: done, ICOM err 0x%x (%d msec)\n",
	                                             se, msec_time()-stime);
	return rv;
}

/* Get the current measurement clock mode */
/* Return pointers may be NULL if not needed. */
/* Firmware Version >= 301 only */
i1pro_code
i1pro_getmcmode(
	i1pro *p,
	int *maxmcmode,		/* mcmode must be <= maxmcmode */
	int *mcmode,		/* readback current mcmode */
	int *subclkdiv,		/* Sub clock divider ratio */
	int *intclkusec,	/* Integration clock in usec */
	int *subtmode		/* Subtract mode on read using average of value 127 */
) {
	i1proimp *m = (i1proimp *)p->m;
	unsigned char pbuf[8];	/* status bytes read */
	int _maxmcmode;		/* mcmode must be < maxmcmode */
	int _mcmode;		/* readback current mcmode */
	int _unknown;		/* Unknown */
	int _subclkdiv;		/* Sub clock divider ratio */
	int _intclkusec;	/* Integration clock in usec */
	int _subtmode;		/* Subtract mode on read using average of value 127 */
	int se, rv = I1PRO_OK;
	int stime = 0;

	a1logd(p->log,2,"i1pro_getmcmode: called @ %d msec\n",
	                   (stime = msec_time()) - m->msec);

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xD1, 0, 0, pbuf, 6, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro_getmcmode: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	_maxmcmode  = pbuf[0];
	_mcmode     = pbuf[1];
	_unknown    = pbuf[2];
	_subclkdiv  = pbuf[3];
	_intclkusec = pbuf[4];
	_subtmode   = pbuf[5];

	a1logd(p->log,2,"i1pro_getmcmode: returns %d, %d, (%d), %d, %d 0x%x ICOM err 0x%x (%d msec)\n",
	      _maxmcmode, _mcmode, _unknown, _subclkdiv, _intclkusec, _subtmode, se, msec_time()-stime);

	if (maxmcmode != NULL) *maxmcmode = _maxmcmode;
	if (mcmode != NULL) *mcmode = _mcmode;
	if (subclkdiv != NULL) *subclkdiv = _subclkdiv;
	if (intclkusec != NULL) *intclkusec = _intclkusec;
	if (subtmode != NULL) *subtmode = _subtmode;

	return rv;
}


/* Wait for a reply triggered by an instrument switch press */
i1pro_code i1pro_waitfor_switch(i1pro *p, double top) {
	i1proimp *m = (i1proimp *)p->m;
	int rwbytes;			/* Data bytes read */
	unsigned char buf[8];	/* Result  */
	int se, rv = I1PRO_OK;
	int stime = 0;

	a1logd(p->log,2,"i1pro_waitfor_switch: read 1 byte from switch hit port @ %d msec\n",
	                                                     (stime = msec_time()) - m->msec);

	/* Now read 1 byte */
	se = p->icom->usb_read(p->icom, NULL, 0x84, buf, 1, &rwbytes, top);

	if (se & ICOM_TO) {
		a1logd(p->log,2,"i1pro_waitfor_switch: read 0x%x bytes, timed out (%d msec)\n",
		                   rwbytes,msec_time()-stime);
		return I1PRO_INT_BUTTONTIMEOUT;
	}

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro_waitfor_switch: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	if (rwbytes != 1) {
		a1logd(p->log,1,"i1pro_waitfor_switch: read 0x%x bytes, short read error (%d msec)\n",
		                                                          rwbytes,msec_time()-stime);
		return I1PRO_HW_SW_SHORTREAD;
	}

	a1logd(p->log,2,"i1pro_waitfor_switch: read 0x%x bytes value 0x%x ICOM err 0x%x (%d msec)\n",
		                                                 rwbytes, buf[0], se, msec_time()-stime);

	return rv; 
}

/* Wait for a reply triggered by a key press (thread version) */
/* Returns I1PRO_OK if the switch has been pressed, */
/* or I1PRO_INT_BUTTONTIMEOUT if */
/* no switch was pressed befor the time expired, */
/* or some other error. */
i1pro_code i1pro_waitfor_switch_th(i1pro *p, double top) {
	i1proimp *m = (i1proimp *)p->m;
	int rwbytes;			/* Data bytes read */
	unsigned char buf[8];	/* Result  */
	int se, rv = I1PRO_OK;
	int stime = 0;

	a1logd(p->log,2,"i1pro_waitfor_switch_th: read 1 byte from switch hit port @ %d msec\n",
	                                                      (stime = msec_time()) - m->msec);

	/* Now read 1 byte */
	se = p->icom->usb_read(p->icom, &m->sw_cancel, 0x84, buf, 1, &rwbytes, top);

	if (se & ICOM_TO) {
		a1logd(p->log,2,"i1pro_waitfor_switch_th: read 0x%x bytes, timed out (%d msec)\n",
		                                                      rwbytes,msec_time()-stime);
		return I1PRO_INT_BUTTONTIMEOUT;
	}

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,2,"i1pro_waitfor_switch_th: failed with ICOM err 0x%x (%d msec)\n",
		                                                           se,msec_time()-stime);
		return rv;
	}

	if (rwbytes != 1) {
		a1logd(p->log,2,"i1pro_waitfor_switch_th: read 0x%x bytes, short read error (%d msec)\n",
		                                                              rwbytes,msec_time()-stime);
		return I1PRO_HW_SW_SHORTREAD;
	}

	a1logd(p->log,2,"i1pro_waitfor_switch_th: read 0x%x bytes value 0x%x ICOM err 0x%x (%d msec)\n",
		                                                   rwbytes, buf[0], se,msec_time()-stime);

	return rv; 
}

/* Terminate switch handling */
/* This seems to always return an error ? */
i1pro_code
i1pro_terminate_switch(
	i1pro *p
) {
	i1proimp *m = (i1proimp *)p->m;
	unsigned char pbuf[8];	/* 8 bytes to write */
	int se, rv = I1PRO_OK;

	a1logd(p->log,2,"i1pro_terminate_switch: called\n");

	/* These values may not be significant */
	pbuf[0] = pbuf[1] = pbuf[2] = pbuf[3] = 0xff;
	pbuf[4] = 0xfc;
	pbuf[5] = 0xee;
	pbuf[6] = 0x12;
	pbuf[7] = 0x00;
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xD0, 3, 0, pbuf, 8, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,2,"i1pro_terminate_switch: Warning: Terminate Switch Handling failed with ICOM err 0x%x\n",se);
	} else {
		a1logd(p->log,2,"i1pro_terminate_switch: done, ICOM err 0x%x\n",se);
	}

	/* In case the above didn't work, cancel the I/O */
	msec_sleep(50);
	if (m->th_termed == 0) {
		a1logd(p->log,3,"i1pro terminate switch thread failed, canceling I/O\n");
		p->icom->usb_cancel_io(p->icom, &m->sw_cancel);
	}
	
	return rv;
}

/* ============================================================ */
/* Low level i1pro2 (Rev E) commands */

/* Get the EEProm size */
i1pro_code
i1pro2_geteesize(
    i1pro *p,
    int *eesize
) {
	int se, rv = I1PRO_OK;
	unsigned char buf[4];	/* Result  */
	int _eesize = 0;

	a1logd(p->log,2,"i1pro2_geteesize: called\n");

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xD9, 0, 0, buf, 4, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro2_geteesize: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	_eesize = buf2int(buf);

	a1logd(p->log,2,"i1pro2_geteesize: returning %d ICOM err 0x%x\n", _eesize, se);

	if (eesize != NULL)
		*eesize = _eesize;

	return rv;
}

/* Get the Chip ID */
/* This does actually work with the Rev D. */
/* (It returns all zero's unless you've read the EEProm first !) */
i1pro_code
i1pro2_getchipid(
	i1pro *p,
	unsigned char chipid[8]
) {
	int se, rv = I1PRO_OK;

	a1logd(p->log,2,"i1pro2_getchipid: called\n");

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xD2, 0, 0, chipid, 8, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro2_getchipid: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	a1logd(p->log,2,"i1pro2_getchipid: returning %02X-%02X%02X%02X%02X%02X%02X%02X ICOM err 0x%x\n",
                           chipid[0], chipid[1], chipid[2], chipid[3],
                           chipid[4], chipid[5], chipid[6], chipid[7], se);
	return rv;
}

/* Get Rev E measure characteristics. */
i1pro_code
i1pro2_getmeaschar(
    i1pro *p,
    int *clkusec,		/* Return integration clock length in usec ?      (ie. 36)  */
    int *xraw,			/* Return number of extra non-reading (dark) raw bands ? (ie. 6)   */
    int *nraw,			/* Return number of reading raw bands ?           (ie. 128) */
    int *subdiv			/* Sub divider and minium integration clocks  ?   (ie. 136) */
) {
	int se, rv = I1PRO_OK;
	unsigned char buf[16];	/* Result  */
    int _clkusec;
    int _xraw;
    int _nraw;
    int _subdiv;

	a1logd(p->log,2,"i1pro2_getmeaschar: called\n");

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xD5, 0, 0, buf, 16, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro2_getmeaschar: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	_clkusec = buf2int(buf + 0);
	_xraw = buf2int(buf + 4);
	_nraw = buf2int(buf + 8);
	_subdiv = buf2int(buf + 12);

	a1logd(p->log,2,"i1pro2_getmeaschar: returning clkusec %d, xraw %d, nraw %d, subdiv %d ICOM err 0x%x\n", _clkusec, _xraw, _nraw, _subdiv, se);

	if (clkusec != NULL)
		*clkusec = _clkusec;
	if (xraw != NULL)
		*xraw = _xraw;
	if (nraw != NULL)
		*nraw = _nraw;
	if (subdiv != NULL)
		*subdiv = _subdiv;

	return rv;
}

/* Delayed trigger implementation, called from thread */
/* We assume that the Rev E measurement parameters have been set in */
/* the i1proimp structure c_* values */
static int
i1pro2_delayed_trigger(void *pp) {
	i1pro *p = (i1pro *)pp;
	i1proimp *m = (i1proimp *)p->m;
	unsigned char pbuf[14];	/* 14 bytes to write */
	int se, rv = I1PRO_OK;
	int stime = 0;

	int2buf(pbuf + 0, m->c_intclocks); 
	int2buf(pbuf + 4, m->c_lampclocks); 
	int2buf(pbuf + 8, m->c_nummeas); 
	short2buf(pbuf + 12, m->c_measmodeflags2); 

	if ((m->c_measmodeflags & I1PRO_MMF_NOLAMP) == 0) {		/* Lamp will be on for measurement */
		m->llampoffon = msec_time();						/* Record when it turned on */
	}

	a1logd(p->log,2,"i1pro2_delayed_trigger: Rev E start sleep @ %d msec\n",
	                                                 msec_time() - m->msec);
#ifdef USE_RD_SYNC
	p->icom->usb_wait_io(p->icom, &m->rd_sync);		/* Wait for read to start */
#else
	/* Delay the trigger */
	msec_sleep(m->trig_delay);
#endif

	m->tr_t1 = msec_time();		/* Diagnostic */

	a1logd(p->log,2,"i1pro2_delayed_trigger: trigger Rev E @ %d msec\n",
	                                     (stime = msec_time()) - m->msec);

	m->trigstamp = usec_time();
	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xD4, 0, 0, pbuf, 14, 2.0);

	m->tr_t2 = msec_time();		/* Diagnostic */

	m->trig_se = se;
	m->trig_rv = icoms2i1pro_err(se);

	a1logd(p->log,2,"i1pro2_delayed_trigger: done ICOM err 0x%x (%d msec)\n",
	                                                  se,msec_time()-stime);
	return 0;
}

/* Trigger a measurement after the nominated delay */
/* The actual return code will be in m->trig_rv after the delay. */
/* This allows us to start the measurement read before the trigger, */
/* ensuring that process scheduling latency can't cause the read to fail. */
i1pro_code
i1pro2_triggermeasure(i1pro *p, int delay) {
	i1proimp *m = (i1proimp *)p->m;
	int rv = I1PRO_OK;

	a1logd(p->log,2,"i1pro2_triggermeasure: triggering Rev Emeasurement after %dmsec "
	                               "delay @ %d msec\n", delay, msec_time() - m->msec);

	/* NOTE := would be better here to create thread once, and then trigger it */
	/* using a condition variable. */
	if (m->trig_thread != NULL)
		m->trig_thread->del(m->trig_thread);

    m->tr_t1 = m->tr_t2 = m->tr_t3 = m->tr_t4 = m->tr_t5 = m->tr_t6 = m->tr_t7 = 0;
	m->trig_delay = delay;

	if ((m->trig_thread = new_athread(i1pro2_delayed_trigger, (void *)p)) == NULL) {
		a1logd(p->log,1,"i1pro2_triggermeasure: creating delayed trigger Rev E thread failed\n");
		return I1PRO_INT_THREADFAILED;
	}

#ifdef WAIT_FOR_DELAY_TRIGGER	/* hack to diagnose threading problems */
	while (m->tr_t2 == 0) {
		Sleep(1);
	}
#endif
	a1logd(p->log,2,"i1pro2_triggermeasure: scheduled triggering Rev E OK\n");

	return rv;
}

/* Get the UV before and after measurement voltage drop */
i1pro_code
i1pro2_getUVvolts(
    i1pro *p,
    int *before,
    int *after
) {
	int se, rv = I1PRO_OK;
	unsigned char buf[4];	/* Result  */
	int _before = 0;
	int _after = 0;

	a1logd(p->log,2,"i1pro2_getUVvolts: called\n");

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xD8, 0, 0, buf, 4, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro2_getUVvolts: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	_before = buf2ushort(buf);
	_after = buf2ushort(buf+2);

	a1logd(p->log,2,"i1pro2_getUVvolts: returning %d, %d ICOM err 0x%x\n", _before, _after, se);

	if (before != NULL)
		*before = _before;

	if (after != NULL)
		*after = _after;

	return rv;
}

/* Terminate Ruler tracking (???) */
/* The parameter seems to be always 0 ? */
static int
i1pro2_stop_ruler(void *pp, int parm) {
	i1pro *p = (i1pro *)pp;
	i1proimp *m = (i1proimp *)p->m;
	unsigned char pbuf[2];	/* 2 bytes to write */
	int se, rv = I1PRO_OK;

	short2buf(pbuf, parm); 

	a1logd(p->log,2,"i1pro2_stop_ruler: called with 0x%x\n", parm);

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xD7, 0, 0, pbuf, 2, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro2_stop_ruler: failed with ICOM err 0x%x\n",rv);
		return rv;
	}

	a1logd(p->log,2,"i1pro2_stop_ruler: returning ICOM err 0x%x\n",rv);

	return rv;
}

	/* Send a raw indicator LED sequence. */
/*
	The byte sequence has the following format:
	(all values are big endian)

	XXXX	Number of following blocks, BE.

	Blocks are:

	YYYY	Number of bytes in the block
	RRRR	Number of repeats of the block, FFFFFFFF = infinite

	A sequence of codes:

	LL TTTT
			L = Led mask:
				01 = Red Right
				02 = Green Right
				04 = Blue Right
				08 = Red Left
				10 = Green Left
				20 = Blue Left
			TTT = clock count for this mask (ie. aprox. 73 usec clock period)
	PWM typically alternates between on & off state with a period total of
	0x50 clocks = 170 Hz. 255 clocks would be 54 Hz.

 */

static int
i1pro2_indLEDseq(void *pp, unsigned char *buf, int size) {
	i1pro *p = (i1pro *)pp;
	i1proimp *m = (i1proimp *)p->m;
	int rwbytes;			/* Data bytes written */
	unsigned char pbuf[4];	/* Number of bytes being send */
	int se, rv = I1PRO_OK;

	int2buf(pbuf, size); 

	a1logd(p->log,2,"i1pro2_indLEDseq: length %d bytes\n", size);

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0xD6, 0, 0, pbuf, 4, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro2_indLEDseq: failed with ICOM err 0x%x\n",rv);
		return rv;
	}

	a1logd(p->log,2,"i1pro2_geteesize: command got ICOM err 0x%x\n", se);

	/* Now write the bytes */
	se = p->icom->usb_write(p->icom, NULL, 0x03, buf, size, &rwbytes, 5.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		a1logd(p->log,1,"i1pro2_indLEDseq: data write failed with ICOM err 0x%x\n",se);
		return rv;
	}

	if (rwbytes != size) {
		a1logd(p->log,1,"i1pro2_indLEDseq: wrote 0x%x bytes, short write error\n",rwbytes);
		return I1PRO_HW_LED_SHORTWRITE;
	}

	a1logd(p->log,2,"i1pro2_indLEDseq: wrote 0x%x bytes LED sequence, ICOM err 0x%x\n", size, rv);

	return rv;
}

/* Turn indicator LEDs off */
static int
i1pro2_indLEDoff(void *pp) {
	i1pro *p = (i1pro *)pp;
	int rv = I1PRO_OK;
	unsigned char seq[] = {
		0x00, 0x00, 0x00, 0x01,

		0x00, 0x00, 0x00, 0x07,
		0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x10
	};

	a1logd(p->log,2,"i1pro2_indLEDoff: called\n");
	rv = i1pro2_indLEDseq(p, seq, sizeof(seq));
	a1logd(p->log,2,"i1pro2_indLEDoff: returning ICOM err 0x%x\n",rv);

	return rv;
}

#ifdef NEVER

	// ~~99 play with LED settings
	if (p->itype == instI1Pro2) {

		// LED is capable of white and red

		/* Turns it off */
		unsigned char b1[] = {
			0x00, 0x00, 0x00, 0x01,

			0x00, 0x00, 0x00, 0x07,
			0x00, 0x00, 0x00, 0x01,
			0x00, 0x00, 0x01
		};

		/* Makes it white */
		unsigned char b2[] = {
			0x00, 0x00, 0x00, 0x02,

			0x00, 0x00, 0x00, 0x0a,
			0x00, 0x00, 0x00, 0x01,
			0x00, 0x36, 0x00,
			0x00, 0x00, 0x01,

			0x00, 0x00, 0x00, 0x0a,
			0xff, 0xff, 0xff, 0xff,
			0x3f, 0x36, 0x40,
			0x00, 0x00, 0x01
		};

		/* Makes it pulsing white */
		unsigned char b3[] = {
			0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x01, 0x00, 0x36, 0x40, 0x00,
			0x00, 0x01, 0x00, 0x00, 0x08, 0xec, 0xff, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x03, 0x00, 0x00, 0x50, 0x3f, 0x00, 0x03, 0x00, 0x00, 0x50, 0x3f, 0x00, 0x03, 0x00,
			0x00, 0x4d, 0x3f, 0x00, 0x03, 0x00, 0x00, 0x49, 0x3f, 0x00, 0x03, 0x00, 0x00, 0x45, 0x3f, 0x00,
			0x03, 0x00, 0x00, 0x42, 0x3f, 0x00, 0x03, 0x00, 0x00, 0x42, 0x3f, 0x00, 0x04, 0x00, 0x00, 0x4f,
			0x3f, 0x00, 0x04, 0x00, 0x00, 0x4d, 0x3f, 0x00, 0x04, 0x00, 0x00, 0x49, 0x3f, 0x00, 0x04, 0x00,
			0x00, 0x46, 0x3f, 0x00, 0x04, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x04, 0x00, 0x00, 0x41, 0x3f, 0x00,
			0x05, 0x00, 0x00, 0x4d, 0x3f, 0x00, 0x05, 0x00, 0x00, 0x49, 0x3f, 0x00, 0x05, 0x00, 0x00, 0x46,
			0x3f, 0x00, 0x05, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x06, 0x00, 0x00, 0x4d, 0x3f, 0x00, 0x06, 0x00,
			0x00, 0x4a, 0x3f, 0x00, 0x06, 0x00, 0x00, 0x47, 0x3f, 0x00, 0x06, 0x00, 0x00, 0x44, 0x3f, 0x00,
			0x06, 0x00, 0x00, 0x41, 0x3f, 0x00, 0x07, 0x00, 0x00, 0x49, 0x3f, 0x00, 0x07, 0x00, 0x00, 0x46,
			0x3f, 0x00, 0x07, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x08, 0x00, 0x00, 0x4a, 0x3f, 0x00, 0x07, 0x00,
			0x00, 0x3e, 0x3f, 0x00, 0x08, 0x00, 0x00, 0x44, 0x3f, 0x00, 0x09, 0x00, 0x00, 0x4a, 0x3f, 0x00,
			0x09, 0x00, 0x00, 0x47, 0x3f, 0x00, 0x09, 0x00, 0x00, 0x44, 0x3f, 0x00, 0x0a, 0x00, 0x00, 0x49,
			0x3f, 0x00, 0x09, 0x00, 0x00, 0x3f, 0x3f, 0x00, 0x09, 0x00, 0x00, 0x3d, 0x3f, 0x00, 0x0b, 0x00,
			0x00, 0x48, 0x3f, 0x00, 0x0b, 0x00, 0x00, 0x45, 0x3f, 0x00, 0x0a, 0x00, 0x00, 0x3c, 0x3f, 0x00,
			0x0c, 0x00, 0x00, 0x46, 0x3f, 0x00, 0x0c, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x0d, 0x00, 0x00, 0x46,
			0x3f, 0x00, 0x0c, 0x00, 0x00, 0x3e, 0x3f, 0x00, 0x0c, 0x00, 0x00, 0x3c, 0x3f, 0x00, 0x0d, 0x00,
			0x00, 0x3f, 0x3f, 0x00, 0x0d, 0x00, 0x00, 0x3d, 0x3f, 0x00, 0x0e, 0x00, 0x00, 0x3f, 0x3f, 0x00,
			0x0e, 0x00, 0x00, 0x3d, 0x3f, 0x00, 0x0e, 0x00, 0x00, 0x3b, 0x3f, 0x00, 0x0f, 0x00, 0x00, 0x3d,
			0x3f, 0x00, 0x0e, 0x00, 0x00, 0x37, 0x3f, 0x00, 0x0f, 0x00, 0x00, 0x39, 0x3f, 0x00, 0x10, 0x00,
			0x00, 0x3b, 0x3f, 0x00, 0x12, 0x00, 0x00, 0x40, 0x3f, 0x00, 0x10, 0x00, 0x00, 0x37, 0x3f, 0x00,
			0x13, 0x00, 0x00, 0x3f, 0x3f, 0x00, 0x13, 0x00, 0x00, 0x3d, 0x3f, 0x00, 0x11, 0x00, 0x00, 0x34,
			0x3f, 0x00, 0x12, 0x00, 0x00, 0x36, 0x3f, 0x00, 0x12, 0x00, 0x00, 0x34, 0x3f, 0x00, 0x14, 0x00,
			0x00, 0x38, 0x3f, 0x00, 0x14, 0x00, 0x00, 0x36, 0x3f, 0x00, 0x17, 0x00, 0x00, 0x3c, 0x3f, 0x00,
			0x17, 0x00, 0x00, 0x3a, 0x3f, 0x00, 0x18, 0x00, 0x00, 0x3a, 0x3f, 0x00, 0x15, 0x00, 0x00, 0x31,
			0x3f, 0x00, 0x17, 0x00, 0x00, 0x34, 0x3f, 0x00, 0x16, 0x00, 0x00, 0x30, 0x3f, 0x00, 0x1a, 0x00,
			0x00, 0x37, 0x3f, 0x00, 0x1b, 0x00, 0x00, 0x37, 0x3f, 0x00, 0x18, 0x00, 0x00, 0x2f, 0x3f, 0x00,
			0x1c, 0x00, 0x00, 0x35, 0x3f, 0x00, 0x1c, 0x00, 0x00, 0x33, 0x3f, 0x00, 0x1c, 0x00, 0x00, 0x31,
			0x3f, 0x00, 0x1d, 0x00, 0x00, 0x31, 0x3f, 0x00, 0x1b, 0x00, 0x00, 0x2c, 0x3f, 0x00, 0x1c, 0x00,
			0x00, 0x2c, 0x3f, 0x00, 0x1d, 0x00, 0x00, 0x2c, 0x3f, 0x00, 0x1e, 0x00, 0x00, 0x2c, 0x3f, 0x00,
			0x1d, 0x00, 0x00, 0x29, 0x3f, 0x00, 0x1e, 0x00, 0x00, 0x29, 0x3f, 0x00, 0x23, 0x00, 0x00, 0x2e,
			0x3f, 0x00, 0x22, 0x00, 0x00, 0x2b, 0x3f, 0x00, 0x25, 0x00, 0x00, 0x2d, 0x3f, 0x00, 0x24, 0x00,
			0x00, 0x2a, 0x3f, 0x00, 0x22, 0x00, 0x00, 0x26, 0x3f, 0x00, 0x27, 0x00, 0x00, 0x2a, 0x3f, 0x00,
			0x22, 0x00, 0x00, 0x23, 0x3f, 0x00, 0x23, 0x00, 0x00, 0x23, 0x3f, 0x00, 0x2a, 0x00, 0x00, 0x28,
			0x3f, 0x00, 0x24, 0x00, 0x00, 0x21, 0x3f, 0x00, 0x29, 0x00, 0x00, 0x24, 0x3f, 0x00, 0x2c, 0x00,
			0x00, 0x25, 0x3f, 0x00, 0x28, 0x00, 0x00, 0x20, 0x3f, 0x00, 0x2b, 0x00, 0x00, 0x21, 0x3f, 0x00,
			0x2d, 0x00, 0x00, 0x21, 0x3f, 0x00, 0x2b, 0x00, 0x00, 0x1e, 0x3f, 0x00, 0x2a, 0x00, 0x00, 0x1c,
			0x3f, 0x00, 0x2f, 0x00, 0x00, 0x1e, 0x3f, 0x00, 0x2b, 0x00, 0x00, 0x1a, 0x3f, 0x00, 0x2d, 0x00,
			0x00, 0x1a, 0x3f, 0x00, 0x31, 0x00, 0x00, 0x1b, 0x3f, 0x00, 0x2e, 0x00, 0x00, 0x18, 0x3f, 0x00,
			0x37, 0x00, 0x00, 0x1b, 0x3f, 0x00, 0x38, 0x00, 0x00, 0x1a, 0x3f, 0x00, 0x37, 0x00, 0x00, 0x18,
			0x3f, 0x00, 0x31, 0x00, 0x00, 0x14, 0x3f, 0x00, 0x39, 0x00, 0x00, 0x16, 0x3f, 0x00, 0x3d, 0x00,
			0x00, 0x16, 0x3f, 0x00, 0x36, 0x00, 0x00, 0x12, 0x3f, 0x00, 0x3a, 0x00, 0x00, 0x12, 0x3f, 0x00,
			0x3b, 0x00, 0x00, 0x11, 0x3f, 0x00, 0x40, 0x00, 0x00, 0x11, 0x3f, 0x00, 0x3a, 0x00, 0x00, 0x0e,
			0x3f, 0x00, 0x3b, 0x00, 0x00, 0x0d, 0x3f, 0x00, 0x3c, 0x00, 0x00, 0x0c, 0x3f, 0x00, 0x3d, 0x00,
			0x00, 0x0b, 0x3f, 0x00, 0x3e, 0x00, 0x00, 0x0a, 0x3f, 0x00, 0x3f, 0x00, 0x00, 0x09, 0x3f, 0x00,
			0x40, 0x00, 0x00, 0x08, 0x3f, 0x00, 0x42, 0x00, 0x00, 0x07, 0x3f, 0x00, 0x44, 0x00, 0x00, 0x06,
			0x3f, 0x00, 0x47, 0x00, 0x00, 0x05, 0x3f, 0x00, 0x4b, 0x00, 0x00, 0x04, 0x3f, 0x00, 0x50, 0x00,
			0x00, 0x03, 0x3f, 0x00, 0x44, 0x00, 0x00, 0x01, 0x3f, 0x00, 0x45, 0x3f, 0x00, 0x45, 0x3f, 0x00,
			0x45, 0x3f, 0x00, 0x45, 0x3f, 0x00, 0x44, 0x00, 0x00, 0x01, 0x3f, 0x00, 0x50, 0x00, 0x00, 0x03,
			0x3f, 0x00, 0x4b, 0x00, 0x00, 0x04, 0x3f, 0x00, 0x47, 0x00, 0x00, 0x05, 0x3f, 0x00, 0x44, 0x00,
			0x00, 0x06, 0x3f, 0x00, 0x42, 0x00, 0x00, 0x07, 0x3f, 0x00, 0x40, 0x00, 0x00, 0x08, 0x3f, 0x00,
			0x3f, 0x00, 0x00, 0x09, 0x3f, 0x00, 0x3e, 0x00, 0x00, 0x0a, 0x3f, 0x00, 0x3d, 0x00, 0x00, 0x0b,
			0x3f, 0x00, 0x3c, 0x00, 0x00, 0x0c, 0x3f, 0x00, 0x3b, 0x00, 0x00, 0x0d, 0x3f, 0x00, 0x3a, 0x00,
			0x00, 0x0e, 0x3f, 0x00, 0x40, 0x00, 0x00, 0x11, 0x3f, 0x00, 0x3b, 0x00, 0x00, 0x11, 0x3f, 0x00,
			0x3a, 0x00, 0x00, 0x12, 0x3f, 0x00, 0x36, 0x00, 0x00, 0x12, 0x3f, 0x00, 0x3d, 0x00, 0x00, 0x16,
			0x3f, 0x00, 0x39, 0x00, 0x00, 0x16, 0x3f, 0x00, 0x31, 0x00, 0x00, 0x14, 0x3f, 0x00, 0x37, 0x00,
			0x00, 0x18, 0x3f, 0x00, 0x38, 0x00, 0x00, 0x1a, 0x3f, 0x00, 0x37, 0x00, 0x00, 0x1b, 0x3f, 0x00,
			0x2e, 0x00, 0x00, 0x18, 0x3f, 0x00, 0x31, 0x00, 0x00, 0x1b, 0x3f, 0x00, 0x2d, 0x00, 0x00, 0x1a,
			0x3f, 0x00, 0x2b, 0x00, 0x00, 0x1a, 0x3f, 0x00, 0x2f, 0x00, 0x00, 0x1e, 0x3f, 0x00, 0x2a, 0x00,
			0x00, 0x1c, 0x3f, 0x00, 0x2b, 0x00, 0x00, 0x1e, 0x3f, 0x00, 0x2d, 0x00, 0x00, 0x21, 0x3f, 0x00,
			0x2b, 0x00, 0x00, 0x21, 0x3f, 0x00, 0x28, 0x00, 0x00, 0x20, 0x3f, 0x00, 0x2c, 0x00, 0x00, 0x25,
			0x3f, 0x00, 0x29, 0x00, 0x00, 0x24, 0x3f, 0x00, 0x24, 0x00, 0x00, 0x21, 0x3f, 0x00, 0x2a, 0x00,
			0x00, 0x28, 0x3f, 0x00, 0x23, 0x00, 0x00, 0x23, 0x3f, 0x00, 0x22, 0x00, 0x00, 0x23, 0x3f, 0x00,
			0x27, 0x00, 0x00, 0x2a, 0x3f, 0x00, 0x22, 0x00, 0x00, 0x26, 0x3f, 0x00, 0x24, 0x00, 0x00, 0x2a,
			0x3f, 0x00, 0x25, 0x00, 0x00, 0x2d, 0x3f, 0x00, 0x22, 0x00, 0x00, 0x2b, 0x3f, 0x00, 0x23, 0x00,
			0x00, 0x2e, 0x3f, 0x00, 0x1e, 0x00, 0x00, 0x29, 0x3f, 0x00, 0x1d, 0x00, 0x00, 0x29, 0x3f, 0x00,
			0x1e, 0x00, 0x00, 0x2c, 0x3f, 0x00, 0x1d, 0x00, 0x00, 0x2c, 0x3f, 0x00, 0x1c, 0x00, 0x00, 0x2c,
			0x3f, 0x00, 0x1b, 0x00, 0x00, 0x2c, 0x3f, 0x00, 0x1d, 0x00, 0x00, 0x31, 0x3f, 0x00, 0x1c, 0x00,
			0x00, 0x31, 0x3f, 0x00, 0x1c, 0x00, 0x00, 0x33, 0x3f, 0x00, 0x1c, 0x00, 0x00, 0x35, 0x3f, 0x00,
			0x18, 0x00, 0x00, 0x2f, 0x3f, 0x00, 0x1b, 0x00, 0x00, 0x37, 0x3f, 0x00, 0x1a, 0x00, 0x00, 0x37,
			0x3f, 0x00, 0x16, 0x00, 0x00, 0x30, 0x3f, 0x00, 0x17, 0x00, 0x00, 0x34, 0x3f, 0x00, 0x15, 0x00,
			0x00, 0x31, 0x3f, 0x00, 0x18, 0x00, 0x00, 0x3a, 0x3f, 0x00, 0x17, 0x00, 0x00, 0x3a, 0x3f, 0x00,
			0x17, 0x00, 0x00, 0x3c, 0x3f, 0x00, 0x14, 0x00, 0x00, 0x36, 0x3f, 0x00, 0x14, 0x00, 0x00, 0x38,
			0x3f, 0x00, 0x12, 0x00, 0x00, 0x34, 0x3f, 0x00, 0x12, 0x00, 0x00, 0x36, 0x3f, 0x00, 0x11, 0x00,
			0x00, 0x34, 0x3f, 0x00, 0x13, 0x00, 0x00, 0x3d, 0x3f, 0x00, 0x13, 0x00, 0x00, 0x3f, 0x3f, 0x00,
			0x10, 0x00, 0x00, 0x37, 0x3f, 0x00, 0x12, 0x00, 0x00, 0x40, 0x3f, 0x00, 0x10, 0x00, 0x00, 0x3b,
			0x3f, 0x00, 0x0f, 0x00, 0x00, 0x39, 0x3f, 0x00, 0x0e, 0x00, 0x00, 0x37, 0x3f, 0x00, 0x0f, 0x00,
			0x00, 0x3d, 0x3f, 0x00, 0x0e, 0x00, 0x00, 0x3b, 0x3f, 0x00, 0x0e, 0x00, 0x00, 0x3d, 0x3f, 0x00,
			0x0e, 0x00, 0x00, 0x3f, 0x3f, 0x00, 0x0d, 0x00, 0x00, 0x3d, 0x3f, 0x00, 0x0d, 0x00, 0x00, 0x3f,
			0x3f, 0x00, 0x0c, 0x00, 0x00, 0x3c, 0x3f, 0x00, 0x0c, 0x00, 0x00, 0x3e, 0x3f, 0x00, 0x0d, 0x00,
			0x00, 0x46, 0x3f, 0x00, 0x0c, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x0c, 0x00, 0x00, 0x46, 0x3f, 0x00,
			0x0a, 0x00, 0x00, 0x3c, 0x3f, 0x00, 0x0b, 0x00, 0x00, 0x45, 0x3f, 0x00, 0x0b, 0x00, 0x00, 0x48,
			0x3f, 0x00, 0x09, 0x00, 0x00, 0x3d, 0x3f, 0x00, 0x09, 0x00, 0x00, 0x3f, 0x3f, 0x00, 0x0a, 0x00,
			0x00, 0x49, 0x3f, 0x00, 0x09, 0x00, 0x00, 0x44, 0x3f, 0x00, 0x09, 0x00, 0x00, 0x47, 0x3f, 0x00,
			0x09, 0x00, 0x00, 0x4a, 0x3f, 0x00, 0x08, 0x00, 0x00, 0x44, 0x3f, 0x00, 0x07, 0x00, 0x00, 0x3e,
			0x3f, 0x00, 0x08, 0x00, 0x00, 0x4a, 0x3f, 0x00, 0x07, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x07, 0x00,
			0x00, 0x46, 0x3f, 0x00, 0x07, 0x00, 0x00, 0x49, 0x3f, 0x00, 0x06, 0x00, 0x00, 0x41, 0x3f, 0x00,
			0x06, 0x00, 0x00, 0x44, 0x3f, 0x00, 0x06, 0x00, 0x00, 0x47, 0x3f, 0x00, 0x06, 0x00, 0x00, 0x4a,
			0x3f, 0x00, 0x06, 0x00, 0x00, 0x4d, 0x3f, 0x00, 0x05, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x05, 0x00,
			0x00, 0x46, 0x3f, 0x00, 0x05, 0x00, 0x00, 0x49, 0x3f, 0x00, 0x05, 0x00, 0x00, 0x4d, 0x3f, 0x00,
			0x04, 0x00, 0x00, 0x41, 0x3f, 0x00, 0x04, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x04, 0x00, 0x00, 0x46,
			0x3f, 0x00, 0x04, 0x00, 0x00, 0x49, 0x3f, 0x00, 0x04, 0x00, 0x00, 0x4d, 0x3f, 0x00, 0x04, 0x00,
			0x00, 0x4f, 0x3f, 0x00, 0x03, 0x00, 0x00, 0x42, 0x3f, 0x00, 0x03, 0x00, 0x00, 0x42, 0x3f, 0x00,
			0x03, 0x00, 0x00, 0x45, 0x3f, 0x00, 0x03, 0x00, 0x00, 0x49, 0x3f, 0x00, 0x03, 0x00, 0x00, 0x4d,
			0x3f, 0x00, 0x03, 0x00, 0x00, 0x50, 0x3f, 0x00, 0x03, 0x00, 0x00, 0x50, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00,
			0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43,
			0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00, 0x00, 0x43, 0x3f, 0x00, 0x02, 0x00,
			0x00, 0x43
		};

		unsigned char b4[] = {
			0x00, 0x00, 0x00, 0x01,			// blocks

			0x00, 0x00, 0x00, 0x0a,			// bytes 
			0x00, 0x00, 0x00, 0x04,			// repeat
//			0xff, 0xff, 0xff, 0xff,			// repeat
//			0x3f, 0x00, 0x04,				// Level ????
//			0x00, 0x00, 0x3c				// msec ????
			0x3f, 0x20, 0x00,				//  "
			0x00, 0x20, 0x00				// LED mask + clocks
		};

		printf("~1 send led sequence length %d\n",sizeof(b4));
		if ((ev = i1pro2_indLEDseq(p, b4, sizeof(b4))) != I1PRO_OK)
			return ev;
	}
#endif /* NEVER */

/* ============================================================ */
/* key/value dictionary support for EEProm contents */

/* Search the linked list for the given key */
/* Return NULL if not found */
static i1keyv *i1data_find_key(i1data *d, i1key key) {
	i1keyv *k;
	
	for (k = d->head; k != NULL; k = k->next) {
		if (k->key == key)
			return k;
	}
	return NULL;
}

/* Search the linked list for the given key and */
/* return it, or add it to the list if it doesn't exist. */
/* Return NULL on error */ 
static i1keyv *i1data_make_key(i1data *d, i1key key) {
	i1keyv *k;
	
	for (k = d->head; k != NULL; k = k->next)
		if (k->key == key)
			return k;

	if ((k = (i1keyv *)calloc(1, sizeof(i1keyv))) == NULL) {
		a1logw(d->log, "i1data: malloc failed!\n");
		return NULL;
	}

	k->key = key;
	k->next = NULL;
	if (d->last == NULL) {
		d->head = d->last = k;
	} else {
		d->last->next = k;
		d->last = k;
	}
	return k;
}

/* Return type of data associated with key. Return i1_dtype_unknown if not found */
static i1_dtype i1data_get_type(i1data *d, i1key key) {
	i1keyv *k;

	if ((k = d->find_key(d, key)) != NULL)
		return k->type;
	return i1_dtype_unknown;
}

/* Return the number of data items in a keyv. Return 0 if not found */
static unsigned int i1data_get_count(i1data *d, i1key key) {
	i1keyv *k;

	if ((k = d->find_key(d, key)) != NULL)
		return k->count;
	return 0;
}

/* Return a pointer to the short data for the key. */
/* Return NULL if not found or wrong type */
static int *i1data_get_shorts(i1data *d, unsigned int *count, i1key key) {
	i1keyv *k;

	if ((k = d->find_key(d, key)) == NULL)
		return NULL;

	if (k->type != i1_dtype_short)
		return NULL;

	if (count != NULL)
		*count = k->count;

	return (int *)k->data;
}

/* Return a pointer to the int data for the key. */
/* Return NULL if not found or wrong type */
static int *i1data_get_ints(i1data *d, unsigned int *count, i1key key) {
	i1keyv *k;

	if ((k = d->find_key(d, key)) == NULL)
		return NULL;

	if (k->type != i1_dtype_int)
		return NULL;

	if (count != NULL)
		*count = k->count;

	return (int *)k->data;
}

/* Return a pointer to the double data for the key. */
/* Return NULL if not found or wrong type */
static double *i1data_get_doubles(i1data *d, unsigned int *count, i1key key) {
	i1keyv *k;

	if ((k = d->find_key(d, key)) == NULL)
		return NULL;

	if (k->type != i1_dtype_double)
		return NULL;

	if (count != NULL)
		*count = k->count;

	return (double *)k->data;
}


/* Return pointer to one of the int data for the key. */
/* Return NULL if not found or wrong type or out of range index. */
static int *i1data_get_int(i1data *d, i1key key, unsigned int index) {
	i1keyv *k;

	if ((k = d->find_key(d, key)) == NULL)
		return NULL;

	if (k->type != i1_dtype_int)
		return NULL;

	if (index >= k->count)
		return NULL;

	return ((int *)k->data) + index;
}

/* Return pointer to one of the double data for the key. */
/* Return NULL if not found or wrong type or out of range index. */
static double *i1data_get_double(i1data *d, i1key key, double *data, unsigned int index) {
	i1keyv *k;

	if ((k = d->find_key(d, key)) == NULL)
		return NULL;

	if (k->type != i1_dtype_double)
		return NULL;

	if (index >= k->count)
		return NULL;

	return ((double *)k->data) + index;
}

/* Un-serialize a char buffer into an i1key keyv */
static i1pro_code i1data_unser_shorts(
	i1data *d,
	i1key key,
	int addr,
	unsigned char *buf,
	unsigned int size
) {
	i1keyv *k;
	int i, count;

	count = size/2;

	if (count == 0)
		return I1PRO_DATA_COUNT;

	if ((k = d->make_key(d, key)) == NULL)
		return I1PRO_DATA_MAKE_KEY;

	if (k->data != NULL)
		free(k->data);

	if ((k->data = (void *)malloc(sizeof(int) * count)) == NULL)
		return I1PRO_DATA_MEMORY;

	for (i = 0; i < count; i++, buf += 2) {
		((int *)k->data)[i] = buf2short(buf);
	}

	k->count = count;
	k->size = size;
	k->type = i1_dtype_short;
	if (addr != -1)
		k->addr = addr;

	return I1PRO_OK;
}

/* Un-serialize a char buffer into an i1key keyv */
static i1pro_code i1data_unser_ints(
	i1data *d,
	i1key key,
	int addr,
	unsigned char *buf,
	unsigned int size
) {
	i1keyv *k;
	int i, count;

	count = size/4;

	if (count == 0)
		return I1PRO_DATA_COUNT;

	if ((k = d->make_key(d, key)) == NULL)
		return I1PRO_DATA_MAKE_KEY;

	if (k->data != NULL)
		free(k->data);

	if ((k->data = (void *)malloc(sizeof(int) * count)) == NULL)
		return I1PRO_DATA_MEMORY;

	for (i = 0; i < count; i++, buf += 4) {
		((int *)k->data)[i] = buf2int(buf);
	}

	k->count = count;
	k->size = size;
	k->type = i1_dtype_int;
	if (addr != -1)
		k->addr = addr;

	return I1PRO_OK;
}

/* Create an entry for an end of section marker */
static i1pro_code i1data_add_eosmarker(
	i1data *d,
	i1key key,		/* section number */
	int addr
) {
	i1keyv *k;

	if ((k = d->make_key(d, key)) == NULL)
		return I1PRO_DATA_MAKE_KEY;

	if (k->data != NULL) {
		free(k->data);
		k->data = NULL;
	}

	k->count = 0;
	k->size = 0;
	k->type = i1_dtype_section;
	if (addr != -1)
		k->addr = addr;

	return I1PRO_OK;
}

/* Un-serialize a char buffer of floats into a double keyv */
static i1pro_code i1data_unser_doubles(
	i1data *d,
	i1key key,
	int addr,
	unsigned char *buf,
	unsigned int size
) {
	i1keyv *k;
	int i, count;

	count = size/4;

	if (count == 0)
		return I1PRO_DATA_COUNT;

	if ((k = d->make_key(d, key)) == NULL)
		return I1PRO_DATA_MAKE_KEY;

	if (k->data != NULL)
		free(k->data);

	if ((k->data = (void *)malloc(sizeof(double) * count)) == NULL)
		return I1PRO_DATA_MEMORY;

	for (i = 0; i < count; i++, buf += 4) {
		int val;
		val = buf2int(buf);
		((double *)k->data)[i] = IEEE754todouble((unsigned int)val);
	}

	k->count = count;
	k->size = size;
	k->type = i1_dtype_double;
	if (addr != -1)
		k->addr = addr;

	return I1PRO_OK;
}


/* Serialize an i1key keyv into a char buffer. Error if it is outside the buffer */
static i1pro_code i1data_ser_ints(
	i1data *d,
	i1keyv *k,
	unsigned char *buf,
	unsigned int size
) {
	i1pro *p = d->p;
	int i, len;

	if (k->type != i1_dtype_int)
		return I1PRO_DATA_WRONGTYPE;

	len = k->count * 4;
	if (len > k->size)
		return I1PRO_DATA_BUFSIZE;

	if (k->addr < 0 || k->addr >= size || (k->addr + k->size) > size)
		return I1PRO_DATA_BUFSIZE;

	buf += k->addr;
	for (i = 0; i < k->count; i++, buf += 4) {
		int2buf(buf, ((int *)k->data)[i]);
	}

	return I1PRO_OK;
}

/* Serialize a double keyv as floats into a char buffer. Error if the buf is not big enough */
static i1pro_code i1data_ser_doubles(
	i1data *d,
	i1keyv *k,
	unsigned char *buf,
	unsigned int size
) {
	i1pro *p = d->p;
	int i, len;

	if (k->type != i1_dtype_double)
		return I1PRO_DATA_WRONGTYPE;

	len = k->count * 4;
	if (len > k->size)
		return I1PRO_DATA_BUFSIZE;

	if (k->addr < 0 || k->addr >= size || (k->addr + k->size) > size)
		return I1PRO_DATA_BUFSIZE;

	buf += k->addr;
	for (i = 0; i < k->count; i++, buf += 4) {
		int2buf(buf, doubletoIEEE754(((double *)k->data)[i]));
	}

	return I1PRO_OK;
}

/* Copy an array full of ints to the key */ 
/* Note the count must be the same as the existing key value, */
/* since we are not prepared to re-allocate key/values within */
/* the EEProm, or re-write the directory. */
static i1pro_code i1data_add_ints(i1data *d, i1key key, int *data, unsigned int count) {
	i1keyv *k;
	int i;

	if ((k = d->make_key(d, key)) == NULL)
		return I1PRO_DATA_MAKE_KEY;

	if (count != k->count)
		return I1PRO_DATA_COUNT;

	if (k->data != NULL)
		free(k->data);

	if ((k->data = (void *)malloc(sizeof(int) * count)) == NULL)
		return I1PRO_DATA_MEMORY;

	for (i = 0; i < count; i++) {
		((int *)k->data)[i] = data[i];
	}

	k->count = count;
	k->type = i1_dtype_int;

	return I1PRO_OK;
}

/* Copy an array full of doubles to the key */ 
/* Note the count must be the same as the existing key value, */
/* since we are not prepared to re-allocate key/values within */
/* the EEProm, or re-write the directory. */
static i1pro_code i1data_add_doubles(i1data *d, i1key key, double *data, unsigned int count) {
	i1keyv *k;
	int i;

	if ((k = d->make_key(d, key)) == NULL)
		return I1PRO_DATA_MAKE_KEY;

	if (count != k->count)
		return I1PRO_DATA_COUNT;

	if (k->data != NULL)
		free(k->data);

	if ((k->data = (void *)malloc(sizeof(double) * count)) == NULL)
		return I1PRO_DATA_MEMORY;

	for (i = 0; i < count; i++) {
		((double *)k->data)[i] = data[i];
	}

	k->count = count;
	k->type = i1_dtype_double;

	return I1PRO_OK;
}

/* Initialise the data from the EEProm contents */
static i1pro_code i1data_parse_eeprom(i1data *d, unsigned char *buf, unsigned int len, int extra) {
	i1pro *p = d->p;
	int rv = I1PRO_OK;
	int dir = 0x1000;		/* Location of key directory */
	int block_id;			/* Block id */
	int nokeys;
	i1key key, off, nkey = 0, noff = 0;
	int size;				/* size of key in bytes */
	unsigned char *bp;
	int i;

	if (extra) 
		dir = 0x2000;	/* Directory is at half way in i1pro2 2nd table */

	a1logd(p->log,3,"i1pro_parse_eeprom called with %d bytes\n",len);

	/* Room for minimum number of keys ? */
	if ((dir + 300) > len)
		return I1PRO_DATA_KEY_COUNT;

	block_id = buf2ushort(buf + dir);
	if ((extra == 0 && block_id != 1)	/* Must be 1 for base data */
	 || (extra == 1 && block_id != 2))	/* Must be 2 for i1pro2 extra data*/
		return I1PRO_DATA_KEY_CORRUPT;
	
	nokeys = buf2ushort(buf + dir + 2);	/* Bytes in key table */
	if (nokeys < 300 || nokeys > 512)
		return I1PRO_DATA_KEY_COUNT;

	nokeys = (nokeys - 4)/6;				/* Number of 6 byte entries */

	a1logd(p->log,3,"%d key/values in EEProm table %d\n",nokeys, extra);

	/* We need current and next value to figure data size out */
	bp = buf + dir + 4;
	key = buf2ushort(bp);
	off = buf2int(bp+2);
	bp += 6;
	for (i = 0; i < nokeys; i++, bp += 6, key = nkey, off = noff) {
		i1_dtype type;
		
		if (i < (nokeys-1)) {
			nkey = buf2ushort(bp);
			noff = buf2int(bp+2);
		}
		size = noff - off;
		if (size < 0)
			size = 0;
		type = d->det_type(d, key);

		a1logd(p->log,3,"Table entry %d is Key 0x%04x, type %d addr 0x%x, size %d\n",
			                                                   i,key,type,off,size);

		/* Check data is within range */
		if (off >= len || noff < off || noff > len) {
			a1logd(p->log,3,"Key 0x%04x offset %d and length %d out of range\n",key,off,noff);
			return I1PRO_DATA_KEY_MEMRANGE;
		}

		if (type == i1_dtype_unknown) {
			if (d->log->debug >= 7) {
				int i;
				char oline[100], *bp = oline;
				bp = oline;
				a1logd(d->log,7,"Key 0x%04x is unknown type\n",key);
				for (i = 0; i < size; i++) {
					if ((i % 16) == 0)
						bp += sprintf(bp,"    %04x:",i);
					bp += sprintf(bp," %02x",buf[off + i]);
					if ((i+1) >= size || ((i+1) % 16) == 0) {
						bp += sprintf(bp,"\n");
						a1logd(p->log,7,oline);
						bp = oline;
					}
				}
			}
			if (type == i1_dtype_unknown)
				continue;				/* Ignore it */
		}
		if (type == i1_dtype_section) {
			if ((rv = i1data_add_eosmarker(d, key, off)) != I1PRO_OK) {
				a1logd(p->log,3,"Key 0x%04x section marker failed with 0x%x\n",key,rv);
				return rv;
			}
			continue;
		}
		if (i >= nokeys) {
			a1logd(p->log,3,"Last key wasn't a section marker!\n");
			return I1PRO_DATA_KEY_ENDMARK;
		}
		if (type == i1_dtype_short) {
			if ((rv = i1data_unser_shorts(d, key, off, buf + off, size)) != I1PRO_OK) {
				a1logd(p->log,3,"Key 0x%04x short unserialise failed with 0x%x\n",key,rv);
				return rv;
			}
		} else if (type == i1_dtype_int) {
			if ((rv = i1data_unser_ints(d, key, off, buf + off, size)) != I1PRO_OK) {
				a1logd(p->log,3,"Key 0x%04x int unserialise failed with 0x%x\n",key,rv);
				return rv;
			}
		} else if (type == i1_dtype_double) {
			if ((rv = i1data_unser_doubles(d, key, off, buf + off, size)) != I1PRO_OK) {
				a1logd(p->log,3,"Key 0x%04x double unserialise failed with 0x%x\n",key,rv);
				return rv;
			} 
		} else {
			a1logd(p->log,3,"Key 0x%04x has type we can't handle!\n",key);
		}
	}

	return I1PRO_OK;
}

/* Compute and set the checksum, then serialise all the keys up */
/* to the first marker into a buffer, ready for writing back to */
/* the EEProm. It is an error if this buffer is not located at */
/* zero in the EEProm */
static i1pro_code i1data_prep_section1(
i1data *d,
unsigned char **buf,		/* return allocated buffer */
unsigned int *len
) {
	i1pro *p = d->p;
	i1proimp *m = d->m;
	int chsum1, *chsum2;
	i1keyv *k, *sk, *j;
	i1pro_code ev = I1PRO_OK;

	a1logd(p->log,5,"i1data_prep_section1 called\n");

	/* Compute the checksum for the first copy of the log data */
	chsum1 = m->data->checksum(m->data, 0);

	/* Locate and then set the checksum */
	if ((chsum2 = m->data->get_int(m->data, key_checksum, 0)) == NULL) {
		a1logd(p->log,2,"i1data_prep_section1 failed to locate checksum\n");
		return I1PRO_INT_PREP_LOG_DATA;
	}
	*chsum2 = chsum1;

	/* Locate the first section marker */
	for (sk = d->head; sk != NULL; sk = sk->next) {
		if (sk->type == i1_dtype_section)
			break;
	}
	if (sk == NULL) {
		a1logd(p->log,2,"i1data_prep_section1 failed to find section marker\n");
		return I1PRO_INT_PREP_LOG_DATA;
	}

	/* for each key up to the first section marker */
	/* check it resides within that section, and doesn't */
	/* overlap any other key. */
	for (k = d->head; k != NULL; k = k->next) {
		if (k->type == i1_dtype_section)
			break;
		if (k->addr < 0 || k->addr >= sk->addr || (k->addr + k->size) > sk->addr) {
			a1logd(p->log,2,"i1data_prep_section1 found key outside section\n");
			return I1PRO_INT_PREP_LOG_DATA;
		}
		for (j = k->next; j != NULL; j = j->next) {
			if (j->type == i1_dtype_section)
				break;
			if ((j->addr >= k->addr && j->addr < (k->addr + k->size))
			 || ((j->addr + j->size) > k->addr && (j->addr + j->size) <= (k->addr + k->size))) {
				a1logd(p->log,2,"i1data_prep_section1 found key overlap section, 0x%x %d and 0x%x %d\n",
				k->addr, k->size, j->addr, j->size);
				return I1PRO_INT_PREP_LOG_DATA;
			}
		}
	}

	/* Allocate the buffer for the data */
	*len = sk->addr;
	if ((*buf = (unsigned char *)calloc(sk->addr, sizeof(unsigned char))) == NULL) {
		a1logw(p->log, "i1data: malloc failed!\n");
		return I1PRO_INT_MALLOC;
	}

	/* Serialise it into the buffer */
	for (k = d->head; k != NULL; k = k->next) {
		if (k->type == i1_dtype_section)
			break;
		else if (k->type == i1_dtype_int) {
			if ((ev = m->data->ser_ints(m->data, k, *buf, *len)) != I1PRO_OK) {
				a1logd(p->log,2,"i1data_prep_section1 serializing ints failed\n");
				return ev;
			}
		} else if (k->type == i1_dtype_double) {
			if ((ev = m->data->ser_doubles(m->data, k, *buf, *len)) != I1PRO_OK) {
				a1logd(p->log,2,"i1data_prep_section1 serializing doubles failed\n");
				return ev;
			}
		} else {
			a1logd(p->log,2,"i1data_prep_section1 tried to serialise unknown type\n");
			return I1PRO_INT_PREP_LOG_DATA;
		}
	}
	a1logd(p->log,5,"a_prep_section1 done\n");
	return ev;
}


/* Return the data type for the given key identifier */
static i1_dtype i1data_det_type(i1data *d, i1key key) {

	if (key < 0x100)
		return i1_dtype_section;

	switch(key) {
		/* Log keys */
		case key_meascount:
		case key_meascount + 1000:
			return i1_dtype_int;
		case key_darkreading:
		case key_darkreading + 1000:
			return i1_dtype_int;
		case key_whitereading:
		case key_whitereading + 1000:
			return i1_dtype_int;
		case key_gainmode:
		case key_gainmode + 1000:
			return i1_dtype_int;
		case key_inttime:
		case key_inttime + 1000:
			return i1_dtype_double;
		case key_caldate:
		case key_caldate + 1000:
			return i1_dtype_int;
		case key_calcount:
		case key_calcount + 1000:
			return i1_dtype_int;
		case key_checksum:
		case key_checksum + 1000:
			return i1_dtype_int;
		case key_rpinttime:
		case key_rpinttime + 1000:
			return i1_dtype_double;
		case key_rpcount:
		case key_rpcount + 1000:
			return i1_dtype_int;
		case key_acount:
		case key_acount + 1000:
			return i1_dtype_int;
		case key_lampage:
		case key_lampage + 1000:
			return i1_dtype_double;


		/* Intstrument calibration keys */
		case key_ng_lin:
			return i1_dtype_double;
		case key_hg_lin:
			return i1_dtype_double;
		case key_min_int_time:
			return i1_dtype_double;
		case key_max_int_time:
			return i1_dtype_double;
		case key_mtx_index:
			return i1_dtype_int;
		case key_mtx_nocoef:
			return i1_dtype_int;
		case key_mtx_coef:
			return i1_dtype_double;
		case key_0bb9:
			return i1_dtype_int;
		case key_0bba:
			return i1_dtype_int;
		case key_white_ref:
			return i1_dtype_double;
		case key_emis_coef:
			return i1_dtype_double;
		case key_amb_coef:
			return i1_dtype_double;
		case key_0fa0:
			return i1_dtype_int;
		case key_0bbf:
			return i1_dtype_int;
		case key_cpldrev:
			return i1_dtype_int;
		case key_0bc1:
			return i1_dtype_int;
		case key_capabilities:
			return i1_dtype_int;
		case key_0bc3:
			return i1_dtype_int;
		case key_physfilt:
			return i1_dtype_int;
		case key_0bc5:
			return i1_dtype_int;
		case key_0bc6:
			return i1_dtype_double;
		case key_sens_target:
			return i1_dtype_int;
		case key_sens_dark:
			return i1_dtype_int;
		case key_ng_sens_sat:
			return i1_dtype_int;
		case key_hg_sens_sat:
			return i1_dtype_int;
		case key_serno:
			return i1_dtype_int;
		case key_dom:
			return i1_dtype_int;
		case key_hg_factor:
			return i1_dtype_double;
		default:
			return i1_dtype_unknown;

		/* i1pro2 keys */
		case 0x2ee0:
			return i1_dtype_unknown;	// ~~
		case 0x2ee1:
			return i1_dtype_char;
		case 0x2ee2:
			return i1_dtype_int;
		case 0x2ee3:
			return i1_dtype_int;
		case 0x2ee4:
			return i1_dtype_unknown;	// ~~

		case 0x2eea:
			return i1_dtype_int;
		case 0x2eeb:
			return i1_dtype_int;
		case 0x2eec:
			return i1_dtype_int;

		case 0x2ef4:
			return i1_dtype_double;
		case 0x2ef5:
			return i1_dtype_double;
		case 0x2ef6:
			return i1_dtype_double;
		case 0x2ef9:
			return i1_dtype_double;
		case 0x2efa:
			return i1_dtype_double;
		case 0x2efe:
			return i1_dtype_int;
		case 0x2eff:
			return i1_dtype_int;
	
		case 0x2f08:
			return i1_dtype_double;
		case 0x2f09:
			return i1_dtype_double;
		case 0x2f12:
			return i1_dtype_double;
		case 0x2f13:
			return i1_dtype_double;
		case 0x2f14:
			return i1_dtype_double;
		case 0x2f15:
			return i1_dtype_double;

		case 0x2f44:					/* Wavelength LED reference shape ? */
			return i1_dtype_double;
		case 0x2f45:
			return i1_dtype_int;
		case 0x2f46:
			return i1_dtype_double;
		case 0x2f4e:
			return i1_dtype_double;
		case 0x2f4f:
			return i1_dtype_double;
		case 0x2f50:
			return i1_dtype_double;
		case 0x2f58:
			return i1_dtype_short;		/* Stray light compensation table */
		case 0x2f59:
			return i1_dtype_double;		/* Stray light scale factor ? */
		case 0x2f62:
			return i1_dtype_double;
		case 0x2f63:
			return i1_dtype_double;
		case 0x2f6c:
			return i1_dtype_double;
		case 0x2f6d:
			return i1_dtype_double;
		case 0x2f6e:
			return i1_dtype_double;
		case 0x2f76:
			return i1_dtype_double;
		case 0x2f77:
			return i1_dtype_double;
	
		case 0x32c8:
			return i1_dtype_int;		// Date
		case 0x32c9:
			return i1_dtype_int;		// Date
		case 0x32ca:
			return i1_dtype_unknown;    // ~~
		
		case 0x36b0:
			return i1_dtype_int;		// Date
		case 0x36b1:
			return i1_dtype_int;		// Date
		case 0x36b2:
			return i1_dtype_unknown;    // ~~
		
		case 0x3a99:
			return i1_dtype_unknown;    // ~~
		case 0x3a9a:
			return i1_dtype_unknown;    // ~~
		case 0x3a9b:
			return i1_dtype_unknown;    // ~~
		case 0x3a9c:
			return i1_dtype_unknown;    // ~~
		case 0x3a9d:
			return i1_dtype_unknown;    // ~~

		case 0x3e81:
			return i1_dtype_char;		// "X-Rite"
		case 0x3e82:
			return i1_dtype_unknown;    // ~~
		case 0x3e8a:
			return i1_dtype_unknown;    // ~~

		case 0x3e94:
			return i1_dtype_unknown;    // ~~
	
	}
	return i1_dtype_unknown;
}

/* Given an index starting at 0, return the matching key code */
/* for keys that get checksummed. Return 0 if outside range. */
static i1key i1data_chsum_keys(
	i1data *d,
	int index
) {
	switch(index) {
		case 0:
			return key_meascount;
		case 1:
			return key_darkreading;
		case 2:
			return key_whitereading;
		case 3:
			return key_gainmode;
		case 4:
			return key_inttime;
		case 5:
			return key_caldate;
		case 6:
			return key_calcount;
		case 7:
			return key_rpinttime;
		case 8:
			return key_rpcount;
		case 9:
			return key_acount;
		case 10:
			return key_lampage;
	}
	return 0;
}

/* Compute a checksum. */
static int i1data_checksum(
	i1data *d,
	i1key keyoffset 	/* Offset to apply to keys */
) {
	int i, n, j;
	int chsum = 0;

	for (i = 0; ; i++) {
		i1key key;
		i1keyv *k;

		if ((key = d->chsum_keys(d, i)) == 0)
			break;			/* we're done */
	
		key += keyoffset;

		if ((k = d->find_key(d, key)) == NULL)
			continue;		/* Hmm */

		if (k->type == i1_dtype_int) {
			for (j = 0; j < k->count; j++)
				chsum += ((int *)k->data)[j];	
		} else if (k->type == i1_dtype_double) {
			for (j = 0; j < k->count; j++)
				chsum += doubletoIEEE754(((double *)k->data)[j]);
		}
	}

	return chsum;
}

/* Destroy ourselves */
static void i1data_del(i1data *d) {
	i1keyv *k, *nk;
	
	del_a1log(d->log);		/* Unref it */

	/* Free all the keys and their data */
	for (k = d->head; k != NULL; k = nk) {
		nk = k->next;
		if (k->data != NULL)
			free(k->data);
		free(k);
	}
	free(d);
}

/* Constructor for i1data */
i1data *new_i1data(i1proimp *m) {
	i1data *d;
	if ((d = (i1data *)calloc(1, sizeof(i1data))) == NULL) {
		a1loge(m->p->log, 1, "new_i1data: malloc failed!\n");
		return NULL;
	}

	d->p = m->p;
	d->m = m;

	d->log = new_a1log_d(m->p->log);	/* Take reference */

	d->find_key      = i1data_find_key;
	d->make_key      = i1data_make_key;
	d->get_type      = i1data_get_type;
	d->get_count     = i1data_get_count;
	d->get_shorts    = i1data_get_shorts;
	d->get_ints      = i1data_get_ints;
	d->get_doubles   = i1data_get_doubles;
	d->get_int       = i1data_get_int;
	d->get_double    = i1data_get_double;
	d->unser_ints    = i1data_unser_ints;
	d->unser_doubles = i1data_unser_doubles;
	d->ser_ints      = i1data_ser_ints;
	d->ser_doubles   = i1data_ser_doubles;
	d->parse_eeprom  = i1data_parse_eeprom;
	d->prep_section1 = i1data_prep_section1;
	d->add_ints      = i1data_add_ints;
	d->add_doubles   = i1data_add_doubles;
	d->del           = i1data_del;

	d->det_type      = i1data_det_type;
	d->chsum_keys    = i1data_chsum_keys;
	d->checksum      = i1data_checksum;

	return d;
}

/* ----------------------------------------------------------------- */
