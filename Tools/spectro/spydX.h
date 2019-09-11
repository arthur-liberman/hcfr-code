#ifndef SPYDX_H

/* 
 * Argyll Color Correction System
 *
 * ColorVision Spyder X related software.
 *
 * Author: Graeme W. Gill
 * Date:   20/3/2019
 *
 * Copyright 2006 - 2019, Graeme W. Gill
 * All rights reserved.
 *
 * (Based on spydX.h)
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

#ifdef __cplusplus
	extern "C" {
#endif

/* Note: update spydX_interp_error() and spydX_interp_code() in spydX.c */
/* if anything of these #defines are added or subtracted */

typedef int spydX_code;

#define SPYDX_OK   					0x00

/* Fake Error codes */
#define SPYDX_INTERNAL_ERROR		0x61		/* Internal software error */
#define SPYDX_COMS_FAIL				0x62		/* Communication failure */
#define SPYDX_UNKNOWN_MODEL			0x63		/* Not an spydXlay */
#define SPYDX_DATA_PARSE_ERROR  	0x64		/* Read data parsing error */


#define SPYDX_NO_COMS				0x80		/* No communications when it's needed */
#define SPYDX_CIX_MISMATCH			0x81		/* Got different calibration than asked for */

/* Most 8 bit instrument error codes are unknown */
#define SPYDX_BAD_PARAM				0x01		/* Parameter out of range ? */


/* Internal error codes */
#define SPYDX_INT_CAL_SAVE           0xE009		/* Saving calibration to file failed */
#define SPYDX_INT_CAL_RESTORE        0xE00A		/* Restoring calibration to file failed */
#define SPYDX_INT_CAL_TOUCH          0xE00B		/* Touching calibration to file failed */
 
/* Extra native calibration info */
typedef struct {
	int ix;					/* Native index */

	int v1;					/* Magic 8bit value from get_mtx and supplied to get_setup cmd */
							/* Seems to be gain setting (2 bits), but there are no */
							/* setup entries for gains other than 3 (== 64x) */
							/* This is the same for all 4 calibrations and doesn't vary */
							/* with light level. */

	int v2;					/* Magic 16bit value from get_mtx and supplied to measure cmd */
							/* This is the integration time in msec. Actual time = */
							/* 2.8 * floor(v2/2.8), maximum value = 719. Default = 714 */
							/* This is the same for all 4 calibrations and doesn't vary */
							/* with light level. */

	int v3;					/* Magic value returned and not used ? */

	int s1;					/* 8 bit value from get_setup (same as v1, sets gain) */
	int s2[4];				/* Magic values from get_setup and supplied to measure cmd */  
							/* Some sort of trim ?? */
							/* Doesn't seem to vary */
	int s3[4];				/* Black subtraction values from get_setup */

	double mat[3][3];		/* Native calibration matrix */

} calinfo;

#define SPYDX_NOCALIBS 4

/* SPYDX communication object */
struct _spydX {
	INST_OBJ_BASE

	inst_mode mode;				/* Currently selected mode (emis/ambient/etc.) */

	inst_opt_type trig;			/* Reading trigger mode */

	unsigned int hwvn[2];		/* Harware major, Minor version numbers */

								/* Initial SpyderX = 2.07 */


	char    serno[9];			/* 8:8xB  Serial number as zero terminated string */

	/* Computed factors and state */
	inst_disptypesel *dtlist;	/* Display Type list */
	int ndtlist;				/* Number of valid dtlist entries */

	calinfo cinfo[SPYDX_NOCALIBS];	/* cal  & meas setup info indexed by native ix */
	
	int ix;						/* current native cal index */
	int cbid;					/* current calibration base ID, 0 if not a base */
	int ucbid;					/* Underlying base ID if being used for matrix, 0 othewise */
	disptech dtech;				/* Display technology enum */

	double ccmat[3][3];			/* Current colorimeter correction matrix, unity if none */

	int bcal_done;
	int bcal[3];				/* Black offset calibration values */
	time_t bdate;				/* Date/time of last black calibration */

	int noinitcalib;		 	/* Don't do initial calibrate */
	int lo_secs;				/* Seconds since last opened (from calibration file mod time) */ 

}; typedef struct _spydX spydX;

/* Constructor */
extern spydX *new_spydX(icoms *icom, instType itype);

#ifdef __cplusplus
	}
#endif

#define SPYDX_H
#endif /* SPYDX_H */
