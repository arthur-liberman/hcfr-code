#ifndef CCMX_H
#define CCMX_H

/* 
 * Argyll Color Correction System
 * Colorimeter Correction Matrix support.
 *
 * Author: Graeme W. Gill
 * Date:   19/8/2010
 *
 * Copyright 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * NOTE though that if SALONEINSTLIB is not defined, that this file depends
 * on other libraries that are licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3.
 *
 */

/*
 * This object provides storage and application of a 3x3 XYZ
 * corretion matrix suitable for corrected a particular
 * display colorimeter for a particular display.
 */

/* ------------------------------------------------------------------------------ */

struct _ccmx {

  /* Public: */
	void (*del)(struct _ccmx *p);

	/* Set the contents of the ccmx. return nz on error. */
	int (*set_ccmx)(struct _ccmx *p, char *desc, char *inst, char *disp, char *tech,
	                char *refd, double mtx[3][3]);	

	/* Create a ccmx from measurements. return nz on error. */
	int (*create_ccmx)(struct _ccmx *p, char *desc, char *inst, char *disp, char *tech,
	               char *refd, int nsamples, double refs[][3], double cols[][3]);	

	/* write to a CGATS .ccmx file */
	int (*write_ccmx)(struct _ccmx *p, char *filename);

	/* read from a CGATS .ccmx file */
	int (*read_ccmx)(struct _ccmx *p, char *filename);

	/* Correct an XYZ value */
	void (*xform) (struct _ccmx *p,
	               double *out,					/* Output XYZ */
	               double *in);					/* Input XYZ */

  /* Private: */
	/* (All char * are owned by ccmx) */
	char *desc;		/* Desciption (optional) */
	char *inst;		/* Name of colorimeter instrument */
	char *disp;		/* Name of display (optional if tech) */
	char *tech;		/* Technology (CRT, LCD + backlight type etc.) (optional if disp) */
	char *ref;		/* Name of spectrometer instrument (optional) */
	double matrix[3][3];	/* Transform matrix */
	double av_err;			/* Average error of fit */
	double mx_err;			/* Maximum error of fit */
	
	/* Houskeeping */
	int errc;				/* Error code */
	char err[200];			/* Error message */
}; typedef struct _ccmx ccmx;

/* Create a new, uninitialised ccmx */
ccmx *new_ccmx(void);

#endif /* CCMX_H */





































