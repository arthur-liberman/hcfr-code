
#ifndef THSCREEN_H
#define THSCREEN_H

/*
 * render2d
 *
 * Threshold or Error diffusion screen pixel processing object.
 * (Simplified from DPS code)
 *
 * Author:  Graeme W. Gill
 * Date:    11/7/2005
 * Version: 1.00
 *
 * Copyright 2005, 2012, 2014 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

#define THMXCH2D  8           /* Maximum color channels */

/* Light Separation in screening flag */
typedef enum {
	scls_false = 0,		/* Don't do light ink separation during screening. */
	scls_true  = 0		/* Do light ink separation during screening. */
} sc_lightsep;

/* Input encoding */
typedef enum {
	scie_8  = 0,		/*  8 bit per component */
	scie_16 = 1			/* 16 bit per component */
} sc_iencoding;

/* Output bit order within byte */
typedef enum {
	scoo_l = 0,		/* Little endian */
	scoo_b = 1		/* Big endian */
} sc_oorder;


/* ---------------------------- */
/* Setup a set of screens */

struct _thscreens {
	int np;						/* Number of planes */
	struct _thscreen **sc;		/* List of screens */

	int oebpc;					/* Output encoding bits per component, 1,2,4 or 8 */
	int oelev;					/* Output encoding levels. Must be <= 2 ^ oebpc */
	int oevalues[256];			/* Output encoding values for each level */

	int edif;					/* nz if using error diffusion */
	int **luts;					/* Lookup tables */
	int mxwidth;				/* max width in pixels of raster to be screened */ 
	int lastyoff;				/* Last y offset */
	float **ebuf;				/* Error buffer for each plane */
								/* ebuf[][-1] is used for next pixel error */
	
	void (*quant)(void *qcntx, double *out, double *in); /* optional quantization func. for edif */
	void *qcntx;					/* Context for quant */

	sobol *so;					/* Random number generator for error diffusion */

	/* Screen pixel values */
	void (* screen)(			/* Pointer to dither function */
		struct _thscreens *t,	/* Screening object pointer */
		int width, int height,	/* Width and height to screen in pixels */
		int xoff, int yoff,		/* Offset into screening pattern */
		unsigned char *out,		/* Output pixel buffer */
		unsigned long opitch,	/* Increment between output lines in components */
		unsigned char *in,		/* Input pixel buffer */
		unsigned long ipitch);	/* Increment between input lines in components */

	void (* del)(				/* Destructor */
		struct _thscreens *t);	/* Screening objects pointer */

}; typedef struct _thscreens thscreens;


/* Return a thscreens object */
/* Screen data is used that best matches the requested parameters. */
/* Return NULL on error */
thscreens *new_thscreens(
	int exact,				/* Return only exact matches */
	int nplanes,			/* Number of planes to screen */
	double asp,				/* Target aspect ratio (== dpiX/dpiY) */
	int size,				/* Target screen size */
	sc_iencoding ie,		/* Input encoding - must be scie_16 */
	int oebpc,				/* Output encoding bits per component - must be 8 */
	int oelev,				/* Output encoding levels. Must be <= 2 ^ oebpc */
	int *oevalues,			/* Optional output encoding values for each level */
							/* Must be oelev entries. Default is 0 .. oelev-1 */
	sc_oorder oo,			/* Output bit ordering */
	double overlap,			/* Overlap between levels, 0 - 1.0 */
	int mxwidth,			/* max width in pixels of raster to be screened */ 
	void   **cntx,			/* List of contexts for lookup table callback */
	double (**lutfunc)(void *cntx, double in),	/* List of callback function, NULL if none */
	int edif,				/* nz if using error diffusion */
	void (*quant)(void *qcntx, double *out, double *in), /* optional quantization func. for edif */
	void *qcntx
);

/* ---------------------------- */
/* Screen defintion information */

typedef struct {
	int x;
	int y;
} ccoord;

typedef struct {
	int size;			/* General size */
	int width;			/* width in pixels */
	int height;			/* Height in pixels */
	double asp;			/* Aspect ratio (== dpiX/dpiY) */
	int joint;			/* na for joint screens */
	ccoord **list;		/* Pointer to list of pointers to threshold coordinates */
} thscdef;

/* ------------------------ */
/* Setup of a single screen */

struct _thscreen {
	sc_iencoding ie;			/* Input encoding */
	int oebpc;					/* Output encoding bits per component, 1,2,4 or 8 */
	int oelev;					/* Output encoding levels. Must be <= 2 ^ oebpc */
	int oevalues[256];			/* Output encoding values for each level */
	sc_oorder oo;				/* Output bit ordering */
	double asp;					/* Aspect ratio (== dpiX/dpiY) */
	double overlap;				/* Overlap between levels, 0 - 1.0 */
	int *lut;					/* Lookup table */
	unsigned char _tht[65536 * 3];/* Threshold table */
	unsigned char *tht;			/* Pointer to base of threshold table */
	unsigned char **thp;		/* Pointers to threshold table (offset int _tht) */
	int swidth;					/* Given screen width */
	int sheight;				/* Given screen height */
	int twidth;					/* Rounded up screen table width & stride */
	int theight;				/* Screen table height */ 

	void (* screen)(			/* Pointer to dither function */
		struct _thscreen *t,	/* Screening object pointer */
		int width, int height,	/* Width and height to screen in pixels */
		int xoff, int yoff,		/* Offset into screening pattern */
		unsigned char *out,		/* Output pixel buffer */
		unsigned long opinc,	/* Increment between output pixels in components */
		unsigned long opitch,	/* Increment between output lines in components */
		unsigned char *in,		/* Input pixel buffer */
		unsigned long ipinc,	/* Increment between input pixels in components */
		unsigned long ipitch);	/* Increment between input lines in components */

	void (* del)(				/* Destructor */
		struct _thscreen *t);	/* Screening object pointer */

}; typedef struct _thscreen thscreen;

/* Create a new thscreen object */
/* Return NULL on error */
thscreen *new_thscreen(
	int width,					/* width in pixels of screen */
	int height,					/* Height in pixels of screen */
	int xoff, int yoff,			/* Pattern offsets into width & height (must be +ve) */
	double asp,					/* Aspect ratio (== dpiX/dpiY) */
	int swap,					/* Swap X & Y to invert aspect ratio */
	ccoord *thli,				/* List of screen initialisation threshold coordinates */
	sc_iencoding ie,			/* Input encoding - must be scie_16 */
	int oebpc,					/* Output encoding bits per component - must be 8 */
	int oelev,					/* Output encoding levels. Must be <= 2 ^ oebpc */
	int *oevalues,				/* Optional output encoding values for each level */
								/* Must be oelev entries. Default is 0 .. oelev-1 */
	sc_oorder oo,				/* Output bit ordering */
	double olap,				/* Overlap between levels, 0 - 1.0 */
	void   *cntx,				/* Context for LUT table callback */
	double (*lutfunc)(void *cntx, double in)	/* Callback function, NULL if none */
);

#endif /* THSCREEN_H */
