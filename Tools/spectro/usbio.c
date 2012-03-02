
 /* General USB I/O support */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   2006/22/4
 *
 * Copyright 2006 - 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* These routines supliement the class code in ntio.c and unixio.c */
/* with common and USB specific routines */
/* Rename to icoms.c ?? */

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
#endif
#include "numsup.h"
#include "xspect.h"
#include "insttypes.h"
#include "icoms.h"
#include "conv.h"
#include "usbio.h"
#include "hidio.h"

#ifdef USE_LIBUSB1
# include "libusb.h"
#else
# include "usb.h"
#endif

#ifdef ENABLE_USB

/* To simplify error messages: */
#ifdef USE_LIBUSB1
# define USB_STRERROR(RV) libusb_strerror(RV)
#else
# define USB_STRERROR(RV) usb_strerror()
#endif

/* Check a USB Vendor and product ID, and add the device */
/* to the icoms path if it is supported. Return nz if it was added. */
static int usb_check_and_add(
struct _icoms *p,
struct usb_device *dev
) {
	instType itype;
	struct usb_device_descriptor descriptor;

#ifdef USE_LIBUSB1
	if (libusb_get_device_descriptor(dev, &descriptor) != LIBUSB_SUCCESS)
		return 1;
#else
	descriptor = dev->descriptor;	/* Copy */
#endif
	if (p->debug) fprintf(stderr,"usb_check_and_add() called with VID 0x%x, PID 0x%x\n",descriptor.idVendor, descriptor.idProduct);

#ifdef USE_LIBUSB1
#if defined(NT) || defined(__APPLE__)
	/* Ignore libusb1 HID driver capability */
	{
		struct libusb_config_descriptor *config = NULL;

		if (descriptor.bDeviceClass != LIBUSB_CLASS_HID
		  && libusb_get_config_descriptor(dev, 0, &config) != LIBUSB_SUCCESS) {
			if (p->debug) fprintf(stderr,"Get config desc. 0 failed\n");
			return 1;
		}

		if (descriptor.bDeviceClass == LIBUSB_CLASS_HID
		 || (config->bNumInterfaces > 0
		  && config->interface[0]. num_altsetting > 0
		  && config->interface[0].altsetting[0].bInterfaceClass == LIBUSB_CLASS_HID)) {
			int i;
			/* See if this devices is already in the list via the HID interface */
			/* (This may not be 100% correct in the face of multiple instances
			   of the same device, if Windows allows different drivers for different
			   instances of the same device type.) */
			for (i = 0; i < p->npaths; i++) {
				if (p->paths[i]->vid == descriptor.idVendor
				 && p->paths[i]->pid == descriptor.idProduct)
					break;		/* Yes */
			}
			if (i < p->npaths) {
				if (p->debug) fprintf(stderr,"Is an HID device and already added\n");
				if (config != NULL)
					libusb_free_config_descriptor(config);
				return 0;
			}
		}
		if (config != NULL)
			libusb_free_config_descriptor(config);
	}
#endif
#endif
	if ((itype = inst_usb_match(descriptor.idVendor, descriptor.idProduct))
	                                                                != instUnknown) {
		char pname[400];

		if (p->debug) fprintf(stderr,"usb_check_and_add() found known instrument\n");

		/* Create a path/identification */
		/* (devnum doesn't seem valid ?) */
#ifdef USE_LIBUSB1
		libusb_ref_device(dev);		/* Keep it */
		sprintf(pname,"usb:/bus%d/dev%d/ (%s)",libusb_get_bus_number(dev),libusb_get_device_address(dev), inst_name(itype));
#else
# if defined(UNIX)
		sprintf(pname,"usb:/bus%d/dev%d (%s)",dev->bus->location >> 24, dev->devnum, inst_name(itype));
# else
		sprintf(pname,"usb:/bus%lu/dev%d (%s)",dev->bus->location, dev->devnum, inst_name(itype));
# endif
#endif

		/* Add the path to the list */
		if (p->paths == NULL) {
			if ((p->paths = (icompath **)calloc(sizeof(icompath *), 1 + 1)) == NULL)
				error("icoms: calloc failed!");
		} else {
			if ((p->paths = (icompath **)realloc(p->paths,
			                     sizeof(icompath *) * (p->npaths + 2))) == NULL)
				error("icoms: realloc failed!");
			p->paths[p->npaths+1] = NULL;
		}
		if ((p->paths[p->npaths] = malloc(sizeof(icompath))) == NULL)
			error("icoms: malloc failed!");
		p->paths[p->npaths]->vid = descriptor.idVendor;
		p->paths[p->npaths]->pid = descriptor.idProduct;
		p->paths[p->npaths]->dev = dev;
		p->paths[p->npaths]->hev = NULL;
		p->paths[p->npaths]->itype = itype;
		if ((p->paths[p->npaths]->path = strdup(pname)) == NULL)
			error("icoms: strdup failed!");
		p->npaths++;
		p->paths[p->npaths] = NULL;
		return 1;
	}

	return 0;
}

#endif /* ENABLE_USB */

#ifdef USE_LIBUSB1

/* Add paths to USB connected instruments, to the existing */
/* icompath paths in the icoms structure. */
void usb_get_paths(
struct _icoms *p 
) {
#ifdef ENABLE_USB
	ssize_t i, nlist;
	libusb_context *ctx = NULL;

	struct libusb_device **list;

	/* Scan the USB busses for instruments we recognise */
	/* We're not expecting any of our unstruments to be an interface on a device. */

   	libusb_init(&ctx);

	if (p->debug > 8)
		libusb_set_debug(ctx, p->debug);

	nlist = libusb_get_device_list(NULL, &list);
    
	if (p->debug) fprintf(stderr,"usb_get_paths about to look through devices:\n");

	for (i = 0; i < nlist; i++) {
		usb_check_and_add(p, list[i]);
   	}

	libusb_free_device_list(list,  1);
#endif /* ENABLE_USB */
}

#else /* !USE_LIBUSB1 */

/* Add paths to USB connected instruments, to the existing */
/* icompath paths in the icoms structure. */
void usb_get_paths(
struct _icoms *p 
) {
#ifdef ENABLE_USB
	struct usb_bus *bus;

	/* Check that we've got an up to date version of libusb */
	if (usb_argyll_patched() < 2)
		error("usblib isn't up to date to work with this version of Argyll");

	if (p->debug > 8)
		usb_set_debug(p->debug);

	/* Scan the USB busses for instruments we recognise */
	/* We're not expecting any of our unstruments to be an interface on a device. */

   	usb_init();
   	usb_find_busses();
   	usb_find_devices();
    
	if (p->debug) fprintf(stderr,"usb_get_paths about to look through buses:\n");

   	for (bus = usb_get_busses(); bus != NULL; bus = bus->next) {
		struct usb_device *dev;
		if (p->debug) fprintf(stderr,"usb_get_paths about to look through devices:\n");
   		for (dev = bus->devices; dev != NULL; dev = dev->next) {
			usb_check_and_add(p, dev);
		}
   	}
#endif /* ENABLE_USB */
}
#endif /* !USE_LIBUSB1 */


/* Cleanup and then free a usb dev entry */
void usb_del_usb_device(struct usb_device *dev) {

	if (dev == NULL)
		return;

#ifdef USE_LIBUSB1
	libusb_unref_device(dev);
#endif
}


/* Return the instrument type if the port number is USB, */
/* and instUnknown if it is not. */
instType usb_is_usb_portno(
	icoms *p, 
	int port		/* Enumerated port number, 1..n */
) {
	
	if (p->paths == NULL)
		p->get_paths(p);

	if (port <= 0 || port > p->npaths)
		error("icoms - usb_is_usb_portno: port number %d out of range %d - %d",port,1,p->npaths);

#ifdef ENABLE_USB
	if (p->paths[port-1]->dev != NULL)
		return p->paths[port-1]->itype;
#endif /* ENABLE_USB */

	return instUnknown;
}



/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#ifdef ENABLE_USB

/* Static list so that all open USB/HID connections can be closed on a SIGKILL */
static icoms *icoms_list = NULL;

/* Counter set when we're in a USB read or write */
/* Note - this isn't perfectly thread safe */
static int in_usb_rw = 0;

/* Clean up any open USB ports ready for exit */
static void icoms_cleanup() {
	icoms *pp, *np;
//printf("~1 icoms_cleanipandexit invoked\n");

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
//printf("~1 closing usb port 0x%x\n",pp);
		/* There's a problem here if have more than one USB port */
		/* open - win32 doesn't return from the system call. */
		/* Should we depend on usb read/write routines to call cleanup ? */
		if (pp->is_usb)
			usb_close_port(pp);
		else if (pp->is_hid)
			hid_close_port(pp);
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
//printf("~1 signal handler invoked\n");
	if (in_usb_rw != 0)
		in_usb_rw = -1;
	icoms_cleanup();
#ifdef UNIX
	if (arg == SIGHUP && usbio_hup != SIG_DFL && usbio_hup != SIG_IGN) 
		usbio_hup(arg);
#endif /* UNIX */
	if (arg == SIGINT && usbio_int != SIG_DFL && usbio_int != SIG_IGN) 
		usbio_int(arg);
	if (arg == SIGTERM && usbio_term != SIG_DFL && usbio_term != SIG_IGN) 
		usbio_term(arg);
	exit(0);
}

#ifdef USE_LIBUSB1

/* Callback functions */
static void bulk_transfer_cb(struct libusb_transfer *transfer)
{
	int *completed = transfer->user_data;
	*completed = 1;
	/* caller interprets results and frees transfer */
}

/* Version of libusb1 sync to async code that returns the pointer */
/* to the transfer structure so that the transfer can be cancelled */
/* by another thread. */
/* Note that this isn't perfectly thread safe - there are race windows. */
/* Accessing the libusb_transfer * should at least be protected by a lock. */
/* (For Argyll use this problem is unlikely) */
static int do_sync_bulk_transfer(struct libusb_device_handle *dev_handle,
	void **hcancel, unsigned char endpoint, unsigned char *buffer, int length,
	int *transferred, unsigned int timeout, unsigned char type)
{
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	int completed = 0;
	int r;

	if (!transfer)
		return LIBUSB_ERROR_NO_MEM;

	libusb_fill_bulk_transfer(transfer, dev_handle, endpoint, buffer, length,
		bulk_transfer_cb, &completed, timeout);
	transfer->type = type;

	r = libusb_submit_transfer(transfer);
	if (r < 0) {
		libusb_free_transfer(transfer);
		return r;
	}

	if (hcancel != NULL)
		*hcancel = (void *)transfer;

	while (!completed) {
		r = libusb_handle_events_check(NULL, &completed);
		if (r < 0) {
			if (r == LIBUSB_ERROR_INTERRUPTED)
				continue;
			libusb_cancel_transfer(transfer);
			while (!completed) {
				if (libusb_handle_events_check(NULL, &completed) < 0)
					break;
			}
			if (hcancel != NULL)
				*hcancel = NULL;
			libusb_free_transfer(transfer);
			return r;
		}
	}

	*transferred = transfer->actual_length;
	switch (transfer->status) {
	case LIBUSB_TRANSFER_COMPLETED:
		r = 0;
		break;
	case LIBUSB_TRANSFER_TIMED_OUT:
		r = LIBUSB_ERROR_TIMEOUT;
		break;
	case LIBUSB_TRANSFER_STALL:
		r = LIBUSB_ERROR_PIPE;
		break;
	case LIBUSB_TRANSFER_OVERFLOW:
		r = LIBUSB_ERROR_OVERFLOW;
		break;
	case LIBUSB_TRANSFER_NO_DEVICE:
		r = LIBUSB_ERROR_NO_DEVICE;
		break;
	default:
		r = LIBUSB_ERROR_OTHER;
	}

	if (hcancel != NULL)
		*hcancel = NULL;
	libusb_free_transfer(transfer);

	return r;
}
#endif	/* USE_LIBUSB1 */

// !!! should fix all these to separate error from length transferred !!!
// !!! to take advantage of libus1 !!!

/* Our versions of usblib read/write, that exit if a signal was caught */
/* This is so that MSWindows works properly */
static int icoms_usb_interrupt_write(usb_dev_handle *dev, void **hcancel, int ep, unsigned char *bytes, int size, int timeout) {
#ifdef USE_LIBUSB1
	int tsize;
#endif
	int rv;

	in_usb_rw++;
#ifdef USE_LIBUSB1
	rv = do_sync_bulk_transfer(dev, hcancel, (unsigned char)ep, bytes, size, &tsize, timeout, LIBUSB_TRANSFER_TYPE_INTERRUPT);
	if (rv == LIBUSB_SUCCESS)
		rv = tsize;
#else
	if (hcancel != NULL) *hcancel = (void *)ep;
	rv = usb_interrupt_write(dev, ep, (char *)bytes, size, timeout);
#endif
	if (in_usb_rw < 0)
		exit(0);

	in_usb_rw--;
	return rv;
}

static int icoms_usb_interrupt_read(usb_dev_handle *dev, void **hcancel, int ep, unsigned char *bytes, int size, int timeout) {
#ifdef USE_LIBUSB1
	int tsize;
#endif
	int rv;

	in_usb_rw++;
#ifdef USE_LIBUSB1
	rv = do_sync_bulk_transfer(dev, hcancel, (unsigned char)ep, bytes, size, &tsize, timeout, LIBUSB_TRANSFER_TYPE_INTERRUPT);
	if (rv == LIBUSB_SUCCESS)
		rv = tsize;
#else
	if (hcancel != NULL) *hcancel = (void *)ep;
	rv = usb_interrupt_read(dev, ep, (char *)bytes, size, timeout);
#endif
	if (in_usb_rw < 0)
		exit(0);

	in_usb_rw--;
	return rv;
}

static int icoms_usb_bulk_write(usb_dev_handle *dev, void **hcancel, int ep, unsigned char *bytes, int size, int timeout) {
#ifdef USE_LIBUSB1
	int tsize;
#endif
	int rv;

	in_usb_rw++;
#ifdef USE_LIBUSB1
	rv = do_sync_bulk_transfer(dev, hcancel, (unsigned char)ep, bytes, size, &tsize, timeout, LIBUSB_TRANSFER_TYPE_BULK);
	if (rv == LIBUSB_SUCCESS)
		rv = tsize;
#else
	if (hcancel != NULL) *hcancel = (void *)ep;
	rv = usb_bulk_write(dev, ep, (char *)bytes, size, timeout);
#endif
	if (in_usb_rw < 0)
		exit(0);

	in_usb_rw--;
	return rv;
}

static int icoms_usb_bulk_read(usb_dev_handle *dev, void **hcancel, int ep, unsigned char *bytes, int size, int timeout) {
#ifdef USE_LIBUSB1
	int tsize;
#endif
	int rv;

	in_usb_rw++;
#ifdef USE_LIBUSB1
	rv = do_sync_bulk_transfer(dev, hcancel, (unsigned char)ep, bytes, size, &tsize, timeout, LIBUSB_TRANSFER_TYPE_BULK);
	if (rv == LIBUSB_SUCCESS)
		rv = tsize;
#else
	if (hcancel != NULL) *hcancel = (void *)ep;
	rv = usb_bulk_read(dev, ep, (char *)bytes, size, timeout);
#endif
	if (in_usb_rw < 0)
		exit(0);

	in_usb_rw--;
	return rv;
}


static int icoms_usb_control_msg(
usb_dev_handle *dev, int requesttype, int request,
int value, int index, unsigned char *bytes, int size, 
int timeout) {
	int rv;

	in_usb_rw++;
#ifdef USE_LIBUSB1
	rv = libusb_control_transfer(dev, (uint8_t)requesttype, (uint8_t)request, (uint16_t)value, (uint16_t)index, bytes, (uint16_t)size, timeout);
#else
	rv = usb_control_msg(dev, requesttype, request, value, index, (char *)bytes, size, timeout);
#endif
	if (in_usb_rw < 0)
		exit(0);

	in_usb_rw--;
	return rv;
}

#endif /* ENABLE_USB */

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Install the cleanup signal handlers */
void usb_install_signal_handlers(icoms *p) {
	if (icoms_list == NULL) {
//printf("~1 installing signal handler\n");
#if defined(UNIX)
		usbio_hup = signal(SIGHUP, icoms_sighandler);
#endif /* UNIX */
		usbio_int = signal(SIGINT, icoms_sighandler);
		usbio_term = signal(SIGTERM, icoms_sighandler);
	}

	/* Add it to our static list, to allow automatic cleanup on signal */
	p->next = icoms_list;
	icoms_list = p;
}

/* Delete an icoms from our static signal cleanup list */
void usb_delete_from_cleanup_list(icoms *p) {

	/* Find it and delete it from our static cleanup list */
	if (icoms_list != NULL) {
		if (icoms_list == p) {
			icoms_list = p->next;
			if (icoms_list == NULL) {
//printf("~1 removing signal handler\n");
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

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Close an open USB port */
/* If we don't do this, the port and/or the device may be left in an unusable state. */
void usb_close_port(icoms *p) {

#ifdef ENABLE_USB
	if (p->debug) fprintf(stderr,"usb_close_port() called\n");

	if (p->is_open && p->usbh != NULL) {
		int iface;

		for (iface = 0; iface < p->nifce; iface++)
#ifdef USE_LIBUSB1
			libusb_release_interface(p->usbh, iface);
#else
			usb_release_interface(p->usbh, iface);
#endif

		/* Workaround for some bugs */
		if (p->uflags & icomuf_reset_before_close) {
#ifdef USE_LIBUSB1
			libusb_reset_device(p->usbh);
#else
			usb_reset(p->usbh);
#endif
		}

		/* Close as well, othewise we can't re-open */
		{
#ifdef USE_LIBUSB1
			libusb_close(p->usbh);
#else
			usb_close(p->usbh);
#endif
		}
		p->usbh = NULL;

		if (p->debug) fprintf(stderr,"usb port has been released and closed\n");
	}
	p->is_open = 0;
	if (p->ppath != NULL) {
		if (p->ppath->path != NULL)
			free(p->ppath->path);
		free(p->ppath);
		p->ppath = NULL;
	}

	/* Find it and delete it from our static cleanup list */
	usb_delete_from_cleanup_list(p);

#endif /* ENABLE_USB */
}

/* Open a USB port for all our uses. */
static void usb_open_port(
icoms *p,
int    port,		/* USB com port, 1 - N, 0 for no change. */
int    config,		/* Configuration number */
int    wr_ep,		/* Write end point */
int    rd_ep,		/* Read end point */
icomuflags usbflags,/* Any special handling flags */
int retries,		/* > 0 if we should retry set_configuration (100msec) */ 
char **pnames		/* List of process names to try and kill before opening */
) {
	int tries = 0;
	if (p->debug) fprintf(stderr,"icoms: About to open the USB port, tries %d\n",retries);

	if (port >= 1) {
		if (p->is_open && port != p->port) {	/* If port number changes */
			p->close_port(p);
		}
	}

	/* Make sure the port is open */
	if (!p->is_open) {
		struct usb_device_descriptor descriptor;
#ifdef USE_LIBUSB1
		const struct libusb_interface_descriptor *ifd;
	    struct libusb_config_descriptor *confdesc;
#else
		struct usb_interface_descriptor *ifd;
#endif
		int rv, i, iface;
		kkill_nproc_ctx *kpc = NULL;

		if (p->debug) fprintf(stderr,"icoms: USB port needs opening\n");

		/* Do open retries */
		for (tries = 0; retries >= 0; retries--, tries++) {

			if (p->ppath != NULL) {
				if (p->ppath->path != NULL)
					free(p->ppath->path);
				free(p->ppath);
			}
	
			if (p->paths == NULL || tries > 0)
				p->get_paths(p);
	
			if (port <= 0 || port > p->npaths)
				error("icoms - usb_open_port: port number out of range!");
	
			if (p->paths[port-1]->dev == NULL)
				error("icoms - usb_open_port: Not a USB port!");
	
			if ((p->ppath = malloc(sizeof(icompath))) == NULL)
				error("malloc() failed on com port path");
			*p->ppath = *p->paths[port-1];				/* Structure copy */
			if ((p->ppath->path = strdup(p->paths[port-1]->path)) == NULL)
				error("strdup() failed on com port path");
			p->port = port;
	
			if (p->debug) fprintf(stderr,"icoms: About to open USB port '%s'\n",p->ppath->path);

			if (tries > 0) {
				//msec_sleep(i_rand(50,100));
				msec_sleep(77);
			}
			if (tries > 0 && pnames != NULL && kpc == NULL) {
#if defined(__APPLE__) 
				if ((kpc = kkill_nprocess(pnames, p->debug)) == NULL) {
					if (p->debug) fprintf(stderr,"kkill_nprocess returned error!\n");
				}
#endif /* !__APPLE__ */
			}

#ifdef USE_LIBUSB1
			if ((rv = libusb_open(p->ppath->dev, &p->usbh)) != LIBUSB_SUCCESS)
#else
			if ((p->usbh = usb_open(p->ppath->dev)) == NULL)
#endif
			{
				if (p->debug)
					fprintf(stderr, "Opening USB port '%s' config %d failed (%s) (Permissions ?)",p->ppath->path,config,USB_STRERROR(rv));
				if (retries <= 0) {
					if (kpc != NULL)
						kpc->del(kpc); 
					error("Opening USB port '%s' config %d failed (%s) (Permissions ?)",p->ppath->path,config,USB_STRERROR(rv));
				}
				continue;
			} else if (p->debug)
				fprintf(stderr,"open() of USB port '%s' suceeded\n",p->ppath->path);

			/* Get a copy of the device descriptor so we can see device params */
#ifdef USE_LIBUSB1
			if (libusb_get_device_descriptor(p->ppath->dev, &descriptor) != LIBUSB_SUCCESS)
				error("Get device descriptor on USB port '%s' failed with %d (%s)",p->ppath->path,rv,libusb_strerror(rv));
#else
			descriptor = dev->descriptor;	/* Copy */
#endif

			p->vid = p->ppath->vid;
			p->pid = p->ppath->pid;
			p->usbd = p->ppath->dev;

			p->cnfg = config;
			p->uflags = usbflags;

#ifdef USE_LIBUSB1
			if (p->uflags & icomuf_detach) {
				if (libusb_kernel_driver_active(p->usbh, 0) == 1) {
					if (p->debug) fprintf(stderr,"icoms: detaching '%s'\n",p->ppath->path);
					if (libusb_detach_kernel_driver(p->usbh, 0) != LIBUSB_SUCCESS) {
						if (kpc != NULL)
							kpc->del(kpc); 
						error("Detach kernel driver on USB port '%s' failed with %d (%s)",p->ppath->path,rv,libusb_strerror(rv));
					}
				}
			}
#else
#if LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP == 1
			if (p->uflags & icomuf_detach) {
				usb_detach_kernel_driver_np(p->usbh, 0);
			}
#endif
#endif

			if (p->debug > 1) fprintf(stderr, "Number of configurations = %d\n",
			                          descriptor.bNumConfigurations);
#if defined(UNIX) && !defined(__APPLE__)
			/* only call set_configuration on Linux if the device has more than one */
			/* possible configuration, because Linux does a set_configuration by default, */
			/* and two of them mess up instruments like the Spyder2 */

			if (descriptor.bNumConfigurations > 1) {
#endif

			/* Can't skip this, as it is needed to setup the interface and end points on OS X */
#ifdef USE_LIBUSB1
			if ((rv = libusb_set_configuration(p->usbh, p->cnfg)) < 0)
#else
			if ((rv = usb_set_configuration(p->usbh, p->cnfg)) < 0)
#endif
			{
				if (p->debug)
					fprintf(stderr,"Configuring USB port '%s' to %d failed with %d (%s)",p->ppath->path,config,rv,USB_STRERROR(rv));
				if (retries <= 0) {
					if (kpc != NULL)
						kpc->del(kpc); 
					error("Configuring USB port '%s' to %d failed with %d (%s)",p->ppath->path,config,rv,USB_STRERROR(rv));
				}
#ifdef USE_LIBUSB1
				libusb_reset_device(p->usbh); // ~~999 ?????
				libusb_close(p->usbh);
#else
				usb_reset(p->usbh);
				usb_close(p->usbh);
#endif
				continue;
			}
#if defined(UNIX) && !defined(__APPLE__)
			}		/* End of if bNumConfigurations > 1 */
#endif

			/* We're done */
			break;
		}

		if (kpc != NULL)
			kpc->del(kpc); 

		/* Claim all interfaces of this configuration */
#ifdef USE_LIBUSB1
		if ((rv = libusb_get_active_config_descriptor(p->usbd, &confdesc)) != LIBUSB_SUCCESS) {
			error("Getting config descriptor for USB port '%s' failed with %d (%s)",p->ppath->path,rv,libusb_strerror(rv));
		}
		p->nifce = confdesc->bNumInterfaces;
#else
		p->nifce = p->usbd->config->bNumInterfaces;
#endif

//printf("~1 Number of interfaces = %d\n",p->nifce);

		/* Claim all the interfaces */
		for (iface = 0; iface < p->nifce; iface++) {
			/* (Second parameter is bInterfaceNumber) */

#ifdef USE_LIBUSB1
			/* Should we do a libusb_kernel_driver_active() first ? */
			if ((rv = libusb_claim_interface(p->usbh, iface)) < 0) {
				/* Detatch the existing interface. */
				if (p->uflags & icomuf_detach) {
					libusb_detach_kernel_driver (p->usbh, iface);
					if ((rv = libusb_claim_interface(p->usbh, iface)) < 0) {
						error("Claiming USB port '%s' interface %d failed with %d",p->ppath->path,iface,rv);
					}
				}
			}
#else
			if ((rv = usb_claim_interface(p->usbh, iface)) < 0) {
#if LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP == 1
				/* Detatch the existing interface. */
				if (p->uflags & icomuf_detach) {
					usb_detach_kernel_driver_np(p->usbh, iface);
					if ((rv = usb_claim_interface(p->usbh, iface)) < 0) {
						error("Claiming USB port '%s' interface %d failed with %d",p->ppath->path,iface,rv);
					}
				}
#else
				error("Claiming USB port '%s' interface %d failed with %d",p->ppath->path,iface,rv);
#endif
			}
#endif
	
			/* Fill in the end point details */
#ifdef USE_LIBUSB1
			ifd = &confdesc->interface[iface].altsetting[0];
#else
			ifd = &p->usbd->config[p->cnfg-1].interface[iface].altsetting[0];
#endif
//printf("~1 Number of endpoints on iface %d = %d\n",iface, ifd->bNumEndpoints);
			for (i = 0; i < ifd->bNumEndpoints; i++) {
				int ad = ifd->endpoint[i].bEndpointAddress;
				p->EPINFO(ad).valid = 1;
				p->EPINFO(ad).addr = ad;
				p->EPINFO(ad).packetsize = ifd->endpoint[i].wMaxPacketSize;
				p->EPINFO(ad).type = ifd->endpoint[i].bmAttributes & USB_ENDPOINT_TYPE_MASK;
				/* Some I/F seem to hang if we do this, some seem to hang if we don't ! */
				if (!(p->uflags & icomuf_no_open_clear))
#ifdef USE_LIBUSB1
					libusb_clear_halt(p->usbh, (unsigned char)ad);
#else
					usb_clear_halt(p->usbh, ad);
#endif
//printf("~1 %d: endpoint addr %02x pktsze %d, type %d\n",i,ad,ifd->endpoint[i].wMaxPacketSize,p->EPINFO(ad).type);
			}

#ifdef NEVER
			/* Get the serial number */
			{
				int rv;
				struct usb_device_descriptor descriptor;

				if (p->debug) fprintf(stderr,"About to get device serial number\n");
#ifdef USE_LIBUSB1
				if ((rv = libusb_get_device_descriptor(p->usbd, &descriptor)) != LIBUSB_SUCCESS)
					error("get_device_descriptor failed with %d USB port '%s'",rv,p->ppath->path);
#else
				descriptor = dev->descriptor;	/* Copy */
#endif
				if ((rv = libusb_get_string_descriptor_ascii(p->usbh, descriptor.iSerialNumber, p->serialno, 32)) <= 0) {
					if (p->debug) fprintf(stderr,"Failed to get device serial number %d\n",rv);
					p->serialno[0] = '\000';
				}
				if (p->debug) fprintf(stderr,"Device serial number = '%s'\n",p->serialno);
			}
#endif /* NEVER */
		}

		/* Set "serial" coms values */
		p->wr_ep = wr_ep;
		p->rd_ep = rd_ep;
		p->rd_qa = p->EPINFO(rd_ep).packetsize;
//printf("~1 read quanta = packet size = %d\n",p->rd_qa);
		if (p->rd_qa == 0)
			p->rd_qa = 8;

		p->is_usb = 1;
		p->is_open = 1;
		if (p->debug) fprintf(stderr,"icoms: USB port is now open\n");
	}

	if (p->debug) fprintf(stderr,"icoms: Clearing any USB errors\n");

	/* Install the cleanup signal handlers, and add to our cleanup list */
	usb_install_signal_handlers(p);
}

/* ========================================================= */
/* USB write/read "serial" imitation */

/* Write the characters in the buffer out */
/* Data will be written up to the terminating nul */
/* Return relevant error status bits */
static int
icoms_usb_ser_write(
icoms *p,
char *wbuf,
double tout)
{
	int c, len, wbytes;
	long toc, i, top;		/* Timout count, counter, timeout period */
	int ep = p->wr_ep;		/* End point */
	int bulk = 0;			/* nz if bulk rather than interrupt read */

	if (p->debug) fprintf(stderr,"icoms: About to write '%s' ",icoms_fix(wbuf));

	if (!p->is_open)
		error("icoms_ser_write: not initialised");

	if (p->EPINFO(ep).valid == 0)
		error("icoms_ser_write invalid end point 0x%02x",ep);

	if (p->EPINFO(ep).type != ICOM_EP_TYPE_BULK
	 && p->EPINFO(ep).type != ICOM_EP_TYPE_INTERRUPT)
		error("icoms_ser_write unhandled end point type %d",p->EPINFO(ep).type);

	if (p->EPINFO(ep).type == ICOM_EP_TYPE_BULK)
		bulk = 1;

	len = strlen(wbuf);
	tout *= 1000.0;		/* Timout in msec */
	p->lerr = 0;

	top = (int)(tout + 0.5);		/* Timeout period in msecs */
	toc = (int)(tout/top + 0.5);	/* Number of timout periods in timeout */
	if (toc < 1)
		toc = 1;

#ifdef ENABLE_USB

	/* Until data is all written, we time out, or the user aborts */
	for (i = toc; i > 0 && len > 0;) {
//printf("~1 write: attempting to write %d bytes to usb top = %d, i = %d\n",len,top,i);

		if (bulk)
			wbytes = icoms_usb_bulk_write(p->usbh, NULL, ep, (unsigned char *)wbuf, len, top);
		else
			wbytes = icoms_usb_interrupt_write(p->usbh, NULL, ep, (unsigned char *)wbuf, len, top);
		if (wbytes < 0) {
//printf("~1 usb_interrupt_write failed with %d = '%s'\n",wbytes,usb_strerror());
#ifdef USE_LIBUSB1
			if (wbytes != LIBUSB_ERROR_TIMEOUT)	/* Not a timeout */
#else
#if defined(UNIX)
			if (wbytes != 0 && wbytes != -ETIMEDOUT)      /* Not a timeout } */
#else
			if (wbytes != -116)		/* Not a timeout */
#endif /* UNIX */
#endif
			{
				p->lerr |= ICOM_USBW; 
				break;
			}
			i--;		/* timeout */
		} else if (wbytes > 0) {	/* Account for bytes written */
//printf("~1 wrote %d bytes\n",wbytes);
			i = toc;
			wbuf += wbytes;
			len -= wbytes;
		}
		/* Check for user interrupt */
		if ((c = poll_con_char()) != 0 && p->uih[c] != ICOM_OK) {
			p->cut = c;
			p->lerr |= p->uih[c];
		}
	}
	if (i <= 0)		/* Must have timed out */
		p->lerr |= ICOM_TO; 

#else /* !ENABLE_USB */
	p->lerr = ICOM_NOTS;
#endif /* !ENABLE_USB */

	if (p->debug) fprintf(stderr,"ICOM err 0x%x\n",p->lerr);
	return p->lerr;
}


/* Read characters into the buffer */
/* Return string will be terminated with a nul */
/* Read only in paket sized chunks, and retry if */
/* the bytes requested aren'r read, untill we get a */
/* timeout or a terminating char is read */
static int
icoms_usb_ser_read(icoms *p,
char *rbuf,			/* Buffer to store characters read */
int bsize,			/* Buffer size */
char tc,			/* Terminating characer */
int ntc,			/* Number of terminating characters */
double tout)		/* Time out in seconds */
{
	int c, j, rbytes;
	long toc, i, top;		/* Timout count, counter, timeout period */
	char *rrbuf = rbuf;		/* Start of return buffer */
	int ep = p->rd_ep;		/* End point */
	int bulk = 0;			/* nz if bulk rather than interrupt read */

	if (p->debug) fprintf(stderr,"icoms: Read called\n");

#ifdef QUIET_MEMCHECKERS
	memset(rbuf, 0, bsize);
#endif

	if (!p->is_open)
		error("icoms_usb_ser_read: not initialised");

	if (p->EPINFO(ep).valid == 0)
		error("icoms_usb_ser_read invalid end point 0x%02x",ep);

	if (p->EPINFO(ep).type != ICOM_EP_TYPE_BULK
	 && p->EPINFO(ep).type != ICOM_EP_TYPE_INTERRUPT)
		error("icoms_usb_ser_read unhandled end point type %d",p->EPINFO(ep).type);

	if (p->EPINFO(ep).type == ICOM_EP_TYPE_BULK)
		bulk = 1;

	if (bsize < 3)
		error("icoms_usb_ser_read given too small a buffer");

for (i = 0; i < bsize; i++) rbuf[i] = 0;
 
	p->lerr = 0;
	tout *= 1000.0;		/* Timout in msec */
	bsize--;	/* Allow space for null */

	/* Have to do this in one go, because libusb has no way */
	/* of timing out and returning the number of characters read */
	/* up to the timeout, and it looses characters. */
	top = (int)(tout + 0.5);		/* Timeout period in msecs */
	toc = (int)(tout/top + 0.5);	/* Number of timout periods in timeout */
	if (toc < 1)
		toc = 1;

#ifdef ENABLE_USB
//printf("~1 usb read end point 0x%x, read quanta %d\n",p->rd_ep,p->rd_qa);
	/* Until data is all read, we time out, or the user aborts */
	for (i = toc, j = 0; i > 0 && bsize > 1 && j < ntc ;) {
		int rsize = p->rd_qa < bsize ? p->rd_qa : bsize; 

//printf("~1 read: attempting to read %d bytes from usb, top = %d, i = %d, j = %d\n",bsize > p->rd_qa ? p->rd_qa : bsize,top,i,j);
		/* We read one read quanta at a time (usually 8 bytes), to avoid */
		/* problems with libusb loosing characters whenever it times out. */
		if (bulk)
			rbytes = icoms_usb_bulk_read(p->usbh, NULL, ep, (unsigned char *)rbuf, rsize, top);
		else
			rbytes = icoms_usb_interrupt_read(p->usbh, NULL, ep, (unsigned char *)rbuf, rsize, top);
		if (rbytes < 0) {
//printf("~1 usb_interrupt_read failed with %d = '%s'\n",rbytes,usb_strerror());
//printf("~1 rbuf = '%s'\n",icoms_fix(rrbuf));
#ifdef USE_LIBUSB1
			if (rbytes != LIBUSB_ERROR_TIMEOUT)	/* Not a timeout */
#else
#if defined(UNIX)
			if (rbytes != 0 && rbytes != -ETIMEDOUT)     /* Not a timeout } */
#else
			if (rbytes != -116)		/* Not a timeout */
#endif /* UNIX */
#endif
			{
				p->lerr |= ICOM_USBR; 
				break;
			}
			i--;	/* Timeout */
		} else if (rbytes >= 0) {	/* Account for bytes read */
//printf("~1 rbuf = '%s'\n",icoms_fix(rrbuf));
//printf("~1 read %d bytes,",rbytes);
			i = toc;
			bsize -= rbytes;
			while(rbytes) {	/* Count termination characters */
				if (*rbuf++ == tc)
					j++;
				rbytes--;
			}
//printf(" j = %d\n",j);
		}
		/* Check for user abort */
		if ((c = poll_con_char()) != 0 && p->uih[c] != ICOM_OK) {
			p->cut = c;
			p->lerr |= p->uih[c];
		}
	}

	if (i <= 0)		/* Must have timed out */
		p->lerr |= ICOM_TO; 

#endif /* ENABLE_USB */

	*rbuf = '\000';

	if (p->debug) fprintf(stderr,"icoms: About to return read '%s' ICOM err 0x%x\n",icoms_fix(rrbuf),p->lerr);

	return p->lerr;
}

/* - - - - - - - - - - - - - */
/* USB control message - thread friendly */
static int
icoms_usb_control_th(
icoms *p,
int requesttype,		/* 8 bit request type (USB bmRequestType) */
int request,			/* 8 bit request code (USB bRequest) */
int value,				/* 16 bit value (USB wValue) */
int index,				/* 16 bit index (USB wIndex) */
unsigned char *rwbuf,	/* Write or read buffer */
int rwsize, 			/* Bytes to read or write */
double tout,			/* Timeout in seconds */
int debug,				/* debug flag value */
int *cut,				/* Character that caused termination */
int checkabort) {		/* Check for abort from keyboard */
	int lerr;			/* Last error */
	int c, rwbytes;		/* Data bytes read or written */
	long top;			/* timeout in msec */

	if (debug) {
		fprintf(stderr,"icoms: About to do control  %02x, %02x %04x %04x %04x\n",
	                      requesttype, request, value, index, rwsize);
		if ((requesttype & USB_ENDPOINT_IN) == 0) {
			fprintf(stderr,"icoms: Writing control data %s\n",icoms_tohex(rwbuf, rwsize));
		}
	}

	if (!p->is_open)
		error("icoms_read: not initialised");

	lerr = 0;
	top = (int)(tout * 1000.0 + 0.5);		/* Timout in msec */

#ifdef QUIET_MEMCHECKERS
	if (requesttype & USB_ENDPOINT_IN)
		memset(rwbuf, 0, rwsize);
#endif

#ifdef ENABLE_USB
	rwbytes = icoms_usb_control_msg(p->usbh, requesttype, request, value, index,
	                                    rwbuf, rwsize, top);
	if (rwbytes < 0) {
#ifdef USE_LIBUSB1
		if (rwbytes != LIBUSB_ERROR_TIMEOUT)	/* Not a timeout */
#else
#if defined(UNIX)
		if (rwbytes != 0 && rwbytes != -ETIMEDOUT)     /* Not a timeout */
#else
		if (rwbytes != -116)		/* Not a timeout */
#endif /* UNIX */
#endif
		{
			lerr |= ICOM_USBW; 
		} else {
			lerr |= ICOM_TO; 
		}
	} else if (rwbytes != rwsize) {
		if (requesttype & USB_ENDPOINT_IN)	/* Device to host */
			lerr |= ICOM_USBR;				/* Read error */ 
		else
			lerr |= ICOM_USBW; 				/* Write error */
	}

	if (checkabort) {
		/* Check for user abort */
		if ((c = poll_con_char()) != 0 && p->uih[c] != ICOM_OK) {
			*cut = c;
			lerr |= p->uih[c];
//printf("~1 got for user abort with char 0x%x, code 0x%x\n",c,p->uih[c]);
		}
	}

#else /* !ENABLE_USB */
	lerr = ICOM_NOTS;
#endif /* !ENABLE_USB */

	if (debug && (requesttype & USB_ENDPOINT_IN))
		fprintf(stderr,"icoms: Reading control data %s\n",icoms_tohex(rwbuf, rwsize));
	if (debug) fprintf(stderr,"icoms: About to return control ICOM err 0x%x\n",lerr);

	return lerr;
}

/* USB control message */
/* Same as above, but set error state */
static int
icoms_usb_control(
icoms *p,
int requesttype,		/* 8 bit request type (USB bmRequestType) */
int request,			/* 8 bit request code (USB bRequest) */
int value,				/* 16 bit value (USB wValue) */
int index,				/* 16 bit index (USB wIndex) */
unsigned char *rwbuf,	/* Write or read buffer */
int rwsize, 			/* Bytes to read or write */
double tout) {
	p->lerr = icoms_usb_control_th(p, requesttype, request, value, index, rwbuf, rwsize, tout,
		                                                                 p->debug, &p->cut, 1);
	return p->lerr;
}

/* - - - - - - - - - - - - - */
/* USB Read  - thread friendly */
/* Return error code (don't set error state). */
/* Don't retry on a short read, return ICOM_SHORT. */
static int
icoms_usb_read_th(icoms *p,
	void **hcancel,			/* Cancel handle */
	int ep,					/* End point address */
	unsigned char *rbuf,	/* Read buffer */
	int bsize,				/* Bytes to read */
	int *breadp,			/* Bytes read */
	double tout,			/* Timeout in seconds */
	int debug,				/* debug flag value */
	int *cut,				/* Character that caused termination */
	int checkabort			/* Check for abort from keyboard */
) {
	int lerr;				/* Last error */
	int bread, qa;
	long top;			/* Timeout period */
	int bulk = 0;			/* nz if bulk rather than interrupt read */

#ifdef QUIET_MEMCHECKERS
	memset(rbuf, 0, bsize);
#endif

	if (!p->is_open)
		error("icoms_usb_read: not initialised");

	if (p->EPINFO(ep).valid == 0)
		error("icoms_usb_read invalid end point 0x%02x",ep);

	if (p->EPINFO(ep).type != ICOM_EP_TYPE_BULK
	 && p->EPINFO(ep).type != ICOM_EP_TYPE_INTERRUPT)
		error("icoms_usb_read unhandled end point type %d",p->EPINFO(ep).type);

	if (p->EPINFO(ep).type == ICOM_EP_TYPE_BULK)
		bulk = 1;

//	qa = p->EPINFO(ep).packetsize;	/* For finer grained abort and partial reads */
	qa = bsize;						/* For simpler tracing */

	lerr = 0;
	bread = 0;

	top = (int)(tout * 1000 + 0.5);		/* Timeout period in msecs */

#ifdef ENABLE_USB

	/* Bug workaround - on some OS's for some devices */
	if (p->uflags & icomuf_resetep_before_read) {
		msec_sleep(1);		/* Let device recover ? */
		p->usb_resetep(p, ep);
		msec_sleep(1);		/* Let device recover (ie. Spyder 3) */
	}

	/* Until data is all read, we get a short read, we time out, or the user aborts */
//printf("~1 usb_read of %d bytes, timout %f\n",bsize,tout);
	while (bsize > 0) {
		int c, rbytes;
		int rsize = bsize > qa ? qa : bsize; 

//printf("~1 read %d bytes this time\n",rsize);
		if (bulk)
			rbytes = icoms_usb_bulk_read(p->usbh, hcancel, ep, rbuf, rsize, top);
		else
			rbytes = icoms_usb_interrupt_read(p->usbh, hcancel, ep, rbuf, rsize, top);
//printf("~1 got result %d\n",rbytes);
		if (rbytes < 0) {
#ifdef USE_LIBUSB1
			if (rbytes == LIBUSB_ERROR_TIMEOUT) /* A timeout */
#else
#if defined(UNIX)
			if (rbytes == -ETIMEDOUT)	/* A timeout */
#else
			if (rbytes == -116)			/* A timeout */
#endif /* UNIX */
#endif
				lerr |= ICOM_TO; 
			else
				lerr |= ICOM_USBR; 
			break;
		} else {	/* Account for bytes read */
			bsize -= rbytes;
			rbuf += rbytes;
			bread += rbytes;
		}
		if (rbytes != rsize) {
			lerr |= ICOM_SHORT; 
			break;
		}
		if (checkabort) {
			/* Check for user abort */
			if ((c = poll_con_char()) != 0 && p->uih[c] != ICOM_OK) {
//printf("~1 got for user abort with char 0x%x, code 0x%x\n",c,p->uih[c]);
				*cut = c;
				lerr |= p->uih[c];
			}
		}
	}

	if (breadp != NULL)
		*breadp = bread;

//printf("~1 about to return 0x%x\n",lerr);

#else /* !ENABLE_USB */
	lerr = ICOM_NOTS;
#endif /* !ENABLE_USB */

	if (debug) fprintf(stderr,"icoms: About to return usb read %d bytes, ICOM err 0x%x\n",bread, lerr);

	return lerr;
}

/* USB Read */
/* Same as above, but set error state */
static int
icoms_usb_read(
	icoms *p,
	int ep,					/* End point address */
	unsigned char *rbuf,	/* Read buffer */
	int rsize,				/* Bytes to read */
	int *bread,				/* Bytes read */
	double tout				/* Timeout in seconds */
) {
	p->lerr = icoms_usb_read_th(p, NULL, ep, rbuf, rsize, bread, tout, p->debug, &p->cut, 1);

	return p->lerr;
}

/* - - - - - - - - - - - - - */

/* USB Write  - thread friendly */
/* Return error code (don't set error state). */
/* Don't retry on a short read, return ICOM_SHORT. */
static int
icoms_usb_write_th(icoms *p,
	void **hcancel,			/* Cancel handle */
	int ep,					/* End point address */
	unsigned char *wbuf,	/* Write buffer */
	int bsize,				/* Bytes to write */
	int *bwrittenp,			/* Bytes written */
	double tout,			/* Timeout in seconds */
	int debug,				/* debug flag value */
	int *cut,				/* Character that caused termination */
	int checkabort			/* Check for abort from keyboard */
) {
	int lerr;				/* Last error */
	int bwritten, qa;
	long top;				/* Timout timeout period */
	int bulk = 0;			/* nz if bulk rather than interrupt read */

	if (!p->is_open)
		error("icoms_usb_write: not initialised");

	if (p->EPINFO(ep).valid == 0)
		error("icoms_usb_write invalid end point 0x%02x",ep);

	if (p->EPINFO(ep).type != ICOM_EP_TYPE_BULK
	 && p->EPINFO(ep).type != ICOM_EP_TYPE_INTERRUPT)
		error("icoms_usb_write unhandled end point type %d",p->EPINFO(ep).type);

	if (p->EPINFO(ep).type == ICOM_EP_TYPE_BULK)
		bulk = 1;

//	qa = p->EPINFO(ep).packetsize;	/* For finer grained abort and partial reads */
	qa = bsize;						/* For simpler tracing */

	lerr = 0;
	bwritten = 0;

	top = (int)(tout * 1000 + 0.5);		/* Timeout period in msecs */

#ifdef ENABLE_USB

	/* Until data is all written, we get a short write, we time out, or the user aborts */
	while (bsize > 0) {
		int c, wbytes;
		int wsize = bsize > qa ? qa : bsize; 

		if (bulk)
			wbytes = icoms_usb_bulk_write(p->usbh, hcancel, ep, wbuf, wsize, top);
		else
			wbytes = icoms_usb_interrupt_write(p->usbh, hcancel, ep, wbuf, wsize, top);
		if (wbytes < 0) {
#ifdef USE_LIBUSB1
			if (wbytes == LIBUSB_ERROR_TIMEOUT)	/* A timeout */
#else
#if defined(UNIX)
			if (wbytes == -ETIMEDOUT)	/* A timeout */
#else
			if (wbytes == -116)			/* A timeout */
#endif /* UNIX */
#endif
				lerr |= ICOM_TO; 
			else
				lerr |= ICOM_USBR; 
			break;
		} else {	/* Account for bytes written */
			bsize -= wbytes;
			wbuf += wbytes;
			bwritten += wbytes;
		}
		if (wbytes != wsize) {
			lerr |= ICOM_SHORT; 
			break;
		}
		if (checkabort) {
			/* Check for user abort */
			if ((c = poll_con_char()) != 0 && p->uih[c] != ICOM_OK) {
				*cut = c;
				lerr |= p->uih[c];
			}
		}
	}

	if (bwrittenp != NULL)
		*bwrittenp = bwritten;

#else /* !ENABLE_USB */
	lerr = ICOM_NOTS;
#endif /* !ENABLE_USB */

	if (debug) fprintf(stderr,"icoms: About to return usb write %d bytes, ICOM err 0x%x\n",bwritten, lerr);

	return lerr;
}

/* USB Write */
/* Same as above, but set error state */
static int
icoms_usb_write(
	icoms *p,
	int ep,					/* End point address */
	unsigned char *wbuf,	/* Read buffer */
	int wsize,				/* Bytes to write */
	int *bwritten,			/* Bytes written */
	double tout				/* Timeout in seconds */
) {
	p->lerr = icoms_usb_write_th(p, NULL, ep, wbuf, wsize, bwritten, tout, p->debug, &p->cut, 1);
	return p->lerr;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Cancel i/o in another thread */
/* NOTE this should reall be protected by a lock. */
int icoms_usb_cancel_io(
	icoms *p,
	void *hcancel
) {
	int rv = 0;
#ifdef USE_LIBUSB1
	if (hcancel != NULL)
		rv = libusb_cancel_transfer((struct libusb_transfer *)hcancel);
#else
	msec_sleep(1);		/* Let device recover ? */
	rv = usb_resetep(p->usbh, (int)hcancel);	/* Not reliable ? */
	msec_sleep(1);		/* Let device recover ? */
#endif

	if (rv == 0)
		return ICOM_OK;

	return ICOM_USBW;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Reset and end point data toggle to 0 */
int icoms_usb_resetep(
	icoms *p,
	int ep					/* End point address */
) {
	int rv;
#ifdef USE_LIBUSB1
	rv = libusb_resetep(p->usbh, (unsigned char)ep);		/* Is this the same though ? */
#else
	rv = usb_resetep(p->usbh, ep);			/* Not reliable ? */
#endif

	if (rv == 0)
		return ICOM_OK;

	return ICOM_USBW;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Clear a halt on an end point */
int icoms_usb_clearhalt(
	icoms *p,
	int ep					/* End point address */
) {
	int rv;
#ifdef USE_LIBUSB1
	rv = libusb_clear_halt(p->usbh, (unsigned char)ep);
#else
	rv = usb_clear_halt(p->usbh, ep);
#endif

	if (rv == 0)
		return ICOM_OK;

	return ICOM_USBW;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* write and read */
static int
icoms_write_read(
icoms *p,
char *wbuf,			/* Write puffer */
char *rbuf,			/* Read buffer */
int bsize,			/* Buffer size */
char tc,			/* Terminating characer */
int ntc,			/* Number of terminating characters */
double tout
) {
	int xuserm = 0;			/* User flags from transmit operation */
	if (p->debug) fprintf(stderr,"\nicoms: Write_Read called with '%s'\n",icoms_fix(wbuf));

	p->lerr = 0;

	/* Flush any stray chars */
	/* (This doesn't work on USB because of libusb's problems with timeouts */
	if (!p->is_usb) {
		for (p->lerr = 0;;) {	/* Until we get a timeout */
			int debug = p->debug; p->debug = 0;
			p->read(p, rbuf, bsize, '\000', 100000, 0.01);
			p->debug = debug;
			if (p->lerr != 0)
				break;				/* Expect timeout with nothing to read */
		}
		if (p->lerr & ICOM_USERM) {
			return p->lerr;			/* We got a user interrupt */
		}
		p->lerr = 0;
	}

	/* Write the write data */
	p->write(p, wbuf, tout);

	/* Return error if coms error or user abort. Save error if user terminate or command */
	if ((p->lerr & ~ICOM_USERM) != ICOM_OK || (p->lerr & ICOM_USERM) == ICOM_USER) {
		if (p->debug) fprintf(stderr,"icoms: Write_Read Write failed - returning 0x%x\n",p->lerr);
		return p->lerr;
	} else {
		xuserm = (p->lerr & ICOM_USERM);
	}

	/* Read response */
	p->read(p, rbuf, bsize, tc, ntc, tout);
	if (p->debug) {
		if (p->lerr != 0)
			fprintf(stderr,"icoms: Write_Read Write failed - returning 0x%x\n",p->lerr);
		else
			fprintf(stderr,"icoms: Write_Read Write_Read success, returning '%s'\n",icoms_fix(rbuf));
	}
	if (p->lerr == ICOM_OK && xuserm != ICOM_OK)
		return xuserm;

	return p->lerr;
}

/* ------------------------------------------------- */
/* Set the usb port number and characteristics. */
/* This may be called to re-establish a connection that has failed */
static void
icoms_set_usb_port(
icoms *p, 
int    port,			/* USB com port, 1 - N, 0 for no change. */
int    config,			/* Configuration */
int    wr_ep,			/* "Serial" Write end point */
int    rd_ep,			/* "Serial" Read end point */
icomuflags usbflags,	/* Any special handling flags */
int retries,			/* > 0 if we should retry set_configuration (100msec) */ 
char **pnames			/* List of process names to try and kill before opening */
) {
	if (p->debug) fprintf(stderr,"icoms: About to set usb port characteristics\n");

	if (p->is_open) 
		p->close_port(p);

	if (p->is_usb_portno(p, port) != instUnknown) {

		usb_open_port(p, port, config, wr_ep, rd_ep, usbflags, retries, pnames);

		p->write = icoms_usb_ser_write;
		p->read = icoms_usb_ser_read;

	}
	if (p->debug) fprintf(stderr,"icoms: usb port characteristics set ok\n");

	// ~~~999
//	if (p->debug)
//		usb_set_debug(p->debug);
}

/* Reset user interrupt handling to default (Esc, ^C, q or 'Q' = Abort) */
static void icoms_reset_uih(
icoms *p
) {
	int i;

	for (i = 0; i < 255; i++)
		p->uih[i] = ICOM_OK;

	p->uih[0x1b] = ICOM_USER;	/* Escape */
	p->uih['q']  = ICOM_USER;	/* q */
	p->uih['Q']  = ICOM_USER;	/* Q */
	p->uih[0x03] = ICOM_USER;	/* ^C */
}

/* Set a key range to the given handling type */
/* min & max are between 0 and 255, status is one of */
static void icoms_set_uih(
icoms *p,
int min,		/* Start key code */
int max,		/* End key code (inclusive) */
int status		/* ICOM_OK, ICOM_USER, ICOM_TERM, ICOM_TRIG, ICOM_CMND */
) {
	int i;

	if (min < 0)
		min = 0;
	else if (min > 255)
		min = 255;
	if (max < 0)
		max = 0;
	else if (max > 255)
		max = 255;

	if (status != ICOM_OK
	 && status != ICOM_USER
	 && status != ICOM_TERM
	 && status != ICOM_TRIG
	 && status != ICOM_CMND)
		status = ICOM_OK;

	for (i = min; i <= max; i++) {
		p->uih[i] = status;
	}
}

/* Get the character that caused the user interrupt */
/* Clear it to 0x00 after reading it */
static int icoms_get_uih_char(icoms *p) {
	int c = p->cut;
	p->cut = 0;
	return c;
}


/* Poll for a user abort, terminate, trigger or command. */
/* Wait for a key rather than polling, if wait != 0 */
/* Return: */
/* ICOM_OK if no key was hit or the key has no meaning. */
/* ICOM_USER if User abort has been hit, */
/* ICOM_TERM if User terminate has been hit. */
/* ICOM_TRIG if User trigger has been hit */
/* ICOM_CMND if User command has been hit */
int icoms_poll_user(icoms *p, int wait) {
	int c;

	if (wait) {
		int rv;
		for (;;) {
			c = next_con_char();
			p->cut = c;
			rv = p->uih[c];
			if (rv != ICOM_OK)
				return rv;
		}
	} else {
		c = poll_con_char();
		if (c != 0) {
			p->cut = c;
			return p->uih[c];
		}
	}
	return ICOM_OK;
}

/* ---------------------------------------------------------------------------------*/

/* Set the USB specific icoms methods */
void usb_set_usb_methods(
icoms *p
) {
	p->write_read    = icoms_write_read;

	p->reset_uih     = icoms_reset_uih;
	p->set_uih       = icoms_set_uih;
	p->get_uih_char  = icoms_get_uih_char;

	p->is_usb_portno  = usb_is_usb_portno;
	p->set_usb_port   = icoms_set_usb_port;
	p->usb_control_th = icoms_usb_control_th;
	p->usb_control    = icoms_usb_control;
	p->usb_read_th    = icoms_usb_read_th;
	p->usb_read       = icoms_usb_read;
	p->usb_write_th   = icoms_usb_write_th;
	p->usb_write      = icoms_usb_write;
	p->usb_cancel_io  = icoms_usb_cancel_io;
	p->usb_resetep    = icoms_usb_resetep;
	p->usb_clearhalt  = icoms_usb_clearhalt;

	icoms_reset_uih(p);
}

/* ---------------------------------------------------------------------------------*/
/* utilities */

/* Emit a "normal" beep */
void normal_beep() {
	/* 0msec delay, 1.0KHz for 200 msec */
	msec_beep(0, 1000, 200);
}

/* Emit a "good" beep */
void good_beep() {
	/* 0msec delay, 1.2KHz for 200 msec */
	msec_beep(0, 1200, 200);
}

/* Emit a "bad" double beep */
void bad_beep() {
	/* 0msec delay, 800Hz for 200 msec */
	msec_beep(0, 800, 200);
	/* 500msec delay, 800Hz for 200 msec */
	msec_beep(350, 800, 200);
}

/* Convert control chars to ^[A-Z] notation in a string */
char *
icoms_fix(char *ss) {
	static unsigned char buf[1005];
	unsigned char *d;
	unsigned char *s = (unsigned char *)ss;
	for(d = buf; (d - buf) < 1000;) {
		if (*s < ' ' && *s > '\000') {
			*d++ = '^';
			*d++ = *s++ + '@';
		} else if (*s >= 0x80) {
			*d++ = '\\';
			*d++ = '0' + ((*s >> 6) & 0x3);
			*d++ = '0' + ((*s >> 3) & 0x7);
			*d++ = '0' + ((*s++ >> 0) & 0x7);
		} else {
			*d++ = *s++;
		}
		if (s[-1] == '\000')
			break;
	}
	*d++ = '.';
	*d++ = '.';
	*d++ = '.';
	*d++ = '\000';
	return (char *)buf;
}

/* Convert a limited binary buffer to a list of hex */
char *
icoms_tohex(unsigned char *s, int len) {
	static char buf[64 * 3 + 10];
	int i;
	char *d;

	for(i = 0, d = buf; i < 64 && i < len; i++, s++) {
		sprintf(d, "%s%02x", i > 0 ? " " : "", *s);
		d += strlen(d);
	}
	if (i < len)
		sprintf(d, " ...");

	return buf;
}

/* ---------------------------------------------------------------------------------*/
