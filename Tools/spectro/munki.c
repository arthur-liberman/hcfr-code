
 /* X-Rite ColorMunki related functions */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   12/1/2009
 *
 * Copyright 2006 - 2014, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 * 
 * (Based on i1pro.c)
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
#include "munki.h"
#include "munki_imp.h"

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 5000		/* Maximum reading messagle reply size */

/* Convert a machine specific error code into an abstract dtp code */
static inst_code munki_interp_code(munki *p, munki_code ec);

/* ------------------------------------------------------------------------ */

/* Establish communications with a Munki */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
munki_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	munki *p = (munki *) pp;
	int se;
	icomuflags usbflags = icomuf_none;
#ifdef __APPLE__
	/* If the ColorMunki software has been installed, then there will */
	/* be a daemon process that has the device open. Kill that process off */
	/* so that we can open it here, before it re-spawns. */
	char *pnames[] = {
			"ninjad",
			"ColorMunkiDeviceService",
			NULL
	};
	int retries = 20;
#else /* !__APPLE__ */
	char **pnames = NULL;
	int retries = 0;
#endif /* !__APPLE__ */

	a1logd(p->log, 2, "munki_init_coms: called\n");

	if (p->icom->port_type(p->icom) != icomt_usb) {
		a1logd(p->log, 1, "munki_init_coms: wrong communications type for device!\n");
		return inst_coms_fail;
	}

	a1logd(p->log, 2, "munki_init_coms: about to init USB\n");

	/* Set config, interface, write end point, read end point, read quanta */
	/* ("serial" end points aren't used - the Munki uses USB control messages) */
	if ((se = p->icom->set_usb_port(p->icom, 1, 0x00, 0x00, usbflags, retries, pnames))
		                                                                   != ICOM_OK) { 
		a1logd(p->log, 1, "munki_init_coms: failed ICOM err 0x%x\n",se);
		return munki_interp_code(p, icoms2munki_err(se));
	}

	a1logd(p->log, 2, "munki_init_coms: init coms has suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

static inst_code
munki_determine_capabilities(munki *p) {

	/* Set the Munki capabilities mask */
	p->cap = inst_mode_ref_spot
	       | inst_mode_ref_strip
	       | inst_mode_emis_spot
	       | inst_mode_emis_tele
	       | inst_mode_trans_spot		/* Support this manually using a light table */
	       | inst_mode_trans_strip 
	       | inst_mode_emis_strip		/* Also likely to be light table reading */
		   | inst_mode_emis_ambient
		   | inst_mode_emis_ambient_flash
	       | inst_mode_emis_nonadaptive
	       | inst_mode_colorimeter
	       | inst_mode_spectral
	       | inst_mode_calibration		/* Can indicate cal configuration */
	       ;

	if (munki_imp_highres(p))		/* Static */
		p->cap |= inst_mode_highres;

	p->cap2 = inst2_prog_trig 
			| inst2_user_trig
			| inst2_user_switch_trig
			| inst2_bidi_scan
			| inst2_has_scan_toll
			| inst2_no_feedback
			| inst2_has_leds
			| inst2_has_sensmode
	        ;

	if (p->m != NULL) {
		munkiimp *m = (munkiimp *)p->m;
		munki_state *s = &m->ms[m->mmode];
		if (s->emiss) {
			p->cap2 |= inst2_emis_refr_meas;
			p->cap2 |= inst2_meas_disp_update;
		}
	}

	p->cap3 = inst3_none;

	return inst_ok;
}

/* Return current or given configuration available measurement modes. */
static inst_code munki_meas_config(
inst *pp,
inst_mode *mmodes,
inst_cal_cond *cconds,
int *conf_ix
) {
	munki *p = (munki *)pp;
	munki_code ev;
	mk_spos spos;

	if (mmodes != NULL)
		*mmodes = inst_mode_none;
	if (cconds != NULL)
		*cconds = inst_calc_none;

	if (conf_ix == NULL
	 || *conf_ix < mk_spos_proj
	 || *conf_ix > mk_spos_amb) {
		/* Return current configuration measrement modes */
		ev = munki_getstatus(p, &spos, NULL);
		if (ev != MUNKI_OK)
			return munki_interp_code(p, ev);
	} else {
		/* Return given configuration measurement modes */
		spos = *conf_ix;
	}

	if (spos == mk_spos_proj) {
		if (mmodes != NULL)
			*mmodes = inst_mode_emis_tele;
	} else if (spos == mk_spos_surf) {
		if (mmodes != NULL)
			*mmodes = inst_mode_ref_spot
			        | inst_mode_ref_strip
			        | inst_mode_emis_spot
			        | inst_mode_trans_spot
			        | inst_mode_trans_strip;
	} else if (spos == mk_spos_calib) {
		if (cconds != NULL)
			*cconds = inst_calc_man_cal_smode;
		if (mmodes != NULL)
			*mmodes = inst_mode_calibration;
	} else if (spos == mk_spos_amb) {
		if (mmodes != NULL)
			*mmodes = inst_mode_emis_ambient
			        | inst_mode_emis_ambient_flash;
	}

	/* Return configuration index returned */
	if (conf_ix != NULL)
		*conf_ix = (int)spos;

	/* Add the extra dependent and independent modes */
	if (mmodes != NULL)
		*mmodes |= inst_mode_emis_nonadaptive
		        | inst_mode_colorimeter
		        | inst_mode_spectral;

	return inst_ok;
}


/* Initialise the MUNKI */
/* return non-zero on an error, with dtp error code */
static inst_code
munki_init_inst(inst *pp) {
	munki *p = (munki *)pp;
	munki_code ev = MUNKI_OK;

	a1logd(p->log, 2, "munki_init_inst: called\n");

	if (p->gotcoms == 0)
		return munki_interp_code(p, MUNKI_INT_NO_COMS);	/* Must establish coms before calling init */
	if ((ev = munki_imp_init(p)) != MUNKI_OK) {
		a1logd(p->log, 1, "munki_init_inst: failed with 0x%x\n",ev);
		return munki_interp_code(p, ev);
	}

	p->inited = 1;
	a1logd(p->log, 2, "munki_init_inst: instrument inited OK\n");

	munki_determine_capabilities(p);

	return munki_interp_code(p, ev);
}

static char *munki_get_serial_no(inst *pp) {
	munki *p = (munki *)pp;

	if (!p->gotcoms)
		return "";
	if (!p->inited)
		return "";

	return munki_imp_get_serial_no(p);
}

/* Read a set of strips */
/* Return the dtp error code */
static inst_code
munki_read_strip(
inst *pp,
char *name,			/* Strip name (7 chars) */
int npatch,			/* Number of patches in the pass */
char *pname,		/* Pass name (3 chars) */
int sguide,			/* Guide number */
double pwid,		/* Patch length in mm (DTP41) */
double gwid,		/* Gap length in mm (DTP41) */
double twid,		/* Trailer length in mm (DTP41T) */
ipatch *vals) {		/* Pointer to array of instrument patch values */
	munki *p = (munki *)pp;
	munki_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = munki_imp_measure(p, vals, npatch, 1);

	return munki_interp_code(p, rv);
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
munki_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* Clamp XYZ/Lab to be +ve */
	munki *p = (munki *)pp;
	munki_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = munki_imp_measure(p, val, 1, clamp);

	return munki_interp_code(p, rv);
}

/* Read an emissive refresh rate */
static inst_code
munki_read_refrate(
inst *pp,
double *ref_rate) {
	munki *p = (munki *)pp;
	munki_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = munki_imp_meas_refrate(p, ref_rate);


	return munki_interp_code(p, rv);
}

/* Read the display update delay */
static inst_code
munki_meas_delay(
inst *pp,
int *msecdelay) {
	munki *p = (munki *)pp;
	munki_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = munki_imp_meas_delay(p, msecdelay);

	return munki_interp_code(p, rv);
}

/* Return needed and available inst_cal_type's */
static inst_code munki_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	munki *p = (munki *)pp;
	munki_code rv;

	rv = munki_imp_get_n_a_cals(p, pn_cals, pa_cals);
	return munki_interp_code(p, rv);
}

/* Request an instrument calibration. */
inst_code munki_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	munki *p = (munki *)pp;
	munki_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = munki_imp_calibrate(p, calt, calc, id);

	return munki_interp_code(p, rv);
}

/* Instrument specific error codes interpretation */
static char *
munki_interp_error(inst *pp, munki_code ec) {
//	munki *p = (munki *)pp;
	ec &= inst_imask;
	switch (ec) {
		case MUNKI_INTERNAL_ERROR:
			return "Internal software error";
		case MUNKI_COMS_FAIL:
			return "Communications failure";
		case MUNKI_UNKNOWN_MODEL:
			return "Not an i1 Pro";
		case MUNKI_DATA_PARSE_ERROR:
			return "Data from i1 Display didn't parse as expected";

		case MUNKI_USER_ABORT:
			return "User abort";

		case MUNKI_USER_TRIG:
			return "User trigger";

		case MUNKI_UNSUPPORTED:
			return "Unsupported function";
		case MUNKI_CAL_SETUP:
			return "Calibration retry with correct setup is needed";

		case MUNKI_OK:
			return "No device error";

		case MUNKI_DATA_RANGE:
			return "EEProm data count location out of range";
		case MUNKI_DATA_MEMORY:
			return "EEProm memory alloc failure";

		case MUNKI_HW_EE_SHORTREAD:
			return "Read less bytes for EEProm read than expected";
		case MUNKI_HW_ME_SHORTREAD:
			return "Read less bytes for measurement read than expected";
		case MUNKI_HW_ME_ODDREAD:
			return "Read a number of bytes not a multiple of 274";
		case MUNKI_HW_CALIBVERSION:
			return "Instrument calibration version is unknown";
		case MUNKI_HW_CALIBMATCH:
			return "Calibration doesn't match device";

		case MUNKI_RD_DARKREADINCONS:
			return "Dark calibration reading is inconsistent";
		case MUNKI_RD_SENSORSATURATED:
			return "Sensor is saturated";
		case MUNKI_RD_DARKNOTVALID:
			return "Dark reading is not valid (too light)";
		case MUNKI_RD_NEEDS_CAL:
			return "Mode needs calibration";
		case MUNKI_RD_WHITEREADINCONS:
			return "White calibration reading is inconsistent";
		case MUNKI_RD_WHITEREFERROR:
			return "White reference reading error";
		case MUNKI_RD_LIGHTTOOLOW:
			return "Light level is too low";
		case MUNKI_RD_LIGHTTOOHIGH:
			return "Light level is too high";
		case MUNKI_RD_SHORTMEAS:
			return "Reading is too short";
		case MUNKI_RD_READINCONS:
			return "Reading is inconsistent";
		case MUNKI_RD_REFWHITENOCONV:
			return "White reference calibration didn't converge";
		case MUNKI_RD_NOTENOUGHPATCHES:
			return "Not enough patches";
		case MUNKI_RD_TOOMANYPATCHES:
			return "Too many patches";
		case MUNKI_RD_NOTENOUGHSAMPLES:
			return "Not enough samples per patch";
		case MUNKI_RD_NOFLASHES:
			return "No flashes recognized";
		case MUNKI_RD_NOAMBB4FLASHES:
			return "No ambient found before first flash";
		case MUNKI_RD_NOREFR_FOUND:
			return "No refresh rate detected or failed to measure it";
		case MUNKI_RD_NOTRANS_FOUND:
			return "No delay calibration transition found";

		case MUNKI_SPOS_PROJ:
			return "Sensor should be in projector position";
		case MUNKI_SPOS_SURF:
			return "Sensor should be in surface position";
		case MUNKI_SPOS_CALIB:
			return "Sensor should be in calibration position";
		case MUNKI_SPOS_AMB:
			return "Sensor should be in ambient position";

		case MUNKI_INT_NO_COMS:
			return "Communications hasn't been established";
		case MUNKI_INT_EESIZE:
			return "EEProm is not the expected size";
		case MUNKI_INT_EEOUTOFRANGE:
			return "EEProm access is out of range";
		case MUNKI_INT_CALTOOSMALL:
			return "EEProm calibration data is too short";
		case MUNKI_INT_CALTOOBIG:
			return "EEProm calibration data is too long";
		case MUNKI_INT_CALBADCHSUM:
			return "Calibration data has a bad checksum";
		case MUNKI_INT_ODDREADBUF:
			return "Measurement read buffer is not a multiple of 274";
		case MUNKI_INT_INTTOOBIG:
			return "Integration time is too big";
		case MUNKI_INT_INTTOOSMALL:
			return "Integration time is too small";
		case MUNKI_INT_ILLEGALMODE:
			return "Illegal measurement mode selected";
		case MUNKI_INT_ZEROMEASURES:
			return "Number of measurements requested is zero";
		case MUNKI_INT_WRONGPATCHES:
			return "Number of patches to match is wrong";
		case MUNKI_INT_MEASBUFFTOOSMALL:
			return "Measurement exceeded read buffer";
		case MUNKI_INT_NOTIMPLEMENTED:
			return "Support not implemented";
		case MUNKI_INT_NOTCALIBRATED:
			return "Unexpectedely invalid calibration";
		case MUNKI_INT_THREADFAILED:
			return "Creation of thread failed";
		case MUNKI_INT_BUTTONTIMEOUT:
			return "Button status read timed out";
		case MUNKI_INT_CIECONVFAIL:
			return "Creating spectral to CIE converted failed";
		case MUNKI_INT_MALLOC:
			return "Error in allocating memory";
		case MUNKI_INT_CREATE_EEPROM_STORE:
			return "Error in creating EEProm store";
		case MUNKI_INT_NEW_RSPL_FAILED:
			return "Creating RSPL object faild";
		case MUNKI_INT_CAL_SAVE:
			return "Unable to save calibration to file";
		case MUNKI_INT_CAL_RESTORE:
			return "Unable to restore calibration from file";
		case MUNKI_INT_CAL_TOUCH:
			return "Unable to update calibration file modification time";
		case MUNKI_INT_ASSERT:
			return "Assert fail";
		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
munki_interp_code(munki *p, munki_code ec) {

	ec &= inst_imask;
	switch (ec) {

		case MUNKI_OK:

			return inst_ok;

		case MUNKI_COMS_FAIL:
			return inst_coms_fail | ec;

		case MUNKI_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case MUNKI_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

		case MUNKI_USER_ABORT:
			return inst_user_abort;

		case MUNKI_USER_TRIG:
			return inst_user_trig;

		case MUNKI_UNSUPPORTED:
			return inst_unsupported | ec;

		case MUNKI_RD_NEEDS_CAL:
			return inst_needs_cal | ec; 

		case MUNKI_CAL_SETUP:
		case MUNKI_SPOS_CALIB:
			return inst_cal_setup | ec;

		case MUNKI_SPOS_PROJ:
		case MUNKI_SPOS_SURF:
		case MUNKI_SPOS_AMB:
			return inst_wrong_config | ec;

		case MUNKI_DATA_RANGE:
		case MUNKI_DATA_MEMORY:
		case MUNKI_HW_EE_SHORTREAD:
		case MUNKI_HW_ME_SHORTREAD:
		case MUNKI_HW_ME_ODDREAD:
		case MUNKI_HW_CALIBVERSION:
		case MUNKI_HW_CALIBMATCH:
			return inst_hardware_fail | ec;

		case MUNKI_RD_DARKREADINCONS:
		case MUNKI_RD_SENSORSATURATED:
		case MUNKI_RD_DARKNOTVALID:
		case MUNKI_RD_WHITEREADINCONS:
		case MUNKI_RD_WHITEREFERROR:
		case MUNKI_RD_LIGHTTOOLOW:
		case MUNKI_RD_LIGHTTOOHIGH:
		case MUNKI_RD_SHORTMEAS:
		case MUNKI_RD_READINCONS:
		case MUNKI_RD_REFWHITENOCONV:
		case MUNKI_RD_NOTENOUGHPATCHES:
		case MUNKI_RD_TOOMANYPATCHES:
		case MUNKI_RD_NOTENOUGHSAMPLES:
		case MUNKI_RD_NOFLASHES:
		case MUNKI_RD_NOAMBB4FLASHES:
		case MUNKI_RD_NOREFR_FOUND:
		case MUNKI_RD_NOTRANS_FOUND:
			return inst_misread | ec;

		case MUNKI_INTERNAL_ERROR:
		case MUNKI_INT_NO_COMS:
		case MUNKI_INT_EESIZE:
		case MUNKI_INT_EEOUTOFRANGE:
		case MUNKI_INT_CALTOOSMALL:
		case MUNKI_INT_CALTOOBIG:
		case MUNKI_INT_CALBADCHSUM:
		case MUNKI_INT_ODDREADBUF:
		case MUNKI_INT_INTTOOBIG:
		case MUNKI_INT_INTTOOSMALL:
		case MUNKI_INT_ILLEGALMODE:
		case MUNKI_INT_ZEROMEASURES:
		case MUNKI_INT_MEASBUFFTOOSMALL:
		case MUNKI_INT_NOTIMPLEMENTED:
		case MUNKI_INT_NOTCALIBRATED:
		case MUNKI_INT_THREADFAILED:
		case MUNKI_INT_BUTTONTIMEOUT:
		case MUNKI_INT_CIECONVFAIL:
		case MUNKI_INT_MALLOC:
		case MUNKI_INT_CREATE_EEPROM_STORE:
		case MUNKI_INT_NEW_RSPL_FAILED:
		case MUNKI_INT_CAL_SAVE:
		case MUNKI_INT_CAL_RESTORE:
		case MUNKI_INT_CAL_TOUCH:
		case MUNKI_INT_WRONGPATCHES:
		case MUNKI_INT_ASSERT:
			return inst_internal_error | ec;
	}
	return inst_other_error | ec;
}

/* Convert instrument specific inst_wrong_config error to inst_config enum */
static inst_config munki_config_enum(inst *pp, int ec) {
//	munki *p = (munki *)pp;

	ec &= inst_imask;
	switch (ec) {

		case MUNKI_SPOS_PROJ:
			return inst_conf_projector;

		case MUNKI_SPOS_SURF:
			return inst_conf_surface;

		case MUNKI_SPOS_AMB:
			return inst_conf_ambient;

		case MUNKI_SPOS_CALIB:
			return inst_conf_calibration;
	}
	return inst_conf_unknown;
}

/* Return the instrument capabilities */
void munki_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	munki *p = (munki *)pp;

	if (pcap1 != NULL)
		*pcap1 = p->cap;
	if (pcap2 != NULL)
		*pcap2 = p->cap2;
	if (pcap3 != NULL)
		*pcap3 = p->cap3;

	return;
}

/* Return the corresponding munki measurement mode, */
/* or mk_no_modes if invalid */
static mk_mode munki_convert_mode(munki *p, inst_mode m) {
	mk_mode mmode = 0;

	/* Simple test */
	if (m & ~p->cap)
		return mk_no_modes;

	if (IMODETST(m, inst_mode_ref_spot)) {
		mmode = mk_refl_spot;
	} else if (IMODETST(m, inst_mode_ref_strip)) {
		mmode = mk_refl_scan;
	} else if (IMODETST(m, inst_mode_trans_spot)) {
		mmode = mk_trans_spot;
	} else if (IMODETST(m, inst_mode_trans_strip)) {
		mmode = mk_trans_scan;
	} else if (IMODETST(m, inst_mode_emis_spot)) {
		if (IMODETST(m, inst_mode_emis_nonadaptive))
			mmode = mk_emiss_spot_na;
		else
			mmode = mk_emiss_spot;
	} else if (IMODETST(m, inst_mode_emis_tele)) {
		if (IMODETST(m, inst_mode_emis_nonadaptive))
			mmode = mk_tele_spot_na;
		else
			mmode = mk_tele_spot;
	} else if (IMODETST(m, inst_mode_emis_strip)) {
		mmode = mk_emiss_scan;
	} else if (IMODETST(m, inst_mode_emis_ambient)) {
		mmode = mk_amb_spot;
	} else if (IMODETST(m, inst_mode_emis_ambient_flash)) {
		mmode = mk_amb_flash;
	} else {
		return mk_no_modes;
	}

	return mmode;
}

/* Check device measurement mode */
inst_code munki_check_mode(inst *pp, inst_mode m) {
	munki *p = (munki *)pp;
	mk_mode mmode = 0;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (munki_convert_mode(p, m) == mk_no_modes)
		return inst_unsupported;

	return inst_ok;
}

/* Set device measurement mode */
inst_code munki_set_mode(inst *pp, inst_mode m) {
	munki *p = (munki *)pp;
	mk_mode mmode = 0;
	inst_mode cap = p->cap;
	inst_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if ((mmode = munki_convert_mode(p, m)) == mk_no_modes)
		return inst_unsupported;
	
	if ((rv = munki_interp_code(p, munki_imp_set_mode(p, mmode, m))) != inst_ok)
		return rv;

	munki_determine_capabilities(p);

	return inst_ok;
}

/* 
 * set or reset an optional mode
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 */
static inst_code
munki_get_set_opt(inst *pp, inst_opt_type m, ...) {
	munki *p = (munki *)pp;

	if (m == inst_opt_noinitcalib) {
		va_list args;
		int losecs = 0;

		va_start(args, m);
		losecs = va_arg(args, int);
		va_end(args);

		munki_set_noinitcalib(p, 1, losecs);
		return inst_ok;

	} else if (m == inst_opt_initcalib) {
		munki_set_noinitcalib(p, 0, 0);
		return inst_ok;

	/* Record the trigger mode */
	} else if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_user
	 || m == inst_opt_trig_user_switch) {
		munki_set_trig(p, m);
		return inst_ok;
	}

	if (m == inst_opt_scan_toll) {
		va_list args;
		double toll_ratio = 1.0;

		va_start(args, m);
		toll_ratio = va_arg(args, double);
		va_end(args);
		return munki_interp_code(p, munki_set_scan_toll(p, toll_ratio));
	}

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Not sure if hires can be set before init */
	if (m == inst_opt_highres) {
		return munki_interp_code(p, munki_set_highres(p));
	} else if (m == inst_opt_stdres) {
		return munki_interp_code(p, munki_set_stdres(p));
	}

	if (m == inst_opt_get_gen_ledmask) {
		va_list args;
		int *mask = NULL;

		va_start(args, m);
		mask = va_arg(args, int *);
		va_end(args);
		*mask = 0x1;			/* One general LED */
		return inst_ok;
	} else if (m == inst_opt_set_led_state) {
		va_list args;
		int mask = 0;

		va_start(args, m);
		mask = 1 & va_arg(args, int);
		va_end(args);
		if (mask & 1) {
			p->led_period = 1.0;
			p->led_on_time_prop = 1.0;
			p->led_trans_time_prop = 0.0;
			return munki_interp_code(p, munki_setindled(p, 1000,0,0,-1,0));
		} else {
			p->led_period = 0.0;
			p->led_on_time_prop = 0.0;
			p->led_trans_time_prop = 0.0;
			return munki_interp_code(p, munki_setindled(p, 0,0,0,0,0));
		}
	} else if (m == inst_opt_get_led_state) {
		va_list args;
		int *mask = NULL;

		va_start(args, m);
		mask = va_arg(args, int *);
		va_end(args);
		if (mask != NULL) *mask = p->led_state;
		return inst_ok;
	}

	if (m == inst_opt_get_pulse_ledmask) {
		va_list args;
		int *mask = NULL;

		va_start(args, m);
		mask = va_arg(args, int *);
		va_end(args);
		*mask = 0x1;			/* General LED is pulsable */
		return inst_ok;
	} else if (m == inst_opt_set_led_pulse_state) {
		va_list args;
		double period, on_time_prop, trans_time_prop;
		int ontime, offtime, transtime, nopulses;

		va_start(args, m);
		period = va_arg(args, double);
		on_time_prop = va_arg(args, double);
		trans_time_prop = va_arg(args, double);
		va_end(args);
		if (period < 0.0
		 || on_time_prop < 0.0 || on_time_prop > 1.0
		 || trans_time_prop < 0.0 || trans_time_prop > 1.0
		 || trans_time_prop > on_time_prop || trans_time_prop > (1.0 - on_time_prop))
			return inst_bad_parameter;
		ontime = (int)(1000.0 * period * (on_time_prop - trans_time_prop) + 0.5); 
		offtime = (int)(1000.0 * period * (1.0 - on_time_prop - trans_time_prop) + 0.5); 
		transtime = (int)(1000.0 * period * trans_time_prop + 0.5); 
		nopulses = -1;
		if (period == 0.0 || on_time_prop == 0.0) {
			ontime = offtime = transtime = nopulses = 0;
			p->led_state = 0;
		} else {
			p->led_state = 1;
		}
		p->led_period = period;
		p->led_on_time_prop = on_time_prop;
		p->led_trans_time_prop = trans_time_prop;
		return munki_interp_code(p, munki_setindled(p, ontime,offtime,transtime,nopulses,0));
	} else if (m == inst_opt_get_led_state) {
		va_list args;
		double *period, *on_time_prop, *trans_time_prop;

		va_start(args, m);
		period = va_arg(args, double *);
		on_time_prop = va_arg(args, double *);
		trans_time_prop = va_arg(args, double *);
		va_end(args);
		if (period != NULL) *period = p->led_period;
		if (on_time_prop != NULL) *on_time_prop = p->led_on_time_prop;
		if (trans_time_prop != NULL) *trans_time_prop = p->led_trans_time_prop;
		return inst_ok;
	}

	/* Return the filter */
	if (m == inst_stat_get_filter) {
		inst_opt_filter *filt;
		va_list args;

		va_start(args, m);
		filt = va_arg(args, inst_opt_filter *);
		va_end(args);

		/* The ColorMunki is always UV cut */
		*filt = inst_opt_filter_UVCut;

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
	return inst_unsupported;
}

/* Destroy ourselves */
static void
munki_del(inst *pp) {
	munki *p = (munki *)pp;

	del_munkiimp(p);
	if (p->icom != NULL)
		p->icom->del(p->icom);
	free(p);
}

/* Constructor */
extern munki *new_munki(icoms *icom, instType itype) {
	munki *p;
	int rv;
	if ((p = (munki *)calloc(sizeof(munki),1)) == NULL) {
		a1loge(icom->log, 1, "new_munki: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	/* Inst methods */
	p->init_coms         = munki_init_coms;
	p->init_inst         = munki_init_inst;
	p->capabilities      = munki_capabilities;
	p->meas_config       = munki_meas_config;
	p->get_serial_no     = munki_get_serial_no;
	p->check_mode        = munki_check_mode;
	p->set_mode          = munki_set_mode;
	p->get_set_opt       = munki_get_set_opt;
	p->read_strip        = munki_read_strip;
	p->read_sample       = munki_read_sample;
	p->read_refrate      = munki_read_refrate;
	p->get_n_a_cals      = munki_get_n_a_cals;
	p->calibrate         = munki_calibrate;
	p->meas_delay        = munki_meas_delay;
	p->interp_error      = munki_interp_error;
	p->config_enum       = munki_config_enum;
	p->del               = munki_del;

	p->icom = icom;
	p->itype = icom->itype;

	/* Preliminary capabilities */
	munki_determine_capabilities(p);

	if ((rv = add_munkiimp(p) != MUNKI_OK)) {
		free(p);
		a1loge(icom->log, 1, "new_munki: error %d creating munkiimp\n",rv);
	}

	return p;
}

