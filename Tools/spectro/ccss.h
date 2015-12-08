#ifndef CCSS_H
#define CCSS_H

/* 
 * Argyll Color Correction System
 * Colorimeter Calibration Spectral Set support.
 *
 * Author: Graeme W. Gill
 * Date:   18/8/2011
 *
 * Copyright 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * Based on ccmx.h
 */

/*
 * This object provides storage and application of emisive spectral
 * samples that can be used to compute calibration for suitable
 * colorimeters (such as the i1d3) tuned for particular types of displays.
 */

/* ------------------------------------------------------------------------------ */
#include "disptechs.h"

#ifdef __cplusplus
	extern "C" {
#endif

struct _ccss {

  /* Public: */
	void (*del)(struct _ccss *p);

	/* Set the contents of the ccss. return nz on error. */
	/* (Makes copies of all parameters) */
	int (*set_ccss)(struct _ccss *p, char *orig, char *cdate,
	                char *desc, char *disp, disptech dtech, int refrmode, char *sel,
	                char *ref, int oem, xspect *samples, int no_samp);	

	/* write to a CGATS .ccss file */
	/* return nz on error, with message in err[] */
	int (*write_ccss)(struct _ccss *p, char *filename);

	/* write a CGATS .ccss file to a memory buffer. */
	/* return nz on error, with message in err[] */
	int (*buf_write_ccss)(struct _ccss *p, unsigned char **buf, int *len);

	/* read from a CGATS .ccss file */
	/* return nz on error, with message in err[] */
	int (*read_ccss)(struct _ccss *p, char *filename);

	/* read from a CGATS .ccss file from a memory buffer. */
	/* return nz on error, with message in err[] */
	int (*buf_read_ccss)(struct _ccss *p, unsigned char *buf, int len);

  /* Private: */
	/* (All char * are owned by ccss) */
	char *orig;			/* Originator. May be NULL */
	char *crdate;		/* Creation date (in ctime() format). May be NULL */
	char *desc;			/* General Description (optional) */
	char *disp;			/* Description of the display (Manfrr and Model No) (optional if tech) */
	disptech dtech;		/* Display Technology enumeration (optional if disp) */
	char *tech;			/* Technology string (Looked up from dtech enum) */
	int refrmode;		/* Refresh mode, -1 if unknown, 0 of no, 1 if yes */
	char *sel;			/* Optional UI selector characters. May be NULL */
	char *ref;			/* Name of reference spectrometer instrument (optional) */
	int oem;			/* nz if oem origin */
	xspect *samples;	/* Set of spectral samples */
	int no_samp;		/* Number of samples */
	
	/* Houskeeping - should switch this to a1log ? */
	int errc;				/* Error code */
	char err[200];			/* Error message */
}; typedef struct _ccss ccss;

/* Create a new, uninitialised ccss */
ccss *new_ccss(void);

#ifdef __cplusplus
	}
#endif

#endif /* CCSS_H */




































