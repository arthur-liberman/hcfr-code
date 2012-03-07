
 /* General USB HID I/O support */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   2007/10/10
 *
 * Copyright 2006 - 2007 Graeme W. Gill
 * All rights reserved.
 *
 * (Based on usbio.c)
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* These routines supliement the class code in ntio.c and unixio.c */
/* with HID specific access routines for devices running on operating */
/* systems where this is desirable. */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#if defined(UNIX)
#include <termios.h>
#include <errno.h>
#include <dirent.h>
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

#ifdef USE_LIBUSB1
# include "libusb.h"
#else
# include "usb.h"
#endif
#include "hidio.h"

#if defined(NT)
#include <setupapi.h>
#endif

#if defined(UNIX) && !defined(__APPLE__)
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/types.h> 
#include <usbhid.h> 
#else	/* assume Linux */ 
#include <asm/types.h>
#include <linux/hiddev.h>
#endif
#ifndef HID_MAX_USAGES		/* Workaround Linux Bug ? */
# define HID_MAX_USAGES 1024
#endif
#endif

#undef DEBUG


#if defined(DEBUG)
#define DBG(xxx) fprintf xxx ;
#define dbgo stderr
#else
#define DBG(xxx) 
#endif	/* DEBUG */

#if defined(NT)

/* Declartions to enable HID access without using the DDK */

typedef struct _HIDD_ATTRIBUTES {
	ULONG	Size;
	USHORT	VendorID;
	USHORT	ProductID;
	USHORT	VersionNumber;
} HIDD_ATTRIBUTES, *PHIDD_ATTRIBUTES;

void (WINAPI *pHidD_GetHidGuid)(OUT LPGUID HidGuid) = NULL;
BOOL (WINAPI *pHidD_GetAttributes)(IN HANDLE HidDeviceObject, OUT PHIDD_ATTRIBUTES Attributes) = NULL;

/* See if we can get the wanted function calls */
/* Return nz if OK */
static int setup_dyn_calls() {
	static int dyn_inited = 0;

	if (dyn_inited == 0) {
		dyn_inited = 1;

		pHidD_GetHidGuid = (void (WINAPI*)(LPGUID))
		                   GetProcAddress(LoadLibrary("HID"), "HidD_GetHidGuid");
		pHidD_GetAttributes = (BOOL (WINAPI*)(HANDLE, PHIDD_ATTRIBUTES))
		                      GetProcAddress(LoadLibrary("HID"), "HidD_GetAttributes");

		if (pHidD_GetHidGuid == NULL
		|| pHidD_GetAttributes == NULL)
			dyn_inited = 0;
	}
	return dyn_inited;
}

#endif


/* Add paths to USB connected instruments, to the existing */
/* icompath paths in the icoms structure. */
void hid_get_paths(
struct _icoms *p 
) {
#ifdef ENABLE_USB

#if defined(NT)
	{
		GUID HidGuid;
		HDEVINFO hdinfo;
		SP_DEVICE_INTERFACE_DATA did;
#define DIDD_BUFSIZE sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA) + (sizeof(TCHAR)*MAX_PATH)
		char *buf[DIDD_BUFSIZE];
		PSP_DEVICE_INTERFACE_DETAIL_DATA pdidd = (PSP_DEVICE_INTERFACE_DETAIL_DATA)buf;
		SP_DEVINFO_DATA dinfod;
		int i;
		unsigned short VendorID = 0, ProductID = 0;
	
		/* Make sure we've dynamically linked */
		if (setup_dyn_calls() == 0) {
        	error("Dynamic linking to hid.dll failed");
		}

		/* Get the device interface GUID for HIDClass devices */
		(*pHidD_GetHidGuid)(&HidGuid);

		/* Return device information for all devices of the HID class */
		hdinfo = SetupDiGetClassDevs(&HidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE); 
		if (hdinfo == INVALID_HANDLE_VALUE)
        	error("SetupDiGetClassDevs failed");
	
		/* Get each devices interface data in turn */
		did.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		pdidd = (PSP_DEVICE_INTERFACE_DETAIL_DATA)buf;  
		pdidd->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		dinfod.cbSize = sizeof(SP_DEVINFO_DATA);
		for (i = 0; ; i++) {
			instType itype;

			if (SetupDiEnumDeviceInterfaces(hdinfo, NULL, &HidGuid, i, &did) == 0) {
				if (GetLastError() == ERROR_NO_MORE_ITEMS) {
					break;
				}
				error("SetupDiEnumDeviceInterfaces failed");
			}
			if (SetupDiGetDeviceInterfaceDetail(hdinfo, &did, pdidd, DIDD_BUFSIZE, NULL, &dinfod)
			                                                                                == 0)
				error("SetupDiGetDeviceInterfaceDetail failed");

			/* Extract the vid and pid from the device path */
			{
				int gotid;
				char *cp, buf[20];

				for(gotid = 0;;) {
					if ((cp = strchr(pdidd->DevicePath, 'v')) == NULL)
						break;
					if (strlen(cp) < 8)
						break;
					if (cp[1] != 'i' || cp[2] != 'd' || cp[3] != '_')
						break;
					memcpy(buf, cp + 4, 4);
					buf[4] = '\000';
					if (sscanf(buf, "%hx", &VendorID) != 1)
						break;
					if ((cp = strchr(pdidd->DevicePath, 'p')) == NULL)
						break;
					if (strlen(cp) < 8)
						break;
					if (cp[1] != 'i' || cp[2] != 'd' || cp[3] != '_')
						break;
					memcpy(buf, cp + 4, 4);
					buf[4] = '\000';
					if (sscanf(buf, "%hx", &ProductID) != 1)
						break;
					gotid = 1;
					break;
				}
				if (!gotid) {
					if (p->debug) fprintf(stderr,"found HID device '%s', inst %d but unable get PID and VID\n",pdidd->DevicePath, dinfod.DevInst);
					continue;
				}
			}

			/* If it's a device we're looking for */
			if ((itype = inst_usb_match(VendorID, ProductID)) != instUnknown) {
				char pname[400];
		
				if (p->debug) fprintf(stderr,"found HID device '%s', inst %d that we're looking for\n",pdidd->DevicePath, dinfod.DevInst);

				/* Create human readable path/identification */
				sprintf(pname,"hid:/%d (%s)", dinfod.DevInst, inst_name(itype));
		
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
				if ((p->paths[p->npaths] = calloc(sizeof(icompath), 1)) == NULL)
					error("icoms: calloc failed!");
				p->paths[p->npaths]->vid = VendorID;
				p->paths[p->npaths]->pid = ProductID;
				p->paths[p->npaths]->dev = NULL;		/* Make sure it's NULL */
				if ((p->paths[p->npaths]->hev = (hid_device *)calloc(sizeof(hid_device), 1))
				                                                                     == NULL)
					error("icoms: calloc failed!");
				if ((p->paths[p->npaths]->hev->dpath = strdup(pdidd->DevicePath)) == NULL)
					error("icoms: calloc failed!");
				p->paths[p->npaths]->itype = itype;
				if ((p->paths[p->npaths]->path = strdup(pname)) == NULL)
					error("icoms: strdup failed!");
				p->npaths++;
				p->paths[p->npaths] = NULL;
			} else {
				if (p->debug) fprintf(stderr,"found HID device '%s', inst %d but not one we're looking for\n",pdidd->DevicePath, dinfod.DevInst);
			}
		}

		/* Now we're done with the hdifo */
		if (SetupDiDestroyDeviceInfoList(hdinfo) == 0)
        	error("SetupDiDestroyDeviceInfoList failed");

	}
#endif /* NT */

#ifdef __APPLE__
	/* On OS X the general USB driver will locate the HID devices, but */
	/* not be able to claim the interfaces without using a kext to stop */
	/* the HID driver grabing the interfaces. We switch them from libusb */
	/* access to OS X HID driver access to avoid the problem. */
	{
	    kern_return_t kstat; 
	    CFMutableDictionaryRef sdict;		/* HID Device  dictionary */
		io_iterator_t mit;					/* Matching itterator */

		/* Get dictionary of HID devices */
    	if ((sdict = IOServiceMatching(kIOHIDDeviceKey)) == NULL) {
        	warning("IOServiceMatching returned a NULL dictionary");
		}

		/* Init itterator to find matching types */
		if ((kstat = IOServiceGetMatchingServices(kIOMasterPortDefault, sdict, &mit))
			                                                          != KERN_SUCCESS) 
        	error("IOServiceGetMatchingServices returned %d", kstat);

		/* Find all the matching HID devices */
		for (;;) {
			io_object_t ioob;						/* HID object found */
			CFNumberRef vref, pref;					/* HID Vendor and Product ID propeties */
			unsigned int vid = 0, pid = 0;
			instType itype;

		    if ((ioob = IOIteratorNext(mit)) == 0)
				break;

			/* Get the two properies we need. [ Doing IORegistryEntryCreateCFProperty() is much faster */
			/* than IORegistryEntryCreateCFProperties() in some cases.] */
			if ((vref = IORegistryEntryCreateCFProperty(ioob, CFSTR(kIOHIDVendorIDKey),
			                                         kCFAllocatorDefault,kNilOptions)) != 0) {
				CFNumberGetValue(vref, kCFNumberIntType, &vid);
			    CFRelease(vref);
			}
			if ((pref = IORegistryEntryCreateCFProperty(ioob, CFSTR(kIOHIDProductIDKey),
			                                         kCFAllocatorDefault,kNilOptions)) != 0) {
				CFNumberGetValue(pref, kCFNumberIntType, &pid);
			    CFRelease(pref);
			}

			/* If it's a device we're looking for */
			if ((itype = inst_usb_match(vid, pid)) != instUnknown) {
				char pname[400];
				if (p->debug) fprintf(stderr,"found HID device '%s' that we're looking for\n",inst_name(itype));

				/* Create human readable path/identification */
				/* (There seems to be no easy way of creating an interface no, without */
				/*  heroic efforts looking through the IO registry, so don't try.) */
				sprintf(pname,"hid: (%s)", inst_name(itype));
		
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
				if ((p->paths[p->npaths] = calloc(sizeof(icompath), 1)) == NULL)
					error("icoms: calloc failed!");
				p->paths[p->npaths]->vid = vid;
				p->paths[p->npaths]->pid = pid;
				p->paths[p->npaths]->dev = NULL;		/* Make sure it's NULL */
				if ((p->paths[p->npaths]->hev = (hid_device *)calloc(sizeof(hid_device), 1))
				                                                                     == NULL)
					error("icoms: calloc failed!");
				p->paths[p->npaths]->hev->ioob = ioob;
				ioob = 0;			/* Don't release it */
				p->paths[p->npaths]->itype = itype;
				if ((p->paths[p->npaths]->path = strdup(pname)) == NULL)
					error("icoms: strdup failed!");
				p->npaths++;
				p->paths[p->npaths] = NULL;
			}
			if (ioob != 0)		/* If we haven't kept it */
			    IOObjectRelease(ioob);		/* Release found object */
		}
	    IOObjectRelease(mit);			/* Release the itterator */
	}
#endif /* __APPLE__ */

#if defined(UNIX) && !defined(__APPLE__)

	/* This is how we'd go about adding HID support for Linux, IF it */
	/* was actually capable of communicating application composed reports - */
	/* which it is not, so HID seems pretty busted on Linux.. */
#ifdef NEVER
	/* We need to scan for /dev/hiddev* or /dev/usb/hiddev* device names, */
	/* and then read their vid & pid */
	{
		int i;
		char *devds[] = {		/* Typical locations hiddev will appear under */
				"/dev",
				"/dev/usb",
				""
		};

		/* For each possible device directory */
		for (i = 0;;i++) {
			DIR *dir;
			struct dirent *dentry;

			if (devds[i][0] == '\000')
				break;
			if ((dir = opendir(devds[i])) == NULL)
				continue;
			while ((dentry = readdir(dir)) != NULL) {
				if (strncmp(dentry->d_name, "hiddev", 6) == 0) {
					char dpath[PATH_MAX];
					int fd;
					
					strcpy(dpath, devds[i]);
					strcat(dpath, "/");
					strcat(dpath, dentry->d_name);
//printf("~1 found hid device '%s'\n",dpath);
					/* Open it and see what VID and PID it is */
					if ((fd = open(dpath, O_RDONLY)) >= 0) {
						struct hiddev_devinfo hidinfo;
						if (ioctl(fd, HIDIOCGDEVINFO, &hidinfo) < 0)
							warning("Unable to get HID info for '%s'",dpath);
						else {
//printf("~1 busnum = %d, devnum = %d, vid = 0x%x, pid = 0x%x\n",
//hidinfo.busnum, hidinfo.devnum, hidinfo.vendor, hidinfo.product);
						}
						close(fd);
					}
// More hotplug/udev magic needed to make the device accesible !
//else printf("~1 failed to open '%s' err %d\n",dpath,fd);
				}
			}
			closedir(dir);
		}
	}
#endif /* NEVER */
#endif /* UNIX & ! APPLE */

#endif /* ENABLE_USB */
}

/* Cleanup and then free an hev entry */
void hid_del_hid_device(hid_device *hev) {

	if (hev == NULL)
		return;

#if defined(NT)
	if (hev->dpath != NULL)
		free(hev->dpath);
#endif /* NT */
#ifdef __APPLE__
	if (hev->ioob != 0)
	    IOObjectRelease(hev->ioob);		/* Release found object */
#endif /* __APPLE__ */

	free(hev);
}

/* Return the instrument type if the port number is HID, */
/* and instUnknown if it is not. */
instType hid_is_hid_portno(
	icoms *p, 
	int port		/* Enumerated port number, 1..n */
) {
	
	if (p->paths == NULL)
		p->get_paths(p);

	if (port <= 0 || port > p->npaths)
		error("icoms - set_ser_port: port number out of range!");

#ifdef ENABLE_USB
	if (p->paths[port-1]->hev != NULL)
		return p->paths[port-1]->itype;
#endif /* ENABLE_USB */

	return instUnknown;
}

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Close an open HID port */
/* If we don't do this, the port and/or the device may be left in an unusable state. */
void hid_close_port(icoms *p) {

#ifdef ENABLE_USB
	if (p->debug) fprintf(stderr,"hid_close_port() called\n");

	if (p->is_open && p->hidd != NULL) {

#if defined(NT) 
		CloseHandle(p->hidd->ols.hEvent);
		CloseHandle(p->hidd->fh);
#endif /* NT */

#ifdef __APPLE__
	    IOObjectRelease(p->hidd->port);
		p->hidd->port = 0;

		CFRunLoopRemoveSource(p->hidd->rlr, p->hidd->evsrc, kCFRunLoopDefaultMode);

		CFRelease(p->hidd->evsrc);
		p->hidd->evsrc = NULL;

		p->hidd->rlr = NULL;

		if ((*p->hidd->device)->close(p->hidd->device) != kIOReturnSuccess)
			error("Closing HID port '%s' failed",p->ppath->path);

		if ((*p->hidd->device)->Release(p->hidd->device) != kIOReturnSuccess)
			warning("Releasing HID port '%s' failed",p->ppath->path);
		p->hidd->device = NULL;
#endif /* __APPLE__ */

		p->is_open = 0;
		if (p->debug) fprintf(stderr,"hid port has been released and closed\n");
	}

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


/* Open an HID port for all our uses. */
static void hid_open_port(
icoms *p,
int    port,			/* USB com port, 1 - N, 0 for no change. */
icomuflags hidflags,	/* Any special handling flags */
int retries,			/* > 0 if we should retry set_configuration (100msec) */
char **pnames			/* List of process names to try and kill before opening */
) {
	if (p->debug) fprintf(stderr,"icoms: About to open the USB port\n");

	if (port >= 1) {
		if (p->is_open && port != p->port) {	/* If port number changes */
			p->close_port(p);
		}
	}

	/* Make sure the port is open */
	if (!p->is_open) {
		if (p->debug) fprintf(stderr,"icoms: HID port needs opening\n");

		if (p->ppath != NULL) {
			if (p->ppath->path != NULL)
				free(p->ppath->path);
			free(p->ppath);
		}

		if (p->paths == NULL)
			p->get_paths(p);

		if (port <= 0 || port > p->npaths)
			error("icoms - hid_open_port: port number out of range!");

		if (p->paths[port-1]->hev == NULL)
			error("icoms - hid_open_port: Not an HID port!");

		if ((p->ppath = calloc(sizeof(icompath), 1)) == NULL)
			error("calloc() failed on com port path");
		*p->ppath = *p->paths[port-1];				/* Structure copy */
		if ((p->ppath->path = strdup(p->paths[port-1]->path)) == NULL)
			error("strdup() failed on com port path");
		p->port = port;

		if (p->debug) fprintf(stderr,"icoms: About to open HID port '%s'\n",p->ppath->path);

		p->vid = p->ppath->vid;
		p->pid = p->ppath->pid;
		p->hidd = p->ppath->hev;		/* A more convenient copy */
		p->uflags = hidflags;

#if defined(NT) 
		{
			int tries = 0;

			for (tries = 0; retries >= 0; retries--, tries++) {
				/* Open the device */
  				if ((p->hidd->fh = CreateFile(p->hidd->dpath, GENERIC_READ|GENERIC_WRITE,
				      0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL))
				                                              != INVALID_HANDLE_VALUE) {
					memset(&p->hidd->ols,0,sizeof(OVERLAPPED));
  					if ((p->hidd->ols.hEvent = CreateEvent(NULL, 0, 0, NULL)) == NULL)
						error("Failed to create HID Event'\n");
					break;
				}
				if (tries > 0 && pnames != NULL) {
					/* Open failed. This could be the i1ProfileTray.exe */
					kill_nprocess(pnames, p->debug);
					msec_sleep(100);
				}
			}
			if (p->hidd->fh == INVALID_HANDLE_VALUE)
				error("Failed to open HID device '%s'\n",p->ppath->path);
		}
#endif /* NT */

#ifdef __APPLE__
		{
			IOCFPlugInInterface **piif = NULL;
			IOReturn result;
			SInt32 score;

			if ((result = IOCreatePlugInInterfaceForService(p->hidd->ioob, kIOHIDDeviceUserClientTypeID,
			    kIOCFPlugInInterfaceID, &piif, &score) != kIOReturnSuccess || piif == NULL))
				error("Failed to open HID device '%s', result 0x%x, piif 0x%x\n",p->ppath->path,result,piif);
	
			p->hidd->device = NULL;
			if ((*piif)->QueryInterface(piif,
				    CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID122), (LPVOID)&p->hidd->device)
			        != kIOReturnSuccess || p->hidd->device == NULL)
				error("Getting HID device '%s' failed",p->ppath->path);
			(*piif)->Release(piif);		/* delete intermediate object */

			if ((*p->hidd->device)->open(p->hidd->device, kIOHIDOptionsTypeSeizeDevice)
			                                                       != kIOReturnSuccess)
				error("Opening HID device '%s' failed",p->ppath->path);

			/* Setup to handle interrupt read callbacks */
			p->hidd->port = 0;
			if ((*(p->hidd->device))->createAsyncPort(p->hidd->device, &p->hidd->port)
			        != kIOReturnSuccess || p->hidd->port == 0)
				error("Creating port on HID device '%s' failed",p->ppath->path);

			p->hidd->evsrc = NULL;
			if ((*(p->hidd->device))->createAsyncEventSource(p->hidd->device, &p->hidd->evsrc)
			        != kIOReturnSuccess || p->hidd->evsrc == NULL)
				error("Creating event source on HID device '%s' failed",p->ppath->path);

//			p->hidd->rlr = CFRetain(CFRunLoopGetCurrent());
			p->hidd->rlr = CFRunLoopGetCurrent();

			CFRunLoopAddSource(p->hidd->rlr, p->hidd->evsrc, kCFRunLoopDefaultMode);

		}
#endif /* __APPLE__ */

		p->is_usb = 0;
		p->is_hid = 1;
		p->is_open = 1;
		if (p->debug) fprintf(stderr,"icoms: HID port is now open\n");
	}

	/* Install the cleanup signal handlers, and add to our cleanup list */
	usb_install_signal_handlers(p);
}

/* ========================================================= */

#ifdef __APPLE__

/* HID Interrupt callback for OS X */
static void hid_read_callback(
void *target,
IOReturn result,
void *refcon,
void *sender,
uint32_t size) {
	icoms *p = (icoms *)target;

//printf("\n~1 callback called with size %d, result 0x%x\n",size,result);
	p->hidd->result = result;
	p->hidd->bread = size;
	CFRunLoopStop(p->hidd->rlr);		/* We're done */
}

#endif /* __APPLE__ */

/* HID Interrupt pipe Read  - thread friendly */
/* Return error code (don't set error state). */
/* Don't retry on a short read, return ICOM_SHORT. */
/* [Probably uses the control pipe. We need a different */
/*  call for reading messages from the interrupt pipe] */
static int
icoms_hid_read_th(icoms *p,
	unsigned char *rbuf,	/* Read buffer */
	int bsize,				/* Bytes to read */
	int *breadp,			/* Bytes read */
	double tout,			/* Timeout in seconds */
	int debug,				/* debug flag value */
	int *cut,				/* Character that caused termination */
	int checkabort			/* Check for abort from keyboard */
) {
	int lerr = 0;				/* Last error */
	int bread = 0;

	if (!p->is_open)
		error("icoms_hid_read: not initialised");

#if defined(NT)
	{
		unsigned char *rbuf2;

		/* Create a copy of the data recieved with one more byte */
		if ((rbuf2 = malloc(bsize + 1)) == NULL)
			error("icoms_hid_read, malloc failed");
		rbuf2[0] = 0;
		if (ReadFile(p->hidd->fh, rbuf2, bsize+1, (LPDWORD)&bread, &p->hidd->ols) == 0)  {
			if (GetLastError() != ERROR_IO_PENDING) {
				lerr = ICOM_USBR; 
			} else {
				int res;
				res = WaitForSingleObject(p->hidd->ols.hEvent, (int)(tout * 1000.0 + 0.5));
				if (res == WAIT_FAILED)
					error("HID wait on read failed");
				else if (res == WAIT_TIMEOUT) {
					CancelIo(p->hidd->fh);
					lerr = ICOM_TO; 
					bread = 0;
				} else {
					bread = p->hidd->ols.InternalHigh;
				}
			}
		}
		if (bread > 0)
			bread--;
		memmove(rbuf,rbuf2+1,bsize);
		free(rbuf2);
	}
#endif /* NT */

#ifdef ENABLE_USB
#ifdef __APPLE__
	{
		IOReturn result;

		/* Setup for callback */
		p->hidd->result = -1;
		p->hidd->bread = 0;

		if ((*(p->hidd->device))->setInterruptReportHandlerCallback(p->hidd->device,
			rbuf, bsize, hid_read_callback, (void *)p, NULL) != kIOReturnSuccess)
			error("Setting callback handler for HID '%s' failed", p->ppath->path);
		if ((*(p->hidd->device))->startAllQueues(p->hidd->device) != kIOReturnSuccess)
			error("Starting queues for HID '%s' failed", p->ppath->path);
		/* Call runloop, but exit after handling one callback */
		result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, tout, false);
		if ((*(p->hidd->device))->stopAllQueues(p->hidd->device) != kIOReturnSuccess)
			error("Stopping queues for HID '%s' failed", p->ppath->path);
		if (result == kCFRunLoopRunTimedOut) {
			lerr = ICOM_TO; 
		} else if (result != kCFRunLoopRunStopped) {
			lerr = ICOM_USBR; 
		}
		if (p->hidd->result == -1) {		/* Callback wasn't called */
			lerr = ICOM_TO; 
		} else if (p->hidd->result != kIOReturnSuccess) {
			error("Callback for HID '%s' got unexpected return value", p->ppath->path);
		}
		bread = p->hidd->bread;
	}
#endif /* __APPLE__ */

	if (checkabort) {		/* Check for user abort */
		int c;
		if ((c = poll_con_char()) != 0 && p->uih[c] != ICOM_OK) {
//printf("~1 got for user abort with char 0x%x, code 0x%x\n",c,p->uih[c]);
			*cut = c;
			lerr |= p->uih[c];
		}
	}

	if (breadp != NULL)
		*breadp = bread;

#else /* !ENABLE_USB */
	lerr = ICOM_NOTS;
#endif /* !ENABLE_USB */

	if (debug) fprintf(stderr,"icoms: About to return hid read %d bytes, ICOM err 0x%x\n",bread, lerr);

	return lerr;
}

/* HID Read */
/* Same as above, but set error state */
static int
icoms_hid_read(
	icoms *p,
	unsigned char *rbuf,	/* Read buffer */
	int rsize,				/* Bytes to read */
	int *bread,				/* Bytes read */
	double tout				/* Timeout in seconds */
) {
	p->lerr = icoms_hid_read_th(p, rbuf, rsize, bread, tout, p->debug, &p->cut, 1);

	return p->lerr;
}

/* - - - - - - - - - - - - - */

/* HID Command Pipe Write  - thread friendly */
/* Return error code (don't set error state). */
/* Don't retry on a short read, return ICOM_SHORT. */
static int
icoms_hid_write_th(icoms *p,
	unsigned char *wbuf,	/* Write buffer */
	int bsize,				/* Bytes to write */
	int *bwrittenp,			/* Bytes written */
	double tout,			/* Timeout in seconds */
	int debug,				/* debug flag value */
	int *cut,				/* Character that caused termination */
	int checkabort			/* Check for abort from keyboard */
) {
	int lerr = 0;				/* Last error */
	int bwritten = 0;

	if (!p->is_open)
		error("icoms_hid_write: not initialised");

#ifdef ENABLE_USB

#if defined(NT)
	{
		unsigned char *wbuf2;

		/* Create a copy of the data to send with one more byte */
		if ((wbuf2 = malloc(bsize + 1)) == NULL)
			error("icoms_hid_write, malloc failed");
		memmove(wbuf2+1,wbuf,bsize);
		wbuf2[0] = 0;		/* Extra report ID byte */
		if (WriteFile(p->hidd->fh, wbuf2, bsize+1, (LPDWORD)&bwritten, &p->hidd->ols) == 0) { 
			if (GetLastError() != ERROR_IO_PENDING) {
				lerr = ICOM_USBW; 
			} else {
				int res;
				res = WaitForSingleObject(p->hidd->ols.hEvent, (int)(tout * 1000.0 + 0.5));
				if (res == WAIT_FAILED)
					error("HID wait on write failed");
				else if (res == WAIT_TIMEOUT) {
					CancelIo(p->hidd->fh);
					lerr = ICOM_TO; 
					bwritten = 0;
				} else {
					bwritten = p->hidd->ols.InternalHigh;
				}
			}
		}
		if (bwritten > 0)
			bwritten--;

		free(wbuf2);
	}
#endif /* NT */

#ifdef __APPLE__
	{
		IOReturn result;
		if ((result = (*p->hidd->device)->setReport(
			p->hidd->device,
		    kIOHIDReportTypeOutput, 
			9, 
			(void *)wbuf, 
			bsize, 
			(unsigned int)(tout * 1000.0 + 0.5), 
			NULL, NULL, NULL)) != kIOReturnSuccess) {
			/* (We could detect other error codes and translate them to ICOM) */
			if (result == kIOReturnTimeout)
				lerr = ICOM_TO; 
			else
				lerr = ICOM_USBW; 
		} else {
			bwritten = bsize;
		}
	}
#endif /* __APPLE__ */

	if (checkabort) {	/* Check for user abort */
		int c;
		if ((c = poll_con_char()) != 0 && p->uih[c] != ICOM_OK) {
//printf("~1 got for user abort with char 0x%x, code 0x%x\n",c,p->uih[c]);
			*cut = c;
			lerr |= p->uih[c];
		}
	}

	if (bwrittenp != NULL)
		*bwrittenp = bwritten;

#else /* !ENABLE_USB */
	lerr = ICOM_NOTS;
#endif /* !ENABLE_USB */

	if (debug) fprintf(stderr,"icoms: About to return hid write %d bytes, ICOM err 0x%x\n",bwritten, lerr);

	return lerr;
}

/* HID Write */
/* Same as above, but set error state */
static int
icoms_hid_write(
	icoms *p,
	unsigned char *wbuf,	/* Read buffer */
	int wsize,				/* Bytes to write */
	int *bwritten,			/* Bytes written */
	double tout				/* Timeout in seconds */
) {
	p->lerr = icoms_hid_write_th(p, wbuf, wsize, bwritten, tout, p->debug, &p->cut, 1);
	return p->lerr;
}


/* ------------------------------------------------- */
/* Set the hid port number and characteristics. */
/* This may be called to re-establish a connection that has failed */
static void
icoms_set_hid_port(
icoms *p, 
int    port,			/* HID com port, 1 - N, 0 for no change. */
icomuflags hidflags,	/* Any special handling flags */
int retries,			/* > 0 if we should retry set_configuration (100msec) */
char **pnames			/* List of process names to try and kill before opening */
) {
	if (p->debug) fprintf(stderr,"icoms: About to set hid port characteristics\n");

	if (p->is_open) 
		p->close_port(p);

	if (p->is_hid_portno(p, port) != instUnknown) {

		hid_open_port(p, port, hidflags, retries, pnames);

		p->write = NULL;
		p->read = NULL;

	}
	if (p->debug) fprintf(stderr,"icoms: hid port characteristics set ok\n");

	// ~~~999
//	usb_set_debug(8);
}

/* ---------------------------------------------------------------------------------*/

/* Set the HID specific icoms methods */
void hid_set_hid_methods(
icoms *p
) {
	p->is_hid_portno  = hid_is_hid_portno;
	p->set_hid_port   = icoms_set_hid_port;
	p->hid_read       = icoms_hid_read;
	p->hid_write      = icoms_hid_write;

	p->reset_uih(p);
}

/* ---------------------------------------------------------------------------------*/
