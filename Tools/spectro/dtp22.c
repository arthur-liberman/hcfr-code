
/* 
 * Argyll Color Correction System
 *
 * Xrite DTP22 related functions
 *
 * Author: Graeme W. Gill
 * Date:   17/11/2006
 *
 * Copyright 1996 - 2007, Graeme W. Gill
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
#endif  /* !SALONEINSTLIB */
#include "xspect.h"
#include "insttypes.h"
#include "icoms.h"
#include "conv.h"
#include "dtp22.h"

#undef DEBUG

/* Default flow control (Instrument doesn't support HW flow control) */
#define DEFFC fc_XonXOff

static inst_code dtp22_interp_code(inst *pp, int ec);
static int comp_password(char *out, char *in, unsigned char key[4]);
static inst_code activate_mode(dtp22 *p);
static inst_code dtp22_set_opt_mode(inst *pp, inst_opt_mode m, ...);

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 5000		/* Maximum reading messagle reply size */

/* Known DTP22 challenge/response keys for each OEM */
/* (This is a 24 bit key - only the xor of the middle 2 bytes is significant) */
/* The keys seem to be base 6/36, using nibbles with 2+2 bits: 3 5 6 9 A C */
/* The last digit corresponds to the OEM serial number (ie. 6C + 9base6 = A6) */
/* Possibly each digit is offset by the oemsn if counted in the right sequence ? - */
/* ie. base 40 sequence or so ? Need more examples of keys to tell. */
struct {
	int oemsn;	
	unsigned char key[4];
} keys[] = {
	{  0, { 0x39, 0xa6, 0x55, 0x6c }},	/* Standard DTP22 */
	{  9, { 0x5a, 0x66, 0xcc, 0xa6 }},	/* ColorMark calibrator - MacDermid GRAPHICARTS ColorSpan */
	{ -1, }								/* End marker */
};

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
	/* For some reason the top bit sometimes get set ? */
	rv &= 0x7f;
	return rv;
}

/* Interpret an icoms error into a DTP22 error */
static int icoms2dtp22_err(int se) {
	if (se & ICOM_USERM) {
		se &= ICOM_USERM;
		if (se == ICOM_USER)
			return DTP22_USER_ABORT;
		if (se == ICOM_TERM)
			return DTP22_USER_TERM;
		if (se == ICOM_TRIG)
			return DTP22_USER_TRIG;
		if (se == ICOM_CMND)
			return DTP22_USER_CMND;
	}
	if (se != ICOM_OK)
		return DTP22_COMS_FAIL;
	return DTP22_OK;
}

/* Do a full featured command/response echange with the dtp22 */
/* Return the dtp error code. End on the specified number */
/* of specified characters, or expiry if the specified timeout */
/* Assume standard error code if tc = '>' and ntc = 1 */
/* Return a DTP22 error code */
static int
dtp22_fcommand(
	struct _dtp22 *p,
	char *in,			/* In string */
	char *out,			/* Out string buffer */
	int bsize,			/* Out buffer size */
	char tc,			/* Terminating character */
	int ntc,			/* Number of terminating characters */
	double to) {		/* Timout in seconds */
	int se, rv = DTP22_OK;

	if ((se = p->icom->write_read(p->icom, in, out, bsize, tc, ntc, to)) != 0) {
#ifdef DEBUG
		printf("dtp22 fcommand: serial i/o failure on write_read '%s'\n",icoms_fix(in));
#endif
		return icoms2dtp22_err(se);
	}
	if (tc == '>' && ntc == 1) {
		rv = extract_ec(out);
#ifdef NEVER
if (strcmp(in, "0PR\r") == 0)
rv = 0x1b;
#endif /* NEVER */
		if (rv > 0) {
			rv &= inst_imask;
			if (rv != DTP22_OK) {	/* Clear the error */
				char buf[MAX_MES_SIZE];
				p->icom->write_read(p->icom, "CE\r", buf, MAX_MES_SIZE, '>', 1, 0.5);
			}
		}
	}
#ifdef DEBUG
	printf("command '%s'",icoms_fix(in));
	printf(" returned '%s', value 0x%x\n",icoms_fix(out),rv);
#endif
	return rv;
}

/* Do a standard command/response echange with the dtp22 */
/* Return the dtp error code */
static inst_code
dtp22_command(dtp22 *p, char *in, char *out, int bsize, double to) {
	int rv = dtp22_fcommand(p, in, out, bsize, '>', 1, to);
	return dtp22_interp_code((inst *)p, rv);
}

/* Establish communications with a DTP22 */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
dtp22_init_coms(inst *pp, int port, baud_rate br, flow_control fc, double tout) {
	dtp22 *p = (dtp22 *) pp;
	char buf[MAX_MES_SIZE];
	baud_rate brt[5] = { baud_9600, baud_19200, baud_4800, baud_2400, baud_1200 };
	char *brc[5]     = { "30BR\r",  "60BR\r",   "18BR\r",  "0CBR\r",  "06BR\r" };
	char *fcc;
	unsigned int etime;
	int ci, bi, i, rv;
	inst_code ev = inst_ok;

	if (p->debug) {
		p->icom->debug = p->debug;	/* Turn on debugging */
		fprintf(stderr,"dtp22: About to init coms\n");
	}

	if (p->debug) fprintf(stderr,"dtp22: About to init Serial I/O\n");

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

	/* Figure DTP22 baud rate being asked for */
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

		if (p->debug) fprintf(stderr,"dtp22: Trying different baud rates (%lu msec to go)\n",etime - msec_time());

		/* Until we time out, find the correct baud rate */
		for (i = ci; msec_time() < etime;) {
			p->icom->set_ser_port(p->icom, port, fc_none, brt[i], parity_none,
			                                                 stop_1, length_8);
			if (((ev = dtp22_command(p, "\r", buf, MAX_MES_SIZE, 0.5)) & inst_mask)
			                                                           != inst_coms_fail) {
				break;		/* We've got coms or user abort */
			}
			if (++i >= 5)
				i = 0;
		}

		if ((ev & inst_mask) == inst_user_abort)
			return ev;

		break;		/* Got coms */
	}

	if (msec_time() >= etime) {		/* We haven't established comms */
		return inst_coms_fail;
	}

	/* Set the handshaking */
	if ((ev = dtp22_command(p, fcc, buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Change the baud rate to the rate we've been told */
	if ((rv = p->icom->write_read(p->icom, brc[bi], buf, MAX_MES_SIZE, '>', 1, .2)) != 0) {
		if (extract_ec(buf) != DTP22_OK)
			return inst_coms_fail;
	}

	/* Configure our baud rate and handshaking as well */
	p->icom->set_ser_port(p->icom, port, fc, brt[bi], parity_none, stop_1, length_8);

	/* Loose a character (not sure why) */
	p->icom->write_read(p->icom, "\r", buf, MAX_MES_SIZE, '>', 1, 0.1);

	/* Check instrument is responding, and reset it again. */
	if ((ev = dtp22_command(p, "\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok
	 || (ev = dtp22_command(p, "0PR\r", buf, MAX_MES_SIZE, 2.0)) != inst_ok) {

		if (p->debug) fprintf(stderr,"dtp22: init coms has failed\n");

		p->icom->del(p->icom);		/* Since caller may not clean up */
		p->icom = NULL;
#ifdef DEBUG
		printf("^M failed to get a response, returning inst_coms_fail\n");
#endif
		return inst_coms_fail;
	}

	if (p->debug) fprintf(stderr,"dtp22: init coms has suceeded\n");
#ifdef DEBUG
	printf("Got communications\n");
#endif

	p->gotcoms = 1;
	return inst_ok;
}

/* Initialise the DTP22 */
/* return non-zero on an error, with dtp error code */
static inst_code
dtp22_init_inst(inst *pp) {
	dtp22 *p = (dtp22 *)pp;
	char buf[MAX_MES_SIZE], *bp;
	inst_code ev = inst_ok;
	int i;

	if (p->debug) fprintf(stderr,"dtp22: About to init instrument\n");

	if (p->gotcoms == 0)
		return inst_internal_error;		/* Must establish coms before calling init */

	/* Warm reset it */
	if ((ev = dtp22_command(p, "0PR\r", buf, MAX_MES_SIZE, 2.0)) != inst_ok)
		return ev;

	/* Get the model and version number */
	if ((ev = dtp22_command(p, "SV\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Check that it is a DTP22 */
	if (   strlen(buf) < 12
	    || (strncmp(buf,"X-Rite DTP22",12) != 0))
		return inst_unknown_model;

	/* Factory reset */
//	if ((ev = dtp22_command(p, "5CRI\r", buf, MAX_MES_SIZE, 10.2)) != inst_ok)
//			return ev;

	/* Turn echoing of characters off */
	if ((ev = dtp22_command(p, "0EC\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;

	/* Set decimal point on */
	if ((ev = dtp22_command(p, "0106CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Set color data separator to TAB */
	if ((ev = dtp22_command(p, "0207CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Set delimeter to CR */
	if ((ev = dtp22_command(p, "0008CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Set extra digit resolution (X10) */
	if ((ev = dtp22_command(p, "010ACF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* - - - - - - - - - - - - - - - - - - - - - - - - */
	/* Get some information about the instrument */
	if ((ev = dtp22_command(p, "GI\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Extract some of these */
	if ((bp = strstr(buf, "Serial Number:")) != NULL) {
		bp += strlen("Serial Number:");
		p->serno = atoi(bp);
	} else {
		p->serno = -1;
	}
	if ((bp = strstr(buf, "OEM Serial #:")) != NULL) {
		bp += strlen("OEM Serial #:");
		p->oemsn = atoi(bp);
	} else {
		p->oemsn = -1;
	}
	if ((bp = strstr(buf, "Cal Plaque Serial #:")) != NULL) {
		bp += strlen("Cal Plaque Serial #:");
		p->plaqueno = atoi(bp);
	} else {
		p->plaqueno = -1;
	}
	if (p->verb) {
		int i, j;
		for (j = i = 0; ;i++) {
			if (buf[i] == '<' || buf[i] == '\000')
				break;
			if (buf[i] == '\r') {
				buf[i] = '\000';
				printf(" %s\n",&buf[j]);
				if (buf[i+1] == '\n')
					i++;
				j = i+1;
			}
		}
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - */
	/* Setup for the default type of measurements we want to do */

	/* Disable key codes */
	if ((ev = dtp22_command(p, "0OK\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Disable the read microswitch by default */
	if ((ev = dtp22_command(p, "0PB\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;
	p->trig = inst_opt_trig_keyb;

	/* Set format to colorimetric */
	if ((ev = dtp22_command(p, "0120CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;
	p->mode &= ~inst_spectral;

	/* Set colorimetric to XYZ */
	if ((ev = dtp22_command(p, "0221CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Disable density */
	if ((ev = dtp22_command(p, "0022CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Enable spectral */
	if ((ev = dtp22_command(p, "0126CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Set Illuminant to D50_2 */
	if ((ev = dtp22_command(p, "0427CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;

	/* Read the current calibration values */
//	if ((ev = dtp22_command(p, "0LC\r", buf, MAX_MES_SIZE, 10.2)) != inst_ok)
//		return ev;

	/* See that we have the correct challenge/response key */
	for (i = 0; keys[i].oemsn >= 0; i++) {
		if (keys[i].oemsn == p->oemsn) {
			p->key[0] = keys[i].key[0];
			p->key[1] = keys[i].key[1];
			p->key[2] = keys[i].key[2];
			p->key[3] = keys[i].key[3];
			break;
		}
	}
	if (keys[i].oemsn < 0)
		return inst_unknown_model | DTP22_UNKN_OEM;

	if (ev == inst_ok) {
		p->inited = 1;
		if (p->debug) fprintf(stderr,"dtp22: instrument inited OK\n");
	}

	return ev;
}

/* Read a single sample */
/* Return the instrument error code */
static inst_code
dtp22_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val) {		/* Pointer to instrument patch value */
	dtp22 *p = (dtp22 *)pp;
	char *tp;
	char buf[MAX_RD_SIZE];
	char buf2[50];
	int se;
	inst_code ev = inst_ok;
	int switch_trig = 0;
	int user_trig = 0;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if ((ev = activate_mode(p)) != inst_ok) 
		return ev;

	/* Signal a calibration is needed */
	if (p->need_cal && p->noutocalib == 0) {
		return inst_needs_cal;		/* Get user to calibrate */
	}

	/* Request challenge, so that we can return the response */
	if ((ev = dtp22_command(p, "GP\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;
	
	if (comp_password(buf2, buf, p->key))
		return inst_internal_error | DTP22_INTERNAL_ERROR;

	/* Validate the password */
	strcat(buf2, "VD\r");
	if ((ev = dtp22_command(p, buf2, buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return ev;
	
	if (strncmp(buf,"PASS", 4) != 0)
		return inst_unknown_model | DTP22_BAD_PASSWORD;

	if (p->trig == inst_opt_trig_keyb_switch) {

		/* Enable the read microswitch */
		if ((ev = dtp22_command(p, "3PB\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;

		/* Wait for the microswitch to be triggered, or the user to hit the keyboard */
		for (;;) {
			if ((se = p->icom->read(p->icom, buf, MAX_MES_SIZE, '>', 1, 1.0)) != 0) {
				if ((se & ~ICOM_TO) != 0) {
					if ((se & ICOM_USERM) == ICOM_TRIG) {
						user_trig = 1;
						break;				/* Measure triggered via keyboard */
					}
					/* Abort, term or command */
					/* Disable the read microswitch */
					dtp22_command(p, "2PB\r", buf, MAX_MES_SIZE, 0.2);
					return dtp22_interp_code((inst *)p, icoms2dtp22_err(se));
				}
			} else {	/* Inst error or switch activated */
				if (strlen(buf) >= 4
				 && buf[0] == '<' && isdigit(buf[1]) && isdigit(buf[2]) && buf[3] == '>') {
					if ((ev = dtp22_interp_code((inst *)p, extract_ec(buf))) != inst_ok) {
						dtp22_command(p, "CE\r", buf, MAX_MES_SIZE, 0.5);
						dtp22_command(p, "2PB\r", buf, MAX_MES_SIZE, 0.5);
						return ev;
					}
					switch_trig = 1;
					break;				/* Measure triggered via inst switch */
				}
			}
		}
		/* Disable the read microswitch */
		if ((ev = dtp22_command(p, "2PB\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
		if (p->trig_return)
			printf("\n");

	} else if (p->trig == inst_opt_trig_keyb) {

		if ((se = icoms_poll_user(p->icom, 1)) != ICOM_TRIG) {
			/* Abort, term or command */
			return dtp22_interp_code((inst *)p, icoms2dtp22_err(se));
		}
		user_trig = 1;
		if (p->trig_return)
			printf("\n");
	}

	/* Trigger a read if the switch has not been used */
	if (switch_trig == 0) {
		if ((ev = dtp22_command(p, "RM\r", buf, MAX_RD_SIZE, 20.0)) != inst_ok) {
			return ev; 	/* Misread */
		}
	}

	/* Gather the results in D50_2 XYZ */
	if ((ev = dtp22_command(p, "0SR\r", buf, MAX_RD_SIZE, 5.0)) != inst_ok)
		return ev; 	/* misread */

	/* Parse the buffer */
	/* Replace '\r' with '\000' */
	for (tp = buf; *tp != '\000'; tp++) {
		if (*tp == '\r')
			*tp = '\000';
	}

	if (sscanf(buf, " X %lf Y %lf Z %lf ", &val->XYZ[0], &val->XYZ[1], &val->XYZ[2]) != 3) {
		return inst_protocol_error;
	}

	val->XYZ_v = 1;
	val->aXYZ_v = 0;
	val->Lab_v = 0;
	val->sp.spec_n = 0;
	val->duration = 0.0;

	if (p->mode & inst_mode_spectral) {
		int j;
		char *fmt;

		/* Reset tp to point to start of spectral */
		tp = buf + strlen(buf) + 1;

		/* Different dialects spoken by DTP-22 */
		if (strcmp(tp, "SPECTRAL DATA") == 0 ) {
			tp += strlen(tp) + 1;
			fmt = " w %*lf S %lf ";
		} else {
			fmt = " S %lf ";
		}

		/* Read the spectral value */
		for (j = 0; j < 31; j++) {
			if (sscanf(tp, fmt, &val->sp.spec[j]) != 1)
				return inst_protocol_error;
			tp += strlen(tp) + 1;
		}

		val->sp.spec_n = 31;
		val->sp.spec_wl_short = 400.0;
		val->sp.spec_wl_long = 700.0;
		val->sp.norm = 100.0;
	}

	if (user_trig)
		return inst_user_trig;
	return inst_ok;
}

/* Determine if a calibration is needed. Returns inst_calt_none if not, */
/* inst_calt_unknown if it is unknown, or inst_calt_XXX if needs calibration, */
/* and the first type of calibration needed. */
inst_cal_type dtp22_needs_calibration(inst *pp) {
	dtp22 *p = (dtp22 *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->need_cal && p->noutocalib == 0)
		return inst_calt_ref_white;

	return inst_calt_none;
}

/* Request an instrument calibration. */
/* This is use if the user decides they want to do a calibration, */
/* in anticipation of a calibration (needs_calibration()) to avoid */
/* requiring one during measurement, or in response to measuring */
/* returning inst_needs_cal. Initially us an inst_cal_cond of inst_calc_none, */
/* and then be prepared to setup the right conditions, or ask the */
/* user to do so, each time the error inst_cal_setup is returned. */
inst_code dtp22_calibrate(
inst *pp,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	dtp22 *p = (dtp22 *)pp;
	char buf[MAX_RD_SIZE];
	int se;
	inst_code tv, rv = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	id[0] = '\000';

	/* Default to most likely calibration type */
	if (calt == inst_calt_all) {
		calt = inst_calt_ref_white;
	}

	/* See if it's a calibration we understand */
	if (calt != inst_calt_ref_white
	 && calt != inst_calt_ref_dark)
		return inst_unsupported;

	/* Make sure the conditions are righte for the calbration */
	if (calt == inst_calt_ref_white) {
		sprintf(id, "Serial no. %d",p->plaqueno);
		if (*calc != inst_calc_man_ref_whitek) {
			*calc = inst_calc_man_ref_whitek;
			return inst_cal_setup;
		}
	} else if (calt == inst_calt_ref_dark) {
		if (*calc != inst_calc_man_ref_dark) {
			*calc = inst_calc_man_ref_dark;
			return inst_cal_setup;
		}
	}

	/* Calibration only works when triggered by the read switch... */
	/* Enable the read microswitch */
	if ((rv = dtp22_command(p, "3PB\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return rv;

	if (calt == inst_calt_ref_white) {		/* White calibration */
		if ((rv = activate_mode(p)) != inst_ok) 
			goto do_exit;

		/* Issue white calibration */
		if ((se = p->icom->write(p->icom, "1CA\r", 0.5)) != ICOM_OK) {
			rv = dtp22_interp_code((inst *)p, icoms2dtp22_err(se));
			goto do_exit;
		}

		/* Wait for the microswitch to be triggered, or the user to hit the keyboard */
		for (;;) {
			if ((se = p->icom->read(p->icom, buf, MAX_MES_SIZE, '>', 1, 1.0)) != 0) {
				if ((se & ~ICOM_TO) != 0) {
					/* Abort - abort calibrate */
					p->icom->write_read(p->icom, "CE\r", buf, MAX_MES_SIZE, '>', 1, 0.5);
					rv = dtp22_interp_code((inst *)p, icoms2dtp22_err(se));
					goto do_exit;
				}
			} else {	/* Inst error or switch activated */
				if (strlen(buf) >= 4
				 && buf[0] == '<' && isdigit(buf[1]) && isdigit(buf[2]) && buf[3] == '>') {
					if ((rv = dtp22_interp_code((inst *)p, extract_ec(buf))) != inst_ok) {
						p->icom->write_read(p->icom, "CE\r", buf, MAX_MES_SIZE, '>', 1, 0.5);
					}
					break;			/* Measure triggered via inst switch or error */
				}
			}
		}
		if (p->trig_return)
			printf("\n");
		if (rv != inst_ok)
			goto do_exit;
		p->need_cal = 0;

	} else if (calt == inst_calt_ref_dark) {	/* Black calibration */

		if ((rv = activate_mode(p)) != inst_ok)
			goto do_exit;

		/* Do black calibration */
		if ((rv = dtp22_command(p, "1CB\r", buf, MAX_RD_SIZE, 20)) != inst_ok)
			goto do_exit;

		/* Make calibration permanent */
		if ((rv = dtp22_command(p, "MP\r", buf, MAX_RD_SIZE, 10.0)) != inst_ok)
			goto do_exit;
	}

 do_exit:

	/* Disable the read microswitch */
	if ((tv = dtp22_command(p, "3PB\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok && rv == inst_ok)
		return tv;

	return rv;
}

/* Error codes interpretation */
static char *
dtp22_interp_error(inst *pp, int ec) {
//	dtp22 *p = (dtp22 *)pp;
	ec &= inst_imask;
	switch (ec) {
		case DTP22_INTERNAL_ERROR:
			return "Internal software error";
		case DTP22_COMS_FAIL:
			return "Communications failure";
		case DTP22_UNKNOWN_MODEL:
			return "Not a DTP22 or DTP52";
		case DTP22_DATA_PARSE_ERROR:
			return "Data from DTP didn't parse as expected";
		case DTP22_USER_ABORT:
			return "User hit Abort key";
		case DTP22_USER_TERM:
			return "User hit Terminate key";
		case DTP22_USER_TRIG:
			return "User hit Trigger key";
		case DTP22_USER_CMND:
			return "User hit a Command key";
		case DTP22_UNKN_OEM:
			return "Instrument is an unknown OEM version";
		case DTP22_BAD_PASSWORD:
			return "Instrument password was rejected";

		case DTP22_OK:
			return "No device error";

		case DTP22_BAD_COMMAND:
			return "Unrecognized command";
		case DTP22_PRM_RANGE:
			return "Command parameter out of range";
		case DTP22_MEMORY_OVERFLOW:
			return "Memory bounds error";
		case DTP22_INVALID_BAUD_RATE:
			return "Invalid baud rate";
		case DTP22_TIMEOUT:
			return "Receive timeout";
		case DTP22_SYNTAX_ERROR:
			return "Badly formed parameter";
		case DTP22_INCORRECT_DATA_FORMAT:
			return "Incorrect Data Format";
		case DTP22_WEAK_LAMP:
			return "Lamp is weak";
		case DTP22_LAMP_FAILED:
			return "Lamp has failed";
		case DTP22_UNSTABLE_CAL:
			return "Unstable calibration";
		case DTP22_CAL_GAIN_ERROR:
			return "Error setting gains during calibration";
		case DTP22_SENSOR_FAILURE:
			return "Sensing cell failure";
		case DTP22_BLACK_CAL_TOO_HIGH:
			return "Black calibration values are too high";
		case DTP22_UNSTABLE_BLACK_CAL:
			return "Unstable black calibration";
		case DTP22_CAL_MEM_ERROR:
			return "Memory error with calibration values";
		case DTP22_FILTER_MOTOR:
			return "Filter motor not working";
		case DTP22_LAMP_FAILED_READING:
			return "Lamp failed during reading";
		case DTP22_POWER_INTR_READING:
			return "Power failed during reading";
		case DTP22_SIG_OFFSETS_READING:
			return "Signal offsets exceeded limits during reading";
		case DTP22_RD_SWITCH_TO_SOON:
			return "Read switch released too soon";
		case DTP22_OVERRANGE:
			return "Overrange reading";
		case DTP22_FILT_POS_ERROR:
			return "Filter position sensor error";
		case DTP22_FACT_TST_CONNECT:
			return "Factory test connector error";
		case DTP22_FACT_TST_LAMP_INH:
			return "Factory test lamp inhibit error";

		case DTP22_EEPROM_FAILURE:
			return "EEprom write failure";
		case DTP22_PROGRAM_WRITE_FAIL:
			return "Loading new program error";
		case DTP22_MEMORY_WRITE_FAIL:
			return "Memory write error";
		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
dtp22_interp_code(inst *pp, int ec) {
//	dtp22 *p = (dtp22 *)pp;

	ec &= inst_imask;
	switch (ec) {

		case DTP22_OK:
			return inst_ok;

		case DTP22_INTERNAL_ERROR:
			return inst_internal_error | ec;

		case DTP22_COMS_FAIL:
			return inst_coms_fail | ec;

		case DTP22_UNKNOWN_MODEL:
		case DTP22_UNKN_OEM:
		case DTP22_BAD_PASSWORD:
			return inst_unknown_model | ec;

		case DTP22_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

		case DTP22_USER_ABORT:
			return inst_user_abort | ec;
		case DTP22_USER_TERM:
			return inst_user_term | ec;
		case DTP22_USER_TRIG:
			return inst_user_trig | ec;
		case DTP22_USER_CMND:
			return inst_user_cmnd | ec;

		case DTP22_POWER_INTR_READING:
		case DTP22_RD_SWITCH_TO_SOON:
		case DTP22_OVERRANGE:
		case DTP22_SIG_OFFSETS_READING:
		case DTP22_UNSTABLE_CAL:
		case DTP22_UNSTABLE_BLACK_CAL:
		case DTP22_BLACK_CAL_TOO_HIGH:
		case DTP22_CAL_GAIN_ERROR:				/* Or H/W error ? */
			return inst_misread | ec;

		case DTP22_WEAK_LAMP:
		case DTP22_LAMP_FAILED:
		case DTP22_SENSOR_FAILURE:
		case DTP22_CAL_MEM_ERROR:
		case DTP22_FILTER_MOTOR:
		case DTP22_LAMP_FAILED_READING:
		case DTP22_FACT_TST_CONNECT:
		case DTP22_FACT_TST_LAMP_INH:
		case DTP22_EEPROM_FAILURE:
		case DTP22_PROGRAM_WRITE_FAIL:
		case DTP22_MEMORY_WRITE_FAIL:
		case DTP22_FILT_POS_ERROR:
			return inst_hardware_fail | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
dtp22_del(inst *pp) {
	dtp22 *p = (dtp22 *)pp;
	if (p->icom != NULL)
		p->icom->del(p->icom);
	free(p);
}

/* Return the instrument capabilities */
inst_capability dtp22_capabilities(inst *pp) {
	inst_capability rv;

	rv =  
	  inst_ref_spot
	| inst_colorimeter
	| inst_spectral
	  ;

	return rv;
}

/* Return the instrument capabilities 2 */
inst2_capability dtp22_capabilities2(inst *pp) {
	inst2_capability rv;

	rv = inst2_cal_ref_white
	   | inst2_cal_ref_dark
	   | inst2_prog_trig
	   | inst2_keyb_trig
	   | inst2_keyb_switch_trig
	   | inst2_cal_using_switch		/* DTP22 special */
	   ;

	return rv;
}

/* Activate the last set mode */
static inst_code
activate_mode(dtp22 *p)
{
	static char buf[MAX_MES_SIZE];
	inst_code rv;

	if (p->mode != p->lastmode) {

		if ((p->lastmode & inst_mode_spectral) == inst_mode_spectral
		 && (p->mode     & inst_mode_spectral) != inst_mode_spectral) {
	
			/* Set format to colorimetric + spectral */
			if ((rv = dtp22_command(p, "0020CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
				return rv;
		}
		if ((p->lastmode & inst_mode_spectral) != inst_mode_spectral
		 && (p->mode     & inst_mode_spectral) == inst_mode_spectral) {
	
			/* Set format to just colorimetric */
			if ((rv = dtp22_command(p, "0120CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
				return rv;
		}
		p->mode = p->lastmode;
	}
	return inst_ok;
}

/* 
 * set measurement mode
 */
static inst_code
dtp22_set_mode(inst *pp, inst_mode m)
{
	dtp22 *p = (dtp22 *)pp;
	inst_mode mm;		/* Measurement mode */

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* The measurement mode portion of the mode */
	mm = m & inst_mode_measurement_mask;

	/* General check mode against specific capabilities logic: */
	if (mm != inst_mode_ref_spot) {
		return inst_unsupported;
	}

	p->lastmode = m;

	return inst_ok;
}

/* !! It's not clear if there is a way of knowing */
/* whether the instrument has a UV filter. */

/* 
 * set or reset an optional mode
 */
static inst_code
dtp22_set_opt_mode(inst *pp, inst_opt_mode m, ...)
{
	dtp22 *p = (dtp22 *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Record the trigger mode */
	if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_keyb
	 || m == inst_opt_trig_keyb_switch) {
		p->trig = m;
		return inst_ok;
	}

	if (m == inst_opt_trig_return) {
		p->trig_return = 1;
		return inst_ok;
	} else if (m == inst_opt_trig_no_return) {
		p->trig_return = 0;
		return inst_ok;
	}

	return inst_unsupported;
}

/* Constructor */
extern dtp22 *new_dtp22(icoms *icom, instType itype, int debug, int verb)
{
	dtp22 *p;
	if ((p = (dtp22 *)calloc(sizeof(dtp22),1)) == NULL)
		error("dtp22: malloc failed!");

	if (icom == NULL)
		p->icom = new_icoms();
	else
		p->icom = icom;

	p->debug = debug;
	p->verb = verb;

	p->init_coms                 = dtp22_init_coms;
	p->init_inst                 = dtp22_init_inst;
	p->capabilities              = dtp22_capabilities;
	p->capabilities2             = dtp22_capabilities2;
	p->set_mode                  = dtp22_set_mode;
	p->set_opt_mode              = dtp22_set_opt_mode;
	p->read_sample               = dtp22_read_sample;
	p->needs_calibration         = dtp22_needs_calibration;
	p->calibrate                 = dtp22_calibrate;
	p->interp_error              = dtp22_interp_error;
	p->del                       = dtp22_del;

	p->itype = itype;
	p->mode = inst_mode_unknown;
	p->need_cal = 1;			/* Do a white calibration each time we open the device */

	return p;
}

/* Compute the DTP22/Digital Swatchbook password response. */
/* Return NZ if there was an error */
static int comp_password(char *out, char *in, unsigned char key[4]) {
	unsigned short inv[5];
	unsigned short outv;

	in[10] = '\000';

	/* Convert the 10 hex chars of input to 5 unsigned chars */
	if (sscanf(in, "%2hx%2hx%2hx%2hx%2hx", &inv[0], &inv[1], &inv[2], &inv[3], &inv[4]) != 5)
		return 1;

	/* X-Rite magic... */
	inv[0] ^= key[0];		/* All seen to have 2 bits set in each nibble. */
	inv[1] ^= key[1];		/* ie. taken from set 3,5,6,9,A,C ? */
	inv[2] ^= key[2];
	inv[4] ^= key[3];
	outv = ((inv[0] * 256 + inv[2]) ^ (inv[4] * 256 + inv[1])) + inv[4];

	sprintf(out, "%04x", outv);
	return 0;
}
