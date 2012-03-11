
 /* X-Rite ColorMunki related functions */

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
#include <fcntl.h>
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
#include "icoms.h"
#include "conv.h"
#include "sort.h"

/* Configuration */
#define USE_THREAD		/* [Def] Need to use thread, or there are 1.5 second internal */
						/* instrument delays ! */
#define ENABLE_NONVCAL	/* [Def] Enable saving calibration state between program runs in a file */
#define ENABLE_NONLINCOR	/* [Def] Enable non-linear correction */
						/* NOTE :- high gain scaling will be stuffed if disabled! */
#define ENABLE_LEDTEMPC	/* [Def] Enable LED temperature compensation */
#define ENABLE_SPOS_CHECK	/* [Def] Chech the sensor position is reasonable for measurement */
#define CALTOUT (24 * 60 * 60)	/* [24 Hrs] Calibration timeout in seconds */
#define MAXSCANTIME 15.0	/* [14 Sec] Maximum scan time in seconds */
#define SW_THREAD_TIMEOUT (10 * 60.0) 	/* [10 Min] Switch read thread timeout */

#define SINGLE_READ		/* [Def] Use a single USB read for scan to eliminate latency issues. */
#define HIGH_RES		/* [Def] Enable high resolution spectral mode code. Disable */
						/* to break dependency on rspl library. */

/* Debug [Und] */
#undef DEBUG			/* Turn on debug printfs */
#undef PLOT_DEBUG		/* Use plot to show readings & processing */
#undef RAWR_DEBUG		/* Print out raw reading processing values */
#undef DUMP_SCANV		/* Dump scan readings to a file "mkdump.txt" */
#undef APPEND_MEAN_EMMIS_VAL /* Append averaged uncalibrated reading to file "mkdump.txt" */
#undef IGNORE_WHITE_INCONS	/* Ignore define reference reading inconsistency */
#undef TEST_DARK_INTERP	/* Test out the dark interpolation (need DEBUG too) */
#undef PLOT_RCALCURVE	/* Plot the reflection reference curve */
#undef PLOT_ECALCURVES	/* Plot the emission reference curves */
#undef PLOT_TEMPCOMP	/* Plot before and after temp. compensation */
#undef PATREC_DEBUG		/* Print & Plot patch/flash recognition information */
#undef HIGH_RES_DEBUG
#undef HIGH_RES_PLOT
#undef HIGH_RES_PLOT_STRAYL		/* Plot strat light upsample */


#define DISP_INTT 0.7			/* Seconds per reading in display spot mode */
								/* More improves repeatability in dark colors, but limits */
								/* the maximum brightness level befor saturation. */
								/* A value of 2.0 seconds has a limit of about 110 cd/m^2 */
#define DISP_INTT2 0.3			/* High brightness display spot mode seconds per reading, */
								/* Should be good up to 275 cd/m^2 */
#define DISP_INTT3 0.1			/* High brightness display spot mode seconds per reading, */
								/* Should be good up to 700 cd/m^2 */

#define ADARKINT_MAX 1.0		/* Max cal time for adaptive dark cal */

#define SCAN_OP_LEV 0.10		/* Degree of optimimum sensor value to aim for */
								/* Make it scan as fast as possible */

#define RDEAD_TIME 0.004432		/* Fudge figure to make reflecting intn. time scale linearly */
					
#define EMIS_SCALE_FACTOR 1.0	/* Emission mode scale factor */ 
#define AMB_SCALE_FACTOR (1.0/3.141592654)	/* Ambient mode scale factor - convert */ 
								/* from Lux to Lux/PI */
								/* These factors get the same behaviour as the GMB drivers. */

#define NRAWB 274		/* Number of raw bytes in a single reading */
#define NRAW 128		/* Number of raw sensor values (128 out of 137) */
						/* The actual sensor values used are indexes 6 to 134 inclusive */

/* High res mode settings */
#define HIGHRES_SHORT 360		/* Wavelength to calculate */
#define HIGHRES_LONG  740
#define HIGHRES_WIDTH  (10.0/3.0) /* (The 3.3333 spacing and lanczos2 seems a good combination) */
#define HIGHRES_REF_MIN 410.0	  /* Too much stray light below this in reflective mode */
#define HIGHRES_TRANS_MIN 380.0	  /* Too much stray light below this in reflective mode */

#include "munki.h"
#include "munki_imp.h"

/* - - - - - - - - - - - - - - - - - - */

#define PATCH_CONS_THR 0.05		/* Dark measurement consistency threshold */
#define DARKTHSCAMIN 5000.0		/* Dark threshold scaled/offset minimum */

/* - - - - - - - - - - - - - - - - - - */

#if defined(DEBUG)

#define dbgo stderr
#define DBG(xxx) fprintf xxx ;
#define RDBG(xxx) fprintf xxx ;
#define RLDBG(level, xxx) fprintf xxx ;

#else

#define dbgo stderr
#define DBG(xxx)
#define RDBG(xxx) if (p->debug) fprintf xxx ;
#define RLDBG(level, xxx) if (p->debug >= level) fprintf xxx ;
	
#endif	/* DEBUG */

/* Three levels of runtime debugging messages:

   1 = default, typical I/O messages etc.
   2 = more internal operation messages
   3 = dump extra detailes
   4 = dump EEPROM data
   5 = dump tables etc
*/

/* ============================================================ */
/* Debugging plot support */

#if defined(PLOT_DEBUG) || defined(PATREC_DEBUG) || defined(HIGH_RES_PLOT) ||  defined(HIGH_RES_PLOT_STRAYL)

# include <plot.h>

static int disdebplot = 0;

# define DISDPLOT disdebplot = 1;
# define ENDPLOT disdebplot = 0;

/* Plot a CCD spectra */
void plot_raw(double *data) {
	int i;
	double xx[NRAW];
	double yy[NRAW];

	if (disdebplot)
		return;

	for (i = 0; i < NRAW; i++) {
		xx[i] = (double)i;
		yy[i] = data[i];
	}
	do_plot(xx, yy, NULL, NULL, NRAW);
}

/* Plot two CCD spectra */
void plot_raw2(double *data1, double *data2) {
	int i;
	double xx[NRAW];
	double y1[NRAW];
	double y2[NRAW];

	if (disdebplot)
		return;

	for (i = 0; i < NRAW; i++) {
		xx[i] = (double)i;
		y1[i] = data1[i];
		y2[i] = data2[i];
	}
	do_plot(xx, y1, y2, NULL, NRAW);
}

/* Plot a converted spectra */
void plot_wav(munkiimp *m, double *data) {
	int i;
	double xx[NRAW];
	double yy[NRAW];

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
	double xx[NRAW];
	double yy[NRAW];

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
int munki_average_multimeas(
	munki *p,
	double *avg,			/* return average [nraw] */
	double **multimeas,		/* Array of [nummeas][nraw] value to average */
	int nummeas,			/* number of readings to be averaged */
	double *poallavg,		/* If not NULL, return overall average of bands and measurements */
	double darkthresh		/* Dark threshold (used for consistency check scaling) */
);

/* Implementation struct */

/* Add an implementation structure */
munki_code add_munkiimp(munki *p) {
	munkiimp *m;

	if ((m = (munkiimp *)calloc(1, sizeof(munkiimp))) == NULL) {
		DBG((dbgo,"add_munkiimp malloc %lu bytes failed (1)\n",sizeof(munkiimp)))
		if (p->verb) printf("Malloc %lu bytes failed (1)\n",sizeof(munkiimp));
		return MUNKI_INT_MALLOC;
	}
	m->p = p;

	p->m = (void *)m;
	return MUNKI_OK;
}

/* Shutdown instrument, and then destroy */
/* implementation structure */
void del_munkiimp(munki *p) {

	DBG((dbgo,"munki_del called\n"))
	if (p->m != NULL) {
		int i;
		munkiimp *m = (munkiimp *)p->m;
		munki_state *s;

		if (m->th != NULL) {		/* Terminate switch monitor thread by simulating an event */
			m->th_term = 1;			/* Tell thread to exit on error */
			munki_simulate_event(p, mk_eve_spos_change, 0);
			for (i = 0; m->th_termed == 0 && i < 5; i++)
				msec_sleep(50);	/* Wait for thread to terminate */
			if (i >= 5) {
				DBG((dbgo,"Munki switch thread termination failed\n"))
			}
			m->th->del(m->th);
		}

		/* Free any per mode data */
		for (i = 0; i < mk_no_modes; i++) {
			s = &m->ms[i];

			free_dvector(s->dark_data, 0, m->nraw-1);  
			free_dvector(s->dark_data2, 0, m->nraw-1);  
			free_dvector(s->dark_data3, 0, m->nraw-1);  
			free_dvector(s->white_data, 0, m->nraw-1);
			free_dmatrix(s->iwhite_data, 0, 1, 0, m->nraw-1);  
			free_dmatrix(s->idark_data, 0, 3, 0, m->nraw-1);  

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

	DBG((dbgo,"munki_init:\n"))

	if (p->itype != instColorMunki)
		return MUNKI_UNKNOWN_MODEL;

#ifdef ENABLE_SPOS_CHECK
	m->nosposcheck = 0;
#else
	m->nosposcheck = 1;
#endif

	m->trig = inst_opt_trig_keyb;
	m->scan_toll_ratio = 1.0;

	/* Get the firmware parameters so that we can check eeprom range. */
	if ((ev = munki_getfirm(p, &m->fwrev, &m->tickdur, &m->minintcount, &m->noeeblocks, &m->eeblocksize)) != MUNKI_OK)
		return ev; 
	if (p->debug >= 2) fprintf(stderr,"Firmware rev = %d.%d\n",m->fwrev/256, m->fwrev % 256);

#ifdef NEVER	// Dump the eeprom contents to stdout
	{
		unsigned int i, ii, r, ph;
		int base, size;
	
		if (m->noeeblocks != 2 || m->eeblocksize != 8192)
			error("EEProm is unexpected size");

		size = 8192;
		for (base = 0; base < (2 * 8192); base += 8192) {
			unsigned char eeprom[8192];

			if ((ev = munki_readEEProm(p, eeprom, base, size)) != MUNKI_OK)
				return ev;
		
			ii = i = ph = 0;
			for (r = 1;; r++) {		/* count rows */
				int c = 1;			/* Character location */

				c = 1;
				if (ph != 0) {	/* Print ASCII under binary */
					printf("       ");
					i = ii;				/* Swap */
					c += 6;
				} else {
					printf("0x%04lx: ",i + base);
					ii = i;				/* Swap */
					c += 6;
				}
				while (i < size && c < 53) {
					if (ph == 0) 
						printf("%02x ",eeprom[i]);
					else {
						if (isprint(eeprom[i]))
							printf(" %c ",eeprom[i]);
						else
							printf("   ",eeprom[i]);
					}
					c += 3;
					i++;
				}
				if (ph == 0 || i < size)
					printf("\n");

				if (ph == 1 && i >= size) {
					printf("\n");
					break;
				}
				if (ph == 0)
					ph = 1;
				else
					ph = 0;

			}
		}
	}

#endif	// NEVER
	
	/* Tick in seconds */
	m->intclkp = (double)m->tickdur * 1e-6;

	/* Set these to reasonable values */
	m->min_int_time = m->intclkp * (double)m->minintcount;
	m->max_int_time = 5.0;

	DBG((dbgo, "minintcount %d, min_int_time = %f\n", m->minintcount, m->min_int_time))

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
		DBG((dbgo,"munki_imp_init malloc %d bytes failed\n",rucalsize))
		if (p->verb) printf("Malloc %d bytes failed\n",rucalsize);
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
	if ((m->th = new_athread(munki_switch_thread, (void *)p)) == NULL)
		return MUNKI_INT_THREADFAILED;
#endif

	/* Set up the current state of each mode */
	{
		int i, j;
		munki_state *s;

		/* First set state to basic configuration */
		for (i = 0; i < mk_no_modes; i++) {
			s = &m->ms[i];

			/* Default to an emissive configuration */
			s->targoscale = 0.90;	/* Allow extra 10% margine by default */
			s->gainmode = 0;		/* Normal gain mode */
			s->inttime = 0.5;		/* Initial integration time */


			s->dark_valid = 0;		/* Dark cal invalid */
			s->dark_data = dvectorz(0, m->nraw-1);  
			s->dark_data2 = dvectorz(0, m->nraw-1);  
			s->dark_data3 = dvectorz(0, m->nraw-1);  

			s->cal_valid = 0;		/* Scale cal invalid */
			s->cal_factor1 = dvectorz(0, m->nwav1-1);
			s->cal_factor2 = dvectorz(0, m->nwav2-1);
			s->cal_factor = s->cal_factor1; /* Default to standard resolution */
			s->white_data = dvectorz(0, m->nraw-1);
			s->iwhite_data = dmatrixz(0, 1, 0, m->nraw-1);  

			s->idark_valid = 0;		/* Interpolatable Dark cal invalid */
			s->idark_data = dmatrixz(0, 3, 0, m->nraw-1);  

			s->min_wl = 0.0;		/* Default minimum to report */

			s->dark_int_time  = DISP_INTT;	/* 0.7 */
			s->dark_int_time2 = DISP_INTT2;	/* 0.3 */
			s->dark_int_time3 = DISP_INTT3;	/* 0.1 */

			s->need_calib = 1;		/* By default always need a calibration at start */
			s->need_dcalib = 1;
		}

		/* Then add mode specific settings */
		for (i = 0; i < mk_no_modes; i++) {
			s = &m->ms[i];
			switch(i) {
				case mk_refl_spot:
					s->targoscale = 1.0;	/* Optimised sensor scaling to full */
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
					s->min_wl = HIGHRES_REF_MIN; /* Not enogh illumination to go below this */
					break;

				case mk_refl_scan:
					s->targoscale = SCAN_OP_LEV;	/* Maximize scan rate */
					s->reflective = 1;
					s->scan = 1;
					s->adaptive = 0;
					s->inttime = (s->targoscale * m->cal_int_time - RDEAD_TIME) + RDEAD_TIME;
					if (s->inttime < m->min_int_time)
						s->inttime = m->min_int_time;
					s->dark_int_time = s->inttime;

					s->dpretime = 0.20;		/* Pre-measure time */
					s->wpretime = 0.20;
					s->dcaltime = 0.5;
					s->wcaltime = 0.5;
					s->dreadtime = 0.10;
					s->wreadtime = 0.10;
					s->maxscantime = MAXSCANTIME;
					s->min_wl = HIGHRES_REF_MIN; /* Not enogh illumination to go below this */
					break;

				case mk_disp_spot:				/* Same as emissive spot, but not adaptive */
				case mk_proj_spot:				/* Same as tele spot, but not adaptive */
					s->targoscale = 0.90;		/* Allow extra 10% margine */
					if (i == mk_disp_spot) {
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
				case mk_tele_spot:			/* Adaptive projector */
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

					if (s->scan)
						s->idark_int_time[0] = s->idark_int_time[2] = s->inttime;
					else
						s->idark_int_time[0] = s->idark_int_time[2] = m->min_int_time;
					s->idark_int_time[1] = s->idark_int_time[3] = 1.0;

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

					if (s->scan)
						s->idark_int_time[0] = s->idark_int_time[2] = s->inttime;
					else
						s->idark_int_time[0] = s->idark_int_time[2] = m->min_int_time;
					s->idark_int_time[1] = s->idark_int_time[3] = 1.0;

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
					s->min_wl = HIGHRES_TRANS_MIN;	/* Too much stray light below this ? */
					break;

				case mk_trans_scan:
					s->targoscale = 0.90;		/* Allow extra 10% margine */
					s->trans = 1;
					s->scan = 1;
					s->targoscale = SCAN_OP_LEV;
					s->inttime = s->targoscale * m->cal_int_time;
					if (s->inttime < m->min_int_time)
						s->inttime = m->min_int_time;
					s->dark_int_time = s->inttime;
					s->adaptive = 0;

					if (s->scan)
						s->idark_int_time[0] = s->idark_int_time[2] = s->inttime;
					else
						s->idark_int_time[0] = s->idark_int_time[2] = m->min_int_time;
					s->idark_int_time[1] = s->idark_int_time[3] = 1.0;

					s->dpretime = 0.20;
					s->wpretime = 0.20;
					s->dcaltime = 1.0;
					s->wcaltime = 1.0;
					s->dreadtime = 0.00;
					s->wreadtime = 0.10;
					s->maxscantime = MAXSCANTIME;
					s->min_wl = HIGHRES_TRANS_MIN;	/* Too much stray light below this ? */
					break;
			}
		}
	}

#ifdef ENABLE_NONVCAL
	/* Restore the all modes calibration from the local system */
	munki_restore_calibration(p);
#endif
	
	if (p->verb) {
		printf("Instrument Type:   ColorMunki\n");		// ~~ should get this from version string ?
		printf("Serial Number:     %s\n",m->serno);
		printf("Firmware version:  %d\n",m->fwrev);
		printf("Chip ID:           %02x-%02x%02x%02x%02x%02x%02x%02x\n",
		                           m->chipid[0], m->chipid[1], m->chipid[2], m->chipid[3],
		                           m->chipid[4], m->chipid[5], m->chipid[6], m->chipid[7]);
		printf("Version string:    '%s'\n",m->vstring);
		printf("Calibration Ver.:  %d\n",m->calver);
		printf("Production No.:    %d\n",m->prodno);
	}

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
	mk_mode mmode,
	int spec_en		/* nz to enable reporting spectral */
) {
	munkiimp *m = (munkiimp *)p->m;

	DBG((dbgo,"munki_imp_set_mode called with %d\n",mmode))
	switch(mmode) {
		case mk_refl_spot:
		case mk_refl_scan:
		case mk_disp_spot:
		case mk_proj_spot:
		case mk_emiss_spot:
		case mk_tele_spot:
		case mk_emiss_scan:
		case mk_amb_spot:
		case mk_amb_flash:
		case mk_trans_spot:
		case mk_trans_scan:
			m->mmode = mmode;
			m->spec_en = spec_en ? 1 : 0;
			return MUNKI_OK;
		case mk_no_modes:
			return MUNKI_INT_ILLEGALMODE;
		default:
			break;
	}
	return MUNKI_INT_ILLEGALMODE;
}

/* Determine if a calibration is needed. */
inst_cal_type munki_imp_needs_calibration(
	munki *p
) {
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	time_t curtime = time(NULL);

//printf("~1 checking mode %d\n",m->mmode);

	/* Timout calibrations that are too old */
//printf("~1 curtime = %u, iddate = %u\n",curtime,s->iddate);
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
//printf("~1 need cal ref white\n");
		return inst_calt_ref_white;

	} else if (   (s->trans && !s->cal_valid)
	           || (s->trans && s->need_calib && !m->noautocalib)) {
//printf("~1 need cal trans white\n");
		return inst_calt_trans_white;

	} else if (s->emiss && !s->scan && !s->adaptive && !s->done_dintcal) {
//printf("~1 need cal disp/proj inttime\n");
		if (s->projector)
			return inst_calt_proj_int_time; 
		else
			return inst_calt_disp_int_time; 
	}
	return inst_calt_none;
}

/* - - - - - - - - - - - - - - - - */
/* Calibrate for the current mode. */
/* Request an instrument calibration of the current mode. */
munki_code munki_imp_calibrate(
	munki *p,
	inst_cal_type caltp,	/* Calibration type. inst_calt_all for all neeeded */
	inst_cal_cond *calc,	/* Current condition/desired condition */
	char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	inst_cal_type calt = caltp; /* Specific calibration type */
	int nummeas = 0;
	int transwarn = 0;
	mk_spos spos;
	int i, j, k;

	DBG((dbgo,"munki_imp_calibrate called with calt 0x%x, calc 0x%x\n",caltp, *calc))

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
			if (s->projector)
				calt = inst_calt_proj_int_time;
			else
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
	 && calt != inst_calt_disp_int_time
	 && calt != inst_calt_proj_int_time)
		return MUNKI_UNSUPPORTED;

	/* Get current sensor position */
	if ((ev = munki_getstatus(p, &spos, NULL)) != MUNKI_OK)
		return ev;

	/* Make sure there's the right condition for the calibration */
	if (calt == inst_calt_ref_white) {			/* Reflective white calib */
		if (*calc != inst_calc_man_cal_smode) {
			*calc = inst_calc_man_cal_smode;
			return MUNKI_CAL_SETUP;
		}
		if (!m->nosposcheck && spos != mk_spos_calib)
			return MUNKI_SPOS_CALIB;
	} else if (calt == inst_calt_em_dark) {		/* Emissive Dark calib */
		id[0] = '\000';
		if (*calc != inst_calc_man_cal_smode) {
			*calc = inst_calc_man_cal_smode;
			return MUNKI_CAL_SETUP;
		}
		if (!m->nosposcheck && spos != mk_spos_calib)
			return MUNKI_SPOS_CALIB;
	} else if (calt == inst_calt_trans_dark) {	/* Transmissive dark */
		id[0] = '\000';
		if (*calc != inst_calc_man_cal_smode) {
			*calc = inst_calc_man_cal_smode;
			return MUNKI_CAL_SETUP;
		}
		if (!m->nosposcheck && spos != mk_spos_calib)
			return MUNKI_SPOS_CALIB;
	} else if (calt == inst_calt_trans_white) {	/* Transmissive white */
		id[0] = '\000';
		if (*calc != inst_calc_man_trans_white) {
			*calc = inst_calc_man_trans_white;
			return MUNKI_CAL_SETUP;
		}
		if (!m->nosposcheck && spos != mk_spos_surf)
			return MUNKI_SPOS_SURF;
	}

	/* We are now either in inst_calc_man_cal_smode, */ 
	/* inst_calc_man_trans_white, inst_calc_disp_white or inst_calc_proj_white */
	/* condition for it. */

	/* Reflective uses on the fly black, but we need a black for white calib. */
	if (s->reflective && *calc == inst_calc_man_cal_smode) {
		int stm;

		nummeas = munki_comp_nummeas(p, s->dcaltime, s->inttime);

		DBG((dbgo,"Doing initial reflective black calibration with dcaltime %f, int_time %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode))
		stm = msec_time();
		if ((ev = munki_dark_measure(p, s->dark_data, nummeas, &s->inttime, s->gainmode))
	                                                                         != MUNKI_OK) {
			return ev;
		}
		if (p->debug) fprintf(stderr,"Execution time of dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);

		s->dark_valid = 1;
		s->need_dcalib = 0;
		s->ddate = time(NULL);
		s->dark_int_time = s->inttime;
		s->dark_gain_mode = s->gainmode;
	}

	/* Emsissive scan uses the fastest possible scan rate (??) */
	if (s->emiss && !s->adaptive && s->scan && *calc == inst_calc_man_cal_smode) {
		int stm;

		nummeas = munki_comp_nummeas(p, s->dcaltime, s->inttime);

		DBG((dbgo,"Doing display black calibration with dcaltime %f, int_time %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode))
		stm = msec_time();
		if ((ev = munki_dark_measure(p, s->dark_data, nummeas, &s->inttime, s->gainmode))
	                                                                         != MUNKI_OK) {
			return ev;
		}
		if (p->debug) fprintf(stderr,"Execution time of dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);

		s->dark_valid = 1;
		s->need_dcalib = 0;
		s->ddate = time(NULL);
		s->dark_int_time = s->inttime;
		s->dark_gain_mode = s->gainmode;

		/* Save the calib to all similar modes */
		for (i = 0; i < mk_no_modes; i++) {
			munki_state *ss = &m->ms[i];
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

	/* Emmissive adaptive and transmissive black reference. */
	/* The integration time and gain needs to be variable. */
	if ((s->emiss && s->adaptive && !s->scan && *calc == inst_calc_man_cal_smode)
	 || (s->trans && s->adaptive && !s->scan && *calc == inst_calc_man_cal_smode)) {
		/* Adaptive where we can't measure the black reference on the fly, */
		/* so bracket it and interpolate. */
		/* The black reference is probably temperature dependent, but */
		/* there's not much we can do about this. */

		if (s->scan)
			s->idark_int_time[0] = s->inttime;		/* Use nominal time for scan */
		else
			s->idark_int_time[0] = m->min_int_time;
		nummeas = munki_comp_nummeas(p, s->dcaltime, s->idark_int_time[0]);
		DBG((dbgo,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[0] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[0], nummeas, 0))
		if ((ev = munki_dark_measure(p, s->idark_data[0], nummeas, &s->idark_int_time[0], 0))
		                                                                          != MUNKI_OK) {
			return ev;
		}
	
		s->idark_int_time[1] = ADARKINT_MAX; /* 1.0 */
		nummeas = munki_comp_nummeas(p, s->dcaltime, s->idark_int_time[1]);
		DBG((dbgo,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[1] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[1], nummeas, 0))
		if ((ev = munki_dark_measure(p, s->idark_data[1], nummeas, &s->idark_int_time[1], 0))
		                                                                          != MUNKI_OK) {
			return ev;
		}
	
		if (s->scan)
			s->idark_int_time[2] = s->inttime;		/* Use nominal time for scan */
		else
			s->idark_int_time[2] = m->min_int_time;
		nummeas = munki_comp_nummeas(p, s->dcaltime, s->idark_int_time[2]);
		DBG((dbgo,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[2] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[2], nummeas, 1))
		if ((ev = munki_dark_measure(p, s->idark_data[2], nummeas, &s->idark_int_time[2], 1))
		                                                                          != MUNKI_OK) {
			return ev;
		}
	
		s->idark_int_time[3] = ADARKINT_MAX; /* 1.0 */
		DBG((dbgo,"Doing adaptive interpolated black calibration, dcaltime %f, idark_int_time[3] %f, nummeas %d, gainmode %d\n", s->dcaltime, s->idark_int_time[3], nummeas, 1))
		nummeas = munki_comp_nummeas(p, s->dcaltime, s->idark_int_time[3]);
		if ((ev = munki_dark_measure(p, s->idark_data[3], nummeas, &s->idark_int_time[3], 1))
		                                                                          != MUNKI_OK) {
			return ev;
		}
	
		munki_prepare_idark(p);

		s->idark_valid = 1;
		s->iddate = time(NULL);
	
		if ((ev = munki_interp_dark(p, s->dark_data, s->inttime, s->gainmode)) != MUNKI_OK) {
			return ev;
		}
		s->dark_valid = 1;
		s->need_dcalib = 0;
		s->ddate = s->iddate;
		s->dark_int_time = s->inttime;
		s->dark_gain_mode = s->gainmode;

		/* Save the calib to all similar modes */
		for (i = 0; i < mk_no_modes; i++) {
			munki_state *ss = &m->ms[i];
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
#ifdef TEST_DARK_INTERP
#ifdef DEBUG
		{
			double tinttime;
			double ref[NRAW], interp[NRAW];
			
			fprintf(stderr,"Normal gain offsets, base:\n");
			plot_raw(s->idark_data[0]);
			
			fprintf(stderr,"Normal gain offsets, multiplier:\n");
			plot_raw(s->idark_data[1]);
			
			for (tinttime = m->min_int_time; tinttime < 4.0; tinttime *= 2.0) {

				nummeas = munki_comp_nummeas(p, s->dcaltime, tinttime);
				if ((ev = munki_dark_measure(p, ref, nummeas, &tinttime, 0)) != MUNKI_OK) {
					return ev;
				}
				munki_interp_dark(p, interp, tinttime, 0);
				fprintf(stderr,"Normal gain, int time %f:\n",tinttime);
				plot_raw2(ref, interp);
			}

			fprintf(stderr,"High gain offsets, base:\n");
			plot_raw(s->idark_data[2]);
			
			fprintf(stderr,"High gain offsets, multiplier:\n");
			plot_raw(s->idark_data[3]);
			
			for (tinttime = m->min_int_time; tinttime < 4.0; tinttime *= 2.0) {

				nummeas = munki_comp_nummeas(p, s->dcaltime, tinttime);
				if ((ev = munki_dark_measure(p, ref, nummeas, &tinttime, 1)) != MUNKI_OK) {
					return ev;
				}
				munki_interp_dark(p, interp, tinttime, 1);
				fprintf(stderr,"High gain, int time %f:\n",tinttime);
				plot_raw2(ref, interp);
			}
		}
#endif	/* DEBUG */
#endif	/* TEST_DARK_INTERP */

	}

	/* Emiss non-adaptive (display/projector) needs three pre-set black */
	/* references, so it has fallback intergration times. */
	if (s->emiss && !s->adaptive && !s->scan && *calc == inst_calc_man_cal_smode) {
		int stm;

		nummeas = munki_comp_nummeas(p, s->dcaltime, s->inttime);

		DBG((dbgo,"Doing initial display black calibration with dcaltime %f, int_time %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode))
		stm = msec_time();
		if ((ev = munki_dark_measure(p, s->dark_data, nummeas, &s->inttime, s->gainmode))
	                                                                         != MUNKI_OK) {
			return ev;
		}
		if (p->debug) fprintf(stderr,"Execution time of dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);

		nummeas = munki_comp_nummeas(p, s->dcaltime2, s->dark_int_time2);
		DBG((dbgo,"Doing 2nd initial black calibration with dcaltime2 %f, dark_int_time2 %f, nummeas %d, gainmode %d\n", s->dcaltime2, s->dark_int_time2, nummeas, s->gainmode))
		stm = msec_time();
		if ((ev = munki_dark_measure(p, s->dark_data2, nummeas, &s->dark_int_time2,
		                                                   s->gainmode)) != MUNKI_OK) {
			return ev;
		}
		if (p->debug) fprintf(stderr,"Execution time of 2nd dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);

		nummeas = munki_comp_nummeas(p, s->dcaltime3, s->dark_int_time3);
		DBG((dbgo,"Doing 3rd initial black calibration with dcaltime3 %f, dark_int_time3 %f, nummeas %d, gainmode %d\n", s->dcaltime3, s->dark_int_time3, nummeas, s->gainmode))
		nummeas = munki_comp_nummeas(p, s->dcaltime3, s->dark_int_time3);
		stm = msec_time();
		if ((ev = munki_dark_measure(p, s->dark_data3, nummeas, &s->dark_int_time3,
			                                                   s->gainmode)) != MUNKI_OK) {
			return ev;
		}
		if (p->debug) fprintf(stderr,"Execution time of 3rd dark calib time %f sec = %d msec\n",s->inttime,msec_time() - stm);

		s->dark_valid = 1;
		s->need_dcalib = 0;
		s->ddate = time(NULL);
		s->dark_int_time = s->inttime;
		s->dark_gain_mode = s->gainmode;

		/* Save the calib to all similar modes */
		for (i = 0; i < mk_no_modes; i++) {
			munki_state *ss = &m->ms[i];
			if (ss == s)
				continue;
			if (ss->emiss && !ss->adaptive && !ss->scan) {
				ss->dark_valid = s->dark_valid;
				ss->need_dcalib = s->need_dcalib;
				ss->ddate = s->ddate;
				ss->dark_int_time = s->dark_int_time;
				ss->dark_gain_mode = s->dark_gain_mode;
				for (k = 0; k < m->nraw; k++) {
					ss->dark_data[k] = s->dark_data[k];
					ss->dark_data2[k] = s->dark_data2[k];
					ss->dark_data3[k] = s->dark_data3[k];
				}
			}
		}
	}

	/* Now deal with white calibrations */

	/* If we are doing a reflective white reference calibrate */
	/* or a we are doing a tranmisive white reference calibrate */
	if ((s->reflective && *calc == inst_calc_man_cal_smode)
	 || (s->trans && *calc == inst_calc_man_trans_white)) {
		double dead_time = 0.0;		/* Dead integration time */
		double scale;
		int i;
		double ulimit = m->optsval / m->minsval;	/* Upper scale needed limit */
		double fulimit = sqrt(ulimit);				/* Fast exit limit */
		double llimit = m->optsval / m->maxsval;	/* Lower scale needed limit */
		double fllimit = sqrt(llimit);				/* Fast exit limit */

		DBG((dbgo,"Doing initial reflective white calibration with current inttime %f, gainmode %d\n",
		                                                   s->inttime, s->gainmode))
		DBG((dbgo,"ulimit %f, llimit %f\n",ulimit,llimit))
		DBG((dbgo,"fulimit %f, fllimit %f\n",fulimit,fllimit))
		if (s->reflective) {
			dead_time = RDEAD_TIME;		/* Fudge value that makes int time calcs work */
			/* Heat up the LED to put in in a nominal state for int time adjustment */
			munki_heatLED(p, m->ledpreheattime);
		}
		
		/* Until we're done */
		for (i = 0; i < 6; i++) {

			RDBG((dbgo,"Doing a white calibration with trial int_time %f, gainmode %d\n",
			                                                 s->inttime,s->gainmode))

			if (s->trans) {
				/* compute interpolated dark refence for chosen inttime & gainmode */
				RDBG((dbgo,"Interpolate dark calibration reference\n"))
				if ((ev = munki_interp_dark(p, s->dark_data, s->inttime, s->gainmode))
				                                                             != MUNKI_OK) {
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
			RDBG((dbgo,"Needed scale is %f\n",scale))

			if (ev == MUNKI_RD_SENSORSATURATED) {
				scale = 0.0;			/* Signal it this way */
				ev = MUNKI_OK;
			}
			if (ev != MUNKI_OK) {
				return ev;
			}

			if (scale >= fllimit && scale <= fulimit) {
				RDBG((dbgo,"Close enough for early exit\n"))
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
					return ev;
				}
				s->inttime = ninttime;
				RDBG((dbgo,"New inttime = %f\n",s->inttime))
			}
		}
		if (i >= 6) {
			if (scale == 0.0) {		/* If sensor was saturated */
				if (p->debug)
					fprintf(stderr,"White calibration faild - sensor is saturated\n");
				return MUNKI_RD_SENSORSATURATED;
			}
			if (scale > ulimit || scale < llimit) {
				if (p->debug)
					fprintf(stderr,"White calibration failed - didn't converge (%f %f %f)\n",llimit,scale,ulimit);
				return MUNKI_RD_REFWHITENOCONV; 
			}
		}

		/* We've settled on the inttime and gain mode to get a good white reference. */
		if (s->reflective) {	/* We read the write reference - check it */

			/* Let the LED cool down */
			DBG((dbgo,"Waiting %f secs for LED to cool\n",m->ledwaittime))
			msec_sleep((int)(m->ledwaittime * 1000.0 + 0.5));

			/* Re-calibrate the black with the given integration time */
			nummeas = munki_comp_nummeas(p, s->dcaltime, s->inttime);

			DBG((dbgo,"Doing another reflective black calibration with dcaltime %f, int_time %f, nummeas %d, gainmode %d\n", s->dcaltime, s->inttime, nummeas, s->gainmode))
			if ((ev = munki_dark_measure(p, s->dark_data, nummeas, &s->inttime, s->gainmode))
		                                                                         != MUNKI_OK) {
				return ev;
			}

			/* Take a reflective white reference measurement, */
			/* subtracts black and decompose into base + LED temperature components, */
			/* and compute reftemp white reference. */
			nummeas = munki_comp_nummeas(p, m->calscantime, s->inttime);
			if ((ev = munki_ledtemp_whitemeasure(p, s->white_data, s->iwhite_data, &s->reftemp,
			                                  nummeas, s->inttime, s->gainmode)) != MUNKI_OK) {
				return ev;
			}

			/* Compute wavelength white readings from ref temp sensor reading */
			if ((ev = munki_compute_wav_whitemeas(p, s->cal_factor1, s->cal_factor2,
			                                             s->white_data)) != MUNKI_OK) {
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
				return ev;
			}

			/* Compute a calibration factor given the reading of the white reference. */
			transwarn |= munki_compute_white_cal(p, s->cal_factor1, NULL, s->cal_factor1,
			                                       s->cal_factor2, NULL, s->cal_factor2);
		}
		s->cal_valid = 1;
		s->cfdate = time(NULL);
		s->need_calib = 0;
	}

#ifdef ENABLE_NONVCAL
	/* Save the calibration to a file */
	munki_save_calibration(p);
#endif

	/* Deal with a display integration time calibration */
	if (s->emiss && !s->adaptive && !s->scan
	 && !s->done_dintcal && *calc == inst_calc_disp_white) {
		double scale;
		double *data;

		data = dvectorz(0, m->nraw-1);

		DBG((dbgo,"Doing display integration time calibration\n"))

		/* Simply measure the full display white, and if it's close to */
		/* saturation, switch to the alternate display integration time */
		nummeas = munki_comp_nummeas(p, s->wreadtime, s->inttime);
		ev = munki_whitemeasure(p, data , &scale, nummeas,
		           &s->inttime, s->gainmode, s->targoscale);
		/* Switch to the alternate if things are too bright */
		/* We do this simply by swapping the alternate values in. */
		if (ev == MUNKI_RD_SENSORSATURATED || scale < 1.0) {
			double *tt, tv;
			DBG((dbgo,"Switching to alternate display integration time %f seconds\n",s->dark_int_time2))
			if (p->debug)
				fprintf(stderr,"Switching to alternate display integration time %f seconds\n",s->dark_int_time2);
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
		if (ev != MUNKI_OK) {
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
			return MUNKI_CAL_SETUP;
		}
	}

	if (transwarn) {
		*calc = inst_calc_message;
		if (transwarn & 2)
			strcpy(id, "Warning: Transmission light source is too low for accuracy!");
		else
			strcpy(id, "Warning: Transmission light source is low at some wavelengths!");
		return MUNKI_OK;
	}

	DBG((dbgo,"Finished cal with dark_valid = %d, cal_valid = %d\n",s->dark_valid, s->cal_valid))

	return ev; 
}

/* Interpret an icoms error into a MUNKI error */
static int icoms2munki_err(int se) {
	if (se & ICOM_USERM) {
		se &= ICOM_USERM;
		if (se == ICOM_USER)
			return MUNKI_USER_ABORT;
		if (se == ICOM_TERM)
			return MUNKI_USER_TERM;
		if (se == ICOM_TRIG)
			return MUNKI_USER_TRIG;
		if (se == ICOM_CMND)
			return MUNKI_USER_CMND;
	}
	if (se != ICOM_OK)
		return MUNKI_COMS_FAIL;
	return MUNKI_OK;
}


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
	int nvals			/* Number of values */	
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

	DBG((dbgo,"munki_imp_measure called\n"))
	if (p->debug)
		fprintf(stderr,"Taking %d measurments in %s%s%s%s%s mode called\n", nvals,
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
		DBG((dbgo,"munki_imp_measure need calibration\n"))
		return MUNKI_RD_NEEDS_CAL;
	}
		
	if (nvals <= 0
	 || (!s->scan && nvals > 1)) {
		DBG((dbgo,"munki_imp_measure wrong number of patches\n"))
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
		bsize = NRAWB * nummeas;
		if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
			DBG((dbgo,"munki_imp_measure malloc %d bytes failed (5)\n",bsize))
			if (p->verb) printf("Malloc %d bytes failed (5)\n",bsize);
			return MUNKI_INT_MALLOC;
		}
	}

	/* Allocate buffer for measurement */
	maxnummeas = munki_comp_nummeas(p, s->maxscantime, s->inttime);
	if (maxnummeas < (ninvmeas + nummeas))
		maxnummeas = (ninvmeas + nummeas);
	mbsize = NRAWB * maxnummeas;
	if ((mbuf = (unsigned char *)malloc(sizeof(unsigned char) * mbsize)) == NULL) {
		if (buf != NULL)
			free(buf);
		DBG((dbgo,"munki_imp_measure malloc %d bytes failed (6)\n",mbsize))
		if (p->verb) printf("Malloc %d bytes failed (6)\n",mbsize);
		return MUNKI_INT_MALLOC;
	}
	specrd = dmatrix(0, nvals-1, 0, m->nwav-1);

	if (m->trig == inst_opt_trig_keyb_switch) {

#ifdef USE_THREAD
		int currcount = m->switch_count;
		while (currcount == m->switch_count) {
			int cerr;

			if ((cerr = icoms_poll_user(p->icom, 0)) != ICOM_OK) {
				ev = icoms2munki_err(cerr);
				/* Don't trigger on user key if scan, only trigger */
				/* on instrument switch */
				if (!s->scan || ev != MUNKI_USER_TRIG) {
					user_trig = 1;
					break;
				}
			}
			msec_sleep(100);
		}
#else
		/* Throw one away in case the switch was pressed prematurely */
		munki_waitfor_switch_th(p, NULL, NULL, 0.01);

		for (ev = MUNKI_INT_BUTTONTIMEOUT; ev == MUNKI_INT_BUTTONTIMEOUT;) {
			mk_eve ecode;
			int cerr;
			ev = munki_waitfor_switch_th(p, &ecode, NULL, 0.1);
			if (ecode != mk_eve_button_press)	/* Ignore anything else */
				ev = MUNKI_INT_BUTTONTIMEOUT;
			if ((cerr = icoms_poll_user(p->icom, 0)) != ICOM_OK) {
				ev = icoms2munki_err(cerr);
				/* Don't trigger on user key if scan, only trigger */
				/* on instrument switch */
				if (!s->scan || ev != MUNKI_USER_TRIG) {
					user_trig = 1;
					break;
				}
				ev = MUNKI_INT_BUTTONTIMEOUT;	/* Ignore */
			}
		}
#endif
		if (ev == MUNKI_OK || ev == MUNKI_USER_TRIG) {
			DBG((dbgo,"############# triggered ##############\n"))
			if (m->trig_return)
				printf("\n");
		}

	} else if (m->trig == inst_opt_trig_keyb) {
		ev = icoms2munki_err(icoms_poll_user(p->icom, 1));

		if (ev == MUNKI_USER_TRIG) {
			DBG((dbgo,"############# triggered ##############\n"))
			user_trig = 1;
			if (m->trig_return)
				printf("\n");
		}
	}

	if (ev != MUNKI_OK && ev != MUNKI_USER_TRIG) {
		free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
		free(mbuf);
		if (buf != NULL)
			free(buf);
		DBG((dbgo,"munki_imp_measure user aborted, terminated, command, or failure\n"))
		return ev;		/* User abort, term, command or failure */
	}

	/* Get current sensor position */
	if ((ev = munki_getstatus(p, &spos, NULL)) != MUNKI_OK) {
		free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
		free(mbuf);
		if (buf != NULL)
			free(buf);
		DBG((dbgo,"munki_imp_measure getstatus failed\n"))
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
			DBG((dbgo,"munki_imp_measure: Sensor in wrong position\n"))
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

		DBG((dbgo,"Trial measure emission with inttime %f, gainmode %d\n",s->inttime,s->gainmode))

		/* Take a trial measurement reading using the current mode. */
		/* Used to determine if sensor is saturated, or not optimal */
		nummeas = munki_comp_nummeas(p, s->wreadtime, s->inttime);
		if ((ev = munki_trialmeasure(p, &saturated, &optscale, nummeas, &s->inttime, s->gainmode,
		                                                            s->targoscale)) != MUNKI_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			DBG((dbgo,"munki_imp_measure trial measure failed\n"))
			return ev;  
		}

		if (saturated) {
			s->inttime = m->min_int_time;

			DBG((dbgo,"2nd trial measure emission with inttime %f, gainmode %d\n",
			                                         s->inttime,s->gainmode))
			/* Take a trial measurement reading using the current mode. */
			/* Used to determine if sensor is saturated, or not optimal */
			nummeas = munki_comp_nummeas(p, s->wreadtime, s->inttime);
			if ((ev = munki_trialmeasure(p, &saturated, &optscale, nummeas, &s->inttime, 
			                                          s->gainmode, s->targoscale)) != MUNKI_OK) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
				free(mbuf);
				DBG((dbgo,"munki_imp_measure trial measure failed\n"))
				return ev;  
			}
		}

		DBG((dbgo,"Compute optimal integration time\n"))
		/* For adaptive mode, compute a new integration time and gain mode */
		/* in order to optimise the sensor values. */
		if ((ev = munki_optimise_sensor(p, &s->inttime, &s->gainmode, 
		         s->inttime, s->gainmode, 1, 1, &s->targoscale, optscale, 0.0)) != MUNKI_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			DBG((dbgo,"munki_imp_measure optimise sensor failed\n"))
			return ev;
		}
		DBG((dbgo,"Computed optimal emiss inttime %f and gainmode %d\n",s->inttime,s->gainmode))

		DBG((dbgo,"Interpolate dark calibration reference\n"))
		if ((ev = munki_interp_dark(p, s->dark_data, s->inttime, s->gainmode)) != MUNKI_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			DBG((dbgo,"munki_imp_measure interplate dark ref failed\n"))
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
		mbsize = NRAWB * maxnummeas;
		if ((mbuf = (unsigned char *)malloc(sizeof(unsigned char) * mbsize)) == NULL) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			DBG((dbgo,"munki_imp_measure malloc %d bytes failed (7)\n",mbsize))
			if (p->verb) printf("Malloc %d bytes failed (7)\n",mbsize);
			return MUNKI_INT_MALLOC;
		}

	} else if (s->reflective) {

		DISDPLOT

		DBG((dbgo,"Doing on the fly black calibration_1 with nummeas %d int_time %f, gainmode %d\n",
		                                                   nummeas, s->inttime, s->gainmode))

		if ((ev = munki_dark_measure_1(p, nummeas, &s->inttime, s->gainmode, buf, bsize))
		                                                                           != MUNKI_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(buf);
			free(mbuf);
			DBG((dbgo,"munki_imp_measure dak measure 1 failed\n"))
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
			DBG((dbgo,"munki_imp_measure failed at munki_read_patches_1\n"))
			return ev;
		}

		/* Complete processing of dark readings now that main measurement has been taken */
		if (s->reflective) {
			DBG((dbgo,"Calling black calibration_2 calc with nummeas %d, inttime %f, gainmode %d\n", nummeas, s->inttime,s->gainmode))
			if ((ev = munki_dark_measure_2(p, s->dark_data, nummeas, s->inttime,
			                                  s->gainmode, buf, bsize)) != MUNKI_OK) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
				free(buf);
				free(mbuf);
				DBG((dbgo,"munki_imp_measure failed at munki_dark_measure_2\n"))
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
			nummeas = munki_comp_nummeas(p, s->wreadtime, s->inttime);
			maxnummeas = munki_comp_nummeas(p, s->maxscantime, s->inttime);
			if (maxnummeas < nummeas)
				maxnummeas = nummeas;
			mbsize = NRAWB * maxnummeas;
			if ((mbuf = (unsigned char *)malloc(sizeof(unsigned char) * mbsize)) == NULL) {
				free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
				DBG((dbgo,"munki_imp_measure malloc %d bytes failed (7)\n",mbsize))
				if (p->verb) printf("Malloc %d bytes failed (7)\n",mbsize);
				return MUNKI_INT_MALLOC;
			}
			continue;			/* Do the measurement again */
		}

		if (ev != MUNKI_OK) {
			free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
			free(mbuf);
			DBG((dbgo,"munki_imp_measure failed at munki_read_patches_2\n"))
			return ev;
		}
		break;		/* Don't repeat */
	}
	free(mbuf);

	/* Transfer spectral and convert to XYZ */
	if ((ev = munki_conv2XYZ(p, vals, nvals, specrd)) != MUNKI_OK) {
		free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);
		DBG((dbgo,"munki_imp_measure failed at munki_conv2XYZ\n"))
		return ev;
	}
	free_dmatrix(specrd, 0, nvals-1, 0, m->nwav-1);

	if (nvals > 0)
		vals[0].duration = duration;	/* Possible flash duration */
	
	DBG((dbgo,"munki_imp_measure sucessful return\n"))
	if (user_trig)
		return MUNKI_USER_TRIG;
	return ev; 
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Save the calibration for all modes, stored on local system */

#ifdef ENABLE_NONVCAL

/* non-volatile save/restor state to/from a file */
typedef struct {
	int ef;					/* Error flag */
	unsigned int chsum;		/* Checksum */
} i1pnonv;

static void update_chsum(i1pnonv *x, unsigned char *p, int nn) {
	int i;
	for (i = 0; i < nn; i++, p++)
		x->chsum = ((x->chsum << 13) | (x->chsum >> (32-13))) + *p;
}

/* Write an array of chars to the file. Set the error flag to nz on error */
static void write_chars(i1pnonv *x, FILE *fp, char *dp, int n) {

	if (fwrite((void *)dp, sizeof(char), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(char));
	}
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

/* Read an array of chars from the file. Set the error flag to nz on error */
static void read_chars(i1pnonv *x, FILE *fp, char *dp, int n) {

	if (fread((void *)dp, sizeof(char), n, fp) != n) {
		x->ef = 1;
	} else {
		update_chsum(x, (unsigned char *)dp, n * sizeof(char));
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

munki_code munki_save_calibration(munki *p) {
	munkiimp *m = (munkiimp *)p->m;
	munki_code ev = MUNKI_OK;
	munki_state *s;
	int i;
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

	sprintf(cal_name, "color/.mk_%s.cal", m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_write, xdg_user, cal_name)) < 1)
		return MUNKI_INT_CAL_SAVE;

	if (p->debug >= 1)
		fprintf(stderr,"munki_save_calibration saving to file '%s'\n",cal_paths[0]);
	DBG((dbgo,"munki_save_calibration saving to file '%s'\n",cal_paths[0]));

	if (create_parent_directories(cal_paths[0])
	 || (fp = fopen(cal_paths[0], nmode)) == NULL) {
		DBG((dbgo,"munki_save_calibration failed to open file for writing\n"));
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
			write_doubles(&x, fp, &s->reftemp, 1);
			write_doubles(&x, fp, s->iwhite_data[0], m->nraw);
			write_doubles(&x, fp, s->iwhite_data[1], m->nraw);
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
		if (p->debug >= 1)
			fprintf(stderr,"Writing calibration file failed\n");
		DBG((dbgo,"Writing calibration file failed\n"))
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
munki_code munki_restore_calibration(munki *p) {
	munkiimp *m = (munkiimp *)p->m;
	munki_code ev = MUNKI_OK;
	munki_state *s, ts;
	int i, j;
	char nmode[10];
	char cal_name[40+1];		/* Name */
	char **cal_paths = NULL;
	int no_paths = 0;
	FILE *fp;
	i1pnonv x;
	int argyllversion;
	int ss, nraw, nwav1, nwav2, chsum1, chsum2;
	char serno[17];

	strcpy(nmode, "r");
#if defined(O_BINARY) || defined(_O_BINARY)
	strcat(nmode, "b");
#endif

	sprintf(cal_name, "color/.mk_%s.cal", m->serno);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_read, xdg_user, cal_name)) < 1)
		return MUNKI_INT_CAL_RESTORE;

	if (p->debug >= 1)
		fprintf(stderr,"munki_restore_calibration restoring from file '%s'\n",cal_paths[0]);
	DBG((dbgo,"munki_restore_calibration restoring from file '%s'\n",cal_paths[0]));

	if ((fp = fopen(cal_paths[0], nmode)) == NULL) {
		DBG((dbgo,"munki_restore_calibration failed to open file for reading\n"));
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
		DBG((dbgo,"Identification didn't verify\n"));
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
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* dark_data */
		read_doubles(&x, fp, &dd, 1);				/* dark_int_time2 */
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* dark_data2 */
		read_doubles(&x, fp, &dd, 1);				/* dark_int_time3 */
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* dark_data3 */
		read_ints(&x, fp, &di, 1);					/* dark_gain_mode */

		if (!s->emiss) {
			read_ints(&x, fp, &di, 1);				/* cal_valid */
			read_time_ts(&x, fp, &dt, 1);			/* cfdate */
			for (j = 0; j < m->nwav1; j++)
				read_doubles(&x, fp, &dd, 1);		/* cal_factor1 */
			for (j = 0; j < m->nwav2; j++)
				read_doubles(&x, fp, &dd, 1);		/* cal_factor2 */
			for (j = 0; j < m->nraw; j++)
				read_doubles(&x, fp, &dd, 1);		/* white_data */
			read_doubles(&x, fp, &dd, 1);			/* reftemp */
			for (j = 0; j < m->nraw; j++)
				read_doubles(&x, fp, &dd, 1);		/* iwhite_data[0] */
			for (j = 0; j < m->nraw; j++)
				read_doubles(&x, fp, &dd, 1);		/* iwhite_data[1] */
		}
		
		read_ints(&x, fp, &di, 1);					/* idark_valid */
		read_time_ts(&x, fp, &dt, 1);				/* iddate */
		for (j = 0; j < 4; j++)
			read_doubles(&x, fp, &dd, 1);			/* idark_int_time */
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* idark_data[0] */
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* idark_data[1] */
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* idark_data[2] */
		for (j = 0; j < m->nraw; j++)
			read_doubles(&x, fp, &dd, 1);			/* idark_data[3] */
	}

	chsum1 = x.chsum;
	read_ints(&x, fp, &chsum2, 1);
	
	if (x.ef != 0
	 || chsum1 != chsum2) {
		if (p->debug >= 1)
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
	ts.iwhite_data = dmatrixz(0, 2, 0, m->nraw-1);  
	ts.idark_data = dmatrixz(0, 3, 0, m->nraw-1);  

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
			read_doubles(&x, fp, &ts.reftemp, 1);
			read_doubles(&x, fp, ts.iwhite_data[0], m->nraw);
			read_doubles(&x, fp, ts.iwhite_data[1], m->nraw);
		}
		
		/* Adaptive Dark */
		read_ints(&x, fp, &ts.idark_valid, 1);
		read_time_ts(&x, fp, &ts.iddate, 1);
//printf("~1 mode %d, read iddate = %u\n",i, ts.iddate);
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
				s->reftemp = ts.reftemp;
				for (j = 0; j < m->nraw; j++)
					s->iwhite_data[0][j] = ts.iwhite_data[0][j];
				for (j = 0; j < m->nraw; j++)
					s->iwhite_data[1][j] = ts.iwhite_data[1][j];
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
			DBG((dbgo,"Not restoring cal for mode %d since params don't match:\n",i));
			DBG((dbgo,"emis = %d : %d, trans = %d : %d, ref = %d : %d\n",s->emiss,ts.emiss,s->trans,ts.trans,s->reflective,ts.reflective));
			DBG((dbgo,"scan = %d : %d, flash = %d : %d, ambi = %d : %d, proj = %d : %d, adapt = %d : %d\n",s->scan,ts.scan,s->flash,ts.flash,s->ambient,ts.ambient,s->projector,ts.projector,s->adaptive,ts.adaptive));
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
	free_dmatrix(ts.iwhite_data, 0, 1, 0, m->nraw-1);  
	free_dmatrix(ts.idark_data, 0, 3, 0, m->nraw-1);  

	free_dvector(ts.cal_factor1, 0, m->nwav1-1);
	free_dvector(ts.cal_factor2, 0, m->nwav2-1);

	DBG((dbgo,"munki_restore_calibration done\n"))
 reserr:;

	fclose(fp);

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
	double *sens,		/* Return array [nraw] of sens values */
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

	multimes = dmatrix(0, nummeas-1, 0, m->nraw-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* floating point sensor readings. Check for saturation */
	if (munki_meas_to_sens(p, multimes, NULL, buf, 0, nummeas, m->satlimit, &darkthresh)) {
		free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
		return MUNKI_RD_SENSORSATURATED;
	}

	/* Average a set of measurements into one. */
	/* Return nz if the readings are not consistent */
	/* Return the overall average. */
	rv = munki_average_multimeas(p, sens, multimes, nummeas, &sensavg, darkthresh);     
	free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);

#ifdef PLOT_DEBUG
	printf("Average absolute sensor readings, average = %f, darkthresh %f\n",sensavg,darkthresh);
	plot_raw(sens);
#endif

	if (rv) {
		DBG((dbgo,"munki_dark_measure_2: readings are inconsistent\n"))
		return MUNKI_RD_DARKREADINCONS;
	}

	if (sensavg > (2.0 * darkthresh)) {
		DBG((dbgo,"munki_dark_measure_2: Average %f is > 2 * darkthresh %f\n",sensavg,darkthresh))
		return MUNKI_RD_DARKNOTVALID;
	}
	return ev;
}

/* Take a dark reference measurement (combined parts 1 & 2) */
munki_code munki_dark_measure(
	munki *p,
	double *sens,		/* Return array [nraw] of sens values */
	int nummeas,			/* Number of readings to take */
	double *inttime, 		/* Integration time to use/used */
	int gainmode			/* Gain mode to use, 0 = normal, 1 = high */
) {
	munki_code ev = MUNKI_OK;
	unsigned char *buf;		/* Raw USB reading buffer */
	unsigned int bsize;

	DBG((dbgo, "munki_dark_measure with inttime %f\n",*inttime))
	bsize = NRAWB * nummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		DBG((dbgo,"munki_dark_measure malloc %d bytes failed (8)\n",bsize))
		if (p->verb) printf("Malloc %d bytes failed (8)\n",bsize);
		return MUNKI_INT_MALLOC;
	}

	if ((ev = munki_dark_measure_1(p, nummeas, inttime, gainmode, buf, bsize)) != MUNKI_OK) {
		free(buf);
		return ev;
	}

	if ((ev = munki_dark_measure_2(p, sens, nummeas, *inttime, gainmode, buf, bsize))
	                                                                           != MUNKI_OK) {
		free(buf);
		return ev;
	}
	free(buf);

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

	DBG((dbgo,"munki_heatLED called \n"))

	nummeas = munki_comp_ru_nummeas(p, htime, inttime);

	if (nummeas <= 0)
		return MUNKI_OK;
		
	/* Allocate temporaries */
	bsize = NRAWB * nummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		DBG((dbgo,"munki_heatLED malloc %d bytes failed (10)\n",bsize))
		if (p->verb) printf("Malloc %d bytes failed (10)\n",bsize);
		return MUNKI_INT_MALLOC;
	}

	DBG((dbgo,"Triggering measurement cycle, nummeas %d, inttime %f\n", nummeas, inttime))

	if ((rv = munki_trigger_one_measure(p, nummeas, &inttime, 0, 1, 0))
	                                                                        != MUNKI_OK) {
		free(buf);
		return rv;
	}

	DBG((dbgo,"Gathering readings\n"))

	rv = munki_readmeasurement(p, nummeas, 0, buf, bsize, NULL, 1, 0);

	free(buf);

	return rv;
}


/* Take a reflective or emissive white reference sens measurement, */
/* subtracts black and processes into wavelenths. */
/* (absraw is usually ->white_data) */
munki_code munki_whitemeasure(
	munki *p,
	double *absraw,			/* Return array [nraw] of absraw values (may be NULL) */
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

	DBG((dbgo,"munki_whitemeasure called \n"))

	if (s->reflective) {
		/* Compute invalid samples to allow for LED warmup */
		ninvmeas = munki_comp_ru_nummeas(p, m->refinvalidsampt, *inttime);
	}

	if (nummeas <= 0)
		return MUNKI_INT_ZEROMEASURES;
		
	/* Allocate temporaries */
	bsize = NRAWB * (ninvmeas + nummeas);
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		DBG((dbgo,"munki_whitemeasure malloc %d bytes failed (10)\n",bsize))
		if (p->verb) printf("Malloc %d bytes failed (10)\n",bsize);
		return MUNKI_INT_MALLOC;
	}

	DBG((dbgo,"Triggering measurement cycle, ninvmeas %d, nummeas %d, inttime %f, gainmode %d\n",
	                                              ninvmeas, nummeas, *inttime, gainmode))

	if ((ev = munki_trigger_one_measure(p, ninvmeas + nummeas, inttime, gainmode, 1, 0))
	                                                                        != MUNKI_OK) {
		free(buf);
		return ev;
	}

	DBG((dbgo,"Gathering readings\n"))

	if ((ev = munki_readmeasurement(p, ninvmeas + nummeas, 0, buf, bsize, NULL, 1, 0))
	                                                                      != MUNKI_OK) {
		free(buf);
		return ev;
	}

  	multimes = dmatrix(0, nummeas-1, 0, m->nraw-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* floating point sensor readings. Check for saturation */
	if (munki_meas_to_sens(p, multimes, NULL, buf, ninvmeas, nummeas, m->satlimit, &darkthresh)) {
		free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
		return MUNKI_RD_SENSORSATURATED;
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
	munki_sub_sens_to_abssens(p, nummeas, *inttime, gainmode, multimes, s->dark_data,
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
			free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
			return MUNKI_RD_WHITEREADINCONS;
		}
#endif /* IGNORE_WHITE_INCONS */

		DBG((dbgo,"Average absolute sensor readings, avg %f, max %f, darkth %f satth %f\n",sensavg,darkthresh,trackmax[2]))
#ifdef PLOT_DEBUG
		plot_raw(absraw);
#endif
	}

	if (optscale != NULL) {
		double opttarget;       /* Optimal reading scale target value */

		opttarget = targoscale * trackmax[1];
		if (maxval < 0.01)		/* Could go -ve */
			maxval = 0.01;
		*optscale = opttarget/maxval; 

		DBG((dbgo,"Targscale %f, maxval %f, optimal target = %f, amount to scale = %f\n",
		     targoscale, maxval, opttarget, *optscale))
	}

	free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);		/* Free after using *pmax */

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
	double *absraw			/* Given array [nraw] of absraw values */
) {
#if defined(PLOT_DEBUG) || defined(HIGH_RES)
	munkiimp *m = (munkiimp *)p->m;
#endif
	/* Convert an absraw array from raw wavelengths to output wavelenths */
	if (abswav1 != NULL) {
		munki_abssens_to_abswav1(p, 1, &abswav1, &absraw);

#ifdef PLOT_DEBUG
		printf("Converted to wavelengths std res:\n");
		plot_wav1(m, abswav1);
#endif
	}

#ifdef HIGH_RES
	if (abswav2 != NULL && m->hr_inited == 2) {
		munki_abssens_to_abswav2(p, 1, &abswav2, &absraw);

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
	double *white,			/* Return [nraw] of temperature compensated white reference */
	double **iwhite,		/* Return array [nraw][2] of absraw base and scale values */
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

	DBG((dbgo,"munki_ledtemp_whitemeasure called \n"))

	/* Compute invalid samples to allow for LED warmup */
	ninvmeas = munki_comp_ru_nummeas(p, m->refinvalidsampt, inttime);

	if (nummeas <= 0)
		return MUNKI_INT_ZEROMEASURES;
		
	/* Allocate temporaries */
	bsize = NRAWB * (ninvmeas + nummeas);
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		DBG((dbgo,"munki_whitemeasure malloc %d bytes failed (10)\n",bsize))
		if (p->verb) printf("Malloc %d bytes failed (10)\n",bsize);
		return MUNKI_INT_MALLOC;
	}

	DBG((dbgo,"Triggering measurement cycle, ninvmeas %d, nummeas %d, inttime %f, gainmode %d\n",
	                                              ninvmeas, nummeas, inttime, gainmode))

	if ((ev = munki_trigger_one_measure(p, ninvmeas + nummeas, &inttime, gainmode, 1, 0))
	                                                                        != MUNKI_OK) {
		free(buf);
		return ev;
	}

	DBG((dbgo,"Gathering readings\n"))

	if ((ev = munki_readmeasurement(p, ninvmeas + nummeas, 0, buf, bsize, NULL, 1, 0))
	                                                                      != MUNKI_OK) {
		free(buf);
		return ev;
	}

  	multimes = dmatrix(0, nummeas-1, 0, m->nraw-1);
  	ledtemp = dvector(0, nummeas-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* floating point sensor readings. Check for saturation */
	if (munki_meas_to_sens(p, multimes, ledtemp, buf, ninvmeas, nummeas, m->satlimit,
	                                                                    &darkthresh)) {
		free_dvector(ledtemp, 0, nummeas-1);
		free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
		return MUNKI_RD_SENSORSATURATED;
	}

	/* Make the reference temperature nominal */
	*reftemp = 0.5 * (ledtemp[0] + ledtemp[nummeas-1]);

#ifdef PLOT_DEBUG
	printf("Dark data:\n");
	plot_raw(s->dark_data);
#endif

	/* Subtract the black from sensor values and convert to */
	/* absolute (integration & gain scaled), zero offset based, */
	/* linearized sensor values. */
	munki_sub_sens_to_abssens(p, nummeas, inttime, gainmode, multimes, s->dark_data,
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

		for (w = 0; w < m->nraw; w++) {
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

		for (w = 0; w < m->nraw; w++) {
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

	DBG((dbgo,"Computed linear regression\n"))

#ifdef ENABLE_LEDTEMPC
	/* Compute a temerature compensated set of white readings */
	if ((ev = munki_ledtemp_comp(p, multimes, ledtemp, nummeas, *reftemp, iwhite)) != MUNKI_OK) {   
		free_dvector(ledtemp, 0, nummeas-1);
		free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
		return ev;
	}
#endif /* ENABLE_LEDTEMPC */

	/* Average a set of measurements into one. */
	if ((ev = munki_average_multimeas(p, white, multimes, nummeas, NULL, darkthresh)) != MUNKI_OK) {
		free_dvector(ledtemp, 0, nummeas-1);
		free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
		return ev;
	}

	free_dvector(ledtemp, 0, nummeas-1);
	free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);

	return ev;
}

/* Given the ledtemp base and scale values, */
/* return an absraw reflective white reference for the */
/* given temperature */ 
munki_code munki_ledtemp_white(
	munki *p,
	double *absraw,			/* Return array [nraw] of absraw base and scale values */
	double **iwhite,		/* ledtemp base and scale */
	double ledtemp			/* LED temperature value */
) {
	munkiimp *m = (munkiimp *)p->m;
	int w;

	for (w = 0; w < m->nraw; w++)
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
		for (w = 0; w < m->nraw; w++) {
			double targ, attemp;
			targ   = iwhite[0][w] + reftemp * iwhite[1][w];
			attemp = iwhite[0][w] + ledtemp[i] * iwhite[1][w];

			absraw[i][w] *= targ/attemp;
		}
	}
	return MUNKI_OK;
}

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
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;

	if ((ninvmeas + minnummeas) <= 0)
		return MUNKI_INT_ZEROMEASURES;

	if ((minnummeas + ninvmeas) > maxnummeas)
		maxnummeas = (minnummeas - ninvmeas);
		
	DBG((dbgo,"Triggering & gathering cycle, ninvmeas %d, minnummeas %d, inttime %f, gainmode %d\n",
	                                              ninvmeas, minnummeas, *inttime, gainmode))

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

/* Take a measurement reading using the current mode, part 2 */
/* Converts to completely processed output readings. */
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
	unsigned int bsize
) {
	munki_code ev = MUNKI_OK;
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	double **multimes;		/* Multiple measurement results [maxnummeas|nummeas][nraw]*/
	double **abssens;		/* Linearsised absolute sensor raw values [numpatches][nraw]*/
	double *ledtemp;		/* LED temperature values */
	double darkthresh;		/* Dark threshold (for consistency checking) */
	int rv = 0;

	if (duration != NULL)
		*duration = 0.0;	/* default value */

	/* Allocate temporaries */
	multimes = dmatrix(0, nummeas-1, 0, m->nraw-1);
  	ledtemp = dvector(0, nummeas-1);
	abssens = dmatrix(0, numpatches-1, 0, m->nraw-1);

	/* Take a buffer full of raw readings, and convert them to */
	/* floating point sensor readings. Check for saturation */
	if (munki_meas_to_sens(p, multimes, ledtemp, buf, ninvmeas, nummeas,
	                                         m->satlimit, &darkthresh)) {
		free_dvector(ledtemp, 0, nummeas-1);
		free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
		return MUNKI_RD_SENSORSATURATED;
	}

	/* Subtract the black from sensor values and convert to */
	/* absolute (integration & gain scaled), zero offset based, */
	/* linearized sensor values. */
	munki_sub_sens_to_abssens(p, nummeas, inttime, gainmode, multimes, s->dark_data,
	                             &darkthresh, 1, NULL);

#ifdef PLOT_TEMPCOMP
	/* Plot the raw spectra, 6 at a time */
	if (s->reflective) {
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
#ifdef DUMP_SCANV
	/* Dump raw scan readings to a file "mkdump.txt" */
	{
		int i, j;
		FILE *fp;
		
		if ((fp = fopen("mkdump.txt", "w")) == NULL)
			error("Unable to open debug file mkdump.txt");

		for (i = 0; i < nummeas; i++) {
			fprintf(fp, "%d	",i);
			for (j = 0; j < m->nraw; j++) {
				fprintf(fp, "%f	",multimes[i][j]);
			}
			fprintf(fp,"\n");
		}
		fclose(fp);
	}
#endif

#ifdef ENABLE_LEDTEMPC
	/* Do LED temperature compensation of absraw values */
	if (s->reflective) {
		if ((ev = munki_ledtemp_comp(p, multimes, ledtemp, nummeas, s->reftemp, s->iwhite_data))
		                                                                            != MUNKI_OK) {
			free_dvector(ledtemp, 0, nummeas-1);
			free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
			free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
			DBG((dbgo,"munki_read_patches_2 ledtemp comp failed\n"))
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
			free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
			free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
			DBG((dbgo,"munki_read_patches_2 spot read failed because numpatches != 1\n"))
			return MUNKI_INT_WRONGPATCHES;
		}

		/* Average a set of measurements into one. */
		/* Return zero if readings are consistent. */
		/* Return nz if the readings are not consistent */
		/* Return the overall average. */
		rv = munki_average_multimeas(p, abssens[0], multimes, nummeas, NULL, darkthresh);     
	} else {

		if (s->flash) {

			if (numpatches != 1) {
				free_dvector(ledtemp, 0, nummeas-1);
				free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
				free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
				DBG((dbgo,"munki_read_patches_2 spot read failed because numpatches != 1\n"))
				return MUNKI_INT_WRONGPATCHES;
			}
			if ((ev = munki_extract_patches_flash(p, &rv, duration, abssens[0], multimes,
			                                                 nummeas, inttime)) != MUNKI_OK) {
				free_dvector(ledtemp, 0, nummeas-1);
				free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
				free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
				DBG((dbgo,"munki_read_patches_2 spot read failed at munki_extract_patches_flash\n"))
				return ev;
			}

		} else {
			DBG((dbgo,"Number of patches to be measured = %d\n",nummeas))

			/* Recognise the required number of ref/trans patch locations, */
			/* and average the measurements within each patch. */
			/* Return zero if readings are consistent. */
			/* Return nz if the readings are not consistent */
			/* Return the highest individual element. */
			/* (Note white_data is just for normalization) */
			if ((ev = munki_extract_patches_multimeas(p, &rv, abssens, numpatches, multimes,
			                                                 nummeas, inttime)) != MUNKI_OK) {
				free_dvector(ledtemp, 0, nummeas-1);
				free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
				free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
				DBG((dbgo,"munki_read_patches_2 spot read failed at munki_extract_patches_multimeas\n"))
				return ev;
			}
		}
	}
	free_dvector(ledtemp, 0, nummeas-1);
	free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);

	if (rv) {
		free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);
		DBG((dbgo,"munki_read_patches_2 spot read failed with inconsistent readings\n"))
		return MUNKI_RD_READINCONS;
	}

	/* Convert an abssens array from raw wavelengths to output wavelenths */
	munki_abssens_to_abswav(p, numpatches, specrd, abssens);
	free_dmatrix(abssens, 0, numpatches-1, 0, m->nraw-1);

#ifdef APPEND_MEAN_EMMIS_VAL
	/* Appen averaged emission reading to file "mkdump.txt" */
	{
		int i, j;
		FILE *fp;
		
		/* Create wavelegth label */
		if ((fp = fopen("mkdump.txt", "r")) == NULL) {
			if ((fp = fopen("mkdump.txt", "w")) == NULL)
				error("Unable to reate debug file mkdump.txt");
			for (j = 0; j < m->nwav; j++)
				fprintf(fp,"%f ",XSPECT_WL(m->wl_short, m->wl_long, m->nwav, j));
			fprintf(fp,"\n");
			fclose(fp);
		}
		if ((fp = fopen("mkdump.txt", "a")) == NULL)
			error("Unable to open debug file mkdump.txt");

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
	munki_scale_specrd(p, specrd, numpatches, specrd);

#ifdef PLOT_DEBUG
	printf("Calibrated measuerment spectra:\n");
	plot_wav(m, specrd[0]);
#endif

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
	double *abssens;		/* Linearsised absolute sensor raw values */
	int nmeasuered;			/* Number actually measured */
	double sensavg;			/* Overall average of sensor readings */
	double darkthresh;		/* Dark threshold */
	double trackmax[2];		/* Track optimum target */
	double maxval;			/* Maximum multimeas value */
	int rv;

	if (s->reflective)
		error("munki_trialmeasure: Assert - not meant to be used for reflective read!");
	
	if (nummeas <= 0)
		return MUNKI_INT_ZEROMEASURES;
		
	/* Allocate up front to avoid delay between trigger and read */
	bsize = NRAWB * nummeas;
	if ((buf = (unsigned char *)malloc(sizeof(unsigned char) * bsize)) == NULL) {
		DBG((dbgo,"munki_trialmeasure malloc %d bytes failed (12)\n",bsize))
		if (p->verb) printf("Malloc %d bytes failed (12)\n",bsize);
		return MUNKI_INT_MALLOC;
	}
	multimes = dmatrix(0, nummeas-1, 0, m->nraw-1);
	abssens = dvector(0, m->nraw-1);

	DBG((dbgo,"Triggering measurement cycle, nummeas %d, inttime %f, gainmode %d\n",
	                                              nummeas, *inttime, gainmode))

	if ((ev = munki_trigger_one_measure(p, nummeas, inttime, gainmode, 1, 0)) != MUNKI_OK) {
		free_dvector(abssens, 0, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
		free(buf);
		return ev;
	}

	DBG((dbgo,"Gathering readings\n"))
	if ((ev = munki_readmeasurement(p, nummeas, m->c_measmodeflags & MUNKI_MMF_SCAN,
	                                             buf, bsize, &nmeasuered, 1, 0)) != MUNKI_OK) {
		free_dvector(abssens, 0, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
		free(buf);
		return ev;
	}

	if (saturated != NULL)			/* Initialize return flag */
		*saturated = 0;

	/* Take a buffer full of raw readings, and convert them to */
	/* floating point sensor readings. Check for saturation */
	if (munki_meas_to_sens(p, multimes, NULL, buf, 0, nmeasuered, m->satlimit, &darkthresh)
	 && saturated != NULL) {
		*saturated = 1;
	}
	free(buf);

	/* Comute dark subtraction for this trial's parameters */
	if ((ev = munki_interp_dark(p, s->dark_data, *inttime, gainmode)) != MUNKI_OK) {
		free_dvector(abssens, 0, m->nraw-1);
		free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);
		DBG((dbgo,"munki_imp_measure interplate dark ref failed\n"))
		return ev;
	}

	trackmax[0] = darkthresh;		/* Track dark threshold */
	trackmax[1] = m->optsval;		/* Track the optimal sensor target value */

	/* Subtract the black from sensor values and convert to */
	/* absolute (integration & gain scaled), zero offset based, */
	/* linearized sensor values. */
	/* Return the highest individual element. */
	munki_sub_sens_to_abssens(p, nmeasuered, *inttime, gainmode, multimes, s->dark_data,
	                             trackmax, 2, &maxval);
	darkthresh = trackmax[0];

	/* Average a set of measurements into one. */
	/* Return nz if the readings are not consistent */
	/* Return the overall average. */
	rv = munki_average_multimeas(p, abssens, multimes, nmeasuered, &sensavg, darkthresh);     

	/* Ignore any iconsistency ? */

#ifdef PLOT_DEBUG
	printf("Average absolute sensor readings, average = %f, max %f\n",sensavg,maxval);
	plot_raw(abssens);
#endif

	if (optscale != NULL) {
		double opttarget;		/* Optimal sensor target */

		opttarget = targoscale * trackmax[1];
		if (maxval < 0.01)		/* Could go -ve */
			maxval = 0.01;
		*optscale = opttarget/ maxval; 
		RLDBG(3,(dbgo,"Targscale %f, maxval %f, optimal target = %f, amount to scale = %f\n",
		     targoscale, maxval, opttarget, *optscale))
	}

	free_dvector(abssens, 0, m->nraw-1);
	free_dmatrix(multimes, 0, nummeas-1, 0, m->nraw-1);		/* Free after using *pmax */

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
	*inttime = dintclocks * m->intclkp;		/* Quantized integration time */

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

#ifdef RAWR_DEBUG
#define RRDBG(xxx) fprintf xxx ;
#else
#define RRDBG(xxx) 
#endif	/* RAWR_DEBUG */

/* Take a buffer full of raw readings, and convert them to */
/* directly to floating point sensor values. Return nz if any is saturated */
/* (No black subtraction or liniearization is performed) */
int munki_meas_to_sens(
	munki *p,
	double **sens,			/* Array of [nummeas-ninvalid][nraw] value to return */
	double *ledtemp,		/* Optional array [nummeas-ninvalid] LED temperature values to return */
	unsigned char *buf,		/* Raw measurement data must be 274 * nummeas */
	int ninvalid,			/* Number of initial invalid readings to skip */
	int nummeas,			/* Number of readings measured */
	double satthresh,		/* Saturation threshold to trigger error in raw units (if > 0.0) */
	double *pdarkthresh		/* Return a dark threshold value */
) {
	munkiimp *m = (munkiimp *)p->m;
	int nraw = m->nraw;
	int i, j, k;
	unsigned char *bp;
	double maxval = -1e38;
	double darkthresh = 0.0;
	double ndarkthresh = 0.0;
	int sskip = 2 * 6;		/* Bytes to skip at start */
	int eskip = 2 * 3;		/* Bytes to skip at end */

	if ((NRAW  * 2 + sskip + eskip) != NRAWB)
		error("munki_meas_to_sens: Assert, NRAW %d and NRAWB %d don't match!",NRAW,NRAWB);

	if (ninvalid >= nummeas)
		error("munki_meas_to_sens: Assert, ninvalid %d is >= nummeas %d!",ninvalid,nummeas);

	if (p->debug >= 2 && ninvalid > 0)
		fprintf(stderr,"munki_meas_to_sens: Skipping %d invalid readings\n",ninvalid);

	/* Now process the buffer values */
	for (bp = buf + ninvalid * NRAWB, i = 0; i < nummeas; i++, bp += eskip) {

		/* We use the first 4 readings as an estimate of the dark threshold, */
		/* later used in determening reading consistency. */
		for (k = 0; k < 4; k++) {
			darkthresh += (double)buf2ushort(bp + k * 2);
			ndarkthresh++;
		}
		
		/* The LED voltage drop is the last 16 bits in each reading */
		if (ledtemp != NULL)
			ledtemp[i] = (double)buf2ushort(bp + NRAWB - 2);

		for (bp += sskip, j = 0; j < nraw; j++, bp += 2)  {
			unsigned int rval;
			double fval;

			rval = buf2ushort(bp);
			fval = (double)rval;
			sens[i][j] = fval;
//printf("~1 i = %d, j = %d, addr % 274 = %d, val = %f\n",i,j,(bp - buf) % 274, fval); 

			if (fval > maxval)
				maxval = fval;
		}
	}

	/* Check if any are over saturation threshold */
	if (satthresh > 0.0) {
		if (maxval > satthresh) {
			RLDBG(3,(dbgo,"munki_meas_to_sens: Max sens %f > satthresh %f\n",maxval,satthresh))
			return 1;
		}
		RLDBG(3,(dbgo,"munki_meas_to_sens: Max sens %f < satthresh %f\n",maxval,satthresh))
	}

	darkthresh /= ndarkthresh;
	if (pdarkthresh != NULL)
		*pdarkthresh = darkthresh;
	DBG((dbgo,"munki_meas_to_sens: Dark thrheshold = %f\n",darkthresh))

	return 0;
}

/* Subtract the black from sensor values and convert to */
/* absolute (integration & gain scaled), zero offset based, */
/* linearized sensor values. */
/* Return the highest individual element. */
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
) {
	munkiimp *m = (munkiimp *)p->m;
	int npoly;			/* Number of linearisation coefficients */
	double *polys;		/* the coeficients */
	double scale;		/* Absolute scale value */
	double rawmax, maxval = -1e38;
	double maxzero = 0.0;
	int i, j, k;
	
	if (gainmode) {				/* High gain */
		npoly = m->nlin1;		/* Encodes gain too */
		polys = m->lin1;
	} else {					/* Low gain */
		npoly = m->nlin0;
		polys = m->lin0;
	}
	scale = 1.0/inttime;

	/* For each measurement */
	for (i = 0; i < nummeas; i++) {

		/* For each wavelength */
		for (j = 0; j < m->nraw; j++) {
			double rval, sval, lval;

			rval = abssens[i][j];
			RRDBG((dbgo,"% 3d: rval 0x%x, ",j, rval))
			sval = rval - sub[j];		/* Make zero based */
			RRDBG((dbgo,"sval %.1f, ",sval))

#ifdef ENABLE_NONLINCOR	
			/* Linearise */
			for (lval = polys[npoly-1], k = npoly-2; k >= 0; k--)
				lval = lval * sval + polys[k];
#else
			lval = sval;
#endif
			RRDBG((dbgo,"lval %.1f, ",lval))
			lval *= scale;
			abssens[i][j] = lval;
			RRDBG((dbgo,"absval %.1f\n",lval))

			/* Track the maximum value and the black that was subtracted from it */ 
			if (lval > maxval) {
				maxval = lval;
				rawmax = rval;
				maxzero = sub[j];
				if (maxv != NULL)
					*maxv = abssens[i][j];
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
//printf("~1 trackmax[%d] = %f, maxzero = %f\n",i,lval,maxzero);
		}
	}
}

#ifdef RAWR_DEBUG
#undef RRDBG
#endif

/* Average a set of sens or absens measurements into one. */
/* (Make sure darkthresh is tracked if absens is being averaged!) */
/* Return zero if readings are consistent and not saturated. */
/* Return nz if the readings are not consistent */
/* Return the overall average. */
int munki_average_multimeas(
	munki *p,
	double *avg,			/* return average [nraw] */
	double **multimeas,		/* Array of [nummeas][nraw] value to average */
	int nummeas,			/* number of readings to be averaged */
	double *poallavg,		/* If not NULL, return overall average of bands and measurements */
	double darkthresh		/* Dark threshold (used for consistency check scaling) */
) {
	munkiimp *m = (munkiimp *)p->m;
	int nraw = m->nraw;
	int i, j;
	double oallavg = 0.0;
	double maxavg = -1e38;		/* Track min and max averages of readings */
	double minavg = 1e38;
	double norm;
	int rv = 0;
	
	DBG((dbgo,"munki_average_multimeas %d readings (darkthresh %f)\n",nummeas,darkthresh))

	for (j = 0; j < nraw; j++) 
		avg[j] = 0.0;

	/* Now process the buffer values */
	for (i = 0; i < nummeas; i++) {
		double measavg = 0.0;

		for (j = 0; j < nraw; j++) {
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

	for (j = 0; j < nraw; j++) 
		avg[j] /= (double)nummeas;
	oallavg /= (double)nummeas;

	if (poallavg != NULL)
		*poallavg = oallavg;

	norm = fabs(0.5 * (maxavg+minavg));
	darkthresh = fabs(darkthresh);
	if (darkthresh < DARKTHSCAMIN)
		darkthresh = DARKTHSCAMIN;
	DBG((dbgo,"norm = %f, dark thresh = %f\n",norm,darkthresh))
	if (norm < (2.0 * darkthresh))
		norm = 2.0 * darkthresh;

	RLDBG(3, (dbgo,"avg_multi: overall avg = %f, minavg = %f, maxavg = %f, variance %f, THR %f (darkth %f)\n",
	                   oallavg,minavg,maxavg,(maxavg - minavg)/norm, PATCH_CONS_THR,darkthresh))
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
#define MIN_SAMPLES 2

/* Range of bands to detect transitions */
#define BL 5	/* Start */
#define BH 105	/* End */
#define BW 5	/* Width */

/* Record of possible patch */
typedef struct {
	int ss;				/* Start sample index */
	int no;				/* Number of samples */
	int use;			/* nz if patch is to be used */
} munki_patch;

/* Recognise the required number of ref/trans patch locations, */
/* and average the measurements within each patch. */
/* Return zero if readings are consistent. */
/* Return nz if the readings are not consistent */
munki_code munki_extract_patches_multimeas(
	munki *p,
	int *flags,				/* return flags */
	double **pavg,			/* return patch average [naptch][nraw] */
	int tnpatch,			/* Target number of patches to recognise */
	double **multimeas,		/* Array of [nummeas][nraw] value to extract from */
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
#ifdef PATREC_DEBUG
	double **plot;
#endif

	DBG((dbgo,"munki_extract_patches_multimeas with target number of patches = %d\n",tnpatch))
	if (p->debug >= 1) fprintf(stderr,"Patch recognition looking for %d patches out of %d samples\n",tnpatch,nummeas);

	maxval = dvectorz(0, m->nraw-1);  

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
	if ((pat = (munki_patch *)malloc(sizeof(munki_patch) * apat)) == NULL)
		error("munki: malloc of patch structures failed!");

	avglegth = 0.0;
	for (npat = i = 0; i < (nummeas-1); i++) {
		if (slope[i] > thresh)
			continue;

		/* Start of a new patch */
		if (npat >= apat) {
			apat *= 2;
			if ((pat = (munki_patch *)realloc(pat, sizeof(munki_patch) * apat)) == NULL)
				error("munki: reallloc of patch structures failed!");
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
		return MUNKI_RD_NOTENOUGHPATCHES;
	} else if (npat >= (2 * tnpatch) + 2) {
		free_ivector(sizepop, 0, nummeas-1);
		free_dvector(slope, 0, nummeas-1);  
		free_dvector(maxval, 0, m->nraw-1);  
		free(pat);
		if (p->debug >= 1)
			fprintf(stderr,"Patch recog failed - detecting too many possible patches\n");
		return MUNKI_RD_TOOMANYPATCHES;
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
						return MUNKI_RD_NOTENOUGHSAMPLES;
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
			return MUNKI_RD_TOOMANYPATCHES;
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
		return MUNKI_RD_NOTENOUGHPATCHES;
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
			PRDBG((dbgo,"Too few samples\n"))
			free_dvector(slope, 0, nummeas-1);  
			free_ivector(sizepop, 0, nummeas-1);
			free_dvector(maxval, 0, m->nraw-1);  
			free(pat);
			if (p->debug >= 1)
				fprintf(stderr,"Patch recog failed - patches sampled too sparsely\n");
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
		PRDBG((dbgo,"Patch %d: consistency = %f%%, thresh = %f%%\n",pix,100.0 * cons, 100.0 * patch_cons_thr))
		if (cons > patch_cons_thr) {
			if (p->debug >= 1)
				fprintf(stderr,"Patch recog failed - patch %d is inconsistent (%f%%)\n",pix, cons);
			rv |= 1;
		}
		pix++;
	}

	if (flags != NULL)
		*flags = rv;

#ifdef PATREC_DEBUG
	free_dmatrix(plot, 0, 6, 0, nummeas-1);  
#endif

	free_dvector(slope, 0, nummeas-1);  
	free_ivector(sizepop, 0, nummeas-1);
	free_dvector(maxval, 0, m->nraw-1);  
	free(pat);

	DBG((dbgo,"munki_extract_patches_multimeas done, sat = %s, inconsist = %s\n",
	                  rv & 2 ? "true" : "false", rv & 1 ? "true" : "false"))

	if (p->debug >= 1) fprintf(stderr,"Patch recognition returning OK\n");

	return MUNKI_OK;
}

#undef BL
#undef BH
#undef BW

/* Recognise any flashes in the readings, and */
/* and average their values together as well as summing their duration. */
/* The readings are integrated, so the the units are cd/m^2 seconds. */
/* Return nz on an error */
munki_code munki_extract_patches_flash(
	munki *p,
	int *flags,				/* return flags */
	double *duration,		/* return duration */
	double *pavg,			/* return patch average [nraw] */
	double **multimeas,		/* Array of [nummeas][nraw] value to extract from */
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
	double *aavg;				/* ambient average [nraw] */
	double finttime;			/* Flash integration time */
	int rv = 0;
#ifdef PATREC_DEBUG
	double **plot;
#endif

	DBG((dbgo,"munki_extract_patches_flash\n"))
	if (p->debug >= 1) fprintf(stderr,"Patch recognition looking flashes in %d measurements\n",nummeas);

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
		if (p->debug >= 1) fprintf(stderr,"No flashes found in measurement\n");
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
	PRDBG((dbgo,"munki_extract_patches_flash band %d minval %f maxval %f, mean = %f, thresh = %f\n",maxband,minval,maxval,mean, thresh))

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
		return MUNKI_RD_NOFLASHES;

	/* See if there are as many samples before the first flash */
	if (nsampl < 6)
		nsampl = 6;

	/* Average nsample samples of ambient */
	i = (fsampl-3-nsampl);
	if (i < 0)
		return MUNKI_RD_NOAMBB4FLASHES;
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

	return MUNKI_OK;
}

#ifdef PATREC_DEBUG
#undef PRDBG
#endif


/* Convert an abssens array from raw wavelengths to output wavelenths */
/* for the current resolution. Apply stray light compensation too. */
void munki_abssens_to_abswav(
	munki *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav] */
	double **abssens		/* Source array [nraw] */
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
					oval += m->rmtx_coef[cx] * abssens[i][sx];
			} else {
				sx = m->emtx_index[j];		/* Starting index */
				for (k = 0; k < m->emtx_nocoef[j]; k++, cx++, sx++)
					oval += m->emtx_coef[cx] * abssens[i][sx];
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

/* Convert an abssens array from raw wavelengths to output wavelenths */
/* for the standard resolution. Apply stray light compensation too. */
void munki_abssens_to_abswav1(
	munki *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav1] */
	double **abssens		/* Source array [nraw] */
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
					oval += m->rmtx_coef1[cx] * abssens[i][sx];
			} else {
				sx = m->emtx_index1[j];		/* Starting index */
				for (k = 0; k < m->emtx_nocoef1[j]; k++, cx++, sx++)
					oval += m->emtx_coef1[cx] * abssens[i][sx];
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

/* Convert an abssens array from raw wavelengths to output wavelenths */
/* for the high resolution. Apply light compensation too. */
void munki_abssens_to_abswav2(
	munki *p,
	int nummeas,			/* Return number of readings measured */
	double **abswav,		/* Desination array [nwav2] */
	double **abssens		/* Source array [nraw] */
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
					oval += m->rmtx_coef2[cx] * abssens[i][sx];
			} else {
				sx = m->emtx_index2[j];		/* Starting index */
				for (k = 0; k < m->emtx_nocoef2[j]; k++, cx++, sx++)
					oval += m->emtx_coef2[cx] * abssens[i][sx];
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
#undef EXISTING_SHAPE		/* Else generate filter shape */
#undef USE_GAUSSIAN			/* Use gaussian filter shape, else lanczos2 */

#ifdef NEVER
/* Plot the matrix coefficients */
void munki_debug_plot_mtx_coef(munki *p, int ref) {
	munkiimp *m = (munkiimp *)p->m;
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
		if (ref) {
			sx = m->rmtx_index[j];		/* Starting index */
//printf("start index = %d, nocoef %d\n",sx,m->rmtx_nocoef[j]);
			for (k = 0; k < m->rmtx_nocoef[j]; k++, cx++, sx++) {
//printf("offset %d, coef ix %d val %f from ccd %d\n",k, cx, m->rmtx_coef[cx], sx);
				yy[5][sx] += 0.5 * m->rmtx_coef[cx];
				yy[i][sx] = m->rmtx_coef[cx];
			}
		} else {
			sx = m->emtx_index[j];		/* Starting index */
//printf("start index = %d, nocoef %d\n",sx,m->emtx_nocoef[j]);
			for (k = 0; k < m->emtx_nocoef[j]; k++, cx++, sx++) {
//printf("offset %d, coef ix %d val %f from ccd %d\n",k, cx, m->emtx_coef[cx], sx);
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
	free_dvector(xx, 0, m->nraw-1);
	free_dmatrix(yy, 0, 2, 0, m->nraw-1);
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

/* Create high resolution mode references, */
/* Create Reflective if ref nz, else create Emissive */
/* We expect this to be called twice, once for each. */
munki_code munki_create_hr(munki *p, int ref) {
	munkiimp *m = (munkiimp *)p->m;
	int nraw = m->nraw;
	int i, j, jj, k, cx, sx;
	munki_fc coeff[40][16];	/* Existing filter cooefficients */
	int nwav1;					/* Number of filters */
	double wl_short1, wl_long1;	/* Ouput wavelength of first and last filters */
	double wl_step1;
	munki_xp xp[41];			/* Crossover points each side of filter */
	munki_code ev = MUNKI_OK;
	rspl *raw2wav;				/* Lookup from CCD index to wavelength */
#ifdef EXISTING_SHAPE
    munki_fs fshape[40 * 16];  /* Existing filter shape */
	int ncp = 0;				/* Number of shape points */
#endif
	int *mtx_index1, **pmtx_index2, *mtx_index2;
	int *mtx_nocoef1, **pmtx_nocoef2, *mtx_nocoef2;
	double *mtx_coef1, **pmtx_coef2, *mtx_coef2;
	
	/* Start with nominal values. May alter these if filters are not unique */
	nwav1 = m->nwav1;
	wl_short1 = m->wl_short1;
	wl_long1 = m->wl_long1;
	wl_step1 = (wl_long1 - m->wl_short1)/(m->nwav1-1.0);

	if (ref) {
		mtx_index1 = m->rmtx_index1;
		mtx_nocoef1 = m->rmtx_nocoef1;
		mtx_coef1 = m->rmtx_coef1;
		mtx_index2 = mtx_nocoef2 = NULL;
		mtx_coef2 = NULL;
		pmtx_index2 = &m->rmtx_index2;
		pmtx_nocoef2 = &m->rmtx_nocoef2;
		pmtx_coef2 = &m->rmtx_coef2;
	} else {
		mtx_index1 = m->emtx_index1;
		mtx_nocoef1 = m->emtx_nocoef1;
		mtx_coef1 = m->emtx_coef1;
		mtx_index2 = mtx_nocoef2 = NULL;
		mtx_coef2 = NULL;
		pmtx_index2 = &m->emtx_index2;
		pmtx_nocoef2 = &m->emtx_nocoef2;
		pmtx_coef2 = &m->emtx_coef2;
	}
	
	/* Convert the native filter cooeficient representation to */
	/* a 2D array we can randomly index. Skip any duplicated */
	/* filter cooeficients. */
	for (cx = j = jj = 0; j < m->nwav1; j++) { /* For each output wavelength */
		if (j >= 40)	/* Assert */
			error("munki: number of output wavelenths is > 40");

		/* For each matrix value */
		sx = mtx_index1[j];		/* Starting index */
		if (jj == 0 && (j+1) < m->nwav1 && sx == mtx_index1[j+1])  {	/* Same index */
//printf("~1 skipping %d\n",j);
			wl_short1 += wl_step1;
			nwav1--;
			cx += mtx_nocoef1[j];
			continue;
		}
		for (k = 0; k < mtx_nocoef1[j]; k++, cx++, sx++) {
			if (k >= 16)	/* Assert */
				error("munki: number of filter coeefs is > 16");

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

		xx = dvectorz(0, m->nraw-1);		/* X index */
		yy = dmatrixz(0, 5, 0, m->nraw-1);	/* Curves distributed amongst 5 graphs */
		
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
		free_dvector(xx, 0, m->nraw-1);
		free_dmatrix(yy, 0, 2, 0, m->nraw-1);
	}
#endif /* HIGH_RES_PLOT */

//printf("~1 computing crossover points\n");
	/* Compute the crossover points between each filter */
	for (i = 0; i < (nwav1-1); i++) {
		double den, y1, y2, y3, y4, yn, xn;	/* Location of intersection */
		double besty = -1e6;

		/* between filter i and i+1, we want to find the two */
		/* raw indexes where the weighting values cross over */
		/* Do a brute force search to avoid making assumptions */
		/* about the raw order. */
		for (j = 0; j < (mtx_nocoef1[i]-1); j++) {
			for (k = 0; k < (mtx_nocoef1[i+1]-1); k++) {
				if (coeff[i][j].ix == coeff[i+1][k].ix
				 && coeff[i][j+1].ix == coeff[i+1][k+1].ix
				 && coeff[i][j].we > 0.0 && coeff[i][j+1].we > 0.0
				 && coeff[i+1][k].we > 0.0 && coeff[i+1][k+1].we > 0.0
				 && ((   coeff[i][j].we >= coeff[i+1][k].we
				 	 && coeff[i][j+1].we <= coeff[i+1][k+1].we)
				  || (   coeff[i][j].we <= coeff[i+1][k].we
				 	 && coeff[i][j+1].we >= coeff[i+1][k+1].we))) {
//printf("~1 got it at %d, %d: %d = %d, %d = %d\n",j,k, coeff[i][j].ix, coeff[i+1][k].ix, coeff[i][j+1].ix, coeff[i+1][k+1].ix);

					/* Compute the intersection of the two line segments */
					y1 = coeff[i][j].we;
					y2 = coeff[i][j+1].we;
					y3 = coeff[i+1][k].we;
					y4 = coeff[i+1][k+1].we;
					den = -y4 + y3 + y2 - y1;
					yn = (y2 * y3 - y1 * y4)/den;
					xn = (y3 - y1)/den;
//printf("~1 den = %f, yn = %f, xn = %f\n",den,yn,xn);
//printf("Intersection %d: wav %f, raw %f, wei %f\n",i+1,xp[i+1].wav,xp[i+1].raw,xp[i+1].wei);
					if (yn > besty) {
						xp[i+1].wav = XSPECT_WL(wl_short1, wl_long1, nwav1, i + 0.5);
						xp[i+1].raw = (1.0 - xn) * coeff[i][j].ix + xn * coeff[i][j+1].ix;
						xp[i+1].wei = yn;
						besty = yn;
//printf("Found new best y\n");
					}
//printf("\n");
				}
			}
		}
		if (besty < 0.0) {	/* Assert */
			error("munki: failed to locate crossover between resampling filters");
		}
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
		if (j >= mtx_nocoef1[0])	/* Assert */
			error("munki: failed to end crossover");
		den = -y4 + y3 + y2 - y1;
		yn = (y2 * y3 - y1 * y4)/den;
		xn = (y3 - y1)/den;
//printf("~1 den = %f, yn = %f, xn = %f\n",den,yn,xn);
		xp[0].wav = XSPECT_WL(wl_short1, wl_long1, nwav1, -0.5);
		xp[0].raw = (1.0 - xn) * coeff[0][j].ix + xn * coeff[0][j+1].ix;
		xp[0].wei = yn;
//printf("End 0 intersection %d: wav %f, raw %f, wei %f\n",0,xp[0].wav,xp[0].raw,xp[0].wei);
//printf("\n");

		x5 = xp[nwav1-2].raw;
		y5 = xp[nwav1-2].wei;
		x6 = xp[nwav1-1].raw;
		y6 = xp[nwav1-1].wei;

//printf("~1 x5 %f, y5 %f, x6 %f, y6 %f\n",x5,y5,x6,y6);
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
		if (j >= mtx_nocoef1[nwav1-1])	/* Assert */
			error("munki: failed to end crossover");
		den = -y4 + y3 + y2 - y1;
		yn = (y2 * y3 - y1 * y4)/den;
		xn = (y3 - y1)/den;
//printf("~1 den = %f, yn = %f, xn = %f\n",den,yn,xn);
		xp[nwav1].wav = XSPECT_WL(wl_short1, wl_long1, nwav1, nwav1-0.5);
		xp[nwav1].raw = (1.0 - xn) * coeff[nwav1-1][j].ix + xn * coeff[nwav1-1][j+1].ix;
		xp[nwav1].wei = yn;
//printf("End 36 intersection %d: wav %f, raw %f, wei %f\n",nwav1+1,xp[nwav1].wav,xp[nwav1].raw,xp[nwav1].wei);
//printf("\n");
	}

#ifdef HIGH_RES_PLOT
	/* Plot original re-sampling curves + crossing points */
	{
		double *xx, *ss;
		double **yy;
		double *xc, *yc;

		xx = dvectorz(0, m->nraw-1);		/* X index */
		yy = dmatrixz(0, 5, 0, m->nraw-1);	/* Curves distributed amongst 5 graphs */
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
		free_dvector(xx, 0, m->nraw-1);
		free_dmatrix(yy, 0, 2, 0, m->nraw-1);
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
			DBG((dbgo,"munki: creating rspl for high res conversion failed"))
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
		ghigh[0] = (double)(nraw-1);
		gres[0] = nraw;
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
#define MXNOFC 64
		munki_fc coeff2[500][MXNOFC];	/* New filter cooefficients */
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
			error("munki: fshmax search failed");
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
		if ((*pmtx_nocoef2 = mtx_nocoef2 = (int *)calloc(m->nwav2, sizeof(int))) == NULL)
			error("munki: malloc mtx_nocoef2 failed!");

		/* For all the useful CCD bands */
		for (i = 0; i < nraw; i++) {
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
				rwl = wl - cwl;			/* relative wavelgth to filter */

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

				if (mtx_nocoef2[j] >= MXNOFC)
					error("munki: run out of high res filter space");

				coeff2[j][mtx_nocoef2[j]].ix = i;
				coeff2[j][mtx_nocoef2[j]++].we = we; 
//printf("~1 filter %d, cwl %f, rwl %f, ix %d, we %f\n",j,cwl,rwl,i,we);
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

		/* Normalise the filters area in CCD space, while maintaining the */
		/* total contribution of each CCD at the target too. */
		{
			double tot = 0.0;
			double ccdweight[NRAW], avgw;	/* Weighting determined by cell widths */
#ifdef NEVER
			double ccdsum[NRAW];
			int ii;
#endif

			/* Normalize the overall filter weightings */
			for (j = 0; j < m->nwav2; j++)
				for (k = 0; k < mtx_nocoef2[j]; k++)
					tot += coeff2[j][k].we;
			tot /= (double)m->nwav2;
			for (j = 0; j < m->nwav2; j++)
				for (k = 0; k < mtx_nocoef2[j]; k++)
					coeff2[j][k].we /= tot;

			/* Determine the relative weights for each CCD */
			avgw = 0.0;
			for (i = 0; i < nraw; i++) {
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
					for (k = 0; k < mtx_nocoef2[j]; k++)
						tot += coeff2[j][k].we;
					err = 1.0 - tot;
	
					for (k = 0; k < mtx_nocoef2[j]; k++)
						coeff2[j][k].we += err/mtx_nocoef2[j];
//					for (k = 0; k < mtx_nocoef2[j]; k++)
//						coeff2[j][k].we *= 1.0/tot;
				}

				/* Check CCD sums */
				for (i = 0; i < nraw; i++) 
					ccdsum[i] = 0.0;

				for (j = 0; j < m->nwav2; j++) {
					for (k = 0; k < mtx_nocoef2[j]; k++)
						ccdsum[coeff2[j][k].ix] += coeff2[j][k].we;
				}

				if (ii >= 6)
					break;

				/* Readjust CCD sum */
				for (i = 0; i < nraw; i++) { 
					ccdsum[i] = ccdtsum[i]/ccdsum[i];		/* Amount to adjust */
				}

				for (j = 0; j < m->nwav2; j++) {
					for (k = 0; k < mtx_nocoef2[j]; k++)
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
				for (k = 0; k < mtx_nocoef2[j]; k++) {
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

			if ((*pmtx_index2 = mtx_index2 = (int *)calloc(m->nwav2, sizeof(int))) == NULL)
				error("munki: malloc mtx_index2 failed!");
	
			xcount = 0;
			for (j = 0; j < m->nwav2; j++) {
				mtx_index2[j] = coeff2[j][0].ix;
				xcount += mtx_nocoef2[j];
			}
	
			if ((*pmtx_coef2 = mtx_coef2 = (double *)calloc(xcount, sizeof(double))) == NULL)
				error("munki: malloc mtx_coef2 failed!");

			for (i = j = 0; j < m->nwav2; j++)
				for (k = 0; k < mtx_nocoef2[j]; k++, i++)
					mtx_coef2[i] = coeff2[j][k].we;
		}

		/* Basic capability is initialised */
		m->hr_inited++;

		/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
		/* If both reflective and emissive samplings have been created, */
		/* deal with upsampling the references and calibrations */
		if (m->hr_inited == 2) {
#ifdef SALONEINSTLIB
			double **slp;			/* 2D Array of stray light values */
#endif /* !SALONEINSTLIB */
			rspl *trspl;			/* Upsample rspl */
			cow sd[40 * 40];		/* Scattered data points of existing references */
			datai glow, ghigh;
			datao vlow, vhigh;
			int gres[2];
			double avgdev[2];
			int ii;
#ifdef PATREC_DEBUG
            int jj;
#endif
			co pp;
	
			/* First the 1D references */
			if ((trspl = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL) {
				DBG((dbgo,"munki: creating rspl for high res conversion failed"))
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

				glow[0] = m->wl_short2;
				ghigh[0] = m->wl_long2;
				gres[0] = m->nwav2;
				avgdev[0] = 0.0;
				
				trspl->fit_rspl_w(trspl, 0, sd, i, glow, ghigh, gres, vlow, vhigh, 0.5, avgdev, NULL);

				if ((*ref2 = (double *)calloc(m->nwav2, sizeof(double))) == NULL) {
					raw2wav->del(raw2wav);
					trspl->del(trspl);
					error("munki: malloc mtx_coef2 failed!");
				}

				/* Create upsampled version */
				for (i = 0; i < m->nwav2; i++) {
					pp.p[0] = XSPECT_WL(m->wl_short2, m->wl_long2, m->nwav2, i);
					trspl->interp(trspl, &pp);
					if (pp.v[0] < 0.0)
						pp.v[0] = 0.0;
					(*ref2)[i] = pp.v[0];
				}


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

#ifdef SALONEINSTLIB
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
#else	/* !SALONEINSTLIB */
			/* Then setup 2D stray light using rspl */
			if ((trspl = new_rspl(RSPL_NOFLAGS, 2, 1)) == NULL) {
				DBG((dbgo,"munki: creating rspl for high res conversion failed"))
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
#endif	/* !SALONEINSTLIB */

			m->straylight2 = dmatrixz(0, m->nwav2-1, 0, m->nwav2-1);  

			/* Create upsampled version */
			for (i = 0; i < m->nwav2; i++) {		/* Output wavelength */
				for (j = 0; j < m->nwav2; j++) {	/* Input wavelength */
					double p0, p1;
					p0 = XSPECT_WL(m->wl_short2, m->wl_long2, m->nwav2, i);
					p1 = XSPECT_WL(m->wl_short2, m->wl_long2, m->nwav2, j);
#ifdef SALONEINSTLIB
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
#else /* !SALONEINSTLIB */
					pp.p[0] = p0;
					pp.p[1] = p1;
					trspl->interp(trspl, &pp);
#endif /* !SALONEINSTLIB */
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

#ifdef SALONEINSTLIB
			free_dmatrix(slp, 0, m->nwav1-1, 0, m->nwav1-1);
#else /* !SALONEINSTLIB */
			trspl->del(trspl);
#endif /* !SALONEINSTLIB */

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
#ifdef NEVER
		printf("~1 regenerating calibration for reflection\n");
		printf("~1 raw white data:\n");
		plot_raw(s->white_data);
#endif	/* NEVER */
							munki_abssens_to_abswav1(p, 1, &s->cal_factor1, &s->white_data);
#ifdef NEVER
		printf("~1 Std res intmd. cal_factor:\n");
		plot_wav1(m, s->cal_factor1);
#endif	/* NEVER */
							munki_abssens_to_abswav2(p, 1, &s->cal_factor2, &s->white_data);
#ifdef NEVER
		printf("~1 High intmd. cal_factor:\n");
		plot_wav2(m, s->cal_factor2);
		printf("~1 Std res white ref:\n");
		plot_wav1(m, m->white_ref1);
		printf("~1 High res white ref:\n");
		plot_wav2(m, m->white_ref2);
#endif	/* NEVER */
							munki_compute_white_cal(p, s->cal_factor1, m->white_ref1, s->cal_factor1,
							                           s->cal_factor2, m->white_ref2, s->cal_factor2);
#ifdef NEVER
		printf("~1 Std res final cal_factor:\n");
		plot_wav1(m, s->cal_factor1);
		printf("~1 High final cal_factor:\n");
		plot_wav2(m, s->cal_factor2);
#endif	/* NEVER */
						}
						break;

					case mk_disp_spot:
					case mk_emiss_spot:
					case mk_emiss_scan:
						for (j = 0; j < m->nwav2; j++)
							s->cal_factor2[j] = EMIS_SCALE_FACTOR * m->emis_coef2[j];
						break;
		
					case mk_proj_spot:
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
							munki_abssens_to_abswav1(p, 1, &s->cal_factor1, &s->white_data);
							munki_abssens_to_abswav2(p, 1, &s->cal_factor2, &s->white_data);
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
	munki_code ev = MUNKI_OK;

#ifdef HIGH_RES
	int i;
	munkiimp *m = (munkiimp *)p->m;

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
	munki_code ev = MUNKI_OK;

#ifdef HIGH_RES
	int i;
	munkiimp *m = (munkiimp *)p->m;

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
	double **specrd		/* Spectral readings */
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
		conv = new_xsp2cie(icxIT_none, NULL, icxOT_CIE_1931_2, NULL, icSigXYZData);
	else
		conv = new_xsp2cie(icxIT_D50, NULL, icxOT_CIE_1931_2, NULL, icSigXYZData);
	if (conv == NULL)
		return MUNKI_INT_CIECONVFAIL;

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
		fprintf(stderr,"munki_conv2XYZ got wl_short %f, wl_long %f, nwav %d, min_wl %f\n",
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

		if (s->emiss) {
			for (j = six, k = 0; j < m->nwav; j++, k++) {
				vals[i].sp.spec[k] = specrd[i][j] * sms;
			}
			vals[i].sp.norm = 1.0;
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
	int j, warn = 0;
	
	DBG((dbgo,"munki_compute_white_cal called"))

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
	double new_int_time;
	double min_int_time;	/* Adjusted min_int_time */
	int    new_gain_mode;
	munkiimp *m = (munkiimp *)p->m;

	RDBG((dbgo,"munki_optimise_sensor called, inttime %f, gain mode %d, scale %f\n",cur_int_time,cur_gain_mode, scale))

	min_int_time = m->min_int_time - deadtime;
	cur_int_time -= deadtime;

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
			return MUNKI_RD_LIGHTTOOLOW;
	}
	DBG((dbgo,"after low light clip, inttime %f, gain mode %d\n",new_int_time,new_gain_mode))

	/* Adjust to high light situation */
	if (new_int_time < min_int_time && *targoscale < 1.0) {
		*targoscale *= min_int_time/new_int_time;
		new_int_time = min_int_time;
	}
	DBG((dbgo,"after high light adjust, targoscale %f, inttime %f, gain mode %d\n",*targoscale, new_int_time,new_gain_mode))

	/* Deal with still high light */
	if (new_int_time < min_int_time) {
		if (permitclip)
			new_int_time = min_int_time;
		else
			return MUNKI_RD_LIGHTTOOHIGH;
	}
	DBG((dbgo,"after high light clip, returning inttime %f, gain mode %d\n",new_int_time,new_gain_mode))

	new_int_time += deadtime;

	RDBG((dbgo,"munki_optimise_sensor returning inttime %f, gain mode %d\n",new_int_time,new_gain_mode))
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
void
munki_prepare_idark(
	munki *p
) {
	munkiimp *m = (munkiimp *)p->m;
	munki_state *s = &m->ms[m->mmode];
	int i, j;

	/* For normal and high gain */
	for (i = 0; i < 4; i+=2) {
		for (j = 0; j < m->nraw; j++) {
			double d01, d1;
			d01 = s->idark_data[i+0][j];
			d1 = s->idark_data[i+1][j];
	
			/* Compute increment proportional to time */
			s->idark_data[i+1][j] = (d1 - d01)/(s->idark_int_time[i+1] - s->idark_int_time[i+0]);
	
			/* Compute base */
			s->idark_data[i+0][j] = d1 - s->idark_data[i+1][j];
		}
	}
}

/* Create the dark reference for the given integration time and gain */
/* by interpolating from the 4 readings prepared earlier. */
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
	if (gainmode)
		i = 2;

	for (j = 0; j < m->nraw; j++) {
		double tt;
		tt = s->idark_data[i+0][j] + inttime * s->idark_data[i+1][j];
		result[j] = tt;
	}
	return MUNKI_OK;
}

/* Set the noautocalib mode */
void munki_set_noautocalib(munki *p, int v) {
	munkiimp *m = (munkiimp *)p->m;
	m->noautocalib = v;
}

/* Set the trigger config */
void munki_set_trig(munki *p, inst_opt_mode trig) {
	munkiimp *m = (munkiimp *)p->m;
	m->trig = trig;
}

/* Return the trigger config */
inst_opt_mode munki_get_trig(munki *p) {
	munkiimp *m = (munkiimp *)p->m;
	return m->trig;
}

/* Set the trigger return */
void munki_set_trigret(munki *p, int val) {
	munkiimp *m = (munkiimp *)p->m;
	m->trig_return = val;
}

/* Switch thread handler */
int munki_switch_thread(void *pp) {
	int nfailed = 0;
	munki *p = (munki *)pp;
	munki_code rv = MUNKI_OK; 
	munkiimp *m = (munkiimp *)p->m;
	DBG((dbgo,"Switch thread started\n"))
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
			DBG((dbgo,"Switch thread failed with 0x%x\n",rv))
			continue;
		}
		if (ecode == mk_eve_button_press)
			m->switch_count++;
	}
	DBG((dbgo,"Switch thread returning\n"))
	return rv;
}

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
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nmunki: Read EEProm address 0x%x size 0x%x\n",addr,size);

	if (size < 0 || addr < 0 || (addr + size) > (m->noeeblocks * m->eeblocksize))
		return MUNKI_INT_EEOUTOFRANGE;

	int2buf(&pbuf[0], addr);
	int2buf(&pbuf[4], size);
  	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0x81, 0, 0, pbuf, 8, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		if (isdeb) fprintf(stderr,"\nmunki: EEprom read failed (1) with ICOM err 0x%x\n",se);
		p->icom->debug = isdeb;
		return rv;
	}

	/* Now read the bytes */
	se = p->icom->usb_read(p->icom, 0x81, buf, size, &rwbytes, 5.0);
	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		if (isdeb) fprintf(stderr,"\nmunki: EEprom read failed (2) with ICOM err 0x%x\n",se);
		p->icom->debug = isdeb;
		return rv;
	}

	if (rwbytes != size) {
		if (isdeb) fprintf(stderr,"Read 0x%x bytes, short read error\n",rwbytes);
		p->icom->debug = isdeb;
		return MUNKI_HW_EE_SHORTREAD;
	}

	if (isdeb >= 5) {
		int i;
		for (i = 0; i < size; i++) {
			if ((i % 16) == 0)
				fprintf(stderr,"    %04x:",i);
			fprintf(stderr," %02x",buf[i]);
			if ((i+1) >= size || ((i+1) % 16) == 0)
				fprintf(stderr,"\n");
		}
	}

	if (isdeb) fprintf(stderr,"Read 0x%x bytes, ICOM err 0x%x\n",rwbytes, se);
	p->icom->debug = isdeb;

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
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nmunki: GetFirmParms\n");

	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0x86, 0, 0, pbuf, 24, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		if (isdeb) fprintf(stderr,"\nmunki:  GetFirmParms failed with ICOM err 0x%x\n",se);
		p->icom->debug = isdeb;
		return rv;
	}

	_fwrev_maj   = buf2int(&pbuf[0]);
	_fwrev_min   = buf2int(&pbuf[4]);
	_tickdur     = buf2int(&pbuf[8]);
	_minintcount = buf2int(&pbuf[12]);
	_noeeblocks  = buf2int(&pbuf[16]);
	_eeblocksize = buf2int(&pbuf[20]);

	if (isdeb) fprintf(stderr," GetFirmParms returns fwrev %d.%d, tickdur %d, minint %d, eeblks %d, eeblksz %d ICOM err 0x%x\n",
	    _fwrev_maj, _fwrev_min, _tickdur, _minintcount, _noeeblocks, _eeblocksize, se);

	p->icom->debug = isdeb;

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
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nmunki: GetChipID\n");

	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0x8A, 0, 0, chipid, 8, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		if (isdeb) fprintf(stderr,"\nmunki:  GetChipID failed with ICOM err 0x%x\n",se);
		p->icom->debug = isdeb;
		return rv;
	}

	if (isdeb) fprintf(stderr," GetChipID returns %02x-%02x%02x%02x%02x%02x%02x%02x ICOM err 0x%x\n",
                           chipid[0], chipid[1], chipid[2], chipid[3],
                           chipid[4], chipid[5], chipid[6], chipid[7], se);

	p->icom->debug = isdeb;

	return rv;
}

/* Get the Version String */
munki_code
munki_getversionstring(
	munki *p,
	char vstring[37]
) {
	int se, rv = MUNKI_OK;
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nmunki: GetVersionString\n");

	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0x85, 0, 0, (unsigned char *)vstring, 36, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		if (isdeb) fprintf(stderr,"\nmunki:  GetVersionString failed with ICOM err 0x%x\n",se);
		p->icom->debug = isdeb;
		return rv;
	}

	vstring[36] = '\000';

	if (isdeb) fprintf(stderr," GetVersionString returns '%s' ICOM err 0x%x\n", vstring, se);

	p->icom->debug = isdeb;

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
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nmunki: GetMeasState\n");

	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0x8F, 0, 0, pbuf, 16, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		if (isdeb) fprintf(stderr,"\nmunki:  GetMeasState failed with ICOM err 0x%x\n",se);
		p->icom->debug = isdeb;
		return rv;
	}

	_ledtrange   = buf2int(&pbuf[0]);
	_ledtemp     = buf2int(&pbuf[4]);
	_dutycycle   = buf2int(&pbuf[8]);
	_ADfeedback  = buf2int(&pbuf[12]);

	if (isdeb) fprintf(stderr," GetMeasState returns LED temp range %d, LED temp %d, Duty Cycle %d, ADFeefback %d, ICOM err 0x%x\n",
	    _ledtrange, _ledtemp, _dutycycle, _ADfeedback, se);

	p->icom->debug = isdeb;

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
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nmunki: GetStatus\n");

	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0x87, 0, 0, pbuf, 2, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		if (isdeb) fprintf(stderr,"\nmunki:  GetStatus failed with ICOM err 0x%x\n",se);
		p->icom->debug = isdeb;
		return rv;
	}

	_spos = (mk_spos)pbuf[0];
	_but   = (mk_but)pbuf[1];

	if (isdeb) {
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
		if (_but == mk_but_button_release)
			strcpy(sb2, "Released");
		else if (_but == mk_but_button_press)
			strcpy(sb2, "Pressed");
		else
			sprintf(sb2,"Unknown 0x%x",_but);

		fprintf(stderr," GetStatus Sensor pos. %s, Button state %s, ICOM err 0x%x\n",
	            sb1, sb2, se);
	}

	p->icom->debug = isdeb;

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
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nmunki: SetIndLED %d, %d, %d, %d, %d\n",
	                   p1, p2, p3, p4, p5);

	int2buf(&pbuf[0], p1);
	int2buf(&pbuf[4], p2);
	int2buf(&pbuf[8], p3);
	int2buf(&pbuf[12], p4);
	int2buf(&pbuf[16], p5);

	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0x92, 0, 0, pbuf, 20, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		if (isdeb) fprintf(stderr,"\nmunki: SetIndLED failed with ICOM err 0x%x\n",se);
		p->icom->debug = isdeb;
		return rv;
	}

	if (isdeb) fprintf(stderr,"SetIndLED got ICOM err 0x%x\n",se);

	p->icom->debug = isdeb;

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
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nmunki: TriggerMeasure lamp %d, scan %d, gain %d, intclks %d, nummeas %d\n",
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
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0x80, 0, 0, pbuf, 12, 2.0);

	m->tr_t2 = msec_time();     /* Diagnostic */

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		if (isdeb) fprintf(stderr,"\nmunki: TriggerMeasure failed with ICOM err 0x%x\n",se);
		p->icom->debug = isdeb;
		return rv;
	}

	if (isdeb) fprintf(stderr,"TriggerMeasure got ICOM err 0x%x\n",se);

	p->icom->debug = isdeb;

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
	int isdeb = 0;
//	int gotshort = 0;			/* nz when got a previous short reading */

	if ((bsize % NRAWB) != 0) {
		return MUNKI_INT_ODDREADBUF;
	}

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	extra = 1.0;		/* Extra timeout margin */

#ifdef SINGLE_READ
	if (scanflag == 0)
		nmeas = inummeas;
	else
		nmeas = bsize / NRAWB;		/* Use a single large read */
#else
	nmeas = inummeas;				/* Smaller initial number of measurements */
#endif

	top = extra + m->c_inttime * nmeas;

	if (isdeb) fprintf(stderr,"\nmunki: Read measurement results: inummeas %d, scanflag %d, address %p bsize 0x%x, timout %f\n",inummeas, scanflag, buf, bsize, top);

	for (;;) {
		int size;		/* number of bytes to read */

		size = NRAWB * nmeas;

		if (size > bsize) {		/* oops, no room for read */
			if (isdeb) fprintf(stderr,"Buffer was too short for scan\n");
			p->icom->debug = isdeb;
			return MUNKI_INT_MEASBUFFTOOSMALL;
		}

		m->tr_t6 = msec_time();	/* Diagnostic, start of subsequent reads */
		if (m->tr_t3 == 0) m->tr_t3 = m->tr_t6;		/* Diagnostic, start of first read */

		if (isdeb)  fprintf(stderr,"about to call usb_read with %d bytes\n",size);
		se = p->icom->usb_read(p->icom, 0x81, buf, size, &rwbytes, top);

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
		} else if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
			if (m->trig_rv != MUNKI_OK) {
				if (isdeb) fprintf(stderr,"\nmunki: Measurement trigger failed, ICOM err 0x%x\n",m->trig_se);
				return m->trig_rv;
			}
			if (isdeb && (se & ICOM_TO))
				fprintf(stderr,"\nmunki: Read timed out with top = %f\n",top);
		
			if (isdeb) fprintf(stderr,"\nmunki: Read failed, bytes read 0x%x, ICOM err 0x%x\n",rwbytes, se);
			p->icom->debug = isdeb;
			return rv;
		}
 
		/* If we didn't read a multiple of NRAWB, we've got problems */
		if ((rwbytes % NRAWB) != 0) {
			if (isdeb) fprintf(stderr,"Read 0x%x bytes, odd read error\n",rwbytes);
			p->icom->debug = isdeb;
			return MUNKI_HW_ME_ODDREAD;
		}

		/* Track where we're up to */
		bsize -= rwbytes;
		buf   += rwbytes;
		treadings += rwbytes/NRAWB;

		if (scanflag == 0) {	/* Not scanning */

			/* Expect to read exactly what we asked for */
			if (rwbytes != size) {
				if (isdeb) fprintf(stderr,"Error - unexpected short read, got %d expected %d\n",rwbytes,size);
				p->icom->debug = isdeb;
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
			if (isdeb)  fprintf(stderr,"done because read %d bytes != %d\n",rwbytes,size);
			break;
		}
#endif

		if (bsize == 0) {		/* oops, no room for more scanning read */
			unsigned char tbuf[NRAWB];

			/* We need to clean up, so soak up all the data and throw it away */
			while ((se = p->icom->usb_read(p->icom, 0x81, tbuf, NRAWB, &rwbytes, top)) == ICOM_OK)
				;
			if (isdeb) fprintf(stderr,"Buffer was too short for scan\n");
			p->icom->debug = isdeb;
			return MUNKI_INT_MEASBUFFTOOSMALL;
		}

		/* Read a bunch more readings until the read is short or times out */
		nmeas = bsize / NRAWB;
		if (nmeas > 64)
			nmeas = 64;
		top = extra + m->c_inttime * nmeas;
	}

	/* Must have timed out in initial readings */
	if (treadings < inummeas) {
		if (isdeb) fprintf(stderr,"\nmunki: Read failed, bytes read 0x%x, ICOM err 0x%x\n",rwbytes, se);
		p->icom->debug = isdeb;
		return MUNKI_RD_SHORTMEAS;
	} 

	if (isdeb >= 5) {
		int i;
		for (i = 0; i < (treadings * NRAWB); i++) {
			if ((i % 16) == 0)
				fprintf(stderr,"    %04x:",i);
			fprintf(stderr," %02x",ibuf[i]);
			if ((i+1) >= (treadings * NRAWB) || ((i+1) % 16) == 0)
				fprintf(stderr,"\n");
		}
	}

	if (isdeb)	{
		fprintf(stderr,"Read %d readings, ICOM err 0x%x\n",treadings, se);
		fprintf(stderr,"(Trig & rd times %d %d %d %d)\n",
		    m->tr_t2-m->tr_t1, m->tr_t3-m->tr_t2, m->tr_t4-m->tr_t3, m->tr_t6-m->tr_t5);
	}
	p->icom->debug = isdeb;

	if (nummeas != NULL) *nummeas = treadings;

	return rv;
}

/* Simulating an event */
munki_code munki_simulate_event(munki *p, mk_eve ecode,  int timestamp) {
	munkiimp *m = (munkiimp *)p->m;
	unsigned char pbuf[8];	/* 8 bytes to write */
	int se, rv = MUNKI_OK;
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nmunki: SimulateEvent 0x%x\n",ecode);

	int2buf(&pbuf[0], ecode);
	int2buf(&pbuf[4], timestamp);	/* msec since munki power up */

	se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0x8E, 0, 0, pbuf, 8, 2.0);

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		if (isdeb) fprintf(stderr,"\nmunki: Warning: SimulateEvent 0x%x failed with ICOM err 0x%x\n",ecode,se);
	} else {
		if (isdeb) fprintf(stderr,"SimulateEvent 0x%x done, ICOM err 0x%x\n",ecode,se);
	}

	/* In case the above didn't work, cancel the I/O */
	msec_sleep(50);
	if (m->th_termed == 0) {
		DBG((dbgo,"Munki terminate switch thread failed, canceling I/O\n"))
		p->icom->usb_cancel_io(p->icom, m->hcancel);
	}
	
	p->icom->debug = isdeb;
	return rv;
}

/* Wait for a reply triggered by an event */
munki_code munki_waitfor_switch(munki *p, mk_eve *ecode, int *timest, double top) {
	int rwbytes;			/* Data bytes read */
	unsigned char buf[8];	/* Result  */
	int se, rv = MUNKI_OK;
	int isdeb = p->icom->debug;
	mk_eve _ecode;
	int _timest;

	if (isdeb) fprintf(stderr,"\nmunki: Read 8 bytes from switch hit port\n");

	/* Now read 8 bytes */
	se = p->icom->usb_read(p->icom, 0x83, buf, 8, &rwbytes, top);

	if ((se & ICOM_USERM) == 0 && (se & ICOM_TO)) {
		if (isdeb) fprintf(stderr,"Switch read 0x%x bytes, timed out\n",rwbytes);
		p->icom->debug = isdeb;
		return MUNKI_INT_BUTTONTIMEOUT;
	}

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		if (isdeb) fprintf(stderr,"\nmunki: Switch read failed with ICOM err 0x%x\n",se);
		p->icom->debug = isdeb;
		return rv;
	}

	if (rwbytes != 8) {
		if (isdeb) fprintf(stderr,"Switch read %d bytes, short read error\n",rwbytes);
		p->icom->debug = isdeb;
		return MUNKI_HW_EE_SHORTREAD;
	}

	_ecode  = (mk_eve) buf2int(&buf[0]);
	_timest = buf2int(&buf[4]);
	
	if (isdeb) {
		char sbuf[100];
		if (_ecode == mk_eve_none)
			strcpy(sbuf, "None");
		else if (_ecode == mk_eve_button_press)
			strcpy(sbuf, "Button press");
		else if (_ecode == mk_eve_button_release)
			strcpy(sbuf, "Button release");
		else if (_ecode == mk_eve_spos_change)
			strcpy(sbuf, "Sensor position change");
		else
			sprintf(sbuf,"Unknown 0x%x",_ecode);

		fprintf(stderr,"Event %s, timestamp %d ICOM err 0x%x\n", sbuf, _timest, se);
	}

	if (ecode != NULL) *ecode = _ecode;
	if (timest != NULL) *timest = _timest;

	return rv; 
}

/* Wait for a reply triggered by a key press (thread version) */
/* Returns MUNKI_OK if the switch has been pressed, */
/* or MUNKI_INT_BUTTONTIMEOUT if */
/* no switch was pressed befor the time expired, */
/* or some other error. */
munki_code munki_waitfor_switch_th(munki *p, mk_eve *ecode, int *timest, double top) {
	munkiimp *m = (munkiimp *)p->m;
	int rwbytes;			/* Data bytes read */
	unsigned char buf[8];	/* Result  */
	int se, rv = MUNKI_OK;
	int isdeb = p->icom->debug;
	mk_eve _ecode;
	int _timest;

	if (isdeb) fprintf(stderr,"\nmunki: Read 8 bytes from switch hit port\n");

	/* Now read 8 bytes */
	se = p->icom->usb_read_th(p->icom, &m->hcancel, 0x83, buf, 8, &rwbytes, top, 0, NULL, 0);

	if ((se & ICOM_USERM) == 0 && (se & ICOM_TO)) {
		if (isdeb) fprintf(stderr,"Switch read 0x%x bytes, timed out\n",rwbytes);
		p->icom->debug = isdeb;
		return MUNKI_INT_BUTTONTIMEOUT;
	}

	if ((rv = icoms2munki_err(se)) != MUNKI_OK) {
		if (isdeb) fprintf(stderr,"\nmunki: Switch read failed with ICOM err 0x%x\n",se);
		p->icom->debug = isdeb;
		return rv;
	}

	if (rwbytes != 8) {
		if (isdeb) fprintf(stderr,"Switch read %d bytes, short read error\n",rwbytes);
		p->icom->debug = isdeb;
		return MUNKI_HW_EE_SHORTREAD;
	}

	_ecode  = (mk_eve) buf2int(&buf[0]);
	_timest = buf2int(&buf[4]);			/* msec since munki power up */

	if (isdeb) {
		char sbuf[100];
		if (_ecode == mk_eve_none)
			strcpy(sbuf, "None");
		else if (_ecode == mk_eve_button_press)
			strcpy(sbuf, "Button press");
		else if (_ecode == mk_eve_button_release)
			strcpy(sbuf, "Button release");
		else if (_ecode == mk_eve_spos_change)
			strcpy(sbuf, "Sensor position change");
		else
			sprintf(sbuf,"Unknown 0x%x",_ecode);

		fprintf(stderr,"Event %s, timestamp %d ICOM err 0x%x\n", sbuf, _timest, se);
	}

	if (isdeb) fprintf(stderr,"Switch read %d bytes OK\n",rwbytes);

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

	DBG((dbgo,"munki_parse_eeprom called with %d bytes\n",len))

	/* Check the checksum */
	chsum = buf2uint(buf+8);
	int2buf(buf+8, 0);				/* Zero it out */
	
	for (sum = 0, i = 0; i < (len-3); i += 4) {
		sum += buf2uint(buf + i);
	}



	if (p->debug >= 1) fprintf(stderr,"cal chsum = 0x%x, should be 0x%x - %s\n",sum,chsum, sum == chsum ? "OK": "BAD");
	if (sum != chsum)
		return MUNKI_INT_CALBADCHSUM;


	/* Create class to handle EEProm parsing */
	if ((d = m->data = new_mkdata(p, buf, len, p->verb, p->debug)) == NULL)
		return MUNKI_INT_CREATE_EEPROM_STORE;

	/* Check out the version */
	if (d->get_u16_ints(d, &calver, 0, 1) == NULL)
		return MUNKI_DATA_RANGE;
	if (d->get_u16_ints(d, &compver, 2, 1) == NULL)
		return MUNKI_DATA_RANGE;
	if (p->debug >= 4) fprintf(stderr,"cal version = %d, compatible with %d\n",calver,compver);

	/* We understand versions 3 to 6 */

	if (calver < 3 || compver < 3 || compver > 6)
		return MUNKI_HW_CALIBVERSION;

	/* Choose the version we will treat it as */
	if (calver > 6 && compver <= 6)
		m->calver = 6;
	else
		m->calver = calver;
	if (p->debug >= 4) fprintf(stderr,"Treating as cal version = %d\n",m->calver);

	/* Parse all the calibration info common for vers 3 - 6 */

	/* Production number */
	if (d->get_32_ints(d, &m->prodno, 12, 1) == NULL)
		return MUNKI_DATA_RANGE;
	if (p->debug >= 4) fprintf(stderr,"Produnction no = %d\n",m->prodno);

	/* Chip HW ID */
	if (d->get_8_char(d, (unsigned char *)chipid, 16, 8) == NULL)
		return MUNKI_DATA_RANGE;
	if (p->debug >= 4) fprintf(stderr,"HW Id = %02x-%02x%02x%02x%02x%02x%02x%02x\n",
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
	if (p->debug >= 4) fprintf(stderr,"serial number '%s'\n",m->serno);

	/* Underlying calibration information */

	m->nraw = NRAW;			/* Raw sample bands stored */
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

#ifdef NEVER
// ~~~ hack !!!
m->rmtx_index1[4] = m->rmtx_index1[5] + 3;
m->rmtx_index1[3] = m->rmtx_index1[4] + 3;
m->rmtx_index1[2] = m->rmtx_index1[3] + 3;
m->rmtx_index1[1] = m->rmtx_index1[2] + 3;
m->rmtx_index1[0] = m->rmtx_index1[1] + 3;

for(i = 0; i < 5; i++) {
	for (j = 0; j < 16; j++) {
		m->rmtx_coef1[i * 16 + j] = m->rmtx_coef1[5 * 16 + j]; 
	}
}
#endif

	if (p->debug >= 5) {
		fprintf(stderr,"Reflectance matrix:\n");
		for(i = 0; i < 36; i++) {
			fprintf(stderr," Wave %d, index %d\n",i, m->rmtx_index1[i]);
			for (j = 0; j < 16; j++) {
				if (m->rmtx_coef1[i * 16 + j] != 0.0)
					fprintf(stderr,"  Wt %d =  %f\n",j, m->rmtx_coef1[i * 16 + j]);
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

	if (p->debug >= 5) {
		fprintf(stderr,"Emmission matrix:\n");
		for(i = 0; i < 36; i++) {
			fprintf(stderr," Wave %d, index %d\n",i, m->emtx_index1[i]);
			for (j = 0; j < 16; j++) {
				if (m->emtx_coef1[i * 16 + j] != 0.0)
					fprintf(stderr,"  Wt %d =  %f\n",j, m->emtx_coef1[i * 16 + j]);
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

	if (p->debug >= 4) fprintf(stderr,"Sensor targmin %.0f, opt %.0f, max %.0f, sat %.0f\n",
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

	if (p->debug >= 4) fprintf(stderr,"Cal int time %f, LED pre-heat %f, Led wait %f, LED hold temp duty cycle %d\n", m->cal_int_time, m->ledpreheattime, m->ledwaittime, m->ledholdtempdc);

	if (d->get_u16_ints(d, &tint, 5422, 1) == NULL)
		return MUNKI_DATA_RANGE;
	m->refinvalidsampt = tint * 1e-3;	/* Convert to seconds */

	if (d->get_32_ints(d, &tint, 5424, 1) == NULL)
		return MUNKI_DATA_RANGE;
	m->calscantime = tint * 1e-3;	/* Convert to seconds */

	if (p->debug >= 4) fprintf(stderr,"Invalid sample time %f, Cal scan time %f\n",
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

	if (p->debug >= 5) {
		fprintf(stderr,"Stray Light matrix:\n");
		for(i = 0; i < 36; i++) {
			fprintf(stderr," Wave %d, index %d\n",i, m->rmtx_index1[i]);
			for (j = 0; j < 36; j++)
				fprintf(stderr,"  Wt %d = %f\n",j, m->straylight1[i][j]);
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
		if (p->debug >= 4) fprintf(stderr,"Faked up projector cal reference\n");
	}

	if (m->calver >= 6) {
		if (d->get_8_ints(d, &m->adctype, 8168, 1) == NULL)
			return MUNKI_DATA_RANGE;
	} else {
		m->adctype = 0;
	}

	if (p->debug >= 4) {
		fprintf(stderr,"White ref, emission cal, ambient cal, proj cal:\n");
		for(i = 0; i < 36; i++) {
			fprintf(stderr," %d: %f, %f, %f, %f\n",i, m->white_ref1[i], m->emis_coef1[i],
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
	DBG((dbgo, "highgain = %f\n",m->highgain))

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
	
	free(d);
}

/* Constructor for mkdata */
mkdata *new_mkdata(munki *p, unsigned char *buf, int len, int verb, int debug) {
	mkdata *d;
	if ((d = (mkdata *)calloc(1, sizeof(mkdata))) == NULL)
		error("mkdata: malloc failed!");

	d->p = p;

	d->verb = verb;
	d->debug = debug;

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
