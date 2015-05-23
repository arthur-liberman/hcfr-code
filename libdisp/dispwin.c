
/* 
 * Argyll Color Correction System
 * Display target patch window
 *
 * Author: Graeme W. Gill
 * Date:   4/10/96
 *
 * Copyright 1998 - 2013, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* This program displays test patches on a WinNT, MAC OSX or X11 windowing system. */

/* TTBD
 *
 * Should probably check the display attributes (like visual depth)
 * and complain if we aren't using 24 bit color or better. 
 *
 * Ideally should distinguish clearly between not having access to RAMDAC/VideoLuts
 * (fail) vs. the display not having them at all.
 *
 * Is there a >8 bit way of setting frame buffer value on MSWin (see "Quantize")
 * when using higher bit depth frame buffers ?
 *
 * Is there a >8 bit way of getting/setting RAMDAC indexes ?
 *
 * Should add dithering support to overcome 8 bit limitations of
 * non-RAMDAC access or limited RAMDAC depth. (How do we easily
 * determine the latter ??)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#ifndef NT
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "copyright.h"
#include "aconfig.h"
#include "icc.h"
#include "numlib.h"
#include "cgats.h"
#include "conv.h"
#include "xicc.h"
#include "disptechs.h"
#include "dispwin.h"
#include "ui.h"
#include "webwin.h"
#include "ccast.h"
#include "ccwin.h"
#ifdef NT
# include "madvrwin.h"
#endif
#if defined(UNIX_X11)
# include <dlfcn.h>
# if defined(USE_UCMM)
#  include "ucmm.h"
# endif
#endif

#ifdef __APPLE__

/*
	Note that the new ColorSync API is defined in
	/System/Library/Frameworks/ApplicationServices.framework/Frameworks/ColorSync.framework/Headers

	in
		ColorSync.h
		ColorSyncBase.h
		ColorSyncCMM.h
		ColorSyncDeprecated.h
		ColorSyncDevice.h
		ColorSyncProfile.h
		ColorSyncTransform.h

	and isn't documented by Apple anywhere else.
 */

#include <Foundation/Foundation.h>

#include <AppKit/AppKit.h>

# if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
#  include <copyfile.h>
# endif

#ifndef CGFLOAT_DEFINED
#ifdef __LP64__ || NS_BUILD_32_LIKE_64
typedef double CGFloat;
#else
typedef float CGFloat;
#endif  /* defined(__LP64__) */
#endif	/* !CGFLOAT_DEFINED */

#ifndef NSINTEGER_DEFINED
#if __LP64__ || NS_BUILD_32_LIKE_64
  typedef long NSInteger;
  typedef unsigned long NSUInteger;
#else
  typedef int NSInteger;
  typedef unsigned int NSUInteger;
#endif
#endif	/* !NSINTEGER_DEFINED */

#include <IOKit/Graphics/IOGraphicsLib.h>

#if __MAC_OS_X_VERSION_MAX_ALLOWED <= 1060
/* This wasn't declared in 10.6, although it is needed */
CFUUIDRef CGDisplayCreateUUIDFromDisplayID (uint32_t displayID);
#endif	/* < 10.6 */

#endif /* __APPLE__ */

#define VERIFY_TOL (1.0/255.0)
#undef DISABLE_RANDR				/* Disable XRandR code */

#undef DEBUG
//#define STANDALONE_TEST

#ifdef DEBUG
#define errout stderr
# define debug(xx)	fprintf(errout, xx )
# define debug2(xx)	fprintf xx
# define debugr(xx)	fprintf(errout, xx )
# define debugr2(xx)	fprintf xx
# define debugrr(xx)	fprintf(errout, xx )
# define debugrr2(xx)	fprintf xx
# define debugrr2l(lev, xx)	fprintf xx
#else
#define errout stderr
# define debug(xx) 
# define debug2(xx)
# define debugr(xx) if (p->ddebug) fprintf(errout, xx ) 
# define debugr2(xx) if (p->ddebug) fprintf xx
# define debugrr(xx) if (callback_ddebug) fprintf(errout, xx ) 
# define debugrr2(xx) if (callback_ddebug) fprintf xx
# define debugrr2l(lev, xx) if (callback_ddebug >= lev) fprintf xx
#endif

/* ----------------------------------------------- */
/* Dealing with locating displays */

int callback_ddebug = 0;	/* Diagnostic global for get_displays() and get_a_display() */  
							/* and events */

#ifdef NT

#define sleep(secs) Sleep((secs) * 1000)

static BOOL CALLBACK MonitorEnumProc(
  HMONITOR hMonitor,  /* handle to display monitor */
  HDC hdcMonitor,     /* NULL, because EnumDisplayMonitors hdc is NULL */
  LPRECT lprcMonitor, /* Virtual screen coordinates of this monitor */
  LPARAM dwData       /* Context data */
) {
	disppath ***pdisps = (disppath ***)dwData;
	disppath **disps = *pdisps;
	MONITORINFOEX pmi;
	int ndisps = 0;
	
	debugrr2((errout, "MonitorEnumProc() called with hMonitor = 0x%x\n",hMonitor));

	/* Get some more information */
	pmi.cbSize = sizeof(MONITORINFOEX);
	if (GetMonitorInfo(hMonitor, (MONITORINFO *)&pmi) == 0) {
		debugrr("get_displays failed GetMonitorInfo - ignoring display\n");
		return TRUE;
	}

	/* See if it seems to be a pseudo-display */
	if (strncmp(pmi.szDevice, "\\\\.\\DISPLAYV", 12) == 0) {
		debugrr("Seems to be invisible pseudo-display - ignoring it\n");
		return TRUE;
	}

	/* Add the display to the list */
	if (disps == NULL) {
		if ((disps = (disppath **)calloc(sizeof(disppath *), 1 + 1)) == NULL) {
			debugrr("get_displays failed on malloc\n");
			return FALSE;
		}
	} else {
		/* Count current number on list */
		for (ndisps = 0; disps[ndisps] != NULL; ndisps++)
			;
		if ((disps = (disppath **)realloc(disps,
		                     sizeof(disppath *) * (ndisps + 2))) == NULL) {
			debugrr("get_displays failed on malloc\n");
			return FALSE;
		}
		disps[ndisps+1] = NULL;	/* End marker */
	}

	if ((disps[ndisps] = calloc(sizeof(disppath),1)) == NULL) {
		debugrr("get_displays failed on malloc\n");
		return FALSE;
	}

	if ((disps[ndisps]->name = strdup(pmi.szDevice)) == NULL) {
		debugrr("malloc failed\n");
		return FALSE;
	}
	disps[ndisps]->prim = (pmi.dwFlags & MONITORINFOF_PRIMARY) ? 1 : 0;

	disps[ndisps]->sx = lprcMonitor->left;
	disps[ndisps]->sy = lprcMonitor->top;
	disps[ndisps]->sw = lprcMonitor->right - lprcMonitor->left;
	disps[ndisps]->sh = lprcMonitor->bottom - lprcMonitor->top;

	disps[ndisps]->description = NULL;

	debugrr2((errout, "MonitorEnumProc() set initial monitor info: %d,%d %d,%d name '%s'\n",disps[ndisps]->sx,disps[ndisps]->sy,disps[ndisps]->sw,disps[ndisps]->sh, disps[ndisps]->name));

	*pdisps = disps;
	return TRUE;
}

/* Dynamically linked function support */

BOOL (WINAPI* pEnumDisplayDevices)(PVOID,DWORD,PVOID,DWORD) = NULL;

#if !defined(NTDDI_LONGHORN) || NTDDI_VERSION < NTDDI_LONGHORN

typedef enum {
	WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE,
	WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER
} WCS_PROFILE_MANAGEMENT_SCOPE;

BOOL (WINAPI* pWcsAssociateColorProfileWithDevice)(WCS_PROFILE_MANAGEMENT_SCOPE,PCWSTR,PCWSTR) = NULL;
BOOL (WINAPI* pWcsDisassociateColorProfileFromDevice)(WCS_PROFILE_MANAGEMENT_SCOPE,PCWSTR,PCWSTR) = NULL;

#endif  /* NTDDI_VERSION < NTDDI_LONGHORN */

/* See if we can get the wanted function calls */
/* return nz if OK */
static int setup_dyn_calls() {
	static int dyn_inited = 0;

	if (dyn_inited == 0) {
		dyn_inited = 1;

		/* EnumDisplayDevicesA was left out of lib32.lib on earlier SDK's ... */
		pEnumDisplayDevices = (BOOL (WINAPI*)(PVOID,DWORD,PVOID,DWORD)) GetProcAddress(LoadLibrary("USER32"), "EnumDisplayDevicesA");
		if (pEnumDisplayDevices == NULL)
			dyn_inited = 0;

		/* Vista calls */
#if !defined(NTDDI_LONGHORN) || NTDDI_VERSION < NTDDI_LONGHORN
		pWcsAssociateColorProfileWithDevice = (BOOL (WINAPI*)(WCS_PROFILE_MANAGEMENT_SCOPE,PCWSTR,PCWSTR)) GetProcAddress(LoadLibrary("mscms"), "WcsAssociateColorProfileWithDevice");
		pWcsDisassociateColorProfileFromDevice = (BOOL (WINAPI*)(WCS_PROFILE_MANAGEMENT_SCOPE,PCWSTR,PCWSTR)) GetProcAddress(LoadLibrary("mscms"), "WcsDisassociateColorProfileFromDevice");
		/* These are checked individually */
#endif  /* NTDDI_VERSION < NTDDI_LONGHORN */
	}

	return dyn_inited;
}

/* Simple up conversion from char string to wchar string */
/* Return NULL if malloc fails */
/* ~~~ Note, should probably replace this with mbstowcs() ???? */
static unsigned short *char2wchar(char *s) {
	unsigned char *cp;
	unsigned short *w, *wp;

	if ((w = malloc(sizeof(unsigned short) * (strlen(s) + 1))) == NULL)
		return w;

	for (cp = (unsigned char *)s, wp = w; ; cp++, wp++) {
		*wp = *cp;		/* Zero extend */
		if (*cp == 0)
			break;
	}

	return w;
}

#endif /* NT */


#if defined(UNIX_X11)
/* Hack to notice if the error handler has been triggered */
/* when a function doesn't return a value. */

int g_error_handler_triggered = 0;

/* A noop X11 error handler */
int null_error_handler(Display *disp, XErrorEvent *ev) {
	 g_error_handler_triggered = 1;
	return 0;
}
#endif	/* X11 */

/* Return pointer to list of disppath. Last will be NULL. */
/* Return NULL on failure. Call free_disppaths() to free up allocation */
disppath **get_displays() {
	disppath **disps = NULL;

#ifdef NT
	DISPLAY_DEVICE dd;
	char buf[200];
	int i, j;

	if (setup_dyn_calls() == 0) {
		debugrr("Dynamic linking to EnumDisplayDevices or Vista AssociateColorProfile failed\n");
		free_disppaths(disps);
		return NULL;
	}

	/* Create an initial list of monitors */
	/* (It might be better to call pEnumDisplayDevices(NULL, i ..) instead ??, */
	/* then we can use the StateFlags to distingish monitors not attached to the desktop etc.) */
	if (EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&disps) == 0) {
		debugrr("EnumDisplayMonitors failed\n");
		free_disppaths(disps);
		return NULL;
	}

	/* Now locate detailed information about displays */
	for (i = 0; ; i++) {
		if (disps == NULL || disps[i] == NULL)
			break;

		dd.cb = sizeof(dd);

		debugrr2((errout, "get_displays about to get monitor information for %d\n",i));
		/* Get monitor information */
		for (j = 0; ;j++) {
			if ((*pEnumDisplayDevices)(disps[i]->name, j, &dd, 0) == 0) {
				debugrr2((errout,"EnumDisplayDevices failed on '%s' Mon = %d\n",disps[i]->name,j));
				if (j == 0) {
					strcpy(disps[i]->monid, "");		/* We won't be able to set a profile */
				}
				break;
			}
			if (callback_ddebug) {
				fprintf(errout,"Mon %d, name '%s'\n",j,dd.DeviceName);
				fprintf(errout,"Mon %d, string '%s'\n",j,dd.DeviceString);
				fprintf(errout,"Mon %d, flags 0x%x\n",j,dd.StateFlags);
				fprintf(errout,"Mon %d, id '%s'\n",j,dd.DeviceID);
				fprintf(errout,"Mon %d, key '%s'\n",j,dd.DeviceKey);
			}
			if (j == 0) {
				strcpy(disps[i]->monid, dd.DeviceID);
			}
		}

		sprintf(buf,"%s, at %d, %d, width %d, height %d%s",disps[i]->name+4,
	        disps[i]->sx, disps[i]->sy, disps[i]->sw, disps[i]->sh,
	        disps[i]->prim ? " (Primary Display)" : "");

		if ((disps[i]->description = strdup(buf)) == NULL) {
			debugrr("get_displays failed on malloc\n");
			free_disppaths(disps);
			return NULL;
		}

		debugrr2((errout, "get_displays added description '%s' to display %d\n",disps[i]->description,i));

		/* Note that calling EnumDisplayDevices(NULL, j, ..) for the adapter can return other */
		/* information, such as the graphics card name, and additional state flags. */
		/* EnumDisplaySettings() can also be called to get information such as display depth etc. */
	}

#ifdef NEVER
	/* Explore adapter information */
	for (j = 0; ; j++) {
		/* Get adapater information */
		if ((*pEnumDisplayDevices)(NULL, j, &dd, 0) == 0)
			break;
		printf("Adapt %d, name '%s'\n",j,dd.DeviceName);
		printf("Adapt %d, string '%s'\n",j,dd.DeviceString);
		printf("Adapt %d, flags 0x%x\n",j,dd.StateFlags);
		printf("Adapt %d, id '%s'\n",j,dd.DeviceID);
		printf("Adapt %d, key '%s'\n",j,dd.DeviceKey);
	}
#endif /* NEVER */

#endif /* NT */

#ifdef __APPLE__
	/* Note :- some recent releases of OS X have a feature which */
	/* automatically adjusts the screen brigtness with ambient level. */
	/* We may have to find a way of disabling this during calibration and profiling. */
	/* See the "pset -g" command. */

	/*
		We could possibly use NSScreen instead of CG here,
		If we're using libui, then we have an NSApp, so
		this would be possible.
	 */

	int i;
	CGDisplayErr dstat;
	CGDisplayCount dcount;		/* Number of display IDs */
	CGDirectDisplayID *dids;	/* Array of display IDs */

	if ((dstat = CGGetActiveDisplayList(0, NULL, &dcount)) != kCGErrorSuccess || dcount < 1) {
		debugrr("CGGetActiveDisplayList #1 returned error\n");
		return NULL;
	}
	if ((dids = (CGDirectDisplayID *)malloc(dcount * sizeof(CGDirectDisplayID))) == NULL) {
		debugrr("malloc of CGDirectDisplayID's failed\n");
		return NULL;
	}
	if ((dstat = CGGetActiveDisplayList(dcount, dids, &dcount)) != kCGErrorSuccess) {
		debugrr("CGGetActiveDisplayList #2 returned error\n");
		free(dids);
		return NULL;
	}

	/* Found dcount displays */
	debugrr2((errout,"Found %d screens\n",dcount));

	/* Allocate our list */
	if ((disps = (disppath **)calloc(sizeof(disppath *), dcount + 1)) == NULL) {
		debugrr("get_displays failed on malloc\n");
		free(dids);
		return NULL;
	}
	for (i = 0; i < dcount; i++) {
		if ((disps[i] = calloc(sizeof(disppath), 1)) == NULL) {
			debugrr("get_displays failed on malloc\n");
			free_disppaths(disps);
			free(dids);
			return NULL;
		}
		disps[i]->ddid = dids[i];
	}

	/* Got displays, now figure out a description for each one */
	for (i = 0; i < dcount; i++) {
		CGRect dbound;				/* Bounding rectangle of chosen display */
		io_service_t dport;
		CFDictionaryRef ddr, pndr;
		CFIndex dcount;
		char *dp = NULL, desc[50];
		char buf[200];

		dbound = CGDisplayBounds(dids[i]);
		disps[i]->sx = dbound.origin.x;
		disps[i]->sy = dbound.origin.y;
		disps[i]->sw = dbound.size.width;
		disps[i]->sh = dbound.size.height;
			
		/* Try and get some information about the display */
		if ((dport = CGDisplayIOServicePort(dids[i])) == MACH_PORT_NULL) {
			debugrr("CGDisplayIOServicePort returned error\n");
			free_disppaths(disps);
			free(dids);
			return NULL;
		}

#ifdef NEVER
		{
			io_name_t name;
			if (IORegistryEntryGetName(dport, name) != KERN_SUCCESS) {
				debugrr("IORegistryEntryGetName returned error\n");
				free_disppaths(disps);
				free(dids);
				return NULL;
			}
			printf("Driver %d name = '%s'\n",i,name);
		}
#endif
		if ((ddr = IODisplayCreateInfoDictionary(dport, 0)) == NULL) {
			debugrr("IODisplayCreateInfoDictionary returned NULL\n");
			free_disppaths(disps);
			free(dids);
			return NULL;
		}
		if ((pndr = CFDictionaryGetValue(ddr, CFSTR(kDisplayProductName))) == NULL) {
			debugrr("CFDictionaryGetValue returned NULL\n");
			CFRelease(ddr);
			free_disppaths(disps);
			free(dids);
			return NULL;
		}
		if ((dcount = CFDictionaryGetCount(pndr)) > 0) {
			const void **keys;
			const void **values;
			int j;

			keys = (const void **)calloc(sizeof(void *), dcount);
			values = (const void **)calloc(sizeof(void *), dcount);
			if (keys == NULL || values == NULL) {
				if (keys != NULL)
					free(keys);
				if (values != NULL)
					free(values);
				debugrr("malloc failed\n");
				CFRelease(ddr);
				free_disppaths(disps);
				free(dids);
				return NULL;
			}
			CFDictionaryGetKeysAndValues(pndr, keys, values);
			for (j = 0; j < dcount; j++) {
				const char *k, *v;
				char kbuf[50], vbuf[50];
				k = CFStringGetCStringPtr(keys[j], kCFStringEncodingMacRoman);
				if (k == NULL) {
					if (CFStringGetCString(keys[j], kbuf, 50, kCFStringEncodingMacRoman))
						k = kbuf;
				}
				v = CFStringGetCStringPtr(values[j], kCFStringEncodingMacRoman);
				if (v == NULL) {
					if (CFStringGetCString(values[j], vbuf, 50, kCFStringEncodingMacRoman))
						v = vbuf;
				}
				/* We're only grabing the english description... */
				if (k != NULL && v != NULL && strcmp(k, "en_US") == 0) {
					strncpy(desc, v, 49);
					desc[49] = '\000';
					dp = desc;
				}
			}
			free(keys);
			free(values);
		}
		CFRelease(ddr);

		if (dp == NULL) {
			strcpy(desc, "(unknown)");
			dp = desc;
		}
		sprintf(buf,"%s, at %d, %d, width %d, height %d%s",dp,
	        disps[i]->sx, disps[i]->sy, disps[i]->sw, disps[i]->sh,
	        CGDisplayIsMain(dids[i]) ? " (Primary Display)" : "");

		if ((disps[i]->name = strdup(dp)) == NULL
		 || (disps[i]->description = strdup(buf)) == NULL) {
			debugrr("get_displays failed on malloc\n");
			free_disppaths(disps);
			free(dids);
			return NULL;
		}
	}

	free(dids);
#endif /* __APPLE__ */

#if defined(UNIX_X11)
	int i, j, k;
	int defsix = 0;		/* default screen index */
	int dcount;			/* Number of screens */
	char *dname;
	char dnbuf[100];
	int evb = 0, erb = 0;
	int majv, minv;			/* Version */
	Display *mydisplay;
	int ndisps = 0;
	XineramaScreenInfo *xai = NULL;
	char desc1[100], desc2[200];

	/* There seems to be no way of getting the available displays */
	/* on an X11 system. Attempting to open them in sequence */
	/* takes too long. We just rely on the user supplying the */
	/* right display. We can enumerate screens though. */

	/* Open the base display, and then enumerate all the screens */
	if ((dname = getenv("DISPLAY")) != NULL) {
		char *pp;
		strncpy(dnbuf,dname,99); dnbuf[99] = '\000';
		if ((pp = strrchr(dnbuf, ':')) != NULL) {
			if ((pp = strchr(pp, '.')) == NULL)
				strcat(dnbuf,".0");
			else  {
				if (pp[1] == '\000')
					strcat(dnbuf,"0");
				else {
					pp[1] = '0';
					pp[2] = '\000';
				}
			}
		}
	} else
		strcpy(dnbuf,":0.0");

	if ((mydisplay = XOpenDisplay(dnbuf)) == NULL) {
		debugrr2((errout, "failed to open display '%s'\n",dnbuf));
		return NULL;
	}

#if RANDR_MAJOR == 1 && RANDR_MINOR >= 2 && !defined(DISABLE_RANDR)
	/* Use Xrandr 1.2 if it's available, and if it's not disabled */
	if (getenv("ARGYLL_IGNORE_XRANDR1_2") == NULL
	 && XRRQueryExtension(mydisplay, &evb, &erb) != 0
	 && XRRQueryVersion(mydisplay, &majv, &minv)
	 && majv == 1 && minv >= 2) {

		if (XSetErrorHandler(null_error_handler) == 0) {
			debugrr("get_displays failed on XSetErrorHandler\n");
			XCloseDisplay(mydisplay);
			free_disppaths(disps);
			return NULL;
		}

		dcount = ScreenCount(mydisplay);

		/* Go through all the screens */
		for (i = 0; i < dcount; i++) {
			static void *xrr_found = NULL;	/* .so handle */
			static XRRScreenResources *(*_XRRGetScreenResourcesCurrent)
					                  (Display *dpy, Window window) = NULL;
			XRRScreenResources *scrnres;
			int jj;		/* Screen index */

			if (minv >= 3 && xrr_found == NULL) {
				if ((xrr_found = dlopen("libXrandr.so", RTLD_LAZY)) != NULL)
					_XRRGetScreenResourcesCurrent = dlsym(xrr_found, "XRRGetScreenResourcesCurrent");
			}

			if (minv >= 3 && _XRRGetScreenResourcesCurrent != NULL) { 
				scrnres = _XRRGetScreenResourcesCurrent(mydisplay, RootWindow(mydisplay,i));

			} else {
				scrnres = XRRGetScreenResources(mydisplay, RootWindow(mydisplay,i));
			}
			if (scrnres == NULL) {
				debugrr("XRRGetScreenResources failed\n");
				XCloseDisplay(mydisplay);
				free_disppaths(disps);
				return NULL;
			}

			/* Look at all the screens outputs */
			for (jj = j = 0; j < scrnres->noutput; j++) {
				XRROutputInfo *outi;
				XRRCrtcInfo *crtci;
	
				if ((outi = XRRGetOutputInfo(mydisplay, scrnres, scrnres->outputs[j])) == NULL) {
					debugrr("XRRGetOutputInfo failed\n");
					XRRFreeScreenResources(scrnres);
					XCloseDisplay(mydisplay);
					free_disppaths(disps);
					return NULL;
				}
	
				if (outi->connection == RR_Disconnected ||
					outi->crtc == None) {
					continue;
				}

				/* Check that the VideoLUT's are accessible */
				{
					XRRCrtcGamma *crtcgam;
			
					debugrr("Checking XRandR 1.2 VideoLUT access\n");
					if ((crtcgam = XRRGetCrtcGamma(mydisplay, outi->crtc)) == NULL
					 || crtcgam->size == 0) {
						fprintf(stderr,"XRandR 1.2 is faulty - falling back to older extensions\n");
						if (crtcgam != NULL)
							XRRFreeGamma(crtcgam);
						free_disppaths(disps);
						disps = NULL;
						j = scrnres->noutput;
						i = dcount;
						continue;				/* Abort XRandR 1.2 */
					}
				}
#ifdef NEVER
				{
					Atom *oprops;
					int noprop;

					/* Get a list of the properties of the output */
					oprops = XRRListOutputProperties(mydisplay, scrnres->outputs[j], &noprop);

					printf("num props = %d\n", noprop);
					for (k = 0; k < noprop; k++) {
						printf("%d: atom 0x%x, name = '%s'\n", k, oprops[k], XGetAtomName(mydisplay, oprops[k]));
					}
				}
#endif /* NEVER */

				if ((crtci = XRRGetCrtcInfo(mydisplay, scrnres, outi->crtc)) != NULL) {
					char *pp;

					/* Add the output to the list */
					if (disps == NULL) {
						if ((disps = (disppath **)calloc(sizeof(disppath *), 1 + 1)) == NULL) {
							debugrr("get_displays failed on malloc\n");
							XRRFreeCrtcInfo(crtci);
							XRRFreeScreenResources(scrnres);
							XCloseDisplay(mydisplay);
							return NULL;
						}
					} else {
						if ((disps = (disppath **)realloc(disps,
						                     sizeof(disppath *) * (ndisps + 2))) == NULL) {
							debugrr("get_displays failed on malloc\n");
							XRRFreeCrtcInfo(crtci);
							XRRFreeScreenResources(scrnres);
							XCloseDisplay(mydisplay);
							return NULL;
						}
						disps[ndisps+1] = NULL;	/* End marker */
					}
					/* ndisps is current display we're filling in */
					if ((disps[ndisps] = calloc(sizeof(disppath),1)) == NULL) {
						debugrr("get_displays failed on malloc\n");
						XRRFreeCrtcInfo(crtci);
						XRRFreeScreenResources(scrnres);
						XCloseDisplay(mydisplay);
						free_disppaths(disps);
						return NULL;
					}

					disps[ndisps]->screen = i;
					disps[ndisps]->uscreen = i;
					disps[ndisps]->rscreen = i;
					disps[ndisps]->sx = crtci->x;
					disps[ndisps]->sy = crtci->y;
					disps[ndisps]->sw = crtci->width;
					disps[ndisps]->sh = crtci->height;
					disps[ndisps]->crtc = outi->crtc;				/* XID of crtc */
					disps[ndisps]->output = scrnres->outputs[j];	/* XID of output */		

					sprintf(desc1,"Screen %d, Output %s",ndisps+1,outi->name);
					sprintf(desc2,"%s at %d, %d, width %d, height %d",desc1,
				        disps[ndisps]->sx, disps[ndisps]->sy, disps[ndisps]->sw, disps[ndisps]->sh);

					/* See if it is a clone */
					for (k = 0; k < ndisps; k++) {
						if (disps[k]->crtc == disps[ndisps]->crtc) {
							sprintf(desc1, "[ Clone of %d ]",k+1);
							strcat(desc2, desc1);
						}
					}
					if ((disps[ndisps]->description = strdup(desc2)) == NULL) {
						debugrr("get_displays failed on malloc\n");
						XRRFreeCrtcInfo(crtci);
						XRRFreeScreenResources(scrnres);
						XCloseDisplay(mydisplay);
						free_disppaths(disps);
						return NULL;
					}
	
					/* Form the display name */
					if ((pp = strrchr(dnbuf, ':')) != NULL) {
						if ((pp = strchr(pp, '.')) != NULL) {
							sprintf(pp,".%d",i);
						}
					}
					if ((disps[ndisps]->name = strdup(dnbuf)) == NULL) {
						debugrr("get_displays failed on malloc\n");
						XRRFreeCrtcInfo(crtci);
						XRRFreeScreenResources(scrnres);
						XCloseDisplay(mydisplay);
						free_disppaths(disps);
						return NULL;
					}
					debugrr2((errout, "Display %d name = '%s'\n",ndisps,disps[ndisps]->name));
	
					/* Create the X11 root atom of the default screen */
					/* that may contain the associated ICC profile */
					/* (The _%d variant will probably break with non-Xrandr */
					/* aware software if Xrandr is configured to have more than */
					/* a single virtual screen.) */
					if (jj == 0)
						strcpy(desc1, "_ICC_PROFILE");
					else
						sprintf(desc1, "_ICC_PROFILE_%d",jj);

					if ((disps[ndisps]->icc_atom = XInternAtom(mydisplay, desc1, False)) == None)
						error("Unable to intern atom '%s'",desc1);

					/* Create the atom of the output that may contain the associated ICC profile */
					if ((disps[ndisps]->icc_out_atom = XInternAtom(mydisplay, "_ICC_PROFILE", False)) == None)
						error("Unable to intern atom '%s'","_ICC_PROFILE");
		
					/* Grab the EDID from the output */
					{
						Atom edid_atom, ret_type;
						int ret_format;
						long ret_len = 0, ret_togo;
						unsigned char *atomv = NULL;
						int ii;
						char *keys[] = {		/* Possible keys that may be used */
							"EDID_DATA",
							"EDID",
							""
						};

						/* Try each key in turn */
						for (ii = 0; keys[ii][0] != '\000'; ii++) {
							/* Get the atom for the EDID data */
							if ((edid_atom = XInternAtom(mydisplay, keys[ii], True)) == None) {
								debugrr2((errout, "Unable to intern atom '%s'\n",keys[ii]));
								/* Try the next key */
							} else {

								/* Get the EDID_DATA */
								if (XRRGetOutputProperty(mydisplay, scrnres->outputs[j], edid_atom,
								            0, 0x7ffffff, False, False, XA_INTEGER, 
   		                            &ret_type, &ret_format, &ret_len, &ret_togo, &atomv) == Success
							            && (ret_len == 128 || ret_len == 256)) {
									if ((disps[ndisps]->edid = malloc(sizeof(unsigned char) * ret_len)) == NULL) {
										debugrr("get_displays failed on malloc\n");
										XRRFreeCrtcInfo(crtci);
										XRRFreeScreenResources(scrnres);
										XCloseDisplay(mydisplay);
										free_disppaths(disps);
										return NULL;
									}
									memmove(disps[ndisps]->edid, atomv, ret_len);
									disps[ndisps]->edid_len = ret_len;
									XFree(atomv);
									debugrr2((errout, "Got EDID for display\n"));
									break;
								}
								/* Try the next key */
							}
						}
						if (keys[ii][0] == '\000')
							debugrr2((errout, "Failed to get EDID for display\n"));
					}
		
					jj++;			/* Next enabled index */
					ndisps++;		/* Now it's number of displays */
					XRRFreeCrtcInfo(crtci);
				}
				XRRFreeOutputInfo(outi);
			}
	
			XRRFreeScreenResources(scrnres);
		}
		XSetErrorHandler(NULL);
		defsix = DefaultScreen(mydisplay);
	}
#endif /* randr >= V 1.2 */

	if (disps == NULL) {	/* Use Older style identification */
		debugrr("get_displays checking for Xinerama\n");

		if (XSetErrorHandler(null_error_handler) == 0) {
			debugrr("get_displays failed on XSetErrorHandler\n");
			XCloseDisplay(mydisplay);
			return NULL;
		}

		if (XineramaQueryExtension(mydisplay, &evb, &erb) != 0
		 && XineramaIsActive(mydisplay)) {

			xai = XineramaQueryScreens(mydisplay, &dcount);

			if (xai == NULL || dcount == 0) {
				debugrr("XineramaQueryScreens failed\n");
				XCloseDisplay(mydisplay);
				return NULL;
			}
			defsix = 0;
		} else {
			dcount = ScreenCount(mydisplay);
			defsix = DefaultScreen(mydisplay);
		}

		/* Allocate our list */
		if ((disps = (disppath **)calloc(sizeof(disppath *), dcount + 1)) == NULL) {
			debugrr("get_displays failed on malloc\n");
			XCloseDisplay(mydisplay);
			return NULL;
		}
		for (i = 0; i < dcount; i++) {
			if ((disps[i] = calloc(sizeof(disppath), 1)) == NULL) {
				debugrr("get_displays failed on malloc\n");
				free_disppaths(disps);
				XCloseDisplay(mydisplay);
				return NULL;
			}
		}

		/* Create a description for each screen */
		for (i = 0; i < dcount; i++) {
		    XF86VidModeMonitor monitor;
			int evb = 0, erb = 0;
			char *pp;

			/* Form the display name */
			if ((pp = strrchr(dnbuf, ':')) != NULL) {
				if ((pp = strchr(pp, '.')) != NULL) {
					sprintf(pp,".%d",i);
				}
			}
			if ((disps[i]->name = strdup(dnbuf)) == NULL) {
				debugrr("get_displays failed on malloc\n");
				free_disppaths(disps);
				XCloseDisplay(mydisplay);
				return NULL;
			}
	
			debugrr2((errout, "Display %d name = '%s'\n",i,disps[i]->name));
			if (xai != NULL) {					/* Xinerama */
				disps[i]->screen = 0;			/* We are asuming Xinerame creates a single virtual screen */
				disps[i]->uscreen = i;			/* We are assuming xinerama lists screens in the same order */
				disps[i]->rscreen = i;
				disps[i]->sx = xai[i].x_org;
				disps[i]->sy = xai[i].y_org;
				disps[i]->sw = xai[i].width;
				disps[i]->sh = xai[i].height;
			} else {
				disps[i]->screen = i;
				disps[i]->uscreen = i;
				disps[i]->rscreen = i;
				disps[i]->sx = 0;			/* Must be 0 */
				disps[i]->sy = 0;
				disps[i]->sw = DisplayWidth(mydisplay, disps[i]->screen);
				disps[i]->sh = DisplayHeight(mydisplay, disps[i]->screen);
			}

			/* Create the X11 root atom of the default screen */
			/* that may contain the associated ICC profile */
			if (disps[i]->uscreen == 0)
				strcpy(desc1, "_ICC_PROFILE");
			else
				sprintf(desc1, "_ICC_PROFILE_%d",disps[i]->uscreen);

			if ((disps[i]->icc_atom = XInternAtom(mydisplay, desc1, False)) == None)
				error("Unable to intern atom '%s'",desc1);

			/* See if we can locate the EDID of the monitor for this screen */
			for (j = 0; j < 2; j++) { 
				char edid_name[50];
				Atom edid_atom, ret_type;
				int ret_format = 8;
				long ret_len, ret_togo;
				unsigned char *atomv = NULL;

				if (disps[i]->uscreen == 0) {
					if (j == 0)
						strcpy(edid_name,"XFree86_DDC_EDID1_RAWDATA");
					else
						strcpy(edid_name,"XFree86_DDC_EDID2_RAWDATA");
				} else {
					if (j == 0)
						sprintf(edid_name,"XFree86_DDC_EDID1_RAWDATA_%d",disps[i]->uscreen);
					else
						sprintf(edid_name,"XFree86_DDC_EDID2_RAWDATA_%d",disps[i]->uscreen);
				}

				if ((edid_atom = XInternAtom(mydisplay, edid_name, True)) == None)
					continue;
				if (XGetWindowProperty(mydisplay, RootWindow(mydisplay, disps[i]->uscreen), edid_atom,
				            0, 0x7ffffff, False, XA_INTEGER, 
				            &ret_type, &ret_format, &ret_len, &ret_togo, &atomv) == Success
				            && (ret_len == 128 || ret_len == 256)) {
					if ((disps[i]->edid = malloc(sizeof(unsigned char) * ret_len)) == NULL) {
						debugrr("get_displays failed on malloc\n");
						free_disppaths(disps);
						XCloseDisplay(mydisplay);
						return NULL;
					}
					memmove(disps[i]->edid, atomv, ret_len);
					disps[i]->edid_len = ret_len;
					XFree(atomv);
					debugrr2((errout, "Got EDID for display\n"));
					break;
				} else {
					debugrr2((errout, "Failed to get EDID for display\n"));
				}
			}

			if (XF86VidModeQueryExtension(mydisplay, &evb, &erb) != 0) {
				/* Some propietary multi-screen drivers (ie. TwinView & MergeFB) */
				/* don't implement the XVidMode extension properly. */
				monitor.model = NULL;
				if (XF86VidModeGetMonitor(mydisplay, disps[i]->uscreen, &monitor) != 0
				 && monitor.model != NULL && monitor.model[0] != '\000')
					sprintf(desc1, "%s",monitor.model);
				else
					sprintf(desc1,"Screen %d",i+1);
			} else
				sprintf(desc1,"Screen %d",i+1);

			sprintf(desc2,"%s at %d, %d, width %d, height %d",desc1,
		        disps[i]->sx, disps[i]->sy, disps[i]->sw, disps[i]->sh);
			if ((disps[i]->description = strdup(desc2)) == NULL) {
				debugrr("get_displays failed on malloc\n");
				free_disppaths(disps);
				XCloseDisplay(mydisplay);
				return NULL;
			}
		}
		XSetErrorHandler(NULL);
	}

	/* Put the screen given by the display name at the top */
	{
		disppath *tdispp;
		tdispp = disps[defsix];
		disps[defsix] = disps[0];
		disps[0] = tdispp;
	}

	if (xai != NULL)
		XFree(xai);

	XCloseDisplay(mydisplay);

#endif /* UNIX X11 */

	return disps;
}

/* Free a whole list of display paths */
void free_disppaths(disppath **disps) {
	if (disps != NULL) {
		int i;
		for (i = 0; ; i++) {
			if (disps[i] == NULL)
				break;

			if (disps[i]->name != NULL)
				free(disps[i]->name);
			if (disps[i]->description != NULL)
				free(disps[i]->description);
#if defined(UNIX_X11)
			if (disps[i]->edid != NULL)
				free(disps[i]->edid);
#endif
			free(disps[i]);
		}
		free(disps);
	}
}

/* Delete a single display from the list of display paths */
void del_disppath(disppath **disps, int ix) {
	if (disps != NULL) {
		int i, j, k;
		for (i = 0; ; i++) {
			if (disps[i] == NULL)
				break;

			if (i == ix) {	/* One to delete */
				if (disps[i]->name != NULL)
					free(disps[i]->name);
				if (disps[i]->description != NULL)
					free(disps[i]->description);
#if defined(UNIX_X11)
				if (disps[i]->edid != NULL)
					free(disps[i]->edid);
#endif
				free(disps[i]);

				/* Shuffle the rest down */
				for (j = i, k = i + 1; ;j++, k++) {
					disps[j] = disps[k];
					if (disps[k] == NULL)
						break;
				}
				return;
			}
		}
	}
}

/* ----------------------------------------------- */
/* Deal with selecting a display */

/* Return the given display given its index 0..n-1 */
disppath *get_a_display(int ix) {
	disppath **paths, *rv = NULL;
	int i;

	if ((paths = get_displays()) == NULL)
		return NULL;

	for (i = 0; ;i++) {
		if (paths[i] == NULL) {
			free_disppaths(paths);
			return NULL;
		}
		if (i == ix)
			break;
	}
	if ((rv = malloc(sizeof(disppath))) == NULL) {
		debugrr("get_a_display failed malloc\n");
		free_disppaths(paths);
		return NULL;
	}
	*rv = *paths[i];		/* Structure copy */
	if ((rv->name = strdup(paths[i]->name)) == NULL) {
		debugrr("get_displays failed on malloc\n");
		free(rv->description);
		free(rv);
		free_disppaths(paths);
		return NULL;
	}
	if ((rv->description = strdup(paths[i]->description)) == NULL) {
		debugrr("get_displays failed on malloc\n");
		free(rv);
		free_disppaths(paths);
		return NULL;
	}
#if defined(UNIX_X11)
	if (paths[i]->edid != NULL) {
		if ((rv->edid = malloc(sizeof(unsigned char) * paths[i]->edid_len)) == NULL) {
			debugrr("get_displays failed on malloc\n");
			free(rv);
			free_disppaths(paths);
			return NULL;
		}
		rv->edid_len = paths[i]->edid_len;
		memmove(rv->edid, paths[i]->edid, rv->edid_len );
	}
#endif
	free_disppaths(paths);
	return rv;
}

void free_a_disppath(disppath *path) {
	if (path != NULL) {
		if (path->name != NULL)
			free(path->name);
		if (path->description != NULL)
			free(path->description);
#if defined(UNIX_X11)
		if (path->edid != NULL)
			free(path->edid);
#endif
		free(path);
	}
}

/* ----------------------------------------------- */

/* For VideoLUT/RAMDAC use, we assume that the number of entries in the RAMDAC */
/* meshes perfectly with the display raster depth, so that we can */
/* figure out how to apportion device values. We fail if they don't */
/* seem to mesh. */

/* !!! Would be nice to add an error message return to dispwin and */
/* !!! pass errors back to it so that the detail can be reported */
/* !!! to the user. */

/* Get RAMDAC values. ->del() when finished. */
/* Return NULL if not possible */
static ramdac *dispwin_get_ramdac(dispwin *p) {
	ramdac *r = NULL;
	int i, j;

#ifdef NT
	WORD vals[3][256];		/* 256 x 16 bit elements (Quantize) */

	debugr("dispwin_get_ramdac called\n");

#ifdef NEVER	/* Doesn't seem to return correct information on win2K systems */
	if ((GetDeviceCaps(p->hdc, COLORMGMTCAPS) & CM_GAMMA_RAMP) == 0) {
		debugr("dispwin_get_ramdac failed on GetDeviceCaps(CM_GAMMA_RAMP)\n");
		return NULL;
	}
#endif

	/* Allocate a ramdac */
	if ((r = (ramdac *)calloc(sizeof(ramdac), 1)) == NULL) {
		debugr("dispwin_get_ramdac failed on malloc()\n");
		return NULL;
	}
	r->pdepth = p->pdepth;
	r->nent = (1 << p->pdepth);
	r->clone =  dispwin_clone_ramdac;
	r->setlin = dispwin_setlin_ramdac;
	r->del =    dispwin_del_ramdac;

	for (j = 0; j < 3; j++) {

		if ((r->v[j] = (double *)calloc(sizeof(double), r->nent)) == NULL) {
			for (j--; j >= 0; j--)
				free(r->v[j]);
			free(r);
			debugr("dispwin_get_ramdac failed on malloc()\n");
			return NULL;
		}
	}

	/* GetDeviceGammaRamp() is hard coded for 3 x 256 entries (Quantize) */
	if (r->nent != 256) {
		free(r);
		debugr2((errout,"GetDeviceGammaRamp() is hard coded for nent == 256, and we've got nent = %d!\n",r->nent));
		return NULL;
	}

	if (GetDeviceGammaRamp(p->hdc, vals) == 0) {
		free(r);
		debugr("dispwin_get_ramdac failed on GetDeviceGammaRamp()\n");
		return NULL;
	}
	for (j = 0; j < 3; j++) {
		for (i = 0; i < r->nent; i++) {
			r->v[j][i] = vals[j][i]/65535.0;
		}
	}
#endif	/* NT */

#ifdef __APPLE__
	unsigned int nent;
	CGGammaValue vals[3][16385];

	debugr("dispwin_get_ramdac called\n");

	if (CGGetDisplayTransferByTable(p->ddid, 163845, vals[0], vals[1], vals[2], &nent) != 0) {
		debugr("CGGetDisplayTransferByTable failed\n");
		return NULL;
	}

	if (nent == 16385) {	/* oops - we didn't provide enought space! */
		debugr("CGGetDisplayTransferByTable has more entries than we can handle\n");
		return NULL;
	}

	if (nent != (1 << p->pdepth)) {
		debugr2((errout,"CGGetDisplayTransferByTable number of entries %d mismatches screen depth %d\n",nent,p->pdepth));
		return NULL;
	}

	/* Allocate a ramdac */
	if ((r = (ramdac *)calloc(sizeof(ramdac), 1)) == NULL) {
		debugr("dispwin_get_ramdac failed on malloc()\n");
		return NULL;
	}

	r->pdepth = p->pdepth;
	r->nent = (1 << p->pdepth);
	r->clone =  dispwin_clone_ramdac;
	r->setlin = dispwin_setlin_ramdac;
	r->del =    dispwin_del_ramdac;
	for (j = 0; j < 3; j++) {

		if ((r->v[j] = (double *)calloc(sizeof(double), r->nent)) == NULL) {
			for (j--; j >= 0; j--)
				free(r->v[j]);
			free(r);
			debugr("dispwin_get_ramdac failed on malloc()\n");
			return NULL;
		}
	}

	for (j = 0; j < 3; j++) {
		for (i = 0; i < r->nent; i++) {
			r->v[j][i] = vals[j][i];
		}
	}
#endif /* __APPLE__ */

#if defined(UNIX_X11)
	unsigned short vals[3][16384];
	int nent = 0;
	int evb = 0, erb = 0;

	debugr("dispwin_get_ramdac called\n");

#if RANDR_MAJOR == 1 && RANDR_MINOR >= 2 && !defined(DISABLE_RANDR)
	if (p->crtc != 0) {		/* Using Xrandr 1.2 */
		XRRCrtcGamma *crtcgam;
		int nz = 0;

		debugr("Getting gamma using Randr 1.2\n");

		if ((crtcgam = XRRGetCrtcGamma(p->mydisplay, p->crtc)) == NULL) {
			debugr("XRRGetCrtcGamma failed\n");
			return NULL;
		}

		nent = crtcgam->size;

		if (nent > 16384) {
			debugr("XRRGetCrtcGammaSize  has more entries than we can handle\n");
			return NULL;
		}

		if (nent != (1 << p->pdepth)) {
			debugr2((errout,"XRRGetCrtcGammaSize number of entries %d mismatches screen depth %d bits\n",nent,(1 << p->pdepth)));
			return NULL;
		}

		/* Check for XRandR 1.2 startup bug */
		for (i = 0; i < nent; i++) {
			vals[0][i] = crtcgam->red[i];
			vals[1][i] = crtcgam->green[i];
			vals[2][i] = crtcgam->blue[i];
			nz = vals[0][i] | vals[1][i] | vals[2][i];
		}

		/* Compensate for XRandR 1.2 startup bug */
		if (nz == 0) {
			debugr("Detected XRandR 1.2 bug ? Assuming linear ramp!\n");
			for (i = 0; i < nent; i++) {
				for (j = 0; j < 3; j++)
					vals[j][i] = (int)(65535.0 * i/(nent-1.0) + 0.5);
			}
		}

		XRRFreeGamma(crtcgam);

	} else
#endif /* randr >= V 1.2 */
	{

		if (XF86VidModeQueryExtension(p->mydisplay, &evb, &erb) == 0) {
			debugr("XF86VidModeQueryExtension failed\n");
			return NULL;
		}
		/* Some propietary multi-screen drivers (ie. TwinView & MergedFB) */
		/* don't implement the XVidMode extenstion properly. */
		if (XSetErrorHandler(null_error_handler) == 0) {
			debugr("get_displays failed on XSetErrorHandler\n");
			return NULL;
		}
		nent = -1;
		if (XF86VidModeGetGammaRampSize(p->mydisplay, p->myrscreen, &nent) == 0
		 || nent == -1) {
			XSetErrorHandler(NULL);
			debugr("XF86VidModeGetGammaRampSize failed\n");
			return NULL;
		}
		XSetErrorHandler(NULL);		/* Restore handler */
		if (nent == 0) {
			debugr("XF86VidModeGetGammaRampSize returned 0 size\n");
			return NULL;
		}

		if (nent > 16384) {
			debugr("XF86VidModeGetGammaRampSize has more entries than we can handle\n");
			return NULL;
		}

		if (XF86VidModeGetGammaRamp(p->mydisplay, p->myrscreen, nent,  vals[0], vals[1], vals[2]) == 0) {
			debugr("XF86VidModeGetGammaRamp failed\n");
			return NULL;
		}

		if (nent != (1 << p->pdepth)) {
			debugr2((errout,"CGGetDisplayTransferByTable number of entries %d mismatches screen depth %d bits\n",nent,(1 << p->pdepth)));
			return NULL;
		}
	}

	/* Allocate a ramdac */
	if ((r = (ramdac *)calloc(sizeof(ramdac), 1)) == NULL) {
		debugr("dispwin_get_ramdac failed on malloc()\n");
		return NULL;
	}

	r->pdepth = p->pdepth;
	r->nent = (1 << p->pdepth);
	r->clone = dispwin_clone_ramdac;
	r->setlin = dispwin_setlin_ramdac;
	r->del = dispwin_del_ramdac;
	for (j = 0; j < 3; j++) {

		if ((r->v[j] = (double *)calloc(sizeof(double), r->nent)) == NULL) {
			for (j--; j >= 0; j--)
				free(r->v[j]);
			free(r);
			debugr("dispwin_get_ramdac failed on malloc()\n");
			return NULL;
		}
	}

	for (i = 0; i < r->nent; i++) {
		for (j = 0; j < 3; j++) {
			r->v[j][i] = vals[j][i]/65535.0;
		}
	}
#endif	/* UNXI X11 */
	debugr("dispwin_get_ramdac returning OK\n");
	return r;
}

#ifdef __APPLE__
	/* Various support functions */

#if __MAC_OS_X_VERSION_MAX_ALLOWED < 1060

/* Given a location, return a string for it's path */
static char *plocpath(CMProfileLocation *ploc) {

	if (ploc->locType == cmFileBasedProfile) {
		FSRef newRef;
  		UInt8 path[256] = "";

		/* Note that there is no non-deprecated equivalent to this. */
		/* Apple need to remove the cmFileBasedProfile type from the */
		/* CMProfileLocation to do away with it. */
		if (FSpMakeFSRef(&ploc->u.fileLoc.spec, &newRef) == noErr) {
			OSStatus stus;
			if ((stus = FSRefMakePath(&newRef, path, 256)) == 0 || stus == fnfErr)
				return strdup((char *)path);
			return NULL;
		} 
	} else if (ploc->locType == cmPathBasedProfile) {
		return strdup(ploc->u.pathLoc.path);
	}
	return NULL;
}

/* Ugh! ColorSync doesn't take care of endian issues !! */
static void cs_w32(unsigned long *p, unsigned long val) {
	((char *)p)[0] = (char)(val >> 24);
	((char *)p)[1] = (char)(val >> 16);
	((char *)p)[2] = (char)(val >> 8);
	((char *)p)[3] = (char)(val);
}

static void cs_w16(unsigned short *p, unsigned short val) {
	((char *)p)[0] = (char)(val >> 8);
	((char *)p)[1] = (char)(val);
}

#endif /* < 10.6 */

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060

/* There doesn't seem to be any means of determining the locations */
/* of profiles using current OS X API's, so we simply hard code them. */
/* This makes the older code easier too. */

#define COLORSYNC_DIR_NETWORK	"/Network/Library/ColorSync/Profiles/"
#define COLORSYNC_DIR_SYSTEM	"/System/Library/ColorSync/Profiles/"
#define COLORSYNC_DIR_LOCAL	    "/Library/ColorSync/Profiles/"
#define COLORSYNC_DIR_USER	    "/Library/ColorSync/Profiles/"

/* Given a profile name and a scope, return the path to the */
/* installed profile. free the returned string when done. */
/* Returns NULL on error */
static char *iprof_path(p_scope scope, char *fname) {
	char *home = "", *dirname, *basename, *rv = NULL;
	int tlen = 0;

	/* Locate the base filename in the fname */
	for (basename = fname + strlen(fname);  ; basename--) {
		if (basename <= fname || basename[-1] == '/')
			break;
	}

	/* NSFileManager's URLForDirectory: etc. doesn't have ColorSync, */
	/* so we have no choice but to hard code the paths */
	if (scope == p_scope_network)
		dirname = COLORSYNC_DIR_NETWORK; 
	else if (scope == p_scope_system)
		dirname = COLORSYNC_DIR_SYSTEM;
	else if (scope == p_scope_local)
		dirname = COLORSYNC_DIR_LOCAL;
	else {
		dirname = COLORSYNC_DIR_USER;
		if ((home = getenv("HOME")) == NULL){ 
			return NULL;
		}
	}

	tlen = strlen(home) + strlen(dirname) + strlen(basename) + 1;
	if ((rv = malloc(tlen)) == NULL) {
		return NULL;
	}

	strcpy(rv, home);
	strcat(rv, dirname);
	strcat(rv, basename);

	return rv;
}

/* Callback */
typedef struct {
	CFUUIDRef dispuuid;		/* UUID to match */
	CFStringRef id;			/* ProfileId */
	CFURLRef url;			/* URL to return */
} diter_cntx_t;

static bool diter_callback(CFDictionaryRef dict, void *cntx) {
	diter_cntx_t *cx = (diter_cntx_t *)cntx;
	CFStringRef str;
	CFUUIDRef uuid;
	CFBooleanRef iscur;

	if ((str = CFDictionaryGetValue(dict, kColorSyncDeviceClass)) == NULL) {
		debugrr("Failed to get kColorSyncDeviceClass\n");
		return true;
	}
	if (!CFEqual(str, kColorSyncDisplayDeviceClass)) {
		return true;
	}
	if ((uuid = CFDictionaryGetValue(dict, kColorSyncDeviceID)) == NULL) {
		debugrr("Failed to get kColorSyncDeviceID\n");
		return true;
	}
	if (!CFEqual(uuid, cx->dispuuid)) {
		return true;
	}
	if ((iscur = CFDictionaryGetValue(dict, kColorSyncDeviceProfileIsCurrent)) == NULL) {
		debugrr("Failed to get kColorSyncDeviceProfileIsCurrent\n");
		return true;
	}
	if (!CFBooleanGetValue(iscur)) {
		return true;
	}

	/* get the URL */
	if ((cx->id = CFDictionaryGetValue(dict, kColorSyncDeviceProfileID)) == NULL) {
		debugrr("Failed to get current profile ID\n");
		return true;
	}
	if ((cx->url = CFDictionaryGetValue(dict, kColorSyncDeviceProfileURL)) == NULL) {
		debugrr("Failed to get current profile URL\n");
		return true;
	}
	CFRetain(cx->id);
	CFRetain(cx->url);

	return false;
}

/* Return the url to the given displays current profile. */
/* Return NULL on error. CFRelease when done. */
/* Optionally return the ProfileID string */
static CFURLRef cur_profile_url(CFStringRef *idp, dispwin *p) {
	diter_cntx_t cx;

	if ((cx.dispuuid = CGDisplayCreateUUIDFromDisplayID(p->ddid)) == NULL) {
		debugr2((errout,"CGDisplayCreateUUIDFromDisplayID() failed\n"));
		return NULL;
	}
	cx.id = NULL;
	cx.url = NULL;

	ColorSyncIterateDeviceProfiles(diter_callback, (void *)&cx);

	CFRelease(cx.dispuuid);

	if (idp != NULL)
		*idp = cx.id;
	else
		CFRelease(cx.id);
	return cx.url;
}

/* Convert a URL into a local POSIX path string */
/* Return NULL on error. Free returned string when done. */
static char *url_to_path(CFURLRef url) {
	CFStringRef urlstr;
	CFIndex bufSize;
	char *dpath = NULL;			/* return value */

	urlstr = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
	bufSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(urlstr),
	                                              kCFStringEncodingUTF8) + 1;
	if ((dpath = malloc(bufSize)) == NULL) {
		debugrr("url_to_path: malloc failed\n");
		CFRelease(urlstr);
		return NULL;
	}
	if (!CFStringGetCString(urlstr, dpath, bufSize, kCFStringEncodingUTF8)) {
		debugrr("url_to_path: CFStringGetCString failed\n");
		CFRelease(urlstr);
		return NULL;
	}
	CFRelease(urlstr);

	return dpath;
}

/* Return the local POSIX path to the given displays current profile */
/* Return NULL on error. Free returned string when done. */
static char *cur_profile(dispwin *p) {
	CFURLRef url;
	char *dpath = NULL;			/* return value */

	if ((url = cur_profile_url(NULL, p)) == NULL) {
		debugr2((errout,"cur_profile failed to find current profile\n"));
		return NULL;
	}

	dpath = url_to_path(url);

	CFRelease(url);

	return dpath;
}

#endif /* >= 10.6 */

/* Return a CMProfileRef/ColorSyncProfileRef for the */
/* displays profile. Return NULL on error */
static void *cur_colorsync_ref(dispwin *p) {
	void *cspr = NULL;
 
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	CFURLRef url;
	ColorSyncProfileRef ref;
	CFErrorRef ev;

	if ((url = cur_profile_url(NULL, p)) == NULL) {
		debugr2((errout,"cur_colorsync_ref got NULL URL\n"));
		return NULL;
	}

	if ((ref = ColorSyncProfileCreateWithURL(url, &ev)) == NULL) {
		debugr2((errout,"ColorSyncProfileCreateWithURL failed\n"));
		return NULL;
	}
	CFRelease(url);

	cspr = (void *)ref;

#else	/* 10.5 and prior */
	CMError ev;
	CMDeviceProfileID curID;	/* Current Device Default profile ID */
	CMProfileLocation cploc;	/* Current profile location */
	CMProfileRef prof;

	/* Get the default ID for the display */
	if ((ev = CMGetDeviceDefaultProfileID(cmDisplayDeviceClass, (CMDeviceID)p->ddid, &curID)) != noErr) {
		debugr2((errout,"CMGetDeviceDefaultProfileID() failed with error %d\n",ev));
		return NULL;
	}

	/* Get the displays profile */
	if ((ev = CMGetDeviceProfile(cmDisplayDeviceClass, (CMDeviceID)p->ddid, curID, &cploc)) != noErr) {
		debugr2((errout,"CMGetDeviceDefaultProfileID() failed with error %d\n",ev));
		return NULL;
	}

	if ((ev = CMOpenProfile(&prof, &cploc)) != noErr) {
		debugr2((errout,"CMOpenProfile() failed with error %d\n",ev));
		return NULL;
	}
	cspr = (void *)prof;
	
#endif

	return cspr;
}

#endif /* __APPLE__ */

/* Set the RAMDAC values. */
/* Return nz if not possible */
/* Return 2 for OS X when the current profile is a system profile */
static int dispwin_set_ramdac(dispwin *p, ramdac *r, int persist) {
	int i, j;

#ifdef NT
	WORD vals[3][256];		/* 16 bit elements */

	debugr("dispwin_set_ramdac called\n");

#ifdef NEVER	/* Doesn't seem to return correct information on win2K systems */
	if ((GetDeviceCaps(p->hdc, COLORMGMTCAPS) & CM_GAMMA_RAMP) == 0) {
		debugr("dispwin_set_ramdac failed on GetDeviceCaps(CM_GAMMA_RAMP)\n");
		return 1;
	}
#endif

	for (j = 0; j < 3; j++) {
		for (i = 0; i < r->nent; i++) {
			double vv = r->v[j][i];
			if (vv < 0.0)
				vv = 0.0;
			else if (vv > 1.0)
				vv = 1.0;
			vals[j][i] = (int)(65535.0 * vv + 0.5);
		}
	}

	if (SetDeviceGammaRamp(p->hdc, vals) == 0) {
		debugr2((errout,"dispwin_set_ramdac failed on SetDeviceGammaRamp() with error %d\n",GetLastError()));
		return 1;
	}
	GdiFlush();
#endif	/* NT */

#ifdef __APPLE__
	{		/* Transient first */
		CGGammaValue vals[3][16384];
	
		debugr("dispwin_set_ramdac called\n");

		for (j = 0; j < 3; j++) {
			for (i = 0; i < r->nent; i++) {
				double vv = r->v[j][i];
				if (vv < 0.0)
					vv = 0.0;
				else if (vv > 1.0)
					vv = 1.0;
				vals[j][i] = vv;
			}
		}

		if (CGSetDisplayTransferByTable(p->ddid, r->nent, vals[0], vals[1], vals[2]) != 0) {
			debugr("CGSetDisplayTransferByTable failed\n");
			return 1;
		}

	}

	/* In theory IOFBSetGamma() might work - but maybe it needs root, and won't */
	/* sync with OS X's view of what's loaded. */

	/* By default the OSX RAMDAC access is transient, lasting only as long */
	/* as the process setting it. To set a temporary but persistent beyond this process */
	/* calibration, we fake up a profile and install it in such a way that it will disappear, */
	/* restoring the previous display profile whenever the current ColorSync display profile */
	/* is restored to the screen. NOTE that this trick will fail if it is not possible */
	/* to rename the currently selected profile file, ie. because it is a system profile. */
	/* [ Would a workaround be to use a link, or copy of the system file ? ] */
	if (persist)
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	if (persist) {					/* Persistent */
		int rv = 0;
		CFStringRef id;
		CFURLRef url;
		char *tpath;				/* Temporary profiles/original profiles path */
		char *ppath;				/* Current/renamed profiles path */
		icmFile *rd_fp, *wr_fp;
		icc *icco;
		CFUUIDRef dispuuid;
		CFStringRef keys[1];
		CFURLRef values[1];
		CFDictionaryRef dict;

		debugr("Set_ramdac persist\n");

		/* Get the current installed profile */
		if ((url = cur_profile_url(&id, p)) == NULL) {
			debugr2((errout,"cur_profile_url failed for current profile\n"));
			return 1;
		}
		if ((tpath = url_to_path(url)) == NULL) {
			debugr2((errout,"url_to_path failed for current profile\n"));
			CFRelease(id);
			CFRelease(url);
			return 1;
		}

		debugr2((errout, "Current profile path = '%s'\n",tpath));

		/* Create a patched version with our calibration: */
		/* (Hmm. I think icclib will cope with V4 OK) */

		/* Open up the profile for reading */
		if ((rd_fp = new_icmFileStd_name(tpath,"r")) == NULL) {
			debugr2((errout,"Failed to open profile '%s'\n",tpath));
			free(tpath);
			CFRelease(id);
			CFRelease(url);
			return 1;
		}
	
		if ((icco = new_icc()) == NULL) {
			debugr2((errout,"Creation of ICC object failed\n"));
			rd_fp->del(rd_fp);
			free(tpath);
			CFRelease(id);
			CFRelease(url);
			return 1;
		}
	
		/* Read header etc. */
		if ((rv = icco->read(icco,rd_fp,0)) != 0) {
			debugr2((errout,"%d, %s",rv,icco->err));
			icco->del(icco);
			rd_fp->del(rd_fp);
			free(tpath);
			CFRelease(id);
			CFRelease(url);
			return 1;
		}
		/* Read every tag */
		if (icco->read_all_tags(icco) != 0) {
			debugr2((errout,"Unable to read all tags: %d, %s",icco->errc,icco->err));
			icco->del(icco);
			rd_fp->del(rd_fp);
			free(tpath);
			CFRelease(id);
			CFRelease(url);
			return 1;
		}
	
		rd_fp->del(rd_fp); rd_fp = NULL;
	
		/* Replace the description */
		{
			icmTextDescription *wo;
			char *dst = "Dispwin Temp";

			if (icco->find_tag(icco, icSigProfileDescriptionTag) == 0) {
				if (icco->delete_tag(icco, icSigProfileDescriptionTag) != 0) {
					debugr2((errout,"Unable to delete Description tag: %d, %s",icco->errc,icco->err));
					icco->del(icco);
					free(tpath);
					CFRelease(id);
					CFRelease(url);
					return 1;
				}
			}
	
			if ((wo = (icmTextDescription *)icco->add_tag(
			           icco, icSigProfileDescriptionTag,	icSigTextDescriptionType)) == NULL) { 
				debugr2((errout,"Unable to add Description tag: %d, %s",icco->errc,icco->err));
				icco->del(icco);
				free(tpath);
				CFRelease(id);
				CFRelease(url);
				return 1;
			}
	
			wo->size = strlen(dst)+1; 	/* Allocated and used size of desc, inc null */
			wo->allocate((icmBase *)wo);/* Allocate space */
			strcpy(wo->desc, dst);		/* Copy the string in */
		}
		/* Replace the vcgt */
		{
			int c,i;
			icmVideoCardGamma *wo;

			if (icco->find_tag(icco, icSigVideoCardGammaTag) == 0) {
				if (icco->delete_tag(icco, icSigVideoCardGammaTag) != 0) {
					debugr2((errout,"Unable to delete VideoCardGamma tag: %d, %s",icco->errc,icco->err));
					icco->del(icco);
					free(tpath);
					CFRelease(id);
					CFRelease(url);
					return 1;
				}
			}
	
			if ((wo = (icmVideoCardGamma *)icco->add_tag(icco, icSigVideoCardGammaTag,
			                                        icSigVideoCardGammaType)) == NULL) {
				debugr2((errout,"Unable to add VideoCardGamma tag: %d, %s",icco->errc,icco->err));
				icco->del(icco);
				free(tpath);
				CFRelease(id);
				CFRelease(url);
				return 1;
			}
	
			wo->tagType = icmVideoCardGammaTableType;
			wo->u.table.channels = 3;			/* rgb */
			wo->u.table.entryCount = r->nent;	/* number of calibration entries */
			wo->u.table.entrySize = 2;			/* 16 bits */
			wo->allocate((icmBase*)wo);

			for (i = 0; i < r->nent; i++) {
				for (j = 0; j < 3; j++) {
					double vv = r->v[j][i];
					int ivv;
					if (vv < 0.0)
						vv = 0.0;
					else if (vv > 1.0)
						vv = 1.0;
					((unsigned short*)wo->u.table.data)[r->nent * j + i] = (int)(vv * 65535.0 + 0.5);
				}
			}
		}
	
		if ((ppath = malloc(strlen(tpath) + 6)) == NULL) {
			debugr2((errout,"malloc failed for display '%s'\n",p->name));
			icco->del(icco);
			free(tpath);
			CFRelease(id);
			CFRelease(url);
			return 1;
		}
		strcpy(ppath, tpath);
		strcat(ppath,".orig");

		/* Rename the currently installed profile temporarily. */
		/* This will fail if current profile is a system profile and not writable by the user. */
		/* This could be worked around by cloning the system profile to the user */
		/* area and installing it before modifiying it. */
		if (rename(tpath, ppath) != 0) {
			debugr2((errout,"Unable to rename '%s' to '%s'\n",tpath,ppath));
			icco->del(icco);
			free(ppath);
			free(tpath);
			CFRelease(id);
			CFRelease(url);
			return 2;
		}

		/* Rename worked */

		debugr2((errout,"Renamed current profile '%s' to '%s'\n",tpath,ppath));

		/* Save the modified profile to the original profile name */
		if ((wr_fp = new_icmFileStd_name(tpath,"w")) == NULL) {
			debugr2((errout,"Failed to open '%s' for writing\n",tpath));
			free(ppath);
			icco->del(icco);
			free(tpath);
			CFRelease(id);
			CFRelease(url);
			return 1;
		}
	
		if ((rv = icco->write(icco,wr_fp,0)) != 0) {
			debugr2((errout,"Write file: %d, %s",rv,icco->err));
			free(ppath);
			wr_fp->del(wr_fp);
			icco->del(icco);
			free(tpath);
			CFRelease(id);
			CFRelease(url);
			return 1;
		}
	
		icco->del(icco);
		wr_fp->del(wr_fp);

		/* Update to the "current" profile, which is actually the modified profile */
		if ((dispuuid = CGDisplayCreateUUIDFromDisplayID(p->ddid)) == NULL) {
			debugr2((errout,"CGDisplayCreateUUIDFromDisplayID() failed\n"));
			free(ppath);
			free(tpath);
			CFRelease(id);
			CFRelease(url);
			return 1;
		}

		keys[0] = id;
		values[0] = url;

		if ((dict = CFDictionaryCreate(kCFAllocatorDefault, (const void **)&keys,
			                     (const void **)&values, 1, NULL, NULL)) == NULL) {
			debugr2((errout,"CFDictionaryCreate() failed\n"));
			CFRelease(dispuuid);
			free(ppath);
			free(tpath);
			CFRelease(id);
			CFRelease(url);
			return 1;
		}
		if (!ColorSyncDeviceSetCustomProfiles(kColorSyncDisplayDeviceClass, dispuuid, dict)) {
			debugr2((errout,"ColorSyncDeviceSetCustomProfiles() failed\n"));
			CFRelease(dict);
			CFRelease(dispuuid);
			free(ppath);
			free(tpath);
			CFRelease(id);
			CFRelease(url);
			return 1;
		}
		CFRelease(dict);
		CFRelease(dispuuid);
		CFRelease(id);
		CFRelease(url);

		/* Delete the temporary profile */
		unlink(tpath);

		/* Rename the current profile back to it's correct name */
		if (rename(ppath, tpath) != 0) {
			debugr2((errout,"Renaming existing profile '%s' failed\n",ppath));
			return 1;
		}
		debugr2((errout,"Restored '%s' back to '%s'\n",ppath,tpath));
		free(ppath);
		free(tpath);
	}
#else	/* < 10.6 */
	if (persist) {					/* Persistent */
		CMError ev;
		CMProfileRef prof;			/* Current AVID profile */
		CMProfileLocation ploc;		/* Current profile location */
		UInt32 plocsz = sizeof(CMProfileLocation);
		char *ppath;				/* Current/renamed profiles path */
		char *tpath;				/* Temporary profiles/original profiles path */
		CMProfileRef tprof;			/* Temporary profile */
		CMProfileLocation tploc;	/* Temporary profile */
		CMVideoCardGammaType *vcgt = NULL;	/* vcgt tag */
		int size;
		int i, j;

		debugr("Set_ramdac persist\n");

		/* Get the current installed profile */
		if ((ev = CMGetProfileByAVID((CMDisplayIDType)p->ddid, &prof)) != noErr) {
			debugr2((errout,"CMGetProfileByAVID() failed for display '%s' with error %d\n",p->name,ev));
			return 1;
		}

		/* Get the current installed  profile's location */
		if ((ev = NCMGetProfileLocation(prof, &ploc, &plocsz)) != noErr) {
			debugr2((errout,"NCMGetProfileLocation() failed for display '%s' with error %d\n",p->name,ev));
			return 1;
		}

		debugr2((errout, "Current profile path = '%s'\n",plocpath(&ploc)));

		if ((tpath = plocpath(&ploc)) == NULL) {
			debugr2((errout,"plocpath failed for display '%s'\n",p->name));
			return 1;
		}

		if (strlen(tpath) > 255) {
			debugr2((errout,"current profile path is too long\n"));
			return 1;
		}
		if ((ppath = malloc(strlen(tpath) + 6)) == NULL) {
			debugr2((errout,"malloc failed for display '%s'\n",p->name));
			free(tpath);
			return 1;
		}
		strcpy(ppath,tpath);
		strcat(ppath,".orig");

		/* Rename the currently installed profile temporarily */
		if (rename(tpath, ppath) != 0) {
			debugr2((errout,"Renaming existing profile '%s' failed\n",ppath));
			return 2;
		}
		debugr2((errout,"Renamed current profile '%s' to '%s'\n",tpath,ppath));

		/* Make a copy of the renamed current profile back to it's true name */
		tploc.locType = cmPathBasedProfile;
		strncpy(tploc.u.pathLoc.path, tpath, 255);
		tploc.u.pathLoc.path[255] = '\000';

		/* Make the temporary copy */
		if ((ev = CMCopyProfile(&tprof, &tploc, prof)) != noErr) {
			debugr2((errout,"CMCopyProfile() failed for display '%s' with error %d\n",p->name,ev));
			CMCloseProfile(prof);
			unlink(tpath);
			rename(ppath, tpath);
			return 1;
		}
		CMCloseProfile(prof);

		if ((ev = CMSetProfileDescriptions(tprof, "Dispwin Temp", 13, NULL, 0, NULL, 0)) != noErr) {
			debugr2((errout,"cmVideoCardGammaTag`() failed for display '%s' with error %d\n",p->name,ev));
			CMCloseProfile(tprof);
			unlink(tpath);
			rename(ppath, tpath);
			return 1;
		}
		
		/* Change the description and set the vcgt tag to the calibration */
		if ((vcgt = malloc(size = (sizeof(CMVideoCardGammaType) - 1 + 3 * 2 * r->nent))) == NULL) {
			debugr2((errout,"malloc of vcgt tag failed for display '%s' with error %d\n",p->name,ev));
			CMCloseProfile(tprof);
			unlink(tpath);
			rename(ppath, tpath);
			return 1;
		}
		cs_w32(&vcgt->typeDescriptor, cmSigVideoCardGammaType);
		cs_w32(&vcgt->gamma.tagType, cmVideoCardGammaTableType);	/* Table, not formula */
		cs_w16(&vcgt->gamma.u.table.channels, 3);
		cs_w16(&vcgt->gamma.u.table.entryCount, r->nent);
		cs_w16(&vcgt->gamma.u.table.entrySize, 2);

		for (i = 0; i < r->nent; i++) {
			for (j = 0; j < 3; j++) {
				double vv = r->v[j][i];
				int ivv;
				if (vv < 0.0)
					vv = 0.0;
				else if (vv > 1.0)
					vv = 1.0;
				ivv = (int)(vv * 65535.0 + 0.5);
				cs_w16(((unsigned short *)vcgt->gamma.u.table.data) + ((j * r->nent) + i), ivv);
			}
		}

		/* Replace or add a vcgt tag */
		if ((ev = CMSetProfileElement(tprof, cmVideoCardGammaTag, size, vcgt)) != noErr) {
			debugr2((errout,"CMSetProfileElement vcgt tag failed with error %d\n",ev));
			free(vcgt);
			CMCloseProfile(tprof);
			unlink(tpath);
			rename(ppath, tpath);
			return 1;
		}
		free(vcgt);

		if ((ev = CMUpdateProfile(tprof)) != noErr) {
			debugr2((errout,"CMUpdateProfile failed with error %d\n",ev));
			CMCloseProfile(tprof);
			unlink(tpath);
			rename(ppath, tpath);
			return 1;
		}

		/* Make temporary file the current profile - updates LUTs */
		if ((ev = CMSetProfileByAVID((CMDisplayIDType)p->ddid, tprof)) != noErr) {
			debugr2((errout,"CMSetProfileByAVID() failed for display '%s' with error %d\n",p->name,ev));
			CMCloseProfile(tprof);
			unlink(tpath);
			rename(ppath, tpath);
			return 1;
		}
		CMCloseProfile(tprof);
		debugr2((errout,"Set display to use temporary profile '%s'\n",tpath));

		/* Delete the temporary profile */
		unlink(tpath);

		/* Rename the current profile back to it's correct name */
		if (rename(ppath, tpath) != 0) {
			debugr2((errout,"Renaming existing profile '%s' failed\n",ppath));
			return 1;
		}
		debugr2((errout,"Restored '%s' back to '%s'\n",ppath,tpath));
	}
#endif	/* < 10.6 */

#endif /* __APPLE__ */

#if defined(UNIX_X11)
	unsigned short vals[3][16384];

	debugr("dispwin_set_ramdac called\n");

	for (j = 0; j < 3; j++) {
		for (i = 0; i < r->nent; i++) {
			double vv = r->v[j][i];
			if (vv < 0.0)
				vv = 0.0;
			else if (vv > 1.0)
				vv = 1.0;
			vals[j][i] = (int)(vv * 65535.0 + 0.5);
		}
	}

#if RANDR_MAJOR == 1 && RANDR_MINOR >= 2 && !defined(DISABLE_RANDR)
	if (p->crtc != 0) {		/* Using Xrandr 1.2 */
		XRRCrtcGamma *crtcgam;

		debugr("Setting gamma using Randr 1.2\n");

		if ((crtcgam = XRRAllocGamma(r->nent)) == NULL) {
			debugr(" XRRAllocGamma failed\n");
			return 1;
		}

		for (i = 0; i < r->nent; i++) {
			crtcgam->red[i]   = vals[0][i];
			crtcgam->green[i] = vals[1][i];
			crtcgam->blue[i]  = vals[2][i];
		}

		XRRSetCrtcGamma(p->mydisplay, p->crtc, crtcgam);
		XSync(p->mydisplay, False);		/* Flush the change out */

		XRRFreeGamma(crtcgam);

	} else
#endif /* randr >= V 1.2 */
	{
		/* Some propietary multi-screen drivers (ie. TwinView & MergedFB) */
		/* don't implement the XVidMode extenstion properly. */
		if (XSetErrorHandler(null_error_handler) == 0) {
			debugr("get_displays failed on XSetErrorHandler\n");
			return 1;
		}
		if (XF86VidModeSetGammaRamp(p->mydisplay, p->myrscreen, r->nent, vals[0], vals[1], vals[2]) == 0) {
			XSetErrorHandler(NULL);
			debugr("XF86VidModeSetGammaRamp failed\n");
			return 1;
		}
		XSync(p->mydisplay, False);		/* Flush the change out */
		XSetErrorHandler(NULL);
	}
#endif	/* UNXI X11 */

	debugr("dispwin_set_ramdac returning OK\n");

	return 0;
}


/* Clone ourselves */
ramdac *dispwin_clone_ramdac(ramdac *r) {
	ramdac *nr;
	int i, j;

	debug("dispwin_clone_ramdac called\n");

	/* Allocate a ramdac */
	if ((nr = (ramdac *)calloc(sizeof(ramdac), 1)) == NULL) {
		return NULL;
	}

	*nr = *r;		/* Structrure copy */

	for (j = 0; j < 3; j++) {

		if ((nr->v[j] = (double *)calloc(sizeof(double), r->nent)) == NULL) {
			for (j--; j >= 0; j--)
				free(nr->v[j]);
			free(nr);
			return NULL;
		}
	}

	for (j = 0; j < 3; j++) {
		for (i = 0; i < r->nent; i++) {
			nr->v[j][i] = r->v[j][i];
		}
	}

	debug("clone is done\n");
	return nr;
}

/* Set the ramdac values to linear */
void dispwin_setlin_ramdac(ramdac *r) {
	int i, j;

	debug("dispwin_setlin_ramdac called\n");

	for (i = 0; i < r->nent; i++) {
		double val = i/(r->nent - 1.0);
		for (j = 0; j < 3; j++) {
			r->v[j][i] = val;
		}
	}
}

/* We're done with a ramdac structure */
void dispwin_del_ramdac(ramdac *r) {
	int j;

	debug("dispwin_del_ramdac called\n");

	for (j = 0; j < 3; j++) {
		free(r->v[j]);
	}

	free(r);
}

/* ----------------------------------------------- */
/* Useful function for X11 profile atom settings */

#if defined(UNIX_X11)
/* Return NZ on error */
static int set_X11_atom(dispwin *p, char *fname) {
	FILE *fp;
	unsigned long psize, bread;
	unsigned char *atomv;

	debugr("Setting _ICC_PROFILE property\n");

	/* Read in the ICC profile, then set the X11 atom value */
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	if ((fp = fopen(fname,"rb")) == NULL)
#else
	if ((fp = fopen(fname,"r")) == NULL)
#endif
	{
		debugr2((errout,"Can't open file '%s'\n",fname));
		return 1;
	}

	/* Figure out how big it is */
	if (fseek(fp, 0, SEEK_END)) {
		debugr2((errout,"Seek '%s' to EOF failed\n",fname));
		return 1;
	}
	psize = (unsigned long)ftell(fp);

	if (fseek(fp, 0, SEEK_SET)) {
		debugr2((errout,"Seek '%s' to SOF failed\n",fname));
		return 1;
	}

	if ((atomv = (unsigned char *)malloc(psize)) == NULL) {
		debugr2((errout,"Failed to allocate buffer for profile '%s'\n",fname));
		return 1;
	}

	if ((bread = fread(atomv, 1, psize, fp)) != psize) {
		debugr2((errout,"Failed to read profile '%s' into buffer\n",fname));
		return 1;
	}
	
	fclose(fp);

	XChangeProperty(p->mydisplay, RootWindow(p->mydisplay, 0), p->icc_atom,
	                XA_CARDINAL, 8, PropModeReplace, atomv, psize);

#if RANDR_MAJOR == 1 && RANDR_MINOR >= 2 && !defined(DISABLE_RANDR)
	if (p->icc_out_atom != 0) {
		/* If Xrandr 1.2, set property on output */
		/* This seems to fail on some servers. Ignore the error ? */
		if (XSetErrorHandler(null_error_handler) == 0) {
			debugr("get_displays failed on XSetErrorHandler\n");
			return 1;
		}
		g_error_handler_triggered = 0;
		XRRChangeOutputProperty(p->mydisplay, p->output, p->icc_out_atom,
	                XA_CARDINAL, 8, PropModeReplace, atomv, psize);
		if (g_error_handler_triggered != 0) {
			debugr("XRRChangeOutputProperty failed\n");
			warning("Unable to set _ICC_PROFILE property on output"); 
		}
		XSync(p->mydisplay, False);		/* Flush the change out */
		XSetErrorHandler(NULL);
	}
#endif /* randr >= V 1.2 */
	free(atomv);

	return 0;
}
#endif  /* UNXI X11 */

/* ----------------------------------------------- */
/* See if colord is available */

#if defined(UNIX_X11) && defined(USE_UCMM)

/* colord libcolordcompat.so shim functions */

ucmm_error (*cd_edid_install_profile)(unsigned char *edid, int edid_len,
                                      ucmm_scope scope, char *profile_fn) = NULL;
ucmm_error (*cd_edid_remove_profile)(unsigned char  *edid, int edid_len, char *profile_fn) = NULL;
ucmm_error (*cd_edid_get_profile)(unsigned char *edid, int edid_len, char **profile_fn) = NULL;
int cd_init = 0;		/* nz if we've looked for colord */
void *cd_found = NULL;	/* .so handle if we've found colord */

/* Return nz if found colord functions */
int dispwin_checkfor_colord() {

	if (cd_init)
		return (cd_found != NULL);

	cd_found = NULL;

	if ((cd_found = dlopen("libcolordcompat.so", RTLD_LAZY)) != NULL) {

		cd_edid_install_profile = dlsym(cd_found, "cd_edid_install_profile");
		cd_edid_remove_profile = dlsym(cd_found, "cd_edid_remove_profile");
		cd_edid_get_profile = dlsym(cd_found, "cd_edid_get_profile");

		if (cd_edid_install_profile == NULL
		 || cd_edid_remove_profile == NULL
		 || cd_edid_get_profile == NULL) {
			cd_found = NULL;
		}
	}

	cd_init = 1;

	return (cd_found != NULL);
}

#endif



/* ----------------------------------------------- */
/* Install a display profile and make */
/* it the default for this display. */
/* Set the display to the calibration in the profile */
/* (r == NULL if no calibration) */
/* (We assume that the caller has checked that it's an ICC profile) */
/* Return nz if failed */
int dispwin_install_profile(dispwin *p, char *fname, ramdac *r, p_scope scope) {
	debugr2((errout,"dispwin_install_profile '%s'\n",fname));
#ifdef NT
	{
		char *fullpath;
		char *basename;
		char colpath[MAX_PATH];
		unsigned long colpathlen = MAX_PATH;
		WCS_PROFILE_MANAGEMENT_SCOPE wcssc;
		unsigned short *wpath, *wbname, *wmonid;

		if (GetColorDirectory(NULL, colpath, &colpathlen) == 0) {
			debugr2((errout,"Getting color directory failed\n"));
			return 1;
		}

		if ((fullpath = _fullpath(NULL, fname, 0)) == NULL) {
			debugr2((errout,"_fullpath() failed\n"));
			return 1;
		}

		if ((basename = PathFindFileNameX(fullpath)) == NULL) {
			debugr2((errout,"Locating base name in '%s' failed\n",fname));
			free(fullpath);
			return 1;
		}

		if ((strlen(colpath) + strlen(basename) + 2) > MAX_PATH) {
			debugr2((errout,"Installed profile path too long\n"));
			free(fullpath);
			return 1;
		}
		strcat(colpath, "\\");
		strcat(colpath, basename);

		/* Setup in case we're on Vista */
		if (scope == p_scope_user)
			wcssc = WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER;
		else 
			wcssc = WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE;

		if ((wpath = char2wchar(fullpath)) == NULL) { 
			debugr2((errout,"char2wchar failed\n"));
			free(fullpath);
			return 1;
		}

		if ((wbname = char2wchar(basename)) == NULL) { 
			debugr2((errout,"char2wchar failed\n"));
			free(wpath);
			free(fullpath);
			return 1;
		}

		if ((wmonid = char2wchar(p->monid)) == NULL) {
			debugr2((errout,"char2wchar failed\n"));
			free(wbname);
			free(wpath);
			free(fullpath);
			return 1;
		}

		debugr2((errout,"Installing '%s'\n",fname));
		
		/* Install doesn't replace an existing installed profile, */
		/* so we need to try and delete this profile first */
		if (pWcsDisassociateColorProfileFromDevice != NULL) {
			(*pWcsDisassociateColorProfileFromDevice)(wcssc, (PCWSTR)wbname, (PCWSTR)wmonid);
		} else {
			DisassociateColorProfileFromDevice(NULL, basename, p->monid);
		}
		if (UninstallColorProfile(NULL,  basename, TRUE) == 0) {
			/* UninstallColorProfile fails on Win2K */
			_unlink(colpath);
		}

		if (InstallColorProfile(NULL, fullpath) == 0) {
			debugr2((errout,"InstallColorProfile() failed for file '%s' with error %d\n",fname,GetLastError()));
			free(wmonid);
			free(wbname);
			free(wpath);
			free(fullpath);
			return 1;
		}

		debugr2((errout,"Associating '%s' with '%s'\n",fullpath,p->monid));
		if (pWcsAssociateColorProfileWithDevice != NULL) {
			debugr("Using Vista Associate\n");
			if ((*pWcsAssociateColorProfileWithDevice)(wcssc, (PCWSTR)wpath, (PCWSTR)wmonid) == 0) {
				debugr2((errout,"WcsAssociateColorProfileWithDevice() failed for file '%s' with error %d\n",fullpath,GetLastError()));
				free(wmonid);
				free(wbname);
				free(wpath);
				free(fullpath);
				return 1;
			}
		} else {
			if (AssociateColorProfileWithDevice(NULL, fullpath, p->monid) == 0) {
				debugr2((errout,"AssociateColorProfileWithDevice() failed for file '%s' with error %d\n",fullpath,GetLastError()));
				free(wmonid);
				free(wbname);
				free(wpath);
				free(fullpath);
				return 1;
			}
		}

		free(wmonid);
		free(wbname);
		free(wpath);
		free(fullpath);
		/* The default profile will be the last one associated */

		/* MSWindows doesn't generally set the display to the current profile calibration, */
		/* so we do it. */
		if (p->set_ramdac(p,r,1))
			error("Failed to set VideoLUT");

		return 0;
	}
#endif /* NT */

/* For Linux and OS X, make sure we don't create a file with the wrong owner */
#if defined(UNIX)
	/* If we're creating a user profile and running as root sudo */
	if (scope == p_scope_user && geteuid() == 0) {
		char *uids, *gids;
		int uid, gid;

		debugr("We're setting a user profile running as root - run as user\n");
		if ((uids = getenv("SUDO_UID")) != NULL
		 && (gids = getenv("SUDO_GID")) != NULL) {
			uid = atoi(uids);
			gid = atoi(gids);
			if (setegid(gid) || seteuid(uid)) {
				debugr("seteuid or setegid failed\n");
			} else {
				debug2((errout,"Set euid %d and egid %d\n",uid,gid));
			}
		}
	/* If setting local system profile and not effective root, but sudo */
	} else if (scope != p_scope_user && getuid() == 0 && geteuid() != 0) {
		if (getenv("SUDO_UID") != NULL
		 && getenv("SUDO_GID") != NULL) {

			debugr("We're setting a system profile running as user - revert to root\n");
			if (setegid(getgid()) || seteuid(getuid()))
				debugr("seteuid or setegid failed\n");
		}
	}
#endif /* OS X || Linux */

#ifdef __APPLE__

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	{
		char *dpath;		/* Install file path */
		CFUUIDRef dispuuid;
		CFStringRef cfprofpath;
		CFStringRef keys[1];
		CFURLRef values[1];
		CFDictionaryRef dict;
	
		if ((dispuuid = CGDisplayCreateUUIDFromDisplayID(p->ddid)) == NULL) {
			debugr2((errout,"CGDisplayCreateUUIDFromDisplayID() failed\n"));
			return 1;
		}

		/* Determine the location the profile will be installed into */
		if ((dpath = iprof_path(scope, fname)) == NULL) {
			debugr2((errout,"iprof_path() failed\n"));
			CFRelease(dispuuid);
			return 1;
		}

		debugr2((errout,"Source profile '%s'\n",fname));
		debugr2((errout,"Destination profile '%s'\n",dpath));

		/* Copy the new profile to the destination */
		if (copyfile(fname, dpath, NULL, COPYFILE_ALL) != 0) {
			debugr2((errout,"copyfile failed\n"));
			free(dpath);
			CFRelease(dispuuid);
			return 1;
		}
		
		/* Register it with the OS and make it the default */
		if ((cfprofpath = CFStringCreateWithCString(kCFAllocatorDefault, dpath,
		                                        kCFStringEncodingUTF8)) == NULL) {
			debugr2((errout,"CFStringCreateWithCString() failed\n"));
			free(dpath);
			CFRelease(dispuuid);
			return 1;
		}
		free(dpath); dpath = NULL;

		keys[0] = kColorSyncDeviceDefaultProfileID;
		if ((values[0] = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfprofpath,
		                                         kCFURLPOSIXPathStyle, false)) == NULL) {
			debugr2((errout,"CFURLCreateWithFileSystemPath() failed\n"));
			CFRelease(cfprofpath);
			CFRelease(dispuuid);
			return 1;
		}

		if ((dict = CFDictionaryCreate(kCFAllocatorDefault, (const void **)&keys,
			                     (const void **)&values, 1, NULL, NULL)) == NULL) {
			debugr2((errout,"CFDictionaryCreate() failed\n"));
			CFRelease(values[0]);
			CFRelease(cfprofpath);
			CFRelease(dispuuid);
			return 1;
		}
		if (!ColorSyncDeviceSetCustomProfiles(kColorSyncDisplayDeviceClass, dispuuid, dict)) {
			debugr2((errout,"ColorSyncDeviceSetCustomProfiles() failed\n"));
			CFRelease(dict);
			CFRelease(values[0]);
			CFRelease(cfprofpath);
			CFRelease(dispuuid);
			return 1;
		}
		CFRelease(dict);
		CFRelease(values[0]);
		CFRelease(cfprofpath);
		CFRelease(dispuuid);

		return 0;
	}
#else	/* 10.6 and prior */
		// Switch to using iprof_path() to simplify ?
	{
		CMError ev;
		short vref;
		FSRef dirref;
		char dpath[FILENAME_MAX];
		char *basename;
	
		CMProfileLocation ploc;		/* Source profile location */
		CMProfileRef prof;			/* Source profile */
		CMProfileLocation dploc;	/* Destinaion profile location */
		CMProfileRef dprof;			/* Destinaion profile */

		if (scope == p_scope_network)
			vref = kNetworkDomain;
		else if (scope == p_scope_system)
			vref = kSystemDomain;
		else if (scope == p_scope_local)
			vref = kLocalDomain;
		else 
			vref = kUserDomain;

		/* Locate the appropriate ColorSync path */
		if ((ev = FSFindFolder(vref, kColorSyncProfilesFolderType, kCreateFolder, &dirref)) != noErr) {
			debugr2((errout,"FSFindFolder() failed with error %d\n",ev));
			return 1;
		}

		/* Convert to POSIX path */
		if ((ev = FSRefMakePath(&dirref, (unsigned char *)dpath, FILENAME_MAX)) != noErr) {
			debugr2((errout,"FSRefMakePath failed with error %d\n",ev));
			return 1;
		}

		/* Locate the base filename in the fname */
		for (basename = fname + strlen(fname);  ; basename--) {
			if (basename <= fname || basename[-1] == '/')
				break;
		}

		/* Append the basename to the ColorSync directory path */
		if ((strlen(dpath) + strlen(basename) + 2) > FILENAME_MAX
		 || (strlen(dpath) + strlen(basename) + 2) > 256) {
			debugr2((errout,"ColorSync dir + profile name too long\n"));
			return 1;
		}
		strcat(dpath, "/");
		strcat(dpath, basename);

		debugr2((errout,"Source profile '%s'\n",fname));
		debugr2((errout,"Destination profile '%s'\n",dpath));

		/* Open the profile we want to install */
		ploc.locType = cmPathBasedProfile;
		strncpy(ploc.u.pathLoc.path, fname, 255);
		ploc.u.pathLoc.path[255] = '\000';

		if ((ev = CMOpenProfile(&prof, &ploc)) != noErr) {
			debugr2((errout,"CMOpenProfile() failed for file '%s' with error %d\n",fname,ev));
			return 1;
		}

		/* Delete any current profile */
		unlink(dpath);

		/* Make a copy of it to the ColorSync directory */
		dploc.locType = cmPathBasedProfile;
		strncpy(dploc.u.pathLoc.path, dpath, 255);
		dploc.u.pathLoc.path[255] = '\000';

		if ((ev = CMCopyProfile(&dprof, &dploc, prof)) != noErr) {
			debugr2((errout,"CMCopyProfile() failed for file '%s' with error %d\n",dpath,ev));
			return 1;
		}
		
		/* Make it the current profile - updates LUTs */
		if ((ev = CMSetProfileByAVID((CMDisplayIDType)p->ddid, dprof)) != noErr) {
			debugr2((errout,"CMSetProfileByAVID() failed for file '%s' with error %d\n",fname,ev));
			return 1;
		}
		CMCloseProfile(prof);
		CMCloseProfile(dprof);

		return 0;
	}
#endif	/* 10.6 and prior */
#endif /*  __APPLE__ */

#if defined(UNIX_X11) && defined(USE_UCMM)
	{
		ucmm_error ev;
		ucmm_scope sc;
		FILE *fp;
		unsigned long psize, bread;
		unsigned char *atomv;
		int rv;

		if (scope == p_scope_network
		 || scope == p_scope_system
		 || scope == p_scope_local)
			sc = ucmm_local_system;
		else 
			sc = ucmm_user;

		if (cd_found)
			ev = cd_edid_install_profile(p->edid, p->edid_len, sc, fname);
			// Hmm. We're relying on colord error codes being in sync with ucmm.
		else
			ev = ucmm_install_monitor_profile(sc, p->edid, p->edid_len, p->name, fname);

		if (ev != ucmm_ok) {
			debugr2((errout,"Installing profile '%s' failed with error %d '%s'\n",fname,ev,ucmm_error_string(ev)));
			return 1;
		} 

		if ((rv = set_X11_atom(p, fname)) != 0) {
			debugr2((errout,"Setting X11 atom failed"));
			return 1;
		}

		/* X11 doesn't set the display to the current profile calibration, */
		/* so we do it. */
		if (p->set_ramdac(p,r,1)) {
			debugr2((errout,"Failed to set VideoLUT"));
			return 1;
		}
		return 0;
	}
#endif  /* UNXI X11 */

	return 1;
}

/* Un-Install a display profile */
/* Return nz if failed, */
/* 1 if not sucessfully deleted */
/* 2 if profile not found */
int dispwin_uninstall_profile(dispwin *p, char *fname, p_scope scope) {
	debugr2((errout,"dispwin_uninstall_profile '%s'\n", fname));
#ifdef NT
	{
		char *fullpath;
		char *basename;
		char colpath[MAX_PATH];
		unsigned long colpathlen = MAX_PATH;
		WCS_PROFILE_MANAGEMENT_SCOPE wcssc;
		unsigned short *wbname, *wmonid;

		if (GetColorDirectory(NULL, colpath, &colpathlen) == 0) {
			debugr2((errout,"Getting color directory failed\n"));
			return 1;
		}

		if ((fullpath = _fullpath(NULL, fname, 0)) == NULL) {
			debugr2((errout,"_fullpath() failed\n"));
			return 1;
		}

		if ((basename = PathFindFileNameX(fullpath)) == NULL) {
			debugr2((errout,"Locating base name in '%s' failed\n",fname));
			free(fullpath);
			return 1;
		}

		if ((strlen(colpath) + strlen(basename) + 2) > MAX_PATH) {
			debugr2((errout,"Installed profile path too long\n"));
			free(fullpath);
			return 1;
		}
		strcat(colpath, "\\");
		strcat(colpath, basename);

		/* Setup in case we're on Vista */
		if (scope == p_scope_user)
			wcssc = WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER;
		else 
			wcssc = WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE;

		if ((wbname = char2wchar(basename)) == NULL) { 
			debugr2((errout,"char2wchar failed\n"));
			free(fullpath);
			return 1;
		}

		if ((wmonid = char2wchar(p->monid)) == NULL ) {
			debugr2((errout,"char2wchar failed\n"));
			free(wbname);
			free(fullpath);
			return 1;
		}

		debugr2((errout,"Disassociating '%s' from '%s'\n",basename,p->monid));

		if (pWcsDisassociateColorProfileFromDevice != NULL) {
			debugr("Using Vista Disassociate\n");
			/* Ignore error if profile is already disasociated or doesn't exist */
			if ((*pWcsDisassociateColorProfileFromDevice)(wcssc, wbname, wmonid) == 0
			 && GetLastError() != 2015 && GetLastError() != 2011) {
				debugr2((errout,"WcsDisassociateColorProfileWithDevice() failed for file '%s' with error %d\n",basename,GetLastError()));
				free(wmonid);
				free(wbname);
				free(fullpath);
				return 1;
			}
		} else {
			/* Ignore error if profile is already disasociated or doesn't exist */
			if (DisassociateColorProfileFromDevice(NULL, basename, p->monid) == 0
			 && GetLastError() != 2015 && GetLastError() != 2011) {
				debugr2((errout,"DisassociateColorProfileWithDevice() failed for file '%s' with error %d\n",basename,GetLastError()));
				free(wmonid);
				free(wbname);
				free(fullpath);
				return 1;
			}
		}

		if (UninstallColorProfile(NULL, basename, TRUE) == 0) {
			/* This can happen when some other program has the profile open */
//			int ev;
//			struct _stat sbuf;
			debugr2((errout,"Warning, uninstallColorProfile() failed for file '%s' with error %d\n", basename,GetLastError()));
			free(wmonid);
			free(wbname);
			free(fullpath);
			return 2;
		}

		free(wmonid);
		free(wbname);
		free(fullpath);

		return 0;
	}
#endif /* NT */

/* For Linux and OS X, make sure we don't create a file with the wrong owner */
#if defined(UNIX)
	/* If we're creating a user profile and running as root sudo */
	if (scope == p_scope_user && geteuid() == 0) {
		char *uids, *gids;
		int uid, gid;

		debugr("We're setting a user profile running as root - run as user\n");
		if ((uids = getenv("SUDO_UID")) != NULL
		 && (gids = getenv("SUDO_GID")) != NULL) {
			uid = atoi(uids);
			gid = atoi(gids);
			if (setegid(gid) || seteuid(uid)) {
				debugr("seteuid or setegid failed\n");
			}
			debug2((errout,"Set euid %d and egid %d\n",uid,gid));
		}
	/* If setting local system proile and not effective root, but sudo */
	} else if (scope != p_scope_user && getuid() == 0 && geteuid() != 0) {
		if (getenv("SUDO_UID") != NULL
		 && getenv("SUDO_GID") != NULL) {

			debugr("We're setting a system profile running as user - revert to root\n");
			setegid(getgid());
			seteuid(getuid());
		}
	}
#endif /* OS X || Linux */
#ifdef __APPLE__
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
	{
		char *dpath;		/* Un-install file path */
		struct stat sbuf;
		int ev;
		CFStringRef keys[1];
		CFURLRef values[1];
		CFDictionaryRef dict;
		CFUUIDRef dispuuid;
	
		/* Determine the location the profile will be installed into */
		if ((dpath = iprof_path(scope, fname)) == NULL) {
			debugr2((errout,"iprof_path() failed\n"));
			return 1;
		}

		debugr2((errout,"Profile to delete '%s'\n",dpath));

		if (stat(dpath, &sbuf) != 0) {
			debugr2((errout,"delete '%s' profile doesn't exist\n",dpath));
			return 2;
		}
		if ((ev = unlink(dpath)) != 0) {
			debugr2((errout,"delete '%s' failed with %d\n",dpath,ev));
			return 1;
		}

		free(dpath); dpath = NULL;

		/* Make ColorSync notice that it's gone */
		/* (Works on 10.7, but not 10.6 ? */
		if ((dispuuid = CGDisplayCreateUUIDFromDisplayID(p->ddid)) == NULL) {
			debugr2((errout,"CGDisplayCreateUUIDFromDisplayID() failed\n"));
			return 1;
		}

		keys[0] = kColorSyncDeviceDefaultProfileID;
		values[0] = (CFURLRef)kCFNull;

		if ((dict = CFDictionaryCreate(kCFAllocatorDefault, (const void **)&keys,
			                     (const void **)&values, 1, NULL, NULL)) == NULL) {
			debugr2((errout,"CFDictionaryCreate() failed\n"));
			CFRelease(dispuuid);
			return 1;
		}

		if (!ColorSyncDeviceSetCustomProfiles(kColorSyncDisplayDeviceClass, dispuuid, dict)) {
			debugr2((errout,"ColorSyncDeviceSetCustomProfiles() failed\n"));
			CFRelease(dict);
			CFRelease(dispuuid);
			return 1;
		}
		CFRelease(dict);
		CFRelease(dispuuid);

		return 0;
	}
#else	/* 10.6 and prior */
		// ~~~ can use above code
	{
		CMError cmev;
		int ev;
		short vref;
		char dpath[FILENAME_MAX];
		char *basename;
		FSRef dirref;
		struct stat sbuf;

		if (scope == p_scope_network)
			vref = kNetworkDomain;
		else if (scope == p_scope_system)
			vref = kSystemDomain;
		else if (scope == p_scope_local)
			vref = kLocalDomain;
		else 
			vref = kUserDomain;

		/* Locate the appropriate ColorSync path */
		if ((cmev = FSFindFolder(vref, kColorSyncProfilesFolderType, kCreateFolder, &dirref)) != noErr) {
			debugr2((errout,"FSFindFolder() failed with error %d\n",cmev));
			return 1;
		}

		/* Convert to POSIX path */
		if ((cmev = FSRefMakePath(&dirref, (unsigned char *)dpath, FILENAME_MAX)) != noErr) {
			debugr2((errout,"FSRefMakePath failed with error %d\n",cmev));
			return 1;
		}

		/* Locate the base filename in the fname */
		for (basename = fname + strlen(fname);  ; basename--) {
			if (basename <= fname || basename[-1] == '/')
				break;
		}

		/* Append the basename to the ColorSync directory path */
		if ((strlen(dpath) + strlen(basename) + 2) > FILENAME_MAX
		 || (strlen(dpath) + strlen(basename) + 2) > 256) {
			debugr2((errout,"ColorSync dir + profile name too long\n"));
			return 1;
		}
		strcat(dpath, "/");
		strcat(dpath, basename);
		debugr2((errout,"Profile to delete '%s'\n",dpath));

		if (stat(dpath, &sbuf) != 0) {
			debugr2((errout,"delete '%s' profile doesn't exist\n",dpath));
			return 2;
		}
		if ((ev = unlink(dpath)) != 0) {
			debugr2((errout,"delete '%s' failed with %d\n",dpath,ev));
			return 1;
		}

		return 0;
	}
#endif	/* 10.6 and prior */
#endif /*  __APPLE__ */

#if defined(UNIX_X11) && defined(USE_UCMM)
	{
		ucmm_error ev;
		ucmm_scope sc;

		if (scope == p_scope_network
		 || scope == p_scope_system
		 || scope == p_scope_local)
			sc = ucmm_local_system;
		else 
			sc = ucmm_user;

		if (cd_found)
			ev = cd_edid_remove_profile(p->edid, p->edid_len, fname);
		else
			ev = ucmm_uninstall_monitor_profile(sc, p->edid, p->edid_len, p->name, fname);

		if (ev != ucmm_ok) {
			debugr2((errout,"Installing profile '%s' failed with error %d '%s'\n",fname,ev,ucmm_error_string(ev)));
			return 1;
		} 

		XDeleteProperty(p->mydisplay, RootWindow(p->mydisplay, 0), p->icc_atom);

#if RANDR_MAJOR == 1 && RANDR_MINOR >= 2 && !defined(DISABLE_RANDR)
		/* If Xrandr 1.2, set property on output */
		if (p->icc_out_atom != 0) {
			XRRDeleteOutputProperty(p->mydisplay, p->output, p->icc_out_atom);
		}
#endif /* randr >= V 1.2 */
		return 0;
	}
#endif  /* UNXI X11 */

	return 1;
}

/* Get the currently installed display profile and return it as an icmFile. */
/* Return the name as well, up to mxlen chars, excluding nul. */
/* Return NULL if failed. */
icmFile *dispwin_get_profile(dispwin *p, char *name, int mxlen) {
		icmFile *rd_fp = NULL;

#ifdef NT
	{
		char buf[MAX_PATH];
		DWORD blen = MAX_PATH;

		if (GetICMProfile(p->hdc, &blen, buf) == 0) {
			debugr2((errout, "GetICMProfile failed, lasterr = %d\n",GetLastError()));
			return NULL;
		}

		debugr2((errout,"Loading default profile '%s'\n",buf));
		if ((rd_fp = new_icmFileStd_name(buf,"r")) == NULL)
			debugr2((errout,"Can't open file '%s'",buf));

		return rd_fp;
	}
#endif /* NT */

#ifdef __APPLE__
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	{
		char *dpath; 			/* Read file path */
		struct stat sbuf;
		icmAlloc *al;
		void *buf;
		FILE *fp;

		if ((dpath = cur_profile(p)) == NULL) {
			debugr2((errout,"cur_profile() failed\n"));
			return NULL;
		}

		/* Get the profile size */
		if (stat(dpath, &sbuf) != 0) {
			debugr2((errout,"Failed to open profile '%s'\n",dpath));
			free(dpath);
			return NULL;
		}

		if ((al = new_icmAllocStd()) == NULL) {
			debugr("new_icmAllocStd failed\n");
			free(dpath);
		    return NULL;
		}
		if ((buf = al->malloc(al, sbuf.st_size)) == NULL) {
			debugr("malloc of profile buffer failed\n");
			free(dpath);
		    return NULL;
		}

		if ((fp = fopen(dpath, "r")) == NULL) {
			debugr2((errout,"opening '%s' failed\n",dpath));
			al->free(al, buf);
			free(dpath);
			return NULL;
		}
		if (fread(buf, 1, sbuf.st_size, fp) != sbuf.st_size) {
			debugr2((errout,"reading '%s' failed\n",dpath));
			al->free(al, buf);
			fclose(fp);
			free(dpath);
			return NULL;
		}
		fclose(fp);
		free(dpath); dpath = NULL;

		/* Memory File fp that will free the buffer when deleted: */
		if ((rd_fp = new_icmFileMem_ad(buf, sbuf.st_size, al)) == NULL) {
			debugr("Creating memory file profile failed");
			al->free(al, buf);
			al->del(al);
			return NULL;
		}

		if (name != NULL) {
			strncpy(name, "Display", mxlen);
			name[mxlen] = '\000';
		}

		return rd_fp;
	}

#else	/* 10.5 and prior */
	{
		CMError ev;
		CMProfileRef prof, dprof;		/* Source profile */
		CMProfileLocation dploc;		/* Destinaion profile location */
		CMAppleProfileHeader hdr;
		icmAlloc *al;

#ifdef NEVER
		/* Get the current display profile */
		if ((ev = CMGetProfileByAVID((CMDisplayIDType)p->ddid, &prof)) != noErr) {
			debugr2((errout,"CMGetProfileByAVID() failed with error %d\n",ev));
			return NULL;
		}
#else
		CMDeviceProfileID curID;	/* Current Device Default profile ID */
		CMProfileLocation cploc;	/* Current profile location */

		/* Get the default ID for the display */
		if ((ev = CMGetDeviceDefaultProfileID(cmDisplayDeviceClass, (CMDeviceID)p->ddid, &curID)) != noErr) {
			debugr2((errout,"CMGetDeviceDefaultProfileID() failed with error %d\n",ev));
			return NULL;
		}

		/* Get the displays profile */
		if ((ev = CMGetDeviceProfile(cmDisplayDeviceClass, (CMDeviceID)p->ddid, curID, &cploc)) != noErr) {
			debugr2((errout,"CMGetDeviceDefaultProfileID() failed with error %d\n",ev));
			return NULL;
		}

		if ((ev = CMOpenProfile(&prof, &cploc)) != noErr) {
			debugr2((errout,"CMOpenProfile() failed with error %d\n",ev));
			return NULL;
		}
#endif

		/* Get the profile size */
		if ((ev = CMGetProfileHeader(prof, &hdr)) != noErr) {
			debugr2((errout,"CMGetProfileHeader() failed with error %d\n",ev));
			return NULL;
		}

		/* Make a copy of the profile to a memory buffer */
		dploc.locType = cmBufferBasedProfile;
		dploc.u.bufferLoc.size = hdr.cm1.size;
		if ((al = new_icmAllocStd()) == NULL) {
			debugr("new_icmAllocStd failed\n");
		    return NULL;
		}
		if ((dploc.u.bufferLoc.buffer = al->malloc(al, dploc.u.bufferLoc.size)) == NULL) {
			debugr("malloc of profile buffer failed\n");
		    return NULL;
		}

		if ((ev = CMCopyProfile(&dprof, &dploc, prof)) != noErr) {
			debugr2((errout,"CMCopyProfile() failed for AVID to buffer with error %d\n",ev));
			return NULL;
		}

		/* Memory File fp that will free the buffer when deleted: */
		if ((rd_fp = new_icmFileMem_ad((void *)dploc.u.bufferLoc.buffer, dploc.u.bufferLoc.size, al)) == NULL) {
			debugr("Creating memory file from CMProfileLocation failed");
			al->free(al, dploc.u.bufferLoc.buffer);
			al->del(al);
			CMCloseProfile(prof);
			CMCloseProfile(dprof);
			return NULL;
		}

		if (name != NULL) {
			strncpy(name, "Display", mxlen);
			name[mxlen] = '\000';
		}

		CMCloseProfile(prof);
		CMCloseProfile(dprof);

		return rd_fp;
	}
#endif	/* 10.5 and prior */
#endif /*  __APPLE__ */

#if defined(UNIX_X11) && defined(USE_UCMM)
	/* Try and get the currently installed profile from ucmm */
	{
		ucmm_error ev;
		char *profile = NULL;

		debugr2((errout,"dispwin_get_profile called\n"));

		if (cd_found)
			ev = cd_edid_get_profile(p->edid, p->edid_len, &profile);
		else
			ev = ucmm_get_monitor_profile(p->edid, p->edid_len, p->name, &profile);

		if (ev == ucmm_ok) {

			if (name != NULL) {
				strncpy(name, profile, mxlen);
				name[mxlen] = '\000';
			}

			debugr2((errout,"Loading current profile '%s'\n",profile));
			if ((rd_fp = new_icmFileStd_name(profile,"r")) == NULL) {
				debugr2((errout,"Can't open file '%s'",profile));
				free(profile);
				return NULL;
			}

			/* Implicitly we set the X11 atom to be the profile we just got */ 
			debugr2((errout,"Setting X11 atom to current profile '%s'\n",profile));
			if (set_X11_atom(p, profile) != 0) {
				debugr2((errout,"Setting X11 atom to profile '%s' failed",profile));
				/* Hmm. We ignore this error */
			}
			return rd_fp;
		} 
		if (ev != ucmm_no_profile) {
			debugr2((errout,"Got ucmm error %d '%s'\n",ev,ucmm_error_string(ev)));
			return NULL;
		}
		debugr2((errout,"Failed to get configured profile, so use X11 atom\n"));
		/* Drop through to using the X11 root window atom */
	}
	{
		Atom ret_type;
		int ret_format;
		long ret_len, ret_togo;
		char aname[30];
		unsigned char *atomv = NULL;	/* Profile loaded from/to atom */
		unsigned char *buf;
		icmAlloc *al;

		atomv = NULL;

		strcpy(aname, "_ICC_PROFILE");

#if RANDR_MAJOR == 1 && RANDR_MINOR >= 2 && !defined(DISABLE_RANDR)
		/* If Xrandr 1.2, get property on output */
		if (p->icc_out_atom != 0) {

			/* Get the ICC profile property */
			if (XRRGetOutputProperty(p->mydisplay, p->output, p->icc_out_atom,
			            0, 0x7ffffff, False, False, XA_CARDINAL, 
   	                     &ret_type, &ret_format, &ret_len, &ret_togo, &atomv) != Success || ret_len == 0) {
				debugr("Failed to read ICC_PROFILE property from Xranr output\n");
			}

		}
#endif /* randr >= V 1.2 */

		if (atomv == NULL) {
			if (p->myuscreen != 0)
				sprintf(aname, "_ICC_PROFILE_%d",p->myuscreen);
	
			/* Get the ICC profile property */
			if (XGetWindowProperty(p->mydisplay, RootWindow(p->mydisplay, 0), p->icc_atom,
			            0, 0x7ffffff, False, XA_CARDINAL, 
   	                     &ret_type, &ret_format, &ret_len, &ret_togo, &atomv) != Success || ret_len == 0) {
				debugr2((errout,"Getting property '%s' from RootWindow\n", aname)); 
				return NULL;
			}
		}

		/* This is a bit of a fiddle to keep the memory allocations */
		/* straight. (We can't assume that X11 and icc are using the */
		/* same allocators) */
		if ((al = new_icmAllocStd()) == NULL) {
			debugr("new_icmAllocStd failed\n");
		    return NULL;
		}
		if ((buf = al->malloc(al, ret_len)) == NULL) {
			debugr("malloc of profile buffer failed\n");
		    return NULL;
		}
		memmove(buf, atomv, ret_len);
		XFree(atomv);

		/* Memory File fp that will free the buffer when deleted: */
		if ((rd_fp = new_icmFileMem_ad((void *)buf, ret_len, al)) == NULL) {
			debugr("Creating memory file from X11 atom failed");
			al->free(al, buf);
			al->del(al);
			return NULL;
		}

		if (name != NULL) {
			strncpy(name, aname, mxlen);
			name[mxlen] = '\000';
		}
		return rd_fp;
	}
#endif	  /* UNXI X11 */

	return NULL;
}

/* ----------------------------------------------- */

/* Restore the display state and free ramdacs */
static void restore_display(dispwin *p) {

	if (p->oor != NULL) {
		p->oor->del(p->oor);
		p->oor = NULL;
	}
	/* Restore the ramdac */
	if (p->or != NULL) {
		p->set_ramdac(p, p->or, 0);
		p->set_ramdac(p, p->or, 0);		/* Hmm. To be sure to be sure... */
		p->or->del(p->or);
		p->or = NULL;
		debugr("Restored original ramdac\n");
	}
	if (p->r != NULL) {
		p->r->del(p->r);
		p->r = NULL;
	}

#if defined(UNIX_X11)

#if ScreenSaverMajorVersion >= 1 && ScreenSaverMinorVersion >= 1	/* X11R7.1 */
	if (p->xsssuspend) {
		XScreenSaverSuspend(p->mydisplay, False);
		p->xsssuspend = 0;
	}
#endif
	if (p->xssvalid) {
		/* Restore the X11 screen saver state */
		XSetScreenSaver(p->mydisplay, p->timeout, p->interval,
	                p->prefer_blanking, p->allow_exposures);
	}

	/* Restore the xscreensaver */
	if (p->xssrunning) {
		system("xscreensaver -nosplash 2>/dev/null >/dev/null&");
	}

	if (p->gnomessrunning && p->gnomepid != -1) {
		kill(p->gnomepid, SIGKILL);		/* Kill the process inhibiting the screen saver */
	}

	/* Restore the KDE screen saver state */
	if (p->kdessrunning) {
		system("dcop kdesktop KScreensaverIface enable true 2>&1 >/dev/null");
	}

	/* Restore DPMS */
	if (p->dpmsenabled) {
		DPMSEnable(p->mydisplay);
	}

	/* Flush any changes out */
	XSync(p->mydisplay, False);
#endif /* UNIX_X11 */
}
	
/* ----------------------------------------------- */
/* On something killing our process, deal with Ramac & ScreenSaver cleanup */

#ifdef NT
void (__cdecl *dispwin_int)(int sig) = SIG_DFL;
void (__cdecl *dispwin_term)(int sig) = SIG_DFL;
#endif
#ifdef UNIX
void (*dispwin_hup)(int sig) = SIG_DFL;
void (*dispwin_int)(int sig) = SIG_DFL;
void (*dispwin_term)(int sig) = SIG_DFL;
#endif

/* List of dispwin's to clean up */
static dispwin *signal_dispwin = NULL;

static void dispwin_sighandler(int arg) {
	static amutex_static(lock);
	dispwin *pp, *np;

	/* Make sure we don't re-enter */
	if (amutex_trylock(lock)) {
		return;
	}

	/* Restore all dispwin's Ramdacs & screen savers */
	for (pp = signal_dispwin; pp != NULL; pp = np) {
	    np = pp->next;
		restore_display(pp);
	}

	/* Call through to previous handler */
#ifdef UNIX
	if (arg == SIGHUP && dispwin_hup != SIG_DFL && dispwin_hup != SIG_IGN) 
		dispwin_hup(arg);
#endif
	if (arg == SIGINT && dispwin_int != SIG_DFL && dispwin_int != SIG_IGN) 
		dispwin_int(arg);
	if (arg == SIGTERM && dispwin_term != SIG_DFL && dispwin_term != SIG_IGN) 
		dispwin_term(arg);

	amutex_unlock(lock);
	exit(0);
}

static void dispwin_install_signal_handlers(dispwin *p) {

	if (signal_dispwin == NULL) {
		/* Install the signal handler to ensure cleanup */
#ifdef UNIX
		dispwin_hup = signal(SIGHUP, dispwin_sighandler);
#endif /* UNIX */
		dispwin_int = signal(SIGINT, dispwin_sighandler);
		dispwin_term = signal(SIGTERM, dispwin_sighandler);
	}

	/* Add this one to the list */
	p->next = signal_dispwin;
	signal_dispwin = p;
}

static void dispwin_uninstall_signal_handlers(dispwin *p) {

	/* Find it and delete it from our static cleanup list */
	if (signal_dispwin != NULL) {
		if (signal_dispwin == p) {
			signal_dispwin = p->next;
			if (signal_dispwin == NULL) {
#if defined(UNIX)
				signal(SIGHUP, dispwin_hup);
#endif /* UNIX */
				signal(SIGINT, dispwin_int);
				signal(SIGTERM, dispwin_term);
			}
		} else {
			dispwin *pp;
			for (pp = signal_dispwin; pp != NULL; pp = pp->next) { 
				if (pp->next == p) {
					pp->next = p->next;
					break;
				}
			}
		}
	}
	p->next = NULL;
}

/* ----------------------------------------------- */
/* Test patch window specific declarations */

#ifdef __APPLE__

@class DWWin;
@class DWView;

/* Our OS X window specific information */
typedef struct {
	dispwin *p;
    DWWin *window;      		/* Our NSWindow */
    DWView *view;       		/* Our NSView */
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
	NSColorSpace *nscs;			/* Colorspace from profile */
#endif
} osx_cntx_t;

// - - - - - - - - - - - - - - - - - - - - - - - - - 


@interface DWView : NSView {
	osx_cntx_t *cntx;
}
- (void)setCntx:(osx_cntx_t *)cntx;
@end

@implementation DWView

- (void)setCntx:(osx_cntx_t *)val {
	cntx = val;
}

// A transparent 1x1 GIF: (43 bytes)
unsigned char emptyCursor[43] = {
	0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x21, 0xf9, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44, 0x01, 0x00, 0x3b };

/* Make cursor invisible over our window */
/*
 * This doesn't work very well. The only way to work it properly
 * is to create a CGEventTap and do a hide/unhide cursor when
 * the mouse enters the window. This needs the main thread 
 * to be dedicated to running the event loop, so would involve
 * some trickiness after main() in every program.
 */
- (void)resetCursorRects {
	[super resetCursorRects];
//	[self addCursorRect:[self bounds] cursor:[NSCursor crosshairCursor]];
	NSData *idata = [NSData dataWithBytes:(void *)emptyCursor length:43];
	NSImage *img = [NSImage alloc];
	img = [img initWithData:idata];
	NSCursor *curs = [NSCursor alloc];
	curs = [curs initWithImage:img hotSpot:NSMakePoint(0,0)];
	[self addCursorRect:[self bounds] cursor:curs];
}

- (void)drawRect:(NSRect)rect {
	osx_cntx_t *cx = cntx;
	dispwin *p = cx->p;
	NSRect frect;
	NSBezierPath* aPath = [NSBezierPath bezierPath];
	
	frect = NSMakeRect(p->tx, p->ty, (1.0 + p->tw), (1.0 + p->th));

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
	/* Use matching profile to (hopefully) trigger null color transform. */
	/* This doesn't work on < 10.6 though. */
	if (cx->nscs != NULL) {
		CGFloat rgb[4];
		NSInteger cnt = 4;
		rgb[0] = p->r_rgb[0];
		rgb[1] = p->r_rgb[1];
		rgb[2] = p->r_rgb[2];
		rgb[3] = 1.0;
		cnt = [ cx->nscs numberOfColorComponents ] + 1;
		[[NSColor colorWithColorSpace: cx->nscs components: rgb count: cnt] setFill];

	/* This works for < 10.6, but not for >= 10.6 on non-primary display */
	} else
#endif
	{
		[[NSColor colorWithDeviceRed: p->r_rgb[0]
   	                        green: p->r_rgb[1]
   	                         blue: p->r_rgb[2]
   	                        alpha: 1.0] setFill];
	}
	[aPath appendBezierPathWithRect:frect];
	[aPath fill];
}

@end

// - - - - - - - - - - - - - - - - - - - - - - - - - 

@interface DWWin : NSWindow {
	osx_cntx_t *cntx;
}
- (void)setCntx:(osx_cntx_t *)cntx;
@end

@implementation DWWin

- (void)setCntx:(osx_cntx_t *)val {
	cntx = val;
}

- (BOOL)canBecomeMainWindow {
	return NO;
}

/* So that we can change the cursor on a borderless window: */
- (BOOL)canBecomeKeyWindow {
    return YES;
}

- (BOOL)isMoveable {
	return NO;
}

- (BOOL)windowShouldClose:(id)sender {
//	printf("Got Window windowShouldClose\n");

//	[NSApp terminate: nil];
    return YES;
}

@end

/* Create our window */
static void create_my_win(NSRect rect, osx_cntx_t *cx) {
	dispwin *p = cx->p;
	SInt32 MacMajVers, MacMinVers, MacBFVers;
	void *cspr = NULL;			/* ColorSync profile ref. */
	int i;

	/* We need to locate the NSScreen that corresponds to the */
	/* CGDirectDisplayID - look through them for a match. */
	NSScreen *screen = nil;
	NSArray *screenArray = [NSScreen screens];
	for (i = 0; i < [screenArray count]; i++) {
		NSDictionary *screenDescription = [[screenArray objectAtIndex:i] deviceDescription];

		// CFShow(screenDescription);	// Dump it out
		/* Hmm. Also has "NSDeviceBitsPerSample" entry with value 8 in dict. */

	    NSNumber* screenID = [screenDescription objectForKey:@"NSScreenNumber"];
	    CGDirectDisplayID ddid = (CGDirectDisplayID)[screenID unsignedIntValue];			
		if (ddid == p->ddid) {
			screen = [screenArray objectAtIndex:i];
			break;
		}
	}

	/* Create Window */
	cx->window = [[DWWin alloc] initWithContentRect: rect
                                        styleMask: NSBorderlessWindowMask
                                          backing: NSBackingStoreBuffered
                                            defer: YES
	                                       screen: screen];

	[cx->window setLevel: NSScreenSaverWindowLevel];

	[cx->window setBackgroundColor: [NSColor blackColor]];

	[cx->window setTitle: @"Argyll Window"];

	/* Use our view for the whole window to draw plot */
	cx->view = [DWView new];
	[cx->view setCntx:(void *)cx];
	[cx->window setContentView: cx->view];

	[cx->window makeKeyAndOrderFront: nil];

	/* Use a null color transform to ensure device values */
	/* on non-primary display for 10.6+ */
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060

	/* Get the ColorSync profile for this display */
	if ((cspr = cur_colorsync_ref(p)) == NULL) {
		debugr2((errout,"cur_colorsync_ref failed\n"));

	} else {

#ifdef NEVER
		/* This is buggy on 10.6 (returns gray space). it does work on 10.4-10.5 & 10.7+ */
		cx->nscs = [[NSColorSpace alloc] initWithColorSyncProfile: cspr];
#else
		CGColorSpaceRef cgref;

		/* This works on 10.6+ */
		if ((cgref = CGColorSpaceCreateWithPlatformColorSpace(cspr)) == NULL) {
			debugr2((errout,"CGColorSpaceCreateWithPlatformColorSpace failed\n"));
		} else {
			/* 10.4 doesn't declare initWithCGColorSpace, but does implement it */
			cx->nscs = [[NSColorSpace alloc] initWithCGColorSpace: cgref];
		}
		CFRelease(cgref);
#endif
		if (cx->nscs == NULL) {
			debugr2((errout,"initWithColorSyncProfile failed\n"));
		} else {
			if ([ cx->nscs numberOfColorComponents ] != 3) {
				[cx->nscs release];
				cx->nscs = NULL;
			}
		}

		if (cspr != NULL) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
			CFRelease((ColorSyncProfileRef)cspr);
#else
			CMCloseProfile((CMProfileRef)cspr);
#endif
		}
	}
#endif /* >= 10.6 */

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
	/* >= 10.6+ device colors don't work on secondary display, need null transform. */
	/*  < 10.6 null transform doesn't work. */

	if (Gestalt(gestaltSystemVersionMajor,  &MacMajVers) == noErr
	 && Gestalt(gestaltSystemVersionMinor,  &MacMinVers) == noErr
	 && Gestalt(gestaltSystemVersionBugFix, &MacBFVers) == noErr
	 && MacMajVers >= 10 && MacMinVers >= 6
	 && cx->nscs == NULL) {
		warning("Unable to create null color transform - test colors may be wrong!");
	}
#endif

}

#endif /* __APPLE__ */

/* ----------------------------------------------- */

/* Change the window color. */
/* Return 1 on error, 2 on window being closed */
static int dispwin_set_color(
dispwin *p,
double r, double g, double b	/* Color values 0.0 - 1.0 */
) {
	int j;
	double orgb[3];		/* Previous RGB value */
//	double kr, kf;
	int update_delay = 0;

	debugr("dispwin_set_color called\n");

	if (p->nowin) {
		return 1;
	}

	orgb[0] = p->rgb[0]; p->rgb[0] = r;
	orgb[1] = p->rgb[1]; p->rgb[1] = g;
	orgb[2] = p->rgb[2]; p->rgb[2] = b;

//printf("\n set rgb %f %f %f\n",p->rgb[0], p->rgb[1], p->rgb[2]);

	for (j = 0; j < 3; j++) {
		if (p->rgb[j] < 0.0)
			p->rgb[j] = 0.0;
		else if (p->rgb[j] > 1.0)
			p->rgb[j] = 1.0;
		p->r_rgb[j] = p->s_rgb[j] = p->rgb[j];
		if (p->out_tvenc) {
			p->r_rgb[j] = p->s_rgb[j] = ((235.0 - 16.0) * p->s_rgb[j] + 16.0)/255.0;

			/* For video encoding the extra bits of precision are created by bit shifting */
			/* rather than scaling, so we need to scale the fp value to account for this. */
			if (p->pdepth > 8)
				p->r_rgb[j] = (p->s_rgb[j] * 255 * (1 << (p->pdepth - 8)))
				            /((1 << p->pdepth) - 1.0); 	
		}
	}

//if (p->out_tvenc) {
//printf(" %d: 8 bit tv = s_rgb %f %f %f\n",j, p->s_rgb[0], p->s_rgb[1], p->s_rgb[2]);
//printf(" %d: %d bitraster r_rgb %f %f %f\n",j, p->pdepth,p->r_rgb[0], p->r_rgb[1], p->r_rgb[2]);
//}

	/* Use ramdac for high precision native output. */
	/* The ramdac is used to hold the lsb that the frame buffer */
	/* doesn't hold. */
	if ((p->native & 1) == 1) {
		double prange = p->r->nent - 1.0;

		p->r->setlin(p->r);			/* In case something else altered this */

		for (j = 0; j < 3; j++) {
			int tt;
			double vv;

			vv = p->s_rgb[j];

			/* For video encoding the extra bits of precision are created by bit shifting */
			/* rather than scaling, so we need to scale the fp value to account for this. */
			if (p->out_tvenc && p->edepth > 8)
				vv = (vv * 255 * (1 << (p->edepth - 8)))/((1 << p->edepth) - 1.0); 	
				
			tt = (int)(vv * prange + 0.5);
			p->r_rgb[j] = (double)tt/prange;	/* RAMDAC output Quantized value */
			p->r->v[j][tt] = vv;

//printf(" cell[%d] = r_rgb %f, cell val %f\n",tt, p->r_rgb[j], vv);
		}
		if (p->set_ramdac(p, p->r, 0)) {
			debugr("set_ramdac() failed\n");
			return 1;
		}
	}

	/* - - - - - - - - - - - - - - */
#ifdef NT
	{
//		MSG msg;
//		INPUT fip;

		/* Stop the system going to sleep */

		/* This used to work OK in reseting the screen saver on */
		/* all versions of MSWin. */
		SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);

#ifdef NEVER
		/* (Some people use ES_CONTINUOUS | ES_AWAYMODE_REQUIRED as well ?? */
		/*  See also setting SPI_SETSCREENSAVEACTIVE? ) */

		/* Another approach is a fake mouse non-movementr. */
		SystemParametersInfo(SPI_SETBLOCKSENDINPUTRESETS, FALSE, NULL, 0);
		fip.type = INPUT_MOUSE;
		fip.mi.dx = 0;
		fip.mi.dy = 0;
		fip.mi.dwFlags = MOUSEEVENTF_MOVE;
		fip.mi.time = 0;
		fip.mi.dwExtraInfo = 0;
		SendInput(1, &fip, sizeof(INPUT));
#endif
		
		p->colupd++;
//printf("~1 set color %f %f %f\n", p->r_rgb[0], p->r_rgb[1], p->r_rgb[2]);

		/* Trigger a WM_PAINT */
		if (!InvalidateRect(p->hwnd, NULL, FALSE)) {
			debugr2((errout,"InvalidateRect failed, lasterr = %d\n",GetLastError()));
			return 1;
		}
		UpdateWindow(p->hwnd);

//printf("~1 waiting for paint\n");
		/* Wait for WM_PAINT to be executed */
		while (p->colupd != p->colupde && p->cberror == 0) {
			msec_sleep(10);
		}
//printf("~1 paint done\n");
	}
#endif /* NT */

	/* - - - - - - - - - - - - - - */

#ifdef __APPLE__

	if (p->winclose) {
		return 2;
	}

	/* If we may be in a different thread to the main thread or */
	/* the application thread, establish our own pool. */
	NSAutoreleasePool *tpool = nil;
	if (pthread_self() != ui_thid
	 && pthread_self() != ui_main_thid)
		tpool = [NSAutoreleasePool new];

	/* Stop the system going to sleep */
    UpdateSystemActivity(OverallAct);

	/* Make sure our window is brought to the front at least once, */
	/* but not every time, in case the user wants to kill the application. */
	if (p->btf == 0){
		OSStatus stat;
		ProcessSerialNumber cpsn;
		if ((stat = GetCurrentProcess(&cpsn)) != noErr) {
			debugr2((errout,"GetCurrentProcess returned error %d\n",stat));
		} else {
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1030
			if ((stat = TransformProcessType(&cpsn, kProcessTransformToForegroundApplication)) != noErr) {
				debugr2((errout,"TransformProcessType returned error %d\n",stat));
			}
#endif /* OS X 10.3 */
			if ((stat = SetFrontProcess(&cpsn)) != noErr) {
				debugr2((errout,"SetFrontProcess returned error %d\n",stat));
			}
		}
		p->btf = 1;
	}

	/* Trigger an update that fills window with r_rgb[] */
	[((osx_cntx_t *)(p->osx_cntx))->view setNeedsDisplay: YES ];

	if (tpool != nil)
		[tpool release];

#endif /* __APPLE__ */

	/* - - - - - - - - - - - - - - */

#if defined(UNIX_X11)
	{
		Colormap mycmap;
		XColor col;
		int vali[3];

		/* Indicate that we've got activity to the X11 Screensaver */
		XResetScreenSaver(p->mydisplay);

		/* Quantize to 16 bit color */
		for (j = 0; j < 3; j++)
			vali[j] = (int)(65535.0 * p->r_rgb[j] + 0.5);

		mycmap = DefaultColormap(p->mydisplay, p->myscreen);
		col.red = vali[0];
		col.green = vali[1];
		col.blue = vali[2];
		XAllocColor(p->mydisplay, mycmap, &col);
		XSetForeground(p->mydisplay, p->mygc, col.pixel);

		XFillRectangle(p->mydisplay, p->mywindow, p->mygc,
		               p->tx, p->ty, p->tw, p->th);

		XSync(p->mydisplay, False);		/* Make sure it happens */
	}
#endif	/* UNXI X11 */

	if (p->callout != NULL) {
		int rv;
		char *cmd;

		if ((cmd = malloc(strlen(p->callout) + 200)) == NULL)
			error("Malloc of command string failed");

		sprintf(cmd, "%s %d %d %d %f %f %f",p->callout,
			        (int)(r * 255.0 + 0.5),(int)(g * 255.0 + 0.5),(int)(b * 255.0 + 0.5), r, g, b);
		if ((rv = system(cmd)) != 0)
			warning("System command '%s' failed with %d",cmd,rv); 
		free(cmd);
	}

	update_delay = dispwin_compute_delay(p, orgb);

	/* Allow some time for the display & instrument to update */
	/* before  a measurement can take place. This allows for CRT refresh, */
	/* or LCD processing/update time, + display settling time (quite long for */
	/* smaller LCD changes), and any instrument reaction time. */
	debugr2((errout, "dispwin_set_color delaying %d msec\n",update_delay));
	msec_sleep(update_delay);

	if (p->cberror) {	/* Callback routine failed */
		return 1;
	}

	return 0;
}

/* ----------------------------------------------- */

/* Set a patch delay and instrument reaction time values. */
/* The overall delay between patch change and triggering */
/* the instrument is (patch_delay + display_settle - inst_reaction) */
/* and will never be less than the min_update_delay value. */
void dispwin_set_update_delay(dispwin *p, int patch_delay, int inst_reaction) {
	p->patch_delay = patch_delay;
	p->inst_reaction = inst_reaction;
}

/* Set/unset the blackground color flag */
/* Return nz on error */
static int dispwin_set_bg(dispwin *p, int blackbg) {
	return 1;		/* Need to re-create window */
}

/* Set the display settling time constants. Use -ve value to leave current value */
/* unchanged. (These values are used as part of the update delay calculations - see above). */
void dispwin_set_settling_delay(dispwin *p, double rise_time, double fall_time, double de_aim) {
	if (rise_time >= 0.0)
		p->rise_time = rise_time;
	if (fall_time >= 0.0)
		p->fall_time = fall_time;
	if (de_aim >= 0.0)
		p->de_aim = de_aim;
}

/* Enable or disable the update delay. This is used to disable the update delay */
/* when measuring the patch_delay and inst_reaction */
void dispwin_enable_update_delay(dispwin *p, int enable) {
	p->do_update_del = enable;
}

/* Return the update delay we should use (msec) */
int dispwin_compute_delay(dispwin *p, double *orgb) {
	int update_delay = 0, disp_settle = 0;

	if (p->do_update_del == 0) {
		return 0;
	}

	if (p->do_resp_time_del) {
		double xdelay;

		/* Compute am expected response time for the change in level, */
		/* to achieve 0.1 delta E */
		xdelay = disp_settle_time(orgb, p->rgb, p->rise_time * p->settle_mult,
		                                        p->fall_time * p->settle_mult, p->de_aim);
		disp_settle = (int)(xdelay * 1000.0 + 0.5);
	}

	update_delay = p->patch_delay + disp_settle - p->inst_reaction;

	/* Enforce minimum delay */
	if (update_delay < p->min_update_delay)
		update_delay = p->min_update_delay;

	if (p->ddebug) fprintf(stderr,"dispwin: update delay %d msec = patch_delay %d + disp_settle %d  - inst_reaction %d\n", update_delay, p->patch_delay, disp_settle, p->inst_reaction);

	return update_delay;
}

/* ----------------------------------------------- */
/* Set the shell set color callout */
void dispwin_set_callout(
dispwin *p,
char *callout
) {
	debugr2((errout,"dispwin_set_callout called with '%s'\n",callout));

	p->callout = strdup(callout);
}

/* ----------------------------------------------- */
/* Destroy ourselves */
void dispwin_del(
dispwin *p
) {

	debugr("dispwin_del called\n");

	if (p == NULL)
		return;

	/* Restore original RAMDAC if we were in native mode, */
	/* and restore screensaver */
	restore_display(p);
	dispwin_uninstall_signal_handlers(p);

	/* -------------------------------------------------- */
#ifdef NT

	if (p->hwnd != NULL) {

		p->quit = 1;
		if (PostMessage(p->hwnd, WM_CLOSE, (WPARAM)NULL, (LPARAM)NULL) != 0) {
			while(p->hwnd != NULL)
				msec_sleep(20);
		} else {
			debugr2((errout, "PostMessage(WM_GETICON failed, lasterr = %d\n",GetLastError()));
		}
//		DestroyCursor(p->curs); 

		if (p->mth != NULL) {			/* Message thread */
			p->mth->del(p->mth);
		}
	}

	if (p->hdc != NULL)
		DeleteDC(p->hdc);

#endif /* NT */
	/* -------------------------------------------------- */

	/* -------------------------------------------------- */
#ifdef __APPLE__
	if (p->nowin == 0) {	/* We have a window up */
		restore_display(p);
		if (p->osx_cntx != NULL) {	/* And we've allocated a context */
			osx_cntx_t *cx = (osx_cntx_t *)p->osx_cntx;

			p->winclose = 1;

			[cx->window release];

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1040
			if (cx->nscs != NULL)
				[cx->nscs release];
#endif

			free(p->osx_cntx);
			p->osx_cntx = NULL;
		}
	}

// ~~
//	CGDisplayShowCursor(p->ddid);

#endif /* __APPLE__ */
	/* -------------------------------------------------- */

	/* -------------------------------------------------- */
#if defined(UNIX_X11)
	debugr("About to close display\n");

	if (p->mydisplay != NULL) {
		if (p->nowin == 0) {	/* We have a window up */
	
			XFreeGC(p->mydisplay, p->mygc);
			XDestroyWindow(p->mydisplay, p->mywindow);
		}
		XCloseDisplay(p->mydisplay);
	}
	debugr("finished\n");

#endif	/* UNXI X11 */
	/* -------------------------------------------------- */

	if (p->name != NULL)
		free(p->name);
	if (p->description != NULL)
		free(p->description);
	if (p->callout != NULL)
		free(p->callout);

	free(p);
}

/* ----------------------------------------------- */
/* Event handler callbacks */

#ifdef NT

/* Undocumented flag. Set when minimized/maximized etc. */
#ifndef SWP_STATECHANGED
#define SWP_STATECHANGED 0x8000
#endif

/* MingW doesn't seem to have this, even though it's been there sine Win2k ... */
#ifndef ICM_DONE_OUTSIDEDC
# define ICM_DONE_OUTSIDEDC 4
#endif

static LRESULT CALLBACK MainWndProc(
	HWND hwnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam
) {
	debugrr2l(4, (stderr, "Handling message type 0x%x\n",message));

	if (message >= WM_APP) {
		debugrr2l(4, (stderr, "Message ignored\n"));
		return 0;
	}

	switch(message) {
		case WM_PAINT: {
			dispwin *p = NULL;
			int vali[3];
			HDC hdc;
			PAINTSTRUCT ps;
			RECT rect;
			HBRUSH hbr;
			int j;

#ifdef _WIN64
			if ((p = (dispwin *)GetWindowLongPtr(hwnd, GWLP_USERDATA)) == NULL)
#else
			if ((p = (dispwin *)GetWindowLong(hwnd, GWL_USERDATA)) == NULL)
#endif
			{
				debugrr2l(4,(stderr, "GetWindowLongPtr failed, lasterr = %d\n",GetLastError()));
				hdc = BeginPaint(hwnd, &ps);
				/* Don't know where to paint */
				EndPaint(hwnd, &ps);
				return 0;
			}

			/* Check that there is something to paint */
			if (GetUpdateRect(hwnd, NULL, FALSE) == 0) {
				debugrr2l(4, (stderr,"The update region was empty\n"));
			}

			/* Frame buffer Quantize 8 bit color */
			for (j = 0; j < 3; j++) {
				vali[j] = (int)(255.0 * p->r_rgb[j] + 0.5);
			}

			if ((hdc = BeginPaint(hwnd, &ps)) == NULL) {
				debugrr2l(4, (stderr,"BeginPaint failed\n"));
				EndPaint(hwnd, &ps);
				p->cberror = 2;
				return 0;
			}

			if (SaveDC(hdc) == 0) {
				debugrr2l(4, (stderr,"SaveDC failed\n"));
				EndPaint(hwnd, &ps);
				p->cberror = 3;
				return 0;
			}

			/* Try and turn ICM & WCS off */
			if (!SetICMMode(hdc, ICM_DONE_OUTSIDEDC)) {
				OSVERSIONINFO osver;
				osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
				osver.dwMajorVersion = 5;
				GetVersionEx(&osver);
				/* This seems to fail with "invalid handle" under NT4 */
				/* so only report an error if Win2K or more */
				if (osver.dwMajorVersion >= 5)
					printf("SetICMMode failed, lasterr = %d\n",GetLastError());
			}

			if ((hbr = CreateSolidBrush(RGB(vali[0],vali[1],vali[2]))) == NULL) {
				debugrr2l(4, (stderr,"CreateSolidBrush failed\n"));
				RestoreDC(hdc,-1);
				EndPaint(hwnd, &ps);
				p->cberror = 4;
				return 0;
			}
			if (SelectObject(hdc, hbr) == NULL
			 || SetRect(&rect, p->tx, p->ty, p->tx + p->tw, p->ty + p->th) == 0
			 || FillRect(hdc, &rect, hbr) == 0) {
				debugrr2l(4, (stderr,"SelectObject/SetRect/FillRect failed\n"));
				p->cberror = 5;
			}

			DeleteObject(hbr);
			RestoreDC(hdc,-1);
			EndPaint(hwnd, &ps);
			GdiFlush();

			p->colupde = p->colupd;		/* We're updated to this color */

			return 0;
		}

		/* Prevent any changes in position of the window */
		case WM_WINDOWPOSCHANGING: {
			WINDOWPOS *wpos = (WINDOWPOS *)lParam;
			debugrr2l(4,(stderr, "It's a windowposchange, flags = 0x%x, x,y %d %d, w,h %d %d\n",wpos->flags, wpos->x, wpos->y, wpos->cx, wpos->cy));
			wpos->flags &= ~SWP_FRAMECHANGED & ~SWP_NOREDRAW;
			wpos->flags |= SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER
			            |  SWP_NOSENDCHANGING | SWP_NOSIZE | SWP_SHOWWINDOW;
			debugrr2l(4,(stderr, "flags now = 0x%x\n",wpos->flags));
			return DefWindowProc(hwnd, message, wParam, lParam);
		}
		case WM_WINDOWPOSCHANGED: {
			WINDOWPOS *wpos = (WINDOWPOS *)lParam;
			debugrr2l(4,(stderr, "It's a windowposchanged, flags = 0x%x, x,y %d %d, w,h %d %d\n",wpos->flags, wpos->x, wpos->y, wpos->cx, wpos->cy));
			debugrr2l(4,(stderr, "It's a windowposchanged, flags = 0x%x\n",wpos->flags));
			return 0;
		}
		case WM_CLOSE: 
			DestroyWindow(hwnd);
			return 0; 
 
		case WM_DESTROY: 
		    PostQuitMessage(0); 
			return 0;
	}

	debugrr2l(4,(stderr, "Handle message using DefWindowProc()\n"));

	return DefWindowProc(hwnd, message, wParam, lParam);
}

#endif /* NT */

#if defined(UNIX_X11)
	/* None */
#endif	/* UNXI X11 */

/* ----------------------------------------------- */
#ifdef NT

/* Thread to handle message processing, so that there is no delay */
/* when the main thread is doing other things. */
int win_message_thread(void *pp) {
	dispwin *p = (dispwin *)pp;
	MSG msg;
	WNDCLASS wc;

	debugrr2l(4, (stderr, "win_message_thread started\n"));

	/* Fill in window class structure with parameters that describe the */
	/* main window. */
	wc.style         = 0 ;			/* Class style(s). */
	wc.lpfnWndProc   = MainWndProc;	/* Function to retrieve messages for windows of this class. */
	wc.cbClsExtra    = 0;			/* No per-class extra data. */
	wc.cbWndExtra    = 0;			/* No per-window extra data. */
	wc.hInstance     = NULL;		/* Application that owns the class. */
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_CROSS);
	wc.hbrBackground = GetStockObject(BLACK_BRUSH);		/* So full screen black works */
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = p->AppName;

	/* Make the cursor disapear over our window */
	/* (How does it know our window ??) */
	ShowCursor(FALSE);

	if ((p->arv = RegisterClass(&wc)) == 0) {
		debugr2((errout, "RegisterClass failed, lasterr = %d\n",GetLastError()));
		p->inited = 2;
		return 0;
	}

#ifndef WS_EX_NOACTIVATE
# define WS_EX_NOACTIVATE 0x08000000L
#endif
	p->hwnd = CreateWindowEx(
		WS_EX_NOACTIVATE | WS_EX_TOPMOST,
//				0,
		p->AppName,
		"Argyll Display Calibration Window",
		WS_VISIBLE | WS_DISABLED | WS_POPUP, 
//		WS_EX_TOPMOST
//		WS_EX_PALETTEWINDOW
//		WS_OVERLAPPEDWINDOW | WS_VISIBLE
//		WS_POPUPWINDOW | WS_VISIBLE,
		p->xo, p->yo,			/* Location */
		p->wi, p->he,			/* Size */
		NULL,					/* Handle to parent or owner */
		NULL,					/* Handle to menu or child window */
		NULL, 					/* hInstance Handle to appication instance */
		NULL);					/* pointer to window creation data */

	if (!p->hwnd) {
		debugr2((errout, "CreateWindow failed, lasterr = %d\n",GetLastError()));
		p->inited = 2;
		return 0;
	}

	/* Associate the dispwin object with the window, */
	/* so that the event callback can access it */
#ifdef _WIN64
	SetWindowLongPtr(p->hwnd, GWLP_USERDATA, (LONG_PTR)p);
#else
	SetWindowLong(p->hwnd, GWL_USERDATA, (LONG)p);
#endif

	/*
		Should we call BOOL SystemParametersInfo()
		to disable high contrast, powertimout and screensaver timeout ?

	 */

	debugrr2l(4, (stderr, "win_message_thread initialized - about to process messages\n"));
	p->inited = 1;

	for (;;) {
		if (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (p->quit != 0) {
				/* Process any pending messages */
				while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				break;
			}
		}
	}

	if (UnregisterClass(p->AppName, NULL) == 0) {
		warning("UnregisterClass failed, lasterr = %d\n",GetLastError());
	}

	p->hwnd = NULL;		/* Signal it's been deleted */

	return 0;
}

#endif /* NT */

/* Set the defauly update delay values */
void dispwin_set_default_delays(dispwin *p) {
	char *cp;

	p->min_update_delay = 20;

	if ((cp = getenv("ARGYLL_MIN_DISPLAY_UPDATE_DELAY_MS")) != NULL) {
		p->min_update_delay = atoi(cp);
		if (p->min_update_delay < 20)
			p->min_update_delay = 20;
		if (p->min_update_delay > 60000)
			p->min_update_delay = 60000;
		debugr2((errout, "new_dispwin: Minimum display update delay set to %d msec\n",p->min_update_delay));
	}

	p->settle_mult = 1.0;

	if ((cp = getenv("ARGYLL_DISPLAY_SETTLE_TIME_MULT")) != NULL) {
		p->settle_mult = atof(cp);
		if (p->settle_mult < 1e-6)
			p->settle_mult = 1e-6;
		if (p->settle_mult > 1e4)
			p->settle_mult = 1e4;
		debugr2((errout, "new_dispwin: Settling time multiplier %f\n",p->settle_mult));
	}

	p->patch_delay = PATCH_UPDATE_DELAY;			/* Default patch delay */
	p->inst_reaction = INSTRUMENT_REACTIONTIME;		/* Default inst delay */
	p->rise_time = DISPLAY_RISE_TIME;
	p->fall_time = DISPLAY_FALL_TIME;
	p->de_aim = DISPLAY_SETTLE_AIM; 

	p->do_resp_time_del = 1;		/* Default this to on */
	p->do_update_del = 1;			/* Default this to on */
}

/* Create a RAMDAC access and display test window, default grey */
dispwin *new_dispwin(
disppath *disp,					/* Display to calibrate. */
double width, double height,	/* Width and height in mm */
double hoff, double voff,		/* Offset from center in fraction of screen, range -1.0 .. 1.0 */
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
) {
	dispwin *p = NULL;

	debug("new_dispwin called\n");

#if defined(UNIX_X11) && defined(USE_UCMM)
	dispwin_checkfor_colord();		/* Make colord functions available */
	if (ddebug) {
		if (cd_found)
			fprintf(stderr,"using colord for profile installation\n");
		else
			fprintf(stderr,"using ucmm for profile installation\n");
	}
#endif

	if ((p = (dispwin *)calloc(sizeof(dispwin), 1)) == NULL) {
		if (ddebug) fprintf(stderr,"new_dispwin failed because malloc failed\n");
		return NULL;
	}

	/* !!!! Make changes in webwin.c as well !!!! */
	p->width = width;
	p->height = height;
	p->nowin = nowin;
	p->native = native;
	p->out_tvenc = out_tvenc;
	p->blackbg = blackbg;
	p->ddebug = ddebug;
	p->get_ramdac          = dispwin_get_ramdac;
	p->set_ramdac          = dispwin_set_ramdac;
	p->install_profile     = dispwin_install_profile;
	p->uninstall_profile   = dispwin_uninstall_profile;
	p->get_profile         = dispwin_get_profile;
	p->set_color           = dispwin_set_color;
	p->set_bg              = dispwin_set_bg;
	p->set_update_delay    = dispwin_set_update_delay;
	p->set_settling_delay  = dispwin_set_settling_delay;
	p->enable_update_delay = dispwin_enable_update_delay;
	p->set_callout         = dispwin_set_callout;
	p->del                 = dispwin_del;

	p->rgb[0] = p->rgb[1] = p->rgb[2] = 0.5;	/* Set Grey as the initial test color */

	dispwin_set_default_delays(p);

	/* Basic object is initialised, so create a window */

	/* -------------------------------------------------- */
#ifdef NT
	{
//		WNDCLASS wc;
		int disp_hsz, disp_vsz;		/* Display horizontal/vertical size in mm */
		int disp_hrz, disp_vrz;		/* Display horizontal/vertical resolution in pixels */
		int wi, he;					/* Width and height of window in pixels */
		int xo, yo;					/* Window location in pixels */
		int bpp;
		
		p->AppName = "Argyll Test Window";
		
		debugr2((errout, "new_dispwin: About to open display '%s'\n",disp->name));

		/* Get device context to main display */
		/* (This is the recommended way of doing this, and works on Vista) */
		if ((p->hdc = CreateDC(disp->name, NULL, NULL, NULL)) == NULL) {
			debugr2((errout, "new_dispwin: CreateDC failed, lasterr = %d\n",GetLastError()));
			dispwin_del(p);
			return NULL;
		}

		if ((p->name = strdup(disp->name)) == NULL) {
			debugr2((errout, "new_dispwin: Malloc failed\n"));
			dispwin_del(p);
			return NULL;
		}
		if ((p->description = strdup(disp->description)) == NULL) {
			debugr2((errout, "new_dispwin: Malloc failed\n"));
			dispwin_del(p);
			return NULL;
		}
		strcpy(p->monid, disp->monid);

		disp_hsz = GetDeviceCaps(p->hdc, HORZSIZE);	/* mm */
		disp_vsz = GetDeviceCaps(p->hdc, VERTSIZE);
		disp_hrz = GetDeviceCaps(p->hdc, HORZRES);	/* pixels */
		disp_vrz = GetDeviceCaps(p->hdc, VERTRES);

		wi = (int)(width * (double)disp_hrz/(double)disp_hsz + 0.5);
		if (wi > disp_hrz)
			wi = disp_hrz;
		he = (int)(height * (double)disp_vrz/(double)disp_vsz + 0.5);
		if (he > disp_vrz)
			he = disp_vrz;

		if (p->blackbg) {	/* Window fills the screen, test area is within it */
			p->tx = (int)((hoff * 0.5 + 0.5) * (disp->sw - wi) + 0.5);
			p->ty = (int)((voff * 0.5 + 0.5) * (disp->sh - he) + 0.5);
			p->tw = wi;
			p->th = he;
			wi = disp->sw;
			he = disp->sh;
			xo = disp->sx;
			yo = disp->sy;
		} else {			/* Test area completely fills the window */
			p->tx = 0;
			p->ty = 0;
			p->tw = wi;
			p->th = he;
			xo = disp->sx + (int)((hoff * 0.5 + 0.5) * (disp->sw - wi) + 0.5);
			yo = disp->sy + (int)((voff * 0.5 + 0.5) * (disp->sh - he) + 0.5);
		}
		p->ww = wi;
		p->wh = he;

		/* It's a bit difficult to know how windows defines the display */
		/* depth. Microsofts doco is fuzzy, and typical values */
		/* for BITSPIXEL and PLANES are confusing (What does "32" and "1" */
		/* mean ?) NUMCOLORS seems to be -1 on my box, and perhaps */
		/* is only applicable to up to 256 paletized colors. The doco */
		/* for COLORRES is also fuzzy, but it returns a meaningful number */
		/* on my box (24) */
		if (p->ddebug) {
			fprintf(errout,"Windows display RASTERCAPS 0x%x, BITSPIXEL %d, PLANES %d, NUMCOLORS %d, COLORRES %d\n",
			        GetDeviceCaps(p->hdc, RASTERCAPS),
			        GetDeviceCaps(p->hdc, BITSPIXEL),GetDeviceCaps(p->hdc, PLANES),
			        GetDeviceCaps(p->hdc, NUMCOLORS),GetDeviceCaps(p->hdc, COLORRES));
		}
		if (GetDeviceCaps(p->hdc, RASTERCAPS) & RC_PALETTE) {
			debugr2((errout, "new_dispwin: can't calibrate palette based device!\n"));
			dispwin_del(p);
			return NULL;
		}
		bpp = GetDeviceCaps(p->hdc, COLORRES);
		if (bpp <= 0)
			p->pdepth = 8;		/* Assume this is so */
		else
			p->pdepth = bpp/3;

		p->edepth = 16;

		if (nowin == 0) {

			/* We use a thread to process the window messages, so that */
			/* Task Manager doesn't think it's not responding. */

			/* Because messages only get delivered to same thread that created window, */
			/* the thread needs to create window. :-( */

			p->xo = xo;		/* Pass info to thread */
			p->yo = yo;
			p->wi = wi;
			p->he = he;

			if ((p->mth = new_athread(win_message_thread, (void *)p)) == NULL) {
				debugr2((errout, "new_dispwin: new_athread failed\n"));
				dispwin_del(p);
				return NULL;
			}

			/* Wait for thread to run */
			while (p->inited == 0) {
				msec_sleep(20);
			}
			/* If thread errored */
			if (p->inited != 1) {		/* Error */
				debugr2((errout, "new_dispwin: new_athread returned error\n"));
				dispwin_del(p);
				return NULL;
			}
		}

		/* Install the signal handler to ensure cleanup */
		dispwin_install_signal_handlers(p);
	}

#endif /* NT */
	/* -------------------------------------------------- */

	/* -------------------------------------------------- */
#ifdef __APPLE__

	if ((p->name = strdup(disp->name)) == NULL) {
		debugr2((errout,"new_dispwin: Malloc failed\n"));
		dispwin_del(p);
		return NULL;
	}
	p->ddid = disp->ddid;		/* Display we're working on */

	/* Hmm. Could we use CGDisplayGammaTableCapacity() instead ? */
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	{
		CGDisplayModeRef dispmode;
		CFStringRef pixenc;
		int cap = CGDisplayGammaTableCapacity(p->ddid);
		int fbdepth = 0;

		debugr2((errout,"new_dispwin: CGDisplayGammaTableCapacity = %d\n",cap));

		/* Compute GammaTable depth */ 
		{
			for (p->pdepth = 1; p->pdepth < 17; p->pdepth++) {
				if ((1 << p->pdepth) == cap)
					break;
			}
			if (p->pdepth >= 17) {
				debugr2((errout,"new_dispwin: failed to extract depth from GammaTableCapacity %d\n",cap));
				dispwin_del(p);
				return NULL;
			}
			debugr2((errout,"new_dispwin: found pixel depth %d bits\n",p->pdepth));
		}
		/* Get frame buffer depth for sanity check, but don't actually make used of it */

		dispmode = CGDisplayCopyDisplayMode(p->ddid);
		pixenc = CGDisplayModeCopyPixelEncoding(dispmode);

		/* Hmm. Don't know what to do with kIO16BitFloatPixels or kIO32BitFloatPixels */
		if (CFStringCompare(pixenc, CFSTR(kIO64BitDirectPixels), kCFCompareCaseInsensitive)
		                                                             == kCFCompareEqualTo)
			fbdepth = 16;
		else if (CFStringCompare(pixenc, CFSTR(kIO30BitDirectPixels), kCFCompareCaseInsensitive)
		                                                             == kCFCompareEqualTo)
			fbdepth = 10;
		else if (CFStringCompare(pixenc, CFSTR(IO32BitDirectPixels), kCFCompareCaseInsensitive)
		                                                             == kCFCompareEqualTo)
			fbdepth = 8;
		else if (CFStringCompare(pixenc, CFSTR(IO16BitDirectPixels), kCFCompareCaseInsensitive)
			                                                             == kCFCompareEqualTo)
			fbdepth = 5;
#ifndef DEBUG
		if (p->ddebug)
#endif
		{
			char buf[200];
			CFStringGetCString(pixenc, buf, 200, kCFStringEncodingUTF8);
			debugr2((errout,"new_dispwin: CGDisplayModePixelEncoding = '%s'\n",buf));
		}

		CFRelease(pixenc);
		CGDisplayModeRelease(dispmode);

		if (p->pdepth != fbdepth) {
			static int warned  = 0;
			if (!warned) {
				warning("new_dispwin: frame buffer depth %d != GammaTable depth %d\n",fbdepth, p->pdepth);
				warned = 1;
			}
		}
	}
#else
	p->pdepth = CGDisplayBitsPerSample(p->ddid);
#endif

	p->edepth = 16;				/* By experiment it seems to be 16 bits too */

	if (nowin == 0) {			/* Create a window */
		osx_cntx_t *cx;
		CGSize sz;				/* Display size in mm */
		int wi, he;				/* Width and height in pixels */
		int xo, yo;				/* Window location */
		NSRect wrect;

		debugr2((errout, "new_dispwin: About to open display '%s'\n",disp->name));

		/* If we may be in a different thread to the main thread or */
		/* the application thread, establish our own pool. */
		NSAutoreleasePool *tpool = nil;
		if (pthread_self() != ui_thid
		 && pthread_self() != ui_main_thid)
			tpool = [NSAutoreleasePool new];

		/* If there is no NSApp, then we haven't run main() in libui before */
		/* main() in the application. */
		if (NSApp == nil) {
			fprintf(stderr,"NSApp is nil - need to rename main() to main() and link with libui !\n");
			exit(1);
		}

		if ((cx = (osx_cntx_t *)calloc(sizeof(osx_cntx_t), 1)) == NULL) {
			if (tpool != nil)
				[tpool release];
			debugr2((errout,"new_dispwin: Malloc failed (osx_cntx_t)\n"));
			dispwin_del(p);
			return NULL;
		}
		cx->p = p;
		p->osx_cntx = cx;

		sz = CGDisplayScreenSize(p->ddid);
		debugr2((errout," Display size = %f x %f mm\n",sz.width,sz.height));

		wi = (int)(width * disp->sw/sz.width + 0.5);
		if (wi > disp->sw)
			wi = disp->sw;
		he = (int)(height * disp->sh/sz.height + 0.5);
		if (he > disp->sh)
			he = disp->sh;

		/* (Because Cocoa origin is botton left, we flip voff) */
		/* (Cocoa doesn't use disp->sx/sy either - each screen origin is at 0,0) */
		if (p->blackbg) {	/* Window fills the screen, test area is within it */
			p->tx = (int)((hoff * 0.5 + 0.5) * (disp->sw - wi) + 0.5);
			p->ty = (int)((-voff * 0.5 + 0.5) * (disp->sh - he) + 0.5);
			p->tw = wi;
			p->th = he;
			wi = disp->sw;
			he = disp->sh;
			xo = 0;
			yo = 0;
		} else {			/* Test area completely fills the window */
			p->tx = 0;
			p->ty = 0;
			p->tw = wi;
			p->th = he;
			xo = (int)((hoff * 0.5 + 0.5) * (disp->sw - wi) + 0.5);
			yo = (int)((-voff * 0.5 + 0.5) * (disp->sh - he) + 0.5);
		}
		p->ww = wi;
		p->wh = he;

		wrect.origin.x = xo;
		wrect.origin.y = yo;
		wrect.size.width = wi;
		wrect.size.height = he;

		create_my_win(wrect, cx);

		if (tpool != nil)
			[tpool release];

		p->winclose = 0;
	}

	/* Install the signal handler to ensure cleanup */
	dispwin_install_signal_handlers(p);
#endif /* __APPLE__ */
	/* -------------------------------------------------- */

	/* -------------------------------------------------- */
#if defined(UNIX_X11)
	{
		/* NOTE: That we're not doing much to detect if the display/window
		   we open is unsuitable for high quality color (ie. at least
		   24 bit etc.
		 */

#ifdef NEVER		// ?? is this for a specific reason ??
		if (signal_dispwin != NULL) {
			debugr2((errout,"new_dispwin: Attempting to open more than one dispwin!\n"));
			dispwin_del(p);
			return NULL;
		}
#endif

		/* stuff for X windows */
		char *pp, *bname;               /* base display name */
		Window rootwindow;
		char *appname = "TestWin";
		Visual *myvisual;
		XSetWindowAttributes myattr;
		XEvent      myevent;
		XTextProperty myappname;
		XSizeHints  mysizehints;
		XWMHints    mywmhints;
		int evb = 0, erb = 0;		/* Extension version */
		unsigned long myforeground,mybackground;
		int disp_hsz, disp_vsz;		/* Display horizontal/vertical size in mm */
		int disp_hrz, disp_vrz;		/* Display horizontal/vertical resolution in pixels (virtual screen) */
		int wi, he;				/* Width and height of window in pixels */
		int xo, yo;				/* Window location in pixels */
	
		/* Create the base display name (in case of Xinerama, XRandR) */
		if ((bname = strdup(disp->name)) == NULL) {
			debugr2((errout,"new_dispwin: Malloc failed\n"));
			dispwin_del(p);
			return NULL;
		}
		if ((pp = strrchr(bname, ':')) != NULL) {
			if ((pp = strchr(pp, '.')) != NULL) {
				sprintf(pp,".%d",disp->screen);
			}
		}

		/* open the display */
		p->mydisplay = XOpenDisplay(bname);
		if(!p->mydisplay) {
			debugr2((errout,"new_dispwin: Unable to open display '%s'\n",bname));
			dispwin_del(p);
			free(bname);
			return NULL;
		}
		free(bname);
		debugr("new_dispwin: Opened display OK\n");

		if ((p->name = strdup(disp->name)) == NULL) {
			debugr2((errout,"new_dispwin: Malloc failed\n"));
			dispwin_del(p);
			return NULL;
		}
		p->myscreen = disp->screen;
		p->myuscreen = disp->uscreen;
		p->myrscreen = disp->rscreen;

#if RANDR_MAJOR == 1 && RANDR_MINOR >= 2 && !defined(DISABLE_RANDR)
		/* These will be NULL otherwise */
		p->icc_atom = disp->icc_atom;
		p->crtc = disp->crtc;
		p->output = disp->output;
		p->icc_out_atom = disp->icc_out_atom;
#endif /* randr >= V 1.2 */

		if (disp->edid != NULL) {
			if ((p->edid = malloc(sizeof(unsigned char) * disp->edid_len)) == NULL) {
				debugr2((errout,"new_dispwin: Malloc failed\n"));
				dispwin_del(p);
				return NULL;
			}
			p->edid_len = disp->edid_len;
			memmove(p->edid, disp->edid, p->edid_len);
		}

		//p->pdepth = DefaultDepth(p->mydisplay, p->myscreen)/3;

		// Hmm. Should we explicitly get the root window visual,
		// since our test window inherits it from root ?
		myvisual = DefaultVisual(p->mydisplay, p->myscreen);
		p->pdepth = myvisual->bits_per_rgb;
		p->edepth = 16;

		if (nowin == 0) {			/* Create a window */
			rootwindow = RootWindow(p->mydisplay, p->myscreen);

			myforeground = BlackPixel(p->mydisplay, p->myscreen);
			mybackground = BlackPixel(p->mydisplay, p->myscreen);
		
			/* Get device context to main display */
			disp_hsz = DisplayWidthMM(p->mydisplay, p->myscreen);
			disp_vsz = DisplayHeightMM(p->mydisplay, p->myscreen);
			disp_hrz = DisplayWidth(p->mydisplay, p->myscreen);
			disp_vrz = DisplayHeight(p->mydisplay, p->myscreen);

			/* Compute width and offset from overal display in case Xinerama is active */
			wi = (int)(width * (double)disp_hrz/(double)disp_hsz + 0.5);
			if (wi > disp_hrz)
				wi = disp_hrz;
			he = (int)(height * (double)disp_vrz/(double)disp_vsz + 0.5);
			if (he > disp_vrz)
				he = disp_vrz;

			if (p->blackbg) {	/* Window fills the screen, test area is within it */
				p->tx = (int)((hoff * 0.5 + 0.5) * (disp->sw - wi) + 0.5);
				p->ty = (int)((voff * 0.5 + 0.5) * (disp->sh - he) + 0.5);
				p->tw = wi;
				p->th = he;
				wi = disp->sw;
				he = disp->sh;
				xo = disp->sx;
				yo = disp->sy;
			} else {			/* Test area completely fills the window */
				p->tx = 0;
				p->ty = 0;
				p->tw = wi;
				p->th = he;
				xo = disp->sx + (int)((hoff * 0.5 + 0.5) * (disp->sw - wi) + 0.5);
				yo = disp->sy + (int)((voff * 0.5 + 0.5) * (disp->sh - he) + 0.5);
			}
			p->ww = wi;
			p->wh = he;

			/* Setup Size Hints */
			mysizehints.flags = PPosition | USSize;
			mysizehints.x = xo;
			mysizehints.y = yo;
			mysizehints.width = wi;
			mysizehints.height = he;
		
			/* Setup Window Manager Hints */
			mywmhints.flags = InputHint | StateHint;
			mywmhints.input = 0;
			mywmhints.initial_state = NormalState;

			/* Setup Window Attributes */
			myattr.background_pixel = mybackground;
			myattr.bit_gravity = CenterGravity;
			myattr.win_gravity = CenterGravity;
			myattr.backing_store = WhenMapped;		/* Since we aren't listning to events */
			if (override)
				myattr.override_redirect = True;		/* Takes the WM out of the picture */
			else
				myattr.override_redirect = False;

			debugr("Opening window\n");
			p->mywindow = XCreateWindow(
				p->mydisplay, rootwindow,
				mysizehints.x,mysizehints.y,mysizehints.width,mysizehints.height,
				0,							/* Border width */
				CopyFromParent,				/* Depth */
				InputOutput,				/* Class */
				CopyFromParent,				/* Visual */
				CWBackPixel | CWBitGravity	/* Attributes Valumask */
				| CWWinGravity | CWBackingStore | CWOverrideRedirect,
				&myattr						/* Attribute details */
			);

#ifdef NEVER
			XWindowAttributes mywattributes;

			/* Get the windows attributes */
			if (XGetWindowAttributes(
				p->mydisplay, p->mywindow,
				&mywattributes) == 0) {
				debugr("new_dispwin: XGetWindowAttributes failed\n");
				dispwin_del(p);
				return NULL;
			}
			p->pdepth = mywattributes.depth/3;
#endif

			/* Setup TextProperty */
			XStringListToTextProperty(&appname, 1, &myappname);

			XSetWMProperties(
				p->mydisplay, p->mywindow,
				&myappname,				/* Window name */
				&myappname,				/* Icon name */
				NULL, 0,				/* argv, argc */
				&mysizehints,
				&mywmhints,
				NULL);					/* No class hints */
		
			/* Set aditional properties */
			{
				Atom optat;
				unsigned int opaque = 0xffffffff;
				unsigned int xid = (unsigned int)rootwindow;	/* Hope this is 32 bit */
				XChangeProperty(
					p->mydisplay, p->mywindow,
					XA_WM_TRANSIENT_FOR,		/* Property */
					XA_WINDOW,					/* Type */
					32,							/* format = bits in type of unsigned int */
					PropModeReplace,			/* Change mode */
					(char *)(&xid),				/* Data is Root Window XID */
					1							/* Number of elements of data */
				);

				/* Set hint for compositing WMs that the window must be opaque */
				if ((optat = XInternAtom(p->mydisplay, "_NET_WM_WINDOW_OPACITY", False)) != None) {
					XChangeProperty(p->mydisplay, p->mywindow, optat,
	           				     XA_CARDINAL, 32, PropModeReplace, (char *)(&opaque), 1);
				}
				if ((optat = XInternAtom(p->mydisplay, "_NET_WM_WINDOW_OPACITY_LOCKED", False)) != None) {
					XChangeProperty(p->mydisplay, p->mywindow, optat,
	           				     XA_CARDINAL, 32, PropModeReplace, (char *)(&opaque), 1);
				}
			}

			p->mygc = XCreateGC(p->mydisplay,p->mywindow,0,0);
			XSetBackground(p->mydisplay,p->mygc,mybackground);
			XSetForeground(p->mydisplay,p->mygc,myforeground);
			
			/* Create an invisible cursor over our window */
			{
				Cursor mycursor;
				Pixmap mypixmap;
				Colormap mycmap;
				XColor col;
				char pmdata[1] = { 0 };

				col.red = col.green = col.blue = 0;

				mycmap = DefaultColormap(p->mydisplay, p->myscreen);
				XAllocColor(p->mydisplay, mycmap, &col);
				mypixmap = XCreatePixmapFromBitmapData(p->mydisplay, p->mywindow, pmdata, 1, 1, 0, 0, 1); 
				mycursor = XCreatePixmapCursor(p->mydisplay, mypixmap, mypixmap, &col, &col, 0,0);
				XDefineCursor(p->mydisplay, p->mywindow, mycursor);
			}

			XSelectInput(p->mydisplay,p->mywindow, ExposureMask);
		
			XMapRaised(p->mydisplay,p->mywindow);
			debug("Raised window\n");
		
			/* ------------------------------------------------------- */
			/* Suspend any screensavers if we can */

			/* Install the signal handler to ensure cleanup */
			dispwin_install_signal_handlers(p);

#if ScreenSaverMajorVersion >= 1 && ScreenSaverMinorVersion >= 1	/* X11R7.1 ??? */

			/* Disable any screensavers that work properly with XScreenSaverSuspend() */
			if (XScreenSaverQueryExtension (p->mydisplay, &evb, &erb) != 0) {
				int majv, minv;
				XScreenSaverSuspend(p->mydisplay, True);
				p->xsssuspend = 1;

				/* Else we'd have to register as a screensaver to */
				/* prevent another one activating ?? */
			}
#endif	/* X11R7.1 screensaver extension */

			/* Disable the native X11 screensaver */
			if (p->xsssuspend == 0) {

				/* Save the screensaver state, and then disable it */
				XGetScreenSaver(p->mydisplay, &p->timeout, &p->interval,
				                &p->prefer_blanking, &p->allow_exposures);
				XSetScreenSaver(p->mydisplay, 0, 0, DefaultBlanking, DefaultExposures);
				p->xssvalid = 1;
			}

			/* Disable xscreensaver if it is running */
			if (p->xssrunning == 0) {
				p->xssrunning = (system("xscreensaver-command -version 2>/dev/null >/dev/null") == 0);
				if (p->xssrunning)
					system("xscreensaver-command -exit 2>/dev/null >/dev/null");
			}

			/* Disable gnomescreensaver if it is running */
			if (p->gnomessrunning == 0) {
				p->gnomessrunning = (system("gnome-screensaver-command -q "
				                     "2>/dev/null >/dev/null") == 0);
				if (p->gnomessrunning) {
					sigset_t nsm, osm;
					/* Ensure that other process doesn't get the signals we want to catch */
					sigemptyset(&nsm);
					sigaddset(&nsm,SIGHUP);
					sigaddset(&nsm,SIGINT);
					sigaddset(&nsm,SIGTERM);
					sigprocmask(SIG_BLOCK, &nsm, &osm);

					if ((p->gnomepid = fork()) == 0) {
						freopen("/dev/null", "r", stdin);
						freopen("/dev/null", "a", stdout);		/* Hide output */
						freopen("/dev/null", "a", stderr);
						execlp("gnome-screensaver-command", "gnome-screensaver-command","-i","-n","argyll","-r","measuring screen",NULL); 
 
						_exit(0);
					}
					sigprocmask(SIG_SETMASK, &osm, NULL);		/* restore the signals */
				}
			}
		
			/* kscreensaver > 3.5.9 obeys XResetScreenSaver(), but earlier versions don't. */
			/* Disable any KDE screen saver if it's active */
			if (p->kdessrunning == 0) {
				/* dcop is very slow if we're not actually running kde. */
				/* Check that kde is running first */
				if (system("ps -e 2>/dev/null | grep kdesktop 2>/dev/null >/dev/null") == 0) {
					p->kdessrunning = (system("dcop kdesktop KScreensaverIface isEnabled "
			                "2>/dev/null | grep true 2>/dev/null >/dev/null") == 0);
				}
				if (p->kdessrunning) {
					system("dcop kdesktop KScreensaverIface enable false 2>&1 >/dev/null");
				}
			}

			/* If DPMS is enabled, disable it */
			if (DPMSQueryExtension(p->mydisplay, &evb, &erb) != 0) {
				CARD16 power_level;
				BOOL state;

				if (DPMSInfo(p->mydisplay, &power_level, &state)) {
					if ((p->dpmsenabled = state) != 0)
						DPMSDisable(p->mydisplay);
				}
			}
		
			/* Deal with any pending events */
			debug("About to enter main loop\n");
			while(XPending(p->mydisplay) > 0) {
				XNextEvent(p->mydisplay, &myevent);
				switch(myevent.type) {
					case Expose:
						if(myevent.xexpose.count == 0) {	/* Repare the exposed region */
							debug("Servicing final expose\n");
							XFillRectangle(p->mydisplay, p->mywindow, p->mygc,
							               p->tx, p->ty, p->tw, p->th);
							debug("Finished expose\n");
						}
						break;
				}
			}
		} else {
			/* Install the signal handler to ensure cleanup */
			dispwin_install_signal_handlers(p);
		}
	}
#endif	/* UNIX X11 */
	/* -------------------------------------------------- */

	/* Save the original ramdac, which gets restored on exit */
	if ((p->or = p->get_ramdac(p)) != NULL) {

		if (noramdac != NULL)
			*noramdac = 0;

		debugr("Saved original VideoLUT\n");

		/* Copy original ramdac that never gets altered */
		if ((p->oor = p->or->clone(p->or)) == NULL) {
			dispwin_del(p);
			debugr("ramdac clone failed - memory ?\n");
			return NULL;
		}

		/* Create a working ramdac for native or other use */
		if ((p->r = p->or->clone(p->or)) == NULL) {
			dispwin_del(p);
			debugr("ramdac clone failed - memory ?\n");
			return NULL;
		}

	} else {
		debugr("Unable to access VideoLUT\n");
		if (noramdac != NULL)
			*noramdac = 1;
		p->native = native &= ~1;
		p->oor = p->or = p->r = NULL;
	}

	if (!p->nowin) {

		/* Make sure initial test color is displayed */
		dispwin_set_color(p, p->rgb[0], p->rgb[1], p->rgb[2]);

		/* Hmm. Could we add this ?? */
		/* Hard to know whether OS CM is active though. By default */
		/* dispwin disables it. */
		if (nocm != NULL)
			*nocm = 1;

		p->native = native &= ~2;
	}

	debugr("new_dispwin: return sucessfully\n");
	return p;
}

/* ================================================================ */
#if defined(UNIX_X11)
/* Process to continuously monitor XRandR events, */
/* and load the appropriate calibration and profiles */
/* for each monitor. */
int x11_daemon_mode(disppath *disp, int verb, int ddebug) {

#if RANDR_MAJOR == 1 && RANDR_MINOR >= 2 && !defined(DISABLE_RANDR)
	char *dname;
	char *pp;
	char dnbuf[100];
	Display *mydisplay;
	int majv, minv;			/* Version */
	int evb = 0, erb = 0;
	int dopoll = 1;				/* Until XRandR is fixed */
	XEvent myevent;
	int update_profiles = 1;	/* Do it on entry */ 

	/* Open the base display */
	strncpy(dnbuf,disp->name,99); dnbuf[99] = '\000';
	if ((pp = strrchr(dnbuf, ':')) != NULL) {
		if ((pp = strchr(pp, '.')) == NULL)
			strcat(dnbuf,".0");
		else  {
			if (pp[1] == '\000')
				strcat(dnbuf,"0");
			else {
				pp[1] = '0';
				pp[2] = '\000';
			}
		}
	}

	if ((mydisplay = XOpenDisplay(dnbuf)) == NULL) {
		debug2((errout, "x11_daemon_mode: failed to open display '%s'\n",dnbuf));
		return -1;
	}

	if (verb) printf("Opened display '%s'\n",dnbuf);

	/* !!!! we want to create a test here, to see if we have to poll, */
	/* !!!! or whether we spontainously get events when the EDID changes. */

	/* Use Xrandr 1.2 if it's available and not disabled */
	if (getenv("ARGYLL_IGNORE_XRANDR1_2") == NULL
	 && XRRQueryExtension(mydisplay, &evb, &erb) != 0
	 && XRRQueryVersion(mydisplay, &majv, &minv)
	 && majv == 1 && minv >= 2) {
		static void *xrr_found = NULL;	/* .so handle */
		static XRRScreenResources *(*_XRRGetScreenResourcesCurrent)
				                  (Display *dpy, Window window) = NULL;
		XRRScreenResources *scrnres;

		if (minv >= 3 && xrr_found == NULL) {
			if ((xrr_found = dlopen("libXrandr.so", RTLD_LAZY)) != NULL)
				_XRRGetScreenResourcesCurrent = dlsym(xrr_found, "XRRGetScreenResourcesCurrent");
		}

		if (verb) printf("Found XRandR 1.2 or latter\n");

		XRRSelectInput(mydisplay,RootWindow(mydisplay,0),
				RRScreenChangeNotifyMask
			|	RRCrtcChangeNotifyMask
			|	RROutputChangeNotifyMask
			|	RROutputPropertyNotifyMask
		);

		/* Deal with any pending events */
		if (verb) printf("About to enter main loop waiting for XRandR changes\n");
		for(;;) {

			if (update_profiles == 0) {
				if (dopoll) {
					for (;;) {
						if (minv >= 3 && _XRRGetScreenResourcesCurrent != NULL) { 
							_XRRGetScreenResourcesCurrent(mydisplay, RootWindow(mydisplay,0));
				
						} else {
							XRRGetScreenResources(mydisplay, RootWindow(mydisplay,0));
						}
						if(XPending(mydisplay) > 0)
							break;
						sleep(2);
					}
				} else {
					/* Sleep until there is an event */
					XPeekEvent(mydisplay, &myevent);
				}
			}
	
			/* Get all our events until we run out */
			while (XPending(mydisplay) > 0) {
				XNextEvent(mydisplay, &myevent);
				if (myevent.type == evb + RRScreenChangeNotify) {
//					printf("~1 Got RRScreenChangeNotify\n");
					update_profiles = 1; 
				} else if (myevent.type == evb + RRNotify) {
					update_profiles = 1; 
					XRRNotifyEvent *rrne = (XRRNotifyEvent *)(&myevent);
					if (rrne->subtype == RRNotify_CrtcChange) {
//						printf("~1 Got RRCrtcChangeNotify\n");
					}
					else if (rrne->subtype == RRNotify_OutputChange) {
//						printf("~1 Got RROutputChangeNotify\n");
					}
					else if (rrne->subtype == RRNotify_OutputProperty) {
//						printf("~1 Got RROutputPropertyNotify\n");
					}
				}
			}

			if (update_profiles) {
				disppath **dp;
				ramdac *r = NULL;

				if (verb) printf("Updating profiles for display '%s'\n",dnbuf);

				dp = get_displays();
				if (dp == NULL || dp[0] == NULL) {
					if (verb) printf("Failed to enumerate all the screens for display '%s'\n",dnbuf);
						continue;
				} else {
					int i, j;
					dispwin *dw;
					char calname[MAXNAMEL+1] = "\000";	/* Calibration file name */
					icmFile *rd_fp = NULL;
					icc *icco = NULL;
					icmVideoCardGamma *wo;
					double iv;

					for (i = 0; ; i++) {
						if (dp[i] == NULL)
							break;
						if (verb) printf("Updating display %d = '%s'\n",i+1,dp[i]->description);
		
						if ((dw = new_dispwin(dp[i], 0.0, 0.0, 0.0, 0.0, 1, 0, NULL, NULL, 0, 0, 0, ddebug)) == NULL) {
							if (verb) printf("Failed to access screen %d of display '%s'\n",i+1,dnbuf);
							continue;
						}
						if ((r = dw->get_ramdac(dw)) == NULL) {
							if (verb) printf("Failed to access VideoLUT of screen %d for display '%s'\n",i+1,dnbuf);
							dw->del(dw);
							continue;
						}

						/* Grab the installed profile from the ucmm */
						if ((rd_fp = dw->get_profile(dw, calname, MAXNAMEL)) == NULL) {
							if (verb) printf("Failed to find profile of screen %d for display '%s'\n",i+1,dnbuf);
							r->del(r);
							dw->del(dw);
							continue;
						}

						if ((icco = new_icc()) == NULL) {
							if (verb) printf("Failed to create profile object for screen %d for display '%s'\n",i+1,dnbuf);
							rd_fp->del(rd_fp);
							r->del(r);
							dw->del(dw);
							continue;
						}

						/* Read header etc. */
						if (icco->read(icco, rd_fp,0) != 0) {		/* Read ICC OK */
							if (verb) printf("Failed to read profile for screen %d for display '%s'\n",i+1,dnbuf);
							icco->del(icco);
							rd_fp->del(rd_fp);
							r->del(r);
							dw->del(dw);
							continue;
						}

						if ((wo = (icmVideoCardGamma *)icco->read_tag(icco, icSigVideoCardGammaTag)) == NULL) {
							if (verb) printf("Failed to find vcgt tagd in profile for screen %d for display '%s' so setting linear\n",i+1,dnbuf);
							for (j = 0; j < r->nent; j++) {
								iv = j/(r->nent-1.0);
								r->v[0][j] = iv;
								r->v[1][j] = iv;
								r->v[2][j] = iv;
							}
						} else {
							if (wo->u.table.channels == 3) {
								for (j = 0; j < r->nent; j++) {
									iv = j/(r->nent-1.0);
									r->v[0][j] = wo->lookup(wo, 0, iv);
									r->v[1][j] = wo->lookup(wo, 1, iv);
									r->v[2][j] = wo->lookup(wo, 2, iv);
								}
							} else if (wo->u.table.channels == 1) {
								for (j = 0; j < r->nent; j++) {
									iv = j/(r->nent-1.0);
									r->v[0][j] = 
									r->v[1][j] = 
									r->v[2][j] = wo->lookup(wo, 0, iv);
								}
								debug("Got monochrom vcgt calibration\n");
							} else {
								if (verb) printf("vcgt tag is unrecognized in profile for screen %d for display '%s'\n",i+1,dnbuf);
								icco->del(icco);
								rd_fp->del(rd_fp);
								r->del(r);
								dw->del(dw);
								continue;
							}
						}
						if (dw->set_ramdac(dw,r,1) != 0) {
							if (verb) printf("Unable to set vcgt tag for screen %d for display '%s'\n",i+1,dnbuf);
							icco->del(icco);
							rd_fp->del(rd_fp);
							r->del(r);
							dw->del(dw);
							continue;
						}
						if (verb) printf("Loaded profile and calibration for screen %d for display '%s'\n",i+1,dnbuf);
						icco->del(icco);
						rd_fp->del(rd_fp);
						r->del(r);
						dw->del(dw);
					}
				}
				free_disppaths(dp);
				update_profiles = 0;
			}
		}
	} else
#endif /* randr >= V 1.2 */

	if (verb) printf("XRandR 1.2 is not available - quitting\n");
	return -1;
}

#endif

/* ================================================================ */
#ifdef STANDALONE_TEST
/* test/utility code */

#if defined(__APPLE__) && defined(__POWERPC__)

/* Workaround for a ppc gcc 3.3 optimiser bug... */
/* It seems to cause a segmentation fault instead of */
/* converting an integer loop index into a float, */
/* when there are sufficient variables in play. */
static int gcc_bug_fix(int i) {
	static int nn;
	nn += i;
	return nn;
}
#endif	/* APPLE */

#include "numlib.h"

/* Flag = 0x0000 = default */
/* Flag & 0x0001 = list ChromCast's */
static void usage(int flag, char *diag, ...) {
	disppath **dp;
	fprintf(stderr,"Test display patch window, Set Video LUTs, Install profiles, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	if (diag != NULL) {
		va_list args;
		fprintf(stderr,"Diagnostic: ");
		va_start(args, diag);
		vfprintf(stderr, diag, args);
		va_end(args);
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"usage: dispwin [options] [calfile] \n");
	fprintf(stderr," -v                   Verbose mode\n");
#if defined(UNIX_X11)
	fprintf(stderr," -display displayname Choose X11 display name\n");
	fprintf(stderr," -d n[,m]             Choose the display n from the following list (default 1)\n");
	fprintf(stderr,"                      Optionally choose different display m for Video LUT access\n"); 
#else
	fprintf(stderr," -d n                 Choose the display from the following list (default 1)\n");
#endif
	dp = get_displays();
	if (dp == NULL || dp[0] == NULL) {
		fprintf(stderr,"    ** No displays found **\n");
	} else {
		int i;
		for (i = 0; ; i++) {
			if (dp[i] == NULL)
				break;
			fprintf(stderr,"    %d = '%s'\n",i+1,dp[i]->description);
		}
	}
	free_disppaths(dp);
	fprintf(stderr," -dweb[:port]         Display via web server at port (default 8080)\n");
	fprintf(stderr," -dcc[:n]             Display via n'th ChromeCast (default 1, ? for list)\n");
	if (flag & 0x001) {
		ccast_id **ids;
		if ((ids = get_ccids()) == NULL) {
			fprintf(stderr,"    ** Error discovering ChromCasts **\n");
		} else {
			if (ids[0] == NULL)
				fprintf(stderr,"    ** No ChromCasts found **\n");
			else {
				int i;
				for (i = 0; ids[i] != NULL; i++)
					fprintf(stderr,"    %d = '%s'\n",i+1,ids[i]->name);
				free_ccids(ids);
			}
		}
	}
#ifdef NT
	fprintf(stderr," -dmadvr              Display via MadVR Video Renderer\n");
#endif

	fprintf(stderr," -P ho,vo,ss[,vs]     Position test window and scale it\n");
	fprintf(stderr," -F                   Fill whole screen with black background\n");
	fprintf(stderr," -i                   Run forever with random values\n");
	fprintf(stderr," -G filename          Display RGB colors from CGATS (ie .ti1) file\n");
	fprintf(stderr," -C r.rr,g.gg,b.bb    Add this RGB color to list to be displayed\n");
	fprintf(stderr," -m                   Manually cycle through values\n");
	fprintf(stderr," -f                   Test grey ramp fade\n");
	fprintf(stderr," -r                   Test just Video LUT loading & Beeps\n");
	fprintf(stderr," -n                   Test native output (rather than through Video LUT and C.M.)\n");
	fprintf(stderr," -s filename          Save the currently loaded Video LUT to 'filename'\n");
	fprintf(stderr," -c                   Load a linear display calibration\n");
	fprintf(stderr," -V                   Verify that calfile/profile cal. is currently loaded in LUT\n");
	fprintf(stderr," -I                   Install profile for display and use it's calibration\n");
	fprintf(stderr," -U                   Un-install profile for display\n");
	fprintf(stderr," -S d                 Specify the install/uninstall scope for OS X [nlu] or X11/Vista [lu]\n");
	fprintf(stderr,"                      d is one of: n = network, l = local system, u = user (default)\n");
	fprintf(stderr," -L                   Load installed profiles cal. into Video LUT\n");
#if defined(UNIX_X11)
	fprintf(stderr," -X                   Run in daemon loader mode for given X11 server\n");
#endif /* X11 */
	fprintf(stderr," -D [level]           Print debug diagnostics to stderr\n");
	fprintf(stderr," calfile              Load calibration (.cal or %s) into Video LUT\n",ICC_FILE_EXT);
	exit(1);
}

/* 32 bit pseudo random sequencer based on XOR feedback */
/* generates number between 1 and 4294967295 */
#define PSRAND32(S) (((S) & 0x80000000) ? (((S) << 1) ^ 0xa398655d) : ((S) << 1))

int 
main(int argc, char *argv[]) {
	int fa, nfa, mfa;			/* current argument we're looking at */
	int verb = 0;				/* Verbose flag */
	int ddebug = 0;				/* debug level */
	int webdisp = 0;			/* NZ for web display, == port number */
	int ccdisp = 0;			 	/* NZ for ChromeCast, == list index */
#ifdef NT
	int madvrdisp = 0;			/* NZ for MadVR display */
#endif
	disppath *disp = NULL;		/* Display being used */
	double hpatscale = 1.0, vpatscale = 1.0;	/* scale factor for test patch size */
	double ho = 0.0, vo = 0.0;	/* Test window offsets, -1.0 to 1.0 */
	int out_tvenc = 0;			/* 1 to use RGB Video Level encoding */
	int blackbg = 0;       		/* NZ if whole screen should be filled with black */
	int nowin = 0;				/* Don't create test window */
	int ramd = 0;				/* Just test ramdac */
	int fade = 0;				/* Test greyramp fade */
	int native = 0;				/* X0 = use current per channel calibration curve */
								/* X1 = set native linear output and use ramdac high precn. */
								/* 0X = use current color management cLut (MadVR) */
								/* 1X = disable color management cLUT (MadVR) */
	int noramdac = 0;			/* nz if no ramdac access. native is set to X0 */
	int nocm = 0;				/* nz if no CM cLUT access. native is set to 0X */
	int inf = 0;				/* Infnite/manual patches flag */
	char pcname[MAXNAMEL+1] = "\000";	/* CGATS patch color name */
	int nmrgb = 0;				/* Number of manual RGB values */
	double mrgb[10][3];			/* Manual RGB values */
	int clear = 0;				/* Clear any display calibration (any calname is ignored) */
	char sname[MAXNAMEL+1] = "\000";	/* Current cal save name */
	int verify = 0;				/* Verify that calname is currently loaded */
	int installprofile = 0;		/* Install (1) or uninstall (2) a profile for display */
	int loadprofile = 0;		/* Load displays profile calibration into LUT */
	int loadfile = 0;			/* Load given profile into LUT */
	p_scope scope = p_scope_user;	/* Scope of profile instalation/un-instalation */
	int daemonmode = 0;			/* X11 daemin loader mode */
	char calname[MAXNAMEL+1] = "\000";	/* Calibration file name */
	dispwin *dw;
	unsigned int seed = 0x56781234;
	int i, j;
//	ramdac *r = NULL;
	int is_ok_icc = 0;			/* The profile is OK */

	error_program = "Dispwin";
	check_if_not_interactive();

	/* Process the arguments */
	mfa = 0;        /* Minimum final arguments */
	for(fa = 1;fa < argc;fa++) {

		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-') {	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1+mfa) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage(0,"Usage requested");

			else if (argv[fa][1] == 'v')
				verb = 1;

			/* Debug */
			else if (argv[fa][1] == 'D') {
				ddebug = 1;
				if (na != NULL && na[0] >= '0' && na[0] <= '9') {
					ddebug = atoi(na);
					fa = nfa;
				}
				g_log->debug = ddebug;
				callback_ddebug = ddebug;	/* dispwin global */
			}

			/* Display number */
			else if (argv[fa][1] == 'd') {
				if (strncmp(na,"web",3) == 0
				 || strncmp(na,"WEB",3) == 0) {
					webdisp = 8080;
					if (na[3] == ':') {
						webdisp = atoi(na+4);
						if (webdisp == 0 || webdisp > 65535)
							usage(0,"Web port number must be in range 1..65535");
					}
					fa = nfa;
				} else if (strncmp(na,"cc",2) == 0
				 || strncmp(na,"CC",2) == 0) {
					ccdisp = 1;
					if (na[2] == ':') {
						if (na[3] < '0' || na[3] > '9')
							usage(0x0001,"Available ChromeCasts");

						ccdisp = atoi(na+3);
						if (ccdisp <= 0)
							usage(0,"ChromCast number must be in range 1..N");
					}
					fa = nfa;
#ifdef NT
				} else if (strncmp(na,"madvr",5) == 0
				 || strncmp(na,"MADVR",5) == 0) {
					madvrdisp = 1;
					fa = nfa;
#endif
				} else {
#if defined(UNIX_X11)
					int ix, iv;

					/* X11 type display name. */
					if (strcmp(&argv[fa][2], "isplay") == 0 || strcmp(&argv[fa][2], "ISPLAY") == 0) {
						if (++fa >= argc || argv[fa][0] == '-') usage(0,"-DISPLAY parameter missing");
						setenv("DISPLAY", argv[fa], 1);
					} else {
						if (na == NULL) usage(0,"-d parameter missing");
						fa = nfa;
						if (sscanf(na, "%d,%d",&ix,&iv) != 2) {
							ix = atoi(na);
							iv = 0;
						}
						if (disp != NULL)
							free_a_disppath(disp);
						if ((disp = get_a_display(ix-1)) == NULL)
							usage(0,"-d parameter '%s' is out of range",na);
						if (iv > 0)
							disp->rscreen = iv-1;
					}
#else
					int ix;
					if (na == NULL) usage(0,"-d parameter is missing");
					fa = nfa;
					ix = atoi(na);
					if (disp != NULL)
						free_a_disppath(disp);
					if ((disp = get_a_display(ix-1)) == NULL)
						usage(0,"-d parameter '%s' is out of range",na);
#endif
				}
			}

			/* Test patch offset and size */
			else if (argv[fa][1] == 'P') {
				fa = nfa;
				if (na == NULL) usage(0,"-p parameters are missing");
				if (sscanf(na, " %lf,%lf,%lf,%lf ", &ho, &vo, &hpatscale, &vpatscale) == 4) {
					;
				} else if (sscanf(na, " %lf,%lf,%lf ", &ho, &vo, &hpatscale) == 3) {
					vpatscale = hpatscale;
				} else {
					usage(0,"-p parameters '%s' is badly formatted",na);
				}
				if (ho < 0.0 || ho > 1.0
				 || vo < 0.0 || vo > 1.0
				 || hpatscale <= 0.0 || hpatscale > 50.0
				 || vpatscale <= 0.0 || vpatscale > 50.0)
					usage(0,"-p parameters '%s' is out of range",na);
				ho = 2.0 * ho - 1.0;
				vo = 2.0 * vo - 1.0;

			/* Black background */
			} else if (argv[fa][1] == 'F') {
				blackbg = 1;

			/* Video mode encoding */
			} else if (argv[fa][1] == 'E') {
				out_tvenc = 1;

			} else if (argv[fa][1] == 'i')
				inf = 1;

			else if (argv[fa][1] == 'm')
				inf = 2;

			/* CGATS patch color file */
			else if (argv[fa][1] == 'G') {
				fa = nfa;
				if (na == NULL) usage(0,"-G parameter is missing");
				strncpy(pcname,na,MAXNAMEL); pcname[MAXNAMEL] = '\000';
			}
			/* Manual color */
			else if (argv[fa][1] == 'C') {
				fa = nfa;
				if (nmrgb >= 10)
					usage(0,"Can only be up to 10 -C values");
				if (na == NULL) usage(0,"-C parameters are missing");
				if (sscanf(na, "%lf,%lf,%lf ",&mrgb[nmrgb][0],&mrgb[nmrgb][1],&mrgb[nmrgb][2]) != 3)
					usage(0,"-C parameters '%s' are badly formatted",na);
				if (mrgb[nmrgb][0] < 0.0 || mrgb[nmrgb][0] > 1.0
				 || mrgb[nmrgb][1] < 0.0 || mrgb[nmrgb][1] > 1.0
				 || mrgb[nmrgb][2] < 0.0 || mrgb[nmrgb][2] > 1.0)
					usage(0,"-C parameters %f %f %f are out of range 0.0 - 1.0",mrgb[nmrgb][0],mrgb[nmrgb][1],mrgb[nmrgb][2]);
				nmrgb++;
			}
			else if (argv[fa][1] == 'f')
				fade = 1;

			else if (argv[fa][1] == 'r')
				ramd = 1;

			else if (argv[fa][1] == 'n') {
				native = 3;			/* Disable cal & any CM */
			}

			else if (argv[fa][1] == 's') {
				fa = nfa;
				if (na == NULL) usage(0,"-s parameter is missing");
				strncpy(sname,na,MAXNAMEL); sname[MAXNAMEL] = '\000';
			}

			else if (argv[fa][1] == 'c' || argv[fa][1] == 'C')
				clear = 1;

			else if (argv[fa][1] == 'V')
				verify = 1;

			else if (argv[fa][1] == 'I')
				installprofile = 1;

			else if (argv[fa][1] == 'U')
				installprofile = 2;

			else if (argv[fa][1] == 'L')
				loadprofile = 1;

			else if (argv[fa][1] == 'X')
				daemonmode = 1;

			else if (argv[fa][1] == 'S') {
				fa = nfa;
				if (na == NULL) usage(0,"-S parameter is missing");
					if (na[0] == 'n' || na[0] == 'N')
						scope = p_scope_network;
					else if (na[0] == 'l' || na[0] == 'L')
						scope = p_scope_local;
					else if (na[0] == 'u' || na[0] == 'U')
						scope = p_scope_user;
			}
			else
				usage(0,"Unknown flag '%s'",argv[fa]);
		}
		else
			break;
	}

	/* No explicit display has been set */
	if (disp == NULL
#ifdef NT
	 && madvrdisp == 0
#endif
	 && webdisp == 0
	 && ccdisp == 0) {
		int ix = 0;
#if defined(UNIX_X11)
		char *dn, *pp;

		if ((dn = getenv("DISPLAY")) != NULL) {
			if ((pp = strrchr(dn, ':')) != NULL) {
				if ((pp = strchr(pp, '.')) != NULL) {
					if (pp[1] != '\000')
						ix = atoi(pp+1);
				}
			}
		}
#endif
		if ((disp = get_a_display(ix)) == NULL)
			error("Unable to open the default display");
	}

	/* See if there's a calibration file */
	if (fa < argc) {
		strncpy(calname,argv[fa++],MAXNAMEL); calname[MAXNAMEL] = '\000';
		if (installprofile == 0 && loadprofile == 0 && verify == 0)
			loadfile = 1;		/* Load the given profile into the videoLUT */
	}

#if defined(UNIX_X11)
	if (webdisp == 0 && ccdisp == 0 && daemonmode) {
		return x11_daemon_mode(disp, verb, ddebug);
	}
#endif

	/* Bomb on bad combinations (not all are being detected) */
	if (installprofile && calname[0] == '\000')
		error("Can't install or uninstall a displays profile without profile argument");

	if (verify && calname[0] == '\000' && loadprofile == 0)
		error("No calibration/profile provided to verify against");

	if (verify && installprofile == 1)
		error("Can't verify and install a displays profile at the same time");

	if (verify && installprofile == 2)
		error("Can't verify and uninstall a displays profile at the same time");

	/* Don't create a window if it won't be used */
	if (ramd != 0 || sname[0] != '\000' || clear != 0 || verify != 0 || loadfile != 0 || installprofile != 0 || loadprofile != 0)
		nowin = 1;

	if (webdisp != 0) {
		if ((dw = new_webwin(webdisp, 100.0 * hpatscale, 100.0 * vpatscale, ho, vo, nowin, native,
			                        &noramdac, &nocm, out_tvenc, blackbg, verb, ddebug)) == NULL) {
			printf("Error - new_webwin failed!\n");
			return -1;
		}

	} else if (ccdisp != 0) {
		ccast_id **ids;
		if ((ids = get_ccids()) == NULL) {
			printf("Error - discovering ChromCasts failed\n");
			return -1;
		}
		if (ids[0] == NULL) {
			printf("Error - there are no ChromCasts to use\n");
			return -1;
		}
		for (i = 0; ids[i] != NULL; i++)
			;
		if (ccdisp < 1 || ccdisp > i) {
			printf("Error - chosen ChromCasts (%d) is outside list (1..%d)\n",ccdisp,i);
			return -1;
		}
		
		if ((dw = new_ccwin(ids[ccdisp-1], 100.0 * hpatscale, 100.0 * vpatscale,
			                   ho, vo, nowin, native, &noramdac, &nocm, out_tvenc,
			                   blackbg, verb, ddebug)) == NULL) {
			printf("Error - new_ccwin failed!\n");
			free_ccids(ids);
			return -1;
		}
		free_ccids(ids);

#ifdef NT
	} else if (madvrdisp != 0) {
		if (out_tvenc) {
			printf("Error - Set TV encodfing in MadVR\n");
			return -1;
		}
			
		if ((dw = new_madvrwin(100.0 * hpatscale, 100.0 * vpatscale, ho, vo, nowin, native,
			                   &noramdac, &nocm, out_tvenc, blackbg, verb, ddebug)) == NULL) {
			printf("Error - new_madvrwin failed! (Is it running and up to date?)\n");
			return -1;
		}
#endif
	} else {
		if (verb) printf("About to open dispwin object on the display\n");
		if ((dw = new_dispwin(disp, 100.0 * hpatscale, 100.0 * vpatscale, ho, vo, nowin, native,
			                         &noramdac, &nocm, out_tvenc, blackbg, 1, ddebug)) == NULL) {
			printf("Error - new_dispwin failed!\n");
			return -1;
		}
	}

	if ((native & 1) != 0 && noramdac) {
		warning("Unable to access to VideoLUTs so can't be sure colors are native");
	} 

	/* Save the current Video LUT to the calfile */
	if (sname[0] != '\000') {
		cgats *ocg;			/* output cgats structure */
		time_t clk = time(0);
		struct tm *tsp = localtime(&clk);
		char *atm = asctime(tsp);	/* Ascii time */
		cgats_set_elem *setel;		/* Array of set value elements */
		int nsetel = 0;
		
		if (verb)
			printf("About to save current loaded calibration to file '%s'\n",sname);

		if (dw->oor == NULL) {
			error("We don't have access to the VideoLUT");
		}

		ocg = new_cgats();				/* Create a CGATS structure */
		ocg->add_other(ocg, "CAL"); 	/* our special type is Calibration file */

		ocg->add_table(ocg, tt_other, 0);	/* Add a table for RAMDAC values */
		ocg->add_kword(ocg, 0, "DESCRIPTOR", "Argyll Device Calibration Curves",NULL);
		ocg->add_kword(ocg, 0, "ORIGINATOR", "Argyll synthcal", NULL);
		atm[strlen(atm)-1] = '\000';	/* Remove \n from end */
		ocg->add_kword(ocg, 0, "CREATED",atm, NULL);

		ocg->add_kword(ocg, 0, "DEVICE_CLASS","DISPLAY", NULL);
		ocg->add_kword(ocg, 0, "COLOR_REP", "RGB", NULL);

		ocg->add_field(ocg, 0, "RGB_I", r_t);
		ocg->add_field(ocg, 0, "RGB_R", r_t);
		ocg->add_field(ocg, 0, "RGB_G", r_t);
		ocg->add_field(ocg, 0, "RGB_B", r_t);

		if ((setel = (cgats_set_elem *)malloc(sizeof(cgats_set_elem) * 4)) == NULL)
			error("Malloc failed!");

		/* Write the video lut curve values */
		for (i = 0; i < dw->oor->nent; i++) {
			double iv = i/(dw->oor->nent-1.0);

#if defined(__APPLE__) && defined(__POWERPC__)
			gcc_bug_fix(i);
#endif
			setel[0].d = iv;
			setel[1].d = dw->oor->v[0][i];
			setel[2].d = dw->oor->v[1][i];
			setel[3].d = dw->oor->v[2][i];

			ocg->add_setarr(ocg, 0, setel);
		}

		free(setel);

		if (ocg->write_name(ocg, sname))
			error("Write error to '%s' : %s",sname,ocg->err);

		ocg->del(ocg);		/* Clean up */

		/* Fall through, as we may want to do other stuff too */
	}

	/* Clear the display calibration curve */
	if (clear != 0) {
		int rv;
		
		if (dw->or == NULL)
			error("We don't have access to the VideoLUT for clearing");

		for (i = 0; i < dw->or->nent; i++) {
			double iv = i/(dw->or->nent-1.0);
			dw->or->v[0][i] = iv;
			dw->or->v[1][i] = iv;
			dw->or->v[2][i] = iv;
		}
		if (verb)
			printf("About to clear the calibration\n");
		if ((rv = dw->set_ramdac(dw,dw->or,1)) != 0) {
			if (rv == 2)
				warning("Failed to set VideoLUTs persistently because current System Profile can't be renamed");
			else
				error("Failed to set VideoLUTs");
		}

		/* Fall through, as we may want to do other stuff too */
	}

	/* Un-Install the profile from the display */
	if (installprofile == 2) {
		int rv;
		if ((rv = dw->uninstall_profile(dw, calname, scope))) {
			if (rv == 2)
				warning("Profile '%s' not found to uninstall!",calname);
			else
				error("Error trying to uninstall profile '%s'!",calname);
		}
		if (verb) {
			printf("Un-Installed '%s'\n",calname);
		}
	}

	/* Get any calibration from the provided .cal file or .profile, */
	/* or calibration from the current default display profile, */
	/* and put it in dw->or */
	if (loadfile != 0 || verify != 0 || loadprofile != 0 || installprofile == 1) {
		icmFile *rd_fp = NULL;
		icc *icco = NULL;
		cgats *ccg = NULL;			/* calibration cgats structure */
		
		if (dw->r == NULL)
			error("We don't have access to the VideoLUT for loading");

		/* Should we load calfile instead of installed profile if it's present ??? */
		if (loadprofile) {
			if (calname[0] != '\000')
				warning("Profile '%s' provided as argument is being ignored!\n",calname);
				
			/* Get the current displays profile */
			debug2((errout,"Loading calibration from display profile '%s'\n",dw->name));
			if ((rd_fp = dw->get_profile(dw, calname, MAXNAMEL)) == NULL)
				error("Failed to get the displays current ICC profile\n");

		} else {
			/* Open up the profile for reading */
			debug2((errout,"Loading calibration from file '%s'\n",calname));
			if ((rd_fp = new_icmFileStd_name(calname,"r")) == NULL)
				error("Can't open file '%s'",calname);
		}

		if ((icco = new_icc()) == NULL)
			error("Creation of ICC object failed");

		/* Read header etc. */
		if (icco->read(icco, rd_fp,0) == 0) {		/* Read ICC OK */
			icmVideoCardGamma *wo;
			double iv;

			is_ok_icc = 1;			/* The profile is OK */

			if ((wo = (icmVideoCardGamma *)icco->read_tag(icco, icSigVideoCardGammaTag)) == NULL) {
				warning("No vcgt tag found in profile - assuming linear\n");
				for (i = 0; i < dw->r->nent; i++) {
					iv = i/(dw->r->nent-1.0);
					dw->r->v[0][i] = iv;
					dw->r->v[1][i] = iv;
					dw->r->v[2][i] = iv;
				}
			} else {
				
				if (wo->u.table.channels == 3) {
					for (i = 0; i < dw->r->nent; i++) {
						iv = i/(dw->r->nent-1.0);
						dw->r->v[0][i] = wo->lookup(wo, 0, iv);
						dw->r->v[1][i] = wo->lookup(wo, 1, iv);
						dw->r->v[2][i] = wo->lookup(wo, 2, iv);
//printf("~1 entry %d = %f %f %f\n",i,dw->r->v[0][i],dw->r->v[1][i],dw->r->v[2][i]);
					}
					debug("Got color vcgt calibration\n");
				} else if (wo->u.table.channels == 1) {
					for (i = 0; i < dw->r->nent; i++) {
						iv = i/(dw->r->nent-1.0);
						dw->r->v[0][i] = 
						dw->r->v[1][i] = 
						dw->r->v[2][i] = wo->lookup(wo, 0, iv);
					}
					debug("Got monochrom vcgt calibration\n");
				}
				/* ~~~ Ideally the vcgt should have been tagged if it is TV encoded, so */
				/* that the scaling can be adjusted here if the RAMDAC depth differs from */
				/* the vcgt depth. ~~~ */
			}
		} else {	/* See if it's a .cal file */
			int ncal;
			int ii, fi, ri, gi, bi;
			double cal[3][256];
			int out_tvenc = 0;			/* nz to use (16-235)/255 video encoding */
			
			icco->del(icco);			/* Don't need these now */
			icco = NULL;
			rd_fp->del(rd_fp);
			rd_fp = NULL;

			ccg = new_cgats();			/* Create a CGATS structure */
			ccg->add_other(ccg, "CAL"); /* our special calibration type */
		
			if (ccg->read_name(ccg, calname)) {
				ccg->del(ccg);
				error("File '%s' is not a valid ICC profile or Argyll .cal file",calname);
			}
		
			if (ccg->ntables == 0 || ccg->t[0].tt != tt_other || ccg->t[0].oi != 0)
				error("Calibration file isn't a CAL format file");
			if (ccg->ntables < 1)
				error("Calibration file '%s' doesn't contain at least one table",calname);
		
			if ((ncal = ccg->t[0].nsets) <= 0)
				error("No data in set of file '%s'",calname);
		
			if (ncal != 256)
				error("Expect 256 data sets in file '%s'",calname);
		
			if ((fi = ccg->find_kword(ccg, 0, "DEVICE_CLASS")) < 0)
				error("Calibration file '%s' doesn't contain keyword COLOR_REP",calname);
			if (strcmp(ccg->t[0].kdata[fi],"DISPLAY") != 0)
				error("Calibration file '%s' doesn't have DEVICE_CLASS of DISPLAY",calname);

			if ((fi = ccg->find_kword(ccg, 0, "TV_OUTPUT_ENCODING")) >= 0) {
				if (strcmp(ccg->t[0].kdata[fi], "YES") == 0
				 || strcmp(ccg->t[0].kdata[fi], "yes") == 0)
					out_tvenc = 1;
			}

			if ((ii = ccg->find_field(ccg, 0, "RGB_I")) < 0)
				error("Calibration file '%s' doesn't contain field RGB_I",calname);
			if (ccg->t[0].ftype[ii] != r_t)
				error("Field RGB_R in file '%s' is wrong type",calname);
			if ((ri = ccg->find_field(ccg, 0, "RGB_R")) < 0)
				error("Calibration file '%s' doesn't contain field RGB_R",calname);
			if (ccg->t[0].ftype[ri] != r_t)
				error("Field RGB_R in file '%s' is wrong type",calname);
			if ((gi = ccg->find_field(ccg, 0, "RGB_G")) < 0)
				error("Calibration file '%s' doesn't contain field RGB_G",calname);
			if (ccg->t[0].ftype[gi] != r_t)
				error("Field RGB_G in file '%s' is wrong type",calname);
			if ((bi = ccg->find_field(ccg, 0, "RGB_B")) < 0)
				error("Calibration file '%s' doesn't contain field RGB_B",calname);
			if (ccg->t[0].ftype[bi] != r_t)
				error("Field RGB_B in file '%s' is wrong type",calname);
			for (i = 0; i < ncal; i++) {
				cal[0][i] = *((double *)ccg->t[0].fdata[i][ri]);
				cal[1][i] = *((double *)ccg->t[0].fdata[i][gi]);
				cal[2][i] = *((double *)ccg->t[0].fdata[i][bi]);
			}

			/* Interpolate from cal value to RAMDAC entries */
			for (i = 0; i < dw->r->nent; i++) {
				double val, w;
				unsigned int ix;
	
				val = (ncal-1.0) * i/(dw->r->nent-1.0);
				ix = (unsigned int)floor(val);		/* Coordinate */
				if (ix > (ncal-2))
					ix = (ncal-2);
				w = val - (double)ix;		/* weight */
				for (j = 0; j < 3; j++) {
					val = cal[j][ix];
					dw->r->v[j][i] = val + w * (cal[j][ix+1] - val);
				}
			}
			/* If the calibration was created with a restricted range video encoding, */
			/* ensure that the installed calibration applies this encoding. */
			if (out_tvenc) {
				for (i = 0; i < dw->r->nent; i++) {
					for (j = 0; j < 3; j++) {
						dw->r->v[j][i] = (dw->r->v[j][i] * (235.0-16.0) + 16.0)/255.0;

						/* For video encoding the extra bits of precision are created by bit */
						/* shifting rather than scaling, so we need to scale the fp value to */
						/* account for this. */
						if (dw->edepth > 8)
							dw->r->v[j][i] = (dw->r->v[j][i] * 255 * (1 << (dw->edepth - 8)))
				                       /((1 << dw->edepth) - 1.0); 	
					}
				}
			}

			debug("Got cal file calibration\n");
		}
		if (ccg != NULL)
			ccg->del(ccg);
		if (icco != NULL)
			icco->del(icco);
		if (rd_fp != NULL)
			rd_fp->del(rd_fp);
	}

	/* Install the profile into the display and set as the default */
	if (installprofile == 1) {
		if (is_ok_icc == 0)
			error("File '%s' doesn't seem to be an ICC profile!",calname);

		if (dw->r== NULL) {
			warning("Unable to access VideoLUT so can't install calibration");
		} else {
			if (verb)
				printf("About to install '%s' as display's default profile\n",calname);
			if (dw->or)
				dw->or->del(dw->or);
			if ((dw->or = dw->r->clone(dw->r)) == NULL)
				error("Failed to clone VideoLUT - memory ?");
			if (dw->install_profile(dw, calname, dw->or, scope))
				error("Failed to install profile '%s'!",calname);
			if (verb)
				printf("Installed '%s' and made it the default\n",calname);
		}

	/* load the LUT with the calibration from the given file or the current display profile. */
	} else if (loadfile != 0 || loadprofile != 0) {
		int rv;

		if (dw->or == NULL) {
			warning("Calibration not loaded because there is no access to the VideoLUT");
		} else {
			if (verb)
				printf("About to set display to given calibration\n");
			if (dw->or)
				dw->or->del(dw->or);
			if ((dw->or = dw->r->clone(dw->r)) == NULL)
				error("Failed to clone VideoLUT - memory ?");
			if ((rv = dw->set_ramdac(dw,dw->or,1)) != 0) {
				if (rv == 2)
					error("Failed to set VideoLUTs persistently because current System Profile can't be renamed");
				else
					error("Failed to set VideoLUTs");
			}
			if (verb)
				printf("Calibration set\n");
		}
	}

	if (verify != 0) {
		int ver = 1;
		double berr = 0.0;

		if (dw->oor == NULL)
			error("Unable to get original VideoLUT for verify");
	
		if (dw->r == NULL)
			error("No calibration to verify against");

		if (dw->r->nent != dw->oor->nent)
			error("VideoLUTs have different size");

		for (j = 0; j < 3; j++) {
			for (i = 0; i < dw->oor->nent; i++) {
				double err;
				err = fabs(dw->oor->v[j][i] - dw->r->v[j][i]);
				if (err > berr)
					berr = err;
				if (err > VERIFY_TOL) {
					ver = 0;
				}
			}
		}
		if (ver)
			printf("Verify: '%s' IS loaded (discrepancy %.1f%%)\n", calname, berr * 100);
		else
			printf("Verify: '%s' is NOT loaded (discrepancy %.1f%%)\n", calname, berr * 100);
	}

	/* If no other command selected, do a Window or VideoLUT test */
	if (sname[0] == '\000' && clear == 0 && installprofile == 0 && loadfile == 0 && verify == 0 && loadprofile == 0) {

		if (ramd == 0) {

			if (fade) {
				int i;
				int steps = 256;
				for (i = 0; i < steps; i++) {
					double tt;
					tt = i/(steps - 1.0);
					dw->set_color(dw, tt, tt, tt);
					msec_sleep(20);
					printf("Val = %f\n",tt);
				}

			/* Patch colors from a CGATS file */
			} else if (pcname[0] != '\000') {
				cgats *icg;
				int i, npat;
				int ri, gi, bi;
				int si = -1;
				
				if ((icg = new_cgats()) == NULL)
					error("new_cgats() failed\n");
				icg->add_other(icg, "");		/* Allow any signature file */

				if (icg->read_name(icg, pcname))
					error("File '%s' read error : %s",pcname, icg->err);

				if (icg->ntables < 1)		/* We don't use second table at the moment */
					error ("Input file '%s' doesn't contain at least one table",pcname);

				if ((npat = icg->t[0].nsets) <= 0)
					error ("File '%s has no sets of data in the first table",pcname);

				si = icg->find_field(icg, 0, "SAMPLE_ID");
				if (si >= 0 && icg->t[0].ftype[si] != nqcs_t)
					error("In file '%s' field SAMPLE_ID is wrong type",pcname);

				if ((ri = icg->find_field(icg, 0, "RGB_R")) < 0)
					error ("Input file '%s' doesn't contain field RGB_R",pcname);
				if (icg->t[0].ftype[ri] != r_t)
					error ("In file '%s' field RGB_R is wrong type",pcname);
				if ((gi = icg->find_field(icg, 0, "RGB_G")) < 0)
					error ("Input file '%s' doesn't contain field RGB_G",pcname);
				if (icg->t[0].ftype[gi] != r_t)
					error ("In file '%s' field RGB_G is wrong type",pcname);
				if ((bi = icg->find_field(icg, 0, "RGB_B")) < 0)
					error ("Input file '%s' doesn't contain field RGB_B",pcname);
				if (icg->t[0].ftype[bi] != r_t)
					error ("In file '%s' field RGB_B is wrong type",pcname);

				if (inf == 2)
					printf("\nHit return to advance each color\n");

					if (inf == 2) {
						printf("\nHit return to start\n");
						getchar();				
					}
				for (i = 0; i < npat; i++) {
					double r, g, b;
					r = *((double *)icg->t[0].fdata[i][ri]) / 100.0;
					g = *((double *)icg->t[0].fdata[i][gi]) / 100.0;
					b = *((double *)icg->t[0].fdata[i][bi]) / 100.0;

					if (si >= 0)
						printf("Patch id '%s'",((char *)icg->t[0].fdata[i][si]));
					else
						printf("Patch no %d",i+1);
					printf(" color %f %f %f\n",r,g,b);
				
					if (dw->set_color(dw, r, g, b) != 0) {	
						error ("set_color failed");
					}

					if (inf == 2)
						getchar();				
					else
						sleep(2);
				}
				icg->del(icg);

			/* Manually define patche colors */
			} else if (nmrgb > 0) {
				int i;
				int ri, gi, bi;
				
				if (inf == 2)
					printf("\nHit return to advance each color\n");

					if (inf == 2) {
						printf("\nHit return to start\n");
						getchar();				
					}
				for (i = 0; i < nmrgb; i++) {
					double r, g, b;
					r = mrgb[i][0];
					g = mrgb[i][1];
					b = mrgb[i][2];

					printf("Patch no %d",i+1);
					printf(" color %f %f %f\n",r,g,b);
				
					if (dw->set_color(dw, r, g, b) != 0) {	
						error ("set_color failed");
					}

					if (inf == 2)
						getchar();				
					else
						sleep(2);
				}

			/* Preset and random patch colors */
			} else {

				if (inf == 2)
					printf("\nHit return to advance each color\n");

				printf("Setting White\n");
				dw->set_color(dw, 1.0, 1.0, 1.0);	/* White */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting 75%% Grey\n");
				dw->set_color(dw, 0.75, 0.75, 0.75);	/* Grey */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting 50%% Grey\n");
				dw->set_color(dw, 0.5, 0.5, 0.5);	/* Grey */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting 25%% Grey\n");
				dw->set_color(dw, 0.25, 0.25, 0.25);	/* Grey */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting 12.5%% Grey\n");
				dw->set_color(dw, 0.125, 0.125, 0.125);	/* Grey */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting Black\n");
				dw->set_color(dw, 0.0, 0.0, 0.0);

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting Red\n");
				dw->set_color(dw, 1.0, 0.0, 0.0);	/* Red */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting Green\n");
				dw->set_color(dw, 0.0, 1.0,  0.0);	/* Green */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting Blue\n");
				dw->set_color(dw, 0.0, 0.0, 1.0);	/* Blue */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting Cyan\n");
				dw->set_color(dw, 0.0, 1.0, 1.0);	/* Cyan */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting Magenta\n");
				dw->set_color(dw, 1.0, 0.0,  1.0);	/* Magenta */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting Yellow\n");
				dw->set_color(dw, 1.0, 1.0, 0.0);	/* Yellow */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting 50%% Red\n");
				dw->set_color(dw, 0.5, 0.0, 0.0);	/* Red */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting 50%% Green\n");
				dw->set_color(dw, 0.0, 0.5,  0.0);	/* Green */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				printf("Setting 50%% Blue\n");
				dw->set_color(dw, 0.0, 0.0, 0.5);	/* Blue */

				if (inf == 2)
					getchar();				
				else
					sleep(2);

				if (inf == 1) {
					for (;inf != 0;) {
						double col[3];
	
						for (i = 0; i < 3; i++) {
							seed = PSRAND32(seed);
							col[i] = seed/4294967295.0;
						}
	
						printf("Setting %f %f %f\n",col[0],col[1],col[2]);
						dw->set_color(dw, col[0],col[1],col[2]);
	
						if (inf == 2)
							getchar();
						else
							sleep(2);
	
					}
				}
			}
		}

		if (inf != 2) {
			/* Test out the VideoLUT access */
			if (dw->r != NULL) {	/* Working ramdac to use */
				
				/* Try darkening it */
				for (j = 0; j < 3; j++) {
					for (i = 0; i < dw->r->nent; i++) {
						dw->r->v[j][i] = pow(dw->or->v[j][i], 2.0);
					}
				}
				printf("Darkening screen\n");
				if (dw->set_ramdac(dw, dw->r, 0)) {
					dw->set_ramdac(dw, dw->or, 0);		/* is this needed ? */
					error("Failed to set VideoLUTs");
				}
				sleep(1);
	
				/* Try lightening it */
				for (j = 0; j < 3; j++) {
					for (i = 0; i < dw->r->nent; i++) {
						dw->r->v[j][i] = pow(dw->or->v[j][i], 0.5);
					}
				}
				printf("Lightening screen\n");
				if (dw->set_ramdac(dw,dw->r,0)) {
					dw->set_ramdac(dw,dw->or,0);		/* is this needed ? */
					error("Failed to set VideoLUTs");
				}
				sleep(1);
	
				/* restor it */
				printf("Restoring screen\n");
				if (dw->set_ramdac(dw,dw->or,0)) {
					error("Failed to set VideoLUTs");
				}
	
			} else {
				printf("We don't have access to the VideoLUT\n");
			}
	
			/* Test out the beeps */
			printf("Normal beep\n");
			normal_beep();
	
			sleep(1);
		
			printf("Good beep\n");
			good_beep();
	
			sleep(1);
		
			printf("Bad double beep\n");
			bad_beep();
	
			sleep(2);		/* Allow beep to complete */
		}
	}
	
	if (disp != NULL)
		free_a_disppath(disp);

	if (verb)
		printf("About to destroy dispwin object\n");

	dw->del(dw);

	return 0;
}

#endif /* STANDALONE_TEST */

/* ================================================================ */
/* Possible ThinkPad MSWin code to keep the main screen on */

#ifdef NEVER

Public Class Form1
    Declare Sub XRCalibrationLidTurnOnNotification Lib "C:\Program Files (x86)\X-Rite\PANTONE Color Calibrator\XRLaptopIFSDK.dll" ()
    Declare Sub XRCalibrationLidTurnOffNotification Lib "C:\Program Files (x86)\X-Rite\PANTONE Color Calibrator\XRLaptopIFSDK.dll" ()

    Private Sub Form1_Load(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles MyBase.Load

    End Sub

    Private Sub Button1_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button1.Click
        Call XRCalibrationLidTurnOnNotification()
    End Sub

    Private Sub Button2_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Button2.Click
        Call XRCalibrationLidTurnOffNotification()
    End Sub
End Class

#endif

/* ================================================================ */
/* Unused Apple code */

#ifdef NEVER
	/* Got displays, now have a look through them */
	for (i = 0; i < dcount; i++) {
		GDHandle gdh;
		GDPtr gdp;

		/* Dump display mode dictionary */
		CFIndex nde, j;
		CFDictionaryRef dr;
		void **keys, **values;
		
		dr = CGDisplayCurrentMode(dids[i]);
		nde = CFDictionaryGetCount(dr);

		printf("Dict contains %d entries \n", nde);
		if ((keys = (void **)malloc(nde * sizeof(void *))) == NULL) {
			debug("malloc failed for disp mode keys\n");
			free_disppaths(disps);
			free(dids);
			return NULL;
		}
		if ((values = (void **)malloc(nde * sizeof(void *))) == NULL) {
			debug("malloc failed for disp mode values\n");
			free(keys);
			free_disppaths(disps);
			free(dids);
			return NULL;
		}
		CFDictionaryGetKeysAndValues(dr, (const void **)keys, (const void **)values);
		for (j = 0; j < nde; j++) {
			printf("Entry %d key = %s\n", j, CFStringGetCStringPtr(keys[j], kCFStringEncodingMacRoman));
		}
		free(values);
		free(keys);
	}
#endif


#ifdef NEVER
/* How to install profiles for devices. */

/* Callback to locate a profile ID. */
struct idp_rec {
	CMDeviceID ddid;			/* Device ID */
//	char *fname;				/* Profile we're trying to find */
	CMDeviceProfileID id;		/* Corresponding ID */
	CMDeviceScope dsc;			/* Matching device scope */
	int found;					/* Flag indicating it's been found */
};

OSErr ItDevProfProc (
const CMDeviceInfo *di,
const NCMDeviceProfileInfo *pi,
void *refCon) 
{
	CMError ev;
	struct idp_rec *r = (struct idp_rec *)refCon;
	CMDeviceProfileID id;

	if (di->deviceClass != cmDisplayDeviceClass
	 || di->deviceID != r->ddid) {
		return noErr;
	}

	/* We'd qualify on the device mode too (deviceState ??), */
	/* if we wanted to replace a profile for a particular mode. */

	/* Assume this is a display with only one mode, and return */
	/* the profile id and device scope */
	r->id = pi->profileID;
	r->dsc = di->deviceScope;
	r->found = 1;
	return noErr;
//printf("~1 got match\n");

/* Callback to locate a profile ID. */
struct idp_rec {
	CMDeviceID ddid;			/* Device ID */
	CMDeviceProfileID id;		/* Corresponding ID */
	CMDeviceScope dsc;			/* Matching device scope */
	int found;					/* Flag indicating it's been found */
};

OSErr ItDevProfProc (
const CMDeviceInfo *di,
const NCMDeviceProfileInfo *pi,
void *refCon) 
{
	CMError ev;
	struct idp_rec *r = (struct idp_rec *)refCon;
	CMDeviceProfileID id;

	if (di->deviceClass != cmDisplayDeviceClass
	 || di->deviceID != r->ddid) {
		return noErr;
	}

	/* Assume this is a display with only one mode, and return */
	/* the profile id and device scope */
	r->id = pi->profileID;
	r->dsc = di->deviceScope;
	r->found = 1;
	return noErr;
}

#ifndef NEVER

/* Given a location, return a string for it's path */
static char *plocpath(CMProfileLocation *ploc) {

	if (ploc->locType == cmFileBasedProfile) {
		FSRef newRef;
  		static UInt8 path[256] = "";
//printf("~1 converted spec file location\n");

		if (FSpMakeFSRef(&ploc->u.fileLoc.spec, &newRef) == noErr) {
			OSStatus stus;
			if ((stus = FSRefMakePath(&newRef, path, 256)) == 0 || stus == fnfErr) {
				return path;
			}
		} 
		return strdup(path);
	} else if (ploc->locType == cmPathBasedProfile) {
		return strdup(ploc->u.pathLoc.path);
	}
	return NULL;
}

#else

/* fss2path takes the FSSpec of a file, folder or volume and returns it's POSIX (?) path. */
/* Return NULL on error. Free returned string */
/* NOTE FSSpec is deprecated in 10.5. Replace with FSRef ?? */
static char *fss2path(FSSpec *fss) {
	int i, l;						 /* fss->name contains name of last item in path */
	char *path;

	l = fss->name[0];
	if ((path = malloc(l + 1)) == NULL)
		return NULL;
	for (i = 0; i < l; i++) {
		if (fss->name[i+1] == '/')
			path[i] = ':';
		else
			path[i] = fss->name[i+1];
	}
	path[i] = '\000';
//printf("~1 path = '%s', l = %d\n",path,l);

	if(fss->parID != fsRtParID) { /* path is more than just a volume name */
		FSSpec tfss;
//		CInfoPBRec pb;
		FSRefParam pb;
		int tl;
		char *tpath;
		
		memmove(&tfss, fss, sizeof(FSSpec));		/* Copy so we can modify */
		memset(&pb, 0, sizeof(FSRefParam));
		pb.ioNamePtr = tfss.name;
		pb.ioVRefNum = tfss.vRefNum;
		pb.ioDrParID = tfss.parID;
		do {
			pb.ioFDirIndex = -1;	/* get parent directory name */
			pb.ioDrDirID = pb.dirInfo.ioDrParID;	
			if(PBGetCatlogInfoSync(&pb) != noErr) {
				free(path);
				return NULL;
			}

			/* Pre-pend the directory name separated by '/' */
			if (pb.dirInfo.ioDrDirID == fsRtDirID) {
				tl = 0;			/* Don't pre-pend volume name */
			} else {
				tl = tfss.name[0];
			}
			if ((tpath = malloc(tl + l + 1)) == NULL) {
				free(path);
			 	return NULL;
			}
			for (i = 0; i < tl; i++) {
				if (tfss.name[i+1] == '/')
					tpath[i] = ':';
				else
					tpath[i] = tfss.name[i+1];
			}
			tpath[i] = '/';
			for (i = 0; i < l; i++)
				tpath[tl+1+i] = path[i];
			tpath[tl+1+i] = '\000';
			free(path);
			path = tpath;
			l = tl + 1 + l;
//printf("~1 path = '%s', l = %d\n",path,l);
		} while(pb.dirInfo.ioDrDirID != fsRtDirID); /* while more directory levels */
	}

	return path;
}

/* Return a string containing the profiles path. */
/* Return NULL on error. Free the string after use. */
static char *plocpath(CMProfileLocation *ploc) {
	if (ploc->locType == cmFileBasedProfile) {
		return fss2path(&ploc->u.fileLoc.spec);
	} else if (ploc->locType == cmPathBasedProfile) {
		return strdup(ploc->u.pathLoc.path);
	}
	return NULL;
}

#endif

/* Test code that checks what the current display default profile is, three ways */
static void pcurpath(dispwin *p) {
		CMProfileRef xprof;			/* Current AVID profile */
		CMProfileLocation xploc;		/* Current profile location */
		UInt32 xplocsz = sizeof(CMProfileLocation);
		struct idp_rec cb;
		CMError ev;

		/* Get the current installed profile */
		if ((ev = CMGetProfileByAVID((CMDisplayIDType)p->ddid, &xprof)) != noErr) {
			debug2((errout,"CMGetProfileByAVID() failed with error %d\n",ev));
			goto skip;
		}

		/* Get the current installed  profile's location */
		if ((ev = NCMGetProfileLocation(xprof, &xploc, &xplocsz)) != noErr) {
			debug2((errout,"NCMGetProfileLocation() failed with error %d\n",ev));
			goto skip;
		}

//printf("~1 Current profile by AVID = '%s'\n",plocpath(&xploc));

		/* Get the current CMDeviceProfileID and device scope */
		cb.ddid = (CMDeviceID)p->ddid;			/* Display Device ID */
		cb.found = 0;

		if ((ev = CMIterateDeviceProfiles(ItDevProfProc, NULL, NULL, cmIterateAllDeviceProfiles, (void *)&cb)) != noErr) {
			debug2((errout,"CMIterateDeviceProfiles() failed with error %d\n",ev));
			goto skip;
		}
		if (cb.found == 0) {
			debug2((errout,"Failed to find exsiting profiles ID\n"));
			goto skip;
		}
		/* Got cb.id */

		if ((ev = CMGetDeviceProfile(cmDisplayDeviceClass, (CMDeviceID)p->ddid, cb.id, &xploc)) != noErr) {
			debug2((errout,"Failed to GetDeviceProfile\n"));
			goto skip;
		}
//printf("~1 Current profile by Itterate = '%s'\n",plocpath(&xploc));

		/* Get the default ID for the display */
		if ((ev = CMGetDeviceDefaultProfileID(cmDisplayDeviceClass, (CMDeviceID)p->ddid, &cb.id)) != noErr) {
			debug2((errout,"CMGetDeviceDefaultProfileID() failed with error %d\n",ev));
			goto skip;
		}

		/* Get the displays default profile */
		if ((ev = CMGetDeviceProfile(cmDisplayDeviceClass, (CMDeviceID)p->ddid, cb.id, &xploc)) != noErr) {
			debug2((errout,"CMGetDeviceDefaultProfileID() failed with error %d\n",ev));
			goto skip;
		}
//printf("~1 Current profile by get default = '%s'\n",plocpath(&xploc));

	skip:;
}
	/* If we want the path to the profile, we'd do this: */

	printf("id = 0x%x\n",pi->profileID);
	if (pi->profileLoc.locType == cmFileBasedProfile) {
		FSRef newRef;
  		UInt8 path[256] = "";

		if (FSpMakeFSRef(&pi->profileLoc.u.fileLoc.spec, &newRef) == noErr) {
			OSStatus stus;
			if ((stus = FSRefMakePath(&newRef, path, 256)) == 0 || stus == fnfErr) {
				printf("file = '%s'\n",path);
				if (strcmp(r->fname, (char *)path) == 0) {
					r->id = pi->profileID;
					r->found = 1;
					printf("got match\n");
				}
			}
		} 
	} else if (pi->profileLoc.locType == cmPathBasedProfile) {
		if (strcmp(r->fname, pi->profileLoc.u.pathLoc.path) == 0) {
			r->id = pi->profileID;
			r->dsc = di->deviceScope;
			r->found = 1;
			printf("got match\n");
		}
	}


	{
		struct idp_rec cb;

		/* The CMDeviceProfileID wll always be 1 for a display, because displays have only one mode, */
		/* so the Iterate could be avoided for it. */

		cb.ddid = (CMDeviceID)p->ddid;			/* Display Device ID */
//		cb.fname = dpath;
		cb.found = 0;

		if ((ev = CMIterateDeviceProfiles(ItDevProfProc, NULL, NULL, cmIterateAllDeviceProfiles, (void *)&cb)) != noErr) {
			debug2((errout,"CMIterateDeviceProfiles() failed with error %d\n",dpath,ev));
			return 1;
		}
		if (cb.found == 0) {
			debug2((errout,"Failed to find exsiting  profiles ID, so ca't set it as default\n"));
			return 1;
		}
		if ((ev = CMSetDeviceProfile(cmDisplayDeviceClass, (CMDeviceID)p->ddid, &cb.dsc, cb.id, &dploc)) != noErr) {
			debug2((errout,"CMSetDeviceProfile() failed for file '%s' with error %d\n",dpath,ev));
			return 1;
		}
		/* There is no point in doing the following, because displays only have one mode. */
		/* Make it the default for the display */
		if ((ev = CMSetDeviceDefaultProfileID(cmDisplayDeviceClass, (CMDeviceID)p->ddid, cb.id)) != noErr) {
			debug2((errout,"CMSetDeviceDefaultProfileID() failed for file '%s' with error %d\n",dpath,ev));
			return 1;
		}

#endif /* NEVER */

#ifdef NEVER
	/* Unused 10.6+ code */
	CFUUIDRef dispuuid;
	CFDictionaryRef dict, sdict, pdict;
	CFStringRef id, urlstr;
	CFURLRef url;
	CFIndex bufSize;
	char *dpath = NULL;			/* return value */

	if ((dispuuid = CGDisplayCreateUUIDFromDisplayID(p->ddid)) == NULL) {
		debugr2((errout,"CGDisplayCreateUUIDFromDisplayID() failed\n"));
		return NULL;
	}

	if ((dict = ColorSyncDeviceCopyDeviceInfo(kColorSyncDisplayDeviceClass, dispuuid)) == NULL) {
		debugr2((errout,"ColorSyncDeviceCopyDeviceInfo() failed\n"));
		CFRelease(dispuuid);
		return NULL;
	}

	/* Get the factory profile dictionary */
	if ((sdict = CFDictionaryGetValue(dict, kColorSyncFactoryProfiles)) == NULL) {
		debugrr("Failed to get kColorSyncFactoryProfiles\n");
		CFRelease(dict);
		CFRelease(dispuuid);
		return NULL;
	}
	/* Lookup the default profile ID */
	if ((id = CFDictionaryGetValue(sdict, kColorSyncDeviceDefaultProfileID)) == NULL) {
		debugrr("Failed to get kColorSyncDeviceDefaultProfileID\n");
		CFRelease(dict);
		CFRelease(dispuuid);
		return NULL;
	}

//	printf("~1 got ProfileID '%s'\n",CFStringGetCStringPtr(id, kCFStringEncodingMacRoman));

	/* See if this ProfileID is in the factory profile dictionary */
	if ((pdict = CFDictionaryGetValue(sdict, id)) != NULL) {
	} else {
		debugrr("Failed to get factory profile with id \n");

		/* Get the custom profile dictionary */
		if ((sdict = CFDictionaryGetValue(dict, kColorSyncCustomProfiles)) == NULL) {
			debugrr("Failed to get kColorSyncCustomProfiles\n");
			CFRelease(dict);
			CFRelease(dispuuid);
			return NULL;
		}
		/* See if the default id is in the custom profile dictionary */
		if ((pdict = CFDictionaryGetValue(sdict, id)) == NULL) {
			debugrr("Failed to locate default profile in factory or custom profile list\n");
			CFRelease(dict);
			CFRelease(dispuuid);
			return NULL;
		}
	}

	/* get the URL */
	if ((url = CFDictionaryGetValue(pdict, kColorSyncDeviceProfileURL)) == NULL) {
		debugrr("Failed to get default profile URL\n");
		CFRelease(dict);
		CFRelease(dispuuid);
		return NULL;
	}

//	urlstr = CFURLGetString(url);
//	printf("~1 got URL '%s'\n",CFStringGetCStringPtr(urlstr, kCFStringEncodingMacRoman));

	urlstr = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
	bufSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(urlstr),
	                                              kCFStringEncodingUTF8) + 1;
	if ((dpath = malloc(bufSize)) == NULL) {
		debugrr("cur_profile malloc failed\n");
		CFRelease(dict);
		CFRelease(dispuuid);
		return NULL;
	}
	if (!CFStringGetCString(urlstr, dpath, bufSize, kCFStringEncodingUTF8)) {
		debugrr("cur_profile CFStringGetCString failed\n");
		CFRelease(dict);
		CFRelease(dispuuid);
		return NULL;
	}
	printf("~1 got path '%s'\n",dpath);

	CFRelease(dict);
	CFRelease(dispuuid);

//	return dpath;
	return NULL;
#endif

