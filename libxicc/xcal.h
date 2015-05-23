
#ifndef XCAL_H
#define XCAL_H

/* 
 * Argyll Color Correction System
 * Calibration curve class.
 *
 * Author: Graeme W. Gill
 * Date:   30/10/2005
 *
 * Copyright 2005 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * This class allows reading and using a calibration file.
 * Creation is currently left up to specialized programs (dispcal, printcal).
 * This is also used by xicc to automatically take calibration into account
 * when computing ink limits.
 */

struct _xcal {

  /* Public: */
	void (*del)(struct _xcal *p);

	/* Read a calibration file from a CGATS table */
	/* Return nz if this fails (filename is for error messages) */
	int (*read_cgats) (struct _xcal *p, cgats *cg, int table, char *filename);

	/* Read a calibration file from an ICC vcgt tag */
	/* Return nz if this fails */
	int (*read_icc) (struct _xcal *p, icc *c);

	/* Read a calibration file */
	/* Return nz if this fails */
	int (*read) (struct _xcal *p, char *filename);

	/* Write a calibration to a new cgats table */
	/* Return nz if this fails */
	int (*write_cgats)(struct _xcal *p, cgats *tcg);

	/* Write a calibration file */
	/* Return nz if this fails */
	int (*write)(struct _xcal *p, char *filename);

	/* Translate values through the curves. */
	void (*interp) (struct _xcal *p, double *out, double *in);

	/* Translate a value backwards through the curves. */
	/* Return nz if the inversion fails */ 
	int (*inv_interp) (struct _xcal *p, double *out, double *in);

	/* Translate a value through one of the curves */
	double (*interp_ch) (struct _xcal *p, int ch, double in);

	/* Translate a value backwards through one of the curves */
	/* Return -1.0 if the inversion fails */
	double (*inv_interp_ch) (struct _xcal *p, int ch, double in);

	int noramdac;			/* Set to nz if there was no VideoLUT access */
	int tvenc;				/* nz if this cal was created using (16-235)/255 Video encoding */

  /* Private: */
	icProfileClassSignature devclass;	/* Type of device */
	inkmask devmask;					/* ICX ink mask of device space */
	icColorSpaceSignature colspace;		/* Corresponding ICC device space sig (0 if none) */
	int devchan;			/* Number of chanels in device space */
	profxinf xpi;			/* Extra calibration description information */

	char err[CGATS_ERRM_LENGTH];		/* Error message */
	int errc;							/* Error code */

	rspl *cals[MAX_CHAN];
	
}; typedef struct _xcal xcal;

/* Create a new, uninitialised xcal */
xcal *new_xcal(void);

#endif /* XCAL */

