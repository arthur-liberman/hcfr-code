
#ifndef ICOMS_H

/* An abstracted instrument serial and USB communication class. */

/* 
 * Argyll Color Management System
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
#include "conv.h"
#endif

#if defined(UNIX_APPLE)
#include <IOKit/usb/IOUSBLib.h>
#endif

#ifdef __cplusplus
	extern "C" {
#endif

#undef QUIET_MEMCHECKERS		/* #define to memset coms read buffers before reading */

/* USB open/handling/buf workaround flags */
typedef enum {
	icomuf_none                = 0x0000,
	icomuf_detach              = 0x0001,	/* Attempt to detach from system driver */
	icomuf_no_open_clear       = 0x0002,	/* Don't send a clear_halt after opening the port */
	icomuf_reset_before_close  = 0x0004,	/* Reset port before closing it */
											/* ??? Could change to reset before open ??? */
	icomuf_resetep_before_read = 0x0008		/* Do a usb_resetep before each ep read */
} icomuflags;

typedef enum {
	icomt_unknown        = 0x0000,

	/* Category of device */
	icomt_instrument     = 0x010000,		/* Color measurement instrument (default) */
	icomt_v3dlut         = 0x020000,		/* A Video 3D cLUT box */
	icomt_vtpg           = 0x040000,		/* A Video test patern generator box */
	icomt_printer        = 0x080000,		/* A printing device */


	icomt_cat_any        = 0x3f0000,		/* Could be any device category */
	icomt_cat_mask       = 0xff0000,		/* Mask for device category */

	/* Type of underlying communication port */
	/* (fastserio driver will have icomt_serial and icomt_usb set) */
	icomt_serial         = 0x000001,		/* Serial port (i.e. has baud rate etc.) */
	icomt_usb            = 0x000002,		/* USB port */
	icomt_hid            = 0x000004,		/* HID USB port */
	icomt_bt             = 0x000008,		/* Bluetooth (non-serial) */
//	icomt_net            = 0x000010,		/* Network Connected */

	icomt_port_mask      = 0x0000ff,		/* Mask for port type */

	/* Attributes */
	icomt_fastserial     = 0x000100,		/* Fast Serial (i.e. USB, fastserio, Bluetooth) */
	icomt_btserial       = 0x000200,		/* Bluetooth serial port */
	icomt_seriallike     = 0x000400,		/* Uses serial message methods, but */
											/* may not be actual icomt_serial. */

	icomt_attr_mask      = 0x00ff00,		/* Mask for port attributes */

	icomt_portattr_mask  = icomt_port_mask | icomt_attr_mask,

	icomt_portattr_all   = icomt_portattr_mask	/* Scan for all port types */

} icom_type;

/* Status bits/return values */
#define ICOM_OK	    0x000000		/* No error */

#define ICOM_NOTS 	0x001000		/* Not supported */
#define ICOM_SIZE	0x002000		/* Request/response size exceeded limits */
#define ICOM_TO		0x004000		/* Timed out, but there may be bytes read/written */
#define ICOM_SHORT	0x008000		/* No timeout but number of bytes wasn't read/written */
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
	int interface;		/* interface number */
#if defined(UNIX_APPLE)
	int pipe;			/* pipe number (1..N, OS X only) */
#endif
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
	int fd;				/* ep file descriptor */
#endif
} usb_ep;

#define ICOM_EP_TYPE_BULK 2
#define ICOM_EP_TYPE_INTERRUPT 3

#include "usbio.h"
#include "hidio.h"

#endif /* ENABLE_USB */

/* - - - - - - - - - - - - - - - - - - - -  */

/* Store information about a possible instrument communication path */
/* (Note a path doesn't have a reference to icompaths or its log) */
struct _icompath {
	devType dtype;				/* Type of device if known */
	char *name;					/* instance description */

	icom_type dctype;			/* Device and com. type */
#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	char *spath;				/* Serial device path */
#endif
#ifdef ENABLE_USB
	int nep;					/* Number of end points */
	unsigned int vid, pid; 		/* USB vendor and product id's */
	struct usb_idevice *usbd;	/* USB port, NULL if not USB */
	struct hid_idevice *hidd;	/* HID port, NULL if not HID */
#endif /* ENABLE_USB */
};

extern icompath icomFakeDevice;	/* Declare fake device */

/* Device type specific list index */
typedef enum {
	dtix_combined = 0,		/* Combined list */
	dtix_inst,				/* Instrument */
	dtix_3dlut,
	dtix_vtpg,
	dtix_printer,

	dtix_number				/* Number of entries */
} icom_dtix;

/* The available instrument communication paths */
struct _icompaths {
	a1log *log;					/* Verbose, debuge & error logging */

	/* Combined and device category specific path lists (read only). */
	/* Paths may appear on more than one, if they are multi-function, */
	/* or the category can't be determined without opening them. */
	/* (dpaths[dtix_combined] is the owner of the icompath, */
	/* and all the others are only populated after discovery.) */
	icompath **dpaths[dtix_number];		/* Device paths if any */
	int ndpaths[dtix_number];			/* Number of device paths */

	/* Alias of dpaths[dtix_inst] for backwards compatibility, */
	/* only set after discovery. */
	icompath **paths;			/* Device instrument if any */
	int npaths;					/* Number of instrument paths */

	/* Re-discover and populate the available instrument list */
	/* return icom error */
	int (*refresh)(struct _icompaths *p);

	/* Same as above, but just scan for selected types of devices and/or port types */
	int (*refresh_sel)(struct _icompaths *p, icom_type mask);

	/* Return the instrument path corresponding to the port number, or NULL if out of range */
	icompath *(*get_path)(
		struct _icompaths *p, 
		int  	       port);		/* Enumerated port number, 1..n */
#define FAKE_DEVICE_PORT -98		/* Fake display & instrument */
#define DEMO_DEVICE_PORT -99		/* Fake Demo instrument */

	/* Return the device path corresponding to the port number, or NULL if out of range */
	icompath *(*get_path_sel)(
		struct _icompaths *p, 
		icom_dtix dtix,				/* Device type list */
		int  	       port);		/* Enumerated port number, 1..n */

	/* Clear all the device paths */
	void (*clear)(struct _icompaths *p);

	/* We're done */
	void (*del)(struct _icompaths *p);

	/* ====== internal implementation ======= */

	/* Fast serial scan exclusion list - from ARGYLL_EXCLUDE_SERIAL_SCAN env. var. */
	int exno;
	char **exlist;

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	/* Return nz if the serial port is on the fast serial scan exclusion list */
	int (*fs_excluded)(struct _icompaths *p, struct _icompath *path);

	/* Add a serial path to combined. path is copied. Return icom error */
	int (*add_serial)(struct _icompaths *p, char *name, char *spath, icom_type dctype);
#endif /* ENABLE_SERIAL */

#ifdef ENABLE_USB
	/* Add a usb path to combined. usbd is taken, others are copied. Return icom error */
	int (*add_usb)(struct _icompaths *p, char *name, unsigned int vid, unsigned int pid,
	               int nep, struct usb_idevice *usbd, devType dtype);

	/* Add an hid path to combined. hidd is taken, others are copied. Return icom error */
	int (*add_hid)(struct _icompaths *p, char *name, unsigned int vid, unsigned int pid,
	               int nep, struct hid_idevice *hidd, devType dtype);
#endif /* ENABLE_USB */

	/* Delete the last combined path */
	void (*del_last_path)(struct _icompaths *p);

	/* Return the last combined path */
	icompath *(*get_last_path)(struct _icompaths *p);

};

/* Allocate an icom paths and set it to the list of available devices */
/* Use g_log as argument for default logging. */
/* Return NULL on error */
icompaths *new_icompaths(a1log *log);

/* Same as above, but just scan for selected types of devices and/or port types */
icompaths *new_icompaths_sel(a1log *log, icom_type mask);


/* - - - - - - - - - - - */
/* Serial related stuff */

/* Flow control */
typedef enum {
	fc_nc = 0,			/* not configured/default */
	fc_None,
	fc_XonXOff,
	fc_Hardware,		/* RTS CTS flow control */
	fc_HardwareDTR		/* DTR DSR flow control */			
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
	baud_115200  = 12,
	baud_230400  = 13,
	baud_921600  = 14
} baud_rate;

char *baud_rate_to_str(baud_rate br);

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

/* Interrupt callback type */
typedef enum {
	icomi_data_available		/* Data is available to be read */
} icom_int;

/* Cancelation token. */
/* Note that reinit & usb_transaction's must balance! */
typedef struct _usb_cancelt usb_cancelt;

#ifdef ENABLE_USB
void usb_init_cancel(usb_cancelt *p);
void usb_uninit_cancel(usb_cancelt *p);
void usb_reinit_cancel(usb_cancelt *p);
#endif

struct _icoms {
  /* Private: */

	amutex lock;				/* Protect against thread races for some operations */
								/* (i.e. close) */

	/* Copy of some of icompath contents: */
	icom_type dctype;			/* Device cat. and com. type */
	devType dtype;				/* Type of device if known */

	char *name;					/* Device description */
	
	int is_open;				/* Flag, NZ if this port is open */

	void *icntx;				/* Optional instrument context */

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)

	/* Serial port parameters */
	char *spath;				/* Serial port path */
#if defined (MSDOS)
	unsigned short porta;		/* Hardware port address */
#endif
#if defined (NT)
	HANDLE phandle;				/* NT handle */
#endif
#if defined (UNIX)
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
	icomuflags uflags;		/* Bug workaround flags */

	unsigned int vid, pid; 	/* USB vendor and product id's, used to distiguish instruments */
	int nconfig;			/* Number of configurations */
	int config;				/* Config this info applies to (always 1 ?) */
	int cconfig;			/* Current configuration (0 or 1 ?) */
	int nifce;				/* Number of interfaces */
	int nep;				/* Number of end points */
	int wr_ep, rd_ep;		/* Default end points to use for "serial" read/write */
	int rd_qa;				/* Read quanta size */
	int ms_bytes;			/* No. of Modem status bytes to strip from each read */
	int latmsec;			/* Latency timeout in msec for modem status bytes */
	int (*interp_ms)(struct _icoms *p, unsigned char *msbytes);
							/* return icom error from ms bytes, NULL if none */

	usb_ep ep[32];			/* Information about each end point for general usb i/o */

/* Macro to access end point information */
#define EPINFO(addr) ep[((addr >> 3) & 0x10) + (addr & 0x0f)]

	/* HID port parameters */
	struct hid_idevice *hidd;	/* HID port - copy of ppath->hidd */

#endif /* ENABLE_USB */

	/* General parameters */
	int lserr;					/* Last serial communication error code */
	a1log *log;					/* Debug & Error log */
	int debug;					/* legacy - Flag, nz to print instrument io info to stderr */

	/* Linked list to automate SIGKILL cleanup */
	struct _icoms *next;
	
  /* Public: */

	/* Return the device category */
	/* (Returns bit flags) */
	icom_type (*dev_cat)(struct _icoms *p);

	/* Return the communication port type */
	/* (Can use equality tests on return value for normal ports, */
	/* or bit flag for fastserio USB/serial port) */
	icom_type (*port_type)(struct _icoms *p);

	/* Return the communication port attributes */
	/* (Returns bit flags) */
	icom_type (*port_attr)(struct _icoms *p);

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	/* Select the serial communications port and characteristics - extended version */
	/* return icom error */
	int (*set_ser_port_ex)(
		struct _icoms *p, 
		flow_control fc,		/* Flow control */
		baud_rate	 baud,
		parity		 parity,
		stop_bits	 stop_bits,
		word_length	 word_length,
		int delayms);			/* Delay in ms after open */

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
		char *buf,			/* nul terminated unless nch > 0 */
		int nch,			/* if > 0, number of characters to write, else nul terminated */
		double tout);		/* Timeout in seconds */

	/* "Serial" read characters into the buffer */
	/* The returned data will be terminated by a nul. */
	/* return icom error */
	int (*read)(
		struct _icoms *p,
		char *buf,			/* Buffer to store characters read */
		int bsize,			/* Buffer size. Make this larger than chars required! */
		int *bread,			/* Bytes read (not including forced '\000') */
		char *tc,			/* Terminating characters, NULL for none or char count mode */
		int ntc,			/* Number of terminating characters or char count needed. */
		double tout);		/* Timeout in seconds */

	/* "Serial" write and read */
	/* return icom error */
	int (*write_read)(
		struct _icoms *p,
		char *wbuf,			/* Write puffer */
		int nwch,			/* if > 0, number of characters to write, else nul terminated */
		char *rbuf,			/* Read buffer */
		int bsize,			/* Buffer size. Make this larger than chars required! */
		int *bread,			/* Bytes read (not including forced '\000') */
		char *tc,			/* Terminating characers, NULL for none or char count mode */
		int ntc,			/* Number of any terminating characters needed, or char count needed */
		double tout);		/* Timeout in seconds */

	/* "Serial" write and read with read flush */
	/* return icom error */
	int (*write_read_ex)(
		struct _icoms *p,
		char *wbuf,			/* Write puffer */
		int nwch,			/* if > 0, number of characters to write, else nul terminated */
		char *rbuf,			/* Read buffer */
		int bsize,			/* Buffer size. Make this larger than chars required! */
		int *bread,			/* Bytes read (not including forced '\000') */
		char *tc,			/* Terminating characers, NULL for none or char count mode */
		int ntc,			/* Number of any terminating characters needed, or char count needed */
		double tout,		/* Timeout in seconds */
		int frbw);			/* nz to Flush Read Before Write */

	/* For serial device, clear any errors */
	void (*ser_clearerr)(struct _icoms *p);

	/* For a USB device, do a control message */
	/* return icom error */
	int (*usb_control)(struct _icoms *p,
		int requesttype,		/* 8 bit request type (USB bmRequestType) */
		int request,			/* 8 bit request code (USB bRequest) */
		int value,				/* 16 bit value (USB wValue, sent little endian) */
		int index,				/* 16 bit index (USB wIndex, sent little endian) */
		unsigned char *rwbuf,	/* Read or write buffer */
		int rwsize, 			/* Size to write/max bytes to read */
		int *xferred,			/* Bytes written or read (may be NULL) */
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

	/* Wait until a read/write in another thread has started. */
	/* return icom error */
	int (*usb_wait_io)(struct _icoms *p,
		usb_cancelt *cantelt);			/* Cancel handle */

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

	/* Optional callback to client from device */ 
	/* Default implementation is a NOOP */
	int (*interrupt)(struct _icoms *p,
		int icom_int);			/* Interrupt cause */

	/* Destroy ourselves */
	void (*del)(struct _icoms *p);
};

/* Constructor */
extern icoms *new_icoms(icompath *ipath, a1log *log);

/* - - - - - - - - - - - - - - - - - - -- */
/* Utilities */

/* Convert control chars to ^[A-Z] notation in a string. */
/* Returns a maximum of 1000 characters in static buffer. */
char *icoms_fix(char *s);

/* Convert a limited binary buffer to a list of hex */
char *icoms_tohex(unsigned char *s, int len);

/* Implementation declarations */

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)

int icoms_ser_write(icoms *p, char *wbuf, int nwch, double tout);
int icoms_ser_read(icoms *p, char *rbuf, int bsize, int *bread,
                                    char *tc, int ntc, double tout);

#endif

#ifdef __cplusplus
	}
#endif

#define ICOMS_H
#endif /* ICOMS_H */
