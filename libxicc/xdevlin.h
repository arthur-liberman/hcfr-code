#ifndef XDEVLIN_H
#define XDEVLIN_H
/* 
 * International Color Consortium color transform expanded support
 *
 * Author:  Graeme W. Gill
 * Date:    29/8/01
 * Version: 1.00
 *
 * Copyright 2001 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/*
 * This class handles the creation of device chanel linearisation
 * curves, given a callback function that maps the device chanels
 * to the value that should be linearised.
 *
 * This class is independent of other icc or icx classes.
 *
 * Its usual use is to create an Lab linerisation curve
 * for native XYZ profiles.
 */

/* The device linearisation class */
struct _xdevlin {

	/* Private: */
	int di;					/* Device dimentionality */
	rspl *curves[MXDI];		/* di Linearisation curves */
	double clipc[MXDI];		/* center of device range */
	double min[MXDI], max[MXDI];	/* Device chanel min/max */
	int pol;				/* Polarity, 0 for minimise other chanels */
	int setch;				/* Chanel to set */
	double lmin, lmax;		/* Linear min & max to rescale */
	void *lucntx;			/* Lookup context */
	void (*lookup) (void *lucntx, double *lin, double *dev);	/* Callback function */

	/* Public: */
	void (*del)(struct _xdevlin *p);

	/* Return the linearisation values given the device values */
	void (*lin)(struct _xdevlin *p, double *out, double *in);

	/* Return the inverse linearisation */
	void (*invlin)(struct _xdevlin *p, double *out, double *in);

}; typedef struct _xdevlin xdevlin;

xdevlin *new_xdevlin(
	int di,				/* Device dimenstionality */
	double *min, double *max,	/* Min & max range of device values, NULL = 0.0 - 1.0 */ 
	void *cntx,			/* Context for callback */
	void (*lookup) (void *cntx, double lin[MXDO], double dev[MXDI])
						/* Callback function, return linear parameter as lin[0] */
);

#endif /* XDEVLIN_H */



































