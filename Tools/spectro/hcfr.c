
/* 
 * Argyll Color Correction System
 *
 * HCFR Association HCFR sensor related functions
 *
 * Author: Graeme W. Gill
 * Date:   20/1/2007
 *
 * Copyright 2013, Graeme W. Gill
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

/*
	TTBD:
		Should really switch this to be high speed serial type of coms,
		now that this is supported by inst API.

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
#else /* SALONEINSTLIB */
#include "sa_config.h"
#include "numsup.h"
#endif /* SALONEINSTLIB */
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "hcfr.h"

static inst_code hcfr_interp_code(inst *pp, int ec);

#define MAX_MES_SIZE 500		/* Maximum normal message reply size */
#define MAX_RD_SIZE 5000		/* Maximum reading messagle reply size */

/* ==================================================================== */

/* Interpret an icoms error into a HCFR error */
static int icoms2hcfr_err(int se) {
	if (se != ICOM_OK)
		return HCFR_COMS_FAIL;
	return HCFR_OK;
}

/* Do a standard command/response echange with the hcfr */
/* Return the dtp error code */
static inst_code
hcfr_command(
	hcfr *p,
	char *in,		/* In string */
	char *out,		/* Out string */
	int bsize,		/* Out buffer size */
	double to		/* Timeout in seconds */
) {
	int rv, se;

	if ((se = p->icom->write_read(p->icom, in, 0, out, bsize, NULL, "\n", 1, to)) != 0) {
		int ec;
		a1logd(p->log, 1, "hcfr_command: serial i/o failure on write_read '%s'\n",icoms_fix(in));
		return hcfr_interp_code((inst *)p, icoms2hcfr_err(se));
	}
	a1logd(p->log, 4, "hcfr_command: command '%s' returned '%s', value 0x%x\n",
	                                          icoms_fix(in), icoms_fix(out),HCFR_OK);
	return hcfr_interp_code((inst *)p, HCFR_OK);
}

/* Do a break to check coms is working */
static inst_code
hcfr_break(
	hcfr *p
) {
	int rwbytes;			/* Data bytes read or written */
	int se, rv = inst_ok;

	se = p->icom->usb_control(p->icom,
		               IUSB_ENDPOINT_OUT | IUSB_REQ_TYPE_CLASS | IUSB_REQ_RECIP_INTERFACE,
	                   0x22, 0, 0, NULL, 0, 1.0);

	rv = hcfr_interp_code((inst *)p, icoms2hcfr_err(se));

	a1logd(p->log, 4, "hcfr_break: done, ICOM err 0x%x\n",se);

	return rv;
}

/* Flush an pending messages from the device */
static inst_code
hcfr_flush(
	hcfr *p
) {
	icoms *c = p->icom;
	char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;
	int rv;

	for (rv = ICOM_OK;;) {
		rv = c->read(c, buf, MAX_MES_SIZE, NULL, NULL, 100000, 0.05);
		if (rv != ICOM_OK)
			break;				/* Expect timeout with nothing to read */
	}
	a1logd(p->log, 5, "hcfr_flush: done\n");
		
	return inst_ok;
}

/* Get and check the firmware version */
static inst_code
hcfr_get_check_version(
	hcfr *p,
	int *pmaj,
	int *pmin
) {
	char ibuf[2];
	char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;
	int maj, min;

	a1logd(p->log, 4, "hcfr_get_check_version: called\n");

	if (p->gotcoms == 0)
		return inst_internal_error;

	ibuf[0] = HCFR_GET_VERS;
	ibuf[1] = 0x00;

	if ((ev = hcfr_command(p, ibuf, buf, MAX_MES_SIZE, 1.0)) != inst_ok)
		return ev;

	if (strlen(buf) < 6) {
		a1logd(p->log, 1, "hcfr_get_check_version: version string too short\n");
		return hcfr_interp_code((inst *)p, HCFR_BAD_FIRMWARE);
	}

	if (sscanf(buf, "v%d.%d", &maj,&min) != 2) {
		a1logd(p->log, 1, "hcfr_get_check_version: version string doesn't match format\n");
		return hcfr_interp_code((inst *)p, HCFR_BAD_FIRMWARE);
	}

	if (maj != HCFR_FIRMWARE_MAJOR_VERSION || min < HCFR_FIRMWARE_MINOR_VERSION) {
		a1logd(p->log, 1, "hcfr_get_check_version: version string out of range\n");
		return hcfr_interp_code((inst *)p, HCFR_BAD_FIRMWARE);
	}

	a1logd(p->log, 4, "hcfr_get_check_version: got firmare version %d.%d\n",maj,min);
	if (pmaj != NULL)
		*pmaj = maj;
	if (pmin != NULL)
		*pmin = min;
		
	return inst_ok;
}

/* Get a raw measurement value */
static inst_code
hcfr_get_rgb(
	hcfr *p,
	double rgb[3]	/* return value */
) {
	char ibuf[2];
	char buf[MAX_MES_SIZE], *bp;
	char vbuf[4];
	inst_code ev = inst_ok;
	double mul, div;
	double vals[8];
	int onesens = 0;
	int i;

	a1logd(p->log, 3, "hcfr_get_rgb: called\n");

	if (p->gotcoms == 0)
		return inst_internal_error;

	ibuf[0] = HCFR_MEAS_RGB			/* Read RGB */
	        | HCFR_MEAS_SENS0		/* Use one sensors because it's faster */
//	        | HCFR_MEAS_SENS1
	        | HCFR_INTERLACE_0
//	        | HCFR_FAST_MEASURE
	        ;

	ibuf[1] = 0x00;

	if ((ev = hcfr_command(p, ibuf, buf, MAX_MES_SIZE, 60.0)) != inst_ok) {
		a1logd(p->log, 1, "hcfr_get_rgb: hcfr_command failed\n");
		return ev;
	}

	if (strlen(buf) < 156) {
		a1logd(p->log, 1, "hcfr_get_rgb: not enough bytes returned = expected %d, got %d\n",156,strlen(buf));
		return hcfr_interp_code((inst *)p, HCFR_BAD_READING);
	}
		
	if (strncmp(buf, "RGB_1:", 6) == 0)
		onesens = 1;
	else if (strncmp(buf, "RGB_2:", 6) != 0) {
		a1logd(p->log, 1, "hcfr_get_rgb: RGB_1 or RGB_2 not founde\n");
		return hcfr_interp_code((inst *)p, HCFR_BAD_READING);
	}

	vbuf[3] = 0x00;
	bp = buf + 6;
	
	strncpy(vbuf, bp, 3); div = (double)atoi(vbuf); bp += 3;

	strncpy(vbuf, bp, 3); mul = (double)atoi(vbuf); bp += 3;
	
	/* Compute all the possible values for 4 colors and 2 sensors */
	for (i = 0; i < 8; i++) {
		unsigned int num, den;

		strncpy(vbuf, bp, 3); den =              atoi(vbuf); bp += 3;
		strncpy(vbuf, bp, 3); den = (den << 8) + atoi(vbuf); bp += 3;
		strncpy(vbuf, bp, 3); den = (den << 8) + atoi(vbuf); bp += 3;
		strncpy(vbuf, bp, 3); den = (den << 8) + atoi(vbuf); bp += 3;

		strncpy(vbuf, bp, 3); num =              atoi(vbuf); bp += 3;
		strncpy(vbuf, bp, 3); num = (num << 8) + atoi(vbuf); bp += 3;

		if (den == 0)		/* Hmm. */
			vals[i] = -1.0;
		else
			vals[i] = 1e4 * (double)num * mul * div / (double)den;
//		a1logd(p->log, 6,"vals[%d] = %f = num %d * mul %f * div %f / den %d\n", i, vals[i], num,mul,div,den);
	}
	if (onesens) {
		rgb[0] = vals[0];
		rgb[1] = vals[1];
		rgb[2] = vals[2];
	} else {
		rgb[0] = 0.5 * (vals[0] + vals[4]);
		rgb[1] = 0.5 * (vals[1] + vals[5]);
		rgb[2] = 0.5 * (vals[2] + vals[6]);
	}
	a1logd(p->log, 3, "hcfr_get_rgb: returning value %f %f %f\n",rgb[0],rgb[1],rgb[2]);

	return inst_ok;
}

/* Compute the calibration matricies. */
/* The basic calibration data is from my particular HCFR, measured */
/* against one of my CRT and LCD displays, with the reference XYZ */
/* derived from my i1pro. */
static inst_code
hcfr_comp_matrix(
	hcfr *p
) {
	double tmat[3][3];
	double xmat[3][3];
	double itmat[3][3];

	/* CRT */

	/* Red test patch, sensor then reference */
	tmat[0][0] = 71.71880890;
	tmat[1][0] = 8.53740337;
	tmat[2][0] = 3.08216218;

	xmat[0][0] = 21.988601;
	xmat[1][0] = 12.131219;
	xmat[2][0] = 1.312786;

	/* Green test patch, sensor then reference */
	tmat[0][1] = 6.26299108; 
	tmat[1][1] = 37.49843127;
	tmat[2][1] = 15.91104086;

	xmat[0][1] = 13.677691;
	xmat[1][1] = 28.870823;
	xmat[2][1] = 5.636190;

	/* Blue test patch, sensor then reference */
	tmat[0][2] = 1.30620298; 
	tmat[1][2] = 4.62894673;
	tmat[2][2] = 27.57654019;

	xmat[0][2] = 6.387302;
	xmat[1][2] = 2.755360;
	xmat[2][2] = 33.588242;

	/* Compute the inverse */
	if (icmInverse3x3(itmat, tmat))
		return hcfr_interp_code((inst *)p, HCFR_CALIB_CALC);

	/* Multiply by target values */
	icmMul3x3_2(p->crt, xmat, itmat);

	/* LCD */

	/* Red test patch, sensor then reference */
	tmat[0][0] = 39.94356609;
	tmat[1][0] = 11.59679928;
	tmat[2][0] = 8.18430397;

	xmat[0][0] = 51.875052;
	xmat[1][0] = 30.640815;
	xmat[2][0] = 4.712397;

	/* Green test patch, sensor then reference */
	tmat[0][1] = 14.45920285;
	tmat[1][1] = 33.82116329;
	tmat[2][1] = 17.64558523;

	xmat[0][1] = 37.482638;
	xmat[1][1] = 64.670821;
	xmat[2][1] = 14.554874;

	/* Blue test patch, sensor then reference */
	tmat[0][2] = 8.29727493;
	tmat[1][2] = 17.95182031;
	tmat[2][2] = 38.20123872;

	xmat[0][2] = 25.098392;
	xmat[1][2] = 23.719352;
	xmat[2][2] = 108.134087;

	/* Compute the inverse */
	if (icmInverse3x3(itmat, tmat))
		return hcfr_interp_code((inst *)p, HCFR_CALIB_CALC);

	/* Multiply by target values */
	icmMul3x3_2(p->lcd, xmat, itmat);

	return inst_ok;
}

/* ==================================================================== */

/* Establish communications with a HCFR */
/* Return HCFR_COMS_FAIL on failure to establish communications */
static inst_code
hcfr_init_coms(inst *pp, baud_rate br, flow_control fc, double tout) {
	hcfr *p = (hcfr *) pp;
	int rsize;
	long etime;
	int bi, i, se;
	inst_code ev = inst_ok;
	icomuflags usbflags = icomuf_no_open_clear | icomuf_detach;

#if defined(UNIX_APPLE) && !defined(__ppc__)
	/* Except on Intel OS X 10.4/5 for some reasone. */
	/* It would be good if the HCFR had a better USB implementation... */
	usbflags &= ~icomuf_no_open_clear;
#endif

	a1logd(p->log, 2, "hcfr_init_coms: About to init USB\n");

	if (p->icom->port_type(p->icom) != icomt_usb) {
		a1logd(p->log, 1, "hcfr_init_coms: expect hcfr to be USB\n");
		return hcfr_interp_code((inst *)p, HCFR_UNKNOWN_MODEL);
	}

	/* Set config, interface, "Serial" write & read end points */
	/* Note if we clear halt the interface hangs */
	if ((se = p->icom->set_usb_port(p->icom, 1, 0x03, 0x83, usbflags, 0, NULL)) != ICOM_OK) { 
		a1logd(p->log, 1, "hcfr_init_coms: set_usb_port failed ICOM err 0x%x\n",se);
		return hcfr_interp_code((inst *)p, icoms2hcfr_err(se));
	}

	if ((ev = hcfr_break(p)) != inst_ok) {
		a1logd(p->log, 1, "hcfr_init_coms: break failed\n");
		return ev;
	}
	p->gotcoms = 1;

	a1logd(p->log, 2, "hcfr_init_coms: inited coms OK\n");

	return inst_ok;
}

static inst_code set_default_disp_type(hcfr *p);

/* Initialise the HCFR */
/* return non-zero on an error, with dtp error code */
static inst_code
hcfr_init_inst(inst *pp) {
	hcfr *p = (hcfr *)pp;
	static char buf[MAX_MES_SIZE];
	inst_code ev = inst_ok;

	a1logd(p->log, 2, "hcfr_init_inst: called\n");

	if (p->gotcoms == 0)
		return inst_internal_error;		/* Must establish coms before calling init */

#ifndef NEVER		/* Doesn't work with OSX ? */
//	hcfr_flush(p);
#endif

	if ((ev = hcfr_get_check_version(p, &p->maj, &p->min)) != inst_ok) {
		a1logd(p->log, 1, "hcfr_init_inst: check_version failed\n");
		return ev;
	}

	if ((ev = hcfr_comp_matrix(p)) != inst_ok) {
		return ev;
	}

	p->trig = inst_opt_trig_user;

	/* Setup the default display type */
	if ((ev = set_default_disp_type(p)) != inst_ok) {
		return ev;
	}

	p->inited = 1;
	a1logd(p->log, 2, "hcfr_init_inst: instrument inited OK\n");

	return inst_ok;
}

/* Read a single sample */
/* Return the dtp error code */
static inst_code
hcfr_read_sample(
inst *pp,
char *name,			/* Strip name (7 chars) */
ipatch *val,		/* Pointer to instrument patch value */
instClamping clamp) {		/* NZ if clamp XYZ/Lab to be +ve */
	hcfr *p = (hcfr *)pp;
	inst_code ev;
	double rgb[3];
	int user_trig = 0;

	if (!p->gotcoms)
		return inst_no_coms;
	if (!p->inited)
		return inst_no_init;

	if (p->trig == inst_opt_trig_user) {

		if (p->uicallback == NULL) {
			a1logd(p->log, 1, "hcfr: inst_opt_trig_user but no uicallback function set!\n");
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
		 && (ev = p->uicallback(p->uic_cntx, inst_armed)) == inst_user_abort)
			return ev;				/* Abort */
	}

	if ((ev = hcfr_get_rgb(p, rgb)) != inst_ok)
		return ev;

	if (p->ix == 0) {			/* LCD */
		icmMulBy3x3(val->XYZ, p->lcd, rgb);

	} else if (p->ix == 1) {	/* CRT */
		icmMulBy3x3(val->XYZ, p->crt, rgb);

	} else {					/* Raw */
		val->XYZ[0] = rgb[0];
		val->XYZ[1] = rgb[1];
		val->XYZ[2] = rgb[2];
	}

	/* Apply the colorimeter correction matrix */
	icmMulBy3x3(val->XYZ, p->ccmat, val->XYZ);

	/* This may not change anything since instrument may clamp */
	if (clamp)
		icmClamp3(val->XYZ, val->XYZ);

	val->loc[0] = '\000';
	val->mtype = inst_mrt_emission;
	val->XYZ_v = 1;		/* These are absolute XYZ readings */
	val->sp.spec_n = 0;
	val->duration = 0.0;

	if (user_trig)
		return inst_user_trig;
	return inst_ok;
}

static inst_code set_base_disp_type(hcfr *p, int cbid);

/* Insert a colorimetric correction matrix in the instrument XYZ readings */
/* This is only valid for colorimetric instruments. */
/* To remove the matrix, pass NULL for the filter filename */
static inst_code hcfr_col_cor_mat(
inst *pp,
disptech dtech,		/* Use disptech_unknown if not known */				\
int cbid,       	/* Calibration display type base ID, 1 if unknown */\
double mtx[3][3]
) {
	hcfr *p = (hcfr *)pp;
	inst_code ev;

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
	p->refrmode = disptech_get_id(dtech)->refr;
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

	return inst_ok;
}

/* Error codes interpretation */
static char *
hcfr_interp_error(inst *pp, int ec) {
//	hcfr *p = (hcfr *)pp;
	ec &= inst_imask;
	switch (ec) {
		case HCFR_INTERNAL_ERROR:
			return "Internal software error";
		case HCFR_COMS_FAIL:
			return "Communications failure";
		case HCFR_UNKNOWN_MODEL:
			return "Not a HCFR or DTP52";
		case HCFR_DATA_PARSE_ERROR:
			return "Data from DTP didn't parse as expected";

		case HCFR_OK:
			return "No device error";

		case HCFR_BAD_READING:
			return "Invalid reading";

		case HCFR_BAD_FIRMWARE:
			return "Bad firmware version";

		case HCFR_CALIB_CALC:
			return "Error computing calibration matrix";

		default:
			return "Unknown error code";
	}
}


/* Convert a machine specific error code into an abstract dtp code */
static inst_code 
hcfr_interp_code(inst *pp, int ec) {
	hcfr *p = (hcfr *)pp;

	ec &= inst_imask;
	switch (ec) {

		case HCFR_OK:
			return inst_ok;

		case HCFR_CALIB_CALC:
			return inst_internal_error | ec;

		case HCFR_COMS_FAIL:
			return inst_coms_fail | ec;

		case HCFR_UNKNOWN_MODEL:
			return inst_unknown_model | ec;

		case HCFR_DATA_PARSE_ERROR:
			return inst_protocol_error | ec;

		case HCFR_BAD_READING:
			return inst_misread | ec;

//		case HCFR_NEEDS_OFFSET_CAL:
//			return inst_needs_cal | ec;

		case HCFR_BAD_FIRMWARE:
			return inst_hardware_fail | ec;
	}
	return inst_other_error | ec;
}

/* Destroy ourselves */
static void
hcfr_del(inst *pp) {
	hcfr *p = (hcfr *)pp;
	if (p->icom != NULL)
		p->icom->del(p->icom);
	inst_del_disptype_list(p->dtlist, p->ndtlist);
	p->vdel(pp);
	free(p);
}

/* Return the instrument mode capabilities */
static void hcfr_capabilities(inst *pp,
inst_mode *pcap1,
inst2_capability *pcap2,
inst3_capability *pcap3) {
	hcfr *p = (hcfr *)pp;
	inst_mode cap = 0;
	inst2_capability cap2 = 0;

	cap |= inst_mode_emis_spot
	    |  inst_mode_colorimeter
	       ;

	cap2 |= inst2_prog_trig
	     |  inst2_user_trig
	     |  inst2_disptype
	     |  inst2_ccmx
	        ;

	if (pcap1 != NULL)
		*pcap1 = cap;
	if (pcap2 != NULL)
		*pcap2 = cap2;
	if (pcap3 != NULL)
		*pcap3 = inst3_none;
}

/* Check device measurement mode */
static inst_code hcfr_check_mode(inst *pp, inst_mode m) {
	inst_mode cap;

	if (!pp->gotcoms)
		return inst_no_coms;
	if (!pp->inited)
		return inst_no_init;

	pp->capabilities(pp, &cap, NULL, NULL);

	/* Simple test */
	if (m & ~cap)
		return inst_unsupported;

	/* only display emission mode supported */
	if (!IMODETST(m, inst_mode_emis_spot)) {
		return inst_unsupported;
	}

	return inst_ok;
}

/* Set device measurement mode */
static inst_code hcfr_set_mode(inst *pp, inst_mode m) {
	inst_code ev;

	if ((ev = hcfr_check_mode(pp, m)) != inst_ok)
		return ev;

	return inst_ok;
}

static inst_disptypesel hcfr_disptypesel[4] = {
	{
		inst_dtflags_default,
		0,
		"l",
		"LCD display",
		0,
		disptech_lcd,
		0
	},
	{
		inst_dtflags_none,		/* flags */
		0,						/* cbid */
		"c",					/* sel */
		"CRT display",			/* desc */
		0,						/* refr */
		disptech_crt,			/* disptype */
		1						/* ix */
	},
	{
		inst_dtflags_none,
		1,
		"R",
		"Raw Reading",
		0,
		disptech_unknown,
		2
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
static inst_code hcfr_get_disptypesel(
inst *pp,
int *pnsels,				/* Return number of display types */
inst_disptypesel **psels,	/* Return the array of display types */
int allconfig,				/* nz to return list for all configs, not just current. */
int recreate				/* nz to re-check for new ccmx & ccss files */
) {
	hcfr *p = (hcfr *)pp;
	inst_code rv = inst_ok;

	/* Create/Re-create a current list of abailable display types */
	if (p->dtlist == NULL || recreate) {
		if ((rv = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    hcfr_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return rv;
	}

	if (pnsels != NULL)
		*pnsels = p->ndtlist;

	if (psels != NULL)
		*psels = p->dtlist;

	return inst_ok;
}

/* Given a display type entry, setup for that type */
static inst_code set_disp_type(hcfr *p, inst_disptypesel *dentry) {

	if (dentry->flags & inst_dtflags_ccmx) {
		inst_code ev;
		if ((ev = set_base_disp_type(p, dentry->cc_cbid)) != inst_ok)
			return ev;
		icmCpy3x3(p->ccmat, dentry->mat);
		p->dtech = dentry->dtech;
		p->cbid = 0; 	/* Can't be a base type */

	} else {

		p->ix = dentry->ix; 
		p->dtech = dentry->dtech;
		p->cbid = dentry->cbid; 
		p->ucbid = dentry->cbid;    /* This is underying base if dentry is base selection */
		icmSetUnity3x3(p->ccmat);
	}
	p->refrmode = dentry->refr; 

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
static inst_code hcfr_set_disptype(inst *pp, int ix) {
	hcfr *p = (hcfr *)pp;
	inst_code ev;
	inst_disptypesel *dentry;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list(pp, &p->ndtlist, &p->dtlist,
		    hcfr_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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
static inst_code set_default_disp_type(hcfr *p) {
	inst_code ev;
	int i;

	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
		    hcfr_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
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
static inst_code set_base_disp_type(hcfr *p, int cbid) {
	inst_code ev;
	int i;

	if (cbid == 0) {
		a1loge(p->log, 1, "hcfr set_base_disp_type: can't set base display type of 0\n");
		return inst_wrong_setup;
	}
	if (p->dtlist == NULL) {
		if ((ev = inst_creat_disptype_list((inst *)p, &p->ndtlist, &p->dtlist,
		    hcfr_disptypesel, 0 /* doccss*/, 1 /* doccmx */)) != inst_ok)
			return ev;
	}

	for (i = 0; !(p->dtlist[i].flags & inst_dtflags_end); i++) {
		if (p->dtlist[i].cbid == cbid)
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
static inst_code hcfr_get_disptechi(
inst *pp,
disptech *dtech,
int *refrmode,
int *cbid) {
	hcfr *p = (hcfr *)pp;
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
 * Since there is no interaction with the instrument,
 * was assume that all of these can be done before initialisation.
 */
static inst_code
hcfr_get_set_opt(inst *pp, inst_opt_type m, ...) {
	hcfr *p = (hcfr *)pp;
	inst_code ev = inst_ok;

	/* Record the trigger mode */
	if (m == inst_opt_trig_prog
	 || m == inst_opt_trig_user) {
		p->trig = m;
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
extern hcfr *new_hcfr(icoms *icom, instType dtype) {
	hcfr *p;
	if ((p = (hcfr *)calloc(sizeof(hcfr),1)) == NULL) {
		a1loge(icom->log, 1, "new_hcfr: malloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(icom->log);

	p->init_coms        = hcfr_init_coms;
	p->init_inst        = hcfr_init_inst;
	p->capabilities     = hcfr_capabilities;
	p->check_mode       = hcfr_check_mode;
	p->set_mode         = hcfr_set_mode;
	p->get_disptypesel  = hcfr_get_disptypesel;
	p->set_disptype     = hcfr_set_disptype;
	p->get_disptechi    = hcfr_get_disptechi;
	p->get_set_opt      = hcfr_get_set_opt;
	p->read_sample      = hcfr_read_sample;
	p->col_cor_mat      = hcfr_col_cor_mat;
	p->interp_error     = hcfr_interp_error;
	p->del              = hcfr_del;

	p->icom = icom;
	p->dtype = dtype;

	icmSetUnity3x3(p->ccmat);	/* Set the colorimeter correction matrix to do nothing */
	p->dtech = disptech_unknown;

	return p;
}


