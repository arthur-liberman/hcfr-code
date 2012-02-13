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
 * Copyright 1996 - 2008 Graeme W. Gill
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

#endif

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

#endif

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
/* the process started. */
unsigned int msec_time();

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

/* An Argyll thread. */
struct _athread {
#if defined (NT)
	HANDLE th;				/* Thread */
#endif
#if defined (UNIX) || defined(__APPLE__)
	pthread_t thid;			/* Thread ID */
#endif
	int result;				/* Return code from thread function */

	/* Thread function to call */
	int (*function)(void *context);

	/* And the context to call it with */
	void *context;

    /* Kill the thread and delete the object */
	/* (Killing it may have side effects, so this is a last */
	/*  resort if the thread hasn't exited) */
    void (*del)(struct _athread *p);

}; typedef struct _athread athread;

/* Create and start a thread */
/* Thread function should only return on completion or error. */
/* It should return 0 on completion or exit, nz on error. */
athread *new_athread(int (*function)(void *context), void *context);


/* - - - - - - - - - - - - - - - - - - -- */

/* Delete a file */
void delete_file(char *fname);

/* Given the path to a file, ensure that all the parent directories */
/* are created. return nz on error */
int create_parent_directories(char *path);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Provide a system independent glob type function */
typedef struct {
#ifdef NT
	char *base;				/* Base path */
    struct _finddata_t ffs;
	long ff;
	int first;
#else	/* UNIX */
	glob_t g;
	int rv;			/* glob return value */
	size_t ix;
#endif
	int merr;		/* NZ on malloc error */
} aglob;


/* Create the aglob for files matching the given path and pattern. */
/* Return nz on malloc error */
int aglob_create(aglob *g, char *spath);

/* Return an allocated string of the next match. */
/* Return NULL if no more matches */
char *aglob_next(aglob *g);

/* Free the aglob once we're done with it */
void aglob_cleanup(aglob *g);

/* - - - - - - - - - - - - - - - - - - -- */

struct _kkill_nproc_ctx {
	athread *th;
	char **pname;
	int debug;
	int stop;
	int done;
    void (*del)(struct _kkill_nproc_ctx *p);
}; typedef struct _kkill_nproc_ctx kkill_nproc_ctx;

#if defined(__APPLE__) || defined(NT)

/* Kill a list of named processes. NULL for last */
/* return < 0 if this fails. */
/* return 0 if there is no such process */
/* return 1 if a process was killed */
int kill_nprocess(char **pname, int debug);

/* Start a thread to constantly kill a process. */
/* Call ctx->del() when done */
kkill_nproc_ctx *kkill_nprocess(char **pname, int debug);

#endif /* __APPLE__ || NT */

#include "xdg_bds.h"

/* - - - - - - - - - - - - - - - - - - -- */
/* CCSS support */

typedef struct {
	char *path;		/* Path to the file */
	char *desc;		/* Technolofy + display description */
} iccss;

/* return a list of installed ccss files. */
/* The list is sorted by description and terminated by a NULL entry. */
/* If no is != NULL, return the number in the list */
/* Return NULL and -1 if there is a malloc error */
iccss *list_iccss(int *no);

/* Free up a iccss list */
void free_iccss(iccss *list);

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
void sa_SetUnity3x3(double mat[3][3]);
void sa_Cpy3x3(double out[3][3], double mat[3][3]);
void sa_MulBy3x3(double out[3], double mat[3][3], double in[3]);
void sa_Mul3x3_2(double dst[3][3], double src1[3][3], double src2[3][3]);
int sa_Inverse3x3(double out[3][3], double in[3][3]);
void sa_Transpose3x3(double out[3][3], double in[3][3]);
void sa_Scale3(double out[3], double in[3], double rat);


#define icmXYZNumber sa_XYZNumber
#define icColorSpaceSignature sa_ColorSpaceSignature
#define icSigXYZData sa_SigXYZData
#define icSigLabData sa_SigLabData
#define icmD50 sa_D50
#define icmSetUnity3x3 sa_SetUnity3x3
#define icmCpy3x3 sa_Cpy3x3
#define icmMulBy3x3 sa_MulBy3x3
#define icmMul3x3_2 sa_Mul3x3_2
#define icmInverse3x3 sa_Inverse3x3
#define icmTranspose3x3 sa_Transpose3x3
#define icmScale3 sa_Scale3

#endif /* SALONEINSTLIB */
/* - - - - - - - - - - - - - - - - - - -- */


#ifdef __cplusplus
	}
#endif

#define CONV_H
#endif /* CONV_H */
