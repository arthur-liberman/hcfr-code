#ifndef SPECBOS_H

/* 
 * Argyll Color Management System
 *
 * JETI specbos related defines
 *
 * Author: Graeme W. Gill
 * Date:   13/3/2013
 *
 * Copyright 2001 - 2013, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Based on DTP02.h
 */

/* 
   If you make use of the instrument driver code here, please note
   that it is the author(s) of the code who are responsibility
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

/* Fake Error codes */
#define SPECBOS_INTERNAL_ERROR			0xff01		/* Internal software error */
#define SPECBOS_TIMEOUT				    0xff02		/* Communication timeout */
#define SPECBOS_COMS_FAIL				0xff03		/* Communication failure */
#define SPECBOS_UNKNOWN_MODEL			0xff04		/* Not a JETI specbos */
#define SPECBOS_DATA_PARSE_ERROR  		0xff05		/* Read data parsing error */

#define SPECBOS_SPOS_EMIS				0xff06		/* Needs to be in emsissive configuration */
#define SPECBOS_SPOS_AMB				0xff07		/* Needs to be in ambient configuration */

/* Real instrument error code */
#define SPECBOS_OK						0

#define SPECBOS_COMMAND					4
#define SPECBOS_PASSWORD				7
#define SPECBOS_DIGIT					8
#define SPECBOS_ARG_1					10
#define SPECBOS_ARG_2					11
#define SPECBOS_ARG_3					12
#define SPECBOS_ARG_4					13
#define SPECBOS_PARAM					20
#define SPECBOS_CONFIG_ARG				21
#define SPECBOS_CONTROL_ARG				22
#define SPECBOS_READ_ARG				23
#define SPECBOS_FETCH_ARG				24
#define SPECBOS_MEAS_ARG				25
#define SPECBOS_CALC_ARG				26
#define SPECBOS_CAL_ARG					27
#define SPECBOS_PARAM_CHSUM				101
#define SPECBOS_USERFILE_CHSUM			102
#define SPECBOS_USERFILE2_CHSUM			103
#define SPECBOS_USERFILE2_ARG			104
#define SPECBOS_OVEREXPOSED				120
#define SPECBOS_UNDEREXPOSED			121
#define SPECBOS_ADAPT_INT_TIME			123
#define SPECBOS_NO_SHUTTER				130
#define SPECBOS_NO_DARK_MEAS			131
#define SPECBOS_NO_REF_MEAS				132
#define SPECBOS_NO_TRANS_MEAS			133
#define SPECBOS_NO_RADMTRC_CALC			134
#define SPECBOS_NO_CCT_CALC				135
#define SPECBOS_NO_CRI_CALC				136
#define SPECBOS_NO_DARK_COMP			137
#define SPECBOS_NO_LIGHT_MEAS			138
#define SPECBOS_NO_PEAK_CALC			139
#define SPECBOS_CAL_DATA				140
#define SPECBOS_EXCEED_CAL_WL			141
#define SPECBOS_SCAN_BREAK				147
#define SPECBOS_TO_CYC_OPT_TRIG			160
#define SPECBOS_DIV_CYC_TIME			161
#define SPECBOS_WRITE_FLASH_PARM		170
#define SPECBOS_READ_FLASH_PARM			171
#define SPECBOS_FLASH_ERASE				172
#define SPECBOS_NO_CALIB_FILE			180
#define SPECBOS_CALIB_FILE_HEADER		181
#define SPECBOS_WRITE_CALIB_FILE		182
#define SPECBOS_CALIB_FILE_VALS			183
#define SPECBOS_CALIB_FILE_NO			184
#define SPECBOS_CLEAR_CALIB_FILE		186
#define SPECBOS_CLEAR_CALIB_ARG			187
#define SPECBOS_NO_LAMP_FILE			190
#define SPECBOS_LAMP_FILE_HEADER		191
#define SPECBOS_WRITE_LAMP_FILE			192
#define SPECBOS_LAMP_FILE_VALS			193
#define SPECBOS_LAMP_FILE_NO			194
#define SPECBOS_CLEAR_LAMP_FILE			196
#define SPECBOS_CLEAR_LAMP_FILE_ARG		197
#define SPECBOS_RAM_CHECK				200
#define SPECBOS_DATA_OUTPUT				220
#define SPECBOS_RAM_LOW					225
#define SPECBOS_FIRST_MEM_ALLOC			230
#define SPECBOS_SECOND_MEM_ALLOC		231
#define SPECBOS_THIRD_MEM_ALLOC			232
#define SPECBOS_RADMTRC_WL_RANGE		251
#define SPECBOS_BOOT_BAT_POWER			280
#define SPECBOS_TRIG_CONF_1				500
#define SPECBOS_TRIG_CONF_2				501

/* Internal software errors */
#define SPECBOS_INT_THREADFAILED       1000

/* SPECBOS communication object */
struct _specbos {
	INST_OBJ_BASE

	int bt;						/* Bluetooth coms rather than USB/serial flag */

	amutex lock;				/* Command lock */

	int model;					/* JETI specbos/spectraval model number */
								/* 1201 */
								/* 1211 */
								/* 1501 */
								/* 1511 - has display */

	int noXYZ;					/* nz if firmware doesn't support fetch*XYZ */
	int badCal;					/* nz if its been calibrated with a reduced WL range by 3rd party */

	inst_mode mode;				/* Currently instrument mode */

	int refrmode;				/* nz if in refresh display mode */
								/* (1201 has a refresh mode ?? but can't measure frequency) */
	int    rrset;				/* Flag, nz if the refresh rate has been determined */
	double refperiod;			/* if > 0.0 in refmode, target int time quantization */
	double refrate;				/* Measured refresh rate in Hz */
	int    refrvalid;			/* nz if refrate is valid */

	inst_opt_type trig;			/* Reading trigger mode */

	double measto;				/* Measurement maximum time target value */
	int nbands;					/* Number of spectral bands */
	double wl_short;
	double wl_long;

	xspect trans_white;			/* Synthetic transmission mode white reference */
	xsp2cie *conv;				/* transmission spectral to XYZ conversion */
	int doing_cal;				/* Flag - doing internal calibration measure */

	/* Other state */
	int noaverage;				/* Current setting of number of measurements to average */
								/* 0 for default */

	int setnav;					/* number of averages set in the instrument */

	athread *th;                /* Diffuser position monitoring thread */
	volatile int th_term;		/* nz to terminate thread */
	volatile int th_termed;		/* nz when thread terminated */
	int dpos;					/* Diffuser position, 0 = emissive, 1 = ambient */
	int laser;					/* Target laser state, nz = on */

	int maxtin_warn;			/* NZ if conf:maxtin failure warning has been given */

	int serno;					/* Spectrometer serial number */

	}; typedef struct _specbos specbos;

/* Constructor */
extern specbos *new_specbos(icoms *icom, instType itype);

#ifdef __cplusplus
	}
#endif

#define SPECBOS_H
#endif /* SPECBOS_H */
