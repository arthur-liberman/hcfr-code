
/* 
 * Argyll Color Correction System
 *
 * Gretag i1Pro implementation functions
 *
 * Author: Graeme W. Gill
 * Date:   24/11/2006
 *
 * Copyright 2006 - 2010 Graeme W. Gill
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
	correctly, since the targets are in raw sensor value,
	but the comparison is done after subtracting black ??
	See the Munki implementation for an approach to fix this ??
*/

/*
	Notes:

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
#include <fcntl.h>
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
//#include "rspl.h"
#else /* SALONEINSTLIB */
#include <fcntl.h>
#include "sa_config.h"
#include "numsup.h"
#include "rspl1.h"
#endif /* SALONEINSTLIB */
#include "xspect.h"
#include "insttypes.h"
#include "icoms.h"
#include "conv.h"
#include "sort.h"

/* Configuration */
#define USE_THREAD		/* Need to use thread, or there are 1.5 second internal */
						/* instrument delays ! */
#undef WAIT_FOR_DELAY_TRIGGER	/* Hack to diagnose threading problems */
#undef ENABLE_WRITE		/* Enable writing of calibration and log data to the EEProm */
#define ENABLE_NONVCAL	/* Enable saving calibration state between program runs in a file */
#define ENABLE_NONLINCOR	/* Enable non-linear correction */
#define CALTOUT (24 * 60 * 60)	/* Calibration timeout in seconds */
#define MAXSCANTIME 15.0	/* Maximum scan time in seconds */
#define SW_THREAD_TIMEOUT	(10 * 60.0) 	/* Switch read thread timeout */

#define SINGLE_READ		/* Use a single USB read for scan to eliminate latency issues. */
//#define HIGH_RES		/* Enable high resolution spectral mode code. Dissable */
						/* to break dependency on rspl library. */

/* Debug */
#undef DEBUG			/* Turn on debug printfs */
#undef PLOT_DEBUG		/* Use plot to show readings & processing */
#undef DUMP_SCANV		/* Dump scan readings to a file "i1pdump.txt" */
#undef APPEND_MEAN_EMMIS_VAL /* Append averaged uncalibrated reading to file "i1pdump.txt" */
#undef PATREC_DEBUG		/* Print & Plot patch/flash recognition information */
#undef RAWR_DEBUG		/* Print out raw reading processing values */
#undef IGNORE_WHITE_INCONS	/* Ignore define reference reading inconsistency */
#undef HIGH_RES_DEBUG
#undef HIGH_RES_PLOT
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

#define ADARKINT_MIN 0.01		/* Min cal time for adaptive dark cal */
#define ADARKINT_MAX 1.0		/* Max cal time for adaptive dark cal */

#define EMIS_SCALE_FACTOR 1.0	/* Emission mode scale factor */ 
#define AMB_SCALE_FACTOR (1.0/3.141592654)	/* Ambient mode scale factor - convert */ 
								/* from Lux to Lux/PI */
								/* These factors get the same behaviour as the GMB drivers. */

/* High res mode settings */
#define HIGHRES_SHORT 350
#define HIGHRES_LONG  740
#define HIGHRES_WIDTH  (10.0/3.0) /* (The 3.3333 spacing and lanczos2 seems a good combination) */
#define HIGHRES_REF_MIN 375.0	  /* Too much stray light below this in reflective mode */


#include "i1pro.h"
#include "i1pro_imp.h"

/* - - - - - - - - - - - - - - - - - - */
#define LAMP_OFF_TIME 1500		/* msec to make sure lamp is dark for dark measurement */
#define PATCH_CONS_THR 0.1		/* Dark measurement consistency threshold */
#define TRIG_DELAY 10			/* Measure trigger delay to allow pending read, msec */

/* - - - - - - - - - - - - - - - - - - */

#if defined(DEBUG) || defined(PLOT_DEBUG) || defined(PATREC_DEBUG)
# include <plot.h>
#endif


#if defined(DEBUG) || defined(PLOT_DEBUG) || defined(HIGH_RES_PLOT)
static int disdebplot = 0;

#define DBG(xxx) fprintf xxx ;
#define dbgo stderr
#define DISDPLOT disdebplot = 1;
#define ENDPLOT disdebplot = 0;

#else

#define DBG(xxx) 
#define dbgo stderr
#define DISDPLOT 
#define ENDPLOT 
	
#endif	/* DEBUG */


/* Three levels of runtime debugging messages:

   1 = default, typical I/O messages etc.
   2 = more internal operation messages
   3 = dump all data as well.
*/

#if defined(PLOT_DEBUG) || defined(HIGH_RES_PLOT)
/* ============================================================ */
/* Debugging support */

/* Plot a CCD spectra */
void plot_raw(double *data) {
	int i;
	double xx[128];
	double yy[128];

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
void plot_wav(i1proimp *m, double *data) {
	int i;
	double xx[128];
	double yy[128];

	if (disdebplot)
		return;

	for (i = 0; i < m->nwav; i++) {
		xx[i] = XSPECT_WL(m->wl_short, m->wl_long, m->nwav, i);
		yy[i] = data[i];
	}
	do_plot(xx, yy, NULL, NULL, m->nwav);
}

/* Plot a standard res spectra */
void plot_wav1(i1proimp *m, double *data) {
	int i;
	double xx[36];
	double yy[36];

	if (disdebplot)
		return;

	for (i = 0; i < m->nwav1; i++) {
		xx[i] = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, i);
		yy[i] = data[i];
	}
	do_plot(xx, yy, NULL, NULL, m->nwav1);
}

/* Plot a high res spectra */
void plot_wav2(i1proimp *m, double *data) {
	int i;
	double xx[128];
	double yy[128];

	if (disdebplot)
		return;

	for (i = 0; i < m->nwav2; i++) {
		xx[i] = XSPECT_WL(m->wl_short2, m->wl_long2, m->nwav2, i);
		yy[i] = data[i];
	}
	do_plot(xx, yy, NULL, NULL, m->nwav2);
}

#endif	/* PLOT_DEBUG */

/* ============================================================ */

/* Implementation struct */

/* Add an implementation structure */
i1pro_code add_i1proimp(i1pro *p) {
	i1proimp *m;

	if ((m = (i1proimp *)calloc(1, sizeof(i1proimp))) == NULL) {
		DBG((dbgo,"add_i1proimp malloc %ld bytes failed (1)\n",sizeof(i1proimp)))
		if (p->verb) printf("Malloc %ld bytes failed (1)\n",sizeof(i1proimp));
		return I1PRO_INT_MALLOC;
	}
	m->p = p;

	/* EEProm data store */
	if ((m->data = new_i1data(m, p->verb, p->debug)) == NULL)
		I1PRO_INT_CREATE_EEPROM_STORE;
		
	m->msec = msec_time();

	p->m = (void *)m;
	return I1PRO_OK;
}

/* Shutdown instrument, and then destroy */
/* implementation structure */
void del_i1proimp(i1pro *p) {

	DBG((dbgo,"i1pro_del called\n"))
	if (p->m != NULL) {
		int i;
		i1proimp *m = (i1proimp *)p->m;
		i1pro_state *s;
		i1pro_code ev;

		if ((ev = i1pro_update_log(p)) != I1PRO_OK) {
			DBG((dbgo,"i1pro_update_log failed\n"))
			if (p->verb) printf("Updating the calibration and log parameters to EEProm failed\n");
		}

		/* i1pro_terminate_switch() seems to fail on a rev A & Rev C ?? */
		if (m->th != NULL) {		/* Terminate switch monitor thread */
			m->th_term = 1;			/* Tell thread to exit on error */
			i1pro_terminate_switch(p);
			
			for (i = 0; m->th_termed == 0 && i < 5; i++)
				msec_sleep(50);		/* Wait for thread to terminate */
			if (i >= 5) {
				DBG((dbgo,"i1pro switch thread termination failed\n"))
			}
			m->th->del(m->th);
		}
		DBG((dbgo,"i1pro switch thread terminated\n"))

		/* Free any per mode data */
		for (i = 0; i < i1p_no_modes; i++) {
			s = &m->ms[i];

			free_dvector(s->dark_data, 0, m->nraw-1);  
			free_dvector(s->dark_data2, 0, m->nraw-1);  
			free_dvector(s->dark_data3, 0, m->nraw-1);  
			free_dvector(s->white_data, 0, m->nraw-1);
			free_dmatrix(s->idark_data, 0, 3, 0, m->nraw-1);  

			free_dvector(s->cal_factor1, 0, m->nwav1-1);
			free_dvector(s->cal_factor2, 0, m->nwav2-1);
		}

		/* Free EEProm key data */
		if (m->data != NULL)
			m->data->del(m->data);

		/* Free other calibration data */

		if (m->mtx_index2 != NULL)
			free(m->mtx_index2);
		if (m->mtx_nocoef2 != NULL)
			free(m->mtx_nocoef2);
		if (m->mtx_coef2 != NULL)
			free(m->mtx_coef2);

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
	unsigned char eeprom[8192];	/* EEProm contents */

	DBG((dbgo,"i1pro_init:\n"))

	if (p->prelim_itype == instI1Monitor)
		p->itype = instI1Monitor;
	else if (p->prelim_itype == instI1Pro)
		p->itype = instI1Pro;
	else
		return I1PRO_UNKNOWN_MODEL;

	m->trig = inst_opt_trig_keyb;
	m->scan_toll_ratio = 1.0;

	/* Take conservative approach to when the light was last on. */
	/* Assume it might have been on right before init was called again. */
	m->slamponoff = msec_time();
	m->llampoffon = msec_time();
	m->llamponoff = msec_time();

	if ((ev = i1pro_reset(p, 0x1f)) != I1PRO_OK)
		return ev;

#ifdef USE_THREAD
	/* Setup the switch monitoring thread */
	if ((m->th = new_athread(i1pro_switch_thread, (void *)p)) == NULL)
		return I1PRO_INT_THREADFAILED;
#endif

	/* Get the current misc. status, fwrev etc */
	if ((ev = i1pro_getmisc(p, &m->fwrev, NULL, &m->maxpve, NULL, &m->powmode)) != I1PRO_OK)
		return ev; 
	DBG((dbgo,"Firmware rev = %d\n",m->fwrev))
	if (p->debug >= 1) fprintf(stderr,"Firmware rev = %d\n",m->fwrev);
	
	/* Read the EEProm */
	if ((ev = i1pro_readEEProm(p, eeprom, 0, 8192)) != I1PRO_OK)
		return ev;

	if ((ev = m->data->parse_eeprom(m->data, eeprom, 8192)) != I1PRO_OK)
		return ev;

	/* Setup various calibration parameters from the EEprom */
	{
		int *ip, i, xcount;
		unsigned int count;
		double *dp;

		/* Information about the instrument */

		if ((ip = m->data->get_ints(m->data, &count, key_serno)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->serno = ip[0];
		if (p->debug >= 1) fprintf(stderr,"Serial number = %d\n",m->serno);
		sprintf(m->sserno,"%ud",m->serno);

		if ((ip = m->data->get_ints(m->data, &count, key_dom)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->dom = ip[0];
		if (p->debug >= 1) fprintf(stderr,"Date of manufactur = %d-%d-%d\n",
		                          m->dom/1000000, (m->dom/10000) % 100, m->dom % 10000);

		if ((ip = m->data->get_ints(m->data, &count, key_cpldrev)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->cpldrev = ip[0];
		DBG((dbgo,"CPLD rev = %d\n",m->cpldrev))
		if (p->debug >= 1) fprintf(stderr,"CPLD rev = %d\n",m->cpldrev);

		if ((ip = m->data->get_ints(m->data, &count, key_capabilities)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->capabilities = ip[0];
		if (p->debug >= 1) fprintf(stderr,"Capabilities flag = 0x%x\n",m->capabilities);

		if ((ip = m->data->get_ints(m->data, &count, key_physfilt)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->physfilt = ip[0];
		if (p->debug >= 1) fprintf(stderr,"Physical filter flag = 0x%x\n",m->physfilt);

		/* Underlying calibration information */

		m->nraw = 128;
		if (m->data->get_ints(m->data, &m->nwav1, key_mtx_index) == 0)
			return I1PRO_HW_CALIBINFO;
		if (m->nwav1 != 36)
			return I1PRO_HW_CALIBINFO;
		m->wl_short1 = 380.0;		/* Normal res. range */
		m->wl_long1 = 730.0;

		/* Fill this in here too */
		m->wl_short2 = HIGHRES_SHORT;
		m->wl_long2 = HIGHRES_LONG;
		m->nwav2 = (int)((m->wl_long2-m->wl_short2)/HIGHRES_WIDTH + 0.5) + 1;	

		/* Default to standard resolution */
		m->nwav = m->nwav1;
		m->wl_short = m->wl_short1;
		m->wl_long = m->wl_long1; 

		if ((dp = m->data->get_doubles(m->data, &count, key_hg_factor)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->highgain = dp[0];
		if (p->debug >= 1) fprintf(stderr,"High gain         = %.10f\n",m->highgain);

		if ((m->lin0 = m->data->get_doubles(m->data, &m->nlin0, key_ng_lin)) == NULL
		                                                               || m->nlin0 < 1)
			return I1PRO_HW_CALIBINFO;

		if ((m->lin1 = m->data->get_doubles(m->data, &m->nlin1, key_hg_lin)) == NULL
		                                                               || m->nlin1 < 1)
			return I1PRO_HW_CALIBINFO;

		if (p->debug >= 1) {
			fprintf(stderr,"Normal non-lin    =");
			for(i = 0; i < m->nlin0; i++)
				fprintf(stderr," %1.10f",m->lin0[i]);
			fprintf(stderr,"\n");
			fprintf(stderr,"High Gain non-lin =");
			for(i = 0; i < m->nlin1; i++)
				fprintf(stderr," %1.10f",m->lin1[i]);
			fprintf(stderr,"\n");
		}

		if ((dp = m->data->get_doubles(m->data, &count, key_min_int_time)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->min_int_time = dp[0];

		/* And then override it */
		if (m->fwrev >= 301)
			m->min_int_time = 0.004716;
		else
			m->min_int_time = 0.00884;

		if ((dp = m->data->get_doubles(m->data, &count, key_max_int_time)) == NULL || count < 1)
			return I1PRO_HW_CALIBINFO;
		m->max_int_time = dp[0];

		if ((m->mtx_index1 = m->data->get_ints(m->data, &count, key_mtx_index)) == NULL
		                                                                   || count != m->nwav1)
			return I1PRO_HW_CALIBINFO;

		if ((m->mtx_nocoef1 = m->data->get_ints(m->data, &count, key_mtx_nocoef)) == NULL
		                                                                   || count != m->nwav1)
			return I1PRO_HW_CALIBINFO;

		for (xcount = i = 0; i < m->nwav1; i++)	/* Count number expected in matrix coeffs */
			xcount += m->mtx_nocoef1[i];

		if ((m->mtx_coef1 = m->data->get_doubles(m->data, &count, key_mtx_coef)) == NULL
		                                                                    || count != xcount)
			return I1PRO_HW_CALIBINFO;

		if ((m->white_ref1 = m->data->get_doubles(m->data, &count, key_white_ref)) == NULL
		                                                                   || count != m->nwav1) {
			if (p->itype != instI1Monitor)
				return I1PRO_HW_CALIBINFO;
			m->white_ref1 = NULL;
		}

		if ((m->emis_coef1 = m->data->get_doubles(m->data, &count, key_emis_coef)) == NULL
		                                                                   || count != m->nwav1)
			return I1PRO_HW_CALIBINFO;

		if ((m->amb_coef1 = m->data->get_doubles(m->data, &count, key_amb_coef)) == NULL
		                                                                   || count != m->nwav1) {
			if (p->itype != instI1Monitor
			 && m->capabilities & 0x6000)		/* Expect ambient calibration */
				return I1PRO_HW_CALIBINFO;
			m->amb_coef1 = NULL;
		}
		/* Default to standard resolution */
		m->mtx_index = m->mtx_index1;
		m->mtx_nocoef = m->mtx_nocoef1;
		m->mtx_coef = m->mtx_coef1;
		m->white_ref = m->white_ref1;
		m->emis_coef = m->emis_coef1;
		m->amb_coef = m->amb_coef1;
	
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
			DBG((dbgo,"Read log information OK\n"))

			break;
		}
	}


	/* Set up the current state of each mode */
	{
		int i, j;
		i1pro_state *s;

		/* First set state to basic configuration */
		/* (We assume it's been zero'd) */
		for (i = 0; i < i1p_no_modes; i++) {
			s = &m->ms[i];

			/* Default to an emissive configuration */
			s->targoscale = 1.0;	/* Default full scale */
			s->gainmode = 0;		/* Normal gain mode */

			s->inttime = 0.5;		/* Integration time */
			s->lamptime = 0.50;		/* Lamp turn on time (up to 1.0 sec is better, */

			s->dark_valid = 0;		/* Dark cal invalid */
			s->dark_data = dvectorz(0, m->nraw-1);  
			s->dark_data2 = dvectorz(0, m->nraw-1);  
			s->dark_data3 = dvectorz(0, m->nraw-1);  

			s->cal_valid = 0;		/* Scale cal invalid */
			s->cal_factor1 = dvectorz(0, m->nwav1-1);
			s->cal_factor2 = dvectorz(0, m->nwav2-1);
			s->cal_factor = s->cal_factor1; /* Default to standard resolution */
			s->white_data = dvectorz(0, m->nraw-1);

			s->idark_valid = 0;		/* Dark cal invalid */
			s->idark_data = dmatrixz(0, 3, 0, m->nraw-1);  

			s->min_wl = 0.0;		/* No minimum by default */

			s->dark_int_time  = DISP_INTT;	/* 2.0 */
			s->dark_int_time2 = DISP_INTT2;	/* 0.8 */
			s->dark_int_time3 = DISP_INTT3;	/* 0.3 */

			s->idark_int_time[0] = s->idark_int_time[2] = ADARKINT_MIN;	/* 0.01 */
			s->idark_int_time[1] = s->idark_int_time[3] = ADARKINT_MAX; /* 1.0 */

			s->need_calib = 1;		/* By default always need a calibration at start */
			s->need_dcalib = 1;
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
												/* (The actual value the OMD uses is 0.20) */
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

				case i1p_disp_spot:				/* Same as emissive spot, but not adaptive */
					for (j = 0; j < m->nwav1; j++)
						s->cal_factor1[j] = EMIS_SCALE_FACTOR * m->emis_coef1[j];
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
					for (j = 0; j < m->nwav1; j++)
						s->cal_factor1[j] = EMIS_SCALE_FACTOR * m->emis_coef1[j];
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
					for (j = 0; j < m->nwav1; j++)
						s->cal_factor1[j] = EMIS_SCALE_FACTOR * m->emis_coef1[j];
					s->cal_valid = 1;
					s->emiss = 1;
					s->scan = 1;
					s->adaptive = 1;
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
					for (j = 0; j < m->nwav1; j++)
						s->cal_factor1[j] = EMIS_SCALE_FACTOR * m->emis_coef1[j];
					s->cal_valid = 1;
#else
					if (m->amb_coef1 != NULL) {
						for (j = 0; j < m->nwav1; j++)
							s->cal_factor1[j] = AMB_SCALE_FACTOR * m->emis_coef1[j] * m->amb_coef1[j];
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
					for (j = 0; j < m->nwav1; j++)
						s->cal_factor1[j] = EMIS_SCALE_FACTOR * m->emis_coef1[j];
					s->cal_valid = 1;
#else
					if (m->amb_coef1 != NULL) {
						for (j = 0; j < m->nwav1; j++)
							s->cal_factor1[j] = AMB_SCALE_FACTOR * m->emis_coef1[j] * m->amb_coef1[j];
						s->cal_valid = 1;
					}
#endif
					s->emiss = 1;
					s->ambient = 1;
					s->scan = 1;
					s->flash = 1;
					s->adaptive = 0;

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
					s->inttime = m->min_int_time;	/* Maximize scan rate */
					s->dark_int_time = s->inttime;
					if (m->fwrev >= 301)			/* (We're not using scan targoscale though) */
						s->targoscale = 0.25;
					else
						s->targoscale = 0.5;
					s->adaptive = 0;

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

	if (p->itype != instI1Monitor) {
		/* Restore the previous reflective spot calibration from the EEProm */
		/* Get ready to operate the instrument */
		if ((ev = i1pro_restore_refspot_cal(p)) != I1PRO_OK)
			return ev; 
	}

#ifdef ENABLE_NONVCAL
	/* Restore the all modes calibration from the local system */
	i1pro_restore_calibration(p);
#endif
	
	/* Get ready to operate the instrument */
	if ((ev = i1pro_establish_high_power(p)) != I1PRO_OK)
		return ev; 

	/* Get the current measurement parameters (why ?) */
	if ((ev = i1pro_getmeasparams(p, &m->r_intclocks, &m->r_lampclocks, &m->r_nummeas, &m->r_measmodeflags)) != I1PRO_OK)
		return ev; 

	if (p->verb) {
		printf("Instrument Type:   Eye-One Pro\n");
		printf("Serial Number:     %d\n",m->serno);
		printf("Firmware version:  %d\n",m->fwrev);
		printf("CPLD version:      %d\n",m->cpldrev);
		printf("Date manufactured: %d-%d-%d\n",m->dom/1000000, (m->dom/10000) % 100, m->dom % 10000);
		printf("U.V. filter ?:     %s\n",m->physfilt == 0x82 ? "Yes" : "No");
		printf("Measure Ambient ?: %s\n",m->capabilities & 0x6000 ? "Yes" : "No");

#ifndef NEVER
		printf("Tot. Measurement Count:           %d\n",m->meascount);
		printf("Remission Spot Count:             %d\n",m->rpcount);
		printf("Remission Scan Count:             %d\n",m->acount);
		printf("Date of last Remission spot cal:  %s",ctime(&m->caldate));
		printf("Remission Spot Count at last cal: %d\n",m->calcount);
		printf("Total lamp usage:                 %f\n",m->lampage);
#endif
	}

	return ev;
}

/* Return a pointer to the serial number */
char *i1pro_imp_get_serial_no(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;

	return m->sserno; 
}

/* Return non-zero if capable of ambient mode */
int i1pro_imp_ambient(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;

	if (m->capabilities & 0x6000)		/* Expect ambient calibration */
		return 1;
#ifdef FAKE_AMBIENT
	return 1;
#endif
	return 0;
}

/* Set the measurement mode. It may need calibrating */
i1pro_code i1pro_imp_set_mode(
	i1pro *p,
	i1p_mode mmode,
	int spec_en		/* nz to enable reporting spectral */
) {
	i1proimp *m = (i1proimp *)p->m;

	DBG((dbgo,"i1pro_imp_set_mode called with %d\n",mmode))
	switch(mmode) {
		case i1p_refl_spot:
		case i1p_refl_scan:
			if (p->itype != instI1Pro)
				return I1PRO_INT_ILLEGALMODE;		/* i1Monitor */
			/* Fall through */
		case i1p_disp_spot:
		case i1p_emiss_spot:
		case i1p_emiss_scan:
		case i1p_amb_spot:
		case i1p_amb_flash:
		case i1p_trans_spot:
		case i1p_trans_scan:
			m->mmode = mmode;
			m->spec_en = spec_en ? 1 : 0;
			return I1PRO_OK;
	}
	return I1PRO_INT_ILLEGALMODE;
}

/* Determine if a calibration is needed. */
inst_cal_type i1pro_imp_needs_calibration(
	i1pro *p
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	time_t curtime = time(NULL);

	/* Timout calibrations that are too old */
	if ((curtime - s->iddate) > CALTOUT) {
		DBG((dbgo,"Invalidating adaptive dark cal as %d secs from last cal\n",curtime - s->iddate))
		s->idark_valid = 0;
	}
	if ((curtime - s->ddate) > CALTOUT) {
		DBG((dbgo,"Invalidating dark cal as %d secs from last cal\n",curtime - s->ddate))
		s->dark_valid = 0;
	}
	if (!s->emiss && (curtime - s->cfdate) > CALTOUT) {
		DBG((dbgo,"Invalidating white cal as %d secs from last cal\n",curtime - s->cfdate))
		s->cal_valid = 0;
	}

//printf("~1 idark_valid = %d, dark_valid = %d, cal_valid = %d\n",
//s->idark_valid,s->dark_valid,s->cal_valid); 
//printf("~1 need_calib = %d, need_dcalib = %d, noautocalib = %d\n",
//s->need_calib,s->need_dcalib, m->noautocalib); 

	if ((s->emiss && s->adaptive && !s->idark_valid)
	 || ((!s->emiss || !s->adaptive) && !s->dark_valid)
	 || (s->need_dcalib && !m->noautocalib)
	 || (s->reflective && !s->cal_valid)
	 || (s->reflective && s->need_calib && !m->noautocalib)) {
		return inst_calt_ref_white;

	} else if (   (s->trans && !s->cal_valid)
	           || (s->trans && s->need_calib && !m->noautocalib)) {
		return inst_calt_trans_white;
	} else if (s->emiss && !s->scan && !s->adaptive && !s->done_dintcal) {
		return inst_calt_disp_int_time; 
	}
	return inst_calt_none;
}

/* - - - - - - - - - - - - - - - - */
/* Calibrate for the current mode. */
/* Request an instrument calibration of the current mode. */
i1pro_code i1pro_imp_calibrate(
	i1pro *p,
	inst_cal_type caltp,	/* Calibration type. inst_calt_all for all neeeded */
	inst_cal_cond *calc,	/* Current condition/desired condition */
	char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	inst_cal_type calt = caltp; /* Specific calibration type */
	int nummeas = 0;
	int transwarn = 0;
	int ltocmode = 0;			/* 1 = Lamp turn on compensation mode */
	int i, j, k;

	DBG((dbgo,"i1pro_imp_calibrate called with calp 0x%x, calc 0x%x\n",caltp, *calc))

	/* Translate inst_calt_all into something specific */
	if (caltp == inst_calt_all) {
		if ((s->reflective && !s->cal_valid)
		 || (s->reflective && s->need_calib && !m->noautocalib)
		 || (s->reflective && !s->dark_valid && !s->idark_valid)
		 || (s->reflective && s->need_dcalib && !m->noautocalib)) {
			calt = inst_calt_ref_white;			/* Do black and white calib on white refernence */

		} else if ((s->emiss && !s->dark_valid && !s->idark_valid)
		       ||  (s->emiss && s->need_dcalib && !m->noautocalib)) {
			calt = inst_calt_em_dark;

		} else if ((s->trans && !s->dark_valid && !s->idark_valid)
		       ||  (s->trans && s->need_dcalib && !m->noautocalib)) {
			calt = inst_calt_trans_dark;

		} else if ((s->trans && !s->cal_valid)
			    || (s->trans && s->need_calib && !m->noautocalib)) {
			calt = inst_calt_trans_white;

		} else if (s->emiss && !s->scan && !s->adaptive && !s->done_dintcal) {
			calt = inst_calt_disp_int_time;

		} else {		/* Assume a user instigated white calibration */
			if (s->trans) {
				calt = inst_calt_trans_white;
			} else if (s->emiss) {
				calt = inst_calt_em_dark;
			} else {
				calt = inst_calt_ref_white;
			}
		}
	}

	/* See if it's a calibration we understand */
	if (calt != inst_calt_ref_white
	 && calt != inst_calt_em_dark
	 && calt != inst_calt_trans_dark
	 && calt != inst_calt_trans_white
	 && calt != inst_calt_disp_int_time)
		return I1PRO_UNSUPPORTED;

	/* Make sure there's the right condition for the calibration */
	if (calt == inst_calt_ref_white) {		/* Reflective white calib */
		sprintf(id, "Serial no. %d",m->serno);
		if (*calc != inst_calc_man_ref_white) {
			*calc = inst_calc_man_ref_white;
			return I1PRO_CAL_SETUP;
		}
	} else if (calt == inst_calt_em_dark) {	/* Emissive Dark calib */
		id[0] = '\000';
		if (*calc != inst_calc_man_em_dark) {
			*calc = inst_calc_man_em_dark;
			return I1PRO_CAL_SETUP;
		}
	} else if (calt == inst_calt_trans_dark) {	/* Transmissvice dark */
		id[0] = '\000';
		if (*calc != inst_calc_man_trans_dark) {
			*calc = inst_calc_man_trans_dark;
			return I1PRO_CAL_SETUP;
		}
	} else if (calt == inst_calt_trans_white) {	/* Transmissvice white */
		id[0] = '\000';
		if (*calc != inst_calc_man_trans_white) {
			*calc = inst_calc_man_trans_white;
			return I1PRO_CAL_SETUP;
		}
	}

	/* Sanity check scan mode settings, in case something strange */
	/* has been restored from the persistence file. */
	if (s->scan && s->inttime > (2.1 * m->min_int_time)) {
		s->inttime = m->min_int_time;	/* Maximize scan rate */
	}

	/* We are now either in inst_calc_man_ref_white, inst_calc_man_em_dark, */
	/* inst_calc_man_trans_dark, inst_calc_man_trans_white or inst_calc_disp_white, */
	/* sequenced in that order, and in the appropriate condition for it. */

	/* Reflective uses on the fly black, even for adaptive. */
	/* Emiss and trans can use single black ref only for non-adaptive */
	/* using the current inttime & gainmode, while display mode */
	/* does an extra fallback black cal for bright displays. */
	if ((s->reflective && *calc == inst_calc_man_ref_white)
	 || (s->emiss && !s->adaptive && !s->scan && *calc == inst_calc_man_em_dark)
	 || (s->trans && !s->adaptive && *calc == inst_calc_man_trans_dark)) {
		int stm;

		nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->inttime);

		DBG((dbgo,"Doing initial black calibration with dcaltime %f, int_time %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode))
		stm = msec_time();
		if ((ev = i1pro_dark_measure(p, s->dark_data, nummeas, &s->inttime, s->gainmode))
	                                                                         != I1PRO_OK)
			return ev;
		if (p->debug) fprintf(stderr,"Execution time of dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);

		/* Special display mode alternate integration time black measurement */
		if (s->emiss && !s->scan && !s->adaptive) {
			nummeas = i1pro_comp_nummeas(p, s->dcaltime2, s->dark_int_time2);
			DBG((dbgo,"Doing 2nd initial black calibration with dcaltime2 %f, dark_int_time2 %f, nummeas %d, gainmode %d\n", s->dcaltime2, s->dark_int_time2, nummeas, s->gainmode))
			stm = msec_time();
			if ((ev = i1pro_dark_measure(p, s->dark_data2, nummeas, &s->dark_int_time2,
			                                                   s->gainmode)) != I1PRO_OK)
				return ev;
			if (p->debug) fprintf(stderr,"Execution time of 2nd dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);

			nummeas = i1pro_comp_nummeas(p, s->dcaltime3, s->dark_int_time3);
			DBG((dbgo,"Doing 3rd initial black calibration with dcaltime3 %f, dark_int_time3 %f, nummeas %d, gainmode %d\n", s->dcaltime3, s->dark_int_time3, nummeas, s->gainmode))
			nummeas = i1pro_comp_nummeas(p, s->dcaltime3, s->dark_int_time3);
			stm = msec_time();
			if ((ev = i1pro_dark_measure(p, s->dark_data3, nummeas, &s->dark_int_time3,
			                                                   s->gainmode)) != I1PRO_OK)
				return ev;
			if (p->debug) fprintf(stderr,"Execution time of 3rd dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);
		}
		s->dark_valid = 1;
		s->need_dcalib = 0;
		s->ddate = time(NULL);
		s->dark_int_time = s->inttime;
		s->dark_gain_mode = s->gainmode;
	}

	/* Emsissive scan (flash) uses the fastest possible scan rate (??) */
	if (s->emiss && !s->adaptive && s->scan && *calc == inst_calc_man_em_dark) {
		int stm;

		nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->inttime);

		DBG((dbgo,"Doing display black calibration with dcaltime %f, int_time %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode))
		stm = msec_time();
		if ((ev = i1pro_dark_measure(p, s->dark_data, nummeas, &s->inttime, s->gainmode))
	                                                                         != I1PRO_OK) {
			return ev;
		}
		if (p->debug) fprintf(stderr,"Execution time of dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);

		s->dark_valid = 1;
		s->need_dcalib = 0;
		s->ddate = time(NULL);
		s->dark_int_time = s->inttime;
		s->dark_gain_mode = s->gainmode;

		/* Save the calib to all similar modes */
		DBG((dbgo,"Saving emissive scan black calib to similar modes\n"))
		for (i = 0; i < i1p_no_modes; i++) {
			i1pro_state *ss = &m->ms[i];
			if (ss == s)
				continue;
			if (ss->emiss && !ss->adaptive && ss->scan) {
				ss->dark_valid = s->dark_valid;
				ss->need_dcalib = s->need_dcalib;
				ss->ddate = s->ddate;
				ss->dark_int_time = s->dark_int_time;
				ss->dark_gain_mode = s->dark_gain_mode;
				for (k = 0; k < m->nraw; k++) {
					ss->dark_data[k] = s->dark_data[k];
				}
			}
		}
	}

	/* Deal with an emmissive/transmissive black reference */
	/* in non-scan mode, where the integration time and gain may vary. */
	if ((s->emiss && s->adaptive && !s->scan && *calc == inst_calc_man_em_dark)
	 || (s->trans && s->adaptive && !s->scan && *calc == inst_calc_man_trans_dark)) {
		int i, j, k;

		/* Adaptive where we can't measure the black reference on the fly, */
		/* so bracket it and interpolate. */
		/* The black reference is probably temperature dependent, but */
		/* there's not much we can do about this. */

		s->idark_int_time[0] = ADARKINT_MIN; /* 0.01 */
		nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->idark_int_time[0]);
		DBG((dbgo,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[0] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[0], nummeas, 0))
		if ((ev = i1pro_dark_measure(p, s->idark_data[0], nummeas, &s->idark_int_time[0], 0))
		                                                                          != I1PRO_OK)
			return ev;
	
		s->idark_int_time[1] = ADARKINT_MAX; /* 1.0 */
		nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->idark_int_time[1]);
		DBG((dbgo,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[1] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[1], nummeas, 0))
		if ((ev = i1pro_dark_measure(p, s->idark_data[1], nummeas, &s->idark_int_time[1], 0))
		                                                                          != I1PRO_OK)
			return ev;
	
		s->idark_int_time[2] = ADARKINT_MIN; /* 0.01 */
		nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->idark_int_time[2]);
		DBG((dbgo,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[2] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[2], nummeas, 1))
		if ((ev = i1pro_dark_measure(p, s->idark_data[2], nummeas, &s->idark_int_time[2], 1))
		                                                                          != I1PRO_OK)
			return ev;
	
		s->idark_int_time[3] = ADARKINT_MAX; /* 1.0 */
		DBG((dbgo,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[3] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[3], nummeas, 1))
		nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->idark_int_time[3]);
		if ((ev = i1pro_dark_measure(p, s->idark_data[3], nummeas, &s->idark_int_time[3], 1))
		                                                                          != I1PRO_OK)
			return ev;
	
		i1pro_prepare_idark(p);
		s->idark_valid = 1;
		s->iddate = time(NULL);
	
		if ((ev = i1pro_interp_dark(p, s->dark_data, s->inttime, s->gainmode)) != I1PRO_OK)
			return ev;
		s->dark_valid = 1;
		s->need_dcalib = 0;
		s->ddate = s->iddate;
		s->dark_int_time = s->inttime;
		s->dark_gain_mode = s->gainmode;

		/* Save the calib to all similar modes */
		DBG((dbgo,"Saving adaptive black calib to similar modes\n"))
		for (i = 0; i < i1p_no_modes; i++) {
			i1pro_state *ss = &m->ms[i];
			if (ss == s)
				continue;
			if ((ss->emiss || ss->trans) && ss->adaptive && !ss->scan) {
				ss->idark_valid = s->idark_valid;
				ss->need_dcalib = s->need_dcalib;
				ss->iddate = s->iddate;
				ss->dark_int_time = s->dark_int_time;
				ss->dark_gain_mode = s->dark_gain_mode;
				for (j = 0; j < 4; j++) {
					ss->idark_int_time[j] = s->idark_int_time[j];
					for (k = 0; k < m->nraw; k++)
						ss->idark_data[j][k] = s->idark_data[j][k];
				}
			}
		}

		DBG((dbgo,"Done adaptive interpolated black calibration\n"))

		/* Test accuracy of dark level interpolation */
#ifdef NEVER
#ifdef DEBUG
		{
			double tinttime;
			double ref[128], interp[128];
			
			fprintf(stderr,"Normal gain offsets:\n");
			plot_raw(s->idark_data[0]);
			
			fprintf(stderr,"High gain offsets:\n");
			plot_raw(s->idark_data[2]);
			
			tinttime = 0.2;
			nummeas = i1pro_comp_nummeas(p, s->dcaltime, tinttime);
			if ((ev = i1pro_dark_measure(p, ref, nummeas, &tinttime, 0)) != I1PRO_OK)
				return ev;
			i1pro_interp_dark(p, interp, 0.2, 0);
			plot_raw2(ref, interp);
			
			tinttime = 0.2;
			nummeas = i1pro_comp_nummeas(p, s->dcaltime, tinttime);
			if ((ev = i1pro_dark_measure(p, ref, nummeas, &tinttime, 1)) != I1PRO_OK)
				return ev;
			i1pro_interp_dark(p, interp, 0.2, 1);
			plot_raw2(ref, interp);
		}
#endif	/* DEBUG */
#endif	/* NEVER */

	}

	/* Deal with an emissive/transmisive adaptive black reference */
	/* when in scan mode. */
	if ((s->emiss && s->adaptive && s->scan && *calc == inst_calc_man_em_dark)
	 || (s->trans && s->adaptive && s->scan && *calc == inst_calc_man_trans_dark)) {
		int j;
		/* We know scan is locked to the minimum integration time, */
		/* so we can measure the dark data at that integration time, */
		/* but we don't know what gain mode will be used, so measure both, */
		/* and choose the appropriate one on the fly. */
	

		s->idark_int_time[0] = s->inttime;
		nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->idark_int_time[0]);
		DBG((dbgo,"Doing adaptive scan black calibration, dcaltime %f, idark_int_time[0] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[0], nummeas, s->gainmode))
		if ((ev = i1pro_dark_measure(p, s->idark_data[0], nummeas, &s->idark_int_time[0], 0))
		                                                                          != I1PRO_OK)
			return ev;
	
		s->idark_int_time[2] = s->inttime;
		nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->idark_int_time[2]);
		DBG((dbgo,"Doing adaptive scan black calibration, dcaltime %f, idark_int_time[2] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[2], nummeas, s->gainmode))
		if ((ev = i1pro_dark_measure(p, s->idark_data[2], nummeas, &s->idark_int_time[2], 1))
		                                                                          != I1PRO_OK)
			return ev;
	
		s->idark_valid = 1;
		s->iddate = time(NULL);

		if (s->gainmode) {
			for (j = 0; j < m->nraw; j++)
				s->dark_data[j] = s->idark_data[2][j];
		} else {
			for (j = 0; j < m->nraw; j++)
				s->dark_data[j] = s->idark_data[0][j];
		}
		s->dark_valid = 1;
		s->need_dcalib = 0;
		s->ddate = s->iddate;
		s->dark_int_time = s->inttime;
		s->dark_gain_mode = s->gainmode;

		DBG((dbgo,"Done adaptive scan black calibration\n"))

		/* Save the calib to all similar modes */
		DBG((dbgo,"Saving adaptive scan black calib to similar modes\n"))
		for (i = 0; i < i1p_no_modes; i++) {
			i1pro_state *ss = &m->ms[i];
			if (ss == s)
				continue;
			if ((ss->emiss || ss->trans) && ss->adaptive && s->scan) {
				ss->idark_valid = s->idark_valid;
				ss->need_dcalib = s->need_dcalib;
				ss->iddate = s->iddate;
				ss->dark_int_time = s->dark_int_time;
				ss->dark_gain_mode = s->dark_gain_mode;
				for (j = 0; j < 4; j++) {
					ss->idark_int_time[j] = s->idark_int_time[j];
					for (k = 0; k < m->nraw; k++)
						ss->idark_data[j][k] = s->idark_data[j][k];
				}
			}
		}
	}

	/* If we are doing a white reference calibrate */
	if ((s->reflective && *calc == inst_calc_man_ref_white)
	 || (s->trans && *calc == inst_calc_man_trans_white)) {
		int optimal;
		double scale;

		DBG((dbgo,"Doing initial white calibration with current inttime %f, gainmode %d\n",
		                                                     s->inttime, s->gainmode))
		nummeas = i1pro_comp_nummeas(p, s->wcaltime, s->inttime);
		ev = i1pro_whitemeasure(p, s->cal_factor1, s->cal_factor2, s->white_data, &scale, nummeas,
		           &s->inttime, s->gainmode, s->scan ? 1.0 : s->targoscale, 0);
		if (ev == I1PRO_RD_SENSORSATURATED) {
			scale = 0.0;			/* Signal it this way */
			ev = I1PRO_OK;
		}
		if (ev != I1PRO_OK)
			return ev;

		/* For non-scan modes, we adjust the integration time to avoid saturation, */
		/* and to try and match the target optimal sensor value */
		if (!s->scan) {
			/* If it's adaptive and not good, or if it's not adaptive and even worse, */
			/* or if we're using lamp dynamic compensation for reflective scan, */
			/* change the parameters until the white is optimal. */
			if ((s->adaptive && (scale < 0.95 || scale > 1.05))
			 || (scale < 0.3 || scale > 2.0)
				) {
		
				/* Need to have done adaptive black measure to change inttime/gain params */
				if (*calc != inst_calc_man_ref_white && !s->idark_valid) {
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
						DBG((dbgo,"Doing another black calibration with dadaptime %f, min inttime %f, nummeas %d, gainmode %d\n", s->dadaptime, s->inttime, nummeas, s->gainmode))
						if ((ev = i1pro_dark_measure(p, s->dark_data, nummeas, &s->inttime,
						                                              s->gainmode)) != I1PRO_OK)
							return ev;

					} else if (s->idark_valid) {
						/* compute interpolated dark refence for chosen inttime & gainmode */
						DBG((dbgo,"Interpolate dark calibration reference\n"))
						if ((ev = i1pro_interp_dark(p, s->dark_data, s->inttime, s->gainmode))
						                                                             != I1PRO_OK)
							return ev;
						s->dark_valid = 1;
						s->ddate = s->iddate;
						s->dark_int_time = s->inttime;
						s->dark_gain_mode = s->gainmode;
					} else {
						return I1PRO_INT_NOINTERPDARK;
					}
					DBG((dbgo,"Doing another white calibration with min inttime %f, gainmode %d\n",
					                                                  s->inttime,s->gainmode))
					nummeas = i1pro_comp_nummeas(p, s->wadaptime, s->inttime);
					if ((ev = i1pro_whitemeasure(p, s->cal_factor1, s->cal_factor2, s->white_data,
					   &scale, nummeas, &s->inttime, s->gainmode, s->targoscale, 0)) != I1PRO_OK) {
						return ev;
					}
				}

				/* Compute a new integration time and gain mode */
				/* in order to optimise the sensor values. Error if can't get */
				/* scale we want. */
				if ((ev = i1pro_optimise_sensor(p, &s->inttime, &s->gainmode, 
				         s->inttime, s->gainmode, s->trans, 0, s->targoscale, scale)) != I1PRO_OK) {
					return ev;
				}
				DBG((dbgo,"Computed optimal white inttime %f and gainmode %d\n",
				                                                      s->inttime,s->gainmode))
			
				if (*calc == inst_calc_man_ref_white) {
					nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->inttime);
					DBG((dbgo,"Doing final black calibration with dcaltime %f, opt inttime %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode))
					if ((ev = i1pro_dark_measure(p, s->dark_data, nummeas, &s->inttime,
					                                              s->gainmode)) != I1PRO_OK)
						return ev;
					s->dark_valid = 1;
					s->ddate = time(NULL);
					s->dark_int_time = s->inttime;
					s->dark_gain_mode = s->gainmode;

				} else if (s->idark_valid) {
					/* compute interpolated dark refence for chosen inttime & gainmode */
					DBG((dbgo,"Interpolate dark calibration reference\n"))
					if ((ev = i1pro_interp_dark(p, s->dark_data, s->inttime, s->gainmode))
					                                                               != I1PRO_OK)
						return ev;
					s->dark_valid = 1;
					s->ddate = s->iddate;
					s->dark_int_time = s->inttime;
					s->dark_gain_mode = s->gainmode;
				} else {
					return I1PRO_INT_NOINTERPDARK;
				}
			
				DBG((dbgo,"Doing final white calibration with opt int_time %f, gainmode %d\n",
				                                                 s->inttime,s->gainmode))
				nummeas = i1pro_comp_nummeas(p, s->wcaltime, s->inttime);
				if ((ev = i1pro_whitemeasure(p, s->cal_factor1, s->cal_factor2, s->white_data,
				  &scale, nummeas, &s->inttime, s->gainmode, s->targoscale, ltocmode)) != I1PRO_OK)
					return ev;
			}

		/* For scan we take a different approach. We try and use the minimum possible */
		/* integration time so as to maximize sampling rate, and adjust the gain */
		/* if necessary. */
		} else if (s->adaptive) {
			int j;
			if (scale == 0.0) {		/* If sensor was saturated */
				if (p->debug) fprintf(dbgo,"Scan illuminant is saturating sensor\n");
				if (s->gainmode == 0) {
					return I1PRO_RD_SENSORSATURATED;		/* Nothing we can do */
				}
				if (p->debug) fprintf(dbgo,"Switching to low gain mode");
				s->gainmode = 0;
				/* Measure white again with low gain */
				nummeas = i1pro_comp_nummeas(p, s->wcaltime, s->inttime);
				if ((ev = i1pro_whitemeasure(p, s->cal_factor1, s->cal_factor2, s->white_data,
				   &scale, nummeas, &s->inttime, s->gainmode, 1.0, 0)) != I1PRO_OK)
					return ev;
			} else if (s->gainmode == 0 && scale > m->highgain) {
				if (p->debug) fprintf(dbgo,"Scan signal is so low we're switching to high gain mode\n");
				s->gainmode = 1;
				/* Measure white again with high gain */
				nummeas = i1pro_comp_nummeas(p, s->wcaltime, s->inttime);
				if ((ev = i1pro_whitemeasure(p, s->cal_factor1, s->cal_factor2, s->white_data,
				   &scale, nummeas, &s->inttime, s->gainmode, 1.0, 0)) != I1PRO_OK)
					return ev;
			}

			DBG((dbgo,"After scan gain adaption\n",scale))
			if (scale > 6.0) {
				transwarn |= 2;
				if (p->debug)
					fprintf(stderr,"scan white reference is not bright enough by %f\n",scale);
			}

			if (*calc == inst_calc_man_ref_white) {
				nummeas = i1pro_comp_nummeas(p, s->dcaltime, s->inttime);
				DBG((dbgo,"Doing final black calibration with dcaltime %f, opt inttime %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode))
				if ((ev = i1pro_dark_measure(p, s->dark_data, nummeas, &s->inttime,
				                                              s->gainmode)) != I1PRO_OK)
					return ev;
				s->dark_valid = 1;
				s->ddate = time(NULL);
				s->dark_int_time = s->inttime;
				s->dark_gain_mode = s->gainmode;

			} else if (s->idark_valid) {
				/* compute interpolated dark refence for chosen inttime & gainmode */
				DBG((dbgo,"Interpolate dark calibration reference\n"))
				if (s->gainmode) {
					for (j = 0; j < m->nraw; j++)
						s->dark_data[j] = s->idark_data[2][j];
				} else {
					for (j = 0; j < m->nraw; j++)
						s->dark_data[j] = s->idark_data[0][j];
				}
				s->dark_valid = 1;
				s->ddate = s->iddate;
				s->dark_int_time = s->inttime;
				s->dark_gain_mode = s->gainmode;
			} else {
				return I1PRO_INT_NOINTERPDARK;
			}
			DBG((dbgo,"Doing final white calibration with opt int_time %f, gainmode %d\n",
			                                                 s->inttime,s->gainmode))
		}


		/* We've settled on the inttime and gain mode to get a good white reference. */
		if (s->reflective) {	/* We read the white reference - check it */
			/* Check a reflective white measurement, and check that */
			/* it seems reasonable. Return I1PRO_OK if it is, error if not. */
			if ((ev = i1pro_check_white_reference1(p, s->cal_factor1)) != I1PRO_OK) {
				return ev;
			}
			/* Compute a calibration factor given the reading of the white reference. */
			i1pro_compute_white_cal(p, s->cal_factor1, m->white_ref1, s->cal_factor1,
			                           s->cal_factor2, m->white_ref2, s->cal_factor2);

		} else {
			/* Compute a calibration factor given the reading of the white reference. */
			transwarn |= i1pro_compute_white_cal(p, s->cal_factor1, NULL, s->cal_factor1,
			                                       s->cal_factor2, NULL, s->cal_factor2);
		}
		s->cal_valid = 1;
		s->cfdate = time(NULL);
		s->need_calib = 0;
	}

	/* Update and write the EEProm log if the is a refspot calibration */
	if (s->reflective && !s->scan && s->dark_valid && s->cal_valid) {
		m->calcount = m->rpcount;
		m->caldate = time(NULL);
		if ((ev = i1pro_update_log(p)) != I1PRO_OK) {
			DBG((dbgo,"i1pro_update_log failed\n"))
			if (p->verb) printf("Updating the calibration and log parameters to EEProm failed\n");
		}
	}
#ifdef ENABLE_NONVCAL
	/* Save the calibration to a file */
	i1pro_save_calibration(p);
#endif

	/* Deal with a display integration time calibration */
	if (s->emiss && !s->scan && !s->adaptive
	 && !s->done_dintcal && *calc == inst_calc_disp_white) {
		double scale;
		double *data;

		data = dvectorz(0, m->nraw-1);

		DBG((dbgo,"Doing display integration time calibration\n"))

		/* Simply measure the full display white, and if it's close to */
		/* saturation, switch to the alternate display integration time */
		nummeas = i1pro_comp_nummeas(p, s->wreadtime, s->inttime);
		ev = i1pro_whitemeasure(p, NULL, NULL, data , &scale, nummeas,
		           &s->inttime, s->gainmode, s->targoscale, 0);
		/* Switch to the alternate if things are too bright */
		/* We do this simply by swapping the alternate values in. */
		if (ev == I1PRO_RD_SENSORSATURATED || scale < 1.0) {
			double *tt, tv;
			DBG((dbgo,"Switching to alternate display integration time %f seconds\n",s->dark_int_time2))
			if (p->debug)
				fprintf(stderr,"Switching to alternate display integration time %f seconds\n",s->dark_int_time2);
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
				double *tt, tv;
				DBG((dbgo,"Switching to 3rd alternate display integration time %f seconds\n",s->dark_int_time3))
				if (p->debug)
					fprintf(stderr,"Switching to alternate display integration time %f seconds\n",s->dark_int_time3);
				/* Undo previous swap */
				tv = s->inttime; s->inttime = s->dark_int_time2; s->dark_int_time2 = tv;
				tt = s->dark_data; s->dark_data = s->dark_data2; s->dark_data2 = tt;
				/* swap in 2nd alternate */
				tv = s->inttime; s->inttime = s->dark_int_time3; s->dark_int_time3 = tv;
				tt = s->dark_data; s->dark_data = s->dark_data3; s->dark_data3 = tt;
				s->dispswap = 2;
			}
		}
		free_dvector(data, 0, m->nraw-1);
		if (ev != I1PRO_OK) {
			return ev;
		}
		s->done_dintcal = 1;

		DBG((dbgo,"Done display integration time calibration\n"))
	}

	/* Go around again if we're calibrating all, and transmissive, */
	/* since transmissive needs two calibration steps. */
	if (caltp == inst_calt_all) {
		if ( (s->trans && !s->cal_valid)
		  || (s->trans && s->need_calib && !m->noautocalib)) {
			calt = inst_calt_trans_white;
			*calc = inst_calc_man_trans_white;
			return I1PRO_CAL_SETUP;
		}
	}

	if (transwarn) {
		*calc = inst_calc_message;
		if (transwarn & 2)
			strcpy(id, "Warning: Transmission light source is too low for accuracy!");
		else
			strcpy(id, "Warning: Transmission light source is low at some wavelengths!");
		return I1PRO_OK;
	}

	DBG((dbgo,"Finished cal with dark_valid = %d, cal_valid = %d\n",s->dark_valid, s->cal_valid))

	return ev; 
}

/* Interpret an icoms error into a I1PRO error */
static int icoms2i1pro_err(int se) {
	if (se & ICOM_USERM) {
		se &= ICOM_USERM;
		if (se == ICOM_USER)
			return I1PRO_USER_ABORT;
		if (se == ICOM_TERM)
			return I1PRO_USER_TERM;
		if (se == ICOM_TRIG)
			return I1PRO_USER_TRIG;
		if (se == ICOM_CMND)
			return I1PRO_USER_CMND;
	}
	if (se != ICOM_OK)
		return I1PRO_COMS_FAIL;
	return I1PRO_OK;
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
	int nvals			/* Number of values */	
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

	DBG((dbgo,"i1pro_imp_measure called\n"))
	if (p->debug)
		fprintf(stderr,"Taking %d measurments in %s%s%s%s mode called\n", nvals,
		        s->emiss ? "Emission" : s->trans ? "Trans" : "Refl", 
		        s->emiss && s->ambient ? " Ambient" : "",
		        s->scan ? " Scan" : "",
		        s->flash ? " Flash" : "",
		        s->adaptive ? " Adaptive" : "");


	if ((s->emiss && s->adaptive && !s->idark_valid)
	 || ((!s->emiss || !s->adaptive) && !s->dark_valid)
	 || !s->cal_valid) {
		DBG((dbgo,"emis %d, adaptive %d, idark_valid %d\n",s->emiss,s->adaptive,s->idark_valid))
		DBG((dbgo,"dark_valid %d, cal_valid %d\n",s->dark_valid,s->cal_valid))
		DBG((dbgo,"i1pro_imp_measure need calibration\n"))
		return I1PRO_RD_NEEDS_CAL;
	}
		
	if (nvals <= 0
	 || (!s->scan && nvals > 1)) {
		DBG((dbgo,"i1pro_imp_measure wrong number of patches\n"))
		return I1PRO_INT_WRONGPATCHES;
	}

	/* Notional number of measurements, befor adaptive and not counting scan */
	nummeas = i1pro_comp_nummeas(p, s->wreadtime, s->inttime);

	/* Do dark cal allocation before we wait for user hitting switch */
	if (s->reflective) {
		bsize = 256 * nummeas;
		if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
			DBG((dbgo,"i1pro_imp_measure malloc %d bytes failed (5)\n",bsize))
			if (p->verb) printf("Malloc %d bytes failed (5)\n",bsize);
			return I1PRO_INT_MALLOC;
		}
	}

	/* Allocate buffer for measurement */
	maxnummeas = i1pro_comp_nummeas(p, s->maxscantime, s->inttime);
	if (maxnummeas < nummeas)
		maxnummeas = nummeas;
	mbsize = 256 * maxnummeas;
	if ((mbuf = (unsigned char *)malloc(sizeof(unsigned char) * mbsize)) == NULL) {
		if (buf != NULL)
			free(buf);
		DBG((dbgo,"i1pro_imp_measure malloc %d bytes failed (6)\n",mbsize))
		if (p->verb) printf("Malloc %d bytes failed (6)\n",mbsize);
		return I1PRO_INT_MALLOC;
	}
	specrd = dmatrix(0, nvals-1, 0, m->nwav-1);

	if (m->trig == inst_opt_trig_keyb_switch) {
#ifdef USE_THREAD
		int currcount = m->switch_count;
		while (currcount == m->switch_count) {
			int cerr;

			if ((cerr = icoms_poll_user(p->icom, 0)) != ICOM_OK) {
				ev = icoms2i1pro_err(cerr);
				user_trig = 1;
				break;
			}
			msec_sleep(100);
		}
#else
		/* Throw one away in case the switch was pressed prematurely */
		i1pro_waitfor_switch_th(p, 0.01);

		for (ev = I1PRO_INT_BUTTONTIMEOUT; ev == I1PRO_INT_BUTTONTIMEOUT;) {
			int cerr;
			ev = i1pro_waitfor_switch_th(p, 0.1);
			if ((cerr = icoms_poll_user(p->icom, 0)) != ICOM_OK) {
				ev = icoms2i1pro_err(cerr);
				user_trig = 1;
				break;
			}
		}
#endif
		if (ev == I1PRO_OK || ev == I1PRO_USER_TRIG) {
			DBG((dbgo,"############# triggered ##############\n"))
			if (m->trig_return)
				printf("\n");
		}

	} else if (m->trig == inst_opt_trig_keyb) {
		ev = icoms2i1pro_err(icoms_poll_user(p->icom, 1));

		if (ev == I1PRO_USER_TRIG) {
			DBG((dbgo,"############# triggered ##############\n"))
			user_trig = 1;
			if (m->trig_return)
				printf("\n");
		}
	}

	if (ev != I1PRO_OK && ev != I1PRO_USER_TRIG) {
		free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
		free(mbuf);
		if (buf != NULL)
			free(buf);
		DBG((dbgo,"i1pro_imp_measure user aborted, terminated, command, or failure\n"))
		return ev;		/* User abort, term, command or failure */
	}

	if (s->emiss && !s->scan && s->adaptive) {
		int saturated = 0;
		double optscale = 1.0;
		s->inttime = 0.25;
		s->gainmode = 0;
		s->dark_valid = 0;

		DBG((dbgo,"Trial measure emission with inttime %f, gainmode %d\n",s->inttime,s->gainmode))

		/* Take a trial measurement reading using the current mode. */
		/* Used to determine if sensor is saturated, or not optimal */
		nummeas = i1pro_comp_nummeas(p, s->wreadtime, s->inttime);
		if ((ev = i1pro_trialmeasure(p, &saturated, &optscale, nummeas, &s->inttime, s->gainmode,
		                                                            s->targoscale)) != I1PRO_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			DBG((dbgo,"i1pro_imp_measure trial measure failed\n"))
			return ev;  
		}

		if (saturated) {
			s->inttime = m->min_int_time;

			DBG((dbgo,"2nd trial measure emission with inttime %f, gainmode %d\n",
			                                         s->inttime,s->gainmode))
			/* Take a trial measurement reading using the current mode. */
			/* Used to determine if sensor is saturated, or not optimal */
			nummeas = i1pro_comp_nummeas(p, s->wreadtime, s->inttime);
			if ((ev = i1pro_trialmeasure(p, &saturated, &optscale, nummeas, &s->inttime, 
			                                          s->gainmode, s->targoscale)) != I1PRO_OK) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
				free(mbuf);
				DBG((dbgo,"i1pro_imp_measure trial measure failed\n"))
				return ev;  
			}
		}

		DBG((dbgo,"Compute optimal integration time\n"))
		/* For adaptive mode, compute a new integration time and gain mode */
		/* in order to optimise the sensor values. */
		if ((ev = i1pro_optimise_sensor(p, &s->inttime, &s->gainmode, 
		         s->inttime, s->gainmode, 1, 1, s->targoscale, optscale)) != I1PRO_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			DBG((dbgo,"i1pro_imp_measure optimise sensor failed\n"))
			return ev;
		}
		DBG((dbgo,"Computed optimal emiss inttime %f and gainmode %d\n",s->inttime,s->gainmode))

		DBG((dbgo,"Interpolate dark calibration reference\n"))
		if ((ev = i1pro_interp_dark(p, s->dark_data, s->inttime, s->gainmode)) != I1PRO_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			DBG((dbgo,"i1pro_imp_measure interplate dark ref failed\n"))
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
		mbsize = 256 * maxnummeas;
		if ((mbuf = (unsigned char *)malloc(sizeof(unsigned char) * mbsize)) == NULL) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			DBG((dbgo,"i1pro_imp_measure malloc %d bytes failed (7)\n",mbsize))
			if (p->verb) printf("Malloc %d bytes failed (7)\n",mbsize);
			return I1PRO_INT_MALLOC;
		}

	} else if (s->reflective) {

		DISDPLOT

		DBG((dbgo,"Doing on the fly black calibration_1 with nummeas %d int_time %f, gainmode %d\n",
		                                                   nummeas, s->inttime, s->gainmode))

		if ((ev = i1pro_dark_measure_1(p, nummeas, &s->inttime, s->gainmode, buf, bsize))
		                                                                           != I1PRO_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(buf);
			free(mbuf);
			DBG((dbgo,"i1pro_imp_measure dak measure 1 failed\n"))
			return ev;
		}

		ENDPLOT
	}
	/* Take a measurement reading using the current mode. */
	/* Converts to completely processed output readings. */

	DBG((dbgo,"Do main measurement reading\n"))

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
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			if (buf != NULL)
				free(buf);
			free(mbuf);
			DBG((dbgo,"i1pro_imp_measure failed at i1pro_read_patches_1\n"))
			return ev;
		}

		/* Complete black reference measurement */
		if (s->reflective) {
			DBG((dbgo,"Calling black calibration_2 calc with nummeas %d, inttime %f, gainmode %d\n", nummeas, s->inttime,s->gainmode))
			if ((ev = i1pro_dark_measure_2(p, s->dark_data, nummeas, s->inttime, s->gainmode,
			                                                      buf, bsize)) != I1PRO_OK) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
				free(buf);
				free(mbuf);
				DBG((dbgo,"i1pro_imp_measure failed at i1pro_dark_measure_2\n"))
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
				DBG((dbgo,"Switching to alternate display integration time %f seconds\n",s->dark_int_time2))
				if (p->debug)
					fprintf(stderr,"Switching to alternate display integration time %f seconds\n",s->dark_int_time2);
				tv = s->inttime; s->inttime = s->dark_int_time2; s->dark_int_time2 = tv;
				tt = s->dark_data; s->dark_data = s->dark_data2; s->dark_data2 = tt;
				s->dispswap = 1;
			} else if (s->dispswap == 1) {
				DBG((dbgo,"Switching to 2nd alternate display integration time %f seconds\n",s->dark_int_time3))
				if (p->debug)
					fprintf(stderr,"Switching to alternate display integration time %f seconds\n",s->dark_int_time3);
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
			mbsize = 256 * maxnummeas;
			if ((mbuf = (unsigned char *)malloc(sizeof(unsigned char) * mbsize)) == NULL) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
				DBG((dbgo,"i1pro_imp_measure malloc %d bytes failed (7)\n",mbsize))
				if (p->verb) printf("Malloc %d bytes failed (7)\n",mbsize);
				return I1PRO_INT_MALLOC;
			}
			continue;			/* Do the measurement again */
		}

		if (ev != I1PRO_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			DBG((dbgo,"i1pro_imp_measure failed at i1pro_read_patches_2\n"))
			return ev;
		}
		break;		/* Don't repeat */
	}
	free(mbuf);

	/* Transfer spectral and convert to XYZ */
	if ((ev = i1pro_conv2XYZ(p, vals, nvals, specrd)) != I1PRO_OK) {
		free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
		DBG((dbgo,"i1pro_imp_measure failed at i1pro_conv2XYZ\n"))
		return ev;
	}
	free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
	
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
	
	DBG((dbgo,"i1pro_imp_measure sucessful return\n"))
	if (user_trig)
		return I1PRO_USER_TRIG;
	return ev; 
}

/* - - - - - - - - - - - - - - - - - - - - - - */
/* i1 refspot calibration/log stored on instrument */

/* Restore the reflective spot calibration information from the EEPRom */
/* Always returns success, even if the restore fails */
i1pro_code i1pro_restore_refspot_cal(i1pro *p) {
	int chsum1, *chsum2;
	int *ip, i, xcount;
	unsigned int count;
	double *dp;
	unsigned char buf[256];
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[i1p_refl_spot];		/* NOT current mode, refspot mode */
	i1key offst = 0;							/* Offset to copy to use */
	i1pro_code ev = I1PRO_OK;

	DBG((dbgo,"Doing Restoring reflective spot calibration information from the EEProm\n"))

	chsum1 = m->data->checksum(m->data, 0);
	if ((chsum2 = m->data->get_int(m->data, key_checksum, 0)) == NULL || chsum1 != *chsum2) {
		offst = key_2logoff;
		chsum1 = m->data->checksum(m->data, key_2logoff);
		if ((chsum2 = m->data->get_int(m->data, key_checksum + key_2logoff, 0)) == NULL
		     || chsum1 != *chsum2) {
			if (p->verb) printf("Neither EEPRom checksum was valid\n");
			return I1PRO_OK;
		}
	}

	/* Get the calibration gain mode */
	if ((ip = m->data->get_ints(m->data, &count, key_gainmode + offst)) == NULL || count < 1) {
		if (p->verb) printf("Failed to read calibration gain mode from EEPRom\n");
		return I1PRO_OK;
	}
	if (ip[0] == 0)
		s->gainmode = 1;
	else
		s->gainmode = 0;

	/* Get the calibration integrattion time */
	if ((dp = m->data->get_doubles(m->data, &count, key_inttime + offst)) == NULL || count < 1) {
		if (p->verb) printf("Failed to read calibration integration time from EEPRom\n");
		return I1PRO_OK;
	}
	s->inttime = dp[0];
	if (s->inttime < m->min_int_time)	/* Hmm. EEprom is occasionaly screwed up */
		s->inttime = m->min_int_time;

	/* Get the dark data */
	if ((ip = m->data->get_ints(m->data, &count, key_darkreading + offst)) == NULL
	                                                              || count != 128) {
		if (p->verb) printf("Failed to read calibration dark data from EEPRom\n");
		return I1PRO_OK;
	}

	/* Convert back to a single raw big endian instrument readings */
	for (i = 0; i < 128; i++) {
		buf[i * 2 + 0] = (ip[i] >> 8) & 0xff;
		buf[i * 2 + 1] = ip[i] & 0xff;
	}

	/* Convert to calibration data */
	DBG((dbgo,"Calling black calibration_2 calc with nummeas %d, inttime %f, gainmode %d\n", 1, s->inttime,s->gainmode))
	if ((ev = i1pro_dark_measure_2(p, s->dark_data, 1, s->inttime, s->gainmode,
		                                                 buf, 256)) != I1PRO_OK) {
		if (p->verb) printf("Failed to convert EEProm dark data to calibration\n");
		return I1PRO_OK;
	}

	/* We've sucessfully restored the dark calibration */
	s->dark_valid = 1;
	s->ddate = m->caldate;

	/* Get the white calibration data */
	if ((ip = m->data->get_ints(m->data, &count, key_whitereading + offst)) == NULL
	                                                               || count != 128) {
		if (p->verb) printf("Failed to read calibration white data from EEPRom\n");
		return I1PRO_OK;
	}

	/* Convert back to a single raw big endian instrument readings */
	for (i = 0; i < 128; i++) {
		buf[i * 2 + 0] = (ip[i] >> 8) & 0xff;
		buf[i * 2 + 1] = ip[i] & 0xff;
	}

	/* Convert to calibration data */
	if ((ev = i1pro_whitemeasure_buf(p, s->cal_factor1, s->cal_factor2, s->white_data,
	                s->inttime, s->gainmode, buf)) != I1PRO_OK) {
		if (p->verb) printf("Failed to convert EEProm white data to calibration\n");
		return I1PRO_OK;
	}

	/* Check a reflective white measurement, and check that */
	/* it seems reasonable. Return I1PRO_OK if it is, error if not. */
	if ((ev = i1pro_check_white_reference1(p, s->cal_factor1)) != I1PRO_OK) {
		if (p->verb) printf("Failed to convert EEProm white data to calibration\n");
		return I1PRO_OK;
	}
	/* Compute a calibration factor given the reading of the white reference. */
	i1pro_compute_white_cal(p, s->cal_factor1, m->white_ref1, s->cal_factor1,
	                           s->cal_factor2, m->white_ref2, s->cal_factor2);

	/* We've sucessfully restored the calibration */
	s->cal_valid = 1;
	s->cfdate = m->caldate;

	return I1PRO_OK;
}

/* Save the reflective spot calibration information to the EEPRom data object. */
/* Note we don't actually write to the EEProm here! */
static i1pro_code i1pro_set_log_data(i1pro *p) {
	int *ip, i, xcount;
	unsigned int count;
	double *dp;
	double absmeas[128];
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[i1p_refl_spot];		/* NOT current mode, refspot mode */
	i1key offst = 0;							/* Offset to copy to use */
	i1pro_code ev = I1PRO_OK;

	DBG((dbgo,"i1pro_set_log_data called\n"))

	if (s->dark_valid == 0 || s->cal_valid == 0)
		return I1PRO_INT_NO_CAL_TO_SAVE;

	/* Set the calibration gain mode */
	if ((ip = m->data->get_ints(m->data, &count, key_gainmode + offst)) == NULL || count < 1) {
		if (p->verb) printf("Failed to access calibration gain mode from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	if (s->gainmode == 0)
		ip[0] = 1;
	else
		ip[0] = 0;

	/* Set the calibration integration time */
	if ((dp = m->data->get_doubles(m->data, &count, key_inttime + offst)) == NULL || count < 1) {
		if (p->verb) printf("Failed to read calibration integration time from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	dp[0] = s->inttime;

	/* Set the dark data */
	if ((ip = m->data->get_ints(m->data, &count, key_darkreading + offst)) == NULL
	                                                              || count != 128) {
		if (p->verb) printf("Failed to access calibration dark data from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}

	/* Convert abs dark_data to raw data */
	if ((ev = i1pro_abssens_to_meas(p, ip, s->dark_data, s->inttime, s->gainmode)) != I1PRO_OK)
		return ev;

	/* Add back black level to white data */
	for (i = 0; i < 128; i++)
		absmeas[i] = s->white_data[i] + s->dark_data[i];

	/* Get the white data */
	if ((ip = m->data->get_ints(m->data, &count, key_whitereading + offst)) == NULL
	                                                               || count != 128) {
		if (p->verb) printf("Failed to access calibration white data from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}

	/* Convert abs white_data to raw data */
	if ((ev = i1pro_abssens_to_meas(p, ip, absmeas, s->inttime, s->gainmode)) != I1PRO_OK)
		return ev;

	/* Set all the log counters */

	/* Total Measure (Emis/Remis/Ambient/Trans/Cal) count */
	if ((ip = m->data->get_ints(m->data, &count, key_meascount)) == NULL || count < 1) {
		if (p->verb) printf("Failed to access meascount log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	ip[0] = m->meascount;

	/* Remspotcal last calibration date */
	if ((ip = m->data->get_ints(m->data, &count, key_caldate)) == NULL || count < 1) {
		if (p->verb) printf("Failed to access caldate log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	ip[0] = m->caldate;

	/* Remission spot measure count at last Remspotcal. */
	if ((ip = m->data->get_ints(m->data, &count, key_calcount)) == NULL || count < 1) {
		if (p->verb) printf("Failed to access calcount log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	ip[0] = m->calcount;

	/* Last remision spot reading integration time */
	if ((dp = m->data->get_doubles(m->data, &count, key_rpinttime)) == NULL || count < 1) {
		if (p->verb) printf("Failed to access rpinttime log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	dp[0] = m->rpinttime;

	/* Remission spot measure count */
	if ((ip = m->data->get_ints(m->data, &count, key_rpcount)) == NULL || count < 1) {
		if (p->verb) printf("Failed to access rpcount log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	ip[0] = m->rpcount;

	/* Remission scan measure count (??) */
	if ((ip = m->data->get_ints(m->data, &count, key_acount)) == NULL || count < 1) {
		if (p->verb) printf("Failed to access acount log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	ip[0] = m->acount;

	/* Total lamp usage time in seconds (??) */
	if ((dp = m->data->get_doubles(m->data, &count, key_lampage)) == NULL || count < 1) {
		if (p->verb) printf("Failed to access lampage log counter from EEPRom\n");
		return I1PRO_INT_EEPROM_DATA_MISSING;
	}
	dp[0] = m->lampage;

	DBG((dbgo,"i1pro_set_log_data done\n"))

	return I1PRO_OK;
}

/* Update the single remission calibration and instrument usage log */
i1pro_code i1pro_update_log(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_code ev = I1PRO_OK;
	unsigned char *buf;		/* Buffer to write to EEProm */
	unsigned int len;

	DBG((dbgo,"i1pro_update_log:\n"))

	/* Copy refspot calibration and log data to EEProm data store */ 
	if ((ev = i1pro_set_log_data(p)) != I1PRO_OK) {
		DBG((dbgo,"i1pro_update_log i1pro_set_log_data failed\n"))
		return ev;
	}

	/* Compute checksum and serialise into buffer ready to write */
	if ((ev = m->data->prep_section1(m->data, &buf, &len)) != I1PRO_OK) {
		DBG((dbgo,"i1pro_update_log prep_section1 failed\n"))
		return ev;
	}

	/* First copy of log */
	if ((ev = i1pro_writeEEProm(p, buf, 0x0000, len)) != I1PRO_OK) {
		DBG((dbgo,"i1pro_update_log i1pro_writeEEProm 0x0000 failed\n"))
		return ev;
	}
	/* Second copy of log */
	if ((ev = i1pro_writeEEProm(p, buf, 0x0800, len)) != I1PRO_OK) {
		DBG((dbgo,"i1pro_update_log i1pro_writeEEProm 0x0800 failed\n"))
		return ev;
	}

	free(buf);

	DBG((dbgo,"i1pro_update_log done\n"))

	return ev;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Save the calibration for all modes, stored on local system */

#ifdef ENABLE_NONVCAL

/* non-volatile save/restor state */
typedef struct {
	int ef;					/* Error flag */
	unsigned int chsum;		/* Checksum */
} i1pnonv;

static void update_chsum(i1pnonv *x, unsigned char *p, int nn) {
	int i;
	for (i = 0; i < nn; i++, p++)
		x->chsum = ((x->chsum << 13) | (x->chsum >> (32-13))) + *p;
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
	int i, j;
	char nmode[10];
	char cal_name[40+1];		/* Name */
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
	sprintf(cal_name, "color/.i1p_%d.cal", m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_write, xdg_user, cal_name)) < 1)
		return I1PRO_INT_CAL_SAVE;

	if (p->debug > 1)
		fprintf(stderr,"i1pro_save_calibration saving to file '%s'\n",cal_paths[0]);
	DBG((dbgo,"i1pro_save_calibration saving to file '%s'\n",cal_paths[0]));

	if (create_parent_directories(cal_paths[0])
	 || (fp = fopen(cal_paths[0], nmode)) == NULL) {
		DBG((dbgo,"i1pro_save_calibration failed to open file for writing\n"));
		xdg_free(cal_paths, no_paths);
		return I1PRO_INT_CAL_SAVE;
	}
	
	x.ef = 0;
	x.chsum = 0;

	/* A crude structure signature */
	ss = sizeof(i1pro_state) + sizeof(i1proimp);

	/* Some file identification */
	write_ints(&x, fp, &argyllversion, 1);
	write_ints(&x, fp, &ss, 1);
	write_ints(&x, fp, &m->serno, 1);
	write_ints(&x, fp, &m->nraw, 1);
	write_ints(&x, fp, (int *)&m->nwav1, 1);
	write_ints(&x, fp, (int *)&m->nwav2, 1);

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
		write_ints(&x, fp, &s->dark_valid, 1);
		write_time_ts(&x, fp, &s->ddate, 1);
		write_doubles(&x, fp, &s->dark_int_time, 1);
		write_doubles(&x, fp, s->dark_data, m->nraw);
		write_doubles(&x, fp, &s->dark_int_time2, 1);
		write_doubles(&x, fp, s->dark_data2, m->nraw);
		write_doubles(&x, fp, &s->dark_int_time3, 1);
		write_doubles(&x, fp, s->dark_data3, m->nraw);
		write_ints(&x, fp, &s->dark_gain_mode, 1);

		if (!s->emiss) {
			write_ints(&x, fp, &s->cal_valid, 1);
			write_time_ts(&x, fp, &s->cfdate, 1);
			write_doubles(&x, fp, s->cal_factor1, m->nwav1);
			write_doubles(&x, fp, s->cal_factor2, m->nwav2);
			write_doubles(&x, fp, s->white_data, m->nraw);
		}
		
		write_ints(&x, fp, &s->idark_valid, 1);
		write_time_ts(&x, fp, &s->iddate, 1);
		write_doubles(&x, fp, s->idark_int_time, 4);
		write_doubles(&x, fp, s->idark_data[0], m->nraw);
		write_doubles(&x, fp, s->idark_data[1], m->nraw);
		write_doubles(&x, fp, s->idark_data[2], m->nraw);
		write_doubles(&x, fp, s->idark_data[3], m->nraw);
	}

	DBG((dbgo,"Checkum = 0x%x\n",x.chsum))
	write_ints(&x, fp, (int *)&x.chsum, 1);

	if (x.ef != 0) {
		if (p->debug > 1)
			fprintf(stderr,"Writing calibration file failed\n");
		DBG((dbgo,"Writing calibration file failed\n"))
		return I1PRO_INT_CAL_SAVE;
		fclose(fp);
		delete_file(cal_paths[0]);
	} else {
		fclose(fp);
		DBG((dbgo,"Writing calibration file done\n"))
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
	char cal_name[40+1];		/* Name */
	char **cal_paths = NULL;
	int no_paths = 0;
	FILE *fp;
	i1pnonv x;
	int argyllversion;
	int ss, serno, nraw, nwav1, nwav2, chsum1, chsum2;

	strcpy(nmode, "r");
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	strcat(nmode, "b");
#endif
	/* Create the file name */
	sprintf(cal_name, "color/.i1p_%d.cal", m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_read, xdg_user, cal_name)) < 1)
		return I1PRO_INT_CAL_RESTORE;

	if (p->debug > 1)
		fprintf(stderr,"i1pro_restore_calibration restoring from file '%s'\n",cal_paths[0]);
	DBG((dbgo,"i1pro_restore_calibration restoring from file '%s'\n",cal_paths[0]));

	if ((fp = fopen(cal_paths[0], nmode)) == NULL) {
		DBG((dbgo,"i1pro_restore_calibration failed to open file for reading\n"));
		xdg_free(cal_paths, no_paths);
		return I1PRO_INT_CAL_RESTORE;
	}

	x.ef = 0;
	x.chsum = 0;

	/* Check the file identification */
	read_ints(&x, fp, &argyllversion, 1);
	read_ints(&x, fp, &ss, 1);
	read_ints(&x, fp, &serno, 1);
	read_ints(&x, fp, &nraw, 1);
	read_ints(&x, fp, &nwav1, 1);
	read_ints(&x, fp, &nwav2, 1);
	if (x.ef != 0
	 || argyllversion != ARGYLL_VERSION
	 || ss != (sizeof(i1pro_state) + sizeof(i1proimp))
	 || serno != m->serno
	 || nraw != m->nraw
	 || nwav1 != m->nwav1
	 || nwav2 != m->nwav2) {
		DBG((dbgo,"Identification didn't verify\n"));
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
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
		read_doubles(&x, fp, &dd, 1);
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
		read_doubles(&x, fp, &dd, 1);
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
		read_ints(&x, fp, &di, 1);

		if (!s->emiss) {
			read_ints(&x, fp, &di, 1);
			read_time_ts(&x, fp, &dt, 1);
			for (j = 0; j < m->nwav1; j++)
				read_doubles(&x, fp, &dd, 1);
			for (j = 0; j < m->nwav2; j++)
				read_doubles(&x, fp, &dd, 1);
			for (j = 0; j < m->nraw; j++)
				read_doubles(&x, fp, &dd, 1);
		}
		
		read_ints(&x, fp, &di, 1);
		read_time_ts(&x, fp, &dt, 1);
		for (j = 0; j < 4; j++)
			read_doubles(&x, fp, &dd, 1);
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);
	}

	chsum1 = x.chsum;
	read_ints(&x, fp, &chsum2, 1);
	
	if (x.ef != 0
	 || chsum1 != chsum2) {
		if (p->debug > 1)
			fprintf(stderr,"Unable to restore previous calibration due to checksum error\n");
		DBG((dbgo,"Checksum didn't verify, got 0x%x, expected 0x%x\n",chsum1, chsum2));
		goto reserr;
	}

	rewind(fp);

	/* Allocate space in temp structure */

	ts.dark_data = dvectorz(0, m->nraw-1);  
	ts.dark_data2 = dvectorz(0, m->nraw-1);  
	ts.dark_data3 = dvectorz(0, m->nraw-1);  
	ts.cal_factor1 = dvectorz(0, m->nwav1-1);
	ts.cal_factor2 = dvectorz(0, m->nwav2-1);
	ts.white_data = dvectorz(0, m->nraw-1);
	ts.idark_data = dmatrixz(0, 3, 0, m->nraw-1);  

	/* Read the identification */
	read_ints(&x, fp, &argyllversion, 1);
	read_ints(&x, fp, &ss, 1);
	read_ints(&x, fp, &m->serno, 1);
	read_ints(&x, fp, &m->nraw, 1);
	read_ints(&x, fp, (int *)&m->nwav1, 1);
	read_ints(&x, fp, (int *)&m->nwav2, 1);

	/* For each mode, save the calibration if it's valid */
	for (i = 0; i < i1p_no_modes; i++) {
		double dd, inttime;
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

		/* Static Dark */
		read_ints(&x, fp, &ts.dark_valid, 1);
		read_time_ts(&x, fp, &ts.ddate, 1);
		read_doubles(&x, fp, &ts.dark_int_time, 1);
		read_doubles(&x, fp, ts.dark_data, m->nraw);
		read_doubles(&x, fp, &ts.dark_int_time2, 1);
		read_doubles(&x, fp, ts.dark_data2, m->nraw);
		read_doubles(&x, fp, &ts.dark_int_time3, 1);
		read_doubles(&x, fp, ts.dark_data3, m->nraw);
		read_ints(&x, fp, &ts.dark_gain_mode, 1);

		if (!ts.emiss) {
			/* Reflective */
			read_ints(&x, fp, &ts.cal_valid, 1);
			read_time_ts(&x, fp, &ts.cfdate, 1);
			read_doubles(&x, fp, ts.cal_factor1, m->nwav1);
			read_doubles(&x, fp, ts.cal_factor2, m->nwav2);
			read_doubles(&x, fp, ts.white_data, m->nraw);
		}
		
		/* Adaptive Dark */
		read_ints(&x, fp, &ts.idark_valid, 1);
		read_time_ts(&x, fp, &ts.iddate, 1);
		read_doubles(&x, fp, ts.idark_int_time, 4);
		read_doubles(&x, fp, ts.idark_data[0], m->nraw);
		read_doubles(&x, fp, ts.idark_data[1], m->nraw);
		read_doubles(&x, fp, ts.idark_data[2], m->nraw);
		read_doubles(&x, fp, ts.idark_data[3], m->nraw);

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
			s->dark_valid = ts.dark_valid;
			s->ddate = ts.ddate;
			s->dark_int_time = ts.dark_int_time;
			for (j = 0; j < m->nraw; j++)
				s->dark_data[j] = ts.dark_data[j];
			s->dark_int_time2 = ts.dark_int_time2;
			for (j = 0; j < m->nraw; j++)
				s->dark_data2[j] = ts.dark_data2[j];
			s->dark_int_time3 = ts.dark_int_time3;
			for (j = 0; j < m->nraw; j++)
				s->dark_data3[j] = ts.dark_data3[j];
			s->dark_gain_mode = ts.dark_gain_mode;
			if (!ts.emiss) {
				s->cal_valid = ts.cal_valid;
				s->cfdate = ts.cfdate;
				for (j = 0; j < m->nwav1; j++)
					s->cal_factor1[j] = ts.cal_factor1[j];
				for (j = 0; j < m->nwav2; j++)
					s->cal_factor2[j] = ts.cal_factor2[j];
				for (j = 0; j < m->nraw; j++)
					s->white_data[j] = ts.white_data[j];
			}
			s->idark_valid = ts.idark_valid;
			s->iddate = ts.iddate;
			for (j = 0; j < 4; j++)
				s->idark_int_time[j] = ts.idark_int_time[j];
			for (j = 0; j < m->nraw; j++)
				s->idark_data[0][j] = ts.idark_data[0][j];
			for (j = 0; j < m->nraw; j++)
				s->idark_data[1][j] = ts.idark_data[1][j];
			for (j = 0; j < m->nraw; j++)
				s->idark_data[2][j] = ts.idark_data[2][j];
			for (j = 0; j < m->nraw; j++)
				s->idark_data[3][j] = ts.idark_data[3][j];

		} else {
			DBG((dbgo,"Not restoring cal for mode %d since params don't match:\n",i));
			DBG((dbgo,"emis = %d : %d, trans = %d : %d, ref = %d : %d\n",s->emiss,ts.emiss,s->trans,ts.trans,s->reflective,ts.reflective));
			DBG((dbgo,"scan = %d : %d, flash = %d : %d, ambi = %d : %d, adapt = %d : %d\n",s->scan,ts.scan,s->flash,ts.flash,s->ambient,ts.ambient,s->adaptive,ts.adaptive));
			DBG((dbgo,"inttime = %f : %f\n",s->inttime,ts.inttime));
			DBG((dbgo,"darkit1 = %f : %f, 2 = %f : %f, 3 = %f : %f\n",s->dark_int_time,ts.dark_int_time,s->dark_int_time2,ts.dark_int_time2,s->dark_int_time3,ts.dark_int_time3));
			DBG((dbgo,"idarkit0 = %f : %f, 1 = %f : %f, 2 = %f : %f, 3 = %f : %f\n",s->idark_int_time[0],ts.idark_int_time[0],s->idark_int_time[1],ts.idark_int_time[1],s->idark_int_time[2],ts.idark_int_time[2],s->idark_int_time[3],ts.idark_int_time[3]));
		}
	}

	/* Free up temporary space */
	free_dvector(ts.dark_data, 0, m->nraw-1);  
	free_dvector(ts.dark_data2, 0, m->nraw-1);  
	free_dvector(ts.dark_data3, 0, m->nraw-1);  
	free_dvector(ts.white_data, 0, m->nraw-1);
	free_dmatrix(ts.idark_data, 0, 3, 0, m->nraw-1);  

	free_dvector(ts.cal_factor1, 0, m->nwav1-1);
	free_dvector(ts.cal_factor2, 0, m->nwav2-1);

	DBG((dbgo,"i1pro_restore_calibration done\n"))
 reserr:;

	fclose(fp);
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

	if (p->debug) fprintf(stderr,"Switching to high power mode\n");

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
		
	if ((ev = i1pro_trigger_one_measure(p, nummeas, inttime, gainmode, 1, 1)) != I1PRO_OK)
		return ev;

	if ((ev = i1pro_readmeasurement(p, nummeas, 0, buf, bsize, NULL, 1, 1)) != I1PRO_OK)
		return ev;

	return ev;
}

/* Take a dark reference measurement - part 2 */
i1pro_code i1pro_dark_measure_2(
	i1pro *p,
	double *abssens,		/* Return array [nraw] of abssens values */
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

	multimes = dmatrix(0, nummeas-1, 0, m->nraw-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* absolute linearised sensor values. */
	i1pro_meas_to_abssens(p, multimes, buf, nummeas, inttime, gainmode);

	if (gainmode == 0)
		satthresh = m->sens_sat0;
	else
		satthresh = m->sens_sat1;
	satthresh = i1pro_raw_to_abssens(p, satthresh, inttime, gainmode);  

	darkthresh = m->sens_dark + inttime * 900.0;
	if (gainmode)
		darkthresh *= m->highgain;
	darkthresh = i1pro_raw_to_abssens(p, darkthresh, inttime, gainmode);  

	/* Average a set of measurements into one. */
	/* Return zero if readings are consistent and not saturated. */
	/* Return nz with bit 1 set if the readings are not consistent */
	/* Return nz with bit 2 set if the readings are saturated */
	/* Return the highest individual element. */
	/* Return the overall average. */
	rv = i1pro_average_multimeas(p, abssens, multimes, nummeas, NULL, &sensavg,
	                                                       satthresh, darkthresh);     
	free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);

#ifdef PLOT_DEBUG
	printf("Average absolute sensor readings, average = %f, satthresh %f:\n",sensavg, satthresh);
	plot_raw(abssens);
#endif

	if (rv & 1)
		return I1PRO_RD_DARKREADINCONS;

	if (rv & 2)
		return I1PRO_RD_SENSORSATURATED;

	DBG((dbgo,"Dark threshold = %f\n",darkthresh))

	if (sensavg > darkthresh)
		return I1PRO_RD_DARKNOTVALID;

	return ev;
}

/* Take a dark reference measurement (combined parts 1 & 2) */
i1pro_code i1pro_dark_measure(
	i1pro *p,
	double *abssens,		/* Return array [nraw] of abssens values */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode			/* Gain mode to use, 0 = normal, 1 = high */
) {
	i1pro_code ev = I1PRO_OK;
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;

	bsize = 256 * nummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		DBG((dbgo,"i1pro_dark_measure malloc %d bytes failed (8)\n",bsize))
		if (p->verb) printf("Malloc %d bytes failed (8)\n",bsize);
		return I1PRO_INT_MALLOC;
	}

	if ((ev = i1pro_dark_measure_1(p, nummeas, inttime, gainmode, buf, bsize)) != I1PRO_OK) {
		free(buf);
		return ev;
	}

	if ((ev = i1pro_dark_measure_2(p, abssens, nummeas, *inttime, gainmode, buf, bsize))
	                                                                           != I1PRO_OK) {
		free(buf);
		return ev;
	}
	free(buf);

	return ev;
}


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
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	double **multimes;		/* Multiple measurement results */
	int rv;

	DBG((dbgo,"i1pro_whitemeasure called \n"))

	{
		if (nummeas <= 0)
			return I1PRO_INT_ZEROMEASURES;
			
		/* Allocate temporaries up front to avoid delay between trigger and read */
		bsize = 256 * nummeas;
		if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
			DBG((dbgo,"i1pro_whitemeasure malloc %d bytes failed (10)\n",bsize))
			if (p->verb) printf("Malloc %d bytes failed (10)\n",bsize);
			return I1PRO_INT_MALLOC;
		}
		multimes = dmatrix(0, nummeas-1, 0, m->nraw-1);
	
		DBG((dbgo,"Triggering measurement cycle, nummeas %d, inttime %f, gainmode %d\n",
		                                              nummeas, *inttime, gainmode))
	
		if ((ev = i1pro_trigger_one_measure(p, nummeas, inttime, gainmode, 1, 0)) != I1PRO_OK) {
			free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
			free(buf);
			return ev;
		}
	
		DBG((dbgo,"Gathering readings\n"))
	
		if ((ev = i1pro_readmeasurement(p, nummeas, 0, buf, bsize, NULL, 1, 0)) != I1PRO_OK) {
			free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
			free(buf);
			return ev;
		}
	

		/* Take a buffer full of raw readings, and convert them to */
		/* absolute linearised sensor values. */
		i1pro_meas_to_abssens(p, multimes, buf, nummeas, *inttime, gainmode);

#ifdef PLOT_DEBUG
		printf("Dark data:\n");
		plot_raw(s->dark_data);
#endif

		/* Subtract the black level */
		i1pro_sub_abssens(p, nummeas, multimes, s->dark_data);
	}

	/* Convert linearised white value into output wavelength white reference */
	ev = i1pro_whitemeasure_3(p, abswav1, abswav2, absraw, optscale, nummeas,
	                                             *inttime, gainmode, targoscale, multimes);

	free(buf);
	free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);

	return ev;
}

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
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	double *meas;		/* Multiple measurement results */

	DBG((dbgo,"i1pro_whitemeasure_buf called \n"))

	meas = dvector(0, m->nraw-1);
	
	/* Take a buffer full of raw readings, and convert them to */
	/* absolute linearised sensor values. */
	i1pro_meas_to_abssens(p, &meas, buf, 1, inttime, gainmode);

	/* Subtract the black level */
	i1pro_sub_abssens(p, 1, &meas, s->dark_data);

	/* Convert linearised white value into output wavelength white reference */
	ev = i1pro_whitemeasure_3(p, abswav1, abswav2, absraw, NULL, 1, inttime, gainmode,
	                                                                    0.0, &meas);

	free_dvector(meas, 0, m->nraw-1);

	return ev;
}

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
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	double highest;			/* Highest of sensor readings */
	double sensavg;			/* Overall average of sensor readings */
	double satthresh;		/* Saturation threshold */
	double darkthresh;		/* Dark threshold */
	double opttarget;		/* Optimal sensor target */
	int rv;

	DBG((dbgo,"i1pro_whitemeasure_3 called \n"))

	if (gainmode == 0)
		satthresh = m->sens_sat0;
	else
		satthresh = m->sens_sat1;
	satthresh = i1pro_raw_to_abssens(p, satthresh, inttime, gainmode);  

	darkthresh = m->sens_dark + inttime * 900.0;
	if (gainmode)
		darkthresh *= m->highgain;
	darkthresh = i1pro_raw_to_abssens(p, darkthresh, inttime, gainmode);  

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
	if (abswav1 != NULL) {
		i1pro_abssens_to_abswav1(p, 1, &abswav1, &absraw);

#ifdef PLOT_DEBUG
		printf("Converted to wavelengths std res:\n");
		plot_wav1(m, abswav1);
#endif
	}

#ifdef HIGH_RES
	if (abswav2 != NULL && m->hr_inited) {
		i1pro_abssens_to_abswav2(p, 1, &abswav2, &absraw);

#ifdef PLOT_DEBUG
		printf("Converted to wavelengths high res:\n");
		plot_wav2(m, abswav2);
#endif
	}
#endif /* HIGH_RES */

	if (optscale != NULL) {
		double lhighest = highest;

		if (lhighest < 1.0)
			lhighest = 1.0;

		/* Compute correction factor to make sensor optimal */
		opttarget = i1pro_raw_to_abssens(p, (double)m->sens_target, inttime, gainmode);  
		opttarget *= targoscale;
	
	
		DBG((dbgo,"Optimal target = %f, amount to scale = %f\n",opttarget, opttarget/lhighest))

		*optscale = opttarget/lhighest; 
	}

	return ev;
}

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
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	int rv = 0;

	if (minnummeas <= 0)
		return I1PRO_INT_ZEROMEASURES;
	if (minnummeas > maxnummeas)
		maxnummeas = minnummeas;
		
	DBG((dbgo,"Triggering & gathering cycle, minnummeas %d, inttime %f, gainmode %d\n",
	                                              minnummeas, *inttime, gainmode))

	if ((ev = i1pro_trigger_one_measure(p, minnummeas, inttime, gainmode, 0, 0)) != I1PRO_OK) {
		return ev;
	}

	if ((ev = i1pro_readmeasurement(p, minnummeas, m->c_measmodeflags & I1PRO_MMF_SCAN,
	                                             buf, bsize, nmeasuered, 0, 0)) != I1PRO_OK) {
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
	double **multimes;		/* Multiple measurement results [maxnummeas|nmeasuered][nraw]*/
	double **abssens;		/* Linearsised absolute sensor raw values [numpatches][nraw]*/
	double satthresh;		/* Saturation threshold */
	double darkthresh;		/* Dark threshold (for consistency checking) */
	int rv = 0;

	if (duration != NULL)
		*duration = 0.0;	/* default value */

	/* Allocate temporaries */
	multimes = dmatrix(0, nmeasuered-1, 0, m->nraw-1);
	abssens = dmatrix(0, numpatches-1, 0, m->nraw-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* absolute linearised sensor values. */
	i1pro_meas_to_abssens(p, multimes, buf, nmeasuered, inttime, gainmode);

	/* Subtract the black level */
	i1pro_sub_abssens(p, nmeasuered, multimes, s->dark_data);


#ifdef DUMP_SCANV
	/* Dump raw scan readings to a file "i1pdump.txt" */
	{
		int i, j;
		FILE *fp;
		
		if ((fp = fopen("i1pdump.txt", "w")) == NULL)
			error("Unable to open debug file i1pdump.txt");

		for (i = 0; i < nmeasuered; i++) {
			fprintf(fp, "%d	",i);
			for (j = 0; j < m->nraw; j++) {
				fprintf(fp, "%f	",multimes[i][j]);
			}
			fprintf(fp,"\n");
		}
		fclose(fp);
	}
#endif
	if (gainmode == 0)
		satthresh = m->sens_sat0;
	else
		satthresh = m->sens_sat1;
	satthresh = i1pro_raw_to_abssens(p, satthresh, inttime, gainmode);  

	darkthresh = m->sens_dark + inttime * 900.0;
	if (gainmode)
		darkthresh *= m->highgain;
	darkthresh = i1pro_raw_to_abssens(p, darkthresh, inttime, gainmode);  

	if (!s->scan) {
		if (numpatches != 1) {
			free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
			free_dmatrix(multimes, 0, nmeasuered-1, 0, m->nraw-1);
			DBG((dbgo,"i1pro_read_patches_2 spot read failed because numpatches != 1\n"))
			return I1PRO_INT_WRONGPATCHES;
		}

		/* Average a set of measurements into one. */
		/* Return zero if readings are consistent and not saturated. */
		/* Return nz with bit 1 set if the readings are not consistent */
		/* Return nz with bit 2 set if the readings are saturated */
		/* Return the highest individual element. */
		/* Return the overall average. */
		rv = i1pro_average_multimeas(p, abssens[0], multimes, nmeasuered, NULL, NULL,
	                                                            satthresh, darkthresh);     
	} else {
		if (s->flash) {

			if (numpatches != 1) {
				free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
				free_dmatrix(multimes, 0, nmeasuered-1, 0, m->nraw-1);
				DBG((dbgo,"i1pro_read_patches_2 spot read failed because numpatches != 1\n"))
				return I1PRO_INT_WRONGPATCHES;
			}
			if ((ev = i1pro_extract_patches_flash(p, &rv, duration, abssens[0], multimes,
			                                                 nmeasuered, inttime)) != I1PRO_OK) {
				free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
				free_dmatrix(multimes, 0, nmeasuered-1, 0, m->nraw-1);
				DBG((dbgo,"i1pro_read_patches_2 spot read failed at i1pro_extract_patches_flash\n"))
				return ev;
			}

		} else {
			DBG((dbgo,"Number of patches measured = %d\n",nmeasuered))


			/* Recognise the required number of ref/trans patch locations, */
			/* and average the measurements within each patch. */
			/* Return zero if readings are consistent and not saturated. */
			/* Return nz with bit 1 set if the readings are not consistent */
			/* Return nz with bit 2 set if the readings are saturated */
			/* Return the highest individual element. */
			/* (Note white_data is just for normalization) */
			if ((ev = i1pro_extract_patches_multimeas(p, &rv, abssens, numpatches, multimes,
			                            nmeasuered, NULL, satthresh, inttime)) != I1PRO_OK) {
				free_dmatrix(multimes, 0, nmeasuered-1, 0, m->nraw-1);
				free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
				DBG((dbgo,"i1pro_read_patches_2 spot read failed at i1pro_extract_patches_multimeas\n"))
				return ev;
			}
		}
	}
	free_dmatrix(multimes, 0, nmeasuered-1, 0, m->nraw-1);

	if (rv & 1) {
		free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
		DBG((dbgo,"i1pro_read_patches_2 spot read failed with inconsistent readings\n"))
		return I1PRO_RD_READINCONS;
	}

	if (rv & 2) {
		free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
		DBG((dbgo,"i1pro_read_patches_2 spot read failed with sensor saturated\n"))
		return I1PRO_RD_SENSORSATURATED;
	}

	/* Convert an abssens array from raw wavelengths to output wavelenths */
	i1pro_abssens_to_abswav(p, numpatches, specrd, abssens);
	free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);

#ifdef APPEND_MEAN_EMMIS_VAL
	/* Append averaged emission reading to file "i1pdump.txt" */
	{
		int i, j;
		FILE *fp;
		
		/* Create wavelegth label */
		if ((fp = fopen("i1pdump.txt", "r")) == NULL) {
			if ((fp = fopen("i1pdump.txt", "w")) == NULL)
				error("Unable to reate debug file i1pdump.txt");
			for (j = 0; j < m->nwav; j++)
				fprintf(fp,"%f ",XSPECT_WL(m->wl_short, m->wl_long, m->nwav, j));
			fprintf(fp,"\n");
			fclose(fp);
		}
		if ((fp = fopen("i1pdump.txt", "a")) == NULL)
			error("Unable to open debug file i1pdump.txt");

		for (j = 0; j < m->nwav; j++)
			fprintf(fp, "%f	",specrd[0][j] * m->emis_coef[j]);
		fprintf(fp,"\n");
		fclose(fp);
	}
#endif

#ifdef PLOT_DEBUG
	printf("Converted to wavelengths:\n");
	plot_wav(m, specrd[0]);
#endif


	/* Scale to the calibrated output values */
	i1pro_scale_specrd(p, specrd, numpatches, specrd);

#ifdef PLOT_DEBUG
	printf("Calibrated measuerment spectra:\n");
	plot_wav(m, specrd[0]);
#endif

	return ev;
}

/* Take a measurement reading using the current mode (combined parts 1 & 2) */
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
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	int nmeasuered;			/* Number actually measured */
	double satthresh;		/* Saturation threshold */
	double darkthresh;		/* Dark threshold (for consistency checking) */
	int rv = 0;

	if (minnummeas <= 0)
		return I1PRO_INT_ZEROMEASURES;
	if (minnummeas > maxnummeas)
		maxnummeas = minnummeas;
		
	/* Allocate temporaries */
	bsize = 256 * maxnummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		DBG((dbgo,"i1pro_read_patches malloc %d bytes failed (11)\n",bsize))
		if (p->verb) printf("Malloc %d bytes failed (11)\n",bsize);
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
	double *abssens;		/* Linearsised absolute sensor raw values */
	int nmeasuered;			/* Number actually measured */
	double highest;			/* Highest of sensor readings */
	double sensavg;			/* Overall average of sensor readings */
	double satthresh;		/* Saturation threshold */
	double darkthresh;		/* Dark threshold */
	double opttarget;		/* Optimal sensor target */
	int rv;

	if (nummeas <= 0)
		return I1PRO_INT_ZEROMEASURES;
		
	/* Allocate up front to avoid delay between trigger and read */
	bsize = 256 * nummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		DBG((dbgo,"i1pro_trialmeasure malloc %d bytes failed (12)\n",bsize))
		if (p->verb) printf("Malloc %d bytes failed (12)\n",bsize);
		return I1PRO_INT_MALLOC;
	}
	multimes = dmatrix(0, nummeas-1, 0, m->nraw-1);
	abssens = dvector(0, m->nraw-1);

	DBG((dbgo,"Triggering measurement cycle, nummeas %d, inttime %f, gainmode %d\n",
	                                              nummeas, *inttime, gainmode))

	if ((ev = i1pro_trigger_one_measure(p, nummeas, inttime, gainmode, 1, 0)) != I1PRO_OK) {
		free_dvector(abssens, 0, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
		free(buf);
		return ev;
	}

	DBG((dbgo,"Gathering readings\n"))
	if ((ev = i1pro_readmeasurement(p, nummeas, m->c_measmodeflags & I1PRO_MMF_SCAN,
	                                             buf, bsize, &nmeasuered, 1, 0)) != I1PRO_OK) {
		free_dvector(abssens, 0, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
		free(buf);
		return ev;
	}

	/* Take a buffer full of raw readings, and convert them to */
	/* absolute linearised sensor values. */
	i1pro_meas_to_abssens(p, multimes, buf, nmeasuered, *inttime, gainmode);

	/* Comute dark subtraction for this trial's parameters */
	if ((ev = i1pro_interp_dark(p, s->dark_data, s->inttime, s->gainmode)) != I1PRO_OK) {
		free_dvector(abssens, 0, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
		free(buf);
		DBG((dbgo,"i1pro_imp_measure interplate dark ref failed\n"))
		return ev;
	}

	/* Subtract the black level */
	i1pro_sub_abssens(p, nummeas, multimes, s->dark_data);

	if (gainmode == 0)
		satthresh = m->sens_sat0;
	else
		satthresh = m->sens_sat1;
	satthresh = i1pro_raw_to_abssens(p, satthresh, *inttime, gainmode);  

	darkthresh = m->sens_dark + *inttime * 900.0;
	if (gainmode)
		darkthresh *= m->highgain;
	darkthresh = i1pro_raw_to_abssens(p, darkthresh, *inttime, gainmode);  

	/* Average a set of measurements into one. */
	/* Return zero if readings are consistent and not saturated. */
	/* Return nz with bit 1 set if the readings are not consistent */
	/* Return nz with bit 2 set if the readings are saturated */
	/* Return the highest individual element. */
	/* Return the overall average. */
	rv = i1pro_average_multimeas(p, abssens, multimes, nmeasuered, &highest, &sensavg,
	                                                            satthresh, darkthresh);     
#ifdef PLOT_DEBUG
	printf("Average absolute sensor readings, average = %f, satthresh %f:\n",sensavg, satthresh);
	plot_raw(abssens);
#endif

	if (saturated != NULL) {
		*saturated = 0;
		if (rv & 2)
			*saturated = 1;
	}

	/* Compute correction factor to make sensor optimal */
	opttarget = (double)m->sens_target * targoscale;
	opttarget = i1pro_raw_to_abssens(p, opttarget, *inttime, gainmode);  

	if (optscale != NULL) {
		double lhighest = highest;

		if (lhighest < 1.0)
			lhighest = 1.0;

		*optscale = opttarget/lhighest; 
	}

	free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
	free_dvector(abssens, 0, m->nraw-1);
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
	int calib_measure,		/* flag - nz if this is a calibration measurement */
	int dark_measure		/* flag - nz if this is a dark measurement */
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
			/* Configure a clock mode that gives us an optimal integration time ? */
			for(mcmode = 1;; mcmode++) {
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
			if (p->debug > 1)
				fprintf(stderr,"Switched to perfect mode, subtmode flag = 0x%x\n",subtmodeflags & 0x01);
			if (subtmodeflags & 0x01)
				m->subtmode = 1;	/* Last reading subtract mode */
		}
	}

	/* Compute integration clocks */
	dintclocks = floor(*inttime/m->intclkp + 0.5);
	if (dintclocks > 65535.0)
		return I1PRO_INT_INTTOOBIG;
	intclocks = (int)dintclocks;
	*inttime = dintclocks * m->intclkp;		/* Quantized integration time */

	dlampclocks = floor(s->lamptime/(m->subclkdiv * m->intclkp) + 0.5);
	if (dlampclocks > 256.0)		/* Clip - not sure why. Silly value anyway */
		dlampclocks = 256.0;
	lampclocks = (int)dlampclocks;
//printf("~1 lampclocks = %d = 0x%x\n",lampclocks,lampclocks);
	s->lamptime = dlampclocks * m->subclkdiv * m->intclkp;	/* Quantized lamp time */

	if (nummeas > 65535)
		nummeas = 65535;		/* Or should we error ? */

	/* Create measurement mode flag byte for this operation */
	measmodeflags = 0;
	if (s->scan && !calib_measure)
		measmodeflags |= I1PRO_MMF_SCAN;		/* Never scan on a calibration */
	if (!s->reflective || dark_measure)
		measmodeflags |= I1PRO_MMF_NOLAMP;		/* No lamp if not reflective or dark measure */
	if (gainmode == 0)
		measmodeflags |= I1PRO_MMF_GAINMODE;	/* Normal gain mode */

	/* Do a setmeasparams */
#ifdef NEVER
	if (intclocks != m->c_intclocks				/* If any parameters have changed */
	 || lampclocks != m->c_lampclocks
	 || nummeas != m->c_nummeas
	 || measmodeflags != m->c_measmodeflags)
#endif /* NEVER */
	{

		/* Set the hardware for measurement */
		if ((ev = i1pro_setmeasparams(p, intclocks, lampclocks, nummeas, measmodeflags)) != I1PRO_OK)
			return ev;
	
		m->c_intclocks = intclocks;
		m->c_lampclocks = lampclocks;
		m->c_nummeas = nummeas;
		m->c_measmodeflags = measmodeflags;

		m->c_inttime = *inttime;		/* Special harware is configured */
		m->c_lamptime = s->lamptime;
	}

	/* If the lamp needs to be off, make sure at least 1.5 seconds */
	/* have elapsed since it was last on, to make sure it's dark. */
	if ((measmodeflags & I1PRO_MMF_NOLAMP)
	 && (timssinceoff = (msec_time() - m->llamponoff)) < LAMP_OFF_TIME) {
		if (p->debug >= 2)
			fprintf(stderr,"Sleep %d msec for lamp cooldown\n",LAMP_OFF_TIME - timssinceoff);
		msec_sleep(LAMP_OFF_TIME - timssinceoff);	/* Make sure time adds up to 1.5 seconds */
	}

	/* Trigger a measurement */
	if ((ev = i1pro_triggermeasure(p, TRIG_DELAY)) != I1PRO_OK)
		return ev;

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
	val = buf[0];
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[3]));
	return val;
}

/* Take a short sized buffer, and convert it to an int */
static int buf2short(unsigned char *buf) {
	int val;
	val = buf[0];
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

#ifdef RAWR_DEBUG
#define RRDBG(xxx) fprintf xxx ;
#else
#define RRDBG(xxx) 
#endif	/* RAWR_DEBUG */

/* Take a buffer full of raw readings, and convert them to */
/* absolute linearised sensor values. */
void i1pro_meas_to_abssens(
	i1pro *p,
	double **abssens,		/* Array of [nummeas][nraw] value to return */
	unsigned char *buf,		/* Raw measurement data must be 256 * nummeas */
	int nummeas,			/* Return number of readings measured */
	double inttime, 		/* Integration time used */
	int gainmode			/* Gain mode, 0 = normal, 1 = high */
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k;
	unsigned char *bp;
	unsigned int maxpve = m->maxpve;	/* maximum +ve sensor value + 1 */ 
	double avlastv = 0.0;
	double gain;
	int npoly;			/* Number of linearisation coefficients */
	double *polys;		/* the coeficients */
	double scale;		/* Absolute scale value */

	/* if subtmode is set, compute the average last reading raw value. */
	/* Could this be some sort of temperature compensation offset ??? */

	/* For a scan it seems kind of strange to subtract the average of all */
	/* the last value readings. We could try subtracting per buffer */
	/* instead ? */

	/* (This value seem similar to that returned by GetMisc unkn1) */
	/* (Not sure if it's reasonable to extend the sign and then do this */
	/* computation, or whether it makes any difference. Put some asserts */
	/* in for now to detect if it makes a difference. */
	if (m->subtmode) {
		for (bp = buf + 254, i = 0; i < nummeas; i++, bp += 256) {
			unsigned int lastv;
			lastv = buf2ushort(bp);
			if (lastv >= maxpve) {
				lastv -= 0x00010000;	/* Convert to -ve */
				DBG((dbgo,"Assert warning :- last value of measurement is -ve !\n"))
			}
			avlastv += (double)lastv;
		}
		avlastv /= (double)nummeas;
		if (p->debug > 2)
			fprintf(stderr,"subtmode got avlastv = %f\n",avlastv);
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

	/* Now process the buffer values */
	for (bp = buf + 2, i = 0; i < nummeas; i++, bp += 4) {
		for (j = 1; j < 127; j++, bp += 2)  {
			unsigned int rval;
			double fval, lval;

			rval = buf2ushort(bp);
			RRDBG((dbgo,"% 3d:rval 0x%x, ",j, rval))
			if (rval >= maxpve)
				rval -= 0x00010000;	/* Convert to -ve */
			RRDBG((dbgo,"srval 0x%x, ",rval))
			fval = (double)(int)rval;
			RRDBG((dbgo,"fval %.0f, ",fval))
			fval -= avlastv;
			RRDBG((dbgo,"fval-av %.0f, ",fval))

#ifdef ENABLE_NONLINCOR	
			/* Linearise */
			for (lval = polys[npoly-1], k = npoly-2; k >= 0; k--)
				lval = lval * fval + polys[k];
#else
			lval = fval;
#endif
			RRDBG((dbgo,"lval %.1f, ",lval))

			/* And scale to be an absolute sensor reading */
			abssens[i][j] = lval * scale;
			RRDBG((dbgo,"absval %.1f\n",lval * scale))
		}
		/* Duplicate first and last values in buffer to make up to 128 */
		abssens[i][0] = abssens[i][1];
		abssens[i][127] = abssens[i][126];
	}
}

#ifdef RAWR_DEBUG
#undef RRDBG
#endif

/* Take a raw sensor value, and convert it into an absolute sensor value */
double i1pro_raw_to_abssens(
	i1pro *p,
	double raw,				/* Input value */
	double inttime, 		/* Integration time used */
	int gainmode			/* Gain mode, 0 = normal, 1 = high */
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k;
	double gain;
	int npoly;			/* Number of linearisation coefficients */
	double *polys;		/* the coeficients */
	double scale;		/* Absolute scale value */
	double fval, lval;

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

#ifdef ENABLE_NONLINCOR	
	/* Linearise */
	for (lval = polys[npoly-1], k = npoly-2; k >= 0; k--) {
		lval = lval * raw + polys[k];
	}
#else
	lval = raw;
#endif

	/* And scale to be an absolute sensor reading */
	lval = lval * scale;

	return lval;
}

/* Invert the polinomial linearization. */
/* Since the linearisation is nearly a straight line, */
/* a simple Newton inversion will suffice. */
static double inv_poly(double *polys, int npoly, double inv) {
	double outv = inv, lval, del = 100.0;
	int i, k;

	for (i = 0; i < 100 && fabs(del) > 1e-6; i++) {
		for (lval = polys[npoly-1], k = npoly-2; k >= 0; k--) {
			lval = lval * outv + polys[k];
		}
		del = (inv - lval);
		outv += 0.99 * del;
	}

	return outv;
}

/* Take a single set of absolute linearised sensor values and */
/* convert them back into raw reading values. */
i1pro_code i1pro_abssens_to_meas(
	i1pro *p,
	int meas[128],			/* Return raw measurement data */
	double abssens[128],	/* Array of [nraw] value to process */
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
		DBG((dbgo,"i1pro_abssens_to_meas subtmode set\n"))
		if (p->verb) printf("i1pro_abssens_to_meas subtmode set\n");
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
		lval = abssens[j] / scale;

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
	double *avg,			/* return average [nraw] */
	double **multimeas,		/* Array of [nummeas][nraw] value to average */
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
	
	DBG((dbgo,"i1pro_average_multimeas %d readings\n",nummeas))

	for (j = 0; j < 128; j++) 
		avg[j] = 0.0;

	/* Now process the buffer values */
	for (i = 0; i < nummeas; i++) {
		double measavg = 0.0;

		for (j = 1; j < 127; j++) {
			double val;

			val = multimeas[i][j];

			if (val > highest)
				highest = val;
			if (val > satthresh)
				avgoverth++;
			measavg += val;
			avg[j] += val;
		}
		measavg /= 126.0;
		oallavg += measavg;
		if (measavg < minavg)
			minavg = measavg;
		if (measavg > maxavg)
			maxavg = measavg;

		/* and the duplicated values at the end */
		avg[0]   += multimeas[i][0];
		avg[127] += multimeas[i][127];
	}

	for (j = 0; j < 128; j++) 
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
	DBG((dbgo,"norm = %f, dark thresh = %f\n",norm,darkthresh))
	if (norm < (2.0 * darkthresh))
		norm = 2.0 * darkthresh;

	DBG((dbgo,"overall avg = %f, minavg = %f, maxavg = %f, variance %f\n",
	                   oallavg,minavg,maxavg,(maxavg - minavg)/norm))
	if ((maxavg - minavg)/norm > PATCH_CONS_THR) {
		rv |= 1;
	}
	return rv;
}


#ifdef PATREC_DEBUG
#define PRDBG(xxx) fprintf xxx ;
#ifndef dbgo
#define dbgo stdout
#endif
#else
#define PRDBG(xxx) 
#endif	/* PATREC_DEBUG */

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
/* Return zero if readings are consistent and not saturated. */
/* Return nz with bit 1 set if the readings are not consistent */
/* Return nz with bit 2 set if the readings are saturated */
/* Return the highest individual element. */
i1pro_code i1pro_extract_patches_multimeas(
	i1pro *p,
	int *flags,				/* return flags */
	double **pavg,			/* return patch average [naptch][nraw] */
	int tnpatch,			/* Target number of patches to recognise */
	double **multimeas,		/* Array of [nummeas][nraw] value to extract from */
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
	int rv = 0;
	double patch_cons_thr = PATCH_CONS_THR * m->scan_toll_ratio;
#ifdef PATREC_DEBUG
	double **plot;
#endif

	DBG((dbgo,"i1pro_extract_patches_multimeas with target number of patches = %d\n",tnpatch))
	if (p->debug >= 1) fprintf(stderr,"Patch recognition looking for %d patches out of %d samples\n",tnpatch,nummeas);

	maxval = dvectorz(0, m->nraw-1);  

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
	/* Plot out 6 lots of 6 values each */ 
	plot = dmatrixz(0, 6, 0, nummeas-1);  
//	for (j = 1; j < (m->nraw-6); j += 6) 			/* Plot all the bands */
//	for (j = 45; j < (m->nraw-6); j += 100) 		/* Do just one band */
	for (j = 5; j < (m->nraw-6); j += 30) {		/* Do four bands */
		for (k = 0; k < 6; k ++) {
			for (i = 0; i < nummeas; i++) { 
				plot[k][i] = multimeas[i][j+k]/maxval[j+k];
			}
		}
		for (i = 0; i < nummeas; i++)
			plot[6][i] = (double)i;
		printf("Bands %d - %d\n",j,j+5);
		do_plot6(plot[6], plot[0], plot[1], plot[2], plot[3], plot[4], plot[5], nummeas);
	}
#endif

	sslope = dmatrixz(0, nummeas-1, 0, m->nraw-1);  
	slope = dvectorz(0, nummeas-1);  
	fslope = dvectorz(0, nummeas-1);  
	sizepop = ivectorz(0, nummeas-1);

#ifndef NEVER		/* Good with this on */
	/* Average bands together */
	for (i = 0; i < nummeas; i++) {
		for (j = BL + BW; j < (BH - BW); j++) {
			for (k = -BL; k <= BW; k++)		/* Box averaging filter over bands */
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
		for (j = BL; j < BH; j++) {
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

			for (j = BL; j < BH; j++) {				/* Bands */

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
		for (jj = 0, j = BL; jj < 6 && j < BH; jj++, j += ((BH-BL)/6)) {
			double sum = 0.0;
			for (k = -BL; k <= BW; k++)		/* Box averaging filter over bands */
				sum += multimeas[i][j + k];
			plot[jj][i] = sum/((2.0 * BL + 1.0) * maxval[j+k]);
		}
	}
	for (i = 0; i < nummeas; i++)
		plot[6][i] = (double)i;
	do_plot6(plot[6], slope, plot[0], plot[1], plot[2], plot[3], plot[4], nummeas);
#endif

	free_dvector(fslope, 0, nummeas-1);  
	free_dmatrix(sslope, 0, nummeas-1, 0, m->nraw-1);  

	/* Now threshold the measurements into possible patches */
	apat = 2 * nummeas;
	if ((pat = (i1pro_patch *)malloc(sizeof(i1pro_patch) * apat)) == NULL)
		error("i1pro: malloc of patch structures failed!");

	avglegth = 0.0;
	for (npat = i = 0; i < (nummeas-1); i++) {
		if (slope[i] > thresh)
			continue;

		/* Start of a new patch */
		if (npat >= apat) {
			apat *= 2;
			if ((pat = (i1pro_patch *)realloc(pat, sizeof(i1pro_patch) * apat)) == NULL)
				error("i1pro: reallloc of patch structures failed!");
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
	PRDBG((dbgo,"Number of patches = %d\n",npat))

	/* We don't count the first and last patches, as we assume they are white leader */
	if (npat < (tnpatch + 2)) {
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, 0, m->nraw-1);  
		free(pat);
		if (p->debug >= 1)
			fprintf(stderr,"Patch recog failed - unable to detect enough possible patches\n");
		return I1PRO_RD_NOTENOUGHPATCHES;
	} else if (npat >= (2 * tnpatch) + 2) {
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, 0, m->nraw-1);  
		free(pat);
		if (p->debug >= 1)
			fprintf(stderr,"Patch recog failed - detecting too many possible patches\n");
		return I1PRO_RD_TOOMANYPATCHES;
	}
	avglegth /= (double)npat;

#ifdef PATREC_DEBUG
	for (i = 0; i < npat; i++) {
		printf("Raw patch %d, start %d, length %d\n",i, pat[i].ss, pat[i].no);
	}
#endif

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

	PRDBG((dbgo,"Median patch width %f\n",median))

	/* Now decide which patches to use. */
	/* Try a widening window around the median. */
	for (window = 0.2, try = 0; try < 15; window *= 1.4, try++) {
		int bgcount = 0, bgstart = 0;
		int gcount, gstart;
		double wmin = median/(1.0 + window);
		double wmax = median * (1.0 + window);

		PRDBG((dbgo,"Window = %f - %f\n",wmin, wmax))
		/* Track which is the largest contiguous group that */
		/* is within our window */
		gcount = gstart = 0;
		for (i = 1; i < npat; i++) {
			if (i < (npat-1) && pat[i].no <= wmax) {		/* Small enough */
				if (pat[i].no >= wmin) {	/* And big enough */
					if (gcount == 0) {		/* Start of new group */
						gcount++;
						gstart = i;
						PRDBG((dbgo,"Start group at %d\n",gstart))
					} else {
						gcount++;			/* Continuing new group */
						PRDBG((dbgo,"Continue group at %d, count %d\n",gstart,gcount))
					}
				}
			} else {	/* Too big or end of patches, end this group */
				PRDBG((dbgo,"Terminating group group at %d, count %d\n",gstart,gcount))
				if (gcount > bgcount) {		/* New biggest group */
					bgcount = gcount;
					bgstart = gstart;
					PRDBG((dbgo,"New biggest\n"))
				}
				gcount = gstart = 0;		/* End this group */
			}
		}
		PRDBG((dbgo,"Biggest group is at %d, count %d\n",bgstart,bgcount))

		if (bgcount == tnpatch) {			/* We're done */
			for (i = bgstart, j = 0; i < npat && j < tnpatch; i++) {
				if (pat[i].no <= wmax && pat[i].no >= wmin) {
					pat[i].use = 1;
					j++;
					if (pat[i].no < MIN_SAMPLES) {
						PRDBG((dbgo,"Too few samples\n"))
						free_ivector(sizepop, 0, nummeas-1);
						free_dvector(slope, 0, nummeas-1);  
						free_dvector(maxval, 0, m->nraw-1);  
						free(pat);
						if (p->debug >= 1)
							fprintf(stderr,"Patch recog failed - patches sampled too sparsely\n");
						return I1PRO_RD_NOTENOUGHSAMPLES;
					}
				}
			}
			break;

		} else if (bgcount > tnpatch) {
			PRDBG((dbgo,"Too many patches\n"))
			free_ivector(sizepop, 0, nummeas-1);
			free_dvector(slope, 0, nummeas-1);  
			free_dvector(maxval, 0, m->nraw-1);  
			free(pat);
			if (p->debug >= 1)
				fprintf(stderr,"Patch recog failed - detected too many consistent patches\n");
			return I1PRO_RD_TOOMANYPATCHES;
		}
	}
	if (try >= 15) {
		PRDBG((dbgo,"Not enough patches\n"))
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, 0, m->nraw-1);  
		free(pat);
		if (p->debug >= 1)
			fprintf(stderr,"Patch recog failed - unable to find enough consistent patches\n");
		return I1PRO_RD_NOTENOUGHPATCHES;
	}

#ifdef PATREC_DEBUG
	printf("Got %d patches out of potentional %d:\n",tnpatch, npat);
	printf("Average patch legth %f\n",avglegth);
	for (i = 1; i < (npat-1); i++) {
		if (pat[i].use == 0)
			continue;
		printf("Patch %d, start %d, length %d:\n",i, pat[i].ss, pat[i].no, pat[i].use);
	}
#endif

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
	printf("After trimming got:\n");
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
		for (jj = 0, j = BL; jj < 6 && j < BH; jj++, j += ((BH-BL)/6)) {
			double sum = 0.0;
			for (k = -BL; k <= BW; k++)		/* Box averaging filter over bands */
				sum += multimeas[i][j + k];
			plot[jj][i] = sum/((2.0 * BL + 1.0) * maxval[j+k]);
		}
	}
	for (i = 0; i < nummeas; i++)
		plot[6][i] = (double)i;
	do_plot6(plot[6], slope, plot[0], plot[1], plot[2], plot[3], plot[4], nummeas);
#endif

#ifdef PATREC_DEBUG
	free_dmatrix(plot, 0, 6, 0, nummeas-1);  
#endif

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
			PRDBG((dbgo,"Too few samples (%d, need %d)\n",pat[k].no,MIN_SAMPLES))
			free_dvector(slope, 0, nummeas-1);  
			free_ivector(sizepop, 0, nummeas-1);
			free_dvector(maxval, 0, m->nraw-1);  
			free(pat);
			if (p->debug >= 1)
				fprintf(stderr,"Patch recog failed - patches sampled too sparsely\n");
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
		PRDBG((dbgo,"Patch %d: consistency = %f%%, thresh = %f%%\n",pix,100.0 * cons, 100.0 * patch_cons_thr))
		if (cons > patch_cons_thr) {
			if (p->debug >= 1)
				fprintf(stderr,"Patch recog failed - patch %k is inconsistent (%f%%)\n",cons);
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
	free_dvector(maxval, 0, m->nraw-1);  
	free(pat);

	if ((rv & 2) && p->debug >= 1)
		fprintf(stderr,"Patch recog failed - some patches are saturated\n");

	DBG((dbgo,"i1pro_extract_patches_multimeas done, sat = %s, inconsist = %s\n",
	                  rv & 2 ? "true" : "false", rv & 1 ? "true" : "false"))

	if (p->debug >= 1) fprintf(stderr,"Patch recognition returning OK\n");

	return I1PRO_OK;
}
#undef BL
#undef BH
#undef BW


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
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k, pix;
	double minval, maxval;		/* min and max input value at wavelength of maximum input */
	double mean;				/* Mean of the max wavelength band */
	int maxband;				/* Band of maximum value */
	double thresh;				/* Level threshold */
	int fsampl;					/* Index of the first sample over the threshold */
	int nsampl;					/* Number of samples over the threshold */
	double *aavg;				/* ambient average [nraw] */
	double finttime;			/* Flash integration time */
	int rv = 0;
#ifdef PATREC_DEBUG
	double **plot;
#endif

	DBG((dbgo,"i1pro_extract_patches_flash\n"))
	if (p->debug >= 1) fprintf(stderr,"Patch recognition looking flashes in %d measurements\n",nummeas);

	/* Discover the maximum input value for flash dection */
	maxval = -1e6;
	for (j = 0; j < m->nraw; j ++) {
		for (i = 0; i < nummeas; i++) {
			if (multimeas[i][j] > maxval) {
				maxval = multimeas[i][j];
				maxband = j;
			}
		}
	}

	if (maxval <= 0.0) {
		if (p->debug >= 1) fprintf(stderr,"No flashes found in measurement\n");
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
	PRDBG((dbgo,"i1pro_extract_patches_flash band %d minval %f maxval %f, mean = %f, thresh = %f\n",maxband,minval,maxval,mean, thresh))

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
#endif

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
	PRDBG((dbgo,"Number of flash patches = %d\n",nsampl))
	if (nsampl == 0)
		return I1PRO_RD_NOFLASHES;

	/* See if there are as many samples before the first flash */
	if (nsampl < 6)
		nsampl = 6;

	/* Average nsample samples of ambient */
	i = (fsampl-3-nsampl);
	if (i < 0)
		return I1PRO_RD_NOAMBB4FLASHES;
	PRDBG((dbgo,"Ambient samples %d to %d \n",i,fsampl-3))
	aavg = dvectorz(0, m->nraw-1);  
	for (nsampl = 0; i < (fsampl-3); i++) {
		for (j = 0; j < m->nraw-1; j++)
			aavg[j] += multimeas[i][j];
		nsampl++;
	}

	/* Average all the values over the threshold, */
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
			PRDBG((dbgo,"Integrating flash sample no %d \n",i))
			for (j = 0; j < m->nraw-1; j++)
				pavg[j] += multimeas[i][j];
			k++;
		}
	}
	for (j = 0; j < m->nraw-1; j++)
		pavg[j] = pavg[j]/(double)k - aavg[j]/(double)nsampl;

	PRDBG((dbgo,"Number of flash patches integrated = %d\n",k))

	finttime = inttime * (double)k;
	if (duration != NULL)
		*duration = finttime;

	/* Convert to cd/m^2 seconds */
	for (j = 0; j < m->nraw-1; j++)
		pavg[j] *= finttime;

	if (flags != NULL)
		*flags = rv;

	free_dvector(aavg, 0, m->nraw-1);  

	return I1PRO_OK;
}

#ifdef PATREC_DEBUG
#undef PRDBG
#endif


/* Subtract one abssens array from another */
void i1pro_sub_abssens(
	i1pro *p,
	int nummeas,			/* Return number of readings measured */
	double **abssens,		/* Source/Desination array [nraw] */
	double *sub				/* Value to subtract [nraw] */
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j;
	
	/* For each measurement */
	for (i = 0; i < nummeas; i++) {

		/* For each wavelength */
		for (j = 0; j < m->nraw; j++) {
			abssens[i][j] -= sub[j];
		}
	}
}

/* Convert an abssens array from raw wavelengths to output wavelenths */
/* for the current resolution */
void i1pro_abssens_to_abswav(
	i1pro *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav] */
	double **abssens		/* Source array [nraw] */
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k, cx, sx;
	
	/* For each measurement */
	for (i = 0; i < nummeas; i++) {

		/* For each output wavelength */
		for (cx = j = 0; j < m->nwav; j++) {
			double oval = 0.0;
	
			/* For each matrix value */
			sx = m->mtx_index[j];		/* Starting index */
			for (k = 0; k < m->mtx_nocoef[j]; k++, cx++, sx++) {
				oval += m->mtx_coef[cx] * abssens[i][sx];
			}
			abswav[i][j] = oval;
		}
	}
}

/* Convert an abssens array from raw wavelengths to output wavelenths */
/* for the standard resolution */
void i1pro_abssens_to_abswav1(
	i1pro *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav1] */
	double **abssens		/* Source array [nraw] */
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k, cx, sx;
	
	/* For each measurement */
	for (i = 0; i < nummeas; i++) {

		/* For each output wavelength */
		for (cx = j = 0; j < m->nwav1; j++) {
			double oval = 0.0;
	
			/* For each matrix value */
			sx = m->mtx_index1[j];		/* Starting index */
			for (k = 0; k < m->mtx_nocoef1[j]; k++, cx++, sx++) {
				oval += m->mtx_coef1[cx] * abssens[i][sx];
			}
			abswav[i][j] = oval;
		}
	}
}

/* Convert an abssens array from raw wavelengths to output wavelenths */
/* for the high resolution */
void i1pro_abssens_to_abswav2(
	i1pro *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav2] */
	double **abssens		/* Source array [nraw] */
) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k, cx, sx;
	
	/* For each measurement */
	for (i = 0; i < nummeas; i++) {

		/* For each output wavelength */
		for (cx = j = 0; j < m->nwav2; j++) {
			double oval = 0.0;
	
			/* For each matrix value */
			sx = m->mtx_index2[j];		/* Starting index */
			for (k = 0; k < m->mtx_nocoef2[j]; k++, cx++, sx++) {
				oval += m->mtx_coef2[cx] * abssens[i][sx];
			}
			abswav[i][j] = oval;
		}
	}
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
		for (j = 0; j < m->nwav; j++) {
			outspecrd[i][j] = inspecrd[i][j] * s->cal_factor[j];
		}
	}
}


/* =============================================== */
#ifdef HIGH_RES

/* High res congiguration */
#undef EXISTING_SHAPE		/* Else generate filter shape */
#undef USE_DECONV			/* Use deconvolution curve */
#undef USE_GAUSSIAN			/* Use gaussian filter shape, else lanczos2 */
#undef COMPUTE_DISPERSION	/* Compute slit & optics dispersion from red laser data */

#ifdef NEVER
/* Plot the matrix coefficients */
void i1pro_debug_plot_mtx_coef(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k, cx, sx;
	double *xx, *ss;
	double **yy;

	xx = dvectorz(0, m->nraw-1);		/* X index */
	yy = dmatrixz(0, 5, 0, m->nraw-1);	/* Curves distributed amongst 5 graphs */
	
	for (i = 0; i < m->nraw; i++)
		xx[i] = i;

	/* For each output wavelength */
	for (cx = j = 0; j < m->nwav; j++) {
		i = j % 5;

//printf("Out wave = %d\n",j);
		/* For each matrix value */
		sx = m->mtx_index[j];		/* Starting index */
//printf("start index = %d, nocoef %d\n",sx,m->mtx_nocoef[j]);
		for (k = 0; k < m->mtx_nocoef[j]; k++, cx++, sx++) {
//printf("offset %d, coef ix %d val %f from ccd %d\n",k, cx, m->mtx_coef[cx], sx);
			yy[5][sx] += 0.5 * m->mtx_coef[cx];
			yy[i][sx] = m->mtx_coef[cx];
		}
	}

	do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
	free_dvector(xx, 0, m->nraw-1);
	free_dmatrix(yy, 0, 2, 0, m->nraw-1);
}
#endif

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
//printf("~1 params %f %f %f, rv = %f\n", tp[0],tp[1],tp[2],rv);
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
	double y;

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
#else
#ifdef USE_GAUSSIAN
	/* gausian */
	wi = wi/(2.0 * sqrt(2.0 * log(2.0)));	/* Convert width at half max to std. dev. */
    x = x/(sqrt(2.0) * wi);
    y = 1.0/(wi * sqrt(2.0 * DBL_PI)) * exp(-(x * x));
#else

	/* lanczos2 */
	x = fabs(1.0 * x/wi);
	if (x >= 2.0)
		return 0.0; 
	if (x < 1e-5)
		return 1.0;
	y = sin(DBL_PI * x)/(DBL_PI * x) * sin(DBL_PI * x/2.0)/(DBL_PI * x/2.0);
#endif
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

/* Create high resolution mode references */
i1pro_code i1pro_create_hr(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;
	int i, j, k, cx, sx;
	i1pro_fc coeff[100][16];	/* Existing filter cooefficients */
	i1pro_xp xp[101];			/* Crossover points each side of filter */
	i1pro_code ev = I1PRO_OK;
	rspl *raw2wav;				/* Lookup from CCD index to wavelength */
	i1pro_fs fshape[100 * 16];  /* Existing filter shape */
	int ncp = 0;				/* Number of shape points */
#ifdef COMPUTE_DISPERSION
	double spf[3];				/* Spread function parameters */
#endif /* COMPUTE_DISPERSION */
	
	/* Convert the native filter cooeficient representation to */
	/* a 2D array we can randomly index. */
	for (cx = j = 0; j < m->nwav1; j++) { /* For each output wavelength */
		if (j >= 100)	/* Assert */
			error("i1pro: number of output wavelenths is > 100");

		/* For each matrix value */
		sx = m->mtx_index1[j];		/* Starting index */
		for (k = 0; k < m->mtx_nocoef1[j]; k++, cx++, sx++) {
			if (k >= 16)	/* Assert */
				error("i1pro: number of filter coeefs is > 16");

			coeff[j][k].ix = sx;
			coeff[j][k].we = m->mtx_coef1[cx];
		}
	}

#ifdef HIGH_RES_PLOT
	/* Plot original re-sampling curves */
	{
		double *xx, *ss;
		double **yy;

		xx = dvectorz(0, m->nraw-1);		/* X index */
		yy = dmatrixz(0, 5, 0, m->nraw-1);	/* Curves distributed amongst 5 graphs */
		
		for (i = 0; i < m->nraw; i++)
			xx[i] = i;

		/* For each output wavelength */
		for (j = 0; j < m->nwav1; j++) {
			i = j % 5;

			/* For each matrix value */
			for (k = 0; k < m->mtx_nocoef1[j]; k++) {
				yy[5][coeff[j][k].ix] += 0.5 * coeff[j][k].we;
				yy[i][coeff[j][k].ix] = coeff[j][k].we;
			}
		}

		printf("Original wavelength sampling curves:\n");
		do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
		free_dvector(xx, 0, m->nraw-1);
		free_dmatrix(yy, 0, 2, 0, m->nraw-1);
	}
#endif /* HIGH_RES_PLOT */

	/* Compute the crossover points between each filter */
	for (i = 0; i < (m->nwav1-1); i++) {
		double den, y1, y2, y3, y4, yn, xn;	/* Location of intersection */

		/* between filter i and i+1, we want to find the two */
		/* raw indexes where the weighting values cross over */
		/* Do a brute force search to avoid making assumptions */
		/* about the raw order */
		for (j = 0; j < (m->mtx_nocoef1[i]-1); j++) {
			for (k = 0; k < (m->mtx_nocoef1[i+1]-1); k++) {
//printf("~1 checking %d, %d: %d = %d, %d = %d\n",j,k, coeff[i][j].ix, coeff[i+1][k].ix, coeff[i][j+1].ix, coeff[i+1][k+1].ix);
				if (coeff[i][j].ix == coeff[i+1][k].ix
				 && coeff[i][j+1].ix == coeff[i+1][k+1].ix
				 && coeff[i][j].we > 0.0 && coeff[i][j+1].we > 0.0
				 && coeff[i][k].we > 0.0 && coeff[i][k+1].we > 0.0
				 && ((   coeff[i][j].we >= coeff[i+1][k].we
				 	 && coeff[i][j+1].we <= coeff[i+1][k+1].we)
				  || (   coeff[i][j].we <= coeff[i+1][k].we
				 	 && coeff[i][j+1].we >= coeff[i+1][k+1].we))) {
//printf("~1 got it at %d, %d: %d = %d, %d = %d\n",j,k, coeff[i][j].ix, coeff[i+1][k].ix, coeff[i][j+1].ix, coeff[i+1][k+1].ix);
//printf("~1 got it\n");
					goto gotit;
				}
			}
		}
	gotit:;
		if (j >= m->mtx_nocoef1[i])	/* Assert */
			error("i1pro: failed to locate crossover between resampling filters");
//printf("~1 %d: overlap at %d, %d: %f : %f, %f : %f\n",i, j,k, coeff[i][j].we, coeff[i+1][k].we, coeff[i][j+1].we, coeff[i+1][k+1].we);

		/* Compute the intersection of the two line segments */
		y1 = coeff[i][j].we;
		y2 = coeff[i][j+1].we;
		y3 = coeff[i+1][k].we;
		y4 = coeff[i+1][k+1].we;
		den = -y4 + y3 + y2 - y1;
		yn = (y2 * y3 - y1 * y4)/den;
		xn = (y3 - y1)/den;
//printf("~1 den = %f, yn = %f, xn = %f\n",den,yn,xn);
		xp[i+1].wav = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, i + 0.5);
		xp[i+1].raw = (1.0 - xn) * coeff[i][j].ix + xn * coeff[i][j+1].ix;
		xp[i+1].wei = yn;
//printf("Intersection %d: wav %f, raw %f, wei %f\n",i+1,xp[i+1].wav,xp[i+1].raw,xp[i+1].wei);
//printf("\n");
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
		for (j = 0; j < (m->mtx_nocoef1[0]-1); j++) {
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
		if (j >= m->mtx_nocoef1[0])	/* Assert */
			error("i1pro: failed to end crossover");
		den = -y4 + y3 + y2 - y1;
		yn = (y2 * y3 - y1 * y4)/den;
		xn = (y3 - y1)/den;
//printf("~1 den = %f, yn = %f, xn = %f\n",den,yn,xn);
		xp[0].wav = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, -0.5);
		xp[0].raw = (1.0 - xn) * coeff[0][j].ix + xn * coeff[0][j+1].ix;
		xp[0].wei = yn;
//printf("End 0 intersection %d: wav %f, raw %f, wei %f\n",0,xp[0].wav,xp[0].raw,xp[0].wei);
//printf("\n");

		x5 = xp[m->nwav1-2].raw;
		y5 = xp[m->nwav1-2].wei;
		x6 = xp[m->nwav1-1].raw;
		y6 = xp[m->nwav1-1].wei;

//printf("~1 x5 %f, y5 %f, x6 %f, y6 %f\n",x5,y5,x6,y6);
		/* Search for possible intersection point with first curve */
		/* Create equation for line from next two intersection points */
		for (j = 0; j < (m->mtx_nocoef1[0]-1); j++) {
			/* Extrapolate line to this segment */
			y3 = y5 + (coeff[m->nwav1-1][j].ix - x5)/(x6 - x5) * (y6 - y5);
			y4 = y5 + (coeff[m->nwav1-1][j+1].ix - x5)/(x6 - x5) * (y6 - y5);
			/* This segment of curve */
			y1 = coeff[m->nwav1-1][j].we;
			y2 = coeff[m->nwav1-1][j+1].we;
			if ( ((  y1 >= y3 && y2 <= y4)		/* Segments overlap */
			   || (  y1 <= y3 && y2 >= y4))
			  && ((    coeff[m->nwav1-1][j].ix < x5 && coeff[m->nwav1-1][j].ix < x6
			        && coeff[m->nwav1-1][j+1].ix < x5 && coeff[m->nwav1-1][j+1].ix < x6)
			   || (    coeff[m->nwav1-1][j+1].ix > x5 && coeff[m->nwav1-1][j+1].ix > x6
				    && coeff[m->nwav1-1][j].ix > x5 && coeff[m->nwav1-1][j].ix > x6))) {
				break;
			}
		}
		if (j >= m->mtx_nocoef1[m->nwav1-1])	/* Assert */
			error("i1pro: failed to end crossover");
		den = -y4 + y3 + y2 - y1;
		yn = (y2 * y3 - y1 * y4)/den;
		xn = (y3 - y1)/den;
//printf("~1 den = %f, yn = %f, xn = %f\n",den,yn,xn);
		xp[m->nwav1].wav = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, m->nwav1-0.5);
		xp[m->nwav1].raw = (1.0 - xn) * coeff[m->nwav1-1][j].ix + xn * coeff[m->nwav1-1][j+1].ix;
		xp[m->nwav1].wei = yn;
//printf("End 36 intersection %d: wav %f, raw %f, wei %f\n",m->nwav1+1,xp[m->nwav1].wav,xp[m->nwav1].raw,xp[m->nwav1].wei);
//printf("\n");
	}

#ifdef HIGH_RES_DEBUG
	/* Check to see if the area of each filter curve is the same */
	/* (yep, width times 2 * xover height is close to 1.0, and the */
	/*  sum of the weightings is exactly 1.0) */
	for (i = 0; i < m->nwav1; i++) {
		double area1, area2;
		area1 = fabs(xp[i].raw - xp[i+1].raw) * (xp[i].wei + xp[i+1].wei);

		area2 = 0.0;
		for (j = 0; j < (m->mtx_nocoef1[i]); j++)
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

		if ((raw2wav = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
			DBG((dbgo,"i1pro: creating rspl for high res conversion failed"))
			return I1PRO_INT_NEW_RSPL_FAILED;
		}

		vlow[0] = 1e6;
		vhigh[0] = -1e6;
		for (i = 0; i < (m->nwav1+1); i++) {
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
		
		raw2wav->fit_rspl(raw2wav, 0, sd, m->nwav1+1, glow, ghigh, gres, vlow, vhigh, 0.5, avgdev, NULL);
	}

#ifdef COMPUTE_DISPERSION
	/* Fit our red laser CCD data to a slit & optics Gaussian dispersion model */
	{
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
			co pp;

			/* Translate CCD cell boundaries index to wavelength */
			pp.p[0] = ccd;
			raw2wav->interp(raw2wav, &pp);
			lwl[i] = pp.v[0];
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
			warning("hropt_opt1\n");

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

//printf("~1 Normalized intergral = %f\n",gaussint(spf, spf[1] - 30.0, spf[1] + 30.0));
//printf("~1 Half width = %f\n",2.0 * sqrt(2.0 * log(2.0)) * spf[2]);
	}
#endif /* COMPUTE_DISPERSION */

#ifdef EXISTING_SHAPE
	/* Convert each weighting curves values into normalized values and */
	/* accumulate into a single curve. */
	/* This probably isn't quite correct - we really need to remove */
	/* the effects of the convolution with the CCD cell widths. */
	/* perhaps it's closer to a lanczos2 if this were done ? */
	{
		for (i = 0; i < m->nwav1; i++) {
			double cwl;		/* center wavelegth */
			double weight = 0.0;

			for (j = 0; j < (m->mtx_nocoef1[i]); j++) {
				double w1, w2, cellw;
				co pp;

				/* Translate CCD cell boundaries index to wavelength */
				pp.p[0] = (double)coeff[i][j].ix - 0.5; 
				raw2wav->interp(raw2wav, &pp);
				w1 = pp.v[0];

				pp.p[0] = (double)coeff[i][j].ix + 0.5; 
				raw2wav->interp(raw2wav, &pp);
				w2 = pp.v[0];

				cellw = fabs(w2 - w1);

				cwl = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, i);

				/* Translate CCD index to wavelength */
				pp.p[0] = (double)coeff[i][j].ix; 
				raw2wav->interp(raw2wav, &pp);
				fshape[ncp].wl = pp.v[0] - cwl;
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
			printf("Accumulated curve:\n");
			do_plot(x1, y1, NULL, NULL, ncp);

			free_dvector(x1, 0, ncp-1);
			free_dvector(y1, 0, ncp-1);
		}
#endif /* HIGH_RES_PLOT */
	}
#endif /* EXISTING_SHAPE */

#ifdef HIGH_RES_DEBUG
	/* Check that the filter sums to a constant */
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

	{
		double fshmax;				/* filter shape max wavelength from center */
#define MXNOFC 64
		i1pro_fc coeff2[500][MXNOFC];	/* New filter cooefficients */
		double twidth;

		/* Construct a set of filters that uses more CCD values */
		twidth = HIGHRES_WIDTH;

		if (m->nwav2 > 500)		/* Assert */
			error("High res filter has too many bands");

#ifdef EXISTING_SHAPE		/* Else generate filter shape */
		/* Cut the filter width by half, to conver from 10nm to 5nm spacing */
		for (i = 0; i < ncp; i++)
			fshape[i].wl *= twidth/10.0;
		fshmax = -fshape[0].wl;		/* aximum extent needed around zero */
		if (fshape[ncp-1].wl > fshmax)
			fshmax = fshape[ncp-1].wl;
#else
		/* Use a crude means of determining width */
		for (fshmax = 50.0; fshmax >= 0.0; fshmax -= 0.1) {
			if (fabs(lanczos2(twidth, fshmax)) > 0.001) {
				fshmax += 0.1;
				break;
			}
		}
		if (fshmax <= 0.0)
			error("i1pro: fshmax search failed");
#endif
//printf("~1 fshmax = %f\n",fshmax);

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
		if ((m->mtx_nocoef2 = (int *)calloc(m->nwav2, sizeof(int))) == NULL)
			error("i1pro: malloc mtx_nocoef2 failed!");

		/* For all the useful CCD bands */
		for (i = 1; i < 127; i++) {
			co pp;
			double w1, wl, w2;

			/* Translate CCD center to to wavelength */
			pp.p[0] = (double)i;
			raw2wav->interp(raw2wav, &pp);
			wl = pp.v[0];

			/* Translate CCD cell boundaries index to wavelength */
			pp.p[0] = i - 0.5;
			raw2wav->interp(raw2wav, &pp);
			w1 = pp.v[0];

			pp.p[0] = i + 0.5;
			raw2wav->interp(raw2wav, &pp);
			w2 = pp.v[0];

//printf("~1 CCD %d, wl %f\n",i,wl);

			/* For each filter */
			for (j = 0; j < m->nwav2; j++) {
				double cwl, rwl;		/* center, relative wavelegth */
				double we;

				cwl = m->wl_short2 + (double)j * (m->wl_long2 - m->wl_short2)/(m->nwav2-1.0);
				rwl = wl - cwl;			/* relative wavelength to filter */

				if (fabs(w1 - cwl) > fshmax && fabs(w2 - cwl) > fshmax)
					continue;		/* Doesn't fall into this filter */

#ifndef NEVER
				/* Integrate in 0.05 nm increments from filter shape */
				{
					int nn;
					double lw, ll;

					nn = (int)(fabs(w2 - w1)/0.05 + 0.5);
			
					lw = w1;
#ifdef EXISTING_SHAPE
					ll = lin_fshape(fshape, ncp, w1- cwl);
#else
					ll = lanczos2(twidth, w1- cwl);
#endif
					we = 0.0;
					for (k = 0; k < nn; k++) { 
						double cw, cl;

#if defined(__APPLE__) && defined(__POWERPC__)
						gcc_bug_fix(k);
#endif
						cw = w1 + (k+1.0)/(nn +1.0) * (w2 - w1);
#ifdef EXISTING_SHAPE
						cl = lin_fshape(fshape, ncp, cw - cwl);
#else
						cl = lanczos2(twidth, cw- cwl);
#endif
						we += 0.5 * (cl + ll) * (lw - cw);
						ll = cl;
						lw = cw;
					}
				}


#else			/* Point sample with weighting */

#ifdef EXISTING_SHAPE
				we = fabs(w2 - w1) * lin_fshape(fshape, ncp, rwl);
#else
				we = fabs(w2 - w1) * lanczos2(twidth, rwl);
#endif

#endif			/* Integrate/Point sample */

				if (m->mtx_nocoef2[j] >= MXNOFC)
					error("i1pro: run out of high res filter space");

				coeff2[j][m->mtx_nocoef2[j]].ix = i;
				coeff2[j][m->mtx_nocoef2[j]++].we = we; 
//printf("~1 filter %d, cwl %f, rwl %f, ix %d, we %f, nocoefs %d\n",j,cwl,rwl,i,we,m->mtx_nocoef2[j]-1);
			}
		}

#ifdef HIGH_RES_PLOT
		/* Plot resampled curves */
		{
			double *xx, *ss;
			double **yy;

			xx = dvectorz(0, m->nraw-1);		/* X index */
			yy = dmatrixz(0, 5, 0, m->nraw-1);	/* Curves distributed amongst 5 graphs */
			
			for (i = 0; i < m->nraw; i++)
				xx[i] = i;

			/* For each output wavelength */
			for (j = 0; j < m->nwav2; j++) {
				i = j % 5;

				/* For each matrix value */
				for (k = 0; k < m->mtx_nocoef2[j]; k++) {
					yy[5][coeff2[j][k].ix] += 0.5 * coeff2[j][k].we;
					yy[i][coeff2[j][k].ix] = coeff2[j][k].we;
				}
			}

			printf("Hi-Res wavelength sampling curves:\n");
			do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
			free_dvector(xx, 0, m->nraw-1);
			free_dmatrix(yy, 0, 2, 0, m->nraw-1);
		}
#endif /* HIGH_RES_PLOT */

		/* Normalise the filters area in CCD space, while maintaining the */
		/* total contribution of each CCD at the target too. */
		{
			int ii;
			double tot = 0.0;
			double ccdweight[128], avgw;	/* Weighting determined by cell widths */
			double ccdsum[128];

			/* Normalize the overall filter weightings */
			for (j = 0; j < m->nwav2; j++)
				for (k = 0; k < m->mtx_nocoef2[j]; k++)
					tot += coeff2[j][k].we;
			tot /= (double)m->nwav2;
			for (j = 0; j < m->nwav2; j++)
				for (k = 0; k < m->mtx_nocoef2[j]; k++)
					coeff2[j][k].we /= tot;

			/* Determine the relative weights for each CCD */
			avgw = 0.0;
			for (i = 1; i < 127; i++) {
				co pp;

				pp.p[0] = i - 0.5;
				raw2wav->interp(raw2wav, &pp);
				ccdweight[i] = pp.v[0];

				pp.p[0] = i + 0.5;
				raw2wav->interp(raw2wav, &pp);
				ccdweight[i] = fabs(ccdweight[i] - pp.v[0]);
				avgw += ccdweight[i];
			}
			avgw /= 126.0;
			// ~~this isn't right because not all CCD's get used!!

#ifdef NEVER
			/* Itterate */
			for (ii = 0; ; ii++) {

				/* Normalize the individual filter weightings */
				for (j = 0; j < m->nwav2; j++) {
					double err;
					tot = 0.0;
					for (k = 0; k < m->mtx_nocoef2[j]; k++)
						tot += coeff2[j][k].we;
					err = 1.0 - tot;
	
					for (k = 0; k < m->mtx_nocoef2[j]; k++)
						coeff2[j][k].we += err/m->mtx_nocoef2[j];
//					for (k = 0; k < m->mtx_nocoef2[j]; k++)
//						coeff2[j][k].we *= 1.0/tot;
				}

				/* Check CCD sums */
				for (i = 1; i < 127; i++) 
					ccdsum[i] = 0.0;

				for (j = 0; j < m->nwav2; j++) {
					for (k = 0; k < m->mtx_nocoef2[j]; k++)
						ccdsum[coeff2[j][k].ix] += coeff2[j][k].we;
				}

				if (ii >= 6)
					break;

				/* Readjust CCD sum */
				for (i = 1; i < 127; i++) { 
					ccdsum[i] = ccdtsum[i]/ccdsum[i];		/* Amount to adjust */
				}

				for (j = 0; j < m->nwav2; j++) {
					for (k = 0; k < m->mtx_nocoef2[j]; k++)
						coeff2[j][k].we *= ccdsum[coeff2[j][k].ix];
				}
			}
#endif	/* NEVER */
		}

#ifdef HIGH_RES_PLOT
		/* Plot resampled curves */
		{
			double *xx, *ss;
			double **yy;

			xx = dvectorz(0, m->nraw-1);		/* X index */
			yy = dmatrixz(0, 5, 0, m->nraw-1);	/* Curves distributed amongst 5 graphs */
			
			for (i = 0; i < m->nraw; i++)
				xx[i] = i;

			/* For each output wavelength */
			for (j = 0; j < m->nwav2; j++) {
				i = j % 5;

				/* For each matrix value */
				for (k = 0; k < m->mtx_nocoef2[j]; k++) {
					yy[5][coeff2[j][k].ix] += 0.5 * coeff2[j][k].we;
					yy[i][coeff2[j][k].ix] = coeff2[j][k].we;
				}
			}

			printf("Normalized Hi-Res wavelength sampling curves:\n");
			do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
			free_dvector(xx, 0, m->nraw-1);
			free_dmatrix(yy, 0, 2, 0, m->nraw-1);
		}
#endif /* HIGH_RES_PLOT */
		/* Convert into runtime format */
		{
			int xcount;

			if ((m->mtx_index2 = (int *)calloc(m->nwav2, sizeof(int))) == NULL)
				error("i1pro: malloc mtx_index2 failed!");
	
			xcount = 0;
			for (j = 0; j < m->nwav2; j++) {
				m->mtx_index2[j] = coeff2[j][0].ix;
				xcount += m->mtx_nocoef2[j];
			}
	
			if ((m->mtx_coef2 = (double *)calloc(xcount, sizeof(double))) == NULL)
				error("i1pro: malloc mtx_coef2 failed!");

			for (i = j = 0; j < m->nwav2; j++)
				for (k = 0; k < m->mtx_nocoef2[j]; k++, i++)
					m->mtx_coef2[i] = coeff2[j][k].we;
		}

		/* Now compute the upsampled references */
		{
			rspl *trspl;			/* Upsample rspl */
			cow sd[100];			/* Scattered data points of existing references */
			datai glow, ghigh;
			datao vlow, vhigh;
			int gres[1];
			double avgdev[1];
			int ii;
			co pp;
	
			if ((trspl = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
				DBG((dbgo,"i1pro: creating rspl for high res conversion failed"))
				raw2wav->del(raw2wav);
				return I1PRO_INT_NEW_RSPL_FAILED;
			}
			
			for (ii = 0; ii < 3; ii++) {
				double **ref2, *ref1;

				if (ii == 0) {
					ref1 = m->white_ref1;
					ref2 = &m->white_ref2;
				} else if (ii == 1) {
					ref1 = m->emis_coef1;
					ref2 = &m->emis_coef2;
				} else {
					if (m->amb_coef1 == NULL)
						break;
					ref1 = m->amb_coef1;
					ref2 = &m->amb_coef2;
				}

				if (ref1 == NULL)
					continue;		/* The instI1Monitor doesn't have a reflective cal */

				vlow[0] = 1e6;
				vhigh[0] = -1e6;
				for (i = 0; i < m->nwav1; i++) {

					sd[i].p[0] = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, i);
					sd[i].v[0] = ref1[i];
					sd[i].w = 1.0;

					if (sd[i].v[0] < vlow[0])
						vlow[0] = sd[i].v[0];
					if (sd[i].v[0] > vhigh[0])
					    vhigh[0] = sd[i].v[0];
				}

				if (ii == 1) {		/* fudge factors */

#ifdef NEVER
					/* Increase boost at short wavelegths */
					if (m->wl_short2 < 380.0) {
						sd[i].p[0] = m->wl_short2;
						sd[i].v[0] = sd[0].v[0] * (1.0 + (380.0 - m->wl_short2)/20.0);
						sd[i++].w = 0.6;
					}
#endif

					/* Reduces boost at long wavelegths */
					if (m->wl_long2 > 730.0) {
						sd[i].p[0] = m->wl_long2;
						sd[i].v[0] = sd[m->nwav1-1].v[0] * (1.0 + (m->wl_long2 - 730.0)/100.0);
						sd[i++].w = 0.3;
					}
				}
				
				glow[0] = m->wl_short2;
				ghigh[0] = m->wl_long2;
				gres[0] = m->nwav2;
				avgdev[0] = 0.0;
				
				trspl->fit_rspl_w(trspl, 0, sd, i, glow, ghigh, gres, vlow, vhigh, 0.5, avgdev, NULL);

				if ((*ref2 = (double *)calloc(m->nwav2, sizeof(double))) == NULL) {
					raw2wav->del(raw2wav);
					trspl->del(trspl);
					error("i1pro: malloc mtx_coef2 failed!");
				}

				for (i = 0; i < m->nwav2; i++) {
					pp.p[0] = m->wl_short2
					        + (double)i * (m->wl_long2 - m->wl_short2)/(m->nwav2-1.0);
					trspl->interp(trspl, &pp);
					(*ref2)[i] = pp.v[0];
				}
#ifdef HIGH_RES_PLOT
				/* Plot original and upsampled reference */
				{
					double *x1 = dvectorz(0, m->nwav2-1);
					double *y1 = dvectorz(0, m->nwav2-1);
					double *y2 = dvectorz(0, m->nwav2-1);
		
					for (i = 0; i < m->nwav2; i++) {
						double wl = m->wl_short2 + (double)i * (m->wl_long2 - m->wl_short2)/(m->nwav2-1.0);
						x1[i] = wl;
						y1[i] = (*ref2)[i];
						if (wl < m->wl_short1 || wl > m->wl_long1) {
							y2[i] = 0.0;
						} else {
							double x, wl1, wl2;
							for (j = 0; j < (m->nwav1-1); j++) {
								wl1 = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, j);
								wl2 = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, j+1);
								if (wl >= wl1 && wl <= wl2)
									break;
							}
							x = (wl - wl1)/(wl2 - wl1);
							y2[i] = ref1[j] + (ref1[j+1] - ref1[j]) * x;
						}
					}
					printf("Original and up-sampled ");
					if (ii == 0) {
						printf("Reflective cal. curve:\n");
					} else if (ii == 1) {
						ref1 = m->emis_coef1;
						ref2 = &m->emis_coef2;
						printf("Emission cal. curve:\n");
					} else {
						printf("Ambient cal. curve:\n");
					}
					do_plot(x1, y1, y2, NULL, m->nwav2);
		
					free_dvector(x1, 0, m->nwav2-1);
					free_dvector(y1, 0, m->nwav2-1);
					free_dvector(y2, 0, m->nwav2-1);
				}
#endif /* HIGH_RES_PLOT */
			}

			trspl->del(trspl);
		}

		/* Basic capability is initialised */
		m->hr_inited = 1;

		/* Allocate space for per mode calibration reference */
		/* and bring high res calibration factors into line */
		/* with current standard res. ones */
		for (i = 0; i < i1p_no_modes; i++) {
			i1pro_state *s = &m->ms[i];

			s->cal_factor2 = dvectorz(0, m->nwav2-1);

			switch(i) {
				case i1p_refl_spot:
				case i1p_refl_scan:
					if (s->cal_valid) {
#ifdef NEVER
	printf("~1 regenerating calibration for reflection\n");
	printf("~1 raw white data:\n");
	plot_raw(s->white_data);
#endif	/* NEVER */
						i1pro_abssens_to_abswav1(p, 1, &s->cal_factor1, &s->white_data);
#ifdef NEVER
	printf("~1 Std res intmd. cal_factor:\n");
	plot_wav1(m, s->cal_factor1);
#endif	/* NEVER */
						i1pro_abssens_to_abswav2(p, 1, &s->cal_factor2, &s->white_data);
#ifdef NEVER
	printf("~1 High intmd. cal_factor:\n");
	plot_wav2(m, s->cal_factor2);
	printf("~1 Std res white ref:\n");
	plot_wav1(m, m->white_ref1);
	printf("~1 High res white ref:\n");
	plot_wav2(m, m->white_ref2);
#endif	/* NEVER */
						i1pro_compute_white_cal(p, s->cal_factor1, m->white_ref1, s->cal_factor1,
						                           s->cal_factor2, m->white_ref2, s->cal_factor2);
#ifdef NEVER
	printf("~1 Std res final cal_factor:\n");
	plot_wav1(m, s->cal_factor1);
	printf("~1 High final cal_factor:\n");
	plot_wav2(m, s->cal_factor2);
#endif	/* NEVER */
					}
					break;

				case i1p_disp_spot:
				case i1p_emiss_spot:
				case i1p_emiss_scan:
					for (j = 0; j < m->nwav2; j++)
						s->cal_factor2[j] = EMIS_SCALE_FACTOR * m->emis_coef2[j];
					break;
	
				case i1p_amb_spot:
				case i1p_amb_flash:
#ifdef FAKE_AMBIENT
					for (j = 0; j < m->nwav2; j++)
						s->cal_factor2[j] = EMIS_SCALE_FACTOR * m->emis_coef2[j];
					s->cal_valid = 1;
#else

					if (m->amb_coef1 != NULL) {
						for (j = 0; j < m->nwav2; j++)
							s->cal_factor2[j] = AMB_SCALE_FACTOR * m->emis_coef2[j] * m->amb_coef2[j];
						s->cal_valid = 1;
					}
#endif
					break;
				case i1p_trans_spot:
				case i1p_trans_scan:
					if (s->cal_valid) {
						i1pro_abssens_to_abswav1(p, 1, &s->cal_factor1, &s->white_data);
						i1pro_abssens_to_abswav2(p, 1, &s->cal_factor2, &s->white_data);
						i1pro_compute_white_cal(p, s->cal_factor1, NULL, s->cal_factor1,
			                                       s->cal_factor2, NULL, s->cal_factor2);
					}
					break;
			}
		}
	}

	raw2wav->del(raw2wav);

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
		if ((ev = i1pro_create_hr(p)) != I1PRO_OK) {
			return ev;
		}
	}

	m->nwav = m->nwav2;
	m->wl_short = m->wl_short2;
	m->wl_long = m->wl_long2; 

	m->mtx_index = m->mtx_index2;
	m->mtx_nocoef = m->mtx_nocoef2;
	m->mtx_coef = m->mtx_coef2;
	m->white_ref = m->white_ref2;
	m->emis_coef = m->emis_coef2;
	m->amb_coef = m->amb_coef2;

	for (i = 0; i < i1p_no_modes; i++) {
		i1pro_state *s = &m->ms[i];
		s->cal_factor = s->cal_factor2;
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
	m->nwav = m->nwav1;
	m->wl_short = m->wl_short1;
	m->wl_long = m->wl_long1; 

	m->mtx_index = m->mtx_index1;
	m->mtx_nocoef = m->mtx_nocoef1;
	m->mtx_coef = m->mtx_coef1;
	m->white_ref = m->white_ref1;
	m->emis_coef = m->emis_coef1;
	m->amb_coef = m->amb_coef1;

	for (i = 0; i < i1p_no_modes; i++) {
		i1pro_state *s = &m->ms[i];
		s->cal_factor = s->cal_factor1;
	}
	m->highres = 0;

#else
	ev = I1PRO_UNSUPPORTED;
#endif /* HIGH_RES */

	return ev;
}

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
	double **specrd		/* Spectral readings */
) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	xsp2cie *conv;	/* Spectral to XYZ conversion object */
	int i, j, k;
	int six = 0;		/* Starting index */
	int nwl = m->nwav;	/* Number of wavelegths */
	double wl_short = m->wl_short;	/* Starting wavelegth */
	double sms;			/* Weighting */

	if (s->emiss)
		conv = new_xsp2cie(icxIT_none, NULL, icxOT_CIE_1931_2, NULL, icSigXYZData);
	else
		conv = new_xsp2cie(icxIT_D50, NULL, icxOT_CIE_1931_2, NULL, icSigXYZData);
	if (conv == NULL)
		return I1PRO_INT_CIECONVFAIL;

	/* Don't report any wavelengths below the minimum for this mode */
	if ((s->min_wl-1e-3) > wl_short) {
		double wl;
		for (j = 0; j < m->nwav; j++) {
			wl = XSPECT_WL(m->wl_short, m->wl_long, m->nwav, j);
			if (wl >= (s->min_wl-1e-3))
				break;
		}
		six = j;
		wl_short = wl;
		nwl -= six;
	}

	if (p->debug >= 1) {
		fprintf(stderr,"i1pro_conv2XYZ got wl_short %f, wl_long %f, nwav %d, min_wl %f\n",
		                m->wl_short, m->wl_long, m->nwav, s->min_wl);
		fprintf(stderr,"      after skip got wl_short %f, nwl = %d\n", wl_short, nwl);
	}

	for (sms = 0.0, i = 1; i < 21; i++)
		sms += opt_adj_weights[i];
	sms *= opt_adj_weights[0];

	for (i = 0; i < nvals; i++) {

		vals[i].XYZ_v = 0;
		vals[i].aXYZ_v = 0;
		vals[i].Lab_v = 0;
		vals[i].sp.spec_n = 0;
		vals[i].duration = 0.0;
	
		vals[i].sp.spec_n = nwl;
		vals[i].sp.spec_wl_short = wl_short;
		vals[i].sp.spec_wl_long = m->wl_long;

		/* Leave values as cd/m^2 */
		if (s->emiss) {
			for (j = six, k = 0; j < m->nwav; j++, k++) {
				vals[i].sp.spec[k] = specrd[i][j] * sms;
			}
			vals[i].sp.norm = 1.0;

		/* Scale values to percentage */
		} else {
			for (j = six, k = 0; j < m->nwav; j++, k++) {
				vals[i].sp.spec[k] = 100.0 * specrd[i][j] * sms;
			}
			vals[i].sp.norm = 100.0;
		}

		/* Set the XYZ */
		if (s->emiss) {
			conv->convert(conv, vals[i].aXYZ, &vals[i].sp);
			vals[i].aXYZ_v = 1;
		} else {
			conv->convert(conv, vals[i].XYZ, &vals[i].sp);
			vals[i].XYZ_v = 1;
			vals[i].XYZ[0] *= 100.0;
			vals[i].XYZ[1] *= 100.0;
			vals[i].XYZ[2] *= 100.0;
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
	double *abswav			/* [mwav1] Measurement to check */
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	double *emiswav, normfac;
	double avg01, avg2227;
	int j;

	emiswav = dvector(0, m->nraw-1);

	/* Convert from absolute wavelength converted sensor reading, */
	/* to calibrated emission wavelength spectrum. */

	/* For each output wavelength */
	for (j = 0; j < m->nwav1; j++) {
		emiswav[j] = m->emis_coef1[j] * abswav[j];
	}
#ifdef PLOT_DEBUG
	printf("White ref read converted to emissive spectrum:\n");
	plot_wav1(m, emiswav);
#endif

	/* Normalise the measurement to the reflectance ofthe 17 wavelength */
	/* of the white reference (550nm), as well as dividing out the */
	/* reference white. This should leave us with the iluminant spectrum, */
	normfac = m->white_ref1[17]/emiswav[17];
	
	for (j = 0; j < m->nwav1; j++) {
		emiswav[j] *= normfac/m->white_ref1[j];
	}

#ifdef PLOT_DEBUG
	printf("normalised to reference white read:\n");
	plot_wav1(m, emiswav);
#endif

	/* Compute two sample averages of the illuminant spectrum. */
	avg01 = 0.5 * (emiswav[0] + emiswav[1]);

	for (avg2227 = 0, j = 22; j < 28; j++) {
		avg2227 += emiswav[j];
	}
	avg2227 /= (double)(28 - 22);

	free_dvector(emiswav, 0, m->nraw-1);

	/* And check them against tolerance for the illuminant. */
	if (m->physfilt == 0x82) {		/* UV filter */
		if (0.0 < avg01 && avg01 < 0.05
		 && 1.2 < avg2227 && avg2227 < 1.76)
			return I1PRO_OK;

	} else {						/* No filter */
		if (0.11 < avg01 && avg01 < 0.22
		 && 1.35 < avg2227 && avg2227 < 1.6)
			return I1PRO_OK;
	}
	return I1PRO_RD_WHITEREFERROR;
}

/* Compute a mode calibration factor given the reading of the white reference. */
/* Return 1 if any of the transmission wavelengths are low. */
int i1pro_compute_white_cal(
	i1pro *p,
	double *cal_factor1,	/* [nwav1] Calibration factor to compute */
	double *white_ref1,		/* [nwav1] White reference to aim for, NULL for 1.0 */
	double *white_read1,	/* [nwav1] The white that was read */
	double *cal_factor2,	/* [nwav2] Calibration factor to compute */
	double *white_ref2,		/* [nwav2] White reference to aim for, NULL for 1.0 */
	double *white_read2		/* [nwav2] The white that was read */
) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	int j, warn = 0;;
	
	if (white_ref1 == NULL) {		/* transmission white reference */
		double avgwh = 0.0;

		/* Compute average white reference reading */
		for (j = 0; j < m->nwav1; j++)
			avgwh += white_read1[j];
		avgwh /= (double)m->nwav1;

		/* For each wavelength */
		for (j = 0; j < m->nwav1; j++) {
			/* If reference is < 0.4% of average */
			if (white_read1[j]/avgwh < 0.004) {
				cal_factor1[j] = 1.0/(0.004 * avgwh);
				warn = 1;
			} else {
				cal_factor1[j] = 1.0/white_read1[j];	
			}
		}

	} else {					/* Reflection white reference */

		/* For each wavelength */
		for (j = 0; j < m->nwav1; j++) {
			if (white_read1[j] < 1000.0)
				cal_factor1[j] = white_ref1[j]/1000.0;	
			else
				cal_factor1[j] = white_ref1[j]/white_read1[j];	
		}
	}

#ifdef HIGH_RES
	if (m->hr_inited == 0)
		return warn;

	if (white_ref2 == NULL) {		/* transmission white reference */
		double avgwh = 0.0;

		/* Compute average white reference reading */
		for (j = 0; j < m->nwav2; j++)
			avgwh += white_read2[j];
		avgwh /= (double)m->nwav2;

		/* For each wavelength */
		for (j = 0; j < m->nwav2; j++) {
			/* If reference is < 0.4% of average */
			if (white_read2[j]/avgwh < 0.004) {
				cal_factor2[j] = 1.0/(0.004 * avgwh);
				warn = 1;
			} else {
				cal_factor2[j] = 1.0/white_read2[j];	
			}
		}

	} else {					/* Reflection white reference */

		/* For each wavelength */
		for (j = 0; j < m->nwav2; j++) {
			if (white_read2[j] < 1000.0)
				cal_factor2[j] = white_ref2[j]/1000.0;	
			else
				cal_factor2[j] = white_ref2[j]/white_read2[j];	
		}
	}
#endif /* HIGH_RES */
	return warn;
}

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
) {
	i1pro_code ev = I1PRO_OK;
	i1proimp *m = (i1proimp *)p->m;
	double new_int_time;
	int    new_gain_mode;

	DBG((dbgo,"i1pro_optimise_sensor called, inttime %f, gain mode %d, scale %f\n",cur_int_time,cur_gain_mode, scale))

	/* Compute new normal gain integration time */
	if (cur_gain_mode)
		new_int_time = cur_int_time * scale * m->highgain;
	else
		new_int_time = cur_int_time * scale;
	new_gain_mode = 0;

	DBG((dbgo,"target inttime %f, gain mode %d\n",new_int_time,new_gain_mode))

	/* Adjust to low light situation */
	if ((new_int_time > m->max_int_time || new_int_time > 2.0) && permithg) {
		new_int_time /= m->highgain;
		new_gain_mode = 1;
	}
	DBG((dbgo,"after low light adjust, inttime %f, gain mode %d\n",new_int_time,new_gain_mode))

	/* Deal with still low light */
	if (new_int_time > m->max_int_time) {
		if (permitclip)
			new_int_time = m->max_int_time;
		else
			return I1PRO_RD_LIGHTTOOLOW;
	}
	DBG((dbgo,"after low light clip, inttime %f, gain mode %d\n",new_int_time,new_gain_mode))

	/* Adjust to high light situation */
	if (new_int_time < m->min_int_time && targoscale < 1.0) {
		new_int_time /= targoscale;			/* Aim for non-scaled sensor optimum */
		if (new_int_time > m->min_int_time)	/* But scale as much as possible */
			new_int_time = m->min_int_time;
	}
	DBG((dbgo,"after high light adjust, inttime %f, gain mode %d\n",new_int_time,new_gain_mode))

	/* Deal with still high light */
	if (new_int_time < m->min_int_time) {
		if (permitclip)
			new_int_time = m->min_int_time;
		else
			return I1PRO_RD_LIGHTTOOHIGH;
	}
	DBG((dbgo,"after high light clip, returning inttime %f, gain mode %d\n",new_int_time,new_gain_mode))

	if (pnew_int_time != NULL)
		*pnew_int_time = new_int_time;

	if (pnew_gain_mode != NULL)
		*pnew_gain_mode = new_gain_mode;

	return I1PRO_OK;
}

/* Compute the number of measurements needed, given the target */
/* time and integration time. Will return 0 if target time is 0 */
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
void
i1pro_prepare_idark(
	i1pro *p
) {
	i1proimp *m = (i1proimp *)p->m;
	i1pro_state *s = &m->ms[m->mmode];
	int i, j;

	/* For normal and high gain */
	for (i = 0; i < 4; i+=2) {
		for (j = 0; j < m->nraw; j++) {
			double d01, d1;
			d01 = s->idark_data[i+0][j] * s->idark_int_time[i+0];
			d1 = s->idark_data[i+1][j] * s->idark_int_time[i+1];
	
			/* Compute increment */
			s->idark_data[i+1][j] = (d1 - d01)/(s->idark_int_time[i+1] - s->idark_int_time[i+0]);
	
			/* Compute base */
			s->idark_data[i+0][j] = d1 - s->idark_data[i+1][j];
		}
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
	if (gainmode)
		i = 2;

	for (j = 0; j < m->nraw; j++) {
		double tt;
		tt = s->idark_data[i+0][j] + inttime * s->idark_data[i+1][j];
		tt /= inttime;			/* Convert to absolute */
		result[j] = tt;
	}
	return I1PRO_OK;
}

/* Set the noautocalib mode */
void i1pro_set_noautocalib(i1pro *p, int v) {
	i1proimp *m = (i1proimp *)p->m;
	m->noautocalib = v;
}

/* Set the trigger config */
void i1pro_set_trig(i1pro *p, inst_opt_mode trig) {
	i1proimp *m = (i1proimp *)p->m;
	m->trig = trig;
}

/* Return the trigger config */
inst_opt_mode i1pro_get_trig(i1pro *p) {
	i1proimp *m = (i1proimp *)p->m;
	return m->trig;
}

/* Set the trigger return */
void i1pro_set_trigret(i1pro *p, int val) {
	i1proimp *m = (i1proimp *)p->m;
	m->trig_return = val;
}

/* Switch thread handler */
int i1pro_switch_thread(void *pp) {
	int nfailed = 0;
	i1pro *p = (i1pro *)pp;
	i1pro_code rv = I1PRO_OK; 
	i1proimp *m = (i1proimp *)p->m;
	DBG((dbgo,"Switch thread started\n"))
	for (nfailed = 0;nfailed < 5;) {
		rv = i1pro_waitfor_switch_th(p, SW_THREAD_TIMEOUT);
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
			DBG((dbgo,"Switch thread failed with 0x%x\n",rv))
			continue;
		}
		m->switch_count++;
	}
	DBG((dbgo,"Switch thread returning\n"))
	return rv;
}

/* ============================================================ */
/* Low level commands */

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
	int rwbytes;			/* Data bytes read or written */
	unsigned char pbuf[1];	/* 1 bytes to write */
	int se, rv = I1PRO_OK;
	int isdeb = 0;
	int stime;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\ni1pro: Instrument reset with mask 0x%02x @ %d msec\n",
	                          mask,(stime = msec_time()) - m->msec);

	pbuf[0] = mask;
	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xCA, 0, 0, pbuf, 1, 2.0);

	rv = icoms2i1pro_err(se);

	if (isdeb) fprintf(stderr,"Reset complete, ICOM err 0x%x (%d msec)\n",se,msec_time()-stime);

	/* Allow time for hardware to stabalize */
	msec_sleep(100);

	p->icom->debug = p->debug;

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
	int se, rv = I1PRO_OK;
	int isdeb = 0;
	int stime;

	if (size >= 0x10000)
		return I1PRO_INT_EETOOBIG;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\ni1pro: Read EEProm address 0x%x size 0x%x @ %d msec\n",
	                           addr, size, (stime = msec_time()) - m->msec);

	int2buf(&pbuf[0], addr);
	short2buf(&pbuf[4], size);
	pbuf[6] = pbuf[7] = 0;
  	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xC4, 0, 0, pbuf, 8, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		if (isdeb) fprintf(stderr,"\ni1pro: EEprom read failed with ICOM err 0x%x\n",se);
		p->icom->debug = p->debug;
		return rv;
	}

	/* Now read the bytes */
	se = p->icom->usb_read(p->icom, 0x82, buf, size, &rwbytes, 5.0);
	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		if (isdeb) fprintf(stderr,"\ni1pro: EEprom read failed with ICOM err 0x%x\n",se);
		p->icom->debug = p->debug;
		return rv;
	}

	if (rwbytes != size) {
		if (isdeb) fprintf(stderr,"Read 0x%x bytes, short read error\n",rwbytes);
		p->icom->debug = p->debug;
		return I1PRO_HW_EE_SHORTREAD;
	}

	if (isdeb >= 3) {
		int i;
		for (i = 0; i < size; i++) {
			if ((i % 16) == 0)
				fprintf(stderr,"    %04x:",i);
			fprintf(stderr," %02x",buf[i]);
			if ((i+1) >= size || ((i+1) % 16) == 0)
				fprintf(stderr,"\n");
		}
	}

	if (isdeb) fprintf(stderr,"Read 0x%x bytes, ICOM err 0x%x (%d msec)\n",
	                   rwbytes, se, msec_time()-stime);
	p->icom->debug = p->debug;

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
	int se = 0, rv = I1PRO_OK;
	int i, isdeb = 0;
	int stime;

	/* Don't write over fixed values, as the instrument could become unusable.. */ 
	if (addr < 0 || addr > 0x1000 || (addr + size) >= 0x1000)
		return I1PRO_INT_EETOOBIG;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\ni1pro: Write EEProm address 0x%x size 0x%x @ %d msec\n",
	                   addr,size, (stime = msec_time()) - m->msec);

	if (isdeb >= 3) {
		int i;
		for (i = 0; i < size; i++) {
			if ((i % 16) == 0)
				fprintf(stderr,"    %04x:",i);
			fprintf(stderr," %02x",buf[i]);
			if ((i+1) >= size || ((i+1) % 16) == 0)
				fprintf(stderr,"\n");
		}
	}

#ifdef ENABLE_WRITE
	int2buf(&pbuf[0], addr);
	short2buf(&pbuf[4], size);
	short2buf(&pbuf[6], 0x100);		/* Might be accidental, left over from getmisc.. */
  	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xC3, 0, 0, pbuf, 8, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		if (isdeb) fprintf(stderr,"\ni1pro: EEprom write failed with ICOM err 0x%x\n",se);
		p->icom->debug = p->debug;
		return rv;
	}

	/* Now write the bytes */
	se = p->icom->usb_write(p->icom, 0x03, buf, size, &rwbytes, 5.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		if (isdeb) fprintf(stderr,"\ni1pro: EEprom write failed with ICOM err 0x%x\n",se);
		p->icom->debug = p->debug;
		return rv;
	}

	if (rwbytes != size) {
		if (isdeb) fprintf(stderr,"Write 0x%x bytes, short write error\n",rwbytes);
		p->icom->debug = p->debug;
		return I1PRO_HW_EE_SHORTREAD;
	}

	/* Now we write two separate bytes of 0 - confirm write ?? */
	for (i = 0; i < 2; i++) {
		pbuf[0] = 0;

		/* Now write the bytes */
		se = p->icom->usb_write(p->icom, 0x03, pbuf, 1, &rwbytes, 5.0);

		if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
			if (isdeb) fprintf(stderr,"\ni1pro: EEprom write failed with ICOM err 0x%x\n",se);
			p->icom->debug = p->debug;
			return rv;
		}

		if (rwbytes != 1) {
			if (isdeb) fprintf(stderr,"Write 0x%x bytes, short write error\n",rwbytes);
			p->icom->debug = p->debug;
			return I1PRO_HW_EE_SHORTREAD;
		}
	}
	if (isdeb) fprintf(stderr,"Write 0x%x bytes, ICOM err 0x%x (%d msec)\n",
	                   size, se, msec_time()-stime);

	/* The instrument needs some recovery time after a write */
	msec_sleep(50);

#else /* ENABLE_WRITE */

	if (isdeb) fprintf(stderr,"(NOT) Write 0x%x bytes, ICOM err 0x%x\n",size, se);

#endif /* ENABLE_WRITE */

	p->icom->debug = p->debug;

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
	int rwbytes;			/* Data bytes read or written */
	unsigned char pbuf[8];	/* status bytes read */
	int _fwrev;
	int _unkn1;
	int _maxpve;
	int _unkn3;
	int _powmode;
	int se, rv = I1PRO_OK;
	int isdeb = 0;
	int stime;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\ni1pro: GetMisc @ %d msec\n",(stime = msec_time()) - m->msec);

	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xC9, 0, 0, pbuf, 8, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		if (isdeb) fprintf(stderr,"\ni1pro: GetMisc failed with ICOM err 0x%x\n",se);
		p->icom->debug = p->debug;
		return rv;
	}

	_fwrev   = buf2ushort(&pbuf[0]);
	_unkn1   = buf2ushort(&pbuf[2]);	/* Value set after each read. Average ?? */
	_maxpve  = buf2ushort(&pbuf[4]);
	_unkn3   = pbuf[6];		/* Flag values are tested, but don't seem to be used ? */
	_powmode = pbuf[7];

	if (isdeb) fprintf(stderr,"GetMisc returns %d, 0x%04x, 0x%04x, 0x%02x, 0x%02x ICOM err 0x%x (%d msec)\n",
	                   _fwrev, _unkn1, _maxpve, _unkn3, _powmode, se, msec_time()-stime);

	p->icom->debug = p->debug;

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
	int rwbytes;			/* Data bytes read or written */
	unsigned char pbuf[8];	/* status bytes read */
	int _intclocks;
	int _lampclocks;
	int _nummeas;
	int _measmodeflags;
	int se, rv = I1PRO_OK;
	int isdeb = 0;
	int stime;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\ni1pro: GetMeasureParams @ %d msec\n",
	                   (stime = msec_time()) - m->msec);

	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xC2, 0, 0, pbuf, 8, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		if (isdeb) fprintf(stderr,"\ni1pro: MeasureParam failed with ICOM err 0x%x\n",se);
		p->icom->debug = p->debug;
		return rv;
	}

	_intclocks     = buf2ushort(&pbuf[0]);
	_lampclocks    = buf2ushort(&pbuf[2]);
	_nummeas       = buf2ushort(&pbuf[4]);
	_measmodeflags = pbuf[6];

	if (isdeb) fprintf(stderr,"MeasureParam returns %d, %d, %d, 0x%02x ICOM err 0x%x (%d msec)\n",
	                   _intclocks, _lampclocks, _nummeas, _measmodeflags, se, msec_time()-stime);

	p->icom->debug = p->debug;

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
	int rwbytes;			/* Data bytes read or written */
	unsigned char pbuf[8];	/* command bytes written */
	int se, rv = I1PRO_OK;
	int isdeb = 0;
	int stime;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\ni1pro: SetMeasureParam %d, %d, %d, 0x%02x @ %d msec\n",
	                   intclocks, lampclocks, nummeas, measmodeflags,
	                   (stime = msec_time()) - m->msec);

	short2buf(&pbuf[0], intclocks);
	short2buf(&pbuf[2], lampclocks);
	short2buf(&pbuf[4], nummeas);
	pbuf[6] = measmodeflags;
	pbuf[7] = 0;

	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xC1, 0, 0, pbuf, 8, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		if (isdeb) fprintf(stderr,"\ni1pro: SetMeasureParams failed with ICOM err 0x%x\n",se);
		p->icom->debug = p->debug;
		return rv;
	}

	if (isdeb) fprintf(stderr,"SetMeasureParams got ICOM err 0x%x (%d msec)\n",
	                   se,msec_time()-stime);
	p->icom->debug = p->debug;

	return rv;
}

/* Delayed trigger implementation, called from thread */
static int
i1pro_delayed_trigger(void *pp) {
	i1pro *p = (i1pro *)pp;
	i1proimp *m = (i1proimp *)p->m;
	int rwbytes;			/* Data bytes read or written */
	int se, rv = I1PRO_OK;
	int stime;

	if ((m->c_measmodeflags & I1PRO_MMF_NOLAMP) == 0) {		/* Lamp will be on for measurement */
		m->llampoffon = msec_time();						/* Record when it turned on */
//printf("~1 got lamp off -> on at %d (%f)\n",m->llampoffon, (m->llampoffon - m->llamponoff)/1000.0);

	}

	if (p->icom->debug) fprintf(stderr,"Delayed trigget start sleep @ %d msec\n",
	                            msec_time() - m->msec);

	/* Delay the trigger */
	msec_sleep(m->trig_delay);

	m->tr_t1 = msec_time();		/* Diagnostic */

	if (p->icom->debug) fprintf(stderr,"Trigger  @ %d msec\n",(stime = msec_time()) - m->msec);

	se = p->icom->usb_control_th(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xC0, 0, 0, NULL, 0, 2.0, 0, NULL, 0);

	m->tr_t2 = msec_time();		/* Diagnostic */

	m->trig_se = se;
	m->trig_rv = icoms2i1pro_err(se);

	if (p->icom->debug) fprintf(stderr,"Triggering measurement ICOM err 0x%x (%d msec)\n",
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
	int isdeb = p->debug;

	if (isdeb) fprintf(stderr,"\ni1pro: Triggering measurement after %dmsec delay @ %d msec\n",
	                           delay, msec_time() - m->msec);

	/* NOTE := would be better here to create thread once, and then trigger it */
	/* using a condition variable. */
	if (m->trig_thread != NULL)
		m->trig_thread->del(m->trig_thread);

    m->tr_t1 = m->tr_t2 = m->tr_t3 = m->tr_t4 = m->tr_t5 = m->tr_t6 = m->tr_t7 = 0;
	m->trig_delay = delay;

	if ((m->trig_thread = new_athread(i1pro_delayed_trigger, (void *)p)) == NULL) {
		if (isdeb) fprintf(stderr,"Creating delayed trigger thread failed\n");
		return I1PRO_INT_THREADFAILED;
	}

#ifdef WAIT_FOR_DELAY_TRIGGER	/* hack to diagnose threading problems */
	while (m->tr_t2 == 0) {
		Sleep(1);
	}
#endif
	if (isdeb) fprintf(stderr,"Scheduled triggering OK\n");

	return rv;
}

/* Read a measurements results. */
/* A buffer full of bytes is returned. */
/* (This will fail on a Rev. A if there is more than about a 40 msec delay */
/* between triggering the measurement and starting this read. */
/* It appears though that the read can be pending before triggering though. */
/* Scan reads will also terminate if there is too great a delay beteween each read.) */
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
) {
	i1proimp *m = (i1proimp *)p->m;
	unsigned char *ibuf = buf;	/* Incoming buffer */
	int nmeas;					/* Number of measurements for this read */
	double top, extra;			/* Time out period */
	int rwbytes;				/* Data bytes read or written */
	int se, rv = I1PRO_OK;
	int treadings = 0;
	int isdeb = 0;
	int stime;
//	int gotshort = 0;			/* nz when got a previous short reading */

	if ((bsize & 0xff) != 0) {
		return I1PRO_INT_ODDREADBUF;
	}

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\ni1pro: Read measurement results inummeas %d, scanflag %d, address 0x%x bsize 0x%x @ %d msec\n",inummeas, scanflag, buf, bsize, (stime = msec_time()) - m->msec);

	extra = 1.0;		/* Extra timeout margin */

	/* Deal with Rev A+ & Rev B quirk: */
	if ((m->fwrev >= 200 && m->fwrev < 300)
	 || (m->fwrev >= 300 && m->fwrev < 400))
		extra += m->l_inttime;
	m->l_inttime = m->c_inttime;

#ifdef SINGLE_READ
	if (scanflag == 0)
		nmeas = inummeas;
	else
		nmeas = bsize / 256;		/* Use a single large read */
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

		size = 256 * nmeas;

		if (size > bsize) {		/* oops, no room for read */
			if (isdeb) fprintf(stderr,"Buffer was too short for scan\n");
			p->icom->debug = p->debug;
			return I1PRO_INT_MEASBUFFTOOSMALL;
		}

		m->tr_t6 = msec_time();	/* Diagnostic, start of subsequent reads */
		if (m->tr_t3 == 0) m->tr_t3 = m->tr_t6;		/* Diagnostic, start of first read */

		se = p->icom->usb_read(p->icom, 0x82, buf, size, &rwbytes, top);

		m->tr_t5 = m->tr_t7;
		m->tr_t7 = msec_time();	/* Diagnostic, end of subsequent reads */
		if (m->tr_t4 == 0) {
			m->tr_t5 = m->tr_t2;
			m->tr_t4 = m->tr_t7;	/* Diagnostic, end of first read */
		}

#ifdef NEVER		/* Use short + timeout to terminate scan */
		if (gotshort != 0 && se == ICOM_TO) {	/* We got a timeout after a short read. */ 
			if (isdeb)  {
				fprintf(stderr,"Read timed out in %f secs after getting short read\n",top);
				fprintf(stderr,"(Trig & rd times %d %d %d %d)\n",
				    m->tr_t2-m->tr_t1, m->tr_t3-m->tr_t2, m->tr_t4-m->tr_t3, m->tr_t6-m->tr_t5);
			}
			break;		/* We're done */
		} else
#endif
		  if (se == ICOM_SHORT) {		/* Expect this to terminate scan reading */
			if (isdeb)  {
				fprintf(stderr,"Short read, read %d bytes, asked for %d\n",rwbytes,size);
				fprintf(stderr,"(Trig & rd times %d %d %d %d)\n",
				    m->tr_t2-m->tr_t1, m->tr_t3-m->tr_t2, m->tr_t4-m->tr_t3, m->tr_t6-m->tr_t5);
			}
		} else if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
			if (m->trig_rv != I1PRO_OK) {
				if (isdeb) fprintf(stderr,"\ni1pro: Measurement trigger failed, ICOM err 0x%x\n",m->trig_se);
				return m->trig_rv;
			}
			if (isdeb && (se & ICOM_TO))
				fprintf(stderr,"\ni1pro: Read timed out with top = %f\n",top);
		
			if (isdeb) fprintf(stderr,"\ni1pro: Read failed, bytes read 0x%x, ICOM err 0x%x\n",rwbytes, se);
			p->icom->debug = p->debug;
			return rv;
		}
 
		/* If we didn't read a multiple of 256, we've got problems */
		if ((rwbytes & 0xff) != 0) {
			if (isdeb) fprintf(stderr,"Read 0x%x bytes, odd read error\n",rwbytes);
			p->icom->debug = p->debug;
			return I1PRO_HW_ME_ODDREAD;
		}

		/* Track where we're up to */
		bsize -= rwbytes;
		buf   += rwbytes;
		treadings += rwbytes/256;

		if (scanflag == 0) {	/* Not scanning */

			/* Expect to read exactly what we asked for */
			if (rwbytes != size) {
				if (isdeb) fprintf(stderr,"Error - unexpected short read, got %d expected %d\n",rwbytes,size);
				p->icom->debug = p->debug;
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
			unsigned char tbuf[256];

			/* We need to clean up, so soak up all the data and throw it away */
			while ((se = p->icom->usb_read(p->icom, 0x82, tbuf, 256, &rwbytes, top)) == ICOM_OK)
				;
			if (isdeb) fprintf(stderr,"Buffer was too short for scan\n");
			p->icom->debug = p->debug;
			return I1PRO_INT_MEASBUFFTOOSMALL;
		}

		/* Read a bunch more readings until the read is short or times out */
		nmeas = bsize / 256;
		if (nmeas > 64)
			nmeas = 64;
		top = extra + m->c_inttime * nmeas;
	}

	if ((m->c_measmodeflags & I1PRO_MMF_NOLAMP) == 0) {		/* Lamp was on for measurement */
		m->slamponoff = m->llamponoff;						/* remember second last */
		m->llamponoff = msec_time();						/* Record when it turned off */
//printf("~1 got lamp on -> off at %d (%f)\n",m->llamponoff, (m->llamponoff - m->llampoffon)/1000.0);
		m->lampage += (m->llamponoff - m->llampoffon)/1000.0;	/* Time lamp was on */
	}
	/* Update log values */
	if (dark_measure == 0)
		m->meascount++;

	/* Must have timed out in initial readings */
	if (treadings < inummeas) {
		if (isdeb) fprintf(stderr,"\ni1pro: Read failed, bytes read 0x%x, ICOM err 0x%x\n",rwbytes, se);
		p->icom->debug = p->debug;
		return I1PRO_RD_SHORTMEAS;
	} 

	if (isdeb >= 3) {
		int i;
		for (i = 0; i < (treadings * 256); i++) {
			if ((i % 16) == 0)
				fprintf(stderr,"    %04x:",i);
			fprintf(stderr," %02x",ibuf[i]);
			if ((i+1) >= (treadings * 256) || ((i+1) % 16) == 0)
				fprintf(stderr,"\n");
		}
	}

	if (isdeb)	{
		fprintf(stderr,"Read %d readings, ICOM err 0x%x (%d msec)\n",
		                treadings, se, msec_time()-stime);
		fprintf(stderr,"(Trig & rd times %d %d %d %d)\n",
		    m->tr_t2-m->tr_t1, m->tr_t3-m->tr_t2, m->tr_t4-m->tr_t3, m->tr_t6-m->tr_t5);
	}
	p->icom->debug = p->debug;

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
	int rwbytes;			/* Data bytes read or written */
	unsigned char pbuf[1];	/* 1 bytes to write */
	int se, rv = I1PRO_OK;
	int isdeb = 0;
	int stime;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\ni1pro: Set measurement clock mode %d @ %d msec\n",
	                   mcmode, (stime = msec_time()) - m->msec);

	pbuf[0] = mcmode;
	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xCF, 0, 0, pbuf, 1, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		if (isdeb) fprintf(stderr,"\ni1pro: Set measuremnt clock mode failed with ICOM err 0x%x\n",se);
		p->icom->debug = p->debug;
		return rv;
	}

	if (isdeb) fprintf(stderr,"Set measuremnt clock mode done, ICOM err 0x%x (%d msec)\n",
	                   se, msec_time()-stime);
	p->icom->debug = p->debug;

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
	int rwbytes;			/* Data bytes read or written */
	unsigned char pbuf[8];	/* status bytes read */
	int _maxmcmode;		/* mcmode must be < maxmcmode */
	int _mcmode;		/* readback current mcmode */
	int _unknown;		/* Unknown */
	int _subclkdiv;		/* Sub clock divider ratio */
	int _intclkusec;	/* Integration clock in usec */
	int _subtmode;		/* Subtract mode on read using average of value 127 */
	int se, rv = I1PRO_OK;
	int isdeb = 0;
	int stime;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\ni1pro: GetMeasureClockMode @ %d msec\n",
	                   (stime = msec_time()) - m->msec);

	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xD1, 0, 0, pbuf, 6, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		if (isdeb) fprintf(stderr,"\ni1pro: MeasureClockMode failed with ICOM err 0x%x\n",se);
		p->icom->debug = p->debug;
		return rv;
	}

	_maxmcmode  = pbuf[0];
	_mcmode     = pbuf[1];
	_unknown    = pbuf[2];
	_subclkdiv  = pbuf[3];
	_intclkusec = pbuf[4];
	_subtmode   = pbuf[5];

	if (isdeb) fprintf(stderr,"MeasureClockMode returns %d, %d, (%d), %d, %d 0x%x ICOM err 0x%x (%d msec)\n",
	      _maxmcmode, _mcmode, _unknown, _subclkdiv, _intclkusec, _subtmode, se, msec_time()-stime);

	p->icom->debug = p->debug;

	if (maxmcmode != NULL) *maxmcmode = _maxmcmode;
	if (mcmode != NULL) *mcmode = _mcmode;
	if (subclkdiv != NULL) *subclkdiv = _subclkdiv;
	if (intclkusec != NULL) *intclkusec = _intclkusec;
	if (subtmode != NULL) *subtmode = _subtmode;

	return rv;
}


/* Wait for a reply triggered by a key press */
i1pro_code i1pro_waitfor_switch(i1pro *p, double top) {
	i1proimp *m = (i1proimp *)p->m;
	int rwbytes;			/* Data bytes read */
	unsigned char buf[8];	/* Result  */
	int se, rv = I1PRO_OK;
	int isdeb = p->debug;
	int stime;

	if (isdeb) fprintf(stderr,"\ni1pro: Read 1 byte from switch hit port @ %d msec\n",
	                   (stime = msec_time()) - m->msec);

	/* Now read 1 byte */
	se = p->icom->usb_read(p->icom, 0x84, buf, 1, &rwbytes, top);

	if ((se & ICOM_USERM) == 0 && (se & ICOM_TO)) {
		if (isdeb) fprintf(stderr,"Switch read 0x%x bytes, timed out (%d msec)\n",
		                   rwbytes,msec_time()-stime);
		p->icom->debug = p->debug;
		return I1PRO_INT_BUTTONTIMEOUT;
	}

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		if (isdeb) fprintf(stderr,"\ni1pro: Switch read failed with ICOM err 0x%x\n",se);
		p->icom->debug = p->debug;
		return rv;
	}

	if (rwbytes != 1) {
		if (isdeb) fprintf(stderr,"Switch read 0x%x bytes, short read error (%d msec)\n",
		                   rwbytes,msec_time()-stime);
		p->icom->debug = p->debug;
		return I1PRO_HW_EE_SHORTREAD;
	}

	if (isdeb) {
		fprintf(stderr,"Switch read 0x%x bytes, Byte read 0x%x ICOM err 0x%x (%d msec)\n",
		        rwbytes, buf[0], se, msec_time()-stime);
	}
	p->icom->debug = p->debug;

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
	int isdeb = p->debug;
	int stime;

	if (isdeb) fprintf(stderr,"\ni1pro: Read 1 byte from switch hit port @ %d msec\n",
	                   (stime = msec_time()) - m->msec);

	/* Now read 1 byte */
	se = p->icom->usb_read_th(p->icom, &m->hcancel, 0x84, buf, 1, &rwbytes, top, 0, NULL, 0);

	if ((se & ICOM_USERM) == 0 && (se & ICOM_TO)) {
		if (isdeb) fprintf(stderr,"Switch read 0x%x bytes, timed out (%d msec)\n",
		                   rwbytes,msec_time()-stime);
		p->icom->debug = p->debug;
		return I1PRO_INT_BUTTONTIMEOUT;
	}

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		if (isdeb) fprintf(stderr,"\ni1pro: Switch read failed with ICOM err 0x%x (%d msec)\n",
		                   se,msec_time()-stime);
		p->icom->debug = p->debug;
		return rv;
	}

	if (rwbytes != 1) {
		if (isdeb) fprintf(stderr,"Switch read 0x%x bytes, short read error (%d msec)\n",
		                   rwbytes,msec_time()-stime);
		p->icom->debug = p->debug;
		return I1PRO_HW_EE_SHORTREAD;
	}

	if (isdeb) {
		fprintf(stderr,"Switch read 0x%x bytes, Byte read 0x%x ICOM err 0x%x (%d msec)\n",
		        rwbytes, buf[0], se,msec_time()-stime);
	}
	p->icom->debug = p->debug;

	return rv; 
}

/* Terminate switch handling */
/* This seems to always return an error ? */
i1pro_code
i1pro_terminate_switch(
	i1pro *p
) {
	i1proimp *m = (i1proimp *)p->m;
	int rwbytes;			/* Data bytes read or written */
	unsigned char pbuf[8];	/* 8 bytes to write */
	int se, rv = I1PRO_OK;
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\ni1pro: Terminate switch Handling\n");

	/* These values may not be significant */
	pbuf[0] = pbuf[1] = pbuf[2] = pbuf[3] = 0xff;
	pbuf[4] = 0xfc;
	pbuf[5] = 0xee;
	pbuf[6] = 0x12;
	pbuf[7] = 0x00;
	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xD0, 3, 0, pbuf, 8, 2.0);

	if ((rv = icoms2i1pro_err(se)) != I1PRO_OK) {
		if (isdeb) fprintf(stderr,"\ni1pro: Warning: Terminate Switch Handling failed with ICOM err 0x%x\n",se);
	} else {
		if (isdeb) fprintf(stderr,"Terminate Switch Handling done, ICOM err 0x%x\n",se);
	}

	/* In case the above didn't work, cancel the I/O */
	msec_sleep(50);
	if (m->th_termed == 0) {
		DBG((dbgo,"i1pro terminate switch thread failed, canceling I/O\n"))
		p->icom->usb_cancel_io(p->icom, m->hcancel);
	}
	
	p->icom->debug = p->debug;
	return rv;
}


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

	if ((k = (i1keyv *)calloc(1, sizeof(i1keyv))) == NULL)
		error("i1data: malloc failed!");

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

	if (index < 0 || index >= k->count)
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

	if (index < 0 || index >= k->count)
		return NULL;

	return ((double *)k->data) + index;
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
static i1pro_code i1data_parse_eeprom(i1data *d, unsigned char *buf, unsigned int len) {
	int rv = I1PRO_OK;
	int dir = 0x1000;	/* Location of key directory */
	int nokeys;
	i1key key, off, nkey, noff;
	unsigned char *bp;
	int i;

	DBG((dbgo,"i1pro_parse_eeprom called with %d bytes\n",len))

	/* Room for minimum number of keys ? */
	if ((dir + 300) > len)
		return I1PRO_DATA_KEY_COUNT;

	if (buf2short(buf + dir) != 1)	/* Must be 1 */
		I1PRO_DATA_KEY_CORRUPT;
	
	nokeys = buf2short(buf + dir + 2);	/* Bytes in key table */
	if (nokeys < 300 || nokeys > 512)
		return I1PRO_DATA_KEY_COUNT;

	nokeys = (nokeys - 4)/6;				/* Number of 6 byte entries */

	if (d->debug >= 2) fprintf(stderr,"%d key/values in EEProm table\n",nokeys);

	/* We need current and next value to figure data size out */
	bp = buf + dir + 4;
	key = buf2short(bp);
	off = buf2int(bp+2);
	bp += 6;
	for (i = 0; i < nokeys; i++, bp += 6, key = nkey, off = noff) {
		i1_dtype type;
		
		if (i < (nokeys-1)) {
			nkey = buf2short(bp);
			noff = buf2int(bp+2);
		}
		type = d->det_type(d, key);

		if (d->debug >= 2)
			fprintf(stderr,"Table entry %d is Key 0x%04x, type %d addr 0x%x, size %d\n",
			                                                   i,key,type,off,noff-off);

		if (type == i1_dtype_unknown) {
			if (d->debug >= 2) fprintf(stderr,"Key 0x%04x is unknown type\n",key);
			continue;				/* Ignore it ?? */
		}
		if (type == i1_dtype_section) {
			if ((rv = i1data_add_eosmarker(d, key, off)) != I1PRO_OK) {
				if (d->debug >= 2)
					fprintf(stderr,"Key 0x%04x section marker failed with 0x%x\n",key,rv);
				return rv;
			}
			continue;
		}
		if (i >= nokeys) {
			if (d->debug >= 2) fprintf(stderr,"Last key wasn't a section marker!\n");
			return I1PRO_DATA_KEY_ENDMARK;
		}
		/* Check data is within range */
		if (off < 0 || off >= len || noff < off || noff > len) {
			if (d->debug >= 2)
				fprintf(stderr,"Key 0x%04x offset %d and lenght %d out of range\n",key,off,noff);
			return I1PRO_DATA_KEY_MEMRANGE;
		}

		if (type == i1_dtype_int) {
			if ((rv = i1data_unser_ints(d, key, off, buf + off, noff - off)) != I1PRO_OK) {
				if (d->debug >= 2)
					fprintf(stderr,"Key 0x%04x int unserialise failed with 0x%x\n",key,rv);
				return rv;
			} 
		} else if (type == i1_dtype_double) {
			if ((rv = i1data_unser_doubles(d, key, off, buf + off, noff - off)) != I1PRO_OK) {
				if (d->debug >= 2)
					fprintf(stderr,"Key 0x%04x double unserialise failed with 0x%x\n",key,rv);
				return rv;
			} 
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
	int chsum1, *chsum2;
	i1proimp *m = d->m;
	i1pro_code ev = I1PRO_OK;
	i1keyv *k, *sk, *j;

	DBG((dbgo,"i1data_prep_section1 called\n"))

	/* Compute the checksum for the first copy of the log data */
	chsum1 = m->data->checksum(m->data, 0);

	/* Locate and then set the checksum */
	if ((chsum2 = m->data->get_int(m->data, key_checksum, 0)) == NULL) {
		DBG((dbgo,"i1data_prep_section1 failed to locate checksum\n"))
		return I1PRO_INT_PREP_LOG_DATA;
	}
	*chsum2 = chsum1;

	/* Locate the first section marker */
	for (sk = d->head; sk != NULL; sk = sk->next) {
		if (sk->type == i1_dtype_section)
			break;
	}
	if (sk == NULL) {
		DBG((dbgo,"i1data_prep_section1 failed to find section marker\n"))
		return I1PRO_INT_PREP_LOG_DATA;
	}

	/* for each key up to the first section marker */
	/* check it resides within that section, and doesn't */
	/* overlap any other key. */
	for (k = d->head; k != NULL; k = k->next) {
		if (k->type == i1_dtype_section)
			break;
		if (k->addr < 0 || k->addr >= sk->addr || (k->addr + k->size) > sk->addr) {
			DBG((dbgo,"i1data_prep_section1 found key outside section\n"))
			return I1PRO_INT_PREP_LOG_DATA;
		}
		for (j = k->next; j != NULL; j = j->next) {
			if (j->type == i1_dtype_section)
				break;
			if ((j->addr >= k->addr && j->addr < (k->addr + k->size))
			 || ((j->addr + j->size) > k->addr && (j->addr + j->size) <= (k->addr + k->size))) {
				DBG((dbgo,"i1data_prep_section1 found key overlap section, 0x%x %d and 0x%x %d\n",
				k->addr, k->size, j->addr, j->size))
				return I1PRO_INT_PREP_LOG_DATA;
			}
		}
	}

	/* Allocate the buffer for the data */
	*len = sk->addr;
	if ((*buf = (unsigned char *)calloc(sk->addr, sizeof(unsigned char))) == NULL)
		error("i1data: malloc failed!");

	/* Serialise it into the buffer */
	for (k = d->head; k != NULL; k = k->next) {
		if (k->type == i1_dtype_section)
			break;
		else if (k->type == i1_dtype_int) {
			if ((ev = m->data->ser_ints(m->data, k, *buf, *len)) != I1PRO_OK) {
				DBG((dbgo,"i1data_prep_section1 serializing ints failed\n"))
				return ev;
			}
		} else if (k->type == i1_dtype_double) {
			if ((ev = m->data->ser_doubles(m->data, k, *buf, *len)) != I1PRO_OK) {
				DBG((dbgo,"i1data_prep_section1 serializing doubles failed\n"))
				return ev;
			}
		} else {
			DBG((dbgo,"i1data_prep_section1 tried to serialise unknown type\n"))
			return I1PRO_INT_PREP_LOG_DATA;
		}
	}
	DBG((dbgo,"a_prep_section1 done\n"))
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
i1data *new_i1data(i1proimp *m, int verb, int debug) {
	i1data *d;
	if ((d = (i1data *)calloc(1, sizeof(i1data))) == NULL)
		error("i1data: malloc failed!");

	d->p = m->p;
	d->m = m;

	d->verb = verb;
	d->debug = debug;

	d->find_key      = i1data_find_key;
	d->make_key      = i1data_make_key;
	d->get_type      = i1data_get_type;
	d->get_count     = i1data_get_count;
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
