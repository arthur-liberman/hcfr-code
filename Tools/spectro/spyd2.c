
/* 
 * Argyll Color Correction System
 *
 * Datacolor/ColorVision Spyder 2/3/4 related software.
 *
 * Author: Graeme W. Gill
 * Date:   17/9/2007
 *
 * Copyright 2006 - 2012, Graeme W. Gill
 * All rights reserved.
 *
 * (Based initially on i1disp.c)
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/*
	IMPORTANT NOTES:

    The Spyder 2 instrument cannot function without the driver software
	having access to the vendor supplied PLD firmware pattern for it.
    This firmware is not provided with Argyll, since it is not available
    under a compatible license.

    The purchaser of a Spyder 2 instrument should have received a copy
    of this firmware along with their instrument, and should therefore be able to
    enable the Argyll driver for this instrument by using the spyd2en utility
	to create a spyd2PLD.bin file.

	[ The Spyder 3 & 4 don't need a PLD firmware file. ]



    The Spyder 4 instrument will not have the full range of manufacturer
	calibration settings available without the vendor calibration data.
    This calibration day is not provided with Argyll, since it is not
	available under a compatible license.

    The purchaser of a Spyder 4 instrument should have received a copy
    of this calibration data along with their instrument, and should therefore
	be able to enable the use of the full range of calibration settings
	by using the spyd4en utility to create a spyd4cal.bin file.

	Alternatively, you can use Argyll .ccss files to set a Spyder 4
	calibration.

	[ The Spyder 2 & 3 don't need a calibration data file. ]

	The hwver == 5 code is not fully implemented.
	It's not possible to get it going without an instrument to verify on.
	(Perhaps it has only 4 sensors ?) 

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
#include <fcntl.h>
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

#undef PLOT_SPECTRA        /* Plot the sensor senitivity spectra */
#undef SAVE_SPECTRA			/* Save the sensor senitivity spectra to "sensors.sp" */

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
# define RINTTIME 1.0			/* Base time to integrate reading over - refresh display */ 
# define NINTTIME 1.0			/* Base time to integrate reading over - non-refresh display */ 
#else /* !DO_ADAPTIVE */
# define RINTTIME 5.0			/* Integrate over fixed longer time (manufacturers default) */ 
# define NINTTIME 5.0			/* Integrate over fixed longer time (manufacturers default) */ 
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

/* Take a word sized buffer, and convert it to an unsigned int big endian. */
static unsigned int buf2uint(unsigned char *buf) {
	unsigned int val;
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

/* Take a 64 sized return buffer, and convert it to a ORD64 */
static ORD64 buf2ord64(unsigned char *buf) {
	ORD64 val;
	val = buf[7];
	val = ((val << 8) + (0xff & buf[6]));
	val = ((val << 8) + (0xff & buf[5]));
	val = ((val << 8) + (0xff & buf[4]));
	val = ((val << 8) + (0xff & buf[3]));
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[0]));
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
	int addr,	/* Serial EEprom address, 0 - 1023 */
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

	if (addr < 0
	 || (p->hwver < 7 && (addr + size) > 512)
	 || (p->hwver == 7 && (addr + size) > 1024))
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

	if (addr < 0
	 || (p->hwver < 7 && (addr + size) > 512)
	 || (p->hwver == 7 && (addr + size) > 1024))
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

/* Get minmax command. */
/* Figures out the current minimum and maximum frequency periods */
/* so as to be able to set a frame detect threshold. */
/* Note it returns 0,0 if there is not enough light. */ 
/* (The light to frequency output period size is inversly */
/*  related to the lightness level) */
/* (This isn't used by the manufacturers Spyder3/4 driver, */
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

	/* Spyder2 */
	if (p->hwver < 4 && flag == 1) {
		if (isdeb) fprintf(stderr,"\nspyd2: Get Refresh Rate got trigger timeout");
		p->icom->debug = isdeb;
		return spyd2_interp_code((inst *)p, SPYD2_TRIGTIMEOUT);
	}

	/* Spyder2 */
	if (p->hwver < 4 && flag == 2) {
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
	if (p->hwver == 5) {		/* Hmm. not sure if this is right */
		value /= 1000;
	}
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

		/* Spyder2 */
		if (p->hwver < 4 && flag == 1) {
			if (isdeb) fprintf(stderr,"\nspyd2: Reading Stat is trigger timeout");
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, SPYD2_TRIGTIMEOUT);
		}

		/* Spyder2 */
		if (p->hwver < 4 && flag == 2) {
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
	/* Spyder2 */
	if (p->hwver < 4) {
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
			if (sensv[k] > 1.5 && p->ref != 0) {
				sensv[k] = ((double)transcnt) * (double)p->rrate/(double)nframes;
//printf("~1 %d: corrected senv %f\n",k,sensv[k]);
			}
#endif
		}

	/* Spyder 3/4 decoding */
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

/* Spyder3/4: Read ambient light timing */
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


/* Spyder3/4: Read ambient light channel 0 or 1 */
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

/* Spyder3/4: Read temperature config */
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

/* Spyder 3/4: Write Register */
static inst_code
spyd2_WriteReg(
	spyd2 *p,
	int reg,
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
		fprintf(stderr,"\nspyd2: Write val %d to Register %d\n",reg, val);

	if (val < 0)
		val = 0;
	else if (val > 255)
		val = 255;
	value = val;				/* Leave little endian */

	reg &= 0xff;
	value |= (reg << 8);

	for (retr = 0; ; retr++) {
		/* Issue the trigger command */
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xE2, value, 0, NULL, 0, 5.0);

		if (se == ICOM_OK) {
			if (isdeb) fprintf(stderr,"Write Register OK, ICOM code 0x%x\n",se);
			break;
		}
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Write Register failed with  ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Write Register retry with ICOM err 0x%x\n",se);
	}

	p->icom->debug = isdeb;

	return rv;
}


/* Spyder3/4: Read Register */
static inst_code
spyd2_ReadRegister(
	spyd2 *p,
	int reg,
	int *pval	/* Return the register value */
) {
	unsigned char pbuf[2];	/* Temp value read */
	int ival;
//	double _val;
	int se;
	int isdeb = 0;
	int retr;
	inst_code rv = inst_ok;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	p->icom->debug = 0;

	reg &= 0xff;

	if (isdeb) fprintf(stderr,"\nspyd2: Read Register %d\n",reg);

	for (retr = 0; ; retr++) {
		se = p->icom->usb_control(p->icom,
		               USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
	                   0xE0, reg, 0, pbuf, 2, 5.0);

		if (se == ICOM_OK)
			break;
		if ((se & ICOM_USERM) || retr >= RETRIES ) {
			if (isdeb) fprintf(stderr,"\nspyd2: Read Register failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return spyd2_interp_code((inst *)p, icoms2spyd2_err(se));
		}
		msec_sleep(500);
		if (isdeb) fprintf(stderr,"\nspyd2: Read Register retry with ICOM err 0x%x\n",se);
	}

	ival = buf2ushort(&pbuf[0]);
//	_val = (double)ival * 12.5;		/* Read temperature */

	if (isdeb) fprintf(stderr,"Read Register %d returns %d ICOM err 0x%x\n", ival, se);

	p->icom->debug = isdeb;

	if (pval != NULL) *pval = ival;

	return rv;
}


/* hwver == 5, set gain */
/* Valid gain values 1, 4, 16, 64 */
static inst_code
spyd2_SetGain(
	spyd2 *p,
	int gain
) {
	int gv = 0;

	p->gain = (double)gain;

	switch (gain) {
		case 1:
			gv = 0;
			break;
		case 4:
			gv = 16;
			break;
		case 16:
			gv = 32;
			break;
		case 64:
			gv = 48;
			break;
	}
	return spyd2_WriteReg(p, 7, gv); 
}

/* ============================================================ */
/* Medium level commands */

/* Read a 8 bits from the EEProm */
static inst_code
spyd2_rd_ee_uchar(
	spyd2 *p,				/* Object */
	unsigned int *outp,		/* Where to write value */
	int addr				/* EEprom Address, 0 - 510 */
) {
	inst_code ev;
	unsigned char buf[1];
	int v, val;

	if ((ev = spyd2_readEEProm(p, buf, addr, 1)) != inst_ok)
		return ev;

	*outp = buf[0];

	return inst_ok;
}

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

unsigned int crctab[256];

void crc32_init(void) {
    int i, j;

    unsigned int crc;

    for (i = 0; i < 256; i++) {
        crc = i;
        for (j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xedb88320;
            else
                crc = crc >> 1;
        }
        crctab[i] = crc;
//		printf("crctab[%d] = 0x%08x\n",i,crctab[i]);
    }
}

static unsigned int crc32(unsigned char *data, int len) {
    unsigned int        crc;
    int                 i;
    unsigned char       octet;
    
    crc = ~0;
    for (i = 0; i < len; i++)
		crc = crctab[(crc ^ *data++) & 0xff] ^ (crc >> 8);
    return ~crc;
}

/* For HWV 7, check the EEPRom CRC */
static inst_code
spyd2_checkEECRC(
	spyd2 *p				/* Object */
) {
	inst_code ev;
	unsigned char buf[1024], *bp;
	unsigned int crct, crc;			/* Target value, computed value */
	int i, j;

	crc32_init();

	if ((ev = spyd2_readEEProm(p, buf, 0, 1024)) != inst_ok)
		return ev;

	/* Target value */
	crct = buf2uint(buf + 1024 - 4);

	bp = buf;
    crc = ~0;
	for (i = 0; i < (1024 - 4); i++, bp++)
		crc = crctab[(crc ^ *bp) & 0xff] ^ (crc >> 8);
    crc = ~crc;

	if (p->debug) fprintf(stderr,"spyd2: EEProm CRC is 0x%x, should be 0x%x\n",crc,crct);

	if (crc != crct)
		return spyd2_interp_code((inst *)p, SPYD2_BAD_EE_CRC);
		
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
	int addr				/* Register Address, 0 - 1023 */
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

/* Special purpose short read, */
/* Read 7 x 41 vectors of ints from the EEprom */
static inst_code
spyd2_rdreg_7x41xshort(
	spyd2 *p,				/* Object */
	double sens[7][41],		/* Destination */
	int addr				/* Register Address, 0 - 1023 */
) {
	inst_code ev;
	unsigned char buf[7 * 41 * 2], *bp;
	int i, j;

	if ((ev = spyd2_readEEProm(p, buf, addr, 7 * 41 * 2)) != inst_ok)
		return ev;

	bp = buf;
	for (i = 0; i < 7; i++) {
		for (j = 0; j < 41; j++, bp += 2) {
			int val;
			val = buf2ushort(bp);
			sens[i][j] = val / 100.0;
		}
	}

	return inst_ok;
}

/* Get refresh rate command. Return 0.0 */
/* if no refresh rate can be established */
/* (This isn't used by the manufacturers Spyder3/4 driver, */
/*  but the instrument seems to impliment it.) */
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
	double inttime = 0.0;

	if (p->debug) fprintf(stderr,"spyd2: about to get a reading\n");

	if (p->ref)
		inttime = RINTTIME;		/* ie. 4 seconds */
	else
		inttime = NINTTIME;		/* ie. 2 seconds */

	/* Compute number of frames for desired base read time */
	nframes = (int)(inttime * p->rrate + 0.5);

	/* Establish the frame rate detect threshold level */
	/* (The Spyder 3 doesn't use this ?) */
	clocks1 = (int)((double)(nframes * CLKRATE)/(10.0 * p->rrate));		/* Use 10% of measurement clocks */

	if ((ev = spyd2_GetMinMax(p, &clocks1, &min, &max)) != inst_ok)
		return ev; 

	/* Setup for measurement */
	thresh = (max - min)/5 + min;			/* Threshold is at 80% of max brightness */
	if (thresh == 0)
		thresh = 65535;						/* Set to max, otherwise reading will be 0 */
	frclocks = (int)(CLKRATE/p->rrate);
	minfclks = frclocks/3;					/* Allow for 180 Hz */
	maxfclks = (frclocks * 5)/2;			/* Allow for 24 Hz */

	if (p->hwver < 7) {
		/* Check calibration is valid */
		if (p->calix == 0 && (p->fbits & 1) == 0)
			return spyd2_interp_code((inst *)p, SPYD2_NOCRTCAL);
	
		if (p->calix == 1 && (p->fbits & 2) == 0)
			return spyd2_interp_code((inst *)p, SPYD2_NOLCDCAL);
	}

	if (p->debug) {
		fprintf(stderr,"Using cal table %d\n",p->calix);
		if (p->hwver)
			fprintf(stderr,"Using spectral cal table %d\n",p->calix4);
	}

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
		if (p->debug)
			printf("Maxtcnt = %d, Mintcnt = %d\n",maxtcnt,mintcnt);
		if (i > 0)
			break;			/* Done adaptive */

		/* Decide whether to go around again */

		if (maxtcnt <= (100/16)) {
			nframes *= 16;			/* Typically 16 seconds */
			if (p->debug) fprintf(stderr,"Using maximum integration time\n");
		} else if (maxtcnt < 100) {
			double mulf;
			mulf = 100.0/maxtcnt;
			mulf -= 0.8;			/* Just want to accumulate up to target, not re-do it */
			nframes = (int)(nframes * mulf + 0.5);
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

	if (p->hwver == 5) {
		double gainscale = 1.0;
		unsigned int v381;

		if ((ev = spyd2_rd_ee_uchar(p, &v381, 381)) != inst_ok)
			return ev; 

		gainscale = (double)v381/p->gain;
		if (p->debug) fprintf(stderr,"spyd2: hwver5 v381 = %d, gain = %f, gainscale = %f\n",v381,p->gain,gainscale);
		/* Convert sensor readings to XYZ value */
		for (j = 0; j < 3; j++) {
			XYZ[j] = p->cal_A[p->calix][j][0];		/* First entry is a constant */
			for (k = 1; k < 8; k++)
				XYZ[j] += a_sensv[k] * p->cal_A[p->calix][j][k+2] * gainscale;
		}

	} else {
		/* Convert sensor readings to XYZ value */
		for (j = 0; j < 3; j++) {
			XYZ[j] = p->cal_A[p->calix][j][0];		/* First entry is a constant */
			for (k = 1; k < 8; k++)
				XYZ[j] += a_sensv[k] * p->cal_A[p->calix][j][k+1];
		}
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
			XYZ[j] += pows[k] * p->cal_B[p->calix][j][k];
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

/* Spyder3/4: Do an ambient reading */

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
/* Spyder 4 manufacturer calibration data */
int spyd4_nocals = 0;				/* Number of calibrations */
xspect *spyd4_cals = NULL;			/* [nocals] Device spectrum */

/* ------------------------------------------------------------ */
/* Spyder4: Create a calibration matrix using the manufacturers */
/* calibration data. */

static inst_code
spyd4_set_cal(
	spyd2 *p,		/* Object */
	int ix			/* Selection, 0 .. spyd4_nocals-1 */ 
) {
	int i, j, k;
	xspect oc[3];			/* The XYZ observer curves */

	if (ix < 0 || ix >= spyd4_nocals) {
		return spyd2_interp_code((inst *)p, SPYD2_DISP_SEL_RANGE) ;
	} 

	/* The Manufacturers calibration routine computes a least squares */
	/* spectral fit of the sensor spectral sensitivities to the */
	/* standard observer curves, weighted by the white illuminant */
	/* of the display technology. We use the same approach for the */
	/* default calibration selections, to be faithful to the Manufacturers */
	/* intentions. */

	if (standardObserver(&oc[0], &oc[1], &oc[2], icxOT_CIE_1931_2)) {
		return spyd2_interp_code((inst *)p, SPYD2_DISP_SEL_RANGE) ;
	}

	/* We compute X,Y & Z independently. */
	for (k = 0; k < 3; k++) {
		double target[81];			/* Spectral target @ 5nm spacing */
		double **wsens;				/* Weighted sensor sensitivities */
		double **psisens;			/* Pseudo inverse of sensitivies */

		/* Load up the observer curve and weight it by the display spectrum */
		/* and mW Lumoinance efficiency factor. */
		for (i = 0; i < 81; i++) {
			double nm = 380.0 + i * 5.0;
			target[i] = value_xspect(&spyd4_cals[ix], nm) * value_xspect(&oc[k], nm) * 0.683002;
		}

		/* Load up the sensor curves and weight by the display spectrum */
		wsens = dmatrix(0, 6, 0, 80);
		psisens = dmatrix(0, 80, 0, 6);
		for (j = 0; j < 7; j++) {
			for (i = 0; i < 81; i++) {
				double nm = 380.0 + i * 5.0;
				wsens[j][i] = value_xspect(&spyd4_cals[ix], nm) * value_xspect(&p->sens[j], nm);
			}
		}

		/* Compute the pseudo-inverse matrix */
		if (lu_psinvert(psisens, wsens, 7, 81) != 0) {
			free_dmatrix(wsens, 0, 6, 0, 80);
			free_dmatrix(psisens, 0, 80, 0, 6);
			return spyd2_interp_code((inst *)p, SPYD2_CAL_FAIL) ;
		}

		{
			double *cc, *tt;
			p->cal_A[1][k][0] = 0.0;		/* Offset is zero */
			p->cal_A[1][k][1] = 0.0;		/* Unused is zero */
			cc = &p->cal_A[1][k][2];		/* 7 cal values go here */
			tt = target;
	
			/* Multiply inverse by target to get calibration matrix */
			if (matrix_mult(&cc, 1, 7, &tt, 1, 81, psisens, 81, 7))
				return spyd2_interp_code((inst *)p, SPYD2_CAL_FAIL) ;
			}
//			printf("Cal %d = %f %f %f %f %f %f %f\n", k, p->cal_A[1][k][2], p->cal_A[1][k][3], p->cal_A[1][k][4], p->cal_A[1][k][5], p->cal_A[1][k][6], p->cal_A[1][k][7], p->cal_A[1][k][8]);
		}

#ifdef PLOT_SPECTRA
	/* Plot the calibrated sensor spectra */
	{
		int i, j, k;
		double xx[81];
		double yy[10][81], *yp[10];

		for (i = 0; i < 81; i++)
			xx[i] = 380.0 + i * 5.0;
	
		for (j = 0; j < 3; j++) {
			for (i = 0; i < 81; i++) {
				yy[j][i] = 0.0;
				for (k = 0; k < 7; k++) {
					yy[j][i] += p->cal_A[1][j][k+2] * value_xspect(&p->sens[k], xx[i]);
				}
			}
			yp[j] = yy[j];
		}
		for (; j < 10; j++)
			yp[j] = NULL;
	
		printf("The calibrated sensor sensitivities\n");
		do_plot10(xx, yp[0], yp[1], yp[2], yp[3], yp[4], yp[5], yp[6], yp[7], yp[8], yp[9], 81, 0);
	}
#endif /* PLOT_SPECTRA */
	
	p->calix4 = ix;
	return inst_ok;
}


/* The CCSS calibration uses a set of spectral samples, */
/* and a least squares matrix is computed to map the sensor RGB */
/* to the computed XYZ values. This allows better accuracy for */
/* a typical display that has only 3 degrees of freedom, and */
/* allows weigting towards a distribution of actual spectral samples. */
/* Because the typical display has only three degrees of freedom, */
/* while the instrument has 7 sensors, some extra dummy spectral */
/* samples are added to the list to provide some slight extra goal. */

/* [ Given the poor curve shapes that can come out of this, it's not */
/*   clear that it wouldn't be better using the default flat-spetrum */
/*   calibration and computing a 3x3 calibration matrix over the top of it. ] */
static inst_code
spyd4_comp_calmat(
	spyd2 *p,
	icxObserverType obType,	/* XYZ Observer type */
	xspect custObserver[3],	/* Optional custom observer */					\
	xspect *samples,	/* Array of nsamp spectral samples */
	int nsamp			/* Number of real samples */
) {
	int i, j, k;
	int nasamp = nsamp + 81; /* Number of real + augmented samples */
	double exwt = 1.0;	/* Extra spectral point weight */
	double intsv;		/* Maximum sample value */
	double **sampXYZ;	/* Sample XYZ values */
	double **sampSENS;	/* Sample Sensor values */
	double **isampSENS;	/* Pseudo-inverse of sensor values */
	double **calm;		/* Calibration matrix */
	xsp2cie *conv;
	double wl;
	xspect white;

	if (nsamp < 3)
		return spyd2_interp_code((inst *)p, SPYD2_TOO_FEW_CALIBSAMP);

	/* Create white spectrum samples */
	XSPECT_COPY_INFO(&white, &samples[0]);
	for (j = 0; j < white.spec_n; j++)
		white.spec[j] = 0.0;
	for (i = 0; i < nsamp; i++) {
		for (j = 0; j < white.spec_n; j++)
			if (samples[i].spec[j] > white.spec[j])
				white.spec[j] = samples[i].spec[j];
	}

	/* Compute XYZ of the real sample array. */
	if ((conv = new_xsp2cie(icxIT_none, NULL, obType, custObserver, icSigXYZData)) == NULL)
		return spyd2_interp_code((inst *)p, SPYD2_INT_CIECONVFAIL);
	sampXYZ = dmatrix(0, nasamp-1, 0, 3-1);
	for (i = 0; i < nsamp; i++) {
		conv->convert(conv, sampXYZ[i], &samples[i]); 
//printf("~1 asamp[%d] XYZ = %f %f %f\n", i,sampXYZ[nsamp+i][0],sampXYZ[nsamp+i][1], sampXYZ[nsamp+i][2]);
	}

	/* Create extra spectral samples */
	for (i = 0; i < 81; i++) {
		for (j = 0; j < 3; j++) {
			wl = 380.0 + i * 5;
			sampXYZ[nsamp+i][j] = exwt * value_xspect(&white, wl)
			                    * value_xspect(&conv->observer[j], wl) * 0.683002;
		}
//printf("~1 asamp[%d] XYZ = %f %f %f\n", i,sampXYZ[nsamp+i][0],sampXYZ[nsamp+i][1], sampXYZ[nsamp+i][2]);
	}
	conv->del(conv);

	sampSENS = dmatrix(0, nasamp-1, 0, 7-1);

	/* Compute sensor values of the sample array */
	for (i = 0; i < nsamp; i++) {
		for (j = 0; j < 7; j++) {
			sampSENS[i][j] = 0.0;
			for (wl = p->sens[0].spec_wl_short; wl <= p->sens[0].spec_wl_long; wl += 1.0) {
				sampSENS[i][j] += value_xspect(&samples[i], wl) * value_xspect(&p->sens[j], wl); 
			}
		}
	}
	/* Create sensor values of the extra sample array */
	for (i = 0; i < 81; i++) {
		for (j = 0; j < 7; j++) {
			wl = 380.0 + i * 5;
			sampSENS[nsamp+i][j] = exwt * value_xspect(&white, wl) * value_xspect(&p->sens[j], wl); 
		}
//printf("~1 asamp[%d] Sens = %f %f %f %f %f %f %f\n", i,
//sampSENS[nsamp+i][0],sampSENS[nsamp+i][1], sampSENS[nsamp+i][2],
//sampSENS[nsamp+i][3],sampSENS[nsamp+i][4], sampSENS[nsamp+i][5],
//sampSENS[nsamp+i][6]);
	}
#if defined(DEBUG) && defined(PLOT_SPECTRA)
	/* Plot the target extra values */
	{
		int i, j, k;
		double xx[81];
		double yy[10][81], *yp[10];

		for (i = 0; i < 81; i++)
			xx[i] = 380.0 + i * 5;
	
		for (j = 0; j < 3; j++) {
			for (i = 0; i < 81; i++) {
				yy[j][i] = sampXYZ[nsamp+i][j];
			}
			yp[j] = yy[j];
		}
		for (; j < 10; j++)
			yp[j] = NULL;
	
		printf("The target extra XYZ values\n");
		do_plot10(xx, yp[0], yp[1], yp[2], yp[3], yp[4], yp[5], yp[6], yp[7], yp[8], yp[9], 81, 0);

		for (j = 0; j < 7; j++) {
			for (i = 0; i < 81; i++) {
				yy[j][i] = sampSENS[nsamp+i][j];
			}
			yp[j] = yy[j];
		}
		for (; j < 10; j++)
			yp[j] = NULL;
	
		printf("The given extra sensor values\n");
		do_plot10(xx, yp[0], yp[1], yp[2], yp[3], yp[4], yp[5], yp[6], yp[7], yp[8], yp[9], 81, 0);
	}
#endif /* PLOT_SPECTRA */


	isampSENS = dmatrix(0, 7-1, 0, nasamp-1);

	/* Compute the pseudo inverse of sampSENS */
	if (lu_psinvert(isampSENS, sampSENS, nasamp, 7) != 0) {
		free_dmatrix(sampXYZ, 0, nasamp-1, 0, 3-1);
		free_dmatrix(sampSENS, 0, nasamp-1, 0, 7-1);
		free_dmatrix(isampSENS, 0, 7-1, 0, nasamp-1);
		return spyd2_interp_code((inst *)p, SPYD2_CAL_FAIL) ;
	}

	calm = dmatrix(0, 7-1, 0, 3-1);

	/* Multiply inverse by target to get calibration matrix */
	if (matrix_mult(calm, 7, 3, isampSENS, 7, nasamp, sampXYZ, nasamp, 3)) {
		free_dmatrix(sampXYZ, 0, nasamp-1, 0, 3-1);
		free_dmatrix(sampSENS, 0, nasamp-1, 0, 7-1);
		free_dmatrix(isampSENS, 0, 7-1, 0, nasamp-1);
		free_dmatrix(calm, 0, 7-1, 0, 3-1);
		return spyd2_interp_code((inst *)p, SPYD2_CAL_FAIL);
	}

	/* Copy the matrix into place */
	for (i = 0; i < 7; i++) {
		for (j = 0; j < 3; j++) {
//calm[i][j] = 0.5;
			p->cal_A[1][j][2+i] = calm[i][j];
		}
	}

	free_dmatrix(calm, 0, 7-1, 0, 3-1);

#ifdef NEVER

	/* Compute the residuals */
	{
		double **refXYZ;	
		double t1, t2;

		refXYZ = dmatrix(0, nasamp-1, 0, 3-1);

		if (matrix_mult(refXYZ, nasamp, 3, sampSENS, nasamp, 7, calm, 7, 3)) {
			printf("Residual matrix mult failed\n");
		} else {
			t1 = 0.0;
			for (i = 0; i < nsamp; i++) {
				t1 += icmLabDE(refXYZ[i],sampXYZ[i]);
			}
			t1 /= nsamp;
			printf("Average error for sample points = %f\n",t1);
			t2 = 0.0;
			for (i = nsamp; i < (nsamp + 81); i++) {
				t2 += icmLabDE(refXYZ[i],sampXYZ[i]);
//				printf("Resid %d error = %f, %f %f %f, %f %f %f\n",
//				i, icmLabDE(refXYZ[i],sampXYZ[i]), sampXYZ[i][0], sampXYZ[i][1],
//				sampXYZ[i][2], refXYZ[i][0], refXYZ[i][1], refXYZ[i][2]);
			}
			t2 /= 81;
			printf("Average error for extra points = %f\n",t2);
		}
	}
#endif

#ifdef PLOT_SPECTRA
	/* Plot the calibrated sensor spectra */
	{
		int i, j, k;
		double xx[81];
		double yy[10][81], *yp[10];

		for (i = 0; i < 81; i++)
			xx[i] = 380.0 + i * 5.0;
	
		for (j = 0; j < 3; j++) {
			for (i = 0; i < 81; i++) {
				yy[j][i] = 0.0;
				for (k = 0; k < 7; k++) {
					yy[j][i] += p->cal_A[1][j][k+2] * value_xspect(&p->sens[k], xx[i]);
				}
			}
			yp[j] = yy[j];
		}
		for (; j < 10; j++)
			yp[j] = NULL;
	
		printf("The calibrated sensor sensitivities\n");
		do_plot10(xx, yp[0], yp[1], yp[2], yp[3], yp[4], yp[5], yp[6], yp[7], yp[8], yp[9], 81, 0);
	}
#endif /* PLOT_SPECTRA */

	free_dmatrix(sampXYZ, 0, nasamp-1, 0, 3-1);
	free_dmatrix(sampSENS, 0, nasamp-1, 0, 7-1);
	free_dmatrix(isampSENS, 0, 7-1, 0, nasamp-1);

	return inst_ok;
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
	if ((ev = spyd2_rd_ee_uchar(p, &p->hwver, 5)) != inst_ok)
		return ev;

	/* Feature bits */
	if ((ev = spyd2_rd_ee_uchar(p, &p->fbits, 6)) != inst_ok)
		return ev;

	if (p->debug >= 1) fprintf(stderr,"hwver = 0x%02x%02x\n",p->hwver,p->fbits);

	/* Check the EEProm checksum */
	if (p->hwver == 7) {
		if ((ev = spyd2_checkEECRC(p)) != inst_ok)
		return ev;
	}

	/* Serial number */
	if ((ev = spyd2_readEEProm(p, (unsigned char *)p->serno, 8, 8)) != inst_ok)
		return ev;
	p->serno[8] = '\000';
	if (p->debug >= 4) fprintf(stderr,"serno = '%s'\n",p->serno);

	if (p->hwver < 7) {

		/* (Not sure if we should look at fbits 1 or not) */
		if (p->fbits & 1) {

			/* Spyde2: CRT calibration values */
			/* Spyde3: Unknown calibration values */
			if ((ev = spyd2_rdreg_3x9xfloat(p, p->cal_A[0][0], p->cal_A[0][1], p->cal_A[0][2], 16))
			                                                                            != inst_ok)
				return ev;
			if ((ev = spyd2_rdreg_3x9xfloat(p, p->cal_B[0][0], p->cal_B[0][1], p->cal_B[0][2], 128))
			                                                                            != inst_ok)
				return ev;
	
	
			/* Hmm. The 0 table seems to sometimes be scaled. Is this a bug ? */
			/* (might be gain factor ?) */
			/* The spyder 3/4 doesn't use this anyway. */
			if (p->hwver >= 4) {
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
		}

		if (p->fbits & 2) {
			/* Spyder2: LCD calibration values */
			/* Spyder3: Normal CRT/LCD calibration values */
			if ((ev = spyd2_rdreg_3x9xfloat(p, p->cal_A[1][0], p->cal_A[1][1], p->cal_A[1][2], 256))
			                                                                            != inst_ok)
				return ev;
			if ((ev = spyd2_rdreg_3x9xfloat(p, p->cal_B[1][0], p->cal_B[1][1], p->cal_B[1][2], 384))
			                                                                            != inst_ok)
				return ev;
		}

		/* The monochrome "TOKIOBLUE" calibration */
		/* (Not sure if this is fbits 2 and 4 or not) */
		if (p->fbits & 4) {

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
		}

		if (p->debug >= 4) {
			int i, j, k;

			if (p->fbits & 1) {
				fprintf(stderr,"Cal_A:\n");
				for (i = 0; i < 2;i++) {
					for (j = 0; j < 3; j++) {
						for (k = 0; k < 9; k++) {
							fprintf(stderr,"Cal_A [%d][%d][%d] = %f\n",i,j,k,p->cal_A[i][j][k]);
						}
					}
				}
			}
			if (p->fbits & 2) {
				fprintf(stderr,"\nCal_B:\n");
				for (i = 0; i < 2;i++) {
					for (j = 0; j < 3; j++) {
						for (k = 0; k < 9; k++) {
							fprintf(stderr,"Cal_B [%d][%d][%d] = %f\n",i,j,k,p->cal_B[i][j][k]);
						}
					}
				}
			}
			if (p->fbits & 4) {
				fprintf(stderr,"\nCal_F:\n");
				for (i = 0; i < 7;i++) {
					fprintf(stderr,"Cal_F [%d] = %f\n",i,p->cal_F[i]);
				}
			}
			fprintf(stderr,"\n");
		}

	} else if (p->hwver == 7) {
		int i, j;
		unsigned int sscal;
		double tsens[7][41];

		/* Read sensor sensitivity spectral data */
		if ((ev = spyd2_rdreg_7x41xshort(p, tsens, 170)) != inst_ok)
			return ev;

		/* Sensor scale factor */
		if ((ev = spyd2_rd_ee_ushort(p, &sscal, 21)) != inst_ok) 
			return ev;

		/* And apply it to the sensor data */
		for (j = 0; j < 7; j++) {
			for (i = 0; i < 41; i++) {
				tsens[j][i] /= 1000;		/* Convert to Hz per mW/nm/m^2 */
				tsens[j][i] /= sscal/1e5;		/* Sensitivity scale value */
			}
		}

		/* Convert sensor values to xspect's */
		for (i = 0; i < 7; i++) {
			p->sens[i].spec_n = 41;
			p->sens[i].spec_wl_short = 380;
			p->sens[i].spec_wl_long = 780;
			p->sens[i].norm = 1.0;
			for (j = 0; j < 41; j++) {
				p->sens[i].spec[j] = tsens[i][j];
			}
		}
#ifdef SAVE_SPECTRA
		write_nxspect("sensors.sp", p->sens, 7, 0);
#endif

		/* Linearization */
		if ((ev = spyd2_rdreg_3x9xfloat(p, p->cal_B[1][0], p->cal_B[1][1], p->cal_B[1][2], 60))
		                                                                    != inst_ok)
			return ev;
		
#ifdef PLOT_SPECTRA
	/* Plot the sensor spectra */
	{
		int i, j;
		double xx[81];
		double yy[10][81], *yp[10];

		for (i = 0; i < 81; i++)
			xx[i] = 380.0 + i * 5.0;
	
		for (j = 0; j < 7; j++) {
			for (i = 0; i < 81; i++)
				yy[j][i] = value_xspect(&p->sens[j], xx[i]);
			yp[j] = yy[j];
		}
		for (; j < 10; j++)
			yp[j] = NULL;
	
		printf("The sensor and ambient sensor sensitivy curves\n");
		do_plot10(xx, yp[0], yp[1], yp[2], yp[3], yp[4], yp[5], yp[6], yp[7], yp[8], yp[9], 81, 0);


		for (j = 0; j < spyd4_nocals; j++) {
			double max = 0;
			for (i = 0; i < 81; i++) {
				if (yy[j][i] = value_xspect(&spyd4_cals[j], xx[i]) > max)
					max = value_xspect(&spyd4_cals[j], xx[i]);
			}
			for (i = 0; i < 81; i++)
				yy[j][i] = value_xspect(&spyd4_cals[j], xx[i])/max;
			yp[j] = yy[j];
		}
		for (; j < 10; j++)
			yp[j] = NULL;
	
		printf("The display spectra\n");
		do_plot10(xx, yp[0], yp[1], yp[2], yp[3], yp[4], yp[5], yp[6], yp[7], yp[8], yp[9], 81, 0);
	}
#endif /* PLOT_SPECTRA */

	}

	if (p->debug) fprintf(stderr,"spyd2: all EEProm read OK\n");

	return inst_ok;
}

/* ------------------------------------------------------------ */

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


/* ------------------------------------------------------------ */
/* Setup Spyder4 native calibrations                            */

/* Load the manufacturers Spyder4 calibration data */
/* Return a SPYD2_ error value */
static int
spyd4_load_cal(int debug) {
	char **bin_paths = NULL;
	int no_paths = 0;
	unsigned int size;
	unsigned char *buf = NULL;
	FILE *fp;
	int nocals = 0;
	int i, j;

	/* If already loaded */
	if (spyd4_nocals != 0)
		return SPYD2_OK;

	for (;;) {		/* So we can break */
		if ((no_paths = xdg_bds(NULL, &bin_paths, xdg_data, xdg_read, xdg_user, "color/spyd4cal.bin")) < 1)
			break;

		/* open binary file */
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
		if ((fp = fopen(bin_paths[0],"rb")) == NULL)
#else
		if ((fp = fopen(bin_paths[0],"r")) == NULL)
#endif
			break;
		xdg_free(bin_paths, no_paths);

		/* Figure out how big file it is */
		if (fseek(fp, 0, SEEK_END)) {
			fclose(fp);
			break;
		}
		size = (unsigned long)ftell(fp);

		if ((size % (41 * 8)) != 0) {
			fclose(fp);
			if (debug) fprintf(stderr,"spyd2: calibration file '%s' is unexpected size\n",bin_paths[0]);
			break;
		}

		nocals = size/(41 * 8);
		if (nocals != 6) {
			fclose(fp);
			if (debug) fprintf(stderr,"spyd2: calibration file '%s' is unexpected number of calibrations (%d)\n",bin_paths[0],nocals);
			break;
		}

		if (fseek(fp, 0, SEEK_SET)) {
			fclose(fp);
			break;
		}
	
		if ((buf = (unsigned char *)calloc(nocals * 41, 8)) == NULL) {
			fclose(fp);
			return SPYD2_MALLOC;
		}

		if (fread(buf, 1, size, fp) != size) {
			free(buf);
			fclose(fp);
			break;
		}
		fclose(fp);
		break;
	}

	if (buf == NULL)
		nocals = 1;

	if ((spyd4_cals = (xspect *)calloc(nocals, sizeof(xspect))) == NULL) {
		if (buf != NULL)
			free(buf);
		fclose(fp);
		return SPYD2_MALLOC;
	}

	/* If we have calibrations */
	if (buf != NULL) {
		unsigned char *bp;

		for (i = 0; i < nocals; i++) {
			bp = buf + 41 * 8 * i;

			spyd4_cals[i].spec_n = 41;
			spyd4_cals[i].spec_wl_short = 380;
			spyd4_cals[i].spec_wl_long = 780;
			spyd4_cals[i].norm = 1.0;

			for (j = 0; j < 41; j++, bp += 8) {
				ORD64 val;

				val = buf2ord64(bp);
				spyd4_cals[i].spec[j] = IEEE754_64todouble(val);
//				printf("cal[%d][%d] = %f\n",i,j,spyd4_cals[i].spec[j]);
			}
		}

	} else {

		/* Create a default calibration */
		for (j = 0; j < 41; j++)
			spyd4_cals[0].spec_n = 41;
			spyd4_cals[0].spec_wl_short = 380;
			spyd4_cals[0].spec_wl_long = 780;
			spyd4_cals[0].norm = 1.0;

			for (j = 0; j < 41; j++) {
				spyd4_cals[0].spec[j] = 1.0;
			}
	}

	spyd4_nocals = nocals;

	return SPYD2_OK;
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
	if (p->itype == instSpyder3) {
		usbflags |= icomuf_resetep_before_read;		/* The spyder USB is buggy ? */
	}
#endif

	/* On OS X the Spyder 2 can't close properly */
#if defined(UNIX) && defined(__APPLE__)			/* OS X*/
	if (p->itype == instSpyder2) {
		usbflags |= icomuf_reset_before_close;		/* The spyder 2 USB is buggy ? */
	}
#endif

#ifdef NEVER	/* Don't want this now that we avoid 2nd set_config on Linux */
#if defined(UNIX) && !defined(__APPLE__)		/* Linux*/
	/* On Linux the Spyder 2 doesn't work reliably unless each */
	/* read is preceeded by a reset endpoint. */
	if (p->itype == instSpyder2) {
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

	if (p->itype != instSpyder2
	 && p->itype != instSpyder3
	 && p->itype != instSpyder4)
		return spyd2_interp_code((inst *)p, SPYD2_UNKNOWN_MODEL);

	p->rrset = 0;
	p->rrate = DEFRRATE;
	for (i = 0; i < 8; i++)
		p->prevraw[i] = 0;			/* Internal counters will be reset */

	/* For Spyder 1 & 2, reset the hardware and wait for it to become ready. */
	if (p->itype != instSpyder3
	 && p->itype != instSpyder4) {

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
		/* Because the Spyder 3/4 doesn't have a reset command, */
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

	/* Spyder 2 */
	if (p->hwver < 4) {
		/* Download the PLD pattern and check the status */
		if ((ev = spyd2_download_pld(p)) != inst_ok)
			return ev;
	}

	p->gain = 1.0;
	if (p->hwver == 5) {
		if ((ev = spyd2_SetGain(p, 4)) != inst_ok)
			return ev;
	}

	/* Set a default calibration */
	if (p->hwver < 7) {
		p->ref = 0;
		p->calix = 1;
	} else {
		p->ref = 0;
		p->calix = 1;		/* 3 & 4 alays use the second table */

		/* Set the default calibration */
		if ((ev = spyd4_set_cal(p, 0)) != inst_ok)
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

	
	if (p->hwver >= 4) {
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
		printf("Hardware version:  0x%02x%02x\n",p->hwver,p->fbits);
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

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

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

		/* Attempt a CRT frame rate calibration if needed */
		if (p->ref != 0 && p->rrset == 0) { 
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

/* Use a Colorimeter Calibration Spectral Set to set the */
/* instrumen calibration. */
/* This is only valid for colorimetric instruments. */
/* To set calibration back to default, pass NULL for sets. */
inst_code spyd2_col_cal_spec_set(
inst *pp,
icxObserverType obType,
xspect custObserver[3],
xspect *sets,
int no_sets
) {
	spyd2 *p = (spyd2 *)pp;
	inst_code ev = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;
	if (p->hwver < 7)
		return inst_unsupported;

	if (obType == icxOT_default)
		obType = icxOT_CIE_1931_2;
		
	if (sets == NULL || no_sets <= 0) {
		if ((ev = spyd4_set_cal(p, 0)) != inst_ok)
			return ev;
	} else {
		/* Use given spectral samples */
		if ((ev = spyd4_comp_calmat(p, obType, custObserver, sets, no_sets)) != inst_ok)
			return ev;
	}
	return ev;
}

/* Determine if a calibration is needed. Returns inst_calt_none if not, */
/* inst_calt_unknown if it is unknown, or inst_calt_crt_freq */
/* if the display refresh rate has not bee determined, */
/* and we are in CRT mode */
inst_cal_type spyd2_needs_calibration(inst *pp) {
	spyd2 *p = (spyd2 *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->ref != 0 && p->rrset == 0)
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

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	id[0] = '\000';

	/* Translate default into what's needed or expected default */
	if (calt == inst_calt_all) {
		if (p->ref != 0)
			calt = inst_calt_crt_freq;
	}

	if (calt == inst_calt_crt_freq && p->ref != 0) {
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

		/* device specific errors */
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
		case SPYD2_BAD_EE_CRC:
			return "Serial EEProm CRC failed";

		/* Internal errors */
		case SPYD2_BAD_EE_ADDRESS:
			return "Serial EEProm read is out of range";
		case SPYD2_BAD_EE_SIZE:
			return "Serial EEProm read size > 256";
		case SPYD2_NO_PLD_PATTERN:
			return "No PLD firmware pattern is available (have you run spyd2en ?)";
		case SPYD2_NO_COMS:
			return "Communications hasn't been established";
		case SPYD2_NOT_INITED:
			return "Insrument hasn't been initialised";
		case SPYD2_NOCRTCAL:
			return "Insrument is missing the CRT calibration table";
		case SPYD2_NOLCDCAL:
			return "Insrument is missing the Normal or LCD calibration table";
		case SPYD2_MALLOC:
			return "Memory allocation failure";
		case SPYD2_OBS_SELECT:
			return "Failed to set observer type";
		case SPYD2_CAL_FAIL:
			return "Calibration calculation failed";
		case SPYD2_INT_CIECONVFAIL:
			return "Creating spectral to CIE converted failed";
		case SPYD2_TOO_FEW_CALIBSAMP:
			return "There are too few spectral calibration samples - need at least 3";

		/* Configuration */
		case SPYD2_DISP_SEL_RANGE:
			return "Display device selection out of range";

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
		case SPYD2_MALLOC:
		case SPYD2_OBS_SELECT:
		case SPYD2_CAL_FAIL:
		case SPYD2_INT_CIECONVFAIL:
		case SPYD2_TOO_FEW_CALIBSAMP:
			return inst_internal_error | ec;

		case SPYD2_COMS_FAIL:
		case SPYD2_BADREADSIZE:
		case SPYD2_TRIGTIMEOUT:
		case SPYD2_BADSTATUS:
		case SPYD2_OVERALLTIMEOUT:
			return inst_coms_fail | ec;

		case SPYD2_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

//			return inst_protocol_error | ec;

		case SPYD2_USER_ABORT:
			return inst_user_abort | ec;
		case SPYD2_USER_TERM:
			return inst_user_term | ec;
		case SPYD2_USER_TRIG:
			return inst_user_trig | ec;
		case SPYD2_USER_CMND:
			return inst_user_cmnd | ec;

		case SPYD2_NOCRTCAL:
		case SPYD2_NOLCDCAL:
		case SPYD2_PLDLOAD_FAILED:
		case SPYD2_BAD_EE_CRC:
			return inst_hardware_fail | ec;

		case SPYD2_DISP_SEL_RANGE:
			return inst_wrong_config | ec;

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
	   | inst_ccss
	     ;

	if (p->itype == instSpyder3
	 || p->itype == instSpyder4) {
		rv |= inst_emis_disptype;
	} else {
		rv |= inst_emis_disptype;
		rv |= inst_emis_disptypem;
	}

	/* We don't seem to have a way of detecting the lack */
	/* of ambinent capability, short of doing a read */
	/* and noticing the result is zero. */
	if (p->itype == instSpyder3
	 || p->itype == instSpyder4) {
		rv |= inst_emis_ambient;
		rv |= inst_emis_ambient_mono;
	}

	if (p->itype == instSpyder4)
		rv |= inst_ccss;		/* Spyder4 has spectral sensiivities */

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

	if (p->itype == instSpyder3
	 || p->itype == instSpyder4) {
		rv |= inst2_has_leds;
	}
	return rv;
}

inst_disptypesel spyd2_disptypesel[3] = {
	{
		1,
		"c",
		"Spyder2: CRT display",
		1
	},
	{
		2,
		"l",
		"Spyder2: LCD display",
		0
	},
	{
		0,
		"",
		"",
		-1
	}
};

inst_disptypesel spyd3_disptypesel[3] = {
	{
		1,
		"rc",
		"Spyder3: Refresh display",
		1
	},
	{
		2,
		"nl",
		"Spyder3: Non-Refresh display [Default]",
		0
	},
	{
		0,
		"",
		"",
		-1
	}
};

inst_disptypesel spyd4_disptypesel_1[8] = {
	{
		1,
		"rc1",
		"Spyder4: Generic Refresh Display",
		1
	},
	{
		2,
		"nl2",
		"Spyder4: Generic Non-Refresh Display [Default]",
		1
	},
	{
		0,
		"",
		"",
		-1
	}
};

inst_disptypesel spyd4_disptypesel[8] = {
	{
		1,
		"rc1",
		"Spyder4: Generic Refresh Display",
		1
	},
	{
		2,
		"nl2",
		"Spyder4: Generic Non-Refresh Display [Default]",
		1
	},
	{
		3,
		"3",
		"Spyder4: LCD, CCFL Backlight",
		0
	},
	{
		4,
		"4",
		"Spyder4: Wide Gamut LCD, CCFL Backlight",
		0
	},
	{
		5,
		"5",
		"Spyder4: LCD, White LED Backlight",
		0
	},
	{
		6,
		"6",
		"Spyder4: Wide Gamut LCD, RGB LED Backlight",
		0
	},
	{
		7,
		"7",
		"Spyder4: LCD, CCFL Type 2 Backlight",
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
static inst_code spyd2_get_opt_details(
inst *pp,
inst_optdet_type m,	/* Requested option detail type */
...) {				/* Status parameters */                             
	spyd2 *p = (spyd2 *)pp;
	inst_code rv = inst_ok;

	if (m == inst_optdet_disptypesel) {
		va_list args;
		int *pnsels;
		inst_disptypesel **psels;

		va_start(args, m);
		pnsels = va_arg(args, int *);
		psels = va_arg(args, inst_disptypesel **);
		va_end(args);

		if (p->itype == instSpyder4) {
			if (spyd4_nocals <= 1) {
				*pnsels = 2;
				*psels = spyd4_disptypesel_1;
			} else {
				*pnsels = 7;
				*psels = spyd4_disptypesel;
			}
		} else if (p->itype == instSpyder3) {
			*pnsels = 2;
			*psels = spyd3_disptypesel;
		} else {
			*pnsels = 2;
			*psels = spyd2_disptypesel;
		}
		
		return inst_ok;
	}

	return inst_unsupported;
}

/* Set device measurement mode */
inst_code spyd2_set_mode(inst *pp, inst_mode m) {
	spyd2 *p = (spyd2 *)pp;
	inst_mode mm;		/* Measurement mode */

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* The measurement mode portion of the mode */
	mm = m & inst_mode_measurement_mask;

	if (p->hwver < 4) {

		/* only display emission mode supported */
		if (mm != inst_mode_emis_spot
		 && mm != inst_mode_emis_disp) {
			return inst_unsupported;
		}

	} else {
		/* only display emission mode and ambient supported */
		if (mm != inst_mode_emis_spot
		 && mm != inst_mode_emis_disp
		 && mm != inst_mode_emis_ambient) {
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
spyd2_set_opt_mode(inst *pp, inst_opt_mode m, ...) {
	spyd2 *p = (spyd2 *)pp;
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

		if (p->hwver >= 7) {
			int calix4;

			p->calix = 1;				/* Calibration is always second table for hwv = 7 */

			calix4 = 0;
			if (ix == 1) {
				if (p->ref == 0)
					p->rrset = 0;		/* This is a hint we may have swapped displays */
				p->ref = 1;
			} else {	/* 2..7 */
				if (p->ref != 0)
					p->rrset = 0;		/* This is a hint we may have swapped displays */
				p->ref = 0;
				calix4 = ix-2;
				if (calix4 > spyd4_nocals)
					return inst_unsupported; 
			}
			/* Create the spectral calibration if its changed */
			if (p->calix4 != calix4) {
				if ((ev = spyd4_set_cal(p, calix4)) != inst_ok)
					return ev;
			}
			return inst_ok;

		/* Spyder 1, 2, or 3 */
		} else {
			if (ix == 1) {
				if (p->ref == 0)
					p->rrset = 0;		/* This is a hint we may have swapped displays */
				p->ref = 1;
			} else if (ix == 2) {
				if (p->ref != 0)
					p->rrset = 0;		/* This is a hint we may have swapped displays */
				p->ref = 0;
			} else {
				return inst_unsupported;
			}
			if (p->hwver >= 4)
				p->calix = 1;			/* Spyder 3+ uses a single calibration */
			else
				p->calix = ix-1;		/* Spyder 2: 0 = CRT, 1 = LCD */
			return inst_ok;
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

	/* Operate the LED */
	if (p->hwver >= 4) {
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
extern spyd2 *new_spyd2(icoms *icom, instType itype, int debug, int verb)
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
	p->get_opt_details   = spyd2_get_opt_details;
	p->set_mode          = spyd2_set_mode;
	p->set_opt_mode      = spyd2_set_opt_mode;
	p->read_sample       = spyd2_read_sample;
	p->needs_calibration = spyd2_needs_calibration;
	p->calibrate         = spyd2_calibrate;
	p->col_cor_mat       = spyd2_col_cor_mat;
	p->col_cal_spec_set  = spyd2_col_cal_spec_set;
	p->interp_error      = spyd2_interp_error;
	p->del               = spyd2_del;

	p->itype = itype;

	/* Load manufacturers Spyder4 calibrations */
	if (itype == instSpyder4) {
		int rv;
		if ((rv = spyd4_load_cal(debug)) != SPYD2_OK && debug) {
			printf("Loading Spyder4 calibrations failed with '%s'\n",p->interp_error((inst *)p, rv));
		}
		if (spyd4_nocals < 1 && debug)
			printf("Spyder4 calibrations not available\n");
	}

	return p;
}

