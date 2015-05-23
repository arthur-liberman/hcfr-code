
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
#if defined (NT)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef UNIX
#include <unistd.h>
#include <sys/param.h>
#include <pthread.h>
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
		a1loge(g_log, 1, "set_exe_path: malloc %d bytes failed",i+5);
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

		if ((mh = GetModuleHandle((LPCSTR)exe_path)) == NULL) {
			a1loge(g_log, 1, "set_exe_path: GetModuleHandle '%s' failed with%d",
			                                            exe_path,GetLastError());
			exe_path[0] = '\000';
			return;
		}
		
		/* Retry until we don't truncate the returned path */
		for (pl = 100; ; pl *= 2) {
			if (tpath != NULL)
				free(tpath);
			if ((tpath = malloc(pl)) == NULL) {
				a1loge(g_log, 1, "set_exe_path: malloc %d bytes failed",pl);
				exe_path[0] = '\000';
			return;
			}
			if ((i = GetModuleFileName(mh, (LPSTR)(tpath), pl)) == 0) {
				a1loge(g_log, 1, "set_exe_path: GetModuleFileName '%s' failed with%d",
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
					a1loge(g_log, 1, "set_exe_path: Search path exceeds PATH_MAX");
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
							a1loge(g_log, 1, "set_exe_path: malloc %d bytes failed",strlen(b2)+1);
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
				a1loge(g_log, 1, "set_exe_path: malloc %d bytes failed",strlen(exe_path + i));
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

#ifdef NT
# define A1LOG_LOCK(log)									\
	if (g_log_init == 0) {									\
	    InitializeCriticalSection(&log->lock);				\
		g_log_init = 1;										\
	}														\
	EnterCriticalSection(&log->lock)
# define A1LOG_UNLOCK(log) LeaveCriticalSection(&log->lock)
#endif
#ifdef UNIX
# define A1LOG_LOCK(log)									\
	if (g_log_init == 0) {									\
	    pthread_mutex_init(&log->lock, NULL);				\
		g_log_init = 1;										\
	}														\
	pthread_mutex_lock(&log->lock)
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

/* Set the tag. Note that the tage string is NOT copied, just referenced */
void a1log_tag(a1log *log, char *tag) {
	log->tag = tag;
}

/* Log a verbose message if level >= verb */
void a1logv(a1log *log, int level, char *fmt, ...) {
	if (log != NULL) {
		if (log->verb >= level) {
			va_list args;
	
			A1LOG_LOCK(log);
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
	
			A1LOG_LOCK(log);
			va_start(args, fmt);
			log->loge(log->cntx, log, fmt, args);
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
		A1LOG_LOCK(log);
		va_start(args, fmt);
		log->loge(log->cntx, log, fmt, args);
		va_end(args);
		A1LOG_UNLOCK(log);
		if (log->logd != log->loge) {
			A1LOG_LOCK(log);
			va_start(args, fmt);
			log->logd(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
		if (log->logv != log->loge && log->logv != log->logd) {
			A1LOG_LOCK(log);
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
			A1LOG_LOCK(log);
			log->errc = ecode;
			va_start(args, fmt);
			vsnprintf(log->errm, A1_LOG_BUFSIZE, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
		va_start(args, fmt);
		/* log to all the outputs, but only log once */
		A1LOG_LOCK(log);
		va_start(args, fmt);
		log->loge(log->cntx, log, fmt, args);
		va_end(args);
		A1LOG_UNLOCK(log);
		if (log->logd != log->loge) {
			A1LOG_LOCK(log);
			va_start(args, fmt);
			log->logd(log->cntx, log, fmt, args);
			va_end(args);
			A1LOG_UNLOCK(log);
		}
		if (log->logv != log->loge && log->logv != log->logd) {
			A1LOG_LOCK(log);
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

		A1LOG_LOCK(g_log);
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

	A1LOG_LOCK(g_log);
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

	A1LOG_LOCK(g_log);
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
//	size_t c;

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
/* 2D Double vector malloc/free */
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
/* 2D vector copy */
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

/* Diagnostic - print to g_log debug */
void matrix_print(char *c, double **a, int nr,  int nc) {
	int i, j;
	a1logd(g_log, 0, "%s, %d x %d\n",c,nr,nc);

	for (j = 0; j < nr; j++) {
		a1logd(g_log, 0, " ");
		for (i = 0; i < nc; i++) {
			a1logd(g_log, 0, " %.2f",a[j][i]);
		}
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

void write_ORD8(unsigned int d, ORD8 *p) {
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

void write_INR8(int d, ORD8 *p) {
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

void write_ORD16_be(unsigned int d, ORD8 *p) {
	if (d > 0xffff)
		d = 0xffff;
	p[0] = (ORD8)(d >> 8);
	p[1] = (ORD8)(d);
}

void write_ORD16_le(unsigned int d, ORD8 *p) {
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

void write_INR16_be(int d, ORD8 *p) {
	if (d > 0x7fff)
		d = 0x7fff;
	else if (d < -0x8000)
		d = -0x8000;
	p[0] = (ORD8)(d >> 8);
	p[1] = (ORD8)(d);
}

void write_INR16_le(int d, ORD8 *p) {
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

void write_ORD32_be(unsigned int d, ORD8 *p) {
	p[0] = (ORD8)(d >> 24);
	p[1] = (ORD8)(d >> 16);
	p[2] = (ORD8)(d >> 8);
	p[3] = (ORD8)(d);
}

void write_ORD32_le(unsigned int d, ORD8 *p) {
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

void write_INR32_be(int d, ORD8 *p) {
	p[0] = (ORD8)(d >> 24);
	p[1] = (ORD8)(d >> 16);
	p[2] = (ORD8)(d >> 8);
	p[3] = (ORD8)(d);
}

void write_INR32_le(int d, ORD8 *p) {
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

void write_ORD64_be(ORD64 d, ORD8 *p) {
	p[0] = (ORD8)(d >> 56);
	p[1] = (ORD8)(d >> 48);
	p[2] = (ORD8)(d >> 40);
	p[3] = (ORD8)(d >> 32);
	p[4] = (ORD8)(d >> 24);
	p[5] = (ORD8)(d >> 16);
	p[6] = (ORD8)(d >> 8);
	p[7] = (ORD8)(d);
}

void write_ORD64_le(ORD64 d, ORD8 *p) {
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

void write_INR64_be(INR64 d, ORD8 *p) {
	p[0] = (ORD8)(d >> 56);
	p[1] = (ORD8)(d >> 48);
	p[2] = (ORD8)(d >> 40);
	p[3] = (ORD8)(d >> 32);
	p[4] = (ORD8)(d >> 24);
	p[5] = (ORD8)(d >> 16);
	p[6] = (ORD8)(d >> 8);
	p[7] = (ORD8)(d);
}

void write_INR64_le(INR64 d, ORD8 *p) {
	p[0] = (ORD8)(d);
	p[1] = (ORD8)(d >> 8);
	p[2] = (ORD8)(d >> 16);
	p[3] = (ORD8)(d >> 24);
	p[4] = (ORD8)(d >> 32);
	p[5] = (ORD8)(d >> 40);
	p[6] = (ORD8)(d >> 48);
	p[7] = (ORD8)(d >> 56);
}

