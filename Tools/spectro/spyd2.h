#ifndef SPYD2_H

/* 
 * Argyll Color Correction System
 *
 * ColorVision Spyder 2 & 3 related software.
 *
 * Author: Graeme W. Gill
 * Date:   19/10/2006
 *
 * Copyright 2006 - 2009, Graeme W. Gill
 * All rights reserved.
 *
 * (Based on i1disp.c)
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

/* Note: update spyd2_interp_error() and spyd2_interp_code() in spyd2.c */
/* if anything of these #defines are added or subtracted */

/* Fake Error codes */
#define SPYD2_INTERNAL_ERROR		0x61		/* Internal software error */
#define SPYD2_COMS_FAIL				0x62		/* Communication failure */
#define SPYD2_UNKNOWN_MODEL			0x63		/* Not an spyd2lay */
#define SPYD2_DATA_PARSE_ERROR  	0x64		/* Read data parsing error */
#define SPYD2_USER_ABORT			0x65		/* User hit abort */
#define SPYD2_USER_TERM		    	0x66		/* User hit terminate */
#define SPYD2_USER_TRIG 			0x67		/* User hit trigger */
#define SPYD2_USER_CMND		    	0x68		/* User hit command */

/* Real error code */
#define SPYD2_OK   					0x00

/* Sub codes for device specific reasoning */
#define SPYD2_BADSTATUS 			0x01
#define SPYD2_PLDLOAD_FAILED	 	0x02
#define SPYD2_BADREADSIZE           0x03
#define SPYD2_TRIGTIMEOUT           0x04
#define SPYD2_OVERALLTIMEOUT        0x05
#define SPYD2_BAD_EE_CRC	        0x06

/* Internal software errors */
#define SPYD2_BAD_EE_ADDRESS	    0x21
#define SPYD2_BAD_EE_SIZE		    0x22
#define SPYD2_NO_PLD_PATTERN	    0x23
#define SPYD2_NO_COMS  		        0x24
#define SPYD2_NOT_INITED  		    0x25
#define SPYD2_NOCRTCAL  		    0x26		/* No CRT calibration data */
#define SPYD2_NOLCDCAL  		    0x27		/* No CRT calibration data */
#define SPYD2_MALLOC     		    0x28
#define SPYD2_OBS_SELECT   		    0x29		/* Observer */
#define SPYD2_CAL_FAIL   		    0x2A
#define SPYD2_TOO_FEW_CALIBSAMP     0x2B
#define SPYD2_INT_CIECONVFAIL       0x2C

/* Configuration */
#define SPYD2_DISP_SEL_RANGE  	    0x40		/* Calibration selection is out of range */

/* SPYD2/3 communication object */
struct _spyd2 {
	INST_OBJ_BASE

	inst_mode mode;				/* Currently selected mode */

	inst_opt_mode trig;			/* Reading trigger mode */
	int trig_return;			/* Emit "\n" after trigger */

	/* Serial EEPROM registers */
								/* versioni & feature bits */
								/* Spyder2         = 0x0307 */
								/* Spyder3 Express = 0x040f */
								/* Spyder3 Pro     = 0x0407 */
								/* Spyder3 Elite   = 0x0407 */
								/* Spyder4 Pro     = 0x070F */

	unsigned int hwver;			/* 5:B	Harware version number */

	unsigned int fbits;			/* 6:B Feature bits 0,1,2,3 correspond to calibration types */
								/* CRT/UNK, LCD/NORM, TOK, CRT/UNK  */

	char    serno[9];			/* 8:8xB  Serial number as zero terminated string */

								/* Spyder2: [0][][] = CRT, [1][][] = LCD */
								/* Spyder3: [0][][] = UNK, [1][][] = CRT & LCD */
					
	/* hwver 3..6 uses these calibrations */
	double  cal_A[2][3][9];		/* HW3..6: 16, 256  CRT/LCD A calibration matrix */
	double  cal_B[2][3][9];		/* HW3..6: 128, 384 CRT/LCD B calibration matrix */

								/* HW7: cal_A[1] computed from sensor spectral data. */
								/* HWy: cal_B[1] 60, 384 Linearity correction */

								/* The first (A) 3x9 is a sensor to XYZ transform. */
								/* cal[0] is an offset value, while the */
								/* remaining 8 entries are the sensor weightings. */
								/* Because there are only 7 real sensors, cal[1] */
								/* and sensor[0] are skipped. */

								/* The second (B) 3x9 is an additional non-linearity */
								/* correction matrix, applied to the XYZ created */
								/* from the first 3x9. The cooeficients consist */
								/* of the weights for each product, ie: */
								/* X, Y, Z, X*X, X*Z, Y*Z, X*X, Y*Y, Z*Z */
								/* for each corrected output XYZ */

	double cal_F[7];			/* 240:4, 364:3  F calibration vector */ 
								/* This might be Y only weightings for the 7 sensor values, */
								/* with no offset value (TOK type ?). */

	/* hwver 7 (Spyder 4) uses computed calibrations */
//	double sens[7][41];			/* Sensor sensitivity curves in Hz per mW/nm/m^2 */
	xspect sens[7];				/* Sensor sensitivity curves in Hz per mW/nm/m^2 */

	/* Computed factors and state */
	int prevraw[8];				/* Previous raw reading values */
	int prevrawinv;				/* Previous raw readings invalid flag - after an abort */
	int     ref;				/* 0 for constant, 1 for refresh display */ 
	int     rrset;				/* Flag, nz if the refresh rate has been determined */
	double  rrate;				/* Current refresh rate. Set to DEFREFR if !determined */
	int     calix;				/* Cal table index */
	int     calix4;				/* Spyder 4 spectral cal index, last calix4, 0..spyd4_nocals-1 */
	double  gain;				/* hwver == 5 gain value (default 4) */

	double ccmat[3][3];			/* Colorimeter correction matrix */

	/* Other state */
	int     led_state;			/* Spyder 3: Current LED state */
	double	led_period, led_on_time_prop, led_trans_time_prop;	/* Spyder 3: Pulse state */

}; typedef struct _spyd2 spyd2;

/* Constructor */
extern spyd2 *new_spyd2(icoms *icom, instType itype, int debug, int verb);


#define SPYD2_H
#endif /* SPYD2_H */
