#ifndef HUEY_H

/* 
 * Argyll Color Correction System
 *
 * GretagMacbeth Huey related defines
 *
 * Author: Graeme W. Gill
 * Date:   7/10/2007
 *
 * Copyright 2006 - 2013, Graeme W. Gill
 * All rights reserved.
 *
 * (Based on i1disp.h)
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

/* Note: update huey_interp_error() and huey_interp_code() in huey.c */
/* if anything of these #defines are added or subtracted */

/* Fake Error codes */
#define HUEY_INTERNAL_ERROR			0x61		/* Internal software error */
#define HUEY_COMS_FAIL				0x62		/* Communication failure */
#define HUEY_UNKNOWN_MODEL			0x63		/* Not an huey */
#define HUEY_DATA_PARSE_ERROR  		0x64		/* Read data parsing error */

/* Real error code */
#define HUEY_OK   					0x00

/* Sub codes for device specific reasoning */
#define HUEY_FLOAT_NOT_SET			0x01
#define HUEY_NOT_READY 			    0x02

#define HUEY_BAD_SERIAL_NUMBER		0x03
#define HUEY_BAD_LCD_CALIBRATION	0x04
#define HUEY_BAD_CRT_CALIBRATION	0x05
#define HUEY_EEPROM_WRITE_FAIL   	0x06

#define HUEY_BAD_WR_LENGTH	        0x07
#define HUEY_BAD_RD_LENGTH	        0x08
#define HUEY_BAD_RET_CMD	        0x09
#define HUEY_BAD_RET_STAT	        0x0A
#define HUEY_UNEXPECTED_RET_VAL	    0x0B

#define HUEY_BAD_STATUS        	    0x0C
#define HUEY_UNKNOWN_VERS_ID   	    0x0D

#define HUEY_BAD_COMMAND   	        0x0E		/* Command isn't recognised by instrument */

/* Internal errors */
#define HUEY_BAD_REG_ADDRESS	    0x20
#define HUEY_BAD_INT_THRESH	        0x21
#define HUEY_NO_COMS   		        0x22
#define HUEY_NOT_INITED  		    0x23
#define HUEY_CANT_BLACK_CALIB       0x24
#define HUEY_CANT_MEASP_CALIB       0x25
#define HUEY_WRONG_DEVICE           0x26


/* HUEY communication object */
struct _huey {
	INST_OBJ_BASE

	int      lenovo;			/* 0 = normal, 1 = 'huyL' */

	inst_mode mode;				/* Currently selected mode */

	inst_opt_type trig;			/* Reading trigger mode */

	/* EEPROM registers */
	/* Number is the register address, and B, S, W, F indicate the type/size */
	int     ser_no;				/* Serial number */

	double  LCD_cal[9];			/* LCD 3x3 calibration matrix */
	int     LCD_caltime;		/* Calibration time in secs from January 1, 1970, UTC */

	double  CRT_cal[9];			/* CRT 3x3 calibration matrix */
	int     CRT_caltime;		/* cal valid/time flag.  0xffffffff = invalid ?, */
								/* time in secs from January 1, 1970, UTC */

	double  clk_prd;			/* Master clock period, hard coded to 1e-6 */

	double  dark_cal[3];		/* Dark current calibration values */

	char    unlk_string[5];		/* Unlock string */

	double  amb_cal;			/* Ambient calibration value */

	/* Computed factors and state */
	double  clk_freq;			/* Inverted clk_prd, ie master clock frequency, typically apx 1e6 */

	int     sampno;				/* Number of CRT samples we're aiming to take, def 100 */
	int     int_clocks;			/* Integration time in clocks */

	inst_disptypesel *dtlist;	/* Display Type list */
	int     ndtlist;			/* Number of valid dtlist entries */
	int     icx;				/* 0 = LCD, 1 = CRT matrix */
	int     cbid;				/* calibration base ID, 0 if not a base */
	int     refrmode;			/* Refresh mode (always 0) */
	double  ccmat[3][3];		/* Colorimeter correction matrix */

	/* Other state */
	int     led_state;			/* Current LED state */

}; typedef struct _huey huey;

/* Constructor */
extern huey *new_huey(icoms *icom, instType itype);


#define HUEY_H
#endif /* HUEY_H */
