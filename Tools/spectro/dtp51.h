#ifndef DTP51_H

/* 
 * Argyll Color Correction System
 *
 * Xrite DTP51 related defines
 *
 * Author: Graeme W. Gill
 * Date:   5/10/1996
 *
 * Copyright 1996 - 2013, Graeme W. Gill
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

/* Fake Error codes */
#define DTP51_INTERNAL_ERROR		0x61			/* Internal software error */
#define DTP51_COMS_FAIL				0x62			/* Communication failure */
#define DTP51_UNKNOWN_MODEL			0x63			/* Not a DPT51 or DTP52 */
#define DTP51_DATA_PARSE_ERROR  	0x64			/* Read data parsing error */

/* Real error code */
#define DTP51_OK   					0x00

#define DTP51_BAD_COMMAND			0x01
#define DTP51_PRM_RANGE				0x02
#define DTP51_DISPLAY_OVERFLOW		0x03
#define DTP51_MEMORY_OVERFLOW		0x04
#define DTP51_INVALID_BAUD_RATE		0x05
#define DTP51_TIMEOUT				0x07
#define DTP51_INVALID_PASS			0x09
#define DTP51_INVALID_STEP			0x0A
#define DTP51_NO_DATA_AVAILABLE		0x0B

#define DTP51_LAMP_MARGINAL			0x10
#define DTP51_LAMP_FAILURE			0x11
#define DTP51_STRIP_RESTRAINED		0x12
#define DTP51_BAD_CAL_STRIP			0x13
#define DTP51_MOTOR_ERROR			0x14
#define DTP51_BAD_BARCODE			0x15

#define DTP51_INVALID_READING		0x20
#define DTP51_WRONG_COLOR			0x21
#define DTP51_BATTERY_TOO_LOW		0x22
#define DTP51_NEEDS_CALIBRATION		0x23
#define DTP51_COMP_TABLE_MISMATCH	0x24
#define DTP51_BAD_COMP_TABLE		0x25
#define DTP51_NO_VALID_DATA			0x26
#define DTP51_BAD_PATCH				0x27

#define DTP51_BAD_STRING_LENGTH		0x30
#define DTP51_BAD_CHARACTER			0x31
#define DTP51_BAD_MEAS_TYPE			0x32
#define DTP51_BAD_COLOR				0x33
#define DTP51_BAD_STEPS				0x34
#define DTP51_BAD_STOP_LOCATION		0x35
#define DTP51_BAD_OUTPUT_TYPE		0x36
#define DTP51_MEMORY_ERROR			0x37
#define DTP51_BAD_N_FACTOR			0x38
#define DTP51_STRIP_DOESNT_EXIST	0x39
#define DTP51_BAD_MIN_MAX_VALUE		0x3A

#define DTP51_BAD_SERIAL_NUMBER		0x40

/* DTP51 communication object */
struct _dtp51 {
	INST_OBJ_BASE

	int need_cal;				/* needs calibration */
	inst_opt_type trig;			/* Reading trigger mode */
	
	}; typedef struct _dtp51 dtp51;

/* Constructor */
extern dtp51 *new_dtp51(icoms *icom, instType itype);



#define DTP51_H
#endif /* DTP51_H */
