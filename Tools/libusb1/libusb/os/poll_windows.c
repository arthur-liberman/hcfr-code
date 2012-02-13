/*
 * poll_windows: poll compatibility wrapper for Windows
 * Copyright (C) 2009-2010 Pete Batard <pbatard@gmail.com>
 * With contributions from Michael Plante, Orin Eman et al.
 * Parts of poll implementation from libusb-win32, by Stephan Meyer et al.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

/*
 * poll() and pipe() Windows compatibility layer for libusb 1.0
 *
 * The way this layer works is by using OVERLAPPED with async I/O transfers, as
 * OVERLAPPED have an associated event which is flagged for I/O completion.
 * 
 * For USB pollable async I/O, you would typically:
 * - obtain a Windows HANDLE to a file or device that has been opened in
 *   OVERLAPPED mode
 * - call usbi_create_fd with this handle to obtain a custom fd.
 *   Note that if you need simultaneous R/W access, you need to call create_fd
 *   twice, once in _O_RDONLY and once in _O_WRONLY mode to obtain 2 separate
 *   pollable fds
 * - leave the core functions call the poll routine and flag POLLIN/POLLOUT
 * 
 * The pipe pollable synchronous I/O works using the overlapped event associated
 * with a fake pipe. The read/write functions are only meant to be used in that
 * context.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>

#include <libusbi.h>

#undef ENABLE_CANCELIOEX		/* Doesn't seem to work reliably */

// Uncomment to debug the polling layer
#define DEBUG_POLL_WINDOWS
#if defined(DEBUG_POLL_WINDOWS)
#define poll_dbg usbi_dbg
#else
// MSVC6 cannot use a variadic argument and non MSVC
// compilers produce warnings if parenthesis are ommitted.
#if defined(_MSC_VER)
#define poll_dbg
#else
#define poll_dbg(...)
#endif
#endif

#if defined(_PREFAST_)
#pragma warning(disable:28719)
#endif

#if defined(__CYGWIN__)
// cygwin produces a warning unless these prototypes are defined
extern int _close(int fd);
extern int _snprintf(char *buffer, size_t count, const char *format, ...);
extern int cygwin_attach_handle_to_fd(char *name, int fd, HANDLE handle, int bin, int access); 
// _open_osfhandle() is not available on cygwin, but we can emulate
// it for our needs with cygwin_attach_handle_to_fd()
static inline int _open_osfhandle(intptr_t osfhandle, int flags)
{
	int access;
	switch (flags) {
	case _O_RDONLY:
		access = GENERIC_READ;
		break;
	case _O_WRONLY:
		access = GENERIC_WRITE;
		break;
	case _O_RDWR:
		access = GENERIC_READ|GENERIC_WRITE;
		break;
	default:
		usbi_err(NULL, "unuspported access mode");
		return -1;
	}
	return cygwin_attach_handle_to_fd("/dev/null", -1, (HANDLE)osfhandle, -1, access);
}
#endif

#define CHECK_INIT_POLLING do {if(!is_polling_set) init_polling();} while(0)

// public fd data
const struct winfd INVALID_WINFD = {-1, INVALID_HANDLE_VALUE, NULL, 0, 0, FALSE, 0, 0, NULL, FALSE, RW_NONE };
struct winfd poll_fd[MAX_FDS];
// internal fd data
struct {
	CRITICAL_SECTION mutex; // lock for fds
	BYTE marker;            // 1st byte of a usbi_read operation gets stored here
	// Additional variables for XP CancelIoEx partial emulation
	HANDLE duplicate_handle;
	DWORD thread_id;
} _poll_fd[MAX_FDS];

// globals
BOOLEAN is_polling_set = FALSE;
#if defined(DYNAMIC_FDS)
HANDLE fd_update = INVALID_HANDLE_VALUE;	// event to notify poll of fd update
HANDLE new_fd[MAX_FDS];		// overlapped event handles for fds created since last poll
unsigned nb_new_fds = 0;	// nb new fds created since last poll
usbi_mutex_t new_fd_mutex;	// mutex required for the above
#endif
static volatile LONG compat_spinlock = 0;

int windows_cancel_transfer(struct usbi_transfer *itransfer);
static BOOL (__stdcall *pCancelIoEx)(HANDLE, LPOVERLAPPED) = NULL;
#define CancelIoEx_Available (pCancelIoEx != NULL)

// CancelIoEx, available on Vista and later only, provides the ability to cancel
// a single transfer (OVERLAPPED) when used. As it may not be part of any of the 
// platform headers, we hook into the Kernel32 system DLL directly to seek it.
// On previous versions of Windows, CancelIo is available, but it cancels
// all outstanding io for the given handle and thread. If this is the thread that
// started the io, we can use it, but if not and we really have to
// cancel the io, use windows_cancel_transfer() to cancel all io to the end point. 
__inline BOOL cancel_io(struct usbi_transfer *itransfer, int index)
{
	int i;

	if ((index < 0) || (index >= MAX_FDS)) {
		return FALSE;
	}
	if ( (poll_fd[index].fd < 0) || (poll_fd[index].handle == INVALID_HANDLE_VALUE)
	  || (poll_fd[index].handle == 0) ) {
		return TRUE;
	}
	if (CancelIoEx_Available) {
		BOOL rv = TRUE;
		int i;
		for (i = poll_fd[index].num_urbs-1; i > poll_fd[index].num_retired; i--) {
			if (!HasOverlappedIoCompleted(&poll_fd[index].urbs[i].overlapped)) {
				poll_dbg("CancelIoEx called on fd %d index %d urb %d", poll_fd[index].fd, index, i);
				rv = rv && (*pCancelIoEx)(poll_fd[index].handle, &poll_fd[index].urbs[i].overlapped);
			}
		}
		return rv;
	}

	// See if any need cancelling
	for (i = poll_fd[index].num_urbs-1; i > poll_fd[index].num_retired; i--) {
		if (!HasOverlappedIoCompleted(&poll_fd[index].urbs[i].overlapped)) {
			break;
		}
	}
	if (i <= poll_fd[index].num_retired) {		// No
		return TRUE;
	}

	// If we can use CancelIo safely, use it
	if (_poll_fd[index].thread_id == GetCurrentThreadId()) {
		poll_dbg("CancelIo called on fd %d index %d", poll_fd[index].fd, index);
		return CancelIo(poll_fd[index].handle);
	}
	if (itransfer == NULL) {
		usbi_warn(NULL, "Unable to cancel I/O that was started from another thread");
		return FALSE;
	}

	// Cancel transfers to the given end point
	poll_dbg("Cancel Transfer called on ep %02x", __USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer)->endpoint);
	if (windows_cancel_transfer(itransfer) != LIBUSB_SUCCESS)
		return FALSE;
	return TRUE;
}

// Init
void init_polling(void)
{
	int i;

	while (InterlockedExchange((LONG *)&compat_spinlock, 1) == 1) {
		SleepEx(0, TRUE);
	}
	if (!is_polling_set) {
#ifdef ENABLE_CANCELIOEX
		pCancelIoEx = (BOOL (__stdcall *)(HANDLE,LPOVERLAPPED))
			GetProcAddress(GetModuleHandle("KERNEL32"), "CancelIoEx");
#endif
		usbi_dbg("Will use CancelIo%s for I/O cancellation", 
			CancelIoEx_Available?"Ex":"");
		for (i=0; i<MAX_FDS; i++) {
			poll_fd[i] = INVALID_WINFD;
			_poll_fd[i].marker = 0;
			_poll_fd[i].duplicate_handle = INVALID_HANDLE_VALUE;
			_poll_fd[i].thread_id = 0;
			InitializeCriticalSection(&_poll_fd[i].mutex);
		}
#if defined(DYNAMIC_FDS)
		// We need to create an update event so that poll is warned when there
		// are new/deleted fds during a timeout wait operation
		fd_update = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (fd_update == NULL) {
			usbi_err(NULL, "unable to create update event");
		}
		usbi_mutex_init(&new_fd_mutex, NULL);
		nb_new_fds = 0;
#endif
		is_polling_set = TRUE;
	}
	compat_spinlock = 0;
}

// Internal function to retrieve the table index (and lock the fd mutex)
int _fd_to_index_and_lock(int fd)
{
	int i;

	if (fd <= 0)
		return -1;

	for (i=0; i<MAX_FDS; i++) {
		if (poll_fd[i].fd == fd) {
			EnterCriticalSection(&_poll_fd[i].mutex);
			// fd might have changed before we got to critical
			if (poll_fd[i].fd != fd) {
				LeaveCriticalSection(&_poll_fd[i].mutex);
				continue;
			}
			return i;
		}
	}
	return -1;
}

struct winurb *create_urbs(int num_urbs) 
{
	int i;

	struct winurb *urbs = calloc(num_urbs, sizeof(struct winurb));
	if (urbs == NULL) {
		return NULL;
	}
	for (i = 0; i < num_urbs; i++) {
		urbs[i].overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if(urbs[i].overlapped.hEvent == NULL) {
			while (--i >= 0)
				CloseHandle(urbs[i].overlapped.hEvent);
			free (urbs);
			return NULL;
		}
	}
	return urbs;
}

void free_urbs(struct winurb *urbs, int num_urbs)
{
	int i;

	if (urbs == NULL)
		return;

	for (i = 0; i < num_urbs; i++) {
		if ( (urbs[i].overlapped.hEvent != 0) 
		  && (urbs[i].overlapped.hEvent != INVALID_HANDLE_VALUE) ) {
			CloseHandle(urbs[i].overlapped.hEvent);
		}
	}
	free(urbs);
}

void reset_urbs(struct winurb *urbs, int num_urbs)
{
	int i;

	HANDLE event_handle;
	if (urbs == NULL)
		return;

	for (i = 0; i < num_urbs; i++) {
		event_handle = urbs[i].overlapped.hEvent;
		if (event_handle != NULL) {
			ResetEvent(event_handle);
		}
		memset(&urbs[i], 0, sizeof(struct winurb));
		urbs[i].overlapped.hEvent = event_handle;
	}
}

void exit_polling(void)
{
	int i;

	while (InterlockedExchange((LONG *)&compat_spinlock, 1) == 1) {
		SleepEx(0, TRUE);
	}
	if (is_polling_set) {
		is_polling_set = FALSE;

		for (i=0; i<MAX_FDS; i++) {
			// Cancel any async I/O (handle can be invalid)
			cancel_io(NULL, i);
			// If anything was pending on that I/O, it should be
			// terminating, and we should be able to access the fd
			// mutex lock before too long
			EnterCriticalSection(&_poll_fd[i].mutex);
			if ( (poll_fd[i].fd > 0) && (poll_fd[i].handle != INVALID_HANDLE_VALUE) && (poll_fd[i].handle != 0)
			  && (GetFileType(poll_fd[i].handle) == FILE_TYPE_UNKNOWN) ) {
				_close(poll_fd[i].fd);
			}
			free_urbs(poll_fd[i].urbs, poll_fd[i].num_urbs);
			poll_fd[i] = INVALID_WINFD;
#if defined(DYNAMIC_FDS)
			usbi_mutex_destroy(&new_fd_mutex);
			CloseHandle(fd_update);
			fd_update = INVALID_HANDLE_VALUE;
#endif
			LeaveCriticalSection(&_poll_fd[i].mutex);
			DeleteCriticalSection(&_poll_fd[i].mutex);
		}
	}
	compat_spinlock = 0;
}

/*
 * Create a fake pipe.
 * As libusb only uses pipes for signaling, all we need from a pipe is an
 * event. To that extent, we create one wfd and use its overlapped as a means
 * to access that event.
 */
int usbi_pipe(int filedes[2])
{
	int i;
	HANDLE handle[2];
	struct winurb *urbs;

	CHECK_INIT_POLLING;

	poll_dbg("usbi_pipe() called");

	urbs = create_urbs(1);
	if(urbs == NULL) {
		return -1;
	}
	// Borrow otherwise unused overlapped InternalHigh as counter
	urbs->overlapped.InternalHigh = 0;
	handle[0] = CreateFileA("NUL", 0, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (handle[0] == INVALID_HANDLE_VALUE) {
		usbi_err(NULL, "could not create pipe handle: errcode %d", (int)GetLastError());
		free_urbs(urbs, 1);
		return -1;
	}
	filedes[1] = filedes[0] = _open_osfhandle((intptr_t)handle[0], _O_RDWR);
	poll_dbg("filedes[1] = filedes[0] = %d", filedes[0]);

	for (i=0; i<MAX_FDS; i++) {
		if (poll_fd[i].fd < 0) {
			EnterCriticalSection(&_poll_fd[i].mutex);
			// fd might have been allocated before we got to critical
			if (poll_fd[i].fd >= 0) {
				LeaveCriticalSection(&_poll_fd[i].mutex);
				continue;
			}
			poll_fd[i] = INVALID_WINFD;		// Init all the fields
			poll_fd[i].fd = filedes[0];
			poll_fd[i].handle = handle[0];
			poll_fd[i].urbs = urbs;
			poll_fd[i].num_urbs = 1;
			poll_fd[i].signalpipe = TRUE;
			poll_fd[i].rw = RW_READ | RW_WRITE;
			LeaveCriticalSection(&_poll_fd[i].mutex);
			return 0;
		}
	}

	poll_dbg("usbi_pipe() failed");

	CloseHandle(handle[0]);
	free_urbs(urbs, 1);
	return -1;
}

/*
 * Create both an fd and an array of winurb's that contain OVERLAPPED from an
 * open Windows handle, so that it can be used with our polling function.
 * The handle MUST support overlapped transfers (usually requires CreateFile
 * with FILE_FLAG_OVERLAPPED)
 * Return a pollable file descriptor struct, or NULL on error
 *
 * Note that the fd returned by this function is a per-transfer fd, rather
 * than a per-session fd and cannot be used for anything else but our 
 * custom functions (the fd itself points to the NUL: device)
 * if you plan to do R/W on the same handle, you MUST create 2 fds: one for
 * read and one for write. Using a single R/W fd is unsupported and will
 * produce unexpected results
 */
struct winfd *usbi_create_fd(HANDLE handle, int access_mode, int num_urbs)
{
	int i, j, fd;
	struct winfd *wfd = NULL;
	struct winurb* urbs = NULL;

	CHECK_INIT_POLLING;

	if ((handle == 0) || (handle == INVALID_HANDLE_VALUE)) {
		return NULL;
	}

	if ((access_mode != _O_RDONLY) && (access_mode != _O_WRONLY)) {
		usbi_warn(NULL, "only one of _O_RDONLY or _O_WRONLY are supported.\n"
			"If you want to poll for R/W simultaneously, create multiple fds from the same handle.");
		return NULL;
	}
	// Ensure that we get a non system conflicting unique fd
	fd = _open_osfhandle((intptr_t)CreateFileA("NUL", 0, 0,
		NULL, OPEN_EXISTING, 0, NULL), _O_RDWR);
	if (fd < 0) {
		return NULL;
	}

	urbs = create_urbs(num_urbs);
	if(urbs == NULL) {
		_close(fd);
		return NULL;
	}

	for (i=0; i<MAX_FDS; i++) {
		if (poll_fd[i].fd < 0) {
			EnterCriticalSection(&_poll_fd[i].mutex);
			// fd might have been removed before we got to critical
			if (poll_fd[i].fd >= 0) {
				LeaveCriticalSection(&_poll_fd[i].mutex);
				continue;
			}
			wfd = &poll_fd[i];
			*wfd = INVALID_WINFD;		// Init all the fields
			wfd->fd = fd;
			wfd->handle = handle;
			// Attempt to emulate some of the CancelIoEx behaviour on platforms
			// that don't have it, by creating a duplicate handle to restrict CancelIo's scope.
			if (pCancelIoEx == NULL) {
				_poll_fd[i].thread_id = GetCurrentThreadId();
				if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
					&wfd->handle, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
					usbi_warn(NULL, "could not duplicate handle for CancelIo - using orignal one");
					wfd->handle = handle;
					_poll_fd[i].duplicate_handle = INVALID_HANDLE_VALUE;
				} else {
					_poll_fd[i].duplicate_handle = wfd->handle;
				}
			}
			wfd->urbs = urbs;
			wfd->num_urbs = num_urbs;
			LeaveCriticalSection(&_poll_fd[i].mutex);
#if defined(DYNAMIC_FDS)
			usbi_mutex_lock(&new_fd_mutex);
			for (j = 0; j < num_urbs; j++)
				new_fd[nb_new_fds++] = urbs[j].overlapped.hEvent;
			usbi_mutex_unlock(&new_fd_mutex);
			// Notify poll that fds have been updated
			SetEvent(fd_update);
#endif
			if (access_mode == _O_RDONLY) {
				wfd->rw = RW_READ;
			} else {
				wfd->rw = RW_WRITE;
			}

			return wfd;
		}
	}
	free_urbs(urbs, num_urbs);
	_close(fd);
	return NULL;
}

void _free_index(int index)
{
	// Cancel any async IO (Don't care about the validity of our handles for this)
	// close fake handle for devices
	if (_poll_fd[index].duplicate_handle != INVALID_HANDLE_VALUE) {
		CloseHandle(_poll_fd[index].duplicate_handle);
		_poll_fd[index].duplicate_handle = INVALID_HANDLE_VALUE;
	} else {
		cancel_io(NULL, index);
	}
	if ( (poll_fd[index].handle != INVALID_HANDLE_VALUE) && (poll_fd[index].handle != 0)
	  && (GetFileType(poll_fd[index].handle) == FILE_TYPE_UNKNOWN) ) {
		_close(poll_fd[index].fd);
	}
	free_urbs(poll_fd[index].urbs, poll_fd[index].num_urbs);
	_poll_fd[index].thread_id = 0;
	poll_fd[index] = INVALID_WINFD;
}

/*
 * Release a pollable file descriptor. 
 *
 * Note that the associated Windows handle is not closed by this call
 */
void usbi_free_fd(int fd)
{
	int index;

	CHECK_INIT_POLLING;

	index = _fd_to_index_and_lock(fd);
	if (index < 0) {
		return;
	}
	_free_index(index);
	LeaveCriticalSection(&_poll_fd[index].mutex);
}

/*
 * The functions below perform various conversions between fd, handle and OVERLAPPED
 * (Note that NULL is returned on error)
 */
struct winfd *fd_to_winfd(int fd)		// Not used
{
	int i;
	struct winfd *wfd;

	CHECK_INIT_POLLING;

	if (fd <= 0)
		return NULL;

	for (i=0; i<MAX_FDS; i++) {
		if (poll_fd[i].fd == fd) {
			EnterCriticalSection(&_poll_fd[i].mutex);
			// fd might have been deleted before we got to critical
			if (poll_fd[i].fd != fd) {
				LeaveCriticalSection(&_poll_fd[i].mutex);
				continue;
			}
			wfd = &poll_fd[i];
			LeaveCriticalSection(&_poll_fd[i].mutex);
			return wfd;
		}
	}
	return NULL;
}

struct winfd *handle_to_winfd(HANDLE handle)
{
	int i;
	struct winfd *wfd;

	CHECK_INIT_POLLING;

	if ((handle == 0) || (handle == INVALID_HANDLE_VALUE))
		return NULL;

	for (i=0; i<MAX_FDS; i++) {
		if (poll_fd[i].handle == handle) {
			EnterCriticalSection(&_poll_fd[i].mutex);
			// fd might have been deleted before we got to critical
			if (poll_fd[i].handle != handle) {
				LeaveCriticalSection(&_poll_fd[i].mutex);
				continue;
			}
			wfd = &poll_fd[i];
			LeaveCriticalSection(&_poll_fd[i].mutex);
			return wfd;
		}
	}
	return NULL;
}

/*
 * POSIX poll equivalent, using Windows OVERLAPPED
 * Currently, this function only accepts one of POLLIN or POLLOUT per fd
 * (but you can create multiple fds from the same handle for read and write)
 */
int usbi_poll(struct pollfd *fds, unsigned int nfds, int timeout)
{
	unsigned i;
	int index, object_index, triggered;
	HANDLE *handles_to_wait_on;
	int *handle_to_index;
	DWORD nb_handles_to_wait_on = 0;
	DWORD elapsed;
	DWORD ret;

#if defined(DYNAMIC_FDS)
	DWORD nb_extra_handles = 0;
	unsigned j;

	// To address the possibility of missing new fds between the time the new
	// pollable fd set is assembled, and the ResetEvent() call below, an 
	// additional new_fd[] HANDLE table is used for any new fd that was created
	// since the last call to poll (see below)
	ResetEvent(fd_update);

	// At this stage, any new fd creation will be detected through the fd_update
	// event notification, and any previous creation that we may have missed 
	// will be picked up through the existing new_fd[] table.
#endif

	CHECK_INIT_POLLING;

	handles_to_wait_on = malloc((nfds+1)*sizeof(HANDLE));	// +1 for fd_update
	handle_to_index = malloc(nfds*sizeof(int));
	if ((handles_to_wait_on == NULL) || (handle_to_index == NULL)) {
		errno = ENOMEM;
		triggered = -1;
		goto poll_exit;
	}

	// Loop until we timout or are triggered
	for (;;) {

		triggered = 0;
		nb_handles_to_wait_on = 0;

		for (i = 0; i < nfds; ++i) {
			struct winfd *fdp;
			struct winurb *urbp;

			fds[i].revents = 0;

			// Only one of POLLIN or POLLOUT can be selected with this version of poll (not both)
			if ((fds[i].events & ~POLLIN) && (!(fds[i].events & POLLOUT))) {
				fds[i].revents |= POLLERR;
				errno = EACCES;
				usbi_warn(NULL, "unsupported set of events");
				triggered = -1;
				goto poll_exit;
			}

			index = _fd_to_index_and_lock(fds[i].fd);
			fdp = &poll_fd[index];
			poll_dbg("fd %d index %d urb %d/%d %sarmed for events %04X", fdp->fd, index, fdp->num_retired+1, fdp->num_urbs, fdp->signalpipe ? "(fake pipe) " : "", fds[i].events);

			if ( (index < 0) || (fdp->handle == INVALID_HANDLE_VALUE)
			  || (fdp->handle == 0) || (fdp->urbs == NULL)) {
				fds[i].revents |= POLLNVAL | POLLERR;
				errno = EBADF;
				if (index >= 0) {
					LeaveCriticalSection(&_poll_fd[index].mutex);
				}
				usbi_warn(NULL, "invalid fd");
				triggered = -1;
				goto poll_exit;
			}

			// IN or OUT must match our fd direction
			if ((fds[i].events & POLLIN) && (fdp->rw & RW_READ) == 0) {
				fds[i].revents |= POLLNVAL | POLLERR;
				errno = EBADF;
				usbi_warn(NULL, "attempted POLLIN on fds[%d] = %d without READ access", i, fds[i].fd);
				LeaveCriticalSection(&_poll_fd[index].mutex);
				triggered = -1;
				goto poll_exit;
			}

			if ((fds[i].events & POLLOUT) && (fdp->rw & RW_WRITE) == 0) {
				fds[i].revents |= POLLNVAL | POLLERR;
				errno = EBADF;
				usbi_warn(NULL, "attempted POLLOUT on fds[%d] = %d without WRITE access", i, fds[i].fd);
				LeaveCriticalSection(&_poll_fd[index].mutex);
				triggered = -1;
				goto poll_exit;
			}

			// Somethings gone badly wrong
			if (fdp->num_retired >= fdp->num_urbs) {
				usbi_err(NULL, "poll on fd %d urb %d/%d fds[%d] without any outstanding I/O", fds[i].fd, fdp->num_retired+1, fdp->num_urbs, i);
				LeaveCriticalSection(&_poll_fd[index].mutex);
				triggered = -1;
				goto poll_exit;
			}

			/* Look though all the overlapped's for this fd */
			for (;;) {
				DWORD result, io_size;
				urbp = &fdp->urbs[fdp->num_retired];

				// Special case our fake pipe - see if it signaled
				if (fdp->signalpipe) {
				 	result = WaitForSingleObject(fdp->urbs[0].overlapped.hEvent, 0);
					if (result == WAIT_OBJECT_0) {
						fds[i].revents = fds[i].events;
						LeaveCriticalSection(&_poll_fd[index].mutex);
						triggered++;
						poll_dbg("fd %d index %d fake pipe triggered events %04X", fdp->fd, index, fdp->num_urbs, fds[i].revents);
						goto poll_exit;
					}
					if (result != WAIT_TIMEOUT) {
						fds[i].revents |= POLLNVAL | POLLERR;
						errno = EBADF;
						usbi_warn(NULL, "fd %d fake pipe WaitForSingleObject failed with %d", fdp->fd, GetLastError());
						LeaveCriticalSection(&_poll_fd[index].mutex);
						triggered = -1;
						goto poll_exit;
					}
				} else if (urbp->sync_compl || HasOverlappedIoCompleted(&urbp->overlapped)) {
					fdp->num_retired++;
					if (urbp->sync_compl) {
						result = urbp->sync_result;

						if (result == NO_ERROR) {
							io_size = urbp->sync_size + urbp->cntr_extra_size;
							fdp->io_result = NO_ERROR;
							fdp->io_size += io_size;
							poll_dbg("  sync completed urb %d/%d %d bytes total (+= %d)",fdp->num_retired,fdp->num_urbs, fdp->io_size, io_size);
							if (!fdp->aborted && io_size != urbp->req_size) {
								poll_dbg("  sync short transfer, req_size %d, io_size %d",urbp->req_size,io_size);
								fdp->aborted = TRUE;
								cancel_io(fdp->itransfer, index);
							}
						} else {
							if (fdp->io_result == NO_ERROR) {
								if (!fdp->aborted || result != ERROR_OPERATION_ABORTED) 
									fdp->io_result = result;
							}
							poll_dbg("  failed urb %d/%d err %d, %d bytes total (%d)",fdp->num_retired,fdp->num_urbs, result, fdp->io_size, io_size);
						}
					} else {	// HasOverlappedIoCompleted
						if (GetOverlappedResult(fdp->handle, &urbp->overlapped, &io_size, FALSE)) {
							if (!fdp->aborted)
								io_size += urbp->cntr_extra_size;
							fdp->io_result = NO_ERROR;
							fdp->io_size += io_size;
							poll_dbg("  completed urb %d/%d %d bytes total (+= %d)",fdp->num_retired,fdp->num_urbs, fdp->io_size, io_size);
							if (!fdp->aborted && io_size != urbp->req_size) {
								poll_dbg("  short transfer, req_size %d, io_size %d",urbp->req_size,io_size);
								fdp->aborted = TRUE;
								cancel_io(fdp->itransfer, index);
							}
						} else {
							result = GetLastError();

							if (fdp->io_result == NO_ERROR) {
								if (!fdp->aborted || result != ERROR_OPERATION_ABORTED) 
									fdp->io_result = result;
							poll_dbg("  failed urb %d/%d err %d, %d bytes total (%d)",fdp->num_retired,fdp->num_urbs, result, fdp->io_size, io_size);
							}
						}
					}
					
					if (fdp->num_retired >= fdp->num_urbs) {
						fds[i].revents = fds[i].events;
						LeaveCriticalSection(&_poll_fd[index].mutex);
						triggered++;
						poll_dbg("fd %d index %d urb %d/%d triggered events %04X", fdp->fd, index, fdp->num_retired, fdp->num_urbs, fds[i].revents);
						goto poll_exit;
					}
					continue;		// Check out next urb
				}
		
				poll_dbg("Adding fd %d index %d hEvent %d to wait list", fdp->fd, index, urbp->overlapped.hEvent);
				handles_to_wait_on[nb_handles_to_wait_on] = urbp->overlapped.hEvent;
				handle_to_index[nb_handles_to_wait_on] = i;
#if defined(DYNAMIC_FDS)
				// If this fd from the poll set is also part of the new_fd event handle table, remove it
				usbi_mutex_lock(&new_fd_mutex);
				for (j=0; j<nb_new_fds; j++) {
					if (handles_to_wait_on[nb_handles_to_wait_on] == new_fd[j]) {
						new_fd[j] = INVALID_HANDLE_VALUE;
						break;
					}
				}
				usbi_mutex_unlock(&new_fd_mutex);
#endif
				nb_handles_to_wait_on++;
				break;		// Wait on this one
			}
			LeaveCriticalSection(&_poll_fd[index].mutex);
		}
#if defined(DYNAMIC_FDS)
		// At this stage, new_fd[] should only contain events from fds that
		// have been added since the last call to poll, but are not (yet) part
		// of the pollable fd set. Typically, these would be from fds that have
		// been created between the construction of the fd set and the calling
		// of poll. 
		// Event if we won't be able to return usable poll data on these events,
		// make sure we monitor them to return an EINTR code
		usbi_mutex_lock(&new_fd_mutex); // We could probably do without
		for (i=0; i<nb_new_fds; i++) {
			if (new_fd[i] != INVALID_HANDLE_VALUE) {
				handles_to_wait_on[nb_handles_to_wait_on++] = new_fd[i];
				nb_extra_handles++;
			}
		}
		usbi_mutex_unlock(&new_fd_mutex);
		poll_dbg("dynamic_fds: added %d extra handles", nb_extra_handles);
#endif

		if (timeout == 0) {
			poll_dbg("  timed out (time = 0)");
			triggered = 0;	// 0 = timeout
			goto poll_exit;
		}

		if (nb_handles_to_wait_on == 0 || triggered != 0) {
			// Hmm. We should not be here!
			usbi_err(NULL, "Assert: internal error in usbi_poll()");
			errno = EBADF;
			triggered = -1;
			goto poll_exit;
		}

		// If nothing was triggered, wait on all fds that require it
#if defined(DYNAMIC_FDS)
		// Register for fd update notifications
		handles_to_wait_on[nb_handles_to_wait_on++] = fd_update;
		nb_extra_handles++;
#endif
		if (timeout < 0) {
			poll_dbg("starting infinite wait for %d handles...", (int)nb_handles_to_wait_on);
		} else {
			poll_dbg("starting %d ms wait for %d handles...", timeout, (int)nb_handles_to_wait_on);
		}
		elapsed = GetTickCount();
		ret = WaitForMultipleObjects(nb_handles_to_wait_on, handles_to_wait_on, 
			FALSE, (timeout<0)?INFINITE:(DWORD)timeout);
		elapsed = GetTickCount() - elapsed;
		object_index = ret-WAIT_OBJECT_0;
		if ((object_index >= 0) && ((DWORD)object_index < nb_handles_to_wait_on)) {
#if defined(DYNAMIC_FDS)
			if ((DWORD)object_index >= (nb_handles_to_wait_on-nb_extra_handles)) {
				// Detected fd update => flag a poll interruption
				if ((DWORD)object_index == (nb_handles_to_wait_on-1))
					poll_dbg("  dynamic_fds: fd_update event");
				else
					poll_dbg("  dynamic_fds: new fd I/O event");
				errno = EINTR;
				triggered = -1;
				goto poll_exit;
			}
#endif
			poll_dbg("  completed after wait");
			i = handle_to_index[object_index];
			index = _fd_to_index_and_lock(fds[i].fd);
			if (index >= 0)
				LeaveCriticalSection(&_poll_fd[index].mutex);
			// Compute remaining timeout
			if (elapsed <= (DWORD)timeout)
				timeout -= elapsed;
			else
				timeout = 0;
			// Loop around to check if we're done or another wait is needed.
		} else if (ret == WAIT_TIMEOUT) {
			poll_dbg("  timed out");
			triggered = 0;	// 0 = timeout
			goto poll_exit;
		} else {
			errno = EIO;
			triggered = -1;	// error
			goto poll_exit;
		}
	}

poll_exit:
	if (handles_to_wait_on != NULL) {
		free(handles_to_wait_on);
	}
	if (handle_to_index != NULL) {
		free(handle_to_index);
	}
#if defined(DYNAMIC_FDS)
	usbi_mutex_lock(&new_fd_mutex);
	nb_new_fds = 0;
	usbi_mutex_unlock(&new_fd_mutex);
#endif
	return triggered;
}

/*
 * close a pollable fd
 *
 * Note that this function will also close the associated handle
 */
int usbi_close(int fd)
{
	int index;
	HANDLE handle;
	int r = -1;

	CHECK_INIT_POLLING;

	index = _fd_to_index_and_lock(fd);

	if (index < 0) {
		errno = EBADF;
	} else {
		handle = poll_fd[index].handle;
		_free_index(index);
		if (CloseHandle(handle) == 0) {
			errno = EIO;
		} else {
			r = 0;
		}
		LeaveCriticalSection(&_poll_fd[index].mutex);
	}
	return r;
}

/*
 * synchronous write for fake "pipe" signaling
 */
ssize_t usbi_write(int fd, const void *buf, size_t count)
{
	int index;

	CHECK_INIT_POLLING;

	if (count != sizeof(unsigned char)) {
		usbi_err(NULL, "this function should only used for signaling");
		return -1;
	}

	index = _fd_to_index_and_lock(fd);

	if ( (index < 0) || (poll_fd[index].urbs[0].overlapped.hEvent == NULL) 
	  || (poll_fd[index].rw & RW_WRITE) == 0 ) {
		errno = EBADF;
		if (index >= 0) {
			LeaveCriticalSection(&_poll_fd[index].mutex);
		}
		return -1;
	}

	poll_dbg("set pipe event (fd = %d, thread = %08X)", poll_fd[index].fd, GetCurrentThreadId());
	SetEvent(poll_fd[index].urbs[0].overlapped.hEvent);
	// If two threads write on the pipe at the same time, we need to
	// process two separate reads => use the overlapped as a counter
	poll_fd[index].urbs[0].overlapped.InternalHigh++;

	LeaveCriticalSection(&_poll_fd[index].mutex);
	return sizeof(unsigned char);
}

/*
 * synchronous read for fake "pipe" signaling
 */
ssize_t usbi_read(int fd, void *buf, size_t count)
{
	int index;
	ssize_t r = -1;

	CHECK_INIT_POLLING;

	if (count != sizeof(unsigned char)) {
		usbi_err(NULL, "this function should only used for signaling");
		return -1;
	}

	index = _fd_to_index_and_lock(fd);

	if (index < 0) {
		errno = EBADF;
		return -1;
	}

	if ((poll_fd[index].rw & RW_READ) == 0) {
		errno = EBADF;
		goto out;
	}

	if (WaitForSingleObject(poll_fd[index].urbs[0].overlapped.hEvent, INFINITE) != WAIT_OBJECT_0) {
		usbi_warn(NULL, "waiting for event fd %d failed: %d", poll_fd[index].fd, (int)GetLastError());
		errno = EIO;
		goto out;
	}

	poll_dbg("clr pipe event (fd = %d, thread = %08X)", poll_fd[index], GetCurrentThreadId());
	poll_fd[index].urbs[0].overlapped.InternalHigh--;
	// Don't reset unless we don't have any more events to process
	if (poll_fd[index].urbs[0].overlapped.InternalHigh <= 0) {
		poll_dbg("Reset pipe event fd %d",poll_fd[index].fd);
		ResetEvent(poll_fd[index].urbs[0].overlapped.hEvent);
	}

	r = sizeof(unsigned char);

out:
	LeaveCriticalSection(&_poll_fd[index].mutex);
	return r;
}
