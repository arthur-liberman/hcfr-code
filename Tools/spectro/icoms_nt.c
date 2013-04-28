
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

/* Create and return a list of available serial ports or USB instruments for this system. */
/* We look at the registry key "HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM" */
/* to determine serial ports, and use libusb to discover USB instruments. */
/* Create and return a list of available serial ports or USB instruments for this system */
/* return icom error */
int icompaths_refresh_paths(icompaths *p) {
	int rv, usbend = 0;
	int i,j;
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

#ifdef ENABLE_SERIAL
	/* Look in the registry for serial ports */
	if ((stat = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM",
	                         0, KEY_READ, &sch)) != ERROR_SUCCESS) {
		a1logd(p->log, 1, "icoms_get_paths: There don't appear to be any serial ports\n");
		return ICOM_OK;			/* Maybe they have USB ports */
	}

	/* Look at all the values in this key */
	a1logd(p->log, 8, "icoms_get_paths: looking through all the values in the SERIALCOMM key\n");
	
	for (i = 0; ; i++) {
		char valname[500];
		DWORD vnsize = 500;
		DWORD vtype;
		char value[500];
		DWORD vsize = 500;

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
		valname[500-1] = '\000';
		value[500-1] = '\000';

		if (vtype != REG_SZ) {
			a1logw(p->log, "icoms_get_paths: RegEnumValue didn't return stringz type\n");
			continue;
		}

		/* Add the port to the list */
		p->add_serial(p, value, value);
		a1logd(p->log, 8, "icoms_get_paths: Added path '%s'\n",value);
	}
	if ((stat = RegCloseKey(sch)) != ERROR_SUCCESS) {
		a1logw(p->log, "icoms_get_paths: RegCloseKey failed with %d\n",stat);
	}
#endif /* ENABLE_SERIAL */

	/* Sort the COM keys so people don't get confused... */
	a1logd(p->log, 6, "icoms_get_paths: we now have %d entries and are about to sort them\n",p->npaths);
	for (i = usbend; i < (p->npaths-1); i++) {
		for (j = i+1; j < p->npaths; j++) {
			if (strcmp(p->paths[i]->name, p->paths[j]->name) > 0) {
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
#ifdef ENABLE_SERIAL
		if (p->phandle != NULL) {
			CloseHandle(p->phandle);
		}
#endif /* ENABLE_SERIAL */
		p->is_open = 0;
	}
}

#ifdef ENABLE_SERIAL

static int icoms_ser_write(icoms *p, char *wbuf, double tout);
static int icoms_ser_read(icoms *p, char *rbuf, int bsize, char tc, int ntc, double tout);

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
		              "       Baud Rate = %d\n"
		              "       Parity = %d\n"
		              "       Stop bits = %d\n"
		              "       Word length = %d\n"
		              ,p->name ,fc ,baud ,parity ,stop ,word);

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
			a1logd(p->log, 8, "icoms_set_ser_port: about to open serial port '%s'\n",p->spath);

			if ((p->phandle = CreateFile(
				p->spath,
				GENERIC_READ|GENERIC_WRITE,
				0,				/* Exclusive access */
				NULL,			/* No security */
				OPEN_EXISTING,	/* Does not make sense to create */
				0,				/* No overlapped I/O */
				NULL)			/* NULL template */
			) == INVALID_HANDLE_VALUE) {
				a1loge(p->log, ICOM_SYS, "icoms_set_ser_port: open port '%s' failed with LastError %d\n",p->spath,GetLastError());
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
		dcb.fDsrSensitivity = FALSE;				/* Not using Dsr Flow control */
		dcb.fTXContinueOnXoff = TRUE;				/* */
		dcb.fOutX = FALSE;							/* No default Xon/Xoff flow control */
		dcb.fInX = FALSE;							/* No default Xon/Xoff flow control */
		dcb.fErrorChar = FALSE;
		dcb.fNull = FALSE;
		dcb.fRtsControl = RTS_CONTROL_ENABLE;		/* Turn RTS on during connection */
		dcb.fAbortOnError = TRUE;

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
/* Serial write/read */

/* Write the characters in the buffer out */
/* Data will be written up to the terminating nul */
/* Return relevant error status bits */
/* Set the icoms lserr value */
static int
icoms_ser_write(
icoms *p,
char *wbuf,
double tout)
{
	COMMTIMEOUTS tmo;
	DWORD wbytes;
	int c, len;
	long toc, i, top;		/* Timout count, counter, timeout period */
	int rv = ICOM_OK;

	a1logd(p->log, 8, "icoms_ser_write: About to write '%s' ",icoms_fix(wbuf));
	if (!p->is_open) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_write: device not initialised\n");
		p->lserr = rv = ICOM_SYS;
		return rv;
	}

	len = strlen(wbuf);
	tout *= 1000.0;		/* Timout in msec */

	top = 100;						/* Timeout period in msecs */
	toc = (int)(tout/top + 0.5);	/* Number of timout periods in timeout */
	if (toc < 1)
		toc = 1;

	/* Set the timout value */
	tmo.ReadIntervalTimeout = top;
	tmo.ReadTotalTimeoutMultiplier = 0;
	tmo.ReadTotalTimeoutConstant = top;
	tmo.WriteTotalTimeoutMultiplier = top;
	tmo.WriteTotalTimeoutConstant = 0;
	if (!SetCommTimeouts(p->phandle, &tmo)) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_write: SetCommTimeouts failed with %d\n",GetLastError());
		p->lserr = rv = ICOM_SYS;
		return rv;
	}

	/* Until data is all written or we time out */
	for (i = toc; i > 0 && len > 0;) {
		if (!WriteFile(p->phandle, wbuf, len, &wbytes, NULL)) {
			DWORD errs;
			if (!ClearCommError(p->phandle,&errs,NULL))
				error("Write to COM port failed, and Clear error failed");
			if (errs & CE_BREAK)
				rv |= ICOM_BRK; 
			if (errs & CE_FRAME)
				rv |= ICOM_FER; 
			if (errs & CE_RXPARITY)
				rv |= ICOM_PER; 
			if (errs & CE_RXOVER)
				rv |= ICOM_OER; 
			break;
		} else if (wbytes == 0) { 
			i--;			/* Timeout */
		} else if (wbytes > 0) { /* Account for bytes done */
			i = toc;
			len -= wbytes;
			wbuf += len;
		}
	}
	if (i <= 0) {		/* Timed out */
		rv |= ICOM_TO;
	}
	a1logd(p->log, 8, "icoms_ser_write: returning ICOM err 0x%x\n",rv);

	p->lserr = rv;
	return rv;
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
	COMMTIMEOUTS tmo;
	DWORD rbytes;
	int c, j;
	long toc, i, top;		/* Timout count, counter, timeout period */
	char *rrbuf = rbuf;		/* Start of return buffer */
	DCB dcb;
	int rv = ICOM_OK;

	if (p->phandle == NULL) {
		a1loge(p->log, ICOM_SYS, "icoms_read: device not initialised\n");
		p->lserr = rv = ICOM_SYS;
		return rv;
	}

	if (bsize < 3) {
		a1loge(p->log, ICOM_SYS, "icoms_read: given too small a buffer\n");
		p->lserr = rv = ICOM_SYS;
		return rv;
	}

#ifdef NEVER
	/* The Prolific 2303 USB<->serial seems to choke on this, */ 
	/* so we just put up with a 100msec delay at the end of each */
	/* reply. */
	if (GetCommState(p->phandle, &dcb) == FALSE) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_read: GetCommState failed with %d\n",GetLastError());
		p->lserr = rv = ICOM_SYS;
		return rv;
	}

	dcb.EofChar = tc;

	if (!SetCommState(p->phandle, &dcb)) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_read: SetCommState failed %d\n",GetLastError());
		p->lserr = rv = ICOM_SYS;
		return rv;
	}
#endif
	p->tc = tc;

	tout *= 1000.0;		/* Timout in msec */
	bsize--;	/* Allow space for null */

	top = 100;						/* Timeout period in msecs */
	toc = (int)(tout/top + 0.5);	/* Number of timout periods in timeout */
	if (toc < 1)
		toc = 1;

	/* Set the timout value */
	tmo.ReadIntervalTimeout = top;
	tmo.ReadTotalTimeoutMultiplier = 0;
	tmo.ReadTotalTimeoutConstant = top;
	tmo.WriteTotalTimeoutMultiplier = top;
	tmo.WriteTotalTimeoutConstant = 0;
	if (!SetCommTimeouts(p->phandle, &tmo)) {
		a1loge(p->log, ICOM_SYS, "icoms_ser_read: SetCommTimeouts failed with %d\n",GetLastError());
		p->lserr = rv = ICOM_SYS;
		return rv;
	}

	/* Until data is all read or we time out */
	for (i = toc, j = 0; i > 0 && bsize > 1 && j < ntc ;) {
		if (!ReadFile(p->phandle, rbuf, bsize, &rbytes, NULL)) {
			DWORD errs;
			if (!ClearCommError(p->phandle,&errs,NULL))
				error("Read from COM port failed, and Clear error failed");
			if (errs & CE_BREAK)
				rv |= ICOM_BRK; 
			if (errs & CE_FRAME)
				rv |= ICOM_FER; 
			if (errs & CE_RXPARITY)
				rv |= ICOM_PER; 
			if (errs & CE_RXOVER)
				rv |= ICOM_OER; 
			break;
		} else if (rbytes == 0) { 
			i--;			/* Timeout */
		} else if (rbytes > 0) { /* Account for bytes done */
			i = toc;
			bsize -= rbytes;
			while(rbytes--) {	/* Count termination characters */
				if (*rbuf++ == tc)
					j++;
			}
		}
	}
	if (i <= 0) {			/* timed out */
		rv |= ICOM_TO;
	}
	*rbuf = '\000';
	a1logd(p->log, 8, "icoms_ser_read: returning '%s' ICOM err 0x%x\n",icoms_fix(rrbuf),rv);

	p->lserr = rv;
	return rv;
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
	p->log = del_a1log(p->log);		/* unref */
	free (p);
}

