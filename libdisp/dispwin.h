
#ifndef DISPWIN_H

/* 
 * Argyll Color Correction System
 * Display target patch window
 *
 * Author: Graeme W. Gill
 * Date:   4/10/96
 *
 * Copyright 1998 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/*
 * Hmm. Should make display settling time a user overridable parameter,
 * to allow for very fast response displays such as oled ?
 */
#ifdef __cplusplus
	extern "C" {
#endif

#define PATCH_UPDATE_DELAY 200		/* default & minimum patch update delay allowance */
#define INSTRUMENT_REACTIONTIME 0	/* default nominal instrument reaction time */

/* Display rise and fall time model. This is CRT like */
#define DISPLAY_RISE_TIME 0.04		/* Assumed rise time to 90% of target level */ 
#define DISPLAY_FALL_TIME 0.25		/* Assumed fall time to 90% of target level */
#define DISPLAY_SETTLE_AIM 0.1		/* Aim for 0.2 Delta E */

#ifdef NT
#define OEMRESOURCE
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0501
#define _WIN32_WINNT 0x0501
#endif
#if !defined(WINVER) || WINVER < 0x0501
#if defined(WINVER)
# undef WINVER
#endif
#define WINVER 0x0501
#endif
#include <windows.h>
#include <icm.h>

#if(WINVER < 0x0500)
#error Require WINVER >= 0x500 to compile (multi-monitor API needed)
#endif

#ifndef COLORMGMTCAPS	/* In case SDK is out of date */

#define COLORMGMTCAPS   121

#define CM_NONE             0x00000000
#define CM_DEVICE_ICM       0x00000001
#define CM_GAMMA_RAMP       0x00000002
#define CM_CMYK_COLOR       0x00000004

#endif	/* !COLORMGMTCAPS */

/* Avoid shlwapi.h - there are problems in using it in latter SDKs */
#ifndef WINSHLWAPI
#define WINSHLWAPI DECLSPEC_IMPORT
#endif

WINSHLWAPI LPSTR WINAPI PathFindFileNameA(LPCSTR);
WINSHLWAPI LPWSTR WINAPI PathFindFileNameW(LPCWSTR);

#ifdef UNICODE
#define PathFindFileNameX PathFindFileNameW
#else
#define PathFindFileNameX PathFindFileNameA
#endif

#endif /* NT */

#ifdef __APPLE__	/* Assume OS X Cocoa */

#include <Carbon/Carbon.h>		/* To declare CGDirectDisplayID  */

#endif /* __APPLE__ */

#if defined(UNIX_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/xf86vmode.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/scrnsaver.h>
#include <X11/extensions/dpms.h>
#endif /* UNIX_X11 */

/* - - - - - - - - - - - - - - - - - - - - - - - */

/* Profile instalation/association scope */
typedef enum {
	p_scope_user     = 0,		/* (user profiles) Linux, OS X & Vista */
	p_scope_local    = 1,		/* (local system profiles) Linux, OS X & vista */
	p_scope_system   = 2,		/* (system supplied profiles) OS X. [ Linux, Vista same as local ] */
	p_scope_network  = 3		/* (shared network profiles) [ OS X. Linux, Vista same as local ] */
} p_scope;

/* - - - - - - - - - - - - - - - - - - - - - - - */
/* Enumerate and list all the available displays */

/* Structure to store infomation about possible displays */
typedef struct {
	char *name;			/* Display name */
	char *description;	/* Description of display or URL */
	int sx,sy;			/* Displays offset in pixels */
	int sw,sh;			/* Displays width and height in pixels*/
#ifdef NT
	char monid[128];	/* Monitor ID */
	int prim;			/* NZ if primary display monitor */
#endif /* NT */
#ifdef __APPLE__
	CGDirectDisplayID ddid;
#endif /* __APPLE__ */
#if defined(UNIX_X11)
	int screen;				/* Screen to select */
	int uscreen;			/* Underlying screen */
	int rscreen;			/* Underlying RAMDAC screen */
	Atom icc_atom;			/* ICC profile root atom for this display */
	unsigned char *edid;	/* 128 or 256 bytes of monitor EDID, NULL if none */
	int edid_len;			/* 128 or 256 */

#if RANDR_MAJOR == 1 && RANDR_MINOR >= 2
	/* Xrandr stuff - output is connected 1:1 to a display */
	RRCrtc crtc;				/* Associated crtc */
	RROutput output;			/* Associated output */
	Atom icc_out_atom;			/* ICC profile atom for this output */
#endif /* randr >= V 1.2 */
#endif /* UNIX_X11 */
} disppath;

/* Return pointer to list of disppath. Last will be NULL. */
/* Return NULL on failure. */
/* Call free_disppaths() to free up allocation */
disppath **get_displays();

void free_disppaths(disppath **paths);

/* Delete the display at the given index from the paths */
void del_disppath(disppath **paths, int ix);

/* Return the given display given its index 0..n-1 */
disppath *get_a_display(int ix);

void free_a_disppath(disppath *path);

extern int callback_ddebug;		/* Diagnostic global for get_displays() and get_a_display() */  

/* - - - - - - - - - - - - - - - - - - - - - - - */
/* Structure to handle RAMDAC values */
struct _ramdac {

	/* Should have separate frame buffer depth + representation to account */
	/* for floating point frame buffers, even though this isn't currently used. */

	int pdepth;		/* Frame buffer depth into RAMDAC, usually 8 */
	int nent;		/* Number of entries, =  2^pdepth */
	double *v[3];	/* 2^pdepth entries for RGB, values 0.0 - 1.0 */

	/* Clone ourselves */
	struct _ramdac *(*clone)(struct _ramdac *p);

	/* Set the curves to linear */
	void (*setlin)(struct _ramdac *p);

	/* Destroy ourselves */
	void (*del)(struct _ramdac *p);
}; typedef struct _ramdac ramdac;


/* - - - - - - - - - - - - - - - - - - - - - - - */
/* Dispwin object */
/* This is used by all the different test patch window types, */
/* dispwin, webwin, madvrwin and ccwin.
/* !!!! Make changes in dispwin.c, webwin.c, madvrwin.c & ccwin.c !!!!  */
/* !!!! if this structure gets changed. !!!! */

struct _dispwin {

/* private: */
	char *name;			/* Display path (ie. '\\.\DISPLAY1') */
						/* or "10.0.0.1:0.0" */
	char *description;	/* Description of display */

	/* Plot instance information */
//	int sx,sy;			/* Screen offset in pixels */
//	int sw,sh;			/* Screen width and height in pixels*/
//	int ww,wh;			/* Window width and height */
//	int tx,ty;			/* Test area within window offset in pixels */
//	int tw,th;			/* Test area width and height in pixels */

	double rgb[3];		/* Current color (full resolution, full range) */
	double s_rgb[3];	/* Current color (possibly scaled range) */
	double r_rgb[3];	/* Current color (raster value) */
	int out_tvenc;		/* 1 to use RGB Video Level encoding */
//	int patch_delay;	/* Measured patch update latency delay in msec, default 200 */
//	int inst_reaction;	/* Measured instrument reaction time delay in msec, default 0 */
//	double rise_time;	/* Display settling rise time */
//	double fall_time;	/* Display settling fall time */
//	double de_aim;		/* Display settling deltaE aim */
//	int min_update_delay;	/* Minimum overall update latency delay, default 20, */
							/* overriden by EnvVar */
//	double settle_mult;	/* Settling time multiplier */
//	int do_resp_time_del;	/* NZ to compute and use expected display response time */
//	int do_update_del;		/* NZ to do update delay */ 
//	double extra_update_delay;	/* Test window internal extra delay (used in delay cal.) */
	int nowin;			/* Don't create a test window */
	int native;			/*  X0 = use current per channel calibration curve */
						/*  X1 = set native linear output and use ramdac high precision */
						/*  0X = use current color management cLut (MadVR) */
						/*  1X = disable color management cLUT (MadVR) */
	ramdac *oor;		/* Original orgininal ramdac contents, NULL if not accessible */
	ramdac *or;			/* Original ramdac contents, NULL if not accessible, restored on exit */
	ramdac *r;			/* Ramdac in use for native mode or general use */
	double width, height;	/* Orginial size in mm or % */
	int blackbg;		/* NZ if black full screen background */

//	char *callout;		/* if not NULL - set color Shell callout routine */

	/* Linked list to automate SIGKILL cleanup */
//	struct _dispwin *next;

#ifdef NT
//	char monid[128];	/* Monitor ID (ie. 'Monitor\MEA1773\{4D36E96E-E325-11CE-BFC1-08002BE10318}\0015'*/
//	HDC hdc;			/* Handle to display */
//	char *AppName;
//	HWND hwnd;			/* Window handle */
//	HCURSOR curs;		/* Invisible cursor */
	
//	MSG msg;
//	ATOM arv;

//	int xo, yo, wi, he;	/* Window location & size */
//	athread *mth;		/* Window message thread */
//	int inited;
//	int quit;			/* Request to quit */

//	volatile int colupd;		/* Color update count */
//	volatile int colupde;		/* Color update count echo */

#endif /* NT */

#ifdef __APPLE__
	CGDirectDisplayID ddid;
	void *osx_cntx;			/* OSX specific info */	
	int btf;				/* Flag, nz if window has been brought to the front once */
	int winclose;			/* Flag, set to nz if window was closed */
#endif /* __APPLE__ */

#if defined(UNIX_X11)
	Display *mydisplay;
	int myscreen;			/* Usual or virtual screen with Xinerama */
	int myuscreen;			/* Underlying screen */
	int myrscreen;			/* Underlying RAMDAC screen */
	Atom icc_atom;			/* ICC profile root atom for this display */
	unsigned char *edid;	/* 128 or 256 bytes of monitor EDID, NULL if none */
	int edid_len;			/* 128 or 256 */

#if RANDR_MAJOR == 1 && RANDR_MINOR >= 2
	/* Xrandr stuff - output is connected 1:1 to a display */
	RRCrtc crtc;				/* Associated crtc */
	RROutput output;			/* Associated output */
	Atom icc_out_atom;			/* ICC profile atom for this output */
#endif /* randr >= V 1.2 */

	/* Test window access */
	Window mywindow;
	GC mygc;

    /* Screensaver state */
	int xsssuspend;				/* Was able to suspend the screensaver using XScreenSaverSuspend */

	int xssvalid;				/* Was able to save & disable X screensaver using XSetScreenSaver */
	int timeout, interval;
	int prefer_blanking;
	int allow_exposures;

	int xssrunning;				/* Disabled xscreensaver */

	int gnomessrunning;			/* Disabled gnome screensaver and is was enabled */
	pid_t gnomepid;				/* gnome-screensaver-command -i pid */

	int kdessrunning;			/* Disabled kde screensaver and is was enabled */

	int dpmsenabled;			/* DPMS is enabled */
	
#endif /* UNIX_X11 */

	void *pcntx;				/* Private context (ie., webwin, ccwin) */
//	volatile unsigned int ncix, ccix;	/* Counters to trigger webwin colorchange */
//	volatile int mg_stop;				/* Stop flag */

//	volatile int cberror;		/* NZ if error detected in a callback routine */
	int ddebug;					/* >0 to print debug to stderr */

/* public: */
	int pdepth;		/* Frame buffer plane depth of display */
//	int edepth;		/* Notional ramdac entry size in bits. (Bits actually used may be less) */
					/* This is used to scale out_tvenc appropriately */

	/* Get RAMDAC values. ->del() when finished. */
	/* Return NULL if not possible */
//	ramdac *(*get_ramdac)(struct _dispwin *p);

	/* Set the RAMDAC values. */
	/* Return nz if not possible */
//	int (*set_ramdac)(struct _dispwin *p, ramdac *r, int persist);

	/* Install a display profile and make */
	/* it the defult for this display. */
	/* Return nz if failed */
//	int (*install_profile)(struct _dispwin *p, char *fname, ramdac *r, p_scope scope);

	/* Un-install a display profile. */
	/* Return nz if failed */
//	int (*uninstall_profile)(struct _dispwin *p, char *fname, p_scope scope);

	/* Get the currently installed display profile and return it as an icmFile. */
	/* Return the name as well, up to mxlen chars, excluding nul. */
	/* Return NULL if failed */
//	icmFile *(*get_profile)(struct _dispwin *p, char *name, int mxlen);

	/* Set a color (values 0.0 - 1.0) */
	/* Return nz on error */
	int (*set_color)(struct _dispwin *p, double r, double g, double b);

	/* Set/unset the blackground color flag. */
	/* Will only change on next set_col() */
	/* Return nz on error */
	int (*set_bg)(struct _dispwin *p, int blackbg);

	/* Optional - may be NULL */
	/* set patch info */
	/* Return nz on error */
//	int (*set_pinfo)(struct _dispwin *p, int pno, int tno);

	/* Set a patch delay and instrument reaction time values. */
	/* The overall delay between patch change and triggering */
	/* the instrument is (patch_delay + display_settle - inst_reaction) */
	/* and will never be less than the min_update_delay value. */
//	void (*set_update_delay)(struct _dispwin *p, int patch_delay, int inst_reaction);

	/* Set the display settling time constants. Use -ve value to leave current value */
	/* unchanged. (These values are used as part of the update delay calculations - see above). */
//	void (*set_settling_delay)(struct _dispwin *p, double rise_time, double fall_time, double de_aim);

	/* Enable or disable the update delay. This is used to disable the update delay */
	/* when measuring the patch_delay and inst_reaction */
//	void (*enable_update_delay)(struct _dispwin *p, int enable);

	/* Set a shell set color callout command line */
//	void (*set_callout)(struct _dispwin *p, char *callout);

	/* Destroy ourselves */
	void (*del)(struct _dispwin *p);

}; typedef struct _dispwin dispwin;

/* Create a RAMDAC access and display test window, default white */
dispwin *new_dispwin(
	disppath *screen,				/* Screen to calibrate. */
	double width, double height,	/* Width and height in mm */
	double hoff, double voff,		/* Offset from c. in fraction of screen, range -1.0 .. 1.0 */
	int nowin,						/* NZ if no window should be created - RAMDAC access only */
	int native,						/* X0 = use current per channel calibration curve */
									/* X1 = set native linear output and use ramdac high precn. */
									/* 0X = use current color management cLut (MadVR) */
									/* 1X = disable color management cLUT (MadVR) */
	int *noramdac,					/* Return nz if no ramdac access. native is set to X0 */
	int *nocm,						/* Return nz if no CM cLUT access. native is set to 0X */
	int out_tvenc,					/* 1 = use RGB Video Level encoding */
	int blackbg,					/* NZ if whole screen should be filled with black */
	int override,					/* NZ if override_redirect is to be used on X11 */
	int ddebug						/* >0 to print debug statements to stderr */
);

/* Shared implementation */
void dispwin_set_default_delays(dispwin *p);
void dispwin_set_update_delay(dispwin *p, int patch_delay, int inst_reaction);
void dispwin_set_settling_delay(dispwin *p, double rise_time, double fall_time, double de_aim);
void dispwin_enable_update_delay(dispwin *p, int enable);
int dispwin_compute_delay(dispwin *p, double *orgb);

ramdac *dispwin_clone_ramdac(ramdac *r);
void dispwin_setlin_ramdac(ramdac *r);
void dispwin_del_ramdac(ramdac *r);


#ifdef __cplusplus
	}
#endif

#define DISPWIN_H
#endif /* DISPWIN_H */

