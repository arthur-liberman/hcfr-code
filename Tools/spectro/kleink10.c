
/* 
 * Argyll Color Management System
 *
 * Klein K10 related functions
 *
 * Author: Graeme W. Gill
 * Date:   29/4/2014
 *
 * Copyright 1996 - 2014, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Based on DTP92.c & specbos.c
 */

/* 
   If you make use of the instrument driver code here, please note
   that it is the author(s) of the code who are responsibility
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

/*

	TTBD:

*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#else	/* !SALONEINSTLIB */
#include "sa_config.h"
#include "numsup.h"
#endif /* !SALONEINSTLIB */
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "kleink10.h"

#undef HIGH_SPEED			/* [und] Use high speed flicker measure for refresh rate etc. */
#define AUTO_AVERAGE		/* [def] Automatically average more readings for low light */
#define RETRY_RANGE_ERROR 4	/* [4]   Retry range error readings 4 times */
#undef ENABLE_L01_ERROR		/* [und] Error on L0 and L1 command failure */

#undef PLOT_REFRESH     /* [und] Plot refresh rate measurement info */
#undef PLOT_UPDELAY     /* [und] Plot update delay measurement info */

#undef TEST_FAKE_CALIBS	/* Fake having a full calibration set (98 calibs) */
#undef TEST_BAUD_CHANGE	/* Torture test baud rate change on non high speed K10 */

static inst_disptypesel k10_disptypesel[98];
static inst_code k10_interp_code(kleink10 *p, int ec);
static inst_code k10_read_cal_list(kleink10 *p);
static inst_code set_default_disp_type(kleink10 *p);
static inst_code k10_read_flicker_samples(kleink10 *p, double duration, double *srate,
                                          double **pvals, int *pnsamp, int usefast);

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 8000		/* Maximum reading message reply size */

/* Decode a K10 error letter */
static int decodeK10err(char c) {
//printf("~1 decoding error code 0x%x\n",c);
	if (c == '0') {
		return K10_OK;
	} else if (c == 'B') {
		return K10_FIRMWARE;
	} else if (c == 'X') {
		return K10_FIRMWARE;
	} else if (c == 'b') {
		return K10_BLACK_EXCESS;
	} else if (c == 's') {
		return K10_BLACK_OVERDRIVE;
	} else if (c == 't') {
		return K10_BLACK_ZERO;
	} else if (c == 'w') {
		return K10_OVER_HIGH_RANGE;
	} else if (c == 'v') {
		return K10_TOP_OVER_RANGE;
	} else if (c == 'u') {
		return K10_BOT_UNDER_RANGE;
	} else if (c == 'L') {
		return K10_AIMING_LIGHTS;
	} else {
		return K10_UNKNOWN;
	}
}

/* Extract an error code from a reply string */
/* Remove the error code from the string and return the */
/* new length in *nlength */
/* Return K10_BAD_RETVAL if no error code can be found */
static int 
extract_ec(char *s, int *nlength, int bread) {
#define MAXECHARS 1
	char *f, *p;
	char tt[MAXECHARS+1];
	int rv;
	p = s + bread;

//printf("Got '%s' bread %d\n",s,bread);

	/* Find the trailing '>' */
	for (p--; p >= s; p--) {
		if (*p == '>')
			break;
	}
	if (p < s) {
//printf("p %d < s %d ? %d\n", p, s, p < s);
		return K10_BAD_RETVAL;
	}
//printf("trailing is at %d '%s'\n",p - s, p);

	/* Find the leading '<' */
	for (f = p-1; f >= (p-MAXECHARS-1) && f >= s; f--) {
		if (*f == '<')
			break;
		/* Turns out the error code may be non-text */
#ifdef NEVER
		if ((*f < '0' || *f > '9')
		 && (*f < 'a' || *f > 'z')
		 && (*f < 'A' || *f > 'Z'))
			return K10_BAD_RETVAL;
#endif /* NEVER */
	}
	if (f < s || f < (p-MAXECHARS-1) || (p-f) <= 1) {
//printf("f < s ? %d, f < (p-MAXECHARS-1) ? %d, (p-f) <= 1 ? %d\n", f < s, f < (p-10), (p-f) <= 1);
		return K10_BAD_RETVAL;
	}
//printf("leading is at %d '%s'\n",f - s, f);

	if (p-f-1 <= 0) {
//printf("p-f-1 %d <= 0 ? %d\n", p-f-1, p-f-1 <= 0);
		return K10_BAD_RETVAL;
	}

	strncpy(tt, f+1, p-f-1);
	tt[p-f-1] = '\000';
//printf("error code is '%s'\n",tt);

	/* Interpret the error character(s) */
	/* It's not clear if more than one error can be returned. */
	/* We are only looking at the first character - we should */
	/* really prioritize them if more than one can occur. */
	for (p = tt; *p != '\000'; p++) {
		rv = decodeK10err(*p);
		break;
	}

	/* Remove the error code from the reply */
	if (nlength != NULL)
		*nlength = f - s;
	*f = '\000';
	return rv;
}

/* Interpret an icoms error into a KLEINK10 error */
static int icoms2k10_err(int se) {
	if (se != ICOM_OK) {
		if (se & ICOM_TO)
			return K10_TIMEOUT; 
		return K10_COMS_FAIL;
	}
	return K10_OK;
}

typedef enum {
	ec_n = 0,			/* No error code or command echo */
	ec_e = 1,			/* Error code */
	ec_c = 2,			/* Command echo */
	ec_ec = 3			/* Both error code and command echo */
} ichecks;

/* Do a full command/response echange with the kleink10 */
/* (This level is not multi-thread safe) */
/* Return the kleink10 error code. */
static int
k10_fcommand(
struct _kleink10 *p,
char *in,				/* In string */
char *out,				/* Out string buffer */
int bsize,				/* Out buffer size */
int *pbread,			/* Bytes read (including '\000') */
int nchar,				/* Number of characters to expect */
double to,				/* Timeout in seconds */
ichecks xec,			/* Error check */
int nd					/* nz to disable debug messages */
) {
	int se, rv = K10_OK;
	int bwrite, bread = 0;
	char cmd[10];

	bwrite = strlen((char *)in);
	strncpy((char *)cmd, (char *)in, 2);
	cmd[2] = '\000';

	if ((se = p->icom->write_read_ex(p->icom, in, 0, out, bsize, &bread, NULL, nchar, to, 1))
		                                                                    != ICOM_OK) {
		rv =  icoms2k10_err(se);

	} else {

		if (!nd && p->log->debug >= 6) {
			a1logd(p->log, 6, "k10_fcommand: command sent\n");
			adump_bytes(p->log, "  ", (unsigned char *)in, 0, bwrite);
			a1logd(p->log, 6, "  returned %d bytes:\n",bread);
			adump_bytes(p->log, "  ", (unsigned char *)out, 0, bread);
		}

		if (xec & ec_e) {
			rv = extract_ec(out, &bread, bread);
		}

		if ((xec & ec_c) && rv == K10_OK && strncmp(cmd, out, 2) != 0) {
			rv = K10_CMD_VERIFY;
		}
	}
	if (!nd) a1logd(p->log, 6, "  error code 0x%x\n",rv);

	if (pbread != NULL)
		*pbread = bread;

	return rv;
}

/* Do a normal command/response echange with the kleink10. */
/* (This level is not multi-thread safe) */
/* Return the inst code */
static inst_code
k10_command(
kleink10 *p,
char *in,		/* In string */
char *out,		/* Out string buffer */
int bsize,				/* Out buffer size */
int *bread,				/* Bytes read */
int nchar,				/* Number of characters to expect */
ichecks xec,			/* Error check */
double to) {			/* Timout in seconds */
	int rv = k10_fcommand(p, in, out, bsize, bread, nchar, to, xec, 0);
	return k10_interp_code(p, rv);
}

/* Do a write to the kleink10 */
/* (This level is not multi-thread safe) */
/* Return the kleink10 error code. */
static int
k10_write(
struct _kleink10 *p,
char *in,				/* In string */
double to				/* Timeout in seconds */
) {
	int rv = K10_OK;
	int se;

	if ((se = p->icom->write(p->icom, in, 0, to)) != ICOM_OK) {
		rv =  icoms2k10_err(se);

	} else {

		if (p->log->debug >= 6) {
			a1logd(p->log, 6, "k10_write: command sent\n");
			adump_bytes(p->log, "  ", (unsigned char *)in, 0, strlen((char *)in));
		}
	}
	a1logd(p->log, 6, "  error code 0x%x\n",rv);

	return rv;
}

/* Do a read from the kleink10 */
/* (This level is not multi-thread safe) */
/* Return the kleink10 error code. */
static int
k10_read(
struct _kleink10 *p,
char *out,				/* Out string buffer */
int bsize,				/* Out buffer size */
int *pbread,			/* Bytes read (including '\000') */
char *tc,				/* Terminating characters, NULL for none or char count mode */
int nchar,				/* Number of terminating characters needed, or char count needed */
double to				/* Timeout in seconds */
) {
	int se, rv = K10_OK;
	int bread = 0;

	if ((se = p->icom->read(p->icom, out, bsize, &bread, tc, nchar, to)) != ICOM_OK) {
		rv =  icoms2k10_err(se);
	} else {

		if (p->log->debug >= 6) {
			a1logd(p->log, 6, "k10_read: read %d bytes\n",bread);
			adump_bytes(p->log, "  ", (unsigned char *)out, 0, bread);
		}
	}
	a1logd(p->log, 6, "  error code 0x%x\n",rv);

	if (pbread != NULL)
		*pbread = bread;

	return rv;
}

/* Change baud rates */
/* (This level is not multi-thread safe) */
/* Return the kleink10 error code. */
static int
k10_set_baud(
struct _kleink10 *p,
baud_rate br
) {
	int se, rv = K10_OK;
	if ((se = p->icom->set_ser_port(p->icom, fc_None, br, parity_none,
		                                      stop_1, length_8)) != ICOM_OK) {
		rv =  icoms2k10_err(se);
	} else {
		if (p->log->debug >= 6) {
			a1logd(p->log, 6, "k10_set_baud: %d\n",br);
		}
	}
	a1logd(p->log, 6, "  error code 0x%x\n",rv);

	return rv;
}

/* ------------------------------------------------------------ */

/* Establish communications with a kleink10 */
/* Return K10_COMS_FAIL on failure to establish communications */
static inst_code
k10_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	kleink10 *p = (kleink10 *) pp;
	char buf[MAX_MES_SIZE];
	baud_rate brt[] = { baud_9600, baud_nc };
	unsigned int etime;
	unsigned int i;
	instType dtype = pp->dtype;
	int se;
	char *cp;

	inst_code ev = inst_ok;

	a1logd(p->log, 2, "k10_init_coms: About to init Serial I/O\n");

	if (p->gotcoms) {
		a1logd(p->log, 2, "k10_init_coms: already inited\n");
		return inst_ok;
	}

	amutex_lock(p->lock);

	if (!(p->icom->port_type(p->icom) & icomt_serial)) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_init_coms: wrong communications type for device!\n");
		return inst_coms_fail;
	}
	
	/* The tick to give up on */
	etime = msec_time() + (long)(500.0 + 0.5);

	a1logd(p->log, 1, "k10_init_coms: Trying different baud rates (%u msec to go)\n",etime - msec_time());

	/* Until we time out, find the correct baud rate */
	for (i = 0; msec_time() < etime; i++) {
		if (brt[i] == baud_nc) {
			i = 0;
		}
		a1logd(p->log, 5, "k10_init_coms: Trying %s baud, %d msec to go\n",
			                      baud_rate_to_str(brt[i]), etime- msec_time());
		if ((se = p->icom->set_ser_port(p->icom, fc_None, brt[i], parity_none,
			                         stop_1, length_8)) != ICOM_OK) { 
			amutex_unlock(p->lock);
			a1logd(p->log, 5, "k10_init_coms: set_ser_port failed with 0x%x\n",se);
			return k10_interp_code(p, icoms2k10_err(se));;		/* Give up */
		}

		/* Check instrument is responding */
		if (((ev = k10_command(p, "P0\r", buf, MAX_MES_SIZE, NULL, 21, ec_ec, 0.5)) & inst_mask)
			                                                       != inst_coms_fail) {
			goto got_coms;		/* We've got coms or user abort */
		}

		/* Check for user abort */
		if (p->uicallback != NULL) {
			inst_code ev;
			if ((ev = p->uicallback(p->uic_cntx, inst_negcoms)) == inst_user_abort) {
				amutex_unlock(p->lock);
				a1logd(p->log, 1, "k10_init_coms: user aborted\n");
				return inst_user_abort;
			}
		}
	}

	/* We haven't established comms */
	amutex_unlock(p->lock);
	a1logd(p->log, 2, "k10_init_coms: failed to establish coms\n");
	return inst_coms_fail;

  got_coms:;

	/* Check the response */
	if (ev != inst_ok) {
		amutex_unlock(p->lock);
		a1logd(p->log, 2, "k10_init_coms: status command failed\n");
		return ev;
	}

	if (strncmp (buf+2, "K-10   ", 7) == 0)
		p->model = k10_k10;
	else if (strncmp (buf+2, "K-10-A ", 7) == 0)
		p->model = k10_k10a;
	else if (strncmp (buf+2, "KV-10-A", 7) == 0)
		p->model = k10_kv10a;
	else {
		amutex_unlock(p->lock);
		a1logd(p->log, 2, "k10_init_coms: unrecognised model '%s'\n",buf);
		return inst_unknown_model;
	}

	/* Extract the serial number */
	strncpy(p->serial_no, buf+9, 9);
	p->serial_no[20] = '\000';
	
	a1logd(p->log, 2, "k10_init_coms: coms established\n");

	p->gotcoms = 1;

	amutex_unlock(p->lock);

	/* Get the list of calibrations */
	if ((ev = k10_read_cal_list(p)) != inst_ok) {
		return ev;
	}

	a1logd(p->log, 2, "k10_init_coms: init coms is returning\n");
	return inst_ok;
}

/* Initialise the KLEINK10 */
/* return non-zero on an error, with dtp error code */
static inst_code
k10_init_inst(inst *pp) {
	kleink10 *p = (kleink10 *)pp;
	char mes[100];
	char buf[MAX_MES_SIZE];
	unsigned int stime;
	int se;
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "k10_init_inst: called\n");

	if (p->gotcoms == 0)
		return inst_internal_error;		/* Must establish coms before calling init */

	amutex_lock(p->lock);
	
	/* Make sure the target lights are off */
	if ((ev = k10_command(p, "L0\r", buf, MAX_MES_SIZE, NULL, 2+3, ec_ec, 0.5)) != inst_ok
		/* Strangely the L0/1 command may return irrelevant error codes... */
	 	&& (ev & inst_imask) != K10_UNKNOWN
	 	&& (ev & inst_imask) != K10_BLACK_EXCESS
		&& (ev & inst_imask) != K10_BLACK_OVERDRIVE
		&& (ev & inst_imask) != K10_BLACK_ZERO
		&& (ev & inst_imask) != K10_OVER_HIGH_RANGE
		&& (ev & inst_imask) != K10_TOP_OVER_RANGE
		&& (ev & inst_imask) != K10_BOT_UNDER_RANGE) {
#ifdef ENABLE_L01_ERROR
		amutex_unlock(p->lock);
		return ev;
#else
		a1logd(p->log, 1, "k10_init_inst: warning - L0 failed with 0x%x - ignored\n",ev);
#endif
	}
	p->lights = 0;

	/* Make sure we are auto ranging by default */
	if ((ev = k10_command(p, "J8\r", buf, MAX_MES_SIZE, NULL, 2+3, ec_ec, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	p->autor = 1;

	/* Grab the firware version */
	stime = msec_time();
	if ((ev = k10_command(p, "P2\r", buf, MAX_MES_SIZE, NULL, 2+8+3, ec_ec, 2.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}
	p->comdel = (msec_time() - stime)/2;	/* Or is this the FD232 update latency ? */
	strncpy(p->firm_ver, buf+2, 8);
	p->firm_ver[8] = '\000';

	amutex_unlock(p->lock);

	/* Set a default calibration */
	if ((ev = set_default_disp_type(p)) != inst_ok) {
		return ev;
	}

	p->inited = 1;

	/* Do a flicker read to work around glitch at the 0.4 second mark of the */
	/* first one after power up. We ignore any error. */
	if ((ev = k10_read_flicker_samples(p, 0.5, NULL, NULL, NULL, 0)) != inst_ok) {
		a1logd(p->log, 1, "k10_init_inst: warning - startup k10_read_flicker_samples failed with 0x%x - ignored\n",ev);
	}

	a1logd(p->log, 2, "k10_init_inst: instrument inited OK\n");

	if (p->log->verb) {
		char *model = "Unknown";
		switch (p->model) {
			case k10_k1:
				model = "K-1";
				break;
			case k10_k8:
				model = "K-8";
				break;
			case k10_k10:
				model = "K-10";
				break;
			case k10_k10a:
				model = "K-10A";
				break;
			case k10_kv10a:
				model = "KV-10A";
				break;
		}
		a1logv(p->log, 1, " Model:               '%s'\n",model);
		a1logv(p->log, 1, " Serial number:       '%s'\n",p->serial_no);
		a1logv(p->log, 1, " Firmware version:    '%s'\n",p->firm_ver);
	}

	return inst_ok;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Convert a Klein Measurement encoded 24 bit value to a double */
double KleinMeas2double(char *ibuf) {
	unsigned char *buf = (unsigned char *)ibuf;
	unsigned int ip;
	double op;
	int sn = 0, ma;
	int ep;

	ip = (buf[0] << 8) + buf[1];
	sn = (ip >> 15) & 0x1;
	ma = ip & ((1 << 15)-1);
	ep = buf[2];
	if (ep >= 128)
		ep -= 256; 

	op = (double)ma;
	op *= pow(2.0, (double)ep-16);
	if (sn)
		op = -op;
	return op;
}

/* Decode measurement RGB range into 3 x 1..6 */
static void decodeRange(int *out, char iin) {
	unsigned char in = (unsigned char)iin;
	int t, r0, r1, r2, r3;
	int tt;

	out[0] = (in >> 7) & 1;
	out[1] = (in >> 6) & 1;
	out[2] = (in >> 5) & 1;

	in &= 0x1F;

	out[0] += 1 + 2 * ((in / 9) % 3);
	out[1] += 1 + 2 * ((in / 3) % 3);
	out[2] += 1 + 2 * (in % 3);
}


/* Convert a Klein Calibration encoded 24 bit value to a double */
double KleinCal2double(char *ibuf) {
	unsigned char *buf = (unsigned char *)ibuf;
	ORD32 ip;
	double op;
	ORD32 sn = 0, ma;
	int ep;

	ip = (buf[0] << 8) + buf[1];
	sn = (ip >> 15) & 0x1;
	ma = ip & ((1 << 15)-1);
	ep = buf[2];
	if (ep >= 128)
		ep -= 256; 

	op = (double)ma;
	op *= pow(2.0, (double)ep-15);
	if (sn)
		op = -op;
	return op;
}

/* Convert a native double to an Klein Calibration encoded 24 bit value, */
void double2KleinCal(char *ibuf, double d) {
	unsigned char *buf = (unsigned char *)ibuf;
	ORD32 sn = 0, ma;
	int ep;
	double n;

	if (d < 0.0) {
		sn = 1;
		d = -d;
	}
	if (d != 0.0) {
		ep = (int)floor(log(d)/log(2.0)) + 1;

		n = pow(0.5, (double)(ep - 15));	/* Normalisation factor */

		/* If rounding would cause an overflow, adjust exponent */
		if (floor(d * n + 0.5) >= (double)(1 << 15)) {
			n *= 0.5;
			ep++;
		}

		if (ep < -128) {		/* Alow denormalised */
			ep = -128;
			n = pow(0.5, (double)(ep - 15));	/* Normalisation factor */
		}

		if (ep > 127) {	/* Saturate maximum */
			ep = 127;
			d = (double)(1 << 15)-1;
		} else {
			d *= n;
			if (d < 0.5)
				ep = 0;
		}
	} else {
		ep = 0;					/* Zero */
	}
	ma = (((ORD32)floor(d + 0.5)) & ((1 << 16)-1)) | (sn << 15);
	buf[0] = ((ma >> 8) & 0xff);
	buf[1] = (ma & 0xff);

	buf[2] = ep;
}

double CalMan2double(char *ibuf) {
	unsigned char *buf = (unsigned char *)ibuf;
	ORD64 val;

	/* Load LE into 64 bit */
	val = buf[7];
	val = ((val << 8) + (0xff & buf[6]));
	val = ((val << 8) + (0xff & buf[5]));
	val = ((val << 8) + (0xff & buf[4]));
	val = ((val << 8) + (0xff & buf[3]));
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[0]));

	return IEEE754_64todouble(val);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Decode an N5 measre command response */
static inst_code decodeN5(kleink10 *p, double *XYZ, int *range, char *buf, int blen) {
 
	if (blen < (2 + 3 * 3 + 1)) {
		a1logd(p->log, 1, "decodeN5: failed to parse '%s'\n",icoms_fix(buf));
		return inst_protocol_error;
	}

	if (XYZ != NULL) {
		XYZ[0] = KleinMeas2double(buf+2);
		XYZ[1] = KleinMeas2double(buf+5);
		XYZ[2] = KleinMeas2double(buf+8);
	}

	if (range != NULL)
		decodeRange(range, buf[11]);

	return inst_ok;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Read a given calibration matrix */
static inst_code
k10_read_cal_matrix(
kleink10 *p,
inst_disptypesel *m,	/* Matrix calibration to write */
int ix					/* Klein calibration index 1 - 96 */
) {
	inst_code ev = inst_protocol_error;
	int se;
	char cmd[3];
	char buf[MAX_MES_SIZE];
	int bread, i, j, k;

	if (!p->gotcoms)
		return inst_no_coms;

	amutex_lock(p->lock);

	/* Trigger cal matrix read */
	if ((ev = k10_command(p, "D1\r", buf, MAX_MES_SIZE, &bread, 2, ec_c, 2.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	if (buf[0] != 'D' || buf[1] != '1') {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_read_cal_matrix: didn't get echo'd commad D1\n");
		return inst_protocol_error;
	}

	/* Send the cal index and read matrix */
	cmd[0] = ix;
	cmd[1] = '\r';
	cmd[2] = '\000';

	if ((ev = k10_command(p, cmd, buf, MAX_MES_SIZE, &bread, 128+3, ec_e, 2.0)) != inst_ok) {
		amutex_unlock(p->lock);
		return ev;
	}

	if (bread < 128) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_read_cal_matrix: not enough bytes returned (%d)\n",bread);
		return inst_protocol_error;
	}

	a1logd(p->log, 6, "Cal '%s':\n",m->desc);

	/* CalMan format matrix */
	if (buf[21] == 'C') {
		for (k = 24, i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++) {
				if ((bread-k) < 8) {
					amutex_unlock(p->lock);
					return inst_protocol_error;
				}
				m->mat[i][j] = CalMan2double(buf + k);
				k += 8;
				a1logd(p->log, 6, " Mat[%d][%d] = %f\n",i,j,m->mat[i][j]);
			}
		}

	/* Klein format matrix */
	} else {
		for (k = 101, i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++) {
				if ((bread-k) < 3) {
					amutex_unlock(p->lock);
					return inst_protocol_error;
				}
				m->mat[i][j] = KleinCal2double(buf + k);
				k += 3;
				a1logd(p->log, 6, " Mat[%d][%d] = %f\n",i,j,m->mat[i][j]);
			}
		}
	}
	m->flags |= inst_dtflags_ld;		/* It's now loaded */
	amutex_unlock(p->lock);

	return inst_ok;
}

/* Guess appropriate disptype and selector letters for standard calibrations */
static void guess_disptype(inst_disptypesel *s, char *desc) {
	disptech dtype;
	disptech_info *i;
	char *sel = NULL;

	if (strcmp(desc, "Default CRT File") == 0) {
		dtype = disptech_crt;
	} else if (strcmp(desc, "Klein DLP Lux") == 0) {
		dtype = disptech_dlp;
		sel = "P";
	} else if (strcmp(desc, "Klein SMPTE C") == 0) {
		dtype = disptech_crt;
		sel = "E";
	} else if (strcmp(desc, "TVL XVM245") == 0) {		/* RGB LED LCD Video display */
		dtype = disptech_lcd_rgbled;
	} else if (strcmp(desc, "Klein LED Bk LCD") == 0) {
		dtype = disptech_lcd_rgbled;
		sel = "d";
	} else if (strcmp(desc, "Klein Plasma") == 0) {
		dtype = disptech_plasma;
	} else if (strcmp(desc, "DLP Screen") == 0) {
		dtype = disptech_dlp;
	} else if (strcmp(desc, "TVL LEM150") == 0) {		/* OLED */
		dtype = disptech_oled;
	} else if (strcmp(desc, "Sony EL OLED") == 0) {		/* OLED */
		dtype = disptech_oled;
		sel = "O";
	} else if (strcmp(desc, "Eizo CG LCD") == 0) {		/* Wide gamut IPS LCD RGB ? (or RG+P ?)*/
		dtype = disptech_lcd_rgbled_ips;
		sel = "z";
	} else if (strcmp(desc, "FSI 2461W") == 0) {		/* Wide gamut IPS ? LCD CCFL */
		dtype = disptech_lcd_ccfl_wg;
	} else if (strcmp(desc, "HP DreamColor 2") == 0) {	/* Wide gamut IPS ? LCD RG+P */
		dtype = disptech_lcd_rgledp;
	} else {
		dtype = disptech_unknown;
	}
		
	i = disptech_get_id(dtype);
	s->dtech = dtype;
	if (sel != NULL)
		strcpy(s->sel, sel);
	else
		strcpy(s->sel, i->sel);
}

/* Read the list of calibrations available */
static inst_code
k10_read_cal_list(
kleink10 *p) {
	inst_code ev = inst_protocol_error;
	char buf[MAX_RD_SIZE];
	int bread, i, j, ix, n;
	char name[21];

	if (!p->gotcoms)
		return inst_no_coms;

	/* Make sure factory matrix values is in the first entry */
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			if (i == j)
				k10_disptypesel[0].mat[i][j] = 1.0;
			else
				k10_disptypesel[0].mat[i][j] = 0.0;
		}
	}
	k10_disptypesel[0].flags |= inst_dtflags_ld;

	amutex_lock(p->lock);

	/* Grab the raw info */
	if ((ev = k10_command(p, "D7\r", buf, MAX_RD_SIZE, &bread, 1925, ec_ec, 6.0)) != inst_ok) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_read_cal_list D7 returning error 0x%x\n",ev);
		return ev;
	}

	/* Parse it. There should be 96 calibrations */
	name[20] = '\000';
	for (i = 2, ix = 1, n = 1; ix <= 96 && (bread-i) >= 20; i += 20, ix++) {

		for (j = 0; j < 20; j++)
			name[j] = buf[i + j];

		if (((unsigned char *)name)[0] == 0xff) {
#ifdef TEST_FAKE_CALIBS
 #pragma message("!!!!!!!!!!!!!!! Klein K10 TEST_FULL_CALIB set !!!!!!!!!!!!!!!!!!!")
			sprintf(name, "Fake_%d",ix);
#else
			//printf("Cal %d is 0xff - skipping\n",ix);
			continue;
#endif
		}

		/* Remove trailing spaces */
		for (j = 19; j >= 0; j--) {
			if (name[j] != ' ') {
				name[j+1] = '\000';
				break;
			}
		}

		// printf("Adding Cal %d is '%s'\n",ix,name);

		/* Add it to the list */
		memset((void *)&k10_disptypesel[n], 0, sizeof(inst_disptypesel));
		k10_disptypesel[n].flags = inst_dtflags_mtx | inst_dtflags_wr;	/* Not loaded yet */
		k10_disptypesel[n].cbid = 0;
		strcpy(k10_disptypesel[n].desc, name);
		k10_disptypesel[n].refr = 0;
		k10_disptypesel[n].ix = ix;
		guess_disptype(&k10_disptypesel[n], name);
		n++;
	}

	/* Put marker at end */
	k10_disptypesel[n].flags = inst_dtflags_end;
	k10_disptypesel[n].cbid = 0;
	k10_disptypesel[n].sel[0] = '\000';
	k10_disptypesel[n].desc[0] = '\000';
	k10_disptypesel[n].refr = 0;
	k10_disptypesel[n].ix = 0;

	amutex_unlock(p->lock);

	return inst_ok;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void abort_flicker(kleink10 *p, int isnew, double *retbuf) {
	char buf[MAX_MES_SIZE];
	int bread;

	/* Abort flicker transfer */
	k10_write(p, "N5\r", 2.0);

	/* Flush the buffer of any remaining characters. */
	k10_read(p, buf, MAX_MES_SIZE, &bread, "<0>", 3, 1.0);

	/* Return the baud rate to normal */
	if (isnew)
		k10_set_baud(p, baud_9600);

#ifdef TEST_BAUD_CHANGE
	else {
		k10_set_baud(p, baud_19200);
		k10_set_baud(p, baud_9600);
	}
#endif

	/* Clean up everything else */
	amutex_unlock(p->lock);

	if (retbuf != NULL)
		free(retbuf);
}

/* Read flicker samples */
/* Free *pvals after use */
static inst_code
k10_read_flicker_samples(
kleink10 *p,
double duration,		/* duration to take samples */
double *srate,			/* Return the sampel rate */
double **pvals,			/* Return the sample values */
int *pnsamp,			/* Return the number of samples */
int usefast				/* If nz use fast rate is possible */
) {
	int se = K10_OK;
	inst_code ev = inst_ok;
	int isnew = 0;
	double rate = 256;
	double *retbuf;
	int tsamp, nsamp;	
	char mes[4] = "JX\r";
	char buf[MAX_MES_SIZE];
	int boff, bread;
	int range[3];
	unsigned int stime;
	int derr = 0, rerr = 0;
	int i;

	stime = msec_time();

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	amutex_lock(p->lock);

#ifdef HIGH_SPEED
	/* This isn't reliable, because there is no way to ensure that */
	/* the T1 command has been sent before we change the baud rate, */
	/* and if we wait too long we will loose the measurements. */
	if (usefast && strcmp(p->firm_ver, "v01.09fh") > 0) {
		isnew = 1;			/* We can use faster T1 command */
		rate = 384;
		a1logd(p->log, 1, "k10_read_flicker: using faster T1\n");
	}
#endif /* HIGH_SPEED */

	/* Target number of samples */
	tsamp = (int)(duration * (double)rate + 0.5);

	if (tsamp < 1)
		tsamp = 1;

	a1logd(p->log, 1, "k10_read_flicker: taking %d samples\n",tsamp);

	if ((retbuf = (double *)malloc(sizeof(double) * tsamp)) == NULL) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_read_flicker: malloc of %d bytes failed\n",sizeof(double) * tsamp);
		return k10_interp_code(p, K10_INT_MALLOC);
	}

	/* Make sure the target lights are off */
	if (p->lights) {
		int se;
		if ((ev = k10_command(p, "L0\r", buf, MAX_MES_SIZE, NULL, 2+3, ec_ec, 0.5)) != inst_ok
			/* Strangely the L0/1 command mat return irrelevant error codes... */
	 		&& (ev & inst_imask) != K10_UNKNOWN
		 	&& (ev & inst_imask) != K10_BLACK_EXCESS
			&& (ev & inst_imask) != K10_BLACK_OVERDRIVE
			&& (ev & inst_imask) != K10_BLACK_ZERO
			&& (ev & inst_imask) != K10_OVER_HIGH_RANGE
			&& (ev & inst_imask) != K10_TOP_OVER_RANGE
			&& (ev & inst_imask) != K10_BOT_UNDER_RANGE) {
#ifdef ENABLE_L01_ERROR
			amutex_unlock(p->lock);
			free(retbuf);
			a1logd(p->log, 1, "k10_read_flicker: L0 failed\n");
			return ev;
#else
			a1logd(p->log, 1, "k10_read_flicker: warning - L0 failed with 0x%x - ignored\n",ev);
#endif
		}
		p->lights = 0;
	}

	/* Make sure we are auto ranging */
	if (!p->autor) {
		if ((ev = k10_command(p, "J8\r", buf, MAX_MES_SIZE, NULL, 2+3, ec_ec, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			a1logd(p->log, 1, "k10_read_flicker: J8 failed with 0x%x\n",ev);
			return ev;
		}
		p->autor = 1;
	}

	/* Take a measurement to get ranges ? */
	if ((ev = k10_command(p, "N5\r", buf, MAX_MES_SIZE, &bread, 15, ec_ec, 2.0)) != inst_ok) {
		amutex_unlock(p->lock);
		free(retbuf);
		a1logd(p->log, 1, "k10_read_flicker: N5 failed with 0x%x\n",ev);
		return ev;
	}

	if ((ev = decodeN5(p, NULL, range, buf, bread)) != inst_ok) {
		a1logd(p->log, 1, "k10_read_flicker: decodeN5 failed with 0x%x\n",ev);
		amutex_unlock(p->lock);
		return ev;
	}

	/* Set a fixed range to avoid a range change error */
	p->autor = 0;
	if ((ev = k10_command(p, "J7\r", buf, MAX_MES_SIZE, NULL, 2+3, ec_ec, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_read_flicker: J7 failed with 0x%x\n",ev);
		return ev;
	}
	mes[1] = '0' + range[1];	/* Green range */
	if ((ev = k10_command(p, mes, buf, MAX_MES_SIZE, NULL, 2+3, ec_ec, 1.0)) != inst_ok) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_read_flicker: %s failed with 0x%x\n",buf,ev);
		return ev;
	}

	/* Issue an T2 for normal speed flicker measure, or T1 for fast */
	a1logd(p->log, 6, "k10_read_flicker: issuing T1/T2 command\n");
	if ((se = k10_write(p, isnew ? "T1\r" : "T2\r", 2.0)) != K10_OK) {
		amutex_unlock(p->lock);
		free(retbuf);
		a1logd(p->log, 1, "k10_read_flicker: T1/T2 failed with 0x%x\n",icoms2k10_err(se));
		return k10_interp_code(p, se);
	}

	stime = msec_time() - stime;
	stime -= p->comdel;

	/* Switch to 19200 baud if using fast */
	if (isnew) {
		/* Allow the T1/T2 to flow out before changing the baud rate */
		msec_sleep(2);

	 	if ((se = k10_set_baud(p, baud_19200)) != K10_OK) {
			abort_flicker(p, isnew, retbuf);
			a1logd(p->log, 1, "k10_read_flicker: T1 19200 baud failed with 0x%x\n",
			                                                     icoms2k10_err(se));
			return k10_interp_code(p, se);
		}
	}

#ifdef TEST_BAUD_CHANGE
	else {
		msec_sleep(2);
		k10_set_baud(p, baud_19200);
		k10_set_baud(p, baud_9600);
	}
#endif

	/* Capture flicker packets until we've got enough samples */
	for (boff = nsamp = 0; nsamp < tsamp; ) {
		if ((se = k10_read(p, buf + boff, MAX_MES_SIZE - boff, &bread,
			                                                NULL, 96, 2.0)) != K10_OK) {
			abort_flicker(p, isnew, retbuf);
			a1logd(p->log, 1, "k10_read_flicker: reading packet failed with 0x%x\n",icoms2k10_err(se));
			return k10_interp_code(p, se);
		}

		boff += bread;

		/* Extract the values we want */
		/* (We could get XYZ, range & error value too) */
		if (boff >= 96) {
			int trange[3];
			unsigned char *ubuf = (unsigned char *)buf;

			for (i = 0; i < 32 && nsamp < tsamp; i++, nsamp++)
				retbuf[nsamp] = ubuf[i * 3 + 1] * 256.0 + ubuf[i * 3 + 2];
		
			/* Check the error and range */
			if ((se = decodeK10err(buf[3 * 13])) != K10_OK) {
				a1logd(p->log, 1, "k10_read_flicker: decode error 0x%x\n",se);
				derr = se;

			} else {
			
				decodeRange(trange, buf[3 * 11]);
	
				if (trange[0] != range[0]
				 || trange[1] != range[1]
				 || trange[2] != range[2]) {
					a1logd(p->log, 1, "k10_read_flicker: range changed\n");
					rerr = 1;
				}
			}

			/* Shuffle any remaining bytes down */
			if (boff > 96)
				memmove(buf, buf + 96, boff - 96);
			boff -= 96;

#ifdef NEVER
			{		/* Dump */
				char xtra[32];
				double XYZ[3];
				int range[3];
				char err;
	
				for (i = 0; i < 32; i++)
					xtra[i] = buf[i * 3 + 0];
			
				adump_bytes(p->log, " ", (unsigned char *)buf, 0, 96);
				printf("Extra bytes:\n");
				adump_bytes(p->log, " ", (unsigned char *)xtra, 0, 32);

				XYZ[0] = KleinMeas2double(xtra+2);
				XYZ[1] = KleinMeas2double(xtra+5);
				XYZ[2] = KleinMeas2double(xtra+8);

				decodeRange(range, xtra[11]);
				err = xtra[13];
				printf("XYZ %f %f %f range %d %d %d err '%c'\n\n",
				XYZ[0], XYZ[1], XYZ[2], range[0], range[1], range[2],err);
			}
#endif

		}
	}

	a1logd(p->log, 6, "k10_read_flicker: read %d samples\n",nsamp);

	/* Then issue an N5 to cancel, and clean up */
	abort_flicker(p, isnew, NULL);

	if (derr != 0) {
		free(retbuf);
		a1logd(p->log, 1, "k10_read_flicker: got error 0x%x during readings\n",derr);
		return icoms2k10_err(derr);
	}

	if (rerr != 0) {
		free(retbuf);
		a1logd(p->log, 1, "k10_read_flicker: range changed during readings\n");
		return icoms2k10_err(K10_RANGE_CHANGE);
	}

#ifdef NEVER
	{		/* Plot */
		double *xx;

		xx = (double *)malloc(sizeof(double) * tsamp);
		for (i = 0; i < tsamp; i++)
			xx[i] = (double)i/(double)rate;

		do_plot(xx, retbuf, NULL, NULL, tsamp);
		free(xx);
	}
#endif

	if (pvals != NULL)
		*pvals = retbuf;
	else
		free(retbuf);
	if (pnsamp != NULL)
		*pnsamp = nsamp;
	if (srate != NULL)
		*srate = (double)rate; 

	return inst_ok;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Read a single sample */
static inst_code
k10_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	kleink10 *p = (kleink10 *)pp;
	char buf[MAX_RD_SIZE];
	int user_trig = 0;
	int bsize;
	inst_code rv = inst_protocol_error;
	int range[3];			/* Range for RGB sensor values */
	int i, tries, ntav = 1;	/* Number of readings to average */
	double v, vv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	amutex_lock(p->lock);

	if (p->trig == inst_opt_trig_user) {
		amutex_unlock(p->lock);

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "kleink10: inst_opt_trig_user but no uicallback function set!\n");
			return inst_unsupported;
		}

		for (;;) {
			if ((rv = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
				if (rv == inst_user_abort) {
					return rv;				/* Abort */
				}
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
		amutex_lock(p->lock);

	/* Progromatic Trigger */
	} else {
		/* Check for abort */
		if (p->uicallback != NULL
		 && (rv = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort) {
			amutex_unlock(p->lock);
			return rv;				/* Abort */
		}
	}

	/* Make sure the target lights are off */
	if (p->lights) {
		if ((rv = k10_command(p, "L0\r", buf, MAX_MES_SIZE, NULL, 2+3, ec_ec, 0.5)) != inst_ok
			/* Strangely the L0/1 command mat return irrelevant error codes... */
	 		&& (rv & inst_imask) != K10_UNKNOWN
		 	&& (rv & inst_imask) != K10_BLACK_EXCESS
			&& (rv & inst_imask) != K10_BLACK_OVERDRIVE
			&& (rv & inst_imask) != K10_BLACK_ZERO
			&& (rv & inst_imask) != K10_OVER_HIGH_RANGE
			&& (rv & inst_imask) != K10_TOP_OVER_RANGE
			&& (rv & inst_imask) != K10_BOT_UNDER_RANGE) {
#ifdef ENABLE_L01_ERROR
			amutex_unlock(p->lock);
			a1logd(p->log, 1, "k10_read_sample: L0 failed\n");
			return rv;
#else
			a1logd(p->log, 1, "k10_read_sample: warning - L0 failed with 0x%x - ignored\n",rv);
#endif
		}
		p->lights = 0;
	}

	/* Make sure we are auto ranging */
	if (!p->autor) {
		if ((rv = k10_command(p, "J8\r", buf, MAX_MES_SIZE, NULL, 2+3, ec_ec, 1.0)) != inst_ok) {
			amutex_unlock(p->lock);
			return rv;
		}
		p->autor = 1;
	}


	for (tries = 0; tries < RETRY_RANGE_ERROR; tries++) { 

		/* Take a measurement */
		rv = k10_command(p, "N5\r", buf, MAX_MES_SIZE, &bsize, 15, ec_ec, 2.0);

		if (rv == inst_ok
		 || (   (rv & inst_imask) != K10_TOP_OVER_RANGE  
		     && (rv & inst_imask) != K10_BOT_UNDER_RANGE))
			break;
	}

	if (rv == inst_ok)
		rv = decodeN5(p, val->XYZ, range, buf, bsize);

	if (rv == inst_ok) {
		double thr[4] = { 0.2, 2.0, 20.0, 50.0 };		/* Threshold */
		double nav[4] = {  20,  10,    4,    2 };		/* Count */

		/* Make v the largest */
		v = val->XYZ[1];
		if (val->XYZ[0] > v)
			v = val->XYZ[0];
		if (val->XYZ[2] > v)
			v = val->XYZ[2];
	
#ifdef AUTO_AVERAGE
		if (!IMODETST(p->mode, inst_mode_emis_nonadaptive)) {
			/* Decide how many extra readings to average into result. */
			/* Interpolate between the thresholds */
			if (v < 0.2) {
				ntav = nav[0];
			} else if (v < thr[1]) {
				vv = 1.0 - (v - thr[0]) / (thr[1] - thr[0]);
				vv = vv * vv * vv;
				ntav = (int)(vv * (nav[0] - 10) + 10.0 + 0.5);
			} else if (v < thr[2]) {
				vv = 1.0 - (v - thr[1]) / (thr[2] - thr[1]);
				vv = vv * vv * vv;
				ntav = (int)(vv * (nav[1] - nav[2]) + nav[2] + 0.5);
			} else if (v < thr[3]) {
				vv = 1.0 - (v - thr[2]) / (thr[3] - thr[2]);
				vv = vv * vv * vv;
				ntav = (int)(vv * (nav[2] - nav[3]) + nav[3] + 0.5);
			}	/* else default 1 */
		}
#endif

		/* Measure extras up to ntav */
		for (i = 1; i < ntav; i++) {
			double XYZ[3];

			for (tries = 0; tries < RETRY_RANGE_ERROR; tries++) { 
				rv = k10_command(p, "N5\r", buf, MAX_MES_SIZE, &bsize, 15, ec_ec, 2.0);
				if (rv == inst_ok
				 || (   (rv & inst_imask) != K10_TOP_OVER_RANGE  
				     && (rv & inst_imask) != K10_BOT_UNDER_RANGE))
					break;
			}

			if (rv != inst_ok) {	// An error, or retry failed
				break;
			}

			if ((rv = decodeN5(p, XYZ, range, buf, bsize)) != inst_ok)
				break;

			val->XYZ[0] += XYZ[0];
			val->XYZ[1] += XYZ[1];
			val->XYZ[2] += XYZ[2];
		}
	}


	if (rv != inst_ok) {
		amutex_unlock(p->lock);
		return rv;
	}

	val->XYZ[0] /= (double)ntav;
	val->XYZ[1] /= (double)ntav;
	val->XYZ[2] /= (double)ntav;


	amutex_unlock(p->lock);

	/* Apply the calibration correction matrix */
	icmMulBy3x3(val->XYZ, p->ccmat, val->XYZ);

//printf("matrix = %f %f %f\n", p->ccmat[0][0], p->ccmat[0][1], p->ccmat[0][2]);
//printf("         %f %f %f\n", p->ccmat[1][0], p->ccmat[1][1], p->ccmat[2][2]);
//printf("         %f %f %f\n", p->ccmat[2][0], p->ccmat[2][1], p->ccmat[2][2]);
//printf("XYZ = %f %f %f\n", val->XYZ[0], val->XYZ[1], val->XYZ[2]);
//printf("range = %d %d %d\n", range[0], range[1], range[2]);

	/* This may not change anything since instrument may clamp */
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);

	val->loc[0] = '\000';

	/* Check if the matrix seems to be an Ambient matrix */
	if ((p->ccmat[0][0] + p->ccmat[1][1] + p->ccmat[2][2])/3.0 > 5.0)
		val->mtype = inst_mrt_ambient;
	else
		val->mtype = inst_mrt_emission;
	val->mcond = inst_mrc_none;
	val->XYZ_v = 1;		/* These are absolute XYZ readings */
	val->sp.spec_n = 0;
	val->duration = 0.0;
	rv = inst_ok;


	if (user_trig)
		return inst_user_trig;
	return rv;
}

/* - - - - - - - - - - - - - - - - */
/*

	Determining the refresh rate for a refresh type display.

	This is easy because the sample rate of the Kleoin
	is well above the refresh rates we ant to measure.

	If there is no aparent refresh, or the refresh rate is not determinable,
	return a period of 0.0 and inst_ok;
*/

#undef FREQ_SLOW_PRECISE	/* [und] Interpolate then autocorrelate, else autc & filter */

#define NFSAMPS 450		/* Maximum number of samples to read (= 1.0sec) */
#define NFMXTIME 0.5	/* Time to take measurements over */
#define PBPMS 20		/* bins per msec */
#define PERMIN ((1000 * PBPMS)/40)	/* 40 Hz */
#define PERMAX ((1000 * PBPMS)/4)	/* 4 Hz*/
#define NPER (PERMAX - PERMIN + 1)
#define PWIDTH (8 * PBPMS)			/* 8 msec bin spread to look for peak in */
#define MAXPKS 20					/* Number of peaks to find */

static inst_code k10_imp_measure_refresh(
	kleink10 *p,
	double *ref_rate
) {
	inst_code ev;
	int i, j, k, mm;

	int nfsamps;			/* Actual samples read */
	double *samp;			/* Samples */
	double srate;			/* Sampling rate used to measure frequency */
	double rsamp;			/* Sampling time */

	double minv;			/* Minimum reading */
	double maxv;			/* Maximum reading */
	double maxt;			/* Time range */

#ifdef FREQ_SLOW_PRECISE
	int nbins;
	double *bins;			/* PBPMS sample bins */
#else
	double tcorr[NPER];		/* Temp for initial autocorrelation */
	int ntcorr[NPER];		/* Number accumulated */
#endif
	double corr[NPER];		/* Filtered correlation for each period value */
	double mincv, maxcv;	/* Max and min correlation values */
	double crange;			/* Correlation range */
	double peaks[MAXPKS];	/* Peak wavelength */
	double peakh[MAXPKS];	/* Peak heighheight */
	int npeaks;				/* Number of peaks */
	double pval;			/* Period value */
	double rfreq;			/* Computed refresh frequency for each try */
	int tix = 0;			/* try index */

	a1logd(p->log,2,"k10_imp_meas_refrate called\n");

	if (ref_rate != NULL)
		*ref_rate = 0.0;	/* Define refresh rate on error */

	rfreq = 0.0;
	npeaks = 0;			/* Number of peaks */

	if ((ev = k10_read_flicker_samples(p, NFMXTIME, &srate, &samp, &nfsamps, 1)) != inst_ok) {  
		return ev;
	} 

	rsamp = 1.0/srate;

#ifdef PLOT_REFRESH
	/* Plot the raw sensor values */
	{
		double xx[NFSAMPS];

		for (i = 0; i < nfsamps; i++)
			xx[i] = i * rsamp;
		printf("Fast scan sensor values and time (sec)\n");
		do_plot(xx, samp, NULL, NULL, nfsamps);
	}
#endif	/* PLOT_REFRESH */

	/* Locate the smallest values and maximum time */
	maxt = -1e6;
	minv = 1e20;
	maxv = -11e20;
	for (i = nfsamps-1; i >= 0; i--) {
		if (samp[i] < minv)
			minv = samp[i]; 
		if (samp[i] > maxv)
			maxv = samp[i]; 
	}
	maxt = (nfsamps-1) * rsamp;

	/* Zero offset the readings */
	for (i = nfsamps-1; i >= 0; i--)
		samp[i] -= minv;

#ifdef FREQ_SLOW_PRECISE	/* Interp then autocorrelate */

	/* Create PBPMS bins and interpolate readings into them */
	nbins = 1 + (int)(maxt * 1000.0 * PBPMS + 0.5);
	if ((bins = (double *)calloc(sizeof(double), nbins)) == NULL) {
		a1loge(p->log, inst_internal_error, "k10_imp_measure_refresh: malloc nbins %d failed\n",nbins);
		free(samp);
		return k10_interp_code(p, K10_INT_MALLOC);
	}

	/* Do the interpolation */
	for (k = 0; k < (nfsamps-1); k++) {
		int sbin, ebin;
		double ksec = k * rsamp;
		double ksecp1 = (k+1) * rsamp;
		sbin = (int)(ksec * 1000.0 * PBPMS + 0.5);
		ebin = (int)(ksecp1 * 1000.0 * PBPMS + 0.5);
		for (i = sbin; i <= ebin; i++) {
			double bl;
#if defined(__APPLE__) && defined(__POWERPC__)
			gcc_bug_fix(i);
#endif
			bl = (i - sbin)/(double)(ebin - sbin);	/* 0.0 to 1.0 */
			bins[i] = (1.0 - bl) * samp[k] + bl * samp[k+1];
		} 
	}

#ifdef NEVER

	/* Plot interpolated values */
	{
		double *xx = malloc(sizeof(double) * nbins);

		if (xx == NULL) {
			a1loge(p->log, inst_internal_error, "k10_imp_measure_refresh: malloc plot nbins %d  failed\n",nbins);
			free(samp);
			return k10_interp_code(p, K10_INT_MALLOC);
		}
		for (i = 0; i < nbins; i++)
			xx[i] = i / (double)PBPMS;			/* msec */
		printf("Interpolated fast scan sensor values and time (msec)\n");
		do_plot(xx, bins, NULL, NULL, nbins);
		free(xx);
	}
#endif /* NEVER */

	/* Compute auto-correlation at 1/PBPMS msec intervals */
	/* from 25 msec (40Hz) to 100msec (10 Hz) */
	mincv = 1e48, maxcv = -1e48;
	for (i = 0; i < NPER; i++) {
		int poff = PERMIN + i;		/* Offset to corresponding sample */

		corr[i] = 0;
		for (k = 0; (k + poff) < nbins; k++)
			corr[i] += bins[k] * bins[k + poff];
		corr[i] /= (double)k;		/* Normalize */

		if (corr[i] > maxcv)
			maxcv = corr[i];
		if (corr[i] < mincv)
			mincv = corr[i];
	}
	/* Free the bins */
	free(bins);

#else /* !FREQ_SLOW_PRECISE  Fast - autocorrellate then filter */

	/* Do point by point correllation of samples */
	for (i = 0; i < NPER; i++) {
		tcorr[i] = 0.0;
		ntcorr[i] = 0;
	}
	
	for (j = 0; j < (nfsamps-1); j++) {
	
		for (k = j+1; k < nfsamps; k++) {
			double del, cor;
			int bix;
	
			del = (k - j) * rsamp;			/* Sample time delta */ 
			bix = (int)(del * 1000.0 * PBPMS + 0.5);
			if (bix < PERMIN)
				continue;
			if (bix > PERMAX)
				break;
			bix -= PERMIN;
		
			cor = samp[j] * samp[k];
	
//printf("~1 j %d k %d, del %f bix %d cor %f\n",j,k,del,bix,cor);
			tcorr[bix] += cor;
			ntcorr[bix]++;
		} 
	}
	/* Divide out count and linearly interpolate */
	j = 0;
	for (i = 0; i < NPER; i++) {
		if (ntcorr[i] > 0) {
			tcorr[i] /= ntcorr[i];
			if ((i - j) > 1) {
				if (j == 0) {
					for (k = j; k < i; k++)
						tcorr[k] = tcorr[i];
	
				} else {		/* Linearly interpolate from last value */
					double ww = (double)i-j;
					for (k = j+1; k < i; k++) {
						double bl = (k-j)/ww;
						tcorr[k] = (1.0 - bl) * tcorr[j] + bl * tcorr[i];
					}
				}
			}
			j = i;
		}
	}
	if (j < (NPER-1)) {
		for (k = j+1; k < NPER; k++) {
			tcorr[k] = tcorr[j];
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
		do_plot(xx, y1, NULL, NULL, NPER);
	}
#endif /* PLOT_REFRESH */
	
	/* Apply a gausian filter */
#define FWIDTH 100
	{
		double gaus_[2 * FWIDTH * PBPMS + 1];
		double *gaus = &gaus_[FWIDTH * PBPMS];
		double bb = 1.0/pow(2, 5.0);
		double fw = rsamp * 1000.0;
		int ifw;
	
//printf("~1 sc = %f = %f msec\n",1.0/rsamp, fw);
//printf("~1 fw = %f, ifw = %d\n",fw,ifw);
	
		fw *= 0.9;
		ifw = (int)ceil(fw * PBPMS);
		if (ifw > FWIDTH * PBPMS)
			error("k10: Not enough space for lanczos 2 filter");
		for (j = -ifw; j <= ifw; j++) {
			double x, y;
			x = j/(PBPMS * fw);
			if (fabs(x) > 1.0)
				y = 0.0;
			else
				y = 1.0/pow(2, 5.0 * x * x) - bb;
			gaus[j] = y;
//printf("~1 gaus[%d] = %f\n",j,y);
		}
	
		for (i = 0; i < NPER; i++) {
			double sum = 0.0;
			double wght = 0.0;
			
			for (j = -ifw; j <= ifw; j++) {
				double w;
				int ix = i + j;
				if (ix < 0)
					ix = -ix;
				if (ix > (NPER-1))
					ix = 2 * NPER-1 - ix;
				w = gaus[j];
				sum += w * tcorr[ix];
				wght += w;
			}
//printf("~1 corr[%d] wgt = %f\n",i,wght);
			corr[i] = sum / wght;
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
	a1logd(p->log,3,"Correlation value range %f - %f = %f = %f%%\n",mincv, maxcv,crange, 100.0 * (maxcv-mincv)/maxcv);

#ifdef PLOT_REFRESH
	/* Plot this measuremnts auto correlation */
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

#define PFDB 4	// normally debug level 4
	/* If there is sufficient level and distict correlations */
	if (crange/maxcv >= 0.1) {

		a1logd(p->log,PFDB,"Searching for peaks\n");

		/* Locate all the peaks starting at the longest correllation */
		for (i = (NPER-1-PWIDTH); i >= 0 && npeaks < MAXPKS; i--) {
			double v1, v2, v3;
			v1 = corr[i];
			v2 = corr[i + PWIDTH/2];	/* Peak */
			v3 = corr[i + PWIDTH];

			if (fabs(v3 - v1)/crange < 0.05
			 && (v2 - v1)/crange > 0.025
			 && (v2 - v3)/crange > 0.025
			 && (v2 - mincv)/crange > 0.5) {
				double pkv;			/* Peak value */
				int pki;			/* Peak index */
				double ii, bl;

#ifdef PLOT_REFRESH
				a1logd(p->log,PFDB,"Max between %f and %f msec\n",
				       (i + PERMIN)/(double)PBPMS,(i + PWIDTH + PERMIN)/(double)PBPMS);
#endif

				/* Locate the actual peak */
				pkv = -1.0;
				pki = 0;
				for (j = i; j < (i + PWIDTH); j++) {
					if (corr[j] > pkv) {
						pkv = corr[j];
						pki = j;
					}
				}
#ifdef PLOT_REFRESH
				a1logd(p->log,PFDB,"Peak is at %f msec, %f corr\n", (pki + PERMIN)/(double)PBPMS, pkv);
#endif

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
#ifdef PLOT_REFRESH
				a1logd(p->log,PFDB,"Interpolated peak is at %f msec\n", pval);
#endif
				peaks[npeaks] = pval;
				peakh[npeaks] = corr[pki];
				npeaks++;

				i -= PWIDTH;
			}
#ifdef NEVER
			if (v2 > v1 && v2 > v3) {
				printf("Peak rejected:\n");
				printf("(v3 - v1)/crange = %f < 0.05 ?\n",fabs(v3 - v1)/crange);
				printf("(v2 - v1)/crange = %f > 0.025 ?\n",(v2 - v1)/crange);
				printf("(v2 - v3)/crange = %f > 0.025 ?\n",(v2 - v3)/crange);
				printf("(v2 - mincv)/crange = %f > 0.5 ?\n",(v2 - mincv)/crange);
			}
#endif
		}
		a1logd(p->log,3,"Number of peaks located = %d\n",npeaks);

	} else {
		a1logd(p->log,3,"All rejected, crange/maxcv = %f < 0.06\n",crange/maxcv);
	}
#undef PFDB

	a1logd(p->log,3,"Number of peaks located = %d\n",npeaks);

	if (npeaks > 1) {		/* Compute aparent refresh rate */
		int nfails;
		double div, avg, ano;

		/* Try and locate a common divisor amongst all the peaks. */
		/* This is likely to be the underlying refresh rate. */
		for (k = 0; k < npeaks; k++) {
			for (j = 1; j < 25; j++) {
				avg = ano = 0.0;
				div = peaks[k]/(double)j;
				if (div < 5.0)
					continue;		/* Skip anything higher than 200Hz */ 
//printf("~1 trying %f Hz\n",1000.0/div);
				for (nfails = i = 0; i < npeaks; i++) {
					double rem, cnt;

					rem = peaks[i]/div;
					cnt = floor(rem + 0.5);
					rem = fabs(rem - cnt);

#ifdef PLOT_REFRESH
					a1logd(p->log, 3, "remainder for peak %d = %f\n",i,rem);
#endif
					if (rem > 0.06) {
						if (++nfails > 2)
							break;				/* Fail this divisor */
					} else {
						avg += peaks[i];		/* Already weighted by cnt */
						ano += cnt;
					}
				}

				if (nfails == 0 || (nfails <= 2 && npeaks >= 6))
					break;		/* Sucess */
				/* else go and try a different divisor */
			}
			if (j < 25)
				break;			/* Success - found common divisor */
		}
		if (k >= npeaks) {
			a1logd(p->log,3,"Failed to locate common divisor\n");
		
		} else {
			pval = 1000.0 * ano/avg;
			if (pval > srate) {
				a1logd(p->log,3,"Discarding frequency %f > sample rate %f\n",pval, srate);
			} else {
				rfreq = pval;
				a1logd(p->log,3,"Located frequency %f sum %f dif %f\n",pval, pval + srate, fabs(pval - srate));
				tix++;
			}
		}
	}

	if (tix) {

		/* The Klein samples so fast, we don't have to deal with */
		/* sub Nyquist aliases. */

		if (ref_rate != NULL)
			*ref_rate = rfreq;
	
		/* Error against my 85Hz CRT - GWG */
		a1logd(p->log, 1, "Refresh rate %f Hz, error = %.4f%%\n",rfreq,100.0 * fabs(rfreq - 85.0)/(85.0));
		free(samp);
		return k10_interp_code(p, K10_OK);

	} else {
		a1logd(p->log, 3, "Refresh rate was unclear\n");
	}

	free(samp);

	return k10_interp_code(p, K10_NOREFR_FOUND); 
}
#undef NFSAMPS 
#undef PBPMS
#undef PERMIN
#undef PERMAX
#undef NPER
#undef PWIDTH

/* Read an emissive refresh rate */
static inst_code
k10_read_refrate(
inst *pp,
double *ref_rate
) {
	kleink10 *p = (kleink10 *)pp;
	char buf[MAX_MES_SIZE];
	double refrate;
	inst_code rv;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (ref_rate != NULL)
		*ref_rate = 0.0;

	if ((rv = k10_imp_measure_refresh(p, &refrate)) != inst_ok) {
		return rv;
	}

	if (refrate == 0.0)
		return inst_misread;

	if (ref_rate != NULL)
		*ref_rate = refrate;

	return inst_ok;
}

/* - - - - - - - - - - - - - - - - */
/* Measure a display update delay. It is assumed that */
/* white_stamp(init) has been called, and then a */
/* white to black change has been made to the displayed color, */
/* and this will measure the time it took for the update to */
/* be noticed by the instrument, up to 2.0 seconds. */
/* (It is assumed that white_change() will be called at the time the patch */
/* changes color.) */
/* inst_misread will be returned on failure to find a transition to black. */

#define NDSAMPS 40			/* Maximum samples */ 
#define NDMXTIME 2.0		/* Maximum time to take */

static inst_code k10_meas_delay(
inst *pp,
int *pdispmsec, 	/* Return display update delay in msec */
int *pinstmsec) {	/* Return instrument reaction time in msec */
	kleink10 *p = (kleink10 *)pp;
	inst_code ev;
	char mes[MAX_MES_SIZE];
	int bread;
	int i, j, k;
	double sutime, putime, cutime, eutime;
	struct {
		double sec;
		double xyz[3];
	} samp[NDSAMPS];
	int ndsamps;
	double stot, etot, del, thr;
	double stime, etime;
	int isdeb;
	int avgsampsp;
	int dispmsec, instmsec;

	if (pinstmsec != NULL)
		*pinstmsec = -230; 

	if (!p->gotcoms)
		return inst_no_coms;

	if (!p->inited)
		return inst_no_init;

	if (usec_time() < 0.0) {
		a1loge(p->log, inst_internal_error, "k10_imp_meas_delay: No high resolution timers\n");
		return inst_internal_error; 
	}

	/* Turn debug off so that they doesn't intefere with measurement timing */
	isdeb = p->log->debug;
	p->icom->log->debug = 0;

	/* Read the samples */
	putime = usec_time() / 1000000.0;
	amutex_lock(p->lock);
	for (i = 0; i < NDSAMPS; i++) {

		/* Take a measurement to get ranges ? */
		if ((ev = k10_command(p, "N5\r", mes, MAX_MES_SIZE, &bread, 15, ec_ec, 2.0)) != inst_ok) {
			amutex_unlock(p->lock);
			p->log->debug = isdeb;
			a1logd(p->log, 1, "k10_meas_delay: measurement failed\n");
			return ev;
		}
	
		if ((ev = decodeN5(p, samp[i].xyz, NULL, mes, bread)) != inst_ok) {
			amutex_unlock(p->lock);
			p->log->debug = isdeb;
			a1logd(p->log, 1, "k10_meas_delay: measurement decode failed\n");
			return ev;
		}

		cutime = usec_time() / 1000000.0;
//		samp[i].sec = 0.5 * (putime + cutime);	/* Mean of before and after stamp ? */
		samp[i].sec = cutime;	/* Assume took until measure was received */
//		samp[i].sec = putime;	/* Assume sampled at time triggered */
		putime = cutime;
		if (cutime > NDMXTIME)
			break; 
	}
	ndsamps = i;
	amutex_unlock(p->lock);

	/* Average sample spacing in msec */
	avgsampsp = (int)(1000.0 * (samp[i-1].sec - samp[0].sec)/(i-1.0) + 0.5);

	/* Restore debugging */
	p->log->debug = isdeb;

	if (ndsamps == 0) {
		a1logd(p->log, 1, "k10_meas_delay: No measurement samples returned in time\n");
		return inst_internal_error; 
	}

	if (p->whitestamp < 0.0) {
		a1logd(p->log, 1, "k10_meas_delay: White transition wasn't timestamped\n");
		return inst_internal_error; 
	}

	/* Set the times to be white transition relative */
	for (i = 0; i < ndsamps; i++)
		samp[i].sec -= p->whitestamp / 1000000.0;

	/* Over the first 100msec, locate the maximum value */
	stime = samp[0].sec;
	stot = -1e9;
	for (i = 0; i < ndsamps; i++) {
		if (samp[i].xyz[1] > stot)
			stot = samp[i].xyz[1];
		if ((samp[i].sec - stime) > 0.1)
			break;
	}

	/* Over the last 100msec, locate the maximum value */
	etime = samp[ndsamps-1].sec;
	etot = -1e9;
	for (i = ndsamps-1; i >= 0; i--) {
		if (samp[i].xyz[1] > etot)
			etot = samp[i].xyz[1];
		if ((etime - samp[i].sec) > 0.1)
			break;
	}

	del = etot - stot;
	thr = etot - 0.10 * del;		/* 10% of transition threshold */

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "k10_meas_delay: start tot %f end tot %f del %f, thr %f\n", stot, etot, del, thr);
#endif

#ifdef PLOT_UPDELAY
	/* Plot the raw sensor values */
	{
		double xx[NDSAMPS];
		double y1[NDSAMPS];
		double y2[NDSAMPS];
		double y3[NDSAMPS];

		for (i = 0; i < ndsamps; i++) {
			xx[i] = samp[i].sec;
			y1[i] = samp[i].xyz[0];
			y2[i] = samp[i].xyz[1];
			y3[i] = samp[i].xyz[2];
		}
		printf("Display update delay measure sensor values and time (sec)\n");
		do_plot(xx, y1, y2, y3, ndsamps);
	}
#endif

	/* Check that there has been a transition */
	if (del < (0.7 * etot)) {
		a1logd(p->log, 1, "k10_meas_delay: can't detect change from black to white\n");
		return inst_misread; 
	}

	/* Working from the start, locate the time at which the level was above the threshold */
	for (i = 0; i < (ndsamps-1); i++) {
		if (samp[i].xyz[1] > thr)
			break;
	}

	a1logd(p->log, 2, "k10_meas_delay: stoped at sample %d time %f\n",i,samp[i].sec);

	/* Compute overall delay */
	dispmsec = (int)(samp[i].sec * 1000.0 + 0.5);
	
	/* The 20 Hz filter is probably a FIR which introduces a delay in */
	/* the samples being measured, creating both a settling delay and */
	/* a look ahead. A negative inst. reaction time value will cause the */
	/* patch_delay to be extended by that amount of time. */
	/* We assume 2 samples times to settle, but round up the patch */
	/* delay conservatively. */
	instmsec = -2 * avgsampsp;

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "k10_meas_delay: raw %d & %d msec\n",dispmsec,instmsec);
#endif

	dispmsec += instmsec;		/* Account for lookahead */

	if (dispmsec < 0) 		/* This can happen if the patch generator delays it's return */
		dispmsec = 0;

	/* Round the patch delay to to next highest avgsampsp */ 
	dispmsec = (int)((1.0 + floor((double)dispmsec/(double)avgsampsp)) * avgsampsp + 0.5);

	if (pdispmsec != NULL)
		*pdispmsec = dispmsec;

	if (pinstmsec != NULL)
		*pinstmsec = instmsec;

#ifdef PLOT_UPDELAY
	a1logd(p->log, 0, "k10_meas_delay: returning %d & %d msec\n",dispmsec,instmsec);
#endif

	return inst_ok;
}
#undef NDSAMPS
#undef DINTT
#undef NDMXTIME


/* Timestamp the white patch change during meas_delay() */
static inst_code k10_white_change(
inst *pp,
int init) {
	kleink10 *p = (kleink10 *)pp;
	inst_code ev;

	if (init)
		p->whitestamp = -1.0;
	else {
		if ((p->whitestamp = usec_time()) < 0.0) {
			a1loge(p->log, inst_internal_error, "k10_wite_changeO: No high resolution timers\n");
			return inst_internal_error; 
		}
	}

	return inst_ok;
}

/* - - - - - - - - - - - - - - - - */

/* Do a black calibration */
static inst_code
k10_do_black_cal(
	kleink10 *p
) {
	inst_code ev;
	char mes[MAX_MES_SIZE];
	unsigned char *umes = (unsigned char *)mes;
	int bread;
	int i, j, k;
	int val, th1, th2;
	int bvals[6][3];		/* Black values for range 1 to 6 */
	int thermal;			/* Thermal value */

	amutex_lock(p->lock);

	/* First get the Measure Count to check that TH1 and TH2 are between 50 and 200 */
	/* (Don't know why or what these mean - something to do with temperature compensation */
	/* values not being setup ?) */
	if ((ev = k10_command(p, "M6\r", mes, MAX_MES_SIZE, &bread, 20, ec_e, 2.0)) != inst_ok) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_do_black_cal: M6 failed\n");
		return ev;
	}

	if (bread < 17) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_do_black_cal: not enough bytes returned from M6 (%d)\n",bread);
		return inst_protocol_error;
	}

	th1 = umes[14];
	th2 = umes[15];

	if (th1 < 50 || th1 > 200
	 || th2 < 50 || th2 > 200) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "th1 %d or th2 %d is out of range 50-200\n",th1,th2);
		return k10_interp_code(p, K10_BLACK_CAL_INIT);
	}

	/* Do the black calibration */
	if ((ev = k10_command(p, "B9\r", mes, MAX_MES_SIZE, &bread, 43, ec_ec, 15.0)) != inst_ok) {
		a1logd(p->log, 1, "k10_do_black_cal: B9 failed\n");
		amutex_unlock(p->lock);
		return ev;
	}

	if (bread < 40) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_do_black_cal: not enough bytes returned from B9 (%d)\n",bread);
		return inst_protocol_error;
	}

	/* Parse the black values that resulted */
	for (k = i = 0; i < 6; i++) {
		for (j = 0; j < 3; j++, k++) {
			val = umes[2 + 2 * k] * 256 + umes[2 + 2 * k + 1]; 
			if (val < 500 || val > 2500) {
				amutex_unlock(p->lock);
				a1logd(p->log, 1, "k10_do_black_cal: B9 black result value out of range\n");
				return k10_interp_code(p, K10_BLACK_CAL_FAIL);
			}
			bvals[i][j] = val;
		}
	}
	val = umes[2 + 2 * k] * 256 + umes[2 + 2 * k + 1]; 
	if (val < 500 || val > 2500) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_do_black_cal: B9 black thermal result value out of range\n");
		return k10_interp_code(p, K10_BLACK_CAL_FAIL);
	}
	thermal = val;

	if (p->log->debug >= 4) {
		for (i = 0; i < 6; i++) 
			a1logd(p->log, 4, "Black cal. Range %d XYZ = %d %d %d\n",
			             i+1, bvals[i][0], bvals[i][1], bvals[i][2]);
			a1logd(p->log, 4, "Thermal %d\n",thermal);
	}

	/* All looks well - copy into Flash ROM */
	if ((ev = k10_command(p, "B7\r", mes, MAX_MES_SIZE, &bread, 2, ec_c, 2.0)) != inst_ok) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_do_black_cal: B7 failed\n");
		return ev;
	}

	/* Send verification code and get error code*/
	if ((ev = k10_command(p, "{00000000}@%#\r", mes, MAX_MES_SIZE, &bread, 3, ec_e, 2.0)) != inst_ok) {
		amutex_unlock(p->lock);
		a1logd(p->log, 1, "k10_do_black_cal: B7 followup failed\n");
		return ev;
	}
	amutex_unlock(p->lock);

	a1logd(p->log, 4, "k10_do_black_cal: Done\n");

	return inst_ok;
}

/* - - - - - - - - - - - - - - - - */

/* Return needed and available inst_cal_type's */
static inst_code k10_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	kleink10 *p = (kleink10 *)pp;
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
		
	/* Can do a black cal, but not required */
	a_cals |= inst_calt_emis_offset;

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	return inst_ok;
}

/* Request an instrument calibration. */
inst_code k10_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
inst_calc_id_type *idtype,	/* Condition identifier type */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	kleink10 *p = (kleink10 *)pp;
	inst_code ev;
    inst_cal_type needed, available;

	if (!p->gotcoms)
		return inst_no_coms;

	if (!p->inited)
		return inst_no_init;

	*idtype = inst_calc_id_none;
	id[0] = '\000';

	if ((ev = k10_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
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

		a1logd(p->log,4,"k10_calibrate: doing calt 0x%x\n",calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return inst_ok;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return inst_unsupported;
	}

	/* Do the appropriate calibration */
	if (*calt & inst_calt_emis_offset) {

		if ((*calc & inst_calc_cond_mask) != inst_calc_man_em_dark) {
			*calc = inst_calc_man_em_dark;
			return inst_cal_setup;
		}

		/* Do black offset calibration */
		if ((ev = k10_do_black_cal(p)) != inst_ok)
			return ev;

		*calt &= ~inst_calc_man_em_dark;
	}
	return inst_ok;
}

/* Error codes interpretation */
static char *
k10_interp_error(inst *pp, int ec) {
	kleink10 *p = (kleink10 *)pp;
	ec &= inst_imask;
	switch (ec) {
		case K10_INTERNAL_ERROR:
			return "Internal software error";
		case K10_TIMEOUT:
			return "Communications timeout";
		case K10_COMS_FAIL:
			return "Communications failure";
		case K10_UNKNOWN_MODEL:
			return "Not a Klein K10";
		case K10_DATA_PARSE_ERROR:
			return "Data from kleink10 didn't parse as expected";
//		case K10_SPOS_EMIS:
//			return "Ambient filter should be removed";
//		case K10_SPOS_AMB:
//			return "Ambient filter should be used";

		case K10_OK:
			return "No device error";

		case K10_CMD_VERIFY:
			return "Instrument didn't echo command code";
		case K10_BAD_RETVAL:
			return "Unable to parse return instruction return code";

		case K10_FIRMWARE:
			return "Firmware error";

		case K10_BLACK_EXCESS:
			return "Black Excessive";
		case K10_BLACK_OVERDRIVE:
			return "Black Overdrive";
		case K10_BLACK_ZERO:
			return "Black Zero";

		case K10_OVER_HIGH_RANGE:
			return "Over High Range";
		case K10_TOP_OVER_RANGE:
			return "Top over range";
		case K10_BOT_UNDER_RANGE:
			return "Bottom under range";
		case K10_AIMING_LIGHTS:
			return "Aiming lights on when measuring";

		case K10_UNKNOWN:
			return "Unknown error from instrument";

		case K10_INT_MALLOC:
			return "Memory allocation failure";

		case K10_NOREFR_FOUND:
			return "No refresh rate detected or failed to measure it";

		case K10_NOTRANS_FOUND:
			return "No delay measurment transition found";

		case K10_RANGE_CHANGE:
			return "Range changed during measurement";

		case K10_BLACK_CAL_INIT:
			return "Instrument hasn't been setup for black calibration";
		case K10_BLACK_CAL_FAIL:
			return "Black calibration failed";

		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
k10_interp_code(kleink10 *p, int ec) {

	ec &= inst_imask;
	switch (ec) {

		case K10_OK:
			return inst_ok;

		case K10_INTERNAL_ERROR:
		case K10_AIMING_LIGHTS:
		case K10_UNKNOWN:
		case K10_INT_MALLOC:
			return inst_internal_error | ec;

		case K10_TIMEOUT:
		case K10_COMS_FAIL:
			return inst_coms_fail | ec;

		case K10_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case K10_CMD_VERIFY:
		case K10_BAD_RETVAL:
		case K10_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

		case K10_FIRMWARE:
		case K10_BLACK_EXCESS:			// ?
		case K10_BLACK_OVERDRIVE:		// ?
		case K10_BLACK_ZERO:			// ?
		case K10_BLACK_CAL_INIT:
			return inst_hardware_fail | ec;

		case K10_OVER_HIGH_RANGE:
		case K10_TOP_OVER_RANGE:
		case K10_BOT_UNDER_RANGE:
		case K10_NOREFR_FOUND:
		case K10_NOTRANS_FOUND:
		case K10_RANGE_CHANGE:
		case K10_BLACK_CAL_FAIL:
			return inst_misread | ec;

	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
k10_del(inst *pp) {
	if (pp != NULL) {
		kleink10 *p = (kleink10 *)pp;
		if (p->icom != NULL)
			p->icom->del(p->icom);
		amutex_del(p->lock);
		p->vdel(pp);
		free(p);
	}
}

/* Return the instrument mode capabilities */
static void k10_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	kleink10 *p = (kleink10 *)pp;
	inst_mode cap1 = 0;
	inst2_capability cap2 = 0;

	cap1 |= inst_mode_emis_tele
	     |  inst_mode_emis_spot
	     |  inst_mode_ambient			/* But cc matrix is up to user */
	     |  inst_mode_emis_nonadaptive
	     |  inst_mode_colorimeter
	        ;

	/* can inst2_has_sensmode, but not report it asynchronously */
	cap2 |= inst2_prog_trig
	     |  inst2_user_trig
	     |  inst2_disptype
	     |  inst2_has_target	/* Has target lights */
		 |	inst2_ccmx
	     |  inst2_emis_refr_meas
	     |  inst2_meas_disp_update
	        ;


	if (pcap1 != NULL)
		*pcap1 = cap1;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = inst3_none;
}

/* Check device measurement mode */
static inst_code k10_check_mode(inst *pp, inst_mode m) {
	inst_mode cap;

	if (!pp->gotcoms)
		return inst_no_coms;
	if (!pp->inited)
		return inst_no_init;

	pp->capabilities(pp, &cap, NULL, NULL);

	/* Simple test */
	if (m & ~cap)
		return inst_unsupported;

	if (!IMODETST(m, inst_mode_emis_spot)
	 && !IMODETST(m, inst_mode_emis_tele)
	 && !IMODETST(m, inst_mode_emis_ambient)) {
		return inst_unsupported;
	}

	return inst_ok;
}

/* Set device measurement mode */
static inst_code k10_set_mode(inst *pp, inst_mode m) {
	kleink10 *p = (kleink10 *)pp;
	int refrmode;
	inst_code ev;

	if ((ev = k10_check_mode(pp, m)) != inst_ok)
		return ev;

	p->mode = m;

	return inst_ok;
}

/* This table gets extended on initialisation */
/* There is 1 factory + 96 programmable + end marker */
static inst_disptypesel k10_disptypesel[98] = {
	{
		inst_dtflags_default | inst_dtflags_mtx,	/* flags */
		1,							/* cbid */
		"F",						/* sel */
		"Factory Default",			/* desc */
		0,							/* refr */
		disptech_unknown,			/* disptype */
		0							/* ix */
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
static inst_code k10_get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	kleink10 *p = (kleink10 *)pp;
	inst_code rv = inst_ok;

	/* Create/Re-create a current list of available display types */
	if (p->dtlist == NULL || recreate) {
		if ((rv = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
			k10_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return rv;
	}

	if (pnsels != NULL)
		*pnsels = p->ndtlist;

	if (psels != NULL)
		*psels = p->dtlist;

	return inst_ok;
}

/* Given a display type entry, setup calibration from that type */
static inst_code set_disp_type(kleink10 *p, inst_disptypesel *dentry) {

	/* If aninbuilt matrix hasn't been read from the instrument, */
	/* read it now. */
	if ((dentry->flags & inst_dtflags_mtx) 
	 && (dentry->flags & inst_dtflags_ld) == 0) { 
		inst_code rv;
		if ((rv = k10_read_cal_matrix(p, dentry, dentry->ix)) != inst_ok)
			return rv;
	}

	if (dentry->flags & inst_dtflags_ccmx) {
		if (dentry->cc_cbid != 1) {
			a1loge(p->log, 1, "k10: matrix must use cbid 1!\n",dentry->cc_cbid);
			return inst_wrong_setup;
		}

		p->dtech = dentry->dtech;
		icmCpy3x3(p->ccmat, dentry->mat);
		p->cbid = 0;	/* Can't be a base type now */

	} else {
		p->dtech = dentry->dtech;
		icmCpy3x3(p->ccmat, dentry->mat);
		p->cbid = dentry->cbid;
		p->ucbid = dentry->cbid;    /* This is underying base if dentry is base selection */
	}

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

/* Setup the default display type */
static inst_code set_default_disp_type(kleink10 *p) {
	inst_code ev;
	int i;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
			k10_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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
static inst_code k10_set_disptype(inst *pp, int ix) {
	kleink10 *p = (kleink10 *)pp;
	inst_code ev;
	inst_disptypesel *dentry;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
			k10_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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

/* Get the disptech and other corresponding info for the current */
/* selected display type. Returns disptype_unknown by default. */
/* Because refrmode can be overridden, it may not match the refrmode */
/* of the dtech. (Pointers may be NULL if not needed) */
static inst_code k10_get_disptechi(
inst *pp,
disptech *dtech,
int *refrmode,
int *cbid) {
	kleink10 *p = (kleink10 *)pp;
	if (dtech != NULL)
		*dtech = p->dtech;
	if (refrmode != NULL)
		*refrmode = disptech_get_id(disptech_unknown)->refr;
	if (cbid != NULL)
		*cbid = p->cbid;
	return inst_ok;
}

/* Insert a colorimetric correction matrix in the instrument XYZ readings */
/* This is only valid for colorimetric instruments. */
/* To remove the matrix, pass NULL for the filter filename */
inst_code k10_col_cor_mat(
inst *pp,
disptech dtech,		/* Use disptech_unknown if not known */				\
int cbid,       	/* Calibration display type base ID, 1 if unknown */\
double mtx[3][3]
) {
	kleink10 *p = (kleink10 *)pp;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	/* We don't have to set the base type since the instrument always returns factory */
	if (cbid != 1) {
		a1loge(p->log, 1, "k10: matrix must use cbid 1!\n",cbid);
		return inst_wrong_setup;
	}

	if (mtx == NULL) {
		icmSetUnity3x3(p->ccmat);
	} else {
		icmCpy3x3(p->ccmat, mtx);
	}

	p->dtech = dtech;
	p->cbid = 0;

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

/* 
 * set or reset an optional mode
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 */
static inst_code
k10_get_set_opt(inst *pp, inst_opt_type m, ...) {
	kleink10 *p = (kleink10 *)pp;
	char buf[MAX_MES_SIZE];
	int se;

	a1logd(p->log, 5, "k10_get_set_opt: opt type 0x%x\n",m);

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

	/* Get target light state */
	if (m == inst_opt_get_target_state) {
		va_list args;
		int *pstate, lstate = 0;

		va_start(args, m);
		pstate = va_arg(args, int *);
		va_end(args);

		if (pstate != NULL)
			*pstate = p->lights;

		return inst_ok;

	/* Set target light state */
	} else if (m == inst_opt_set_target_state) {
		inst_code ev;
		va_list args;
		int state = 0;

		va_start(args, m);
		state = va_arg(args, int);
		va_end(args);

		amutex_lock(p->lock);

		if (state == 2) { 		/* Toggle */
			if (p->lights)
				state = 0;
			else
				state = 1;
		}

		if (state == 1) {		/* Turn on */ 
			if ((ev = k10_command(p, "L1\r", buf, MAX_MES_SIZE, NULL, 2+3, ec_ec, 0.5)) != inst_ok
				/* Strangely the L0/1 command mat return irrelevant error codes... */
		 		&& (ev & inst_imask) != K10_UNKNOWN
			 	&& (ev & inst_imask) != K10_BLACK_EXCESS
				&& (ev & inst_imask) != K10_BLACK_OVERDRIVE
				&& (ev & inst_imask) != K10_BLACK_ZERO
				&& (ev & inst_imask) != K10_OVER_HIGH_RANGE
				&& (ev & inst_imask) != K10_TOP_OVER_RANGE
				&& (ev & inst_imask) != K10_BOT_UNDER_RANGE) {
#ifdef ENABLE_L01_ERROR
				amutex_unlock(p->lock);
				a1logd(p->log, 1, "k10_get_set_opt: L1 failed\n");
				return ev;
#else
				a1logd(p->log, 1, "k10_get_set_opt: warning - L1 failed with 0x%x - ignored\n",ev);
#endif
			}
			p->lights = 1;
		} else if (state == 0) {	/* Turn off */
			if ((ev = k10_command(p, "L0\r", buf, MAX_MES_SIZE, NULL, 2+3, ec_ec, 0.5)) != inst_ok
				/* Strangely the L0/1 command mat return irrelevant error codes... */
		 		&& (ev & inst_imask) != K10_UNKNOWN
			 	&& (ev & inst_imask) != K10_BLACK_EXCESS
				&& (ev & inst_imask) != K10_BLACK_OVERDRIVE
				&& (ev & inst_imask) != K10_BLACK_ZERO
				&& (ev & inst_imask) != K10_OVER_HIGH_RANGE
				&& (ev & inst_imask) != K10_TOP_OVER_RANGE
				&& (ev & inst_imask) != K10_BOT_UNDER_RANGE) {
#ifdef ENABLE_L01_ERROR
				amutex_unlock(p->lock);
				a1logd(p->log, 1, "k10_get_set_opt: L0 failed\n");
				return ev;
#else
				a1logd(p->log, 1, "k10_get_set_opt: warning - L0 failed with 0x%x - ignored\n",ev);
#endif
			}
			p->lights = 0;
		}
		amutex_unlock(p->lock);
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
extern kleink10 *new_kleink10(icoms *icom, instType dtype) {
	kleink10 *p;
	if ((p = (kleink10 *)calloc(sizeof(kleink10),1)) == NULL) {
		a1loge(icom->log, 1, "new_kleink10: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	p->init_coms         = k10_init_coms;
	p->init_inst         = k10_init_inst;
	p->capabilities      = k10_capabilities;
	p->check_mode        = k10_check_mode;
	p->set_mode          = k10_set_mode;
	p->get_disptypesel   = k10_get_disptypesel;
	p->set_disptype      = k10_set_disptype;
	p->get_disptechi     = k10_get_disptechi;
	p->get_set_opt       = k10_get_set_opt;
	p->read_sample       = k10_read_sample;
	p->read_refrate      = k10_read_refrate;
	p->get_n_a_cals      = k10_get_n_a_cals;
	p->calibrate         = k10_calibrate;
	p->col_cor_mat       = k10_col_cor_mat;
	p->meas_delay        = k10_meas_delay;
	p->white_change      = k10_white_change;
	p->interp_error      = k10_interp_error;
	p->del               = k10_del;

	p->icom = icom;
	p->dtype = dtype;
	p->dtech = disptech_unknown;

	amutex_init(p->lock);

	/* Attempt to get the calibration list */
	k10_init_coms((inst *)p, baud_nc, fc_nc, 0.0);

	return p;
}

