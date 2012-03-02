#ifndef HCFR_H

/* 
 * Argyll Color Correction System
 *
 * Colorimtre HCFR related defines
 *
 * Author: Graeme W. Gill
 * Date:   20/1/2007
 *
 * Copyright 2007, Graeme W. Gill
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

#include "inst.h"

/* Required minimum firmware version */
#define HCFR_FIRMWARE_MAJOR_VERSION	5
#define HCFR_FIRMWARE_MINOR_VERSION	0


/* Command byte contents. (A value of 0x00 won't be tranmsitted properly) */
/* 0xff = get firmware version */


#define HCFR_MEAS_RGB		0x01		/* Enable reading RGB sensor values */
#define HCFR_MEAS_WHITE		0x02		/* Enable reading White values */
#define HCFR_MEAS_SENS0		0x04		/* Read sensor 0 */
#define HCFR_MEAS_SENS1		0x08		/* Read sensor 1 */
#define HCFR_INTERLACE_0	0x00		/* No interlace */
#define HCFR_INTERLACE_1	0x10		/* 2 way interlace ? */
#define HCFR_INTERLACE_2	0x20		/* 4 way interlace ?? */
#define HCFR_INTERLACE_3	0x30		/* ? way interlace ??? */
#define HCFR_FAST_MEASURE	0x40		/* Fast measure */

#define HCFR_GET_VERS		0xFF		/* Get the firmware version number */

/* Note: update hcfr_interp_error() and hcfr_interp_code() in hcfr.c */
/* if anything of these #defines are added or subtracted */

/* Fake Error codes */
#define HCFR_INTERNAL_ERROR				0x61		/* Internal software error */
#define HCFR_COMS_FAIL					0x62		/* Communication failure */
#define HCFR_UNKNOWN_MODEL				0x63		/* Not an HCFR */
#define HCFR_DATA_PARSE_ERROR  			0x64		/* Read data parsing error */
#define HCFR_USER_ABORT		    		0x65		/* User hit abort */
#define HCFR_USER_TERM		    		0x66		/* User hit terminate */
#define HCFR_USER_TRIG 		    		0x67		/* User hit trigger */
#define HCFR_USER_CMND		    		0x68		/* User hit command */

/* Real error code */
#define HCFR_OK   						0x00

#define HCFR_BAD_FIRMWARE 				0x01		/* Bad firmware version */

#define HCFR_BAD_READING				0x30		/* Error doing or parsing reading */

#define HCFR_CALIB_CALC				    0x40		/* Error computing calibration matrix */

/* HCFR communication object */
struct _hcfr {
	INST_OBJ_BASE

	int maj, min;			/* Firmware version */

	int cal_mode;			/* Calibration type */
							/* 0 = CRT */
							/* 1 = LCD */
							/* 2 = raw RGB from sensors */

	double crt[3][3];		/* CRT RGB->XYZ transformation matrix */
	double lcd[3][3];		/* CRT RGB->XYZ transformation matrix */

	double ccmat[3][3];		/* Colorimeter correction matrix */

	inst_opt_mode trig;		/* Reading trigger mode */
	int trig_return;			/* Emit "\n" after trigger */

	}; typedef struct _hcfr hcfr;

/* Constructor */
extern hcfr *new_hcfr(icoms *icom, instType itype, int debug, int verb);


#define HCFR_H
#endif /* HCFR_H */
