
/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   12/1/2009
 *
 * Copyright 2006 - 2014, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * (Base on i1pro_imp.c)
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
#undef USE_HIGH_GAIN_MODE /* [Und] Make use of high gain mode in emissive measurements */
#define USE_THREAD		/* [Def] Need to use thread, or there are 1.5 second internal */
						/* instrument delays ! */
#define ENABLE_NONVCAL	/* [Def] Enable saving calibration state between program runs to a file */
#define ENABLE_NONLINCOR	/* [Def] Enable non-linear correction */
						/* NOTE :- high gain scaling will be stuffed if disabled! */
#define ENABLE_LEDTEMPC	/* [Def] Enable LED temperature compensation */
#define ENABLE_BKDRIFTC	/* [Def] Enable Emis. Black drift compensation using sheilded cell values */
#define HEURISTIC_BKDRIFTC	/* [Def] Use heusristic black drift correction version */
#undef  ENABLE_REFSTRAYC	/* [Und] Enable Reflective stray light compensation */
#define REFSTRAYC_FACTOR 0.000086 /* [0.00043] Compensation factor */
#undef  ENABLE_REFLEDINTER	/* [Und] Enable Reflective LED interference correction */

#define ENABLE_SPOS_CHECK	/* [Def] Check the sensor position is reasonable for measurement */
#define FILTER_SPOS_EVENTS	/* [Def] Use a thread to filter SPOS event changes */
#define FILTER_TIME 500		/* [500] Filter time in msec */
#define DCALTOUT (1 * 60 * 60)		/* [1 Hrs] Dark Calibration timeout in seconds */
#define WCALTOUT (24 * 60 * 60)		/* [24 Hrs] White Calibration timeout in seconds */
#define MAXSCANTIME 20.0	/* [20 Sec] Maximum scan time in seconds */
#define SW_THREAD_TIMEOUT (10 * 60.0) 	/* [10 Min] Switch read thread timeout */

#define SINGLE_READ		/* [Def] Use a single USB read for scan to eliminate latency issues. */
#define HIGH_RES		/* [Def] Enable high resolution spectral mode code. Disable */
						/* to break dependency on rspl library. */
# undef FAST_HIGH_RES_SETUP	/* Slightly better accuracy ? */

/* Debug [Und] */
#undef DEBUG			/* Turn on extra messages & plots */
#undef PLOT_DEBUG		/* Use plot to show readings & processing */
#undef PLOT_REFRESH 	/* Plot refresh rate measurement info */
#undef PLOT_UPDELAY		/* Plot data used to determine display update delay */
#undef RAWR_DEBUG		/* Print out raw reading processing values */
#undef DUMP_SCANV		/* Dump scan readings to a file "mkdump.txt" */
#undef DUMP_DARKM		/* Append raw dark readings to file "mkddump.txt" */
#undef DUMP_BKLED		/* Save REFSTRAYC & REFLEDNOISE comp plot to "refbk1.txt" & "refbk2.txt" */
#undef APPEND_MEAN_EMMIS_VAL /* Append averaged uncalibrated reading to file "mkdump.txt" */
#undef IGNORE_WHITE_INCONS	/* Ignore define reference reading inconsistency */
#undef TEST_DARK_INTERP	/* Test out the dark interpolation (need DEBUG for plot) */
#undef PLOT_RCALCURVE	/* Plot the reflection reference curve */
#undef PLOT_ECALCURVES	/* Plot the emission reference curves */
#undef PLOT_TEMPCOMP	/* Plot before and after LED temp. compensation */
#undef PLOT_PATREC		/* Print & Plot patch/flash recognition information */
#undef HIGH_RES_DEBUG
#undef HIGH_RES_PLOT
#undef HIGH_RES_PLOT_STRAYL		/* Plot stray light upsample */


#define DISP_INTT 0.7			/* Seconds per reading in display spot mode */
								/* More improves repeatability in dark colors, but limits */
								/* the maximum brightness level befor saturation. */
								/* A value of 2.0 seconds has a limit of about 110 cd/m^2 */
#define DISP_INTT2 0.3			/* High brightness display spot mode seconds per reading, */
								/* Should be good up to 275 cd/m^2 */
#define DISP_INTT3 0.1			/* High brightness display spot mode seconds per reading, */
								/* Should be good up to 700 cd/m^2 */

#define ADARKINT_MIN 0.01		/* Min cal time for adaptive dark cal */
#define ADARKINT_MAX 2.0		/* Max cal time for adaptive dark cal with high gain mode */
#define ADARKINT_MAX2 4.0		/* Max cal time for adaptive dark for no high gain */

#define RDEAD_TIME 0.004432		/* Fudge figure to make reflecting intn. time scale linearly */
					
#define EMIS_SCALE_FACTOR 1.0	/* Emission mode scale factor */ 
#define AMB_SCALE_FACTOR 1.0	/* Ambient mode scale factor for Lux */

#define NSEN_MAX 140            /* Maximum nsen/raw value we can cope with */

/* Wavelength to start duplicating values below, because it is too noisy */
#define WL_REF_MIN 420.0
#define WL_EMIS_MIN 400.0

/* High res mode settings */
#define HIGHRES_SHORT 380		/* Wavelength to calculate. Too noisy to try expanding range. */
#define HIGHRES_LONG  730
#define HIGHRES_WIDTH  (10.0/3.0) /* (The 3.3333 spacing and lanczos2 seems a good combination) */


#include "munki.h"
#include "munki_imp.h"

/* - - - - - - - - - - - - - - - - - - */

#define PATCH_CONS_THR 0.05		/* Dark measurement consistency threshold */
#define DARKTHSCAMIN 5000.0		/* Dark threshold scaled/offset minimum */

/* - - - - - - - - - - - - - - - - - - */

/* Three levels of runtime debugging messages:

	~~~ this is no longer accurate. a1logd calls
	~~~ probably need to be tweaked.
   1 = default, typical I/O messages etc.
   2 = more internal operation messages
   3 = dump extra detailes
   4 = dump EEPROM data
   5 = dump tables etc
*/

/* ============================================================ */

// Print bytes as hex to debug log */
static void dump_bytes(a1log *log, char *pfx, unsigned char *buf, int base, int len) {
	int i, j, ii;
	char oline[200] = { '\000' }, *bp = oline;
	for (i = j = 0; i < len; i++) {
		if ((i % 16) == 0)
			bp += sprintf(bp,"%s%04x:",pfx,base+i);
		bp += sprintf(bp," %02x",buf[i]);
		if ((i+1) >= len || ((i+1) % 16) == 0) {
			for (ii = i; ((ii+1) % 16) != 0; ii++)
				bp += sprintf(bp,"   ");
			bp += sprintf(bp,"  ");
			for (; j <= i; j++) {
				if (!(buf[j] & 0x80) && isprint(buf[j]))
					bp += sprintf(bp,"%c",buf[j]);
				else
					bp += sprintf(bp,".");
			}
			bp += sprintf(bp,"\n");
			a1logd(log,0,"%s",oline);
			bp = oline;
		}
	}
}

/* ============================================================ */
/* Debugging plot support */

#if defined(DEBUG) || defined(PLOT_DEBUG) || defined(PLOT_PATREC) || defined(HIGH_RES_PLOT) ||  defined(HIGH_RES_PLOT_STRAYL)

# include <plot.h>

static int disdebplot = 0;

# define DISDPLOT disdebplot = 1;
# define ENDPLOT disdebplot = 0;

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
	double xx[NSEN_MAX];
	double y1[NSEN_MAX];
	double y2[NSEN_MAX];

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
void plot_wav(munkiimp *m, double *data) {
	int i;
	double xx[NSEN_MAX];
	double yy[NSEN_MAX];

	if (disdebplot)
		return;

	for (i = 0; i < m->nwav; i++) {
		xx[i] = XSPECT_WL(m->wl_short, m->wl_long, m->nwav, i);
		yy[i] = data[i];
	}
	do_plot(xx, yy, NULL, NULL, m->nwav);
}

/* Plot a standard res spectra */
void plot_wav1(munkiimp *m, double *data) {
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
void plot_wav2(munkiimp *m, double *data) {
	int i;
	double xx[NSEN_MAX];
	double yy[NSEN_MAX];

	if (disdebplot)
		return;

	for (i = 0; i < m->nwav2; i++) {
		xx[i] = XSPECT_WL(m->wl_short2, m->wl_long2, m->nwav2, i);
		yy[i] = data[i];
	}
	do_plot(xx, yy, NULL, NULL, m->nwav2);
}

#else	/* !PLOT_DEBUG */

# define DISDPLOT 
# define ENDPLOT 

#endif	/* !PLOT_DEBUG */

/* ============================================================ */

munki_code munki_touch_calibration(munki *p);

/* Implementation struct */

/* Add an implementation structure */
munki_code add_munkiimp(munki *p) {
	munkiimp *m;

	if ((m = (munkiimp *)calloc(1, sizeof(munkiimp))) == NULL) {
		a1logd(p->log,3,"add_munkiimp malloc %lu bytes failed (1)\n",sizeof(munkiimp));
		return MUNKI_INT_MALLOC;
	}
	m->p = p;

	m->lo_secs = 2000000000;        /* A very long time */

	p->m = (void *)m;
	return MUNKI_OK;
}

/* Shutdown instrument, and then destroy */
/* implementation structure */
void del_munkiimp(munki *p) {

	a1logd(p->log,3,"munki_del called\n");

#ifdef ENABLE_NONVCAL
	/* Touch it so that we know when the instrument was last open */
	munki_touch_calibration(p);
#endif /* ENABLE_NONVCAL */

	if (p->m != NULL) {
		int i;
		munkiimp *m = (munkiimp *)p->m;
		munki_state *s;

#ifdef FILTER_SPOS_EVENTS
		if (m->spos_th != NULL)
			m->spos_th_term = 1;
#endif
		
		if (m->th != NULL) {		/* Terminate switch monitor thread by simulating an event */
			m->th_term = 1;			/* Tell thread to exit on error */
			munki_simulate_event(p, mk_eve_spos_change, 0);
			for (i = 0; m->th_termed == 0 && i < 5; i++)
				msec_sleep(50);	/* Wait for thread to terminate */
			if (i >= 5) {
				a1logd(p->log,3,"Munki switch thread termination failed\n");
			}
			m->th->del(m->th);
			usb_uninit_cancel(&m->sw_cancel);	/* Don't need cancel token now */
		}

#ifdef FILTER_SPOS_EVENTS
		if (m->spos_th != NULL) {
			for (i = 0; m->spos_th_termed == 0 && i < 5; i++)
				msec_sleep(50);	/* Wait for thread to terminate */
			if (i >= 5) {
				a1logd(p->log,3,"Munki spos thread termination failed\n");
			}
			m->spos_th->del(m->spos_th);
		}
#endif

		/* Free any per mode data */
		for (i = 0; i < mk_no_modes; i++) {
			s = &m->ms[i];

			free_dvector(s->dark_data, -1, m->nraw-1);  
			free_dvector(s->dark_data2, -1, m->nraw-1);  
			free_dvector(s->dark_data3, -1, m->nraw-1);  
			free_dvector(s->white_data, -1, m->nraw-1);
			free_dmatrix(s->iwhite_data, 0, 1, -1, m->nraw-1);  
			free_dmatrix(s->idark_data, 0, 3, -1, m->nraw-1);  

			free_dvector(s->cal_factor1, 0, m->nwav1-1);
			free_dvector(s->cal_factor2, 0, m->nwav2-1);
		}

		/* Free EEProm key data */
		if (m->data != NULL)
			m->data->del(m->data);

		/* Free arrays */

		if (m->lin0 != NULL)
			free(m->lin0);
		if (m->lin1 != NULL)
			free(m->lin1);

		if (m->white_ref1 != NULL)
			free(m->white_ref1);
		if (m->emis_coef1 != NULL)
			free(m->emis_coef1);
		if (m->amb_coef1 != NULL)
			free(m->amb_coef1);
		if (m->proj_coef1 != NULL)
			free(m->proj_coef1);

		if (m->white_ref2 != NULL)
			free(m->white_ref2);
		if (m->emis_coef2 != NULL)
			free(m->emis_coef2);
		if (m->amb_coef2 != NULL)
			free(m->amb_coef2);
		if (m->proj_coef2 != NULL)
			free(m->proj_coef2);

		if (m->straylight1 != NULL)
			free_dmatrix(m->straylight1, 0, m->nwav1-1, 0, m->nwav1-1);  

		if (m->straylight2 != NULL)
			free_dmatrix(m->straylight2, 0, m->nwav1-2, 0, m->nwav1-2);  

		if (m->rmtx_index1 != NULL)
			free(m->rmtx_index1);
		if (m->rmtx_nocoef1 != NULL)
			free(m->rmtx_nocoef1);
		if (m->rmtx_coef1 != NULL)
			free(m->rmtx_coef1);

		if (m->rmtx_index2 != NULL)
			free(m->rmtx_index2);
		if (m->rmtx_nocoef2 != NULL)
			free(m->rmtx_nocoef2);
		if (m->rmtx_coef2 != NULL)
			free(m->rmtx_coef2);

		if (m->emtx_index1 != NULL)
			free(m->emtx_index1);
		if (m->emtx_nocoef1 != NULL)
			free(m->emtx_nocoef1);
		if (m->emtx_coef1 != NULL)
			free(m->emtx_coef1);

		if (m->emtx_index2 != NULL)
			free(m->emtx_index2);
		if (m->emtx_nocoef2 != NULL)
			free(m->emtx_nocoef2);
		if (m->emtx_coef2 != NULL)
			free(m->emtx_coef2);

		free(m);
		p->m = NULL;
	}
}

/* ============================================================ */
/* Little endian wire format conversion routines */

/* Take an int, and convert it into a byte buffer little endian */
static void int2buf(unsigned char *buf, int inv) {
	buf[3] = (inv >> 24) & 0xff;
	buf[2] = (inv >> 16) & 0xff;
	buf[1] = (inv >> 8) & 0xff;
	buf[0] = (inv >> 0) & 0xff;
}

/* Take a short, and convert it into a byte buffer little endian */
static void short2buf(unsigned char *buf, int inv) {
	buf[1] = (inv >> 8) & 0xff;
	buf[0] = (inv >> 0) & 0xff;
}

/* Take a word sized buffer, and convert it to an int */
static int buf2int(unsigned char *buf) {
	int val;
	val =      ((signed char *)buf)[3];
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

/* Take a word sized buffer, and convert it to an unsigned int */
static unsigned int buf2uint(unsigned char *buf) {
	unsigned int val;
	val =               (0xff & buf[3]);
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

/* Take a short sized buffer, and convert it to an int */
static int buf2short(unsigned char *buf) {
	int val;
	val =      ((signed char *)buf)[1];
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

/* Take a unsigned short sized buffer, and convert it to an int */
static int buf2ushort(unsigned char *buf) {
	int val;
	val =               (0xff & buf[1]);
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

/* ============================================================ */
/* High level functions */

/* Initialise our software state from the hardware */
munki_code munki_imp_init(munki *p) {
	munkiimp *m = (munkiimp *)p->m;
	munki_code ev = MUNKI_OK;
	unsigned char buf[4];
	int calsize = 0, rucalsize;
	unsigned char *calbuf;	/* EEProm contents */

	a1logd(p->log,2,"munki_init:\n");

	if (p->itype != instColorMunki)
		return MUNKI_UNKNOWN_MODEL;

#ifdef ENABLE_SPOS_CHECK
	m->nosposcheck = 0;
#else
# pragma message("####### ColorMunki Sensor Position Check is OFF! ########")
	m->nosposcheck = 1;
#endif

	m->trig = inst_opt_trig_user;
	m->scan_toll_ratio = 1.0;

	/* Get the firmware parameters so that we can check eeprom range. */
	if ((ev = munki_getfirm(p, &m->fwrev, &m->tickdur, &m->minintcount, &m->noeeblocks, &m->eeblocksize)) != MUNKI_OK)
		return ev; 
	a1logd(p->log,2,"Firmware rev = %d.%d\n",m->fwrev/256, m->fwrev % 256);

	/* Check the EEProm */
	if (m->noeeblocks != 2 || m->eeblocksize != 8192) {
		a1logw(p->log,"EEProm is unexpected size\n");
		return MUNKI_INT_ASSERT;
	}

	/* Dump the eeprom contents as a block */
	if (p->log->debug >= 7) {
		int base, size;
	
		a1logd(p->log,7, "EEPROM contents:\n"); 

		size = 8192;
		for (base = 0; base < (2 * 8192); base += 8192) {
			unsigned char eeprom[8192];

			if ((ev = munki_readEEProm(p, eeprom, base, size)) != MUNKI_OK)
				return ev;
		
			dump_bytes(p->log, "  ", eeprom, base, size);
		}
	}

	/* Tick in seconds */
	m->intclkp = (double)m->tickdur * 1e-6;

	/* Set these to reasonable values */
	m->min_int_time = m->intclkp * (double)m->minintcount;
	m->max_int_time = 4.5;

	a1logd(p->log,3, "minintcount %d, min_int_time = %f\n", m->minintcount, m->min_int_time);

	/* Get the Chip ID */
	if ((ev = munki_getchipid(p, m->chipid)) != MUNKI_OK)
		return ev; 

	/* Get the Version String */
	if ((ev = munki_getversionstring(p, m->vstring)) != MUNKI_OK)
		return ev; 

	/* Read the calibration size */
	if ((ev = munki_readEEProm(p, buf, 4, 4)) != MUNKI_OK)
		return ev;
	calsize = buf2int(buf);
	rucalsize = (calsize + 3) & ~3;	/* Round up to next 32 bits */
	
	if (calsize < 12)
		return MUNKI_INT_CALTOOSMALL;
	if (calsize > (m->noeeblocks * m->eeblocksize))
		return MUNKI_INT_CALTOOBIG;

	/* Read the calibration raw data from the EEProm */
	 if ((calbuf = (unsigned char *)calloc(rucalsize, sizeof(unsigned char))) == NULL) {
		a1logd(p->log,3,"munki_imp_init malloc %d bytes failed\n",rucalsize);
		return MUNKI_INT_MALLOC;
	}
	if ((ev = munki_readEEProm(p, calbuf, 0, calsize)) != MUNKI_OK)
		return ev;

	if ((ev = munki_parse_eeprom(p, calbuf, rucalsize)) != MUNKI_OK)
		return ev;

	free(calbuf);
	calbuf = NULL;

#ifdef USE_THREAD
	/* Setup the switch monitoring thread */
	usb_init_cancel(&m->sw_cancel);			/* Get cancel token ready */
	if ((m->th = new_athread(munki_switch_thread, (void *)p)) == NULL)
		return MUNKI_INT_THREADFAILED;
#endif

#ifdef FILTER_SPOS_EVENTS
	/* Setup the sensor position filter thread */
	if ((m->spos_th = new_athread(munki_spos_thread, (void *)p)) == NULL)
		return MUNKI_INT_THREADFAILED;
#endif

	/* Set up the current state of each mode */
	{
		int i, j;
		munki_state *s;

		/* First set state to basic configuration */
		for (i = 0; i < mk_no_modes; i++) {
			s = &m->ms[i];

			s->mode = i;

			/* Default to an emissive configuration */
			s->targoscale = 0.90;	/* Allow extra 10% margine by default */
			s->targmaxitime = 2.0;	/* Maximum integration time to aim for */
			s->targoscale2 = 0.15;	/* Proportion of targoscale to meed targmaxitime */

#ifdef USE_HIGH_GAIN_MODE
			s->auto_gain = 1;		/* No high gain by default */
#else
			s->auto_gain = 0;		/* No high gain by default */
#endif
			s->gainmode = 0;		/* Normal gain mode */
			s->inttime = 0.5;		/* Initial integration time */


			s->dark_valid = 0;		/* Dark cal invalid */
			s->dark_data = dvectorz(-1, m->nraw-1);  
			s->dark_data2 = dvectorz(-1, m->nraw-1);  
			s->dark_data3 = dvectorz(-1, m->nraw-1);  

			s->cal_valid = 0;		/* Scale cal invalid */
			s->cal_factor1 = dvectorz(0, m->nwav1-1);
			s->cal_factor2 = dvectorz(0, m->nwav2-1);
			s->cal_factor = s->cal_factor1; /* Default to standard resolution */
			s->white_data = dvectorz(-1, m->nraw-1);
			s->iwhite_data = dmatrixz(0, 1, -1, m->nraw-1);  

			s->idark_valid = 0;		/* Interpolatable Dark cal invalid */
			s->idark_data = dmatrixz(0, 3, -1, m->nraw-1);  

			s->dark_int_time  = DISP_INTT;	/* 0.7 */
			s->dark_int_time2 = DISP_INTT2;	/* 0.3 */
			s->dark_int_time3 = DISP_INTT3;	/* 0.1 */

			s->idark_int_time[0] = s->idark_int_time[2] = m->min_int_time;
			s->idark_int_time[1] = ADARKINT_MAX; /* 2.0 */
			s->idark_int_time[3] = ADARKINT_MAX2; /* 4.0 */

			s->want_calib = 1;		/* By default want an initial calibration */
			s->want_dcalib = 1;
		}

		/* Then add mode specific settings */
		for (i = 0; i < mk_no_modes; i++) {
			s = &m->ms[i];
			switch(i) {
				case mk_refl_spot:
					s->auto_gain = 0;			/* Don't automatically set gain */
					s->targoscale = 1.0;		/* Optimised sensor scaling to full */
					s->reflective = 1;
					s->adaptive = 0;
					s->inttime = s->targoscale * m->cal_int_time;
					s->dark_int_time = s->inttime;

					s->dpretime = 0.20;			/* Pre-measure time */
					s->wpretime = 0.20;
					s->dcaltime = 0.5;			/* same as reading */
					s->wcaltime = 0.5;			/* same as reading */
					s->dreadtime = 0.5;			/* same as reading */
					s->wreadtime = 0.5;
					s->maxscantime = 0.0;
					break;

				case mk_refl_scan:
					s->auto_gain = 0;				/* Don't automatically set gain */
					s->targoscale = 1.0;			/* Maximize level */
					s->reflective = 1;
					s->scan = 1;
					s->adaptive = 0;
//					s->inttime = (s->targoscale * m->cal_int_time - RDEAD_TIME) + RDEAD_TIME;
//					if (s->inttime < m->min_int_time)
//						s->inttime = m->min_int_time;
//					s->dark_int_time = s->inttime;
					s->inttime = s->targoscale * m->cal_int_time;
					s->dark_int_time = s->inttime;

					s->dpretime = 0.20;		/* Pre-measure time */
					s->wpretime = 0.20;
					s->dcaltime = 0.5;
					s->wcaltime = 0.5;
					s->dreadtime = 0.10;
					s->wreadtime = 0.10;
					s->maxscantime = MAXSCANTIME;
					break;

				case mk_emiss_spot_na:			/* Emissive spot not adaptive */
				case mk_tele_spot_na:			/* Tele spot not adaptive */
					s->targoscale = 0.90;		/* Allow extra 10% margine */
					if (i == mk_emiss_spot_na) {
						for (j = 0; j < m->nwav1; j++)
							s->cal_factor1[j] = EMIS_SCALE_FACTOR * m->emis_coef1[j];
					} else {
						for (j = 0; j < m->nwav1; j++)
							s->cal_factor1[j] = EMIS_SCALE_FACTOR * m->proj_coef1[j];
						s->projector = 1;
					}
					s->cal_valid = 1;
					s->emiss = 1;
					s->adaptive = 0;

					s->inttime = DISP_INTT;		/* Default disp integration time (ie. 0.7 sec) */
					s->dark_int_time = s->inttime;
					s->dark_int_time2 = DISP_INTT2;	/* Alternate disp integration time (ie. 0.3) */
					s->dark_int_time3 = DISP_INTT3;	/* Alternate disp integration time (ie. 0.1) */

					s->dpretime = 0.0;
					s->wpretime = 0.20;
					s->dcaltime = 1.0;		/* ie. determines number of measurements */
					s->dcaltime2 = 1.0;
					s->dcaltime3 = 1.0;
					s->wcaltime = 0.0;
					s->dreadtime = 0.0;
					s->wreadtime = DISP_INTT;
					s->maxscantime = 0.0;
					break;

				case mk_emiss_spot:
				case mk_amb_spot:
				case mk_tele_spot:				/* Adaptive projector */
					s->targoscale = 0.90;		/* Allow extra 5% margine */
					if (i == mk_emiss_spot) {
						for (j = 0; j < m->nwav1; j++)
							s->cal_factor1[j] = EMIS_SCALE_FACTOR * m->emis_coef1[j];
					} else if (i == mk_amb_spot) {
						for (j = 0; j < m->nwav1; j++)
							s->cal_factor1[j] = AMB_SCALE_FACTOR * m->amb_coef1[j];
						s->ambient = 1;
					} else {
						for (j = 0; j < m->nwav1; j++)
							s->cal_factor1[j] = EMIS_SCALE_FACTOR * m->proj_coef1[j];
						s->projector = 1;
					}

					s->cal_valid = 1;
					s->emiss = 1;
					s->adaptive = 1;

					s->dpretime = 0.0;
					s->wpretime = 0.10;
					s->dcaltime = 1.0;
					s->wcaltime = 0.0;
					s->dreadtime = 0.0;
					s->wreadtime = 1.0;
					s->maxscantime = 0.0;
					break;

				case mk_emiss_scan:
				case mk_amb_flash:
					s->targoscale = 0.90;		/* Allow extra 10% margine */
					if (i == mk_emiss_scan) {
						for (j = 0; j < m->nwav1; j++)
							s->cal_factor1[j] = EMIS_SCALE_FACTOR * m->emis_coef1[j];
					} else {
						for (j = 0; j < m->nwav1; j++)
							s->cal_factor1[j] = AMB_SCALE_FACTOR * m->amb_coef1[j];
						s->ambient = 1;
						s->flash = 1;
					}
					s->cal_valid = 1;
					s->emiss = 1;
					s->scan = 1;
					s->adaptive = 0;
					s->inttime = m->min_int_time;
					s->dark_int_time = s->inttime;

					s->dpretime = 0.0;
					s->wpretime = 0.10;
					s->dcaltime = 1.0;
					s->wcaltime = 0.0;
					s->dreadtime = 0.0;
					s->wreadtime = 0.10;
					s->maxscantime = MAXSCANTIME;
					break;

				/* Transparency has a white reference, and interpolated dark ref. */
				case mk_trans_spot:
					s->targoscale = 0.90;		/* Allow extra 10% margine */
					s->trans = 1;
					s->adaptive = 1;

					s->dpretime = 0.20;
					s->wpretime = 0.20;
					s->dcaltime = 1.0;
					s->wcaltime = 1.0;
					s->dreadtime = 0.0;
					s->wreadtime = 1.0;
					s->maxscantime = 0.0;
					break;

				case mk_trans_scan:
					// ~~99 should we use high gain mode ??
					s->targoscale = 0.10;		/* Scan as fast as possible */
					s->trans = 1;
					s->scan = 1;
					s->inttime = s->targoscale * m->cal_int_time;
					if (s->inttime < m->min_int_time)
						s->inttime = m->min_int_time;
					s->dark_int_time = s->inttime;
					s->adaptive = 0;

					s->dpretime = 0.20;
					s->wpretime = 0.20;
					s->dcaltime = 1.0;
					s->wcaltime = 1.0;
					s->dreadtime = 0.00;
					s->wreadtime = 0.10;
					s->maxscantime = MAXSCANTIME;
					break;
			}
		}
	}

#ifdef ENABLE_NONVCAL
	/* Restore the all modes calibration from the local system */
	munki_restore_calibration(p);
	/* Touch it so that we know when the instrument was last opened */
	munki_touch_calibration(p);
#endif
	
	a1logv(p->log, 1, 
		"Instrument Type:   ColorMunki\n"	// ~~ should get this from version string ?
		"Serial Number:     %s\n"
		"Firmware version:  %d\n"
		"Chip ID:           %02X-%02X%02X%02X%02X%02X%02X%02X\n"
		"Version string:    '%s'\n"
		"Calibration Ver.:  %d\n"
		"Production No.:    %d\n",
		m->serno,
		m->fwrev,
	    m->chipid[0], m->chipid[1], m->chipid[2], m->chipid[3],
		m->chipid[4], m->chipid[5], m->chipid[6], m->chipid[7],
		m->vstring,
		m->calver,
		m->prodno);

	/* Flash the LED, just cos we can! */
	if ((ev = munki_setindled(p, 1000,0,0,-1,0)) != MUNKI_OK)
		return ev;
	msec_sleep(200);
	if ((ev = munki_setindled(p, 0,0,0,0,0)) != MUNKI_OK)
		return ev;

	return ev;
}

/* Return a pointer to the serial number */
char *munki_imp_get_serial_no(munki *p) {
	munkiimp *m = (munkiimp *)p->m;

	return m->serno; 
}

/* Set the measurement mode. It may need calibrating */
munki_code munki_imp_set_mode(
	munki *p,
	mk_mode mmode,		/* Operating mode */
	inst_mode mode		/* Full mode mask for options */
) {
	munkiimp *m = (munkiimp *)p->m;

	a1logd(p->log,2,"munki_imp_set_mode called with mode no. %d and mask 0x%x\n",mmode,m);
	switch(mmode) {
		case mk_refl_spot:
		case mk_refl_scan:
		case mk_emiss_spot_na:
		case mk_tele_spot_na:
		case mk_emiss_spot:
		case mk_tele_spot:
		case mk_emiss_scan:
		case mk_amb_spot:
		case mk_amb_flash:
		case mk_trans_spot:
		case mk_trans_scan:
			m->mmode = mmode;
			break;
		default:
			return MUNKI_INT_ILLEGALMODE;
	}
	m->spec_en = (mode & inst_mode_spectral) != 0;

	if ((mode & inst_mode_highres) != 0) {
		munki_code rv;
		if ((rv = munki_set_highres(p)) != MUNKI_OK)
			return rv;
	} else {
		munki_set_stdres(p);	/* Ignore any error */
	}

	return MUNKI_OK;
}

/* Return needed and available inst_cal_type's */
munki_code munki_imp_get_n_a_cals(munki *p, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	munkiimp *m = (munkiimp *)p->m;
	munki_state *cs = &m->ms[m->mmode];
	time_t curtime = time(NULL);
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;

	a1logd(p->log,3,"munki_imp_get_n_a_cals: checking mode %d\n",m->mmode);

	/* Timout calibrations that are too old */
	a1logd(p->log,4,"curtime %u, iddate %u, ddate %u, cfdate %u\n",curtime,cs->iddate,cs->ddate,cs->cfdate);
	if ((curtime - cs->iddate) > DCALTOUT) {
		a1logd(p->log,3,"Invalidating adaptive dark cal as %d secs from last cal\n",curtime - cs->iddate);
		cs->idark_valid = 0;
	}
	if ((curtime - cs->ddate) > DCALTOUT) {
		a1logd(p->log,3,"Invalidating dark cal as %d secs from last cal\n",curtime - cs->ddate);
		cs->dark_valid = 0;
	}
	if (!cs->emiss && (curtime - cs->cfdate) > WCALTOUT) {
		a1logd(p->log,3,"Invalidating white cal as %d secs from last cal\n",curtime - cs->cfdate);
		cs->cal_valid = 0;
	}

	if (cs->reflective) {
		if (!cs->dark_valid
		 || (cs->want_dcalib && !m->noinitcalib))
			n_cals |= inst_calt_ref_dark;
		a_cals |= inst_calt_ref_dark;

		if (!cs->cal_valid
		 || (cs->want_calib && !m->noinitcalib))
			n_cals |= inst_calt_ref_white;
		a_cals |= inst_calt_ref_white;
	}
	if (cs->emiss) {
		if ((!cs->adaptive && !cs->dark_valid)
		 || (cs->adaptive && !cs->idark_valid)
		 || (cs->want_dcalib && !m->noinitcalib))
			n_cals |= inst_calt_em_dark;
		a_cals |= inst_calt_em_dark;
	}
	if (cs->trans) {
		if ((!cs->adaptive && !cs->dark_valid)
		 || (cs->adaptive && !cs->idark_valid)
	     || (cs->want_dcalib && !m->noinitcalib))
			n_cals |= inst_calt_trans_dark;
		a_cals |= inst_calt_trans_dark;

		if (!cs->cal_valid
	     || (cs->want_calib && !m->noinitcalib))
			n_cals |= inst_calt_trans_vwhite;
		a_cals |= inst_calt_trans_vwhite;
	}
	if (cs->emiss && !cs->scan && !cs->adaptive) {
		if (!cs->done_dintsel)
			n_cals |= inst_calt_emis_int_time;
		a_cals |= inst_calt_emis_int_time;
	}

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	a1logd(p->log,3,"munki_imp_get_n_a_cals: returning n_cals 0x%x, a_cals 0x%x\n",n_cals, a_cals);

	return MUNKI_OK;
}

/* - - - - - - - - - - - - - - - - */
/* Calibrate for the current mode. */
/* Request an instrument calibration of the current mode. */
munki_code munki_imp_calibrate(
	munki *p,
	inst_cal_type *calt,	/* Calibration type to do/remaining */
	inst_cal_cond *calc,	/* Current condition/desired condition */
	char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	int mmode = m->mmode;					/* Current actual mode */
	munki_state *cs = &m->ms[m->mmode];
	int sx1, sx2, sx;
	time_t cdate = time(NULL);
	int nummeas = 0;
	mk_spos spos;
	int i, j, k;
	inst_cal_type needed, available;

	a1logd(p->log,3,"munki_imp_calibrate called with calt 0x%x, calc 0x%x\n",*calt, *calc);

	if ((ev = munki_imp_get_n_a_cals(p, &needed, &available)) != MUNKI_OK)
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

		a1logd(p->log,4,"munki_imp_calibrate: doing calt 0x%x\n",*calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return MUNKI_OK;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return MUNKI_UNSUPPORTED;
	}

	/* Get current sensor position */
	if ((ev = munki_getstatus(p, &spos, NULL)) != MUNKI_OK) {
		return ev;
	}
	a1logd(p->log,4,"munki sensor position = 0x%x\n",spos);

#ifdef NEVER
	/* We can set the *calc to the actual conditions, in which case */
	/* the calibration will commence immediately. */
	if (!m->nosposcheck && spos == mk_spos_calib) {
		*calc = inst_calc_man_cal_smode;
		a1logd(p->log,4,"munki set calc to cal conditions\n",spos);

	}
#endif

	/* Make sure that the instrument configuration matches the */
	/* conditions */
	if (*calc == inst_calc_man_cal_smode) {
		if (!m->nosposcheck && spos != mk_spos_calib) {
			return MUNKI_SPOS_CALIB;
		}
	} else if (*calc == inst_calc_man_trans_white) {
		if (!m->nosposcheck && spos != mk_spos_surf) {
			return MUNKI_SPOS_SURF;
		}
	}

	a1logd(p->log,4,"munki_imp_calibrate has right conditions\n");

	if (*calt & inst_calt_ap_flag) {
		sx1 = 0; sx2 = mk_no_modes;		/* Go through all the modes */
	} else {
		sx1 = m->mmode; sx2 = sx1 + 1;		/* Just current mode */
	}

	/* Go through the modes we are going to cover */
	for (sx = sx1; sx < sx2; sx++) {
		munki_state *s = &m->ms[sx];
		m->mmode = sx;				/* A lot of functions we call rely on this */

		a1logd(p->log,3,"\nCalibrating mode %d\n", s->mode);

		/* Sanity check scan mode settings, in case something strange */
		/* has been restored from the persistence file. */
//		if (s->scan && s->inttime > (2.1 * m->min_int_time)) {
//			s->inttime = m->min_int_time;	/* Maximize scan rate */
//		}
	
		/* We are now either in inst_calc_man_cal_smode, */ 
		/* inst_calc_man_trans_white, inst_calc_disp_white or inst_calc_proj_white */
		/* sequenced in that order, and in the appropriate condition for it. */
	
		/* Fixed int. time black calibration: */
		/* Reflective uses on the fly black, even for adaptive. */
		/* Emiss and trans can use single black ref only for non-adaptive */
		/* using the current inttime & gainmode, while display mode */
		/* does an extra fallback black cal for bright displays. */
		if ((*calt & (inst_calt_ref_dark
		            | inst_calt_em_dark
		            | inst_calt_trans_dark | inst_calt_ap_flag))
		 && *calc == inst_calc_man_cal_smode
		 && ( s->reflective
		  || (s->emiss && !s->adaptive && !s->scan)
		  || (s->trans && !s->adaptive))) {
			int stm;
			int usesdct23 = 0;			/* Is a mode that uses dcaltime2 & 3 */

			if (s->emiss && !s->adaptive && !s->scan)
				usesdct23 = 1;
	
			nummeas = munki_comp_nummeas(p, s->dcaltime, s->inttime);
	
			a1logd(p->log,3,"\nDoing initial black calibration with dcaltime %f, int_time %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode);
			stm = msec_time();
			if ((ev = munki_dark_measure(p, s->dark_data, nummeas, &s->inttime, s->gainmode))
		                                                                         != MUNKI_OK) {
				m->mmode = mmode;           /* Restore actual mode */
				return ev;
			}
			a1logd(p->log,4,"Execution time of dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);
	
			/* Special display mode alternate integration time black measurement */
			if (usesdct23) {
				nummeas = munki_comp_nummeas(p, s->dcaltime2, s->dark_int_time2);
				a1logd(p->log,3,"Doing 2nd initial black calibration with dcaltime2 %f, dark_int_time2 %f, nummeas %d, gainmode %d\n", s->dcaltime2, s->dark_int_time2, nummeas, s->gainmode);
				stm = msec_time();
				if ((ev = munki_dark_measure(p, s->dark_data2, nummeas, &s->dark_int_time2,
				                                                   s->gainmode)) != MUNKI_OK) {
					m->mmode = mmode;           /* Restore actual mode */
					return ev;
				}
				a1logd(p->log,4,"Execution time of 2nd dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);
		
				nummeas = munki_comp_nummeas(p, s->dcaltime3, s->dark_int_time3);
				a1logd(p->log,3,"Doing 3rd initial black calibration with dcaltime3 %f, dark_int_time3 %f, nummeas %d, gainmode %d\n", s->dcaltime3, s->dark_int_time3, nummeas, s->gainmode);
				nummeas = munki_comp_nummeas(p, s->dcaltime3, s->dark_int_time3);
				stm = msec_time();
				if ((ev = munki_dark_measure(p, s->dark_data3, nummeas, &s->dark_int_time3,
					                                                   s->gainmode)) != MUNKI_OK) {
					m->mmode = mmode;           /* Restore actual mode */
					return ev;
				}
				a1logd(p->log,4,"Execution time of 3rd dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);
		
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
			for (i = 0; i < mk_no_modes; i++) {
				munki_state *ss = &m->ms[i];
				if (ss == s || ss->ddate == cdate)
					continue;
				if ( (s->reflective
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
#ifndef NEVER	// ~~99
if (ss->dark_int_time2 != s->dark_int_time2
 || ss->dark_int_time3 != s->dark_int_time3)
	a1logd(p->log,1,"copying cal to mode with different cal/gain mode: %d -> %d\n",s->mode, ss->mode);
#endif
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
		 && *calc == inst_calc_man_cal_smode
		 && (s->emiss && !s->adaptive && s->scan)) {
			int stm;
	
			nummeas = munki_comp_nummeas(p, s->dcaltime, s->inttime);
	
			a1logd(p->log,3,"\nDoing emissive (flash) black calibration with dcaltime %f, int_time %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode);
			stm = msec_time();
			if ((ev = munki_dark_measure(p, s->dark_data, nummeas, &s->inttime, s->gainmode))
		                                                                         != MUNKI_OK) {
				m->mmode = mmode;           /* Restore actual mode */
				return ev;
			}
			a1logd(p->log,4,"Execution time of dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);
	
			s->dark_valid = 1;
			s->want_dcalib = 0;
			s->ddate = cdate;
			s->dark_int_time = s->inttime;
			s->dark_gain_mode = s->gainmode;
			*calt &= ~inst_calt_em_dark;
	
			/* Save the calib to all similar modes */
			for (i = 0; i < mk_no_modes; i++) {
				munki_state *ss = &m->ms[i];
				if (ss == s || ss->ddate == cdate)
					continue;
				if ((ss->emiss && !ss->adaptive && ss->scan)
				 && ss->dark_int_time == s->dark_int_time
				 && ss->dark_gain_mode == s->dark_gain_mode) {
					ss->dark_valid = s->dark_valid;
					ss->want_dcalib = s->want_dcalib;
					ss->ddate = s->ddate;
					ss->dark_int_time = s->dark_int_time;
					ss->dark_gain_mode = s->dark_gain_mode;
					for (k = -1; k < m->nraw; k++) {
						ss->dark_data[k] = s->dark_data[k];
					}
				}
			}
		}
	
		/* Adaptive black calibration: */
		/* Emmissive adaptive and transmissive black reference. */
		/* in non-scan mode, where the integration time and gain may vary. */
		if ((*calt & (inst_calt_ref_dark
		            | inst_calt_em_dark
		            | inst_calt_trans_dark | inst_calt_ap_flag))
		 && *calc == inst_calc_man_cal_smode
		 && ((s->emiss && s->adaptive && !s->scan)
		  || (s->trans && s->adaptive && !s->scan))) {
			/* Adaptive where we can't measure the black reference on the fly, */
			/* so bracket it and interpolate. */
			/* The black reference is probably temperature dependent, but */
			/* there's not much we can do about this. */
	
			s->idark_int_time[0] = m->min_int_time;
			nummeas = munki_comp_nummeas(p, s->dcaltime, s->idark_int_time[0]);
			a1logd(p->log,3,"\nDoing adaptive interpolated black calibration, dcaltime %f, idark_int_time[0] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[0], nummeas, 0);
			if ((ev = munki_dark_measure(p, s->idark_data[0], nummeas, &s->idark_int_time[0], 0))
			                                                                          != MUNKI_OK) {
				m->mmode = mmode;           /* Restore actual mode */
				return ev;
			}
		
			nummeas = munki_comp_nummeas(p, s->dcaltime, s->idark_int_time[1]);
			a1logd(p->log,3,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[1] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[1], nummeas, 0);
			if ((ev = munki_dark_measure(p, s->idark_data[1], nummeas, &s->idark_int_time[1], 0))
			                                                                          != MUNKI_OK) {
				m->mmode = mmode;           /* Restore actual mode */
				return ev;
			}
		
			if (s->auto_gain) {			/* If high gain is permitted */
				s->idark_int_time[2] = m->min_int_time;	/* 0.01 */
				nummeas = munki_comp_nummeas(p, s->dcaltime, s->idark_int_time[2]);
				a1logd(p->log,3,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[2] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[2], nummeas, 1);
				if ((ev = munki_dark_measure(p, s->idark_data[2], nummeas, &s->idark_int_time[2], 1))
				                                                                          != MUNKI_OK) {
					m->mmode = mmode;           /* Restore actual mode */
					return ev;
				}
			
				s->idark_int_time[3] = ADARKINT_MAX; /* 2.0 */
				a1logd(p->log,3,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[3] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[3], nummeas, 1);
				nummeas = munki_comp_nummeas(p, s->dcaltime, s->idark_int_time[3]);
				if ((ev = munki_dark_measure(p, s->idark_data[3], nummeas, &s->idark_int_time[3], 1))
				                                                                          != MUNKI_OK) {
					m->mmode = mmode;           /* Restore actual mode */
					return ev;
				}
			}
		
			munki_prepare_idark(p);
	
			s->idark_valid = 1;
			s->iddate = cdate;
		
			if ((ev = munki_interp_dark(p, s->dark_data, s->inttime, s->gainmode)) != MUNKI_OK) {
				m->mmode = mmode;           /* Restore actual mode */
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
			a1logd(p->log,3,"Saving adaptive black calib to similar modes\n");
			for (i = 0; i < mk_no_modes; i++) {
				munki_state *ss = &m->ms[i];
				if (ss == s || ss->iddate == cdate)
					continue;
				if ((ss->emiss || ss->trans) && ss->adaptive && !ss->scan) {
					ss->idark_valid = s->idark_valid;
					ss->want_dcalib = s->want_dcalib;
					ss->iddate = s->iddate;
					ss->dark_int_time = s->dark_int_time;
					ss->dark_gain_mode = s->dark_gain_mode;
#ifndef NEVER	// ~~99
if (ss->dark_int_time != s->dark_int_time
 || ss->dark_gain_mode != s->dark_gain_mode)
	a1logd(p->log,1,"copying cal to mode with different cal/gain mode: %d -> %d\n",s->mode, ss->mode);
#endif
					for (j = 0; j < (s->auto_gain ? 4 : 2); j++) {
						ss->idark_int_time[j] = s->idark_int_time[j];
#ifndef NEVER	// ~~99
if (ss->idark_int_time[j] != s->idark_int_time[j])
	a1logd(p->log,1,"copying cal to mode with different cal/gain mode: %d -> %d\n",s->mode, ss->mode);
#endif
						for (k = -1; k < m->nraw; k++)
							ss->idark_data[j][k] = s->idark_data[j][k];
					}
				}
			}
	
			a1logd(p->log,3,"Done adaptive interpolated black calibration\n");
	
			/* Test accuracy of dark level interpolation */
#ifdef TEST_DARK_INTERP
			{
				double tinttime;
				double ref[NSEN_MAX], interp[NSEN_MAX];
				
	//			fprintf(stderr,"Normal gain offsets, base:\n");
	//			plot_raw(s->idark_data[0]);
	//			fprintf(stderr,"Normal gain offsets, multiplier:\n");
	//			plot_raw(s->idark_data[1]);
				
#ifdef DUMP_DARKM
				extern int ddumpdarkm;
				ddumpdarkm = 1;
#endif
				for (tinttime = m->min_int_time; ; tinttime *= 2.0) {
					if (tinttime >= m->max_int_time)
						tinttime = m->max_int_time;
	
					nummeas = munki_comp_nummeas(p, s->dcaltime, tinttime);
					if ((ev = munki_dark_measure(p, ref, nummeas, &tinttime, 0)) != MUNKI_OK) {
						m->mmode = mmode;           /* Restore actual mode */
						return ev;
					}
					munki_interp_dark(p, interp, tinttime, 0);
#ifdef DEBUG
					fprintf(stderr,"Normal gain, int time %f:\n",tinttime);
					plot_raw2(ref, interp);
#endif
					if ((tinttime * 1.1) > m->max_int_time)
						break;
				}
#ifdef DUMP_DARKM
				ddumpdarkm = 0;
#endif
	
				if (s->auto_gain) {
//					fprintf(stderr,"High gain offsets, base:\n");
//					plot_raw(s->idark_data[2]);
//					fprintf(stderr,"High gain offsets, multiplier:\n");
//					plot_raw(s->idark_data[3]);
				
					for (tinttime = m->min_int_time; ; tinttime *= 2.0) {
						if (tinttime >= m->max_int_time)
							tinttime = m->max_int_time;
		
						nummeas = munki_comp_nummeas(p, s->dcaltime, tinttime);
						if ((ev = munki_dark_measure(p, ref, nummeas, &tinttime, 1)) != MUNKI_OK) {
							m->mmode = mmode;           /* Restore actual mode */
							return ev;
						}
						munki_interp_dark(p, interp, tinttime, 1);
#ifdef DEBUG
						printf("High gain, int time %f:\n",tinttime);
						plot_raw2(ref, interp);
#endif
						if ((tinttime * 1.1) > m->max_int_time)
							break;
					}
			}
			}
#endif	/* TEST_DARK_INTERP */
	
		}
	
		/* Deal with an emissive/transmisive adaptive black reference */
		/* when in scan mode. */
		if ((*calt & (inst_calt_ref_dark
		            | inst_calt_em_dark
		            | inst_calt_trans_dark | inst_calt_ap_flag))
		 && *calc == inst_calc_man_cal_smode
		 && ((s->emiss && s->adaptive && s->scan)
		  || (s->trans && s->adaptive && s->scan))) {
			int j;
			/* We know scan is locked to the minimum integration time, */
			/* so we can measure the dark data at that integration time, */
			/* but we don't know what gain mode will be used, so measure both, */
			/* and choose the appropriate one on the fly. */
		
			s->idark_int_time[0] = s->inttime;
			nummeas = munki_comp_nummeas(p, s->dcaltime, s->idark_int_time[0]);
			a1logd(p->log,3,"\nDoing adaptive scan black calibration, dcaltime %f, idark_int_time[0] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[0], nummeas, s->gainmode);
			if ((ev = munki_dark_measure(p, s->idark_data[0], nummeas, &s->idark_int_time[0], 0))
			                                                                          != MUNKI_OK) {
				m->mmode = mmode;           /* Restore actual mode */
				return ev;
			}
		
			if (s->auto_gain) {
				s->idark_int_time[2] = s->inttime;
				nummeas = munki_comp_nummeas(p, s->dcaltime, s->idark_int_time[2]);
				a1logd(p->log,3,"Doing adaptive scan black calibration, dcaltime %f, idark_int_time[2] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[2], nummeas, s->gainmode);
				if ((ev = munki_dark_measure(p, s->idark_data[2], nummeas, &s->idark_int_time[2], 1))
				                                                                          != MUNKI_OK) {
					m->mmode = mmode;           /* Restore actual mode */
					return ev;
				}
			}
		
			s->idark_valid = 1;
			s->iddate = cdate;
	
			if (s->auto_gain && s->gainmode) {
				for (j = -1; j < m->nraw; j++)
					s->dark_data[j] = s->idark_data[2][j];
			} else {
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
	
			a1logd(p->log,3,"Done adaptive scan black calibration\n");
	
			/* Save the calib to all similar modes */
			/* We're assuming they have the same int times */
			a1logd(p->log,3,"Saving adaptive scan black calib to similar modes\n");
			for (i = 0; i < mk_no_modes; i++) {
				munki_state *ss = &m->ms[i];
				if (ss == s || s->iddate == cdate)
					continue;
				if ((ss->emiss || ss->trans) && ss->adaptive && s->scan) {
					ss->idark_valid = s->idark_valid;
					ss->want_dcalib = s->want_dcalib;
					ss->iddate = s->iddate;
					ss->dark_int_time = s->dark_int_time;
					ss->dark_gain_mode = s->dark_gain_mode;

					for (j = 0; j < (s->auto_gain ? 4 : 2); j += 2) {
						ss->idark_int_time[j] = s->idark_int_time[j];
						for (k = -1; k < m->nraw; k++)
							ss->idark_data[j][k] = s->idark_data[j][k];
					}
				}
			}
		}
	
		/* Now deal with white calibrations */
	
		/* If we are doing a reflective white reference calibrate */
		/* or a we are doing a tranmisive white reference calibrate */
		if ((*calt & (inst_calt_ref_white
		            | inst_calt_trans_vwhite | inst_calt_ap_flag))
		 && ((*calc == inst_calc_man_cal_smode && s->reflective) 
		  || (*calc == inst_calc_man_trans_white && s->trans))) {
//		 && s->cfdate < cdate)
			double dead_time = 0.0;		/* Dead integration time */
			double scale;
			int i;
			double ulimit = m->optsval / m->minsval;	/* Upper scale needed limit */
			double fulimit = sqrt(ulimit);				/* Fast exit limit */
			double llimit = m->optsval / m->maxsval;	/* Lower scale needed limit */
			double fllimit = sqrt(llimit);				/* Fast exit limit */
	
			a1logd(p->log,3,"\nDoing initial white calibration with current inttime %f, gainmode %d\n",
			                                                   s->inttime, s->gainmode);
			a1logd(p->log,3,"ulimit %f, llimit %f\n",ulimit,llimit);
			a1logd(p->log,3,"fulimit %f, fllimit %f\n",fulimit,fllimit);
			if (s->reflective) {
				dead_time = RDEAD_TIME;		/* Fudge value that makes int time calcs work */
				/* Heat up the LED to put in in a nominal state for int time adjustment */
				munki_heatLED(p, m->ledpreheattime);
			}
			
			/* Until we're done */
			for (i = 0; i < 6; i++) {
	
				a1logd(p->log,3,"Doing a white calibration with trial int_time %f, gainmode %d\n",
				                                                 s->inttime,s->gainmode);
	
				if (s->trans && s->adaptive) {
					/* compute interpolated dark refence for chosen inttime & gainmode */
					a1logd(p->log,3,"Interpolate dark calibration reference\n");
					if ((ev = munki_interp_dark(p, s->dark_data, s->inttime, s->gainmode))
					                                                             != MUNKI_OK) {
						m->mmode = mmode;           /* Restore actual mode */
						return ev;
					}
					s->dark_valid = 1;
					s->ddate = s->iddate;
					s->dark_int_time = s->inttime;
					s->dark_gain_mode = s->gainmode;
				}
				nummeas = munki_comp_nummeas(p, s->wcaltime, s->inttime);
				ev = munki_whitemeasure(p, s->white_data, &scale, nummeas, &s->inttime, s->gainmode,
				                                                                      s->targoscale);
				a1logd(p->log,3,"Needed scale is %f\n",scale);
	
				if (ev == MUNKI_RD_SENSORSATURATED) {
					scale = 0.0;			/* Signal it this way */
					ev = MUNKI_OK;
				}
				if (ev != MUNKI_OK) {
					m->mmode = mmode;           /* Restore actual mode */
					return ev;
				}
	
				if (scale >= fllimit && scale <= fulimit) {
					a1logd(p->log,3,"Close enough for early exit\n");
					break;			/* OK, we can stop straight away */
				}
		
				if (scale == 0.0) {		/* If sensor was saturated */
					s->inttime = m->min_int_time;
					s->gainmode = 0;
					s->dark_valid = 0;
				} else {
					double ninttime;
	
					/* Compute a new integration time and gain mode */
					/* in order to optimise the sensor values. Error if can't get */
					/* scale we want. */
					if ((ev = munki_optimise_sensor(p, &ninttime, &s->gainmode, s->inttime,
					    s->gainmode, s->trans, 0, &s->targoscale, scale, dead_time)) != MUNKI_OK) {
						m->mmode = mmode;           /* Restore actual mode */
						return ev;
					}
					s->inttime = ninttime;
					a1logd(p->log,3,"New inttime = %f\n",s->inttime);
				}
			}
			if (i >= 6) {
				if (scale == 0.0) {		/* If sensor was saturated */
					a1logd(p->log,1, "White calibration failed - sensor is saturated\n");
					m->mmode = mmode;           /* Restore actual mode */
					return MUNKI_RD_SENSORSATURATED;
				}
				if (scale > ulimit || scale < llimit) {
					a1logd(p->log,1,"White calibration failed - didn't converge (%f %f %f)\n",llimit,scale,ulimit);
					m->mmode = mmode;           /* Restore actual mode */
					return MUNKI_RD_REFWHITENOCONV; 
				}
			}
	
			/* We've settled on the inttime and gain mode to get a good white reference. */
			if (s->reflective) {	/* We read the write reference - check it */
	
				/* Let the LED cool down */
				a1logd(p->log,3,"Waiting %f secs for LED to cool\n",m->ledwaittime);
				msec_sleep((int)(m->ledwaittime * 1000.0 + 0.5));
	
				/* Re-calibrate the black with the given integration time */
				nummeas = munki_comp_nummeas(p, s->dcaltime, s->inttime);
	
				a1logd(p->log,3,"Doing another reflective black calibration with dcaltime %f, int_time %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode);
				if ((ev = munki_dark_measure(p, s->dark_data, nummeas, &s->inttime, s->gainmode))
			                                                                         != MUNKI_OK) {
					m->mmode = mmode;           /* Restore actual mode */
					return ev;
				}
	
				/* Take a reflective white reference measurement, */
				/* subtracts black and decompose into base + LED temperature components, */
				/* and compute reftemp white reference. */
				nummeas = munki_comp_nummeas(p, m->calscantime, s->inttime);
				if ((ev = munki_ledtemp_whitemeasure(p, s->white_data, s->iwhite_data, &s->reftemp,
				                                  nummeas, s->inttime, s->gainmode)) != MUNKI_OK) {
					m->mmode = mmode;           /* Restore actual mode */
					return ev;
				}
	
				/* Compute wavelength white readings from ref temp sensor reading */
				if ((ev = munki_compute_wav_whitemeas(p, s->cal_factor1, s->cal_factor2,
				                                             s->white_data)) != MUNKI_OK) {
					m->mmode = mmode;           /* Restore actual mode */
					return ev;
				}
	
				/* We don't seem to sanity check the white reference. Presumably */
				/* this is because a LED isn't going to burn out... */
	
				/* Compute a calibration factor given the reading of the white reference. */
				munki_compute_white_cal(p, s->cal_factor1, m->white_ref1, s->cal_factor1,
				                           s->cal_factor2, m->white_ref2, s->cal_factor2);
	
			} else {
				/* Compute wavelength white readings from sensor */
				if ((ev = munki_compute_wav_whitemeas(p, s->cal_factor1, s->cal_factor2,
				                                             s->white_data)) != MUNKI_OK) {
					m->mmode = mmode;           /* Restore actual mode */
					return ev;
				}
	
				/* Compute a calibration factor given the reading of the white reference. */
				m->transwarn |= munki_compute_white_cal(p, s->cal_factor1, NULL, s->cal_factor1,
				                                       s->cal_factor2, NULL, s->cal_factor2);
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
//		 && s->cfdate < cdate
		 && (s->emiss && !s->adaptive && !s->scan)) {
			double scale;
			double *data;
			double *tt, tv;
	
			data = dvectorz(-1, m->nraw-1);
	
			a1logd(p->log,3,"\nDoing display integration time calibration\n");
	
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
			nummeas = munki_comp_nummeas(p, s->wreadtime, s->inttime);
			ev = munki_whitemeasure(p, data , &scale, nummeas,
			           &s->inttime, s->gainmode, s->targoscale);
			/* Switch to the alternate if things are too bright */
			/* We do this simply by swapping the alternate values in. */
			if (ev == MUNKI_RD_SENSORSATURATED || scale < 1.0) {
				a1logd(p->log,3,"Switching to alternate display integration time %f seconds\n",s->dark_int_time2);
				tv = s->inttime; s->inttime = s->dark_int_time2; s->dark_int_time2 = tv;
				tt = s->dark_data; s->dark_data = s->dark_data2; s->dark_data2 = tt;
				s->dispswap = 1;
	
				/* Do another measurement of the full display white, and if it's close to */
				/* saturation, switch to the 3rd alternate display integration time */
				nummeas = munki_comp_nummeas(p, s->wreadtime, s->inttime);
				ev = munki_whitemeasure(p, data , &scale, nummeas,
				           &s->inttime, s->gainmode, s->targoscale);
				/* Switch to the 3rd alternate if things are too bright */
				/* We do this simply by swapping the alternate values in. */
				if (ev == MUNKI_RD_SENSORSATURATED || scale < 1.0) {
					a1logd(p->log,3,"Switching to 3rd alternate display integration time %f seconds\n",s->dark_int_time3);
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
			if (ev != MUNKI_OK) {
				m->mmode = mmode;           /* Restore actual mode */
				return ev;
			}
			s->done_dintsel = 1;
			s->diseldate = cdate;
			*calt &= ~inst_calt_emis_int_time;
	
			a1logd(p->log,3,"Done display integration time selection\n");
		}

	}	/* Look at next mode */
	m->mmode = mmode;           /* Restore actual mode */

	/* Make sure there's the right condition for the calibration */
	if (*calt & (inst_calt_ref_dark | inst_calt_ref_white)) {	/* Reflective calib */
		if (*calc != inst_calc_man_cal_smode) {
			*calc = inst_calc_man_cal_smode;
			return MUNKI_CAL_SETUP;
		}
	} else if (*calt & inst_calt_em_dark) {		/* Emissive Dark calib */
		id[0] = '\000';
		if (*calc != inst_calc_man_cal_smode) {
			*calc = inst_calc_man_cal_smode;
			return MUNKI_CAL_SETUP;
		}
	} else if (*calt & inst_calt_trans_dark) {	/* Transmissive dark */
		id[0] = '\000';
		if (*calc != inst_calc_man_cal_smode) {
			*calc = inst_calc_man_cal_smode;
			return MUNKI_CAL_SETUP;
		}
	} else if (*calt & inst_calt_trans_vwhite) {	/* Transmissive white for emulated trans. */
		id[0] = '\000';
		if (*calc != inst_calc_man_trans_white) {
			*calc = inst_calc_man_trans_white;
			return MUNKI_CAL_SETUP;
		}
	} else if (*calt & inst_calt_emis_int_time) {
		id[0] = '\000';
		if (*calc != inst_calc_emis_white) {
			*calc = inst_calc_emis_white;
			return MUNKI_CAL_SETUP;
		}
	}

	/* Go around again if we've still got calibrations to do */
	if (*calt & inst_calt_all_mask) {
		return MUNKI_CAL_SETUP;
	}

	/* We must be done */

#ifdef ENABLE_NONVCAL
	/* Save the calibration to a file */
	munki_save_calibration(p);
#endif
	
	if (m->transwarn) {
		*calc = inst_calc_message;
		if (m->transwarn & 2)
			strcpy(id, "Warning: Transmission light source is too low for accuracy!");
		else
			strcpy(id, "Warning: Transmission light source is low at some wavelengths!");
		m->transwarn = 0;
		return MUNKI_OK;
	}

	a1logd(p->log,3,"Finished cal with dark_valid = %d, cal_valid = %d\n",cs->dark_valid, cs->cal_valid);

	return ev; 
}

/* Interpret an icoms error into a MUNKI error */
int icoms2munki_err(int se) {
	if (se != ICOM_OK)
		return MUNKI_COMS_FAIL;
	return MUNKI_OK;
}

/* - - - - - - - - - - - - - - - - */
/* Measure a display update delay. It is assumed that a */
/* white to black change has been made to the displayed color, */
/* and this will measure the time it took for the update to */
/* be noticed by the instrument, up to 0.6 seconds. */
/* inst_misread will be returned on failure to find a transition to black. */
#define NDMXTIME 0.7		/* Maximum time to take */
#define NDSAMPS 500			/* Debug samples */

typedef struct {
	double sec;
	double rgb[3];
	double tot;
} i1rgbdsamp;

munki_code munki_imp_meas_delay(
munki *p,
int *msecdelay)	{	/* Return the number of msec */
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	int i, j, k, mm;
	double **multimeas;			/* Spectral measurements */
	int nummeas;
	double rgbw[3] = { 610.0, 520.0, 460.0 };
	double ucalf = 1.0;				/* usec_time calibration factor */
	double inttime;
	i1rgbdsamp *samp;
	double stot, etot, del, thr;
	double etime;
	int isdeb;

	/* Read the samples */
	inttime = m->min_int_time;
	nummeas = (int)(NDMXTIME/inttime + 0.5);
	multimeas = dmatrix(0, nummeas-1, -1, m->nwav-1);
	if ((samp = (i1rgbdsamp *)calloc(sizeof(i1rgbdsamp), nummeas)) == NULL) {
		a1logd(p->log, 1, "munki_meas_delay: malloc failed\n");
		return MUNKI_INT_MALLOC;
	}

//printf("~1 %d samples at %f int\n",nummeas,inttime);
	if ((ev = munki_read_patches_all(p, multimeas, nummeas, &inttime, 0)) != inst_ok) {
		free_dmatrix(multimeas, 0, nummeas-1, 0, m->nwav-1);
		free(samp);
		return ev;
	} 

	/* Convert the samples to RGB */
	for (i = 0; i < nummeas; i++) {
		samp[i].sec = i * inttime;
		samp[i].rgb[0] = samp[i].rgb[1] = samp[i].rgb[2] = 0.0;
		for (j = 0; j < m->nwav; j++) {
			double wl = XSPECT_WL(m->wl_short, m->wl_long, m->nwav, j);

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
	free_dmatrix(multimeas, 0, nummeas-1, 0, m->nwav-1);

	a1logd(p->log, 3, "munki_measure_refresh: Read %d samples for refresh calibration\n",nummeas);

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

	/* Over the first 100msec, locate the maximum value */
	etime = samp[nummeas-1].sec;
	stot = -1e9;
	for (i = 0; i < nummeas; i++) {
		if (samp[i].tot > stot)
			stot = samp[i].tot;
		if (samp[i].sec > 0.1)
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

	del = stot - etot;
	thr = etot + 0.30 * del;		/* 30% of transition threshold */

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "munki_meas_delay: start tot %f end tot %f del %f, thr %f\n", stot, etot, del, thr);
#endif

	/* Check that there has been a transition */
	if (del < 5.0) {
		free(samp);
		a1logd(p->log, 1, "munki_meas_delay: can't detect change from white to black\n");
		return MUNKI_RD_NOTRANS_FOUND; 
	}

	/* Locate the time at which the values are above the end values */
	for (i = nummeas-1; i >= 0; i--) {
		if (samp[i].tot > thr)
			break;
	}
	if (i < 0)		/* Assume the update was so fast that we missed it */
		i = 0;

	a1logd(p->log, 2, "munki_meas_delay: stoped at sample %d time %f\n",i,samp[i].sec);

	*msecdelay = (int)(samp[i].sec * 1000.0 + 0.5);

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "munki_meas_delay: returning %d msec\n",*msecdelay);
#endif
	free(samp);

	return MUNKI_OK;
}
#undef NDSAMPS
#undef NDMXTIME


/* - - - - - - - - - - - - - - - - */
/* Measure a patch or strip or flash in the current mode. */
/* To try and speed up the reaction time between */
/* triggering a scan measurement and being able to */
/* start moving the instrument, we pre-allocate */
/* all the buffers and arrays, and pospone processing */
/* until after the scan is complete. */
munki_code munki_imp_measure(
	munki *p,
	ipatch *vals,		/* Pointer to array of instrument patch value */
	int nvals,			/* Number of values */	
	instClamping clamp	/* Clamp XYZ/Lab to be +ve */
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	unsigned char *buf = NULL;	/* Raw USB reading buffer for reflection dark cal */
	unsigned int bsize;
	unsigned char *mbuf = NULL;	/* Raw USB reading buffer for measurement */
	unsigned int mbsize;
	int nummeas = 0, maxnummeas = 0;
	int nmeasuered = 0;			/* Number actually measured */
	double invsampt = 0.0;		/* Invalid sample time */			
	int ninvmeas = 0;			/* Number of invalid measurements */
	double **specrd = NULL;		/* Cooked spectral patch values */
	double duration = 0.0;		/* Possible flash duration value */
	mk_spos spos;
	int user_trig = 0;

	a1logd(p->log,2,"munki_imp_measure called\n");
	a1logd(p->log,3,"Taking %d measurments in %s%s%s%s%s mode called\n", nvals,
		        s->emiss ? "Emission" : s->trans ? "Trans" : "Refl", 
		        s->emiss && s->ambient ? " Ambient" : "",
		        s->scan ? " Scan" : "",
		        s->flash ? " Flash" : "",
		        s->adaptive ? " Adaptive" : "");


	if ((s->emiss && s->adaptive && !s->idark_valid)
	 || ((!s->emiss || !s->adaptive) && !s->dark_valid)
	 || !s->cal_valid) {
		a1logd(p->log,3,"emis %d, adaptive %d, idark_valid %d\n",s->emiss,s->adaptive,s->idark_valid);
		a1logd(p->log,3,"dark_valid %d, cal_valid %d\n",s->dark_valid,s->cal_valid);
		a1logd(p->log,3,"munki_imp_measure need calibration\n");
		return MUNKI_RD_NEEDS_CAL;
	}
		
	if (nvals <= 0
	 || (!s->scan && nvals > 1)) {
		a1logd(p->log,3,"munki_imp_measure wrong number of patches\n");
		return MUNKI_INT_WRONGPATCHES;
	}

	if (s->reflective) {
		/* Number of invalid samples to allow for LED warmup */
		invsampt = m->refinvalidsampt;
		ninvmeas = munki_comp_ru_nummeas(p, invsampt, s->inttime);
	}

	/* Notional number of measurements, befor adaptive and not counting scan */
	nummeas = munki_comp_nummeas(p, s->wreadtime, s->inttime);

	/* Allocate buffer for dark measurement */
	if (s->reflective) {
		bsize = m->nsen * 2 * nummeas;
		if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
			a1logd(p->log,1,"munki_imp_measure malloc %d bytes failed (5)\n",bsize);
			return MUNKI_INT_MALLOC;
		}
	}

	/* Allocate buffer for measurement */
	maxnummeas = munki_comp_nummeas(p, s->maxscantime, s->inttime);
	if (maxnummeas < (ninvmeas + nummeas))
		maxnummeas = (ninvmeas + nummeas);
	mbsize = m->nsen * 2 * maxnummeas;
	if ((mbuf = (unsigned char *)malloc(sizeof(unsigned char) * mbsize)) == NULL) {
		if (buf != NULL)
			free(buf);
		a1logd(p->log,1,"munki_imp_measure malloc %d bytes failed (6)\n",mbsize);
		return MUNKI_INT_MALLOC;
	}
	specrd = dmatrix(0, nvals-1, 0, m->nwav-1);

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
						ev = MUNKI_USER_ABORT;
						break;						/* Abort */
					}
					if (!s->scan && rc == inst_user_trig) {
						ev = MUNKI_USER_TRIG;
						user_trig = 1;
						break;						/* Trigger */
					}
				}
				msec_sleep(100);
			}
		}
#else
		/* Throw one away in case the switch was pressed prematurely */
		munki_waitfor_switch_th(p, NULL, NULL, 0.01);

		for (;;) {
			mk_eve ecode;
			int cerr;

			if ((ev = munki_waitfor_switch_th(p, &ecode, NULL, 0.1)) != MUNKI_OK
			 && ev != MUNKI_INT_BUTTONTIMEOUT)
				break;			/* Error */

			if (ev == MUNKI_OK && ecode == mk_eve_switch_press)
				break;			/* switch triggered */

			/* Don't trigger on user key if scan, only trigger */
			/* on instrument switch */
			if (p->uicallback != NULL
			  && (rc = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
				if (rc == inst_user_abort) {
					ev = MUNKI_USER_ABORT;
					break;						/* Abort */
				}
				if (!s->scan && rc == inst_user_trig) {
					ev = MUNKI_USER_TRIG;
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
			ev = MUNKI_UNSUPPORTED;

		} else {

			for (;;) {
				inst_code rc;
				if ((rc = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
					if (rc == inst_user_abort) {
						ev = MUNKI_USER_ABORT;	/* Abort */
						break;
					}
					if (rc == inst_user_trig) {
						ev = MUNKI_USER_TRIG;
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
			ev = MUNKI_USER_ABORT;	/* Abort */
	}

	if (ev != MUNKI_OK && ev != MUNKI_USER_TRIG) {
		free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
		free(mbuf);
		if (buf != NULL)
			free(buf);
		a1logd(p->log,3,"munki_imp_measure user aborted, terminated, command, or failure\n");
		return ev;		/* User abort, term, command or failure */
	}

	/* Get current sensor position */
	if ((ev = munki_getstatus(p, &spos, NULL)) != MUNKI_OK) {
		free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
		free(mbuf);
		if (buf != NULL)
			free(buf);
		a1logd(p->log,3,"munki_imp_measure getstatus failed\n");
		return ev;
	}

	/* Check the current sensor position */
	if (!m->nosposcheck) {
		if (s->emiss) {
			if (s->ambient) {
				if (spos != mk_spos_amb)
					ev = MUNKI_SPOS_AMB;
			} else if (s->projector) {
				if (spos != mk_spos_proj)
					ev = MUNKI_SPOS_PROJ;
			} else {	/* Display */
				if (spos != mk_spos_surf)
					ev = MUNKI_SPOS_SURF;
			}
		} else {	/* Reflective or transmissive */
			if (spos != mk_spos_surf)
				ev = MUNKI_SPOS_SURF;
		}
		if (ev != MUNKI_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			if (buf != NULL)
				free(buf);
			a1logd(p->log,3,"munki_imp_measure: Sensor in wrong position\n");
			return ev;
		}
	}

	/* Emissive adaptive, non-scan */
	if (s->emiss && !s->scan && s->adaptive) {
		int saturated = 0;
		double optscale = 1.0;
		s->inttime = 0.25;
		s->gainmode = 0;
		s->dark_valid = 0;

		a1logd(p->log,3,"Trial measure emission with inttime %f, gainmode %d\n",s->inttime,s->gainmode);

		/* Take a trial measurement reading using the current mode. */
		/* Used to determine if sensor is saturated, or not optimal */
		nummeas = munki_comp_nummeas(p, s->wreadtime, s->inttime);
		if ((ev = munki_trialmeasure(p, &saturated, &optscale, nummeas, &s->inttime, s->gainmode,
		                                                            s->targoscale)) != MUNKI_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			a1logd(p->log,3,"munki_imp_measure trial measure failed\n");
			return ev;  
		}

		if (saturated) {
			s->inttime = m->min_int_time;

			a1logd(p->log,3,"2nd trial measure emission with inttime %f, gainmode %d\n",
			                                         s->inttime,s->gainmode);
			/* Take a trial measurement reading using the current mode. */
			/* Used to determine if sensor is saturated, or not optimal */
			nummeas = munki_comp_nummeas(p, s->wreadtime, s->inttime);
			if ((ev = munki_trialmeasure(p, &saturated, &optscale, nummeas, &s->inttime, 
			                                          s->gainmode, s->targoscale)) != MUNKI_OK) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
				free(mbuf);
				a1logd(p->log,3,"munki_imp_measure trial measure failed\n");
				return ev;  
			}
		}

		a1logd(p->log,3,"Compute optimal integration time\n");
		/* For adaptive mode, compute a new integration time and gain mode */
		/* in order to optimise the sensor values. */
		if ((ev = munki_optimise_sensor(p, &s->inttime, &s->gainmode, 
		         s->inttime, s->gainmode, 1, 1, &s->targoscale, optscale, 0.0)) != MUNKI_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			a1logd(p->log,3,"munki_imp_measure optimise sensor failed\n");
			return ev;
		}
		a1logd(p->log,3,"Computed optimal emiss inttime %f and gainmode %d\n",s->inttime,s->gainmode);

		a1logd(p->log,3,"Interpolate dark calibration reference\n");
		if ((ev = munki_interp_dark(p, s->dark_data, s->inttime, s->gainmode)) != MUNKI_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			a1logd(p->log,3,"munki_imp_measure interplate dark ref failed\n");
			return ev;
		}
		s->dark_valid = 1;
		s->dark_int_time = s->inttime;
		s->dark_gain_mode = s->gainmode;

		/* Recompute number of measurements and realloc measurement buffer */
		free(mbuf);
		nummeas = munki_comp_nummeas(p, s->wreadtime, s->inttime);
		maxnummeas = munki_comp_nummeas(p, s->maxscantime, s->inttime);
		if (maxnummeas < nummeas)
			maxnummeas = nummeas;
		mbsize = m->nsen * 2 * maxnummeas;
		if ((mbuf = (unsigned char *)malloc(sizeof(unsigned char) * mbsize)) == NULL) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			a1logd(p->log,1,"munki_imp_measure malloc %d bytes failed (7)\n",mbsize);
			return MUNKI_INT_MALLOC;
		}

	} else if (s->reflective) {

		DISDPLOT

		a1logd(p->log,3,"Doing on the fly black calibration_1 with nummeas %d int_time %f, gainmode %d\n",
		                                                   nummeas, s->inttime, s->gainmode);

		if ((ev = munki_dark_measure_1(p, nummeas, &s->inttime, s->gainmode, buf, bsize))
		                                                                           != MUNKI_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(buf);
			free(mbuf);
			a1logd(p->log,3,"munki_imp_measure dak measure 1 failed\n");
			return ev;
		}

		ENDPLOT
	}
	/* Take a measurement reading using the current mode. */
	/* Converts to completely processed output readings. */

	a1logd(p->log,3,"Do main measurement reading\n");

	/* Indicate to the user that they can now scan the instrument, */
	/* after a little delay that allows for the instrument reaction time. */
	if (s->scan) {
		/* 100msec delay, 1KHz for 200 msec */
		msec_beep(0 + (int)(invsampt * 1000.0 + 0.9), 1000, 200);
	}

	/* Retry loop in case a display read is saturated */
	for (;;) {

		/* Trigger measure and gather raw readings */
		if ((ev = munki_read_patches_1(p, ninvmeas, nummeas, maxnummeas, &s->inttime, s->gainmode,
		                                       &nmeasuered, mbuf, mbsize)) != MUNKI_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			if (buf != NULL)
				free(buf);
			free(mbuf);
			a1logd(p->log,3,"munki_imp_measure failed at munki_read_patches_1\n");
			return ev;
		}

		/* Complete processing of dark readings now that main measurement has been taken */
		if (s->reflective) {
			a1logd(p->log,3,"Calling black calibration_2 calc with nummeas %d, inttime %f, gainmode %d\n", nummeas, s->inttime,s->gainmode);
			if ((ev = munki_dark_measure_2(p, s->dark_data, nummeas, s->inttime,
			                                  s->gainmode, buf, bsize)) != MUNKI_OK) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
				free(buf);
				free(mbuf);
				a1logd(p->log,3,"munki_imp_measure failed at munki_dark_measure_2\n");
				return ev;
			}
			s->dark_valid = 1;
			s->dark_int_time = s->inttime;
			s->dark_gain_mode = s->gainmode;
			free(buf);
		}

		/* Process the raw measurement readings into final spectral readings */
		ev = munki_read_patches_2(p, &duration, specrd, nvals, s->inttime, s->gainmode,
		                                              ninvmeas, nmeasuered, mbuf, mbsize);
		/* Special case display mode read. If the sensor is saturated, and */
		/* we haven't already done so, switch to the alternate integration time */
		/* and try again. */
		if (s->emiss && !s->scan && !s->adaptive
		 && ev == MUNKI_RD_SENSORSATURATED
		 && s->dispswap < 2) {
			double *tt, tv;

			if (s->dispswap == 0) {
				a1logd(p->log,3,"Switching to alternate display integration time %f seconds\n",s->dark_int_time2);
				tv = s->inttime; s->inttime = s->dark_int_time2; s->dark_int_time2 = tv;
				tt = s->dark_data; s->dark_data = s->dark_data2; s->dark_data2 = tt;
				s->dispswap = 1;
			} else if (s->dispswap == 1) {
				a1logd(p->log,3,"Switching to 2nd alternate display integration time %f seconds\n",s->dark_int_time3);
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
			nummeas = munki_comp_nummeas(p, s->wreadtime, s->inttime);
			maxnummeas = munki_comp_nummeas(p, s->maxscantime, s->inttime);
			if (maxnummeas < nummeas)
				maxnummeas = nummeas;
			mbsize = m->nsen * 2 * maxnummeas;
			if ((mbuf = (unsigned char *)malloc(sizeof(unsigned char) * mbsize)) == NULL) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
				a1logd(p->log,1,"munki_imp_measure malloc %d bytes failed (7)\n",mbsize);
				return MUNKI_INT_MALLOC;
			}
			continue;			/* Do the measurement again */
		}

		if (ev != MUNKI_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			a1logd(p->log,3,"munki_imp_measure failed at munki_read_patches_2\n");
			return ev;
		}
		break;		/* Don't repeat */
	}
	free(mbuf);

	/* Transfer spectral and convert to XYZ */
	if ((ev = munki_conv2XYZ(p, vals, nvals, specrd, clamp)) != MUNKI_OK) {
		free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
		a1logd(p->log,3,"munki_imp_measure failed at munki_conv2XYZ\n");
		return ev;
	}
	free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);

	if (nvals > 0)
		vals[0].duration = duration;	/* Possible flash duration */
	
	a1logd(p->log,3,"munki_imp_measure sucessful return\n");
	if (user_trig)
		return MUNKI_USER_TRIG;
	return ev; 
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
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

munki_code munki_measure_rgb(munki *p, double *inttime, double *rgb);

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

munki_code munki_imp_meas_refrate(
	munki *p,
	double *ref_rate
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	int i, j, k, mm;
	double **multimeas;			/* Spectral measurements */
	int nummeas;
	double rgbw[3] = { 610.0, 520.0, 460.0 };
	double ucalf = 1.0;				/* usec_time calibration factor */
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

	a1logd(p->log,2,"munki_imp_meas_refrate called\n");

	if (!s->emiss) {
		a1logd(p->log,2,"munki_imp_meas_refrate not in emissive mode\n");
		return MUNKI_UNSUPPORTED;
	}

	for (mm = 0; mm < TRIES; mm++) {
		rfreq[mm] = 0.0;
		npeaks = 0;			/* Number of peaks */
		nummeas = NFSAMPS;
		multimeas = dmatrix(0, nummeas-1, -1, m->nwav-1);

		if (mm == 0)
			inttime = m->min_int_time;
		else {
			double rval, dmm;
			randn = PSRAND32L(randn);
			rval = (double)randn/4294967295.0;
			dmm = ((double)mm + rval - 0.5)/(TRIES - 0.5);
			inttime = m->min_int_time * (1.0 + dmm * 0.80);
		}

		if ((ev = munki_read_patches_all(p, multimeas, nummeas, &inttime, 0)) != inst_ok) {
			free_dmatrix(multimeas, 0, nummeas-1, 0, m->nwav-1);
			return ev;
		} 

		rsamp[tix] = 1.0/inttime;

		/* Convert the samples to RGB */
		for (i = 0; i < nummeas && i < NFSAMPS; i++) {
			samp[i].sec = i * inttime;
			samp[i].rgb[0] = samp[i].rgb[1] = samp[i].rgb[2] = 0.0;
			for (j = 0; j < m->nwav; j++) {
				double wl = XSPECT_WL(m->wl_short, m->wl_long, m->nwav, j);

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
		nfsamps = i;

		a1logd(p->log, 3, "munki_measure_refresh: Read %d samples for refresh calibration\n",nfsamps);

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
			samp[i].sec *= ucalf;
			if (samp[i].sec > maxt)
				maxt = samp[i].sec;
			for (j = 0; j < 3; j++) {
				samp[i].rgb[j] -= minv[j];
			}
		}

#ifdef FREQ_SLOW_PRECISE	/* Interpolate then autocorrelate */

		/* Create PBPMS bins and interpolate readings into them */
		nbins = 1 + (int)(maxt * 1000.0 * PBPMS + 0.5);
		for (j = 0; j < 3; j++) {
			if ((bins[j] = (double *)calloc(sizeof(double), nbins)) == NULL) {
				a1loge(p->log, inst_internal_error, "munki_measure_refresh: malloc failed\n");
				return MUNKI_INT_MALLOC;
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
				a1loge(p->log, inst_internal_error, "munki_measure_refresh: malloc failed\n");
				for (j = 0; j < 3; j++)
					free(bins[j]);
				return MUNKI_INT_MALLOC;
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
			error("munki: Not enough space for lanczos 2 filter");
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
				return MUNKI_OK;
			}
		}
	} else {
		a1logd(p->log, 3, "Not enough tries suceeded to determine refresh rate\n");
	}

	if (ref_rate != NULL)
		*ref_rate = 0.0;

	return MUNKI_RD_NOREFR_FOUND; 
}
#undef NFSAMPS 
#undef PBPMS
#undef PERMIN
#undef PERMAX
#undef NPER
#undef PWIDTH

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Save the calibration for all modes, stored on local system */

#ifdef ENABLE_NONVCAL

/* non-volatile save/restor state to/from a file */
typedef struct {
	int ef;					/* Error flag, 1 = write failed, 2 = close failed */
	unsigned int chsum;		/* Checksum */
} mknonv;

static void update_chsum(mknonv *x, unsigned char *p, int nn) {
	int i;
	for (i = 0; i < nn; i++, p++)
		x->chsum = ((x->chsum << 13) | (x->chsum >> (32-13))) + *p;
}

/* Write an array of chars to the file. Set the error flag to nz on error */
static void write_chars(mknonv *x, FILE *fp, char *dp, int n) {

	if (fwrite((void *)dp, sizeof(char), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(char));
	}
}

/* Write an array of ints to the file. Set the error flag to nz on error */
static void write_ints(mknonv *x, FILE *fp, int *dp, int n) {

	if (fwrite((void *)dp, sizeof(int), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(int));
	}
}

/* Write an array of doubles to the file. Set the error flag to nz on error */
static void write_doubles(mknonv *x, FILE *fp, double *dp, int n) {

	if (fwrite((void *)dp, sizeof(double), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(double));
	}
}

/* Write an array of time_t's to the file. Set the error flag to nz on error */
/* (This will cause file checksum fail if different executables on the same */
/*  system have different time_t values) */
static void write_time_ts(mknonv *x, FILE *fp, time_t *dp, int n) {

	if (fwrite((void *)dp, sizeof(time_t), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(time_t));
	}
}

/* Read an array of ints from the file. Set the error flag to nz on error */
static void read_ints(mknonv *x, FILE *fp, int *dp, int n) {

	if (fread((void *)dp, sizeof(int), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(int));
	}
}

/* Read an array of chars from the file. Set the error flag to nz on error */
static void read_chars(mknonv *x, FILE *fp, char *dp, int n) {

	if (fread((void *)dp, sizeof(char), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(char));
	}
}


/* Read an array of doubles from the file. Set the error flag to nz on error */
static void read_doubles(mknonv *x, FILE *fp, double *dp, int n) {

	if (fread((void *)dp, sizeof(double), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(double));
	}
}

/* Read an array of time_t's from the file. Set the error flag to nz on error */
/* (This will cause file checksum fail if different executables on the same */
/*  system have different time_t values) */
static void read_time_ts(mknonv *x, FILE *fp, time_t *dp, int n) {

	if (fread((void *)dp, sizeof(time_t), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(time_t));
	}
}

munki_code munki_save_calibration(munki *p) {
	munkiimp *m = (munkiimp *)p->m;
	munki_code ev = MUNKI_OK;
	munki_state *s;
	int i;
	char nmode[10];
	char cal_name[100];		/* Name */
	char **cal_paths = NULL;
	int no_paths = 0;
	FILE *fp;
	mknonv x;
	int ss;
	int argyllversion = ARGYLL_VERSION;

	strcpy(nmode, "w");
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	strcat(nmode, "b");
#endif

	sprintf(cal_name, "ArgyllCMS/.mk_%s.cal", m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_write, xdg_user, cal_name)) < 1) {
		a1logd(p->log,1,"munki_save_calibration xdg_bds returned no paths\n");
		return MUNKI_INT_CAL_SAVE;
	}

	a1logd(p->log,3,"munki_save_calibration saving to file '%s'\n",cal_paths[0]);

	if (create_parent_directories(cal_paths[0])
	 || (fp = fopen(cal_paths[0], nmode)) == NULL) {
		a1logd(p->log,3,"munki_save_calibration failed to open file for writing\n");
		xdg_free(cal_paths, no_paths);
		return MUNKI_INT_CAL_SAVE;
	}
	
	x.ef = 0;
	x.chsum = 0;

	/* A crude structure signature */
	ss = sizeof(munki_state) + sizeof(munkiimp);

	/* Some file identification */
	write_ints(&x, fp, &argyllversion, 1);
	write_ints(&x, fp, &ss, 1);
	write_chars(&x, fp, m->serno, 17);
	write_ints(&x, fp, &m->nraw, 1);
	write_ints(&x, fp, (int *)&m->nwav1, 1);
	write_ints(&x, fp, (int *)&m->nwav2, 1);

	/* For each mode, save the calibration if it's valid */
	for (i = 0; i < mk_no_modes; i++) {
		s = &m->ms[i];

		/* Mode identification */
		write_ints(&x, fp, &s->emiss, 1);
		write_ints(&x, fp, &s->trans, 1);
		write_ints(&x, fp, &s->reflective, 1);
		write_ints(&x, fp, &s->scan, 1);
		write_ints(&x, fp, &s->flash, 1);
		write_ints(&x, fp, &s->ambient, 1);
		write_ints(&x, fp, &s->projector, 1);
		write_ints(&x, fp, &s->adaptive, 1);

		/* Configuration calibration is valid for */
		write_ints(&x, fp, &s->gainmode, 1);
		write_doubles(&x, fp, &s->inttime, 1);

		/* Calibration information */
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
			write_doubles(&x, fp, s->cal_factor1, m->nwav1);
			write_doubles(&x, fp, s->cal_factor2, m->nwav2);
			write_doubles(&x, fp, s->white_data-1, m->nraw+1);
			write_doubles(&x, fp, &s->reftemp, 1);
			write_doubles(&x, fp, s->iwhite_data[0]-1, m->nraw+1);
			write_doubles(&x, fp, s->iwhite_data[1]-1, m->nraw+1);
		}
		
		write_ints(&x, fp, &s->idark_valid, 1);
		write_time_ts(&x, fp, &s->iddate, 1);
		write_doubles(&x, fp, s->idark_int_time, 4);
		write_doubles(&x, fp, s->idark_data[0]-1, m->nraw+1);
		write_doubles(&x, fp, s->idark_data[1]-1, m->nraw+1);
		write_doubles(&x, fp, s->idark_data[2]-1, m->nraw+1);
		write_doubles(&x, fp, s->idark_data[3]-1, m->nraw+1);
	}

	a1logd(p->log,3,"Checkum = 0x%x\n",x.chsum);
	write_ints(&x, fp, (int *)&x.chsum, 1);

	if (fclose(fp) != 0)
		x.ef = 2;

	if (x.ef != 0) {
		a1logd(p->log,3,"Writing calibration file failed with %d\n",x.ef);
		delete_file(cal_paths[0]);
	} else {
		a1logd(p->log,3,"Writing calibration file succeeded\n");
	}
	xdg_free(cal_paths, no_paths);

	return ev;
}

/* Restore the all modes calibration from the local system */
munki_code munki_restore_calibration(munki *p) {
	munkiimp *m = (munkiimp *)p->m;
	munki_code ev = MUNKI_OK;
	munki_state *s, ts;
	int i, j;
	char nmode[10];
	char cal_name[100];		/* Name */
	char **cal_paths = NULL;
	int no_paths = 0;
	FILE *fp;
	mknonv x;
	int argyllversion;
	int ss, nraw, nwav1, nwav2, chsum1, chsum2;
	char serno[17];

	strcpy(nmode, "r");
#if defined(O_BINARY) || defined(_O_BINARY)
	strcat(nmode, "b");
#endif

	sprintf(cal_name, "ArgyllCMS/.mk_%s.cal" SSEPS "color/.mk_%s.cal", m->serno, m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_read, xdg_user, cal_name)) < 1) {
		a1logd(p->log,1,"munki_restore_calibration xdg_bds returned no paths\n");
		return MUNKI_INT_CAL_RESTORE;
	}

	a1logd(p->log,2,"munki_restore_calibration restoring from file '%s'\n",cal_paths[0]);

	/* Check the last modification time */
	{
		struct sys_stat sbuf;

		if (sys_stat(cal_paths[0], &sbuf) == 0) {
			m->lo_secs = time(NULL) - sbuf.st_mtime;
			a1logd(p->log,2,"munki_restore_calibration: %d secs from instrument last open\n",m->lo_secs);
		} else {
			a1logd(p->log,2,"munki_restore_calibration: stat on file failed\n");
		}
	}

	if ((fp = fopen(cal_paths[0], nmode)) == NULL) {
		a1logd(p->log,2,"munki_restore_calibration failed to open file for reading\n");
		xdg_free(cal_paths, no_paths);
		return MUNKI_INT_CAL_RESTORE;
	}
	xdg_free(cal_paths, no_paths);
	
	x.ef = 0;
	x.chsum = 0;

	/* Check the file identification */
	read_ints(&x, fp, &argyllversion, 1);
	read_ints(&x, fp, &ss, 1);
	read_chars(&x, fp, serno, 17);
	read_ints(&x, fp, &nraw, 1);
	read_ints(&x, fp, &nwav1, 1);
	read_ints(&x, fp, &nwav2, 1);
	if (x.ef != 0
	 || argyllversion != ARGYLL_VERSION
	 || ss != (sizeof(munki_state) + sizeof(munkiimp))
	 || strcmp(serno, m->serno) != 0
	 || nraw != m->nraw
	 || nwav1 != m->nwav1
	 || nwav2 != m->nwav2) {
		a1logd(p->log,3,"Identification didn't verify\n");
		goto reserr;
	}

	/* Do a dummy read to check the checksum */
	for (i = 0; i < mk_no_modes; i++) {
		int di;
		double dd;
		time_t dt;
		int emiss, trans, reflective, ambient, projector, scan, flash, adaptive;

		s = &m->ms[i];

		/* Mode identification */
		read_ints(&x, fp, &emiss, 1);
		read_ints(&x, fp, &trans, 1);
		read_ints(&x, fp, &reflective, 1);
		read_ints(&x, fp, &scan, 1);
		read_ints(&x, fp, &flash, 1);
		read_ints(&x, fp, &ambient, 1);
		read_ints(&x, fp, &projector, 1);
		read_ints(&x, fp, &adaptive, 1);

		/* Configuration calibration is valid for */
		read_ints(&x, fp, &di, 1);					/* gainmode */
		read_doubles(&x, fp, &dd, 1);				/* inttime */

		/* Calibration information */
		read_ints(&x, fp, &di, 1);					/* dark_valid */
		read_time_ts(&x, fp, &dt, 1);				/* ddate */
		read_doubles(&x, fp, &dd, 1);				/* dark_int_time */
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* dark_data */
		read_doubles(&x, fp, &dd, 1);				/* dark_int_time2 */
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* dark_data2 */
		read_doubles(&x, fp, &dd, 1);				/* dark_int_time3 */
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* dark_data3 */
		read_ints(&x, fp, &di, 1);					/* dark_gain_mode */

		if (!s->emiss) {
			read_ints(&x, fp, &di, 1);				/* cal_valid */
			read_time_ts(&x, fp, &dt, 1);			/* cfdate */
			for (j = 0; j < m->nwav1; j++)
				read_doubles(&x, fp, &dd, 1);		/* cal_factor1 */
			for (j = 0; j < m->nwav2; j++)
				read_doubles(&x, fp, &dd, 1);		/* cal_factor2 */
			for (j = -1; j < m->nraw; j++)
				read_doubles(&x, fp, &dd, 1);		/* white_data */
			read_doubles(&x, fp, &dd, 1);			/* reftemp */
			for (j = -1; j < m->nraw; j++)
				read_doubles(&x, fp, &dd, 1);		/* iwhite_data[0] */
			for (j = -1; j < m->nraw; j++)
				read_doubles(&x, fp, &dd, 1);		/* iwhite_data[1] */
		}
		
		read_ints(&x, fp, &di, 1);					/* idark_valid */
		read_time_ts(&x, fp, &dt, 1);				/* iddate */
		for (j = 0; j < 4; j++)
			read_doubles(&x, fp, &dd, 1);			/* idark_int_time */
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* idark_data[0] */
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* idark_data[1] */
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* idark_data[2] */
		for (j = -1; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* idark_data[3] */
	}

	chsum1 = x.chsum;
	read_ints(&x, fp, &chsum2, 1);
	
	if (x.ef != 0
	 || chsum1 != chsum2) {
		a1logd(p->log,3,"Checksum didn't verify, got 0x%x, expected 0x%x\n",chsum1, chsum2);
		goto reserr;
	}

	rewind(fp);

	/* Allocate space in temp structure */

	ts.dark_data = dvectorz(-1, m->nraw-1);  
	ts.dark_data2 = dvectorz(-1, m->nraw-1);  
	ts.dark_data3 = dvectorz(-1, m->nraw-1);  
	ts.cal_factor1 = dvectorz(0, m->nwav1-1);
	ts.cal_factor2 = dvectorz(0, m->nwav2-1);
	ts.white_data = dvectorz(-1, m->nraw-1);
	ts.iwhite_data = dmatrixz(0, 2, -1, m->nraw-1);  
	ts.idark_data = dmatrixz(0, 3, -1, m->nraw-1);  

	/* Read the identification */
	read_ints(&x, fp, &argyllversion, 1);
	read_ints(&x, fp, &ss, 1);
	read_chars(&x, fp, m->serno, 17);
	read_ints(&x, fp, &m->nraw, 1);
	read_ints(&x, fp, (int *)&m->nwav1, 1);
	read_ints(&x, fp, (int *)&m->nwav2, 1);

	/* For each mode, save the calibration if it's valid */
	for (i = 0; i < mk_no_modes; i++) {
		s = &m->ms[i];

		/* Mode identification */
		read_ints(&x, fp, &ts.emiss, 1);
		read_ints(&x, fp, &ts.trans, 1);
		read_ints(&x, fp, &ts.reflective, 1);
		read_ints(&x, fp, &ts.scan, 1);
		read_ints(&x, fp, &ts.flash, 1);
		read_ints(&x, fp, &ts.ambient, 1);
		read_ints(&x, fp, &ts.projector, 1);
		read_ints(&x, fp, &ts.adaptive, 1);

		/* Configuration calibration is valid for */
		read_ints(&x, fp, &ts.gainmode, 1);
		read_doubles(&x, fp, &ts.inttime, 1);

		/* Calibration information: */

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
			read_doubles(&x, fp, ts.cal_factor1, m->nwav1);
			read_doubles(&x, fp, ts.cal_factor2, m->nwav2);
			read_doubles(&x, fp, ts.white_data-1, m->nraw+1);
			read_doubles(&x, fp, &ts.reftemp, 1);
			read_doubles(&x, fp, ts.iwhite_data[0]-1, m->nraw+1);
			read_doubles(&x, fp, ts.iwhite_data[1]-1, m->nraw+1);
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
		 && s->projector == ts.projector
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
			s->projector = ts.projector;
			s->adaptive = ts.adaptive;

			s->gainmode = ts.gainmode;
			s->inttime = ts.inttime;
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
				for (j = 0; j < m->nwav1; j++)
					s->cal_factor1[j] = ts.cal_factor1[j];
				for (j = 0; j < m->nwav2; j++)
					s->cal_factor2[j] = ts.cal_factor2[j];
				for (j = -1; j < m->nraw; j++)
					s->white_data[j] = ts.white_data[j];
				s->reftemp = ts.reftemp;
				for (j = -1; j < m->nraw; j++)
					s->iwhite_data[0][j] = ts.iwhite_data[0][j];
				for (j = -1; j < m->nraw; j++)
					s->iwhite_data[1][j] = ts.iwhite_data[1][j];
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
//printf("~1 mode %d\n",i);
//printf("~1 adaptive = %d\n",s->adaptive);
//printf("~1 innttime %f %f\n",s->inttime, ts.inttime);
//printf("~1 dark_int_time %f %f\n",s->dark_int_time,ts.dark_int_time);
//printf("~1 dark_int_time2 %f %f\n",s->dark_int_time2,ts.dark_int_time2);
//printf("~1 dark_int_time3 %f %f\n",s->dark_int_time3,ts.dark_int_time3);
//printf("~1 idark_int_time0 %f %f\n",s->idark_int_time[0],ts.idark_int_time[0]);
//printf("~1 idark_int_time1 %f %f\n",s->idark_int_time[1],ts.idark_int_time[1]);
//printf("~1 idark_int_time2 %f %f\n",s->idark_int_time[2],ts.idark_int_time[2]);
//printf("~1 idark_int_time3 %f %f\n",s->idark_int_time[3],ts.idark_int_time[3]);
			a1logd(p->log,4,"Not restoring cal for mode %d since params don't match:\n",i);
			a1logd(p->log,4,"emis = %d : %d, trans = %d : %d, ref = %d : %d\n",s->emiss,ts.emiss,s->trans,ts.trans,s->reflective,ts.reflective);
			a1logd(p->log,4,"scan = %d : %d, flash = %d : %d, ambi = %d : %d, proj = %d : %d, adapt = %d : %d\n",s->scan,ts.scan,s->flash,ts.flash,s->ambient,ts.ambient,s->projector,ts.projector,s->adaptive,ts.adaptive);
			a1logd(p->log,4,"inttime = %f : %f\n",s->inttime,ts.inttime);
			a1logd(p->log,4,"darkit1 = %f : %f, 2 = %f : %f, 3 = %f : %f\n",s->dark_int_time,ts.dark_int_time,s->dark_int_time2,ts.dark_int_time2,s->dark_int_time3,ts.dark_int_time3);
			a1logd(p->log,4,"idarkit0 = %f : %f, 1 = %f : %f, 2 = %f : %f, 3 = %f : %f\n",s->idark_int_time[0],ts.idark_int_time[0],s->idark_int_time[1],ts.idark_int_time[1],s->idark_int_time[2],ts.idark_int_time[2],s->idark_int_time[3],ts.idark_int_time[3]);
		}
	}

	/* Free up temporary space */
	free_dvector(ts.dark_data, -1, m->nraw-1);  
	free_dvector(ts.dark_data2, -1, m->nraw-1);  
	free_dvector(ts.dark_data3, -1, m->nraw-1);  
	free_dvector(ts.white_data, -1, m->nraw-1);
	free_dmatrix(ts.iwhite_data, 0, 1, -1, m->nraw-1);  
	free_dmatrix(ts.idark_data, 0, 3, -1, m->nraw-1);  

	free_dvector(ts.cal_factor1, 0, m->nwav1-1);
	free_dvector(ts.cal_factor2, 0, m->nwav2-1);

	a1logd(p->log,3,"munki_restore_calibration done\n");
 reserr:;

	fclose(fp);

	return ev;
}

munki_code munki_touch_calibration(munki *p) {
	munkiimp *m = (munkiimp *)p->m;
	munki_code ev = MUNKI_OK;
	char cal_name[100];		/* Name */
	char **cal_paths = NULL;
	int no_paths = 0;
	int rv;

	sprintf(cal_name, "ArgyllCMS/.mk_%s.cal" SSEPS "color/.mk_%s.cal", m->serno, m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_read, xdg_user, cal_name)) < 1)
		return MUNKI_INT_CAL_TOUCH;

	a1logd(p->log,2,"munki_touch_calibration touching file '%s'\n",cal_paths[0]);

	if ((rv = sys_utime(cal_paths[0], NULL)) != 0) {
		a1logd(p->log,2,"munki_touch_calibration failed with %d\n",rv);
		xdg_free(cal_paths, no_paths);
		return MUNKI_INT_CAL_TOUCH;
	}
	xdg_free(cal_paths, no_paths);

	return ev;
}

#endif /* ENABLE_NONVCAL */


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
) {
	munki_code ev = MUNKI_OK;

	if (nummeas <= 0)
		return MUNKI_INT_ZEROMEASURES;
		
	if ((ev = munki_trigger_one_measure(p, nummeas, inttime, gainmode, 1, 1)) != MUNKI_OK)
		return ev;

	if ((ev = munki_readmeasurement(p, nummeas, 0, buf, bsize, NULL, 1, 1)) != MUNKI_OK)
		return ev;

	return ev;
}

/* Take a dark reference measurement - part 2 */
munki_code munki_dark_measure_2(
	munki *p,
	double *sens,		/* Return array [-1 nraw] of sens values */
	int nummeas,			/* Number of readings to take */
	double inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	unsigned char *buf,		/* raw USB reading buffer to process */
	unsigned int bsize		/* Buffer size to process */
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	double **multimes;		/* Multiple measurement results */
	double darkthresh;		/* Dark threshold */
	double sensavg;			/* Overall average of sensor readings */
	int rv;

	multimes = dmatrix(0, nummeas-1, -1, m->nraw-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* floating point sensor readings. Check for saturation */
	if ((rv = munki_sens_to_raw(p, multimes, NULL, buf, 0, nummeas, m->satlimit, &darkthresh))
		                                                                        != MUNKI_OK) {
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		return rv;
	}

	/* Average a set of measurements into one. */
	/* Return nz if the readings are not consistent */
	/* Return the overall average. */
	rv = munki_average_multimeas(p, sens, multimes, nummeas, &sensavg, darkthresh);     
	free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);

#ifdef PLOT_DEBUG
	a1logd(p->log,4,"dark_measure_2: Avg abs. sensor readings = %f, sh/darkthresh %f\n",sensavg,darkthresh);
	printf("sens data:\n");
	plot_raw(sens);
#endif

	if (rv) {
		a1logd(p->log,3,"munki_dark_measure_2: readings are inconsistent\n");
		return MUNKI_RD_DARKREADINCONS;
	}

	if (sensavg > (2.0 * darkthresh)) {
		a1logd(p->log,3,"munki_dark_measure_2: Average %f is > 2 * darkthresh %f\n",sensavg,darkthresh);
		return MUNKI_RD_DARKNOTVALID;
	}
	return ev;
}

#ifdef DUMP_DARKM
int ddumpdarkm = 0;
#endif

/* Take a dark reference measurement (combined parts 1 & 2) */
munki_code munki_dark_measure(
	munki *p,
	double *raw,			/* Return array [-1 nraw] of raw values */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode			/* Gain mode to use, 0 = normal, 1 = high */
) {
	munkiimp *m = (munkiimp *)p->m;
	munki_code ev = MUNKI_OK;
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;

	a1logd(p->log,3, "munki_dark_measure with inttime %f\n",*inttime);
	bsize = m->nsen * 2 * nummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"munki_dark_measure malloc %d bytes failed (8)\n",bsize);
		return MUNKI_INT_MALLOC;
	}

	if ((ev = munki_dark_measure_1(p, nummeas, inttime, gainmode, buf, bsize)) != MUNKI_OK) {
		free(buf);
		return ev;
	}

	if ((ev = munki_dark_measure_2(p, raw, nummeas, *inttime, gainmode, buf, bsize))
	                                                                           != MUNKI_OK) {
		free(buf);
		return ev;
	}
	free(buf);

#ifdef DUMP_DARKM
	/* Dump raw dark readings to a file "mkddump.txt" */
	if (ddumpdarkm) {
		int j;
		FILE *fp;
		
		if ((fp = fopen("mkddump.txt", "a")) == NULL)
			a1logw(p->log,"Unable to open debug file mkddump.txt\n");
		else {
			fprintf(fp, "\nDark measure: nummeas %d, inttime %f, gainmode %d, darkcells %f\n",nummeas,*inttime,gainmode, raw[-1]);
			fprintf(fp,"\t\t\t{ ");
			for (j = 0; j < (m->nraw-1); j++)
				fprintf(fp, "%f, ",raw[j]);
			fprintf(fp, "%f },\n",raw[j]);
			fclose(fp);
		}
	}
#endif
	return ev;
}

/* Heat the LED up for given number of seconds by taking a reading */
munki_code munki_heatLED(
	munki *p,
	double htime		/* Heat up time */
) {
	munkiimp *m = (munkiimp *)p->m;
	double inttime = m->cal_int_time; 		/* Integration time to use/used */
	int nummeas;
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	int rv;

	a1logd(p->log,3,"munki_heatLED called \n");

	nummeas = munki_comp_ru_nummeas(p, htime, inttime);

	if (nummeas <= 0)
		return MUNKI_OK;
		
	/* Allocate temporaries */
	bsize = m->nsen * 2 * nummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"munki_heatLED malloc %d bytes failed (10)\n",bsize);
		return MUNKI_INT_MALLOC;
	}

	a1logd(p->log,3,"Triggering measurement cycle, nummeas %d, inttime %f\n", nummeas, inttime);

	if ((rv = munki_trigger_one_measure(p, nummeas, &inttime, 0, 1, 0))
	                                                                        != MUNKI_OK) {
		free(buf);
		return rv;
	}

	a1logd(p->log,3,"Gathering readings\n");

	rv = munki_readmeasurement(p, nummeas, 0, buf, bsize, NULL, 1, 0);

	free(buf);

	return rv;
}


/* Take a reflective or emissive white reference sens measurement, */
/* subtracts black and processes into wavelenths. */
/* (absraw is usually ->white_data) */
munki_code munki_whitemeasure(
	munki *p,
	double *absraw,			/* Return array [-1 nraw] of absraw values (may be NULL) */
	double *optscale,		/* Return scale for gain/int time to make optimal (may be NULL) */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	double targoscale		/* Ratio of optimal sensor value to aim for */
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	int ninvmeas = 0;			/* Number of invalid measurements */
	double **multimes;		/* Multiple measurement results */
	double sensavg;			/* Overall average of sensor readings */
	double darkthresh;		/* Dark threshold */
	double trackmax[3];		/* Track optimum target & darkthresh */
	double maxval;			/* Maximum multimeas value */
	int rv;

	a1logd(p->log,3,"munki_whitemeasure called \n");

	if (s->reflective) {
		/* Compute invalid samples to allow for LED warmup */
		ninvmeas = munki_comp_ru_nummeas(p, m->refinvalidsampt, *inttime);
	}

	if (nummeas <= 0)
		return MUNKI_INT_ZEROMEASURES;
		
	/* Allocate temporaries */
	bsize = m->nsen * 2 * (ninvmeas + nummeas);
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"munki_whitemeasure malloc %d bytes failed (10)\n",bsize);
		return MUNKI_INT_MALLOC;
	}

	a1logd(p->log,3,"Triggering measurement cycle, ninvmeas %d, nummeas %d, inttime %f, gainmode %d\n",
	                                              ninvmeas, nummeas, *inttime, gainmode);

	if ((ev = munki_trigger_one_measure(p, ninvmeas + nummeas, inttime, gainmode, 1, 0))
	                                                                        != MUNKI_OK) {
		free(buf);
		return ev;
	}

	a1logd(p->log,3,"Gathering readings\n");

	if ((ev = munki_readmeasurement(p, ninvmeas + nummeas, 0, buf, bsize, NULL, 1, 0))
	                                                                      != MUNKI_OK) {
		free(buf);
		return ev;
	}

  	multimes = dmatrix(0, nummeas-1, -1, m->nraw-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* floating point sensor readings. Check for saturation */
	if ((rv = munki_sens_to_raw(p, multimes, NULL, buf, ninvmeas, nummeas, m->satlimit,
		                                                     &darkthresh)) != MUNKI_OK) {
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		return rv;
	}

#ifdef PLOT_DEBUG
	printf("Dark data:\n");
	plot_raw(s->dark_data);
#endif

	trackmax[0] = darkthresh;		/* Track the dark threshold value */
	trackmax[1] = m->optsval;		/* Track the optimal sensor target value */
	trackmax[2] = m->satlimit;		/* For debugging */

	/* Subtract the black from sensor values and convert to */
	/* absolute (integration & gain scaled), zero offset based, */
	/* linearized sensor values. */
	/* Return the highest individual element. */
	munki_sub_raw_to_absraw(p, nummeas, *inttime, gainmode, multimes, s->dark_data,
	                             trackmax, 3, &maxval);
	darkthresh = trackmax[0];
	free(buf);

	if (absraw != NULL) {
		/* Average a set of measurements into one. */
		/* Return nz if the readings are not consistent */
		/* Return the overall average. */
		rv = munki_average_multimeas(p, absraw, multimes, nummeas, &sensavg, darkthresh);     
	
#ifndef IGNORE_WHITE_INCONS
		if (rv) {
			free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
			return MUNKI_RD_WHITEREADINCONS;
		}
#endif /* IGNORE_WHITE_INCONS */

		a1logd(p->log,3,"Average absolute sensor readings, avg %f, max %f, darkth %f satth %f\n",
		               sensavg,maxval,darkthresh,trackmax[2]);
#ifdef PLOT_DEBUG
		printf("absraw whitemeas:\n");
		plot_raw(absraw);
#endif
	}

	if (optscale != NULL) {
		double opttarget;       /* Optimal reading scale target value */

		opttarget = targoscale * trackmax[1];
		if (maxval < 0.01)		/* Could go -ve */
			maxval = 0.01;
		*optscale = opttarget/maxval; 

		a1logd(p->log,3,"Targscale %f, maxval %f, optimal target = %f, amount to scale = %f\n",
		     targoscale, maxval, opttarget, *optscale);
	}

	free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);		/* Free after using *pmax */

	return ev;
}

/* Given an absraw white reference measurement, */
/* compute the wavelength equivalents. */
/* (absraw is usually ->white_data) */
/* (abswav1 is usually ->cal_factor1) */
/* (abswav2 is usually ->cal_factor2) */
munki_code munki_compute_wav_whitemeas(
	munki *p,
	double *abswav1,		/* Return array [nwav1] of abswav values (may be NULL) */
	double *abswav2,		/* Return array [nwav2] of abswav values (if hr_init, may be NULL) */
	double *absraw			/* Given array [-1 nraw] of absraw values */
) {
	munkiimp *m = (munkiimp *)p->m;

	/* Convert an absraw array from raw wavelengths to output wavelenths */
	if (abswav1 != NULL) {
		munki_absraw_to_abswav1(p, 1, &abswav1, &absraw);

#ifdef PLOT_DEBUG
		printf("White meas converted to wavelengths std res:\n");
		plot_wav1(m, abswav1);
#endif
	}

#ifdef HIGH_RES
	if (abswav2 != NULL && m->hr_inited == 2) {
		munki_absraw_to_abswav2(p, 1, &abswav2, &absraw);

#ifdef PLOT_DEBUG
		printf("Converted to wavelengths high res:\n");
		plot_wav2(m, abswav2);
#endif
	}
#endif /* HIGH_RES */

	return MUNKI_OK;
}

/* Take a reflective white reference measurement, */
/* subtracts black and decompose into base + LED temperature components */
munki_code munki_ledtemp_whitemeasure(
	munki *p,
	double *white,			/* Return [-1 nraw] of temperature compensated white reference */
	double **iwhite,		/* Return array [-1 nraw][2] of absraw base and scale values */
	double *reftemp,		/* Return a reference temperature to normalize to */
	int nummeas,			/* Number of readings to take */
	double inttime, 		/* Integration time to use/used */
	int gainmode			/* Gain mode to use, 0 = normal, 1 = high */
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	int ninvmeas = 0;		/* Number of invalid measurements */
	double **multimes;		/* Multiple measurement results */
	double *ledtemp;		/* LED temperature for each measurement */
	double darkthresh;		/* Dark threshold */
	int rv;

	a1logd(p->log,3,"munki_ledtemp_whitemeasure called \n");

	/* Compute invalid samples to allow for LED warmup */
	ninvmeas = munki_comp_ru_nummeas(p, m->refinvalidsampt, inttime);

	if (nummeas <= 0) {
		return MUNKI_INT_ZEROMEASURES;
	}
		
	/* Allocate temporaries */
	bsize = m->nsen * 2 * (ninvmeas + nummeas);
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"munki_whitemeasure malloc %d bytes failed (10)\n",bsize);
		return MUNKI_INT_MALLOC;
	}

	a1logd(p->log,3,"Triggering measurement cycle, ninvmeas %d, nummeas %d, inttime %f, gainmode %d\n",
	                                              ninvmeas, nummeas, inttime, gainmode);

	if ((ev = munki_trigger_one_measure(p, ninvmeas + nummeas, &inttime, gainmode, 1, 0))
	                                                                        != MUNKI_OK) {
		free(buf);
		return ev;
	}

	a1logd(p->log,3,"Gathering readings\n");

	if ((ev = munki_readmeasurement(p, ninvmeas + nummeas, 0, buf, bsize, NULL, 1, 0))
	                                                                      != MUNKI_OK) {
		free(buf);
		return ev;
	}

  	multimes = dmatrix(0, nummeas-1, -1, m->nraw-1);
  	ledtemp = dvector(0, nummeas-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* floating point sensor readings. Check for saturation */
	if ((ev = munki_sens_to_raw(p, multimes, ledtemp, buf, ninvmeas, nummeas, m->satlimit,
	                                                           &darkthresh)) != MUNKI_OK) {
		free_dvector(ledtemp, 0, nummeas-1);
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		return ev;
	}

	/* Make the reference temperature nominal */
	*reftemp = 0.5 * (ledtemp[0] + ledtemp[nummeas-1]);

#ifdef PLOT_DEBUG
	printf("Ledtemp Dark data:\n");
	plot_raw(s->dark_data);
#endif

	/* Subtract the black from sensor values and convert to */
	/* absolute (integration & gain scaled), zero offset based, */
	/* linearized sensor values. */
	munki_sub_raw_to_absraw(p, nummeas, inttime, gainmode, multimes, s->dark_data,
	                             &darkthresh, 1, NULL);

	free(buf);

	/* For each raw wavelength, compute a linear regression */
	{
		int i, w;
		double tt, ss, sx, sy, sxdss, stt, b;

		ss = (double)nummeas;
		for (sx = 0.0, i = 0; i < nummeas; i++)
			sx += ledtemp[i];
		sxdss = sx/ss;

		for (w = -1; w < m->nraw; w++) {
			for (sy = 0.0, i = 0; i < nummeas; i++)
				sy += multimes[i][w];
			
			for (stt = b = 0.0, i = 0; i < nummeas; i++) {
				tt = ledtemp[i] - sxdss;
				stt += tt * tt;
				b += tt * multimes[i][w];
			}
			b /= stt;

			iwhite[0][w] = (sy - sx * b)/ss;
			iwhite[1][w] = b;
		}
	}
#ifdef DEBUG
	{	/* Verify the linear regression */
		int i, w;
		double x, terr = 0.0, errc = 0.0;

		for (w = -1; w < m->nraw; w++) {
			for (i = 0; i < nummeas; i++) {
				x = iwhite[0][w] + ledtemp[i] * iwhite[1][w];
				terr += fabs(x -  multimes[i][w]);
				errc++;
			}
		}
		terr /= errc;
		printf("Linear regression average error = %f\n",terr);
	}
#endif

#ifdef PLOT_TEMPCOMP
	/* Plot the raw spectra and model, 3 at a time */
	{
		int i, j, k;
		double *indx;
		double **mod;
		indx = dvectorz(0, nummeas-1);  
	  	mod = dmatrix(0, 5, 0, nummeas-1);
		for (i = 0; i < nummeas; i++)
			indx[i] = 3.0 * i/(nummeas-1.0);

		for (j = 0; (j+2) < (m->nraw-1); j += 3) {
			for (i = 0; i < nummeas; i++) {
				for (k = j; k < (j + 3); k++) {
					mod[k-j][i] = iwhite[0][k] + ledtemp[i] * iwhite[1][k];
				}
				for (k = j; k < (j + 3); k++) {
					mod[k-j+3][i] = multimes[i][k];
				}
			}

			printf("Bands %d - %d\n",j, j+2);
			do_plot6(indx, mod[0], mod[1], mod[2], mod[3], mod[4], mod[5], nummeas);
		}
		free_dvector(indx, 0, nummeas-1);  
	  	free_dmatrix(mod, 0, 5, 0, nummeas-1);
	}
#endif

	a1logd(p->log,3,"Computed linear regression\n");

#ifdef ENABLE_LEDTEMPC
	/* Compute a temerature compensated set of white readings */
	if ((ev = munki_ledtemp_comp(p, multimes, ledtemp, nummeas, *reftemp, iwhite)) != MUNKI_OK) {   
		free_dvector(ledtemp, 0, nummeas-1);
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		return ev;
	}
#endif /* ENABLE_LEDTEMPC */

	/* Average a set of measurements into one. */
	if ((rv = munki_average_multimeas(p, white, multimes, nummeas, NULL, darkthresh)) != 0) {
		free_dvector(ledtemp, 0, nummeas-1);
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		a1logd(p->log,3,"munki_ledtemp_whitemeasure: readings are inconsistent\n");
		return MUNKI_RD_DARKREADINCONS;
	}

	free_dvector(ledtemp, 0, nummeas-1);
	free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);

	return ev;
}

/* Given the ledtemp base and scale values, */
/* return an absraw reflective white reference for the */
/* given temperature */ 
munki_code munki_ledtemp_white(
	munki *p,
	double *absraw,			/* Return array [-1 nraw] of absraw base and scale values */
	double **iwhite,		/* ledtemp base and scale */
	double ledtemp			/* LED temperature value */
) {
	munkiimp *m = (munkiimp *)p->m;
	int w;

	for (w = -1; w < m->nraw; w++)
		absraw[w] = iwhite[0][w] + ledtemp * iwhite[1][w];

	return MUNKI_OK;
}

/* Given a set of absraw sensor readings and the corresponding temperature, */
/* compensate the readings to be at the nominated temperature. */
munki_code munki_ledtemp_comp(
	munki *p,
	double **absraw,		/* [nummeas][raw] measurements to compensate */
	double *ledtemp,		/* LED temperature for each measurement */
	int nummeas,			/* Number of measurements */
	double reftemp,			/* LED reference temperature to compensate to */
	double **iwhite			/* ledtemp base and scale information */
) {
	munkiimp *m = (munkiimp *)p->m;
	int i, w;

	for (i = 0; i < nummeas; i++) {
		for (w = 0; w < m->nraw; w++) {		/* Don't try and compensate shielded values */
			double targ, attemp;
			targ   = iwhite[0][w] + reftemp * iwhite[1][w];
			attemp = iwhite[0][w] + ledtemp[i] * iwhite[1][w];

			absraw[i][w] *= targ/attemp;
		}
	}
	return MUNKI_OK;
}

/* Trigger measure and gather raw readings using the current mode. */
munki_code munki_read_patches_1(
	munki *p,
	int ninvmeas,			/* Number of extra invalid measurements at start */
	int minnummeas,			/* Minimum number of measurements to take */
	int maxnummeas,			/* Maximum number of measurements to allow for */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	int *nmeasuered,		/* Number actually measured (excluding ninvmeas) */
	unsigned char *buf,		/* Raw USB reading buffer */
	unsigned int bsize		/* Raw USB readings buffer size in bytes */
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;

	if ((ninvmeas + minnummeas) <= 0)
		return MUNKI_INT_ZEROMEASURES;

	if ((minnummeas + ninvmeas) > maxnummeas)
		maxnummeas = (minnummeas - ninvmeas);
		
	a1logd(p->log,3,"Triggering & gathering cycle, ninvmeas %d, minnummeas %d, inttime %f, gainmode %d\n",
	                                              ninvmeas, minnummeas, *inttime, gainmode);

	if ((ev = munki_trigger_one_measure(p, ninvmeas + minnummeas, inttime, gainmode, 0, 0))
	                                                                           != MUNKI_OK) {
		return ev;
	}

	if ((ev = munki_readmeasurement(p, ninvmeas + minnummeas, m->c_measmodeflags & MUNKI_MMF_SCAN,
	                                             buf, bsize, nmeasuered, 0, 0)) != MUNKI_OK) {
		return ev;
	}

	if (nmeasuered != NULL)
		*nmeasuered -= ninvmeas;	/* Correct for invalid number */

	return ev;
}

/* Given a buffer full of raw USB values, process them into */ 
/* completely processed spectral output patch readings. */
munki_code munki_read_patches_2(
	munki *p,
	double *duration,		/* Return flash duration in seconds */
	double **specrd,		/* Return array [numpatches][nwav] of spectral reading values */
	int numpatches,			/* Number of patches to return */
	double inttime, 		/* Integration time to used */
	int gainmode,			/* Gain mode useed, 0 = normal, 1 = high */
	int ninvmeas,			/* Number of extra invalid measurements at start */
	int nummeas,			/* Number of actual measurements */
	unsigned char *buf,		/* Raw USB reading buffer */
	unsigned int bsize		/* Raw USB reading buffer size */
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	double **multimes;		/* Multiple measurement results [maxnummeas|nummeas][-1 nraw]*/
	double **absraw;		/* Linearsised absolute sensor raw values [numpatches][-1 nraw]*/
	double *ledtemp;		/* LED temperature values */
	double darkthresh;		/* Dark threshold (for consistency checking) */
	int rv = 0;

	if (duration != NULL)
		*duration = 0.0;	/* default value */

	/* Allocate temporaries */
	multimes = dmatrix(0, nummeas-1, -1, m->nraw-1);
  	ledtemp = dvector(0, nummeas-1);
	absraw = dmatrix(0, numpatches-1, -1, m->nraw-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* floating point sensor readings. Check for saturation */
	if ((rv = munki_sens_to_raw(p, multimes, ledtemp, buf, ninvmeas, nummeas,
	                                         m->satlimit, &darkthresh)) != MUNKI_OK) {
		free_dvector(ledtemp, 0, nummeas-1);
		free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		return rv;
	}

	/* Subtract the black from sensor values and convert to */
	/* absolute (integration & gain scaled), zero offset based, */
	/* linearized sensor values. */
	munki_sub_raw_to_absraw(p, nummeas, inttime, gainmode, multimes, s->dark_data,
	                             &darkthresh, 1, NULL);

#ifdef DUMP_SCANV
	/* Dump raw scan readings to a file "mkdump.txt" */
	{
		int i, j;
		FILE *fp;
		
		if ((fp = fopen("mkdump.txt", "w")) == NULL)
			a1logw(p->log,"Unable to open debug file mkdump.txt\n");
		else {
			for (i = 0; i < nummeas; i++) {
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

#ifdef ENABLE_LEDTEMPC


	/* Do LED temperature compensation of absraw values */
	if (s->reflective) {

#ifdef PLOT_TEMPCOMP
	/* Plot the raw spectra, 6 at a time */
		{
			int i, j, k;
			double *indx;
			double **mod;
			indx = dvectorz(0, nummeas-1);  
		  	mod = dmatrix(0, 5, 0, nummeas-1);
			for (i = 0; i < nummeas; i++)
				indx[i] = (double)i;
	
//		for (j = 0; (j+5) < m->nraw; j += 6) {
			for (j = 50; (j+5) < 56; j += 6) {
				for (i = 0; i < nummeas; i++) {
					for (k = j; k < (j + 6); k++) {
						mod[k-j][i] = multimes[i][k];
					}
				}
	
				printf("Before temp comp, bands %d - %d\n",j, j+5);
				do_plot6(indx, mod[0], mod[1], mod[2], mod[3], mod[4], mod[5], nummeas);
			}
			free_dvector(indx, 0, nummeas-1);  
		  	free_dmatrix(mod, 0, 5, 0, nummeas-1);
		}
#endif
		/* Do the LED temperature compensation */
		if ((ev = munki_ledtemp_comp(p, multimes, ledtemp, nummeas, s->reftemp, s->iwhite_data))
		                                                                            != MUNKI_OK) {
			free_dvector(ledtemp, 0, nummeas-1);
			free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
			free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
			a1logd(p->log,3,"munki_read_patches_2 ledtemp comp failed\n");
			return ev;
		} 
#ifdef PLOT_TEMPCOMP
		/* Plot the raw spectra, 6 at a time */
		{
			int i, j, k;
			double *indx;
			double **mod;
			indx = dvectorz(0, nummeas-1);  
		  	mod = dmatrix(0, 5, 0, nummeas-1);
			for (i = 0; i < nummeas; i++)
				indx[i] = (double)i;
	
//			for (j = 0; (j+5) < m->nraw; j += 6) {
			for (j = 50; (j+5) < 56; j += 6) {
				for (i = 0; i < nummeas; i++) {
					for (k = j; k < (j + 6); k++) {
						mod[k-j][i] = multimes[i][k];
					}
				}
	
				printf("After temp comp, bands %d - %d\n",j, j+5);
				do_plot6(indx, mod[0], mod[1], mod[2], mod[3], mod[4], mod[5], nummeas);
			}
			free_dvector(indx, 0, nummeas-1);  
		  	free_dmatrix(mod, 0, 5, 0, nummeas-1);
		}
#endif
	}
#endif /* ENABLE_LEDTEMPC */


	if (!s->scan) {
		if (numpatches != 1) {
			free_dvector(ledtemp, 0, nummeas-1);
			free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
			free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
			a1logd(p->log,3,"munki_read_patches_2 spot read failed because numpatches != 1\n");
			return MUNKI_INT_WRONGPATCHES;
		}

		/* Average a set of measurements into one. */
		/* Return zero if readings are consistent. */
		/* Return nz if the readings are not consistent */
		/* Return the overall average. */
		rv = munki_average_multimeas(p, absraw[0], multimes, nummeas, NULL, darkthresh);     
	} else {

		if (s->flash) {

			if (numpatches != 1) {
				free_dvector(ledtemp, 0, nummeas-1);
				free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
				free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
				a1logd(p->log,3,"munki_read_patches_2 spot read failed because numpatches != 1\n");
				return MUNKI_INT_WRONGPATCHES;
			}
			if ((ev = munki_extract_patches_flash(p, &rv, duration, absraw[0], multimes,
			                                                 nummeas, inttime)) != MUNKI_OK) {
				free_dvector(ledtemp, 0, nummeas-1);
				free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
				free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
				a1logd(p->log,3,"munki_read_patches_2 spot read failed at munki_extract_patches_flash\n");
				return ev;
			}

		} else {
			a1logd(p->log,3,"Number of patches to be measured = %d\n",nummeas);

			/* Recognise the required number of ref/trans patch locations, */
			/* and average the measurements within each patch. */
			if ((ev = munki_extract_patches_multimeas(p, &rv, absraw, numpatches, multimes,
			                                   nummeas, inttime)) != MUNKI_OK) {
				free_dvector(ledtemp, 0, nummeas-1);
				free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
				free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
				a1logd(p->log,3,"munki_read_patches_2 spot read failed at munki_extract_patches_multimeas\n");
				return ev;
			}
		}
	}
	free_dvector(ledtemp, 0, nummeas-1);
	free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);

	if (rv) {
		free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
		a1logd(p->log,3,"munki_read_patches_2 spot read failed with inconsistent readings\n");
		return MUNKI_RD_READINCONS;
	}

#ifdef ENABLE_REFSTRAYC	/* Enable Reflective stray light compensation */
	if (s->reflective) {
		int i, j;
# if defined(PLOT_DEBUG) || defined(DUMP_BKLED)
		double xx[140];
		double yy[3][140];
# endif

		double fact = REFSTRAYC_FACTOR;		/* Slightly conservative */

		for (i = 0; i < numpatches; i++) {

			for (j = 0; j < m->nraw; j++) {
# if defined(PLOT_DEBUG) || defined(DUMP_BKLED)
				yy[0][j] = absraw[i][j];
# endif
				absraw[i][j] -= fact * s->white_data[j];
# if defined(PLOT_DEBUG) || defined(DUMP_BKLED)
				xx[j] = j;
				yy[1][j] = fact * s->white_data[j];
				yy[2][j] = absraw[i][j];
# endif
			}
# ifdef PLOT_DEBUG
			printf("Before/After subtracting stray ref. light %d:\n",i);
			do_plot6(xx, yy[0], yy[1], yy[2], NULL, NULL, NULL, m->nraw);
# endif
# ifdef DUMP_BKLED		/* Save REFSTRAYC & REFLEDNOISE comp plot to "refbk1.txt" & "refbk2.txt" */
		{
			xspect sp[3];
			for (i = 0; i < 3; i++) {
				sp[i].spec_n = 128;
				sp[i].spec_wl_short = 0.0;
				sp[i].spec_wl_long = 127.0;
				sp[i].norm = 1.0;
				for (j = 0; j < 128; j++)
					sp[i].spec[j] = yy[i][j];
			}
			write_nxspect("refbk2.txt", sp, 3, 0);
# pragma message("######### munki DUMP_BKLED enabled! ########")
		}
# endif	/* DUMP_BKLED */
		}
	}
#endif /* ENABLE_REFSTRAYC */

	/* Convert an absraw array from raw wavelengths to output wavelenths */
	munki_absraw_to_abswav(p, numpatches, specrd, absraw);
	free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);

#ifdef APPEND_MEAN_EMMIS_VAL
	/* Append averaged emission reading to file "mkdump.txt" */
	{
		int i, j;
		FILE *fp;
		
		/* Create wavelegth label */
		if ((fp = fopen("mkdump.txt", "r")) == NULL) {
			if ((fp = fopen("mkdump.txt", "w")) == NULL)
				a1logw(p->log,"Unable to reate debug file mkdump.txt\n");
			else {
				for (j = 0; j < m->nwav; j++)
					fprintf(fp,"%f ",XSPECT_WL(m->wl_short, m->wl_long, m->nwav, j));
				fprintf(fp,"\n");
				fclose(fp);
			}
		}
		if ((fp = fopen("mkdump.txt", "a")) == NULL)
			a1logw(p->log,"Unable to open debug file mkdump.txt\n");
		else {
			for (j = 0; j < m->nwav; j++)
				fprintf(fp, "%f	",specrd[0][j] * m->emis_coef[j]);
			fprintf(fp,"\n");
			fclose(fp);
		}
	}
#endif

#ifdef PLOT_DEBUG
	printf("Converted to wavelengths:\n");
	plot_wav(m, specrd[0]);
#endif

	/* Scale to the calibrated output values */
	munki_scale_specrd(p, specrd, numpatches, specrd);

#ifdef PLOT_DEBUG
	printf("Calibrated measuerment spectra:\n");
	plot_wav(m, specrd[0]);
#endif

	return ev;
}

/* Given a buffer full of raw USB values, process them into */ 
/* completely processed spectral output readings, */
/* but don't average together or extract patches or flash. */
/* This is used for delay & refresh rate measurement. */
/* !! Doesn't do LED temperature compensation for reflective !! */
/* (! Note that we aren't currently detecting saturation here!) */
munki_code munki_read_patches_2a(
	munki *p,
	double **specrd,		/* Return array [numpatches][nwav] of spectral reading values */
	int numpatches,			/* Number of patches measured and to return */
	double inttime, 		/* Integration time to used */
	int gainmode,			/* Gain mode useed, 0 = normal, 1 = high */
	unsigned char *buf,		/* Raw USB reading buffer */
	unsigned int bsize
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	double **absraw;		/* Linearsised absolute sensor raw values [numpatches][-1 nraw]*/
	double *ledtemp;		/* LED temperature values */
	double satthresh;		/* Saturation threshold */
	double darkthresh;		/* Dark threshold for consistency scaling limit */

	/* Allocate temporaries */
	absraw = dmatrix(0, numpatches-1, -1, m->nraw-1);
	ledtemp = dvector(0, numpatches-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* floating point sensor readings. Check for saturation */
	if ((ev = munki_sens_to_raw(p, absraw, ledtemp, buf, 0, numpatches,
					                        m->satlimit, &darkthresh)) != MUNKI_OK) {
		free_dvector(ledtemp, 0, numpatches-1);
		free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);
		return ev;
	}
	
	/* Subtract the black from sensor values and convert to */
	/* absolute (integration & gain scaled), zero offset based, */
	/* linearized sensor values. */
	munki_sub_raw_to_absraw(p, numpatches, inttime, gainmode, absraw, s->dark_data,
                           &darkthresh, 1, NULL);

	a1logd(p->log,3,"Number of patches measured = %d\n",numpatches);

	/* Convert an absraw array from raw wavelengths to output wavelenths */
	munki_absraw_to_abswav(p, numpatches, specrd, absraw);

	free_dvector(ledtemp, 0, numpatches-1);
	free_dmatrix(absraw, 0, numpatches-1, -1, m->nraw-1);

#ifdef PLOT_DEBUG
	printf("Converted to wavelengths:\n");
	plot_wav(m, specrd[0]);
#endif

	/* Scale to the calibrated output values */
	munki_scale_specrd(p, specrd, numpatches, specrd);

#ifdef PLOT_DEBUG
	printf("Calibrated measuerment spectra:\n");
	plot_wav(m, specrd[0]);
#endif

	return ev;
}

/* Take a measurement reading using the current mode (combined parts 1 & 2a) */
/* Converts to completely processed output readings, without averaging or extracting */
/* sample patches, for emissive measurement mode. */
/* This is used for delay & refresh rate measurement. */
munki_code munki_read_patches_all(
	munki *p,
	double **specrd,		/* Return array [numpatches][nwav] of spectral reading values */
	int numpatches,			/* Number of sample to measure */
	double *inttime, 		/* Integration time to use/used */
	int gainmode			/* Gain mode to use, 0 = normal, 1 = high */
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	int rv = 0;

	bsize = m->nsen * 2 * numpatches;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"munki_read_patches malloc %d bytes failed (11)\n",bsize);
		return MUNKI_INT_MALLOC;
	}

	/* Trigger measure and gather raw readings */
	if ((ev = munki_read_patches_1(p, 0, numpatches, numpatches, inttime, gainmode,
	                                       NULL, buf, bsize)) != MUNKI_OK) { 
		free(buf);
		return ev;
	}

	/* Process the raw readings without averaging or extraction */
	if ((ev = munki_read_patches_2a(p, specrd, numpatches, *inttime, gainmode,
	                                                 buf, bsize)) != MUNKI_OK) {
		free(buf);
		return ev;
	}
	free(buf);
	return ev;
}

/* Take a trial emission measurement reading using the current mode. */
/* Used to determine if sensor is saturated, or not optimal */
/* in adaptive emission mode. */
munki_code munki_trialmeasure(
	munki *p,
	int *saturated,			/* Return nz if sensor is saturated */
	double *optscale,		/* Return scale for gain/int time to make optimal (may be NULL) */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	double targoscale		/* Ratio of optimal sensor value to aim for */
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;
	double **multimes;		/* Multiple measurement results */
	double *absraw;		/* Linearsised absolute sensor raw values */
	int nmeasuered;			/* Number actually measured */
	double sensavg;			/* Overall average of sensor readings */
	double darkthresh;		/* Dark threshold */
	double trackmax[2];		/* Track optimum target */
	double maxval;			/* Maximum multimeas value */
	int rv;

	if (s->reflective) {
		a1logw(p->log, "munki_trialmeasure: Assert - not meant to be used for reflective read!\n");
		return MUNKI_INT_ASSERT;
	}
	
	if (nummeas <= 0)
		return MUNKI_INT_ZEROMEASURES;
		
	/* Allocate up front to avoid delay between trigger and read */
	bsize = m->nsen * 2 * nummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		a1logd(p->log,1,"munki_trialmeasure malloc %d bytes failed (12)\n",bsize);
		return MUNKI_INT_MALLOC;
	}
	multimes = dmatrix(0, nummeas-1, -1, m->nraw-1);
	absraw = dvector(-1, m->nraw-1);

	a1logd(p->log,3,"Triggering measurement cycle, nummeas %d, inttime %f, gainmode %d\n",
	                                              nummeas, *inttime, gainmode);

	if ((ev = munki_trigger_one_measure(p, nummeas, inttime, gainmode, 1, 0)) != MUNKI_OK) {
		free_dvector(absraw, -1, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free(buf);
		return ev;
	}

	a1logd(p->log,3,"Gathering readings\n");
	if ((ev = munki_readmeasurement(p, nummeas, m->c_measmodeflags & MUNKI_MMF_SCAN,
	                                             buf, bsize, &nmeasuered, 1, 0)) != MUNKI_OK) {
		free_dvector(absraw, -1, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		free(buf);
		return ev;
	}

	if (saturated != NULL)			/* Initialize return flag */
		*saturated = 0;

	/* Take a buffer full of raw readings, and convert them to */
	/* floating point sensor readings. Check for saturation */
	if ((rv = munki_sens_to_raw(p, multimes, NULL, buf, 0, nmeasuered, m->satlimit,
		                                                 &darkthresh)) != MUNKI_OK) {
	 	if (rv != MUNKI_RD_SENSORSATURATED) {
			free(buf);
			return rv;
		}
	 	if (saturated != NULL)
			*saturated = 1;
	}
	free(buf);

	/* Comute dark subtraction for this trial's parameters */
	if ((ev = munki_interp_dark(p, s->dark_data, *inttime, gainmode)) != MUNKI_OK) {
		free_dvector(absraw, -1, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);
		a1logd(p->log,3,"munki_imp_measure interplate dark ref failed\n");
		return ev;
	}

	trackmax[0] = darkthresh;		/* Track dark threshold */
	trackmax[1] = m->optsval;		/* Track the optimal sensor target value */

	/* Subtract the black from sensor values and convert to */
	/* absolute (integration & gain scaled), zero offset based, */
	/* linearized sensor values. */
	/* Return the highest individual element. */
	munki_sub_raw_to_absraw(p, nmeasuered, *inttime, gainmode, multimes, s->dark_data,
	                             trackmax, 2, &maxval);
	darkthresh = trackmax[0];

	/* Average a set of measurements into one. */
	/* Return nz if the readings are not consistent */
	/* Return the overall average. */
	rv = munki_average_multimeas(p, absraw, multimes, nmeasuered, &sensavg, darkthresh);     

	/* Ignore iconsistent readings ?? */

#ifdef PLOT_DEBUG
	printf("Average absolute sensor readings, average = %f, max %f\n",sensavg,maxval);
	plot_raw(absraw);
#endif

	if (optscale != NULL) {
		double opttarget;		/* Optimal sensor target */

		opttarget = targoscale * trackmax[1];
		if (maxval < 0.01)		/* Could go -ve */
			maxval = 0.01;
		*optscale = opttarget/ maxval; 
		a1logd(p->log,4,"Targscale %f, maxval %f, optimal target = %f, amount to scale = %f\n",
		     targoscale, maxval, opttarget, *optscale);
	}

	free_dvector(absraw, -1, m->nraw-1);
	free_dmatrix(multimes, 0, nummeas-1, -1, m->nraw-1);		/* Free after using *pmax */

	return ev;
}

/* Trigger a single measurement cycle. This could be a dark calibration, */
/* a calibration, or a real measurement. This is used to create the */
/* higher level "calibrate" and "take reading" functions. */
/* The setup for the operation is in the current mode state. */
/* Call munki_readmeasurement() to collect the results */
munki_code
munki_trigger_one_measure(
	munki *p,
	int nummeas,			/* Minimum number of measurements to make */
	double *inttime, 		/* Integration time to use/used */
	int gainmode,			/* Gain mode to use, 0 = normal, 1 = high */
	int calib_measure,		/* flag - nz if this is a calibration measurement */
	int dark_measure		/* flag - nz if this is a dark measurement */
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	double dintclocks;
	int intclocks;		/* Number of integration clocks */
	int measmodeflags;	/* Measurement mode command flags */
	int holdtempduty;	/* Hold temperature duty cycle */

	/* Compute integration clocks. Take account of (seeming) dead integration time */
	dintclocks = floor((*inttime)/m->intclkp + 0.5);
	intclocks = (int)dintclocks;
	*inttime = (double)intclocks * m->intclkp;		/* Quantized integration time */

	/* Create measurement mode flag byte for this operation */
	measmodeflags = 0;
	if (s->scan && !calib_measure)
		measmodeflags |= MUNKI_MMF_SCAN;		/* Never scan on a calibration */
	if (s->reflective && !dark_measure)
		measmodeflags |= MUNKI_MMF_LAMP;		/* Need lamp if reflective and not dark measure */
	if (gainmode == 1)
		measmodeflags |= MUNKI_MMF_HIGHGAIN;	/* High gain mode */
	holdtempduty = m->ledholdtempdc;			/* From the EEProm value */

	/* Trigger a measurement */
	if ((ev = munki_triggermeasure(p, intclocks, nummeas, measmodeflags, holdtempduty)) != MUNKI_OK)
		return ev;
	
	m->c_measmodeflags = measmodeflags;

	m->c_inttime = *inttime;

	return ev;
}

/* ============================================================ */
/* lower level reading processing and computation */

/* Take a buffer full of raw readings, and convert them to */
/* directly to floating point raw values. Return MUNKI_RD_SENSORSATURATED if any is saturated */
/* (No black subtraction or linearization is performed) */
munki_code munki_sens_to_raw(
	munki *p,
	double **raw,			/* Array of [nummeas-ninvalid][-1 nraw] value to return */
	double *ledtemp,		/* Optional array [nummeas-ninvalid] LED temperature values to return */
	unsigned char *buf,		/* Raw measurement data must be 274 * nummeas */
	int ninvalid,			/* Number of initial invalid readings to skip */
	int nummeas,			/* Number of readings measured */
	double satthresh,		/* Saturation threshold to trigger error in raw units (if > 0.0) */
	double *pdarkthresh		/* Return a dark threshold value = sheilded cell values */
) {
	munkiimp *m = (munkiimp *)p->m;
	int i, j, k;
	unsigned char *bp;
	double maxval = -1e38;
	double darkthresh = 0.0;
	double ndarkthresh = 0.0;
	int sskip = 2 * 6;		/* Bytes to skip at start */
	int eskip = 2 * 3;		/* Bytes to skip at end */

	if ((m->nraw * 2 + sskip + eskip) != (m->nsen * 2)) {
		a1logw(p->log,"NRAW %d and NRAWB %d don't match!\n",m->nraw,m->nsen * 2);
		return MUNKI_INT_ASSERT;
	}

	if (ninvalid > 0) 
		a1logd(p->log, 4, "munki_sens_to_raw: Skipping %d invalid readings\n",ninvalid);

	/* Now process the buffer values */
	for (bp = buf + ninvalid * m->nsen * 2, i = 0; i < nummeas; i++, bp += eskip) {

		/* The first 4 readings are shielded cells, and we use them as an */								/* estimate of the dark reading consistency, as well as for */
		/* compensating the dark level calibration for any temperature changes. */

		/* raw average of all measurement shielded cell values */
		for (k = 0; k < 4; k++) {
			darkthresh += (double)buf2ushort(bp + k * 2);
			ndarkthresh++;
		}

		/* raw of shielded cells per reading */
		raw[i][-1] = 0.0;
		for (k = 0; k < 4; k++) {
			raw[i][-1] += (double)buf2ushort(bp + k * 2);
		}
		raw[i][-1] /= 4.0;
		
		/* The LED voltage drop is the last 16 bits in each reading */
		if (ledtemp != NULL)
			ledtemp[i] = (double)buf2ushort(bp + (m->nsen * 2) - 2);

		/* The 128 raw spectral values */
		for (bp += sskip, j = 0; j < m->nraw; j++, bp += 2) {
			unsigned int rval;
			double fval;

			rval = buf2ushort(bp);
			fval = (double)rval;
			raw[i][j] = fval;
//			printf("~1 i = %d, j = %d, addr % 274 = %d, val = %f\n",i,j,(bp - buf) % 274, fval); 

			if (fval > maxval)
				maxval = fval;
		}
	}

	/* Check if any are over saturation threshold */
	if (satthresh > 0.0) {
		if (maxval > satthresh) {
			a1logd(p->log,4,"munki_sens_to_raw: Max sens %f > satthresh %f\n",maxval,satthresh);
			return MUNKI_RD_SENSORSATURATED;
		}
		a1logd(p->log,4,"munki_sens_to_raw: Max sens %f < satthresh %f\n",maxval,satthresh);
	}

	darkthresh /= ndarkthresh;
	if (pdarkthresh != NULL)
		*pdarkthresh = darkthresh;
	a1logd(p->log,3,"munki_sens_to_raw: Dark thrheshold = %f\n",darkthresh);

	return MUNKI_OK;
}

/* Subtract the black from raw values and convert to */
/* absolute (integration & gain scaled), zero offset based, */
/* linearized raw values. */
/* Return the highest individual element. */
void munki_sub_raw_to_absraw(
	munki *p,
	int nummeas,			/* Return number of readings measured */
	double inttime, 		/* Integration time used */
	int gainmode,			/* Gain mode, 0 = normal, 1 = high */
	double **absraw,		/* Source/Desination array [-1 nraw] */
	double *sub,			/* Value to subtract [-1 nraw] (ie. cal dark data) */
	double *trackmax, 		/* absraw values that should be offset the same as max */
	int ntrackmax,			/* Number of trackmax values */
	double *maxv			/* If not NULL, return the maximum value */
) {
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	int npoly;			/* Number of linearisation coefficients */
	double *polys;		/* the coeficients */
	double scale;		/* Absolute scale value */
	double submax = -1e6;	/* Subtraction value maximum */
	double asub[NSEN_MAX];
	double avgscell, zero;
	double rawmax, maxval = -1e38;
	double maxzero = 0.0;
	int i, j, k;

	/* Heusristic correction for LED interference bump for 0.018 secs int_time */
	int    pos[] = { 0,    20,   56,   62,   75,   127 };
//	double off[] = { 0.7, 0.0,  0.6, -0.9,  -1.2, -0.7 };
	double off[] = { 0.7, 0.0,  0.6, -0.9,  -0.8, -0.5 };
	
	if (gainmode) {				/* High gain */
		npoly = m->nlin1;		/* Encodes gain too */
		polys = m->lin1;
	} else {					/* Low gain */
		npoly = m->nlin0;
		polys = m->lin0;
	}
	scale = 1.0/inttime;

	/* Adjust black to allow for temperature change by using the */
	/* shielded cell values as a reference. */
	/* We use a heuristic to compute a zero based scale for adjusting the */
	/* black. It's not clear why it works best this way, or how */
	/* dependent on the particular instrument the magic numbers are, */
	/* but it reduces the black level error from over 10% to about 0.3% */

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
	zero = 1.08 * 0.5 * (avgscell + sub[-1]);

	/* make sure that the zero point is above any black value */
	if (zero < (1.005 * avgscell))
		zero = 1.005 * avgscell;
	if (zero < (1.005 * sub[-1]))
		zero = 1.005 * sub[-1];
	if (zero < (1.005 * submax))
		zero = 1.005 * submax;

	a1logd(p->log,4,"Black shielded value = %f, Reading shielded value = %f\n",sub[-1], avgscell);

	/* Compute the adjusted black for each band */
	if (s->reflective) {

		/* It seems that having the LED on shifts the shielded cell values */
		/* by about 2.5, and this stuffs up the reflective measurement. */
		/* This seems to be from the LED PWM driver, which perhaps */
		/* is synchronous to the sensor clock, and so switches */
		/* at a certain point in the transfer of data from the sensor. */
		/* The result is a step up from 0-60, and then down from 61-128. */
		/* Perhaps altering the LED PWM setting and seeing if this point */
		/* shifts would be a way of confirming this ? */
		/* There is also some stray light reflected into the sensor */
		/* from the LED, but due to the LED step, the sensor reading is */
		/* less than the dark data at some wavelengths. */
		/* The details of the LED step seem to be integration time dependent, */
		/* but decresing the scanning rate therebye increasing integration */
		/* time and light level reduces the impact of this error. */

		/* Since we do an on the fly black measurement before each */
		/* reflective measurement, ignoring the shielded cell values */
		/* shouldn't affect accuracy so much. */

#ifdef ENABLE_REFLEDINTER
		/* A heuristic to correct for the LED noise. */
		/* This is only valid for int_time of 0.0182 secs, */
		/* and it's not clear how well it works across different */
		/* temperatures or examples of the ColorMunki. */
		/* in another revision ?? */
		for (j = 0; j < m->nraw; j++) {
			int ix;
			double bl, val;

			for (ix = 0; ; ix++) {
				if (j >= pos[ix] && j <= pos[ix+1]) 
					break;
			}
			bl = (j - pos[ix])/((double)pos[ix+1] - pos[ix]);
			val = (1.0 - bl) * off[ix] + bl * off[ix+1];
			asub[j] = sub[j] + val;
		}
#else
		for (j = 0; j < m->nraw; j++)
			asub[j] = sub[j];		/* Just use the calibration dark data */
#endif

	} else {
		/* No LED on operation - use sheilded cell values */
		for (j = 0; j < m->nraw; j++) {
#ifdef ENABLE_BKDRIFTC

# ifdef HEURISTIC_BKDRIFTC
			/* heuristic scaled correction */
			asub[j] = zero - (zero - sub[j]) * (zero - avgscell)/(zero - sub[-1]);
# else
			/* simple additive correction */
#  pragma message("######### munki Simple shielded cell temperature correction! ########")
			asub[j] = sub[j] + (avgscell - sub[-1]);
# endif
#else
#  pragma message("######### munki No shielded cell temperature correction! ########")
			asub[j] = sub[j];		/* Just use the calibration dark data */
#endif
		}
	}

#if defined(PLOT_DEBUG) || defined(DUMP_BKLED)
	{
	double xx[130];
	double yy[3][130];

	for (j = -1; j < m->nraw+1; j++)
		yy[0][j+1] = 0.0;

	for (i = 0; i < nummeas; i++) {
		for (j = -1; j < m->nraw; j++)
			yy[0][j+1] += absraw[i][j];
	}
	for (j = -1; j < m->nraw; j++)
		yy[0][j+1] /= (double)nummeas;

	for (j = -1; j < m->nraw; j++)
		yy[1][j+1]= sub[j];

	/* Show what ENABLE_REFLEDINTER would do */
	for (j = 0; j < m->nraw; j++) {
		int ix;
		double bl, val;

		for (ix = 0; ; ix++) {
			if (j >= pos[ix] && j <= pos[ix+1]) 
				break;
		}
		bl = (j - pos[ix])/((double)pos[ix+1] - pos[ix]);
		val = (1.0 - bl) * off[ix] + bl * off[ix+1];
		yy[2][j+1] = yy[0][j+1] - val;
	}
	yy[2][0] = yy[0][0];

	for (j = -1; j < m->nraw; j++)
		xx[j+1] = (double)j;

	xx[0]= -10.0;

# ifdef PLOT_DEBUG
	printf("sub_raw_to_absraw %d samp avg - dark ref:\n",nummeas);
	do_plot(xx, yy[0], yy[1], yy[2], 129);
# endif
# ifdef DUMP_BKLED
	{
		xspect sp[3];
		for (i = 0; i < 3; i++) {
			sp[i].spec_n = 128;
			sp[i].spec_wl_short = 0.0;
			sp[i].spec_wl_long = 127.0;
			sp[i].norm = 1.0;
			for (j = 0; j < 128; j++)
				sp[i].spec[j] = yy[i][j+1];
		}
		write_nxspect("refbk1.txt", sp, 3, 0);
	}
# endif	/* DUMP_BKLED */
	}
#endif	/* PLOT_DEBUG || DUMP_BKLED */

	/* For each measurement */
	for (i = 0; i < nummeas; i++) {
		double rval, sval, lval;
	
		for (j = 0; j < m->nraw; j++) {

			rval = absraw[i][j];

			sval = rval - asub[j];		/* Make zero based */

#ifdef ENABLE_NONLINCOR	
			/* Linearise */
			for (lval = polys[npoly-1], k = npoly-2; k >= 0; k--)
				lval = lval * sval + polys[k];
#else
			lval = sval;
#endif
			lval *= scale;
			absraw[i][j] = lval;

			/* Track the maximum value and the black that was subtracted from it */ 
			if (lval > maxval) {
				maxval = lval;
				rawmax = rval;
				maxzero = asub[j];
				if (maxv != NULL)
					*maxv = absraw[i][j];
			}
		}
	}

	/* Process the "tracked to max" values too */
	if (ntrackmax > 0 && trackmax != NULL)  {
		for (i = 0; i < ntrackmax; i++) {
			double rval, fval, lval;

			rval = trackmax[i];
			fval = rval - maxzero;

#ifdef ENABLE_NONLINCOR	
			/* Linearise */
			for (lval = polys[npoly-1], k = npoly-2; k >= 0; k--)
				lval = lval * fval + polys[k];
#else
			lval = fval;
#endif
			lval *= scale;
			trackmax[i] = lval;
//			printf("~1 trackmax[%d] = %f, maxzero = %f\n",i,lval,maxzero);
		}
	}
}

/* Average a set of sens or absens measurements into one. */
/* (Make sure darkthresh is tracked if absens is being averaged!) */
/* Return zero if readings are consistent and not saturated. */
/* Return nz if the readings are not consistent */
/* Return the overall average. */
int munki_average_multimeas(
	munki *p,
	double *avg,			/* return average [-1 nraw] */
	double **multimeas,		/* Array of [nummeas][-1 nraw] value to average */
	int nummeas,			/* number of readings to be averaged */
	double *poallavg,		/* If not NULL, return overall average of bands and measurements */
	double darkthresh		/* Dark threshold (used for consistency check scaling) */
) {
	munkiimp *m = (munkiimp *)p->m;
	int i, j;
	double oallavg = 0.0;
	double maxavg = -1e38;		/* Track min and max averages of readings */
	double minavg = 1e38;
	double norm;
	int rv = 0;
	
	a1logd(p->log,3,"munki_average_multimeas %d readings (darkthresh %f)\n",nummeas,darkthresh);

	for (j = -1; j < m->nraw; j++) 
		avg[j] = 0.0;

	/* Now process the buffer values */
	for (i = 0; i < nummeas; i++) {
		double measavg = 0.0;

		avg[-1] += multimeas[i][-1];			/* shielded cell value */

		for (j = 0; j < m->nraw; j++) {
			double val;

			val = multimeas[i][j];

			measavg += val;
			avg[j] += val;
		}
		measavg /= (double)m->nraw;
		oallavg += measavg;
		if (measavg < minavg)
			minavg = measavg;
		if (measavg > maxavg)
			maxavg = measavg;
	}

	for (j = -1; j < m->nraw; j++) 
		avg[j] /= (double)nummeas;
	oallavg /= (double)nummeas;

	if (poallavg != NULL)
		*poallavg = oallavg;

	norm = fabs(0.5 * (maxavg+minavg));
	darkthresh = fabs(darkthresh);
	if (darkthresh < DARKTHSCAMIN)
		darkthresh = DARKTHSCAMIN;
	a1logd(p->log,3,"norm = %f, dark thresh = %f\n",norm,darkthresh);
	if (norm < (2.0 * darkthresh))
		norm = 2.0 * darkthresh;

	a1logd(p->log,4,"avg_multi: overall avg = %f, minavg = %f, maxavg = %f, variance %f, THR %f (darkth %f)\n",
	                   oallavg,minavg,maxavg,(maxavg - minavg)/norm, PATCH_CONS_THR,darkthresh);
	if ((maxavg - minavg)/norm > PATCH_CONS_THR) {
		rv |= 1;
	}
	return rv;
}

/* Minimum number of scan samples in a patch */
#define MIN_SAMPLES 2

/* Range of bands to detect transitions */
#define BL 5	/* Start */
#define BH 105	/* End */
#define BW 5	/* Width */

/* Record of possible patch within a reading buffer */
typedef struct {
	int ss;				/* Start sample index */
	int no;				/* Number of samples */
	int use;			/* nz if patch is to be used */
} munki_patch;

/* Recognise the required number of ref/trans patch locations, */
/* and average the measurements within each patch. */
/* *flags returns zero if readings are consistent. */
/* *flags returns nz if the readings are not consistent */
/* (Doesn't extract [-1] shielded values, since they have already been used) */
munki_code munki_extract_patches_multimeas(
	munki *p,
	int *flags,				/* return flags */
	double **pavg,			/* return patch average [naptch][-1 nraw] */
	int tnpatch,			/* Target number of patches to recognise */
	double **multimeas,		/* Array of [nummeas][-1 nraw] value to extract from */
	int nummeas,			/* number of readings made */
	double inttime			/* Integration time (used to adjust consistency threshold) */
) {
	munkiimp *m = (munkiimp *)p->m;
	int i, j, k, pix;
	double **sslope;			/* Signed difference between i and i+1 */
	double *slope;				/* Accumulated absolute difference between i and i+1 */
	double *fslope;				/* Filtered slope */
	munki_patch *pat;			/* Possible patch information */
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
	double white_avg;			/* Average of (aproximate) white data */
	int rv = 0;
	double patch_cons_thr = PATCH_CONS_THR * m->scan_toll_ratio;
#ifdef PLOT_PATREC
	double **plot;
#endif

	a1logd(p->log,3,"munki_extract_patches_multimeas: looking for %d patches out of %d samples\n",tnpatch,nummeas);

	maxval = dvectorz(-1, m->nraw-1);  

	/* Loosen consistency threshold for short intergation time */
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

#ifdef PLOT_PATREC
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

#ifdef NEVER
	/* Plot the shielded cell value */
	for (i = 0; i < nummeas; i++) { 
		plot[0][i] = multimeas[i][-1];
		plot[6][i] = (double)i;
	}
	printf("Sheilded values\n");
	do_plot6(plot[6], plot[0], NULL, NULL, NULL, NULL, NULL, nummeas);
#endif

	sslope = dmatrixz(0, nummeas-1, -1, m->nraw-1);  
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

#ifdef PLOT_PATREC
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
	free_dmatrix(sslope, 0, nummeas-1, -1, m->nraw-1);  

	/* Now threshold the measurements into possible patches */
	apat = 2 * nummeas;
	if ((pat = (munki_patch *)malloc(sizeof(munki_patch) * apat)) == NULL) {
		a1logd(p->log,1,"munki: malloc of patch structures failed!\n");
		return MUNKI_INT_MALLOC;
	}

	avglegth = 0.0;
	for (npat = i = 0; i < (nummeas-1); i++) {
		if (slope[i] > thresh)
			continue;

		/* Start of a new patch */
		if (npat >= apat) {
			apat *= 2;
			if ((pat = (munki_patch *)realloc(pat, sizeof(munki_patch) * apat)) == NULL) {
				a1logd(p->log,1,"munki: reallloc of patch structures failed!\n");
				return MUNKI_INT_MALLOC;
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

	/* We don't count the first and last patches, as we assume they are white leader. */
	/* (They are marked !use in list anyway) */
	if (npat < (tnpatch + 2)) {
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, -1, m->nraw-1);  
		free(pat);
		a1logd(p->log,1,"Patch recog failed - unable to detect enough possible patches\n");
		return MUNKI_RD_NOTENOUGHPATCHES;
	} else if (npat >= (2 * tnpatch) + 2) {
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, -1, m->nraw-1);  
		free(pat);
		a1logd(p->log,1,"Patch recog failed - detecting too many possible patches\n");
		return MUNKI_RD_TOOMANYPATCHES;
	}
	avglegth /= (double)npat;

#ifdef PLOT_PATREC
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
		if (j > ((npat-2)/2))
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
						a1logd(p->log,1,"Patch recog failed - patches sampled too sparsely\n");
						return MUNKI_RD_NOTENOUGHSAMPLES;
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
			a1logd(p->log,1,"Patch recog failed - detected too many consistent patches\n");
			return MUNKI_RD_TOOMANYPATCHES;
		}
	}
	if (try >= 15) {
		a1logd(p->log,7,"Not enough patches\n");
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, -1, m->nraw-1);  
		free(pat);
		a1logd(p->log,1,"Patch recog failed - unable to find enough consistent patches\n");
		return MUNKI_RD_NOTENOUGHPATCHES;
	}

#ifdef PLOT_PATREC
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
//		nno = (pat[k].no * 2)/3;		// experimental tighter trim
		trim = (pat[k].no - nno)/2;

		pat[k].ss += trim;
		pat[k].no = nno;
	}

#ifdef PLOT_PATREC
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

	printf("Trimmed output - averaged bands:\n");
	/* Plot box averaged bands */
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

#ifdef NEVER
	/* Plot all the bands */
	printf("Trimmed output - all bands:\n");
	for (j = 0; j < (m->nraw-5); j += 5) {
		for (k = 0; k < 5; k ++) {
			for (i = 0; i < nummeas; i++) { 
				plot[k][i] = multimeas[i][j+k]/maxval[j+k];
			}
		}
		for (i = 0; i < nummeas; i++)
			plot[6][i] = (double)i;
		printf("Bands %d - %d\n",j,j+5);
		do_plot6(plot[6], slope, plot[0], plot[1], plot[2], plot[3], plot[4], nummeas);
	}
#endif

#endif

	/* Now compute averaged patch values */
	
	/* Compute average of (aproximate) white */
	white_avg = 0.0;
	for (j = 1; j < (m->nraw-1); j++) 
		white_avg += maxval[j];
	white_avg /= (m->nraw - 2.0);

	/* Now process the buffer values */
	for (i = 0; i < tnpatch; i++)
		for (j = 0; j < m->nraw; j++) 
			pavg[i][j] = 0.0;

	for (pix = 0, k = 1; k < (npat-1); k++) {
		double maxavg = -1e38;	/* Track min and max averages of readings for consistency */
		double minavg = 1e38;
		double cons;			/* Consistency */

		if (pat[k].use == 0)
			continue;

		if (pat[k].no <= MIN_SAMPLES) {
			a1logd(p->log,7,"Too few samples\n");
			free_dvector(slope, 0, nummeas-1);  
			free_ivector(sizepop, 0, nummeas-1);
			free_dvector(maxval, -1, m->nraw-1);  
			free(pat);
			a1logd(p->log,1,"Patch recog failed - patches sampled too sparsely\n");
			return MUNKI_RD_NOTENOUGHSAMPLES;
		}

		/* Measure samples that make up patch value */
		for (i = pat[k].ss; i < (pat[k].ss + pat[k].no); i++) {
			double measavg = 0.0;

			for (j = 0; j < m->nraw; j++) {
				double val;

				val = multimeas[i][j];

				measavg += val;
				pavg[pix][j] += val;
			}
			measavg /= (m->nraw-2.0);
			if (measavg < minavg)
				minavg = measavg;
			if (measavg > maxavg)
				maxavg = measavg;
		}

		for (j = 0; j < m->nraw; j++) 
			pavg[pix][j] /= (double)pat[k].no;

		cons = (maxavg - minavg)/white_avg;
		a1logd(p->log,7,"Patch %d: consistency = %f%%, thresh = %f%%\n",pix,100.0 * cons, 100.0 * patch_cons_thr);
		if (cons > patch_cons_thr) {
			a1logd(p->log,1,"Patch recog failed - patch %d is inconsistent (%f%%)\n",pix, cons);
			rv |= 1;
		}
		pix++;
	}

	if (flags != NULL)
		*flags = rv;

#ifdef PLOT_PATREC
	free_dmatrix(plot, 0, 6, 0, nummeas-1);  
#endif

	free_dvector(slope, 0, nummeas-1);  
	free_ivector(sizepop, 0, nummeas-1);
	free_dvector(maxval, -1, m->nraw-1);  
	free(pat);		/* Otherwise caller will have to do it */

	a1logd(p->log,3,"munki_extract_patches_multimeas done, sat = %s, inconsist = %s\n",
	                  rv & 2 ? "true" : "false", rv & 1 ? "true" : "false");

	a1logd(p->log,2,"Patch recognition returning OK\n");

	return MUNKI_OK;
}

#undef BL
#undef BH
#undef BW

/* Recognise any flashes in the readings, and */
/* and average their values together as well as summing their duration. */
/* The readings are integrated, so the the units are cd/m^2 seconds. */
/* Return nz on an error */
/* (Doesn't extract [-1] shielded values, since they have already been used) */
munki_code munki_extract_patches_flash(
	munki *p,
	int *flags,				/* return flags */
	double *duration,		/* return duration */
	double *pavg,			/* return patch average [-1 nraw] */
	double **multimeas,		/* Array of [nummeas][-1 nraw] value to extract from */
	int nummeas,			/* number of readings made */
	double inttime			/* Integration time (used to compute duration) */
) {
	munkiimp *m = (munkiimp *)p->m;
	int i, j, k;
	double minval, maxval;		/* min and max input value at wavelength of maximum input */
	double mean;				/* Mean of the max wavelength band */
	int maxband;				/* Band of maximum value */
	double thresh;				/* Level threshold */
	int fsampl;					/* Index of the first sample over the threshold */
	int nsampl;					/* Number of samples over the threshold */
	double *aavg;				/* ambient average [-1 nraw] */
	double finttime;			/* Flash integration time */
	int rv = 0;
#ifdef PLOT_PATREC
	double **plot;
#endif

	a1logd(p->log,3,"munki_extract_patches_flash: %d measurements\n",nummeas);

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
		a1logd(p->log,1,"No flashes found in measurement\n");
		return MUNKI_RD_NOFLASHES;
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
	a1logd(p->log,7,"munki_extract_patches_flash band %d minval %f maxval %f, mean = %f, thresh = %f\n",maxband,minval,maxval,mean, thresh);

#ifdef PLOT_PATREC
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

#ifdef PLOT_PATREC
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
		return MUNKI_RD_NOFLASHES;

	/* See if there are as many samples before the first flash */
	if (nsampl < 6)
		nsampl = 6;

	/* Average nsample samples of ambient */
	i = (fsampl-3-nsampl);
	if (i < 0)
		return MUNKI_RD_NOAMBB4FLASHES;
	a1logd(p->log,7,"Ambient samples %d to %d \n",i,fsampl-3);
	aavg = dvectorz(-1, m->nraw-1);  
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

	return MUNKI_OK;
}

/* Convert an absraw array from raw wavelengths to output wavelenths */
/* for the current resolution. Apply stray light compensation too. */
void munki_absraw_to_abswav(
	munki *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav] */
	double **absraw			/* Source array [-1 nraw] */
) {
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	double *tm;			/* Temporary array */
	int i, j, k, cx, sx;
	
	tm = dvector(0, m->nwav-1);

	/* For each measurement */
	for (i = 0; i < nummeas; i++) {

		/* For each output wavelength */
		for (cx = j = 0; j < m->nwav; j++) {
			double oval = 0.0;
	
			/* For each matrix value */
			if (s->reflective) {
				sx = m->rmtx_index[j];		/* Starting index */
				for (k = 0; k < m->rmtx_nocoef[j]; k++, cx++, sx++)
					oval += m->rmtx_coef[cx] * absraw[i][sx];
			} else {
				sx = m->emtx_index[j];		/* Starting index */
				for (k = 0; k < m->emtx_nocoef[j]; k++, cx++, sx++)
					oval += m->emtx_coef[cx] * absraw[i][sx];
			}
			tm[j] = oval;
		}

		/* Now apply stray light compensation */ 
		/* For each output wavelength */
		for (j = 0; j < m->nwav; j++) {
			double oval = 0.0;
	
			/* For each matrix value */
			for (k = 0; k < m->nwav; k++)
				oval += m->straylight[j][k] * tm[k];
			abswav[i][j] = oval;
		}
	}
	free_dvector(tm, 0, m->nwav-1);
}

/* Convert an absraw array from raw wavelengths to output wavelenths */
/* for the standard resolution. Apply stray light compensation too. */
void munki_absraw_to_abswav1(
	munki *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav1] */
	double **absraw		/* Source array [-1 nraw] */
) {
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	double *tm;			/* Temporary array */
	int i, j, k, cx, sx;
	
	tm = dvector(0, m->nwav1-1);

	/* For each measurement */
	for (i = 0; i < nummeas; i++) {

		/* For each output wavelength */
		for (cx = j = 0; j < m->nwav1; j++) {
			double oval = 0.0;
	
			/* For each matrix value */
			if (s->reflective) {
				sx = m->rmtx_index1[j];		/* Starting index */
				for (k = 0; k < m->rmtx_nocoef1[j]; k++, cx++, sx++)
					oval += m->rmtx_coef1[cx] * absraw[i][sx];
			} else {
				sx = m->emtx_index1[j];		/* Starting index */
				for (k = 0; k < m->emtx_nocoef1[j]; k++, cx++, sx++)
					oval += m->emtx_coef1[cx] * absraw[i][sx];
			}
			tm[j] = oval;
		}

		/* Now apply stray light compensation */ 
		/* For each output wavelength */
		for (j = 0; j < m->nwav1; j++) {
			double oval = 0.0;
	
			/* For each matrix value */
			for (k = 0; k < m->nwav1; k++)
				oval += m->straylight1[j][k] * tm[k];
			abswav[i][j] = oval;
		}
	}
	free_dvector(tm, 0, m->nwav1-1);
}

/* Convert an absraw array from raw wavelengths to output wavelenths */
/* for the high resolution. Apply light compensation too. */
void munki_absraw_to_abswav2(
	munki *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav2] */
	double **absraw			/* Source array [-1 nraw] */
) {
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	double *tm;			/* Temporary array */
	int i, j, k, cx, sx;
	
	tm = dvector(0, m->nwav2-1);

	/* For each measurement */
	for (i = 0; i < nummeas; i++) {

		/* For each output wavelength */
		for (cx = j = 0; j < m->nwav2; j++) {
			double oval = 0.0;
	
			/* For each matrix value */
			if (s->reflective) {
				sx = m->rmtx_index2[j];		/* Starting index */
				for (k = 0; k < m->rmtx_nocoef2[j]; k++, cx++, sx++)
					oval += m->rmtx_coef2[cx] * absraw[i][sx];
			} else {
				sx = m->emtx_index2[j];		/* Starting index */
				for (k = 0; k < m->emtx_nocoef2[j]; k++, cx++, sx++)
					oval += m->emtx_coef2[cx] * absraw[i][sx];
			}
			tm[j] = oval;
		}

		/* Now apply stray light compensation */ 
		/* For each output wavelength */
		for (j = 0; j < m->nwav2; j++) {
			double oval = 0.0;
	
			/* For each matrix value */
			for (k = 0; k < m->nwav2; k++)
				oval += m->straylight2[j][k] * tm[k];
			abswav[i][j] = oval;
		}
	}
	free_dvector(tm, 0, m->nwav2-1);
}

/* Convert an abswav array of output wavelengths to scaled output readings. */
void munki_scale_specrd(
	munki *p,
	double **outspecrd,		/* Destination */
	int numpatches,			/* Number of readings/patches */
	double **inspecrd		/* Source */
) {
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
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
#undef EXISTING_SHAPE		/* [und] Else generate filter shape */
#define USE_GAUSSIAN		/* [def] Use gaussian filter shape, else lanczos2 */

#define DO_CCDNORM			/* [def] Normalise CCD values to original */
#define DO_CCDNORMAVG		/* [unde] Normalise averages rather than per CCD bin */

#ifdef NEVER
/* Plot the matrix coefficients */
void munki_debug_plot_mtx_coef(munki *p, int ref) {
	munkiimp *m = (munkiimp *)p->m;
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
		if (ref) {
			sx = m->rmtx_index[j];		/* Starting index */
//			printf("start index = %d, nocoef %d\n",sx,m->rmtx_nocoef[j]);
			for (k = 0; k < m->rmtx_nocoef[j]; k++, cx++, sx++) {
//				printf("offset %d, coef ix %d val %f from ccd %d\n",k, cx, m->rmtx_coef[cx], sx);
				yy[5][sx] += 0.5 * m->rmtx_coef[cx];
				yy[i][sx] = m->rmtx_coef[cx];
			}
		} else {
			sx = m->emtx_index[j];		/* Starting index */
//			printf("start index = %d, nocoef %d\n",sx,m->emtx_nocoef[j]);
			for (k = 0; k < m->emtx_nocoef[j]; k++, cx++, sx++) {
//				printf("offset %d, coef ix %d val %f from ccd %d\n",k, cx, m->emtx_coef[cx], sx);
				yy[5][sx] += 0.5 * m->emtx_coef[cx];
				yy[i][sx] = m->emtx_coef[cx];
			}
		}
	}

	if (ref)
		printf("Reflective cooeficients\n");
	else
		printf("Emissive cooeficients\n");
	do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
	free_dvector(xx, -1, m->nraw-1);
	free_dmatrix(yy, 0, 2, -1, m->nraw-1);
}
#endif

/* Filter shape point */
typedef	struct {
	double wl, we;
} munki_fs;

/* Filter cooeficient values */
typedef struct {
	int ix;				/* Raw index */
	double we;			/* Weighting */
} munki_fc;

/* Wavelenth calibration crossover point information */
typedef struct {
	double wav;				/* Wavelegth of point */
	double raw;				/* Raw index of point */		
	double wei;				/* Weigting of the point */
} munki_xp;

/* Linearly interpolate the filter shape */
static double lin_fshape(munki_fs *fsh, int n, double x) {
	int i;
	double y;

	if (x <= fsh[0].wl)
		return fsh[0].we;
	else if (x >= fsh[n-1].wl)
		return fsh[n-1].we;

	for (i = 0; i < (n-1); i++)
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

#ifdef USE_GAUSSIAN
	/* gausian */
	wi = wi/(2.0 * sqrt(2.0 * log(2.0)));	/* Convert width at half max to std. dev. */
    x = x/(sqrt(2.0) * wi);
//	y = 1.0/(wi * sqrt(2.0 * DBL_PI)) * exp(-(x * x));		/* Unity area */
	y = exp(-(x * x));										/* Center at 1.0 */
#else

	/* lanczos2 */
	x = fabs(1.0 * x/wi);
	if (x >= 2.0)
		return 0.0; 
	if (x < 1e-5)
		return 1.0;
	y = sin(DBL_PI * x)/(DBL_PI * x) * sin(DBL_PI * x/2.0)/(DBL_PI * x/2.0);
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

#ifdef SALONEINSTLIB
# define ONEDSTRAYLIGHTUS
#endif

/* Create high resolution mode references, */
/* Create Reflective if ref nz, else create Emissive */
/* We expect this to be called twice, once for each. */
munki_code munki_create_hr(munki *p, int ref) {
	munkiimp *m = (munkiimp *)p->m;
	int i, j, jj, k, cx, sx;
	munki_fc coeff[40][16];	/* Existing filter cooefficients */
	int nwav1;					/* Number of filters */
	double wl_short1, wl_long1;	/* Ouput wavelength of first and last filters */
	double wl_step1;
	munki_xp xp[41];			/* Crossover points each side of filter */
	munki_code ev = MUNKI_OK;
	rspl *raw2wav;				/* Lookup from CCD index to wavelength */
	munki_fs fshape[40 * 16];  /* Existing filter shape */
	int ncp = 0;				/* Number of shape points */
	int *mtx_index1, **pmtx_index2, *mtx_index2;
	int *mtx_nocoef1, **pmtx_nocoef2, *mtx_nocoef2;
	double *mtx_coef1, **pmtx_coef2, *mtx_coef2;

	double min_wl = ref ? WL_REF_MIN : WL_EMIS_MIN;
	
	/* Start with nominal values. May alter these if filters are not unique */
	nwav1 = m->nwav1;
	wl_short1 = m->wl_short1;
	wl_long1 = m->wl_long1;
	wl_step1 = (wl_long1 - m->wl_short1)/(m->nwav1-1.0);

	if (ref) {
		mtx_index1 = m->rmtx_index1;
		mtx_nocoef1 = m->rmtx_nocoef1;
		mtx_coef1 = m->rmtx_coef1;
		mtx_index2 = NULL;
		mtx_nocoef2 = NULL;
		mtx_coef2 = NULL;
		pmtx_index2 = &m->rmtx_index2;
		pmtx_nocoef2 = &m->rmtx_nocoef2;
		pmtx_coef2 = &m->rmtx_coef2;
	} else {
		mtx_index1 = m->emtx_index1;
		mtx_nocoef1 = m->emtx_nocoef1;
		mtx_coef1 = m->emtx_coef1;
		mtx_index2 = NULL;
		mtx_nocoef2 = NULL;
		mtx_coef2 = NULL;
		pmtx_index2 = &m->emtx_index2;
		pmtx_nocoef2 = &m->emtx_nocoef2;
		pmtx_coef2 = &m->emtx_coef2;
	}
	
	/* Convert the native filter cooeficient representation to */
	/* a 2D array we can randomly index. Skip any duplicated */
	/* filter cooeficients. */
	for (cx = j = jj = 0; j < m->nwav1; j++) { /* For each output wavelength */
		if (j >= 40) {	/* Assert */
			a1logw(p->log,"munki: number of output wavelenths is > 40\n");
			return MUNKI_INT_ASSERT;
		}

		/* For each matrix value */
		sx = mtx_index1[j];		/* Starting index */
		if (j < (m->nwav1-1) && sx == mtx_index1[j+1]) {	/* Skip duplicates + last */
//			printf("~1 skipping %d\n",j);
			wl_short1 += wl_step1;
			nwav1--;
			cx += mtx_nocoef1[j];
			continue;
		}
		for (k = 0; k < mtx_nocoef1[j]; k++, cx++, sx++) {
			if (k >= 16) {	/* Assert */
				a1logw(p->log,"munki: number of filter coeefs is > 16\n");
				return MUNKI_INT_ASSERT;
			}

			coeff[jj][k].ix = sx;
			coeff[jj][k].we = mtx_coef1[cx];
		}
		jj++;
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
		for (j = 0; j < nwav1; j++) {
			i = j % 5;

			/* For each matrix value */
			for (k = 0; k < mtx_nocoef1[j]; k++) {
				yy[5][coeff[j][k].ix] += 0.5 * coeff[j][k].we;
				yy[i][coeff[j][k].ix] = coeff[j][k].we;
			}
		}

		if (ref)
			printf("Original reflection wavelength sampling curves:\n");
		else
			printf("Original emission wavelength sampling curves:\n");
		do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
		free_dvector(xx, -1, m->nraw-1);
		free_dmatrix(yy, 0, 2, -1, m->nraw-1);
	}
#endif /* HIGH_RES_PLOT */

//	a1logd(p->log,3,"computing crossover points\n");
	/* Compute the crossover points between each filter */
	for (i = 0; i < (nwav1-1); i++) {
		double den, y1, y2, y3, y4, yn, xn;	/* Location of intersection */
		double eps = 1e-6;			/* Numerical tollerance */
		double besty = -1e6;

		/* between filter i and i+1, we want to find the two */
		/* raw indexes where the weighting values cross over */
		/* Do a brute force search to avoid making assumptions */
		/* about the raw order. */
		for (j = 0; j < (mtx_nocoef1[i]-1); j++) {
			for (k = 0; k < (mtx_nocoef1[i+1]-1); k++) {
				if (coeff[i][j].ix == coeff[i+1][k].ix
				 && coeff[i][j+1].ix == coeff[i+1][k+1].ix) {

//					a1logd(p->log,3,"got it at %d, %d: %d = %d, %d = %d\n",j,k, coeff[i][j].ix, coeff[i+1][k].ix, coeff[i][j+1].ix, coeff[i+1][k+1].ix);

					/* Compute the intersection of the two line segments */
					y1 = coeff[i][j].we;
					y2 = coeff[i][j+1].we;
					y3 = coeff[i+1][k].we;
					y4 = coeff[i+1][k+1].we;
//					a1logd(p->log,3,"y1 %f, y2 %f, y3 %f, y4 %f\n",y1, y2, y3, y4);
					den = -y4 + y3 + y2 - y1;
					if (fabs(den) < eps)
						continue; 
					yn = (y2 * y3 - y1 * y4)/den;
					xn = (y3 - y1)/den;
					if (xn < -eps || xn > (1.0 + eps))
						continue;
//					a1logd(p->log,3,"den = %f, yn = %f, xn = %f\n",den,yn,xn);
					if (yn > besty) {
						xp[i+1].wav = XSPECT_WL(wl_short1, wl_long1, nwav1, i + 0.5);
						xp[i+1].raw = (1.0 - xn) * coeff[i][j].ix + xn * coeff[i][j+1].ix;
						xp[i+1].wei = yn;
						besty = yn;
//					a1logd(p->log,3,"Intersection %d: wav %f, raw %f, wei %f\n",i+1,xp[i+1].wav,xp[i+1].raw,xp[i+1].wei);
//					a1logd(p->log,3,"Found new best y %f\n",yn);
					}
//				a1logd(p->log,3,"\n");
				}
			}
		}
		if (besty < 0.0) {	/* Assert */
			a1logw(p->log,"munki: failed to locate crossover between resampling filters\n");
			return MUNKI_INT_ASSERT;
		}
//		a1logd(p->log,3,"\n");
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
		for (j = 0; j < (mtx_nocoef1[0]-1); j++) {
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
		if (j >= mtx_nocoef1[0]) {	/* Assert */
			a1logw(p->log,"munki: failed to find end crossover\n");
			return MUNKI_INT_ASSERT;
		}
		den = -y4 + y3 + y2 - y1;
		yn = (y2 * y3 - y1 * y4)/den;
		xn = (y3 - y1)/den;
//		a1logd(p->log,3,"den = %f, yn = %f, xn = %f\n",den,yn,xn);
		xp[0].wav = XSPECT_WL(wl_short1, wl_long1, nwav1, -0.5);
		xp[0].raw = (1.0 - xn) * coeff[0][j].ix + xn * coeff[0][j+1].ix;
		xp[0].wei = yn;
//		a1logd(p->log,3,"End 0 intersection %d: wav %f, raw %f, wei %f\n",0,xp[0].wav,xp[0].raw,xp[0].wei);
//		a1logd(p->log,3,"\n");

		x5 = xp[nwav1-2].raw;
		y5 = xp[nwav1-2].wei;
		x6 = xp[nwav1-1].raw;
		y6 = xp[nwav1-1].wei;

//		a1logd(p->log,3,"x5 %f, y5 %f, x6 %f, y6 %f\n",x5,y5,x6,y6);
		/* Search for possible intersection point with first curve */
		/* Create equation for line from next two intersection points */
		for (j = 0; j < (mtx_nocoef1[0]-1); j++) {
			/* Extrapolate line to this segment */
			y3 = y5 + (coeff[nwav1-1][j].ix - x5)/(x6 - x5) * (y6 - y5);
			y4 = y5 + (coeff[nwav1-1][j+1].ix - x5)/(x6 - x5) * (y6 - y5);
			/* This segment of curve */
			y1 = coeff[nwav1-1][j].we;
			y2 = coeff[nwav1-1][j+1].we;
			if ( ((  y1 >= y3 && y2 <= y4)		/* Segments overlap */
			   || (  y1 <= y3 && y2 >= y4))
			  && ((    coeff[nwav1-1][j].ix < x5 && coeff[nwav1-1][j].ix < x6
			        && coeff[nwav1-1][j+1].ix < x5 && coeff[nwav1-1][j+1].ix < x6)
			   || (    coeff[nwav1-1][j+1].ix > x5 && coeff[nwav1-1][j+1].ix > x6
				    && coeff[nwav1-1][j].ix > x5 && coeff[nwav1-1][j].ix > x6))) {
				break;
			}
		}
		if (j >= mtx_nocoef1[nwav1-1]) {	/* Assert */
			a1logw(p->log, "munki: failed to find end crossover\n");
			return MUNKI_INT_ASSERT;
		}
		den = -y4 + y3 + y2 - y1;
		yn = (y2 * y3 - y1 * y4)/den;
		xn = (y3 - y1)/den;
//		a1logd(p->log,3,"den = %f, yn = %f, xn = %f\n",den,yn,xn);
		xp[nwav1].wav = XSPECT_WL(wl_short1, wl_long1, nwav1, nwav1-0.5);
		xp[nwav1].raw = (1.0 - xn) * coeff[nwav1-1][j].ix + xn * coeff[nwav1-1][j+1].ix;
		xp[nwav1].wei = yn;
//		a1logd(p->log,3,"End 36 intersection %d: wav %f, raw %f, wei %f\n",nwav1+1,xp[nwav1].wav,xp[nwav1].raw,xp[nwav1].wei);
//		a1logd(p->log,3,"\n");
	}

#ifdef HIGH_RES_PLOT
	/* Plot original re-sampling curves + crossing points */
	{
		double *xx, *ss;
		double **yy;
		double *xc, *yc;

		xx = dvectorz(-1, m->nraw-1);		/* X index */
		yy = dmatrixz(0, 5, -1, m->nraw-1);	/* Curves distributed amongst 5 graphs */
		xc = dvectorz(0, nwav1);		/* Crossover X */
		yc = dvectorz(0, nwav1);		/* Crossover Y */
		
		for (i = 0; i < m->nraw; i++)
			xx[i] = i;

		/* For each output wavelength */
		for (j = 0; j < nwav1; j++) {
			i = j % 5;

			/* For each matrix value */
			for (k = 0; k < mtx_nocoef1[j]; k++) {
				yy[5][coeff[j][k].ix] += 0.5 * coeff[j][k].we;
				yy[i][coeff[j][k].ix] = coeff[j][k].we;
			}
		}

		/* Crosses at intersection points */
		for (i = 0; i <= nwav1; i++) {
			xc[i] = xp[i].raw;
			yc[i] = xp[i].wei;
		}

		if (ref)
			printf("Original reflection sampling curves + crossover points\n");
		else
			printf("Original emsission sampling curves + crossover points\n");
		do_plot6p(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw, xc, yc, nwav1+1);
		free_dvector(xx, -1, m->nraw-1);
		free_dmatrix(yy, 0, 2, -1, m->nraw-1);
		free_dvector(xc, 0, nwav1);
		free_dvector(yc, 0, nwav1);
	}
#endif /* HIGH_RES_PLOT */

#ifdef HIGH_RES_DEBUG
	/* Check to see if the area of each filter curve is the same */
	/* (yep, width times 2 * xover height is close to 1.0, and the */
	/*  sum of the weightings is exactly 1.0) */
	for (i = 0; i < nwav1; i++) {
		double area1, area2;
		area1 = fabs(xp[i].raw - xp[i+1].raw) * (xp[i].wei + xp[i+1].wei);

		area2 = 0.0;
		for (j = 0; j < (mtx_nocoef1[i]); j++)
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
			a1logd(p->log,3,"munki: creating rspl for high res conversion failed\n");
			return MUNKI_INT_NEW_RSPL_FAILED;
		}

		vlow[0] = 1e6;
		vhigh[0] = -1e6;

		for (i = 0; i < (nwav1+1); i++) {
			sd[i].p[0] = xp[i].raw;
			sd[i].v[0] = xp[i].wav;

			if (sd[i].v[0] < vlow[0])
				vlow[0] = sd[i].v[0];
			if (sd[i].v[0] > vhigh[0])
				vhigh[0] = sd[i].v[0];
		}
		glow[0] = 0.0;
		ghigh[0] = (double)(m->nraw-1);
		gres[0] = m->nraw;
		avgdev[0] = 0.0;
		
		raw2wav->fit_rspl(raw2wav, 0, sd, nwav1+1, glow, ghigh, gres, vlow, vhigh, 1.0, avgdev, NULL);
	}

#ifdef EXISTING_SHAPE
	/* Convert each weighting curves values into normalized values and */
	/* accumulate into a single curve. */
	/* This probably isn't quite correct - we really need to remove */
	/* the effects of the convolution with the CCD cell widths. */
	/* perhaps it's closer to a lanczos2 if this were done ? */
	{
		for (i = 0; i < nwav1; i++) {
			double cwl;		/* center wavelegth */
			double weight = 0.0;

			for (j = 0; j < (mtx_nocoef1[i]); j++) {
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

				cwl = XSPECT_WL(wl_short1, wl_long1, nwav1, i);

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
		HEAPSORT(munki_fs, fshape, ncp)
#undef HEAP_COMPARE

		/* Strip out leading zero's */
		for (i = 0; i < ncp; i++) {
			if (fshape[i].we != 0.0)
				break;
		}
		if (i > 1 && i < ncp) {
			memmove(&fshape[0], &fshape[i-1], sizeof(munki_fs) * (ncp - i + 1));
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
			if (ref)
				printf("Shape of existing reflection sampling curve:\n");
			else
				printf("Shape of existing emission sampling curve:\n");
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
#define MXNOWL 500      /* Max hires bands */
#define MXNOFC 64
		munki_fc coeff2[MXNOWL][MXNOFC];	/* New filter cooefficients */
		double twidth;

		/* Construct a set of filters that uses more CCD values */
		twidth = HIGHRES_WIDTH;

		if (m->nwav2 > MXNOWL) {		/* Assert */
			a1logw(p->log,"High res filter has too many bands\n");
			return MUNKI_INT_ASSERT;
		}

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
		if (fshmax <= 0.0) {
			a1logw(p->log,"munki: fshmax search failed\n");
			return MUNKI_INT_ASSERT;
		}
#endif
//		a1logd(p->log,1,"fshmax = %f\n",fshmax);

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
		if ((*pmtx_nocoef2 = mtx_nocoef2 = (int *)calloc(m->nwav2, sizeof(int))) == NULL) {
			a1logd(p->log,1,"munki: malloc mtx_nocoef2 failed!\n");
			return MUNKI_INT_MALLOC;
		}

		/* For all the useful CCD bands */
		for (i = 0; i < m->nraw; i++) {
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

			a1logd(p->log,1,"CCD %d, wl %f - %f\n",i,w1,w2);

			/* For each filter */
			for (j = 0; j < m->nwav2; j++) {
				double cwl, rwl;		/* center, relative wavelegth */
				double we;

				cwl = m->wl_short2 + (double)j * (m->wl_long2 - m->wl_short2)/(m->nwav2-1.0);

				if (cwl < min_wl)		/* Duplicate below this wl */
					cwl = min_wl;

				rwl = wl - cwl;			/* relative wavelgth to filter */

				if (fabs(w1 - cwl) > fshmax && fabs(w2 - cwl) > fshmax)
					continue;		/* Doesn't fall into this filter */

#ifndef NEVER
				/* Integrate in 0.05 nm increments from filter shape */
				{
					int nn;
					double lw, ll;
#ifdef FAST_HIGH_RES_SETUP
# define FINC 0.2
#else
# define FINC 0.05
#endif
					nn = (int)(fabs(w2 - w1)/FINC + 0.5);
			
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

				if (mtx_nocoef2[j] >= MXNOFC) {
					a1logw(p->log,"munki: run out of high res filter space\n");
					return MUNKI_INT_ASSERT;
				}

				coeff2[j][mtx_nocoef2[j]].ix = i;
				coeff2[j][mtx_nocoef2[j]++].we = we; 
				a1logd(p->log,1,"filter %d, cwl %f, rwl %f, ix %d, we %f\n",j,cwl,rwl,i,we);
			}
		}

		/* Dump the filter coefficients */
		if (p->log->debug >= 1) {	

			/* For each output wavelength */
			for (j = 0; j < m->nwav2; j++) {

				a1logd(p->log,1,"filter %d, cwl %f\n",j,XSPECT_WL(m->wl_short2, m->wl_long2, m->nwav2, j));
				/* For each matrix value */
				for (k = 0; k < mtx_nocoef2[j]; k++) {
					a1logd(p->log,1," CCD %d, we %f\n",coeff2[j][k].ix,coeff2[j][k].we);
				}
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
				for (k = 0; k < mtx_nocoef2[j]; k++) {
					yy[5][coeff2[j][k].ix] += 0.5 * coeff2[j][k].we;
					yy[i][coeff2[j][k].ix] = coeff2[j][k].we;
				}
			}

			if (ref)
				printf("Hi-Res reflection wavelength sampling curves:\n");
			else
				printf("Hi-Res emission wavelength sampling curves:\n");
			do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
			free_dvector(xx, 0, m->nraw-1);
			free_dmatrix(yy, 0, 2, 0, m->nraw-1);
		}
#endif /* HIGH_RES_PLOT */

		/* Convert into runtime format */
		{
			int xcount;

			if ((*pmtx_index2 = mtx_index2 = (int *)calloc(m->nwav2, sizeof(int))) == NULL) {
				a1logd(p->log,1,"munki: malloc mtx_index2 failed!\n");
				return MUNKI_INT_MALLOC;
			}
	
			xcount = 0;
			for (j = 0; j < m->nwav2; j++) {
				mtx_index2[j] = coeff2[j][0].ix;
				xcount += mtx_nocoef2[j];
			}
	
			if ((*pmtx_coef2 = mtx_coef2 = (double *)calloc(xcount, sizeof(double))) == NULL) {
				a1logd(p->log,1,"munki: malloc mtx_coef2 failed!\n");
				return MUNKI_INT_MALLOC;
			}

			for (i = j = 0; j < m->nwav2; j++)
				for (k = 0; k < mtx_nocoef2[j]; k++, i++)
					mtx_coef2[i] = coeff2[j][k].we;
		}

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
			for (cx = j = 0; j < m->nwav1; j++) { /* For each wavelength */

				/* For each matrix value */
				sx = mtx_index1[j];		/* Starting index */
				if (j < (m->nwav1-2) && sx == mtx_index1[j+1]) {
					cx += mtx_nocoef1[j];
					continue;			/* Skip all duplicate filters */
				}
				for (k = 0; k < mtx_nocoef1[j]; k++, cx++, sx++) {
					ccdsum[0][sx] += mtx_coef1[cx];
//printf("~1 Norm CCD [%d] %f += [%d] %f\n",sx,ccdsum[0][sx],cx, mtx_coef1[cx]);
				}
			}

			/* Compute the weighting of each CCD value in the hires output */
			for (cx = j = 0; j < m->nwav2; j++) { /* For each wavelength */

				/* For each matrix value */
				sx = mtx_index2[j];		/* Starting index */
				if (j < (m->nwav2-2) && sx == mtx_index2[j+1]) {
					cx += mtx_nocoef2[j];
					continue;			/* Skip all duplicate filters */
				}
				for (k = 0; k < mtx_nocoef2[j]; k++, cx++, sx++) {
					ccdsum[1][sx] += mtx_coef2[cx];
//printf("~1 HiRes CCD [%d] %f += [%d] %f\n",sx,ccdsum[1][sx],cx, mtx_coef2[cx]);
				}
			}

#ifdef HIGH_RES_PLOT
			/* Plot target CCD values */
			{
				double xx[128], y1[128], y2[128];
	
				for (i = 0; i < 128; i++) {
					xx[i] = i;
					y1[i] = ccdsum[0][i];
					y2[i] = ccdsum[1][i];
				}
	
				printf("Raw target and actual CCD weight sums:\n");
				do_plot(xx, y1, y2, NULL, 128);
			}
#endif

			/* Figure valid range and extrapolate to edges */
			dth[0] = 0.0;		/* ref */
			dth[1] = 0.007;		/* hires */

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
				x[0] += 2.0;
				x[3] -= 6.0;
				x[1] = floor((2 * x[0] + x[3])/3.0);
				x[2] = floor((x[0] + 2 * x[3])/3.0);

				for (i = 0; i < 4; i++)
					y[i] = ccdsum[k][(int)x[i]];

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
				double xx[129], y1[129], y2[129];
	
				for (i = 0; i < 128; i++) {
					xx[i] = i;
					y1[i] = ccdsum[0][i]/avg[0];
					y2[i] = ccdsum[1][i]/avg[1];
				}
				xx[i] = i;
				y1[i] = 0.0;
				y2[i] = 0.0;
	
				printf("Extrap. target and actual CCD weight sums:\n");
				do_plot(xx, y1, y2, NULL, 129);
			}
#endif

#ifdef DO_CCDNORMAVG	/* Just correct by average */
			for (cx = j = 0; j < m->nwav2; j++) { /* For each wavelength */

				/* For each matrix value */
				sx = mtx_index2[j];		/* Starting index */
				for (k = 0; k < mtx_nocoef2[j]; k++, cx++, sx++) {
					mtx_coef2[cx] *= 10.0/twidth * avg[0]/avg[1];
				}
			}

#else			/* Correct by CCD bin */

			/* Correct the weighting of each CCD value in the hires output */
			for (i = 0; i < 128; i++) {
				ccdsum[1][i] = 10.0/twidth * ccdsum[0][i]/ccdsum[1][i];		/* Correction factor */
			}
			for (cx = j = 0; j < m->nwav2; j++) { /* For each wavelength */

				/* For each matrix value */
				sx = mtx_index2[j];		/* Starting index */
				for (k = 0; k < mtx_nocoef2[j]; k++, cx++, sx++) {
					mtx_coef2[cx] *= ccdsum[1][sx];
				}
			}
#endif
		}
#endif /* DO_CCDNORM */

#ifdef HIGH_RES_PLOT
		{
			static munki_fc coeff2[MXNOWL][MXNOFC];	
			double *xx, *ss;
			double **yy;

			/* Convert the native filter cooeficient representation to */
			/* a 2D array we can randomly index. */
			for (cx = j = 0; j < m->nwav2; j++) { /* For each output wavelength */
				if (j >= MXNOWL) {	/* Assert */
					a1loge(p->log,1,"munki: number of hires output wavelenths is > %d\n",MXNOWL);
					return MUNKI_INT_ASSERT;
				}

				/* For each matrix value */
				sx = mtx_index2[j];		/* Starting index */
				for (k = 0; k < mtx_nocoef2[j]; k++, cx++, sx++) {
					if (k >= MXNOFC) {	/* Assert */
						a1loge(p->log,1,"munki: number of hires filter coeefs is > %d\n",MXNOFC);
						return MUNKI_INT_ASSERT;
					}
					coeff2[j][k].ix = sx;
					coeff2[j][k].we = mtx_coef2[cx];
				}
			}
	
			xx = dvectorz(-1, m->nraw-1);		/* X index */
			yy = dmatrixz(0, 5, -1, m->nraw-1);	/* Curves distributed amongst 5 graphs */
			
			for (i = 0; i < m->nraw; i++)
				xx[i] = i;

			/* For each output wavelength */
			for (j = 0; j < m->nwav2; j++) {
				i = j % 5;

				/* For each matrix value */
				for (k = 0; k < mtx_nocoef2[j]; k++) {
					yy[5][coeff2[j][k].ix] += 0.5 * coeff2[j][k].we;
					yy[i][coeff2[j][k].ix] = coeff2[j][k].we;
				}
			}

			printf("Normalized Hi-Res wavelength sampling curves: %s\n",ref ? "refl" : "emis");
			do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], m->nraw);
			free_dvector(xx, -1, m->nraw-1);
			free_dmatrix(yy, 0, 2, -1, m->nraw-1);
		}
#endif /* HIGH_RES_PLOT */
#undef MXNOWL
#undef MXNOFC

		/* Basic capability is initialised */
		m->hr_inited++;

		/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
		/* If both reflective and emissive samplings have been created, */
		/* deal with upsampling the references and calibrations */
		if (m->hr_inited == 2) {
#ifdef ONEDSTRAYLIGHTUS
			double **slp;			/* 2D Array of stray light values */
#endif /* !ONEDSTRAYLIGHTUS */
			rspl *trspl;			/* Upsample rspl */
			cow sd[40 * 40];		/* Scattered data points of existing references */
			datai glow, ghigh;
			datao vlow, vhigh;
			int gres[2];
			double avgdev[2];
			int ii, jj;
			co pp;
	
			/* First the 1D references */
			if ((trspl = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
				a1logd(p->log,3,"munki: creating rspl for high res conversion failed\n");
				raw2wav->del(raw2wav);
				return MUNKI_INT_NEW_RSPL_FAILED;
			}
			
			for (ii = 0; ii < 4; ii++) {
				double **ref2, *ref1;

				if (ii == 0) {
					ref1 = m->white_ref1;
					ref2 = &m->white_ref2;
				} else if (ii == 1) {
					ref1 = m->emis_coef1;
					ref2 = &m->emis_coef2;
				} else if (ii == 2) {
					ref1 = m->amb_coef1;
					ref2 = &m->amb_coef2;
				} else {
					ref1 = m->proj_coef1;
					ref2 = &m->proj_coef2;
				}

				vlow[0] = 1e6;
				vhigh[0] = -1e6;

				/* Set scattered points */
				for (i = 0; i < m->nwav1; i++) {

					sd[i].p[0] = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, i);
					sd[i].v[0] = ref1[i];
					sd[i].w = 1.0;

					if (sd[i].v[0] < vlow[0])
						vlow[0] = sd[i].v[0];
					if (sd[i].v[0] > vhigh[0])
						vhigh[0] = sd[i].v[0];
				}

#ifdef NEVER
				/* Add some corrections at short wavelengths */
				/* (The combination of the diffraction grating and */
				/*  LED light source doesn't give us much to work with here.) */
				if (ii == 1) {	/* Emission */
					sd[0].v[0] *= 10.0;		/* 380 */
					sd[1].v[0] *= 3.0;		/* 390 */
					sd[2].v[0] *= 1.0;		/* 400 */
				}

				if (ii == 2) {	/* Ambient */
					sd[0].v[0] *= 5.0;		/* 380 */		/* Doesn't help much, because */
					sd[1].v[0] *= 2.0;		/* 390 */		/* the diffuser absorbs short WL */
					sd[2].v[0] *= 1.0;		/* 400 */
				}

				if (ii == 3) {	/* Projector */
					sd[0].v[0] *= 0.1;		/* 380 */
					sd[1].v[0] *= 1.0;		/* 390 */

					sd[i].p[0] = 350.0; 	/* 350 */
					sd[i].v[0] = 0.2 * sd[0].v[0];
					sd[i++].w = 1.0;
				}
#endif

				glow[0] = m->wl_short2;
				ghigh[0] = m->wl_long2;
				gres[0] = m->nwav2;
				avgdev[0] = 0.0;
				
				trspl->fit_rspl_w(trspl, 0, sd, i, glow, ghigh, gres, vlow, vhigh, 5.0, avgdev, NULL);

				if ((*ref2 = (double *)calloc(m->nwav2, sizeof(double))) == NULL) {
					raw2wav->del(raw2wav);
					trspl->del(trspl);
					a1logd(p->log,1,"munki: malloc mtx_coef2 failed!\n");
					return MUNKI_INT_MALLOC;
				}

				/* Create upsampled version */
				for (i = 0; i < m->nwav2; i++) {
					pp.p[0] = XSPECT_WL(m->wl_short2, m->wl_long2, m->nwav2, i);
					if (pp.p[0] < min_wl)	/* Duplicate below this wl */
						pp.p[0] = min_wl;
					trspl->interp(trspl, &pp);
					if (pp.v[0] < 0.0)
						pp.v[0] = 0.0;
					(*ref2)[i] = pp.v[0];
				}


#ifdef NEVER
				/* Add some corrections at short wavelengths */
				if (ii == 0) {
					/* 376.67 - 470 */
					double corr[5][29] = {
						{ 4.2413, 4.0654, 3.6425, 3.2194, 2.8692, 2.3964,
						  1.9678, 1.3527, 0.7978, 0.7823, 0.8474, 0.9227,
						   0.9833, 1.0164, 1.0270, 1.0241, 1.0157, 1.0096,
						   1.0060, 1.0, 1.0, 1.0, 1.0, 1.0,
						   1.0, 1.0, 1.0, 1.0, 1.0 },

					};

					for (i = 0; i < m->nwav2; i++) {
						double wl;
						int ix;
						wl = XSPECT_WL(m->wl_short2, m->wl_long2, m->nwav2, i);
						ix = XSPECT_IX(376.6666667, 470.0, 29, wl);

						if (ix < 0)
							ix = 0;
						else if (ix >= 29)
							ix = 28;
						(*ref2)[i] *= corr[ii][ix];
					}

				}
#endif

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
						printf("Emission cal. curve:\n");
					} else if (ii == 2) {
						printf("Ambient cal. curve:\n");
					} else {
						printf("Projector cal. curve:\n");
					}
					do_plot(x1, y1, y2, NULL, m->nwav2);
		
					free_dvector(x1, 0, m->nwav2-1);
					free_dvector(y1, 0, m->nwav2-1);
					free_dvector(y2, 0, m->nwav2-1);
				}
#endif /* HIGH_RES_PLOT */
			}
			trspl->del(trspl);

#ifdef ONEDSTRAYLIGHTUS
			/* Then the 2D stray light using linear interpolation */
			slp = dmatrix(0, m->nwav1-1, 0, m->nwav1-1);

			/* Set scattered points */
			for (i = 0; i < m->nwav1; i++) {		/* Output wavelength */
				for (j = 0; j < m->nwav1; j++) {	/* Input wavelength */

					slp[i][j] = m->straylight1[i][j];

					/* Use interpolate/extrapolate for middle points */
					if (j == (i-1) || j == i || j == (i+1)) {
						int j0, j1;
						double w0, w1;
						if (j == (i-1)) {
							if (j <= 0)
								j0 = j+3, j1 = j+4;
							else if (j >= (m->nwav1-3))
								j0 = j-2, j1 = j-1;
							else
								j0 = j-1, j1 = j+3;
						} else if (j == i) {
							if (j <= 1)
								j0 = j+2, j1 = j+3;
							else if (j >= (m->nwav1-2))
								j0 = j-3, j1 = j-2;
							else
								j0 = j-2, j1 = j+2;
						} else if (j == (i+1)) {
							if (j <= 2)
								j0 = j+1, j1 = j+2;
							else if (j >= (m->nwav1-1))
								j0 = j-4, j1 = j-3;
							else
								j0 = j-3, j1 = j+1;
						}
						w1 = (j - j0)/(j1 - j0);
						w0 = 1.0 - w1;
						slp[i][j] = w0 * m->straylight1[i][j0]
						          + w1 * m->straylight1[i][j1];

					}
				}
			}
#else	/* !ONEDSTRAYLIGHTUS */
			/* Then setup 2D stray light using rspl */
			if ((trspl = new_rspl(RSPL_NOFLAGS, 2, 1)) == NULL) {
				a1logd(p->log,3,"munki: creating rspl for high res conversion failed\n");
				raw2wav->del(raw2wav);
				return MUNKI_INT_NEW_RSPL_FAILED;
			}
			
			/* Set scattered points */
			for (i = 0; i < m->nwav1; i++) {		/* Output wavelength */
				for (j = 0; j < m->nwav1; j++) {	/* Input wavelength */
					int ix = i * m->nwav1 + j;

					sd[ix].p[0] = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, i);
					sd[ix].p[1] = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, j);
					sd[ix].v[0] = m->straylight1[i][j];
					sd[ix].w = 1.0;
					if (j == (i-1) || j == i || j == (i+1))
						sd[ix].w = 0.0;
				}
			}

			glow[0] = m->wl_short2;
			glow[1] = m->wl_short2;
			ghigh[0] = m->wl_long2;
			ghigh[1] = m->wl_long2;
			gres[0] = m->nwav2;
			gres[1] = m->nwav2;
			avgdev[0] = 0.0;
			avgdev[1] = 0.0;
			
			trspl->fit_rspl_w(trspl, 0, sd, m->nwav1 * m->nwav1, glow, ghigh, gres, NULL, NULL, 0.5, avgdev, NULL);
#endif	/* !ONEDSTRAYLIGHTUS */

			m->straylight2 = dmatrixz(0, m->nwav2-1, 0, m->nwav2-1);  

			/* Create upsampled version */
			for (i = 0; i < m->nwav2; i++) {		/* Output wavelength */
				for (j = 0; j < m->nwav2; j++) {	/* Input wavelength */
					double p0, p1;
					p0 = XSPECT_WL(m->wl_short2, m->wl_long2, m->nwav2, i);
					if (p0 < min_wl)	/* Duplicate below this wl */
						p0 = min_wl;
					p1 = XSPECT_WL(m->wl_short2, m->wl_long2, m->nwav2, j);
					if (p1 < min_wl)	/* Duplicate below this wl */
						p1 = min_wl;
#ifdef ONEDSTRAYLIGHTUS
					/* Do linear interp with clipping at ends */
					{
						int x0, x1, y0, y1;
						double xx, yy, w0, w1, v0, v1;

						xx = (m->nwav1-1.0) * (p0 - m->wl_short1)/(m->wl_long1 - m->wl_short1);
						x0 = (int)floor(xx);
						if (x0 <= 0)
							x0 = 0;
						else if (x0 >= (m->nwav1-2))
							x0 = m->nwav1-2;
						x1 = x0 + 1;
						w1 = xx - (double)x0;
						w0 = 1.0 - w1;

						yy = (m->nwav1-1.0) * (p1 - m->wl_short1)/(m->wl_long1 - m->wl_short1);
						y0 = (int)floor(yy);
						if (y0 <= 0)
							y0 = 0;
						else if (y0 >= (m->nwav1-2))
							y0 = m->nwav1-2;
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
					m->straylight2[i][j] = pp.v[0] * HIGHRES_WIDTH/10.0;
					if (m->straylight2[i][j] > 0.0)
						m->straylight2[i][j] = 0.0;
				}
			}

			/* Fix primary wavelength weight and neighbors */
			for (i = 0; i < m->nwav2; i++) {		/* Output wavelength */
				double sum;

				if (i > 0)
					m->straylight2[i][i-1] = 0.0;
				m->straylight2[i][i] = 0.0;
				if (i < (m->nwav2-1))
					m->straylight2[i][i+1] = 0.0;

				for (sum = 0.0, j = 0; j < m->nwav2; j++)
					sum += m->straylight2[i][j];

				m->straylight2[i][i] = 1.0 - sum;		/* Total sum should be 1.0 */
			}

#ifdef HIGH_RES_PLOT_STRAYL
			/* Plot original and upsampled reference */
			{
				double *x1 = dvectorz(0, m->nwav2-1);
				double *y1 = dvectorz(0, m->nwav2-1);
				double *y2 = dvectorz(0, m->nwav2-1);
	
				for (i = 0; i < m->nwav2; i++) {		/* Output wavelength */
					double wli = XSPECT_WL(m->wl_short2, m->wl_long2, m->nwav2, i);
					int i1 = XSPECT_IX(m->wl_short1, m->wl_long1, m->nwav1, wli);

					for (j = 0; j < m->nwav2; j++) {
						double wl = XSPECT_WL(m->wl_short2, m->wl_long2, m->nwav2, j);
						x1[j] = wl;
						y1[j] = m->straylight2[i][j];
						if (y1[j] == 0.0)
							y1[j] = -8.0;
						else
							y1[j] = log10(fabs(y1[j]));
						if (wli < m->wl_short1 || wli > m->wl_long1
						 || wl < m->wl_short1 || wl > m->wl_long1) {
							y2[j] = -8.0;
						} else {
							double x, wl1, wl2;
							for (k = 0; k < (m->nwav1-1); k++) {
								wl1 = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, k);
								wl2 = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, k+1);
								if (wl >= wl1 && wl <= wl2)
									break;
							}
							x = (wl - wl1)/(wl2 - wl1);
							y2[j] = m->straylight1[i1][k] + (m->straylight1[i1][k+1]
							      - m->straylight1[i1][k]) * x;
							if (y2[j] == 0.0)
								y2[j] = -8.0;
							else
								y2[j] = log10(fabs(y2[j]));
						}
					}
					do_plot(x1, y1, y2, NULL, m->nwav2);
				}
	
				free_dvector(x1, 0, m->nwav2-1);
				free_dvector(y1, 0, m->nwav2-1);
				free_dvector(y2, 0, m->nwav2-1);
			}
#endif /* HIGH_RES_PLOT */

#ifdef ONEDSTRAYLIGHTUS
			free_dmatrix(slp, 0, m->nwav1-1, 0, m->nwav1-1);
#else /* !ONEDSTRAYLIGHTUS */
			trspl->del(trspl);
#endif /* !ONEDSTRAYLIGHTUS */

			/* - - - - - - - - - - - - - - - - - - - - - - - - - */
			/* Allocate space for per mode calibration reference */
			/* and bring high res calibration factors into line */
			/* with current standard res. ones */
			for (i = 0; i < mk_no_modes; i++) {
				munki_state *s = &m->ms[i];

				s->cal_factor2 = dvectorz(0, m->nwav2-1);

				switch(i) {
					case mk_refl_spot:
					case mk_refl_scan:
						if (s->cal_valid) {
							munki_absraw_to_abswav1(p, 1, &s->cal_factor1, &s->white_data);
							munki_absraw_to_abswav2(p, 1, &s->cal_factor2, &s->white_data);
							munki_compute_white_cal(p, s->cal_factor1, m->white_ref1, s->cal_factor1,
							                           s->cal_factor2, m->white_ref2, s->cal_factor2);
						}
						break;

					case mk_emiss_spot_na:
					case mk_emiss_spot:
					case mk_emiss_scan:
						for (j = 0; j < m->nwav2; j++)
							s->cal_factor2[j] = EMIS_SCALE_FACTOR * m->emis_coef2[j];
						break;
		
					case mk_tele_spot_na:
					case mk_tele_spot:
						for (j = 0; j < m->nwav2; j++)
							s->cal_factor2[j] = EMIS_SCALE_FACTOR * m->proj_coef2[j];
						break;
		
					case mk_amb_spot:
					case mk_amb_flash:
						if (m->amb_coef1 != NULL) {
							for (j = 0; j < m->nwav2; j++)
								s->cal_factor2[j] = AMB_SCALE_FACTOR * m->amb_coef2[j];
							s->cal_valid = 1;
						}
						break;
					case mk_trans_spot:
					case mk_trans_scan:
						if (s->cal_valid) {
							munki_absraw_to_abswav1(p, 1, &s->cal_factor1, &s->white_data);
							munki_absraw_to_abswav2(p, 1, &s->cal_factor2, &s->white_data);
							munki_compute_white_cal(p, s->cal_factor1, NULL, s->cal_factor1,
				                                       s->cal_factor2, NULL, s->cal_factor2);
						}
						break;
				}
			}
		}
	}

	raw2wav->del(raw2wav);

	return ev;
}

#endif /* HIGH_RES */


/* return nz if high res is supported */
int munki_imp_highres(munki *p) {
#ifdef HIGH_RES
	return 1;
#else
	return 0;
#endif /* HIGH_RES */
}

/* Set to high resolution mode */
munki_code munki_set_highres(munki *p) {
	int i;
	munkiimp *m = (munkiimp *)p->m;
	munki_code ev = MUNKI_OK;

#ifdef HIGH_RES
	if (m->hr_inited == 0) {
		if ((ev = munki_create_hr(p, 1)) != MUNKI_OK)	/* Reflective */
			return ev;	
		if ((ev = munki_create_hr(p, 0)) != MUNKI_OK)	/* Emissive */
			return ev;
	}

	m->nwav = m->nwav2;
	m->wl_short = m->wl_short2;
	m->wl_long = m->wl_long2; 

	m->rmtx_index = m->rmtx_index2;
	m->rmtx_nocoef = m->rmtx_nocoef2;
	m->rmtx_coef = m->rmtx_coef2;
	m->emtx_index = m->emtx_index2;
	m->emtx_nocoef = m->emtx_nocoef2;
	m->emtx_coef = m->emtx_coef2;
	m->white_ref = m->white_ref2;
	m->emis_coef = m->emis_coef2;
	m->amb_coef = m->amb_coef2;
	m->proj_coef = m->proj_coef2;
	m->straylight = m->straylight2;

	for (i = 0; i < mk_no_modes; i++) {
		munki_state *s = &m->ms[i];
		s->cal_factor = s->cal_factor2;
	}
	m->highres = 1;
#else
	ev = MUNKI_UNSUPPORTED;
#endif /* HIGH_RES */

	return ev;
}

/* Set to standard resolution mode */
munki_code munki_set_stdres(munki *p) {
	int i;
	munkiimp *m = (munkiimp *)p->m;
	munki_code ev = MUNKI_OK;

#ifdef HIGH_RES
	m->nwav = m->nwav1;
	m->wl_short = m->wl_short1;
	m->wl_long = m->wl_long1; 

	m->rmtx_index = m->rmtx_index1;
	m->rmtx_nocoef = m->rmtx_nocoef1;
	m->rmtx_coef = m->rmtx_coef1;
	m->emtx_index = m->emtx_index1;
	m->emtx_nocoef = m->emtx_nocoef1;
	m->emtx_coef = m->emtx_coef1;
	m->white_ref = m->white_ref1;
	m->emis_coef = m->emis_coef1;
	m->amb_coef = m->amb_coef1;
	m->proj_coef = m->proj_coef1;
	m->straylight = m->straylight1;

	for (i = 0; i < mk_no_modes; i++) {
		munki_state *s = &m->ms[i];
		s->cal_factor = s->cal_factor1;
	}
	m->highres = 0;

#else
	ev = MUNKI_UNSUPPORTED;
#endif /* HIGH_RES */

	return ev;
}

/* Modify the scan consistency tolerance */
munki_code munki_set_scan_toll(munki *p, double toll_ratio) {
	munkiimp *m = (munkiimp *)p->m;
	munki_code ev = MUNKI_OK;

	m->scan_toll_ratio = toll_ratio;

	return MUNKI_OK;
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

/* Convert from spectral to XYZ, and transfer to the ipatch array */
munki_code munki_conv2XYZ(
	munki *p,
	ipatch *vals,		/* Values to return */
	int nvals,			/* Number of values */
	double **specrd,	/* Spectral readings */
	instClamping clamp	/* Clamp XYZ/Lab to be +ve */
) {
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	xsp2cie *conv;	/* Spectral to XYZ conversion object */
	int i, j, k;
	int six = 0;		/* Starting index */
	int nwl = m->nwav;	/* Number of wavelegths */
	double wl_short = m->wl_short;	/* Starting wavelegth */
	double sms;			/* Weighting */

	if (s->emiss)
		conv = new_xsp2cie(icxIT_none, NULL, icxOT_CIE_1931_2, NULL, icSigXYZData, (icxClamping)clamp);
	else
		conv = new_xsp2cie(icxIT_D50, NULL, icxOT_CIE_1931_2, NULL, icSigXYZData, (icxClamping)clamp);
	if (conv == NULL)
		return MUNKI_INT_CIECONVFAIL;

	a1logd(p->log,3,"munki_conv2XYZ got wl_short %f, wl_long %f, nwav %d\n"
	                "      after skip got wl_short %f, nwl = %d\n",
	                m->wl_short, m->wl_long, m->nwav, wl_short, nwl);

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
		vals[i].sp.spec_wl_long = m->wl_long;

		if (s->emiss) {
			for (j = six, k = 0; j < m->nwav; j++, k++) {
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
			for (j = six, k = 0; j < m->nwav; j++, k++) {
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
	return MUNKI_OK;
}

/* Compute a mode calibration factor given the reading of the white reference. */
/* Return 1 if any of the transmission wavelengths are low. */
int munki_compute_white_cal(
	munki *p,
	double *cal_factor1,	/* [nwav1] Calibration factor to compute */
	double *white_ref1,		/* [nwav1] White reference to aim for, NULL for 1.0 */
	double *white_read1,	/* [nwav1] The white that was read */
	double *cal_factor2,	/* [nwav2] Calibration factor to compute */
	double *white_ref2,		/* [nwav2] White reference to aim for, NULL for 1.0 */
	double *white_read2		/* [nwav2] The white that was read */
) {
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	int j, warn = 0;
	
	a1logd(p->log,3,"munki_compute_white_cal called\n");

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
munki_code munki_optimise_sensor(
	munki *p,
	double *pnew_int_time,
	int    *pnew_gain_mode,
	double cur_int_time,	/* Current intergration time */
	int    cur_gain_mode,	/* nz if currently high gain */
	int    permithg,		/* nz to permit switching to high gain mode */
	int    permitclip,		/* nz to permit clipping out of range int_time, else error */
	double *targoscale,		/* Optimising target scale ( <= 1.0) */
							/* (May be altered if integration time isn't possible) */
	double scale,			/* scale needed of current int time to reach optimum */
	double deadtime			/* Dead integration time (if any) */
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	double new_int_time;
	double min_int_time;	/* Adjusted min_int_time */
	int    new_gain_mode;

	a1logd(p->log,3,"munki_optimise_sensor called, inttime %f, gain mode %d, scale %f\n",cur_int_time,cur_gain_mode, scale);

	min_int_time = m->min_int_time - deadtime;
	cur_int_time -= deadtime;

	/* Compute new normal gain integration time */
	if (cur_gain_mode)
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
		/* Hmm. It seems not be a good idea to use high gain mode if it compromises */
		/* the longer integration time which reduces noise. */
		if (s->auto_gain) {
			if (new_int_time > m->max_int_time && permithg) {
				new_int_time /= m->highgain;
				new_gain_mode = 1;
				a1logd(p->log,3,"Switching to high gain mode\n");
			}
		}
	}
	a1logd(p->log,3,"after low light adjust, inttime %f, gain mode %d\n",new_int_time,new_gain_mode);

	/* Deal with still low light */
	if (new_int_time > m->max_int_time) {
		if (permitclip)
			new_int_time = m->max_int_time;
		else
			return MUNKI_RD_LIGHTTOOLOW;
	}
	a1logd(p->log,3,"after low light clip, inttime %f, gain mode %d\n",new_int_time,new_gain_mode);

	/* Adjust to high light situation */
	if (new_int_time < min_int_time && *targoscale < 1.0) {
		*targoscale *= min_int_time/new_int_time;
		new_int_time = min_int_time;
	}
	a1logd(p->log,3,"after high light adjust, targoscale %f, inttime %f, gain mode %d\n",*targoscale, new_int_time,new_gain_mode);

	/* Deal with still high light */
	if (new_int_time < min_int_time) {
		if (permitclip)
			new_int_time = min_int_time;
		else
			return MUNKI_RD_LIGHTTOOHIGH;
	}
	a1logd(p->log,3,"after high light clip, returning inttime %f, gain mode %d\n",new_int_time,new_gain_mode);

	new_int_time += deadtime;

	a1logd(p->log,3,"munki_optimise_sensor returning inttime %f, gain mode %d\n",new_int_time,new_gain_mode);
	if (pnew_int_time != NULL)
		*pnew_int_time = new_int_time;

	if (pnew_gain_mode != NULL)
		*pnew_gain_mode = new_gain_mode;

	return MUNKI_OK;
}

/* Compute the number of measurements needed, given the target */
/* time and integration time. Will return 0 if target time is 0 */
int munki_comp_nummeas(
	munki *p,
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

/* Compute the rounded up number of measurements needed, */
/* given the target time and integration time. */
/* Will return 0 if target time is 0 */
int munki_comp_ru_nummeas(
	munki *p,
	double meas_time,
	double int_time
) {
	int nmeas;
	if (meas_time <= 0.0)
		return 0;
	nmeas = (int)ceil(meas_time/int_time);
	return nmeas;
}

/* Convert the dark interpolation data to a useful state */
/* (also allow for interpolating the shielded cell values) */
void
munki_prepare_idark(
	munki *p
) {
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	int i, j;

	/* For normal and high gain */
	for (i = 0; i < 4; i+=2) {
		for (j = -1; j < m->nraw; j++) {
			double d01, d1;
			d01 = s->idark_data[i+0][j];
			d1 = s->idark_data[i+1][j];
	
			/* Compute increment proportional to time */
			s->idark_data[i+1][j] = (d1 - d01)/(s->idark_int_time[i+1] - s->idark_int_time[i+0]);
	
			/* Compute base */
			s->idark_data[i+0][j] = d01 - s->idark_data[i+1][j] * s->idark_int_time[i+0];;
		}
	}
}

/* Create the dark reference for the given integration time and gain */
/* by interpolating from the 4 readings prepared earlier */
munki_code
munki_interp_dark(
	munki *p,
	double *result,		/* Put result of interpolation here */
	double inttime,
	int gainmode
) {
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	int i, j;

	if (!s->idark_valid)
		return MUNKI_INT_NOTCALIBRATED;
		
	i = 0;
	if (s->auto_gain && gainmode)
		i = 2;

	for (j = -1; j < m->nraw; j++) {
		double tt;
		tt = s->idark_data[i+0][j] + inttime * s->idark_data[i+1][j];
		result[j] = tt;
	}
	return MUNKI_OK;
}

/* Set the noinitcalib mode */
void munki_set_noinitcalib(munki *p, int v, int losecs) {
	munkiimp *m = (munkiimp *)p->m;
	/* Ignore disabling init calib if more than losecs since instrument was open */
a1logd(p->log,3,"set_noinitcalib v = %d, ->lo_secs %d, losecs %d secs\n",v, m->lo_secs,losecs);
	if (v && losecs != 0 && m->lo_secs >= losecs) {
		a1logd(p->log,3,"initcalib disable ignored because %d >= %d secs\n",m->lo_secs,losecs);
		return;
	}
	m->noinitcalib = v;
}

/* Set the trigger config */
void munki_set_trig(munki *p, inst_opt_type trig) {
	munkiimp *m = (munkiimp *)p->m;
	m->trig = trig;
}

/* Return the trigger config */
inst_opt_type munki_get_trig(munki *p) {
	munkiimp *m = (munkiimp *)p->m;
	return m->trig;
}

/* Switch thread handler */
static int munki_switch_thread(void *pp) {
	int nfailed = 0;
	munki *p = (munki *)pp;
	munkiimp *m = (munkiimp *)p->m;
	munki_code rv = MUNKI_OK; 
	a1logd(p->log,3,"Switch thread started\n");
	for (nfailed = 0;nfailed < 5;) {
		mk_eve ecode;

		rv = munki_waitfor_switch_th(p, &ecode, NULL, SW_THREAD_TIMEOUT);
		if (m->th_term) {
			m->th_termed = 1;
			break;
		}
		if (rv == MUNKI_INT_BUTTONTIMEOUT) {
			nfailed = 0;
			continue;
		}
		if (rv != MUNKI_OK) {
			nfailed++;
			a1logd(p->log,3,"Switch thread failed with 0x%x\n",rv);
			continue;
		}
		if (ecode == mk_eve_switch_press) {
			m->switch_count++;
			if (!m->hide_switch && p->eventcallback != NULL) {
				p->eventcallback(p->event_cntx, inst_event_switch);
			}
		} else if (ecode == mk_eve_spos_change) {
#ifdef FILTER_SPOS_EVENTS
			/* Signal change to filer thread */
			m->spos_msec = msec_time();
			m->spos_change++;
#else
			if (m->eventcallback != NULL) {
				m->eventcallback(p->event_cntx, inst_event_mconf);
			}
#endif
		}
	}
	a1logd(p->log,3,"Switch thread returning\n");
	return rv;
}

#ifdef FILTER_SPOS_EVENTS
static int munki_spos_thread(void *pp) {
	munki *p = (munki *)pp;
	munkiimp *m = (munkiimp *)p->m;
	int change = m->spos_change;		/* Current count */

	a1logd(p->log,3,"spos thread started\n");

	for (;;) {

		if (m->spos_th_term) {
			m->spos_th_termed = 1;
			break;
		}

		/* Do callback if change has persisted for 1 second */
		if (change != m->spos_change
		 && (msec_time() - m->spos_msec) >= FILTER_TIME) {
			change = m->spos_change;
			if (p->eventcallback != NULL) {
				p->eventcallback(p->event_cntx, inst_event_mconf);
			}
		}
		msec_sleep(100);
	}
	return 0;
}
#endif


/* ============================================================ */
/* Low level commands */

/* USB Instrument commands */

/* Read from the EEProm */
munki_code
munki_readEEProm(
	munki *p,
	unsigned char *buf,		/* Where to read it to */
	int addr,				/* Address in EEprom to read from */
	int size				/* Number of bytes to read (max 65535) */
) {
	munkiimp *m = (munkiimp *)p->m;
	int rwbytes;			/* Data bytes read or written */
	unsigned char pbuf[8];	/* Write EEprom parameters */
	int se, rv = MUNKI_OK;

	a1logd(p->log,2,"munki_readEEProm: address 0x%x size 0x%x\n",addr,size);

	if (size < 0 || addr < 0 || (addr + size) > (m->noeeblocks * m->eeblocksize))
		return MUNKI_INT_EEOUTOFRANGE;

	int2buf(&pbuf[0], addr);
	int2buf(&pbuf[4], size);
  	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0x81, 0, 0, pbuf, 8, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		a1logd(p->log,1,"munki_readEEProm: read failed (1) with ICOM err 0x%x\n",se);
		return rv;
	}

	/* Now read the bytes */
	se = p->icom->usb_read(p->icom, NULL, 0x81, buf, size, &rwbytes, 5.0);
	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		a1logd(p->log,1,"munki_readEEProm: read failed (2) with ICOM err 0x%x\n",se);
		return rv;
	}

	if (rwbytes != size) {
		a1logd(p->log,1,"munki_readEEProm: 0x%x bytes, short read error\n",rwbytes);
		return MUNKI_HW_EE_SHORTREAD;
	}

	if (p->log->debug >= 5) {
		int i;
		char oline[100] = { '\000' }, *bp = oline;
		for (i = 0; i < size; i++) {
			if ((i % 16) == 0)
				bp += sprintf(bp,"    %04x:",i);
			bp += sprintf(bp," %02x",buf[i]);
			if ((i+1) >= size || ((i+1) % 16) == 0) {
				bp += sprintf(bp,"\n");
				a1logd(p->log,5,oline);
				bp = oline;
			}
		}
	}

	a1logd(p->log,2,"munki_readEEProm: got 0x%x bytes, ICOM err 0x%x\n",rwbytes, se);

	return rv;
}



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
) {
	unsigned char pbuf[24];	/* status bytes read */
	int _fwrev_maj, _fwrev_min;
	int _tickdur;
	int _minintcount;
	int _noeeblocks;
	int _eeblocksize;
	int se, rv = MUNKI_OK;

	a1logd(p->log,2,"munki_getfirm:\n");

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0x86, 0, 0, pbuf, 24, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		a1logd(p->log,1,"munki_getfirm: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	_fwrev_maj   = buf2int(&pbuf[0]);
	_fwrev_min   = buf2int(&pbuf[4]);
	_tickdur     = buf2int(&pbuf[8]);
	_minintcount = buf2int(&pbuf[12]);
	_noeeblocks  = buf2int(&pbuf[16]);
	_eeblocksize = buf2int(&pbuf[20]);

	a1logd(p->log,2,"munki_getfirm: returning fwrev %d.%d, tickdur %d, minint %d, eeblks %d, "
	       "eeblksz %d ICOM err 0x%x\n", _fwrev_maj, _fwrev_min, _tickdur, _minintcount,
	                                     _noeeblocks, _eeblocksize, se);

	if (fwrev != NULL) *fwrev = _fwrev_maj * 256 + _fwrev_min ;
	if (tickdur != NULL) *tickdur = _tickdur;
	if (minintcount != NULL) *minintcount = _minintcount;
	if (noeeblocks != NULL) *noeeblocks = _noeeblocks;
	if (eeblocksize != NULL) *eeblocksize = _eeblocksize;

	return rv;
}

/* Get the Chip ID */
munki_code
munki_getchipid(
	munki *p,
	unsigned char chipid[8]
) {
	int se, rv = MUNKI_OK;

	a1logd(p->log,2,"munki_getchipid: called\n");

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0x8A, 0, 0, chipid, 8, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		a1logd(p->log,1,"munki_getchipid:  GetChipID failed with ICOM err 0x%x\n",se);
		return rv;
	}

	a1logd(p->log,2," GetChipID returns %02X-%02X%02X%02X%02X%02X%02X%02X ICOM err 0x%x\n",
                           chipid[0], chipid[1], chipid[2], chipid[3],
                           chipid[4], chipid[5], chipid[6], chipid[7], se);
	return rv;
}

/* Get the Version String */
munki_code
munki_getversionstring(
	munki *p,
	char vstring[37]
) {
	int se, rv = MUNKI_OK;

	a1logd(p->log,2,"munki_getversionstring: called\n");

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0x85, 0, 0, (unsigned char *)vstring, 36, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		a1logd(p->log,1,"munki_getversionstring: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	vstring[36] = '\000';

	a1logd(p->log,2,"munki_getversionstring: returning '%s' ICOM err 0x%x\n", vstring, se);

	return rv;
}

/* Get the measurement state */
/* return pointers may be NULL if not needed. */
munki_code
munki_getmeasstate(
	munki *p,
	int *ledtrange,		/* LED temperature range */
	int *ledtemp,		/* LED temperature */
	int *dutycycle,		/* Duty Cycle */
	int *ADfeedback		/* A/D converter feedback */
) {
	unsigned char pbuf[16];	/* values read */
	int _ledtrange;
	int _ledtemp;
	int _dutycycle;
	int _ADfeedback;
	int se, rv = MUNKI_OK;

	a1logd(p->log,2,"munki_getmeasstate: called\n");

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0x8F, 0, 0, pbuf, 16, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		a1logd(p->log,1,"munki_getmeasstate: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	_ledtrange   = buf2int(&pbuf[0]);
	_ledtemp     = buf2int(&pbuf[4]);
	_dutycycle   = buf2int(&pbuf[8]);
	_ADfeedback  = buf2int(&pbuf[12]);

	a1logd(p->log,2,"munki_getmeasstate: returning LED temp range %d, LED temp %d, "
	                "Duty Cycle %d, ADFeefback %d, ICOM err 0x%x\n",
	                _ledtrange, _ledtemp, _dutycycle, _ADfeedback, se);

	if (ledtrange != NULL) *ledtrange = _ledtrange;
	if (ledtemp != NULL) *ledtemp = _ledtemp;
	if (dutycycle != NULL) *dutycycle = _dutycycle;
	if (ADfeedback != NULL) *ADfeedback = _ADfeedback;

	return rv;
}

/* Get the device status */
/* return pointers may be NULL if not needed. */
munki_code
munki_getstatus(
	munki *p,
	mk_spos *spos,		/* Return the sensor position */
	mk_but *but			/* Return Button state */
) {
	unsigned char pbuf[2];	/* status bytes read */
	mk_spos _spos;
	mk_but _but;
	int se, rv = MUNKI_OK;

	a1logd(p->log,2,"munki_getstatus: called\n");

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0x87, 0, 0, pbuf, 2, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		a1logd(p->log,1,"munki_getstatus: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	_spos = (mk_spos)pbuf[0];
	_but   = (mk_but)pbuf[1];

	if (p->log->debug >= 3) {
		char sb1[50], sb2[50];
		if (_spos == mk_spos_proj)
			strcpy(sb1, "Projector");
		else if (_spos == mk_spos_surf)
			strcpy(sb1, "Surface");
		else if (_spos == mk_spos_calib)
			strcpy(sb1, "Calibration");
		else if (_spos == mk_spos_amb)
			strcpy(sb1, "Ambient");
		else
			sprintf(sb1,"Unknown 0x%x",_spos);
		if (_but == mk_but_switch_release)
			strcpy(sb2, "Released");
		else if (_but == mk_but_switch_press)
			strcpy(sb2, "Pressed");
		else
			sprintf(sb2,"Unknown 0x%x",_but);

		a1logd(p->log,3,"munki_getstatus: Sensor pos. %s, Button state %s, ICOM err 0x%x\n",
	            sb1, sb2, se);
	}

	if (spos != NULL) *spos = _spos;
	if (but != NULL) *but = _but;

	return rv;
}

/* Set the indicator LED state (advanced) */
/* NOTE that the instrument seems to turn it off */
/* whenever any other sort of operation occurs. */
munki_code
munki_setindled(
    munki *p,
    int p1,			/* On time (msec) */
    int p2,			/* Off time (msec) */
    int p3,			/* Transition time (msec) */
    int p4,			/* Number of pulses, -1 = max */
    int p5			/* Ignored ? */
) {
	unsigned char pbuf[20];	/* command bytes written */
	int se, rv = MUNKI_OK;

	a1logd(p->log,2,"munki_setindled: %d, %d, %d, %d, %d\n",
	                   p1, p2, p3, p4, p5);

	int2buf(&pbuf[0], p1);		/* On time (msec) */
	int2buf(&pbuf[4], p2);		/* Off time (msec) */
	int2buf(&pbuf[8], p3);		/* Transition time (msec) */
	int2buf(&pbuf[12], p4);		/* Number of pulses, -1 = max */
	int2buf(&pbuf[16], p5);		/* Unknown */

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0x92, 0, 0, pbuf, 20, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		a1logd(p->log,1,"munki_setindled: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	a1logd(p->log,2,"munki_setindled: OK ICOM err 0x%x\n",se);

	return rv;
}

/* Trigger a measurement with the given measurement parameters */
munki_code
munki_triggermeasure(
	munki *p,
	int intclocks,		/* Number of integration clocks */
	int nummeas,		/* Number of measurements to make */
	int measmodeflags,	/* Measurement mode flags */
	int holdtempduty	/* Hold temperature duty cycle */
) {
	munkiimp *m = (munkiimp *)p->m;
	unsigned char pbuf[12];	/* command bytes written */
	int se, rv = MUNKI_OK;

	a1logd(p->log,2,"munki_triggermeasure: lamp %d, scan %d, gain %d, intclks %d, nummeas %d\n",
	          (measmodeflags & MUNKI_MMF_LAMP) ? 1 : 0,
	          (measmodeflags & MUNKI_MMF_SCAN) ? 1 : 0,
	          (measmodeflags & MUNKI_MMF_HIGHGAIN) ? 1 : 0,
              intclocks, nummeas);

	pbuf[0] = (measmodeflags & MUNKI_MMF_LAMP) ? 1 : 0;
	pbuf[1] = (measmodeflags & MUNKI_MMF_SCAN) ? 1 : 0;
	pbuf[2] = (measmodeflags & MUNKI_MMF_HIGHGAIN) ? 1 : 0;
	pbuf[3] = holdtempduty;
	int2buf(&pbuf[4], intclocks);
	int2buf(&pbuf[8], nummeas);

	m->tr_t1 = m->tr_t2 = m->tr_t3 = m->tr_t4 = m->tr_t5 = m->tr_t6 = m->tr_t7 = 0;
	m->tr_t1 = msec_time();     /* Diagnostic */

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0x80, 0, 0, pbuf, 12, 2.0);

	m->tr_t2 = msec_time();     /* Diagnostic */

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		a1logd(p->log,1,"munki_triggermeasure: failed with ICOM err 0x%x\n",se);
		return rv;
	}

	a1logd(p->log,2,"munki_triggermeasure: OK ICOM err 0x%x\n",se);

	return rv;
}

/* Read a measurements results. */
/* A buffer full of bytes is returned. */
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
) {
	munkiimp *m = (munkiimp *)p->m;
	unsigned char *ibuf = buf;	/* Incoming buffer */
	int nmeas;					/* Number of measurements for this read */
	double top, extra;			/* Time out period */
	int rwbytes;				/* Data bytes read or written */
	int se, rv = MUNKI_OK;
	int treadings = 0;
//	int gotshort = 0;			/* nz when got a previous short reading */

	if ((bsize % (m->nsen * 2)) != 0) {
		a1logd(p->log,1,"munki_readmeasurement: got %d bytes, nsen = %d\n",bsize,m->nsen);
		return MUNKI_INT_ODDREADBUF;
	}

	extra = 1.0;		/* Extra timeout margin */

#ifdef SINGLE_READ
	if (scanflag == 0)
		nmeas = inummeas;
	else
		nmeas = bsize / (m->nsen * 2);		/* Use a single large read */
#else
	nmeas = inummeas;				/* Smaller initial number of measurements */
#endif

	top = extra + m->c_inttime * nmeas;

	a1logd(p->log,2,"munki_readmeasurement: inummeas %d, scanflag %d, address %p bsize 0x%x, timout %f\n",inummeas, scanflag, buf, bsize, top);

	for (;;) {
		int size;		/* number of bytes to read */

		size = (m->nsen * 2) * nmeas;

		if (size > bsize) {		/* oops, no room for read */
			a1logd(p->log,1,"munki_readmeasurement: Buffer was too short for scan\n");
			return MUNKI_INT_MEASBUFFTOOSMALL;
		}

		m->tr_t6 = msec_time();	/* Diagnostic, start of subsequent reads */
		if (m->tr_t3 == 0) m->tr_t3 = m->tr_t6;		/* Diagnostic, start of first read */

		a1logd(p->log,5,"about to call usb_read with %d bytes\n",size);
		se = p->icom->usb_read(p->icom, NULL, 0x81, buf, size, &rwbytes, top);

		m->tr_t5 = m->tr_t7;
		m->tr_t7 = msec_time();	/* Diagnostic, end of subsequent reads */
		if (m->tr_t4 == 0) {
			m->tr_t5 = m->tr_t2;
			m->tr_t4 = m->tr_t7;	/* Diagnostic, end of first read */
		}

#ifdef NEVER		/* Use short + timeout to terminate scan */
		if (gotshort != 0 && se == ICOM_TO) {	/* We got a timeout after a short read. */ 
			a1logd(p->log,1,"Read timed out in %f secs after getting short read\n"
			                "(Trig & rd times %d %d %d %d)\n",
			         top,
			         m->tr_t2-m->tr_t1, m->tr_t3-m->tr_t2, m->tr_t4-m->tr_t3, m->tr_t6-m->tr_t5);
			break;		/* We're done */
		} else
#endif
		if (se == ICOM_SHORT) {		/* Expect this to terminate scan reading */
			a1logd(p->log,5,"Short read, read %d bytes, asked for %d\n"
			                "(Trig & rd times %d %d %d %d)\n",
					 rwbytes,size,
				     m->tr_t2-m->tr_t1, m->tr_t3-m->tr_t2, m->tr_t4-m->tr_t3, m->tr_t6-m->tr_t5);
		} else if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
			if (m->trig_rv != MUNKI_OK) {
				a1logd(p->log,1,"munki_readmeasurement: trigger failed, ICOM err 0x%x\n",m->trig_se);
				return m->trig_rv;
			}
			if (se & ICOM_TO)
				a1logd(p->log,1,"munki_readmeasurement: read timed out with top = %f\n",top);
		
			a1logd(p->log,1,"munki_readmeasurement: read failed, bytes read 0x%x, ICOM err 0x%x\n",rwbytes, se);
			return rv;
		}
 
		/* If we didn't read a multiple of m->nsen * 2, we've got problems */
		if ((rwbytes % (m->nsen * 2)) != 0) {
			a1logd(p->log,1,"munki_readmeasurement: read %d bytes, nsen %d, odd read error\n",rwbytes, m->nsen);
			return MUNKI_HW_ME_ODDREAD;
		}

		/* Track where we're up to */
		bsize -= rwbytes;
		buf   += rwbytes;
		treadings += rwbytes/(m->nsen * 2);

		if (scanflag == 0) {	/* Not scanning */

			/* Expect to read exactly what we asked for */
			if (rwbytes != size) {
				a1logd(p->log,1,"munki_readmeasurement: unexpected short read, got %d expected %d\n",rwbytes,size);
				return MUNKI_HW_ME_SHORTREAD;
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
			a1logd(p->log,5,"done because read %d bytes != %d\n",rwbytes,size);
			break;
		}
#endif

		if (bsize == 0) {		/* oops, no room for more scanning read */
			unsigned char tbuf[NSEN_MAX * 2];

			/* We need to clean up, so soak up all the data and throw it away */
			while ((se = p->icom->usb_read(p->icom, NULL, 0x81, tbuf, m->nsen * 2, &rwbytes, top)) == ICOM_OK)
				;
			a1logd(p->log,1,"munki_readmeasurement: buffer was too short for scan\n");
			return MUNKI_INT_MEASBUFFTOOSMALL;
		}

		/* Read a bunch more readings until the read is short or times out */
		nmeas = bsize / (m->nsen * 2);
		if (nmeas > 64)
			nmeas = 64;
		top = extra + m->c_inttime * nmeas;
	}

	/* Must have timed out in initial readings */
	if (treadings < inummeas) {
		a1logd(p->log,1,"munki_readmeasurement: read failed, bytes read 0x%x, ICOM err 0x%x\n",rwbytes, se);
		return MUNKI_RD_SHORTMEAS;
	} 

	if (p->log->debug >= 5) {
		int i, size = treadings * m->nsen * 2;
		char oline[100] = { '\000' }, *bp = oline;
		for (i = 0; i < size; i++) {
			if ((i % 16) == 0)
				bp += sprintf(bp,"    %04x:",i);
			bp += sprintf(bp," %02x",ibuf[i]);
			if ((i+1) >= size || ((i+1) % 16) == 0) {
				bp += sprintf(bp,"\n");
				a1logd(p->log,5,oline);
				bp = oline;
			}
		}
	}

	a1logd(p->log,2,"munki_readmeasurement: Read %d readings, ICOM err 0x%x\n"
	                "(Trig & rd times %d %d %d %d)\n",
	                 treadings, se,
					 m->tr_t2-m->tr_t1, m->tr_t3-m->tr_t2, m->tr_t4-m->tr_t3, m->tr_t6-m->tr_t5);

	if (nummeas != NULL) *nummeas = treadings;

	return rv;
}

/* Simulating an event */
munki_code munki_simulate_event(munki *p, mk_eve ecode,  int timestamp) {
	munkiimp *m = (munkiimp *)p->m;
	unsigned char pbuf[8];	/* 8 bytes to write */
	int se, rv = MUNKI_OK;

	a1logd(p->log,2,"munki_simulate_event: 0x%x\n",ecode);

	int2buf(&pbuf[0], ecode);
	int2buf(&pbuf[4], timestamp);	/* msec since munki power up */

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_DEVICE,
	                   0x8E, 0, 0, pbuf, 8, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK)
		a1logd(p->log,1,"munki_simulate_event: event 0x%x failed with ICOM err 0x%x\n",ecode,se);
	else 
		a1logd(p->log,2,"munki_simulate_event: 0x%x done, ICOM err 0x%x\n",ecode,se);

	/* Cancel the I/O in case there is no response*/
	msec_sleep(50);
	if (m->th_termed == 0) {
		a1logd(p->log,1,"munki_simulate_event: terminate switch thread failed, canceling I/O\n");
		p->icom->usb_cancel_io(p->icom, &m->sw_cancel);
	}

	return rv;
}

/* Wait for a reply triggered by an event */
munki_code munki_waitfor_switch(munki *p, mk_eve *ecode, int *timest, double top) {
	int rwbytes;			/* Data bytes read */
	unsigned char buf[8];	/* Result  */
	int se, rv = MUNKI_OK;
	mk_eve _ecode;
	int _timest;

	a1logd(p->log,2,"munki_waitfor_switch: Read 8 bytes from switch hit port\n");

	/* Now read 8 bytes */
	se = p->icom->usb_read(p->icom, NULL, 0x83, buf, 8, &rwbytes, top);

	if (se & ICOM_TO) {
		a1logd(p->log,1,"munki_waitfor_switch: read 0x%x bytes, timed out\n",rwbytes);
		return MUNKI_INT_BUTTONTIMEOUT;
	}

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		a1logd(p->log,2,"munki_waitfor_switch: read failed with ICOM err 0x%x\n",se);
		return rv;
	}

	if (rwbytes != 8) {
		a1logd(p->log,1,"munki_waitfor_switch: read %d bytes, short read error\n",rwbytes);
		return MUNKI_HW_EE_SHORTREAD;
	}

	_ecode  = (mk_eve) buf2int(&buf[0]);
	_timest = buf2int(&buf[4]);
	
	if (p->log->debug >= 3) {
		char sbuf[100];
		if (_ecode == mk_eve_none)
			strcpy(sbuf, "None");
		else if (_ecode == mk_eve_switch_press)
			strcpy(sbuf, "Button press");
		else if (_ecode == mk_eve_switch_release)
			strcpy(sbuf, "Button release");
		else if (_ecode == mk_eve_spos_change)
			strcpy(sbuf, "Sensor position change");
		else
			sprintf(sbuf,"Unknown 0x%x",_ecode);

		a1logd(p->log,3,"munki_waitfor_switch: Event %s, timestamp %d ICOM err 0x%x\n", sbuf, _timest, se);
	}

	a1logd(p->log,2,"munki_waitfor_switch: read %d bytes OK\n",rwbytes);

	if (ecode != NULL) *ecode = _ecode;
	if (timest != NULL) *timest = _timest;

	return rv; 
}

/* Wait for a reply triggered by a key press or config change (thread version) */
/* Returns MUNKI_OK if the switch has been pressed, */
/* or MUNKI_INT_BUTTONTIMEOUT if */
/* no switch was pressed befor the time expired, */
/* or some other error. */
munki_code munki_waitfor_switch_th(munki *p, mk_eve *ecode, int *timest, double top) {
	munkiimp *m = (munkiimp *)p->m;
	int rwbytes;			/* Data bytes read */
	unsigned char buf[8];	/* Result  */
	int se, rv = MUNKI_OK;
	mk_eve _ecode;
	int _timest;

	a1logd(p->log,2,"munki_waitfor_switch_th: Read 8 bytes from switch hit port\n");

	/* Now read 8 bytes */
	se = p->icom->usb_read(p->icom, &m->sw_cancel, 0x83, buf, 8, &rwbytes, top);

	if (se & ICOM_TO) {
		a1logd(p->log,1,"munki_waitfor_switch_th: read 0x%x bytes, timed out\n",rwbytes);
		return MUNKI_INT_BUTTONTIMEOUT;
	}

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		a1logd(p->log,2,"munki_waitfor_switch_th: read failed with ICOM err 0x%x\n",se);
		return rv;
	}

	if (rwbytes != 8) {
		a1logd(p->log,1,"munki_waitfor_switch_th: read %d bytes, short read error\n",rwbytes);
		return MUNKI_HW_EE_SHORTREAD;
	}

	_ecode  = (mk_eve) buf2int(&buf[0]);
	_timest = buf2int(&buf[4]);			/* msec since munki power up */

	if (p->log->debug >= 3) {
		char sbuf[100];
		if (_ecode == mk_eve_none)
			strcpy(sbuf, "None");
		else if (_ecode == mk_eve_switch_press)
			strcpy(sbuf, "Button press");
		else if (_ecode == mk_eve_switch_release)
			strcpy(sbuf, "Button release");
		else if (_ecode == mk_eve_spos_change)
			strcpy(sbuf, "Sensor position change");
		else
			sprintf(sbuf,"Unknown 0x%x",_ecode);

		a1logd(p->log,3,"munki_waitfor_switch_th: Event %s, timestamp %d ICOM err 0x%x\n", sbuf, _timest, se);
	}

	a1logd(p->log,2,"munki_waitfor_switch_th: read %d bytes OK\n",rwbytes);

	if (ecode != NULL) *ecode = _ecode;
	if (timest != NULL) *timest = _timest;

	return rv; 
}


/* ============================================================ */
/* key/value dictionary support for EEProm contents */

/* Fixup values for the window reference */

/* Check values */
static double proj_check[36] = {
	0.0,
	0.0,
	0.0,
	0.0,
	0.0,
	0.0,
	0.827859997749328610,
	0.849550008773803710,
	0.855490028858184810,
	0.858709990978240970,
	0.861010015010833740,
	0.862879991531372070,
	0.864109992980957030,
	0.864619970321655270,
	0.865379989147186280,
	0.865629971027374270,
	0.865689992904663090,
	0.865499973297119140,
	0.865499973297119140,
	0.865760028362274170,
	0.866209983825683590,
	0.866630017757415770,
	0.867579996585845950,
	0.868969976902008060,
	0.870270013809204100,
	0.871270000934600830,
	0.872340023517608640,
	0.873269975185394290,
	0.873669981956481930,
	0.873640000820159910,
	0.874390006065368650,
	0.873179972171783450,
	0.872720003128051760,
	0.872640013694763180,
	0.873160004615783690,
	0.873440027236938480
};


/* Correction values to emission ref. */
static double proj_fix[36] = {
	0.639684915542602540,
	0.639684915542602540,
	0.639684915542602540,
	0.812916100025177000,
	0.846581041812896730,
	0.854855418205261230,
	0.859299719333648680,
	0.861804306507110600,
	0.863713920116424560,
	0.865424513816833500,
	0.866307735443115230,
	0.867028772830963130,
	0.867631316184997560,
	0.868214190006256100,
	0.868206322193145750,
	0.868299305438995360,
	0.867988884449005130,
	0.868103504180908200,
	0.868657410144805910,
	0.869595944881439210,
	0.870542407035827640,
	0.871895790100097660,
	0.873195052146911620,
	0.874702811241149900,
	0.876054167747497560,
	0.877129673957824710,
	0.877931654453277590,
	0.877546310424804690,
	0.876341819763183590,
	0.875181615352630620,
	0.875020027160644530,
	0.875684559345245360,
	0.876559674739837650,
	0.876724362373352050,
	0.876553714275360110,
	0.875786423683166500
};

/* Initialise the calibration from the EEProm contents. */
/* (We're handed a buffer that's been rounded up to an even 32 bits by */
/* padding with zero's) */
munki_code munki_parse_eeprom(munki *p, unsigned char *buf, unsigned int len) {
	munkiimp *m = (munkiimp *)p->m;
	mkdata *d;
	int rv = MUNKI_OK;
	unsigned int chsum, sum;
	int calver, compver;			/* Calibration version and compatiblity version */
	unsigned char chipid[8];		/* Calibration chip id */
	int tint, *tinta;				/* Temporary */
	double tdouble;					/* Temporary */
	int i, j;

	a1logd(p->log,2,"munki_parse_eeprom: called with %d bytes\n",len);

	/* Check the checksum */
	chsum = buf2uint(buf+8);
	int2buf(buf+8, 0);				/* Zero it out */
	
	for (sum = 0, i = 0; i < (len-3); i += 4) {
		sum += buf2uint(buf + i);
	}



	a1logd(p->log,3,"munki_parse_eeprom: cal chsum = 0x%x, should be 0x%x - %s\n",sum,chsum, sum == chsum ? "OK": "BAD");
	if (sum != chsum)
		return MUNKI_INT_CALBADCHSUM;


	/* Create class to handle EEProm parsing */
	if ((d = m->data = new_mkdata(p, buf, len)) == NULL)
		return MUNKI_INT_CREATE_EEPROM_STORE;

	/* Check out the version */
	if (d->get_u16_ints(d, &calver, 0, 1) == NULL)
		return MUNKI_DATA_RANGE;
	if (d->get_u16_ints(d, &compver, 2, 1) == NULL)
		return MUNKI_DATA_RANGE;
	a1logd(p->log,4,"cal version = %d, compatible with %d\n",calver,compver);

	/* We understand versions 3 to 6 */

	if (calver < 3 || compver < 3 || compver > 6)
		return MUNKI_HW_CALIBVERSION;

	/* Choose the version we will treat it as */
	if (calver > 6 && compver <= 6)
		m->calver = 6;
	else
		m->calver = calver;
	a1logd(p->log,4,"Treating as cal version = %d\n",m->calver);

	/* Parse all the calibration info common for vers 3 - 6 */

	/* Production number */
	if (d->get_32_ints(d, &m->prodno, 12, 1) == NULL)
		return MUNKI_DATA_RANGE;
	a1logd(p->log,4,"Produnction no = %d\n",m->prodno);

	/* Chip HW ID */
	if (d->get_8_char(d, (unsigned char *)chipid, 16, 8) == NULL)
		return MUNKI_DATA_RANGE;
	a1logd(p->log,4,"HW Id = %02x-%02x%02x%02x%02x%02x%02x%02x\n",
		                           chipid[0], chipid[1], chipid[2], chipid[3],
		                           chipid[4], chipid[5], chipid[6], chipid[7]);

	/* Check that the chipid matches the calibration */
	for (i = 0; i < 8; i++) {
		if (chipid[i] != m->chipid[i])
			return MUNKI_HW_CALIBMATCH;
	}

	/* Serial number */
	if (d->get_8_asciiz(d, m->serno, 24, 16) == NULL)
		return MUNKI_DATA_RANGE;
	a1logd(p->log,4,"serial number '%s'\n",m->serno);

	/* Underlying calibration information */

	m->nsen = 137;			/* Sensor bands stored */
	m->nraw = 128;			/* Raw bands stored */
	m->nwav1 = 36;			/* Standard res number of cooked spectrum band */
	m->wl_short1 = 380.0;	/* Standard res short and long wavelengths */
	m->wl_long1 = 730.0;

	/* Fill this in here too */
	m->wl_short2 = HIGHRES_SHORT;
	m->wl_long2 = HIGHRES_LONG;
	m->nwav2 = (int)((m->wl_long2-m->wl_short2)/HIGHRES_WIDTH + 0.5) + 1;	

	/* Reflection wavelength calibration information */
	/* This is setup assuming 128 raw bands, starting */
	/* at offset 6 from the values returned by the hardware. */
	if ((m->rmtx_index1 = d->get_32_ints(d, NULL, 40, 36)) == NULL)
		return MUNKI_DATA_RANGE;

	/* Fake the number of matrix cooeficients for each out wavelength */
	if ((m->rmtx_nocoef1 = (int *)malloc(sizeof(int) * 36)) == NULL)
		return MUNKI_DATA_MEMORY;
	for (i = 0; i < 36; i++)
		m->rmtx_nocoef1[i] = 16;

	if ((m->rmtx_coef1 = d->get_32_doubles(d, NULL, 184, 36 * 16)) == NULL)
		return MUNKI_DATA_RANGE;

	if (p->log->debug >= 7) {
		a1logd(p->log,7,"Reflectance matrix:\n");
		for(i = 0; i < 36; i++) {
			a1logd(p->log,7," Wave %d, index %d\n",i, m->rmtx_index1[i]);
			for (j = 0; j < 16; j++) {
				if (m->rmtx_coef1[i * 16 + j] != 0.0)
					a1logd(p->log,7,"  Wt %d =  %f\n",j, m->rmtx_coef1[i * 16 + j]);
			}
		}
	}

	/* Emission wavelength calibration information */
	if ((m->emtx_index1 = d->get_32_ints(d, NULL, 2488, 36)) == NULL)
		return MUNKI_DATA_RANGE;

	/* Fake the number of matrix cooeficients for each out wavelength */
	if ((m->emtx_nocoef1 = (int *)malloc(sizeof(int) * 36)) == NULL)
		return MUNKI_DATA_MEMORY;
	for (i = 0; i < 36; i++)
		m->emtx_nocoef1[i] = 16;

	if ((m->emtx_coef1 = d->get_32_doubles(d, NULL, 2632, 36 * 16)) == NULL)
		return MUNKI_DATA_RANGE;

	if (p->log->debug >= 7) {
		a1logd(p->log,5,"Emmission matrix:\n");
		for(i = 0; i < 36; i++) {
			a1logd(p->log,7," Wave %d, index %d\n",i, m->emtx_index1[i]);
			for (j = 0; j < 16; j++) {
				if (m->emtx_coef1[i * 16 + j] != 0.0)
					a1logd(p->log,7,"  Wt %d =  %f\n",j, m->emtx_coef1[i * 16 + j]);
			}
		}
	}

	/* Linearization */
	if ((m->lin0 = d->rget_32_doubles(d, NULL, 4936, 4)) == NULL)
		return MUNKI_DATA_RANGE;
	m->nlin0 = 4;

	if ((m->lin1 = d->rget_32_doubles(d, NULL, 4952, 4)) == NULL)
		return MUNKI_DATA_RANGE;
	m->nlin1 = 4;

	if (p->log->debug >= 3) {
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

	/* Reflectance reference */
	if ((m->white_ref1 = d->get_32_doubles(d, NULL, 4968, 36)) == NULL)
		return MUNKI_DATA_RANGE;

	/* Emission reference */
	if ((m->emis_coef1 = d->get_32_doubles(d, NULL, 5112, 36)) == NULL)
		return MUNKI_DATA_RANGE;

	/* Ambient reference */
	if ((m->amb_coef1 = d->get_32_doubles(d, NULL, 5256, 36)) == NULL)
		return MUNKI_DATA_RANGE;

	/* Sensor target values */
	if (d->get_u16_ints(d, &tint, 5400, 1) == NULL)
		return MUNKI_DATA_RANGE;
	m->minsval = (double)tint;
	if (d->get_u16_ints(d, &tint, 5402, 1) == NULL)
		return MUNKI_DATA_RANGE;
	m->optsval = (double)tint;
	if (d->get_u16_ints(d, &tint, 5404, 1) == NULL)
		return MUNKI_DATA_RANGE;
	m->maxsval = (double)tint;
	if (d->get_u16_ints(d, &tint, 5406, 1) == NULL)
		return MUNKI_DATA_RANGE;
	m->satlimit = (double)tint;

	a1logd(p->log,4,"Sensor targmin %.0f, opt %.0f, max %.0f, sat %.0f\n",
	                           m->minsval,m->optsval,m->maxsval,m->satlimit);

	if (d->get_32_doubles(d, &m->cal_int_time, 5408, 1) == NULL)
		return MUNKI_DATA_RANGE;
	m->cal_int_time *= 1e-3;				/* Convert to seconds */

	if (d->get_32_ints(d, &tint, 5412, 1) == NULL)
		return MUNKI_DATA_RANGE;
	m->ledpreheattime = tint * 1e-3;	/* Convert to seconds */

	if (d->get_32_ints(d, &tint, 5416, 1) == NULL)
		return MUNKI_DATA_RANGE;
	m->ledwaittime = tint * 1e-3;		/* Convert to seconds */

	if (d->get_u16_ints(d, &m->ledholdtempdc, 5420, 1) == NULL)
		return MUNKI_DATA_RANGE;

	a1logd(p->log,4,"Cal int time %f, LED pre-heat %f, Led wait %f, LED hold temp duty cycle %d\n", m->cal_int_time, m->ledpreheattime, m->ledwaittime, m->ledholdtempdc);

	if (d->get_u16_ints(d, &tint, 5422, 1) == NULL)
		return MUNKI_DATA_RANGE;
	m->refinvalidsampt = tint * 1e-3;	/* Convert to seconds */

	if (d->get_32_ints(d, &tint, 5424, 1) == NULL)
		return MUNKI_DATA_RANGE;
	m->calscantime = tint * 1e-3;	/* Convert to seconds */

	a1logd(p->log,4,"Invalid sample time %f, Cal scan time %f\n",
	                           m->refinvalidsampt, m->calscantime);

	/* Stray light compensation. Note that 16 bit numbers are signed. */
	if ((tinta = d->get_16_ints(d, NULL, 5428, 36 * 36)) == NULL)
		return MUNKI_DATA_RANGE;
	if (m->calver >= 4) {
		if (d->get_32_doubles(d, &tdouble, 8020, 1) == NULL)
			return MUNKI_DATA_RANGE;
	} else {
		tdouble = 0.001;		/* Hmm. this is quite different to EEProm value */
	}
	/* Convert from ints to floats */
	m->straylight1 = dmatrixz(0, 35, 0, 35);  
	for (i = 0; i < 36; i++) {
		for (j = 0; j < 36; j++) {
			m->straylight1[i][j] = tdouble * tinta[i * 36 + j];
			if (i == j)
				m->straylight1[i][j] += 1.0;
		}
	}
	free(tinta);

	if (p->log->debug >= 7) {
		a1logd(p->log,7,"Stray Light matrix:\n");
		for(i = 0; i < 36; i++) {
			double sum = 0.0;
			a1logd(p->log,7," Wave %d, index %d\n",i, m->rmtx_index1[i]);
			for (j = 0; j < 36; j++) {
				sum += m->straylight1[i][j];
				a1logd(p->log,7,"  Wt %d = %f\n",j, m->straylight1[i][j]);
			}
			a1logd(p->log,7,"  Sum = %f\n",sum);
		}
	}

	if (m->calver >= 5) {
		/* Projector reference */
		if ((m->proj_coef1 = d->get_32_doubles(d, NULL, 8024, 36)) == NULL)
			return MUNKI_DATA_RANGE;

		/* Apparently this can be faulty though. Check if it is */
		for (i = 0; i < 6; i++) {
			if (m->proj_coef1[i] != m->proj_coef1[i])
				break;					/* Not Nan */
		}
		if (i == 6) {				/* First 6 are Nan's */
			for (; i < 36; i++) {
				if ((m->emis_coef1[i]/m->proj_coef1[i] - proj_check[i]) > 0.001)
					break;			/* Not less than 0.001 */
			}
		}
		if (i == 36) {		/* It's faulty */
			free(m->proj_coef1);
			m->proj_coef1 = NULL;		/* Fall through to fakeup */
		}
	}

	if (m->proj_coef1 == NULL) {		/* Fake up a projector reference */
		if ((m->proj_coef1 = (double *)malloc(sizeof(double) * 36)) == NULL)
			return MUNKI_DATA_MEMORY;
		for (i = 0; i < 36; i++) {
			m->proj_coef1[i] = m->emis_coef1[i]/proj_fix[i];
		}
		a1logd(p->log,4,"Faked up projector cal reference\n");
	}

	if (m->calver >= 6) {
		if (d->get_8_ints(d, &m->adctype, 8168, 1) == NULL)
			return MUNKI_DATA_RANGE;
	} else {
		m->adctype = 0;
	}

	if (p->log->debug >= 7) {
		a1logd(p->log,4,"White ref, emission cal, ambient cal, proj cal:\n");
		for(i = 0; i < 36; i++) {
			a1logd(p->log,7," %d: %f, %f, %f, %f\n",i, m->white_ref1[i], m->emis_coef1[i],
			                                          m->amb_coef1[i], m->proj_coef1[i]);
		}
	}

#ifdef PLOT_RCALCURVE
	/* Plot the reflection reference curve */
	{
		int i;
		double xx[36];
		double y1[36];
	
		for (i = 0; i < m->nwav1; i++) {
			xx[i] = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, i);
			y1[i] = m->white_ref1[i];
		}
		printf("Reflection Reference (Black)\n");
		do_plot(xx, y1, NULL, NULL, 36);
	}
#endif /* PLOT_RCALCURVE */

#ifdef PLOT_ECALCURVES
	/* Plot the emission reference curves */
	{
		int i;
		double xx[36];
		double y1[36], y2[36], y3[36];
	
		printf("Emission Reference (Black), Ambient (Red), Projector (Green)\n");
		for (i = 0; i < m->nwav1; i++) {
			xx[i] = XSPECT_WL(m->wl_short1, m->wl_long1, m->nwav1, i);
			y1[i] = m->emis_coef1[i];
			y2[i] = m->amb_coef1[i];
			y3[i] = m->proj_coef1[i];
			if (y3[i] > 0.02)
				y3[i] = 0.02;
		}
		do_plot(xx, y1, y2, y3, 36);
	}
#endif /* PLOT_ECALCURVES */

	/* Default to standard resolution */
	m->nwav = m->nwav1;
	m->wl_short = m->wl_short1;
	m->wl_long = m->wl_long1; 

	m->rmtx_index  = m->rmtx_index1;
	m->rmtx_nocoef = m->rmtx_nocoef1;
	m->rmtx_coef   = m->rmtx_coef1;
	m->emtx_index  = m->emtx_index1;
	m->emtx_nocoef = m->emtx_nocoef1;
	m->emtx_coef   = m->emtx_coef1;

	m->white_ref = m->white_ref1;
	m->emis_coef = m->emis_coef1;
	m->amb_coef  = m->amb_coef1;
	m->proj_coef = m->proj_coef1;
	m->straylight = m->straylight1;
	
	m->highgain = 1.0/m->lin1[1];	/* Gain is encoded in linearity */
	a1logd(p->log,3, "highgain = %f\n",m->highgain);

	return rv;
}

		
/* Return a pointer to an array of chars containing data from 8 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range. */
static unsigned char *mkdata_get_8_char(struct _mkdata *d, unsigned char *rv, int off, int count) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 1) > d->len)
		return NULL;

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
static char *mkdata_get_8_asciiz(struct _mkdata *d, char *rv, int off, int count) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 1) > d->len)
		return NULL;

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
static int *mkdata_get_8_ints(struct _mkdata *d, int *rv, int off, int count) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 1) > d->len)
		return NULL;

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
static int *mkdata_get_u8_ints(struct _mkdata *d, int *rv, int off, int count) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 1) > d->len)
		return NULL;

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
static int *mkdata_get_16_ints(struct _mkdata *d, int *rv, int off, int count) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 2) > d->len)
		return NULL;

	if (rv == NULL) {
		if ((rv = (int *)malloc(sizeof(int) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 2) {
		rv[i] = buf2short(d->buf + off);
	}
	return rv;
}

/* Return a pointer to an array of ints containing data from unsigned 16 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range. */
static int *mkdata_get_u16_ints(struct _mkdata *d, int *rv, int off, int count) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 2) > d->len)
		return NULL;

	if (rv == NULL) {
		if ((rv = (int *)malloc(sizeof(int) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 2) {
		rv[i] = buf2ushort(d->buf + off);
	}
	return rv;
}

/* Return a pointer to an array of ints containing data from 32 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range. */
static int *mkdata_get_32_ints(struct _mkdata *d, int *rv, int off, int count) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 4) > d->len)
		return NULL;

	if (rv == NULL) {
		if ((rv = (int *)malloc(sizeof(int) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 4) {
		rv[i] = buf2int(d->buf + off);
	}
	return rv;
}

/* Return a pointer to an array of unsigned ints containing data from unsigned 32 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range. */
static unsigned int *mkdata_get_u32_uints(struct _mkdata *d, unsigned int *rv, int off, int count) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 4) > d->len)
		return NULL;

	if (rv == NULL) {
		if ((rv = (unsigned int *)malloc(sizeof(unsigned int) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 4) {
		rv[i] = buf2uint(d->buf + off);
	}
	return rv;
}

/* Return a pointer to an array of doubles containing data from 32 bits. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range or malloc failure. */
static double *mkdata_get_32_doubles(struct _mkdata *d, double *rv, int off, int count) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 4) > d->len)
		return NULL;

	if (rv == NULL) {
		if ((rv = (double *)malloc(sizeof(double) * count)) == NULL)
			return NULL;
	}

	for (i = 0; i < count; i++, off += 4) {
		unsigned int val;
		val = buf2uint(d->buf + off);
		rv[i] = IEEE754todouble(val);
	}
	return rv;
}

/* Return a pointer to an array of doubles containing data from 32 bits, */
/* with the array filled in reverse order. */
/* If rv is NULL, the returned value will have been allocated, othewise */
/* the rv will be returned. Return NULL if out of range or malloc failure. */
static double *mkdata_rget_32_doubles(struct _mkdata *d, double *rv, int off, int count) {
	int i;

	if (count <= 0
	 || off < 0
	 || (off + count * 4) > d->len)
		return NULL;

	if (rv == NULL) {
		if ((rv = (double *)malloc(sizeof(double) * count)) == NULL)
			return NULL;
	}

	for (i = count-1; i >= 0; i--, off += 4) {
		unsigned int val;
		val = buf2uint(d->buf + off);
		rv[i] = IEEE754todouble(val);
	}
	return rv;
}


/* Destroy ourselves */
static void mkdata_del(mkdata *d) {
	del_a1log(d->log);		/* Unref it */
	free(d);
}

/* Constructor for mkdata */
mkdata *new_mkdata(munki *p, unsigned char *buf, int len) {
	mkdata *d;
	if ((d = (mkdata *)calloc(1, sizeof(mkdata))) == NULL) {
		a1loge(p->log, 1, "new_mkdata: malloc failed!\n");
		return NULL;
	}

	d->p = p;

	d->log = new_a1log_d(p->log);	/* Take reference */

	d->buf = buf;
	d->len = len;

	d->get_8_char      = mkdata_get_8_char;
	d->get_8_asciiz    = mkdata_get_8_asciiz;
	d->get_8_ints      = mkdata_get_8_ints;
	d->get_u8_ints     = mkdata_get_u8_ints;
	d->get_16_ints     = mkdata_get_16_ints;
	d->get_u16_ints    = mkdata_get_u16_ints;
	d->get_32_ints     = mkdata_get_32_ints;
	d->get_u32_uints   = mkdata_get_u32_uints;
	d->get_32_doubles  = mkdata_get_32_doubles;
	d->rget_32_doubles = mkdata_rget_32_doubles;

	d->del           = mkdata_del;

	return d;
}

/* ----------------------------------------------------------------- */
