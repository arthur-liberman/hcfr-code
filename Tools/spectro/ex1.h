#ifndef EX1_H

/* 
 * Argyll Color Correction System
 *
 * Image Engineering EX1
 *
 * Author: Graeme W. Gill
 * Date:   4/4/2015
 *
 * Copyright 2001 - 2015, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Based on spebos.h
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
#define EX1_INTERNAL_ERROR			0xff01		/* Internal software error */
#define EX1_TIMEOUT				    0xff02		/* Communication timeout */
#define EX1_COMS_FAIL				0xff03		/* Communication failure */
#define EX1_UNKNOWN_MODEL			0xff04		/* Not a JETI ex1 */
#define EX1_DATA_PARSE_ERROR  		0xff05		/* Read data parsing error */


/* Real instrument error code */
#define EX1_OK						0
#define EX1_NOT_IMP					1

/* Internal software errors */
#define EX1_INT_THREADFAILED       1000

/* EX1 communication object */
struct _ex1 {
	INST_OBJ_BASE

	amutex lock;				/* Command lock (not necessary) */

	int model;					/* ex1 model number */

	inst_mode mode;				/* Currently instrument mode */

	int refrmode;				/* nz if in refresh display mode */
								/* (1201 has a refresh mode ?? but can't measure frequency) */
	int    rrset;				/* Flag, nz if the refresh rate has been determined */
	double refperiod;			/* if > 0.0 in refmode, target int time quantization */
	double refrate;				/* Measured refresh rate in Hz */
	int    refrvalid;			/* nz if refrate is valid */

	inst_opt_type trig;			/* Reading trigger mode */

	double measto;				/* Expected measurement timeout value */
	int nbands;					/* Number of spectral bands */
	double wl_short;
	double wl_long;

	/* Other state */

	}; typedef struct _ex1 ex1;

/* Constructor */
extern ex1 *new_ex1(icoms *icom, instType itype);


#define EX1_H
#endif /* EX1_H */
