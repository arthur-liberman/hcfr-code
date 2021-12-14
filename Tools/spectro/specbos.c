
/* 
 * Argyll Color Management System
 *
 * JETI specbos & spectraval related functions
 *
 * Author: Graeme W. Gill
 * Date:   13/3/2013
 *
 * Copyright 1996 - 2014, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Based on DTP92.c
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

	TTBD:

	Should add a reflective mode,
	by doing a white calibration and dividing the measurement.

	Should check transmissive white spectral quality

	Should save transmissive white cal. into file, and restore it on startup.

	Should time transmissive white cal out.

	Should add support for inst_mode_emis_nonadaptive.

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
#include "specbos.h"

static inst_code specbos_interp_code(inst *pp, int ec);

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 8000		/* Maximum reading message reply size */

#define DEFAULT_TRANS_NAV 10	/* Default transmission mode number of averages */
#define DEFAULT_NAV 1			/* Default other mode number of averages */

#define EXPL1211AVG				/* [def] Intermix dark calibration when averaging */

/* Interpret an icoms error into a SPECBOS error */
static int icoms2specbos_err(int se) {
	if (se != ICOM_OK) {
		if (se & ICOM_TO)
			return SPECBOS_TIMEOUT; 
		return SPECBOS_COMS_FAIL;
	}
	return SPECBOS_OK;
}

/* Type of reply terminator expected */
typedef enum {
    tnorm = 0,			/* Normal terminators */
    tmeas = 1,			/* Measurement command */
    trefr = 2,			/* Refresh measurement */
    tspec = 3			/* Spectraval spectral read */
} spterm;

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
spterm ctype,		/* Exected reply terminator type */
int nd				/* nz to disable debug messages */
) {
	int se;
	int bread = 0;
	char *cp, *tc = "", *dp;

	if (ctype == tnorm)
		tc = "\r\006\025";		/* Return, Ack or Nak */
	else if (ctype == tmeas)
		tc = "\007\025";		/* Bell or Nak */
	else if (ctype == trefr)
		tc = "\r\025";			/* Return or Nak */
	else if (ctype == tspec)
		tc = "\003\025";		/* Atx or Nak */

	se = p->icom->write_read_ex(p->icom, in, 0, out, bsize, &bread, tc, ntc, to, 1);

	/* Because we are sometimes waiting for 3 x \r characters to terminate the read, */
	/* we will instead time out on getting a single NAK (\025), so ignore timout */
	/* if we got a NAK. */
	if (se == ICOM_TO && bread > 0 && out[0] == '\025')
		se = ICOM_OK;

	if (se != 0) {
		if (!nd) a1logd(p->log, 1, "specbos_fcommand: serial i/o failure on write_read '%s' 0x%x\n",icoms_fix(in),se);
		return icoms2specbos_err(se);
	}

	/* Over Bluetooth, we get an erroneous string "AT+JSCR\r\n" mixed in our output. */
	/* This would appear to be from the eBMU Bluetooth adapter AT command set. */
	if (bread > 9 && strncmp(out, "AT+JSCR\r\n", 9) == 0) {
		a1logd(p->log, 8, "specbos: ignored 'AT+JSCR\\r\\n' response\n");
		memmove(out, out+9, bsize-9);
		bread -= 9;
	}
	if (bread > 8 && strncmp(out, "AT+JSCR\r", 8) == 0) {
		a1logd(p->log, 8, "specbos: ignored 'AT+JSCR\\r' response\n");
		memmove(out, out+8, bsize-8);
		bread -= 8;
	}

	/* See if there was an error, and remove any enquire codes */
	for (dp = cp = out; *cp != '\000' && (dp - out) < bsize; cp++) {
		if (*cp == '\025') {	/* Got a NAK */
			char buf[100];

			if ((se = p->icom->write_read(p->icom, "*stat:err?\r", 0, buf, 100, NULL, "\r", 1, 1.0)) != 0) {
				if (!nd) a1logd(p->log, 1, "specbos_fcommand: serial i/o failure on write_read '%s'\n",icoms_fix(in));
				return icoms2specbos_err(se);;
			}
			if (p->model == 1501 || p->model == 1511) {
				if (sscanf(buf, "%d ",&se) != 1) {
					if (!nd) a1logd(p->log, 1, "specbos_fcommand: failed to parse error code '%s'\n",icoms_fix(buf));
					return SPECBOS_DATA_PARSE_ERROR;
				}
			} else {
				if (sscanf(buf, "Error Code: %d ",&se) != 1) {
					if (!nd) a1logd(p->log, 1, "specbos_fcommand: failed to parse error code '%s'\n",icoms_fix(buf));
					return SPECBOS_DATA_PARSE_ERROR;
				}
			}
					
			if (!nd) a1logd(p->log, 1, "Got specbos error code %d\n",se);
			break;
		}
		if (*cp == '\005')	/* Got an Enquire */
			continue;		/* remove it */
		*dp = *cp;
		dp++;
	}
	out[bsize-1] = '\000';

	if (!nd) a1logd(p->log, 4, "specbos_fcommand: command '%s' returned '%s' bytes %d, err 0x%x\n",
	                                          icoms_fix(in), icoms_fix(out),strlen(out), se);
	return se;
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
	int rv = specbos_fcommand(p, in, out, bsize, to, 1, tnorm, 0);
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

	if ((se = p->icom->read(p->icom, out, bsize, NULL, tc, 1, to)) != 0) {
		a1logd(p->log, 1, "specbos_readresp: serial i/o failure\n");
		return icoms2specbos_err(se);
	}
	return inst_ok;
}

/* Establish communications with a specbos */
/* Return SPECBOS_COMS_FAIL on failure to establish communications */
static inst_code
specbos_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	specbos *p = (specbos *) pp;
	char buf[MAX_MES_SIZE];
	baud_rate brt[] = { baud_921600, baud_115200, baud_38400, baud_nc };
// spectraval 38400, 115200, 230400, 921600, 3000000

	unsigned int etime;
	unsigned int len, i;
	instType dtype = pp->dtype;
	int se;

	inst_code ev = inst_ok;
	int val;

	a1logd(p->log, 2, "specbos_init_coms: About to init Serial I/O\n");


	if (!(p->icom->dctype & icomt_serial)
	 && !(p->icom->dctype & icomt_fastserial)) {
		a1logd(p->log, 1, "specbos_init_coms: wrong communications type for device! (dctype 0x%x)\n",p->icom->dctype);
		return inst_coms_fail;
	}
	
	/* Communications has already been established over BlueTooth serial */
	if (p->bt) {

		amutex_lock(p->lock);

		/* Let instrument get its act together */
		msec_sleep(600);

		/* Get the instrument identification */
		if ((ev = specbos_command(p, "*idn?\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok) {
			amutex_unlock(p->lock);
			a1logd(p->log, 2, "specbos_init_coms: idn didn't return\n");
			return ev;
		}

		msec_sleep(600);

	/* We need to setup communications */
	} else {
		int delayms = 0;
		amutex_lock(p->lock);

		/* The tick to give up on */
		etime = msec_time() + (long)(1500.0 + 0.5);

		a1logd(p->log, 1, "specbos_init_coms: Trying different baud rates (%u msec to go)\n",etime - msec_time());

		/* Spectraval Bluetooth serial doesn't seem to actuall function */
		/* until 600msec after it is opened. We get an erroneous "AT+JSCR\r\n" reply */
		/* within that time, and it won't re-open after closing. */
		if (p->icom->dctype & icomt_btserial)
			delayms = 600;

		/* Until we time out, find the correct baud rate */
		for (i = 0; msec_time() < etime; i++) {
			if (brt[i] == baud_nc) {
				i = 0;
			}

			/* Only connect at 115200 if bluetooth */
			if ((p->icom->dctype & icomt_btserial) != 0 && brt[i] != baud_115200)
				continue;
				
			a1logd(p->log, 5, "specbos_init_coms: Trying %s baud, %d msec to go\n",
				                      baud_rate_to_str(brt[i]), etime- msec_time());
			if ((se = p->icom->set_ser_port_ex(p->icom, fc_None, brt[i], parity_none,
				                         stop_1, length_8, delayms)) != ICOM_OK) { 
				amutex_unlock(p->lock);
				a1logd(p->log, 5, "specbos_init_coms: set_ser_port failed with 0x%x\n",se);
				return specbos_interp_code((inst *)p, icoms2specbos_err(se));;		/* Give up */
			}

//			if ((p->icom->sattr & icom_bt) != 0)
//				specbos_command(p, "\r", buf, MAX_MES_SIZE, 0.5);

			/* Check instrument is responding */
			if (((ev = specbos_command(p, "*idn?\r", buf, MAX_MES_SIZE, 0.5)) & inst_mask)
				                                                       != inst_coms_fail) {
				goto got_coms;		/* We've got coms or user abort */
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

		/* We haven't established comms */
		amutex_unlock(p->lock);
		a1logd(p->log, 2, "specbos_init_coms: failed to establish coms\n");
		return inst_coms_fail;

  got_coms:;
	}

	/* Check the response */
	len = strlen(buf);
	p->model = 0;

	if (len >= 4 && strncmp(buf, "SB05", 4) == 0) {
		p->model = 1201;
	} else if ((len >= 10 && strncmp(buf, "JETI_SDCM3", 10) == 0)
	        || (len >= 9  && strncmp(buf, "DCM3_JETI", 9) == 0)
			|| (len >= 17  && strncmp(buf, "PECFIRM_JETI_1501", 17) == 0)
			|| (len >= 18  && strncmp(buf, "SPECFIRM_JETI_1501", 18) == 0)) {
		p->model = 1501;
	} else {
		if (len < 11
		 || sscanf(buf, "JETI_SB%d\r", &p->model) != 1) {
			amutex_unlock(p->lock);
			a1logd(p->log, 2, "specbos_init_coms: unrecognised ident string '%s'\n",icoms_fix(buf));
			return inst_protocol_error;
		}
	}

	if (p->model != 1201
	 && p->model != 1211
	 && p->model != 1501) {
		amutex_unlock(p->lock);
		a1logd(p->log, 2, "specbos_init_coms: unrecognised model %04d\n",p->model);
		return inst_unknown_model;
	}
	a1logd(p->log, 2, "specbos_init_coms: init coms has suceeded\n");

	/* See if it's a 1501 or 1511 */
	if (p->model == 1501) {
		int i;
		int dispen = 0;

		/* Try a few times because of BT misbehaviour */
		for (i = 0; i < 3; i++) {
			if ((ev = specbos_command(p, "*conf:dispen?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				if (i < 3)
					continue;
				amutex_unlock(p->lock);
				a1logd(p->log, 2, "specbos_init_coms: failed to get display status\n");
				return inst_protocol_error;
			}
			
			if (sscanf(buf, "%d ",&dispen) != 1) {
				if (i < 3)
					continue;
				amutex_unlock(p->lock);
				a1loge(p->log, 1, "specbos_init_inst: failed to parse display status\n");
				return specbos_interp_code((inst *)p, SPECBOS_DATA_PARSE_ERROR);
			} else {
				break;
			}
		}

		a1logd(p->log, 1, " spectraval %s display\n",dispen ? "has" : "doesn't have");
		if (dispen) {
			p->model = 1511;

			/* Set remote mode (for old firmare) */
			if ((ev = specbos_command(p, "*REMOTE 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				a1logd(p->log, 2, "specbos_init_coms: remote command failed (newer firmware ?)\n");
			}
		}

		/* Check Bluetooth status */
		if ((ev = specbos_command(p, "*conf:bten?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			a1logd(p->log, 2, "specbos_init_coms: failed to get Bluetooth status\n");
			return inst_protocol_error;
		}
	}

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

	if ((ev = specbos_command(p, "*para:spnum?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	if (sscanf(buf, "spectrometer number: %d ",&val) == 1
	 || sscanf(buf, "%d ",&val) == 1) {
		a1logv(p->log, 1, " Spectrometer serial number: %d\n",val);
		p->serno = val;
	} else {
		a1logv(p->log, 1, " Failed to parse serial number\n");
		p->serno = -1;
	}

	p->gotcoms = 1;

	amutex_unlock(p->lock);
	return inst_ok;
}

/*
	Notes on commands for 1201:

	*conf is temporary, *para can be saved to instrument
	Since we set the instrument up every time, use *conf,
	but use *para for things we change temporarily.

	The 15x1 only has *para (*conf is for external HW)

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
static inst_code specbos_get_target_laser(specbos *p, int *laser, int nd);

/* Diffuser position and laser state thread. */
/* Poll the instrument at 500msec intervals */
int specbos_diff_thread(void *pp) {
	int nfailed = 0;
	specbos *p = (specbos *)pp;
	inst_code rv1 = inst_ok; 
	inst_code rv2 = inst_ok; 
	a1logd(p->log,3,"Diffuser thread started\n");
//	for (nfailed = 0; nfailed < 5;)
	/* Try indefinitely, in case instrument is put to sleep */
	for (;;) {
		int pos;

		amutex_lock(p->lock);
		if (p->model != 1501 && p->model != 1511)
			rv1 = specbos_get_diffpos(p, &pos, 1); 
		rv2 = specbos_get_target_laser(p, &p->laser, 1); 
		amutex_unlock(p->lock);

		if (p->th_term) {
			p->th_termed = 1;
			break;
		}
		if (rv1 != inst_ok || rv2 != inst_ok) {
			nfailed++;
			a1logd(p->log,3,"Diffuser thread failed with 0x%x 0x%x\n",rv1,rv2);
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
	return rv1 != inst_ok ? rv1 : rv2;
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
	
	if (p->model != 1501 && p->model != 1511) {
		/* Restore the instrument to it's default settings */
		if ((ev = specbos_command(p, "*conf:default\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok)
			return ev;
	}

	/* Set calibration type to auto on ambient cap */
	if ((ev = specbos_command(p, "*para:calibn 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	if (p->model == 1501 || p->model == 1511) {
		/* Set auto exposure/integration time */
		if ((ev = specbos_command(p, "*para:tint 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
	} else {

		/* Set auto exposure/integration time */
		if ((ev = specbos_command(p, "*para:expo 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok
//       this would have no effect:
//		 || (ev = specbos_command(p, "*para:adapt 2\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok
		) {
			amutex_unlock(p->lock);
			return ev;
		}

		/* Note: to set fixed int time, need to set conf:expo 2 & conf:adap 0 & conf:tint XX */

		/* Should we reset *para:bord to 70 98 ?? */
		/* What about mintint and maxtint ?? */
	}

	/* Set target maximum measure time (no averaging) */
	/* (We will then setup instrument to compy with this target) */
	/* 3.6 is the assumed maximum fixed overhead */
	p->measto = 20.0;		/* Set default. Specbos default is 60.0 */

	if (p->model == 1211)
		p->measto = 6.0 + 3.6;		/* Aprox. Same overall time as i1d3 ?? */
	else if (p->model == 1201)
		p->measto = 16.0 + 3.6;
	else if (p->model == 1501 || p->model == 1511)
		p->measto = 6.0 + 3.6;

	/* Implement max auto int. time, to speed up display measurement */
	if (p->model == 1501 || p->model == 1511) {
#ifdef NEVER		/* Bound auto by integration time (worse for inttime > 1.0) */ 
		int maxaver = 2;	/* Maximum averages for auto int time */
		double dmaxtint;
		int maxtint;

		/* Actual time taken depends on maxtint, autoavc & fudge factor. */
		dmaxtint = (p->measto - 3.6)/(2.0 * maxaver);

		//printf("dmaxtint %f\n",dmaxtint);

		maxtint = (int)(dmaxtint * 1000.0+0.5);

		if (maxtint < 1000 || maxtint > 64999) {
			warning("specbos: assert, maxtint %d out of range",maxtint);
			if (maxtint < 1000)
				maxtint = 1000;
			else if (maxtint > 64999)
				maxtint = 64999;
		}

		/* Set maximum integration time */
		sprintf(mes, "*para:maxtint %d\r", maxtint);
		if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}

		/* Set maximum number of auto averages. Min value is 2 */
		sprintf(mes, "*para:maxaver %d\r", maxaver);
		if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
#else	/* Bound auto by no. averages (better - limit maxtint to 1.0) */
		double dmaxtint = 1.0;		/* Recommended maximum is 1.0 */
		int maxtint;
		int maxaver;	/* Maximum averages for auto int time */

		/* Set the max integration time to 1.0 seconds: */
		maxtint = (int)(dmaxtint * 1000.0+0.5);

		if (maxtint < 1000 || maxtint > 64999) {
			warning("specbos: assert, maxtint %d out of range",maxtint);
			if (maxtint < 1000)
				maxtint = 1000;
			else if (maxtint > 64999)
				maxtint = 64999;
		}

		sprintf(mes, "*para:maxtint %d\r", maxtint);
		if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}

		/* Then compute the maximum number of measurements to meet measto: */

		/* Total time = overhead + initial sample + 2 * int time per measure */
		maxaver = (int)ceil((p->measto - 3.6)/(2.0 * dmaxtint));

		if (maxaver < 2) {
			warning("specbos: assert, maxaver %d out of range",maxaver);
			maxaver = 2;
		}

		a1logd(p->log, 6, "specbos_init_inst: set maxaver %d\n",maxaver);

		/* Set maximum number of auto averages. Min value is 2 */
		sprintf(mes, "*para:maxaver %d\r", maxaver);
		if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
#endif

		/* spectraval doesn't support *fetch:XYZ command */
		p->noXYZ  = 1;

	} else {
		double dmaxtin;
		int maxtin;

		/* Total time = overhead + initial sample + 2 * int time per measure */
		dmaxtin = (p->measto - 3.6)/2.0;		/* Fudge factor */

		maxtin = (int)(dmaxtin * 1000.0+0.5);

		if (maxtin < 1000 || maxtin > 64999) {
			warning("specbos: assert, maxtint %d out of range",maxtin);
			if (maxtin < 1000)
				maxtin = 1000;
			else if (maxtin > 60000)		/* 60 secs according to auxiliary doco. */
				maxtin = 60000;
		}

		/* Set maximum integration time that auto will use */
		sprintf(mes, "*conf:maxtin %d\r", maxtin);
		if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			/* 1201 Firmware V1.8.3 and earlier doesn't support maxtin. */
			/* Ignore error with a warning. */
			if (p->model == 1201) {
				if (!p->maxtin_warn)
					warning("specbos: conf:maxtin %d command failed (Old Firmware ?)",maxtin);
				p->maxtin_warn = 1;

			/* Fatal error otherwise */
			} else {
				amutex_unlock(p->lock);
				return ev;
			}
		}

#ifdef NEVER	/* Use default */
		/* Set split time to limit dark current error for long integration times */
		sprintf(mes, "*para:splitt %d\r", 1000);
		if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
#endif

	}

	if (p->model == 1501 || p->model == 1511) {
		int i;
		int wstart, wend, wstep;

		/* Try several times due to BT problems */
		for (i = 0; i < 3; i++) {
			/* Set the measurement format to None. We will read measurement manually. */
			/* (0 = None, 1 = Binary, 2 = ASCII) */
			if ((ev = specbos_command(p, "*para:form 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				if (i < 3)
					continue;
				amutex_unlock(p->lock);
				return ev;
			}

			if ((ev = specbos_command(p, "*para:wran?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				if (i < 3)
					continue;
				amutex_unlock(p->lock);
				return ev;
			}
			if (sscanf(buf, "%d %d %d",&wstart,&wend,&wstep) != 3) {
				if (i < 3)
					continue;
				amutex_unlock(p->lock);
				a1loge(p->log, 1, "specbos_init_inst: failed to parse wavelength range\n");
				return specbos_interp_code((inst *)p, SPECBOS_DATA_PARSE_ERROR);

			} else {
				break;
			}
		}

		p->wl_short = (double)wstart;
		p->wl_long = (double)wend;

		a1logd(p->log, 1, " Short wl range %f\n",p->wl_short);
	
		if (p->wl_long > 830.0)
			p->wl_long = 830.0;			/* Limit it to useful visual range */
	
		a1logd(p->log, 1, " Long wl range %f\n",p->wl_long);

		p->nbands = (int)((p->wl_long - p->wl_short + 1.0)/1.0 + 0.5);
	
		/* Set the wavelength range and resolution */
		sprintf(mes, "*para:wran %d %d 1\r", (int)(p->wl_short+0.5), (int)(p->wl_long+0.5));
		if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}

		/* Set to average just 1 reading */
		if ((ev = specbos_command(p, "*para:aver 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
		p->setnav = 1;

	} else {

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
			return specbos_interp_code((inst *)p, SPECBOS_DATA_PARSE_ERROR);
		}
		a1logd(p->log, 1, " Short wl range %f\n",p->wl_short);
	
		if ((ev = specbos_command(p, "*para:wavend?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
		if (sscanf(buf, "Predefined end wave: %lf ",&p->wl_long) != 1) {
			amutex_unlock(p->lock);
			a1loge(p->log, 1, "specbos_init_inst: failed to parse end wave\n");
			return specbos_interp_code((inst *)p, SPECBOS_DATA_PARSE_ERROR);
		}
		if (p->wl_long > 830.0)			/* Could go to 1000 with 1211 */
			p->wl_long = 830.0;			/* Limit it to useful visual range */
	
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
		p->setnav = 1;
	}

	if (p->log->verb) {
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
		
		if (p->serno != -1) {
			a1logv(p->log, 1, " Spectrometer serial number: %d\n",p->serno);
		} else {
			a1logv(p->log, 1, " Failed to parse serial number\n");
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

static inst_code specbos_imp_measure_set_refresh(specbos *p);
static inst_code specbos_imp_set_refresh(specbos *p);

/* Get the ambient diffuser position */
/* (Not valid for 1501 or 1511) */ 
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
	if ((ec = specbos_fcommand(p, "*contr:mhead?\r", buf, MAX_MES_SIZE, 1.0, 1, tnorm, nd)) != inst_ok) {
		return specbos_interp_code((inst *)p, ec);
	}
	if (sscanf(buf, "mhead: %d ",pos) != 1) {
		a1logd(p->log, 2, "specbos_init_coms: unrecognised measuring head string '%s'\n",icoms_fix(buf));
		return inst_protocol_error;
	}
	return inst_ok;
}

/* Get the target laser state */
/* (This is not multithread safe) */
static inst_code
specbos_get_target_laser(
	specbos *p,				/* Object */
	int *laser,				/* 0 = off, 1 = on */
	int nd					/* nz = no debug message */
) {
	char buf[MAX_RD_SIZE];
	int ec;
	int lstate;

	if ((ec = specbos_fcommand(p, "*contr:laser?\r", buf, MAX_MES_SIZE, 1.0, 1, tnorm, nd)) != inst_ok) {
		return specbos_interp_code((inst *)p, ec);
	}
	if (p->model == 1501 || p->model == 1511) {
		if (sscanf(buf, "%d ",&lstate) != 1) {
			a1logd(p->log, 1, "specbos_get_target_laser: failed to parse laser state\n");
			return specbos_interp_code((inst *)p, SPECBOS_DATA_PARSE_ERROR);
		}
	} else {
		if (sscanf(buf, "laser: %d ",&lstate) != 1) {
			a1logd(p->log, 1, "specbos_get_target_laser: failed to parse laser state\n");
			return specbos_interp_code((inst *)p, SPECBOS_DATA_PARSE_ERROR);
		}
	}
	*laser = lstate;
	return inst_ok;
}

/* Configure for averaging */
static inst_code set_average(specbos *p, int nav, int lock) {
	char mes[100];
	char buf[MAX_MES_SIZE];
	inst_code ev;

	if (lock) amutex_lock(p->lock);

	if (p->model == 1501 || p->model == 1511) {
		/* Set number to average */
		sprintf(mes, "*para:aver %d\r", nav);
		if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			if (lock) amutex_unlock(p->lock);
			return ev;
		}
	} else {
#ifdef EXPL1211AVG
		if (nav >= 12)
			nav = 4;
		else if (nav >= 6)
			nav = 2;
		else if (nav >= 1)
			nav = 1;
#endif
		/* Set number to average */
		sprintf(mes, "*conf:aver %d\r", nav);
		if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			if (lock) amutex_unlock(p->lock);
			return ev;
		}
	}

	/* Number actual set in instrument */
	p->setnav = nav;

	if (lock) amutex_unlock(p->lock);
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
	double measto = p->measto;
	int n, nav, nxav;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Request calibration if it is needed */
	if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission
	 && p->trans_white.spec_n == 0) {
		return inst_needs_cal;
	}

	amutex_lock(p->lock);

	if (!p->doing_cal && p->trig == inst_opt_trig_user) {
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
	} else if (!p->doing_cal) {
		/* Check for abort */
		if (p->uicallback != NULL
		 && (rv = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort) {
			amutex_unlock(p->lock);
			return rv;				/* Abort */
		}
	}

	if (p->model != 1501 && p->model != 1511) {
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
	}

	/* Make sure the target laser is off */
	if ((rv = specbos_command(p, "*contr:laser 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}
	p->laser = 0;

	/* Attempt a refresh display frame rate calibration if needed */
	if (p->refrmode != 0 && p->rrset == 0) {
		a1logd(p->log, 1, "specbos: need refresh rate calibration before measure\n");
		if ((rv = specbos_imp_measure_set_refresh(p)) != inst_ok) {
			amutex_unlock(p->lock);
			return rv; 
		}
	}
	
	/* Determine number to average */
	if (p->noaverage != 0) {
		nav = p->noaverage;
	} else if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission) {
		nav = DEFAULT_TRANS_NAV;
	} else {
		nav = DEFAULT_NAV;
	}

	/* And set it (ignored if not 15X1 and EXPL1211AVG)  */
	if ((rv = set_average(p, nav, 0)) != inst_ok)
		return rv;

	/* Adjust timeout to account for averaging */
	/* (Allow extra 1 sec fudge factor per average) */
	if (p->setnav > 1) {
		measto = p->setnav * (p->measto - 3.6 + 1.0) + 3.6;
		a1logd(p->log, 6, " Adjusted measto to %f for noaver %d\n",measto,p->setnav);
	}

	if (p->model == 1501 || p->model == 1511) {
		/* Set the measurement format to None. We will read measurement manually. */
		if ((rv = specbos_command(p, "*para:form 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return rv;
		}
	}

	/* Allow for explicit averaging */
	nxav = 1;
	if (nav > p->setnav) {
		nxav = (nav + p->setnav-1) / p->setnav;
	}
	if (nxav < 1)
		nxav = 1;

	/* Clear the value and zero the accumulation */
	val->loc[0] = '\000';
	val->mtype = inst_mrt_none;
	val->mcond = inst_mrc_none;
	val->XYZ_v = 0;
	icmSet3(val->XYZ, 0.0);
	val->sp.spec_n = 0;
	vect_set(val->sp.spec, 0.0, XSPECT_MAX_BANDS);
	val->duration = 0.0;

	for (n = 0; n < nxav; n++) {
		ipatch tval;		/* Temporary value */

#ifdef EXPL1211AVG
		/* Switch off auto exposure after first measurement, to save some time */
		if (n > 0 && p->model != 1501 && p->model != 1511) {
			if ((ec = specbos_command(p, "*para:expo 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				warning("Changing &para:expo to 0 failed");
			}
		}
#endif

		/* Trigger a measurement (Allow 10 second timeout margine) */
		/* (Note that ESC will abort it) */
		if (p->model == 1501 || p->model == 1511)
			ec = specbos_fcommand(p, "*meas:refer\r", buf, MAX_MES_SIZE, measto + 10.0 , 1, tmeas, 0);
		else
			ec = specbos_fcommand(p, "*init\r", buf, MAX_MES_SIZE, measto + 10.0 , 1, tmeas, 0);

		// To test out bug workaround:
		// if (!p->badCal) ec = SPECBOS_EXCEED_CAL_WL;

		/* If the specbos has been calibrated by a 3rd party, its calibrated range */
		/* may be out of sync with its claimed range. Reduce the range and retry. */
		if (ec == SPECBOS_EXCEED_CAL_WL && !p->badCal) {
			char mes[100];
			inst_code ev = inst_ok;

			a1logd(p->log, 1, " Got SPECBOS_EXCEED_CAL_WL error (Faulty 3rd party Calibration ?)\n");
			a1logd(p->log, 1, " Trying workaround by restricting range to 380-780nm\n");

			if (p->wl_short < 380.0)
				p->wl_short = 380.0;
			if (p->wl_long > 780.0)
				p->wl_long = 780.0;

			p->nbands = (int)((p->wl_long - p->wl_short + 1.0)/1.0 + 0.5);

			/* Re-set the wavelength range and resolution */
			sprintf(mes, "*conf:wran %d %d 1\r", (int)(p->wl_short+0.5), (int)(p->wl_long+0.5));
			if ((ev = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				amutex_unlock(p->lock);
				return ev;
			}
			p->badCal = 1;

			/* Try command again */
			if (p->model == 1501 || p->model == 1511)
				ec = specbos_fcommand(p, "*meas:refer\r", buf, MAX_MES_SIZE, measto + 10.0 , 1, tmeas, 0);
			else
				ec = specbos_fcommand(p, "*init\r", buf, MAX_MES_SIZE, measto + 10.0 , 1, tmeas, 0);
		}


		if (ec != SPECBOS_OK) {
			amutex_unlock(p->lock);
			return specbos_interp_code((inst *)p, ec);
		}


		if (p->noXYZ) {	/* "*fetch:XYZ" will fail, so assume it failed rather than trying it */			
			ec = SPECBOS_COMMAND;

		} else {		 /* Read the XYZ */
			ec = specbos_fcommand(p, "*fetch:XYZ\r", buf, MAX_RD_SIZE, 0.5, 3, tnorm, 0);
		}


		if (ec == SPECBOS_OK) {

			if (sscanf(buf, " X: %lf Y: %lf Z: %lf ",
		           &tval.XYZ[0], &tval.XYZ[1], &tval.XYZ[2]) != 3) {
				amutex_unlock(p->lock);
				a1logd(p->log, 1, "specbos_read_sample: failed to parse '%s'\n",buf);
				return specbos_interp_code((inst *)p, SPECBOS_DATA_PARSE_ERROR);
			}
		
		/* Hmm. Some older firmware versions are reported to not support the */
		/* "fetch:XYZ" command. Use an alternative if it fails. */
		} else if (ec == SPECBOS_COMMAND) {
			double Yxy[3];

			p->noXYZ = 1;

			if (p->model == 1501 || p->model == 1511) {
				if ((ec = specbos_fcommand(p, "*calc:PHOTOmetric\r", buf, MAX_RD_SIZE, 1.0, 1, tnorm, 0))
				                                                                        != SPECBOS_OK) {
					amutex_unlock(p->lock);
					return specbos_interp_code((inst *)p, ec);
				}
				if (sscanf(buf, "%lf ", &Yxy[0]) != 1) {
					amutex_unlock(p->lock);
					a1logd(p->log, 1, "specbos_read_sample: failed to parse '%s'\n",buf);
					return specbos_interp_code((inst *)p, SPECBOS_DATA_PARSE_ERROR);
				}
			} else {
				if ((ec = specbos_fcommand(p, "*fetch:PHOTOmetric\r", buf, MAX_RD_SIZE, 0.5, 1, tnorm, 0))
				                                                                        != SPECBOS_OK) {
					amutex_unlock(p->lock);
					return specbos_interp_code((inst *)p, ec);
				}
				if (sscanf(buf, "Luminance[cd/m^2]: %lf ", &Yxy[0]) != 1) {
					amutex_unlock(p->lock);
					a1logd(p->log, 1, "specbos_read_sample: failed to parse '%s'\n",buf);
					return specbos_interp_code((inst *)p, SPECBOS_DATA_PARSE_ERROR);
				}
			}
		
			if (p->model == 1501 || p->model == 1511) {
				if ((ec = specbos_fcommand(p, "*calc:CHROMXY\r", buf, MAX_RD_SIZE, 0.5, 1, tnorm, 0))
				                                                                    != SPECBOS_OK) {
					amutex_unlock(p->lock);
					return specbos_interp_code((inst *)p, ec);
				}
				if (sscanf(buf, " %lf %lf ", &Yxy[1], &Yxy[2]) != 2) {
					amutex_unlock(p->lock);
					a1logd(p->log, 1, "specbos_read_sample: failed to parse '%s'\n",buf);
					return specbos_interp_code((inst *)p, SPECBOS_DATA_PARSE_ERROR);
				}
			} else {
				if ((ec = specbos_fcommand(p, "*fetch:CHROMXY\r", buf, MAX_RD_SIZE, 0.5, 1, tnorm, 0))
				                                                                    != SPECBOS_OK) {
					amutex_unlock(p->lock);
					return specbos_interp_code((inst *)p, ec);
				}
				if (sscanf(buf, "Chrom_x: %lf Chrom_y: %lf ", &Yxy[1], &Yxy[2]) != 2) {
					amutex_unlock(p->lock);
					a1logd(p->log, 1, "specbos_read_sample: failed to parse '%s'\n",buf);
					return inst_protocol_error;
				}
			}
			icmYxy2XYZ(tval.XYZ, Yxy);

		} else if (ec != SPECBOS_OK) {
			amutex_unlock(p->lock);
			return specbos_interp_code((inst *)p, ec);
		}

		/* This may not change anything since instrument may clamp */
		tval.sp.spec_n = 0;
		rv = inst_ok;
	
	
		/* spectrum data is returned only if requested, */
		/* or if we are emulating transmission mode */
		if (p->mode & inst_mode_spectral
		 || (p->mode & inst_mode_illum_mask) == inst_mode_transmission) {
			int tries, maxtries = 5;
			int ii,i, xsize;
			char *cp, *ncp;
	 
			if (p->model == 1501 || p->model == 1511) {
				/* Turn on spetrum output */
				if ((ec = specbos_command(p, "*para:form 2\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
					amutex_unlock(p->lock);
					return ec;
				}
				ii = 0;		/* Spectrum from the start */
			} else {
				ii = -2;	/* Skip first two tokens */ 
	 
				/* (Format 12 doesn't seem to work on the 1211) */
				/* (Format 9 reportedly doesn't work on the 1201) */
				/* The folling works on the 1211 and is reported to work on the 1201 */
			}
	
			/* Because the specbos doesn't use flow control in its */
			/* internal serial communications, it may overrun */
			/* the FT232R buffer, so retry fetching the spectra if */
			/* we get a comm error or parsing error. */
			for (tries = 0; tries < maxtries; tries++) {
	
				/* Fetch the spectral readings */
				if (p->model == 1501 || p->model == 1511)
					ec = specbos_fcommand(p, "*calc:sprad\r", buf, MAX_RD_SIZE, 6.0,
					                                            1, tspec, 0);
				else
					ec = specbos_fcommand(p, "*fetch:sprad\r", buf, MAX_RD_SIZE, 6.0,
					                                             2+p->nbands+1, tnorm, 0);
				if (ec != SPECBOS_OK) {
					a1logd(p->log, 1, "specbos_fcommand: failed with 0x%x\n",ec);
					goto try_again;	/* Retry the fetch */
				}
		
				tval.sp.spec_n = p->nbands;
				tval.sp.spec_wl_short = p->wl_short;
				tval.sp.spec_wl_long = p->wl_long;
		
				/* Spectral data is in W/nm/m^2 */
				tval.sp.norm = 1.0;
				cp = buf;
				for (i = ii; i < tval.sp.spec_n; i++) {
					if ((ncp = strchr(cp, '\r')) == NULL) {
						a1logd(p->log, 1, "specbos_read_sample: failed to parse spectra '%s' at %d/%d\n",cp,i+1,tval.sp.spec_n);
						goto try_again;		/* Retry the fetch and parse */
					}
					*ncp = '\000';
					if (i >= 0) {
						double wl, sp = -1e6;
						if (p->model == 1501 || p->model == 1511) {
							if (sscanf(cp, "%lf %lf", &wl, &sp) != 2)
								sp = -1e6;
						} else {
							if (sscanf(cp, "%lf", &sp) != 1)
								sp = -1e6;
						}
						if (sp == -1e6) {
							a1logd(p->log, 1, "specbos_read_sample: failed to parse spectra '%s' at %d/%d\n",cp,i+1,tval.sp.spec_n);
							goto try_again;		/* Retry the fetch and parse */
						}
						a1logd(p->log, 6, "sample %d/%d got %f from '%s'\n",i+1,tval.sp.spec_n,sp,cp);
						tval.sp.spec[i] = 1000.0 * sp;	/* Convert to mW/m^2/nm */
						if (p->mode & inst_mode_ambient)
							tval.mtype = inst_mrt_ambient;
					}
					cp = ncp+1;
				}
				/* We've parsed correctly, so don't retry */
				break;
	
			  try_again:;
			}
			if (tries >= maxtries) {
				a1logd(p->log, 1, "specbos_fcommand: ran out of retries - parsing error ?\n");
				amutex_unlock(p->lock);
				return inst_protocol_error;
			}
			a1logd(p->log, 1, "specbos_read_sample: got total %d samples/%d expected in %d tries\n",i,tval.sp.spec_n, tries);
	
		}

		icmAdd3(val->XYZ, val->XYZ, tval.XYZ); 
		if (tval.sp.spec_n > 0) {
			XSPECT_COPY_INFO(&val->sp, &tval.sp);
			vect_add(val->sp.spec, tval.sp.spec, tval.sp.spec_n);  
		}

	}	/* Next explicit average */

#ifdef EXPL1211AVG
	if (nxav > 0 && p->model != 1501 && p->model != 1511) {
		/* Return to automatic exposure */
		if ((ec = specbos_command(p, "*para:expo 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			warning("Restoring &para:expo to 0 failed");
		}
	}
#endif

	/* Return (possibly averaged) result */
	if (nxav > 1) {
		icmScale3(val->XYZ, val->XYZ, 1.0/nxav); 
		if (val->sp.spec_n > 0)
			vect_scale(val->sp.spec, val->sp.spec, 1.0/nxav, val->sp.spec_n);  
	}

	val->loc[0] = '\000';

	/* Ambient or Trans 90/diffuse */
	if (p->mode & inst_mode_ambient) {
		val->mtype = inst_mrt_ambient;
	} else	/* Emis or Trans diffuse/90 */
		val->mtype = inst_mrt_emission;

	val->XYZ_v = 1;		/* These are absolute XYZ readings */
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);

	val->duration = 0.0;

	amutex_unlock(p->lock);


	/* Emulate transmission mode */
	if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission) {
		int i;

		if (p->trans_white.spec_n != val->sp.spec_n
		 || p->trans_white.spec_wl_short != val->sp.spec_wl_short
		 || p->trans_white.spec_wl_long != val->sp.spec_wl_long) {
			a1logd(p->log, 1, "specbos_read_sample: Emulated transmission mode got mismatched white ref. !");
			return specbos_interp_code((inst *)p, SPECBOS_INTERNAL_ERROR);
		}

		for (i = 0; i < p->trans_white.spec_n; i++) {
			double vv;

			if ((p->trans_white.spec[i] * 3.0) < val->sp.spec[i]) {
				val->sp.spec[i] = 0.0;
			} else {
				val->sp.spec[i] = 100.0 * val->sp.spec[i]/p->trans_white.spec[i];
			}
		}
		val->sp.norm = 100.0;

		/* Do some filtering of the short wavelengths. */
		/* (This is really compensating for lower instrument sensitivity */
		/* and an assumed 'A' type illuminant source.) */
		{
			int i, ii, j, k;
			double w, ifw, fw, nn;
			double wl, we, vv;
			xspect ts = val->sp;		/* Structure  copy */
			double refl;
	
			/* Do a linear regression to establish and end reflection value */
			{
				double s = 0.0, sxx = 0.0, sy = 0.0, sx = 0.0, sxy = 0.0;
	
				ii = XSPECT_XIX(&val->sp, 400.0);		/* End index */
				for (i = 0; i < ii; i++) {
					s++;
					sxx += i * i;
					sx += i;
					sy += ts.spec[i];
					sxy += i * ts.spec[i];
				}
				refl = (sxx * sy - sx * sxy)/(s * sxx - sx * sx); 
//printf("~1 [0] = %f, linear regression = %f\n",ts.spec[0],refl);
			}
	
			ii = XSPECT_XIX(&val->sp, 500.0);		/* End index */
			ifw = 5.0;								/* Initial +/-width */
			for (i = 0; i < ii; i++) {
				wl = XSPECT_XWL(&val->sp, i);
				vv = (ii - i)/(double)ii;				/* Reduce width as we increase center wl */
				fw = pow(vv, 1.7) * ifw;
				w = nn = vv = 0.0;
	
				/* Scan down */
				for (j = 0; ; j++) {
					w = wl - XSPECT_XWL(&val->sp, i - j);	/* Offset we're at */
					if (w >= fw)
						break;
	
					we = fw - w;
					we = sqrt(we);
					nn += we;
	
					if ((i - j) < 0) {	/* Reflect */
						vv += we * (2.0 * refl - ts.spec[i + j]);
					} else {
						vv += we * ts.spec[i - j];
					}
				}
	
				/* Scan up */
				for (j = 0; ; j++) {
					w = XSPECT_XWL(&val->sp, i + j) - wl;	/* Offset we're at */
					if (w >= fw)
						break;
					we = fw - w;
					we = sqrt(we);
					nn += we;
					vv += we * ts.spec[i + j];
				}
				vv /= nn;
				val->sp.spec[i] = vv;
			}
		}

		/* Convert to XYZ */
		if (p->conv == NULL) {
			p->conv = new_xsp2cie(icxIT_D50, 0.0, NULL, icxOT_CIE_1931_2, NULL, icSigXYZData,
			                                                                  icxNoClamp);
			if (p->conv == NULL) {
				a1logd(p->log, 1, "specbos_read_sample: Emulated transmission new_xsp2cie() failed");
				return specbos_interp_code((inst *)p, SPECBOS_INTERNAL_ERROR);
			}
		}
		p->conv->convert(p->conv, val->XYZ, &val->sp);
		if (clamp)
			icmClamp3(val->XYZ, val->XYZ);
		val->XYZ_v = 1;
		val->XYZ[0] *= 100.0;
		val->XYZ[1] *= 100.0;
		val->XYZ[2] *= 100.0;

		val->mtype = inst_mrt_transmissive;
	}


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
		if (p->model == 1501 || p->model == 1511) {
			if ((rv = specbos_command(p, "*para:syncmod 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				return rv;
			}
		} else {
			if ((rv = specbos_command(p, "*conf:cycmod 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				return rv;
			}
		}
		if (p->model == 1501 || p->model == 1511) {
			sprintf(mes,"*para:syncfreq %f\r",1.0/p->refperiod);
			if ((rv = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				return rv;
			}
		} else {
			sprintf(mes,"*conf:cyctim %f\r",p->refperiod * 1e6);
			if ((rv = specbos_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				return rv;
			}
		}
		a1logd(p->log,5,"specbos_imp_set_refresh set refresh rate to %f Hz\n",1.0/p->refperiod);
	} else {
		if (p->model == 1501 || p->model == 1511) {
			if ((rv = specbos_command(p, "*para:syncmod 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				return rv;
			}
		} else {
			if ((rv = specbos_command(p, "*conf:cycmod 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				return rv;
			}
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

	if (ref_rate != NULL)
		*ref_rate = 0.0;

	if (p->model == 1201)
		return inst_unsupported; 

	/* Make sure the target laser is off */
	if ((rv = specbos_command(p, "*contr:laser 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		return rv;
	}

	if (p->model == 1501 || p->model == 1511) {
		if ((ec = specbos_fcommand(p, "*meas:flic\r", buf, MAX_MES_SIZE, 30.0, 1, trefr, 0)) != SPECBOS_OK) {
			return specbos_interp_code((inst *)p, ec);
		}
	} else {
		if ((ec = specbos_fcommand(p, "*contr:cyctim 200 4000\r", buf, MAX_MES_SIZE, 5.0, 1, trefr, 0)) != SPECBOS_OK) {
			return specbos_interp_code((inst *)p, ec);
		}
	}

	if (p->model == 1501 || p->model == 1511) {
		double ffreq;
		if (sscanf(buf+1, "%lf ", &ffreq) != 1) {
			a1logd(p->log, 1, "specbos_imp_measure_refresh rate: failed to parse string '%s'\n",icoms_fix(buf));
			*ref_rate = 0.0;
			return inst_misread;
		}
		refperiod = 1000.0/ffreq;
	} else {
		if ((cp = strchr(buf, 'c')) == NULL)
			cp = buf;
		if (sscanf(cp, "cyctim[ms]: %lf ", &refperiod) != 1) {
			a1logd(p->log, 1, "specbos_imp_measure_refresh rate: failed to parse string '%s'\n",icoms_fix(buf));
			*ref_rate = 0.0;
			return inst_misread;
		}
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

	if (ref_rate != NULL)
		*ref_rate = 0.0;

	amutex_lock(p->lock);
	if ((rv = specbos_imp_measure_refresh(p, &refrate)) != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}
	amutex_unlock(p->lock);

	if (refrate == 0.0)
		return inst_misread;

	if (ref_rate != NULL)
		*ref_rate = refrate;

	return inst_ok;
}

/* Measure and then set refperiod, refrate if possible */
/* (Not thread safe) */
static inst_code
specbos_imp_measure_set_refresh(
	specbos *p			/* Object */
) {
	inst_code rv;
	double refrate = 0.0;
	int mul;
	double pval;

	if ((rv = specbos_imp_measure_refresh(p, &refrate)) != inst_ok) {
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
		return rv;
	}

	return inst_ok;
}

/* Measure and then set refperiod, refrate if possible */
static inst_code
specbos_measure_set_refresh(
	specbos *p			/* Object */
) {
	int rv;

	amutex_lock(p->lock);
	rv = specbos_imp_measure_set_refresh(p);
	amutex_unlock(p->lock);
	return rv;
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

	if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission) {
		if (p->trans_white.spec_n == 0)
			n_cals |= inst_calt_trans_vwhite;
		a_cals |= inst_calt_trans_vwhite;
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
inst_calc_id_type *idtype,	/* Condition identifier type */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	specbos *p = (specbos *)pp;
	inst_code ev;
    inst_cal_type needed, available;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	*idtype = inst_calc_id_none;
	id[0] = '\000';

	if ((ev = specbos_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
		return ev;

	a1logd(p->log,4,"specbos_calibrate: needed 0x%x, avaialble 0x%x\n",needed, available);

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
		a1logd(p->log,4,"specbos_calibrate: not a calibration we understand\n");
		return inst_unsupported;
	}

	if ((*calt & inst_calt_ref_freq) && p->refrmode != 0) {
		inst_code ev = inst_ok;


		if ((*calc & inst_calc_cond_mask) != inst_calc_emis_80pc) {
			*calc = inst_calc_emis_80pc;
			return inst_cal_setup;
		}

		/* Do refresh display rate calibration */
		if ((ev = specbos_measure_set_refresh(p)) != inst_ok)
			return ev; 

		*calt &= ~inst_calt_ref_freq;
	}

	/* If we are doing a transmission white reference calibrate */
	if ((*calt & inst_calt_trans_vwhite) != 0
	 && (*calc & inst_calc_cond_mask) == inst_calc_man_trans_white
	 && (p->mode & inst_mode_illum_mask) == inst_mode_transmission) {
		inst_mode cmode = p->mode;
		int i;
		ipatch val;

		a1logd(p->log,4,"specbos_calibrate: doing transmission white calib\n");

		if (p->mode & inst_mode_ambient)
			p->mode = inst_mode_emis_ambient | inst_mode_spectral; 
		else
			p->mode = inst_mode_emis_tele | inst_mode_spectral; 
		p->doing_cal = 1;

		/* Set to average 50 readings for calibration */
		// !!!! NOTE :- this doesn't work because specbos_read_sample() overrides it !!!!!
		if ((ev = set_average(p, 50, 1)) != inst_ok)
			return ev;

		if ((ev = specbos_read_sample(pp, "Transmission White", &val, 0)) != inst_ok) {
			a1logd(p->log,1,"specbos_calibrate: Transmission white cal failed with 0x%x\n",ev);
			p->mode = cmode;
			p->doing_cal = 0;
			set_average(p, 1, 1);
			return ev;
		}
		p->trans_white = val.sp; /* Struct copy */

		/* Restore average */
		if ((ev = set_average(p, 1, 1)) != inst_ok)
			return ev;

		// ~~~~9999 check white quality
		
		*calt &= ~inst_calt_trans_vwhite;

		p->mode = cmode;
		p->doing_cal = 0;
	}

	/* Make sure there's the right condition for any remaining calibrations. */
	if (*calt & inst_calt_trans_vwhite) {/* Transmissvice white for emulated transmission */
		*idtype = inst_calc_id_none;
		id[0] = '\000';
		if ((*calc & inst_calc_cond_mask) != inst_calc_man_trans_white) {
			*calc = inst_calc_man_trans_white;
			a1logd(p->log,4,"specbos_calibrate: needs calc 0x%x\n",*calc);
			return inst_cal_setup;
		}
	}

	/* Go around again if we've still got calibrations to do */
	if (*calt & inst_calt_all_mask) {
		a1logd(p->log,4,"specbos_calibrate: more calibrations to do\n");
		return inst_cal_setup;
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
		case SPECBOS_ADAPT_INT_TIME:
			return inst_misread | ec;

		case SPECBOS_PASSWORD:
		case SPECBOS_PARAM_CHSUM:
		case SPECBOS_USERFILE_CHSUM:
		case SPECBOS_USERFILE2_CHSUM:
		case SPECBOS_USERFILE2_ARG:
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
			for (i = 0; p->th_termed == 0 && i < 50; i++)
				msec_sleep(100);	/* Wait for thread to terminate */
			if (i >= 50) {
				a1logd(p->log,3,"specbos diffuser thread termination failed\n");
				p->th->terminate(p->th);	/* Try and force thread to terminate */
			}
			p->th->del(p->th);
		}
		if (p->icom != NULL)
			p->icom->del(p->icom);
		amutex_del(p->lock);
		if (p->conv != NULL)
			p->conv->del(p->conv);
		p->vdel(pp);
		free(p);
	}
}

/* Return the instrument mode capabilities */
static void specbos_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	specbos *p = (specbos *)pp;
	inst_mode cap1 = 0;
	inst2_capability cap2 = 0;
	inst3_capability cap3 = 0;

	cap1 |= inst_mode_emis_tele
	     |  inst_mode_trans_spot		/* Emulated transmission mode diffuse/90 */
	     |  inst_mode_trans_spot_a		/* Emulated transmission mode 90/diffuse */
	     |  inst_mode_colorimeter
	     |  inst_mode_spectral
	     |  inst_mode_emis_refresh_ovd
	     |  inst_mode_emis_norefresh_ovd
	        ;

	if (p->model != 1501 && p->model != 1511) {
		cap1 |= inst_mode_ambient;
	}

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

	/* Can average multiple measurements */
	cap3 |= inst3_average;

	if (pcap1 != NULL)
		*pcap1 = cap1;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = cap3;
}

/* Return current or given configuration available measurement modes. */
/* NOTE that conf_ix values shoudn't be changed, as it is used as a persistent key */
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
		if (p->model != 1501 && p->model != 1511) {
			if ((ev = specbos_get_diffpos(p, &pos, 0)) != inst_ok) {
				amutex_unlock(p->lock);
				return ev;
			}
		} else {
			pos = 0;	/* 1501 & 1511 only have emssion */
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
static inst_code specbos_check_mode(inst *pp, inst_mode m) {
	specbos *p = (specbos *)pp;
	inst_mode cap;

	if (!pp->gotcoms)
		return inst_no_coms;
	if (!pp->inited)
		return inst_no_init;

	pp->capabilities(pp, &cap, NULL, NULL);

	/* Simple test */
	if (m & ~cap)
		return inst_unsupported;

	/* 1501 doesn't support ambient */
	if ((p->model == 1501 || p->model == 1511)
	 &&	IMODETST(m, inst_mode_emis_ambient)) {
		return inst_unsupported;
	}

	/* Only tele emission, ambient and (emulated) transmision modes supported */
	if (!IMODETST(m, inst_mode_emis_tele)
	 && !IMODETST(m, inst_mode_trans_spot)
	 && !IMODETST(m, inst_mode_trans_spot_a)
	 &&	!IMODETST(m, inst_mode_emis_ambient)) {
		return inst_unsupported;
	}

	return inst_ok;
}

/* Set device measurement mode */
static inst_code specbos_set_mode(inst *pp, inst_mode m) {
	specbos *p = (specbos *)pp;
	int refrmode;
	inst_code ev;

	if ((ev = specbos_check_mode(pp, m)) != inst_ok) {
		a1logd(p->log,1,"specbos_set_mode 0x%x invalid\n",m);
		return ev;
	}

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

static inst_disptypesel specbos_disptypesel[3] = {
	{
		inst_dtflags_default,
		1,
		"nl",
		"Non-Refresh display",
		0,
		disptech_lcd,
		0
	},
	{
		inst_dtflags_none,			/* flags */
		2,							/* cbid */
		"rc",						/* sel */
		"Refresh display",			/* desc */
		1,							/* refr */
		disptech_crt,				/* disptype */
		1							/* ix */
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

/* Set the display type - refresh or not */
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
specbos_get_set_opt(inst *pp, inst_opt_type m, ...) {
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

	/* Get laser target state. */
	/* For speed we don't return the real time state, */
	/* but the state from the last poll */ 
	if (m == inst_opt_get_target_state) {
		va_list args;
		int *pstate;

		va_start(args, m);
		pstate = va_arg(args, int *);
		va_end(args);

		if (pstate != NULL)
			*pstate = p->laser;

		return inst_ok;

	/* Set laser target state */
	} else if (m == inst_opt_set_target_state) {
		va_list args;
		int state = 0;

		va_start(args, m);
		state = va_arg(args, int);
		va_end(args);

		amutex_lock(p->lock);
		if (state == 2) {			/* Toggle */ 

			/* Get the current state */
			if ((ev = specbos_get_target_laser(p, &p->laser, 0)) != inst_ok) { 
				amutex_unlock(p->lock);
				return ev;
			}
			a1logd(p->log, 5, " Laser state = %d\n",p->laser);
			if (p->laser == 0)
				state = 1;
			else if (p->laser == 1)
				state = 0;
		}
		if (state == 1) { 
			if ((ev = specbos_command(p, "*contr:laser 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				amutex_unlock(p->lock);
				return ev;
			}
			p->laser = 1;
		} else if (state == 0) {
			if ((ev = specbos_command(p, "*contr:laser 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
				amutex_unlock(p->lock);
				return ev;
			}
			p->laser = 0;
		}
		amutex_unlock(p->lock);
		return inst_ok;
	}

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (m == inst_opt_set_averages) {
		va_list args;
		int nav = 1;

		va_start(args, m);
		nav = va_arg(args, int);
		va_end(args);

		if (nav < 0)
			return inst_bad_parameter;

		p->noaverage = nav;

		/* Averages will get set before each measurement */

		return inst_ok;
	}

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
extern specbos *new_specbos(icoms *icom, instType dtype) {
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
	p->dtype = dtype;
	if (p->dtype == instSpecbos1201)
		p->model = 1201;

	amutex_init(p->lock);

	p->noaverage = 1;

	return p;
}

