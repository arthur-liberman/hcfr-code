
#ifndef ICOMS_H

 /* An abstracted instrument serial and USB communication class. */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   2006/4/20
 *
 * Copyright 1996 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 * 
 * Derived from serio.h
 */

/*

	Notes:	The user keyboard/interrupt handling is really broken.
	It's not noticed most of the time because mostly keys are only
	hit at the expected times. Serial instruments (X-Rite) are more
	forgiving in interrupting coms to/from the instrument, as well
	as being long winded and needing abort. For USB insruments it's
	not necessarily robust to interrupt or terminate after a give
	USB transaction. The handling of user commands and aborts
	is not consistent either, possibly leaving some instruments
	suseptable to bodgy results due to an unrecognised aborted
	command. Really, the instrument driver should deterimine
	at what points an operation can be aborted, and how to recover.

	Because the instrument itself can be a source of commands,
	some way of waiting for instrument or user input is needed.
	Could a threaded approach with instrument abort work ?
	
*/

/* Some MSWin specific stuff is in icoms, and used by usbio & hidio */
#if defined (NT)
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0501
# if defined(_WIN32_WINNT) 
#  undef _WIN32_WINNT
# endif
# define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(__APPLE__)
#include <IOKit/usb/IOUSBLib.h>
#endif

#ifdef __cplusplus
	extern "C" {
#endif

#undef QUIET_MEMCHECKERS		/* #define to memset coms read buffers before reading */

typedef struct _icompath icompath;
typedef struct _icompaths icompaths;
typedef struct _icoms icoms;

#ifdef ENABLE_USB

struct usb_idevice;		/* Forward declarations */
struct hid_idevice;

/* Information about each end point */
typedef struct {
	int valid;			/* Flag, nz if this endpoint is valid */
	int addr;			/* Address of end point */
	int packetsize;		/* The max packet size */
	int type;			/* 2 = bulk, 3 = interrupt */	
#ifdef __cplusplus
	int interfaceNum;		/* interface number */
#else
	int interface;		/* interface number */
#endif
	int pipe;			/* pipe number (1..N, OS X only) */
} usb_ep;

#define ICOM_EP_TYPE_BULK 2
#define ICOM_EP_TYPE_INTERRUPT 3

#include "usbio.h"
#include "hidio.h"

#endif /* ENABLE_USB */

/* - - - - - - - - - - - - - - - - - - - -  */

/* Store information about a possible instrument communication path */
/* (Note a path doesn't have a reference to icompaths or its' log) */
struct _icompath{
	char *name;					/* instance description */
#ifdef ENABLE_SERIAL
	char *spath;				/* Serial device path */
#endif
#ifdef ENABLE_USB
	int nep;					/* Number of end points */
	unsigned int vid, pid; 		/* USB vendor and product id's */
	struct usb_idevice *usbd;	/* USB port, NULL if not USB */
	struct hid_idevice *hidd;	/* HID port, NULL if not HID */
#endif /* ENABLE_USB */
	instType itype;				/* Type of instrument if known */
};

extern icompath icomFakeDevice;	/* Declare fake device */

/* The available instrument communication paths */
struct _icompaths {
	int npaths;					/* Number of paths */
	icompath **paths;			/* Paths if any */
	a1log *log;					/* Verbose, debuge & error logging */

	/* Re-populate the available instrument list */
	/* return icom error */
	int (*refresh)(struct _icompaths *p);

#ifdef ENABLE_SERIAL
	/* Add a serial path. path is copied. Return icom error */
	int (*add_serial)(struct _icompaths *p, char *name, char *spath);
#endif /* ENABLE_SERIAL */

#ifdef ENABLE_USB
	/* Add a usb path. usbd is taken, others are copied. Return icom error */
	int (*add_usb)(struct _icompaths *p, char *name, unsigned int vid, unsigned int pid,
	                int nep, struct usb_idevice *usbd, instType itype);

	/* Add an hid path. hidd is taken, others are copied. Return icom error */
	int (*add_hid)(struct _icompaths *p, char *name, unsigned int vid, unsigned int pid,
	                int nep, struct hid_idevice *hidd, instType itype);
#endif /* ENABLE_USB */

	/* Return the path corresponding to the port number, or NULL if out of range */
	icompath *(*get_path)(
		struct _icompaths *p, 
		int  	       port);		/* Enumerated port number, 1..n */

	/* Clear all the paths */
	void (*clear)(struct _icompaths *p);

	/* We're done */
	void (*del)(struct _icompaths *p);

};

/* Allocate an icom paths and set it to the list of available devices */
/* Return NULL on error */
icompaths *new_icompaths(a1log *log);


/* - - - - - - - - - - - */
/* Serial related stuff */

/* Flow control */
typedef enum {
	fc_nc = 0,			/* not configured/default */
	fc_none,
	fc_XonXOff,
	fc_Hardware
} flow_control;

/* baud rate available on all systems */
typedef enum {
	baud_nc      = 0,		/* Not Configured */
	baud_110     = 1,
	baud_300     = 2,
	baud_600     = 3,
	baud_1200    = 4,
	baud_2400    = 5,
	baud_4800    = 6,
	baud_9600    = 7,
	baud_14400   = 8,
	baud_19200   = 9,
	baud_38400   = 10,
	baud_57600   = 11,
	baud_115200  = 12
} baud_rate;

/* Possible parity */
typedef enum {
	parity_nc,
	parity_none,
	parity_odd,
	parity_even
} parity;

/* Possible stop bits */
typedef enum {
	stop_nc,
	stop_1,
	stop_2
} stop_bits;

/* Possible word length */
typedef enum {
	length_nc,
	length_5,
	length_6,
	length_7,
	length_8
} word_length;

/* USB open/handling/buf workaround flags */
typedef enum {
	icomuf_none                = 0x0000,
	icomuf_detach              = 0x0001,	/* Attempt to detach from system driver */
	icomuf_no_open_clear       = 0x0001,	/* Don't send a clear_halt after opening the port */
	icomuf_reset_before_close  = 0x0004,	/* Reset port before closing it */
	icomuf_resetep_before_read = 0x0008		/* Do a usb_resetep before each ep read */
} icomuflags;

/* Type of port */
typedef enum {
	icomt_serial,		/* Serial port */
	icomt_usb,			/* USB port */
	icomt_hid			/* HID (USB) port */
} icom_type;

/* Status bits/return values */
#define ICOM_OK	    0x000000		/* No error */

#define ICOM_NOTS 	0x001000		/* Not supported */
#define ICOM_SIZE	0x002000		/* Request/response size exceeded limits */
#define ICOM_TO		0x004000		/* Timed out */
#define ICOM_SHORT	0x008000		/* Number of bytes wasn't read/written */
#define ICOM_CANC	0x010000		/* Was cancelled */
#define ICOM_SYS	0x020000		/* System error (ie. malloc, system call fail) */
#define ICOM_VER	0x040000		/* Version error - need up to date kernel driver */

#define ICOM_USBR	0x000100		/* Unspecified USB read error */
#define ICOM_USBW	0x000200		/* Unspecified USB write error */
#define ICOM_SERR	0x000400		/* Unspecified Serial read error */
#define ICOM_SERW	0x000800		/* Unspecified Serial write error */

#define ICOM_XRE	0x000040		/* Xmit shift reg empty */
#define ICOM_XHE	0x000020		/* Xmit hold reg empty */
#define ICOM_BRK	0x000010		/* Break detected */
#define ICOM_FER	0x000008		/* Framing error */
#define ICOM_PER	0x000004		/* Parity error */
#define ICOM_OER	0x000002		/* Overun error */
#define ICOM_DRY	0x000001		/* Recv data ready */



/* Cancelation token. */
typedef struct _usb_cancelt usb_cancelt;

#ifdef ENABLE_USB
void usb_init_cancel(usb_cancelt *p);
void usb_uninit_cancel(usb_cancelt *p);

#endif

struct _icoms {
  /* Private: */

	/* Copy of some of icompath contents: */
	char *name;					/* Device description */
	instType itype;				/* Type of instrument if known */
	
	int is_open;				/* Flag, NZ if this port is open */

#ifdef ENABLE_SERIAL

	/* Serial port parameters */
	char *spath;				/* Serial port path */
#if defined (MSDOS)
	unsigned short porta;		/* Hardware port address */
#endif
#if defined (NT)
	HANDLE phandle;				/* NT handle */
#endif
#if defined (UNIX) || defined(__APPLE__)
	int fd;						/* Unix file descriptor */
#endif
	flow_control fc;
	baud_rate	br;
	parity		py;
	stop_bits	sb;
	word_length	wl;

#endif	/* ENABLE_SERIAL */

#ifdef ENABLE_USB

	/* USB port parameters */
	struct usb_idevice *usbd;	/* USB port - copy of ppath->usbd */
#ifndef NATIVE_USB				/* Hmm. could put all this within usb_idevice if not #defined ? */
# ifdef USE_LIBUSB1
	libusb_device_handle *usbh;
# else
	struct usb_dev_handle *usbh;
# endif
#endif
	icomuflags uflags;		/* Bug workaround flags */

	unsigned int vid, pid; 	/* USB vendor and product id's, used to distiguish instruments */
	int nconfig;			/* Number of configurations */
	int config;				/* Config this info applies to (always 1 ?) */
	int cconfig;			/* Current configuration (0 or 1 ?) */
	int nifce;				/* Number of interfaces */
	int nep;				/* Number of end points */
	int wr_ep, rd_ep;		/* Default end points to use for "serial" read/write */
	int rd_qa;				/* Read quanta size */

	usb_ep ep[32];			/* Information about each end point for general usb i/o */

/* Macro to access end point information */
#define EPINFO(addr) ep[((addr >> 3) & 0x10) + (addr & 0x0f)]

	/* HID port parameters */
	struct hid_idevice *hidd;	/* HID port - copy of ppath->hidd */

#endif /* ENABLE_USB */

	/* General parameters */
	int lserr;					/* Last serial communication error code */
	int tc;						/* Current serial parser termination character (-1 if not set) */
	a1log *log;					/* Debug & Error log */
	int debug;					/* legacy - Flag, nz to print instrument io info to stderr */

	/* Linked list to automate SIGKILL cleanup */
	struct _icoms *next;
	
  /* Public: */

	/* Return the port type */
	icom_type (*port_type)(struct _icoms *p);

#ifdef ENABLE_SERIAL
	/* Select the serial communications port and characteristics */
	/* return icom error */
	int (*set_ser_port)(
		struct _icoms *p, 
		flow_control fc,		/* Flow control */
		baud_rate	 baud,
		parity		 parity,
		stop_bits	 stop_bits,
		word_length	 word_length);
#endif  /* ENABLE_SERIAL */

#ifdef ENABLE_USB
	/* Select the USB communications port and characteristics */
	/* return icom error */
	int (*set_usb_port)(
		struct _icoms *p, 
	    int            config,		/* Configuration */
	    int            wr_ep,		/* "serial" write end point */
	    int            rd_ep,		/* "serial" read end point */
		icomuflags usbflags,		/* Any special handling flags */
		int retries,				/* > 0 if we should retry set_configuration (100msec) */
		char **pnames				/* List of process names to try and kill before opening */
	);

	/* Select the HID communications port and characteristics */
	/* return icom error */
	int (*set_hid_port)(
		struct _icoms *p, 
		icomuflags usbflags,		/* Any special handling flags */
		int retries,				/* > 0 if we should retry set_configuration (100msec) */
		char **pnames				/* List of process names to try and kill before opening */
	);
#endif /* ENABLE_USB */

	/* Close the port */
	void (*close_port)(struct _icoms *p);

	/* "Serial" write the characters in the buffer out */
	/* Data will be written up to the terminating nul. */
	/* return icom error */
	int (*write)(
		struct _icoms *p,
		char *buf,
		double tout);		/* Timeout in seconds */

	/* "Serial" read characters into the buffer */
	/* The returned data will be terminated by a nul. */
	/* return icom error */
	int (*read)(
		struct _icoms *p,
		char *buf,			/* Buffer to store characters read */
		int bsize,			/* Buffer size */
		char tc,			/* Terminating character */
		int ntc,			/* Number of terminating characters */
		double tout);		/* Timeout in seconds */

	/* "Serial" write and read */
	/* return icom error */
	int (*write_read)(
		struct _icoms *p,
		char *wbuf,			/* Write puffer */
		char *rbuf,			/* Read buffer */
		int bsize,			/* Buffer size */
		char tc,			/* Terminating characer */
		int ntc,			/* Number of terminating characters */
		double tout);		/* Timeout in seconds */

	/* For a USB device, do a control message */
	/* return icom error */
	int (*usb_control)(struct _icoms *p,
		int requesttype,		/* 8 bit request type (USB bmRequestType) */
		int request,			/* 8 bit request code (USB bRequest) */
		int value,				/* 16 bit value (USB wValue, sent little endian) */
		int index,				/* 16 bit index (USB wIndex, sent little endian) */
		unsigned char *rwbuf,	/* Read or write buffer */
		int rwsize,				/* Bytes to read or write */
		double tout);			/* Timeout in seconds */

	/* For a USB device, do a bulk or interrupt read from an end point */
	/* return icom error */
	int (*usb_read)(struct _icoms *p,
		usb_cancelt *cancelt,	/* Optionaly tokem that can be used to cancel */
		int ep,					/* End point address */
		unsigned char *buf,		/* Read buffer */
		int bsize,				/* Bytes to read or write */
		int *bread,				/* Bytes read */
		double tout);			/* Timeout in seconds */

	/* For a USB device, do a bulk or interrupt write to an end point */
	/* return icom error */
	int (*usb_write)(struct _icoms *p,
		usb_cancelt *cancelt,	/* Optionaly tokem that can be used to cancel */
		int ep,					/* End point address */
		unsigned char *wbuf,	/* Write buffer */
		int wsize,				/* Bytes to or write */
		int *bwritten,			/* Bytes written */
		double tout);			/* Timeout in seconds */

	/* Cancel a read/write in another thread */
	/* return icom error */
	int (*usb_cancel_io)(struct _icoms *p,
		usb_cancelt *cantelt);			/* Cancel handle */

	/* Reset and end point toggle state to 0 */
	/* return icom error */
	int (*usb_resetep)(struct _icoms *p,
		int ep);				/* End point address */

	/* Clear an end point halt */
	/* return icom error */
	int (*usb_clearhalt)(struct _icoms *p,
		int ep);				/* End point address */

	/* For an HID device, read a message from the device. */
	/* return icom error */
	int (*hid_read)(struct _icoms *p,
		unsigned char *buf,		/* Read buffer */
		int bsize,				/* Bytes to read or write */
		int *bread,				/* Bytes read */
		double tout);			/* Timeout in seconds */

	/* For an HID device, write a message to the device */
	/* return icom error */
	int (*hid_write)(struct _icoms *p,
		unsigned char *wbuf,	/* Write buffer */
		int wsize,				/* Bytes to or write */
		int *bwritten,			/* Bytes written */
		double tout);			/* Timeout in seconds */

	/* Destroy ourselves */
	void (*del)(struct _icoms *p);

};

/* Constructor */
extern icoms *new_icoms(icompath *ipath, a1log *log);

/* - - - - - - - - - - - - - - - - - - -- */
/* Utilities */

/* Convert control chars to ^[A-Z] notation in a string */
char *icoms_fix(char *s);

/* Convert a limited binary buffer to a list of hex */
char *icoms_tohex(unsigned char *s, int len);

#ifdef __cplusplus
	}
#endif

#define ICOMS_H
#endif /* ICOMS_H */
