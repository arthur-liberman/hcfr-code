#ifndef I1DISP_H

/* 
 * Argyll Color Correction System
 *
 * Gretag i1Display related defines
 *
 * Author: Graeme W. Gill
 * Date:   19/10/2006
 *
 * Copyright 2006 - 2007, Graeme W. Gill
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

/* Note: update i1disp_interp_error() and i1disp_interp_code() in i1disp.c */
/* if anything of these #defines are added or subtracted */

/* Fake Error codes */
#define I1DISP_INTERNAL_ERROR			0x61		/* Internal software error */
#define I1DISP_COMS_FAIL				0x62		/* Communication failure */
#define I1DISP_UNKNOWN_MODEL			0x63		/* Not an i1display */
#define I1DISP_DATA_PARSE_ERROR  		0x64		/* Read data parsing error */
#define I1DISP_USER_ABORT			    0x65		/* User hit abort */
#define I1DISP_USER_TERM		    	0x66		/* User hit terminate */
#define I1DISP_USER_TRIG 			    0x67		/* User hit trigger */
#define I1DISP_USER_CMND		    	0x68		/* User hit command */

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


/* I1DISP communication object */
struct _i1disp {
	INST_OBJ_BASE

	int       dtype;			/* Device type: 0 = i1D1, 1 = i1D2 */	
	int       lite;				/* i1D2: 0 = normal, 1 = "Lite" */
	int       munki;			/* i1D2: 0 = normal, 1 = "Munk" */
	int       hpdream;			/* i1D2: 0 = normal, 1 = "ObiW" */
	int       calmanx2;			/* i1D2: 0 = normal, 1 = "CMX2" */
	int       chroma4;			/* 0 = other, 1 = Sequel Chroma 4 (i1D1 based) */
	inst_mode mode;				/* Currently selected mode */

	inst_opt_mode trig;			/* Reading trigger mode */
	int trig_return;			/* Emit "\n" after trigger */

	/* EEPROM registers */
	/* Number is the register address, and B, S, W, F indicate the type/size */
	int     reg0_W;				/* Serial number */

	double  reg4_F[9];			/* LCD 3x3 calibration matrix (also known as "user") */
	int     reg50_W;			/* Calibration time in secs from January 1, 1970, UTC */
	int     reg126_S;			/* LCD cal valid/state flag. For the i1disp this is 0xd, */
								/* perhaps meaning that it is the LCD matrix. */
								/* It's set to 7 after storing a user calibration. */
								/* A value of 0xffff or < 7 means that it's invalid */
								/* A value of 2 seems valid for OEM instruments */
								/* (Heidelberg Viewmaker & Lacie Blue Eye) */

	double  reg54_F[9];			/* CRT 3x3 calibration matrix (also known as "factory") */
	int     reg90_W;			/* cal valid/time flag.  0xffffffff = invalid, */
								/* time in secs from January 1, 1970, UTC */

	int     reg40_S;			/* Calibration factor, typically 1000 */
	int     reg42_S;			/* Calibration factor, typically 10000 */
	int     reg44_S[3];			/* Calibration factors, typically 100 */

	double  clk_prd;			/* Master clock period, typically aprox. 1e-6 */

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
	int     crt;				/* NZ if set to CRT */ 
	double  clk_freq;			/* Inverted reg94_F, ie master clock frequency, typically apx 1e6 */
	double  rgbadj1[3];			/* RGB adjustment values, typically 1e6 */
	double  rgbadj2[3];			/* RGB adjustment values, typically 1.0 */
	double  amb[9];				/* Ambient measurement matrix = ref144[] * average of LCD & CRT */

	double ccmat[3][3];			/* Colorimeter correction matrix */

	/* For dtype == 1 (Eye-One Display2) */
	int     itset;				/* Flag, nz if the integration time has been measured and set */
	double  sampfreq;			/* Refresh rate for i1d2, CRT default 60, LCD = 100 */
	int     sampno;				/* Number of refresh rate samples we're aiming to take, def 100 */
	double  samptime;			/* Total sample time in seconds (= sampno * sampfreq) */
	int     nmeasprds;       	/* Number of disp refresh period measurments to average, deflt 5 */
	int     int_clocks;			/* Integration time in clocks */

}; typedef struct _i1disp i1disp;

/* Constructor */
extern i1disp *new_i1disp(icoms *icom, instType itype, int debug, int verb);


#define I1DISP_H
#endif /* I1DISP_H */
