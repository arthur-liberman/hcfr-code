
 /* Platform isolation convenience functions */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   28/9/97
 *
 * Copyright 1997 - 2010 Graeme W. Gill
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
#endif
#include "numsup.h"
#include "xspect.h"
#include "ccss.h"
#include "insttypes.h"
#include "icoms.h"
#include "conv.h"
#include "usbio.h"
#include "sort.h"

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
#endif /* __APPLE__ */

#undef DEBUG

#ifdef DEBUG
# define errout stderr
# define DBG(xx)	fprintf(errout, xx )
# define DBGF(xx)	fprintf xx
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
#include <direct.h> 

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

		return 0;
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
/* the process started. */
/* (Is this based on timeGetTime() ? ) */
unsigned int msec_time() {
	return GetTickCount();
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
			error("Delayed beep failed to create thread");
	} else {
		Beep(freq, msec);
	}
}

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

/* Destroy the thread */
static void athread_del(
athread *p
) {
	DBG("athread_del called\n");

	if (p == NULL)
		return;

	if (p->th != NULL) {		/* Oops. this isn't good. */
		DBG("athread_del calling TerminateThread() because thread hasn't finished\n");
		TerminateThread(p->th, -1);		/* But it is worse to leave it hanging around */
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
	CloseHandle(p->th);
	p->th = NULL;
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
	if ((p = (athread *)calloc(sizeof(athread), 1)) == NULL)
		return NULL;

	p->function = function;
	p->context = context;
	p->del = athread_del;

	/* Create a thread */
#ifdef USE_BEGINTHREAD
	p->th = _beginthread(threadproc, 0, (void *)p);
	if (p->th == -1) {
#else
	p->th = CreateThread(NULL, 0, threadproc, (void *)p, 0, NULL);
	if (p->th == NULL) {
#endif
		DBG("Failed to create thread\n");
		athread_del(p);
		return NULL;
	}

	DBG("About to exit new_athread()\n");
	return p;
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
			error("tcgetattr failed with '%s' on stdin", strerror(errno));
		news = origs;
		news.c_lflag &= ~(ICANON | ECHO);
		news.c_cc[VTIME] = 0;
		news.c_cc[VMIN] = 1;
		if (tcsetattr(STDIN_FILENO,TCSANOW, &news) < 0)
			error("next_con_char: tcsetattr failed with '%s' on stdin", strerror(errno));
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
		error("poll on stdin returned unexpected value 0x%x",pa[0].revents);
	}

	/* Restore stdin */
	if (!not_interactive && tcsetattr(STDIN_FILENO, TCSANOW, &origs) < 0)
		error("tcsetattr failed with '%s' on stdin", strerror(errno));

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
			error("tcgetattr failed with '%s' on stdin", strerror(errno));
		news = origs;
		news.c_lflag &= ~(ICANON | ECHO);
		news.c_cc[VTIME] = 0;
		news.c_cc[VMIN] = 1;
		if (tcsetattr(STDIN_FILENO,TCSANOW, &news) < 0)
			error("next_con_char: tcsetattr failed with '%s' on stdin", strerror(errno));
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
		error("tcsetattr failed with '%s' on stdin", strerror(errno));

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

/* Return the current time in msec. This is not related to any particular epoch */
unsigned int msec_time() {
	unsigned int rv;
	static struct timeval startup = { 0, 0 };
	struct timeval cv;

	/* Is this monotonic ? */
	/* On Linux, should clock_gettime with CLOCK_MONOTONIC be used instead ? */
	/* On OS X, should mach_absolute_time() be used ? */
	/* or host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &clk) */
	gettimeofday(&cv, NULL);

	/* Set time to 0 on first invocation */
	if (startup.tv_sec == 0 && startup.tv_usec == 0)
		startup = cv;

	/* Subtract, taking care of carry */
	cv.tv_sec -= startup.tv_sec;
	if (startup.tv_usec > cv.tv_usec) {
		cv.tv_sec--;
		cv.tv_usec += 1000000;
	}
	cv.tv_usec -= startup.tv_usec;

	/* Convert usec to msec */
	rv = cv.tv_sec * 1000 + cv.tv_usec / 1000;

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
printf("~1 set to forground got %d\n",rv);
	return rv;
#else
    int rv = 0;
    struct thread_precedence_policy tppolicy;

    tppolicy.importance = 500;

    if (thread_policy_set(mach_thread_self(),
        THREAD_PRECEDENCE_POLICY, (thread_policy_t)&tppolicy,
        THREAD_PRECEDENCE_POLICY_COUNT) != KERN_SUCCESS)
		rv = 1;
printf("~1 set to important got %d\n",rv);
	return rv;
#endif /* NEVER */
#else /* !APPLE */
	int rv;
	struct sched_param param;
	param.sched_priority = 32;

	/* This doesn't work unless we're running as su :-( */
	rv = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
//printf("Set got %d\n",rv);
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
printf("~1 set to normal got %d\n",rv);
#else
    int rv = 0;
    struct thread_precedence_policy tppolicy;

    tppolicy.importance = 1;

    if (thread_policy_set(mach_thread_self(),
        THREAD_STANDARD_POLICY, (thread_policy_t)&tppolicy,
        THREAD_STANDARD_POLICY_COUNT) != KERN_SUCCESS)
		rv = 1;
printf("~1 set to standard got %d\n",rv);
	return rv;
#endif /* NEVER */
#else /* !APPLE */
	struct sched_param param;
	param.sched_priority = 0;
	int rv;

	rv = pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
//printf("Reset got %d\n",rv);
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
	SysBeep((beep_msec * 60)/1000);
#else
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
			error("Delayed beep failed to create thread");
	} else {
#ifdef __APPLE__
		SysBeep((msec * 60)/1000);
#else
		/* Linux is pretty lame in this regard... */
		fprintf(stdout, "\a"); fflush(stdout);
#endif
	}
}


#ifdef NEVER
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* If we're UNIX and running on X11, we could do this */
/* sort of thing (from xset), IF we were linking with X11: */

static void
set_bell_vol(Display *dpy, int percent)
{
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
set_bell_pitch(Display *dpy, int pitch)
{
XKeyboardControl values;
values.bell_pitch = pitch;
XChangeKeyboardControl(dpy, KBBellPitch, &values);
return;
}

static void
set_bell_dur(Display *dpy, int duration)
{
XKeyboardControl values;
values.bell_duration = duration;
XChangeKeyboardControl(dpy, KBBellDuration, &values);
return;
}

XBell(..);

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#endif /* NEVER */

/* - - - - - - - - - - - - - - - - - - - - - - - - */

/* Destroy the thread */
static void athread_del(
athread *p
) {
	DBG("athread_del called\n");

	if (p == NULL)
		return;

	pthread_cancel(p->thid);
	pthread_join(p->thid, NULL);

	free(p);
}

static void *threadproc(
	void *param
) {
	athread *p = (athread *)param;

	p->result = p->function(p->context);
	return 0;
}
 

athread *new_athread(
	int (*function)(void *context),
	void *context
) {
	int rv;
	athread *p = NULL;

	DBG("new_athread called\n");
	if ((p = (athread *)calloc(sizeof(athread), 1)) == NULL)
		return NULL;

	p->function = function;
	p->context = context;
	p->del = athread_del;

	/* Create a thread */
	rv = pthread_create(&p->thid, NULL, threadproc, (void *)p);
	if (rv != 0) {
		DBG("Failed to create thread\n");
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
		kill_nprocess(ctx->pname, ctx->debug);
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
		DBG("kkill_nprocess del failed to stop - killing thread\n");
		p->th->del(p->th);
	}

	free(p);

	DBG("kkill_nprocess del done\n");
}

/* Start a thread to constantly kill a process. */
/* Call ctx->del() when done */
kkill_nproc_ctx *kkill_nprocess(char **pname, int debug) {
	kkill_nproc_ctx *p;

	DBG("kkill_nprocess called\n");
	if (debug) {
		int i;
		fprintf(stderr,"kkill_nprocess called with");
		for (i = 0; pname[i] != NULL; i++)
			fprintf(stderr," '%s'",pname[i]);
		fprintf(stderr,"\n");
	}

	if ((p = (kkill_nproc_ctx *)calloc(sizeof(kkill_nproc_ctx), 1)) == NULL) {
		DBG("kkill_nprocess calloc failed\n");
		return NULL;
	}

	p->pname = pname;
	p->debug = debug;
	p->del = kkill_nprocess_del;

	if ((p->th = new_athread(th_kkill_nprocess, p)) == NULL) {
		DBG("kkill_nprocess new_athread failed\n");
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
int kill_nprocess(char **pname, int debug) {
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
				printf("Failed to kill '%s'\n",entry.szExeFile);
			} else {
				printf("Killed '%s'\n",entry.szExeFile);
			}
			CloseHandle(proc);
		}
		for (j = 0;; j++) {
			if (pname[j] == NULL)	/* End of list */
				break;
			if (debug >= 8)
				fprintf(stderr,"Checking process '%s' against list '%s'\n",entry.szExeFile,pname[j]);
			if (strcmp(entry.szExeFile,pname[j]) == 0) {
				if (debug)
					fprintf(stderr,"Killing process '%s'\n",entry.szExeFile);
				DBGF((errout,"killing process '%s' pid %d\n",entry.szExeFile,entry.th32ProcessID));

				if ((proc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID)) == NULL
				 || TerminateProcess(proc,0) == 0) {
					if (debug)
						fprintf(stderr,"kill process '%s' failed with %d\n",pname[j],GetLastError());
					DBGF((errout,"kill process '%s' failed with %d\n",pname[j],GetLastError()));
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
int kill_nprocess(char **pname, int debug) {
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
			DBGF((errout,"sysctl #1 failed with %d\n", errno));
			return -1;
		}
	
		/* Add some more entries in case the number of processors changed */
		length += 10 * sizeof(struct kinfo_proc);
		if ((procList = malloc(length)) == NULL) {
			DBGF((errout,"malloc failed for %d bytes\n", length));
			return -1;
		}

		/* Call again with memory */
		if ((err = sysctl(name, (sizeof(name) / sizeof(*name)) - 1,
                          procList, &length,
                          NULL, 0)) == -1) {
			DBGF((errout,"sysctl #1 failed with %d\n", errno));
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
			if (debug >= 8)
				fprintf(stderr,"Checking process '%s' against list '%s'\n",procList[i].kp_proc.p_comm,pname[j]);
			if (strncmp(procList[i].kp_proc.p_comm,pname[j],MAXCOMLEN) == 0) {
				if (debug)
					fprintf(stderr,"Killing process '%s'\n",procList[i].kp_proc.p_comm);
				DBGF((errout,"killing process '%s' pid %d\n",pname[j],procList[i].kp_proc.p_pid));
				if (kill(procList[i].kp_proc.p_pid, SIGTERM) != 0) {
					if (debug)
						fprintf(stderr,"kill process '%s' failed with %d\n",pname[j],errno);
					DBGF((errout,"kill process '%s' failed with %d\n",pname[j],errno));
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
/* Provide a system independent glob type function */

/* Create the aglob */
/* Return nz on malloc error */
int aglob_create(aglob *g, char *spath) {
#ifdef NT
	char *pp;
	int rlen;
	/* Figure out where the filename starts */
	if ((pp = strrchr(spath, '/')) == NULL
	 && (pp = strrchr(spath, '\\')) == NULL)
		rlen = 0;
	else
		rlen = pp - spath + 1;

	if ((g->base = malloc(rlen + 1)) == NULL)
		return 1;

	memmove(g->base, spath, rlen);
	g->base[rlen] = '\000';

	g->first = 1;
    g->ff = _findfirst(spath, &g->ffs);
#else /* UNIX */
	g->rv = glob(spath, GLOB_NOSORT, NULL, &g->g);
	if (g->rv == GLOB_NOSPACE)
		return 1;
	g->ix = 0;
#endif
	g->merr = 0;
	return 0;
}

/* Return an allocated string of the next match. */
/* Return NULL if no more matches */
char *aglob_next(aglob *g) {
	char *fpath;

#ifdef NT
	if (g->ff == -1L) {
		return NULL;
	}
	if (g->first == 0) {
		if (_findnext(g->ff, &g->ffs) != 0) {
			return NULL;
		}
	}
	g->first = 0;

	/* Convert match filename to full path */
	if ((fpath = malloc(strlen(g->base) + strlen(g->ffs.name) + 1)) == NULL) {
		g->merr = 1;
		return NULL;
	}
	strcpy(fpath, g->base);
	strcat(fpath, g->ffs.name);
	return fpath;
#else
	if (g->rv != 0 || g->ix >= g->g.gl_pathc)
		return NULL;
	if ((fpath = strdup(g->g.gl_pathv[g->ix])) == NULL) {
		g->merr = 1;
		return NULL;
	}
	g->ix++;
	return fpath;
#endif
}

void aglob_cleanup(aglob *g) {
#ifdef NT
	if (g->ff != -1L)
		_findclose(g->ff);
	free(g->base);
#else /* UNIX */
	if (g->rv == 0)
		globfree(&g->g);
#endif
}

/* ============================================================= */
/* CCSS location support */

/* return a list of installed ccss files. */
/* The list is sorted by description and terminated by a NULL entry. */
/* If no is != NULL, return the number in the list */
/* Return NULL and -1 if there is a malloc error */
iccss *list_iccss(int *no) {
	int i, j;
	iccss *rv;

	char **paths = NULL;
	int npaths = 0;

	npaths = xdg_bds(NULL, &paths, xdg_data, xdg_read, xdg_user, "color/*.ccss");

	if ((rv = malloc(sizeof(iccss) * (npaths + 1))) == NULL) {
		xdg_free(paths, npaths);
		if (no != NULL) *no = -1;
		return NULL;
	}
	
	for (i = j = 0; i < npaths; i++) {
		ccss *cs;
		int len;
		char *pp;
		char *tech, *disp;

		if ((cs = new_ccss()) == NULL) {
			for (--j; j>= 0; j--) {
				free(rv[j].path);
				free(rv[j].desc);
			}
			xdg_free(paths, npaths);
			if (no != NULL) *no = -1;
			return NULL;
		}
		if (cs->read_ccss(cs, paths[i]))
			continue;		/* Skip any unreadable ccss's */

		if ((tech = cs->tech) == NULL)
			tech = "";
		if ((disp = cs->disp) == NULL)
			disp = "";
		len = strlen(tech) + strlen(disp) + 4;
		if ((pp = malloc(len)) == NULL) {
			for (--j; j >= 0; j--) {
				free(rv[j].path);
				free(rv[j].desc);
			}
			free(rv);
			xdg_free(paths, npaths);
			if (no != NULL) *no = -1;
			return NULL;
		}
		if ((rv[j].path = _strdup(paths[i])) == NULL) {
			for (--j; j >= 0; j--) {
				free(rv[j].path);
				free(rv[j].desc);
			}
			free(rv);
			free(pp);
			xdg_free(paths, npaths);
			if (no != NULL) *no = -1;
			return NULL;
		}
		strcpy(pp, tech);
		strcat(pp, " (");
		strcat(pp, disp);
		strcat(pp, ")");
		rv[j].desc = pp;
		cs->del(cs);
		j++;
	}
	xdg_free(paths, npaths);
	rv[j].path = NULL;
	rv[j].desc = NULL;
	if (no != NULL)
		*no = j;

	/* Sort the list */
#define HEAP_COMPARE(A,B) (strcmp(A.desc, B.desc) < 0) 
	HEAPSORT(iccss, rv, j)
#undef HEAP_COMPARE

	return rv;
}

/* Free up a iccss list */
void free_iccss(iccss *list) {
	int i;

	if (list != NULL) {
		for (i = 0; list[i].path != NULL || list[i].desc != NULL; i++) {
			if (list[i].path != NULL)
				free(list[i].path);
			if (list[i].desc != NULL)
				free(list[i].desc);
		}
		free(list);
	}
}

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

#endif /* SALONEINSTLIB */
/* ============================================================= */


