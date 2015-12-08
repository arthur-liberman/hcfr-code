
 /* Windows NT serial I/O class */

/* 
 * Argyll Color Correction System
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

#include <conio.h>

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
instType fast_ser_inst_type(icoms *p, int tryhard, void *, void *); 
#endif /* ENABLE_SERIAL */

/* Create and return a list of available serial ports or USB instruments for this system. */
/* We look at the registry key "HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM" */
/* to determine serial ports, and use libusb to discover USB instruments. */
/* Create and return a list of available serial ports or USB instruments for this system */
/* return icom error */
int icompaths_refresh_paths(icompaths *p) {
	int rv, usbend = 0;
	int i, j;
	LONG stat;
	HKEY sch;		/* Serial coms handle */

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

	a1logd(p->log, 6, "icoms_get_paths: got %d paths, looking up the registry for serial ports\n",p->npaths);

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	// (Beware KEY_WOW64_64KEY ?)

#define MXKSIZE 500
#define MXVSIZE 300

	/* Look in the registry for serial ports */
	if ((stat = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM",
	                         0, KEY_READ, &sch)) != ERROR_SUCCESS) {
		a1logd(p->log, 1, "icoms_get_paths: There don't appear to be any serial ports\n");
		return ICOM_OK;			/* Maybe they have USB ports */
	}

	/* Look at all the values in this key */
	a1logd(p->log, 8, "icoms_get_paths: looking through all the values in the SERIALCOMM key\n");

	for (i = 0; ; i++) {
		char valname[MXKSIZE], *vp;
		DWORD vnsize = MXKSIZE;
		DWORD vtype;
		char value[MXVSIZE];
		DWORD vsize = MXVSIZE;
		icom_ser_attr sattr = icom_normal;

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
			a1logd(p->log, 8, "icoms_get_paths: got ERROR_NO_MORE_ITEMS\n");
			break;
		}
		if (stat == ERROR_MORE_DATA		/* Hmm. Should expand buffer size */
		 || stat != ERROR_SUCCESS) {
			a1logw(p->log, "icoms_get_paths: RegEnumValue failed with %d\n",stat);
			break;
		}
		valname[MXKSIZE-1] = '\000';
		value[MXVSIZE-1] = '\000';

		if (vtype != REG_SZ) {
			a1logw(p->log, "icoms_get_paths: RegEnumValue didn't return stringz type\n");
			continue;
		}

		if ((vp = strrchr(valname, '\\')) == NULL)
			vp = valname;
		else
			vp++;

		/* See if it looks like a fast port */
		if (strncmp(vp, "VCP", 3) == 0) {				/* Virtual */
			sattr |= icom_fast;
		}

		if (strncmp(vp, "BtPort", 6) == 0) {		/* Blue tooth */
			sattr |= icom_fast;
			sattr |= icom_bt;
		}

#ifndef ENABLE_SERIAL
		if (sattr & icom_fast) {		/* Only add fast ports if !ENABLE_SERIAL */
#endif
		/* Add the port to the list */
		p->add_serial(p, value, value, sattr);
		a1logd(p->log, 8, "icoms_get_paths: Added path '%s' sattr 0x%x\n",value,sattr);
#ifndef ENABLE_SERIAL
		}
#endif

		/* If fast, try and identify it */
		if (sattr & icom_fast) {
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
	if ((stat = RegCloseKey(sch)) != ERROR_SUCCESS) {
		a1logw(p->log, "icoms_get_paths: RegCloseKey failed with %d\n",stat);
	}
#endif /* ENABLE_SERIAL || ENABLE_FAST_SERIAL */

	/* Sort the COM keys so people don't get confused... */
	/* Sort identified instruments ahead of unknown serial ports */
	a1logd(p->log, 6, "icoms_get_paths: we now have %d entries and are about to sort them\n",p->npaths);
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

	a1logd(p->log, 8, "icoms_get_paths: returning %d paths and ICOM_OK\n",p->npaths);

	return ICOM_OK;
}


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
		if (p->phandle != NULL) {
			CloseHandle(p->phandle);
		}
#endif /* ENABLE_SERIAL */
		p->is_open = 0;
	}
}

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)

static int icoms_ser_write(icoms *p, char *wbuf, int nwch, double tout);
static int icoms_ser_read(icoms *p, char *rbuf, int bsize, int *bread,
                                    char *tc, int ntc, double tout);

#ifndef CBR_921600
# define CBR_921600 921600
#endif

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

	a1logd(p->log, 8, "icoms_set_ser_port: About to set port characteristics:\n"
		              "       Port name = %s\n"
		              "       Flow control = %d\n"
		              "       Baud Rate = %s\n"
		              "       Parity = %d\n"
		              "       Stop bits = %d\n"
		              "       Word length = %d\n"
		              ,p->name ,fc ,baud_rate_to_str(baud) ,parity ,stop ,word);

	if (p->is_open)
		p->close_port(p);

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
			char buf[50];	/* Temporary for COM device path */

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
			p->is_open = 1;
		}

		if (GetCommState(p->phandle, &dcb) == FALSE) {
			CloseHandle(p->phandle);
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
//		dcb.fAbortOnError = TRUE;					// Hmm. Stuffs up FTDI. Is it needed ?
		dcb.fAbortOnError = FALSE;

		switch (p->fc) {
			case fc_nc:
				CloseHandle(p->phandle);
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: illegal flow control %d\n",p->fc);
				return ICOM_SYS;
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
			case baud_921600:
				dcb.BaudRate = CBR_921600;
				break;
			default:
				CloseHandle(p->phandle);
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: illegal baud rate! (0x%x)\n",p->br);
				return ICOM_SYS;
		}

		PurgeComm(p->phandle, PURGE_TXCLEAR | PURGE_RXCLEAR);

		if (!SetCommState(p->phandle, &dcb)) {
			CloseHandle(p->phandle);
			a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: SetCommState failed with LastError %d\n",
			                                                                   GetLastError());
			return ICOM_SYS;
		}

		PurgeComm(p->phandle, PURGE_TXCLEAR | PURGE_RXCLEAR);

		p->write = icoms_ser_write;
		p->read = icoms_ser_read;

	}
	a1logd(p->log, 8, "icoms_set_ser_port: port characteristics set ok\n");

	return ICOM_OK;
}

/* ---------------------------------------------------------------------------------*/
/* Serial write, read */

/* Write the characters in the buffer out */
/* Data will be written up to the terminating nul */
/* Return relevant error status bits */
/* Set the icoms lserr value */
static int
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
				error("icoms_ser_write: failed, and Clear error failed");
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
static int
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
				error("icoms_ser_read: failed, and Clear error failed");
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
#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	if (p->spath != NULL)
		free(p->spath);
#endif
	p->log = del_a1log(p->log);
	if (p->name != NULL)
		free(p->name);
	p->log = del_a1log(p->log);		/* unref */
	free (p);
}

