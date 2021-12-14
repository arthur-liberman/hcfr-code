
/* 
 * Argyll Color Management System
 *
 * Datacolor Spyder X related software.
 *
 * Author: Graeme W. Gill
 * Date:   17/9/2007
 *
 * Copyright 2006 - 2019, Graeme W. Gill
 * All rights reserved.
 *
 * (Based on spyd2.c)
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
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

/* TTBD:

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
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "rspec.h"
#include "spydX.h"

#define ENABLE_NONVCAL /* [Def] Enable saving calibration state between program runs in a file */
#define DCALTOUT (30 * 60) /* [30 Minutes] Dark Calibration timeout in seconds */

static inst_code spydX_interp_code(inst *pp, int ec);
static int spydX_save_calibration(spydX *p);
static int spydX_restore_calibration(spydX *p);
static int spydX_touch_calibration(spydX *p);
 

/* ------------------------------------------------------------------------ */
/* Implementation */

/* Interpret an icoms error into a SPYDX error */
static int icoms2spydX_err(int se) {
	if (se != ICOM_OK)
		return SPYDX_COMS_FAIL;
	return SPYDX_OK;
}

/* ============================================================ */
/* Low level commands */


#define BUF_SIZE 1024

/* USB Instrument commands */

/* Reset the instrument */
static inst_code
spydX_reset(
	spydX *p
) {
	int se;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX_reset: called\n");

	se = p->icom->usb_control(p->icom,
	               IUSB_REQ_TYPE_VENDOR | IUSB_REQ_RECIP_INTERFACE,
                   0x02, 2, 0, NULL, 0, NULL, 5.0);

	if (se == ICOM_OK) {
		a1logd(p->log, 6, "spydX_reset: complete, ICOM code 0x%x\n",se);
	}

	msec_sleep(500);	// ~~~

	return rv;
}

/* Execute a command */
static spydX_code
spydX_command(
	spydX *p,
	int cmd,				/* Command code */
	unsigned char *send,	/* Send buffer */
	unsigned int s_size,	/* Send size */
	unsigned char *reply,	/* Reply buffer */
	unsigned int r_size,	/* Reply size */
	int chsum,				/* nz to check checksum */
	double to				/* Timeout in seconds */
) {
	ORD8 buf[BUF_SIZE];
	unsigned int nonce, chnonce;
	int xfrd;				/* Bytes transferred */
	unsigned int iec;		/* Instrument error code */
	int xrlen;				/* Expected recieve length */
	int se;

	if ((s_size + 5) > BUF_SIZE
	 || (r_size + 5) > BUF_SIZE)
		error("USB buffer size too small in '%s' line %d\n",__FILE__,__LINE__);
	
	nonce = 0xffff & rand32(0);

	/* Setup send command */
	buf[0] = (unsigned char)cmd;
	write_ORD16_be(buf+1, nonce);
	write_ORD16_be(buf+3, s_size);
	if (s_size > 0)
		memcpy(buf+5, send, s_size);
	
	if (p->log->debug >= 7) {
		a1logd(p->log, 1, "sending:\n");
		adump_bytes(p->log, "  ", buf, 0, 5 + s_size);
	}

	se = p->icom->usb_write(p->icom, NULL, 0x01, buf, 5 + s_size, &xfrd, to);
	
	if (se != 0) {
		a1logd(p->log, 1, "spydX_command: Command send failed with ICOM err 0x%x\n",se);

		/* Flush any response */
		p->icom->usb_read(p->icom, NULL, 0x81, buf, 5 + r_size, NULL, to);

		return SPYDX_COMS_FAIL;
	}

	if (xfrd != (5 + s_size)) {
		a1logd(p->log, 1, "spydX_command: Command sent %d bytes instead of %d\n",
		                                                      xfrd, 5 + s_size);

		/* Flush any response */
		p->icom->usb_read(p->icom, NULL, 0x81, buf, 5 + r_size, NULL, to);

		return SPYDX_COMS_FAIL;
	}

	/* Read the response */
	a1logd(p->log, 5, "spydX_command: Reading response\n");

	se = p->icom->usb_read(p->icom, NULL, 0x81, buf, 5 + r_size, &xfrd, to);

	if (p->log->debug >= 7) {
		a1logd(p->log, 1, "recieved:\n");
		adump_bytes(p->log, "  ", buf, 0, xfrd);
	}

	if (se != 0) {
		a1logd(p->log, 1, "spydX_command: response read failed with ICOM err 0x%x\n",se);

		return SPYDX_COMS_FAIL;
	}

	if (xfrd != (5 + r_size)) {
		a1logd(p->log, 1, "spydX_command: Command got %d bytes instead of %d\n",
		                                                      xfrd, 5 + r_size);
		return SPYDX_COMS_FAIL;
	}

	/* Check instrument error code */
	if ((iec = read_ORD16_be(buf + 2)) != 0) {
		a1logd(p->log, 1, "spydX_command: Got instrument error %d\n",iec);
		return SPYDX_COMS_FAIL;
	}

	/* Check nonce */
	if ((chnonce = read_ORD16_be(buf + 0)) != nonce) {
		a1logd(p->log, 1, "spydX_command: Nonce mismatch got 0x%x expect 0x%x\n",chnonce,nonce);
		return SPYDX_COMS_FAIL;
	}

	/* Check expected data length */
	if ((xrlen = read_ORD16_be(buf + 3)) != r_size) {
		a1logd(p->log, 1, "spydX_command: Reply payload len %d but expect %d\n",xrlen,r_size);
		return SPYDX_COMS_FAIL;
	}

	if (chsum) {
		unsigned int sum = 0, i;
		for (i = 0; i < (r_size-1); i++)
			sum += buf[5 + i];

		sum &= 0xff;
		if (sum != buf[5 + i]) {
			a1logd(p->log, 1, "spydX_command: Checksum failed, is 0x%x should be 0x%x\n",sum,buf[5 + i]);
			return SPYDX_COMS_FAIL;
		}
	}

	/* Return payload */
	if (r_size > 0)
		memcpy(reply, buf + 5, r_size);

	return SPYDX_OK;
}

/* Get the HW version number */
static inst_code
spydX_getHWverno(
	spydX *p,
	unsigned int hwvn[2]	/* major, minor */
) {
	ORD8 reply[0x17], buf[3];
	int se = ICOM_OK;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX_getHWverno: called\n");

	se = spydX_command(p, 0xD9, NULL, 0, reply, 0x17, 0, 5.0);  

	if (se != SPYDX_OK) {
		rv = spydX_interp_code((inst *)p, icoms2spydX_err(se));
		a1logd(p->log, 6, "spydX_getHWverno: failed with ICOM code 0x%x\n",rv);
		return rv;
	}

	/* Major no */
	buf[0] = reply[0];
	buf[1] = '\000';
	hwvn[0] = atoi((char *)buf);

	/* Minor no */
	buf[0] = reply[2];
	buf[1] = reply[3];
	buf[2] = '\000';
	hwvn[1] = atoi((char *)buf);

	a1logd(p->log, 3, "spydX_getHWverno got '%d.%02d'\n",hwvn[0],hwvn[1]);

	return rv;
}

/* Get the Serial number */
static inst_code
spydX_getSerNo(
	spydX *p,
	char serno[9]		/* Length 8 string with nul */
) {
	ORD8 reply[0x25];
	int se = ICOM_OK;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX_getSerNo: called\n");

	se = spydX_command(p, 0xC2, NULL, 0, reply, 0x25, 0, 5.0);  

	if (se != SPYDX_OK) {
		rv = spydX_interp_code((inst *)p, icoms2spydX_err(se));
		a1logd(p->log, 6, "spydX_getSerNo: failed with ICOM code 0x%x\n",rv);
		return rv;
	}

	memcpy(serno, reply + 4, 8);
	serno[8] = '\000';

	a1logd(p->log, 3, "spydX_getSerNo got '%s'\n",serno);

	return rv;
}

/* Get a calibration */
static inst_code
spydX_getCalibration(
	spydX *p,
	double mat[3][3],		/* return matrix */
	int *pv1,				/* Return v1 */
	int *pv2,				/* Return v2 */
	int *pv3,				/* Return v3 */
	int cix					/* Calibration index to get */
) {
	int i, j, k;
	ORD8 send[0x1];
	ORD8 reply[0x2A];
	int v0, v1, v2, v3;
	int se = ICOM_OK;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX_getCalibration %d: called\n",cix);

	send[0] = cix;

	se = spydX_command(p, 0xCB, send, 1, reply, 0x2A, 1, 5.0);  

	if (se != SPYDX_OK) {
		rv = spydX_interp_code((inst *)p, icoms2spydX_err(se));
		a1logd(p->log, 6, "spydX_getCalibration: failed with ICOM code 0x%x\n",rv);
		return rv;
	}

	v0 = read_ORD8(reply + 0);		/* Confirm cix */

	if (v0 != cix) {
		rv = spydX_interp_code((inst *)p, SPYDX_CIX_MISMATCH);
		a1logd(p->log, 6, "spydX_getCalibration cix mismatch: set %d got %d\n",cix,v0);
		return rv;
	}

	v1 = read_ORD8(reply + 1);		/* Magic 8 bit value fed to setup command */
	v2 = read_ORD16_be(reply + 2);	/* Magic 16 bit value fed to measure command */

	for (k = i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++, k++) {
			ORD32 vv = read_ORD32_le(reply + 4 + k * 4);
			mat[i][j] = IEEE754todouble(vv);
		}
	}
	v3 = read_ORD8(reply + 0x28);	/* Magic 8 bit value unused */

	*pv1 = v1;
	*pv2 = v2;
	*pv3 = v3;

	if (p->log->debug >= 3) {
		a1logd(p->log, 3, "spydX_getCalibration got v1 = %d, v2 = %d, v3 =  %d\n",v1,v2,v3);
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				a1logd(p->log, 3, " Mat[%d][%d] = %f\n",i,j,mat[i][j]);
	}

	return rv;
}

/* Do measurement setup. */
/* This is used before the measurement command. */
/*
	v1 value of 3 works.
	v1 values 0, 1, 2 return all 0xff values that fails checksum.
	v1 value > 3 returns 5 bytes header with error byte = 0x01
 */
static inst_code
spydX_measSettup(
	spydX *p,
	int *ps1,				/* Return s1 (same as v1 ?) */
	int s2[4],				/* Return 4 magic values need for measure (non-linearity ?)*/
	int s3[4],				/* Return 4 black subtraction values */
	int v1					/* Magic 8bit value from get_mtx - gain selector ? */
) {
	int i;
	ORD8 send[0x1];
	ORD8 reply[0xA];
	int s1;
	int se = ICOM_OK;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX_measSettup %d: called\n",v1);

	send[0] = v1;

	se = spydX_command(p, 0xC3, send, 1, reply, 0xA, 1, 5.0);  

	if (se != SPYDX_OK) {
		rv = spydX_interp_code((inst *)p, icoms2spydX_err(se));
		a1logd(p->log, 6, "spydX_measSettup: failed with ICOM code 0x%x\n",rv);
		return rv;
	}

	s1 = read_ORD8(reply + 0);		/* Confirm v1 */

	if (s1 != v1) {
		rv = spydX_interp_code((inst *)p, SPYDX_CIX_MISMATCH);
		a1logd(p->log, 6, "spydX_measSettup v1 mismatch: set %d got %d\n",v1,s1);
		return rv;
	}

	*ps1 = s1;

	/* Some sort of per channel value for measurement (i.e. like integration time ?) */
	for (i = 0; i < 4; i++)
		s2[i] = read_ORD8(reply + 1 + i);

	for (i = 0; i < 4; i++)
		s3[i] = read_ORD8(reply + 5 + i);

	a1logd(p->log, 3, "spydX_measSettup got s1 = %d\n",s1);
	a1logd(p->log, 3, "  s2 = %d %d %d %d\n", s2[0], s2[1], s2[2], s2[3]);
	a1logd(p->log, 3, "  s2 = %d %d %d %d\n", s3[0], s3[1], s3[2], s3[3]);

	return rv;
}

/* Do a raw measurement. */
/*

	s1 value seems to be gain selector:
		0 = 1	
		1 = 3.7
		2 = 16
		3 = 64
	Gain ratio's don't seem to be perfect though (i.e. ~ 2% errors ??)

	v2 is integration time in msec, max value 719. 
		i.e. inttime = 2.8 * floor(v2/2.8)
	Calibrated value = 714 = 255 units = 714 msec.

	s2[3] values seem to act like a signed gain trim to an offset value ?


*/
static inst_code
spydX_Measure(
	spydX *p,
	int raw[4],				/* return 4 raw channel values, X,Y,Z,iR */
	int s2[4],				/* 4 magic trim values need for measure (non linearity ?) */
	int s1,					/* 8 bit gain setting from calibration. Same as v1 */
	int v2					/* 16 bit int time in msec from get calibration */
) {
	int i;
	ORD8 send[0x7];
	ORD8 reply[0x8];
	int se = ICOM_OK;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX_Measure v2 = %d, s1 = %d, s2 = %d %d %d %d\n",
		                                     v2,s1,s2[0],s2[1],s2[2],s2[3]);

	/* Reset the instrument to trigger an auto-zero ? */
	if ((rv = spydX_reset(p)) != inst_ok)
		return rv;

	write_ORD16_be(send + 0, v2);
	write_ORD8(send + 2, s1);
	for (i = 0; i < 4; i++)
		write_ORD8(send + 3 + i, s2[i]);

	se = spydX_command(p, 0xD2, send, 0x7, reply, 0x8, 0, 5.0);  

	if (se != SPYDX_OK) {
		rv = spydX_interp_code((inst *)p, icoms2spydX_err(se));
		a1logd(p->log, 6, "spydX_Measure: failed with ICOM code 0x%x\n",rv);
		return rv;
	}

	for (i = 0; i < 4; i++)
		raw[i] = read_ORD16_be(reply + 2 * i);

	a1logd(p->log, 3, "spydX_Measure got XYZir = %d %d %d %d\n", raw[0], raw[1], raw[2], raw[3]);

	return rv;
}


/* Measure ambient light */
/* Gain settings: 0x00 = 1.0, 0x01 = 8.0, 0x10 = 16.0, 0x11 = 120.0 */
static inst_code
spydX_AmbMeasure(
	spydX *p,
	int raw[4],			/* Return four ambient values (last two same as ap[] ?) */
	int ap[2]			/* Parameters, integration time,  gain control bits */
) {
	int i;
	ORD8 send[0x2];
	ORD8 reply[0x6];
	int se = ICOM_OK;
	inst_code rv = inst_ok;

	a1logd(p->log, 3, "spydX_AmbMeasure av = %d, %d\n", ap[0], ap[1]);

	write_ORD8(send + 0, ap[0]);
	write_ORD8(send + 1, ap[1]);

	se = spydX_command(p, 0xD4, send, 0x2, reply, 0x6, 0, 5.0);  

	if (se != SPYDX_OK) {
		rv = spydX_interp_code((inst *)p, icoms2spydX_err(se));
		a1logd(p->log, 6, "spydX_AmbMeasure: failed with ICOM code 0x%x\n",rv);
		return rv;
	}

	raw[0] = read_ORD16_be(reply + 2 * 0);
	raw[1] = read_ORD16_be(reply + 2 * 1);
	raw[2] = read_ORD8(reply + 4);				/* Returns ap[0] */
	raw[3] = read_ORD8(reply + 5);				/* Returns ap[1] */

	a1logd(p->log, 3, "spydX_AmbMeasure got raw %d %d\n", raw[0], raw[1]);

	return rv;
}

/* =================================================================== */
/* Medium level commands */

/* Do a reading. */
static inst_code
spydX_GetReading(
	spydX *p,
	double *XYZ		/* return the XYZ values */
) {
	inst_code rv = inst_ok;
	int ix = p->ix;
	int raw[4];

	/* Do measurement setup. */
	rv = spydX_measSettup(p, &p->cinfo[ix].s1, p->cinfo[ix].s2, p->cinfo[ix].s3,
	                          p->cinfo[ix].v1);
	if (rv != inst_ok)
		return rv;

	/* Do measurement */
	rv = spydX_Measure(p, raw, p->cinfo[ix].s2, p->cinfo[ix].s1, p->cinfo[ix].v2);
	if (rv != inst_ok)
		return rv;

	/* Subtract black */
	raw[0] -= (p->cinfo[ix].s3[0] + p->bcal[0]);
	raw[1] -= (p->cinfo[ix].s3[1] + p->bcal[1]);
	raw[2] -= (p->cinfo[ix].s3[2] + p->bcal[2]);

	/* Apply calibration matrix */
	XYZ[0] = p->cinfo[ix].mat[0][0] * (double)raw[0] 
	       + p->cinfo[ix].mat[0][1] * (double)raw[1]
	       + p->cinfo[ix].mat[0][2] * (double)raw[2];

	XYZ[1] = p->cinfo[ix].mat[1][0] * (double)raw[0] 
	       + p->cinfo[ix].mat[1][1] * (double)raw[1]
	       + p->cinfo[ix].mat[1][2] * (double)raw[2];

	XYZ[2] = p->cinfo[ix].mat[2][0] * (double)raw[0] 
	       + p->cinfo[ix].mat[2][1] * (double)raw[1]
	       + p->cinfo[ix].mat[2][2] * (double)raw[2];

	a1logd(p->log, 3, "spydX_GetReading: final XYZ reading %f %f %f\n",XYZ[0], XYZ[1], XYZ[2]);

	return rv;
}

/* Do an ambient reading */

/* NOTE :- the ambient sensor seem to be an AMS TSL25721. */
/* It has two sensors, one wide band and the other infra-red, */
/* the idea being to subtract them to get a rough human response. */
/* The reading is 16 bits, with 2 bits of gain and 8 bits of integration time conrtol */
static inst_code
spydX_GetAmbientReading(
	spydX *p,
	double *XYZ		/* return the ambient XYZ values */
) {
	int ap[2], raw[4];
	double amb0, amb1, gain, intt, atten;
	double cpl, lux1, lux2, tlux, amb;
	inst_code ev = inst_ok;

	a1logd(p->log, 3, "spydX_GetAmbientReading: called\n");

	/* Do measurement. */
	ap[0] = 101;	/* Integration time */
	ap[1] = 0x10;	/* Gain setting 16 */
	if ((ev = spydX_AmbMeasure(p, raw, ap)) != inst_ok)
		return ev;

	amb0 = raw[0];
	amb1 = raw[1];
	
	intt = (double)raw[2];

	switch(raw[3]) {
		case 0x00:
			gain = 1.0;
			break;
		case 0x01:
			gain = 8.0;
			break;
		case 0x10:
		default:
			gain = 16.0;
			break;
		case 0x11:
			gain = 120.0;
			break;
	}

	/* Attenuation/calbration. This is very rough, */
	/* because the SpyderX ambient sensor seems to be quite directional, */
	/* as well as having a poor  spectral characteristic, */
	/* which shouldn't be the case for a true ambient sensor. */
	atten = 44.0;

	/* Counts per lux */
	cpl = (intt * gain) / (atten * 60.0);

	lux1 = (amb0 - 1.87 * amb1)/cpl;
	lux2 = (0.63 * amb0 - amb1)/cpl;
	tlux = lux1 > lux2 ? lux1 : lux2;
	tlux = tlux < 0.0 ? 0.0 : tlux;
	
//	a1logd(p->log, 4, "spydX_GetAmbientReading: cpl %f lux1 %f lux2 %f\n",cpl, lux1, lux2);

	/* Compute the Y value */
	XYZ[1] = tlux;						/*  */
	XYZ[0] = icmD50.X * XYZ[1];			/* Convert to D50 neutral */
	XYZ[2] = icmD50.Z * XYZ[1];

	a1logd(p->log, 3, "spydX_GetAmbientReading: returning %f %f %f\n",XYZ[0],XYZ[1],XYZ[2]);

	return ev;
}

static inst_code
spydX_BlackCal(
	spydX *p
) {
	inst_code rv = inst_ok;
	int ix = p->ix;
	int raw[4];
	int i;

	/* Do measurement setup. */
	rv = spydX_measSettup(p, &p->cinfo[ix].s1, p->cinfo[ix].s2, p->cinfo[ix].s3,
	                          p->cinfo[ix].v1);
	if (rv != inst_ok)
		return rv;

	/* Do measurement */
	rv = spydX_Measure(p, raw, p->cinfo[ix].s2, p->cinfo[ix].s1, p->cinfo[ix].v2);
	if (rv != inst_ok)
		return rv;

	/* Subtract black */
	for (i = 0; i < 3; i++) 
		raw[i] -= p->cinfo[ix].s3[i];

	/* New calibration values */
	for (i = 0; i < 3; i++) 
		p->bcal[i] = raw[i];

	a1logd(p->log, 3, "spydX_BlackCal: offsets %d %d %d\n",p->bcal[0], p->bcal[1], p->bcal[2]);

	return rv;
}


/* ============================================================ */

/* Establish communications with a SPYDX */
/* If it's a serial port, use the baud rate given, and timeout in to secs */
/* Return SPYDX_COMS_FAIL on failure to establish communications */
static inst_code
spydX_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	spydX *p = (spydX *) pp;
	int se;
	icomuflags usbflags = icomuf_none;

	a1logd(p->log, 2, "spydX_init_coms: about to init coms\n");

	if (p->icom->port_type(p->icom) != icomt_usb) {
		a1logd(p->log, 1, "spydX_init_coms: wrong communications type for device!\n");
		return inst_coms_fail;
	}

	a1logd(p->log, 2, "spydX_init_coms: about to init USB\n");

//	usbflags |= icomuf_no_open_clear;
//	usbflags |= icomuf_resetep_before_read;

	/* Set config, interface, write end point, read end point */
	/* ("serial" end points aren't used - the spydX uses USB control & write/read) */
	if ((se = p->icom->set_usb_port(p->icom, 1, 0x00, 0x00, usbflags, 0, NULL)) != ICOM_OK) { 
		a1logd(p->log, 1, "spydX_init_coms: failed ICOM err 0x%x\n",se);
		return spydX_interp_code((inst *)p, icoms2spydX_err(se));
	}

	a1logd(p->log, 2, "spydX_init_coms: suceeded\n");

	p->gotcoms = 1;
	return inst_ok;
}

static inst_code set_default_disp_type(spydX *p);

/* Initialise the SPYDX */
/* return non-zero on an error, with an inst_code */
static inst_code
spydX_init_inst(inst *pp) {
	spydX *p = (spydX *)pp;
	inst_code ev = inst_ok;
	int stat;
	int i;

	a1logd(p->log, 2, "spydX_init_inst: called\n");

	if (p->gotcoms == 0) /* Must establish coms before calling init */
		return spydX_interp_code((inst *)p, SPYDX_NO_COMS);

	if (p->dtype != instSpyderX)
		return spydX_interp_code((inst *)p, SPYDX_UNKNOWN_MODEL);

	/* Reset the instrument */
	if ((ev = spydX_reset(p)) != inst_ok)
		return ev;

	/* Get HW version */
	if ((ev = spydX_getHWverno(p, p->hwvn)) != inst_ok)
		return ev;

	/* Get serial no */
	if ((ev = spydX_getSerNo(p, p->serno)) != inst_ok)
		return ev;

	/* Set a default calibration */
	if ((ev = set_default_disp_type(p)) != inst_ok) {
		return ev;
	}

	p->lo_secs = 2000000000;			/* A very long time */

#ifdef ENABLE_NONVCAL
	/* Restore the all modes calibration from the local system */
	spydX_restore_calibration(p);

	/* Touch it so that we know when the instrument was last opened */
	spydX_touch_calibration(p);
#endif

	/* Do an ambient measurement to initialize it */
	{
		int ap[2] = { 101, 0x10 }, raw[4];
		spydX_AmbMeasure(p, raw, ap);
	}

	p->trig = inst_opt_trig_user;		/* default trigger mode */

	p->inited = 1;
	a1logd(p->log, 2, "spydX_init_inst: inited OK\n");

	a1logv(p->log, 1, "Instrument Type:   %s\n"
		              "Serial Number:     %s\n"
		              "Hardware version:  %d.%02d\n"
	                  ,inst_name(p->dtype) ,p->serno ,p->hwvn[0],p->hwvn[1]);


	return inst_ok;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
spydX_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		 /* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	spydX *p = (spydX *)pp;
	int user_trig = 0;
	inst_code ev = inst_protocol_error;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->trig == inst_opt_trig_user) {

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "spydX: inst_opt_trig_user but no uicallback function set!\n");
			return inst_unsupported;
		}

		for (;;) {
			if ((ev = p->uicallback(p->uic_cntx, inst_armed)) != inst_ok) {
				if (ev == inst_user_abort)
					return ev;				/* Abort */
				if (ev == inst_user_trig) {
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
		 && (ev = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort) {
			return ev;				/* Abort */
		}
	}

	if (IMODETST(p->mode, inst_mode_emis_ambient)) {
		ev = spydX_GetAmbientReading(p, val->XYZ);

	} else {
		ev = inst_ok;

		/* Read the XYZ value */
		if ((ev = spydX_GetReading(p, val->XYZ)) == inst_ok) {

			/* Apply the colorimeter correction matrix */
			icmMulBy3x3(val->XYZ, p->ccmat, val->XYZ);
		}
	}

	if (ev != inst_ok)
		return ev;


	/* This may not change anything since instrument may clamp */
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);

	val->loc[0] = '\000';
	if (IMODETST(p->mode, inst_mode_emis_ambient))
		val->mtype = inst_mrt_ambient;
	else
		val->mtype = inst_mrt_emission;
	val->mcond = inst_mrc_none;
	val->XYZ_v = 1;		/* These are absolute XYZ readings ? */
	val->sp.spec_n = 0;
	val->duration = 0.0;


	if (user_trig)
		return inst_user_trig;
	return ev;
}

static inst_code set_base_disp_type(spydX *p, int cbid);

/* Insert a colorimetric correction matrix in the instrument XYZ readings */
/* This is only valid for colorimetric instruments. */
/* To remove the matrix, pass NULL for the filter filename */
inst_code spydX_col_cor_mat(
inst *pp,
disptech dtech,		/* Use disptech_unknown if not known */				\
int cbid,       	/* Calibration display type base ID, 1 if unknown */\
double mtx[3][3]
) {
	spydX *p = (spydX *)pp;
	inst_code ev = inst_ok;

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
	p->cbid = 0;	/* Can't be base type now */

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

	return ev;
}

/* Return needed and available inst_cal_type's */
static inst_code spydX_get_n_a_cals(inst *pp, inst_cal_type *pn_cals, inst_cal_type *pa_cals) {
	spydX *p = (spydX *)pp;
	time_t curtime = time(NULL);
	inst_cal_type n_cals = inst_calt_none;
	inst_cal_type a_cals = inst_calt_none;
	
	if ((curtime - p->bdate) > DCALTOUT) {
		a1logd(p->log,2,"Invalidating black cal as %d secs from last cal\n",curtime - p->bdate);
		p->bcal_done = 0;
	}
		
	if (!IMODETST(p->mode, inst_mode_emis_ambient)) {
		if (!p->bcal_done || !p->noinitcalib)
			n_cals |= inst_calt_emis_offset;
		a_cals |= inst_calt_emis_offset;
	}

	if (pn_cals != NULL)
		*pn_cals = n_cals;

	if (pa_cals != NULL)
		*pa_cals = a_cals;

	return inst_ok;
}

/* Request an instrument calibration. */
inst_code spydX_calibrate(
inst *pp,
inst_cal_type *calt,	/* Calibration type to do/remaining */
inst_cal_cond *calc,	/* Current condition/desired condition */
inst_calc_id_type *idtype,	/* Condition identifier type */
char id[CALIDLEN]		/* Condition identifier (ie. white reference ID) */
) {
	spydX *p = (spydX *)pp;
    inst_cal_type needed, available;
	inst_code ev;
	int ec;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if ((ev = spydX_get_n_a_cals((inst *)p, &needed, &available)) != inst_ok)
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

		a1logd(p->log,4,"spydX_calibrate: doing calt 0x%x\n",calt);

		if ((*calt & inst_calt_n_dfrble_mask) == 0)		/* Nothing todo */
			return inst_ok;
	}

	/* See if it's a calibration we understand */
	if (*calt & ~available & inst_calt_all_mask) { 
		return inst_unsupported;
	}

	/* Black calibration: */
	if (*calt & inst_calt_emis_offset) {
		time_t cdate = time(NULL);

		if ((*calc & inst_calc_cond_mask) != inst_calc_man_em_dark) {
			*calc = inst_calc_man_em_dark;
			return inst_cal_setup;
		}

		/* Do black offset calibration */
		if ((ev = spydX_BlackCal(p)) != inst_ok)
			return ev;
		p->bcal_done = 1;
		p->bdate = cdate;
	}

#ifdef ENABLE_NONVCAL
	/* Save the calibration to a file */
	spydX_save_calibration(p);
#endif

	return inst_ok;
}

/* Error codes interpretation */
static char *
spydX_interp_error(inst *pp, int ec) {
//	spydX *p = (spydX *)pp;
	ec &= inst_imask;
	switch (ec) {
		case SPYDX_INTERNAL_ERROR:
			return "Non-specific software internal software error";
		case SPYDX_COMS_FAIL:
			return "Communications failure";
		case SPYDX_UNKNOWN_MODEL:
			return "Not a Spyder 2, 3, 4 or 5";
		case SPYDX_DATA_PARSE_ERROR:
			return "Data from i1 Display didn't parse as expected";
		case SPYDX_INT_CAL_SAVE:
			return "Saving calibration file failed";
		case SPYDX_INT_CAL_RESTORE:
			return "Restoring calibration file failed";
		case SPYDX_INT_CAL_TOUCH:
			return "Touching calibration file failed";

		case SPYDX_OK:
			return "No device error";

		/* device specific errors */
		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
spydX_interp_code(inst *pp, int ec) {
//	spydX *p = (spydX *)pp;

	ec &= inst_imask;
	switch (ec) {

		case SPYDX_OK:
			return inst_ok;


		case SPYDX_INTERNAL_ERROR:
			return inst_internal_error | ec;

		case SPYDX_COMS_FAIL:
		case SPYDX_DATA_PARSE_ERROR:
			return inst_coms_fail | ec;

		case SPYDX_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

//			return inst_protocol_error | ec;
//			return inst_hardware_fail | ec;
//			return inst_wrong_setup | ec;
//			return inst_misread | ec;

	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
spydX_del(inst *pp) {
	spydX *p = (spydX *)pp;

#ifdef ENABLE_NONVCAL
	/* Touch it so that we know when the instrument was last open */
	spydX_touch_calibration(p);
#endif

	if (p->icom != NULL)
		p->icom->del(p->icom);
	p->vdel(pp);
	free(p);
}

/* Set the noinitcalib mode */
static void spydX_set_noinitcalib(spydX *p, int v, int losecs) {

	/* Ignore disabling init calib if more than losecs since instrument was open */
	if (v && losecs != 0 && p->lo_secs >= losecs) {
		a1logd(p->log,3,"initcalib disable ignored because %d >= %d secs\n",p->lo_secs,losecs);
		return;
	}
	p->noinitcalib = v;
}

/* Return the instrument mode capabilities */
static void spydX_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	spydX *p = (spydX *)pp;
	inst_mode cap1 = 0;
	inst2_capability cap2 = 0;

	cap1 |= inst_mode_emis_spot
	     |  inst_mode_colorimeter
	     |  inst_mode_emis_ambient
	        ;


	cap2 |= inst2_prog_trig
	     |  inst2_user_trig
	     |  inst2_ccmx
		 |  inst2_disptype
		 |  inst2_ambient_mono
	        ;

	if (pcap1 != NULL)
		*pcap1 = cap1;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = inst3_none;
}

/* Check device measurement mode */
static inst_code spydX_check_mode(inst *pp, inst_mode m) {
	spydX *p = (spydX *)pp;
	inst_mode cap;

	if (!p->gotcoms)
		return inst_no_coms;

	if (!p->inited)
		return inst_no_init;

	pp->capabilities(pp, &cap, NULL, NULL);

	/* Simple test */
	if (m & ~cap)
		return inst_unsupported;

	if (!IMODETST(m, inst_mode_emis_spot)
	 && !IMODETST(m, inst_mode_emis_ambient)) {
			return inst_unsupported;
	}

	return inst_ok;
}

/* Set device measurement mode */
static inst_code spydX_set_mode(inst *pp, inst_mode m) {
	spydX *p = (spydX *)pp;
	inst_code ev;

	if ((ev = spydX_check_mode(pp, m)) != inst_ok)
		return ev;

	p->mode = m;

	return inst_ok;
}

static inst_disptypesel spydX_disptypesel[SPYDX_NOCALIBS+1] = {
	{
		inst_dtflags_mtx | inst_dtflags_default,		/* flags */
		1,							/* cbid */
		"l",						/* sel */
		"General",					/* desc */
		0,							/* refr */
		disptech_lcd_ccfl,			/* disptype */
		0							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"e",						/* sel */
		"Standard LED",				/* desc */
		1,							/* refr */
		disptech_lcd_wled,			/* disptype */
		1							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"b",						/* sel */
		"Wide Gamut LED",			/* desc */
		1,							/* refr */
		disptech_lcd_rgbled,		/* disptype */
		2							/* ix */
	},
	{
		inst_dtflags_mtx,			/* flags */
		0,							/* cbid */
		"i",						/* sel */
		"GB LED",					/* desc */
		1,							/* refr */
		disptech_lcd_gbrledp,		/* disptype */
		3							/* ix */
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
static inst_code spydX_get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	spydX *p = (spydX *)pp;
	inst_code rv = inst_ok;

	/* Create/Re-create a current list of available display types. */
	if (p->dtlist == NULL || recreate) {
		if ((rv = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    spydX_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return rv;
	}

	if (pnsels != NULL)
		*pnsels = p->ndtlist;

	if (psels != NULL)
		*psels = p->dtlist;

	return inst_ok;
}

/* Given a display type entry, setup for that type */
static inst_code set_disp_type(spydX *p, inst_disptypesel *dentry) {

	/* If an inbuilt matrix hasn't been read from the instrument, */
	/* read it now. */
	if ((dentry->flags & inst_dtflags_mtx) 
	 && (dentry->flags & inst_dtflags_ld) == 0) { 
		inst_code rv;
		int ix = dentry->ix; 

		p->cinfo[ix].ix = ix;

		rv = spydX_getCalibration(p,
	                 p->cinfo[ix].mat,
	                &p->cinfo[ix].v1,
	                &p->cinfo[ix].v2,
	                &p->cinfo[ix].v3,
	                ix);

		if (rv != inst_ok)
			return rv;

		icmSetUnity3x3(dentry->mat);			/* Not used for native calibration */

		dentry->flags |= inst_dtflags_ld;		/* It's now loaded */
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

	p->ix = dentry->ix;				/* Native index */

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
static inst_code spydX_set_disptype(inst *pp, int ix) {
	spydX *p = (spydX *)pp;
	inst_code ev;
	inst_disptypesel *dentry;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (ix < 0 || ix >= p->ndtlist)
		return inst_unsupported;

	dentry = &p->dtlist[ix];

	if ((ev = set_disp_type(p, dentry)) != inst_ok) {
		return ev;
	}

	return inst_ok;
}

/* Setup the default display type */
static inst_code set_default_disp_type(spydX *p) {
	inst_code ev;
	int i;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
		    spydX_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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
static inst_code set_base_disp_type(spydX *p, int cbid) {
	inst_code ev;
	int i;

	if (cbid == 0) {
		a1loge(p->log, 1, "spydX set_base_disp_type: can't set base display type of 0\n");
		return inst_wrong_setup;
	}

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
		    spydX_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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
static inst_code spydX_get_disptechi(
inst *pp,
disptech *dtech,
int *refrmode,
int *cbid) {
	spydX *p = (spydX *)pp;
	if (dtech != NULL)
		*dtech = p->dtech;
	if (cbid != NULL)
		*cbid = p->cbid;
	return inst_ok;
}

/* 
 * set or reset an optional mode
 *
 * Some options talk to the instrument, and these will
 * error if it hasn't been initialised.
 * [We could fix this by setting a flag and adding
 *  some extra logic in init()]
 */
static inst_code
spydX_get_set_opt(inst *pp, inst_opt_type m, ...) {
	spydX *p = (spydX *)pp;
	inst_code ev = inst_ok;

	if (m == inst_opt_initcalib) {			/* default */
		spydX_set_noinitcalib(p, 0, 0);
		return inst_ok;

	} else if (m == inst_opt_noinitcalib) {		/* Disable initial calibration */
		va_list args;
		int losecs = 0;

		va_start(args, m);
		losecs = va_arg(args, int);
		va_end(args);

		spydX_set_noinitcalib(p, 1, losecs);
		return inst_ok;
	}

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
extern spydX *new_spydX(icoms *icom, instType dtype) {
	spydX *p;
	if ((p = (spydX *)calloc(sizeof(spydX),1)) == NULL) {
		a1loge(icom->log, 1, "new_spydX: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	p->init_coms         = spydX_init_coms;
	p->init_inst         = spydX_init_inst;
	p->capabilities      = spydX_capabilities;
	p->check_mode        = spydX_check_mode;
	p->set_mode          = spydX_set_mode;
	p->get_disptypesel   = spydX_get_disptypesel;
	p->set_disptype      = spydX_set_disptype;
	p->get_disptechi     = spydX_get_disptechi;
	p->get_set_opt       = spydX_get_set_opt;
	p->read_sample       = spydX_read_sample;
	p->get_n_a_cals      = spydX_get_n_a_cals;
	p->calibrate		 = spydX_calibrate;
	p->col_cor_mat       = spydX_col_cor_mat;
	p->interp_error      = spydX_interp_error;
	p->del               = spydX_del;

	p->icom = icom;
	p->dtype = dtype;
	p->dtech = disptech_unknown;

	return p;
}


/* =============================================================================== */
/* Calibration info save/restore to file */

static int spydX_save_calibration(spydX *p) {
	int ev = SPYDX_OK;
	int i;
	char fname[100];		/* Name */
	calf x;
	int argyllversion = ARGYLL_VERSION;
	int ss;

	snprintf(fname, 99, ".spydX_%s.cal", p->serno);

	if (calf_open(&x, p->log, fname, 1)) {
		x.ef = 2;
		goto done;
	}

	ss = sizeof(spydX);

	/* Some file identification */
	calf_wints(&x, &argyllversion, 1);
	calf_wints(&x, &ss, 1);
	calf_wstrz(&x, p->serno);

	/* Save the black calibration if it's valid */
	calf_wints(&x, &p->bcal_done, 1);
	calf_wtime_ts(&x, &p->bdate, 1);
	calf_wints(&x, p->bcal, 3);

	a1logd(p->log,3,"nbytes = %d, Checkum = 0x%x\n",x.nbytes,x.chsum);
	calf_wints(&x, (int *)(&x.chsum), 1);

	if (calf_done(&x))
		x.ef = 3;

  done:;
	if (x.ef != 0) {
		a1logd(p->log,2,"Writing calibration file failed with %d\n",x.ef);
		ev = SPYDX_INT_CAL_SAVE;
	} else {
		a1logd(p->log,2,"Writing calibration file succeeded\n");
	}

	return ev;
}

/* Restore the black calibration from the local system */
static int spydX_restore_calibration(spydX *p) {
	int ev = SPYDX_OK;
	int i, j;
	char fname[100];		/* Name */
	calf x;
	int argyllversion;
	int ss, nbytes, chsum1, chsum2;
	char *serno = NULL;

	snprintf(fname, 99, ".spydX_%s.cal", p->serno);

	if (calf_open(&x, p->log, fname, 0)) {
		x.ef = 2;
		goto done;
	}

	/* Last modified time */
	p->lo_secs = x.lo_secs;

	/* Do a dumy read to check the checksum, then a real read */
	for (x.rd = 0; x.rd < 2; x.rd++) {
		calf_rewind(&x);

		/* Check the file identification */
		calf_rints2(&x, &argyllversion, 1);
		calf_rints2(&x, &ss, 1);
		calf_rstrz2(&x, &serno);

		if (x.ef != 0
		 || argyllversion != ARGYLL_VERSION
		 || ss != (sizeof(spydX))
		 || strcmp(serno, p->serno) != 0) {
			a1logd(p->log,2,"Identification didn't verify\n");
			if (x.ef == 0)
				x.ef = 4;
			goto done;
		}

		/* Read the black calibration if it's valid */
		calf_rints(&x, &p->bcal_done, 1);
		calf_rtime_ts(&x, &p->bdate, 1);
		calf_rints(&x, p->bcal, 3);

		/* Check the checksum */
		chsum1 = x.chsum;
		nbytes = x.nbytes;
		calf_rints2(&x, &chsum2, 1);
	
		if (x.ef != 0
		 || chsum1 != chsum2) {
			a1logd(p->log,2,"Checksum didn't verify, bytes %d, got 0x%x, expected 0x%x\n",nbytes,chsum1, chsum2);
			if (x.ef == 0)
				x.ef = 5;
			goto done;
		}
	}

	a1logd(p->log, 3, "Restored spydX_BlackCal: offsets %d %d %d\n",p->bcal[0], p->bcal[1], p->bcal[2]);

	a1logd(p->log,5,"spydX_restore_calibration done\n");
 done:;

	free(serno);
	if (calf_done(&x))
		x.ef = 3;

	if (x.ef != 0) {
		a1logd(p->log,2,"Reading calibration file failed with %d\n",x.ef);
		ev = SPYDX_INT_CAL_RESTORE;
	}

	return ev;
}

static int spydX_touch_calibration(spydX *p) {
	int ev = SPYDX_OK;
	char fname[100];		/* Name */
	int rv;

	snprintf(fname, 99, ".spydX_%s.cal", p->serno);

	if (calf_touch(p->log, fname)) {
		a1logd(p->log,2,"Touching calibration file time failed with\n");
		return SPYDX_INT_CAL_TOUCH;
	}

	return SPYDX_OK;
}

