
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
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"


icom_type dev_category(instType itype);
int serial_get_paths(icompaths *p, icom_type mask);

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

/* Delete the last combined path */
/* (Assume this is before icompaths_make_dslists() has been called) */ 
static void icompaths_del_last_path(
	icompaths *p
) {
	icom_dtix ix = dtix_combined;
	
	if (p->ndpaths[ix] == 0)
		return;

	icompath_del(p->dpaths[ix][p->ndpaths[ix]-1]);
	p->dpaths[ix][p->ndpaths[ix]-1] = NULL;
	p->ndpaths[ix]--;
}

/* Return the last combined path */
/* (Assume this is before icompaths_make_dslists() has been called) */ 
static icompath *icompaths_get_last_path(
	icompaths *p
) {
	icom_dtix ix = dtix_combined;

	if (p->ndpaths[ix] == 0)
		return NULL;

	return p->dpaths[ix][p->ndpaths[ix]-1];
}

/* Return the instrument path corresponding to the port number, or NULL if out of range */
static icompath *icompaths_get_path(
	icompaths *p, 
	int port		/* Enumerated port number, 1..n */
) {
	if (port == FAKE_DEVICE_PORT)
		return &icomFakeDevice;



	if (port <= 0 || port > p->ndpaths[dtix_inst])
		return NULL;

	return p->dpaths[dtix_inst][port-1];
}

/* Return the device path corresponding to the port number, or NULL if out of range */
static icompath *icompaths_get_path_sel(
	icompaths *p, 
	icom_dtix dtix,	/* Device type list */
	int port			/* Enumerated port number, 1..n */
) {
	/* Check it is an enumeration */
	if (dtix != dtix_combined
	 && dtix != dtix_inst
	 && dtix != dtix_3dlut
	 && dtix != dtix_vtpg
	 && dtix != dtix_printer
	 && dtix != dtix_cmfm) {
//printf("~1 unrec index\n");
		return NULL;
	}

	if (dtix == dtix_inst) {
//printf("~1 inst\n");
		return icompaths_get_path(p, port);
	}
	
	if (port <= 0 || port > p->ndpaths[dtix]) {
//printf("~1 port %d out of range %d - %d\n", port,0, p->ndpaths[dtix]);
		return NULL;
	}

//printf("~1 OK index for port %d\n",port);
	return p->dpaths[dtix][port-1];
}

/* Clear a single device paths list */
static void icompaths_clear(icompaths *p, icom_dtix ix) {

	if (p->dpaths[ix] != NULL) {
		int i;
		/* If underlying owner of icompaths */
		if (ix == dtix_combined) {
			for (i = 0; i < p->ndpaths[ix]; i++) {
				icompath_del(p->dpaths[ix][i]);
			}
		}
		free(p->dpaths[ix]);
		p->dpaths[ix] = NULL;
		p->ndpaths[ix] = 0;

		/* Clear instrument alias */
		if (ix == dtix_inst) {
			p->npaths = 0;
			p->paths = NULL;
		}
	}
}

/* Clear all device paths */
static void icompaths_clear_all(icompaths *p) {

	if (p == NULL)
		return;

	/* Free any old instrument list */
	icompaths_clear(p, dtix_inst);
	icompaths_clear(p, dtix_3dlut);
	icompaths_clear(p, dtix_vtpg);
	icompaths_clear(p, dtix_printer);
	icompaths_clear(p, dtix_cmfm);
	icompaths_clear(p, dtix_combined);
}

/* Add the give path to the given list. */
/* If the path is NULL, allocat an empty */
/* one and add it to the combined list */
/* Return icom error */
static int icompaths_add_path(icompaths *p, int ix, icompath *xp) {
	if (xp == NULL)
		ix = dtix_combined;
	if (p->dpaths[ix] == NULL) {
		if ((p->dpaths[ix] = (icompath **)calloc(1 + 1, sizeof(icompath *))) == NULL) {
			a1loge(p->log, ICOM_SYS, "icompaths: calloc failed!\n");
			return ICOM_SYS;
		}
	} else {
		icompath **npaths;
		if ((npaths = (icompath **)realloc(p->dpaths[ix],
		                     sizeof(icompath *) * (p->ndpaths[ix] + 2))) == NULL) {
			a1loge(p->log, ICOM_SYS, "icompaths: realloc failed!\n");
			return ICOM_SYS;
		}
		p->dpaths[ix] = npaths;
		p->dpaths[ix][p->ndpaths[ix]+1] = NULL;
	}
	if (xp == NULL) {
		if ((xp = calloc(1, sizeof(icompath))) == NULL) {
			a1loge(p->log, ICOM_SYS, "icompaths: malloc failed!\n");
			return ICOM_SYS;
		}
	} 
	p->dpaths[ix][p->ndpaths[ix]] = xp;
	p->ndpaths[ix]++;
	p->dpaths[ix][p->ndpaths[ix]] = NULL;
	return ICOM_OK;
}

/* Based on each icompath's dctype, create aliase lists for everything */
/* on the combined path based on each device type. */
/* (We are only called after clearing all lists) */ 
int icompaths_make_dslists(icompaths *p) {
	int rv, i;

	for (i = 0; i < p->ndpaths[dtix_combined]; i++) {
		icompath *xp = p->dpaths[dtix_combined][i];

		if (xp == NULL)
			break;

		a1logd(g_log, 8, "icompaths_make_dslists '%s' dctype 0x%x\n",xp->name,xp->dctype);

		if (xp->dctype & icomt_instrument) {
			if ((rv = icompaths_add_path(p, dtix_inst, xp)) != ICOM_OK)
				return rv;
		}
		if (xp->dctype & icomt_v3dlut) {
			if ((rv = icompaths_add_path(p, dtix_3dlut, xp)) != ICOM_OK)
				return rv;
		}
		if (xp->dctype & icomt_vtpg) {
			if ((rv = icompaths_add_path(p, dtix_vtpg, xp)) != ICOM_OK)
				return rv;
		}
		if (xp->dctype & icomt_printer) {
			if ((rv = icompaths_add_path(p, dtix_printer, xp)) != ICOM_OK)
				return rv;
		}
		if (xp->dctype & icomt_cmfm) {
			if ((rv = icompaths_add_path(p, dtix_cmfm, xp)) != ICOM_OK)
				return rv;
		}
	}

	/* Maintain backwards compatible instrument list alias */
	p->npaths = p->ndpaths[dtix_inst];
	p->paths = p->dpaths[dtix_inst];
	
	return ICOM_OK;
}

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)

/* Add a serial path. */
/* return icom error */
static int icompaths_add_serial(icompaths *p, char *name, char *spath, icom_type dctype)  {
	icom_dtix ix = dtix_combined;
	icompath *xp;
	int rv;

	if ((rv = icompaths_add_path(p, ix, NULL)) != ICOM_OK)
		return rv;

	xp = p->dpaths[ix][p->ndpaths[ix]-1];

	a1logd(g_log, 8, "icompaths_add_serial got '%s' dctype 0x%x\n",name,dctype);

	/* Type of port, port attributes, device category */
	xp->dctype |= icomt_cat_any;		/* Assume any for now */
	xp->dctype |= icomt_serial;
	xp->dctype |= icomt_seriallike;
	xp->dctype |= dctype;

	if ((xp->name = strdup(name)) == NULL) {
		a1loge(p->log, ICOM_SYS, "icompaths: strdup failed!\n");
		return ICOM_SYS;
	}
	if ((xp->spath = strdup(spath)) == NULL) {
		a1loge(p->log, ICOM_SYS, "icompaths: strdup failed!\n");
		return ICOM_SYS;
	}

	a1logd(g_log, 8, "icompaths_add_serial returning '%s' dctype 0x%x\n",xp->name,xp->dctype);

	return ICOM_OK;
}

/* Modify a serial path to add the device type */
int icompaths_set_serial_itype(icompath *p, devType itype) {
	char pname[400], *cp;

	/* Convert device type to category */
	p->dctype = (p->dctype & ~icomt_cat_mask) | dev_category(itype);

	p->dtype = itype;

	/* Strip any existing description in brackets */
	if ((cp = strchr(p->name, '(')) != NULL && cp > p->name)
		cp[-1] = '\000';
	sprintf(pname,"%s (%s)", p->name, inst_name(itype));
	cp = p->name;
	if ((p->name = strdup(pname)) == NULL) {
		p->name = cp;
		a1loge(g_log, ICOM_SYS, "icompaths_set_serial_itype: strdup path failed!\n");
		return ICOM_SYS;
	}
	free(cp);

	a1logd(g_log, 8, "icompaths_set_serial_itype '%s' returning dctype 0x%x\n",p->name,p->dctype);

	return ICOM_OK;
}

#endif /* ENABLE_SERIAL */

#ifdef ENABLE_USB

/* Set an icompath details. */
/* return icom error */
static
int icompath_set_usb(a1log *log, icompath *p, char *name, unsigned int vid, unsigned int pid,
                              int nep, struct usb_idevice *usbd, devType itype) {
	int rv;


	if ((p->name = strdup(name)) == NULL) {
		a1loge(log, ICOM_SYS, "icompath: strdup failed!\n");
		return ICOM_SYS;
	}

	a1logd(g_log, 8, "icompath_set_usb '%s' got dctype 0x%x\n",p->name,p->dctype);

	p->dctype |= icomt_usb;
	p->dctype = (p->dctype & ~icomt_cat_mask) | dev_category(itype);

	p->nep = nep;
	p->vid = vid;
	p->pid = pid;
	p->usbd = usbd;
	p->dtype = itype;

	a1logd(g_log, 8, "icompath_set_usb '%s' returning dctype 0x%x\n",p->name,p->dctype);

	return ICOM_OK;
}

/* Add a usb path. usbd is taken, others are copied. */
/* return icom error */
static int icompaths_add_usb(icompaths *p, char *name, unsigned int vid, unsigned int pid,
                             int nep, struct usb_idevice *usbd, devType itype) {
	icom_dtix ix = dtix_combined;
	icompath *xp;
	int rv;

	if ((rv = icompaths_add_path(p, 0, NULL)) != ICOM_OK)
		return rv;

	xp = p->dpaths[ix][p->ndpaths[ix]-1];

	return icompath_set_usb(p->log, xp, name, vid, pid, nep, usbd, itype);
}

/* Add an hid path. hidd is taken, others are copied. */
/* return icom error */
static int icompaths_add_hid(icompaths *p, char *name, unsigned int vid, unsigned int pid,
                             int nep, struct hid_idevice *hidd, devType itype) {
	icom_dtix ix = dtix_combined;
	icompath *xp;
	int rv;

	if ((rv = icompaths_add_path(p, 0, NULL)) != ICOM_OK)
		return rv;
	
	xp = p->dpaths[ix][p->ndpaths[ix]-1];

	a1logd(g_log, 8, "icompaths_add_hid '%s' got dctype 0x%x\n",xp->name,xp->dctype);

	xp->dctype |= icomt_hid;
	xp->dctype = (xp->dctype & ~icomt_cat_mask) | dev_category(itype);
	
	if ((xp->name = strdup(name)) == NULL) {
		a1loge(p->log, ICOM_SYS, "icompaths: strdup failed!\n");
		return ICOM_SYS;
	}

	xp->nep = nep;
	xp->vid = vid;
	xp->pid = pid;
	xp->hidd = hidd;
	xp->dtype = itype;

	a1logd(g_log, 8, "icompath_set_usb '%s' returning dctype 0x%x\n",xp->name,xp->dctype);

	return ICOM_OK;
}
#endif /* ENABLE_USB */

static void icompaths_del(icompaths *p) {
	if (p != NULL) {
		icompaths_clear_all(p);
	
		if (p->exlist != NULL) {
			int i;
			for (i = 0; i < p->exno; i++) {
				if (p->exlist[i] != NULL)
					free(p->exlist[i]);
			}
			free(p->exlist);
		}
		p->log = del_a1log(p->log);		/* Unreference it and set to NULL */
		free(p);
	}
}

/* Create and return a list of available serial ports or USB devices for this system. */
/* Return icom error */
int icompaths_refresh_paths_sel(icompaths *p, icom_type mask) {
	int rv, usbend = 0;
	int i, j;

	a1logd(p->log, 7, "icoms_refresh_paths: called with mask = 0x%x\n",mask);

	/* Clear any existing device paths */
	p->clear(p);

#ifdef ENABLE_USB
	if (mask & icomt_hid) {
		a1logd(p->log, 6, "icoms_refresh_paths: looking for HID device\n");
		if ((rv = hid_get_paths(p)) != ICOM_OK)
			return rv;
	}
	if (mask & icomt_usb) {
		a1logd(p->log, 6, "icoms_refresh_paths: looking for USB device\n");
		if ((rv = usb_get_paths(p)) != ICOM_OK)
			return rv;
	}
	usbend = p->ndpaths[dtix_combined];
	a1logd(p->log, 6, "icoms_refresh_paths: now got %d paths\n",usbend);
#endif /* ENABLE_USB */

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)

	if (mask & (icomt_serial | icomt_fastserial | icomt_btserial)) {
		a1logd(p->log, 6, "icoms_refresh_paths: looking for serial ports\n");
		if ((rv = serial_get_paths(p, mask)) != ICOM_OK)
			return rv;

	}
#endif /* ENABLE_SERIAL || ENABLE_FAST_SERIAL */

	/* Sort the COM keys so people don't get confused... */
	/* Sort identified instruments ahead of unknown serial ports */
	a1logd(p->log, 6, "icoms_refresh_paths: we now have %d devices and are about to sort them\n",p->ndpaths[dtix_combined]);

	{
		icompath **pl = p->dpaths[dtix_combined];
		int np = p->ndpaths[dtix_combined];

		for (i = usbend; i < (np-1); i++) {
			for (j = i+1; j < np; j++) {
				if ((pl[i]->dtype == instUnknown && pl[j]->dtype != instUnknown)
				 || (((pl[i]->dtype == instUnknown && pl[j]->dtype == instUnknown)
				   || (pl[i]->dtype != instUnknown && pl[j]->dtype != instUnknown))
				  && strcmp(pl[i]->name, pl[j]->name) > 0)) {
					icompath *tt = pl[i];
					pl[i] = pl[j];
					pl[j] = tt;
				}
			}
		}
	}

	/* Create the device specific lists */
	if ((rv = icompaths_make_dslists(p)) != ICOM_OK) {
		a1logd(p->log, 1, "icoms_refresh_paths: icompaths_make_dslists failed with %d\n",rv);
		return rv;
	}

	a1logd(p->log, 8, "icoms_refresh_paths: returning %d paths and ICOM_OK\n",p->ndpaths[dtix_combined]);

	return ICOM_OK;
}

/* (In implementation specific) */
int serial_is_open(icoms *p);
void serial_close_port(icoms *p);

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
		if (serial_is_open(p)) {
			serial_close_port(p);
		}
#endif /* ENABLE_SERIAL */
		p->is_open = 0;
	}
}

int icompaths_refresh_paths(icompaths *p) {
	return icompaths_refresh_paths_sel(p, icomt_instrument | icomt_portattr_all);
}

static int create_fserexcl(icompaths *p) {
	char *envv;

	p->exno = 0;

	/* Get the exclusion list */
	if ((envv = getenv("ARGYLL_EXCLUDE_SERIAL_SCAN")) != NULL) {
		char *cp, *pcp;

		/* Count the number of strings */
		for (cp = envv;; cp++) {
			if (*cp == ';'
			 || *cp == ','
			 || *cp == '\000')
				p->exno++;
			if (*cp == '\000')
				break;
		}

		if ((p->exlist = (char **)calloc(p->exno, sizeof(char *))) == NULL) {
			a1logd(p->log, 1, "create_fserexcl: calloc failed!\n");
			return 1;
        }

		/* Copy strings to array */
		p->exno = 0;
		for (pcp = cp = envv;; cp++) {
			if (*cp == ';'
			 || *cp == ','
			 || *cp == '\000') {
				
				if (cp - pcp > 0) {
					if ((p->exlist[p->exno] = (char *)calloc(cp - pcp +1, sizeof(char))) == NULL) {
						a1logd(p->log, 1, "create_fserexcl: calloc failed!\n");
						return 1;
					}
					memmove(p->exlist[p->exno], pcp, cp - pcp);
					p->exlist[p->exno][cp - pcp] = '\000';
					p->exno++;
				}
				pcp = cp + 1;
			}
			if (*cp == '\000')
				break;
		}

#ifdef NEVER
		printf("Exclusion list len %d =\n",p->exno);
		for (i = 0; i < p->exno; i++) 
			printf(" '%s'\n",p->exlist[i]);
#endif
	}
	return 0;
}

/* Return nz if the given path is on the fast serial scan */
/* exclusion list */
static int icompaths_fs_excluded(icompaths *p, icompath *path) {
	
	a1logd(p->log, 5, "fs_excluded check '%s'\n",path->spath);

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	if (p->exlist != NULL) {
		int i;

		for (i = 0; i < p->exno; i++) {
			if (strcmp(p->exlist[i], path->spath) == 0) {
				a1logd(p->log, 5, "excluding '%s' from fast scan\n",path->spath);
				return 1;
			}
		}
	}
#endif
	return 0;
}

/* Allocate an icom paths and set it to the list of available devices */
/* that match the icom_type mask. */ 
/* Return NULL on error */
icompaths *new_icompaths_sel(a1log *log, icom_type mask) {
	icompaths *p;

	a1logd(log, 3, "new_icompath: called with mask 0x%x\n",mask);

	if ((p = (icompaths *)calloc(1, sizeof(icompaths))) == NULL) {
		a1loge(log, ICOM_SYS, "new_icompath: calloc failed!\n");
		return NULL;
	}

	p->log = new_a1log_d(log);

	p->clear         = icompaths_clear_all;
	p->refresh       = icompaths_refresh_paths;
	p->refresh_sel   = icompaths_refresh_paths_sel;
	p->get_path      = icompaths_get_path;
	p->get_path_sel  = icompaths_get_path_sel;
	p->del           = icompaths_del;

	/* ====== internal implementation ======= */

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	p->fs_excluded   = icompaths_fs_excluded;
	p->add_serial    = icompaths_add_serial;
#endif /* ENABLE_SERIAL */
#ifdef ENABLE_USB
	p->add_usb       = icompaths_add_usb;
	p->add_hid       = icompaths_add_hid;
#endif /* ENABLE_USB */
	p->del_last_path = icompaths_del_last_path;
	p->get_last_path = icompaths_get_last_path;
	/* ====================================== */

	/* Get list of fast scan exclusion devices */
	create_fserexcl(p);

	if (icompaths_refresh_paths_sel(p, mask)) {
		a1loge(log, ICOM_SYS, "new_icompaths: icompaths_refresh_paths failed!\n");
		free(p);
		return NULL;
	}

	return p;
}

/* Allocate an icom paths and set it to the list of available instruments */
/* Return NULL on error */
icompaths *new_icompaths(a1log *log) {
	return new_icompaths_sel(log, icomt_instrument | icomt_portattr_all);
}


/* ----------------------------------------------------- */

/* Copy an icompath into an icom */
/* return icom error */
static int icom_copy_path_to_icom(icoms *p, icompath *pp) {
	int rv;

	if (p->name != NULL)
		free(p->name);
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
	p->dctype = pp->dctype;
	p->dtype = pp->dtype;

	a1logd(g_log, 8, "icom_copy_path_to_icom '%s' returning dctype 0x%x\n",p->name,p->dctype);

	return ICOM_OK;
}

/* Return the device category */
/* (Returns bit flags) */
static icom_type icoms_cat_type(
icoms *p
) {
	return p->dctype & icomt_cat_mask;
}

/* Return the communication port type */
/* (Can use equality tests on return value) */
static icom_type icoms_port_type(
icoms *p
) {
	return p->dctype & icomt_port_mask;
}

/* Return the communication port attributes */
/* (Returns bit flags) */
static icom_type icoms_port_attr(
icoms *p
) {
	return p->dctype & icomt_attr_mask;
}

/* ----------------------------------------------------- */

/* Include the icoms implementation dependent function implementations */
#ifdef NT
# include "icoms_nt.c"
#endif
#if defined(UNIX)
# include "icoms_ux.c"
#endif

/* write and read convenience function with read flush */
/* return icom error */
static int
icoms_write_read_ex(
icoms *p,
char *wbuf,			/* Write puffer */
int nwch,			/* if > 0, number of characters to write, else nul terminated */
char *rbuf,			/* Read buffer */
int bsize,			/* Buffer size */
int *bread,			/* Bytes read (not including forced '\000') */
char *tc,			/* Terminating characers, NULL for none or char count mode */
int ntc,			/* Number of terminating characters needed, or char count needed */
double tout,		/* Timeout for write and then read (i.e. max = 2 x tout) */
int frbw			/* nz to Flush Read Before Write */
) {
	int rv = ICOM_OK;

	a1logd(p->log, 8, "icoms_write_read: called\n");

	if (p->write == NULL || p->read == NULL) {	/* Neither serial nor USB ? */
		a1loge(p->log, ICOM_NOTS, "icoms_write_read: No write and read functions set!\n");
		return ICOM_NOTS;
	}

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	/* Flush any stray chars if serial */
	if (frbw && (p->dctype & icomt_serial)) {
		char tbuf[500];
		int debug = p->log->debug;
		int bread;

		p->ser_clearerr(p);

		if (debug < 8)
			p->log->debug =  0;
		/* (Could use tcflush() or ioctl(TCFLSH) on *nix,  */
		/*  except these don't work on USB serial ports!) */
		for (;;) {
			bread = 0;
			p->read(p, tbuf, 500, &bread, NULL, 500, 0.02);
			if (bread == 0)
				break;
		}
		p->log->debug = debug;
		rv = ICOM_OK;
	}
#endif

	/* Write the write data */
	rv = p->write(p, wbuf, nwch, tout);

	/* Return error if coms */
	if (rv != ICOM_OK) {
		if (rv & ICOM_TO) 
			a1logd(p->log, 8, "icoms_write_read: write T.O. %f sec. returning 0x%x\n",tout, rv);
		else
			a1logd(p->log, 8, "icoms_write_read: write failed - returning 0x%x\n",rv);
		return rv;
	}

	/* Read response */
	rv = p->read(p, rbuf, bsize, bread, tc, ntc, tout);
	
	if (rv & ICOM_TO) 
		a1logd(p->log, 8, "icoms_write_read: read T.O. %f sec. returning 0x%x\n",tout, rv);
	else
		a1logd(p->log, 8, "icoms_write_read: read returning 0x%x\n",rv);

	return rv;
}

/* write and read convenience function */
/* return icom error */
static int
icoms_write_read(
icoms *p,
char *wbuf,			/* Write puffer */
int nwch,			/* if > 0, number of characters to write, else nul terminated */
char *rbuf,			/* Read buffer */
int bsize,			/* Buffer size */
int *bread,			/* Bytes read (not including forced '\000') */
char *tc,			/* Terminating characers, NULL for none or char count mode */
int ntc,			/* Number of terminating characters needed, or char count needed */
double tout		/* Timeout for write and then read (i.e. max = 2 x tout) */
) {
	return icoms_write_read_ex(p, wbuf, nwch, rbuf, bsize, bread, tc, ntc, tout, 0);
}

/* Default NOP implementation - Serial open or set_methods may override */ 
static void icoms_ser_clearerr(icoms *p) {
	return;
}

/* Optional callback to client from device */ 
/* Default implementation is a NOOP */
static int icoms_interrupt(icoms *p,
	int icom_int		/* Interrupt cause */
) {
	return ICOM_OK;
}

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

/* icoms Constructor */
/* Return NULL on error */
icoms *new_icoms(
	icompath *ipath,	/* instrument to open */
	a1log *log			/* log to take reference from, NULL for default */
) {
	icoms *p;

	a1logd(log, 2, "new_icoms '%s' itype '%s' dctype 0x%x\n",ipath->name,inst_sname(ipath->dtype),ipath->dctype);

	if ((p = (icoms *)calloc(1, sizeof(icoms))) == NULL) {
		a1loge(log, ICOM_SYS, "new_icoms: calloc failed!\n");
		return NULL;
	}

	if ((p->name = strdup(ipath->name)) == NULL) {
		a1loge(log, ICOM_SYS, "new_icoms: strdup failed!\n");
		return NULL;
	}
	p->dtype = ipath->dtype;

	/* Copy ipath info to this icoms */
	if (icom_copy_path_to_icom(p, ipath)) {
		free(p->name);
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

	p->dev_cat   = icoms_cat_type;
	p->port_type = icoms_port_type;
	p->port_attr = icoms_port_attr;

#if defined(ENABLE_SERIAL) || defined(ENABLE_FAST_SERIAL)
	p->set_ser_port_ex = icoms_set_ser_port_ex;
	p->set_ser_port = icoms_set_ser_port;
#endif	/* ENABLE_SERIAL */

	p->write = NULL;	/* Serial open or set_methods will set */
	p->read = NULL;
	p->write_read = icoms_write_read;
	p->write_read_ex = icoms_write_read_ex;
	p->ser_clearerr = icoms_ser_clearerr;		/* Default NOP implementation */
	p->interrupt = icoms_interrupt;

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
	/* 0 msec delay, 800Hz for 200 msec */
	msec_beep(0, 800, 200);
	/* 350 msec delay, 800Hz for 200 msec */
	msec_beep(350, 800, 200);
}

char *baud_rate_to_str(baud_rate br) {
	switch (br) {
		case baud_nc:
			return "Not Configured";
		case baud_110:
			return "110";
		case baud_300:
			return "300";
		case baud_600:
			return "600";
		case baud_1200:
			return "1400";
		case baud_2400:
			return "2400";
		case baud_4800:
			return "4800";
		case baud_9600:
			return "9600";
		case baud_14400:
			return "14400";
		case baud_19200:
			return "19200";
		case baud_38400:
			return "38400";
		case baud_57600:
			return "57600";
		case baud_115200:
			return "115200";
		case baud_230400:
			return "230400";
		case baud_921600:
			return "921600";
	}
	return "Unknown";
}

/* Convert control chars to ^[A-Z] notation in a string */
/* Can be called 3 times without reusing the static buffer */
/* Returns a maximum of 5000 characters */
char *
icoms_fix(char *ss) {
	static unsigned char buf[3][10005];
	static int ix = 0;
	unsigned char *d;
	unsigned char *s = (unsigned char *)ss;
	if (++ix >= 3)
		ix = 0;
	if (ss == NULL) {
		strcpy((char *)buf[ix],"(null)");
		return (char *)buf[ix];
	}
	for(d = buf[ix]; (d - buf[ix]) < 10000;) {
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

