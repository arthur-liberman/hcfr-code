#ifndef COLORHUG_H

/*
 * Argyll Color Correction System
 *
 * Hughski ColorHug related defines
 *
 * Author: Richard Hughes
 * Date:   30/11/2011
 *
 * Copyright 2006 - 2007, Graeme W. Gill
 * Copyright 2011, Richard Hughes
 * All rights reserved.
 *
 * (Based on huey.h)
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#include "inst.h"

/* Note: update colorhug_interp_error() and colorhug_interp_code() in colorhug.c */
/* if anything of these #defines are added or subtracted */

/* Fake Error codes */
#define COLORHUG_INTERNAL_ERROR			0x61		/* Internal software error */
#define COLORHUG_COMS_FAIL				0x62		/* Communication failure */
#define COLORHUG_UNKNOWN_MODEL			0x63		/* Not an colorhug */
#define COLORHUG_DATA_PARSE_ERROR 		0x64		/* Read data parsing error */
#define COLORHUG_USER_ABORT				0x65		/* User hit abort */
#define COLORHUG_USER_TERM				0x66		/* User hit terminate */
#define COLORHUG_USER_TRIG				0x67		/* User hit trigger */
#define COLORHUG_USER_CMND				0x68		/* User hit command */

/* Real error codes */
#define COLORHUG_OK  					0x00
#define COLORHUG_UNKNOWN_CMD			0x01
#define COLORHUG_WRONG_UNLOCK_CODE		0x02
#define COLORHUG_NOT_IMPLEMENTED		0x03
#define COLORHUG_UNDERFLOW_SENSOR		0x04
#define COLORHUG_NO_SERIAL				0x05
#define COLORHUG_WATCHDOG				0x06
#define COLORHUG_INVALID_ADDRESS		0x07
#define COLORHUG_INVALID_LENGTH			0x08
#define COLORHUG_INVALID_CHECKSUM		0x09
#define COLORHUG_INVALID_VALUE			0x0a
#define COLORHUG_UNKNOWN_CMD_FOR_BOOTLOADER		0x0b
#define COLORHUG_NO_CALIBRATION			0x0c
#define COLORHUG_OVERFLOW_MULTIPLY		0x0d
#define COLORHUG_OVERFLOW_ADDITION		0x0e
#define COLORHUG_OVERFLOW_SENSOR		0x0f
#define COLORHUG_OVERFLOW_STACK			0x10

/* Internal errors */
#define COLORHUG_NO_COMS  				0x22
#define COLORHUG_NOT_INITED 			0x23
#define COLORHUG_BAD_WR_LENGTH			0x25
#define COLORHUG_BAD_RD_LENGTH			0x26
#define COLORHUG_BAD_RET_CMD			0x27
#define COLORHUG_BAD_RET_STAT			0x28


/* COLORHUG communication object */
struct _colorhug {
	INST_OBJ_BASE

	inst_mode mode;				/* Currently selected mode */

	inst_opt_mode trig;			/* Reading trigger mode */
	int trig_return;			/* Emit "\n" after trigger */

	int ser_no;					/* Serial number */
	int calix;					/* Calibration matrix index */
	double ccmat[3][3];			/* Colorimeter correction matrix */
	int	 led_state;				/* Current LED state */

}; typedef struct _colorhug colorhug;

/* Constructor */
extern colorhug *new_colorhug(icoms *icom, instType itype, int debug, int verb);


#define COLORHUG_H
#endif /* COLORHUG_H */