
 /* X-Rite ColorMunki related functions */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   12/1/2009
 *
 * Copyright 2006 - 2010, Graeme W. Gill
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
#endif	/* !SALONEINSTLIB */
#include "xspect.h"
#include "insttypes.h"
#include "icoms.h"
#include "conv.h"
#include "munki.h"
#include "munki_imp.h"

#undef DEBUG

#ifdef DEBUG
#define DBG(xxx) printf xxx ;
#else
#define DBG(xxx) 
#endif

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 5000		/* Maximum reading messagle reply size */

/* Convert a machine specific error code into an abstract dtp code */
static inst_code munki_interp_code(munki *p, munki_code ec);

/* ------------------------------------------------------------------------ */

/* Establish communications with a Munki */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
munki_init_coms(inst *pp, int port, baud_rate br, flow_control fc, double tout) {
	munki *p = (munki *) pp;
	char buf[16];
	int rsize;
	long etime;
	int bi, i, rv;
	inst_code ev = inst_ok;
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

	if (p->debug) {
		p->icom->debug = p->debug;	/* Turn on debugging */
		fprintf(stderr,"munki: About to init coms\n");
	}

	if (p->icom->is_usb_portno(p->icom, port) == instUnknown) {
		if (p->debug) fprintf(stderr,"munki: init_coms called to wrong device!\n");

		return munki_interp_code(p, MUNKI_UNKNOWN_MODEL);
	}

	if (p->debug) fprintf(stderr,"munki: About to init USB\n");

	/* Set config, interface, write end point, read end point, read quanta */
	/* ("serial" end points aren't used - the Munki uses USB control messages) */
	p->icom->set_usb_port(p->icom, port, 1, 0x00, 0x00, usbflags, retries, pnames); 

	if (p->debug) fprintf(stderr,"munki: init coms has suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

/* Initialise the MUNKI */
/* return non-zero on an error, with dtp error code */
static inst_code
munki_init_inst(inst *pp) {
	munki *p = (munki *)pp;
	munki_code ev = MUNKI_OK;

	if (p->debug) fprintf(stderr,"munki: About to init instrument\n");

	if (p->gotcoms == 0)
		return munki_interp_code(p, MUNKI_INT_NO_COMS);	/* Must establish coms before calling init */
	if ((ev = munki_imp_init(p)) != MUNKI_OK) {
		if (p->debug) fprintf(stderr,"munki_imp_init() failed\n");
		return munki_interp_code(p, ev);
	}

	/* Set the Munki capabilities mask */
	p->cap |= inst_ref_spot
	       |  inst_ref_strip
	       |  inst_emis_spot
	       |  inst_emis_disp
	       |  inst_emis_proj		/* Support of a specific projector mode */
	       |  inst_emis_tele
	       |  inst_trans_spot		/* Support this manually using a light table */
	       |  inst_trans_strip 
	       |  inst_emis_strip		/* Also likely to be light table reading */
	       |  inst_colorimeter
	       |  inst_spectral
		   |  inst_emis_ambient
		   |  inst_emis_ambient_flash
	       ;

	p->cap2 |= inst2_cal_ref_white
	        | inst2_cal_trans_white 
	        | inst2_cal_disp_int_time 
	        | inst2_cal_proj_int_time 
	        | inst2_prog_trig 
			| inst2_keyb_trig
			| inst2_keyb_switch_trig
			| inst2_bidi_scan
			| inst2_has_scan_toll
			| inst2_no_feedback
			| inst2_has_leds
			| inst2_has_sensmode
	        ;

	if (munki_imp_highres(p))
		p->cap |= inst_highres;

	return munki_interp_code(p, ev);
}

static char *munki_get_serial_no(inst *pp) {
	munki *p = (munki *)pp;
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

	rv = munki_imp_measure(p, vals, npatch);

	return munki_interp_code(p, rv);
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
munki_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val) {		/* Pointer to instrument patch value */
	munki *p = (munki *)pp;
	munki_code rv;

	rv = munki_imp_measure(p, val, 1);

	return munki_interp_code(p, rv);
}

/* Determine if a calibration is needed. Returns inst_calt_none if not, */
/* inst_calt_unknown if it is unknown, or inst_calt_XXX if needs calibration, */
/* and the first type of calibration needed. */
inst_cal_type munki_needs_calibration(inst *pp) {
	munki *p = (munki *)pp;
	return munki_imp_needs_calibration(p);
}

/* Request an instrument calibration. */
/* This is use if the user decides they want to do a calibration, */
/* in anticipation of a calibration (needs_calibration()) to avoid */
/* requiring one during measurement, or in response to measuring */
/* returning inst_needs_cal. Initially us an inst_cal_cond of inst_calc_none, */
/* and then be prepared to setup the right conditions, or ask the */
/* user to do so, each time the error inst_cal_setup is returned. */
inst_code munki_calibrate(
inst *pp,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	munki *p = (munki *)pp;
	munki_code rv;

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
			return "User hit Abort key";
		case MUNKI_USER_TERM:
			return "User hit Terminate key";
		case MUNKI_USER_TRIG:
			return "User hit Trigger key";
		case MUNKI_USER_CMND:
			return "User hit a Command key";

		case MUNKI_UNSUPPORTED:
			return "Unsupported function";
		case MUNKI_CAL_SETUP:
			return "Calibration retry with correct setup is needed";

		case MUNKI_OK:
			return "No device error";

		case MUNKI_DATA_COUNT:
			return "EEProm data count unexpected size";
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
			return "Measurement read buffer is too small";
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
			return inst_user_abort | ec;
		case MUNKI_USER_TERM:
			return inst_user_term | ec;
		case MUNKI_USER_TRIG:
			return inst_user_trig | ec;
		case MUNKI_USER_CMND:
			return inst_user_cmnd | ec;

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
			return inst_wrong_sensor_pos | ec;

		case MUNKI_DATA_COUNT:
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
			return inst_misread | ec;

		case MUNKI_INTERNAL_ERROR:
		case MUNKI_INT_NO_COMS:
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
		case MUNKI_INT_WRONGPATCHES:
		case MUNKI_INT_CAL_RESTORE:
			return inst_internal_error | ec;
	}
	return inst_other_error | ec;
}

/* Return the instrument capabilities */
inst_capability munki_capabilities(inst *pp) {
	munki *p = (munki *)pp;

	return p->cap;
}

/* Return the instrument capabilities 2 */
inst2_capability munki_capabilities2(inst *pp) {
	munki *p = (munki *)pp;
	inst2_capability rv;

	rv = p->cap2;

	return rv;
}

/* Set device measurement mode */
inst_code munki_set_mode(inst *pp, inst_mode m) {
	munki *p = (munki *)pp;
	inst_mode mm;			/* Request measurement mode */
	mk_mode mmode = -1;	/* Instrument measurement mode */

	/* The measurement mode portion of the mode */
	mm = m & inst_mode_measurement_mask;

	if ((mm & inst_mode_illum_mask) == inst_mode_reflection) {
		if ((mm & inst_mode_sub_mask) == inst_mode_spot) {
			mmode = mk_refl_spot;
		} else if ((mm & inst_mode_sub_mask) == inst_mode_strip) {
			mmode = mk_refl_scan;
		} else {
			return inst_unsupported;
		}
	} else if ((mm & inst_mode_illum_mask) == inst_mode_transmission) {
		if ((mm & inst_mode_sub_mask) == inst_mode_spot) {
			mmode = mk_trans_spot;
		} else if ((mm & inst_mode_sub_mask) == inst_mode_strip) {
			mmode = mk_trans_scan;
		} else {
			return inst_unsupported;
		}
	} else if ((mm & inst_mode_illum_mask) == inst_mode_emission) {
		if ((mm & inst_mode_sub_mask) == inst_mode_spot) {
			if ((mm & inst_mode_mod_mask) == inst_mode_disp) {
				mmode = mk_disp_spot;
			} else {
				mmode = mk_emiss_spot;
			}
		} else if ((mm & inst_mode_sub_mask) == inst_mode_tele) {
			if ((mm & inst_mode_mod_mask) == inst_mode_disp) {
				mmode = mk_proj_spot;
			} else {
				mmode = mk_tele_spot;
			}
		} else if ((mm & inst_mode_sub_mask) == inst_mode_strip) {
			mmode = mk_emiss_scan;
		} else if ((mm & inst_mode_sub_mask) == inst_mode_ambient) {
			mmode = mk_amb_spot;
		} else if ((mm & inst_mode_sub_mask) == inst_mode_ambient_flash) {
			mmode = mk_amb_flash;
		} else {
			return inst_unsupported;
		}
	} else {
		return inst_unsupported;
	}
	return munki_interp_code(p, munki_imp_set_mode(p, mmode, m & inst_mode_spectral));
}

/* Get a dynamic status */
static inst_code munki_get_status(
inst *pp,
inst_status_type m,	/* Requested status type */
...) {				/* Status parameters */                             
	munki *p = (munki *)pp;
	inst_code rv = inst_ok;

	/* Return the sensor mode */
	if (m == inst_stat_sensmode) {
		munki_code ev;
		inst_stat_smode *smode;
		va_list args;
		mk_spos spos;

		va_start(args, m);
		smode = va_arg(args, inst_stat_smode *);
		va_end(args);

		*smode = inst_stat_smode_unknown;

		/* Get the device status */
		ev = munki_getstatus(p, &spos, NULL);
		if (ev != MUNKI_OK);
			return munki_interp_code(p, ev);

		if (spos == mk_spos_proj)
			*smode = inst_stat_smode_proj;
		else if (spos == mk_spos_surf)
			*smode = inst_stat_smode_ref | inst_stat_smode_disp;
		else if (spos == mk_spos_calib)
			*smode = inst_stat_smode_calib;
		else if (spos == mk_spos_amb)
			*smode = inst_stat_smode_amb;

		return inst_ok;
	}

	/* Return the filter */
	else if (m == inst_stat_get_filter) {
		munki_code ev;
		inst_opt_filter *filt;
		va_list args;

		va_start(args, m);
		filt = va_arg(args, inst_opt_filter *);
		va_end(args);

		/* The ColorMunki is always UV cut */
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
munki_set_opt_mode(inst *pp, inst_opt_mode m, ...)
{
	munki *p = (munki *)pp;
	char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;

	/* Ignore these modes - not applicable, but be nice. */
	if (m == inst_opt_disp_crt
	 || m == inst_opt_disp_lcd
	 || m == inst_opt_proj_crt
	 || m == inst_opt_proj_lcd) {
		return inst_ok;
	}

	if (m == inst_opt_noautocalib) {
		munki_set_noautocalib(p, 1);
		return inst_ok;

	} else if (m == inst_opt_autocalib) {
		munki_set_noautocalib(p, 0);
		return inst_ok;

	/* Record the trigger mode */
	} else if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_keyb
	 || m == inst_opt_trig_keyb_switch) {
		munki_set_trig(p, m);
		return inst_ok;
	}
	if (m == inst_opt_trig_return) {
		munki_set_trigret(p, 1);
		return inst_ok;
	} else if (m == inst_opt_trig_no_return) {
		munki_set_trigret(p, 0);
		return inst_ok;
	}

	if (m == inst_opt_highres) {
		return munki_interp_code(p, munki_set_highres(p));
	} else if (m == inst_opt_stdres) {
		return munki_interp_code(p, munki_set_stdres(p));
	}

	if (m == inst_opt_scan_toll) {
		va_list args;
		double toll_ratio = 1.0;

		va_start(args, m);
		toll_ratio = va_arg(args, double);
		va_end(args);
		return munki_interp_code(p, munki_set_scan_toll(p, toll_ratio));
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
extern munki *new_munki(icoms *icom, int debug, int verb)
{
	munki *p;
	if ((p = (munki *)calloc(sizeof(munki),1)) == NULL)
		error("munki: malloc failed!");

	if (icom == NULL)
		p->icom = new_icoms();
	else
		p->icom = icom;

	p->debug = debug;
	p->verb = verb;

	if (add_munkiimp(p) != MUNKI_OK) {
		free(p);
		error("munki: creating munkiimp");
	}

	/* Inst methods */
	p->init_coms         = munki_init_coms;
	p->init_inst         = munki_init_inst;
	p->get_serial_no     = munki_get_serial_no;
	p->capabilities      = munki_capabilities;
	p->capabilities2     = munki_capabilities2;
	p->set_mode          = munki_set_mode;
	p->get_status        = munki_get_status;
	p->set_opt_mode      = munki_set_opt_mode;
	p->read_strip        = munki_read_strip;
	p->read_sample       = munki_read_sample;
	p->needs_calibration = munki_needs_calibration;
	p->calibrate         = munki_calibrate;
	p->interp_error      = munki_interp_error;
	p->del               = munki_del;

	p->itype = instUnknown;		/* Until initalisation */

	return p;
}

