
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

/*
 * This module provides device gamut boundary functionality used to
 * locate the non-monotonic device boundary surface, enabling
 * fast and accurate PCS gamut boundary plotting as well (for CMYK)
 * as allowing the location of device boundary overlaps, so that they
 * can be eliminated. This also allows rapid K min/max finding,
 * and rapid smoothed K map creation. 
 */

#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#ifdef __sun
#include <unistd.h>
#endif
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "icc.h"
#include "rspl.h"
#include "xicc.h"
#include "plot.h"
#include "xdgb.h"
#include "sort.h"


/*
 * TTBD:
 *
 */

#undef DEBUG 			/* Verbose debug information */

/* - - - - - - - - - - - - - - - - - */

/* Diagnostic */
static void plot_xdgb(
xdgb *p
) {
}

/* - - - - - - - - - */

/* - - - - - - - - - */
/* create the device surface points. */
/* return nz on error */
int xdgb_fit(
	struct _xdgb *p, 
	int flags,				/* Flag values */
	int di,					/* Input dimensions */
	int fdi					/* Output dimensions */
) {
	int i, e, f;
	double *b;				/* Base of parameters for this section */
	int poff;

	p->flags  = flags;
	if (flags & XDGB_VERB)
		p->verb = 1;
	else
		p->verb = 0;
	p->di      = di;
	p->fdi     = fdi;

#ifdef DEBUG
	printf("xdgbc called with flags = 0x%x, di = %d, fdi = %d\n",flags,di,fdi);
#endif

	return 0;
}

/* We're done with an xdgb */
static void xdgb_del(xdgb *p) {
	free(p);
}

/* Create a transform fitting object */
/* return NULL on error */
xdgb *new_xdgb(
) {
	xdgb *p;

	if ((p = (xdgb *)calloc(1, sizeof(xdgb))) == NULL) {
		return NULL;
	}

	/* Set method pointers */
	p->del = xdgb_del;

	return p;
}



