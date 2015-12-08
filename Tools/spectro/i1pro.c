
/* 
 * Argyll Color Correction System
 *
 * Gretag i1Monitor & i1Pro related functions
 *
 * Author: Graeme W. Gill
 * Date:   24/11/2006
 *
 * Copyright 2006 - 2014, Graeme W. Gill
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

/*
	TTBD

	There may be a bug in HiRes emissive calibration - if you do an initial
	cal. in normal, then switch to HiRes, no white plate cal is performed.
	Is it using a previous cal result for this ?

	Should add extra filter compensation support.

	Should alias projector mode to display mode ??

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
#include "rspl.h"
#else /* SALONEINSTLIB */
#include "sa_config.h"
#include "numsup.h"
#include "rspl1.h"
#endif /* SALONEINSTLIB */
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "i1pro.h"
#include "i1pro_imp.h"

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 5000		/* Maximum reading messagle reply size */

/* Convert a machine specific error code into an abstract inst code */
static inst_code i1pro_interp_code(i1pro *p, i1pro_code ec);

/* ------------------------------------------------------------------------ */

/* Establish communications with a I1DISP */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return I1PRO_COMS_FAIL on failure to establish communications */
static inst_code
i1pro_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	i1pro *p = (i1pro *) pp;
	int rsize, se;
	icomuflags usbflags = icomuf_none;
#ifdef __APPLE__
	/* If the X-Rite software has been installed, then there may */
	/* be a daemon process that has the device open. Kill that process off */
	/* so that we can open it here, before it re-spawns. */
	char *pnames[] = {
//			"i1iSisDeviceService",
			"i1ProDeviceService",
			NULL
	};
	int retries = 20;
#else /* !__APPLE__ */
	char **pnames = NULL;
	int retries = 0;
#endif /* !__APPLE__ */

	a1logd(p->log, 2, "i1pro_init_coms: called\n");

	if (p->icom->port_type(p->icom) != icomt_usb) {
		a1logd(p->log, 1, "i1pro_init_coms: wrong communications type for device!\n");
		return inst_coms_fail;
	}

	a1logd(p->log, 2, "i1pro_init_coms: about to init USB\n");

	/* Set config, interface, write end point, read end point, read quanta */
	/* ("serial" end points aren't used - the i1display uses USB control messages) */
	if ((se = p->icom->set_usb_port(p->icom, 1, 0x00, 0x00, usbflags, retries, pnames))
		                                                                    != ICOM_OK) { 
		a1logd(p->log, 1, "i1pro_init_coms: failed ICOM err 0x%x\n",se);
		return i1pro_interp_code(p, icoms2i1pro_err(se));
	}

	a1logd(p->log, 2, "i1pro_init_coms: init coms has suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

static inst_code
i1pro_determine_capabilities(i1pro *p) {

	/* Set the base Monitor/Pro capabilities mask */
	p->cap =  inst_mode_emis_spot
	       |  inst_mode_emis_tele
	       |  inst_mode_emis_strip		/* Also likely to be light table reading */
	       |  inst_mode_trans_spot		/* Support this manually using a light table */
	       |  inst_mode_trans_strip 
	       |  inst_mode_emis_nonadaptive
	       |  inst_mode_colorimeter
	       |  inst_mode_spectral
	       ;

	/* Set the Pro capabilities mask */
	if (p->itype == instI1Pro
	 || p->itype == instI1Pro2) {
		p->cap |= inst_mode_ref_spot
		       |  inst_mode_ref_strip
		       ;
	}

	/* Set the Pro2 capabilities mask */
	if (p->itype == instI1Pro2) {
		p->cap |= inst_mode_ref_uv
		       ;
	}

	if (i1pro_imp_highres(p))		/* This is static */
		p->cap |= inst_mode_highres;

	if (i1pro_imp_ambient(p)) {		/* This depends on the instrument */
		p->cap |= inst_mode_emis_ambient
		       |  inst_mode_emis_ambient_flash
		       ;
	}

	p->cap2 = inst2_prog_trig 
			| inst2_user_trig
			| inst2_user_switch_trig
			| inst2_bidi_scan
			| inst2_has_scan_toll
			| inst2_no_feedback
	        ;

	if (p->m != NULL) {
		i1proimp *m = (i1proimp *)p->m;
		i1pro_state *s = &m->ms[m->mmode];
		if (s->emiss) {
			p->cap2 |= inst2_meas_disp_update;
			p->cap2 |= inst2_emis_refr_meas;
		}
	}

	p->cap3 = inst3_none;

	return inst_ok;
}

/* Initialise the I1PRO */
/* return non-zero on an error, with inst error code */
static inst_code
i1pro_init_inst(inst *pp) {
	i1pro *p = (i1pro *)pp;
	i1pro_code ev = I1PRO_OK;

	a1logd(p->log, 2, "i1pro_init_inst: called\n");

	if (p->gotcoms == 0)
		return i1pro_interp_code(p, I1PRO_INT_NO_COMS);	/* Must establish coms before calling init */
	if ((ev = i1pro_imp_init(p)) != I1PRO_OK) {
		a1logd(p->log, 1, "i1pro_init_inst: failed with 0x%x\n",ev);
		return i1pro_interp_code(p, ev);
	}

	p->inited = 1;
	a1logd(p->log, 2, "i1pro_init_inst: instrument inited OK\n");

	/* Now it's initied, we can get true capabilities */
	i1pro_determine_capabilities(p);

	return i1pro_interp_code(p, ev);
}

static char *i1pro_get_serial_no(inst *pp) {
	i1pro *p = (i1pro *)pp;
	
	if (!pp->gotcoms)
		return "";
	if (!pp->inited)
		return "";

	return i1pro_imp_get_serial_no(p);
}

/* Read a set of strips */
static inst_code
i1pro_read_strip(
inst *pp,
char *name,			/* Strip name (7 chars) */
int npatch,			/* Number of patches in the pass */
char *pname,		/* Pass name (3 chars) */
int sguide,			/* Guide number */
double pwid,		/* Patch length in mm (DTP41) */
double gwid,		/* Gap length in mm (DTP41) */
double twid,		/* Trailer length in mm (DTP41T) */
ipatch *vals) {		/* Pointer to array of instrument patch values */
	i1pro *p = (i1pro *)pp;
	i1pro_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = i1pro_imp_measure(p, vals, npatch, 1);

	return i1pro_interp_code(p, rv);
}

/* Read a single sample */
static inst_code
i1pro_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* Clamp XYZ/Lab to be +ve */
	i1pro *p = (i1pro *)pp;
	i1pro_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = i1pro_imp_measure(p, val, 1, clamp);

	return i1pro_interp_code(p, rv);
}

/* Read an emissive refresh rate */
static inst_code
i1pro_read_refrate(
inst *pp,
double *ref_rate) {
	i1pro *p = (i1pro *)pp;
	i1pro_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (ref_rate != NULL)
		*ref_rate = 0.0;

	rv = i1pro_imp_meas_refrate(p, ref_rate);



	return i1pro_interp_code(p, rv);
}

/* Read the display update delay */
static inst_code
i1pro_meas_delay(
inst *pp,
int *pdispmsec,		/* Return display update delay in msec */
int *pinstmsec) {	/* Return instrument reaction time in msec */
	i1pro *p = (i1pro *)pp;
	i1pro_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = i1pro_imp_meas_delay(p, pdispmsec, pinstmsec);

	return i1pro_interp_code(p, rv);
}

/* Timestamp the white patch change during meas_delay() */
static inst_code i1pro_white_change(
inst *pp,
int init) {
	i1pro *p = (i1pro *)pp;

	return i1pro_imp_white_change(p, init);
}

/* Return needed and available inst_cal_type's */
static inst_code i1pro_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	i1pro *p = (i1pro *)pp;
	i1pro_code rv;

	rv = i1pro_imp_get_n_a_cals(p, pn_cals, pa_cals);
	return i1pro_interp_code(p, rv);
}

/* Request an instrument calibration. */
static inst_code i1pro_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	i1pro *p = (i1pro *)pp;
	i1pro_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = i1pro_imp_calibrate(p, calt, calc, id);
	return i1pro_interp_code(p, rv);
}

/* Instrument specific error codes interpretation */
static char *
i1pro_interp_error(inst *pp, i1pro_code ec) {
//	i1pro *p = (i1pro *)pp;
	ec &= inst_imask;
	switch (ec) {
		case I1PRO_INTERNAL_ERROR:
			return "Internal software error";
		case I1PRO_COMS_FAIL:
			return "Communications failure";
		case I1PRO_UNKNOWN_MODEL:
			return "Not an i1 Pro";
		case I1PRO_DATA_PARSE_ERROR:
			return "Data from i1 Display didn't parse as expected";

		case I1PRO_USER_ABORT:
			return "User abort";

		case I1PRO_USER_TRIG:
			return "User trigger";

		case I1PRO_UNSUPPORTED:
			return "Unsupported function";
		case I1PRO_CAL_SETUP:
			return "Calibration retry with correct setup is needed";

		case I1PRO_OK:
			return "No device error";

		case I1PRO_DATA_COUNT:
			return "EEProm data count unexpectedly small";
		case I1PRO_DATA_BUFSIZE:
			return "EEProm data buffer too small";
		case I1PRO_DATA_MAKE_KEY:
			return "EEProm data creating key failed";
		case I1PRO_DATA_MEMORY:
			return "EEProm memory alloc failure";
		case I1PRO_DATA_KEYNOTFOUND:
			return "EEProm key value wasn't found";
		case I1PRO_DATA_WRONGTYPE:
			return "EEProm key is the wrong type";
		case I1PRO_DATA_KEY_CORRUPT:
			return "EEProm key table seems to be corrupted";
		case I1PRO_DATA_KEY_COUNT:
			return "EEProm key table count is too big or small";
		case I1PRO_DATA_KEY_UNKNOWN:
			return "EEProm unknown key type";
		case I1PRO_DATA_KEY_MEMRANGE:
			return "EEProm key data is out of range of EEProm";
		case I1PRO_DATA_KEY_ENDMARK:
			return "EEProm end section marker was missing";

		case I1PRO_HW_HIGHPOWERFAIL:
			return "Failed to switch to high power mode";
		case I1PRO_HW_EE_SIZE:
			return "EEProm size is too small";
		case I1PRO_HW_EE_SHORTREAD:
			return "Read less bytes for EEProm read than expected";
		case I1PRO_HW_EE_SHORTWRITE:
			return "Wrote less bytes for EEProm write than expected";
		case I1PRO_HW_ME_SHORTREAD:
			return "Read less bytes for measurement read than expected";
		case I1PRO_HW_ME_ODDREAD:
			return "Read a number of bytes not a multiple of 256";
		case I1PRO_HW_SW_SHORTREAD:
			return "Read less bytes for Switch read than expected";
		case I1PRO_HW_LED_SHORTWRITE:
			return "Wrote fewer LED sequence bytes than expected";
		case I1PRO_HW_UNEX_SPECPARMS:
			return "Instrument has unexpected spectral parameters";
		case I1PRO_HW_CALIBINFO:
			return "Instrument calibration info is missing or corrupted";
		case I1PRO_WL_TOOLOW:
			return "Wavelength calibration reading is too low";
		case I1PRO_WL_SHAPE:
			return "Wavelength calibration reading shape is incorrect";
		case I1PRO_WL_ERR2BIG:
			return "Wavelength calibration correction is excessive";

		case I1PRO_RD_DARKREADINCONS:
			return "Dark calibration reading is inconsistent";
		case I1PRO_RD_SENSORSATURATED:
			return "Sensor is saturated";
		case I1PRO_RD_DARKNOTVALID:
			return "Dark reading is not valid (too light)";
		case I1PRO_RD_NEEDS_CAL:
			return "Mode needs calibration";
		case I1PRO_RD_WHITEREADINCONS:
			return "White calibration reading is inconsistent";
		case I1PRO_RD_WHITEREFERROR:
			return "White reference reading error";
		case I1PRO_RD_LIGHTTOOLOW:
			return "Light level is too low";
		case I1PRO_RD_LIGHTTOOHIGH:
			return "Light level is too high";
		case I1PRO_RD_SHORTMEAS:
			return "Reading is too short";
		case I1PRO_RD_READINCONS:
			return "Reading is inconsistent";
		case I1PRO_RD_TRANSWHITERANGE:
			return "Transmission white reference is out of range";
		case I1PRO_RD_NOTENOUGHPATCHES:
			return "Not enough patches";
		case I1PRO_RD_TOOMANYPATCHES:
			return "Too many patches";
		case I1PRO_RD_NOTENOUGHSAMPLES:
			return "Not enough samples per patch";
		case I1PRO_RD_NOFLASHES:
			return "No flashes recognized";
		case I1PRO_RD_NOAMBB4FLASHES:
			return "No ambient found before first flash";
		case I1PRO_RD_NOREFR_FOUND:
			return "No refresh rate detected or failed to measure it";
		case I1PRO_RD_NOTRANS_FOUND:
			return "No delay calibration transition found";

		case I1PRO_INT_NO_COMS:
			return "Communications hasn't been established";
		case I1PRO_INT_EETOOBIG:
			return "Read of EEProm is too big (> 65536)";
		case I1PRO_INT_ODDREADBUF:
			return "Measurement read buffer is not a multiple of reading size";
		case I1PRO_INT_SMALLREADBUF:
			return "Measurement read buffer is too small for initial measurement";
		case I1PRO_INT_INTTOOBIG:
			return "Integration time is too big";
		case I1PRO_INT_INTTOOSMALL:
			return "Integration time is too small";
		case I1PRO_INT_ILLEGALMODE:
			return "Illegal measurement mode selected";
		case I1PRO_INT_ZEROMEASURES:
			return "Number of measurements requested is zero";
		case I1PRO_INT_WRONGPATCHES:
			return "Number of patches to match is wrong";
		case I1PRO_INT_MEASBUFFTOOSMALL:
			return "Measurement exceeded read buffer";
		case I1PRO_INT_NOTIMPLEMENTED:
			return "Support not implemented";
		case I1PRO_INT_NOTCALIBRATED:
			return "Unexpectedely invalid calibration";
		case I1PRO_INT_NOINTERPDARK:
			return "Need interpolated dark and don't have it";
		case I1PRO_INT_THREADFAILED:
			return "Creation of thread failed";
		case I1PRO_INT_BUTTONTIMEOUT:
			return "Button status read timed out";
		case I1PRO_INT_CIECONVFAIL:
			return "Creating spectral to CIE converted failed";
		case I1PRO_INT_PREP_LOG_DATA:
			return "Error in preparing log data";
		case I1PRO_INT_MALLOC:
			return "Error in allocating memory";
		case I1PRO_INT_CREATE_EEPROM_STORE:
			return "Error in creating EEProm store";
		case I1PRO_INT_SAVE_SUBT_MODE:
			return "Can't save calibration if in subt mode";
		case I1PRO_INT_NO_CAL_TO_SAVE:
			return "No calibration data to save";
		case I1PRO_INT_EEPROM_DATA_MISSING:
			return "EEProm data is missing";
		case I1PRO_INT_NEW_RSPL_FAILED:
			return "Creating RSPL object faild";
		case I1PRO_INT_CAL_SAVE:
			return "Unable to save calibration to file";
		case I1PRO_INT_CAL_RESTORE:
			return "Unable to restore calibration from file";
		case I1PRO_INT_CAL_TOUCH:
			return "Unable to update calibration file modification time";
		case I1PRO_INT_ADARK_INVALID:
			return "Adaptive dark calibration is invalid";
		case I1PRO_INT_NO_HIGH_GAIN:
			return "Rev E mode doesn't have a high gain mode";
		case I1PRO_INT_ASSERT:
			return "Assert fail";

		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract inst code */
static inst_code 
i1pro_interp_code(i1pro *p, i1pro_code ec) {

	ec &= inst_imask;
	switch (ec) {

		case I1PRO_OK:
			return inst_ok;

		case I1PRO_INTERNAL_ERROR:
			return inst_internal_error | ec;

		case I1PRO_COMS_FAIL:
			return inst_coms_fail | ec;

		case I1PRO_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case I1PRO_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

		case I1PRO_USER_ABORT:
			return inst_user_abort;

		case I1PRO_USER_TRIG:
			return inst_user_trig;

		case I1PRO_UNSUPPORTED:
			return inst_unsupported | ec;

		case I1PRO_CAL_SETUP:
			return inst_cal_setup | ec;

		case I1PRO_HW_HIGHPOWERFAIL:
		case I1PRO_HW_EE_SHORTREAD:
		case I1PRO_HW_ME_SHORTREAD:
		case I1PRO_HW_ME_ODDREAD:
		case I1PRO_HW_CALIBINFO:
			return inst_hardware_fail | ec;

		case I1PRO_RD_DARKREADINCONS:
		case I1PRO_RD_SENSORSATURATED:
		case I1PRO_RD_DARKNOTVALID:
		case I1PRO_RD_WHITEREADINCONS:
		case I1PRO_RD_WHITEREFERROR:
		case I1PRO_RD_LIGHTTOOLOW:
		case I1PRO_RD_LIGHTTOOHIGH:
		case I1PRO_RD_SHORTMEAS:
		case I1PRO_RD_READINCONS:
		case I1PRO_RD_TRANSWHITERANGE:
		case I1PRO_RD_NOTENOUGHPATCHES:
		case I1PRO_RD_TOOMANYPATCHES:
		case I1PRO_RD_NOTENOUGHSAMPLES:
		case I1PRO_RD_NOFLASHES:
		case I1PRO_RD_NOAMBB4FLASHES:
		case I1PRO_RD_NOREFR_FOUND:
		case I1PRO_RD_NOTRANS_FOUND:
			return inst_misread | ec;

		case I1PRO_RD_NEEDS_CAL:
			return inst_needs_cal | ec;

		case I1PRO_INT_NO_COMS:
		case I1PRO_INT_EETOOBIG:
		case I1PRO_INT_ODDREADBUF:
		case I1PRO_INT_SMALLREADBUF:
		case I1PRO_INT_INTTOOBIG:
		case I1PRO_INT_INTTOOSMALL:
		case I1PRO_INT_ILLEGALMODE:
		case I1PRO_INT_ZEROMEASURES:
		case I1PRO_INT_MEASBUFFTOOSMALL:
		case I1PRO_INT_NOTIMPLEMENTED:
		case I1PRO_INT_NOTCALIBRATED:
		case I1PRO_INT_NOINTERPDARK:
		case I1PRO_INT_THREADFAILED:
		case I1PRO_INT_BUTTONTIMEOUT:
		case I1PRO_INT_CIECONVFAIL:
		case I1PRO_INT_PREP_LOG_DATA:
		case I1PRO_INT_MALLOC:
		case I1PRO_INT_CREATE_EEPROM_STORE:
		case I1PRO_INT_SAVE_SUBT_MODE:
		case I1PRO_INT_NO_CAL_TO_SAVE:
		case I1PRO_INT_EEPROM_DATA_MISSING:
		case I1PRO_INT_NEW_RSPL_FAILED:
		case I1PRO_INT_CAL_SAVE:
		case I1PRO_INT_CAL_RESTORE:
		case I1PRO_INT_CAL_TOUCH:
		case I1PRO_INT_ADARK_INVALID:
		case I1PRO_INT_NO_HIGH_GAIN:
		case I1PRO_INT_ASSERT:
			return inst_internal_error | ec;
	}
	return inst_other_error | ec;
}

/* Return the instrument capabilities */
static void i1pro_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	i1pro *p = (i1pro *)pp;

	if (pcap1 != NULL)
		*pcap1 = p->cap;
	if (pcap2 != NULL)
		*pcap2 = p->cap2;
	if (pcap3 != NULL)
		*pcap3 = p->cap3;
}

/* Return the corresponding i1pro measurement mode, */
/* or i1p_no_modes if invalid */
static i1p_mode i1pro_convert_mode(i1pro *p, inst_mode m) {
	i1p_mode mmode = 0;	/* Instrument measurement mode */

	/* Simple test */
	if (m & ~p->cap) {
		return i1p_no_modes;
	}

	if (IMODETST(m, inst_mode_ref_spot)) {
		mmode = i1p_refl_spot;
	} else if (IMODETST(m, inst_mode_ref_strip)) {
		mmode = i1p_refl_scan;
	} else if (IMODETST(m, inst_mode_trans_spot)) {
		mmode = i1p_trans_spot;
	} else if (IMODETST(m, inst_mode_trans_strip)) {
		mmode = i1p_trans_scan;
	} else if (IMODETST(m, inst_mode_emis_spot)
	        || IMODETST(m, inst_mode_emis_tele)) {
		if (IMODETST(m, inst_mode_emis_nonadaptive))
			mmode = i1p_emiss_spot_na;
		else
			mmode = i1p_emiss_spot;
	} else if (IMODETST(m, inst_mode_emis_strip)) {
		mmode = i1p_emiss_scan;
	} else if (IMODETST(m, inst_mode_emis_ambient)
	        && (p->cap & inst_mode_emis_ambient)) {
		mmode = i1p_amb_spot;
	} else if (IMODETST(m, inst_mode_emis_ambient_flash)
	        && (p->cap & inst_mode_emis_ambient_flash)) {
		mmode = i1p_amb_flash;
	} else {
		return i1p_no_modes;
	}

	return mmode;
}

/* Check device measurement mode */
static inst_code i1pro_check_mode(inst *pp, inst_mode m) {
	i1pro *p = (i1pro *)pp;
	i1p_mode mmode = 0;	/* Instrument measurement mode */

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (i1pro_convert_mode(p, m) == i1p_no_modes)
		return inst_unsupported;

	return inst_ok;
}

/* Set device measurement mode */
static inst_code i1pro_set_mode(inst *pp, inst_mode m) {
	i1pro *p = (i1pro *)pp;
	i1p_mode mmode;		/* Instrument measurement mode */
	inst_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if ((mmode = i1pro_convert_mode(p, m)) == i1p_no_modes)
		return inst_unsupported;

	if ((rv = i1pro_interp_code(p, i1pro_imp_set_mode(p, mmode, m))) != inst_ok) 
		return rv;

	i1pro_determine_capabilities(p);

	return inst_ok;
}

/* 
 * set or reset an optional mode
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 */
static inst_code
i1pro_get_set_opt(inst *pp, inst_opt_type m, ...)
{
	i1pro *p = (i1pro *)pp;

	if (m == inst_opt_initcalib) {		/* default */
		i1pro_set_noinitcalib(p, 0, 0);
		return inst_ok;

	} else if (m == inst_opt_noinitcalib) {
		va_list args;
		int losecs = 0;

		va_start(args, m);
		losecs = va_arg(args, int);
		va_end(args);

		i1pro_set_noinitcalib(p, 1, losecs);
		return inst_ok;

	/* Record the trigger mode */
	} else if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_user
	 || m == inst_opt_trig_user_switch) {
		i1pro_set_trig(p, m);
		return inst_ok;
	}

	if (m == inst_opt_scan_toll) {
		va_list args;
		double toll_ratio = 1.0;

		va_start(args, m);
		toll_ratio = va_arg(args, double);
		va_end(args);
		return i1pro_interp_code(p, i1pro_set_scan_toll(p, toll_ratio));
	}

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Not sure if this can be set before init */
	if (m == inst_opt_highres) {
		return i1pro_interp_code(p, i1pro_set_highres(p));
	} else if (m == inst_opt_stdres) {
		return i1pro_interp_code(p, i1pro_set_stdres(p));
	}

	/* Return the filter */
	if (m == inst_stat_get_filter) {
		i1proimp *imp = (i1proimp *)p->m;
		inst_opt_filter *filt;
		va_list args;

		va_start(args, m);
		filt = va_arg(args, inst_opt_filter *);
		va_end(args);

		*filt = inst_opt_filter_none;

		if (imp->physfilt == 0x82)
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
}

/* Destroy ourselves */
static void
i1pro_del(inst *pp) {
	i1pro *p = (i1pro *)pp;

	del_i1proimp(p);
	if (p->icom != NULL)
		p->icom->del(p->icom);
	p->vdel(pp);
	free(p);
}

/* Constructor */
extern i1pro *new_i1pro(icoms *icom, instType itype) {
	i1pro *p;
	int rv;
	if ((p = (i1pro *)calloc(sizeof(i1pro),1)) == NULL) {
		a1loge(icom->log, 1, "new_i1pro: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	/* Inst methods */
	p->init_coms         = i1pro_init_coms;
	p->init_inst         = i1pro_init_inst;
	p->capabilities      = i1pro_capabilities;
	p->get_serial_no     = i1pro_get_serial_no;
	p->check_mode        = i1pro_check_mode;
	p->set_mode          = i1pro_set_mode;
	p->get_set_opt       = i1pro_get_set_opt;
	p->read_strip        = i1pro_read_strip;
	p->read_sample       = i1pro_read_sample;
	p->read_refrate      = i1pro_read_refrate;
	p->get_n_a_cals      = i1pro_get_n_a_cals;
	p->calibrate         = i1pro_calibrate;
	p->meas_delay        = i1pro_meas_delay;
	p->white_change      = i1pro_white_change;
	p->interp_error      = i1pro_interp_error;
	p->del               = i1pro_del;

	p->icom = icom;
	p->itype = itype;

	i1pro_determine_capabilities(p);

	if ((rv = add_i1proimp(p)) != I1PRO_OK) {
		free(p);
		a1loge(icom->log, 1, "new_i1pro: error %d creating i1proimp\n",rv);
		return NULL;
	}

	return p;
}

