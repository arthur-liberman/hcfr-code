#ifndef DTP92_H

/* 
 * Argyll Color Correction System
 *
 * Xrite DTP92/94 related defines
 *
 * Author: Graeme W. Gill
 * Date:   5/6/2001
 *
 * Copyright 2001 - 2007, Graeme W. Gill
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

/* Note: update dtp92_interp_error() and dtp92_interp_code() in dtp92.c */
/* if anything of these #defines are added or subtracted */

/* Fake Error codes */
#define DTP92_INTERNAL_ERROR			0x61		/* Internal software error */
#define DTP92_COMS_FAIL					0x62		/* Communication failure */
#define DTP92_UNKNOWN_MODEL				0x63		/* Not a DPT92 */
#define DTP92_DATA_PARSE_ERROR  		0x64		/* Read data parsing error */
#define DTP92_USER_ABORT		    	0x65		/* User hit abort */
#define DTP92_USER_TERM		    		0x66		/* User hit terminate */
#define DTP92_USER_TRIG 		    	0x67		/* User hit trigger */
#define DTP92_USER_CMND		    		0x68		/* User hit command */

/* Real error code */
#define DTP92_OK   						0x00

#define DTP92_BAD_COMMAND				0x01
#define DTP92_PRM_RANGE					0x02
#define DTP92_MEMORY_OVERFLOW			0x04
#define DTP92_INVALID_BAUD_RATE			0x05	/* 92 only */
#define DTP92_TIMEOUT					0x07
#define DTP92_SYNTAX_ERROR				0x08
#define DTP92_NO_DATA_AVAILABLE			0x0B
#define DTP92_MISSING_PARAMETER			0x0C
#define DTP92_CALIBRATION_DENIED		0x0D
#define DTP92_NEEDS_OFFSET_CAL			0x16
#define DTP92_NEEDS_RATIO_CAL			0x17	/* 92 only */
#define DTP92_NEEDS_LUMINANCE_CAL		0x18
#define DTP92_NEEDS_WHITE_POINT_CAL		0x19
#define DTP92_NEEDS_BLACK_POINT_CAL		0x1A
#define DTP92_NEEDS_OFFSET_DRIFT_CAL	0x1B	/* 94 only */
#define DTP92_INVALID_READING			0x20
#define DTP92_BAD_COMP_TABLE			0x25
#define DTP92_TOO_MUCH_LIGHT			0x28
#define DTP92_NOT_ENOUGH_LIGHT			0x29

#define DTP92_BAD_SERIAL_NUMBER			0x40

#define DTP92_NO_MODULATION				0x50	/* 92 only */

#define DTP92_EEPROM_FAILURE			0x70
#define DTP92_FLASH_WRITE_FAILURE		0x71
#define DTP92_BAD_CONFIGURATION			0x72	/* 94 only */
#define DTP92_INST_INTERNAL_ERROR		0x7F	/* 92 only */


/* DTP92 communication object */
struct _dtp92 {
	INST_OBJ_BASE

	double ccmat[3][3];			/* Colorimeter correction matrix */

	int need_offset_cal;		/* Flags to indicate type of calibration needed */
	int need_ratio_cal;
	inst_opt_mode trig;			/* Reading trigger mode */
	int trig_return;			/* Emit "\n" after trigger */

	}; typedef struct _dtp92 dtp92;

/* Constructor */
extern dtp92 *new_dtp92(icoms *icom, instType itype, int debug, int verb);


#define DTP92_H
#endif /* DTP92_H */
