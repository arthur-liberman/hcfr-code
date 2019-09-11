

/*
 * Argyll Color Correction System
 *
 * Hughski ColorHug related functions
 *
 * Author: Richard Hughes
 * Date:   30/11/2011
 *
 * Copyright 2006 - 2014, Graeme W. Gill
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
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "colorhug.h"

static inst_code colorhug_interp_code(inst *pp, int ec);

/* Interpret an icoms error into a ColorHug error */
static int icoms2colorhug_err(int se) {
	if (se != ICOM_OK)
		return COLORHUG_COMS_FAIL;
	return COLORHUG_OK;
}

/* ColorHug commands that we care about */
typedef enum {
	ch_set_mult		        = 0x04,		/* Set multiplier value */
	ch_set_integral	        = 0x06,		/* Set integral time */
	ch_get_firmware_version	= 0x07,		/* Get the Firmware version number */
	ch_get_serial	        = 0x0b,		/* Gets the serial number */
	ch_set_leds		        = 0x0e,		/* Sets the LEDs */
	ch_take_reading         = 0x22,		/* Takes a raw reading minus dark offset */
	ch_take_reading_xyz	    = 0x23,		/* Takes an XYZ reading using the current matrix */
	ch_get_post_scale       = 0x2a		/* Get the post scaling factor */
} ColorHugCmd;

/* Diagnostic - return a description given the instruction code */
static char *inst_desc(int cc) {
	static char buf[40];
	switch(cc) {
	case 0x04:
		return "SetMultiplier";
	case 0x06:
		return "SetIntegral";
	case 0x07:
		return "GetFirmwareVersion";
	case 0x0b:
		return "GetSerial";
	case 0x0e:
		return "SetLeds";
	case 0x22:
		return "TakeReading";
	case 0x23:
		return "TakeReadingXYZ";
	case 0x2a:
		return "GetPostScale";
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
		case COLORHUG_DEVICE_DEACTIVATED:
			return "Device deactivated";
		case COLORHUG_INCOMPLETE_REQUEST:
			return "Incomplete request";

		/* Internal errors */
		case COLORHUG_NO_COMS:
			return "Communications hasn't been established";
		case COLORHUG_NOT_INITED:
			return "Instrument hasn't been initialised";
		case COLORHUG_WRONG_MODEL:
			return "Attempt to use wrong command for model";
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
	int xwbytes, wbytes;
	int xrbytes, xrbytes2, rbytes;
	int se, ua = 0, rv = inst_ok;
	int ishid = p->icom->port_type(p->icom) == icomt_hid;

	a1logd(p->log,5,"colorhg_command: sending cmd '%s' args '%s'\n",
				           inst_desc(cmd), icoms_tohex(in, in_size));

	/* Send the command with any specified data */
	memset(buf, 0, 64);
	buf[0] = cmd;
	if (in != NULL)
		memcpy(buf + 1, in, in_size);
	if (ishid) {
		xwbytes = 64;
		se = p->icom->hid_write(p->icom, buf, xwbytes, &wbytes, timeout);
	} else {
//		xwbytes = in_size + 1;		/* cmd + arguments */
		xwbytes = 64;
		se = p->icom->usb_write(p->icom, NULL, 0x01, buf, xwbytes, &wbytes, timeout);
	}
	a1logd(p->log,8,"colorhug_command: Send %d bytes and %d sent\n",xwbytes,wbytes);
	if (se != 0) {
		a1logd(p->log,1,"colorhug_command: command send failed with ICOM err 0x%x\n",se);
		return colorhug_interp_code((inst *)p, COLORHUG_COMS_FAIL);
	}
	rv = colorhug_interp_code((inst *)p, icoms2colorhug_err(ua));
	if (rv == inst_ok && wbytes != xwbytes)
		rv = colorhug_interp_code((inst *)p, COLORHUG_BAD_WR_LENGTH);
	a1logd(p->log,6,"colorhug_command: got inst code \n",rv);

	if (rv != inst_ok) {
		/* Flush any response if write failed */
		if (ishid)
			p->icom->hid_read(p->icom, buf, 64, &rbytes, timeout);
		else
			p->icom->usb_read(p->icom, NULL, 0x81, buf, out_size + 2, &rbytes, timeout);
		return rv;
	}

	/* Now fetch the response */
	a1logd(p->log,6,"colorhug_command: Reading response\n");

	if (ishid) {
		xrbytes = xrbytes2 = 64;
		se = p->icom->hid_read(p->icom, buf, xrbytes, &rbytes, timeout);
	} else {
		xrbytes = 64;
		xrbytes2 = out_size + 2;	/* For backwards compatibility with fw <= 1.1.8 */
		se = p->icom->usb_read(p->icom, NULL, 0x81, buf, xrbytes, &rbytes, timeout);
	}

	a1logd(p->log,8,"colorhug_command: Read %d bytes and %d read\n",xrbytes,rbytes);
	if (rbytes >= 2) {
		a1logd(p->log,6,"colorhug_command: recieved cmd '%s' error '%s' args '%s'\n",
				inst_desc(buf[1]),
				colorhug_interp_error((inst *) p, buf[0]),
				icoms_tohex(buf, rbytes - 2));
	}

	if (se != 0) {

		/* deal with command error */
		if (buf[0] != COLORHUG_OK) {
			a1logd(p->log,1,"colorhug_command: Got Colorhug !OK\n");
			rv = colorhug_interp_code((inst *)p, buf[0]);
			return rv;
		}

		/* deal with underrun or overrun */
		if (rbytes != xrbytes
		 && rbytes != xrbytes2) {
			a1logd(p->log,1,"colorhug_command: got underrun or overrun\n");
			rv = colorhug_interp_code((inst *)p, COLORHUG_BAD_RD_LENGTH);
			return rv;
		}

		if (se != ICOM_SHORT) {		/* Allow short read for firware compatibility */
			/* there's another reason it failed */
			a1logd(p->log,1,"colorhug_command: read failed with ICOM err 0x%x\n",se);
			return colorhug_interp_code((inst *)p, COLORHUG_COMS_FAIL);
		}
	}
	rv = colorhug_interp_code((inst *)p, icoms2colorhug_err(ua));

	/* check the command was the same */
	if (rv == inst_ok && buf[1] != cmd) {
		a1logd(p->log,1,"colorhug_command: command wasn't echo'd\n");
		rv = colorhug_interp_code((inst *)p, COLORHUG_BAD_RET_CMD);
		return rv;
	}
	if (rv == inst_ok && out != NULL)
		memcpy(out, buf + 2, out_size);

	a1logd(p->log,5,"colorhg_command: returning '%s' ICOM err 0x%x\n",
				                  icoms_tohex(buf + 2, out_size),ua);
	return rv;
}

/* --------------------------------------------- */
/* Little endian wire format conversion routines */

/* Take an int, and convert it into a byte buffer */
static void int2buf_le(unsigned char *buf, int inv) {
	buf[0] = (inv >> 0) & 0xff;
	buf[1] = (inv >> 8) & 0xff;
	buf[2] = (inv >> 16) & 0xff;
	buf[3] = (inv >> 24) & 0xff;
}

/* Take an unsigned int, and convert it into a byte buffer */
static void uint2buf_le(unsigned char *buf, unsigned int inv) {
	buf[0] = (inv >> 0) & 0xff;
	buf[1] = (inv >> 8) & 0xff;
	buf[2] = (inv >> 16) & 0xff;
	buf[3] = (inv >> 24) & 0xff;
}

/* Take a short, and convert it into a byte buffer */
static void short2buf_le(unsigned char *buf, int inv) {
	buf[0] = (inv >> 0) & 0xff;
	buf[1] = (inv >> 8) & 0xff;
}

/* Take an unsigned short, and convert it into a byte buffer */
static void ushort2buf_le(unsigned char *buf, unsigned int inv) {
	buf[0] = (inv >> 0) & 0xff;
	buf[1] = (inv >> 8) & 0xff;
}

/* Take a word sized buffer, and convert it to an int */
static int buf2int_le(unsigned char *buf) {
	int val;
	val =               buf[3];
	val = ((val << 8) + buf[2]);
	val = ((val << 8) + buf[1]);
	val = ((val << 8) + buf[0]);
	return val;
}

/* Take a word sized buffer, and convert it to an unsigned int */
static unsigned int buf2uint_le(unsigned char *buf) {
	unsigned int val;
	val =               buf[3];
	val = ((val << 8) + buf[2]);
	val = ((val << 8) + buf[1]);
	val = ((val << 8) + buf[0]);
	return val;
}

/* Take a short sized buffer, and convert it to an int */
static int buf2short_le(unsigned char *buf) {
	int val;
	val =               buf[1];
	val = ((val << 8) + buf[0]);
	return val;
}

/* Take an unsigned short sized buffer, and convert it to an int */
static unsigned int buf2ushort_le(unsigned char *buf) {
	unsigned int val;
	val =               buf[1];
	val = ((val << 8) + buf[0]);
	return val;
}

/* --------------------------------------------- */


/* Converts 4 bytes of packed float into a double */
static double buf2pfdouble(unsigned char *buf)
{
 	return (double) buf2int_le(buf) / (double) 0x10000;
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
						  2.0);
	return ev;
}

/* Take a measurement from the device */
/* There are 64 calibration matricies, index 0..63 */
/* 0 is the factory calibration, while 1..63 are */
/* applied on top of the factory calibration as corrections. */
/* Index 64..70 are mapped via the mapping table */
/* to an index between 0 and 63, and notionaly correspond */
/* as follows:       */
/*	LCD       = 0    */
/*	CRT       = 1    */
/*	Projector = 2    */
/*	LED       = 3    */
/*	Custom1   = 4    */
/*	Custom2   = 5    */
static inst_code
colorhug_take_measurement(colorhug *p, double XYZ[3])
{
	inst_code ev;
	int i;
	ORD8 ibuf[2];

	if (!p->inited)
		return colorhug_interp_code((inst *)p, COLORHUG_NOT_INITED);

	if (p->icx == 11) {		/* Raw */
		unsigned char obuf[3 * 4];

		/* Do the measurement, and return the values */
		ev = colorhug_command(p, ch_take_reading,
							  NULL, 0,
							  obuf, 3 * 4,
							  30.0);
		if (ev != inst_ok)
			return ev;
	
		/* Convert to doubles */
		for (i = 0; i < 3; i++) {
//printf("%d raw = 0x%x\n",i,buf2int_le(obuf + i * 4));
			XYZ[i] = p->postscale * buf2pfdouble(obuf + i * 4);
		}
	} else {
		int icx = 64 + p->icx;
		unsigned char obuf[3 * 4];

		if (p->icx == 10)	/* Factory */	
			icx = 0;

		/* Choose the calibration matrix */
		short2buf_le(ibuf + 0, icx);
	
		/* Do the measurement, and return the values */
		ev = colorhug_command(p, ch_take_reading_xyz,
							  ibuf, sizeof (ibuf),
							  obuf, 3 * 4,
							  30.0);
		if (ev != inst_ok)
			return ev;
	
		/* Convert to doubles */
		for (i = 0; i < 3; i++) {
//printf("%d raw = 0x%x\n",i,buf2int_le(obuf + i * 4));
			XYZ[i] = buf2pfdouble(obuf + i * 4);
		}
	}

	/* Apply the colorimeter correction matrix */
	icmMulBy3x3(XYZ, p->ccmat, XYZ);

	a1logd(p->log,3,"colorhug_take_measurement: XYZ = %f %f %f\n",XYZ[0],XYZ[1],XYZ[2]);

	return inst_ok;
}

/* Establish communications with a ColorHug */
static inst_code
colorhug_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	int se;
	colorhug *p = (colorhug *) pp;
	icomuflags usbflags = icomuf_none;

	a1logd(p->log, 2, "colorhug_init_coms: About to init coms\n");

	/* Open as an HID if available */
	if (p->icom->port_type(p->icom) == icomt_hid) {

		a1logd(p->log, 3, "colorhug_init_coms: About to init HID\n");

		/* Set config, interface */
		if ((se = p->icom->set_hid_port(p->icom, usbflags, 0, NULL)) != ICOM_OK) {
			a1logd(p->log, 1, "colorhug_init_coms: set_hid_port failed ICOM err 0x%x\n",se);
			return colorhug_interp_code((inst *)p, icoms2colorhug_err(se));
		}

	} else if (p->icom->port_type(p->icom) == icomt_usb) {

		a1logd(p->log, 3, "colorhug_init_coms: About to init USB\n");

#if defined(UNIX_X11)
		usbflags |= icomuf_detach;
		/* Some Linux drivers can't open the device a second time, so */
		/* use the reset on close workaround. */
		usbflags |= icomuf_reset_before_close;
#endif

		/* Set config, interface, write end point, read end point */
		// ~~ does Linux need icomuf_reset_before_close ? Why ?
		if ((se = p->icom->set_usb_port(p->icom, 1, 0x00, 0x00, usbflags, 0, NULL))
			                                                                 != ICOM_OK) { 
			a1logd(p->log, 1, "colorhug_init_coms: set_usb_port failed ICOM err 0x%x\n",se);
			return colorhug_interp_code((inst *)p, icoms2colorhug_err(se));
		}

	} else {
		a1logd(p->log, 1, "colorhug_init_coms: wrong communications type for device!\n");
		return inst_internal_error;
	}

	a1logd(p->log, 2, "colorhug_init_coms: inited coms OK\n");

	p->gotcoms = 1;
	return inst_ok;
}

/* Get the firmware version */
static inst_code
colorhug_get_firmwareversion (colorhug *p)
{
	inst_code ev;
	unsigned char obuf[6];

	ev = colorhug_command(p, ch_get_firmware_version,
						  NULL, 0,
						  obuf, 6,
						  2.0);
	if (ev != inst_ok)
		return ev;

	p->maj = buf2short_le(obuf + 0);
	p->min = buf2short_le(obuf + 2);
	p->uro = buf2short_le(obuf + 4);

	a1logd(p->log,2,"colorhug: Firware version = %d.%d.%d\n",p->maj,p->min,p->uro); 

	return ev;
}

/* Get the serial number */
static inst_code
colorhug_get_serialnumber (colorhug *p)
{
	inst_code ev;
	unsigned char obuf[6];

	ev = colorhug_command(p, ch_get_serial,
						  NULL, 0,
						  obuf, 4,
						  2.0);
	if (ev != inst_ok)
		return ev;

	p->ser_no = buf2uint_le(obuf + 0);

	a1logd(p->log,2,"colorhug: Serial number = %d\n",p->ser_no); 

	return ev;
}

/* Set the device multiplier */
static inst_code
colorhug_set_multiplier (colorhug *p, int multiplier)
{
	inst_code ev;
	unsigned char ibuf[1];

	if (p->stype != ch_one)
		return colorhug_interp_code((inst *)p, COLORHUG_WRONG_MODEL);

	/* Set the desired multiplier */
	ibuf[0] = multiplier;
	ev = colorhug_command(p, ch_set_mult,
						  ibuf, sizeof (ibuf),
						  NULL, 0,
						  2.0);
	return ev;
}

/* Set the device integral time */
static inst_code
colorhug_set_integral (colorhug *p, int integral)
{
	inst_code ev;
	unsigned char ibuf[2];

	if (p->stype != ch_one)
		return colorhug_interp_code((inst *)p, COLORHUG_WRONG_MODEL);

	/* Set the desired integral time */
	short2buf_le(ibuf + 0, integral);
	ev = colorhug_command(p, ch_set_integral,
						  ibuf, sizeof (ibuf),
						  NULL, 0,
						  2.0);
	return ev;
}

/* Get the post scale factor */
static inst_code
colorhug_get_postscale (colorhug *p, double *postscale)
{
	inst_code ev;
	unsigned char obuf[4];

	/* Hmm. The post scale is in the 2nd short returned */
	ev = colorhug_command(p, ch_get_post_scale,
						  NULL, 0,
						  obuf, 4,
						  2.0);
	*postscale = buf2pfdouble(obuf);
	return ev;
}

static inst_code set_default_disp_type(colorhug *p);

/* Initialise the ColorHug */
static inst_code
colorhug_init_inst(inst *pp)
{
	colorhug *p = (colorhug *)pp;
	inst_code ev;
	int i;

	a1logd(p->log, 2, "colorhug_init_coms: About to init coms\n");

	/* Must establish coms first */
	if (p->gotcoms == 0)
		return colorhug_interp_code((inst *)p, COLORHUG_NO_COMS);

	/* Get the firmware version */
	ev = colorhug_get_firmwareversion(p);
	if (ev != inst_ok)
		return ev;

	/* Get the serial number */
	ev = colorhug_get_serialnumber(p);
	if (ev != inst_ok)
		return ev;

	/* Turn the LEDs off */
	ev = colorhug_set_LEDs(p, 0x0);
	if (ev != inst_ok)
		return ev;

	if (p->stype == ch_one) {

		/* Turn the sensor on */
		ev = colorhug_set_multiplier(p, 0x03);
		if (ev != inst_ok)
			return ev;

		/* Set the integral time to maximum precision */
		ev = colorhug_set_integral(p, 0xffff);
		if (ev != inst_ok)
			return ev;
	}

	if (p->maj <= 1 && p->min <= 1 && p->uro <= 4) {

		/* Get the post scale factor */
		ev = colorhug_get_postscale(p, &p->postscale);
		if (ev != inst_ok)
			return ev;

	
	/* In firmware >= 1.1.5, the postscale is done in the firmware */
	} else {
		p->postscale = 1.0;
	}

	p->trig = inst_opt_trig_user;

	/* Setup the default display type */
	if ((ev = set_default_disp_type(p)) != inst_ok) {
		return ev;
	}

	p->inited = 1;
	a1logd(p->log, 2, "colorhug_init: inited coms OK\n");

	a1logv(p->log,1,"Serial Number:     %06u\n"
	                "Firmware Version:  %d.%d.%d\n"
	                ,p->ser_no,p->maj,p->min,p->uro);

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

	return inst_ok;
}

/* Read a single sample */
static inst_code
colorhug_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	colorhug *p = (colorhug *)pp;
	int user_trig = 0;
	int rv = inst_protocol_error;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->trig == inst_opt_trig_user) {

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "colorhug: inst_opt_trig_user but no uicallback function set!\n");
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
		/* Check for abort */
		if (p->uicallback != NULL
		 && (rv = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort)
			return rv;				/* Abort */
	}

	/* Read the XYZ value */
	rv = colorhug_take_measurement(p, val->XYZ);

	if (rv != inst_ok)
		return rv;

	/* This may not change anything since instrument may clamp */
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);

	val->mtype = inst_mrt_emission;
	val->XYZ_v = 1;		/* These are absolute XYZ readings ? */
	val->sp.spec_n = 0;
	val->duration = 0.0;


	if (user_trig)
		return inst_user_trig;
	return rv;
}

static inst_code set_base_disp_type(colorhug *p, int cbid);

/* Insert a colorimetric correction matrix */
inst_code colorhug_col_cor_mat(
inst *pp,
disptech dtech,		/* Use disptech_unknown if not known */
int cbid,       	/* Calibration display type base ID required, 1 if unknown */
double mtx[3][3]
) {
	colorhug *p = (colorhug *)pp;
	inst_code ev;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if ((ev = set_base_disp_type(p, cbid)) != inst_ok)
		return ev;
	if (mtx == NULL)
		icmSetUnity3x3(p->ccmat);
	else
		icmCpy3x3(p->ccmat, mtx);

	p->dtech = dtech;
	p->refrmode = disptech_get_id(dtech)->refr;
	p->cbid = 0;	/* Base has been overwitten */

	if (p->log->debug >= 4) {
		a1logd(p->log,4,"ccmat           = %f %f %f\n",
		                 p->ccmat[0][0], p->ccmat[0][1], p->ccmat[0][2]);
		a1logd(p->log,4,"                  %f %f %f\n",
		                 p->ccmat[1][0], p->ccmat[1][1], p->ccmat[1][2]);
		a1logd(p->log,4,"                  %f %f %f\n\n",
		                 p->ccmat[2][0], p->ccmat[2][1], p->ccmat[2][2]);
		a1logd(p->log,4,"ucbid = %d, cbid = %d\n",p->ucbid, p->cbid);
		a1logd(p->log,4,"\n");
	}

	return inst_ok;
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
		case COLORHUG_WRONG_MODEL:
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
		case COLORHUG_DEVICE_DEACTIVATED:
		case COLORHUG_INCOMPLETE_REQUEST:
		case COLORHUG_BAD_WR_LENGTH:
		case COLORHUG_BAD_RD_LENGTH:
		case COLORHUG_BAD_RET_CMD:
		case COLORHUG_BAD_RET_STAT:
			return inst_protocol_error | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
colorhug_del(inst *pp) {
	colorhug *p = (colorhug *)pp;
	if (p != NULL) {
		if (p->icom != NULL)
			p->icom->del(p->icom);
		inst_del_disptype_list(p->dtlist, p->ndtlist);
		p->vdel(pp);
		free(p);
	}
}

/* Return the instrument mode capabilities */
void colorhug_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	colorhug *p = (colorhug *)pp;
	inst_mode cap = 0;
	inst2_capability cap2 = 0;

	cap |= inst_mode_emis_spot
	    |  inst_mode_colorimeter
	       ;

	cap2 |= inst2_prog_trig
	     |  inst2_user_trig
	     |  inst2_has_leds
	     |  inst2_disptype
	     |  inst2_ccmx
	        ;

	if (pcap1 != NULL)
		*pcap1 = cap;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = inst3_none;
}

/* Check device measurement mode */
inst_code colorhug_check_mode(inst *pp, inst_mode m) {
	colorhug *p = (colorhug *)pp;
	inst_mode cap;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	pp->capabilities(pp, &cap, NULL, NULL);

	/* Simple test */
	if (m & ~cap)
		return inst_unsupported;

	/* only display emission mode and ambient supported */
	if (!IMODETST(m, inst_mode_emis_spot)
	 && !IMODETST(m, inst_mode_emis_ambient)) {
		return inst_unsupported;
	}

	return inst_ok;
}

/* Set device measurement mode */
inst_code colorhug_set_mode(inst *pp, inst_mode m) {
	colorhug *p = (colorhug *)pp;
	inst_code ev;

	if ((ev = colorhug_check_mode(pp, m)) != inst_ok)
		return ev;

	p->mode = m;

	return inst_ok;
}

/* The HW handles up to 6, + 2 special */
static inst_disptypesel colorhug_disptypesel[7] = {
	{
		inst_dtflags_default,				/* flags */
		0,									/* cbix */
		"l",								/* sel */
		"LCD, CCFL Backlight",				/* desc */
		0,									/* refr */
		disptech_lcd_ccfl,					/* disptype */
		0									/* ix */
	},
	{
		inst_dtflags_none,
		0,
		"c",
		"CRT display",
		0,
		disptech_crt,
		1
	},
	{
		inst_dtflags_none,
		0,
		"p",
		"Projector",
		0,
		disptech_dlp,
		2
	},
	{
		inst_dtflags_none,
		0,
		"e",
		"LCD, White LED Backlight",
		0,
		disptech_lcd_wled,
		3
	},
	{
		inst_dtflags_none,
		1,
		"F",
		"Factory matrix (For Calibration)",
		0,
		disptech_unknown,
		10
	},
	{
		inst_dtflags_none,
		2,
		"R",
		"Raw Reading (For Factory matrix Calibration)",
		0,
		disptech_unknown,
		11
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
static inst_code colorhug_get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	colorhug *p = (colorhug *)pp;
	inst_code rv = inst_ok;

	/* Create/Re-create a current list of abailable display types */
	if (p->dtlist == NULL || recreate) {
		if ((rv = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    colorhug_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return rv;
	}

	if (pnsels != NULL)
		*pnsels = p->ndtlist;

	if (psels != NULL)
		*psels = p->dtlist;

	return inst_ok;
}

/* Given a display type implementation from inst_disptypesel */
static inst_code set_disp_type(colorhug *p, inst_disptypesel *dentry) {
	int ix;

	if (dentry->flags & inst_dtflags_ccmx) {
		inst_code ev;
		if ((ev = set_base_disp_type(p, dentry->cc_cbid)) != inst_ok)
			return ev;
		icmCpy3x3(p->ccmat, dentry->mat);
		p->dtech = dentry->dtech;
		p->cbid = 0; 	/* Can't be a base type */

	} else {

		/* The HW handles up to 6 calibrations */
		ix = dentry->ix; 
		if (ix != 10 && ix != 11 && (ix < 0 || ix > 3))
			return inst_unsupported;
	
		p->icx = ix;
		p->dtech = dentry->dtech;
		p->cbid = dentry->cbid; 
		p->ucbid = dentry->cbid;	/* This is underying base if dentry is base selection */
		icmSetUnity3x3(p->ccmat);
	}
	p->refrmode = dentry->refr; 

	if (p->log->debug >= 4) {
		a1logd(p->log,4,"ccmat           = %f %f %f\n",
		                 p->ccmat[0][0], p->ccmat[0][1], p->ccmat[0][2]);
		a1logd(p->log,4,"                  %f %f %f\n",
		                 p->ccmat[1][0], p->ccmat[1][1], p->ccmat[1][2]);
		a1logd(p->log,4,"                  %f %f %f\n\n",
		                 p->ccmat[2][0], p->ccmat[2][1], p->ccmat[2][2]);
		a1logd(p->log,4,"ucbid = %d, cbid = %d\n",p->ucbid, p->cbid);
		a1logd(p->log,4,"\n");
	}

	return inst_ok;
}

/* Set the display type */
static inst_code colorhug_set_disptype(inst *pp, int ix) {
	colorhug *p = (colorhug *)pp;
	inst_code ev;
	inst_disptypesel *dentry;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    colorhug_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return ev;
	}

	if (ix < 0 || ix >= p->ndtlist)
		return inst_unsupported;

	dentry = &p->dtlist[ix];

	if ((ev = set_disp_type(p, dentry)) != inst_ok) {
		return ev;
	}

	return inst_ok;
}

/* Setup the default display type */
static inst_code set_default_disp_type(colorhug *p) {
	inst_code ev;
	int i;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
		    colorhug_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return ev;
	}

	for (i = 0; !(p->dtlist[i].flags & inst_dtflags_end); i++) {
		if (p->dtlist[i].flags & inst_dtflags_default)
			break;
	}
	if (p->dtlist[i].flags & inst_dtflags_end) {
		a1loge(p->log, 1, "set_default_disp_type: failed to find type!\n");
		return inst_internal_error; 
	}
	if ((ev = set_disp_type(p, &p->dtlist[i])) != inst_ok) {
		return ev;
	}

	return inst_ok;
}

/* Setup the display type to the given base type */
static inst_code set_base_disp_type(colorhug *p, int cbid) {
	inst_code ev;
	int i;

	if (cbid == 0) {
		a1loge(p->log, 1, "colorhug set_base_disp_type: can't set base display type of 0\n");
		return inst_wrong_setup;
	}
	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
		    colorhug_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return ev;
	}

	for (i = 0; !(p->dtlist[i].flags & inst_dtflags_end); i++) {
		if (!(p->dtlist[i].flags & inst_dtflags_ccmx)		/* Prevent infinite recursion */
		 && p->dtlist[i].cbid == cbid)
			break;
	}
	if (p->dtlist[i].flags & inst_dtflags_end) {
		a1loge(p->log, 1, "set_base_disp_type: failed to find cbid %d!\n",cbid);
		return inst_wrong_setup; 
	}
	if ((ev = set_disp_type(p, &p->dtlist[i])) != inst_ok) {
		return ev;
	}

	return inst_ok;
}

/* Get the disptech and other corresponding info for the current */
/* selected display type. Returns disptype_unknown by default. */
/* Because refrmode can be overridden, it may not match the refrmode */
/* of the dtech. (Pointers may be NULL if not needed) */
static inst_code colorhug_get_disptechi(
inst *pp,
disptech *dtech,
int *refrmode,
int *cbid) {
	colorhug *p = (colorhug *)pp;
	if (dtech != NULL)
		*dtech = p->dtech;
	if (refrmode != NULL)
		*refrmode = p->refrmode;
	if (cbid != NULL)
		*cbid = p->cbid;
	return inst_ok;
}

/*
 * Set or reset an optional mode.
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 */
static inst_code
colorhug_get_set_opt(inst *pp, inst_opt_type m, ...) {
	colorhug *p = (colorhug *)pp;
	inst_code ev = inst_ok;

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

/* Constructor */
extern colorhug *new_colorhug(icoms *icom, instType dtype) {
	colorhug *p;
	int i;

	if ((p = (colorhug *)calloc(sizeof(colorhug),1)) == NULL) {
		a1loge(icom->log, 1, "new_colorhug: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	p->init_coms         = colorhug_init_coms;
	p->init_inst         = colorhug_init_inst;
	p->capabilities      = colorhug_capabilities;
	p->check_mode        = colorhug_check_mode;
	p->set_mode          = colorhug_set_mode;
	p->get_disptypesel   = colorhug_get_disptypesel;
	p->set_disptype      = colorhug_set_disptype;
	p->get_disptechi     = colorhug_get_disptechi;
	p->get_set_opt       = colorhug_get_set_opt;
	p->read_sample       = colorhug_read_sample;
	p->col_cor_mat       = colorhug_col_cor_mat;
	p->interp_error      = colorhug_interp_error;
	p->del               = colorhug_del;

	p->icom = icom;
	p->dtype = dtype;

	if (dtype == instColorHug2)
		p->stype = ch_two;

	icmSetUnity3x3(p->ccmat);
	p->dtech = disptech_unknown;

	return p;
}

