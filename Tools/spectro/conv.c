
 /* Platform isolation convenience functions */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   28/9/97
 *
 * Copyright 1997 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#ifdef NT
# if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0501
#  define _WIN32_WINNT 0x0501
# endif
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#include <conio.h>
#include <tlhelp32.h>
#include <direct.h>
#endif

#if defined(UNIX)
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

/* select() defined, but not poll(), so emulate poll() */
#if defined(FD_CLR) && !defined(POLLIN)
#include "pollem.h"
#define poll_x pollem
#else
#include <sys/poll.h>	/* Else assume poll() is native */
#define poll_x poll
#endif
#endif

#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#else
#include "sa_config.h"
#endif
#include "numsup.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"

#ifdef __APPLE__
//#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOBSD.h>
#include <mach/mach_init.h>
#include <mach/task_policy.h>
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
# include <AudioToolbox/AudioServices.h>
#endif
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
# include <objc/objc-auto.h>
#endif
#endif /* __APPLE__ */

#undef DEBUG

#ifdef DEBUG
# define DBG(xx)	a1logd(g_log, 0, xx )
# define DBGA g_log, 0 		/* First argument to DBGF() */
# define DBGF(xx)	a1logd xx
#else
# define DBG(xx)
# define DBGF(xx)
#endif

#ifdef __BORLANDC__
#define _kbhit kbhit
#endif

/* ============================================================= */
/*                           MS WINDOWS                          */  
/* ============================================================= */
#ifdef NT

/* wait for and then return the next character from the keyboard */
/* (If not_interactive, return getchar()) */
int next_con_char(void) {
	int c;

	if (not_interactive) {
		HANDLE stdinh;
  		char buf[1];
		DWORD bread;

		if ((stdinh = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE) {
			return 0;
		}
	
		/* Ignore end of line characters */
		for (;;) {
			if (ReadFile(stdinh, buf, 1, &bread, NULL)
			 && bread == 1
			 && buf[0] != '\r' && buf[0] != '\n') { 
				return buf[0];
			}
		}
	}

	c = _getch();
	return c;
}

/* Horrible hack to poll stdin when we're not interactive */
static int th_read_char(void *pp) {
	char *rp = (char *)pp;
	HANDLE stdinh;
  	char buf[1];
	DWORD bread;

	if ((stdinh = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE)
		return 0;

	if (ReadFile(stdinh, buf, 1, &bread, NULL)
	 && bread == 1
	 && buf[0] != '\r' && buf[0] != '\n') { 
		*rp = buf[0];
	}

	return 0;
}

/* If there is one, return the next character from the keyboard, else return 0 */
/* (If not_interactive, always returns 0) */
int poll_con_char(void) {

	if (not_interactive) {		/* Can't assume that it's the console */
		athread *getch_thread = NULL;
		char c = 0;
	
		/* This is pretty horrible. The problem is that we can't use */
		/* any of MSWin's async file read functions, because we */
		/* have no way of ensuring that the STD_INPUT_HANDLE has been */
		/* opened with FILE_FLAG_OVERLAPPED. Used a thread instead... */
		if ((getch_thread = new_athread(th_read_char, &c)) != NULL) {
			HANDLE stdinh;

			if ((stdinh = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE) {
				return 0;
			}

			Sleep(1);			/* We just hope 1 msec is enough for the thread to start */
			CancelIo(stdinh);
			getch_thread->del(getch_thread);
			return c;
		}
		return 0;
	}

	/* Assume it's the console */
	if (_kbhit() != 0) {
		int c = next_con_char();
		return c;
	}
	return 0; 
}

/* Suck all characters from the keyboard */
/* (If not_interactive, does nothing) */
void empty_con_chars(void) {

	if (not_interactive) {
		return;
	}

	Sleep(50);					/* _kbhit seems to have a bug */
	while (_kbhit()) {
		if (next_con_char() == 0x3)	/* ^C Safety */
			break;
	}
}

/* Sleep for the given number of seconds */
void sleep(unsigned int secs) {
	Sleep(secs * 1000);
}

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

static athread *beep_thread = NULL;
static int beep_delay;
static int beep_freq;
static int beep_msec;

/* Delayed beep handler */
static int delayed_beep(void *pp) {
	msec_sleep(beep_delay);
	Beep(beep_freq, beep_msec);
	return 0;
}

/* Activate the system beeper */
void msec_beep(int delay, int freq, int msec) {
	if (delay > 0) {
		if (beep_thread != NULL)
			beep_thread->del(beep_thread);
		beep_delay = delay;
		beep_freq = freq;
		beep_msec = msec;
		if ((beep_thread = new_athread(delayed_beep, NULL)) == NULL)
			a1logw(g_log, "msec_beep: Delayed beep failed to create thread\n");
	} else {
		Beep(freq, msec);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef NEVER    /* Not currently needed, or effective */

/* Set the current threads priority */
int set_interactive_priority() {
	if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) == 0)
		return 1;
	return 0;
}

int set_normal_priority() {
	if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL) == 0)
		return 1;
	return 0;
}

#endif /* NEVER */


#undef USE_BEGINTHREAD

/* Wait for the thread to exit. Return the result */
static int athread_wait(struct _athread *p) {

	if (p->finished)
		return p->result;

	WaitForSingleObject(p->th, INFINITE);

	return p->result;
}

/* Destroy the thread */
static void athread_del(
athread *p
) {
	DBG("athread_del called\n");

	if (p == NULL)
		return;

	if (p->th != NULL) {
		if (!p->finished) {
			DBG("athread_del calling TerminateThread() because thread hasn't finished\n");
			TerminateThread(p->th, (DWORD)-1);		/* But it is worse to leave it hanging around */
		}
		CloseHandle(p->th);
	}

	free(p);
}

/* _beginthread doesn't leak memory, but */
/* needs to be linked to a different library */
#ifdef USE_BEGINTHREAD
/* Thread function */
static void __cdecl threadproc(
	void *lpParameter
) {
#else
DWORD WINAPI threadproc(
	LPVOID lpParameter
) {
#endif
	athread *p = (athread *)lpParameter;

	p->result = p->function(p->context);
	p->finished = 1;
#ifdef USE_BEGINTHREAD
#else
	return 0;
#endif
}
 

athread *new_athread(
	int (*function)(void *context),
	void *context
) {
	athread *p = NULL;

	DBG("new_athread called\n");

	if ((p = (athread *)calloc(sizeof(athread), 1)) == NULL) {
		a1loge(g_log, 1, "new_athread: calloc failed\n");
		return NULL;
	}

	p->function = function;
	p->context = context;
	p->wait = athread_wait;
	p->del = athread_del;

	/* Create a thread */
#ifdef USE_BEGINTHREAD
	p->th = _beginthread(threadproc, 0, (void *)p);
	if (p->th == -1) {
#else
	p->th = CreateThread(NULL, 0, threadproc, (void *)p, 0, NULL);
	if (p->th == NULL) {
#endif
		a1loge(g_log, 1, "new_athread: CreateThread failed with %d\n",GetLastError());
		p->th = NULL;
		athread_del(p);
		return NULL;
	}

	DBG("new_athread returning OK\n");
	return p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Delete a file */
void delete_file(char *fname) {
	_unlink(fname);
}

/* Given the path to a file, ensure that all the parent directories */
/* are created. return nz on error */
int create_parent_directories(char *path) {
	struct _stat sbuf;
	char *pp = path;

	if (*pp != '\000'		/* Skip drive number */
		&& ((*pp >= 'a' && *pp <= 'z') || (*pp >= 'A' && *pp <= 'Z'))
	    && pp[1] == ':')
		pp += 2;
	if (*pp == '/')
		pp++;			/* Skip root directory */
	for (;pp != NULL && *pp != '\000';) {
		if ((pp = strchr(pp, '/')) != NULL) {
			*pp = '\000';
			if (_stat(path,&sbuf) != 0)
			{
				if (_mkdir(path) != 0)
					return 1;
			}
			*pp = '/';
			pp++;
		}
	}
	return 0;
}

#endif /* NT */


/* ============================================================= */
/*                          UNIX/OS X                            */
/* ============================================================= */

#if defined(UNIX)

/* Wait for and return the next character from the keyboard */
/* (If not_interactive, return getchar()) */
int next_con_char(void) {
	struct pollfd pa[1];		/* Poll array to monitor stdin */
	struct termios origs, news;
	char rv = 0;

	if (!not_interactive) {
		/* Configure stdin to be ready with just one character */
		if (tcgetattr(STDIN_FILENO, &origs) < 0)
			a1logw(g_log, "next_con_char: tcgetattr failed with '%s' on stdin", strerror(errno));
		news = origs;
		news.c_lflag &= ~(ICANON | ECHO);
		news.c_cc[VTIME] = 0;
		news.c_cc[VMIN] = 1;
		if (tcsetattr(STDIN_FILENO,TCSANOW, &news) < 0)
			a1logw(g_log, "next_con_char: tcsetattr failed with '%s' on stdin", strerror(errno));
	}

	/* Wait for stdin to have a character */
	pa[0].fd = STDIN_FILENO;
	pa[0].events = POLLIN | POLLPRI;
	pa[0].revents = 0;

	if (poll_x(pa, 1, -1) > 0
	 && (pa[0].revents == POLLIN
		 || pa[0].revents == POLLPRI)) {
		char tb[3];
		if (read(STDIN_FILENO, tb, 1) > 0) {	/* User hit a key */
			rv = tb[0] ;
		}
	} else {
		if (!not_interactive)
			tcsetattr(STDIN_FILENO, TCSANOW, &origs);
		a1logw(g_log, "next_con_char: poll on stdin returned unexpected value 0x%x",pa[0].revents);
	}

	/* Restore stdin */
	if (!not_interactive && tcsetattr(STDIN_FILENO, TCSANOW, &origs) < 0) {
		a1logw(g_log, "next_con_char: tcsetattr failed with '%s' on stdin", strerror(errno));
	}

	return rv;
}

/* If here is one, return the next character from the keyboard, else return 0 */
/* (If not_interactive, always returns 0) */
int poll_con_char(void) {
	struct pollfd pa[1];		/* Poll array to monitor stdin */
	struct termios origs, news;
	char rv = 0;

	if (!not_interactive) {
		/* Configure stdin to be ready with just one character */
		if (tcgetattr(STDIN_FILENO, &origs) < 0)
			a1logw(g_log, "poll_con_char: tcgetattr failed with '%s' on stdin", strerror(errno));
		news = origs;
		news.c_lflag &= ~(ICANON | ECHO);
		news.c_cc[VTIME] = 0;
		news.c_cc[VMIN] = 1;
		if (tcsetattr(STDIN_FILENO,TCSANOW, &news) < 0)
			a1logw(g_log, "poll_con_char: tcsetattr failed with '%s' on stdin", strerror(errno));
	}

	/* Wait for stdin to have a character */
	pa[0].fd = STDIN_FILENO;
	pa[0].events = POLLIN | POLLPRI;
	pa[0].revents = 0;

	if (poll_x(pa, 1, 0) > 0
	 && (pa[0].revents == POLLIN
		 || pa[0].revents == POLLPRI)) {
		char tb[3];
		if (read(STDIN_FILENO, tb, 1) > 0) {	/* User hit a key */
			rv = tb[0] ;
		}
	}

	/* Restore stdin */
	if (!not_interactive && tcsetattr(STDIN_FILENO, TCSANOW, &origs) < 0)
		a1logw(g_log, "poll_con_char: tcsetattr failed with '%s' on stdin", strerror(errno));

	return rv;
}

/* Suck all characters from the keyboard */
/* (If not_interactive, does nothing) */
void empty_con_chars(void) {
	if (not_interactive)
		return;

	tcflush(STDIN_FILENO, TCIFLUSH);
}

/* Sleep for the given number of msec */
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

/* Provide substitute for clock_gettime() in OS X */

#if defined(__APPLE__) && !defined(CLOCK_MONOTONIC)
#include <mach/mach_time.h>
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
static int clock_gettime(int clk_id, struct timespec *t){
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    uint64_t time;
    time = mach_absolute_time();
    double nseconds = ((double)time * (double)timebase.numer)/((double)timebase.denom);
    double seconds = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e9);
    t->tv_sec = seconds;
    t->tv_nsec = nseconds;
    return 0;
}
#endif

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
		cv.tv_nsec += 100000000;
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

/* - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef NEVER    /* Not currently needed, or effective */

/* Set the current threads priority */
int set_interactive_priority() {
#ifdef __APPLE__
#ifdef NEVER
    int rv = 0;
    struct task_category_policy tcatpolicy;

    tcatpolicy.role = TASK_FOREGROUND_APPLICATION;

    if (task_policy_set(mach_task_self(),
        TASK_CATEGORY_POLICY, (thread_policy_t)&tcatpolicy,
        TASK_CATEGORY_POLICY_COUNT) != KERN_SUCCESS)
		rv = 1;
//		a1logd(g_log, 8, "set_interactive_priority: set to forground got %d\n",rv);
	return rv;
#else
    int rv = 0;
    struct thread_precedence_policy tppolicy;

    tppolicy.importance = 500;

    if (thread_policy_set(mach_thread_self(),
        THREAD_PRECEDENCE_POLICY, (thread_policy_t)&tppolicy,
        THREAD_PRECEDENCE_POLICY_COUNT) != KERN_SUCCESS)
		rv = 1;
//		a1logd(g_log, 8, "set_interactive_priority: set to important got %d\n",rv);
	return rv;
#endif /* NEVER */
#else /* !APPLE */
	int rv;
	struct sched_param param;
	param.sched_priority = 32;

	/* This doesn't work unless we're running as su :-( */
	rv = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
//		a1logd(g_log, 8, "set_interactive_priority: set got %d\n",rv);
	return rv;
#endif /* !APPLE */
}

int set_normal_priority() {
#ifdef __APPLE__
#ifdef NEVER
    int rv = 0;
    struct task_category_policy tcatpolicy;

    tcatpolicy.role = TASK_UNSPECIFIED;

    if (task_policy_set(mach_task_self(),
        TASK_CATEGORY_POLICY, (thread_policy_t)&tcatpolicy,
        TASK_CATEGORY_POLICY_COUNT) != KERN_SUCCESS)
		rev = 1;
//		a1logd(g_log, 8, "set_normal_priority: set to normal got %d\n",rv);
#else
    int rv = 0;
    struct thread_precedence_policy tppolicy;

    tppolicy.importance = 1;

    if (thread_policy_set(mach_thread_self(),
        THREAD_STANDARD_POLICY, (thread_policy_t)&tppolicy,
        THREAD_STANDARD_POLICY_COUNT) != KERN_SUCCESS)
		rv = 1;
//		a1logd(g_log, 8, "set_normal_priority: set to standard got %d\n",rv);
	return rv;
#endif /* NEVER */
#else /* !APPLE */
	struct sched_param param;
	param.sched_priority = 0;
	int rv;

	rv = pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
//		a1logd(g_log, 8, "set_normal_priority: reset got %d\n",rv);
	return rv;
#endif /* !APPLE */
}

#endif /* NEVER */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static athread *beep_thread = NULL;
static int beep_delay;
static int beep_freq;
static int beep_msec;

/* Delayed beep handler */
static int delayed_beep(void *pp) {
	msec_sleep(beep_delay);
#ifdef __APPLE__
# if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
	AudioServicesPlayAlertSound(kUserPreferredAlert);
# else
	SysBeep((beep_msec * 60)/1000);
# endif
#else	/* UNIX */
	/* Linux is pretty lame in this regard... */
	fprintf(stdout, "\a"); fflush(stdout);
#endif 
	return 0;
}

/* Activate the system beeper */
void msec_beep(int delay, int freq, int msec) {
	if (delay > 0) {
		if (beep_thread != NULL)
			beep_thread->del(beep_thread);
		beep_delay = delay;
		beep_freq = freq;
		beep_msec = msec;
		if ((beep_thread = new_athread(delayed_beep, NULL)) == NULL)
			a1logw(g_log, "msec_beep: Delayed beep failed to create thread\n");
	} else {
#ifdef __APPLE__
# if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
			AudioServicesPlayAlertSound(kUserPreferredAlert);
# else
			SysBeep((msec * 60)/1000);
# endif
#else	/* UNIX */
		fprintf(stdout, "\a"); fflush(stdout);
#endif
	}
}


#ifdef NEVER
/* If we're UNIX and running on X11, we could do this */
/* sort of thing (from xset) to sound a beep, */
/* IF we were linking with X11: */

static void
set_bell_vol(Display *dpy, int percent) {
	XKeyboardControl values;
	XKeyboardState kbstate;
	values.bell_percent = percent;
	if (percent == DEFAULT_ON)
	  values.bell_percent = SERVER_DEFAULT;
	XChangeKeyboardControl(dpy, KBBellPercent, &values);
	if (percent == DEFAULT_ON) {
	  XGetKeyboardControl(dpy, &kbstate);
	  if (!kbstate.bell_percent) {
	    values.bell_percent = -percent;
	    XChangeKeyboardControl(dpy, KBBellPercent, &values);
	  }
	}
	return;
}

static void
set_bell_pitch(Display *dpy, int pitch) {
	XKeyboardControl values;
	values.bell_pitch = pitch;
	XChangeKeyboardControl(dpy, KBBellPitch, &values);
	return;
}

static void
set_bell_dur(Display *dpy, int duration) {
	XKeyboardControl values;
	values.bell_duration = duration;
	XChangeKeyboardControl(dpy, KBBellDuration, &values);
	return;
}

XBell(..);

#endif /* NEVER */

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Wait for the thread to exit. Return the result */
static int athread_wait(struct _athread *p) {

	if (p->finished)
		return p->result;

	pthread_join(p->thid, NULL);

	return p->result;
}

/* Destroy the thread */
static void athread_del(
athread *p
) {
	DBG("athread_del called\n");

	if (p == NULL)
		return;

	if (!p->finished) {
		pthread_cancel(p->thid);
	}
	pthread_join(p->thid, NULL);
	free(p);
}

static void *threadproc(
	void *param
) {
	athread *p = (athread *)param;

	/* Register this thread with the Objective-C garbage collector */
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
	 objc_registerThreadWithCollector();
#endif

	p->result = p->function(p->context);
	p->finished = 1;

	return 0;
}
 

athread *new_athread(
	int (*function)(void *context),
	void *context
) {
	int rv;
	athread *p = NULL;

	DBG("new_athread called\n");

	if ((p = (athread *)calloc(sizeof(athread), 1)) == NULL) {
		a1loge(g_log, 1, "new_athread: calloc failed\n");
		return NULL;
	}

	p->function = function;
	p->context = context;
	p->wait = athread_wait;
	p->del = athread_del;

	/* Create a thread */
	rv = pthread_create(&p->thid, NULL, threadproc, (void *)p);
	if (rv != 0) {
		a1loge(g_log, 1, "new_athread: pthread_create failed with %d\n",rv);
		athread_del(p);
		return NULL;
	}

	DBG("About to exit new_athread()\n");
	return p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Delete a file */
void delete_file(char *fname) {
	unlink(fname);
}

/* Given the path to a file, ensure that all the parent directories */
/* are created. return nz on error */
int create_parent_directories(char *path) {
	struct stat sbuf;
	char *pp = path;
	mode_t mode = 0700;		/* Default directory mode */

	if (*pp == '/')
		pp++;			/* Skip root directory */
	for (;pp != NULL && *pp != '\000';) {
		if ((pp = strchr(pp, '/')) != NULL) {
			*pp = '\000';
			if (stat(path,&sbuf) != 0)
			{
				if (mkdir(path, mode) != 0)
					return 1;
			} else
				mode = sbuf.st_mode;
			*pp = '/';
			pp++;
		}
	}
	return 0;
}

#endif /* defined(UNIX) */

/* - - - - - - - - - - - - - - - - - - - - - - - - */
#if defined(__APPLE__) || defined(NT)

/* Thread to monitor and kill the named processes */
static int th_kkill_nprocess(void *pp) {
	kkill_nproc_ctx *ctx = (kkill_nproc_ctx *)pp;

	while(ctx->stop == 0) {
		kill_nprocess(ctx->pname, ctx->log);
		msec_sleep(20);			/* Don't hog the CPU */
	}
	ctx->done = 1;

	return 0;
}

static void kkill_nprocess_del(kkill_nproc_ctx *p) {
	int i;

	p->stop = 1;

	DBG("kkill_nprocess del called\n");
	for (i = 0; p->done == 0 && i < 100; i++) {
		msec_sleep(50);
	}

	if (p->done == 0) {			/* Hmm */
		a1logw(p->log,"kkill_nprocess del failed to stop - killing thread\n");
		p->th->del(p->th);
	}

	del_a1log(p->log);
	free(p);

	DBG("kkill_nprocess del done\n");
}

/* Start a thread to constantly kill a process. */
/* Call ctx->del() when done */
kkill_nproc_ctx *kkill_nprocess(char **pname, a1log *log) {
	kkill_nproc_ctx *p;

	DBG("kkill_nprocess called\n");
	if (log != NULL && log->debug >= 8) {
		int i;
		a1logv(log, 8, "kkill_nprocess called with");
		for (i = 0; pname[i] != NULL; i++)
			a1logv(log, 8, " '%s'",pname[i]);
		a1logv(log, 8, "\n");
	}

	if ((p = (kkill_nproc_ctx *)calloc(sizeof(kkill_nproc_ctx), 1)) == NULL) {
		a1loge(log, 1, "kkill_nprocess: calloc failed\n");
		return NULL;
	}

	p->pname = pname;
	p->log = new_a1log_d(log);
	p->del = kkill_nprocess_del;

	if ((p->th = new_athread(th_kkill_nprocess, p)) == NULL) {
		del_a1log(p->log);
		free(p);
		return NULL;
	}
	return p;
}

#ifdef NT

/* Kill a list of named process. */
/* Kill the first process found, then return */
/* return < 0 if this fails. */
/* return 0 if there is no such process */
/* return 1 if a process was killed */
int kill_nprocess(char **pname, a1log *log) {
	PROCESSENTRY32 entry;
	HANDLE snapshot;
	int j;

	/* Get a snapshot of the current processes */
	if ((snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)) == NULL) {
		return -1;
	}

	while(Process32Next(snapshot,&entry) != FALSE) {
		HANDLE proc;

		if (strcmp(entry.szExeFile, "spotread.exe") == 0
		 && (proc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID)) != NULL) {
			if (TerminateProcess(proc,0) == 0) {
				a1logv(log, 8, "kill_nprocess: Failed to kill '%s'\n",entry.szExeFile);
			} else {
				a1logv(log, 8, "kill_nprocess: Killed '%s'\n",entry.szExeFile);
			}
			CloseHandle(proc);
		}
		for (j = 0;; j++) {
			if (pname[j] == NULL)	/* End of list */
				break;
			a1logv(log, 8, "kill_nprocess: Checking process '%s' against list '%s'\n",
			                                                  entry.szExeFile,pname[j]);
			if (strcmp(entry.szExeFile,pname[j]) == 0) {
				a1logv(log, 1, "kill_nprocess: killing process '%s' pid %d\n",
				                            entry.szExeFile,entry.th32ProcessID);

				if ((proc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID)) == NULL
				 || TerminateProcess(proc,0) == 0) {
					a1logv(log, 1, "kill_nprocess: kill process '%s' failed with %d\n",
					                                             pname[j],GetLastError());
					CloseHandle(proc);
					CloseHandle(snapshot);
					return -1;
				}
				CloseHandle(proc);
				/* Stop on first one found ? */
				CloseHandle(snapshot);
				return 1;
			}
		}
	}
	CloseHandle(snapshot);
	return 0;
}

#endif /* NT */

#if defined(__APPLE__)

/* Kill a list of named process. */
/* Kill the first process found, then return */
/* return < 0 if this fails. */
/* return 0 if there is no such process */
/* return 1 if a process was killed */
int kill_nprocess(char **pname, a1log *log) {
	struct kinfo_proc *procList = NULL;
	size_t procCount = 0;
	static int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
	size_t length;
	int rv = 0;
	int i, j;

	procList = NULL;
	for (;;) {
		int err;

		/* Establish the amount of memory needed */
		length = 0;
		if (sysctl(name, (sizeof(name) / sizeof(*name)) - 1,
                      NULL, &length,
                      NULL, 0) == -1) {
			DBGF((DBGA,"sysctl #1 failed with %d\n", errno));
			return -1;
		}
	
		/* Add some more entries in case the number of processors changed */
		length += 10 * sizeof(struct kinfo_proc);
		if ((procList = malloc(length)) == NULL) {
			DBGF((DBGA,"malloc failed for %d bytes\n", length));
			return -1;
		}

		/* Call again with memory */
		if ((err = sysctl(name, (sizeof(name) / sizeof(*name)) - 1,
                          procList, &length,
                          NULL, 0)) == -1) {
			DBGF((DBGA,"sysctl #1 failed with %d\n", errno));
			free(procList);
			return -1;
		}
		if (err == 0) {
			break;
		} else if (err == ENOMEM) {
			free(procList);
			procList = NULL;
		}
	}

	procCount = length / sizeof(struct kinfo_proc);

	/* Locate the processes */
	for (i = 0; i < procCount; i++) {
		for (j = 0;; j++) {
			if (pname[j] == NULL)	/* End of list */
				break;
			a1logv(log, 8, "kill_nprocess: Checking process '%s' against list '%s'\n",
			                                         procList[i].kp_proc.p_comm,pname[j]);
			if (strncmp(procList[i].kp_proc.p_comm,pname[j],MAXCOMLEN) == 0) {
				a1logv(log, 1, "kill_nprocess: killing process '%s' pid %d\n",
				                            pname[j],procList[i].kp_proc.p_pid);
				if (kill(procList[i].kp_proc.p_pid, SIGTERM) != 0) {
					a1logv(log, 1, "kill_nprocess: kill process '%s' failed with %d\n",
					                                                    pname[j],errno);
					free(procList);
					return -1;
				}
				/* Stop on first one found ? */
				free(procList);
				return 1;
			}
		}
	}
	free(procList);
	return rv;
}

#endif /* __APPLE__ */

#endif /* __APPLE__ || NT */

/* ============================================================= */

/* A very small subset of icclib, copied to here. */
/* This is just enough to support the standalone instruments */
#ifdef SALONEINSTLIB

sa_XYZNumber sa_D50 = {
    0.9642, 1.0000, 0.8249
};

void sa_SetUnity3x3(double mat[3][3]) {
	int i, j;
	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			if (i == j)
				mat[j][i] = 1.0;
			else
				mat[j][i] = 0.0;
		}
	}
	
}

void sa_Cpy3x3(double dst[3][3], double src[3][3]) {
	int i, j;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++)
			dst[j][i] = src[j][i];
	}
}

void sa_MulBy3x3(double out[3], double mat[3][3], double in[3]) {
	double tt[3];

	tt[0] = mat[0][0] * in[0] + mat[0][1] * in[1] + mat[0][2] * in[2];
	tt[1] = mat[1][0] * in[0] + mat[1][1] * in[1] + mat[1][2] * in[2];
	tt[2] = mat[2][0] * in[0] + mat[2][1] * in[1] + mat[2][2] * in[2];

	out[0] = tt[0];
	out[1] = tt[1];
	out[2] = tt[2];
}

void sa_Mul3x3_2(double dst[3][3], double src1[3][3], double src2[3][3]) {
	int i, j, k;
	double td[3][3];		/* Temporary dest */

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			double tt = 0.0;
			for (k = 0; k < 3; k++)
				tt += src1[j][k] * src2[k][i];
			td[j][i] = tt;
		}
	}

	/* Copy result out */
	for (j = 0; j < 3; j++)
		for (i = 0; i < 3; i++)
			dst[j][i] = td[j][i];
}


/* Matrix Inversion by Richard Carling from "Graphics Gems", Academic Press, 1990 */
#define det2x2(a, b, c, d) (a * d - b * c)

static void adjoint(
double out[3][3],
double in[3][3]
) {
    double a1, a2, a3, b1, b2, b3, c1, c2, c3;

    /* assign to individual variable names to aid  */
    /* selecting correct values  */

	a1 = in[0][0]; b1 = in[0][1]; c1 = in[0][2];
	a2 = in[1][0]; b2 = in[1][1]; c2 = in[1][2];
	a3 = in[2][0]; b3 = in[2][1]; c3 = in[2][2];

    /* row column labeling reversed since we transpose rows & columns */

    out[0][0]  =   det2x2(b2, b3, c2, c3);
    out[1][0]  = - det2x2(a2, a3, c2, c3);
    out[2][0]  =   det2x2(a2, a3, b2, b3);
        
    out[0][1]  = - det2x2(b1, b3, c1, c3);
    out[1][1]  =   det2x2(a1, a3, c1, c3);
    out[2][1]  = - det2x2(a1, a3, b1, b3);
        
    out[0][2]  =   det2x2(b1, b2, c1, c2);
    out[1][2]  = - det2x2(a1, a2, c1, c2);
    out[2][2]  =   det2x2(a1, a2, b1, b2);
}

static double sa_Det3x3(double in[3][3]) {
    double a1, a2, a3, b1, b2, b3, c1, c2, c3;
    double ans;

	a1 = in[0][0]; b1 = in[0][1]; c1 = in[0][2];
	a2 = in[1][0]; b2 = in[1][1]; c2 = in[1][2];
	a3 = in[2][0]; b3 = in[2][1]; c3 = in[2][2];

    ans = a1 * det2x2(b2, b3, c2, c3)
        - b1 * det2x2(a2, a3, c2, c3)
        + c1 * det2x2(a2, a3, b2, b3);
    return ans;
}

#define SA__SMALL_NUMBER	1.e-8

int sa_Inverse3x3(double out[3][3], double in[3][3]) {
    int i, j;
    double det;

    /*  calculate the 3x3 determinant
     *  if the determinant is zero, 
     *  then the inverse matrix is not unique.
     */
    det = sa_Det3x3(in);

    if ( fabs(det) < SA__SMALL_NUMBER)
        return 1;

    /* calculate the adjoint matrix */
    adjoint(out, in);

    /* scale the adjoint matrix to get the inverse */
    for (i = 0; i < 3; i++)
        for(j = 0; j < 3; j++)
		    out[i][j] /= det;
	return 0;
}

#undef SA__SMALL_NUMBER	
#undef det2x2

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* Transpose a 3x3 matrix */
void sa_Transpose3x3(double out[3][3], double in[3][3]) {
	int i, j;
	if (out != in) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				out[i][j] = in[j][i];
	} else {
		double tt[3][3];
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				tt[i][j] = in[j][i];
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				out[i][j] = tt[i][j];
	}
}

/* Scale a 3 vector by the given ratio */
void sa_Scale3(double out[3], double in[3], double rat) {
	out[0] = in[0] * rat;
	out[1] = in[1] * rat;
	out[2] = in[2] * rat;
}

/* Clamp a 3 vector to be +ve */
extern void sa_Clamp3(double out[3], double in[3]) {
	int i;
	for (i = 0; i < 3; i++)
		out[i] = in[i] < 0.0 ? 0.0 : in[i];
}

/* Return the normal Delta E given two Lab values */
double sa_LabDE(double *Lab0, double *Lab1) {
	double rv = 0.0, tt;

	tt = Lab0[0] - Lab1[0];
	rv += tt * tt;
	tt = Lab0[1] - Lab1[1];
	rv += tt * tt;
	tt = Lab0[2] - Lab1[2];
	rv += tt * tt;

	return sqrt(rv);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */
/* A sub-set of ludecomp code from numlib          */

int sa_lu_decomp(double **a, int n, int *pivx, double *rip) {
	int    i, j;
	double *rscale, RSCALE[10];		

	if (n <= 10)
		rscale = RSCALE;
	else
		rscale = dvector(0, n-1);

	for (i = 0; i < n; i++) {
		double big;
		for (big = 0.0, j=0; j < n; j++) {
			double temp;
			temp = fabs(a[i][j]);
			if (temp > big)
				big = temp;
		}
		if (fabs(big) <= DBL_MIN) {
			if (rscale != RSCALE)
				free_dvector(rscale, 0, n-1);
			return 1;		
		}
		rscale[i] = 1.0/big;	
	}

	for (*rip = 1.0, j = 0; j < n; j++) {
		double big;
		int k, bigi = 0;

		for (i = 0; i < j; i++) {
			double sum;
			sum = a[i][j];
			for (k = 0; k < i; k++)
				sum -= a[i][k] * a[k][j];
			a[i][j] = sum;
		}

		for (big = 0.0, i = j; i < n; i++) {
			double sum, temp;
			sum = a[i][j];
			for (k = 0; k < j; k++)
				sum -= a[i][k] * a[k][j];
			a[i][j] = sum;
			temp = rscale[i] * fabs(sum);	
			if (temp >= big) {
				big = temp;		
				bigi = i;		
			}
		}
		
		if (j != bigi) {
			{	
				double *temp;
				temp = a[bigi];
				a[bigi] = a[j];
				a[j] = temp;
			}
			*rip = -(*rip);				
			rscale[bigi] = rscale[j];	
		}
		
		pivx[j] = bigi;					
		if (fabs(a[j][j]) <= DBL_MIN) {
			if (rscale != RSCALE)
				free_dvector(rscale, 0, n-1);
			return 1; 					
		}

		if (j != (n-1)) {
			double temp;
			temp = 1.0/a[j][j];
			for (i = j+1; i < n; i++)
				a[i][j] *= temp;
		}
	}
	if (rscale != RSCALE)
		free_dvector(rscale, 0, n-1);
	return 0;
}

void sa_lu_backsub(double **a, int n, int *pivx, double  *b) {
	int i, j;
	int nvi;		
	
	for (nvi = -1, i = 0; i < n; i++) {
		int px;
		double sum;

		px = pivx[i];
		sum = b[px];
		b[px] = b[i];
		if (nvi >= 0) {
			for (j = nvi; j < i; j++)
				sum -= a[i][j] * b[j];
		} else {
			if (sum != 0.0)
				nvi = i;			
		}
		b[i] = sum;
	}

	for (i = (n-1); i >= 0; i--) {
		double sum;
		sum = b[i];
		for (j = i+1; j < n; j++)
			sum -= a[i][j] * b[j];
		b[i] = sum/a[i][i];
	}
}

int sa_lu_invert(double **a, int n) {
	int i, j;
	double rip;		
	int *pivx, PIVX[10];
	double **y;

	if (n <= 10)
		pivx = PIVX;
	else
		pivx = ivector(0, n-1);

	if (sa_lu_decomp(a, n, pivx, &rip)) {
		if (pivx != PIVX)
			free_ivector(pivx, 0, n-1);
		return 1;
	}

	y = dmatrix(0, n-1, 0, n-1);
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			y[i][j] = a[i][j];
		}
	}

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++)
			a[i][j] = 0.0;
		a[i][i] = 1.0;
		sa_lu_backsub(y, n, pivx, a[i]);
	}

	free_dmatrix(y, 0, n-1, 0, n-1);
	if (pivx != PIVX)
		free_ivector(pivx, 0, n-1);

	return 0;
}

int sa_lu_psinvert(double **out, double **in, int m, int n) {
	int rv = 0;
	double **tr;		
	double **sq;		

	tr = dmatrix(0, n-1, 0, m-1);
	matrix_trans(tr, in, m,  n);

	if (m > n) {
		sq = dmatrix(0, n-1, 0, n-1);
		if ((rv = matrix_mult(sq, n, n, tr, n, m, in, m, n)) == 0) {
			if ((rv = sa_lu_invert(sq, n)) == 0) {
				rv = matrix_mult(out, n, m, sq, n, n, tr, n, m);
			}
		}
		free_dmatrix(sq, 0, n-1, 0, n-1);
	} else {
		sq = dmatrix(0, m-1, 0, m-1);
		if ((rv = matrix_mult(sq, m, m, in, m, n, tr, n, m)) == 0) {
			if ((rv = sa_lu_invert(sq, m)) == 0) {
				rv = matrix_mult(out, n, m, tr, n, m, sq, m, m);
			}
		}
		free_dmatrix(sq, 0, m-1, 0, m-1);
	}

	free_dmatrix(tr, 0, n-1, 0, m-1);
	return rv;
}


#endif /* SALONEINSTLIB */
/* ============================================================= */


