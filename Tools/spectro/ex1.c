
/* 
 * Argyll Color Correction System
 *
 * Image Engineering EX1 related functions
 *
 * Author: Graeme W. Gill
 * Date:   4/4/2015
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
#include "ex1.h"

static inst_code ex1_interp_code(inst *pp, int ec);

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 8000		/* Maximum reading message reply size */

/* Interpret an icoms error into a EX1 error */
static int icoms2ex1_err(int se) {
	if (se != ICOM_OK) {
		if (se & ICOM_TO)
			return EX1_TIMEOUT; 
		return EX1_COMS_FAIL;
	}
	return EX1_OK;
}

/* Debug - dump a command packet at debug level deb1 */
static void dump_command(ex1 *p, ORD8 *buf, int len, int debl) {
	if (debl < p->log->debug)
		return;

	if (len < 64) {
		a1logd(p->log, 4, "Command packet too short (%d bytes)\n",len);
		return;
	}

	if (buf[0] != 0xC1 || buf[1] != 0xC0) {
		a1logd(p->log, 4, "Command missing start bytes (0x%02x, 0x%02x)\n",buf[0],buf[1]);
	}

	// ~~~~999 
	// etc.
}

/* Do a full command/response exchange with the ex1 */
/* (This level is not multi-thread safe) */
/* Return the ex1 error code. */
static int
ex1_fcommand(
ex1 *p,
char *in,			/* In string */
char *out,			/* Out string buffer */
int bsize,			/* Out buffer size */
double to,			/* Timeout in seconds */
int ntc,			/* Number or termination chars */
int ctype,			/* 0 = normal, 1 = *init, 2 = refr reading */
int nd				/* nz to disable debug messages */
) {
	int se;
	int bread = 0;
	char *cp, *tc = "", *dp;

	// ~~~99 
	return EX1_NOT_IMP;
 
	if (ctype == 0)
		tc = "\r\006\025";		/* Return, Ack or Nak */
	else if (ctype == 1)
		tc = "\007\025";		/* Bell or Nak */
	else if (ctype == 2)
		tc = "\r\025";			/* Return or Nak */

	se = p->icom->write_read(p->icom, in, 0, out, bsize, &bread, tc, ntc, to);

	/* Because we are sometimes waiting for 3 x \r characters to terminate the read, */
	/* we will instead time out on getting a single NAK (\025), so convert timout */
	/* with bytes to non-timeout, so that we can process the error. */
	if (se == ICOM_TO && bread > 0)
		se = ICOM_OK;

	if (se != 0) {
		if (!nd) a1logd(p->log, 1, "ex1_fcommand: serial i/o failure on write_read '%s' 0x%x\n",icoms_fix(in),se);
		return icoms2ex1_err(se);
	}

	/* See if there was an error, and remove any enquire codes */
	for (dp = cp = out; *cp != '\000' && (dp - out) < bsize; cp++) {
		if (*cp == '\025') {	/* Got a NAK */
			char buf[100];

			if ((se = p->icom->write_read(p->icom, "*stat:err?\r", 0, buf, 100, NULL, "\r", 1, 1.0)) != 0) {
				if (!nd) a1logd(p->log, 1, "ex1_fcommand: serial i/o failure on write_read '%s'\n",icoms_fix(in));
				return icoms2ex1_err(se);;
			}
			if (sscanf(buf, "Error Code: %d ",&se) != 1) {
				if (!nd) a1logd(p->log, 1, "ex1_fcommand: failed to parse error code '%s'\n",icoms_fix(buf));
				return EX1_DATA_PARSE_ERROR;
			}
					
			if (!nd) a1logd(p->log, 1, "Got ex1 error code %d\n",se);
			break;
		}
		if (*cp == '\005')	/* Got an Enquire */
			continue;		/* remove it */
		*dp = *cp;
		dp++;
	}
	out[bsize-1] = '\000';

	if (!nd) a1logd(p->log, 4, "ex1_fcommand: command '%s' returned '%s' bytes %d, err 0x%x\n",
	                                          icoms_fix(in), icoms_fix(out),strlen(out), se);
	return se;
}

/* Do a normal command/response echange with the ex1. */
/* (This level is not multi-thread safe) */
/* Return the inst code */
static inst_code
ex1_command(
struct _ex1 *p,
char *in,			/* In string */
char *out,			/* Out string buffer */
int bsize,			/* Out buffer size */
double to) {		/* Timout in seconds */
	int rv = ex1_fcommand(p, in, out, bsize, to, 1, 0, 0);
	return ex1_interp_code((inst *)p, rv);
}

/* Read another line of response */
/* (This level is not multi-thread safe) */
/* Return the inst code */
static int
ex1_readresp(
struct _ex1 *p,
char *out,			/* Out string buffer */
int bsize,			/* Out buffer size */
double to			/* Timeout in seconds */
) {
	int rv, se;
	char *cp, *tc = "\r\006\025";		/* Return, Ack or Nak */

	// ~~~999
	return inst_unsupported;

	if ((se = p->icom->read(p->icom, out, bsize, NULL, tc, 1, to)) != 0) {
		a1logd(p->log, 1, "ex1_readresp: serial i/o failure\n");
		return icoms2ex1_err(se);
	}
	return inst_ok;
}

/* Establish communications with a ex1 */
/* Return EX1_COMS_FAIL on failure to establish communications */
static inst_code
ex1_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	ex1 *p = (ex1 *) pp;
	char buf[MAX_MES_SIZE];
	baud_rate brt[] = { baud_921600, baud_115200, baud_38400, baud_nc };
	instType itype = pp->itype;
	int se;

	inst_code ev = inst_ok;

	a1logd(p->log, 2, "ex1_init_coms: called\n");

	amutex_lock(p->lock);

	if (p->icom->port_type(p->icom) != icomt_usb) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "ex1_init_coms: wrong communications type for device!\n");
		return inst_coms_fail;
	}
	
	// ~~~99 check it is responding.

	// ~~~99 check the model type

	a1logd(p->log, 2, "ex1_init_coms: init coms has suceeded\n");

	p->gotcoms = 1;

	amutex_unlock(p->lock);

	return inst_ok;
}

/* Initialise the EX1 */
/* return non-zero on an error, with dtp error code */
static inst_code
ex1_init_inst(inst *pp) {
	ex1 *p = (ex1 *)pp;
	char mes[100];
	char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "ex1_init_inst: called\n");

	if (p->gotcoms == 0)
		return inst_internal_error;		/* Must establish coms before calling init */

	amutex_lock(p->lock);
	
	/* Restore the instrument to it's default settings */
	if ((ev = ex1_command(p, "*conf:default\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok)
		return ev;

	/* Set calibration type to auto on ambient cap */
	if ((ev = ex1_command(p, "*para:calibn 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	/* Set auto exposure/integration time */
	/* Set calibration type to auto on ambient cap */
	if ((ev = ex1_command(p, "*para:expo 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok
	 || (ev = ex1_command(p, "*para:adapt 2\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
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
	if ((ev = ex1_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	/* Set the measurement function to be Radiometric spectrum */
	if ((ev = ex1_command(p, "*conf:func 6\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	/* Set the measurement format to ASCII */
	if ((ev = ex1_command(p, "*conf:form 4\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	if ((ev = ex1_command(p, "*para:wavbeg?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	if (sscanf(buf, "Predefined start wave: %lf ",&p->wl_short) != 1) {
		amutex_unlock(p->lock);
		a1loge(p->log, 1, "ex1_init_inst: failed to parse start wave\n");
		return ev;
	}
	a1logd(p->log, 1, " Short wl range %f\n",p->wl_short);

	if ((ev = ex1_command(p, "*para:wavend?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	if (sscanf(buf, "Predefined end wave: %lf ",&p->wl_long) != 1) {
		amutex_unlock(p->lock);
		a1loge(p->log, 1, "ex1_init_inst: failed to parse end wave\n");
		return ev;
	}
	if (p->wl_long > 830.0)			/* Could go to 1000 with 1211 */
		p->wl_long = 830.0;

	a1logd(p->log, 1, " Long wl range %f\n",p->wl_long);

	p->nbands = (int)((p->wl_long - p->wl_short + 1.0)/1.0 + 0.5);

	/* Set the wavelength range and resolution */
	sprintf(mes, "*conf:wran %d %d 1\r", (int)(p->wl_short+0.5), (int)(p->wl_long+0.5));
	if ((ev = ex1_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	/* Set to average just 1 reading */
	if ((ev = ex1_command(p, "*conf:aver 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	if (p->log->verb) {
		int val;
		char *sp;

		if ((ev = ex1_command(p, "*idn?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
		if ((sp = strrchr(buf, '\r')) != NULL)
			*sp = '\000';
		a1logv(p->log, 1, " Identificaton:       %s\n",buf);
		
		if ((ev = ex1_command(p, "*vers?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
		
		if ((sp = strrchr(buf, '\r')) != NULL)
			*sp = '\000';
		a1logv(p->log, 1, " Firmware:            %s\n",buf);
		
		if ((ev = ex1_command(p, "*para:spnum?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
		if (sscanf(buf, "spectrometer number: %d ",&val) == 1) {
			a1logv(p->log, 1, " Spectrometer number: %d\n",val);
		}
		
		if ((ev = ex1_command(p, "*para:serno?\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return ev;
		}
		if (sscanf(buf, "serial number: %d ",&val) == 1) {
			a1logv(p->log, 1, " Serial number:       %d\n",val);
		}
	}

	p->inited = 1;
	a1logd(p->log, 2, "ex1_init_inst: instrument inited OK\n");
	amutex_unlock(p->lock);

	return inst_ok;
}

static inst_code ex1_imp_measure_set_refresh(ex1 *p);
static inst_code ex1_imp_set_refresh(ex1 *p);

/* Get the ambient diffuser position */
/* (This is not multithread safe) */
static inst_code
ex1_get_diffpos(
	ex1 *p,				/* Object */
	int *pos,				/* 0 = display, 1 = ambient */
	int nd					/* nz = no debug message */
) {
	char buf[MAX_RD_SIZE];
	int ec;

	/* See if we're in emissive or ambient mode */
	if ((ec = ex1_fcommand(p, "*contr:mhead?\r", buf, MAX_MES_SIZE, 1.0, 1, 0, nd)) != inst_ok) {
		return ex1_interp_code((inst *)p, ec);
	}
	if (sscanf(buf, "mhead: %d ",pos) != 1) {
		a1logd(p->log, 2, "ex1_init_coms: unrecognised measuring head string '%s'\n",icoms_fix(buf));
		return inst_protocol_error;
	}
	return inst_ok;
}

/* Get the target laser state */
/* (This is not multithread safe) */
static inst_code
ex1_get_target_laser(
	ex1 *p,				/* Object */
	int *laser,				/* 0 = off, 1 = on */
	int nd					/* nz = no debug message */
) {
	char buf[MAX_RD_SIZE];
	int ec;
	int lstate;

	if ((ec = ex1_fcommand(p, "*contr:laser?\r", buf, MAX_MES_SIZE, 1.0, 1, 0, nd)) != inst_ok) {
		return ex1_interp_code((inst *)p, ec);
	}
	if (sscanf(buf, "laser: %d ",&lstate) != 1) {
		a1loge(p->log, 2, "ex1_get_target_laser: failed to parse laser state\n");
		return inst_protocol_error;
	}
	*laser = lstate;
	return inst_ok;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
ex1_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	ex1 *p = (ex1 *)pp;
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
			a1logd(p->log, 1, "ex1: inst_opt_trig_user but no uicallback function set!\n");
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
	if ((rv = ex1_get_diffpos(p, &pos, 0) ) != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}

	/* Attempt a refresh display frame rate calibration if needed */
	if (p->refrmode != 0 && p->rrset == 0) {
		a1logd(p->log, 1, "ex1: need refresh rate calibration before measure\n");
		if ((rv = ex1_imp_measure_set_refresh(p)) != inst_ok) {
			amutex_unlock(p->lock);
			return rv; 
		}
	}
		
	/* Trigger a measurement */
	if ((ec = ex1_fcommand(p, "*init\r", buf, MAX_MES_SIZE, 5.0 * p->measto + 10.0 , 1, 1, 0)) != EX1_OK) {
		amutex_unlock(p->lock);
		return ex1_interp_code((inst *)p, ec);
	}



	if (ec == EX1_OK) {

		if (sscanf(buf, " X: %lf Y: %lf Z: %lf ",
	           &val->XYZ[0], &val->XYZ[1], &val->XYZ[2]) != 3) {
			amutex_unlock(p->lock);
			a1logd(p->log, 1, "ex1_read_sample: failed to parse '%s'\n",buf);
			return inst_protocol_error;
		}
	
		amutex_unlock(p->lock);
		return ex1_interp_code((inst *)p, ec);
	}

	/* This may not change anything since instrument may clamp */
	if (clamp)
	icmClamp3(val->XYZ, val->XYZ);
	val->loc[0] = '\000';
	if (p->mode & inst_mode_ambient) {
		val->mtype = inst_mrt_ambient;
	} else
		val->mtype = inst_mrt_emission;
	val->XYZ_v = 1;		/* These are absolute XYZ readings */
	val->sp.spec_n = 0;
	val->duration = 0.0;
	rv = inst_ok;


	/* spectrum data is returned only if requested */
	if (p->mode & inst_mode_spectral) {
		int tries, maxtries = 5;
		int i, xsize;
		char *cp, *ncp;
 
		/* (Format 12 doesn't seem to work on the 1211) */
		/* (Format 9 reportedly doesn't work on the 1201) */
		/* The folling works on the 1211 and is reported to work on the 1201 */

		/* Because the ex1 doesn't use flow control in its */
		/* internal serial communications, it may overrun */
		/* the FT232R buffer, so retry fetching the spectra if */
		/* we get a comm error or parsing error. */
		for (tries = 0;;) {

			/* Fetch the spectral readings */
			ec = ex1_fcommand(p, "*fetch:sprad\r", buf, MAX_RD_SIZE, 2.0, 2+p->nbands+1, 0, 0);
			tries++;
			if (ec != EX1_OK) {
				if (tries > maxtries) {
					amutex_unlock(p->lock);
					a1logd(p->log, 1, "ex1_fcommand: failed with 0x%x\n",ec);
					return ex1_interp_code((inst *)p, ec);
				}
				continue;	/* Retry the fetch */
			}
	
			val->sp.spec_n = p->nbands;
			val->sp.spec_wl_short = p->wl_short;
			val->sp.spec_wl_long = p->wl_long;
	
			/* Spectral data is in W/nm/m^2 */
			val->sp.norm = 1.0;
			cp = buf;
			for (i = -2; i < val->sp.spec_n; i++) {
				if ((ncp = strchr(cp, '\r')) == NULL) {
					a1logd(p->log, 1, "ex1_read_sample: failed to parse spectra at %d/%d\n",i+1,val->sp.spec_n);
					if (tries > maxtries) {
						amutex_unlock(p->lock);
						return inst_protocol_error;
					}
					continue;		/* Retry the fetch and parse */
				}
				*ncp = '\000';
				if (i >= 0) {
					a1logd(p->log, 6, "sample %d/%d got %f from '%s'\n",i+1,val->sp.spec_n,atof(cp),cp);
					val->sp.spec[i] = 1000.0 * atof(cp);	/* Convert to mW/m^2/nm */
					if (p->mode & inst_mode_ambient)
						val->mtype = inst_mrt_ambient;
				}
				cp = ncp+1;
			}
			/* We've parsed correctly, so don't retry */
			break;
		}
		a1logd(p->log, 1, "ex1_read_sample: got total %d samples/%d expected in %d tries\n",i,val->sp.spec_n, tries);
	}
	amutex_unlock(p->lock);


	if (user_trig)
		return inst_user_trig;
	return rv;
}

/* Set the instrument to match the current refresh settings */
/* (Not thread safe) */
static inst_code
ex1_imp_set_refresh(ex1 *p) {
	char buf[MAX_MES_SIZE];
	inst_code rv;

	if (p->model == 1201)
		return inst_unsupported; 

	/* Set synchronised read if we should do so */
	if (p->refrmode != 0 && p->refrvalid) {
		char mes[100];
		if ((rv = ex1_command(p, "*conf:cycmod 1\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			return rv;
		}
		sprintf(mes,"*conf:cyctim %f\r",p->refperiod * 1e6);
		if ((rv = ex1_command(p, mes, buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			return rv;
		}
		a1logd(p->log,5,"ex1_imp_set_refresh set refresh rate to %f Hz\n",1.0/p->refperiod);
	} else {
		if ((rv = ex1_command(p, "*conf:cycmod 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
			return rv;
		}
		a1logd(p->log,5,"ex1_imp_set_refresh set non-refresh mode\n");
	}
	return inst_ok;
}

/* Implementation of read refresh rate */
/* (Not thread safe) */
/* Return 0.0 if none detectable */
static inst_code
ex1_imp_measure_refresh(
ex1 *p,
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
	if ((rv = ex1_command(p, "*contr:laser 0\r", buf, MAX_MES_SIZE, 1.0)) != inst_ok) {
		return rv;
	}

	if ((ec = ex1_fcommand(p, "*contr:cyctim 200 4000\r", buf, MAX_MES_SIZE, 5.0, 1, 2, 0)) != EX1_OK) {
		return ex1_interp_code((inst *)p, ec);
	}

	if ((cp = strchr(buf, 'c')) == NULL)
		cp = buf;
	if (sscanf(cp, "cyctim[ms]: %lf ", &refperiod) != 1) {
		a1logd(p->log, 1, "ex1_read_refrate rate: failed to parse string '%s'\n",icoms_fix(buf));
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
ex1_read_refrate(
inst *pp,
double *ref_rate
) {
	ex1 *p = (ex1 *)pp;
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
	if ((rv = ex1_imp_measure_refresh(p, &refrate)) != inst_ok) {
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
ex1_imp_measure_set_refresh(
	ex1 *p			/* Object */
) {
	inst_code rv;
	double refrate = 0.0;
	int mul;
	double pval;

	if ((rv = ex1_imp_measure_refresh(p, &refrate)) != inst_ok) {
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

	if ((rv = ex1_imp_set_refresh(p)) != inst_ok) {
		return rv;
	}

	return inst_ok;
}

/* Measure and then set refperiod, refrate if possible */
static inst_code
ex1_measure_set_refresh(
	ex1 *p			/* Object */
) {
	int rv;

	amutex_lock(p->lock);
	rv = ex1_imp_measure_set_refresh(p);
	amutex_unlock(p->lock);
	return rv;
}

/* Return needed and available inst_cal_type's */
static inst_code ex1_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	ex1 *p = (ex1 *)pp;
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
inst_code ex1_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	ex1 *p = (ex1 *)pp;
	inst_code ev;
    inst_cal_type needed, available;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	id[0] = '\000';

	if ((ev = ex1_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
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

		a1logd(p->log,4,"ex1_calibrate: doing calt 0x%x\n",calt);

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
		if ((ev = ex1_measure_set_refresh(p)) != inst_ok)
			return ev; 

		*calt &= ~inst_calt_ref_freq;
	}
	return inst_ok;
}

/* Return the last calibrated refresh rate in Hz. Returns: */
static inst_code ex1_get_refr_rate(inst *pp,
double *ref_rate
) {
	ex1 *p = (ex1 *)pp;
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
static inst_code ex1_set_refr_rate(inst *pp,
double ref_rate
) {
	ex1 *p = (ex1 *)pp;
	inst_code rv;

	a1logd(p->log,5,"ex1_set_refr_rate %f Hz\n",ref_rate);

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
	if ((rv = ex1_imp_set_refresh(p)) != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}
	amutex_unlock(p->lock);

	return inst_ok;
}


/* Error codes interpretation */
static char *
ex1_interp_error(inst *pp, int ec) {
//	ex1 *p = (ex1 *)pp;
	ec &= inst_imask;
	switch (ec) {
		case EX1_INTERNAL_ERROR:
			return "Internal software error";
		case EX1_TIMEOUT:
			return "Communications timeout";
		case EX1_COMS_FAIL:
			return "Communications failure";
		case EX1_UNKNOWN_MODEL:
			return "Not a JETI ex1";
		case EX1_DATA_PARSE_ERROR:
			return "Data from ex1 didn't parse as expected";


		case EX1_OK:
			return "No device error";

		case EX1_NOT_IMP:
			return "Not implemented";

		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
ex1_interp_code(inst *pp, int ec) {

	ec &= inst_imask;
	switch (ec) {

		case EX1_OK:
			return inst_ok;

//			return inst_internal_error | ec;

//			return inst_coms_fail | ec;

//			return inst_unknown_model | ec;

//			return inst_protocol_error | ec;

//			return inst_wrong_config | ec;

//			return inst_bad_parameter | ec;

//			return inst_misread | ec;

//			return inst_hardware_fail | ec;

		case EX1_NOT_IMP:
			return inst_unsupported | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
ex1_del(inst *pp) {
	if (pp != NULL) {
		ex1 *p = (ex1 *)pp;
		if (p->icom != NULL)
			p->icom->del(p->icom);
		amutex_del(p->lock);
		free(p);
	}
}

/* Return the instrument mode capabilities */
static void ex1_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	ex1 *p = (ex1 *)pp;
	inst_mode cap1 = 0;
	inst2_capability cap2 = 0;

	cap1 |= inst_mode_emis_tele
//	     |  inst_mode_ambient			// ?? is it 
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

// ~~~~9999
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
/* NOTE that conf_ix values shoudn't be changed, as it is used as a persistent key */
static inst_code ex1_meas_config(
inst *pp,
inst_mode *mmodes,
inst_cal_cond *cconds,
int *conf_ix
) {
	ex1 *p = (ex1 *)pp;
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
		if ((ev = ex1_get_diffpos(p, &pos, 0)) != inst_ok) {
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
static inst_code ex1_check_mode(inst *pp, inst_mode m) {
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
static inst_code ex1_set_mode(inst *pp, inst_mode m) {
	ex1 *p = (ex1 *)pp;
	int refrmode;
	inst_code ev;

	if ((ev = ex1_check_mode(pp, m)) != inst_ok)
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

static inst_disptypesel ex1_disptypesel[3] = {
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
static inst_code ex1_get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	ex1 *p = (ex1 *)pp;
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
		*psels = ex1_disptypesel;

	return inst_ok;
}

/* Given a display type entry, setup for that type */
static inst_code set_disp_type(ex1 *p, inst_disptypesel *dentry) {
	inst_code rv;
	int refrmode;

	refrmode = dentry->refr;

	a1logd(p->log,5,"ex1 set_disp_type refmode %d\n",refrmode);

	if (     IMODETST(p->mode, inst_mode_emis_norefresh_ovd)) {	/* Must test this first! */
		refrmode = 0;
	} else if (IMODETST(p->mode, inst_mode_emis_refresh_ovd)) {
		refrmode = 1;
	}

	if (p->refrmode != refrmode)
		p->rrset = 0;					/* This is a hint we may have swapped displays */
	p->refrmode = refrmode; 

	amutex_lock(p->lock);
	if ((rv = ex1_imp_set_refresh(p)) != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}
	amutex_unlock(p->lock);

	return inst_ok;
}

/* Set the display type - refresh or not */
static inst_code ex1_set_disptype(inst *pp, int ix) {
	ex1 *p = (ex1 *)pp;
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

	a1logd(p->log,5,"ex1  ex1_set_disptype ix %d\n",ix);
	dentry = &ex1_disptypesel[ix];

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
ex1_get_set_opt(inst *pp, inst_opt_type m, ...)
{
	ex1 *p = (ex1 *)pp;
	char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;

	a1logd(p->log, 5, "ex1_get_set_opt: opt type 0x%x\n",m);

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

	return inst_unsupported;
}

/* Constructor */
extern ex1 *new_ex1(icoms *icom, instType itype) {
	ex1 *p;
	if ((p = (ex1 *)calloc(sizeof(ex1),1)) == NULL) {
		a1loge(icom->log, 1, "new_ex1: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	p->init_coms         = ex1_init_coms;
	p->init_inst         = ex1_init_inst;
	p->capabilities      = ex1_capabilities;
	p->meas_config       = ex1_meas_config;
	p->check_mode        = ex1_check_mode;
	p->set_mode          = ex1_set_mode;
	p->get_disptypesel   = ex1_get_disptypesel;
	p->set_disptype      = ex1_set_disptype;
	p->get_set_opt       = ex1_get_set_opt;
	p->read_sample       = ex1_read_sample;
	p->read_refrate      = ex1_read_refrate;
	p->get_n_a_cals      = ex1_get_n_a_cals;
	p->calibrate         = ex1_calibrate;
	p->get_refr_rate     = ex1_get_refr_rate;
	p->set_refr_rate     = ex1_set_refr_rate;
	p->interp_error      = ex1_interp_error;
	p->del               = ex1_del;

	p->icom = icom;
	p->itype = icom->itype;

	amutex_init(p->lock);

	return p;
}

