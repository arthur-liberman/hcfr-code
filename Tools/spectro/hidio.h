
#ifndef HIDIO_H

 /* General USB HID I/O support */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   2007/10/10
 *
 * Copyright 2006 - 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* These routines supliement the class code in ntio.c and unixio.c */
/* They do benign things if ENABLE_USB is undefined */

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
void hid_get_paths(struct _icoms *p);

/* Return the instrument type if the port number is HID, */
/* and instUnknown if it is not. */
instType hid_is_hid_portno(icoms *p, int port);

void hid_close_port(icoms *p);

/* Set the HID specific icoms methods */
void hid_set_hid_methods(icoms *p);

/* Opaque structure to hid OS dependent HID details */
struct _hid_device {
	int temp;					/* Shut the compiler up */
#if defined(NT)
	char *dpath;				/* Path to open the device */
	/* Stuff setup when device is open: */
	HANDLE fh;					/* File handle for write/read */
	OVERLAPPED ols;				/* Overlapped structure for write/read */
#endif
#if defined(__APPLE__)
	io_object_t ioob;					/* Object to open */
	/* Stuff setup when device is open: */
	IOHIDDeviceInterface122 **device;	/* OS X HID device we've opened */
	mach_port_t port;					/* Other stuff present when opened */
	CFRunLoopSourceRef evsrc;
	CFRunLoopRef rlr;
	IOReturn result;
    int bread;            				/* Bytes read by callback */
#endif
#if defined (UNIX) && !defined(__APPLE__)
#endif
}; typedef struct _hid_device hid_device;

/* Cleanup and then free an hev entry */
void hid_del_hid_device(hid_device *hev);

#ifdef __cplusplus
	}
#endif

#define HIDIO_H
#endif /* HIDIO_H */

