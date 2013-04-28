
/* 
 * Argyll Color Correction System
 *
 * GretagMacbeth Huey related functions
 *
 * Author: Graeme W. Gill
 * Date:   18/10/2006
 *
 * Copyright 2006 - 2013, Graeme W. Gill
 * All rights reserved.
 *
 * (Based on i1disp.c)
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
#include "huey.h"

#define dbgo stderr

static inst_code huey_interp_code(inst *pp, int ec);
static inst_code huey_check_unlock(huey *p);

#define CALFACTOR 3.428         	/* Emissive magic calibration factor */
#define AMB_SCALE_FACTOR 5.772e-3	/* Ambient mode scale factor */ 
									/* This is only approximate, and were derived */
									/* by matching readings from the i1pro. */

/* ------------------------------------------------------------------------ */
/* Implementation */

/* Interpret an icoms error into a HUEY error */
/* If torc is nz, then a trigger or command is OK, */
/* othewise  they are treated as an abort. */
static int icoms2huey_err(int se, int torc) {
	if (se != ICOM_OK)
		return HUEY_COMS_FAIL;
	return HUEY_OK;
}

/* i1Display command codes */
/* B = byte (8bit), S = short (16bit), W = word (32bit), A = string */
/* U = unused byte, - = no arguments/results */
/* The is a 7 byte command buffer and 6 response recieve buffer. */
/* :2 means the read is from a second 8 byte ep x81 read. */
/* cbuf[-] is command byte */
/* rbuf[-2] is continuation byte */
/* rbuf[-1] is echo of command byte */
/* rbuf2[-2] is an error byte if nz */
typedef enum {
    i1d_status       = 0x00,		/* -:A         Get status string starting at 0, 1sec */
    i1d_rd_green     = 0x02,		/* -:W         Read the green channel, 1sec */
    i1d_rd_blue      = 0x03,		/* -:W         Read the blue channel, 1sec */
    i1d_setintgt     = 0x05,		/* W:-         Set the integration time, 1sec */
    i1d_getintgt     = 0x06,		/* -:W         Get the integration time, 1sec */
    i1d_wrreg        = 0x07,		/* BB:?        Write a register value, 1sec */
    i1d_rdreg        = 0x08,		/* B:B         Read a register value, 1sec */
    i1d_unlock       = 0x0e,		/* BBBB:-      Unlock the interface */
    i1d_m_red_2      = 0x13,		/* B:2:W       Measure the red channel in freq mode, 1,10sec */
									/* B = sync mode, typically 2 */
    i1d_m_rgb_edge_2 = 0x16,		/* SSS:2:WB    Measure RGB edge/period mode, 1.70sec, ret red */
									/* 2nd return value is not used ? */
    i1d_rdambient    = 0x17,		/* BB:2:BWB    Read Ambient, 1,10sec */
									/* Returns first B param as first response */
									/* Returns W as value read */
									/* Returns aditional byte at end */

    i1d_set_leds     = 0x18, 		/* BB:B        Set 4 LEDs state, 1sec */
									/* 1st B is always 0 in practice */
									/* 2nd B bits 0-4, 0 == on */
									/* Echo's led state with returned B */
    i1d_rgb_edge_3   = 0x19 		/* SB:2:WB  Unknown measurement command 1,10sec */
									/* S is number of edges ?? */
									/* B is the channel */
									/* W is the reading */
									/* B is not used ? */

} i1DispCC;

/* Diagnostic - return a description given the instruction code */
static char *inst_desc(int cc) {
	static char buf[40];			/* Fallback string */
	switch(cc) {
		case 0x00:
			return "GetStatus";
		case 0x02:
			return "RdGreen";
		case 0x03:
			return "RdBlue";
		case 0x05:
			return "SetIntTime";
		case 0x06:
			return "GetIntTime";
		case 0x07:
			return "WrReg";
		case 0x08:
			return "RdReg";
		case 0x09:
			return "GetMeasPeriod";
		case 0x0e:
			return "Unlock";
		case 0x13:
			return "RdRedFreqMode";
		case 0x16:
			return "MeasRGBPeriMode";
		case 0x17:
			return "RdAmbient";
		case 0x18:
			return "SetLEDs";
		case 0x19:
			return "MeasRGBPeriMode2";
	}
	sprintf(buf,"Unknown %02x",cc);
	return buf;
}

/* Do a command/response exchange with the huey. */
/* Return the error code */
/* The Huey is set up as an HID device, which can ease the need */
/* for providing a kernel driver on MSWindows systems, */
/* but it doesn't seem to actually be used as an HID device. */
/* We allow for communicating via libusb, or an HID driver. */
static inst_code
huey_command(
	huey *p,					/* huey object */
	i1DispCC cc,				/* Command code */
	unsigned char *in,			/* 7 Command bytes to send */
	unsigned char *out,			/* 6 Response bytes returned */
	double to,					/* Timeout in seconds */
	double to2					/* Timeout in seconds for 2nd read */
) {
	int i;
	unsigned char buf[8];	/* 8 bytes to write/read */
	int wbytes;				/* bytes written */
	int rbytes;				/* bytes read from ep */
	int se, ua = 0, rv = inst_ok;
	int ishid = p->icom->port_type(p->icom) == icomt_hid;

	a1logd(p->log,5,"huey_command: Sending '%s' args '%s'\n",inst_desc(cc), icoms_tohex(in, 7));

	/* Send the command using the control interface */
	buf[0] = cc;				/* Construct the command == HID report number */
	memmove(buf + 1, in, 7);

	if (ishid) {
		se = p->icom->hid_write(p->icom, buf, 8, &wbytes, to); 
	} else {
		se = p->icom->usb_control(p->icom,
		      IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_CLASS | IUSB_REQ_RECIP_INTERFACE, 0x9, 0x200, 0, buf, 8, to);
		wbytes = 8;
	}
	if (se != 0) {
		a1logd(p->log,1,"huey_command: command send failed with ICOM err 0x%x\n",se);
		return huey_interp_code((inst *)p, HUEY_COMS_FAIL);
	}
	rv = huey_interp_code((inst *)p, icoms2huey_err(ua, 0));

	if (rv == inst_ok && wbytes != 8)
		rv = huey_interp_code((inst *)p, HUEY_BAD_WR_LENGTH);

	a1logd(p->log,6,"huey_command: get inst code\n",rv);

	if (rv != inst_ok) {
		/* Flush any response if write failed */
		if (ishid)
			p->icom->hid_read(p->icom, buf, 8, &rbytes, to);
		else
			p->icom->usb_read(p->icom, NULL, 0x81, buf, 8, &rbytes, to);
		return rv;
	}

	/* Now fetch the response */
	a1logd(p->log,6,"huey_command: Reading response\n");

	if (ishid) {
		se = p->icom->hid_read(p->icom, buf, 8, &rbytes, to);
	} else {
		se = p->icom->usb_read(p->icom, NULL, 0x81, buf, 8, &rbytes, to);
	} 
	if (se != 0) {
		a1logd(p->log,1,"huey_command: read failed with ICOM err 0x%x\n",se);
		return huey_interp_code((inst *)p, HUEY_COMS_FAIL);
	}
	rv = huey_interp_code((inst *)p, icoms2huey_err(ua, 0));
	if (rv == inst_ok && rbytes != 8)
		rv = huey_interp_code((inst *)p, HUEY_BAD_RD_LENGTH);
	if (rv == inst_ok && buf[1] != cc)
		rv = huey_interp_code((inst *)p, HUEY_BAD_RET_CMD);
		
	/* Some commands don't use the first response, but need to */
	/* fetch a second response, with a longer timeout. */
	/* This seems to be all of the measurement trigger commands */
	if (rv == inst_ok && buf[0] == 0x90) {	/* there is more */
		a1logd(p->log,6,"huey_command: Reading extended response\n");

		if (ishid) {
			se = p->icom->hid_read(p->icom, buf, 8, &rbytes, to2);
		} else {
			se = p->icom->usb_read(p->icom, NULL, 0x81, buf, 8, &rbytes, to2);
		} 
		if (se != 0) {
			a1logd(p->log,1,"huey_command: read failed with ICOM err 0x%x\n",se);
			return huey_interp_code((inst *)p, HUEY_COMS_FAIL);
		}
		rv = huey_interp_code((inst *)p, icoms2huey_err(ua, 0));
		if (rv == inst_ok && rbytes != 8)
			rv = huey_interp_code((inst *)p, HUEY_BAD_RD_LENGTH);
		if (rv == inst_ok && buf[1] != cc) {
			rv = huey_interp_code((inst *)p, HUEY_BAD_RET_CMD);
		}
	}

	/* The first byte returned seems to be a  command result error code. */
	/* Not all codes are known, but it seems that the 6 byte payload */
	/* is an error message, for instance 0x80 "NoCmd". */
	/* The second byte is always the command code being echo'd back. */
	if (rv == inst_ok && cc != 0x00 && buf[0] != 0x00) {
		ua = HUEY_BAD_RET_STAT;
		if (buf[0] == 0x80)
			ua = HUEY_BAD_COMMAND;
		rv = huey_interp_code((inst *)p, ua);
	}

	if (rv == inst_ok) {
		memmove(out, buf + 2, 6);
	} else {
		memset(out, 0, 6);
	}
	a1logd(p->log,5,"huey_command: returning '%s' ICOM err 0x%x\n",icoms_tohex(out, 6),ua);

	return rv; 
}

/* Take an int, and convert it into a byte buffer */
static void int2buf(unsigned char *buf, int inv) {
	buf[0] = (inv >> 24) & 0xff;
	buf[1] = (inv >> 16) & 0xff;
	buf[2] = (inv >> 8) & 0xff;
	buf[3] = (inv >> 0) & 0xff;
}

/* Take a short, and convert it into a byte buffer */
static void short2buf(unsigned char *buf, int inv) {
	buf[0] = (inv >> 8) & 0xff;
	buf[1] = (inv >> 0) & 0xff;
}

/* Take a word sized return buffer, and convert it to an int */
static int buf2int(unsigned char *buf) {
	int val;
	val = buf[0];
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[3]));
	return val;
}

/* Read a byte from a register */
static inst_code
huey_rdreg_byte(
	huey *p,				/* Object */
	int *outp,				/* Where to write value */
	int addr				/* Register Address, 0 - 255 */
) {
	unsigned char buf[8];
	int rsize;
	inst_code ev;

	if (addr < 0 || addr > 255)
		return huey_interp_code((inst *)p, HUEY_BAD_REG_ADDRESS);

	/* Read a byte */
	memset(buf, 0, 7);
	buf[0] = addr;
	if ((ev = huey_command(p, i1d_rdreg, buf, buf, 1.0, 1.0)) != inst_ok)
		return ev;
	
	/* We expect the address to be returned */
	if ((buf[0] & 0xff) != addr)
		return huey_interp_code((inst *)p, HUEY_UNEXPECTED_RET_VAL);
		
	*outp = (int)(buf[1] & 0xff);

	return inst_ok;
}

/* Read a short from a register */
static inst_code
huey_rdreg_short(
	huey *p,				/* Object */
	int *outp,				/* Where to write value */
	int addr				/* Register Address, 0 - 126 */
) {
	inst_code ev;
	int v, val;

	if ((ev = huey_rdreg_byte(p, &v, addr)) != inst_ok)
		return ev;
	val = v;

	if ((ev = huey_rdreg_byte(p, &v, addr+1)) != inst_ok)
		return ev;
	val = ((val << 8) + (0xff & v));

	*outp = val;

	return inst_ok;
}

/* Read a word from a register */
static inst_code
huey_rdreg_word(
	huey *p,				/* Object */
	int *outp,				/* Where to write value */
	int addr				/* Register Address, 0 - 124 */
) {
	inst_code ev;
	int v, val;

	if ((ev = huey_rdreg_byte(p, &v, addr)) != inst_ok)
		return ev;
	val = v;

	if ((ev = huey_rdreg_byte(p, &v, addr+1)) != inst_ok)
		return ev;
	val = ((val << 8) + (0xff & v));

	if ((ev = huey_rdreg_byte(p, &v, addr+2)) != inst_ok)
		return ev;
	val = ((val << 8) + (0xff & v));

	if ((ev = huey_rdreg_byte(p, &v, addr+3)) != inst_ok)
		return ev;
	val = ((val << 8) + (0xff & v));

	*outp = val;

	return inst_ok;
}


/* Read a float from a register */
/* Will return HUEY_FLOAT_NOT_SET if the float value was 0xffffffff */
static inst_code
huey_rdreg_float(
	huey *p,				/* Object */
	double *outp,			/* Where to write value */
	int addr				/* Register Address, 0 - 124 */
) {
	inst_code ev;
	int val;

	if ((ev = huey_rdreg_word(p, &val, addr)) != inst_ok)
		return ev;

	if (ev == 0xffffffff) {
		return inst_ok;
	}

	*outp = IEEE754todouble((unsigned int)val);
	return inst_ok;
}


/* Write a byte to a register */
static inst_code
huey_wrreg_byte(
	huey *p,				/* Object */
	int inv,				/* Input value */
	int addr				/* Register Address, 0 - 127 */
) {
	int cval;
	unsigned char ibuf[8], obuf[8];
	int rsize;
	inst_code ev;

	ibuf[0] = addr;  
	ibuf[1] = inv;  

	/* Write a byte */
	if ((ev = huey_command(p, i1d_wrreg, ibuf, obuf, 1.0, 1.0)) != inst_ok)
		return ev;

	return inst_ok;
}

/* Write a word to a register */
static inst_code
huey_wrreg_word(
	huey *p,				/* Object */
	int inv,				/* Where to write value */
	int addr				/* Register Address, 0 - 124 */
) {
	inst_code ev;
	int v;

	v = (inv >> 24) & 0xff;
	if ((ev = huey_wrreg_byte(p, v, addr) ) != inst_ok)
		return ev;

	v = (inv >> 16) & 0xff;
	if ((ev = huey_wrreg_byte(p, v, addr+1) ) != inst_ok)
		return ev;

	v = (inv >> 8) & 0xff;
	if ((ev = huey_wrreg_byte(p, v, addr+2) ) != inst_ok)
		return ev;

	v = (inv >> 0) & 0xff;
	if ((ev = huey_wrreg_byte(p, v, addr+3) ) != inst_ok)
		return ev;

	return inst_ok;
}

/* Write a float to a register */
static inst_code
huey_wrreg_float(
	huey *p,				/* Object */
	double inv,				/* Value to write */
	int addr				/* Register Address, 0 - 124 */
) {
	inst_code ev;
	int val;

	val = (int)doubletoIEEE754(inv);

	if ((ev = huey_wrreg_word(p, val, addr)) != inst_ok)
		return ev;
	return inst_ok;
}

/* Read the integration time */
static inst_code
huey_rd_int_time(
	huey *p,				/* Object */
	int *outp				/* Where to write value */
) {
	unsigned char buf[8];
	int rsize;
	inst_code ev;

	if ((ev = huey_command(p, i1d_getintgt, buf, buf, 1.0, 1.0)) != inst_ok)
		return ev;
	
	*outp = buf2int(buf);

	return inst_ok;
}

/* Set the integration time */
/* (Not used for Huey ?) */
static inst_code
huey_wr_int_time(
	huey *p,				/* Object */
	int inv					/* Value to write */
) {
	unsigned char buf[16];
	int rsize;
	inst_code ev;

	int2buf(buf, inv);
	if ((ev = huey_command(p, i1d_setintgt, buf, buf, 1.0, 1.0)) != inst_ok)
		return ev;

	return inst_ok;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Take a raw measurement using a given integration time. */
/* The measureent is the count of (both) edges from the L2V */
/* over the integration time */
static inst_code
huey_freq_measure(
	huey *p,				/* Object */
	double rgb[3]			/* Return the RGB edge count values */
) {
	int i;
	unsigned char ibuf[8];
	unsigned char obuf[8];
	inst_code ev;

	/* Do the measurement, and return the Red value */
	ibuf[0] = 2;		/* Sync mode 2 for CRT */
	if ((ev = huey_command(p, i1d_m_red_2, ibuf, obuf, 1.0, 10.0)) != inst_ok)
		return ev;
	rgb[0] = (double)buf2int(obuf);

	/* Get the green value */
	if ((ev = huey_command(p, i1d_rd_green, ibuf, obuf, 1.0, 1.0)) != inst_ok)
		return ev;
	rgb[1] = (double)buf2int(obuf);

	/* Get the blue value */
	if ((ev = huey_command(p, i1d_rd_blue, ibuf, obuf, 1.0, 1.0)) != inst_ok)
		return ev;
	rgb[2] = (double)buf2int(obuf);

	return inst_ok;
}

/* Take a raw measurement that returns the number of clocks */
/* between and initial edge and edgec[] subsequent edges of the L2F. */
/* Both edges are counted. */
static inst_code
huey_period_measure(
	huey *p,			/* Object */
	int edgec[3],		/* Measurement edge count for each channel */
	double rgb[3]		/* Return the RGB values */
) {
	int i;
	unsigned char ibuf[16];
	unsigned char obuf[16];
	int rsize;
	inst_code ev;

	/* Set the edge count */
	short2buf(ibuf + 0, edgec[0]);
	short2buf(ibuf + 2, edgec[1]);
	short2buf(ibuf + 4, edgec[2]);
	
	/* Do the measurement, and return the Red value */
	if ((ev = huey_command(p, i1d_m_rgb_edge_2, ibuf, obuf, 1.0, 70.0)) != inst_ok)
		return ev;
	rgb[0] = (double)buf2int(obuf);

	/* Get the green value */
	if ((ev = huey_command(p, i1d_rd_green, ibuf, obuf, 1.0, 1.0)) != inst_ok)
		return ev;
	rgb[1] = (double)buf2int(obuf);

	/* Get the blue value */
	if ((ev = huey_command(p, i1d_rd_blue, ibuf, obuf, 1.0, 1.0)) != inst_ok)
		return ev;
	rgb[2] = (double)buf2int(obuf);

	return inst_ok;
}

/* Take a another mode raw mesurement from the device for */
/* (not currently used ?) */
static inst_code
huey_take_raw_measurement_3(
	huey *p,			/* Object */
	int edgec[3],		/* Measurement edge count for each channel */
	double rgb[3]		/* Return the RGB values */
) {
	int i;
	unsigned char ibuf[16];
	unsigned char obuf[16];
	int rsize;
	inst_code ev;

	/* Do the measurement, and return the Red value */
	short2buf(ibuf + 0, edgec[0]);
	ibuf[2] = 0;	/* Channel */
	if ((ev = huey_command(p, i1d_m_rgb_edge_2, ibuf, obuf, 1.0, 10.0)) != inst_ok)
		return ev;
	rgb[0] = (double)buf2int(obuf);

	/* Do the measurement, and return the Green value */
	short2buf(ibuf + 0, edgec[1]);
	ibuf[2] = 1;	/* Channel */
	if ((ev = huey_command(p, i1d_m_rgb_edge_2, ibuf, obuf, 1.0, 10.0)) != inst_ok)
		return ev;
	rgb[1] = (double)buf2int(obuf);

	/* Do the measurement, and return the Blue value */
	short2buf(ibuf + 0, edgec[2]);
	ibuf[2] = 2;	/* Channel */
	if ((ev = huey_command(p, i1d_m_rgb_edge_2, ibuf, obuf, 1.0, 10.0)) != inst_ok)
		return ev;
	rgb[2] = (double)buf2int(obuf);

	return inst_ok;
}

/* Take a cooked measurement from the device for Huey. */
/* The sensors are likely to be light to frequency converters */
/* such as the TAOS TSL237, and there are only so many ways */
/* the output can be measured. If the light level is high enough, */
/* you could simply count the number of output transitions (edges) */
/* in a fixed period of time. If the frequency is low though, */
/* this limits precision due to quantization of the count. */
/* Another way is to time how long it takes to count a certain */
/* number of edges. This has higher precision at low frequencies, */
/* but has the problem of having an unknown measurement duration, */
/* and seems to be the main method chosen for the Huey. */
static inst_code
huey_take_measurement_2(
	huey *p,				/* Object */
	int refr,				/* nz if crt mode */
	double rgb[3]			/* Return the rgb values */
) {
	int i, j;
	int edgec[3] = {1,1,1};	/* Measurement edge count for each channel */
	int rem[3] = {1,1,1};	/* remeasure flags */
	inst_code ev;

	if (p->inited == 0)
		return huey_interp_code((inst *)p, HUEY_NOT_INITED);

	a1logd(p->log,4,"take_measurement_2 called with refr = %d\n",refr);

	/* For Refresh mode, do an initial set of measurements over a fixed period */
	if (refr) {

		if ((ev = huey_freq_measure(p, rgb)) != inst_ok)
			return ev;

		a1logd(p->log,4,"Raw initial CRT RGB = %f %f %f\n",rgb[0],rgb[1],rgb[2]);

		/* Decide whether any channels need re-measuring, */
		/* and computed cooked values. Threshold is typically 75 */
		for (i = 0; i < 3; i++) {
			rem[i] = (rgb[i] <= (0.75 * (double)p->sampno)) ? 1 : 0;
			rgb[i] = 0.5 * rgb[i] * 1e6/(double)p->int_clocks;
		}
		a1logd(p->log,4,"Re-measure flags = %d %d %d\n",rem[0],rem[1],rem[2]);
	}

	/* If any need re-measuring */
	if (rem[0] || rem[1] || rem[2]) {
		double rgb2[3];

		/* Do a first or second set of measurements */
		if ((ev = huey_period_measure(p, edgec, rgb2)) != inst_ok)
			return ev;
		a1logd(p->log,4,"Raw initial/subsequent ecount %d %d %d RGB = %f %f %f\n",
		     edgec[0], edgec[1], edgec[2], rgb2[0], rgb2[1], rgb2[2]);

		/* Compute adjusted edge count for channels we're remeasuring, */
		/* aiming for count values of clk_freq (~1e6). */
		for (i = 0; i < 3; i++) {
			double ns;
			if (rem[i]) {
				if (p->clk_freq > ((2000.0 - 0.5) * rgb2[i]))
					ns = 2000.0;
				else {
					ns = floor(p->clk_freq/rgb2[i]) + 0.5;
					if (ns < 1.0)
						ns = 1.0;
				}
				edgec[i] = (int)ns;
			}
		}
	
		/* If we compute a different edge count, read again */
		if (edgec[0] > 1 || edgec[1] > 1 || edgec[2] > 1) {
			double rgb3[3];		/* 2nd RGB Readings */
	
			if ((ev = huey_period_measure(p, edgec, rgb3)) != inst_ok)
				return ev;
	
			a1logd(p->log,4,"Raw subsequent2 ecount %d %d %d RGB = %f %f %f\n",
			     edgec[0], edgec[1], edgec[2], rgb3[0], rgb3[1], rgb3[2]);

			/* Average readings if we repeated a measurement with the same threshold */
			/* (Minor advantage, but may as well use it) */
			for (i = 0; i < 3; i++) {
				if (edgec[i] == 1)
					rgb2[i] = 0.5 * (rgb2[i] + rgb3[i]);
				else
					rgb2[i] = rgb3[i];
			}
		}

		/* Compute adjusted readings, ovewritting initial cooked values */
		for (i = 0; i < 3; i++) {
			if (rem[i]) {
				rgb[i] = ((double)edgec[i])/(rgb2[i] * 2.0 * p->clk_prd);
				a1logd(p->log,4,"%d after scale = %f\n",i,rgb[i]);
		
				rgb[i] -= p->dark_cal[i];		/* Subtract black level */
				a1logd(p->log,4,"%d after sub black = %f\n",i,rgb[i]);
		
				if (rgb[i] < 0.0001)
					rgb[i] = 0.0001;
				a1logd(p->log,4,"%d after limit min = %f\n",i,rgb[i]);
			}
		}
	}

	a1logd(p->log,4,"Cooked RGB = %f %f %f\n",rgb[0],rgb[1],rgb[2]);
	
	return inst_ok;
}

/* Take a raw ambient measurement */
static inst_code
huey_take_amb_measurement_1(
	huey *p,			/* Object */
	int a1,				/* 8 bit argument - unknown */
	int syncmode,		/* 8 bit argument - sync mode */
	double *amb,		/* Return the raw ambient value */
	int *rb				/* Returned byte */
) {
	int i;
	unsigned char ibuf[16];
	unsigned char obuf[16];
	int rsize;
	inst_code ev;

	a1 &= 0xff;
	ibuf[0] = a1;
	ibuf[1] = syncmode;
	
	/* Do the measurement */
	if ((ev = huey_command(p, i1d_rdambient, ibuf, obuf, 1.0, 10.0)) != inst_ok)
		return ev;

	/* Expect first argument to be returned */
	if (obuf[0] != a1)
		return huey_interp_code((inst *)p, HUEY_UNEXPECTED_RET_VAL);

	*amb = (double)buf2int(obuf+1);
	*rb = 0xff & obuf[5];

	return inst_ok;
}

/* Take a cooked ambient measurement */
static inst_code
huey_take_amb_measurement(
	huey *p,			/* Object */
	int refr,			/* nz if refresh mode */
	double *amb			/* Return the ambient value */
) {
	int rb;				/* Returned byte - not used */
	inst_code ev;

	if (p->inited == 0)
		return huey_interp_code((inst *)p, HUEY_NOT_INITED);

	a1logd(p->log,4,"take_amb_measurement_2 called with refr = %d\n",refr);

	/* First param is always 3, second is sync mode */
	if ((ev = huey_take_amb_measurement_1(p, 3, refr ? 2 : 0, amb, &rb)) != inst_ok)
 		return ev;
	a1logd(p->log,4,"Raw ambient = %f\n",*amb);
	return inst_ok;
}

/* Set the indicator LED's state. */
/* The bottom 4 bits set the LED state from bottom (0) */
/* to top (3), 0 = off, 1 = on */
static inst_code
huey_set_LEDs(
	huey *p,			/* Object */
	int mask			/* 8 bit LED mask */
) {
	int i;
	unsigned char ibuf[8];
	unsigned char obuf[8];
	inst_code ev;

	mask &= 0xf;
	p->led_state = mask;

	ibuf[0] = 0;
	ibuf[1] = 0xf & (~mask);
	
	/* Do command */
	if ((ev = huey_command(p, i1d_set_leds, ibuf, obuf, 1.0, 1.0)) != inst_ok)
		return ev;

	return inst_ok;
}

/* . . . . . . . . . . . . . . . . . . . . . . . . */

/* Take a XYZ measurement from the device */
static inst_code
huey_take_XYZ_measurement(
	huey *p,				/* Object */
	double XYZ[3]			/* Return the XYZ values */
) {
	int i, j;
	double rgb[3];		/* RGB Readings */
	inst_code ev;
	double *mat;		/* Pointer to matrix */

	if (IMODETST(p->mode, inst_mode_emis_ambient)) {
		if ((ev = huey_take_amb_measurement(p, 0, &XYZ[1])) != inst_ok)
			return ev;
		XYZ[1] *= AMB_SCALE_FACTOR;		/* Times aproximate fudge factor */
		XYZ[0] = icmD50.X * XYZ[1];		/* Convert to D50 neutral */
		XYZ[2] = icmD50.Z * XYZ[1];
	} else {
		if ((ev = huey_take_measurement_2(p, p->refrmode, rgb)) != inst_ok)
			return ev;

		if (p->icx)
			mat = p->CRT_cal;		/* CRT/factory matrix */
		else
			mat = p->LCD_cal;		/* LCD/user matrix */

		for (i = 0; i < 3; i++) {
			XYZ[i] = 0.0;
			for (j = 0; j < 3; j++) {
				XYZ[i] += mat[i * 3 + j] * rgb[j]; 
			}
			XYZ[i] *= CALFACTOR;	/* Times magic scale factor */
		}

		/* Apply the colorimeter correction matrix */
		icmMulBy3x3(XYZ, p->ccmat, XYZ);
	}
	a1logd(p->log,3,"huey_take_XYZ_measurement: XYZ = %f %f %f\n",XYZ[0],XYZ[1],XYZ[2]);
	return inst_ok;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Check the device is responding, and unlock if necessary */
static inst_code
huey_check_unlock(
	huey *p				/* Object */
) {
	unsigned char buf[8];
	int rsize;
	inst_code ev;
	int vv;
	double ver;

	a1logd(p->log,2,"huey_check_unlock: called\n");

	/* Check the instrument status */
	if ((ev = huey_command(p, i1d_status, buf, buf, 1.0,1.0)) != inst_ok)
		return ev;

	if (strncmp((char *)buf, "Locked", 6) == 0) {
		if (p->lenovo)
			strcpy((char *)buf,"huyL");
		else
			strcpy((char *)buf,"GrMb");

		if ((ev = huey_command(p, i1d_unlock, buf, buf, 1.0,1.0)) != inst_ok)
			return ev;

		if ((ev = huey_command(p, i1d_status, buf, buf, 1.0,1.0)) != inst_ok)
			return ev;
	}

	if (strncmp((char *)buf, "huL002", 6) != 0		/* Lenovo Huey ? */
	 && strncmp((char *)buf, "Cir001", 6) != 0) {	/* Huey */
		a1logd(p->log,1,"huey_check_unlock: unknown model '%s'\n",buf);
		return huey_interp_code((inst *)p, HUEY_UNKNOWN_MODEL);
	}

	a1logd(p->log,2,"huey_check_unlock: instrument is responding, unlocked, and right type\n");

	return inst_ok;
}

/* Read all the relevant register values */
static inst_code
huey_read_all_regs(
	huey *p				/* Object */
) {
	inst_code ev;
	int i;

	a1logd(p->log,2,"huey_check_unlock: about to read all the registers\n");

	/* Serial number */
	if ((ev = huey_rdreg_word(p, &p->ser_no, 0) ) != inst_ok)
		return ev;
	a1logd(p->log,4,"serial number = %d\n",p->ser_no);


	/* LCD/user calibration values */
	for (i = 0; i < 9; i++) {
		if ((ev = huey_rdreg_float(p, &p->LCD_cal[i], 4 + 4 * i) ) != inst_ok)
			return ev;
		a1logd(p->log,4,"LCD/user cal[%d] = %f\n",i,p->LCD_cal[i]);
	}
	/* LCD/user calibration time */
	if ((ev = huey_rdreg_word(p, &p->LCD_caltime, 50) ) != inst_ok)
		return ev;
	a1logd(p->log,2,"LCD/user calibration time = 0x%x = %s\n",p->LCD_caltime, ctime_32(&p->LCD_caltime));

	/* CRT/factory calibration values */
	for (i = 0; i < 9; i++) {
		if ((ev = huey_rdreg_float(p, &p->CRT_cal[i], 54 + 4 * i) ) != inst_ok)
			return ev;
		a1logd(p->log,3,"CRT/factory cal[%d] = %f\n",i,p->CRT_cal[i]);
	}
	/* CRT/factory calibration flag */
	if ((ev = huey_rdreg_word(p, &p->CRT_caltime, 90) ) != inst_ok)
		return ev;
	a1logd(p->log,3,"CRT/factory flag = 0x%x = %s\n",p->CRT_caltime, ctime_32(&p->CRT_caltime));


	/* Hard coded in Huey */
	p->clk_prd = 1e-6;
	a1logd(p->log,3,"Clock period = %f\n",p->clk_prd);

	/* Dark current calibration values */
	for (i = 0; i < 3; i++) {
		if ((ev = huey_rdreg_float(p, &p->dark_cal[i], 103 + 4 * i)) != inst_ok) {
			if ((ev & inst_imask) != HUEY_FLOAT_NOT_SET)
				return ev;
			p->dark_cal[i] = 0.0;
		}
		a1logd(p->log,3,"darkcal[%d] = %f\n",i,p->dark_cal[i]);
	}

	/* Ambient darkcurrent calibration value ? */
	if ((ev = huey_rdreg_float(p, &p->amb_cal, 148)) != inst_ok) {
		if ((ev & inst_imask) != HUEY_FLOAT_NOT_SET)
			return ev;
		p->amb_cal = 0.0;
	}
	a1logd(p->log,3,"Ambient cal = %f\n",p->amb_cal);

	/* Unlock string */
	for (i = 0; i < 4; i++) {
		int vv;
		if ((ev = huey_rdreg_byte(p, &vv, 122 + i) ) != inst_ok)
			return ev;
		p->unlk_string[i] = (char)vv;
	}
	p->unlk_string[i] = '\000';
	a1logd(p->log,3,"unlock string = '%s'\n",p->unlk_string);

	/* Read the integration time */
	if ((ev = huey_rd_int_time(p, &p->int_clocks) ) != inst_ok)
		return ev;
	a1logd(p->log,3,"Integration time = %d\n",p->int_clocks);

	a1logd(p->log,2,"huey_check_unlock: all registers read OK\n");

	return inst_ok;
}

/* Compute factors that depend on the register values */
static inst_code
huey_compute_factors(
	huey *p				/* Object */
) {
	int i;

	/* Check that certain value are valid */
	if (p->ser_no == 0xffffffff) /* (It appears that some instruments have no serial number!) */
		a1logw(p->log,"huey: bad instrument serial number\n");

	if (p->LCD_caltime == 0xffffffff)
		return huey_interp_code((inst *)p, HUEY_BAD_LCD_CALIBRATION);

	if (p->CRT_caltime == 0xffffffff)
		return huey_interp_code((inst *)p, HUEY_BAD_CRT_CALIBRATION);

	/* clk_prd inversion */
	p->clk_freq = 1.0/p->clk_prd;
	a1logd(p->log,3,"clk_freq = %f\n",p->clk_freq);
	
	/* Set some defaults */
	p->sampno = 100;		/* Minimum sampling count */

	return inst_ok;
}

/* ------------------------------------------------------------------------ */

static inst_code set_default_disp_type(huey *p);

/* Establish communications with a HUEY */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
huey_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	huey *p = (huey *) pp;
	unsigned char buf[8];
	int rsize;
	long etime;
	int bi, i, se, rv;
	inst_code ev = inst_ok;
	char **pnames = NULL;
	int retries = 0;

	a1logd(p->log, 2, "huey_init_coms: About to init coms\n");

	/* Open as an HID if available */
	if (p->icom->port_type(p->icom) == icomt_hid) {

		a1logd(p->log, 3, "huey_init_coms: About to init HID\n");

		/* Set config, interface */
		if ((se = p->icom->set_hid_port(p->icom, icomuf_none, retries, pnames)) != ICOM_OK) { 
			a1logd(p->log, 1, "huey_init_coms: set_hid_port failed ICOM err 0x%x\n",se);
			return huey_interp_code((inst *)p, icoms2huey_err(se, 0));
		}

	} else if (p->icom->port_type(p->icom) == icomt_usb) {

		a1logd(p->log, 3, "huey_init_coms: About to init USB\n");

		/* Set config, interface, write end point, read end point */
		/* ("serial" end points aren't used - the huey uses USB control messages) */
		/* We need to detatch the HID driver on Linux */

		if ((se = p->icom->set_usb_port(p->icom, 1, 0x00, 0x00, icomuf_detach, 0, NULL))
		                                                                     != ICOM_OK) { 
			a1logd(p->log, 1, "huey_init_coms: set_usb_port failed ICOM err 0x%x\n",se);
			return huey_interp_code((inst *)p, icoms2huey_err(se, 0));
		}

	} else {
		a1logd(p->log, 1, "huey_init_coms: wrong communications type for device!\n");
		return huey_interp_code((inst *)p, HUEY_UNKNOWN_MODEL);
	}

	if ((p->icom->vid == 0x0765 && p->icom->pid == 0x5001)
	||  (p->icom->vid == 0x0765 && p->icom->pid == 0x5010)) { 
		a1logd(p->log, 2, "huey_init_coms: Lenovo version\n");
		p->lenovo = 1;
	}

	/* Check instrument is responding */
	if ((ev = huey_command(p, i1d_status, buf, buf, 1.0, 1.0)) != inst_ok) {
		a1logd(p->log, 1, "huey_init_coms: instrument didn't respond 0x%x\n",ev);
		return ev;
	}

	/* Setup the default display type */
	if ((ev = set_default_disp_type(p)) != inst_ok) {
		return ev;
	}
	
	a1logd(p->log, 2, "huey_init_coms: inited coms OK\n");

	p->gotcoms = 1;
	return inst_ok;
}

/* Initialise the HUEY */
/* return non-zero on an error, with dtp error code */
static inst_code
huey_init_inst(inst *pp) {
	huey *p = (huey *)pp;
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "huey_init_inst: called\n");

	if (p->gotcoms == 0)
		return huey_interp_code((inst *)p, HUEY_NO_COMS);	/* Must establish coms first */

	/* Check instrument is responding, and right type */
	if ((ev = huey_check_unlock(p)) != inst_ok)
		return ev;

	/* Turn the LEDs off */
	if ((ev = huey_set_LEDs(p, 0x0)) != inst_ok)
		return ev;

	/* Read all the registers and store their contents */
	if ((ev = huey_read_all_regs(p)) != inst_ok)
		return ev;

	if ((ev = huey_compute_factors(p)) != inst_ok)
		return ev;

	p->trig = inst_opt_trig_user;

	p->inited = 1;
	a1logd(p->log, 2, "huey_init_inst: inited OK\n");

	/* Flash the LEDs, just cos we can! */
	if ((ev = huey_set_LEDs(p, 0x1)) != inst_ok)
		return ev;
	msec_sleep(50);
	if ((ev = huey_set_LEDs(p, 0x2)) != inst_ok)
		return ev;
	msec_sleep(50);
	if ((ev = huey_set_LEDs(p, 0x4)) != inst_ok)
		return ev;
	msec_sleep(50);
	if ((ev = huey_set_LEDs(p, 0x8)) != inst_ok)
		return ev;
	msec_sleep(50);
	if ((ev = huey_set_LEDs(p, 0x4)) != inst_ok)
		return ev;
	msec_sleep(50);
	if ((ev = huey_set_LEDs(p, 0x2)) != inst_ok)
		return ev;
	msec_sleep(50);
	if ((ev = huey_set_LEDs(p, 0x1)) != inst_ok)
		return ev;
	msec_sleep(50);
	if ((ev = huey_set_LEDs(p, 0x0)) != inst_ok)
		return ev;

	return inst_ok;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
huey_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	huey *p = (huey *)pp;
	int user_trig = 0;
	int rv = inst_protocol_error;

a1logd(p->log, 1, "huey: huey_read_sample called\n");

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->trig == inst_opt_trig_user) {

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "huey: inst_opt_trig_user but no uicallback function set!\n");
			return inst_unsupported;
		}

a1logd(p->log, 1, "huey: about to wait for user trigger\n");
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
a1logd(p->log, 1, "huey: checking for abort\n");
		/* Check for abort */
		if (p->uicallback != NULL
		 && (rv = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort)
			return rv;				/* Abort */
	}

a1logd(p->log, 1, "huey: about to call huey_take_XYZ_measurement\n");
	/* Read the XYZ value */
	if ((rv = huey_take_XYZ_measurement(p, val->XYZ)) != inst_ok) {
		return rv;
	}
	/* This may not change anything since instrument may clamp */
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);

	val->loc[0] = '\000';
	if (IMODETST(p->mode, inst_mode_emis_ambient))
		val->mtype = inst_mrt_ambient;
	else
		val->mtype = inst_mrt_none;
	val->XYZ_v = 1;		/* These are absolute XYZ readings ? */
	val->sp.spec_n = 0;
	val->duration = 0.0;

	if (user_trig)
		return inst_user_trig;
	return rv;
}

/* Insert a colorimetric correction matrix in the instrument XYZ readings */
/* This is only valid for colorimetric instruments. */
/* To remove the matrix, pass NULL for the filter filename */
inst_code huey_col_cor_mat(
inst *pp,
double mtx[3][3]
) {
	huey *p = (huey *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (mtx == NULL) {
		icmSetUnity3x3(p->ccmat);
	} else {
		if (p->cbid == 0) {
			a1loge(p->log, 1, "huey: can't set col_cor_mat over non base display type\n");
			return inst_wrong_setup;
		}
		icmCpy3x3(p->ccmat, mtx);
	}
		
	return inst_ok;
}

/* Error codes interpretation */
static char *
huey_interp_error(inst *pp, int ec) {
//	huey *p = (huey *)pp;
	ec &= inst_imask;
	switch (ec) {
		case HUEY_INTERNAL_ERROR:
			return "Internal software error";
		case HUEY_COMS_FAIL:
			return "Communications failure";
		case HUEY_UNKNOWN_MODEL:
			return "Not a known Huey Model";
		case HUEY_DATA_PARSE_ERROR:
			return "Data from i1 Display didn't parse as expected";

		case HUEY_OK:
			return "No device error";

		case HUEY_FLOAT_NOT_SET:
			return "Float value is not set in EEPROM";
		case HUEY_NOT_READY:
			return "Command didn't return command code - not ready ?";

		case HUEY_BAD_SERIAL_NUMBER:
			return "Serial number isn't set";
		case HUEY_BAD_LCD_CALIBRATION:
			return "LCD calibration values aren't set";
		case HUEY_BAD_CRT_CALIBRATION:
			return "CRT calibration values aren't set";
		case HUEY_EEPROM_WRITE_FAIL:
			return "Write to EEPROM failed to verify";

		case HUEY_BAD_WR_LENGTH:
			return "Unable to write full message to instrument";
		case HUEY_BAD_RD_LENGTH:
			return "Unable to read full message to instrument";
		case HUEY_BAD_RET_CMD:
			return "Message from instrument didn't echo command code";
		case HUEY_BAD_RET_STAT:
			return "Message from instrument had bad status code";
		case HUEY_UNEXPECTED_RET_VAL:
			return "Message from instrument has unexpected value";

		case HUEY_BAD_STATUS:
			return "Instrument status is unrecognised format";
		case HUEY_UNKNOWN_VERS_ID:
			return "Instrument version number or ID byte not recognised";
		case HUEY_BAD_COMMAND:
			return "Instrument didn't recognise the command";

		/* Internal errors */
		case HUEY_BAD_REG_ADDRESS:
			return "Out of range register address";
		case HUEY_BAD_INT_THRESH:
			return "Out of range integration threshold";
		case HUEY_NO_COMS:
			return "Communications hasn't been established";
		case HUEY_NOT_INITED:
			return "Insrument hasn't been initialised";
		case HUEY_CANT_BLACK_CALIB:
			return "Device doesn't support black calibration";
		case HUEY_CANT_MEASP_CALIB:
			return "Device doesn't support measurment period calibration";
		case HUEY_WRONG_DEVICE:
			return "Wrong type of device for called function";
		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
huey_interp_code(inst *pp, int ec) {
//	huey *p = (huey *)pp;

	ec &= inst_imask;
	switch (ec) {

		case HUEY_OK:
		case HUEY_FLOAT_NOT_SET:		/* Internal indication */
		case HUEY_NOT_READY:			/* Internal indication */
			return inst_ok;

		case HUEY_INTERNAL_ERROR:
		case HUEY_BAD_REG_ADDRESS:
		case HUEY_BAD_INT_THRESH:
		case HUEY_NO_COMS:
		case HUEY_NOT_INITED:
		case HUEY_CANT_BLACK_CALIB:
		case HUEY_CANT_MEASP_CALIB:
		case HUEY_WRONG_DEVICE:
			return inst_internal_error | ec;

		case HUEY_COMS_FAIL:
			return inst_coms_fail | ec;

		case HUEY_UNKNOWN_MODEL:
		case HUEY_BAD_STATUS:
		case HUEY_UNKNOWN_VERS_ID:
			return inst_unknown_model | ec;

		case HUEY_DATA_PARSE_ERROR:
		case HUEY_BAD_WR_LENGTH:
		case HUEY_BAD_RD_LENGTH:
		case HUEY_BAD_RET_CMD:
		case HUEY_BAD_RET_STAT:
		case HUEY_UNEXPECTED_RET_VAL:
		case HUEY_BAD_COMMAND:
			return inst_protocol_error | ec;

		case HUEY_BAD_SERIAL_NUMBER:
		case HUEY_BAD_LCD_CALIBRATION:
		case HUEY_BAD_CRT_CALIBRATION:
		case HUEY_EEPROM_WRITE_FAIL:
			return inst_hardware_fail | ec;

		/* return inst_misread | ec; */
		/* return inst_needs_cal_2 | ec; */
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
huey_del(inst *pp) {
	huey *p = (huey *)pp;
	if (p->icom != NULL)
		p->icom->del(p->icom);
	inst_del_disptype_list(p->dtlist, p->ndtlist);
	free(p);
}

/* Return the instrument mode capabilities */
void huey_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	huey *p = (huey *)pp;
	inst_mode cap1 = 0;
	inst2_capability cap2 = 0;

	cap1 |= inst_mode_emis_spot
	     |  inst_mode_emis_ambient
	     |  inst_mode_colorimeter
	        ;

	cap2 |= inst2_prog_trig
	     |  inst2_user_trig
	     |  inst2_has_leds
	     |  inst2_disptype
	     |  inst2_ambient_mono
	     |  inst2_ccmx
		    ;

	if (pcap1 != NULL)
		*pcap1 = cap1;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = inst3_none;
}

/* Check device measurement mode */
inst_code huey_check_mode(inst *pp, inst_mode m) {
	huey *p = (huey *)pp;
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
inst_code huey_set_mode(inst *pp, inst_mode m) {
	huey *p = (huey *)pp;
	inst_code ev;

	if ((ev = huey_check_mode(pp, m)) != inst_ok)
		return ev;

	p->mode = m;

	return inst_ok;
}

inst_disptypesel huey_disptypesel[3] = {
	{
		inst_dtflags_default,
		1,
		"l",
		"LCD display",
		0,
		0
	},
	{
		inst_dtflags_none,		/* flags */
		2,						/* cbix */
		"c",					/* sel */
		"CRT display",			/* desc */
		1,						/* refr */
		1						/* ix */
	},
	{
		inst_dtflags_end,
		0,
		"",
		"",
		0,
		0
	}
};

/* Get mode and option details */
static inst_code huey_get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	huey *p = (huey *)pp;
	inst_code rv = inst_ok;

	/* Create/Re-create a current list of abailable display types */
	if (p->dtlist == NULL || recreate) {
		if ((rv = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    huey_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return rv;
	}

	if (pnsels != NULL)
		*pnsels = p->ndtlist;

	if (psels != NULL)
		*psels = p->dtlist;

	return inst_ok;
}

/* Given a display type entry, setup for that type */
static inst_code set_disp_type(huey *p, inst_disptypesel *dentry) {

	p->icx = dentry->ix; 
	p->refrmode = dentry->refr; 
	p->cbid = dentry->cbid; 

	if (dentry->flags & inst_dtflags_ccmx) {
		icmCpy3x3(p->ccmat, dentry->mat);
	} else {
		icmSetUnity3x3(p->ccmat);
	}

	return inst_ok;
}

/* Setup the default display type */
static inst_code set_default_disp_type(huey *p) {
	inst_code ev;
	int i;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
		    huey_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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

/* Set the display type */
static inst_code huey_set_disptype(inst *pp, int ix) {
	huey *p = (huey *)pp;
	inst_code ev;
	inst_disptypesel *dentry;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    huey_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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

/* 
 * set or reset an optional mode
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 */
static inst_code
huey_get_set_opt(inst *pp, inst_opt_type m, ...)
{
	huey *p = (huey *)pp;
	inst_code ev = inst_ok;

	/* Record the trigger mode */
	if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_user) {
		p->trig = m;
		return inst_ok;
	}

	/* Get the display type information */
	if (m == inst_opt_get_dtinfo) {
		va_list args;
		int *refrmode, *cbid;

		va_start(args, m);
		refrmode = va_arg(args, int *);
		cbid = va_arg(args, int *);
		va_end(args);

		if (refrmode != NULL)
			*refrmode = p->refrmode;
		if (cbid != NULL)
			*cbid = p->cbid;

		return inst_ok;
	}

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* Operate the LEDS */
	if (m == inst_opt_get_gen_ledmask) {
		va_list args;
		int *mask = NULL;

		va_start(args, m);
		mask = va_arg(args, int *);
		va_end(args);
		*mask = 0xf;			/* Four general LEDs */
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
		return huey_set_LEDs(p, mask);
	}

	return inst_unsupported;
}

/* Constructor */
extern huey *new_huey(icoms *icom, instType itype) {
	huey *p;
	if ((p = (huey *)calloc(sizeof(huey),1)) == NULL) {
		a1loge(icom->log, 1, "new_huey: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	p->init_coms         = huey_init_coms;
	p->init_inst         = huey_init_inst;
	p->capabilities      = huey_capabilities;
	p->check_mode        = huey_check_mode;
	p->set_mode          = huey_set_mode;
	p->get_disptypesel   = huey_get_disptypesel;
	p->set_disptype      = huey_set_disptype;
	p->get_set_opt       = huey_get_set_opt;
	p->read_sample       = huey_read_sample;
	p->col_cor_mat       = huey_col_cor_mat;
	p->interp_error      = huey_interp_error;
	p->del               = huey_del;

	p->icom = icom;
	p->itype = icom->itype;

	icmSetUnity3x3(p->ccmat);	/* Set the colorimeter correction matrix to do nothing */

	return p;
}

