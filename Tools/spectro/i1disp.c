
/* 
 * Argyll Color Correction System
 *
 * Gretag i1Display related functions
 *
 * Author: Graeme W. Gill
 * Date:   18/10/2006
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
#else	/* !SALONEINSTLIB */
#include "sa_config.h"
#include "numsup.h"
#endif /* !SALONEINSTLIB */
#include "xspect.h"
#include "insttypes.h"
#include "icoms.h"
#include "conv.h"
#include "i1disp.h"

#undef DEBUG

#define dbgo stderr

#ifdef DEBUG
#define DBG(xxx) fprintf xxx ;
#else
#define DBG(xxx) if (p->debug >= 1) fprintf xxx ; 
#endif

static inst_code i1disp_interp_code(inst *pp, int ec);
static inst_code i1disp_do_fcal_setit(i1disp *p);
static inst_code i1disp_check_unlock(i1disp *p);

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 5000		/* Maximum reading messagle reply size */

#define CALFACTOR 3.428			/* Emissive magic calibration factor */

/* ------------------------------------------------------------------------ */
/* Implementation */

/* Interpret an icoms error into a I1DISP error */
static int icoms2i1disp_err(int se) {
	if (se & ICOM_USERM) {
		se &= ICOM_USERM;
		if (se == ICOM_USER)
			return I1DISP_USER_ABORT;
		if (se == ICOM_TERM)
			return I1DISP_USER_TERM;
		if (se == ICOM_TRIG)
			return I1DISP_USER_TRIG;
		if (se == ICOM_CMND)
			return I1DISP_USER_CMND;
	}
	if (se != ICOM_OK)
		return I1DISP_COMS_FAIL;
	return I1DISP_OK;
}

/* i1Display command codes - number:X is argument count:return type */
/* B = byte (8bit), S = short (16bit), W = word (32bit), */
/* A = string (5 bytes total max) , - = none */
typedef enum {
    i1d_status       = 0x00,		/* -:A    Get status string */
    i1d_rd_red       = 0x01,		/* -:W    Read the red channel clk count */
    i1d_rd_green     = 0x02,		/* -:W    Read the green channel clk count */
    i1d_rd_blue      = 0x03,		/* -:W    Read the blue channel clk count */
    i1d_getmeas_p    = 0x04,		/* -:W    Read the measure refresh period */
    i1d_setintgt     = 0x05,		/* W:-    Set the integration time */
    i1d_getintgt     = 0x06,		/* -:W    Get the integration time */
    i1d_wrreg        = 0x07,		/* BB:-   Write a register value */
    i1d_rdreg        = 0x08,		/* B:B    Read a register value */
    i1d_getmeas_p2   = 0x09,		/* -:W    Read the measure refresh period (finer ?) */
    i1d_m_red_p      = 0x0a,		/* B:W    Measure the red period */
    i1d_m_green_p    = 0x0b,		/* B:W    Measure the green period */
    i1d_m_blue_p     = 0x0c,		/* B:W    Measure the blue period */
    i1d_m_rgb_p      = 0x0d,		/* BBB:W  Measure the RGB period for given edge count */
    i1d_unlock       = 0x0e,		/* BBBB:- Unlock the interface */

    i1d_m_red_p2     = 0x10,		/* S:W    Measure the red period (16 bit ?) */
    i1d_m_green_p2   = 0x11,		/* S:W    Measure the green period (16 bit ?) */
    i1d_m_blue_p2    = 0x12,		/* S:W    Measure the blue period (16 bit ?) */
									/* S = ??? */

    i1d_m_red_2      = 0x13,		/* B:W    Measure the red channel (16 bit ?) */
									/* B = sync mode, typically 1 */

    i1d_setmedges2   = 0x14,		/* SB:-   Set number of edges used for measurment 16 bit */
									/* B = channel */
    i1d_getmedges2   = 0x15,		/* B:S    Get number of edges used for measurment 16 bit */
									/* B = channel */

    i1d_m_rgb_edge_2 = 0x16,		/* -:W    Measure RGB Edge (16 bit ?) */

    i1d_set_pll_p    = 0x11,		/* SS:-   Set PLL period */
    i1d_get_pll_p    = 0x12,		/* -:W    Get PLL period */

    i1d_m_rgb_edge_3 = 0x10,		/* BBBB:W Measure RGB Edge */
									/* BBBB = ??? */
    i1d_g_green_3    = 0x11,		/* -:W    Get green data */
    i1d_g_blue_3     = 0x12,		/* -:W    Get blue data */

    i1d_wrxreg       = 0x13,		/* SB:-   Write an extended register value */
    i1d_rdxreg       = 0x14 		/* S:B    Read an extended register value */
} i1DispCC;

/* Do a command/response exchange with the i1disp. */
/* Return the error code */
/* The i1 display uses a rather convoluted means of communication (historical?). */
/* Messages from the host are conveyed one byte per USB control message, */
/* with the byte conveyed in bRequest, with each such control message reading */
/* 8 bytes of data from the device. The last 8 bytes read in an overall */
/* message contains up to 5 response bytes, the last always being nul (?). */
/* Muti-byte quantities are transmitted in big-endian order. */
/* (Instructions 1,2,3,8 & 9 seem to be retried up to 5 times. We're */
/* not doing this, as it may just be historical) */
static inst_code
i1disp_command_1(
	i1disp *p,									/* i1display object */
	i1DispCC cc,								/* Command code */
	unsigned char *in, int insize,				/* Parameter to send */
	unsigned char *out, int bsize, int *rsize,	/* Parameter returned */
	double to									/* Timeout in seconds */
) {
	int requesttype;		/* 8 bit request type (USB bmRequestType) */
	int request;			/* 8 bit request code (USB bRequest) */
	int value;				/* 16 bit value (USB wValue, sent little endian) */
	int index;				/* 16 bit index (USB wIndex, sent little endian) */
	int rwsize;				/* 16 bit data size (USB wLength, send little endian) */ 
	int rcc = 0;			/* Return cc code from instruction */
	int i, tsize;
	unsigned char buf[8];	/* 8 bytes to read */
	int se, ua = 0, rv = inst_ok;
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;
	
	tsize = insize + 2;
	*rsize = 0;

	if (isdeb > 1) fprintf(dbgo,"i1disp: Sending cmd %02x args '%s'",cc, icoms_tohex(in, insize));

	/* For each byte to be sent */
	for (i = 0; i < tsize; i++) {
		unsigned int smsec;

		/* Control message to read 8 bytes */
		requesttype = USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_ENDPOINT;
		if (i == 0) 				/* Count */
			request = insize + 1;
		else if (i == 1)			/* Command */
			request = (int)cc;
		else						/* Data */
			request = (int)in[i-2];
		value = i;					/* Incrementing count */
		index = (tsize - i - 1);	/* Decrementing count */
		rwsize = 8;
		
		smsec = msec_time();
		if ((se = p->icom->usb_control(p->icom, requesttype, request, value, index,
		                                                        buf, rwsize, to)) != 0) {
			if (se & ICOM_USERM)
				ua = (se & ICOM_USERM);
			if (se & ~ICOM_USERM) {
				if (isdeb > 1) {
					fprintf(dbgo,"\ni1disp: Message send failed with ICOM err 0x%x\n",se);
					if (se & ICOM_TO)
						fprintf(dbgo,"\ni1disp: Timeout %f sec, Took %f sec.\n",to, (msec_time() - smsec)/1000.0);

				}
				p->icom->debug = isdeb;
				return i1disp_interp_code((inst *)p, I1DISP_COMS_FAIL);
			}
		}

		/* We could check the return data. This seems to be what we sent, */
		/* unless it's the last exchange in the message sequence. */
		/* I don't currently know how or if the device signals an error. */

		/* If this is the last exchange, copy return value out */
		if (i == (tsize-1)) {
			*rsize = buf[1];
			if (*rsize > bsize)
				*rsize = bsize;
			if (*rsize > 5)
				*rsize = 5;
			memmove(out, buf + 3, *rsize);

			/* buf[2] is usually the cc, except for i1d_unlock. */
			/* If it is not the cc, this may indicate that the command */
			/* should be retried up to a total of 5 times, before */
			/* assuming it has suceeded. */
			rcc = buf[2] & 0xff;
		}
	}
	rv = i1disp_interp_code((inst *)p, icoms2i1disp_err(ua));

	if (rv == inst_ok && rcc != cc)
		rv = i1disp_interp_code((inst *)p, I1DISP_NOT_READY);

	/* Instrument returns "LOCK" to any instruction if it is locked */
	if (rv == inst_ok && *rsize == 5 && strncmp((char *)out,"LOCK",4) == 0) {
		rv = i1disp_interp_code((inst *)p, I1DISP_LOCKED);
	}

	if (isdeb > 1) fprintf(dbgo," response '%s' ICOM err 0x%x\n",icoms_tohex(out, *rsize),ua);
	p->icom->debug = isdeb;

	return rv; 
}

/* Do a command/response exchange with the i1disp, taking care of */
/* a LOCK error */
static inst_code
i1disp_command(
	i1disp *p,									/* i1display object */
	i1DispCC cc,								/* Command code */
	unsigned char *in, int insize,				/* Parameter to send */
	unsigned char *out, int bsize, int *rsize,	/* Parameter returned */
	double to									/* Timeout in seconds */
) {
	inst_code rv;

	if ((rv = i1disp_command_1(p, cc, in, insize, out, bsize, rsize, to)) == inst_ok)
		return rv;

	/* Unlock and try again */
	if ((rv & inst_imask) == I1DISP_LOCKED) {
		if ((rv = i1disp_check_unlock(p)) != inst_ok)
			return rv;
		rv = i1disp_command_1(p, cc, in, insize, out, bsize, rsize, to);
	}
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
	val = (signed char)buf[0];
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[3]));
	return val;
}

/* Read a byte from a register */
static inst_code
i1disp_rdreg_byte(
	i1disp *p,				/* Object */
	int *outp,				/* Where to write value */
	int addr				/* Register Address, 0 - 127 */
) {
	unsigned char c, buf[16];
	int rsize;
	inst_code ev;

	if (p->dtype == 0) {
		if (addr < 0 || addr > 127)
			return i1disp_interp_code((inst *)p, I1DISP_BAD_REG_ADDRESS);
	} else {
		if (addr < 0 || addr > 159)
			return i1disp_interp_code((inst *)p, I1DISP_BAD_REG_ADDRESS);
	}
	c = (unsigned char)addr;  

	/* Read a byte */
	if ((ev = i1disp_command(p, i1d_rdreg, &c, 1,
		         buf, 8, &rsize, 0.5)) != inst_ok)
		return ev;
	
	if (rsize != 3)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);

	if (buf[0] != c)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_VAL);
		
	*outp = (int)buf[1];

	return inst_ok;
}

/* Read a short from a register */
static inst_code
i1disp_rdreg_short(
	i1disp *p,				/* Object */
	int *outp,				/* Where to write value */
	int addr				/* Register Address, 0 - 126 */
) {
	inst_code ev;
	int v, val;

	if ((ev = i1disp_rdreg_byte(p, &v, addr)) != inst_ok)
		return ev;
	val = v;

	if ((ev = i1disp_rdreg_byte(p, &v, addr+1)) != inst_ok)
		return ev;
	val = ((val << 8) + (0xff & v));

	*outp = val;

	return inst_ok;
}

/* Read a word from a register */
static inst_code
i1disp_rdreg_word(
	i1disp *p,				/* Object */
	int *outp,				/* Where to write value */
	int addr				/* Register Address, 0 - 124 */
) {
	inst_code ev;
	int v, val;

	if ((ev = i1disp_rdreg_byte(p, &v, addr)) != inst_ok)
		return ev;
	val = v;

	if ((ev = i1disp_rdreg_byte(p, &v, addr+1)) != inst_ok)
		return ev;
	val = ((val << 8) + (0xff & v));

	if ((ev = i1disp_rdreg_byte(p, &v, addr+2)) != inst_ok)
		return ev;
	val = ((val << 8) + (0xff & v));

	if ((ev = i1disp_rdreg_byte(p, &v, addr+3)) != inst_ok)
		return ev;
	val = ((val << 8) + (0xff & v));

	*outp = val;

	return inst_ok;
}


/* Read a float from a register */
/* Will return I1DISP_FLOAT_NOT_SET if the float value was 0xffffffff */
static inst_code
i1disp_rdreg_float(
	i1disp *p,				/* Object */
	double *outp,			/* Where to write value */
	int addr				/* Register Address, 0 - 124 */
) {
	inst_code ev;
	int val;

	if ((ev = i1disp_rdreg_word(p, &val, addr)) != inst_ok)
		return ev;

	if (ev == 0xffffffff) {
		return I1DISP_FLOAT_NOT_SET;
	}

	*outp = IEEE754todouble((unsigned int)val);
	return inst_ok;
}


/* Write a byte to a register */
static inst_code
i1disp_wrreg_byte(
	i1disp *p,				/* Object */
	int inv,				/* Input value */
	int addr				/* Register Address, 0 - 127 */
) {
	int cval;
	unsigned char ibuf[16], obuf[16];
	int rsize;
	inst_code ev;

	inv &= 0xff;

	/* Read it first, to see if it needs writing */
	if ((ev = i1disp_rdreg_byte(p, &cval, addr) ) != inst_ok)
		return ev;

	if (cval == inv) 	/* No need to write */
		return inst_ok;

	ibuf[0] = (unsigned char)addr;  
	ibuf[1] = (unsigned char)inv;  

	/* Write a byte */
	if ((ev = i1disp_command(p, i1d_wrreg, ibuf, 2,
		         obuf, 8, &rsize, 0.5)) != inst_ok)
		return ev;

	if (rsize != 2)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);

	if (obuf[0] != addr)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_VAL);
		
	/* Check it got written properly */
	if ((ev = i1disp_rdreg_byte(p, &cval, addr) ) != inst_ok)
		return ev;

	cval &= 0xff;
	if (cval != inv)	/* No need to write */
		return i1disp_interp_code((inst *)p, I1DISP_EEPROM_WRITE_FAIL);

	return inst_ok;
}

/* Write a word to a register */
static inst_code
i1disp_wrreg_word(
	i1disp *p,				/* Object */
	int inv,				/* Where to write value */
	int addr				/* Register Address, 0 - 124 */
) {
	inst_code ev;
	int v;

	v = (inv >> 24) & 0xff;
	if ((ev = i1disp_wrreg_byte(p, v, addr) ) != inst_ok)
		return ev;

	v = (inv >> 16) & 0xff;
	if ((ev = i1disp_wrreg_byte(p, v, addr+1) ) != inst_ok)
		return ev;

	v = (inv >> 8) & 0xff;
	if ((ev = i1disp_wrreg_byte(p, v, addr+2) ) != inst_ok)
		return ev;

	v = (inv >> 0) & 0xff;
	if ((ev = i1disp_wrreg_byte(p, v, addr+3) ) != inst_ok)
		return ev;

	return inst_ok;
}

/* Write a float to a register */
static inst_code
i1disp_wrreg_float(
	i1disp *p,				/* Object */
	double inv,				/* Value to write */
	int addr				/* Register Address, 0 - 124 */
) {
	inst_code ev;
	int val;

	val = (int)doubletoIEEE754(inv);

	if ((ev = i1disp_wrreg_word(p, val, addr)) != inst_ok)
		return ev;
	return inst_ok;
}

/* Read the integration time */
static inst_code
i1disp_rd_int_time(
	i1disp *p,				/* Object */
	int *outp				/* Where to write value */
) {
	unsigned char buf[16];
	int rsize;
	inst_code ev;

	if ((ev = i1disp_command(p, i1d_getintgt, NULL, 0,
		         buf, 8, &rsize, 0.5)) != inst_ok)
		return ev;
	
	if (rsize != 5)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);

	*outp = buf2int(buf);

	return inst_ok;
}

/* Set the integration time */
static inst_code
i1disp_wr_int_time(
	i1disp *p,				/* Object */
	int inv					/* Value to write */
) {
	unsigned char buf[16];
	int rsize;
	inst_code ev;

	int2buf(buf, inv);
	if ((ev = i1disp_command(p, i1d_setintgt, buf, 4,
		         buf, 8, &rsize, 0.5)) != inst_ok)
		return ev;

	return inst_ok;
}

/* Read the refresh period */
static inst_code
i1disp_rd_meas_ref_period(
	i1disp *p,				/* Object */
	int *outp				/* Where to write value */
) {
	unsigned char buf[16];
	int rsize;
	inst_code ev;

	if ((ev = i1disp_command(p, i1d_getmeas_p2, NULL, 0,
		         buf, 8, &rsize, 1.5)) != inst_ok)
		return ev;
	
	if (rsize != 5)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);

	*outp = buf2int(buf);

	return inst_ok;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Take a raw RGB period measurement from the device for an i1d1. */
/* The time taken to count the given number of L2F clock edges (+ve & -ve) */
/* is measured in clk clk_freq counts. */ 
static inst_code
i1d1_period_measure(
	i1disp *p,				/* Object */
	int edgec[3],			/* Number of clock edges to count */
	double rgb[3]			/* Return the number of clk's */
) {
	int i;
	unsigned char ibuf[16];
	unsigned char obuf[16];
	int rsize;
	inst_code ev;

	/* Sanity check the number of edges */
	for (i = 0; i < 3; i++) {
		if (edgec[i] < 1 || edgec[i] > 255)
			return i1disp_interp_code((inst *)p, I1DISP_BAD_INT_THRESH);
		ibuf[i] = (char)edgec[i];
	}

	/* Do the measurement, and return the Red value */
	if ((ev = i1disp_command(p, i1d_m_rgb_p, ibuf, 3,
		         obuf, 8, &rsize, 60.0)) != inst_ok)
		return ev;
	if (rsize != 5)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);
	rgb[0] = (double)buf2int(obuf);

	/* Get the green value */
	if ((ev = i1disp_command(p, i1d_rd_green, NULL, 0,
		         obuf, 8, &rsize, 0.5)) != inst_ok)
		return ev;
	if (rsize != 5)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);
	rgb[1] = (double)buf2int(obuf);

	/* Get the blue value */
	if ((ev = i1disp_command(p, i1d_rd_blue, NULL, 0,
		         obuf, 8, &rsize, 0.5)) != inst_ok)
		return ev;
	if (rsize != 5)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);
	rgb[2] = (double)buf2int(obuf);

	return inst_ok;
}

/* Take a cooked period measurement from the device for the i1d1 */
/* and return the frequency for each sensor. */
static inst_code
i1d1_take_measurement(
	i1disp *p,				/* Object */
	int cal,				/* nz if black is not to be subtracted */
	double rgb[3]			/* Return the rgb frequency values */
) {
	int i;
	int edgec[3];		/* Edge count 1..255 for each channel */
	inst_code ev;

	if (p->inited == 0)
		return i1disp_interp_code((inst *)p, I1DISP_NOT_INITED);

	if (p->dtype != 0)
		return i1disp_interp_code((inst *)p, I1DISP_WRONG_DEVICE);

	/* Do an initial measurement with minimum edge count of 1 */
	edgec[0] = edgec[1] = edgec[2] = 1;

	if ((ev = i1d1_period_measure(p, edgec, rgb)) != inst_ok)
		return ev;

	DBG((dbgo, "Initial RGB = %f %f %f\n",rgb[0],rgb[1],rgb[2]))

	/* Compute adjusted edge count, aiming */
	/* for count values of clk_freq = 1 second (~1e6). */
	for (i = 0; i < 3; i++) {
		double ns;
		if (p->clk_freq > ((255.0 - 0.5) * rgb[i]))
			ns = 255.0;
		else {
			ns = floor(p->clk_freq/rgb[i]) + 0.5;
			if (ns < 1.0)
				ns = 1.0;
		}
		edgec[i] = (int)ns;
	}

	/* Only if we compute a different edge count, read again */
	if (edgec[0] > 1 || edgec[1] > 1 || edgec[2] > 1) {
		double rgb2[3];		/* 2nd RGB Readings */

		if ((ev = i1d1_period_measure(p, edgec, rgb2)) != inst_ok)
			return ev;

		/* Average readings if we repeated a measurement with the same edge count */
		/* (Minor advantage, but may as well use it) */
		for (i = 0; i < 3; i++) {
			if (edgec[i] == 1)
				rgb[i] = 0.5 * (rgb[i] + rgb2[i]);
			else
				rgb[i] = rgb2[i];
		}
	}

	DBG((dbgo, "scaled %d %d %d gives RGB = %f %f %f\n", edgec[0],edgec[1],edgec[2], rgb[0],rgb[1],rgb[2]))

	/* Compute adjusted readings as a frequency. */
	/* We need to divide the number of edges/2 by the period in seconds */
	for (i = 0; i < 3; i++) {
		rgb[i] = (p->rgbadj2[i] * (double)edgec[i])/(rgb[i] * 2.0 * p->clk_prd);
		DBG((dbgo, "%d sensor frequency = %f\n",i,rgb[i]))

		/* If we're not calibrating the black */
		if (cal == 0) {
			rgb[i] -= p->reg103_F[i];		/* Subtract black level */
			DBG((dbgo, "%d after sub black = %f\n",i,rgb[i]))
	
			if (rgb[i] < 0.0001)
				rgb[i] = 0.0001;
			DBG((dbgo, "%d after limit min = %f\n",i,rgb[i]))
		}
	}
	DBG((dbgo, "Adjusted RGB = %f %f %f\n",rgb[0],rgb[1],rgb[2]))

	return inst_ok;
}

/* . . . . . . . . . . . . . . . . . . . . . . . . */

/* Take a fixed period frequency measurement from the device for an i1d2. */
/* This measures edge count over the set integration period. */

/* Take a raw measurement using a given integration time. */
/* The measureent is the count of (both) edges from the L2V */
/* over the integration time */
static inst_code
i1d2_freq_measure(
	i1disp *p,				/* Object */
	double rgb[3]			/* Return the RGB edge count values */
) {
	unsigned char ibuf[16];
	unsigned char obuf[16];
	int rsize;
	inst_code ev;

	/* Do the measurement, and return the Red value */
	ibuf[0] = 1;		/* Sync mode 1 */
	if ((ev = i1disp_command(p, i1d_m_red_2, ibuf, 1,
		         obuf, 8, &rsize, p->samptime + 1.0)) != inst_ok)
		return ev;
	if (rsize != 5)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);
	rgb[0] = (double)buf2int(obuf);

	/* Get the green value */
	if ((ev = i1disp_command(p, i1d_rd_green, NULL, 0,
		         obuf, 8, &rsize, 0.5)) != inst_ok)
		return ev;
	if (rsize != 5)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);
	rgb[1] = (double)buf2int(obuf);

	/* Get the blue value */
	if ((ev = i1disp_command(p, i1d_rd_blue, NULL, 0,
		         obuf, 8, &rsize, 0.5)) != inst_ok)
		return ev;
	if (rsize != 5)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);
	rgb[2] = (double)buf2int(obuf);

	return inst_ok;
}

/* Meaure the period by Take a raw initial subsequent mesurement from the device for an i1d2 */
static inst_code
i1disp_period_measure(
	i1disp *p,			/* Object */
	int edgec[3],		/* Measurement edge count for each channel */
	double rgb[3]		/* Return the RGB clock count values */
) {
	int i;
	unsigned char ibuf[16];
	unsigned char obuf[16];
	int rsize;
	inst_code ev;

	/* Set the edge count */
	for (i = 0; i < 3; i++) {
		short2buf(ibuf, edgec[i]);	/* Edge count */
		ibuf[2] = (unsigned char)i;	/* Channel number */
		if ((ev = i1disp_command(p, i1d_setmedges2, ibuf, 3,
			         obuf, 8, &rsize, p->samptime + 1.0)) != inst_ok)
			return ev;
	}

	/* Do the measurement, and return the Red value */
	if ((ev = i1disp_command(p, i1d_m_rgb_edge_2, ibuf, 0,
		         obuf, 8, &rsize, 120.0)) != inst_ok)
		return ev;
	if (rsize != 5)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);
	rgb[0] = (double)buf2int(obuf);

	/* Get the green value */
	if ((ev = i1disp_command(p, i1d_rd_green, NULL, 0,
		         obuf, 8, &rsize, 0.5)) != inst_ok)
		return ev;
	if (rsize != 5)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);
	rgb[1] = (double)buf2int(obuf);

	/* Get the blue value */
	if ((ev = i1disp_command(p, i1d_rd_blue, NULL, 0,
		         obuf, 8, &rsize, 0.5)) != inst_ok)
		return ev;
	if (rsize != 5)
		return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);
	rgb[2] = (double)buf2int(obuf);

	return inst_ok;
}

#define ME 1		/* One edge initially (should this be 2 ?) */

/* Take a cooked measurement from the device for the i1d2 */
static inst_code
i1d2_take_measurement(
	i1disp *p,				/* Object */
	int crtm,				/* nz if crt mode */
	double rgb[3]			/* Return the rgb values */
) {
	int i;
	double rmeas[3];			/* Raw measurement */
	int edgec[3] = {ME,ME,ME};	/* Measurement edge count for each channel */
	int cdgec[3] = {ME,ME,ME};	/* CRT computed edge count for re-measure */
	int mask = 0x7;         /* Period measure mask */
	inst_code ev;

	if (p->inited == 0)
		return i1disp_interp_code((inst *)p, I1DISP_NOT_INITED);

	if (p->dtype == 0)
		return i1disp_interp_code((inst *)p, I1DISP_WRONG_DEVICE);

	DBG((dbgo, "i1d2_take_measurement called with crtm = %d\n",crtm));

	/* Do CRT frequency calibration if CRT, */
	/* and set integration time to default or CRT calibrated value. */
	if (p->itset == 0) {
		if ((ev = i1disp_do_fcal_setit(p)) != inst_ok)
			return ev;
	}

	/* For CRT mode, do an initial set of fixed integration time */
	/* frequency measurement */
	if (crtm) {

		DBG((dbgo,"Doing fixed period frequency measurement over %f secs\n",p->int_clocks/(double)p->clk_freq))

		if ((ev = i1d2_freq_measure(p, rmeas)) != inst_ok)
			return ev;

		for (i = 0; i < 3; i++)
			rgb[i] = 0.5 * rmeas[i] * p->rgbadj1[i]/(double)p->int_clocks;

		DBG((dbgo,"Got %f %f %f raw, %f %f %f Hz\n",
		rmeas[0], rmeas[1], rmeas[2], rgb[0], rgb[1], rgb[2]))

		/* Decide whether any channels need re-measuring, */
		/* and computed cooked values. Threshold is a count of 75 */
		mask = 0x0;
		for (i = 0; i < 3; i++) {
			if (rmeas[i] <= (0.75 * (double)p->sampno)) {
				mask |= (1 << i);		/* Yes */
				if (rmeas[i] >= 10) {	/* Compute target edges */
					cdgec[i] = (int)(2.0 * rgb[i] * p->int_clocks/(double)p->clk_freq + 0.5); 
					if (cdgec[i] > 2000)
						cdgec[i] = 2000;
					else if (cdgec[i] < ME)
						cdgec[i] = ME;
				}
			}
		}
		DBG((dbgo,"Re-measure mask = 0x%x\n",mask))
		DBG((dbgo,"cdgec = %d %d %d\n",cdgec[0],cdgec[1],cdgec[2]))
	}

	/* If any need re-measuring */
	if (mask != 0) {

		/* See if we need to compute a target edge count */
		for (i = 0; i < 3; i++) {
			if ((mask & (1 << i)) && cdgec[i] == ME)
				break;
		}

		/* Yes we do */
		if (i < 3) {

			DBG((dbgo,"Doing 1st period pre-measurement mask 0x%x, edgec %d %d %d\n",
			mask, edgec[0], edgec[1], edgec[2]))

			/* Do an initial measurement of 1 edge to estimate the */
			/* number of edges needed for the whole integration time. */
			if ((ev = i1disp_period_measure(p, edgec, rmeas)) != inst_ok)
				return ev;

			DBG((dbgo,"Got %f %f %f raw %f %f %f Hz\n",
			rmeas[0], rmeas[1], rmeas[2],
			(p->rgbadj2[0] * (double)edgec[0])/(rmeas[0] * 2.0 * p->clk_prd),
			(p->rgbadj2[1] * (double)edgec[1])/(rmeas[1] * 2.0 * p->clk_prd),
			(p->rgbadj2[2] * (double)edgec[2])/(rmeas[2] * 2.0 * p->clk_prd)))

			/* Compute adjusted edge count for channels we're remeasuring, */
			/* aiming for a values of int_clocks. */
			for (i = 0; i < 3; i++) {
				double ns;
				if ((mask & (1 << i)) == 0)
					continue;
				if (p->int_clocks > ((2000.0 - 0.5) * rmeas[i]))
					ns = 2000.0;			/* Maximum edge count */
				else {
					ns = floor(p->int_clocks/rmeas[i]) + 0.5;
					if (ns < ME)			/* Minimum edge count */
						ns = ME;
				}
				edgec[i] = (int)ns;
			}
		}

		/* Use frequency computed edge count if available */
		for (i = 0; i < 3; i++) {
			if ((mask & (1 << i)) == 0)
				continue;
			if (cdgec[i] != ME)
				edgec[i] = cdgec[i];
		}
	
		/* If we compute a different edge count, read again */
		if (edgec[0] > ME || edgec[1] > ME || edgec[2] > ME) {
			double rmeas2[3];		/* 2nd RGB Readings */
	
			DBG((dbgo,"Doing period re-measurement mask 0x%x, edgec %d %d %d\n",
			mask, edgec[0], edgec[1], edgec[2]))

			if ((ev = i1disp_period_measure(p, edgec, rmeas2)) != inst_ok)
				return ev;
	
			DBG((dbgo,"Got %f %f %f raw %f %f %f Hz\n",
			rmeas[0], rmeas[1], rmeas[2],
			(p->rgbadj2[0] * (double)edgec[0])/(rmeas2[0] * 2.0 * p->clk_prd),
			(p->rgbadj2[1] * (double)edgec[1])/(rmeas2[1] * 2.0 * p->clk_prd),
			(p->rgbadj2[2] * (double)edgec[2])/(rmeas2[2] * 2.0 * p->clk_prd)))

			DBG((dbgo,"Int period %f %f %f secs\n",
			rmeas2[0]/p->clk_freq, rmeas2[1]/p->clk_freq, rmeas2[2]/p->clk_freq))

			/* Average readings if we repeated a measurement with the same count */
			/* (Minor advantage, but may as well use it) */
			for (i = 0; i < 3; i++) {
				if (edgec[i] == ME)
					rmeas[i] = 0.5 * (rmeas[i] + rmeas2[i]);
				else
					rmeas[i] = rmeas2[i];
			}
		}

		/* Compute adjusted readings, ovewritting initial cooked values */
		for (i = 0; i < 3; i++) {
			if ((mask & (1 << i)) == 0)
				continue;
			rgb[i] = (p->rgbadj2[i] * (double)edgec[i])/(rmeas[i] * 2.0 * p->clk_prd);
			DBG((dbgo,"%d after scale = %f\n",i,rgb[i]))
	
			rgb[i] -= p->reg103_F[i];		/* Subtract black level */
			DBG((dbgo,"%d after sub black = %f\n",i,rgb[i]))
	
			if (rgb[i] < 0.0001)
				rgb[i] = 0.0001;
			DBG((dbgo,"%d after limit min = %f\n",i,rgb[i]))
		}
	}

	DBG((dbgo,"Cooked RGB Hz = %f %f %f\n",rgb[0],rgb[1],rgb[2]))
	
	return inst_ok;
}

#undef ME

/* . . . . . . . . . . . . . . . . . . . . . . . . */

/* Take a XYZ measurement from the device */
static inst_code
i1disp_take_XYZ_measurement(
	i1disp *p,				/* Object */
	double XYZ[3]			/* Return the XYZ values */
) {
	int i, j;
	double rgb[3];		/* RGB Readings */
	inst_code ev;
	double *mat;		/* Pointer to matrix */

	if (p->dtype == 0) {		/* i1 disp 1 */
		if ((ev = i1d1_take_measurement(p, 0, rgb)) != inst_ok)
			return ev;
	} else {				/* i1 disp 2 */
		int crtm = 0;		/* Assume LCD or ambient */

		if ((p->mode & inst_mode_measurement_mask) != inst_mode_emis_ambient && p->crt) {
			crtm = 1; 
		}

		if ((ev = i1d2_take_measurement(p, crtm, rgb)) != inst_ok)
			return ev;
	}

	/* Multiply by calibration matrix to arrive at XYZ */
	if ((p->mode & inst_mode_measurement_mask) == inst_mode_emis_ambient) {
		mat = p->amb;				/* Ambient matrix */
	} else {
		if (p->crt)
			mat = p->reg54_F;		/* CRT/factory matrix */
		else
			mat = p->reg4_F;		/* LCD/user matrix */
	}
	for (i = 0; i < 3; i++) {
		XYZ[i] = 0.0;
		for (j = 0; j < 3; j++) {
			XYZ[i] += mat[i * 3 + j] * rgb[j]; 
		}
		XYZ[i] *= CALFACTOR;		/* Times magic scale factor */

#ifdef NEVER
		if (p->chroma4)
			XYZ[i] *= 4.0/3.0;			/* (Not sure about this factor!) */
#endif
	}

	if ((p->mode & inst_mode_measurement_mask) != inst_mode_emis_ambient) {

		/* Apply the colorimeter correction matrix */
		icmMulBy3x3(XYZ, p->ccmat, XYZ);
	}

	DBG((dbgo,"returning XYZ = %f %f %f\n",XYZ[0],XYZ[1],XYZ[2]))
	return inst_ok;
}

/* Do a black calibration (i1display 1) */
static inst_code
i1disp_do_black_cal(
	i1disp *p				/* Object */
) {
	int i;
	double rgb1[3], rgb2[3];	/* RGB Readings */
	inst_code ev;

	if (p->dtype != 0)
		return i1disp_interp_code((inst *)p, I1DISP_CANT_BLACK_CALIB);

	/* Do a couple of readings without subtracting the black */
	if ((ev = i1d1_take_measurement(p, 1, rgb1)) != inst_ok)
		return ev;
	if ((ev = i1d1_take_measurement(p, 1, rgb2)) != inst_ok)
		return ev;

	/* Average the results */
	for (i = 0; i < 3; i++) {
		rgb1[i] = 0.5 * (rgb1[i] + rgb2[i]);

		/* Since the readings are clamped to 0.0001, */
		/* aim for a black level of 0.0001 */
		rgb1[i] -= 0.0001;
	}

	DBG((dbgo,"Black rgb = %f %f %f\n",rgb1[0],rgb1[1],rgb1[2]))

	/* Save it to the EEPROM */
	for (i = 0; i < 3; i++) {
		if ((ev = i1disp_wrreg_float(p, rgb1[i], 103 + 4 * i)) != inst_ok)
			return ev;
		p->reg103_F[i] = rgb1[i];
	}
	return inst_ok;
}

/* Do a refersh period cailbration if CRT, and set integration time in device */
static inst_code
i1disp_do_fcal_setit(
	i1disp *p				/* Object */
) {
	int i;
	inst_code ev;
	double measp = 0.0;

	DBG((dbgo,"Frequency calibration called\n"))

	if (p->dtype == 0)
		return i1disp_interp_code((inst *)p, I1DISP_CANT_MEASP_CALIB);

	if (p->crt && (p->mode & inst_mode_measurement_mask) != inst_mode_emis_ambient) {

		/* Average a few refresh period readings */
		for (i = 0; i < p->nmeasprds; i++) {
			int mp;
	
			if ((ev = i1disp_rd_meas_ref_period(p, &mp)) != inst_ok)
				return ev;
			if (mp == 0)
				break;			/* Too dark to measure */
			measp += (double)mp;
		}

		/* Compute the measurement frequency */
		if (measp != 0.0) {
			measp /= (double)p->nmeasprds;
			measp *= p->clk_prd;		/* Multiply by master clock period to get seconds */
			p->sampfreq = 1.0/measp;	/* Measurement sample frequency */
			DBG((dbgo,"Sample frequency measured = %f\n",p->sampfreq))
		} else {
			p->sampfreq = 60.0;	/* Integration time = 1.67 seconds */
		}
	} else {
		p->sampfreq = 100.0;	/* Make integration clocks = 1e6 = 1.0 seconds, same as default */
	}

	/* Compute actual sampling time */
	p->samptime = p->sampno / p->sampfreq;
	DBG((dbgo,"Computed sample time = %f seconds\n",p->samptime))

	/* Compute the integration period in clocks rounded to sample frequency */
	p->int_clocks = (int)floor((double)p->sampno/((double)p->reg40_S * 1e-9 * p->sampfreq) + 0.5);

	DBG((dbgo,"Setting integration time to = %d clocks\n",p->int_clocks))
	if ((ev = i1disp_wr_int_time(p, p->int_clocks)) != inst_ok)
		return ev;

	/* Read the integration time (could it be limited by instrument?) */
	if ((ev = i1disp_rd_int_time(p, &p->int_clocks) ) != inst_ok)
		return ev;
	DBG((dbgo,"Actual integration time = %d clocks = %f sec\n",p->int_clocks, p->int_clocks/p->clk_freq))

	p->itset = 1;
	return inst_ok;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Check the device is responding, and unlock if necessary */
static inst_code
i1disp_check_unlock(
	i1disp *p				/* Object */
) {
	unsigned char buf[16];
	int rsize;
	inst_code ev;
	int i, vv;
	double ver;

	struct {
		unsigned char code[4];
		int *flag;
	} codes[] = {
		{ { 'G','r','M','b' }, NULL },			/* "GrMb" i1 Display */
		{ { 'L','i','t','e' }, &p->lite },		/* "Lite" i1 Display LT */
		{ { 'M','u','n','k' }, &p->munki },		/* "Munk" ColorMunki Create */
		{ { 'O','b','i','W' }, &p->hpdream },	/* "ObiW" HP DreamColor */
		{ { 'O','b','i','w' }, &p->hpdream },	/* "Obiw" HP DreamColor */
		{ { 'C','M','X','2' }, &p->calmanx2 },	/* "CMX2" Calman X2 */
		{ { 'R','G','B','c' }, NULL },			/* */
		{ { 'C','E','C','5' }, NULL },			/* */
		{ { 'C','M','C','5' }, NULL },			/* */
		{ { 'C','M','G','5' }, NULL },			/* */
		{ { 0x00,0x00,0x01,0x00 }, NULL },		/* */
		{ { 0x09,0x0b,0x0c,0x0d }, NULL },		/* */
		{ { 0x0e,0x0e,0x0e,0x0e }, NULL },		/* */
		{ { 0x11,0x02,0xde,0xf0 }, NULL },		/* Barco Chroma 5 ? */
		{ { ' ',' ',' ',' ' }, (int *)-1 }
	}; 

	if (p->debug) fprintf(dbgo,"i1disp: about to check response and unlock instrument if needed\n");

	/* Get status */
	if ((ev = i1disp_command_1(p, i1d_status, NULL, 0,
	                   buf, 8, &rsize, 0.5)) != inst_ok && (ev & inst_imask) != I1DISP_LOCKED)
		return ev;		/* An error other than being locked */

	/* Try and unlock it if it is locked */
	if ((ev & inst_imask) == I1DISP_LOCKED) {

		/* Reset any flags */
		for (i = 0; ;i++) {
			if (codes[i].flag == (int *)-1)
				break;
			if (codes[i].flag != NULL)
				*codes[i].flag = 0;
		}

		/* Try each code in turn */
		for (i = 0; ;i++) {
			if (codes[i].flag == (int *)-1)
				break;

			/* Try unlock code. Ignore I1DISP_NOT_READY status. */
			if (((ev = i1disp_command_1(p, i1d_unlock, codes[i].code, 4,
			         buf, 8, &rsize, 0.5)) & inst_mask) != inst_ok
			                 && (ev & inst_imask) != I1DISP_LOCKED)
				return ev;		/* Some other sort of failure */
	
			/* Get status */
			if ((ev = i1disp_command_1(p, i1d_status, NULL, 0,
			           buf, 8, &rsize, 0.5)) != inst_ok
			                 && (ev & inst_imask) != I1DISP_LOCKED)
				return ev;	/* An error other than being locked */

			if (ev == inst_ok) {	/* Correct code */
				if (codes[i].flag != NULL)
					*codes[i].flag = 1;
				break;
			}
		}
	}

	if (rsize != 5 || !isdigit(buf[0]) || buf[1] != '.'
	               || !isdigit(buf[2]) || !isdigit(buf[3])) {
		return i1disp_interp_code((inst *)p, I1DISP_BAD_STATUS);
	}

	buf[4] = '\000';
	ver = atof((char *)buf);
	DBG((dbgo,"Version string = %5.3f\n",ver))

	/* Read register 0x79 for the model identifier */
	if ((ev = i1disp_rdreg_byte(p, &vv, 121) ) != inst_ok) {
		return ev;
	}
	vv &= 0xff;

	DBG((dbgo,"Version character = 0x%02x = '%c'\n",vv,vv))

	/* Sequel Chroma 4 with vv == 0xff ? */
	/* Barco Chroma 5 with ver = 5.01 and vv = '5' */
	if (ver >= 4.0 && ver < 5.1
	 && (vv == 0xff || vv == 0x35)) {
		p->dtype = 0;			/* Sequel Chroma 4 ?? */
		p->chroma4 = 1;			/* Treat like an Eye-One Display 1 */
								/* !!! Not fully tested !!! */
	} else if (ver >= 5.1 && ver <= 5.3 && vv == 'L') {
		p->dtype = 0;			/* Eye-One Display 1 */
	} else if (ver >= 6.0 && ver <= 6.29 && vv == 'L') {
		p->dtype = 1;			/* Eye-One Display 2 */
	} else {
		/* Reject any version or model we don't know about */
		if (p->verb) {
			printf("Version string = %3.1f\n",ver);
			printf("ID character = 0x%02x = '%c'\n",vv,vv);
		}
		return i1disp_interp_code((inst *)p, I1DISP_UNKNOWN_VERS_ID);
	}

	if (p->debug) fprintf(dbgo,"i1disp: instrument is responding, unlocked, and right type\n");

	return inst_ok;
}

/* Read all the relevant register values */
static inst_code
i1disp_read_all_regs(
	i1disp *p				/* Object */
) {
	inst_code ev;
	int i;

	if (p->debug) fprintf(dbgo,"i1disp: about to read all the registers\n");

	/* Serial number */
	if ((ev = i1disp_rdreg_word(p, &p->reg0_W, 0) ) != inst_ok)
		return ev;
	DBG((dbgo,"serial number = %d\n",p->reg0_W))


	/* LCD/user calibration values */
	for (i = 0; i < 9; i++) {
		if ((ev = i1disp_rdreg_float(p, &p->reg4_F[i], 4 + 4 * i) ) != inst_ok)
			return ev;
	DBG((dbgo,"LCD/user cal[%d] = %f\n",i,p->reg4_F[i]))
	}
	/* LCD/user calibration time */
	if ((ev = i1disp_rdreg_word(p, &p->reg50_W, 50) ) != inst_ok)
		return ev;
	DBG((dbgo,"LCD/user calibration time = 0x%x = %s\n",p->reg50_W, ctime_32(&p->reg50_W)))

	/* LCD/user calibration flag */
	if ((ev = i1disp_rdreg_short(p, &p->reg126_S, 126) ) != inst_ok)
		return ev;
	DBG((dbgo,"user cal flag = 0x%x\n",p->reg126_S))


	/* CRT/factory calibration values */
	for (i = 0; i < 9; i++) {
		if ((ev = i1disp_rdreg_float(p, &p->reg54_F[i], 54 + 4 * i) ) != inst_ok)
			return ev;
		DBG((dbgo,"CRT/factory cal[%d] = %f\n",i,p->reg54_F[i]))
	}
	/* CRT/factory calibration flag */
	if ((ev = i1disp_rdreg_word(p, &p->reg90_W, 90) ) != inst_ok)
		return ev;
	DBG((dbgo,"CRT/factory flag = 0x%x = %s\n",p->reg90_W, ctime_32(&p->reg90_W)))


	/* Calibration factor */
	if ((ev = i1disp_rdreg_short(p, &p->reg40_S, 40) ) != inst_ok)
		return ev;
	DBG((dbgo,"Reg40 = %d\n",p->reg40_S))

	/* Calibration factor */
	if ((ev = i1disp_rdreg_short(p, &p->reg42_S, 42) ) != inst_ok)
		return ev;
	DBG((dbgo,"Reg42 = %d\n",p->reg42_S))

	/* Calibration factors */
	for (i = 0; i < 3; i++) {
		if ((ev = i1disp_rdreg_short(p, &p->reg44_S[i], 44 + 2 * i) ) != inst_ok)
			return ev;
		DBG((dbgo,"reg44[%d] = %d\n",i,p->reg44_S[i]))
	}


	/* Overall reading scale value ?? */
	if ((ev = i1disp_rdreg_float(p, &p->clk_prd, 94) ) != inst_ok)
		return ev;
	DBG((dbgo,"Clock Period = %e\n",p->clk_prd))

	/* unknown */
	if ((ev = i1disp_rdreg_word(p, &p->reg98_W, 98) ) != inst_ok)
		return ev;
	DBG((dbgo,"reg98 = 0x%x = %s\n",p->reg98_W,ctime_32(&p->reg98_W)))

	/* unknown */
	if ((ev = i1disp_rdreg_byte(p, &p->reg102_B, 102) ) != inst_ok)
		return ev;
	DBG((dbgo,"reg102 = 0x%x\n",p->reg102_B))

	/* Dark current calibration values */
	/* Should we set to a default 0.0 if reg126_S < 0xd ?? */
	for (i = 0; i < 3; i++) {
		if ((ev = i1disp_rdreg_float(p, &p->reg103_F[i], 103 + 4 * i)) != inst_ok) {
			if ((ev & inst_imask) != I1DISP_FLOAT_NOT_SET)
				return ev;
			p->reg103_F[i] = 0.0;
		}
		DBG((dbgo,"darkcal[%d] = %f\n",i,p->reg103_F[i]))
	}

	/* Unknown byte */
	if ((ev = i1disp_rdreg_byte(p, &p->reg115_B, 115) ) != inst_ok)
		return ev;
	DBG((dbgo,"Unknown 115 byte = 0x%x\n",p->reg115_B))

	/* device ID byte */
	if ((ev = i1disp_rdreg_byte(p, &p->reg121_B, 121) ) != inst_ok)
		return ev;
	DBG((dbgo,"device type byte = 0x%x\n",p->reg121_B))

	/* Unlock string */
	for (i = 0; i < 4; i++) {
		int vv;
		if ((ev = i1disp_rdreg_byte(p, &vv, 122 + i) ) != inst_ok)
			return ev;
		p->reg122_B[i] = (char)vv;
	}
	p->reg122_B[i] = '\000';
	DBG((dbgo,"unlock string = '%s'\n",p->reg122_B))

	/* Read extra registers */
	if (p->dtype == 1) {

#ifdef NEVER	/* Not used, so don't bother */
		for (i = 0; i < 3; i++) {
			if ((ev = i1disp_rdreg_float(p, &p->reg128_F[i], 128 + 4 * i) ) != inst_ok)
				return ev;
			DBG((dbgo,"reg128_F[%d] = %f\n",i,p->reg128_F[i]))
		}
#endif /* NEVER */

		for (i = 0; i < 3; i++) {
			if ((ev = i1disp_rdreg_float(p, &p->reg144_F[i], 144 + 4 * i) ) != inst_ok) {
				if ((ev & inst_imask) != I1DISP_FLOAT_NOT_SET)
					return ev;
				p->reg144_F[i] = 1.0;
			}
			DBG((dbgo,"Ambient scale factor [%d] = %f\n",i,p->reg144_F[i]))
		}

		/* Read the integration time */
		if ((ev = i1disp_rd_int_time(p, &p->int_clocks) ) != inst_ok)
			return ev;
		DBG((dbgo,"Integration time = %d\n",p->int_clocks))
	}

	if (p->debug) fprintf(dbgo,"i1disp: all registers read OK\n");

	return inst_ok;
}

/* Compute factors that depend on the register values */
static inst_code
i1disp_compute_factors(
	i1disp *p				/* Object */
) {
	int i;

	/* Check that certain value are valid */
	if (p->reg0_W == 0xffffffff)
		return i1disp_interp_code((inst *)p, I1DISP_BAD_SERIAL_NUMBER);

	/* LCD calibration date valid ? */
	if (p->reg50_W == 0xffffffff)
		return i1disp_interp_code((inst *)p, I1DISP_BAD_LCD_CALIBRATION);

	/* The value stored in reg126_S ("user cal flag") seems hard to interpret. */
	/* For the i1display 1&2, it has a value of 0xd. */
	/* Value 0x7 seems to be for a "user calibration" */
	/* Values 3 & 6 seem to always "errors" as does a value */
	/* < 7 in most circumstances. But the Heidelberg Viewmaker */
	/* (from Sequel Imaging) and Lacie Blue Eye colorimeter seems to have a value of 2. */
	/* The Barco sensor seems to have a value of 0x20 */
	if (p->reg126_S == 0xffffffff || (p->reg126_S < 7 && p->reg126_S != 2))
		return i1disp_interp_code((inst *)p, I1DISP_BAD_LCD_CALIBRATION);

	/* Not quite sure about this, but we're assuming this */
	/* is set to 2 or 0xd if reg4-36 hold the LCD calibration, */
	/* and some other number if they are not set, or set */
	/* to a custom user calibration. */
	if (p->reg126_S != 0xd && p->reg126_S != 2 && p->reg126_S != 0x20)
		return i1disp_interp_code((inst *)p, I1DISP_BAD_LCD_CALIBRATION);
		
	if (p->reg90_W == 0xffffffff)
		return i1disp_interp_code((inst *)p, I1DISP_BAD_CRT_CALIBRATION);

	/* Compute ambient matrix */
	for (i = 0; i < 9; i++)
		p->amb[i] = p->reg144_F[i % 3] * 0.5 * (p->reg4_F[i] + p->reg54_F[i]);

	/* clk_prd inversion */
	p->clk_freq = 1.0/p->clk_prd;
	DBG((dbgo,"clk_freq = %f\n",p->clk_freq))
	
	/* RGB channel scale factors */
	for (i = 0; i < 3; i++) {
		double tt;
		tt = (double)p->reg44_S[i] * 1e11/((double)p->reg40_S * (double)p->reg42_S);
		p->rgbadj1[i] = floor(tt + 0.5);
		DBG((dbgo,"reg44+%dcalc = %f\n",i,p->rgbadj1[i]))
		p->rgbadj2[i] = tt * 1e-9 * (double)p->reg40_S;
		DBG((dbgo,"reg44+%dcalc2 = %f\n",i,p->rgbadj2[i]))
	}

	/* Set some defaults */
	p->sampfreq = 60.0;		/* Display refresh rate/sample frequency */
	p->sampno = 100;		/* Minimum sampling count. Set target integration time. */
	p->nmeasprds = 5;		/* Number of disp refresh period measurments to average */ 
							/* in doing frequency calibration */

	return inst_ok;
}

/* ------------------------------------------------------------------------ */

/* Establish communications with a I1DISP */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
i1disp_init_coms(inst *pp, int port, baud_rate br, flow_control fc, double tout) {
	i1disp *p = (i1disp *) pp;
	unsigned char buf[16];
	int rsize;
	inst_code ev = inst_ok;

	if (p->debug) {
		p->icom->debug = p->debug;	/* Turn on debugging */
		fprintf(dbgo,"i1disp: About to init coms\n");
	}

	if (p->icom->is_usb_portno(p->icom, port) == instUnknown) {
		if (p->debug) fprintf(dbgo,"i1disp: init_coms called to wrong device!\n");
			return i1disp_interp_code((inst *)p, I1DISP_UNKNOWN_MODEL);
	}

	if (p->debug) fprintf(dbgo,"i1disp: About to init USB\n");

	/* Set config, interface, write end point, read end point */
	/* ("serial" end points aren't used - the i1display uses USB control messages) */
	p->icom->set_usb_port(p->icom, port, 1, 0x00, 0x00, icomuf_none, 0, NULL); 

	/* Check instrument is responding */
	if ((ev = i1disp_command_1(p, i1d_status, NULL, 0, buf, 8, &rsize, 0.5)) != inst_ok
	                                            && (ev & inst_imask) != I1DISP_LOCKED) {
		if (p->debug) fprintf(dbgo,"i1disp: init coms failed with rv = 0x%x\n",ev);
		return ev;
	}

	if (p->debug) fprintf(dbgo,"i1disp: init coms has suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

/* Initialise the I1DISP */
/* return non-zero on an error, with dtp error code */
static inst_code
i1disp_init_inst(inst *pp) {
	i1disp *p = (i1disp *)pp;
	inst_code ev = inst_ok;

	if (p->debug) fprintf(dbgo,"i1disp: About to init instrument\n");

	if (p->gotcoms == 0)
		return i1disp_interp_code((inst *)p, I1DISP_NO_COMS);	/* Must establish coms first */

	/* Check instrument is responding, and right type */
	if ((ev = i1disp_check_unlock(p)) != inst_ok)
		return ev;

	/* Read all the registers and store their contents */
	if ((ev = i1disp_read_all_regs(p)) != inst_ok)
		return ev;

	if ((ev = i1disp_compute_factors(p)) != inst_ok)
		return ev;

	p->trig = inst_opt_trig_keyb;

	if (ev == inst_ok) {
		p->inited = 1;
		if (p->debug) fprintf(dbgo,"i1disp: instrument inited OK\n");
	}

	return ev;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
i1disp_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val) {		/* Pointer to instrument patch value */
	i1disp *p = (i1disp *)pp;
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
			return i1disp_interp_code((inst *)p, icoms2i1disp_err(se));
		}
		user_trig = 1;
		if (p->trig_return)
			printf("\n");
	}

	/* Read the XYZ value */
	if ((rv = i1disp_take_XYZ_measurement(p, val->aXYZ)) != inst_ok)
		return rv;

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
inst_code i1disp_col_cor_mat(
inst *pp,
double mtx[3][3]
) {
	i1disp *p = (i1disp *)pp;

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
inst_cal_type i1disp_needs_calibration(inst *pp) {
	i1disp *p = (i1disp *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->dtype == 1 && p->crt != 0 && p->itset == 0)
		return inst_calt_crt_freq;
	return inst_ok;
}

/* Request an instrument calibration. */
/* This is use if the user decides they want to do a calibration, */
/* in anticipation of a calibration (needs_calibration()) to avoid */
/* requiring one during measurement, or in response to measuring */
/* returning inst_needs_cal. Initially us an inst_cal_cond of inst_calc_none, */
/* and then be prepared to setup the right conditions, or ask the */
/* user to do so, each time the error inst_cal_setup is returned. */
inst_code i1disp_calibrate(
inst *pp,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	i1disp *p = (i1disp *)pp;
	int rv = 0;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	id[0] = '\000';

	/* Translate default into what's needed or expected default */
	if (calt == inst_calt_all) {
		if (p->dtype == 1 && p->crt != 0)
			calt = inst_calt_crt_freq;
		else if (p->dtype == 0)
			calt = inst_calt_disp_offset;
	}

	/* Do the appropriate calibration, or return "unsupported" */
	if (p->dtype == 0) {		/* Eye-One Display 1 */
		if (calt == inst_calt_disp_offset) {

			if (*calc != inst_calt_disp_offset) {
				*calc = inst_calt_disp_offset;
				return inst_cal_setup;
			}

			/* Do black offset calibration */
			if ((rv = i1disp_do_black_cal(p)) != inst_ok)
				return rv;

			return inst_ok;
		}
	} else {				/* Eye-One Display 2 */
		if (calt == inst_calt_crt_freq && p->crt != 0) {

			if (*calc != inst_calc_disp_white) {
				*calc = inst_calc_disp_white;
				return inst_cal_setup;
			}

			/* Do CRT frequency calibration and set integration time */
			if ((rv = i1disp_do_fcal_setit(p)) != inst_ok)
				return rv;

			return inst_ok;
		}
	}

	return inst_unsupported;
}

/* Error codes interpretation */
static char *
i1disp_interp_error(inst *pp, int ec) {
//	i1disp *p = (i1disp *)pp;
	ec &= inst_imask;
	switch (ec) {
		case I1DISP_INTERNAL_ERROR:
			return "Internal software error";
		case I1DISP_COMS_FAIL:
			return "Communications failure";
		case I1DISP_UNKNOWN_MODEL:
			return "Not a i1 Display";
		case I1DISP_DATA_PARSE_ERROR:
			return "Data from i1 Display didn't parse as expected";
		case I1DISP_USER_ABORT:
			return "User hit Abort key";
		case I1DISP_USER_TERM:
			return "User hit Terminate key";
		case I1DISP_USER_TRIG:
			return "User hit Trigger key";
		case I1DISP_USER_CMND:
			return "User hit a Command key";

		case I1DISP_OK:
			return "No device error";

		case I1DISP_FLOAT_NOT_SET:
			return "Float value is not set in EEPROM";
		case I1DISP_NOT_READY:
			return "Command didn't return command code - not ready ?";

		case I1DISP_BAD_SERIAL_NUMBER:
			return "Serial number isn't set";
		case I1DISP_BAD_LCD_CALIBRATION:
			return "LCD calibration values aren't set";
		case I1DISP_BAD_CRT_CALIBRATION:
			return "CRT calibration values aren't set";
		case I1DISP_EEPROM_WRITE_FAIL:
			return "Write to EEPROM failed to verify";

		case I1DISP_UNEXPECTED_RET_SIZE:
			return "Message from instrument has unexpected size";
		case I1DISP_UNEXPECTED_RET_VAL:
			return "Message from instrument has unexpected value";

		case I1DISP_BAD_STATUS:
			return "Instrument status is unrecognised format";
		case I1DISP_UNKNOWN_VERS_ID:
			return "Instrument version number or ID byte not recognised";

		/* Internal errors */
		case I1DISP_BAD_REG_ADDRESS:
			return "Out of range register address";
		case I1DISP_BAD_INT_THRESH:
			return "Out of range integration threshold";
		case I1DISP_NO_COMS:
			return "Communications hasn't been established";
		case I1DISP_NOT_INITED:
			return "Insrument hasn't been initialised";
		case I1DISP_CANT_BLACK_CALIB:
			return "Device doesn't support black calibration";
		case I1DISP_CANT_MEASP_CALIB:
			return "Device doesn't support measurment period calibration";
		case I1DISP_WRONG_DEVICE:
			return "Wrong type of device for called function";
		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
i1disp_interp_code(inst *pp, int ec) {
//	i1disp *p = (i1disp *)pp;

	ec &= inst_imask;
	switch (ec) {

		case I1DISP_OK:
		case I1DISP_FLOAT_NOT_SET:		/* Internal indication */
		case I1DISP_NOT_READY:			/* Internal indication */
			return inst_ok;

		case I1DISP_INTERNAL_ERROR:
		case I1DISP_BAD_REG_ADDRESS:
		case I1DISP_BAD_INT_THRESH:
		case I1DISP_NO_COMS:
		case I1DISP_NOT_INITED:
		case I1DISP_CANT_BLACK_CALIB:
		case I1DISP_CANT_MEASP_CALIB:
		case I1DISP_WRONG_DEVICE:
		case I1DISP_LOCKED:
			return inst_internal_error | ec;

		case I1DISP_COMS_FAIL:
			return inst_coms_fail | ec;

		case I1DISP_UNKNOWN_MODEL:
		case I1DISP_BAD_STATUS:
		case I1DISP_UNKNOWN_VERS_ID:
			return inst_unknown_model | ec;

		case I1DISP_DATA_PARSE_ERROR:
		case I1DISP_UNEXPECTED_RET_SIZE:
		case I1DISP_UNEXPECTED_RET_VAL:
			return inst_protocol_error | ec;

		case I1DISP_USER_ABORT:
			return inst_user_abort | ec;
		case I1DISP_USER_TERM:
			return inst_user_term | ec;
		case I1DISP_USER_TRIG:
			return inst_user_trig | ec;
		case I1DISP_USER_CMND:
			return inst_user_cmnd | ec;

		case I1DISP_BAD_SERIAL_NUMBER:
		case I1DISP_BAD_LCD_CALIBRATION:
		case I1DISP_BAD_CRT_CALIBRATION:
		case I1DISP_EEPROM_WRITE_FAIL:
			return inst_hardware_fail | ec;

		/* return inst_misread | ec; */
		/* return inst_needs_cal_2 | ec; */
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
i1disp_del(inst *pp) {
	i1disp *p = (i1disp *)pp;
	if (p->icom != NULL)
		p->icom->del(p->icom);
	free(p);
}

/* Return the instrument capabilities */
inst_capability i1disp_capabilities(inst *pp) {
	i1disp *p = (i1disp *)pp;
	inst_capability rv;

	rv = inst_emis_spot
	   | inst_emis_disp
	   | inst_emis_disptype
	   | inst_emis_disptypem
	   | inst_colorimeter
	   | inst_ccmx
	     ;

	/* Eye-One Display 2 has ambient capability */
	if (p->dtype == 1)
	   rv |= inst_emis_ambient;

	return rv;
}

/* Return the instrument capabilities 2 */
inst2_capability i1disp_capabilities2(inst *pp) {
	i1disp *p = (i1disp *)pp;
	inst2_capability rv;

	if (p->dtype == 0) {	/* Eye-One Display 1 */
	   rv = inst2_cal_disp_offset;
	} else {				/* Eye-One Display 2 */
	   rv = inst2_cal_crt_freq;
	}
	rv |= inst2_prog_trig;
	rv |= inst2_keyb_trig;

	return rv;
}

inst_disptypesel i1disp_disptypesel[3] = {
	{
		1,
		"c",
		"i1Disp: CRT display",
		1
	},
	{
		2,
		"l",
		"i1Disp: LCD display",
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
static inst_code i1disp_get_opt_details(
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
		*psels = i1disp_disptypesel;
		
		return inst_ok;
	}

	return inst_unsupported;
}

/* Set device measurement mode */
inst_code i1disp_set_mode(inst *pp, inst_mode m)
{
	i1disp *p = (i1disp *)pp;
	inst_mode mm;		/* Measurement mode */

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* The measurement mode portion of the mode */
	mm = m & inst_mode_measurement_mask;

	/* only display emission mode supported */
	if (mm != inst_mode_emis_spot
	 && mm != inst_mode_emis_disp
	 && (p->dtype != 1 || mm != inst_mode_emis_ambient)) {
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
i1disp_set_opt_mode(inst *pp, inst_opt_mode m, ...)
{
	i1disp *p = (i1disp *)pp;

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
				p->itset = 0;		/* This is a hint we may have swapped displays */
			p->crt = 1;
			return inst_ok;
		} else if (ix == 2) {
			if (p->crt != 0)
				p->itset = 0;		/* This is a hint we may have swapped displays */
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

	return inst_unsupported;
}

/* Constructor */
extern i1disp *new_i1disp(icoms *icom, instType itype, int debug, int verb)
{
	i1disp *p;
	if ((p = (i1disp *)calloc(sizeof(i1disp),1)) == NULL)
		error("i1disp: malloc failed!");

	if (icom == NULL)
		p->icom = new_icoms();
	else
		p->icom = icom;

	p->debug = debug;
	p->verb = verb;

	icmSetUnity3x3(p->ccmat);	/* Set the colorimeter correction matrix to do nothing */

	p->init_coms         = i1disp_init_coms;
	p->init_inst         = i1disp_init_inst;
	p->capabilities      = i1disp_capabilities;
	p->capabilities2     = i1disp_capabilities2;
	p->get_opt_details   = i1disp_get_opt_details;
	p->set_mode          = i1disp_set_mode;
	p->set_opt_mode      = i1disp_set_opt_mode;
	p->read_sample       = i1disp_read_sample;
	p->needs_calibration = i1disp_needs_calibration;
	p->calibrate         = i1disp_calibrate;
	p->col_cor_mat       = i1disp_col_cor_mat;
	p->interp_error      = i1disp_interp_error;
	p->del               = i1disp_del;

	p->itype = itype;

	if (p->itype == instI1Disp2)
		p->dtype = 1;			/* i1Display2 */

	return p;
}

