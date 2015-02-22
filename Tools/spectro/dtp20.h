#ifndef DTP20_H

/* 
 * Argyll Color Correction System
 *
 * Xrite DTP20 related defines
 *
 * Author: Graeme W. Gill
 * Date:   10/1/2007
 *
 * Copyright 1996 - 2013 Graeme W. Gill
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
#define DTP20_INTERNAL_ERROR			0x81			/* Internal software error */
#define DTP20_COMS_FAIL					0x82			/* Communication failure */
#define DTP20_UNKNOWN_MODEL				0x83			/* Not a DPT51 or DTP52 */
#define DTP20_DATA_PARSE_ERROR  		0x84			/* Read data parsing error */

#define DTP20_NOT_EMPTY					0x89			/* Trying to read strips when not empty */
#define DTP20_UNEXPECTED_STATUS			0x90			/* Instrument has unexpected status */

/* Real error code */
#define DTP20_OK   						0x00

#define DTP20_MEASUREMENT_STATUS		0x01

#define DTP20_BAD_COMMAND				0x11
#define DTP20_BAD_PARAMETERS			0x12
#define DTP20_PRM_RANGE_ERROR			0x13
#define DTP20_BUSY						0x14

#define DTP20_MEASUREMENT_ERROR			0x20
#define DTP20_TIMEOUT					0x21
#define DTP20_BAD_STRIP					0x22

#define DTP20_NEEDS_CAL_ERROR			0x28
#define DTP20_CAL_FAILURE_ERROR			0x29

#define DTP20_INSTRUMENT_ERROR			0x30
#define DTP20_LAMP_ERROR				0x31

#define DTP20_BAD_TID					0x33
#define DTP20_FLASH_ERASE_FAILURE		0x34
#define DTP20_FLASH_WRITE_FAILURE		0x35
#define DTP20_FLASH_VERIFY_FAILURE		0x36
#define DTP20_MEMORY_ERROR				0x37
#define DTP20_ADC_ERROR					0x38
#define DTP20_PROCESSOR_ERROR			0x39
#define DTP20_BATTERY_ERROR				0x3A
#define DTP20_BATTERY_LOW_ERROR			0x3B
#define DTP20_INPUT_POWER_ERROR			0x3C

#define DTP20_BATTERY_ABSENT_ERROR		0x3E
#define DTP20_BAD_CONFIGURATION			0x3F

#define DTP20_BAD_SPOT					0x41
#define DTP20_END_OF_DATA				0x42
#define DTP20_DBASE_PROFILE_NOT_EMPTY	0x43
#define DTP20_MEMORY_OVERFLOW_ERROR 	0x44
#define DTP20_BAD_CALIBRATION			0x45

#define DTP20_CYAN_CAL_ERROR			0x50
#define DTP20_MAGENTA_CAL_ERROR			0x51
#define DTP20_YELLOW_CAL_ERROR			0x52
#define DTP20_PATCH_SIZE_ERROR			0x53
#define DTP20_FAIL_PAPER_CHECK			0x54
#define DTP20_SHORT_SCAN_ERROR			0x55
#define DTP20_STRIP_READ_ERROR			0x56
#define DTP20_SHORT_TID_ERROR			0x57
#define DTP20_SHORT_STRIP_ERROR			0x58
#define DTP20_EDGE_COLOR_ERROR			0x59
#define DTP20_SPEED_ERROR				0x5A
#define DTP20_UNDEFINED_SCAN_ERROR		0x5B
#define DTP20_INVALID_STRIP_ID			0x5C
#define DTP20_BAD_SERIAL_NUMBER			0x5D
#define DTP20_TID_ALREADY_SCANNED		0x5E
#define DTP20_PROFILE_DATABASE_FULL 	0x5F

#define DTP20_SPOT_DATABASE_FULL		0x60
#define DTP20_TID_STRIP_MIN_ERROR		0x61
#define DTP20_REREAD_DATABASE_FULL		0x62
#define DTP20_STRIP_DEFINE_TOO_SHORT 	0x63
#define DTP20_STRIP_DEFINE_TOO_LONG 	0x64
#define DTP20_BAD_STRIP_DEFINE			0x65

#define DTP20_BOOTLOADER_MODE			0x7F

/* DTP20 communication object */
struct _dtp20 {
	/* **** base instrument class **** */
	INST_OBJ_BASE

	/* *** DTP20 private data **** */
	inst_mode  cap;				/* Instrument mode capability */
	inst2_capability cap2;		/* Instrument capability 2 */
	inst3_capability cap3;		/* Instrument capability 3 */
	inst_mode    mode;			/* Currently instrument mode */

	int need_cal;				/* Got a need_cal error */
	inst_opt_type trig;			/* Reading trigger mode */

	int savix;					/* Index of last saved spot reading read */

}; typedef struct _dtp20 dtp20;

/* Constructor */
extern dtp20 *new_dtp20(icoms *icom, instType itype);



#define DTP20_H
#endif /* DTP20_H */
