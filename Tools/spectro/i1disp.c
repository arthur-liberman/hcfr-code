/* 
 * Argyll Color Correction System
 *
 * Gretag i1Display related functions
 *
 * Author: Graeme W. Gill
 * Date:   18/10/2006
 *
 * Copyright 2006 - 2013, Graeme W. Gill
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
#include "conv.h"
#include "icoms.h"
#include "i1disp.h"

static void dump_bytes(a1log *log, char *pfx, unsigned char *buf, int base, int len);
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
	if (se != ICOM_OK)
		return I1DISP_COMS_FAIL;
	return I1DISP_OK;
}

/* i1Display command codes - number:X is argument count:return type */
/* B = byte (8bit), S = short (16bit), W = word (32bit), */
/* A = string (5 bytes total max) , - = none */
typedef enum {
    i1d_status       = 0x00,		/* -:A    Get status string */
    i1d_rd_red       = 0x01,		/* -:W    Read the red channel clk count (and trig ?) */
    i1d_rd_green     = 0x02,		/* -:W    Read the green channel clk count */
    i1d_rd_blue      = 0x03,		/* -:W    Read the blue channel clk count */
    i1d_getmeas_p    = 0x04,		/* -:W    Read the measure refresh period */
    i1d_setintgt     = 0x05,		/* W:-    Set the integration time */
    i1d_getintgt     = 0x06,		/* -:W    Get the integration time */
    i1d_wrreg        = 0x07,		/* BB:-   Write a register value */
    i1d_rdreg        = 0x08,		/* B:B    Read a register value */
    i1d_getmeas_p2   = 0x09,		/* -:W    Read the measure refresh period (finer ?) */
    i1d_m_red_p      = 0x0a,		/* B:W    Measure the red period for given edge count */
    i1d_m_green_p    = 0x0b,		/* B:W    Measure the green period for given edge count */
    i1d_m_blue_p     = 0x0c,		/* B:W    Measure the blue period for given edge count */
    i1d_m_rgb_p      = 0x0d,		/* BBB:W  Measure the RGB period for given edge count */
    i1d_unlock       = 0x0e,		/* BBBB:- Unlock the interface */

    i1d_m_red_p2     = 0x10,		/* S:W    Measure the red period (16 bit) */
    i1d_m_green_p2   = 0x11,		/* S:W    Measure the green period (16 bit) */
    i1d_m_blue_p2    = 0x12,		/* S:W    Measure the blue period (16 bit) */
									/* S = edge count */

    i1d_m_red_2      = 0x13,		/* B:W    Measure the red channel (16 bit) */
									/* B = sync mode, typically 1 */

    i1d_setmedges2   = 0x14,		/* SB:-   Set number of edges used for measurment 16 bit */
									/* B = channel */
    i1d_getmedges2   = 0x15,		/* B:S    Get number of edges used for measurment 16 bit */
									/* B = channel */

    i1d_m_rgb_edge_2 = 0x16,		/* -:W    Measure RGB Edge (16 bit) */

    i1d_set_pll_p    = 0x11,		/* SS:-   Set PLL period */
    i1d_get_pll_p    = 0x12,		/* -:W    Get PLL period */

    i1d_m_rgb_edge_3 = 0x10,		/* BBBB:W Measure RGB Edge & return red. */
									/* BBBB = edge counts ??? */
    i1d_g_green_3    = 0x11,		/* -:W    Get green data */
    i1d_g_blue_3     = 0x12,		/* -:W    Get blue data */

    i1d_wrxreg       = 0x13,		/* SB:-   Write an extra register value */
    i1d_rdxreg       = 0x14, 		/* S:B    Read an extra register value */

    i1d_rdexreg      = 0x19 		/* BS:B   Smile: Read an extended register value */
									/*        The address range overlapps i1d_rdreg */
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

	tsize = insize + 2;
	*rsize = 0;

	a1logd(p->log, 4, "i1disp: Sending cmd %02x args '%s'\n",cc, icoms_tohex(in, insize));

	/* For each byte to be sent */
	for (i = 0; i < tsize; i++) {
		unsigned int smsec;

		/* Control message to read 8 bytes */
		requesttype = IUSB_ENDPOINT_IN | IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_ENDPOINT;
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
			a1logd(p->log, 1, "i1disp: Message send failed with ICOM err 0x%x\n",se);
			p->last_com_err = se;
			return i1disp_interp_code((inst *)p, I1DISP_COMS_FAIL);
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

	a1logd(p->log, 4, "i1disp: response '%s' ICOM err 0x%x\n",icoms_tohex(out, *rsize),ua);

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
	int addr				/* Register Address, 0 - 127, 159 */
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

/* ColorMunki Smile: Read a byte from an extended register range */
static inst_code
i1disp_rdexreg_bytes(
	i1disp *p,				/* Object */
	unsigned char *outp,	/* Where to write values */
	int addr,				/* Register Address, 16 bit */
	int len					/* Number of bytes, 8 bits */
) {
	unsigned char ibuf[16];
	unsigned char obuf[16];
	int rsize;
	inst_code ev;

	if (p->dtype != 2)		/* Only ColorMunki Smile ? */
		return i1disp_interp_code((inst *)p, I1DISP_WRONG_DEVICE);

	if (addr < 0 || addr > 0x0200) 
		return i1disp_interp_code((inst *)p, I1DISP_BAD_REG_ADDRESS);

	if (len < 0 || (addr + len) > 0x0200)
		return i1disp_interp_code((inst *)p, I1DISP_BAD_REG_ADDRESS);

	for (; len > 0; ) {
		int rlen = len;
		if (rlen > 4)
			rlen = 4;

		/* Read up to 4 bytes at a time */
		short2buf(ibuf+0, addr);	/* Address to read from */
		ibuf[2] = rlen;				/* Number of bytes to read */
		if ((ev = i1disp_command(p, i1d_rdexreg, ibuf, 3,
			                        obuf, 16, &rsize, 0.5)) != inst_ok)
			return ev;
	
		if ((rsize < 4 && (rsize != (2 + rlen)))
		 || (rsize >= 4 && (rsize != (1 + rlen))))
			return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_SIZE);
	
		if (obuf[0] != rlen)			/* Number of bytes returned */
			return i1disp_interp_code((inst *)p, I1DISP_UNEXPECTED_RET_VAL);

		memcpy(outp + addr, obuf + 1, rlen); 
		addr += rlen;
		len -= rlen;
	}
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

	if ((ev = i1disp_command(p, i1d_getmeas_p, NULL, 0,
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

	a1logd(p->log, 3, "Initial RGB = %f %f %f\n",rgb[0],rgb[1],rgb[2]);

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

	a1logd(p->log, 3, "scaled %d %d %d gives RGB = %f %f %f\n", edgec[0],edgec[1],edgec[2], rgb[0],rgb[1],rgb[2]);

	/* Compute adjusted readings as a frequency. */
	/* We need to divide the number of edges/2 by the period in seconds */
	for (i = 0; i < 3; i++) {
		rgb[i] = (p->rgbadj[i] * 0.5 * (double)edgec[i] * p->clk_freq)/rgb[i];
		a1logd(p->log, 3, "%d sensor frequency = %f\n",i,rgb[i]);

		/* If we're not calibrating the black */
		if (cal == 0) {
			rgb[i] -= p->reg103_F[i];		/* Subtract black level */
			a1logd(p->log, 3, "%d after sub black = %f\n",i,rgb[i]);
	
			if (rgb[i] < 0.0001)
				rgb[i] = 0.0001;
			a1logd(p->log, 3, "%d after limit min = %f\n",i,rgb[i]);
		}
	}
	a1logd(p->log, 3, "Adjusted RGB = %f %f %f\n",rgb[0],rgb[1],rgb[2]);

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
	double *inttime,		/* Integration time in seconds. (Return clock rounded) */
	double rgb[3]			/* Return the RGB edge count values */
) {
	unsigned char ibuf[16];
	unsigned char obuf[16];
	int intclks;
	int rsize;
	inst_code ev;

	if (*inttime > 20.0)		/* Hmm */
		*inttime = 20.0;

	intclks = (int)(*inttime * p->iclk_freq + 0.5);
	*inttime = (double)intclks / p->iclk_freq;
	if (intclks != p->int_clocks) {
		if ((ev = i1disp_wr_int_time(p, intclks)) != inst_ok)
			return ev;
		if ((ev = i1disp_rd_int_time(p, &intclks) ) != inst_ok)
			return ev;
		p->int_clocks = intclks;
		*inttime = (double)p->int_clocks/p->iclk_freq;
	}

	/* Do the measurement, and return the Red value */
	ibuf[0] = 1;		/* Sync mode 1 */
	if ((ev = i1disp_command(p, i1d_m_red_2, ibuf, 1,
		         obuf, 8, &rsize, p->inttime + 1.0)) != inst_ok)
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

/* Take a raw measurement that returns the number of clocks */
/* between and initial edge and edgec[] subsequent edges of the L2F. */
/* Both edges are counted. */ 
static inst_code
i1d2_period_measure(
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
			         obuf, 8, &rsize, 1.0)) != inst_ok)
			return ev;
	}

	/* Do the measurement, and return the Red value */
	if ((ev = i1disp_command(p, i1d_m_rgb_edge_2, ibuf, 0,
		         obuf, 8, &rsize, 120.0)) != inst_ok) {
		return ev;
	}
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

#ifndef NEVER

/* ### Quick and precise for low levels, but subject to long */
/* ### delays if the light level drops during measurement, and */
/* ### may be less accurate at low levels due to dark noise */

/* Take a cooked measurement from the device for the i1d2 */
static inst_code
i1d2_take_measurement(
	i1disp *p,				/* Object */
	int refreshm,			/* Measure in refresh mode flag */
	double rgb[3]			/* Return the rgb values */
) {
	int i;
	double rmeas[3];			/* Raw measurement */
	int edgec[3] = {ME,ME,ME};	/* Measurement edge count for each channel */
	int cdgec[3] = {ME,ME,ME};	/* CRT computed edge count for re-measure */
	int mask = 0x0;				/* Period measure mask */
	inst_code ev;

	if (p->inited == 0)
		return i1disp_interp_code((inst *)p, I1DISP_NOT_INITED);

	if (p->dtype == 0)
		return i1disp_interp_code((inst *)p, I1DISP_WRONG_DEVICE);

	a1logd(p->log, 3, "i1d2_take_measurement called with refreshm = %d\n",refreshm);

	/* Do refresh period measurement */
	if (p->dtype == 1 && refreshm && p->rrset == 0) {
		if ((ev = i1disp_do_fcal_setit(p)) != inst_ok)
			return ev;

		/* Quantize the sample time */
		if (p->refperiod > 0.0) {
			int n;
			n = (int)ceil(p->dinttime/p->refperiod);
			p->inttime = n * p->refperiod;
			a1logd(p->log, 3, "i1disp: integration time quantize to %f secs\n",p->inttime);
		} else {
			p->inttime = p->dinttime;	
			a1logd(p->log, 3, "i1disp: integration time set to %f secs\n",p->inttime);
		}
	}

// Do a frequency measurement first, to reduces chances of a light change causing delays.
// This makes LCD same as CRT as far as quantization errors in high level readings.
//	if (refreshm) {
		/* Do an initial fixed integration time frequency measurement. */
		a1logd(p->log, 3, "Doing fixed period frequency measurement over %f secs\n",p->inttime);
	
		if ((ev = i1d2_freq_measure(p, &p->inttime, rmeas)) != inst_ok)
			return ev;
	
		for (i = 0; i < 3; i++)
			rgb[i] = p->rgbadj[i] * 0.5 * rmeas[i]/p->inttime;
	
		a1logd(p->log, 3, "Got %f %f %f raw, %f %f %f Hz\n",
		rmeas[0], rmeas[1], rmeas[2], rgb[0], rgb[1], rgb[2]);
	
		/* Decide whether any channels need re-measuring, */
		/* and computed cooked values. Threshold is a count of 75 */
		for (i = 0; i < 3; i++) {
			if (rmeas[i] <= 75.0) {
				mask |= (1 << i);		/* Yes */
				if (rmeas[i] >= 10.0) {	/* Compute target edges */
					cdgec[i] = (int)(2.0 * rgb[i] * p->inttime + 0.5); 
					if (cdgec[i] > 2000)
						cdgec[i] = 2000;
					else if (cdgec[i] < ME)
						cdgec[i] = ME;
				}
			}
		}
//	} else {
//		mask = 0x7;
//	}
	a1logd(p->log, 3, "Re-measure mask = 0x%x\n",mask);
	a1logd(p->log, 3, "cdgec = %d %d %d\n",cdgec[0],cdgec[1],cdgec[2]);

	/* If any need re-measuring */
	if (mask != 0) {

		/* See if we need to compute a target edge count */
		for (i = 0; i < 3; i++) {
			if ((mask & (1 << i)) && cdgec[i] == ME)
				break;
		}

		/* Yes we do */
		if (i < 3) {

			a1logd(p->log, 3, "Doing 1st period pre-measurement mask 0x%x, edgec %d %d %d\n",
			mask, edgec[0], edgec[1], edgec[2]);

			/* Do an initial measurement of 1 edge to estimate the */
			/* number of edges needed for the whole integration time. */
			if ((ev = i1d2_period_measure(p, edgec, rmeas)) != inst_ok)
				return ev;

			a1logd(p->log, 3, "Got %f %f %f raw %f %f %f Hz\n",
			rmeas[0], rmeas[1], rmeas[2],
			(p->rgbadj[0] * 0.5 * (double)edgec[0] * p->clk_freq)/rmeas[0],
			(p->rgbadj[1] * 0.5 * (double)edgec[1] * p->clk_freq)/rmeas[1],
			(p->rgbadj[2] * 0.5 * (double)edgec[2] * p->clk_freq)/rmeas[2]);

			/* Compute adjusted edge count for channels we're remeasuring, */
			/* aiming for a values of int_clocks. */
			for (i = 0; i < 3; i++) {
				double ns;
				if ((mask & (1 << i)) == 0)
					continue;
				if (p->int_clocks > ((2000.0 - 0.5) * rmeas[i]))
					ns = 2000.0;			/* Maximum edge count */
				else {
					ns = floor(p->inttime * edgec[i] * p->clk_freq/rmeas[i] + 0.5);
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
	
			a1logd(p->log, 3, "Doing period re-measurement mask 0x%x, edgec %d %d %d\n",
			mask, edgec[0], edgec[1], edgec[2]);

			if ((ev = i1d2_period_measure(p, edgec, rmeas2)) != inst_ok)
				return ev;
	
			a1logd(p->log, 3, "Got %f %f %f raw %f %f %f Hz\n",
			rmeas[0], rmeas[1], rmeas[2],
			(p->rgbadj[0] * 0.5 * (double)edgec[0] * p->clk_freq)/rmeas2[0],
			(p->rgbadj[1] * 0.5 * (double)edgec[1] * p->clk_freq)/rmeas2[1],
			(p->rgbadj[2] * 0.5 * (double)edgec[2] * p->clk_freq)/rmeas2[2]);

			a1logd(p->log, 3, "Int period %f %f %f secs\n",
			rmeas2[0]/p->clk_freq, rmeas2[1]/p->clk_freq, rmeas2[2]/p->clk_freq);

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
			rgb[i] = (p->rgbadj[i] * 0.5 * (double)edgec[i] * p->clk_freq)/rmeas[i];
			a1logd(p->log, 3, "%d after scale = %f\n",i,rgb[i]);
	
			rgb[i] -= p->reg103_F[i];		/* Subtract black level */
			a1logd(p->log, 3, "%d after sub black = %f\n",i,rgb[i]);
	
			if (rgb[i] < 0.0001)
				rgb[i] = 0.0001;
			a1logd(p->log, 3, "%d after limit min = %f\n",i,rgb[i]);
		}
	}

	a1logd(p->log, 3, "Cooked RGB Hz = %f %f %f\n",rgb[0],rgb[1],rgb[2]);
	
	return inst_ok;
}

#else
/* Less precise (worse quatization errors), but more robust */
/* against excessive delays if the light level drops during measurement. */
/* Limits period measurement to an edge count < 35, but that can */
/* still take a long time in the dark. */

/* Take a cooked measurement from the device for the i1d2 */
static inst_code
i1d2_take_measurement(
	i1disp *p,				/* Object */
	int refreshm,				/* Measure in crt mode flag */
	double rgb[3]			/* Return the rgb values */
) {
	int i;
	double rmeas[3];			/* Raw measurement */
	int edgec[3] = {ME,ME,ME};	/* Measurement edge count for each channel */
	int cdgec[3] = {ME,ME,ME};	/* CRT computed edge count for re-measure */
	int fmask = 0x0;             /* Freq re-measure mask */
	int mask = 0x0;				/* Period measure mask */
	inst_code ev;

	if (p->inited == 0)
		return i1disp_interp_code((inst *)p, I1DISP_NOT_INITED);

	if (p->dtype == 0)
		return i1disp_interp_code((inst *)p, I1DISP_WRONG_DEVICE);

	a1logd(p->log, 3, "i1d2_take_measurement called with refreshm = %d\n",refreshm);

	/* Do refresh period measurement */
	if (p->dtype == 1 && refreshm && p->rrset == 0) {
		if ((ev = i1disp_do_fcal_setit(p)) != inst_ok)
			return ev;

		/* Quantize the sample time */
		if (p->refperiod > 0.0) {
			int n;
			n = (int)ceil(p->dinttime/p->refperiod);
			p->inttime = n * p->refperiod;
			a1logd(p->log, 3, "i1disp: integration time quantize to %f secs\n",p->inttime);
		} else {
			p->inttime = p->dinttime;	
			a1logd(p->log, 3, "i1disp: ntegration time set to %f secs\n",p->inttime);
		}
	}

	/* Do an initial fixed integration time frequency measurement. */
	a1logd(p->log, 3, "Doing fixed period frequency measurement over %f secs\n",p->inttime)

	if ((ev = i1d2_freq_measure(p, &p->inttime, rmeas)) != inst_ok)
		return ev;

	for (i = 0; i < 3; i++)
		rgb[i] = p->rgbadj[i] * 0.5 * rmeas[i]/p->inttime;

	a1logd(p->log, 3, "Got %f %f %f raw, %f %f %f Hz\n",
	rmeas[0], rmeas[1], rmeas[2], rgb[0], rgb[1], rgb[2]);

	/* Decide whether any channels need re-measuring. */
	/* Threshold is a count of 75, and switch to period */
	/* measurement mode on count less than 37. */
	fmask = 0x0;
	for (i = 0; i < 3; i++) {

		if (rmeas[i] <= 75.0) {
			fmask |= (1 << i);		/* Yes, do another freq re-measure */

			if (rmeas[i] <= 37.5) {	
				mask |= (1 << i);		/* Do a period re-measure */
				fmask = 0;				/* Don't bother with freq re-measure */  
			}
			if (rmeas[i] >= 10.0) {	/* Compute target edges */
				cdgec[i] = (int)(2.0 * rgb[i] * p->inttime + 0.5); 
				if (cdgec[i] > 2000)
					cdgec[i] = 2000;
				else if (cdgec[i] < ME)
					cdgec[i] = ME;
			}
		}
	}
	a1logd(p->log, 3, "Freq mask = 0x%x, Period mask 0x%x\n",fmask, mask);
	a1logd(p->log, 3, "cdgec = %d %d %d\n",cdgec[0],cdgec[1],cdgec[2]);

	/* If there is a frequency re-measure */
	/* ** This doesn't actually work. The quantization error */
	/* for each read is 0.5, but averaging 2 reads it drops */
	/* to 0.354, not the desired 0.25 that would have been */
	/* acheived with double the integration time. ** */ 
	if (fmask != 0) {
		a1logd(p->log, 3, "Doing frequency re-measurement over %f secs\n",p->inttime);
		if ((ev = i1d2_freq_measure(p, &p->inttime, rmeas)) != inst_ok)
			return ev;

		for (i = 0; i < 3; i++) {
			rgb[i] += p->rgbadj[i] * 0.5 * rmeas[i]/p->inttime;
			rgb[i] /= 2.0;
		}

		a1logd(p->log, 3, "Got %f %f %f raw, %f %f %f Avg. Hz\n",
		rmeas[0], rmeas[1], rmeas[2], rgb[0], rgb[1], rgb[2]);

	/* If there is a period re-measure */
	} else if (mask != 0) {

		/* See if we need to compute a target edge count */
		for (i = 0; i < 3; i++) {
			if ((mask & (1 << i)) && cdgec[i] == ME)
				break;
		}

		/* Yes we do */
		if (i < 3) {

			a1logd(p->log, 3, "Doing 1st period pre-measurement mask 0x%x, edgec %d %d %d\n",
			mask, edgec[0], edgec[1], edgec[2]);

			/* Do an initial measurement of 1 edge to estimate the */
			/* number of edges needed for the whole integration time. */
			if ((ev = i1d2_period_measure(p, edgec, rmeas)) != inst_ok)
				return ev;

			a1logd(p->log, 3, "Got %f %f %f raw %f %f %f Hz\n",
			rmeas[0], rmeas[1], rmeas[2],
			(p->rgbadj[0] * 0.5 * (double)edgec[0] * p->clk_freq)/rmeas[0],
			(p->rgbadj[1] * 0.5 * (double)edgec[1] * p->clk_freq)/rmeas[1],
			(p->rgbadj[2] * 0.5 * (double)edgec[2] * p->clk_freq)/rmeas[2]);

			/* Compute adjusted edge count for channels we're remeasuring, */
			/* aiming for a values of int_clocks. */
			for (i = 0; i < 3; i++) {
				double ns;
				if ((mask & (1 << i)) == 0)
					continue;
				if (p->int_clocks > ((2000.0 - 0.5) * rmeas[i]))
					ns = 2000.0;			/* Maximum edge count */
				else {
					ns = floor(p->inttime * edgec[i] * p->clk_freq/rmeas[i] + 0.5);
					if (ns < ME)			/* Minimum edge count */
						ns = ME;
				}
				edgec[i] = (int)ns;

				/* Sanity check cdgec value, in case light level has changed */
				if ((edgec[i] * 3) < (cdgec[i] * 2)) {
					cdgec[i] = edgec[i];
				}
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
	
			a1logd(p->log, 3, "Doing period re-measurement mask 0x%x, edgec %d %d %d\n",
			mask, edgec[0], edgec[1], edgec[2]);

			if ((ev = i1d2_period_measure(p, edgec, rmeas2)) != inst_ok)
				return ev;
	
			a1logd(p->log, 3, "Got %f %f %f raw %f %f %f Hz\n",
			rmeas[0], rmeas[1], rmeas[2],
			(p->rgbadj[0] * 0.5 * (double)edgec[0] * p->clk_freq)/rmeas2[0],
			(p->rgbadj[1] * 0.5 * (double)edgec[1] * p->clk_freq)/rmeas2[1],
			(p->rgbadj[2] * 0.5 * (double)edgec[2] * p->clk_freq)/rmeas2[2]);

			a1logd(p->log, 3, "Int period %f %f %f secs\n",
			rmeas2[0]/p->clk_freq, rmeas2[1]/p->clk_freq, rmeas2[2]/p->clk_freq);

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
			rgb[i] = (p->rgbadj[i] * 0.5 * (double)edgec[i] * p->clk_freq)/rmeas[i];
			a1logd(p->log, 3, "%d after scale = %f\n",i,rgb[i]);
	
			rgb[i] -= p->reg103_F[i];		/* Subtract black level */
			a1logd(p->log, 3, "%d after sub black = %f\n",i,rgb[i]);
	
			if (rgb[i] < 0.0001)
				rgb[i] = 0.0001;
			a1logd(p->log, 3, "%d after limit min = %f\n",i,rgb[i]);
		}
	}

	a1logd(p->log, 3, "Cooked RGB Hz = %f %f %f\n",rgb[0],rgb[1],rgb[2]);
	
	return inst_ok;
}
#endif
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
	} else {				/* i1 disp 2 or ColorMunki Smile */
		int refreshm = 0;	/* Assume non-refresh mode */

		if (p->refrmode && !IMODETST(p->mode, inst_mode_emis_ambient)) {
			refreshm = 1; 
		}

		if ((ev = i1d2_take_measurement(p, refreshm, rgb)) != inst_ok)
			return ev;
	}

	/* Multiply by calibration matrix to arrive at XYZ */
	if (IMODETST(p->mode, inst_mode_emis_ambient)) {
		mat = p->amb;				/* Ambient matrix */
	} else {
		if (p->icx)
			mat = p->reg54_F;		/* CRT/factory matrix/CCFL */
		else
			mat = p->reg4_F;		/* LCD/user matrix/LED */
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

	if (!IMODETST(p->mode, inst_mode_emis_ambient)) {

		/* Apply the colorimeter correction matrix */
		icmMulBy3x3(XYZ, p->ccmat, XYZ);
	}

	a1logd(p->log, 3, "returning XYZ = %f %f %f\n",XYZ[0],XYZ[1],XYZ[2]);
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

	a1logd(p->log, 3, "Black rgb = %f %f %f\n",rgb1[0],rgb1[1],rgb1[2]);

	/* Save it to the EEPROM */
	for (i = 0; i < 3; i++) {
		if ((ev = i1disp_wrreg_float(p, rgb1[i], 103 + 4 * i)) != inst_ok)
			return ev;
		p->reg103_F[i] = rgb1[i];
	}
	return inst_ok;
}

/* Measure the refresh rate */
static inst_code
i1disp_read_refrate(
	inst *pp,
	double *ref_rate		/* Return value, 0.0 if fails */
) {
	i1disp *p = (i1disp *)pp;
	int i;
	inst_code ev;
	double measp = 0.0;

	a1logd(p->log, 3, "Frequency calibration called\n");

	if (p->dtype != 1)
		return inst_unsupported;

	/* Average a few refresh period readings */
	for (i = 0; i < p->nmeasprds; i++) {
		int mp;

		/* Measures period in clocks */
		if ((ev = i1disp_rd_meas_ref_period(p, &mp)) != inst_ok)
			return ev;
		if (mp == 0) {
			measp = 0.0;
			break;			/* Too dark to measure */
		}
		measp += (double)mp;
	}

	/* Compute the measurement frequency */
	if (measp != 0.0) {
		double rrate = (p->clk_freq * (double)p->nmeasprds)/measp;
		a1logd(p->log, 3, "Sample frequency measured = %f\n",rrate);
		if (ref_rate != NULL)
			*ref_rate = rrate;
		return inst_ok;
	} else {
		a1logd(p->log, 3, "No discernable refresh frequency measured\n");
		if (ref_rate != NULL)
			*ref_rate = 0.0;
		return inst_misread;
	}
}

/* Do a refresh period cailbration  */
static inst_code
i1disp_do_fcal_setit(
	i1disp *p				/* Object */
) {
	int i;
	inst_code ev;

	a1logd(p->log, 3, "Frequency calibration called\n");

	if (p->dtype != 1)
		return i1disp_interp_code((inst *)p, I1DISP_CANT_MEASP_CALIB);

	if ((ev = i1disp_read_refrate((inst *)p, &p->refrate)) != inst_ok
	 && ev != inst_misread) 
		return ev;

	if (p->refrate != 0.0) {
		p->refperiod = 1.0/p->refrate;
		p->refrvalid = 1;
	} else {
		p->refrvalid = 0;
	}
	p->rrset = 1;

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
		{ { 0x24,0xb6,0xb5,0x13 }, NULL },		/* ColorMunki Smile */
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

	a1logd(p->log, 3, "i1disp: about to check response and unlock instrument if needed\n");

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
			if (codes[i].flag == (int *)-1) {
				a1logd(p->log, 3, "Failed to find correct unlock code\n");
				return i1disp_interp_code((inst *)p, I1DISP_UNKNOWN_MODEL);
			}

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
	a1logd(p->log, 3, "Version string = %5.3f\n",ver);

	/* Read register 0x79 for the model identifier */
	if ((ev = i1disp_rdreg_byte(p, &vv, 121) ) != inst_ok) {
		return ev;
	}
	vv &= 0xff;

	a1logd(p->log, 3, "Version character = 0x%02x = '%c'\n",vv,vv);

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

	} else if (ver >= 6.0 && ver <= 6.29 && vv == 'M') {
		/* ColorMunki Create ? */
		/* ColorMunki Smile */
		if (p->dtype == 0)		/* Not sure if this is set by Create */
			p->dtype = 1;
	} else {
		/* Reject any version or model we don't know about */
		a1logv(p->log, 1, "Version string = %3.1f\nID character = 0x%02x = '%c'\n",ver,vv,vv);
		return i1disp_interp_code((inst *)p, I1DISP_UNKNOWN_VERS_ID);
	}

	a1logd(p->log, 2, "i1disp: instrument is responding, unlocked, and right type\n");

	return inst_ok;
}

/* Read all the relevant register values */
static inst_code
i1disp_read_all_regs(
	i1disp *p				/* Object */
) {
	inst_code ev;
	int i;

	a1logd(p->log, 3, "i1disp: about to read all the registers\n");

	/* Serial number */
	if ((ev = i1disp_rdreg_word(p, &p->reg0_W, 0) ) != inst_ok)
		return ev;
	a1logd(p->log, 3, "serial number = %d\n",p->reg0_W);

	/* LCD/user calibration values */
	for (i = 0; i < 9; i++) {
		if ((ev = i1disp_rdreg_float(p, &p->reg4_F[i], 4 + 4 * i) ) != inst_ok)
			return ev;
		a1logd(p->log, 3, "LCD/user cal[%d] = %f\n",i,p->reg4_F[i]);
	}
	/* LCD/user calibration time */
	if ((ev = i1disp_rdreg_word(p, &p->reg50_W, 50) ) != inst_ok)
		return ev;
	a1logd(p->log, 3, "LCD/user calibration time = 0x%x = %s\n",p->reg50_W, ctime_32(&p->reg50_W));

	/* LCD/user calibration flag */
	if ((ev = i1disp_rdreg_short(p, &p->reg126_S, 126) ) != inst_ok)
		return ev;
	a1logd(p->log, 3, "User cal flag = 0x%x\n",p->reg126_S);


	/* CRT/factory calibration values */
	for (i = 0; i < 9; i++) {
		if ((ev = i1disp_rdreg_float(p, &p->reg54_F[i], 54 + 4 * i) ) != inst_ok)
			return ev;
		a1logd(p->log, 3, "CRT/factory cal[%d] = %f\n",i,p->reg54_F[i]);
	}
	/* CRT/factory calibration flag */
	if ((ev = i1disp_rdreg_word(p, &p->reg90_W, 90) ) != inst_ok)
		return ev;
	a1logd(p->log, 3, "CRT/factory flag = 0x%x = %s\n",p->reg90_W, ctime_32(&p->reg90_W));


	/* Integration clock period in nsec */
	if ((ev = i1disp_rdreg_short(p, &p->reg40_S, 40) ) != inst_ok)
		return ev;
	a1logd(p->log, 3, "Reg40 = %d\n",p->reg40_S);

	/* Calibration factor */
	if ((ev = i1disp_rdreg_short(p, &p->reg42_S, 42) ) != inst_ok)
		return ev;
	a1logd(p->log, 3, "Reg42 = %d\n",p->reg42_S);

	/* Calibration factors */
	for (i = 0; i < 3; i++) {
		if ((ev = i1disp_rdreg_short(p, &p->reg44_S[i], 44 + 2 * i) ) != inst_ok)
			return ev;
		a1logd(p->log, 3, "reg44[%d] = %d\n",i,p->reg44_S[i]);
	}

	/* Measurement/master clock period */
	if ((ev = i1disp_rdreg_float(p, &p->clk_prd, 94) ) != inst_ok)
		return ev;
	a1logd(p->log, 3, "Master clock Frequency = %e\n",1/p->clk_prd);

	/* unknown */
	if ((ev = i1disp_rdreg_word(p, &p->reg98_W, 98) ) != inst_ok)
		return ev;
	a1logd(p->log, 3, "reg98 = 0x%x = %s\n",p->reg98_W,ctime_32(&p->reg98_W));

	/* unknown */
	if ((ev = i1disp_rdreg_byte(p, &p->reg102_B, 102) ) != inst_ok)
		return ev;
	a1logd(p->log, 3, "reg102 = 0x%x\n",p->reg102_B);

	/* Dark current calibration values */
	/* Should we set to a default 0.0 if reg126_S < 0xd ?? */
	for (i = 0; i < 3; i++) {
		if ((ev = i1disp_rdreg_float(p, &p->reg103_F[i], 103 + 4 * i)) != inst_ok) {
			if ((ev & inst_imask) != I1DISP_FLOAT_NOT_SET)
				return ev;
			p->reg103_F[i] = 0.0;
		}
		a1logd(p->log, 3, "darkcal[%d] = %f\n",i,p->reg103_F[i]);
	}

	/* Unknown byte */
	if ((ev = i1disp_rdreg_byte(p, &p->reg115_B, 115) ) != inst_ok)
		return ev;
	a1logd(p->log, 3, "Unknown 115 byte = 0x%x\n",p->reg115_B);

	/* device ID byte */
	if ((ev = i1disp_rdreg_byte(p, &p->reg121_B, 121) ) != inst_ok)
		return ev;
	a1logd(p->log, 3, "device type byte = 0x%x\n",p->reg121_B);

	/* Unlock string */
	for (i = 0; i < 4; i++) {
		int vv;
		if ((ev = i1disp_rdreg_byte(p, &vv, 122 + i) ) != inst_ok)
			return ev;
		p->reg122_B[i] = (char)vv;
	}
	p->reg122_B[i] = '\000';
	a1logd(p->log, 3, "unlock string = '%s'\n",p->reg122_B);

	p->serno[0] = '\000';

	/* Read extra registers */
	if (p->dtype == 1) {

#ifdef NEVER	/* Not used, so don't bother */
		for (i = 0; i < 3; i++) {
			if ((ev = i1disp_rdreg_float(p, &p->reg128_F[i], 128 + 4 * i) ) != inst_ok)
				return ev;
			a1logd(p->log, 3, "reg128_F[%d] = %f\n",i,p->reg128_F[i]);
		}
#endif /* NEVER */

		for (i = 0; i < 3; i++) {
			if ((ev = i1disp_rdreg_float(p, &p->reg144_F[i], 144 + 4 * i) ) != inst_ok) {
				if ((ev & inst_imask) != I1DISP_FLOAT_NOT_SET)
					return ev;
				p->reg144_F[i] = 1.0;
			}
			a1logd(p->log, 3, "Ambient scale factor [%d] = %f\n",i,p->reg144_F[i]);
		}

		/* Read the integration time */
		if ((ev = i1disp_rd_int_time(p, &p->int_clocks) ) != inst_ok)
			return ev;
		a1logd(p->log, 3, "Integration time = %d\n",p->int_clocks);

	/* ColorMunki Smile extra information */
	/* (Colormunki Create too ????) */ 
	} else  if (p->dtype == 2) {
		int i, v;

		/* Smile doesn't have ambient - reg144 seems to contain LCD cal type, */
		/* ie. "CCFLWLED" */

		/* Serial Number */
		if ((ev = i1disp_rdexreg_bytes(p, (unsigned char *)p->serno, 0x104, 20)) != inst_ok)
			return ev;
		p->serno[19] = '\000';
	}
	a1logd(p->log, 2, "i1disp: all registers read OK\n");

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
	/* For the i1display 1&2, it has a value of 0x0d. */
	/* Value 0x07 seems to be for a "user calibration" */
	/* Values 3 & 6 seem to always "errors" as does a value */
	/* < 7 in most circumstances. But the Heidelberg Viewmaker */
	/* (from Sequel Imaging) and Lacie Blue Eye colorimeter seems to have a value of 2. */
	/* The Barco sensor seems to have a value of 0x20 */
	/* The ColorMunki Smile has a value of 0x21 */
	if (p->reg126_S == 0xffffffff || (p->reg126_S < 7 && p->reg126_S != 2))
		return i1disp_interp_code((inst *)p, I1DISP_BAD_LCD_CALIBRATION);

	/* Not quite sure about this, but we're assuming this */
	/* is set to 2 or 0xd if reg4-36 hold the LCD calibration, */
	/* and some other number if they are not set, or set */
	/* to a custom user calibration. */
	if (p->reg126_S != 0x02
	 && p->reg126_S != 0x0d
	 && p->reg126_S != 0x20
	 && p->reg126_S != 0x21)
		return i1disp_interp_code((inst *)p, I1DISP_BAD_LCD_CALIBRATION);
		
	if (p->reg90_W == 0xffffffff)
		return i1disp_interp_code((inst *)p, I1DISP_BAD_CRT_CALIBRATION);

	/* Compute ambient matrix */
	for (i = 0; i < 9; i++)
		p->amb[i] = p->reg144_F[i % 3] * 0.5 * (p->reg4_F[i] + p->reg54_F[i]);

	/* Integration clock frequency */
	p->iclk_freq = 1.0/(p->reg40_S * 1e-9);

	/* Master/Measurement clock frequency */
	p->clk_freq = 1.0/p->clk_prd;

	/* RGB channel calibration factors */
	for (i = 0; i < 3; i++) {

		/* Individual channel calibration factors, typically 1.0 */
		p->rgbadj[i] = (double)p->reg44_S[i] * 100.0/(double)p->reg42_S;
		a1logd(p->log, 3, "reg44+%dcalc2 = %f\n",i,p->rgbadj[i]);
	}

	/* Set some defaults */
	p->nmeasprds = 5;		/* Number of disp refresh period measurments to average */ 
							/* in doing frequency calibration */
	p->dinttime = 1.0;		/* 1.0 second integration time default */
	p->inttime = p->dinttime;	/* Current integration time */

	return inst_ok;
}

/* ------------------------------------------------------------------------ */

/* Establish communications with a I1DISP */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
i1disp_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	i1disp *p = (i1disp *) pp;
	unsigned char buf[16];
	int rsize;
	int se;
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "i1disp: About to init coms\n");

	if (p->icom->port_type(p->icom) != icomt_usb) {
		a1logd(p->log, 1, "i1disp_init_coms: coms is not the right type!\n");
		return i1disp_interp_code((inst *)p, I1DISP_UNKNOWN_MODEL);
	}

	/* Set config, interface, write end point, read end point */
	/* ("serial" end points aren't used - the i1display uses USB control messages) */

	/* Set config, interface, write end point, read end point, read quanta */
	if ((se = p->icom->set_usb_port(p->icom, 1, 0x00, 0x00, icomuf_none, 0, NULL)) != ICOM_OK) { 
		a1logd(p->log, 1, "i1disp_init_coms: set_usbe_port failed ICOM err 0x%x\n",se);
		return i1disp_interp_code((inst *)p, icoms2i1disp_err(se));
	}

	/* Check instrument is responding */
	if ((ev = i1disp_command_1(p, i1d_status, NULL, 0, buf, 8, &rsize, 0.5)) != inst_ok
	                                            && (ev & inst_imask) != I1DISP_LOCKED) {
		a1logd(p->log, 1, "i1disp_init_coms: failed with rv = 0x%x\n",ev);
		return ev;
	}

	a1logd(p->log, 2, "i1disp: init coms OK\n");

	p->gotcoms = 1;
	return inst_ok;
}

static inst_code set_default_disp_type(i1disp *p);

/* Initialise the I1DISP */
/* return non-zero on an error, with dtp error code */
static inst_code
i1disp_init_inst(inst *pp) {
	i1disp *p = (i1disp *)pp;
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "i1disp_init_inst: called\n");

	if (p->gotcoms == 0)
		return i1disp_interp_code((inst *)p, I1DISP_NO_COMS);	/* Must establish coms first */

	/* Check instrument is responding, and right type */
	if ((ev = i1disp_check_unlock(p)) != inst_ok)
		return ev;

	/* Read all the registers and store their contents */
	if ((ev = i1disp_read_all_regs(p)) != inst_ok)
		return ev;

#ifdef NEVER
	/* Dump all the register space */
	if (p->dtype < 2) {
		unsigned char buf[0x200];
		int i, len;

		len = 128;
		if (p->dtype != 0)
			len = 160;

		for (i = 0; i < len; i++) {
			int v;
			if ((ev = i1disp_rdreg_byte(p, &v, i)) != inst_ok) {
				return ev;
			}
			buf[i] = v;
		}
		dump_bytes(p->log, "dump:", buf, 0, len);

	/* Dump ColorMunki Smile extended range */
	/* Main difference is Ascii serial number + other minor unknown */
	} else {
		unsigned char buf[0x200];

	
		if ((ev = i1disp_rdexreg_bytes(p, buf, 0, 0x200)) != inst_ok) {
			return ev;
		}
		dump_bytes(p->log, "dump:", buf, 0, 0x200);
	}
#endif /* NEVER */

	if ((ev = i1disp_compute_factors(p)) != inst_ok)
		return ev;

	p->trig = inst_opt_trig_user;

	/* Set a default calibration */
	if ((ev = set_default_disp_type(p)) != inst_ok) {
		return ev;
	}

	p->inited = 1;
	a1logd(p->log, 2, "i1disp_init_inst: inited OK\n");

	return inst_ok;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
i1disp_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	i1disp *p = (i1disp *)pp;
	int user_trig = 0;
	int rv = inst_protocol_error;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->trig == inst_opt_trig_user) {

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "i1disp: inst_opt_trig_user but no uicallback function set!\n");
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
	if ((rv = i1disp_take_XYZ_measurement(p, val->XYZ)) != inst_ok)
		return rv;
	/* This may not change anything since instrument may clamp */
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);
	val->loc[0] = '\000';
	if (IMODETST(p->mode, inst_mode_emis_ambient))
		val->mtype = inst_mrt_ambient;
	else
		val->mtype = inst_mrt_emission;
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
inst_code i1disp_col_cor_mat(
inst *pp,
double mtx[3][3]
) {
	i1disp *p = (i1disp *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (mtx == NULL) {
		icmSetUnity3x3(p->ccmat);
	} else {
		if (p->cbid == 0) {
			a1loge(p->log, 1, "i1disp: can't set col_cor_mat over non base display type\n");
			return inst_wrong_setup;
		}
		icmCpy3x3(p->ccmat, mtx);
	}

	return inst_ok;
}

/* Return needed and available inst_cal_type's */
static inst_code i1disp_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	i1disp *p = (i1disp *)pp;
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
		
	if (p->dtype == 0) {	/* Eye-One Display 1 */
	   a_cals |= inst_calt_emis_offset;
	}

	if (p->dtype == 1 && p->refrmode != 0) {
		if (p->rrset == 0)
			n_cals |= inst_calt_ref_freq;
		a_cals |= inst_calt_ref_freq;
	}

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

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
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	i1disp *p = (i1disp *)pp;
	inst_code ev;
    inst_cal_type needed, available;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	id[0] = '\000';

	if ((ev = i1disp_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
		return ev;

	/* Translate inst_calt_all/needed into something specific */
	if (*calt == inst_calt_all
	 || *calt == inst_calt_needed
	 || *calt == inst_calt_available) {
		if (*calt == inst_calt_all) 
			*calt = (needed & inst_calt_n_dfrble_mask) | inst_calt_ap_flag;
		else if (*calt == inst_calt_needed)
			*calt = needed & inst_calt_n_dfrble_mask;
		else if (*calt == inst_calt_available)
			*calt = available & inst_calt_n_dfrble_mask;

		a1logd(p->log,4,"i1disp_calibrate: doing calt 0x%x\n",calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return inst_ok;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return inst_unsupported;
	}

	/* Do the appropriate calibration */
	if (p->dtype == 0) {		/* Eye-One Display 1 */
		if (*calt & inst_calt_emis_offset) {

			if (*calc != inst_calc_man_ref_dark) {
				*calc = inst_calc_man_ref_dark;
				return inst_cal_setup;
			}

			/* Do black offset calibration */
			if ((ev = i1disp_do_black_cal(p)) != inst_ok)
				return ev;

			*calt &= ~inst_calt_emis_offset;
		}

	} else {				/* Eye-One Display 2 */
		if ((*calt & inst_calt_ref_freq) && p->refrmode != 0) {

			if (*calc != inst_calc_emis_white) {
				*calc = inst_calc_emis_white;
				return inst_cal_setup;
			}

			/* Do CRT frequency calibration and set integration time */
			if ((ev = i1disp_do_fcal_setit(p)) != inst_ok)
				return ev;

			/* Quantize the sample time */
			if (p->refperiod > 0.0) {
				int n;
				n = (int)ceil(p->dinttime/p->refperiod);
				p->inttime = n * p->refperiod;
				a1logd(p->log, 3, "i1disp: integration time quantize to %f secs\n",p->inttime);
			} else {
				p->inttime = p->dinttime;	
				a1logd(p->log, 3, "i1disp: integration time set to %f secs\n",p->inttime);
			}

			*calt &= ~inst_calt_ref_freq;
		}
	}

	return inst_ok;
}

/* Return the last calibrated refresh rate in Hz. Returns: */
static inst_code i1disp_get_refr_rate(inst *pp,
double *ref_rate
) {
	i1disp *p = (i1disp *)pp;
	if (p->refrvalid) {
		*ref_rate = p->refrate;
		return inst_ok;
	} else if (p->rrset) {
		*ref_rate = 0.0;
		return inst_misread;
	}
	return inst_needs_cal;
}

/* Set the calibrated refresh rate in Hz. */
/* Set refresh rate to 0.0 to mark it as invalid */
/* Rates outside the range 5.0 to 150.0 Hz will return an error */
static inst_code i1disp_set_refr_rate(inst *pp,
double ref_rate
) {
	i1disp *p = (i1disp *)pp;

	if (ref_rate != 0.0 && (ref_rate < 5.0 || ref_rate > 150.0))
		return inst_bad_parameter;

	p->refrate = ref_rate;
	if (ref_rate == 0.0)
		p->refrvalid = 0;
	else {
		p->refperiod = 1.0/p->refrate;
		p->refrvalid = 1;
	}
	p->rrset = 1;

	return inst_ok;
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
	inst_del_disptype_list(p->dtlist, p->ndtlist);
	free(p);
}

/* Return the instrument capabilities */
void i1disp_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	i1disp *p = (i1disp *)pp;
	inst_mode cap1 = 0;
	inst2_capability cap2 = 0;

	cap1 |= inst_mode_emis_spot
	     |  inst_mode_colorimeter
	        ;

	cap2 |= inst2_prog_trig
	     |  inst2_user_trig
	     |  inst2_disptype
	     |  inst2_ccmx
	        ;

	/* i1D2 has refresh display & ambient capability */
	/* but i1D1 & ColorMunki Smile don't */
	if (p->dtype == 1) {
		cap1 |= inst_mode_emis_ambient
	         |  inst_mode_emis_refresh_ovd
	         |  inst_mode_emis_norefresh_ovd
		        ;

		cap2 |= inst2_refresh_rate
		     |  inst2_emis_refr_meas
		        ;
	}

	if (pcap1 != NULL)
		*pcap1 = cap1;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = inst3_none;
}

/* Check device measurement mode */
inst_code i1disp_check_mode(inst *pp, inst_mode m) {
	i1disp *p = (i1disp *)pp;
	inst_mode cap;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	pp->capabilities(pp, &cap, NULL, NULL);

	/* Simple filter for most modes */
	if (m & ~cap)
		return inst_unsupported;

	/* Only display emission mode supported */
	if (!IMODETST(m, inst_mode_emis_spot)
	 && !(p->dtype == 1 && IMODETST(m, inst_mode_emis_ambient))) {
		return inst_unsupported;
	}

	return inst_ok;
}

/* Set device measurement mode */
inst_code i1disp_set_mode(inst *pp, inst_mode m) {
	i1disp *p = (i1disp *)pp;
	inst_code ev;

	if ((ev = i1disp_check_mode(pp, m)) != inst_ok)
		return ev;

	p->mode = m;

	if (     IMODETST(p->mode, inst_mode_emis_norefresh_ovd))	/* Must test this first! */
		p->refrmode = 0;
	else if (IMODETST(p->mode, inst_mode_emis_refresh_ovd))
		p->refrmode = 1;

	return inst_ok;
}

inst_disptypesel i1disp_disptypesel[3] = {
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


inst_disptypesel smile_disptypesel[3] = {
	{
		inst_dtflags_default,		/* flags */
		1,							/* cbix */
		"f",						/* sel */
		"LCD with CCFL backlight",	/* desc */
		0,							/* refr */
		1							/* ix */
	},
	{
		inst_dtflags_none,
		0,
		"e",
		"LCD with White LED backlight",
		0,
		0
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

static void set_base_disptype_list(i1disp *p) {
	/* set the base display type list */
	if (p->itype == instSmile) {
		p->_dtlist = smile_disptypesel;
	} else {
		p->_dtlist = i1disp_disptypesel;
	}
}

/* Get mode and option details */
static inst_code i1disp_get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	i1disp *p = (i1disp *)pp;
	inst_code rv = inst_ok;

	/* Create/Re-create a current list of abailable display types */
	if (p->dtlist == NULL || recreate) {
		if ((rv = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    p->_dtlist, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return rv;
	}

	if (pnsels != NULL)
		*pnsels = p->ndtlist;

	if (psels != NULL)
		*psels = p->dtlist;

	return inst_ok;
}

/* Given a display type entry, setup for that type */
static inst_code set_disp_type(i1disp *p, inst_disptypesel *dentry) {
	int refrmode;

	p->icx = dentry->ix;
	p->cbid = dentry->cbid;
	refrmode = dentry->refr;

	if (     IMODETST(p->mode, inst_mode_emis_norefresh_ovd)) {	/* Must test this first! */
		refrmode = 0;
	} else if (IMODETST(p->mode, inst_mode_emis_refresh_ovd)) {
		refrmode = 1;
	}

	if (p->refrmode != refrmode) {
		p->rrset = 0;					/* This is a hint we may have swapped displays */
		p->refrvalid = 0;
	}
	p->refrmode = refrmode; 

	if (dentry->flags & inst_dtflags_ccmx) {
		icmCpy3x3(p->ccmat, dentry->mat);
	} else {
		icmSetUnity3x3(p->ccmat);
	}

	return inst_ok;
}

/* Setup the default display type */
static inst_code set_default_disp_type(i1disp *p) {
	inst_code ev;
	int i;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
		    p->_dtlist, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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
static inst_code i1disp_set_disptype(inst *pp, int ix) {
	i1disp *p = (i1disp *)pp;
	inst_code ev;
	inst_disptypesel *dentry;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    p->_dtlist, 1 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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
 * Since there is no interaction with the instrument,
 * was assume that all of these can be done before initialisation.
 */
static inst_code
i1disp_get_set_opt(inst *pp, inst_opt_type m, ...)
{
	i1disp *p = (i1disp *)pp;
	inst_code ev;

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

	/* Record the trigger mode */
	if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_user) {
		p->trig = m;
		return inst_ok;
	}

	return inst_unsupported;
}

/* Constructor */
extern i1disp *new_i1disp(icoms *icom, instType itype) {
	i1disp *p;
	if ((p = (i1disp *)calloc(sizeof(i1disp),1)) == NULL) {
		a1loge(icom->log, 1, "new_i1disp: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	p->init_coms         = i1disp_init_coms;
	p->init_inst         = i1disp_init_inst;
	p->capabilities      = i1disp_capabilities;
	p->check_mode        = i1disp_check_mode;
	p->set_mode          = i1disp_set_mode;
	p->get_disptypesel   = i1disp_get_disptypesel;
	p->set_disptype      = i1disp_set_disptype;
	p->get_set_opt       = i1disp_get_set_opt;
	p->read_sample       = i1disp_read_sample;
	p->read_refrate      = i1disp_read_refrate;
	p->get_n_a_cals      = i1disp_get_n_a_cals;
	p->calibrate         = i1disp_calibrate;
	p->col_cor_mat       = i1disp_col_cor_mat;
	p->get_refr_rate     = i1disp_get_refr_rate;
	p->set_refr_rate     = i1disp_set_refr_rate;
	p->interp_error      = i1disp_interp_error;
	p->del               = i1disp_del;

	p->icom = icom;
	p->itype = icom->itype;

	if (p->itype == instI1Disp2)
		p->dtype = 1;			/* i1Display2 */

	else if (p->itype == instSmile) {
		p->dtype = 2;			/* Smile */
	}

	icmSetUnity3x3(p->ccmat);	/* Set the colorimeter correction matrix to do nothing */
	set_base_disptype_list(p);

	return p;
}

/* ---------------------------------------------------------------- */

// Print bytes as hex to debug log */
static void dump_bytes(a1log *log, char *pfx, unsigned char *buf, int base, int len) {
	int i, j, ii;
	char oline[200] = { '\000' }, *bp = oline;
	for (i = j = 0; i < len; i++) {
		if ((i % 16) == 0)
			bp += sprintf(bp,"%s%04x:",pfx,base+i);
		bp += sprintf(bp," %02x",buf[i]);
		if ((i+1) >= len || ((i+1) % 16) == 0) {
			for (ii = i; ((ii+1) % 16) != 0; ii++)
				bp += sprintf(bp,"   ");
			bp += sprintf(bp,"  ");
			for (; j <= i; j++) {
				if (!(buf[j] & 0x80) && isprint(buf[j]))
					bp += sprintf(bp,"%c",buf[j]);
				else
					bp += sprintf(bp,".");
			}
			bp += sprintf(bp,"\n");
			a1logd(log,0,oline);
			bp = oline;
		}
	}
}




