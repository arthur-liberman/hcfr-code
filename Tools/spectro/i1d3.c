
/* 
 * Argyll Color Correction System
 *
 * GretagMacbeth Huey related functions
 *
 * Author: Graeme W. Gill
 * Date:   28/7/2011
 *
 * Copyright 2006 - 2011, Graeme W. Gill
 * All rights reserved.
 *
 * (Based on huey.c)
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
#include "i1d3.h"

#undef DEBUG
#undef DUMP_MATRIX
#undef PLOT_SPECTRA		/* Plot the sensor senitivity spectra */
#undef SAVE_SPECTRA		/* Save the sensor senitivity spectra to "sensors.cmf" */
#undef PLOT_REFRESH		/* Plot data used to determine refresh rate */

#ifdef DEBUG
#define DBG(xxx) printf xxx ;
#else
#define DBG(xxx) 
#endif

static inst_code i1d3_interp_code(inst *pp, int ec);
static inst_code i1d3_check_unlock(i1d3 *p);

/* ------------------------------------------------------------------- */
#if defined(__APPLE__) && defined(__POWERPC__)

/* Workaround for a PPC gcc 3.3 optimiser bug... */
/* It seems to cause a segmentation fault instead of */
/* converting an integer loop index into a float, */
/* when there are sufficient variables in play. */
static int gcc_bug_fix(int i) {
	static int nn;
	nn += i;
	return nn;
}
#endif	/* APPLE */

/* ------------------------------------------------------------------------ */
/* Implementation */

/* Interpret an icoms error into a I1D3 error */
/* If torc is nz, then a trigger or command is OK, */
/* othewise  they are treated as an abort. */
static int icoms2i1d3_err(int se, int torc) {
	if (se & ICOM_USERM) {
		se &= ICOM_USERM;
		if (torc) {
			if (se == ICOM_TRIG)
				return I1D3_USER_TRIG;
			if (se == ICOM_CMND)
				return I1D3_USER_CMND;
		}
		if (se == ICOM_TERM)
			return I1D3_USER_TERM;
		return I1D3_USER_ABORT;
	}
	if (se != ICOM_OK)
		return I1D3_COMS_FAIL;
	return I1D3_OK;
}

/* i1d3 command codes. */
/* A 64 bit command/response buffer is always used, communicating */
/* over EP 0x81 and 0x01. The command byte 0 is the major code, */
/* and byte 1 is the sub code for command 0x00 . The response is byte 0 */
/* error code, byte 1 echoing the major command number. */
/* Major code 00 works when locked ? */
typedef enum {
    i1d3_getinfo      = 0x0000,		/* Product name + Firmware version + Firmware Date string */
    i1d3_status       = 0x0001,		/* status number ?? */
    i1d3_prodname     = 0x0010,		/* Product name string */
    i1d3_prodtype     = 0x0011,		/* Product type number */
    i1d3_firmver      = 0x0012,		/* Firmware version string */
    i1d3_firmdate     = 0x0013,		/* Firmware date string */
    i1d3_locked       = 0x0020,		/* Get locked status */
    i1d3_measure1     = 0x0100,		/* Used by all measure */
    i1d3_measure2     = 0x0200,		/* Used by all measure except ambient */
    i1d3_readintee    = 0x0800,		/* Read internal EEPROM */
    i1d3_readextee    = 0x1200,		/* Read external EEPROM */
    i1d3_setled       = 0x2100,		/* Set the LED state */
    i1d3_rd_sensor    = 0x9300,		/* Read the analog sensor */
    i1d3_get_diff     = 0x9400,		/* Get the diffuser position */
    i1d3_lockchal     = 0x9900,		/* Request lock challenge */
    i1d3_lockresp     = 0x9a00,		/* Unlock response */
    i1d3_relock       = 0x9b00		/* Close device - relock ? */
} i1Disp3CC;

/* Diagnostic - return a description given the instruction code */
static char *inst_desc(i1Disp3CC cc) {
	static char buf[40];			/* Fallback string */
	switch(cc) {
		case i1d3_getinfo:
			return "GetInfo";
		case i1d3_status:
			return "GetStatus";
		case i1d3_prodname:
			return "GetProductName";
		case i1d3_prodtype:
			return "GetProductType";
		case i1d3_firmver:
			return "GetFirmwareVersion";
		case i1d3_firmdate:
			return "GetFirmwareDate";
		case i1d3_locked:
			return "GetLockedStatus";
		case i1d3_measure1:
			return "Measure1";
		case i1d3_measure2:
			return "Measure2";
		case i1d3_readintee:
			return "ReadInternalEEPROM";
		case i1d3_readextee:
			return "ReadExternalEEPROM";
		case i1d3_setled:
			return "SetLED";
		case i1d3_rd_sensor:
			return "ReadAnalogSensor";
		case i1d3_get_diff:
			return "GetDiffuserPositio";
		case i1d3_lockchal:
			return "GetLockChallenge";
		case i1d3_lockresp:
			return "SendLockResponse";
		case i1d3_relock:
			return "ReLock";
	}
	sprintf(buf,"Unknown %04x",cc);
	return buf;
}

/* Do a command/response exchange with the i1d3. */
/* Return the error code */
/* The i1d3 is set up as an HID device, which can ease the need */
/* for providing a kernel driver on MSWindows systems, */
/* but it doesn't seem to actually be used as an HID device. */
/* We allow for communicating via libusb, or an HID driver. */
static inst_code
i1d3_command(
	i1d3 *p,					/* i1d3 object */
	i1Disp3CC cc,				/* Command code */
	unsigned char *send,			/* 64 Command bytes to send */
	unsigned char *recv,			/* 64 Response bytes returned */
	double to					/* Timeout in seconds */
) {
	unsigned char cmd;		/* Major command code */
	int wbytes;				/* bytes written */
	int rbytes;				/* bytes read from ep */
	int se, ua = 0, rv = inst_ok;
	int isdeb = 0;

	/* Turn off low level debug messages, and sumarise them here */
	isdeb = p->icom->debug;
	if (isdeb <= 2)
		p->icom->debug = 0;
	
	/* Send the command using interrupt transfer to EP 0x01 */
	send[0] = cmd = (cc >> 8) & 0xff;	/* Major command == HID report number */
	if (cmd == 0x00)
		send[1] = (cc & 0xff);	/* Minor command */

	if (isdeb) fprintf(stderr,"i1d3: Sending cmd '%s' args '%s'",inst_desc(cc), icoms_tohex(send, 8));

	if (p->icom->is_hid) {
		/* Don't poll the keyboard */
		se = p->icom->hid_write_th(p->icom, send, 64, &wbytes, to, p->debug, NULL, 0); 
	} else {
		/* Don't poll the keyboard */
		se = p->icom->usb_write_th(p->icom, NULL, 0x01, send, 64, &wbytes, to, p->debug, NULL, 0);  
	}
	if (se != 0) {
		if (se & ICOM_USERM) {
			ua = (se & ICOM_USERM);
		}
		if (se & ~ICOM_USERM) {
			if (isdeb) fprintf(stderr,"\ni1d3: Command send failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return i1d3_interp_code((inst *)p, I1D3_COMS_FAIL);
		}
	}
	rv = i1d3_interp_code((inst *)p, icoms2i1d3_err(ua, 0));
	if (isdeb) fprintf(stderr," ICOM err 0x%x\n",ua);

	if (rv == inst_ok && wbytes != 64) {
		if (isdeb) fprintf(stderr," wbytes = %d != 64\n",wbytes);
		rv = i1d3_interp_code((inst *)p, I1D3_BAD_WR_LENGTH);
	}

	if (rv != inst_ok) {
		/* Flush any response */
		if (p->icom->is_hid) {
			p->icom->hid_read(p->icom, recv, 64, &rbytes, to);
		} else {
			p->icom->usb_read(p->icom, 0x81, recv, 64, &rbytes, to);
		} 
		p->icom->debug = isdeb;
		return rv;
	}

	/* Now fetch the response */
	if (isdeb) fprintf(stderr,"i1d3: Reading response ");

	if (p->icom->is_hid) {
		/* Don't poll the keyboard */
		se = p->icom->hid_read_th(p->icom, recv, 64, &rbytes, to, p->debug, NULL, 0);
	} else {
		/* Don't poll the keyboard */
		se = p->icom->usb_read_th(p->icom, NULL, 0x81, recv, 64, &rbytes, to, p->debug, NULL, 0);
	} 
	if (se != 0) {
		if (se & ICOM_USERM) {
			ua = (se & ICOM_USERM);
		}
		if (se & ~ICOM_USERM) {
			if (isdeb) fprintf(stderr,"\ni1d3: Response read failed with ICOM err 0x%x\n",se);
			p->icom->debug = isdeb;
			return i1d3_interp_code((inst *)p, I1D3_COMS_FAIL);
		}
	}
	if (rv == inst_ok && rbytes != 64) {
		if (isdeb) fprintf(stderr," rbytes = %d != 64\n",rbytes);
		rv = i1d3_interp_code((inst *)p, I1D3_BAD_RD_LENGTH);
	}

	/* The first byte returned seems to be a command result error code. */
	/* The second byte is usually the command code being echo'd back, but not always. */
	if (rv == inst_ok && recv[0] != 0x00) {
		if (isdeb) fprintf(stderr," status byte != 00 = 0x%x\n",recv[0]);
		rv = i1d3_interp_code((inst *)p, I1D3_BAD_RET_STAT);
	}

	if (rv == inst_ok) {
		if (cc != i1d3_get_diff) {
			if (recv[1] != cmd) {
				if (isdeb) fprintf(stderr," major cmd not echo'd != 0x%02x = 0x%02x\n",cmd,recv[1]);
				rv = i1d3_interp_code((inst *)p, I1D3_BAD_RET_CMD);
			}
		}
	}

	if (isdeb) fprintf(stderr," '%s' ICOM err 0x%x\n",icoms_tohex(recv, 14),ua);
	p->icom->debug = isdeb;

	return rv; 
}

/* Read a packet and time out or throw it away */
static inst_code
i1d3_dummy_read(
	i1d3 *p					/* i1d3 object */
) {
	unsigned char buf[64];
	int rbytes;				/* bytes read from ep */
	int se, rv = inst_ok;

	if (p->icom->is_hid) {
		se = p->icom->hid_read(p->icom, buf, 64, &rbytes, 0.1);
	} else {
		se = p->icom->usb_read(p->icom, 0x81, buf, 64, &rbytes, 0.1);
	} 

	return rv; 
}

/* Byte to int conversion. Most things seem to be little endian... */

/* Take an int, and convert it into a byte buffer */
static void int2buf(unsigned char *buf, int inv) {
	buf[0] = (inv >> 0) & 0xff;
	buf[1] = (inv >> 8) & 0xff;
	buf[2] = (inv >> 16) & 0xff;
	buf[3] = (inv >> 24) & 0xff;
}

/* Take a short, and convert it into a byte buffer */
static void short2buf(unsigned char *buf, int inv) {
	buf[0] = (inv >> 0) & 0xff;
	buf[1] = (inv >> 8) & 0xff;
}

/* Take a short, and convert it into a byte buffer (Big Endian) */
static void short2bufBE(unsigned char *buf, int inv) {
	buf[0] = (inv >> 8) & 0xff;
	buf[1] = (inv >> 0) & 0xff;
}


/* Take a 64 sized return buffer, and convert it to an ORD64 */
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

/* Take a word sized return buffer, and convert it to an unsigned int */
static unsigned int buf2uint(unsigned char *buf) {
	unsigned int val;
	val = buf[3];
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

/* Take a word sized return buffer, and convert it to an int */
static int buf2int(unsigned char *buf) {
	int val;
	val = buf[3];
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}


/* Take a short sized return buffer, and convert it to an int */
static int buf2short(unsigned char *buf) {
	int val;
	val = buf[1];
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Get Product name + Firmware version + Firmware Date string */
static inst_code
i1d3_get_info(
	i1d3 *p,				/* Object */
	char *rv				/* 64 byte buffer */
) {
	unsigned char todev[64];
	unsigned char fromdev[64];
	inst_code ev;
	int isdeb = p->icom->debug;

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_getinfo, todev, fromdev, 1.0)) != inst_ok)
		return ev;
	
	strncpy((char *)rv, (char *)fromdev + 2, 63);

	if (isdeb) fprintf(stderr,"i1d3: getinfo got '%s'\n",rv);

	return inst_ok;
}

/* Check the status. 0 = OK, 1 = BAD */
/* Not sure what sort of status this is. The result changes some */
/* other command parameter treatment. Could it be somthing like */
/* "factory calibrated" status ? */
static inst_code
i1d3_check_status(
	i1d3 *p,				/* Object */
	int *stat				/* Status - 0 if OK, 1 if not OK */
) {
	unsigned char todev[64];
	unsigned char fromdev[64];
	inst_code ev;
	int isdeb = p->icom->debug;

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_status, todev, fromdev, 1.0)) != inst_ok)
		return ev;
	
	*stat = 1;		/* Bad */
	if (fromdev[2] != 0 || (buf2short(fromdev + 3) >= 5))
		*stat = 0;		/* OK */

	if (isdeb) fprintf(stderr,"i1d3: checkstats got %s\n",*stat == 0 ? "OK" : "Bad");

	return inst_ok;
}

/* Get Product name */
static inst_code
i1d3_get_prodname(
	i1d3 *p,				/* Object */
	char *rv				/* 32 byte buffer */
) {
	unsigned char todev[64];
	unsigned char fromdev[64];
	inst_code ev;
	int isdeb = p->icom->debug;

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_prodname, todev, fromdev, 1.0)) != inst_ok)
		return ev;
	
	strncpy((char *)rv, (char *)fromdev + 2, 31);

	if (isdeb) fprintf(stderr,"i1d3: get prodname got '%s'\n",rv);

	return inst_ok;
}

/* Get Product type number */
static inst_code
i1d3_get_prodtype(
	i1d3 *p,				/* Object */
	int *stat				/* 16 bit version number */
) {
	unsigned char todev[64];
	unsigned char fromdev[64];
	inst_code ev;
	int isdeb = p->icom->debug;

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_prodtype, todev, fromdev, 1.0)) != inst_ok)
		return ev;
	
	*stat = buf2short(fromdev + 3);

	if (isdeb) fprintf(stderr,"i1d3: get_prodtype got 0x%x\n",*stat);

	return inst_ok;
}

/* Get firmware version */
static inst_code
i1d3_get_firmver(
	i1d3 *p,				/* Object */
	char *rv				/* 32 byte buffer */
) {
	unsigned char todev[64];
	unsigned char fromdev[64];
	inst_code ev;
	int isdeb = p->icom->debug;

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_firmver, todev, fromdev, 1.0)) != inst_ok)
		return ev;
	
	strncpy((char *)rv, (char *)fromdev + 2, 31);

	if (isdeb) fprintf(stderr,"i1d3: get firmver got '%s'\n",rv);

	return inst_ok;
}

/* Get firmware date name */
static inst_code
i1d3_get_firmdate(
	i1d3 *p,				/* Object */
	char *rv				/* 32 byte buffer */
) {
	unsigned char todev[64];
	unsigned char fromdev[64];
	inst_code ev;
	int isdeb = p->icom->debug;

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_firmdate, todev, fromdev, 1.0)) != inst_ok)
		return ev;
	
	strncpy((char *)rv, (char *)fromdev + 2, 31);

	if (isdeb) fprintf(stderr,"i1d3: get firmdate got '%s'\n",rv);

	return inst_ok;
}

/* Check the lock status */
static inst_code
i1d3_lock_status(
	i1d3 *p,				/* Object */
	int *stat				/* Status - 0 if Unlocked, 1 if locked */
) {
	unsigned char todev[64];
	unsigned char fromdev[64];
	inst_code ev;
	int isdeb = p->icom->debug;

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_locked, todev, fromdev, 1.0)) != inst_ok)
		return ev;
	
	*stat = 1;		/* Locked */
	if (fromdev[2] != 0 || fromdev[3] == 0)
		*stat = 0;		/* Not Locked */

	if (isdeb) fprintf(stderr,"i1d3: lock_status got %s\n",*stat == 1 ? "Locked" : "Unlocked");

	return inst_ok;
}

static void create_unlock_response(unsigned int *k, unsigned char *c, unsigned char *r);


/* Unlock the device */
static inst_code
i1d3_unlock(
	i1d3 *p				/* Object */
) {
	unsigned char todev[64];
	unsigned char fromdev[64];
	struct {
		char *pname;							/* Product name */
		unsigned int key[2];					/* Unlock code */
		i1d3_dtype dtype;						/* Base type enumerator */
		i1d3_dtype stype;						/* Sub type enumerator */
	} codes[] = {
		{ "i1Display3 ",         { 0xe9622e9f, 0x8d63e133 }, i1d3_disppro, i1d3_disppro },
		{ "Colormunki Display ", { 0xe01e6e0a, 0x257462de }, i1d3_munkdisp, i1d3_munkdisp },
		{ "i1Display3 ",         { 0xcaa62b2c, 0x30815b61 }, i1d3_disppro, i1d3_oem },
		{ "i1Display3 ",         { 0xa9119479, 0x5b168761 }, i1d3_disppro, i1d3_nec_ssp },
		{ NULL } 
	}; 
	inst_code ev;
	int isdeb = p->icom->debug;
	int ix;

	if (isdeb) fprintf(stderr,"i1d3: Unlock:\n");

	/* Until we give up */
	for (ix = 0;;ix++) {

		/* If we've run out of unlock keys */
		if (codes[ix].pname == NULL)
			return i1d3_interp_code((inst *)p, I1D3_UNKNOWN_UNLOCK);

//		return i1d3_interp_code((inst *)p, I1D3_UNLOCK_FAIL);

		/* Skip any keys that don't match the product name */
		if (strcmp(p->prod_name, codes[ix].pname) != 0) {
			continue;
		}

//		if (isdeb) fprintf(stderr,"i1d3: Trying unlock key 0x%08x 0x%08x\n",
//			codes[ix].key[0], codes[ix].key[1]);

		p->dtype = codes[ix].dtype;
		p->stype = codes[ix].stype;

		/* Attempt unlock */
		memset(todev, 0, 64);
		memset(fromdev, 0, 64);

		/* Get a challenge */
		if ((ev = i1d3_command(p, i1d3_lockchal, todev, fromdev, 1.0)) != inst_ok)
			return ev;

		/* Convert challenge to response */
		create_unlock_response(codes[ix].key, fromdev, todev);

		/* Send the response */
		if ((ev = i1d3_command(p, i1d3_lockresp, todev, fromdev, 1.0)) != inst_ok)
			return ev;

		if (fromdev[2] == 0x77) {		/* Sucess */
			break;
		}

		if (isdeb) fprintf(stderr,"i1d3: Trying next unlock key\n");
		/* Try the next key */
	}

	return inst_ok;
}

/* Get the ambient diffuser position */
static inst_code
i1d3_get_diffpos(
	i1d3 *p,				/* Object */
	int *pos				/* 0 = display, 1 = ambient */
) {
	unsigned char todev[64];
	unsigned char fromdev[64];
	inst_code ev;
	int isdeb = p->icom->debug;

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_get_diff, todev, fromdev, 1.0)) != inst_ok)
		return ev;
	
	*pos = fromdev[1];

	if (isdeb) fprintf(stderr,"i1d3: i1d3_get_diff got %d\n",*pos);

	return inst_ok;
}

/* Read bytes from the internal EEPROM */
static inst_code
i1d3_read_internal_eeprom(
	i1d3 *p,				/* Object */
	int addr,				/* address, 0 .. 255 */
	int len,				/* length, 0 .. 255 */
	unsigned char *bytes	/* return bytes here */
) {
	inst_code ev;
	unsigned char todev[64];
	unsigned char fromdev[64];
	int ll;
	int isdeb = p->icom->debug;

	if (addr < 0 || addr > 255)
		return i1d3_interp_code((inst *)p, I1D3_BAD_MEM_ADDRESS);

	if (len < 0 || (addr + len) > 256)
		return i1d3_interp_code((inst *)p, I1D3_BAD_MEM_LENGTH);
		
	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	/* Bread read up into 60 bytes packets */
	for (; len > 0; addr += ll, bytes += ll, len -= ll) {
		ll = len;
		if (ll > 60)
			ll = 60;

		/* OEM driver retries several times after a 10msec sleep on failure. */
		/* Can a failure actually happen though ? */
		todev[1] = (unsigned char)addr;
		todev[2] = (unsigned char)ll;
	
		p->icom->debug = 0;
		if ((ev = i1d3_command(p, i1d3_readintee, todev, fromdev, 1.0)) != inst_ok) {
			p->icom->debug = isdeb;
			return ev;
		}
	
		memmove(bytes, fromdev + 4, ll);
	}

	p->icom->debug = isdeb;
	return inst_ok;
}

/* Read bytes from the external EEPROM */
static inst_code
i1d3_read_external_eeprom(
	i1d3 *p,				/* Object */
	int addr,				/* address, 0 .. 8191 */
	int len,				/* length, 0 .. 8192 */
	unsigned char *bytes	/* return bytes here */
) {
	inst_code ev;
	unsigned char todev[64];
	unsigned char fromdev[64];
	int ll;
	int isdeb = p->icom->debug;

	if (addr < 0 || addr > 8191)
		return i1d3_interp_code((inst *)p, I1D3_BAD_MEM_ADDRESS);

	if (len < 0 || (addr + len) > 8192)
		return i1d3_interp_code((inst *)p, I1D3_BAD_MEM_LENGTH);
		
	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	/* Bread read up into 59 bytes packets */
	for (; len > 0; addr += ll, bytes += ll, len -= ll) {
		ll = len;
		if (ll > 59)
			ll = 59;

		/* OEM driver retries several times after a 10msec sleep on failure. */
		/* Can a failure actually happen though ? */
		short2bufBE(todev + 1, addr);
		todev[3] = (unsigned char)ll;
	
		p->icom->debug = 0;
		if ((ev = i1d3_command(p, i1d3_readextee, todev, fromdev, 1.0)) != inst_ok) {
			p->icom->debug = isdeb;
			return ev;
		}
	
		memmove(bytes, fromdev + 5, ll);
	}

	p->icom->debug = isdeb;
	return inst_ok;
}


/* Take a raw measurement using a given integration time. */
/* The measureent is the count of (both) edges from the L2V */
/* over the integration time */
static inst_code
i1d3_freq_measure(
	i1d3 *p,				/* Object */
	double *inttime,		/* Integration time in seconds. (Return clock rounded) */
	double rgb[3]			/* Return the RGB values */
) {
	int intclks;
	unsigned char todev[64];
	unsigned char fromdev[64];
	inst_code ev;

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if (*inttime > 20.0)		/* Hmm */
		*inttime = 20.0;

	/* Max = 357.9 seconds ? */
	intclks = (int)(*inttime * p->clk_freq + 0.5);
	*inttime = (double)intclks / p->clk_freq;

	int2buf(todev + 1, intclks);

	todev[23] = 0;			/* Unknown parameter, always 0 */
	
	if ((ev = i1d3_command(p, i1d3_measure1, todev, fromdev, 20.0)) != inst_ok)
		return ev;
	
	rgb[0] = (double)buf2uint(fromdev + 2);
	rgb[1] = (double)buf2uint(fromdev + 6);
	rgb[2] = (double)buf2uint(fromdev + 10);

	return inst_ok;
}

/* Take a raw measurement that returns the number of clocks */
/* between and initial edge and edgec[] subsequent edges of the L2F. */
/* The edge count must be between 1 and 65535 inclusive. */ 
/* Both edges are counted. It's advisable to use and even edgec[], */
/* because the L2F output may not be symetric. */
/* If there are no edges within 10 seconds, return a count of 0 */
static inst_code
i1d3_period_measure(
	i1d3 *p,				/* Object */
	int edgec[3],			/* Measurement edge count for each channel */
	int mask,				/* Bit mask to enable channels */
	double rgb[3]			/* Return the RGB values */
) {
	unsigned char todev[64];
	unsigned char fromdev[64];
	inst_code ev;

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	short2buf(todev + 1, edgec[0]);
	short2buf(todev + 3, edgec[1]);
	short2buf(todev + 5, edgec[2]);

	todev[7] = (unsigned char)mask;	
	todev[8] = 0;			/* Unknown parameter, always 0 */	
	
	if ((ev = i1d3_command(p, i1d3_measure2, todev, fromdev, 20.0)) != inst_ok)
		return ev;
	
	rgb[0] = (double)buf2uint(fromdev + 2);
	rgb[1] = (double)buf2uint(fromdev + 6);
	rgb[2] = (double)buf2uint(fromdev + 10);

	return inst_ok;
}

typedef enum {
	i1d3_flash = 1,
	i1d3_fade  = 3,
} i1d3_ledmode;

static inst_code
i1d3_set_LEDs(
	i1d3 *p,				/* Object */
	i1d3_ledmode mode,		/* 1 = off & on, 3 = off & fade on */ 
	double offtime,			/* Off time */
	double ontime,			/* On time. Fade is included in this */
	int count				/* Pulse count. 0x80 = infinity ? */
) {
	unsigned char todev[64];
	unsigned char fromdev[64];
	inst_code ev;
	double mul1, mul2;
	int ftime, ntime; 

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	mul1 = p->clk_freq/(1 << 23);
	mul2 = p->clk_freq/(1 << 19);

	ftime = (int)(0.5 + offtime * mul2);
	if (ftime < 0)
		ftime = 0;
	else if (ftime > 255)
		ftime = 255;

	if (mode == 1)
		ntime = (int)(0.5 + ontime * mul2);
	else if (mode == 3)
		ntime = (int)(0.5 + ontime * mul1);
	else 
		return i1d3_interp_code((inst *)p, I1D3_BAD_LED_MODE);

	if (ntime < 0)
		ntime = 0;
	else if (ntime > 255)
		ntime = 255;

	if (count < 0)
		count = 0;
	else if (count > 0x80)
		count = 0x80;

	todev[1] = (unsigned char)mode;
	todev[2] = (unsigned char)ftime;
	todev[3] = (unsigned char)ntime;
	todev[4] = (unsigned char)count;

	if ((ev = i1d3_command(p, i1d3_setled, todev, fromdev, 1.0)) != inst_ok)
		return ev;

	return inst_ok;
}



/* - - - - - - - - - - - - - - - - - - - - - - */
/*

	determining the refresh rate for a refresh type display;

	Read 1300 .5 msec samples as fast as possible, and
	timestamp them.
	Interpolate values up to .05 msec regular samples.
	Do an auto-correlation on the samples.
	Pick the longest peak between 10 andf 40Hz as the best sample period,
	and halve this to use as the quantization value (ie. make
	it lie between 20 and 80 Hz).

	If there was no error, return refresh quanization period it.

	If there is no aparent refresh, or the refresh rate is not determinable,
	return a period of 0.0 and inst_ok;

	To break up the USB synchronization, the integration time
	is randomized slightly.
*/

#ifndef PSRAND32L 
# define PSRAND32L(S) ((S) * 1664525L + 1013904223L)
#endif
#define NFSAMPS 1300		/* Number of samples to read */
#define NFMXTIME 6.0		/* Maximum time to take (2000 == 6) */
#define PBPMS 20		/* bins per msec */
//#define PERMIN ((1000 * PBPMS)/80)	/* 80 Hz */
//#define PERMAX ((1000 * PBPMS)/20)	/* 20 Hz*/
#define PERMIN ((1000 * PBPMS)/40)	/* 40 Hz */
#define PERMAX ((1000 * PBPMS)/10)	/* 10 Hz*/
#define NPER (PERMAX - PERMIN + 1)
#define PWIDTH (3 * PBPMS)			/* 3 msec bin spread to look for peak in */
#define MAXPKS 20					/* Number of peaks to find */

static inst_code
i1d3_measure_refresh(
	i1d3 *p,			/* Object */
	double *period
) {
	inst_code ev;
	int i, j, k;
	double ucalf = 1.0;				/* usec_time calibration factor */
	double inttimel = 0.0003;
	double inttimeh = 0.0040;
	double sutime, putime, cutime;
	unsigned int randn = 0x12345678;
	struct {
		double itime;	/* Integration time */
		double sec;
		double rgb[3];
	} samp[NFSAMPS];
	int nfsamps;		/* Actual samples read */
	double maxt;		/* Time range */
	int nbins;
	double *bins[3];	/* PBPMS sample bins */
	double corr[NPER];	/* Correlation for each period value */
	double mincv, maxcv;	/* Max and min correlation values */
	double crange;			/* Correlation range */
	double peaks[MAXPKS];		/* Each peak from longest to shortest */
	int npeaks = 0;				/* Number of peaks */
	double pval;			/* Period value */
	int isdeb, iscdeb;

	if (usec_time() < 0.0) {
		if (p->debug) fprintf(stderr,"i1d3: No high resolution timers\n");
		return inst_internal_error; 
	}

	isdeb = p->debug;
	iscdeb = p->icom->debug;
	p->icom->debug = 0;
	p->debug = 0;

	/* Do some measurement and throw them away, to make sure the code is in cache. */
	for (i = 0; i < 5; i++) {
		if ((ev = i1d3_freq_measure(p, &inttimeh, samp[i].rgb)) != inst_ok)
	 		return ev;
	}

#ifdef NEVER		/* This appears to be unnecessary */
	/* Calibrate the usec timer against the instrument */
	{
		double inttime1, inttime2;
		inttime1 = 0.001; 
		inttime2 = 0.501; 

		sutime = usec_time();

		if ((ev = i1d3_freq_measure(p, &inttime1, samp[0].rgb)) != inst_ok)
	 		return ev;

		putime = usec_time();

		if ((ev = i1d3_freq_measure(p, &inttime2, samp[0].rgb)) != inst_ok)
	 		return ev;

		cutime = usec_time();

		ucalf = 1000000.0 * (inttime2 - inttime1)/(cutime - 2.0 * putime + sutime);

		if (p->verb) printf("Clock calibration factor = %f\n",ucalf);
	}
#endif

	/* Read the samples */
	sutime = usec_time();
	putime = (usec_time() - sutime) / 1000000.0;
	for (i = 0; i < NFSAMPS; i++) {
		double rval;
		
		randn = PSRAND32L(randn); 
		rval = (double)randn/4294967295.0;
		rval *= rval;
		rval *= rval;		/* Sharpen it up */
		samp[i].itime = (inttimeh - inttimel) * rval + inttimel;

		if ((ev = i1d3_freq_measure(p, &samp[i].itime, samp[i].rgb)) != inst_ok)
 			return ev;
		cutime = (usec_time() - sutime) / 1000000.0;
		samp[i].sec = 0.5 * (putime + cutime);	/* Mean of before and after stamp */
		putime = cutime;
		if (cutime > NFMXTIME)
			break; 
	}
	nfsamps = i;
	if (nfsamps < 100) {
		*period = 0.0;
		if (p->verb) printf("No distict refresh period\n");
		if (p->debug) fprintf(stderr,"i1d3: Couldn't find a distinct refresh frequency\n");
		return inst_ok; 
	}
 
	p->icom->debug = iscdeb;
	p->debug = isdeb;
	if (p->verb) printf("Read %d samples for refresh calibration\n",nfsamps);

#ifdef NEVER
	/* Plot the raw sensor values */
	{
		double xx[NFSAMPS];
		double y1[NFSAMPS];
		double y2[NFSAMPS];
		double y3[NFSAMPS];

		for (i = 0; i < nfsamps; i++) {
			xx[i] = samp[i].sec;
			y1[i] = samp[i].rgb[0];
			y2[i] = samp[i].rgb[1];
			y3[i] = samp[i].rgb[2];
		//printf("%d: %f -> %f\n",i,samp[i].sec, samp[i].rgb[0]);
		}
		printf("Fast scan sensor values and time (sec)\n");
		do_plot6(xx, y1, y2, y3, NULL, NULL, NULL, nfsamps);
	}
#endif

	/* Re-zero the sample times, normalise int time, and calibrate it. */
	maxt = -1e6;
	for (i = nfsamps-1; i >= 0; i--) {
		samp[i].sec -= samp[0].sec; 
		samp[i].sec *= ucalf;
		if (samp[i].sec > maxt)
			maxt = samp[i].sec;
		for (j = 0; j < 3; j++) {
			samp[i].rgb[j] /= samp[i].itime;
		}
	}

	/* Create PBPMS bins and interpolate readings into them */
	nbins = 1 + (int)(maxt * 1000.0 * PBPMS + 0.5);
	for (j = 0; j < 3; j++) {
		if ((bins[j] = (double *)calloc(sizeof(double), nbins)) == NULL)
			error("i1d3: malloc failed in i1d3_measure_refresh()!");
	}

	/* Do the interpolation */
	for (k = 0; k < (nfsamps-1); k++) {
		int sbin, ebin;
		sbin = (int)(samp[k].sec * 1000.0 * PBPMS + 0.5);
		ebin = (int)(samp[k+1].sec * 1000.0 * PBPMS + 0.5);
		for (i = sbin; i <= ebin; i++) {
			double bl;
#if defined(__APPLE__) && defined(__POWERPC__)
			gcc_bug_fix(i);
#endif
			bl = (i - sbin)/(double)(ebin - sbin);	/* 0.0 to 1.0 */
			for (j = 0; j < 3; j++) {
				bins[j][i] = (1.0 - bl) * samp[k].rgb[j] + bl * samp[k+1].rgb[j];
			}
		} 
	}

#ifdef PLOT_REFRESH
	/* Plot interpolated values */
	{
		double *xx;
		double *y1;
		double *y2;
		double *y3;

		xx = malloc(sizeof(double) * nbins);
		y1 = malloc(sizeof(double) * nbins);
		y2 = malloc(sizeof(double) * nbins);
		y3 = malloc(sizeof(double) * nbins);

		if (xx == NULL || y1 == NULL || y2 == NULL || y3 == NULL)
			error("i1d3: malloc failed in i1d3_measure_refresh()!");
		for (i = 0; i < nbins; i++) {
			xx[i] = i / (double)PBPMS;			/* msec */
			y1[i] = bins[0][i];
			y2[i] = bins[1][i];
			y3[i] = bins[2][i];
		}
		printf("Interpolated fast scan sensor values and time (msec)\n");
		do_plot6(xx, y1, y2, y3, NULL, NULL, NULL, nbins);

		free(xx);
		free(y1);
		free(y2);
		free(y3);
	}
#endif /* PLOT_REFRESH */

	/* Compute auto-correlation at 1/PBPMS msec intervals */
	/* from 12.5 msec (80Hz) to 50msec (20 Hz) */
	mincv = 1e48, maxcv = -1e48;
	for (i = 0; i < NPER; i++) {
		int poff = PERMIN + i;		/* Offset to corresponding sample */
		corr[i] = 0.0;

		for (k = 0; (k + poff) < nbins; k++) {
			for (j = 0; j < 3; j++) {
				corr[i] += bins[j][k] * bins[j][k + poff];
			}
		}
		corr[i] /= (double)k;		/* Normalize */
		if (corr[i] > maxcv)
			maxcv = corr[i];
		if (corr[i] < mincv)
			mincv = corr[i];
	}
	crange = maxcv - mincv;
	DBG(("Corr value range %f - %f = %f = %f%%\n",mincv, maxcv,crange, 100.0 * (maxcv-mincv)/maxcv))

#ifdef PLOT_REFRESH
	/* Plot auto correlation */
	{
		double xx[NPER];
		double y1[NPER];

		for (i = 0; i < NPER; i++) {
			xx[i] = (i + PERMIN) / (double)PBPMS;			/* msec */
			y1[i] = corr[i];
		}
		printf("Auto correlation (msec)\n");
		do_plot6(xx, y1, NULL, NULL, NULL, NULL, NULL, NPER);
	}
#endif /* PLOT_REFRESH */

	/* If there is distict correlations */
	if ((maxcv-mincv)/maxcv > 0.20) {

		/* Locate all the peaks starting at the longest correllation */
		for (i = (NPER-1-PWIDTH); i >= 0 && npeaks < MAXPKS; i--) {
			double v1, v2, v3;
			v1 = corr[i];
			v2 = corr[i + PWIDTH/2];
			v3 = corr[i + PWIDTH];

			if (fabs(v3 - v1) < (0.1 * crange)
			 && (v2 - v1) > (0.05 * crange)
			 && (v2 - v3) > (0.05 * crange)) {
				double pkv;			/* Peak value */
				int pki;			/* Peak index */
				double ii, bl;

				DBG(("Max between %f and %f msec\n",(i + PERMIN)/(double)PBPMS,(i + PWIDTH + PERMIN)/(double)PBPMS))

				/* Locate the actual peak */
				pkv = -1.0;
				pki = 0;
				for (j = i; j < (i + PWIDTH); j++) {
					if (corr[j] > pkv) {
						pkv = corr[j];
						pki = j;
					}
				}
				DBG(("Peak is at %f msec, %f corr\n", (pki + PERMIN)/(double)PBPMS, pkv))

				/* Interpolate the peak value for higher precision */
				/* j = bigest */
				if (corr[pki-1] > corr[pki+1])  {
					j = pki-1;
					k = pki+1;
				} else {
					j = pki+1;
					k = pki-1;
				}
				bl = (corr[pki] - corr[j])/(corr[pki] - corr[k]);
				bl = (bl + 1.0)/2.0;
				ii = bl * pki + (1.0 - bl) * j;
				pval = (ii + PERMIN)/(double)PBPMS;

				DBG(("Interpolated peak is at %f msec\n", pval))

				peaks[npeaks++] = pval;

				i -= PWIDTH;
			}
		}
	}

	DBG(("Number of peaks located = %d\n",npeaks))
	if (npeaks == 0) {
		*period = 0.0;
		if (p->verb) printf("No distict refresh period\n");
		if (p->debug) fprintf(stderr,"i1d3: Couldn't find a distinct refresh frequency\n");
		return inst_ok; 
	}

	if (npeaks == 1) {
		pval = peaks[0] / 2000.0;	/* Scale by half and convert to seconds */

	} else {
		double div;
		double avg, ano;
		/* Try and locate a common divisor amongst all the peaks. */
		/* This is likely to be the underlying refresh rate. */
		for (j = 1; j < 6; j++) {
			div = peaks[npeaks-1]/(double)j;
			avg = ano = 0.0;
			for (i = 0; i < npeaks; i++) {
				double rem, cnt;

				rem = peaks[i]/div;
				cnt = floor(rem + 0.5);
				rem = fabs(rem - cnt);

//printf("~1 remainder for peak %d = %f\n",i,rem);
				if (rem > 0.04)
					break;
				avg += peaks[i];		/* Already weighted by cnt */
				ano += cnt;
			}

			if (i >= npeaks)
				break;		/* Sucess */
			
		}
		if (j >= 6) {
			DBG(("Failed to locate common divisor\n"))
			pval = peaks[0] / 2000.0;	/* Scale by half and convert to seconds */
		} else {
			int mul;
			pval = avg/ano;
			pval /= 1000.0;		/* Convert to seconds */

			if (p->verb) printf("Refresh rate = %f Hz\n",1.0/pval);

			/* Error against my 85Hz CRT - GWG */
//			printf("Refresh rate error = %.4f%%\n",100.0 * fabs(1.0/pval - 85.0)/(85.0));	

			/* Scale to just above 20 Hz */
			mul = (int)floor((1.0/20) / pval);
			pval *= mul;
		}
	}

	if (p->verb) printf("Refresh wampling period = %f msec\n",pval * 1000);
	if (p->debug) fprintf(stderr,"i1d3: Refresh sampling period = %f msec\n",pval);

	*period = pval;

	return inst_ok;
}
#undef NFSAMPS 
#undef PBPMS
#undef PERMIN
#undef PERMAX
#undef NPER
#undef PWIDTH

/* - - - - - - - - - - - - - - - - - - - - - - */

/* Take an ambient measurement and return the cooked reading */
/* The cooked reading is the frequency of the L2V */
static inst_code
i1d3_take_amb_measurement(
	i1d3 *p,			/* Object */
	double *rgb			/* Return the ambient RGB values */
) {
	int i;				/* Returned byte - not used */
	int pos;
	inst_code ev;

	if (p->inited == 0)
		return i1d3_interp_code((inst *)p, I1D3_NOT_INITED);

	DBG(("take_amb_measurement called\n"));

	/* Check that the ambient filter is in place */
	if ((ev = i1d3_get_diffpos(p, &pos)) != inst_ok)
 		return ev;

	if (pos != 1)
		return i1d3_interp_code((inst *)p, I1D3_SPOS_AMB);

	if ((ev = i1d3_freq_measure(p, &p->inttime, rgb)) != inst_ok)
 		return ev;

	/* Scale to account for counting both edges (?) over integration time */
	/* and subtract black level */
	for (i = 0; i < 3; i++) {
		rgb[i] *= 0.5/p->inttime;
		rgb[i] -= p->black[i];
		if (rgb[i] < 0.0)
			rgb[i] = 0.0;
	}

	DBG(("take_amb_measurement returned %s\n",icmPdv(3,rgb)));

	return inst_ok;
}



/* - - - - - - - - - - - - - - - - - - - - - - */

//#define DEBUG
//#undef DBG
//#define DBG(xxx) printf xxx ;

/* Take an display measurement and return the cooked reading */
/* The cooked reading is the frequency of the L2V */
static inst_code
i1d3_take_emis_measurement(
	i1d3 *p,			/* Object */
	i1d3_mmode mode,	/* Measurement mode */
	double *rgb			/* Return the cooked emsissive RGB values */
) {
	int i;
	int pos;
	inst_code ev;
	double rmeas[3] = { -1.0, -1.0, -1.0 };	/* raw measurement */
	int edgec[3] = {2,2,2};	/* Measurement edge count for each channel (not counting start edge) */
	int mask = 0x7;			/* Period measure mask */
#ifdef DEBUG
	int msecstart = msec_time();
#endif

	if (p->inited == 0)
		return i1d3_interp_code((inst *)p, I1D3_NOT_INITED);

	DBG(("\ntake_emis_measurement called\n"));

	/* Check that the ambient filter is not place */
	if ((ev = i1d3_get_diffpos(p, &pos)) != inst_ok)
 		return ev;

	if (pos != 0)
		return i1d3_interp_code((inst *)p, I1D3_SPOS_EMIS);


	/* If we should take a frequency measurement first */
	if (mode == i1d3_adaptive || mode == i1d3_frequency) {

		/* Typically this is 200msec */
		DBG(("Doing fixed period frequency measurement over %f secs\n",p->inttime));

		/* Take a frequency measurement over a fixed period */
		if ((ev = i1d3_freq_measure(p, &p->inttime, rmeas)) != inst_ok)
 			return ev;

		/* Convert to frequency (assume raw meas is both edges count over integration time) */
		for (i = 0; i < 3; i++) {
			rgb[i] = (0.5 * rmeas[i])/p->inttime;
		}

		DBG(("Got %s raw, %s Hz\n",icmPdv(3,rmeas),icmPdv(3,rgb)));
	}

	/* If some period measurement will be done */
	if (mode != i1d3_frequency) {

		/* Decide if a period measurement is needed */
		if (mode == i1d3_adaptive) {

			mask = 0x0;

			for (i = 0; i < 3; i++) {
				/* Not measured or count is too small for desired precision. */
				/* (We're being twice as critical as the OEM driver here) */
				if (rmeas[i] < 200.0) {		/* Could be 0.25% quantization error */
					DBG(("chan %d needs period reading\n",i));
					mask |= 1 << i;			
				} else {
					DBG(("chan %d has sufficient frequeny count\n",i));
//					edgec[i] = 0;
				}
			}
		}

		if (mask != 0x0) {	/* Some measurement wasn't accurate enough, so use period */
			int mask2 = mask;

			/* See if we need to do some pre-measurement to compute how many */
			/* edges to count. */
			for (i = 0; i < 3; i++) {
				if ((mask & (1 << i)) == 0)
					continue;
				
				if (rmeas[i] < 10.0) {
					DBG(("chan %d needs pre-measurement\n",i));
					mask2 |= 1 << i;			
				} else {
					double freq;
					mask2 &= ~(1 << i);			
					/* Convert rmeas[i] from frequency to period equivalent */
					/* for subsequent calculations */
					freq = (rmeas[i] * 0.5)/p->inttime;
					rmeas[i] = (0.5 * edgec[i] * p->clk_freq)/freq; 
					DBG(("chan %d has sufficient frequeny count to avoid pre-measure (rmeas_p %f)\n",i,rmeas[i]));
				}
			}
			if (mask2 != 0x0) {
				int mask3 = 0x0;

				DBG(("Doing 1st period pre-measurement mask 0x%x, edgec %s\n",mask2,icmPiv(3,edgec)));
				/* Take an initial period pre-measurement over 2 edges */
				if ((ev = i1d3_period_measure(p, edgec, mask2, rmeas)) != inst_ok)
		 			return ev;

				DBG(("Got %s raw %f %f %f Hz\n",icmPdv(3,rmeas),
				     0.5 * edgec[0] * p->clk_freq/rmeas[0],
				     0.5 * edgec[1] * p->clk_freq/rmeas[1],
				     0.5 * edgec[2] * p->clk_freq/rmeas[2]));

				/* Do 2nd initial measurement if the count is small, in case */
				/* we are measuring a CRT with a refresh rate which adds innacuracy, */
				/* and could result in a unecessarily long re-reading. */
				/* Don't do this for Munki Display, because of its slow measurements. */
				if (p->dtype != i1d3_munkdisp) {
					for (i = 0; i < 3; i++) {
						if ((mask2 & (1 << i)) == 0)
							continue;

						if (rmeas[i] > 0.5) {
							double nedgec;
							int inedgec;

							/* Compute number of edges needed for a clock count */
							/* of 0.100 seconds */
							nedgec = edgec[i] * 0.100 * p->clk_freq/rmeas[i];

							DBG(("chan %d target edges %f\n",i,nedgec));

							/* Limit to a legal range */
							if (nedgec > 65534.0)
								nedgec = 65534.0;
							else if (nedgec < 2.0)
								nedgec = 2.0;

							/* Round to nearest even edge count */
							inedgec = 2 * (int)floor(nedgec/2.0 + 0.5);

							DBG(("chan %d set edgec to %d\n",i,inedgec));

							/* Don't do 2nd initial measure if we have fewer number of edges */
							if (inedgec > edgec[i]) {
								mask3 |= (1 << i);
								edgec[i] = inedgec;
							}
						}
#ifdef DEBUG
						else printf("chan %d had no reading, so skipping period measurement\n",i);
#endif
					}
					if (mask3 != 0x0) {
						double rmeas2[3];

						DBG(("Doing 2nd initial period measurement mask 0x%x, edgec %s\n",mask3,icmPiv(3,edgec)));
						/* Take a 2nd initial period  measurement */
						if ((ev = i1d3_period_measure(p, edgec, mask3, rmeas2)) != inst_ok)
				 			return ev;

						DBG(("Got %s raw %f %f %f Hz\n",icmPdv(3,rmeas2),
						     0.5 * edgec[0] * p->clk_freq/rmeas2[0],
						     0.5 * edgec[1] * p->clk_freq/rmeas2[1],
						     0.5 * edgec[2] * p->clk_freq/rmeas2[2]));

						/* Transfer updated counts from 2nd initial measurement */
						for (i = 0; i < 3; i++) {
							if ((mask3 & (1 << i)) == 0)
								continue;
#ifdef DEBUG
							{
								double ratio;
								if (rmeas2[i] > rmeas[i])
									ratio = rmeas2[i] / rmeas[i];
								else
									ratio = rmeas[i] / rmeas2[i];
								printf("Chan %d 1st to 2nd initial value ratio %f\n",i,ratio);
							}
#endif
							rmeas[i]  = rmeas2[i];
						}
					}
				}
			}

			/* Now setup for re-measure, aiming for full period measurement */
			for (i = 0; i < 3; i++) {
				if ((mask & (1 << i)) == 0)
					continue;

				if (rmeas[i] > 0.5) {
					double nedgec;

					/* Compute number of edges needed for a clock count */
					/* of 2 * p->inttime (typically 0.4) seconds (ie. typical 2.4e6 clocks). */
					nedgec = edgec[i] * 2.0 * p->inttime * p->clk_freq/rmeas[i];

					DBG(("chan %d target edges %f\n",i,nedgec));

					/* Limit to a legal range */
					if (nedgec > 65534.0)
						nedgec = 65534.0;
					else if (nedgec < 2.0)
						nedgec = 2.0;

					/* Round to nearest even edge count */
					edgec[i] = 2 * (int)floor(nedgec/2.0 + 0.5);

					DBG(("chan %d set edgec to %d\n",i,edgec[i]));

					/* Don't measure again if we have same number of edges */
					if (edgec[i] == 2) {

						/* Use measurement from 2 edges */
						rgb[i] = (p->clk_freq * 0.5 * edgec[i])/rmeas[i];
						mask &= ~(1 << i);
						edgec[i] = 0;

						DBG(("chan %d skipping re-measure, frequency %f\n",i,rgb[i]));
					}
				} else {
					/* Don't measure again, we failed to see any edges */
					rgb[i] = 0.0;
					mask &= ~(1 << i);
					edgec[i] = 0;
				}
			}

			if (mask != 0x0) {
				DBG(("Doing period re-measure mask 0x%x, edgec %s\n",mask,icmPiv(3,edgec)));

				/* Measure again with desired precision, taking up to 0.4/0.8 secs */
				if ((ev = i1d3_period_measure(p, edgec, mask, rmeas)) != inst_ok)
		 			return ev;
	
				for (i = 0; i < 3; i++) {
					if ((mask & (1 << i)) == 0)
						continue;
	
					/* Compute the frequency from period measurement */
					if (rmeas[i] < 0.5)		/* Number of edges wasn't counted */
						rgb[i] = 0.0;
					else
						rgb[i] = (p->clk_freq * 0.5 * edgec[i])/rmeas[i];
					DBG(("chan %d raw %f frequency %f (%f Sec)\n",i,rmeas[i],rgb[i],
					                            rmeas[i]/p->clk_freq));

				}
			}
			DBG(("Got %s Hz after period measure\n",icmPdv(3,rgb)));
		}
	}

	DBG(("Took %d msec to measure\n", msec_time() - msecstart));

	/* Subtract black level */
	for (i = 0; i < 3; i++) {
		rgb[i] -= p->black[i];
		if (rgb[i] < 0.0)
			rgb[i] = 0.0;
	}

	DBG(("Cooked RGB = %f %f %f\n",rgb[0],rgb[1],rgb[2]))
	
	return inst_ok;
}

//#undef DEBUG
//#undef DBG
//#define DBG(xxx) 

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Take a XYZ measurement from the device */
static inst_code
i1d3_take_XYZ_measurement(
	i1d3 *p,				/* Object */
	double XYZ[3]			/* Return the XYZ values */
) {
	inst_code ev;

	if ((p->mode & inst_mode_measurement_mask) == inst_mode_emis_ambient) {
		if ((ev = i1d3_take_amb_measurement(p, XYZ)) != inst_ok)
			return ev;

		/* Multiply by ambient calibration matrix */
		icmMulBy3x3(XYZ, p->ambi_cal, XYZ);
		icmScale3(XYZ, XYZ, 1/3.141592654);		/* Convert from Lux to cd/m^2 */

	} else {

		/* Constant fast speed, poor accuracy for black */
//		if ((ev = i1d3_take_emis_measurement(p, i1d3_frequency, XYZ)) != inst_ok)
//			return ev;

		/* Most accurate ? */
//		if ((ev = i1d3_take_emis_measurement(p, i1d3_period, XYZ)) != inst_ok)
//			return ev;

		/* Best combination */
		if ((ev = i1d3_take_emis_measurement(p, i1d3_adaptive, XYZ)) != inst_ok)
			return ev;

		/* Multiply by current emissive calibration matrix */
		icmMulBy3x3(XYZ, p->emis_cal, XYZ);

		/* Apply the (optional) colorimeter correction matrix */
		icmMulBy3x3(XYZ, p->ccmat, XYZ);

	}
	DBG(("returning XYZ = %f %f %f\n",XYZ[0],XYZ[1],XYZ[2]))
	return inst_ok;
}

// ============================================================

/* Decode the Internal EEPRom */
static inst_code i1d3_decode_intEE(
	i1d3 *p,
	unsigned char *buf			/* Buffer holding 256 bytes from Internal EEProm */
) {
	int i;
	unsigned int t1;

	/* Read the black level offset */
	for (i = 0; i < 3; i++) {
		t1 = buf2uint(buf + 0x0004 + 4 * i);

		if (t1 == 0xffffffff)
			p->black[0] = 0.0;
		else
			p->black[0] = (double)t1/6e6;
	}

	return inst_ok;
}

/* Decode the External EEPRom */
static inst_code i1d3_decode_extEE(
	i1d3 *p,
	unsigned char *buf			/* Buffer holding 8192 bytes from External EEProm */
) {
	int i, j;
	unsigned int off;
	unsigned int chsum, rchsum;
	xspect tmp;

	for (chsum = 0, i = 4; i < 6042; i++)
		chsum += buf[i];

	chsum &= 0xffff;		/* 16 bit sum */

	rchsum = buf2short(buf + 2);

	if (rchsum != chsum)
		return i1d3_interp_code((inst *)p, I1D3_BAD_EX_CHSUM);
		
	/* Read 3 x sensor spectral sensitivits */
	/* These seem to be in Hz per W/nm @ 1nm spacing, */
	/* so convert to Hz per mW/nm which is our default assumption. */
	p->cal_date = buf2ord64(buf + 0x001E);

	for (j = 0; j < 3; j++) {
		p->sens[j].spec_n = 351;
		p->sens[j].spec_wl_short = 380.0;
		p->sens[j].spec_wl_long = 730.0;
		p->sens[j].norm = 1.0;
		for (i = 0, off = 0x0026 + j * 351 * 4; i < 351; i++, off += 4) {
			unsigned int val;
			val = buf2uint(buf + off);
			p->sens[j].spec[i] = IEEE754todouble(val);
			p->sens[j].spec[i] /= 1000;
		}
		p->ambi[j] = p->sens[j];	/* Structure copy */
	}

#ifdef SAVE_SPECTRA
	write_cmf("sensors.cmf", p->sens);
#endif

	/* Read ambient filter spectrum */
	tmp.spec_n = 351;
	tmp.spec_wl_short = 380.0;
	tmp.spec_wl_long = 730.0;
	tmp.norm = 1.0;
	for (i = 0, off = 0x10bc; i < 351; i++, off += 4) {
		unsigned int val;
		val = buf2uint(buf + off);
		tmp.spec[i] = IEEE754todouble(val);
	}

	/* Compute ambient sensor sensitivity by multiplying filter in */
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 351; i++)
			p->ambi[j].spec[i] *= tmp.spec[i];
	}
#ifdef PLOT_SPECTRA
	/* Plot the spectra */
	{
		double xx[351];
		double y1[351];
		double y2[351];
		double y3[351];
		double y4[351];
		double y5[351];
		double y6[351];

		for (i = 0; i < 351; i++) {
			xx[i] = XSPECT_XWL(&tmp, i);
			y1[i] = p->sens[0].spec[i];
			y2[i] = p->sens[1].spec[i];
			y3[i] = p->sens[2].spec[i];
			y4[i] = p->ambi[0].spec[i];
			y5[i] = p->ambi[1].spec[i];
			y6[i] = p->ambi[2].spec[i];
		}
		printf("The sensor and ambient sensor sensitivy curves\n");
		do_plot6(xx, y1, y2, y3, y4, y5, y6, 351);
	}
#endif /* PLOT_SPECTRA */

	// Should try and read 4 factory 3x3 matricies too,
	// even though they are not usually set.

	return inst_ok;
}

/* ------------------------------------------------------------------------ */
/* Calibration code */

/* Maximum Ignorance by Least Squares regression (MIbLSr) Calibration. */
/* This makes no assumption about the spectral distribution of */
/* typical samples or their underlying dimensionality. */
/* We use this as a default means of calibration, and as */
/* a means of calibrating the Ambient readings. */
/* (This matches the OEM default calibrations.) */
/* We could weight this towards minimizing white error */
/* by synthesizing a white patch to add to the "sample" set */
/* (but this might make the result worse!), or we could */
/* add or use spectral shape target (ie. analogous to */
/* one sample per spectral wavelength, weighted by CMF's) */

/* The more general calibration uses a set of spectral samples, */
/* and a least squares matrix is computed to map the sensor RGB */
/* to the computed XYZ values. This allows better accuracy for */
/* a typical display that has only 3 degrees of freedom, and */
/* allows weigting towards a distribution of actual spectral samples. */
/* (The OEM driver supplies .edr files with this information. We use */
/* .ccsp files) */
/* To allow less than 3 samples, extra secondary constraints could be added, */
/* such as CMF's as pseudo-samples or a spectral shape target. */

static inst_code
i1d3_comp_calmat(
	i1d3 *p,
	double mat[3][3],	/* Return calibration matrix from RGB to XYZ */
	icxObserverType obType,	/* XYZ Observer type */
	xspect custObserver[3],	/* Optional custom observer */					\
	xspect *RGBcmfs,	/* Array of 3 sensor CMFs, either emissive or ambient */
	xspect *samples,	/* Array of nsamp spectral samples, or RGBcmfs for MIbLSr */
						/* (~~~ weighting array ? ~~~) */
	int nsamp			/* Number of samples */
) {
	int i, j, k;
	double **sampXYZ;	/* Sample XYZ values */
	double **sampRGB;	/* Sample RGB values */
	double XYZ[3][3];
	double RGB[3][3];
	double iRGB[3][3];
	xsp2cie *conv;

	if (nsamp < 3)
		return i1d3_interp_code((inst *)p, I1D3_TOO_FEW_CALIBSAMP);

	sampXYZ = dmatrix(0, nsamp-1, 0, 3-1);
	sampRGB = dmatrix(0, nsamp-1, 0, 3-1);

	/* Compute XYZ of the sample array */
	if ((conv = new_xsp2cie(icxIT_none, NULL, obType, custObserver, icSigXYZData)) == NULL)
		return i1d3_interp_code((inst *)p, I1D3_INT_CIECONVFAIL);
	for (i = 0; i < nsamp; i++) {
		conv->convert(conv, sampXYZ[i], &samples[i]); 
	}
	conv->del(conv);

	/* Compute sensor RGB of the sample array */
	if ((conv = new_xsp2cie(icxIT_none, NULL, icxOT_custom, RGBcmfs, icSigXYZData)) == NULL) {
		free_dmatrix(sampXYZ, 0, nsamp-1, 0, 3-1);
		free_dmatrix(sampRGB, 0, nsamp-1, 0, 3-1);
		return i1d3_interp_code((inst *)p, I1D3_INT_CIECONVFAIL);
	}
	for (i = 0; i < nsamp; i++) {
		conv->convert(conv, sampRGB[i], &samples[i]); 
		/* But we need to undo lumens scaling, because it doesn't apply to RGB sensor values */
		for (j = 0; j < 3; j++)
			sampRGB[i][j] /= 0.683002;
	}
	conv->del(conv);

	/* If there are exactly 3 samples, we can directly compute the */
	/* correction matrix, since the problem is not over-determined. */
	if (nsamp == 3) {
		copy_dmatrix_to3x3(XYZ, sampXYZ, 0, 2, 0, 2);
		copy_dmatrix_to3x3(RGB, sampRGB, 0, 2, 0, 2);
		if (icmInverse3x3(iRGB, RGB)) {
			free_dmatrix(sampXYZ, 0, nsamp-1, 0, 3-1);
			free_dmatrix(sampRGB, 0, nsamp-1, 0, 3-1);
			return i1d3_interp_code((inst *)p, I1D3_TOO_FEW_CALIBSAMP);
		}

		icmMul3x3_2(mat, iRGB, XYZ);
		icmTranspose3x3(mat, mat);

	/* Otherwise we compute the least squares calibration matrix. */
	} else {
		/* Multiply the [3 x nsamp] XYZ matrix by the [nsamp x 3] RGB */
		/* matrix to produce the [3 x 3] design matrix. */
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++) {
				XYZ[j][i] = 0.0;
				for (k = 0; k < nsamp; k++)
					XYZ[j][i] += sampXYZ[k][i] * sampRGB[k][j];
			}
		}
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++) {
				RGB[j][i] = 0.0;
				for (k = 0; k < nsamp; k++)
					RGB[j][i] += sampRGB[k][i] * sampRGB[k][j];
			}
		}
		if (icmInverse3x3(iRGB, RGB)) {
			free_dmatrix(sampXYZ, 0, nsamp-1, 0, 3-1);
			free_dmatrix(sampRGB, 0, nsamp-1, 0, 3-1);
			return i1d3_interp_code((inst *)p, I1D3_TOO_FEW_CALIBSAMP);
		}

		icmMul3x3_2(mat, iRGB, XYZ);
		icmTranspose3x3(mat, mat);
	}
	free_dmatrix(sampXYZ, 0, nsamp-1, 0, 3-1);
	free_dmatrix(sampRGB, 0, nsamp-1, 0, 3-1);

	return inst_ok;
}


/* ------------------------------------------------------------------------ */

/* Establish communications with a I1D3 */
/* Return DTP_COMS_FAIL on failure to establish communications */
static inst_code
i1d3_init_coms(inst *pp, int port, baud_rate br, flow_control fc, double tout) {
	i1d3 *p = (i1d3 *) pp;
	int stat;
	inst_code ev = inst_ok;
	icomuflags usbflags = icomuf_none;
#ifdef NT
	/* If the X-Rite software has been installed, then there may */
	/* be a utility that has the device open. Kill that process off */
	/* so that we can open it here. */
	char *pnames[] = {
			"i1ProfilerTray.exe",
			NULL
	};
	int retries = 2;
#else /* !NT */
	char **pnames = NULL;
	int retries = 0;
#endif /* !NT */

	if (p->debug) {
		p->icom->debug = p->debug;	/* Turn on debugging */
		fprintf(stderr,"i1d3: About to init coms\n");
	}

	/* On Linux, the i1d3 doesn't seem to close properly - */
	/* something to do with detaching the default HID driver ?? */
#if defined(UNIX) && !defined(__APPLE__)
	usbflags |= icomuf_reset_before_close;
#endif
	/* Open as an HID if available */
	if (p->icom->is_hid_portno(p->icom, port) != instUnknown) {

		if (p->debug) fprintf(stderr,"i1d3: About to init HID\n");

		/* Set config, interface */
		p->icom->set_hid_port(p->icom, port, icomuf_none, retries, pnames); 

	} else if (p->icom->is_usb_portno(p->icom, port) != instUnknown) {

		if (p->debug) fprintf(stderr,"i1d3: About to init USB\n");

		/* Set config, interface, write end point, read end point */
		/* ("serial" end points aren't used - the i1d3 uses USB control messages) */
		/* We need to detatch the HID driver on Linux */
		p->icom->set_usb_port(p->icom, port, 1, 0x00, 0x00, usbflags | icomuf_detach, 0, NULL); 

	} else {
		if (p->debug) fprintf(stderr,"i1d3: init_coms called to wrong device!\n");
			return i1d3_interp_code((inst *)p, I1D3_UNKNOWN_MODEL);
	}

#if defined(UNIX) && defined(__APPLE__)
	/* We seem to have to clear any pending messages for OS X HID */
	i1d3_dummy_read(p);
#endif

	/* Check instrument is responding */
	if ((ev = i1d3_check_status(p,&stat)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: init coms failed with rv = 0x%x\n",ev);
		return ev;
	}
	if (p->debug) fprintf(stderr,"i1d3: init coms has suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

// Print bytes as hex to fp
static void dump_bytes(FILE *fp, char *pfx, unsigned char *buf, int len) {
	int i, j, ii;
	for (i = j = 0; i < len; i++) {
		if ((i % 16) == 0)
			fprintf(fp,"%s%04x:",pfx,i);
		fprintf(fp," %02x",buf[i]);
		if ((i+1) >= len || ((i+1) % 16) == 0) {
			for (ii = i; ((ii+1) % 16) != 0; ii++)
				fprintf(fp,"   ");
			fprintf(fp,"  ");
			for (; j <= i; j++) {
				if (isprint(buf[j]))
					fprintf(fp,"%c",buf[j]);
				else
					fprintf(fp,".");
			}
			fprintf(fp,"\n");
		}
	}
}

/* Initialise the I1D3 */
/* return non-zero on an error, with dtp error code */
static inst_code
i1d3_init_inst(inst *pp) {
	i1d3 *p = (i1d3 *)pp;
	inst_code ev = inst_ok;
	int stat;
	unsigned char buf[8192];

	if (p->debug) fprintf(stderr,"i1d3: About to init instrument\n");

	p->rrset = 0;

	if (p->gotcoms == 0)
		return i1d3_interp_code((inst *)p, I1D3_NO_COMS);	/* Must establish coms first */

	// Get instrument information */
	if ((ev = i1d3_check_status(p, &p->status)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: init_inst failed with rv = 0x%x\n",ev);
	}
	if (p->status != 0) {
		ev = I1D3_BAD_STATUS;
		if (p->debug) fprintf(stderr,"i1d3: init_inst failed with rv = 0x%x\n",ev);
		return ev;
	}
	if ((ev = i1d3_get_prodname(p, p->prod_name)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: init_inst failed with rv = 0x%x\n",ev);
	}
	if ((ev = i1d3_get_prodtype(p, &p->prod_type)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: init_inst failed with rv = 0x%x\n",ev);
	}
	if (p->prod_type == 0x0002) {	/* If ColorMunki Display */
		/* Set this in case it doesn't need unlocking */
		p->dtype = p->stype = i1d3_munkdisp;
	}
	if ((ev = i1d3_get_firmver(p, p->firm_ver)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: init_inst failed with rv = 0x%x\n",ev);
	}
	if ((ev = i1d3_get_firmdate(p, p->firm_date)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: init_inst failed with rv = 0x%x\n",ev);
	}

	/* Unlock instrument */
	if ((ev = i1d3_lock_status(p,&stat)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: init coms failed with rv = 0x%x\n",ev);
		return ev;
	}
	if (stat != 0) {		/* Locked, so unlock it */
		if ((ev = i1d3_unlock(p)) != inst_ok) {
			if (p->debug) fprintf(stderr,"i1d3: init coms failed with rv = 0x%x\n",ev);
			return ev;
		}
		if ((ev = i1d3_lock_status(p,&stat)) != inst_ok) {
			if (p->debug) fprintf(stderr,"i1d3: init coms failed with rv = 0x%x\n",ev);
			return ev;
		}
		if (stat != 0) {
			ev = i1d3_interp_code((inst *)p, I1D3_UNLOCK_FAIL);
			if (p->debug) fprintf(stderr,"i1d3: init coms failed with rv = 0x%x\n",ev);
			return ev;
		}
	}

	/* Read the instrument information and calibration */
	if ((ev = i1d3_read_internal_eeprom(p,0,256,buf)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: read_internal_eeprom failed with rv = 0x%x\n",ev);
		return ev;
	}
	if (p->debug > 2) {
		fprintf(stderr, "Internal EEPROM:\n"); 
		dump_bytes(stderr, "  ", buf, 256);
	}
	/* Decode the Internal EEPRom */
	if ((ev = i1d3_decode_intEE(p, buf)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: decode_internal_eeprom failed with rv = 0x%x\n",ev);
		return ev;
	}

	if ((ev = i1d3_read_external_eeprom(p,0,8192,buf)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: read_external_eeprom failed with rv = 0x%x\n",ev);
		return ev;
	}
	if (p->debug > 2) {
		fprintf(stderr, "External EEPROM:\n"); 
		dump_bytes(stderr, "  ", buf, 8192);
	}
	/* Decode the External EEPRom */
	if ((ev = i1d3_decode_extEE(p, buf)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: decode_external_eeprom failed with rv = 0x%x\n",ev);
		return ev;
	}

	/* Set known constants */
	p->clk_freq = 12e6;		/* 12 Mhz */
	p->dinttime = 0.2;		/* 0.2 second integration time default */
	p->inttime = p->dinttime;	/* Start in non-refresh mode */

	/* Create the default calibrations */
	if ((ev = i1d3_comp_calmat(p, p->emis_cal, icxOT_CIE_1931_2, NULL, p->sens, p->sens, 3)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: create default emissive calibration failed with rv = 0x%x\n",ev);
		return ev;
	}

	if ((ev = i1d3_comp_calmat(p, p->ambi_cal, icxOT_CIE_1931_2, NULL, p->ambi, p->ambi, 3)) != inst_ok) {
		if (p->debug) fprintf(stderr,"i1d3: create default ambient calibration failed with rv = 0x%x\n",ev);
		return ev;
	}

#ifdef DUMP_MATRIX
	printf("Default calibration:\n");
	printf("Ambient matrix  = %f %f %f\n", p->ambi_cal[0][0], p->ambi_cal[0][1], p->ambi_cal[0][2]);
	printf("                  %f %f %f\n", p->ambi_cal[1][0], p->ambi_cal[1][1], p->ambi_cal[1][2]);
	printf("                  %f %f %f\n\n", p->ambi_cal[2][0], p->ambi_cal[2][1], p->ambi_cal[2][2]);
	printf("Emissive matrix = %f %f %f\n", p->emis_cal[0][0], p->emis_cal[0][1], p->emis_cal[0][2]);
	printf("                  %f %f %f\n", p->emis_cal[1][0], p->emis_cal[1][1], p->emis_cal[1][2]);
	printf("                  %f %f %f\n", p->emis_cal[2][0], p->emis_cal[2][1], p->emis_cal[2][2]);
	printf("\n");
#endif

	if (ev == inst_ok) {
		p->inited = 1;
		if (p->debug) fprintf(stderr,"i1d3: instrument inited OK\n");
	}

	/* Flash the LED, just cos we can! */
	if ((ev = i1d3_set_LEDs(p, i1d3_flash, 0.2, 0.05, 2)) != inst_ok)
		return ev;

	return ev;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
i1d3_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val) {		/* Pointer to instrument patch value */
	i1d3 *p = (i1d3 *)pp;
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
			return i1d3_interp_code((inst *)p, icoms2i1d3_err(se, 1));
		}
		user_trig = 1;
		if (p->trig_return)
			printf("\n");
	}

	/* Attempt a refresh display frame rate calibration if needed */
	if (p->dtype != i1d3_munkdisp && p->refmode != 0 && p->rrset == 0) {
		inst_code ev = inst_ok;

		if ((ev = i1d3_measure_refresh(p, &p->refperiod)) != inst_ok)
			return ev; 
		p->rrset = 1;

		/* Quantize the sample time */
		if (p->refperiod > 0.0) {		/* If we have a refresh period *?
			int n;
			n = (int)ceil(p->dinttime/p->refperiod);
			p->inttime = n * p->refperiod;
			if (p->debug) fprintf(stderr,"i1d3: integration time quantize to %f secs\n",p->inttime);

		} else {	/* We don't have a period, so simply double the default */
			p->inttime = 2.0 * p->dinttime;
			if (p->debug) fprintf(stderr,"i1d3: integration time doubled to %f secs\n",p->inttime);
		}
	}

	/* Read the XYZ value */
	if ((rv = i1d3_take_XYZ_measurement(p, val->aXYZ)) != inst_ok)
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
/* To remove the matrix, pass NULL for the matrix. */
inst_code i1d3_col_cor_mat(
inst *pp,
double mtx[3][3]
) {
	i1d3 *p = (i1d3 *)pp;

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
inst_code i1d3_col_cal_spec_set(
inst *pp,
icxObserverType obType,
xspect custObserver[3],
xspect *sets,
int no_sets
) {
	i1d3 *p = (i1d3 *)pp;
	inst_code ev = inst_ok;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (obType == icxOT_default)
		obType = icxOT_CIE_1931_2;
		
	if (sets == NULL || no_sets <= 0) {
		/* Create the default calibrations */
		if ((ev = i1d3_comp_calmat(p, p->emis_cal, obType, custObserver, p->sens, p->sens, 3))
		                                                                           != inst_ok)
			return ev;
		if ((ev = i1d3_comp_calmat(p, p->ambi_cal, obType, custObserver, p->ambi, p->ambi, 3))
		                                                                           != inst_ok)
			return ev;
	} else {
		/* Use given spectral samples */
		if ((ev = i1d3_comp_calmat(p, p->emis_cal, obType, custObserver, p->sens, sets, no_sets))
		                                                                           != inst_ok)
			return ev;
		/* Use MIbLSr */
		if ((ev = i1d3_comp_calmat(p, p->ambi_cal, obType, custObserver, p->ambi, p->ambi, 3))
		                                                                           != inst_ok)
			return ev;
	}
#ifdef DUMP_MATRIX
	printf("CCSS update calibration:\n");
	printf("Ambient matrix  = %f %f %f\n", p->ambi_cal[0][0], p->ambi_cal[0][1], p->ambi_cal[0][2]);
	printf("                  %f %f %f\n", p->ambi_cal[1][0], p->ambi_cal[1][1], p->ambi_cal[1][2]);
	printf("                  %f %f %f\n\n", p->ambi_cal[2][0], p->ambi_cal[2][1], p->ambi_cal[2][2]);
	printf("Emissive matrix = %f %f %f\n", p->emis_cal[0][0], p->emis_cal[0][1], p->emis_cal[0][2]);
	printf("                  %f %f %f\n", p->emis_cal[1][0], p->emis_cal[1][1], p->emis_cal[1][2]);
	printf("                  %f %f %f\n", p->emis_cal[2][0], p->emis_cal[2][1], p->emis_cal[2][2]);
	printf("\n");
#endif
	return ev;
}

/* Determine if a calibration is needed. Returns inst_calt_none if not, */
/* inst_calt_unknown if it is unknown, or inst_calt_crt_freq for an */
/* if a frequency calibration is needed, and we are in refresh mode */
inst_cal_type i1d3_needs_calibration(inst *pp) {
	i1d3 *p = (i1d3 *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->dtype != i1d3_munkdisp && p->refmode != 0 && p->rrset == 0)
		return inst_calt_crt_freq;

	return inst_ok;
}

/* Request an instrument calibration. */
inst_code i1d3_calibrate(
inst *pp,
inst_cal_type calt,		/* Calibration type. inst_calt_all for all neeeded */
inst_cal_cond *calc,	/* Current condition/desired condition */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	i1d3 *p = (i1d3 *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	id[0] = '\000';

	/* Translate default into what's needed or expected default */
	if (calt == inst_calt_all) {
		if (p->dtype != i1d3_munkdisp && p->refmode != 0)
			calt = inst_calt_crt_freq;
	}

	if (calt == inst_calt_crt_freq && p->dtype != i1d3_munkdisp && p->refmode != 0) {
		inst_code ev = inst_ok;

		if (*calc != inst_calc_disp_white) {
			*calc = inst_calc_disp_white;
			return inst_cal_setup;
		}

		/* Do refresh display rate calibration */
		if ((ev = i1d3_measure_refresh(p, &p->refperiod)) != inst_ok)
			return ev; 
		p->rrset = 1;

		/* Quantize the sample time */
		if (p->refperiod > 0.0) {
			int n;
			n = (int)ceil(p->dinttime/p->refperiod);
			p->inttime = n * p->refperiod;
			if (p->debug) fprintf(stderr,"i1d3: integration time quantize to %f secs\n",p->inttime);
		} else {
			p->inttime = 2.0 * p->dinttime;	/* Double integration time */
			if (p->debug) fprintf(stderr,"i1d3: integration time doubled to %f secs\n",p->inttime);
		}
		return inst_ok;
	}
	return inst_unsupported;
}

/* Error codes interpretation */
static char *
i1d3_interp_error(inst *pp, int ec) {
//	i1d3 *p = (i1d3 *)pp;
	ec &= inst_imask;
	switch (ec) {
		case I1D3_INTERNAL_ERROR:
			return "Internal software error";
		case I1D3_COMS_FAIL:
			return "Communications failure";
		case I1D3_UNKNOWN_MODEL:
			return "Not a known Huey Model";
		case I1D3_DATA_PARSE_ERROR:
			return "Data from i1 Display didn't parse as expected";
		case I1D3_USER_ABORT:
			return "User hit Abort key";
		case I1D3_USER_TERM:
			return "User hit Terminate key";
		case I1D3_USER_TRIG:
			return "User hit Trigger key";
		case I1D3_USER_CMND:
			return "User hit a Command key";

		case I1D3_OK:
			return "No device error";

		case I1D3_UNKNOWN_UNLOCK:
			return "Don't know unlock code for device";
		case I1D3_UNLOCK_FAIL:
			return "Device unlock command failed";
		case I1D3_BAD_EX_CHSUM:
			return "External EEPRrom checksum doesn't match";

		case I1D3_SPOS_EMIS:
			return "Ambient filter should be removed";
		case I1D3_SPOS_AMB:
			return "Ambient filter should be used";

		case I1D3_BAD_WR_LENGTH:
			return "Unable to write full message to instrument";
		case I1D3_BAD_RD_LENGTH:
			return "Unable to read full message to instrument";
		case I1D3_BAD_RET_STAT:
			return "Message from instrument had bad status code";
		case I1D3_BAD_RET_CMD:
			return "Message from instrument didn't echo command code";
		case I1D3_BAD_STATUS:
			return "Instrument status is unrecognised format";

		case I1D3_NO_COMS:
			return "Communications hasn't been established";;
		case I1D3_NOT_INITED:
			return "Instrument hasn't been initialized";
		case I1D3_BAD_MEM_ADDRESS:
			return "Out of range EEPROM address";
		case I1D3_BAD_MEM_LENGTH:
			return "Out of range EEPROM length";
		case I1D3_INT_CIECONVFAIL:
			return "Creating spectral to CIE converted failed";
		case I1D3_TOO_FEW_CALIBSAMP:
			return "There are too few spectral calibration samples - need at least 3";
		case I1D3_INT_MATINV_FAIL:
			return "Calibration matrix inversion failed";
		case I1D3_BAD_LED_MODE:
			return "Parameters to set LED are incorrect";

		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
i1d3_interp_code(inst *pp, int ec) {
//	i1d3 *p = (i1d3 *)pp;

	ec &= inst_imask;
	switch (ec) {

		case I1D3_OK:
			return inst_ok;

		case I1D3_BAD_MEM_ADDRESS:
		case I1D3_BAD_MEM_LENGTH:
		case I1D3_INT_CIECONVFAIL:
		case I1D3_TOO_FEW_CALIBSAMP:
		case I1D3_INT_MATINV_FAIL:
		case I1D3_BAD_LED_MODE:
		case I1D3_NO_COMS:
		case I1D3_NOT_INITED:
			return inst_internal_error | ec;

		case I1D3_COMS_FAIL:
			return inst_coms_fail | ec;

		case I1D3_UNKNOWN_UNLOCK:
		case I1D3_UNLOCK_FAIL:
		case I1D3_DATA_PARSE_ERROR:
		case I1D3_BAD_WR_LENGTH:
		case I1D3_BAD_RD_LENGTH:
		case I1D3_BAD_RET_STAT:
		case I1D3_BAD_RET_CMD:
		case I1D3_BAD_STATUS:
			return inst_protocol_error | ec;

		case I1D3_USER_ABORT:
			return inst_user_abort | ec;
		case I1D3_USER_TERM:
			return inst_user_term | ec;
		case I1D3_USER_TRIG:
			return inst_user_trig | ec;
		case I1D3_USER_CMND:
			return inst_user_cmnd | ec;

		case I1D3_SPOS_EMIS:
		case I1D3_SPOS_AMB:
			return inst_wrong_sensor_pos | ec;

		case I1D3_BAD_EX_CHSUM:
			return inst_hardware_fail | ec;

/* Unused:
	inst_notify
	inst_warning
	inst_unknown_model
	inst_misread 
	inst_nonesaved
	inst_nochmatch
	inst_needs_cal
	inst_cal_setup
	inst_unsupported
	inst_unexpected_reply
	inst_wrong_config
	inst_bad_parameter
 */
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
i1d3_del(inst *pp) {
	i1d3 *p = (i1d3 *)pp;
	if (p->icom != NULL)
		p->icom->del(p->icom);
	free(p);
}

/* Return the instrument capabilities */
inst_capability i1d3_capabilities(inst *pp) {
	inst_capability rv;

	rv = inst_emis_spot
	   | inst_emis_disp
	   | inst_emis_proj
	   | inst_emis_ambient
	   | inst_colorimeter
	   | inst_emis_disptype
	   | inst_ccmx
	   | inst_ccss
	     ;

	return rv;
}

/* Return the instrument capabilities 2 */
inst2_capability i1d3_capabilities2(inst *pp) {
	i1d3 *p = (i1d3 *)pp;
	inst2_capability rv = 0;

	rv |= inst2_has_sensmode;
	rv |= inst2_prog_trig;
	rv |= inst2_keyb_trig;
	rv |= inst2_has_leds;

	if (p->dtype != i1d3_munkdisp) {
		rv |= inst2_cal_crt_freq;
	}

	return rv;
}

/* This isn't used at the moment, */
/* but most of the implementation is in place. */
inst_disptypesel i1d3_disptypesel[3] = {
	{
		1,
		"rc",
		"i1d3: Refresh display",
		1
	},
	{
		2,
		"nl",
		"i1d3: Non-Refresh display [Default]",
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
static inst_code i1d3_get_opt_details(
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
		*psels = i1d3_disptypesel;
		
		return inst_ok;
	}

	return inst_unsupported;
}

/* Set device measurement mode */
inst_code i1d3_set_mode(inst *pp, inst_mode m)
{
	i1d3 *p = (i1d3 *)pp;
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

/* Get a dynamic status */
static inst_code i1d3_get_status(
inst *pp,
inst_status_type m,	/* Requested status type */
...) {				/* Status parameters */                             
	i1d3 *p = (i1d3 *)pp;

	/* Return the sensor mode */
	if (m == inst_stat_sensmode) {
		inst_code ev;
		inst_stat_smode *smode;
		va_list args;
		int pos;

		va_start(args, m);
		smode = va_arg(args, inst_stat_smode *);
		va_end(args);

		*smode = inst_stat_smode_unknown;

		if ((ev = i1d3_get_diffpos(p, &pos)) != inst_ok)
 			return ev;
	
		if (pos == 1)
			*smode = inst_stat_smode_amb;
		else
			*smode = inst_stat_smode_disp | inst_stat_smode_proj;

		return inst_ok;
	}

	return inst_unsupported;
}

/* 
 * set or reset an optional mode
 * We assume that the instrument has been initialised.
 */
static inst_code
i1d3_set_opt_mode(inst *pp, inst_opt_mode m, ...)
{
	i1d3 *p = (i1d3 *)pp;

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
			p->refmode = 1;					/* Refresh mode */
			if (p->dtype == i1d3_munkdisp) {
				p->inttime = 2.0 * p->dinttime;	/* Double integration time */
			} else {
				p->inttime = p->dinttime;		/* Normal integration time */
			}
			p->rrset = 0;					/* This is a hint we may have swapped displays */
			return inst_ok;
		} else if (ix == 0 || ix == 2) {	/* Default or 2 */
			p->refmode = 0;					/* Non-Refresh mode */
			p->inttime = p->dinttime;		/* Normal integration time */
			p->rrset = 0;					/* This is a hint we may have swapped displays */
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
		*mask = 0x1;			/* One general LEDs */
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

		if (p->led_state & 0x1)
			return i1d3_set_LEDs(p, i1d3_flash, 0.0, 100.0, 0x80);
		else
			return i1d3_set_LEDs(p, i1d3_flash, 100.0, 0.0, 0x80);
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
		double offtime, ontime;
		i1d3_ledmode mode;
		int nopulses;

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

		p->led_period = period;
		p->led_on_time_prop = on_time_prop;
		p->led_trans_time_prop = trans_time_prop;

		/* i1d3 doesn't have controllable fade time, so any = fade */
		if (trans_time_prop > 0.0) {
			mode = i1d3_fade;
			offtime = period * (1.0 - on_time_prop - trans_time_prop); 
			ontime = period * (on_time_prop + trans_time_prop); 
		} else {
			mode = i1d3_flash;
			offtime = period * (1.0 - on_time_prop); 
			ontime = period * on_time_prop; 
		}
		nopulses = 0x80;
		if (period == 0.0 || on_time_prop == 0.0) {
			mode = i1d3_flash;
			offtime = 100.0;
			ontime = 0.0;
			p->led_state = 0;
		} else {
			p->led_state = 1;
		}
		return i1d3_set_LEDs(p, mode, offtime, ontime, nopulses);

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
extern i1d3 *new_i1d3(icoms *icom, instType itype, int debug, int verb)
{
	i1d3 *p;
	if ((p = (i1d3 *)calloc(sizeof(i1d3),1)) == NULL)
		error("i1d3: malloc failed!");

	if (icom == NULL)
		p->icom = new_icoms();
	else
		p->icom = icom;

	p->debug = debug;
	p->verb = verb;

	icmSetUnity3x3(p->ccmat);	/* Set the colorimeter correction matrix to do nothing */

	p->init_coms         = i1d3_init_coms;
	p->init_inst         = i1d3_init_inst;
	p->capabilities      = i1d3_capabilities;
	p->capabilities2     = i1d3_capabilities2;
	p->get_opt_details   = i1d3_get_opt_details;
	p->set_mode          = i1d3_set_mode;
	p->set_opt_mode      = i1d3_set_opt_mode;
	p->read_sample       = i1d3_read_sample;
	p->needs_calibration = i1d3_needs_calibration;
	p->col_cor_mat       = i1d3_col_cor_mat;
	p->col_cal_spec_set  = i1d3_col_cal_spec_set;
	p->calibrate         = i1d3_calibrate;
	p->interp_error      = i1d3_interp_error;
	p->del               = i1d3_del;

	p->itype = itype;

	return p;
}



/* Combine the 2 word key and 64 byte challenge into a 64 bit response.  */
static void create_unlock_response(unsigned int *k, unsigned char *c, unsigned char *r) {
	int i;
	unsigned char sc[8], sr[16];	/* Sub-challeng and response */

	/* Only 8 bytes is used out of challenge buffer starting at */
	/* offset 35. Bytes are decoded with xor of byte 3 value. */
	for (i = 0; i < 8; i++)
		sc[i] = c[3] ^ c[35 + i];
	
	/* Combine key with 16 byte challenge to create core 16 byte response */
	{
		unsigned int ci[2];		/* challenge as 4 ints */
		unsigned int co[4];		/* product, difference of 4 ints */
		unsigned int sum;		/* Sum of all input bytes */
		unsigned char s0, s1;	/* Byte components of sum. */

		/* Shuffle bytes into 32 bit ints to be able to use 32 bit computation. */
		ci[0] = (sc[3] << 24)
              + (sc[0] << 16)
              + (sc[4] << 8)
              + (sc[6]);

		ci[1] = (sc[1] << 24)
              + (sc[7] << 16)
              + (sc[2] << 8)
              + (sc[5]);
	
		/* Computation on the ints */
		co[0] = -k[0] - ci[1];
		co[1] = -k[1] - ci[0];
		co[2] = ci[1] * -k[0];
		co[3] = ci[0] * -k[1];
	
		/* Sum of challenge bytes */
		for (sum = 0, i = 0; i < 8; i++)
			sum += sc[i];

		/* Minus the two key values as bytes */
		sum += (0xff & -k[0]) + (0xff & (-k[0] >> 8))
	        + (0xff & (-k[0] >> 16)) + (0xff & (-k[0] >> 24));
		sum += (0xff & -k[1]) + (0xff & (-k[1] >> 8))
	         + (0xff & (-k[1] >> 16)) + (0xff & (-k[1] >> 24));
	
		/* Convert sum to bytes. Only need 2, because sum of 16 bytes can't exceed 16 bits. */
		s0 =  sum       & 0xff;
		s1 = (sum >> 8) & 0xff;
	
		/* Final computation of bytes from 4 ints + sum bytes */
		sr[0] =  ((co[0] >> 16) & 0xff) + s0;
		sr[1] =  ((co[2] >>  8) & 0xff) - s1;
		sr[2] =  ( co[3]        & 0xff) + s1;
		sr[3] =  ((co[1] >> 16) & 0xff) + s0;
		sr[4] =  ((co[2] >> 16) & 0xff) - s1;
		sr[5] =  ((co[3] >> 16) & 0xff) - s0;
		sr[6] =  ((co[1] >> 24) & 0xff) - s0;
		sr[7] =  ( co[0]        & 0xff) - s1;
		sr[8] =  ((co[3] >>  8) & 0xff) + s0;
		sr[9] =  ((co[2] >> 24) & 0xff) - s1;
		sr[10] = ((co[0] >>  8) & 0xff) + s0;
		sr[11] = ((co[1] >>  8) & 0xff) - s1;
		sr[12] = ( co[1]        & 0xff) + s1;
		sr[13] = ((co[3] >> 24) & 0xff) + s1;
		sr[14] = ( co[2]        & 0xff) + s0;
		sr[15] = ((co[0] >> 24) & 0xff) - s0;
	}

	/* The OEM driver sets the resonse to random bytes, */
	/* but we don't need to do this, since the device doesn't */
	/* look at them. */
	for (i = 0; i < 64; i++)
		r[i] = 0;

	/* The actual resonse is 16 bytes at offset 24 in the response buffer. */
	/* The OEM driver xor's challeng byte 2 with response bytes 4..63, but */
	/* since the instrument doesn't look at them, we only do this to the actual */
	/* response. */
	for (i = 0; i < 16; i++)
		r[24 + i] = c[2] ^ sr[i];
}
