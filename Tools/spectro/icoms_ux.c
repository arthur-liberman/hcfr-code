
 /* Unix icoms and serial I/O class */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   18/11/2000
 *
 * Copyright 1997 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/*
    TTBD:
 */

#include <sys/types.h>      /* Include sys/select.h ? */
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>
#include <string.h>

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

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
instType fast_ser_inst_type(icoms *p, int tryhard, void *, void *); 
#endif /* ENABLE_SERIAL */

/* Create and return a list of available serial ports or USB instruments for this system */
/* return icom error */
int icompaths_refresh_paths(icompaths *p) {
	int rv, usbend = 0;
	int i,j;

	a1logd(p->log, 8, "icoms_get_paths: called\n");

	/* Clear any existing paths */
	p->clear(p);

#ifdef ENABLE_USB
	if ((rv = hid_get_paths(p)) != ICOM_OK)
		return rv;
	if ((rv = usb_get_paths(p)) != ICOM_OK)
		return rv;
#endif /* ENABLE_USB */
	usbend = p->npaths;

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
#ifdef __APPLE__
	/* Search the OSX registry for serial ports */
// ~~99 need to identify FTDI serial ports and mark them "fast"
	{
	    kern_return_t kstat; 
	    mach_port_t mp;						/* Master IO port */
	    CFMutableDictionaryRef sdict;		/* Serial Port  dictionary */
		io_iterator_t mit;					/* Matching itterator */
		io_object_t ioob;					/* Serial object found */

		/* Get dictionary of serial ports */
    	if ((sdict = IOServiceMatching(kIOSerialBSDServiceValue)) == NULL) {
        	a1loge(p->log, ICOM_SYS, "IOServiceMatching returned a NULL dictionary\n");
			return ICOM_OK;
		}

		/* Set value to match to RS232 type serial */
        CFDictionarySetValue(sdict, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDRS232Type));

		/* Init itterator to find matching types. Consumes sdict reference */
		if ((kstat = IOServiceGetMatchingServices(kIOMasterPortDefault, sdict, &mit))
		                                                                     != KERN_SUCCESS) {
        	a1loge(p->log, ICOM_SYS, "IOServiceGetMatchingServices returned %d\n", kstat);
			return ICOM_SYS;
		}

		/* Find all the matching serial ports */
		for (;;) {
			char pname[100];
			int fast = 0;
			
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

			if (strstr(pname, "usbserial") != NULL)
				fast = 1;

			/* Add the port to the list */
			p->add_serial(p, pname, pname, fast);
			a1logd(p->log, 8, "icoms_get_paths: Added path '%s' fast %d\n",pname, fast);

			/* If fast, try and identify it */
			if (fast) {
				icompath *path;
				icoms *icom;
				if ((path = p->get_last_path(p)) != NULL
				 && (icom = new_icoms(path, p->log)) != NULL) {
					instType itype = fast_ser_inst_type(icom, 0, NULL, NULL);
					if (itype != instUnknown)
						icompaths_set_serial_itype(path, itype);
					icom->del(icom);
				}
			}
		continue2:
            CFRelease(dfp);
		continue1:
		    IOObjectRelease(ioob);		/* Release found object */
		}
	    IOObjectRelease(mit);			/* Release the itterator */
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
	/* We will assume that ttyUSB* ports includes FTDI ports */
	{
		DIR *dd;
		struct dirent *de;
		char *dirn = "/dev/";
	
		if ((dd = opendir(dirn)) == NULL) {
			a1loge(p->log, ICOM_SYS, "failed to open directory \"%s\"\n",dirn);
			return ICOM_OK;
		}

		for (;;) {
			int fd;
			char *dpath;
			int fast = 0;

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
				a1loge(p->log, ICOM_SYS, "icompaths_refresh_paths() malloc failed!\n");
				return ICOM_SYS;
			}
			strcpy(dpath, dirn);
			strcat(dpath, de->d_name);

			/* See if the serial port is real */
			if (strncmp(de->d_name, "ttyUSB", 5) != 0) {

				/* Hmm. This is probably a bad idea - it can upset other */
				/* programs that use the serial ports ? */
				if ((fd = open(dpath, O_RDONLY | O_NOCTTY | O_NONBLOCK)) < 0) {
					a1logd(p->log, 8, "icoms_get_paths: failed to open serial \"%s\" - not real\n",dpath);
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
			a1logd(p->log, 8, "icoms_get_paths: open'd serial \"%s\" - assume real\n",dpath);

			if (strncmp(de->d_name, "ttyUSB", 5) == 0)
				fast = 1;

			/* Add the path to the list */
			p->add_serial(p, dpath, dpath, 0);
			a1logd(p->log, 8, "icoms_get_paths: Added path '%s' fast %d\n",dpath,fast);

			/* If fast, try and identify it */
			if (fast) {
				icompath *path;
				icoms *icom;
				if ((path = p->get_last_path(p)) != NULL
				 && (icom = new_icoms(path, p->log)) != NULL) {
					instType itype = fast_ser_inst_type(icom, 0, NULL, NULL);
					if (itype != instUnknown)
						icompaths_set_serial_itype(path, itype);
					icom->del(icom);
				}
			}
		}
		closedir(dd);
	}
#endif /* ! __APPLE__ */
#endif /* ENABLE_SERIAL */

	/* Sort the serial /dev keys so people don't get confused... */
	/* Sort identified instruments ahead of unknown serial ports */
	for (i = usbend; i < (p->npaths-1); i++) {
		for (j = i+1; j < p->npaths; j++) {
			if ((p->paths[i]->itype == instUnknown && p->paths[j]->itype != instUnknown)
			 || (((p->paths[i]->itype == instUnknown && p->paths[j]->itype == instUnknown)
			   || (p->paths[i]->itype != instUnknown && p->paths[j]->itype != instUnknown))
			  && strcmp(p->paths[i]->name, p->paths[j]->name) > 0)) {
				icompath *tt = p->paths[i];
				p->paths[i] = p->paths[j];
				p->paths[j] = tt;
			}
		}
	}
	return ICOM_OK;
}

/* -------------------------------------------------------------------- */

/* Close the port */
static void icoms_close_port(icoms *p) {
	if (p->is_open) {
#ifdef ENABLE_USB
		if (p->usbd) {
			usb_close_port(p);
		} else if (p->hidd) {
			hid_close_port(p);
		}
#endif
#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
		if (p->fd != -1) {
			close(p->fd);
			p->fd = -1;
		}
#endif /* ENABLE_SERIAL */
		p->is_open = 0;
	}
}

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)

static int icoms_ser_write(icoms *p, char *wbuf, double tout);
static int icoms_ser_read(icoms *p, char *rbuf, int bsize, char *tc, int ntc, double tout);


#ifdef __APPLE__
# ifndef IOSSIOSPEED
#  define IOSSIOSPEED    _IOW('T', 2, speed_t)
# endif
# ifndef B921600
#  define B921600 921600
# endif
#endif

/* Set the serial port number and characteristics */
/* This always re-opens the port */
/* return icom error */
static int
icoms_set_ser_port(
icoms       *p, 
flow_control fc,
baud_rate	 baud,
parity		 parity,
stop_bits	 stop,
word_length	 word
) {
	int rv;
	struct termios tio;
	speed_t speed = 0;

	a1logd(p->log, 8, "icoms_set_ser_port: About to set port characteristics:\n"
		              "       Port name = %s\n"
		              "       Flow control = %d\n"
		              "       Baud Rate = %d\n"
		              "       Parity = %d\n"
		              "       Stop bits = %d\n"
		              "       Word length = %d\n"
		              ,p->name ,fc ,baud ,parity ,stop ,word);


	if (p->is_open) 	/* Close it and re-open it */
		p->close_port(p);

	if (p->port_type(p) == icomt_serial) {

		a1logd(p->log, 8, "icoms_set_ser_port: Make sure serial port is open\n");

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

			a1logd(p->log, 8, "icoms_set_ser_port: about to open serial port '%s'\n",p->spath);

			if ((p->fd = open(p->spath, O_RDWR | O_NOCTTY )) < 0) {
				a1logd(p->log, 1, "icoms_set_ser_port: open port '%s' failed with %d (%s)\n",p->spath,p->fd,strerror(errno));
				return ICOM_SYS;
			}
			/* O_NONBLOCK O_SYNC */
			p->is_open = 1;
		}

		if ((rv = tcgetattr(p->fd, &tio)) < 0) {
			a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: tcgetattr on '%s' failed with %d (%s)\n",p->spath,p->fd,strerror(errno));
			return ICOM_SYS;
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
				close(p->fd);
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: illegal flow control %d\n",p->fc);
				return ICOM_SYS;
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
				close(p->fd);
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: illegal parity setting %d\n",p->py);
				return ICOM_SYS;
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
				close(p->fd);
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: illegal stop bits %d\n",p->sb);
				return ICOM_SYS;
			case stop_1:
				break;		/* defaults to 1 */
			case stop_2:
				tio.c_cflag |= CSTOPB;
				break;
		}

		switch (p->wl) {
			case length_nc:
				close(p->fd);
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: illegal word length %d\n",p->wl);
				return ICOM_SYS;
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
			case baud_921600:
				speed = B921600;
				break;
			default:
				close(p->fd);
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: illegal baud rate! (0x%x)\n",p->br);
				return ICOM_SYS;
		}

		tcflush(p->fd, TCIOFLUSH);			/* Discard any current in/out data */

#ifdef NEVER
		// for osx >= 10.4 ? or >= 10.3 ??
		// Doesn't actually seem to be needed... ??
		if (speed > B115200) {
			if (ioctl(p->fd, IOSSIOSPEED, &speed ) == -1)
				printf( "Error %d calling ioctl( ..., IOSSIOSPEED, ... )\n", errno );
	
		}
#endif
		if ((rv = cfsetispeed(&tio,  speed)) < 0) {
			close(p->fd);
			a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: cfsetispeed failed with '%s'\n", strerror(errno));
			return ICOM_SYS;
		}
		if ((rv = cfsetospeed(&tio,  speed)) < 0) {
			close(p->fd);
			a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: cfsetospeed failed with '%s'\n", strerror(errno));
			return ICOM_SYS;
		}

		/* Make change immediately */
		if ((rv = tcsetattr(p->fd, TCSANOW, &tio)) < 0) {
			close(p->fd);
			a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: tcsetattr failed with '%s'\n", strerror(errno));
			return ICOM_SYS;
		}

		tcflush(p->fd, TCIOFLUSH);			/* Discard any current in/out data */

		p->write = icoms_ser_write;
		p->read = icoms_ser_read;

	}
	a1logd(p->log, 8, "icoms_set_ser_port: port characteristics set ok\n");

	return ICOM_OK;
}

/* ---------------------------------------------------------------------------------*/
/* Serial write/read */

/* Write the characters in the buffer out */
/* Data will be written up to the terminating nul */
/* Return relevant error status bits */
/* Set the icoms lserr value */
static int
icoms_ser_write(
icoms *p,
char *wbuf,
double tout
) {
	int rv, retrv = ICOM_OK;
	int len, wbytes;
	long toc, i, top;			/* Timout count, counter, timeout period */
	struct pollfd pa[1];		/* Poll array to monitor serial write and stdin */
	int nfd = 1;				/* Number of fd's to poll */
	struct termios origs, news;

	a1logd(p->log, 8, "icoms_ser_write: About to write '%s' ",icoms_fix(wbuf));
	if (!p->is_open) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_write: device not initialised\n");
		p->lserr = rv = ICOM_SYS;
		return rv;
	}

	/* Setup to wait for serial output not block */
	pa[0].fd = p->fd;
	pa[0].events = POLLOUT;
	pa[0].revents = 0;

	/* Until timed out, aborted, or transmitted */
	len = strlen(wbuf);
	tout *= 1000.0;		/* Timout in msec */

	top = 100;						/* Timeout period in msecs */
	toc = (int)(tout/top + 0.5);	/* Number of timout periods in timeout */
	if (toc < 1)
		toc = 1;

	/* Until data is all written, we time out, or the user aborts */
	for(i = toc; i > 0 && len > 0;) {
		if (poll_x(pa, nfd, top) > 0) {
			if (pa[0].revents != 0) {
				if (pa[0].revents != POLLOUT) {
					a1loge(p->log, ICOM_SYS, "icoms_ser_write: poll returned "
					                     "unexpected value 0x%x",pa[0].revents);
					p->lserr = rv = ICOM_SYS;
					return rv;
				}
				
				/* We can write it without blocking */
				if ((wbytes = write(p->fd, wbuf, len)) < 0) {
					retrv |= ICOM_SERW;
					break;
				} else if (wbytes > 0) {
					i = toc;
					len -= wbytes;
					wbuf += wbytes;
				}
			}
		} else {
			i--;		/* timeout (or error!) */
		}
	}
	if (i <= 0) {		/* Timed out */
		retrv |= ICOM_TO;
	}

	a1logd(p->log, 8, "icoms_ser_write: returning ICOM err 0x%x\n",retrv);

	p->lserr = retrv;
	return retrv;
}

/* Read characters into the buffer */
/* Return string will be terminated with a nul */
/* return icom error */
static int
icoms_ser_read(
icoms *p,
char *rbuf,			/* Buffer to store characters read */
int bsize,			/* Buffer size */
char *tc,			/* Terminating characers, NULL for none */
int ntc,			/* Number of terminating characters */
double tout			/* Time out in seconds */
) {
	int rv, retrv = ICOM_OK;
	int rbytes;
	long j, toc, i, top;		/* Timout count, counter, timeout period */
	struct pollfd pa[1];		/* Poll array to monitor serial read and stdin */
	int nfd = 1;				/* Number of fd's to poll */
	struct termios origs, news;
	char *rrbuf = rbuf;		/* Start of return buffer */

	if (!p->is_open) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_read: device not initialised\n");
		p->lserr = rv = ICOM_SYS;
		return rv;
	}

	if (bsize < 3) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_read: given too small a buffer\n");
		p->lserr = rv = ICOM_SYS;
		return rv;
	}

	/* Wait for serial input to have data */
	pa[0].fd = p->fd;
	pa[0].events = POLLIN | POLLPRI;
	pa[0].revents = 0;

	bsize--;	/* Allow space for null */
	tout *= 1000.0;		/* Timout in msec */

	top = 100;						/* Timeout period in msecs */
	toc = (int)(tout/top + 0.5);	/* Number of timout periods in timeout */
	if (toc < 1)
		toc = 1;

	/* Until data is all read, we time out, or the user aborts */
	for(i = toc, j = 0; i > 0 && bsize > 1 && j < ntc ;) {

		if (poll_x(pa, nfd, top) > 0) {
			if (pa[0].revents != 0) {
				if (pa[0].revents != POLLIN &&  pa[0].revents != POLLPRI) {
					a1loge(p->log, ICOM_SYS, "icoms_ser_read: poll on serin returned "
					                     "unexpected value 0x%x",pa[0].revents);
					p->lserr = rv = ICOM_SYS;
					return rv;
				}

				/* We have data to read from input */
				if ((rbytes = read(p->fd, rbuf, bsize)) < 0) {
					retrv |= ICOM_SERR;
					break;
				} else if (rbytes > 0) {
					i = toc;		/* Reset time */
					bsize -= rbytes;
					if (tc != NULL) {
						while(rbytes--) {	/* Count termination characters */
							char ch = *rbuf++, *tcp = tc;
							
							while(*tcp != '\000') {
								if (ch == *tcp)
									j++;
								tcp++;
							}
						}
					} else
						rbuf += rbytes;
				}
			}
		} else {
			i--;		/* We timed out (or error!) */
		}
	}

	*rbuf = '\000';
	if (i <= 0) {			/* timed out */
		retrv |= ICOM_TO;
	}

	a1logd(p->log, 8, "icoms_ser_read: returning '%s' ICOM err 0x%x\n",icoms_fix(rrbuf),retrv);

	p->lserr = retrv;
	return retrv;
}

#endif /* ENABLE_SERIAL */

/* ---------------------------------------------------------------------------------*/

/* Destroy ourselves */
static void
icoms_del(icoms *p) {
	a1logd(p->log, 8, "icoms_del: called\n");
	if (p->is_open) {
		a1logd(p->log, 8, "icoms_del: closing port\n");
		p->close_port(p);
	}
#ifdef ENABLE_USB
	usb_del_usb(p);
	hid_del_hid(p);
#endif
	p->log = del_a1log(p->log);
	free (p);
}

