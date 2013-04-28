/* 
 * Argyll Color Correction System
 *
 * Xrite DTP92/94 related functions
 *
 * Author: Graeme W. Gill
 * Date:   5/10/1996
 *
 * Copyright 1996 - 2013, Graeme W. Gill
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

/*
	TTBD:
		Should make DTP92 switch trigger a reading.

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
#include "dtp92.h"

/* Default flow control */
#define DEFFC fc_none

#define IGNORE_NEEDS_OFFSET_DRIFT_CAL_ERR

static inst_code dtp92_interp_code(inst *pp, int ec);

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 5000		/* Maximum reading messagle reply size */

/* Extract an error code from a reply string */
/* Return -1 if no error code can be found */
static int 
extract_ec(char *s) {
	char *p;
	char tt[3];
	int rv;
	p = s + strlen(s);
	/* Find the trailing '>' */
	for (p--; p >= s;p--) {
		if (*p == '>')
			break;
	}
	if (  (p-3) < s
	   || p[0] != '>'
	   || p[-3] != '<')
		return -1;
	tt[0] = p[-2];
	tt[1] = p[-1];
	tt[2] = '\000';
	if (sscanf(tt,"%x",&rv) != 1)
		return -1;
	return rv;
}

/* Interpret an icoms error into a DTP92 error */
static int icoms2dtp92_err(int se) {
	if (se != ICOM_OK) {
		if (se & ICOM_TO)
			return DTP92_TIMEOUT; 
		return DTP92_COMS_FAIL;
	}
	return DTP92_OK;
}

/* Do a full featured command/response echange with the dtp92 */
/* Return the dtp error code. End on the specified number */
/* of specified characters, or expiry if the specified timeout */
/* Assume standard error code if tc = '>' and ntc = 1 */
/* Return a DTP92 error code */
static int
dtp92_fcommand(
	struct _dtp92 *p,
	char *in,			/* In string */
	char *out,			/* Out string buffer */
	int bsize,			/* Out buffer size */
	char tc,			/* Terminating character */
	int ntc,			/* Number of terminating characters */
	double to) {		/* Timout in seconds */
	int rv, se;

	if ((se = p->icom->write_read(p->icom, in, out, bsize, tc, ntc, to)) != 0) {
		a1logd(p->log, 1, "dtp92_fcommand: serial i/o failure on write_read '%s'\n",icoms_fix(in));
		return icoms2dtp92_err(se);
	}
	rv = DTP92_OK;
	if (tc == '>' && ntc == 1) {
		rv = extract_ec(out);

#ifdef NEVER
	if (strcmp(in, "0PR\r") == 0)
		rv = DTP92_NEEDS_OFFSET_DRIFT_CAL;		/* Emulate 1B error */
#endif /* NEVER */

		if (rv > 0) {
			rv &= inst_imask;
			if (rv != DTP92_OK) {	/* Clear the error */
				char buf[MAX_MES_SIZE];
				p->icom->write_read(p->icom, "CE\r", buf, MAX_MES_SIZE, '>', 1, 0.5);
			}
		}
	}
	a1logd(p->log, 4, "dtp92_fcommand: command '%s' returned '%s', value 0x%x\n",
	                                          icoms_fix(in), icoms_fix(out),rv);

#ifdef IGNORE_NEEDS_OFFSET_DRIFT_CAL_ERR
	if (strcmp(in, "0PR\r") == 0 && rv == DTP92_NEEDS_OFFSET_DRIFT_CAL) {
		static int warned = 0;
		if (!warned) {
			a1logw(p->log,"dtp92: Got error NEEDS_OFFSET_DRIFT_CAL on instrument reset - being ignored.");
			warned = 1;
		}
		rv = 0;
	}
#endif
	return rv;
}

/* Do a standard command/response echange with the dtp92 */
/* Return the dtp error code */
static inst_code
dtp92_command(dtp92 *p, char *in, char *out, int bsize, double to) {
	int rv = dtp92_fcommand(p, in, out, bsize, '>', 1, to);
	return dtp92_interp_code((inst *)p, rv);
}

/* Establish communications with a DTP92 */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
dtp92_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	dtp92 *p = (dtp92 *) pp;
	static char buf[MAX_MES_SIZE];
	baud_rate brt[5] = { baud_9600, baud_19200, baud_4800, baud_2400, baud_1200 };
	char *brc[5]     = { "30BR\r",  "60BR\r",   "18BR\r",  "0CBR\r",  "06BR\r" };
	char *fcc;
	unsigned int etime;
	instType itype = pp->itype;
	int ci, bi, i, se;
	inst_code ev = inst_ok;

	if (p->icom->port_type(p->icom) == icomt_usb) {
#ifdef ENABLE_USB

		a1logd(p->log, 2, "dtp92_init_coms: About to init USB\n");

		/* Note that the 92Q and 94 have a different  */
		/* arrangement of end points: 				*/
		/*											*/
		/*	92Q				94						*/
		/*	---				--						*/
		/*											*/
		/*	0x81 i Bulk		0x81 i Intr.			*/
		/*	0x01 o Bulk								*/
		/*	0x82 i Intr.							*/
		/*	0x02 o Bulk		0x02 o Intr.			*/
		/*											*/
		/* Set config, interface, write end point, read end point, read quanta */
		if (itype == instDTP94)
			se = p->icom->set_usb_port(p->icom, 1, 0x02, 0x81, icomuf_none, 0, NULL); 
		else
			se = p->icom->set_usb_port(p->icom, 1, 0x01, 0x81, icomuf_none, 0, NULL); 

		if (se != ICOM_OK) { 
			a1logd(p->log, 1, "dtp92_init_coms: set_usb_port failed ICOM err 0x%x\n",se);
			return dtp92_interp_code((inst *)p, icoms2dtp92_err(se));
		}

		/* Blind reset it twice - it seems to sometimes hang up */
		/* otherwise under OSX */
		dtp92_command(p, "0PR\r", buf, MAX_MES_SIZE, 0.5);
		dtp92_command(p, "0PR\r", buf, MAX_MES_SIZE, 0.5);
#else	/* !ENABLE_USB */
		a1logd(p->log, 1, "dtp20: Failed to find USB connection to instrument\n");
		return inst_coms_fail;
#endif	/* !ENABLE_USB */

	} else {

#ifdef ENABLE_SERIAL
		a1logd(p->log, 2, "dtp92_init_coms: About to init Serial I/O\n");

		/* Deal with flow control setting */
		if (fc == fc_nc)
			fc = DEFFC;
		if (fc == fc_XonXOff) {
			fcc = "0304CF\r";
		} else if (fc == fc_Hardware) {
			fcc = "0104CF\r";
		} else {
			fc = fc_none;
			fcc = "0004CF\r";
		}

		/* Figure DTP92 baud rate being asked for */
		for (bi = 0; bi < 5; bi++) {
			if (brt[bi] == br)
				break;
		}
		if (bi >= 5)
			bi = 0;	

		/* Figure current icoms baud rate */
		for (ci = 0; ci < 5; ci++) {
			if (brt[ci] == p->icom->br)
				break;
		}
		if (ci >= 5)
			ci = bi;	

		/* The tick to give up on */
		etime = msec_time() + (long)(1000.0 * tout + 0.5);

		while (msec_time() < etime) {

			a1logd(p->log, 4, "dtp92_init_coms: Trying different baud rates (%u msec to go)\n",
                                                              etime - msec_time());

			/* Until we time out, find the correct baud rate */
			for (i = ci; msec_time() < etime;) {
				if ((se = p->icom->set_ser_port(p->icom, fc_none, brt[i], parity_none,
					                                    stop_1, length_8)) != ICOM_OK) { 
					a1logd(p->log, 1, "dtp92_init_coms: set_ser_port failed ICOM err 0x%x\n",se);
					return dtp92_interp_code((inst *)p, icoms2dtp92_err(se));
				}

				if (((ev = dtp92_command(p, "\r", buf, MAX_MES_SIZE, 0.5)) & inst_mask)
					                                                 != inst_coms_fail)
					break;		/* We've got coms or user abort */

				/* Check for user abort */
				if (p->uicallback != NULL) {
					inst_code ev;
					if ((ev = p->uicallback(p->uic_cntx, inst_negcoms)) == inst_user_abort) {
						a1logd(p->log, 1, "dtp92_init_coms: user aborted\n");
						return inst_user_abort;
					}
				}
	
				if (++i >= 5)
					i = 0;
			}
			break;		/* Got coms */
		}

		if (msec_time() >= etime) {		/* We haven't established comms */
			return inst_coms_fail;
		}

		/* Set the handshaking */
		if ((ev = dtp92_command(p, fcc, buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;

		/* Change the baud rate to the rate we've been told */
		if ((se = p->icom->write_read(p->icom, brc[bi], buf, MAX_MES_SIZE, '>', 1, .2)) != 0) {
			if (extract_ec(buf) != DTP92_OK)
				return inst_coms_fail;
		}

		/* Configure our baud rate and handshaking as well */
		if ((se = p->icom->set_ser_port(p->icom, fc, brt[bi], parity_none,
			                                    stop_1, length_8)) != ICOM_OK) { 
			a1logd(p->log, 1, "dtp92_init_coms: set_ser_port failed ICOM err 0x%x\n",se);
			return dtp92_interp_code((inst *)p, icoms2dtp92_err(se));
		}

		/* Loose a character (not sure why) */
		p->icom->write_read(p->icom, "\r", buf, MAX_MES_SIZE, '>', 1, 0.1);
#else	/* !ENABLE_SERIAL */
		a1logd(p->log, 1, "dtp20: Failed to find serial connection to instrument\n");
		return inst_coms_fail;
#endif	/* !ENABLE_SERIAL */
	}

	/* Check instrument is responding, and reset it again. */
	if ((ev = dtp92_command(p, "\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok
	 || (ev = dtp92_command(p, "0PR\r", buf, MAX_MES_SIZE, 2.0)) != inst_ok) {

		a1logd(p->log, 1, "dtp92_init_coms: failed with ICOM 0x%x\n",ev);

#ifdef NEVER	/* Experimental code for fixing 0x1B "Offset Drift invalid" error */
		if ((ev & inst_mask) == inst_hardware_fail) {
			/* Could be a checksum type error. Try resetting to factory settings */
			warning("Got error %s (%s) on attempting to communicate with instrument.",
			             p->inst_interp_error((inst *)p, ev), p->interp_error((inst *)p, ev));
			if ((ev & inst_imask) == DTP92_NEEDS_OFFSET_DRIFT_CAL) {
				char tbuf[100];
				double odv = 0.0;
				printf("Got Offset Drift Cal error. Will try re-writing it\n");
	 			if ((ev = dtp92_command(p, "SD\r", buf, MAX_MES_SIZE, 2.0)) != inst_ok)
					error("Reading current offset drift value failed");
				if (sscanf(buf, "%lf<", &odv) != 1)
					error("Unable to parse offset drift value");
				printf("Read current offset drift value of %f\n", odv);
				if (odv > 0.5 || odv < 0.05)
					error("offset drift value seems like nonsense - aborting");
				sprintf(tbuf,"%05dSD\r",(int)(odv * 100000));
				printf("Command about to be sent is '%s', OK ? (Y/N)\n",icoms_fix(tbuf));
				if (getchar() == 'Y') {
					if ((ev = dtp92_command(p, tbuf, buf, MAX_MES_SIZE, 6.0)) != inst_ok)
						error("Writing offset drift value failed");
					else
						printf("Writing offset drift value suceeded!\n");
				} else {
					printf("No command written\n");
				}
				printf("Now try re-running program\n");
			}
		}
#endif	/* NEVER */
		
		p->icom->del(p->icom);		/* Since caller may not clean up */
		p->icom = NULL;
		return inst_coms_fail;
	}

	a1logd(p->log, 2, "dtp92_init_coms: init coms has suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

static inst_code set_default_disp_type(dtp92 *p);

/* Initialise the DTP92 */
/* return non-zero on an error, with dtp error code */
static inst_code
dtp92_init_inst(inst *pp) {
	dtp92 *p = (dtp92 *)pp;
	static char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "dtp92_init_inst: called\n");

	if (p->gotcoms == 0)
		return inst_internal_error;		/* Must establish coms before calling init */

	/* Reset it ( without disconnecting USB or resetting baud rate etc.) */
	if ((ev = dtp92_command(p, "0PR\r", buf, MAX_MES_SIZE, 2.0)) != inst_ok)
		return ev;

	/* Get the model and version number */
	if ((ev = dtp92_command(p, "SV\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Check that it is a DTP92, DTP92Q or DTP94 */
	if (   strlen(buf) < 12
	    || (strncmp(buf,"X-Rite DTP92",12) != 0
	     && strncmp(buf,"X-Rite DTP94",12) != 0))
		return inst_unknown_model;

    if (strncmp(buf,"X-Rite DTP94",12) == 0)
		p->itype = instDTP94;
	else
		p->itype = instDTP92;

	if (p->itype == instDTP92) {
		/* Turn echoing of characters off */
		if ((ev = dtp92_command(p, "DEC\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
	}

	if (p->itype == instDTP92) {
		/* Set decimal point on */
		if ((ev = dtp92_command(p, "0106CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
	}

	/* Set color data separator to TAB */
	if ((ev = dtp92_command(p, "0207CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Set delimeter to CR */
	if ((ev = dtp92_command(p, "0008CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Set extra digit resolution (X10) */
	if ((ev = dtp92_command(p, "010ACF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	if (p->itype == instDTP92) {
		/* Set absolute (luminance) calibration */
		if ((ev = dtp92_command(p, "0118CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
	}

	/* Set no black point subtraction */
	if ((ev = dtp92_command(p, "0019CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Set to factory calibration */
	if ((ev = dtp92_command(p, "EFC\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
		return ev;

	if (p->itype == instDTP94) {
		/* Compensate for offset drift */
		if ((ev = dtp92_command(p, "0117CF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
	}

	if (p->itype == instDTP92) {
		/* Enable ABS mode (in case firmware doesn't default to this after EFC) */
		if ((ev = dtp92_command(p, "0118CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
	
		/* Disable -BLK mode (in case firmware doesn't default to this after EFC) */
		if ((ev = dtp92_command(p, "0019CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
	
		/* Set to transmit just the colorimetric data */
		if ((ev = dtp92_command(p, "0120CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
	
		/* Transmit colorimetric data as XYZ */
		if ((ev = dtp92_command(p, "0221CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;

		/* Disable Luminance group */
		if ((ev = dtp92_command(p, "0022CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
	
		/* Disable Frequency group */
		if ((ev = dtp92_command(p, "0023CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
	
		/* Disable color temperature group */
		if ((ev = dtp92_command(p, "0024CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
	
		/* Disable RGB group */
		if ((ev = dtp92_command(p, "0025CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
	
		/* Disable pushbutton */
		if ((ev = dtp92_command(p, "DPB\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;

		/* Set sample size to 16 (default is 16) */
		if ((ev = dtp92_command(p, "10SS\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
	}
	p->trig = inst_opt_trig_user;

	/* Set the default display type */
	if ((ev = set_default_disp_type(p)) != inst_ok)
		return ev;

	/* ??? Need to set CTYP appropriately ??? */

#ifdef NEVER	/* Debug code */
	if (p->itype == instDTP94) {
		int i, val;
		char tb[50];

		for (i = 0; i < 0x800; i++) {
			sprintf(tb,"%04xRN\r",i);
			if ((ev = dtp92_command(p, tb, buf, MAX_MES_SIZE, 0.2)) != inst_ok)
				return ev;
			if (sscanf(buf,"%x",&val) != 1)
				return inst_coms_fail;
			tb[0] = val;
			tb[1] = '\000';
			a1logd(p->log, 0, "Mem location 0x%04x = 0x%02x '%s'\n",i,val,icoms_fix(tb));
		}
	}
#endif /* NEVER */

	if (p->log->verb) {
		int i, j;
		if ((ev = dtp92_command(p, "GI\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok) {
			a1logd(p->log, 1, "dtp20: GI command failed with ICOM err 0x%x\n",ev);
			return ev;
		}
		for (j = i = 0; ;i++) {
			if (buf[i] == '<' || buf[i] == '\000')
				break;
			if (buf[i] == '\r') {
				buf[i] = '\000';
				a1logv(p->log, 1, " %s\n",&buf[j]);
				if (buf[i+1] == '\n')
					i++;
				j = i+1;
			}
		}
	}

	p->inited = 1;
	a1logd(p->log, 2, "dtp92_init_inst: instrument inited OK\n");

	return inst_ok;
}

/* The DTP92 seems to have a bug whereby it adds a spurious */
/* digit after the 'Z' of the Z value. Try and discard this. */

/* Read a single sample */
/* Return the dtp error code */
static inst_code
dtp92_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	dtp92 *p = (dtp92 *)pp;
	char buf[MAX_RD_SIZE];
	int tries;
	int user_trig = 0;
	inst_code rv = inst_protocol_error;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Could change SS to suite level expected. */
#ifdef NEVER
	if (p->itype == instDTP92) {
		/* Set sample size to 31 (default is 16) for low level readings */
		if ((rv = dtp92_command(p, "1fSS\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return rv;
	}
#endif

	if (p->trig == inst_opt_trig_user) {

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "dtp92: inst_opt_trig_user but no uicallback function set!\n");
			return inst_unsupported;
		}

		for (;;) {
			if ((rv = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
				if (rv == inst_user_abort)
					return rv;				/* Abort */
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
		 && (rv = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort)
			return rv;				/* Abort */
	}

	/* Until we get a valid return */
	for (tries = 0; tries < 2; tries++) {

		/* Take a reading */
		/* (DTP94 has optional parameters, but the default is what we want, XYZ in cd/m^2) */
		if ((rv = dtp92_command(p, "RM\r", buf, MAX_RD_SIZE, 10.0)) != inst_ok) {
			if ((rv & inst_imask) == DTP92_NEEDS_OFFSET_CAL)
				p->need_offset_cal = 1;
			else if ((rv & inst_imask) == DTP92_NEEDS_RATIO_CAL)	/* DTP92 only */
				p->need_ratio_cal = 1;
			return rv;
		}

		if (sscanf(buf, " X%*c %lf\t Y%*c %lf\t Z%*c %lf ",
	           &val->XYZ[0], &val->XYZ[1], &val->XYZ[2]) == 3) {

			/* Apply the colorimeter correction matrix */
			icmMulBy3x3(val->XYZ, p->ccmat, val->XYZ);

			/* This may not change anything since instrument may clamp */
			if (clamp)
				icmClamp3(val->XYZ, val->XYZ);
			val->loc[0] = '\000';
			val->mtype = inst_mrt_emission;
			val->XYZ_v = 1;		/* These are absolute XYZ readings */
			val->sp.spec_n = 0;
			val->duration = 0.0;
			rv = inst_ok;
			break;
		} else {
			a1logd(p->log, 1, "dtp92_read_sample: failed to parse string '%s'\n",buf);
			rv = inst_coms_fail;
		}
	}

#ifdef NEVER
	if (p->itype == instDTP92) {
		/* Set sample size back to 16 */
		if ((rv = dtp92_command(p, "10SS\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return rv;
	}
#endif

	if (user_trig)
		return inst_user_trig;
	return rv;
}

/* Read an emissive refresh rate */
static inst_code
dtp92_read_refrate(
inst *pp,
double *ref_rate
) {
	dtp92 *p = (dtp92 *)pp;
	char buf[MAX_RD_SIZE];
	char buf2[MAX_RD_SIZE];
	double refrate = 0.0;
	inst_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Measure the refresh rate */
	rv = dtp92_command(p, "00103RM\r", buf, MAX_RD_SIZE, 5.0);

	if (rv != inst_ok) {
		if ((rv & inst_imask) == DTP92_NO_DATA_AVAILABLE)
			rv = inst_misread;
		return rv;
	}

	if (sscanf(buf, "Hz %lf ", &refrate) != 1) {
		a1logd(p->log, 1, "dtp92_read_refrate rate: failed to parse string '%s'\n",buf);
		*ref_rate = 0.0;
		return inst_misread;
	}
	if (refrate == 0.0) {
		return inst_misread;
	}
	*ref_rate = refrate;
	return inst_ok;
}

/* Insert a colorimetric correction matrix in the instrument XYZ readings */
/* This is only valid for colorimetric instruments. */
/* To remove the matrix, pass NULL for the filter filename */
static inst_code dtp92_col_cor_mat(
inst *pp,
double mtx[3][3]
) {
	dtp92 *p = (dtp92 *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (mtx == NULL)
		icmSetUnity3x3(p->ccmat);
	else
		icmCpy3x3(p->ccmat, mtx);
		
	return inst_ok;
}

/* Return needed and available inst_cal_type's */
static inst_code dtp92_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	dtp92 *p = (dtp92 *)pp;
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
		
	if (p->itype == instDTP92) {
		if (p->need_ratio_cal)
			n_cals |= inst_calt_emis_ratio;
		a_cals |= inst_calt_emis_ratio;
	}

	if (p->need_offset_cal)
		n_cals |= inst_calt_emis_offset;
	a_cals |= inst_calt_emis_offset;

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	return inst_ok;
}

/* Request an instrument calibration. */
/* This is use if the user decides they want to do a calibration */
/* in anticipation of a calibration (needs_calibration()) to avoid */
/* requiring one during measurement, or in response to measuring */
/* returning inst_needs_cal. Initially use an inst_cal_cond of inst_calc_none, */
/* and then be prepared to setup the right conditions, or ask the */
/* user to do so, each time the error inst_cal_setup is returned. */
static inst_code dtp92_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	dtp92 *p = (dtp92 *)pp;
	char buf[MAX_RD_SIZE];
	inst_code ev = inst_ok;
    inst_cal_type needed, available;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	id[0] = '\000';

	if ((ev = dtp92_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
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

		a1logd(p->log,4,"dtp92_calibrate: doing calt 0x%x\n",calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return inst_ok;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return inst_unsupported;
	}

	if (*calt & inst_calt_emis_offset) {		/* Dark offset calibration */

		if (*calc != inst_calc_man_em_dark) {
			*calc = inst_calc_man_em_dark;
			return inst_cal_setup;
		}

		/* Do offset calibration */
		if ((ev = dtp92_command(p, "CO\r", buf, MAX_RD_SIZE, 12)) != inst_ok)
			return ev;

		*calt &= inst_calt_emis_offset;
	}

	if (*calt & inst_calt_emis_ratio) {	/* Cell ratio calibration */

		if (*calc != inst_calc_emis_grey
		 && *calc != inst_calc_emis_grey_darker
		 && *calc != inst_calc_emis_grey_ligher) {
			*calc = inst_calc_emis_grey;
			return inst_cal_setup;
		}

		/* Do ratio calibration */
		if ((ev = dtp92_command(p, "CR\r", buf, MAX_RD_SIZE, 25.0)) != inst_ok) {

			if ((ev & inst_imask) == DTP92_TOO_MUCH_LIGHT) {
				*calc = inst_calc_emis_grey_darker;
				return inst_cal_setup;
			} else if ((ev & inst_imask) == DTP92_NOT_ENOUGH_LIGHT) {
				*calc = inst_calc_emis_grey_ligher;
				return inst_cal_setup;
			}
			return ev;		/* Error */
		}
		*calt &= inst_calt_emis_ratio;
	}

	return ev;
}

/* Return the last reading refresh rate in Hz. */
static inst_code dtp92_get_refr_rate(inst *pp,
double *ref_rate
) {
	dtp92 *p = (dtp92 *)pp;
	char buf[MAX_RD_SIZE];
	char buf2[MAX_RD_SIZE];
	double refrate = 0.0;
	inst_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Get the last readings refresh rate */
	rv = dtp92_command(p, "10103RM\r", buf, MAX_RD_SIZE, 5.0);

	if (rv != inst_ok) {
		if ((rv & inst_imask) == DTP92_NO_DATA_AVAILABLE)
			rv = inst_misread;
		return rv;
	}

	if (sscanf(buf, "Hz %lf ", &refrate) != 1) {
		a1logd(p->log, 1, "dtp92_read_refresh rate: failed to parse string '%s'\n",buf);
		*ref_rate = 0.0;
		return inst_misread;
	}
	if (refrate == 0.0) {
		return inst_misread;
	}
	*ref_rate = refrate;
	return inst_ok;
}

/* Set the calibrated refresh rate in Hz. */
/* Set refresh rate to 0.0 to mark it as invalid */
/* Rates outside the range 5.0 to 150.0 Hz will return an error */
static inst_code dtp92_set_refr_rate(inst *pp,
double ref_rate
) {
	dtp92 *p = (dtp92 *)pp;

	/* The DTP92 can't have a refresh rate set */
	return inst_unsupported;
}

/* Error codes interpretation */
static char *
dtp92_interp_error(inst *pp, int ec) {
//	dtp92 *p = (dtp92 *)pp;
	ec &= inst_imask;
	switch (ec) {
		case DTP92_INTERNAL_ERROR:
			return "Internal software error";
		case DTP92_COMS_FAIL:
			return "Communications failure";
		case DTP92_UNKNOWN_MODEL:
			return "Not a DTP92 or DTP94";
		case DTP92_DATA_PARSE_ERROR:
			return "Data from DTP didn't parse as expected";

		case DTP92_OK:
			return "No device error";

		case DTP92_BAD_COMMAND:
			return "Unrecognized command";
		case DTP92_PRM_RANGE:
			return "Command parameter out of range";
		case DTP92_MEMORY_OVERFLOW:
			return "Memory bounds error";
		case DTP92_INVALID_BAUD_RATE:
			return "Invalid baud rate";
		case DTP92_TIMEOUT:
			return "Receive timeout";
		case DTP92_SYNTAX_ERROR:
			return "Badly formed parameter";
		case DTP92_NO_DATA_AVAILABLE:
			return "No data available";
		case DTP92_MISSING_PARAMETER:
			return "Paramter is missing";
		case DTP92_CALIBRATION_DENIED:
			return "Invalid calibration enable code";
		case DTP92_NEEDS_OFFSET_CAL:
			return "Offset calibration checksum failed";
		case DTP92_NEEDS_RATIO_CAL:
			return "Ratio calibration checksum failed";
		case DTP92_NEEDS_LUMINANCE_CAL:
			return "Luminance calibration checksum failed";
		case DTP92_NEEDS_WHITE_POINT_CAL:
			return "White point calibration checksum failed";
		case DTP92_NEEDS_BLACK_POINT_CAL:
			return "Black point calibration checksum failed";
		case DTP92_NEEDS_OFFSET_DRIFT_CAL:
			return "Offset drift calibration checksum failed";
		case DTP92_INVALID_READING:
			return "Unable to take a reading";
		case DTP92_BAD_COMP_TABLE:
			return "Bad compensation table";
		case DTP92_TOO_MUCH_LIGHT:
			return "Too much light entering instrument";
		case DTP92_NOT_ENOUGH_LIGHT:
			return "Not enough light to complete operation";

		case DTP92_BAD_SERIAL_NUMBER:
			return "New serial number is invalid";

		case DTP92_NO_MODULATION:
			return "No refresh modulation detected";

		case DTP92_EEPROM_FAILURE:
			return "EEprom write failure";
		case DTP92_FLASH_WRITE_FAILURE:
			return "Flash memory write failure";
		case DTP92_BAD_CONFIGURATION:
			return "Configuration data checksum failed";
		case DTP92_INST_INTERNAL_ERROR:
			return "Internal instrument error";
		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
dtp92_interp_code(inst *pp, int ec) {

	ec &= inst_imask;
	switch (ec) {

		case DTP92_OK:
			return inst_ok;

		case DTP92_INTERNAL_ERROR:
			return inst_internal_error | ec;

		case DTP92_COMS_FAIL:
			return inst_coms_fail | ec;

		case DTP92_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case DTP92_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

		case DTP92_NO_MODULATION:		/* ?? */
		case DTP92_TOO_MUCH_LIGHT:
		case DTP92_NOT_ENOUGH_LIGHT:
		case DTP92_INVALID_READING:
			return inst_misread | ec;

		case DTP92_NEEDS_OFFSET_CAL:
			return inst_needs_cal | ec;

		case DTP92_NEEDS_RATIO_CAL:
			return inst_needs_cal | ec;

		case DTP92_NEEDS_LUMINANCE_CAL:
		case DTP92_NEEDS_WHITE_POINT_CAL:
		case DTP92_NEEDS_BLACK_POINT_CAL:
		case DTP92_NEEDS_OFFSET_DRIFT_CAL:
			return inst_hardware_fail | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
dtp92_del(inst *pp) {
	dtp92 *p = (dtp92 *)pp;
	if (p->icom != NULL)
		p->icom->del(p->icom);
	inst_del_disptype_list(p->dtlist, p->ndtlist);
	free(p);
}

/* Return the instrument mode capabilities */
void dtp92_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	dtp92 *p = (dtp92 *)pp;
	inst_mode cap1 = 0;
	inst2_capability cap2 = 0;

	cap1 |= inst_mode_emis_spot
	     |  inst_mode_colorimeter
	        ;

	cap2 |= inst2_prog_trig
	     |  inst2_user_trig
	     |  inst2_ccmx
	        ;				/* The '92 does have a switch, but we're not currently supporting it */

	if (p->itype == instDTP94) {
		cap2 |= inst2_disptype;
	} else {
		cap2 |= inst2_refresh_rate;
		cap2 |= inst2_emis_refr_meas;
	}

	if (pcap1 != NULL)
		*pcap1 = cap1;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = inst3_none;
}

/* Check device measurement mode */
inst_code dtp92_check_mode(inst *pp, inst_mode m) {
	inst_mode cap;

	if (!pp->gotcoms)
		return inst_no_coms;
	if (!pp->inited)
		return inst_no_init;

	pp->capabilities(pp, &cap, NULL, NULL);

	/* Simple test */
	if (m & ~cap)
		return inst_unsupported;

	/* Only display emission mode supported */
	if (!IMODETST(m, inst_mode_emis_spot)) {
		return inst_unsupported;
	}

	return inst_ok;
}

/* Set device measurement mode */
inst_code dtp92_set_mode(inst *pp, inst_mode m) {
	inst_code ev;

	if ((ev = dtp92_check_mode(pp, m)) != inst_ok)
		return ev;

	return inst_ok;
}

inst_disptypesel dtp92_disptypesel[2] = {
	{
		inst_dtflags_default,		/* flags */
		2,							/* cbid */
		"c",						/* sel */
		"CRT display",				/* desc */
		1,							/* refr */
		0							/* ix */
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

inst_disptypesel dtp94_disptypesel[4] = {
	{
		inst_dtflags_default,
		1,
		"l",
		"LCD display",
		0,
		2
	},
	{
		inst_dtflags_none,			/* flags */
		2,							/* cbid */
		"c",						/* sel */
		"CRT display",				/* desc */
		1,							/* refr */
		1							/* ix */
	},
	{
		inst_dtflags_none,
		3,
		"g",
		"Generic display",
		0,							/* Might be auto refresh detect ? */
		0
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

static void set_base_disptype_list(dtp92 *p) {
	if (p->itype == instDTP94) {
		p->_dtlist = dtp94_disptypesel;
	} else {
		p->_dtlist = dtp92_disptypesel;
	}
}

/* Get mode and option details */
static inst_code dtp92_get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	dtp92 *p = (dtp92 *)pp;
	inst_code rv = inst_ok;

	/* Create/Re-create a current list of abailable display types */
	if (p->dtlist == NULL || recreate) {
		if ((rv = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    p->_dtlist, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return rv;
	}

	if (pnsels != NULL)
		*pnsels = p->ndtlist;

	if (psels != NULL)
		*psels = p->dtlist;

	return inst_ok;
}

/* Given a display type entry, setup for that type */
static inst_code set_disp_type(dtp92 *p, inst_disptypesel *dentry) {

	p->icx = dentry->ix; 
	p->cbid = dentry->cbid; 
	p->refrmode = dentry->refr; 

	if (p->itype == instDTP92) {
		if (p->icx != 0)
			return inst_unsupported;

	} else {	/* DTP94 */
		static char buf[MAX_MES_SIZE];
		inst_code ev;
		
		if (p->icx == 0) {			/* Generic/Non-specific */
			if ((ev = dtp92_command(p, "0016CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
				return ev;
		} else if (p->icx == 1) {	/* CRT */
			if ((ev = dtp92_command(p, "0116CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
				return ev;
		} else if (p->icx == 2) {	/* LCD */
			if ((ev = dtp92_command(p, "0216CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
				return ev;
		} else {
			return inst_unsupported;
		}
	}

	if (dentry->flags & inst_dtflags_ccmx) {
		icmCpy3x3(p->ccmat, dentry->mat);
	} else {
		icmSetUnity3x3(p->ccmat);
	}

	return inst_ok;
}

/* Setup the default display type */
static inst_code set_default_disp_type(dtp92 *p) {
	inst_code ev;
	int i;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
		    p->_dtlist, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return ev;
	}

	for (i = 0; !(p->dtlist[i].flags & inst_dtflags_end); i++) {
		if (p->dtlist[i].flags & inst_dtflags_default)
			break;
	}
	if (p->dtlist[i].flags & inst_dtflags_end) {
		a1loge(p->log, 1, "set_default_disp_type: failed to find type!\n");
		return inst_internal_error; 
	}
	if ((ev = set_disp_type(p, &p->dtlist[i])) != inst_ok) {
		return ev;
	}

	return inst_ok;
}

/* Set the display type */
static inst_code dtp92_set_disptype(inst *pp, int ix) {
	dtp92 *p = (dtp92 *)pp;
	inst_code ev;
	inst_disptypesel *dentry;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    p->_dtlist, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return ev;
	}

	if (ix < 0 || ix >= p->ndtlist)
		return inst_unsupported;

	dentry = &p->dtlist[ix];

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
dtp92_get_set_opt(inst *pp, inst_opt_type m, ...)
{
	dtp92 *p = (dtp92 *)pp;
	char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;

	/* Record the trigger mode */
	if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_user) {
		p->trig = m;
		return inst_ok;
	}

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Get the display type information */
	if (m == inst_opt_get_dtinfo) {
		va_list args;
		int *refrmode, *cbid;

		va_start(args, m);
		refrmode = va_arg(args, int *);
		cbid = va_arg(args, int *);
		va_end(args);

		if (refrmode != NULL)
			*refrmode = p->refrmode;
		if (cbid != NULL)
			*cbid = p->cbid;

		return inst_ok;
	}

	return inst_unsupported;
}

/* Constructor */
extern dtp92 *new_dtp92(icoms *icom, instType itype) {
	dtp92 *p;
	if ((p = (dtp92 *)calloc(sizeof(dtp92),1)) == NULL) {
		a1loge(icom->log, 1, "new_dtp92: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	p->init_coms         = dtp92_init_coms;
	p->init_inst         = dtp92_init_inst;
	p->capabilities      = dtp92_capabilities;
	p->check_mode        = dtp92_check_mode;
	p->set_mode          = dtp92_set_mode;
	p->get_disptypesel   = dtp92_get_disptypesel;
	p->set_disptype      = dtp92_set_disptype;
	p->get_set_opt       = dtp92_get_set_opt;
	p->read_sample       = dtp92_read_sample;
	p->read_refrate      = dtp92_read_refrate;
	p->get_n_a_cals      = dtp92_get_n_a_cals;
	p->calibrate         = dtp92_calibrate;
	p->col_cor_mat       = dtp92_col_cor_mat;
	p->get_refr_rate     = dtp92_get_refr_rate;
	p->set_refr_rate     = dtp92_set_refr_rate;
	p->interp_error      = dtp92_interp_error;
	p->del               = dtp92_del;

	p->icom = icom;
	p->itype = icom->itype;

	icmSetUnity3x3(p->ccmat);	/* Set the colorimeter correction matrix to do nothing */
	set_base_disptype_list(p);

	return p;
}

