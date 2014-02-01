
/* General USB I/O support, BSD native implementation. */
/* This file is conditionaly #included into usbio.c */
 
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

/*
	!!!! This driver is incomplete and non-functional !!!!

	BSD uses fd per end point, so simplifies things.

	No clear ep or abort i/o though, so we could try clear halt,
	or close fd and see if that works in aborting transaction ?

	Posix aio would probably work, but it's not loaded by default :-(

	Could use libusb20 API, but not backwards or cross compatible,
	and is very likely to be buggy ?

 */

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glob.h>
#include <sys/ioctl.h>
#if defined(__FreeBSD__)
# include <dev/usb/usb_ioctl.h>		/* Not sure what's going on with FreeBSD... */
#else
# include <dev/usb/usb.h>			/* The usual include for BSD */
#endif 

/* select() defined, but not poll(), so emulate poll() */
#if defined(FD_CLR) && !defined(POLLIN)
#include "pollem.h"
#define poll_x pollem
#else
#include <sys/poll.h>	/* Else assume poll() is native */
#define poll_x poll
#endif

/* Add paths to USB connected instruments */
/* Return an icom error */
int usb_get_paths(
icompaths *p 
) {
	int i, j;
	char *paths[] = {
#if defined(__FreeBSD__)
	    "/dev/usb/[0-9]*.*.0",		/* FreeBSD >= 8 */
	    "/dev/ugen[0-9]*",			/* FreeBSD < 8, but no .E */
#else
	    "/dev/ugen/[0-9]*.00",		/* NetBSD, OpenBSD */
#endif
	    NULL
	};
	int vid, pid;
	int nconfig = 0, nep = 0;
	char *dpath;
	instType itype;
	struct usb_idevice *usbd = NULL;

	a1logd(p->log, 6, "usb_get_paths: about to look through usb devices:\n");

	/* See what device names to look for */
	for (j = 0; ; j++) {
		glob_t g;
		int fd;
		struct usb_device_info di;
		int rv, found = 0;

		if (paths[j] == NULL)
			break;

		memset(&g, 0, sizeof(g));

		if (glob(paths[j], GLOB_NOSORT, NULL, &g) != 0) {
			continue;
		}

		/* For all the nodes found by the glob */
		for (i = 0; i < g.gl_pathc; i++) {

#if defined(__FreeBSD__)
			/* Skip anything with an end point number */
			if (j == 1 && strchr(g.gl_pathv[i], '.') != NULL)
				continue;
#endif
			if ((fd = open(g.gl_pathv[i], O_RDONLY)) < 0)
				continue;

			if (ioctl(fd, USB_GET_DEVICEINFO, &di) < 0)
				continue;

			vid = di.udi_vendorNo;
			pid = di.udi_productNo;

			a1logd(p->log, 6, "usb_get_paths: checking vid 0x%04x, pid 0x%04x\n",vid,pid);

			/* Do a preliminary match */
			if ((itype = inst_usb_match(vid, pid, 0)) == instUnknown) {
				a1logd(p->log, 6 , "usb_get_paths: instrument not reconized\n");
				continue;
			}

			// ~~99 need to check number of end points ~~~
			// ~~99 and number of configs
			nconfig = 1;
 
//USB_GET_DEVICEINFO	struct usb_device_info
//USB_GET_DEVICE_DESC	struct usb_device_descriptor

			/* Allocate an idevice so that we can fill in the end point information */
			if ((usbd = (struct usb_idevice *) calloc(sizeof(struct usb_idevice), 1)) == NULL) {
				a1loge(p->log, ICOM_SYS, "icoms: calloc failed!\n");
				close(fd);
				globfree(&g);
				return ICOM_SYS;
			}

			usbd->nconfig = nconfig;
			
			/* Found a known instrument ? */
			if ((itype = inst_usb_match(vid, pid, nep)) != instUnknown) {
				char pname[400], *cp;
		
				a1logd(p->log, 2, "usb_get_paths: found instrument vid 0x%04x, pid 0x%04x\n",vid,pid);
		
				/* Create the base device path */
				dpath = g.gl_pathv[i];
#if defined(__FreeBSD__)
				if (j == 0) {		/* Remove .0 */
					if ((cp = strrchr(dpath, '.')) != NULL
					 && cp[1] == '0' && cp[2] == '\000')
						*cp = '\000';
				}
#else
				/* Remove .00 */
				if ((cp = strrchr(dpath, '.')) != NULL
				 && cp[1] == '0' && cp[2] == '0' && cp[3] == '\000')
					*cp = '\000';
#endif
				if ((usbd->dpath = strdup(dpath)) == NULL) {
					a1loge(p->log, ICOM_SYS, "usb_get_paths: strdup path failed!\n");
					free(usbd);
					close(fd);
					globfree(&g);
					return ICOM_SYS;
				}

				/* Create a path/identification */
				sprintf(pname,"%s (%s)", dpath, inst_name(itype));
				if ((usbd->dpath = strdup(dpath)) == NULL) {
					a1loge(p->log, ICOM_SYS, "usb_get_paths: strdup path failed!\n");
					free(usbd);
					close(fd);
					globfree(&g);
					return ICOM_SYS;
				}
		
				/* Add the path and ep info to the list */
				if ((rv = p->add_usb(p, pname, vid, pid, nep, usbd, itype)) != ICOM_OK)
					return rv;


			} else {
				free(usbd);
			}

			close(fd);
		}
		globfree(&g);

		/* Only try one glob string */
		break;
	}

	a1logd(p->log, 8, "usb_get_paths: returning %d paths and ICOM_OK\n",p->npaths);
	return ICOM_OK;
}

/* Copy usb_idevice contents from icompaths to icom */
/* return icom error */
int usb_copy_usb_idevice(icoms *d, icompath *s) {
	int i;
	if (s->usbd == NULL) { 
		d->usbd = NULL;
		return ICOM_OK;
	}
	if ((d->usbd = calloc(sizeof(struct usb_idevice), 1)) == NULL) {
		a1loge(d->log, ICOM_SYS, "usb_copy_usb_idevice: malloc\n");
		return ICOM_SYS;
	}
	if ((d->usbd->dpath = strdup(s->usbd->dpath)) == NULL) {
		a1loge(d->log, ICOM_SYS, "usb_copy_usb_idevice: malloc\n");
		return ICOM_SYS;
	}
	/* Copy the ep info */
	d->nconfig = s->usbd->nconfig;
	d->nifce = s->usbd->nifce;
	d->config = s->usbd->config;
	for (i = 0; i < 32; i++)
		d->ep[i] = s->usbd->ep[i];		/* Struct copy */
	return ICOM_OK;
}

/* Cleanup and then free a usb dev entry */
void usb_del_usb_idevice(struct usb_idevice *usbd) {

	if (usbd == NULL)
		return;

	if (usbd->dpath != NULL)
		free(usbd->dpath);
	free(usbd);
}

/* Cleanup any USB specific icoms state */
void usb_del_usb(icoms *p) {

	usb_del_usb_idevice(p->usbd);
}

/* Close an open USB port */
/* If we don't do this, the port and/or the device may be left in an unusable state. */
void usb_close_port(icoms *p) {

	a1logd(p->log, 6, "usb_close_port: called\n");

#ifdef NEVER    // ~~99
	if (p->is_open && p->usbd != NULL) {
		struct usbdevfs_urb urb;
		unsigned char buf[8+IUSB_DESC_TYPE_DEVICE_SIZE];
		int iface, rv;

		/* Release all the interfaces */
		for (iface = 0; iface < p->nifce; iface++)
			ioctl(p->usbd->fd, USBDEVFS_RELEASEINTERFACE, &iface);

		/* Workaround for some bugs - reset device on close */
		if (p->uflags & icomuf_reset_before_close) {
			if ((rv = ioctl(p->usbd->fd, USBDEVFS_RESET, NULL)) != 0) {
				a1logd(p->log, 1, "usb_close_port: reset returned %d\n",rv);
			}
		}

		if (p->usbd->running) {		/* If reaper is still running */
			unsigned char buf[1] = { 0 };

			a1logd(p->log, 6, "usb_close_port: waking reaper thread to trigger exit\n");
			p->usbd->shutdown = 1;

			if (write(p->usbd->sd_pipe[1], buf, 1) < 1) {
				a1logd(p->log, 1, "usb_close_port: writing to sd_pipe failed with '%s'\n", strerror(errno));
				/* Hmm. We could be in trouble ? */
			}
		}
		a1logd(p->log, 6, "usb_close_port: waiting for reaper thread\n");
		pthread_join(p->usbd->thread, NULL);	/* Wait for urb reaper thread to exit */
		close(p->usbd->fd);
		pthread_mutex_destroy(&p->usbd->lock);
		close(p->usbd->sd_pipe[0]);
		close(p->usbd->sd_pipe[1]);

		a1logd(p->log, 6, "usb_close_port: usb port has been released and closed\n");
	}
	p->is_open = 0;
#endif  // ~~99

	/* Find it and delete it from our static cleanup list */
	usb_delete_from_cleanup_list(p);
}

static void *urb_reaper(void *context);		/* Declare */

/* Open a USB port for all our uses. */
/* This always re-opens the port */
/* return icom error */
static int usb_open_port(
icoms *p,
int    config,		/* Configuration number */
int    wr_ep,		/* Write end point */
int    rd_ep,		/* Read end point */
icomuflags usbflags,/* Any special handling flags */
int retries,		/* > 0 if we should retry set_configuration (100msec) */ 
char **pnames		/* List of process names to try and kill before opening */
) {
	int rv, tries = 0;
	a1logd(p->log, 8, "usb_open_port: Make sure USB port is open, tries %d\n",retries);

	if (p->is_open)
		p->close_port(p);

#ifdef NEVER    // ~~99
	/* Make sure the port is open */
	if (!p->is_open) {
		int rv, i, iface;
		kkill_nproc_ctx *kpc = NULL;

		if (config != 1) {
			/* Nothing currently needs it, so we haven't implemented it yet... */
			a1loge(p->log, ICOM_NOTS, "usb_open_port: native driver cant handle config %d\n",config);
			return ICOM_NOTS;
		}

		/* Do open retries */
		for (tries = 0; retries >= 0; retries--, tries++) {

			a1logd(p->log, 8, "usb_open_port: About to open USB port '%s'\n",p->usbd->dpath);

			if (tries > 0) {
				//msec_sleep(i_rand(50,100));
				msec_sleep(77);
			}

			p->uflags = usbflags;

			/* We should only do a set configuration if the device has more than one */
			/* possible configuration and it is currently not the desired configuration, */
			/* but we should avoid doing a set configuration if the OS has already */
			/* selected the configuration we want, since two set configs seem to */
			/* mess up the Spyder2, BUT we can't do a get config because this */
			/* messes up the i1pro-D. */

			/* Linux set_configuration(1) by default, so we don't need to do anything */
			p->cconfig = 1;

			if (p->cconfig != config) {
				if ((rv = ioctl(p->usbd->fd, USBDEVFS_SETCONFIGURATION, &config)) != 0) {
					a1logd(p->log, 1, "icoms_usb_setconfig failed with %d\n",rv);
					return ICOM_SYS;
				}
				p->cconfig = config;
				a1logd(p->log, 6, "usb_open_port: set config %d OK\n",config);
			}

			/* We're done */
			break;
		}

		if (kpc != NULL)
			kpc->del(kpc); 

		/* Claim all the interfaces */
		for (iface = 0; iface < p->nifce; iface++) {

			if ((rv = ioctl(p->usbd->fd, USBDEVFS_CLAIMINTERFACE, &iface)) < 0) {
				struct usbdevfs_getdriver getd;
				getd.interface = iface;

				a1logd(p->log, 1, "usb_open_port: Claiming USB port '%s' interface %d initally failed with %d\n",p->usbd->dpath,iface,rv);

				/* Detatch the existing interface if kernel driver is active. */
				if (p->uflags & icomuf_detach
				 && ioctl(p->usbd->fd, USBDEVFS_GETDRIVER, &getd) == 0) {
					struct usbdevfs_ioctl cmd;
					a1logd(p->log, 1, "usb_open_port: Attempting kernel detach\n");
					cmd.ifno = iface;
					cmd.ioctl_code = USBDEVFS_DISCONNECT;
					cmd.data = NULL;
				 	ioctl(p->usbd->fd, USBDEVFS_IOCTL, &cmd);
					if ((rv = ioctl(p->usbd->fd, USBDEVFS_CLAIMINTERFACE, &iface)) < 0) {
						a1loge(p->log, ICOM_SYS, "usb_open_port: Claiming USB port '%s' interface %d failed after detach with %d\n",p->usbd->dpath,iface,rv);
						return ICOM_SYS;
					}
				} else {
					a1loge(p->log, ICOM_SYS, "usb_open_port: Claiming USB port '%s' interface %d failed with %d\n",p->usbd->dpath,iface,rv);
					return ICOM_SYS;
				}
			}
		}

		/* Clear any errors. */
		/* (Some I/F seem to hang if we do this, some seem to hang if we don't !) */
		/* The ColorMunki on Linux only starts every second time if we don't do this. */
		if (!(p->uflags & icomuf_no_open_clear)) {
			for (i = 0; i < 32; i++) {
				if (!p->ep[i].valid)
					continue;
				p->usb_clearhalt(p, p->ep[i].addr); 
			}
		}

		/* Set "serial" coms values */
		p->wr_ep = wr_ep;
		p->rd_ep = rd_ep;
		p->rd_qa = p->EPINFO(rd_ep).packetsize;
		if (p->rd_qa == 0)
			p->rd_qa = 8;
		a1logd(p->log, 8, "usb_open_port: 'serial' read quanta = packet size = %d\n",p->rd_qa);

		/* Start the reaper thread to handle URB completions */
		if ((rv = pipe(p->usbd->sd_pipe)) < 0) {
			a1loge(p->log, ICOM_SYS, "usb_open_port: creat pipe failed with %d\n",rv);
			return ICOM_SYS;
		}
		pthread_mutex_init(&p->usbd->lock, NULL);
		
		p->usbd->running = 1;
		if ((rv = pthread_create(&p->usbd->thread, NULL, urb_reaper, (void*)p)) < 0) {
			p->usbd->running = 0;
			a1loge(p->log, ICOM_SYS, "usb_open_port: creating urb reaper thread failed with %s\n",rv);
			return ICOM_SYS;
		}

		p->is_open = 1;
		a1logd(p->log, 8, "usb_open_port: USB port is now open\n");
	}

#endif  // ~~99
	/* Install the cleanup signal handlers, and add to our cleanup list */
	usb_install_signal_handlers(p);

	return ICOM_OK;
}

/* - - - - - - - - - - - - - - - - - - - - - */
/* Our universal USB transfer function */
static int icoms_usb_transaction(
	icoms *p,
	usb_cancelt *cancelt,
	int *transferred,
	icom_usb_trantype ttype,	/* transfer type */
	unsigned char endpoint,		/* 0x80 for control write, 0x00 for control read */
	unsigned char *buffer,
	int length,
	unsigned int timeout		/* In msec */
) {
	int type;
	int remlen;
	unsigned char *bp;
	int xlength = 0;
	int i;
	int reqrv = ICOM_OK;

#ifdef NEVER    // ~~99
	in_usb_rw++;
	a1logd(p->log, 8, "icoms_usb_transaction: req type 0x%x ep 0x%x size %d\n",ttype,endpoint,length);

	if (!p->usbd->running) {
		in_usb_rw--;
		a1logv(p->log, 1, "icoms_usb_transaction: reaper thread is not running\n");
		return ICOM_SYS;
	}

	/* Translate icoms transfer type of Linux */
	switch (ttype) {
		case icom_usb_trantype_command:
			type = USBDEVFS_URB_TYPE_CONTROL;
			break;
		case icom_usb_trantype_interrutpt:
			type = USBDEVFS_URB_TYPE_INTERRUPT;
			break;
		case icom_usb_trantype_bulk:
			type = USBDEVFS_URB_TYPE_BULK;
			break;
	}

	/* Setup the icom req and urbs */
	req.urbs = NULL;
	pthread_mutex_init(&req.lock, NULL);
	pthread_cond_init(&req.cond, NULL);

	/* Linux historically only copes with 16384 length urbs, */
	/* so break up longer requests into multiple urbs */

	req.cancelled = 0;
	req.nourbs = req.nurbs = (length + (1 << 14)-1) >> 14;
	if ((req.urbs = (usbio_urb *)calloc(sizeof(usbio_urb), req.nourbs)) == NULL) {
		in_usb_rw--;
		a1loge(p->log, ICOM_SYS, "icoms_usb_transaction: control transfer too big! (%d)\n",length);
		return ICOM_SYS;
	}

	bp = buffer;
	remlen = length;
	for (i = 0; i < req.nurbs; i++) {
		req.urbs[i].req = &req;
		req.urbs[i].urbno = i;
		/* Setup Linux URB */
		req.urbs[i].urb.usercontext = &req.urbs[i];
		req.urbs[i].urb.type = type; 
		if (type != USBDEVFS_URB_TYPE_CONTROL)
			req.urbs[i].urb.endpoint = endpoint;
		if (remlen > 16384)
			req.urbs[i].urb.buffer_length = 16384;
		else
			req.urbs[i].urb.buffer_length = remlen;
		req.urbs[i].urb.buffer = (void *)bp;
		remlen -= req.urbs[i].urb.buffer_length;
		bp += req.urbs[i].urb.buffer_length;
		req.urbs[i].urb.status = -EINPROGRESS;
	}
a1logd(p->log, 8, "icoms_usb_transaction: reset req 0x%p nourbs to %d\n",&req,req.nourbs);

	/* Add our request to the req list so that it can be cancelled on reap failure */
	pthread_mutex_lock(&p->usbd->lock);
	req.next = p->usbd->reqs;
	p->usbd->reqs = &req;
	pthread_mutex_unlock(&p->usbd->lock);

	/* submit the URBs */
	for (i = 0; i < req.nurbs; i++) {
		if ((rv = ioctl(p->usbd->fd, USBDEVFS_SUBMITURB, &req.urbs[i].urb)) < 0) {
			a1logd(p->log, 1, "coms_usb_transaction: Submitting urb to fd %d failed with %d\n",p->usbd->fd, rv);
			req.urbs[i].urb.status = ICOM_SYS;	/* Mark it as failed to submit */
			req.nourbs--;
		}
	}
	
	if (cancelt != NULL) {
		amutex_lock(cancelt->cmtx);
		cancelt->hcancel = (void *)&req;
		cancelt->state = 1;
		amutex_unlock(cancelt->cond);		/* Signal any thread waiting for IO start */
		amutex_unlock(cancelt->cmtx);
	}

	/* Wait for the reaper to wake us, or for a timeout, */
	/* or for the reaper to die. */
	pthread_mutex_lock(&req.lock);
	if (req.nourbs > 0) {
		struct timeval tv;
		struct timespec ts;

		// this is unduly complicated...
		gettimeofday(&tv, NULL);
		ts.tv_sec = tv.tv_sec + timeout/1000;
		ts.tv_nsec = (tv.tv_usec + (timeout % 1000) * 1000) * 1000L;
		if (ts.tv_nsec > 1000000000L) {
			ts.tv_nsec -= 1000000000L;
			ts.tv_sec++;
		}
		
		for(;;) {	/* Ignore spurious wakeups */
			if ((rv = pthread_cond_timedwait(&req.cond, &req.lock, &ts)) != 0) {
				if (rv != ETIMEDOUT) {
					pthread_mutex_unlock(&req.lock);
					a1logd(p->log, 1, "coms_usb_transaction: pthread_cond_timedwait failed with %d\n",rv);
					rv = ICOM_SYS;
					goto done;
				}

				/* Timed out - cancel the remaining URB's */
				a1logd(p->log, 8, "coms_usb_transaction: time out - cancel remaining URB's\n");
				reqrv = ICOM_TO;
				if (!req.cancelled && (rv = cancel_req(p, &req, -1)) != ICOM_OK) {
					pthread_mutex_unlock(&req.lock);
					reqrv = ICOM_SYS;
					/* Since cancelling failed, we can't wait for them to be reaped */
					goto done;
				}
	
				/* Wait for the cancelled URB's to be reaped */
				for (;req.nourbs > 0;) {	/* Ignore spurious wakeups */
					if ((rv = pthread_cond_wait(&req.cond, &req.lock)) != 0) {
						pthread_mutex_unlock(&req.lock);
						a1logd(p->log, 1, "coms_usb_transaction:  pthread_cond_wait failed with %d\n",rv);
						reqrv = ICOM_SYS;
						/* Waiting for reap failed, so give up */
						goto done;
					}
				}
			} else {
				a1logd(p->log, 8, "coms_usb_transaction: reap - %d left\n",req.nourbs);
			}
			if (req.nourbs <= 0)
				break;				/* All urbs's are done */
		}
	}
	pthread_mutex_unlock(&req.lock);

	/* Compute the overall result by going through the urbs. */
	for (i = 0; i < req.nurbs; i++) {
		int stat = req.urbs[i].urb.status;
		xlength += req.urbs[i].urb.actual_length;

		if (stat == ICOM_SYS) {	/* Submit or cancel failed */
			reqrv = ICOM_SYS;
		} else if (reqrv == ICOM_OK && stat < 0 && stat != -ECONNRESET) {	/* Error result */
			if ((endpoint & IUSB_ENDPOINT_DIR_MASK) == IUSB_ENDPOINT_OUT)
				reqrv = ICOM_USBW;
			else
				reqrv = ICOM_USBR;
		} else if (reqrv == ICOM_OK && stat == -ECONNRESET) {	/* Cancelled */
			reqrv = ICOM_CANC;
		} else if (reqrv == ICOM_OK
		        && req.urbs[i].urb.actual_length < req.urbs[i].urb.buffer_length) {
			/* Disregard any following urb's status - they are probably cancelled */
			break;
		}
		/* reqrv == ICOM_TO will ignore urb status */
	}

	if (ttype == icom_usb_trantype_command)
		xlength += IUSB_REQ_HEADER_SIZE;		/* Account for header - linux doesn't */

	/* requested size wasn't transferred ? */
	if (reqrv == ICOM_OK && xlength != length)
		reqrv = ICOM_SHORT;

	if (transferred != NULL)
		*transferred = xlength;

done:;
	if (cancelt != NULL) {
		amutex_lock(cancelt->cmtx);
		cancelt->hcancel = (void *)NULL;
		if (cancelt->state == 0)
			amutex_unlock(cancelt->cond);
		cancelt->state = 2;
		amutex_unlock(cancelt->cmtx);
	}

	/* Remove our request from the list  */
	pthread_mutex_lock(&p->usbd->lock);
	preq = &p->usbd->reqs;
	while (*preq != &req && *preq != NULL)		/* Find it */
		preq = &((*preq)->next);
	if (*preq != NULL)
		*preq = (*preq)->next;
	pthread_mutex_unlock(&p->usbd->lock);

	if (req.urbs != NULL)
		free(req.urbs);
	pthread_cond_destroy(&req.cond);
	pthread_mutex_destroy(&req.lock);

	if (in_usb_rw < 0)
		exit(0);

	in_usb_rw--;

	a1logd(p->log, 8, "coms_usb_transaction: returning err 0x%x and %d bytes\n",reqrv, xlength);
#endif  // ~~99

	return reqrv;
}


/* Return error icom error code */
static int icoms_usb_control_msg(
icoms *p,
int *transferred,
int requesttype, int request,
int value, int index, unsigned char *bytes, int size, 
int timeout) {
	int reqrv = ICOM_OK;
	int dirw = (requesttype & IUSB_REQ_DIR_MASK) == IUSB_REQ_HOST_TO_DEV ? 1 : 0;
	unsigned char *buf;

	a1logd(p->log, 8, "icoms_usb_control_msg: type 0x%x req 0x%x size %d\n",requesttype,request,size);

#ifdef NEVER    // ~~99
	/* Allocate a buffer for the ctrl header + payload */
	if ((buf = calloc(1, IUSB_REQ_HEADER_SIZE + size)) == NULL) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_control_msg: calloc failed\n");
		return ICOM_SYS;
	}

	/* Setup the control header */
	buf[0] = requesttype;
	buf[1] = request;
	short2buf(buf + 2, value);
	short2buf(buf + 4, index);
	short2buf(buf + 6, size);

	/* If it's a write, copy the write data into the buffer */
	if (dirw)
		memcpy(buf + IUSB_REQ_HEADER_SIZE, bytes, size);

	reqrv = icoms_usb_transaction(p, NULL, transferred, icom_usb_trantype_command,   
	                  dirw ? 0x80 : 0x00, buf, IUSB_REQ_HEADER_SIZE + size, timeout);  

	/* If read, copy the data back */
	if (!dirw)
		memcpy(bytes, buf + IUSB_REQ_HEADER_SIZE, size);

	if (transferred != NULL)	/* Adjust for header size requested */
		*transferred -= IUSB_REQ_HEADER_SIZE;

	free(buf);

#endif  // ~~99
	a1logd(p->log, 8, "icoms_usb_control_msg: returning err 0x%x and %d bytes\n",reqrv, *transferred);
	return reqrv;
}

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Time out error return value */

#define USBIO_ERROR_TIMEOUT	-ETIMEDOUT

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Cancel i/o in another thread */
int icoms_usb_cancel_io(
	icoms *p,
	usb_cancelt *cancelt
) {
	int rv = ICOM_OK;
#ifdef NEVER    // ~~99
	a1logd(p->log, 8, "icoms_usb_cancel_io called\n");
	usb_lock_cancel(cancelt);
	if (cancelt->hcancel != NULL)
		rv = cancel_req(p, (usbio_req *)cancelt->hcancel, -1);
	usb_unlock_cancel(cancelt);

	if (rv != ICOM_OK)	/* Assume this could be because of faulty device */
		rv = ICOM_USBW;
#endif  // ~~99

	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Reset and end point data toggle to 0 */
int icoms_usb_resetep(
	icoms *p,
	int ep					/* End point address */
) {
	int rv = ICOM_OK;

#ifdef NEVER    // ~~99
	if ((rv = ioctl(p->usbd->fd, USBDEVFS_RESETEP, &ep)) != 0) {
		a1logd(p->log, 1, "icoms_usb_resetep failed with %d\n",rv);
		rv = ICOM_USBW;
	}
#endif  // ~~99
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Clear a halt on an end point */
int icoms_usb_clearhalt(
	icoms *p,
	int ep					/* End point address */
) {
	int rv = ICOM_OK;

#ifdef NEVER    // ~~99
	if ((rv = ioctl(p->usbd->fd, USBDEVFS_CLEAR_HALT, &ep)) != 0) {
		a1logd(p->log, 1, "icoms_usb_clearhalt failed with %d\n",rv);
		rv = ICOM_USBW;
	}
#endif  // ~~99
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

