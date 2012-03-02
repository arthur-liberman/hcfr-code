

/*
 * Argyll Color Correction System
 *
 * Hughski ColorHug related functions
 *
 * Author: Richard Hughes
 * Date:   30/11/2011
 *
 * Copyright 2006 - 2007, Graeme W. Gill
 * Copyright 2011, Richard Hughes
 * All rights reserved.
 *
 * (Based on huey.c)
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
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
#include "colorhug.h"

static inst_code colorhug_interp_code(inst *pp, int ec);

/* Interpret an icoms error into a ColorHug error */
static int icoms2colorhug_err(int se) {
	if (se & ICOM_USERM) {
		se &= ICOM_USERM;
		if (se == ICOM_USER)
			return COLORHUG_USER_ABORT;
		if (se == ICOM_TERM)
			return COLORHUG_USER_TERM;
		if (se == ICOM_TRIG)
			return COLORHUG_USER_TRIG;
		if (se == ICOM_CMND)
			return COLORHUG_USER_CMND;
	}
	if (se != ICOM_OK)
		return COLORHUG_COMS_FAIL;
	return COLORHUG_OK;
}

/* ColorHug commands that we care about */
typedef enum {
	ch_set_mult		= 0x04,		/* Set multiplier value */
	ch_set_integral	= 0x06,		/* Set integral time */
	ch_get_serial	= 0x0b,		/* Gets the serial number */
	ch_set_leds		= 0x0e,		/* Sets the LEDs */
	ch_take_reading	= 0x23		/* Takes an XYZ reading */
} ColorHugCmd;

/* Diagnostic - return a description given the instruction code */
static char *inst_desc(int cc) {
	static char buf[40];
	switch(cc) {
	case 0x04:
		return "SetMultiplier";
	case 0x06:
		return "SetIntegral";
	case 0x0b:
		return "GetSerial";
	case 0x0e:
		return "SetLeds";
	case 0x23:
		return "TakeReadingXYZ";
	}
	sprintf(buf,"Unknown %02x",cc);
	return buf;
}

/* Error codes interpretation */
static char *
colorhug_interp_error(inst *pp, int ec) {
	ec &= inst_imask;
	switch (ec) {
		case COLORHUG_INTERNAL_ERROR:
			return "Internal software error";
		case COLORHUG_COMS_FAIL:
			return "Communications failure";
		case COLORHUG_UNKNOWN_MODEL:
			return "Not a known ColorHug Model";
		case COLORHUG_USER_ABORT:
			return "User hit Abort key";
		case COLORHUG_USER_TERM:
			return "User hit Terminate key";
		case COLORHUG_USER_TRIG:
			return "User hit Trigger key";
		case COLORHUG_USER_CMND:
			return "User hit a Command key";

		case COLORHUG_OK:
			return "OK";
		case COLORHUG_UNKNOWN_CMD:
			return "Unknown connamd";
		case COLORHUG_WRONG_UNLOCK_CODE:
			return "Wrong unlock code";
		case COLORHUG_NOT_IMPLEMENTED:
			return "Not implemented";
		case COLORHUG_UNDERFLOW_SENSOR:
			return "Sensor underflow";
		case COLORHUG_NO_SERIAL:
			return "No serial";
		case COLORHUG_WATCHDOG:
			return "Watchdog";
		case COLORHUG_INVALID_ADDRESS:
			return "Invalid address";
		case COLORHUG_INVALID_LENGTH:
			return "Invalid length";
		case COLORHUG_INVALID_CHECKSUM:
			return "Invlid checksum";
		case COLORHUG_INVALID_VALUE:
			return "Invalid value";
		case COLORHUG_UNKNOWN_CMD_FOR_BOOTLOADER:
			return "Unknown command for bootloader";
		case COLORHUG_NO_CALIBRATION:
			return "No calibration";
		case COLORHUG_OVERFLOW_MULTIPLY:
			return "Multiply overflow";
		case COLORHUG_OVERFLOW_ADDITION:
			return "Addition overflow";
		case COLORHUG_OVERFLOW_SENSOR:
			return "Sensor overflow";
		case COLORHUG_OVERFLOW_STACK:
			return "Stack overflow";

		/* Internal errors */
		case COLORHUG_NO_COMS:
			return "Communications hasn't been established";
		case COLORHUG_NOT_INITED:
			return "Insrument hasn't been initialised";
		default:
			return "Unknown error code";
	}
}

/* Do a command/response exchange with the colorhug */
static inst_code
colorhug_command(colorhug *p,
				 ColorHugCmd cmd,
				 unsigned char *in, unsigned int in_size,
				 unsigned char *out, unsigned int out_size,
				 double timeout)
{
	int i;
	unsigned char buf[64];
	int wbytes;
	int rbytes;
	int se, ua = 0, rv = inst_ok;
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	if (isdeb <= 2)
		p->icom->debug = 0;

	if (isdeb) {
		fprintf(stderr,"colorhug: Sending cmd '%s' args '%s'\n",
				inst_desc(cmd), icoms_tohex(in, in_size));
	}

	/* Send the command with any specified data */
	buf[0] = cmd;
	if (in != NULL)
		memcpy(buf + 1, in, in_size);
	if (p->icom->is_hid) {
		se = p->icom->hid_write(p->icom, buf, in_size + 1, &wbytes, timeout);
	} else {
		se = p->icom->usb_write(p->icom, 0x01, buf, in_size + 1, &wbytes, timeout);
	}
	if (se != 0) {
		if (se & ICOM_USERM) {
			ua = (se & ICOM_USERM);
		}
		if (se & ~ICOM_USERM) {
			if (isdeb)
				fprintf(stderr,"colorhug: Command send failed with ICOM err 0x%x\n", se);
			p->icom->debug = isdeb;
			return colorhug_interp_code((inst *)p, COLORHUG_COMS_FAIL);
		}
	}
	rv = colorhug_interp_code((inst *)p, icoms2colorhug_err(ua));
	if (isdeb)
		fprintf(stderr,"colorhug: ICOM err 0x%x\n",ua);
	if (wbytes != in_size + 1)
		rv = colorhug_interp_code((inst *)p, COLORHUG_BAD_WR_LENGTH);
	if (rv != inst_ok) {
		p->icom->debug = isdeb;
		return rv;
	}

	/* Now fetch the response */
	if (isdeb)
		fprintf(stderr,"colorhug: Reading response\n");

	if (p->icom->is_hid) {
		se = p->icom->hid_read(p->icom, buf, out_size + 2, &rbytes, timeout);
	} else {
		se = p->icom->usb_read(p->icom, 0x81, buf, out_size + 2, &rbytes, timeout);
	}

	if (isdeb && rbytes >= 2) {
		fprintf(stderr,"Recieved cmd '%s' error '%s' args '%s'\n",
				inst_desc(buf[1]),
				colorhug_interp_error((inst *) p, buf[0]),
				icoms_tohex(buf, rbytes - 2));
	}

	if (se != 0) {

		/* deal with command error */
		if (rbytes == 2 && buf[0] != COLORHUG_OK) {
			rv = colorhug_interp_code((inst *)p, buf[0]);
			return rv;
		}

		/* deal with underrun or overrun */
		if (rbytes != out_size + 2) {
			rv = colorhug_interp_code((inst *)p, COLORHUG_BAD_RD_LENGTH);
			return rv;
		}

		/* there's another reason it failed */
		if (se & ICOM_USERM) {
			ua = (se & ICOM_USERM);
		}
		if (se & ~ICOM_USERM) {
			if (isdeb)
				fprintf(stderr,"colorhug: Response read failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return colorhug_interp_code((inst *)p, COLORHUG_COMS_FAIL);
		}
	}
	rv = colorhug_interp_code((inst *)p, icoms2colorhug_err(ua));
	if (rv != inst_ok)
		return rv;

	/* check the command was the same */
	if (buf[1] != cmd) {
		rv = colorhug_interp_code((inst *)p, COLORHUG_BAD_RET_CMD);
		return rv;
	}
	if (out != NULL) {
		memcpy(out, buf + 2, out_size);
	}
	if (isdeb) {
		fprintf(stderr,"colorhug: '%s' ICOM err 0x%x\n",
				icoms_tohex(buf + 2, out_size),ua);
	}
	p->icom->debug = isdeb;
	return rv;
}

/* Take a short, and convert it into a byte buffer */
static void short2buf(unsigned char *buf, int inv)
{
	buf[0] = (inv >> 0) & 0xff;
	buf[1] = (inv >> 8) & 0xff;
}

/* Converts a packed float to a double */
static double packed_float_to_double (int32_t pf)
{
	return (double) pf / (double) 0x10000;
}

/* Set the device LED state */
static inst_code
colorhug_set_LEDs(colorhug *p, int mask)
{
	int i;
	unsigned char ibuf[4];
	inst_code ev;

	mask &= 0x3;
	p->led_state = mask;

	ibuf[0] = mask;
	ibuf[1] = 0; /* repeat */
	ibuf[2] = 0; /* on */
	ibuf[3] = 0; /* off */

	/* Do command */
	ev = colorhug_command(p, ch_set_leds,
						  ibuf, sizeof (ibuf), /* input */
						  NULL, 0, /* output */
						  1.0);
	return ev;
}

/* Take a XYZ measurement from the device */
static inst_code
colorhug_take_XYZ_measurement(colorhug *p, double XYZ[3])
{
	inst_code ev;
	int i;
	uint8_t ibuf[2];
	uint32_t obuf[3];

	if (!p->inited)
		return colorhug_interp_code((inst *)p, COLORHUG_NOT_INITED);

	/* Choose the calibration matrix */
	short2buf(ibuf + 0, p->calix + 64);

	/* Do the measurement, and return the values */
	ev = colorhug_command(p, ch_take_reading,
						  ibuf, sizeof (ibuf),
						  (unsigned char *) obuf, sizeof (obuf),
						  30.0);
	if (ev != inst_ok)
		return ev;

	/* Convert to doubles */
	for (i = 0; i < 3; i++)
		XYZ[i] = packed_float_to_double (obuf[i]);

	/* Apply the colorimeter correction matrix */
	icmMulBy3x3(XYZ, p->ccmat, XYZ);

	if (p->debug) {
		fprintf(stderr,"colorhug: returning XYZ = %f %f %f\n",
				XYZ[0],XYZ[1],XYZ[2]);
	}
	return inst_ok;
}

/* Establish communications with a ColorHug */
static inst_code
colorhug_init_coms(inst *pp, int port, baud_rate br, flow_control fc, double tout) {
	colorhug *p = (colorhug *) pp;

	if (p->debug) {
		p->icom->debug = p->debug;	/* Turn on debugging */
		fprintf(stderr,"colorhug: About to init coms\n");
	}

	/* Open as an HID if available */
	if (p->icom->is_hid_portno(p->icom, port) != instUnknown) {

		if (p->debug)
			fprintf(stderr,"colorhug: About to init HID\n");

		/* Set config, interface */
		p->icom->set_hid_port(p->icom, port, icomuf_none, 0, NULL);

	} else if (p->icom->is_usb_portno(p->icom, port) != instUnknown) {

		if (p->debug)
			fprintf(stderr,"colorhug: About to init USB\n");

		/* Set config, interface, write end point, read end point */
		p->icom->set_usb_port(p->icom, port, 1, 0x00, 0x00, icomuf_detach, 0, NULL);

	} else {
		if (p->debug)
			fprintf(stderr,"colorhug: init_coms called to wrong device!\n");
		return colorhug_interp_code((inst *)p, COLORHUG_UNKNOWN_MODEL);
	}

	if (p->debug)
		fprintf(stderr,"colorhug: init coms has suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

/* Set the device multiplier */
static inst_code
colorhug_set_multiplier (colorhug *p, int multiplier)
{
	inst_code ev;
	unsigned char ibuf[1];

	/* Set the desired multiplier */
	ibuf[0] = multiplier;
	ev = colorhug_command(p, ch_set_mult,
						  ibuf, sizeof (ibuf),
						  NULL, 0,
						  1.0);
	return ev;
}

/* Set the device integral time */
static inst_code
colorhug_set_integral (colorhug *p, int integral)
{
	inst_code ev;
	unsigned char ibuf[2];

	/* Set the desired integral time */
	short2buf(ibuf + 0, integral);
	ev = colorhug_command(p, ch_set_integral,
						  ibuf, sizeof (ibuf),
						  NULL, 0,
						  1.0);
	return ev;
}

/* Initialise the ColorHug */
static inst_code
colorhug_init_inst(inst *pp)
{
	colorhug *p = (colorhug *)pp;
	inst_code ev;

	if (p->debug)
		fprintf(stderr,"colorhug: About to init instrument\n");

	/* Must establish coms first */
	if (p->gotcoms == 0)
		return colorhug_interp_code((inst *)p, COLORHUG_NO_COMS);

	/* Turn the LEDs off */
	ev = colorhug_set_LEDs(p, 0x0);
	if (ev != inst_ok)
		return ev;

	/* Turn the sensor on */
	ev = colorhug_set_multiplier(p, 0x03);
	if (ev != inst_ok)
		return ev;

	/* Set the integral time to maximum precision */
	ev = colorhug_set_integral(p, 0xffff);
	if (ev != inst_ok)
		return ev;

	p->trig = inst_opt_trig_keyb;
	p->inited = 1;
	if (p->debug)
		fprintf(stderr,"colorhug: instrument inited OK\n");

	/* Flash the LEDs */
	ev = colorhug_set_LEDs(p, 0x1);
	if (ev != inst_ok)
		return ev;
	msec_sleep(50);
	ev = colorhug_set_LEDs(p, 0x2);
	if (ev != inst_ok)
		return ev;
	msec_sleep(50);
	ev = colorhug_set_LEDs(p, 0x1);
	if (ev != inst_ok)
		return ev;
	msec_sleep(50);
	ev = colorhug_set_LEDs(p, 0x0);
	if (ev != inst_ok)
		return ev;

	return ev;
}

/* Read a single sample */
static inst_code
colorhug_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val) {		/* Pointer to instrument patch value */
	colorhug *p = (colorhug *)pp;
	int user_trig = 0;
	int rv = inst_protocol_error;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->trig == inst_opt_trig_keyb) {
		int se;
		if ((se = icoms_poll_user(p->icom, 1)) != ICOM_TRIG) {
			/* Abort, term or command */
			return colorhug_interp_code((inst *)p, icoms2colorhug_err(se));
		}
		user_trig = 1;
		if (p->trig_return)
			printf("\n");
	}

	/* Read the XYZ value */
	if ((rv = colorhug_take_XYZ_measurement(p, val->aXYZ)) != inst_ok) {
		return rv;
	}

	val->XYZ_v = 0;
	val->aXYZ_v = 1;		/* These are absolute XYZ readings ? */
	val->Lab_v = 0;
	val->sp.spec_n = 0;
	val->duration = 0.0;

	if (user_trig)
		return inst_user_trig;
	return rv;
}

/* Insert a colorimetric correction matrix */
inst_code colorhug_col_cor_mat(
inst *pp,
double mtx[3][3]
) {
	colorhug *p = (colorhug *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (mtx == NULL)
		icmSetUnity3x3(p->ccmat);
	else
		icmCpy3x3(p->ccmat, mtx);

	return inst_ok;
}

/* Determine if a calibration is needed */
inst_cal_type colorhug_needs_calibration(inst *pp) {
	if (!pp->gotcoms)
		return inst_no_coms;
	if (!pp->inited)
		return inst_no_init;

	return inst_ok;
}

/* Request an instrument calibration */
inst_code colorhug_calibrate(
inst *pp,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	if (!pp->gotcoms)
		return inst_no_coms;
	if (!pp->inited)
		return inst_no_init;

	id[0] = '\000';
	return inst_unsupported;
}

/* Convert a machine specific error code into an abstract dtp code */
static inst_code
colorhug_interp_code(inst *pp, int ec) {
	ec &= inst_imask;
	switch (ec) {

		case COLORHUG_OK:
			return inst_ok;

		case COLORHUG_INTERNAL_ERROR:
		case COLORHUG_NO_COMS:
		case COLORHUG_NOT_INITED:
			return inst_internal_error | ec;

		case COLORHUG_COMS_FAIL:
			return inst_coms_fail | ec;

		case COLORHUG_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case COLORHUG_UNKNOWN_CMD:
		case COLORHUG_WRONG_UNLOCK_CODE:
		case COLORHUG_NOT_IMPLEMENTED:
		case COLORHUG_UNDERFLOW_SENSOR:
		case COLORHUG_NO_SERIAL:
		case COLORHUG_WATCHDOG:
		case COLORHUG_INVALID_ADDRESS:
		case COLORHUG_INVALID_LENGTH:
		case COLORHUG_INVALID_CHECKSUM:
		case COLORHUG_INVALID_VALUE:
		case COLORHUG_UNKNOWN_CMD_FOR_BOOTLOADER:
		case COLORHUG_NO_CALIBRATION:
		case COLORHUG_OVERFLOW_MULTIPLY:
		case COLORHUG_OVERFLOW_ADDITION:
		case COLORHUG_OVERFLOW_SENSOR:
		case COLORHUG_OVERFLOW_STACK:
		case COLORHUG_BAD_WR_LENGTH:
		case COLORHUG_BAD_RD_LENGTH:
		case COLORHUG_BAD_RET_CMD:
		case COLORHUG_BAD_RET_STAT:
			return inst_protocol_error | ec;

		case COLORHUG_USER_ABORT:
			return inst_user_abort | ec;
		case COLORHUG_USER_TERM:
			return inst_user_term | ec;
		case COLORHUG_USER_TRIG:
			return inst_user_trig | ec;
		case COLORHUG_USER_CMND:
			return inst_user_cmnd | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
colorhug_del(inst *pp) {
	colorhug *p = (colorhug *)pp;
	if (p->icom != NULL)
		p->icom->del(p->icom);
	free(p);
}

/* Return the instrument capabilities */
inst_capability colorhug_capabilities(inst *pp) {
	colorhug *p = (colorhug *)pp;
	inst_capability rv;

	rv = inst_emis_spot
	   | inst_emis_disp
	   | inst_emis_disptype
	   | inst_colorimeter
	   | inst_ccmx
	;

	return rv;
}

/* Return the instrument capabilities 2 */
inst2_capability colorhug_capabilities2(inst *pp) {
	colorhug *p = (colorhug *)pp;
	inst2_capability rv = 0;

	rv |= inst2_prog_trig;
	rv |= inst2_keyb_trig;
	rv |= inst2_has_leds;

	return rv;
}

/* The HW handles up to 6 */
inst_disptypesel colorhug_disptypesel[5] = {
	{
		1,
		"l",
		"ColorHug: LCD, CCFL Backlight [Default]",
		0
	},
	{
		2,
		"c",
		"ColorHug: CRT display",
		1
	},
	{
		3,
		"p",
		"ColorHug: Projector",
		0
	},
	{
		4,
		"e",
		"ColorHug: LCD, White LED Backlight",
		0
	},
	{
		0,
		"",
		"",
		-1
	}
};

/* Get mode and option details */
static inst_code colorhug_get_opt_details(
inst *pp,
inst_optdet_type m,	/* Requested option detail type */
...) {				/* Status parameters */                             
	colorhug *p = (colorhug *)pp;
	inst_code rv = inst_ok;

	if (m == inst_optdet_disptypesel) {
		va_list args;
		int *pnsels;
		inst_disptypesel **psels;

		va_start(args, m);
		pnsels = va_arg(args, int *);
		psels = va_arg(args, inst_disptypesel **);
		va_end(args);

		*pnsels = 4;
		*psels = colorhug_disptypesel;
		
		return inst_ok;
	}

	return inst_unsupported;
}

/* Set device measurement mode */
inst_code colorhug_set_mode(inst *pp, inst_mode m)
{
	colorhug *p = (colorhug *)pp;
	inst_mode mm;		/* Measurement mode */

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* The measurement mode portion of the mode */
	mm = m & inst_mode_measurement_mask;

	/* only display emission mode and ambient supported */
	if (mm != inst_mode_emis_spot
	 && mm != inst_mode_emis_disp
	 && mm != inst_mode_emis_ambient) {
		return inst_unsupported;
	}

	/* Spectral mode is not supported */
	if (m & inst_mode_spectral)
		return inst_unsupported;

	p->mode = m;
	return inst_ok;
}

/* Set or reset an optional mode */
static inst_code
colorhug_set_opt_mode(inst *pp, inst_opt_mode m, ...)
{
	colorhug *p = (colorhug *)pp;
	inst_code ev = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Set the display type */
	if (m == inst_opt_disp_type) {
		va_list args;
		int ix;

		va_start(args, m);
		ix = va_arg(args, int);
		va_end(args);

		/* The HW handles up to 6 calibrations */
		if (ix < 0 || ix > 3)
			return inst_unsupported;
		p->calix = ix;
		return inst_ok;
	}

	/* Record the trigger mode */
	if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_keyb) {
		p->trig = m;
		return inst_ok;
	}
	if (m == inst_opt_trig_return) {
		p->trig_return = 1;
		return inst_ok;
	} else if (m == inst_opt_trig_no_return) {
		p->trig_return = 0;
		return inst_ok;
	}

	/* Operate the LEDs */
	if (m == inst_opt_get_gen_ledmask) {
		va_list args;
		int *mask = NULL;

		va_start(args, m);
		mask = va_arg(args, int *);
		va_end(args);
		*mask = 0x3;			/* Two general LEDs */
		return inst_ok;
	} else if (m == inst_opt_get_led_state) {
		va_list args;
		int *mask = NULL;

		va_start(args, m);
		mask = va_arg(args, int *);
		va_end(args);
		*mask = p->led_state;
		return inst_ok;
	} else if (m == inst_opt_set_led_state) {
		va_list args;
		int mask = 0;

		va_start(args, m);
		mask = va_arg(args, int);
		va_end(args);
		return colorhug_set_LEDs(p, mask);
	}

	return inst_unsupported;
}

/* Constructor */
extern colorhug *new_colorhug(icoms *icom, instType itype, int debug, int verb)
{
	colorhug *p;
	if ((p = (colorhug *)calloc(sizeof(colorhug),1)) == NULL)
		error("colorhug: malloc failed!");

	if (icom == NULL)
		p->icom = new_icoms();
	else
		p->icom = icom;

	p->debug = debug;
	p->verb = verb;

	/* Set the colorimeter correction matrix to do nothing */
	icmSetUnity3x3(p->ccmat);

	p->init_coms         = colorhug_init_coms;
	p->init_inst         = colorhug_init_inst;
	p->capabilities      = colorhug_capabilities;
	p->capabilities2     = colorhug_capabilities2;
	p->get_opt_details   = colorhug_get_opt_details;
	p->set_mode          = colorhug_set_mode;
	p->set_opt_mode      = colorhug_set_opt_mode;
	p->read_sample       = colorhug_read_sample;
	p->needs_calibration = colorhug_needs_calibration;
	p->col_cor_mat       = colorhug_col_cor_mat;
	p->calibrate         = colorhug_calibrate;
	p->interp_error      = colorhug_interp_error;
	p->del               = colorhug_del;

	p->itype = itype;

	return p;
}

