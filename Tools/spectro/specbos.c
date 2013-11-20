
/* 
 * Argyll Color Correction System
 *
 * JETI specbos 1211/1201 related functions
 *
 * Author: Graeme W. Gill
 * Date:   13/3/2013
 *
 * Copyright 1996 - 2013, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Based on DTP92.c
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

	Should add a reflective and transmissive modes,
	by doing a white calibration and dividing the measurement.

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
#include "specbos.h"

/* Default flow control */
#define DEFFC fc_none

static inst_code specbos_interp_code(inst *pp, int ec);

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 8000		/* Maximum reading message reply size */

/* Interpret an icoms error into a SPECBOS error */
static int icoms2specbos_err(int se) {
	if (se != ICOM_OK) {
		if (se & ICOM_TO)
			return SPECBOS_TIMEOUT; 
		return SPECBOS_COMS_FAIL;
	}
	return SPECBOS_OK;
}

/* Do a full command/response echange with the specbos */
/* (This level is not multi-thread safe) */
/* Return the specbos error code. */
static int
specbos_fcommand(
struct _specbos *p,
char *in,			/* In string */
char *out,			/* Out string buffer */
int bsize,			/* Out buffer size */
double to,			/* Timeout in seconds */
int ntc,			/* Number or termination chars */
int ctype,			/* 0 = normal, 1 = *init, 2 = refr reading */
int nd				/* nz to disable debug messages */
) {
	int se;
	char *cp, *tc = "", *dp;

	if (ctype == 0)
		tc = "\r\006\025";		/* Return, Ack or Nak */
	else if (ctype == 1)
		tc = "\007\025";		/* Bell or Nak */
	else if (ctype == 2)
		tc = "\r\025";			/* Return or Nak */

	if ((se = p->icom->write_read(p->icom, in, out, bsize, tc, ntc, to)) != 0) {
		if (!nd) a1logd(p->log, 1, "specbos_fcommand: serial i/o failure on write_read '%s' 0x%x\n",icoms_fix(in),se);
		return icoms2specbos_err(se);
	}

	/* See if there was an error, and remove any enquire codes */
	for (dp = cp = out; *cp != '\000' && (out - dp) < bsize; cp++) {
		if (*cp == '\025') {	/* Got a Nak */
			char buf[100];

			if ((se = p->icom->write_read(p->icom, "*stat:err?\r", buf, 100, "\r", 1, 1.0)) != 0) {
				if (!nd) a1logd(p->log, 1, "specbos_fcommand: serial i/o failure on write_read '%s'\n",icoms_fix(in));
				if (sscanf(buf, "Error Code: %d ",&se) != 1) {
					if (!nd) a1logd(p->log, 1, "specbos_fcommand: failed to parse error code '%s'\n",icoms_fix(buf));
					return icoms2specbos_err(se);
				}
			}
			break;
		}
		if (*cp == '\005')	/* Got an Enquire */
			continue;		/* remove it */
		*dp = *cp;
		dp++;
	}
	out[bsize-1] = '\000';

	if (!nd) a1logd(p->log, 4, "specbos_fcommand: command '%s' returned '%s'\n",
	                                          icoms_fix(in), icoms_fix(out));

	return SPECBOS_OK;
}

/* Do a normal command/response echange with the specbos. */
/* (This level is not multi-thread safe) */
/* Return the inst code */
static inst_code
specbos_command(
struct _specbos *p,
char *in,			/* In string */
char *out,			/* Out string buffer */
int bsize,			/* Out buffer size */
double to) {		/* Timout in seconds */
	int rv = specbos_fcommand(p, in, out, bsize, to, 1, 0, 0);
	return specbos_interp_code((inst *)p, rv);
}

/* Read another line of response */
/* (This level is not multi-thread safe) */
/* Return the inst code */
static int
specbos_readresp(
struct _specbos *p,
char *out,			/* Out string buffer */
int bsize,			/* Out buffer size */
double to			/* Timeout in seconds */
) {
	int rv, se;
	char *cp, *tc = "\r\006\025";		/* Return, Ack or Nak */

	if ((se = p->icom->read(p->icom, out, bsize, tc, 1, to)) != 0) {
		a1logd(p->log, 1, "specbos_readresp: serial i/o failure\n");
		return icoms2specbos_err(se);
	}
	return inst_ok;
}

/* Establish communications with a specbos */
/* Ignore the serial parameters - leave serial in default state. */
/* Return SPECBOS_COMS_FAIL on failure to establish communications */
static inst_code
specbos_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	specbos *p = (specbos *) pp;
	char buf[MAX_MES_SIZE];
	baud_rate brt[] = { baud_921600, baud_115200, baud_38400, baud_nc };
	unsigned int etime;
	unsigned int i;
	instType itype = pp->itype;
	int se;

	inst_code ev = inst_ok;

	a1logd(p->log, 2, "specbos_init_coms: About to init Serial I/O\n");

	amutex_lock(p->lock);

	if (p->icom->port_type(p->icom) != icomt_serial
	 && p->icom->port_type(p->icom) != icomt_usbserial) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "specbos_init_coms: wrong communications type for device!\n");
		return inst_coms_fail;
	}
	
	/* The tick to give up on */
	etime = msec_time() + (long)(1500.0 + 0.5);

	a1logd(p->log, 1, "specbos_init_coms: Trying different baud rates (%u msec to go)\n",etime - msec_time());

	/* Until we time out, find the correct baud rate */
	for (i = 0; msec_time() < etime; i++) {
		if (brt[i] == baud_nc) {
			i = 0;
		}
		a1logd(p->log, 5, "specbos_init_coms: trying baud ix %d\n",brt[i]);
		if ((se = p->icom->set_ser_port(p->icom, fc_none, brt[i], parity_none,
			                         stop_1, length_8)) != ICOM_OK) { 
			amutex_unlock(p->lock);
			a1logd(p->log, 5, "specbos_init_coms: set_ser_port failed with 0x%x\n",se);
			return specbos_interp_code((inst *)p, icoms2specbos_err(se));;		/* Give up */
		}


		/* Check instrument is responding */
		if (((ev = specbos_command(p, "*idn?\r", buf, MAX_MES_SIZE, 0.2)) & inst_mask)
			                                                       != inst_coms_fail) {
			break;		/* We've got coms or user abort */
		}

		/* Check for user abort */
		if (p->uicallback != NULL) {
			inst_code ev;
			if ((ev = p->uicallback(p->uic_cntx, inst_negcoms)) == inst_user_abort) {
				amutex_unlock(p->lock);
				a1logd(p->log, 1, "specbos_init_coms: user aborted\n");
				return inst_user_abort;
			}
		}
	}

	if (msec_time() >= etime) {		/* We haven't established comms */
		amutex_unlock(p->lock);
		a1logd(p->log, 2, "specbos_init_coms: failed to establish coms\n");
		return inst_coms_fail;
	}

	/* Check the response */
	p->model = 0;

	if (strncmp (buf, "SB05", 4) == 0)		/* Special case */
		p->model = 1201;
	else {
		if (strlen(buf) < 11
		 || sscanf(buf, "JETI_SB%d\r", &p->model) != 1) {
			amutex_unlock(p->lock);
			a1logd(p->log, 2, "specbos_init_coms: unrecognised ident string '%s'\n",icoms_fix(buf));
			return inst_protocol_error;
		}
	}

	if (p->model != 1201 && p->model != 1211) {
		amutex_unlock(p->lock);
		a1logd(p->log, 2, "specbos_init_coms: unrecognised model %04d\n",p->model);
		return inst_unknown_model;
	}
	a1logd(p->log, 2, "specbos_init_coms: init coms has suceeded\n");

	p->gotcoms = 1;

#ifdef NEVER 	/* Test a change in baud rate on next plug in */
	a1logd(p->log, 2, "specbos_init_coms: changing baudrate\n");
//	if ((ev = specbos_command(p, "*para:baud 384\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok)
//	if ((ev = specbos_command(p, "*para:baud 115\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok)
	if ((ev = specbos_command(p, "*para:baud 921\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok)
	{
		amutex_unlock(p->lock);
		return ev;
	}
	if ((ev = specbos_command(p, "*para:save\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
#endif

	amutex_unlock(p->lock);

	return inst_ok;
}

static inst_code set_default_disp_type(specbos *p);

/*
	Notes on commands

	*conf is temporary, *para can be saved to instrument
	Since we set the instrument up every time, use *conf ?
	*PARAmeter:SAVE

	Set calibration to auto on presense of ambient cap
	*para:calibn 0

	To check what mode it's in:
	*contr:mhead?
	0 for emissive, 1 for ambient.

	Set auto exposure
	*para:expo 1			// Adapt
	*para:adap 2			// Adapt if under or over
	*conf:maxtin 4000		// Limit to 4 secs for display measurement rather than 60000

	Set output format
	*conf:form 10			// interpolated ASCII with wavelength
	*conf:form 12			// H/L 32 bit float with length & checksum - doesn't seem to work :-(

	Measurement function
	*conf:func 6				// Radiometric spectrum

	Set to wavelength range and step
	*conf:wran 350 850 1

	Set to average just 1 reading
	*conf:aver 1

	Do a configured measurement
	*init

	Fetch the results
	*fetch:XYZ				// Fetches XYZ whatever the function ?
	*fetch 10				// Fetch configured function in format 10

	Measure refresh
	*conf:cycmod 1
	*contr:cyctim 200 4000	// Returns msec

	Set refresh sync mode
	*conf:cycmod 1
	*conf:cyctim cyctim 	// in usec

	message terminations:
		
	*xxx?	\r 		after data \r\r after spectral ??
			\025    Nak on error ??

	*para
	*conf
	*contr	\006	Ack on success,
			\025    Nak on error

	All enquiries and parameter setting: '\r'

	*init	\006	Ack on command accepted
			\025	Nak on error
	        \007	Bell on completion

	*fetch
	*calc
	*calib
	*stat
	*help
	*fetch	\r      after data, \r\r after spectral ??

*/

static inst_code specbos_get_diffpos(specbos *p, int *pos, int nd);

/* Diffuser position thread. */
/* Poll the instrument at 500msec intervals */
int specbos_diff_thread(void *pp) {
	int nfailed = 0;
	specbos *p = (specbos *)pp;
	inst_code rv = inst_ok; 
	a1logd(p->log,3,"Diffuser thread started\n");
	for (nfailed = 0; nfailed < 5;) {
		int pos;

		amutex_lock(p->lock);
		rv = specbos_get_diffpos(p, &pos, 1); 
		amutex_unlock(p->lock);

		if (p->th_term) {
			p->th_termed = 1;
			break;
		}
		if (rv != inst_ok) {
			nfailed++;
			a1logd(p->log,3,"Diffuser thread failed with 0x%x\n",rv);
			continue;
		}
		if (pos != p->dpos) {
			p->dpos = pos;
			if (p->eventcallback != NULL) {
				p->eventcallback(p->event_cntx, inst_event_mconf);
			}
		}
		msec_sleep(500);
	}
	a1logd(p->log,3,"Diffuser thread returning\n");
	return rv;
}

/* Initialise the SPECBOS */
/* return non-zero on an error, with dtp error code */
static inst_code
specbos_init_inst(inst *pp) {
	specbos *p = (specbos *)pp;
	char mes[100];
	char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "specbos_init_inst: called\n");

	if (p->gotcoms == 0)
		return inst_internal_error;		/* Must establish coms before calling init */

	amutex_lock(p->lock);
	
	/* Restore the instrument to it's default settings */
	if ((ev = specbos_command(p, "*conf:default\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok)
		return ev;

	/* Set calibration type to auto on ambient cap */
	if ((ev = specbos_command(p, "*para:calibn 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	/* Set auto exposure/integration time */
	/* Set calibration type to auto on ambient cap */
	if ((ev = specbos_command(p, "*para:expo 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok
	 || (ev = specbos_command(p, "*para:adapt 2\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	p->measto = 20.0;			/* default */

	if (p->model == 1211)
		p->measto = 5.0;		/* Same overall time as i1d3 ?? */
	else if (p->model == 1201)
		p->measto = 15.0;

	/* Set maximum integration time to speed up display measurement */
	sprintf(mes, "*conf:maxtin %d\r", (int)(p->measto * 1000.0+0.5));
	if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	/* Set the measurement function to be Radiometric spectrum */
	if ((ev = specbos_command(p, "*conf:func 6\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	/* Set the measurement format to ASCII */
	if ((ev = specbos_command(p, "*conf:form 4\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	if ((ev = specbos_command(p, "*para:wavbeg?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	if (sscanf(buf, "Predefined start wave: %lf ",&p->wl_short) != 1) {
		amutex_unlock(p->lock);
		a1loge(p->log, 1, "specbos_init_inst: failed to parse start wave\n");
		return ev;
	}
	a1logd(p->log, 1, " Short wl range %f\n",p->wl_short);

	if ((ev = specbos_command(p, "*para:wavend?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	if (sscanf(buf, "Predefined end wave: %lf ",&p->wl_long) != 1) {
		amutex_unlock(p->lock);
		a1loge(p->log, 1, "specbos_init_inst: failed to parse end wave\n");
		return ev;
	}
	if (p->wl_long > 830.0)			/* Could go to 1000 with 1211 */
		p->wl_long = 830.0;

	a1logd(p->log, 1, " Long wl range %f\n",p->wl_long);

	p->nbands = (int)((p->wl_long - p->wl_short + 1.0)/1.0 + 0.5);

	/* Set the wavelength range and resolution */
	sprintf(mes, "*conf:wran %d %d 1\r", (int)(p->wl_short+0.5), (int)(p->wl_long+0.5));
	if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	/* Set to average just 1 reading */
	if ((ev = specbos_command(p, "*conf:aver 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	if (p->log->verb) {
		int val;
		char *sp;

		if ((ev = specbos_command(p, "*idn?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
		if ((sp = strrchr(buf, '\r')) != NULL)
			*sp = '\000';
		a1logv(p->log, 1, " Identificaton:       %s\n",buf);
		
		if ((ev = specbos_command(p, "*vers?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
		
		if ((sp = strrchr(buf, '\r')) != NULL)
			*sp = '\000';
		a1logv(p->log, 1, " Firmware:            %s\n",buf);
		
		if ((ev = specbos_command(p, "*para:spnum?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
		if (sscanf(buf, "spectrometer number: %d ",&val) == 1) {
			a1logv(p->log, 1, " Spectrometer number: %d\n",val);
		}
		
		if ((ev = specbos_command(p, "*para:serno?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
		if (sscanf(buf, "serial number: %d ",&val) == 1) {
			a1logv(p->log, 1, " Serial number:       %d\n",val);
		}
	}

	/* Start the diffuser monitoring thread */
	if ((p->th = new_athread(specbos_diff_thread, (void *)p)) == NULL) {
		amutex_unlock(p->lock);
		return SPECBOS_INT_THREADFAILED;
	}

	p->inited = 1;
	a1logd(p->log, 2, "specbos_init_inst: instrument inited OK\n");
	amutex_unlock(p->lock);

	return inst_ok;
}

static inst_code specbos_measure_set_refresh(specbos *p);
static inst_code specbos_imp_set_refresh(specbos *p);

/* Get the ambient diffuser position */
/* (This is not multithread safe) */
static inst_code
specbos_get_diffpos(
	specbos *p,				/* Object */
	int *pos,				/* 0 = display, 1 = ambient */
	int nd					/* nz = no debug message */
) {
	char buf[MAX_RD_SIZE];
	int ec;

	/* See if we're in emissive or ambient mode */
	if ((ec = specbos_fcommand(p, "*contr:mhead?\r", buf, MAX_MES_SIZE, 1.0, 1, 0, nd)) != inst_ok) {
		return specbos_interp_code((inst *)p, ec);
	}
	if (sscanf(buf, "mhead: %d ",pos) != 1) {
		a1logd(p->log, 2, "specbos_init_coms: unrecognised measuring head string '%s'\n",icoms_fix(buf));
		return inst_protocol_error;
	}
	return inst_ok;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
specbos_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	specbos *p = (specbos *)pp;
	char buf[MAX_RD_SIZE];
	int ec; 
	int user_trig = 0;
	int pos = -1;
	inst_code rv = inst_protocol_error;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	amutex_lock(p->lock);

	if (p->trig == inst_opt_trig_user) {
		amutex_unlock(p->lock);

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "specbos: inst_opt_trig_user but no uicallback function set!\n");
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
		amutex_lock(p->lock);

	/* Progromatic Trigger */
	} else {
		/* Check for abort */
		if (p->uicallback != NULL
		 && (rv = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort) {
			amutex_unlock(p->lock);
			return rv;				/* Abort */
		}
	}

	/* See if we're in emissive or ambient mode */
	if ((rv = specbos_get_diffpos(p, &pos, 0) ) != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}

	if (p->mode & inst_mode_ambient) {
		if (pos != 1) {
			amutex_unlock(p->lock);
			return specbos_interp_code((inst *)p, SPECBOS_SPOS_AMB);
		}
	} else {
		if (pos != 0) {
			amutex_unlock(p->lock);
			return specbos_interp_code((inst *)p, SPECBOS_SPOS_EMIS);
		}
	}

	/* Make sure the target laser is off */
	if ((rv = specbos_command(p, "*contr:laser 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}

	/* Attempt a refresh display frame rate calibration if needed */
	if (p->refrmode != 0 && p->rrset == 0) {
		if ((rv = specbos_measure_set_refresh(p)) != inst_ok) {
			amutex_unlock(p->lock);
			return rv; 
		}
	}
		
	/* Trigger a measurement */
	if ((ec = specbos_fcommand(p, "*init\r", buf, MAX_MES_SIZE, 5.0 * p->measto + 2.0 , 1, 1, 0)) != SPECBOS_OK) {
		amutex_unlock(p->lock);
		return specbos_interp_code((inst *)p, ec);
	}

	/* Read the XYZ */
	if ((ec = specbos_fcommand(p, "*fetch:XYZ\r", buf, MAX_RD_SIZE, 0.5, 3, 0, 0)) != SPECBOS_OK) {
		amutex_unlock(p->lock);
		return specbos_interp_code((inst *)p, ec);
	}

	if (sscanf(buf, " X: %lf Y: %lf Z: %lf ",
           &val->XYZ[0], &val->XYZ[1], &val->XYZ[2]) == 3) {

		/* This may not change anything since instrument may clamp */
		if (clamp)
			icmClamp3(val->XYZ, val->XYZ);
		val->loc[0] = '\000';
		if (p->mode & inst_mode_ambient) {
			val->mtype = inst_mrt_ambient;
//			val->XYZ[0] /= 3.1415926;
//			val->XYZ[1] /= 3.1415926;
//			val->XYZ[2] /= 3.1415926;
		} else
			val->mtype = inst_mrt_emission;
		val->XYZ_v = 1;		/* These are absolute XYZ readings */
		val->sp.spec_n = 0;
		val->duration = 0.0;
		rv = inst_ok;
	} else {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "specbos_read_sample: failed to parse '%s'\n",buf);
		return inst_protocol_error;
	}

	/* spectrum data is returned only if requested */
	if (p->mode & inst_mode_spectral) {
		int i, xsize;
		char *cp, *ncp;
 
		/* (Format 12 doesn't seem to work on the 1211) */
		/* (Format 9 reportedly doesn't work on the 1201) */
		/* The folling works on the 1211 and is reported to work on the 1201 */

		/* Fetch the spectral readings */
		if ((ec = specbos_fcommand(p, "*fetch:sprad\r", buf, MAX_RD_SIZE, 1.0, 2+p->nbands+1, 0, 0)) != SPECBOS_OK) {
			return specbos_interp_code((inst *)p, ec);
		}

		val->sp.spec_n = p->nbands;
		val->sp.spec_wl_short = p->wl_short;
		val->sp.spec_wl_long = p->wl_long;

		/* Spectral data is in W/nm/m^2 */
		val->sp.norm = 1.0;
		cp = buf;
		for (i = -2; i < val->sp.spec_n; i++) {
			if ((ncp = strchr(cp, '\r')) == NULL) {
				amutex_unlock(p->lock);
				a1logd(p->log, 1, "specbos_read_sample: failed to parse spectral\n");
				return inst_protocol_error;
			}
			*ncp = '\000';
			if (i >= 0) {
				val->sp.spec[i] = 1000.0 * atof(cp);	/* Convert to mW/m^2/nm */
				if (p->mode & inst_mode_ambient)
					val->mtype = inst_mrt_ambient;
			}
			cp = ncp+1;
		}
	}
	amutex_unlock(p->lock);

	if (user_trig)
		return inst_user_trig;
	return rv;
}

/* Set the instrument to match the current refresh settings */
/* (Not thread safe) */
static inst_code
specbos_imp_set_refresh(specbos *p) {
	char buf[MAX_MES_SIZE];
	inst_code rv;

	if (p->model == 1201)
		return inst_unsupported; 

	/* Set synchronised read if we should do so */
	if (p->refrmode != 0 && p->refrvalid) {
		char mes[100];
		if ((rv = specbos_command(p, "*conf:cycmod 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			return rv;
		}
		sprintf(mes,"*conf:cyctim %f\r",p->refperiod * 1e6);
		if ((rv = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			return rv;
		}
		a1logd(p->log,5,"specbos_imp_set_refresh set refresh rate to %f Hz\n",1.0/p->refperiod);
	} else {
		if ((rv = specbos_command(p, "*conf:cycmod 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			return rv;
		}
		a1logd(p->log,5,"specbos_imp_set_refresh set non-refresh mode\n");
	}
	return inst_ok;
}

/* Implementation of read refresh rate */
/* (Not thread safe) */
/* Return 0.0 if none detectable */
static inst_code
specbos_imp_measure_refresh(
specbos *p,
double *ref_rate
) {
	char buf[MAX_MES_SIZE], *cp;
	double refperiod = 0.0;
	int ec;
	inst_code rv;

	if (p->model == 1201)
		return inst_unsupported; 

	/* Make sure the target laser is off */
	if ((rv = specbos_command(p, "*contr:laser 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		return rv;
	}

	if ((ec = specbos_fcommand(p, "*contr:cyctim 200 4000\r", buf, MAX_MES_SIZE, 5.0, 1, 2, 0)) != SPECBOS_OK) {
		return specbos_interp_code((inst *)p, ec);
	}

	if ((cp = strchr(buf, 'c')) == NULL)
		cp = buf;
	if (sscanf(cp, "cyctim[ms]: %lf ", &refperiod) != 1) {
		a1logd(p->log, 1, "specbos_read_refrate rate: failed to parse string '%s'\n",icoms_fix(buf));
		*ref_rate = 0.0;
		return inst_misread;
	}

	if (refperiod == 0.0)
		*ref_rate = 0.0;
	else
		*ref_rate = 1000.0/refperiod;

	return inst_ok;
}

/* Read an emissive refresh rate */
static inst_code
specbos_read_refrate(
inst *pp,
double *ref_rate
) {
	specbos *p = (specbos *)pp;
	char buf[MAX_MES_SIZE];
	double refrate;
	inst_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	amutex_lock(p->lock);
	if ((rv = specbos_imp_measure_refresh(p, &refrate)) != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}
	amutex_unlock(p->lock);

	if (refrate == 0.0)
		return inst_misread;

	*ref_rate = refrate;

	return inst_ok;
}

/* Measure and then set refperiod, refrate if possible */
static inst_code
specbos_measure_set_refresh(
	specbos *p			/* Object */
) {
	inst_code rv;
	double refrate = 0.0;
	int mul;
	double pval;

	amutex_lock(p->lock);
	if ((rv = specbos_imp_measure_refresh(p, &refrate)) != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}

	if (refrate != 0.0) {
		p->refrate = refrate;
		p->refrvalid = 1;
		p->refperiod = 1.0/refrate;
	} else {
		p->refrate = 0.0;
		p->refrvalid = 0;
		p->refperiod = 0.0;
	}
	p->rrset = 1;

	if ((rv = specbos_imp_set_refresh(p)) != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}
	amutex_unlock(p->lock);

	return inst_ok;
}

/* Return needed and available inst_cal_type's */
static inst_code specbos_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	specbos *p = (specbos *)pp;
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
		
	if (p->refrmode != 0) {
		if (p->rrset == 0)
			n_cals |= inst_calt_ref_freq;
		a_cals |= inst_calt_ref_freq;
	}

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	return inst_ok;
}

/* Request an instrument calibration. */
inst_code specbos_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	specbos *p = (specbos *)pp;
	inst_code ev;
    inst_cal_type needed, available;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	id[0] = '\000';

	if ((ev = specbos_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
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

		a1logd(p->log,4,"specbos_calibrate: doing calt 0x%x\n",calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return inst_ok;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return inst_unsupported;
	}

	if ((*calt & inst_calt_ref_freq) && p->refrmode != 0) {
		inst_code ev = inst_ok;


		if (*calc != inst_calc_emis_80pc) {
			*calc = inst_calc_emis_80pc;
			return inst_cal_setup;
		}

		/* Do refresh display rate calibration */
		if ((ev = specbos_measure_set_refresh(p)) != inst_ok)
			return ev; 

		*calt &= ~inst_calt_ref_freq;
	}
	return inst_ok;
}

/* Return the last calibrated refresh rate in Hz. Returns: */
static inst_code specbos_get_refr_rate(inst *pp,
double *ref_rate
) {
	specbos *p = (specbos *)pp;
	if (p->refrvalid) {
		*ref_rate = p->refrate;
		return inst_ok;
	} else if (p->rrset) {
		*ref_rate = 0.0;
		return inst_misread;
	}
	return inst_needs_cal;
}

/* Set the calibrated refresh rate in Hz. */
/* Set refresh rate to 0.0 to mark it as invalid */
/* Rates outside the range 5.0 to 150.0 Hz will return an error */
static inst_code specbos_set_refr_rate(inst *pp,
double ref_rate
) {
	specbos *p = (specbos *)pp;
	inst_code rv;

	a1logd(p->log,5,"specbos_set_refr_rate %f Hz\n",ref_rate);

	if (ref_rate != 0.0 && (ref_rate < 5.0 || ref_rate > 150.0))
		return inst_bad_parameter;

	p->refrate = ref_rate;
	if (ref_rate == 0.0)
		p->refrvalid = 0;
	else {
		p->refperiod = 1.0/ref_rate;
		p->refrvalid = 1;
	}
	p->rrset = 1;

	/* Set the instrument to given refresh rate */
	amutex_lock(p->lock);
	if ((rv = specbos_imp_set_refresh(p)) != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}
	amutex_unlock(p->lock);

	return inst_ok;
}


/* Error codes interpretation */
static char *
specbos_interp_error(inst *pp, int ec) {
//	specbos *p = (specbos *)pp;
	ec &= inst_imask;
	switch (ec) {
		case SPECBOS_INTERNAL_ERROR:
			return "Internal software error";
		case SPECBOS_TIMEOUT:
			return "Communications timeout";
		case SPECBOS_COMS_FAIL:
			return "Communications failure";
		case SPECBOS_UNKNOWN_MODEL:
			return "Not a JETI specbos";
		case SPECBOS_DATA_PARSE_ERROR:
			return "Data from specbos didn't parse as expected";
		case SPECBOS_SPOS_EMIS:
			return "Ambient filter should be removed";
		case SPECBOS_SPOS_AMB:
			return "Ambient filter should be used";

		case SPECBOS_OK:
			return "No device error";

		case SPECBOS_COMMAND:
			return "Command";
		case SPECBOS_PASSWORD:
			return "Password";
		case SPECBOS_DIGIT:
			return "Digit";
		case SPECBOS_ARG_1:
			return "Argument 1";
		case SPECBOS_ARG_2:
			return "Argument 2";
		case SPECBOS_ARG_3:
			return "Argument 3";
		case SPECBOS_ARG_4:
			return "Argument 4";
		case SPECBOS_PARAM:
			return "Parameter argument";
		case SPECBOS_CONFIG_ARG:
			return "Config argument";
		case SPECBOS_CONTROL_ARG:
			return "Control argument";
		case SPECBOS_READ_ARG:
			return "Read argument";
		case SPECBOS_FETCH_ARG:
			return "Fetch argument";
		case SPECBOS_MEAS_ARG:
			return "Measuring argument";
		case SPECBOS_CALC_ARG:
			return "Calculation argument";
		case SPECBOS_CAL_ARG:
			return "Calibration argument";
		case SPECBOS_PARAM_CHSUM:
			return "Parameter checksum";
		case SPECBOS_USERFILE_CHSUM:
			return "Userfile checksum";
		case SPECBOS_USERFILE2_CHSUM:
			return "Userfile2 checksum";
		case SPECBOS_USERFILE2_ARG:
			return "Userfile2 argument";
		case SPECBOS_OVEREXPOSED:
			return "Overexposure";
		case SPECBOS_UNDEREXPOSED:
			return "Underexposure";
		case SPECBOS_ADAPT_INT_TIME:
			return "Adaption integration time";
		case SPECBOS_NO_SHUTTER:
			return "Shutter doesn't exist";
		case SPECBOS_NO_DARK_MEAS:
			return "No dark measurement";
		case SPECBOS_NO_REF_MEAS:
			return "No reference measurement";
		case SPECBOS_NO_TRANS_MEAS:
			return "No transmission measurement";
		case SPECBOS_NO_RADMTRC_CALC:
			return "No radiometric calculation";
		case SPECBOS_NO_CCT_CALC:
			return "No CCT calculation";
		case SPECBOS_NO_CRI_CALC:
			return "No CRI calculation";
		case SPECBOS_NO_DARK_COMP:
			return "No dark compensation";
		case SPECBOS_NO_LIGHT_MEAS:
			return "No light measurement";
		case SPECBOS_NO_PEAK_CALC:
			return "No peak calculation";
		case SPECBOS_CAL_DATA:
			return "Calibration data";
		case SPECBOS_EXCEED_CAL_WL:
			return "Exceeded calibration wavelength";
		case SPECBOS_SCAN_BREAK:
			return "Scan break";
		case SPECBOS_TO_CYC_OPT_TRIG:
			return "Timeout cycle on optical trigger";
		case SPECBOS_DIV_CYC_TIME:
			return "Divider cycle time";
		case SPECBOS_WRITE_FLASH_PARM:
			return "Write parameter to flash";
		case SPECBOS_READ_FLASH_PARM:
			return "Read parameter from flash";
		case SPECBOS_FLASH_ERASE:
			return "Erase flash";
		case SPECBOS_NO_CALIB_FILE:
			return "No calibration file";
		case SPECBOS_CALIB_FILE_HEADER:
			return "Calibration file header";
		case SPECBOS_WRITE_CALIB_FILE:
			return "Write calibration file";
		case SPECBOS_CALIB_FILE_VALS:
			return "Calibration file values";
		case SPECBOS_CALIB_FILE_NO:
			return "Calibration file number";
		case SPECBOS_CLEAR_CALIB_FILE:
			return "Clear calibration file";
		case SPECBOS_CLEAR_CALIB_ARG:
			return "Clear calibration file argument";
		case SPECBOS_NO_LAMP_FILE:
			return "No lamp file";
		case SPECBOS_LAMP_FILE_HEADER:
			return "Lamp file header";
		case SPECBOS_WRITE_LAMP_FILE:
			return "Write lamp file";
		case SPECBOS_LAMP_FILE_VALS:
			return "Lamp file values";
		case SPECBOS_LAMP_FILE_NO:
			return "Lamp file number";
		case SPECBOS_CLEAR_LAMP_FILE:
			return "Clear lamp file";
		case SPECBOS_CLEAR_LAMP_FILE_ARG:
			return "Clear lamp file argument";
		case SPECBOS_RAM_CHECK:
			return "RAM check";
		case SPECBOS_DATA_OUTPUT:
			return "Data output";
		case SPECBOS_RAM_LOW:
			return "Insufcient RAM";
		case SPECBOS_FIRST_MEM_ALLOC:
			return "First memory allocation";
		case SPECBOS_SECOND_MEM_ALLOC:
			return "Second memory allocation";
		case SPECBOS_THIRD_MEM_ALLOC:
			return "Third memory allocation";
		case SPECBOS_RADMTRC_WL_RANGE:
			return "Wavelength range for radiometric calculation";
		case SPECBOS_BOOT_BAT_POWER:
			return "Boot by battery power";
		case SPECBOS_TRIG_CONF_1:
			return "Trigger configuration 1";
		case SPECBOS_TRIG_CONF_2:
			return "Trigger configuration 2";

		case SPECBOS_INT_THREADFAILED:
			return "Starting diffuser position thread failed";

		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
specbos_interp_code(inst *pp, int ec) {

	ec &= inst_imask;
	switch (ec) {

		case SPECBOS_OK:
			return inst_ok;

		case SPECBOS_INTERNAL_ERROR:
		case SPECBOS_INT_THREADFAILED:
			return inst_internal_error | ec;

		case SPECBOS_TIMEOUT:
		case SPECBOS_COMS_FAIL:
			return inst_coms_fail | ec;

		case SPECBOS_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case SPECBOS_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

		case SPECBOS_SPOS_EMIS:
		case SPECBOS_SPOS_AMB:
			return inst_wrong_config | ec;

		case SPECBOS_COMMAND:
		case SPECBOS_DIGIT:
		case SPECBOS_ARG_1:
		case SPECBOS_ARG_2:
		case SPECBOS_ARG_3:
		case SPECBOS_ARG_4:
		case SPECBOS_PARAM:
		case SPECBOS_CONFIG_ARG:
		case SPECBOS_CONTROL_ARG:
		case SPECBOS_READ_ARG:
		case SPECBOS_FETCH_ARG:
		case SPECBOS_MEAS_ARG:
		case SPECBOS_CALC_ARG:
		case SPECBOS_CAL_ARG:
			return inst_bad_parameter | ec;

		case SPECBOS_OVEREXPOSED:
		case SPECBOS_UNDEREXPOSED:
			return inst_misread | ec;

		case SPECBOS_PASSWORD:
		case SPECBOS_PARAM_CHSUM:
		case SPECBOS_USERFILE_CHSUM:
		case SPECBOS_USERFILE2_CHSUM:
		case SPECBOS_USERFILE2_ARG:
		case SPECBOS_ADAPT_INT_TIME:
		case SPECBOS_NO_SHUTTER:
		case SPECBOS_NO_DARK_MEAS:
		case SPECBOS_NO_REF_MEAS:
		case SPECBOS_NO_TRANS_MEAS:
		case SPECBOS_NO_RADMTRC_CALC:
		case SPECBOS_NO_CCT_CALC:
		case SPECBOS_NO_CRI_CALC:
		case SPECBOS_NO_DARK_COMP:
		case SPECBOS_NO_LIGHT_MEAS:
		case SPECBOS_NO_PEAK_CALC:
		case SPECBOS_CAL_DATA:
		case SPECBOS_EXCEED_CAL_WL:
		case SPECBOS_SCAN_BREAK:
		case SPECBOS_TO_CYC_OPT_TRIG:
		case SPECBOS_DIV_CYC_TIME:
		case SPECBOS_WRITE_FLASH_PARM:
		case SPECBOS_READ_FLASH_PARM:
		case SPECBOS_FLASH_ERASE:
		case SPECBOS_NO_CALIB_FILE:
		case SPECBOS_CALIB_FILE_HEADER:
		case SPECBOS_WRITE_CALIB_FILE:
		case SPECBOS_CALIB_FILE_VALS:
		case SPECBOS_CALIB_FILE_NO:
		case SPECBOS_CLEAR_CALIB_FILE:
		case SPECBOS_CLEAR_CALIB_ARG:
		case SPECBOS_NO_LAMP_FILE:
		case SPECBOS_LAMP_FILE_HEADER:
		case SPECBOS_WRITE_LAMP_FILE:
		case SPECBOS_LAMP_FILE_VALS:
		case SPECBOS_LAMP_FILE_NO:
		case SPECBOS_CLEAR_LAMP_FILE:
		case SPECBOS_CLEAR_LAMP_FILE_ARG:
		case SPECBOS_RAM_CHECK:
		case SPECBOS_DATA_OUTPUT:
		case SPECBOS_RAM_LOW:
		case SPECBOS_FIRST_MEM_ALLOC:
		case SPECBOS_SECOND_MEM_ALLOC:
		case SPECBOS_THIRD_MEM_ALLOC:
		case SPECBOS_RADMTRC_WL_RANGE:
		case SPECBOS_BOOT_BAT_POWER:
		case SPECBOS_TRIG_CONF_1:
		case SPECBOS_TRIG_CONF_2:
			return inst_hardware_fail | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
specbos_del(inst *pp) {
	if (pp != NULL) {
		specbos *p = (specbos *)pp;
		if (p->th != NULL) {		/* Terminate diffuser monitor thread  */
			int i;
			p->th_term = 1;			/* Tell thread to exit on error */
			for (i = 0; p->th_termed == 0 && i < 5; i++)
				msec_sleep(100);	/* Wait for thread to terminate */
			if (i >= 5) {
				a1logd(p->log,3,"specbos diffuser thread termination failed\n");
			}
			p->th->del(p->th);
		}
		if (p->icom != NULL)
			p->icom->del(p->icom);
		amutex_del(p->lock);
		free(p);
	}
}

/* Return the instrument mode capabilities */
void specbos_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	specbos *p = (specbos *)pp;
	inst_mode cap1 = 0;
	inst2_capability cap2 = 0;

	cap1 |= inst_mode_emis_tele
	     |  inst_mode_ambient
	     |  inst_mode_colorimeter
	     |  inst_mode_spectral
	     |  inst_mode_emis_refresh_ovd
	     |  inst_mode_emis_norefresh_ovd
	        ;

	/* can inst2_has_sensmode, but not report it asynchronously */
	cap2 |= inst2_prog_trig
	     |  inst2_user_trig
	     |  inst2_disptype
	     |  inst2_has_target	/* Has a laser target */
	        ;

	if (p->model != 1201) {
		cap2 |= inst2_emis_refr_meas;
		cap2 |= inst2_set_refresh_rate;
		cap2 |= inst2_get_refresh_rate;
	}

	if (pcap1 != NULL)
		*pcap1 = cap1;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = inst3_none;
}

/* Return current or given configuration available measurement modes. */
static inst_code specbos_meas_config(
inst *pp,
inst_mode *mmodes,
inst_cal_cond *cconds,
int *conf_ix
) {
	specbos *p = (specbos *)pp;
	inst_code ev;
	inst_mode mval;
	int pos;

	if (mmodes != NULL)
		*mmodes = inst_mode_none;
	if (cconds != NULL)
		*cconds = inst_calc_unknown;

	if (conf_ix == NULL
	 || *conf_ix < 0
	 || *conf_ix > 1) {
		/* Return current configuration measrement modes */
		amutex_lock(p->lock);
		if ((ev = specbos_get_diffpos(p, &pos, 0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
		amutex_unlock(p->lock);
	} else {
		/* Return given configuration measurement modes */
		pos = *conf_ix;
	}

	if (pos == 1) {
		mval = inst_mode_emis_ambient;
	} else if (pos == 0) {
		mval |= inst_mode_emis_tele;
	}

	/* Add the extra dependent and independent modes */
	mval |= inst_mode_emis_refresh_ovd
	     |  inst_mode_emis_norefresh_ovd
	     |  inst_mode_colorimeter
	     |  inst_mode_spectral;

	if (mmodes != NULL)	
		*mmodes = mval;

	/* Return configuration index returned */
	if (conf_ix != NULL)
		*conf_ix = pos;

	return inst_ok;
}

/* Check device measurement mode */
inst_code specbos_check_mode(inst *pp, inst_mode m) {
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
	if (!IMODETST(m, inst_mode_emis_tele)
	 && !IMODETST(m, inst_mode_emis_ambient)) {
		return inst_unsupported;
	}

	return inst_ok;
}

/* Set device measurement mode */
inst_code specbos_set_mode(inst *pp, inst_mode m) {
	specbos *p = (specbos *)pp;
	int refrmode;
	inst_code ev;

	if ((ev = specbos_check_mode(pp, m)) != inst_ok)
		return ev;

	p->mode = m;


	if (p->model != 1201) {		/* Can't set refresh mode on 1201 */

		/* Effective refresh mode may change */
		refrmode = p->refrmode;
		if (     IMODETST(p->mode, inst_mode_emis_norefresh_ovd)) {	/* Must test this first! */
			refrmode = 0;
		} else if (IMODETST(p->mode, inst_mode_emis_refresh_ovd)) {
			refrmode = 1;
		}
		
		if (p->refrmode != refrmode) {
			p->rrset = 0;					/* This is a hint we may have swapped displays */
			p->refrvalid = 0;
		}
		p->refrmode = refrmode; 
	}

	return inst_ok;
}

inst_disptypesel specbos_disptypesel[3] = {
	{
		inst_dtflags_default,
		1,
		"nl",
		"Non-Refresh display",
		0,
		0
	},
	{
		inst_dtflags_none,			/* flags */
		2,							/* cbid */
		"rc",						/* sel */
		"Refresh display",			/* desc */
		1,							/* refr */
		1							/* ix */
	},
	{
		inst_dtflags_end,
		0,
		"",
		"",
		0,
		0
	}
};

/* Get mode and option details */
static inst_code specbos_get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	specbos *p = (specbos *)pp;
	inst_code rv = inst_ok;

	if ((!allconfig && (p->mode & inst_mode_ambient))		/* If set to Ambient */
	 || p->model == 1201) {			/* Or 1201, return empty list */

		if (pnsels != NULL)
			*pnsels = 0;
	
		if (psels != NULL)
			*psels = NULL;

		return inst_ok;
	}


	if (pnsels != NULL)
		*pnsels = 2;

	if (psels != NULL)
		*psels = specbos_disptypesel;

	return inst_ok;
}

/* Given a display type entry, setup for that type */
static inst_code set_disp_type(specbos *p, inst_disptypesel *dentry) {
	inst_code rv;
	int refrmode;

	refrmode = dentry->refr;

	a1logd(p->log,5,"specbos set_disp_type refmode %d\n",refrmode);

	if (     IMODETST(p->mode, inst_mode_emis_norefresh_ovd)) {	/* Must test this first! */
		refrmode = 0;
	} else if (IMODETST(p->mode, inst_mode_emis_refresh_ovd)) {
		refrmode = 1;
	}

	if (p->refrmode != refrmode)
		p->rrset = 0;					/* This is a hint we may have swapped displays */
	p->refrmode = refrmode; 

	amutex_lock(p->lock);
	if ((rv = specbos_imp_set_refresh(p)) != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}
	amutex_unlock(p->lock);

	return inst_ok;
}

/* Set the display type */
static inst_code specbos_set_disptype(inst *pp, int ix) {
	specbos *p = (specbos *)pp;
	inst_code ev;
	inst_disptypesel *dentry;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->model == 1201)			/* No display type to select on 1201 */
		return inst_unsupported;

	if (ix < 0 || ix >= 2)
		return inst_unsupported;

	a1logd(p->log,5,"specbos  specbos_set_disptype ix %d\n",ix);
	dentry = &specbos_disptypesel[ix];

	if ((ev = set_disp_type(p, dentry)) != inst_ok) {
		return ev;
	}

	return inst_ok;
}

/* 
 * set or reset an optional mode
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 */
static inst_code
specbos_get_set_opt(inst *pp, inst_opt_type m, ...)
{
	specbos *p = (specbos *)pp;
	char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;

	a1logd(p->log, 5, "specbos_get_set_opt: opt type 0x%x\n",m);

	/* Record the trigger mode */
	if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_user) {
		p->trig = m;
		return inst_ok;
	}

	/* Set laser target state */
	if (m == inst_opt_set_target_state) {
		va_list args;
		int state = 0;

		va_start(args, m);
		state = va_arg(args, int);
		va_end(args);

		amutex_lock(p->lock);
		if (state == 2) { 
			int lstate = -1;
			if ((ev = specbos_command(p, "*contr:laser?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				amutex_unlock(p->lock);
				a1loge(p->log, 1, "specbos_get_set_opt: failed to send laser? command\n");
				return ev;
			}
			if (sscanf(buf, "laser: %d ",&lstate) != 1) {
				amutex_unlock(p->lock);
				a1loge(p->log, 1, "specbos_get_set_opt: failed to parse laser state\n");
				return ev;
			}
			a1logd(p->log, 5, " Laser state = %d\n",lstate);
			if (lstate == 0)
				lstate = 1;
			else if (lstate == 1)
				lstate = 0;
			if (lstate == 0 || lstate == 1) {
				char mes[100];
				sprintf(mes,"*contr:laser %d\r",lstate);
				if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
					amutex_unlock(p->lock);
					return ev;
				}
			}
		} else if (state == 1) { 
			if ((ev = specbos_command(p, "*contr:laser 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				amutex_unlock(p->lock);
				return ev;
			}
		} else if (state == 0) {
			if ((ev = specbos_command(p, "*contr:laser 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				amutex_unlock(p->lock);
				return ev;
			}
		}
		amutex_unlock(p->lock);
		return inst_ok;
	}

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	return inst_unsupported;
}

/* Constructor */
extern specbos *new_specbos(icoms *icom, instType itype) {
	specbos *p;
	if ((p = (specbos *)calloc(sizeof(specbos),1)) == NULL) {
		a1loge(icom->log, 1, "new_specbos: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	p->init_coms         = specbos_init_coms;
	p->init_inst         = specbos_init_inst;
	p->capabilities      = specbos_capabilities;
	p->meas_config       = specbos_meas_config;
	p->check_mode        = specbos_check_mode;
	p->set_mode          = specbos_set_mode;
	p->get_disptypesel   = specbos_get_disptypesel;
	p->set_disptype      = specbos_set_disptype;
	p->get_set_opt       = specbos_get_set_opt;
	p->read_sample       = specbos_read_sample;
	p->read_refrate      = specbos_read_refrate;
	p->get_n_a_cals      = specbos_get_n_a_cals;
	p->calibrate         = specbos_calibrate;
	p->get_refr_rate     = specbos_get_refr_rate;
	p->set_refr_rate     = specbos_set_refr_rate;
	p->interp_error      = specbos_interp_error;
	p->del               = specbos_del;

	p->icom = icom;
	p->itype = icom->itype;
	if (p->itype == instSpecbos1201)
		p->model = 1201;

	amutex_init(p->lock);

	return p;
}

