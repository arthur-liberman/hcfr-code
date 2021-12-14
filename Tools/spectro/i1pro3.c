
/* 
 * Argyll Color Management System
 *
 * X-Rite i1Pro3 related functions
 */

/*
 * Author: Graeme W. Gill
 * Date:   14/9/2020
 *
 * Copyright 2006 - 2021, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* 
   If you make use of the instrument driver code here, please note
   that it is the author(s) of the code who are responsibility
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
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "i1pro3.h"
#include "i1pro3_imp.h"

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 5000		/* Maximum reading messagle reply size */

/* Convert a machine specific error code into an abstract inst code */
static inst_code i1pro3_interp_code(i1pro3 *p, i1pro3_code ec);

/* ------------------------------------------------------------------------ */

/* Establish communications with a I1DISP */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return I1PRO3_COMS_FAIL on failure to establish communications */
static inst_code
i1pro3_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	i1pro3 *p = (i1pro3 *) pp;
	int rsize, se;
	icomuflags usbflags = icomuf_none;
#ifdef UNIX_APPLE
	/* If the X-Rite software has been installed, then there may */
	/* be a daemon process that has the device open. Kill that process off */
	/* so that we can open it here, before it re-spawns. */
	char *pnames[] = {
//			"i1iSisDeviceService",
			"i1ProDeviceService",
			NULL
	};
	int retries = 20;
#else /* !UNIX_APPLE */
	char **pnames = NULL;
	int retries = 0;
#endif /* !UNIX_APPLE */

	a1logd(p->log, 2, "i1pro3_init_coms: called\n");


	if (p->icom->port_type(p->icom) != icomt_usb) {
		a1logd(p->log, 1, "i1pro3_init_coms: wrong communications type for device!\n");
		return inst_coms_fail;
	}

	a1logd(p->log, 2, "i1pro3_init_coms: about to init USB\n");

	/* Set config, interface, write end point, read end point, read quanta */
	/* ("serial" end points aren't used - the i1display uses USB control messages) */
	if ((se = p->icom->set_usb_port(p->icom, 1, 0x00, 0x00, usbflags, retries, pnames))
		                                                                    != ICOM_OK) { 
		a1logd(p->log, 1, "i1pro3_init_coms: failed ICOM err 0x%x\n",se);
		return i1pro3_interp_code(p, icoms2i1pro3_err(se));
	}

	a1logd(p->log, 2, "i1pro3_init_coms: init coms has suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

static inst_code
i1pro3_determine_capabilities(i1pro3 *p) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1pro3_state *s = m != NULL ? &m->ms[m->mmode] : NULL;

	p->cap =  inst_mode_emis_spot
	       |  inst_mode_emis_tele
	       |  inst_mode_emis_strip		/* Also likely to be light table reading */
	       |  inst_mode_trans_spot		/* Support this manually using a light table */
	       |  inst_mode_trans_strip 
	       |  inst_mode_emis_nonadaptive
	       |  inst_mode_colorimeter
	       |  inst_mode_spectral
		   |  inst_mode_ref_spot
		   |  inst_mode_ref_strip
		   ;

	if (i1pro3_imp_highres(p)) {		/* This is static */
		if (s != NULL && !s->reflective)
			p->cap |= inst_mode_highres;
	}

	if (m != NULL && m->capabilities & I1PRO3_CAP_AMBIENT) {
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

	if (s != NULL && s->emiss) {
		p->cap2 |= inst2_meas_disp_update;
		p->cap2 |= inst2_emis_refr_meas;
	}

	if (s != NULL && s->reflective) {
		p->cap3 = inst3_filter_none			/* M0 */
		        | inst3_filter_D50			/* M1 */
		        | inst3_filter_UVCut		/* M2 */
		        ;

		if (m->capabilities & I1PRO3_CAP_POL)
			p->cap3 |= inst3_filter_pol;	/* M3 */
	}

	return inst_ok;
}

/* Initialise the I1PRO3 */
/* return non-zero on an error, with inst error code */
static inst_code
i1pro3_init_inst(inst *pp) {
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3_code ev = I1PRO3_OK;

	a1logd(p->log, 2, "i1pro3_init_inst: called\n");

	if (p->gotcoms == 0)
		return i1pro3_interp_code(p, I1PRO3_INT_NO_COMS);	/* Must establish coms before calling init */
	if ((ev = i1pro3_imp_init(p)) != I1PRO3_OK) {
		a1logd(p->log, 1, "i1pro3_init_inst: failed with 0x%x\n",ev);
		return i1pro3_interp_code(p, ev);
	}

	p->inited = 1;
	a1logd(p->log, 2, "i1pro3_init_inst: instrument inited OK\n");

	/* Now it's initied, we can get true capabilities */
	i1pro3_determine_capabilities(p);

	return i1pro3_interp_code(p, ev);
}

static char *i1pro3_get_serial_no(inst *pp) {
	i1pro3 *p = (i1pro3 *)pp;
	
	if (!pp->gotcoms)
		return "";
	if (!pp->inited)
		return "";

	return i1pro3_imp_get_serial_no(p);
}

/* Read a set of strips */
static inst_code
i1pro3_read_strip(
inst *pp,
char *name,			/* Strip name (7 chars) */
int npatch,			/* Number of patches in the pass */
char *pname,		/* Pass name (3 chars) */
int sguide,			/* Guide number */
double pwid,		/* Patch length in mm (DTP41) */
double gwid,		/* Gap length in mm (DTP41) */
double twid,		/* Trailer length in mm (DTP41T) */
ipatch *vals) {		/* Pointer to array of instrument patch values */
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = i1pro3_imp_measure(p, vals, npatch, instClamp);

	return i1pro3_interp_code(p, rv);
}

/* Read a single sample */
static inst_code
i1pro3_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* Clamp XYZ/Lab to be +ve */
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = i1pro3_imp_measure(p, val, 1, clamp);

	return i1pro3_interp_code(p, rv);
}

/* Read an emissive refresh rate */
static inst_code
i1pro3_read_refrate(
inst *pp,
double *ref_rate) {
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (ref_rate != NULL)
		*ref_rate = 0.0;

	rv = i1pro3_imp_meas_refrate(p, ref_rate);



	return i1pro3_interp_code(p, rv);
}

/* Read the display update delay */
static inst_code
i1pro3_meas_delay(
inst *pp,
int *pdispmsec,		/* Return display update delay in msec */
int *pinstmsec) {	/* Return instrument reaction time in msec */
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = i1pro3_imp_meas_delay(p, pdispmsec, pinstmsec);

	return i1pro3_interp_code(p, rv);
}

/* Timestamp the white patch change during meas_delay() */
static inst_code i1pro3_white_change(
inst *pp,
int init) {
	i1pro3 *p = (i1pro3 *)pp;

	return i1pro3_imp_white_change(p, init);
}

/* Return needed and available inst_cal_type's */
static inst_code i1pro3_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3_code rv;

	rv = i1pro3_imp_get_n_a_cals(p, pn_cals, pa_cals);
	return i1pro3_interp_code(p, rv);
}

/* Request an instrument calibration. */
static inst_code i1pro3_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
inst_calc_id_type *idtype,	/* Condition identifier type */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = i1pro3_imp_calibrate(p, calt, calc, idtype, id);
	return i1pro3_interp_code(p, rv);
}

/* Instrument specific error codes interpretation */
static char *
i1pro3_interp_error(inst *pp, i1pro3_code ec) {
//	i1pro3 *p = (i1pro3 *)pp;
	static char buf[100];

	ec &= inst_imask;
	switch (ec) {
		case I1PRO3_INTERNAL_ERROR:
			return "Internal software error";
		case I1PRO3_COMS_FAIL:
			return "Communications failure";
		case I1PRO3_UNKNOWN_MODEL:
			return "Not an i1 Pro";
		case I1PRO3_DATA_PARSE_ERROR:
			return "Data from i1 Display didn't parse as expected";

		case I1PRO3_USER_ABORT:
			return "User abort";

		case I1PRO3_USER_TRIG:
			return "User trigger";

		case I1PRO3_UNSUPPORTED:
			return "Unsupported function";
		case I1PRO3_CAL_SETUP:
			return "Calibration retry with correct setup is needed";
		case I1PRO3_CAL_TRANSWHITEWARN:
			return "Transmission white is too low at some wavelengths";

		case I1PRO3_OK:
			return "No device error";

		case I1PRO3_HW_EE_VERSION:
			return "EEProm format is unknown version";
		case I1PRO3_HW_EE_CHKSUM:
			return "EEProm has a bad checksum";
		case I1PRO3_HW_EE_RANGE:
			return "EEProm attempt to read outside buffer range";
		case I1PRO3_HW_EE_FORMAT:
			return "EEProm seems corrupt";
		case I1PRO3_HW_EE_CHIPID:
			return "HW ChipId doesn't match EE ChipID";
		case I1PRO3_HW_EE_SHORTREAD:
			return "Read less bytes for EEProm read than expected";
		case I1PRO3_HW_ME_SHORTREAD:
			return "Read less bytes for measurement read than expected";
		case I1PRO3_HW_SW_SHORTREAD:
			return "Read less bytes for Switch read than expected";
		case I1PRO3_HW_LED_SHORTWRITE:
			return "Wrote fewer LED sequence bytes than expected";
		case I1PRO3_WL_TOOLOW:
			return "Wavelength calibration reading is too low";
		case I1PRO3_WL_SHAPE:
			return "Wavelength calibration reading shape is incorrect";
		case I1PRO3_WL_ERR2BIG:
			return "Wavelength calibration correction is excessive";

		case I1PRO3_SPOS_CAL:
			return "Standard adapter should be fitted and instrument placed on calibration tile";
		case I1PRO3_SPOS_STD:
			return "Standard surface measurement adapter should be fitted";
		case I1PRO3_SPOS_AMB:
			return "Ambient measurement adapter should be fitted";
		case I1PRO3_SPOS_POLCAL:
			return "Polarization filter should be fitted and instrument placed on calibration tile";
		case I1PRO3_SPOS_POL:
			return "Polarization filter should be fitted";

		case I1PRO3_RD_SENSORSATURATED:
			return "Sensor is saturated";
		case I1PRO3_RD_DARKNOTVALID:
			return "Dark reading is not valid (too light)";
		case I1PRO3_RD_NEEDS_CAL:
			return "Mode needs calibration";
		case I1PRO3_RD_WHITEREADINCONS:
			return "White calibration reading is inconsistent";
		case I1PRO3_RD_SHORTMEAS:
			return "Reading is too short";
		case I1PRO3_RD_READINCONS:
			return "Reading is inconsistent";
		case I1PRO3_RD_TRANSWHITELEVEL:
			return "Transmission white reference is too low";
		case I1PRO3_RD_NOTENOUGHPATCHES:
			return "Not enough patches";
		case I1PRO3_RD_TOOMANYPATCHES:
			return "Too many patches";
		case I1PRO3_RD_NOTENOUGHSAMPLES:
			return "Not enough samples per patch - Slow Down!";
		case I1PRO3_RD_NOFLASHES:
			return "No flashes recognized";
		case I1PRO3_RD_NOAMBB4FLASHES:
			return "No ambient found before first flash";
		case I1PRO3_RD_NOREFR_FOUND:
			return "No refresh rate detected or failed to measure it";
		case I1PRO3_RD_NOTRANS_FOUND:
			return "No delay calibration transition found";

		case I1PRO3_INT_NO_COMS:
			return "Communications hasn't been established";
		case I1PRO3_INT_EETOOBIG:
			return "Read of EEProm is too big";
		case I1PRO3_INT_ODDREADBUF:
			return "Measurement read buffer is not a multiple of reading size";
		case I1PRO3_INT_ILLEGALMODE:
			return "Illegal measurement mode selected";
		case I1PRO3_INT_WRONGMODE:
			return "In wrong measurement mode";
		case I1PRO3_INT_ZEROMEASURES:
			return "Number of measurements requested is zero";
		case I1PRO3_INT_WRONGPATCHES:
			return "Number of patches to match is wrong";
		case I1PRO3_INT_MEASBUFFTOOSMALL:
			return "Measurement exceeded read buffer";
		case I1PRO3_INT_NOTIMPLEMENTED:
			return "Support not implemented";
		case I1PRO3_INT_NOTCALIBRATED:
			return "Unexpectedely invalid calibration";
		case I1PRO3_INT_THREADFAILED:
			return "Creation of thread failed";
		case I1PRO3_INT_BUTTONTIMEOUT:
			return "Button status read timed out";
		case I1PRO3_INT_CIECONVFAIL:
			return "Creating spectral to CIE converted failed";
		case I1PRO3_INT_MALLOC:
			return "Error in allocating memory";
		case I1PRO3_INT_CREATE_EEPROM_STORE:
			return "Error in creating EEProm store";
		case I1PRO3_INT_CAL_SAVE:
			return "Unable to save calibration to file";
		case I1PRO3_INT_CAL_RESTORE:
			return "Unable to restore calibration from file";
		case I1PRO3_INT_CAL_TOUCH:
			return "Unable to update calibration file modification time";
		case I1PRO3_INT_ASSERT:
			return "Assert fail";

		default:
			sprintf(buf, "Unknown error code 0x%x",ec);
			return buf;
	}
}


/* Convert a machine specific error code into an abstract inst code */
static inst_code 
i1pro3_interp_code(i1pro3 *p, i1pro3_code ec) {
	ec &= inst_imask;
	switch (ec) {

		case I1PRO3_OK:
			return inst_ok;

		case I1PRO3_INTERNAL_ERROR:
			return inst_internal_error | ec;

		case I1PRO3_COMS_FAIL:
			return inst_coms_fail | ec;

		case I1PRO3_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case I1PRO3_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

		case I1PRO3_USER_ABORT:
			return inst_user_abort;

		case I1PRO3_USER_TRIG:
			return inst_user_trig;

		case I1PRO3_UNSUPPORTED:
			return inst_unsupported | ec;

		case I1PRO3_CAL_SETUP:
			return inst_cal_setup | ec;

		case I1PRO3_CAL_TRANSWHITEWARN:
			return inst_warning | ec;

		case I1PRO3_HW_EE_VERSION:
		case I1PRO3_HW_EE_CHKSUM:
		case I1PRO3_HW_EE_RANGE:
		case I1PRO3_HW_EE_FORMAT:
		case I1PRO3_HW_EE_CHIPID:
		case I1PRO3_HW_EE_SHORTREAD:
		case I1PRO3_HW_ME_SHORTREAD:
		case I1PRO3_HW_SW_SHORTREAD:
		case I1PRO3_HW_LED_SHORTWRITE:
		case I1PRO3_WL_TOOLOW:
		case I1PRO3_WL_SHAPE:
		case I1PRO3_WL_ERR2BIG:
			return inst_hardware_fail | ec;

		case I1PRO3_SPOS_CAL:
		case I1PRO3_SPOS_STD:
		case I1PRO3_SPOS_AMB:
		case I1PRO3_SPOS_POLCAL:
		case I1PRO3_SPOS_POL:
			return inst_wrong_config | ec;

		case I1PRO3_RD_SENSORSATURATED:
		case I1PRO3_RD_DARKNOTVALID:
		case I1PRO3_RD_WHITEREADINCONS:
		case I1PRO3_RD_SHORTMEAS:
		case I1PRO3_RD_READINCONS:
		case I1PRO3_RD_TRANSWHITELEVEL:
		case I1PRO3_RD_NOTENOUGHPATCHES:
		case I1PRO3_RD_TOOMANYPATCHES:
		case I1PRO3_RD_NOTENOUGHSAMPLES:
		case I1PRO3_RD_NOFLASHES:
		case I1PRO3_RD_NOAMBB4FLASHES:
		case I1PRO3_RD_NOREFR_FOUND:
		case I1PRO3_RD_NOTRANS_FOUND:
			return inst_misread | ec;

		case I1PRO3_RD_NEEDS_CAL:
			return inst_needs_cal | ec;

		case I1PRO3_INT_NO_COMS:
		case I1PRO3_INT_EETOOBIG:
		case I1PRO3_INT_ODDREADBUF:
		case I1PRO3_INT_ILLEGALMODE:
		case I1PRO3_INT_WRONGMODE:
		case I1PRO3_INT_ZEROMEASURES:
		case I1PRO3_INT_WRONGPATCHES:
		case I1PRO3_INT_MEASBUFFTOOSMALL:
		case I1PRO3_INT_NOTIMPLEMENTED:
		case I1PRO3_INT_NOTCALIBRATED:
		case I1PRO3_INT_THREADFAILED:
		case I1PRO3_INT_BUTTONTIMEOUT:
		case I1PRO3_INT_CIECONVFAIL:
		case I1PRO3_INT_MALLOC:
		case I1PRO3_INT_CREATE_EEPROM_STORE:
		case I1PRO3_INT_CAL_SAVE:
		case I1PRO3_INT_CAL_RESTORE:
		case I1PRO3_INT_CAL_TOUCH:
		case I1PRO3_INT_ASSERT:
			return inst_internal_error | ec;
	}
	return inst_other_error | ec;
}

/* Convert instrument specific inst_wrong_config error to inst_config enum */
static inst_config i1pro3_config_enum(inst *pp, int ec) {

	ec &= inst_imask;
	switch (ec) {

		case I1PRO3_SPOS_CAL:
			return inst_conf_calibration;

		case I1PRO3_SPOS_STD:
			return inst_conf_surface;

		case I1PRO3_SPOS_AMB:
			return inst_conf_ambient;

		case I1PRO3_SPOS_POLCAL:
			return inst_conf_polarized_calibration;

		case I1PRO3_SPOS_POL:
			return inst_conf_surface_polarized;

	}
	return inst_conf_unknown;
}

/* Return the instrument capabilities */
static void i1pro3_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	i1pro3 *p = (i1pro3 *)pp;

	if (pcap1 != NULL)
		*pcap1 = p->cap;
	if (pcap2 != NULL)
		*pcap2 = p->cap2;
	if (pcap3 != NULL)
		*pcap3 = p->cap3;
}

/* Return the corresponding i1pro3 measurement mode, */
/* or i1p3_no_modes if invalid */
static i1p3_mode i1pro3_convert_mode(i1pro3 *p, inst_mode im) {
	i1pro3imp *m = (i1pro3imp *)p->m;
	i1p3_mode mmode = 0;	/* Instrument measurement mode */

	/* Simple test */
	if (im & ~p->cap) {
		return i1p3_no_modes;
	}

	if (IMODETST(im, inst_mode_ref_spot)) {
		if (m->filt == inst_opt_filter_pol)
			mmode = i1p3_refl_spot_pol;
		else
			mmode = i1p3_refl_spot;
	} else if (IMODETST(im, inst_mode_ref_strip)) {
		if (m->filt == inst_opt_filter_pol)
			mmode = i1p3_refl_scan_pol;
		else
			mmode = i1p3_refl_scan;

	} else if (IMODETST(im, inst_mode_trans_spot)) {
		mmode = i1p3_trans_spot;
	} else if (IMODETST(im, inst_mode_trans_strip)) {
		mmode = i1p3_trans_scan;
	} else if (IMODETST(im, inst_mode_emis_spot)
	        || IMODETST(im, inst_mode_emis_tele)) {
		if (IMODETST(im, inst_mode_emis_nonadaptive))
			mmode = i1p3_emiss_spot_na;
		else
			mmode = i1p3_emiss_spot;
	} else if (IMODETST(im, inst_mode_emis_strip)) {
		mmode = i1p3_emiss_scan;
	} else if (IMODETST(im, inst_mode_emis_ambient)
	        && (p->cap & inst_mode_emis_ambient)) {
		mmode = i1p3_amb_spot;
	} else if (IMODETST(im, inst_mode_emis_ambient_flash)
	        && (p->cap & inst_mode_emis_ambient_flash)) {
		mmode = i1p3_amb_flash;
	} else {
		return i1p3_no_modes;
	}

	return mmode;
}

/* Check device measurement mode */
static inst_code i1pro3_check_mode(inst *pp, inst_mode m) {
	i1pro3 *p = (i1pro3 *)pp;
	i1p3_mode mmode = 0;	/* Instrument measurement mode */

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (i1pro3_convert_mode(p, m) == i1p3_no_modes)
		return inst_unsupported;

	return inst_ok;
}

/* Set device measurement mode */
static inst_code i1pro3_set_mode(inst *pp, inst_mode im) {
	i1pro3 *p = (i1pro3 *)pp;
	i1p3_mode mmode;		/* Instrument measurement mode */
	inst_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if ((mmode = i1pro3_convert_mode(p, im)) == i1p3_no_modes)
		return inst_unsupported;

	if ((rv = i1pro3_interp_code(p, i1pro3_imp_set_mode(p, mmode, im))) != inst_ok) 
		return rv;

	i1pro3_determine_capabilities(p);

	return inst_ok;
}

/* Return array of reflective measurement conditions selectors in current mode */
static inst_code i1pro3_get_meascond(
struct _inst *pp,
int *no_selectors,			/* Return number of display types */
inst_meascondsel **psels	/* Return the array of measurement conditions types */
							/* Free it after use */
) {
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3imp *m = p->m;
	i1pro3_state *s = m != NULL ? &m->ms[m->mmode] : NULL;
	inst_meascondsel *sels = NULL;
	int no = 0;

	if (psels != NULL)
		*psels = NULL;
	if (no_selectors != NULL)
		*no_selectors = 0;

	if (s != NULL && s->reflective) {

		if ((sels = (inst_meascondsel *)calloc(sizeof(inst_meascondsel), 4)) == NULL) {
			a1loge(p->log, 1, "i1pro3_get_meascond: malloc failed!\n");
			return inst_system_error;
		}

		strcpy(sels[no].desc, "M0");
		sels[no].fsel = inst_opt_filter_none;		/* M0 */
		no++;

		strcpy(sels[no].desc, "M1 (D50)");
		sels[no].fsel = inst_opt_filter_D50;			/* M1 */
		no++;

		strcpy(sels[no].desc, "M2 (UV cut)");
		sels[no].fsel = inst_opt_filter_UVCut;		/* M0 */
		no++;

		if (m->capabilities & I1PRO3_CAP_POL) {

			strcpy(sels[no].desc, "M3 (Polarized)");
			sels[no].fsel = inst_opt_filter_pol;		/* M0 */
			no++;
		}
	}

	if (no_selectors != NULL)
		*no_selectors = no;
	if (psels != NULL)
		*psels = sels;

	return inst_ok;
}

/* 
 * set or reset an optional mode
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 */
static inst_code
i1pro3_get_set_opt(inst *pp, inst_opt_type ot, ...) {
	i1pro3 *p = (i1pro3 *)pp;
	i1pro3imp *m = (i1pro3imp *)p->m;

	if (ot == inst_opt_initcalib) {		/* default */
		i1pro3_set_noinitcalib(p, 0, 0);
		return inst_ok;

	} else if (ot == inst_opt_noinitcalib) {
		va_list args;
		int losecs = 0;

		va_start(args, ot);
		losecs = va_arg(args, int);
		va_end(args);

		i1pro3_set_noinitcalib(p, 1, losecs);
		return inst_ok;

	/* Record the trigger mode */
	} else if (ot == inst_opt_trig_prog
	 || ot == inst_opt_trig_user
	 || ot == inst_opt_trig_user_switch) {
		i1pro3_set_trig(p, ot);
		return inst_ok;
	}

	if (ot == inst_opt_scan_toll) {
		va_list args;
		double toll_ratio = 1.0;

		va_start(args, ot);
		toll_ratio = va_arg(args, double);
		va_end(args);
		return i1pro3_interp_code(p, i1pro3_set_scan_toll(p, toll_ratio));
	}

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited || m == NULL)
		return inst_no_init;

	if (ot == inst_opt_highres) {
		return i1pro3_interp_code(p, i1pro3_set_highres(p));
	} else if (ot == inst_opt_stdres) {
		return i1pro3_interp_code(p, i1pro3_set_stdres(p));
	}

	/* Set the filter type */
	if (ot == inst_opt_set_filter) {
		va_list args;
		inst_opt_filter fe = inst_opt_filter_none;

		va_start(args, ot);

		fe = va_arg(args, inst_opt_filter);

		va_end(args);

		switch(fe) {
		
			case inst_opt_filter_none:		/* M0 */
				m->filt = fe;
				return inst_ok;
			case inst_opt_filter_D50:		/* M1 */
				m->filt = fe;
				return inst_ok;
			case inst_opt_filter_UVCut:		/* M2 */
				m->filt = fe;
				return inst_ok;
			case inst_opt_filter_pol:		/* M3 */
				if (m->capabilities & I1PRO3_CAP_POL) {
					inst_opt_filter ofe = m->filt;

					m->filt = fe;


					if (ofe != fe) {		/* Possible change of mode */
						i1p3_mode mmode;
						i1pro3_code ev;

						if ((mmode = i1pro3_convert_mode(p, m->imode)) == i1p3_no_modes) {
							return inst_unsupported;
						}
						if ((ev = i1pro3_imp_set_mode(p, mmode, m->imode)) != I1PRO3_OK) {
							return i1pro3_interp_code(p, ev);
						}
					}
					return inst_ok;
				}
				return inst_unsupported;
			default:
				break;
		}
		return inst_unsupported;
	}

	/* Return the current filter */
	if (ot == inst_stat_get_filter) {
		inst_opt_filter *filt;
		va_list args;

		va_start(args, ot);
		filt = va_arg(args, inst_opt_filter *);
		va_end(args);

		*filt = m->filt;

		return inst_ok;
	}

	/* Set xcalstd */
	if (ot == inst_opt_set_xcalstd) {
		xcalstd standard;
		va_list args;

		va_start(args, ot);
		standard = va_arg(args, xcalstd);
		va_end(args);

		m->target_calstd = standard;

		return inst_ok;
	}

	/* Get the current effective xcalstd */
	if (ot == inst_opt_get_xcalstd) {
		xcalstd *standard;
		va_list args;

		va_start(args, ot);
		standard = va_arg(args, xcalstd *);
		va_end(args);

		if (m->target_calstd == xcalstd_native)
			*standard = m->native_calstd;		/* If not overridden */
		else
			*standard = m->target_calstd;		/* Overidden std. */

		return inst_ok;
	}

	if (ot == inst_opt_set_custom_filter) {
		va_list args;
		xspect *sp = NULL;

		va_start(args, ot);

		sp = va_arg(args, xspect *);

		va_end(args);

		if (sp == NULL || sp->spec_n == 0) {
			m->custfilt_en = 0;
			m->custfilt.spec_n = 0;
		} else {
			m->custfilt_en = 1;
			m->custfilt = *sp;			/* Struct copy */
		}
		return inst_ok;
	}

	if (ot == inst_stat_get_custom_filter) {
		va_list args;
		xspect *sp = NULL;

		va_start(args, ot);
		sp = va_arg(args, xspect *);
		va_end(args);

		if (m->custfilt_en) {
			*sp = m->custfilt;			/* Struct copy */
		} else {
			sp = NULL;
		}
		return inst_ok;
	}

	/* Return the white calibration tile spectrum */
	/* (We always return the normal rez. reference values) */
	if (ot == inst_opt_get_cal_tile_sp) {
		xspect *sp;
		inst_code rv;
		va_list args;
		int i;

		va_start(args, ot);
		sp = va_arg(args, xspect *);
		va_end(args);

		if (m->white_ref[0] == NULL)
			return inst_no_init;

		sp->spec_n = m->nwav[0];
		sp->spec_wl_short = m->wl_short[0];
		sp->spec_wl_long = m->wl_long[0];
		sp->norm = 100.0;

		for (i = 0; i < sp->spec_n; i++)
			sp->spec[i] = m->white_ref[0][i] * 100.0; 
		
		return inst_ok;
	}

	/* Use default implementation of other inst_opt_type's */
	{
		inst_code rv;
		va_list args;

		va_start(args, ot);
		rv = inst_get_set_opt_def(pp, ot, args);
		va_end(args);

		return rv;
	}
}

/* Destroy ourselves */
static void
i1pro3_del(inst *pp) {
	i1pro3 *p = (i1pro3 *)pp;

	del_i1pro3imp(p);
	if (p->icom != NULL)
		p->icom->del(p->icom);
	p->vdel(pp);
	free(p);
}

/* Constructor */
extern i1pro3 *new_i1pro3(icoms *icom, instType dtype) {
	i1pro3 *p;
	int rv;
	if ((p = (i1pro3 *)calloc(sizeof(i1pro3),1)) == NULL) {
		a1loge(icom->log, 1, "new_i1pro3: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);


	/* Inst methods */
	p->init_coms         = i1pro3_init_coms;
	p->init_inst         = i1pro3_init_inst;
	p->capabilities      = i1pro3_capabilities;
	p->get_serial_no     = i1pro3_get_serial_no;
	p->check_mode        = i1pro3_check_mode;
	p->get_meascond      = i1pro3_get_meascond;
	p->set_mode          = i1pro3_set_mode;
	p->get_set_opt       = i1pro3_get_set_opt;
	p->read_strip        = i1pro3_read_strip;
	p->read_sample       = i1pro3_read_sample;
	p->read_refrate      = i1pro3_read_refrate;
	p->get_n_a_cals      = i1pro3_get_n_a_cals;
	p->calibrate         = i1pro3_calibrate;
	p->meas_delay        = i1pro3_meas_delay;
	p->white_change      = i1pro3_white_change;
	p->interp_error      = i1pro3_interp_error;
	p->config_enum       = i1pro3_config_enum;
	p->del               = i1pro3_del;

	p->icom = icom;
	p->dtype = dtype;

	if ((rv = add_i1pro3imp(p)) != I1PRO3_OK) {
		free(p);
		a1loge(icom->log, 1, "new_i1pro3: error %d creating i1pro3imp\n",rv);
		return NULL;
	}

	i1pro3_determine_capabilities(p);

	return p;
}

