
#ifndef MCV_H
#define MCV_H

/* 
 * Argyll Color Correction System
 * Monotonic curve class for display calibration.
 *
 * Author: Graeme W. Gill
 * Date:   30/10/2005
 *
 * Copyright 2005 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * This is based on the monotonic curve equations used elsewhere,
 * but currently intended to support the display calibration process.
 * moncurve is not currently general, missing:
 *
 *  input scaling
 *  output scaling
 *
 * The nominal input and output ranges are 0.0 to 1.0,
 */

/* A test patch value */
typedef struct {
	double p;		/* Position */
	double v;		/* Value */
	double w;		/* Weighting, nominally 1.0 */
} mcvco;

struct _mcv {

  /* Public: */
	void (*del)(struct _mcv *p);

	/* Fit the curve to the given points */
	void (*fit) (struct _mcv *p,
	            int verb,		/* Vebosity level, 0 = none */
	            int order,		/* Number of curve orders, 1..MCV_MAXORDER */
		        mcvco *d,		/* Array holding scattered initialisation data */
		        int ndp,		/* Number of data points */
	            double smooth	/* Degree of smoothing, 1.0 = normal */			
	);

	/* Offset the the output so that the value for input 0.0, */
	/* is the given value. Don't change the 1.0 output */
	void (*force_0) (struct _mcv *p,
	            double target	/* Target output value */
	);

	/* Scale the the output so that the value for input 1.0, */
	/* is the given value. Don't change the 0.0 output */
	void (*force_1) (struct _mcv *p,
	            double target	/* Target output value */
	);

	/* Scale the the output so that the value for input 1.0, */
	/* is the given target value. Scale the value for 0.0 too. */
	void (*force_scale) (struct _mcv *p,
	            double target	/* Target output value */
	);

	/* Return the number of parameters and the parameters in */
	/* an allocated array. free() when done. */
	/* The parameters are the offset, scale, then all the other parameters */
	int (*get_params)(struct _mcv *p, double **rp);

	/* Translate a value through the current curve */
	double (*interp) (struct _mcv *p,
	                  double in);	/* Input value */

	/* Translate a value backwards through the current curve */
	double (*inv_interp) (struct _mcv *p,
	                  double in);	/* Input value */


	/* Translate a value given the parametrs */
	double (*interp_p) (struct _mcv *p,
	                  double *pms, double in);	/* Input value */

	/* return the shaper parameters normalising weight */ 
	double (*shweight_p)(struct _mcv *p, double *v, double smooth);

	/* Translate a value given the parametrs, with partial derivatives */
	double (*dinterp_p) (struct _mcv *p, double *pms, double *dv, double vv);

	/* return the shaper parameters normalising weight, with partial derivatives */ 
	double (*dshweight_p)(struct _mcv *p, double *v, double *dv, double smooth);


  /* Private: */
	int verb;				/* Verbose */
	int noos;				/* flag, 2 = offset and scale not fitted */
	int luord;				/* Lookup order including offset and scale */
	double *pms;			/* Allocated curve parameters */
	double *dv;				/* Work space for dv's during optimisation */
	double resid;			/* Residual fit error */

	mcvco *d;				/* Array holding scattered initialisation data */
	int ndp;				/* Number of data points */
	double dra;				/* Data range */

	double smooth;			/* Smoothing factor */

}; typedef struct _mcv mcv;

/* Create a new, uninitialised mcv that will fit with offset and scale */
mcv *new_mcv(void);

/* Create a new mcv initiated with the given curve parameters */
mcv *new_mcv_p(double *pp, int np);

/* Create a new, uninitialised mcv with offset and scale not to be fitted, */
/* and defaulting to 0.0 and 1.0 */
mcv *new_mcv_noos(void);

#endif /* MCV */

