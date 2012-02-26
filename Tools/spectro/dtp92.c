/* 
 * Argyll Color Correction System
 *
 * Xrite DTP92/94 related functions
 *
 * Author: Graeme W. Gill
 * Date:   5/10/1996
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
#include "numlib.h"
#include "xspect.h"
#include "insttypes.h"
#include "icoms.h"
#include "conv.h"
#include "dtp92.h"

#undef DEBUG

/* Default flow control */
#define DEFFC fc_none

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
	if (se & ICOM_USERM) {
		se &= ICOM_USERM;
		if (se == ICOM_USER)
			return DTP92_USER_ABORT;
		if (se == ICOM_TERM)
			return DTP92_USER_TERM;
		if (se == ICOM_TRIG)
			return DTP92_USER_TRIG;
		if (se == ICOM_CMND)
			return DTP92_USER_CMND;
	}
	if (se != ICOM_OK)
		return DTP92_COMS_FAIL;
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
#ifdef DEBUG
		printf("dtp92 fcommand: serial i/o failure on write_read '%s'\n",icoms_fix(in));
#endif
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
#ifdef DEBUG
	printf("command '%s'",icoms_fix(in));
	printf(" returned '%s', value 0x%x\n",icoms_fix(out),rv);
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
dtp92_init_coms(inst *pp, int port, baud_rate br, flow_control fc, double tout) {
	dtp92 *p = (dtp92 *) pp;
	static char buf[MAX_MES_SIZE];
	baud_rate brt[5] = { baud_9600, baud_19200, baud_4800, baud_2400, baud_1200 };
	char *brc[5]     = { "30BR\r",  "60BR\r",   "18BR\r",  "0CBR\r",  "06BR\r" };
	char *fcc;
	long etime;
	instType itype;
	int ci, bi, i, rv;
	inst_code ev = inst_ok;

	if (p->debug) {
		p->icom->debug = p->debug;	/* Turn on debugging */
		fprintf(stderr,"dtp92: About to init coms\n");
	}

	if ((itype = p->icom->is_usb_portno(p->icom, port)) != instUnknown) {

		if (p->debug) fprintf(stderr,"dtp92Q/dtp94: About to init USB\n");

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
			p->icom->set_usb_port(p->icom, port, 1, 0x02, 0x81, icomuf_none, 0, NULL); 
		else
			p->icom->set_usb_port(p->icom, port, 1, 0x01, 0x81, icomuf_none, 0, NULL); 

		/* Blind reset it twice - it seems to sometimes hang up */
		/* otherwise under OSX */
		dtp92_command(p, "0PR\r", buf, MAX_MES_SIZE, 0.5);
		dtp92_command(p, "0PR\r", buf, MAX_MES_SIZE, 0.5);

	} else {

		if (p->debug) fprintf(stderr,"dtp92: About to init Serial I/O\n");

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

			if (p->debug) fprintf(stderr,"dtp92: Trying different baud rates (%ld ticks to go)\n",etime - msec_time());

			/* Until we time out, find the correct baud rate */
			for (i = ci; msec_time() < etime;) {
				p->icom->set_ser_port(p->icom, port, fc_none, brt[i], parity_none,
				                                                 stop_1, length_8);
				if (((ev = dtp92_command(p, "\r", buf, MAX_MES_SIZE, 0.5)) & inst_mask)
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
		if ((ev = dtp92_command(p, fcc, buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;

		/* Change the baud rate to the rate we've been told */
		if ((rv = p->icom->write_read(p->icom, brc[bi], buf, MAX_MES_SIZE, '>', 1, .2)) != 0) {
			if (extract_ec(buf) != DTP92_OK)
				return inst_coms_fail;
		}

		/* Configure our baud rate and handshaking as well */
		p->icom->set_ser_port(p->icom, port, fc, brt[bi], parity_none, stop_1, length_8);

		/* Loose a character (not sure why) */
		p->icom->write_read(p->icom, "\r", buf, MAX_MES_SIZE, '>', 1, 0.1);
	}

	/* Check instrument is responding, and reset it again. */
	if ((ev = dtp92_command(p, "\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok
	 || (ev = dtp92_command(p, "0PR\r", buf, MAX_MES_SIZE, 2.0)) != inst_ok) {

		if (p->debug) fprintf(stderr,"dtp92: init coms has failed\n");

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
#ifdef DEBUG
		printf("^M failed to get a response, returning inst_coms_fail\n");
#endif
		return inst_coms_fail;
	}

	if (p->debug) fprintf(stderr,"dtp92: init coms has suceeded\n");
#ifdef DEBUG
	printf("Got communications\n");
#endif

	p->gotcoms = 1;
	return inst_ok;
}

/* Initialise the DTP92 */
/* return non-zero on an error, with dtp error code */
static inst_code
dtp92_init_inst(inst *pp) {
	dtp92 *p = (dtp92 *)pp;
	static char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;

	if (p->debug) fprintf(stderr,"dtp92: About to init instrument\n");

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
	p->trig = inst_opt_trig_keyb;

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
			printf("Mem location 0x%04x = 0x%02x '%s'\n",i,val,icoms_fix(tb));
		}
	}
#endif /* NEVER */

	if (p->verb) {
		int i, j;
		if ((ev = dtp92_command(p, "GI\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
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

	if (ev == inst_ok) {
		p->inited = 1;
		if (p->debug) fprintf(stderr,"dtp92: instrument inited OK\n");
	}

	return ev;
}

/* The DTP92 seems to have a bug whereby it adds a spurious */
/* digit after the 'Z' of the Z value. Try and discard this. */

/* Read a single sample */
/* Return the dtp error code */
static inst_code
dtp92_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val) {		/* Pointer to instrument patch value */
	dtp92 *p = (dtp92 *)pp;
	char buf[MAX_RD_SIZE];
	int tries;
	int user_trig = 0;
	int rv = inst_protocol_error;

	/* Could change SS to suite level expected. */
#ifdef NEVER
	if (p->itype == instDTP92) {
		/* Set sample size to 31 (default is 16) for low level readings */
		if ((rv = dtp92_command(p, "1fSS\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return rv;
	}
#endif

	if (p->trig == inst_opt_trig_keyb) {
		int se;
		if ((se = icoms_poll_user(p->icom, 1)) != ICOM_TRIG) {
			/* Abort, term or command */
			return dtp92_interp_code((inst *)p, icoms2dtp92_err(se));
		}
		user_trig = 1;
		if (p->trig_return)
			printf("\n");
	}

	/* Until we get a valid return */
	for (tries = 0; tries < 2; tries++) {

		/* Take a reading */
		/* (DTP94 has optional parameters, but the default is what we want, XYZ in cd/m^2) */
		if ((rv = dtp92_command(p, "RM\r", buf, MAX_RD_SIZE, 10.0)) != inst_ok) {
			if ((rv & inst_imask) == DTP92_NEEDS_OFFSET_CAL)
				p->need_offset_cal = 1;
			else if ((rv & inst_imask) == DTP92_NEEDS_RATIO_CAL)
				p->need_ratio_cal = 1;
			return rv;
		}

		if (sscanf(buf, " X%*c %lf\t Y%*c %lf\t Z%*c %lf ",
	           &val->aXYZ[0], &val->aXYZ[1], &val->aXYZ[2]) == 3) {

			/* Apply the colorimeter correction matrix */
			icmMulBy3x3(val->aXYZ, p->ccmat, val->aXYZ);

			val->XYZ_v = 0;
			val->aXYZ_v = 1;		/* These are absolute XYZ readings */
			val->Lab_v = 0;
			val->sp.spec_n = 0;
			val->duration = 0.0;
			rv = inst_ok;
			break;
		} else {
//printf("~1 failed to parse string '%s'\n",buf);
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

/* Insert a colorimetric correction matrix in the instrument XYZ readings */
/* This is only valid for colorimetric instruments. */
/* To remove the matrix, pass NULL for the filter filename */
inst_code dtp92_col_cor_mat(
inst *pp,
double mtx[3][3]
) {
	dtp92 *p = (dtp92 *)pp;

	if (mtx == NULL)
		icmSetUnity3x3(p->ccmat);
	else
		icmCpy3x3(p->ccmat, mtx);
		
	return inst_ok;
}

/* Determine if a calibration is needed. Returns inst_calt_none if not, */
/* inst_calt_unknown if it is unknown, or inst_calt_XXX if needs calibration, */
/* and the first type of calibration needed. */
inst_cal_type dtp92_needs_calibration(inst *pp) {
	dtp92 *p = (dtp92 *)pp;

	if (p->need_offset_cal)
		return inst_calt_disp_offset;

	else if (p->need_ratio_cal)
		return inst_calt_disp_ratio;

	return inst_calt_unknown;
}

/* Request an instrument calibration. */
/* This is use if the user decides they want to do a calibration */
/* in anticipation of a calibration (needs_calibration()) to avoid */
/* requiring one during measurement, or in response to measuring */
/* returning inst_needs_cal. Initially use an inst_cal_cond of inst_calc_none, */
/* and then be prepared to setup the right conditions, or ask the */
/* user to do so, each time the error inst_cal_setup is returned. */
inst_code dtp92_calibrate(
inst *pp,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	dtp92 *p = (dtp92 *)pp;
	char buf[MAX_RD_SIZE];
	inst_code rv = inst_ok;
	id[0] = '\000';

	/* Default to most likely calibration type */
	if (calt == inst_calt_all) {
		if (p->need_offset_cal)
			calt = inst_calt_disp_offset;
	
		else if (p->need_ratio_cal)
			calt = inst_calt_disp_ratio;

		else
			calt = inst_calt_disp_offset;
	}

	/* See if it's a calibration we understand */
	if (calt != inst_calt_disp_offset
	 && calt != inst_calt_disp_ratio)
		return inst_unsupported;
		
	/* Make sure the conditions are right for the calbration */
	if (calt == inst_calt_disp_offset) {
		if (*calc != inst_calc_man_em_dark) {
			*calc = inst_calc_man_em_dark;
			return inst_cal_setup;
		}
	} else if (calt == inst_calt_disp_ratio) {
		if (*calc != inst_calc_disp_grey
		 && *calc != inst_calc_disp_grey_darker
		 && *calc != inst_calc_disp_grey_ligher) {
			*calc = inst_calc_disp_grey;
			return inst_cal_setup;
		}
	}

	/* Now that setup is right, perform calibration */

	if (calt == inst_calt_disp_offset) {		/* Dark offset calibration */

		/* Do offset calibration */
		if ((rv = dtp92_command(p, "CO\r", buf, MAX_RD_SIZE, 12)) != inst_ok)
			return rv;

		return inst_ok;

	} else if (calt == inst_calt_disp_ratio) {	/* Cell ratio calibration */

		/* Do ratio calibration */
		if ((rv = dtp92_command(p, "CR\r", buf, MAX_RD_SIZE, 25.0)) != inst_ok) {

			if ((rv & inst_imask) == DTP92_TOO_MUCH_LIGHT) {
				*calc = inst_calc_disp_grey_darker;
				return inst_cal_setup;
			} else if ((rv & inst_imask) == DTP92_NOT_ENOUGH_LIGHT) {
				*calc = inst_calc_disp_grey_ligher;
				return inst_cal_setup;
			}
			return rv;		/* Error */
		}
	}

	return rv;
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
		case DTP92_USER_ABORT:
			return "User hit Abort key";
		case DTP92_USER_TERM:
			return "User hit Terminate key";
		case DTP92_USER_TRIG:
			return "User hit Trigger key";
		case DTP92_USER_CMND:
			return "User hit a Command key";

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

		case DTP92_USER_ABORT:
			return inst_user_abort | ec;
		case DTP92_USER_TERM:
			return inst_user_term | ec;
		case DTP92_USER_TRIG:
			return inst_user_trig | ec;
		case DTP92_USER_CMND:
			return inst_user_cmnd | ec;

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
	free(p);
}

/* Return the instrument capabilities */
inst_capability dtp92_capabilities(inst *pp) {
	dtp92 *p = (dtp92 *)pp;
	inst_capability rv;

	rv = inst_emis_spot
	   | inst_emis_disp
	   | inst_colorimeter
	   | inst_ccmx
	     ;

	if (p->itype == instDTP94) {
		rv |= inst_emis_disp_crt;
		rv |= inst_emis_disp_lcd;
	}

	return rv;
}

/* Return the instrument capabilities 2 */
inst2_capability dtp92_capabilities2(inst *pp) {
	dtp92 *p = (dtp92 *)pp;
	inst2_capability rv;

	rv = inst2_cal_disp_offset
	   | inst2_prog_trig
	   | inst2_keyb_trig
	   ;				/* The '92 does have a switch, but we're not currently supporting it */

	if (p->itype == instDTP92)
		rv |= inst2_cal_disp_ratio;

	return rv;
}

/* Set device measurement mode */
inst_code dtp92_set_mode(inst *pp, inst_mode m)
{
	inst_mode mm;		/* Measurement mode */

	/* The measurement mode portion of the mode */
	mm = m & inst_mode_measurement_mask;

	/* only display emission mode supported */
	if (mm != inst_mode_emis_disp
	 && mm != inst_mode_emis_spot) {
		return inst_unsupported;
	}

	/* Spectral mode is not supported */
	if (m & inst_mode_spectral)
		return inst_unsupported;

	return inst_ok;
}

/* 
 * set or reset an optional mode
 * We assume that the instrument has been initialised.
 */
static inst_code
dtp92_set_opt_mode(inst *pp, inst_opt_mode m, ...)
{
	dtp92 *p = (dtp92 *)pp;
	char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;

	if (m == inst_opt_disp_crt) {
		if (p->itype == instDTP94) {
			if ((ev = dtp92_command(p, "0116CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
				return ev;
		}
		return inst_ok;
	} else if (m == inst_opt_disp_lcd) {
		if (p->itype == instDTP92)
			return inst_unsupported;
		if ((ev = dtp92_command(p, "0216CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
			return ev;
		return inst_ok;

	}

	/* Record the trigger mode */
	if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_keyb) {
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
extern dtp92 *new_dtp92(icoms *icom, int debug, int verb)
{
	dtp92 *p;
	if ((p = (dtp92 *)calloc(sizeof(dtp92),1)) == NULL)
		error("dtp92: malloc failed!");

	if (icom == NULL)
		p->icom = new_icoms();
	else
		p->icom = icom;

	p->debug = debug;
	p->verb = verb;

	icmSetUnity3x3(p->ccmat);	/* Set the colorimeter correction matrix to do nothing */

	p->init_coms         = dtp92_init_coms;
	p->init_inst         = dtp92_init_inst;
	p->capabilities      = dtp92_capabilities;
	p->capabilities2     = dtp92_capabilities2;
	p->set_mode          = dtp92_set_mode;
	p->set_opt_mode      = dtp92_set_opt_mode;
	p->read_sample       = dtp92_read_sample;
	p->needs_calibration = dtp92_needs_calibration;
	p->calibrate         = dtp92_calibrate;
	p->col_cor_mat       = dtp92_col_cor_mat;
	p->interp_error      = dtp92_interp_error;
	p->del               = dtp92_del;

	p->itype = instUnknown;		/* Until initalisation */

	return p;
}

