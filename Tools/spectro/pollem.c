
/* Unix serial I/O class poll() emulation. */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   12/9/2004
 *
 * Copyright 2004, 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#ifdef UNIX

/* Fake up poll() support on systems that only support select() */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <memory.h>

/* select() defined, but not poll(), so emulate poll() */
#if defined(FD_CLR) && !defined(POLLIN)

#include <sys/time.h>

#include "pollem.h"

int pollem(struct pollfd *fds, unsigned long nfds, int timeout) {
	int i, nfd;
	fd_set rd_ary;				/* Select array for read file descriptors */
	fd_set wr_ary;				/* Select array for write file descriptors */
	fd_set ex_ary;				/* Select array for exception file descriptors */
	struct timeval tv;			/* Timeout value */
	struct timeval *ptv = &tv;	/* Pointer to above */
	int result;					/* Select return value */

	/* Translate files and events */
	FD_ZERO(&rd_ary);
	FD_ZERO(&wr_ary);
	FD_ZERO(&ex_ary);

	for (i = nfd = 0; i < nfds; i++) {
		fds[i].revents = 0;

		if (fds[i].events & POLLIN) {
			FD_SET(fds[i].fd, &rd_ary);
			if (fds[i].fd > nfd)
				nfd = fds[i].fd;
		}

		if (fds[i].events & POLLPRI) {
			FD_SET(fds[i].fd, &ex_ary);
			if (fds[i].fd > nfd)
				nfd = fds[i].fd;
		}

		if (fds[i].events & POLLOUT) {
			FD_SET(fds[i].fd, &wr_ary);
			if (fds[i].fd > nfd)
				nfd = fds[i].fd;
		}
	}
	nfd++;	/* Maximum file desciptor index + 1 */

	/* Translate timeout */
	if (timeout == -1) {
		ptv = NULL;			/* Wait forever */

	} else if (timeout == 0) {
		tv.tv_sec = 0;		/* Return imediately */
		tv.tv_usec = 0;

	} else {				/* Convert milliseconds to seconds and useconds */
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout - tv.tv_sec * 1000) * 1000;
	}

	/* Do the select */
	if ((result = select(nfd, &rd_ary, &wr_ary, &ex_ary, ptv)) > 0) {

		for (i = 0; i < nfds; i++) {
			fds[i].revents = 0;

			if (FD_ISSET(fds[i].fd, &ex_ary))
				fds[i].revents |= POLLPRI;

			if (FD_ISSET(fds[i].fd, &rd_ary))
				fds[i].revents |= POLLIN;
	
			if (FD_ISSET(fds[i].fd, &wr_ary))
				fds[i].revents |= POLLOUT;
		}
	}
	
	return result;
}

#endif /* FD_CLR */

#endif /* UNIX */
