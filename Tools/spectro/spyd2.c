
/* 
 * Argyll Color Correction System
 *
 * Datacolor/ColorVision Spyder 2/3 related software.
 *
 * Author: Graeme W. Gill
 * Date:   17/9/2007
 *
 * Copyright 2006 - 2009, Graeme W. Gill
 * All rights reserved.
 *
 * (Based initially on i1disp.c)
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/*
	IMPORTANT NOTE:

    The Spyder 2 instrument cannot function without the driver software
	having access to the vendor supplied PLD firmware pattern for it.
    This firmware is not provided with Argyll, since it is not available
    under a compatible license.

    The purchaser of a Spyder 2 instrument should have received a copy
    of this firmware along with their instrument, and should therefore be able to
    enable the Argyll driver for this instrument by using the spyd2en utility
	to create a spyd2PLD.bin file.

	[ The Spyder 3 doesn't need a PLD firmware file. ]

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

/* TTBD:

	Would be good to add read/write data values if debug >= 3
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
#include "spyd2.h"

#undef DEBUG

#ifdef DEBUG
#define DBG(xxx) printf xxx ;
#else
#define DBG(xxx) 
#endif

#define DO_RESETEP				/* Do the miscelanous resetep()'s */
#define CLKRATE 1000000			/* Clockrate the Spyder 2 hardware runs at */
#define MSECDIV (CLKRATE/1000)	/* Divider to turn clocks into msec */
#define DEFRRATE 50				/* Default display refresh rate */
#define DO_ADAPTIVE				/* Adapt the integration time to the light level */
								/* This helps repeatability at low levels A LOT */

#define LEVEL2					/* Second level (nonliniarity) calibration */
#define RETRIES 4				/* usb_reads are unreliable - bug in spyder H/W ?*/

#ifdef DO_ADAPTIVE
# define RDTIME 2.0				/* Base time to integrate reading over */ 
#else /* !DO_ADAPTIVE */
# define RDTIME 5.0				/* Integrate over fixed longer time (manufacturers default) */ 
#endif /* !DO_ADAPTIVE */

static inst_code spyd2_interp_code(inst *pp, int ec);

/* ------------------------------------------------------------------------ */
/* Implementation */

/* Interpret an icoms error into a SPYD2 error */
static int icoms2spyd2_err(int se) {
	if (se & ICOM_USERM) {
		se &= ICOM_USERM;
		if (se == ICOM_USER)
			return SPYD2_USER_ABORT;
		if (se == ICOM_TERM)
			return SPYD2_USER_TERM;
		if (se == ICOM_TRIG)
			return SPYD2_USER_TRIG;
		if (se == ICOM_CMND)
			return SPYD2_USER_CMND;
	}
	if (se != ICOM_OK)
		return SPYD2_COMS_FAIL;
	return SPYD2_OK;
}

/* ----------------------------------------------------- */
/* Big endian wire format conversion routines */

/* Take an int, and convert it into a byte buffer big endian */
static void int2buf(unsigned char *buf, int inv) {
	buf[0] = (inv >> 24) & 0xff;
	buf[1] = (inv >> 16) & 0xff;
	buf[2] = (inv >> 8) & 0xff;
	buf[3] = (inv >> 0) & 0xff;
}

/* Take a short, and convert it into a byte buffer big endian */
static void short2buf(unsigned char *buf, int inv) {
	buf[0] = (inv >> 8) & 0xff;
	buf[1] = (inv >> 0) & 0xff;
}

/* Take a short sized buffer, and convert it to an int big endian */
static int buf2short(unsigned char *buf) {
	int val;
	val = buf[0];
	val = ((val << 8) + (0xff & buf[1]));
	return val;
}

/* Take a unsigned short sized buffer, and convert it to an int big endian */
static int buf2ushort(unsigned char *buf) {
	int val;
	val =               (0xff & buf[0]);
	val = ((val << 8) + (0xff & buf[1]));
	return val;
}

/* Take a word sized buffer, and convert it to an int big endian. */
static int buf2int(unsigned char *buf) {
	int val;
	val =                       buf[0];
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[3]));
	return val;
}


/* Take a 3 byte buffer, and convert it to an unsigned int big endian. */
static unsigned int buf2uint24(unsigned char *buf) {
	unsigned int val;
	val =                       buf[0];
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[2]));
	return val;
}

/* Take a 24 bit unsigned sized buffer in little endian */
/* format, and return an int */
static unsigned int buf2uint24le(unsigned char *buf) {
	unsigned int val;
	val =               (0xff & buf[2]);
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

/* Take a 24 bit unsigned sized buffer in little endian */
/* nibble swapped format, and return an int */
static unsigned int buf2uint24lens(unsigned char *buf) {
	unsigned int val;
	val =              (0xf &  buf[2]);
	val = (val << 4) + (0xf & (buf[2] >> 4));
	val = (val << 4) + (0xf &  buf[1]);
	val = (val << 4) + (0xf & (buf[1] >> 4));
	val = (val << 4) + (0xf &  buf[0]);
	val = (val << 4) + (0xf & (buf[0] >> 4));
	return val;
}

/* ============================================================ */
/* Low level commands */

/* USB Instrument commands */

/* Spyder 2: Reset the instrument */
static inst_code
spyd2_reset(
	spyd2 *p
) {
	int se;
	int isdeb = 0;
	int retr;
	inst_code rv = inst_ok;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nspyd2: Instrument reset\n");

	for (retr = 0; ; retr++) {
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xC7, 0, 0, NULL, 0, 5.0);

		if (se == ICOM_OK) {
			if (isdeb) fprintf(stderr,"Reset complete, ICOM code 0x%x\n",se);
			break;
		}
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Reset failed with  ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Reset retry with  ICOM err 0x%x\n",se);
	}

	p->icom->debug = isdeb;
	return rv;
}

/* Spyder 2: Get status */
/* return pointer may be NULL if not needed. */
static inst_code
spyd2_getstatus(
	spyd2 *p,
	int *stat	/* Return the 1 byte status code */
) {
	unsigned char pbuf[8];	/* status bytes read */
	int _stat;
	int se;
	int isdeb = 0;
	int retr;
	inst_code rv = inst_ok;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nspyd2: Get Status\n");

	for (retr = 0; ; retr++) {
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xC6, 0, 0, pbuf, 8, 5.0);

		if (se == ICOM_OK)
			break;
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Get Status failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Get Status retry with ICOM err 0x%x\n",se);
	}
	msec_sleep(100);		/* Limit rate status commands can be given */

	_stat = pbuf[0];		/* Only the first byte is examined. */
							/* Other bytes have information, but SW ignores them */

	if (isdeb) fprintf(stderr,"Get Status returns %d ICOM err 0x%x\n", _stat, se);

	p->icom->debug = isdeb;

	if (stat != NULL) *stat = _stat;

	return rv;
}

/* Read Serial EEProm bytes (implementation) */
/* Can't read more than 256 in one go */
static inst_code
spyd2_readEEProm_imp(
	spyd2 *p,
	unsigned char *buf,	/* Buffer to return bytes in */
	int addr,	/* Serial EEprom address, 0 - 511 */
	int size	/* Number of bytes to read, 0 - 128 (ie. max of  bank) */
) {
	int se;
	int isdeb = 0;
	int retr;
	inst_code rv = inst_ok;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb >= 2) fprintf(stderr,"\nspyd2: Read EEProm addr %d, bytes %d\n",addr,size);

	if (addr < 0 || (addr + size) > 512)
		return spyd2_interp_code((inst *)p, SPYD2_BAD_EE_ADDRESS);

	if (size >= 256)
		return spyd2_interp_code((inst *)p, SPYD2_BAD_EE_SIZE);

	for (retr = 0; ; retr++) {
		se = p->icom->usb_control(p->icom,
			               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
		                   0xC4, addr, size, buf, size, 5.0);
		if (se == ICOM_OK)
			break;
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Read bytes failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Read bytes retry with ICOM err 0x%x\n",se);
	}


	if (isdeb >= 2) fprintf(stderr,"Read EEProm ICOM err 0x%x\n", se);

	p->icom->debug = isdeb;

	return rv;
}

/* Read Serial EEProm bytes */
/* (Handles reads > 256 bytes) */
static inst_code
spyd2_readEEProm(
	spyd2 *p,
	unsigned char *buf,	/* Buffer to return bytes in */
	int addr,	/* Serial EEprom address, 0 - 511 */
	int size	/* Number of bytes to read, 0 - 511 */
) {

	if (addr < 0 || (addr + size) > 512)
		return spyd2_interp_code((inst *)p, SPYD2_BAD_EE_ADDRESS);
	
	while (size > 255) {		/* Single read is too big */
		inst_code rv;
		if ((rv = spyd2_readEEProm_imp(p, buf, addr, 255)) != inst_ok)
			return rv;
		size -= 255;
		buf  += 255;
		addr += 255;
	}
	return spyd2_readEEProm_imp(p, buf, addr, size);
}

/* Spyder 2: Download PLD pattern */
static inst_code
spyd2_loadPLD(
	spyd2 *p,
	unsigned char *buf,		/* Bytes to download */
	int size				/* Number of bytes */
) {
	int se;
	int isdeb = 0;
	int retr;
	inst_code rv = inst_ok;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb >= 2) fprintf(stderr,"\nspyd2: Load PLD %d bytes\n",size);

	for (retr = 0; ; retr++) {
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xC0, 0, 0, buf, size, 5.0);

		if (se == ICOM_OK)
			break;
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Load PLD failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Load PLD retry with ICOM err 0x%x\n",se);
	}

	if (isdeb >= 2) fprintf(stderr,"Load PLD returns ICOM err 0x%x\n", se);

	p->icom->debug = isdeb;

	return rv;
}

/* Get minmax command */
/* Figures out the current minimum and maximum frequency periods */
/* so as to be able to set a frame detect threshold. */
/* Note it returns 0,0 if there is not enough light. */ 
/* (The light to frequency output period size is inversly */
/*  related to the lightness level) */
/* (This isn't used by the manufacturers Spyder3 driver, */
/*  but the instrument seems to impliment it.) */
static inst_code
spyd2_GetMinMax(
	spyd2 *p,
	int *clocks,		/* Number of clocks to use (may get limited) */
	int *min,			/* Return min and max light->frequency periods */
	int *max
) {
	int rwbytes;			/* Data bytes read or written */
	int se;
	int isdeb = 0;
	inst_code rv = inst_ok;
	int value;
	int index;
	int retr;
	unsigned char buf[8];	/* return bytes read */

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb >= 2) fprintf(stderr,"\nspyd2: Get Min/Max, %d clocks\n",*clocks);

	/* Issue the triggering command */
	if (*clocks > 0xffffff)
		*clocks = 0xffffff;		/* Maximum count hardware will take ? */
	value = *clocks >> 8;
	value = (value >> 8) | ((value << 8) & 0xff00);		/* Convert to big endian */
	index = (*clocks << 8) & 0xffff;
	index = (index >> 8) | ((index << 8) & 0xff00);		/* Convert to big endian */

	for (retr = 0; ; retr++) {
		/* Issue the trigger command */
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xC2, value, index, NULL, 0, 5.0);

		if ((se & ICOM_USERM) || (se != ICOM_OK && retr >= RETRIES)) {
			/* Complete the operation so as not to leave the instrument in a hung state */
			msec_sleep(*clocks/MSECDIV);
			p->icom->usb_read(p->icom, 0x81, buf, 8, &rwbytes, 1.0);

			if (isdeb) fprintf(stderr,"\nspyd2: Get Min/Max Trig failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		if (se != ICOM_OK) {
			/* Complete the operation so as not to leave the instrument in a hung state */
			msec_sleep(*clocks/MSECDIV);
			p->icom->usb_read(p->icom, 0x81, buf, 8, &rwbytes, 1.0);

			msec_sleep(500);
			if (isdeb) fprintf(stderr,"\nspyd2: Get Min/Max Trig retry with ICOM err 0x%x\n",se);
			continue;
		}

		if (isdeb >= 2) fprintf(stderr,"Trigger Min/Max returns ICOM err 0x%x\n", se);
	
		/* Allow some time for the instrument to respond */
		msec_sleep(*clocks/MSECDIV);
	
		/* Now read the bytes */
		se = p->icom->usb_read(p->icom, 0x81, buf, 8, &rwbytes, 5.0);
		if (se == ICOM_OK)
			break;
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Get Min/Max failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Get Min/Max retry with ICOM err 0x%x\n",se);
	}

	if (rwbytes != 8) {
		if (isdeb) fprintf(stderr,"\nspyd2: Get Min/Max got short data read %d",rwbytes);
		p->icom->debug = isdeb;
		return spyd2_interp_code((inst *)p, SPYD2_BADREADSIZE);
	}

	*min = buf2ushort(&buf[0]);
	*max = buf2ushort(&buf[2]);

	if (isdeb >= 2) fprintf(stderr,"Get Min/Max got %d/%d returns ICOM err 0x%x\n", *min, *max, se);
	p->icom->debug = isdeb;

	return rv;
}

/* Get refresh rate (low level) command */
/* (This isn't used by the manufacturers Spyder3 driver, */
/*  but the instrument seems to impliment it.) */
static inst_code
spyd2_GetRefRate_ll(
	spyd2 *p,
	int *clocks,		/* Maximum number of clocks to use */
	int nframes,		/* Number of frames to count */
	int thresh,			/* Frame detection threshold */
	int *minfclks,		/* Minimum number of clocks per frame */
	int *maxfclks,		/* Maximum number of clocks per frame */
	int *clkcnt			/* Return number of clocks for nframes frames */
) {
	int rwbytes;			/* Data bytes read or written */
	int se;
	int isdeb = 0;
	inst_code rv = inst_ok;
	int value;
	int index;
	int flag;
	int retr;
	unsigned char buf1[8];	/* send bytes */
	unsigned char buf2[8];	/* return bytes read */

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb >= 2) fprintf(stderr,"\nspyd2: Get Refresh Rate, %d clocks\n",*clocks);

	/* Setup the triggering parameters */
	if (*clocks > 0xffffff)		/* Enforce hardware limits */
		*clocks = 0xffffff;
	if (*minfclks > 0x7fff)
		*minfclks = 0x7fff;
	if (*maxfclks > 0x7fff)
		*maxfclks = 0x7fff;
	value = *clocks >> 8;
	value = (value >> 8) | ((value << 8) & 0xff00);		/* Convert to big endian */
	index = (*clocks << 8) & 0xffff;
	index = (index >> 8) | ((index << 8) & 0xff00);		/* Convert to big endian */

	/* Setup parameters in send buffer */
	short2buf(&buf1[0], thresh);
	short2buf(&buf1[2], nframes);
	short2buf(&buf1[4], *minfclks);
	short2buf(&buf1[6], *maxfclks);

	/* Issue the triggering command */
	for (retr = 0; ; retr++) {
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xC3, value, index, buf1, 8, 5.0);

		if ((se & ICOM_USERM) || (se != ICOM_OK && retr >= RETRIES)) {
			/* Complete the operation so as not to leave the instrument in a hung state */
			msec_sleep(*clocks/MSECDIV);
			p->icom->usb_read(p->icom, 0x81, buf2, 8, &rwbytes, 1.0);

			if (isdeb) fprintf(stderr,"\nspyd2: Get Refresh Rate Trig failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		if (se != ICOM_OK) {
			/* Complete the operation so as not to leave the instrument in a hung state */
			msec_sleep(*clocks/MSECDIV);
			p->icom->usb_read(p->icom, 0x81, buf2, 8, &rwbytes, 1.0);

			msec_sleep(500);
			if (isdeb) fprintf(stderr,"\nspyd2: Get Refresh Rate Trig retry with ICOM err 0x%x\n",se);
			continue;
		}

		if (isdeb >= 2) fprintf(stderr,"Trigger Get Refresh Rate returns ICOM err 0x%x\n", se);

		/* Allow some time for the instrument to respond */
		msec_sleep(*clocks/MSECDIV);
	
		/* Now read the bytes */
		se = p->icom->usb_read(p->icom, 0x81, buf2, 8, &rwbytes, 5.0);
		if (se == ICOM_OK)
			break;
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Get Refresh Rate failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Get Refresh Rate retry with ICOM err 0x%x\n",se);
	}

	if (rwbytes != 8) {
		if (isdeb) fprintf(stderr,"\nspyd2: Get Refresh Rate got short data read %d",rwbytes);
		p->icom->debug = isdeb;
		return spyd2_interp_code((inst *)p, SPYD2_BADREADSIZE);
	}

	flag = buf2[0];
	*clkcnt = buf2uint24(&buf2[1]);

	if (p->itype != instSpyder3
	 && flag == 1) {
		if (isdeb) fprintf(stderr,"\nspyd2: Get Refresh Rate got trigger timeout");
		p->icom->debug = isdeb;
		return spyd2_interp_code((inst *)p, SPYD2_TRIGTIMEOUT);
	}

	if (p->itype != instSpyder3
	 && flag == 2) {
		if (isdeb) fprintf(stderr,"\nspyd2: Get Refresh Rate got overall timeout");
		p->icom->debug = isdeb;
		return spyd2_interp_code((inst *)p, SPYD2_OVERALLTIMEOUT);
	}

	if (isdeb >= 2) fprintf(stderr,"Get Refresh Rate got %d, returns ICOM err 0x%x\n", *clkcnt, se);
	p->icom->debug = isdeb;

	return rv;
}

/* Get a reading (low level) command */
static inst_code
spyd2_GetReading_ll(
	spyd2 *p,
	int *clocks,		/* Nominal/Maximum number of clocks to use */
	int nframes,		/* Number of frames being measured */
	int thresh,			/* Frame detection threshold */
	int *minfclks,		/* Minimum number of clocks per frame */
	int *maxfclks,		/* Maximum number of clocks per frame */
	double *sensv,		/* Return the 8 sensor readings (may be NULL) */ 
	int *maxtcnt,		/* Return the maximum transition count (may be NULL) */
	int *mintcnt		/* Return the minimum transition count (may be NULL) */
) {
	int rwbytes;			/* Data bytes read or written */
	int se;
	int isdeb = 0;
	inst_code rv = inst_ok;
	int value;
	int index;
	int flag;
	int nords, retr;
	unsigned char buf1[8];		/* send bytes */
	unsigned char buf2[9 * 8];	/* return bytes read */
	int rvals[3][8];			/* Raw values */
	int _maxtcnt = 0;			/* Maximum transition count */
	int _mintcnt = 0x7fffffff;	/* Minumum transition count */
	int i, j, k;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb >= 2) fprintf(stderr,"\nspyd2: Get Reading, clocks = %d, minfc = %d, maxfc = %d\n",*clocks,*minfclks,*maxfclks);

	/* Setup the triggering parameters */
	if (*clocks > 0xffffff)
		*clocks = 0xffffff;
	if (*minfclks > 0x7fff)
		*minfclks = 0x7fff;
	if (*maxfclks > 0x7fff)
		*maxfclks = 0x7fff;
	value = *clocks >> 8;
	value = (value >> 8) | ((value << 8) & 0xff00);		/* Convert to big endian */
	index = (*clocks << 8) & 0xffff;
	index = (index >> 8) | ((index << 8) & 0xff00);		/* Convert to big endian */

	/* Setup parameters in send buffer */
	/* (Spyder3 doesn't seem to use these. Perhaps it does its */
	/* own internal refresh detection and syncronization ?) */
	thresh *= 256;
	int2buf(&buf1[0], thresh);
	short2buf(&buf1[4], *minfclks);
	short2buf(&buf1[6], *maxfclks);

#ifdef NEVER
	if (p->itype == instSpyder3) {
		for (i = 0; i < 8; i++) {
			buf1[i] = 0;
		}
	}
#endif

	/* The Spyder comms seems especially flakey... */
	for (retr = 0, nords = 1; ; retr++, nords++) {

//int start = msec_time();

		/* Issue the triggering command */
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xC1, value, index, buf1, 8, 5.0);

		if ((se & ICOM_USERM) || (se != ICOM_OK && retr >= RETRIES)) {
			/* Complete the operation so as not to leave the instrument in a hung state */
			msec_sleep(*clocks/MSECDIV);
			for (i = 0; i < (1+9); i++)
				p->icom->usb_read(p->icom, 0x81, buf2, 8, &rwbytes, 1.0);

			if (isdeb) fprintf(stderr,"\nspyd2: Get Reading Trig failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		if (se != ICOM_OK) {
			/* Complete the operation so as not to leave the instrument in a hung state */
			msec_sleep(*clocks/MSECDIV);
			for (i = 0; i < (1+9); i++)
				p->icom->usb_read(p->icom, 0x81, buf2, 8, &rwbytes, 1.0);

			msec_sleep(500);
			if (isdeb) fprintf(stderr,"\nspyd2: Get Reading Trig retry with ICOM err 0x%x\n",se);
			continue;
		}

		if (isdeb >= 2) fprintf(stderr,"Trigger Get Reading returns ICOM code 0x%x\n", se);

//printf("~1 sleeping %d msecs\n",*clocks/MSECDIV);
		/* Allow some time for the instrument to respond */
		msec_sleep(*clocks/MSECDIV);
	
		/* Now read the first 8 bytes (status etc.) */
		se = p->icom->usb_read(p->icom, 0x81, buf2, 8, &rwbytes, 5.0);
		if ((se & ICOM_USERM) || (se != ICOM_OK && retr >= RETRIES)) {
			if (isdeb) fprintf(stderr,"\nspyd2: Get Reading Stat failed with ICOM err 0x%x\n",se);
			/* Complete the operation so as not to leave the instrument in a hung state */
			for (i = 0; i < 9; i++)
				p->icom->usb_read(p->icom, 0x81, buf2, 8, &rwbytes, 1.0);
			msec_sleep(500);

			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
//printf("~1 trig -> read = %d msec\n",msec_time() - start);

		if (se != ICOM_OK) {
			if (isdeb >= 2) fprintf(stderr,"Get Reading Stat retry with ICOM err 0x%x\n", se);
			/* Complete the operation so as not to leave the instrument in a hung state */
			for (i = 0; i < 9; i++)
				p->icom->usb_read(p->icom, 0x81, buf2, 8, &rwbytes, 1.0);
			msec_sleep(500);
			continue;		/* Retry the whole command */
		}

		if (rwbytes != 8) {
			if (isdeb) fprintf(stderr,"\nspyd2: Get Reading Stat got short data read %d",rwbytes);
			/* Complete the operation so as not to leave the instrument in a hung state */
			for (i = 0; i < 9; i++)
				p->icom->usb_read(p->icom, 0x81, buf2, 8, &rwbytes, 1.0);
			msec_sleep(500);

			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, SPYD2_BADREADSIZE);
		}
	
		flag = buf2[0];

		if (p->itype != instSpyder3
		 && flag == 1) {
			if (isdeb) fprintf(stderr,"\nspyd2: Reading Stat is trigger timeout");
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, SPYD2_TRIGTIMEOUT);
		}

		if (p->itype != instSpyder3
		 && flag == 2) {
			if (isdeb) fprintf(stderr,"\nspyd2: Reading Stat is overall timeout");
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, SPYD2_OVERALLTIMEOUT);
		}

		/* Now read the following 9 x 8 bytes of sensor data */
		for (i = 0; i < 9; i++) {

			se = p->icom->usb_read(p->icom, 0x81, buf2 + i * 8, 8, &rwbytes, 5.0);
			if ((se & ICOM_USERM) || (se != ICOM_OK && retr >= RETRIES)) {
				int ii = i;
				/* Complete the operation so as not to leave the instrument in a hung state */
				for (; ii < 9; ii++)
					p->icom->usb_read(p->icom, 0x81, buf2 + i * 8, 8, &rwbytes, 1.0);

				if (isdeb) fprintf(stderr,"\nspyd2: Get Reading failed with ICOM err 0x%x\n",se);
				p->icom->debug = isdeb;
				return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
			}
			if (se != ICOM_OK) {
				int ii = i;
				/* Complete the operation so as not to leave the instrument in a hung state */
				for (; ii < 9; ii++)
					p->icom->usb_read(p->icom, 0x81, buf2 + i * 8, 8, &rwbytes, 1.0);

				break;
			}
			if (rwbytes != 8) {
				int ii = i;
				if (isdeb) fprintf(stderr,"\nspyd2: Get Reading got short data read %d",rwbytes);

				/* Complete the operation so as not to leave the instrument in a hung state */
				for (; ii < 9; ii++)
					p->icom->usb_read(p->icom, 0x81, buf2 + i * 8, 8, &rwbytes, 1.0);

				p->icom->debug = isdeb;
				return spyd2_interp_code((inst *)p, SPYD2_BADREADSIZE);
			}
		}

		if (i >= 9)
			break;		/* We're done */

		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Get Reading retry with ICOM err 0x%x\n",se);

		if (isdeb) fprintf(stderr,"\nspyd2: Resetting end point\n");
#ifdef DO_RESETEP				/* Do the miscelanous resetep()'s */
		p->icom->usb_resetep(p->icom, 0x81);
		msec_sleep(1);			/* Let device recover ? */
#endif /*  DO_RESETEP */
	}	/* End of whole command retries */

	if (sensv == NULL) {
		p->icom->debug = isdeb;
		return rv;
	}

#ifdef NEVER
	printf("Got bytes:\n");
	for (i = 0; i < 9; i++) {
		printf(" %d: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",i * 8,
		buf2[i * 8 + 0], buf2[i * 8 + 1], buf2[i * 8 + 2], buf2[i * 8 + 3],
		buf2[i * 8 + 4], buf2[i * 8 + 5], buf2[i * 8 + 6], buf2[i * 8 + 7]);
	}
#endif

	/* Spyder 2 decoding */
	if (p->itype != instSpyder3) {
		/* Convert the raw buffer readings into 3 groups of 8 integers. */
		/* At the start of each reading, the HW starts counting master */
		/* (1MHz) clocks. When the first transition after the start of */
		/* the reading is received from a light->frequency sensor (TAOS TSL237), */
		/* the clock count is recorded, and returned as the second of the three */
		/* numbers returned for each sensor. */
		/* When the last transition before the end of the reading period is */
		/* received, the clock count is recorded, and returned as the first */
		/* of the three numbers. The integration period is therefore */
		/* the first number minus the second. */
		/* The third number is the number of transitions from the sensor */
		/* counted during the integration period. Since this 24 bit counter is */
		/* not reset between readings, the previous count is recorded (prevraw[]), */
		/* and subtracted (modulo 24 bits) from the current value. */
		/* The light level is directly proportional to the frequency, */
		/* hence the transitions-1 counted. */
		/* In the case of a CRT, the total number of clocks is assumed to be */
		/* set to an integer number of refresh cycles, and the total transitions */
		/* over that period are counted. */
		for (i = j = 0; j < 3; j++) {
			for (k = 0; k < 8; k++, i += 3) {
				rvals[j][k] = buf2uint24lens(buf2 + i);
//printf("~1 got rvals[%d][%d] = 0x%x\n",j,k,rvals[j][k]);
			}
		}

		/* And convert 3 integers per sensor into sensor values */
		for (k = 0; k < 8; k++) {
			int transcnt, intclks;

//printf("~1 sensor %d:\n",k);

			/* Compute difference of third integer to previous value */
			/* read, modulo 24 bits */
			if (p->prevraw[k] <= rvals[2][k]) {
				transcnt = rvals[2][k] - p->prevraw[k];
//printf("~1 transcnt = %d - %d = %d\n",rvals[2][k], p->prevraw[k],transcnt);
			} else {
				transcnt = rvals[2][k] + 0x1000000 - p->prevraw[k];
//printf("~1 transcnt = %d + %d - %d = %d\n",rvals[2][k], 0x1000000, p->prevraw[k],transcnt);
			}

			p->prevraw[k] = rvals[2][k];	/* New previuos value */

			/* Compute difference of first integer to second */
			intclks = rvals[0][k] - rvals[1][k];
//printf("~1 intclks = %d - %d = %d\n",rvals[0][k], rvals[1][k]);

			if (transcnt == 0 || intclks <= 0) {		/* It's too dark ... */
				sensv[k] = 0.0;
				_mintcnt = 0;
			} else {			/* Transitions within integration period */
								/* hence one is discarded ? */
				sensv[k] = ((double)transcnt - 1.0) * (double)CLKRATE
				         / ((double)nords * (double)intclks);
				if (transcnt > _maxtcnt)
					_maxtcnt = transcnt;
				if (transcnt < _mintcnt)
					_mintcnt = transcnt;
			}
			if (p->debug >= 4)
				fprintf(stderr,"%d: initial senv %f from transcnt %d and intclls %d\n",k,sensv[k],transcnt,intclks);

#ifdef NEVER	/* This seems to make repeatability worse ??? */
			/* If CRT and bright enough */
			if (sensv[k] > 1.5 && p->lcd == 0) {
				sensv[k] = ((double)transcnt) * (double)p->rrate/(double)nframes;
//printf("~1 %d: corrected senv %f\n",k,sensv[k]);
			}
#endif
		}

	/* Spyder 3 decoding */
	} else {
		/* Convert the raw buffer readings into 3 groups of 8 integers. */
		/* At the start of each reading, the HW starts counting master */
		/* (1MHz) clocks. When the first transition after the start of */
		/* the reading is received from a light->frequency sensor (TAOS TSL238T), */
		/* the clock count is recorded, and returned as the second of the three */
		/* numbers returned for each sensor. */
		/* When the last transition before the end of the reading period is */
		/* received, the clock count is recorded, and returned as the first */
		/* of the three numbers. The integration period is therefore */
		/* the first number minus the second. */
		/* The third number is the number of transitions from the sensor */
		/* counted during the integration period. */
		/* The light level is directly proportional to the frequency, */
		/* hence the transitions-1 counted. */

		int map[8] = { 0,0,1,2,5,6,7,4 };	/* Map sensors into Spyder 2 order */

		for (j = 0; j < 3; j++) {
			for (k = 0; k < 8; k++) {
				rvals[j][k] = buf2uint24le(buf2 + (j * 24 + map[k] * 3));
//printf("~1 got rvals[%d][%d] = 0x%x\n",j,k,rvals[j][k]);
			}
		}

		/* And convert 3 integers per sensor into sensor values */
		for (k = 0; k < 8; k++) {
			int transcnt, intclks;

			/* Number of sensor transitions */
			transcnt = rvals[2][k];
//printf("~1 %d: transcnt = %d\n",k, rvals[2][k]);

			/* Compute difference of first integer to second */
			intclks = rvals[0][k] - rvals[1][k];
//printf("~1 %d: intclks = %d - %d = %d\n",k, 65 * rvals[0][k]/8, 65 * rvals[1][k]/8, intclks * 65/8);

			if (transcnt == 0 || intclks <= 0) {		/* It's too dark ... */
				sensv[k] = 0.0;
				_mintcnt = 0;
			} else {			/* Transitions within integration period */
								/* hence one is discarded ? */
				sensv[k] = ((double)transcnt - 1.0) * (double)CLKRATE
				         / ((double)intclks * 8.125);
				if (transcnt > _maxtcnt)
					_maxtcnt = transcnt;
				if (transcnt < _mintcnt)
					_mintcnt = transcnt;
			}
			if (p->debug >= 4)
				fprintf(stderr,"%d: initial senv %f from transcnt %d and intclls %d\n",k,sensv[k],transcnt,intclks);
		}
	}

	if (maxtcnt != NULL)
		*maxtcnt = _maxtcnt;
	if (mintcnt != NULL)
		*mintcnt = _mintcnt;

	p->icom->debug = isdeb;
	return rv;
}

/* Spyder 3: Set the LED */
static inst_code
spyd2_setLED(
	spyd2 *p,
	int mode,			/* LED mode: 0 = off, 1 = pulse, 2 = on */
	double period		/* Pulse period in seconds */
) {
	int se;
	int isdeb = 0;
	inst_code rv = inst_ok;
	int value;
	int index;
	int retr;
	int ptime;			/* Pulse time, 1 - 255 x 20 msec */

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (mode < 0)
		mode = 0;
	else if (mode > 2)
		mode = 2;
	ptime = (int)(period/0.02 + 0.5);
	if (ptime < 0)
		ptime = 0;
	else if (ptime > 255)
		ptime = 255;

	if (isdeb >= 2) {
		if (mode == 1)
			fprintf(stderr,"\nspyd2: Set LED to pulse, %f secs\n",ptime * 0.02);
		else
			fprintf(stderr,"\nspyd2: Set LED to %s\n",mode == 0 ? "off" : "on");
	}

	value = mode;				/* Leave little endian */
	index = ptime;

	for (retr = 0; ; retr++) {
		/* Issue the trigger command */
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xF6, value, index, NULL, 0, 5.0);

		if (se == ICOM_OK) {
			if (isdeb) fprintf(stderr,"SetLED OK, ICOM code 0x%x\n",se);
			break;
		}
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: SetLED failed with  ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: SetLED retry with ICOM err 0x%x\n",se);
	}

	p->icom->debug = isdeb;

	return rv;
}


/* Spyder 3: Set the ambient control register */
static inst_code
spyd2_SetAmbReg(
	spyd2 *p,
	int val			/* 8 bit ambient config register value */
) {
	int se;
	int isdeb = 0;
	inst_code rv = inst_ok;
	int value;
	int retr;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb >= 2)
		fprintf(stderr,"\nspyd2: Set Ambient control register to %d\n",val);

	if (val < 0)
		val = 0;
	else if (val > 255)
		val = 255;
	value = val;				/* Leave little endian */

	for (retr = 0; ; retr++) {
		/* Issue the trigger command */
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xF3, value, 0, NULL, 0, 5.0);

		if (se == ICOM_OK) {
			if (isdeb) fprintf(stderr,"Set Ambient control register OK, ICOM code 0x%x\n",se);
			break;
		}
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Set Ambient control register failed with  ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Set Ambient control register retry with ICOM err 0x%x\n",se);
	}

	p->icom->debug = isdeb;

	return rv;
}

/* Spyder3: Read ambient light timing */
/* The byte value seems to be composed of:
	bits 0,1
	bits 4
*/
static inst_code
spyd2_ReadAmbTiming(
	spyd2 *p,
	int *val	/* Return the 8 bit timing value */
) {
	unsigned char pbuf[1];	/* Timing value read */
	int _val;
	int se;
	int isdeb = 0;
	int retr;
	inst_code rv = inst_ok;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nspyd2: Read Ambient Timing\n");

	for (retr = 0; ; retr++) {
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xF4, 0, 0, pbuf, 1, 5.0);

		if (se == ICOM_OK)
			break;
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Read Ambient Timing failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Read Ambient Timing retry with ICOM err 0x%x\n",se);
	}

	_val = pbuf[0];

	if (isdeb) fprintf(stderr,"Read Ambient Timing returns %d ICOM err 0x%x\n", _val, se);

	p->icom->debug = isdeb;

	if (val != NULL) *val = _val;

	return rv;
}


/* Spyder3: Read ambient light channel 0 or 1 */
static inst_code
spyd2_ReadAmbChan(
	spyd2 *p,
	int chan,	/* Ambient channel, 0 or 1 */
	int *val	/* Return the 16 bit channel value */
) {
	unsigned char pbuf[2];	/* Channel value read */
	int _val;
	int se;
	int isdeb = 0;
	int retr;
	inst_code rv = inst_ok;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	chan &= 1;

	if (isdeb) fprintf(stderr,"\nspyd2: Read Ambient channel %d\n",chan);

	for (retr = 0; ; retr++) {
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xF0 + chan, 0, 0, pbuf, 2, 5.0);

		if (se == ICOM_OK)
			break;
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Read Ambient channel failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Read Ambient channel retry with ICOM err 0x%x\n",se);
	}

	_val = buf2ushort(pbuf);

	if (isdeb) fprintf(stderr,"Read Ambient channel %d returns %d ICOM err 0x%x\n", chan, _val, se);

	p->icom->debug = isdeb;

	if (val != NULL) *val = _val;

	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Spyder3: Read temperature config */
static inst_code
spyd2_ReadTempConfig(
	spyd2 *p,
	int *val	/* Return the 8 bit config value */
) {
	unsigned char pbuf[1];	/* Config value read */
	int _val;
	int se;
	int isdeb = 0;
	int retr;
	inst_code rv = inst_ok;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nspyd2: Read Temp Config\n");

	for (retr = 0; ; retr++) {
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xE1, 0, 0, pbuf, 1, 5.0);

		if (se == ICOM_OK)
			break;
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Read Temp Config failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Read Temp Config retry with ICOM err 0x%x\n",se);
	}

	_val = pbuf[0];

	if (isdeb) fprintf(stderr,"Read Temp Config returns %d ICOM err 0x%x\n", _val, se);

	p->icom->debug = isdeb;

	if (val != NULL) *val = _val;

	return rv;
}

/* Spyder 3: Set the Temperature Config */
static inst_code
spyd2_SetTempConfig(
	spyd2 *p,
	int val			/* 8 bit temp config register value */
) {
	int se;
	int isdeb = 0;
	inst_code rv = inst_ok;
	int value;
	int retr;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb >= 2)
		fprintf(stderr,"\nspyd2: Set Temp Config to %d\n",val);

	if (val < 0)
		val = 0;
	else if (val > 255)
		val = 255;
	value = val;				/* Leave little endian */

	for (retr = 0; ; retr++) {
		/* Issue the trigger command */
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xE2, value, 0, NULL, 0, 5.0);

		if (se == ICOM_OK) {
			if (isdeb) fprintf(stderr,"Set Temp Config OK, ICOM code 0x%x\n",se);
			break;
		}
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Set Temp Config failed with  ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Set Temp Config retry with ICOM err 0x%x\n",se);
	}

	p->icom->debug = isdeb;

	return rv;
}


/* Spyder3: Read Temperature */
static inst_code
spyd2_ReadTemperature(
	spyd2 *p,
	double *val	/* Return the temperature */
) {
	unsigned char pbuf[2];	/* Temp value read */
	int ival;
	double _val;
	int se;
	int isdeb = 0;
	int retr;
	inst_code rv = inst_ok;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	if (isdeb) fprintf(stderr,"\nspyd2: Read Temperature\n");

	for (retr = 0; ; retr++) {
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xE0, 0, 0, pbuf, 2, 5.0);

		if (se == ICOM_OK)
			break;
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Read Temperature failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Read Temperature retry with ICOM err 0x%x\n",se);
	}

	ival = buf2ushort(&pbuf[0]);
	_val = (double)ival * 12.5;

	if (isdeb) fprintf(stderr,"Read Temperature returns %d (%f) ICOM err 0x%x\n", ival, _val, se);

	p->icom->debug = isdeb;

	if (val != NULL) *val = _val;

	return rv;
}

/* ============================================================ */
/* Medium level commands */

/* Read a 16 bit word from the EEProm */
static inst_code
spyd2_rd_ee_ushort(
	spyd2 *p,				/* Object */
	unsigned int *outp,		/* Where to write value */
	int addr				/* EEprom Address, 0 - 510 */
) {
	inst_code ev;
	unsigned char buf[2];
	int v, val;

	if ((ev = spyd2_readEEProm(p, buf, addr, 2)) != inst_ok)
		return ev;

	*outp = buf2ushort(buf);

	return inst_ok;
}

/* Read a 32 bit word from the EEProm */
static inst_code
spyd2_rd_ee_int(
	spyd2 *p,				/* Object */
	int *outp,				/* Where to write value */
	int addr				/* EEprom Address, 0 - 508 */
) {
	inst_code ev;
	unsigned char buf[4];
	int v, val;

	if ((ev = spyd2_readEEProm(p, buf, addr, 4)) != inst_ok)
		return ev;

	*outp = buf2int(buf);

	return inst_ok;
}

/* Read a float from the EEProm */
static inst_code
spyd2_rdreg_float(
	spyd2 *p,				/* Object */
	double *outp,			/* Where to write value */
	int addr				/* Register Address, 0 - 508 */
) {
	inst_code ev;
	int val;

	if ((ev = spyd2_rd_ee_int(p, &val, addr)) != inst_ok)
		return ev;

	*outp = IEEE754todouble((unsigned int)val);
	return inst_ok;
}

/* Special purpose float read, */
/* Read three 9 vectors of floats from the EEprom */
static inst_code
spyd2_rdreg_3x9xfloat(
	spyd2 *p,				/* Object */
	double *out0,			/* Where to write first 9 doubles */
	double *out1,			/* Where to write second 9 doubles */
	double *out2,			/* Where to write third 9 doubles */
	int addr				/* Register Address, 0 - 404 */
) {
	inst_code ev;
	unsigned char buf[3 * 9 * 4], *bp;
	int i;

	if ((ev = spyd2_readEEProm(p, buf, addr, 3 * 9 * 4)) != inst_ok)
		return ev;

	bp = buf;
	for (i = 0; i < 9; i++, bp +=4, out0++) {
		int val;
		val = buf2int(bp);
		*out0 = IEEE754todouble((unsigned int)val);
	}

	for (i = 0; i < 9; i++, bp +=4, out1++) {
		int val;
		val = buf2int(bp);
		*out1 = IEEE754todouble((unsigned int)val);
	}

	for (i = 0; i < 9; i++, bp +=4, out2++) {
		int val;
		val = buf2int(bp);
		*out2 = IEEE754todouble((unsigned int)val);
	}

	return inst_ok;
}

/* Get refresh rate command. Return 0.0 */
/* if no refresh rate can be established */
static inst_code
spyd2_GetRefRate(
	spyd2 *p,
	double *refrate		/* return the refresh rate */
) {
	inst_code ev;
	int clocks;			/* Clocks to run commands */
	int min, max;		/* min and max light intensity frequency periods */

	if (p->debug) fprintf(stderr,"spyd2: about to get the refresh rate\n");

	/* Establish the frame rate detect threshold level */
	clocks = (10 * CLKRATE)/DEFRRATE;

	if ((ev = spyd2_GetMinMax(p, &clocks, &min, &max)) != inst_ok)
		return ev; 

	if (min == 0 || max < (5 * min)) {
		if (p->debug) fprintf(stderr,"spyd2: no refresh rate detectable\n");
		*refrate = 0.0;
	} else {
		int frclocks;		/* notional clocks per frame */
		int nframes;		/* Number of frames to count */
		int thresh;			/* Frame detection threshold */
		int minfclks;		/* Minimum number of clocks per frame */
		int maxfclks;		/* Maximum number of clocks per frame */
		int clkcnt;		/* Return number of clocks for nframes frames */

		frclocks = CLKRATE/DEFRRATE;
		nframes = 50;
		thresh = (max - min)/5 + min;			/* Threshold is at 80% of max brightness */
		minfclks = frclocks/3;					/* Allow for 180 Hz */
		maxfclks = (frclocks * 5)/2;			/* Allow for 24 Hz */
		clocks = nframes * frclocks * 2;		/* Allow for 120 Hz */

		if ((ev = spyd2_GetRefRate_ll(p, &clocks, nframes, thresh, &minfclks, &maxfclks,
		                                 &clkcnt)) != inst_ok)
			return ev; 

		/* Compute the refresh rate */
		*refrate = ((double)nframes * (double)CLKRATE)/(double)clkcnt;
		if (p->debug) fprintf(stderr,"spyd2: refresh rate is %f Hz\n",*refrate);
	}
	return ev;
}

/* Do a reading. */
/* Note that the Spyder 3 seems to give USB errors on the data */
/* read if the measurement time is too small (ie. 0.2 seconds) */
/* when reading dark values. */
static inst_code
spyd2_GetReading(
	spyd2 *p,
	double *XYZ		/* return the XYZ values */
) {
	inst_code ev;
	int clocks1, clocks2;	/* Clocks to run commands */
	int min, max;		/* min and max light intensity frequency periods */
	int frclocks;		/* notional clocks per frame */
	int nframes;		/* Number of frames to measure over */
	int thresh;			/* Frame detection threshold */
	int minfclks;		/* Minimum number of clocks per frame */
	int maxfclks;		/* Maximum number of clocks per frame */
	double sensv[8];	/* The 8 final sensor value readings */
	int maxtcnt;		/* The maximum transition count measured */
	int mintcnt;		/* The minumum transition count measured */
	double a_sensv[8];	/* Accumulated sensor value readings */
	double a_w[8];		/* Accumulated sensor value weight */
	double pows[9];		/* Power combinations of initial XYZ */
	int i, j, k;
	int table;			/* Calibration table to use */

	if (p->debug) fprintf(stderr,"spyd2: about to get a reading\n");

	/* Compute number of frames for desired base read time */
	nframes = (int)(RDTIME * p->rrate + 0.5);

	/* Establish the frame rate detect threshold level */
	/* (The Spyder 3 doesn't use this ?) */
	clocks1 = (nframes * CLKRATE)/(10 * p->rrate);		/* Use 10% of measurement clocks */

	if ((ev = spyd2_GetMinMax(p, &clocks1, &min, &max)) != inst_ok)
		return ev; 

	/* Setup for measurement */
	thresh = (max - min)/5 + min;			/* Threshold is at 80% of max brightness */
	if (thresh == 0)
		thresh = 65535;						/* Set to max, otherwise reading will be 0 */
	frclocks = CLKRATE/p->rrate;
	minfclks = frclocks/3;					/* Allow for 180 Hz */
	maxfclks = (frclocks * 5)/2;			/* Allow for 24 Hz */

	table = p->lcd;
	if (p->itype == instSpyder3)
		table = 1;							/* Always use 2nd table for Spyder 3 */
											/* (First table is not noticably different */
											/*  but can have scaled values.) */
	if (p->debug >= 4)
		fprintf(stderr,"Using cal table %d\n",table);

	for (k = 0; k < 8; k++) 	/* Zero weighted average */
		a_sensv[k] = a_w[k] = 0.0;

	/* For initial and possible adaptive readings */
	for (i = 0;; i++) {
		double itime;		/* Integration time */

		clocks2 = (int)((double)nframes/p->rrate * (double)CLKRATE + 0.5);

		if ((ev = spyd2_GetReading_ll(p, &clocks2, nframes, thresh, &minfclks, &maxfclks,
		                                 sensv, &maxtcnt, &mintcnt)) != inst_ok)
			return ev; 
//printf("~1 returned number of clocks = %d\n",clocks2);

		if (p->debug) {
			for (k = 0; k < 8; k++)
				fprintf(stderr,"Sensor %d value = %f\n",k,sensv[k]);
		}

		itime = (double)clocks2 / (double)CLKRATE;
//printf("~1 reading %d was %f secs\n",i,itime);

		/* Accumulate it for weighted average */
		for (k = 0; k < 8; k++) {
			if (sensv[k] != 0.0) {		/* Skip value where we didn't get any transitions */
				a_sensv[k] += sensv[k] * itime;
				a_w[k] += itime;
			}
		}
		
#ifdef DO_ADAPTIVE
		if (i > 0)
			break;			/* Done adaptive */
		/* Decide whether to go around again */
//printf("~1 maxtcnt = %d, mintcnt = %d\n",maxtcnt,mintcnt);

		if (maxtcnt <= (50/8)) {
			nframes *= 8;
			if (p->debug) fprintf(stderr,"Using maximum integration time\n");
		} else if (maxtcnt < 50) {
			double mulf;
			mulf = 50.0/maxtcnt;
			nframes *= mulf;
			if (p->debug) fprintf(stderr,"Increasing total integration time by %.1f times\n",1+mulf);
		} else {
			break;			/* No need for another reading */
		}
#else /* !DO_ADAPTIVE */
		fprintf(stderr,"!!!! Spyder 2 DO_ADAPTIVE is off !!!!\n");
		break;
#endif /* !DO_ADAPTIVE */
	}

	/* Compute weighted average and guard against silliness */
	for (k = 0; k < 8; k++) {
		if (a_w[k] > 0.0) {
			a_sensv[k] /= a_w[k];
		}
	}

	/* Convert sensor readings to XYZ value */
	for (j = 0; j < 3; j++) {
		XYZ[j] = p->cal_A[table][j][0];		/* First entry is a constant */
		for (k = 1; k < 8; k++)
			XYZ[j] += a_sensv[k] * p->cal_A[table][j][k+1];
	}

//printf("~1 real Y = %f\n",XYZ[1]);

	if (p->debug) fprintf(stderr,"spyd2: got initial XYZ reading %f %f %f\n",XYZ[0], XYZ[1], XYZ[2]);
#ifdef LEVEL2
	/* Add "level 2" correction factors */
	pows[0] = XYZ[0];
	pows[1] = XYZ[1];
	pows[2] = XYZ[2];
	pows[3] = XYZ[0] * XYZ[1];
	pows[4] = XYZ[0] * XYZ[2];
	pows[5] = XYZ[1] * XYZ[2];
	pows[6] = XYZ[0] * XYZ[0];
	pows[7] = XYZ[1] * XYZ[1];
	pows[8] = XYZ[2] * XYZ[2];


	for (j = 0; j < 3; j++) {
		XYZ[j] = 0.0;

		for (k = 0; k < 9; k++) {
			XYZ[j] += pows[k] * p->cal_B[table][j][k];
		}
	}
	if (p->debug) fprintf(stderr,"spyd2: got 2nd level XYZ reading %f %f %f\n",XYZ[0], XYZ[1], XYZ[2]);
#endif

	/* Protect against silliness (This may stuff up averages though!) */
	for (j = 0; j < 3; j++) {
		if (XYZ[j] < 0.0)
			XYZ[j] = 0.0;
	}
	if (p->debug) fprintf(stderr,"spyd2: got final XYZ reading %f %f %f\n",XYZ[0], XYZ[1], XYZ[2]);

	return ev;
}

/* Spyder3: Do an ambient reading */

/* NOTE :- the ambient sensor is something like a TAOS TLS 2562CS. */
/* It has two sensors, one wide band and the other infra-red, */
/* the idea being to subtract them to get a rough human response. */
/* The reading is 16 bits, and the 8 bit conrol register */
/* controls gain and integration time: */

/* Bits 0,1 inttime, 0 = scale 0.034, 1 = scale 0.252. 2 = scale 1, 3 = manual */
/* Bit 2, manual, 0 = stop int, 1 = start int */
/* Bit 4, gain, 0 = gain 1, 1 = gain 16 */


static inst_code
spyd2_GetAmbientReading(
	spyd2 *p,
	double *XYZ		/* return the ambient XYZ values */
) {
	inst_code ev = inst_ok;
	int tconf;		/* Ambient timing config */
	int iamb0, iamb1;
	double amb0, amb1;	/* The two ambient values */
	double trv;		/* Trial value */
	double thr[8] = { 64/512.0,  128/512.0, 192/512.0, 256/512.0,	/* Magic tables */
	                  312/512.0, 410/512.0, 666/512.0, 0 };
	double s0[8]  = { 498/128.0, 532/128.0, 575/128.0, 624/128.0,
	                  367/128.0, 210/128.0, 24/128.0,  0 };
	double s1[8]  = { 446/128.0, 721/128.0, 891/128.0, 1022/128.0,
	                  508/128.0, 251/128.0, 18/128.0,  0 };
	double amb;		/* Combined ambient value */
	double sfact;
	int i;

	if (p->debug) fprintf(stderr,"spyd2: about to get an ambient reading\n");

	/* Set the ambient control register to 3 */
	if ((ev = spyd2_SetAmbReg(p, 3)) != inst_ok)
		return ev;

	/* Wait one second */
	msec_sleep(1000);

	/* Read the ambient timing config value */
	if ((ev = spyd2_ReadAmbTiming(p, &tconf)) != inst_ok)
		return ev;
//printf("~1 ambient timing = %d\n",tconf);

	/* Read the ambient values */
	if ((ev = spyd2_ReadAmbChan(p, 0, &iamb0)) != inst_ok)
		return ev;
	if ((ev = spyd2_ReadAmbChan(p, 1, &iamb1)) != inst_ok)
		return ev;

//printf("~1 ambient values = %d, %d\n",iamb0,iamb1);
	amb0 = iamb0/128.0;
	amb1 = iamb1/128.0;

	/* Set the ambient control register to 0 */
	if ((ev = spyd2_SetAmbReg(p, 0)) != inst_ok)
		return ev;

	/* Compute the scale factor from the timing config value */
	if ((tconf & 3) == 0)
		sfact = 1.0/0.034;
	else if ((tconf & 3) == 1)
		sfact = 1.0/0.252;
	else
		sfact = 1.0;

	if ((tconf & 0x10) == 0)
		sfact *= 16.0;

	amb0 *= sfact;
	amb1 *= sfact;

	if (amb0 > 0.0)
		trv = amb1/amb0;
	else
		trv = 0.0;
	
	for (i = 0; i < 7; i++) {
		if (trv <= thr[i])
			break;
	}
//printf("~1 trv = %f, s0 = %f, s1 = %f\n",trv, s0[i],s1[i]);
	/* Compute ambient in Lux */
	amb = s0[i] * amb0 - s1[i] * amb1;
		
//printf("~1 combined ambient = %f Lux\n",amb);
	/* Compute the Y value */

	XYZ[1] = amb;						/* cd/m^2 ??? - not very accurate, due to */
										/* spectral response and/or integration angle. */
	XYZ[0] = icmD50.X * XYZ[1];			/* Convert to D50 neutral */
	XYZ[2] = icmD50.Z * XYZ[1];

	if (p->debug) fprintf(stderr,"spyd2: got ambient reading %f %f %f\n",XYZ[0],XYZ[1],XYZ[2]);

	return ev;
}

/* ------------------------------------------------------------ */

/* Read all the relevant register values */
static inst_code
spyd2_read_all_regs(
	spyd2 *p				/* Object */
) {
	inst_code ev;

	if (p->debug) fprintf(stderr,"spyd2: about to read all the EEProm values\n");

	/* HW version */
	if ((ev = spyd2_rd_ee_ushort(p, &p->hwver, 5)) != inst_ok)
		return ev;

	if (p->debug >= 1) fprintf(stderr,"hwver = 0x%x\n",p->hwver);

	/* Serial number */
	if ((ev = spyd2_readEEProm(p, (unsigned char *)p->serno, 8, 8)) != inst_ok)
		return ev;
	p->serno[8] = '\000';
	if (p->debug >= 4) fprintf(stderr,"serno = '%s'\n",p->serno);

	/* Spyde2: CRT calibration values */
	/* Spyde3: Unknown calibration values */
	if ((ev = spyd2_rdreg_3x9xfloat(p, p->cal_A[0][0], p->cal_A[0][1], p->cal_A[0][2], 16))
	                                                                            != inst_ok)
		return ev;
	if ((ev = spyd2_rdreg_3x9xfloat(p, p->cal_B[0][0], p->cal_B[0][1], p->cal_B[0][2], 128))
	                                                                            != inst_ok)
		return ev;

	/* Hmm. The 0 table seems to sometimes be scaled. Is this a bug ? */
	/* The spyder 3 doesn't use this anyway. */
	if (p->itype == instSpyder3) {
		int j, k, i;
		double avgmag = 0.0;
	
		for (i = j = 0; j < 3; j++) {
			for (k = 0; k < 9; k++) {
				if (p->cal_A[0][j][k] != 0.0) {
					avgmag += fabs(p->cal_A[0][j][k]);
					i++;
				}
			}
		}
		avgmag /= (double)(i);
		if (p->debug >= 4) fprintf(stderr,"Cal_A avgmag = %f\n",avgmag);

		if (avgmag < 0.05) {
			if (p->debug >= 4) fprintf(stderr,"Scaling Cal_A by 16\n");
			for (j = 0; j < 3; j++) {
				for (k = 0; k < 9; k++) {
					p->cal_A[0][j][k] *= 16.0;
				}
			}
		}
	}

	/* Spyder2: LCD calibration values */
	/* Spyder3: Normal calibration values */
	if ((ev = spyd2_rdreg_3x9xfloat(p, p->cal_A[1][0], p->cal_A[1][1], p->cal_A[1][2], 256))
	                                                                            != inst_ok)
		return ev;
	if ((ev = spyd2_rdreg_3x9xfloat(p, p->cal_B[1][0], p->cal_B[1][1], p->cal_B[1][2], 384))
	                                                                            != inst_ok)
		return ev;

	/* Luminence only calibration values ??? */
	if ((ev = spyd2_rdreg_float(p, &p->cal_F[0], 240)) != inst_ok)
		return ev;
	if ((ev = spyd2_rdreg_float(p, &p->cal_F[1], 244)) != inst_ok)
		return ev;
	if ((ev = spyd2_rdreg_float(p, &p->cal_F[2], 248)) != inst_ok)
		return ev;
	if ((ev = spyd2_rdreg_float(p, &p->cal_F[3], 252)) != inst_ok)
		return ev;
	if ((ev = spyd2_rdreg_float(p, &p->cal_F[4], 364)) != inst_ok)
		return ev;
	if ((ev = spyd2_rdreg_float(p, &p->cal_F[5], 368)) != inst_ok)
		return ev;
	if ((ev = spyd2_rdreg_float(p, &p->cal_F[6], 372)) != inst_ok)
		return ev;

	if (p->debug >= 4) {
		int i, j, k;

		
		fprintf(stderr,"Cal_A:\n");
		for (i = 0; i < 2;i++) {
			for (j = 0; j < 3; j++) {
				for (k = 0; k < 9; k++) {
					fprintf(stderr,"Cal_A [%d][%d][%d] = %f\n",i,j,k,p->cal_A[i][j][k]);
				}
			}
		}
		fprintf(stderr,"\nCal_B:\n");
		for (i = 0; i < 2;i++) {
			for (j = 0; j < 3; j++) {
				for (k = 0; k < 9; k++) {
					fprintf(stderr,"Cal_B [%d][%d][%d] = %f\n",i,j,k,p->cal_B[i][j][k]);
				}
			}
		}
		fprintf(stderr,"\nCal_F:\n");
		for (i = 0; i < 7;i++) {
			fprintf(stderr,"Cal_F [%d] = %f\n",i,p->cal_F[i]);
		}
		fprintf(stderr,"\n");
	}

	if (p->debug) fprintf(stderr,"spyd2: all EEProm read OK\n");

	return inst_ok;
}


/* Table to hold Spyder 2 Firmware, if it's installed */
unsigned int _spyder2_pld_size = 0;  /* Number of bytes to download */
unsigned int *spyder2_pld_size = &_spyder2_pld_size;
unsigned char *spyder2_pld_bytes = NULL;

/* Spyder 2: Download the PLD if it is available, and check status */
static inst_code
spyd2_download_pld(
	spyd2 *p				/* Object */
) {
	inst_code ev;
	int stat;
	int i;

	if (p->debug) fprintf(stderr,"spyd2: about to download the PLD pattern\n");

	if (*spyder2_pld_size == 0 || *spyder2_pld_size == 0x11223344) {
		if (p->debug) fprintf(stderr,"spyd2: No PLD pattern available! (have you run spyd2en ?)\n");
		return spyd2_interp_code((inst *)p, SPYD2_NO_PLD_PATTERN) ;
	}
		
	for (i = 0; i < *spyder2_pld_size; i += 8) {
		if ((ev = spyd2_loadPLD(p, spyder2_pld_bytes + i, 8)) != inst_ok)
			return ev;
	}

	/* Let the PLD initialize */
	msec_sleep(500);
		
#ifdef DO_RESETEP				/* Do the miscelanous resetep()'s */
	/* Reset the coms */
	p->icom->usb_resetep(p->icom, 0x81);
	msec_sleep(1);			/* Let device recover ? */
#endif /*  DO_RESETEP */

	/* Check the status */
	if ((ev = spyd2_getstatus(p, &stat)) != inst_ok)
		return ev;

	if (stat != 0) {
		if (p->debug) fprintf(stderr,"spyd2: PLD download failed!\n");
		return spyd2_interp_code((inst *)p, SPYD2_PLDLOAD_FAILED);
	}

	if (p->debug) fprintf(stderr,"spyd2: PLD pattern downloaded\n");

	msec_sleep(500);
#ifdef DO_RESETEP				/* Do the miscelanous resetep()'s */
	p->icom->usb_resetep(p->icom, 0x81);
	msec_sleep(1);			/* Let device recover ? */
#endif /*  DO_RESETEP */

	return inst_ok;
}


/* ============================================================ */


/* Establish communications with a SPYD2 */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
spyd2_init_coms(inst *pp, int port, baud_rate br, flow_control fc, double tout) {
	spyd2 *p = (spyd2 *) pp;
	char buf[16];
	int rsize;
	long etime;
	int bi, i, rv;
	icomuflags usbflags = icomuf_none;
	inst_code ev = inst_ok;

	if (p->debug) {
		p->icom->debug = p->debug;	/* Turn on debugging */
		fprintf(stderr,"spyd2: About to init coms\n");
	}

	if (p->icom->is_usb_portno(p->icom, port) == instUnknown) {
		if (p->debug) fprintf(stderr,"spyd2: init_coms called to wrong device!\n");
			return spyd2_interp_code((inst *)p, SPYD2_UNKNOWN_MODEL);
	}

	if (p->debug) fprintf(stderr,"spyd2: About to init USB\n");

	/* On MSWindows the Spyder 3 doesn't work reliably unless each */
	/* read is preceeded by a reset endpoint. */
	/* (!!! This needs checking to see if it's still true. */
	/*  Should switch back to libusb0.sys and re-test.) */
	/* (and Spyder 2 hangs if a reset ep is done on MSWin.) */
	/* The spyder 2 doesn't work well with the winusb driver either, */
	/* it needs icomuf_resetep_before_read to work at all, and */
	/* gets retries anyway. So we use the libusb0 driver for it. */
#if defined(NT)
	if (p->prelim_itype == instSpyder3) {
		usbflags |= icomuf_resetep_before_read;		/* The spyder USB is buggy ? */
	}
#endif

	/* On OS X the Spyder 2 can't close properly */
#if defined(UNIX) && defined(__APPLE__)			/* OS X*/
	if (p->prelim_itype != instSpyder3) {
		usbflags |= icomuf_reset_before_close;		/* The spyder 2 USB is buggy ? */
	}
#endif

#ifdef NEVER	/* Don't want this now that we avoid 2nd set_config on Linux */
#if defined(UNIX) && !defined(__APPLE__)		/* Linux*/
	/* On Linux the Spyder 2 doesn't work reliably unless each */
	/* read is preceeded by a reset endpoint. */
	if (p->prelim_itype != instSpyder3) {
		usbflags |= icomuf_resetep_before_read;		/* The spyder USB is buggy ? */
	}
#endif
#endif

	/* Set config, interface, write end point, read end point */
	/* ("serial" end points aren't used - the spyd2lay uses USB control messages) */
	p->icom->set_usb_port(p->icom, port, 1, 0x00, 0x00, usbflags, 0, NULL); 

	if (p->debug) fprintf(stderr,"spyd2: init coms has suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

/* Initialise the SPYD2 */
/* return non-zero on an error, with an inst_code */
static inst_code
spyd2_init_inst(inst *pp) {
	spyd2 *p = (spyd2 *)pp;
	inst_code ev = inst_ok;
	int stat;
	int i;

	if (p->debug) fprintf(stderr,"spyd2: About to init instrument\n");

	if (p->gotcoms == 0) /* Must establish coms before calling init */
		return spyd2_interp_code((inst *)p, SPYD2_NO_COMS);

	if (p->prelim_itype != instSpyder2
	 && p->prelim_itype != instSpyder3)
		return spyd2_interp_code((inst *)p, SPYD2_UNKNOWN_MODEL);

	p->itype = p->prelim_itype;

	p->rrset = 0;
	p->rrate = DEFRRATE;
	for (i = 0; i < 8; i++)
		p->prevraw[i] = 0;			/* Internal counters will be reset */

	/* For Spyder 1 & 2, reset the hardware and wait for it to become ready. */
	if (p->itype != instSpyder3) {

		/* Reset the instrument */
		if ((ev = spyd2_reset(p)) != inst_ok)
			return ev;

		/* Fetch status until we get a status = 1 */
		for (i = 0; i < 50; i++) {
			if ((ev = spyd2_getstatus(p, &stat)) != inst_ok)
				return ev;
	
			if (stat == 1)
				break;
		}
		if (i >= 50)
			return spyd2_interp_code((inst *)p, SPYD2_BADSTATUS);

	} else {
		/* Because the Spyder 3 doesn't have a reset command, */
		/* it may be left in a borked state if the driver is aborted. */
		/* Make sure there's no old read data hanging around. */
		/* Sometimes it takes a little while for the old data to */
		/* turn up, so try at least for 1 second. */
		/* This won't always work if the driver is re-started */
		/* quickly after aborting a long integration read. */

		unsigned char buf[8];	/* return bytes read */
		int rwbytes;			/* Data bytes read or written */

		
		for (i = 0; i < 50; i++) {
			if ((p->icom->usb_read(p->icom, 0x81, buf, 8, &rwbytes, 0.1) & ICOM_TO)
			 && i > 9)
				break; 		/* Done when read times out */
		}
	}

	/* Read the Serial EEProm contents */
	if ((ev = spyd2_read_all_regs(p)) != inst_ok)
		return ev;

	if (p->itype != instSpyder3) {
		/* Download the PLD pattern and check the status */
		if ((ev = spyd2_download_pld(p)) != inst_ok)
			return ev;
	}

	/* Do a dumy sensor read */
	{
		int clocks = 500;
		int minfclks = 0;
		int maxfclks = 0;
		msec_sleep(100);
		if ((ev = spyd2_GetReading_ll(p, &clocks, 10, 0, &minfclks, &maxfclks, NULL, NULL, NULL)) != inst_ok)
			return ev;
	}

	p->trig = inst_opt_trig_keyb;		/* default trigger mode */

	if (ev == inst_ok) {
		p->inited = 1;
		if (p->debug) fprintf(stderr,"spyd2: instrument inited OK\n");
	}

	
	if (p->itype == instSpyder3) {
		/* Flash the LED, just cos we can! */
		if ((ev = spyd2_setLED(p, 2, 0.0)) != inst_ok)
			return ev;
		msec_sleep(200);
		if ((ev = spyd2_setLED(p, 0, 0.0)) != inst_ok)
			return ev;
	}

	if (p->verb) {
		printf("Instrument Type:   %s\n",inst_name(p->itype));
		printf("Serial Number:     %s\n",p->serno);
		printf("Hardware version:  0x%04x\n",p->hwver);
	}

	return ev;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
spyd2_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val) {		/* Pointer to instrument patch value */
	spyd2 *p = (spyd2 *)pp;
	int user_trig = 0;
	inst_code ev = inst_protocol_error;

	if (p->trig == inst_opt_trig_keyb) {
		int se;
		if ((se = icoms_poll_user(p->icom, 1)) != ICOM_TRIG) {
			/* Abort, term or command */
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		user_trig = 1;
		if (p->trig_return)
			printf("\n");
	}

	if ((p->mode & inst_mode_measurement_mask) == inst_mode_emis_ambient) {
		if ((ev = spyd2_GetAmbientReading(p, val->aXYZ)) != inst_ok)
			return ev;

	} else {

		/* Attemp a CRT frame rate calibration if needed */
		if (p->lcd == 0 && p->rrset == 0) { 
			double refrate;

			if ((ev = spyd2_GetRefRate(p, &refrate)) != inst_ok)
				return ev; 
			if (refrate != 0.0) {
				p->rrate = refrate;
				p->rrset = 1;
			}
		}

		/* Read the XYZ value */
		if ((ev = spyd2_GetReading(p, val->aXYZ)) != inst_ok)
			return ev;

		/* Apply the colorimeter correction matrix */
		icmMulBy3x3(val->aXYZ, p->ccmat, val->aXYZ);
	}

	val->XYZ_v = 0;
	val->aXYZ_v = 1;		/* These are absolute XYZ readings ? */
	val->Lab_v = 0;
	val->sp.spec_n = 0;
	val->duration = 0.0;

	if (user_trig)
		return inst_user_trig;
	return ev;
}

/* Insert a colorimetric correction matrix in the instrument XYZ readings */
/* This is only valid for colorimetric instruments. */
/* To remove the matrix, pass NULL for the filter filename */
inst_code spyd2_col_cor_mat(
inst *pp,
double mtx[3][3]
) {
	spyd2 *p = (spyd2 *)pp;

	if (mtx == NULL)
		icmSetUnity3x3(p->ccmat);
	else
		icmCpy3x3(p->ccmat, mtx);
		
	return inst_ok;
}

/* Determine if a calibration is needed. Returns inst_calt_none if not, */
/* inst_calt_unknown if it is unknown, or inst_calt_crt_freq */
/* if the display refresh rate has not bee determined, */
/* and we are in CRT mode */
inst_cal_type spyd2_needs_calibration(inst *pp) {
	spyd2 *p = (spyd2 *)pp;

	if (p->lcd == 0 && p->rrset == 0)
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
inst_code spyd2_calibrate(
inst *pp,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	spyd2 *p = (spyd2 *)pp;
	inst_code ev = inst_ok;

	id[0] = '\000';

	/* Translate default into what's needed or expected default */
	if (calt == inst_calt_all) {
		if (p->lcd == 0)
			calt = inst_calt_crt_freq;
	}

	if (calt == inst_calt_crt_freq && p->lcd == 0) {
		double refrate;

		if (*calc != inst_calc_disp_white) {
			*calc = inst_calc_disp_white;
			return inst_cal_setup;
		}

		/* Do CRT frame rate calibration */
		if ((ev = spyd2_GetRefRate(p, &refrate)) != inst_ok)
			return ev; 

		if (refrate == 0.0) {
			p->rrate = DEFRRATE;
		} else {
			p->rrate = refrate;
			p->rrset = 1;
		}

		return inst_ok;
	}

	return inst_unsupported;
}

/* Error codes interpretation */
static char *
spyd2_interp_error(inst *pp, int ec) {
//	spyd2 *p = (spyd2 *)pp;
	ec &= inst_imask;
	switch (ec) {
		case SPYD2_INTERNAL_ERROR:
			return "Non-specific software internal software error";
		case SPYD2_COMS_FAIL:
			return "Communications failure";
		case SPYD2_UNKNOWN_MODEL:
			return "Not a Spyder 2 or 3";
		case SPYD2_DATA_PARSE_ERROR:
			return "Data from i1 Display didn't parse as expected";
		case SPYD2_USER_ABORT:
			return "User hit Abort key";
		case SPYD2_USER_TERM:
			return "User hit Terminate key";
		case SPYD2_USER_TRIG:
			return "User hit Trigger key";
		case SPYD2_USER_CMND:
			return "User hit a Command key";

		case SPYD2_OK:
			return "No device error";

		case SPYD2_BADSTATUS:
			return "Too many retries waiting for status to come good";
		case SPYD2_PLDLOAD_FAILED:
			return "Wrong status after download of PLD";
		case SPYD2_BADREADSIZE:
			return "Didn't read expected amount of data";
		case SPYD2_TRIGTIMEOUT:
			return "Trigger timout";
		case SPYD2_OVERALLTIMEOUT:
			return "Overall timout";

		/* Internal errors */
		case SPYD2_BAD_EE_ADDRESS:
			return "Serial EEProm read is out of range 0 - 511";
		case SPYD2_BAD_EE_SIZE:
			return "Serial EEProm read size > 256";
		case SPYD2_NO_PLD_PATTERN:
			return "No PLD firmware pattern is available (have you run spyd2en ?)";
		case SPYD2_NO_COMS:
			return "Communications hasn't been established";
		case SPYD2_NOT_INITED:
			return "Insrument hasn't been initialised";
		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
spyd2_interp_code(inst *pp, int ec) {
//	spyd2 *p = (spyd2 *)pp;

	ec &= inst_imask;
	switch (ec) {

		case SPYD2_OK:
			return inst_ok;

		case SPYD2_INTERNAL_ERROR:
		case SPYD2_NO_COMS:
		case SPYD2_NOT_INITED:
		case SPYD2_BAD_EE_ADDRESS:
		case SPYD2_BAD_EE_SIZE:
		case SPYD2_NO_PLD_PATTERN:
			return inst_internal_error | ec;

		case SPYD2_COMS_FAIL:
		case SPYD2_BADREADSIZE:
		case SPYD2_TRIGTIMEOUT:
		case SPYD2_BADSTATUS:
		case SPYD2_OVERALLTIMEOUT:
			return inst_coms_fail | ec;

		case SPYD2_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

//		return inst_protocol_error | ec;

		case SPYD2_USER_ABORT:
			return inst_user_abort | ec;
		case SPYD2_USER_TERM:
			return inst_user_term | ec;
		case SPYD2_USER_TRIG:
			return inst_user_trig | ec;
		case SPYD2_USER_CMND:
			return inst_user_cmnd | ec;

		case SPYD2_PLDLOAD_FAILED:
			return inst_hardware_fail | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
spyd2_del(inst *pp) {
	spyd2 *p = (spyd2 *)pp;
	if (p->icom != NULL)
		p->icom->del(p->icom);
	free(p);
}

/* Return the instrument capabilities */
inst_capability spyd2_capabilities(inst *pp) {
	spyd2 *p = (spyd2 *)pp;
	inst_capability rv = 0;

	rv = inst_emis_spot
	   | inst_emis_disp
	   | inst_colorimeter
	   | inst_ccmx
	   | inst_emis_disp_crt
	   | inst_emis_disp_lcd
	     ;

	if (p->itype == instSpyder3
	 && (p->hwver & 0x8) == 0) {	/* Not Spyder3Express */
// ~~~999 this doesn't seem right ????
		rv |= inst_emis_ambient;
		rv |= inst_emis_ambient_mono;
	}

	return rv;
}

/* Return the instrument capabilities 2 */
inst2_capability spyd2_capabilities2(inst *pp) {
	spyd2 *p = (spyd2 *)pp;
	inst2_capability rv = 0;

	rv = inst2_cal_crt_freq
	   | inst2_prog_trig
	   | inst2_keyb_trig
	     ;

	if (p->itype == instSpyder3) {
		rv |= inst2_has_leds;
	}
	return rv;
}

/* Set device measurement mode */
inst_code spyd2_set_mode(inst *pp, inst_mode m)
{
	spyd2 *p = (spyd2 *)pp;
	inst_mode mm;		/* Measurement mode */

	/* The measurement mode portion of the mode */
	mm = m & inst_mode_measurement_mask;

	if (p->itype != instSpyder3) {

		/* only display emission mode supported */
		if (mm != inst_mode_emis_spot
		 && mm != inst_mode_emis_disp) {
			return inst_unsupported;
		}

	} else {
		/* only display emission mode and ambient supported */
		if (mm != inst_mode_emis_spot
		 && mm != inst_mode_emis_disp
		 && ((p->hwver & 0x8) != 0 || mm != inst_mode_emis_ambient)) {
			return inst_unsupported;
		}
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
spyd2_set_opt_mode(inst *pp, inst_opt_mode m, ...)
{
	spyd2 *p = (spyd2 *)pp;
	inst_code ev = inst_ok;

	if (m == inst_opt_disp_crt) {
		if (p->lcd == 1)
			p->rrset = 0;		/* This is a hint we may have swapped displays */
		p->lcd = 0;
		return inst_ok;
	} else if (m == inst_opt_disp_lcd) {
		if (p->lcd != 1)
			p->rrset = 0;		/* This is a hint we may have swapped displays */
		p->lcd = 1;
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

	/* Operate the LED */
	if (p->itype == instSpyder3) {
		if (m == inst_opt_get_gen_ledmask) {
			va_list args;
			int *mask = NULL;
	
			va_start(args, m);
			mask = va_arg(args, int *);
			va_end(args);
			*mask = 0x1;			/* One general LED */
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
			mask = 1 & va_arg(args, int);
			va_end(args);
			if ((ev = spyd2_setLED(p, mask & 1 ? 2 : 0, 0.0)) == inst_ok) {
				p->led_state = mask;
			}
			return ev;
		}
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
		int mode;

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
		if (period == 0.0 || on_time_prop == 0.0) {
			period = 0.0;
			mode = 0;
			p->led_state = 0;
		} else {
			mode = 1;
			p->led_state = 1;
		}
		p->led_period = period;
		p->led_on_time_prop = on_time_prop;
		p->led_trans_time_prop = trans_time_prop;
		return spyd2_setLED(p, mode, period);
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

/* Constructor */
extern spyd2 *new_spyd2(icoms *icom, int debug, int verb)
{
	spyd2 *p;
	if ((p = (spyd2 *)calloc(sizeof(spyd2),1)) == NULL)
		error("spyd2: malloc failed!");

	if (icom == NULL)
		p->icom = new_icoms();
	else
		p->icom = icom;

	p->debug = debug;
	p->verb = verb;

	icmSetUnity3x3(p->ccmat);	/* Set the colorimeter correction matrix to do nothing */

	p->init_coms         = spyd2_init_coms;
	p->init_inst         = spyd2_init_inst;
	p->capabilities      = spyd2_capabilities;
	p->capabilities2     = spyd2_capabilities2;
	p->set_mode          = spyd2_set_mode;
	p->set_opt_mode      = spyd2_set_opt_mode;
	p->read_sample       = spyd2_read_sample;
	p->needs_calibration = spyd2_needs_calibration;
	p->calibrate         = spyd2_calibrate;
	p->col_cor_mat       = spyd2_col_cor_mat;
	p->interp_error      = spyd2_interp_error;
	p->del               = spyd2_del;

	p->itype = instUnknown;		/* Until initalisation */

	return p;
}

