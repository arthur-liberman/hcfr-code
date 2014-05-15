#ifndef I1D3_H

/* 
 * Argyll Color Correction System
 *
 * X-Rite i1d3 related defines
 *
 * Author: Graeme W. Gill
 * Date:   7/10/2007
 *
 * Copyright 2006 - 2013, Graeme W. Gill
 * All rights reserved.
 *
 * (Based on huey.h)
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
#define I1D3_INTERNAL_ERROR			0x61		/* Internal software error */
#define I1D3_COMS_FAIL				0x62		/* Communication failure */
#define I1D3_UNKNOWN_MODEL			0x63		/* Not an i1d3 */
#define I1D3_DATA_PARSE_ERROR  		0x64		/* Read data parsing error */

/* Real error code */
#define I1D3_OK   					0x00

#define I1D3_UNKNOWN_UNLOCK			0x01
#define I1D3_UNLOCK_FAIL			0x02
#define I1D3_BAD_EX_CHSUM  			0x03

#define I1D3_SPOS_EMIS				0x05		/* Needs to be in emsissive configuration */
#define I1D3_SPOS_AMB				0x06		/* Needs to be in ambient configuration */

#define I1D3_TOO_FEW_CALIBSAMP	    0x10

#define I1D3_BAD_WR_LENGTH			0x11
#define I1D3_BAD_RD_LENGTH			0x12
#define I1D3_BAD_RET_STAT			0x13
#define I1D3_BAD_RET_CMD			0x14
#define I1D3_NOT_INITED				0x15

/* Internal errors */
#define I1D3_BAD_MEM_ADDRESS	    0x20
#define I1D3_BAD_MEM_LENGTH		    0x21
#define I1D3_INT_CIECONVFAIL		0x22
#define I1D3_INT_MATINV_FAIL		0x23
#define I1D3_BAD_LED_MODE 			0x24
#define I1D3_NO_COMS				0x25
#define I1D3_BAD_STATUS				0x26
#define I1D3_INT_THREADFAILED       0x27

/* Sub-type of instrument */
typedef enum {
	i1d3_disppro    = 0,	/* i1 DisplayPro */
	i1d3_munkdisp   = 1,	/* ColorMunki Display */
	i1d3_oem        = 2,	/* OEM */
	i1d3_nec_ssp    = 3, 	/* NEC SpectraSensor Pro */
	i1d3_quato_sh3  = 4, 	/* Quato Silver Haze 3 */
	i1d3_hp_dreamc  = 5, 	/* HP DreameColor */
	i1d3_sc_c6      = 6 	/* SpectraCal C6 */
} i1d3_dtype;

/* Measurement mode */
typedef enum {
	i1d3_adaptive     = 0,	/* Frequency over fixed period then adaptive period measurement */
	i1d3_frequency    = 1,	/* Frequency over fixed period measurement */
	i1d3_period       = 2	/* Adaptive period measurement */
} i1d3_mmode;

/* I1D3 communication object */
struct _i1d3 {
	INST_OBJ_BASE

	amutex lock;				/* Command lock */

	/* Modes */
	inst_mode mode;				/* Currently selected mode */
	inst_opt_type trig;			/* Reading trigger mode */

	/* Information and EEPROM values */
	i1d3_dtype dtype;			/* Base type of instrument, ie i1d3_disppro or i1d3_munkdisp */
	i1d3_dtype stype;			/* Sub type of instrument, ie. any of i1d3_dtype. */
								/* (Only accurate if it needed unlocking). */
	int status;					/* 0 if status is ok (not sure what this is) */
	char serial_no[21];			/* "I1-11.A-01.100999.02" or "CM-11.A-01.100999.02" */
	char vers_no[11];			/* "A-01", "A-02" */
	char prod_name[32];			/* "i1Display3 " or "ColorMunki Display" */
	int prod_type;				/* 16 bit product type number. i1d3_disppro = 0x0001, */
								/* i1d3_munkdisp = 0x0002 */
	char firm_ver[32];			/* Firmwar version string. ie. "v1.0 " */
	char firm_date[32];			/* Firmwar date string. ie. "11Jan11" */

	/* Calibration information */
	ORD64 cal_date;				/* Calibration date */
	xspect sens[3];				/* RGB Sensor spectral sensitivities in Hz per mW/nm */
	xspect ambi[3];				/* RGB Sensor with ambient filter spectral sensitivities */

	double black[3];			/* Black level to subtract */
	double emis_cal[3][3];		/* Current emssion calibration matrix */
	double ambi_cal[3][3];		/* Current ambient calibration matrix */

	inst_disptypesel *dtlist;	/* Display Type list */
	int ndtlist;				/* Number of valid dtlist entries */
	int icx;					/* Internal calibration matrix index, 11 = Raw */
	int cbid;					/* calibration base ID, 0 if not a base */
	int refrmode;				/* nz if in refresh display mode/double int. time */
	icxObserverType obType;		/* ccss observer to use */
	xspect custObserver[3];		/* Custom ccss observer to use */
	double ccmat[3][3];			/* Optional colorimeter correction matrix, unity if none. */
	xspect *samples;			/* Copy of current calibration spectral samples, NULL if none */
	int nsamp;					/* Number of samples, 0 if none */

	/* Computed factors and state */
	int    rrset;				/* Flag, nz if the refresh rate has been determined */
	double refperiod;			/* if > 0.0 in refmode, target int time quantization */
	double refrate;				/* Measured refresh rate in Hz */
	int    refrvalid;			/* nz if refrate is valid */
	double clk_freq;			/* Clock frequency (12Mhz) */
	double omininttime;			/* Override minimum integration time = 0.0 = none */
	double dinttime;			/* default integration time = 0.2 seconds */
	double mininttime;			/* current minimum integration time (doubled for refresh) */
	double inttime;				/* current (quantized, doubled) integration time = 0.2 seconds */

	double transblend;			/* Blend between fixed and adaptive integration */
								/* at low light levels */

	/* Other state */
	int     led_state;			/* : Current LED on/off state */
	double	led_period, led_on_time_prop, led_trans_time_prop;	/* Pulse state */

	athread *th;                /* Diffuser position monitoring thread */
	volatile int th_en;			/* Enable updating diffuser possition */
	volatile int th_term;		/* nz to terminate thread */
	volatile int th_termed;		/* nz when thread terminated */
	int dpos;					/* Diffuser position, 0 = display, 1 = ambient */

}; typedef struct _i1d3 i1d3;

/* Constructor */
extern i1d3 *new_i1d3(icoms *icom, instType itype);


#define I1D3_H
#endif /* I1D3_H */
