
/* Unix icoms and serial I/O class */
/* This is #includeed in icom.c */

/* 
 * Argyll Color Management System
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

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)

#include <sys/types.h>      /* Include sys/select.h ? */
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>
#include <string.h>

/* select() defined, but not poll(), so emulate poll() */
#if defined(FD_CLR) && !defined(POLLIN)
# include "pollem.h"
# define poll_x pollem
#else
# include <sys/poll.h>	/* Else assume poll() is native */
# define poll_x poll
#endif

#ifdef UNIX_APPLE
//#include <stdbool.h>
# include <sys/sysctl.h>
# include <sys/param.h>
# include <CoreFoundation/CoreFoundation.h>
# include <IOKit/IOKitLib.h>
# include <IOKit/serial/IOSerialKeys.h>
# include <IOKit/IOBSD.h>
# include <mach/mach_init.h>
# include <mach/task_policy.h>
#endif /* UNIX_APPLE */


devType fast_ser_dev_type(icoms *p, int tryhard, void *, void *); 

/* Add paths to serial connected device. */
/* Return an icom error */
int serial_get_paths(icompaths *p, icom_type mask) {
	int rv;

	a1logd(p->log, 7, "serial_get_paths: called with mask %d\n",mask);

#ifdef UNIX_APPLE
	/* Search the OSX registry for serial ports */
	if (mask & (icomt_serial | icomt_fastserial | icomt_btserial)) {
	    kern_return_t kstat; 
	    mach_port_t mp;						/* Master IO port */
	    CFMutableDictionaryRef sdict;		/* Serial Port  dictionary */
		io_iterator_t mit;					/* Matching itterator */
		io_object_t ioob;					/* Serial object found */

		a1logd(p->log, 6, "serial_get_paths: looking up serial ports services\n");

		/* Get dictionary of serial ports */
    	if ((sdict = IOServiceMatching(kIOSerialBSDServiceValue)) == NULL) {
        	a1loge(p->log, ICOM_SYS, "IOServiceMatching returned a NULL dictionary\n");
			return ICOM_OK;		/* Hmm. There are no serial ports ? */
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
			char pname[200];
			icom_type dctype = icomt_unknown;
			
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

			a1logd(p->log, 8, "serial_get_paths: checking '%s'\n",pname);

			/* Ignore infra red port or any other noise */
			if (strstr(pname, "IrDA") != NULL
			 || strstr(pname, "Dialup") != NULL
			 || strstr(pname, "PDA-Sync") != NULL)
				goto continue2;

			/* Would be nice to identify FTDI serial ports more specifically ? */
			if (strstr(pname, "usbserial") != NULL)
				dctype |= icomt_fastserial;

			if (strstr(pname, "Bluetooth") != NULL
			 || strstr(pname, "JETI") != NULL) {
				dctype |= icomt_fastserial;
				dctype |= icomt_btserial;
			}

#ifndef ENABLE_SERIAL
			if (dctype & icomt_fastserial) {		/* Only add fast ports if !ENABLE_SERIAL */
#endif
			if (((mask & icomt_serial) && !(dctype & icomt_fastserial))
			 || ((mask & icomt_fastserial) && (dctype & icomt_fastserial)
			                               && !(dctype & icomt_btserial))
			 || ((mask & icomt_btserial) && (dctype & icomt_btserial))) {
				/* Add the port to the list */
				p->add_serial(p, pname, pname, dctype);
				a1logd(p->log, 8, "serial_get_paths: Added path '%s' dctype 0x%x\n",pname, dctype);
			}
#ifndef ENABLE_SERIAL
			}
#endif
			/* If fast, try and identify it */
			if (dctype & icomt_fastserial) {
				icompath *path;
				icoms *icom;
				if ((path = p->get_last_path(p)) != NULL
				 && (icom = new_icoms(path, p->log)) != NULL) {
					if (!p->fs_excluded(p, path)) {
						instType itype = fast_ser_dev_type(icom, 0, NULL, NULL);
						if (itype != instUnknown)
							icompaths_set_serial_itype(path, itype);	/* And set category */
					}
					icom->del(icom);
				}
				a1logd(p->log, 8, "serial_get_paths: Identified '%s' dctype 0x%x\n",inst_sname(path->dtype),path->dctype);
			}
		continue2:
            CFRelease(dfp);
		continue1:
		    IOObjectRelease(ioob);		/* Release found object */
		}
	    IOObjectRelease(mit);			/* Release the itterator */
	}

#else /* Other UNIX like systems */

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
	/* We will assume that ttyUSB* ports includes FTDI ports. */
	/* Bluetooth ports are named ttyHS* or rfcomm* ?? */

	if (mask & (icomt_serial | icomt_fastserial | icomt_bt)) {
		DIR *dd;
		struct dirent *de;
		char *dirn = "/dev/";
	
		a1logd(p->log, 6, "serial_get_paths: looking up serial port devices\n");

		if ((dd = opendir(dirn)) == NULL) {
			a1loge(p->log, ICOM_SYS, "failed to open directory \"%s\"\n",dirn);
			return ICOM_OK;
		}

		for (;;) {
			int fd;
			char *dpath;
			icom_type dctype = icomt_unknown;

			if ((de = readdir(dd)) == NULL) {
				break;
			}

			a1logd(p->log, 8, "serial_get_paths: checking '%s'\n",de->d_name);

			if (!(
#if defined(__FreeBSD__) || defined(__OpenBSD__)
			   /* This should match uart & USB devs. */
				( strncmp (de->d_name, "cua", 3) == 0
				&& strlen (de->d_name) < 7)
#else
				/* Presumably Linux.. */
			    (   strncmp(de->d_name, "ttyS", 4) == 0
			     && de->d_name[4] >= '0' && de->d_name[4] <= '9')
			 || (   strncmp(de->d_name, "ttyUSB", 6) == 0)
			 || (   strncmp(de->d_name, "ttyHS", 5) == 0)
			 || (   strncmp(de->d_name, "rfcomm", 6) == 0)
#endif
			))
				continue;

			if ((dpath = (char *)malloc(strlen(dirn) + strlen(de->d_name) + 1)) == NULL) {
				closedir(dd);
				a1loge(p->log, ICOM_SYS, "icompaths_refresh_paths_sel() malloc failed!\n");
				return ICOM_SYS;
			}
			strcpy(dpath, dirn);
			strcat(dpath, de->d_name);

			/* See if the (not fast) serial port is real */
			if (strncmp(de->d_name, "ttyUSB", 6) != 0
			 && strncmp(de->d_name, "ttyHS", 5) != 0
			 && strncmp(de->d_name, "rfcomm", 6) != 0) {

				/* Hmm. This is probably a bad idea - it can upset other */
				/* programs that use the serial ports ? */
				if ((fd = open(dpath, O_RDONLY | O_NOCTTY | O_NONBLOCK)) < 0) {
					a1logd(p->log, 8, "serial_get_paths: failed to open serial \"%s\" r/o - not real\n",dpath);
					free(dpath);
					msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
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
				msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
				a1logd(p->log, 8, "serial_get_paths: open'd serial \"%s\" r/o - assume real\n",dpath);
			}

			if (strncmp(de->d_name, "ttyUSB", 6) == 0
			 || strncmp(de->d_name, "ttyHS", 5) == 0
			 || strncmp(de->d_name, "rfcomm", 6) == 0)
				dctype |= icomt_fastserial;

			if (strncmp(de->d_name, "rfcomm", 6) == 0) {
				dctype |= icomt_fastserial;
				dctype |= icomt_btserial;
			}

#ifndef ENABLE_SERIAL
			if (dctype & icomt_fastserial) {		/* Only add fast ports if !ENABLE_SERIAL */
#endif
			if (((mask & icomt_serial) && !(dctype & icomt_fastserial))
			 || ((mask & icomt_fastserial) && (dctype & icomt_fastserial)
			                               && !(dctype & icomt_btserial))
			 || ((mask & icomt_btserial) && (dctype & icomt_btserial))) {
				/* Add the port to the list */
				p->add_serial(p, dpath, dpath, dctype);
				a1logd(p->log, 8, "serial_get_paths: Added path '%s' dctype 0x%x\n",dpath, dctype);
			}
#ifndef ENABLE_SERIAL
			}
#endif
			free(dpath);

			/* If fast, try and identify it */
			if (dctype & icomt_fastserial) {
				icompath *path;
				icoms *icom;
				if ((path = p->get_last_path(p)) != NULL
				 && (icom = new_icoms(path, p->log)) != NULL) {
					if (!p->fs_excluded(p, path)) {
						instType itype = fast_ser_dev_type(icom, 0, NULL, NULL);
						if (itype != instUnknown)
							icompaths_set_serial_itype(path, itype);	/* And set category */
					}
					icom->del(icom);
				}
				a1logd(p->log, 8, "serial_get_paths: Identified '%s' dctype 0x%x\n",inst_sname(path->dtype),path->dctype);
			}
		}
		closedir(dd);
	}
#endif /* ! UNIX_APPLE */

	return ICOM_OK;
}

/* -------------------------------------------------------------------- */

/* Is the serial port actually open ? */
int serial_is_open(icoms *p) {
	return p->fd != -1;
}

/* Close the serial port */
void serial_close_port(icoms *p) {

	if (p->is_open && p->fd != -1) {
		close(p->fd);
		p->fd = -1;
		msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
	}
}

/* -------------------------------------------------------------------- */

#ifdef UNIX_APPLE
# ifndef IOSSIOSPEED
#  define IOSSIOSPEED    _IOW('T', 2, speed_t)
# endif
#endif 

#if defined(UNIX_APPLE) || defined(__OpenBSD__)
# ifndef B230400
#  define B230400 230400
# endif
# ifndef B460800
#  define B460800 460800
# endif
# ifndef B512000
#  define B512000 512000
# endif
# ifndef B921600
#  define B921600 921600
# endif
#endif

/* Set the serial port number and characteristics */
/* This always re-opens the port */
/* return icom error */
static int
icoms_set_ser_port_ex(
icoms       *p, 
flow_control fc,
baud_rate	 baud,
parity		 parity,
stop_bits	 stop,
word_length	 word,
int          delayms) {		/* Delay after open in msec */		
	int rv;
	struct termios tio;
	speed_t speed = 0;

	a1logd(p->log, 8, "icoms_set_ser_port: About to set port characteristics:\n"
		              "       Port name = %s\n"
		              "       Flow control = %d\n"
		              "       Baud Rate = %s\n"
		              "       Parity = %d\n"
		              "       Stop bits = %d\n"
		              "       Word length = %d\n"
	                  "       Open delay = %d ms\n"
		              ,p->name ,fc ,baud_rate_to_str(baud) ,parity ,stop ,word, delayms);


#ifdef NEVER		/* This is too slow on OS X */
	if (p->is_open) { 	/* Close it and re-open it */
		a1logd(p->log, 8, "icoms_set_ser_port: closing port\n");
		p->close_port(p);
	}
#endif

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

			/* Hmm. Recent OS X to FTDI can take 1500 msec to open. Why ? */
			/* O_NONBLOCK O_SYNC */
			if ((p->fd = open(p->spath, O_RDWR | O_NOCTTY )) < 0) {
				a1logd(p->log, 1, "icoms_set_ser_port: open port '%s' r/w failed with %d (%s)\n",
				       p->spath,p->fd,strerror(errno));
				return ICOM_SYS;
			}

			if (delayms < 160)		/* Seems to need at least 80 msec for many drivers */
				delayms = 160;

			msec_sleep(delayms);	/* For Bluetooth and other drivers */

#ifdef NEVER
            /* If we used O_NONBLOCK */
            if (fcntl(p->fd, F_SETFL, 0) == -1) {
                a1logd(p->log, 1, "icoms_set_ser_port: clearing O_NONBLOCK failed with %d (%s)\n",
				       p->spath,p->fd,strerror(errno));
                return ICOM_SYS;
            }
#endif

#ifdef NEVER	/* See what supplementary groups we are a member of */
		{
			int j, ngroups = 16;
			gid_t *groups = (gid_t *)malloc (ngroups * sizeof(gid_t));
			struct passwd *pw = getpwuid(getuid());
		
			if (groups == NULL) {
				a1logd(p->log, 0, "icoms_set_ser_port: malloc of sgroups failed\n");
				goto fail;
			}
			if (pw == NULL) {
				a1logd(p->log, 0, "icoms_set_ser_port: getpwuid failed\n");
				goto fail;
			}
		
			if (getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups) < 0) {
				groups = realloc(groups, ngroups * sizeof(gid_t));
				if (groups == NULL) {
					a1logd(p->log, 0, "icoms_set_ser_port: realloc of sgroups failed\n");
					goto fail;
				}
				getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups);
			}
			a1logd(p->log, 0, "icoms_set_ser_port: ngroups = %d\n", ngroups);
			for (j = 0; j < ngroups; j++) {
				struct group *gr = getgrgid(groups[j]);
				if (gr != NULL)
					a1logd(p->log, 0, "icoms_set_ser_port: %d: gid %d\n", j,groups[j]);
				else
					a1logd(p->log, 0, "icoms_set_ser_port: %d: gid %d (%s)\n", j,groups[j],gr->gr_name);
			}
			  fail:;
		}
#endif
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
				msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
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
#ifdef UNIX_APPLE
				tio.c_cflag |= CCTS_OFLOW;
				tio.c_cflag |= CRTS_IFLOW;
#else
				tio.c_cflag |= CRTSCTS;
#endif
				break;
			case fc_HardwareDTR:
				/* Use DTR/DSR bi-directional flow control */
#ifdef UNIX_APPLE
				tio.c_cflag |= CDSR_OFLOW;
				tio.c_cflag |= CDTR_IFLOW;
#else
#ifdef CDTRDSR	/* Hmm. Not all Linux's support this... */
				tio.c_cflag |= CDTRDSR;
#endif
#endif
				break;
			default:
				break;
		}

		switch (p->py) {
			case parity_nc:
				close(p->fd);
				msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
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
				msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
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
				msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
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
			case baud_230400:
				speed = B230400;
				break;
			case baud_921600:
				speed = B921600;
				break;
			default:
				close(p->fd);
				msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
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
			msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
			a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: cfsetispeed failed with '%s'\n", strerror(errno));
			return ICOM_SYS;
		}
		if ((rv = cfsetospeed(&tio,  speed)) < 0) {
			close(p->fd);
			msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
			a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: cfsetospeed failed with '%s'\n", strerror(errno));
			return ICOM_SYS;
		}

		/* Make change immediately */
		if ((rv = tcsetattr(p->fd, TCSANOW, &tio)) < 0) {
			close(p->fd);
			msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
			a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: tcsetattr failed with '%s'\n", strerror(errno));
			return ICOM_SYS;
		}

		tcflush(p->fd, TCIOFLUSH);			/* Discard any current in/out data */
		msec_sleep(50);		/* Improves reliability of USB<->Serial converters */

		p->write = icoms_ser_write;
		p->read = icoms_ser_read;

	}
	a1logd(p->log, 8, "icoms_set_ser_port: port characteristics set ok\n");

	return ICOM_OK;
}

/* Set the serial port characteristics */
/* This always re-opens the port */
/* return an icom error */
static int
icoms_set_ser_port(
icoms *p, 
flow_control fc,
baud_rate	 baud,
parity		 parity,
stop_bits	 stop,
word_length	 word)
{
	return icoms_set_ser_port_ex(p, fc, baud, parity, stop, word, 0);
}

/* ------------------------------ */
/* Could add read flush function, to discard all read data using 

	tcflush(fd, TCIFLUSH);

*/


/* ---------------------------------------------------------------------------------*/
/* Serial write/read */

/* Write the characters in the buffer out */
/* Data will be written up to the terminating nul */
/* Return relevant error status bits */
/* Set the icoms lserr value */
int
icoms_ser_write(
icoms *p,
char *wbuf,			/* null terminated unless nwch > 0 */
int nwch,			/* if > 0, number of characters to write */
double tout
) {
	int len, wbytes;
	long ttop, top;				/* Total timeout period, timeout period */
	unsigned int stime, etime;	/* Start and end times of USB operation */
	struct pollfd pa[1];		/* Poll array to monitor serial write and stdin */
	int nfd = 1;				/* Number of fd's to poll */
	struct termios origs, news;
	int retrv = ICOM_OK;

	a1logd(p->log, 8, "\nicoms_ser_write: writing '%s'\n",
	       nwch > 0 ? icoms_tohex((unsigned char *)wbuf, nwch) : icoms_fix(wbuf));

	if (!p->is_open) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_write: device not initialised\n");
		p->lserr = ICOM_SYS;
		return p->lserr;
	}

	/* Setup to wait for serial output not block */
	pa[0].fd = p->fd;
	pa[0].events = POLLOUT;
	pa[0].revents = 0;

	if (nwch != 0)
		len = nwch;
	else
		len = strlen(wbuf);

	ttop = (int)(tout * 1000.0 + 0.5);        /* Total timeout period in msecs */

	a1logd(p->log, 8, "\nicoms_ser_write: ep 0x%x, bytes %d, ttop %d, quant %d\n", p->rd_ep, len, ttop, p->rd_qa);

	etime = stime = msec_time();

	/* Until data is all written or we time out */
	for (top = ttop; top > 0 && len > 0;) {

		if (poll_x(pa, nfd, top) > 0) {		/* Wait for something */

			if (pa[0].revents != 0) {
				if (pa[0].revents != POLLOUT) {
					a1loge(p->log, ICOM_SYS, "icoms_ser_write: poll returned "
					                     "unexpected value 0x%x",pa[0].revents);
					p->lserr = ICOM_SYS;
					return p->lserr;
				}
				
				/* We can write it without blocking */
				wbytes = write(p->fd, wbuf, len);
				if (wbytes < 0) {
					a1logd(p->log, 8, "icoms_ser_write: write failed with %d\n",wbytes);
					retrv |= ICOM_SERW;
					break;

				} else if (wbytes > 0) {
					a1logd(p->log, 8, "icoms_ser_write: wrote %d bytes\n",wbytes);
					len -= wbytes;
					wbuf += wbytes;
				}
			}
		}
		etime = msec_time();
		top = ttop - (etime - stime);	/* Remaining time */
	}

	if (top <= 0) {			/* Must have timed out */
		a1logd(p->log, 8, "icoms_ser_write: timeout, took %d msec out of %d\n",etime - stime,ttop);
		retrv |= ICOM_TO; 
	}

//	tcdrain(p->fd);

	a1logd(p->log, 8, "icoms_ser_write: took %d msec, returning ICOM err 0x%x\n",etime - stime,retrv);
	p->lserr = retrv;
	return p->lserr;
}

/* Read characters into the buffer */
/* Return string will be terminated with a nul */
/* return icom error */
int
icoms_ser_read(
icoms *p,
char *rbuf,			/* Buffer to store characters read */
int bsize,			/* Buffer size */
int *pbread,		/* Bytes read (not including forced '\000') */
char *tc,			/* Terminating characers, NULL for none or char count mode */
int ntc,			/* Number of terminating characters or char count needed, if 0 use bsize */
double tout			/* Time out in seconds */
) {
	int j, rbytes;
	long ttop, top;			/* Total timeout period, timeout period */
	unsigned int stime, etime;		/* Start and end times of USB operation */
	struct pollfd pa[1];		/* Poll array to monitor serial read and stdin */
	int nfd = 1;				/* Number of fd's to poll */
	struct termios origs, news;
	char *rrbuf = rbuf;		/* Start of return buffer */
	int bread = 0;
	int retrv = ICOM_OK;
	int nreads;				/* Number of reads performed */

	if (!p->is_open) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_read: device not initialised\n");
		p->lserr = ICOM_SYS;
		return p->lserr;
	}

	if (bsize < 3) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_read: given too small a buffer\n");
		p->lserr = ICOM_SYS;
		return p->lserr;
	}

	for (j = 0; j < bsize; j++)
		rbuf[j] = 0;
 
	ttop = (int)(tout * 1000.0 + 0.5);        /* Total timeout period in msecs */

	a1logd(p->log, 8, "\nicoms_ser_read: bytes %d, ttop %d, ntc %d\n", bsize, ttop, ntc);

	/* Wait for serial input to have data */
	pa[0].fd = p->fd;
	pa[0].events = POLLIN | POLLPRI;
	pa[0].revents = 0;

	bsize -=1;			/* Allow space for forced null */

	/* Until data is all read, we time out, or the user aborts */
	etime = stime = msec_time();
	j = (tc == NULL && ntc <= 0) ? -1 : 0;

	/* Until data is all read or we time out */
	for (top = ttop, nreads = 0; top > 0 && bsize > 0 && j < ntc ;) {

		if (poll_x(pa, nfd, top) > 0) {
			if (pa[0].revents != 0) {
				int btr;
				if (pa[0].revents != POLLIN &&  pa[0].revents != POLLPRI) {
					a1loge(p->log, ICOM_SYS, "icoms_ser_read: poll on serin returned "
					                     "unexpected value 0x%x",pa[0].revents);
					p->lserr = ICOM_SYS;
					return p->lserr;
				}

				/* We have data to read from input */
				rbytes = read(p->fd, rbuf, bsize);
				if (rbytes < 0) {
					a1logd(p->log, 8, "icoms_ser_read: read failed with %d, rbuf = '%s'\n",rbytes,icoms_fix(rrbuf));
					retrv |= ICOM_SERR;
					break;

				} else if (rbytes > 0) {
					a1logd(p->log, 8, "icoms_ser_read: read %d bytes, rbuf = '%s'\n",rbytes,icoms_fix(rrbuf));
		
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
						a1logd(p->log, 8, "icoms_ser_read: tc count %d\n",j);
					} else {
						if (ntc > 0)
							j += rbytes;
						rbuf += rbytes;
					}
				}
			}
		}
		etime = msec_time();
		top = ttop - (etime - stime);	/* Remaining time */
	}

	*rbuf = '\000';
	a1logd(p->log, 8, "icoms_ser_read: read %d total bytes with %d reads\n",rbuf - rrbuf, nreads);
	if (pbread != NULL)
		*pbread = (rbuf - rrbuf);

	/* If ran out of time and not completed */
	a1logd(p->log, 8, "icoms_ser_read: took %d msec\n",etime - stime);
	if (top <= 0 && bsize > 0 && j < ntc) {
		a1logd(p->log, 8, "icoms_ser_read: timeout, took %d msec out of %d\n",etime - stime,ttop);
		retrv |= ICOM_TO; 
	}

	a1logd(p->log, 8, "icoms_ser_read: took %d msec, returning '%s' ICOM err 0x%x\n",
	       etime - stime, tc == NULL && ntc > 0
			                          ? icoms_tohex((unsigned char *)rrbuf, rbuf - rrbuf)
	                                  : icoms_fix(rrbuf), retrv);

	p->lserr = retrv;
	return p->lserr;
}

#endif /* ENABLE_SERIAL || ENABLE_FAST_SERIAL*/

