
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#if defined(UNIX)
# include <utime.h>
#else
# include <sys/utime.h>
#endif
#include <sys/stat.h>
#include <stdarg.h>
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#else	/* !SALONEINSTLIB */
#include "sa_config.h"
#include "numsup.h"
#endif /* !SALONEINSTLIB */
#ifndef SALONEINSTLIB
#  include "plot.h"
#endif
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "inst.h"
#include "rspec.h"

#include "xrga.h"

/* A conversion equation */
typedef struct {
	double  gain0, gain1;		/* Destination gain at 550 nm, slope */
	double  wl0;				/* Wavlength shift at 550nm to source - assumed constant */
} xrga_eqn;

/* XRGA conversion values in order [pol][src][dst] */
/* Parameters are gain at 550nm, slope of gain per nm, src wavelengh offset in nm. */
/* These values were inferred from the results one gets from processing spectral */
/* values though X-Rite's conversion routines. */

xrga_eqn xrga_equations[2][3][3] = {
	{	/* Unpolarized */
		{
			{ 1.000000,  0.000000,  0.000000 },	/* XRDI -> XRDI */
			{ 1.000046, -0.000050, -1.400568 },	/* XRDI -> GMDI */
			{ 1.000005, -0.000025, -0.400084 }	/* XRDI -> XRGA */
		},
		{
			{ 1.000046, 0.000050, 1.400568 },	/* GMDI -> XRDI */
			{ 1.000000, 0.000000, 0.000000 },	/* GMDI -> GMDI */
			{ 1.000015, 0.000025, 1.000207 }	/* GMDI -> XRGA */
		},
		{
			{ 1.000005,  0.000025,  0.400084 },	/* XRGA -> XRDI */
			{ 1.000015, -0.000025, -1.000207 },	/* XRGA -> GMDI */
			{ 1.000000,  0.000000,  0.000000 }	/* XRGA -> XRGA */
		}
	},
	{	/* Polarized */
		{
			{ 1.000000,  0.000000,  0.000000 },	/* XRDI -> XRDI */
			{ 1.028710, -0.000081, -1.399477 },	/* XRDI -> GMDI */
			{ 1.000005, -0.000025, -0.400084 }	/* XRDI -> XRGA */
		},
		{
			{ 0.971957, 0.000072, 1.398829 },	/* GMDI -> XRDI */
			{ 1.000000, 0.000000, 0.000000 },	/* GMDI -> GMDI */
			{ 0.971975, 0.000049, 0.998938 }	/* GMDI -> XRGA */
		},
		{
			{ 1.000005,  0.000025,  0.400084 },	/* XRGA -> XRDI */
			{ 1.028711, -0.000056, -0.999421 },	/* XRGA -> GMDI */
			{ 1.000000,  0.000000,  0.000000 }	/* XRGA -> XRGA */
		}
	}
};


/* Core conversion code. dst and src are assumed to be different xspect's */
static void convert_xrga(xspect *dst, xspect *src, xrga_eqn *eq) {
	int j;

	XSPECT_COPY_INFO(dst, src);		/* Copy parameters */

	for (j = 0; j < dst->spec_n; j++) {
		double dw, sw, ga;
		double spcing, f;
		double y[4], yw;
		double x[4];
		int i;
	
		dw = XSPECT_XWL(dst, j);			/* Destination wavelength */
		sw = dw + eq->wl0;					/* Source wavelength */
		ga = eq->gain0 + (dw - 550.0) * eq->gain1;	/* Gain at this dest wl */ 

		/* Compute fraction 0.0 - 1.0 out of known spectrum. */
		/* Place it so that the target wavelength lands in middle section */
		/* of Lagrange basis points. */
		spcing = (src->spec_wl_long - src->spec_wl_short)/(src->spec_n-1.0);
		f = (sw - src->spec_wl_short) / (src->spec_wl_long - src->spec_wl_short);
		f *= (src->spec_n - 1.0);
		i = (int)floor(f);			/* Base grid coordinate */
	
		if (i < 1)					/* Limit to valid Lagrange basis index range, */
			i = 1;					/* and extrapolate from that at the ends. */
		else if (i > (src->spec_n - 3))
			i = (src->spec_n - 3);
	
		/* Setup the surrounding values */
		x[0] = src->spec_wl_short + (i-1) * spcing;
		y[0] = src->spec[i-1];
		x[1] = src->spec_wl_short + i * spcing;
		y[1] = src->spec[i];
		x[2] = src->spec_wl_short + (i+1) * spcing;
		y[2] = src->spec[i+1];
		x[3] = src->spec_wl_short + (i+2) * spcing;
		y[3] = src->spec[i+2];
	

		/* Compute interpolated value using Lagrange: */
		yw = y[0] * (sw-x[1]) * (sw-x[2]) * (sw-x[3])/((x[0]-x[1]) * (x[0]-x[2]) * (x[0]-x[3]))
		   + y[1] * (sw-x[0]) * (sw-x[2]) * (sw-x[3])/((x[1]-x[0]) * (x[1]-x[2]) * (x[1]-x[3]))
		   + y[2] * (sw-x[0]) * (sw-x[1]) * (sw-x[3])/((x[2]-x[0]) * (x[2]-x[1]) * (x[2]-x[3]))
		   + y[3] * (sw-x[0]) * (sw-x[1]) * (sw-x[2])/((x[3]-x[0]) * (x[3]-x[1]) * (x[3]-x[2]));

		yw *= ga;

		dst->spec[j] = yw;
	}
}

/* Apply a conversion from one calibration standard to another to an xspect */
void xspec_convert_xrga(xspect *dst, xspect *srcp, xcalpol pol, xcalstd dsp, xcalstd ssp) {
	xrga_eqn *eq;
	xspect tmp, *src = srcp;

	/* If no conversion and no copy needed */
	if (!XCALSTD_NEEDED(ssp, dsp) && dst == src)
		return;

	/* If no conversion needed  */
	if (!XCALSTD_NEEDED(ssp, dsp)) {
		*dst = *src;		/* Struct copy */
		return;
	}

	/* If the dest is the same as the src, make a temporary copy, */
	/* since convert_xrga() needs them to be distinct */
	if (dst == src) {
		tmp = *src;			/* Struct copy */
		src = &tmp;
	}

	eq = &xrga_equations[pol][ssp][dsp];
	convert_xrga(dst, src, eq);
}

/* Apply a conversion from one calibration standard to another to an array of ipatch's */
void ipatch_convert_xrga(ipatch *vals, int nvals,
                         xcalpol pol, xcalstd dsp, xcalstd ssp, int clamp) {
	xrga_eqn *eq;
	xspect tmp;
	xsp2cie *conv = NULL;	/* Spectral to XYZ conversion object */
	int i;

	/* If no conversion needed */
	if (ssp == xcalstd_native || dsp == xcalstd_native
	 || dsp == ssp || nvals <= 0)
		return;

	/* Conversion to use */
	eq = &xrga_equations[pol][ssp][dsp];

	for (i = 0; i < nvals; i++) {
		if (vals[i].mtype != inst_mrt_reflective
		 || vals[i].sp.spec_n <= 0) {
			continue;
		}
		tmp = vals[i].sp;		// Struct copy */
		convert_xrga(&vals[i].sp, &tmp, eq);

		/* Re-compute XYZ */
		if (vals[i].XYZ_v) {
			if (conv == NULL) {
				conv = new_xsp2cie(icxIT_D50, 0.0, NULL, icxOT_CIE_1931_2,
				                   NULL, icSigXYZData, (icxClamping)clamp);
			}
			conv->convert(conv, vals[i].XYZ, &vals[i].sp);
			vals[i].XYZ_v = 1;
			vals[i].XYZ[0] *= 100.0;	/* Convert to % */
			vals[i].XYZ[1] *= 100.0;
			vals[i].XYZ[2] *= 100.0;
		}
	}
	if (conv != NULL)
		conv->del(conv);
}

