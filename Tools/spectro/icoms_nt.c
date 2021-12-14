 
/* Windows NT serial I/O class */

/* 
 * Argyll Color Management System
 *
 * Author: Graeme W. Gill
 * Date:   28/9/97
 *
 * Copyright 1997 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)

#include <conio.h>

devType fast_ser_dev_type(icoms *p, int tryhard, void *, void *); 

/* Add paths to serial connected device. */
/* Return an icom error */
int serial_get_paths(icompaths *p, icom_type mask) {
	int i;
	LONG stat;
	HKEY sch;		/* Serial coms handle */

	a1logd(p->log, 7, "serial_get_paths: called with mask = %d\n",mask);

	// (Beware KEY_WOW64_64KEY ?)

#define MXKSIZE 500
#define MXVSIZE 300

	if (mask & (icomt_serial | icomt_fastserial | icomt_btserial)) {

		a1logd(p->log, 6, "serial_get_paths: looking up the registry for serial ports\n");
		/* Look in the registry for serial ports */
		if ((stat = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM",
		                         0, KEY_READ, &sch)) != ERROR_SUCCESS) {
			a1logd(p->log, 1, "serial_get_paths: There don't appear to be any serial ports\n");
			return ICOM_OK;
		}

		/* Look at all the values in this key */
		a1logd(p->log, 8, "serial_get_paths: looking through all the values in the SERIALCOMM key\n");

		for (i = 0; ; i++) {
			char valname[MXKSIZE], *vp;
			DWORD vnsize = MXKSIZE;
			DWORD vtype;
			char value[MXVSIZE];
			DWORD vsize = MXVSIZE;
			icom_type dctype = icomt_unknown;

			stat = RegEnumValue(
				sch,		/* handle to key to enumerate */
				i,			/* index of subkey to enumerate */
				valname,    /* address of buffer for value name */
				&vnsize,	/* address for size of value name buffer */
				NULL,		/* reserved */
				&vtype,		/* Address of value type */
				value,		/* Address of value buffer */
				&vsize		/* Address of value buffer size */
			);
			if (stat == ERROR_NO_MORE_ITEMS) {
				a1logd(p->log, 8, "serial_get_paths: got ERROR_NO_MORE_ITEMS\n");
				break;
			}
			if (stat == ERROR_MORE_DATA		/* Hmm. Should expand buffer size */
			 || stat != ERROR_SUCCESS) {
				a1logw(p->log, "serial_get_paths: RegEnumValue failed with %d\n",stat);
				break;
			}
			valname[MXKSIZE-1] = '\000';
			value[MXVSIZE-1] = '\000';

			if (vtype != REG_SZ) {
				a1logw(p->log, "serial_get_paths: RegEnumValue didn't return stringz type\n");
				continue;
			}

			if ((vp = strrchr(valname, '\\')) == NULL)
				vp = valname;
			else
				vp++;

			a1logd(p->log, 8, "serial_get_paths: checking '%s'\n",vp);

			/* See if it looks like a fast port */
			if (strncmp(vp, "VCP", 3) == 0) {			/* Virtual */
				dctype |= icomt_fastserial;
			}

			if (strncmp(vp, "USBSER", 6) == 0) {		/* USB Serial port */
				dctype |= icomt_fastserial;
			}

			if (strncmp(vp, "BtPort", 6) == 0			/* Blue tooth */
			 || strncmp(vp, "BthModem", 8) == 0) {
				dctype |= icomt_fastserial;
				dctype |= icomt_btserial;
			}

#ifndef ENABLE_SERIAL
			if (dctype & icomt_fastserial) {	/* Only add fast ports if !ENABLE_SERIAL */
#endif
			if (((mask & icomt_serial) && !(dctype & icomt_fastserial))
			 || ((mask & icomt_fastserial) && (dctype & icomt_fastserial)
			                               && !(dctype & icomt_btserial))
			 || ((mask & icomt_btserial) && (dctype & icomt_btserial))) {

				// ~~ would be nice to add better description, similar
				//    to that of device manager, i.e. "Prolific USB-to-SerialBridge (COM6)"
				/* Add the port to the list */
				p->add_serial(p, value, value, dctype);
				a1logd(p->log, 8, "serial_get_paths: Added '%s' path '%s' dctype 0x%x\n",vp, value,dctype);
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
						devType itype = fast_ser_dev_type(icom, 0, NULL, NULL);
						if (itype != instUnknown)
							icompaths_set_serial_itype(path, itype);	/* And set category */
					}
					icom->del(icom);
				}
				a1logd(p->log, 8, "serial_get_paths: Identified '%s' dctype 0x%x\n",inst_sname(path->dtype),path->dctype);
			}
		}
		if ((stat = RegCloseKey(sch)) != ERROR_SUCCESS) {
			a1logw(p->log, "serial_get_paths: RegCloseKey failed with %d\n",stat);
		}
	}

	return ICOM_OK;
}

/* -------------------------------------------------------------------- */

/* Is the serial port actually open ? */
int serial_is_open(icoms *p) {
	return p->phandle != NULL;
}

/* Close the serial port */
void serial_close_port(icoms *p) {

	if (p->is_open && p->phandle != NULL) {
		CloseHandle(p->phandle);
		p->phandle = NULL;
		msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
	}
}

/* Clear any serial errors */
static void nt_ser_clearerr(icoms *p) {
	DWORD errs;

	if (!ClearCommError(p->phandle, &errs,NULL))
   		warning("nt_ser_clearerr: failed, and Clear error failed in %s at %d",__FILE__,__LINE__);

	return;
}

/* -------------------------------------------------------------------- */

#ifndef CBR_230400
# define CBR_230400 230400
#endif
#ifndef CBR_460800
# define CBR_460800 460800
#endif
#ifndef CBR_512000
# define CBR_512000 512000
#endif
#ifndef CBR_921600
# define CBR_921600 921600
#endif

/* Set the serial port characteristics - extended */
/* This always re-opens the port */
/* return an icom error */
static int
icoms_set_ser_port_ex(
icoms *p, 
flow_control fc,
baud_rate	 baud,
parity		 parity,
stop_bits	 stop,
word_length	 word,
int          delayms) {		/* Delay after open in msec */		

	a1logd(p->log, 8, "icoms_set_ser_port: About to set port characteristics:\n"
		              "       Port name = %s\n"
		              "       Flow control = %d\n"
		              "       Baud Rate = %s\n"
		              "       Parity = %d\n"
		              "       Stop bits = %d\n"
		              "       Word length = %d\n"
		              "       Open delay = %d ms\n"
		              ,p->name ,fc ,baud_rate_to_str(baud) ,parity ,stop ,word, delayms);

#ifdef NEVER	/* Is this needed ? */
	if (p->is_open) {
		a1logd(p->log, 8, "icoms_set_ser_port: closing port '%s'\n",p->name);
		p->close_port(p);
	}
#endif

	if (p->port_type(p) == icomt_serial) {
		DCB dcb;

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
			char buf[100];	/* Temporary for COM device path */

			a1logd(p->log, 8, "icoms_set_ser_port: about to open serial port '%s'\n",p->spath);

			/* Workaround bug of ports > COM9 */
			sprintf(buf, "\\\\.\\%s", p->spath);

			if ((p->phandle = CreateFile(
				buf,
				GENERIC_READ|GENERIC_WRITE,
				0,				/* Exclusive access */
				NULL,			/* No security */
				OPEN_EXISTING,	/* Does not make sense to create */
				0,				/* No overlapped I/O */
				NULL)			/* NULL template */
			) == INVALID_HANDLE_VALUE) {
				a1logd(p->log, 1, "icoms_set_ser_port: open port '%s' failed with LastError %d\n",buf,GetLastError());
				return ICOM_SYS;
			}

			if (delayms < 160)		/* Seems to need at least 80 msec with many drivers */
				delayms = 160;

			msec_sleep(delayms);	/* For Bluetooth */

			p->is_open = 1;
		}

		if (GetCommState(p->phandle, &dcb) == FALSE) {
			CloseHandle(p->phandle);
			msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
			a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: reading state '%s' failed with LastError %d\n",p->spath,GetLastError());
			return ICOM_SYS;
		}

		/* Set misc stuff to default */
		dcb.fBinary = TRUE; 						/* Binary mode is only one that works */
		dcb.fOutxCtsFlow = FALSE;					/* Not using Cts flow control */
		dcb.fOutxDsrFlow = FALSE;					/* Not using Dsr Flow control */
		dcb.fDtrControl = DTR_CONTROL_ENABLE;		/* Enable DTR during connection */
		dcb.fDsrSensitivity = FALSE;				/* Not using Dsr to ignore characters */
		dcb.fTXContinueOnXoff = TRUE;				/* */
		dcb.fOutX = FALSE;							/* No default Xon/Xoff flow control */
		dcb.fInX = FALSE;							/* No default Xon/Xoff flow control */
		dcb.fErrorChar = FALSE;
		dcb.fNull = FALSE;
		dcb.fRtsControl = RTS_CONTROL_ENABLE;		/* Turn RTS on during connection */
		dcb.fAbortOnError = FALSE;					/* Hmm. TRUE Stuffs up FTDI. Is it needed ? */

		switch (p->fc) {
			case fc_nc:
				CloseHandle(p->phandle);
				msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: illegal flow control %d\n",p->fc);
				return ICOM_SYS;
			case fc_None:
				/* Use no flow control */
				break;
			case fc_XonXOff:
				/* Use Xon/Xoff bi-directional flow control */
				dcb.fOutX = TRUE;
				dcb.fInX = TRUE;
				dcb.XonChar = 0x11;					/* ^Q */
				dcb.XoffChar = 0x13;				/* ^S */
				// dcb.fTXContinueOnXoff = FALSE;		/* Stop transmitting if input is full */
				break;
			case fc_Hardware:
				/* Use RTS/CTS bi-directional flow control */
				dcb.fOutxCtsFlow = TRUE;
				dcb.fRtsControl = RTS_CONTROL_HANDSHAKE; 
				break;
			case fc_HardwareDTR:
				/* Use DTR/DSR bi-directional flow control */
				dcb.fOutxDsrFlow = TRUE;
				dcb.fDtrControl = DTR_CONTROL_HANDSHAKE; 
				break;
			default:
				break;
		}

		switch (p->py) {
			case parity_nc:
				CloseHandle(p->phandle);
				msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: illegal parity setting %d\n",p->py);
				return ICOM_SYS;
			case parity_none:
				dcb.fParity = FALSE;
				dcb.Parity = NOPARITY;
				break;
			case parity_odd:
				dcb.fParity = TRUE;
				dcb.Parity = ODDPARITY;
				break;
			case parity_even:
				dcb.fParity = TRUE;
				dcb.Parity = EVENPARITY;
				break;
			default:
				break;
		}

		switch (p->sb) {
			case stop_nc:
				CloseHandle(p->phandle);
				msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: illegal stop bits %d\n",p->sb);
				return ICOM_SYS;
			case stop_1:
				dcb.StopBits = ONESTOPBIT;
				break;
			case stop_2:
				dcb.StopBits = TWOSTOPBITS;
				break;
		}

		switch (p->wl) {
			case length_nc:
				CloseHandle(p->phandle);
				msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: illegal word length %d\n",p->wl);
				return ICOM_SYS;
			case length_5:
				dcb.ByteSize = 5;
				break;
			case length_6:
				dcb.ByteSize = 6;
				break;
			case length_7:
				dcb.ByteSize = 7;
				break;
			case length_8:
				dcb.ByteSize = 8;
				break;
		}

		switch (p->br) {
			case baud_110:
				dcb.BaudRate = CBR_110;
				break;
			case baud_300:
				dcb.BaudRate = CBR_300;
				break;
			case baud_600:
				dcb.BaudRate = CBR_600;
				break;
			case baud_1200:
				dcb.BaudRate = CBR_1200;
				break;
			case baud_2400:
				dcb.BaudRate = CBR_2400;
				break;
			case baud_4800:
				dcb.BaudRate = CBR_4800;
				break;
			case baud_9600:
				dcb.BaudRate = CBR_9600;
				break;
			case baud_14400:
				dcb.BaudRate = CBR_14400;
				break;
			case baud_19200:
				dcb.BaudRate = CBR_19200;
				break;
			case baud_38400:
				dcb.BaudRate = CBR_38400;
				break;
			case baud_57600:
				dcb.BaudRate = CBR_57600;
				break;
			case baud_115200:
				dcb.BaudRate = CBR_115200;
				break;
			case baud_230400:
				dcb.BaudRate = CBR_230400;
				break;
			case baud_921600:
				dcb.BaudRate = CBR_921600;
				break;
			default:
				CloseHandle(p->phandle);
				msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: illegal baud rate! (0x%x)\n",p->br);
				return ICOM_SYS;
		}

		PurgeComm(p->phandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

		if (!SetCommState(p->phandle, &dcb)) {
			CloseHandle(p->phandle);
			msec_sleep(100);		/* Improves reliability of USB<->Serial converters */
			a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: SetCommState failed with LastError %d\n",
			                                                                   GetLastError());
			return ICOM_SYS;
		}

		PurgeComm(p->phandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

		msec_sleep(50);		/* Improves reliability of USB<->Serial converters */

		p->write = icoms_ser_write;
		p->read = icoms_ser_read;
		p->ser_clearerr = nt_ser_clearerr;

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


/* ---------------------------------------------------------------------------------*/
/* Serial write, read */

/* Write the characters in the buffer out */
/* Data will be written up to the terminating nul */
/* Return relevant error status bits */
/* Set the icoms lserr value */
int
icoms_ser_write(
icoms *p,
char *wbuf,			/* null terminated unless nwch > 0 */
int nwch,			/* if > 0, number of characters to write */
double tout)
{
	COMMTIMEOUTS tmo;
	DWORD wbytes;
	int len;
	long ttop, top;				/* Total timeout period, timeout period */
	unsigned int stime, etime;	/* Start and end times of USB operation */
	int retrv = ICOM_OK;

	a1logd(p->log, 8, "\nicoms_ser_write: writing '%s'\n",
	       nwch > 0 ? icoms_tohex(wbuf, nwch) : icoms_fix(wbuf));

	if (!p->is_open) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_write: device not initialised\n");
		p->lserr = ICOM_SYS;
		return p->lserr;
	}

	if (nwch != 0)
		len = nwch;
	else
		len = strlen(wbuf);

	ttop = (int)(tout * 1000.0 + 0.5);        /* Total timeout period in msecs */

	a1logd(p->log, 8, "\nicoms_ser_write: ep 0x%x, bytes %d, ttop %d, quant %d\n", p->rd_ep, len, ttop, p->rd_qa);

	/* Set the timout value */
	tmo.ReadIntervalTimeout = 0;
	tmo.ReadTotalTimeoutMultiplier = 0;
	tmo.ReadTotalTimeoutConstant = ttop;
	tmo.WriteTotalTimeoutMultiplier = 0;
	tmo.WriteTotalTimeoutConstant = ttop;
	if (!SetCommTimeouts(p->phandle, &tmo)) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_write: SetCommTimeouts failed with %d\n",GetLastError());
		p->lserr = ICOM_SYS;
		return p->lserr;
	}

	etime = stime = msec_time();

	/* Until data is all written or we time out */
	for (top = ttop; top > 0 && len > 0;) {
		int rv;
		rv = WriteFile(p->phandle, wbuf, len, &wbytes, NULL);
		etime = msec_time();

		if (wbytes > 0) { /* Account for bytes done */
			a1logd(p->log, 8, "icoms_ser_write: wrote %d bytes\n",wbytes);
			wbuf += wbytes;
			len -= wbytes;
		}
		if (rv == 0) {
			DWORD errs;
			if (!ClearCommError(p->phandle,&errs,NULL))
				warning("icoms_ser_write: failed, and Clear error failed in %s at %d",__FILE__,__LINE__);
			if (errs & CE_BREAK)
				retrv |= ICOM_BRK; 
			if (errs & CE_FRAME)
				retrv |= ICOM_FER; 
			if (errs & CE_RXPARITY)
				retrv |= ICOM_PER; 
			if (errs & CE_RXOVER)
				retrv |= ICOM_OER; 
			a1logd(p->log, 8, "icoms_ser_write: read failed with 0x%x\n",rv);
			break;
		}

		top = ttop - (etime - stime);	/* Remaining time */
	}
	if (top <= 0) {			/* Must have timed out */
		a1logd(p->log, 8, "icoms_ser_write: timeout, took %d msec out of %d\n",etime - stime,ttop);
		retrv |= ICOM_TO; 
	}

	a1logd(p->log, 8, "icoms_ser_write: took %d msec, returning ICOM err 0x%x\n",etime - stime,retrv);

	p->lserr = retrv;
	return p->lserr;
}


/* Read characters into the buffer */
/* Return string will be terminated with a nul */
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
	COMMTIMEOUTS tmo;
	DWORD rbytes;
	int j;
	long ttop, top;			/* Total timeout period, timeout period */
	unsigned int stime, etime;		/* Start and end times of USB operation */
	char *rrbuf = rbuf;		/* Start of return buffer */
	int retrv = ICOM_OK;
	int nreads;				/* Number of reads performed */

	if (p->phandle == NULL) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_read: device not initialised\n");
		p->lserr = ICOM_SYS;
		return p->lserr;
	}

	if (bsize < 3) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_read: given too small a buffer (%d)\n",bsize);
		p->lserr = ICOM_SYS;
		return p->lserr;
	}

	for (j = 0; j < bsize; j++)
		rbuf[j] = 0;
 
	ttop = (int)(tout * 1000.0 + 0.5);        /* Total timeout period in msecs */

	a1logd(p->log, 8, "\nicoms_ser_read: bytes %d, ttop %d, ntc %d\n", bsize, ttop, ntc);

	/* Set the timout value */
	tmo.ReadIntervalTimeout = 20;			/* small inter character to detect tc */
	tmo.ReadTotalTimeoutMultiplier = 0;		/* No per byte */
	tmo.ReadTotalTimeoutConstant = ttop;	/* Just overall total */
	tmo.WriteTotalTimeoutMultiplier = 0;
	tmo.WriteTotalTimeoutConstant = ttop;
	if (!SetCommTimeouts(p->phandle, &tmo)) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_read: SetCommTimeouts failed with %d\n",GetLastError());
		p->lserr = ICOM_SYS;
		return p->lserr;
	}

	bsize -= 1;				/* Allow space for null */

	/* Until data is all read, we time out, or the user aborts */
	etime = stime = msec_time();
	top = ttop;
	j = (tc == NULL && ntc <= 0) ? -1 : 0;

	/* Until data is all read or we time out */
	for (nreads = 0; top > 0 && bsize > 0 && j < ntc ;) {
		int rv;
		rv = ReadFile(p->phandle, rbuf, bsize, &rbytes, NULL);
		etime = msec_time();
		nreads++;

		if (rbytes > 0) {	/* Account for bytes read */

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

		/* Deal with any errors */
		if (rv == 0) {
			DWORD errs;
			if (!ClearCommError(p->phandle,&errs,NULL))
				warning("icoms_ser_read: failed, and Clear error failed in %s at %d",__FILE__,__LINE__);
			if (errs & CE_BREAK)
				retrv |= ICOM_BRK; 
			if (errs & CE_FRAME)
				retrv |= ICOM_FER; 
			if (errs & CE_RXPARITY)
				retrv |= ICOM_PER; 
			if (errs & CE_RXOVER)
				retrv |= ICOM_OER; 
			a1logd(p->log, 8, "icoms_ser_read: read failed with 0x%x, rbuf = '%s'\n",rv,icoms_fix(rrbuf));
			break;
		}
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
	       etime - stime, tc == NULL && ntc > 0 ? icoms_tohex(rrbuf, rbuf - rrbuf)
	                                                    : icoms_fix(rrbuf), retrv);

	p->lserr = retrv;
	return p->lserr;
}

#endif /* ENABLE_SERIAL || ENABLE_FAST_SERIAL*/

