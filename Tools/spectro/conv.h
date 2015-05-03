#ifndef CONV_H

/*
 * Some system dependent comvenience functions.
 * Implemented in unixio.c and ntio.c
 */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   2008/2/9
 *
 * Copyright 1996 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 * 
 * Derived from icoms.h
 */

#if defined (NT)
# if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0501
#  if defined _WIN32_WINNT
#   undef _WIN32_WINNT
#  endif
#  define _WIN32_WINNT 0x0501
# endif
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <io.h>
#endif

#if defined (UNIX) || defined(__APPLE__)
# include <unistd.h>
# include <glob.h>
# include <pthread.h>
#endif

#ifdef __cplusplus
	extern "C" {
#endif

/* - - - - - - - - - - - - - - - - - - -- */
/* System compatibility #defines */
#if defined (NT)

#ifndef sys_stat
# define sys_stat _stat
#endif
#ifndef sys_mkdir
# define sys_mkdir _mkdir
#endif
#ifndef sys_read
# define sys_read _read
#endif
#ifndef sys_utime
# define sys_utime _utime
# define sys_utimbuf _utimbuf
#endif
#ifndef sys_access
# define sys_access _access
#endif

#ifndef snprintf
# define snprintf _snprintf
# define vsnprintf _vsnprintf
#endif
#ifndef stricmp
# define stricmp _stricmp
#endif

#endif	/* NT */

#if defined (UNIX)

#ifndef sys_stat
# define sys_stat stat
#endif
#ifndef sys_mkdir
# define sys_mkdir mkdir
#endif
#ifndef sys_read
# define sys_read read
#endif
#ifndef sys_utime
# define sys_utime utime
# define sys_utimbuf utimbuf
#endif
#ifndef sys_access
# define sys_access access
#endif

#ifndef stricmp
# define stricmp strcasecmp
#endif

#endif	/* UNIX */

/* - - - - - - - - - - - - - - - - - - -- */
/* System dependent convenience functions */

/* wait for and then return the next character from the keyboard */
/* (If not_interactive, return getchar()) */
int next_con_char(void);

/* If there is one, return the next character from the keyboard, else return 0 */
/* (If not_interactive, always returns 0) */
int poll_con_char(void);

/* Empty the console of any pending characters */
/* (If not_interactive, does nothing) */
void empty_con_chars(void);

/* Sleep for the given number of msec */
void msec_sleep(unsigned int msec);

/* Return the current time in msec since */
/* the first invokation of msec_time() */
unsigned int msec_time();

/* Return the current time in usec */
/* the first invokation of usec_time() */
double usec_time();

/* Activate the system beeper after a delay */
/* (Note frequancy and duration may not be honoured on all systems) */
void msec_beep(int delay, int freq, int msec);

void normal_beep(); /* Emit a "normal" beep */
void good_beep(); /* Emit a "good" beep */
void bad_beep(); /* Emit a "bad" double beep */

/* - - - - - - - - - - - - - - - - - - -- */

#ifdef NEVER	/* Not currently needed, or effective */

/* Set the current threads priority */
/* return nz if this fails */
int set_interactive_priority();

int set_normal_priority();

#endif /* NEVER */

/* - - - - - - - - - - - - - - - - - - -- */
/* An Argyll mutex */

/* amutex_trylock() returns nz if it can't lock the mutex */

#ifdef NT
# define amutex CRITICAL_SECTION 
# define amutex_static(lock) CRITICAL_SECTION lock = {(void*)-1,-1 }
# define amutex_init(lock) InitializeCriticalSection(&(lock))
# define amutex_del(lock) DeleteCriticalSection(&(lock))
# define amutex_lock(lock) EnterCriticalSection(&(lock))
# define amutex_trylock(lock) (!TryEnterCriticalSection(&(lock)))
# define amutex_unlock(lock) LeaveCriticalSection(&(lock))

# define acond HANDLE
# define acond_static(cond) pthread_cond_t (cond) = PTHREAD_COND_INITIALIZER
# define acond_init(cond) (cond = CreateEvent(NULL, 0, 0, NULL))
# define acond_del(cond) CloseHandle(cond)
# define acond_wait(cond, lock) (LeaveCriticalSection(&(lock)),	\
                          WaitForSingleObject(cond, INFINITE),	\
                          EnterCriticalSection(&(lock)))
# define acond_signal(cond) SetEvent(cond)
/* + timeout version */
#endif

#ifdef UNIX

# define amutex pthread_mutex_t
# define amutex_static(lock) pthread_mutex_t (lock) = PTHREAD_MUTEX_INITIALIZER
# define amutex_init(lock) pthread_mutex_init(&(lock), NULL)
# define amutex_del(lock) pthread_mutex_destroy(&(lock))
# define amutex_lock(lock) pthread_mutex_lock(&(lock))
# define amutex_trylock(lock) pthread_mutex_trylock(&(lock))
# define amutex_unlock(lock) pthread_mutex_unlock(&(lock))

# define acond pthread_cond_t
# define acond_static(cond) pthread_cond_t (cond) = PTHREAD_COND_INITIALIZER
# define acond_init(cond) pthread_cond_init(&(cond), NULL)
# define acond_del(cond) pthread_cond_destroy(&(cond))
# define acond_wait(cond, lock) pthread_cond_wait(&(cond), &(lock))
# define acond_signal(cond) pthread_cond_signal(&(cond))
/* + timeout version */
#endif

/* - - - - - - - - - - - - - - - - - - -- */

/* An Argyll thread. */
struct _athread {
#if defined (NT)
	HANDLE th;				/* Thread */
#endif
#if defined (UNIX) || defined(__APPLE__)
	pthread_t thid;			/* Thread ID */
#endif
	int finished;			/* Set when the thread returned */
	int result;				/* Return code from thread function */

	/* Thread function to call */
	int (*function)(void *context);

	/* And the context to call it with */
	void *context;

	/* Wait for the thread to exit. Return the result */
	int (*wait)(struct _athread *p);

    /* Kill the thread and delete the object */
	/* (Killing it may have side effects, so this is a last */
	/*  resort if the thread hasn't exited) */
    void (*del)(struct _athread *p);

}; typedef struct _athread athread;

/* Create and start a thread. Return NULL on error. */
/* Thread function should only return on completion or error. */
/* It should return 0 on completion or exit, nz on error. */
athread *new_athread(int (*function)(void *context), void *context);


/* - - - - - - - - - - - - - - - - - - -- */

/* Delete a file */
void delete_file(char *fname);

/* Given the path to a file, ensure that all the parent directories */
/* are created. return nz on error */
int create_parent_directories(char *path);

/* - - - - - - - - - - - - - - - - - - -- */

struct _kkill_nproc_ctx {
	athread *th;
	char **pname;
	a1log *log;
	int stop;
	int done;
    void (*del)(struct _kkill_nproc_ctx *p);
}; typedef struct _kkill_nproc_ctx kkill_nproc_ctx;

#if defined(__APPLE__) || defined(NT)

/* Kill a list of named processes. NULL for last */
/* return < 0 if this fails. */
/* return 0 if there is no such process */
/* return 1 if a process was killed */
int kill_nprocess(char **pname, a1log *log);

/* Start a thread to constantly kill a process. */
/* Call ctx->del() when done */
kkill_nproc_ctx *kkill_nprocess(char **pname, a1log *log);

#endif /* __APPLE__ || NT */

#include "xdg_bds.h"

/* - - - - - - - - - - - - - - - - - - -- */
/* A very small subset of icclib */
#ifdef SALONEINSTLIB

typedef struct {
    double  X;
    double  Y;
    double  Z;
} sa_XYZNumber;

typedef enum {
    sa_SigXYZData                        = 0x58595A20L,  /* 'XYZ ' */
    sa_SigLabData                        = 0x4C616220L   /* 'Lab ' */
} sa_ColorSpaceSignature;

extern sa_XYZNumber sa_D50;
extern sa_XYZNumber sa_D65;
void sa_SetUnity3x3(double mat[3][3]);
void sa_Cpy3x3(double out[3][3], double mat[3][3]);
void sa_MulBy3x3(double out[3], double mat[3][3], double in[3]);
void sa_Mul3x3_2(double dst[3][3], double src1[3][3], double src2[3][3]);
int sa_Inverse3x3(double out[3][3], double in[3][3]);
void sa_Transpose3x3(double out[3][3], double in[3][3]);
void sa_Scale3(double out[3], double in[3], double rat);
double sa_LabDE(double *in0, double *in1);
void sa_Clamp3(double out[3], double in[3]);
void sa_XYZ2Lab(sa_XYZNumber *w, double *out0, double *in0);
/* Yxy to XYZ */
void sa_Yxy2XYZ(double *out, double *in);

#define icmXYZNumber sa_XYZNumber
#define icColorSpaceSignature sa_ColorSpaceSignature
#define icSigXYZData sa_SigXYZData
#define icSigLabData sa_SigLabData
#define icmD50 sa_D50
#define icmD65 sa_D65
#define icmSetUnity3x3 sa_SetUnity3x3
#define icmCpy3x3 sa_Cpy3x3
#define icmMulBy3x3 sa_MulBy3x3
#define icmMul3x3_2 sa_Mul3x3_2
#define icmInverse3x3 sa_Inverse3x3
#define icmTranspose3x3 sa_Transpose3x3
#define icmScale3 sa_Scale3
#define icmClamp3 sa_Clamp3
#define icmLabDE sa_LabDE
#define icmXYZ2Lab sa_XYZ2Lab
#define icmYxy2XYZ sa_Yxy2XYZ

/* A subset of numlib */

int sa_lu_psinvert(double **out, double **in, int m, int n);

#define lu_psinvert sa_lu_psinvert

#endif /* SALONEINSTLIB */

/* - - - - - - - - - - - - - - - - - - -- */
/* Diagnostic aids */

// Print bytes as hex to debug log */
void adump_bytes(a1log *log, char *pfx, unsigned char *buf, int base, int len);

/* - - - - - - - - - - - - - - - - - - -- */


#ifdef __cplusplus
	}
#endif

#define CONV_H
#endif /* CONV_H */
