
 /* General instrument + serial I/O support */

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

/* These routines supliement the class code in ntio.c and unixio.c */
/* with common and USB specific routines */
/* Rename to icoms.c ?? */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#if defined(UNIX)
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#endif
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#else
#include "sa_config.h"
#endif
#include "numsup.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"


/* ----------------------------------------------------- */

/* Fake display & instrument device */
icompath icomFakeDevice = { instFakeDisp, "Fake Display Device" };


/* Free an icompath */
static
void icompath_del_contents(icompath *p) {

	if (p->name != NULL)
		free(p->name);
#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	if (p->spath != NULL)
		free(p->spath);
#endif /* ENABLE_SERIAL */
#ifdef ENABLE_USB
	usb_del_usb_idevice(p->usbd);
	hid_del_hid_idevice(p->hidd);
#endif /* ENABLE_USB */
}

/* Free an icompath */
static void icompath_del(icompath *p) {

	icompath_del_contents(p);
	free(p);
}

/* Delete the last path */
static void icompaths_del_last_path(
	icompaths *p
) {
	if (p->npaths == 0)
		return;

	icompath_del(p->paths[p->npaths-1]);
	p->paths[p->npaths-1] = NULL;
	p->npaths--;
}

/* Return the last path */
static icompath *icompaths_get_last_path(
	icompaths *p
) {
	if (p->npaths == 0)
		return NULL;

	return p->paths[p->npaths-1];
}

/* Return the path corresponding to the port number, or NULL if out of range */
static icompath *icompaths_get_path(
	icompaths *p, 
	int port		/* Enumerated port number, 1..n */
) {
	if (port == FAKE_DEVICE_PORT)
		return &icomFakeDevice;


	if (port <= 0 || port > p->npaths)
		return NULL;

	return p->paths[port-1];
}

static void icompaths_clear(icompaths *p) {

	/* Free any old list */
	if (p->paths != NULL) {
		int i;
		for (i = 0; i < p->npaths; i++) {
			icompath_del(p->paths[i]);
		}
		free(p->paths);
		p->npaths = 0;
		p->paths = NULL;
	}
}

/* Add an empty new path. */
/* Return icom error */
static int icompaths_add_path(icompaths *p) {
	if (p->paths == NULL) {
		if ((p->paths = (icompath **)calloc(sizeof(icompath *), 1 + 1)) == NULL) {
			a1loge(p->log, ICOM_SYS, "icompaths: calloc failed!\n");
			return ICOM_SYS;
		}
	} else {
		icompath **npaths;
		if ((npaths = (icompath **)realloc(p->paths,
		                     sizeof(icompath *) * (p->npaths + 2))) == NULL) {
			a1loge(p->log, ICOM_SYS, "icompaths: realloc failed!\n");
			return ICOM_SYS;
		}
		p->paths = npaths;
		p->paths[p->npaths+1] = NULL;
	}
	if ((p->paths[p->npaths] = calloc(sizeof(icompath), 1)) == NULL) {
		a1loge(p->log, ICOM_SYS, "icompaths: malloc failed!\n");
		return ICOM_SYS;
	}
	p->npaths++;
	p->paths[p->npaths] = NULL;
	return ICOM_OK;
}

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)

/* Add a serial path */
/* return icom error */
static int icompaths_add_serial(icompaths *p, char *name, char *spath, int fast) {
	int rv;

	if ((rv = icompaths_add_path(p)) != ICOM_OK)
		return rv;
	
	if ((p->paths[p->npaths-1]->name = strdup(name)) == NULL) {
		a1loge(p->log, ICOM_SYS, "icompaths: strdup failed!\n");
		return ICOM_SYS;
	}
	if ((p->paths[p->npaths-1]->spath = strdup(spath)) == NULL) {
		a1loge(p->log, ICOM_SYS, "icompaths: strdup failed!\n");
		return ICOM_SYS;
	}
	p->paths[p->npaths-1]->fast = fast;

	return ICOM_OK;
}

/* Modify a serial path to add the instrument type */
int icompaths_set_serial_itype(icompath *p, instType itype) {
	char pname[400], *cp;

	p->itype = itype;

	/* Strip any existing description in brackets */
	if ((cp = strchr(p->name, '(')) != NULL && cp > p->name)
		cp[-1] = '\000';
	sprintf(pname,"%s (%s)", p->name, inst_name(itype));
	cp = p->name;
	if ((p->name = strdup(pname)) == NULL) {
		a1loge(g_log, ICOM_SYS, "icompaths_set_serial_itype: strdup path failed!\n");
		return ICOM_SYS;
	}
	free(cp);
	return ICOM_OK;
}

#endif /* ENABLE_SERIAL */

#ifdef ENABLE_USB

/* Set an icompath details */
/* return icom error */
static
int icompath_set_usb(a1log *log, icompath *p, char *name, unsigned int vid, unsigned int pid,
                              int nep, struct usb_idevice *usbd, instType itype) {
	int rv;

	if ((p->name = strdup(name)) == NULL) {
		a1loge(log, ICOM_SYS, "icompath: strdup failed!\n");
		return ICOM_SYS;
	}

	p->nep = nep;
	p->vid = vid;
	p->pid = pid;
	p->usbd = usbd;
	p->itype = itype;

	return ICOM_OK;
}

/* Add a usb path. usbd is taken, others are copied. */
/* return icom error */
static int icompaths_add_usb(icompaths *p, char *name, unsigned int vid, unsigned int pid,
                              int nep, struct usb_idevice *usbd, instType itype) {
	int rv;

	if ((rv = icompaths_add_path(p)) != ICOM_OK)
		return rv;

	return icompath_set_usb(p->log, p->paths[p->npaths-1], name, vid, pid, nep, usbd, itype);

	return ICOM_OK;
}

/* Add an hid path. hidd is taken, others are copied. */
/* return icom error */
static int icompaths_add_hid(icompaths *p, char *name, unsigned int vid, unsigned int pid,
                              int nep, struct hid_idevice *hidd, instType itype) {
	int rv;

	if ((rv = icompaths_add_path(p)) != ICOM_OK)
		return rv;
	
	if ((p->paths[p->npaths-1]->name = strdup(name)) == NULL) {
		a1loge(p->log, ICOM_SYS, "icompaths: strdup failed!\n");
		return ICOM_SYS;
	}

	p->paths[p->npaths-1]->nep = nep;
	p->paths[p->npaths-1]->vid = vid;
	p->paths[p->npaths-1]->pid = pid;
	p->paths[p->npaths-1]->hidd = hidd;
	p->paths[p->npaths-1]->itype = itype;

	return ICOM_OK;
}
#endif /* ENABLE_USB */

static void icompaths_del(icompaths *p) {
	icompaths_clear(p);

	p->log = del_a1log(p->log);		/* Unreference it */
	free(p);
}

int icompaths_refresh_paths(icompaths *p);	/* Defined in platform specific */

/* Allocate an icom paths and set it to the list of available devices */
/* Return NULL on error */
icompaths *new_icompaths(a1log *log) {
	icompaths *p;
	if ((p = (icompaths *)calloc(sizeof(icompaths), 1)) == NULL) {
		a1loge(log, ICOM_SYS, "new_icompath: calloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(log);

	p->clear         = icompaths_clear;
	p->refresh       = icompaths_refresh_paths;
	p->del_last_path = icompaths_del_last_path;
	p->get_last_path = icompaths_get_last_path;
	p->get_path      = icompaths_get_path;
#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	p->add_serial    = icompaths_add_serial;
#endif /* ENABLE_SERIAL */
#ifdef ENABLE_USB
	p->add_usb       = icompaths_add_usb;
	p->add_hid       = icompaths_add_hid;
#endif /* ENABLE_USB */
	p->del           = icompaths_del;

	if (icompaths_refresh_paths(p)) {
		a1loge(log, ICOM_SYS, "new_icompaths: icompaths_refresh_paths failed!\n");
		return NULL;
	}

	return p;
}


/* ----------------------------------------------------- */

/* Copy an icompath into an icom */
/* return icom error */
static int icom_copy_path_to_icom(icoms *p, icompath *pp) {
	int rv;

	if ((p->name = strdup(pp->name)) == NULL) {
		a1loge(p->log, ICOM_SYS, "copy_path_to_icom: malloc name failed\n");
		return ICOM_SYS;
	}
#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	if (pp->spath != NULL) {
		if ((p->spath = strdup(pp->spath)) == NULL) {
			a1loge(p->log, ICOM_SYS, "copy_path_to_icom: malloc spath failed\n");
			return ICOM_SYS;
		}
		p->fast = pp->fast;
	} else {
		p->spath = NULL;
	}
#endif /* ENABLE_SERIAL */
#ifdef ENABLE_USB
	p->nep = pp->nep;
	p->vid = pp->vid;
	p->pid = pp->pid;
	if ((rv = usb_copy_usb_idevice(p, pp)) != ICOM_OK)
		return rv;
	if ((rv = hid_copy_hid_idevice(p, pp)) != ICOM_OK)
		return rv;
#endif
	p->itype = pp->itype;

	return ICOM_OK;
}

/* Return the port type */
static icom_type icoms_port_type(
icoms *p
) {

#ifdef ENABLE_USB
	if (p->hidd != NULL)
		return icomt_hid;
	if (p->usbd != NULL)
		return icomt_usb;
#endif

	return icomt_serial;
}

/* ----------------------------------------------------- */

/* Include the icoms implementation dependent function implementations */
#ifdef NT
# include "icoms_nt.c"
#endif
#if defined(UNIX)
# include "icoms_ux.c"
#endif

/* write and read convenience function */
/* return icom error */
static int
icoms_write_read(
icoms *p,
char *wbuf,			/* Write puffer */
char *rbuf,			/* Read buffer */
int bsize,			/* Buffer size */
char *tc,			/* Terminating characers, NULL for none */
int ntc,			/* Number of terminating characters needed to terminate */
double tout
) {
	int rv = ICOM_OK;

	a1logd(p->log, 8, "icoms_write_read: called with '%s'\n",icoms_fix(wbuf));

	if (p->write == NULL || p->read == NULL) {	/* Neither serial nor USB ? */
		a1loge(p->log, ICOM_NOTS, "icoms_write_read: Neither serial nor USB device!\n");
		return ICOM_NOTS;
	}

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	/* Flush any stray chars if serial */
	if (p->usbd == NULL && p->hidd == NULL) {
		int debug = p->log->debug;

		if (debug < 8)
			p->log->debug =  0;
		for (; rv == ICOM_OK;) 	/* Until we get a timeout */
			rv = p->read(p, rbuf, bsize, '\000', 100000, 0.01);
		p->log->debug = debug;
		rv = ICOM_OK;
	}
#endif

	/* Write the write data */
	rv = p->write(p, wbuf, tout);

	/* Return error if coms */
	if (rv != ICOM_OK) {
		a1logd(p->log, 8, "icoms_write_read: failed - returning 0x%x\n",rv);
		return rv;
	}

	/* Read response */
	rv = p->read(p, rbuf, bsize, tc, ntc, tout);
	
	a1logd(p->log, 8, "icoms_write_read: returning 0x%x\n",rv);

	return rv;
}

/* icoms Constructor */
/* Return NULL on error */
icoms *new_icoms(
	icompath *ipath,	/* instrument to open */
	a1log *log			/* log to take reference from, NULL for default */
) {
	icoms *p;
	if ((p = (icoms *)calloc(sizeof(icoms), 1)) == NULL) {
		a1loge(log, ICOM_SYS, "new_icoms: calloc failed!\n");
		return NULL;
	}

	if ((p->name = strdup(ipath->name)) == NULL) {
		a1loge(log, ICOM_SYS, "new_icoms: strdup failed!\n");
		return NULL;
	}
	p->itype = ipath->itype;

	/* Copy ipath info to this icoms */
	if (icom_copy_path_to_icom(p, ipath)) {
		free(p);
		return NULL;
	}

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
#ifdef NT
	p->phandle = NULL;
#endif
#ifdef UNIX
	p->fd = -1;
#endif
	p->fc = fc_nc;
	p->br = baud_nc;
	p->py = parity_nc;
	p->sb = stop_nc;
	p->wl = length_nc;
#endif	/* ENABLE_SERIAL */

	p->lserr = 0;
//	p->tc = -1;

	p->log = new_a1log_d(log);
	p->debug = p->log->debug;		/* Legacy code */
	
	p->close_port = icoms_close_port;

	p->port_type = icoms_port_type;
#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	p->set_ser_port = icoms_set_ser_port;
#endif	/* ENABLE_SERIAL */

	p->write = NULL;	/* Serial open or set_methods will set */
	p->read = NULL;
	p->write_read = icoms_write_read;

	p->del = icoms_del;

#ifdef ENABLE_USB
	usb_set_usb_methods(p);
	hid_set_hid_methods(p);
#endif /* ENABLE_USB */

	return p;
}

/* ---------------------------------------------------------------------------------*/
/* utilities */

/* Emit a "normal" beep */
void normal_beep() {
	/* 0msec delay, 1.0KHz for 200 msec */
	msec_beep(0, 1000, 200);
}

/* Emit a "good" beep */
void good_beep() {
	/* 0msec delay, 1.2KHz for 200 msec */
	msec_beep(0, 1200, 200);
}

/* Emit a "bad" double beep */
void bad_beep() {
	/* 0msec delay, 800Hz for 200 msec */
	msec_beep(0, 800, 200);
	/* 500msec delay, 800Hz for 200 msec */
	msec_beep(350, 800, 200);
}

/* Convert control chars to ^[A-Z] notation in a string */
/* Can be called 5 times without reusing the static buffer */
char *
icoms_fix(char *ss) {
	static unsigned char buf[5][1005];
	static int ix = 0;
	unsigned char *d;
	unsigned char *s = (unsigned char *)ss;
	if (++ix >= 5)
		ix = 0;
	if (ss == NULL) {
		strcpy((char *)buf[ix],"(null)");
		return (char *)buf[ix];
	}
	for(d = buf[ix]; (d - buf[ix]) < 1000;) {
		if (*s < ' ' && *s > '\000') {
			*d++ = '^';
			*d++ = *s++ + '@';
		} else if (*s >= 0x80) {
			*d++ = '\\';
			*d++ = '0' + ((*s >> 6) & 0x3);
			*d++ = '0' + ((*s >> 3) & 0x7);
			*d++ = '0' + ((*s++ >> 0) & 0x7);
		} else {
			*d++ = *s++;
		}
		if (s[-1] == '\000')
			break;
	}
	*d++ = '.';
	*d++ = '.';
	*d++ = '.';
	*d++ = '\000';
	return (char *)buf[ix];
}

/* Convert a limited binary buffer to a list of hex */
char *
icoms_tohex(unsigned char *s, int len) {
	static char buf[64 * 3 + 10];
	int i;
	char *d;

	buf[0] = '\000';
	for(i = 0, d = buf; i < 64 && i < len; i++, s++) {
		sprintf(d, "%s%02x", i > 0 ? " " : "", *s);
		d += strlen(d);
	}
	if (i < len)
		sprintf(d, " ...");

	return buf;
}

