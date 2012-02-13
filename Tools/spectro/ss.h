
#ifndef SS_H

/* 
 * Argyll Color Correction System
 *
 * Gretag Spectrolino and Spectroscan related
 * defines and declarations.
 *
 * Author: Graeme W. Gill
 * Date:   13/7/2005
 *
 * Copyright 2005 - 2007 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Derived from DTP41.h
 *
 * This is an alternative driver to spm/gretag.
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

#undef EMSST		/* Debug - emulate a SpectroScanT with a SpectroScan */

#include "inst.h"
#include "ss_imp.h"

#define SS_MAX_WR_SIZE 1000	/* Assumed maximum normal message query size */
#define SS_MAX_RD_SIZE 1000	/* Assumed maximum normal messagle answer size */

/* Gretag Spectrolino/Spectroscan communication object */
struct _ss {
	/* **** base instrument class **** */
	INST_OBJ_BASE

	/* *** Spectroscan/lino private data **** */
	inst_capability  cap;		/* Instrument capability */
	inst2_capability cap2;		/* Instrument capability 2 */
	inst_mode	nextmode;		/* Next requested mode */
	inst_mode	mode;			/* Currently instrument mode */

	/* Desired measurement configuration */
	ss_aft     filt;			/* Filter type (None/UV/D65 etc.) */
	ss_dst     dstd;			/* Density standard (ANSI A/ANSI T/DIN etc.) */
	ss_ilt     illum;			/* Illuminant type (A/C/D50 etc.) */
	ss_ot      obsv;			/* Observer type (2deg/10deg) */
	ss_wbt     wbase;			/* White base type (Paper/Abs>) */
	ss_ctt     phmode;			/* Photometric mode (Absolute/Relative) */
	double     phref;			/* Photometric reference (cd/m^2) */

	int		calcount;			/* Calibration needed counter */
	int     pisrow;             /* patches in a reading direction serpentine rows */
	int		need_w_cal;			/* White/dark calibration needed flag */
	int		need_t_cal;			/* Transmission calibration needed flag */
	int     noautocalib;		/* Don't mode change or auto calibrate */
	int     forcecalib;			/* Force a calibration even if not near white tile */
	inst_opt_mode trig;			/* Reading trigger mode */
	int trig_return;			/* Emit "\n" after trigger */
	int     offline;			/* nz if we're off line */

	/* Emulated manual transmission mode support */
	double tref[36];			/* White reference spectrum */
	double cill[36];			/* Colorimetry illuminant */

	/* telescopic adapter compensation */
	int compen;					/* Compensation filter enabled */
	double comp[36];			/* Compensation filter */

#ifdef EMSST
	int tmode;					/* Transmission mode */
	ss_rt sbr;					/* Standby reference */
	double sbx, sby;			/* Standby location */
#endif

	/* Serialisation support: */

	/* Send buffer */
	char _sbuf[SS_MAX_WR_SIZE];	/* Buffer allocation */
	char *sbufe;				/* Pointer to last valid location in _sbuf[] */
	char *sbuf;					/* Next character output */

	/* Receive buffer */
	char _rbuf[SS_MAX_RD_SIZE];	/* Buffer allocation */
	char *rbufe;				/* Pointer to last valid location in _rbuf[] */
	char *rbuf;					/* Next character output */

	/* Accumulated device error */
	ss_et snerr;

	}; typedef struct _ss ss;

/* Constructor */
extern ss *new_ss(icoms *icom, int debug, int verb);

#define SS_H
#endif /* SS_H */
