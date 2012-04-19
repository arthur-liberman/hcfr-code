
#ifndef ICOMS_H

 /* An abstracted instrument serial and USB communication class. */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   2006/4/20
 *
 * Copyright 1996 - 2007 Graeme W. Gill
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
	suseptable to body results due to an unrecognised aborted
	command. Really, the instrument driver should deterimine
	at what points an operation can be aborted, and how to recover.

	Because the instrument itself can be a source of commands,
	some way of waiting for instrument or user input is needed.
	Could a threaded approach with instrument abort work ?
	
*/

#ifdef ENABLE_USB
# ifdef USE_LIBUSB1
#  include "libusb.h"
# else
#  include "usb.h"
# endif
#endif /* ENABLE_USB */

#ifdef __cplusplus
	extern "C" {
#endif

#undef QUIET_MEMCHECKERS		/* #define to memset coms read buffers before reading */

/* Provide some backwards compatibility to libusb0 */
#ifdef USE_LIBUSB1
# define USB_ENDPOINT_IN        LIBUSB_ENDPOINT_IN
# define USB_ENDPOINT_OUT       LIBUSB_ENDPOINT_OUT
# define USB_RECIP_DEVICE       LIBUSB_RECIPIENT_DEVICE
# define USB_RECIP_INTERFACE    LIBUSB_RECIPIENT_INTERFACE
# define USB_RECIP_ENDPOINT     LIBUSB_RECIPIENT_ENDPOINT
# define USB_TYPE_STANDARD      LIBUSB_REQUEST_TYPE_STANDARD
# define USB_TYPE_CLASS         LIBUSB_REQUEST_TYPE_CLASS
# define USB_TYPE_VENDOR        LIBUSB_REQUEST_TYPE_VENDOR
# define USB_ENDPOINT_TYPE_MASK LIBUSB_TRANSFER_TYPE_MASK

#define usb_device              libusb_device
#define usb_device_descriptor   libusb_device_descriptor
#define usb_dev_handle          libusb_device_handle
#define usb_config_descriptor   libusb_config_descriptor
#define usb_strerror         libusb_strerror
#endif

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
#define ICOM_OK	    0x00000		/* No error */

#define ICOM_USER 	0x10000		/* User abort operation */
#define ICOM_TERM 	0x20000		/* User terminated operation */
#define ICOM_TRIG 	0x30000		/* User trigger */
#define ICOM_CMND 	0x40000		/* User command */
#define ICOM_USERM 	0xF0000		/* Mask of user interrupts */

#define ICOM_NOTS 	0x01000		/* Not supported */
#define ICOM_TO		0x02000		/* Timed out */
#define ICOM_SHORT	0x04000		/* Number of bytes wasn't read/written */

#define ICOM_USBR	0x00100		/* Unspecified USB read error */
#define ICOM_USBW	0x00200		/* Unspecified USB write error */
#define ICOM_SERR	0x00400		/* Unspecified Serial read error */
#define ICOM_SERW	0x00800		/* Unspecified Serial write error */

#define ICOM_XRE	0x00040		/* Xmit shift reg empty */
#define ICOM_XHE	0x00020		/* Xmit hold reg empty */
#define ICOM_BRK	0x00010		/* Break detected */
#define ICOM_FER	0x00008		/* Framing error */
#define ICOM_PER	0x00004		/* Parity error */
#define ICOM_OER	0x00002		/* Overun error */
#define ICOM_DRY	0x00001		/* Recv data ready */

/* Store information about a possible instrument communication path */
typedef struct {
	char *path;					/* Serial port path, or USB/HID device description */
#ifdef ENABLE_USB
	unsigned int vid, pid; 		/* USB vendor and product id's */
	struct usb_device *dev;		/* USB port, NULL if not USB */
	struct _hid_device *hev;	/* HID port, NULL if not HID */
	instType itype;				/* Type of instrument on the USB or HID port */
#endif /* ENABLE_USB */
} icompath;

#ifdef ENABLE_USB

/* Information about each end point */
typedef struct {
	int valid;			/* Flag, nz if this endpoint is valid */
	int addr;			/* Address of end point */
	int packetsize;		/* The max packet size */
	int type;			/* 2 = bulk, 3 = interrupt */	
} usb_ep;

#define ICOM_EP_TYPE_BULK 2
#define ICOM_EP_TYPE_INTERRUPT 3

#endif

#ifdef __cplusplus
	}
#endif

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

#ifdef __cplusplus
	extern "C" {
#endif

struct _icoms {
  /* Private: */
	icompath *ppath;			/* path to selected port */
	
	int port;					/* io port number: 1, 2, 3, 4 .. n */
	int is_open;				/* Flag, NZ if this port is open */
	int is_usb;					/* Flag, NZ if this is a USB port */
	int is_hid;					/* Flag, NZ if this is an HID port */

	/* Serial port parameters */
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

#ifdef ENABLE_USB
	/* USB port parameters */
#ifdef USE_LIBUSB1
	libusb_context *ctx; 
#endif
	unsigned int vid, pid; 		/* USB vendor and product id's */
	struct usb_device *usbd;
	struct usb_dev_handle *usbh;
	int cnfg;				/* Configuration */
	int nifce;				/* Number of interfaces */
	int wr_ep, rd_ep;		/* Default end points to use for "serial" read/write */
	int rd_qa;				/* Read quanta size */

	usb_ep ep[32];			/* Information about each end point for general usb i/o */
	icomuflags uflags;		/* Bug workaround flags */

/* Macro to access end point information */
#define EPINFO(addr) ep[((addr >> 3) & 0x10) + (addr & 0x0f)]

	/* HID port parameters */
	struct _hid_device *hidd;	/* HID port, NULL if not HID */

#endif /* ENABLE_USB */

	/* General parameters */
	int lerr;					/* Last error code */
	int tc;						/* Current serial parser termination character (-1 if not set) */
	int npaths;					/* Number of paths */
	icompath **paths;			/* Paths if any */
	int debug;					/* Flag, nz to print instrument io info to stderr */

	int uih[256];				/* User interrupt handling key table. Value can be: */
								/* ICOM_OK, ICOM_USER, ICOM_TERM, ICOM_TRIG, ICOM_CMND */
	int cut;					/* The character that caused the termination */

	/* Linked list to automate SIGKILL cleanup */
	struct _icoms *next;
	
  /* Public: */

	/* Return a list of instrument io communication paths */
	/* Return NULL on failure. */
	icompath ** (*get_paths)(		/* Return pointer to list of icompath */
		struct _icoms *p 			/* End is marked with NULL pointer, */
	);								/* Storage will be freed with icoms object */

	/* Return the port type */
	icom_type (*port_type)(struct _icoms *p);

	/* Return the instrument type if the port number is USB, instUnknown if not */
	instType (*is_usb_portno)(
		struct _icoms *p, 
		int 	 	 port);		/* Enumerated port number, 1..n */

	/* Return the instrument type if the port number is HID, */
	/* and instUnknown if it is not. */
	instType (*is_hid_portno)(
		struct _icoms *p, 
		int port);		/* Enumerated port number, 1..n */

	/* Select the serial communications port and characteristics */
	void (*set_ser_port)(
		struct _icoms *p, 
		int 	 	 port,		/* Enumerated port number, 1..n */
		flow_control fc,		/* Flow control */
		baud_rate	 baud,
		parity		 parity,
		stop_bits	 stop_bits,
		word_length	 word_length);

	/* Select the USB communications port and characteristics */
	void (*set_usb_port)(
		struct _icoms *p, 
		int            port,		/* Enumerated port number, 1..n */
	    int            config,		/* Configuration */
	    int            wr_ep,		/* "serial" write end point */
	    int            rd_ep,		/* "serial" read end point */
		icomuflags usbflags,		/* Any special handling flags */
		int retries,				/* > 0 if we should retry set_configuration (100msec) */
		char **pnames				/* List of process names to try and kill before opening */
	);

	/* Select the HID communications port and characteristics */
	void (*set_hid_port)(
		struct _icoms *p, 
		int            port,		/* Enumerated port number, 1..n */
		icomuflags usbflags,		/* Any special handling flags */
		int retries,				/* > 0 if we should retry set_configuration (100msec) */
		char **pnames				/* List of process names to try and kill before opening */
	);

	/* Close the port */
	void (*close_port)(struct _icoms *p);

	/* Reset user interrupt handling to default (Esc, ^C, q or 'Q' = Abort) */
	void (*reset_uih)(struct _icoms *p);

	/* Set a key range to the given handling type */
	/* min & max are between 0 and 255, status is one of */
	/* ICOM_OK, ICOM_USER, ICOM_TERM, ICOM_TRIG, ICOM_CMND */
	void (*set_uih)(struct _icoms *p, int min, int max, int status);

	/* Get the character that caused the user interrupt */
	/* Clear it to 0x00 after reading it. */
	int (*get_uih_char)(struct _icoms *p);

	/* "Serial" write the characters in the buffer out */
	/* Data will be written up to the terminating nul. */
	/* Return error code & set error state. */
	int (*write)(
		struct _icoms *p,
		char *buf,
		double tout);		/* Timeout in seconds */

	/* "Serial" read characters into the buffer */
	/* Return error code & set error state. */
	/* The returned data will be terminated by a nul. */
	int (*read)(
		struct _icoms *p,
		char *buf,			/* Buffer to store characters read */
		int bsize,			/* Buffer size */
		char tc,			/* Terminating character */
		int ntc,			/* Number of terminating characters */
		double tout);		/* Timeout in seconds */

	/* "Serial" write and read */
	/* Return error code & set error state. */
	int (*write_read)(
		struct _icoms *p,
		char *wbuf,			/* Write puffer */
		char *rbuf,			/* Read buffer */
		int bsize,			/* Buffer size */
		char tc,			/* Terminating characer */
		int ntc,			/* Number of terminating characters */
		double tout);		/* Timeout in seconds */

	/* For a USB device, do a control message */
	/* Return error code (don't set error state). */
	int (*usb_control_th)(struct _icoms *p,
		int requesttype,		/* 8 bit request type (USB bmRequestType) */
		int request,			/* 8 bit request code (USB bRequest) */
		int value,				/* 16 bit value (USB wValue, sent little endian) */
		int index,				/* 16 bit index (USB wIndex, sent little endian) */
		unsigned char *rwbuf,	/* Read or write buffer */
		int rwsize,				/* Bytes to read or write */
		double tout,			/* Timeout in seconds */
		int debug,				/* debug flag value */
		int *cut,				/* Character that caused termination */
		int checkabort);		/* check for user abort */

	/* Same as above, but set error state */
	int (*usb_control)(struct _icoms *p,
		int requesttype,		/* 8 bit request type (USB bmRequestType) */
		int request,			/* 8 bit request code (USB bRequest) */
		int value,				/* 16 bit value (USB wValue, sent little endian) */
		int index,				/* 16 bit index (USB wIndex, sent little endian) */
		unsigned char *rwbuf,	/* Read or write buffer */
		int rwsize,				/* Bytes to read or write */
		double tout);			/* Timeout in seconds */

	/* For a USB device, do a bulk or interrupt read from an end point */
	/* Return error code (don't set error state). */
	int (*usb_read_th)(struct _icoms *p,
		void **hcancel,			/* Optionaly return handle to allow cancel */
		int ep,					/* End point address */
		unsigned char *buf,		/* Read buffer */
		int bsize,				/* Bytes to read or write */
		int *bread,				/* Bytes read */
		double tout,			/* Timeout in seconds */
		int debug,				/* debug flag value */
		int *cut,				/* Character that caused termination */
		int checkabort);		/* check for user abort */

	/* Same as above, but set error state */
	int (*usb_read)(struct _icoms *p,
		int ep,					/* End point address */
		unsigned char *buf,		/* Read buffer */
		int bsize,				/* Bytes to read or write */
		int *bread,				/* Bytes read */
		double tout);			/* Timeout in seconds */

	/* For a USB device, do a bulk or interrupt write to an end point */
	/* Return error code (don't set error state). */
	int (*usb_write_th)(struct _icoms *p,
		void **hcancel,			/* Optionaly return handle to allow cancel */
		int ep,					/* End point address */
		unsigned char *wbuf,	/* Write buffer */
		int wsize,				/* Bytes to or write */
		int *bwritten,			/* Bytes written */
		double tout,			/* Timeout in seconds */
		int debug,				/* debug flag value */
		int *cut,				/* Character that caused termination */
		int checkabort);		/* check for user abort */

	/* Same as above, but set error state */
	int (*usb_write)(struct _icoms *p,
		int ep,					/* End point address */
		unsigned char *wbuf,	/* Write buffer */
		int wsize,				/* Bytes to or write */
		int *bwritten,			/* Bytes written */
		double tout);			/* Timeout in seconds */

	/* Cancel a read/write in another thread */
	int (*usb_cancel_io)(struct _icoms *p,
		void *hcancel);			/* Cancel handle */

	/* Reset and end point toggle state to 0 */
	int (*usb_resetep)(struct _icoms *p,
		int ep);				/* End point address */

	/* Clear an end point halt */
	int (*usb_clearhalt)(struct _icoms *p,
		int ep);				/* End point address */

	/* For an HID device, read a message from the device - thread friendly. */
	/* Set error state on error */
	int (*hid_read_th)(struct _icoms *p,
		unsigned char *buf,		/* Read buffer */
		int bsize,				/* Bytes to read or write */
		int *bread,				/* Bytes read */
		double tout,			/* Timeout in seconds */
		int debug,				/* debug flag value */
		int *cut,				/* Character that caused termination */
		int checkabort);		/* Check for abort from keyboard */

	/* For an HID device, read a message from the device. */
	/* Set error state on error */
	int (*hid_read)(struct _icoms *p,
		unsigned char *buf,		/* Read buffer */
		int bsize,				/* Bytes to read or write */
		int *bread,				/* Bytes read */
		double tout);			/* Timeout in seconds */

	/* For an HID device, write a message to the device - thread friendly. */
	/* Set error state on error */
	int (*hid_write_th)(struct _icoms *p,
		unsigned char *wbuf,	/* Write buffer */
		int wsize,				/* Bytes to or write */
		int *bwritten,			/* Bytes written */
		double tout,			/* Timeout in seconds */
		int debug,				/* debug flag value */
		int *cut,				/* Character that caused termination */
		int checkabort);		/* Check for abort from keyboard */

	/* For an HID device, write a message to the device */
	/* Set error state on error */
	int (*hid_write)(struct _icoms *p,
		unsigned char *wbuf,	/* Write buffer */
		int wsize,				/* Bytes to or write */
		int *bwritten,			/* Bytes written */
		double tout);			/* Timeout in seconds */

	/* Destroy ourselves */
	void (*del)(struct _icoms *p);

}; typedef struct _icoms icoms;

/* Constructor */
extern icoms *new_icoms(void);

/* - - - - - - - - */
/* Other functions */

/* Poll for a user abort, terminate, trigger or command. */
/* Wait for a key rather than polling, if wait != 0 */
/* Return: */
/* ICOM_OK if no key has been hit, */
/* ICOM_USER if User abort has been hit, */
/* ICOM_TERM if User terminate has been hit. */
/* ICOM_TRIG if User trigger has been hit */
/* ICOM_CMND if User command has been hit */
int icoms_poll_user(icoms *p, int wait);

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
