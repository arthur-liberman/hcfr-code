
 /* Abstract instrument class implemenation */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   10/3/2001
 *
 * Copyright 2001 - 2013 Graeme W. Gill
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
#else
#include "sa_config.h"
#endif /* !SALONEINSTLIB */
#include "numsup.h"
#include "xspect.h"
#include "conv.h"

#include "insttypes.h"
#include "icoms.h"
#include "inst.h"
#include "insttypeinst.h"
#include "ccmx.h"
#include "ccss.h"
#include "sort.h"

#if defined(ENABLE_FAST_SERIAL)
instType fast_ser_inst_type(icoms *p, int tryhard, 
       inst_code (*uicallback)(void *cntx, inst_ui_purp purp), void *cntx);
# if defined(ENABLE_SERIAL)
static instType ser_inst_type(icoms *p, 
       inst_code (*uicallback)(void *cntx, inst_ui_purp purp), void *cntx);
# endif /* ENABLE_SERIAL */
#endif /* ENABLE_FAST_SERIAL */

/* ------------------------------------ */
/* Default methods for instrument class */

/* Establish communications at the indicated baud rate. */
/* Timout in to seconds, and return non-zero error code */
static inst_code init_coms(
inst *p,
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

/* Return the instrument mode capabilities */
static void capabilities(inst *p, inst_mode *cap1,
                         inst2_capability *cap2, inst3_capability *cap3) {
	if (cap1 != NULL)
		*cap1 = inst_mode_none;
	if (cap2 != NULL)
		*cap2 = inst2_none;
	if (cap3 != NULL)
		*cap3 = inst3_none;
}

/* Return current or given configuration available measurement modes. */
/* This default routine assumed one confiration which will be */
/* all the measurement modes returned by capabilities(). */
static inst_code meas_config(inst *p,
inst_mode *mmodes,		/* Return all the valid measurement modes */
						/* for the current configuration */
inst_cal_cond *cconds,	/* Return the valid calibration conditions */
int *conf_ix) {			/* Request mode for given configuration, and */
						/* return the index of the configuration returned */
	if (mmodes != NULL) {
		*mmodes = inst_mode_none;

		/* Available measurement modes is current capabilities */
		p->capabilities(p, mmodes, NULL, NULL);
	}

	if (cconds != NULL)
		*cconds = inst_calc_unknown;

	if (conf_ix != NULL)
		*conf_ix = 0;

	return inst_ok;
}

/* Check the device measurement mode */                                       
static inst_code check_mode(
inst *p,
inst_mode m) {		/* Requested mode */
	return inst_unsupported;
}

/* Set the device measurement mode */                                       
static inst_code set_mode(
inst *p,
inst_mode m) {		/* Requested mode */
	return inst_unsupported;
}

/* Return array of display type selectors */
static inst_code get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	return inst_unsupported;
}

/* Set the display type */
static inst_code set_disptype(
inst *pp,
int ix						/* index into the inst_disptypesel[] */
) {
	return inst_unsupported;
}

/* Get the disptech and other corresponding info for the current */
/* selected display type. Returns disptype_unknown by default. */
/* Because refrmode can be overridden, it may not match the refrmode */
/* of the dtech. (Pointers may be NULL if not needed) */
static inst_code get_disptechi(
inst *pp,
disptech *dtech,
int *refrmode,
int *cbid) {
	if (dtech != NULL)
		*dtech = disptech_unknown;
	if (refrmode != NULL)
		*refrmode = disptech_get_id(disptech_unknown)->refr;
	if (cbid != NULL)
		*cbid = 0;
	return inst_ok;
}

/* Get a status or set or get an option (default implementation) */
inst_code inst_get_set_opt_def(
inst *p,
inst_opt_type m,	/* Requested option type */
va_list args) {		/* Option parameters */                             

	return inst_unsupported;
}

/* Get a status or set or get an option */
static inst_code get_set_opt(
inst *p,
inst_opt_type m,	/* Requested status type */
...) {				/* Status parameters */                             
	inst_code rv;
	va_list args;

	va_start(args, m);
	rv = inst_get_set_opt_def(p, m, args);	/* Call the above */
	va_end(args);

	return rv;
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
char *name,		 			/* Patch identifier (up to 7 chars) */
ipatch *val,				/* Pointer to value to be returned */
instClamping clamp) {		/* NZ to clamp XYZ to be +ve */
	return inst_unsupported;
}

/* Measure the emissive refresh rate in Hz. */
static inst_code read_refrate(
inst *p,
double *ref_rate) {
	if (ref_rate != NULL)
		*ref_rate = 0.0;
	return inst_unsupported;
}

/* Determine if a calibration is needed. Returns inst_calt_none if not */  
/* or unknown, or a mask of the needed calibrations. */
/* Default implementation - call through to get_n_a_cals() */
static inst_cal_type needs_calibration(
inst *p) {
	inst_cal_type n_cals;
	
	if (p->get_n_a_cals(p, &n_cals, NULL) != inst_ok)
		return inst_calt_none;

	return n_cals;
}

/* Return combined mask of  of needed or available inst_cal_type's */
/* for the current mode. */
inst_code inst_get_n_a_cals(
struct _inst *p,
inst_cal_type *n_cals,
inst_cal_type *a_cals) {

	if (n_cals != NULL)
		*n_cals = inst_calt_none;

	if (a_cals != NULL)
		*a_cals = inst_calt_none;

	return inst_ok;
}

/* Request an instrument calibration. */
/* DOESN'T use the trigger mode */
/* Return inst_unsupported if calibration type is not appropriate. */
static inst_code calibrate(
inst *p,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]) {	/* Condition identifier (ie. white reference ID, filter ID) */
	return inst_unsupported;
}

/* Measure a display update delay. It is assumed that a */
/* black to white change has been made to the displayed color, */
/* and this will measure the time it took for the update to */
/* (It is assumed that white_change() will be called at the time the patch */
/* changes color.) */
/* be noticed by the instrument, up to 1.0 seconds. */
/* inst_misread will be returned on failure to find a transition. */
static inst_code meas_delay(
inst *p,
int *pdispmsec,
int *pinstmsec) {
	return inst_unsupported;
}

/* Call used by other thread to timestamp the transition. */
static inst_code white_change(
struct _inst *p, int init) {
	return inst_unsupported;
}
																				\
/* Return the last calibrated refresh rate in Hz. Returns: */
/* inst_unsupported - if this instrument doesn't suport a refresh mode */
/*                    or is unable to retrieve the refresh rate */
/* inst_needs_cal   - if the refresh rate value is not valid */
/* inst_misread     - if no refresh rate could be determined */
/* inst_ok          - on returning a valid reading */
static inst_code get_refr_rate(
struct _inst *p,
double *ref_rate) {
	if (ref_rate != NULL)
		*ref_rate = 0.0;
	return inst_unsupported;
}

/* Set the calibrated refresh rate in Hz. */
/* Set refresh rate to 0.0 to mark it as invalid */
/* Rates outside the range 5.0 to 150.0 Hz will return an error */
static inst_code set_refr_rate(
struct _inst *p,
double ref_rate) {
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
disptech dtech,		/* Use disptech_unknown if not known */	
int cbid,       	/* Calibration display type base ID needed, 1 if unknown */
double mtx[3][3]) {	/* XYZ matrix */
	return inst_unsupported;
}

/* Use a Colorimeter Calibration Spectral Set to set the */
/* instrumen calibration. */
/* This is only valid for colorimetric instruments. */
/* To set calibration back to default, pass NULL for ccss. */
static inst_code col_cal_spec_set(
inst *pp,
disptech dtech,		/* Use disptech_unknown if not known */
xspect *sets,
int no_sets) {
	return inst_unsupported;
}

/* Supply a user interaction callback function. */
static void set_uicallback(
inst *pp,
inst_code (*uicallback)(void *cntx, inst_ui_purp purp),
void *cntx)	 {
	pp->uicallback = uicallback;
	pp->uic_cntx = cntx;
}

/* Supply an asynchronous event callback function. */
static void set_event_callback(
inst *pp,
void (*eventcallback)(void *cntx, inst_event_type event),
void *cntx)	 {
	pp->eventcallback = eventcallback;
	pp->event_cntx = cntx;
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
		case inst_user_trig:
			return "User hit Trigger Key";
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
		case inst_wrong_config:
			return "Wrong Sensor Position";
		case inst_wrong_setup:
			return "Wrong or conflicting setup";
		case inst_bad_parameter:
			return "Bad Parameter Value";
		case inst_hardware_fail:
			return "Hardware Failure";
		case inst_system_error:
			return "Operating System Error";
		case inst_other_error:
			return "Non-specific error";
	}
	return "Unknown inst error code";
}

/* Instrument specific error codes interpretation */
static char *interp_error(inst *p, int ec) {
	return "interp_error is uninplemented in base class!";
}

/* Return the last serial communication error code */
/* (This is used for deciding fallback/retry strategies) */
static int last_scomerr(inst *p) {
	return p->icom->lserr;
}

/* Convert instrument specific inst_wrong_config error to inst_config enum */
static inst_config config_enum(inst *p, int ec) {
	return inst_conf_unknown;
}

/* ---------------------------------------------- */
/* Virtual constructor. */
/* Return NULL for unknown instrument, */
/* or serial instrument if nocoms == 0. */
extern inst *new_inst(
icompath *path,		/* Device path to instrument */
int nocoms,			/* Don't open if communications are needed to establish inst type */
a1log *log,			/* log to use */
inst_code (*uicallback)(void *cntx, inst_ui_purp purp),		/* optional uicallback */
void *cntx			/* Context for callback */
) {
	instType itype = instUnknown;	/* Actual type */
	icoms *icom;
	inst *p = NULL;

	if (path == NULL) {
		a1logd(log, 2, "new_inst: got NULL path\n");
		return NULL;
	}

	a1logd(log, 2, "new_inst: called with path '%s'\n",path->name);

	if ((icom = new_icoms(path, log)) == NULL) {
		a1logd(log, 2, "new_inst: new_icoms failed to open it\n");
		return NULL;
	}


	/* Set instrument type from USB port, if not specified */
	itype = icom->itype;		/* Instrument type if its known from usb/hid */

#if defined(ENABLE_FAST_SERIAL)
	if (itype == instUnknown && !nocoms && icom->fast) {
		itype = fast_ser_inst_type(icom, 1, uicallback, cntx);		/* Else type from serial */
	}
#endif /* ENABLE_FAST_SERIAL */

#if defined(ENABLE_SERIAL)
	if (itype == instUnknown && !nocoms)
		itype = ser_inst_type(icom, uicallback, cntx);		/* Else type from serial */
#endif /* ENABLE_SERIAL */


#ifdef ENABLE_SERIAL
	if (itype == instDTP22)
		p = (inst *)new_dtp22(icom, itype);
	else if (itype == instDTP41)
		p = (inst *)new_dtp41(icom, itype);
	else if (itype == instDTP51)
		p = (inst *)new_dtp51(icom, itype);
	else if (itype == instDTP92)
		p = (inst *)new_dtp92(icom, itype);
	else if ((itype == instSpectrolino ) ||
			 (itype == instSpectroScan ) ||
			 (itype == instSpectroScanT))
		p = (inst *)new_ss(icom, itype);
/* NYI
	else if (itype == instSpectrocam)
		p = (inst *)new_spc(icom, itype);
*/
#endif /* ENABLE_SERIAL */

#ifdef ENABLE_FAST_SERIAL
	if (itype == instSpecbos1201
	 || itype == instSpecbos)
		p = (inst *)new_specbos(icom, itype);
	if (itype == instKleinK10)
		p = (inst *)new_kleink10(icom, itype);
#endif /* ENABLE_SERIAL */

#ifdef ENABLE_USB
	if (itype == instDTP94)
		p = (inst *)new_dtp92(icom, itype);
//	else if (itype == instDTP20)
//		p = (inst *)new_dtp20(icom, itype);
	else if (itype == instI1Disp1 ||
		    itype == instI1Disp2)
		p = (inst *)new_i1disp(icom, itype);
	else if (itype == instI1Disp3)
		p = (inst *)new_i1d3(icom, itype);
	else if (itype == instI1Monitor)
		p = (inst *)new_i1pro(icom, itype);
	else if ((itype == instI1Pro) ||
	         (itype == instI1Pro2))
		p = (inst *)new_i1pro(icom, itype);
	else if (itype == instColorMunki)
		p = (inst *)new_munki(icom, itype);
	else if (itype == instSpyder1)
		p = (inst *)new_spyd2(icom, itype);
	else if (itype == instSpyder2)
		p = (inst *)new_spyd2(icom, itype);
	else if (itype == instSpyder3)
		p = (inst *)new_spyd2(icom, itype);
	else if (itype == instSpyder4)
		p = (inst *)new_spyd2(icom, itype);
	else if (itype == instSpyder5)
		p = (inst *)new_spyd2(icom, itype);
	else if (itype == instHuey)
		p = (inst *)new_huey(icom, itype);
	else if (itype == instSmile)
		p = (inst *)new_i1disp(icom, itype);
	else if (itype == instEX1)
		p = (inst *)new_ex1(icom, itype);
	else if (itype == instHCFR)
		p = (inst *)new_hcfr(icom, itype);
	else if (itype == instColorHug
	      || itype == instColorHug2)
		p = (inst *)new_colorhug(icom, itype);
#endif /* ENABLE_USB */



	/* Nothing matched */
	if (p == NULL) {
		a1logd(log, 2, "new_inst: instrument type not recognised\n");
		icom->del(icom);
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
	if (p->meas_config == NULL)
		p->meas_config = meas_config;
	if (p->set_mode == NULL)
		p->set_mode = set_mode;
	if (p->get_disptypesel == NULL)
		p->get_disptypesel = get_disptypesel;
	if (p->set_disptype == NULL)
		p->set_disptype = set_disptype;
	if (p->get_disptechi == NULL)
		p->get_disptechi = get_disptechi;
	if (p->get_set_opt == NULL)
		p->get_set_opt = get_set_opt;
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
	if (p->read_refrate == NULL)
		p->read_refrate = read_refrate;
	if (p->needs_calibration == NULL)
		p->needs_calibration = needs_calibration;
	if (p->get_n_a_cals == NULL)
		p->get_n_a_cals = inst_get_n_a_cals;
	if (p->calibrate == NULL)
		p->calibrate = calibrate;
	if (p->meas_delay == NULL)
		p->meas_delay = meas_delay;
	if (p->white_change == NULL)
		p->white_change = white_change;
	if (p->get_refr_rate == NULL)
		p->get_refr_rate = get_refr_rate;
	if (p->set_refr_rate == NULL)
		p->set_refr_rate = set_refr_rate;
	if (p->comp_filter == NULL)
		p->comp_filter = comp_filter;
	if (p->col_cor_mat == NULL)
		p->col_cor_mat = col_cor_mat;
	if (p->col_cal_spec_set == NULL)
		p->col_cal_spec_set = col_cal_spec_set;
	if (p->set_uicallback == NULL)
		p->set_uicallback = set_uicallback;
	if (p->set_event_callback == NULL)
		p->set_event_callback = set_event_callback;
	if (p->inst_interp_error == NULL)
		p->inst_interp_error = inst_interp_error;
	if (p->interp_error == NULL)
		p->interp_error = interp_error;
	if (p->last_scomerr == NULL)
		p->last_scomerr = last_scomerr;
	if (p->config_enum == NULL)
		p->config_enum = config_enum;

	/* Set the provided user interaction callback */
	p->set_uicallback(p, uicallback, cntx);

	return p;
}

/* --------------------------------------------------- */

/* Free a display type list */
void inst_del_disptype_list(inst_disptypesel *list, int no) {

	if (list != NULL) {
		int i;
		for (i = 0; i < no; i++) {
			if (list[i].path != NULL)
				free(list[i].path);
			if (list[i].sets != NULL)
				free(list[i].sets);
		}
		free(list);
	}
}

/* Ensure that the list is large enough for n+1 entries (+1 == end marker) */
/* Return NULL on malloc error */
static inst_disptypesel *expand_dlist(inst_disptypesel *list, int nlist, int *nalist) {
	
	if ((nlist+1) > *nalist) {
		int xnalist = (2 * nlist + 6);
		inst_disptypesel *xlist;

		if ((xlist = realloc(list, xnalist * sizeof(inst_disptypesel))) == NULL) {
			inst_del_disptype_list(list, nlist);
			*nalist = 0;
			return NULL;
		}
		*nalist = xnalist;
		list = xlist;
	}

	/* Set end marker */
	list[nlist].flags = inst_dtflags_end;
	list[nlist].cbid = 0;
	list[nlist].sel[0] = '\000';
	list[nlist].desc[0] = '\000';
	list[nlist].refr = 0;
	list[nlist].ix = 0;
	list[nlist].cc_cbid = 0;
	list[nlist].path = NULL;
	list[nlist].sets = NULL;
	list[nlist].no_sets = 0;

	return list;
}

/* For each selector we need to:

	check each selector char
	if already used,
		remove it.
	if no selector remain,
		allocate a free one.
	mark all used selectors

	We treat the first selector as more important
	than any aliases that come after it, so we need
	to do two passes to resolve what gets used.

	Use disptechs_set_sel() utility function to some of the work.
*/

/* Create the display type list */
inst_code inst_creat_disptype_list(inst *p,
int *pndtlist,					/* Number in returned list */
inst_disptypesel **pdtlist,		/* Returned list */
inst_disptypesel *sdtlist,		/* Static list */
int doccss,						/* Add installed ccss files */
int doccmx						/* Add matching installed ccmx files */
) {
	inst_disptypesel *list = NULL;
	int i, j, k, nlist = 0, nalist = 0;
	char usels[256];			/* Used selectors */
	static char *asels = "123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

	/* free the old list */
	inst_del_disptype_list(*pdtlist, *pndtlist);
	*pdtlist = NULL;
	*pndtlist = 0;

	for (i = 0; i < 256; i++)
		usels[i] = 0;
	k = 0;		/* Next selector index */

	/* Add entries from the static list and their primary selectors */
	/* Count the number in the static list */
	for (i = 0; !(sdtlist[i].flags & inst_dtflags_end); i++) {

		if ((list = expand_dlist(list, ++nlist, &nalist)) == NULL)
			return inst_internal_error;

		list[nlist-1] = sdtlist[i];		/* Struct copy */

		if (disptechs_set_sel(2, list[nlist-1].sel, usels, &k, asels)) {
			a1loge(p->log, 1, "inst_creat_disptype_list run out of selectors\n");
			break;
		}
	}

	/* Add any ccss's */
	if (doccss) {
		iccss *ss_list;
		if ((ss_list = list_iccss(NULL)) == NULL) {
			free(list);
			return inst_internal_error;
		}

		for (i = 0; ss_list[i].path != NULL; i++) {
	
			if ((list = expand_dlist(list, ++nlist, &nalist)) == NULL)
				return inst_internal_error;
	
			list[nlist-1].flags = inst_dtflags_ccss | inst_dtflags_ld | inst_dtflags_wr;

			if (ss_list[i].sel != NULL) {
				strncpy(list[nlist-1].sel, ss_list[i].sel, INST_DTYPE_SEL_LEN);
				list[nlist-1].sel[INST_DTYPE_SEL_LEN-1] = '\000';
			}
			if (disptechs_set_sel(2, list[nlist-1].sel, usels, &k, asels)) {
				a1loge(p->log, 1, "inst_creat_disptype_list run out of selectors\n");
				break;
			}

			strncpy(list[nlist-1].desc, ss_list[i].desc, INST_DTYPE_DESC_LEN);
			list[nlist-1].desc[INST_DTYPE_DESC_LEN-1] = '\000';
			list[nlist-1].dtech = ss_list[i].dtech;
			list[nlist-1].refr = ss_list[i].refr;
			list[nlist-1].ix = 0;
			list[nlist-1].path = ss_list[i].path; ss_list[i].path = NULL;
			list[nlist-1].cbid = 0;
			list[nlist-1].sets = ss_list[i].sets; ss_list[i].sets = NULL;
			list[nlist-1].no_sets = ss_list[i].no_sets; ss_list[i].no_sets = 0;
		}
	}

	/* Add any ccmx's */
	if (doccmx) {
		iccmx *ss_list;

		/* Just ccmx's for this instrument */
		if ((ss_list = list_iccmx(p->itype, NULL)) == NULL) {
			free(list);
			return inst_internal_error;
		}

		for (i = 0; ss_list[i].path != NULL; i++) {
	
			/* Check that there is a matching base calibation */
			for (j = 0; j < nlist; j++) {
				if (ss_list[i].cc_cbid != 0
				 && list[j].cbid == ss_list[i].cc_cbid)
					break;
			}
			if (j >= nlist) {
				a1loge(p->log, 1, "inst_creat_disptype_list can't find cbid %d for '%s'\n",ss_list[i].cc_cbid, ss_list[i].path);
				continue;
			}

			if ((list = expand_dlist(list, ++nlist, &nalist)) == NULL)
				return inst_internal_error;
	
			list[nlist-1].flags = inst_dtflags_ccmx | inst_dtflags_ld | inst_dtflags_wr;

			if (ss_list[i].sel != NULL) {
				strncpy(list[nlist-1].sel, ss_list[i].sel, INST_DTYPE_SEL_LEN);
				list[nlist-1].sel[INST_DTYPE_SEL_LEN-1] = '\000';
			}
			if (disptechs_set_sel(2, list[nlist-1].sel, usels, &k, asels)) {
				a1loge(p->log, 1, "inst_creat_disptype_list run out of selectors\n");
				break;
			}
			strncpy(list[nlist-1].desc, ss_list[i].desc, INST_DTYPE_DESC_LEN);
			list[nlist-1].desc[INST_DTYPE_DESC_LEN-1] = '\000';
			list[nlist-1].dtech = ss_list[i].dtech;
			list[nlist-1].refr = ss_list[i].refr;
			list[nlist-1].ix = list[j].ix; /* Copy underlying cal selection from base */
			list[nlist-1].path = ss_list[i].path; ss_list[i].path = NULL;
			list[nlist-1].cbid = 0;
			list[nlist-1].cc_cbid = ss_list[i].cc_cbid;
			icmCpy3x3(list[nlist-1].mat, ss_list[i].mat);
		}
	}

	/* Create needed selectors */
	for (i = 0; i < nlist; i++)
		disptechs_set_sel(4, list[i].sel, usels, &k, asels);

	/* Verify or delete any secondary selectors */
	for (i = 0; i < nlist; i++)
		disptechs_set_sel(3, list[i].sel, usels, &k, asels);

	if (pndtlist != NULL)
		*pndtlist = nlist;
	if (pdtlist != NULL)
		*pdtlist = list;

	return inst_ok;
}

/* ============================================================= */
/* CCMX location support */

/* return a list of installed ccmx files. */
/* if itype != instUnknown, return those that match the given instrument. */
/* The list is sorted by description and terminated by a NULL entry. */
/* If no is != NULL, return the number in the list */
/* Return NULL and -1 if there is a malloc error */
iccmx *list_iccmx(instType itype, int *no) {
	int i, j;
	iccmx *rv;

	char **paths = NULL;
	int npaths = 0;


	npaths = xdg_bds(NULL, &paths, xdg_data, xdg_read, xdg_user,
						"ArgyllCMS/\052.ccmx" XDG_FUDGE "color/\052.ccmx"
	);

	if ((rv = malloc(sizeof(iccmx) * (npaths + 1))) == NULL) {
		a1loge(g_log, 1, "list_iccmx: malloc of paths failed\n");
		xdg_free(paths, npaths);
		if (no != NULL) *no = -1;
		return NULL;
	}
	
	for (i = j = 0; i < npaths; i++) {
		ccmx *cs;
		int len;
		char *pp;
		disptech dtech;
		char *tech, *disp;
		int cc_cbid, refr;

		if ((cs = new_ccmx()) == NULL) {
			a1loge(g_log, 1, "list_iccmx: new_ccmx failed\n");
			for (--j; j>= 0; j--) {
				free(rv[j].path);
				free(rv[j].desc);
			}
			xdg_free(paths, npaths);
			if (no != NULL) *no = -1;
			return NULL;
		}
		if (cs->read_ccmx(cs, paths[i])) {
			cs->del(cs);
			continue;		/* Skip any unreadable ccmx's */
		}

		/* Skip any that don't match */
		if (itype != instUnknown && cs->inst != NULL && inst_enum(cs->inst) != itype)
			continue;

		a1logd(g_log, 5, "Reading '%s'\n",paths[i]);
		if ((tech = cs->tech) == NULL)
			tech = "";
		if ((disp = cs->disp) == NULL)
			disp = "";
		cc_cbid = cs->cc_cbid;
		dtech = cs->dtech;
		refr = cs->refrmode;
		len = strlen(tech) + strlen(disp) + 4;
		if ((pp = malloc(len)) == NULL) {
			a1loge(g_log, 1, "list_iccmx: malloc failed\n");
			for (--j; j >= 0; j--) {
				free(rv[j].path);
				free(rv[j].desc);
			}
			cs->del(cs);
			free(rv);
			xdg_free(paths, npaths);
			if (no != NULL) *no = -1;
			return NULL;
		}
		if ((rv[j].path = strdup(paths[i])) == NULL) {
			a1loge(g_log, 1, "list_iccmx: strdup failed\n");
			for (--j; j >= 0; j--) {
				free(rv[j].path);
				free(rv[j].desc);
			}
			cs->del(cs);
			free(rv);
			free(pp);
			xdg_free(paths, npaths);
			if (no != NULL) *no = -1;
			return NULL;
		}
		strcpy(pp, tech);
		strcat(pp, " (");
		strcat(pp, disp);
		strcat(pp, ")");
		rv[j].desc = pp;
		rv[j].cc_cbid = cc_cbid;
		rv[j].dtech = dtech;
		rv[j].refr = refr;
		rv[j].sel = cs->sel;  cs->sel = NULL;
		icmCpy3x3(rv[j].mat, cs->matrix);
		cs->del(cs);
		j++;
	}
	xdg_free(paths, npaths);
	rv[j].path = NULL;
	rv[j].desc = NULL;
	rv[j].cc_cbid = 0;
	rv[j].dtech = disptech_unknown;
	rv[j].refr = -1;
	rv[j].sel = NULL;
	if (no != NULL)
		*no = j;

	/* Sort the list */
#define HEAP_COMPARE(A,B) (strcmp(A.desc, B.desc) < 0) 
	HEAPSORT(iccmx, rv, j)
#undef HEAP_COMPARE

	return rv;
}

/* Free up a iccmx list */
void free_iccmx(iccmx *list) {
	int i;

	if (list != NULL) {
		for (i = 0; list[i].path != NULL || list[i].desc != NULL; i++) {
			if (list[i].path != NULL)
				free(list[i].path);
			if (list[i].desc != NULL)
				free(list[i].desc);
			if (list[i].sel != NULL)
				free(list[i].sel);
		}
		free(list);
	}
}

/* ============================================================= */
/* CCSS location support */

/* return a list of installed ccss files. */
/* The list is sorted by description and terminated by a NULL entry. */
/* If no is != NULL, return the number in the list */
/* Return NULL and -1 if there is a malloc error */
iccss *list_iccss(int *no) {
	int i, j;
	iccss *rv;

	char **paths = NULL;
	int npaths = 0;


	npaths = xdg_bds(NULL, &paths, xdg_data, xdg_read, xdg_user,
						"ArgyllCMS/\052.ccss" XDG_FUDGE "color/\052.ccss"
	);

	if ((rv = malloc(sizeof(iccss) * (npaths + 1))) == NULL) {
		a1loge(g_log, 1, "list_iccss: malloc of paths failed\n");
		xdg_free(paths, npaths);
		if (no != NULL) *no = -1;
		return NULL;
	}
	
	for (i = j = 0; i < npaths; i++) {
		ccss *cs;
		int len;
		char *pp;
		disptech dtech;
		char *tech, *disp;
		int refr;

		if ((cs = new_ccss()) == NULL) {
			a1loge(g_log, 1, "list_iccss: new_ccss failed\n");
			for (--j; j>= 0; j--) {
				free(rv[j].path);
				free(rv[j].desc);
			}
			xdg_free(paths, npaths);
			if (no != NULL) *no = -1;
			return NULL;
		}
		if (cs->read_ccss(cs, paths[i])) {
			cs->del(cs);
			continue;		/* Skip any unreadable ccss's */
		}

		a1logd(g_log, 5, "Reading '%s'\n",paths[i]);
		if ((tech = cs->tech) == NULL)
			tech = "";
		if ((disp = cs->disp) == NULL)
			disp = "";
		dtech = cs->dtech;
		refr = cs->refrmode;
		len = strlen(tech) + strlen(disp) + 4;
		if ((pp = malloc(len)) == NULL) {
			a1loge(g_log, 1, "list_iccss: malloc failed\n");
			for (--j; j >= 0; j--) {
				free(rv[j].path);
				free(rv[j].desc);
			}
			cs->del(cs);
			free(rv);
			xdg_free(paths, npaths);
			if (no != NULL) *no = -1;
			return NULL;
		}
		if ((rv[j].path = strdup(paths[i])) == NULL) {
			a1loge(g_log, 1, "list_iccss: strdup failed\n");
			for (--j; j >= 0; j--) {
				free(rv[j].path);
				free(rv[j].desc);
			}
			cs->del(cs);
			free(rv);
			free(pp);
			xdg_free(paths, npaths);
			if (no != NULL) *no = -1;
			return NULL;
		}
		strcpy(pp, tech);
		strcat(pp, " (");
		strcat(pp, disp);
		strcat(pp, ")");
		rv[j].desc = pp;
		rv[j].dtech = dtech;
		rv[j].refr = refr;
		rv[j].sel = cs->sel;  cs->sel = NULL;
		rv[j].sets = cs->samples;  cs->samples = NULL;
		rv[j].no_sets = cs->no_samp; cs->no_samp = 0;
		cs->del(cs);
		j++;
	}
	xdg_free(paths, npaths);
	rv[j].path = NULL;
	rv[j].desc = NULL;
	rv[j].dtech = disptech_unknown;
	rv[j].refr = -1;
	rv[j].sel = NULL;
	rv[j].sets = NULL;
	rv[j].no_sets = 0;
	if (no != NULL)
		*no = j;

	/* Sort the list */
#define HEAP_COMPARE(A,B) (strcmp(A.desc, B.desc) < 0) 
	HEAPSORT(iccss, rv, j)
#undef HEAP_COMPARE

	return rv;
}

/* Free up a iccss list */
void free_iccss(iccss *list) {
	int i;

	if (list != NULL) {
		for (i = 0; list[i].path != NULL || list[i].desc != NULL; i++) {
			if (list[i].path != NULL)
				free(list[i].path);
			if (list[i].desc != NULL)
				free(list[i].desc);
			if (list[i].sel != NULL)
				free(list[i].sel);
			if (list[i].sets != NULL)
				free(list[i].sets);
		}
		free(list);
	}
}

/* ============================================================= */
/* Detect fast serial instruments */

#ifdef ENABLE_FAST_SERIAL
static void hex2bin(char *buf, int len);

/* Heuristicly determine the instrument type for */
/* a fast serial connection, and instUnknown if not serial. */
/* Set it in icoms and also return it. */
instType fast_ser_inst_type(
	icoms *p,
	int tryhard,
	inst_code (*uicallback)(void *cntx, inst_ui_purp purp),		/* optional uicallback */
	void *cntx			/* Context for callback */
) {
	instType rv = instUnknown;
	char buf[100];
	baud_rate brt[] = { baud_921600, baud_115200, baud_38400, baud_9600, baud_nc };
	unsigned int etime;
	unsigned int i;
	int se, len;
	
	if (p->port_type(p) != icomt_serial
	 && p->port_type(p) != icomt_usbserial)
		return p->itype;

	/* The tick to give up on */
	etime = msec_time() + (long)(2000.0 + 0.5);

	a1logd(p->log, 1, "fser_inst_type: Trying different baud rates (%u msec to go)\n",etime - msec_time());

	/* Until we time out, find the correct baud rate */
	for (i = 0; msec_time() < etime; i++) {
		if (brt[i] == baud_nc) {
			i = 0;
			if (!tryhard)
				break;		/* try only once */
		}
		if ((se = p->set_ser_port(p, fc_none, brt[i], parity_none,
			                         stop_1, length_8)) != ICOM_OK) { 
			a1logd(p->log, 5, "fser_inst_type: set_ser_port failed with 0x%x\n",se);
			return instUnknown;		/* Give up */
		}

//		a1logd(p->log, 5, "brt = %d\n",brt[i]);

		if (brt[i] == baud_9600) {
			/* See if it's a Klein K10 */

			if ((se = p->write_read(p, "P0\r", 0, buf, 100, NULL, ">", 1, 0.100)) != inst_ok) {
				/* Check for user abort */
				if (uicallback != NULL) {
					inst_code ev;
					if ((ev = uicallback(cntx, inst_negcoms)) == inst_user_abort) {
						a1logd(p->log, 5, "fser_inst_type: User aborted\n");
						return instUnknown;
					}
				}
				continue;
			}
			len = strlen(buf);
	
			a1logd(p->log, 5, "len = %d\n",len);
	
			/* Is this a Klein K1/K8/K10 response ? */
			if (strncmp(buf, "P0K-1 ", 6) == 0
			 || strncmp(buf, "P0K-8 ", 6) == 0
			 || strncmp(buf, "P0K-10", 6) == 0
			 || strncmp(buf, "P0KV-10", 7) == 0) {
				rv = instKleinK10;
				break;
			}
		} else {
			/* See if it's a JETI specbos */
			if ((se = p->write_read(p, "*idn?\r", 0, buf, 100, NULL, "\r", 1, 0.100)) != inst_ok) {
				/* Check for user abort */
				if (uicallback != NULL) {
					inst_code ev;
					if ((ev = uicallback(cntx, inst_negcoms)) == inst_user_abort) {
						a1logd(p->log, 5, "fser_inst_type: User aborted\n");
						return instUnknown;
					}
				}
				continue;
			}
			len = strlen(buf);
	
			a1logd(p->log, 5, "len = %d\n",len);
	
			/* JETI specbos returns "JETI_SBXXXX", where XXXX is the instrument type, */
			/* except for the 1201 which returns "SB05" */
	
			/* Is this a JETI specbos 1201 response ? */
			if (strncmp(buf, "SB05", 4) == 0) {
//				a1logd(p->log, 5, "specbos1201\n");
				rv = instSpecbos1201;
				break;
			}
			/* Is this a JETI specbos XXXX response ? */
			if (len >= 11 && strncmp(buf, "JETI_SB", 7) == 0) {
//				a1logd(p->log, 5, "specbos\n");
				rv = instSpecbos;
				break;
			}
		}
	}

	if (rv == instUnknown
	 && msec_time() >= etime) {		/* We haven't established comms */
		a1logd(p->log, 5, "fser_inst_type: Failed to establish coms\n");
		p->itype = rv;
		return instUnknown;
	}

	a1logd(p->log, 5, "fser_inst_type: Instrument type is '%s'\n", inst_name(rv));

	p->itype = rv;

	return rv;
}

#endif /* ENABLE_FAST_SERIAL */

/* ============================================================= */
/* Detect serial instruments */

#ifdef ENABLE_SERIAL
static void hex2bin(char *buf, int len);

/* Heuristicly determine the instrument type for */
/* a serial connection, and instUnknown if not serial. */
/* Set it in icoms and also return it. */
static instType ser_inst_type(
	icoms *p,
	inst_code (*uicallback)(void *cntx, inst_ui_purp purp),		/* optional uicallback */
	void *cntx			/* Context for callback */
) {
	instType rv = instUnknown;
	char buf[100];
	baud_rate brt[] = { baud_9600, baud_19200, baud_4800, baud_2400,
	                     baud_1200, baud_38400, baud_57600, baud_115200,
	                     baud_600, baud_300, baud_110, baud_nc };
	unsigned int etime;
	unsigned int bi, i;
	int se, len, bread;
	int xrite = 0;
	int ss = 0;
	int so = 0;
	
#ifdef ENABLE_USB
	if (p->usbd != NULL || p->hidd != NULL)
		return p->itype;
#endif /* ENABLE_USB */

	bi = 0;

	/* The tick to give up on */
	etime = msec_time() + (long)(1000.0 * 20.0 + 0.5);

	a1logd(p->log, 1, "ser_inst_type: Trying different baud rates (%u msec to go)\n",etime - msec_time());

	/* Until we time out, find the correct baud rate */
	for (i = bi; msec_time() < etime; i++) {
		if (brt[i] == baud_nc)
			i = 0;
		if ((se = p->set_ser_port(p, fc_none, brt[i], parity_none,
			                         stop_1, length_8)) != ICOM_OK) { 
			a1logd(p->log, 5, "ser_inst_type: set_ser_port failed with 0x%x\n",se);
			return instUnknown;		/* Give up */
		}

//		a1logd(p->log, 5, "brt = %d\n",brt[i]);
		bread = 0;
		if ((se = p->write_read(p, ";D024\r\n", 0, buf, 100, &bread, "\r", 1, 0.5)) != inst_ok) {
			/* Check for user abort */
			if (uicallback != NULL) {
				inst_code ev;
				if ((ev = uicallback(cntx, inst_negcoms)) == inst_user_abort) {
					a1logd(p->log, 5, "ser_inst_type: User aborted\n");
					return instUnknown;
				}
			}
			if (bread == 0)
				continue;
		}
		len = strlen(buf);

//		a1logd(p->log, 5, "len = %d\n",len);
		if (len < 4)
			continue;

		/* Is this an X-Rite error value such as "<01>" ? */
		if (buf[0] == '<' && isdigit(buf[1]) && isdigit(buf[2]) && buf[3] == '>') {
//			a1logd(p->log, 5, "xrite\n");
			xrite = 1;
			break;
		}

		/* Is this a Spectrolino error resonse ? */
		if (len >= 5 && strncmp(buf, ":26", 3) == 0) {
//			a1logd(p->log, 5, "spectrolino\n");
			so = 1;
			break;
		}
		/* Is this a SpectroScan response ? */
		if (len >= 7 && strncmp(buf, ":D183", 5) == 0) {
//			a1logd(p->log, 5, "spectroscan\n");
			ss = 1;
			break;
		}
	}

	if (rv == instUnknown
	 && msec_time() >= etime) {		/* We haven't established comms */
		a1logd(p->log, 5, "ser_inst_type: Failed to establish coms\n");
		return instUnknown;
	}

	a1logd(p->log, 5, "ser_inst_type: Got coms with instrument\n");

	/* Spectrolino */
	if (so) {
		rv = instSpectrolino;
	}

	/* SpectroScan */
	if (ss) {
		rv = instSpectroScan;
		if ((se = p->write_read(p, ";D030\r\n", 0, buf, 100, NULL, "\n", 1, 1.5)) == 0)  {
			if (strlen(buf) >= 41) {
				hex2bin(&buf[5], 12);
//				a1logd(p->log, 5, "spectroscan type = '%s'\n",buf);
				if (strncmp(buf, ":D190SpectroScanT", 17) == 0)
					rv = instSpectroScanT;
			}
		}
	}
	if (xrite) {

		/* Get the X-Rite model and version number */
		if ((se = p->write_read(p, "SV\r\n", 0, buf, 100, NULL, ">", 1, 2.5)) != 0)
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

	a1logd(p->log, 5, "ser_inst_type: Instrument type is '%s'\n", inst_name(rv));

	p->close_port(p);	/* Or should we leave it open ?? */

	p->itype = rv;

	return rv;
}

#endif /* ENABLE_SERIAL */

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)

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

/* ============================================================= */
/* inst_mode persistent storage support */

/* Table listing and relating masks to symbols. */
/* Need to keep this in sync with inst.h */ 
struct {
	int mode;		/* Mode bits */
	char *sym;		/* 4 character symbol */
} inst_mode_sym[] = {
	{ inst_mode_reflection, inst_mode_reflection_sym },
	{ inst_mode_s_reflection, inst_mode_s_reflection_sym },
	{ inst_mode_transmission, inst_mode_transmission_sym },
	{ inst_mode_emission, inst_mode_emission_sym },

	{ inst_mode_spot, inst_mode_spot_sym },
	{ inst_mode_strip, inst_mode_strip_sym },
	{ inst_mode_xy, inst_mode_xy_sym },
	{ inst_mode_chart, inst_mode_chart_sym },
	{ inst_mode_ambient, inst_mode_ambient_sym },
	{ inst_mode_ambient_flash, inst_mode_ambient_flash_sym },
	{ inst_mode_tele, inst_mode_tele_sym },

	{ inst_mode_emis_nonadaptive, inst_mode_emis_nonadaptive_sys },
	{ inst_mode_ref_uv, inst_mode_ref_uv_sym },
	{ inst_mode_emis_refresh_ovd, inst_mode_emis_refresh_ovd_sym },
	{ inst_mode_emis_norefresh_ovd, inst_mode_emis_norefresh_ovd_sym },

	{ inst_mode_colorimeter, inst_mode_colorimeter_sym },
	{ inst_mode_spectral, inst_mode_spectral_sym },
	{ inst_mode_highres, inst_mode_highres_sym },

	{ inst_mode_calibration, inst_mode_calibration_sym },

	{ 0, NULL }
};

/* Return a string with a symbolic encoding of the mode flags */
void inst_mode_to_sym(char sym[MAX_INST_MODE_SYM_SZ], inst_mode mode) {
	int i;
	char *cp = sym;

	for (i = 0; inst_mode_sym[i].mode != 0; i++) {
		if (mode & inst_mode_sym[i].mode) {
			if (cp != sym)
				*cp++ = '_';
			strncpy(cp, inst_mode_sym[i].sym, 4);
			cp += 4;
		}
	}
	*cp++ = '\000';
}

/* Return a set of mode flags that correspondf to the symbolic encoding */
/* Return nz if a symbol wasn't recognized */
int sym_to_inst_mode(inst_mode *mode, const char *sym) {
	int i;
	const char *cp = sym;
	int rv = 0;

	for (*mode = 0;;) {
		if (cp[0] == '\000' || cp[1] == '\000' || cp[2] == '\000' || cp[3] == '\000')
			break;

		for (i = 0; inst_mode_sym[i].mode != 0; i++) {
			if (strncmp(inst_mode_sym[i].sym, cp, 4) == 0) {
				*mode |= inst_mode_sym[i].mode;
				break;
			}
		}
		if (inst_mode_sym[i].mode == 0)
			rv = 1;

		cp += 4;

		if (*cp == '_')
			cp++;
	}

	return rv;
}























