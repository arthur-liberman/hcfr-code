
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
#include <sys/time.h>
#include <sys/types.h>
#include <pwd.h>

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
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"

#ifdef UNIX_APPLE
//#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <Carbon/Carbon.h>
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
#endif /* UNIX_APPLE */

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

#ifdef NEVER	// Don't need this now.

/* Horrible hack to poll stdin when we're not interactive. */
/* This has the drawback that the char and returm must be */
/* written in one operation for the character to be recognised - */
/* trying to do this manually typically doesn't work unless you are */
/* very fast and lucky. */
static int th_read_char(void *pp) {
	char *rp = (char *)pp;
	HANDLE stdinh;
  	char buf[1];
	DWORD bread;

	if ((stdinh = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE) {
		*rp = 0;		/* We've started */
		return 0;
	}

	*rp = 0;		/* We've started */

	if (ReadFile(stdinh, buf, 1, &bread, NULL)
	 && bread == 1
	 && buf[0] != '\r' && buf[0] != '\n') { 
		*rp = buf[0];
	}

	return 0;
}
#endif	/* NEVER */

/* If there is one, return the next character from the keyboard, else return 0 */
/* (If not_interactive, always returns 0) */
int poll_con_char(void) {

	if (not_interactive) {		/* Can't assume that it's the console */

#ifdef NEVER	// Use better approach below.
		athread *getch_thread = NULL;
		volatile char c = 0xff;
	
		/* This is pretty horrible. The problem is that we can't use */
		/* any of MSWin's async file read functions, because we */
		/* have no way of ensuring that the STD_INPUT_HANDLE has been */
		/* opened with FILE_FLAG_OVERLAPPED. Used a thread instead... */
		/* ReOpenFile() would in theory fix this, but it's not available in WinXP, only Visa+, */
		/* and aparently doesn't work on stdin anyway! :-( */
		if ((getch_thread = new_athread(th_read_char, (char *)&c)) != NULL) {
			HANDLE stdinh;

			if ((stdinh = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE) {
				return 0;
			}

			/* Wait for the thread to start */
			while (c == 0xff) {
				Sleep(10);			/* We just hope 1 msec is enough for the thread to start */
			}
			Sleep(10);			/* Give it time to read */
			CancelIo(stdinh);	/* May not work since ReadFile() is on a different thread ? */
			getch_thread->del(getch_thread);
			return c;
		}
#else	/* ! NEVER */
		/* This approach is very flakey from the console, but seems */
		/* to work reliably when operated progromatically. */
		HANDLE stdinh;
		char buf[10] = { 0 }, c;
		DWORD bread;

		if ((stdinh = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE) {
			return 0;
		}
//		printf("Waiting\n");
		if (WaitForSingleObject(stdinh, 1) == WAIT_OBJECT_0) {
//			printf("stdin signalled\n");
			
//			FlushFileBuffers(stdinh);
//			FlushConsoleInputBuffer(stdinh);
			if (ReadFile(stdinh, buf, 1, &bread, NULL)) {
				int i;
//				fprintf(stderr,"Read %d chars 0x%x 0x%x 0x%x\n",bread,buf[0],buf[1], buf[2]);
				if (buf[0] != '\r' && buf[0] != '\n')
					return buf[0];
				return 0;
			}
		}
#endif	/* !NEVER */
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
/* (If not_interactive, does nothing ?) */
void empty_con_chars(void) {

	if (not_interactive) {
		HANDLE stdinh;
		char buf[100] = { 0 }, c;
		DWORD bread;

		if ((stdinh = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE)
			return;
		for (;;) {
			/* Wait for 1msec */

			/* Do dummy read, as stdin seems to be signalled on startup */
			if (WaitForSingleObject(stdinh, 1) == WAIT_OBJECT_0)
				ReadFile(stdinh, buf, 0, &bread, NULL);

			if (WaitForSingleObject(stdinh, 1) == WAIT_OBJECT_0) {
				ReadFile(stdinh, buf, 100, &bread, NULL);
			} else {
				break;
			}
		}
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

static athread *beep_thread = NULL;
static int beep_delay;
static int beep_freq;
static int beep_msec;

/* Delayed beep handler */
static int delayed_beep(void *pp) {
	msec_sleep(beep_delay);
	a1logd(g_log,8, "msec_beep activate\n");
	Beep(beep_freq, beep_msec);
	return 0;
}

/* Activate the system beeper */
void msec_beep(int delay, int freq, int msec) {
	a1logd(g_log,8, "msec_beep %d msec\n",msec);
	if (delay > 0) {
		if (beep_thread != NULL)
			beep_thread->del(beep_thread);
		beep_delay = delay;
		beep_freq = freq;
		beep_msec = msec;
		if ((beep_thread = new_athread(delayed_beep, NULL)) == NULL)
			a1logw(g_log, "msec_beep: Delayed beep failed to create thread\n");
	} else {
		a1logd(g_log,8, "msec_beep activate\n");
		Beep(freq, msec);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

int acond_timedwait_imp(HANDLE cond, CRITICAL_SECTION *lock, int msec) {
	int rv;
	LeaveCriticalSection(lock);
	rv = WaitForSingleObject(cond, msec);
	EnterCriticalSection(lock);

	if (rv == WAIT_TIMEOUT)
		return 1;

	if (rv != WAIT_OBJECT_0)
		return 2;

	return 0;
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

/* Return the login $HOME directory. */
/* (Useful if we might be running sudo) */
/* No NT equivalent ?? */
char *login_HOME() {
	return getenv("HOME");
}

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

/* - - - - - - - - - - - - - - - - - - - - - - - - */

int acond_timedwait_imp(pthread_cond_t *cond, pthread_mutex_t *lock, int msec) {
	struct timeval tv;
	struct timespec ts;
	int rv;

	// this is unduly complicated...
	gettimeofday(&tv, NULL);
	ts.tv_sec = tv.tv_sec + msec/1000;
	ts.tv_nsec = (tv.tv_usec + (msec % 1000) * 1000) * 1000L;
	if (ts.tv_nsec > 1000000000L) {
		ts.tv_nsec -= 1000000000L;
		ts.tv_sec++;
	}

	rv = pthread_cond_timedwait(cond, lock, &ts);

	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef NEVER    /* Not currently needed, or effective */

/* Set the current threads priority */
int set_interactive_priority() {
#ifdef UNIX_APPLE
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
#else /* !UNIX_APPLE */
	int rv;
	struct sched_param param;
	param.sched_priority = 32;

	/* This doesn't work unless we're running as su :-( */
	rv = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
//		a1logd(g_log, 8, "set_interactive_priority: set got %d\n",rv);
	return rv;
#endif /* !UNIX_APPLE */
}

int set_normal_priority() {
#ifdef UNIX_APPLE
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
#else /* !UNIX_APPLE */
	struct sched_param param;
	param.sched_priority = 0;
	int rv;

	rv = pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
//		a1logd(g_log, 8, "set_normal_priority: reset got %d\n",rv);
	return rv;
#endif /* !UNIX_APPLE */
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
	a1logd(g_log,8, "msec_beep activate\n");
#ifdef UNIX_APPLE
# if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
	AudioServicesPlayAlertSound(kUserPreferredAlert);
# else
	SysBeep((beep_msec * 60)/1000);
# endif
#else	/* UNIX */
	/* Linux is pretty lame in this regard... */
	/* Maybe we could write an 8Khz 8 bit sample to /dev/dsp, or /dev/audio ? */
	/* The ALSA system is the modern way for audio output. */
	/* Also check out what sox does: <http://sox.sourceforge.net/> */
	fprintf(stdout, "\a"); fflush(stdout);
#endif 
	return 0;
}

/* Activate the system beeper */
void msec_beep(int delay, int freq, int msec) {
	a1logd(g_log,8, "msec_beep %d msec\n",msec);
	if (delay > 0) {
		if (beep_thread != NULL)
			beep_thread->del(beep_thread);
		beep_delay = delay;
		beep_freq = freq;
		beep_msec = msec;
		if ((beep_thread = new_athread(delayed_beep, NULL)) == NULL)
			a1logw(g_log, "msec_beep: Delayed beep failed to create thread\n");
	} else {
		a1logd(g_log,8, "msec_beep activate\n");
#ifdef UNIX_APPLE
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
	/* (Hmm. Done by default in latter versions though, hence deprecated in them ?) */
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

#if defined(UNIX_APPLE)
	{
		pthread_attr_t stackSzAtrbt;

		/* Default stack size is 512K - this is a bit small - raise it to 4MB */
		if ((rv = pthread_attr_init(&stackSzAtrbt)) != 0
		 || (rv = pthread_attr_setstacksize(&stackSzAtrbt, 4 * 1024 * 1024)) != 0) {
			fprintf(stderr,"new_athread: thread_attr_setstacksize failed with %d\n",rv);
			return NULL;
		}

		/* Create a thread */
		rv = pthread_create(&p->thid, &stackSzAtrbt, threadproc, (void *)p);
		if (rv != 0) {
			a1loge(g_log, 1, "new_athread: pthread_create failed with %d\n",rv);
			athread_del(p);
			return NULL;
		}
	}
#else	/* !APPLE */

	/* Create a thread */
	rv = pthread_create(&p->thid, NULL, threadproc, (void *)p);
	if (rv != 0) {
		a1loge(g_log, 1, "new_athread: pthread_create failed with %d\n",rv);
		athread_del(p);
		return NULL;
	}
#endif /* !APPLE */

	DBG("About to exit new_athread()\n");
	return p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Return the login $HOME directory. */
/* (Useful if we might be running sudo) */
char *login_HOME() {

	if (getuid() == 0) {	/* If we are running as root */
		char *uids;

		if ((uids = getenv("SUDO_UID")) != NULL) {		/* And we sudo's to get it */
			int uid;
			struct passwd *pwd;

			uid = atoi(uids);

			if ((pwd = getpwuid(uid)) != NULL) {
				return pwd->pw_dir;
			}
		}
	}

	return getenv("HOME");
}


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
#if defined(UNIX_APPLE) || defined(NT)

/* Thread to monitor and kill the named processes */
static int th_kkill_nprocess(void *pp) {
	kkill_nproc_ctx *ctx = (kkill_nproc_ctx *)pp;

	/* set result to 0 if it ever suceeds or there was no such process */
	ctx->th->result = -1;
	while(ctx->stop == 0) {
		if (kill_nprocess(ctx->pname, ctx->log) >= 0)
			ctx->th->result = 0;
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

#if defined(UNIX_APPLE)

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

#endif /* UNIX_APPLE */

#endif /* UNIX_APPLE || NT */


