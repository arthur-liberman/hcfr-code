
/* 
 * Argyll Color Correction System
 *
 * GretagMacbeth Huey related functions
 *
 * Author: Graeme W. Gill
 * Date:   28/7/2011
 *
 * Copyright 2006 - 2014, Graeme W. Gill
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

/* TTBD:

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
#include "i1d3.h"

#undef PLOT_SPECTRA		/* Plot the sensor senitivity spectra */
#undef PLOT_XYZSPECTRA	/* Plot the calibrated sensor senitivity spectra */
#undef SAVE_SPECTRA		/* Save the sensor senitivity spectra to "sensors.cmf" */
#undef SAVE_XYZSPECTRA	/* Save the XYZ senitivity spectra to "sensorsxyz.cmf" (scale 1.4) */
#undef SAVE_STDXYZ		/* save 1931 2 degree to stdobsxyz.cmf */
#undef PLOT_REFRESH		/* Plot data used to determine refresh rate */
#undef PLOT_UPDELAY		/* Plot data used to determine display update delay */

#undef DEBUG_TWEAKS	/* Allow environment variable tweaks to int time etc. */
/* I1D3_MIN_REF_QUANT_TIME		in seconds. Default 0.05 */
/* I1D3_MIN_INT_TIME		    in seconds. Default 0.4 for refresh displays */

#define I1D3_MEAS_TIMEOUT 40.0      /* Longest reading timeout in seconds */ 
									/* Typically 20.0 is the maximum needed. */

#define I1D3_SAT_FREQ 100000.0		/* L2F sensor frequency limit */

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
/* Response codes:

	00	OK
	83	After pulse count measure in low light. Means ???

 */
typedef enum {
    i1d3_getinfo      = 0x0000,		/* Product name + Firmware version + Firmware Date string */
    i1d3_status       = 0x0001,		/* status number ?? */
    i1d3_prodname     = 0x0010,		/* Product name string */
    i1d3_prodtype     = 0x0011,		/* Product type number */
    i1d3_firmver      = 0x0012,		/* Firmware version string */
    i1d3_firmdate     = 0x0013,		/* Firmware date string */
    i1d3_locked       = 0x0020,		/* Get locked status */
    i1d3_freqmeas     = 0x0100,		/* Measure transition over given time */
    i1d3_periodmeas   = 0x0200,		/* Measure time between transition count */
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
		case i1d3_freqmeas:
			return "Frequency Measure";
		case i1d3_periodmeas:
			return "Period Measure";
		case i1d3_readintee:
			return "ReadInternalEEPROM";
		case i1d3_readextee:
			return "ReadExternalEEPROM";
		case i1d3_setled:
			return "SetLED";
		case i1d3_rd_sensor:
			return "ReadAnalogSensor";
		case i1d3_get_diff:
			return "GetDiffuserPosition";
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
/* This is protected by a mutex, so it is multi-thread safe. */
/* The i1d3 is set up as an HID device, which can ease the need */
/* for providing a kernel driver on MSWindows systems, */
/* but it doesn't seem to actually be used as an HID device. */
/* We allow for communicating via usbio, or an HID driver. */
static inst_code
i1d3_command(
	i1d3 *p,					/* i1d3 object */
	i1Disp3CC cc,				/* Command code */
	unsigned char *send,		/* 64 Command bytes to send */
	unsigned char *recv,		/* 64 Response bytes returned */
	double to,					/* Timeout in seconds */
	int nd						/* nz to disable debug messages */
) {
	unsigned char cmd;		/* Major command code */
	int wbytes;				/* bytes written */
	int rbytes;				/* bytes read from ep */
	int se, ua = 0, rv = inst_ok;
	int ishid = p->icom->port_type(p->icom) == icomt_hid;

	amutex_lock(p->lock);

	/* Send the command using interrupt transfer to EP 0x01 */
	send[0] = cmd = (cc >> 8) & 0xff;	/* Major command == HID report number */
	if (cmd == 0x00)
		send[1] = (cc & 0xff);	/* Minor command */

	if (!nd) {
		if (cc == i1d3_lockresp)
			a1logd(p->log, 4, "i1d3_command: Sending cmd '%s' args '%s'\n",
			                         inst_desc(cc), icoms_tohex(send, 64));
		else
			a1logd(p->log, 4, "i1d3_command: Sending cmd '%s' args '%s'\n",
			                         inst_desc(cc), icoms_tohex(send, 8));
	}

	if (p->icom->port_type(p->icom) == icomt_hid) {
		se = p->icom->hid_write(p->icom, send, 64, &wbytes, to); 
	} else {
		se = p->icom->usb_write(p->icom, NULL, 0x01, send, 64, &wbytes, to);  
	}
	if (se != 0) {
		if (!nd) a1logd(p->log, 1, "i1d3_command: Command send failed with ICOM err 0x%x\n",se);
		/* Flush any response */
		if (ishid) {
			p->icom->hid_read(p->icom, recv, 64, &rbytes, to);
		} else {
			p->icom->usb_read(p->icom, NULL, 0x81, recv, 64, &rbytes, to);
		}
		amutex_unlock(p->lock);
		return i1d3_interp_code((inst *)p, I1D3_COMS_FAIL);
	}
	rv = i1d3_interp_code((inst *)p, icoms2i1d3_err(ua, 0));
	if (!nd) a1logd(p->log, 5, "i1d3_command: ICOM err 0x%x\n",ua);

	if (rv == inst_ok && wbytes != 64) {
		if (!nd) a1logd(p->log, 1, "i1d3_command: wbytes = %d != 64\n",wbytes);
		rv = i1d3_interp_code((inst *)p, I1D3_BAD_WR_LENGTH);
	}

	if (rv != inst_ok) {
		/* Flush any response */
		if (ishid) {
			p->icom->hid_read(p->icom, recv, 64, &rbytes, to);
		} else {
			p->icom->usb_read(p->icom, NULL, 0x81, recv, 64, &rbytes, to);
		}
		amutex_unlock(p->lock);
		return rv;
	}

	/* Now fetch the response */
	if (!nd) a1logd(p->log, 5, "i1d3_command: Reading response\n");

	if (ishid) {
		se = p->icom->hid_read(p->icom, recv, 64, &rbytes, to);
	} else {
		se = p->icom->usb_read(p->icom, NULL, 0x81, recv, 64, &rbytes, to);
	} 
	if (se != 0) {
		if (!nd) a1logd(p->log, 1, "i1d3_command: response read failed with ICOM err 0x%x\n",se);
		/* Flush any extra response, in case responses are out of sync */
		if (ishid) {
			p->icom->hid_read(p->icom, recv, 64, &rbytes, 0.2);
		} else {
			p->icom->usb_read(p->icom, NULL, 0x81, recv, 64, &rbytes, 0.2);
		}
		amutex_unlock(p->lock);
		return i1d3_interp_code((inst *)p, I1D3_COMS_FAIL);
	}
	if (rv == inst_ok && rbytes != 64) {
		if (!nd) a1logd(p->log, 1, "i1d3_command: rbytes = %d != 64\n",rbytes);
		rv = i1d3_interp_code((inst *)p, I1D3_BAD_RD_LENGTH);
	}

	/* Hmm. Not sure about this bug workaround. Is this a rev B thing ? */
	/* May get status 0x83 on i1d3_periodmeas when there are no transitions ? */ 
	/* If so, ignore the error. */
	if (rv == inst_ok && cc == i1d3_periodmeas && recv[1] == 0x02 && recv[0] == 0x83) {
		int i;
		for (i = 2; i < 14; i++) {
			if (recv[i] != 0)
				break;
		}
		if (i >= 14) {		/* returned all zero's */
			if (!nd) a1logd(p->log, 1, "i1d3_command: ignoring status byte = 0x%x\n",recv[0]);
			recv[0] = 0x00;	/* Fudge OK status */
		}
	}

	/* The first byte returned seems to be a command result error code. */
	if (rv == inst_ok && recv[0] != 0x00) {
		if (!nd) a1logd(p->log, 1, "i1d3_command: status byte != 00 = 0x%x\n",recv[0]);
		rv = i1d3_interp_code((inst *)p, I1D3_BAD_RET_STAT);
	}

	/* The second byte is usually the command code being echo'd back, but not always. */
	/* ie., get i1d3_get_diff returns the status instead. */
	if (rv == inst_ok) {
		if (cc != i1d3_get_diff) {
			if (recv[1] != cmd) {
				if (!nd) a1logd(p->log, 1, "i1d3_command: major cmd not echo'd != 0x%02x = 0x%02x\n",
				                                                               cmd,recv[1]);
				rv = i1d3_interp_code((inst *)p, I1D3_BAD_RET_CMD);
			}
		} else {	/* i1d3_get_diff as special case */
			int i;
			for (i = 2; i < 64; i++) {
				if (recv[i] != 0x00) {
					if (!nd) a1logd(p->log, 1, "i1d3_command: i1d3_get_diff not zero filled\n");
					rv = i1d3_interp_code((inst *)p, I1D3_BAD_RET_CMD);
					break;
				}
			}
		}
	}

	if (!nd) {
		if (cc == i1d3_lockchal)
			a1logd(p->log, 4, "i1d3_command: got '%s' ICOM err 0x%x\n",icoms_tohex(recv, 64),ua);
		else
			a1logd(p->log, 4, "i1d3_command: got '%s' ICOM err 0x%x\n",icoms_tohex(recv, 14),ua);
	}

	if (rv != inst_ok) {
		/* Flush any extra response, in case responses are out of sync */
		if (ishid) {
			p->icom->hid_read(p->icom, recv, 64, &rbytes, 0.2);
		} else {
			p->icom->usb_read(p->icom, NULL, 0x81, recv, 64, &rbytes, 0.2);
		}
	}

	amutex_unlock(p->lock);
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

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_getinfo, todev, fromdev, 1.0, 0)) != inst_ok)
		return ev;
	
	strncpy((char *)rv, (char *)fromdev + 2, 63);

	a1logd(p->log, 3, "i1d3_get_info: got '%s'\n",rv);

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

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_status, todev, fromdev, 1.0, 0)) != inst_ok)
		return ev;
	
	*stat = 1;		/* Bad */
	if (fromdev[2] != 0 || (buf2short(fromdev + 3) >= 5))
		*stat = 0;		/* OK */

	a1logd(p->log, 3, "i1d3_check_status: got %s\n",*stat == 0 ? "OK" : "Bad");

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

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_prodname, todev, fromdev, 1.0, 0)) != inst_ok)
		return ev;
	
	strncpy((char *)rv, (char *)fromdev + 2, 31);

	a1logd(p->log, 3, "i1d3_get_prodname: got '%s'\n",rv);

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

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_prodtype, todev, fromdev, 1.0, 0)) != inst_ok)
		return ev;
	
	*stat = buf2short(fromdev + 3);

	a1logd(p->log, 3, "i1d3_get_prodtype: got 0x%x\n",*stat);

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

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_firmver, todev, fromdev, 1.0, 0)) != inst_ok)
		return ev;
	
	strncpy((char *)rv, (char *)fromdev + 2, 31);

	a1logd(p->log, 3, "i1d3_get_firmver: got '%s'\n",rv);

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

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_firmdate, todev, fromdev, 1.0, 0)) != inst_ok)
		return ev;
	
	strncpy((char *)rv, (char *)fromdev + 2, 31);

	a1logd(p->log, 3, "i1d3_get_firmdate: got '%s'\n",rv);

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

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_locked, todev, fromdev, 1.0, 0)) != inst_ok)
		return ev;
	
	*stat = 1;		/* Locked */
	if (fromdev[2] != 0 || fromdev[3] == 0)
		*stat = 0;		/* Not Locked */

	a1logd(p->log, 3, "i1d3_lock_status: got %s\n",*stat == 1 ? "Locked" : "Unlocked");

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
		i1d3_dtype btype;						/* Base type enumerator */
		i1d3_dtype stype;						/* Sub type enumerator */
	} codes[] = {
		{ "i1Display3 ",         { 0xe9622e9f, 0x8d63e133 }, i1d3_disppro,  i1d3_disppro },
		{ "Colormunki Display ", { 0xe01e6e0a, 0x257462de }, i1d3_munkdisp, i1d3_munkdisp },
		{ "i1Display3 ",         { 0xcaa62b2c, 0x30815b61 }, i1d3_disppro,  i1d3_oem },
		{ "i1Display3 ",         { 0xa9119479, 0x5b168761 }, i1d3_disppro,  i1d3_nec_ssp },
		{ "i1Display3 ",         { 0x160eb6ae, 0x14440e70 }, i1d3_disppro,  i1d3_quato_sh3 },
		{ "i1Display3 ",         { 0x291e41d7, 0x51937bdd }, i1d3_disppro,  i1d3_hp_dreamc },
		{ "i1Display3 ",         { 0xc9bfafe0, 0x02871166 }, i1d3_disppro,  i1d3_sc_c6 },
		{ "i1Display3 ",         { 0x1abfae03, 0xf25ac8e8 }, i1d3_disppro,  i1d3_wacom_dc },
		{ NULL } 
	}; 
	inst_code ev;
	int ix, nix;

	a1logd(p->log, 2, "i1d3_unlock: called\n");

	/* Count the keys */
	for (nix = 0;;nix++) {
		if (codes[nix].pname == NULL)
			break;
	}

	/* Until we give up */
	for (ix = 0;;ix++) {

		/* If we've run out of unlock keys */
		if (codes[ix].pname == NULL) {
			a1logw(p->log, "i1d3: Unknown lock code. Please contact ArgyllCMS for help\n");
			return i1d3_interp_code((inst *)p, I1D3_UNKNOWN_UNLOCK);
		}

//		return i1d3_interp_code((inst *)p, I1D3_UNLOCK_FAIL);

		/* Skip any keys that don't match the product name */
		if (strcmp(p->prod_name, codes[ix].pname) != 0) {
			continue;
		}

//		a1logd(p->log, 3, "i1d3_unlock: Trying unlock key 0x%08x 0x%08x\n",
//			codes[ix].key[0], codes[ix].key[1]);
		a1logd(p->log, 3, "i1d3_unlock: Trying unlock key %d/%d\n", ix+1, nix);

		p->btype = codes[ix].btype;
		p->stype = codes[ix].stype;

		/* Attempt unlock */
		memset(todev, 0, 64);
		memset(fromdev, 0, 64);

		/* Get a challenge */
		if ((ev = i1d3_command(p, i1d3_lockchal, todev, fromdev, 1.0, 0)) != inst_ok)
			return ev;

		/* Convert challenge to response */
		create_unlock_response(codes[ix].key, fromdev, todev);

		/* Send the response */
		if ((ev = i1d3_command(p, i1d3_lockresp, todev, fromdev, 1.0, 0)) != inst_ok)
			return ev;

		if (fromdev[2] == 0x77) {		/* Sucess */
			break;
		}

		a1logd(p->log, 3, "i1d3_unlock: Trying next unlock key\n");
		/* Try the next key */
	}

	return inst_ok;
}

/* Get the ambient diffuser position */
static inst_code
i1d3_get_diffpos(
	i1d3 *p,				/* Object */
	int *pos,				/* 0 = display, 1 = ambient */
	int nd					/* nz = no debug message */
) {
	unsigned char todev[64];
	unsigned char fromdev[64];
	inst_code ev;

	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	if ((ev = i1d3_command(p, i1d3_get_diff, todev, fromdev, 1.0, nd)) != inst_ok)
		return ev;
	
	*pos = fromdev[1];

	if (nd == 0)
		a1logd(p->log, 3, "i1d3_get_diffpos: got %d\n",*pos);

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
	
		if ((ev = i1d3_command(p, i1d3_readintee, todev, fromdev, 1.0, 0)) != inst_ok) {
			return ev;
		}
	
		memmove(bytes, fromdev + 4, ll);
	}

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
	int sdebug;

	if (addr < 0 || addr > 8191)
		return i1d3_interp_code((inst *)p, I1D3_BAD_MEM_ADDRESS);

	if (len < 0 || (addr + len) > 8192)
		return i1d3_interp_code((inst *)p, I1D3_BAD_MEM_LENGTH);
		
	memset(todev, 0, 64);
	memset(fromdev, 0, 64);

	/* Bread read up into 59 bytes packets */
	sdebug = p->log->debug;
	p->log->debug = p->log->debug >= 2 ? p->log->debug - 2 : 0;		/* Supress command traces */ 
	for (; len > 0; addr += ll, bytes += ll, len -= ll) {
		ll = len;
		if (ll > 59)
			ll = 59;

		/* OEM driver retries several times after a 10msec sleep on failure. */
		/* Can a failure actually happen though ? */
		short2bufBE(todev + 1, addr);
		todev[3] = (unsigned char)ll;
	
		if ((ev = i1d3_command(p, i1d3_readextee, todev, fromdev, 1.0, 0)) != inst_ok) {
			p->log->debug = sdebug;
			return ev;
		}
	
		memmove(bytes, fromdev + 5, ll);
	}
	p->log->debug = sdebug;

	return inst_ok;
}


/* Take a raw measurement using a given integration time. */
/* The measurent is the count of (both) edges from the L2V */
/* over the integration time. */
static inst_code
i1d3_freq_measure(
	i1d3 *p,				/* Object */
	double *inttime,		/* Integration time in seconds. (Return clock rounded) */
	double rgb[3]			/* Return the RGB count values */
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
	
	if ((ev = i1d3_command(p, i1d3_freqmeas, todev, fromdev, I1D3_MEAS_TIMEOUT, 0)) != inst_ok)
		return ev;
	
	rgb[0] = (double)buf2uint(fromdev + 2);
	rgb[1] = (double)buf2uint(fromdev + 6);
	rgb[2] = (double)buf2uint(fromdev + 10);

	/* The HW holds the L2F *OE high (disabled) until the start of the measurement period, */
	/* and this has the effect of holding the internal integrator in a reset state. */
	/* This then synchronizes the frequency output to the start of */
	/* the measurement, which has the effect of rounding down the count output. */
	/* To compensate, we have to add 0.5 to the count. */
	rgb[0] += 0.5;
	rgb[1] += 0.5;
	rgb[2] += 0.5;

	return inst_ok;
}

/* Take a raw measurement that returns the number of clocks */
/* between and initial edge at the start of the period (triggered by */
/* the *OE going low and the integrator being started) and edgec[] */
/* subsequent edges of the L2F. The edge count must be between */
/* 1 and 65535 inclusive. Both edges are counted. It's advisable */
/* to use an even edgec[], because the L2F output may not be symetric. */
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
	
	if ((ev = i1d3_command(p, i1d3_periodmeas, todev, fromdev, I1D3_MEAS_TIMEOUT, 0)) != inst_ok)
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

	if ((ev = i1d3_command(p, i1d3_setled, todev, fromdev, 1.0, 0)) != inst_ok)
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
	Pick the longest peak between 10 and 40Hz as the best sample period,
	and halve this to use as the quantization value (ie. make
	it lie between 20 and 80 Hz).

	If there was no error, return refresh quanization period it.

	If there is no aparent refresh, or the refresh rate is not determinable,
	return a period of 0.0 and inst_ok;

	To break up the USB synchronization, the integration time
	is randomized slightly.

	NOTE :- we should really switch to using period measurement mode here,
    since it is more accurate ?
*/

#ifndef PSRAND32L 
# define PSRAND32L(S) ((S) * 1664525L + 1013904223L)
#endif
#undef FREQ_SLOW_PRECISE	/* [und] Interpolate then autocorrelate, else autc & filter */
#define NFSAMPS 1300		/* Number of samples to read */
#define NFMXTIME 6.0		/* Maximum time to take (2000 == 6) */
#define PBPMS 20			/* bins per msec */
#define PERMIN ((1000 * PBPMS)/40)	/* 40 Hz */
#define PERMAX ((1000 * PBPMS)/5)	/* 5 Hz*/
#define NPER (PERMAX - PERMIN + 1)
//#define PWIDTH (3 * PBPMS)			/* 3 msec bin spread to look for peak in */
#define PWIDTH (8 * PBPMS)			/* 3 msec bin spread to look for peak in */
#define MAXPKS 20					/* Number of peaks to find */

/* Set refperiod, refrate if possible */
static inst_code
i1d3_imp_measure_refresh(
	i1d3 *p,			/* Object */
	double *prefrate,	/* Return value, 0.0 if none */	
	double *ppval		/* Return period value, 0.0 if none */	
) {
	inst_code ev;
	int i, j, k;
	double ucalf = 1.0;				/* usec_time calibration factor */
	double inttimel = 0.0003;
	double inttimeh = 0.0040;
	double sutime, putime, cutime, eutime;
	static unsigned int randn = 0x12345678;
	struct {
		double itime;	/* Integration time */
		double sec;
		double rgb[3];
	} samp[NFSAMPS];
	int nfsamps;			/* Actual samples read */
	double maxt;			/* Time range */
	double rms[3];			/* RMS value of each channel */
	double trms;			/* Total RMS */
	int nbins;
	double *bins[3];		/* PBPMS sample bins */
	double tcorr[NPER];		/* Temp for initial autocorrelation */
	double corr[NPER];		/* Filtered correlation for each period value */
	double mincv, maxcv;	/* Max and min correlation values */
	double crange;			/* Correlation range */
	double peaks[MAXPKS];	/* Each peak from longest to shortest */
	int npeaks = 0;			/* Number of peaks */
	double pval;			/* Period value */
	int isdeb;
	int isth;

	if (prefrate != NULL)
		*prefrate = 0.0;
	if (ppval != NULL)
		*ppval = 0.0;

	if (usec_time() < 0.0) {
		a1loge(p->log, inst_internal_error, "i1d3_measure_refresh: No high resolution timers\n");
		return inst_internal_error; 
	}

	/* Turn debug and thread off so that they doesn't intefere with measurement timing */
	isdeb = p->log->debug;
	p->icom->log->debug = 0;
	isth = p->th_en;
	p->th_en = 0;

	/* Do some measurement and throw them away, to make sure the code is in cache. */
	for (i = 0; i < 5; i++) {
		if ((ev = i1d3_freq_measure(p, &inttimeh, samp[i].rgb)) != inst_ok) {
			p->log->debug = isdeb;
			p->th_en = isth;
	 		return ev;
		}
	}

#ifdef NEVER		/* This appears to be unnecessary */
	/* Calibrate the usec timer against the instrument */
	{
		double inttime1, inttime2;
		inttime1 = 0.001; 
		inttime2 = 0.501; 

		sutime = usec_time();

		if ((ev = i1d3_freq_measure(p, &inttime1, samp[0].rgb)) != inst_ok) {
			p->log->debug = isdeb;
			p->th_en = isth;
	 		return ev;
		}

		putime = usec_time();

		if ((ev = i1d3_freq_measure(p, &inttime2, samp[0].rgb)) != inst_ok) {
			p->log->debug = isdeb;
			p->th_en = isth;
	 		return ev;
		}

		cutime = usec_time();

		ucalf = 1000000.0 * (inttime2 - inttime1)/(cutime - 2.0 * putime + sutime);

		a1logd(p->log, 3, "i1d3_measure_refresh: Clock calibration factor = %f\n",ucalf);
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
		rval *= rval;		/* Sharpen random time up */
		samp[i].itime = (inttimeh - inttimel) * rval + inttimel;

		if ((ev = i1d3_freq_measure(p, &samp[i].itime, samp[i].rgb)) != inst_ok) {
			p->log->debug = isdeb;
			p->th_en = isth;
 			return ev;
		}
		cutime = (usec_time() - sutime) / 1000000.0;
		samp[i].sec = 0.5 * (putime + cutime);	/* Mean of before and after stamp */
//samp[i].sec *= 85.0/20.0;		/* Test 20 Hz */
//samp[i].sec *= 85.0/100.0;		/* Test 100 Hz */
		putime = cutime;
		if (cutime > NFMXTIME)
			break; 
	}
	/* Restore debug & thread */
	p->log->debug = isdeb;
	p->th_en = isth;

	nfsamps = i;
	if (nfsamps < 100) {
		a1logv(p->log, 1, "No distict refresh period\n");
		a1logd(p->log, 3, "i1d3_measure_refresh: Couldn't find a distinct refresh frequency\n");
		return inst_ok;
	}
 
	a1logd(p->log, 3, "i1d3_measure_refresh: Read %d samples for refresh calibration\n",nfsamps);

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
	rms[0] = rms[1] = rms[2] = 0.0;
	for (i = nfsamps-1; i >= 0; i--) {
		samp[i].sec -= samp[0].sec; 
		samp[i].sec *= ucalf;
		if (samp[i].sec > maxt)
			maxt = samp[i].sec;
		for (j = 0; j < 3; j++) {
			samp[i].rgb[j] /= samp[i].itime;
			rms[j] += samp[i].rgb[j] * samp[i].rgb[j];
		}
	}
	trms = 0.0;
	for (j = 0; j < 3; j++) {
		rms[j] /= (double)nfsamps;
		trms += rms[j];
		rms[j] = sqrt(rms[j]);
	}
	trms = sqrt(trms);
	a1logd(p->log, 4, "RMS = %f %f %f, total %f\n", rms[0], rms[1], rms[2], trms);

#ifdef FREQ_SLOW_PRECISE	/* Interp then autocorrelate */

	/* Create PBPMS bins and interpolate readings into them */
	nbins = 1 + (int)(maxt * 1000.0 * PBPMS + 0.5);
	for (j = 0; j < 3; j++) {
		if ((bins[j] = (double *)calloc(sizeof(double), nbins)) == NULL) {
			a1loge(p->log, inst_internal_error, "i1d3_measure_refresh: malloc failed\n");
			return inst_internal_error;
		}
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

#ifdef NEVER
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

		if (xx == NULL || y1 == NULL || y2 == NULL || y3 == NULL) {
			a1loge(p->log, inst_internal_error, "i1d3_measure_refresh: malloc failed\n");
			for (j = 0; j < 3; j++)
				free(bins[j]);
			return inst_internal_error;
		}
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
	/* from 25 msec (40Hz) to 100msec (10 Hz) */
	mincv = 1e48, maxcv = -1e48;
	for (i = 0; i < NPER; i++) {
		int poff = PERMIN + i;		/* Offset to corresponding sample */
		corr[i] = 0.0;

		for (k = 0; (k + poff) < nbins; k++)
			corr[i] += bins[0][k] * bins[0][k + poff];
		for (k = 0; (k + poff) < nbins; k++)
			corr[i] += bins[1][k] * bins[1][k + poff];
		for (k = 0; (k + poff) < nbins; k++)
			corr[i] += bins[2][k] * bins[2][k + poff];

		corr[i] /= (double)k;		/* Normalize */
		if (corr[i] > maxcv)
			maxcv = corr[i];
		if (corr[i] < mincv)
			mincv = corr[i];
	}
	for (j = 0; j < 3; j++)
		free(bins[j]);

#else /* !FREQ_SLOW_PRECISE  Fast - autocorrellate then filter */

	/* Do point by point correllation of samples */
	for (i = 0; i < NPER; i++)
		tcorr[i] = 0.0;

	for (j = 0; j < (nfsamps-1); j++) {

		for (k = j+1; k < nfsamps; k++) {
			double del, cor;
			int bix;

			del = samp[k].sec - samp[j].sec;
			bix = (int)(del * 1000.0 * PBPMS + 0.5);
			if (bix < PERMIN)
				continue;
			if (bix > PERMAX)
				break;
			bix -= PERMIN;
		
//			cor = samp[j].rgb[0] * samp[k].rgb[0]
//	            + samp[j].rgb[1] * samp[k].rgb[1]
//	            + samp[j].rgb[2] * samp[k].rgb[2];

			cor = samp[j].rgb[1] * samp[k].rgb[1];

			tcorr[bix] += cor;
		} 
	}

#ifdef PLOT_REFRESH
	/* Plot unfiltered auto correlation */
	{
		double xx[NPER];
		double y1[NPER];

		for (i = 0; i < NPER; i++) {
			xx[i] = (i + PERMIN) / (double)PBPMS;			/* msec */
			y1[i] = tcorr[i];
		}
		printf("Unfiltered auto correlation (msec)\n");
		do_plot6(xx, y1, NULL, NULL, NULL, NULL, NULL, NPER);
	}
#endif /* PLOT_REFRESH */

	/* Apply a 5 msec gausian filter */
#define FWIDTH 6
	{
		double gaus_[2 * FWIDTH * PBPMS + 1];
		double *gaus = &gaus_[FWIDTH * PBPMS];
		double bb = 1.0/pow(2, 5.0);
	
		for (j = (-FWIDTH * PBPMS); j <= (FWIDTH * PBPMS); j++) {
			double tt;
			tt = (double)j/(FWIDTH * PBPMS);
			gaus[j] = 1.0/pow(2, 5.0 * tt * tt) - bb;
		}
		for (k = 0; k < 1; k++) {
			for (i = 0; i < NPER; i++) {
				double sum = 0.0;
				double wght = 0.0;
				
				for (j = (-FWIDTH * PBPMS); j <= (FWIDTH * PBPMS); j++) {
					double w;
					int ix = i + j;
					if (ix < 0)
						continue;
					if (ix > (NPER-1))
						break;
					w = gaus[j];
					sum += w * tcorr[ix];
					wght += w;
				}
				corr[i] = sum / wght;
			}
		}
	}

	/* Compute min & max */
	mincv = 1e48, maxcv = -1e48;
	for (i = 0; i < NPER; i++) {
		if (corr[i] > maxcv)
			maxcv = corr[i];
		if (corr[i] < mincv)
			mincv = corr[i];
	}

#endif /* !FREQ_SLOW_PRECISE  Fast - autocorrellate then filter */

	crange = maxcv - mincv;
	a1logd(p->log,4,"Correlation value range %f - %f = %f = %f%%\n",mincv, maxcv,crange, 100.0 * (maxcv-mincv)/maxcv);

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

	/* If there is sufficient level and distict correlations */
	if (trms >= 1000 && (maxcv-mincv)/maxcv >= 0.10) {

		/* Locate all the peaks starting at the longest correllation */
		for (i = (NPER-1-PWIDTH); i >= 0 && npeaks < MAXPKS; i--) {
			double v1, v2, v3;
			v1 = corr[i];
			v2 = corr[i + PWIDTH/2];
			v3 = corr[i + PWIDTH];

			if (fabs(v3 - v1) < (0.05 * crange)
			 && (v2 - v1) > (0.025 * crange)
			 && (v2 - v3) > (0.025 * crange)) {
				double pkv;			/* Peak value */
				int pki;			/* Peak index */
				double ii, bl;

				a1logd(p->log,4,"Max between %f and %f msec\n",
				       (i + PERMIN)/(double)PBPMS,(i + PWIDTH + PERMIN)/(double)PBPMS);

				/* Locate the actual peak */
				pkv = -1.0;
				pki = 0;
				for (j = i; j < (i + PWIDTH); j++) {
					if (corr[j] > pkv) {
						pkv = corr[j];
						pki = j;
					}
				}
				a1logd(p->log,4,"Peak is at %f msec, %f corr\n", (pki + PERMIN)/(double)PBPMS, pkv);

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

				a1logd(p->log,4,"Interpolated peak is at %f msec\n", pval);

				peaks[npeaks++] = pval;

				i -= PWIDTH;
			}
		}
	}

	a1logd(p->log,3,"Number of peaks located = %d\n",npeaks);
	if (npeaks == 0) {
		a1logd(p->log, 2, "i1d3: Couldn't find a distinct refresh frequency\n");
		a1logv(p->log, 1, "No distict refresh period\n");
		return inst_ok;
	}

	if (npeaks == 1) {
		a1logd(p->log,3,"Only one peak\n");
		pval = peaks[0] / 2000.0;	/* Scale by half and convert to seconds */

		a1logd(p->log, 1, "Quantizing to %f msec\n",pval);
		a1logv(p->log, 1, "Quantizing to %f msec\n",pval);

		if (ppval != NULL)
			*ppval = pval;

	} else {
		int nfails;
		double div, avg, ano;
		/* Try and locate a common divisor amongst all the peaks. */
		/* This is likely to be the underlying refresh rate. */
		for (k = 0; k < npeaks; k++) {
			for (j = 1; j < 20; j++) {
				avg = ano = 0.0;
				div = peaks[k]/(double)j;
				if (div < 9.0)
					continue;		/* Skip anything over 100Hz */
				for (nfails = i = 0; i < npeaks; i++) {
					double rem, cnt;

					rem = peaks[i]/div;
					cnt = floor(rem + 0.5);
					rem = fabs(rem - cnt);

					a1logd(p->log, 1, "remainder for peak %d = %f\n",i,rem);
					if (rem > 0.06) {
						if (++nfails > 2)
							break;				/* Fail this divisor */
					}
					avg += peaks[i];		/* Already weighted by cnt */
					ano += cnt;
				}

//				if (i >= npeaks)
				if (nfails == 0 || (nfails <= 2 && npeaks >= 6))
					break;		/* Sucess */
				/* else go and try a different divisor */
			}
			if (j < 20)
				break;			/* Found common divisor */
		}
		if (k >= npeaks) {
			a1logd(p->log,3,"Failed to locate common divisor\n");
			pval = peaks[0] / 2000.0;	/* Scale by half and convert to seconds */

			if (ppval != NULL)
				*ppval = pval;


			a1logd(p->log, 1, "Quantizing to %f msec\n",pval);
			a1logv(p->log, 1, "Quantizing to %f msec\n",pval);

		} else {
			int mul;
			double refrate;
			double pval;

			pval = avg/ano;
			pval /= 1000.0;		/* Convert to seconds */
			refrate = 1.0/pval;

			if (prefrate != NULL)
				*prefrate = refrate;	/* Save it for get_refr_rate() */
			
			/* Error against my 85Hz CRT - GWG */
//			a1logd(p->log, 1, "Refresh rate error = %.4f%%\n",100.0 * fabs(refrate - 85.0)/(85.0));

			/* Scale to just above 20 Hz, but make it multiple of 2 or 4 */
#ifdef DEBUG_TWEAKS
			{
				double quanttime = 1.0/20.0;
				char *cp;
				if ((cp = getenv("I1D3_MIN_REF_QUANT_TIME")) != NULL)
					quanttime = atof(cp);
				mul = (int)floor(quanttime / pval);
			}
#else
			mul = (int)floor((1.0/20.0) / pval);
#endif
			if (mul > 1) {
				if (mul >= 8)
					mul = (mul + 3) & ~3;	/* Round up to mult of 4 */
				else
					mul = (mul + 1) & ~1;	/* Round up to mult of 2 */
				pval *= mul;
			}

			a1logd(p->log, 1, "Refresh rate = %f Hz, quantizing to %f msec\n",refrate,pval);
			a1logv(p->log, 1, "Refresh rate = %f Hz, quantizing to %f msec\n",refrate,pval);

			if (ppval != NULL)
				*ppval = pval;
		}
	}

	return inst_ok;
}
#undef NFSAMPS 
#undef PBPMS
#undef PERMIN
#undef PERMAX
#undef NPER
#undef PWIDTH

/* Measure and then set refperiod, refrate if possible */
static inst_code
i1d3_measure_set_refresh(
	i1d3 *p			/* Object */
) {
	inst_code rv;
	double refrate = 0.0;
	int mul;
	double pval;

	if ((rv = i1d3_imp_measure_refresh(p, &refrate, &pval)) != inst_ok) {
		return rv;
	}

	p->refrate = refrate;
	p->refrvalid = refrate != 0.0 ? 1 : 0;
	p->refperiod = pval;
	p->rrset = 1;

	return inst_ok;
}

/* - - - - - - - - - - - - - - - - - - - - - - */

#ifdef NEVER	/* No longer used - use general measurement instead */

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

	a1logd(p->log,3,"take_amb_measurement called\n");

	/* Check that the ambient filter is in place */
	if ((ev = i1d3_get_diffpos(p, &pos, 0)) != inst_ok)
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

	a1logd(p->log,3,"take_amb_measurement returned %f %f %f\n",rgb[0],rgb[1],rgb[2]);

	return inst_ok;
}

#endif

/* - - - - - - - - - - - - - - - - - - - - - - */

/* Take a general measurement and return the cooked reading */
/* The cooked reading is the frequency of the L2V */
static inst_code
i1d3_take_emis_measurement(
	i1d3 *p,			/* Object */
	i1d3_mmode mode,	/* Measurement mode */
	double *rgb			/* Return the cooked emsissive RGB values */
) {
	int i, k;
	inst_code ev;
	double rmeas[3] = { -1.0, -1.0, -1.0 };	/* raw measurement */
	int edgec[3] = {2,2,2};	/* Measurement edge count for each channel (not counting start edge) */
	int mask = 0x7;			/* Period measure mask */
	int msecstart = msec_time();	/* Debug */
	double rgb2[3] = { 0.0, 0.0, 0.0 };  /* Trial measurement RGB values */
	int isth;

	if (p->inited == 0)
		return i1d3_interp_code((inst *)p, I1D3_NOT_INITED);

	a1logd(p->log,3,"\ntake_emis_measurement called\n");

	/* Suspend thread so that it doesn't intefere with measurement timing */
	isth = p->th_en;
	p->th_en = 0;

	/* If we should take a frequency measurement first, */
	/* since it is done in a predictable duration */
	if (mode == i1d3_adaptive || mode == i1d3_frequency) {

		/* Typically this is 200msec for non-refresh, 400msec for refresh. */
		a1logd(p->log,3,"Doing fixed period frequency measurement over %f secs\n",p->inttime);

		/* Take a frequency measurement over a fixed period */
		if ((ev = i1d3_freq_measure(p, &p->inttime, rmeas)) != inst_ok) {
			p->th_en = isth;
 			return ev;
		}

		/* Convert to frequency (assume raw meas is both edges count over integration time) */
		for (i = 0; i < 3; i++) {
			rgb[i] = (0.5 * rmeas[i])/p->inttime;
		}

		a1logd(p->log,3,"Got %f %f %f raw, %f %f %f Hz\n",rmeas[0],rmeas[1],rmeas[2],rgb[0],rgb[1],rgb[2]);

		for (i = 0; i < 3; i++) {
			if (rgb[i] > I1D3_SAT_FREQ)
				return i1d3_interp_code((inst *)p, I1D3_TOOBRIGHT);
		}
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
					a1logd(p->log,3,"chan %d needs re-reading\n",i);
					mask |= 1 << i;			
				} else {
					a1logd(p->log,3,"chan %d has sufficient frequeny count\n",i);
				}
			}
		}

		if (mask != 0x0) {	/* Some measurement wasn't accurate enough, so use period */
							/* or longer frequency measurement */
			int mask2 = mask;
			double tintt[3];		/* Per channel re-measure target int. times */
			double tinttime;		/* Maximum re-measure target integration time */

			/* See if we need to do some pre-measurement to compute how many */
			/* edges to count. */
			for (i = 0; i < 3; i++) {
				if ((mask & (1 << i)) == 0)
					continue;
				
				if (rmeas[i] < 10.0
				 || (p->btype != i1d3_munkdisp && rmeas[i] < 20.0)) {
					a1logd(p->log,3,"chan %d needs pre-measurement\n",i);
					mask2 |= 1 << i;			
				} else {
					double freq;
					mask2 &= ~(1 << i);			
					/* Convert rmeas[i] from frequency to period equivalent */
					/* for subsequent calculations */
					freq = (rmeas[i] * 0.5)/p->inttime;
					rmeas[i] = (0.5 * edgec[i] * p->clk_freq)/freq; 
					a1logd(p->log,3,"chan %d has sufficient frequeny count to avoid pre-measure (rmeas_p %f)\n",i,rmeas[i]);
				}
			}
			if (mask2 != 0x0) {
				int mask3 = 0x0;
				double rmeas2[3];

				a1logd(p->log,3,"Doing 1st period pre-measurement mask 0x%x, edgec %d %d %d\n",mask2,edgec[0],edgec[1],edgec[2]);
				/* Take an initial period pre-measurement over 2 edges */
				if ((ev = i1d3_period_measure(p, edgec, mask2, rmeas2)) != inst_ok) {
					p->th_en = isth;
		 			return ev;
				}

				a1logd(p->log,3,"Got %f %f %f raw %f %f %f Hz\n",rmeas2[0],rmeas2[1],rmeas2[2],
				     0.5 * edgec[0] * p->clk_freq/rmeas2[0],
				     0.5 * edgec[1] * p->clk_freq/rmeas2[1],
				     0.5 * edgec[2] * p->clk_freq/rmeas2[2]);

				/* Transfer updated counts from 1st initial measurement */
				for (i = 0; i < 3; i++) {
					if ((mask2 & (1 << i)) != 0) {
						rmeas[i] = rmeas2[i];

						/* Compute trial RGB in case we need it later */
						if (rmeas[i] >= 0.5) {
							rgb2[i] = (p->clk_freq * 0.5 * edgec[i])/rmeas[i];
						}
					}
				}

				/* Do 2nd initial measurement if the count is small, in case */
				/* we are measuring a CRT with a refresh rate which adds innacuracy, */
				/* and could result in a unecessarily long re-reading. */
				/* Don't do this for Munki Display, because of its slow measurements. */
				if (p->btype != i1d3_munkdisp) {
					for (i = 0; i < 3; i++) {
						if ((mask2 & (1 << i)) == 0)
							continue;

						if (rmeas2[i] > 0.5) {
							double pintt, nedgec;
							int inedgec;

							/* Compute number of edges needed for a clock count */
							/* of 0.100 seconds */

							pintt = 0.1;

							if (p->refperiod > 0.0) {		/* If we have a refresh period */
								int n;
								n = (int)ceil(pintt/p->refperiod);	/* Quantize */
								pintt = n * p->refperiod;
							}

							nedgec = edgec[i] * pintt * p->clk_freq/rmeas2[i];

							a1logd(p->log,3,"chan %d target edges %f\n",i,nedgec);

							/* Limit to a legal range */
							if (nedgec > 65534.0)
								nedgec = 65534.0;
							else if (nedgec < 2.0)
								nedgec = 2.0;

							/* Round down to nearest even edge count */
							inedgec = (int)(2.0 * floor(nedgec/2.0));

							a1logd(p->log,3,"chan %d set edgec to %d\n",i,inedgec);

							/* Don't do 2nd initial measure if we have fewer number of edges */
							if (inedgec > edgec[i]) {
								mask3 |= (1 << i);
								edgec[i] = (int)inedgec;
							}
						} else {
							a1logd(p->log,3,"chan %d had no reading, so skipping period measurement\n",i);
						}
					}
					if (mask3 != 0x0) {

						a1logd(p->log,3,"Doing 2nd initial period measurement mask 0x%x, edgec %d %d %d\n",mask2,edgec[0],edgec[1],edgec[2]);
						/* Take a 2nd initial period  measurement */
						if ((ev = i1d3_period_measure(p, edgec, mask3, rmeas2)) != inst_ok) {
							p->th_en = isth;
				 			return ev;
						}

						a1logd(p->log,3,"Got %f %f %f raw %f %f %f Hz\n",rmeas2[0],rmeas2[1],rmeas2[2],
						     0.5 * edgec[0] * p->clk_freq/rmeas2[0],
						     0.5 * edgec[1] * p->clk_freq/rmeas2[1],
						     0.5 * edgec[2] * p->clk_freq/rmeas2[2]);

						/* Transfer updated counts from 2nd initial measurement */
						for (i = 0; i < 3; i++) {
							if ((mask3 & (1 << i)) != 0)
								rmeas[i]  = rmeas2[i];

							/* Compute trial RGB in case we need it later */
							if (rmeas[i] >= 0.5) {
								rgb2[i] = (p->clk_freq * 0.5 * edgec[i])/rmeas[i];
							}
						}
					}
				}
			}

			/* Now setup for re-measure, aiming for longer freq/full period measurement. */
			/* Compute a target integration time for this re-measurement */
			tinttime = tintt[0] = tintt[1] = tintt[2] = p->inttime;
			for (i = 0; i < 3; i++) {
				double nedgec;

				if ((mask & (1 << i)) == 0 || rmeas[i] <= 0.5)
					continue;

				/* Compute number of edges needed for a clock count */
				/* of p->inttime (0.2/0.4 secs) */
				nedgec = edgec[i] * p->inttime * p->clk_freq/rmeas[i];

				/* If we will get less than 200 edges, raise the target integration */
				/* time in a curve to aim at a higher edge count up to 200 */
				if (nedgec < 200.0) {
					double bl, tedges;
					double mint;

					/* Blend down from target of 200 to minimum target of 1 edge over 8 sec. */
					/* (Allow margine away from max integration time of 6 secs) */
					mint = p->inttime/6.0;
					bl = (nedgec - mint)/(200.0 - mint);
					if (bl < 0.0)
						bl = 0.0;
					else {
						/* This power sets how fast the int. time rises */
						bl = pow(bl, 0.5);		/* Use longer int. times to increase ecount */
					}
					tedges = bl * (200.0 - mint) + mint;

					tintt[i] = tedges/(edgec[i] * p->clk_freq/rmeas[i]);

					if (tintt[i] > 6.0)		/* Maximum possible is 6 seconds */
						tintt[i] = 6.0;		/* to ensure it completes within the 20 timeout */

					if (p->refperiod > 0.0) {		/* If we have a refresh period */
						int n;
						n = (int)ceil(tintt[i]/p->refperiod);	/* Quantize */
						tintt[i] = n * p->refperiod;
					}
					if (tintt[i] > tinttime)	/* New overal max. int. time */
						tinttime = tintt[i];
				}
			}
			a1logd(p->log,3,"target re-measure inttime %f\n",tinttime);

			/* Now compute the number of edges to measure */
			for (i = 0; i < 3; i++) {
				if ((mask & (1 << i)) == 0)
					continue;

				if (rmeas[i] > 0.5) {
					double nedgec, onedgec, atintt;

					/* Compute number of edges needed for a clock count */
					/* of tintt[i], the individual channels goal */
					nedgec = edgec[i] * tintt[i] * p->clk_freq/rmeas[i];

					/* Limit to a legal range */
					if (nedgec > 65534.0)
						nedgec = 65534.0;
					else if (nedgec < 2.0)
						nedgec = 2.0;

					/* Round down to the nearest even edge count */
					nedgec = 2.0 * (int)floor(nedgec/2.0);

					/* Compute number of edges needed for the overall goal */
					/* of clock count of tinttime */
					onedgec = edgec[i] * tinttime * p->clk_freq/rmeas[i];

					/* Limit to a legal range */
					if (onedgec > 65534.0)
						onedgec = 65534.0;
					else if (onedgec < 2.0)
						onedgec = 2.0;

					/* Round down to nearest even edge count */
					onedgec = 2.0 * (int)floor(onedgec/2.0);

					/* Use this overall edge count goal, as long as */
					/* it doesn't excessively increase the overall integration time */
					atintt = onedgec * rmeas[i]/(edgec[i] * p->clk_freq);

					if (atintt < (1.1 * tinttime))
						nedgec = onedgec;

					a1logd(p->log,3,"chan %d set edgec to %d\n",i,(int)nedgec);

					/* Don't measure again if we have same number of edges as last time */
					if (edgec[i] == (int)nedgec) {

						/* Use previous measurement */
						rgb[i] = (p->clk_freq * 0.5 * edgec[i])/rmeas[i];
						mask &= ~(1 << i);
						edgec[i] = 0;

						a1logd(p->log,3,"chan %d skipping re-measure, frequency %f\n",i,rgb[i]);

					} else {
						edgec[i] = (int)nedgec;
					}
				} else {
					/* Don't measure again, we failed to see any edges */
					rgb[i] = 0.0;
					mask &= ~(1 << i);
					edgec[i] = 0;
				}
			}

			if (mask != 0x0) {
				int minedgec = 1000;

				for (i = 0; i < 3; i++) {
					if ((mask & (1 << i)) == 0)
						continue;
					if (edgec[i] < minedgec)
						minedgec = edgec[i];
				}
				a1logd(p->log,3,"Minedgec = %d\n",minedgec);

				/* Use frequency measurement over the fixed period if refresh display */
				/* This compromises quantization error for improved stability */
				if (p->refperiod > 0.0		/* If we have a refresh period */
				 && minedgec >= 100) {		/* and the expected edge count is sufficient */
					int n;

					a1logd(p->log,3,"Doing freq re-measure inttime %f\n",tinttime);

					/* Take a frequency measurement over a fixed period */
					if ((ev = i1d3_freq_measure(p, &tinttime, rmeas)) != inst_ok) {
						p->th_en = isth;
			 			return ev;
					}

					/* Convert raw measurement to frequency */
					for (i = 0; i < 3; i++) {
						rgb[i] = (0.5 * rmeas[i])/tinttime;
					}

					a1logd(p->log,3,"Got %f %f %f raw, %f %f %f Hz after re-measure\n",rmeas[0],rmeas[1],rmeas[2],rgb[0],rgb[1],rgb[2]);

					for (i = 0; i < 3; i++) {
						if (rgb[i] > I1D3_SAT_FREQ)
							return i1d3_interp_code((inst *)p, I1D3_TOOBRIGHT);
					}

				} else {
					/* Use period measurement of the target number of edges */
					/* (Note that if the patch isn't constant and drops compared to */
					/* the trial measurement used to set the target number of edges, */
					/* that the measurement may time out and return 0. In this case */
					/* we fall back on the trial value rather than return 0.) */

					a1logd(p->log,3,"Doing period re-measure mask 0x%x, edgec %d %d %d\n",mask,edgec[0],edgec[1],edgec[2]);
					/* Measure again with desired precision, taking up to 0.4/0.8 secs */
					if ((ev = i1d3_period_measure(p, edgec, mask, rmeas)) != inst_ok) {
						p->th_en = isth;
			 			return ev;
					}
		
					for (i = 0; i < 3; i++) {
						double tt;
						if ((mask & (1 << i)) == 0)
							continue;
		
						/* Compute the frequency from period measurement */
						if (rmeas[i] < 0.5)		/* Number of edges wasn't counted */
							rgb[i] = rgb2[i];	/* Trial value, since it may be more realistic */
						else
							rgb[i] = (p->clk_freq * 0.5 * edgec[i])/rmeas[i];
						a1logd(p->log,3,"chan %d raw %f frequency %f (%f Sec)\n",i,rmeas[i],rgb[i],
						                            rmeas[i]/p->clk_freq);
					}
					a1logd(p->log,3,"Got %f %f %f Hz after period measure\n",rgb[0],rgb[1],rgb[2]);
				}
			}
		}
	}
	p->th_en = isth;

	a1logd(p->log,3,"Took %d msec to measure\n", msec_time() - msecstart);

	/* Subtract black level */
	for (i = 0; i < 3; i++) {
		rgb[i] -= p->black[i];
		if (rgb[i] < 0.0)
			rgb[i] = 0.0;
	}

	a1logd(p->log,3,"Cooked RGB = %f %f %f\n",rgb[0],rgb[1],rgb[2]);

	for (i = 0; i < 3; i++) {
		if (rgb[i] > I1D3_SAT_FREQ)
			return i1d3_interp_code((inst *)p, I1D3_TOOBRIGHT);
	}
	
	return inst_ok;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Take a XYZ measurement from the device */
static inst_code
i1d3_take_XYZ_measurement(
	i1d3 *p,				/* Object */
	double XYZ[3]			/* Return the XYZ values */
) {
	int pos;
	inst_code ev;

	i1d3_mmode mmode = i1d3_adaptive;

	if (IMODETST(p->mode, inst_mode_emis_nonadaptive)) {
		mmode = i1d3_frequency;
	}

	if (IMODETST(p->mode, inst_mode_emis_ambient)) {

		/* Check that the ambient filter is in place */
		if ((ev = i1d3_get_diffpos(p, &pos, 0)) != inst_ok)
 			return ev;
	
		if (pos != 1)
			return i1d3_interp_code((inst *)p, I1D3_SPOS_EMIS);

		/* Best type of reading, including refresh support */
		if ((ev = i1d3_take_emis_measurement(p, mmode, XYZ)) != inst_ok)
			return ev;

		/* Multiply by ambient calibration matrix */
		icmMulBy3x3(XYZ, p->ambi_cal, XYZ);		/* Values in Lux */

	} else {

		/* Check that the ambient filter is not in place */
		if ((ev = i1d3_get_diffpos(p, &pos, 0)) != inst_ok)
 			return ev;
	
		if (pos != 0)
			return i1d3_interp_code((inst *)p, I1D3_SPOS_EMIS);

		/* Best type of reading, including refresh support */
		if ((ev = i1d3_take_emis_measurement(p, mmode, XYZ)) != inst_ok)
			return ev;

		/* Multiply by current emissive calibration matrix */
		icmMulBy3x3(XYZ, p->emis_cal, XYZ);

		/* Apply the (optional) colorimeter correction matrix */
		icmMulBy3x3(XYZ, p->ccmat, XYZ);

	}
	a1logd(p->log,3,"returning XYZ = %f %f %f\n",XYZ[0],XYZ[1],XYZ[2]);
	return inst_ok;
}

// ============================================================

/* Decode the Internal EEPROM */
static inst_code i1d3_decode_intEE(
	i1d3 *p,
	unsigned char *buf			/* Buffer holding 256 bytes from Internal EEProm */
) {
	int i;
	unsigned int t1;

	/* Read the serial number */
	strncpy(p->serial_no, (char *)buf + 0x10, 20);
	p->serial_no[20] = '\000';

	strncpy(p->vers_no, (char *)buf + 0x2C, 10);
	p->vers_no[10] = '\000';

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

	rchsum = buf2short(buf + 2);

	/* For the "A-01" revsions the checksum is from 0x4 to 0x179a */
	/* The "A-02" & B seems to have abandoned reliable checksums ?? */
	/* (Some seem to work between 0x4 - 0x178e or 0xf - 0x1792) */
	for (chsum = 0, i = 4; i < 0x179a; i++)
		chsum += buf[i];
	chsum &= 0xffff;

	if (rchsum != chsum) {

		if (strcmp(p->vers_no, "A-01") == 0) { 
			a1logd(p->log, 1, "i1d3_decode_extEE: checksum failed, is 0x%x, should be 0x%x\n",chsum,rchsum);
			return i1d3_interp_code((inst *)p, I1D3_BAD_EX_CHSUM);
		}

		/* Hmm. Try alternate range */ 
		for (chsum = 0, i = 4; i < 0x178e; i++)
			chsum += buf[i];
		chsum &= 0xffff;

		if (rchsum != chsum) {
			a1logd(p->log, 3, "i1d3_decode_extEE: '%s' checksum failed, is 0x%x, should be 0x%x\n",p->vers_no,chsum,rchsum);
		} else {
			a1logd(p->log, 3, "i1d3_decode_extEE: '%s' alternate checksum succeded!\n",p->vers_no);
		}
		/* Ignore checksum error */
	}
		
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
/* .ccss files) */
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
	if ((conv = new_xsp2cie(icxIT_none, 0.0, NULL, obType, custObserver, icSigXYZData, icxClamp)) == NULL)
		return i1d3_interp_code((inst *)p, I1D3_INT_CIECONVFAIL);
	for (i = 0; i < nsamp; i++) {
		conv->convert(conv, sampXYZ[i], &samples[i]); 
	}
	conv->del(conv);

	/* Compute sensor RGB of the sample array */
	if ((conv = new_xsp2cie(icxIT_none, 0.0, NULL, icxOT_custom, RGBcmfs, icSigXYZData, icxClamp)) == NULL) {
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
	/* (Another possibility is to use a minimization algorithm) */
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

#if defined(PLOT_XYZSPECTRA) || defined(SAVE_XYZSPECTRA)

	/* Compute the calibrated sensor spectra */
	{
		int i, j, k;
		xspect calcmfs[3];
		double scale;
		xspect *xyz[3];
		standardObserver(xyz, icxOT_CIE_1931_2);

		for (j = 0; j < 3; j++) {
			XSPECT_COPY_INFO(&calcmfs[j], &RGBcmfs[j]); 
		}

		/* For each wavelength */
		for (i = 0; i < RGBcmfs[0].spec_n; i++) {
			/* Do matrix multiply */
			for (j = 0; j < 3; j++) {
				calcmfs[j].spec[i] = 0.0;
				for (k = 0; k < 3; k++) {
					calcmfs[j].spec[i] += mat[j][k] * RGBcmfs[k].spec[i];
				}
			}
		}

		/* Scale the X to be 1.0 */
		scale = value_xspect(xyz[1], 555.0)/value_xspect(&calcmfs[1], 555.0);
		for (i = 0; i < RGBcmfs[0].spec_n; i++) {
			for (j = 0; j < 3; j++) {
				calcmfs[j].spec[i] *= scale;
			}
		}


#ifdef PLOT_XYZSPECTRA
		{
			double xx[XSPECT_MAX_BANDS];
			double y1[XSPECT_MAX_BANDS];
			double y2[XSPECT_MAX_BANDS];
			double y3[XSPECT_MAX_BANDS];
	
			for (i = 0; i < calcmfs[0].spec_n; i++) {
				xx[i] = XSPECT_XWL(&calcmfs[0], i);
				y1[i] = calcmfs[0].spec[i];
				y2[i] = calcmfs[1].spec[i];
				y3[i] = calcmfs[2].spec[i];
			}
			printf("The calibrated sensor sensitivities\n");
			do_plot(xx, y1, y2, y3, calcmfs[0].spec_n);
		}
#endif /* PLOT_XYZSPECTRA */
#ifdef SAVE_XYZSPECTRA		/* Save the default XYZ senitivity spectra to "sensorsxyz.cmf" */
		write_nxspect("sensorsxyz.cmf", calcmfs, 3, 0);
#endif
	}
#endif /* PLOT_XYZSPECTRA || SAVE_XYZSPECTRA */
#ifdef SAVE_STDXYZ
	{
		xspect *xyz[3], xyzl[3];
		standardObserver(xyz, icxOT_CIE_1931_2);
		xyzl[0] = *xyz[0];
		xyzl[1] = *xyz[1];
		xyzl[2] = *xyz[2];
		write_nxspect("stdobsxyz.cmf", xyzl, 3, 0);
	}
#endif /* SAVE_STDXYZ */

	return inst_ok;
}

/* Preset the calibration to a spectral sample type. */
/* ccmat[][] is set to unity */
static inst_code
i1d3_set_speccal(
	i1d3 *p,
	xspect *samples,	/* Array of nsamp spectral samples, or RGBcmfs for MIbLSr */
	int nsamp			/* Number of samples */
) {
	int i;

	/* Save a the spectral samples to the current state */
	if (p->samples != NULL)
		free(p->samples);
	p->nsamp = 0;
	if ((p->samples = (xspect *)calloc(sizeof(xspect), nsamp)) == NULL) {
		a1loge(p->log, inst_internal_error, "i1d3_set_speccal: malloc failed\n");
		return inst_internal_error;
	}
	for (i = 0; i < nsamp; i++ )
		p->samples[i] = samples[i];		/* Struct copy */
	p->nsamp = nsamp;

	icmSetUnity3x3(p->ccmat);			/* No matrix */

	return inst_ok;
}

/* Preset the calibration to a matrix. The spectral type is set to none */
static inst_code
i1d3_set_matcal(i1d3 *p, double mtx[3][3]) {

	if (p->samples != NULL)
		free(p->samples);
	p->samples = NULL;
	p->nsamp = 0;

	if (mtx == NULL)
		icmSetUnity3x3(p->ccmat);
	else
		icmCpy3x3(p->ccmat, mtx);

	return inst_ok;
}

/* Set the calibration to the currently preset type */
static inst_code
i1d3_set_cal(i1d3 *p) {
	inst_code ev = inst_ok;

	if (p->samples != NULL && p->nsamp > 0) {

		/* Create matrix for specified samples */
		if ((ev = i1d3_comp_calmat(p, p->emis_cal, p->obType, p->custObserver,
			                       p->sens, p->samples, p->nsamp)) != inst_ok) {
			a1logd(p->log, 1, "i1d3_set_cal: comp_calmat ccss failed with rv = 0x%x\n",ev);
			return ev;
		}
		/* Use MIbLSr for ambient */
		if ((ev = i1d3_comp_calmat(p, p->ambi_cal, p->obType, p->custObserver,
			                       p->ambi, p->ambi, 3)) != inst_ok)
			return ev;

		icmSetUnity3x3(p->ccmat);		/* to be sure to be sure... */

	} else {	/* Assume default spectral samples, + possible matrix */

		/* Create the default MIbLSr calibration matrix */
		if ((ev = i1d3_comp_calmat(p, p->emis_cal, p->obType, p->custObserver,
			                              p->sens, p->sens, 3)) != inst_ok) {
			a1logd(p->log, 1, "i1d3_set_cal: comp_calmat dflt failed with rv = 0x%x\n",ev);
			return ev;
		}
		/* Use MIbLSr for ambient */
		if ((ev = i1d3_comp_calmat(p, p->ambi_cal, p->obType, p->custObserver,
			                              p->ambi, p->ambi, 3)) != inst_ok)
			return ev;

		/* We assume any ccmat has/will be set by caller */
	}

	if (p->log->debug >= 4) {
		if (IMODETST(p->mode, inst_mode_emis_ambient)) {
			a1logd(p->log,4,"Ambient matrix  = %f %f %f\n",
			                 p->ambi_cal[0][0], p->ambi_cal[0][1], p->ambi_cal[0][2]);
			a1logd(p->log,4,"                  %f %f %f\n",
			                 p->ambi_cal[1][0], p->ambi_cal[1][1], p->ambi_cal[1][2]);
			a1logd(p->log,4,"                  %f %f %f\n\n",
			                 p->ambi_cal[2][0], p->ambi_cal[2][1], p->ambi_cal[2][2]);
		} else {
			a1logd(p->log,4,"Emissive matrix = %f %f %f\n",
			                 p->emis_cal[0][0], p->emis_cal[0][1], p->emis_cal[0][2]);
			a1logd(p->log,4,"                  %f %f %f\n",
			                 p->emis_cal[1][0], p->emis_cal[1][1], p->emis_cal[1][2]);
			a1logd(p->log,4,"                  %f %f %f\n\n",
			                 p->emis_cal[2][0], p->emis_cal[2][1], p->emis_cal[2][2]);
		}
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

/* ------------------------------------------------------------------------ */

/* Establish communications with a I1D3 */
/* Return I1D3_COMS_FAIL on failure to establish communications */
static inst_code
i1d3_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	i1d3 *p = (i1d3 *) pp;
	int stat, se;
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

	a1logd(p->log, 2, "i1d3_init_coms: called\n");

	/* On Linux, the i1d3 doesn't seem to close properly, and won't re-open - */
	/* something to do with detaching the default HID driver ?? */
#if defined(UNIX_X11)
	usbflags |= icomuf_detach;
	usbflags |= icomuf_reset_before_close;
#endif

	/* On MSWin it doesn't like clearing on open when running direct (i.e not HID) */
	usbflags |= icomuf_no_open_clear;

	/* Open as an HID if available */
	if (p->icom->port_type(p->icom) == icomt_hid) {

		a1logd(p->log, 2, "i1d3_init_coms: About to init HID\n");

		/* Set config, interface */
		if ((se = p->icom->set_hid_port(p->icom, icomuf_none, retries, pnames))
			                                                        != ICOM_OK) { 
			a1logd(p->log, 1, "i1d3_init_coms: set_hid_port failed ICOM err 0x%x\n",se);
			return i1d3_interp_code((inst *)p, icoms2i1d3_err(se, 0));
		}

	} else if (p->icom->port_type(p->icom) == icomt_usb) {

		a1logd(p->log, 2, "i1d3_init_coms: About to init USB\n");

		/* Set config, interface, write end point, read end point */
		/* ("serial" end points aren't used - the i1d3 uses USB control messages) */
		/* We need to detatch the HID driver on Linux */
		if ((se = p->icom->set_usb_port(p->icom, 1, 0x00, 0x00, usbflags, 0, NULL))
			                                                                          != ICOM_OK) { 
			a1logd(p->log, 1, "i1d3_init_coms: set_usb_port failed ICOM err 0x%x\n",se);
			return i1d3_interp_code((inst *)p, icoms2i1d3_err(se, 0));
		}

	} else {
		a1logd(p->log, 1, "i1d3_init_coms: wrong communications type for device!\n");
		return inst_coms_fail;
	}

	/* Check instrument is responding */
	if ((ev = i1d3_check_status(p,&stat)) != inst_ok) {
		a1logd(p->log, 1, "i1d3_init_coms: failed with rv = 0x%x\n",ev);
		return ev;
	}
	a1logd(p->log, 2, "i1d3_init_coms: suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

/* Diffuser position thread. */
/* Poll the instrument at 100msec intervals */
static int i1d3_diff_thread(void *pp) {
	int nfailed = 0;
	i1d3 *p = (i1d3 *)pp;
	inst_code rv = inst_ok; 
	a1logd(p->log,3,"Diffuser thread started\n");
//	for (nfailed = 0; nfailed < 5;)
	/* Try indefinitely, in case instrument is put to sleep */
	for (;;) {
		int pos;

		/* Don't get diffpos if we're doing something else that */
		/* is timing critical */
		if (p->th_en) {
//a1logd(p->log,3,"Diffuser thread loop debug = %d\n",p->log->debug);
			rv = i1d3_get_diffpos(p, &pos, p->log->debug < 8 ? 1 : 0); 
			if (p->th_term) {
				p->th_termed = 1;
				break;
			}
			if (rv != inst_ok) {
				nfailed++;
				a1logd(p->log,3,"Diffuser thread failed with 0x%x\n",rv);
				continue;
			}
			if (pos != p->dpos) {
				p->dpos = pos;
				if (p->eventcallback != NULL) {
					p->eventcallback(p->event_cntx, inst_event_mconf);
				}
			}
		}
		msec_sleep(100);
	}
	a1logd(p->log,3,"Diffuser thread returning\n");
	return rv;
}

static inst_code set_default_disp_type(i1d3 *p);

/* Initialise the I1D3 */
static inst_code
i1d3_init_inst(inst *pp) {
	i1d3 *p = (i1d3 *)pp;
	inst_code ev = inst_ok;
	int i, stat;
	unsigned char buf[8192];

	a1logd(p->log, 2, "i1d3_init_inst: called, debug = %d\n",p->log->debug);

	p->rrset = 0;

	if (p->gotcoms == 0)
		return i1d3_interp_code((inst *)p, I1D3_NO_COMS);	/* Must establish coms first */

	// Get instrument information */
	if ((ev = i1d3_check_status(p, &p->status)) != inst_ok)
		return ev;
	if (p->status != 0) {
		a1logd(p->log, 1, "i1d3_init_inst: bad device status\n");
		return i1d3_interp_code((inst *)p, I1D3_BAD_STATUS);
	}

	if ((ev = i1d3_get_prodname(p, p->prod_name)) != inst_ok)
		return ev;
	if ((ev = i1d3_get_prodtype(p, &p->prod_type)) != inst_ok)
		return ev;
	if (p->prod_type == 0x0002) {	/* If ColorMunki Display */
		/* Set this in case it doesn't need unlocking */
		p->btype = p->stype = i1d3_munkdisp;
	}
	if ((ev = i1d3_get_firmver(p, p->firm_ver)) != inst_ok)
		return ev;
	if ((ev = i1d3_get_firmdate(p, p->firm_date)) != inst_ok)
		return ev;

	/* Unlock instrument */
	if ((ev = i1d3_lock_status(p,&stat)) != inst_ok)
		return ev;

	if (stat != 0) {		/* Locked, so unlock it */
		a1logd(p->log, 3, "i1d3_init_inst: unlocking the instrument\n");

		if ((ev = i1d3_unlock(p)) != inst_ok)
			return ev;
		if ((ev = i1d3_lock_status(p,&stat)) != inst_ok)
			return ev;
		if (stat != 0) {
			a1logd(p->log, 1, "i1d3_init_inst: failed to unlock instrument\n");
			return i1d3_interp_code((inst *)p, I1D3_UNLOCK_FAIL);
		}
	}

	/* Read the instrument information and calibration */
	if ((ev = i1d3_read_internal_eeprom(p,0,256,buf)) != inst_ok)
		return ev;
	if (p->log->debug >= 8) {
		a1logd(p->log, 8, "Internal EEPROM:\n"); 
		adump_bytes(p->log, "  ", buf, 0, 256);
	}
	/* Decode the Internal EEPRom */
	if ((ev = i1d3_decode_intEE(p, buf)) != inst_ok)
		return ev;

	if ((ev = i1d3_read_external_eeprom(p,0,8192,buf)) != inst_ok)
		return ev;
	if (p->log->debug >= 8) {
		a1logd(p->log, 8, "External EEPROM:\n"); 
		adump_bytes(p->log, "  ", buf, 0, 8192);
	}
	/* Decode the External EEPRom */
	if ((ev = i1d3_decode_extEE(p, buf)) != inst_ok)
		return ev;

	/* Set known constants */
	p->clk_freq = 12e6;				/* 12 Mhz */
	p->omininttime = 0.0;			/* No override */
	p->dinttime = 0.2;				/* 0.2 second integration time default */
	p->inttime = p->dinttime;		/* Start in non-refresh mode */
	p->mininttime = p->inttime;			/* Current value */

	/* Create the default calibrations */

	p->obType = icxOT_CIE_1931_2;	/* Set the default ccss observer */

	/* Setup the default display type */
	if ((ev = set_default_disp_type(p)) != inst_ok) {
		return ev;
	}

	p->inited = 1;
	a1logd(p->log, 2, "i1d3_init_inst: inited OK\n");

	a1logv(p->log,1,"Product Name:      %s\n"
	                "Serial Number:     %s\n"
	                "Firmware Version:  %s\n"
	                "Firmware Date:     %s\n"
	                ,p->prod_name,p->serial_no,p->firm_ver,p->firm_date);

	if (p->log->debug >= 4) {
		a1logd(p->log,4,"Default calibration:\n");
		a1logd(p->log,4,"Emissive matrix = %f %f %f\n",
		                 p->emis_cal[0][0], p->emis_cal[0][1], p->emis_cal[0][2]);
		a1logd(p->log,4,"                  %f %f %f\n",
		                 p->emis_cal[1][0], p->emis_cal[1][1], p->emis_cal[1][2]);
		a1logd(p->log,4,"                  %f %f %f\n",
		                 p->emis_cal[2][0], p->emis_cal[2][1], p->emis_cal[2][2]);
		a1logd(p->log,4,"Ambient matrix  = %f %f %f\n",
		                 p->ambi_cal[0][0], p->ambi_cal[0][1], p->ambi_cal[0][2]);
		a1logd(p->log,4,"                  %f %f %f\n",
		                 p->ambi_cal[1][0], p->ambi_cal[1][1], p->ambi_cal[1][2]);
		a1logd(p->log,4,"                  %f %f %f\n\n",
		                 p->ambi_cal[2][0], p->ambi_cal[2][1], p->ambi_cal[2][2]);
		a1logd(p->log,4,"\n");
	}

	/* Start the diffuser monitoring thread */
	p->th_en = 1;
	if ((p->th = new_athread(i1d3_diff_thread, (void *)p)) == NULL)
		return i1d3_interp_code((inst *)p, I1D3_INT_THREADFAILED);

	/* Flash the LED, just cos we can! */
	if ((ev = i1d3_set_LEDs(p, i1d3_flash, 0.2, 0.05, 2)) != inst_ok)
		return ev;

	a1logd(p->log, 2, "i1d3_init_inst: done\n");

	return ev;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
i1d3_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	i1d3 *p = (i1d3 *)pp;
	int user_trig = 0;
	int rv = inst_protocol_error;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	a1logd(p->log, 1, "i1d3: i1d3_read_sample called\n");

	if (p->trig == inst_opt_trig_user) {

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "i1d3: inst_opt_trig_user but no uicallback function set!\n");
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
		 && (rv = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort) {
			return rv;				/* Abort */
		}
	}

	/* Attempt a refresh display frame rate calibration if needed */
	if (p->btype != i1d3_munkdisp && p->refrmode != 0 && p->rrset == 0) {
		inst_code ev = inst_ok;

		p->mininttime = 2.0 * p->dinttime;

		if (p->omininttime != 0.0)
			p->mininttime = p->omininttime;	/* Override */
		
#ifdef DEBUG_TWEAKS
		{
			char *cp;
			if ((cp = getenv("I1D3_MIN_INT_TIME")) != NULL)
				p->mininttime = atof(cp);
		}
#endif

		if ((ev = i1d3_measure_set_refresh(p)) != inst_ok)
			return ev; 

		/* Quantize the sample time */
		if (p->refperiod > 0.0) {		/* If we have a refresh period */
			int n;
			n = (int)ceil(p->mininttime/p->refperiod);
			p->inttime = n * p->refperiod;
			a1logd(p->log, 3, "i1d3: integration time quantize to %f secs\n",p->inttime);

		} else {	/* We don't have a period, so simply use the double default */
			p->inttime = p->mininttime;
			a1logd(p->log, 3, "i1d3: integration time integration time doubled to %f secs\n",p->inttime);
		}
	}


	/* Read the XYZ value */
	rv = i1d3_take_XYZ_measurement(p, val->XYZ);

	if (rv != inst_ok)
		return rv;


	/* This may not change anything since instrument may clamp */
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);

	val->loc[0] = '\000';
	if (IMODETST(p->mode, inst_mode_emis_ambient))
		val->mtype = inst_mrt_ambient;
	else
		val->mtype = inst_mrt_emission;
	val->XYZ_v = 1;
	val->sp.spec_n = 0;
	val->duration = 0.0;


	if (user_trig)
		return inst_user_trig;

	return rv;
}

/* Read an emissive refresh rate */
static inst_code
i1d3_read_refrate(
inst *pp,
double *ref_rate) {
	i1d3 *p = (i1d3 *)pp;
	inst_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->btype == i1d3_munkdisp)
		return inst_unsupported;

	if (ref_rate != NULL)
		*ref_rate = 0.0;

	if ((rv = i1d3_imp_measure_refresh(p, ref_rate, NULL)) != inst_ok)
		return rv;


	if (ref_rate != NULL && *ref_rate == 0.0)
		return inst_misread;


	return inst_ok;
}

/* Make a possible change of the refresh mode */
static void update_refmode(i1d3 *p, int refrmode) {

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

	/* default before any refresh rate calibration */
	if (p->refrmode) {
		p->inttime = 2.0 * p->dinttime;	/* Double integration time */
	} else {
		p->inttime = p->dinttime;		/* Normal integration time */
	}
	if (p->omininttime != 0.0)
		p->inttime = p->omininttime;	/* Override */
	p->mininttime = p->inttime;			/* Current value */
}

static inst_code set_base_disp_type(i1d3 *p, int cbid);

/* Insert a colorimetric correction matrix in the instrument XYZ readings */
/* This is only valid for colorimetric instruments. */
/* To remove the matrix, pass NULL for the matrix. */
static inst_code i1d3_col_cor_mat(
inst *pp,
disptech dtech,		/* Use disptech_unknown if not known */				\
int cbid,       	/* Calibration display type base ID, 1 if unknown */\
double mtx[3][3]
) {
	i1d3 *p = (i1d3 *)pp;
	inst_code ev = inst_ok;

	a1logd(p->log, 4, "i1d3_col_cor_mat%s dtech %d cbid %d\n",mtx == NULL ? " (noop)": "",dtech,cbid);

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if ((ev = set_base_disp_type(p, cbid)) != inst_ok)
		return ev;

	if ((ev = i1d3_set_matcal(p, mtx)) != inst_ok)
		return ev;

	p->dtech = dtech;
	p->cbid = 0;

	/* Effective refresh mode may change */
	update_refmode(p, disptech_get_id(dtech)->refr);

	return i1d3_set_cal(p);
}

/* Use a Colorimeter Calibration Spectral Set to set the */
/* instrumen calibration. */
/* This is only valid for colorimetric instruments. */
/* To set calibration back to default, pass NULL for sets. */
static inst_code i1d3_col_cal_spec_set(
inst *pp,
disptech dtech,
xspect *sets,
int no_sets
) {
	i1d3 *p = (i1d3 *)pp;
	inst_code ev = inst_ok;

	a1logd(p->log, 4, "i1d3_col_cal_spec_set%s\n",sets == NULL ? " (default)": "");

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	p->dtech = dtech;
	p->cbid = 0;

	if (sets == NULL || no_sets <= 0) {
		if ((ev = set_default_disp_type(p)) != inst_ok) {
			return ev;
		}
	} else {
		if ((ev = i1d3_set_speccal(p, sets, no_sets)) != inst_ok)
			return ev; 

		p->ucbid = 0;			/* We're using external samples */
		ev = i1d3_set_cal(p);
	}
	update_refmode(p, disptech_get_id(dtech)->refr);

	return ev;
}

/* Return needed and available inst_cal_type's */
static inst_code i1d3_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	i1d3 *p = (i1d3 *)pp;
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
		
	if (p->btype != i1d3_munkdisp && p->refrmode != 0) {
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
static inst_code i1d3_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
inst_calc_id_type *idtype,	/* Condition identifier type */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	i1d3 *p = (i1d3 *)pp;
	inst_code ev;
    inst_cal_type needed, available;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	*idtype = inst_calc_id_none;
	id[0] = '\000';

	if ((ev = i1d3_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
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

		a1logd(p->log,4,"i1d3_calibrate: doing calt 0x%x\n",calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return inst_ok;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return inst_unsupported;
	}

	if ((*calt & inst_calt_ref_freq) && p->btype != i1d3_munkdisp && p->refrmode != 0) {
		inst_code ev = inst_ok;

		p->mininttime = 2.0 * p->dinttime;

		if ((*calc & inst_calc_cond_mask) != inst_calc_emis_80pc) {
			*calc = inst_calc_emis_80pc;
			return inst_cal_setup;
		}

		if (p->omininttime != 0.0)
			p->mininttime = p->omininttime;	/* Override */
		
#ifdef DEBUG_TWEAKS
		{
			char *cp;
			if ((cp = getenv("I1D3_MIN_INT_TIME")) != NULL)
				p->mininttime = atof(cp);
		}
#endif

		/* Do refresh display rate calibration */
		if ((ev = i1d3_measure_set_refresh(p)) != inst_ok)
			return ev; 

		/* Quantize the sample time */
		if (p->refperiod > 0.0) {
			int n;
			n = (int)ceil(p->mininttime/p->refperiod);
			p->inttime = n * p->refperiod;
			a1logd(p->log, 3, "i1d3: integration time quantize to %f secs\n",p->inttime);
		} else {
			p->inttime = p->mininttime;	/* Double default integration time */
			a1logd(p->log, 3, "i1d3: integration time integration time doubled to %f secs\n",p->inttime);
		}
		*calt &= ~inst_calt_ref_freq;
	}
	return inst_ok;
}

/* Measure a display update delay. It is assumed that */
/* white_stamp(init) has been called, and then a */
/* black to white change has been made to the displayed color, */
/* and this will measure the time it took for the update to */
/* be noticed by the instrument, up to 2.0 seconds. */
/* (It is assumed that white_change() will be called at the time the patch */
/* changes color.) */
/* inst_misread will be returned on failure to find a transition to black. */
#define NDSAMPS 400 
#define DINTT 0.005			/* Too short hits blanking */
#define NDMXTIME 2.0		/* Maximum time to take */

static inst_code i1d3_meas_delay(
inst *pp,
int *pdispmsec,      /* Return display update delay in msec */
int *pinstmsec) {    /* Return instrument reaction time in msec */
	i1d3 *p = (i1d3 *)pp;
	inst_code ev;
	int i, j, k;
	double putime, cutime;
	int mtachdel;
	struct {
		double sec;
		double rgb[3];
		double tot;
	} samp[NDSAMPS];
	int ndsamps;
	double inttime = DINTT;
	double stot, etot, del, thr;
	double stime, etime;
	int isdeb;
	int isth;
	int dispmsec, instmsec;

	if (pinstmsec != NULL)
		*pinstmsec = 0; 

	if (!p->gotcoms)
		return inst_no_coms;

	if (!p->inited)
		return inst_no_init;

	if (usec_time() < 0.0) {
		a1loge(p->log, inst_internal_error, "i1d3_meas_delay: No high resolution timers\n");
		return inst_internal_error; 
	}

	/* Turn debug and thread off so that they doesn't intefere with measurement timing */
	isdeb = p->log->debug;
	p->icom->log->debug = 0;
	isth = p->th_en;
	p->th_en = 0;

	/* Read the samples */
	putime = usec_time() / 1000000.0;
	for (i = 0; i < NDSAMPS; i++) {
		if ((ev = i1d3_freq_measure(p, &inttime, samp[i].rgb)) != inst_ok) {
			a1logd(p->log, 1, "i1d3_meas_delay: measurement failed\n");
			p->log->debug = isdeb;
			p->th_en = isth;
 			return ev;
		}
		cutime = usec_time() / 1000000.0;
		samp[i].sec = 0.5 * (putime + cutime);	/* Mean of before and after stamp */
		putime = cutime;
		samp[i].tot = samp[i].rgb[0] + samp[i].rgb[1] + samp[i].rgb[2];
		if (cutime > NDMXTIME)
			break; 
	}
	ndsamps = i;

	/* Restore debugging & thread */
	p->log->debug = isdeb;
	p->th_en = isth;

	if (ndsamps == 0) {
		a1logd(p->log, 1, "i1d3_meas_delay: No measurement samples returned in time\n");
		return inst_internal_error; 
	}

	if (p->whitestamp < 0.0) {
		a1logd(p->log, 1, "i1d3_meas_delay: White transition wasn't timestamped\n");
		return inst_internal_error; 
	}

	/* Set the times to be white transition relative */
	for (i = 0; i < ndsamps; i++)
		samp[i].sec -= p->whitestamp / 1000000.0;

	/* Over the first 100msec, locate the maximum value */
	stime = samp[0].sec;
	stot = -1e9;
	for (i = 0; i < ndsamps; i++) {
		if (samp[i].tot > stot)
			stot = samp[i].tot;
		if ((samp[i].sec - stime) > 0.1)
			break;
	}

	/* Over the last 100msec, locate the maximum value */
	etime = samp[ndsamps-1].sec;
	etot = -1e9;
	for (i = ndsamps-1; i >= 0; i--) {
		if (samp[i].tot > etot)
			etot = samp[i].tot;
		if ((etime - samp[i].sec) > 0.1)
			break;
	}

	del = etot - stot;
	thr = stot + 0.2 * del;		/* 20% of transition threshold */

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "i1d3_meas_delay: start tot %f end tot %f del %f, thr %f\n", stot, etot, del, thr);
#endif

#ifdef PLOT_UPDELAY
	/* Plot the raw sensor values */
	{
		double xx[NDSAMPS];
		double y1[NDSAMPS];
		double y2[NDSAMPS];
		double y3[NDSAMPS];
		double y4[NDSAMPS];

		for (i = 0; i < ndsamps; i++) {
			xx[i] = samp[i].sec;
			y1[i] = samp[i].rgb[0];
			y2[i] = samp[i].rgb[1];
			y3[i] = samp[i].rgb[2];
			y4[i] = samp[i].tot;
			//printf("%d: %f -> %f\n",i,samp[i].sec, samp[i].tot);
		}
		printf("Display update delay measure sensor values and time (sec)\n");
		do_plot6(xx, y1, y2, y3, y4, NULL, NULL, ndsamps);
	}
#endif

	/* Check that there has been a transition */
	if (del < 10.0) {
		a1logd(p->log, 1, "i1d3_meas_delay: can't detect change from black to white\n");
		return inst_misread; 
	}

	/* Working from the start, locate the time at which the level was above the threshold */
	for (i = 0; i < (ndsamps-1); i++) {
		if (samp[i].tot > thr)
			break;
	}

	a1logd(p->log, 2, "i1d3_meas_delay: stoped at sample %d time %f\n",i,samp[i].sec);

	/* Compute overall delay */
	dispmsec = (int)(samp[i].sec * 1000.0 + 0.5);
	instmsec = 0;

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "i1d3_meas_delay: disp %d, inst %d msec\n",dispmsec,instmsec);
#else
	a1logd(p->log, 2, "i1d3_meas_delay: disp %d, inst %d msec\n",dispmsec,instmsec);
#endif

	if (dispmsec < 0) 		/* This can happen if the patch generator delays it's return */
		dispmsec = 0;

	if (pdispmsec != NULL)
		*pdispmsec = dispmsec;

	if (pinstmsec != NULL)
		*pinstmsec = instmsec; 

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "i1d3_meas_delay: returning %d & %d msec\n",dispmsec,instmsec);
#else
	a1logd(p->log, 2, "i1d3_meas_delay: returning %d & %d msec\n",dispmsec,instmsec);
#endif

	return inst_ok;
}
#undef NDSAMPS
#undef DINTT
#undef NDMXTIME

/* Timestamp the white patch change during meas_delay() */
/* Initialise the whitestap to invalid if init nz */
static inst_code i1d3_white_change(
inst *pp, int init) {
	i1d3 *p = (i1d3 *)pp;

	if (init)
		p->whitestamp = -1.0;
	else {
		if ((p->whitestamp = usec_time()) < 0.0) {
			a1loge(p->log, inst_internal_error, "i1d3_wite_change: No high resolution timers\n");
			return inst_internal_error; 
		}
	}
	return inst_ok;
}

/* Return the last calibrated refresh rate in Hz. Returns: */
static inst_code i1d3_get_refr_rate(inst *pp,
double *ref_rate
) {
	i1d3 *p = (i1d3 *)pp;
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
/* Note that it's possible to set a ColorMunki Display to use */
/* synchronised measurement this way. */
static inst_code i1d3_set_refr_rate(inst *pp,
double ref_rate
) {
	i1d3 *p = (i1d3 *)pp;

	if (ref_rate != 0.0 && (ref_rate < 5.0 || ref_rate > 150.0))
		return inst_bad_parameter;

	p->refrate = ref_rate;
	if (ref_rate == 0.0)
		p->refrvalid = 0;
	else {
		int mul;
		double pval;

		/* Scale to just above 20 Hz, but make it multiple of 2 or 4 */
		pval = 1.0/ref_rate;
#ifdef DEBUG_TWEAKS
		{
			double quanttime = 1.0/20.0;
			char *cp;
			if ((cp = getenv("I1D3_MIN_REF_QUANT_TIME")) != NULL)
				quanttime = atof(cp);
			mul = (int)floor(quanttime / pval);
		}
#else
		mul = (int)floor((1.0/20.0) / pval);
#endif
		if (mul > 1) {
			if (mul >= 8)
				mul = (mul + 3) & ~3;	/* Round up to mult of 4 */
			else
				mul = (mul + 1) & ~1;	/* Round up to mult of 2 */
			pval *= mul;
		}
		p->refperiod = pval;
		p->refrvalid = 1;
	}
	p->rrset = 1;

	return inst_ok;
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
		case I1D3_INT_THREADFAILED:
			return "Starting diffuser position thread failed";

		case I1D3_TOOBRIGHT:
			return "Too bright to read accuractly";

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
			
		case I1D3_INTERNAL_ERROR:
			return inst_internal_error | ec;

		case I1D3_BAD_MEM_ADDRESS:
		case I1D3_BAD_MEM_LENGTH:
		case I1D3_INT_CIECONVFAIL:
		case I1D3_TOO_FEW_CALIBSAMP:
		case I1D3_INT_MATINV_FAIL:
		case I1D3_BAD_LED_MODE:
		case I1D3_NO_COMS:
		case I1D3_NOT_INITED:
		case I1D3_INT_THREADFAILED:
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

		case I1D3_SPOS_EMIS:
		case I1D3_SPOS_AMB:
			return inst_wrong_config | ec;

		case I1D3_BAD_EX_CHSUM:
			return inst_hardware_fail | ec;

		case I1D3_TOOBRIGHT:
			return inst_misread | ec;

/* Unused:
	inst_notify
	inst_warning
	inst_unknown_model
	inst_nonesaved
	inst_nochmatch
	inst_needs_cal
	inst_cal_setup
	inst_unsupported
	inst_unexpected_reply
	inst_wrong_setup
	inst_bad_parameter
 */
	}
	return inst_other_error | ec;
}

/* Convert instrument specific inst_wrong_config error to inst_config enum */
static inst_config i1d3_config_enum(inst *pp, int ec) {
//	i1d3 *p = (i1d3 *)pp;

	ec &= inst_imask;
	switch (ec) {

		case I1D3_SPOS_EMIS:
			return inst_conf_emission;

		case I1D3_SPOS_AMB:
			return inst_conf_ambient;
	}
	return inst_conf_unknown;
}

/* Destroy ourselves */
static void
i1d3_del(inst *pp) {
	if (pp != NULL) {
		i1d3 *p = (i1d3 *)pp;

		if (p->th != NULL) {		/* Terminate diffuser monitor thread  */
			int i;
			p->th_term = 1;			/* Tell thread to exit on error */
			for (i = 0; p->th_termed == 0 && i < 5; i++)
				msec_sleep(50);	/* Wait for thread to terminate */
			if (i >= 5) {
				a1logd(p->log,3,"i1d3 diffuser thread termination failed\n");
			}
			p->th->del(p->th);
		}
		if (p->icom != NULL)
			p->icom->del(p->icom);
		inst_del_disptype_list(p->dtlist, p->ndtlist);
		if (p->samples != NULL)
			free(p->samples);
		amutex_del(p->lock);
		p->vdel(pp);
		free(p);
	}
}

/* Return the instrument capabilities */
static void i1d3_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	i1d3 *p = (i1d3 *)pp;
	inst_mode cap1 = 0;
	inst2_capability cap2 = 0;

	cap1 |= inst_mode_emis_spot
	     |  inst_mode_emis_tele
	     |  inst_mode_emis_ambient
	     |  inst_mode_emis_refresh_ovd			/* (allow override ccmx & ccss mode) */
	     |  inst_mode_emis_norefresh_ovd
	     |  inst_mode_emis_nonadaptive
	     |  inst_mode_colorimeter
	        ;

	cap2 |= inst2_has_sensmode
	     |  inst2_prog_trig
	     |  inst2_user_trig
	     |  inst2_has_leds
	     |  inst2_disptype
	     |  inst2_ccmx
	     |  inst2_ccss
	     |  inst2_get_min_int_time
	     |  inst2_set_min_int_time
	        ;

	if (p->btype != i1d3_munkdisp) {
		cap2 |= inst2_meas_disp_update;
		cap2 |= inst2_get_refresh_rate;
		cap2 |= inst2_set_refresh_rate;
		cap2 |= inst2_emis_refr_meas;
	}
	if (pcap1 != NULL)
		*pcap1 = cap1;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = inst3_none;
}

/* Return current or given configuration available measurement modes. */
/* NOTE that conf_ix values shoudn't be changed, as it is used as a persistent key */
static inst_code i1d3_meas_config(
inst *pp,
inst_mode *mmodes,
inst_cal_cond *cconds,
int *conf_ix
) {
	i1d3 *p = (i1d3 *)pp;
	inst_code ev;
	inst_mode mval;
	int pos;

	if (mmodes != NULL)
		*mmodes = inst_mode_none;
	if (cconds != NULL)
		*cconds = inst_calc_unknown;

	if (conf_ix == NULL
	 || *conf_ix < 0
	 || *conf_ix > 1) {
		/* Return current configuration measrement modes */
		if ((ev = i1d3_get_diffpos(p, &pos, 0)) != inst_ok)
			return ev;
	} else {
		/* Return given configuration measurement modes */
		pos = *conf_ix;
	}

	if (pos == 1) {
		mval = inst_mode_emis_ambient;
	} else {
		mval = inst_mode_emis_spot
		     | inst_mode_emis_tele;
	}

	/* Add the extra dependent and independent modes */
	mval |= inst_mode_emis_refresh_ovd
	     |  inst_mode_emis_norefresh_ovd
	     |  inst_mode_colorimeter;

	if (mmodes != NULL)	
		*mmodes = mval;

	/* Return configuration index returned */
	if (conf_ix != NULL)
		*conf_ix = pos;

	return inst_ok;
}

/* Check device measurement mode */
static inst_code i1d3_check_mode(inst *pp, inst_mode m) {
	i1d3 *p = (i1d3 *)pp;
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
	 && !IMODETST(m, inst_mode_emis_tele)
	 && !IMODETST(m, inst_mode_emis_ambient)) {
		return inst_unsupported;
	}

	return inst_ok;
}

/* Set device measurement mode */
static inst_code i1d3_set_mode(inst *pp, inst_mode m) {
	i1d3 *p = (i1d3 *)pp;
	int refrmode;
	inst_code ev;

	if ((ev = i1d3_check_mode(pp, m)) != inst_ok)
		return ev;

	p->mode = m;

	/* Effective refresh mode may change */
	update_refmode(p, p->refrmode);

	return inst_ok;
}

static inst_disptypesel i1d3_disptypesel[3] = {
	{
		inst_dtflags_default,
		1,
		"nl",
		"Non-Refresh display",
		0,
		disptech_lcd,
		0
	},
	{
		inst_dtflags_none,			/* flags */
		2,							/* cbid */
		"rc",						/* sel */
		"Refresh display",			/* desc */
		1,							/* refr */
		disptech_crt,				/* disptype */
		1							/* ix */
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
static inst_code i1d3_get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	i1d3 *p = (i1d3 *)pp;
	inst_code rv = inst_ok;

	if (!allconfig && p->dpos) {	/* If set to Ambient, there are no display types ? */

		if (pnsels != NULL)
			*pnsels = 0;
	
		if (psels != NULL)
			*psels = NULL;

		return inst_ok;
	}

	/* Create/Re-create a current list of abailable display types */
	if (p->dtlist == NULL || recreate) {
		if ((rv = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    i1d3_disptypesel, 1 /* doccss*/, 1 /* doccmx */)) != inst_ok) {
			return rv;
		}
	}

	if (pnsels != NULL)
		*pnsels = p->ndtlist;

	if (psels != NULL)
		*psels = p->dtlist;

	return inst_ok;
}

/* Given a display type entry, setup for that type */
static inst_code set_disp_type(i1d3 *p, inst_disptypesel *dentry) {
	inst_code ev;
	int refrmode;

	p->icx = dentry->ix;
	p->dtech = dentry->dtech;
	p->cbid = dentry->cbid;

	update_refmode(p, dentry->refr);

	if (dentry->flags & inst_dtflags_ccss) {	/* Spectral sample */

		if ((ev = i1d3_set_speccal(p, dentry->sets, dentry->no_sets)) != inst_ok) 
			return ev;
		p->ucbid = dentry->cbid;	/* This is underying base if dentry is base selection */

	} else {

		if (dentry->flags & inst_dtflags_ccmx) {	/* Matrix */
			if ((ev = set_base_disp_type(p, dentry->cc_cbid)) != inst_ok)
				return ev;
			if ((ev = i1d3_set_matcal(p, dentry->mat)) != inst_ok)
				return ev;
			p->cbid = 0;	/* Matrix will be an override of cbid set in i1d3_set_cal() */

		} else {	/* Native */
			if ((ev = i1d3_set_matcal(p, NULL)) != inst_ok)		/* Noop */
				return ev;
			p->ucbid = dentry->cbid;	/* This is underying base if dentry is base selection */
		}
	}
	return i1d3_set_cal(p);		/* Make it happen */
}

/* Set the display type */
static inst_code i1d3_set_disptype(inst *pp, int ix) {
	i1d3 *p = (i1d3 *)pp;
	inst_code ev;
	inst_disptypesel *dentry;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    i1d3_disptypesel, 1 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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
static inst_code set_default_disp_type(i1d3 *p) {
	inst_code ev;
	int i;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
		    i1d3_disptypesel, 1 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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
static inst_code set_base_disp_type(i1d3 *p, int cbid) {
	inst_code ev;
	int i;

	if (cbid == 0) {
		a1loge(p->log, 1, "i1d3 set_base_disp_type: can't set base display type of 0\n");
		return inst_wrong_setup;
	}
	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
		    i1d3_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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
static inst_code i1d3_get_disptechi(
inst *pp,
disptech *dtech,
int *refrmode,
int *cbid) {
	i1d3 *p = (i1d3 *)pp;
	if (dtech != NULL)
		*dtech = p->dtech;
	if (refrmode != NULL)
		*refrmode = p->refrmode;
	if (cbid != NULL)
		*cbid = p->cbid;
	return inst_ok;
}

/* 
 * set or reset an optional mode
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 */
static inst_code
i1d3_get_set_opt(inst *pp, inst_opt_type m, ...) {
	i1d3 *p = (i1d3 *)pp;
	inst_code ev;

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

	/* Get the current minimum integration time */
	if (m == inst_opt_get_min_int_time) {
		va_list args;
		double *dpoint;

		va_start(args, m);
		dpoint = va_arg(args, double *);
		va_end(args);

		if (dpoint != NULL)
			*dpoint = p->mininttime;

		return inst_ok;
	}

	/* Set the minimum integration time */
	if (m == inst_opt_set_min_int_time) {
		va_list args;
		double dval;

		va_start(args, m);
		dval = va_arg(args, double);
		va_end(args);

		p->omininttime = dval;

		/* Hmm. This code is duplicated a lot.. */
		if (p->btype != i1d3_munkdisp && p->refrmode != 0) {
			inst_code ev = inst_ok;
	
			p->mininttime = 2.0 * p->dinttime;
	
			if (p->omininttime != 0.0)
				p->mininttime = p->omininttime;	/* Override */
			
#ifdef DEBUG_TWEAKS
			{
				char *cp;
				if ((cp = getenv("I1D3_MIN_INT_TIME")) != NULL)
					p->mininttime = atof(cp);
			}
#endif
	
			/* Quantize the sample time if we have a refresh rate */
			if (p->rrset && p->refperiod > 0.0) {		/* If we have a refresh period */
				int n;
				n = (int)ceil(p->mininttime/p->refperiod);
				p->inttime = n * p->refperiod;
				a1logd(p->log, 3, "i1d3: integration time quantize to %f secs\n",p->inttime);
	
			} else {	/* We don't have a period, so simply use the double default */
				p->inttime = p->mininttime;
				a1logd(p->log, 3, "i1d3: integration time integration time doubled to %f secs\n",p->inttime);
			}
		}

		return inst_ok;
	}


	/* Set the ccss observer type */
	if (m == inst_opt_set_ccss_obs) {
		va_list args;
		icxObserverType obType;
		xspect *custObserver;

		va_start(args, m);
		obType = va_arg(args, icxObserverType);
		custObserver = va_arg(args, xspect *);
		va_end(args);

		if (obType == icxOT_default)
			obType = icxOT_CIE_1931_2;
		p->obType = obType;
		if (obType == icxOT_custom) {
			p->custObserver[0] = custObserver[0];
			p->custObserver[1] = custObserver[1];
			p->custObserver[2] = custObserver[2];
		}

		a1logd(p->log, 4, "inst_opt_set_ccss_obs\n");

		return i1d3_set_cal(p);			/* Recompute calibration if spectral sample */
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

		p->led_state = mask;
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
extern i1d3 *new_i1d3(icoms *icom, instType dtype) {
	i1d3 *p;

	if ((p = (i1d3 *)calloc(sizeof(i1d3),1)) == NULL) {
		a1loge(icom->log, 1, "new_i1d3: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	p->init_coms         = i1d3_init_coms;
	p->init_inst         = i1d3_init_inst;
	p->capabilities      = i1d3_capabilities;
	p->meas_config       = i1d3_meas_config;
	p->check_mode        = i1d3_check_mode;
	p->set_mode          = i1d3_set_mode;
	p->get_disptypesel   = i1d3_get_disptypesel;
	p->set_disptype      = i1d3_set_disptype;
	p->get_disptechi     = i1d3_get_disptechi;
	p->get_set_opt       = i1d3_get_set_opt;
	p->read_sample       = i1d3_read_sample;
	p->read_refrate      = i1d3_read_refrate;
	p->col_cor_mat       = i1d3_col_cor_mat;
	p->col_cal_spec_set  = i1d3_col_cal_spec_set;
	p->get_n_a_cals      = i1d3_get_n_a_cals;
	p->calibrate         = i1d3_calibrate;
	p->meas_delay        = i1d3_meas_delay;
	p->white_change      = i1d3_white_change;
	p->get_refr_rate     = i1d3_get_refr_rate;
	p->set_refr_rate     = i1d3_set_refr_rate;
	p->interp_error      = i1d3_interp_error;
	p->config_enum       = i1d3_config_enum;
	p->del               = i1d3_del;

	p->icom = icom;
	p->dtype = dtype;

	amutex_init(p->lock);
	icmSetUnity3x3(p->ccmat);
	p->dtech = disptech_unknown;

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
	        +  (0xff & (-k[0] >> 16)) + (0xff & (-k[0] >> 24));
		sum += (0xff & -k[1]) + (0xff & (-k[1] >> 8))
	        +  (0xff & (-k[1] >> 16)) + (0xff & (-k[1] >> 24));
	
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
