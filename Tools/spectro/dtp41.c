
/* 
 * Argyll Color Correction System
 *
 * Xrite DTP41 related functions
 *
 * Author: Graeme W. Gill
 * Date:   10/3/2001
 *
 * Copyright 1996 - 2013, Graeme W. Gill
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
#include "dtp41.h"
#include "xrga.h"

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
	if (se != ICOM_OK) {
		if (se & ICOM_TO)
			return DTP41_TIMEOUT; 
		return DTP41_COMS_FAIL;
	}
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
char *tc,			/* Terminating characters */
int ntc,			/* Number of terminating characters */
double to) {		/* Timout in seconts */
	int rv, se;

	if ((se = p->icom->write_read(p->icom, in, 0, out, bsize, NULL, tc, ntc, to)) != 0) {
		a1logd(p->log, 1, "dtp41_fcommand: serial i/o failure 0x%x on write_read '%s'\n",se,icoms_fix(in));
		return icoms2dtp41_err(se);
	}
	rv = DTP41_OK;
	if (tc[0] == '>' && ntc == 1) {
		rv = extract_ec(out);
		if (rv > 0) {
			rv &= inst_imask;
			if (rv != DTP41_OK) {	/* Clear the error */
				char buf[MAX_MES_SIZE];
				p->icom->write_read(p->icom, "CE\r", 0, buf, MAX_MES_SIZE, NULL, ">", 1, 0.5);
			}
		}
	}
	a1logd(p->log, 4, "dtp41_fcommand: command '%s' returned '%s', value 0x%x\n",
	                                            icoms_fix(in), icoms_fix(out),rv);
	return rv;
}

/* Do a standard command/response echange with the dtp41 */
/* Return the instrument error code */
static inst_code
dtp41_command(dtp41 *p, char *in, char *out, int bsize, double to) {
	int rv = dtp41_fcommand(p, in, out, bsize, ">", 1, to);
	return dtp41_interp_code((inst *)p, rv);
}

/* Establish communications with a DTP41 */
/* Use the baud rate given, and timeout in to secs */
/* Return DTP41_COMS_FAIL on failure to establish communications */
static inst_code
dtp41_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	dtp41 *p = (dtp41 *)pp;
	static char buf[MAX_MES_SIZE];
	baud_rate brt[9] = { baud_9600, baud_19200, baud_38400, baud_57600,
	                     baud_4800, baud_2400, baud_1200, baud_600, baud_300 };
	char *brc[9] =     { "9600BR\r", "19200BR\r", "38400BR\r", "57600BR\r",
	                     "4800BR\r", "2400BR\r", "1200BR\r", "600BR\r", "300BR\r" };
	char *fcc;
	unsigned int etime;
	int ci, bi, i, se;
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "dtp41_init_coms: About to init Serial I/O\n");

	/* Deal with flow control setting */
	if (fc == fc_nc)
		fc = DEFFC;
	if (fc == fc_XonXOff) {
		fcc = "0304CF\r";
	} else if (fc == fc_Hardware) {
		fcc = "0104CF\r";
	} else {
		fc = fc_None;
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

	/* Until we time out, find the correct baud rate */
	for (i = ci; msec_time() < etime;) {
		a1logd(p->log, 4, "dtp41_init_coms: Trying %s baud, %d msec to go\n",
			                      baud_rate_to_str(brt[i]), etime- msec_time());
		if ((se = p->icom->set_ser_port(p->icom, fc_None, brt[i], parity_none,
			                                    stop_1, length_8)) != ICOM_OK) { 
			a1logd(p->log, 1, "dtp41_init_coms: set_ser_port failed ICOM err 0x%x\n",se);
			return dtp41_interp_code((inst *)p, icoms2dtp41_err(se));
		}
		if (((ev = dtp41_command(p, "\r", buf, MAX_MES_SIZE, 0.5)) & inst_mask)
			                                                 != inst_coms_fail)
			goto got_coms;		/* We've got coms or user abort */


		/* Check for user abort */
		if (p->uicallback != NULL) {
			inst_code ev;
			if ((ev = p->uicallback(p->uic_cntx, inst_negcoms)) == inst_user_abort) {
				a1logd(p->log, 1, "dtp41_init_coms: user aborted\n");
				return inst_user_abort;
			}
		}
		if (++i >= 9)
			i = 0;
	}
	/* We haven't established comms */
	return inst_coms_fail;

  got_coms:;

	/* set the protocol to RCI */
	if ((ev = dtp41_command(p, "0012CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	/* Set the handshaking (cope with coms breakdown) */
	if ((se = p->icom->write_read(p->icom, fcc, 0, buf, MAX_MES_SIZE, NULL, ">", 1, 1.5)) != 0) {
		if (extract_ec(buf) != DTP41_OK)
			return inst_coms_fail;
	}

	/* Change the baud rate to the rate we've been told (cope with coms breakdown) */
	if ((se = p->icom->write_read(p->icom, brc[bi], 0, buf, MAX_MES_SIZE, NULL, ">", 1, 1.5)) != 0) {
		if (extract_ec(buf) != DTP41_OK)
			return inst_coms_fail;
	}

	/* Configure our baud rate and handshaking as well */
	if ((se = p->icom->set_ser_port(p->icom, fc, brt[bi], parity_none, stop_1, length_8))
		                                                                      != ICOM_OK) {
		a1logd(p->log, 1, "dtp41_init_coms: set_ser_port failed ICOM err 0x%x\n",se);
		return dtp41_interp_code((inst *)p, icoms2dtp41_err(se));
	}

	/* Loose a character (not sure why) */
	p->icom->write_read(p->icom, "\r", 0, buf, MAX_MES_SIZE, NULL, ">", 1, 0.5);

	/* Check instrument is responding */
	if ((ev = dtp41_command(p, "\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok) {
		a1logd(p->log, 1, "dtp41_init_coms: instrument failed to respond\n");
		return inst_coms_fail;
	}

	a1logd(p->log, 2, "dtp41_init_coms: init coms has suceeded\n");

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

		if (twid >= 9999.5) {
			a1logw(p->log, "DTP41 build_strip given trailer length %f > 9999 mm\n",twid);
			twid = 9999.0;
		}
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
	char *envv;
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "dtp41_init_inst: called\n");

	if (p->gotcoms == 0)
		return inst_internal_error;		/* Must establish coms before calling init */

	p->native_calstd = xcalstd_xrdi;
	p->target_calstd = xcalstd_native;		/* Default to native calibration standard*/

	/* Honour Environment override */
	if ((envv = getenv("ARGYLL_XCALSTD")) != NULL) {
		if (strcmp(envv, "XRGA") == 0)
			p->target_calstd = xcalstd_xrga;
		else if (strcmp(envv, "XRDI") == 0)
			p->target_calstd = xcalstd_xrdi;
		else if (strcmp(envv, "GMDI") == 0)
			p->target_calstd = xcalstd_gmdi;
	}

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
	p->trig = inst_opt_trig_user_switch;

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

#ifdef NEVER
	/* See what the transmission mode is up to */
	dtp41_command(p, "DEVELOPERPW\r", buf, MAX_MES_SIZE, 1.5);
	dtp41_command(p, "0119CF\r", buf, MAX_MES_SIZE, 1.5);
	dtp41_command(p, "36OD\r", buf, MAX_MES_SIZE, 1.5);
	dtp41_command(p, "1422OD\r", buf, MAX_MES_SIZE, 1.5);
#endif

	/* We are configured in this mode now */
	p->mode = inst_mode_ref_strip;

	if (p->lastmode != p->mode) {
		if ((ev = activate_mode(p)) != inst_ok)
			return ev;
	}

	p->inited = 1;

	a1logd(p->log, 2, "dtp41_init_inst: instrument inited OK\n");

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

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Configure for dynamic mode */
	p->lastmode = (p->lastmode & ~inst_mode_sub_mask) | inst_mode_strip;
	activate_mode(p);

	build_strip(p, tbuf, name, npatch, pname, sguide, pwid, gwid, twid);
	
	/* Send strip definition */
	if ((ev = dtp41_command(p, tbuf, buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	if (p->trig == inst_opt_trig_user_switch) {

		/* Wait for the Read status, or a user trigger/abort */
		for (;;) {
			if ((ev = dtp41_command(p, "", buf, MAX_MES_SIZE, 0.5)) != inst_ok) {
				if ((ev & inst_mask) == inst_needs_cal)
					p->need_cal = 1;

				if ((ev & inst_imask) != DTP41_TIMEOUT) 
					return ev;			/* Instrument or comms error */

				/* Timed out */
				if (p->uicallback != NULL) {	/* Check for user trigger */
					if ((ev = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
						if (ev == inst_user_abort)
							return ev;		/* User abort */
						if (ev == inst_user_trig) {
							user_trig = 1;
							break;
						}
					}
				}

			} else {	/* Got read status - assume triggered */
				switch_trig = 1;
				break;					/* Switch activated */
			}
		}
		/* Notify of trigger */
		if (p->uicallback)
			p->uicallback(p->uic_cntx, inst_triggered); 

	} else if (p->trig == inst_opt_trig_user) {

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "dtp41: inst_opt_trig_user but no uicallback function set!\n");
			return inst_unsupported;
		}

		for (;;) {
			if ((ev = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
				if (ev == inst_user_abort)
					return ev;				/* Abort */
				if (ev == inst_user_trig) {
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
		 && (ev = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort)
			return ev;				/* Abort */
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
		vals[i].loc[0] = '\000';
		if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission)
			vals[i].mtype = inst_mrt_transmissive;
		else
			vals[i].mtype = inst_mrt_reflective;
		vals[i].XYZ_v = 1;
		vals[i].sp.spec_n = 0;
		vals[i].duration = 0.0;
		tp += strlen(tp) + 1;
	}

	if (p->mode & inst_mode_spectral
	 || (XCALSTD_NEEDED(p->target_calstd, p->native_calstd)
		&& ((p->mode & inst_mode_illum_mask) != inst_mode_transmission))
	 || p->custfilt_en
	) {

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

	/* Apply any XRGA conversion */
	ipatch_convert_xrga(vals, npatch, xcalstd_nonpol, p->target_calstd, p->native_calstd,
	                    instClamp);

	/* Apply custom filter compensation */
	if (p->custfilt_en)
		ipatch_convert_custom_filter(vals, npatch, &p->custfilt, instClamp);

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
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	dtp41 *p = (dtp41 *)pp;
	char *tp;
	static char buf[MAX_RD_SIZE];
	int i, se;
	inst_code ev = inst_ok;
	int switch_trig = 0;
	int user_trig = 0;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Configure for static mode */
	p->lastmode = (p->lastmode & ~inst_mode_sub_mask) | inst_mode_spot;
	activate_mode(p);

	/* Set static measurement mode */
	if ((ev = dtp41_command(p, "0013CF\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok)
		return ev;

	if (p->trig == inst_opt_trig_user_switch) {

		/* Wait for the Read status, or a user trigger/abort */
		for (;;) {
			if ((ev = dtp41_command(p, "", buf, MAX_MES_SIZE, 0.5)) != inst_ok) {
				if ((ev & inst_mask) == inst_needs_cal)
					p->need_cal = 1;

				if ((ev & inst_imask) != DTP41_TIMEOUT)
					return ev;			/* Instrument or comms error */

				/* Timed out */
				if (p->uicallback != NULL) {	/* Check for user trigger */
					if ((ev = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
						if (ev == inst_user_abort)
							return ev;		/* User abort */
						if (ev == inst_user_trig) {
							user_trig = 1;
							break;				/* User trigger */
						}
					}
				}

			} else {			/* Assume read status and trigger */
				switch_trig = 1;
				break;					/* Switch activated */
			}
		}
		/* Notify of trigger */
		if (p->uicallback)
			p->uicallback(p->uic_cntx, inst_triggered); 

	} else if (p->trig == inst_opt_trig_user) {

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "dtp41: inst_opt_trig_user but no uicallback function set!\n");
			return inst_unsupported;
		}

		for (;;) {
			if ((ev = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
				if (ev == inst_user_abort)
					return ev;				/* Abort */
				if (ev == inst_user_trig) {
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
		 && (ev = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort)
			return ev;				/* Abort */
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
	/* This may not change anything since instrument may clamp */
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);
	val->loc[0] = '\000';
	if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission)
		val->mtype = inst_mrt_transmissive;
	else
		val->mtype = inst_mrt_reflective;
	val->XYZ_v = 1;
	val->sp.spec_n = 0;
	val->duration = 0.0;

	if (p->mode & inst_mode_spectral
	 || (XCALSTD_NEEDED(p->target_calstd, p->native_calstd)
		&& ((p->mode & inst_mode_illum_mask) != inst_mode_transmission))) {
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

	/* Apply any XRGA conversion */
	ipatch_convert_xrga(val, 1, xcalstd_nonpol, p->target_calstd, p->native_calstd, clamp);

	if (user_trig)
		return inst_user_trig;
	return inst_ok;
}

/* Return needed and available inst_cal_type's */
static inst_code dtp41_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	dtp41 *p = (dtp41 *)pp;
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
		
	if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission) {
		if (p->need_cal)
			n_cals |= inst_calt_trans_white;	/* ??? */
		a_cals |= inst_calt_trans_white;
	} else {
		if (p->need_cal)
			n_cals |= inst_calt_ref_white;
		a_cals |= inst_calt_ref_white;
	}

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	return inst_ok;
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
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
inst_calc_id_type *idtype,	/* Condition identifier type */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	dtp41 *p = (dtp41 *)pp;
	inst_code ev;
    inst_cal_type needed, available;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	*idtype = inst_calc_id_none;
	id[0] = '\000';

	if ((ev = dtp41_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
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

		a1logd(p->log,4,"dtp41_calibrate: doing calt 0x%x\n",calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return inst_ok;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return inst_unsupported;
	}

	if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission) {
		if (*calt & inst_calt_trans_white) {

			if ((*calc & inst_calc_cond_mask) != inst_calc_uop_trans_white)
				/* Ask user to do calibration */
				*calc = inst_calc_uop_trans_white;
				return inst_cal_setup;
			}
	
			p->need_cal = 0;
		 	*calt &= ~inst_calt_trans_white;
	
	} else {
		if (*calt & inst_calt_ref_white) {

			if ((*calc & inst_calc_cond_mask) != inst_calc_uop_ref_white) {
				/* Ask user to do calibration */
				*calc = inst_calc_uop_ref_white;
				return inst_cal_setup;
			}
		
			p->need_cal = 0;
			*calt &= ~inst_calt_ref_white;
		}
	}

	return inst_ok;					/* Calibration done */
}

/* Error codes interpretation */
static char *
dtp41_interp_error(inst *pp, int ec) {
//	dtp41 *p = (dtp41 *)pp;
	ec &= inst_imask;
	switch (ec) {

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
	p->vdel(pp);
	free(p);
}

/* Interogate the device to discover its capabilities */
/* If we haven't initilalised the instrument, we don't */
/* know if it supports transparency. */
static void	discover_capabilities(dtp41 *p) {
	static char buf[MAX_MES_SIZE];
	inst_code rv = inst_ok;

	p->cap = inst_mode_ref_spot
	       | inst_mode_ref_strip
	       | inst_mode_colorimeter
	       | inst_mode_spectral
	       ;

	p->cap2 = inst2_prog_trig
            | inst2_user_trig
	        | inst2_user_switch_trig
	        ;

	p->cap3 = inst3_none;

	if (p->inited) {
		/* Check whether we have transmission capability */
		if ((rv = dtp41_command(p, "0119CF\r", buf, MAX_MES_SIZE, 1.5)) == inst_ok) {
			p->cap |= inst_mode_trans_spot
			       |  inst_mode_trans_strip
			       ;
		}
		/* Set back to reflectance mode */
		rv = dtp41_command(p, "0019CF\r", buf, MAX_MES_SIZE, 1.5);
	}
}

/* Return the instrument capabilities */
void dtp41_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	dtp41 *p = (dtp41 *)pp;

	if (p->cap == inst_mode_none)
		discover_capabilities(p);

	if (pcap1 != NULL)
		*pcap1 = p->cap;
	if (pcap2 != NULL)
		*pcap2 = p->cap2;
	if (pcap3 != NULL)
		*pcap3 = p->cap3;
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
 * check measurement mode
 * We assume that the instrument has been initialised.
 */
static inst_code
dtp41_check_mode(inst *pp, inst_mode m) {
	dtp41 *p = (dtp41 *)pp;
	inst_mode cap;

	pp->capabilities(pp, &cap, NULL, NULL);

	if (m & ~cap)		/* Simple elimination test */
		return inst_unsupported;

	/* Check specific modes */
	if (!IMODETST(m, inst_mode_ref_spot)
	 && !IMODETST(m, inst_mode_ref_strip)
	 && !IMODETST2(cap, m, inst_mode_trans_spot)
	 && !IMODETST2(cap, m, inst_mode_trans_strip)) {
		return inst_unsupported;
	}

	return inst_ok;
}

/* 
 * set measurement mode
 * We assume that the instrument has been initialised.
 */
static inst_code
dtp41_set_mode(inst *pp, inst_mode m) {
	dtp41 *p = (dtp41 *)pp;
	inst_code ev;

	if ((ev = dtp41_check_mode(pp, m)) != inst_ok)
		return ev;

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
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 */
static inst_code
dtp41_get_set_opt(inst *pp, inst_opt_type m, ...)
{
	dtp41 *p = (dtp41 *)pp;
	inst_code rv = inst_ok;
	static char buf[MAX_MES_SIZE];

	/* Set xcalstd */
	if (m == inst_opt_set_xcalstd) {
		xcalstd standard;
		va_list args;

		va_start(args, m);
		standard = va_arg(args, xcalstd);
		va_end(args);

		p->target_calstd = standard;

		return inst_ok;
	}

	/* Get the current effective xcalstd */
	if (m == inst_opt_get_xcalstd) {
		xcalstd *standard;
		va_list args;

		va_start(args, m);
		standard = va_arg(args, xcalstd *);
		va_end(args);

		if (p->target_calstd == xcalstd_native)
			*standard = p->native_calstd;		/* If not overridden */
		else
			*standard = p->target_calstd;		/* Overidden std. */

		return inst_ok;
	}

	if (m == inst_opt_set_custom_filter) {
		va_list args;
		xspect *sp = NULL;

		va_start(args, m);

		sp = va_arg(args, xspect *);

		va_end(args);

		if (sp == NULL || sp->spec_n == 0) {
			p->custfilt_en = 0;
			p->custfilt.spec_n = 0;
		} else {
			p->custfilt_en = 1;
			p->custfilt = *sp;			/* Struct copy */
		}
		return inst_ok;
	}

	if (m == inst_stat_get_custom_filter) {
		va_list args;
		xspect *sp = NULL;

		va_start(args, m);
		sp = va_arg(args, xspect *);
		va_end(args);

		if (p->custfilt_en) {
			*sp = p->custfilt;			/* Struct copy */
		} else {
			sp = NULL;
		}
		return inst_ok;
	}

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Set the trigger mode */
	if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_user
	 || m == inst_opt_trig_user_switch) {
		p->trig = m;

		if (m == inst_opt_trig_user_switch) {
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
extern dtp41 *new_dtp41(icoms *icom, instType dtype) {
	dtp41 *p;
	if ((p = (dtp41 *)calloc(sizeof(dtp41),1)) == NULL) {
		a1loge(icom->log, 1, "new_dtp41: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log(icom->log, 0, 0, NULL, NULL, NULL, NULL);

	p->init_coms     = dtp41_init_coms;
	p->init_inst     = dtp41_init_inst;
	p->capabilities  = dtp41_capabilities;
	p->check_mode    = dtp41_check_mode;
	p->set_mode      = dtp41_set_mode;
	p->get_set_opt   = dtp41_get_set_opt;
	p->read_strip    = dtp41_read_strip;
	p->read_sample   = dtp41_read_sample;
	p->get_n_a_cals  = dtp41_get_n_a_cals;
	p->calibrate     = dtp41_calibrate;
	p->interp_error  = dtp41_interp_error;
	p->del           = dtp41_del;

	p->icom = icom;
	p->dtype = dtype;
	p->cap = inst_mode_none;			/* Unknown until set */
	p->mode = inst_mode_none;			/* Not in a known mode yet */
	p->nstaticr = 5;					/* Number of static readings */

	return p;
}
