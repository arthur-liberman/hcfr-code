#ifndef CONV_H

/*
 * Some system dependent convenience functions.
 * Implemented in unixio.c and ntio.c
 */

/* 
 * Argyll Color Management System
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

#if defined(UNIX)
# include <unistd.h>
# include <glob.h>
# include <pthread.h>
#endif

#ifdef __cplusplus
	extern "C" {
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

/* Activate the system beeper after a delay */
/* (Note frequency and duration may not be honoured on all systems) */
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
/* An Argyll mutex and condition */

/* amutex_trylock() returns nz if it can't lock the mutex */
/* acond_timedwait() returns nz if it times out */

/* We have to use a hack to get a static amutex to work for NT */
/* We invoke an initilizer function if we notice that it hasn't been initialized. */

/* NOTE !!!	Locks are counted, so the number of locks and unlocks have to balance, */
/* AND this means that locks only work between threads !!!! */

#ifdef NT

# define amutex CRITICAL_SECTION 
# define amutex_static_LockCount -9999		/* Sentinel value */
# define AMUTEXCHK(lock) ((lock).LockCount == amutex_static_LockCount ? amutex_chk(&(lock)) : 0)

# define amutex_static(lock) CRITICAL_SECTION lock = { NULL, amutex_static_LockCount, 0 }
# define amutex_init(lock)    InitializeCriticalSection(&(lock))
# define amutex_del(lock)     DeleteCriticalSection(&(lock))
# define amutex_lock(lock)    (AMUTEXCHK(lock), EnterCriticalSection(&(lock)))
# define amutex_trylock(lock) (AMUTEXCHK(lock), !TryEnterCriticalSection(&(lock)))
# define amutex_unlock(lock)  (AMUTEXCHK(lock), LeaveCriticalSection(&(lock)))

# define acond HANDLE
//# define acond_static(cond) pthread_cond_t (cond) = PTHREAD_COND_INITIALIZER
# define acond_init(cond) (cond = CreateEvent(NULL, 0, 0, NULL))
# define acond_del(cond) CloseHandle(cond)
# define acond_wait(cond, lock) (LeaveCriticalSection(&(lock)),	\
                          WaitForSingleObject(cond, INFINITE),	\
                          EnterCriticalSection(&(lock)))
# define acond_signal(cond) SetEvent(cond)
# define acond_timedwait(cond, lock, msec) acond_timedwait_imp(cond, &(lock), msec)

int amutex_chk(CRITICAL_SECTION *lock);

int acond_timedwait_imp(HANDLE cond, CRITICAL_SECTION *lock, int msec);

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
# define acond_timedwait(cond, lock, msec) acond_timedwait_imp(&(cond), &(lock), msec)

int acond_timedwait_imp(pthread_cond_t *cond, pthread_mutex_t *lock, int msec);

#endif


/* - - - - - - - - - - - - - - - - - - -- */

/* An Argyll thread. */
struct _athread {
#if defined (NT)
	HANDLE th;				/* Thread */
#endif
#if defined(UNIX)
	pthread_t thid;			/* Thread ID */
#endif

	/* - - - - - - - - - - */
	/* Resuable mechanics: */
	int reusable;			/* nz if thread is reusable */
	int dofinish;			/* signal thread to exit reuse loop */

	amutex startm;			/* Thread checkpoint */
	acond startc;
	int startv;

	amutex stopm;			/* Client checkpoint */
	acond stopc;
	int stopv;

	/* - - - - - - - - */

	int joined;				/* Set when the thread was joined */
	int result;				/* Return code from thread function */

	/* Thread function to call */
	int (*function)(void *context);

	/* And the context to call it with */
	void *context;


	/* If reusable, start a stopped thread. NOP if not reusable */
	void (*start)(struct _athread *p);

	/* If reusable, wait for the thread to stop. Return the result. NOP if not reusable */
	int (*wait_stop)(struct _athread *p);

	/* Wait for the thread to exit. Return the result. Causes reusable thread to exit. */
	int (*wait)(struct _athread *p);

    /* Forcefully terminate the thread. */
	/* (Termination may have side effects, so this is a last */
	/*  resort if the thread hasn't exited) */
    void (*terminate)(struct _athread *p);

	/* Wait for the thread if it has not already been waited or terminated, */
	/* and then delete the threads resources. */
    void (*del)(struct _athread *p);

}; typedef struct _athread athread;

/* Create and start a thread. Return NULL on error. */
/* Thread function should only return on completion or error. */
/* It should return 0 on completion or exit, nz on error. */

/* If reusable is nz, then thread is created in stopped mode, and */
/* can be started using ->start(). Once the function has returned, */
/* it stops again, and can be re-started using ->start(). */ 

athread *new_athread_reusable(int (*function)(void *context), void *context, int reusable);

#define new_athread(func, ctx) new_athread_reusable(func, ctx, 0)


/* - - - - - - - - - - - - - - - - - - -- */

/* Return the login $HOME directory. */
/* (Useful if we might be running sudo) */
char *login_HOME();

/* Delete a file */
void delete_file(char *fname);

/* Given the path to a file, ensure that all the parent directories */
/* are created. return nz on error */
int create_parent_directories(char *path);

/* - - - - - - - - - - - - - - - - - - -- */

/* return the number of processors */
int system_processors();

/* - - - - - - - - - - - - - - - - - - -- */

struct _kkill_nproc_ctx {
	athread *th;
	char **pname;
	a1log *log;
	int stop;
	int done;
    void (*del)(struct _kkill_nproc_ctx *p);
}; typedef struct _kkill_nproc_ctx kkill_nproc_ctx;

#if defined(UNIX_APPLE) || defined(NT)

/* Kill a list of named processes. NULL for last */
/* return < 0 if this fails. */
/* return 0 if there is no such process */
/* return 1 if a process was killed */
int kill_nprocess(char **pname, a1log *log);

/* Start a thread to constantly kill a process. */
/* Call ctx->del() when done */
kkill_nproc_ctx *kkill_nprocess(char **pname, a1log *log);

#endif /* UNIX_APPLE || NT */

#include "xdg_bds.h"

/* - - - - - - - - - - - - - - - - - - -- */
/* Some compatibility functions */

#if defined(UNIX_APPLE)
size_t osx_strnlen(const char *string, size_t maxlen);
char *osx_strndup(const char *s, size_t n);
#endif

#ifdef __cplusplus
	}
#endif

#define CONV_H
#endif /* CONV_H */
