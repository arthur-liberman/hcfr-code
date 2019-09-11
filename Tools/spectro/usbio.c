
 /* General USB I/O support */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   2006/22/4
 *
 * Copyright 2006 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* These routines supliement the class code in icoms_nt.c and icoms_ux.c */
/* with common and USB specific routines */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#if defined(UNIX)
#include <termios.h>
#include <errno.h>
#include <unistd.h>
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

#ifdef ENABLE_USB

/* Counter set when we're in a USB read or write */
/* Note - this isn't perfectly thread safe */
int in_usb_rw = 0;

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Cancel token utility functions */

/* Used by caller of icoms to init and uninit token */
void usb_init_cancel(usb_cancelt *p) {
	
	amutex_init(p->cmtx);
	amutex_init(p->condx);

	p->hcancel = NULL;
}

void usb_uninit_cancel(usb_cancelt *p) {
	amutex_del(p->cmtx);
	amutex_del(p->condx);
}

/* Used by caller of icoms to re-init for wait_io */
/* Must be called before icoms_usb_wait_io() */
void usb_reinit_cancel(usb_cancelt *p) {
	
	amutex_lock(p->cmtx);

	p->hcancel = NULL;
	p->state = 0;
	amutex_lock(p->condx);		/* Block until IO is started */

	amutex_unlock(p->cmtx);
}

/* Wait for the given transaction to be pending or complete. */
static int icoms_usb_wait_io(
	icoms *p,
	usb_cancelt *cancelt
) {
	amutex_lock(cancelt->condx);	/* Wait for unlock */
	amutex_unlock(cancelt->condx);	/* Free it up for next time */
	return ICOM_OK;
}

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Include the USB implementation dependent function implementations */
# ifdef NT
#  include "usbio_nt.c"
# endif
# if defined(UNIX_APPLE)
#  include "usbio_ox.c"
# endif
# if defined(UNIX_X11)
#  if defined(__FreeBSD__) || defined(__OpenBSD__)
#   include "usbio_bsd.c"
#  else
#   include "usbio_lx.c"
#  endif
# endif

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* I/O routines supported by icoms - uses platform independent */
/* USB routines implemented by code in usbio_*.c above */

/* USB control message */
static int
icoms_usb_control(
icoms *p,
int requesttype,		/* 8 bit request type (USB bmRequestType) */
int request,			/* 8 bit request code (USB bRequest) */
int value,				/* 16 bit value (USB wValue) */
int index,				/* 16 bit index (USB wIndex) */
unsigned char *rwbuf,	/* Write or read buffer */
int rwsize, 			/* Bytes to read or write */
double tout				/* Timeout in seconds */
) {
	int rv = 0;			/* Return value */
	int rwbytes;		/* Data bytes read or written */
	long top;			/* timeout in msec */

	if (p->log->debug >= 8) {
		a1logd(p->log, 8, "icoms_usb_control: message  %02x, %02x %04x %04x %04x\n",
	                                 requesttype, request, value, index, rwsize);
		if ((requesttype & IUSB_ENDPOINT_IN) == 0)
			a1logd(p->log, 8, " writing data %s\n",icoms_tohex(rwbuf, rwsize));
	}

	if (!p->is_open) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_control: device not open\n");
		return ICOM_SYS;
	}

	top = (int)(tout * 1000.0 + 0.5);		/* Timout in msec */

#ifdef QUIET_MEMCHECKERS
	if (requesttype & IUSB_ENDPOINT_IN)
		memset(rwbuf, 0, rwsize);
#endif

	/* Call back end implementation */
	rv = icoms_usb_control_msg(p, &rwbytes, requesttype, request, value, index,
	                                                          rwbuf, rwsize, top);
	a1logd(p->log, 8, "icoms_usb_control: returning ICOM err 0x%x\n",rv);

	if (p->log->debug >= 8 && (requesttype & IUSB_ENDPOINT_IN)) 
		a1logd(p->log, 8, " read data %s\n",icoms_tohex(rwbuf, rwsize));

	return rv;
}

/* - - - - - - - - - - - - - */
/* USB Read/Write. Direction is set by ep. */
/* Don't retry on a short read, return ICOM_SHORT. */
static int
icoms_usb_rw(icoms *p,
	usb_cancelt  *cancelt,	/* Cancel handle */
	int ep,					/* End point address */
	unsigned char *rbuf,	/* Read/Write buffer */
	int bsize,				/* Bytes to read */
	int *breadp,			/* Bytes read/written */
	double tout				/* Timeout in seconds */
) {
	int lerr;				/* Last error */
	int bread, qa;
	long top;				/* Timeout period */
	icom_usb_trantype type;	/* bulk or interrupt */

#ifdef QUIET_MEMCHECKERS
	memset(rbuf, 0, bsize);
#endif

	if (!p->is_open) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_rw: device not initialised\n");
		return ICOM_SYS;
	}

	if (p->EPINFO(ep).valid == 0) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_rw: invalid end point 0x%02x\n",ep);
		return ICOM_SYS;
	}

	if (p->EPINFO(ep).type != ICOM_EP_TYPE_BULK
	 && p->EPINFO(ep).type != ICOM_EP_TYPE_INTERRUPT) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_rw: unhandled end point type %d\n",p->EPINFO(ep).type);
		return ICOM_SYS;
	}

	if (p->EPINFO(ep).type == ICOM_EP_TYPE_BULK)
		type = icom_usb_trantype_bulk;
	else
		type = icom_usb_trantype_interrutpt;

	qa = bsize;						/* For simpler tracing */

	lerr = 0;
	bread = 0;

	top = (int)(tout * 1000 + 0.5);		/* Timeout period in msecs */

	/* Bug workaround - on some OS's for some devices */
	if (p->uflags & icomuf_resetep_before_read
	 && (ep & IUSB_ENDPOINT_DIR_MASK) == IUSB_ENDPOINT_IN) {
		msec_sleep(1);		/* Let device recover ? */
		p->usb_resetep(p, ep);
		msec_sleep(1);		/* Let device recover (ie. Spyder 3) */
	}

	/* Until data is all read/written, we get a short read/write, we time out, or the user aborts */
//	a1logd(p->log, 8, "icoms_usb_rw: read/write of %d bytes, timout %f\n",bsize,tout);
	while (bsize > 0) {
		int rv, rbytes;
		int rsize = bsize > qa ? qa : bsize; 

//		a1logd(p->log, 8, "icoms_usb_rw: read/write %d bytes this time\n",rsize);
		rv = icoms_usb_transaction(p, cancelt, &rbytes, type, (unsigned char)ep, rbuf, rsize, top);
		if (rv != 0 && rv != ICOM_SHORT) {
			lerr = rv;
			break;
		} else {	/* Account for bytes read/written */
			bsize -= rbytes;
			rbuf += rbytes;
			bread += rbytes;
		}
		if (rbytes != rsize) {
			lerr |= ICOM_SHORT; 
			break;
		}
	}

	if (breadp != NULL)
		*breadp = bread;

	a1logd(p->log, 8, "icoms_usb_rw: returning %d bytes, ICOM err 0x%x\n",bread, lerr);

	return lerr;
}

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Static list so that all open USB/HID connections can be closed on a SIGKILL */
static icoms *icoms_list = NULL;

/* Clean up any open USB ports ready for exit on signal */
static void icoms_cleanup() {
	icoms *pp, *np;

#if defined(UNIX)
	/* This is a bit of a hack to compensate for the fact */
	/* that a ^C will kill the program while ICANON is off. */
	/* It's really better to restore the original attributes, */
	/* even when USB is not compiled in. */
	struct termios news;
	if (tcgetattr(STDIN_FILENO, &news) >= 0) {
		news.c_lflag |= (ICANON | ECHO);
		tcsetattr(STDIN_FILENO,TCSANOW, &news);
	}
#endif

	for (pp = icoms_list; pp != NULL; pp = np) { 
		np = pp->next;
		a1logd(pp->log, 6, "icoms_cleanup: closing usb port 0x%x\n",pp);
		/* There's a problem here if have more than one USB port */
		/* open - win32 doesn't return from the system call. */
		/* Should we depend on usb read/write routines to call cleanup ? */
		pp->close_port(pp);
	}
}

#ifdef NT
void (__cdecl *usbio_int)(int sig) = SIG_DFL;
void (__cdecl *usbio_term)(int sig) = SIG_DFL;
#endif
#ifdef UNIX
void (*usbio_hup)(int sig) = SIG_DFL;
void (*usbio_int)(int sig) = SIG_DFL;
void (*usbio_term)(int sig) = SIG_DFL;
#endif

/* On something killing our process, deal with USB cleanup */
static void icoms_sighandler(int arg) {
	static amutex_static(lock);

	a1logd(g_log, 6, "icoms_sighandler: invoked with arg = %d\n",arg);

	/* Make sure we don't re-enter */
	if (amutex_trylock(lock)) {
		return;
	}

	if (in_usb_rw != 0)
		in_usb_rw = -1;
	icoms_cleanup();

	/* Call the existing handlers */
#ifdef UNIX
	if (arg == SIGHUP && usbio_hup != SIG_DFL && usbio_hup != SIG_IGN)
		usbio_hup(arg);
#endif /* UNIX */
	if (arg == SIGINT && usbio_int != SIG_DFL && usbio_int != SIG_IGN)
		usbio_int(arg);
	if (arg == SIGTERM && usbio_term != SIG_DFL && usbio_term != SIG_IGN) 
		usbio_term(arg);

	a1logd(g_log, 6, "icoms_sighandler: calling exit()\n");

	amutex_unlock(lock);
	exit(0);
}

/* - - - - - - - - - - - - - - - - - - - */

/* Install the cleanup signal handlers */
void usb_install_signal_handlers(icoms *p) {
	if (icoms_list == NULL) {
		a1logd(g_log, 6, "usb_install_signal_handlers: called\n");
#if defined(UNIX)
		usbio_hup = signal(SIGHUP, icoms_sighandler);
#endif /* UNIX */
		usbio_int = signal(SIGINT, icoms_sighandler);
		usbio_term = signal(SIGTERM, icoms_sighandler);
	}

	/* Add it to our static list, to allow automatic cleanup on signal */
	p->next = icoms_list;
	icoms_list = p;
	a1logd(g_log, 6, "usb_install_signal_handlers: done\n");
}

/* Delete an icoms from our static signal cleanup list */
void usb_delete_from_cleanup_list(icoms *p) {

	/* Find it and delete it from our static cleanup list */
	if (icoms_list != NULL) {
		if (icoms_list == p) {
			icoms_list = p->next;
			if (icoms_list == NULL) {
#if defined(UNIX)
				signal(SIGHUP, usbio_hup);
#endif /* UNIX */
				signal(SIGINT, usbio_int);
				signal(SIGTERM, usbio_term);
			}
		} else {
			icoms *pp;
			for (pp = icoms_list; pp != NULL; pp = pp->next) { 
				if (pp->next == p) {
					pp->next = p->next;
					break;
				}
			}
		}
	}
}

/* ========================================================= */
/* USB write/read "serial" imitation */

/* Write the characters in the buffer out */
/* Data will be written up to the terminating nul */
/* Return relevant error status bits */
static int
icoms_usb_ser_write(
icoms *p,
char *wbuf,			/* null terminated unless nch > 0 */
int nwch,			/* if > 0, number of characters to write */
double tout)
{
	int len, wbytes;
	long ttop, top;			/* Total timeout period, timeout period */
	unsigned int stime, etime;		/* Start and end times of USB operation */
	int ep = p->wr_ep;		/* End point */
	icom_usb_trantype type;	/* bulk or interrupt */
	int retrv = ICOM_OK;

	a1logd(p->log, 8, "\nicoms_usb_ser_write: writing '%s'\n",
	       nwch > 0 ? icoms_tohex((unsigned char *)wbuf, nwch) : icoms_fix(wbuf));

	if (!p->is_open) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_ser_write: device is not open\n");
		return ICOM_SYS;
	}

	if (p->EPINFO(ep).valid == 0) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_ser_write: invalid end point 0x%02x\n",ep);
		return ICOM_SYS;
	}
	
	if (p->EPINFO(ep).type != ICOM_EP_TYPE_BULK
	 && p->EPINFO(ep).type != ICOM_EP_TYPE_INTERRUPT) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_ser_write: unhandled end point type %d",p->EPINFO(ep).type);
		return ICOM_SYS;
	}

	if (p->EPINFO(ep).type == ICOM_EP_TYPE_BULK)
		type = icom_usb_trantype_bulk;
	else
		type = icom_usb_trantype_interrutpt;

	if (nwch != 0)
		len = nwch;
	else
		len = strlen(wbuf);

	ttop = (int)(tout * 1000.0 + 0.5);        /* Total timeout period in msecs */

	a1logd(p->log, 8, "\nicoms_usb_ser_write: ep 0x%x, bytes %d, ttop %d, quant %d\n", p->rd_ep, len, ttop, p->rd_qa);

	etime = stime = msec_time();

	/* Until data is all written, we time out, or the user aborts */
	for (top = ttop; top > 0 && len > 0;) {
		int rv;
		a1logd(p->log, 8, "icoms_usb_ser_write: attempting to write %d bytes to usb top = %d\n",len,top);

		rv = icoms_usb_transaction(p, NULL, &wbytes, type, (unsigned char)ep, (unsigned char *)wbuf, len, top);
		etime = msec_time();
		if (rv != ICOM_OK) {
			if (rv != ICOM_TO) {
				retrv |= rv;
				break;
			}
		} else {	/* Account for bytes written */
			a1logd(p->log, 8, "icoms_usb_ser_write: wrote %d bytes\n",wbytes);
			wbuf += wbytes;
			len -= wbytes;
		}
		top = ttop - (etime - stime);	/* Remaining time */
	}

	if (top <= 0) {			/* Must have timed out */
		a1logd(p->log, 8, "icoms_usb_ser_write: timeout, took %d msec out of %d\n",etime - stime,ttop);
		retrv |= ICOM_TO; 
	}

	a1logd(p->log, 8, "icoms_usb_ser_write: took %d msec, returning ICOM err 0x%x\n",etime - stime,retrv);

	return retrv;
}


/* Read characters into the buffer */
/* Return string will be terminated with a nul */
/* Read only in packet sized chunks, and retry if */
/* the bytes requested aren't read, untill we get a */
/* timeout or a terminating char is read */
static int
icoms_usb_ser_read(icoms *p,
char *rbuf,			/* Buffer to store characters read */
int bsize,			/* Buffer size */
int *pbread,		/* Bytes read (not including forced '\000') */
char *tc,			/* Terminating characers, NULL for none or char count mode */
int ntc,			/* Number of terminating characters or char count needed */
double tout)		/* Time out in seconds */
{
	int j, rbytes;
	long ttop, top;			/* Total timeout period, timeout period */
	unsigned int stime, etime;		/* Start and end times of USB operation */
	char *rrbuf = rbuf;		/* Start of return buffer */
	int ep = p->rd_ep;		/* End point */
	icom_usb_trantype type;	/* bulk or interrupt */
	int retrv = ICOM_OK;
	int nreads;			/* Number of reads performed */
	int fastserial = 0;		/* If fast serial type */

	if (!p->is_open) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_ser_read: device is not open\n");
		return ICOM_SYS;
	}

	if (p->EPINFO(ep).valid == 0) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_ser_read: invalid end point 0x%02x\n",ep);
		return ICOM_SYS;
	}
	
	if (p->EPINFO(ep).type != ICOM_EP_TYPE_BULK
	 && p->EPINFO(ep).type != ICOM_EP_TYPE_INTERRUPT) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_ser_read: unhandled end point type %d",p->EPINFO(ep).type);
		return ICOM_SYS;
	}

	if (p->EPINFO(ep).type == ICOM_EP_TYPE_BULK)
		type = icom_usb_trantype_bulk;
	else
		type = icom_usb_trantype_interrutpt;

	if (bsize < 3) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_ser_read given too small a buffer (%d)", bsize);
		return ICOM_SYS;
	}

	if (p->dctype & icomt_fastserial)
		fastserial  = 1;

	for (j = 0; j < bsize; j++)
		rbuf[j] = 0;
 
	/* The DTP94 doesn't cope with a timeout on OS X, so we need to avoid */
	/* them by giving each read the largest timeout period possible. */
	/* This also reduces the problem of libusb 0.1 not returning the */
	/* number of characters read on a timeout. */

	ttop = (int)(tout * 1000.0 + 0.5);        /* Total timeout period in msecs */

	a1logd(p->log, 8, "\nicoms_usb_ser_read: ep 0x%x, bytes %d, ttop %d, ntc %d, quant %d\n", p->rd_ep, bsize, ttop, ntc, p->rd_qa);

	bsize -= 1;				/* Allow space for null */
	bsize -= p->ms_bytes;	/* Allow space for modem status bytes */

	j = (tc == NULL && ntc <= 0) ? -1 : 0;
	etime = stime = msec_time();

	/* Until data is all read, we time out, or the user aborts */
	for (top = ttop, nreads = 0; top > 0 && bsize > 0 && j < ntc ;) {
		int c, rv;
		int rsize = bsize; 

		/* If not a fast USB/BT serial port, read in quanta size chunks */ 
		if (!fastserial && rsize > p->rd_qa)
			rsize = p->rd_qa;

		a1logd(p->log, 8, "icoms_usb_ser_read: attempting to read %d bytes from usb, top = %d, j = %d\n",rsize,top,j);

		rv = icoms_usb_transaction(p, NULL, &rbytes, type, (unsigned char)ep, (unsigned char *)rbuf, rsize, top);
		etime = msec_time();
		nreads++;

		if (rbytes > 0) {	/* Account for bytes read */

			/* Account for modem status bytes. Modem bytes are per usb read, */
			/* or every p->rd_qa bytes. */
			if (p->ms_bytes) {		/* Throw away modem bytes */
				char *bp = rbuf;
				int rb = rbytes;
				for (; rb > 0; ) {
					int nb = rb < p->ms_bytes ? rb : p->ms_bytes;	/* Bytes to shift */
					if (p->interp_ms != NULL && nb >= p->ms_bytes)
						retrv |= p->interp_ms(p, (unsigned char *)bp);	/* Deal with error flags in ms bytes */
					a1logd(p->log, 8, "icoms_usb_ser_read: discarded %d modem bytes 0x%02x 0x%02x\n",nb,nb >= 1 ? (bp[0] & 0xff) : 0, nb >= 2 ? (bp[1] & 0xff) : 0);
					rb -= nb;
					rbytes -= nb;
					memmove(bp, bp+nb, rb);
					bp += p->rd_qa - p->ms_bytes;
					rb -= p->rd_qa - p->ms_bytes;
				}
				rbuf[rbytes] = 0;
			}

			a1logd(p->log, 8, "icoms_usb_ser_read: read %d bytes, rbuf = '%s'\n",rbytes,icoms_fix(rrbuf));

			bsize -= rbytes;
			if (tc != NULL) {
				while(rbytes--) {	/* Count termination characters */
					char ch = *rbuf++, *tcp = tc;
					
					while(*tcp != '\000') {
						if (ch == *tcp)
							j++;
						tcp++;
					}
				}
				a1logd(p->log, 8, "icoms_usb_ser_read: tc count %d\n",j);
			} else {
				if (ntc > 0)
					j += rbytes;
				rbuf += rbytes;
			}
		}

		/* Deal with any errors */
		if (rv != ICOM_OK && rv != ICOM_SHORT) {
			a1logd(p->log, 8, "icoms_usb_ser_read: read failed with 0x%x, rbuf = '%s'\n",rv,icoms_fix(rrbuf));
			retrv |= rv;
			break;
		}
		top = ttop - (etime - stime);	/* Remaining time */
	}

	*rbuf = '\000';
	a1logd(p->log, 8, "icoms_usb_ser_read: read %d total bytes with %d reads\n",rbuf - rrbuf, nreads);
	if (pbread != NULL)
		*pbread = (rbuf - rrbuf);

	/* If ran out of time and not completed */
	a1logd(p->log, 8, "icoms_usb_ser_read: took %d msec\n",etime - stime);
	if (top <= 0 && bsize > 0 && j < ntc) {
		a1logd(p->log, 8, "icoms_usb_ser_read: timeout, took %d msec out of %d\n",etime - stime,ttop);
		retrv |= ICOM_TO; 
	}

	a1logd(p->log, 8, "icoms_usb_ser_read: took %d msec, returning '%s' ICOM err 0x%x\n",
	       etime - stime, tc == NULL && ntc > 0 ? icoms_tohex((unsigned char*)rrbuf, rbuf - rrbuf) : icoms_fix(rrbuf), retrv);

	return retrv;
}


/* ------------------------------------------------- */

/* Set the usb port number and characteristics. */
/* This may be called to re-establish a connection that has failed */
/* return an icom error */
static int
icoms_set_usb_port(
icoms *p, 
int    config,			/* Configuration */
int    wr_ep,			/* "Serial" Write end point */
int    rd_ep,			/* "Serial" Read end point */
icomuflags usbflags,	/* Any special handling flags */
int retries,			/* > 0 if we should retry set_configuration (100msec) */ 
char **pnames			/* List of process names to try and kill before opening */
) {
	a1logd(p->log, 8, "icoms_set_usb_port: About to set usb port characteristics\n");

	if (p->port_type(p) & icomt_usb) {
		int rv;

		if (p->is_open) 
			p->close_port(p);

		if ((rv = usb_open_port(p, config, wr_ep, rd_ep, usbflags, retries, pnames)) != ICOM_OK) {
			return rv;
		}

		p->write = icoms_usb_ser_write;
		p->read = icoms_usb_ser_read;

	} else {
		a1logd(p->log, 8, "icoms_set_usb_port: Not a USB port!\n");
		return ICOM_NOTS;
	}
	a1logd(p->log, 6, "icoms_set_usb_port: usb port characteristics set ok\n");


	return ICOM_OK;
}

/* ---------------------------------------------------------------------------------*/

/* Set the USB specific icoms methods */
void usb_set_usb_methods(
icoms *p
) {
	p->set_usb_port     = icoms_set_usb_port;
	p->usb_control      = icoms_usb_control;
	p->usb_read         = icoms_usb_rw;
	p->usb_write        = icoms_usb_rw;
	p->usb_wait_io      = icoms_usb_wait_io;
	p->usb_cancel_io    = icoms_usb_cancel_io;
	p->usb_resetep      = icoms_usb_resetep;
	p->usb_clearhalt    = icoms_usb_clearhalt;
}

/* ---------------------------------------------------------------------------------*/


#endif /* ENABLE_USB */
