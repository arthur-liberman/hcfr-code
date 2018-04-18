
/* 
 * Argyll Color Correction System
 *
 * Image Engineering EX1 related functions
 *
 * Author: Graeme W. Gill
 * Date:   4/4/2015
 *
 * Copyright 1996 - 2015, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Based on specbos.c
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

/*

	TTBD:

*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#else	/* !SALONEINSTLIB */
#include "sa_config.h"
#include "numsup.h"
#endif /* !SALONEINSTLIB */
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "inst.h"
#include "rspec.h"
#include "ex1.h"

#define ENABLE_NONVCAL	/* [Def] Enable saving calibration state between program runs in a file */

#undef DEBUG
#undef PLOT_DEBUG		/* Use plot to show readings & processing */
#undef TEST_DARK_INTERP	/* Test adaptive dark interpolation */

#define EX1_MAX_MEAS_TIME 1.0	/* Maximum measurement time to use */
 
/* ----------------------------------------------------------------- */
/* High level EX1 commands, implemented at the bottom of the file */

#define EX1_EP 0x1				/* End point to use. Can use 1 and/or 2 */
#define MIN_CYCLE_TIME 0.009	/* Minimum time per measurement cycle */

#define SENSE_AIM (0.85 * ((1 << 14)-1))		/* Sensor level aim point */
#define SENSE_SAT (0.95 * ((1 << 14)-1))		/* Saturation level */ 

#define DCALTOUT  (1 * 60 * 60) /* [1 Hour ??] Dark Calibration timeout in seconds */

static int icoms2ex1_err(int se);
static inst_code ex1_interp_code(inst *pp, int ec);
static char *ex1_interp_native_error(ex1 *p, int ec);

static void ex1_flush(ex1 *p);
static int ex1_get_hw_rev(ex1 *p, int *hwrev);
static int ex1_get_fw_rev(ex1 *p, int *fwrev);
static int ex1_get_slit_width(ex1 *p, int *swidth);
static int ex1_get_fiber_width(ex1 *p, int *fibw);
static int ex1_get_grating(ex1 *p, char **grating);
static int ex1_get_filter(ex1 *p, char **filter);
static int ex1_get_coating(ex1 *p, char **coating);
static int ex1_get_alias(ex1 *p, char **alias);
static int ex1_get_serno(ex1 *p, char **serno);
static int ex1_get_wl_coefs(ex1 *p, unsigned int *nocoefs, double **coefs);
static int ex1_get_nl_coefs(ex1 *p, unsigned int *nocoefs, double **coefs);
static int ex1_get_ir_coefs(ex1 *p, unsigned int *nocoefs, double **coefs, double *area);
static int ex1_get_sl_coefs(ex1 *p, unsigned int *nocoefs, double **coefs);

#define EX1_TRIGMODE_IMED	0x0		/* Immediate software trigger mode. */
#define EX1_TRIGMODE_TRIG	0x1		/* Trigger on ext trigger signal after possible delay. */
#define EX1_TRIGMODE_STROBE	0x2		/* Trigger afer possible delay on internal cont. strobe. */ 

static int ex1_set_trig_mode(ex1 *p, int trigmode);

#define EX1_INTTIME_MIN 10.0e-6
#define EX1_INTTIME_MAX 10.0 

static int ex1_set_inttime(ex1 *p, double *qinttime, double inttime);

#define EX1_DELTIME_MIN 5.0e-6
#define EX1_DELTIME_MAX 335.5e-3

static int ex1_set_trig_delay(ex1 *p, double *qtrigdel, double trigdel);

#define EX1_STRBPER_MIN 50.0e-6
#define EX1_STRBPER_MAX 5.0

static int ex1_set_strobe_period(ex1 *p, double *qstbper, double stbper);

#define EX1_STRB_DISABLE	0x0		/* Continuous strobe disabled */
#define EX1_STRB_ENABLE 	0x1		/* Continuous strobe enabled */

static int ex1_set_strobe_enable(ex1 *p, int enable);

static int ex1_set_single_strobe_enable(ex1 *p, int enable);

#define EX1_AVERAGE_MIN 1
#define EX1_AVERAGE_MAX 5000

static int ex1_set_average(ex1 *p, int noavg);

#define EX1_BIN_OFF 	0x0			/* Full res, 1024 bins */
#define EX1_BIN_2	    0x1			/* 1/2 res, 512 bins */
#define EX1_BIN_4	    0x2			/* 1/4 res, 256 bins */
#define EX1_BIN_8	    0x3			/* 1/8 res, 128 bins */

static int ex1_set_binning(ex1 *p, int bf);

#define EX1_BOXCAR_MIN 0
#define EX1_BOXCAR_MAX 15

static int ex1_set_boxcar(ex1 *p, int nobox);

static int ex1_measure(ex1 *p, double *raw);

static int ex1_save_calibration(ex1 *p);
static int ex1_restore_calibration(ex1 *p);
static int ex1_touch_calibration(ex1 *p);

/* ----------------------------------------------------------------- */

/* Establish communications with a ex1 */
/* Return EX1_COMS_FAIL on failure to establish communications */
static inst_code
ex1_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	ex1 *p = (ex1 *) pp;
	instType dtype = pp->dtype;
	int se;

	inst_code ev = inst_ok;

	a1logd(p->log, 2, "ex1_init_coms: called\n");

	if (p->icom->port_type(p->icom) != icomt_usb) {
		a1logd(p->log, 1, "ex1_init_coms: wrong communications type for device!\n");
		return inst_coms_fail;
	}
	
	/* Set config, interface, write end point, read end point, read quanta */
	if ((se = p->icom->set_usb_port(p->icom, 1, EX1_EP, EX1_EP | 0x80, icomuf_none, 0, NULL)) != ICOM_OK) { 
		a1logd(p->log, 1, "ex1_init_coms: set_usbe_port failed ICOM err 0x%x\n",se);
		return ex1_interp_code((inst *)p, icoms2ex1_err(se));
	}

	/* Should we reset it ? */

	/* Make sure no message is waiting */
	ex1_flush(p);

	/* Check it is responding */
	if ((se = ex1_get_hw_rev(p, &p->hwrev)) != EX1_OK) {
		return ex1_interp_code((inst *)p, se);
	}

	p->gotcoms = 1;

	a1logd(p->log, 2, "ex1_init_coms: init coms has suceeded\n");

	return inst_ok;
}

/* Initialise the EX1 */
/* return non-zero on an error, with dtp error code */
static inst_code
ex1_init_inst(inst *pp) {
	ex1 *p = (ex1 *)pp;
	inst_code ev = inst_ok;
	rspec_inf *sconf = &p->sconf;
	unsigned int count, i;

	a1logd(p->log, 2, "ex1_init_inst: called\n");

	if (p->gotcoms == 0)
		return inst_internal_error;		/* Must establish coms before calling init */
	
	p->lo_secs = 2000000000;		/* A very long time */
	p->max_meastime = EX1_MAX_MEAS_TIME;

	/* Get information about the instrument */
	if ((ev = ex1_get_alias(p, &p->alias)) != EX1_OK)
		return ex1_interp_code((inst *)p, ev);
	if ((ev = ex1_get_hw_rev(p, &p->hwrev)) != EX1_OK)
		return ex1_interp_code((inst *)p, ev);
	if ((ev = ex1_get_fw_rev(p, &p->fwrev)) != EX1_OK)
		return ex1_interp_code((inst *)p, ev);
	if ((ev = ex1_get_serno(p, &p->serno)) != EX1_OK)
		return ex1_interp_code((inst *)p, ev);
	if ((ev = ex1_get_slit_width(p, &p->slitw)) != EX1_OK)
		return ex1_interp_code((inst *)p, ev);
	if ((ev = ex1_get_fiber_width(p, &p->fiberw)) != EX1_OK)
		return ex1_interp_code((inst *)p, ev);
	if ((ev = ex1_get_grating(p, &p->grating)) != EX1_OK)
		return ex1_interp_code((inst *)p, ev);
	if ((ev = ex1_get_filter(p, &p->filter)) != EX1_OK)
		return ex1_interp_code((inst *)p, ev);
	if ((ev = ex1_get_coating(p, &p->coating)) != EX1_OK)
		return ex1_interp_code((inst *)p, ev);

	/* Set the instrument to sane defaults: */

	/* Set trigger mode to software immediate */
	if ((ev = ex1_set_trig_mode(p, EX1_TRIGMODE_IMED)) != EX1_OK) {
		return ex1_interp_code((inst *)p, ev);
	}

	/* Set integration time to minimum */
	if ((ev = ex1_set_inttime(p, &p->inttime, EX1_INTTIME_MIN)) != EX1_OK) {
		return ex1_interp_code((inst *)p, ev);
	}

	/* Set trigger delay time to minumum */
	if ((ev = ex1_set_trig_delay(p, NULL, EX1_DELTIME_MIN)) != EX1_OK) {
		return ex1_interp_code((inst *)p, ev);
	}

	/* Disable continuous strobe */
	if ((ev = ex1_set_strobe_enable(p, EX1_STRB_DISABLE)) != EX1_OK) {
		return ex1_interp_code((inst *)p, ev);
	}

	/* Disable single strobe */
	if ((ev = ex1_set_single_strobe_enable(p, EX1_STRB_DISABLE)) != EX1_OK) {
		return ex1_interp_code((inst *)p, ev);
	}

	/* Set averages to 1 */
	if ((ev = ex1_set_average(p, EX1_AVERAGE_MIN)) != EX1_OK) {
		return ex1_interp_code((inst *)p, ev);
	}

	/* Turn off binning */
	if ((ev = ex1_set_binning(p, EX1_BIN_OFF)) != EX1_OK) {
		return ex1_interp_code((inst *)p, ev);
	}

	/* Turn off boxcar filtering */
	if ((ev = ex1_set_boxcar(p, EX1_BOXCAR_MIN)) != EX1_OK) {
		return ex1_interp_code((inst *)p, ev);
	}

	/* Setup calibration information and measurement config. */
	clear_rspec_inf(sconf);
	sconf->log = p->log;
	sconf->nsen = 1024;
	sconf->nshgrps = 0;
	sconf->nilltkgrps = 0;
	sconf->lightrange.off = 0;				/* All of sensor value go to raw */
	sconf->lightrange.num = sconf->nsen;
	sconf->nraw = sconf->lightrange.num;

	if ((ev = ex1_get_wl_coefs(p, &sconf->nwlcal, &sconf->wlcal)) != EX1_OK) {
		return ex1_interp_code((inst *)p, ev);
	}
	if ((ev = ex1_get_nl_coefs(p, &sconf->nlin, &sconf->lin)) != EX1_OK) {
		return ex1_interp_code((inst *)p, ev);
	}
	sconf->lindiv = 1;		/* Divide type polynomial correction */

	/* If we every figure out what the format is, convert it */
	/* into 2D matrix form and store into sconf. */
	if ((ev = ex1_get_sl_coefs(p, &p->nstraylight, &p->straylight)) != EX1_OK) {
		return ex1_interp_code((inst *)p, ev);
	}

	/* Get raw calibration values */
	if ((ev = ex1_get_ir_coefs(p, &count, &sconf->ecal, &p->emis_area))  != EX1_OK) {
		return ex1_interp_code((inst *)p, ev);
	}
	if (count != sconf->nraw) {
		a1logd(p->log, 1, " Calibration array is unexpected length (is %d, should be %d)\n",
		                                                                count, sconf->nraw);
		return inst_wrong_setup;
	}
#ifdef PLOT_DEBUG
	printf("emission cal:\n");
	plot_ecal(sconf);
#endif

	/* Hmm. not 100% sure this is correct, but it gives the right */
	/* sort of numbers.. */
	for (i = 0; i < sconf->nraw; i++)
		sconf->ecal[i] /= p->emis_area; 

	sconf->ecaltype = rspec_raw;

#ifdef PLOT_DEBUG
	printf("adjusted emission cal:\n");
	plot_ecal(sconf);
#endif

	/* Figure out the calibrated range */
	rspec_comp_raw_range_from_ecal(sconf);

//printf("~1 raw range = %d - %d\n",sconf->rawrange.off, sconf->rawrange.off + sconf->rawrange.num-1);
//printf("~1 = %f - %f nm\n",rspec_raw2nm(sconf, sconf->rawrange.off), rspec_raw2nm(sconf, sconf->rawrange.off + sconf->rawrange.num-1));

	sconf->ktype = rspec_gausian;			/* Default */
//	sconf->ktype = rspec_lanczos2;
//	sconf->ktype = rspec_lanczos3;
//	sconf->ktype = rspec_triangle;
//	sconf->ktype = rspec_cubicspline;
	sconf->wl_space = 2.0;
	sconf->wl_short = rspec_raw2nm(sconf, sconf->rawrange.off);
	sconf->wl_short = sconf->wl_space * ceil(sconf->wl_short/sconf->wl_space);
	if (sconf->wl_short < 350.0)
		sconf->wl_short = 350.0;
	sconf->wl_long = rspec_raw2nm(sconf, sconf->rawrange.off + sconf->rawrange.num-1);
	sconf->wl_long = sconf->wl_space * floor(sconf->wl_long/sconf->wl_space);
	if (sconf->wl_long > 800.0)
		sconf->wl_long = 800.0;

	sconf->nwav = (int)(floor(sconf->wl_long - sconf->wl_short)/sconf->wl_space + 1);
	
	a1logd(p->log, 1, " %d Wavelengths %f - %f spacing %f\n",sconf->nwav,
		                  sconf->wl_short,sconf->wl_long,sconf->wl_space);
	
	/* The EX1 is Vis range 350-800 nm */
	/* so the raw spacing is about 0.45 nm */
	/* The EX1 has a 100um slit, so FWHM will be 6nm */
	/* so aim for calibrated wavlength spacing of 2nm */

	rspec_make_resample_filters(sconf);
#ifdef PLOT_DEBUG
	plot_resample_filters(sconf);
#endif

	p->idark_int_time[0] = EX1_INTTIME_MIN;
	p->idark_int_time[1] = 2.0;

#ifdef ENABLE_NONVCAL
	/* Restore idarl calibration from the local system */
	ex1_restore_calibration(p);
	/* Touch it so that we know when the instrument was last opened */
	ex1_touch_calibration(p);
#endif

#ifdef NEVER	/* Test linearity iversion */
	{
		double v1, v2, v3;

		for (v1 = 0.0; v1 < SENSE_SAT; v1 += 1638.0) {
			v2 = linearize_val_rspec(sconf, v1);
			v3 = inv_linearize_val_rspec(sconf, v2);
	
			printf("Sens in %f -> lin %f -> non-lin %f (%f)\n",v1,v2,v3,fabs(v1 - v3));
		}
	}
#endif /* NEVER */

	p->conv = new_xsp2cie(icxIT_none, 0.0, NULL, icxOT_CIE_1931_2, NULL, icSigXYZData, icxNoClamp);

	if (p->conv == NULL)
		return EX1_INT_CIECONVFAIL;

	if (p->log->verb) {
		a1logv(p->log, 1, " Model:             %s\n",p->alias != NULL ? p->alias : "Unknown");
		a1logv(p->log, 1, " HW rev:            %d\n",p->hwrev);
		a1logv(p->log, 1, " FW rev:            %d\n",p->fwrev);
		a1logv(p->log, 1, " Serial number:     %s\n",p->serno);
		if (p->slitw != 0)
			a1logv(p->log, 1, " Slit width:        %d microns\n",p->slitw);
		else
			a1logv(p->log, 1, " Slit width:        Unknown\n");
		if (p->fiberw != 0)
			a1logv(p->log, 1, " Fiber width:       %d microns\n",p->fiberw);
		else
			a1logv(p->log, 1, " Fiber width:       Unknown\n");
		a1logv(p->log, 1, " Grating:           %s\n",p->grating != NULL ? p->grating : "Unknown");
		a1logv(p->log, 1, " Filter:            %s\n",p->filter != NULL ? p->filter : "Unknown");
		a1logv(p->log, 1, " Coating:           %s\n",p->coating != NULL ? p->coating : "Unknown");
	}

	p->inited = 1;
	a1logd(p->log, 2, "ex1_init_inst: instrument inited OK\n");

	return inst_ok;
}

/* Do a raw measurement. */
/* Will delete existing *praw */
/* return EX1 error */
static int ex1_do_meas(ex1 *p, rspec **praw, double *inttime, double duration) {
	rspec *sens, *raw;
	int notoav;
	int ec;

	notoav = (int)ceil(duration/(MIN_CYCLE_TIME + *inttime));
	if (notoav <= 0)
		notoav = 1;
	if (notoav > EX1_AVERAGE_MAX)
		notoav = EX1_AVERAGE_MAX; 
	
	/* Set integration time */
	if ((ec = ex1_set_inttime(p, &p->inttime, *inttime)) != EX1_OK) {
		return ec;
	}
	*inttime = p->inttime;		/* Quantized integration time */

	/* Set no. of averages */
	if ((ec = ex1_set_average(p, notoav)) != EX1_OK) {
		return ec;
	}

	sens = new_rspec(&p->sconf, rspec_sensor, 1);

	if ((ec = ex1_measure(p, sens->samp[0])) != EX1_OK) {
		del_rspec(sens);
		return ec;
	}

	sens->mtype = inst_mrt_emission;
	sens->inttime = p->inttime;

	/* + Any other processing from sens to raw, */
	/* i.e. shielded cell tracking, illuminant temp value etc. ? */

	raw = extract_raw_from_sensor_rspec(sens);
	del_rspec(sens);

	if (*praw != NULL)
		del_rspec(*praw);
	*praw = raw;

	return EX1_OK;
}

/* Read a single sample */
/* Return the EX1 error code */
static inst_code
ex1_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp	/* NZ if clamp XYZ/Lab to be +ve */
) {
	ex1 *p = (ex1 *)pp;
	int user_trig = 0;
	int pos = -1;
	inst_code rv = inst_protocol_error;
	rspec_inf *sconf = &p->sconf; 
	rspec *raw = NULL, *wav = NULL;
	int ec, i; 

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;


	if (p->trig == inst_opt_trig_user) {

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "ex1: inst_opt_trig_user but no uicallback function set!\n");
			return inst_unsupported;
		}

		for (;;) {
			if ((rv = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
				if (rv == inst_user_abort) {
					return rv;				/* Abort */
				}
				if (rv == inst_user_trig) {
					user_trig = 1;
					break;					/* Trigger */
				}
			}
			msec_sleep(200);
		}
		/* Notify of trigger */
		if (p->uicallback)
			p->uicallback(p->uic_cntx, inst_triggered); 

	/* Progromatic Trigger */
	} else {
		/* Check for abort */
		if (p->uicallback != NULL
		 && (rv = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort) {
			return rv;				/* Abort */
		}
	}
	{
		double inttime, tinttime;
		int six;
		double max = 1e6;
		double blk;

		a1logd(p->log,5,"Starting auto integration time:\n");

		/* Find an integration time small enough to avoid overload */
		for (inttime = 0.01; max > SENSE_SAT && inttime >= EX1_INTTIME_MIN; inttime *= 0.1) {
			a1logd(p->log,5,"Trying inttime %f\n",inttime);
			if ((ec = ex1_do_meas(p, &raw, &inttime, 0.05)) != EX1_OK) {
				return ex1_interp_code((inst *)p, ec);
			}
			max = largest_val_rspec(NULL, &six, raw);
			a1logd(p->log,5,"Max %.0f at six %d, Sat level %.0f\n",max,six,SENSE_SAT);
			if (max <= SENSE_SAT)
				break;
		}
		if (max >= SENSE_SAT) {
			a1logd(p->log,1,"Saturated\n");
			return ex1_interp_code((inst *)p, EX1_RD_SENSORSATURATED);
		}

		blk = ex1_interp_idark_val(sconf, 0, six, inttime);
		a1logd(p->log,5,"Black at max = %f\n",blk);

		/* Find an integration time large enough to measure something significant */
		for (; max < (2.0 * blk)
			   && inttime <= p->max_meastime
			   && inttime <= EX1_INTTIME_MAX;) {
			tinttime = inttime * 10.0;
			if (tinttime > p->max_meastime
			 || tinttime > EX1_INTTIME_MAX)
				break;
			inttime = tinttime;
			a1logd(p->log,5,"Trying intttime %f\n",inttime);
			if ((ec = ex1_do_meas(p, &raw, &inttime, 0.05)) != EX1_OK) {
				return ex1_interp_code((inst *)p, ec);
			}
			max = largest_val_rspec(NULL, &six, raw);
			blk = ex1_interp_idark_val(sconf, 0, six, inttime);
			a1logd(p->log,5,"Max %.0f, black %.0f\n",max,blk);
			if (max >= (2.0 * blk))
				break;
		}

		/* If we haven't already tried the maximum time, calculate optimal inttime */
		if (inttime <= p->max_meastime
		 && max >= (2.0 * blk)) {
			double llev, blk2;

			/* Compute the light level for our measurement */
			llev = linearize_val_rspec(sconf, max - blk)/inttime;
			a1logd(p->log,5,"Light level = %f\n",llev);

			/* Calculate an initial target integration time to use to estimat black */
			tinttime = (SENSE_AIM - blk)/(max - blk) * inttime; 
			a1logd(p->log,5,"Initial optimal inttime %f\n",tinttime);

			/* Itterate */
			for (i = 0; i < 5; i++) {
				blk2 = ex1_interp_idark_val(sconf, 0, six, tinttime);
				tinttime = linearize_val_rspec(sconf, SENSE_AIM - blk2)/llev;
			}
			a1logd(p->log,5,"Optimal inttime %f\n",tinttime);

			if (tinttime < p->max_meastime) {
				if (tinttime < EX1_INTTIME_MIN)
					tinttime = EX1_INTTIME_MIN;
				if (tinttime > EX1_INTTIME_MAX)
					tinttime = EX1_INTTIME_MAX;
	
				/* Do the real measurement */
				if (inttime <= 1.0) {
					inttime = tinttime;
	
					a1logd(p->log,5,"Doing real measurement with inttime %f\n",inttime);
					if ((ec = ex1_do_meas(p, &raw, &inttime, 1.0)) != EX1_OK) {
						return ex1_interp_code((inst *)p, ec);
					}
					max = largest_val_rspec(NULL, &six, raw);
					a1logd(p->log,5,"Got max %.0f, aimed for %.0f\n",max,SENSE_AIM);
				}
			}
		}

		if (max >= SENSE_SAT) {
			/* Hmm. Should retry in case light level changed during measurement ? */
			return ex1_interp_code((inst *)p, EX1_RD_SENSORSATURATED);
		}
	}



#ifdef PLOT_DEBUG
	printf("Raw:\n");
	plot_rspec1(raw);
#endif

	subtract_idark_rspec(raw);

#ifdef PLOT_DEBUG
	{
		rspec *nl = new_rspec_clone(raw);
		linearize_rspec(raw);
		printf("non-lin and linearized:\n");
		plot_rspec2(nl, raw);
		del_rspec(nl);
	}
#else
	linearize_rspec(raw);
#endif

	emis_calibrate_rspec(raw);

#ifdef PLOT_DEBUG
	printf("Calibrated raw:\n");
	plot_rspec1(raw);
#endif

	wav = convert_wav_from_raw_rspec(raw);
	del_rspec(raw);

	inttime_calibrate_rspec(wav);

#ifdef PLOT_DEBUG
	printf("Wav:\n");
	plot_rspec1(wav);
#endif

	val->mtype = wav->mtype;

	/* Copy spectral in */
	val->sp.spec_n = sconf->nwav;
	val->sp.spec_wl_short = sconf->wl_short;
	val->sp.spec_wl_long  = sconf->wl_long;
	val->sp.norm = 1.0;		/* Spectral data is in W/nm/m^2 */

	for (i = 0; i < val->sp.spec_n; i++) {
		val->sp.spec[i] = 1000.0 * wav->samp[0][i];
	}

	del_rspec(wav);

	/* Set the XYZ */
	p->conv->convert(p->conv, val->XYZ, &val->sp);
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);
	val->XYZ_v = 1;



	if (user_trig)
		return inst_user_trig;
	return rv;
}

/* Return needed and available inst_cal_type's */
static inst_code ex1_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	ex1 *p = (ex1 *)pp;
	time_t curtime = time(NULL);
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
	int idark_valid = p->idark_valid;
		
	if ((curtime - p->iddate) > DCALTOUT) {
		a1logd(p->log,2,"Invalidating adaptive dark cal as %d secs from last cal\n",curtime - p->iddate);
		idark_valid = 0;
	}

	if (!idark_valid
	 || (p->want_dcalib && !p->noinitcalib))
		n_cals |= inst_calt_em_dark;
	a_cals |= inst_calt_em_dark;

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	a1logd(p->log,3,"ex1: returning n_cals 0x%x, a_cals 0x%x\n",n_cals, a_cals);

	return inst_ok;
}

/* Request an instrument calibration. */
inst_code ex1_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
inst_calc_id_type *idtype,	/* Condition identifier type */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	ex1 *p = (ex1 *)pp;
	rspec_inf *sconf = &p->sconf;
    inst_cal_type needed, available;
	inst_code ev;
	int ec;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if ((ev = ex1_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
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

		a1logd(p->log,4,"ex1_calibrate: doing calt 0x%x\n",calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return inst_ok;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return inst_unsupported;
	}

	/* Adaptive black calibration: */
	if (*calt & inst_calt_em_dark) {
		time_t cdate = time(NULL);
		int i, j, k;
		rspec *raw;

		if ((*calc & inst_calc_cond_mask) != inst_calc_man_em_dark) {
			*calc = inst_calc_man_em_dark;
			return inst_cal_setup;
		}

		a1logd(p->log,2,"\nDoing emis adapative black calibration\n");

		/* Bracket the dark level and interpolate. */

		if ((ec = ex1_do_meas(p, &sconf->idark[0], &p->idark_int_time[0], 1.0)) != EX1_OK) {
			return ex1_interp_code((inst *)p, ec);
		}

		if ((ec = ex1_do_meas(p, &sconf->idark[1], &p->idark_int_time[1], 1.0)) != EX1_OK) {
			return ex1_interp_code((inst *)p, ec);
		}

		p->idark_valid = 1;
		p->want_dcalib = 0;
		p->iddate = cdate;
		*calt &= ~inst_calt_em_dark;
	
#ifdef PLOT_DEBUG		/* Use plot to show readings & processing */
		printf("idark calib:\n");
		plot_rspec2(sconf->idark[0], sconf->idark[1]);
#endif

		/* Test accuracy of dark level interpolation */
#ifdef TEST_DARK_INTERP
		{
			double tinttime;
			rspec *ref, *interp;
			
			for (tinttime = EX1_INTTIME_MIN; ; tinttime *= 2.0) {
				if (tinttime >= EX1_INTTIME_MAX)
					tinttime = EX1_INTTIME_MAX;

				if ((ec = ex1_do_meas(p, &ref, &tinttime, 1.0)) != EX1_OK)
					return ex1_interp_code((inst *)p, ec);

				interp = ex1_interp_idark(sconf, tinttime);

				fprintf(stderr,"Low gain ref vs. interp dark offset for inttime %f:\n",tinttime);
				plot_rspec2(ref, interp);

				del_rspec(interp);
				del_rspec(ref);

				if ((tinttime * 1.1) > EX1_INTTIME_MAX)
					break;
			}
		}
#endif	/* NEVER */

#ifdef ENABLE_NONVCAL
		/* Save the idark calibration to a file */
		ex1_save_calibration(p);
#endif
	}

	return inst_ok;
}

/* Error codes interpretation */
static char *
ex1_interp_error(inst *pp, int ec) {
	char *rv;
	ex1 *p = (ex1 *)pp;

	ec &= inst_imask;

	/* See if it's an native error code */
	if ((rv = ex1_interp_native_error(p, ec)) != NULL)
		return rv;

	switch (ec) {
		case EX1_TIMEOUT:
			return "Communications timeout";
		case EX1_COMS_FAIL:
			return "Communications failure";
		case EX1_UNKNOWN_MODEL:
			return "Not an EX1";
		case EX1_SHORT_WRITE:
			return "Short USB write";
		case EX1_SHORT_READ:
			return "Short USB read";
		case EX1_LONG_READ:
			return "Long USB read";
		case EX1_DATA_CHSUM_ERROR:
			return "Data checksum error";
		case EX1_DATA_PARSE_ERROR:
			return "Data from ex1 didn't parse as expected";

		case EX1_RD_SENSORSATURATED:
			return "Sensor is saturated";


		/* Internal software errors */
		case EX1_INTERNAL_ERROR:
			return "Internal software error";
		case EX1_NOT_IMP:
			return "Not implemented";
		case EX1_MEMORY:
			return "Memory allocation failed";
		case EX1_INT_THREADFAILED:
			return "Thread failed";
		case EX1_INTTIME_RANGE:
			return "Integration time is out of range";
		case EX1_DELTIME_RANGE:
			return "Trigger delat time is out of range";
		case EX1_STROBE_RANGE:
			return "Multi strobe period time is out of range";
		case EX1_AVERAGE_RANGE:
			return "Number to average is out of range";
		case EX1_BOXCAR_RANGE:
			return "Boxcar filtering is out of range";
		case EX1_INT_CAL_SAVE:
			return "Saving calibration file failed";
		case EX1_INT_CAL_RESTORE:
			return "Restoring calibration file failed";
		case EX1_INT_CAL_TOUCH:
			return "Touching calibration file failed";
		case EX1_INT_CIECONVFAIL:
			return "Creating spectral to XYZ conversion failed";

		/* Hardware errors */
		case EX1_NO_WL_CAL:
			return "Instrument doesn't contain wavelength calibration";
		case EX1_NO_IR_CAL:
			return "Instrument doesn't contain irradiance calibration";

		default:
			return "Unknown error code";
	}
}

/* EX1 Native error codes interpretation */
/* Return NULL if not recognised */
static char *
ex1_interp_native_error(ex1 *p, int ec) {
	switch (ec) {
		case EX1_OK:
			return "No device error";
		case EX1_UNSUP_PROTOCOL:
			return "Invalid/unsupported protocol";
		case EX1_MES_UNKN:
			return "Unknown message type";
		case EX1_MES_BAD_CHSUM:
			return "Bad message checksum";
		case EX1_MES_TOO_LARGE:
			return "Message is too large";
		case EX1_MES_PAYLD_LEN:
			return "Payload length doesn't match message type";
		case EX1_MES_PAYLD_INV:
			return "Payload data is invalid";
		case EX1_DEV_NOT_RDY:
			return "Device not ready for message type";
		case EX1_MES_UNK_CHSUM:
			return "Unknown checksum type";
		case EX1_DEV_UNX_RST:
			return "Unexpected device reset";
		case EX1_TOO_MANY_BUSSES:
			return "Too many command sources";
		case EX1_OUT_OF_MEM:
			return "Device is out of memory";
		case EX1_NO_INF:
			return "Information doesn't exist";
		case EX1_DEV_INT_ERR:
			return "Device internal error";
		case EX1_DECRYPT_ERR:
			return "Could not decrypt";
		case EX1_FIRM_LAYOUT:
			return "Firmware layout is invalid";
		case EX1_PACKET_SIZE:
			return "Data packet size is not 64 bytes";
		case EX1_HW_REV_INCPT:
			return "HW rev. is incompatible with firmware";
		case EX1_FLASH_MAP:
			return "Flash map is incompatible with firmware";
		case EX1_DEFERRED:
			return "Operation/Response deffered";
		default:
			return NULL;
	}
}

/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
ex1_interp_code(inst *pp, int ec) {

	ec &= inst_imask;
	switch (ec) {

		case EX1_OK:
			return inst_ok;

		case EX1_INTERNAL_ERROR:
		case EX1_MEMORY:
		case EX1_INT_THREADFAILED:
		case EX1_INTTIME_RANGE:
		case EX1_DELTIME_RANGE:
		case EX1_STROBE_RANGE:
		case EX1_AVERAGE_RANGE:
		case EX1_BOXCAR_RANGE:
		case EX1_INT_CAL_SAVE:
		case EX1_INT_CAL_RESTORE:
		case EX1_INT_CAL_TOUCH:
		case EX1_INT_CIECONVFAIL:
			return inst_internal_error | ec;

		case EX1_TIMEOUT:
		case EX1_COMS_FAIL:
		case EX1_SHORT_WRITE:
		case EX1_SHORT_READ:
		case EX1_LONG_READ:
			return inst_coms_fail | ec;

		case EX1_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case EX1_DATA_CHSUM_ERROR:
		case EX1_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

//			return inst_bad_parameter | ec;
//			return inst_wrong_config | ec;

		case EX1_NO_WL_CAL:
		case EX1_NO_IR_CAL:

		/* Native instrument error */
		case EX1_UNSUP_PROTOCOL:
		case EX1_MES_UNKN:
		case EX1_MES_BAD_CHSUM:
		case EX1_MES_TOO_LARGE:
		case EX1_MES_PAYLD_LEN:
		case EX1_MES_PAYLD_INV:
		case EX1_DEV_NOT_RDY:
		case EX1_MES_UNK_CHSUM:
		case EX1_DEV_UNX_RST:
		case EX1_TOO_MANY_BUSSES:
		case EX1_OUT_OF_MEM:
		case EX1_NO_INF:
		case EX1_DEV_INT_ERR:
		case EX1_DECRYPT_ERR:
		case EX1_FIRM_LAYOUT:
		case EX1_PACKET_SIZE:
		case EX1_HW_REV_INCPT:
		case EX1_FLASH_MAP:
		case EX1_DEFERRED:
			return inst_hardware_fail | ec;

		case EX1_RD_SENSORSATURATED:
			return inst_misread | ec;

		case EX1_NOT_IMP:
			return inst_unsupported | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
ex1_del(inst *pp) {
	if (pp != NULL) {
		ex1 *p = (ex1 *)pp;

#ifdef ENABLE_NONVCAL
		ex1_touch_calibration(p);
#endif

		if (p->icom != NULL)
			p->icom->del(p->icom);
		if (p->cbuf != NULL)
			free(p->cbuf);
		if (p->alias != NULL)
			free(p->alias);
		if (p->serno != NULL)
			free(p->serno);
		if (p->grating != NULL)
			free(p->grating);
		if (p->filter != NULL)
			free(p->filter);
		if (p->coating != NULL)
			free(p->coating);

		free_rspec_inf(&p->sconf);

		if (p->straylight != NULL)
			free(p->straylight);
		if (p->emis_coef != NULL)
			free(p->emis_coef);

		if (p->conv != NULL)
			p->conv->del(p->conv);

		p->vdel(pp);
		free(p);
	}
}

/* Return the instrument mode capabilities */
static void ex1_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	ex1 *p = (ex1 *)pp;
	inst_mode cap1 = 0;
	inst2_capability cap2 = 0;

	cap1 |= inst_mode_emis_tele
	     |  inst_mode_colorimeter
	     |  inst_mode_spectral
	        ;

	cap2 |= inst2_prog_trig
	     |  inst2_user_trig
	        ;

	if (pcap1 != NULL)
		*pcap1 = cap1;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = inst3_none;
}

/* Return current or given configuration available measurement modes. */
/* NOTE that conf_ix values shoudn't be changed, as it is used as a persistent key */
static inst_code ex1_meas_config(
inst *pp,
inst_mode *mmodes,
inst_cal_cond *cconds,
int *conf_ix
) {
	ex1 *p = (ex1 *)pp;
	inst_code ev;
	inst_mode mval = 0;

	if (mmodes != NULL)
		*mmodes = inst_mode_none;
	if (cconds != NULL)
		*cconds = inst_calc_unknown;

	/* Add the extra dependent and independent modes */
	mval |= inst_mode_emis_tele
	     |  inst_mode_colorimeter
	     |  inst_mode_spectral;

	if (mmodes != NULL)	
		*mmodes = mval;

	/* Return configuration index returned */
	if (conf_ix != NULL)
		*conf_ix = 0;		/* There is only one */

	return inst_ok;
}

/* Check device measurement mode */
static inst_code ex1_check_mode(inst *pp, inst_mode m) {
	inst_mode cap;

	if (!pp->gotcoms)
		return inst_no_coms;
	if (!pp->inited)
		return inst_no_init;

	pp->capabilities(pp, &cap, NULL, NULL);

	/* Simple test */
	if (m & ~cap)
		return inst_unsupported;

	/* Only tele emission mode supported */
	if (!IMODETST(m, inst_mode_emis_tele)) {
		return inst_unsupported;
	}

	return inst_ok;
}

/* Set device measurement mode */
static inst_code ex1_set_mode(inst *pp, inst_mode m) {
	ex1 *p = (ex1 *)pp;
	int refrmode;
	inst_code ev;

	if ((ev = ex1_check_mode(pp, m)) != inst_ok)
		return ev;

	p->mode = m;

	return inst_ok;
}

/* Set the noinitcalib mode */
static void ex1_set_noinitcalib(ex1 *p, int v, int losecs) {

	/* Ignore disabling init calib if more than losecs since instrument was open */
	if (v && losecs != 0 && p->lo_secs >= losecs) {
		a1logd(p->log,3,"initcalib disable ignored because %d >= %d secs\n",p->lo_secs,losecs);
		return;
	}
	p->noinitcalib = v;
}

/* 
 * set or reset an optional mode
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 */
static inst_code
ex1_get_set_opt(inst *pp, inst_opt_type m, ...) {
	ex1 *p = (ex1 *)pp;
	inst_code ev = inst_ok;

	a1logd(p->log, 5, "ex1_get_set_opt: opt type 0x%x\n",m);

	if (m == inst_opt_initcalib) {		/* default */
		ex1_set_noinitcalib(p, 0, 0);
		return inst_ok;

	} else if (m == inst_opt_noinitcalib) {
		va_list args;
		int losecs = 0;

		va_start(args, m);
		losecs = va_arg(args, int);
		va_end(args);

		ex1_set_noinitcalib(p, 1, losecs);
		return inst_ok;

	/* Record the trigger mode */
	} else if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_user) {
		p->trig = m;
		return inst_ok;
	}

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Use default implementation of other inst_opt_type's */
	{
		inst_code rv;
		va_list args;

		va_start(args, m);
		rv = inst_get_set_opt_def(pp, m, args);
		va_end(args);

		return rv;
	}
}

/* Constructor */
extern ex1 *new_ex1(icoms *icom, instType dtype) {
	ex1 *p;
	if ((p = (ex1 *)calloc(sizeof(ex1),1)) == NULL) {
		a1loge(icom->log, 1, "new_ex1: malloc failed!\n");
		return NULL;
	}

	/* Allocate a default communication buffer */
	if ((p->cbuf = calloc(1, 64)) == NULL) {
		a1loge(icom->log, 1, "new_ex1: malloc failed!\n");
		free(p);
		return NULL;
	}
	p->cbufsz = 64;

	p->log = new_a1log_d(icom->log);

	p->init_coms         = ex1_init_coms;
	p->init_inst         = ex1_init_inst;
	p->capabilities      = ex1_capabilities;
	p->meas_config       = ex1_meas_config;
	p->check_mode        = ex1_check_mode;
	p->set_mode          = ex1_set_mode;
	p->get_set_opt       = ex1_get_set_opt;
	p->read_sample       = ex1_read_sample;
	p->get_n_a_cals      = ex1_get_n_a_cals;
	p->calibrate         = ex1_calibrate;
	p->interp_error      = ex1_interp_error;
	p->del               = ex1_del;

	p->icom = icom;
	p->dtype = dtype;

	p->want_dcalib = 1;		/* Always do an initial dark calibration */

	return p;
}

/* =============================================================================== */
/* Calibration info save/restore */

static int ex1_save_calibration(ex1 *p) {
	int ev = EX1_OK;
	int i;
	char fname[100];		/* Name */
	calf x;
	int argyllversion = ARGYLL_VERSION;
	int ss;

	snprintf(fname, 99, ".ex1_%s.cal", p->serno);

	if (calf_open(&x, p->log, fname, 1)) {
		x.ef = 2;
		goto done;
	}

	ss = sizeof(ex1);

	/* Some file identification */
	calf_wints(&x, &argyllversion, 1);
	calf_wints(&x, &ss, 1);
	calf_wstrz(&x, p->serno);

	/* Save the black calibration if it's valid */
	calf_wints(&x, &p->idark_valid, 1);
	calf_wtime_ts(&x, &p->iddate, 1);
	calf_wrspec(&x, p->sconf.idark[0]);
	calf_wrspec(&x, p->sconf.idark[1]);

	a1logd(p->log,3,"nbytes = %d, Checkum = 0x%x\n",x.nbytes,x.chsum);
	calf_wints(&x, (int *)(&x.chsum), 1);

	if (calf_done(&x))
		x.ef = 3;

  done:;
	if (x.ef != 0) {
		a1logd(p->log,2,"Writing calibration file failed with %d\n",x.ef);
		ev = EX1_INT_CAL_SAVE;
	} else {
		a1logd(p->log,2,"Writing calibration file succeeded\n");
	}

	return ev;
}

/* Restore the all modes calibration from the local system */
static int ex1_restore_calibration(ex1 *p) {
	int ev = EX1_OK;
	int i, j;
	char fname[100];		/* Name */
	calf x;
	int argyllversion;
	int ss, nbytes, chsum1, chsum2;
	char *serno = NULL;

	snprintf(fname, 99, ".ex1_%s.cal", p->serno);

	if (calf_open(&x, p->log, fname, 0)) {
		x.ef = 2;
		goto done;
	}

	/* Last modified time */
	p->lo_secs = x.lo_secs;

	/* Do a dumy read to check the checksum, then a real read */
	for (x.rd = 0; x.rd < 2; x.rd++) {
		calf_rewind(&x);

		/* Check the file identification */
		calf_rints2(&x, &argyllversion, 1);
		calf_rints2(&x, &ss, 1);
		calf_rstrz2(&x, &serno);

		if (x.ef != 0
		 || argyllversion != ARGYLL_VERSION
		 || ss != (sizeof(ex1))
		 || strcmp(serno, p->serno) != 0) {
			a1logd(p->log,2,"Identification didn't verify\n");
			if (x.ef == 0)
				x.ef = 4;
			goto done;
		}

		/* Read the black calibration if it's valid */
		calf_rints(&x, &p->idark_valid, 1);
		calf_rtime_ts(&x, &p->iddate, 1);
		calf_rrspec(&x, &p->sconf.idark[0], &p->sconf);
		calf_rrspec(&x, &p->sconf.idark[1], &p->sconf);

		/* Check the checksum */
		chsum1 = x.chsum;
		nbytes = x.nbytes;
		calf_rints2(&x, &chsum2, 1);
	
		if (x.ef != 0
		 || chsum1 != chsum2) {
			a1logd(p->log,2,"Checksum didn't verify, bytes %d, got 0x%x, expected 0x%x\n",nbytes,chsum1, chsum2);
			if (x.ef == 0)
				x.ef = 5;
			goto done;
		}
	}

	a1logd(p->log,5,"ex1_restore_calibration done\n");
 done:;

	free(serno);
	if (calf_done(&x))
		x.ef = 3;

	if (x.ef != 0) {
		a1logd(p->log,2,"Reading calibration file failed with %d\n",x.ef);
		ev = EX1_INT_CAL_RESTORE;
	}

	return ev;
}

static int ex1_touch_calibration(ex1 *p) {
	int ev = EX1_OK;
	char fname[100];		/* Name */
	int rv;

	snprintf(fname, 99, ".ex1_%s.cal", p->serno);

	if (calf_touch(p->log, fname)) {
		a1logd(p->log,2,"Touching calibration file time failed with\n");
		return EX1_INT_CAL_TOUCH;
	}

	return EX1_OK;
}

/* =============================================================================== */
/* EX1 lower level communications */

/* Interpret an icoms error into a EX1 error */
static int icoms2ex1_err(int se) {
	if (se != ICOM_OK) {
		if (se & ICOM_TO)
			return EX1_TIMEOUT; 
		return EX1_COMS_FAIL;
	}
	return EX1_OK;
}

/* Message format definitions */

#define EX1_FLAG_RESP	0x0001		/* Response to an earlier request */
#define EX1_FLAG_ACK	0x0002		/* Acknowldgement response */
#define EX1_FLAG_RQACK	0x0004		/* Request for acknowldgement */
#define EX1_FLAG_NACK	0x0008		/* Negative acknowldgement response */
#define EX1_FLAG_EXPTN	0x0010		/* Exception occured */
#define EX1_FLAG_PVDEP	0x0020		/* Protocol version is deprecated */

#define EX1_CHSUM_NONE	0x0			/* No checksum */
#define EX1_CHSUM_MD5	0x1			/* MD5 checksum */

#define EX1_TLC_MASK	0xFFF00000	/* Top-level category message type mask */
#define EX1_TLC_GENERAL	0x00000000	/* General device characteristics */
#define EX1_TLC_SPECTRO	0x00100000	/* Spectrometer feature */
#define EX1_TLC_GPIO   	0x00200000	/* GPIO feature */
#define EX1_TLC_STROBE	0x00300000	/* Strobe feature */
#define EX1_TLC_TEMP	0x00400000	/* Temperature feature */

static char *cmdtstring(unsigned int cmd);

/* Debug - dump a command packet at debug level deb1 */
static void dump_command(ex1 *p, ORD8 *buf, int len, int deblev) {
	unsigned int pver = 0;		/* Protocol version */
	unsigned int flags = 0;
	unsigned int merrno = 0;
	char *es;
	unsigned int mestype = 0;
	unsigned int tlc = 0;			/* Top level message category */
	unsigned int regarding = 0;
	unsigned int chstype = EX1_CHSUM_NONE;
	unsigned int imdatlen = 0;
	unsigned int bremain = 0;
	unsigned int pll = 0;			/* Explicit payload length */
	int fo;							/* Footer offset */

	if (deblev < p->log->debug)
		return;

	if (len < 44) {
		a1logd(p->log, 0, " Command packet too short (%d bytes)\n",len);
		return;
	}

	/* First comes the header */
	if (buf[0] != 0xC1 || buf[1] != 0xC0) {
		a1logd(p->log, 0, " Start bytes wrong (0x%02x, 0x%02x)\n",buf[0],buf[1]);
	}

	pver = read_ORD16_le(buf + 2);
	if (pver < 0x1000) {
		a1logd(p->log, 0, " Unknown protocol version (0x%x)\n",pver);
		return;
	}
	a1logd(p->log, 0, " Protocol version: 0x%x\n",pver);
	
	flags = read_ORD16_le(buf + 4);
	a1logd(p->log, 0, " Flags: 0x%x\n",flags);

	if (flags & EX1_FLAG_RESP)
		a1logd(p->log, 0, "   Response to an earlier request\n");
	if (flags & EX1_FLAG_ACK)
		a1logd(p->log, 0, "   Acknowldgement response\n");
	if (flags & EX1_FLAG_RQACK)
		a1logd(p->log, 0, "   Request for acknowldgement\n");
	if (flags & EX1_FLAG_NACK)
		a1logd(p->log, 0, "   Negative acknowldgement response\n");
	if (flags & EX1_FLAG_EXPTN)
		a1logd(p->log, 0, "   Exception occured\n");
	if (flags & EX1_FLAG_PVDEP)
		a1logd(p->log, 0, "   Protocol version is deprecated request\n");

	merrno = read_ORD16_le(buf + 6);
	a1logd(p->log, 0, " Error no.: 0x%x\n",merrno);
	if ((es = ex1_interp_native_error(p, merrno)) != NULL)
		a1logd(p->log, 0, "   '%s'\n",es);

	mestype = read_ORD32_le(buf + 8);
	a1logd(p->log, 0, " Mes. Type: 0x%x = %s\n",mestype, cmdtstring(mestype));
	tlc = mestype & EX1_TLC_MASK;
	switch(tlc) {
		case EX1_TLC_GENERAL:
			a1logd(p->log, 0, "   General device characteristics\n");
			break;
		case EX1_TLC_SPECTRO:
			a1logd(p->log, 0, "   Spectrometer feature\n");
			break;
		case EX1_TLC_GPIO:
			a1logd(p->log, 0, "   GPIO feature\n");
			break;
		case EX1_TLC_STROBE:
			a1logd(p->log, 0, "   Strobe feature\n");
			break;
		case EX1_TLC_TEMP:
			a1logd(p->log, 0, "   Temperature feature\n");
			break;
	}

	regarding = read_ORD32_le(buf + 12);
	a1logd(p->log, 0, " Regarding: 0x%x\n",regarding);

	chstype = read_ORD8(buf + 22);
	a1logd(p->log, 0, " checksum: 0x%x\n",chstype);
	if (chstype == EX1_CHSUM_NONE)
		a1logd(p->log, 0, "   none\n");
	else if (chstype == EX1_CHSUM_MD5)
		a1logd(p->log, 0, "   MD5\n");
	
	imdatlen = read_ORD8(buf + 23);
	a1logd(p->log, 0, " immediate data %d bytes%s\n",imdatlen, imdatlen > 16 ? " (illegal)" : " ");

	if (imdatlen > 0)
		a1logd(p->log, 0, "   0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x" 
		" 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		buf[24], buf[25], buf[26], buf[27], buf[28], buf[29], buf[30], buf[31], 
		buf[32], buf[33], buf[34], buf[35], buf[36], buf[37], buf[38], buf[39]);

	bremain = read_ORD32_le(buf + 40);
	a1logd(p->log, 0, " bytes remaining %d\n",bremain);
	
	if (bremain < 20) {
		a1logd(p->log, 0, "   - too small for chsum & footer\n");
		return;
	}

	if ((44 + bremain) > len) {
		a1logd(p->log, 0, "   - too large for for message size (%d available)\n",len - 44);
		return;
	}

	/* Now comes the payload */
	pll = bremain - 20;	/* Payload length */

	if (pll > 0) {
		adump_bytes(p->log, "   ", buf + 44, 0, pll);
	}

	/* Then the checksum */
	if (chstype == EX1_CHSUM_NONE) {
		a1logd(p->log, 0, " checksum not used\n");
	} else if (chstype == EX1_CHSUM_MD5) {
		icmMD5 *md5;

		if ((md5 = new_icmMD5()) == NULL) {
			a1logd(p->log, 0, " new_icmMD5 failed\n");
		} else {
			ORD8 chsum[16];
			int i;

			md5->add(md5, buf, 44 + pll);
			md5->get(md5, chsum);
			for (i = 0; i < 16; i++) {
				if (chsum[i] != buf[44+pll+i])
					break;
			}
			if (i < 16)
				a1logd(p->log, 0, " MD5 checksum error\n");
			else
				a1logd(p->log, 0, " MD5 checksum OK\n");
			md5->del(md5);
		}
	} else {
		a1logd(p->log, 0, " checksum not checked (unknown type)\n");
	}

	/* Finally the footer */
	fo = 44 + pll + 16;
	if (buf[fo] != 0xC5 || buf[fo + 1] != 0xC4 || buf[fo + 2] != 0xC3 || buf[fo + 3] != 0xC2) {
		a1logd(p->log, 0, " Footer error (0x%02x 0x%02x 0x%02x 0x%02x)\n",
		                            buf[fo],buf[fo+1],buf[fo+2],buf[fo+3]);
	}
}

/* Do a full command/response exchange with the ex1 */
/* (This level is not multi-thread safe) */
/* Return the ex1 error code. */
static int
ex1_command(
ex1 *p,
unsigned int cmd,	/* 32 bit command or query code */
ORD8 *in,			/* Input data */
int ilen,			/* data length */
ORD8 *out,			/* Output payload - if NULL, assume it's a command */
int olen,			/* Expected returned data length */
int *prlen,			/* Actual data returned. If NULL, expect exactly olen */
double to,			/* Timeout in seconds */
int nd				/* nz to disable debug messages */
) {
	int se, rv;
	ORD8 *buf;
	int bsize = 64;		/* Size needed */
	int rwbytes = 0;
	int stime;

	int slen = 64;		/* Send length */
	int toff;			/* Tail offset */

	int rlen = 64;		/* Receive length */
	unsigned int pver = 0;		/* Protocol version */
	unsigned int flags = 0;
	unsigned int merrno = 0;
	unsigned int mestype = 0;
	unsigned int tlc = 0;			/* Top level message category */
	unsigned int regarding = 0;
	unsigned int chstype = EX1_CHSUM_MD5;
	unsigned int imdatlen = 0;
	unsigned int bremain = 0;
	unsigned int pll = 0;			/* Explicit payload length */
	unsigned int ardlen = 0;		/* Actual read data length (payload) */
	int fo;							/* Footer offset */

	if (in == NULL)
		ilen = 0;

	if (out == NULL)
		olen = 0;

	a1logd(p->log,6,"ex1_command: 0x%x '%s' ilen %d olen %d\n", cmd, cmdtstring(cmd), ilen, olen);

	if (p->log->debug >= 7 && ilen > 0) {
		adump_bytes(p->log, "  ", in, 0, ilen);
	}

	stime = msec_time();

	if (ilen > 16) {	/* Too big for immediate data */
		slen += ilen;
	}

	if (out != NULL && olen > 16) {	/* Too big for immediate data */
		rlen += olen;
	}

	/* Make sure out buffer is big enough to send or revieve */
	bsize = slen > rlen ? slen : rlen;
	if (bsize > p->cbufsz) {
		if ((p->cbuf = realloc(p->cbuf, bsize)) == NULL) {
			rv = EX1_MEMORY;
			goto done;
		}
		p->cbufsz = bsize;
	}
	buf = p->cbuf;

	/* Header */
	buf[0] = 0xC1;
	buf[1] = 0xC0;

	/* Protocol version */
	write_ORD16_le(buf + 2, 0x1100);
	
	/* If it's a command, request an acknowledge */
	flags = 0;
	if (out == NULL) {
		flags |= EX1_FLAG_RQACK;
	}
	write_ORD16_le(buf + 4, flags);

	/* Error number */
	write_ORD16_le(buf + 6, 0);

	/* Command */
	write_ORD32_le(buf + 8, cmd);

	/* Regarding identifier (not used) */
	write_ORD32_le(buf + 12, 0);

	/* Checksum type 0 = none */
	write_ORD8(buf + 22, chstype);

	/* Reserved bytes */
	memset(buf + 16, 0, 6);
	
	/* Immediate data */
	if (ilen <= 16) {
		write_ORD8(buf + 23, ilen);
		if (ilen > 0)
			memcpy(buf + 24, in, ilen); 
		if (ilen < 16)
			memset(buf + 24 + ilen, 0, 16 - ilen);
	
		/* Bytes remaining = checksum + footer = 20 */
		write_ORD32_le(buf + 40, 20);
		toff = 44;
		pll = 0;

	/* or Payload */
	} else {
		write_ORD8(buf + 23, 0);
		write_ORD32_le(buf + 40, ilen + 20);
		if (ilen > 0)
			memcpy(buf + 44, in, ilen); 
		toff = 44 + ilen;
		pll = ilen;
	}

	/* Checksum bytes */

	if (chstype == EX1_CHSUM_MD5) {
		icmMD5 *md5;

		if ((md5 = new_icmMD5()) == NULL) {
			a1logd(p->log, 1, "new_icmMD5 failed\n");
			merrno = EX1_INTERNAL_ERROR;
		} else {
			ORD8 chsum[16];
			int i;

			md5->add(md5, buf, toff);
			md5->get(md5, chsum);
			for (i = 0; i < 16; i++)
				buf[44+pll+i] = chsum[i];
			md5->del(md5);
		}
	} else {
		memset(buf + toff, 0, 16);
	}

	/* Finally the footer */
	buf[toff + 16 + 0] = 0xC5;
    buf[toff + 16 + 1] = 0xC4;
    buf[toff + 16 + 2] = 0xC3;
    buf[toff + 16 + 3] = 0xC2;

	if (p->log->debug >= 8) {
		a1logd(p->log,1,"\nex1_command: SENDING:\n");
		dump_command(p, buf, slen, p->log->debug);
	}

	/* Send the command */
	se = p->icom->usb_write(p->icom, NULL, EX1_EP, buf, slen, &rwbytes, 1.0);

	if ((rv = icoms2ex1_err(se)) != EX1_OK) {
		a1logd(p->log,1,"ex1_command: send failed with ICOM err 0x%x\n",se);
		goto done;
	}

	if (rwbytes != slen) {
		a1logd(p->log,1,"ex1_command: send %d/%d bytes - short\n",rwbytes, slen);
		rv = EX1_SHORT_WRITE;
		goto done;
	}

	/* - - - - - - - - - - - - - - */

	/* Get the expected reply to a query, or an acknowledgement to a command */
	se = p->icom->usb_read(p->icom, NULL, EX1_EP | 0x80, buf, 64, &rwbytes, to); 

	if ((rv = icoms2ex1_err(se)) != EX1_OK) {
		a1logd(p->log,1,"ex1_command: read failed with ICOM err 0x%x\n",se);
		goto done;
	}

	if (p->log->debug >= 8) { 
		a1logd(p->log,1,"\nex1_command: RECIEVING:\n");
		dump_command(p, buf, rwbytes, p->log->debug);
	}

	if (rwbytes != 64) {
		a1logd(p->log,1,"ex1_command: read %d/%d bytes - short\n",rwbytes, 64);
		rv = EX1_SHORT_READ;
		goto done;
	}

	/* Parse and check the reply: */

	/* First comes the header */
	if (buf[0] != 0xC1 || buf[1] != 0xC0) {
		a1logd(p->log, 1, "ex1_command: start bytes wrong (0x%02x, 0x%02x)\n",buf[0],buf[1]);
		rv = EX1_DATA_PARSE_ERROR;
		goto done;
	}

	pver = read_ORD16_le(buf + 2);
	if (pver < 0x1000) {
		a1logd(p->log, 1, "Unknown protocol version (0x%x)\n",pver);
		rv = EX1_DATA_PARSE_ERROR;
		goto done;
	}
	
	flags = read_ORD16_le(buf + 4);
	merrno = read_ORD16_le(buf + 6);
	mestype = read_ORD32_le(buf + 8);
	regarding = read_ORD32_le(buf + 12);
	chstype = read_ORD8(buf + 22);
	imdatlen = read_ORD8(buf + 23);
	bremain = read_ORD32_le(buf + 40);

	/* If there has been an error, return it */
	if (merrno != EX1_OK) {
		rv = merrno;
		goto done;
	}

	if (bremain < 20) {
		a1logd(p->log, 1, "Bytes remaining %d is too small for chsum & footer\n",bremain);
		rv = EX1_DATA_PARSE_ERROR;
		goto done;
	}

	/* Explicit playload length */
	pll = bremain - 20;	/* Payload length */

	if (pll > 0 && imdatlen > 0) {
		a1logd(p->log, 1, "Got both immediate payoad %d bytes and explicit %d bytes\n",imdatlen,pll);
		rv = EX1_DATA_PARSE_ERROR;
		goto done;
	}

	/* Payload is immediate */
	if (imdatlen > 0) {
		if (imdatlen > olen) {
			a1logd(p->log, 1, "Got %d bytes payload when expecting %d\n",imdatlen, olen);
			rv = EX1_LONG_READ;
			goto done;
		}
		memcpy(out, buf + 24, imdatlen);
		if (prlen != NULL)
			*prlen = imdatlen;
		ardlen = imdatlen;
	}
	
	/* If there is an explicit payload, read the remaining bytes */
	else if (pll > 0) {
		/* Make sure buffer is big enough for read */
		bsize = pll + 64;
		if (bsize > p->cbufsz) {
			if ((p->cbuf = realloc(p->cbuf, bsize)) == NULL) {
				rv = EX1_MEMORY;
				goto done;
			}
			p->cbufsz = bsize;
		}
		buf = p->cbuf;

		se = p->icom->usb_read(p->icom, NULL, EX1_EP | 0x80, buf + 64, pll, &rwbytes, to); 

//		adump_bytes(p->log, "  ", buf + 44, 0, pll);

		if (rwbytes != pll) {
			a1logd(p->log,1,"ex1_command: read %d/%d bytes - short\n",rwbytes, pll);
			rv = EX1_SHORT_READ;
			goto done;
		}

		if (rwbytes > olen) {
			a1logd(p->log, 1, "Got %d bytes payload when expecting %d\n",rwbytes, olen);
			rv = EX1_LONG_READ;
			goto done;
		}
		memcpy(out, buf + 44, pll);
		if (prlen != NULL)
			*prlen = pll;
		ardlen = pll;
	}

	/* Then the checksum */
	if (chstype == EX1_CHSUM_MD5) {
		icmMD5 *md5;

		if ((md5 = new_icmMD5()) == NULL) {
			a1logd(p->log, 1, "new_icmMD5 failed\n");
			rv = EX1_INTERNAL_ERROR;
			goto done;
		} else {
			ORD8 chsum[16];
			int i;

			md5->add(md5, buf, 44 + pll);
			md5->get(md5, chsum);
			for (i = 0; i < 16; i++) {
				if (chsum[i] != buf[44+pll+i])
					break;
			}
			if (i < 16) {
				a1logd(p->log, 1, "MD5 checksum failed\n");
				md5->del(md5);
				rv = EX1_DATA_CHSUM_ERROR; 
				goto done;
			}
			md5->del(md5);
		}
	}

	/* If we were expecting an exact ammount */
	if (prlen == NULL && ardlen != olen) {
		a1logd(p->log, 1, "Got %d bytes payload when expecting %d\n",ardlen, olen);
		rv = EX1_SHORT_READ;
		goto done;
	}

	/* Finally the footer */
	fo = 44 + pll + 16;
	if (buf[fo] != 0xC5 || buf[fo + 1] != 0xC4 || buf[fo + 2] != 0xC3 || buf[fo + 3] != 0xC2) {
		a1logd(p->log, 1, "Footer error (0x%02x 0x%02x 0x%02x 0x%02x)\n",
		                            buf[fo],buf[fo+1],buf[fo+2],buf[fo+3]);
		rv = EX1_DATA_PARSE_ERROR;
		goto done;
	}
 
	if (p->log->debug >= 7 && out != NULL && olen > 0) {
		adump_bytes(p->log, "  ", out, 0, olen);
	}

  done:;
	a1logd(p->log,6,"ex1_command: returning 0x%x (%d msec)\n", rv, msec_time()-stime);

	return rv;
}

#define EX1_CMD_GET_HW_REV      0x00000080

/* Flush the pipe in case we have an unexpected reply waiting for us. */
/* Note that the instrument will return a measurement initiated from */
/* a previous session if it finishes in this session, i.e. closing */
/* the instrument doesn't abort what it is doing ! */
/* (This helps on MSWin, but stuffs up Linux) */
static void
ex1_flush(
ex1 *p
) {
#ifdef NEVER
	char buf[8096];
	int debugl = p->log->debug;
	p->log->debug = 0;
	p->icom->usb_resetep(p->icom, EX1_EP);
	p->icom->usb_read(p->icom, NULL, EX1_EP | 0x80, buf, 8096, NULL, 0.1); 
	ex1_command(p, EX1_CMD_GET_HW_REV, NULL, 0, buf, 1, NULL, 0.1, 0);
	p->icom->usb_resetep(p->icom, EX1_EP);
	p->log->debug = debugl;
#endif
}

/* ------------------------------------------------------------------- */
/* Implement specific commands */

/* Unimplemented commands/queries */
#define EX1_CMD_RESETDEFAULTS   0x00000001

#define EX1_CMD_GET_NO_USER_STR 0x00000300
#define EX1_CMD_GET_USER_STR_SZ 0x00000301
#define EX1_CMD_GET_USER_STR    0x00000302

/* - - - - - - */

//#define EX1_CMD_GET_HW_REV      0x00000080

/* Return Hardware revision number */
/* Return the ex1 error code. */
static int ex1_get_hw_rev(ex1 *p, int *hwrev) {
	ORD8 buf[1];
	int ec;

	if ((ec = ex1_command(p, EX1_CMD_GET_HW_REV, NULL, 0, buf, 1, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}
	*hwrev = read_ORD8(buf);

	return EX1_OK;
} 


#define EX1_CMD_GET_FW_REV 0x00000090

/* Return Firmware revision number */
/* Return the ex1 error code. */
static int ex1_get_fw_rev(ex1 *p, int *fwrev) {
	ORD8 buf[2];
	int ec;

	if ((ec = ex1_command(p, EX1_CMD_GET_FW_REV, NULL, 0, buf, 2, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}
	*fwrev = read_ORD16_le(buf);

	return EX1_OK;
} 

#define EX1_CMD_GET_SLITW 0x001B0200

/* Get slit width in microns */
/* Return 0 if info. not available */
/* Return the ex1 error code. */
static int ex1_get_slit_width(ex1 *p, int *swidth) {
	ORD8 buf[2];
	int ec;

	if ((ec = ex1_command(p, EX1_CMD_GET_SLITW, NULL, 0, buf, 2, NULL, 1.0, 0)) != EX1_OK) {
		if (ec != EX1_NO_INF)
			return ec;
		*swidth = 0;
	} else {
		*swidth = read_ORD16_le(buf);
	}

	return EX1_OK;
} 

#define EX1_CMD_GET_FIBW 0x001B0300

/* Get fiber diameter in microns */
/* Return the ex1 error code. */
static int ex1_get_fiber_width(ex1 *p, int *fibw) {
	ORD8 buf[2];
	int ec;

	if ((ec = ex1_command(p, EX1_CMD_GET_FIBW, NULL, 0, buf, 2, NULL, 1.0, 0)) != EX1_OK) {
		if (ec != EX1_NO_INF)
			return ec;
		*fibw = 0;
	} else {
		*fibw = read_ORD16_le(buf);
	}

	return EX1_OK;
} 

#define EX1_CMD_GET_GRATING 0x001B0400

/* Return grating string, or NULL if none */
/* (free after use) */
/* Return the ex1 error code. */
static int ex1_get_grating(ex1 *p, char **grating) {
	int bread;
	int len = 32;		/* We assume a max of 32 bytes */
	int ec;

	if ((*grating = malloc(len+1)) == NULL)
		return EX1_MEMORY;
	if ((ec = ex1_command(p, EX1_CMD_GET_GRATING, NULL, 0, (ORD8 *)*grating, len, &bread, 2.0, 0))
		                                                                        != EX1_OK) {
		if (ec != EX1_NO_INF)
			return ec;
		free(*grating);
		*grating = NULL;
	} else {
		(*grating)[bread] = '\000';
	}

	return EX1_OK;
}

#define EX1_CMD_GET_FILTER 0x001B0500

/* Return filter string, or NULL if none */
/* (free after use) */
/* Return the ex1 error code. */
static int ex1_get_filter(ex1 *p, char **filter) {
	int bread;
	int len = 32;		/* We assume a max of 32 bytes */
	int ec;

	if ((*filter = malloc(len+1)) == NULL)
		return EX1_MEMORY;
	if ((ec = ex1_command(p, EX1_CMD_GET_FILTER, NULL, 0, (ORD8 *)*filter, len, &bread, 2.0, 0))
		                                                                        != EX1_OK) {
		if (ec != EX1_NO_INF)
			return ec;
		free(*filter);
		*filter = NULL;
	} else {
		(*filter)[bread] = '\000';
	}

	return EX1_OK;
}

#define EX1_CMD_GET_COATING 0x001B0600

/* Return coating string, or NULL if none */
/* (free after use) */
/* Return the ex1 error code. */
static int ex1_get_coating(ex1 *p, char **coating) {
	int bread;
	int len = 32;		/* We assume a max of 32 bytes */
	int ec;

	if ((*coating = malloc(len+1)) == NULL)
		return EX1_MEMORY;
	if ((ec = ex1_command(p, EX1_CMD_GET_COATING, NULL, 0, (ORD8 *)*coating, len, &bread, 2.0, 0))
		                                                                        != EX1_OK) {
		if (ec != EX1_NO_INF)
			return ec;
		free(*coating);
		*coating = NULL;
	} else {
		(*coating)[bread] = '\000';
	}

	return EX1_OK;
}


#define EX1_CMD_GET_ALIAS_LEN   0x00000201
#define EX1_CMD_GET_ALIAS       0x00000200

/* Return device alias string, or NULL if none */
/* (free after use) */
/* Return the ex1 error code. */
static int ex1_get_alias(ex1 *p, char **alias) {
	ORD8 buf[3];
	int bread;
	int len;
	int ec;

	if ((ec = ex1_command(p, EX1_CMD_GET_ALIAS_LEN, NULL, 0, buf, 1, NULL, 2.0, 0)) != EX1_OK) {
		return ec;
	}

	len = read_ORD8(buf);
	if (len == 0) {
		*alias = NULL;
		return inst_ok;
	}
	if ((*alias = malloc(len+1)) == NULL) {
		return EX1_MEMORY;
	}

	if ((ec = ex1_command(p, EX1_CMD_GET_ALIAS, NULL, 0, (ORD8 *)*alias, len, &bread, 2.0, 0))
		                                                                            != EX1_OK) {
		if (ec != EX1_NO_INF) {
			return ec;
		}
		free(*alias);
		*alias = NULL;
	} else {
		(*alias)[bread] = '\000';
	}

	return EX1_OK;
} 

#define EX1_CMD_GET_SERNO_LEN   0x00000101
#define EX1_CMD_GET_SERNO       0x00000100

/* Return device serial string, or NULL if none */
/* (free after use) */
/* Return the ex1 error code. */
static int ex1_get_serno(ex1 *p, char **serno) {
	ORD8 buf[3];
	int bread;
	int len;
	int ec;

	if ((ec = ex1_command(p, EX1_CMD_GET_SERNO_LEN, NULL, 0, buf, 1, NULL, 2.0, 0)) != EX1_OK) {
		return ec;
	}

	len = read_ORD8(buf);
	if (len == 0) {
		*serno = NULL;
		return inst_ok;
	}
	if ((*serno = malloc(len+1)) == NULL) {
		return EX1_MEMORY;
	}

	if ((ec = ex1_command(p, EX1_CMD_GET_SERNO, NULL, 0, (ORD8 *)*serno, len, &bread, 2.0, 0))
		                                                                            != EX1_OK) {
		if (ec != EX1_NO_INF) {
			return ec;
		}
		free(*serno);
		*serno = NULL;
	} else {
		(*serno)[bread] = '\000';
	}

	return EX1_OK;
} 

/* - - - - - - */

#define EX1_CMD_GET_WL_COEF_COUNT  0x00180100
#define EX1_CMD_GET_WL_COEF        0x00180101

/* Get wavelength cooeficients. */
/* (free after use) */
/* Return the ex1 error code. */
static int ex1_get_wl_coefs(ex1 *p, unsigned int *nocoefs, double **coefs) {
	ORD8 buf[4];
	unsigned int i, no;
	int ec;

	if ((ec = ex1_command(p, EX1_CMD_GET_WL_COEF_COUNT, NULL, 0, buf, 1, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}
	no = read_ORD8(buf);

	/* There should be 4, but 2 is the minimum */
	if (no < 2) {
		return EX1_NO_WL_CAL;
	}
	if ((*coefs = malloc(sizeof(double) * no)) == NULL) {
		return EX1_MEMORY;
	}

	for (i = 0; i < no; i++) {
		write_ORD8(buf, i);
		if ((ec = ex1_command(p, EX1_CMD_GET_WL_COEF, buf, 1, buf, 4, NULL, 1.0, 0)) != EX1_OK) {
			*nocoefs = 0;
			free(*coefs);
			return ec;
		}
		(*coefs)[i] = IEEE754todouble(read_ORD32_le(buf));
	}
	*nocoefs = no;

	if (p->log->debug >= 6) {
		a1logd(p->log,1,"ex1: no. wavelength calib coefs = %d\n",no);
		for (i = 0; i < no; i++) {
			a1logd(p->log,1,"  [%d] = %e\n",i,(*coefs)[i]);
		}
	}

	return EX1_OK;
} 

#define EX1_CMD_GET_NL_COEF_COUNT  0x00181100
#define EX1_CMD_GET_NL_COEF        0x00181101

/* Get non-linearity coefficient */
/* (free after use) */
/* Return the ex1 error code. */
static int ex1_get_nl_coefs(ex1 *p, unsigned int *nocoefs, double **coefs) {
	ORD8 buf[4];
	unsigned int i, no;
	int ec;

	if ((ec = ex1_command(p, EX1_CMD_GET_NL_COEF_COUNT, NULL, 0, buf, 1, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}
	no = read_ORD8(buf);

	if (no == 0) {
		*nocoefs = 0;
		*coefs = NULL;
		return EX1_OK;
	}
	if ((*coefs = malloc(sizeof(double) * no)) == NULL) {
		return EX1_MEMORY;
	}

	for (i = 0; i < no; i++) {
		write_ORD8(buf, i);
		if ((ec = ex1_command(p, EX1_CMD_GET_NL_COEF, buf, 1, buf, 4, NULL, 1.0, 0)) != EX1_OK) {
			free(*coefs);
			*nocoefs = 0;
			*coefs = NULL;
			return ec;
		}
		(*coefs)[i] = IEEE754todouble(read_ORD32_le(buf));
	}
	*nocoefs = no;

	if (p->log->debug >= 6) {
		a1logd(p->log,1,"ex1: no. linearity calib coefs = %d\n",no);
		for (i = 0; i < no; i++) {
			a1logd(p->log,1,"  [%d] = %e\n",i,(*coefs)[i]);
		}
	}

	return EX1_OK;
} 

#define EX1_CMD_GET_IR_COEF_COUNT  0x00182002
#define EX1_CMD_GET_IR_COEF        0x00182001
#define EX1_CMD_GET_IR_AREA        0x00182003

/* Get Irradiance calibration coefficients and collection area */
/* (free after use) */
/* Return the ex1 error code. */
static int ex1_get_ir_coefs(ex1 *p, unsigned int *nocoefs, double **coefs, double *area) {
	ORD8 buf[4], *tbuf;
	unsigned int i, no;
	int ec;

	if ((ec = ex1_command(p, EX1_CMD_GET_IR_COEF_COUNT, NULL, 0, buf, 4, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}
	no = read_ORD32_le(buf);

	if (no == 0) {
		*nocoefs = 0;
		*coefs = NULL;
		return EX1_NO_IR_CAL;
	}
	if ((tbuf = malloc(4 * no)) == NULL) {
		return EX1_MEMORY;
	}

	if ((*coefs = malloc(sizeof(double) * no)) == NULL) {
		free(tbuf);
		return EX1_MEMORY;
	}

	if ((ec = ex1_command(p, EX1_CMD_GET_IR_COEF, NULL, 0, tbuf, 4 * no, NULL, 1.0, 0)) != EX1_OK) {
		free(*coefs);
		*nocoefs = 0;
		*coefs = NULL;
		*area = 0.0;
		return ec;
	}
	for (i = 0; i < no; i++) {
		(*coefs)[i] = IEEE754todouble(read_ORD32_le(tbuf + 4 * i));
	}
	*nocoefs = no;
	free(tbuf);

	if (p->log->debug >= 6) {
		a1logd(p->log,1,"ex1: no. Irradiance calib coefs = %d\n",no);
		for (i = 0; (i+3) < no; i += 4) {
			a1logd(p->log,1,"  [%d] = %e, %e %e %e\n",i,
			          (*coefs)[i], (*coefs)[i+1], (*coefs)[i+2], (*coefs)[i+3]);
		}
	}

	if ((ec = ex1_command(p, EX1_CMD_GET_IR_AREA, NULL, 0, buf, 4, NULL, 1.0, 0)) != EX1_OK) {
		if (ec != EX1_NO_INF) {
			return ec;
		}
		*area = 0.0;
	} else {
		*area = IEEE754todouble(read_ORD32_le(buf));
	}

	a1logd(p->log,1,"ex1: Irradiance collection area = %f\n",*area);

	return EX1_OK;
}

#define EX1_CMD_GET_SL_COEF_COUNT  0x00183100
#define EX1_CMD_GET_SL_COEF        0x00183101

/* Get stray light coefficient */
/* (free after use) */
/* Return the ex1 error code. */
static int ex1_get_sl_coefs(ex1 *p, unsigned int *nocoefs, double **coefs) {
	ORD8 buf[4];
	unsigned int i, no;
	int ec;

	if ((ec = ex1_command(p, EX1_CMD_GET_SL_COEF_COUNT, NULL, 0, buf, 1, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}
	no = read_ORD8(buf);

	if (no == 0) {
		*nocoefs = 0;
		*coefs = NULL;
		return EX1_OK;
	}
	if ((*coefs = malloc(sizeof(double) * no)) == NULL) {
		return EX1_MEMORY;
	}

	for (i = 0; i < no; i++) {
		write_ORD8(buf, i);
		if ((ec = ex1_command(p, EX1_CMD_GET_SL_COEF, buf, 1, buf, 4, NULL, 1.0, 0)) != EX1_OK) {
			free(*coefs);
			*nocoefs = 0;
			*coefs = NULL;
			if (ec != EX1_NO_INF)
				return ec;
			else
				return EX1_OK;
		}
		(*coefs)[i] = IEEE754todouble(read_ORD32_le(buf));
	}
	*nocoefs = no;

	if (p->log->debug >= 6) {
		a1logd(p->log,1,"ex1: no. stray light calib coefs = %d\n",no);
		for (i = 0; i < no; i++) {
			a1logd(p->log,1,"  [%d] = %e\n",i,(*coefs)[i]);
		}
	}

	return EX1_OK;
} 


/* - - - - - - */

#define EX1_CMD_SET_TRIGMODE 0x00110110

/* Set trigger mode */
static int ex1_set_trig_mode(ex1 *p, int trigmode) {
	ORD8 buf[1];
	int ec;

	write_ORD8(buf, trigmode);
	if ((ec = ex1_command(p, EX1_CMD_SET_TRIGMODE, buf, 1, NULL, 0, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}
	return EX1_OK;
} 

#define EX1_CMD_SET_INTTIME 0x00110010

/* Set integration time, min 10 usec, max 10 sec */
/* Return qualtized integration time in qinttime if != NULL */
static int ex1_set_inttime(ex1 *p, double *qinttime, double inttime) {
	ORD8 buf[4];
	unsigned int iinttime;
	int ec;

	inttime = floor(inttime * 1e6 + 0.5);		/* To int usec */
	if (inttime < (EX1_INTTIME_MIN * 1e6) || inttime > (EX1_INTTIME_MAX * 1e6))
		return EX1_INTTIME_RANGE;

	iinttime = (unsigned int)inttime;
	write_ORD32_le(buf, iinttime);

	if ((ec = ex1_command(p, EX1_CMD_SET_INTTIME, buf, 4, NULL, 0, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}

	if (qinttime != NULL)
		*qinttime = inttime/1e6;

	return EX1_OK;
} 

#define EX1_CMD_SET_DELTIME 0x00110510

/* Set trigger delay time, min 5 usec, max 355.5 msec */
/* Return quantized trigger delay time in qtrigdel if != NULL */
static int ex1_set_trig_delay(ex1 *p, double *qtrigdel, double trigdel) {
	ORD8 buf[4];
	unsigned int itrigdel;
	int ec;

	trigdel = floor(trigdel * 1e6 + 0.5);		/* To int usec */
	if (trigdel < (EX1_DELTIME_MIN * 1e6) || trigdel > (EX1_DELTIME_MAX * 1e6))
		return EX1_DELTIME_RANGE;

	itrigdel = (unsigned int)trigdel;
	write_ORD32_le(buf, itrigdel);

	if ((ec = ex1_command(p, EX1_CMD_SET_DELTIME, buf, 4, NULL, 0, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}

	if (qtrigdel != NULL)
		*qtrigdel = trigdel/1e6;

	return EX1_OK;
} 

#define EX1_CMD_SET_STRBPER 0x00310010 

/* Set continuous strobe period, min 50 usec, max 5 sec */
/* Return quantized strobe period in qinttime if != NULL */
static int ex1_set_strobe_period(ex1 *p, double *qstbper, double stbper) {
	ORD8 buf[4];
	unsigned int istbper;
	int ec;

	stbper = floor(stbper * 1e6 + 0.5);		/* To int usec */
	if (stbper < (EX1_STRBPER_MIN * 1e6) || stbper > (EX1_STRBPER_MAX * 1e6))
		return EX1_STROBE_RANGE;

	istbper = (unsigned int)stbper;
	write_ORD32_le(buf, istbper);

	if ((ec = ex1_command(p, EX1_CMD_SET_STRBPER, buf, 4, NULL, 0, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}

	if (qstbper != NULL)
		*qstbper = stbper/1e6;

	return EX1_OK;
} 

#define EX1_CMD_SET_STRBEN 0x00310011

/* Set continuous strobe enable/disable mode */
static int ex1_set_strobe_enable(ex1 *p, int enable) {
	ORD8 buf[1];
	int ec;

	write_ORD8(buf, enable);
	if ((ec = ex1_command(p, EX1_CMD_SET_STRBEN, buf, 1, NULL, 0, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}
	return EX1_OK;
} 

#define EX1_CMD_SET_SING_STRBEN 0x00300012

/* Set single strobe enable/disable mode */
static int ex1_set_single_strobe_enable(ex1 *p, int enable) {
	ORD8 buf[1];
	int ec;

	write_ORD8(buf, enable);
	if ((ec = ex1_command(p, EX1_CMD_SET_SING_STRBEN, buf, 1, NULL, 0, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}
	return EX1_OK;
} 

#define EX1_CMD_SET_AVERAGE 0x00120010

/* Set number of scans to average, range 1 - 5000 */
static int ex1_set_average(ex1 *p, int noavg) {
	ORD8 buf[2];
	int ec;

	if (noavg < EX1_AVERAGE_MIN || noavg > EX1_AVERAGE_MAX)
		return EX1_AVERAGE_RANGE;

	write_ORD16_le(buf, noavg);
	if ((ec = ex1_command(p, EX1_CMD_SET_AVERAGE, buf, 2, NULL, 0, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}
	p->noaverage = noavg;
	return EX1_OK;
} 

#define EX1_CMD_SET_BINNING 0x00110290

/* Set binning factor. */
static int ex1_set_binning(ex1 *p, int bf) {
	ORD8 buf[1];
	int ec;

	write_ORD8(buf, bf);
	if ((ec = ex1_command(p, EX1_CMD_SET_BINNING, buf, 1, NULL, 0, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}
	return EX1_OK;
} 

#define EX1_CMD_SET_BOXCAR 0x00121010

/* Set boxcar filtering width, 0 - 15 */
static int ex1_set_boxcar(ex1 *p, int nobox) {
	ORD8 buf[1];
	int ec;

	if (nobox < EX1_BOXCAR_MIN || nobox > EX1_BOXCAR_MAX)
		return EX1_BOXCAR_RANGE;

	write_ORD8(buf, nobox);
	if ((ec = ex1_command(p, EX1_CMD_SET_BOXCAR, buf, 1, NULL, 0, NULL, 1.0, 0)) != EX1_OK) {
		return ec;
	}
	return EX1_OK;
} 

/* - - - - - - */

#define EX1_CMD_GET_COR_SPEC 0x00101000

/* Get corrected spectrum (temp and pattern noise corrected). */
/* raw must be double[1024] */
/* Return the ex1 error code. */
static int ex1_measure(ex1 *p, double *raw) {
	ORD8 buf[2048];
	unsigned int i, no = 1024;
	int ec;
	double to;
	int stime;

	to = 1.0 + p->noaverage * (MIN_CYCLE_TIME + p->inttime) * 1.1;

//printf("~1 ex1_measure: to = %f from %d x %f\n",to,p->noaverage,p->inttime);

	stime = msec_time();
	if ((ec = ex1_command(p, EX1_CMD_GET_COR_SPEC, NULL, 0, buf, no * 2, NULL, to, 0)) != EX1_OK) {
		return ec;
	}
//printf("Measure took %d msec\n", msec_time()-stime);

	for (i = 0; i < no; i++)
		raw[i] = (double)read_ORD16_le(buf + 2 * i);

	if (p->log->debug >= 6) {
		a1logd(p->log,1,"ex1: spectrum:\n");
		for (i = 0; (i+3) < no; i += 4) {
			a1logd(p->log,1,"  [%d] = %.0f, %.0f %.0f %.0f\n",
			               i, raw[i], raw[i+1], raw[i+2], raw[i+3]);
		}
	}
	return EX1_OK;
}

/* - - - - - - */

/* Return command description */
static char *cmdtstring(unsigned int cmd) {
	switch (cmd) {

		case EX1_CMD_RESETDEFAULTS:
			return "Reset to defaults";
		case EX1_CMD_GET_NO_USER_STR:
			return "Get number of user strings";
		case EX1_CMD_GET_USER_STR_SZ:
			return "Get maximum user string size";
		case EX1_CMD_GET_USER_STR:
			return "Get user string";
		case EX1_CMD_GET_HW_REV:
			return "Get Hardware Revision";
		case EX1_CMD_GET_FW_REV:
			return "Get Firmware revision";
		case EX1_CMD_GET_ALIAS_LEN:
			return "Get Alias string length";
		case EX1_CMD_GET_ALIAS:
			return "Get Alias string";
		case EX1_CMD_GET_SERNO_LEN:
			return "Get Serial number length";
		case EX1_CMD_GET_SERNO:
			return "Get Serial number";
		case EX1_CMD_GET_SLITW:
			return "Get Slit width";
		case EX1_CMD_GET_FIBW:
			return "Get Fiber width";
		case EX1_CMD_GET_GRATING:
			return "Get Grating string";

		case EX1_CMD_GET_WL_COEF_COUNT:
			return "Get Wavelenght coefficient count";
		case EX1_CMD_GET_WL_COEF:
			return "Get Wavelenght coefficient";
		case EX1_CMD_GET_NL_COEF_COUNT:
			return "Get Linearity coefficient count";
		case EX1_CMD_GET_NL_COEF:
			return "Get Linearity coefficient";
		case EX1_CMD_GET_IR_COEF_COUNT:
			return "Get Irradiance coefficient count";
		case EX1_CMD_GET_IR_COEF:
			return "Get Irradiance coefficient";
		case EX1_CMD_GET_IR_AREA:
			return "Get Irradiance collection area";
		case EX1_CMD_GET_SL_COEF_COUNT:
			return "Get Stray light coefficient count";
		case EX1_CMD_GET_SL_COEF:
			return "Get Stray light coefficient";

		case EX1_CMD_SET_TRIGMODE:
			return "Set Trigger mode";
		case EX1_CMD_SET_INTTIME:
			return "Set Intergration time";
		case EX1_CMD_SET_DELTIME:
			return "Set Trigger delay time";
		case EX1_CMD_SET_STRBPER:
			return "Set Strobe period";
		case EX1_CMD_SET_STRBEN:
			return "Set Strobe enable";
		case EX1_CMD_SET_SING_STRBEN:
			return "Set Single Strobe enable";
		case EX1_CMD_SET_AVERAGE:
			return "Set Averaging count";
		case EX1_CMD_SET_BINNING:
			return "Set Binning multiple";
		case EX1_CMD_SET_BOXCAR:
			return "Set Boxcar filtering width";

		case EX1_CMD_GET_COR_SPEC:
			return "Measure corrected spectral values";

		default:
			return "Unknown";
	}
}


