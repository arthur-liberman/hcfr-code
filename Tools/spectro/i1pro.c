
/* 
 * Argyll Color Correction System
 *
 * Gretag i1Monitor & i1Pro related functions
 *
 * Author: Graeme W. Gill
 * Date:   24/11/2006
 *
 * Copyright 2006 - 2007, Graeme W. Gill
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
#else /* SALONEINSTLIB */
#include "sa_config.h"
#include "numsup.h"
#endif /* SALONEINSTLIB */
#include "xspect.h"
#include "insttypes.h"
#include "icoms.h"
#include "conv.h"
#include "i1pro.h"
#include "i1pro_imp.h"

#undef DEBUG

#ifdef DEBUG
#define DBG(xxx) printf xxx ;
#else
#define DBG(xxx) 
#endif

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 5000		/* Maximum reading messagle reply size */

/* Convert a machine specific error code into an abstract dtp code */
static inst_code i1pro_interp_code(i1pro *p, i1pro_code ec);

/* ------------------------------------------------------------------------ */

/* Establish communications with a I1DISP */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
i1pro_init_coms(inst *pp, int port, baud_rate br, flow_control fc, double tout) {
	i1pro *p = (i1pro *) pp;
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

	if (p->debug) {
		p->icom->debug = p->debug;	/* Turn on debugging */
		fprintf(stderr,"i1pro: About to init coms\n");
	}

	if (p->icom->is_usb_portno(p->icom, port) == instUnknown) {
		if (p->debug) fprintf(stderr,"i1pro: init_coms called to wrong device!\n");

		return i1pro_interp_code(p, I1PRO_UNKNOWN_MODEL);
	}

	if (p->debug) fprintf(stderr,"i1pro: About to init USB\n");

	/* Set config, interface, write end point, read end point, read quanta */
	/* ("serial" end points aren't used - the i1display uses USB control messages) */
	p->icom->set_usb_port(p->icom, port, 1, 0x00, 0x00, usbflags, retries, pnames); 

	if (p->debug) fprintf(stderr,"i1pro: init coms has suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

static inst_code
i1pro_determine_capabilities(i1pro *p) {

	/* Set the base Monitor/Pro capabilities mask */
	p->cap =  inst_emis_spot
	       |  inst_emis_disp
	       |  inst_trans_spot		/* Support this manually using a light table */
	       |  inst_trans_strip 
	       |  inst_emis_strip		/* Also likely to be light table reading */
	       |  inst_colorimeter
	       |  inst_spectral
	       ;

	p->cap2 = inst2_cal_trans_white 
	        | inst2_cal_disp_int_time 
	        | inst2_prog_trig 
			| inst2_keyb_trig
			| inst2_keyb_switch_trig
			| inst2_bidi_scan
			| inst2_has_scan_toll
			| inst2_no_feedback
	        ;

	/* Set the Pro capabilities mask */
	if (p->itype == instI1Pro) {
		p->cap |= inst_ref_spot
		       |  inst_ref_strip
		       ;

		p->cap2 |= inst2_cal_ref_white
		        ;
	}

	if (i1pro_imp_highres(p))		/* This is static */
		p->cap |= inst_highres;

	if (i1pro_imp_ambient(p)) {		/* This depends on the instrument */
		p->cap |= inst_emis_ambient
		       |  inst_emis_ambient_flash
		       ;
	}
	return inst_ok;
}

/* Initialise the I1PRO */
/* return non-zero on an error, with dtp error code */
static inst_code
i1pro_init_inst(inst *pp) {
	i1pro *p = (i1pro *)pp;
	i1pro_code ev = I1PRO_OK;

	if (p->debug) fprintf(stderr,"i1pro: About to init instrument\n");

	if (p->gotcoms == 0)
		return i1pro_interp_code(p, I1PRO_INT_NO_COMS);	/* Must establish coms before calling init */
	if ((ev = i1pro_imp_init(p)) != I1PRO_OK) {
		if (p->debug) fprintf(stderr,"i1pro_imp_init() failed\n");
		return i1pro_interp_code(p, ev);
	}

	p->inited = 1;

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
/* Return the dtp error code */
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

	rv = i1pro_imp_measure(p, vals, npatch);

	return i1pro_interp_code(p, rv);
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
i1pro_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val) {		/* Pointer to instrument patch value */
	i1pro *p = (i1pro *)pp;
	i1pro_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	rv = i1pro_imp_measure(p, val, 1);

	return i1pro_interp_code(p, rv);
}

/* Determine if a calibration is needed. Returns inst_calt_none if not, */
/* inst_calt_unknown if it is unknown, or inst_calt_XXX if needs calibration, */
/* and the first type of calibration needed. */
inst_cal_type i1pro_needs_calibration(inst *pp) {
	i1pro *p = (i1pro *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	return i1pro_imp_needs_calibration(p);
}

/* Request an instrument calibration. */
/* This is use if the user decides they want to do a calibration, */
/* in anticipation of a calibration (needs_calibration()) to avoid */
/* requiring one during measurement, or in response to measuring */
/* returning inst_needs_cal. Initially us an inst_cal_cond of inst_calc_none, */
/* and then be prepared to setup the right conditions, or ask the */
/* user to do so, each time the error inst_cal_setup is returned. */
inst_code i1pro_calibrate(
inst *pp,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
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
			return "User hit Abort key";
		case I1PRO_USER_TERM:
			return "User hit Terminate key";
		case I1PRO_USER_TRIG:
			return "User hit Trigger key";
		case I1PRO_USER_CMND:
			return "User hit a Command key";

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
		case I1PRO_HW_EE_SHORTREAD:
			return "Read less bytes for EEProm read than expected";
		case I1PRO_HW_ME_SHORTREAD:
			return "Read less bytes for measurement read than expected";
		case I1PRO_HW_ME_ODDREAD:
			return "Read a number of bytes not a multiple of 256";
		case I1PRO_HW_CALIBINFO:
			return "Instrument calibration info is missing or corrupted";

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

		case I1PRO_INT_NO_COMS:
			return "Communications hasn't been established";
		case I1PRO_INT_EETOOBIG:
			return "Read of EEProm is too big (> 65536)";
		case I1PRO_INT_ODDREADBUF:
			return "Measurement read buffer is not a multiple of 256";
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
			return "Measurement read buffer is too small";
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
		case I1PRO_INT_ADARK_INVALID:
			return "Adaptive dark calibration is invalid";

		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
i1pro_interp_code(i1pro *p, i1pro_code ec) {

	ec &= inst_imask;
	switch (ec) {

		case I1PRO_OK:

			return inst_ok;

		case I1PRO_COMS_FAIL:
			return inst_coms_fail | ec;

		case I1PRO_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case I1PRO_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

		case I1PRO_USER_ABORT:
			return inst_user_abort | ec;
		case I1PRO_USER_TERM:
			return inst_user_term | ec;
		case I1PRO_USER_TRIG:
			return inst_user_trig | ec;
		case I1PRO_USER_CMND:
			return inst_user_cmnd | ec;

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
		case I1PRO_INT_ADARK_INVALID:
			return inst_internal_error | ec;
	}
	return inst_other_error | ec;
}

/* Return the instrument capabilities */
inst_capability i1pro_capabilities(inst *pp) {
	i1pro *p = (i1pro *)pp;

	return p->cap;
}

/* Return the instrument capabilities 2 */
inst2_capability i1pro_capabilities2(inst *pp) {
	i1pro *p = (i1pro *)pp;
	inst2_capability rv;

	rv = p->cap2;

	return rv;
}

/* Set device measurement mode */
inst_code i1pro_set_mode(inst *pp, inst_mode m) {
	i1pro *p = (i1pro *)pp;
	inst_mode mm;			/* Request measurement mode */
	i1p_mode mmode = -1;	/* Instrument measurement mode */

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* The measurement mode portion of the mode */
	mm = m & inst_mode_measurement_mask;

	if ((mm & inst_mode_illum_mask) == inst_mode_reflection) {
		if ((mm & inst_mode_sub_mask) == inst_mode_spot) {
			mmode = i1p_refl_spot;
		} else if ((mm & inst_mode_sub_mask) == inst_mode_strip) {
			mmode = i1p_refl_scan;
		} else {
			return inst_unsupported;
		}
	} else if ((mm & inst_mode_illum_mask) == inst_mode_transmission) {
		if ((mm & inst_mode_sub_mask) == inst_mode_spot) {
			mmode = i1p_trans_spot;
		} else if ((mm & inst_mode_sub_mask) == inst_mode_strip) {
			mmode = i1p_trans_scan;
		} else {
			return inst_unsupported;
		}
	} else if ((mm & inst_mode_illum_mask) == inst_mode_emission) {
		if ((mm & inst_mode_sub_mask) == inst_mode_spot) {
			if ((mm & inst_mode_mod_mask) == inst_mode_disp) {
				mmode = i1p_disp_spot;
			} else {
				mmode = i1p_emiss_spot;
			}
		} else if ((mm & inst_mode_sub_mask) == inst_mode_strip) {
			mmode = i1p_emiss_scan;
		} else if ((mm & inst_mode_sub_mask) == inst_mode_ambient
		        && (p->cap & inst_emis_ambient)) {
			mmode = i1p_amb_spot;
		} else if ((mm & inst_mode_sub_mask) == inst_mode_ambient_flash
		        && (p->cap & inst_emis_ambient_flash)) {
			mmode = i1p_amb_flash;
		} else {
			return inst_unsupported;
		}
	} else {
		return inst_unsupported;
	}
	return i1pro_interp_code(p, i1pro_imp_set_mode(p, mmode, m & inst_mode_spectral));
}

/* Get a (possibly) dynamic status */
static inst_code i1pro_get_status(
inst *pp,
inst_status_type m,	/* Requested status type */
...) {				/* Status parameters */                             
	i1pro *p = (i1pro *)pp;
	i1proimp *imp = (i1proimp *)p->m;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Return the filter */
	if (m == inst_stat_get_filter) {
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

	return inst_unsupported;
}

/* 
 * set or reset an optional mode
 * We assume that the instrument has been initialised.
 */
static inst_code
i1pro_set_opt_mode(inst *pp, inst_opt_mode m, ...)
{
	i1pro *p = (i1pro *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (m == inst_opt_noautocalib) {
		i1pro_set_noautocalib(p, 1);
		return inst_ok;

	} else if (m == inst_opt_autocalib) {
		i1pro_set_noautocalib(p, 0);
		return inst_ok;

	/* Record the trigger mode */
	} else if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_keyb
	 || m == inst_opt_trig_keyb_switch) {
		i1pro_set_trig(p, m);
		return inst_ok;
	}
	if (m == inst_opt_trig_return) {
		i1pro_set_trigret(p, 1);
		return inst_ok;
	} else if (m == inst_opt_trig_no_return) {
		i1pro_set_trigret(p, 0);
		return inst_ok;
	}

	if (m == inst_opt_highres) {
		return i1pro_interp_code(p, i1pro_set_highres(p));
	} else if (m == inst_opt_stdres) {
		return i1pro_interp_code(p, i1pro_set_stdres(p));
	}

	if (m == inst_opt_scan_toll) {
		va_list args;
		double toll_ratio = 1.0;

		va_start(args, m);
		toll_ratio = va_arg(args, double);
		va_end(args);
		return i1pro_interp_code(p, i1pro_set_scan_toll(p, toll_ratio));
	}

	return inst_unsupported;
}

/* Destroy ourselves */
static void
i1pro_del(inst *pp) {
	i1pro *p = (i1pro *)pp;

	del_i1proimp(p);
	if (p->icom != NULL)
		p->icom->del(p->icom);
	free(p);
}

/* Constructor */
extern i1pro *new_i1pro(icoms *icom, instType itype, int debug, int verb)
{
	i1pro *p;
	if ((p = (i1pro *)calloc(sizeof(i1pro),1)) == NULL)
		error("i1pro: malloc failed!");

	if (icom == NULL)
		p->icom = new_icoms();
	else
		p->icom = icom;

	i1pro_determine_capabilities(p);
	p->debug = debug;
	p->verb = verb;

	if (add_i1proimp(p) != I1PRO_OK) {
		free(p);
		error("i1pro: creating i1proimp");
	}

	/* Inst methods */
	p->init_coms         = i1pro_init_coms;
	p->init_inst         = i1pro_init_inst;
	p->capabilities      = i1pro_capabilities;
	p->capabilities2     = i1pro_capabilities2;
	p->get_serial_no     = i1pro_get_serial_no;
	p->set_mode          = i1pro_set_mode;
	p->get_status        = i1pro_get_status;
	p->set_opt_mode      = i1pro_set_opt_mode;
	p->read_strip        = i1pro_read_strip;
	p->read_sample       = i1pro_read_sample;
	p->needs_calibration = i1pro_needs_calibration;
	p->calibrate         = i1pro_calibrate;
	p->interp_error      = i1pro_interp_error;
	p->del               = i1pro_del;

	p->itype = itype;

	return p;
}

