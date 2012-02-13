#ifndef DTP41_H

/* 
 * Argyll Color Correction System
 *
 * Xrite DTP41 related defines
 *
 * Author: Graeme W. Gill
 * Date:   10/3/2001
 *
 * Copyright 1996 - 2007 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Derived from DTP51.h
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
#define DTP41_INTERNAL_ERROR		0x61			/* Internal software error */
#define DTP41_COMS_FAIL				0x62			/* Communication failure */
#define DTP41_UNKNOWN_MODEL			0x63			/* Not a DPT51 or DTP52 */
#define DTP41_DATA_PARSE_ERROR  	0x64			/* Read data parsing error */
#define DTP41_USER_ABORT		    0x65			/* User hit abort */
#define DTP41_USER_TERM		    	0x66			/* User hit terminate */
#define DTP41_USER_TRIG 		    0x67			/* User hit trigger */
#define DTP41_USER_CMND		    	0x68			/* User hit command */

/* Real error code */
#define DTP41_OK   					0x00

#define DTP41_MEASUREMENT_STATUS	0x01
#define DTP41_CALIBRATION_STATUS	0x02
#define DTP41_KEYPRESS_STATUS		0x03
#define DTP41_DEFAULTS_LOADED_STATUS 0x04

#define DTP41_BAD_COMMAND			0x11
#define DTP41_BAD_PARAMETERS		0x12
#define DTP41_PRM_RANGE_ERROR		0x13
#define DTP41_BUSY					0x14
#define DTP41_USER_ABORT_ERROR	    0x15

#define DTP41_MEASUREMENT_ERROR		0x20
#define DTP41_TIMEOUT				0x21
#define DTP41_BAD_STRIP				0x22
#define DTP41_BAD_COLOR				0x23
#define DTP41_BAD_STEP				0x24
#define DTP41_BAD_PASS				0x25
#define DTP41_BAD_PATCHES			0x26
#define DTP41_BAD_READING			0x27
#define DTP41_NEEDS_CAL_ERROR		0x28
#define DTP41_CAL_FAILURE_ERROR		0x29

#define DTP41_INSTRUMENT_ERROR		0x30
#define DTP41_LAMP_ERROR			0x31
#define DTP41_FILTER_ERROR			0x32
#define DTP41_FILTER_MOTOR_ERROR	0x33
#define DTP41_DRIVE_MOTOR_ERROR		0x34
#define DTP41_KEYPAD_ERROR			0x35
#define DTP41_DISPLAY_ERROR			0x36
#define DTP41_MEMORY_ERROR			0x37
#define DTP41_ADC_ERROR				0x38
#define DTP41_PROCESSOR_ERROR		0x39
#define DTP41_BATTERY_ERROR			0x3A
#define DTP41_BATTERY_LOW_ERROR		0x3B
#define DTP41_INPUT_POWER_ERROR		0x3C
#define DTP41_TEMPERATURE_ERROR		0x3D
#define DTP41_BATTERY_ABSENT_ERROR	0x3E
#define DTP41_TRAN_LAMP_ERROR		0x3F
#define DTP41_INVALID_COMMAND_ERROR	0x40

/* DTP41 communication object */
struct _dtp41 {
	/* **** base instrument class **** */
	INST_OBJ_BASE

	/* *** DTP41 private data **** */
	inst_capability  cap;		/* Instrument capability */
	inst2_capability cap2;		/* Instrument capability */
	inst_mode    mode;			/* Currently instrument mode */
	inst_mode    lastmode;		/* Last requested mode */

	int nstaticr;				/* Number of static readings */
	int need_cal;				/* needs calibration */
	inst_opt_mode trig;			/* Reading trigger mode */
	int trig_return;			/* Emit "\n" after trigger */
	
	}; typedef struct _dtp41 dtp41;

/* Constructor */
extern dtp41 *new_dtp41(icoms *icom, int debug, int verb);

#define DTP41_H
#endif /* DTP41_H */
