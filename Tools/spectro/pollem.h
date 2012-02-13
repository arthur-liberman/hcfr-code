
#ifndef POLLEN_H

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

#ifdef __cplusplus
	extern "C" {
#endif

#ifdef UNIX

/* Fake up poll() support on systems that only support select() */

/* Fake poll array structure */
struct pollfd {
	int fd;			/* File descriptor */
	short events;	/* Requested events */
	short revents;	/* Returned events */
};

/* Fake Poll flag values supported */
#define POLLIN	0x01
#define	POLLPRI	0x02
#define	POLLOUT	0x04

/* Timeout is in milliseconds, -1 == wait forever */
int pollem(struct pollfd fds[], unsigned long nfds, int timeout);

#define POLLEN_H
#endif /* POLLEN_H */

#endif /* UNIX */

#ifdef __cplusplus
	}
#endif

