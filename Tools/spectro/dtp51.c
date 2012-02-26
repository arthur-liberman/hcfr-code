/* 
 * Argyll Color Correction System
 *
 * Xrite DTP51 related functions
 *
 * Author: Graeme W. Gill
 * Date:   5/10/96
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
#endif /* !SALONEINSTLIB */
#include "numsup.h"
#include "xspect.h"
#include "insttypes.h"
#include "icoms.h"
#include "conv.h"
#include "dtp51.h"

#undef DEBUG

/* Default flow control */
#define DEFFC fc_Hardware

static inst_code interp_code(inst *pp, int ec);

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
	return rv & 0x7f;		/* Mask out MSB */
}

/* Interpret an icoms error into a DTP51 error */
static int icoms2dtp51_err(int se) {
	if (se & ICOM_USERM) {
		se &= ICOM_USERM;
		if (se == ICOM_USER)
			return DTP51_USER_ABORT;
		if (se == ICOM_TERM)
			return DTP51_USER_TERM;
		if (se == ICOM_TRIG)
			return DTP51_USER_TRIG;
		if (se == ICOM_CMND)
			return DTP51_USER_CMND;
	}
	if (se != ICOM_OK)
		return DTP51_COMS_FAIL;
	return DTP51_OK;
}

/* Do a full featured command/response echange with the dtp51 */
/* Return the dtp error code. End on the specified number */
/* of specified characters, or expiry if the specified timeout */
/* Assume standard error code if tc = '>' and ntc = 1 */
/* Return a DTP51 error code */
static int
dtp51_fcommand(
	struct _dtp51 *p,
	char *in,			/* In string */
	char *out,			/* Out string buffer */
	int bsize,			/* Out buffer size */
	char tc,			/* Terminating character */
	int ntc,			/* Number of terminating characters */
	double to) {		/* Timout in seconts */
	int rv, se;

	if ((se = p->icom->write_read(p->icom, in, out, bsize, tc, ntc, to)) != 0) {
#ifdef DEBUG
		printf("dtp51 fcommand: serial i/o failure on write_read '%s'\n",icoms_fix(in));
#endif
		return icoms2dtp51_err(se);
	}
	rv = DTP51_OK;
	if (tc == '>' && ntc == 1) {
		rv = extract_ec(out);
		if (rv > 0) {
			rv &= inst_imask;
			if (rv != DTP51_OK) {	/* Clear the error */
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

/* Do a standard command/response echange with the dtp51 */
/* Return the dtp error code */
static inst_code
dtp51_command(dtp51 *p, char *in, char *out, int bsize, double to) {
	int rv = dtp51_fcommand(p, in, out, bsize, '>', 1, to);
	return interp_code((inst *)p, rv);
}

/* Establish communications with a DTP51 */
/* Use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
dtp51_init_coms(inst *pp, int port, baud_rate br, flow_control fc, double tout) {
	dtp51 *p = (dtp51 *)pp;
	static char buf[MAX_MES_SIZE];
	baud_rate brt[5] = { baud_9600, baud_19200, baud_4800, baud_2400, baud_1200 };
	char *brc[5]     = { "30BR\r",  "60BR\r",   "18BR\r",  "0CBR\r",  "06BR\r" };
	char *fcc;
	long etime;
	int ci, bi, i, rv;
	inst_code ev = inst_ok;

	if (p->debug)
		p->icom->debug = p->debug;	/* Turn on debugging */

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

	/* Figure DTP51 baud rate being asked for */
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

		/* Until we time out, find the correct baud rate */
		for (i = ci; msec_time() < etime;) {
			p->icom->set_ser_port(p->icom, port, fc_none, brt[i], parity_none, stop_1, length_8);
			if (((ev = dtp51_command(p, "\r", buf, MAX_MES_SIZE, 0.5)) & inst_mask)
			                                                    != inst_coms_fail)
				break;		/* We've got coms or user abort */
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
	if ((ev = dtp51_command(p, fcc, buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Change the baud rate to the rate we've been told */
	if ((rv = p->icom->write_read(p->icom, brc[bi], buf, MAX_MES_SIZE, '>', 1, 1.5)) != 0) {
		if (extract_ec(buf) != DTP51_OK)
			return inst_coms_fail;
	}

	/* Configure our baud rate and handshaking as well */
	p->icom->set_ser_port(p->icom, port, fc, brt[bi], parity_none, stop_1, length_8);

	/* Loose a character (not sure why) */
	p->icom->write_read(p->icom, "\r", buf, MAX_MES_SIZE, '>', 1, 0.5);

	/* Check instrument is responding */
	if ((ev = dtp51_command(p, "\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return inst_coms_fail;

	if (p->verb) {
		int i, j;
		if ((ev = dtp51_command(p, "GI\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok
		 && (ev & inst_imask) != DTP51_BAD_COMMAND)
			return ev;

		if ((ev & inst_imask) == DTP51_BAD_COMMAND) { /* Some fimware doesn't support GI */
			printf("Firware doesn't support GI command\n");
		} else {
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
	}

#ifdef DEBUG
	printf("Got communications\n");
#endif
	p->gotcoms = 1;
	return inst_ok;
}

/* Build a strip definition as a set of passes */
static void
build_strip(
char *tp,			/* pointer to string buffer */
char *name,			/* Strip name (7 chars) */
int npatch,			/* Number of patches in the pass */
char *pname,		/* Pass name (3 chars) */
int sguide) {		/* Guide number */
	int i;

	/* Per strip */
	for (i = 0; i < 7 && *name != '\000'; i++)
		*tp++ = *name++;	/* Strip name padded to 7 characters */
	for (; i < 7; i++)
		*tp++ = ' ';
	*tp++ = '1'; *tp++ = '0'; *tp++ = '0'; *tp++ = '0';	/* N factor */
	*tp++ = 'F';				/* Forward order */
	*tp++ = '1';				/* No min/max transmitted */
	*tp++ = '0';				/* No absolute data, no extra steps */
	*tp++ = '0'; *tp++ = '0'; *tp++ = '0'; *tp++ = '0'; *tp++ = '0';	/* Reserved */

	/* Per pass */
	for (i = 0; i < 3 && *pname != '\000'; i++)
		*tp++ = *pname++;		/* Pass name padded to 3 characters */
	for (; i < 3; i++)
		*tp++ = ' ';
	/* *tp++ = '4'; */			/* Lab data */
	*tp++ = '5';				/* XYZ data */
	*tp++ = '8';				/* Auto color */
	*tp++ = '0' + npatch/10;	/* Number of patches MS */
	*tp++ = '0' + npatch%10;	/* Number of patches LS */
	*tp++ = '0' + sguide/10;	/* Guide location MS */
	*tp++ = '0' + sguide%10;	/* Guide location LS */
	*tp++ = '0';				/* (Data output type) */
	*tp++ = '0';				/* Extra steps */
	*tp++ = '0';				/* Reserved */

	*tp++ = '\r';				/* The CR */
	*tp++ = '\000';				/* The end */
}
		
/* Initialise the DTP51. The spectral flag is ignored. */
/* return non-zero on an error, with dtp error code */
static inst_code
dtp51_init_inst(inst *pp) {
	dtp51 *p = (dtp51 *)pp;
	static char tbuf[100], buf[MAX_MES_SIZE];
	int rv;
	inst_code ev = inst_ok;

	if (p->gotcoms == 0)
		return inst_internal_error;		/* Must establish coms before calling init */

	/* Reset it */
	if ((ev = dtp51_command(p, "0PR\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;
	msec_sleep(2000);	/* Let it recover from reset */

	/* Turn echoing of characters off */
	if ((ev = dtp51_command(p, "EC\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Get the model and version number */
	if ((ev = dtp51_command(p, "SV\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Check that it is a DTP51 or 52 */
	if (   strlen(buf) < 12
	    || strncmp(buf,"X-Rite DTP5",11) != 0
	    || (buf[11] != '1' && buf[11] != '2'))
		return inst_unknown_model;

	/* Set the A/D Conversion rate to normal */
	if ((ev = dtp51_command(p, "00AD\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Disable Bar code reading to save time */
	if ((ev = dtp51_command(p, "0BC\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set the Calibration Check tolerance to default of 0.15D */
	if ((ev = dtp51_command(p, "0FCC\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set Automatic Transmit off */
	if ((ev = dtp51_command(p, "0005CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set Read Status on */
	if ((ev = dtp51_command(p, "1RS\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set decimal pont on */
	if ((ev = dtp51_command(p, "0106CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set color data separator to TAB */
	if ((ev = dtp51_command(p, "0207CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set default strip format to off */
	if ((ev = dtp51_command(p, "0009CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set extra digit resolution */
	if ((ev = dtp51_command(p, "010ACF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set data after pass to off */
	if ((ev = dtp51_command(p, "000BCF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

#ifdef NEVER	/* Doesn't seem to work on DTP51 */
	/* Set the patch detection window to default HRW = 3, LRW 0.122% */
	if ((ev = dtp51_command(p, "2CW\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;
#endif

	/* Enable the LCD display */
	if ((ev = dtp51_command(p, "EL\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Enable the read microswitch */
	if ((ev = dtp51_command(p, "1SM\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set a strip length of 1, to ensure parsing is invalidated */
	build_strip(tbuf, "       ", 1, "   ", 30);
	
	if ((rv = dtp51_fcommand(p, "0105DS\r", buf, MAX_MES_SIZE, '*', 1, 0.5)) != DTP51_OK)
		return interp_code(pp, rv); 

	/* Expect '*' as response */
	if (buf[0] != '*' || buf[1] != '\000')
		return inst_coms_fail;

	/* Send strip definition */
	if ((ev = dtp51_command(p, tbuf, buf, MAX_MES_SIZE, 4.0)) != inst_ok)
		return ev;

	/* This is the only mode supported */
	p->trig = inst_opt_trig_switch;

	if (ev == inst_ok)
		p->inited = 1;

	return ev;
}

/* Read a set of strips */
/* Return the dtp error code */
static inst_code
dtp51_read_strip(
inst *pp,
char *name,			/* Strip name (up to first 7 chars used) */
int npatch,			/* Number of patches in the pass */
char *pname,		/* Pass name (up to first 3 chars used) */
int sguide,			/* Guide number */
double pwid,		/* Patch length in mm (For DTP41) */
double gwid,		/* Gap length in mm (For DTP41) */
double twid,		/* Trailer length in mm (For DTP41T) */
ipatch *vals) {		/* Pointer to array of instrument patch values */
	dtp51 *p = (dtp51 *)pp;
	char tbuf[200], *tp;
	static char buf[MAX_RD_SIZE];
	int i, rv;
	inst_code ev = inst_ok;

	build_strip(tbuf, name, npatch, pname, sguide);
	
	if ((rv = dtp51_fcommand(p, "0105DS\r", buf, MAX_RD_SIZE, '*', 1, 0.5)) != DTP51_OK)
		return interp_code(pp, rv); 

	/* Expect '*' as response */
	if (buf[0] != '*' || buf[1] != '\000')
		return inst_coms_fail;

	/* Send strip definition */
	if ((ev = dtp51_command(p, tbuf, buf, MAX_RD_SIZE, 4.0)) != inst_ok)
		return ev; 

	/* Do a strip read */
	if ((ev = dtp51_command(p, "5SW\r", buf, MAX_RD_SIZE, 1.5)) != inst_ok) {
		if ((ev & inst_mask) == inst_needs_cal)
			p->need_cal = 1;
		return ev;
	}

	/* Wait for the Read status, or a user abort - allow 5 munutes. */
	ev = dtp51_command(p, "", buf, MAX_RD_SIZE, 5 * 60.0);

#ifdef NEVER		/* Why was this being done /? */
	/* Soft reset the unit */
	dtp51_command(p, "0PR\r", buf, MAX_RD_SIZE); /* Soft Reset it */

	if (ev != inst_user_abort && ev != inst_user_term && ev != inst_user_cmnd)
		sleep(2);	/* Let it recover from reset */
#endif

	if (ev != inst_ok) {
		if ((ev & inst_mask) == inst_needs_cal)
			p->need_cal = 1;
		return ev; 	/* misread or user abort */
	}

	if (p->trig_return)
		printf("\n");

	/* Gather the results */
	if ((ev = dtp51_command(p, "TS\r", buf, MAX_RD_SIZE, 0.5 + npatch * 0.1)) != inst_ok)
		return ev;

	/* Parse the buffer */
	/* Replace '\r' with '\000' */
	for (tp = buf; *tp != '\000'; tp++) {
		if (*tp == '\r')
			*tp = '\000';
	}
	for (tp = buf, i = 0; i < npatch; i++) {
		if (*tp == '\000')
			return inst_protocol_error;
#ifdef NEVER	/* Lab */
		if (sscanf(tp, " L %lf a %lf b %lf ",
		           &vals[i].Lab[0], &vals[i].Lab[1], &vals[i].Lab[2]) != 3) {
			if (sscanf(tp, " l %lf a %lf b %lf ",
			           &vals[i].Lab[0], &vals[i].Lab[1], &vals[i].Lab[2]) != 3) {
				return inst_protocol_error;
			}
		}
		vals[i].XYZ_v = 0;
		vals[i].aXYZ_v = 0
		vals[i].Lab_v = 1;
		vals[i].sp.spec_n = 0;
		vals[i].duration = 0.0;
#else /* XYZ */
		if (sscanf(tp, " X %lf Y %lf Z %lf ",
		           &vals[i].XYZ[0], &vals[i].XYZ[1], &vals[i].XYZ[2]) != 3) {
			if (sscanf(tp, " x %lf y %lf z %lf ",
			           &vals[i].XYZ[0], &vals[i].XYZ[1], &vals[i].XYZ[2]) != 3) {
				return inst_protocol_error;
			}
		}
		vals[i].XYZ_v = 1;
		vals[i].aXYZ_v = 0;
		vals[i].Lab_v = 0;
		vals[i].sp.spec_n = 0;
		vals[i].duration = 0.0;
#endif
		tp += strlen(tp) + 1;
	}

#ifdef NEVER
	/* Disable the read microswitch */
	if ((ev = dtp51_command(p, "0SM\r", buf, MAX_RD_SIZE)) != inst_ok)
		return ev;
#endif

	return inst_ok;
}

/* Determine if a calibration is needed. Returns inst_calt_none if not, */
/* inst_calt_unknown if it is unknown, or inst_calt_XXX if needs calibration, */
/* and the first type of calibration needed. */
inst_cal_type dtp51_needs_calibration(inst *pp) {
	dtp51 *p = (dtp51 *)pp;

	if (p->need_cal)
		return inst_calt_ref_white;
	return inst_calt_unknown;
}

/* Request an instrument calibration. */
/* This is used if the user decides they want to do a calibration, */
/* in anticipation of a calibration (needs_calibration()) to avoid */
/* requiring one during measurement, or in response to measuring */
/* returning inst_needs_cal. Initially us an inst_cal_cond of inst_calc_none, */
/* and then be prepared to setup the right conditions, or ask the */
/* user to do so, each time the error inst_cal_setup is returned. */
/* NOTE :- we can trigger a calibration ! */
/* Perhaps we should do so ??? */
inst_code dtp51_calibrate(
inst *pp,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	dtp51 *p = (dtp51 *)pp;
	id[0] = '\000';

	if (calt == inst_calt_all)
		calt = inst_calt_ref_white;

	if (calt != inst_calt_ref_white)
		return inst_unsupported;

	if (*calc == inst_calc_uop_ref_white) {
		p->need_cal = 0;
		return inst_ok;	/* Calibration done */
	}

	*calc = inst_calc_uop_ref_white;	/* Need to ask user to do calibration */
	return inst_cal_setup;
}

/* Error codes interpretation */
static char *
dtp51_interp_error(inst *pp, int ec) {
//	dtp51 *p = (dtp51 *)pp;
	ec &= inst_imask;
	switch (ec) {
		case DTP51_USER_ABORT:
			return "User hit Abort key";
		case DTP51_USER_TERM:
			return "User hit Terminate key";
		case DTP51_USER_TRIG:
			return "User hit Trigger key";
		case DTP51_USER_CMND:
			return "User hit a Command key";
		case DTP51_INTERNAL_ERROR:
			return "Internal software error";
		case DTP51_COMS_FAIL:
			return "Communications failure";
		case DTP51_UNKNOWN_MODEL:
			return "Not a DTP51 or DTP52";
		case DTP51_DATA_PARSE_ERROR:
			return "Data from DTP didn't parse as expected";
		case DTP51_OK:
			return "No device error";
		case DTP51_BAD_COMMAND:
			return "Unrecognized command";
		case DTP51_PRM_RANGE:
			return "Command parameter out of range";
		case DTP51_DISPLAY_OVERFLOW:
			return "Display overflow";
		case DTP51_MEMORY_OVERFLOW:
			return "Memory bounds error";
		case DTP51_INVALID_BAUD_RATE:
			return "Invalid baud rate";
		case DTP51_TIMEOUT:
			return "Receive timeout";
		case DTP51_INVALID_PASS:
			return "Invalid pass";
		case DTP51_INVALID_STEP:
			return "Invalid step";
		case DTP51_NO_DATA_AVAILABLE:
			return "No data availble";
		case DTP51_LAMP_MARGINAL:
			return "Lamp marginal";
		case DTP51_LAMP_FAILURE:
			return "Lamp failure";
		case DTP51_STRIP_RESTRAINED:
			return "Strip was restrained";
		case DTP51_BAD_CAL_STRIP:
			return "Bad calibration strip";
		case DTP51_MOTOR_ERROR:
			return "Motor error";
		case DTP51_BAD_BARCODE:
			return "Bad barcode on cal strip";
		case DTP51_INVALID_READING:
			return "Invalid strip reading";
		case DTP51_WRONG_COLOR:
			return "Wrong color strip";
		case DTP51_BATTERY_TOO_LOW:
			return "Battery too low";
		case DTP51_NEEDS_CALIBRATION:
			return "Needs calibration";
		case DTP51_COMP_TABLE_MISMATCH:
			return "Compensation table mismatch";
		case DTP51_BAD_COMP_TABLE:
			return "Bad compensation table";
		case DTP51_NO_VALID_DATA:
			return "No valid data found";
		case DTP51_BAD_PATCH:
			return "Bad patch in strip";
		case DTP51_BAD_STRING_LENGTH:
			return "Bad strip def. string length";
		case DTP51_BAD_CHARACTER:
			return "Bad chareter";
		case DTP51_BAD_MEAS_TYPE:
			return "Bad measure type field";
		case DTP51_BAD_COLOR:
			return "Bad color field";
		case DTP51_BAD_STEPS:
			return "Bad step field";
		case DTP51_BAD_STOP_LOCATION:
			return "Bad guide stop field";
		case DTP51_BAD_OUTPUT_TYPE:
			return "Bad output type field";
		case DTP51_MEMORY_ERROR:
			return "Memory error (need AC supply)";
		case DTP51_BAD_N_FACTOR:
			return "Bad N factore";
		case DTP51_STRIP_DOESNT_EXIST:
			return "Strip doesn't exist";
		case DTP51_BAD_MIN_MAX_VALUE:
			return "Bad min/max field value";
		case DTP51_BAD_SERIAL_NUMBER:
			return "Bad serial number";
		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
interp_code(inst *pp, int ec) {
//	dtp51 *p = (dtp51 *)pp;

	ec &= inst_imask;
	switch (ec) {

		case DTP51_OK:
			return inst_ok;

		case DTP51_INTERNAL_ERROR:
			return inst_internal_error | ec;

		case DTP51_COMS_FAIL:
			return inst_coms_fail | ec;

		case DTP51_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case DTP51_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

		case DTP51_USER_ABORT:
			return inst_user_abort | ec;
		case DTP51_USER_TERM:
			return inst_user_term | ec;
		case DTP51_USER_TRIG:
			return inst_user_trig | ec;
		case DTP51_USER_CMND:
			return inst_user_cmnd | ec;

		case DTP51_NO_DATA_AVAILABLE:
		case DTP51_LAMP_MARGINAL:
		case DTP51_LAMP_FAILURE:
		case DTP51_STRIP_RESTRAINED:
		case DTP51_MOTOR_ERROR:
		case DTP51_INVALID_READING:
		case DTP51_WRONG_COLOR:
		case DTP51_BATTERY_TOO_LOW:
		case DTP51_COMP_TABLE_MISMATCH:
		case DTP51_BAD_COMP_TABLE:
		case DTP51_NO_VALID_DATA:
		case DTP51_BAD_PATCH:
			return inst_misread | ec;

		case DTP51_NEEDS_CALIBRATION:
			return inst_needs_cal | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
dtp51_del(inst *pp) {
	dtp51 *p = (dtp51 *)pp;
	if (p->icom != NULL)
		p->icom->del(p->icom);
	free(p);
}

/* Return the instrument capabilities */
inst_capability dtp51_capabilities(inst *pp) {
//	dtp51 *p = (dtp51 *)pp;

	return 
	  inst_ref_strip
	| inst_colorimeter
	  ;
}

/* Return the instrument capabilities 2 */
inst2_capability dtp51_capabilities2(inst *pp) {
	inst2_capability rv;

	rv = inst2_cal_ref_white		/* Currently user operated though */
	   | inst2_switch_trig		/* Can only be triggered by microswitch */
	   ;

	return rv;
}

/* Set device measurement mode */
inst_code dtp51_set_mode(inst *pp, inst_mode m)
{
	inst_mode mm;		/* Measurement mode */

	/* The measurement mode portion of the mode */
	mm = m & inst_mode_measurement_mask;

	/* only reflection strip measurement mode suported */
	if (mm != inst_mode_ref_strip) {
		return inst_unsupported;
	}

	/* Spectral mode is not supported */
	if (m & inst_mode_spectral)
		return inst_unsupported;

	return inst_ok;
}

/* !! It's not clear if there is a way of knowing */
/* whether the instrument has a UV filter. */

/* 
 * set or reset an optional mode
 * We assume that the instrument has been initialised.
 */
static inst_code
dtp51_set_opt_mode(inst *pp, inst_opt_mode m, ...)
{
	dtp51 *p = (dtp51 *)pp;

	/* Record the trigger mode */
	if (m == inst_opt_trig_switch) {	/* Can only be triggered this way */
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
extern dtp51 *new_dtp51(icoms *icom, int debug, int verb)
{
	dtp51 *p;
	if ((p = (dtp51 *)calloc(sizeof(dtp51),1)) == NULL)
		error("dtp51: malloc failed!");

	if (icom == NULL)
		p->icom = new_icoms();
	else
		p->icom = icom;

	p->debug = debug;
	p->verb = verb;

	p->init_coms    	= dtp51_init_coms;
	p->init_inst    	= dtp51_init_inst;
	p->capabilities 	= dtp51_capabilities;
	p->capabilities2 	= dtp51_capabilities2;
	p->set_mode     	= dtp51_set_mode;
	p->set_opt_mode     = dtp51_set_opt_mode;
	p->read_strip   	= dtp51_read_strip;
	p->needs_calibration = dtp51_needs_calibration;
	p->calibrate    	= dtp51_calibrate;
	p->interp_error 	= dtp51_interp_error;
	p->del          	= dtp51_del;

	p->itype = instDTP51;

	return p;
}
