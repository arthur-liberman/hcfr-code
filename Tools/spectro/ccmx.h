#ifndef CCMX_H
#define CCMX_H

/* 
 * Argyll Color Management System
 * Colorimeter Correction Matrix support.
 *
 */

/*
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

#ifdef __cplusplus
	extern "C" {
#endif

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
	int (*set_ccmx)(struct _ccmx *p, char *desc, char *inst, char *disp, disptech dtech,
	                int refrmode, int cbid, char *sel, char *refd, int oem, double mtx[3][3]);	

	/* Create a ccmx from measurements. return nz on error. */
	int (*create_ccmx)(struct _ccmx *p, char *desc, char *inst, char *disp, disptech dtech,
	               int refrmode, int cbid, char *sel, char *refd, int oem,
	               int nsamples, double (*refs)[3], double (*cols)[3]);

	/* write to a CGATS .ccmx file */
	int (*write_ccmx)(struct _ccmx *p, char *filename);

	/* write a CGATS .ccmx file to a memory buffer. */
	/* return nz on error, with message in err[] */
	int (*buf_write_ccmx)(struct _ccmx *p, unsigned char **buf, size_t *len);

	/* read from a CGATS .ccmx file */
	int (*read_ccmx)(struct _ccmx *p, char *filename);

	/* read from a CGATS .ccmx file from a memory buffer. */
	int (*buf_read_ccmx)(struct _ccmx *p, unsigned char *buf, size_t len);

	/* Correct an XYZ value */
	void (*xform) (struct _ccmx *p,
	               double *out,					/* Output XYZ */
	               double *in);					/* Input XYZ */

  /* Private: */
	/* (All char * are owned by ccmx) */
	char *desc;		/* Desciption (optional) */
	char *inst;		/* Name of colorimeter instrument */
	char *disp;		/* Name of display (optional if tech) */
	disptech dtech;	/* Display Technology enumeration (optional if disp) */
	char *tech;		/* Technology string (Looked up from dtech enum) */
	int cc_cbid;	/* Calibration display type base ID required, 0 if not known */
	int refrmode;	/* Refresh mode, -1 if unknown, 0 of no, 1 if yes */
	char *sel;		/* Optional UI selector characters. May be NULL */
	char *ref;		/* Name of spectrometer instrument (optional) */
	int oem;			/* nz if oem origin */
	double matrix[3][3];	/* Transform matrix */
	double av_err;			/* Average error of fit */
	double mx_err;			/* Maximum error of fit */
	
	/* Houskeeping */
	int errc;				/* Error code */
	char err[200];			/* Error message */
}; typedef struct _ccmx ccmx;

/* Create a new, uninitialised ccmx */
ccmx *new_ccmx(void);

#ifdef __cplusplus
	}
#endif

#endif /* CCMX_H */




































