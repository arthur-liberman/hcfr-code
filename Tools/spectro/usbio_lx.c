
/* General USB I/O support, Linux native implementation. */
/* ("libusbx ? We don't need no stinking libusbx..." :-) */
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

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/usbdevice_fs.h>

/* select() defined, but not poll(), so emulate poll() */
#if defined(FD_CLR) && !defined(POLLIN)
#include "pollem.h"
#define poll_x pollem
#else
#include <sys/poll.h>	/* Else assume poll() is native */
#define poll_x poll
#endif

/* USB descriptors are little endian */

/* Take a word sized return buffer, and convert it to an unsigned int */
static unsigned int buf2uint(unsigned char *buf) {
	unsigned int val;
	val = buf[3];
	val = ((val << 8) + (0xff & buf[2]));
	val = ((val << 8) + (0xff & buf[1]));
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

/* Take a short sized return buffer, and convert it to an int */
static unsigned int buf2ushort(unsigned char *buf) {
	unsigned int val;
	val = buf[1];
	val = ((val << 8) + (0xff & buf[0]));
	return val;
}

/* Take an int, and convert it into a byte buffer */
static void int2buf(unsigned char *buf, int inv) {
	buf[0] = (inv >> 0) & 0xff;
	buf[1] = (inv >> 8) & 0xff;
	buf[2] = (inv >> 16) & 0xff;
	buf[3] = (inv >> 24) & 0xff;
}

/* Take a short, and convert it into a byte buffer */
static void short2buf(unsigned char *buf, int inv) {
	buf[0] = (inv >> 0) & 0xff;
	buf[1] = (inv >> 8) & 0xff;
}


/* Check a USB Vendor and product ID by reading the device descriptors, */
/* and add the device to the icoms path if it is supported. */
/* Return icom nz error code on fatal error */
static
int usb_check_and_add(
a1log *log,
icompaths *pp,	/* icompaths to add to */
char *dpath		/* path to device */
) {
	int rv;
	unsigned char buf[IUSB_DESC_TYPE_DEVICE_SIZE];
	unsigned vid, pid, nep10 = 0xffff;
	unsigned int configix, nconfig, totlen;
	devType itype;
	struct usb_idevice *usbd = NULL;
	int fd;			/* device file descriptor */

	a1logd(log, 6, "usb_check_and_add: givem '%s'\n",dpath);

	/* Open the device so that we can read it */
	if ((fd = open(dpath, O_RDONLY)) < 0) {
		a1logd(log, 1, "usb_check_and_add: failed to open '%s'\n",dpath);
		return ICOM_OK;
	}

	/* Read the device descriptor */
	if ((rv = read(fd, buf, IUSB_DESC_TYPE_DEVICE_SIZE)) < 0
	  || rv != IUSB_DESC_TYPE_DEVICE_SIZE
	  || buf[0] != IUSB_DESC_TYPE_DEVICE_SIZE
	  || buf[1] != IUSB_DESC_TYPE_DEVICE) {
		a1logd(log, 1, "usb_check_and_add: failed to read device descriptor\n");
		close(fd);
		return ICOM_OK;
	}

	/* Extract the vid and pid */ 	
	vid = buf2ushort(buf + 8);
	pid = buf2ushort(buf + 10);
	nconfig = buf[17];
	
	a1logd(log, 6, "usb_check_and_add: checking vid 0x%04x, pid 0x%04x\n",vid,pid);

	/* Do a preliminary match */
	if ((itype = inst_usb_match(vid, pid, 0)) == instUnknown) {
		a1logd(log, 6 , "usb_check_and_add: instrument not reconized\n");
		close(fd);
		return ICOM_OK;
	}

	/* Allocate an idevice so that we can fill in the end point information */
	if ((usbd = (struct usb_idevice *) calloc(sizeof(struct usb_idevice), 1)) == NULL) {
		a1loge(log, ICOM_SYS, "icoms: calloc failed!\n");
		close(fd);
		return ICOM_SYS;
	}

	usbd->nconfig = nconfig;

	/* Read the configuration descriptors looking for the first configuration, first interface, */
	/* and extract the number of end points */
	for (configix = 0; configix < nconfig; configix++) {
		int configno;
		unsigned int ninfaces, inface, nep;
		unsigned char *buf2, *bp, *zp;

		/* Read the first 4 bytes to get the type and size */
		if ((rv = read(fd, buf, 4)) < 0
		  || rv != 4
		  || buf[1] != IUSB_DESC_TYPE_CONFIG) {
			a1logd(log, 1, "usb_check_and_add: failed to read device config\n");
			free(usbd);
			close(fd);
			return ICOM_OK;
		}

		if ((totlen = buf2ushort(buf + 2)) < 6) {
			a1logd(log, 1, "usb_check_and_add: config desc size strange\n");
			free(usbd);
			close(fd);
			return ICOM_OK;;
		}
		if ((buf2 = calloc(1, totlen)) == NULL) {
			a1loge(log, ICOM_SYS, "usb_check_and_add: calloc of descriptor failed!\n");
			close(fd);
			return ICOM_SYS;
		}

		memcpy(buf2, buf, 4);		/* First 4 bytes read */
		if ((rv = read(fd, buf2 + 4, totlen - 4)) < 0		/* Read the remainder */
		  || rv != (totlen - 4)) {
			a1logd(log, 1, "usb_check_and_add: failed to read device config details\n");
			free(buf2);
			free(usbd);
			close(fd);
			return ICOM_SYS;
		}

		bp = buf2 + buf2[0];	/* Skip coniguration tag */
		zp = buf2 + totlen;		/* Past last bytes */

		/* We are at the first configuration. */
		/* Just read tags and keep track of where we are */
		ninfaces = 0;
		nep = 0;
		usbd->nifce = buf2[4];				/* number of interfaces */
		usbd->config = configno = buf2[5];	/* current configuration */
		for (;bp < zp; bp += bp[0]) {
			int ifaceno;
			if ((bp + 1) >= zp)
				break;			/* Hmm - bodgy, give up */
			if (bp[1] == IUSB_DESC_TYPE_INTERFACE) {
				ninfaces++;
				if ((bp + 2) >= zp)
					break;			/* Hmm - bodgy, give up */
				ifaceno = bp[2];	/* Get bInterfaceNumber */
			} else if (bp[1] == IUSB_DESC_TYPE_ENDPOINT) {
				nep++;
				if ((bp + 5) >= zp)
					break;			/* Hmm - bodgy */
				/* At first config - */
				/* record current nep and end point details */
				if (configno == 1) {
					int ad = bp[2];
					nep10 = nep;
					usbd->EPINFO(ad).valid = 1;
					usbd->EPINFO(ad).addr = ad;
					usbd->EPINFO(ad).packetsize = buf2ushort(bp + 4);
					usbd->EPINFO(ad).type = bp[3] & IUSB_ENDPOINT_TYPE_MASK;
					usbd->EPINFO(ad).interface = ifaceno;
					a1logd(log, 6, "set ep ad 0x%x packetsize %d type %d\n",ad,usbd->EPINFO(ad).packetsize,usbd->EPINFO(ad).type);
				}
			}
			/* Ignore other tags */
		}
		free(buf2);
	}
	if (nep10 == 0xffff) {			/* Hmm. Failed to find number of end points */
		a1logd(log, 1, "usb_check_and_add: failed to find number of end points\n");
		free(usbd);
		close(fd);
		return ICOM_SYS;
	}

	a1logd(log, 6, "usb_check_and_add: found nep10 %d\n",nep10);

	/* Found a known instrument ? */
	if ((itype = inst_usb_match(vid, pid, nep10)) != instUnknown) {
		char pname[400];

		a1logd(log, 2, "usb_check_and_add: found instrument vid 0x%04x, pid 0x%04x\n",vid,pid);

		/* Create a path/identification */
		/* (devnum doesn't seem valid ?) */
		sprintf(pname,"%s (%s)", dpath, inst_name(itype));
		if ((usbd->dpath = strdup(dpath)) == NULL) {
			a1loge(log, ICOM_SYS, "usb_check_and_add: strdup path failed!\n");
			free(usbd);
			close(fd);
			return ICOM_SYS;
		}

		/* Add the path and ep info to the list */
		if ((rv = pp->add_usb(pp, pname, vid, pid, nep10, usbd, itype)) != ICOM_OK) {
			close(fd);
			return rv;
		}



	} else {
		free(usbd);
	}

	close(fd);
	return ICOM_OK;
}

/* Add paths to USB connected instruments */
/* Return an icom error */
int usb_get_paths(
icompaths *p 
) {
	int vid, pid;

	a1logd(p->log, 6, "usb_get_paths: about to look through buses:\n");
	{
		int j;
		char *paths[3] = { "/dev/bus/usb", 		/* current, from udev */
		                   "/proc/bus/usb",		/* old, deprecated */
		                   "/dev" };			/* CONFIG_USB_DEVICE_CLASS, embeded, deprecated ? */
	
		/* See what device names to look for */
		for (j = 0; j < 3; j++) {
			DIR *d1;
			struct dirent *e1;
			int rv, found = 0;
	
			if ((d1 = opendir(paths[j])) == NULL)
				continue;
	
			while ((e1 = readdir(d1)) != NULL) {
				if (e1->d_name[0] == '.')
					continue;
				found = 1;
				if (j < 2) {		/* Directory structure */
					char path1[PATH_MAX];
					char path2[PATH_MAX];
					DIR *d2;
					struct dirent *e2;
					struct stat statbuf;

	                snprintf(path1, PATH_MAX, "%s/%s", paths[j], e1->d_name);

					if ((d2 = opendir(path1)) == NULL)
						continue;
					while ((e2 = readdir(d2)) != NULL) {
						if (e2->d_name[0] == '.')
							continue;
	
		                snprintf(path2, PATH_MAX, "%s/%s/%s", paths[j], e1->d_name, e2->d_name);
						a1logd(p->log, 8, "usb_get_paths: about to stat %s\n",path2);
						if (stat(path2, &statbuf) == 0 && S_ISCHR(statbuf.st_mode)) {
							found = 1;
							if ((rv = usb_check_and_add(p->log, p, path2)) != ICOM_OK) {
								closedir(d1);
								return rv;
							}
						}
					}
					closedir(d2);
				} else {	/* Flat */
					char path2[PATH_MAX];
					struct stat statbuf;

					/* Hmm. This will go badly wrong if this is a /dev/usbdev%d.%d_ep%d, */
					/* since we're not expecting the end points to be separate devices. */
					/* Maybe that's deprectated though... */
	                snprintf(path2, PATH_MAX, "%s/%s", paths[j], e1->d_name);
					a1logd(p->log, 8, "usb_get_paths: about to stat %s\n",path2);
					if (stat(path2, &statbuf) == 0 && S_ISCHR(statbuf.st_mode)) {
						found = 1;
						if ((rv = usb_check_and_add(p->log, p, path2)) != ICOM_OK) {
							closedir(d1);
							return rv;
						}
					}
				}
			}
			closedir(d1);
			if (found)
				break;
		}
	}

	a1logd(p->log, 8, "usb_get_paths: returning %d paths and ICOM_OK\n",p->ndpaths[dtix_combined]);
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

	if (p->is_open && p->usbd != NULL) {
		struct usbdevfs_urb urb;
		unsigned char buf[8+IUSB_DESC_TYPE_DEVICE_SIZE];
		int iface, rv;

		/* Release all the interfaces */
		for (iface = 0; iface < p->nifce; iface++)
			ioctl(p->usbd->fd, USBDEVFS_RELEASEINTERFACE, &iface);

		/* Workaround for some bugs - reset device on close */
		/* !!!! Alternative would be to do reset before open. 
		   On Linux the path stays the same, so could do open/reset/open
		 */
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

			if ((rv = p->usbd->fd = open(p->usbd->dpath, O_RDWR)) < 0) {
				a1logd(p->log, 8, "usb_open_port: open '%s' config %d failed (%d) (Permissions ?)\n",p->usbd->dpath,config,rv);
				if (retries <= 0) {
					if (kpc != NULL) {
						if (kpc->th->result < 0)
							a1logw(p->log, "usb_open_port: killing competing processes failed\n");
						kpc->del(kpc); 
					}
					a1loge(p->log, ICOM_SYS, "usb_open_port: open '%s' config %d failed (%d) (Permissions ?)\n",p->usbd->dpath,config,rv);
					return ICOM_SYS;
				}
				continue;
			} else if (p->debug)
				a1logd(p->log, 2, "usb_open_port: open port '%s' succeeded\n",p->usbd->dpath);

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
		/* (The ColorMunki on some Linux's only starts every second time if we don't do this, */
		/* and on others, every second time if we do.) */
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

	/* Install the cleanup signal handlers, and add to our cleanup list */
	usb_install_signal_handlers(p);

	return ICOM_OK;
}

/*  -------------------------------------------------------------- */
/* I/O structures */

/* an icoms urb */
typedef struct {
	struct _usbio_req *req;		/* Request we're part of */
	int urbno;					/* urb index within request */
	struct usbdevfs_urb urb;	/* Linux urb */
} usbio_urb;

/* an i/o request */
typedef struct _usbio_req {

	int nurbs;					/* Number of urbs in urbs[] */
	usbio_urb *urbs;			/* Allocated */
	volatile int nourbs;		/* Number of outstanding urbs, 0 when done */
	int cancelled;				/* All the URB's have been cancelled */

	pthread_mutex_t lock;		/* Protect req & reaper access */
	pthread_cond_t cond;		/* Signal to thread waiting on req */

	struct _usbio_req *next;	/* Link to next in list */
} usbio_req;

/* - - - - - - - - - - - - - - - - - - - - - */

/* Cancel a req's urbs from the last down to but not including thisurb. */
/* return icom error */
static int cancel_req(icoms *p, usbio_req *req, int thisurb) {
	int i, rv = ICOM_OK;
	for (i = req->nurbs-1; i > thisurb; i--) {
		int ev;
		// ~~99 can we skip done, errored or cancelled urbs ?
		// Does it matter if there is a race between cancellers ? */
		a1logd(p->log, 8, "cancel_req %d\n",i);
		ev = ioctl(p->usbd->fd, USBDEVFS_DISCARDURB, &req->urbs[i].urb);
		if (ev != 0 && ev != EINVAL) {
			/* Hmmm */
			a1loge(p->log, ICOM_SYS, "cancel_req: failed with %d\n",rv);
			rv = ICOM_SYS;
		}
		req->urbs[i].urb.status = -ECONNRESET; 
	}
	req->cancelled = 1;		/* Don't cancel it again */
	return ICOM_OK;
}

/* The reaper thread */
static void *urb_reaper(void *context) {
	icoms *p = (icoms *)context;
	struct usbdevfs_urb *out = NULL;
	int errc = 0;
	int rv;
	struct pollfd pa[2];        /* Poll array to monitor urb result or shutdown */

	a1logd(p->log, 8, "urb_reaper: reap starting\n");

	/* Wait for a URB, and signal the requester */
	for (;;) {
		usbio_urb *iurb;
		usbio_req *req;

		/* Setup to wait for serial output not block */
		pa[0].fd = p->usbd->fd;
		pa[0].events = POLLIN | POLLOUT;
		pa[0].revents = 0;
	
		/* Setup to wait for a shutdown signal via the sd_pipe */
		pa[1].fd = p->usbd->sd_pipe[0];
		pa[1].events = POLLIN;
		pa[1].revents = 0;

		/* Wait for fd to become ready or fail */
		rv = poll_x(pa, 2, -1);

		/* Failed */
		if (rv < 0) {
			a1logd(p->log, 2, "urb_reaper: poll failed with %d\n",rv);
				if (errc++ < 5) {
				continue;
			}
			a1logd(p->log, 2, "urb_reaper: poll failed too many times - shutting down\n");
			p->usbd->shutdown = 1;
			break;
		}

		/* Shutdown event */
		if (pa[1].revents != 0) {
			a1logd(p->log, 6, "urb_reaper: poll returned events %d %d - shutting down\n",
			                   pa[0].revents,pa[1].revents);
			p->usbd->shutdown = 1;
			break;
		}

		/* Hmm. poll returned without event from fd. */
		if (pa[0].revents == 0) {
			a1logd(p->log, 8, "urb_reaper: poll returned events %d %d - ignoring\n",
			                   pa[0].revents,pa[1].revents);
			continue;
		}

		/* Not sure what this returns if there is nothing there */
		rv = ioctl(p->usbd->fd, USBDEVFS_REAPURBNDELAY, &out);

		if (rv == EAGAIN) {
			a1logd(p->log, 8, "urb_reaper: reap returned EAGAIN - ignored\n");
			continue;
		}
		if (rv < 0) {
			a1logd(p->log, 8, "urb_reaper: reap failed with %d\n",rv);
				if (errc++ < 5) {
				continue;
			}
			a1logd(p->log, 8, "urb_reaper: reap failed too many times - shutting down\n");
			p->usbd->shutdown = 1;
			break;
		}

		errc = 0;

		if (out == NULL) {
			a1logd(p->log, 8, "urb_reaper: reap returned NULL URB - ignored\n");
			continue;
		}

		/* Normal reap */
		iurb = (usbio_urb *)out->usercontext;
		req = iurb->req;

		a1logd(p->log, 8, "urb_reaper: urb reap URB %d with status %d, bytes %d, urbs left %d\n",iurb->urbno, out->status, out->actual_length, req->nourbs-1);


		pthread_mutex_lock(&req->lock);	/* Stop requester from missing reap */
		req->nourbs--;					/* We're reaped one */

		/* If urb failed or is done (but not cancelled), cancel all the following urbs */
		if (req->nourbs > 0 && !req->cancelled
		 && ((out->actual_length < out->buffer_length)
		   || (out->status < 0 && out->status != -ECONNRESET))) {
			a1logd(p->log, 8, "urb_reaper: reaper canceling failed or done urb's\n",rv);
			if (cancel_req(p, req, iurb->urbno) != ICOM_OK) {
				pthread_mutex_unlock(&req->lock);
				/* Is this fatal ? Assume so for the moment ... */
				break;
			} 
		}
		if (req->nourbs <= 0)		/* Signal the requesting thread that we're done */
			pthread_cond_signal(&req->cond);
		pthread_mutex_unlock(&req->lock);
	}

	/* Clean up */
	if (p->usbd->shutdown) {
		usbio_req *req;

		a1logd(p->log, 8, "urb_reaper: shutdown or too many failure\n");

		/* Signal that any request should give up, and that the */
		/* reaper thread is going to exit */
		p->usbd->running = 0;

		/* Go through the outstanding request list, and */
		/* mark them as failed and signal them all */
		pthread_mutex_lock(&p->usbd->lock);
		req = p->usbd->reqs;
		while(req != NULL) {
			int i;

			pthread_mutex_lock(&req->lock);	
			for (i = req->nourbs-1; i >= 0; i--) {
				req->urbs[i].urb.status = -ENOMSG;	/* Use ENOMSG as error marker */
			}
			req->nourbs = 0;
			pthread_cond_signal(&req->cond);
			pthread_mutex_unlock(&req->lock);
			req = req->next;
		}
		pthread_mutex_unlock(&p->usbd->lock);
		a1logd(p->log, 8, "urb_reaper: cleared requests\n");
	}
	p->usbd->running = 0;

	a1logd(p->log, 6, "urb_reaper: thread done\n");
	return NULL;
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
	int reqrv = ICOM_OK, rv = 0;
	usbio_req req, **preq;
	int type;
	int remlen;
	unsigned char *bp;
	int xlength = 0;
	int i;

	in_usb_rw++;
	a1logd(p->log, 8, "icoms_usb_transaction: req type 0x%x ep 0x%x size %d to %d\n",ttype,endpoint,length, timeout);

	if (transferred != NULL)
		*transferred = 0;

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
	a1logd(p->log, 8, "icoms_usb_transaction: set req %p nourbs to %d\n",&req,req.nourbs);

	/* Add our request to the req list so that it can be cancelled on reap failure */
	pthread_mutex_lock(&p->usbd->lock);
	req.next = p->usbd->reqs;
	p->usbd->reqs = &req;
	pthread_mutex_unlock(&p->usbd->lock);

	/* submit the URBs */
	for (i = 0; i < req.nurbs; i++) {
		if ((rv = ioctl(p->usbd->fd, USBDEVFS_SUBMITURB, &req.urbs[i].urb)) < 0) {
			a1logd(p->log, 1, "coms_usb_transaction: Submitting urb to fd %d failed with %d\n",p->usbd->fd, rv);
			req.urbs[i].urb.status = -ENOMSG;	/* Mark it as failed to submit */
			req.nourbs--;
		}
	}
	
	if (cancelt != NULL) {
		amutex_lock(cancelt->cmtx);
		cancelt->hcancel = (void *)&req;
		cancelt->state = 1;
		amutex_unlock(cancelt->condx);	/* Signal any thread waiting for IO start */
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

		if (stat == -ENOMSG) {	/* Submit or cancel failed */
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
			amutex_unlock(cancelt->condx);
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
	a1logd(p->log, 8, "icoms_usb_cancel_io called\n");
	amutex_lock(cancelt->cmtx);
	if (cancelt->hcancel != NULL)
		rv = cancel_req(p, (usbio_req *)cancelt->hcancel, -1);
	amutex_unlock(cancelt->cmtx);

	if (rv != ICOM_OK)	/* Assume this could be because of faulty device */
		rv = ICOM_USBW;

	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Reset and end point data toggle to 0 */
int icoms_usb_resetep(
	icoms *p,
	int ep					/* End point address */
) {
	int rv = ICOM_OK;

	if ((rv = ioctl(p->usbd->fd, USBDEVFS_RESETEP, &ep)) != 0) {
		a1logd(p->log, 1, "icoms_usb_resetep failed with %d\n",rv);
		rv = ICOM_USBW;
	}
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Clear a halt on an end point */
int icoms_usb_clearhalt(
	icoms *p,
	int ep					/* End point address */
) {
	int rv = ICOM_OK;

	if ((rv = ioctl(p->usbd->fd, USBDEVFS_CLEAR_HALT, &ep)) != 0) {
		a1logd(p->log, 1, "icoms_usb_clearhalt failed with %d\n",rv);
		rv = ICOM_USBW;
	}
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

