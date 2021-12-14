#ifndef SMCUBE_H

/* 
 * Argyll Color Management System
 *
 * JETI smcube related defines
 *
 * Author: Graeme W. Gill
 * Date:   13/3/2013
 *
 * Copyright 2001 - 2013, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Based on DTP02.h
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
#define SMCUBE_INTERNAL_ERROR			0xff01		/* Internal software error */
#define SMCUBE_TIMEOUT				    0xff02		/* Communication timeout */
#define SMCUBE_COMS_FAIL				0xff03		/* Communication failure */
#define SMCUBE_UNKNOWN_MODEL			0xff04		/* Not a SwatchMate Cube */
#define SMCUBE_DATA_PARSE_ERROR  		0xff05		/* Read data parsing error */


/* Real instrument error code */
#define SMCUBE_OK						0

/* Internal software errors */
#define SMCUBE_INT_THREADFAILED       	0x1000
#define SMCUBE_INT_ILL_WRITE          	0x1001		/* Write to factor calibration */
#define SMCUBE_INT_WHITE_CALIB        	0x1002		/* No white calibration */
#define SMCUBE_INT_BLACK_CALIB        	0x1003		/* No black calibration */
#define SMCUBE_INT_GLOSS_CALIB        	0x1004		/* No gloss calibration */
#define SMCUBE_INT_CAL_SAVE          	0x1005		/* Saving calibration to file failed */
#define SMCUBE_INT_CAL_RESTORE        	0x1006		/* Restoring calibration to file failed */
#define SMCUBE_INT_CAL_TOUCH          	0x1007		/* Touching calibration to file failed */

/* Other errors */
#define SMCUBE_WHITE_CALIB_ERR        	0x2000		/* White calibration isn't reasonable */
#define SMCUBE_BLACK_CALIB_ERR        	0x2001		/* Black calibration isn't reasonable */
#define SMCUBE_GLOSS_CALIB_ERR        	0x2002		/* Gloss calibration isn't reasonable */

/* SMCUBE communication object */
struct _smcube {
	INST_OBJ_BASE

	int bt;						/* Bluetooth coms rather than USB/serial flag */

	amutex lock;				/* Command lock */

	int version;				/* Cube version ? */

	inst_mode mode;				/* Currently instrument mode */

	int icx;					/* Internal calibration index, 0 = Matt, 1 = Gloss, 2 = Factory */
	disptech dtech;				/* Display technology enum (not used) */

	inst_opt_type trig;			/* Reading trigger mode */

	/* Argyll Calibration */
	int white_valid;			/* idark calibration factors valid */
	time_t wdate;				/* Date/time of last white calibration */
	double sscale[3];			/* Sensor RGB white scale values to 0.0 - 1.0 range */
	double ctemp;				/* Calibration temperature */

	int dark_valid;				/* dark calibration factors valid */
	int dark_default;			/* dark calibration factors are from default */
	time_t ddate;				/* Date/time of last dark dark calibration */
	double soff[3];				/* Sensor RGB dark offset values */

	int gloss_valid;			/* gloss calibration factors valid */
	int gloss_default;			/* gloss calibration factors are from default */
	time_t gdate;				/* Date/time of last gloss calibration */
	double goff[3];				/* Sensor gloss RGB offset values */

	int noinitcalib;			/* Disable initial calibration if not essential */
	int lo_secs;				/* Seconds since last opened (from calibration file mod time) */ 
	int want_wcalib;			/* Want White Calibration at start */

	/* Other state */
	athread *th;                /* Diffuser position monitoring thread */
	volatile int th_term;		/* nz to terminate thread */
	volatile int th_termed;		/* nz when thread terminated */

	volatile int switch_count;	/* Incremented in thread */
	volatile int hide_switch;	/* Set to supress switch event during read */
	double XYZ[3];				/* Button triggered XYZ in factory mode */

	}; typedef struct _smcube smcube;

/* Constructor */
extern smcube *new_smcube(icoms *icom, instType itype);

#ifdef __cplusplus
	}
#endif

#define SMCUBE_H
#endif /* SMCUBE_H */
