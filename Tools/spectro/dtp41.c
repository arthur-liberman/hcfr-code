
/* 
 * Argyll Color Correction System
 *
 * Xrite DTP41 related functions
 *
 * Author: Graeme W. Gill
 * Date:   10/3/2001
 *
 * Copyright 1996 - 2007, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Derived from DTP51.c
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
#include <stdarg.h>
#include <time.h>
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#endif /* !SALONEINSTLIB */
#include "numsup.h"
#include "xspect.h"
#include "insttypes.h"
#include "icoms.h"
#include "conv.h"
#include "dtp41.h"

#undef DEBUG

/* Default flow control */
#define DEFFC fc_XonXOff

static inst_code dtp41_interp_code(inst *pp, int ec);
static inst_code activate_mode(dtp41 *p);

#define MAX_MES_SIZE 1000		/* Maximum normal message reply size */
#define MAX_RD_SIZE 100000		/* Maximum reading messagle reply size */

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
	rv &= 0x7f;
	return rv;
}

/* Interpret an icoms error into a DTP41 error */
static int icoms2dtp41_err(int se) {
	if (se & ICOM_USERM) {
		se &= ICOM_USERM;
		if (se == ICOM_USER)
			return DTP41_USER_ABORT;
		if (se == ICOM_TERM)
			return DTP41_USER_TERM;
		if (se == ICOM_TRIG)
			return DTP41_USER_TRIG;
		if (se == ICOM_CMND)
			return DTP41_USER_CMND;
	}
	if (se != ICOM_OK)
		return DTP41_COMS_FAIL;
	return DTP41_OK;
}

/* Do a full featured command/response echange with the dtp41 */
/* End on the specified number of characters, or expiry if */
/* the specified timeout. */
/* Assume standard error code if tc = '>' and ntc = 1 */
/* Return a DTP41 error code */
static int
dtp41_fcommand(
dtp41 *p,
char *in,			/* In string */
char *out,			/* Out string buffer */
int bsize,			/* Out buffer size */
char tc,			/* Terminating character */
int ntc,			/* Number of terminating characters */
double to) {		/* Timout in seconts */
	int rv, se;

	if ((se = p->icom->write_read(p->icom, in, out, bsize, tc, ntc, to)) != 0) {
#ifdef DEBUG
		printf("dtp41 fcommand: serial i/o failure on write_read '%s'\n",icoms_fix(in));
#endif
		return icoms2dtp41_err(se);
	}
	rv = DTP41_OK;
	if (tc == '>' && ntc == 1) {
		rv = extract_ec(out);
		if (rv > 0) {
			rv &= inst_imask;
			if (rv != DTP41_OK) {	/* Clear the error */
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

/* Do a standard command/response echange with the dtp41 */
/* Return the instrument error code */
static inst_code
dtp41_command(dtp41 *p, char *in, char *out, int bsize, double to) {
	int rv = dtp41_fcommand(p, in, out, bsize, '>', 1, to);
	return dtp41_interp_code((inst *)p, rv);
}

/* Establish communications with a DTP41 */
/* Use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
dtp41_init_coms(inst *pp, int port, baud_rate br, flow_control fc, double tout) {
	dtp41 *p = (dtp41 *)pp;
	static char buf[MAX_MES_SIZE];
	baud_rate brt[9] = { baud_9600, baud_19200, baud_38400, baud_57600,
	                     baud_4800, baud_2400, baud_1200, baud_600, baud_300 };
	char *brc[9] =     { "9600BR\r", "19200BR\r", "38400BR\r", "57600BR\r",
	                     "4800BR\r", "2400BR\r", "1200BR\r", "600BR\r", "300BR\r" };
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

	/* Figure DTP41 baud rate being asked for */
	for (bi = 0; bi < 9; bi++) {
		if (brt[bi] == br)
			break;
	}
	if (bi >= 9)
		bi = 0;	

	/* Figure current icoms baud rate */
	for (ci = 0; ci < 9; ci++) {
		if (brt[ci] == p->icom->br)
			break;
	}
	if (ci >= 9)
		ci = bi;	

	/* The tick to give up on */
	etime = msec_time() + (long)(1000.0 * tout + 0.5);

	while (msec_time() < etime) {

		/* Until we time out, find the correct baud rate */
		for (i = ci; msec_time() < etime;) {
			p->icom->set_ser_port(p->icom, port, fc_none, brt[i], parity_none, stop_1, length_8);
			if (((ev = dtp41_command(p, "\r", buf, MAX_MES_SIZE, 0.5)) & inst_mask)
			                                                      != inst_coms_fail)
				break;		/* We've got coms or user abort */
			if (++i >= 9)
				i = 0;
		}

		if ((ev & inst_mask) == inst_user_abort)
			return ev;

		break;		/* Got coms */
	}

	if (msec_time() >= etime) {		/* We haven't established comms */
		return inst_coms_fail;
	}

	/* set the protocol to RCI */
	if ((ev = dtp41_command(p, "0012CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set the handshaking (cope with coms breakdown) */
	if ((rv = p->icom->write_read(p->icom, fcc, buf, MAX_MES_SIZE, '>', 1, 1.5)) != 0) {
		if (extract_ec(buf) != DTP41_OK)
			return inst_coms_fail;
	}

	/* Change the baud rate to the rate we've been told (cope with coms breakdown) */
	if ((rv = p->icom->write_read(p->icom, brc[bi], buf, MAX_MES_SIZE, '>', 1, 1.5)) != 0) {
		if (extract_ec(buf) != DTP41_OK)
			return inst_coms_fail;
	}

	/* Configure our baud rate and handshaking as well */
	p->icom->set_ser_port(p->icom, port, fc, brt[bi], parity_none, stop_1, length_8);

	/* Loose a character (not sure why) */
	p->icom->write_read(p->icom, "\r", buf, MAX_MES_SIZE, '>', 1, 0.5);

	/* Check instrument is responding */
	if ((ev = dtp41_command(p, "\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return inst_coms_fail;

#ifdef DEBUG
	printf("Got communications\n");
#endif
	p->gotcoms = 1;
	return inst_ok;
}

/* Build a strip definition as a set of passes, including DS command */
static void
build_strip(
dtp41 *p,
char *tp,			/* pointer to string buffer */
char *name,			/* Strip name (7 chars) (not used) */
int npatch,			/* Number of patches in the pass */
char *pname,		/* Pass name (3 chars) (not used) */
int sguide,			/* Guide number (not used) */
double pwid,		/* Patch length in mm */
double gwid,		/* Gap length in mm */
double twid			/* Trailer length in mm (DTP41T only) */
) {

	/* Number of patches in strip */
	sprintf(tp, "%03d",npatch);
	tp += 3;

	/* Patch width in mm, dd.dd */
	sprintf(tp, "%05.2f",pwid);
	tp[2] = tp[3];	/* Remove point */
	tp[3] = tp[4];
	tp += 4;

	/* Gap width in mm, dd.dd */
	sprintf(tp, "%05.2f",gwid);
	tp[2] = tp[3];	/* Remove point */
	tp[3] = tp[4];
	tp += 4;

	*tp++ = '0';	/* Normal strip */

	*tp++ = '8';	/* Auto type */

	if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission) {

		if (twid >= 9999.5)
			error ("build_strip given trailer length > 9999 mm");
		/* Trailer length in mm, dddd */
		sprintf(tp, "%04.0f",twid);
		tp += 4;
	}

	*tp++ = 'D';				/* The DS command */
	*tp++ = 'S';
	*tp++ = '\r';				/* The CR */
	*tp++ = '\000';				/* The end */

}
		
/* Initialise the DTP41. */
/* return non-zero on an error, with instrument error code */
static inst_code
dtp41_init_inst(inst *pp) {
	dtp41 *p = (dtp41 *)pp;
	static char tbuf[100], buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;

	if (p->gotcoms == 0)
		return inst_internal_error;		/* Must establish coms before calling init */

	/* Resetting instrument resets the baud rate, so do manual reset. */

	/* Set emulation mode to DTP41 */
	if ((ev = dtp41_command(p, "0010CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Turn echoing of characters off */
	if ((ev = dtp41_command(p, "0009CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set Response delimeter to CR */
	if ((ev = dtp41_command(p, "0008CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Get the model and version number */
	if ((ev = dtp41_command(p, "SV\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Check that it is a DTP41 */
	if (   strlen(buf) < 12
	    || strncmp(buf,"X-Rite DTP41",11) != 0
	    || (buf[11] != '1' && buf[11] != '2'))
		return inst_unknown_model;

	/* Set Language to English */
	if ((ev = dtp41_command(p, "0000CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set Beeper to medium */
	if ((ev = dtp41_command(p, "0201CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set Automatic Transmit off */
	if ((ev = dtp41_command(p, "0005CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set decimal point on */
	if ((ev = dtp41_command(p, "0106CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set color data separator to TAB */
	if ((ev = dtp41_command(p, "0207CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set 2 decimal digit resolution */
	if ((ev = dtp41_command(p, "020ACF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set Min/Max mode off */
	if ((ev = dtp41_command(p, "000CCF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set persistent errors off */
	if ((ev = dtp41_command(p, "000DCF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set show data labels mode off */
	if ((ev = dtp41_command(p, "000FCF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set drive motor calibration at power up to off */
	if ((ev = dtp41_command(p, "0011CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set Reflection calibration timeout to 24 Hrs */
	if ((ev = dtp41_command(p, "181ECF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set Trailer timout to 2 seconds */
	if ((ev = dtp41_command(p, "021FCF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set Transmission calibration timeout to 24 Hrs */
	if ((ev = dtp41_command(p, "1820CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok) {
		/* This may fail if the firmware version is < v8212 */
		if ((ev & inst_imask) != DTP41_PRM_RANGE_ERROR)
			return ev;
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - */
	/* Setup for the type of measurements we want to do */
	/* Enable the read microswitch */
	if ((ev = dtp41_command(p, "01PB\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;
	p->trig = inst_opt_trig_keyb_switch;

	/* Set dynamic measurement mode */
	if ((ev = dtp41_command(p, "0113CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set instrument to reflectance mode */
	if ((ev = dtp41_command(p, "0019CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set data format to Reflectance, so TS can select. */
	if ((ev = dtp41_command(p, "0318CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set density format to spectral */
	if ((ev = dtp41_command(p, "0417CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set Illuminant to D50_2 */
	if ((ev = dtp41_command(p, "0416CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set static samples to 10 */
	if ((ev = dtp41_command(p, "0A14CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set static readings to configured number (usually 5) */
	sprintf(tbuf, "%02x15CF\r", p->nstaticr);
	if ((ev = dtp41_command(p, tbuf, buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

#ifndef NEVER
	/* See what the transmission mode is up to */
	dtp41_command(p, "DEVELOPERPW\r", buf, MAX_MES_SIZE, 1.5);
	dtp41_command(p, "0119CF\r", buf, MAX_MES_SIZE, 1.5);
	dtp41_command(p, "36OD\r", buf, MAX_MES_SIZE, 1.5);
	dtp41_command(p, "1422OD\r", buf, MAX_MES_SIZE, 1.5);
#endif

	/* We are configured in this mode now */
	p->mode = inst_mode_ref_strip;

	if (p->lastmode != p->mode)
		return activate_mode(p);

	if (ev == inst_ok)
		p->inited = 1;

	return inst_ok;
}

/* Read a set of strips */
/* Return the instrument error code */
static inst_code
dtp41_read_strip(
inst *pp,
char *name,			/* Strip name (7 chars) */
int npatch,			/* Number of patches in the pass */
char *pname,		/* Pass name (3 chars) */
int sguide,			/* Guide number */
double pwid,		/* Patch length in mm (For DTP41) */
double gwid,		/* Gap length in mm (For DTP41) */
double twid,		/* Trailer length in mm (For DTP41T) */
ipatch *vals) {		/* Pointer to array of instrument patch values */
	dtp41 *p = (dtp41 *)pp;
	char tbuf[200], *tp;
	static char buf[MAX_RD_SIZE];
	int i, se;
	inst_code ev = inst_ok;
	int switch_trig = 0;
	int user_trig = 0;

	/* Configure for dynamic mode */
	p->lastmode = (p->lastmode & ~inst_mode_sub_mask) | inst_mode_strip;
	activate_mode(p);

	build_strip(p, tbuf, name, npatch, pname, sguide, pwid, gwid, twid);
	
	/* Send strip definition */
	if ((ev = dtp41_command(p, tbuf, buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	if (p->trig == inst_opt_trig_keyb_switch) {

		/* Wait for the Read status, or a user abort/command - allow 5 minuits */
		if ((ev = dtp41_command(p, "", buf, MAX_MES_SIZE, 5 * 60.0)) != inst_ok) {
			if ((ev & inst_mask) == inst_needs_cal)
				p->need_cal = 1;
			if ((ev & inst_mask) != inst_user_trig)
				return ev;
			user_trig = 1;
		} else {
			switch_trig = 1;
		}
		if (p->trig_return)
			printf("\n");

	} else if (p->trig == inst_opt_trig_keyb) {
		if ((se = icoms_poll_user(p->icom, 1)) != ICOM_TRIG) {
			/* Abort, term or command */
			return dtp41_interp_code((inst *)p, icoms2dtp41_err(se));
		}
		user_trig = 1;
		if (p->trig_return)
			printf("\n");
	}

	/* Trigger a read if the switch has not been used */
	if (switch_trig == 0) {
		/* Do a strip read */
		if ((ev = dtp41_command(p, "RM\r", buf, MAX_MES_SIZE, 30.0)) != inst_ok) {
			if ((ev & inst_mask) == inst_needs_cal)
				p->need_cal = 1;
			return ev;
		}
	}

	/* Gather the results in D50_2 XYZ */
	if ((ev = dtp41_command(p, "0405TS\r", buf, MAX_RD_SIZE, 0.5 + npatch * 0.1)) != inst_ok)
		return ev; 	/* Strip misread */

	/* Parse the buffer */
	/* Replace '\r' with '\000' */
	for (tp = buf; *tp != '\000'; tp++) {
		if (*tp == '\r')
			*tp = '\000';
	}
	for (tp = buf, i = 0; i < npatch; i++) {
		if (*tp == '\000')
			return inst_protocol_error;
		if (sscanf(tp, " %lf %lf %lf ",
		           &vals[i].XYZ[0], &vals[i].XYZ[1], &vals[i].XYZ[2]) != 3) {
			if (sscanf(tp, " %lf %lf %lf ",
			           &vals[i].XYZ[0], &vals[i].XYZ[1], &vals[i].XYZ[2]) != 3) {
				return inst_protocol_error;
			}
		}
		vals[i].XYZ_v = 1;
		vals[i].aXYZ_v = 0;
		vals[i].Lab_v = 0;
		vals[i].sp.spec_n = 0;
		vals[i].duration = 0.0;
		tp += strlen(tp) + 1;
	}

	if (p->mode & inst_mode_spectral) {

		/* Gather the results in Spectral reflectance */
		if ((ev = dtp41_command(p, "0403TS\r", buf, MAX_RD_SIZE, 0.5 + npatch * 0.1)) != inst_ok)
			return ev; 	/* Strip misread */
	
		/* Parse the buffer */
		/* Replace '\r' with '\000' */
		for (tp = buf; *tp != '\000'; tp++) {
			if (*tp == '\r')
				*tp = '\000';
		}
		/* Get each patches spetra */
		for (tp = buf, i = 0; i < npatch; i++) {
			int j;
			char *tpp;
			if (strlen(tp) < (31 * 8 - 1)) {
				return inst_protocol_error;
			}

			/* Read the spectral value */
			for (tpp = tp, j = 0; j < 31; j++, tpp += 8) {
				char c;
				c = tpp[7];
				tpp[7] = '\000';
				vals[i].sp.spec[j] = atof(tpp);
				tpp[7] = c;
			}

			vals[i].sp.spec_n = 31;
			vals[i].sp.spec_wl_short = 400.0;
			vals[i].sp.spec_wl_long = 700.0;
			vals[i].sp.norm = 100.0;
			tp += strlen(tp) + 1;
		}
	}
	if (user_trig)
		return inst_user_trig;
	return inst_ok;
}

/* Read a single sample */
/* Return the instrument error code */
static inst_code
dtp41_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val) {		/* Pointer to instrument patch value */
	dtp41 *p = (dtp41 *)pp;
	char *tp;
	static char buf[MAX_RD_SIZE];
	int i, se;
	inst_code ev = inst_ok;
	int switch_trig = 0;
	int user_trig = 0;

	/* Configure for static mode */
	p->lastmode = (p->lastmode & ~inst_mode_sub_mask) | inst_mode_spot;
	activate_mode(p);

	/* Set static measurement mode */
	if ((ev = dtp41_command(p, "0013CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	if (p->trig == inst_opt_trig_keyb_switch) {

		/* Wait for the Read status, or a user abort/command - allow 5 minuits */
		if ((ev = dtp41_command(p, "", buf, MAX_MES_SIZE, 5 * 60.0)) != inst_ok) {
			if ((ev & inst_mask) == inst_needs_cal)
				p->need_cal = 1;
			if ((ev & inst_mask) != inst_user_trig)
				return ev;
			user_trig = 1;
		} else {
			switch_trig = 1;
		}
		if (p->trig_return)
			printf("\n");

	} else if (p->trig == inst_opt_trig_keyb) {
		if ((se = icoms_poll_user(p->icom, 1)) != ICOM_TRIG) {
			/* Abort, term or command */
			return dtp41_interp_code((inst *)p, icoms2dtp41_err(se));
		}
		user_trig = 1;
		if (p->trig_return)
			printf("\n");
	}

	/* Trigger a read if the switch has not been used */
	if (switch_trig == 0) {
		/* Do a read */
		if ((ev = dtp41_command(p, "RM\r", buf, MAX_MES_SIZE, 20.0)) != inst_ok) {
			if ((ev & inst_mask) == inst_needs_cal)
				p->need_cal = 1;
			return ev;
		}
	}

	/* Gather the results in D50_2 XYZ */
	if ((ev = dtp41_command(p, "0405TS\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
		return ev; 	/* Strip misread */

	/* Parse the buffer */
	/* Replace '\r' with '\000' */
	for (tp = buf; *tp != '\000'; tp++) {
		if (*tp == '\r')
			*tp = '\000';
	}
	
	val->XYZ[0] = val->XYZ[1] = val->XYZ[2] = 0.0;

	/* for all the readings taken */
	for (tp = buf, i = 0; i < p->nstaticr; i++) {
		double XYZ[3];

		if (*tp == '\000')
			return inst_protocol_error;

		if (sscanf(tp, " %lf %lf %lf ", &XYZ[0], &XYZ[1], &XYZ[2]) != 3) {
			return inst_protocol_error;
		}
		val->XYZ[0] += XYZ[0];
		val->XYZ[1] += XYZ[1];
		val->XYZ[2] += XYZ[2];
		tp += strlen(tp) + 1;
	}

	/* Average */
	val->XYZ[0] /= (double)p->nstaticr;
	val->XYZ[1] /= (double)p->nstaticr;
	val->XYZ[2] /= (double)p->nstaticr;
	val->XYZ_v = 1;
	val->aXYZ_v = 0;
	val->Lab_v = 0;
	val->sp.spec_n = 0;
	val->duration = 0.0;

	if (p->mode & inst_mode_spectral) {
		int j;

		/* Gather the results in Spectral reflectance */
		if ((ev = dtp41_command(p, "0403TS\r", buf, MAX_RD_SIZE, 0.5)) != DTP41_OK)
			return ev; 	/* Strip misread */
	
		/* Parse the buffer */
		/* Replace '\r' with '\000' */
		for (tp = buf; *tp != '\000'; tp++) {
			if (*tp == '\r')
				*tp = '\000';
		}

		for (j = 0; j < 31; j++)
			val->sp.spec[j] = 0.0;

		/* Get each readings spetra */
		for (tp = buf, i = 0; i < p->nstaticr; i++) {
			char *tpp;
			if (strlen(tp) < (31 * 8 - 1)) {
				return inst_protocol_error;
			}

			/* Read the spectral value */
			for (tpp = tp, j = 0; j < 31; j++, tpp += 8) {
				char c;
				c = tpp[7];
				tpp[7] = '\000';
				val->sp.spec[j] += atof(tpp);
				tpp[7] = c;
			}

			tp += strlen(tp) + 1;
		}

		/* Average the result */
		for (j = 0; j < 31; j++)
			val->sp.spec[j] /= (double)p->nstaticr;

		val->sp.spec_n = 31;
		val->sp.spec_wl_short = 400.0;
		val->sp.spec_wl_long = 700.0;
		val->sp.norm = 100.0;
	}

	/* Set back to dynamic measurement mode */
	if ((ev = dtp41_command(p, "0113CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	if (user_trig)
		return inst_user_trig;
	return inst_ok;
}


/* Determine if a calibration is needed. Returns inst_calt_none if not, */
/* inst_calt_unknown if it is unknown, or inst_calt_XXX if needs calibration, */
/* and the first type of calibration needed. */
/* Can't know ahead of time with the DTP41, but we track if an error */
/* was returned from a read. */
inst_cal_type dtp41_needs_calibration(inst *pp) {
	dtp41 *p = (dtp41 *)pp;
	
	if (p->need_cal) {
		if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission)
			return inst_calt_trans_white;	/* ??? */
		else
			return inst_calt_ref_white;
	}
	return inst_calt_unknown;
}

/* Request an instrument calibration. */
/* This is use if the user decides they want to do a calibration, */
/* in anticipation of a calibration (needs_calibration()) to avoid */
/* requiring one during measurement, or in response to measuring */
/* returning inst_needs_cal. Initially us an inst_cal_cond of inst_calc_none, */
/* and then be prepared to setup the right conditions, or ask the */
/* user to do so, each time the error inst_cal_setup is returned. */
inst_code dtp41_calibrate(
inst *pp,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	dtp41 *p = (dtp41 *)pp;
	id[0] = '\000';

	if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission) {

		if (calt == inst_calt_all)
			calt = inst_calt_trans_white;
	
		if (calt != inst_calt_trans_white)
			return inst_unsupported;

		if (*calc == inst_calc_uop_trans_white) {
			p->need_cal = 0;
			return inst_ok;					/* Calibration done */
		}
	
		*calc = inst_calc_uop_trans_white;	/* Need to ask user to do calibration */

	} else {

		if (calt == inst_calt_all)
			calt = inst_calt_ref_white;
	
		if (calt != inst_calt_ref_white)
			return inst_unsupported;

		if (*calc == inst_calc_uop_ref_white) {
			p->need_cal = 0;
			return inst_ok;					/* Calibration done */
		}
	
		*calc = inst_calc_uop_ref_white;	/* Need to ask user to do calibration */
	}

	return inst_cal_setup;
}

/* Error codes interpretation */
static char *
dtp41_interp_error(inst *pp, int ec) {
//	dtp41 *p = (dtp41 *)pp;
	ec &= inst_imask;
	switch (ec) {

		case DTP41_USER_ABORT:
			return "User hit Abort key";
		case DTP41_USER_TERM:
			return "User hit Terminate key";
		case DTP41_USER_TRIG:
			return "User hit Trigger key";
		case DTP41_USER_CMND:
			return "User hit a Command key";
		case DTP41_INTERNAL_ERROR:
			return "Internal software error";
		case DTP41_COMS_FAIL:
			return "Communications failure";
		case DTP41_UNKNOWN_MODEL:
			return "Not a DTP41";
		case DTP41_DATA_PARSE_ERROR:
			return "Data from DTP41 didn't parse as expected";
		case DTP41_OK:
			return "No device error";
		case DTP41_MEASUREMENT_STATUS:
			return "Measurement complete";
		case DTP41_CALIBRATION_STATUS:
			return "Calibration complete";
		case DTP41_KEYPRESS_STATUS:
			return "A key was pressed";
		case DTP41_DEFAULTS_LOADED_STATUS:
			return "Default configuration values have been loaded";
		case DTP41_BAD_COMMAND:
			return "Unrecognised command";
		case DTP41_BAD_PARAMETERS:
			return "Wrong number of parameters";
		case DTP41_PRM_RANGE_ERROR:
			return "One or more parameters are out of range";
		case DTP41_BUSY:
			return "Instrument is busy - command ignored";
		case DTP41_USER_ABORT_ERROR:
			return "User aborted process";
		case DTP41_MEASUREMENT_ERROR:
			return "General measurement error";
		case DTP41_TIMEOUT:
			return "Receive timeout";
		case DTP41_BAD_STRIP:
			return "Bad strip";
		case DTP41_BAD_COLOR:
			return "Bad color";
		case DTP41_BAD_STEP:
			return "Bad step";
		case DTP41_BAD_PASS:
			return "Bad pass";
		case DTP41_BAD_PATCHES:
			return "Bad patches";
		case DTP41_BAD_READING:
			return "Bad reading";
		case DTP41_NEEDS_CAL_ERROR:
			return "Instrument needs calibration";
		case DTP41_CAL_FAILURE_ERROR:
			return "Calibration failed";
		case DTP41_INSTRUMENT_ERROR:
			return "General instrument error";
		case DTP41_LAMP_ERROR:
			return "Reflectance lamp error";
		case DTP41_FILTER_ERROR:
			return "Filter error";
		case DTP41_FILTER_MOTOR_ERROR:
			return "Filter motor error";
		case DTP41_DRIVE_MOTOR_ERROR:
			return "Strip drive motor error";
		case DTP41_KEYPAD_ERROR:
			return "Keypad error";
		case DTP41_DISPLAY_ERROR:
			return "Display error";
		case DTP41_MEMORY_ERROR:
			return "Memory error";
		case DTP41_ADC_ERROR:
			return "ADC error";
		case DTP41_PROCESSOR_ERROR:
			return "Processor error";
		case DTP41_BATTERY_ERROR:
			return "Battery error";
		case DTP41_BATTERY_LOW_ERROR:
			return "Battery low error";
		case DTP41_INPUT_POWER_ERROR:
			return "Input power error";
		case DTP41_TEMPERATURE_ERROR:
			return "Temperature error";
		case DTP41_BATTERY_ABSENT_ERROR:
			return "Battery absent error";
		case DTP41_TRAN_LAMP_ERROR:
			return "Transmission lamp error";
		case DTP41_INVALID_COMMAND_ERROR:
			return "Invalid command";
		default:
			return "Unknown error code";
	}
}

/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
dtp41_interp_code(inst *pp, int ec) {
//	dtp41 *p = (dtp41 *)pp;

	ec &= inst_imask;
	switch (ec) {

		case DTP41_OK:
			return inst_ok;

		case DTP41_INTERNAL_ERROR:
			return inst_internal_error | ec;

		case DTP41_COMS_FAIL:
			return inst_coms_fail | ec;

		case DTP41_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case DTP41_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

		case DTP41_USER_ABORT:
			return inst_user_abort | ec;
		case DTP41_USER_TERM:
			return inst_user_term | ec;
		case DTP41_USER_TRIG:
			return inst_user_trig | ec;
		case DTP41_USER_CMND:
			return inst_user_cmnd | ec;

		case DTP41_BUSY:
		case DTP41_TIMEOUT:
		case DTP41_BAD_READING:
		case DTP41_INSTRUMENT_ERROR:
		case DTP41_DRIVE_MOTOR_ERROR:
		case DTP41_ADC_ERROR:
		case DTP41_TRAN_LAMP_ERROR:
			return inst_misread | ec;

		case DTP41_NEEDS_CAL_ERROR:
			return inst_needs_cal | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
dtp41_del(inst *pp) {
	dtp41 *p = (dtp41 *)pp;
	if (p->icom != NULL)
		p->icom->del(p->icom);
	free (p);
}

/* Interogate the device to discover its capabilities */
static void	discover_capabilities(dtp41 *p) {
	static char buf[MAX_MES_SIZE];
	inst_code rv = inst_ok;

	p->cap = inst_ref_spot
	       | inst_ref_strip
	       | inst_colorimeter
	       | inst_spectral
	       ;

	p->cap2 = inst2_cal_ref_white		/* User operated though */
            | inst2_prog_trig
            | inst2_keyb_trig
	        | inst2_keyb_switch_trig
	        ;

	/* Check whether we have transmission capability */
	if ((rv = dtp41_command(p, "0119CF\r", buf, MAX_MES_SIZE, 1.5)) == inst_ok) {
		p->cap |= inst_trans_spot
		       |  inst_trans_strip
		       ;

		p->cap2 |= inst2_cal_trans_white;		/* User operated though */
	}
	rv = dtp41_command(p, "0019CF\r", buf, MAX_MES_SIZE, 1.5);
}

/* Return the instrument capabilities */
inst_capability dtp41_capabilities(inst *pp) {
	dtp41 *p = (dtp41 *)pp;

	if (p->cap == inst_unknown)
		discover_capabilities(p);
	return p->cap;
}

/* Return the instrument capabilities 2 */
inst2_capability dtp41_capabilities2(inst *pp) {
	dtp41 *p = (dtp41 *)pp;
	inst2_capability rv;

	if (p->cap2 == inst2_unknown)
		discover_capabilities(p);

	rv = p->cap2;

	return rv;
}

/* Activate the last set mode */
static inst_code
activate_mode(dtp41 *p)
{
	static char buf[MAX_MES_SIZE];
	inst_code rv;

	/* Setup for transmission or reflection */
	if ((p->lastmode & inst_mode_illum_mask) == inst_mode_reflection
	 && (p->mode     & inst_mode_illum_mask) != inst_mode_reflection) {
		if ((rv = dtp41_command(p, "0019CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
			return rv;
	}
	if ((p->lastmode & inst_mode_illum_mask) == inst_mode_transmission
	 && (p->mode     & inst_mode_illum_mask) != inst_mode_transmission) {
		if ((rv = dtp41_command(p, "0119CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
			return rv;
	}

	/* Setup for static or dynamic reading */
	if ((p->lastmode & inst_mode_sub_mask) == inst_mode_spot
	 && (p->mode     & inst_mode_sub_mask) != inst_mode_spot) {
		if ((rv = dtp41_command(p, "0013CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
			return rv;
	}
	if ((p->lastmode & inst_mode_sub_mask) == inst_mode_strip
	 && (p->mode     & inst_mode_sub_mask) != inst_mode_strip) {
		if ((rv = dtp41_command(p, "0113CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
			return rv;
	}

	p->mode = p->lastmode;

	return inst_ok;
}

/* 
 * set measurement mode
 * We assume that the instrument has been initialised.
 */
static inst_code
dtp41_set_mode(inst *pp, inst_mode m) {
	dtp41 *p = (dtp41 *)pp;
	inst_capability cap = pp->capabilities(pp);
	inst_mode mm;		/* Measurement mode */

	/* The measurement mode portion of the mode */
	mm = m & inst_mode_measurement_mask;

	/* General check mode against specific capabilities logic: */
	if (mm == inst_mode_ref_spot) {
 		if (!(cap & inst_ref_spot))
			return inst_unsupported;
	} else if (mm == inst_mode_ref_strip) {
		if (!(cap & inst_ref_strip))
			return inst_unsupported;
	} else if (mm == inst_mode_ref_xy) {
		if (!(cap & inst_ref_xy))
			return inst_unsupported;
	} else if (mm == inst_mode_trans_spot) {
		if (!(cap & inst_trans_spot))
			return inst_unsupported;
	} else if (mm == inst_mode_trans_strip) {
		if (!(cap & inst_trans_strip))
			return inst_unsupported;
	} else if (mm == inst_mode_trans_xy) {
		if (!(cap & inst_trans_xy))
			return inst_unsupported;
	} else if (mm == inst_mode_emis_disp) {
		if (!(cap & inst_emis_disp))
			return inst_unsupported;
	} else {
		return inst_unsupported;
	}

	if (m & inst_mode_colorimeter)
		if (!(cap & inst_colorimeter))
			return inst_unsupported;
		
	if (m & inst_mode_spectral)
		if (!(cap & inst_spectral))
			return inst_unsupported;

	p->lastmode = m;

	if (p->lastmode != p->mode) {
		return activate_mode(p);
	}
	return inst_ok;
}

/* !! It's not clear if there is a way of knowing */
/* whether the instrument has a UV filter. */

/* 
 * set or reset an optional mode
 * We assume that the instrument has been initialised.
 */
static inst_code
dtp41_set_opt_mode(inst *pp, inst_opt_mode m, ...)
{
	dtp41 *p = (dtp41 *)pp;
	inst_code rv = inst_ok;
	static char buf[MAX_MES_SIZE];

	/* Record the trigger mode */
	if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_keyb
	 || m == inst_opt_trig_keyb_switch) {
		p->trig = m;

		if (m == inst_opt_trig_keyb_switch) {
			/* Enable the read microswitch */
			if ((rv = dtp41_command(p, "01PB\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
				return rv;
		} else {
			/* Disable the read microswitch */
			if ((rv = dtp41_command(p, "00PB\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
				return rv;
		}
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
extern dtp41 *new_dtp41(icoms *icom, int debug, int verb)
{
	dtp41 *p;
	if ((p = (dtp41 *)calloc(sizeof(dtp41),1)) == NULL)
		error("dtp41: malloc failed!");

	if (icom == NULL)
		p->icom = new_icoms();
	else
		p->icom = icom;

	p->debug = debug;
	p->verb = verb;

	p->init_coms     = dtp41_init_coms;
	p->init_inst     = dtp41_init_inst;
	p->capabilities  = dtp41_capabilities;
	p->capabilities2 = dtp41_capabilities2;
	p->set_mode      = dtp41_set_mode;
	p->set_opt_mode     = dtp41_set_opt_mode;
	p->read_strip   = dtp41_read_strip;
	p->read_sample  = dtp41_read_sample;
	p->needs_calibration = dtp41_needs_calibration;
	p->calibrate    = dtp41_calibrate;
	p->interp_error = dtp41_interp_error;
	p->del          = dtp41_del;

	p->itype = instDTP41;
	p->cap = inst_unknown;						/* Unknown until initialised */
	p->mode = inst_mode_unknown;				/* Not in a known mode yet */
	p->nstaticr = 5;							/* Number of static readings */

	return p;
}
