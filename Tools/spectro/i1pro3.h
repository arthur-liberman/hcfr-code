#ifndef I1PRO3_H

/* 
 * Argyll Color Management System
 *
 * Gretag i1Pro related defines
 */

/*
 * Author: Graeme W. Gill
 * Date:   14/9/2020
 *
 * Copyright 2006 - 2021, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#ifdef __cplusplus
	extern "C" {
#endif

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

/* I1PRO3 communication object */
struct _i1pro3 {
	INST_OBJ_BASE

//	int       idtype;			/* Device type: 0 = ?? */	


	/* *** i1pro3 private data **** */
	inst_mode  cap;				/* Instrument mode capability */
	inst2_capability cap2;		/* Instrument capability 2 */
	inst3_capability cap3;		/* Instrument capability 3 */

	void *m;					/* Implementation - i1pro3imp type */
}; typedef struct _i1pro3 i1pro3;

/* Constructor */
extern i1pro3 *new_i1pro3(icoms *icom, instType itype);

#ifdef __cplusplus
	}
#endif

#define I1PRO3_H
#endif /* I1PRO3_H */
