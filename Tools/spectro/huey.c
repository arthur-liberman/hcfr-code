
/* 
 * Argyll Color Correction System
 *
 * GretagMacbeth Huey related functions
 *
 * Author: Graeme W. Gill
 * Date:   18/10/2006
 *
 * Copyright 2006 - 2007, Graeme W. Gill
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
#include "icoms.h"
#include "conv.h"
#include "huey.h"

#undef DEBUG

#ifdef DEBUG
#define DBG(xxx) fprintf xxx ;
#else
#define DBG(xxx) if (p->debug >= 5) fprintf xxx ;
#endif

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
static int icoms2huey_err(int se) {
	if (se & ICOM_USERM) {
		se &= ICOM_USERM;
		if (se == ICOM_USER)
			return HUEY_USER_ABORT;
		if (se == ICOM_TERM)
			return HUEY_USER_TERM;
		if (se == ICOM_TRIG)
			return HUEY_USER_TRIG;
		if (se == ICOM_CMND)
			return HUEY_USER_CMND;
	}
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
    i1d_m_rgb_edge_2 = 0x16,		/* SSS:2:WB    Measure RGB Edge period mode, 1,70sec */
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
	unsigned char buf[8];	/* 8 bytes to write/read */
	int wbytes;				/* bytes written */
	int rbytes;				/* bytes read from ep */
	int se, ua = 0, rv = inst_ok;
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	if (isdeb <= 2)
		p->icom->debug = 0;
	
	if (isdeb) fprintf(stderr,"huey: Sending cmd '%s' args '%s'",inst_desc(cc), icoms_tohex(in, 7));

	/* Send the command using the control interface */
	buf[0] = cc;				/* Construct the command */
	memmove(buf + 1, in, 7);

	if (p->icom->is_hid) {
		se = p->icom->hid_write(p->icom, buf, 8, &wbytes, to); 
	} else {
		se = p->icom->usb_control(p->icom,
		      USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0x9, 0x200, 0, buf, 8, to);
		wbytes = 8;
	}
	if (se != 0) {
		if (se & ICOM_USERM) {
			ua = (se & ICOM_USERM);
		}
		if (se & ~ICOM_USERM) {
			if (isdeb) fprintf(stderr,"\nhuey: Command send failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return huey_interp_code((inst *)p, HUEY_COMS_FAIL);
		}
	}
	rv = huey_interp_code((inst *)p, icoms2huey_err(ua));
	if (isdeb) fprintf(stderr," ICOM err 0x%x\n",ua);
	if (wbytes != 8)
		rv = huey_interp_code((inst *)p, HUEY_BAD_WR_LENGTH);
	if (rv != inst_ok) {
		p->icom->debug = isdeb;
		return rv;
	}

	/* Now fetch the response */
	if (isdeb) fprintf(stderr,"huey: Reading response ");

	if (p->icom->is_hid) {
		se = p->icom->hid_read(p->icom, buf, 8, &rbytes, to);
	} else {
		se = p->icom->usb_read(p->icom, 0x81, buf, 8, &rbytes, to);
	} 
	if (se != 0) {
		if (se & ICOM_USERM) {
			ua = (se & ICOM_USERM);
		}
		if (se & ~ICOM_USERM) {
			if (isdeb) fprintf(stderr,"\nhuey: Response read failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return huey_interp_code((inst *)p, HUEY_COMS_FAIL);
		}
	}
	rv = huey_interp_code((inst *)p, icoms2huey_err(ua));
	if (rbytes != 8)
		rv = huey_interp_code((inst *)p, HUEY_BAD_RD_LENGTH);
	if (rv == inst_ok && buf[1] != cc) {
		rv = huey_interp_code((inst *)p, HUEY_BAD_RET_CMD);
	}
		
	/* Some commands don't use the first response, but need to */
	/* fetch a second response, with a longer timeout. */
	/* This seems to be all of the measurement trigger commands */
	if (rv == inst_ok && buf[0] == 0x90) {	/* there is more */

		if (p->icom->is_hid) {
			se = p->icom->hid_read(p->icom, buf, 8, &rbytes, to2);
		} else {
			se = p->icom->usb_read(p->icom, 0x81, buf, 8, &rbytes, to2);
		} 
		if (se != 0) {
			if (se & ICOM_USERM) {
				ua = (se & ICOM_USERM);
			}
			if (se & ~ICOM_USERM) {
				if (isdeb) fprintf(stderr,"\nhuey: Response read failed with ICOM err 0x%x\n",se);
				p->icom->debug = isdeb;
				return huey_interp_code((inst *)p, HUEY_COMS_FAIL);
			}
		}
		rv = huey_interp_code((inst *)p, icoms2huey_err(ua));
		if (rbytes != 8)
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
	if (isdeb) fprintf(stderr," '%s' ICOM err 0x%x\n",icoms_tohex(out, 6),ua);
	p->icom->debug = isdeb;

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
	unsigned char ibuf[8], obuf[8];
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
	inst_code ev;

	int2buf(buf, inv);
	if ((ev = huey_command(p, i1d_setintgt, buf, buf, 1.0, 1.0)) != inst_ok)
		return ev;

	return inst_ok;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Take a raw initial (CRT) measurement from the device for a huey */
static inst_code
huey_take_first_raw_measurement_2(
	huey *p,				/* Object */
	double rgb[3]			/* Return the RGB values */
) {
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

/* Take a raw subsequent (CRT + LCD) mesurement from the device for a huey */
static inst_code
huey_take_raw_measurement_2(
	huey *p,			/* Object */
	int edgec[3],		/* Measurement edge count for each channel */
	double rgb[3]		/* Return the RGB values */
) {
	unsigned char ibuf[16];
	unsigned char obuf[16];
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
	unsigned char ibuf[16];
	unsigned char obuf[16];
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
	int crtm,				/* nz if crt mode */
	double rgb[3]			/* Return the rgb values */
) {
	int i;
	int edgec[3] = {1,1,1};	/* Measurement edge count for each channel */
	int rem[3] = {1,1,1};	/* remeasure flags */
	inst_code ev;

	if (p->inited == 0)
		return huey_interp_code((inst *)p, HUEY_NOT_INITED);

	DBG((dbgo,"take_measurement_2 called with crtm = %d\n",crtm));

	/* For CRT mode, do an initial set of syncromized measurements */
	if (crtm) {

		if ((ev = huey_take_first_raw_measurement_2(p, rgb)) != inst_ok)
			return ev;

		DBG((dbgo,"Raw initial CRT RGB = %f %f %f\n",rgb[0],rgb[1],rgb[2]))

		/* Decide whether any channels need re-measuring, */
		/* and computed cooked values. Threshold is typically 75 */
		for (i = 0; i < 3; i++) {
			rem[i] = (rgb[i] <= (0.75 * (double)p->sampno)) ? 1 : 0;
			rgb[i] = 0.5 * rgb[i] * 1e6/(double)p->int_clocks;
		}
		DBG((dbgo,"Re-measure flags = %d %d %d\n",rem[0],rem[1],rem[2]))
	}

	/* If any need re-measuring */
	if (rem[0] || rem[1] || rem[2]) {
		double rgb2[3];

		/* Do a first or second set of measurements */
		if ((ev = huey_take_raw_measurement_2(p, edgec, rgb2)) != inst_ok)
			return ev;
		DBG((dbgo,"Raw initial/subsequent ecount %d %d %d RGB = %f %f %f\n",
		     edgec[0], edgec[1], edgec[2], rgb2[0], rgb2[1], rgb2[2]))

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
	
			if ((ev = huey_take_raw_measurement_2(p, edgec, rgb3)) != inst_ok)
				return ev;
	
			DBG((dbgo,"Raw subsequent2 ecount %d %d %d RGB = %f %f %f\n",
			     edgec[0], edgec[1], edgec[2], rgb3[0], rgb3[1], rgb3[2]))

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
				DBG((dbgo,"%d after scale = %f\n",i,rgb[i]))
		
				rgb[i] -= p->dark_cal[i];		/* Subtract black level */
				DBG((dbgo,"%d after sub black = %f\n",i,rgb[i]))
		
				if (rgb[i] < 0.0001)
					rgb[i] = 0.0001;
				DBG((dbgo,"%d after limit min = %f\n",i,rgb[i]))
			}
		}
	}

	DBG((dbgo,"Cooked RGB = %f %f %f\n",rgb[0],rgb[1],rgb[2]))
	
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
	unsigned char ibuf[16];
	unsigned char obuf[16];
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
	int crtm,			/* nz if crt mode */
	double *amb			/* Return the ambient value */
) {
	int rb;				/* Returned byte - not used */
	inst_code ev;

	if (p->inited == 0)
		return huey_interp_code((inst *)p, HUEY_NOT_INITED);

	DBG((dbgo,"take_amb_measurement_2 called with crtm = %d\n",crtm));

	/* First param is always 3, second is sync mode */
	if ((ev = huey_take_amb_measurement_1(p, 3, crtm ? 2 : 0, amb, &rb)) != inst_ok)
 		return ev;
	DBG((dbgo,"Raw ambient = %f\n",*amb))
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

	if ((p->mode & inst_mode_measurement_mask) == inst_mode_emis_ambient) {
		if ((ev = huey_take_amb_measurement(p, p->crt, &XYZ[1])) != inst_ok)
			return ev;
		XYZ[1] *= AMB_SCALE_FACTOR;		/* Times aproximate fudge factor */
		XYZ[0] = icmD50.X * XYZ[1];		/* Convert to D50 neutral */
		XYZ[2] = icmD50.Z * XYZ[1];
	} else {
		if ((ev = huey_take_measurement_2(p, p->crt, rgb)) != inst_ok)
			return ev;

		if (p->crt)
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
	DBG((dbgo,"returning XYZ = %f %f %f\n",XYZ[0],XYZ[1],XYZ[2]))
	return inst_ok;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Check the device is responding, and unlock if necessary */
static inst_code
huey_check_unlock(
	huey *p				/* Object */
) {
	unsigned char buf[8];
	inst_code ev;

	if (p->debug) fprintf(stderr,"huey: about to check response and unlock instrument if needed\n");

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
		if (p->debug) fprintf(stderr,"huey: unknown model '%s'\n",buf);
		return huey_interp_code((inst *)p, HUEY_UNKNOWN_MODEL);
	}

	if (p->debug) fprintf(stderr,"huey: instrument is responding, unlocked, and right type\n");

	return inst_ok;
}

/* Read all the relevant register values */
static inst_code
huey_read_all_regs(
	huey *p				/* Object */
) {
	inst_code ev;
	int i;

	if (p->debug) fprintf(stderr,"huey: about to read all the registers\n");

	/* Serial number */
	if ((ev = huey_rdreg_word(p, &p->ser_no, 0) ) != inst_ok)
		return ev;
	DBG((dbgo,"serial number = %d\n",p->ser_no))


	/* LCD/user calibration values */
	for (i = 0; i < 9; i++) {
		if ((ev = huey_rdreg_float(p, &p->LCD_cal[i], 4 + 4 * i) ) != inst_ok)
			return ev;
		DBG((dbgo,"LCD/user cal[%d] = %f\n",i,p->LCD_cal[i]))
	}
	/* LCD/user calibration time */
	if ((ev = huey_rdreg_word(p, &p->LCD_caltime, 50) ) != inst_ok)
		return ev;
	DBG((dbgo,"LCD/user calibration time = 0x%x = %s\n",p->LCD_caltime, ctime((time_t *)&p->LCD_caltime)))


	/* CRT/factory calibration values */
	for (i = 0; i < 9; i++) {
		if ((ev = huey_rdreg_float(p, &p->CRT_cal[i], 54 + 4 * i) ) != inst_ok)
			return ev;
		DBG((dbgo,"CRT/factory cal[%d] = %f\n",i,p->CRT_cal[i]))
	}
	/* CRT/factory calibration flag */
	if ((ev = huey_rdreg_word(p, &p->CRT_caltime, 90) ) != inst_ok)
		return ev;
	DBG((dbgo,"CRT/factory flag = 0x%x = %s\n",p->CRT_caltime, ctime((time_t *)&p->CRT_caltime)))


	/* Hard coded in Huey */
	p->clk_prd = 1e-6;
	DBG((dbgo,"Clock period = %f\n",p->clk_prd))

	/* Dark current calibration values */
	for (i = 0; i < 3; i++) {
		if ((ev = huey_rdreg_float(p, &p->dark_cal[i], 103 + 4 * i)) != inst_ok) {
			if ((ev & inst_imask) != HUEY_FLOAT_NOT_SET)
				return ev;
			p->dark_cal[i] = 0.0;
		}
		DBG((dbgo,"darkcal[%d] = %f\n",i,p->dark_cal[i]))
	}

	/* Ambient darkcurrent calibration value ? */
	if ((ev = huey_rdreg_float(p, &p->amb_cal, 148)) != inst_ok) {
		if ((ev & inst_imask) != HUEY_FLOAT_NOT_SET)
			return ev;
		p->amb_cal = 0.0;
	}
	DBG((dbgo,"Ambient cal = %f\n",p->amb_cal))

	/* Unlock string */
	for (i = 0; i < 4; i++) {
		int vv;
		if ((ev = huey_rdreg_byte(p, &vv, 122 + i) ) != inst_ok)
			return ev;
		p->unlk_string[i] = (char)vv;
	}
	p->unlk_string[i] = '\000';
	DBG((dbgo,"unlock string = '%s'\n",p->unlk_string))

	/* Read the integration time */
	if ((ev = huey_rd_int_time(p, &p->int_clocks) ) != inst_ok)
		return ev;
	DBG((dbgo,"Integration time = %d\n",p->int_clocks))

	if (p->debug) fprintf(stderr,"huey: all registers read OK\n");

	return inst_ok;
}

/* Compute factors that depend on the register values */
static inst_code
huey_compute_factors(
	huey *p				/* Object */
) {
	/* Check that certain value are valid */
	if (p->ser_no == 0xffffffff)
		/* (It appears that some instruments have no serial number!) */
//		return huey_interp_code((inst *)p, HUEY_BAD_SERIAL_NUMBER);
		warning("huey: bad instrument serial number");

	if (p->LCD_caltime == 0xffffffff)
		return huey_interp_code((inst *)p, HUEY_BAD_LCD_CALIBRATION);

	if (p->CRT_caltime == 0xffffffff)
		return huey_interp_code((inst *)p, HUEY_BAD_CRT_CALIBRATION);

	/* clk_prd inversion */
	p->clk_freq = 1.0/p->clk_prd;
	DBG((dbgo,"clk_freq = %f\n",p->clk_freq))
	
	/* Set some defaults */
	p->sampno = 100;		/* Minimum sampling count */

	return inst_ok;
}

/* ------------------------------------------------------------------------ */

/* Establish communications with a HUEY */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
huey_init_coms(inst *pp, int port, baud_rate br, flow_control fc, double tout) {
	huey *p = (huey *) pp;
	unsigned char buf[8];
	inst_code ev = inst_ok;
	char **pnames = NULL;
	int retries = 0;

	if (p->debug) {
		p->icom->debug = p->debug;	/* Turn on debugging */
		fprintf(stderr,"huey: About to init coms\n");
	}

	/* Open as an HID if available */
	if (p->icom->is_hid_portno(p->icom, port) != instUnknown) {

		if (p->debug) fprintf(stderr,"huey: About to init HID\n");

		/* Set config, interface */
		p->icom->set_hid_port(p->icom, port, icomuf_none, retries, pnames); 

		if (p->icom->vid == 0x0765 && p->icom->pid == 0x5001) {
			if (p->debug) fprintf(stderr,"huey: Lenovo version\n");
			p->lenovo = 1;
		}

	} else if (p->icom->is_usb_portno(p->icom, port) != instUnknown) {

		if (p->debug) fprintf(stderr,"huey: About to init USB\n");

		/* Set config, interface, write end point, read end point */
		/* ("serial" end points aren't used - the huey uses USB control messages) */
		/* We need to detatch the HID driver on Linux */
		p->icom->set_usb_port(p->icom, port, 1, 0x00, 0x00, icomuf_detach, 0, NULL); 

		if (p->icom->vid == 0x0765 && p->icom->pid == 0x5001) {
			if (p->debug) fprintf(stderr,"huey: Lenovo version\n");
			p->lenovo = 1;
		}

	} else {
		if (p->debug) fprintf(stderr,"huey: init_coms called to wrong device!\n");
			return huey_interp_code((inst *)p, HUEY_UNKNOWN_MODEL);
	}

	/* Check instrument is responding */
	if ((ev = huey_command(p, i1d_status, buf, buf, 1.0, 1.0)) != inst_ok) {
		if (p->debug) fprintf(stderr,"huey: init coms failed with rv = 0x%x\n",ev);
		return ev;
	}

	if (p->debug) fprintf(stderr,"huey: init coms has suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

/* Initialise the HUEY */
/* return non-zero on an error, with dtp error code */
static inst_code
huey_init_inst(inst *pp) {
	huey *p = (huey *)pp;
	inst_code ev = inst_ok;

	if (p->debug) fprintf(stderr,"huey: About to init instrument\n");

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

	p->trig = inst_opt_trig_keyb;

	if (ev == inst_ok) {
		p->inited = 1;
		if (p->debug) fprintf(stderr,"huey: instrument inited OK\n");
	}

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

	return ev;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
huey_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val) {		/* Pointer to instrument patch value */
	huey *p = (huey *)pp;
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
			return huey_interp_code((inst *)p, icoms2huey_err(se));
		}
		user_trig = 1;
		if (p->trig_return)
			printf("\n");
	}

	/* Read the XYZ value */
	if ((rv = huey_take_XYZ_measurement(p, val->aXYZ)) != inst_ok) {
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

	if (mtx == NULL)
		icmSetUnity3x3(p->ccmat);
	else
		icmCpy3x3(p->ccmat, mtx);
		
	return inst_ok;
}

/* Determine if a calibration is needed. Returns inst_calt_none if not, */
/* inst_calt_unknown if it is unknown, or inst_calt_crt_freq for an */
/* Eye-One Display 2 if a frequency calibration is needed, */
/* and we are in CRT mode */
inst_cal_type huey_needs_calibration(inst *pp) {
	huey *p = (huey *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	return inst_ok;
}

/* Request an instrument calibration. */
inst_code huey_calibrate(
inst *pp,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	huey *p = (huey *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	id[0] = '\000';

	return inst_unsupported;
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
		case HUEY_USER_ABORT:
			return "User hit Abort key";
		case HUEY_USER_TERM:
			return "User hit Terminate key";
		case HUEY_USER_TRIG:
			return "User hit Trigger key";
		case HUEY_USER_CMND:
			return "User hit a Command key";

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

		case HUEY_USER_ABORT:
			return inst_user_abort | ec;
		case HUEY_USER_TERM:
			return inst_user_term | ec;
		case HUEY_USER_TRIG:
			return inst_user_trig | ec;
		case HUEY_USER_CMND:
			return inst_user_cmnd | ec;

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
	free(p);
}

/* Return the instrument capabilities */
inst_capability huey_capabilities(inst *pp) {
	inst_capability rv;

	rv = inst_emis_spot
	   | inst_emis_disp
	   | inst_emis_disptype
	   | inst_emis_disptypem
	   | inst_colorimeter
	   | inst_ccmx
	   | inst_emis_ambient
	   | inst_emis_ambient_mono;
	     ;

	return rv;
}

/* Return the instrument capabilities 2 */
inst2_capability huey_capabilities2(inst *pp) {
	inst2_capability rv = 0;

	rv |= inst2_prog_trig;
	rv |= inst2_keyb_trig;
	rv |= inst2_has_leds;

	return rv;
}

inst_disptypesel huey_disptypesel[3] = {
	{
		1,
		"c",
		"Huey: CRT display",
		1
	},
	{
		2,
		"l",
		"Huey: LCD display",
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
static inst_code huey_get_opt_details(
inst *pp,
inst_optdet_type m,	/* Requested option detail type */
...) {				/* Status parameters */                             
	if (m == inst_optdet_disptypesel) {
		va_list args;
		int *pnsels;
		inst_disptypesel **psels;

		va_start(args, m);
		pnsels = va_arg(args, int *);
		psels = va_arg(args, inst_disptypesel **);
		va_end(args);

		*pnsels = 2;
		*psels = huey_disptypesel;
		
		return inst_ok;
	}

	return inst_unsupported;
}

/* Set device measurement mode */
inst_code huey_set_mode(inst *pp, inst_mode m)
{
	huey *p = (huey *)pp;
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

/* 
 * set or reset an optional mode
 * We assume that the instrument has been initialised.
 */
static inst_code
huey_set_opt_mode(inst *pp, inst_opt_mode m, ...)
{
	huey *p = (huey *)pp;

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

		if (ix == 1) {
			if (p->crt == 0)
			p->crt = 1;
			return inst_ok;
		} else if (ix == 2) {
			if (p->crt != 0)
			p->crt = 0;
			return inst_ok;
		} else {
			return inst_unsupported;
		}
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
extern huey *new_huey(icoms *icom, instType itype, int debug, int verb)
{
	huey *p;
	if ((p = (huey *)calloc(sizeof(huey),1)) == NULL)
		error("huey: malloc failed!");

	if (icom == NULL)
		p->icom = new_icoms();
	else
		p->icom = icom;

	p->debug = debug;
	p->verb = verb;

	icmSetUnity3x3(p->ccmat);	/* Set the colorimeter correction matrix to do nothing */

	p->init_coms         = huey_init_coms;
	p->init_inst         = huey_init_inst;
	p->capabilities      = huey_capabilities;
	p->capabilities2     = huey_capabilities2;
	p->get_opt_details   = huey_get_opt_details;
	p->set_mode          = huey_set_mode;
	p->set_opt_mode      = huey_set_opt_mode;
	p->read_sample       = huey_read_sample;
	p->needs_calibration = huey_needs_calibration;
	p->col_cor_mat       = huey_col_cor_mat;
	p->calibrate         = huey_calibrate;
	p->interp_error      = huey_interp_error;
	p->del               = huey_del;

	p->itype = itype;

	return p;
}

