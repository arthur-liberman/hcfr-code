
/*
 * Copyright 1998 - 2004 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* This is a simple 2d graph plotter that runs on MSWindows/OSX/X11 */
/* The code is in three sections, one for each GUI environment. */
/* (Perhaps common code could be consolidated ?) */


/*
 * TTBD:
 *	
 *		Allow for a window title for each plot.
 *
 *		Put all state information in plot_info (window handles etc.)
 *      Create thread to handle events, so it updates correctly.
 *		(Will have to lock plot info updates or ping pong them);
 *		Have call to destroy window.
 *		(Could then move to having multiple instances of plot).
 *
 *    OS X Cocoa code doesn't get window focus. No idea why.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifdef UNIX
#include <unistd.h>
#endif
#include <math.h>
#include "../../libnum/numlib.h"
#include "plot.h"
//#ifdef STANDALONE_TEST
#include "ui.h"
//#endif /* STANDALONE_TEST */

#undef DODEBUG				/* Print error messages & progress reports */
//#define STANDALONE_TEST	/* Defined by build script */

#define NTICK 10
#define LTHICK 1.2			/* Plot line thickness */
#define ILTHICK ((int)(LTHICK+0.5))		/* Integer line thickness */
//#define ILTHICK 2
#undef CROSSES		/* Mark input points with crosses */

#define DEFWWIDTH  500
#define DEFWHEIGHT 500

/* Graph order is Black = Y1, Red = Y2, Green = Y3, Blue = Y4, Yellow = Y5, Purple = Y6 */
/* Brown = Y7, Orange = Y8, Grey = Y9, Magenta = Y10  */

double nicenum(double x, int round);

#define MXGPHS	10		/* Number of graphs with common X axis */

/* Colors of the graphs */
static int gcolors[MXGPHS][3] = {
	{   0,   0,   0},	/* Black */
	{ 210,  30,   0},	/* Red */
	{   0, 200,  90},	/* Green */
	{   0,  10, 255},	/* Blue */
	{ 200, 200,   0},	/* Yellow */
	{ 220,   0, 255},	/* Purple */
	{ 136,  86,  68},	/* Brown */
	{ 248,  95,   0},	/* Orange */
	{ 160, 160, 160},	/* Grey */
	{ 220, 30,  220}	/* Magenta */
};

/* Information defining plot */
/* There is one global instance of this. */
struct _plot_info {
	void *cx;				/* Other Context */

	int flags;				/* Flags */
	int dowait;				/* Wait for user key if > 0, wait for n secs if < 0 */
	double ratio;			/* Aspect ratio of window, X/Y */

	/* Plot point information */
	double mnx, mxx, mny, mxy;		/* Extrema of values to be plotted */
	int graph;						/* NZ if graph, Z if vectors */
	int revx;						/* reversed X axis */

	double *x1, *x2;
	double *yy[MXGPHS];				/* y1 - y10 */
	char **ntext;
	int n;

	double *x7, *y7;
	plot_col *mcols;
	char **mtext;
	int m;

	double *x8, *y8, *x9, *y9;
	plot_col *ocols;
	int o;

	/* Plot instance information */
	int sx,sy;			/* Screen offset */
	int sw,sh;			/* Screen width and height */
	double scx, scy;	/* Scale from input values to screen pixels */

	}; typedef struct _plot_info plot_info;

/* Global to transfer info to window callback */
static plot_info pd;

#define PLOTF_NONE           0x0000
#define PLOTF_GRAPHCROSSES   0x0001			/* Plot crosses at each point on the graphs */
#define PLOTF_VECCROSSES     0x0002			/* Plot crosses at the end of x1,y1 -> x2,y2 */

/* Declaration of superset */
static int do_plot_imp(
	int flags,
    double xmin, double xmax, double ymin, double ymax,	/* Bounding box */
	double ratio,	/* Aspect ratio of window, X/Y */
	int dowait,		/* > 0 wait for user to hit space key, < 0 delat dowait seconds. */
	double *x1, double *x2,
	double *yy[MXGPHS], char **ntext,
	int n,
	double *x7, double *y7, plot_col *mcols, char **mtext,
    int m,
	double *x8, double *y8, double *x9, double*y9, plot_col *ocols,
	int o
);


#define MAX(a,b) (((a) > (b)) ? (a) : (b))

/* Public routines */
/* Plot up to 3 graphs. Wait for key */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int
do_plot(
double *x,
double *y1,	/* Up to 3 graphs */
double *y2,
double *y3,
int n) {
	int i, j;
	double xmin, xmax, ymin, ymax;
	double *yy[MXGPHS];

	for (j = 0; j < MXGPHS; j++)
		yy[j] = NULL;

	yy[0] = y1;
	yy[1] = y2;
	yy[2] = y3;

	/* Determine min and max dimensions of plot */
	xmin = ymin = 1e6;
	xmax = ymax = -1e6;

	for (i = 0; i < n; i++) {
		if (xmin > x[i])
			xmin = x[i];
		if (xmax < x[i])
			xmax = x[i];

		for (j = 0; j < MXGPHS; j++) {
			if (yy[j] != NULL) {
				if (ymin > yy[j][i])
					ymin = yy[j][i];
				if (ymax < yy[j][i])
					ymax = yy[j][i];
			}
		}
	}

	/* Work out scale factors */
	if ((xmax - xmin) == 0.0)
		xmax += 0.5, xmin -= 0.5;
	if ((ymax - ymin) == 0.0)
		ymax += 0.5, ymin -= 0.5;

	return do_plot_imp(PLOTF_NONE,
	                   xmin, xmax, ymin, ymax, 1.0, 1,
	                   x, NULL, yy, NULL, n,
	                   NULL, NULL, NULL, NULL, 0,
	                   NULL, NULL, NULL, NULL, NULL, 0); 
}

/* Public routines */
/* Plot up to 3 graphs + crosses. Wait for key */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int
do_plot_p(
double *x,
double *y1,	/* Up to 3 graphs */
double *y2,
double *y3,
int n,
double *x4, double *y4,		/* And crosses */
int m) {
	int i, j;
	double xmin, xmax, ymin, ymax;
	double *yy[MXGPHS];

	for (j = 0; j < MXGPHS; j++)
		yy[j] = NULL;

	yy[0] = y1;
	yy[1] = y2;
	yy[2] = y3;

	/* Determine min and max dimensions of plot */
	xmin = ymin = 1e6;
	xmax = ymax = -1e6;

	for (i = 0; i < n; i++) {
		if (xmin > x[i])
			xmin = x[i];
		if (xmax < x[i])
			xmax = x[i];

		for (j = 0; j < MXGPHS; j++) {
			if (yy[j] != NULL) {
				if (ymin > yy[j][i])
					ymin = yy[j][i];
				if (ymax < yy[j][i])
					ymax = yy[j][i];
			}
		}
	}

	for (i = 0; i < m; i++) {
		if (x4 != NULL) {
			if (xmin > x4[i])
				xmin = x4[i];
			if (xmax < x4[i])
				xmax = x4[i];
		}
		if (y4 != NULL) {
			if (ymin > y4[i])
				ymin = y4[i];
			if (ymax < y4[i])
				ymax = y4[i];
		}
	}

	/* Work out scale factors */
	if ((xmax - xmin) == 0.0)
		xmax += 0.5, xmin -= 0.5;
	if ((ymax - ymin) == 0.0)
		ymax += 0.5, ymin -= 0.5;

	return do_plot_imp(PLOTF_NONE,
	                   xmin, xmax, ymin, ymax, 1.0, 1,
	                   x, NULL, yy, NULL, n,
	                   x4, y4, NULL, NULL, m ,
	                   NULL, NULL, NULL, NULL, NULL, 0); 
}

/* Plot up to 3 graphs with specified window size. */
/* if dowait > 0, wait for user key */
/* if dowait < 0, wait for no seconds */
/* If xmax > xmin, use as x scale, else auto. */
/* If ymax > ymin, use as y scale, else auto. */
/* ratio is window X / Y */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int
do_plot_x(
double *x,
double *y1,
double *y2,
double *y3,
int n,
int dowait,
double pxmin,
double pxmax,
double pymin,
double pymax,
double ratio
) {
	int i, j;
	double xmin, xmax, ymin, ymax;
	double *yy[MXGPHS];

	for (j = 0; j < MXGPHS; j++)
		yy[j] = NULL;

	yy[0] = y1;
	yy[1] = y2;
	yy[2] = y3;

	/* Determine min and max dimensions of plot */
	xmin = ymin = 1e6;
	xmax = ymax = -1e6;

	for (i = 0; i < n; i++) {
		if (xmin > x[i])
			xmin = x[i];
		if (xmax < x[i])
			xmax = x[i];

		for (j = 0; j < MXGPHS; j++) {
			if (yy[j] != NULL) {
				if (ymin > yy[j][i])
					ymin = yy[j][i];
				if (ymax < yy[j][i])
					ymax = yy[j][i];
			}
		}
	}

	/* Work out scale factors */
	if ((xmax - xmin) == 0.0)
		xmax += 0.5, xmin -= 0.5;
	if ((ymax - ymin) == 0.0)
		ymax += 0.5, ymin -= 0.5;

	if (pxmax > pxmin) {
		xmax = pxmax;
		xmin = pxmin;
	}
	if (pymax > pymin) {
		ymax = pymax;
		ymin = pymin;
	}

	return do_plot_imp(PLOTF_NONE,
	                   xmin, xmax, ymin, ymax, ratio, dowait,
	                   x, NULL, yy, NULL, n,
	                   NULL, NULL, NULL, NULL, 0,
	                   NULL, NULL, NULL, NULL, NULL, 0); 
}

/* Public routines */
/* Plot up to 6 graphs. Wait for a key */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int
do_plot6(
double *x,	/* X coord */
double *y1,	/* Black */
double *y2,	/* Red */
double *y3,	/* Green */
double *y4,	/* Blue */
double *y5,	/* Yellow */
double *y6,	/* Purple */
int n) {	/* Number of values */
	int i, j;
	double xmin, xmax, ymin, ymax;
	int nn = abs(n);
	double *yy[MXGPHS];

	for (j = 0; j < MXGPHS; j++)
		yy[j] = NULL;

	yy[0] = y1;
	yy[1] = y2;
	yy[2] = y3;
	yy[3] = y4;
	yy[4] = y5;
	yy[5] = y6;

	/* Determine min and max dimensions of plot */
	xmin = ymin = 1e6;
	xmax = ymax = -1e6;

	for (i = 0; i < n; i++) {
		if (xmin > x[i])
			xmin = x[i];
		if (xmax < x[i])
			xmax = x[i];

		for (j = 0; j < MXGPHS; j++) {
			if (yy[j] != NULL) {
				if (ymin > yy[j][i])
					ymin = yy[j][i];
				if (ymax < yy[j][i])
					ymax = yy[j][i];
			}
		}
	}

	/* Work out scale factors */
	if ((xmax - xmin) == 0.0)
		xmax += 0.5, xmin -= 0.5;
	if ((ymax - ymin) == 0.0)
		ymax += 0.5, ymin -= 0.5;

	return do_plot_imp(PLOTF_NONE,
	                   xmin, xmax, ymin, ymax, 1.0, 1,
	                   x, NULL, yy, NULL, n,
	                   NULL, NULL, NULL, NULL, n ,
	                   NULL, NULL, NULL, NULL, NULL, 0); 
}

/* Public routines */
/* Plot up to 6 graphs + optional crosses. Wait for a key */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int
do_plot6p(
double *x,	/* X coord */
double *y1,	/* Black */
double *y2,	/* Red */
double *y3,	/* Green */
double *y4,	/* Blue */
double *y5,	/* Yellow */
double *y6,	/* Purple */
int n,		/* Number of values */
double *xp, double *yp,		/* And crosses */
int m) {
	int i, j;
	double xmin, xmax, ymin, ymax;
	int nn = abs(n);
	double *yy[MXGPHS];

	for (j = 0; j < MXGPHS; j++)
		yy[j] = NULL;

	yy[0] = y1;
	yy[1] = y2;
	yy[2] = y3;
	yy[3] = y4;
	yy[4] = y5;
	yy[5] = y6;

	/* Determine min and max dimensions of plot */
	xmin = ymin = 1e6;
	xmax = ymax = -1e6;

	for (i = 0; i < n; i++) {
		if (xmin > x[i])
			xmin = x[i];
		if (xmax < x[i])
			xmax = x[i];

		for (j = 0; j < MXGPHS; j++) {
			if (yy[j] != NULL) {
				if (ymin > yy[j][i])
					ymin = yy[j][i];
				if (ymax < yy[j][i])
					ymax = yy[j][i];
			}
		}
	}

	for (i = 0; i < m; i++) {
		if (xp != NULL) {
			if (xmin > xp[i])
				xmin = xp[i];
			if (xmax < xp[i])
				xmax = xp[i];
		}
		if (yp != NULL) {
			if (ymin > yp[i])
				ymin = yp[i];
			if (ymax < yp[i])
				ymax = yp[i];
		}
	}

	/* Work out scale factors */
	if ((xmax - xmin) == 0.0)
		xmax += 0.5, xmin -= 0.5;
	if ((ymax - ymin) == 0.0)
		ymax += 0.5, ymin -= 0.5;

	return do_plot_imp(PLOTF_NONE,
	                   xmin, xmax, ymin, ymax, 1.0, 1,
	                   x, NULL, yy, NULL, n,
	                   xp, yp, NULL, NULL, m,
	                   NULL, NULL, NULL, NULL, NULL, 0); 
}

/* Public routines */
/* Plot up to 10 graphs. Wait for a key */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int
do_plot10(
double *x,	/* X coord */
double *y1,	/* Black */
double *y2,	/* Red */
double *y3,	/* Green */
double *y4,	/* Blue */
double *y5,	/* Yellow */
double *y6,	/* Purple */
double *y7,	/* Brown */
double *y8,	/* Orange */
double *y9,	/* Grey */
double *y10,/* White */
int n,		/* Number of values */
int zero	/* Flag - make sure zero is in y range */
) {
	int i, j;
	double xmin, xmax, ymin, ymax;
	int nn = abs(n);
	double *yy[MXGPHS];

	for (j = 0; j < MXGPHS; j++)
		yy[j] = NULL;

	yy[0] = y1;
	yy[1] = y2;
	yy[2] = y3;
	yy[3] = y4;
	yy[4] = y5;
	yy[5] = y6;
	yy[6] = y7;
	yy[7] = y8;
	yy[8] = y9;
	yy[9] = y10;

	/* Determine min and max dimensions of plot */
	xmin = ymin = 1e6;
	xmax = ymax = -1e6;

	for (i = 0; i < n; i++) {
		if (xmin > x[i])
			xmin = x[i];
		if (xmax < x[i])
			xmax = x[i];

		for (j = 0; j < MXGPHS; j++) {
			if (yy[j] != NULL) {
				if (ymin > yy[j][i])
					ymin = yy[j][i];
				if (ymax < yy[j][i])
					ymax = yy[j][i];
			}
		}
	}

	if (zero && ymin > 0.0)
		ymin = 0.0;
		
	/* Work out scale factors */
	if ((xmax - xmin) == 0.0)
		xmax += 0.5, xmin -= 0.5;
	if ((ymax - ymin) == 0.0)
		ymax += 0.5, ymin -= 0.5;

	return do_plot_imp(PLOTF_NONE,
	                   xmin, xmax, ymin, ymax, 1.0, 1,
	                   x, NULL, yy, NULL, n,
	                   NULL, NULL, NULL, NULL, n ,
	                   NULL, NULL, NULL, NULL, NULL, 0); 
}

/* Public routines */
/* Plot up to 10 graphs + optional crosses. Wait for a key */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int
do_plot10p(
double *x,	/* X coord */
double *y1,	/* Black */
double *y2,	/* Red */
double *y3,	/* Green */
double *y4,	/* Blue */
double *y5,	/* Yellow */
double *y6,	/* Purple */
double *y7,	/* Brown */
double *y8,	/* Orange */
double *y9,	/* Grey */
double *y10,/* White */
int n,		/* Number of values */
double *xp, double *yp,		/* And crosses */
int m) {
	int i, j;
	double xmin, xmax, ymin, ymax;
	int nn = abs(n);
	double *yy[MXGPHS];

	for (j = 0; j < MXGPHS; j++)
		yy[j] = NULL;

	yy[0] = y1;
	yy[1] = y2;
	yy[2] = y3;
	yy[3] = y4;
	yy[4] = y5;
	yy[5] = y6;
	yy[6] = y7;
	yy[7] = y8;
	yy[9] = y9;
	yy[5] = y10;

	/* Determine min and max dimensions of plot */
	xmin = ymin = 1e6;
	xmax = ymax = -1e6;

	for (i = 0; i < n; i++) {
		if (xmin > x[i])
			xmin = x[i];
		if (xmax < x[i])
			xmax = x[i];

		for (j = 0; j < MXGPHS; j++) {
			if (yy[j] != NULL) {
				if (ymin > yy[j][i])
					ymin = yy[j][i];
				if (ymax < yy[j][i])
					ymax = yy[j][i];
			}
		}
	}

	for (i = 0; i < m; i++) {
		if (xp != NULL) {
			if (xmin > xp[i])
				xmin = xp[i];
			if (xmax < xp[i])
				xmax = xp[i];
		}
		if (yp != NULL) {
			if (ymin > yp[i])
				ymin = yp[i];
			if (ymax < yp[i])
				ymax = yp[i];
		}
	}

	/* Work out scale factors */
	if ((xmax - xmin) == 0.0)
		xmax += 0.5, xmin -= 0.5;
	if ((ymax - ymin) == 0.0)
		ymax += 0.5, ymin -= 0.5;

	return do_plot_imp(PLOTF_NONE,
	                   xmin, xmax, ymin, ymax, 1.0, 1,
	                   x, NULL, yy, NULL, n,
	                   xp, yp, NULL, NULL, m,
	                   NULL, NULL, NULL, NULL, NULL, 0); 
}


/* Plot a bunch of vectors + optional crosses */
/* return 0 on success, -1 on error */
int do_plot_vec(
double xmin,
double xmax,
double ymin,
double ymax,
double *x1,		/* vector start */
double *y1,
double *x2,		/* vector end */
double *y2,
int n,			/* Number of vectors */
int dowait,
double *x3,		/* extra point */
double *y3,
plot_col *mcols,	/* point colors */
char **mtext,		/* notation */
int m			/* Number of points */
) {
	int j;
	double *yy[MXGPHS];

	for (j = 0; j < MXGPHS; j++)
		yy[j] = NULL;

	yy[0] = y1;
	yy[1] = y2;

	return do_plot_imp(PLOTF_VECCROSSES,
	                   xmin, xmax, ymin, ymax, 1.0, dowait,
	                   x1, x2, yy, NULL, n,
	                   x3, y3, mcols, mtext, m,
	                   NULL, NULL, NULL, NULL, NULL, 0); 
}

/* Plot a bunch of vectors & crosses + optional crosses + optional vectors*/
/* return 0 on success, -1 on error */
int do_plot_vec2(
double xmin,
double xmax,
double ymin,
double ymax,
double *x1,		/* n vector start */
double *y1,
double *x2,		/* vector end and diagonal cross */
double *y2,
char **ntext,	/* text annotation at cross */
int n,			/* Number of vectors */
int dowait,
double *x3,		/* m extra crosses */
double *y3,
plot_col *mcols,/* cross colors */
char **mtext,	/* text annotation at cross */
int m,			/* Number of crosses */
double *x4,		/* o vector start */
double *y4,
double *x5,		/* o vector end */
double *y5,
plot_col *ocols,/* Vector colors */
int o			/* Number of vectors */
) {
	int j;
	double *yy[MXGPHS];

	for (j = 0; j < MXGPHS; j++)
		yy[j] = NULL;

	yy[0] = y1;
	yy[1] = y2;

	return do_plot_imp(PLOTF_VECCROSSES,
	                   xmin, xmax, ymin, ymax, 1.0, dowait,
	                   x1, x2, yy, ntext, n,
	                   x3, y3, mcols, mtext, m,
	                   x4, y4, x5, y5, ocols, o); 
}

/* ********************************** NT version ********************** */
#ifdef NT

#include <windows.h>

#ifdef DODEBUG
# define debugf(xx)	printf xx
#else
# define debugf(xx)
#endif

double plot_ratio = 1.0;
HANDLE plot_th;              /* Thread */
char plot_AppName[] = "PlotWin";
HWND plot_hwnd  = NULL;	/* Open only one window per session */
int plot_signal = 0;	/* Signal a key or quit */

static LRESULT CALLBACK MainWndProc (HWND, UINT, WPARAM, LPARAM);

/* Thread to handle message processing, so that there is no delay */
/* when the main thread is doing other things. */
DWORD WINAPI plot_message_thread(LPVOID lpParameter) {
	MSG msg;
	WNDCLASS wc;
	ATOM arv;
	HWND _hwnd;

	// Fill in window class structure with parameters that describe the
	// main window.

	wc.style         = CS_HREDRAW | CS_VREDRAW;	// Class style(s).
	wc.lpfnWndProc   = MainWndProc;	// Function to retrieve messages for windows of this class.
	wc.cbClsExtra    = 0;			// No per-class extra data.
	wc.cbWndExtra    = 0;			// No per-window extra data.
	wc.hInstance     = NULL;		// Application that owns the class.
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_CROSS);
	wc.hbrBackground = GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = plot_AppName;

	arv = RegisterClass(&wc);

	if (!arv) {
		debugf(("RegisterClass failed, lasterr = %d\n",GetLastError()));
		return -1;
	}

	_hwnd = CreateWindow(
		plot_AppName,
		"2D Diagnostic Graph Plot",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		(int)(DEFWWIDTH * plot_ratio + 0.5),
		DEFWHEIGHT,
		NULL,
		NULL,
		NULL, // hInstance,
		NULL);

	if (!_hwnd) {
		debugf(("CreateWindow failed, lasterr = %d\n",GetLastError()));
		return -1;
	}
	
	ShowWindow(_hwnd, SW_SHOW);

	plot_hwnd = _hwnd;

	/* Now process messages until we're done */
	for (;;) {
		if (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (plot_signal == 99)
				break;
		}
	}

	if (UnregisterClass(plot_AppName, NULL) == 0) {
		debugf(("UnregisterClass failed, lasterr = %d\n",GetLastError()));
	}

	plot_hwnd = NULL;		/* Signal it's been deleted */

	return 0;
}

/* Superset implementation function: */
/* return 0 on success, -1 on error */
/* Hybrid Graph uses x1 : y1, y2, y3, y4, y5, y6 for up to 6 graph curves + */
/* optional diagonal crosses at x7, y7 in yellow (x2 == NULL). */
/* Vector uses x1, y1 to x2, y2 as a vector with a (optional) diagonal cross at x2, y2 */
/* all in black with annotation ntext at the cross, */
/* plus a (optiona) diagonal cross at x7, y7 in yellow. The color for x7, y7 can be */
/* overidden by an array of colors mcols, plus optional label text mtext. (x2 != NULL) */
/* n = number of points/vectors. -ve for reversed X axis */
/* m = number of extra points (x2,y3 or x7,y7) */
/* x8,y8 to x9,y9 are extra optional vectors with optional colors */
static int do_plot_imp(
	int flags,
    double xmin, double xmax, double ymin, double ymax,	/* Bounding box */
	double ratio,	/* Aspect ratio of window, X/Y */
	int dowait,		/* > 0 wait for user to hit space key, < 0 delat dowait seconds. */
    double *x1, double *x2,
    double *yy[MXGPHS], char **ntext,
	int n,
	double *x7, double *y7, plot_col *mcols, char **mtext,
    int m,
	double *x8, double *y8, double *x9, double*y9, plot_col *ocols,
	int o
) {
	pd.flags = flags;
	pd.dowait = 10 * dowait;
	pd.ratio = ratio;
	{
		int j;
		double xr,yr;

		pd.mnx = xmin;
		pd.mny = ymin;
		pd.mxx = xmax;
		pd.mxy = ymax;

		/* Allow some extra around plot */
		xr = pd.mxx - pd.mnx;
		yr = pd.mxy - pd.mny;
		if (xr < 1e-6)
			xr = 1e-6;
		if (yr < 1e-6)
			yr = 1e-6;
		pd.mnx -= xr/10.0;
		pd.mxx += xr/10.0;
		pd.mny -= yr/10.0;
		pd.mxy += yr/10.0;

		/* Transfer raw point info */
		if (x2 == NULL)
			pd.graph = 1;		/* 6 graphs + points */
		else
			pd.graph = 0;
		pd.x1 = x1;
		pd.x2 = x2;
		for (j = 0; j < MXGPHS; j++)
			pd.yy[j] = yy[j];
		pd.ntext = ntext;
		pd.n = abs(n);

		if (n < 0) {
			double tt;
			tt = pd.mxx;
			pd.mxx = pd.mnx;
			pd.mnx = tt;
			pd.revx = 1;
		} else {
			pd.revx = 0;
		}
		pd.x7 = x7;
		pd.y7 = y7;
		pd.mcols = mcols;
		pd.mtext = mtext;
		pd.m = abs(m);

		pd.x8 = x8;
		pd.y8 = y8;
		pd.x9 = x9;
		pd.y9 = y9;
		pd.ocols = ocols;
		pd.o = abs(o);
	}

	/* ------------------------------------------- */
	/* Setup windows stuff */
	{
		/* It would be nice to reduce number of globals. */

		/* We don't clean up properly either - ie. don't delete thread etc. */

		/* Create thread that creats window and processes window messages */
		if (plot_hwnd == NULL) {

			plot_ratio = ratio;

		    plot_th = CreateThread(NULL, 0, plot_message_thread, NULL, 0, NULL);
		    if (plot_th == NULL) {
				debugf(("new_athread failed\n"));
				return -1;
			}
			while (plot_hwnd == NULL)
				Sleep(50);
		}
 
		plot_signal = 0;

		SetForegroundWindow(plot_hwnd);

		/* Force a repaint with the new data */
		if (!InvalidateRgn(plot_hwnd,NULL,TRUE)) {
			debugf(("InvalidateRgn failed, lasterr = %d\n",GetLastError()));
			return -1;
		}

		if (dowait > 0) {		/* Wait for a space key */
			while(plot_signal == 0 && plot_hwnd != NULL)
				Sleep(50);
			plot_signal = 0;
		} else if (dowait < 0) {
			Sleep(-dowait * 1000);
		}
	}
	return 0;
}

void DoPlot(HDC hdc, plot_info *pd);

static LRESULT CALLBACK MainWndProc(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam
) {
	HDC hdc;
	PAINTSTRUCT ps;
	RECT rect;

	debugf(("Handling message type 0x%x\n",message));
	// Could use Set/GetWindowLong() to pass window class info instead of global pd (beware NULL)
	switch(message) {
		case WM_PAINT:
			debugf(("It's a paint message\n"));
			hdc = BeginPaint(hwnd, &ps);
			GetClientRect(hwnd, &rect);

			/* Setup the plot info structure for this drawing */
			pd.sx = rect.left; 
			pd.sy = rect.top; 
			pd.sw = 1 + rect.right - rect.left; 
			pd.sh = 1 + rect.bottom - rect.top; 
			pd.scx = (pd.sw - 10)/(pd.mxx - pd.mnx);
			pd.scy = (pd.sh - 10)/(pd.mxy - pd.mny);
  
			DoPlot(hdc, &pd);

			EndPaint(hwnd, &ps);

			return 0;

		case WM_CHAR:
			debugf(("It's a char message, wParam = 0x%x\n",wParam));
			switch(wParam) {
				case '\r':	
				case '\n':	
				case ' ':	/* Space */
					debugf(("It's a SPACE, so signal it\n"));
					plot_signal = 1;
					return 0;
			}

		case WM_CLOSE: 
			debugf(("It's a close message\n"));
			DestroyWindow(hwnd);
			return 0; 
 
		case WM_DESTROY: 
			debugf(("It's a destroy message\n"));
			plot_signal = 99;
		    PostQuitMessage(0); 
			return 0;
	}

	debugf(("It's a message not handled here\n"));
	return DefWindowProc(hwnd, message, wParam, lParam);
}

void
xtick(HDC hdc, plot_info *pdp, double x, char *lab)
	{
	int xx,yy;
	RECT rct;

	xx = 10 + (int)((x - pdp->mnx) * pdp->scx + 0.5);
	yy = pdp->sh - 10;

	MoveToEx(hdc,xx, yy, NULL);
	LineTo(  hdc,xx, 0);
	rct.right = 
	rct.left = xx;
	rct.top = 
	rct.bottom = yy;
	DrawText(hdc, lab, -1, &rct, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOCLIP);
	}

void
ytick(HDC hdc, plot_info *pdp, double y, char *lab)
	{
	int xx,yy;
	RECT rct;

	xx = 5;
	yy = pdp->sh - 10 - (int)((y - pdp->mny) * pdp->scy + 0.5);
	MoveToEx(hdc,xx,      yy,NULL);
	LineTo(  hdc,pdp->sw, yy);
	rct.right = 
	rct.left = xx;
	rct.top = 
	rct.bottom = yy;
	DrawText(hdc, lab, -1, &rct, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOCLIP);
	}

void
loose_label(HDC hdc, plot_info *pdp, double min, double max, void (*pfunc)(HDC hdc, plot_info *pdp, double, char *)
) {
	char str[6], temp[20];
	int nfrac;
	double d;
	double graphmin, graphmax;
	double range,x;

	range = nicenum(min-max,0);
	d = nicenum(range/(NTICK-1),1);
	graphmin = floor(min/d) * d;
	graphmax = ceil(max/d) * d;
	nfrac = (int)MAX(-floor(log10(d)),0);
	sprintf(str,"%%.%df", nfrac);
	for (x = graphmin; x < graphmax + 0.5 * d; x += d) {
		sprintf(temp,str,x);
		pfunc(hdc,pdp,x,temp);
	}
}

void
DoPlot(
HDC hdc,
plot_info *p
) {
	int i, j;
	int lx,ly;		/* Last x,y */
	HPEN pen;

	pen = CreatePen(PS_DOT,0,RGB(200,200,200));

	SaveDC(hdc);
	SelectObject(hdc,pen);

	/* Plot horizontal axis */
	if (p->revx)
		loose_label(hdc, p, p->mxx, p->mnx, xtick);
	else
		loose_label(hdc, p, p->mnx, p->mxx, xtick);

	/* Plot vertical axis */
	loose_label(hdc, p, p->mny, p->mxy, ytick);

	RestoreDC(hdc,-1);
	DeleteObject(pen);

	if (p->graph) {		/* Up to MXGPHS graphs + crosses */
		int gcolors[MXGPHS][3] = {
			{   0,   0,   0},	/* Black */
			{ 210,  30,   0},	/* Red */
			{   0, 200,  90},	/* Green */
			{   0,  10, 255},	/* Blue */
			{ 200, 200,   0},	/* Yellow */
			{ 220,   0, 255},	/* Purple */
			{ 136,  86,  68},	/* Brown */
			{ 248,  95,   0},	/* Orange */
			{ 160, 160, 160},	/* Grey */
			{ 220, 220, 220}	/* White */
		};
		for (j = MXGPHS-1; j >= 0; j--) {
			double *yp = p->yy[j];
		
			if (yp == NULL)
				continue;

			pen = CreatePen(PS_SOLID,ILTHICK,RGB(gcolors[j][0],gcolors[j][1],gcolors[j][2]));
			SelectObject(hdc,pen);

			lx = (int)((p->x1[0] - p->mnx) * p->scx + 0.5);
			ly = (int)((     yp[0] - p->mny) * p->scy + 0.5);

			for (i = 0; i < p->n; i++) {
				int cx,cy;
				cx = (int)((p->x1[i] - p->mnx) * p->scx + 0.5);
				cy = (int)((     yp[i] - p->mny) * p->scy + 0.5);
	
				MoveToEx(hdc, 10 + lx, p->sh - 10 - ly, NULL);
				LineTo(hdc, 10 + cx, p->sh - 10 - cy);
				if (p->flags & PLOTF_GRAPHCROSSES) {
					MoveToEx(hdc, 10 + cx - 5, p->sh - 10 - cy - 5, NULL);
					LineTo(hdc, 10 + cx + 5, p->sh - 10 - cy + 5);
					LineTo(hdc, 10 + cx - 5, p->sh - 10 - cy + 5);
				}
				lx = cx;
				ly = cy;
			}
			DeleteObject(pen);
		}

	} else {	/* Vectors with cross */

		pen = CreatePen(PS_SOLID,ILTHICK,RGB(0,0,0));
		SelectObject(hdc,pen);

		if (p->ntext != NULL) {
			HFONT fon;

			fon = CreateFont(12, 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE, ANSI_CHARSET,
			                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			                 FF_DONTCARE, NULL);

			if (fon == NULL)
				fprintf(stderr,"plot: CreateFont returned NULL\n");
			else {
				SelectObject(hdc,fon);
				DeleteObject(fon);
			}
		}

		for (i = 0; i < p->n; i++) {
			int cx,cy;

			lx = (int)((p->x1[i] - p->mnx) * p->scx + 0.5);
			ly = (int)((p->yy[0][i] - p->mny) * p->scy + 0.5);

			cx = (int)((p->x2[i] - p->mnx) * p->scx + 0.5);
			cy = (int)((p->yy[1][i] - p->mny) * p->scy + 0.5);

			MoveToEx(hdc, 10 + lx, p->sh - 10 - ly, NULL);
			LineTo(hdc, 10 + cx, p->sh - 10 - cy);

			if (p->flags & PLOTF_VECCROSSES) {
				MoveToEx(hdc, 10 + cx - 5, p->sh - 10 - cy - 5, NULL);
				LineTo(hdc, 10 + cx + 5, p->sh - 10 - cy + 5);
				MoveToEx(hdc, 10 + cx + 5, p->sh - 10 - cy - 5, NULL);
				LineTo(hdc, 10 + cx - 5, p->sh - 10 - cy + 5);
			}

			if (p->ntext != NULL) {
				RECT rct;
				rct.right = 
				rct.left = 10 + cx + 10;
				rct.top = 
				rct.bottom = p->sh - 10 - cy + 10;
				DrawText(hdc, p->ntext[i], -1, &rct, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOCLIP);
			}
		}
		DeleteObject(pen);
	}

	/* Extra points */
	if (p->x7 != NULL && p->y7 != NULL && p->m > 0 ) {
		pen = CreatePen(PS_SOLID,ILTHICK,RGB(210,150,0));
		SelectObject(hdc,pen);
		
		if (p->mtext != NULL) {
			HFONT fon;


			fon = CreateFont(12, 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE, ANSI_CHARSET,
			                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			                 FF_DONTCARE, NULL);

			if (fon == NULL)
				fprintf(stderr,"plot: CreateFont returned NULL\n");
			else {
				SelectObject(hdc,fon);
				DeleteObject(fon);
			}
		}

		for (i = 0; i < p->m; i++) {
			lx = (int)((p->x7[i] - p->mnx) * p->scx + 0.5);
			ly = (int)((p->y7[i] - p->mny) * p->scy + 0.5);

			if (p->mcols != NULL) {
				int rgb[3];

				for (j = 0; j < 3; j++)
					rgb[j] = (int)(p->mcols[i].rgb[j] * 255.0 + 0.5);

				DeleteObject(pen);
				pen = CreatePen(PS_SOLID,ILTHICK,RGB(rgb[0],rgb[1],rgb[2]));
				SelectObject(hdc,pen);

				if (p->mtext != NULL)
					SetTextColor(hdc, RGB(rgb[0],rgb[1],rgb[2]));
			}
			MoveToEx(hdc, 10 + lx - 5, p->sh - 10 - ly, NULL);
			LineTo(hdc, 10 + lx + 5, p->sh - 10 - ly);
			MoveToEx(hdc, 10 + lx, p->sh - 10 - ly - 5, NULL);
			LineTo(hdc, 10 + lx, p->sh - 10 - ly + 5);

			if (p->mtext != NULL) {
				RECT rct;
				rct.right = 
				rct.left = 10 + lx + 10;
				rct.top = 
				rct.bottom = p->sh - 10 - ly - 10;
				DrawText(hdc, p->mtext[i], -1, &rct, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOCLIP);
			}
		}
		DeleteObject(pen);
	}
	/* Extra vectors */
	if (p->x8 != NULL && p->y8 != NULL && p->x9 != NULL && p->y9 && p->o > 0 ) {
		pen = CreatePen(PS_SOLID,ILTHICK,RGB(150,255,255));
		SelectObject(hdc,pen);
		
		for (i = 0; i < p->o; i++) {
			int cx,cy;

			lx = (int)((p->x8[i] - p->mnx) * p->scx + 0.5);
			ly = (int)((p->y8[i] - p->mny) * p->scy + 0.5);

			cx = (int)((p->x9[i] - p->mnx) * p->scx + 0.5);
			cy = (int)((p->y9[i] - p->mny) * p->scy + 0.5);

			if (p->ocols != NULL) {
				int rgb[3];

				for (j = 0; j < 3; j++)
					rgb[j] = (int)(p->ocols[i].rgb[j] * 255.0 + 0.5);

				DeleteObject(pen);
				pen = CreatePen(PS_SOLID,ILTHICK,RGB(rgb[0],rgb[1],rgb[2]));
				SelectObject(hdc,pen);

				if (p->mtext != NULL)
					SetTextColor(hdc, RGB(rgb[0],rgb[1],rgb[2]));
			}
			MoveToEx(hdc, 10 + lx, p->sh - 10 - ly, NULL);
			LineTo(hdc, 10 + cx, p->sh - 10 - cy);
		}
		DeleteObject(pen);
	}

//	while(!kbhit());
	}

#else /* !NT */

/* ************************** APPLE OSX Cocoa version ****************** */
#ifdef __APPLE__

#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>

#ifndef CGFLOAT_DEFINED
#ifdef __LP64__
typedef double CGFloat;
#else
typedef float CGFloat;
#endif  /* defined(__LP64__) */
#endif

#ifdef DODEBUG
# define debugf(xx)	printf xx
#else
# define debugf(xx)
#endif

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


@class PLWin;
@class PLView;

/* Our static instance variables */
typedef struct {
	PLWin *window;				/* NSWindow */
	PLView *view;				/* NSTextField */
	volatile int plot_signal;	/* Signal a key or quit */
} cntx_t;

/* Global plot instanc */

cntx_t *plot_cx = NULL;

// - - - - - - - - - - - - - - - - - - - - - - - - - 
@interface PLView : NSView {
	cntx_t *cntx;
}
- (void)setCntx:(cntx_t *)cntx;
@end

@implementation PLView

- (void)setCntx:(cntx_t *)val {
	cntx = val;
}

/* This function does the work */
static void DoPlot(NSRect *rect, plot_info *pdp);

- (void)drawRect:(NSRect)rect {

	/* Use global plot data struct for now */
	DoPlot(&rect, &pd);
}

@end

// - - - - - - - - - - - - - - - - - - - - - - - - - 

@interface PLWin : NSWindow {
	cntx_t *cntx;
}
- (void)setCntx:(cntx_t *)cntx;
@end

@implementation PLWin

- (void)setCntx:(cntx_t *)val {
	cntx = val;
}

- (BOOL)canBecomeMainWindow {
	return YES;
}

- (void)keyDown:(NSEvent *)event {
	const char *chars;

	chars = [[event characters] UTF8String];
//	printf("Got Window KeyDown type %d char %s\n",(int)[event type], chars);

	if (chars[0] == ' '
	 || chars[0] == '\r') {
//		printf("Set plot_signal = 1\n");
		plot_cx->plot_signal = 1;
	}
}

- (BOOL)windowShouldClose:(id)sender {
//	printf("Got Window windowShouldClose\n");

    [NSApp terminate: nil];
    return YES;
}

@end

/* Create our window */
static void create_my_win(cntx_t *cx) {
	NSRect wRect;

	/* Create Window */
   	wRect.origin.x = 100;
	wRect.origin.y = 100;
	wRect.size.width = (int)(DEFWWIDTH*pd.ratio+0.5);
	wRect.size.height = DEFWHEIGHT;

	cx->window = [[PLWin alloc] initWithContentRect: wRect
                                        styleMask: (NSTitledWindowMask |
	                                                NSClosableWindowMask |
                                                    NSMiniaturizableWindowMask |
                                                    NSResizableWindowMask)
                                          backing: NSBackingStoreBuffered
                                            defer: YES
	                                       screen: nil];		/* Main screen */

	[cx->window setBackgroundColor: [NSColor whiteColor]];

	[cx->window setLevel: NSMainMenuWindowLevel];		// ~~99

	[cx->window setTitle: @"PlotWin"];

	/* Use our view for the whole window to draw plot */
	cx->view = [PLView new];
	[cx->view setCntx:(void *)cx];
	[cx->window setContentView: cx->view];

	[cx->window makeKeyAndOrderFront: nil];

//	[cx->window makeMainWindow];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
	Cocoa NSApp is pretty horrible - it will only get user events
	if created and run in the main thread. So the only way we can
	decouble the windows from the application is to intercept
	main() and create a secondary thread to run the appication while
	leaving main() in reserve for creating an NSApp and windows.
 */

/* Superset implementation function: */
/* return 0 on success, -1 on error */
/* Hybrid Graph uses x1 : y1, y2, y3, y4, y5, y6 for up to 6 graph curves + */
/* optional diagonal crosses at x7, y7 in yellow (x2 == NULL). */
/* Vector uses x1, y1 to x2, y2 as a vector with a (optional) diagonal cross at x2, y2 */
/* all in black with annotation ntext at the cross, */
/* plus a (optiona) diagonal cross at x7, y7 in yellow. The color for x7, y7 can be */
/* overidden by an array of colors mcols, plus optional label text mtext. (x2 != NULL) */
/* n = number of points/vectors. -ve for reversed X axis */
/* m = number of extra points (x2,y3 or x7,y7) */
/* x8,y8 to x9,y9 are extra optional vectors with optional colors */
static int do_plot_imp(
	int flags,
    double xmin, double xmax, double ymin, double ymax,	/* Bounding box */
	double ratio,	/* Aspect ratio of window, X/Y */
	int dowait,		/* > 0 wait for user to hit space key, < 0 delat dowait seconds. */
    double *x1, double *x2,
    double *yy[MXGPHS], char **ntext,
	int n,
	double *x7, double *y7, plot_col *mcols, char **mtext,
    int m,
	double *x8, double *y8, double *x9, double*y9, plot_col *ocols,
	int o
) {
	/* Put information in global pd */
	{
		int j;
		double xr,yr;

		pd.flags = flags;
		pd.dowait = dowait;
		pd.ratio = ratio;

		pd.mnx = xmin;
		pd.mny = ymin;
		pd.mxx = xmax;
		pd.mxy = ymax;

		/* Allow some extra arround plot */
		xr = pd.mxx - pd.mnx;
		yr = pd.mxy - pd.mny;
		if (xr < 1e-6)
			xr = 1e-6;
		if (yr < 1e-6)
			yr = 1e-6;
		pd.mnx -= xr/10.0;
		pd.mxx += xr/10.0;
		pd.mny -= yr/10.0;
		pd.mxy += yr/10.0;

		/* Transfer raw point info */
		if (x2 == NULL)
			pd.graph = 1;		/* 6 graphs + points */
		else
			pd.graph = 0;
		pd.x1 = x1;
		pd.x2 = x2;
		for (j = 0; j < MXGPHS; j++)
			pd.yy[j] = yy[j];
		pd.ntext = ntext;
		pd.n = abs(n);

		if (n < 0) {
			double tt;
			tt = pd.mxx;
			pd.mxx = pd.mnx;
			pd.mnx = tt;
			pd.revx = 1;
		} else {
			pd.revx = 0;
		}
		pd.x7 = x7;
		pd.y7 = y7;
		pd.mcols = mcols;
		pd.mtext = mtext;
		pd.m = abs(m);

		pd.x8 = x8;
		pd.y8 = y8;
		pd.x9 = x9;
		pd.y9 = y9;
		pd.ocols = ocols;
		pd.o = abs(o);
	}

	/* If we may be in a different thread to the main thread or */
	/* the application thread, establish our own pool. */
	NSAutoreleasePool *tpool = nil;
	if (pthread_self() != ui_thid
	 && pthread_self() != ui_main_thid)
		tpool = [NSAutoreleasePool new];

	/* If needed, create the indow */
	if (plot_cx == NULL) {

		/* If there is no NSApp, then we haven't run main() in libui before */
		/* main() in the application. */
		if (NSApp == nil) {
			fprintf(stderr,"NSApp is nil - need to rename main() to main() and link with libui !\n");
			exit(1);
		}

		if ((plot_cx = (cntx_t *)calloc(sizeof(cntx_t), 1)) == NULL)
			error("new_dispwin: Malloc failed (cntx_t)\n");

		create_my_win(plot_cx);

	} else {	/* Trigger an update */
		[plot_cx->view setNeedsDisplay: YES ];
	}

	/* (Main thread will service events) */

	/* Wait until for a key if we should */
	if (dowait > 0) {		/* Wait for a space key */
		for (plot_cx->plot_signal = 0; plot_cx->plot_signal == 0;) {
		    struct timespec ts;

		    ts.tv_sec = 100 / 1000;
    		ts.tv_nsec = (100 % 1000) * 1000000;
		    nanosleep(&ts, NULL);
	    }
	} else if (dowait < 0) {
	    struct timespec ts;
		int msec = -dowait * 1000;

	    ts.tv_sec = msec / 1000;
   		ts.tv_nsec = (msec % 1000) * 1000000;
	    nanosleep(&ts, NULL);
	}

	if (tpool != nil)
		[tpool release];

	return 0;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - -  */
/* Cleanup code (not called) */

static void cleanup() {
	[plot_cx->window release];		/* Take down the plot window */
	free(plot_cx);
	plot_cx = NULL;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Utility to draw text in Cocoa with centering */
/* Size is points */
/* Flags 0x1 == horizontal center */
/* Flags 0x2 == vertical center */
static void ADrawText(NSColor *col, float size, float x, float y, int flags, char *text) {
	NSFont* font = [NSFont systemFontOfSize:size];
	NSDictionary *att = [NSDictionary dictionaryWithObjectsAndKeys:
				font, NSFontAttributeName,
	            col, NSForegroundColorAttributeName,
				nil];
	NSString *str = [[NSString alloc] initWithUTF8String: text];

	if (flags != 0x0) {
		NSSize size;

		/* Figure out how big it will be */
		size = [str sizeWithAttributes: att ];
		if (flags & 0x1) {
			double w = fabs(size.width);
			x -= 0.5 * w;
		}
		if (flags & 0x2) {
			double h = fabs(size.height);
			y -= 0.5 * h;
		}
	}

	[str drawAtPoint: NSMakePoint(x, y) withAttributes: att];
	[str release];		/* Others are autorelease */
}

/* Draw a line */
/* We re-use the same path so that we can set the dash style */
static void ADrawLine(NSBezierPath *path, float xs, float ys, float xe, float ye) {
	[path removeAllPoints ];
	[path moveToPoint:NSMakePoint(xs, ys)];
	[path lineToPoint:NSMakePoint(xe, ye)];
	[path stroke];
}

/* Draw X axis grid lines */
void
xtick(
plot_info *pdp,
NSBezierPath *path,
NSColor *lcol,
NSColor *tcol,
double x, char *lab
) {
	float xx, yy;

	xx = 20.0 + (x - pdp->mnx) * pdp->scx;
	yy = 20.0;

	[lcol setStroke];		/* There is a bug in 10.4 which resets this after each stroke */
	ADrawLine(path, xx, yy, xx, (float)pdp->sh);
	ADrawText(tcol, 10.0, xx, 5.0, 0x1, lab);
}

/* Draw Y axis grid lines */
void
ytick(
plot_info *pdp,
NSBezierPath *path,
NSColor *lcol,
NSColor *tcol,
double y, char *lab
) {
	float xx, yy;

	xx = 20.0;
	yy = 20.0 + (y - pdp->mny) * pdp->scy;

	[lcol setStroke];		/* There is a bug in 10.4 which resets this after each stroke */
	ADrawLine(path, xx, yy, (float)pdp->sw, yy);
	ADrawText(tcol, 10.0, 3.0, yy, 0x2, lab);
}

void
loose_label(
plot_info *pdp,
NSBezierPath *path,
NSColor *lcol,
NSColor *tcol,
double min, double max,
void (*pfunc)(plot_info *pdp, NSBezierPath *path, NSColor *lcol, NSColor *tcol, double, char *)
) {
	char str[6], temp[20];
	int nfrac;
	double d;
	double graphmin, graphmax;
	double range,x;

	range = nicenum(min-max,0);
	d = nicenum(range/(NTICK-1),1);
	graphmin = floor(min/d) * d;
	graphmax = ceil(max/d) * d;
	nfrac = (int)MAX(-floor(log10(d)),0);
	sprintf(str,"%%.%df", nfrac);
	for (x = graphmin; x < graphmax + 0.5 * d; x += d) {
		sprintf(temp,str,x);
		pfunc(pdp, path, lcol, tcol, x, temp);
	}
}

/* Called from within view to plot overall graph  */
static void DoPlot(NSRect *rect, plot_info *pdp) {
	int i, j;
	int lx,ly;		/* Last x,y */
	CGFloat dash_list[2] = {7.0, 2.0};
	/* Note path and tcol are autorelease */
	NSBezierPath *path = [NSBezierPath bezierPath];		/* Path to use */
	NSColor *lcol = nil;
	NSColor *tcol = nil;

	/* Setup the plot info structure for this drawing */
	/* Note port rect is raster like, pdp/Quartz2D is Postscript like */
	pdp->sx = rect->origin.x; 
	pdp->sy = rect->origin.y; 
	pdp->sw = rect->size.width; 
	pdp->sh = rect->size.height; 
	pdp->scx = (pdp->sw - 20)/(pdp->mxx - pdp->mnx);
	pdp->scy = (pdp->sh - 20)/(pdp->mxy - pdp->mny);

	/* Plot the axis lines */
	[path setLineWidth:1.0];
	[path setLineDash: dash_list count: 2 phase: 0.0 ];	/* Set dashed lines for axes */

	/* Make sure text is black */
	tcol = [NSColor colorWithCalibratedRed: 0.0
	                           green: 0.0
	                            blue: 0.0
	                           alpha: 1.0];

	lcol = [NSColor colorWithCalibratedRed:0.7 green: 0.7 blue:0.7 alpha: 1.0];	/* Grey */

	/* Plot horizontal axis */
	if (pdp->revx)
		loose_label(pdp, path, lcol, tcol, pdp->mxx, pdp->mnx, xtick);
	else
		loose_label(pdp, path, lcol, tcol, pdp->mnx, pdp->mxx, xtick);

	/* Plot vertical axis */
	loose_label(pdp, path, lcol, tcol, pdp->mny, pdp->mxy, ytick);

	/* Set to non-dashed line */
	[path setLineWidth: LTHICK];
	[path setLineDash: NULL count: 0 phase: 0.0 ];

	if (pdp->graph) {		/* Up to 6 graphs */
		int gcolors[MXGPHS][3] = {
			{   0,   0,   0},	/* Black */
			{ 210,  30,   0},	/* Red */
			{   0, 200,  90},	/* Green */
			{   0,  10, 255},	/* Blue */
			{ 200, 200,   0},	/* Yellow */
			{ 220,   0, 255},	/* Purple */
			{ 136,  86,  68},	/* Brown */
			{ 248,  95,   0},	/* Orange */
			{ 160, 160, 160},	/* Grey */
			{ 220, 220, 220}	/* White */
		};
		for (j = MXGPHS-1; j >= 0; j--) {
			double *yp = pdp->yy[j];
		
			if (yp == NULL)
				continue;
			[[NSColor colorWithCalibratedRed: gcolors[j][0]/255.0
			                           green: gcolors[j][1]/255.0
			                            blue: gcolors[j][2]/255.0
			                           alpha: 1.0] setStroke];

			if (pdp->n > 0) {
				lx = (int)((pdp->x1[0] - pdp->mnx) * pdp->scx + 0.5);
				ly = (int)((     yp[0] - pdp->mny) * pdp->scy + 0.5);
			}

			for (i = 1; i < pdp->n; i++) {
				int cx,cy;
				cx = (int)((pdp->x1[i] - pdp->mnx) * pdp->scx + 0.5);
				cy = (int)((     yp[i] - pdp->mny) * pdp->scy + 0.5);

				ADrawLine(path, 20.0 + lx, 20.0 + ly, 20 + cx, 20.0 + cy);
				if (pdp->flags & PLOTF_GRAPHCROSSES) {
					ADrawLine(path, 20.0 + cx - 5, 20.0 - cy - 5, 20.0 + cx + 5, 20.0 + cy + 5);
					ADrawLine(path, 20.0 + cx + 5, 20.0 - cy - 5, 20.0 + cx - 5, 20.0 + cy + 5);
				}
				lx = cx;
				ly = cy;
			}
		}

	} else {	/* Vectors */
		[[NSColor colorWithCalibratedRed: 0.0
		                           green: 0.0
		                            blue: 0.0
		                           alpha: 1.0] setStroke];
		if (pdp->ntext != NULL) {
			tcol = [NSColor colorWithCalibratedRed: 0.0
			                           green: 0.0
			                            blue: 0.0
			                           alpha: 1.0];
		}
		for (i = 0; i < pdp->n; i++) {
			int cx,cy;

			lx = (int)((pdp->x1[i] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((pdp->yy[0][i] - pdp->mny) * pdp->scy + 0.5);

			cx = (int)((pdp->x2[i] - pdp->mnx) * pdp->scx + 0.5);
			cy = (int)((pdp->yy[1][i] - pdp->mny) * pdp->scy + 0.5);

			ADrawLine(path, 20.0 + lx, 20.0 + ly, 20.0 + cx, 20.0 + cy);

			if (pdp->flags & PLOTF_VECCROSSES) {
				ADrawLine(path, 20.0 + cx - 5, 20.0 + cy - 5, 20.0 + cx + 5, 20.0 + cy + 5);
				ADrawLine(path, 20.0 + cx + 5, 20.0 + cy - 5, 20.0 + cx - 5, 20.0 + cy + 5);
			}

			if (pdp->ntext != NULL)
				ADrawText(tcol, 9.0, 20.0 + cx + 9, 20.0 + cy - 7, 0x1, pdp->ntext[i]);
		}
	}

	/* Extra points */
	if (pdp->x7 != NULL && pdp->y7 != NULL && pdp->m > 0 ) {
		[[NSColor colorWithCalibratedRed: 0.82		/* Orange ? */
		                           green: 0.59
		                            blue: 0.0
		                           alpha: 1.0] setStroke];
	
		for (i = 0; i < pdp->m; i++) {

			if (pdp->mcols != NULL) {
				[[NSColor colorWithCalibratedRed: pdp->mcols[i].rgb[0]
				                           green: pdp->mcols[i].rgb[1]
				                            blue: pdp->mcols[i].rgb[2]
				                           alpha: 1.0] setStroke];

				if (pdp->mtext != NULL) {
					tcol = [NSColor colorWithCalibratedRed: pdp->mcols[i].rgb[0]
					                           green: pdp->mcols[i].rgb[1]
					                            blue: pdp->mcols[i].rgb[2]
					                           alpha: 1.0];
				}
			}
			lx = (int)((pdp->x7[i] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((pdp->y7[i] - pdp->mny) * pdp->scy + 0.5);

			ADrawLine(path, 20.0 + lx - 5, 20.0 + ly, 20.0 + lx + 5, 20.0 + ly);
			ADrawLine(path, 20.0 + lx, 20.0 + ly - 5, 20.0 + lx, 20.0 + ly + 5);

			if (pdp->mtext != NULL) {
				ADrawText(tcol, 9.0, 20.0 + lx + 9, 20.0 + ly + 7, 0x1, pdp->mtext[i]);
			}
		}
	}

	/* Extra vectors */
	if (pdp->x8 != NULL && pdp->y8 != NULL && pdp->x9 != NULL && pdp->y9 && pdp->o > 0 ) {
		[[NSColor colorWithCalibratedRed: 0.5		/* Light blue */
		                           green: 0.9
		                            blue: 0.9
		                           alpha: 1.0] setStroke];
	
		for (i = 0; i < pdp->o; i++) {
			int cx,cy;

			if (pdp->ocols != NULL) {
				[[NSColor colorWithCalibratedRed: pdp->ocols[i].rgb[0]
				                           green: pdp->ocols[i].rgb[1]
				                            blue: pdp->ocols[i].rgb[2]
				                           alpha: 1.0] setStroke];
				if (pdp->mtext != NULL) {
					tcol = [NSColor colorWithCalibratedRed: pdp->ocols[i].rgb[0]
					                           green: pdp->ocols[i].rgb[1]
					                            blue: pdp->ocols[i].rgb[2]
					                           alpha: 1.0];
				}
			}
			lx = (int)((pdp->x8[i] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((pdp->y8[i] - pdp->mny) * pdp->scy + 0.5);

			cx = (int)((pdp->x9[i] - pdp->mnx) * pdp->scx + 0.5);
			cy = (int)((pdp->y9[i] - pdp->mny) * pdp->scy + 0.5);

			ADrawLine(path, 20.0 + lx, 20.0 + ly, 20.0 + cx, 20.0 + cy);
		}
	}
}

#else /* Assume UNIX + X11 */
/* ********************************** X11 version ********************** */

/* !!!! There is a problem if the user closes the window - an X11 error results */
/*      This seems to happen before a DestroyNotify !. How to fix ??? !!!! */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef DODEBUG
# define debugf(xx)	printf xx
#else
# define debugf(xx)
#endif

void DoPlot(Display *mydisplay, Window mywindow, GC mygc, plot_info *pdp);

/* Superset implementation function: */
/* return 0 on success, -1 on error */
/* Hybrid Graph uses x1 : y1, y2, y3, y4, y5, y6 for up to 6 graph curves + */
/* optional diagonal crosses at x7, y7 in yellow (x2 == NULL). */
/* Vector uses x1, y1 to x2, y2 as a vector with a (optional) diagonal cross at x2, y2 */
/* all in black with annotation ntext at the cross, */
/* plus a (optiona) diagonal cross at x7, y7 in yellow. The color for x7, y7 can be */
/* overidden by an array of colors mcols, plus optional label text mtext. (x2 != NULL) */
/* n = number of points/vectors. -ve for reversed X axis */
/* m = number of extra points (x2,y3 or x7,y7) */
/* x8,y8 to x9,y9 are extra optional vectors with optional colors */
static int do_plot_imp(
	int flags,
    double xmin, double xmax, double ymin, double ymax,	/* Bounding box */
	double ratio,	/* Aspect ratio of window, X/Y */
	int dowait,		/* > 0 wait for user to hit space key, < 0 delat dowait seconds. */
    double *x1, double *x2,
    double *yy[MXGPHS], char **ntext,
	int n,
	double *x7, double *y7, plot_col *mcols, char **mtext,
    int m,
	double *x8, double *y8, double *x9, double*y9, plot_col *ocols,
	int o
) {
	{
		int j;
		double xr,yr;

		pd.flags = flags;
		pd.dowait = dowait;
		pd.ratio = ratio;

		pd.mnx = xmin;
		pd.mny = ymin;
		pd.mxx = xmax;
		pd.mxy = ymax;

		/* Allow some extra arround plot */
		xr = pd.mxx - pd.mnx;
		yr = pd.mxy - pd.mny;
		if (xr < 1e-6)
			xr = 1e-6;
		if (yr < 1e-6)
			yr = 1e-6;
		pd.mnx -= xr/10.0;
		pd.mxx += xr/10.0;
		pd.mny -= yr/10.0;
		pd.mxy += yr/10.0;

		/* Transfer raw point info */
		if (x2 == NULL)
			pd.graph = 1;		/* 6 graphs + points */
		else
			pd.graph = 0;
		pd.x1 = x1;
		pd.x2 = x2;
		for (j = 0; j < MXGPHS; j++)
			pd.yy[j] = yy[j];
		pd.ntext = ntext;
		pd.n = abs(n);

		if (n < 0) {
			double tt;
			tt = pd.mxx;
			pd.mxx = pd.mnx;
			pd.mnx = tt;
			pd.revx = 1;
		} else {
			pd.revx = 0;
		}
		pd.x7 = x7;
		pd.y7 = y7;
		pd.mcols = mcols;
		pd.mtext = mtext;
		pd.m = abs(m);

		pd.x8 = x8;
		pd.y8 = y8;
		pd.x9 = x9;
		pd.y9 = y9;
		pd.ocols = ocols;
		pd.o = abs(o);
	}

	{
		/* stuff for X windows */
		char plot[] = {"plot"};
		static Display *mydisplay = NULL;
		static Window mywindow = -1;
		int dorefresh = 1;
		GC mygc;
		XEvent myevent;
		XSizeHints myhint;
		XWindowAttributes mywattributes;
		int myscreen;
		unsigned long myforeground,mybackground;
		int done;
	
		/* open the display */
		if (mydisplay == NULL) {
			mydisplay = XOpenDisplay("");
			if(!mydisplay)
				error("Unable to open display");
			dorefresh = 0;
		}
		myscreen = DefaultScreen(mydisplay);
		mybackground = WhitePixel(mydisplay,myscreen);
		myforeground = BlackPixel(mydisplay,myscreen);
	
		myhint.x = 100;
		myhint.y = 100;
		myhint.width = (int)(DEFWWIDTH * ratio + 0.5);
		myhint.height = DEFWHEIGHT;
		myhint.flags = PPosition | USSize;
	
		debugf(("Opened display OK\n"));
	
		if (mywindow == -1) {
			debugf(("Opening window\n"));
			mywindow = XCreateSimpleWindow(mydisplay,
					DefaultRootWindow(mydisplay),
					myhint.x,myhint.y,myhint.width,myhint.height,
					5, myforeground,mybackground);
			XSetStandardProperties(mydisplay,mywindow,plot,plot,None,
			       NULL,0, &myhint);
		}
	
		mygc = XCreateGC(mydisplay,mywindow,0,0);
		XSetBackground(mydisplay,mygc,mybackground);
		XSetForeground(mydisplay,mygc,myforeground);
		
		XSelectInput(mydisplay,mywindow,
		     KeyPressMask | ExposureMask | StructureNotifyMask);
	
		if (dorefresh) {
			XExposeEvent ev;

			ev.type = Expose;
			ev.display = mydisplay;
			ev.send_event = True;
			ev.window = mywindow;
			ev.x = 0;
			ev.y = 0;
			ev.width = myhint.width;
			ev.height = myhint.height;
			ev.count = 0;

			XClearWindow(mydisplay, mywindow);
			XSendEvent(mydisplay, mywindow, False, ExposureMask, (XEvent *)&ev);
			
		} else {
			XMapRaised(mydisplay,mywindow);
			debugf(("Raised window\n"));
		}
	
		/* Main event loop */
		debugf(("About to enter main loop\n"));
		done = 0;
		while(done == 0) {
			XNextEvent(mydisplay,&myevent);
			switch(myevent.type) {
				case Expose:
					if(myevent.xexpose.count == 0) {	/* Repare the exposed region */
						XGetWindowAttributes(mydisplay, mywindow, & mywattributes);
						/* Setup the plot info structure for this drawing */
						pd.sx = mywattributes.x; 
						pd.sy = mywattributes.y; 
						pd.sw = mywattributes.width; 
						pd.sh = mywattributes.height; 
						pd.scx = (pd.sw - 10)/(pd.mxx - pd.mnx);
						pd.scy = (pd.sh - 10)/(pd.mxy - pd.mny);
  
						DoPlot(mydisplay,mywindow, mygc, &pd);

						if (pd.dowait <= 0) {		/* Don't wait */
							if (pd.dowait < 0)
								sleep(-pd.dowait);
							debugf(("Not waiting, so set done=1\n"));
							done = 1;
						}
					}
					break;
				case MappingNotify:
					XRefreshKeyboardMapping(&myevent.xmapping);
					break;
				case KeyPress:
					debugf(("Got a button press\n"));
					done = 1;
					break;
			}
		}
		debugf(("About to close display\n"));
		XFreeGC(mydisplay,mygc);
//		XDestroyWindow(mydisplay,mywindow);
//		XCloseDisplay(mydisplay);
		debugf(("finished\n"));
	}
	return 0;
}

/* Draw X axis grid lines */
void
xtick(
Display *mydisplay,
Window mywindow,
GC mygc,
plot_info *pdp,
double x, char *lab
) {
	int xx,yy;

	xx = 10 + (int)((x - pdp->mnx) * pdp->scx + 0.5);
	yy = pdp->sh - 10;

	XDrawLine(mydisplay, mywindow, mygc, xx, yy, xx, 0);
	XDrawImageString(mydisplay, mywindow, mygc, xx-6, yy, lab, strlen(lab));
}

/* Draw Y axis grid lines */
void
ytick(
Display *mydisplay,
Window mywindow,
GC mygc,
plot_info *pdp,
double y, char *lab
) {
	int xx,yy;

	xx = 5;
	yy = pdp->sh - 10 - (int)((y - pdp->mny) * pdp->scy + 0.5);

	XDrawLine(mydisplay, mywindow, mygc, xx, yy, pdp->sw, yy);
	XDrawImageString(mydisplay, mywindow, mygc, xx, yy+4, lab, strlen(lab));
}

void
loose_label(
Display *mydisplay,
Window mywindow,
GC mygc,
plot_info *pdp,
double min, double max,
void (*pfunc)(Display *mydisplay, Window mywindow, GC mygc, plot_info *pdp, double, char *)
) {
	char str[6], temp[20];
	int nfrac;
	double d;
	double graphmin, graphmax;
	double range,x;

	range = nicenum(min-max,0);
	d = nicenum(range/(NTICK-1),1);
	graphmin = floor(min/d) * d;
	graphmax = ceil(max/d) * d;
	nfrac = (int)MAX(-floor(log10(d)),0);
	sprintf(str,"%%.%df", nfrac);
	for (x = graphmin; x < graphmax + 0.5 * d; x += d) {
		sprintf(temp,str,x);
		pfunc(mydisplay, mywindow, mygc, pdp, x, temp);
	}
}

void
DoPlot(
Display *mydisplay,
Window mywindow,
GC mygc,
plot_info *pdp
) {
	int i, j;
	int lx,ly;		/* Last x,y */
	char dash_list[2] = {5, 1};
	Colormap mycmap;
	XColor col;

	mycmap = DefaultColormap(mydisplay, 0);
	col.red = col.green = col.blue = 150 * 256;
	XAllocColor(mydisplay, mycmap, &col);
	XSetForeground(mydisplay,mygc, col.pixel);

	/* Set dashed lines for axes */
	XSetLineAttributes(mydisplay, mygc, 1, LineOnOffDash, CapButt, JoinBevel);
	XSetDashes(mydisplay, mygc, 0, dash_list, 2);
	// ~~ doesn't seem to work. Why ?

	/* Plot horizontal axis */
	if (pdp->revx)
		loose_label(mydisplay, mywindow, mygc, pdp, pdp->mxx, pdp->mnx, xtick);
	else
		loose_label(mydisplay, mywindow, mygc, pdp, pdp->mnx, pdp->mxx, xtick);

	/* Plot vertical axis */
	loose_label(mydisplay, mywindow, mygc, pdp, pdp->mny, pdp->mxy, ytick);

	if (pdp->graph) {		/* Up to 10 graphs */
		for (j = MXGPHS-1; j >= 0; j--) {
			double *yp = pdp->yy[j];
		
			if (yp == NULL)
				continue;

			col.red   = gcolors[j][0] * 256;
			col.green = gcolors[j][1] * 256;
			col.blue  = gcolors[j][2] * 256;
			XAllocColor(mydisplay, mycmap, &col);
			XSetForeground(mydisplay,mygc, col.pixel);
			XSetLineAttributes(mydisplay, mygc, ILTHICK, LineSolid, CapButt, JoinBevel);

			lx = (int)((pdp->x1[0] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((     yp[0] - pdp->mny) * pdp->scy + 0.5);

			for (i = 0; i < pdp->n; i++) {
				int cx,cy;
				cx = (int)((pdp->x1[i] - pdp->mnx) * pdp->scx + 0.5);
				cy = (int)((     yp[i] - pdp->mny) * pdp->scy + 0.5);

				XDrawLine(mydisplay, mywindow, mygc, 10 + lx, pdp->sh - 10 - ly, 10 + cx, pdp->sh - 10 - cy);
				if (pdp->flags & PLOTF_GRAPHCROSSES) {
					XDrawLine(mydisplay, mywindow, mygc, 10 + cx - 5, pdp->sh - 10 - cy - 5, 10 + cx + 5, pdp->sh - 10 - cy + 5);
					XDrawLine(mydisplay, mywindow, mygc, 10 + cx + 5, pdp->sh - 10 - cy - 5, 10 + cx - 5, pdp->sh - 10 - cy + 5);
				}
				lx = cx;
				ly = cy;
			}
		}

	} else {	/* Vectors */

		col.red = col.green = col.blue = 0 * 256;
		XAllocColor(mydisplay, mycmap, &col);
		XSetForeground(mydisplay,mygc, col.pixel);
		XSetLineAttributes(mydisplay, mygc, ILTHICK, LineSolid, CapButt, JoinBevel);

		for (i = 0; i < pdp->n; i++) {
			int cx,cy;

			lx = (int)((pdp->x1[i] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((pdp->yy[0][i] - pdp->mny) * pdp->scy + 0.5);

			cx = (int)((pdp->x2[i] - pdp->mnx) * pdp->scx + 0.5);
			cy = (int)((pdp->yy[1][i] - pdp->mny) * pdp->scy + 0.5);

			/* Vector */
			XDrawLine(mydisplay, mywindow, mygc, 10 + lx, pdp->sh - 10 - ly, 10 + cx, pdp->sh - 10 - cy);

			if (pdp->flags & PLOTF_VECCROSSES) {
				/* Cross at end of vector */
				XDrawLine(mydisplay, mywindow, mygc, 10 + cx - 5, pdp->sh - 10 - cy - 5, 10 + cx + 5, pdp->sh - 10 - cy + 5);
				XDrawLine(mydisplay, mywindow, mygc, 10 + cx + 5, pdp->sh - 10 - cy - 5, 10 + cx - 5, pdp->sh - 10 - cy + 5);
			}

			if (pdp->ntext != NULL)
				XDrawImageString(mydisplay, mywindow, mygc, 10 + cx + 5, pdp->sh - 10 - cy + 7,
				                 pdp->ntext[i], strlen(pdp->ntext[i]));
		}
	}

	/* Extra points */
	if (pdp->x7 != NULL && pdp->y7 != NULL && pdp->m > 0 ) {
		col.red = 210 * 256; col.green = 150 * 256; col.blue = 0 * 256;
		XAllocColor(mydisplay, mycmap, &col);
		XSetForeground(mydisplay,mygc, col.pixel);
		XSetLineAttributes(mydisplay, mygc, ILTHICK, LineSolid, CapButt, JoinBevel);

		for (i = 0; i < pdp->m; i++) {
			lx = (int)((pdp->x7[i] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((pdp->y7[i] - pdp->mny) * pdp->scy + 0.5);

			if (pdp->mcols != NULL) {
				col.red = (int)(pdp->mcols[i].rgb[0] * 65535.0 + 0.5);
				col.green = (int)(pdp->mcols[i].rgb[1] * 65535.0 + 0.5);
				col.blue = (int)(pdp->mcols[i].rgb[2] * 65535.0 + 0.5);

				XAllocColor(mydisplay, mycmap, &col);
				XSetForeground(mydisplay,mygc, col.pixel);

			}
			XDrawLine(mydisplay, mywindow, mygc, 10 + lx - 5, pdp->sh - 10 - ly,
			                                     10 + lx + 5, pdp->sh - 10 - ly);
			XDrawLine(mydisplay, mywindow, mygc, 10 + lx, pdp->sh - 10 - ly - 5,
			                                     10 + lx, pdp->sh - 10 - ly + 5);

			if (pdp->mtext != NULL)
				XDrawImageString(mydisplay, mywindow, mygc, 10 + lx + 5, pdp->sh - 10 - ly - 7,
				                 pdp->mtext[i], strlen(pdp->mtext[i]));
		}
	}

	/* Extra vectors */
	if (pdp->x8 != NULL && pdp->y8 != NULL && pdp->x9 != NULL && pdp->y9 && pdp->o > 0 ) {
		col.red = 150 * 256; col.green = 255 * 256; col.blue = 255 * 256;
		XAllocColor(mydisplay, mycmap, &col);
		XSetForeground(mydisplay,mygc, col.pixel);
		XSetLineAttributes(mydisplay, mygc, ILTHICK, LineSolid, CapButt, JoinBevel);

		for (i = 0; i < pdp->o; i++) {
			int cx,cy;

			lx = (int)((pdp->x8[i] - pdp->mnx) * pdp->scx + 0.5);
			ly = (int)((pdp->y8[i] - pdp->mny) * pdp->scy + 0.5);

			cx = (int)((pdp->x9[i] - pdp->mnx) * pdp->scx + 0.5);
			cy = (int)((pdp->y9[i] - pdp->mny) * pdp->scy + 0.5);

			if (pdp->ocols != NULL) {
				col.red = (int)(pdp->ocols[i].rgb[0] * 65535.0 + 0.5);
				col.green = (int)(pdp->ocols[i].rgb[1] * 65535.0 + 0.5);
				col.blue = (int)(pdp->ocols[i].rgb[2] * 65535.0 + 0.5);

				XAllocColor(mydisplay, mycmap, &col);
				XSetForeground(mydisplay,mygc, col.pixel);

			}

			XDrawLine(mydisplay, mywindow, mygc, 10 + lx, pdp->sh - 10 - ly, 10 + cx, pdp->sh - 10 - cy);
		}
	}
}

#endif /* UNIX + X11 */
#endif /* !NT */
/***********************************************************************/


/* Nice graph labeling functions */

#define expt(a,n) pow(a,(double)(n))

double nicenum(double x, int round) {
	int ex;
	double f;
	double nf;
// printf("nocenum called with %f and %d\n",x,round);
	if (x < 0.0)
		x = -x;
	ex = (int)floor(log10(x));
// printf("ex = %d\n",ex);
	f = x/expt(10.0,ex);
// printf("f = %f\n",f);
	if (round) {
		if (f < 1.5) nf = 1.0;
		else if (f < 3.0) nf = 2.0;
		else if (f < 7.0) nf = 5.0;
		else nf = 10.0;
	} else {
		if (f < 1.0) nf = 1.0;
		else if (f < 2.0) nf = 2.0;
		else if (f < 5.0) nf = 5.0;
		else nf = 10.0;
	}
// printf("nf = %f\n",nf);
// printf("about to return %f\n",(nf * expt(10.0, ex)));
	return (nf * expt(10.0, ex));
}

/* ---------------------------------------------------------------- */
#ifdef STANDALONE_TEST
/* test code */

//#include <windows.h>
//#include <stdio.h>
#include <fcntl.h>
//#include <io.h>


#ifdef NEVER
/* Append debugging string to log.txt */
static void dprintf(char *fmt, ...) {
	FILE *fp = NULL;
	if ((fp = fopen("log.txt", "a+")) != NULL) {
		va_list args;
		va_start(args, fmt);
		vfprintf(fp, fmt, args);
		fflush(fp);
	   	fclose(fp);
		va_end(args);
	}
}
#endif // NEVER

#ifdef NEVER	/* Other non-working enable console output code */
{
	/* This clever code have been found at:
	   Adding Console I/O to a Win32 GUI App
	   Windows Developer Journal, December 1997
	   http://dslweb.nwnexus.com/~ast/dload/guicon.htm
	   Andrew Tucker's Home Page */

	/* This is not so clever, since it doesn't work... */

	// redirect unbuffered STDOUT to the console
	long lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
	int hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	FILE *fp = _fdopen(hConHandle, "w");
	*stdout = *fp;
	setvbuf(stdout, NULL, _IONBF, 0);

	// redirect unbuffered STDIN to the console
	lStdHandle = (long)GetStdHandle(STD_INPUT_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen( hConHandle, "r" );
	*stdin = *fp;
	setvbuf(stdin, NULL, _IONBF, 0);

	// redirect unbuffered STDERR to the console
	lStdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen( hConHandle, "w" );
	*stderr = *fp;
	setvbuf(stderr, NULL, _IONBF, 0);


}
#endif // NEVER

#ifdef NEVER
// ~~~~~~~~~~~~~
//	AllocConsole();
	{
		ULONG pbi[6];
		ULONG ulSize = 0;
		LONG (WINAPI *NtQueryInformationProcess)(HANDLE ProcessHandle,
		                                         ULONG ProcessInformationClass,
		                                         PVOID ProcessInformation,
		                                         ULONG ProcessInformationLength,
		                                         PULONG ReturnLength); 

		BOOL (WINAPI *AttachConsole)(DWORD dwProcessId);

		*(FARPROC *)&NtQueryInformationProcess = 
		          GetProcAddress(LoadLibraryA("NTDLL.DLL"), "NtQueryInformationProcess");
		if(NtQueryInformationProcess) {
printf("~1 found NtQueryInformationProcess\n"); fflush(stdout);
			if(NtQueryInformationProcess(GetCurrentProcess(), 0,
			    &pbi, sizeof(pbi), &ulSize) >= 0 && ulSize == sizeof(pbi)) {
printf("~1 NtQueryInformationProcess suceeded\n"); fflush(stdout);

				*(FARPROC *)&AttachConsole = 
		          GetProcAddress(LoadLibraryA("kernel32.dll"), "AttachConsole");

				if (AttachConsole) {
printf("~1 found AttachConsole\n"); fflush(stdout);
					AttachConsole(pbi[5]);
printf("~1 about to freopen CONNOUT\n"); fflush(stdout);
					freopen("CONOUT$","wb",stdout);
				} else {
printf("~1 failed to find AttachConsole\n"); fflush(stdout);
				}
			}
		}
		// AttachConsole(ID ATTACH_PARENT_CONSOLE); 	// Should work on XP ??

		/* i mean OpenConsoleW - you are as picky as i am - its
		   ordinal=519 and it is exported by name; the header(s)
		   do not include it, which tells me there's got to be a
		   reason for that.
		 */
	}
// ~~~~~~~~~~~~~
#endif // NEVER


int main(int argc, char *argv[]) {
	double x[10]  = {0.0, 0.5, 0.7, 1.0};
	double y1[10] = {0.0, 0.5, 0.7, 1.0};
	double y2[10] = {0.9, 0.8, 1.4, 1.2};
	double y3[10] = {0.1, 0.8, 0.7, -0.1};

	double Bx1[10] = {0.0, 0.5, 0.9, 0.5};
	double By1[10] = {0.0, 0.3, 1.2, 0.2};
	double Bx2[10] = {0.1, 0.8, 0.1, 0.2};
	double By2[10] = {0.1, 1.8, 2.0, 0.5};

	double Bx3[10] = {0.8, 0.4, 1.3, 0.5, 0.23};
	double By3[10] = {0.5, 1.3, 0.4, 0.7, 0.77};

	plot_col mcols[5] = {
	{ 1.0, 0.0, 0.0 },
	{ 0.0, 1.0, 0.0 },
	{ 0.0, 0.0, 1.0 },
	{ 0.6, 0.6, 0.6 },
	{ 1.0, 1.0, 0.0 } };

	char *ntext[5] = { "A", "B", "C", "D" };
	char *mtext[5] = { "10", "20", "30", "40", "50" };

	printf("Doing first plot\n");
	if (do_plot(x,y1,y2,y3,4) < 0)
		printf("Error - do_plot returned -1!\n");

	/* Try a second plot */
	printf("Doing second plot\n");
	x[2] = 0.55;
	if (do_plot(x,y2,y3,y1,3) < 0)
		printf("Error - do_plot returned -1!\n");

	/* Try vectors */
	printf("Doing vector plot\n");
	if (do_plot_vec(0.0, 1.4, 0.0, 2.0, Bx1, By1, Bx2, By2, 4, 1, Bx3, By3, NULL, NULL, 5))
		printf("Error - do_plot_vec returned -1!\n");

	printf("Doing vector plot with colors and notation\n");
	if (do_plot_vec(0.0, 1.4, 0.0, 2.0, Bx1, By1, Bx2, By2, 4, 1, Bx3, By3, mcols, mtext, 5))
		printf("Error - do_plot_vec returned -1!\n");

	printf("Doing vector plot with colors and notation + extra vectors\n");
	if (do_plot_vec2(0.0, 1.4, 0.0, 2.0, Bx1, By1, Bx2, By2, ntext, 4, 1, Bx3, By3, mcols, mtext, 5,
	                x,y1,y2,y3,mcols,4))
		printf("Error - do_plot_vec returned -1!\n");

	printf("We're done\n");
	return 0;
}

#endif /* STANDALONE_TEST */
/* ---------------------------------------------------------------- */

