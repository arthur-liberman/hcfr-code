
#ifndef CCWIN_H

/* 
 * Argyll Color Correction System
 * ChromeCast Display target patch window
 *
 * Author: Graeme W. Gill
 * Date:   8/9/14
 *
 * Copyright 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */


/* See ccast/ccmdns.h for function to get list of available ChromeCasts */

/* Create a ChromeCast display test window, default grey */
#include "ccast.h"

#ifdef __cplusplus
	extern "C" {
#endif

struct _dispwin {

/* private: */
	char *name;			/* Display path (ie. '\\.\DISPLAY1') */
						/* or "10.0.0.1:0.0" */
	char *description;	/* Description of display */

	double rgb[3];		/* Current color (full resolution, full range) */
	double s_rgb[3];	/* Current color (possibly scaled range) */
	double r_rgb[3];	/* Current color (raster value) */
	double width, height;	/* Orginial size in mm or % */
	int out_tvenc;		/* 1 to use RGB Video Level encoding */
	double blackbg;		/* NZ if black full screen background */
	void *pcntx;				/* Private context (ie., webwin, ccwin) */

	/* public: */
	int pdepth;		/* Frame buffer plane depth of display */
	/* Set a color (values 0.0 - 1.0) */
	/* Return nz on error */
	int (*set_color)(struct _dispwin *p, double r, double g, double b);
		
	/* Set/unset the blackground color flag. */
	/* Will only change on next set_col() */
	/* Return nz on error */
	int (*set_bg)(struct _dispwin *p, double blackbg);
	char *ws_url;
	void (*del)(struct _dispwin *p);

}; typedef struct _dispwin dispwin;


dispwin *new_ccwin(
ccast_id *cc_id,                /* ChromeCast to open */
double width, double height,	/* Width and height as multiplier of 10% width default. */
double hoff, double voff,		/* Offset from center in fraction of screen, range -1.0 .. 1.0 */
int out_tvenc,					/* 1 = use RGB Video Level encoding */
double blackbg					/* background ratio */
);

/* ================================================================== */

/* Chromwin context and (possible) web server  */
typedef struct _chws {
	int verb;
	int ddebug;
	
	int direct;					/* Send PNG directly, rather than using web server */

	struct mg_context *mg;		/* Mongoose context (if needed) */
	char *ws_url;				/* Web server URL for accessing server */

//	double hoff, voff;			/* Input position of test square */
	double x, y;				/* position of test square in pixels */
	double w, h;				/* size of test square in pixels */
	int pno;					/* Index to generate a sequence of URLs */
	unsigned char *ibuf;		/* Memory image of .png file */
	size_t ilen;

	ccast *cc;					/* ChromeCast */

	/* Update the png image */
	int (*update)(struct _chws *p, unsigned char *ibuf, size_t ilen, double *bg);

	/* Destroy ourselves */
	void (*del)(struct _chws *p);

} chws;

chws *new_chws(
ccast_id *cc_id,				/* ChromeCast to open */
double width, double height,	/* Width and height as % */
double hoff, double voff,		/* Offset from center in fraction of screen, range -1.0 .. 1.0 */
BOOL ws
);

#ifdef __cplusplus
	}
#endif

#define CCWIN_H
#endif /* CCWIN_H */
