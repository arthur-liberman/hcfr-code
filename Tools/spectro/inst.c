
 /* Abstract instrument class implemenation */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   10/3/2001
 *
 * Copyright 2001 - 2010 Graeme W. Gill
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
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
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
#include "usbio.h"
#include "hidio.h"
#include "inst.h"
#include "insttypeinst.h"

#ifdef ENABLE_SERIAL
static instType ser_inst_type(icoms *p, int port);
#endif /* ENABLE_SERIAL */

#undef DEBUG

#define dbgo stderr

#ifdef DEBUG
#define DBG(xxx) fprintf xxx ;
#else
#define DBG(xxx) if (p != NULL && p->debug) fprintf xxx ;
#endif

/* ------------------------------------ */
/* Default methods for instrument class */

/* Establish communications at the indicated baud rate. */
/* Timout in to seconds, and return non-zero error code */
static inst_code init_coms(
inst *p,
int comport,		/* Serial port number */
baud_rate br,		/* Baud rate */
flow_control fc,	/* Flow control */
double tout) {		/* Timeout */
	return inst_unsupported;
}

/* Initialise or re-initialise the INST */
/* return non-zero on an error, with inst error code */
static inst_code init_inst(  
inst *p) {
	return inst_unsupported;
}

/* Return the instrument type */
static instType get_itype(inst *p) {
	if (p != NULL)
	return p->itype;
	return instUnknown;
}

/* Return the instrument serial number. */
/* (This will be an empty string if there is no serial no) */
static char *get_serial_no(inst *p) {
	return "";
}

/* Return the instrument capabilities */
static inst_capability capabilities(inst *p) {
	return inst_unknown;
}

static inst2_capability capabilities2(inst *p) {
	return inst2_unknown;
}

/* Get mode and option details */
static inst_code get_opt_details(inst *p, inst_optdet_type m, ...) {
	return inst_unsupported;
}

/* Set the device measurement mode */                                       
static inst_code set_mode(
inst *p,
inst_mode m) {		/* Requested mode */
	return inst_unsupported;
}

/* Set or reset an option */
static inst_code set_opt_mode(
inst *p,
inst_opt_mode m,	/* Requested option mode */
...) {				/* Option parameters */                             
	return inst_unsupported;
}

/* Get a dynamic status */
static inst_code get_status(
inst *p,
inst_status_type m,	/* Requested status type */
...) {				/* Status parameters */                             
	return inst_unsupported;
}

/* Read a full test chart composed of multiple sheets */
/* DOESN'T use the trigger mode */
/* Return the inst error code */
static inst_code read_chart(
inst *p,
int npatch,			/* Total patches/values in chart */
int pich,			/* Passes (rows) in chart */
int sip,			/* Steps in each pass (patches in each row) */
int *pis,			/* Passes in each strip (rows in each sheet) */
int chid,			/* Chart ID number */
ipatch *vals) {		/* Pointer to array of values */

	return inst_unsupported;
}

/* For an xy instrument, release the paper */
/* (if cap has inst_xy_holdrel) */
/* Return the inst error code */
static inst_code xy_sheet_release(
inst *p) {
	return inst_unsupported;
}

/* For an xy instrument, hold the paper */
/* (if cap has inst_xy_holdrel) */
/* Return the inst error code */
static inst_code xy_sheet_hold(
inst *p) {
	return inst_unsupported;
}

/* For an xy instrument, allow the user to locate a point */
/* (if cap has inst_xy_locate) */
/* Return the inst error code */
static inst_code xy_locate_start(
inst *p) {
	return inst_unsupported;
}

/* For an xy instrument, read back the location */
/* (if cap has inst_xy_locate) */
/* Return the inst error code */
static inst_code xy_get_location(
inst *p,
double *x, double *y) {
	return inst_unsupported;
}

/* For an xy instrument, end allowing the user to locate a point */
/* (if cap has inst_xy_locate) */
/* Return the inst error code */
static inst_code xy_locate_end(
inst *p) {
	return inst_unsupported;
}

/* For an xy instrument, move the measurement point */
/* (if cap has inst_xy_position) */
/* Return the inst error code */
static inst_code xy_position(
inst *p,
int measure,	/* nz for measure point, z for locate point */	        
double x, double y) {
	return inst_unsupported;
}

/* For an xy instrument, try and clear the table after an abort */
/* Return the inst error code */
static inst_code xy_clear(
inst *p) {
	return inst_unsupported;
}

/* Read a sheet full of patches using xy mode */
/* DOESN'T use the trigger mode */
/* Return the inst error code */
static inst_code read_xy(
inst *p,
int pis,			/* Passes in strips (letters in sheet) */
int sip,			/* Steps in pass (numbers in sheet) */
int npatch,			/* Total patches in strip (skip in last pass) */
char *pname,		/* Starting pass name (' A' to 'ZZ') */             
char *sname,		/* Starting step name (' 1' to '99') */             
double ox, double oy,	/* Origin of first patch */
double ax, double ay,	/* pass increment */
double aax, double aay,	/* pass offset for odd patches */
double px, double py,	/* step/patch increment */
ipatch *vals) {		/* Pointer to array of values */
	return inst_unsupported;
}

/* Read a set of strips (applicable to strip reader) */
/* Obeys the trigger mode set */
/* Return the inst error code */
static inst_code read_strip(
inst *p,
char *name,			/* Strip name (up to 7 chars) */
int npatch,			/* Number of patches in each pass */
char *pname,		/* Pass name (3 chars) */
int sguide,			/* Guide number (decrements by 5) */
double pwid,		/* Patch width in mm (For DTP20/DTP41) */
double gwid,		/* Gap width in mm  (For DTP20/DTP41) */
double twid,		/* Trailer width in mm  (For DTP41T) */
ipatch *vals) {		/* Pointer to array of values */
	return inst_unsupported;
}

/* Read a single sample (applicable to spot instruments) */
/* Obeys the trigger mode set */
/* Return the inst error code */
static inst_code read_sample(
inst *p,
char *name,			/* Patch identifier (up to 7 chars) */
ipatch *val) {		/* Pointer to value to be returned */
	return inst_unsupported;
}

/* Determine if a calibration is needed. Returns inst_calt_none if not, */  
/* inst_calt_unknown if it is unknown, or inst_calt_XXX if needs calibration, */ 
/* and the first type of calibration needed. */
static inst_cal_type needs_calibration(
inst *p) {
	return inst_calt_unknown;
}

/* Request an instrument calibration. */
/* DOESN'T use the trigger mode */
/* Return inst_unsupported if calibration type is not appropriate. */
static inst_code calibrate(
inst *p,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */ 
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]) {	/* Condition identifier (ie. white reference ID, filter ID) */
	return inst_unsupported;
}

/* Insert a compensation filter in the instrument readings */
/* This is typically needed if an adapter is being used, that alters */     
/* the spectrum of the light reaching the instrument */                     
/* To remove the filter, pass NULL for the filter filename */               
static inst_code comp_filter(											        
inst *p,
char *filtername) {		/* File containing compensating filter */
	return inst_unsupported;
}

/* Insert a colorimetric correction matrix in the instrument XYZ readings */
/* This is only valid for colorimetric instruments. */
/* To remove the matrix, pass NULL for the matrix */
static inst_code col_cor_mat(
struct _inst *p,
double mtx[3][3]) {	/* XYZ matrix */
	return inst_unsupported;
}

/* Use a Colorimeter Calibration Spectral Set to set the */
/* instrumen calibration. */
/* This is only valid for colorimetric instruments. */
/* To set calibration back to default, pass NULL for ccss. */
static inst_code col_cal_spec_set(
inst *pp,
icxObserverType obType,
xspect custObserver[3],
xspect *sets,
int no_sets) {
	return inst_unsupported;
}

/* Send a message to the user. */
static inst_code message_user(struct _inst *p, char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);

	return inst_ok;
}

/* Poll for a user abort, terminate, trigger or command. */
/* Wait for a key rather than polling, if wait != 0 */
/* Return: */
/* inst_ok if no key has been hit or key is to be ignored, */
/* inst_user_abort if User abort has been hit, */
/* inst_user_term if User terminate has been hit. */
/* inst_user_trig if User trigger has been hit */
/* inst_user_cmnd if User command has been hit */
static inst_code poll_user(inst *p, int wait) {
	inst_code ev = inst_internal_error;

	switch(icoms_poll_user(p->icom, wait)) {
		case ICOM_OK:
			ev = inst_ok;
			break;
		case ICOM_USER:
			ev = inst_user_abort;
			break;
		case ICOM_TERM:
			ev = inst_user_term;
			break;
		case ICOM_TRIG:
			ev = inst_user_trig;
			break;
		case ICOM_CMND:
			ev = inst_user_cmnd;
			break;
	}
	return ev;
}

/* Generic inst error codes interpretation */
static char *inst_interp_error(inst *p, inst_code ec) {
	switch(ec & inst_mask) {
		case inst_ok:
			return "No error";
		case inst_notify:
			return "Notification";
		case inst_warning:
			return "Warning";
		case inst_no_coms:
			return "Internal error - communications needed but not established";
		case inst_no_init:
			return "Internal error - initialisation needed but not done";
		case inst_internal_error:
			return "Internal software error";
		case inst_coms_fail:
			return "Communications failure";
		case inst_unknown_model:
			return "Not expected instrument model";
		case inst_protocol_error:
			return "Communication protocol breakdown";
		case inst_user_abort:
			return "User hit Abort Key";
		case inst_user_term:
			return "User hit Terminate Key";
		case inst_user_trig:
			return "User hit Trigger Key";
		case inst_user_cmnd:
			return "User hit a Command Key";
		case inst_misread:
			return "Measurement misread";
		case inst_nonesaved:
			return "No saved data to read";
		case inst_nochmatch:
			return "Chart being read doesn't match chart expected";
		case inst_needs_cal:
			return "Instrument needs calibration";
		case inst_cal_setup:
			return "Instrument needs to be setup for calibration";
		case inst_unsupported:
			return "Unsupported function";
		case inst_unexpected_reply:
			return "Unexpected Reply";
		case inst_wrong_sensor_pos:
			return "Wrong Sensor Position";
		case inst_wrong_config:
			return "Wrong Configuration";
		case inst_bad_parameter:
			return "Bad Parameter Value";
		case inst_hardware_fail:
			return "Hardware Failure";
		case inst_other_error:
			return "Non-specific error";
	}
	return "Unknown inst error code";
}

/* Instrument specific error codes interpretation */
static char *interp_error(inst *p, int ec) {
	return "interp_error is uninplemented in base class!";
}

/* Return the last communication error code */
static int last_comerr(inst *p) {
	return p->icom->lerr;
}

/* ---------------------------------------------- */
/* Virtual constructor. */
/* Return NULL for unknown instrument, */
/* or serial instrument if nocoms == 0. */
extern inst *new_inst(
int comport,
int nocoms,			/* Don't open if communications are needed to establish inst type */
int debug,			/* Coms and instrument debug level */
int verb			/* Verbosity flag */
) {			/* Debug flag */
	instType itype;		/* Actual type */
	icoms *icom;		/* Optional icoms to hand in */
	inst *p;

	if ((icom = new_icoms()) == NULL) {
		return NULL;
	}

	if (debug > 0)
		icom->debug = debug;

	/* Set instrument type from USB port, if not specified */
	itype = usb_is_usb_portno(icom, comport);		/* Type from USB */
	if (itype == instUnknown)						/* Not USB */
		itype = hid_is_hid_portno(icom, comport);	/* Else type from HID */
#ifdef ENABLE_SERIAL
	if (itype == instUnknown && !nocoms)
		itype = ser_inst_type(icom, comport);		/* Else type from serial */
#endif /* ENABLE_SERIAL */

	if (itype == instDTP20)
		p = (inst *)new_dtp20(icom, itype, debug, verb);
	else if (itype == instDTP22)
		p = (inst *)new_dtp22(icom, itype, debug, verb);
	else if (itype == instDTP41)
		p = (inst *)new_dtp41(icom, itype, debug, verb);
	else if (itype == instDTP51)
		p = (inst *)new_dtp51(icom, itype, debug, verb);
	else if ((itype == instDTP92) || 
	         (itype == instDTP94))
		p = (inst *)new_dtp92(icom, itype, debug, verb);
	else if ((itype == instSpectrolino ) ||
			 (itype == instSpectroScan ) ||
			 (itype == instSpectroScanT))
		p = (inst *)new_ss(icom, itype, debug, verb);
/* NYI
	else if (itype == instSpectrocam)
		p = (inst *)new_spc(icom, itype, debug, verb);
*/
	else if (itype == instI1Disp1 ||
		    itype == instI1Disp2)
		p = (inst *)new_i1disp(icom, itype, debug, verb);
	else if (itype == instI1Disp3)
		p = (inst *)new_i1d3(icom, itype, debug, verb);
	else if (itype == instI1Monitor)
		p = (inst *)new_i1pro(icom, itype, debug, verb);
	else if (itype == instI1Pro)
		p = (inst *)new_i1pro(icom, itype, debug, verb);
	else if (itype == instColorMunki)
		p = (inst *)new_munki(icom, itype, debug, verb);
	else if (itype == instHCFR)
		p = (inst *)new_hcfr(icom, itype, debug, verb);
	else if (itype == instSpyder2)
		p = (inst *)new_spyd2(icom, itype, debug, verb);
	else if (itype == instSpyder3)
		p = (inst *)new_spyd2(icom, itype, debug, verb);
	else if (itype == instSpyder4)
		p = (inst *)new_spyd2(icom, itype, debug, verb);
	else if (itype == instHuey)
		p = (inst *)new_huey(icom, itype, debug, verb);
	else if (itype == instColorHug)
		p = (inst *)new_colorhug(icom, itype, debug, verb);
	else {
		return NULL;
	}

	/* Add default methods if constructor did not supply them */
	if (p->init_coms == NULL)
		p->init_coms = init_coms;
	if (p->init_inst == NULL)
		p->init_inst = init_inst;
	if (p->get_itype == NULL)
		p->get_itype = get_itype;
	if (p->get_serial_no == NULL)
		p->get_serial_no = get_serial_no;
	if (p->capabilities == NULL)
		p->capabilities = capabilities;
	if (p->capabilities2 == NULL)
		p->capabilities2 = capabilities2;
	if (p->get_opt_details == NULL)
		p->get_opt_details = get_opt_details;
	if (p->set_mode == NULL)
		p->set_mode = set_mode;
	if (p->set_opt_mode == NULL)
		p->set_opt_mode = set_opt_mode;
	if (p->get_status == NULL)
		p->get_status = get_status;
	if (p->read_chart == NULL)
		p->read_chart = read_chart;
	if (p->xy_sheet_release == NULL)
		p->xy_sheet_release = xy_sheet_release;
	if (p->xy_sheet_hold == NULL)
		p->xy_sheet_hold = xy_sheet_hold;
	if (p->xy_locate_start == NULL)
		p->xy_locate_start = xy_locate_start;
	if (p->xy_get_location == NULL)
		p->xy_get_location = xy_get_location;
	if (p->xy_locate_end == NULL)
		p->xy_locate_end = xy_locate_end;
	if (p->xy_position == NULL)
		p->xy_position = xy_position;
	if (p->xy_clear == NULL)
		p->xy_clear = xy_clear;
	if (p->read_xy == NULL)
		p->read_xy = read_xy;
	if (p->read_strip == NULL)
		p->read_strip = read_strip;
	if (p->read_sample == NULL)
		p->read_sample = read_sample;
	if (p->needs_calibration == NULL)
		p->needs_calibration = needs_calibration;
	if (p->calibrate == NULL)
		p->calibrate = calibrate;
	if (p->comp_filter == NULL)
		p->comp_filter = comp_filter;
	if (p->col_cor_mat == NULL)
		p->col_cor_mat = col_cor_mat;
	if (p->col_cal_spec_set == NULL)
		p->col_cal_spec_set = col_cal_spec_set;
	if (p->message_user == NULL)
		p->message_user = message_user;
	if (p->poll_user == NULL)
		p->poll_user = poll_user;
	if (p->inst_interp_error == NULL)
		p->inst_interp_error = inst_interp_error;
	if (p->interp_error == NULL)
		p->interp_error = interp_error;
	if (p->last_comerr == NULL)
		p->last_comerr = last_comerr;

	return p;
}

/* --------------------------------------------------- */

#ifdef ENABLE_SERIAL
static void hex2bin(char *buf, int len);

/* Heuristicly determine the instrument type for */
/* a serial connection, and instUnknown not serial. */
static instType ser_inst_type(
	icoms *p, 
	int port		/* Enumerated port number, 1..n */
) {
	instType rv = instUnknown;
	char buf[100];
	baud_rate brt[] = { baud_9600, baud_19200, baud_4800, baud_2400,
	                     baud_1200, baud_38400, baud_57600, baud_115200,
	                     baud_600, baud_300, baud_110, baud_nc };
	unsigned int etime;
	unsigned int bi, i;
	int se, len;
	int xrite = 0;
	int ss = 0;
	int so = 0;
	
	if (p->paths == NULL)
		p->get_paths(p);

	if (port <= 0 || port > p->npaths)
		return instUnknown;

#ifdef ENABLE_USB
	if (p->paths[port-1]->dev != NULL
	 || p->paths[port-1]->hev != NULL)
		return instUnknown;
#endif /* ENABLE_USB */

	if (p->debug) fprintf(stderr,"instType: About to init Serial I/O\n");

	bi = 0;

	/* The tick to give up on */
	etime = msec_time() + (long)(1000.0 * 20.0 + 0.5);

	if (p->debug) fprintf(stderr,"instType: Trying different baud rates (%u msec to go)\n",etime - msec_time());

	/* Until we time out, find the correct baud rate */
	for (i = bi; msec_time() < etime; i++) {
		if (brt[i] == baud_nc)
			i = 0;
		p->set_ser_port(p, port, fc_none, brt[i], parity_none, stop_1, length_8);

//printf("~1 brt = %d\n",brt[i]);
		if ((se = p->write_read(p, ";D024\r\n", buf, 100, '\r', 1, 0.5)) != 0) {
			if ((se & ICOM_USERM) == ICOM_USER) {
				if (p->debug) fprintf(stderr,"instType: User aborted\n");
				return instUnknown;
			}
		}
		len = strlen(buf);

//printf("~1 len = %d\n",len);
		if (len < 4)
			continue;

		/* Is this an X-Rite error value such as "<01>" ? */
		if (buf[0] == '<' && isdigit(buf[1]) && isdigit(buf[2]) && buf[3] == '>') {
//printf("~1 xrite\n");
			xrite = 1;
			break;
		}

		/* Is this a Spectrolino error resonse ? */
		if (len >= 5 && strncmp(buf, ":26", 3) == 0) {
//printf("~1 spectrolino\n");
			so = 1;
			break;
		}
		/* Is this a SpectroScan response ? */
		if (len >= 7 && strncmp(buf, ":D183", 5) == 0) {
//printf("~1 spectroscan\n");
			ss = 1;
			break;
		}
	}

	if (msec_time() >= etime) {		/* We haven't established comms */
		if (p->debug) fprintf(stderr,"instType: Failed to establish coms\n");
		return instUnknown;
	}

	if (p->debug) fprintf(stderr,"instType: Got coms with instrument\n");

	/* Spectrolino */
	if (so) {
		rv = instSpectrolino;
	}

	/* SpectroScan */
	if (ss) {
		rv = instSpectroScan;
		if ((se = p->write_read(p, ";D030\r\n", buf, 100, '\n', 1, 1.5)) == 0)  {
			if (strlen(buf) >= 41) {
				hex2bin(&buf[5], 12);
//printf("~1 type = '%s'\n",buf);
				if (strncmp(buf, ":D190SpectroScanT", 17) == 0)
					rv = instSpectroScanT;
			}
		}
	}
	if (xrite) {

		/* Get the X-Rite model and version number */
		if ((se = p->write_read(p, "SV\r\n", buf, 100, '>', 1, 2.5)) != 0)
			return instUnknown;
	
		if (strlen(buf) >= 12) {
		    if (strncmp(buf,"X-Rite DTP22",12) == 0)
				rv = instDTP22;
		    if (strncmp(buf,"X-Rite DTP41",12) == 0)
				rv = instDTP41;
		    if (strncmp(buf,"X-Rite DTP42",12) == 0)
				rv = instDTP41;
		    if (strncmp(buf,"X-Rite DTP51",12) == 0)
				rv = instDTP51;
		    if (strncmp(buf,"X-Rite DTP52",12) == 0)
				rv = instDTP51;
		    if (strncmp(buf,"X-Rite DTP92",12) == 0)
				rv = instDTP92;
		    if (strncmp(buf,"X-Rite DTP94",12) == 0)
				rv = instDTP94;
		}
	}

	if (p->debug) fprintf(stderr,"instType: Instrument type is '%s'\n", inst_name(rv));

	return rv;
}

/* Convert an ASCII Hex character to an integer. */
static int h2b(char c) {
	if (c >= '0' && c <= '9')
		return (c-(int)'0');
	if (c >= 'A' && c <= 'F')
		return (10 + c-(int)'A');
	if (c >= 'a' && c <= 'f')
		return (10 + c-(int)'a');
	return 0;
}

/* Convert a Hex encoded buffer into binary. */
/* len is number of bytes out */
static void hex2bin(char *buf, int len) {
	int i;

	for (i = 0; i < len; i++) {
		buf[i] = (char)((h2b(buf[2 * i + 0]) << 4)
		              | (h2b(buf[2 * i + 1]) << 0));
	}
}

#endif /* ENABLE_SERIAL */

/* ================================================================= */

/* A default calibration user interaction handler using the console. */
/* This handles both normal and display based calibration interaction */
/* with the instrument, if a disp_setup function and pointer to disp_win_info */
/* is provided. */
inst_code inst_handle_calibrate(
	inst *p,
	inst_cal_type calt,		/* type of calibration to do. inst_calt_all for all */
	inst_cal_cond calc,		/* Current condition. inst_calc_none for not setup */
	inst_code (*disp_setup) (inst *p,inst_cal_cond calc, disp_win_info *dwi),
							/* Callback for handling a display calibration - May be NULL */
	disp_win_info *dwi		/* Information to be able to open a display test patch - May be NULL */
) {
	inst_code rv = inst_ok, ev;
	int usermes = 0;		/* User was given a message */
	char id[200];			/* Condition identifier */
	int ch;

	DBG((dbgo,"inst_handle_calibrate called\n"))

	/* Untill we're done with the calibration */
	for (;;) {

		DBG((dbgo,"About to call calibrate at top of loop\n"))
	    ev = p->calibrate(p, calt, &calc, id);
		DBG((dbgo,"calibrate returned calt 0x%x, calc 0x%x\n",calt,calc))

		/* We're done */
		if ((ev & inst_mask) == inst_ok) {
			if (calc == inst_calc_message)
				printf("%s\n",id);
			if (usermes)
				printf("Calibration complete\n");
			fflush(stdout);
			DBG((dbgo,"inst_handle_calibrate done 0x%x\n",ev))
			return ev;
		}

		/* User aborted */
		if ((ev & inst_mask) == inst_user_abort
		 || (ev & inst_mask) == inst_user_term) {
			DBG((dbgo,"inst_handle_calibrate user aborted 0x%x\n",ev))
			return ev;
		}

		/* Retry on an error */
		if ((ev & inst_mask) != inst_cal_setup) {
			if ((ev & inst_mask) == inst_unsupported) {
				DBG((dbgo,"inst_handle_calibrate calibration not supported\n"))
				return inst_unsupported;
			}

			printf("Calibration failed with '%s' (%s)\n",
				       p->inst_interp_error(p, ev), p->interp_error(p, ev));
			printf("Hit any key to retry, or Esc or Q to abort:\n");

			empty_con_chars();
			ch = next_con_char();
			printf("\n");
			if (ch == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
				DBG((dbgo,"inst_handle_calibrate user aborted 0x%x\n",inst_user_abort))
				fflush(stdout);
				return inst_user_abort;
			}

		/* Get user to do/setup calibration */
		} else {

			switch (calc) {
				case inst_calc_uop_ref_white:
					printf("Do a reflective white calibration,\n");
					printf(" and then hit any key to continue,\n"); 
					printf(" or hit Esc or Q to abort:"); 
					break;

				case inst_calc_uop_trans_white:
					printf("Do a transmissive white calibration,\n");
					printf(" and then hit any key to continue,\n"); 
					printf(" or hit Esc or Q to abort:"); 
					break;
			
				case inst_calc_uop_trans_dark:
					printf("Do a transmissive dark calibration,\n");
					printf(" and then hit any key to continue,\n"); 
					printf(" or hit Esc or Q to abort:"); 
					break;
			
				case inst_calc_man_ref_white:
					printf("Place the instrument on its reflective white reference %s,\n",id);
					printf(" and then hit any key to continue,\n"); 
					printf(" or hit Esc or Q to abort:"); 
					break;

				case inst_calc_man_ref_whitek:
					printf("Click the instrument on its reflective white reference %s,\n",id);
					printf(" or hit Esc or Q to abort:"); 
					break;

				case inst_calc_man_ref_dark:
					printf("Place the instrument in the dark, not in contact with any surface,\n");
					printf(" and then hit any key to continue,\n"); 
					printf(" or hit Esc or Q to abort:"); 
					break;

				case inst_calc_man_em_dark:
					printf("Place cap on the instrument, or place in on a dark surface,\n");
					printf("or place on the white calibration reference,\n");
					printf(" and then hit any key to continue,\n"); 
					printf(" or hit Esc or Q to abort:"); 
					break;

				case inst_calc_man_cal_smode:
					printf("Set instrument sensor to calibration position,\n");
					printf(" and then hit any key to continue,\n"); 
					printf(" or hit Esc or Q to abort:"); 
					break;

				case inst_calc_man_trans_white:
					printf("Place the instrument on its transmissive white source,\n");
					printf(" and then hit any key to continue,\n"); 
					printf(" or hit Esc or Q to abort:"); 
					break;

				case inst_calc_man_trans_dark:
					printf("Use the appropriate tramissive blocking to block the transmission path,\n");
					printf(" and then hit any key to continue,\n"); 
					printf(" or hit Esc or Q to abort:"); 
					break;

				case inst_calc_change_filter:
					printf("Change filter on instrument to %s,\n",id);
					printf(" and then hit any key to continue,\n"); 
					printf(" or hit Esc or Q to abort:"); 
					break;

				case inst_calc_message:
					printf("%s\n",id);
					printf(" Hit any key to continue,\n"); 
					printf(" or hit Esc or Q to abort:"); 
					break;

				case inst_calc_disp_white:
				case inst_calc_proj_white:
					if (disp_setup == NULL || dwi == NULL) { /* No way of creating a test window */
						DBG((dbgo,"inst_handle_calibrate no way of creating test window 0x%x\n",inst_internal_error))
						return inst_internal_error; 
					}
				
					/* We need to display a 100% white patch to proceed with this */
					/* type of calibration */
					if ((rv = disp_setup(p, calc, dwi)) != inst_ok)
						return rv; 
					break;

				case inst_calc_disp_grey:
				case inst_calc_disp_grey_darker: 
				case inst_calc_disp_grey_ligher:
				case inst_calc_proj_grey:
				case inst_calc_proj_grey_darker: 
				case inst_calc_proj_grey_ligher:
					if (dwi == NULL) {	/* No way of creating a test window */
						DBG((dbgo,"inst_handle_calibrate no way of creating test window 0x%x\n",inst_internal_error))
						return inst_internal_error; 
					}

					/* We need to display a test patch to proceed with this
					 * type of calibration. Typically this will be:
					 *
					 * inst_calc_xxxx_grey:
					 *		set p->cal_gy_level = 0.6
					 *		set p->cal_gy_count = 0;
					 *
					 * inst_calc_xxxx_grey_darker:
					 *		set p->cal_gy_level *= 0.7
					 *		set p->cal_gy_count++
					 *
					 * inst_calc_xxxx_grey_ligher:
					 *		set p->cal_gy_level *= 1.4
					 *		set p->cal_gy_count++
					 *
					 * and return failure if p->cal_gy_count > 4
					 */

					if ((rv = disp_setup(p, calc, dwi)) != inst_ok)
						return rv; 

					break;

				default:
					/* Something isn't being handled */
					DBG((dbgo,"inst_handle_calibrate unhandled calc case 0x%x, err 0x%x\n",calc,inst_internal_error))
					return inst_internal_error;
			}
			fflush(stdout);

			usermes = 1;

			if (calc != inst_calc_man_ref_whitek) {
				empty_con_chars();
				ch = next_con_char();
				printf("\n");
				if (ch == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
					DBG((dbgo,"inst_handle_calibrate user aborted 0x%x\n",inst_user_abort))
					return inst_user_abort;
				}
			}
		}
	}
}

/* ============================================================================= */

/* A helper function to display -y flag usage for each instrument type available */
/* Return accumulated capabilities of all the instruments */
inst_capability inst_show_disptype_options(FILE *fp, char *oline, icoms *icom) { 
	int i, j;
	char buf[200], *bp;
	int olen, pstart;
	int notall = 0;
	inst_capability acap = 0;	/* Accumulate capabilities */

	olen = strlen(oline);		/* lenth of option part of line */

	/* Locate the end of the option */
	for (bp = oline; *bp != '\000' && *bp == ' '; bp++)
		;
	for (; *bp != '\000' && *bp != ' '; bp++)
		;
	pstart = bp - oline;
	if (pstart > 10)
		pstart = 10;
	strncpy(buf, oline, pstart); 
	buf[pstart++] = ' ';

	olen = strlen(oline);		/* lenth of option part of line */

	if (icom->paths == NULL)
	    icom->get_paths(icom);

	for (i = 0; i < icom->npaths; i++) {
		inst *it;
		inst_capability cap;
		int k;

		if ((it = new_inst(i+1, 1, 0, 0)) == NULL) {
			notall = 1;
			continue;
		}

		cap = it->capabilities(it);
		acap |= cap;

		if (cap & inst_emis_disptype) {
			int nsel;
			inst_disptypesel *sels;

			if (it->get_opt_details(it, inst_optdet_disptypesel, &nsel, &sels) != inst_ok) {
				it->del(it);
				continue;
			}
			for (j = 0; j < nsel; j++) {
				int m = pstart;
				for (k = 0; k < 9; k++) {
					if (sels[j].sel[k] == '\000')
						break;
					if (m > pstart)
						buf[m++] = '|';
					buf[m++] = sels[j].sel[k];
				}
				while (m < (olen+1))	/* Indent it by 1 */
					buf[m++] = ' ';
				buf[m++] = '\000';
				
				fprintf(fp, "%s%s\n",buf, sels[j].desc);

				if (j == 0) {
					for (m = 0; m < pstart; m++)
						buf[m] = ' ';
				}
			}
		}
	}
	/* Output a default desciption */
	if (notall) {
		int m = pstart;
		buf[m++] = 'c';
		buf[m++] = '|';
		buf[m++] = 'l';
		while (m < olen)
			buf[m++] = ' ';
		buf[m++] = '\000';
		fprintf(fp, "%s%s\n",buf, " Other: c = CRT, l = LCD");
	}

	return acap;
}

/* A helper function to turn a -y flag into a selection index */
/* Return 0 on error */
int inst_get_disptype_index(inst *it, int c) {
	inst_capability cap;
	int j, k;

	cap = it->capabilities(it);

	if (cap & inst_emis_disptype) {
		int nsel;
		inst_disptypesel *sels;

		if (it->get_opt_details(it, inst_optdet_disptypesel, &nsel, &sels) != inst_ok) {
			return 0;
		}
		for (j = 0; j < nsel; j++) {
			for (k = 0; k < 9; k++) {
				if (sels[j].sel[k] == '\000')
					break;
				if (sels[j].sel[k] == c) {
					return sels[j].ix;
				}
			}
		}
	}
	return 0;
}






















