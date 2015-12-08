
/* 
 * Argyll Color Correction System
 *
 * SwatchMate Cube related functions
 *
 * Author: Graeme W. Gill
 * Date:   18/5/2015
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

	Need inst call to clear optional user calibrations -
	add capability, then option to check if set, and
	option to clear.


	Investigate clash between button triggered and progromatic.
	Need to check why calibration times out ??

	Need to make startup more robust - often fails to find instrument ?
	Need to cleanup shutdown (^C) ?

	Like to test on OS X and Linux.
	- hard to do this, as FT231XS driver support is only recent. 

	Like to add BlueTooth LE to MSWin/OS X/Linux.
	- hard to do this, as BTLE support is only recent.

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
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "inst.h"
#include "rspec.h"
#include "smcube.h"
#include "cubecal.h"		/* Calibration */

#undef USEW				/* [Und] Use white sensor rather than R,G,B */

#define ENABLE_NONVCAL	/* [Def] Enable saving calibration state between program runs in a file */

#define WCALTOUT  (1 * 60 * 60) /* [1 Hour ??] White Calibration timeout in seconds */

#define DEFTO 1.0		/* [1.0] Default command timeout */

/* Cube white reference RGB reflectivity as measured by cube */
//static double cwref[3] = { 0.646601, 0.668981, 0.703421 };
static double cwref[3] = { 0.795893, 0.818593, 0.855143 };

/* Assumed 45/0 RGB reflectance of gloss black reference */
static double glref[3] = { 0.012632, 0.009568, 0.010130 };

/* Default black offset if not calibrated with a light trap */
static double dsoff[3] = { 0.059465, 0.063213, 0.069603 };

/* Default gloss offset if not calibrated with a gloss reference */
static double dgoff[3] = { 0.056007, 0.052993, 0.054589 };

/* Assumed temperature coefficients for sensor output */
static double tempc[3] = { 0.0048, 0.0017, 0.0014 };

/* ------------------------------------------------- */

static inst_code smcube_interp_code(inst *pp, int ec);
static inst_code smcube_poll_measure(smcube *p, double to, int nd);
static inst_code smcube_black_calib(smcube *p, int ctype);
static inst_code smcube_get_temp(smcube *p, double *tval);
static inst_code smcube_get_cal_temp(smcube *p, int addr, double *tval);

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */

/* Interpret an icoms error into a SMCUBE error */
static int icoms2smcube_err(int se) {
	if (se != ICOM_OK) {
		if (se & ICOM_TO)
			return SMCUBE_TIMEOUT; 
		return SMCUBE_COMS_FAIL;
	}
	return SMCUBE_OK;
}

/* Commands */
static inst_code smcube_ping(smcube *p);
static inst_code smcube_get_version(smcube *p, int *val);
static inst_code smcube_get_idle_time(smcube *p, int *pitime, int nd);
static inst_code smcube_fact_measure(smcube *p, double *XYZ);
static inst_code smcube_get_cal_val(smcube *p, int addr, double *cval);
static inst_code smcube_fact_white_calib(smcube *p);
static inst_code smcube_meas_wrgb(smcube *p, int ichan, int *wrgb);


static inst_code smcube_white(smcube *p, int ctype);
static inst_code smcube_measure(smcube *p, double *XYZ);

static void cube_rgb2XYZ(double *xyz, double *irgb);

int static smcube_save_calibration(smcube *p);
int static smcube_touch_calibration(smcube *p);
int static smcube_restore_calibration(smcube *p);

/* ------------------------------------------------- */

/* Do a full command/response echange with the smcube */
/* (This level is not multi-thread safe) */
/* Return the smcube error code. */
static int
smcube_fcommand(
struct _smcube *p,
unsigned char *in,		/* Command string */
int ilen,				/* Number of bytes to send */
unsigned char *out,		/* Reply string buffer */
int olen,				/* Number of bytes expected in reply (buffer expected to be MAX_MES_SIZE) */
double to,				/* Timeout for response in seconds */
int nd					/* nz to disable debug messages */
) {
	int se;

	if (!nd) a1logd(p->log, 4, "smcube_fcommand: command '%s'\n", icoms_tohex(in,ilen));
	if ((se = p->icom->write(p->icom, (char *)in, ilen, 0.2)) != 0) {
		if (!nd) a1logd(p->log, 1, "smcube_fcommand: failure on serial write '%s' 0x%x\n",
		                                                                 icoms_tohex(in,ilen),se);
		return icoms2smcube_err(se);
	}

	/* Now wait for a reply */
	if ((se = p->icom->read(p->icom, (char *)out, MAX_MES_SIZE, NULL, NULL, olen, to)) != 0) {
		if (!nd) a1logd(p->log, 1, "smcube_fcommand: failure on serial 0x%x\n",se);
		return icoms2smcube_err(se);
	}
	if (!nd) a1logd(p->log, 4, "smcube_fcommand: returned '%s' err 0x%x\n",
	                                              icoms_tohex(out,olen), se);
	return se;
}

/* Do a normal command/response echange with the smcube. */
/* (This level is not multi-thread safe) */
/* Return the inst code */
static inst_code
smcube_command(
struct _smcube *p,
unsigned char *in,		/* Command string */
int ilen,				/* Number of bytes to send */
unsigned char *out,		/* Reply string buffer */
int olen,				/* Number of bytes expected in reply (buffer expected to be MAX_MES_SIZE) */
double to) {			/* Timout in seconds */
	int rv = smcube_fcommand(p, in, ilen, out, olen, to, 0);
	return smcube_interp_code((inst *)p, rv);
}

/* Establish communications with a smcube */
/* Return SMCUBE_COMS_FAIL on failure to establish communications */
static inst_code
smcube_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	smcube *p = (smcube *) pp;
	unsigned char buf[MAX_MES_SIZE];
	baud_rate brt[] = { baud_38400, baud_nc };
	unsigned int etime;
	unsigned int i;
	instType itype = pp->itype;
	int se;

	inst_code ev = inst_ok;

	a1logd(p->log, 2, "smcube_init_coms: About to init Serial I/O\n");


	if (p->icom->port_type(p->icom) != icomt_serial
	 && p->icom->port_type(p->icom) != icomt_usbserial) {
		a1logd(p->log, 1, "smcube_init_coms: wrong communications type for device!\n");
		return inst_coms_fail;
	}
	
	if (p->bt) {
		amutex_lock(p->lock);

		/* Check instrument is responding */
		buf[0] = 0x7e, buf[1] = 0x00, buf[2] = 0x02, buf[3] = 0x00; /* Ping command */

		if ((ev = smcube_command(p, buf, 4, buf, 4, DEFTO)) != inst_ok) {
			amutex_unlock(p->lock);
			a1logd(p->log, 2, "smcube_init_coms: ping didn't return\n");
			return ev;
		}
		if (buf[0] != 0x7e || buf[1] != 0x20 || buf[2] != 0x02 || buf[3] != 0x00) {
			amutex_unlock(p->lock);
			a1logd(p->log, 2, "smcube_init_coms: ping reply is wrong\n");
			return inst_unknown_model;
		}
		amutex_unlock(p->lock);

	} else {

		amutex_lock(p->lock);

		/* The tick to give up on */
		etime = msec_time() + (long)(1500.0 + 0.5);
	
		a1logd(p->log, 1, "smcube_init_coms: Trying different baud rates (%u msec to go)\n",etime - msec_time());
	
		/* Until we time out, find the correct baud rate */
		for (i = 0; msec_time() < etime; i++) {
			if (brt[i] == baud_nc) {
				i = 0;
			}
			a1logd(p->log, 5, "smcube_init_coms: trying %s baud\n",baud_rate_to_str(brt[i]));
			if ((se = p->icom->set_ser_port(p->icom, fc_Hardware, brt[i], parity_none,
				                         stop_1, length_8)) != ICOM_OK) { 
				amutex_unlock(p->lock);
				a1logd(p->log, 5, "smcube_init_coms: set_ser_port failed with 0x%x\n",se);
				return smcube_interp_code((inst *)p, icoms2smcube_err(se));;		/* Give up */
			}
	
			/* Check instrument is responding */
			buf[0] = 0x7e, buf[1] = 0x00, buf[2] = 0x02, buf[3] = 0x00; /* Ping command */
			if (((ev = smcube_command(p, buf, 4, buf, 4, DEFTO)) & inst_mask)
				                                                       != inst_coms_fail) {
				break;		/* We've got coms or user abort */
			}
	
			/* Check for user abort */
			if (p->uicallback != NULL) {
				inst_code ev;
				if ((ev = p->uicallback(p->uic_cntx, inst_negcoms)) == inst_user_abort) {
					amutex_unlock(p->lock);
					a1logd(p->log, 1, "smcube_init_coms: user aborted\n");
					return inst_user_abort;
				}
			}
		}
	
		if (msec_time() >= etime) {		/* We haven't established comms */
			amutex_unlock(p->lock);
			a1logd(p->log, 2, "smcube_init_coms: failed to establish coms\n");
			return inst_coms_fail;
		}
	
		/* Check the response */
		if (buf[0] != 0x7e || buf[1] != 0x20 || buf[2] != 0x02 || buf[3] != 0x00) {
			amutex_unlock(p->lock);
			a1logd(p->log, 2, "smcube_init_coms: ping didn't return\n");
			return inst_unknown_model;
		}
		amutex_unlock(p->lock);
	}
	a1logd(p->log, 2, "smcube_init_coms: init coms has suceeded\n");

	p->gotcoms = 1;

	return inst_ok;
}

/* Get idle time loop and user measurement detect */ 
/* Poll the instrument at 500msec intervals */
int smcube_mon_thread(void *pp) {
	int nfailed = 0;
	smcube *p = (smcube *)pp;
	inst_code rv1 = inst_ok; 
	a1logd(p->log,3,"Polling thread started\n");
	/* Try indefinitely, in case instrument is put to sleep */
	for (;;) {
		int itime;

		/* See if there is a button generated measure */
		rv1 = smcube_poll_measure(p, 0.1, 1);
		if ((rv1 & inst_mask) == inst_user_trig) {
			a1logd(p->log,3,"Found user trigger\n");
			p->switch_count++;
			if (!p->hide_switch && p->eventcallback != NULL) {
				p->eventcallback(p->event_cntx, inst_event_switch);
			}
		}
		
		/* Presumably this stops it going to sleep */
		rv1 = smcube_get_idle_time(p, &itime, 1); 

		if (p->th_term) {
			p->th_termed = 1;
			break;
		}
		if (rv1 != inst_ok) {
			nfailed++;
			a1logd(p->log,3,"Monitor thread failed with 0x%x\n",rv1);
			continue;
		}
		msec_sleep(500);
	}
	a1logd(p->log,3,"Monitor thread returning\n");
	return rv1;
}

/* Try and read the user measurement, and then trigger measure here. */
int smcube_utrig_thread(void *pp) {
	smcube *p = (smcube *)pp;
	inst_code rv = inst_ok; 

	/* Give caller a chance to return */
	msec_sleep(50);

	/* See if there is a button generated measure */
	rv = smcube_poll_measure(p, 0.1, 1);
	if ((rv & inst_mask) == inst_user_trig) {
		p->switch_count++;
		if (!p->hide_switch && p->eventcallback != NULL) {
			a1logd(p->log,3,"Found user trigger\n");
			p->eventcallback(p->event_cntx, inst_event_switch);
		}
	}
	return 0;
}

/* icoms interrupt callback - used did measurement ? */
int smcube_interrupt(icoms *icom, int icom_int) {
	smcube *p = (smcube *)icom->icntx;		/* Fetch the instrument context */
	inst_code rv = inst_ok; 

	a1logd(p->log,3,"smcube_interrupt called with %d\n",icom_int);

	if (icom_int != icomi_data_available)
		return ICOM_OK;

	/* See if there is a measurement */
	new_athread(smcube_utrig_thread, (void *)p);

	return 0;
}


/* Initialise the SMCUBE */
/* return non-zero on an error, with dtp error code */
static inst_code
smcube_init_inst(inst *pp) {
	smcube *p = (smcube *)pp;
	char mes[100];
	inst_code ev = inst_ok;
	int ver;

	a1logd(p->log, 2, "smcube_init_inst: called\n");

	if (p->gotcoms == 0)
		return inst_internal_error;		/* Must establish coms before calling init */

#ifdef NEVER
	if ((ev = smcube_get_version(p, &p->version)) != inst_ok) {
		return SMCUBE_UNKNOWN_MODEL;
	}
#endif

	amutex_lock(p->lock);
	
	if (p->log->verb) {
		/* Hmm. There is nothing to report */
	}

	if (!p->bt) {
		/* Start the polling loop thread */
		if ((p->th = new_athread(smcube_mon_thread, (void *)p)) == NULL) {
			amutex_unlock(p->lock);
			return SMCUBE_INT_THREADFAILED;
		}
	} else {
		/* Get called back if data becomes available */
		p->icom->interrupt = smcube_interrupt;
	}

	p->lo_secs = 2000000000;		/* A very long time */

#ifdef ENABLE_NONVCAL
	/* Restore idarl calibration from the local system */
	smcube_restore_calibration(p);
	/* Touch it so that we know when the instrument was last opened */
	smcube_touch_calibration(p);
#endif

	p->inited = 1;
	a1logd(p->log, 2, "smcube_init_inst: instrument inited OK\n");
	amutex_unlock(p->lock);

	if (p->log->verb) {
		a1logv(p->log, 1, " Version: %d\n",p->version);
	}

#ifdef NEVER
	/* Debug - dump the calibration */
	smcube_dump_cal(p);
#endif


	return inst_ok;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
smcube_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	smcube *p = (smcube *)pp;
	int ec; 
	int switch_trig = 0;
	int user_trig = 0;
	inst_code rv = inst_protocol_error;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Signal a calibration is needed */
	if (p->want_wcalib && !p->noinitcalib) {
		return inst_needs_cal;		/* Get user to calibrate */
	}

	if (p->trig == inst_opt_trig_user_switch) {
		int currcount = p->switch_count;		/* Variable set by thread */

		p->hide_switch = 1;						/* Supress switch events */

		currcount = p->switch_count;			/* Variable set by thread */
		while (currcount == p->switch_count) {
			int cerr;

			/* Don't trigger on user key if scan, only trigger */
			/* on instrument switch */
			if (p->uicallback != NULL
			 && (rv = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
				if (rv == inst_user_abort) {
					return rv;				/* Abort */
				}
				if (rv == inst_user_trig) {
					user_trig = 1;
					break;						/* Trigger */
				}
			}
			msec_sleep(100);
		}
		if (currcount != p->switch_count)
			switch_trig = 1;

		a1logd(p->log,3,"############# triggered ##############\n");
		if (p->uicallback)	/* Notify of trigger */
			p->uicallback(p->uic_cntx, inst_triggered); 

		p->hide_switch = 0;						/* Enable switch events again */

	} else if (p->trig == inst_opt_trig_user) {

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "smcube: inst_opt_trig_user but no uicallback function set!\n");
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


	/* Take a measurement */
	if (p->icx < 2) {		/* Argyll calibrated measurement */
	
		rv = smcube_measure(p, val->XYZ);

	/* Original factory measurement */
	} else {
		if (switch_trig) {
			icmCpy3(val->XYZ, p->XYZ);
			rv = inst_ok;

		} else {
			rv = smcube_fact_measure(p, val->XYZ);
		}
	}


    if (rv != inst_ok) {
        return rv;
    }

	/* This may not change anything since instrument may clamp */
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);
	val->loc[0] = '\000';

	val->mtype = inst_mrt_reflective;
	val->XYZ_v = 1;		/* These are absolute XYZ readings */

	val->sp.spec_n = 0;
	val->duration = 0.0;
	rv = inst_ok;



	if (user_trig)
		return inst_user_trig;
	return rv;
}

/* Return needed and available inst_cal_type's */
static inst_code smcube_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	smcube *p = (smcube *)pp;
	time_t curtime = time(NULL);
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
	int white_valid = p->white_valid;
		
	if ((curtime - p->wdate) > WCALTOUT) {
		a1logd(p->log,2,"Invalidating white cal as %d secs from last cal\n",curtime - p->wdate);
		white_valid = 0;
	}

	if (!white_valid
	 || (p->want_wcalib && !p->noinitcalib))
		n_cals |= inst_calt_ref_white;

	a_cals |= inst_calt_ref_white;
	a_cals |= inst_calt_ref_dark;

	/* Gloss calibration if in gloss calibrated mode */
	if (p->icx == 1)
		a_cals |= inst_calt_ref_dark_gl;

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	a1logd(p->log,3,"smcube: returning n_cals 0x%x, a_cals 0x%x\n",n_cals, a_cals);

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	return inst_ok;
}

/* Request an instrument calibration. */
inst_code smcube_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	smcube *p = (smcube *)pp;
    inst_cal_type needed, available;
	int dosave = 0;
	inst_code ev = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	id[0] = '\000';

	if ((ev = smcube_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
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

		a1logd(p->log,4,"smcube_calibrate: doing calt 0x%x\n",calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return inst_ok;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return inst_unsupported;
	}

	if (*calt & inst_calt_ref_white) {		/* White calibration */
		time_t cdate = time(NULL);

		if ((*calc & inst_calc_cond_mask) != inst_calc_man_ref_white) {
			*calc = inst_calc_man_ref_white;
			ev = inst_cal_setup;
			goto done;
		}

		if ((ev = smcube_fact_white_calib(p)) != inst_ok) {
			goto done;
		}

		p->white_valid = 1;
		p->want_wcalib = 0;
		p->wdate = cdate;
		*calt &= ~inst_calt_ref_white;
		dosave = 1;
	}

	/* Light trap calibration: */

	/* Is the user skipping the Light trap calibration ? */
	if (*calt & inst_calt_ref_dark
	 && (*calc & inst_calc_cond_mask) == inst_calc_man_ref_dark
	 && *calc & inst_calc_optional_flag) {
		*calt &= ~inst_calt_ref_dark;
	}

	if (*calt & inst_calt_ref_dark) {
		time_t cdate = time(NULL);

		if ((*calc & inst_calc_cond_mask) != inst_calc_man_ref_dark) {
			*calc = inst_calc_man_ref_dark | inst_calc_optional_flag;
			ev = inst_cal_setup;
			goto done;
		}

		if ((ev = smcube_black_calib(p, 0)) != inst_ok) {
			goto done;
		}

		p->dark_valid = 1;
		p->dark_default = 0;
		p->ddate = cdate;
		*calt &= ~inst_calt_ref_dark;
		dosave = 1;
	}

	/* Gloss black calibration */

	/* Is the user skipping the Gloss calibration ? */
	if (*calt & inst_calt_ref_dark_gl
	 && (*calc & inst_calc_cond_mask) == inst_calc_man_dark_gloss
	 && *calc & inst_calc_optional_flag) {
		*calt &= ~inst_calt_ref_dark_gl;
	}

	if (*calt & inst_calt_ref_dark_gl) {
		time_t cdate = time(NULL);

		if ((*calc & inst_calc_cond_mask) != inst_calc_man_dark_gloss) {
			*calc = inst_calc_man_dark_gloss | inst_calc_optional_flag;
			ev = inst_cal_setup;
			goto done;
		}

		if ((ev = smcube_black_calib(p, 1)) != inst_ok) {
			goto done;
		}

		p->gloss_valid = 1;
		p->gloss_default = 0;
		p->gdate = cdate;
		*calt &= ~inst_calt_ref_dark_gl;
		dosave = 1;
	}

  done:;

#ifdef ENABLE_NONVCAL
	if (dosave) {
		/* Save the idark calibration to a file */
		smcube_save_calibration(p);
	}
#endif

	return ev;
}

/* Error codes interpretation */
static char *
smcube_interp_error(inst *pp, int ec) {
//	smcube *p = (smcube *)pp;
	ec &= inst_imask;
	switch (ec) {
		case SMCUBE_INTERNAL_ERROR:
			return "Internal software error";
		case SMCUBE_TIMEOUT:
			return "Communications timeout";
		case SMCUBE_COMS_FAIL:
			return "Communications failure";
		case SMCUBE_UNKNOWN_MODEL:
			return "Not a SwatchMate Cube";
		case SMCUBE_DATA_PARSE_ERROR:
			return "Data from smcube didn't parse as expected";

		case SMCUBE_OK:
			return "No device error";

		case SMCUBE_INT_THREADFAILED:
			return "Starting diffuser position thread failed";
		case SMCUBE_INT_ILL_WRITE:
			return "Attemp to write to factory calibration";
		case SMCUBE_INT_WHITE_CALIB:
			return "No valid white calibration";
		case SMCUBE_INT_BLACK_CALIB:
			return "No valid black calibration";
		case SMCUBE_INT_GLOSS_CALIB:
			return "No valid gloss calibration";
		case SMCUBE_INT_CAL_SAVE:
			return "Saving calibration file failed";
		case SMCUBE_INT_CAL_RESTORE:
			return "Restoring calibration file failed";
		case SMCUBE_INT_CAL_TOUCH:
			return "Touching calibration file failed";

		case SMCUBE_WHITE_CALIB_ERR:
			return "White calibration is outside expected range";
		case SMCUBE_BLACK_CALIB_ERR:
			return "Black calibration is outside expected range";
		case SMCUBE_GLOSS_CALIB_ERR:
			return "Gloss calibration is outside expected range";

		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
smcube_interp_code(inst *pp, int ec) {

	ec &= inst_imask;
	switch (ec) {

		case SMCUBE_OK:
			return inst_ok;

		case SMCUBE_INTERNAL_ERROR:
		case SMCUBE_INT_THREADFAILED:
		case SMCUBE_INT_ILL_WRITE:
		case SMCUBE_INT_WHITE_CALIB:
		case SMCUBE_INT_BLACK_CALIB:
		case SMCUBE_INT_GLOSS_CALIB:
		case SMCUBE_INT_CAL_SAVE:
		case SMCUBE_INT_CAL_RESTORE:
		case SMCUBE_INT_CAL_TOUCH:
			return inst_internal_error | ec;

		case SMCUBE_TIMEOUT:
		case SMCUBE_COMS_FAIL:
			return inst_coms_fail | ec;

//			return inst_unknown_model | ec;

		case SMCUBE_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

//			return inst_wrong_config | ec;

//			return inst_bad_parameter | ec;

		case SMCUBE_WHITE_CALIB_ERR:
		case SMCUBE_BLACK_CALIB_ERR:
		case SMCUBE_GLOSS_CALIB_ERR:
			return inst_misread | ec;

//			return inst_hardware_fail | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
smcube_del(inst *pp) {
	if (pp != NULL) {
		smcube *p = (smcube *)pp;

#ifdef ENABLE_NONVCAL
		smcube_touch_calibration(p);
#endif

		if (p->th != NULL) {		/* Terminate diffuser monitor thread  */
			int i;
			p->th_term = 1;			/* Tell thread to exit on error */
			for (i = 0; p->th_termed == 0 && i < 5; i++)
				msec_sleep(100);	/* Wait for thread to terminate */
			if (i >= 5) {
				a1logd(p->log,3,"smcube diffuser thread termination failed\n");
			}
			p->th->del(p->th);
		}
		if (p->icom != NULL)
			p->icom->del(p->icom);
		amutex_del(p->lock);
		p->vdel(pp);
		free(p);
	}
}

/* Return the instrument mode capabilities */
static void smcube_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	smcube *p = (smcube *)pp;
	inst_mode cap1 = 0;
	inst2_capability cap2 = 0;

	cap1 |= inst_mode_ref_spot
	     |  inst_mode_colorimeter
	        ;

	/* can inst2_has_sensmode, but not report it asynchronously */
	cap2 |= inst2_prog_trig
	     |  inst2_user_trig
	     |  inst2_user_switch_trig
	     |  inst2_disptype			/* Calibration modes */
	     |  inst2_opt_calibs		/* Has optional calibrations that can be cleared */
	        ;

	if (pcap1 != NULL)
		*pcap1 = cap1;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = inst3_none;
}


/* Check device measurement mode */
static inst_code smcube_check_mode(inst *pp, inst_mode m) {
	inst_mode cap;

	if (!pp->gotcoms)
		return inst_no_coms;
	if (!pp->inited)
		return inst_no_init;

	pp->capabilities(pp, &cap, NULL, NULL);

	/* Simple test */
	if (m & ~cap)
		return inst_unsupported;

	/* General check mode against specific capabilities logic: */
	if (!IMODETST(m, inst_mode_ref_spot)) {
		return inst_unsupported;
	}

	return inst_ok;
}

/* Set device measurement mode */
static inst_code smcube_set_mode(inst *pp, inst_mode m) {
	smcube *p = (smcube *)pp;
	int refrmode;
	inst_code ev;

	if ((ev = smcube_check_mode(pp, m)) != inst_ok)
		return ev;

	p->mode = m;

	return inst_ok;
}

/* Calibration modes */
static inst_disptypesel smcube_disptypesel[4] = {
	{
		inst_dtflags_default,		/* flags */
		0,							/* cbix */
		"m",						/* sel */
		"Matte",					/* desc */
		0,							/* refr */
		disptech_none,				/* disptype */
		0							/* ix */
	},
	{
		inst_dtflags_none,			/* flags */
		0,							/* cbix */
		"g",						/* sel */
		"Gloss",					/* desc */
		0,							/* refr */
		disptech_none,				/* disptype */
		1							/* ix */
	},
	{
		inst_dtflags_none,			/* flags */
		0,							/* cbix */
		"N",						/* sel */
		"Native Calibration",		/* desc */
		0,							/* refr */
		disptech_none,				/* disptype */
		2							/* ix */
	},
	{
		inst_dtflags_end,
		0,
		"",
		"",
		0,
		disptech_none,
		0
	}
};

/* Get mode and option details */
static inst_code smcube_get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	smcube *p = (smcube *)pp;
	inst_code rv = inst_ok;

	if (pnsels != NULL)
		*pnsels = 3;

	if (psels != NULL)
		*psels = smcube_disptypesel;

	return inst_ok;
}

/* Given a display type entry, setup for that type */
static inst_code set_disp_type(smcube *p, inst_disptypesel *dentry) {
	inst_code rv;
	int refrmode;

	p->icx = dentry->ix;
	p->dtech = dentry->dtech;

	return inst_ok;
}

/* Set the display type - refresh or not */
static inst_code smcube_set_disptype(inst *pp, int ix) {
	smcube *p = (smcube *)pp;
	inst_code ev;
	inst_disptypesel *dentry;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (ix < 0 || ix > 2)
		return inst_unsupported;

	a1logd(p->log,5,"smcube smcube_set_disptype ix %d\n",ix);
	dentry = &smcube_disptypesel[ix];

	if ((ev = set_disp_type(p, dentry)) != inst_ok) {
		return ev;
	}

	return inst_ok;
}

/* Set the noinitcalib mode */
static void smcube_set_noinitcalib(smcube *p, int v, int losecs) {

	/* Ignore disabling init calib if more than losecs since instrument was open */
	if (v && losecs != 0 && p->lo_secs >= losecs) {
		a1logd(p->log,3,"initcalib disable ignored because %d >= %d secs\n",p->lo_secs,losecs);
		return;
	}
	p->noinitcalib = v;
}

static void set_optcalibs_default(smcube *p) {
	/* Default black offset */
	p->soff[0] = dsoff[0];
	p->soff[1] = dsoff[1];
	p->soff[2] = dsoff[2];
	p->dark_valid = 1;
	p->dark_default = 1;

	/* Default gloss offset */
	p->goff[0] = dgoff[0];
	p->goff[1] = dgoff[1];
	p->goff[2] = dgoff[2];
	p->gloss_valid = 1;
	p->gloss_default = 1;
}

/* 
 * set or reset an optional mode
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 */
static inst_code
smcube_get_set_opt(inst *pp, inst_opt_type m, ...)
{
	smcube *p = (smcube *)pp;
	inst_code ev = inst_ok;

	a1logd(p->log, 5, "smcube_get_set_opt: opt type 0x%x\n",m);

	if (m == inst_opt_initcalib) {		/* default */
		smcube_set_noinitcalib(p, 0, 0);
		return inst_ok;

	} else if (m == inst_opt_noinitcalib) {
		va_list args;
		int losecs = 0;

		va_start(args, m);
		losecs = va_arg(args, int);
		va_end(args);

		smcube_set_noinitcalib(p, 1, losecs);
		return inst_ok;

	/* Record the trigger mode */
	} else if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_user
	 || m == inst_opt_trig_user_switch) {
		p->trig = m;
		return inst_ok;

	/* Is there a black or gloss optional user calibration being used ? */
	} else if (m == inst_opt_opt_calibs_valid) {
		va_list args;
		int *valid;

		va_start(args, m);
		valid = va_arg(args, int *);
		va_end(args);

		if (p->dark_default && p->gloss_default)
			*valid = 0;
		else
			*valid = 1;

		return inst_ok;

	/* Clear all the optional user calibrations back to default */
	} else if (m == inst_opt_clear_opt_calibs) {
		va_list args;

		set_optcalibs_default(p);

#ifdef ENABLE_NONVCAL
		/* Save the updated calibration state to a file */
		smcube_save_calibration(p);
#endif
		return inst_ok;
	}

	/* Get/Sets that require instrument coms. */
	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	return inst_unsupported;
}

/* Constructor */
extern smcube *new_smcube(icoms *icom, instType itype) {
	smcube *p;
	if ((p = (smcube *)calloc(sizeof(smcube),1)) == NULL) {
		a1loge(icom->log, 1, "new_smcube: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	p->init_coms         = smcube_init_coms;
	p->init_inst         = smcube_init_inst;
	p->capabilities      = smcube_capabilities;
	p->check_mode        = smcube_check_mode;
	p->set_mode          = smcube_set_mode;
	p->get_disptypesel   = smcube_get_disptypesel;
	p->set_disptype      = smcube_set_disptype;
	p->get_set_opt       = smcube_get_set_opt;
	p->read_sample       = smcube_read_sample;
	p->get_n_a_cals      = smcube_get_n_a_cals;
	p->calibrate         = smcube_calibrate;
	p->interp_error      = smcube_interp_error;
	p->del               = smcube_del;

	p->icom = icom;
	icom->icntx = (void *)p;	/* Allow us to get instrument from icom */
	p->itype = itype;

	amutex_init(p->lock);

	p->trig = inst_opt_trig_user;

	p->want_wcalib = 1;			/* Do a white calibration each time we open the device */

	set_optcalibs_default(p);

	return p;
}

/* ============================================================================== */
/* Implementation. All of these are thread safe unless noted */

static inst_code
smcube_ping(smcube *p) {
	unsigned char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;
	int se;

	a1logd(p->log, 2, "smcube_ping:\n");

	if (!p->gotcoms)
		return inst_no_coms;

	amutex_lock(p->lock);

	buf[0] = 0x7e;
	buf[1] = 0x00;
	buf[2] = 0x02;
	buf[3] = 0x00;

	if ((ev = smcube_command(p, buf, 4, buf, 4, DEFTO)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	amutex_unlock(p->lock);

	/* Check the response */
	if (buf[0] != 0x7e || buf[1] != 0x20 || buf[2] != 0x02 || buf[3] == 0x00) {
		return smcube_interp_code((inst *)p, SMCUBE_DATA_PARSE_ERROR);
	}
	a1logd(p->log, 2, "smcube_init_coms: ping sucesss\n");

	amutex_unlock(p->lock);

	return inst_ok;
}

static inst_code
smcube_get_idle_time(smcube *p, int *pitime, int nd) {
	unsigned char buf[MAX_MES_SIZE];
	int itime;
	inst_code ev = inst_ok;
	int se;

	if (!nd)
		a1logd(p->log, 2, "smcube_get_idle_time:\n");

	if (!p->gotcoms)
		return inst_no_coms;

	amutex_lock(p->lock);

	memset(buf, 0, 6);
	buf[0] = 0x7e;
	buf[1] = 0x02;
	buf[2] = 0x51;

	if ((se = smcube_fcommand(p, buf, 6, buf, 6, 0.2, nd)) != inst_ok) {
		amutex_unlock(p->lock);
		return smcube_interp_code((inst *)p, se);
	}
	amutex_unlock(p->lock);

	/* Check protocol */
	if (buf[0] != 0x7e || buf[2] != 0x51) {
		return smcube_interp_code((inst *)p, SMCUBE_DATA_PARSE_ERROR);
	}

	/* Check error code */
	if (buf[3] != 0) {
		return smcube_interp_code((inst *)p, buf[3]);
	}
	itime = read_ORD16_be(buf + 4);

	if (!nd)
		a1logd(p->log, 2, "smcube_get_idle_time: returing %d\n",itime);

	if (pitime != NULL)
		*pitime = itime;

	return inst_ok;
}

/* Do a factory measurement */
static inst_code
smcube_fact_measure(smcube *p, double *XYZ) {
	unsigned char buf[MAX_MES_SIZE];
	int itime;
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "smcube_fact_measure:\n");

	if (!p->gotcoms)
		return inst_no_coms;

	amutex_lock(p->lock);

	memset(buf, 0, 16);
	buf[0] = 0x7e;
	buf[1] =   12;
	buf[2] = 0x40;

	if ((ev = smcube_command(p, buf, 16, buf, 16, 3.5)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	amutex_unlock(p->lock);

	/* Check protocol */
	if (buf[0] != 0x7e || buf[2] != 0x40) {
		return smcube_interp_code((inst *)p, SMCUBE_DATA_PARSE_ERROR);
	}

	/* Check error code */
	if (buf[3] != 0) {
		return smcube_interp_code((inst *)p, buf[3]);
	}
	XYZ[0] = IEEE754todouble(read_ORD32_be(buf + 4));
	XYZ[1] = IEEE754todouble(read_ORD32_be(buf + 8));
	XYZ[2] = IEEE754todouble(read_ORD32_be(buf + 12));
	a1logd(p->log, 2, "smcube_fact_measure: returing L*a*b* %f %f %f\n",XYZ[0], XYZ[1], XYZ[2]);

	icmLab2XYZ(&icmD50_100, XYZ, XYZ);

	a1logd(p->log, 2, "smcube_fact_measure: returing XYZ %f %f %f\n",XYZ[0], XYZ[1], XYZ[2]);

	return inst_ok;
}

/* Try and fetch a button generated measurement meassage */
/* Return inst_user_trig if found one */
static inst_code
smcube_poll_measure(smcube *p, double to, int nd) {
	unsigned char buf[MAX_MES_SIZE];
	int se;
	inst_code ev = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;

	amutex_lock(p->lock);

	if ((se = p->icom->read(p->icom, (char *)buf, MAX_MES_SIZE, NULL, NULL, 16, to)) != 0
		                                                   && (se & ICOM_TO) == 0) {
		amutex_unlock(p->lock);
		return icoms2smcube_err(se);
	}
	amutex_unlock(p->lock);

	/* If we got a timeout, ignore the read */
	if ((se & ICOM_TO) != 0) {
		return inst_ok;
	}

	/* Check protocol */
	if (buf[0] != 0x7e || buf[2] != 0x40) {
		return smcube_interp_code((inst *)p, SMCUBE_DATA_PARSE_ERROR);
	}

	/* Check error code */
	if (buf[3] != 0) {
		return smcube_interp_code((inst *)p, buf[3]);
	}
	p->XYZ[0] = IEEE754todouble(read_ORD32_be(buf + 4));
	p->XYZ[1] = IEEE754todouble(read_ORD32_be(buf + 8));
	p->XYZ[2] = IEEE754todouble(read_ORD32_be(buf + 12));
	if (!nd) a1logd(p->log, 2, "smcube_poll_measure: returing L*a*b* %f %f %f\n",p->XYZ[0], p->XYZ[1], p->XYZ[2]);

	icmLab2XYZ(&icmD50_100, p->XYZ, p->XYZ);

	return inst_user_trig;
}

/* wrgb channel numbers to use */
#ifdef USEW
# define RCH 0		/* Use the White channel */
# define GCH 0
# define BCH 0
#else
# define RCH 1		/* Use the R,G & B channels */
# define GCH 2
# define BCH 3
#endif

/* Measure a 4 channel intensity value */
/* 0 = White */
/* 1 = Red */
/* 2 = Green */
/* 3 = Blue */

static inst_code
smcube_meas_wrgb(smcube *p, int ichan, int *wrgb) {
	unsigned char buf[MAX_MES_SIZE];
	int itime;
	int cmd = 0x47 + ichan;
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "smcube_meas_wrgb: ichan %d\n",ichan);

	if (!p->gotcoms)
		return inst_no_coms;

	amutex_lock(p->lock);

	memset(buf, 0, 12);
	buf[0] = 0x7e;
	buf[1] =    8;
	buf[2] =  cmd;

	if ((ev = smcube_command(p, buf, 12, buf, 12, 1.5)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	amutex_unlock(p->lock);

	/* Check protocol */
	if (buf[0] != 0x7e || buf[2] != cmd) {
		return smcube_interp_code((inst *)p, SMCUBE_DATA_PARSE_ERROR);
	}

	/* Check error code */
	if (buf[3] != 0) {
		return smcube_interp_code((inst *)p, buf[3]);
	}

	wrgb[0] = read_ORD16_be(buf + 4);
	wrgb[1] = read_ORD16_be(buf + 6);
	wrgb[2] = read_ORD16_be(buf + 8);
	wrgb[3] = read_ORD16_be(buf + 10);

	a1logd(p->log, 2, "smcube_meas_wrgb: WRGB %d %d %d %d\n",wrgb[0], wrgb[1], wrgb[2], wrgb[3]);

	return inst_ok;
}

/* Get the version information */
/* Doesn't seem to be implemented ? */
static inst_code smcube_get_version(smcube *p, int *val) {
	unsigned char buf[MAX_MES_SIZE];
	int itime;
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "smcube_version:\n");

	if (!p->gotcoms)
		return inst_no_coms;

	amutex_lock(p->lock);

	memset(buf, 0, 5);
	buf[0] = 0x7e;
	buf[1] =    1;
	buf[2] =  0x19;

	if ((ev = smcube_command(p, buf, 5, buf, 5, 1.5)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	amutex_unlock(p->lock);

	/* Check protocol */
	if (buf[0] != 0x7e || buf[2] != 0x19) {
		return smcube_interp_code((inst *)p, SMCUBE_DATA_PARSE_ERROR);
	}

	/* Check error code */
	if (buf[3] != 0) {
		return smcube_interp_code((inst *)p, buf[3]);
	}

	val[0] = read_ORD8(buf + 4);

	a1logd(p->log, 2, "smcube_version: val %d\n",val[0]);

	return inst_ok;
}


/* Read a calibration value */
static inst_code
smcube_get_cal_val(smcube *p, int addr, double *cval) {
	unsigned char buf[MAX_MES_SIZE];
	int itime;
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "smcube_get_cal_val: addr %d\n",addr);

	if (!p->gotcoms)
		return inst_no_coms;

	amutex_lock(p->lock);

	memset(buf, 0, 9);
	buf[0] = 0x7e;
	buf[1] =    5;
	buf[2] = 0x04;

	buf[4] = addr;

	if ((ev = smcube_command(p, buf, 9, buf, 9, DEFTO)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	amutex_unlock(p->lock);

	/* Check protocol */
	if (buf[0] != 0x7e || buf[2] != 0x04) {
		return smcube_interp_code((inst *)p, SMCUBE_DATA_PARSE_ERROR);
	}

	/* Check error code */
	if (buf[3] != 0) {
		return smcube_interp_code((inst *)p, buf[3]);
	}

	*cval = IEEE754todouble(read_ORD32_be(buf + 5));

	a1logd(p->log, 2, "smcube_get_cal_val: addr %d val %f",addr,*cval);

	return inst_ok;
}

/* Diagnostic - dump all the calibration values */
static void
smcube_dump_cal(smcube *p) {
	int i;
	double val;
	inst_code ev = inst_ok;

	for (i = 0; i < 89; i++) {
		if ((ev = smcube_get_cal_val(p, i, &val)) == inst_ok) {
			printf("Cal addr %d = %f\n",i,val);
		}
	}
}


/* Write a calibration value */
static inst_code
smcube_set_cal_val(smcube *p, int addr, double cval) {
	unsigned char buf[MAX_MES_SIZE];
	int itime;
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "smcube_set_cal_val: addr %d value %f\n",addr,cval);

	if (!p->gotcoms)
		return inst_no_coms;

	if (addr <= 50
	|| (addr >= 69 && addr <= 77) 
	||  addr >= 87) {
		return smcube_interp_code((inst *)p, SMCUBE_INT_ILL_WRITE);
	}

	amutex_lock(p->lock);

	memset(buf, 0, 9);
	buf[0] = 0x7e;
	buf[1] =    5;
	buf[2] = 0x03;

	buf[4] = addr;
	write_ORD32_be(buf + 5, doubletoIEEE754(cval));

	if ((ev = smcube_command(p, buf, 9, buf, 9, DEFTO)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	amutex_unlock(p->lock);

	/* Check protocol */
	if (buf[0] != 0x7e || buf[2] != 0x03) {
		return smcube_interp_code((inst *)p, SMCUBE_DATA_PARSE_ERROR);
	}

	/* Check error code */
	if (buf[3] != 0) {
		return smcube_interp_code((inst *)p, buf[3]);
	}

	a1logd(p->log, 2, "smcube_set_cal_val: addr %d OK\n",addr);

	return inst_ok;
}

/* Get the current temperature */
static inst_code
smcube_get_temp(smcube *p, double *tval) {
	unsigned char buf[MAX_MES_SIZE];
	int itime;
	inst_code ev = inst_ok;
	double temp = 0.0;

	a1logd(p->log, 2, "smcube_get_temp:\n");

	if (!p->gotcoms)
		return inst_no_coms;

	amutex_lock(p->lock);

	memset(buf, 0, 8);
	buf[0] = 0x7e;
	buf[1] =    4;
	buf[2] = 0x41;

	if ((ev = smcube_command(p, buf, 8, buf, 8, DEFTO)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	amutex_unlock(p->lock);

	/* Check protocol */
	if (buf[0] != 0x7e || buf[2] != 0x41) {
		return smcube_interp_code((inst *)p, SMCUBE_DATA_PARSE_ERROR);
	}

	/* Check error code */
	if (buf[3] != 0) {
		return smcube_interp_code((inst *)p, buf[3]);
	}

	temp = IEEE754todouble(read_ORD32_be(buf + 4));

	a1logd(p->log, 2, "smcube_get_temp: val %f OK\n",temp);
	
	if (tval != NULL)
		*tval = temp;

	return inst_ok;
}

/* Get a calibration temperature. */
/* Is this stored automatically on smcube_set_cal_val() ? */ 
/* Typically this is from addr 78, i.e. first user grey scale slot */
static inst_code
smcube_get_cal_temp(smcube *p, int addr, double *tval) {
	unsigned char buf[MAX_MES_SIZE];
	int itime;
	inst_code ev = inst_ok;
	double temp = 0.0;

	a1logd(p->log, 2, "smcube_get_cal_temp: addr %d\n",addr);

	if (!p->gotcoms)
		return inst_no_coms;

	if (addr <= 50
	|| (addr >= 69 && addr <= 77) 
	||  addr >= 87) {
		return smcube_interp_code((inst *)p, SMCUBE_INT_ILL_WRITE);
	}

	amutex_lock(p->lock);

	memset(buf, 0, 9);
	buf[0] = 0x7e;
	buf[1] =    5;
	buf[2] = 0x05;

	buf[4] = addr;

	if ((ev = smcube_command(p, buf, 9, buf, 9, DEFTO)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	amutex_unlock(p->lock);

	/* Check protocol */
	if (buf[0] != 0x7e || buf[2] != 0x05) {
		return smcube_interp_code((inst *)p, SMCUBE_DATA_PARSE_ERROR);
	}

	/* Check error code */
	if (buf[3] != 0) {
		return smcube_interp_code((inst *)p, buf[3]);
	}

	temp = IEEE754todouble(read_ORD32_be(buf + 5));

	a1logd(p->log, 2, "smcube_get_cal_temp: addr %d, val %f OK\n",addr,temp);
	
	if (tval != NULL)
		*tval = temp;

	return inst_ok;
}

/* Do a Factory & Argyll white calibration */
static inst_code
smcube_fact_white_calib(smcube *p) {
	inst_code ev = inst_ok;
	int i, j;
	int wrgb[3][4];

#ifdef USEW
	int normal[3] = { 10009, 20382, 37705 };
#else
	int normal[3] = { 9221, 13650, 28568 };
#endif

	a1logd(p->log, 2, "smcube_fact_white_calib:\n");

	/* Meaure the R, G * B */
	for (i = 0; i < 3; i++) {
		if ((ev = smcube_meas_wrgb(p, i + 1, wrgb[i])) != inst_ok) {
			return ev;
		}
	}

	/* Write the R, G, & B to the Cube User Grey Scale calibration */
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			if ((ev = smcube_set_cal_val(p, 78 + i * 3 + j, (double)wrgb[i][j+1])) != inst_ok) {
				return ev;
			}
		}
	}

	/* Compute Argyll calibration scale factors */
	{
		double tmp[3];

		a1logd(p->log, 2, "smcube_white_calib: Got raw RGB %d %d %d\n",
		                              wrgb[0][RCH],wrgb[1][GCH],wrgb[2][BCH]);

		/* Sanity check raw values */
		if (wrgb[0][RCH] < (normal[0]/2) || wrgb[0][RCH] > (3 * normal[0]/2) 
		 || wrgb[1][GCH] < (normal[1]/2) || wrgb[1][GCH] > (3 * normal[1]/2) 
		 || wrgb[2][BCH] < (normal[2]/2) || wrgb[2][BCH] > (3 * normal[2]/2)) { 
			return smcube_interp_code((inst *)p, SMCUBE_WHITE_CALIB_ERR);
		}

		/* Add black offset into cwref */ 
		for (i = 0; i < 3; i++)
			tmp[i] = cwref[i] * (1.0 - p->soff[i]) + p->soff[i];

		p->sscale[0] = tmp[0]/(double)wrgb[0][RCH];
		p->sscale[1] = tmp[1]/(double)wrgb[1][GCH];
		p->sscale[2] = tmp[2]/(double)wrgb[2][BCH];

		if ((ev = smcube_get_cal_temp(p, 78, &p->ctemp)) != inst_ok)
			return ev;

		a1logd(p->log, 2, "smcube_fact_white_calib: Argyll cal = %e %e %e at temp %f\n",
		                           p->sscale[0], p->sscale[1], p->sscale[2], p->ctemp);
	}

	a1logd(p->log, 2, "smcube_fact_white_calib: done\n");

	return inst_ok;
}

/* Apply temperature compensation to the raw sensor readings */
static void temp_comp_raw(smcube *p, double *rgb, double tval) {
	int j;
	double tchange = tval - p->ctemp;

	for (j = 0; j < 3; j++) {
		rgb[j] *= (1.0 + tchange * tempc[j]);
	} 
}

/* -------------------------------------------------------------- */
/* Argyll calibrated measurement support */

static inst_code
smcube_black_calib(smcube *p, int ctype) {
	inst_code ev = inst_ok;
	int i, j;
	int wrgb[3][4];
	double rgb[3];
	double tval;

	a1logd(p->log, 2, "smcube_black_calib: type %s\n",ctype == 0 ? "trap" : "gloss");

	/* Meaure the R, G * B */
	for (i = 0; i < 3; i++) {
		if ((ev = smcube_meas_wrgb(p, i + 1, wrgb[i])) != inst_ok) {
			return ev;
		}
	}
	rgb[0] = (double)wrgb[0][RCH];
	rgb[1] = (double)wrgb[1][GCH];
	rgb[2] = (double)wrgb[2][BCH];

	if ((ev = smcube_get_temp(p, &tval)) != inst_ok) {
		return ev;
	}
	temp_comp_raw(p, rgb, tval);

	if (!p->white_valid) {
		return smcube_interp_code((inst *)p, SMCUBE_INT_WHITE_CALIB);
	}

	/* Black */
	if (ctype == 0) {

		for (i = 0; i < 3; i++) {
			rgb[i] = p->sscale[i] * rgb[i];
			if (rgb[i] < 0.0)
				rgb[i] = 0.0;
		}

		a1logd(p->log, 2, "smcube_black_calib: soff = %f %f %f, default %f %f %f\n",
		       rgb[0], rgb[1], rgb[2], dsoff[0], dsoff[1], dsoff[2]); 

		/* Sanity check values */
		for (i = 0; i < 3; i++) {
			if (rgb[i] < 0.5 * (dsoff[i])
			 || rgb[i] > 2.0 * (dsoff[i])) {
				a1logd(p->log, 1, "smcube_black_calib: rgb[%d] %f out of range %f .. %f\n",
				                                i, rgb[i], 0.5 * dsoff[i], 2.0 * dsoff[i]);
				break;
			}
		}

		if (i < 3) {
			return smcube_interp_code((inst *)p, SMCUBE_BLACK_CALIB_ERR);
		}

		for (i = 0; i < 3; i++)
			p->soff[i] = rgb[i];

	/* Gloss */
	} else {
		if (!p->dark_valid) {
			return smcube_interp_code((inst *)p, SMCUBE_INT_BLACK_CALIB);
		}

		/* Scale to 100% white reference */
		for (i = 0; i < 3; i++) {
			rgb[i] = p->sscale[i] * rgb[i];
		}

		/* Remove the black offset */
		for (i = 0; i < 3; i++) {
			rgb[i] = (rgb[i] - p->soff[i])/(1.0 - p->soff[i]);
			if (rgb[i] < 0.0)
				rgb[i] = 0.0;
		}
		
		/* Compute the gloss offset */
		for (i = 0; i < 3; i++) {
			rgb[i] = rgb[i] - glref[0];
			if (rgb[i] < 0.0)
				rgb[i] = 0.0;
		}
		a1logd(p->log, 2, "smcube_gloss_calib: goff = %f %f %f, default %f %f %f\n",
		       rgb[0], rgb[1], rgb[2], dgoff[0], dgoff[1], dgoff[2]); 

		/* Sanity check values */
		for (i = 0; i < 3; i++) {
			if (rgb[i] < 0.5 * dgoff[i]
			 || rgb[i] > 2.0 * dgoff[i]) {
				a1logd(p->log, 1, "smcube_gloss_calib: rgb[%d] %f out of range %f .. %f\n",
				                                i, rgb[i], 0.5 * dgoff[i], 2.0 * dgoff[i]);
				break;
			}
		}

		if (i < 3) {
			return smcube_interp_code((inst *)p, SMCUBE_GLOSS_CALIB_ERR);
		}
		
		for (i = 0; i < 3; i++)
			p->goff[i] = rgb[i];
	}

	a1logd(p->log, 2, "smcube_black_calib: done\n");

	return inst_ok;
}

static inst_code
smcube_measure(smcube *p, double *XYZ) {
	inst_code ev = inst_ok;
	int i, j;
	int wrgb[3][4];
	double rgb[3];
	double tval;

	a1logd(p->log, 2, "smcube_measure:\n");

	/* Meaure the R, G * B */
	for (i = 0; i < 3; i++) {
		if ((ev = smcube_meas_wrgb(p, i + 1, wrgb[i])) != inst_ok) {
			return ev;
		}
	}
	rgb[0] = (double)wrgb[0][RCH];
	rgb[1] = (double)wrgb[1][GCH];
	rgb[2] = (double)wrgb[2][BCH];

	a1logd(p->log, 2, "smcube_measure: Raw RGB %f %f %f\n",rgb[0],rgb[1],rgb[2]);

	if ((ev = smcube_get_temp(p, &tval)) != inst_ok) {
		return ev;
	}
	temp_comp_raw(p, rgb, tval);

	a1logd(p->log, 2, "smcube_measure: Temp comp. RGB %f %f %f\n",rgb[0],rgb[1],rgb[2]);

	if (!p->white_valid) {
		return smcube_interp_code((inst *)p, SMCUBE_INT_WHITE_CALIB);
	}

	/* Scale it to white */
	for (i = 0; i < 3; i++)
		rgb[i] *= p->sscale[i];

	a1logd(p->log, 2, "smcube_measure: Scaled RGB %f %f %f\n",rgb[0],rgb[1],rgb[2]);

	if (!p->dark_valid) {
		return smcube_interp_code((inst *)p, SMCUBE_INT_BLACK_CALIB);
	}

	/* Remove the black offset */
	for (i = 0; i < 3; i++) {
		rgb[i] = (rgb[i] - p->soff[i])/(1.0 - p->soff[i]);
		if (rgb[i] < 0.0)
			rgb[i] = 0.0;
	}
	a1logd(p->log, 2, "smcube_measure: Black offset RGB %f %f %f\n",rgb[0],rgb[1],rgb[2]);

	/* If gloss mode */
	if (p->icx == 1) {
		for (i = 0; i < 3; i++) {
			rgb[i] = (rgb[i] - p->goff[i])/(1.0 - p->goff[i]);
			if (rgb[i] < 0.0)
				rgb[i] = 0.0;
		}
		a1logd(p->log, 2, "smcube_measure: Gloss comp. RGB %f %f %f\n",rgb[0],rgb[1],rgb[2]);
	}

	a1logd(p->log, 2, "smcube_measure: RGB reflectance %f %f %f\n",rgb[0],rgb[1],rgb[2]);

#ifdef NEVER
/* Adjust for CC white target */
{
	double targ[3] = { 0.916179, 0.906579, 0.866163 };

	printf("white ref was = %f %f %f\n",cwref[0],cwref[1],cwref[2]);
	for (i = 0; i < 3; i++) {
		cwref[i] *= pow(targ[i]/rgb[i], 0.7);
	}
	printf("adjusted white ref = %f %f %f\n",cwref[0],cwref[1],cwref[2]);
}
#endif

	cube_rgb2XYZ(XYZ, rgb);

	a1logd(p->log, 2, "smcube_measure: done\n");

	return inst_ok;
}

/* ---------------------------- */

/* Convert RGB to XYZ using calibration table */
static void cube_rgb2XYZ(double *xyz, double *irgb) {
	int e;
	double rgb[3];
	int    ix[3];		/* Coordinate of cell */ 
	double co[3];		/* Coordinate offset with the grid cell */
	int    si[3];		/* co[] Sort index, [0] = smalest */

	/* Clip and apply rgb power */
	for (e = 0; e < 3; e++) {
		rgb[e] = irgb[e];
		if (rgb[e] < 0.0)
			rgb[e] = 0.0;
		if (rgb[e] > 1.0)
			rgb[e] = 1.0;
		rgb[e] = pow(rgb[e], clut.dpow);
	}

	/* We are using tetrahedral interpolation. */

	/* Compute base index into grid and coordinate offsets */
	{
		double res_1 = clut.res-1;
		int    res_2 = clut.res-2;
		int e;

		for (e = 0; e < 3; e++) {
			unsigned int x;
			double val;
			val = rgb[e] * res_1;
			if (val < 0.0) {
				val = 0.0;
			} else if (val > res_1) {
				val = res_1;
			}
			x = (unsigned int)floor(val);		/* Grid coordinate */
			if (x > res_2)
				x = res_2;
			co[e] = val - (double)x;	/* 1.0 - weight */
			ix[e] = x;					
		}
	}
	/* Do insertion sort on coordinates, smallest to largest. */
	{
		int e, f, vf;
		double v;
		for (e = 0; e < 3; e++)
			si[e] = e;						/* Initial unsorted indexes */

		for (e = 1; e < 3; e++) {
			f = e;
			v = co[si[f]];
			vf = f;
			while (f > 0 && co[si[f-1]] > v) {
				si[f] = si[f-1];
				f--;
			}
			si[f] = vf;
		}
	}
	/* Now compute the weightings, simplex vertices and output values */
	{
		int e, f;
		double w;		/* Current vertex weight */

		w = 1.0 - co[si[3-1]];		/* Vertex at base of cell */
		for (f = 0; f < 3; f++)
			xyz[f] = w * clut.table[ix[0]][ix[1]][ix[2]][f];

		for (e = 3-1; e > 0; e--) {	/* Middle verticies */
			w = co[si[e]] - co[si[e-1]];
			ix[si[e]]++;
			for (f = 0; f < 3; f++)
				xyz[f] += w * clut.table[ix[0]][ix[1]][ix[2]][f];
		}

		w = co[si[0]];
		ix[si[0]]++;				/* Far corner from base of cell */
		for (f = 0; f < 3; f++)
			xyz[f] += w * clut.table[ix[0]][ix[1]][ix[2]][f];
	}

	if (clut.islab)
		icmLab2XYZ(&icmD50_100, xyz, xyz);
	else
		icmScale3(xyz, xyz, 100.0); 	/* ??? */
}

/* =============================================================================== */
/* Calibration info save/restore */

/* The cube doesn't have an easily accessible serial number :-( */
/* So if you have more than one, you'll be sharing the same calibration !! */

int static smcube_save_calibration(smcube *p) {
	int ev = SMCUBE_OK;
	int i;
	char fname[100];		/* Name */
	calf x;
	int argyllversion = ARGYLL_VERSION;
	int valid;
	int ss;

	snprintf(fname, 99, ".smcube.cal");

	if (calf_open(&x, p->log, fname, 1)) {
		x.ef = 2;
		goto done;
	}

	ss = sizeof(smcube);

	/* Some file identification */
	calf_wints(&x, &argyllversion, 1);
	calf_wints(&x, &ss, 1);

	/* Save all the calibrations */
	calf_wints(&x, &p->white_valid, 1);
	calf_wtime_ts(&x, &p->wdate, 1);
	calf_wdoubles(&x, p->sscale, 3);
	calf_wdoubles(&x, &p->ctemp, 1);

	/* Only save dark cal if it is not the fallback default */ 
	valid = p->dark_valid && !p->dark_default;
	calf_wints(&x, &valid, 1);
	calf_wtime_ts(&x, &p->ddate, 1);
	calf_wdoubles(&x, p->soff, 3);

	/* Only save gloss cal if it is not the fallback default */ 
	valid = p->gloss_valid && !p->gloss_default;
	calf_wints(&x, &valid, 1);
	calf_wtime_ts(&x, &p->gdate, 1);
	calf_wdoubles(&x, p->goff, 3);

	a1logd(p->log,3,"nbytes = %d, Checkum = 0x%x\n",x.nbytes,x.chsum);
	calf_wints(&x, (int *)(&x.chsum), 1);

	if (calf_done(&x))
		x.ef = 3;

  done:;
	if (x.ef != 0) {
		a1logd(p->log,2,"Writing calibration file failed with %d\n",x.ef);
		ev = SMCUBE_INT_CAL_SAVE;
	} else {
		a1logd(p->log,2,"Writing calibration file succeeded\n");
	}

	return ev;
}

/* Restore the all modes calibration from the local system */
int static smcube_restore_calibration(smcube *p) {
	int ev = SMCUBE_OK;
	int i, j;
	char fname[100];		/* Name */
	calf x;
	int argyllversion;
	int valid;
	int ss, nbytes, chsum1, chsum2;

	snprintf(fname, 99, ".smcube.cal");

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

		if (x.ef != 0
		 || argyllversion != ARGYLL_VERSION
		 || ss != (sizeof(smcube))) {
			a1logd(p->log,2,"Identification didn't verify\n");
			if (x.ef == 0)
				x.ef = 4;
			goto done;
		}

		/* Read all the calibrations */
		calf_rints(&x, &p->white_valid, 1);
		calf_rtime_ts(&x, &p->wdate, 1);
		calf_rdoubles(&x, p->sscale, 3);
		calf_rdoubles(&x, &p->ctemp, 1);
	
		calf_rints(&x, &valid, 1);
		calf_rtime_ts(&x, &p->ddate, 1);
		calf_rdoubles(&x, p->soff, 3);
		if (x.rd > 0) {
			if (valid) {
				p->dark_default = 0;
			} else {
				/* Use fallback default black offset */
				p->soff[0] = dsoff[0];
				p->soff[1] = dsoff[1];
				p->soff[2] = dsoff[2];
				p->dark_valid = 1;
				p->dark_default = 1;
			}
		}
	
		calf_rints(&x, &valid, 1);
		calf_rtime_ts(&x, &p->gdate, 1);
		calf_rdoubles(&x, p->goff, 3);
		if (x.rd > 0) {
			if (valid) {
				p->gloss_default = 0;
			} else {
				/* Use fallback default gloss offset */
				p->goff[0] = dgoff[0];
				p->goff[1] = dgoff[1];
				p->goff[2] = dgoff[2];
				p->gloss_valid = 1;
				p->gloss_default = 1;
			}
		}

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

	a1logd(p->log,5,"smcube_restore_calibration done\n");
 done:;

	if (calf_done(&x))
		x.ef = 3;

	if (x.ef != 0) {
		a1logd(p->log,2,"Reading calibration file failed with %d\n",x.ef);
		ev = SMCUBE_INT_CAL_RESTORE;
	}

	return ev;
}

int static smcube_touch_calibration(smcube *p) {
	int ev = SMCUBE_OK;
	char fname[100];		/* Name */
	int rv;

	snprintf(fname, 99, ".smcube.cal");

	if (calf_touch(p->log, fname)) {
		a1logd(p->log,2,"Touching calibration file time failed with\n");
		return SMCUBE_INT_CAL_TOUCH;
	}

	return SMCUBE_OK;
}


