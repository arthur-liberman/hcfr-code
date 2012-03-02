
 /* Unix serial I/O class */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   18/11/2000
 *
 * Copyright 1997 - 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/*
    TTBD:
 */

#ifdef UNIX

#include <sys/types.h>		/* Include sys/select.h ? */
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <string.h>
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

#undef DEBUG

#ifdef DEBUG
# define errout stderr
# define DBG(xx)	fprintf(errout, xx )
# define DBGF(xx)	fprintf xx
#else
# define errout stderr
# define DBG(xx)
# define DBGF(xx)
#endif

/* select() defined, but not poll(), so emulate poll() */
#if defined(FD_CLR) && !defined(POLLIN)
#include "pollem.h"
#define poll_x pollem
#else
#include <sys/poll.h>	/* Else assume poll() is native */
#define poll_x poll
#endif

#ifdef __APPLE__
//#include <stdbool.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOBSD.h>
#include <mach/mach_init.h>
#include <mach/task_policy.h>
#endif /* __APPLE__ */

/* Return the port type */
static icom_type icoms_port_type(
icoms *p
) {
	if (p->is_hid)
		return icomt_hid;
	if (p->is_usb)
		return icomt_usb;
	return icomt_serial;
}

/* Create and return a list of available serial ports or USB instruments for this system */
static icompath **
icoms_get_paths(
icoms *p 
) {
	int usbend = 0;
	int i,j;

	/* Free any old list */
	if (p->paths != NULL) {
		for (i = 0; i < p->npaths; i++) {
			if (p->paths[i]->path != NULL)
				free(p->paths[i]->path);
#ifdef ENABLE_USB
			if (p->paths[i]->dev != NULL)
				usb_del_usb_device(p->paths[i]->dev);
			if (p->paths[i]->hev != NULL)
				hid_del_hid_device(p->paths[i]->hev);
#endif /* ENABLE_USB */
			free(p->paths[i]);
		}
		free(p->paths);
		p->npaths = 0;
		p->paths = NULL;
	}

	hid_get_paths(p);
	usb_get_paths(p);
	usbend = p->npaths;

#ifdef ENABLE_SERIAL
#ifdef __APPLE__
	/* Search the OSX registry for serial ports */
	{
	    kern_return_t kstat; 
	    mach_port_t mp;						/* Master IO port */
	    CFMutableDictionaryRef sdict;		/* Serial Port  dictionary */
		io_iterator_t mit;					/* Matching itterator */
		io_object_t ioob;					/* Serial object found */

		/* Get dictionary of serial ports */
    	if ((sdict = IOServiceMatching(kIOSerialBSDServiceValue)) == NULL) {
        	warning("IOServiceMatching returned a NULL dictionary");
		}

		/* Set value to match to RS232 type serial */
        CFDictionarySetValue(sdict, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDRS232Type));

		/* Init itterator to find matching types */
		if ((kstat = IOServiceGetMatchingServices(kIOMasterPortDefault, sdict, &mit))
		                                                                     != KERN_SUCCESS) 
        	error("IOServiceGetMatchingServices returned %d\n", kstat);

		/* Find all the matching serial ports */
		for (;;) {
			char pname[100];
			
	        CFTypeRef dfp;		/* Device file path */

		    if ((ioob = IOIteratorNext(mit)) == 0)
				break;

		    /* Get the callout device's path (/dev/cu.xxxxx). */
			if ((dfp = IORegistryEntryCreateCFProperty(ioob, CFSTR(kIOCalloutDeviceKey),
			                                      kCFAllocatorDefault, 0)) == NULL)
				goto continue1;

			/* Convert from CF string to C string */
			if (!CFStringGetCString(dfp, pname, 100, kCFStringEncodingASCII))
				goto continue2;

			/* Ignore infra red port or Bluetooth, or any other noise */
			if (strstr(pname, "IrDA") != NULL
			 || strstr(pname, "Dialup") != NULL
			 || strstr(pname, "Bluetooth") != NULL)
				goto continue2;

			/* Add the port to the list */
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
			if ((p->paths[p->npaths]->path = strdup(pname)) == NULL)
				error("icoms: strdup failed!");
#ifdef ENABLE_USB
			p->paths[p->npaths]->dev = NULL;
			p->paths[p->npaths]->hev = NULL;
#endif /* ENABLE_USB */
			p->npaths++;
			p->paths[p->npaths] = NULL;

		continue2:
            CFRelease(dfp);
		continue1:
		    IOObjectRelease(ioob);		/* Release found object */
		}
	    IOObjectRelease(mit);			/* Release the itterator */
		/* Don't have to release sdict ? */
	}
#else
	/* Other UNIX like systems */
	/* Many are crude and list every available device name, whether */
	/* it's usable or not. Do any UNIX systems have a mechanism for listing */
	/* serial ports ?? */

	/* On Linux, the list in /proc/tty/driver/serial may indicate */
	/* which are real or not (if "uart:unknown" then not real) */
	/* e.g.:

		0: uart:16550A port:000003F8 irq:4 tx:3 rx:1755 brk:1 RTS|DTR
		1: uart:16550A port:000002F8 irq:3 tx:11 rx:3 brk:3
		2: uart:unknown port:000003E8 irq:4
		3: uart:unknown port:000002E8 irq:3
		4: uart:unknown port:00000000 irq:0
		5: uart:unknown port:00000000 irq:0
		6: uart:unknown port:00000000 irq:0
		7: uart:unknown port:00000000 irq:0

		but the permissions don't allow looking at this.
	 */
	/* (This info is similar to what is returned by "setserial -g /dev/ttyS*", */
	/*  and "setserial -gb /dev/ttyS*" returns just the real ports.) */
	/* None of this can distinguish if one is the mouse. */

	/* From "QTSerialPort": */
	/*
		Constant         Used By         Naming Convention
		----------       -------------   ------------------------
		_TTY_WIN_        Windows         COM1, COM2
		_TTY_IRIX_       SGI/IRIX        /dev/ttyf1, /dev/ttyf2
		_TTY_HPUX_       HP-UX           /dev/tty1p0, /dev/tty2p0
		_TTY_SUN_        SunOS/Solaris   /dev/ttya, /dev/ttyb
		_TTY_DIGITAL_    Digital UNIX    /dev/tty01, /dev/tty02
		_TTY_FREEBSD_    FreeBSD         /dev/ttyd0, /dev/ttyd1
		_TTY_LINUX_      Linux           /dev/ttyS0, /dev/ttyS1
		<none>           Linux           /dev/ttyS0, /dev/ttyS1
		                 Linux           /dev/ttyUSB0, /dev/ttyUSB1
	*/

	/*
		"Most program set a lock in /var/lock/LCK..tty<XX> on Linux ?
		<http://sunsite.ualberta.ca/LDP/LDP/nag2/x-087-2-serial.devices.html>
		<http://docs.freebsd.org/info/uucp/uucp.info.UUCP_Lock_Files.html>

		We should really use the lock files to avoid treading on
		other programs toes. We assume at the moment that the user
		only picks a serial port with an instrument on it.
	*/

	/* Search for devices that match the pattern /dev/ttyS[0-9]* and /dev/ttyUSB* */
	{
		DIR *dd;
		struct dirent *de;
		char *dirn = "/dev/";
	
		if ((dd = opendir(dirn)) == NULL) {
//			DBGF((errout,"failed to open directory \"%s\"\n",dirn));
			if (p->debug) fprintf(errout,"failed to open directory \"%s\"\n",dirn);
			return p->paths;
		}

		for (;;) {
			int fd;
			char *dpath;

			if ((de = readdir(dd)) == NULL)
				break;

			if (!(
#ifdef __FreeBSD__
			   /* This should match uart & USB devs. */
				( strncmp (de->d_name, "cua", 3) == 0
				&& strlen (de->d_name) < 7)
#else
				/* Presumably Linux.. */
			    (   strncmp(de->d_name, "ttyS", 4) == 0
			     && de->d_name[4] >= '0' && de->d_name[4] <= '9')
			 || (   strncmp(de->d_name, "ttyUSB", 5) == 0)
#endif
			))
				continue;

			if ((dpath = (char *)malloc(strlen(dirn) + strlen(de->d_name) + 1)) == NULL) {
				closedir(dd);
				error("icoms: malloc failed!");
			}
			strcpy(dpath, dirn);
			strcat(dpath, de->d_name);

			/* See if the serial port is real */
			if (strncmp(de->d_name, "ttyUSB", 5) != 0) {

				/* Hmm. This is probably a bad idea - it can upset other */
				/* programs that use the serial ports ? */
				if ((fd = open(dpath, O_RDONLY | O_NOCTTY | O_NONBLOCK)) < 0) {
					if (p->debug) fprintf(errout,"failed to open serial \"%s\"\n",dpath);
					free(dpath);
					continue;
				}
				/* On linux we could do a 
					struct serial_struct serinfo;
	
					serinfo.reserved_char[0] = 0;
	
					if (ioctl(fd, TIOCGSERIAL, &serinfo) < 0
						|| serinfo.type == PORT_UNKNOWN) {
						free(dpath);
						continue;
					}
	
				 */
				close(fd);
			}
			if (p->debug) fprintf(errout,"managed to open serial \"%s\"\n",dpath);

			/* Add the path to the list */
			if (p->paths == NULL) {
				if ((p->paths = (icompath **)calloc(sizeof(icompath *), 1 + 1)) == NULL) {
					free(dpath);
					closedir(dd);
					error("icoms: calloc failed!");
				}
			} else {
				if ((p->paths = (icompath **)realloc(p->paths,
				                     sizeof(icompath *) * (p->npaths + 2))) == NULL) {
					free(dpath);
					closedir(dd);
					error("icoms: realloc failed!");
				}
				p->paths[p->npaths+1] = NULL;
			}
			if ((p->paths[p->npaths] = malloc(sizeof(icompath))) == NULL) {
				free(dpath);
				closedir(dd);
				error("icoms: malloc failed!");
			}
			p->paths[p->npaths]->path = dpath;
#ifdef ENABLE_USB
			p->paths[p->npaths]->dev = NULL;
			p->paths[p->npaths]->hev = NULL;
#endif /* ENABLE_USB */
			p->npaths++;
			p->paths[p->npaths] = NULL;
		}
		closedir(dd);
	}
#endif /* ! __APPLE__ */
#endif /* ENABLE_SERIAL */

	/* Sort the /dev keys so people don't get confused... */
	for (i = usbend; i < (p->npaths-1); i++) {
		for (j = i+1; j < p->npaths; j++) {
			if (strcmp(p->paths[i]->path, p->paths[j]->path) > 0) {
				icompath *tt = p->paths[i];
				p->paths[i] = p->paths[j];
				p->paths[j] = tt;
			}
		}
	}

	return p->paths;
}


/* Close the port */
static void icoms_close_port(icoms *p) {
	if (p->is_open) {
		if (p->is_usb) {
			usb_close_port(p);
		} else if (p->is_hid) {
			hid_close_port(p);
		} else {
			if (p->fd != -1)
				close(p->fd);
				p->fd = -1;
		}
		p->is_open = 0;
	}
}

static int icoms_ser_write(icoms *p, char *wbuf, double tout);
static int icoms_ser_read(icoms *p, char *rbuf, int bsize, char tc, int ntc, double tout);

/* Set the serial port number and characteristics */
static void
icoms_set_ser_port(
icoms *p, 
int 	 	 port,		/* serial com port, 1 - N. 0 for no change */
flow_control fc,
baud_rate	 baud,
parity		 parity,
stop_bits	 stop,
word_length	 word
) {
	struct termios tio;
	speed_t speed = 0;

	if (p->debug) {
		fprintf(stderr,"icoms: About to set port characteristics:\n");
		fprintf(stderr,"       Port = %d\n",port);
		fprintf(stderr,"       Flow control = %d\n",fc);
		fprintf(stderr,"       Baud Rate = %d\n",baud);
		fprintf(stderr,"       Parity = %d\n",parity);
		fprintf(stderr,"       Stop bits = %d\n",stop);
		fprintf(stderr,"       Word length = %d\n",word);
	}

	if (port >= 1) {
		if (p->is_open && port != p->port) {	/* If port number changes */
			p->close_port(p);
		}
	}

	if (p->is_usb_portno(p, port) == instUnknown) {

		if (fc != fc_nc)
			p->fc = fc;
		if (baud != baud_nc)
			p->br = baud;
		if (parity != parity_nc)
			p->py = parity;
		if (stop != stop_nc)
			p->sb = stop;
		if (word != length_nc)
			p->wl = word;

		/* Make sure the port is open */
		if (!p->is_open) {

			if (p->ppath != NULL) {
				if (p->ppath->path != NULL)
					free(p->ppath->path);
				free(p->ppath);
				p->ppath = NULL;
			}

			if (p->paths == NULL)
				icoms_get_paths(p);
		
			if (port <= 0 || port > p->npaths)
				error("icoms - set_ser_port: port number out of range!");

			if ((p->ppath = malloc(sizeof(icompath))) == NULL)
				error("malloc() failed on com port path");
			*p->ppath = *p->paths[port-1];				/* Structure copy */
			if ((p->ppath->path = strdup(p->paths[port-1]->path)) == NULL)
				error("strdup() failed on com port path");
			p->port = port;

			if (p->debug) fprintf(stderr,"icoms: About to open port '%s'\n",p->ppath->path);

			if ((p->fd = open(p->ppath->path, O_RDWR | O_NOCTTY )) < 0)
				error("Opening COM port '%s' failed with '%s'",p->ppath->path, strerror(errno));
			/* O_NONBLOCK O_SYNC */
			if (p->debug) fprintf(stderr,"icoms: Opened port OK, fd = %d\n",p->fd);
			p->is_open = 1;
		}

		if (tcgetattr(p->fd, &tio) < 0) {
			error("tcgetattr failed with '%s' on serial port '%s'", strerror(errno),p->ppath->path);
		}

		/* Clear everything in the tio, and just set what we want */
		memset(&tio, 0, sizeof(struct termios));

		/* Turn on basic configuration: */
		tio.c_iflag |= (
					  IGNBRK		/* Ignore Break */
					   );

		tio.c_oflag |= ( 0 );

		tio.c_cflag |= (
					  CREAD		/* Enable the receiver */
					| CLOCAL	/* Ignore modem control lines */
						);

		tio.c_lflag |= (
					     0		/* Non-canonical input mode */
						);

		/* And configure: */
		tio.c_cc[VTIME] = 1;		/* 0.1 second timeout */
		tio.c_cc[VMIN] = 64;		/* Comfortably less than _PF_MAX_INPUT */

		switch (p->fc) {
			case fc_nc:
				error("icoms - set_ser_port: illegal flow control!");
				break;
			case fc_XonXOff:
				/* Use Xon/Xoff bi-directional flow control */
				tio.c_iflag |= IXON;		/* Enable XON/XOFF flow control on output */
				tio.c_iflag |= IXOFF;		/* Enable XON/XOFF flow control on input */
				tio.c_cc[VSTART] = 0x11;	/* ^Q */
				tio.c_cc[VSTOP] = 0x13;		/* ^S */
				break;
			case fc_Hardware:
				/* Use RTS/CTS bi-directional flow control */
#ifdef __APPLE__
				tio.c_cflag |= CCTS_OFLOW;
				tio.c_cflag |= CRTS_IFLOW;
#else
				tio.c_cflag |= CRTSCTS;
#endif
				break;
			default:
				break;
		}

		switch (p->py) {
			case parity_nc:
				error("icoms - set_ser_port: illegal parity setting!");
				break;
			case parity_none:
				tio.c_iflag &= ~INPCK;		/* Disable input parity checking */
				break;
			case parity_odd:
				tio.c_iflag |= INPCK;		/* Enable input parity checking */
				tio.c_cflag |= PARENB;		/* Enable input and output parity checking */
				tio.c_cflag |= PARODD;		/* Input and output parity is odd */
				break;
			case parity_even:
				tio.c_iflag |= INPCK;		/* Enable input parity checking */
				tio.c_cflag |= PARENB;		/* Enable input and output parity checking */
				break;
		}

		switch (p->sb) {
			case stop_nc:
				error("icoms - set_ser_port: illegal stop bits!");
				break;
			case stop_1:
				break;		/* defaults to 1 */
			case stop_2:
				tio.c_cflag |= CSTOPB;
				break;
		}

		switch (p->wl) {
			case length_nc:
				error("icoms - set_ser_port: illegal word length!");
			case length_5:
				tio.c_cflag |= CS5;
				break;
			case length_6:
				tio.c_cflag |= CS6;
				break;
			case length_7:
				tio.c_cflag |= CS7;
				break;
			case length_8:
				tio.c_cflag |= CS8;
				break;
		}

		/* Set the baud rate */
		switch (p->br) {
			case baud_110:
				speed = B110;
				break;
			case baud_300:
				speed = B300;
				break;
			case baud_600:
				speed = B600;
				break;
			case baud_1200:
				speed = B1200;
				break;
			case baud_2400:
				speed = B2400;
				break;
			case baud_4800:
				speed = B4800;
				break;
			case baud_9600:
				speed = B9600;
				break;
			case baud_19200:
				speed = B19200;
				break;
			case baud_38400:
				speed = B38400;
				break;
			case baud_57600:
				speed = B57600;
				break;
			case baud_115200:
				speed = B115200;
				break;
			default:
				error("icoms - set_ser_port: illegal baud rate!");
				break;
		}

		tcflush(p->fd, TCIOFLUSH);			/* Discard any current in/out data */

		if (cfsetispeed(&tio,  speed) < 0)
			error("cfsetispeed failed with '%s'", strerror(errno));
		if (cfsetospeed(&tio,  speed) < 0)
			error("cfsetospeed failed with '%s'", strerror(errno));

		/* Make change immediately */
		if (tcsetattr(p->fd, TCSANOW, &tio) < 0)
			error("tcsetattr failed with '%s' on '%s'", strerror(errno), p->ppath->path);

		tcflush(p->fd, TCIOFLUSH);			/* Discard any current in/out data */

		p->write = icoms_ser_write;
		p->read = icoms_ser_read;

	}
	if (p->debug) fprintf(stderr,"icoms: port characteristics set ok\n");
}

/* ---------------------------------------------------------------------------------*/
/* Serial write/read */

/* Write the characters in the buffer out */
/* Data will be written up to the terminating nul */
/* Return relevant error status bits */
static int
icoms_ser_write(
icoms *p,
char *wbuf,
double tout
) {
	int len, wbytes;
	long toc, i, top;		/* Timout count, counter, timeout period */
	struct pollfd pa[2];		/* Poll array to monitor serial write and stdin */
	struct termios origs, news;

	if (p->debug) fprintf(stderr,"About to write '%s' ",icoms_fix(wbuf));
	if (p->fd == -1)
		error("icoms_write: not initialised");

	/* Configure stdin to be ready with just one character */
	if (tcgetattr(STDIN_FILENO, &origs) < 0)
		error("tcgetattr failed with '%s' on stdin", strerror(errno));
	news = origs;
	news.c_lflag &= ~(ICANON | ECHO);
	news.c_cc[VTIME] = 0;
	news.c_cc[VMIN] = 1;
	if (tcsetattr(STDIN_FILENO,TCSANOW, &news) < 0)
		error("tcsetattr failed with '%s' on stdin", strerror(errno));

	/* Wait for serial output not block */
	pa[0].fd = p->fd;
	pa[0].events = POLLOUT;
	pa[0].revents = 0;

	/* Wait for stdin to have a character */
	pa[1].fd = STDIN_FILENO;
	pa[1].events = POLLIN | POLLPRI;
	pa[1].revents = 0;

	/* Until timed out, aborted, or transmitted */
	len = strlen(wbuf);
	tout *= 1000.0;		/* Timout in msec */
	p->lerr = 0;

	top = 100;						/* Timeout period in msecs */
	toc = (int)(tout/top + 0.5);	/* Number of timout periods in timeout */
	if (toc < 1)
		toc = 1;

	/* Until data is all written, we time out, or the user aborts */
	for(i = toc; i > 0 && len > 0;) {
		if (poll_x(pa, 2, top) > 0) {
			if (pa[0].revents != 0) {
				if (pa[0].revents != POLLOUT)
					error("poll on serial out returned unexpected value 0x%x",pa[0].revents);
				
				/* We can write it without blocking */
				if ((wbytes = write(p->fd, wbuf, len)) < 0) {
					p->lerr |= ICOM_SERW;
					break;
				} else if (wbytes > 0) {
					i = toc;
					len -= wbytes;
					wbuf += wbytes;
				}
			}
			if (pa[1].revents != 0) {
				char tb[10];
				if (pa[1].revents != POLLIN && pa[1].revents != POLLPRI)
					error("poll on stdin returned unexpected value 0x%x",pa[1].revents);
				/* Check for user abort */
				if (read(STDIN_FILENO, tb, 10) > 0 && p->uih[tb[0]] != ICOM_OK) {
					p->cut = tb[0];
					p->lerr = p->uih[tb[0]];
					if (p->uih[tb[0]] == ICOM_USER
					 || p->uih[tb[0]] == ICOM_TERM
					 || p->uih[tb[0]] == ICOM_TRIG
					 || p->uih[tb[0]] == ICOM_CMND)
						break;
				}
			}
		} else {
			i--;		/* timeout (or error!) */
		}
	}
	if (i <= 0) {		/* Timed out */
		p->lerr |= ICOM_TO;
	}

	/* Restore stdin */
	if (tcsetattr(STDIN_FILENO, TCSANOW, &origs) < 0)
		error("tcsetattr failed with '%s' on stdin", strerror(errno));

	if (p->debug) fprintf(stderr,"ICOM err 0x%x\n",p->lerr);
	return p->lerr;
}

/* Read characters into the buffer */
/* Return string will be terminated with a nul */
static int
icoms_ser_read(
icoms *p,
char *rbuf,			/* Buffer to store characters read */
int bsize,			/* Buffer size */
char tc,			/* Terminating characer */
int ntc,			/* Number of terminating characters */
double tout			/* Time out in seconds */
) {
	int rbytes;
	long j, toc, i, top;		/* Timout count, counter, timeout period */
	struct pollfd pa[2];		/* Poll array to monitor serial read and stdin */
	struct termios origs, news;
	char *rrbuf = rbuf;		/* Start of return buffer */

	if (p->debug) fprintf(stderr,"icoms: Read called\n");
	if (p->fd == -1)
		error("icoms_read: not initialised");

	if (bsize < 3)
		error("icoms_read given too small a buffer");

#ifdef NEVER
	/* The Prolific 2303 USB<->serial seems to choke on this, */
	/* so we just put up with the 100msec delay at the end of each reply. */
	if (tc != p->tc) {	/* Set the termination char */
		struct termios tio;

		if (tcgetattr(p->fd, &tio) < 0)
			error("tcgetattr failed with '%s' on '%s'", strerror(errno),p->ppath->path);

		tio.c_cc[VEOL] = tc;

		/* Make change immediately */
		tcflush(p->fd, TCIFLUSH);
		if (tcsetattr(p->fd, TCSANOW, &tio) < 0)
			error("tcsetattr failed with '%s' on '%s'", strerror(errno),p->ppath->path);

		p->tc = tc;
	}
#endif

	/* Configure stdin to be ready with just one character */
	if (tcgetattr(STDIN_FILENO, &origs) < 0)
		error("ycgetattr failed with '%s' on stdin", strerror(errno));
	news = origs;
	news.c_lflag &= ~(ICANON | ECHO);
	news.c_cc[VTIME] = 0;
	news.c_cc[VMIN] = 1;
	if (tcsetattr(STDIN_FILENO,TCSANOW, &news) < 0)
		error("tcsetattr failed with '%s' on stdin", strerror(errno));

	/* Wait for serial input to have data */
	pa[0].fd = p->fd;
	pa[0].events = POLLIN | POLLPRI;
	pa[0].revents = 0;

	/* Wait for stdin to have a character */
	pa[1].fd = STDIN_FILENO;
	pa[1].events = POLLIN | POLLPRI;
	pa[1].revents = 0;

	bsize--;	/* Allow space for null */
	tout *= 1000.0;		/* Timout in msec */
	p->lerr = 0;

	top = 100;						/* Timeout period in msecs */
	toc = (int)(tout/top + 0.5);	/* Number of timout periods in timeout */
	if (toc < 1)
		toc = 1;

	/* Until data is all read, we time out, or the user aborts */
	for(i = toc, j = 0; i > 0 && bsize > 1 && j < ntc ;) {

		if (poll_x(pa, 2, top) > 0) {
			if (pa[0].revents != 0) {
				if (pa[0].revents != POLLIN &&  pa[0].revents != POLLPRI)
					error("poll on serial in returned unexpected value 0x%x",pa[0].revents);

				/* We have data to read from input */
				if ((rbytes = read(p->fd, rbuf, bsize)) < 0) {
					p->lerr |= ICOM_SERR;
					break;
				} else if (rbytes > 0) {
					i = toc;		/* Reset time */
					bsize -= rbytes;
					while(rbytes--) {	/* Count termination characters */
						if (*rbuf++ == tc)
							j++;
					}
				}
			}
			if (pa[1].revents != 0) {
				char tb[10];
				if (pa[1].revents != POLLIN && pa[1].revents != POLLPRI)
					error("poll on stdin returned unexpected value 0x%x",pa[1].revents);
				/* Check for user abort */
				if (read(STDIN_FILENO, tb, 10) > 0 && p->uih[tb[0]] != ICOM_OK) {
					p->cut = tb[0];
					p->lerr = p->uih[tb[0]];
					if (p->uih[tb[0]] == ICOM_USER
					 || p->uih[tb[0]] == ICOM_TERM
					 || p->uih[tb[0]] == ICOM_TRIG
					 || p->uih[tb[0]] == ICOM_CMND)
						break;
				}
			}
		} else {
			i--;		/* We timed out (or error!) */
		}
	}

	*rbuf = '\000';
	if (i <= 0) {			/* timed out */
		p->lerr |= ICOM_TO;
	}
	if (p->debug) fprintf(stderr,"icoms: About to return read '%s' ICOM err 0x%x\n",icoms_fix(rrbuf),p->lerr);

	/* Restore stdin */
	if (tcsetattr(STDIN_FILENO, TCSANOW, &origs) < 0)
		error("tcsetattr failed with '%s' on stdin", strerror(errno));

	if (p->debug) fprintf(stderr,"icoms: Read returning with 0x%x\n",p->lerr);

	return p->lerr;
}

/* ---------------------------------------------------------------------------------*/

/* Destroy ourselves */
static void
icoms_del(icoms *p) {
	if (p->debug) fprintf(stderr,"icoms: delete called\n");
	if (p->is_open) {
		if (p->debug) fprintf(stderr,"icoms: closing port\n");
		p->close_port(p);
	}
	if (p->paths != NULL) {
		int i;
		for (i = 0; i < p->npaths; i++) {
			if (p->paths[i]->path != NULL)
				free(p->paths[i]->path);
#ifdef ENABLE_USB
			if (p->paths[i]->dev != NULL)
				usb_del_usb_device(p->paths[i]->dev);
			if (p->paths[i]->hev != NULL)
				hid_del_hid_device(p->paths[i]->hev);
#endif /* ENABLE_USB */
			free(p->paths[i]);
		}
		free(p->paths);
	}
	if (p->ppath != NULL) {
		if (p->ppath->path != NULL)
			free(p->ppath->path);
		free(p->ppath);
	}
	free (p);
}

/* Constructor */
icoms *new_icoms() {
	icoms *p;
	if ((p = (icoms *)calloc(sizeof(icoms), 1)) == NULL)
		error("icoms: malloc failed!");

	/* Init things to null values */
	p->fd = -1;
	p->lerr = 0;
	p->ppath = NULL;
	p->port = -1;
	p->br = baud_nc;
	p->py = parity_nc;
	p->sb = stop_nc;
	p->wl = length_nc;
	p->debug = 0;
	
	p->close_port = icoms_close_port;

	p->port_type = icoms_port_type;
	p->get_paths = icoms_get_paths;
	p->set_ser_port = icoms_set_ser_port;

	p->write = NULL;
	p->read = NULL;
	p->del = icoms_del;

	usb_set_usb_methods(p);
	hid_set_hid_methods(p);

	return p;
}

#endif /* UNIX */
