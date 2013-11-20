
#ifndef HIDIO_H

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
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* These routines supliement the class code in ntio.c and unixio.c */

#ifdef __APPLE__
#include <sys/param.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>
#endif /* __APPLE__ */

#ifdef __cplusplus
	extern "C" {
#endif

/* Add or modify paths to USB HID connected instruments, to the existing */
/* icompath paths in the icoms structure, if HID access is supported. */
/* return icom error */
int hid_get_paths(struct _icompaths *p);

void hid_close_port(icoms *p);

/* Set the HID specific icoms methods */
void hid_set_hid_methods(icoms *p);

/* Copy hid_idevice contents */
/* return icom error */
int hid_copy_hid_idevice(icoms *d, icompath *s);

/* Cleanup and then free a hid_del_hid_idevice */
void hid_del_hid_idevice(struct hid_idevice *dev);

/* Cleanup any HID specific icoms info */
void hid_del_hid(icoms *p);

/* Opaque structure to hid OS dependent HID details */
struct hid_idevice {
#if defined(NT)
	char *dpath;				/* Device path */
	/* Stuff setup when device is open: */
	HANDLE fh;					/* File handle for write/read */
	OVERLAPPED ols;				/* Overlapped structure for write/read */
#endif
#if defined(__APPLE__)
# if defined(USE_NEW_OSX_CODE) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
	int lid;							/* Location ID */
	IOHIDDeviceRef ioob;				/* Object to open */
	/* Stuff setup when device is open: */
	CFRunLoopRef rlr;
#else
	int lid;					/* Location ID */
	io_object_t ioob;					/* Object to open */
	/* Stuff setup when device is open: */
	IOHIDDeviceInterface122 **device;	/* OS X HID device we've opened */
	mach_port_t port;					/* Other stuff present when opened */
	CFRunLoopSourceRef evsrc;
	CFRunLoopRef rlr;
	IOReturn result;
#   define HID_RBUF_SIZE 1024
	unsigned char rbuf[HID_RBUF_SIZE];			/* Buffer for read callback */
    int bread;            				/* Bytes read by callback */
#endif	/* __MAC_OS_X_VERSION_MAX_ALLOWED < 1060 */
#endif
#if defined (UNIX) && !defined(__APPLE__)
	int temp;					/* Shut the compiler up */
#endif
};

/* Cleanup and then free an hidd entry */
void hid_del_hid_idevice(struct hid_idevice *hidd);

#ifdef __cplusplus
	}
#endif

#define HIDIO_H
#endif /* HIDIO_H */

