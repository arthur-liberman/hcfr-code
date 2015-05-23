

#define __UI_C__

/*
 * Do OS specific setup for using UI windows.
 *
 * Copyright 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Typically we need to set things up and then call the
 * "normal" main, called "uimain" in ArgyllCMS utils.
 */

#ifdef UNIX

#if __APPLE__

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* OS X */

/*

	OS X is dumb in relying on an event loop that
	_has_ to be in the main thread to operate.
	Since we can invoke UI operations from any thread,
	the way we work around this is to intercept the
	main thread, spawn a secondary thread to run the
	application main(), and then do nothing but
	service the events in the main thread.

	Note though that Cocoa has poor thread safety :-
	ie. NSRunLoop can't be used to access events - use CFRunLoop

	Should bracket all drawing code between the lockFocusIfCanDraw and
	unlockFocus methods of NSView ? Or is this only a problem if
	different threads try and draw to the same window ?

 */

# include <stdio.h>
# include <stdlib.h>
# include <pthread.h>

# include <Foundation/Foundation.h>
# include <AppKit/AppKit.h>

/* This is a mechanism to force libui to link */
int ui_initialized = 0;

static int g_argc;
static char **g_argv;

pthread_t ui_thid = 0;		/* Thread ID of main thread running io run loop */
pthread_t ui_main_thid = 0;	/* Thread ID of thread running application main() */

static void *callMain(void *p) {
	int rv;

	ui_main_thid = pthread_self();

	NSAutoreleasePool *tpool = [NSAutoreleasePool new];

	rv = uimain(g_argc, g_argv);

	[tpool release];

	exit(rv);
}

/* Dumy method for NSThread to start */
@interface MainClass : NSObject
+(void)dummyfunc:(id)param;
@end

@implementation MainClass
+(void)dummyfunc:(id)param{
}

@end

int main(int argc, char ** argv) {

	ui_thid = pthread_self();

	/* Create an NSApp */
	static NSAutoreleasePool *pool = nil;
	ProcessSerialNumber psn = { 0, 0 };

	/* Transform the process so that the desktop interacts with it properly. */
	/* We don't need resources or a bundle if we do this. */
	if (GetCurrentProcess(&psn) == noErr) {
		OSStatus stat;
		if (psn.lowLongOfPSN != 0 && (stat = TransformProcessType(&psn,
			               kProcessTransformToForegroundApplication)) != noErr) {
//			fprintf(stderr,"TransformProcess failed with code %d\n",stat);
		} else {
//			fprintf(stderr,"TransformProcess suceeded\n");
		}
//		if ((stat = SetFrontProcess(&psn)) != noErr) {
//			fprintf(stderr,"SetFrontProcess returned error %d\n",stat);
//		}
	}

	pool = [NSAutoreleasePool new];

	[NSApplication sharedApplication];	/* Creates NSApp */
	[NSApp finishLaunching];

	/* We seem to need this, because otherwise we don't get focus automatically */
	[NSApp activateIgnoringOtherApps: YES];

	/* We need to create at least one NSThread to tell Cocoa that we are using */
	/* threads, and to protect Cococa objects. */
    [NSThread detachNewThreadSelector:@selector(dummyfunc:) toTarget:[MainClass class] withObject:nil];

	/* Call the real main() in another thread */
	int rv;
	pthread_t thid;
	g_argc = argc;
	g_argv = argv;

	ui_initialized = 1;

	if ((rv = pthread_create(&thid, NULL, callMain, (void *)NULL)) != 0) {
		fprintf(stderr,"ui: pthread_create failed with %d\n",rv);
		return -1;
	}

	/* Service the run queue */
	[NSApp run];

	/* Note that we don't actually clean this up on exit - */
	/* possibly we can't. */
//	[NSApp terminate: nil];
}

#else /* !APPLE */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* UNIX */

/* This is a mechanism to force libui to link */
int ui_initialized = 1;			/* Nothing needs initializing */

#endif /* !APPLE */

#endif /* UNIX */

#ifdef NT
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* NT */

#ifdef NEVER

/*
	This code isn't generally usable for console programs, because
	Microsoft changed the default behaviour of cmd.exe in Vista to
	not wait for a gui mode .exe to terminate. A non-gui .exe always
	pops a new console if run from explorer.

	So this approach is still viable for primarily gui programs
	to output debug to stdout, but a different shell or "cmd.exe /E:OFF"
	is needed to interact with it via the shell.

	Alternatives are messy - mark the exe as console and
	have it shut down the popup console in gui mode (looks
	ugly), or have a stub app.bat that invokes the
	real .exe, forcing cmd.exe to operate in the mode where
	it waits for execution to finish.
*/

/* This is a mechanism to force libui to link */
int ui_initialized = 0;

/*

	On MSWin we can rely on WinMain to be called instead of
	main(), so that we can re-attache the stdio so that the
	resulting exe works the same when involked either from
	a shell, or directly from explorer.

 */

/* May have to add link flag -Wl,-subsystem,windows */
/* since MingW is stupid about noticing WinMain or pragma */

#include <windows.h>
#include <stdio.h>

# pragma comment( linker, "/subsystem:windows" )
//# pragma comment( linker, "/subsystem:console /ENTRY:WinMainCRTStartup" )
//# pragma comment( linker, "/subsystem:windows /ENTRY:mainCRTStartup" )
//# pragma comment( linker, "/subsystem:windows /ENTRY:WinMainCRTStartup" )

APIENTRY WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
	{	/* Only works on >= XP though */
		BOOL (WINAPI *AttachConsole)(DWORD dwProcessId);

		*(FARPROC *)&AttachConsole = 
          GetProcAddress(LoadLibraryA("kernel32.dll"), "AttachConsole");

		if (AttachConsole != NULL && AttachConsole(((DWORD)-1)))
		{
			if (_fileno(stdout) < 0)
				freopen("CONOUT$","wb",stdout);
			if (_fileno(stderr) < 0)
				freopen("CONOUT$","wb",stderr);
			if (_fileno(stdin) < 0)
				freopen("CONIN$","rb",stdin);
#ifdef __cplusplus 
			// make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog point to console as well
			std::ios::sync_with_stdio();
#endif
		}
	}

	ui_initialized = 1;

	return uimain(__argc, __argv);
}

#else /* !NEVER */

/* This is a mechanism to force libui to link */
int ui_initialized = 1;			/* Nothing needs initializing */

#endif /* !NEVER */

#endif	/* NT */
