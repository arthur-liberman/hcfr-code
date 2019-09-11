#ifndef DTP22_H

/* 
 * Argyll Color Correction System
 *
 * Xrite DTP22 related defines
 *
 * Author: Graeme W. Gill
 * Date:   17/11/2006
 *
 * Copyright 2001 - 2013, Graeme W. Gill
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

#ifdef __cplusplus
	extern "C" {
#endif

/* Note: update dtp22_interp_error() and dtp22_interp_code() in dtp22.c */
/* if anything of these #defines are added or subtracted */

/* Fake Error codes */
#define DTP22_INTERNAL_ERROR			0x61		/* Internal software error */
#define DTP22_COMS_FAIL					0x62		/* Communication failure */
#define DTP22_UNKNOWN_MODEL				0x63		/* Not a DPT22 */
#define DTP22_DATA_PARSE_ERROR  		0x64		/* Read data parsing error */
#define DTP22_UNKN_OEM		    		0x69		/* Unrecognized OEM */
#define DTP22_BAD_PASSWORD	    		0x6A		/* Password wasn't accepted */

/* Real error code */
#define DTP22_OK   						0x00

#define DTP22_BAD_COMMAND				0x01
#define DTP22_PRM_RANGE					0x02
#define DTP22_MEMORY_OVERFLOW			0x04
#define DTP22_INVALID_BAUD_RATE			0x05
#define DTP22_TIMEOUT					0x07
#define DTP22_SYNTAX_ERROR				0x08
#define DTP22_INCORRECT_DATA_FORMAT 	0x09
#define DTP22_WEAK_LAMP             	0x10
#define DTP22_LAMP_FAILED             	0x11
#define DTP22_UNSTABLE_CAL             	0x12
#define DTP22_CAL_GAIN_ERROR           	0x13
#define DTP22_SENSOR_FAILURE           	0x14
#define DTP22_BLACK_CAL_TOO_HIGH       	0x15
#define DTP22_UNSTABLE_BLACK_CAL       	0x16
#define DTP22_CAL_MEM_ERROR          	0x17
#define DTP22_FILTER_MOTOR          	0x21
#define DTP22_LAMP_FAILED_READING      	0x22
#define DTP22_POWER_INTR_READING      	0x23
#define DTP22_SIG_OFFSETS_READING      	0x24
#define DTP22_RD_SWITCH_TO_SOON      	0x25
#define DTP22_OVERRANGE              	0x26
#define DTP22_FILT_POS_ERROR           	0x28
#define DTP22_FACT_TST_CONNECT         	0x2A
#define DTP22_FACT_TST_LAMP_INH        	0x2B


#define DTP22_EEPROM_FAILURE			0x70
#define DTP22_PROGRAM_WRITE_FAIL		0x71
#define DTP22_MEMORY_WRITE_FAIL			0x72

/* DTP22 communication object */
struct _dtp22 {
	INST_OBJ_BASE

	/* *** DTP41 private data **** */
	unsigned char key[4];		/* Challenge/response key */
	int keyvalid;				/* nz if key is valid */
	int serno;					/* Serial number of instrument */
	int oemsn;					/* Serial number of OEM */
	int plaqueno;				/* Serial number of calibration plaque */
	inst_mode    mode;			/* Currently instrument mode */
	inst_mode    lastmode;		/* Last requested mode */
	int	need_cal;				/* White calibration needed flag */
	int noutocalib;				/* Don't mode change or auto calibrate */
	inst_opt_type trig;			/* Reading trigger mode */

	xcalstd native_calstd;		/* Instrument native calibration standard */
	xcalstd target_calstd;		/* Returned calibration standard */

	int custfilt_en;			/* Custom filter enabled */
	xspect custfilt;			/* Custom filter */

	}; typedef struct _dtp22 dtp22;

/* Constructor */
extern dtp22 *new_dtp22(icoms *icom, instType itype);

#ifdef __cplusplus
	}
#endif

#define DTP22_H
#endif /* DTP22_H */
