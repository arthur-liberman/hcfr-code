
#ifndef XDGB_H
#define XDGB_H

/*
 * International Color Consortium color transform expanded support
 * eXpanded Device Gamut Boundary support.
 *
 * Author:  Graeme W. Gill
 * Date:    2008/11/17
 * Version: 1.00
 *
 * Copyright 2008 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* Flag values */
#define XDGB_VERB          0x0001		/* Verbose output during fitting */


/* Object holding device gamut representation */
struct _xdgb {
	int verb;				/* Verbose */
	int flags;				/* Behaviour flags */
	int di, fdi;			/* Dimensionaluty of input and output */

	/* Methods */
	void (*del)(struct _xdgb *p);

}; typedef struct _xdgb xdgb;

xdgb *new_xdgb();

#endif /* XDGB_H */



































