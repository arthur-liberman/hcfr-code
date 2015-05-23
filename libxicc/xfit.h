
#ifndef XFIT_H
#define XFIT_H

/* 
 * Clut per channel curve fitting
 *
 * Author:  Graeme W. Gill
 * Date:    27/5/2007
 * Version: 1.00
 *
 * Copyright 2000 - 2007 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Based on the rspl.c and xlut.c code.
 */

#define MXLUORD 60		/* Maximum possible shaper harmonic orders to use */

#define PCSD 3			/* PCS dimensions */

#define MXPARMS (2 * MXDI * MXLUORD + (1 << MXDI) * MXDO + MXDO * MXLUORD)

#define MAX_NTP 11		/* Maximum test points within spans */

/* Optimisation mask legal combinations */
typedef enum {
	oc_i   = 1,			/* Input Shaper curve */
	oc_p   = 2,			/* Input Positioning curve */
	oc_m   = 4,			/* Temporary matrix */
	oc_o   = 8,			/* Output */

	oc_io  = oc_i + oc_o,
	oc_im  = oc_i + oc_m,
	oc_mo  = oc_m + oc_o,
	oc_imo = oc_i + oc_m + oc_o,

	oc_pm  = oc_p + oc_m,
	oc_pmo = oc_p + oc_m + oc_o,

	oc_ip  = oc_i + oc_p,
	oc_ipo = oc_i + oc_p + oc_o
} optcomb;

/* Flag values */
#define XFIT_VERB          0x0001		/* Verbose output during fitting */

#define XFIT_FM_INPUT      0x0002		/* Use input space for fit metric (default is output) */

#define XFIT_IN_ZERO       0x0004		/* Adjust input curves 1 & 2 for zero (~~not impd. yet) */
#define XFIT_OPTGRID_RANGE 0x0008		/* Optimize inner grid around used range (~~not impd. yet) */

#define XFIT_OUT_WP_REL    0x0010		/* Extract the white point and make output relative */
#define XFIT_OUT_WP_REL_US 0x0030		/* Same as above but scale to avoid clipping above WP */
#define XFIT_OUT_WP_REL_C  0x0050		/* Same as above but clip any cLUT values over D50 */
#define XFIT_CLIP_WP       0x0080		/* Clip white point to have Y <= 1.0 (conflict with above) */
#define XFIT_OUT_LAB       0x0100		/* Output space is LAB else XYZ for reading WP */

#define XFIT_OUT_ZERO      0x0200		/* Adjust output curves 1 & 2 for zero */

#define XFIT_MAKE_CLUT     0x1000		/* Create rspl clut, even if not needed */


/* xfit reverse transform information for each test point */
typedef struct {
	double ide[MXDO][MXDI];	/* pseudo inverse: rout -> in */
} xfit_piv;

/* Context for optimising input and output luts */
struct _xfit {
	icc *picc;				/* ICC profile used to set cone space matrix, NULL for Bradford. */
	int verb;				/* Verbose */
	int flags;				/* Behaviour flags */
	int di, fdi;			/* Dimensionaluty of input and output */
	optcomb tcomb;			/* Target 1D curve elements to fit */

	icxMatrixModel *skm;	/* Optional skeleton model (used for input profiles) */

	double *wp;				/* Ref. to current white point if XFIT_OUT_WP_REL */
	double *dw;				/* Ref. to device value that should map to D50 if XFIT_OUT_WP_REL */
	double fromAbs[3][3];	/* From abs to relative (used by caller) */
	double toAbs[3][3];		/* To abs from relative (used by caller) */
	int gres[MXDI];			/* clut resolutions being optimised for */
	rspl *clut;				/* final rspl clut */

	void *cntx2;			/* Context of callback */
	double (*to_de2)(void *cntx, double *in1, double *in2);	
							/* callback to convert in or out value to fit metric squared */
	double (*to_dde2)(void *cntx, double dout[2][MXDIDO], double *in1, double *in2);
							/* Same, but with partial derivatives */

	int iluord[MXDI];		/* Input Shaper order actualy used (must be <= MXLUORD) */
	int sm_iluord;			/* Smallest Input Shaper order used */
	int oluord[MXDO];		/* Output Shaper order actualy used (must be <= MXLUORD) */
	int sluord[MXDI];		/* Sub-grid shaper order */
	double in_min[MXDI];	/* Input value scaling minimum */
	double in_max[MXDI];	/* Input value scaling maximum */
	double out_min[MXDO];	/* Output value scaling minimum */
	double out_max[MXDO];	/* Output value scaling maximum */

	int shp_off;			/* Input  parameters offset */
	int shp_offs[MXDI];		/* Input  parameter offsets for each in channel from v[0] */
	int shp_cnt;			/* Input  parameters count */
	int mat_off;			/* Matrix parameters offset from v[0] */
	int mat_offs[MXDO];		/* Matrix  parameter offsets for each out channel from v[0] */
	int mat_cnt;			/* Matrix parameters count */
	int out_off;			/* Output parameters offset from v[0] */
	int out_offs[MXDO];		/* Output  parameter offsets for each out channel from v[0] */
	int out_cnt;			/* Output parameters count */
	int pos_off;			/* Position parameters offset */
	int pos_offs[MXDI];		/* Position parameter offsets for each in channel from v[0] */
	int pos_cnt;			/* Position parameters count */
	int tot_cnt;			/* Total parameter count */

	double *v;				/* Holder for parameters */
							/* Optimisation parameters are layed out:             */
							/*                                                    */
							/* Input pos or shape curves:, di groups of iluord[e] parameters   */
							/*                                                    */
							/* (temp) Matrix: fdi groups of 2 ^ di parameters     */
							/*                                                    */
							/* Output curves:, fdi groups of oluord[f] parameters */
							/*                                                    */
							/* Shaper curves:, di groups of iluord[e] parameters   */
							/*                                                    */

	int nodp;				/* Number of data points                              */
	cow *ipoints;			/* Reference to test points as in->out                */
	cow *rpoints;			/* Modified version of ipoints                        */
	xfit_piv *piv;			/* Point inverse information for XFIT_FM_INPUT         */
	double *uerrv;			/* Array holding span width in DE for current opt chan */

	double mat[3][3];		/* XYZ White point aprox relative to accurate relative matrix */
	double cmat[3][3];		/* Final rspl correction matrix */

	double shp_smooth[MXDI];/* Smoothing factors for each input shape curve, nom = 1.0 */
	double out_smooth[MXDO];

	/* Optimisation state */
	optcomb opt_msk;		/* Optimisation mask: 3 = i+m, 2 = m, 6 = m+o, 7 = i+m+o */
	int opt_ssch;			/* Single shaper channel mode flag */
	int opt_off;			/* Optimisation parameters offset from v[0] */
	int opt_cnt;			/* Optimisation parameters count */
	double *wv;				/* Parameters being optimised */
	double *sa;				/* Search area */
	int opt_ch;				/* Channel being optimized */

	/* Methods */
	void (*del)(struct _xfit *p);

	/* Do the fitting. Return nz on error */ 
	int (*fit)(
		struct _xfit *p, 
		int flags,				/* Xfit flag values */
		int di,					/* Input dimensions */
		int fdi,				/* Output dimensions */
		int rsplflags,			/* clut rspl creation flags */
		double *wp,				/* if flags & XFIT_OUT_WP_REL, */
								/* Initial white point, returns final wp */
		double *dw,				/* Device white value to adjust to be D50 */
		double wpscale,			/* If >= 0.0 scale final wp */  
		double *dgw,			/* Device space gamut boundary white for XFIT_OUT_WP_REL_US */
								/* (ie. RGB 1,1,1 CMYK 0,0,0,0, etc) */
		cow *ipoints,			/* Array of data points to fit - referece taken */
		int nodp,				/* Number of data points */
		icxMatrixModel *skm,	/* Optional skeleton model (used for input profiles) */
		double in_min[MXDI],	/* Input value scaling/domain minimum */
		double in_max[MXDI],	/* Input value scaling/domain maximum */
		int gres[MXDI],			/* clut resolutions being optimised for/returned */
		double out_min[MXDO],	/* Output value scaling/range minimum */
		double out_max[MXDO],	/* Output value scaling/range maximum */
//		co *bpo,				/* If != NULL, black point override in same spaces as ipoints */
		double smooth,			/* clut rspl smoothing factor */
		double oavgdev[MXDO],	/* Average output value deviation */
		double demph,			/* dark emphasis factor for cLUT grid res. */
		int iord[],				/* Order of input positioning/shaper curve for each dimension */
		int sord[],				/* Order of input sub-grid shaper curve (not used) */
		int oord[],				/* Order of output shaper curve for each dimension */
		double shp_smooth[MXDI],/* Smoothing factors for each curve, nom = 1.0 */
		double out_smooth[MXDO],
		optcomb tcomb,			/* Flag - target elements to fit. */
		void *cntx2,			/* Context of callbacks */
								/* Callback to convert two fit values delta E squared */
		double (*to_de2)(void *cntx, double *in1, double *in2),
								/* Same as above, with partial derivatives */
		double (*to_dde2)(void *cntx, double dout[2][MXDIDO], double *in1, double *in2)
	);

	/* Lookup a value though a combined input positioning and shaper curves */
	double (*incurve)(struct _xfit *p, double in, int chan);

	/* Inverse Lookup a value though a combined input positioning and shaper curves */
	double (*invincurve)(struct _xfit *p, double in, int chan);

	/* Lookup a value though an output curve */
	double (*outcurve)(struct _xfit *p, double in, int chan);

	/* Inverse Lookup a value though an output curve */
	double (*invoutcurve)(struct _xfit *p, double in, int chan);

}; typedef struct _xfit xfit;

/* The icc is to provide the cone space matrix. If NULL, Bradford will be used. */
xfit *new_xfit(icc *picc);

#endif /* XFIT_H */



































