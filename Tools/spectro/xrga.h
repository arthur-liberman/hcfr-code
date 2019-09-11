
#ifndef XRGA_H
#define XRGA_H

/*
 * This file contains resources to translate colors to/from
 * X-Rites XRGA calibration standard. This only applies to
 * reflective measurements from historical Gretag-Macbeth & X-Rite
 * instruments, and current X-Rite instruments.
 */

/* 
 * Author:  Graeme W. Gill
 * Date:    9/2/2016
 * Version: 1.00
 *
 * Copyright 2016 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum {
	xcalstd_nonpol = 0,	/* Unpolarized */
	xcalstd_pol    = 1	/* Polarized */
} xcalpol;

/* Apply a conversion from one calibration standard to another to an xspect. */
void xspec_convert_xrga(xspect *dst, xspect *srcp, xcalpol pol, xcalstd dsp, xcalstd ssp);

/* Apply a conversion from one calibration standard to another to an array of ipatch's */
void ipatch_convert_xrga(ipatch *vals, int nvals,
                         xcalpol pol, xcalstd dsp, xcalstd ssp, int clamp);

/* Macro returns true if a conversion is needed */
#define XCALSTD_NEEDED(ssp, dsp) \
	((ssp) != xcalstd_native && (dsp) != xcalstd_native && (dsp) != (ssp))

#ifdef __cplusplus
	}
#endif

#endif /* XRGA_H */






































