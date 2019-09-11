
/*
 * Argyll Color Correction System
 *
 * Xrite DTP20 related functions
 *
 * Author: Graeme W. Gill
 * Date:   10/3/2001
 *
 * Copyright 1996 - 2014, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Derived from DTP41.c
 *
 * Thanks to Rolf Gierling for contributing to the devlopment of this driver.
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
	CS comand values:

	00 - Instrument Not Ready
	01 - Instrument Ready, Empty
	02 - Instrument Ready, TID has been scanned
	03 - Instrument Ready, Complete target has been measured
	04 - Instrument is Busy ?
	05 - 
	06 - Error ?
	07 - Hardware Error
	08 - Clear Database Mode
	09 - Calibration Mode
    10 -
    11 -
    12 -
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
#else /* SALONEINSTLIB */
#include "sa_config.h"
#endif /* SALONEINSTLIB */
#include "numsup.h"
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "dtp20.h"
#include "xrga.h"

static inst_code dtp20_interp_code(inst *pp, int ec);
static inst_code activate_mode(dtp20 *p);

#define MIN_MES_SIZE 10			/* Minimum normal message reply size */
#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 100000		/* Maximum reading messagle reply size */

#define STRIP_READ_TIMEOUT 10.0

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
	/* For some reason the top bit sometimes get set ? (ie. multiple errors ?)*/
	rv &= 0x7f;
	return rv;
}

/* Interpret an icoms error into a DTP20 error */
static int icoms2dtp20_err(int se) {
	if (se != ICOM_OK) {
		if (se & ICOM_TO)
			return DTP20_TIMEOUT; 
		return DTP20_COMS_FAIL;
	}
	return DTP20_OK;
}

/* Do a standard command/response echange with the dtp20 */
/* Return the instrument error code */
static inst_code
dtp20_command(
dtp20 *p,
char *in,		/* In string */
char *out,		/* Out string buffer */
int bsize,				/* Out buffer size */
double to) {			/* Timout in seconts */
	char *tc = ">";			/* Terminating character */
	int ntc = 1;			/* Number of terminating characters */
	int rv, se, insize;

	a1logd(p->log, 4, "dtp20: Sending '%s' bsize %d, to %f\n",icoms_fix(in),bsize,to);

	insize = strlen(in);
	if (insize > 0) {
		if ((se = p->icom->usb_control(p->icom, 0x41, 0x00, 0x00, 0x00, (unsigned char *)in, insize, to)) != ICOM_OK) {
			a1logd(p->log, 1, "dtp20: send command failed ICOM err 0x%x\n",se);
			return dtp20_interp_code((inst *)p, icoms2dtp20_err(se));
		}
	}

	if ((se = p->icom->read(p->icom, out, bsize, NULL, tc, ntc, to)) != 0) {
		a1logd(p->log, 1, "dtp20: read response failed ICOM err 0x%x\n",se);
		return dtp20_interp_code((inst *)p, icoms2dtp20_err(se));
	}

	rv = DTP20_OK;
	if (tc[0] == '>' && ntc == 1) {	/* Expecting DTP type error code */
		rv = extract_ec(out);
		if (rv > 0) {
			rv &= inst_imask;
			if (rv != DTP20_OK) {	/* Clear the error */
				char buf[MAX_MES_SIZE];
				p->icom->usb_control(p->icom, 0x41, 0x00, 0x00, 0x00, (unsigned char *)"CE\r", 3, 0.5);
				p->icom->read(p->icom, buf, MAX_MES_SIZE, NULL, tc, ntc, 0.5);
			}
		}
	}

	a1logd(p->log, 4, "dtp20: response '%s' ICOM err 0x%x\n",icoms_fix(out),rv);
	return dtp20_interp_code((inst *)p, rv);
}

/* Do a command/binary response echange with the dtp20, */
/* for reading binary spectral values. */
/* This is for reading binary spectral data */
/* Return the instrument error code */
static inst_code
dtp20_bin_command(
dtp20 *p,
char *in,			/* In string */
char *out,			/* Out string buffer */
int bsize,			/* Out buffer size & bytes to read */
double top) {		/* Timout in seconds */
	int rv, se, insize;
	int bread = 0;
	char *op;

	a1logd(p->log, 4, "dtp20: Sending '%s'",icoms_fix(in));

	insize = strlen(in);
	if (insize > 0) {
		if ((se = p->icom->usb_control(p->icom, 0x41, 0x00, 0x00, 0x00, (unsigned char *)in, insize, top)) != ICOM_OK) {
			a1logd(p->log, 1, "dtp20: send failed ICOM err 0x%x\n",se);
			return dtp20_interp_code((inst *)p, icoms2dtp20_err(se));
		}
	}

	/* Read in 62 byte chunks */
	for (op = out; bsize > 0;) {
		int rsize = 62;

		if (rsize > bsize)
			rsize = bsize;

		if ((se = p->icom->usb_read(p->icom, NULL, 0x81, (unsigned char *)op, rsize, &bread, top)) != ICOM_OK) {
			if (se == ICOM_SHORT) {
				a1logd(p->log, 1, "dtp20: response failed expected %d got %d ICOM err 0x%x\n",
				                                                              rsize,bread,se);
			} else {
				a1logd(p->log, 1, "dtp20: response failed ICOM err 0x%x\n",se);
			}
			return dtp20_interp_code((inst *)p, icoms2dtp20_err(se));
		}
		bsize -= bread;
		op += bread; 
	}
		
	rv = DTP20_OK;

	a1logd(p->log, 4, "dtp20: response '%s' ICOM err 0x%x\n",
	            icoms_tohex((unsigned char *)out, bread),rv);
	return dtp20_interp_code((inst *)p, rv);
}

/* Establish communications with a DTP20 */
/* Return DTP20_COMS_FAIL on failure to establish communications */
static inst_code
dtp20_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	dtp20 *p = (dtp20 *)pp;
	char buf[MAX_MES_SIZE];
	int se;
	inst_code ev = inst_ok;

	if (p->icom->port_type(p->icom) == icomt_usb) {

		a1logd(p->log, 4, "dtp20: About to init USB\n");

		/* Set config, interface, write end point, read end point, read quanta */
		if ((se = p->icom->set_usb_port(p->icom, 1, 0x00, 0x81, icomuf_none, 0, NULL)) != ICOM_OK) { 
			a1logd(p->log, 1, "dtp20: set_usbe_port failed ICOM err 0x%x\n",se);
			return dtp20_interp_code((inst *)p, icoms2dtp20_err(se));
		}

		/* Blind reset it twice - it seems to sometimes hang up */
		/* otherwise under OSX */
		dtp20_command(p, "0PR\r", buf, MAX_MES_SIZE, 0.5);
		dtp20_command(p, "0PR\r", buf, MAX_MES_SIZE, 0.5);

	} else {
		a1logd(p->log, 1, "dtp20: wrong communications type for device\n");
		return inst_internal_error;
	}

	/* Check instrument is responding */
	if ((ev = dtp20_command(p, "\r", buf, MAX_MES_SIZE, 1.5)) != inst_ok) {
		a1logd(p->log, 1, "dtp20: Failed to get a response from instrument\n");
		return inst_coms_fail;
	}

	/* Print the general information returned by instrument */
	if (p->log->verb) {
		int i, j;
		if ((ev = dtp20_command(p, "GI\r", buf, MAX_MES_SIZE, 2.0)) != inst_ok) {
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

	a1logd(p->log, 4, "dtp20: Got coms OK\n");

	p->gotcoms = 1;
	return inst_ok;
}

/* Build a strip definition as a set of passes, including DS command */
static void
build_strip(
dtp20 *p,
char *tp,			/* pointer to string buffer */
char *name,			/* Strip name (7 chars) (not used) */
int npatch,			/* Number of patches in the pass */
char *pname,		/* Pass name (3 chars) (not used) */
int sguide,			/* Guide number (not used) */
double pwid,		/* Patch length in mm */
double gwid,		/* Gap length in mm */
double twid			/* Trailer length in mm (For DTP41T) */
) {

	/* Enable SID field but don't report it, */
	/* since we expect to be reading DTP20 type charts. */
	/* (Could set to '1' for reading DTP41 type charts) */
	tp[0] = '0';
	
	/* Gap width */
	if((gwid <= 0.25))
		tp[1] = '0';
	else if(gwid <= 0.75)
		tp[1] = '1';
	else if(gwid <= 1.25)
		tp[1] = '2';
	else if(gwid <= 1.75)
		tp[1] = '3';
	else if(gwid <= 2.25)
		tp[1] = '4';
	else if(gwid <= 2.75)
		tp[1] = '5';
	else if(gwid <= 3.25)
		tp[1] = '6';
	else
		tp[1] = '7';
	
	/* Patch width in mm, dd.dd */
	if(pwid <= 6.75)
		tp[2] = '0';
	else if(pwid <= 8.0)
		tp[2] = '1';
	else if(pwid <= 11.25)
		tp[2] = '4';
	else if(pwid <= 12.75)
		tp[2] = '6';
	else 
		tp[2] = '7';
				
	/* Number of patches in strip */
	tp += 3;
	sprintf(tp, "%02d",npatch);
	tp += 2;

	*tp++ = 'D';				/* The DS command */
	*tp++ = 'S';
	*tp++ = '\r';				/* The CR */
	*tp++ = '\000';				/* The end */

}
		
/* Initialise the DTP20. */
/* return non-zero on an error, with instrument error code */
static inst_code
dtp20_init_inst(inst *pp) {
	dtp20 *p = (dtp20 *)pp;
	char buf[MAX_MES_SIZE];
	inst_code rv = inst_ok;
	char *envv;

	a1logd(p->log, 2, "dtp20_init_inst: called\n");

	if (p->gotcoms == 0)
		return inst_no_coms;		/* Must establish coms before calling init */


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

	/* Reset it (without disconnecting USB or clearing stored data) */
	if ((rv = dtp20_command(p, "0PR\r", buf, MAX_MES_SIZE, 2.0)) != inst_ok)
		return rv;
//	if ((rv = dtp20_command(p, "RI\r", buf, MAX_MES_SIZE, 2.0)) != inst_ok)
//		return rv;

	/* Set Response delimeter to CR */
	if ((rv = dtp20_command(p, "0008CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return rv;

	/* Get the model and version number */
	if ((rv = dtp20_command(p, "SV\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
		return rv;

	/* Check that it is a DTP20 */
	if (   strlen(buf) < 12
	    || (strncmp(buf,"X-Rite DTP20",12) != 0))
		return inst_unknown_model;

//	/* Set Beeper to off */
//	if ((rv = dtp20_command(p, "0001CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
//		return rv;

	/* Set Beeper to on */
	if ((rv = dtp20_command(p, "0101CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return rv;

	/* Set Automatic Transmit off */
	if ((rv = dtp20_command(p, "0005CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return rv;

	/* Set color data separator to TAB */
	if ((rv = dtp20_command(p, "0207CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return rv;

	/* Set 2 decimal digit resolution */
	if ((rv = dtp20_command(p, "020ACF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return rv;
		
	/* set default trigger mode */
	p->trig = inst_opt_trig_user_switch;

	/* - - - - - - - - - - - - - - - - - - - - - - - - */
	/* Setup for the type of measurements we want to do */
	/* Set instrument to XYZ mode */
	if ((rv = dtp20_command(p, "0518CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return rv;

	/* Disable multiple data output */
	if ((rv = dtp20_command(p, "001ACF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return rv;

	/* Set Illuminant to D50_2 */
	if ((rv = dtp20_command(p, "0416CF\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok)
		return rv;

	/* Reset retrieval of saved spot readings */ 
	p->savix = 0;

	a1logd(p->log, 2, "dtp20_init_inst: instrument inited OK\n");
	p->inited = 1;

	return inst_ok;
}

/* Read a full test chart composed of multiple sheets. */
/* This only works if the sheets have been read off-line */
/* DOESN'T use the trigger mode */
/* Return the inst error code */
static inst_code dtp20_read_chart(
inst *pp,
int npatch,			/* Total patches/values in chart */
int pich,			/* Passes (rows) in chart */
int sip,			/* Steps in each pass (patches in each row) */
int *pis,			/* Passes in each strip (rows in each sheet) */
int chid,			/* Chart ID number */
ipatch *vals) {		/* Pointer to array of values */
	dtp20 *p = (dtp20 *)pp;
	inst_code ev = inst_ok;
	char buf[MAX_RD_SIZE];
	int cs, sl, ttlp;
	double pw, gw;
	int u[10];
	int id = -1;
	ipatch *tvals;
	int six;		/* strip index */

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (!IMODETST(p->mode,inst_mode_s_ref_chart))
		return inst_unsupported;

	/* Confirm that there is a chart ready to read */
	if ((ev = dtp20_command(p, "CS\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
		return ev;
	if (sscanf(buf," %d ", &cs) != 1)
		return inst_protocol_error;
	if (cs != 3) {
		/* Seems to be no chart saved, but double check, in case of old firmware ( < 1.03) */
		if ((ev = dtp20_command(p, "00TS\r", buf, MAX_RD_SIZE, 2.0)) != inst_ok)
			return inst_nonesaved;
		if (sscanf(buf," %d ", &cs) != 1)
			return inst_nonesaved;
		if (cs == 0)
			return inst_nonesaved;
	}

	/* Get the TID */
	if ((ev = dtp20_command(p, "ST\r", buf, MAX_RD_SIZE, 2.0)) != inst_ok)
		return ev;
	if (sscanf(buf,"Strip Length: %d Total Patches: %d Patch Width: %lf mm Gap Width: %lf mm"
	               " User 1: %d User 2: %d User 3: %d User 4: %d User 5: %d User 6: %d"
	               " User 7: %d User 8: %d User 9: %d User 10: %d ",
	                 &sl, &ttlp, &pw, &gw, &u[0], &u[1], &u[2], &u[3], &u[4],
	                 &u[5], &u[6], &u[7], &u[8], &u[9]) != 14) {
		return inst_protocol_error;
	} 
	/* Compute the user data/chart id */
	if (u[0] == 0) {	/* This seems to be a chart ID */
		id = ((u[1] * 8 + u[2]) * 8 + u[3]) * 8 + u[4];
	}

	/* Check that the chart matches what we're reading */
	/* (Should we have a way of ignoring a chart if mismatch ?) */
	if (ttlp != npatch
	 || sl != sip
	 || (id != -1 && id != chid)) {
		a1logd(p->log, 2, "dtp20: Got %d, xpt %d patches, got %d xpt %d strip lgth, "
		                       "got %d xpt %d chart id\n",ttlp,npatch,sl,sip,id,chid);
		return inst_nochmatch;
	}

	a1logv(p->log, 1, "Chart has %d patches, %d per strip, chart id %d\n",ttlp,sl,id);

	/* Disable multiple data output */
	if ((ev = dtp20_command(p, "001ACF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
		return ev;

	/* Read each strip's values in turn */
	for (tvals = vals, six = 1; six <= pich; six++, tvals += sip) {
		char *tp, cmd[10];
		int i;

		a1logv(p->log, 1, "Reading saved strip %d of %d\n",six,pich);

		/* Select the strip to read */
		sprintf(cmd, "%03d01TS\r",six);

		/* Gather the results in D50_2 XYZ. This instruction seems unreliable! */
		if ((ev = dtp20_command(p, "0518CF\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
			return ev;
		if ((ev = dtp20_command(p, "0518CF\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
			return ev;
		if ((ev = dtp20_command(p, cmd, buf, MAX_RD_SIZE, STRIP_READ_TIMEOUT)) != inst_ok)
			return ev;

		/* Parse the buffer */
		/* Replace '\r' with '\000' */
		for (tp = buf; *tp != '\000'; tp++) {
			if (*tp == '\r')
				*tp = '\000';
		}
		for (tp = buf, i = 0; i < sip; i++) {
			if (*tp == '\000' || strlen(tp) > 40)
				return inst_protocol_error;
			if (sscanf(tp, " %lf %lf %lf ",
			           &tvals[i].XYZ[0], &tvals[i].XYZ[1], &tvals[i].XYZ[2]) != 3) {
				if (sscanf(tp, " %lf %lf %lf ",
				           &tvals[i].XYZ[0], &tvals[i].XYZ[1], &tvals[i].XYZ[2]) != 3) {
					return inst_protocol_error;
				}
			}
			tvals[i].loc[0] = '\000';
			tvals[i].mtype = inst_mrt_reflective;
			tvals[i].XYZ_v = 1;
			tvals[i].sp.spec_n = 0;
			tvals[i].duration = 0.0;
			tp += strlen(tp) + 1;
		}

		if (p->mode & inst_mode_spectral
		 || XCALSTD_NEEDED(p->target_calstd, p->native_calstd)
		 || p->custfilt_en) {

			/* Gather the results in Spectral reflectance */
			if ((ev = dtp20_command(p, "0318CF\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
				return ev;
			/* Set to binary */
			if ((ev = dtp20_command(p, "011BCF\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
				return ev;
			/* Read the strip */
			if ((ev = dtp20_bin_command(p, cmd, buf, 62 * sip, 5.0)) != inst_ok)
				return ev;
	
			/* Get each patches spectra */
			for (tp = buf, i = 0; i < sip; i++) {
				int j;
	
				/* Read the spectral value */
				for (j = 0; j < 31; j++, tp += 2) {
					int vv;
					vv = (unsigned char)tp[0];
					vv = vv * 256 + (unsigned char)tp[1];
					tvals[i].sp.spec[j] = vv * 200.0/65535.0;
				}
	
				tvals[i].sp.spec_n = 31;
				tvals[i].sp.spec_wl_short = 400.0;
				tvals[i].sp.spec_wl_long = 700.0;
				tvals[i].sp.norm = 100.0;
			}

			/* Set to ASCII */
			if ((ev = dtp20_command(p, "001BCF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
				return ev;
			/* Set back to in D50_2 XYZ. This instruction seems unreliable! */
			if ((ev = dtp20_command(p, "0518CF\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
				return ev;
		}

	}

	a1logv(p->log, 1, "All saved strips read\n");

	/* Apply any XRGA conversion */
	ipatch_convert_xrga(vals, npatch, xcalstd_nonpol, p->target_calstd, p->native_calstd,
	                    instClamp);

	/* Apply custom filter compensation */
	if (p->custfilt_en)
		ipatch_convert_custom_filter(vals, npatch, &p->custfilt, instClamp);

	return inst_ok;
}

/* Read a set of strips. This only works reading on-line */
/* Return the instrument error code */
static inst_code
dtp20_read_strip(
inst *pp,
char *name,			/* Strip name (7 chars) */
int npatch,			/* Number of patches in the pass */
char *pname,		/* Pass name (3 chars) */
int sguide,			/* Guide number */
double pwid,		/* Patch length in mm (For DTP20/DTP41) */
double gwid,		/* Gap length in mm (For DTP20/DTP41) */
double twid,		/* Trailer length in mm (For DTP41T) */
ipatch *vals) {		/* Pointer to array of instrument patch values */
	dtp20 *p = (dtp20 *)pp;
	char tbuf[200], *tp;
	char buf[MAX_RD_SIZE];
	int i, cs, se;
	inst_code ev = inst_ok;
	int user_trig = 0;
	int switch_trig = 0;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* This funtion isn't valid in saved data mode */
	if (!IMODETST(p->mode, inst_mode_ref_strip))
		return inst_wrong_setup;

	/* Until we get the right status or give up */
	for (i = 0;;i++) {
		/* Confirm that we are ready to read strips */
		if ((ev = dtp20_command(p, "CS\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
			return ev;
		if (sscanf(buf," %d ", &cs) != 1)
			return inst_protocol_error;
		if (cs == 1)
			break;			/* Read and empty of offline patches */
		if (cs == 2 || cs == 3)
			return (inst_misread | DTP20_NOT_EMPTY);	/* Has onffline patches */	
		if (i < 20) {
			if (cs == 0 || (cs >= 4 && cs <= 12)) {	/* Ignore transient status */
				msec_sleep(100);
				continue;
			}
		}
		return (inst_misread | DTP20_UNEXPECTED_STATUS);
	}

	/* Send strip definition */
	build_strip(p, tbuf, name, npatch, pname, sguide, pwid, gwid, twid);

	if ((ev = dtp20_command(p, tbuf, buf, MAX_MES_SIZE, 1.5)) != inst_ok) {
		a1logv(p->log, 1, "Interactive strip reading won't work on Firmware earlier than V1.03 !\n");
		return ev;
	}

	if (p->trig == inst_opt_trig_user_switch) {
		int touts = 0;

		/* Wait for a strip value to turn up, or a user command/abort */
		for (;;) {

			ev = dtp20_command(p, "CS\r", buf, MAX_MES_SIZE, 0.2);

			if (ev == (inst_coms_fail | DTP20_TIMEOUT)) {
				/* Ignore transient timeouts */
				if (touts++ > 40)
					return ev;
				continue; 

			} else if (ev != inst_ok) {
				if ((ev & inst_mask) == inst_needs_cal)
					p->need_cal = 1;
				return ev;				/* Error */
			} else {					
				int stat;
				if (sscanf(buf, " %d ", &stat) != 1)
					stat = 6;

				/* Ignore benign status - wait for busy or error */
				if (stat != 4 && stat != 6 && stat != 7) {
					/* Not ready - Check for user trigger or command */
					if (p->uicallback != NULL) {
						if ((ev = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
							if (ev == inst_user_abort) {
								return ev;			/* Error or user abort */
							} else if (ev == inst_user_trig) {
								user_trig = 1;		/* User trigger */
								break;
							}
						}
					}
					/* Keep waiting */
					msec_sleep(200);
					continue;

				} else {
					/* Ready - continue on */
					switch_trig = 1;
					break;
				}
			}
		}
		/* Notify of trigger */
		if (p->uicallback)
			p->uicallback(p->uic_cntx, inst_triggered); 

	} else if (p->trig == inst_opt_trig_user) {
		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "dtp20: inst_opt_trig_user but no uicallback function set!\n");
			return inst_unsupported;
		}

		for (;;) {
			if ((ev = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
				if (ev == inst_user_abort) 
					return ev;
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
			return ev;
	}

	/* Trigger a read if the switch has not been used */
	if (switch_trig == 0) {
		/* Do a strip read */
		if ((ev = dtp20_command(p, "RM\r", buf, MAX_MES_SIZE, 10.0)) != inst_ok) {
			if ((ev & inst_mask) == inst_needs_cal)
				p->need_cal = 1;
			return ev;
		}
	}

	/* Disable multiple data output */
	if ((ev = dtp20_command(p, "001ACF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
		return ev;
	/* Gather the results in D50_2 XYZ This instruction seems unreliable! */
	if ((ev = dtp20_command(p, "0518CF\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
		return ev;
	if ((ev = dtp20_command(p, "0518CF\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
		return ev;
	if ((ev = dtp20_command(p, "01TS\r", buf, MAX_RD_SIZE, STRIP_READ_TIMEOUT)) != inst_ok)
		return ev;

	/* Parse the buffer */
	/* Replace '\r' with '\000' */
	for (tp = buf; *tp != '\000'; tp++) {
		if (*tp == '\r')
			*tp = '\000';
	}
	for (tp = buf, i = 0; i < npatch; i++) {
		if (*tp == '\000' || strlen(tp) > 40)
			return inst_protocol_error;
		if (sscanf(tp, " %lf %lf %lf ",
		           &vals[i].XYZ[0], &vals[i].XYZ[1], &vals[i].XYZ[2]) != 3) {
			if (sscanf(tp, " %lf %lf %lf ",
			           &vals[i].XYZ[0], &vals[i].XYZ[1], &vals[i].XYZ[2]) != 3) {
				return inst_protocol_error;
			}
		}
		vals[i].loc[0] = '\000';
		vals[i].mtype = inst_mrt_reflective;
		vals[i].XYZ_v = 1;
		vals[i].sp.spec_n = 0;
		vals[i].duration = 0.0;
		tp += strlen(tp) + 1;

	}

	if (p->mode & inst_mode_spectral
	 || XCALSTD_NEEDED(p->target_calstd, p->native_calstd)) {

		/* Gather the results in Spectral reflectance */
		if ((ev = dtp20_command(p, "0318CF\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
			return ev;
		/* Set to binary */
		if ((ev = dtp20_command(p, "011BCF\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
			return ev;
	
		/* Read the strip */
		if ((ev = dtp20_bin_command(p, "01TS\r", buf, 62 * npatch, STRIP_READ_TIMEOUT)) != inst_ok)
			return ev;

		/* Get each patches spectra */
		for (tp = buf, i = 0; i < npatch; i++) {
			int j;

			/* Read the spectral value */
			for (j = 0; j < 31; j++, tp += 2) {
				int vv;
				vv = (unsigned char)tp[0];
				vv = vv * 256 + (unsigned char)tp[1];
				vals[i].sp.spec[j] = vv * 200.0/65535.0;
			}

			vals[i].sp.spec_n = 31;
			vals[i].sp.spec_wl_short = 400.0;
			vals[i].sp.spec_wl_long = 700.0;
			vals[i].sp.norm = 100.0;
		}

		/* Set back to ASCII */
		if ((ev = dtp20_command(p, "001BCF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		/* Set back to D50 2 degree */
		if ((ev = dtp20_command(p, "0518CF\r", buf, MAX_RD_SIZE, 0.5)) != inst_ok)
			return ev;
	}

	/* Wait for instrument to become not busy */
	for (;;) {
		if ((ev = dtp20_command(p, "CS\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok) {
			return ev;
		} else {
			int stat = 4;
			if (sscanf(buf, " %d ", &stat) != 1 || stat != 4)
				break;
			msec_sleep(200);
		}
	}


	/* Apply any XRGA conversion */
	ipatch_convert_xrga(vals, npatch, xcalstd_nonpol, p->target_calstd, p->native_calstd,
	                    instClamp);

	if (user_trig)
		return inst_user_trig;
	return inst_ok;
}


/* Read a single sample. */
/* This works both on-line and off-line. */
/* Return the instrument error code */
static inst_code
dtp20_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	dtp20 *p = (dtp20 *)pp;
	char buf[MAX_MES_SIZE], *tp;
	int se;
	inst_code ev = inst_ok;
	int switch_trig = 0;
	int user_trig = 0;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* This combination doesn't make any sense... */
	if (IMODETST(p->mode, inst_mode_s_ref_spot) && p->trig == inst_opt_trig_user_switch) {
		return inst_wrong_setup;
	}

	if (p->trig == inst_opt_trig_user_switch) {
		int touts = 0;

		/* Wait for a sample value to turn up, or a user abort */
		for (;;) {

			/* Check for user trigger */
			if (p->uicallback != NULL) {
				if ((ev = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
					if (ev == inst_user_abort) 
						return ev;			/* User abort */
					if (ev == inst_user_trig) {
						user_trig = 1;
						break;
					}
				}
			}

			/* Check for an instrument switch trigger */
			ev = dtp20_command(p, "CS\r", buf, MAX_MES_SIZE, 0.2);

			if (ev == (inst_coms_fail | DTP20_TIMEOUT)) {	/* Assume we're waiting for trigger */
				if (touts++ > 20)
					return ev;
				continue; 
			
			} else if (ev != inst_ok) {
				if ((ev & inst_mask) == inst_needs_cal)
					p->need_cal = 1;
				return ev;				/* Error */

			} else {	/* Got a CS response */
				int stat;
				if (sscanf(buf, " %d ", &stat) != 1)
					stat = 6;
				/* Ignore benign status - wait for busy or error */
				if (stat != 4 && stat != 6 && stat != 7) {
					msec_sleep(200);
					continue;
				}
				switch_trig = 1;
				break;
			}
		}
		/* Notify of trigger */
		if (p->uicallback)
			p->uicallback(p->uic_cntx, inst_triggered); 

	} else if (p->trig == inst_opt_trig_user) {
		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "dtp20: inst_opt_trig_user but no uicallback function set!\n");
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

	/* Read saved spot values */
	if (IMODETST(p->mode, inst_mode_s_ref_spot)) {
		char cmd[10];
		int nsr;

		/* See how many saved spot readings there are */
		if ((ev = dtp20_command(p, "00GM\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		if (sscanf(buf," %d ", &nsr) != 1)
			return inst_protocol_error;

		if (nsr <= p->savix)
			return inst_nonesaved;

		p->savix++;
		sprintf(cmd, "%03d01GM\r",p->savix);

		/* Disable multiple data output */
		if ((ev = dtp20_command(p, "001ACF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		/* Gather the results in D50_2 XYZ This instruction seems unreliable! */
		if ((ev = dtp20_command(p, "0518CF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		if ((ev = dtp20_command(p, "0518CF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		if ((ev = dtp20_command(p, cmd, buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;

	/* Read real spot values */
	} else {
		/* Trigger a read if the switch has not been used */
		if (switch_trig == 0) {
			/* Do a spot read */
			if ((ev = dtp20_command(p, "RM\r", buf, MAX_MES_SIZE, 5.0)) != inst_ok) {
				if ((ev & inst_mask) == inst_needs_cal)
					p->need_cal = 1;
				return ev;
			}
		}
	
		/* Disable multiple data output */
		if ((ev = dtp20_command(p, "001ACF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		/* Gather the results in D50_2 XYZ */
		if ((ev = dtp20_command(p, "0518CF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		if ((ev = dtp20_command(p, "0518CF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		if ((ev = dtp20_command(p, "01GM\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;

	}

	val->XYZ[0] = val->XYZ[1] = val->XYZ[2] = 0.0;

	if (*buf == '\000' || strlen(buf) > 40)
		return inst_protocol_error;

	if (sscanf(buf, " %lf %lf %lf ", &val->XYZ[0], &val->XYZ[1], &val->XYZ[2]) != 3) {
		return inst_protocol_error;
	}
	/* This may not change anything since instrument may clamp */
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);
	val->loc[0] = '\000';
	val->mtype = inst_mrt_reflective;
	val->XYZ_v = 1;
	val->sp.spec_n = 0;
	val->duration = 0.0;


	if (p->mode & inst_mode_spectral
	 || XCALSTD_NEEDED(p->target_calstd, p->native_calstd)) {
		int j;

		/* Set to read spectral reflectance */
		if ((ev = dtp20_command(p, "0318CF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		/* Set to binary */
		if ((ev = dtp20_command(p, "011BCF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		/* Get measurement */
		if ((ev = dtp20_bin_command(p, "01GM\r", buf, 62, 2.0)) != inst_ok)
			return ev;

		for (j = 0; j < 31; j++)
			val->sp.spec[j] = 0.0;

		/* Read the spectral value */
		for (tp = buf, j = 0; j < 31; j++, tp += 2) {
			int vv;
			vv = (unsigned char)tp[0];
			vv = vv * 256 + (unsigned char)tp[1];
			val->sp.spec[j] = vv * 200.0/65535.0;
		}

		val->sp.spec_n = 31;
		val->sp.spec_wl_short = 400.0;
		val->sp.spec_wl_long = 700.0;
		val->sp.norm = 100.0;

		/* Set to ASCII */
		if ((ev = dtp20_command(p, "001BCF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		/* Set to D50 2 degree */
		if ((ev = dtp20_command(p, "0518CF\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
	}


	if (!IMODETST(p->mode, inst_mode_s_ref_spot)) {

		/* Clear the spot database so our reading doesn't appear as a stored reading */
		if ((ev = dtp20_command(p, "02CD\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok)
			return ev;
		p->savix = 0;

		/* Wait for instrument to become not busy */
		for (;;) {
			if ((ev = dtp20_command(p, "CS\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok) {
				return ev;
			} else {
				int stat = 4;
				if (sscanf(buf, " %d ", &stat) != 1 || stat != 4)
					break;
				msec_sleep(200);
			}
		}
	}

	/* Apply any XRGA conversion */
	ipatch_convert_xrga(val, 1, xcalstd_nonpol, p->target_calstd, p->native_calstd, clamp);

	if (user_trig)
		return inst_user_trig;
	return inst_ok;
}

/* Return needed and available inst_cal_type's */
static inst_code dtp20_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	dtp20 *p = (dtp20 *)pp;
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
		
	if (p->need_cal)
		n_cals |= inst_calt_ref_white;
	a_cals |= inst_calt_ref_white;

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
static inst_code dtp20_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
inst_calc_id_type *idtype,	/* Condition identifier type */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	dtp20 *p = (dtp20 *)pp;
	char buf[MAX_MES_SIZE];
	inst_code ev;
    inst_cal_type needed, available;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	*idtype = inst_calc_id_none;
	id[0] = '\000';

	if ((ev = dtp20_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
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

		a1logd(p->log,4,"dtp20_calibrate: doing calt 0x%x\n",calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return inst_ok;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return inst_unsupported;
	}

	if (*calt & inst_calt_ref_white) {
		int i;

		if ((*calc & inst_calc_cond_mask) != inst_calc_man_ref_white) {
			char *cp;
			if ((ev = dtp20_command(p, "04SN\r", buf, MAX_MES_SIZE, 4.5)) != inst_ok)
				return ev;
			for (cp = buf; *cp >= '0' && *cp <= '9'; cp++)
				;
			*cp = '\000';
			strcpy(id, buf);
			*idtype = inst_calc_id_ref_sn;
			*calc = inst_calc_man_ref_white;
			return inst_cal_setup;
		}

		if ((ev = dtp20_command(p, "CR\r", buf, MAX_MES_SIZE, 4.5)) != inst_ok)
			return ev;
		
		/* Wait for instrument to become not busy */
		for (;;) {
			if ((ev = dtp20_command(p, "CS\r", buf, MAX_MES_SIZE, 0.2)) != inst_ok) {
				return ev;
			} else {
				int stat = 4;
				if (sscanf(buf, " %d ", &stat) != 1 || stat != 4)
					break;
				msec_sleep(200);
			}
		}

		p->need_cal = 0;
		*calt &= ~inst_calt_ref_white;
	}

	return inst_ok;
}

/* Error codes interpretation */
static char *
dtp20_interp_error(inst *pp, int ec) {
//	dtp20 *p = (dtp20 *)pp;
	ec &= inst_imask;
	switch (ec) {

		case DTP20_INTERNAL_ERROR:
			return "Internal software error";
		case DTP20_COMS_FAIL:
			return "Communications failure";
		case DTP20_UNKNOWN_MODEL:
			return "Not a DTP20";
		case DTP20_DATA_PARSE_ERROR:
			return "Data from DTP didn't parse as expected";

		case DTP20_NOT_EMPTY:
			return "Trying to read strips when there is already\n"
			       "an offline chart partially read. Clear the instrument and try again";
		case DTP20_UNEXPECTED_STATUS:
			return "Unexpected instrument status";

		case DTP20_OK:
			return "No device error";

		case DTP20_MEASUREMENT_STATUS:
			return "Measurement complete";
		
		case DTP20_BAD_COMMAND:
			return "Unrecognised command";
		case DTP20_BAD_PARAMETERS:
			return "Wrong number of parameters";
		case DTP20_PRM_RANGE_ERROR:
			return "One or more parameters are out of range";
		case DTP20_BUSY:
			return "Instrument is busy - command ignored";
			
		case DTP20_MEASUREMENT_ERROR:
			return "General measurement error";
		case DTP20_TIMEOUT:
			return "Receive timeout";
		case DTP20_BAD_STRIP:
			return "Bad strip";
			
		case DTP20_NEEDS_CAL_ERROR:
			return "Instrument needs calibration";
		case DTP20_CAL_FAILURE_ERROR:
			return "Calibration failed";
			
		case DTP20_INSTRUMENT_ERROR:
			return "General instrument error";
		case DTP20_LAMP_ERROR:
			return "Reflectance lamp error";
		
		case DTP20_BAD_TID:
			return "Invalid TID detected, Re-scan TID";
		case DTP20_FLASH_ERASE_FAILURE:
			return "Flash erase operation failed, Contact support";
		case DTP20_FLASH_WRITE_FAILURE:
			return "Flash write operation failed, Contact support";
		case DTP20_FLASH_VERIFY_FAILURE:
			return "Flash verify operation failed, Contact support";
		case DTP20_MEMORY_ERROR:
			return "Memory access failed, Contact support";
		case DTP20_ADC_ERROR:
			return "Analog to digital converter error, Contact support";
		case DTP20_PROCESSOR_ERROR:
			return "General processor error, Contact support";
		case DTP20_BATTERY_ERROR:
			return "General battery error occurred, Contact support";
		case DTP20_BATTERY_LOW_ERROR:
			return "Battery level too low to measure, Charge battery";
		case DTP20_INPUT_POWER_ERROR:
			return "Input power out of range, Contact support";
		
		case DTP20_BATTERY_ABSENT_ERROR:
			return "Battery could not be detected, Contact support";
		case DTP20_BAD_CONFIGURATION:
			return "Stored configuration data invalid, Set as desired";
		
		case DTP20_BAD_SPOT:
			return "Invalid spot reading was requested, Re-read or resend";
		case DTP20_END_OF_DATA:
			return "End of profile reached, None";
		case DTP20_DBASE_PROFILE_NOT_EMPTY:
			return "Profile database not empty, Clear profile data";
		case DTP20_MEMORY_OVERFLOW_ERROR:
			return "Memory overflow error, Contact support";
		case DTP20_BAD_CALIBRATION:
			return "Bad calibration data detected, Contact support";
		
		case DTP20_CYAN_CAL_ERROR:
			return "Failed cyan calibration during TID read, Re-scan TID";
		case DTP20_MAGENTA_CAL_ERROR:
			return "Failed magenta calibration during TID read, Re-scan TID";
		case DTP20_YELLOW_CAL_ERROR:
			return "Failed yellow calibration during TID read, Re-scan TID";
		case DTP20_PATCH_SIZE_ERROR:
			return "Invalid strip patch size was detected, Re-scan";
		case DTP20_FAIL_PAPER_CHECK:
			return "Failed to verify scan started/stopped on paper, Re-scan";
		case DTP20_SHORT_SCAN_ERROR:
			return "Less than minimum positional ticks detected, Re-scan";
		case DTP20_STRIP_READ_ERROR:
			return "General strip reading error, Re-scan";
		case DTP20_SHORT_TID_ERROR:
			return "Failed TID length verification, Re-scan TID";
		case DTP20_SHORT_STRIP_ERROR:
			return "Strip length invalid, Re-scan";
		case DTP20_EDGE_COLOR_ERROR:
			return "Strip edge color was measured invalid, Re-scan";
		case DTP20_SPEED_ERROR:
			return "Manual scan too fast to gather data, Re-scan";
		case DTP20_UNDEFINED_SCAN_ERROR:
			return "General scan error, Re-scan";
		case DTP20_INVALID_STRIP_ID:
			return "A strip ID field was out-of-range, Re-scan";
		case DTP20_BAD_SERIAL_NUMBER:
			return "A bad serial number has been detected, Contact support";
		case DTP20_TID_ALREADY_SCANNED:
			return "A TID has already been scanned, Scan strips";
		case DTP20_PROFILE_DATABASE_FULL:
			return "Profile database is full, Clear profile data";
		
		case DTP20_SPOT_DATABASE_FULL:
			return "Spot database is full, Clear spot data";
		case DTP20_TID_STRIP_MIN_ERROR:
			return "A TID was specified with fewer than 5 patches, Re-define TID";
		case DTP20_REREAD_DATABASE_FULL:
			return "Strip reread database is full (can't reread), Clear profile data";
		case DTP20_STRIP_DEFINE_TOO_SHORT:
			return "Strip definition contains too few patches, Re-define strip";
		case DTP20_STRIP_DEFINE_TOO_LONG:
			return "Strip definition contains too many patches, Re-define strip";
		case DTP20_BAD_STRIP_DEFINE:
			return "No valid strip defined, Define strip";
		
		case DTP20_BOOTLOADER_MODE:
			return "Instrument is in FW update mode, Reset or load FW";

		default:
			return "Unknown error code";
	}
}

/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
dtp20_interp_code(inst *pp, int ec) {
//	dtp20 *p = (dtp20 *)pp;

	switch (ec) {

		case DTP20_OK:
			return inst_ok;

		case DTP20_MEASUREMENT_STATUS:
		case DTP20_END_OF_DATA:
			return inst_notify | ec;

		case DTP20_INTERNAL_ERROR:
			return inst_internal_error | ec;

		case DTP20_TIMEOUT:
		case DTP20_COMS_FAIL:
			return inst_coms_fail | ec;

		case DTP20_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case DTP20_DATA_PARSE_ERROR:
		case DTP20_BAD_COMMAND:
		case DTP20_BAD_PARAMETERS:
		case DTP20_PRM_RANGE_ERROR:
		case DTP20_BUSY:
			return inst_protocol_error | ec;

		case DTP20_MEASUREMENT_ERROR:
		case DTP20_BAD_STRIP:
		case DTP20_BAD_SPOT:
		case DTP20_CAL_FAILURE_ERROR:
		case DTP20_BAD_TID:
		case DTP20_CYAN_CAL_ERROR:
		case DTP20_MAGENTA_CAL_ERROR:
		case DTP20_YELLOW_CAL_ERROR:
		case DTP20_PATCH_SIZE_ERROR:
		case DTP20_FAIL_PAPER_CHECK:
		case DTP20_SHORT_SCAN_ERROR:
		case DTP20_STRIP_READ_ERROR:
		case DTP20_SHORT_TID_ERROR:
		case DTP20_SHORT_STRIP_ERROR:
		case DTP20_EDGE_COLOR_ERROR:
		case DTP20_SPEED_ERROR:
		case DTP20_UNDEFINED_SCAN_ERROR:
		case DTP20_INVALID_STRIP_ID:
		case DTP20_TID_ALREADY_SCANNED:
		case DTP20_NOT_EMPTY:
		case DTP20_UNEXPECTED_STATUS:
			return inst_misread | ec;

		case DTP20_NEEDS_CAL_ERROR:
			return inst_needs_cal | ec;

		case DTP20_INSTRUMENT_ERROR:
		case DTP20_LAMP_ERROR:
		case DTP20_FLASH_ERASE_FAILURE:
		case DTP20_FLASH_WRITE_FAILURE:
		case DTP20_FLASH_VERIFY_FAILURE:
		case DTP20_MEMORY_ERROR:
		case DTP20_ADC_ERROR:
		case DTP20_PROCESSOR_ERROR:
		case DTP20_BATTERY_ERROR:
		case DTP20_INPUT_POWER_ERROR:
		case DTP20_BATTERY_LOW_ERROR:
		case DTP20_BATTERY_ABSENT_ERROR:
		case DTP20_MEMORY_OVERFLOW_ERROR:
		case DTP20_BAD_CALIBRATION:
		case DTP20_BAD_SERIAL_NUMBER:
			return inst_hardware_fail | ec;

		case DTP20_BAD_CONFIGURATION:
		case DTP20_DBASE_PROFILE_NOT_EMPTY:
		case DTP20_PROFILE_DATABASE_FULL:
		case DTP20_SPOT_DATABASE_FULL:
		case DTP20_TID_STRIP_MIN_ERROR:
		case DTP20_REREAD_DATABASE_FULL:
		case DTP20_STRIP_DEFINE_TOO_SHORT:
		case DTP20_STRIP_DEFINE_TOO_LONG:
		case DTP20_BAD_STRIP_DEFINE:
		case DTP20_BOOTLOADER_MODE:
			return inst_wrong_setup | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
dtp20_del(inst *pp) {
	dtp20 *p = (dtp20 *)pp;
	if (p->icom != NULL)
		p->icom->del(p->icom);
	p->vdel(pp);
	free(p);
}

/* Set the instrument capabilities */
static void	set_capabilities(dtp20 *p) {

	p->cap = inst_mode_ref_spot
	       | inst_mode_ref_strip
	       | inst_mode_s_ref_spot
	       | inst_mode_s_ref_chart
	       | inst_mode_colorimeter
	       | inst_mode_spectral
	       ;

	p->cap2 = inst2_prog_trig
		    | inst2_user_switch_trig
	        | inst2_user_trig
	        | inst2_has_battery
	        ;

	p->cap3 = inst3_none;
}


/* Return the instrument capabilities */
static void dtp20_capabilities(inst *pp,
inst_mode *cap1,
inst2_capability *cap2,
inst3_capability *cap3) {
	dtp20 *p = (dtp20 *)pp;

	if (p->cap == inst_mode_none)
		set_capabilities(p);

	if (cap1 != NULL)
		*cap1 = p->cap;
	if (cap2 != NULL)
		*cap2 = p->cap2;
	if (cap3 != NULL)
		*cap3 = p->cap3;
}

/* 
 * check measurement mode
 */
static inst_code
dtp20_check_mode(inst *pp, inst_mode m) {
	dtp20 *p = (dtp20 *)pp;
	inst_mode cap;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	pp->capabilities(pp, &cap, NULL, NULL);

	/* Simple test */
	if (m & ~cap)
		return inst_unsupported;

	/* Check specific modes */
	if (!IMODETST(m, inst_mode_ref_spot)
	 && !IMODETST(m, inst_mode_ref_strip)
	 && !IMODETST(m, inst_mode_s_ref_spot)
	 && !IMODETST(m, inst_mode_s_ref_chart)) {
		return inst_unsupported;
	}

	return inst_ok;
}

/* 
 * set measurement mode
 */
static inst_code
dtp20_set_mode(inst *pp, inst_mode m) {
	dtp20 *p = (dtp20 *)pp;
	inst_code ev;

	if ((ev = dtp20_check_mode(pp, m)) != inst_ok)
		return ev;

	p->mode = m;

	return inst_ok;
}

/* Get a status or get or set an option */
static inst_code dtp20_get_set_opt(
inst *pp,
inst_opt_type m,	/* Requested status type */
...) {				/* Status parameters */                             
	dtp20 *p = (dtp20 *)pp;

	/* Record the trigger mode */
	if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_user
	 || m == inst_opt_trig_user_switch) {
		p->trig = m;

		return inst_ok;
	}

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

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

	if (m == inst_stat_saved_readings) {
		char buf[MAX_MES_SIZE];
		int ev;
		va_list args;
		inst_stat_savdrd *fe;
		int nsr, cs;

		va_start(args, m);
		fe = va_arg(args, inst_stat_savdrd *);
		va_end(args);

		*fe = inst_stat_savdrd_none;

		/* See how many saved spot readings there are */
		if ((ev = dtp20_command(p, "00GM\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		if (sscanf(buf," %d ", &nsr) != 1)
			return inst_protocol_error;
		if (nsr > p->savix)
			*fe |= inst_stat_savdrd_spot;
		
		/* See if the instrument has read a partial or complete target */
		if ((ev = dtp20_command(p, "CS\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		if (sscanf(buf," %d ", &cs) != 1)
			return inst_protocol_error;
		if (0 && (cs == 2 || cs == 3)) {		// ??? Is this unreliable ???
			*fe |= inst_stat_savdrd_chart;
		} else {
			/* Seems to be no chart saved, but double check, in case of old firmware */
			if ((ev = dtp20_command(p, "00TS\r", buf, MAX_MES_SIZE, 2.0)) == inst_ok) {
				if (sscanf(buf," %d ", &cs) == 1) {
					if (cs != 0) {
						*fe |= inst_stat_savdrd_chart;
					}
				}
			}
		}
		return inst_ok;
	}

	/* Return the number of saved spot readings */
	if (m == inst_stat_s_spot) {
		int ev;
		char buf[MAX_MES_SIZE];
		int *pnsr;
		va_list args;

		va_start(args, m);
		pnsr = va_arg(args, int *);
		va_end(args);

		*pnsr = -1;

		/* See how many saved spot readings there are */
		if ((ev = dtp20_command(p, "00GM\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		if (sscanf(buf," %d ", pnsr) != 1)
			return inst_protocol_error;

		if (*pnsr -= p->savix)		/* ??? */

		return inst_ok;
	}

	/* Return the saved chart details */
	if (m == inst_stat_s_chart) {
		inst_code ev = inst_ok;
		char buf[MAX_RD_SIZE], *bp, *ep;
		int u[10];
		va_list args;
		int *no_patches, *no_rows, *pat_per_row, *chart_id, *missing_row;
		int i, cs;
		double pw, gw;

		va_start(args, m);
		no_patches = va_arg(args, int *);
		no_rows = va_arg(args, int *);
		pat_per_row = va_arg(args, int *);
		chart_id = va_arg(args, int *);
		missing_row = va_arg(args, int *);
		va_end(args);

		*no_patches = *no_rows = *pat_per_row = *chart_id = *missing_row = -1;
	
		/* Get the TID */
		if ((ev = dtp20_command(p, "ST\r", buf, MAX_RD_SIZE, 2.0)) != inst_ok)
			return ev;
		if (sscanf(buf,"Strip Length: %d Total Patches: %d Patch Width: %lf mm Gap Width: %lf mm"
		               " User 1: %d User 2: %d User 3: %d User 4: %d User 5: %d User 6: %d"
		               " User 7: %d User 8: %d User 9: %d User 10: %d ",
		                 pat_per_row, no_patches, &pw, &gw, &u[0], &u[1], &u[2], &u[3], &u[4],
		                 &u[5], &u[6], &u[7], &u[8], &u[9]) != 14) {
			return inst_protocol_error;
		} 
		/* Compute number of rows */
		*no_rows = *no_patches / *pat_per_row;

		/* Compute the user data/chart id */
		if (u[0] == 0) 	/* This seems to be a chart ID */
			*chart_id = ((u[1] * 8 + u[2]) * 8 + u[3]) * 8 + u[4];

		/* See if any strips are missing */
		if ((ev = dtp20_command(p, "CS\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		if (sscanf(buf," %d ", &cs) != 1)
			return inst_protocol_error;
		if (cs == 2) {
			if ((ev = dtp20_command(p, "01TT\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
				return ev;
			bp = buf;
			for (i = 1; i <= *no_rows; i++) {
				int strno = 0;

				/* Locate the next number */
				while(*bp != '\000' && (*bp < '0' || *bp > '9'))
					bp++;

				/* Locate the end of the number */
				ep = bp;
				while (*ep != '\000' && *ep >= '0' && *ep <= '9') 
					ep++;
				*ep = '\000';

				if (ep > bp)
					strno = atoi(bp);
				if (strno != i) {
					*missing_row = i;		/* Assume no missing rows */
					break;
				}
				bp = ep+1;
				if (bp >= (buf + MAX_MES_SIZE))
					return inst_protocol_error;
			}
		}

		return inst_ok;
	}

	/* Return the charged status of the battery */
	if (m == inst_stat_battery) {
		int ev;
		char buf[MAX_MES_SIZE];
		double *pbchl;
		va_list args;
		int cs;

		va_start(args, m);
		pbchl = va_arg(args, double *);
		va_end(args);

		*pbchl = -1.0;

		/* See how charged the battery is */
		if ((ev = dtp20_command(p, "06BA\r", buf, MAX_MES_SIZE, 0.5)) != inst_ok)
			return ev;
		if (sscanf(buf," %d ", &cs) != 1)
			return inst_protocol_error;

		if (cs == 4)
			*pbchl = 1.0;
		else if (cs == 3)
			*pbchl = 0.75;
		else if (cs == 2)
			*pbchl = 0.50;
		else if (cs == 1)
			*pbchl = 0.25;
		else
			*pbchl = 0.0;

		return inst_ok;
	}

	/* !! It's not clear if there is a way of knowing */
	/* whether the instrument has a UV filter !! */

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
extern dtp20 *new_dtp20(icoms *icom, instType dtype) {
	dtp20 *p;
	if ((p = (dtp20 *)calloc(sizeof(dtp20),1)) == NULL) {
		a1loge(icom->log, 1, "new_dtp20: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	p->init_coms       = dtp20_init_coms;
	p->init_inst       = dtp20_init_inst;
	p->capabilities    = dtp20_capabilities;
	p->check_mode      = dtp20_check_mode;
	p->set_mode        = dtp20_set_mode;
	p->get_set_opt     = dtp20_get_set_opt;
	p->read_chart      = dtp20_read_chart;
	p->read_strip      = dtp20_read_strip;
	p->read_sample     = dtp20_read_sample;
	p->get_n_a_cals    = dtp20_get_n_a_cals;
	p->calibrate       = dtp20_calibrate;
	p->interp_error    = dtp20_interp_error;
	p->del             = dtp20_del;

	p->icom = icom;
	p->dtype = dtype;
	p->cap = inst_mode_none;		/* Unknown until set */
	p->mode = inst_mode_none;		/* Not in a known mode yet */

	return p;
}
