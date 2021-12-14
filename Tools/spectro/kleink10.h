#ifndef KLEINK10_H

/* 
 * Argyll Color Management System
 *
 * Klein K10 related defines
 *
 * Author: Graeme W. Gill
 * Date:   29/4/2014
 *
 * Copyright 2001 - 2014, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Based on DTP02.h & specbos.h
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

#include "inst.h"

#ifdef __cplusplus
	extern "C" {
#endif

/* Fake Error codes */
#define K10_INTERNAL_ERROR			0xff01		/* Internal software error */
#define K10_TIMEOUT				    0xff02		/* Communication timeout */
#define K10_COMS_FAIL				0xff03		/* Communication failure */
#define K10_UNKNOWN_MODEL			0xff04		/* Not a JETI kleink10 */
#define K10_DATA_PARSE_ERROR  		0xff05		/* Read data parsing error */

//#define K10_SPOS_EMIS				0xff06		/* Needs to be in emsissive configuration */
//#define K10_SPOS_AMB				0xff07		/* Needs to be in ambient configuration */

/* Real instrument error code */
#define K10_OK						0

/* Other errors */
#define K10_CMD_VERIFY				0x1000		/* Didn't echo the commamd */
#define K10_BAD_RETVAL				0x1001		/* Error code return value can't be parsed */

#define K10_FIRMWARE				0x2001		/* Firmware error */

#define K10_BLACK_EXCESS			0x2010		/* Black Excessive */
#define K10_BLACK_OVERDRIVE			0x2011		/* Black Overdrive */
#define K10_BLACK_ZERO				0x2012		/* Black Zero */

#define K10_OVER_HIGH_RANGE			0x2020		/* Over High Range */
#define K10_TOP_OVER_RANGE			0x2021		/* Top over range */
#define K10_BOT_UNDER_RANGE			0x2022		/* Bottom under range */
#define K10_AIMING_LIGHTS			0x2023		/* Aiming lights on when measuring */

#define K10_RANGE_CHANGE           	0x2024		/* Range changed during measurment */
#define K10_NOREFR_FOUND           	0x2025		/* Unable to measure refresh rate */
#define K10_NOTRANS_FOUND       	0x2026		/* No delay measurment transition found */

#define K10_BLACK_CAL_INIT       	0x2027		/* Instrument not setup for bcal */
#define K10_BLACK_CAL_FAIL       	0x2028		/* Black calibration failed */

#define K10_UNKNOWN			 		0x2030		/* Unknown error code */

#define K10_INT_MALLOC		 		0x3000		/* Malloc failed */

typedef enum {
	k10_k1     = 0,	/* K-1 */
	k10_k8     = 1,	/* K-8 */
	k10_k10    = 2,	/* K-10 */
	k10_k10a   = 3,	/* K-10-A */
	k10_kv10a  = 4,	/* KV-10-A */
} k10_dtype;

/* Klein K10 communication object */
struct _kleink10 {
	INST_OBJ_BASE

	amutex lock;				/* Command lock */

	k10_dtype model;			/* Klein kleink10 model number */
	char serial_no[21];			/* Serial number */
	char firm_ver[21];			/* Firmware version number */
	int comdel;					/* Estimated communication delay to the instrument */

	inst_mode mode;				/* Currently instrument mode */

	inst_opt_type trig;			/* Reading trigger mode */

	int autor;					/* Auto range state, nz = on */
	int lights;					/* Target light state, nz = on */

	inst_disptypesel *dtlist;	/* Current display Type list */
	int     ndtlist;			/* Number of valid dtlist entries */
	int     cbid;				/* current calibration base ID, 0 if not a base */
	int     ucbid;				/* Underlying base ID if being used for matrix, 0 othewise */
	disptech dtech;				/* Display technology enum */
	double  ccmat[3][3];		/* Colorimeter correction matrix */

	volatile double whitestamp;	/* meas_delay() white timestamp */

	}; typedef struct _kleink10 kleink10;

/* Constructor */
extern kleink10 *new_kleink10(icoms *icom, instType itype);

#ifdef __cplusplus
	}
#endif

#define KLEINK10_H
#endif /* KLEINK10_H */
