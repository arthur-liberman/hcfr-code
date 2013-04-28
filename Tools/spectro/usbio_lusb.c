
/* General USB I/O support, legacy Libusb 0.1/1.0 implementation, no longer */
/* used by default, but can be configured in the Jamfile. */
/* The corresponding libusb code is not distributed with the current source */
/* though, and would have to be copied from ArgyllCMS V1.4.0 */

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

/* To simplify error messages: */
#ifdef USE_LIBUSB1
# define USB_STRERROR(RV) libusb_strerror(RV)
#else
# define USB_STRERROR(RV) usb_strerror()
#endif

#ifdef USE_LIBUSB1
# define usb_device             libusb_device
# define usb_device_descriptor  libusb_device_descriptor
# define usb_dev_handle         libusb_device_handle
# define usb_config_descriptor  libusb_config_descriptor
# define usb_strerror           libusb_strerror
#endif

#if defined(__FreeBSD__)		/* Shut spurious warnings up */
# define CASTFIX (intptr_t)
#else
# define CASTFIX
#endif

/* Check a USB Vendor and product ID, and add the device */
/* to the icoms path if it is supported. */
/* (this is used to help implement usb_get_paths) */
/* return icom error */
static int usb_check_and_add(
icompaths *p,
struct usb_device *usbd
) {
	instType itype;
	int nep;		/* Number of end points */

	struct usb_device_descriptor descriptor;

#ifdef USE_LIBUSB1
	enum libusb_error rv;

	if ((rv = libusb_get_device_descriptor(usbd, &descriptor)) != LIBUSB_SUCCESS) {
		a1loge(p->log, ICOM_SYS, "usb_check_and_add: failed with %d (%s)\n",
		                                               rv,USB_STRERROR(rv));
		return ICOM_SYS;
	}
#else
	descriptor = usbd->descriptor;	/* Copy */
#endif
	a1logd(p->log, 6, "usb_check_and_add: called with VID 0x%x, PID 0x%x\n",descriptor.idVendor, descriptor.idProduct);

#ifdef USE_LIBUSB1
#if defined(NT) || defined(__APPLE__)
	/* Ignore libusb1 HID driver capability */
	{
		struct libusb_config_descriptor *confdesc = NULL;

		/* If not obviously HID, we need to fetch the config descriptor */
		if (descriptor.bDeviceClass != LIBUSB_CLASS_HID
		 && (rv = libusb_get_config_descriptor(usbd, 0, &confdesc)) != LIBUSB_SUCCESS) {
			/* This seems to happen for hubs etc., so ignore it */
			a1logd(p->log, 6 , "usb_check_and_add: get conf desc. failed - device not reconized\n");
			return ICOM_OK;
		}

		if (descriptor.bDeviceClass == LIBUSB_CLASS_HID
		 || (confdesc->bNumInterfaces > 0
		  && confdesc->interface[0]. num_altsetting > 0
		  && confdesc->interface[0].altsetting[0].bInterfaceClass == LIBUSB_CLASS_HID)) {
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
				a1logd(p->log, 6, "Is an HID device and already added\n");
				if (confdesc != NULL)
					libusb_free_config_descriptor(confdesc);
				return ICOM_OK;
			}
		}
		if (confdesc != NULL)
			libusb_free_config_descriptor(confdesc);
	}
#endif
#endif

	/* The i1pro2 is detected by checking the number of end points, */
	/* so set this value in icom now by looking at the descriptor */
	{
#ifdef USE_LIBUSB1
	    struct libusb_config_descriptor *confdesc;
		const struct libusb_interface_descriptor *ifd;

		if (libusb_get_config_descriptor(usbd, 0, &confdesc) != LIBUSB_SUCCESS) {
			/* This seems to happen for hubs etc., so ignore it */
			a1logd(p->log, 6 , "usb_check_and_add: get conf desc. failed - device not reconized\n");
			return ICOM_OK;
		}

		ifd = &confdesc->interface[0].altsetting[0];
		nep = ifd->bNumEndpoints;
		if (confdesc != NULL)
			libusb_free_config_descriptor(confdesc);
#else
		struct usb_interface_descriptor *ifd;
		ifd = &usbd->config[0].interface[0].altsetting[0];
		nep = ifd->bNumEndpoints;
#endif
	}
		
	if ((itype = inst_usb_match((unsigned int)descriptor.idVendor,
		                        (unsigned int)descriptor.idProduct, nep)) != instUnknown) {
		char pname[400];

		a1logd(p->log, 2, "usb_check_and_add: found known instrument VID 0x%x, PID 0x%x\n",
		                                        descriptor.idVendor, descriptor.idProduct);

		/* Create a path/identification */
		/* (devnum doesn't seem valid ?) */
#ifdef USE_LIBUSB1
		libusb_ref_device(usbd);		/* Keep it */
		sprintf(pname,"usb:/bus%d/dev%d/ (%s)",libusb_get_bus_number(usbd),libusb_get_device_address(usbd), inst_name(itype));
#else
# if defined(UNIX)
		sprintf(pname,"usb:/bus%d/dev%d (%s)",usbd->bus->location >> 24, usbd->devnum, inst_name(itype));
# else
		sprintf(pname,"usb:/bus%lu/dev%d (%s)",usbd->bus->location, usbd->devnum, inst_name(itype));
# endif
#endif

		/* Add the path to the list */
		p->add_usb(p, pname, descriptor.idVendor, descriptor.idProduct, nep, usbd, itype); 
		return ICOM_OK;
	}
	a1logd(p->log, 6 , "usb_check_and_add: device not reconized\n");

	return ICOM_OK;
}

#ifdef USE_LIBUSB1

/* Add paths of USB connected instruments */
/* return icom error */
int usb_get_paths(
icompaths *p 
) {
	ssize_t i, nlist;
	struct libusb_device **list;

	/* Scan the USB busses for instruments we recognise */
	/* We're not expecting any of our instruments to be an interface on a device. */

	/* Use the default context to avoid worying about versions */
	/* of libusb1 that don't reference count them. (ie. would need */
	/* copies in both icompaths and icoms) */

   	libusb_init(NULL);		/* Use default context */

	/* Enable lower level debugging of device finding */
	if (p->log->debug >= 8)
		libusb_set_debug(NULL, p->log->debug);

	if ((nlist = libusb_get_device_list(NULL, &list)) < 0) {
		a1loge(p->log, ICOM_SYS, "usb_get_paths: get_device_list failed with %d (%s)\n",
		                                                     nlist,USB_STRERROR(nlist));
		return ICOM_SYS;
	}
    
	a1logd(p->log, 6, "usb_get_paths: about to look through devices:\n");

	for (i = 0; i < nlist; i++) {
		usb_check_and_add(p, list[i]);
   	}

	libusb_free_device_list(list,  1);

	a1logd(p->log, 8, "usb_get_paths: returning %d paths and ICOM_OK\n",p->npaths);
	return ICOM_OK;
}

#else /* !USE_LIBUSB1 */

/* Add paths of USB connected instruments */
/* return icom error */
int usb_get_paths(
icompaths *p 
) {
	struct usb_bus *bus;
	int rv;

	/* Enable lower level debugging of device finding */
	if (p->log->debug >= 8)
		usb_set_debug(p->log->debug);

	/* Scan the USB busses for instruments we recognise */
	/* We're not expecting any of our instruments to be an interface on a device. */

   	usb_init();
   	if ((rv = usb_find_busses()) < 0) {
		a1loge(p->log, ICOM_SYS, "usb_get_paths: find_busses failed with %d (%s)\n",
		                                                            rv,USB_STRERROR(rv));
		return ICOM_SYS;
	}
   	if ((rv = usb_find_devices()) < 0) {
		a1loge(p->log, ICOM_SYS, "usb_get_paths: usb_find_devices failed with %d (%s)\n",
		                                                            rv,USB_STRERROR(rv));
		return ICOM_SYS;
	}
    
	a1logd(p->log, 6, "usb_get_paths: about to look through busses:\n");

   	for (bus = usb_get_busses(); bus != NULL; bus = bus->next) {
		struct usb_device *dev;
		a1logd(p->log, 6, "usb_get_paths: about to look through devices:\n");
   		for (dev = bus->devices; dev != NULL; dev = dev->next) {
			if ((rv = usb_check_and_add(p, dev)) != ICOM_OK)
				return rv;
		}
   	}
	a1logd(p->log, 8, "usb_get_paths: returning %d paths and ICOM_OK\n",p->npaths);
	return ICOM_OK;
}
#endif /* !USE_LIBUSB1 */

/* Copy usb_idevice contents from icompaths to icom */
/* return icom error */
int usb_copy_usb_idevice(icoms *d, icompath *s) {
	if (s->usbd == NULL) { 
		d->usbd = NULL;
		return ICOM_OK;
	}

	d->usbd = s->usbd;		/* Copy pointer */
#ifdef USE_LIBUSB1
	libusb_ref_device(d->usbd);
#endif
	return ICOM_OK;
}

/* Cleanup and then free a usb dev entry */
void usb_del_usb_idevice(struct usb_idevice *usbd) {

	if (usbd == NULL)
		return;

#ifdef USE_LIBUSB1
	libusb_unref_device(usbd);
#endif
}

/* Cleanup any USB specific icoms state */
void usb_del_usb(icoms *p) {

#ifdef USE_LIBUSB1
	usb_del_usb_idevice(p->usbd);
#endif /* USE_LIBUSB1 */
}

/* Close an open USB port */
/* If we don't do this, the port and/or the device may be left in an unusable state. */
void usb_close_port(icoms *p) {

	a1logd(p->log, 8, "usb_close_port: called\n");

	if (p->is_open && p->usbh != NULL) {
		int iface;

		for (iface = 0; iface < p->nifce; iface++) {
#ifdef USE_LIBUSB1
			libusb_release_interface(p->usbh, iface);
#else
			usb_release_interface(p->usbh, iface);
#endif
		}

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

		a1logd(p->log, 8, "usb_close_port: port has been released and closed\n");
	}
	p->is_open = 0;

	/* Find it and delete it from our static cleanup list */
	usb_delete_from_cleanup_list(p);

}

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
	int tries = 0;
	a1logd(p->log, 8, "usb_open_port: Make sure USB port is open, tries %d\n",retries);

	if (p->is_open) 
		p->close_port(p);

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

		/* Do open retries */
		for (tries = 0; retries >= 0; retries--, tries++) {

			a1logd(p->log, 8, "usb_open_port: About to open USB port '%s'\n",p->name);

			if (tries > 0) {
				//msec_sleep(i_rand(50,100));
				msec_sleep(77);
			}
			if (tries > 0 && pnames != NULL && kpc == NULL) {
#if defined(__APPLE__) || defined(NT)
				if ((kpc = kkill_nprocess(pnames, p->log)) == NULL)
					a1logd(p->log, 1, "usb_open_port: kkill_nprocess returned error!\n");
#endif /* __APPLE__ */
			}

#ifdef USE_LIBUSB1
			if ((rv = libusb_open(p->usbd, &p->usbh)) != LIBUSB_SUCCESS)
#else
			if ((p->usbh = usb_open(p->usbd)) == NULL)
#endif
			{
				a1logd(p->log, 8, "usb_open_port: open '%s' config %d failed (%s) (Permissions ?)\n",p->name,config,USB_STRERROR(rv));
				if (retries <= 0) {
					if (kpc != NULL)
						kpc->del(kpc); 
					a1loge(p->log, ICOM_SYS, "usb_open_port: open '%s' config %d failed (%s) (Permissions ?)\n",p->name,config,USB_STRERROR(rv));
					return ICOM_SYS;
				}
				continue;
			} else if (p->debug)
				a1logd(p->log, 2, "usb_open_port: open port '%s' succeeded\n",p->name);

			/* Get a copy of the device descriptor so we can see device params */
#ifdef USE_LIBUSB1
			if (libusb_get_device_descriptor(p->usbd, &descriptor) != LIBUSB_SUCCESS) {
				a1loge(p->log, ICOM_SYS, "usb_open_port: get device descriptor on '%s' failed with %d (%s)\n",p->name,rv,USB_STRERROR(rv));
				return ICOM_SYS;
			}
#else
			descriptor = p->usbd->descriptor;	/* Copy */
#endif

			p->uflags = usbflags;

			a1logd(p->log, 8, "usb_open_port: Number of configurations = %d\n",
			                                    descriptor.bNumConfigurations);
			p->nconfig = descriptor.bNumConfigurations;
			p->config = config;

#if defined(UNIX_X11)
			/* only call set_configuration on Linux if the device has more than one */
			/* possible configuration, because Linux does a set_configuration by default, */
			/* and two of them mess up instruments like the Spyder2 */

			if (descriptor.bNumConfigurations > 1) {
#endif

			/* Can't skip this, as it is needed to setup the interface and end points on OS X */
#ifdef USE_LIBUSB1
			if ((rv = libusb_set_configuration(p->usbh, config)) < 0)
#else
			if ((rv = usb_set_configuration(p->usbh, config)) < 0)
#endif
			{
				a1logd(p->log, 8, "usb_open_port: configuring '%s' to %d failed with %d (%s)\n",p->name,config,rv,USB_STRERROR(rv));
				if (retries <= 0) {
					if (kpc != NULL)
						kpc->del(kpc); 
					a1loge(p->log, ICOM_SYS, "usb_open_port: configuring '%s' to %d failed with %d (%s)\n",p->name,config,rv,USB_STRERROR(rv));
					return ICOM_SYS;
				}
				/* reset the port and retry */
#ifdef USE_LIBUSB1
				libusb_reset_device(p->usbh); // ~~999 ?????
				libusb_close(p->usbh);
#else
				usb_reset(p->usbh);
				usb_close(p->usbh);
#endif
				continue;
			}
#if defined(UNIX_X11)
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
			a1loge(p->log, ICOM_SYS, "usb_open_port: get config desc. for '%s' failed with %d (%s)\n",p->name,rv,USB_STRERROR(rv));
			return ICOM_SYS;
		}
		p->nifce = confdesc->bNumInterfaces;
#else
		p->nifce = p->usbd->config->bNumInterfaces;
#endif

//		a1logv(p->log, 8, "usb_open_port: Number of interfaces = %d\n",p->nifce);

		/* Claim all the interfaces */
		for (iface = 0; iface < p->nifce; iface++) {
			/* (Second parameter is bInterfaceNumber) */

#ifdef USE_LIBUSB1
			if ((rv = libusb_claim_interface(p->usbh, iface)) < 0) {
				/* Detatch the existing interface if kernel driver is active. */
				if (p->uflags & icomuf_detach 
					&& libusb_kernel_driver_active(p->usbh, iface) == 1) {
					libusb_detach_kernel_driver (p->usbh, iface);
					if ((rv = libusb_claim_interface(p->usbh, iface)) < 0) {
						a1loge(p->log, ICOM_SYS, "usb_open_port: Claiming USB port '%s' interface %d failed after detach with %d (%s)\n",p->name,iface,rv,USB_STRERROR(rv));
						return ICOM_SYS;
					}
				} else {
					a1loge(p->log, ICOM_SYS, "usb_open_port: Claiming USB port '%s' interface %d failed with %d (%s)\n",p->name,iface,rv,USB_STRERROR(rv));
					return ICOM_SYS;
				}
			}
#else
			if ((rv = usb_claim_interface(p->usbh, iface)) < 0) {
# if LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP == 1
				/* Detatch the existing interface. */
				if (p->uflags & icomuf_detach) {
					usb_detach_kernel_driver_np(p->usbh, iface);
					if ((rv = usb_claim_interface(p->usbh, iface)) < 0) {
						a1loge(p->log, ICOM_SYS, "usb_open_port: Claiming USB port '%s' interface %d failed after detach with %d (%s)\n",p->name,iface,rv,USB_STRERROR(rv));
						return ICOM_SYS;
					}
				}
# else
				a1loge(p->log, ICOM_SYS, "usb_open_port: Claiming USB port '%s' interface %d failed with %d (%s)\n",p->name,iface,rv,USB_STRERROR(rv));
				return ICOM_SYS;
# endif
			}
#endif
	
			/* Fill in the end point details */
#ifdef USE_LIBUSB1
			ifd = &confdesc->interface[iface].altsetting[0];
#else
			ifd = &p->usbd->config[p->config-1].interface[iface].altsetting[0];
#endif
//			a1logv(p->log, 8, "usb_open_port: Number of endpoints on iface %d = %d\n",iface, ifd->bNumEndpoints);
			p->nep = ifd->bNumEndpoints;
			for (i = 0; i < ifd->bNumEndpoints; i++) {
				int ad = ifd->endpoint[i].bEndpointAddress;
				p->EPINFO(ad).valid = 1;
				p->EPINFO(ad).addr = ad;
				p->EPINFO(ad).packetsize = ifd->endpoint[i].wMaxPacketSize;
				p->EPINFO(ad).type = ifd->endpoint[i].bmAttributes & IUSB_ENDPOINT_TYPE_MASK;
				/* Some I/F seem to hang if we do this, some seem to hang if we don't ! */
				if (!(p->uflags & icomuf_no_open_clear))
#ifdef USE_LIBUSB1
					libusb_clear_halt(p->usbh, (unsigned char)ad);
#else
					usb_clear_halt(p->usbh, ad);
#endif
//				a1logv(p->log, 8, "usb_open_port: ep %d: endpoint addr %02x pktsze %d, type %d\n",i,ad,ifd->endpoint[i].wMaxPacketSize,p->EPINFO(ad).type);
			}

#ifdef NEVER
			/* Get the serial number */
			{
				int rv;
				struct usb_device_descriptor descriptor;

				a1logd(p->log, 8, "usb_open_port: About to get device serial number\n");
# ifdef USE_LIBUSB1
				if ((rv = libusb_get_device_descriptor(p->usbd, &descriptor)) != LIBUSB_SUCCESS) {
					a1loge(p->log, ICOM_SYS, "usb_open_port: get_device_descriptor failed with %d USB port '%s'\n",rv,p->name);
					return ICOM_SYS;
				}
# else
				descriptor = dev->descriptor;	/* Copy */
# endif
				if ((rv = libusb_get_string_descriptor_ascii(p->usbh, descriptor.iSerialNumber, p->serialno, 32)) <= 0) {
					a1logd(p->log, 1, "usb_open_port: Failed to get device serial number %d (%s)\n",rv,USB_STRERROR(rv));
					p->serialno[0] = '\000';
				} else {
					a1logd(p->log, 1, "usb_open_port: Device serial number = '%s'\n",p->serialno);
				}
			}
#endif /* NEVER */
		}

		/* Set "serial" coms values */
		p->wr_ep = wr_ep;
		p->rd_ep = rd_ep;
		p->rd_qa = p->EPINFO(rd_ep).packetsize;
		if (p->rd_qa == 0)
			p->rd_qa = 8;
		a1logd(p->log, 8, "usb_open_port: 'serial' read quanta = packet size = %d\n",p->rd_qa);

#ifdef USE_LIBUSB1
		if (confdesc != NULL)
			libusb_free_config_descriptor(confdesc);
#endif
		
		p->is_open = 1;
		a1logd(p->log, 8, "usb_open_port: USB port is now open\n");
	}

	/* Install the cleanup signal handlers, and add to our cleanup list */
	usb_install_signal_handlers(p);

	return ICOM_OK;
}

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Time out error return value */

#ifdef USE_LIBUSB1
#define USBIO_ERROR_TIMEOUT	LIBUSB_ERROR_TIMEOUT
#else
# if defined(UNIX)
#define USBIO_ERROR_TIMEOUT	-ETIMEDOUT
# else
#define USBIO_ERROR_TIMEOUT -116	/* libusb-win32 code */
# endif /* UNIX */
#endif

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

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
/* Return an icoms error code */
static int do_sync_usb_transfer(
	icoms *p,
	struct libusb_device_handle *dev_handle,
	usb_cancelt *cancelt,
	int *transferred,
	icom_usb_trantype ttype,
	unsigned char endpoint,
	unsigned char *buffer,
	int length,
	unsigned int timeout
) {
	enum libusb_transfer_type type;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	int completed = 0;
	int rv;

	if (!transfer) {
		a1logd(p->log, 1, "do_sync_usb_transfer: transfer is NULL!\n");
		return ICOM_SYS;
	}

	/* Translate icoms transfer type of libusb1 */
	switch (ttype) {
		case icom_usb_trantype_command:
			type = LIBUSB_TRANSFER_TYPE_CONTROL;
			break;
		case icom_usb_trantype_interrutpt:
			type = LIBUSB_TRANSFER_TYPE_INTERRUPT;
			break;
		case icom_usb_trantype_bulk:
			type = LIBUSB_TRANSFER_TYPE_BULK;
			break;
	}

	libusb_fill_bulk_transfer(transfer, dev_handle, endpoint, buffer, length,
		bulk_transfer_cb, &completed, timeout);
	transfer->type = type;

	if ((rv = libusb_submit_transfer(transfer)) < 0) {
		libusb_free_transfer(transfer);
		a1logd(p->log, 1, "do_sync_usb_transfer: Submitting transfer failed with %d (%s)\n",rv,USB_STRERROR(rv));
		if ((endpoint & IUSB_ENDPOINT_DIR_MASK) == IUSB_ENDPOINT_OUT)
			return ICOM_USBW; 
		else
			return ICOM_USBR; 
	}

	if (cancelt != NULL) {
		usb_lock_cancel(cancelt);
		cancelt->hcancel = (void *)transfer;
		usb_unlock_cancel(cancelt);
	}

	while (!completed) {
		if ((rv = libusb_handle_events_check(NULL, &completed)) < 0) {
			if (rv == LIBUSB_ERROR_INTERRUPTED)
				continue;			/* Retry */
			/* Give up - cancel transfer and wait until complete */
			libusb_cancel_transfer(transfer);
			while (!completed) {
				if (libusb_handle_events_check(NULL, &completed) < 0)
					break;
			}
			if (cancelt != NULL) {
				usb_lock_cancel(cancelt);
				cancelt->hcancel = NULL;
				usb_unlock_cancel(cancelt);
			}
			libusb_free_transfer(transfer);
			a1loge(p->log, ICOM_SYS, "do_sync_usb_transfer: handle_events failed with %d (%s)\n",rv,USB_STRERROR(rv));
			return ICOM_SYS; 
		}
	}

	*transferred = transfer->actual_length;
	switch (transfer->status) {
		case LIBUSB_TRANSFER_COMPLETED:
			rv = ICOM_OK;
			break;
		case LIBUSB_TRANSFER_TIMED_OUT:
			rv = ICOM_TO;
			break;
		case LIBUSB_TRANSFER_STALL:
		case LIBUSB_TRANSFER_OVERFLOW:
		case LIBUSB_TRANSFER_NO_DEVICE:
			if ((endpoint & IUSB_ENDPOINT_DIR_MASK) == IUSB_ENDPOINT_OUT)
				rv = ICOM_USBW; 
			else
				rv = ICOM_USBR; 
			break;
		case LIBUSB_TRANSFER_CANCELLED:
			rv = ICOM_CANC;
			break;
		default:
			a1loge(p->log, ICOM_SYS, "do_sync_usb_transfer: transfer faile with %d (%s)\n",rv,USB_STRERROR(rv));
			rv = ICOM_SYS;
	}

	if (cancelt != NULL) {
		usb_lock_cancel(cancelt);
		cancelt->hcancel = NULL;
		usb_unlock_cancel(cancelt);
	}
	libusb_free_transfer(transfer);

	/* requested size wasn't transferred */
	if (rv == ICOM_OK && *transferred != length)
		rv = ICOM_SHORT;

	return rv;
}
#endif	/* USE_LIBUSB1 */

/* Return icom error code */
static int icoms_usb_control_msg(
icoms *p,
int *transferred,
int requesttype, int request,
int value, int index, unsigned char *bytes, int size, 
int timeout) {
	int rv;

	in_usb_rw++;
	a1logd(p->log, 8, "icoms_usb_control_msg: type 0x%x, req 0x%x, size %d\n",requesttype,request,size);

	if (transferred != NULL)
		*transferred = 0;
#ifdef USE_LIBUSB1
	rv = libusb_control_transfer(p->usbh, (uint8_t)requesttype, (uint8_t)request,
	        (uint16_t)value, (uint16_t)index, bytes, (uint16_t)size, timeout);
#else
	rv = usb_control_msg(p->usbh, requesttype, request, value, index, (char *)bytes, size, timeout);
#endif
	if (in_usb_rw < 0)	/* interrupt recurssion */
		exit(0);

	in_usb_rw--;

	if (rv < 0) {
		if (rv == USBIO_ERROR_TIMEOUT) {	/* Not a timeout */
			rv = ICOM_TO; 
		} else {
			if (requesttype & IUSB_ENDPOINT_IN)	/* Device to host */
				rv = ICOM_USBR;				/* Read error */ 
			else
				rv = ICOM_USBW; 			/* Write error */
		}
	} else {
		if (transferred != NULL)
			*transferred = rv;
		if (rv != size) {
			rv = ICOM_SHORT;
		} else
			rv = ICOM_OK;
	}
	a1logd(p->log, 8, "icoms_usb_control_msg: returning err 0x%x and %d bytes\n",rv, *transferred);
	return rv;
}

/* Our versions of usblib read/write, that exit if a signal was caught */
/* This is so that MSWindows works properly */
/* return an icom error */
static int icoms_usb_transaction(icoms *p, usb_cancelt *cancelt, int *xbytes,
	icom_usb_trantype type, int ep, unsigned char *bytes, int size, int timeout) {
	int rv;

	in_usb_rw++;

	a1logd(p->log, 8, "coms_usb_transaction: req type 0x%x ep 0x%x size %d\n",type,ep,size);
#ifdef USE_LIBUSB1
	rv = do_sync_usb_transfer(p, p->usbh, cancelt, xbytes, type,
	                           (unsigned char)ep, bytes, size, timeout);
#else
	if (xbytes != NULL)
		*xbytes = 0;
	if (cancelt != NULL) {
		usb_lock_cancel(cancelt);
		cancelt->hcancel = (void *) CASTFIX ep;
		usb_lock_cancel(cancelt);
	}
	if (type == icom_usb_trantype_interrutpt) {
		if ((ep & IUSB_ENDPOINT_DIR_MASK) == IUSB_ENDPOINT_OUT)
			rv = usb_interrupt_write(p->usbh, ep, (char *)bytes, size, timeout);
		else
			rv = usb_interrupt_read(p->usbh, ep, (char *)bytes, size, timeout);
	} else {	/* bulk */
		if ((ep & IUSB_ENDPOINT_DIR_MASK) == IUSB_ENDPOINT_OUT)
			rv = usb_bulk_write(p->usbh, ep, (char *)bytes, size, timeout);
		else
			rv = usb_bulk_read(p->usbh, ep, (char *)bytes, size, timeout);
	}
	if (cancelt != NULL) {
		usb_lock_cancel(cancelt);
		cancelt->hcancel = (void *)-1;
		usb_lock_cancel(cancelt);
	}
	if (rv < 0) {
		if (rv == USBIO_ERROR_TIMEOUT) 
			rv = ICOM_TO; 
		else {
			if ((ep & IUSB_ENDPOINT_DIR_MASK) == IUSB_ENDPOINT_OUT)
				rv = ICOM_USBW; 
			else
				rv = ICOM_USBR; 
		}
	} else {
		if (xbytes != NULL)
			*xbytes = rv;
		if (rv != *xbytes)
			rv = ICOM_SHORT;
		else
			rv = ICOM_OK;
	}
#endif
	if (in_usb_rw < 0)	/* Signal handler recursion error */
		exit(0);

	in_usb_rw--;

	a1logd(p->log, 8, "coms_usb_transaction: returning err 0x%x and %d bytes\n",rv, *xbytes);

	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Cancel i/o in another thread */
int icoms_usb_cancel_io(
	icoms *p,
	usb_cancelt *cancelt
) {
	int rv = 0;
	usb_lock_cancel(cancelt);
#ifdef USE_LIBUSB1
	if (cancelt->hcancel != NULL) {
		rv = libusb_cancel_transfer((struct libusb_transfer *)cancelt->hcancel);
		if (rv == LIBUSB_ERROR_NOT_FOUND)
			rv = 0;
	}
#else
	if ((int) CASTFIX cancelt->hcancel >= 0) {
//		msec_sleep(1);		/* Let device recover ? */
		rv = usb_resetep(p->usbh, (int) CASTFIX cancelt->hcancel);	/* Not reliable ? */
//		msec_sleep(1);		/* Let device recover ? */
	}
#endif
	usb_unlock_cancel(cancelt);

	if (rv == 0)
		return ICOM_OK;

	a1logd(p->log, 1, "icoms_usb_cancel_io: failed with %d (%s)\n", rv,USB_STRERROR(rv));
	return ICOM_SYS;
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

	a1logd(p->log, 1, "icoms_usb_resetep: failed with %d (%s)\n", rv,USB_STRERROR(rv));
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

	a1logd(p->log, 1, "icoms_usb_clearhalt: failed with %d (%s)\n", rv,USB_STRERROR(rv));
	return ICOM_USBW;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

