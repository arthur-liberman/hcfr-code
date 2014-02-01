
/* General USB I/O support, OS X native implementation. */
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

#include <sys/time.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>
//#include <IOKit/IOCFBundle.h>
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
# include <objc/objc-auto.h>
#endif

/* Add paths to USB connected instruments */
/* Return an icom error */
int usb_get_paths(
icompaths *p 
) {
    kern_return_t kstat; 
    CFMutableDictionaryRef sdict;		/* USB Device  dictionary */
	io_iterator_t mit;					/* Matching itterator */
	int vid, pid;
	struct usb_idevice *usbd = NULL;

	a1logd(p->log, 6, "usb_get_paths: about to look through devices:\n");

	/* Get dictionary of USB devices */
   	if ((sdict = IOServiceMatching(kIOUSBDeviceClassName)) == NULL) {
        	a1loge(p->log, ICOM_SYS, "usb_get_paths() IOServiceMatching returned a NULL dictionary\n");
		return ICOM_SYS;
	}

	/* Init itterator to find matching types. Consumes sdict reference */
	if ((kstat = IOServiceGetMatchingServices(kIOMasterPortDefault, sdict, &mit))
		                                                          != KERN_SUCCESS) { 
        	a1loge(p->log, ICOM_SYS, "usb_get_paths() IOServiceGetMatchingServices returned %d\n", kstat);
		return ICOM_SYS;
	}

	/* Find all the matching USB devices */
	for (;;) {
		io_object_t ioob;						/* USB object found */
		io_iterator_t it1;						/* Child level 1 */
	    CFMutableDictionaryRef usbprops = 0;	/* USB Device properties */
		CFNumberRef vref, pref;					/* USB Vendor and Product ID propeties */
		CFNumberRef nconfref;					/* No configurations */
		CFNumberRef nepref, lidpref;			/* No ep's, Location ID properties */
		unsigned int vid = 0, pid = 0, nep, tnep, nconfig = 0, lid = 0;
		instType itype;

	    if ((ioob = IOIteratorNext(mit)) == 0)
			break;

		/* Get the two properies we need. */
		if ((vref = IORegistryEntryCreateCFProperty(ioob, CFSTR(kUSBVendorID),
		                                         kCFAllocatorDefault,kNilOptions)) != 0) {
			CFNumberGetValue(vref, kCFNumberIntType, &vid);
		    CFRelease(vref);
		}
		if ((pref = IORegistryEntryCreateCFProperty(ioob, CFSTR(kUSBProductID),
		                                         kCFAllocatorDefault,kNilOptions)) != 0) {
			CFNumberGetValue(pref, kCFNumberIntType, &pid);
		    CFRelease(pref);
		}

		if ((nconfref = IORegistryEntryCreateCFProperty(ioob, CFSTR("bNumConfigurations"),
		                                         kCFAllocatorDefault,kNilOptions)) != 0) {
			CFNumberGetValue(nconfref, kCFNumberIntType, &nconfig);
		    CFRelease(nconfref);
		}

		if ((lidpref = IORegistryEntryCreateCFProperty(ioob, CFSTR("locationID"),
		                                         kCFAllocatorDefault,kNilOptions)) != 0) {
			CFNumberGetValue(lidpref, kCFNumberIntType, &lid);
		    CFRelease(lidpref);
		}

		a1logd(p->log, 6, "usb_check_and_add: checking vid 0x%04x, pid 0x%04x, lid 0x%x\n",vid,pid,lid);

		/* Do a preliminary match */
		if ((itype = inst_usb_match(vid, pid, 0)) == instUnknown) {
			a1logd(p->log, 6 , "usb_check_and_add: 0x%04x 0x%04x not reconized\n",vid,pid);
		    IOObjectRelease(ioob);		/* Release found object */
			continue;
		}

		/* Allocate an idevice so that we can fill in the end point information */
		if ((usbd = (struct usb_idevice *) calloc(sizeof(struct usb_idevice), 1)) == NULL) {
			a1loge(p->log, ICOM_SYS, "icoms: calloc failed!\n");
			return ICOM_SYS;
		}

		usbd->nconfig = nconfig;
		usbd->config = 1;				/* We are only interested in config 1 */

		/* Go through all the Interfaces, adding up the number of end points */
		tnep = 0;
		if ((kstat = IORegistryEntryCreateIterator(ioob, kIOServicePlane,
		                          kIORegistryIterateRecursively, &it1)) != KERN_SUCCESS) {
			IOObjectRelease(ioob);
			IOObjectRelease(mit);
			a1loge(p->log, kstat, "usb_check_and_add: IORegistryEntryCreateIterator() with %d\n",kstat);
		    return ICOM_SYS;
		}
		usbd->nifce = 0;
		for (;;) {
			io_object_t ch1;					/* Child object 1 */
		    if ((ch1 = IOIteratorNext(it1)) == 0)
				break;
			if (IOObjectConformsTo(ch1, "IOUSBInterface")) {
				unsigned int config = 0;
				
				/* Get the configuration number */
				if ((nconfref = IORegistryEntryCreateCFProperty(ch1, CFSTR(kUSBNumEndpoints),
				                                         kCFAllocatorDefault,kNilOptions)) != 0) {
					CFNumberGetValue(nconfref, kCFNumberIntType, &config);
				    CFRelease(nconfref);
				}

				if (config != 1)
					continue;

				usbd->nifce++;

				/* Get the number of end points */
				if ((nepref = IORegistryEntryCreateCFProperty(ch1, CFSTR(kUSBNumEndpoints),
				                                         kCFAllocatorDefault,kNilOptions)) != 0) {
					CFNumberGetValue(nepref, kCFNumberIntType, &nep);
				    CFRelease(nepref);
					tnep += nep;
				}
			    IOObjectRelease(ch1);
			}
		}
		IOObjectRelease(it1);

		if ((itype = inst_usb_match(vid, pid, tnep)) != instUnknown) {
			int i;
			char pname[400];
			int rv;

			/* If this device is HID, it will have already added to the paths list, */
			/* so check for this and skip this device if it is already there. */
			for (i = 0; i < p->npaths; i++) {
				if (p->paths[i]->vid == vid
				 && p->paths[i]->pid == pid
				 && p->paths[i]->hidd != NULL
				 && p->paths[i]->hidd->lid == lid) {
					a1logd(p->log, 1, "usb_check_and_add: Ignoring device because it is already in list as HID\n");
					break;
				}
			}
			if (i < p->npaths) {
			    IOObjectRelease(ioob);		/* Release found object */
				free(usbd);

			} else {
			
				a1logd(p->log, 1, "usb_check_and_add: found instrument vid 0x%04x, pid 0x%04x\n",vid,pid);
				usbd->lid = lid;
				usbd->ioob = ioob;

				/* Create a path/identification */
				sprintf(pname,"usb%d: (%s)", lid >> 20, inst_name(itype));

				/* Add the path and ep info to the list */
				if ((rv = p->add_usb(p, pname, vid, pid, tnep, usbd, itype)) != ICOM_OK) {
				    IOObjectRelease(ioob);		/* Release found object */
					free(usbd);
				    IOObjectRelease(mit);		/* Release the itterator */
					return rv;
				}

			}
		} else {
		    IOObjectRelease(ioob);		/* Release found object */
			free(usbd);
		}
	}
    IOObjectRelease(mit);			/* Release the itterator */

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
	d->nconfig = s->usbd->nconfig;
	d->config = s->usbd->config;
	d->nifce = s->usbd->nifce;
	d->usbd->ioob = s->usbd->ioob;
	IOObjectRetain(d->usbd->ioob);
	return ICOM_OK;
}

/* Cleanup and then free a usb dev entry */
void usb_del_usb_idevice(struct usb_idevice *usbd) {

	if (usbd == NULL)
		return;

	if (usbd->ioob != 0)
		IOObjectRelease(usbd->ioob);

	free(usbd);
}

/* Cleanup any USB specific icoms state */
void usb_del_usb(icoms *p) {

	usb_del_usb_idevice(p->usbd);
}

/* Clean up anything allocated/created in p->usbd, but don't free p->usbd itself */
static void cleanup_device(icoms *p) {
		
	if (p->usbd != NULL) {
		if (p->usbd->device != NULL) {	/* device is open */
			int i;

			/* Stop the RunLoop and wait for it */
			if (p->usbd->cfrunloop != NULL) {

				a1logd(p->log, 6, "usb_close_port: waiting for RunLoop thread to exit\n");
				CFRunLoopStop(p->usbd->cfrunloop);
				CFRelease(p->usbd->cfrunloop);
				if (p->usbd->thread != NULL)
					pthread_join(p->usbd->thread, NULL);
			}

			/* Release all the interfaces */
			for (i = 0; i < p->usbd->nifce; i++) {
				if (p->usbd->interfaces[i] != NULL) {
					(*p->usbd->interfaces[i])->USBInterfaceClose(p->usbd->interfaces[i]);
					(*p->usbd->interfaces[i])->Release(p->usbd->interfaces[i]);
				}
			}

			/* Close the device */
			(*(p->usbd->device))->USBDeviceClose(p->usbd->device);
			(*(p->usbd->device))->Release(p->usbd->device);
		}
		pthread_cond_destroy(&p->usbd->cond);
		pthread_mutex_destroy(&p->usbd->lock);

		memset(p->usbd, 0, sizeof(struct usb_idevice));
	}
}

/* Close an open USB port */
/* If we don't do this, the port and/or the device may be left in an unusable state. */
void usb_close_port(icoms *p) {

	a1logd(p->log, 6, "usb_close_port: called\n");

	if (p->is_open && p->usbd != NULL) {

		/* Workaround for some bugs - reset device on close */
		if (p->uflags & icomuf_reset_before_close) {
			IOReturn rv;
			if ((rv = (*(p->usbd->device))->ResetDevice(p->usbd->device)) != kIOReturnSuccess) {
				a1logd(p->log, 1, "usb_close_port: ResetDevice failed with 0x%x\n",rv);
			}
		}

		/* Close down and free everything in p->usbd */
		cleanup_device(p);
		if (p->usbd != NULL) {
			free(p->usbd);
			p->usbd = NULL;
		}
		a1logd(p->log, 6, "usb_close_port: usb port has been released and closed\n");
	}
	p->is_open = 0;

	/* Find it and delete it from our static cleanup list */
	usb_delete_from_cleanup_list(p);
}

static int icoms_usb_control_msg(icoms *p, int *transferred, int requesttype, int request,
                       int value, int index, unsigned char *bytes, int size, int timeout);
static void *io_runloop(void *context);

/* Open a USB port for all our uses. */
/* This always re-opens the port */
/* return icom error */
static int usb_open_port(
icoms *p,
int    config,		/* Configuration number, (1 based) */
int    wr_ep,		/* Write end point */
int    rd_ep,		/* Read end point */
icomuflags usbflags,/* Any special handling flags */
int retries,		/* > 0 if we should retry set_configuration (100msec) */ 
char **pnames		/* List of process names to try and kill before opening */
) {
	IOReturn rv;
	int tries = 0;
	a1logd(p->log, 8, "usb_open_port: Make sure USB port is open, tries %d\n",retries);

	if (p->is_open)
		p->close_port(p);

	/* Make sure the port is open */
	if (!p->is_open) {
		kkill_nproc_ctx *kpc = NULL;

		if (config != 1) {
			/* Nothing currently needs it, so we haven't implemented it yet... */
			a1loge(p->log, ICOM_NOTS, "usb_open_port: native driver cant handle config %d\n",config);
			return ICOM_NOTS;
		}

		pthread_mutex_init(&p->usbd->lock, NULL);
		pthread_cond_init(&p->usbd->cond, NULL);

		/* Do open retries */
		for (tries = 0; retries >= 0; retries--, tries++) {
			IOCFPlugInInterface **piif = NULL;
			SInt32 score;

			a1logd(p->log, 8, "usb_open_port: About to open USB port '%s'\n",p->name);

			if (tries > 0) {
				//msec_sleep(i_rand(50,100));
				msec_sleep(77);
			}
			if (tries > 0 && pnames != NULL && kpc == NULL) {
				if ((kpc = kkill_nprocess(pnames, p->log)) == NULL) {
					a1logd(p->log, 1, "kkill_nprocess returned error!\n");
				}
			}

			if ((rv = IOCreatePlugInInterfaceForService(p->usbd->ioob,
			                       kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
			                       &piif, &score)) != kIOReturnSuccess || piif == NULL) {
				a1loge(p->log, rv, "usb_open_port: Failed to get piif for "
				  "USB device '%s', result 0x%x, piif 0x%x\n",p->name,rv,piif);
				return ICOM_SYS;
			}
	
			p->usbd->device = NULL;
			if ((rv = (*piif)->QueryInterface(piif,
				    CFUUIDGetUUIDBytes(DeviceInterfaceID), (void *)&p->usbd->device))
			        != kIOReturnSuccess || p->usbd->device == NULL) {
				a1loge(p->log, rv, "usb_open_port: QueryInterface '%s' failed with 0%x\n",p->name, rv);
				(*piif)->Release(piif);
				return ICOM_SYS;
			}
			(*piif)->Release(piif);		/* delete intermediate object */
	
			if ((rv = (*p->usbd->device)->USBDeviceOpenSeize(p->usbd->device)) != kIOReturnSuccess) {
				a1logd(p->log, 8, "usb_open_port: open '%s' config %d failed (0x%x) (Device being used ?)\n",p->name,config,rv);
				if (retries <= 0) {
					if (kpc != NULL)
						kpc->del(kpc); 
					a1loge(p->log, rv, "usb_open_port: open '%s' config %d failed (0x%x) (Device being used ?)\n",p->name, config, rv);
					(*p->usbd->device)->Release(p->usbd->device);
					return ICOM_SYS;
				}
				continue;
			} else if (p->debug)
				a1logd(p->log, 1, "usb_open_port: open port '%s' succeeded\n",p->name);

			p->uflags = usbflags;

			/* We're done retries */
			break;
		}

		if (kpc != NULL)
			kpc->del(kpc); 

		/* We should only do a set configuration if the device has more than one */
		/* possible configuration and it is currently not the desired configuration, */
		/* but we should avoid doing a set configuration if the OS has already */
		/* selected the configuration we want, since two set configs seem to */
		/* mess up the Spyder2, BUT we can't do a get config because this */
		/* messes up the i1pro-D. */

		/* OS X doesn't do a set_configuration() by default, so force one. */
		p->cconfig = 0;

		if (p->cconfig != config) {
			if ((rv = (*(p->usbd->device))->SetConfiguration(
				p->usbd->device, config)) != kIOReturnSuccess) {
				a1loge(p->log, rv, "usb_open_port: SetConfiguration failed with 0x%x\n",rv);
				cleanup_device(p);
				return ICOM_SYS;
			}
			p->cconfig = config;
			a1logd(p->log, 6, "usb_open_port: SetConfiguration %d OK\n",config);
		}

		/* Get interfaces */
		{
			int i, j;
			IOUSBFindInterfaceRequest req;
			io_iterator_t ioit;
			io_service_t iface;
			IOCFPlugInInterface **pluginref = NULL;
			SInt32 score;

			req.bInterfaceClass    = 
			req.bInterfaceSubClass = 
			req.bInterfaceProtocol = 
			req.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

			if ((rv = (*(p->usbd->device))->CreateInterfaceIterator(
				             p->usbd->device, &req, &ioit)) != kIOReturnSuccess) {
				a1loge(p->log, rv, "usb_open_port: CreateInterfaceIterator failed with 0x%x\n",rv);
				cleanup_device(p);
				return ICOM_SYS;
			}
			iface = IOIteratorNext(ioit);
			for (i = 0; iface != 0; i++, iface = IOIteratorNext(ioit)) {
				u_int8_t nep;

				if ((rv = IOCreatePlugInInterfaceForService(iface,
					     kIOUSBInterfaceUserClientTypeID,
					     kIOCFPlugInInterfaceID,
					     &pluginref, &score)) != kIOReturnSuccess) {
					a1loge(p->log, rv, "usb_open_port: IOCreatePlugInInterfaceForService failed with 0x%x\n",rv);
					IOObjectRelease(iface);
					IOObjectRelease(ioit);
					cleanup_device(p);
					return ICOM_SYS;
				}
				IOObjectRelease(iface);
				if ((rv = (*pluginref)->QueryInterface(pluginref,
					      CFUUIDGetUUIDBytes(InterfaceInterfaceID),
					      (void *)&p->usbd->interfaces[i])) != kIOReturnSuccess) {
					a1loge(p->log, rv, "usb_open_port: QueryInterface failed with 0x%x\n",rv);
					(*pluginref)->Stop(pluginref);
					IODestroyPlugInInterface(pluginref);
					IOObjectRelease(ioit);
					cleanup_device(p);
					return ICOM_SYS;
				}
				(*pluginref)->Stop(pluginref);
				IODestroyPlugInInterface(pluginref);	// this stops IOServices though ??

				if ((rv = (*(p->usbd->interfaces[i]))->USBInterfaceOpen(
					           p->usbd->interfaces[i])) != kIOReturnSuccess) {
					a1loge(p->log, rv, "usb_open_port: USBInterfaceOpen failed with 0x%x\n",rv);
					IOObjectRelease(ioit);
					cleanup_device(p);
					return ICOM_SYS;
				}

				/* Get the end point details, and set reference to pipe no and interfece ix */
				if ((rv = (*(p->usbd->interfaces[i]))->GetNumEndpoints(
					           p->usbd->interfaces[i], &nep)) != kIOReturnSuccess) {
					a1loge(p->log, rv, "usb_open_port: GetNumEndpoints failed with 0x%x\n",rv);
					IOObjectRelease(ioit);
					cleanup_device(p);
					return ICOM_SYS;
				}
				for (j = 0; j < nep; j++) {
					UInt8 direction, pnumber, transferType, interval;
					UInt16 maxPacketSize;
					int ad;
					if ((rv = (*(p->usbd->interfaces[i]))->GetPipeProperties(
						           p->usbd->interfaces[i], (UInt8)(j+1), &direction, &pnumber,
						           &transferType, &maxPacketSize, &interval)) != kIOReturnSuccess) {
						a1loge(p->log, rv, "usb_open_port: GetPipeProperties failed with 0x%x\n",rv);
						IOObjectRelease(ioit);
						cleanup_device(p);
						return ICOM_SYS;
					}
					ad = ((direction << IUSB_ENDPOINT_DIR_SHIFT) & IUSB_ENDPOINT_DIR_MASK)
					   | (pnumber & IUSB_ENDPOINT_NUM_MASK);
					p->EPINFO(ad).valid = 1;
					p->EPINFO(ad).addr = ad;
					p->EPINFO(ad).packetsize = maxPacketSize;
					p->EPINFO(ad).type = transferType & IUSB_ENDPOINT_TYPE_MASK;
					p->EPINFO(ad).interface = i;
					p->EPINFO(ad).pipe = j+1;
					a1logd(p->log, 6, "set ep ad 0x%x packetsize %d type %d, if %d, pipe %d\n",ad,maxPacketSize,transferType & IUSB_ENDPOINT_TYPE_MASK,i,j);
				}
			}
			p->usbd->nifce = i;		/* Just in case.. */

			IOObjectRelease(ioit);
		}
		{
			/* Setup the RunLoop thread */
			a1logd(p->log, 6, "usb_open_port: Starting RunLoop thread\n");
			if ((rv = pthread_create(&p->usbd->thread, NULL, io_runloop, (void*)p)) < 0) {
				a1loge(p->log, ICOM_SYS, "usb_open_port: creating RunLoop thread failed with %s\n",rv);
				cleanup_device(p);
				return ICOM_SYS;
			}

			/* Wait for the runloop thread to start and create a cfrunloop */
			pthread_mutex_lock(&p->usbd->lock);
			while (p->usbd->cfrunloop == NULL)
				pthread_cond_wait(&p->usbd->cond, &p->usbd->lock);
			pthread_mutex_unlock(&p->usbd->lock);
			if (p->usbd->thrv != kIOReturnSuccess) {	/* Thread failed */
				pthread_join(p->usbd->thread, NULL);
				cleanup_device(p);
				return ICOM_SYS;
			}
			CFRetain(p->usbd->cfrunloop);
			a1logd(p->log, 6, "usb_open_port: RunLoop thread started\n");
		}

		/* Clear any errors */
		/* (Some I/F seem to hang if we do this, some seem to hang if we don't !) */
		if (!(p->uflags & icomuf_no_open_clear)) {
			int i;
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

		p->is_open = 1;
		a1logd(p->log, 8, "usb_open_port: USB port is now open\n");
	}

	/* Install the cleanup signal handlers, and add to our cleanup list */
	usb_install_signal_handlers(p);

	return ICOM_OK;
}

/*  -------------------------------------------------------------- */

/* The run loop thread */
static void *io_runloop(void *context) {
	icoms *p = (icoms *)context;
	int i;

	/* Register this thread with the Objective-C garbage collector */
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
	 objc_registerThreadWithCollector();
#endif

	a1logd(p->log, 6, "io_runloop: thread started\n");

	p->usbd->cfrunloop = CFRunLoopGetCurrent();		/* Get this threads RunLoop */
	CFRetain(p->usbd->cfrunloop);

	/* Add a device event source */
	if ((p->usbd->thrv = (*(p->usbd->device))->CreateDeviceAsyncEventSource(
		              p->usbd->device, &p->usbd->cfsource)) != kIOReturnSuccess) {
		a1loge(p->log, p->usbd->thrv, "io_runloop: CreateDeviceAsyncEventSource failed with 0x%x\n",p->usbd->thrv);
	} else {
		CFRunLoopAddSource(p->usbd->cfrunloop, p->usbd->cfsource, kCFRunLoopDefaultMode);
	}

	/* Create an async event source for the interfaces */
	for (i = 0; p->usbd->thrv == kIOReturnSuccess && i < p->usbd->nifce; i++) {
		if ((p->usbd->thrv = (*(p->usbd->interfaces[i]))->CreateInterfaceAsyncEventSource(
			     p->usbd->interfaces[i], &p->usbd->cfsources[i])) != kIOReturnSuccess) {
			a1loge(p->log, p->usbd->thrv, "io_runloop: CreateInterfaceAsyncEventSource failed with 0x%x\n",p->usbd->thrv);
		} else {
			/* Add it to the RunLoop */
			CFRunLoopAddSource(p->usbd->cfrunloop, p->usbd->cfsources[i], kCFRunLoopDefaultMode);
		}
	}

	/* Signal main thread that we've started */
	pthread_mutex_lock(&p->usbd->lock);
	pthread_cond_signal(&p->usbd->cond);
	pthread_mutex_unlock(&p->usbd->lock);

	/* Run the loop, or exit on error */
	if (p->usbd->thrv == kIOReturnSuccess) {
		CFRunLoopRun();		/* Run the loop and deliver events */
	}

	/* Delete the interfaces async event sources */
	for (i = 0; i < p->usbd->nifce; i++) {
		if (p->usbd->cfsources[i] != NULL) {
			CFRunLoopRemoveSource(p->usbd->cfrunloop, p->usbd->cfsources[i], kCFRunLoopDefaultMode);
			CFRelease(p->usbd->cfsources[i]);
		}
	}

	/* Delete the devices event sources */
	if (p->usbd->cfsource != NULL) {
		CFRunLoopRemoveSource(p->usbd->cfrunloop, p->usbd->cfsource, kCFRunLoopDefaultMode);
		CFRelease(p->usbd->cfsource);
	}

	CFRelease(p->usbd->cfrunloop);

	a1logd(p->log, 6, "io_runloop: thread done\n");
	return NULL;
}

/* I/O structures */

typedef struct _usbio_req {
	icoms *p;
	int iix;					/* Interface index */
	UInt8 pno;					/* pipe index */
	volatile int done;			/* Done flag */
	pthread_mutex_t lock;		/* Protect req & callback access */
	pthread_cond_t cond;		/* Signal to thread waiting on req */
	int xlength;				/* Bytes transferred */
	IOReturn result;			/* Result of transaction */
} usbio_req;


/* Async completion callback - called by RunLoop thread */
static void io_callback(void *refcon, IOReturn result, void *arg0) {
	usbio_req *req = (usbio_req *)refcon;

	a1logd(req->p->log, 1, "io_callback: result 0x%x, length %d\n",result,(int)(long)arg0);

	req->xlength = (int)(long)arg0;
	req->result = result;
	req->done = 1;
	pthread_mutex_lock(&req->lock);
	pthread_cond_signal(&req->cond);
	pthread_mutex_unlock(&req->lock);
}

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
	int dirw = (endpoint & IUSB_ENDPOINT_DIR_MASK) == IUSB_ENDPOINT_OUT ? 1 : 0;
	usbio_req req;
	IOReturn result;
	int iix = p->EPINFO(endpoint).interface;
	UInt8 pno = (UInt8)p->EPINFO(endpoint).pipe;

	in_usb_rw++;

	a1logd(p->log, 8, "icoms_usb_transaction: req type 0x%x ep 0x%x size %d\n",ttype,endpoint,length);

	if (ttype != icom_usb_trantype_interrutpt
	 && ttype != icom_usb_trantype_bulk) {
		/* We only handle interrupt & bulk, not control */
		return ICOM_SYS;
	}

	req.p = p;
	req.iix = iix;
	req.pno = pno;
	req.xlength = 0;
	req.done = 0;
	pthread_mutex_init(&req.lock, NULL);
	pthread_cond_init(&req.cond, NULL);

	if (dirw)
		result = (*p->usbd->interfaces[iix])->WritePipeAsync(p->usbd->interfaces[iix], 
		                                  pno, buffer, length, io_callback, &req); 
	else
		result = (*p->usbd->interfaces[iix])->ReadPipeAsync(p->usbd->interfaces[iix], 
		                                  pno, buffer, length, io_callback, &req); 

	if (result != kIOReturnSuccess) {
		a1loge(p->log, ICOM_SYS, "icoms_usb_transaction: %sPipeAsync failed with 0x%x\n",dirw ? "Write" : "Read", result);
		reqrv = ICOM_SYS;
		goto done;
	}

	if (cancelt != NULL) {
		amutex_lock(cancelt->cmtx);
		cancelt->hcancel = (void *)&req;
		cancelt->state = 1;
		amutex_unlock(cancelt->cond);		/* Signal any thread waiting for IO start */
		amutex_unlock(cancelt->cmtx);
	}
				
	/* Wait for the callback to complete */
	pthread_mutex_lock(&req.lock);
	if (!req.done) {
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
					a1logd(p->log, 1, "coms_usb_transaction: pthread_cond_timedwait failed with %d\n",rv);
					(*p->usbd->interfaces[iix])->AbortPipe(p->usbd->interfaces[iix], pno);
					req.result = kIOReturnAborted;
					reqrv = ICOM_SYS;
					break;
				}

				/* Timed out */
				a1logd(p->log, 8, "coms_usb_transaction: time out - aborting io\n");
				(*p->usbd->interfaces[iix])->AbortPipe(p->usbd->interfaces[iix], pno);
				reqrv = ICOM_TO;
				/* Wait for the cancelled io to be signalled */
				if ((rv = pthread_cond_wait(&req.cond, &req.lock)) != 0) {
					pthread_mutex_unlock(&req.lock);
					a1logd(p->log, 1, "coms_usb_transaction:  pthread_cond_wait failed with %d\n",rv);
					req.result = kIOReturnAborted;
					reqrv = ICOM_SYS;
					break;
				}
				break;
			}
			if (req.done)	/* Ignore spurious wakeups */
				break;
		}
	}
	pthread_mutex_unlock(&req.lock);

	a1logd(p->log, 8, "coms_usb_transaction: completed with reqrv 0x%x and xlength %d\n",req.result,req.xlength);

	/* If io was aborted, ClearPipeStall */
	if (req.result == kIOReturnAborted) {
#if (InterfaceVersion > 182)
		(*p->usbd->interfaces[iix])->ClearPipeStallBothEnds(p->usbd->interfaces[iix], pno);
#else
		(*p->usbd->interfaces[iix])->ClearPipeStall(p->usbd->interfaces[iix], pno);
		icoms_usb_control_msg(p, NULL, IUSB_REQ_HOST_TO_DEV | IUSB_REQ_TYPE_STANDARD
		                       | IUSB_REQ_RECIP_ENDPOINT, IUSB_REQ_CLEAR_FEATURE,
		                         IUSB_FEATURE_EP_HALT, endpoint, NULL, 0, 200);
#endif
		if (reqrv == ICOM_OK)		/* If not aborted for a known reason, must be cancelled */
			reqrv = ICOM_CANC;
	}

	/* If normal completion - not timed out or aborted */
	if (reqrv == ICOM_OK) {		/* Completed OK */
		if (req.result == kIOReturnSuccess) {
			if (req.xlength != length)
				reqrv = ICOM_SHORT;
		} else {
			if (dirw)
				reqrv = ICOM_USBW;
			else
				reqrv = ICOM_USBR;
		} 
	}

	if (transferred != NULL)
		*transferred = req.xlength;

done:;
	if (cancelt != NULL) {
		amutex_lock(cancelt->cmtx);
		cancelt->hcancel = (void *)NULL;
		if (cancelt->state == 0)
			amutex_unlock(cancelt->cond);
		cancelt->state = 2;
		amutex_unlock(cancelt->cmtx);
	}

	pthread_cond_destroy(&req.cond);
	pthread_mutex_destroy(&req.lock);

	if (in_usb_rw < 0)
		exit(0);

	in_usb_rw--;

	a1logd(p->log, 8, "coms_usb_transaction: returning err 0x%x and %d bytes\n",reqrv, req.xlength);

	return reqrv;
}


/* Our control message routine */
/* Return error icom error code */
static int icoms_usb_control_msg(
icoms *p,
int *transferred,
int requesttype, int request,
int value, int index, unsigned char *bytes, int size, 
int timeout) {
	int reqrv = ICOM_OK;
	int dirw = (requesttype & IUSB_REQ_DIR_MASK) == IUSB_REQ_HOST_TO_DEV ? 1 : 0;
	IOReturn rv;
	IOUSBDevRequestTO req;

	a1logd(p->log, 8, "icoms_usb_control_msg: type 0x%x req 0x%x size %d\n",requesttype,request,size);
	bzero(&req, sizeof(req));
	req.bmRequestType = requesttype;
	req.bRequest = request;
	req.wValue = value;
	req.wIndex = index;
	req.wLength = size;
	req.pData = bytes;
	req.completionTimeout = timeout;
	req.noDataTimeout = timeout;

	if (transferred != NULL)
		*transferred = 0;

	if ((rv = (*(p->usbd->device))->DeviceRequestTO(p->usbd->device, &req)) != kIOReturnSuccess) {
		if (rv == kIOUSBTransactionTimeout) {
			reqrv = ICOM_TO;
		} else {
			if (dirw)
				reqrv = ICOM_USBW;
			else
				reqrv = ICOM_USBR;
		}
	} else {
		if (transferred != NULL)
			*transferred = req.wLenDone;
	}

	a1logd(p->log, 8, "icoms_usb_control_msg: returning err 0x%x and %d bytes\n",reqrv, transferred != NULL ? *transferred : -1);
	return reqrv;
}

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Time out error return value */

//#define USBIO_ERROR_TIMEOUT	-ETIMEDOUT

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Cancel i/o in another thread */
int icoms_usb_cancel_io(
	icoms *p,
	usb_cancelt *cancelt
) {
	int reqrv = ICOM_OK;

	amutex_lock(cancelt->cmtx);
	usbio_req *req = (usbio_req *)cancelt->hcancel;
	if (req != NULL) {
		IOReturn rv;

		if ((rv = (*p->usbd->interfaces[req->iix])->AbortPipe(
			         p->usbd->interfaces[req->iix], req->pno)) != kIOReturnSuccess) {
			reqrv = ICOM_USBW;
		}
	}
	amutex_unlock(cancelt->cmtx);

	return reqrv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Reset and end point data toggle to 0 */
int icoms_usb_resetep(
	icoms *p,
	int ep					/* End point address */
) {
	int reqrv = ICOM_OK;
	int iix = p->EPINFO(ep).interface;
	UInt8 pno = (UInt8)p->EPINFO(ep).pipe;
	IOReturn rv;

	if ((rv = (*p->usbd->interfaces[iix])->ResetPipe(
		         p->usbd->interfaces[iix], pno)) != kIOReturnSuccess) {
		a1logd(p->log, 1, "icoms_usb_resetep failed with 0x%x\n",rv);
		reqrv = ICOM_USBW;
	}
	return reqrv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Clear a halt on an end point */
int icoms_usb_clearhalt(
	icoms *p,
	int ep					/* End point address */
) {
	int reqrv = ICOM_OK;
	int iix = p->EPINFO(ep).interface;
	UInt8 pno = (UInt8)p->EPINFO(ep).pipe;
	IOReturn rv;
	int irv;

#if (InterfaceVersion > 182)
	if ((rv = (*p->usbd->interfaces[iix])->ClearPipeStallBothEnds(
		         p->usbd->interfaces[iix], pno)) != kIOReturnSuccess) {
		a1logd(p->log, 1, "icoms_usb_clearhalt failed with 0x%x\n",rv);
		reqrv = ICOM_USBW;
	}
#else
	if ((rv = (*p->usbd->interfaces[iix])->ClearPipeStall(
		         p->usbd->interfaces[iix], pno)) != kIOReturnSuccess) {
		a1logd(p->log, 1, "icoms_usb_clearhalt failed with 0x%x\n",rv);
		reqrv = ICOM_USBW;
	}
	if ((irv = icoms_usb_control_msg(p, NULL, IUSB_REQ_HOST_TO_DEV | IUSB_REQ_TYPE_STANDARD
	                       | IUSB_REQ_RECIP_ENDPOINT, IUSB_REQ_CLEAR_FEATURE,
	                         IUSB_FEATURE_EP_HALT, ep, NULL, 0, 200)) != ICOM_OK) {
		a1logd(p->log, 1, "icoms_usb_clearhalt far end failed with 0x%x\n",irv);
		reqrv = ICOM_USBW;
	}
#endif
	return reqrv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef NEVER
		{
			IOUSBFindInterfaceRequest req;
			io_iterator_t ioit;

			/* See if we can find any interfaces */
			req.bInterfaceClass    = 
			req.bInterfaceSubClass = 
			req.bInterfaceProtocol = 
			req.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

			if ((rv = (*(p->usbd->device))->CreateInterfaceIterator(
				             p->usbd->device, &req, &ioit)) != kIOReturnSuccess) {
				a1loge(p->log, rv, "usb_open_port: CreateInterfaceIterator failed with 0x%x\n",rv);
				cleanup_device(p);
				return ICOM_SYS;
			}
		    if (IOIteratorNext(ioit) == 0) {	/* Configure the device */
				IOUSBConfigurationDescriptorPtr confdesc;

				if ((rv = (*(p->usbd->device))->GetConfigurationDescriptorPtr(
					          p->usbd->device, config-1, &confdesc)) != kIOReturnSuccess) {
					a1loge(p->log, rv, "usb_open_port: GetConfigurationDescriptorPtr failed with 0x%x\n",rv);
					IOObjectRelease(ioit);
					cleanup_device(p);
					return ICOM_SYS;
				}
				if ((rv = (*(p->usbd->device))->SetConfiguration(
					p->usbd->device, confdesc->bConfigurationValue)) != kIOReturnSuccess) {
					a1loge(p->log, rv, "usb_open_port: SetConfiguration failed with 0x%x\n",rv);
					IOObjectRelease(ioit);
					cleanup_device(p);
					return ICOM_SYS;
				}
				a1logd(p->log, 6, "usb_open_port: SetConfiguration %d OK\n",confdesc->bConfigurationValue);

			} else {	/* Some diagnostics */
				UInt8 confno;
				if ((rv = (*(p->usbd->device))->GetConfiguration(
					          p->usbd->device, &confno)) != kIOReturnSuccess) {
					a1logd(p->log, 6, "usb_open_port: GetConfiguration failed with 0x%x\n",rv);
				} else {
					a1logd(p->log, 6, "usb_open_port: Device didn't need configuring - currently %d\n",confno);
				}
			}
			IOObjectRelease(ioit);

		}
#endif /* NEVER */
