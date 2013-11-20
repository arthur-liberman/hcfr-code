
/* 
 * Argyll Color Correction System
 *
 * Gretag Spectrolino and Spectroscan related
 * defines and declarations.
 *
 * Author: Graeme W. Gill
 * Date:   13/7/2005
 *
 * Copyright 2005 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Derived from DTP41.h
 *
 * This is an alternative driver to spm/gretag.
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

	There is a bug or limitation with using -N to skip the calibration
	when using any of the emissive modes - the readings end up being nearly zero.

	We aren't saving the spectrolino fake tranmission white reference in
	a calibration file, so -N doesn't work with it.

	You can't trigger a calibration reading using the instrument switch.

	You should be able to do use the table enter key anywhere the user
	is asked to hit a key.

	The corner positioning could be smarter.

	The SpectroscanT transmission cal. merely reminds the user (vie verbose)
	that it is assuming the correct apatture, rather than given them
	a chance to change it.

 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <math.h>
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#else /* SALONEINSTLIB */
#include "sa_config.h"
#include "numsup.h"
#endif /* SALONEINSTLIB */
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "ss.h"

/* Default flow control */
#define DEFFC fc_Hardware

#include <stdarg.h>

/* Some tables to convert between emums and text descriptions */

/* Filter type */
char* filter_desc[] = {
	"Filter not defined",
	"No Filter (U)",
	"Polarizing Filter",
	"D65 Filter",
	"(Unknown Filter)",
	"UV cut Filter",
	"Custon Filter"
};

#define SS_REF_CAL_COUNT 50
#define SS_TRANS_CAL_COUNT 10

/* Track the number of measurements taken */
static void inc_calcount(ss *p) {
	p->calcount++;
}

/* Check if a calibration should be done, and set flags appropriately. */
/* (atstart should be nz at start of serpentine) */
static void check_calcount(ss *p, int atstart) {
	a1logd(p->log, 4, "ss check_calcount: atstart %d, calcount %d, pisrow %d\n",
	                                              atstart,p->calcount,p->pisrow);
	if ((p->mode & inst_mode_illum_mask) == inst_mode_reflection) {
		/* If forced or out and back will cross count, do cal now. */
		if (p->forcecalib							/* Forced */
		 || p->calcount >= SS_REF_CAL_COUNT			/* Needs cal now */
		 || (   atstart								/* At start of up & back */
		     && p->pisrow < SS_REF_CAL_COUNT		/* possible not to cross */
		     && (p->calcount + p->pisrow) > SS_REF_CAL_COUNT)) {	/* and will cross */
			p->need_wd_cal = 1;
			p->forcecalib = 0;
			a1logd(p->log, 4, "ss check_calcount: need_wd_cal set\n");
		}
	} else if (((p->mode & inst_mode_illum_mask) == inst_mode_transmission
	     && (p->calcount >= SS_TRANS_CAL_COUNT || p->forcecalib))) {
		p->forcecalib = 0;
		p->need_t_cal = 2;
		a1logd(p->log, 4, "ss check_calcount: need_t_cal set\n");
	}
}

/* Establish communications with a Spectrolino/Spectroscan */
/* Use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
ss_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	ss *p = (ss *)pp;
	/* We're a bit stuffed if the Specrolino/scan is set to 28800, since */
	/* this rate isn't universally supported by computer systems. */
	baud_rate brt[7] = { baud_9600,           baud_19200,         baud_57600,
	                     baud_2400,           baud_1200,          baud_600,
	                     baud_300 };
	ss_bt ssbrc[7]   = { ss_bt_9600,          ss_bt_19200,        ss_bt_57600,
	                     ss_bt_2400,          ss_bt_1200,         ss_bt_600,
	                     ss_bt_300 };
	ss_ctt sobrc[7]  = { ss_ctt_SetBaud9600, ss_ctt_SetBaud19200, ss_ctt_SetBaud57600,
	                     ss_ctt_SetBaud2400, ss_ctt_SetBaud1200,  ss_ctt_SetBaud600,
	                     ss_ctt_SetBaud300 };
	ss_ctt fcc1;
	ss_hst fcc2;
	long etime;
	int ci, bi, i, se;
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "ss_init_coms: About to init Serial I/O\n");

	/* Deal with flow control setting */
	if (fc == fc_nc)
		fc = DEFFC;
	
	if (fc == fc_XonXOff) {
		fcc1 = ss_ctt_ProtokolWithXonXoff;
		fcc2 = ss_hst_XonXOff;
	} else if (fc == fc_Hardware) {
		fcc1 = ss_ctt_ProtokolWithHardwareHS;
		fcc2 = ss_hst_Hardware;
	} else {
		fc = fc_none;
		fcc1 = ss_ctt_ProtokolWithoutXonXoff;
		fcc2 = ss_hst_None;
	}

	/* Figure Spectrolino baud rate being asked for */
	for (bi = 0; bi < 7; bi++) {
		if (brt[bi] == br)
			break;
	}
	if (bi >= 7)
		bi = 0;

	/* Figure current icoms baud rate */
	for (ci = 0; ci < 7; ci++) {
		if (brt[ci] == p->icom->br)
			break;
	}
	if (ci >= 7)
		ci = bi;	

	/* The tick to give up on */
	etime = clock() + (long)(CLOCKS_PER_SEC * tout + 0.5);

	/* Until we establish comms or give up */
	while (clock() < etime) {

		/* Until we time out, find the correct baud rate */
		for (i = ci; clock() < etime;) {
			a1logd(p->log, 4, "ss_init_coms: trying baud rate %d\n",i);
			if ((se = p->icom->set_ser_port(p->icom, fc_none, brt[i], parity_none,
				                                    stop_1, length_8)) != ICOM_OK) { 
				a1logd(p->log, 1, "ss_init_coms: set_ser_port failed ICOM err 0x%x\n",se);
				p->snerr = icoms2ss_err(se);
				return ss_inst_err(p);
			}

			/* Try a SpectroScan Output Status */
			ss_init_send(p);
			ss_add_ssreq(p, ss_OutputStatus);
			ss_command(p, SH_TMO);
			
			if (ss_sub_1(p) == ss_AnsPFX) {				/* Got comms */
				p->itype = instSpectroScan;				/* Preliminary */
				break;
			}

			/* Try a Spectrolino Parameter Request */
			ss_init_send(p);
			ss_add_soreq(p, ss_ParameterRequest);
			ss_command(p, SH_TMO);
			
			if (ss_sub_1(p) == ss_ParameterAnswer) {	/* Got comms */
			 	p->itype = instSpectrolino;
				break;
			}
			
			/* Check for user abort */
			if (p->uicallback != NULL) {
				inst_code ev;
				if ((ev = p->uicallback(p->uic_cntx, inst_negcoms)) == inst_user_abort) {
					a1logd(p->log, 1, "ss_init_coms: user aborted\n");
					return inst_user_abort;
				}
			}

			if (++i >= 7)
				i = 0;
		}
		break;			/* Got coms */
	}

	if (clock() >= etime) {		/* We haven't established comms */
		return inst_coms_fail;
	}

	a1logd(p->log, 4, "ss_init_coms: got basic communications\n");

	/* Finalise the communications */
	if (p->itype == instSpectrolino) {

		if ((ev = so_do_MeasControlDownload(p, fcc1)) != inst_ok)
			return ev;

		/* Do baudrate change without checking results */
		so_do_MeasControlDownload(p, sobrc[bi]);
		if ((se = p->icom->set_ser_port(p->icom, fc, brt[bi], parity_none,
			                                stop_1, length_8)) != ICOM_OK) { 
			a1logd(p->log, 1, "ss_init_coms: spectrolino set_ser_port failed ICOM err 0x%x\n",se);
			p->snerr = icoms2ss_err(se);
			return ss_inst_err(p);
		}

	} else {	/* Spectroscan */

		ss_do_SetDeviceOnline(p);	/* Put the device online */

		/* Make sure other communication parameters are right */
		if ((ev = ss_do_ChangeHandshake(p, fcc2)) != inst_ok)
			return ev;

		/* Do baudrate change without checking results */
		ss_do_ChangeBaudRate(p, ssbrc[bi]); 
		if ((se = p->icom->set_ser_port(p->icom, fc, brt[bi], parity_none,
			                                      stop_1, length_8)) != ICOM_OK) { 
			a1logd(p->log, 1, "ss_init_coms: spectroscan set_ser_port failed ICOM err 0x%x\n",se);
			p->snerr = icoms2ss_err(se);
			return ss_inst_err(p);
		}

		/* Make sure the Spectrolino is talking to us. */
		if ((ev = ss_do_ScanSpectrolino(p)) != inst_ok) {
			a1logd(p->log, 1, "ss_init_coms: spectroscan, instrument isn't communicating ICOM err 0x%x\n",se);
			return ev;
		}
	}

	a1logd(p->log, 4, "ss_init_coms: establish communications\n");

	/* See if we have a Spectroscan or SpectroscanT, and get other details */
	p->itype = instUnknown;
	{
		char devn[19];

		ev = ss_do_OutputType(p, devn);

		if (ev == inst_ok) {
			a1logd(p->log, 5, "ss_init_coms: got device name '%s'\n",devn);
			if (strncmp(devn, "SpectroScanT",12) == 0) {
				p->itype = instSpectroScanT;
			} else if (strncmp(devn, "SpectroScan",11) == 0) {
				p->itype = instSpectroScan;
			}
		}
	}
	/* Check if there's a Spectrolino */
	{
		char devn[19];
		int sn, sr, yp, mp, dp, hp, np;		/* Date & Time of production */
		int fswl, nosw, dsw;				/* Wavelengths sampled */
		ss_ttt tt;							/* Target Type */

		ev = so_do_TargetIdRequest(p, devn, &sn, &sr, &yp, &mp, &dp, &hp, &np,
		                           &tt, &fswl, &nosw, &dsw);

		if (ev == inst_ok) {

			a1logd(p->log, 4, "ss_init_coms:\n"
			" Got device name '%s'\n"
			" Got target type '%d'\n"
			" Start wl %d, no wl %d, wl space %d\n", devn ,tt, fswl, nosw, dsw);

			/* "Spectrolino" and "Spectrolino 8mm" are known */
			if (tt != ss_ttt_Spectrolino
			  || strncmp(devn, "Spectrolino",11) != 0)
				return inst_unknown_model;
	
			if (p->itype == instUnknown)	/* No SpectrScan */
			 	p->itype = instSpectrolino;
		}
	}
	
#ifdef EMSST
	a1logv(p->log, 0, "DEBUG: Emulating SpectroScanT with SpectroScan!\n");
#endif

	p->gotcoms = 1;

	a1logd(p->log, 2, "ss_init_coms: init coms has suceeded\n");

	return inst_ok;
}

/* Set the capabilities values for the type of instrument */
static void ss_determine_capabilities(ss *p) {

	/* Set the capabilities mask */
	p->cap = inst_mode_ref_spot
	       | inst_mode_emis_spot
	       | inst_mode_emis_tele
	       | inst_mode_colorimeter
	       | inst_mode_spectral
	       ;

	if (p->itype == instSpectrolino) {
		p->cap |= inst_mode_trans_spot;		/* Support this manually using a light table */
	}

	if (p->itype == instSpectroScan
	 || p->itype == instSpectroScanT)	/* Only in reflective mode */
		p->cap |= inst_mode_ref_xy;

	if (p->itype == instSpectroScanT) {
		p->cap |= inst_mode_trans_spot;
	}

	/* Set the capabilities mask 2 */
	p->cap2 = inst2_prog_trig
	        | inst2_user_trig
	        | inst2_user_switch_trig
	        ;

	if (p->itype == instSpectroScan
	 || p->itype == instSpectroScanT) {
		/* These are not available in transmission mode */
		if ((p->mode & inst_mode_illum_mask) != inst_mode_transmission) {
			p->cap2 |= inst2_xy_holdrel
		            |  inst2_xy_locate
		            |  inst2_xy_position
	            	   ;
		}
	}

	p->cap3 = inst3_none;

	a1logd(p->log, 4, "ss_determine_capabilities got cap1 0x%x cap2 0x%x\n",p->cap,p->cap2);
}

/* Initialise the Spectrolino/SpectroScan. */
/* return non-zero on an error, with dtp error code */
static inst_code
ss_init_inst(inst *pp) {
	ss *p = (ss *)pp;
	inst_code rv = inst_ok;

	a1logd(p->log, 2, "ss_init_inst: called\n");

	if (p->gotcoms == 0)
		return inst_internal_error;		/* Must establish coms before calling init */

	/* Reset the instrument to a known state */
	if (p->itype != instSpectrolino) { 
		
		/* Initialise the device without resetting the baud rate */
		if (p->itype == instSpectroScanT) {
			if ((rv = ss_do_SetTableMode(p, ss_tmt_Reflectance)) != inst_ok)
				return rv; 
		}
		if ((rv = ss_do_SetDeviceOnline(p)) != inst_ok)
			return rv;
		if ((rv = ss_do_ResetKeyAcknowlge(p)) != inst_ok)
			return rv;
		if ((rv = ss_do_ReleasePaper(p)) != inst_ok)
			return rv;
		if ((rv = ss_do_InitMotorPosition(p)) != inst_ok)
			return rv;

		if (p->log->verb) {
			char dn[19];		/* Device name */
			unsigned int sn;	/* Serial number */
			char pn[9];			/* Part number */
			int yp;				/* Year of production (e.g. 1996) */
			int mp;				/* Month of production (1-12) */
			int dp;				/* Day of production (1-31) */
			char sv[13];		/* Software version */

			if ((rv = ss_do_OutputType(p, dn)) != inst_ok)
				return rv;
			if ((rv = ss_do_OutputSerialNumber(p, &sn)) != inst_ok)
				return rv;
			if ((rv = ss_do_OutputArticleNumber(p, pn)) != inst_ok)
				return rv;
			if ((rv = ss_do_OutputProductionDate(p, &yp, &mp, &dp)) != inst_ok)
				return rv;
			if ((rv = ss_do_OutputSoftwareVersion(p, sv)) != inst_ok)
				return rv;

			a1logv(p->log, 1,
			       " Device:     %s\n"
			       " Serial No:  %u\n"
			       " Part No:    %s\n"
			       " Prod Date:  %d/%d/%d\n"
			       " SW Version: %s\n", dn, sn, pn, dp, mp, yp, sv);
		}
	}
	/* Do Spectrolino stuff */
	if ((rv = so_do_ResetStatusDownload(p, ss_smt_InitWithoutRemote)) != inst_ok)
		return rv;
	if ((rv = so_do_ExecWhiteRefToOrigDat(p)) != inst_ok)
		return rv;

	if (p->log->verb) {
		char dn[19];		/* device name */
		ss_dnot dno;		/* device number */
		char pn[9];			/* part number */
		unsigned int sn;	/* serial number */
		char sv[13];		/* software release */
		int yp;				/* Year of production (e.g. 1996) */
		int mp;				/* Month of production (1-12) */
		int dp;				/* Day of production (1-31) */
		char devn[19];
		int sn2, sr, hp, np;	/* Date & Time of production */
		int fswl, nosw, dsw;	/* Wavelengths sampled */
		ss_ttt tt;				/* Target Type */

		if ((rv = so_do_DeviceDataRequest(p, dn, &dno, pn, &sn, sv)) != inst_ok)
			return rv;

		if ((rv = so_do_TargetIdRequest(p, devn, &sn2, &sr, &yp, &mp, &dp, &hp, &np,
	                           &tt, &fswl, &nosw, &dsw)) != inst_ok)
			return rv;

		a1logv(p->log, 1,
		       "Device:     %s\n"
		       "Serial No:  %u\n"
		       "Part No:    %s\n"
		       "Prod Date:  %d/%d/%d\n"
		       "SW Version: %s\n", dn, sn, pn, dp, mp, yp, sv);
	}

	/* Set the default colorimetric parameters */
	if ((rv = so_do_ParameterDownload(p, p->dstd, p->wbase, p->illum, p->obsv)) != inst_ok)
		return rv;

	/* Set the capabilities masks */
	ss_determine_capabilities(p);

	/* Deactivate measurement switch */
	if ((rv = so_do_TargetOnOffStDownload(p,ss_toost_Deactivated)) != inst_ok)
		return rv;
	p->trig = inst_opt_trig_user;

	p->inited = 1;
	a1logd(p->log, 2, "ss_init_inst: instrument inited OK\n");

	return inst_ok;
}

/* For an xy instrument, release the paper */
/* Return the inst error code */
static inst_code
ss_xy_sheet_release(
struct _inst *pp) {
	ss *p = (ss *)pp;
	inst_code rv = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->cap2 & inst2_xy_holdrel)
		rv = ss_do_ReleasePaper(p);
	return rv;
}

/* For an xy instrument, hold the paper */
/* Return the inst error code */
static inst_code 
ss_xy_sheet_hold(
struct _inst *pp) {
	ss *p = (ss *)pp;
	inst_code rv = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->cap2 & inst2_xy_holdrel)
		rv = ss_do_HoldPaper(p);
	return rv;
}

/* For an xy instrument, allow the user to locate a point */
/* Return the inst error code */
static inst_code
ss_xy_locate_start(
struct _inst *pp) {
	ss *p = (ss *)pp;
	inst_code rv = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->cap2 & inst2_xy_locate) {
		rv = ss_do_SetDeviceOffline(p);
		p->offline = 1;
	}
	return rv;
}

/* For an xy instrument, position the reading point */
/* Return the inst error code */
static inst_code ss_xy_position(
struct _inst *pp,
int measure,			/* nz if position measure point, z if locate point */
double x, double y
) {
	ss *p = (ss *)pp;
	inst_code rv = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->cap2 & inst2_xy_position)
		if ((rv = ss_do_MoveAbsolut(p, measure ? ss_rt_SensorRef : ss_rt_SightRef, x, y)) != inst_ok)
			return rv;

	return rv;
}

/* For an xy instrument, read back the location */
/* Return the inst error code */
static inst_code
ss_xy_get_location(
struct _inst *pp,
double *x, double *y) {
	ss *p = (ss *)pp;
	inst_code rv = inst_ok;
	ss_rt rr;
	ss_zkt zk;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->cap2 & inst2_xy_position)
		if ((rv = ss_do_OutputActualPosition(p, ss_rt_SightRef, &rr, x, y, &zk)) != inst_ok)
			return rv;

	return rv;
}

/* For an xy instrument, ends allowing the user to locate a point */
/* Return the inst error code */
static inst_code
ss_xy_locate_end(
struct _inst *pp) {
	ss *p = (ss *)pp;
	inst_code rv = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->cap2 & inst2_xy_position) {
		rv = ss_do_SetDeviceOnline(p);
		p->offline = 0;
	}
	return rv;
}

/* For an xy instrument, try and clear the table after an abort */
/* Return the inst error code */
static inst_code
ss_xy_clear(
struct _inst *pp) {
	ss *p = (ss *)pp;
	inst_code rv = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->cap2 & inst2_xy_position) {
		ss_do_SetDeviceOnline(p);	/* Put the device online */
		ss_do_MoveUp(p);			/* Raise the sensor */
		ss_do_ReleasePaper(p);		/* Release the paper */
		ss_do_MoveHome(p);			/* Move to the home position */
	}

	return rv;
}

static inst_code ss_calibrate_imp(ss *p, inst_cal_type *calt, inst_cal_cond *calc, char id[CALIDLEN]);

/* Read a sheet full of patches using xy mode */
/* Return the inst error code */
static inst_code
ss_read_xy(
inst *pp,
int pis,				/* Passes in strip (letters in sheet) */
int sip,				/* Steps in pass (numbers in sheet) */
int npatch,				/* Total patches in strip (skip in last pass) */
char *pname,			/* Starting pass name (' A' to 'ZZ') */
char *sname,			/* Starting step name (' 1' to '99') */
double ox, double oy,	/* Origin of first patch */
double ax, double ay,	/* pass increment */
double aax, double aay,	/* pass offset for odd patches */
double px, double py,	/* step/patch increment */
ipatch *vals) { 		/* Pointer to array of values */
	ss *p = (ss *)pp;
	inst_code rv = inst_ok;
	int pass, step, patch;
	int tries, tc;			/* Total read tries */
	int fstep = 0;			/* NZ if step is fast & quiet direction */

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->itype != instSpectroScan
	 && p->itype != instSpectroScanT)
		return inst_unsupported;

	/* Move quickest in X direction to minimise noise, */
	/* and maximise speed. This means we either increment the step or */
	/* the pass fastest, depending on fstep. */
	if (fabs(px) > fabs(ax)) {
		fstep = 1;
		p->pisrow = 2 * sip;
	} else {
		p->pisrow = 2 * pis;
	}

	tries = sip * pis;		/* Total grid patch count. Not all may have real patches. */

	/* Read all the patches */
	for (step = pass = tc = 0; tc < tries; tc++) {
		int astep, apass;	/* Actual step and pass to use */
		double ix, iy;

		/* Move in serpentine order */
		if (fstep) {
			astep = (pass & 1) ? sip - 1 - step : step;
			apass = pass;
		} else {
			astep = step;
			apass = (step & 1) ? pis - 1 - pass : pass;
		}

		patch = apass * sip + astep;

		if (patch < npatch) {	/* Over a valid patch */
			int try, notries = 5;

			ix = ox + apass * ax + astep * px;
			iy = oy + apass * ay + astep * py;

			if ((step & 1) == 1) {	/* Offset for odd hex patches */
				ix += aax;
				iy += aay;
			}

			/* Retry a reading if it fails. */
			/* (Note that we actually check if retries have failed within the loop) */
			for (try = 0; try < notries; try++) {

				a1logd(p->log, 3, "ss_read_xy: fstep %d, astep/pass %d\n",
				                            fstep, fstep ? astep : apass);
				/* Do calibration if it is absolutely needed, or when it would */
				/* avoid a long move. */
				check_calcount(p, fstep ? (pass & 1) == 0 && step == 0 : (step & 1) == 0 && pass == 0);
				if ( (p->need_wd_cal || p->need_t_cal) && p->noinitcalib == 0) {
					inst_cal_type calt = inst_calt_needed;
					inst_cal_cond calc = inst_calc_none;
					char id[CALIDLEN];

					/* We expect this to be automatic, but handle as if it mightn't be */
					if ((rv = ss_calibrate_imp(p, &calt, &calc, id)) != inst_ok) {
						if (rv == inst_cal_setup)
							return inst_needs_cal;	/* Not automatic, needs a manual setup */
						if (try < notries) {
							p->forcecalib = 1;		/* Force a calibration */
							inc_calcount(p);		/* Set correct type of calib */
							a1logv(p->log, 1, "Calibration failed, retrying...\n");
							continue;	/* Retry */
						}
						return rv;		/* Error */
					}
				}

				{
					ss_rvt refvalid;
					double col[3], spec[36];
					int i;

					vals[patch].loc[0] = '\000';
					vals[patch].mtype = inst_mrt_none;
					vals[patch].XYZ_v = 0;
					vals[patch].sp.spec_n = 0;
					vals[patch].duration = 0.0;
				
					/* move and measure gives us spectrum data anyway */
					if ((rv = ss_do_MoveAndMeasure(p, ix, iy, spec, &refvalid)) != inst_ok) {
						if (try < notries) {
							p->forcecalib = 1;		/* Force a calibration */
							inc_calcount(p);		/* Set correct type of calib */
							a1logv(p->log, 1, "Measurement failed, retrying...\n");
							continue;	/* Retry */
						}
						return rv;		/* Error */
					}

					vals[patch].sp.spec_n = 36;
					vals[patch].sp.spec_wl_short = 380;
					vals[patch].sp.spec_wl_long = 730;
					vals[patch].sp.norm = 100.0;
					
					for (i = 0; i < vals[patch].sp.spec_n; i++)
						vals[patch].sp.spec[i] = 100.0 * (double)spec[i];

					/* Get the XYZ */
					{
						ss_cst rct;
						ss_rvt rvf;
						ss_aft af;
						ss_wbt wb;
						ss_ilt it;
						ss_ot  ot;

						if ((rv = so_do_CParameterRequest(p, ss_cst_XYZ, &rct, col, &rvf,
						     &af, &wb, &it, &ot)) != inst_ok
						  || rvf != ss_rvt_True) {
							if (try < notries) {
								p->forcecalib = 1;		/* Force a calibration */
								inc_calcount(p);		/* Set correct type of calib */
								a1logv(p->log, 1, "XYZ reading failed, retrying...\n");
								continue;	/* Retry */
							}
							return rv;		/* Error */
						}
					}
					vals[patch].XYZ_v = 1;
					vals[patch].XYZ[0] = col[0];
					vals[patch].XYZ[1] = col[1];
					vals[patch].XYZ[2] = col[2];

					/* Track the need for a calibration */
					inc_calcount(p);

					break;			/* Don't need to retry */
				}
			}	/* Retry */
		}

		/* Move on to the next patch */
		if (fstep) {
			if (++step >= sip) {
				step = 0;
				pass++;
			}
		} else {
			if (++pass >= pis) {
				pass = 0;
				step++;
			}
		}
	}

	return rv;
}

/* Read a set of strips */
/* Return the inst error code */
static inst_code
ss_read_strip(
inst *pp,
char *name,			/* Strip name (7 chars) */
int npatch,			/* Number of patches in the pass */
char *pname,		/* Pass name (3 chars) */
int sguide,			/* Guide number */
double pwid,		/* Patch length in mm (For DTP41) */
double gwid,		/* Gap length in mm (For DTP41) */
double twid,		/* Trailer length in mm (For DTP41T) */
ipatch *vals) {		/* Pointer to array of instrument patch values */
//	ss *p = (ss *)pp;
	inst_code rv = inst_unsupported;
	return rv;
}


/* Observer weightings for Spectrolino spectrum, 380 .. 730 nm in 10nm steps */
/* 1931 2 degree/10 degree, X, Y, Z */
/* Derived from the 1mm CIE data by integrating over +/- 5nm */
double obsv[2][3][36] = {
	{
		{
			0.001393497640, 0.004448031900, 0.014518206300, 0.045720800000, 0.138923633000,
			0.279645970000, 0.344841960000, 0.335387990000, 0.288918940000, 0.196038970000,
			0.097089264500, 0.033433134500, 0.006117900200, 0.011512466000, 0.065321232000,
			0.166161125000, 0.291199155000, 0.434290495000, 0.594727005000, 0.761531500000,
			0.914317000000, 1.023460340000, 1.058604000000, 0.999075000000, 0.851037990000,
			0.644076660000, 0.449047000000, 0.285682340000, 0.166610680000, 0.089139475000,
			0.047203532000, 0.023272100000, 0.011556993000, 0.005897781550, 0.002960988050,
			0.001468472565
		},
		{
			0.000040014416, 0.000126320071, 0.000402526680, 0.001272963400, 0.004268400000,
			0.011759799700, 0.023092867000, 0.038306468000, 0.060303866000, 0.091762739000,
			0.139594730000, 0.210065540000, 0.326613130000, 0.504776000000, 0.706552500000,
			0.859214005000, 0.951809665000, 0.993340440000, 0.993019710000, 0.950281660000,
			0.868557660000, 0.756550000000, 0.630964340000, 0.503366340000, 0.380962000000,
			0.266444660000, 0.175871340000, 0.108002605000, 0.061709066000, 0.032657466000,
			0.017165009000, 0.008419183400, 0.004173919800, 0.002129796800, 0.001069267000,
			0.000530292340
		},
		{
			0.006568973000, 0.021026087500, 0.068865635000, 0.218090190000, 0.668415545000,
			1.366703205000, 1.731816230000, 1.769890130000, 1.658588340000, 1.288104470000,
			0.818359800000, 0.471791600000, 0.275824200000, 0.159485840000, 0.080436864500,
			0.042599734500, 0.020750932000, 0.009028633450, 0.004015999900, 0.002160166550,
			0.001629500100, 0.001143333400, 0.000804400000, 0.000372600000, 0.000180700000,
			0.000054966665, 0.000019933332, 0.000002266667, 0.000000000000, 0.000000000000,
			0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000,
			0.000000000000
		}
	},
	{
		{
			0.000221161200, 0.002892312000, 0.021223545000, 0.087243800000, 0.203891450000,
			0.313689500000, 0.379737550000, 0.368800750000, 0.301126300000, 0.194835400000,
			0.082524250000, 0.018557800000, 0.006085000000, 0.039474500000, 0.119180250000,
			0.237142500000, 0.377122750000, 0.531279950000, 0.705108350000, 0.876453800000,
			1.013894200000, 1.113552000000, 1.119829000000, 1.026837000000, 0.855199200000,
			0.646495700000, 0.434312700000, 0.270230400000, 0.154505300000, 0.082548760000,
			0.041674230000, 0.020379080000, 0.009795728000, 0.004661858000, 0.002225776000,
			0.001069074500
		},
		{
			0.000023956650, 0.000309054000, 0.002220395000, 0.009005150000, 0.021600050000,
			0.038992450000, 0.062072600000, 0.089764700000, 0.128515350000, 0.185550850000,
			0.255690850000, 0.342051100000, 0.461805650000, 0.607378100000, 0.759122200000,
			0.874795100000, 0.958787300000, 0.991541600000, 0.995046500000, 0.953158750000,
			0.869616900000, 0.775837500000, 0.657942650000, 0.527902700000, 0.399013000000,
			0.283553200000, 0.181354350000, 0.108672400000, 0.061082550000, 0.032323865000,
			0.016231270000, 0.007919470000, 0.003803046000, 0.001810832500, 0.000865886500,
			0.000416835450
		},
		{
			0.000975787600, 0.012867500000, 0.095777190000, 0.402301600000, 0.971629200000,
			1.549938000000, 1.948388000000, 1.984670000000, 1.739857000000, 1.308151000000,
			0.781796500000, 0.422593900000, 0.222878650000, 0.115174050000, 0.061302800000,
			0.030879300000, 0.013841200000, 0.004144350000, 0.000189450000, 0.000000000000,
			0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000,
			0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000,
			0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000, 0.000000000000,
			0.000000000000
		}
	}
};

/* Read a single sample */
/* Return the dtp error code */
static inst_code
ss_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	ss *p = (ss *)pp;
	inst_code rv = inst_ok;
	int switch_trig = 0;
	int user_trig = 0;
	double col[3], spec[36];
	int i;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (!val)
		return inst_internal_error;

	a1logd(p->log, 4, "ss_read_sample()\n");

	val->loc[0] = '\000';
	val->XYZ_v = 0;
	val->sp.spec_n = 0;
	val->duration = 0.0;

	/* Do calibration if it is needed */
	if ((p->need_wd_cal || p->need_t_cal) && p->noinitcalib == 0) {
		inst_cal_type calt = inst_calt_needed;
		inst_cal_cond calc = inst_calc_none;
		char id[CALIDLEN];

		/* This could be automatic or need manual intervention */
		if ((rv = ss_calibrate_imp(p, &calt, &calc, id)) != inst_ok) {
			if (rv == inst_cal_setup) {
				return inst_needs_cal;	/* Not automatic, needs a manual setup */
			}
			return rv;		/* Error */
		}
	}

	if (p->trig == inst_opt_trig_user_switch) {

		a1logd(p->log, 4, "inst_opt_trig_user_switch\n");

		/* We're assuming that switch trigger won't be selected */
		/* for spot measurement on the spectroscan, so we don't lower the head. */

		/* Activate measurement switch */
		if ((rv = so_do_TargetOnOffStDownload(p,ss_toost_Activated)) != inst_ok) {
			return rv;
		}

		/* Wait for a measurement or for the user to hit a key */
		for (;;) {
			ss_nmt nm;

			/* Query whether a new measurement was performed since the last */
			if ((rv = so_do_NewMeasureRequest(p, &nm)) != inst_ok) {
				return rv;		/* Error */
			}
			if (nm == ss_nmt_NewMeas) {
				switch_trig = 1;
				break;
			}
			/* If SpectroScanT transmission, poll the enter key */
			if (p->itype == instSpectroScanT
			 && (p->mode & inst_mode_illum_mask) == inst_mode_transmission) {
				ss_sks sk;
				ss_ptt pt;
				if ((rv = ss_do_OutputActualKey(p, &sk, &pt)) == inst_ok
				 && sk == ss_sks_EnterKey) {
					user_trig = 1;
					break;					/* Trigger */
				}
			}

			if (p->uicallback != NULL) {	/* Check for user trigger */
				if ((rv = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
					if (rv == inst_user_abort)
						return rv;				/* Abort */
					if (rv == inst_user_trig) {
						user_trig = 1;
						break;					/* Trigger */
					}
				}
			}
		}

		/* Deactivate measurement switch */
		if ((rv = so_do_TargetOnOffStDownload(p,ss_toost_Deactivated)) != inst_ok)
			return rv;

		/* Notify of trigger */
		if (p->uicallback)
			p->uicallback(p->uic_cntx, inst_triggered); 

	} else if (p->trig == inst_opt_trig_user) {

		a1logd(p->log, 4, "inst_opt_trig_user\n");

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "ss: inst_opt_trig_user but no uicallback function set!\n");
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
		a1logd(p->log, 4, "program trigger\n");

		/* Check for abort */
		if (p->uicallback != NULL
		 && (rv = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort)
			return rv;				/* Abort */
	}

	/* Trigger a read if the switch has not been used */
	if (switch_trig == 0) {
		ss_nmt nm;

		a1logd(p->log, 4, "starting a read because switch wasn't used\n");

		/* For the spectroscan, make sure the instrument is on line, */
		/* since it may be off line to allow the user to position it. */
		if (p->itype != instSpectrolino && p->offline) {
			if ((rv = p->xy_locate_end((inst *)p)) != inst_ok)
				return rv;
		}

		/* For reflection spot mode on a SpectroScan, lower the head. */
		/* (A SpectroScanT in transmission will position automatically) */
		if (p->itype != instSpectrolino
		 && (p->mode & inst_mode_illum_mask) != inst_mode_transmission) {
			if ((rv = ss_do_MoveDown(p)) != inst_ok)
					return rv;
		}

		/* trigger it in software */
		if ((rv = so_do_ExecMeasurement(p)) != inst_ok)
			return rv;
		/* Query measurement to reset count */
		if ((rv = so_do_NewMeasureRequest(p, &nm)) != inst_ok)
			return rv;

		/* For reflection spot mode on a SpectroScan, raise the head. */
		/* (A SpectroScanT in transmission will position automatically) */
		if (p->itype != instSpectrolino
		 && (p->mode & inst_mode_illum_mask) != inst_mode_transmission) {
			if ((rv = ss_do_MoveUp(p)) != inst_ok)
					return rv;
		}
	}

	/* Track the need for a calibration */
	inc_calcount(p);

	/* Get the XYZ: */

	/* Emulated spot transmission mode: */
	if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission
	 && p->itype == instSpectrolino) {
		ss_st rst;		/* Return Spectrum Type (Reflectance/Density) */
		ss_rvt rvf;		/* Return Reference Valid Flag */
		ss_aft af;		/* Return filter being used (None/Pol/D65/UV/custom */
		ss_wbt wb;		/* Return white base (Paper/Absolute) */
		double norm;	/* Y normalisation factor */
		int j, tix = 0;	/* Default 2 degree */

		/* Get the spectrum */
		if ((rv = so_do_SpecParameterRequest(p, ss_st_LinearSpectrum,
		          &rst, spec, &rvf, &af, &wb)) != inst_ok)
			return rv;

		/* Divide by transmission white reference to get transmission level. */
		for (i = 0; i < 36; i++) {
			if (p->tref[i] >= 0.0001)
				spec[i] = spec[i]/p->tref[i];
			else
				spec[i] = 0.0;
		}

		if (p->mode & inst_mode_spectral) {
			val->sp.spec_n = 36;
			val->sp.spec_wl_short = 380;
			val->sp.spec_wl_long = 730;
			val->sp.norm = 100.0;
			for (i = 0; i < val->sp.spec_n; i++)
				val->sp.spec[i] = 100.0 * (double)spec[i];
		}

		/* Convert to desired illuminant XYZ */
		val->XYZ[0] = 0.0;
		val->XYZ[1] = 0.0;
		val->XYZ[2] = 0.0;
		if (p->obsv == ss_ot_TenDeg)
			tix = 1;

		/* Compute normalisation factor */
		for (norm = 0.0, i = 0; i < 36; i++) {
			if (p->tref[i] >= 0.0001)
				norm += obsv[tix][1][i] * p->cill[i];
		}
		norm = 100.0/norm;
		
		/* Compute XYZ */
		for (i = 0; i < 36; i++) {
			if (p->tref[i] >= 0.0001) {
				for (j = 0; j < 3; j++)
					val->XYZ[j] += obsv[tix][j][i] * p->cill[i] * spec[i];
			}
		}
		for (j = 0; j < 3; j++)
			val->XYZ[j] *= norm;

		/* This may not change anything since instrument may clamp */
		if (clamp)
			icmClamp3(val->XYZ, val->XYZ);

		val->XYZ_v = 1;
		val->mtype = inst_mrt_transmissive;
	 

	/* Using filter compensation */
	/* This isn't applicable to emulated transmission mode, because */
	/* the filter will be calibrated out in the illuminant measurement. */
	} else if (p->compen != 0) {
		ss_cst rct;
		ss_st rst;		/* Return Spectrum Type (Reflectance/Density) */
		ss_rvt rvf;		/* Return Reference Valid Flag */
		ss_aft af;		/* Return filter being used (None/Pol/D65/UV/custom */
		ss_ilt it;
		ss_ot  ot;
		ss_wbt wb;		/* Return white base (Paper/Absolute) */
#ifdef NEVER
		double norm;	/* Y normalisation factor */
#endif
		double XYZ[3];
		int j, tix = 0;	/* Default 2 degree */

		/* Get the XYZ */
		if ((rv = so_do_CParameterRequest(p, ss_cst_XYZ, &rct, col, &rvf,
		     &af, &wb, &it, &ot)) != inst_ok)
			return rv;

		/* Get the spectrum */
		if ((rv = so_do_SpecParameterRequest(p, ss_st_LinearSpectrum,
		          &rst, spec, &rvf, &af, &wb)) != inst_ok)
			return rv;

		/* Multiply by the filter compensation values to do correction */
		for (i = 0; i < 36; i++) {
			spec[i] *= p->comp[i];
		}

		/* Return the results */
		if (p->mode & inst_mode_spectral) {
			val->sp.spec_n = 36;
			val->sp.spec_wl_short = 380;
			val->sp.spec_wl_long = 730;
			if ((p->mode & inst_mode_illum_mask) == inst_mode_emission) {
				val->sp.norm = 1.0;
				for (i = 0; i < val->sp.spec_n; i++)
					val->sp.spec[i] = (double)spec[i];
			} else {
				val->sp.norm = 100.0;
				for (i = 0; i < val->sp.spec_n; i++)
					val->sp.spec[i] = 100.0 * (double)spec[i];
			}
		}

		/* Convert to desired illuminant XYZ */
		if (p->obsv == ss_ot_TenDeg)
			tix = 1;

		/* Compute XYZ */
		for (j = 0; j < 3; j++)
			XYZ[j] = 0.0;
		for (i = 0; i < 36; i++)
			for (j = 0; j < 3; j++)
				XYZ[j] += obsv[tix][j][i] * spec[i];

		/* This may not change anything since instrument may clamp */
		if (clamp)
			icmClamp3(XYZ, XYZ);

		if ((p->mode & inst_mode_illum_mask) == inst_mode_emission) {
			/* Emission XYZ seems to be Luminous Watts, so */
			/* convert to cd/m^2 */
			val->XYZ_v = 1;
			val->XYZ[0] = XYZ[0] * 683.002;
			val->XYZ[1] = XYZ[1] * 683.002;
			val->XYZ[2] = XYZ[2] * 683.002;
			val->mtype = inst_mrt_emission;
		} else {
			val->XYZ_v = 1;
			val->XYZ[0] = XYZ[0];
			val->XYZ[1] = XYZ[1];
			val->XYZ[2] = XYZ[2];
			if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission)
				val->mtype = inst_mrt_transmissive;
			else
				val->mtype = inst_mrt_reflective;
		}

	/* Normal instrument values */
	} else {
		ss_cst rct;
		ss_rvt rvf;
		ss_aft af;
		ss_wbt wb;
		ss_ilt it;
		ss_ot  ot;

		if ((rv = so_do_CParameterRequest(p, ss_cst_XYZ, &rct, col, &rvf,
		     &af, &wb, &it, &ot)) != inst_ok)
			return rv;

		/* This may not change anything since instrument may clamp */
		if (clamp)
			icmClamp3(col, col);

		if ((p->mode & inst_mode_illum_mask) == inst_mode_emission) {
			val->XYZ_v = 1;
			val->XYZ[0] = col[0];
			val->XYZ[1] = col[1];
			val->XYZ[2] = col[2];
			val->mtype = inst_mrt_emission;
		} else {
			val->XYZ_v = 1;
			val->XYZ[0] = col[0];
			val->XYZ[1] = col[1];
			val->XYZ[2] = col[2];
			if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission)
				val->mtype = inst_mrt_transmissive;
			else
				val->mtype = inst_mrt_reflective;
		}

		/* spectrum data is returned only if requested */
		if (p->mode & inst_mode_spectral) {
			ss_st rst;		/* Return Spectrum Type (Reflectance/Density) */
			ss_rvt rvf;		/* Return Reference Valid Flag */
			ss_aft af;		/* Return filter being used (None/Pol/D65/UV/custom */
			ss_wbt wb;		/* Return white base (Paper/Absolute) */
	
			if ((rv = so_do_SpecParameterRequest(p, ss_st_LinearSpectrum,
			          &rst, spec, &rvf, &af, &wb)) != inst_ok)
				return rv;
	 
			val->sp.spec_n = 36;
			val->sp.spec_wl_short = 380;
			val->sp.spec_wl_long = 730;
			if ((p->mode & inst_mode_illum_mask) == inst_mode_emission) {
				/* Spectral data is in mW/nm/m^2 interpolated to 10nm spacing */
				val->sp.norm = 1.0;
				for (i = 0; i < val->sp.spec_n; i++)
					val->sp.spec[i] = (double)spec[i];
			} else {
				val->sp.norm = 100.0;
				for (i = 0; i < val->sp.spec_n; i++)
					val->sp.spec[i] = 100.0 * (double)spec[i];
			}
		}
	}
	if (user_trig)
		return inst_user_trig;
	return rv;
}

/* Return needed and available inst_cal_type's */
static inst_code ss_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	ss *p = (ss *)pp;
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
		
	if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission) {
		if (p->itype == instSpectrolino) {		/* Emulated transmission */
			if (p->need_wd_cal && p->noinitcalib == 0)
				n_cals |= inst_calt_ref_white;
			a_cals |= inst_calt_ref_white;
			if (p->need_t_cal)					/* We aren't saving trans white ref */
				n_cals |= inst_calt_trans_vwhite;
			a_cals |= inst_calt_trans_vwhite;
		} else {	/* SpectroscanT */
			if (p->need_wd_cal && p->noinitcalib == 0)
				n_cals |= inst_calt_ref_white;
			a_cals |= inst_calt_ref_white;
			if (p->need_t_cal && p->noinitcalib == 0)
				n_cals |= inst_calt_trans_white;
			a_cals |= inst_calt_trans_white;
		}

	} else {
		if (p->need_wd_cal && p->noinitcalib == 0)
			n_cals |= inst_calt_ref_white;
		a_cals |= inst_calt_ref_white;
	}

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	return inst_ok;
}

/* Perform an instrument calibration (implementation). */
static inst_code ss_calibrate_imp(
ss *p,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	inst_code rv = inst_ok;
	ss_aft afilt;	/* Actual filter */
	double sp[36];	/* Spectral values of filter */
	ss_owrt owr;	/* Original white reference */
	inst_cal_type needed, available;

	id[0] = '\000';

	a1logd(p->log, 3, "ss calibrate called with calt = 0x%x, condition 0x%x, need w %d, t %d\n", *calt, *calc, p->need_wd_cal, p->need_t_cal);

	if ((rv = ss_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
		return rv;

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

		a1logd(p->log,4,"ss_imp_calibrate: doing calt 0x%x\n",*calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return inst_ok;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return inst_unsupported;
	}

	/* There are different procedures depending on the intended mode, */
	/* whether this is a Spectrolino or SpectrScan, and whether just a white, */
	/* or a transmission calibration are needed or both. */

	/* All first time calibrations do an initial reflective white calibration. */
	if (*calt & inst_calt_ref_white) {

		a1logd(p->log, 3, "ss cal dealing with being on ref_white\n");

		if ((p->mode & inst_mode_illum_mask) == inst_mode_emission)
			p->filt = ss_aft_NoFilter;	/* Need no filter for emission */
	
		/* Set mode to reflection as a default for calibration */
		if (p->itype == instSpectroScanT) {
			if ((rv = ss_do_SetTableMode(p, ss_tmt_Reflectance)) != inst_ok)
				return rv;
		} else {
			if ((rv = so_do_MeasControlDownload(p, ss_ctt_RemissionMeas)) != inst_ok)
				return rv;
		}
	
		/* Set the desired colorimetric parameters + absolute white base */
		if ((rv = so_do_ParameterDownload(p, p->dstd, ss_wbt_Abs, p->illum, p->obsv)) != inst_ok)
			return rv;

		/* Get the name of the expected white reference */
		if ((rv = so_do_WhiteReferenceRequest(p, p->filt, &afilt, sp, &owr, id)) != inst_ok)
			return rv;

		if (p->noinitcalib == 0) {

			/* Make sure we're in a condition to do the calibration */
			if (p->itype == instSpectrolino && *calc != inst_calc_man_ref_white) {
				*calc = inst_calc_man_ref_white;
				a1logd(p->log, 3, "ss cal need cond. inst_calc_man_ref_white and haven't got it\n");
				return inst_cal_setup;
			}

			/* Do the white calibration */
			for (;;) {		/* Untill everything is OK */

				a1logd(p->log, 3, "ss cal doing white reflective cal\n");
				/* For SpectroScan, move to the white reference in slot 1 and lower */
				if (p->itype != instSpectrolino) {
					if ((rv = ss_do_MoveToWhiteRefPos(p, ss_wrpt_RefTile1)) != inst_ok)
						return rv;
					if ((rv = ss_do_MoveDown(p)) != inst_ok)
						return rv;
				}

				/* Calibrate */
				if ((rv = so_do_ExecRefMeasurement(p, ss_mmt_WhiteCalWithWarn))
				                          != (inst_notify | ss_et_WhiteMeasOK))
					return rv;
				rv = inst_ok;

				/* For SpectroScan, raise */
				if (p->itype != instSpectrolino) {
					if ((rv = ss_do_MoveUp(p)) != inst_ok)
						return rv;
				}

				/* Verify the filter. */
				{
					ss_dst ds;
					ss_wbt wb;
					ss_ilt it;
					ss_ot  ot;
					ss_aft af;

					if ((rv = so_do_ParameterRequest(p, &ds, &wb, &it, &ot, &af)) != inst_ok)
						return rv;
					if (af == p->filt)
						break;

					a1logd(p->log, 3, "got filt %d, want %d\n",af,p->filt);

					strcpy(id, filter_desc[p->filt]);
					*calc = inst_calc_change_filter;
					return inst_cal_setup;
				}
			}
			a1logd(p->log, 3, "reflection calibration and filter verify is complete\n");

			/* Emission or spot transmission mode, dark calibration. */
			if ((p->mode & inst_mode_illum_mask) == inst_mode_emission
			 || ((p->mode & inst_mode_illum_mask) == inst_mode_transmission
			     && p->itype == instSpectrolino)) {
					a1logd(p->log, 3, "emmission/transmission dark calibration:\n");
					/* Set emission mode */
					if ((rv = so_do_MeasControlDownload(p, ss_ctt_EmissionMeas)) != inst_ok)
						return rv;
					if (p->noinitcalib == 0) {
						/* Do dark calibration (Assume we're still on white reference) */
						if ((rv = so_do_ExecRefMeasurement(p, ss_mmt_EmissionCal))
						                    != (inst_notify | ss_et_EmissionCalOK))
							return rv;
					}
					rv = inst_ok;
					a1logd(p->log, 3, "emmission/transmisson dark calibration done\n");
			}

			p->calcount = 0;
			p->need_wd_cal = 0;
		}

		/* Restore the instrument to the desired mode */
		/* SpectroScanT - Transmission mode, set transmission mode. */
		if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission
		     && p->itype == instSpectroScanT) {

			if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission) {
				if ((rv = ss_do_SetTableMode(p, ss_tmt_Transmission)) != inst_ok)
					return rv;
			}
		}
		*calt &= ~inst_calt_ref_white;
	}

	/* ??? If White Base Type is not Absolute, where is Paper type set, */
	/* and how is it calibrated ????? */

	/* For non-reflective measurement, do the recalibration or 2nd part of calibration. */

	/* Transmission mode calibration: */
	if ((*calt & (inst_calt_trans_vwhite | inst_calt_trans_white))
	 && (p->mode & inst_mode_illum_mask) == inst_mode_transmission) {

		a1logd(p->log, 3, "ss cal need trans with calt = trans_white\n");
		/* Emulated spot transmission using spectrolino */
		if ((*calt & inst_calt_trans_vwhite) && p->itype == instSpectrolino) {
			ss_st rst;		/* Return Spectrum Type (Reflectance/Density) */
			ss_rvt rvf;		/* Return Reference Valid Flag */
			ss_aft af;		/* Return filter being used (None/Pol/D65/UV/custom */
			ss_wbt wb;		/* Return white base (Paper/Absolute) */
			ss_ilt it;		/* Return illuminant type */
			int i;

			a1logd(p->log, 3, "ss cal need trans, spectrolino\n");
			/* Make sure we're in a condition to do the calibration */
			if (*calc != inst_calc_man_trans_white) {
				*calc = inst_calc_man_trans_white;
				a1logd(p->log, 3, "ss cal need cond. inst_calc_man_trans_white and haven't got it\n");
				return inst_cal_setup;
			}

			/* Measure white reference spectrum */
			if ((rv = so_do_ExecMeasurement(p)) != inst_ok)
				return rv;
			if ((rv = so_do_SpecParameterRequest(p, ss_st_LinearSpectrum,
			          &rst, p->tref, &rvf, &af, &wb)) != inst_ok)
				return rv;

			so_do_NewMeasureRequest(p, NULL);		/* flush pending measurement */

			/* See how good a source it is */
			for (i = 0; i < 36; i++)  {
				if (p->tref[i] < 0.0001)
					break;
			}

			if (i < 36) {
				*calc = inst_calc_message;
				strcpy(id, "Warning: Transmission light source is low at some wavelengths!");
				rv = inst_ok;
			}

			/* Get the instrument illuminant */
			if ((rv = so_do_IllumTabRequest(p, p->illum, &it, p->cill)) != inst_ok)
				return rv; 

			p->calcount = 0;
		 	p->need_t_cal = 0;
			a1logd(p->log, 3, "transmission lino cal done\n");

		/* SpectroScanT */
		} else if ((*calt & inst_calt_trans_white) && p->itype != instSpectrolino) {
			/* Hmm. Should we really return a message and resume somehow, */
			/* rather than communicating directly with the user... */

			a1logd(p->log, 3, "transmission scan cal being done\n");
#ifdef NEVER
			/* Advise user to change aperture before switching to transmission mode. */
			p->message_user((inst *)p, "\nEnsure that desired transmission aperture is fitted,\n"
			                           "and press space to continue:\n");
			p->poll_user((inst *)p, 1);
#else
			a1logv(p->log,1,"It is assumed that the desired transmission aperture is fitted\n");
#endif
			/* Presuming this is the right return code */
			if ((rv = so_do_ExecRefMeasurement(p, ss_mmt_WhiteCalWithWarn))
			                          != (inst_notify | ss_et_WhiteMeasOK))
				return rv;
			rv = inst_ok;
			p->calcount = 0;
		 	p->need_t_cal = 0;
			a1logd(p->log, 3, "transmission scan cal done\n");
		}
		*calt &= ~(inst_calt_trans_vwhite | inst_calt_trans_white);
	}

	a1logd(p->log, 3, "calibration completed\n");
	return rv;
}

/* Request an instrument calibration. */
/* This is use if the user decides they want to do a calibration, */
/* in anticipation of a calibration (needs_calibration()) to avoid */
/* requiring one during measurement, or in response to measuring */
/* returning inst_needs_cal. Initially use an inst_cal_cond of inst_calc_none, */
/* and then be prepared to setup the right conditions, or ask the */
/* user to do so, each time the error inst_cal_setup is returned. */
inst_code ss_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	ss *p = (ss *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	return ss_calibrate_imp(p, calt, calc, id);
}

/* Insert a compensation filter in the instrument readings */
/* This is typically needed if an adapter is being used, that alters */
/* the spectrum of the light reaching the instrument */
/* To remove the filter, pass NULL for the filter filename */
inst_code ss_comp_filter(
struct _inst *pp,
char *filtername
) {
	ss *p = (ss *)pp;
	
	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

#ifndef SALONEINSTLIB
	if (filtername == NULL) {
		p->compen = 0;
	} else {
		xspect sp;
		int i;
		if (read_xspect(&sp, filtername) != 0) {
			return inst_wrong_setup;
		}
		if (sp.spec_n != 36 || sp.spec_wl_short != 380.0 || sp.spec_wl_long != 730.0) {
			return inst_wrong_setup;
		}
		for (i = 0; i < 36; i++)
			p->comp[i] = sp.spec[i];
		p->compen = 1;
	}
	return inst_ok;
#else /* SALONEINSTLIB */
	return inst_unsupported; 
#endif /* SALONEINSTLIB */
}

/* Instrument specific error codes interpretation */
static char *
ss_interp_error(inst *pp, int ec) {
//	ss *p = (ss *)pp;
	ec &= inst_imask;

	switch (ec) {

		/* Device errors */
		case ss_et_NoError:
			return "No device error";
		case ss_et_MemoryFailure:
			return "Memory failure";
		case ss_et_PowerFailure:
			return "Power failure";
		case ss_et_LampFailure:
			return "Lamp failure";
		case ss_et_HardwareFailure:
			return "Hardware failure";
		case ss_et_FilterOutOfPos:
			return "Filter wheel out of position";
		case ss_et_SendTimeout:
			return "Data transmission timout";
		case ss_et_DriveError:
			return "Data drive defect";
		case ss_et_MeasDisabled:
			return "Measuring disabled";
		case ss_et_DensCalError:
			return "Incorrect input during densitometric calibration";
		case ss_et_EPROMFailure:
			return "Defective EPROM";
		case ss_et_RemOverFlow:
			return "Too much light or wrong white calibration";
		case ss_et_MemoryError:
			return "Checksum error in memory";
		case ss_et_FullMemory:
			return "Memory is full";
		case ss_et_WhiteMeasOK:
			return "White measurement is OK";
		case ss_et_NotReady:
			return "Instrument is not ready - please wait";
		case ss_et_WhiteMeasWarn:
			return "White measurement warning";
		case ss_et_ResetDone:
			return "Reset is done";
		case ss_et_EmissionCalOK:
			return "Emission calibration is OK";
		case ss_et_OnlyEmission:
			return "Only for emission (not reflection)";
		case ss_et_CheckSumWrong:
			return "Wrong checksum";
		case ss_et_NoValidMeas:
			return "No valid measurement (e.g. no white measurement)";
		case ss_et_BackupError:
			return "Error in backing up values";
		case ss_et_ProgramROMError:
			return "Errors in programming ROM";

		/* Incorporate remote error set codes thus: */
		case ss_et_NoValidDStd:
			return "No valid Density standard set";
		case ss_et_NoValidWhite:
			return "No valid White standard set";
		case ss_et_NoValidIllum:
			return "No valid Illumination set";
		case ss_et_NoValidObserver:
			return "No valid Observer set";
		case ss_et_NoValidMaxLambda:
			return "No valid maximum Lambda set";
		case ss_et_NoValidSpect:
			return "No valid spectrum";
		case ss_et_NoValidColSysOrIndex:
			return "No valid color system or index";
		case ss_et_NoValidChar:
			return "No valid character";
		case ss_et_DorlOutOfRange:
			return "Density is out of range";
		case ss_et_ReflectanceOutOfRange:
			return "Reflectance is out of range";
		case ss_et_Color1OutOfRange:
			return "Color 1 is out of range";
		case ss_et_Color2OutOfRange:
			return "Color 2 is out of range";
		case ss_et_Color3OutOfRange:
			return "Color 3 is out of range";
		case ss_et_NotAnSROrBoolean:
			return "Not an SR or Boolean";
		case ss_et_NoValidValOrRef:
			return "No valid value or reference";

		/* Translated scan error codes thus: */
		case ss_et_DeviceIsOffline:
			return "Device has been set offline";
		case ss_et_OutOfRange:
			return "A parameter of the command is out of range";
		case ss_et_ProgrammingError:
			return "Error writing to Flash-EPROM";
		case ss_et_NoUserAccess:
			return "No access to internal function";
		case ss_et_NoValidCommand:
			return "Unknown command sent";
		case ss_et_NoDeviceFound:
			return "Spectrolino can't be found";
		case ss_et_MeasurementError:
			return "Measurement error";
		case ss_et_NoTransmTable:
			return "SpectroScanT command when no tansmission table";
		case ss_et_NotInTransmMode:
			return "SpectroScanT transmission command in reflection mode";
		case ss_et_NotInReflectMode:
			return "SpectroScanT reflection command in transmission mode";

		/* Translated device communication errors */
		case ss_et_StopButNoStart:
			return "No start character received by instrument";
		case ss_et_IllegalCharInRec:
			return "Invalid character received by instrument";
		case ss_et_IncorrectRecLen:
			return "Record length received by instrument incorrect";
		case ss_et_IllegalRecType:
			return "Invalid message number receivec by instrument";
		case ss_et_NoTagField:
			return "No message number received by instrument";
		case ss_et_ConvError:
			return "Received data couldn't be converted by instrument";
		case ss_et_InvalidForEmission:
			return "Invalid message number for emission instrument";
		case ss_et_NoAccess:
			return "Failure in user identification by instrument";

		/* Our own communication errors here too. */
		case ss_et_SerialFail:
			return "Serial communications failure";

		case ss_et_SendBufferFull:
			return "Message send buffer is full";
		case ss_et_RecBufferEmpty:
			return "Message receive buffer is full";
		case ss_et_BadAnsFormat:
			return "Message received from instrument is badly formatted";
		case ss_et_BadHexEncoding:
			return "Message received from instrument has bad Hex encoding";
		case ss_et_RecBufferOverun:
			return "Message received from instrument would overflow recieve buffer";
		default:
			return "Unknown error code";
	}
}

/* Return the instrument mode capabilities */
void ss_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	ss *p = (ss *)pp;

	if (p->cap == inst_mode_none)
		ss_determine_capabilities(p);

	if (pcap1 != NULL)
		*pcap1 = p->cap;
	if (pcap2 != NULL)
		*pcap2 = p->cap2;
	if (pcap3 != NULL)
		*pcap3 = p->cap3;
}

/* 
 * Check measurement mode
 * We assume that the instrument has been initialised.
 */
static inst_code
ss_check_mode(inst *pp, inst_mode m) {
	ss *p = (ss *)pp;
	inst_mode cap;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	pp->capabilities(pp, &cap, NULL, NULL);

	a1logd(p->log, 4, "check_mode 0x%x with cap 0x%x\n",m, cap);

	/* Simple test */
	if (m & ~cap)
		return inst_unsupported;

	/* Check specific modes */
	if (!IMODETST(m, inst_mode_ref_spot)
	 && !IMODETST(m, inst_mode_emis_spot)
	 && !IMODETST(m, inst_mode_emis_tele)
	 && !IMODETST2(cap, m, inst_mode_ref_xy)
	 && !IMODETST2(cap, m, inst_mode_trans_spot)) {		/* Fake or SpectroScanT */
		return inst_unsupported;
	}

	return inst_ok;
}

/* 
 * set measurement mode
 * We assume that the instrument has been initialised.
 * The measurement mode is activated.
 */
static inst_code
ss_set_mode(inst *pp, inst_mode m) {
	ss *p = (ss *)pp;
	inst_code ev;

	if ((ev = ss_check_mode(pp, m)) != inst_ok)
		return ev;

	p->nextmode = m;

	/* Now activate the next mode */
	if (((p->nextmode & inst_mode_illum_mask) == inst_mode_reflection
	  && (p->mode     & inst_mode_illum_mask) != inst_mode_reflection)
	 || ((p->nextmode & inst_mode_illum_mask) == inst_mode_emission
	  && (p->mode     & inst_mode_illum_mask) != inst_mode_emission)
	 || ((p->nextmode & inst_mode_illum_mask) == inst_mode_transmission
	  && (p->mode     & inst_mode_illum_mask) != inst_mode_transmission)) {

		/* Mode has changed */
		p->mode = p->nextmode;

		/* Capabilities may have changed */
		ss_determine_capabilities(p);

		/* So we need calibration (do we, if this mode has been calibrated before ?) */
		p->need_wd_cal = 1;
		if ((p->mode & inst_mode_illum_mask) == inst_mode_transmission) {
			p->need_t_cal = 1;
		}
	}
	return inst_ok;
}

/* 
 * Get a status or get or set an option
 * Since there is no interaction with the instrument,
 * was assume that all of these can be done before initialisation.
 */
static inst_code
ss_get_set_opt(inst *pp, inst_opt_type m, ...) {
	ss *p = (ss *)pp;

	if (m == inst_opt_noinitcalib) {
		va_list args;
		int losecs = 0;

		va_start(args, m);
		losecs = va_arg(args, int);
		va_end(args);

		/* Note that we're not implementing support for losecs at the moment. */
		/* We would have to save a dummy calibration file to track the last time */
		/* the instrument was opened. */

		if (losecs == 0)		/* If losecs != 0, then don't disable init calib */
			p->noinitcalib = 1;

		return inst_ok;

	} else if (m == inst_opt_initcalib) {
		p->noinitcalib = 0;
		return inst_ok;

	} else if (m == inst_opt_set_filter) {
		va_list args;
		inst_opt_filter fe = inst_opt_filter_unknown;

		va_start(args, m);

		fe = va_arg(args, inst_opt_filter);

		va_end(args);

		switch(fe) {
		
			case inst_opt_filter_none:
				p->filt = ss_aft_NoFilter;
				return inst_ok;
			case inst_opt_filter_pol:
				p->filt = ss_aft_PolFilter;
				return inst_ok;
			case inst_opt_filter_D65:
				p->filt = ss_aft_D65Filter;
				return inst_ok;
			case inst_opt_filter_UVCut:
				p->filt = ss_aft_UVCutFilter;
				return inst_ok;
			case inst_opt_filter_Custom:
				p->filt = ss_aft_CustomFilter;
				return inst_ok;
			default:
				break;
		}
		return inst_unsupported;
	}

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

	/* Return the filter */
	if (m == inst_stat_get_filter) {
		inst_opt_filter *filt;
		inst_code rv;
		va_list args;
		ss_dst ds;
		ss_wbt wb;
		ss_ilt it;
		ss_ot  ot;
		ss_aft af;

		va_start(args, m);
		filt = va_arg(args, inst_opt_filter *);
		va_end(args);

		/* Get the filter. */
		if ((rv = so_do_ParameterRequest(p, &ds, &wb, &it, &ot, &af)) != inst_ok)
			return rv;

		switch (af) {
			case ss_aft_NoFilter:
				*filt = inst_opt_filter_none;
				break;
			case ss_aft_PolFilter:
				*filt = inst_opt_filter_pol;
				break;
			case ss_aft_D65Filter:
				*filt = inst_opt_filter_D65 ;
				break;
			case ss_aft_UVCutFilter:
				*filt = inst_opt_filter_UVCut;
				break;
			case ss_aft_CustomFilter:
				*filt = inst_opt_filter_Custom;
				break;
			default:
				*filt = inst_opt_filter_unknown;
				break;
		}
		return inst_ok;
	}

	/* Use default implementation of other inst_opt_type's */
	{
		va_list args;
		inst_code rv;

		va_start(args, m);
		rv = inst_get_set_opt_def(pp, m, args);
		va_end(args);

		return rv;
	}
}

/* Destroy ourselves */
static void
ss_del(inst *pp) {
	ss *p = (ss *)pp;

	if (p->inited
	 && p->itype == instSpectroScanT
	 && (p->mode & inst_mode_illum_mask) == inst_mode_transmission) {
		ss_do_SetDeviceOnline(p);
		ss_do_SetTableMode(p, ss_tmt_Reflectance);
		ss_do_MoveUp(p);			/* Raise the sensor */
		ss_do_ReleasePaper(p);		/* Release the paper */
		ss_do_MoveHome(p);			/* Move to the home position */
	}

	if (p->inited
	 && (p->mode & inst_mode_illum_mask) != inst_mode_transmission) {
		ss_xy_clear(pp);
	}

	if (p->icom != NULL)
		p->icom->del(p->icom);
	free (p);
}

/* Constructor */
extern ss *new_ss(icoms *icom, instType itype) {
	ss *p;
	if ((p = (ss *)calloc(sizeof(ss),1)) == NULL) {
		a1loge(icom->log, 1, "new_ss: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	/* Init public methods */
	p->init_coms    	= ss_init_coms;
	p->init_inst    	= ss_init_inst;
	p->capabilities 	= ss_capabilities;
	p->check_mode       = ss_check_mode;
	p->set_mode     	= ss_set_mode;
	p->get_set_opt     	= ss_get_set_opt;
	p->xy_sheet_release = ss_xy_sheet_release;
	p->xy_sheet_hold    = ss_xy_sheet_hold;
	p->xy_locate_start  = ss_xy_locate_start;
	p->xy_get_location  = ss_xy_get_location;
	p->xy_locate_end	= ss_xy_locate_end;
	p->xy_position		= ss_xy_position;
	p->xy_clear     	= ss_xy_clear;
	p->read_xy      	= ss_read_xy;
	p->read_strip   	= ss_read_strip;
	p->read_sample 	    = ss_read_sample;
	p->get_n_a_cals     = ss_get_n_a_cals;
	p->calibrate    	= ss_calibrate;
	p->comp_filter    	= ss_comp_filter;
	p->interp_error 	= ss_interp_error;
	p->del          	= ss_del;

	/* Init state */
	p->icom = icom;
	p->itype = icom->itype;
	p->cap = inst_mode_none;				/* Unknown until initialised */
	p->mode = inst_mode_none;				/* Not in a known mode yet */
	p->nextmode = inst_mode_none;			/* Not in a known mode yet */

	/* Set default measurement configuration */
	p->filt = ss_aft_NoFilter;
	p->dstd = ss_dst_ANSIT;
	p->illum = ss_ilt_D50;
	p->obsv = ss_ot_TwoDeg;
	p->wbase = ss_wbt_Abs;
	p->phmode = ss_ctt_PhotometricAbsolute;
	p->phref = 1.0;

	/* Init serialisation stuff */
	p->sbuf  = &p->_sbuf[0];
	p->sbufe = &p->_sbuf[SS_MAX_WR_SIZE-2];		/* Allow one byte for nul */
	p->rbuf  = &p->_rbuf[0];
	p->rbufe = &p->_rbuf[0];					/* Initialy empty */
	p->snerr = ss_et_NoError;					/* COMs error */

#ifdef EMSST
	p->tmode = 0;			/* Reflection mode */
	p->sbr = ss_rt_SensorRef;
	p->sbx = 100.0;
	p->sby = 200.0;
#endif

	ss_determine_capabilities(p);

	return p;
}
