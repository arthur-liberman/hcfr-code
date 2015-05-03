
 /* General USB HID I/O support */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   2007/10/10
 *
 * Copyright 2006 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * (Based on usbio.c)
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/*
 * TTBD:
 *		As usual, Apple seem to have deprecated the routines used here,
 *      and reccommend a new API for 10.5 and latter, so we should
 *      switch to using this if compiled on 10.6 or latter.
 *		See IOHIDDeviceGetReportWithCallback & IOHIDDeviceSetReportWithCallback
 *		<https://developer.apple.com/library/mac/technotes/tn2187/_index.html>
 *		for info on the replacements.
 */

/*
    New 10.5 and later code is not completely developed/debugged
     - the read and write routines simply error out.

	~~ It turns out that doing CFRetain() on the CFRunLoopRef and CFRunLoopSourceRef
	   is important (see usbio_ox.c). Make sure that is correct.
	
    Perhaps we should try using

        IOHIDDeviceRegisterInputReportCallback()
    +   Handle_IOHIDDeviceIOHIDReportCallback()
    +   what triggers it ?? How is this related to GetReport ?

    +   OHIDDeviceGetReportWithCallback()
    +   Handle_IOHIDDeviceGetReportCallback()

    +   IOHIDDeviceSetReportWithCallback()
    +   Handle_IOHIDDeviceSetReportCallback()

    Plus IOHIDQueueScheduleWithRunLoop()
         IOHIDQueueUnscheduleFromRunLoop()

    + we probably have to call the run loop ?

*/
#undef USE_NEW_OSX_CODE

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
#else
#include "sa_config.h"
#endif
#include "numsup.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"

#if defined(NT)
#include <setupapi.h>
#endif

#if defined(UNIX_X11)
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__OpenBSD__)
#include <sys/types.h> 
#include <usbhid.h> 
#else	/* assume Linux */ 
# include <asm/types.h>
# include <linux/hiddev.h>
#endif
#ifndef HID_MAX_USAGES		/* Workaround Linux Bug ? */
# define HID_MAX_USAGES 1024
#endif
#endif

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
/* return com error */
int hid_get_paths(icompaths *p) {

	a1logd(p->log, 8, "hid_get_paths: called\n");

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
		unsigned int VendorID = 0, ProductID = 0;
	
		/* Make sure we've dynamically linked */
		if (setup_dyn_calls() == 0) {
        	a1loge(p->log, ICOM_SYS, "hid_get_paths() Dynamic linking to hid.dll failed\n");
			return ICOM_SYS;
		}

		/* Get the device interface GUID for HIDClass devices */
		(*pHidD_GetHidGuid)(&HidGuid);

		/* Return device information for all devices of the HID class */
		hdinfo = SetupDiGetClassDevs(&HidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE); 
		if (hdinfo == INVALID_HANDLE_VALUE) {
        	a1loge(p->log, ICOM_SYS, "hid_get_paths() SetupDiGetClassDevs failed\n");
			return ICOM_SYS;
		}
	
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
	        	a1loge(p->log, ICOM_SYS, "hid_get_paths() SetupDiEnumDeviceInterfaces failed\n");
				return ICOM_SYS;
			}
			if (SetupDiGetDeviceInterfaceDetail(hdinfo, &did, pdidd, DIDD_BUFSIZE, NULL, &dinfod)
			                                                                                == 0) {
	        	a1loge(p->log, ICOM_SYS, "hid_get_paths() SetupDiGetDeviceInterfaceDetail failed\n");
				return ICOM_SYS;
			}

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
					if (sscanf(buf, "%x", &VendorID) != 1)
						break;
					if ((cp = strchr(pdidd->DevicePath, 'p')) == NULL)
						break;
					if (strlen(cp) < 8)
						break;
					if (cp[1] != 'i' || cp[2] != 'd' || cp[3] != '_')
						break;
					memcpy(buf, cp + 4, 4);
					buf[4] = '\000';
					if (sscanf(buf, "%x", &ProductID) != 1)
						break;
					gotid = 1;
					break;
				}
				if (!gotid) {
					a1logd(p->log, 1, "found HID device '%s', inst %d but unable get PID and VID\n",pdidd->DevicePath, dinfod.DevInst);
					continue;
				}
			}

			/* If it's a device we're looking for */
			if ((itype = inst_usb_match(VendorID, ProductID, 0)) != instUnknown) {
				struct hid_idevice *hidd;
				char pname[400];
		
				/* Create human readable path/identification */
				sprintf(pname,"hid:/%d (%s)", dinfod.DevInst, inst_name(itype));

				a1logd(p->log, 2, "found HID device '%s', inst %d that we're looking for\n",pdidd->DevicePath, dinfod.DevInst);

				if ((hidd = (struct hid_idevice *) calloc(sizeof(struct hid_idevice), 1)) == NULL) {
		        	a1loge(p->log, ICOM_SYS, "hid_get_paths() calloc failed!\n");
					return ICOM_SYS;
				}
				if ((hidd->dpath = calloc(1, strlen(pdidd->DevicePath)+2)) == NULL) {
		        	a1loge(p->log, ICOM_SYS, "hid_get_paths() calloc failed!\n");
					return ICOM_SYS;
				}
				/* Windows 10 seems to return paths without the leading '\\' */
				/* (Or are the structure layouts not matching ?) */
				if (pdidd->DevicePath[0] == '\\' &&
				    pdidd->DevicePath[1] != '\\')
					strcpy(hidd->dpath, "\\");
				strcat(hidd->dpath, pdidd->DevicePath);

				/* Add the path to the list */
				p->add_hid(p, pname, VendorID, ProductID, 0, hidd, itype);

			} else {
				a1logd(p->log, 6, "found HID device '%s', inst %d but not one we're looking for\n",pdidd->DevicePath, dinfod.DevInst);
			}
		}

		/* Now we're done with the hdifo */
		if (SetupDiDestroyDeviceInfoList(hdinfo) == 0) {
        	a1loge(p->log, ICOM_SYS, "SetupDiDestroyDeviceInfoList failed\n");
			return ICOM_SYS;
		}
	}
#endif /* NT */

#ifdef __APPLE__
# if defined(USE_NEW_OSX_CODE) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	{
		CFAllocatorContext alocctx;
		IOHIDManagerRef mref;
		CFSetRef setref;
		CFIndex count, i;
		IOHIDDeviceRef *values;

		/* Create HID Manager reference */
		if ((mref = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDManagerOptionNone)) == NULL) {
        	a1loge(p->log, ICOM_SYS, "hid_get_paths() IOHIDManagerCreate returned NULL\n");
			return ICOM_SYS;
		}

		/* Set to match all HID devices */
		IOHIDManagerSetDeviceMatching(mref, NULL); 

		/* Enumerate the devices (doesn't seem to have to open) */
		if ((setref = IOHIDManagerCopyDevices(mref)) == NULL) {
        	a1loge(p->log, ICOM_SYS, "hid_get_paths() IOHIDManagerCopyDevices failed\n");
			CFRelease(mref);
			return ICOM_SYS;
		}

		count = CFSetGetCount(setref);
		if ((values = (IOHIDDeviceRef *)calloc(sizeof(IOHIDDeviceRef), count)) == NULL) {
        	a1loge(p->log, ICOM_SYS, "hid_get_paths() calloc failed!\n");
			CFRelease(setref);
			CFRelease(mref);
			return ICOM_SYS;
		}

		CFSetGetValues(setref, (const void **)values);

		for (i = 0; i < count; i++) {
			CFNumberRef vref, pref;					/* HID Vendor and Product ID propeties */
			CFNumberRef lidpref;					/* Location ID properties */
			unsigned int vid = 0, pid = 0, lid = 0;
			instType itype;
			IOHIDDeviceRef ioob = values[i];		/* HID object found */

        	if ((vref = IOHIDDeviceGetProperty(ioob, CFSTR(kIOHIDVendorIDKey))) != NULL) {
                CFNumberGetValue(vref, kCFNumberSInt32Type, &vid);
				CFRelease(vref);
            }
        	if ((pref = IOHIDDeviceGetProperty(ioob, CFSTR(kIOHIDProductIDKey))) != NULL) {
                CFNumberGetValue(pref, kCFNumberSInt32Type, &pid);
				CFRelease(vref);
            }
			if ((lidpref = IOHIDDeviceGetProperty(ioob, CFSTR("LocationID"))) != NULL) {
				CFNumberGetValue(lidpref, kCFNumberIntType, &lid);
			    CFRelease(lidpref);
			}

			/* If it's a device we're looking for */
			if ((itype = inst_usb_match(vid, pid, 0)) != instUnknown) {
				struct hid_idevice *hidd;
				char pname[400];

	        	a1logd(p->log, 2, "found HID device '%s' lid 0x%x that we're looking for\n",inst_name(itype), lid);

				/* Create human readable path/identification */
				sprintf(pname,"hid%d: (%s)", lid >> 20, inst_name(itype));
		
				if ((hidd = (struct hid_idevice *)calloc(sizeof(struct hid_idevice), 1)) == NULL) {
		        	a1loge(p->log, ICOM_SYS, "hid_get_paths calloc failed!\n");
					return ICOM_SYS;
				}
				hidd->lid = lid;
				CFRetain(ioob);				/* We're retaining it */
				hidd->ioob = ioob;

				/* Add the path to the list */
				p->add_hid(p, pname, vid, pid, 0, hidd, itype);
			}
		}
		/* Clean up */
		free(values);
		CFRelease(setref);
		CFRelease(mref);
	}
#else	/* < 1060 */
	{
	    kern_return_t kstat; 
	    CFMutableDictionaryRef sdict;		/* HID Device  dictionary */
		io_iterator_t mit;					/* Matching itterator */

		/* Get dictionary of HID devices */
    	if ((sdict = IOServiceMatching(kIOHIDDeviceKey)) == NULL) {
        	a1loge(p->log, ICOM_SYS, "hid_get_paths() IOServiceMatching returned a NULL dictionary\n");
			return ICOM_SYS;
		}

		/* Init itterator to find matching types. Consumes sdict reference */
		if ((kstat = IOServiceGetMatchingServices(kIOMasterPortDefault, sdict, &mit))
			                                                          != KERN_SUCCESS) { 
        	a1loge(p->log, ICOM_SYS, "hid_get_paths() IOServiceGetMatchingServices returned %d\n", kstat);
			return ICOM_SYS;
		}

		/* Find all the matching HID devices */
		for (;;) {
			io_object_t ioob;						/* HID object found */
			CFNumberRef vref, pref;					/* HID Vendor and Product ID propeties */
			CFNumberRef lidpref;					/* Location ID properties */
			unsigned int vid = 0, pid = 0, lid = 0;
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
			if ((lidpref = IORegistryEntryCreateCFProperty(ioob, CFSTR("LocationID"),
			                                         kCFAllocatorDefault,kNilOptions)) != 0) {
				CFNumberGetValue(lidpref, kCFNumberIntType, &lid);
			    CFRelease(lidpref);
			}

			/* If it's a device we're looking for */
			if ((itype = inst_usb_match(vid, pid, 0)) != instUnknown) {
				struct hid_idevice *hidd;
				char pname[400];

	        	a1logd(p->log, 2, "found HID device '%s' lid 0x%x that we're looking for\n",inst_name(itype), lid);

				/* Create human readable path/identification */
				sprintf(pname,"hid%d: (%s)", lid >> 20, inst_name(itype));
		
				if ((hidd = (struct hid_idevice *)calloc(sizeof(struct hid_idevice), 1)) == NULL) {
		        	a1loge(p->log, ICOM_SYS, "hid_get_paths calloc failed!\n");
					return ICOM_SYS;
				}
				hidd->lid = lid;
				hidd->ioob = ioob;
				ioob = 0;			/* Don't release it */

				/* Add the path to the list */
				p->add_hid(p, pname, vid, pid, 0, hidd, itype);
			}
			if (ioob != 0)		/* If we haven't kept it */
			    IOObjectRelease(ioob);		/* Release found object */
		}
	    IOObjectRelease(mit);			/* Release the itterator */
	}
#endif	/* __MAC_OS_X_VERSION_MAX_ALLOWED < 1060 */
#endif /* __APPLE__ */

#if defined(UNIX_X11)

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
//					a1logd(p->log, 8, "found hid device '%s'\n",dpath);
					/* Open it and see what VID and PID it is */
					if ((fd = open(dpath, O_RDONLY)) >= 0) {
						struct hiddev_devinfo hidinfo;
						if (ioctl(fd, HIDIOCGDEVINFO, &hidinfo) < 0)
					       	a1logd(p->log, 1, "Unable to get HID info for '%s'\n",dpath);
						else {
//							a1logd(p->log, 8,"busnum = %d, devnum = %d, vid = 0x%x, pid = 0x%x\n",
//							hidinfo.busnum, hidinfo.devnum, hidinfo.vendor, hidinfo.product);
						}
						close(fd);
					}
// More hotplug/udev magic needed to make the device accesible !
//else a1logd(p->log, 8,"failed to open '%s' err %d\n",dpath,fd);
				}
			}
			closedir(dir);
		}
	}
#endif /* NEVER */
#endif /* UNIX_X11 */

	a1logd(p->log, 8, "icoms_get_paths: returning %d paths and ICOM_OK\n",p->npaths);

	return ICOM_OK;
}

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifdef __APPLE__

/* HID Interrupt callback for OS X */
/* This seems to only get called when the run loop is active. */
static void hid_read_callback(
void *target,
IOReturn result,
void *refcon,
void *sender,
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1050
uint32_t size
#else
UInt32 size
#endif
) {
	icoms *p = (icoms *)target;

	a1logd(p->log, 8, "HID callback called with size %d, result 0x%x\n",size,result);
	p->hidd->result = result;
	p->hidd->bread = size;
	if (p->hidd->rlr != NULL) 
		CFRunLoopStop(p->hidd->rlr);		/* We're done */
	else
		a1logd(p->log, 8, "HID callback has no run loop\n");
}

#endif /* __APPLE__ */

/*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


/* Open an HID port for all our uses. */
/* This always re-opens the port */
/* return icom error */
static int hid_open_port(
icoms *p,
icomuflags hidflags,	/* Any special handling flags */
int retries,			/* > 0 if we should retry set_configuration (100msec) */
char **pnames			/* List of process names to try and kill before opening */
) {
	/* Make sure the port is open */
	if (!p->is_open) {
#if defined(NT) 
		a1logd(p->log, 8, "hid_open_port: about to open HID port '%s' path '%s'\n",p->name,p->hidd->dpath);
#else
		a1logd(p->log, 8, "hid_open_port: about to open HID port '%s'\n",p->name);
#endif

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
  					if ((p->hidd->ols.hEvent = CreateEvent(NULL, 0, 0, NULL)) == NULL) {
						a1loge(p->log, ICOM_SYS, "hid_open_port: Failed to create HID Event "
						                              "with %d'\n",GetLastError());
						return ICOM_SYS;
					}
					break;
				}
				if (tries > 0 && pnames != NULL) {
					/* Open failed. This could be the i1ProfileTray.exe */
					kill_nprocess(pnames, p->log);		/* Try and kill it once */
					msec_sleep(100);
				}
			}
			if (p->hidd->fh == INVALID_HANDLE_VALUE) {
				a1loge(p->log, ICOM_SYS, "hid_open_port: Failed to open "
				                     "path '%s' with err %d\n",p->hidd->dpath, GetLastError());
				return ICOM_SYS;
			}
		}
#endif /* NT */

#ifdef __APPLE__
# if defined(USE_NEW_OSX_CODE) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
		{
			/* Open the device */
			if (IOHIDDeviceOpen(p->hidd->ioob, kIOHIDOptionsTypeSeizeDevice) != kIOReturnSuccess) {
				a1loge(p->log, ICOM_SYS, "hid_open_port: Opening HID device '%s' failed",p->name);
				return ICOM_SYS;
			}
		}
#else	/* < 1060 */
		{
			IOCFPlugInInterface **piif = NULL;
			IOReturn result;
			SInt32 score;

			if ((result = IOCreatePlugInInterfaceForService(p->hidd->ioob, kIOHIDDeviceUserClientTypeID,
			    kIOCFPlugInInterfaceID, &piif, &score) != kIOReturnSuccess || piif == NULL)) {
				a1loge(p->log, ICOM_SYS, "hid_open_port: Failed to get piif for "
				  "HID device '%s', result 0x%x, piif 0x%x\n",p->name,result,piif);
				return ICOM_SYS;
			}
	
			p->hidd->device = NULL;
			if ((*piif)->QueryInterface(piif,
				    CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID122), (LPVOID)&p->hidd->device)
			        != kIOReturnSuccess || p->hidd->device == NULL) {
				a1loge(p->log, ICOM_SYS, "hid_open_port: Getting HID device '%s' failed",p->name);
				return ICOM_SYS;
			}
			(*piif)->Release(piif);		/* delete intermediate object */

			if ((*p->hidd->device)->open(p->hidd->device, kIOHIDOptionsTypeSeizeDevice)
			                                                       != kIOReturnSuccess) {
				a1loge(p->log, ICOM_SYS, "hid_open_port: Opening HID device '%s' failed",p->name);
				return ICOM_SYS;
			}

			/* Setup to handle interrupt read callbacks */
			p->hidd->port = 0;
			if ((*(p->hidd->device))->createAsyncPort(p->hidd->device, &p->hidd->port)
			        != kIOReturnSuccess || p->hidd->port == 0) {
				a1loge(p->log, ICOM_SYS, "hid_open_port: Creating port on "
				                          "HID device '%s' failed",p->name);
				return ICOM_SYS;
			}

			p->hidd->evsrc = NULL;
			if ((*(p->hidd->device))->createAsyncEventSource(p->hidd->device, &p->hidd->evsrc)
			        != kIOReturnSuccess || p->hidd->evsrc == NULL) {
				a1loge(p->log, ICOM_SYS, "hid_open_port: Creating event source on "
				                                  "HID device '%s' failed",p->name);
				return ICOM_SYS;
			}

			/* Setup for read callback */
			p->hidd->result = -1;
			p->hidd->bread = 0;
	
			/* And set read callback routine */
			if ((*(p->hidd->device))->setInterruptReportHandlerCallback(p->hidd->device,
				p->hidd->rbuf, 256, hid_read_callback, (void *)p, NULL) != kIOReturnSuccess) {
				a1loge(p->log, ICOM_SYS, "icoms_hid_read: Setting callback handler for "
				                                             "HID '%s' failed\n", p->name);
				return ICOM_SYS;
			}

			if ((*(p->hidd->device))->startAllQueues(p->hidd->device) != kIOReturnSuccess) {
				a1loge(p->log, ICOM_SYS, "icoms_hid_read: Starting queues for "
				                                   "HID '%s' failed\n", p->name);
				return ICOM_SYS;
			}
		}
#endif	/* __MAC_OS_X_VERSION_MAX_ALLOWED < 1060 */
#endif /* __APPLE__ */

		p->is_open = 1;
		a1logd(p->log, 8, "hid_open_port: HID port is now open\n");
	}

	/* Install the cleanup signal handlers, and add to our cleanup list */
	usb_install_signal_handlers(p);

	return ICOM_OK;
}

/* Close an open HID port */
/* If we don't do this, the port and/or the device may be left in an unusable state. */
void hid_close_port(icoms *p) {

	a1logd(p->log, 8, "hid_close_port: called\n");

	if (p->is_open && p->hidd != NULL) {

#if defined(NT) 
		CloseHandle(p->hidd->ols.hEvent);
		CloseHandle(p->hidd->fh);
#endif /* NT */

#ifdef __APPLE__
# if defined(USE_NEW_OSX_CODE) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
		if (IOHIDDeviceClose(p->hidd->ioob, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
			a1loge(p->log, ICOM_SYS, "hid_close_port: closing HID port '%s' failed",p->name);
			return;
		}
#else	/* < 1060 */
		if ((*(p->hidd->device))->stopAllQueues(p->hidd->device) != kIOReturnSuccess) {
			a1loge(p->log, ICOM_SYS, "icoms_hid_read: Stopping queues for "
			                                    "HID '%s' failed\n", p->name);
			return;
		}

	    IOObjectRelease(p->hidd->port);
		p->hidd->port = 0;

		if (p->hidd->evsrc != NULL)
			CFRelease(p->hidd->evsrc);
		p->hidd->evsrc = NULL;

		p->hidd->rlr = NULL;

		if ((*p->hidd->device)->close(p->hidd->device) != kIOReturnSuccess) {
			a1loge(p->log, ICOM_SYS, "hid_close_port: closing HID port '%s' failed",p->name);
			return;
		}

		if ((*p->hidd->device)->Release(p->hidd->device) != kIOReturnSuccess) {
			a1loge(p->log, ICOM_SYS, "hid_close_port: Releasing HID port '%s' failed",p->name);
		}
		p->hidd->device = NULL;
#endif	/* __MAC_OS_X_VERSION_MAX_ALLOWED < 1060 */
#endif /* __APPLE__ */

		p->is_open = 0;
		a1logd(p->log, 8, "hid_close_port: has been released and closed\n");
	}

	/* Find it and delete it from our static cleanup list */
	usb_delete_from_cleanup_list(p);
}


/* ========================================================= */

/* HID Interrupt pipe Read */
/* Don't retry on a short read, return ICOM_SHORT. */
/* [Probably uses the control pipe. We need a different */
/*  call for reading messages from the interrupt pipe] */
static int
icoms_hid_read(icoms *p,
	unsigned char *rbuf,	/* Read buffer */
	int bsize,				/* Bytes to read */
	int *breadp,			/* Bytes read */
	double tout				/* Timeout in seconds */
) {
	int retrv = ICOM_OK;	/* Returned error value */
	int bread = 0;

	a1logd(p->log, 8, "icoms_hid_read: %d bytes, tout %f\n",bsize,tout);

	if (!p->is_open) {
		a1loge(p->log, ICOM_SYS, "icoms_hid_read: device not initialised\n");
		return ICOM_SYS;
	}

#if defined(NT)
	{
		unsigned char *rbuf2;

		/* Create a copy of the data recieved with one more byte */
		if ((rbuf2 = malloc(bsize + 1)) == NULL) {
			a1loge(p->log, ICOM_SYS, "icoms_hid_read: malloc failed\n");
			return ICOM_SYS;
		}
		rbuf2[0] = 0;
		if (ReadFile(p->hidd->fh, rbuf2, bsize+1, (LPDWORD)&bread, &p->hidd->ols) == 0)  {
			if (GetLastError() != ERROR_IO_PENDING) {
				retrv = ICOM_USBR; 
			} else {
				int res;
				res = WaitForSingleObject(p->hidd->ols.hEvent, (int)(tout * 1000.0 + 0.5));
				if (res == WAIT_FAILED) {
					a1loge(p->log, ICOM_SYS, "icoms_hid_read: HID wait on read failed\n");
					return ICOM_SYS;
				} else if (res == WAIT_TIMEOUT) {
					CancelIo(p->hidd->fh);
					retrv = ICOM_TO; 
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

#ifdef __APPLE__
# if defined(USE_NEW_OSX_CODE) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	{
		IOReturn result;
		CFIndex lbsize = bsize;

		/* Normally the _write should have set the ->rlr */
		/* but this may not be so if we are doing a flush ? */
		if (p->hidd->rlr == NULL) {
			p->hidd->rlr = CFRunLoopGetCurrent();
			p->hidd->result = -1;
			p->hidd->bread = 0;
		}

		IOHIDDeviceScheduleWithRunLoop(p->hidd->ioob, p->hidd->rlr, kCFRunLoopDefaultMode); 

#ifndef NEVER
		/* This doesn't work. Don't know why */
		/* Returns 0xe000404f = kIOUSBPipeStalled, Pipe has stalled, error needs to be cleared */
		if ((result = IOHIDDeviceGetReportWithCallback(
			p->hidd->ioob,
		    kIOHIDReportTypeInput, 
			9, 				/* Bulk or Interrupt transfer ???  - or wbuf[0] ?? */
			(uint8_t *)rbuf, 
			&lbsize, 
			tout,
			NULL, NULL)) != kIOReturnSuccess) {
printf("~1 IOHIDDeviceGetReportWithCallback returned 0x%x\n",result);
			/* (We could detect other error codes and translate them to ICOM) */
			if (result == kIOReturnTimeout)
				retrv = ICOM_TO; 
			else
				retrv = ICOM_USBR; 
		} else {
			bsize = lbsize;
			bread = bsize;
		}
#else
		/* This doesn't work. Don't know why */
		/* Returns 0xe000404f = kIOUSBPipeStalled, Pipe has stalled, error needs to be cleared */
		if ((result = IOHIDDeviceGetReport(
			p->hidd->ioob,
		    kIOHIDReportTypeInput, 
			9, 				/* Bulk or Interrupt transfer ???  - or wbuf[0] ?? */
			(uint8_t *)rbuf, 
			&lbsize)) != kIOReturnSuccess) {
printf("~1 IOHIDDeviceGet returned 0x%x\n",result);
			/* (We could detect other error codes and translate them to ICOM) */
			if (result == kIOReturnTimeout)
				retrv = ICOM_TO; 
			else
				retrv = ICOM_USBR; 
		} else {
			bsize = lbsize;
			bread = bsize;
		}
#endif
		
		IOHIDDeviceUnscheduleFromRunLoop(p->hidd->ioob, p->hidd->rlr, kCFRunLoopDefaultMode);
		p->hidd->rlr = NULL;
	}
#else	/* < 1060 */
#ifndef NEVER	/* This works */
	{
		IOReturn result;

		if (bsize > HID_RBUF_SIZE) {
			a1loge(p->log, ICOM_SYS, "icoms_hid_read: Requested more bytes that HID_RBUF_SIZE\n", p->name);
			return ICOM_SYS;
		}

		/* Normally the _write should have set the ->rlr */
		/* but this may not be so if we are doing a flush ? */
		if (p->hidd->rlr == NULL) {
			p->hidd->rlr = CFRunLoopGetCurrent();
			p->hidd->result = -1;
			p->hidd->bread = 0;
		} 

		CFRunLoopAddSource(p->hidd->rlr, p->hidd->evsrc, kCFRunLoopDefaultMode);

		result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, tout, false);

		if (result == kCFRunLoopRunTimedOut) {
			retrv = ICOM_TO; 
		} else if (result != kCFRunLoopRunStopped) {
			retrv = ICOM_USBR; 
		}
		CFRunLoopRemoveSource(p->hidd->rlr, p->hidd->evsrc, kCFRunLoopDefaultMode);
		p->hidd->rlr = NULL;				/* Callback is invalid now */

		if (p->hidd->result == -1) {		/* Callback wasn't called */
			retrv = ICOM_TO; 
		} else if (p->hidd->result != kIOReturnSuccess) {
			a1loge(p->log, ICOM_SYS, "icoms_hid_read: Callback for "
			        "HID '%s' got unexpected return value\n", p->name);
			p->hidd->result = -1;	/* Result is now invalid */
			p->hidd->bread = 0;
			return ICOM_SYS;
		}
		bread = p->hidd->bread;
		if (bread > 0)
			memcpy(rbuf, p->hidd->rbuf, bread > HID_RBUF_SIZE ? HID_RBUF_SIZE : bread);
	}
#else	// NEVER
	/* This doesn't work. Don't know why */
	/* Returns 0xe000404f = kIOUSBPipeStalled, Pipe has stalled, error needs to be cleared */
	{
		IOReturn result;
		if ((result = (*p->hidd->device)->getReport(
			p->hidd->device,
		    kIOHIDReportTypeInput, 
			9, 				/* Bulk or Interrupt transfer */
			(void *)rbuf, 
			(UInt32 *)&bsize, 
			(unsigned int)(tout * 1000.0 + 0.5), 
			NULL, NULL, NULL)
		) != kIOReturnSuccess) {
			/* (We could detect other error codes and translate them to ICOM) */
			if (result == kIOReturnTimeout)
				retrv = ICOM_TO; 
			else
				retrv = ICOM_USBW; 
		} else {
			bread = bsize;
		}
	}
#endif	// NEVER
#endif	/* __MAC_OS_X_VERSION_MAX_ALLOWED < 1060 */
#endif /* __APPLE__ */

	if (breadp != NULL)
		*breadp = bread;

	a1logd(p->log, 8, "icoms_hid_read: About to return hid read %d bytes, ICOM err 0x%x\n",
	                                                                         bread, retrv);

	return retrv;
}

/* - - - - - - - - - - - - - */

/* HID Command Pipe Write */
/* Don't retry on a short read, return ICOM_SHORT. */
static int
icoms_hid_write(icoms *p,
	unsigned char *wbuf,	/* Write buffer */
	int bsize,				/* Bytes to write */
	int *bwrittenp,			/* Bytes written */
	double tout				/* Timeout in seconds */
) {
	int retrv = ICOM_OK;	/* Returned error value */
	int bwritten = 0;

	a1logd(p->log, 8, "icoms_hid_write: %d bytes, tout %f\n",bsize,tout);

	if (!p->is_open) {
		a1loge(p->log, ICOM_SYS, "icoms_hid_write: device not initialised\n");
		return ICOM_SYS;
	}

#if defined(NT)
	{
		unsigned char *wbuf2;

		/* Create a copy of the data to send with one more byte */
		if ((wbuf2 = malloc(bsize + 1)) == NULL) {
			a1loge(p->log, ICOM_SYS, "icoms_hid_write: malloc failed\n");
			return ICOM_SYS;
		}
		memmove(wbuf2+1,wbuf,bsize);
		wbuf2[0] = 0;		/* Extra report ID byte (why ?) */
		if (WriteFile(p->hidd->fh, wbuf2, bsize+1, (LPDWORD)&bwritten, &p->hidd->ols) == 0) { 
			if (GetLastError() != ERROR_IO_PENDING) {
				retrv = ICOM_USBW; 
			} else {
				int res;
				res = WaitForSingleObject(p->hidd->ols.hEvent, (int)(tout * 1000.0 + 0.5));
				if (res == WAIT_FAILED) {
					a1loge(p->log, ICOM_SYS, "icoms_hid_write: HID wait on write failed\n");
					return ICOM_SYS;
				}
				else if (res == WAIT_TIMEOUT) {
					CancelIo(p->hidd->fh);
					retrv = ICOM_TO; 
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
# if defined(USE_NEW_OSX_CODE) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	{
		IOReturn result;
#ifdef NEVER
		/* This doesn't work. Don't know why */
		/* Returns 0xe000404f = kIOUSBPipeStalled, Pipe has stalled, error needs to be cleared */
		/* if we did a read that has timed out */
		/* Returns 0xe00002c7 = kIOReturnUnsupported, without the failed read
		if ((result = IOHIDDeviceSetReportWithCallback(
			p->hidd->ioob,
		    kIOHIDReportTypeOutput, 
			9, 				/* Bulk or Interrupt transfer ???  - or wbuf[0] ?? */
			(uint8_t *)wbuf, 
			bsize, 
			tout,
			NULL, NULL)) != kIOReturnSuccess) {
printf("~1 IOHIDDeviceSetReportWithCallback returned 0x%x\n",result);
			/* (We could detect other error codes and translate them to ICOM) */
			if (result == kIOReturnTimeout)
				retrv = ICOM_TO; 
			else
				retrv = ICOM_USBW; 
		} else {
			bwritten = bsize;
		}
#else
		/* This works, but has no timeout */
		if ((result = IOHIDDeviceSetReport(
			p->hidd->ioob,
		    kIOHIDReportTypeOutput, 
			9, 				/* Bulk or Interrupt transfer ???  - or wbuf[0] ?? */
			(uint8_t *)wbuf, 
			bsize)) != kIOReturnSuccess) {
			/* (We could detect other error codes and translate them to ICOM) */
			if (result == kIOReturnTimeout)
				retrv = ICOM_TO; 
			else
				retrv = ICOM_USBW; 
		} else {
			bwritten = bsize;
		}
#endif
	}
#else	/* < 1060 */
	{
		/* Clear any existing read message */
		/* (We're assuming it's all write then read measages !!) */
		p->hidd->rlr = CFRunLoopGetCurrent();		// Our thread's run loop
		p->hidd->result = -1;
		p->hidd->bread = 0;

		IOReturn result;
		if ((result = (*p->hidd->device)->setReport(
			p->hidd->device,
		    kIOHIDReportTypeOutput, 
			9, 				/* Bulk or Interrupt transfer */
			(void *)wbuf, 
			bsize, 
			(unsigned int)(tout * 1000.0 + 0.5), 
			NULL, NULL, NULL)) != kIOReturnSuccess) {
			/* (We could detect other error codes and translate them to ICOM) */
			if (result == kIOReturnTimeout)
				retrv = ICOM_TO; 
			else
				retrv = ICOM_USBW; 
		} else {
			bwritten = bsize;
		}
	}
#endif	/* __MAC_OS_X_VERSION_MAX_ALLOWED < 1060 */
#endif /* __APPLE__ */

	if (bwrittenp != NULL)
		*bwrittenp = bwritten;

	a1logd(p->log, 8, "icoms_hid_write: wrote %d bytes, ICOM err 0x%x\n",bwritten, retrv);

	return retrv;
}

/* ------------------------------------------------- */
/* Set the hid port number and characteristics. */
/* This may be called to re-establish a connection that has failed */
/* return icom error */
static int
icoms_set_hid_port(
icoms *p, 
icomuflags hidflags,	/* Any special handling flags */
int retries,			/* > 0 if we should retry set_configuration (100msec) */
char **pnames			/* List of process names to try and kill before opening */
) {
	int rv = ICOM_OK;

	a1logd(p->log, 8, "icoms_set_hid_port: About to set HID port characteristics\n");

	if (p->is_open) 
		p->close_port(p);

	if (p->port_type(p) == icomt_hid) {

		if ((rv = hid_open_port(p, hidflags, retries, pnames)) != ICOM_OK)
			return rv;

		p->write = NULL;
		p->read = NULL;

	}
	a1logd(p->log, 8, "icoms_set_hid_port: HID port characteristics set ok\n");

	return rv;
}

/* Copy hid_idevice contents from icompaths to icom */
/* return icom error */
int hid_copy_hid_idevice(icoms *d, icompath *s) {

	if (s->hidd == NULL) { 
		d->hidd = NULL;
		return ICOM_OK;
	}

	if ((d->hidd = calloc(sizeof(struct hid_idevice), 1)) == NULL) {
		a1loge(d->log, ICOM_SYS, "hid_copy_hid_idevice: malloc failed\n");
		return ICOM_SYS;
	}

#if defined(NT)
	if ((d->hidd->dpath = strdup(s->hidd->dpath)) == NULL) {
		a1loge(d->log, ICOM_SYS, "hid_copy_hid_idevice: malloc\n");
		return ICOM_SYS;
	}
#endif
#if defined(__APPLE__)
# if defined(USE_NEW_OSX_CODE) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	d->hidd->ioob = s->hidd->ioob;
	CFRetain(d->hidd->ioob);
#else
	d->hidd->ioob = s->hidd->ioob;
	IOObjectRetain(d->hidd->ioob);
#endif	/* __MAC_OS_X_VERSION_MAX_ALLOWED < 1060 */
#endif	/* __APPLE__ */
#if defined (UNIX_X11)
#endif
	return ICOM_OK;
}

/* Cleanup and then free a hid_del_hid_idevice */
void hid_del_hid_idevice(struct hid_idevice *hidd) {
	if (hidd == NULL)
		return;

#if defined(NT)
	if (hidd->dpath != NULL)
		free(hidd->dpath);
#endif
#if defined(__APPLE__)
# if defined(USE_NEW_OSX_CODE) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	if (hidd->ioob != 0)
		CFRelease(hidd->ioob);
#else
	if (hidd->ioob != 0)
		IOObjectRelease(hidd->ioob);
#endif	/* __MAC_OS_X_VERSION_MAX_ALLOWED < 1060 */
#endif	/* __APPLE__ */
#if defined (UNIX_X11)
#endif
	free(hidd);
}

/* Cleanup any HID specific icoms info */
void hid_del_hid(icoms *p) {
	hid_del_hid_idevice(p->hidd);
}


/* ---------------------------------------------------------------------------------*/

/* Set the HID specific icoms methods */
void hid_set_hid_methods(
icoms *p
) {
	p->set_hid_port   = icoms_set_hid_port;
	p->hid_read       = icoms_hid_read;
	p->hid_write      = icoms_hid_write;
}

/* ---------------------------------------------------------------------------------*/
