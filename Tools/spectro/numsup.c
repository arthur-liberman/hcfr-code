
/* General Numerical routines support functions, */
/* and common Argyll support functions. */
/* (Perhaps these should be moved out of numlib at some stange ?) */

/*
 * Copyright 1997 - 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#if defined (NT)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef UNIX
#include <unistd.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <pthread.h>
#endif
#ifndef SALONEINSTLIB
#include "aconfig.h"
#else
#include "sa_config.h"
#endif

#define NUMSUP_C
#include "numsup.h"

/* 
 * TODO: Should probably break all the non-numlib stuff out into
 *       a separate library, so that it can be ommitted.
 *       Or enhance it so that numerical callers of error()
 *       can get a callback on out of memory etc. ???
 *
 */

/* Globals */

char *exe_path = "\000";			/* Directory executable resides in ('/' dir separator) */
//char *error_program = "Unknown";	/* Name to report as responsible for an error */

static int g_log_init = 0;	/* Initialised ? */
static int g_deb_init = 0;	/* Debug output Initialised ? */
extern a1log default_log;
extern a1log *g_log;

/* Should Vector/Matrix Support functions return NULL on error, */
/* or call error() ? */
int ret_null_on_malloc_fail = 0;	/* Call error() */

/******************************************************************/
/* Executable path routine. Sets default error_program too. */
/******************************************************************/

/* Pass in argv[0] from main() */
/* Sets the error_program name too */
void set_exe_path(char *argv0) {
	int i;

	g_log->tag = argv0;
	i = strlen(argv0);
	if ((exe_path = malloc(i + 5)) == NULL) {
		a1loge(g_log, 1, "set_exe_path: malloc %d bytes failed\n",i+5);
		return;
	}
	strcpy(exe_path, argv0);

#ifdef NT	/* CMD.EXE doesn't give us the full path in argv[0] :-( */
			/* so we need to fix this */
	{
		HMODULE mh;
		char *tpath = NULL;
		int pl;

		/* Add an .exe extension if it is missing */
		if (i < 4 || _stricmp(exe_path +i -4, ".exe") != 0)
			strcat(exe_path, ".exe");

		if ((mh = GetModuleHandle(exe_path)) == NULL) {
			a1loge(g_log, 1, "set_exe_path: GetModuleHandle '%s' failed with%d\n",
			                                            exe_path,GetLastError());
			exe_path[0] = '\000';
			return;
		}
		
		/* Retry until we don't truncate the returned path */
		for (pl = 100; ; pl *= 2) {
			if (tpath != NULL)
				free(tpath);
			if ((tpath = malloc(pl)) == NULL) {
				a1loge(g_log, 1, "set_exe_path: malloc %d bytes failed\n",pl);
				exe_path[0] = '\000';
			return;
			}
			if ((i = GetModuleFileName(mh, tpath, pl)) == 0) {
				a1loge(g_log, 1, "set_exe_path: GetModuleFileName '%s' failed with%d\n",
				                                                tpath,GetLastError());
				exe_path[0] = '\000';
				return;
			}
			if (i < pl)		/* There was enough space */
				break;
		}
		free(exe_path);
		exe_path = tpath;

		/* Convert from MSWindows to UNIX file separator convention */
		for (i = 0; ;i++) {
			if (exe_path[i] == '\000')
				break;
			if (exe_path[i] == '\\')
				exe_path[i] = '/';
		}
	}
#else		/* Neither does UNIX */

	if (*exe_path != '/') {			/* Not already absolute */
		char *p, *cp;
		if (strchr(exe_path, '/') != 0) {	/* relative path */
			cp = ".:";				/* Fake a relative PATH */
		} else  {
			cp = getenv("PATH");
		}
		if (cp != NULL) {
			int found = 0;
			while((p = strchr(cp,':')) != NULL
			 || *cp != '\000') {
				char b1[PATH_MAX], b2[PATH_MAX];
 				int ll;
				if (p == NULL)
					ll = strlen(cp);
				else
					ll = p - cp;
				if ((ll + 1 + strlen(exe_path) + 1) > PATH_MAX) {
					a1loge(g_log, 1, "set_exe_path: Search path exceeds PATH_MAX\n");
					exe_path[0] = '\000';
					return;
				}
				strncpy(b1, cp, ll);		/* Element of path to search */
				b1[ll] = '\000';
				strcat(b1, "/");
				strcat(b1, exe_path);		/* Construct path */
				if (realpath(b1, b2)) {
					if (access(b2, 0) == 0) {	/* See if exe exits */
						found = 1;
						free(exe_path);
						if ((exe_path = malloc(strlen(b2)+1)) == NULL) {
							a1loge(g_log, 1, "set_exe_path: malloc %d bytes failed\n",strlen(b2)+1);
							exe_path[0] = '\000';
							return;
						}
						strcpy(exe_path, b2);
						break;
					}
				}
				if (p == NULL)
					break;
				cp = p + 1;
			}
			if (found == 0)
				exe_path[0] = '\000';
		}
	}
#endif
	/* strip the executable path to the base */
	for (i = strlen(exe_path)-1; i >= 0; i--) {
		if (exe_path[i] == '/') {
			char *tpath;
			if ((tpath = malloc(strlen(exe_path + i))) == NULL) {
				a1loge(g_log, 1, "set_exe_path: malloc %d bytes failed\n",strlen(exe_path + i));
				exe_path[0] = '\000';
				return;
			}
			strcpy(tpath, exe_path + i + 1);
			g_log->tag = tpath;				/* Set g_log->tag to base name */
			exe_path[i+1] = '\000';				/* (The malloc never gets free'd) */
			break;
		}
	}
	/* strip off any .exe from the g_log->tag to be more readable */
	i = strlen(g_log->tag);
	if (i >= 4
	 && g_log->tag[i-4] == '.'
	 && (g_log->tag[i-3] == 'e' || g_log->tag[i-3] == 'E')
	 && (g_log->tag[i-2] == 'x' || g_log->tag[i-2] == 'X')
	 && (g_log->tag[i-1] == 'e' || g_log->tag[i-1] == 'E'))
		g_log->tag[i-4] = '\000';

//	a1logd(g_log, 1, "exe_path = '%s'\n",exe_path);
//	a1logd(g_log, 1, "g_log->tag = '%s'\n",g_log->tag);
}

/******************************************************************/
/* Check "ARGYLL_NOT_INTERACTIVE" environment variable.           */
/******************************************************************/

/* Check if the "ARGYLL_NOT_INTERACTIVE" environment variable is */
/* set, and set cr_char to '\n' if it is. */

int not_interactive = 0;
char cr_char = '\r';

void check_if_not_interactive() {
	char *ev;

	if ((ev = getenv("ARGYLL_NOT_INTERACTIVE")) != NULL) {
		not_interactive = 1;
		cr_char = '\n';
	} else {
		not_interactive = 0;
		cr_char = '\r';
	}
}

/******************************************************************/
/* Default verbose/debug/error loger                              */
/* It's values can be overridden to redirect these messages.      */
/******************************************************************/

static void va_loge(a1log *p, char *fmt, ...);

#ifdef NT

/* Get a string describing the MWin operating system */

typedef struct {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    WCHAR  szCSDVersion[128];
    WORD   wServicePackMajor;
    WORD   wServicePackMinor;
    WORD   wSuiteMask;
    BYTE  wProductType;
    BYTE  wReserved;
} osversioninfoexw;

#ifndef VER_NT_DOMAIN_CONTROLLER
# define VER_NT_DOMAIN_CONTROLLER 0x0000002
# define VER_NT_SERVER 0x0000003
# define VER_NT_WORKSTATION 0x0000001 
#endif

static char *get_sys_info() {
	static char sysinfo[100] = { "Unknown" };
	LONG (WINAPI *pfnRtlGetVersion)(osversioninfoexw*);

	*(FARPROC *)&pfnRtlGetVersion
	 = GetProcAddress(GetModuleHandle("ntdll.dll"), "RtlGetVersion");
	if (pfnRtlGetVersion != NULL) {
	    osversioninfoexw ver = { 0 };
	    ver.dwOSVersionInfoSize = sizeof(ver);

	    if (pfnRtlGetVersion(&ver) == 0) {
			if (ver.dwMajorVersion > 6 || (ver.dwMajorVersion == 6 && ver.dwMinorVersion > 3)) {
				if (ver.wProductType == VER_NT_WORKSTATION)
					sprintf(sysinfo,"Windows V%d.%d SP %d",
						ver.dwMajorVersion,ver.dwMinorVersion,
						ver.wServicePackMajor);
				else
					sprintf(sysinfo,"Windows Server 2016 V%d.%d SP %d",
						ver.dwMajorVersion,ver.dwMinorVersion,
						ver.wServicePackMajor);
				
			} else if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 3) {
				if (ver.wProductType == VER_NT_WORKSTATION)
					sprintf(sysinfo,"Windows V8.1 SP %d",
						ver.wServicePackMajor);
				else
					sprintf(sysinfo,"Windows Server 2012 R2 SP %d",
						ver.wServicePackMajor);

			} else if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 2) {
				if (ver.wProductType == VER_NT_WORKSTATION)
					sprintf(sysinfo,"Windows V8 SP %d",
						ver.wServicePackMajor);
				else
					sprintf(sysinfo,"Windows Server SP %d",
						ver.wServicePackMajor);

			} else if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 1) {
				if (ver.wProductType == VER_NT_WORKSTATION)
					sprintf(sysinfo,"Windows V7 SP %d",
						ver.wServicePackMajor);
				else
					sprintf(sysinfo,"Windows Server 2008 R2 SP %d",
						ver.wServicePackMajor);

			} else if (ver.dwMajorVersion == 6 && ver.dwMinorVersion == 0) {
				if (ver.wProductType == VER_NT_WORKSTATION)
					sprintf(sysinfo,"Windows Vista SP %d",
						ver.wServicePackMajor);
				else
					sprintf(sysinfo,"Windows Server 2008 SP %d",
						ver.wServicePackMajor);

			} else if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 2) {
				// Actually could be Server 2003, Home Server, Server 2003 R2
				sprintf(sysinfo,"Windows XP Pro64 SP %d",
					ver.wServicePackMajor);

			} else if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 1) {
				sprintf(sysinfo,"Windows XP SP %d",
						ver.wServicePackMajor);

			} else if (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 0) {
				sprintf(sysinfo,"Windows XP SP %d",
						ver.wServicePackMajor);

			} else {
				sprintf(sysinfo,"Windows Maj %d Min %d SP %d",
					ver.dwMajorVersion,ver.dwMinorVersion,
					ver.wServicePackMajor);
			}
		}
	}
	return sysinfo;
}


# define A1LOG_LOCK(log, deb)								\
	if (g_log_init == 0) {									\
	    InitializeCriticalSection(&log->lock);				\
		EnterCriticalSection(&log->lock);					\
		g_log_init = 1;										\
	} else {												\
		EnterCriticalSection(&log->lock);					\
	}														\
	if (deb && !g_deb_init) {								\
		va_loge(log, "Argyll 'V%s' Build '%s' System '%s'\n",ARGYLL_VERSION_STR,ARGYLL_BUILD_STR, get_sys_info());	\
		g_deb_init = 1;										\
	}
# define A1LOG_UNLOCK(log) LeaveCriticalSection(&log->lock)
#endif
#ifdef UNIX

static char *get_sys_info() {
	static char sysinfo[200] = { "Unknown" };
	struct utsname ver;

	if (uname(&ver) == 0)
		sprintf(sysinfo,"%s %s %s %s",ver.sysname, ver.version, ver.release, ver.machine);
	return sysinfo;
}

# define A1LOG_LOCK(log, deb)								\
	if (g_log_init == 0) {									\
	    pthread_mutex_init(&log->lock, NULL);				\
		pthread_mutex_lock(&log->lock);						\
		g_log_init = 1;										\
	} else {												\
		pthread_mutex_lock(&log->lock);						\
	}														\
	if (deb && !g_deb_init) {								\
		va_loge(log, "Argyll 'V%s' Build '%s' System '%s'\n",ARGYLL_VERSION_STR,ARGYLL_BUILD_STR, get_sys_info());	\
		g_deb_init = 1;										\
	}
# define A1LOG_UNLOCK(log) pthread_mutex_unlock(&log->lock)
#endif



/* Default verbose logging function - print to stdtout */
static void a1_default_v_log(void *cntx, a1log *p, char *fmt, va_list args) {
	vfprintf(stdout, fmt, args);
	fflush(stdout);
}

/* Default debug & error logging function - print to stderr */
static void a1_default_de_log(void *cntx, a1log *p, char *fmt, va_list args) {
	vfprintf(stderr, fmt, args);
	fflush(stderr);
}

#define a1_default_d_log a1_default_de_log
#define a1_default_e_log a1_default_de_log


/* Call log->loge() with variags */
static void va_loge(a1log *p, char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	p->loge(p->cntx, p, fmt, args);
	va_end(args);
}

/* Global log */
a1log default_log = {
	1,			/* Refcount of 1 because this is not allocated or free'd */
	"argyll",	/* Default tag */
	0,			/* Vebose off */
	0,			/* Debug off */
	NULL,		/* Context */
	&a1_default_v_log,	/* Default verbose to stdout */
	&a1_default_d_log,	/* Default debug to stderr */
	&a1_default_e_log,	/* Default error to stderr */
	0,					/* error code 0 */			
	{ '\000' }			/* No error message */
};
a1log *g_log = &default_log;

/* If log NULL, allocate a new log and return it, */
/* otherwise increment reference count and return existing log, */
/* exit() if malloc fails. */
a1log *new_a1log(
	a1log *log,						/* Existing log to reference, NULL if none */
	int verb,						/* Verbose level to set */
	int debug,						/* Debug level to set */
	void *cntx,						/* Function context value */
		/* Vebose log function to call - stdout if NULL */
	void (*logv)(void *cntx, a1log *p, char *fmt, va_list args),
		/* Debug log function to call - stderr if NULL */
	void (*logd)(void *cntx, a1log *p, char *fmt, va_list args),
		/* Warning/error Log function to call - stderr if NULL */
	void (*loge)(void *cntx, a1log *p, char *fmt, va_list args)
) {
	if (log != NULL) {
		log->refc++;
		return log;
	}
	if ((log = (a1log *)calloc(sizeof(a1log), 1)) == NULL) {
		a1loge(g_log, 1, "new_a1log: malloc of a1log failed, calling exit(1)\n");
		exit(1);
	}
	log->refc = 1;
	log->verb = verb;
	log->debug = debug;

	log->cntx = cntx;
	if (logv != NULL)
		log->logv = logv;
	else
		log->logv = a1_default_v_log;

	if (logd != NULL)
		log->logd = logd;
	else
		log->logd = a1_default_d_log;

	if (loge != NULL)
		log->loge = loge;
	else
		log->loge = a1_default_e_log;

	log->errc = 0;
	log->errm[0] = '\000';

	return log;
}

/* Same as above but set default functions */
a1log *new_a1log_d(a1log *log) {
	return new_a1log(log, 0, 0, NULL, NULL, NULL, NULL);
}

/* Decrement reference count and free log. */
/* Returns NULL */
a1log *del_a1log(a1log *log) {
	if (log != NULL) {
		if (--log->refc <= 0) {
#ifdef NT
			DeleteCriticalSection(&log->lock);
#endif
#ifdef UNIX
			pthread_mutex_destroy(&log->lock);
#endif
			free(log);
		}
	}
	return NULL;
}

/* Set the debug level. */
void a1log_debug(a1log *log, int level) {
	if (log != NULL) {
		log->debug = level;
	}
}

/* Set the vebosity level. */
void a1log_verb(a1log *log, int level) {
	if (log != NULL) {
		log->verb = level;
	}
}

/* Set the tag. Note that the tage string is NOT copied, just referenced */
void a1log_tag(a1log *log, char *tag) {
	if (log != NULL) {
		log->tag = tag;
	}
}

/* Log a verbose message if level >= verb */
void a1logv(a1log *log, int level, char *fmt, ...) {

	if (log != NULL) {
		if (log->verb >= level) {
			va_list args;

			A1LOG_LOCK(log, 0);
			va_start(args, fmt);
			log->logv(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
	}
}

/* Log a debug message if level >= debug */
void a1logd(a1log *log, int level, char *fmt, ...) {
	if (log != NULL) {
		if (log->debug >= level) {
			va_list args;
	
			A1LOG_LOCK(log, 1);
			va_start(args, fmt);
			log->logd(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
	}
}

/* log a warning message to the verbose, debug and error output, */
void a1logw(a1log *log, char *fmt, ...) {
	if (log != NULL) {
		va_list args;
	
		/* log to all the outputs, but only log once */
		A1LOG_LOCK(log, 0);
		va_start(args, fmt);
		log->loge(log->cntx, log, fmt, args);
		va_end(args);
		A1LOG_UNLOCK(log);
		if (log->logd != log->loge) {
			A1LOG_LOCK(log, 1);
			va_start(args, fmt);
			log->logd(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
		if (log->logv != log->loge && log->logv != log->logd) {
			A1LOG_LOCK(log, 0);
			va_start(args, fmt);
			log->logv(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
	}
}

/* log an error message to the verbose, debug and error output, */
/* and latch the error if it is the first. */
/* ecode = system, icoms or instrument error */
void a1loge(a1log *log, int ecode, char *fmt, ...) {
	if (log != NULL) {
		va_list args;
	
		if (log->errc == 0) {
			A1LOG_LOCK(log, 0);
			log->errc = ecode;
			va_start(args, fmt);
			vsnprintf(log->errm, A1_LOG_BUFSIZE, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
		va_start(args, fmt);
		/* log to all the outputs, but only log once */
		A1LOG_LOCK(log, 0);
		va_start(args, fmt);
		log->loge(log->cntx, log, fmt, args);
		va_end(args);
		A1LOG_UNLOCK(log);
		if (log->logd != log->loge) {
			A1LOG_LOCK(log, 1);
			va_start(args, fmt);
			log->logd(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
		if (log->logv != log->loge && log->logv != log->logd) {
			A1LOG_LOCK(log, 0);
			va_start(args, fmt);
			log->logv(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
	}
}

/* Unlatch an error message. */
/* This just resets errc and errm */
void a1logue(a1log *log) {
	if (log != NULL) {
		log->errc = 0;
		log->errm[0] = '\000';
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Print bytes as hex to debug log */
/* base is the base of the displayed offset */
void adump_bytes(a1log *log, char *pfx, unsigned char *buf, int base, int len) {
	int i, j, ii;
	char oline[200] = { '\000' }, *bp = oline;
	if (pfx == NULL)
		pfx = "";
	for (i = j = 0; i < len; i++) {
		if ((i % 16) == 0)
			bp += sprintf(bp,"%s%04x:",pfx,base+i);
		bp += sprintf(bp," %02x",buf[i]);
		if ((i+1) >= len || ((i+1) % 16) == 0) {
			for (ii = i; ((ii+1) % 16) != 0; ii++)
				bp += sprintf(bp,"   ");
			bp += sprintf(bp,"  ");
			for (; j <= i; j++) {
				if (!(buf[j] & 0x80) && isprint(buf[j]))
					bp += sprintf(bp,"%c",buf[j]);
				else
					bp += sprintf(bp,".");
			}
			bp += sprintf(bp,"\n");
			a1logd(log,0,"%s",oline);
			bp = oline;
		}
	}
}

/******************************************************************/
/* Default verbose/warning/error output routines                  */
/* These fall through to, and can be re-director using the        */
/* above log class.                                               */
/******************************************************************/

/* Some utilities to allow us to format output to log functions */
/* (Caller aquires lock) */
static void g_logv(char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	g_log->logv(g_log->cntx, g_log, fmt, args);
	va_end(args);
}

static void g_loge(char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	g_log->loge(g_log->cntx, g_log, fmt, args);
	va_end(args);
}

void
verbose(int level, char *fmt, ...) {
	if (g_log->verb >= level) {
		va_list args;

		A1LOG_LOCK(g_log, 0);
		g_logv("%s: ",g_log->tag);
		va_start(args, fmt);
		g_log->logv(g_log->cntx, g_log, fmt, args);
		va_end(args);
		g_logv("\n");
		A1LOG_UNLOCK(g_log);
	}
}

void
warning(char *fmt, ...) {
	va_list args;

	A1LOG_LOCK(g_log, 0);
	g_loge("%s: Warning - ",g_log->tag);
	va_start(args, fmt);
	g_log->loge(g_log->cntx, g_log, fmt, args);
	va_end(args);
	g_loge("\n");
	A1LOG_UNLOCK(g_log);
}

ATTRIBUTE_NORETURN void
error(char *fmt, ...) {
	va_list args;

	A1LOG_LOCK(g_log, 0);
	g_loge("%s: Error - ",g_log->tag);
	va_start(args, fmt);
	g_log->loge(g_log->cntx, g_log, fmt, args);
	va_end(args);
	g_loge("\n");
	A1LOG_UNLOCK(g_log);

	exit(1);
}


/******************************************************************/
/* Suplimental allcation functions */
/******************************************************************/

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)(-1))
#endif

/* a * b */
static size_t ssat_mul(size_t a, size_t b) {
	size_t c;

	if (a == 0 || b == 0)
		return 0;

	if (a > (SIZE_MAX/b))
		return SIZE_MAX;
	else
		return a * b;
}

/* reallocate and clear new allocation */
void *recalloc(		/* Return new address */
void *ptr,					/* Current address */
size_t cnum,				/* Current number and unit size */
size_t csize,
size_t nnum,				/* New number and unit size */
size_t nsize
) {
	int ind = 0;
	size_t ctot, ntot;

	if (ptr == NULL)
		return calloc(nnum, nsize); 

	if ((ntot = ssat_mul(nnum, nsize)) == SIZE_MAX)
		return NULL;			/* Overflow */

	if ((ctot = ssat_mul(cnum, csize)) == SIZE_MAX)
		return NULL;			/* Overflow */

	ptr = realloc(ptr, ntot);

	if (ptr != NULL && ntot > ctot)
		memset((char *)ptr + ctot, 0, ntot - ctot);			/* Clear the new region */

	return ptr;
}

/******************************************************************/
/* OS X App Nap fixes                                             */
/******************************************************************/

#if defined(__APPLE__)

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
# include <objc/runtime.h>
# include <objc/message.h>
#else
# include <objc/objc-runtime.h>
#endif
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
# include <objc/objc-auto.h>
#endif

/*
	OS X 10.9+ App Nap problems bug:

	<http://stackoverflow.com/questions/22784886/what-can-make-nanosleep-drift-with-exactly-10-sec-on-mac-os-x-10-9>

	NSProcessInfo variables:

	<https://developer.apple.com/library/prerelease/ios/documentation/Cocoa/Reference/Foundation/Classes/NSProcessInfo_Class/#//apple_ref/c/tdef/NSActivityOptions>

	 typedef enum : uint64_t { NSActivityIdleDisplaySleepDisabled = (1ULL << 40),
		NSActivityIdleSystemSleepDisabled = (1ULL << 20),
		NSActivitySuddenTerminationDisabled = (1ULL << 14),
		NSActivityAutomaticTerminationDisabled = (1ULL << 15),
		NSActivityUserInitiated = (0x00FFFFFFULL | NSActivityIdleSystemSleepDisabled ),
		NSActivityUserInitiatedAllowingIdleSystemSleep =
		           (NSActivityUserInitiated & ~NSActivityIdleSystemSleepDisabled ),
		NSActivityBackground = 0x000000FFULL,
		NSActivityLatencyCritical = 0xFF00000000ULL,
	} NSActivityOptions; 

	See <http://stackoverflow.com/questions/19847293/disable-app-nap-in-macos-10-9-mavericks-application>:

	@property (strong) id activity;

	if ([[NSProcessInfo processInfo] respondsToSelector:@selector(beginActivityWithOptions:reason:)]) {
    self.activity = [[NSProcessInfo processInfo] beginActivityWithOptions:0x00FFFFFF reason:@"receiving OSC messages"];
}

	<http://stackoverflow.com/questions/19671197/disabling-app-nap-with-beginactivitywithoptions>

	NSProcessInfo = interface(NSObject)['{B96935F6-3809-4A49-AD4F-CBBAB0F2C961}']
	function beginActivityWithOptions(options: NSActivityOptions; reason: NSString): NSObject; cdecl;

	<http://stackoverflow.com/questions/22164571/weird-behaviour-of-dispatch-after>

*/

static int osx_userinitiated_cnt = 0;
static id osx_userinitiated_activity = nil;

/* Tell App Nap that this is user initiated */
void osx_userinitiated_start() {
	Class pic;		/* Process info class */
	SEL pis;		/* Process info selector */
	SEL bawo;		/* Begin Activity With Options selector */
	id pi;			/* Process info */
	id str;

	if (osx_userinitiated_cnt++ != 0)
		return;

	a1logd(g_log, 7, "OS X - User Initiated Activity start\n");
	
	/* We have to be conservative to avoid triggering an exception when run on older OS X, */
	/* since beginActivityWithOptions is only available in >= 10.9 */
	if ((pic = (Class)objc_getClass("NSProcessInfo")) == nil) {
		return;
	}

	if (class_getClassMethod(pic, (pis = sel_getUid("processInfo"))) == NULL) { 
		return;
	}

	if (class_getInstanceMethod(pic, (bawo = sel_getUid("beginActivityWithOptions:reason:"))) == NULL) {
		a1logd(g_log, 7, "OS X - beginActivityWithOptions not supported\n");
		return;
	}

	/* Get the process instance */
	if ((pi = objc_msgSend((id)pic, pis)) == nil) {
		return;
	}

	/* Create a reason string */
	str = objc_msgSend((id)objc_getClass("NSString"), sel_getUid("alloc"));
	str = objc_msgSend(str, sel_getUid("initWithUTF8String:"), "ArgyllCMS");
			
	/* Start activity that tells App Nap to mind its own business. */
	/* NSActivityUserInitiatedAllowingIdleSystemSleep */
	osx_userinitiated_activity = objc_msgSend(pi, bawo, 0x00FFFFFFULL, str);
}

/* Done with user initiated */
void osx_userinitiated_end() {
	if (osx_userinitiated_cnt > 0) {
		osx_userinitiated_cnt--;
		if (osx_userinitiated_cnt == 0 && osx_userinitiated_activity != nil) {
			a1logd(g_log, 7, "OS X - User Initiated Activity end");
			objc_msgSend(
			             objc_msgSend((id)objc_getClass("NSProcessInfo"),
			             sel_getUid("processInfo")), sel_getUid("endActivity:"),
			             osx_userinitiated_activity);
			osx_userinitiated_activity = nil;
		}
	}
}

static int osx_latencycritical_cnt = 0;
static id osx_latencycritical_activity = nil;

/* Tell App Nap that this is latency critical */
void osx_latencycritical_start() {
	Class pic;		/* Process info class */
	SEL pis;		/* Process info selector */
	SEL bawo;		/* Begin Activity With Options selector */
	id pi;		/* Process info */
	id str;

	if (osx_latencycritical_cnt++ != 0)
		return;

	a1logd(g_log, 7, "OS X - Latency Critical Activity start\n");
	
	/* We have to be conservative to avoid triggering an exception when run on older OS X */
	if ((pic = (Class)objc_getClass("NSProcessInfo")) == nil) {
		return;
	}

	if (class_getClassMethod(pic, (pis = sel_getUid("processInfo"))) == NULL) { 
		return;
	}

	if (class_getInstanceMethod(pic, (bawo = sel_getUid("beginActivityWithOptions:reason:"))) == NULL) {
		a1logd(g_log, 7, "OS X - beginActivityWithOptions not supported\n");
		return;
	}

	/* Get the process instance */
	if ((pi = objc_msgSend((id)pic, pis)) == nil) {
		return;
	}

	/* Create a reason string */
	str = objc_msgSend((id)objc_getClass("NSString"), sel_getUid("alloc"));
	str = objc_msgSend(str, sel_getUid("initWithUTF8String:"), "Measuring Color");
			
	/* Start activity that tells App Nap to mind its own business. */
	/* NSActivityUserInitiatedAllowingIdleSystemSleep | NSActivityLatencyCritical */
	osx_latencycritical_activity = objc_msgSend(pi, bawo, 0x00FFFFFFULL | 0xFF00000000ULL, str);
}

/* Done with latency critical */
void osx_latencycritical_end() {
	if (osx_latencycritical_cnt > 0) {
		osx_latencycritical_cnt--;
		if (osx_latencycritical_cnt == 0 && osx_latencycritical_activity != nil) {
			a1logd(g_log, 7, "OS X - Latency Critical Activity end");
			objc_msgSend(
			             objc_msgSend((id)objc_getClass("NSProcessInfo"),
			             sel_getUid("processInfo")), sel_getUid("endActivity:"),
			             osx_latencycritical_activity);
			osx_latencycritical_activity = nil;
		}
	}
}

#endif	/* __APPLE__ */

/******************************************************************/
/* Numerical Recipes Vector/Matrix Support functions              */
/******************************************************************/
/* Note the z suffix versions return zero'd vectors/matricies */

/* Double Vector malloc/free */
double *dvector(
int nl,		/* Lowest index */
int nh		/* Highest index */
)	{
	double *v;

	if ((v = (double *) malloc((nh-nl+1) * sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dvector()");
	}
	return v-nl;
}

double *dvectorz(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	double *v;

	if ((v = (double *) calloc(nh-nl+1, sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dvector()");
	}
	return v-nl;
}

void free_dvector(
double *v,
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	if (v == NULL)
		return;

	free((char *) (v+nl));
}

/* --------------------- */
/* 2D Double matrix malloc/free */
double **dmatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (double **) malloc((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) malloc(rows * cols * sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

double **dmatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (double **) malloc((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) calloc(rows * cols, sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

void free_dmatrix(
double **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/* In case rows have been swapped, reset the pointers */
void dmatrix_reset(
double **m,
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int cols;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	cols = nch - ncl + 1;

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;
}

/* --------------------- */
/* 2D diagonal half matrix vector malloc/free */
/* A half matrix must have equal rows and columns, */
/* and the column address must always be >= than the row. */
double **dhmatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if (rows != cols) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("dhmatrix() given unequal rows and columns");
	}

	if ((m = (double **) malloc((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dhmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) malloc((rows * rows + rows)/2 * sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dhmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1, j = 1; i <= nrh; i++, j++) /* Set subsequent row addresses */
		m[i] = m[i-1] + j;			/* Start with 1 entry and increment */

	return m;
}

double **dhmatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if (rows != cols) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("dhmatrix() given unequal rows and columns");
	}

	if ((m = (double **) malloc((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dhmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) calloc((rows * rows + rows)/2, sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dhmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1, j = 1; i <= nrh; i++, j++) /* Set subsequent row addresses */
		m[i] = m[i-1] + j;			/* Start with 1 entry and increment */

	return m;
}

void free_dhmatrix(
double **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/* --------------------- */
/* matrix copy */
void copy_dmatrix(
double **dst,
double **src,
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j;
	for (j = nrl; j <= nrh; j++)
		for (i = ncl; i <= nch; i++)
			dst[j][i] = src[j][i];
}

/* Copy a matrix to a 3x3 standard C array */
void copy_dmatrix_to3x3(
double dst[3][3],
double **src,
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j;
	if ((nrh - nrl) > 2)
		nrh = nrl + 2;
	if ((nch - ncl) > 2)
		nch = ncl + 2;
	for (j = nrl; j <= nrh; j++)
		for (i = ncl; i <= nch; i++)
			dst[j][i] = src[j][i];
}

/* -------------------------------------------------------------- */
/* Convert standard C type 2D array into an indirect referenced array */
double **convert_dmatrix(
double *a,	/* base address of normal C array, ie &a[0][0] */
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j, nrow = nrh-nrl+1, ncol = nch-ncl+1;
	double **m;

	/* Allocate pointers to rows */
	if ((m = (double **) malloc(nrow * sizeof(double*))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in convert_dmatrix()");
	}

	m -= nrl;

	m[nrl] = a - ncl;
	for(i=1, j = nrl+1; i < nrow; i++, j++)
		m[j] = m[j-1] + ncol;
	/* return pointer to array of pointers */
	return m;
}

/* Free the indirect array reference (but not array) */
void free_convert_dmatrix(
double **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	free((char*) (m+nrl));
}

/* -------------------------- */
/* Float vector malloc/free */
float *fvector(
int nl,		/* Lowest index */
int nh		/* Highest index */
)	{
	float *v;

	if ((v = (float *) malloc((nh-nl+1) * sizeof(float))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in fvector()");
	}
	return v-nl;
}

float *fvectorz(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	float *v;

	if ((v = (float *) calloc(nh-nl+1, sizeof(float))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in fvector()");
	}
	return v-nl;
}

void free_fvector(
float *v,
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	if (v == NULL)
		return;

	free((char *) (v+nl));
}

/* --------------------- */
/* 2D Float matrix malloc/free */
float **fmatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	float **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (float **) malloc((rows + 1) * sizeof(float *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (float *) malloc(rows * cols * sizeof(float))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

float **fmatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	float **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (float **) malloc((rows + 1) * sizeof(float *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (float *) calloc(rows * cols, sizeof(float))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

void free_fmatrix(
float **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/* ------------------ */
/* Integer vector malloc/free */
int *ivector(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	int *v;

	if ((v = (int *) malloc((nh-nl+1) * sizeof(int))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in ivector()");
	}
	return v-nl;
}

int *ivectorz(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	int *v;

	if ((v = (int *) calloc(nh-nl+1, sizeof(int))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in ivector()");
	}
	return v-nl;
}

void free_ivector(
int *v,
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	if (v == NULL)
		return;

	free((char *) (v+nl));
}


/* ------------------------------ */
/* 2D integer matrix malloc/free */

int **imatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	int **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (int **) malloc((rows + 1) * sizeof(int *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in imatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (int *) malloc(rows * cols * sizeof(int))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in imatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

int **imatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	int **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (int **) malloc((rows + 1) * sizeof(int *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in imatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (int *) calloc(rows * cols, sizeof(int))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in imatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

void free_imatrix(
int **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/* ------------------ */
/* Short vector malloc/free */
short *svector(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	short *v;

	if ((v = (short *) malloc((nh-nl+1) * sizeof(short))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in svector()");
	}
	return v-nl;
}

short *svectorz(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	short *v;

	if ((v = (short *) calloc(nh-nl+1, sizeof(short))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in svector()");
	}
	return v-nl;
}

void free_svector(
short *v,
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	if (v == NULL)
		return;

	free((char *) (v+nl));
}


/* ------------------------------ */
/* 2D short vector malloc/free */

short **smatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	short **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (short **) malloc((rows + 1) * sizeof(short *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in smatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (short *) malloc(rows * cols * sizeof(short))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in smatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

short **smatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	short **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (short **) malloc((rows + 1) * sizeof(short *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in smatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (short *) calloc(rows * cols, sizeof(short))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in smatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

void free_smatrix(
short **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/***************************/
/* Basic matrix operations */
/***************************/

/* Transpose a 0 base matrix */
void matrix_trans(double **d, double **s, int nr,  int nc) {
	int i, j;

	for (i = 0; i < nr; i++) {
		for (j = 0; j < nc; j++) {
			d[j][i] = s[i][j];
		}
	}
}

/* Transpose a 0 base symetrical matrix in place */
void sym_matrix_trans(double **m, int n) {
	int i, j;

	for (i = 0; i < n; i++) {
		for (j = i+1; j < n; j++) {
			double tt = m[j][i]; 
			m[j][i] = m[i][j];
			m[i][j] = tt;
		}
	}
}

/* Multiply two 0 based matricies */
/* Return nz on matching error */
int matrix_mult(
	double **d,  int nr,  int nc,
	double **s1, int nr1, int nc1,
	double **s2, int nr2, int nc2
) {
	int i, j, k;

	/* s1 and s2 must mesh */
	if (nc1 != nr2)
		return 1;

	/* Output rows = s1 rows */
	if (nr != nr1)
		return 2;

	/* Output colums = s2 columns */
	if (nc != nc2)
		return 2;

	for (i = 0; i < nr1; i++) {
		for (j = 0; j < nc2; j++) { 
			d[i][j] = 0.0;  
			for (k = 0; k < nc1; k++) {
				d[i][j] += s1[i][k] * s2[k][j];
			}
		}
	}

	return 0;
}

/* Matrix multiply transpose of s1 by s2 */
/* 0 based matricies,  */
/* This is usefull for using results of lu_invert() */
int matrix_trans_mult(
	double **d,  int nr,  int nc,
	double **ts1, int nr1, int nc1,
	double **s2, int nr2, int nc2
) {
	int i, j, k;

	/* s1 and s2 must mesh */
	if (nr1 != nr2)
		return 1;

	/* Output rows = s1 rows */
	if (nr != nc1)
		return 2;

	/* Output colums = s2 columns */
	if (nc != nc2)
		return 2;

	for (i = 0; i < nc1; i++) {
		for (j = 0; j < nc2; j++) { 
			d[i][j] = 0.0;  
			for (k = 0; k < nr1; k++) {
				d[i][j] += ts1[k][i] * s2[k][j];
			}
		}
	}

	return 0;
}

/* Multiply a 0 based matrix by a vector */
/* d may be same as v */
int matrix_vect_mult(
	double *d, int nd,
	double **m, int nr, int nc,
	double *v, int nv
) {
	int i, j;
	double *_v = v, vv[20];

	if (d == v) {
		if (nv <= 20) {
			_v = vv;
		} else {
			_v = dvector(0, nv-1);
		}
		for (j = 0; j < nv; j++)
			_v[j]  = v[j];
	}

	/* Input vector must match matrix columns */
	if (nv != nc)
		return 1;

	/* Output vector must match matrix rows */
	if (nd != nr)
		return 1;

	for (i = 0; i < nd; i++) {
		d[i] = 0.0;  
		for (j = 0; j < nv; j++)
			d[i] += m[i][j] * _v[j];
	}

	if (_v != v && _v != vv)
		free_dvector(_v, 0, nv-1);

	return 0;
}

/* Multiply a 0 based transposed matrix by a vector */
/* d may be same as v */
int matrix_trans_vect_mult(
	double *d, int nd,
	double **m, int nr, int nc,
	double *v, int nv
) {
	int i, j;
	double *_v = v, vv[20];

	if (d == v) {
		if (nv <= 20) {
			_v = vv;
		} else {
			_v = dvector(0, nv-1);
		}
		for (j = 0; j < nv; j++)
			_v[j]  = v[j];
	}

	/* Input vector must match matrix columns */
	if (nv != nr)
		return 1;

	/* Output vector must match matrix rows */
	if (nd != nc)
		return 1;

	for (i = 0; i < nd; i++) {
		d[i] = 0.0;  
		for (j = 0; j < nv; j++)
			d[i] += m[j][i] * _v[j];
	}

	if (_v != v && _v != vv)
		free_dvector(_v, 0, nv-1);

	return 0;
}


/* Set zero based dvector */
void vect_set(double *d, double v, int len) {
	if (v == 0.0)
		memset((char *)d, 0, len * sizeof(double));
	else {
		int i;
		for (i = 0; i < len; i++)
			d[i] = v;
	}
}


/* Negate and copy a vector, d = -v */
/* d may be same as v */
void vect_neg(double *d, double *s, int len) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = -s[i];
}

/* Add two vectors */
/* d may be same as v */
void vect_add(
	double *d,
	double *v, int len
) {
	int i;

	for (i = 0; i < len; i++)
		d[i] += v[i];
}

/* Add two vectors, d = s1 + s2 */
void vect_add3(
	double *d, double *s1, double *s2, int len
) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = s1[i] + s2[i];
}

/* Subtract two vectors, d -= v */
/* d may be same as v */
void vect_sub(
	double *d, double *v, int len
) {
	int i;
	for (i = 0; i < len; i++)
		d[i] -= v[i];
}

/* Subtract two vectors, d = s1 - s2 */
void vect_sub3(
	double *d, double *s1, double *s2, int len
) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = s1[i] - s2[i];
}

/* Invert and copy a vector, d = 1/s */
void vect_invert(double *d, double *s, int len) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = 1.0/s[i];
}

/* Multiply the elements of two vectors, d = s1 * s2 */
void vect_mul3(
	double *d, double *s1, double *s2, int len
) {
	int i;
	for (i = 0; i < len; i++)
		d[i] = s1[i] * s2[i];
}

/* Scale a vector, */
/* d may be same as v */
void vect_scale(double *d, double *s, double scale, int len) {
	int i;

	for (i = 0; i < len; i++)
		d[i] = s[i] * scale;
}

/* Blend between s0 and s1 for bl 0..1 */
void vect_blend(double *d, double *s0, double *s1, double bl, int len) {
	int i;

	for (i = 0; i < len; i++)
		d[i] = (1.0 - bl) * s0[i] + bl * s1[i];
}

/* Scale s and add to d */
void vect_scaleadd(double *d, double *s, double scale, int len) {
	int i;

	for (i = 0; i < len; i++)
		d[i] += s[i] * scale;
}

/* Take dot product of two vectors */
double vect_dot(double *s1, double *s2, int len) {
	int i;
	double rv = 0.0;
	for (i = 0; i < len; i++)
		rv += s1[i] * s2[i];
	return rv;
}

/* Return the vectors magnitude (norm) */
double vect_mag(double *s, int len) {
	int i;
	double rv = 0.0;

	for (i = 0; i < len; i++)
		rv += s[i] * s[i];

	return sqrt(rv);
}

/* Return the magnitude (norm) of the difference between two vectors */
double vect_diffmag(double *s1, double *s2, int len) {
	int i;
	double rv = 0.0;

	for (i = 0; i < len; i++) {
		double tt = s1[i] - s2[i];
		rv += tt * tt;
	}

	return sqrt(rv);
}

/* Return the normalized vectors */
/* Return nz if norm is zero */
int vect_normalize(double *d, double *s, int len) {
	int i;
	double nv = 0.0;
	int rv = 0;

	for (i = 0; i < len; i++)
		nv += s[i] * s[i];
	nv = sqrt(nv);

	if (nv < 1e-9) {
		nv = 1.0;
		rv = 0;
	}

	for (i = 0; i < len; i++)
		d[i] = s[i]/nv;

	return rv;
}

/* Return the vectors elements maximum magnitude (+ve) */
double vect_max(double *s, int len) {
	int i;
	double rv = 0.0;

	for (i = 0; i < len; i++) {
		double tt = fabs(s[i]);
		if (tt > rv)
			rv = tt;
	}

	return rv;
}

/* Take absolute of each element */
void vect_abs(double *d, double *s, int len) {
	int i;

	for (i = 0; i < len; i++)
		d[i] = fabs(s[i]);
}

/* Take individual elements to signed power */
void vect_spow(double *d, double *s, double pv, int len) {
	int i;

	for (i = 0; i < len; i++) {
		/* pow() isn't guaranteed to behave ... */
		if (pv != 0.0) {
			if (pv < 0.0) {
				if (s[i] < 0.0)
					d[i] = 1.0/-pow(-s[i], -pv);
				else
					d[i] = 1.0/pow(s[i], -pv);
			} else {
				if (s[i] < 0.0)
					d[i] = -pow(-s[i], pv);
				else
					d[i] = pow(s[i], pv);
			}
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - */

/* Set zero based ivector */
void ivect_set(int *d, int v, int len) {
	if (v == 0)
		memset((char *)d, 0, len * sizeof(int));
	else {
		int i;
		for (i = 0; i < len; i++)
			d[i] = v;
	}
}


/* - - - - - - - - - - - - - - - - - - - - - - */

/* Print double matrix to g_log debug */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_dmatrix(a1log *log, char *id, char *pfx, double **a, int nr,  int nc) {
	int i, j;
	a1logd(g_log, 0, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		a1logd(g_log, 0, "%s ",pfx);
		for (i = 0; i < nc; i++)
			a1logd(g_log, 0, "%f%s",a[j][i], i < (nc-1) ? ", " : "");
		a1logd(g_log, 0, "\n");
	}
}

/* Print float matrix to g_log debug */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_fmatrix(a1log *log, char *id, char *pfx, float **a, int nr,  int nc) {
	int i, j;
	a1logd(g_log, 0, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		a1logd(g_log, 0, "%s ",pfx);
		for (i = 0; i < nc; i++)
			a1logd(g_log, 0, "%f%s",a[j][i], i < (nc-1) ? ", " : "");
		a1logd(g_log, 0, "\n");
	}
}

/* Print int matrix to g_log debug */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_imatrix(a1log *log, char *id, char *pfx, int **a, int nr,  int nc) {
	int i, j;
	a1logd(g_log, 0, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		a1logd(g_log, 0, "%s ",pfx);
		for (i = 0; i < nc; i++)
			a1logd(g_log, 0, "%d%s",a[j][i], i < (nc-1) ? ", " : "");
		a1logd(g_log, 0, "\n");
	}
}

/* Print short matrix to g_log debug */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_smatrix(a1log *log, char *id, char *pfx, short **a, int nr,  int nc) {
	int i, j;
	a1logd(g_log, 0, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		a1logd(g_log, 0, "%s ",pfx);
		for (i = 0; i < nc; i++)
			a1logd(g_log, 0, "%d%s",a[j][i], i < (nc-1) ? ", " : "");
		a1logd(g_log, 0, "\n");
	}
}

/* Format double matrix as C code to FILE */
/* id is variable name */
/* pfx used at start of each line */
/* hb sets horizontal element limit to wrap */
/* Assumed indexed from 0 */
void acode_dmatrix(FILE *fp, char *id, char *pfx, double **a, int nr,  int nc, int hb) {
	int i, j;
	fprintf(fp, "%sdouble %s[%d][%d] = {\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++) {
		fprintf(fp, "%s\t{ ",pfx);
		for (i = 0; i < nc; i++) {
			fprintf(fp, "%f%s",a[j][i], i < (nc-1) ? ", " : "");
			if ((i % hb) == (hb-1))
				fprintf(fp, "\n%s\t  ",pfx);
		}
		fprintf(fp, " }%s\n", j < (nr-1) ? "," : "");
	}
	fprintf(fp, "%s};\n",pfx);
}

/* Print double vector to g_log debug */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_dvector(a1log *log, char *id, char *pfx, double *a, int nc) {
	int i;
	a1logd(g_log, 0, "%s%s[%d]\n",pfx,id,nc);
	a1logd(g_log, 0, "%s ",pfx);
	for (i = 0; i < nc; i++)
		a1logd(g_log, 0, "%f%s",a[i], i < (nc-1) ? ", " : "");
	a1logd(g_log, 0, "\n");
}

/* Print float vector to g_log debug */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_fvector(a1log *log, char *id, char *pfx, float *a, int nc) {
	int i;
	a1logd(g_log, 0, "%s%s[%d]\n",pfx,id,nc);
	a1logd(g_log, 0, "%s ",pfx);
	for (i = 0; i < nc; i++)
		a1logd(g_log, 0, "%f%s",a[i], i < (nc-1) ? ", " : "");
	a1logd(g_log, 0, "\n");
}

/* Print int vector to g_log debug */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_ivector(a1log *log, char *id, char *pfx, int *a, int nc) {
	int i;
	a1logd(g_log, 0, "%s%s[%d]\n",pfx,id,nc);
	a1logd(g_log, 0, "%s ",pfx);
	for (i = 0; i < nc; i++)
		a1logd(g_log, 0, "%d%s",a[i], i < (nc-1) ? ", " : "");
	a1logd(g_log, 0, "\n");
}

/* Print short vector to g_log debug */
/* id identifies vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_svector(a1log *log, char *id, char *pfx, short *a, int nc) {
	int i;
	a1logd(g_log, 0, "%s%s[%d]\n",pfx,id,nc);
	a1logd(g_log, 0, "%s ",pfx);
	for (i = 0; i < nc; i++)
		a1logd(g_log, 0, "%d%s",a[i], i < (nc-1) ? ", " : "");
	a1logd(g_log, 0, "\n");
}

/* ============================================================================ */
/* C matrix support */

/* Clip a vector to the range 0.0 .. 1.0 */
/* and return any clipping margine */
double vect_ClipNmarg(int n, double *out, double *in) {
	int j;
	double tt, marg = 0.0;
	for (j = 0; j < n; j++) {
		out[j] = in[j];
		if (out[j] < 0.0) {
			tt = 0.0 - out[j];
			out[j] = 0.0;
			if (tt > marg)
				marg = tt;
		} else if (out[j] > 1.0) {
			tt = out[j] - 1.0;
			out[j] = 1.0;
			if (tt > marg)
				marg = tt;
		}
	}
	return marg;
}

/* 

  mat     in    out

[     ]   []    []
[     ]   []    []
[     ] * [] => []
[     ]   []    []
[     ]   []    []

 */

/* Multiply N vector by NxN transform matrix */
/* Organization is mat[out][in] */
void vect_MulByNxN(int n, double *out, double *mat, double *in) {
	int i, j;
	double _tt[20], *tt = _tt;

	if (n > 20)
		tt = dvector(0, n-1);

	for (i = 0; i < n; i++) {
		tt[i] = 0.0;
		for (j = 0; j < n; j++)
			tt[i] += mat[i * n + j] * in[j];
	}

	for (i = 0; i < n; i++)
		out[i] = tt[i];

	if (n > 20)
		free_dvector(tt, 0, n-1);
}

/* 

  mat         in    out
     N       
              []    
  [     ]     []    []
M [     ] * N [] => [] M
  [     ]     []    []
              []    

 */

/* Multiply N vector by MxN transform matrix to make M vector */
/* Organization is mat[out=M][in=N] */
void vect_MulByMxN(int n, int m, double *out, double *mat, double *in) {
	int i, j;
	double _tt[20], *tt = _tt;

	if (m > 20)
		tt = dvector(0, m-1);

	for (i = 0; i < m; i++) {
		tt[i] = 0.0;
		for (j = 0; j < n; j++)
			tt[i] += mat[i * n + j] * in[j];
	}

	for (i = 0; i < m; i++)
		out[i] = tt[i];

	if (m > 20)
		free_dvector(tt, 0, m-1);
}

/*
   in         mat       out
               M   
            [     ]    
   N        [     ]      M
[     ] * N [     ] => [   ]
            [     ]  
            [     ]    

 */

/* Multiply N vector by transposed NxM transform matrix to make M vector */
/* Organization is mat[in=N][out=M] */
void vect_MulByNxM(int n, int m, double *out, double *mat, double *in) {
	int i, j;
	double _tt[20], *tt = _tt;

	if (m > 20)
		tt = dvector(0, m-1);

	for (i = 0; i < m; i++) {
		tt[i] = 0.0;
		for (j = 0; j < n; j++)
			tt[i] += mat[j * m + i] * in[j];
	}

	for (i = 0; i < m; i++)
		out[i] = tt[i];

	if (m > 20)
		free_dvector(tt, 0, m-1);
}


/* Transpose an NxN matrix */
void matrix_TransposeNxN(int n, double *out, double *in) {
	int i, j;

	if (in != out) {
		for (i = 0; i < n; i++)
			for (j = 0; j < n; j++)
				out[i * n + j] = in[j * n + i];
	} else {
		for (i = 0; i < n; i++) {
			for (j = i+1; j < n; j++) {
				double tt;
				tt = out[i * n + j];
				out[i * n + j] = out[j * n + i];
				out[j * n + i] = tt;
			}
		}
	}
}

/* Print C double matrix to g_log debug */
/* id identifies matrix */
/* pfx used at start of each line */
/* Assumed indexed from 0 */
void adump_C_dmatrix(a1log *log, char *id, char *pfx, double *a, int nr, int nc) {
	int i, j;
	a1logd(g_log, 0, "%s%s[%d][%d]\n",pfx,id,nr,nc);

	for (j = 0; j < nr; j++, a += nc) {
		a1logd(g_log, 0, "%s ",pfx);
		for (i = 0; i < nc; i++)
			a1logd(g_log, 0, "%f%s",a[i], i < (nc-1) ? ", " : "");
		a1logd(g_log, 0, "\n");
	}
}

/*******************************************/
/* Platform independent IEE754 conversions */
/*******************************************/

/* Convert a native double to an IEEE754 encoded single precision value, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
ORD32 doubletoIEEE754(double d) {
	ORD32 sn = 0, ep = 0, ma;
	ORD32 id;

	/* Convert double to IEEE754 single precision. */
	/* This would be easy if we're running on an IEEE754 architecture, */
	/* but isn't generally portable, so we use ugly code: */

	if (d < 0.0) {
		sn = 1;
		d = -d;
	}
	if (d != 0.0) {
		int ee;
		ee = (int)floor(log(d)/log(2.0));
		if (ee < -126)			/* Allow for denormalized */
			ee = -126;
		d *= pow(0.5, (double)(ee - 23));
		ee += 127;
		if (ee < 1)				/* Too small */
			ee = 0;				/* Zero or denormalised */
		else if (ee > 254) {	/* Too large */
			ee = 255;			/* Infinity */
			d = 0.0;
		}
		ep = ee;
	} else {
		ep = 0;					/* Zero */
	}
	ma = ((ORD32)d) & ((1 << 23)-1);
	id = (sn << 31) | (ep << 23) | ma;

	return id;
}

/* Convert a an IEEE754 encoded single precision value to a native double, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
double IEEE754todouble(ORD32 ip) {
	double op;
	ORD32 sn = 0, ep = 0, ma;

	sn = (ip >> 31) & 0x1;
	ep = (ip >> 23) & 0xff;
	ma = ip & 0x7fffff;

	if (ep == 0) { 		/* Zero or denormalised */
		op = (double)ma/(double)(1 << 23);
		op *= pow(2.0, (-126.0));
	} else {
		op = (double)(ma | (1 << 23))/(double)(1 << 23);
		op *= pow(2.0, (((int)ep)-127.0));
	}
	if (sn)
		op = -op;
	return op;
}

/* Convert a native double to an IEEE754 encoded double precision value, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
ORD64 doubletoIEEE754_64(double d) {
	ORD32 sn = 0, ep = 0;
	ORD64 ma, id;

	/* Convert double to IEEE754 double precision. */
	/* This would be easy if we know we're running on an IEEE754 architecture, */
	/* but isn't generally portable, so we use ugly code: */

	if (d < 0.0) {
		sn = 1;
		d = -d;
	}
	if (d != 0.0) {
		int ee;
		ee = (int)floor(log(d)/log(2.0));
		if (ee < -1022)			/* Allow for denormalized */
			ee = -1022;
		d *= pow(0.5, (double)(ee - 52));
		ee += 1023;				/* Exponent bias */
		if (ee < 1)				/* Too small */
			ee = 0;				/* Zero or denormalised */
		else if (ee > 2046) {	/* Too large */
			ee = 2047;			/* Infinity */
			d = 0.0;
		}
		ep = ee;
	} else {
		ep = 0;					/* Zero */
	}
	ma = ((ORD64)d) & (((ORD64)1 << 52)-1);
	id = ((ORD64)sn << 63) | ((ORD64)ep << 52) | ma;

	return id;
}

/* Convert a an IEEE754 encode double precision value to a native double, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
double IEEE754_64todouble(ORD64 ip) {
	double op;
	ORD32 sn = 0, ep = 0;
	INR64 ma;

	sn = (ip >> 63) & 0x1;
	ep = (ip >> 52) & 0x7ff;
	ma = ip & (((INR64)1 << 52)-1);

	if (ep == 0) { 		/* Zero or denormalised */
		op = (double)ma/(double)((INR64)1 << 52);
		op *= pow(2.0, -1022.0);
	} else {
		op = (double)(ma | ((INR64)1 << 52))/(double)((INR64)1 << 52);
		op *= pow(2.0, (((int)ep)-1023.0));
	}
	if (sn)
		op = -op;
	return op;
}

/* Return a string representation of a 32 bit ctime. */
/* A static buffer is used. There is no \n at the end */
char *ctime_32(const INR32 *timer) {
	char *rv;
#if defined(_MSC_VER) && __MSVCRT_VERSION__ >= 0x0601
	rv = _ctime32((const __time32_t *)timer);
#else
	time_t timerv = (time_t) *timer;		/* May case to 64 bit */
	rv = ctime(&timerv);
#endif

	if (rv != NULL)
		rv[strlen(rv)-1] = '\000';

	return rv;
}

/* Return a string representation of a 64 bit ctime. */
/* A static buffer is used. There is no \n at the end */
char *ctime_64(const INR64 *timer) {
	char *rv;
#if defined(_MSC_VER) && __MSVCRT_VERSION__ >= 0x0601
	rv = _ctime64((const __time64_t *)timer);
#else
	time_t timerv;

	if (sizeof(time_t) == 4 && *timer > 0x7fffffff)
		return NULL;
	timerv = (time_t) *timer;			/* May truncate to 32 bits */
	rv = ctime(&timerv);
#endif

	if (rv != NULL)
		rv[strlen(rv)-1] = '\000';

	return rv;
}

/*******************************************/
/* Native to/from byte buffer functions    */
/*******************************************/

/* No overflow detection is done - */
/* numbers are clipped or truncated. */

/* be = Big Endian */
/* le = Little Endian */

/* - - - - - - - - */
/* Unsigned 8 bit */

unsigned int read_ORD8(ORD8 *p) {
	unsigned int rv;
	rv = ((unsigned int)p[0]);
	return rv;
}

void write_ORD8(ORD8 *p, unsigned int d) {
	if (d > 0xff)
		d = 0xff;
	p[0] = (ORD8)(d);
}

/* - - - - - - - - */
/* Signed 8 bit */

int read_INR8(ORD8 *p) {
	int rv;
	rv = (int)(INR8)p[0];
	return rv;
}

void write_INR8(ORD8 *p, int d) {
	if (d > 0x7f)
		d = 0x7f;
	else if (d < -0x80)
		d = -0x80;
	p[0] = (ORD8)(d);
}

/* - - - - - - - - */
/* Unsigned 16 bit */

unsigned int read_ORD16_be(ORD8 *p) {
	unsigned int rv;
	rv = (((unsigned int)p[0]) << 8)
	   + (((unsigned int)p[1]));
	return rv;
}

unsigned int read_ORD16_le(ORD8 *p) {
	unsigned int rv;
	rv = (((unsigned int)p[0]))
	   + (((unsigned int)p[1]) << 8);
	return rv;
}

void write_ORD16_be(ORD8 *p, unsigned int d) {
	if (d > 0xffff)
		d = 0xffff;
	p[0] = (ORD8)(d >> 8);
	p[1] = (ORD8)(d);
}

void write_ORD16_le(ORD8 *p, unsigned int d) {
	if (d > 0xffff)
		d = 0xffff;
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
}

/* - - - - - - - - */
/* Signed 16 bit */

int read_INR16_be(ORD8 *p) {
	int rv;
	rv = (((int)(INR8)p[0]) << 8)
	   + (((int)p[1]));
	return rv;
}

int read_INR16_le(ORD8 *p) {
	int rv;
	rv = (((int)p[0]))
	   + (((int)(INR8)p[1]) << 8);
	return rv;
}

void write_INR16_be(ORD8 *p, int d) {
	if (d > 0x7fff)
		d = 0x7fff;
	else if (d < -0x8000)
		d = -0x8000;
	p[0] = (ORD8)(d >> 8);
	p[1] = (ORD8)(d);
}

void write_INR16_le(ORD8 *p, int d) {
	if (d > 0x7fff)
		d = 0x7fff;
	else if (d < -0x8000)
		d = -0x8000;
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
}

/* - - - - - - - - */
/* Unsigned 32 bit */

unsigned int read_ORD32_be(ORD8 *p) {
	unsigned int rv;
	rv = (((unsigned int)p[0]) << 24)
	   + (((unsigned int)p[1]) << 16)
	   + (((unsigned int)p[2]) << 8)
	   + (((unsigned int)p[3]));
	return rv;
}

unsigned int read_ORD32_le(ORD8 *p) {
	unsigned int rv;
	rv = (((unsigned int)p[0]))
	   + (((unsigned int)p[1]) << 8)
	   + (((unsigned int)p[2]) << 16)
	   + (((unsigned int)p[3]) << 24);
	return rv;
}

void write_ORD32_be(ORD8 *p, unsigned int d) {
	p[0] = (ORD8)(d >> 24);
	p[1] = (ORD8)(d >> 16);
	p[2] = (ORD8)(d >> 8);
	p[3] = (ORD8)(d);
}

void write_ORD32_le(ORD8 *p, unsigned int d) {
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
	p[2] = (ORD8)(d >> 16);
	p[3] = (ORD8)(d >> 24);
}

/* - - - - - - - - */
/* Signed 32 bit */

int read_INR32_be(ORD8 *p) {
	int rv;
	rv = (((int)(INR8)p[0]) << 24)
	   + (((int)p[1]) << 16)
	   + (((int)p[2]) << 8)
	   + (((int)p[3]));
	return rv;
}

int read_INR32_le(ORD8 *p) {
	int rv;
	rv = (((int)p[0]))
	   + (((int)p[1]) << 8)
	   + (((int)p[2]) << 16)
	   + (((int)(INR8)p[3]) << 24);
	return rv;
}

void write_INR32_be(ORD8 *p, int d) {
	p[0] = (ORD8)(d >> 24);
	p[1] = (ORD8)(d >> 16);
	p[2] = (ORD8)(d >> 8);
	p[3] = (ORD8)(d);
}

void write_INR32_le(ORD8 *p, int d) {
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
	p[2] = (ORD8)(d >> 16);
	p[3] = (ORD8)(d >> 24);
}

/* - - - - - - - - */
/* Unsigned 64 bit */

ORD64 read_ORD64_be(ORD8 *p) {
	ORD64 rv;
	rv = (((ORD64)p[0]) << 56)
	   + (((ORD64)p[1]) << 48)
	   + (((ORD64)p[2]) << 40)
	   + (((ORD64)p[3]) << 32)
	   + (((ORD64)p[4]) << 24)
	   + (((ORD64)p[5]) << 16)
	   + (((ORD64)p[6]) << 8)
	   + (((ORD64)p[7]));
	return rv;
}

ORD64 read_ORD64_le(ORD8 *p) {
	ORD64 rv;
	rv = (((ORD64)p[0]))
	   + (((ORD64)p[1]) << 8)
	   + (((ORD64)p[2]) << 16)
	   + (((ORD64)p[3]) << 24)
	   + (((ORD64)p[4]) << 32)
	   + (((ORD64)p[5]) << 40)
	   + (((ORD64)p[6]) << 48)
	   + (((ORD64)p[7]) << 56);
	return rv;
}

void write_ORD64_be(ORD8 *p, ORD64 d) {
	p[0] = (ORD8)(d >> 56);
	p[1] = (ORD8)(d >> 48);
	p[2] = (ORD8)(d >> 40);
	p[3] = (ORD8)(d >> 32);
	p[4] = (ORD8)(d >> 24);
	p[5] = (ORD8)(d >> 16);
	p[6] = (ORD8)(d >> 8);
	p[7] = (ORD8)(d);
}

void write_ORD64_le(ORD8 *p, ORD64 d) {
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
	p[2] = (ORD8)(d >> 16);
	p[3] = (ORD8)(d >> 24);
	p[4] = (ORD8)(d >> 32);
	p[5] = (ORD8)(d >> 40);
	p[6] = (ORD8)(d >> 48);
	p[7] = (ORD8)(d >> 56);
}

/* - - - - - - - - */
/* Signed 64 bit */

INR64 read_INR64_be(ORD8 *p) {
	INR64 rv;
	rv = (((INR64)(INR8)p[0]) << 56)
	   + (((INR64)p[1]) << 48)
	   + (((INR64)p[2]) << 40)
	   + (((INR64)p[3]) << 32)
	   + (((INR64)p[4]) << 24)
	   + (((INR64)p[5]) << 16)
	   + (((INR64)p[6]) << 8)
	   + (((INR64)p[7]));
	return rv;
}

INR64 read_INR64_le(ORD8 *p) {
	INR64 rv;
	rv = (((INR64)p[0]))
	   + (((INR64)p[1]) << 8)
	   + (((INR64)p[2]) << 16)
	   + (((INR64)p[3]) << 24)
	   + (((INR64)p[4]) << 32)
	   + (((INR64)p[5]) << 40)
	   + (((INR64)p[6]) << 48)
	   + (((INR64)(INR8)p[7]) << 56);
	return rv;
}

void write_INR64_be(ORD8 *p, INR64 d) {
	p[0] = (ORD8)(d >> 56);
	p[1] = (ORD8)(d >> 48);
	p[2] = (ORD8)(d >> 40);
	p[3] = (ORD8)(d >> 32);
	p[4] = (ORD8)(d >> 24);
	p[5] = (ORD8)(d >> 16);
	p[6] = (ORD8)(d >> 8);
	p[7] = (ORD8)(d);
}

void write_INR64_le(ORD8 *p, INR64 d) {
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
	p[2] = (ORD8)(d >> 16);
	p[3] = (ORD8)(d >> 24);
	p[4] = (ORD8)(d >> 32);
	p[5] = (ORD8)(d >> 40);
	p[6] = (ORD8)(d >> 48);
	p[7] = (ORD8)(d >> 56);
}

/*******************************/
/* System independent timing */

#ifdef NT

/* Sleep for the given number of msec */
void msec_sleep(unsigned int msec) {
	Sleep(msec);
}

/* Return the current time in msec since */
/* the first invokation of msec_time() */
/* (Is this based on timeGetTime() ? ) */
unsigned int msec_time() {
	unsigned int rv;
	static unsigned int startup = 0;

	rv =  GetTickCount();
	if (startup == 0)
		startup = rv;

	return rv - startup;
}

/* Return the current time in usec */
/* since the first invokation of usec_time() */
/* Return -1.0 if not available */
double usec_time() {
	double rv;
	LARGE_INTEGER val;
	static double scale = 0.0;
	static LARGE_INTEGER startup;

	if (scale == 0.0) {
		if (QueryPerformanceFrequency(&val) == 0)
			return -1.0;
		scale = 1000000.0/val.QuadPart;
		QueryPerformanceCounter(&val);
		startup.QuadPart = val.QuadPart;

	} else {
		QueryPerformanceCounter(&val);
	}
	val.QuadPart -= startup.QuadPart;

	rv = val.QuadPart * scale;
		
	return rv;
}

#endif /* NT */

#if defined(UNIX)

/* Sleep for the given number of msec */
/* (Note that OS X 10.9+ App Nap can wreck this, unless */
/*  it is turned off.) */
void msec_sleep(unsigned int msec) {
#ifdef NEVER
	if (msec > 1000) {
		unsigned int secs;
		secs = msec / 1000;
		msec = msec % 1000;
		sleep(secs);
	}
	usleep(msec * 1000);
#else
	struct timespec ts;

	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000000;
	nanosleep(&ts, NULL);
#endif
}


#if defined(__APPLE__) /* && !defined(CLOCK_MONOTONIC) */

#include <mach/mach_time.h>

/* Return the current time in msec */
/* since the first invokation of msec_time() */
unsigned int msec_time() {
    mach_timebase_info_data_t timebase;
    static uint64_t startup = 0;
    uint64_t time;
	double msec;

    time = mach_absolute_time();
	if (startup == 0)
		startup = time;

    mach_timebase_info(&timebase);
	time -= startup;
    msec = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e6);

    return (unsigned int)floor(msec + 0.5);
}

/* Return the current time in usec */
/* since the first invokation of usec_time() */
double usec_time() {
    mach_timebase_info_data_t timebase;
    static uint64_t startup = 0;
    uint64_t time;
	double usec;

    time = mach_absolute_time();
	if (startup == 0)
		startup = time;

    mach_timebase_info(&timebase);
	time -= startup;
    usec = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e3);

    return usec;
}

#else

/* Return the current time in msec */
/* since the first invokation of msec_time() */
unsigned int msec_time() {
	unsigned int rv;
	static struct timespec startup = { 0, 0 };
	struct timespec cv;

	clock_gettime(CLOCK_MONOTONIC, &cv);

	/* Set time to 0 on first invocation */
	if (startup.tv_sec == 0 && startup.tv_nsec == 0)
		startup = cv;

	/* Subtract, taking care of carry */
	cv.tv_sec -= startup.tv_sec;
	if (startup.tv_nsec > cv.tv_nsec) {
		cv.tv_sec--;
		cv.tv_nsec += 1000000000;
	}
	cv.tv_nsec -= startup.tv_nsec;

	/* Convert nsec to msec */
	rv = cv.tv_sec * 1000 + cv.tv_nsec / 1000000;

	return rv;
}

/* Return the current time in usec */
/* since the first invokation of usec_time() */
double usec_time() {
	double rv;
	static struct timespec startup = { 0, 0 };
	struct timespec cv;

	clock_gettime(CLOCK_MONOTONIC, &cv);

	/* Set time to 0 on first invocation */
	if (startup.tv_sec == 0 && startup.tv_nsec == 0)
		startup = cv;

	/* Subtract, taking care of carry */
	cv.tv_sec -= startup.tv_sec;
	if (startup.tv_nsec > cv.tv_nsec) {
		cv.tv_sec--;
		cv.tv_nsec += 1000000000;
	}
	cv.tv_nsec -= startup.tv_nsec;

	/* Convert to usec */
	rv = cv.tv_sec * 1000000.0 + cv.tv_nsec/1000;

	return rv;
}

#endif

#endif /* UNIX */

/*******************************/
/* Debug convenience functions */
/*******************************/

#define DEB_MAX_CHAN 24

/* Print an int vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *debPiv(int di, int *p) {
	static char buf[5][DEB_MAX_CHAN * 16];
	static int ix = 0;
	int e;
	char *bp;

	if (p == NULL)
		return "(null)";

	if (++ix >= 5)
		ix = 0;
	bp = buf[ix];

	if (di > DEB_MAX_CHAN)
		di = DEB_MAX_CHAN;		/* Make sure that buf isn't overrun */

	for (e = 0; e < di; e++) {
		if (e > 0)
			*bp++ = ' ';
		sprintf(bp, "%d", p[e]); bp += strlen(bp);
	}
	return buf[ix];
}

/* Print a double color vector to a string with format. */
/* Returned static buffer is re-used every 5 calls. */
char *debPdvf(int di, char *fmt, double *p) {
	static char buf[5][DEB_MAX_CHAN * 50];
	static int ix = 0;
	int e;
	char *bp;

	if (p == NULL)
		return "(null)";

	if (fmt == NULL)
		fmt = "%.8f";

	if (++ix >= 5)
		ix = 0;
	bp = buf[ix];

	if (di > DEB_MAX_CHAN)
		di = DEB_MAX_CHAN;		/* Make sure that buf isn't overrun */

	for (e = 0; e < di; e++) {
		if (e > 0)
			*bp++ = ' ';
		sprintf(bp, fmt, p[e]); bp += strlen(bp);
	}
	return buf[ix];
}

/* Print a double color vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *debPdv(int di, double *p) {
	return debPdvf(di, NULL, p);
}

/* Print a float color vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *debPfv(int di, float *p) {
	static char buf[5][DEB_MAX_CHAN * 50];
	static int ix = 0;
	int e;
	char *bp;

	if (p == NULL)
		return "(null)";

	if (++ix >= 5)
		ix = 0;
	bp = buf[ix];

	if (di > DEB_MAX_CHAN)
		di = DEB_MAX_CHAN;		/* Make sure that buf isn't overrun */

	for (e = 0; e < di; e++) {
		if (e > 0)
			*bp++ = ' ';
		sprintf(bp, "%.8f", p[e]); bp += strlen(bp);
	}
	return buf[ix];
}

#undef DEB_MAX_CHAN
