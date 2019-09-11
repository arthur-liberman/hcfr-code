#ifndef I1DISP_H

/* 
 * Argyll Color Correction System
 *
 * Gretag i1Display related defines
 *
 * Author: Graeme W. Gill
 * Date:   19/10/2006
 *
 * Copyright 2006 - 2013, Graeme W. Gill
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

/* Note: update i1disp_interp_error() and i1disp_interp_code() in i1disp.c */
/* if anything of these #defines are added or subtracted */

/* Fake Error codes */
#define I1DISP_INTERNAL_ERROR			0x61		/* Internal software error */
#define I1DISP_COMS_FAIL				0x62		/* Communication failure */
#define I1DISP_UNKNOWN_MODEL			0x63		/* Not an i1display */
#define I1DISP_DATA_PARSE_ERROR  		0x64		/* Read data parsing error */

/* Real error code */
#define I1DISP_OK   					0x00

/* Sub codes for device specific reasoning */
#define I1DISP_FLOAT_NOT_SET			0x01
#define I1DISP_NOT_READY 			    0x02

#define I1DISP_BAD_SERIAL_NUMBER		0x03
#define I1DISP_BAD_LCD_CALIBRATION		0x04
#define I1DISP_BAD_CRT_CALIBRATION		0x05
#define I1DISP_EEPROM_WRITE_FAIL   	    0x06

#define I1DISP_UNEXPECTED_RET_SIZE	    0x07
#define I1DISP_UNEXPECTED_RET_VAL	    0x08

#define I1DISP_BAD_STATUS        	    0x09
#define I1DISP_UNKNOWN_VERS_ID   	    0x10

/* Internal errors */
#define I1DISP_BAD_REG_ADDRESS	        0x20
#define I1DISP_BAD_INT_THRESH	        0x21
#define I1DISP_NO_COMS   		        0x22
#define I1DISP_NOT_INITED  		        0x23
#define I1DISP_CANT_BLACK_CALIB         0x24
#define I1DISP_CANT_MEASP_CALIB         0x25
#define I1DISP_WRONG_DEVICE             0x26
#define I1DISP_LOCKED            	    0x27

/* Sub-type of instrument (i.e. based on vers, char code, unlock code) */
typedef enum {
	i1d2_unkn       = -1,
	i1d2_norm       = 0,	/* Normal (i1d1, i1d2, Smile) */
	i1d2_lite       = 1,	/* "Lite" */
	i1d2_munki      = 2,	/* "Munk" */
	i1d2_hpdream    = 3,	/* "ObiW" */
	i1d1_calmanx2   = 4,	/* "CMX2" */
	i1d1_chroma4    = 5,	/* Chroma 4 */
	i1d1_chroma5    = 6,	/* Chroma 5 */
	i1d1_sencoreIII = 7, 	/* Sencore ColorPro III */
	i1d1_sencoreIV  = 8, 	/* Sencore ColorPro IV */
	i1d1_sencoreV   = 9 	/* Sencore ColorPro V */
} i1d2_stype;


/* I1DISP communication object */
struct _i1disp {
	INST_OBJ_BASE

	int        btype;			/* Device type: 0 = i1D1, 1 = i1D2, 2 = Smile */	
	i1d2_stype stype;			/* Sub type */

	inst_mode mode;				/* Currently selected mode */

	inst_opt_type trig;			/* Reading trigger mode */

	/* EEPROM registers */
	/* Number is the register address, and B, S, W, F indicate the type/size */
	int     reg0_W;				/* Serial number */

	double  reg4_F[9];			/* LCD 3x3 calibration matrix (also known as "user") */
								/* Smile LED backlight */
	int     reg50_W;			/* Calibration time in secs from January 1, 1970, UTC */
	int     reg126_S;			/* LCD cal valid/state flag. For the i1disp this is 0xd, */
								/* perhaps meaning that it is the LCD matrix. */
								/* It's set to 7 after storing a user calibration. */
								/* A value of 0xffff or < 7 means that it's invalid */
								/* A value of 2 seems valid for OEM instruments */
								/* (Heidelberg Viewmaker & Lacie Blue Eye) */

	double  reg54_F[9];			/* CRT 3x3 calibration matrix (also known as "factory") */
								/* Smile CCFL backlight */
	int     reg90_W;			/* CRT cal valid/time flag.  0xffffffff = invalid, */
								/* time in secs from January 1, 1970, UTC */

	int     reg40_S;			/* Integration clock perod in nsec reg40S, typically 1000 */
	int     reg42_S;			/* Int cal. factor denominator, typically 10000 */
	int     reg44_S[3];			/* Int cal. factors numerator/100, typically 100 */

	double  clk_prd;			/* Master clock period, reg94F, typically 1e-6 */

	int     reg98_W;			/* A time value. Date of manufacture ? */
								/* Reg 40-44 write date ? */

	int     reg102_B;			/* Not used ? */

	double  reg103_F[3];		/* Dark current calibration values */
								/* Not valid if reg126_S < 0xd ?? */

	int     reg115_B;			/* Unknown */
	int     reg121_B;			/* Device ID character */

	char    reg122_B[5];		/* Unlock string */

	/* Extra registers for dtype == 1 (Eye-One Display2) */
//	double  reg128_F[3];		/* Not used at all */
	double  reg144_F[3];		/* Ambient matrix adjustment values */
								/* ??? Default to 1.0 if not set in EEPROM */

	/* Computed factors and state */
	double  iclk_freq;			/* Integration clock (from reg40_S), typically 1e6 */
	double  clk_freq;			/* Measurement clock (from reg94_F), typically 1e6 */
	double  rgbadj[3];			/* RGB adjustment values for period meas., typically 1.0 */
	double  amb[9];				/* Ambient measurement matrix = ref144[] * average of LCD & CRT */

	inst_disptypesel *_dtlist;	/* Base list */
	inst_disptypesel *dtlist;	/* Display Type list */
	int     ndtlist;			/* Number of valid dtlist entries */
	int     icx;				/* 0 = LCD, 1 = CRT/CCFL matrix */
	disptech dtech;				/* Display technology enum */
	int     cbid;				/* current calibration base ID, 0 if not a base */
	int     ucbid;				/* Underlying base ID if being used for matrix, 0 othewise */
	double  ccmat[3][3];		/* Colorimeter correction matrix */

	/* For dtype == 1 (Eye-One Display2) */
	int     nmeasprds;       	/* Number of disp refresh period measurments to average, deflt 5 */
	int     refrmode;			/* 0 for constant, 1 for refresh display */ 
	int     rrset;				/* Flag, nz if the refresh rate has been determined */
	double  refperiod;          /* if > 0.0 in refmode, target int time quantization */
	double  refrate;			/* Measured refresh rate in Hz */
	int     refrvalid;			/* nz if refrate is valid */

	double dinttime;			/* default integration time = 1.1 seconds */
	double inttime;				/* current integration time = 1.0 seconds */

	int     int_clocks;			/* Currently set integration time in clocks */

	/* For dtype == 2 (ColorMunki Smile) */
	char serno[20];				/* Ascii serial number */

	/* misc */
	int 	last_com_err;		/* Last icoms error code */

}; typedef struct _i1disp i1disp;

/* Constructor */
extern i1disp *new_i1disp(icoms *icom, instType itype);

#ifdef __cplusplus
	}
#endif

#define I1DISP_H
#endif /* I1DISP_H */
