
/* 
 * Argyll Color Correction System
 * Calibration curve class.
 *
 * Author: Graeme W. Gill
 * Date:   30/10/2005
 *
 * Copyright 2005 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * This class allows reading and using a calibration file.
 * Creation is currently left up to specialized programs (dispcal, printcal).
 * This class doesn't handle the extra table that dispcal creates/uses.
 *
 */

#undef DEBUG		/* Input points */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "xicc.h"

#ifdef NT       /* You'd think there might be some standards.... */
# ifndef __BORLANDC__
#  define stricmp _stricmp
# endif
#else
# define stricmp strcasecmp
#endif

/* rspl setting functions */
static void xcal_rsplset(void *cbntx, double *out, double *in) {
	co *dpoints = (co *)cbntx;
	int ix;

	ix = *((int*)&in[-0-1]);	/* Get grid index being looked up */
	out[0] = dpoints[ix].v[0];
}

/* Read a calibration file from a cgats table */
/* Return nz if this fails */
static int xcal_read_cgats(xcal *p, cgats *tcg, int table, char *filename) {
	int oi;
	int i, j, ti;
	char *ident;
	char *bident;
	int spi[1+MAX_CHAN];	/* CGATS indexes for each field */
	char buf[100];

	if ((oi = tcg->get_oi(tcg, "CAL")) < 0) {
		sprintf(p->err, "Input file '%s' can't be a CAL format file", filename);
		return p->errc = 1;
	}

	if (tcg->t[table].tt != tt_other || tcg->t[table].oi != oi) {
		sprintf(p->err, "Input file '%s' isn't a CAL format file", filename);
		return p->errc = 1;
	}

	/* See what sort of device type this calibration is for */
	if ((ti = tcg->find_kword(tcg, table, "DEVICE_CLASS")) < 0) {
		sprintf(p->err, "Calibration file '%s'doesn't contain keyword DEVICE_CLASS",filename);
		return p->errc = 1;
	}
	if (strcmp(tcg->t[table].kdata[ti],"INPUT") == 0) {
		p->devclass = icSigInputClass;
	} else if (strcmp(tcg->t[table].kdata[ti],"OUTPUT") == 0) {
		p->devclass = icSigOutputClass;
	} else if (strcmp(tcg->t[table].kdata[ti],"DISPLAY") == 0) {
		p->devclass = icSigDisplayClass;
	} else {
		sprintf(p->err,"Calibration file '%s' contain unknown DEVICE_CLASS '%s'",
		                                            filename,tcg->t[table].kdata[ti]);
		return p->errc = 1;
	}

	if ((ti = tcg->find_kword(tcg, table, "COLOR_REP")) < 0) {
		/* Be backwards compatible with V1.0.4 display calibration files */
		if (p->devclass != icSigDisplayClass) {
			sprintf(p->err, "Calibration file '%s'doesn't contain keyword COLOR_REP",filename);
			return p->errc = 1;
		} 
		warning("\n    *** Calibration file '%s'doesn't contain keyword COLOR_REP, assuming RGB ***",filename);
		if ((p->devmask = icx_char2inkmask("RGB") ) == 0) {
			sprintf(p->err, "Calibration file '%s' has unrecognized COLOR_REP '%s'",
			                                          filename,tcg->t[table].kdata[ti]);
			return p->errc = 1;
		}
	} else {
		if ((p->devmask = icx_char2inkmask(tcg->t[table].kdata[ti]) ) == 0) {
			sprintf(p->err, "Calibration file '%s' has unrecognized COLOR_REP '%s'",
			                                          filename,tcg->t[table].kdata[ti]);
			return p->errc = 1;
		}
	}

	if ((ti = tcg->find_kword(tcg, table, "VIDEO_LUT_CALIBRATION_POSSIBLE")) >= 0) {
		if (stricmp(tcg->t[table].kdata[ti], "NO") == 0)
		p->noramdac = 1;
	}

	if ((ti = tcg->find_kword(tcg, table, "TV_OUTPUT_ENCODING")) >= 0) {
		if (strcmp(tcg->t[0].kdata[ti], "YES") == 0
		 || strcmp(tcg->t[0].kdata[ti], "yes") == 0)
			p->tvenc = 1;
	}

	p->colspace = icx_colorant_comb_to_icc(p->devmask);	/* 0 if none */
	p->devchan = icx_noofinks(p->devmask);
	ident = icx_inkmask2char(p->devmask, 1); 
	bident = icx_inkmask2char(p->devmask, 0); 

	/* Grab any descriptive information */
	if ((ti = tcg->find_kword(tcg, table, "MANUFACTURER")) >= 0)
		p->xpi.deviceMfgDesc = strdup(tcg->t[table].kdata[ti]);
	if ((ti = tcg->find_kword(tcg, table, "MODEL")) >= 0)
		p->xpi.modelDesc = strdup(tcg->t[table].kdata[ti]);
	if ((ti = tcg->find_kword(tcg, table, "DESCRIPTION")) >= 0)
		p->xpi.profDesc = strdup(tcg->t[table].kdata[ti]);
	if ((ti = tcg->find_kword(tcg, table, "COPYRIGHT")) >= 0)
		p->xpi.copyright = strdup(tcg->t[table].kdata[ti]);

	if (tcg->t[table].nsets <= 0) {
		sprintf(p->err, "Calibration file '%s' has too few entries %d",
		                                          filename,tcg->t[table].nsets);
		return p->errc = 1;
	}

	/* Figure out the indexes of all the fields */
	sprintf(buf, "%s_I",bident);
	if ((spi[0] = tcg->find_field(tcg, table, buf)) < 0) {
		sprintf(p->err,"Calibration file '%s' doesn't contain field '%s'", filename,buf);
		return p->errc = 1;
	}

	for (j = 0; j < p->devchan; j++) {
		inkmask imask = icx_index2ink(p->devmask, j);
		sprintf(buf, "%s_%s",bident,icx_ink2char(imask));
		if ((spi[1+j] = tcg->find_field(tcg, table, buf)) < 0) {
			sprintf(p->err,"Calibration file '%s' doesn't contain field '%s'", filename,buf);
			return p->errc = 1;
		}
	}

	/* Read in each channels values and put them in a rspl */
	for (j = 0; j < p->devchan; j++) {
		datai low,high;
		int gres[MXDI];
		double smooth = 1.0;
		co *dpoints;

		low[0] = 0.0;
		high[0] = 1.0;
		gres[0] = tcg->t[table].nsets;

		if ((p->cals[j] = new_rspl(RSPL_NOFLAGS,1, 1)) == NULL) {
			sprintf(p->err,"new_rspl() failed");
			return p->errc = 2;
		}

		if ((dpoints = malloc(sizeof(co) * gres[0])) == NULL) {
			sprintf(p->err,"malloc dpoints[%d] failed",gres[0]);
			return p->errc = 2;
		}

		/* Copy the points to our array */
		for (i = 0; i < gres[0]; i++) {
			dpoints[i].p[0] = i/(double)(gres[0]-1);
			dpoints[i].v[0] = *((double *)tcg->t[table].fdata[i][spi[1+j]]);
		}

		/* Set the rspl */
		p->cals[j]->set_rspl(p->cals[j],
				   0, 
				   (void *)dpoints,		/* Read points */
				   xcal_rsplset,		/* Setting function */
				   low, high, gres,		/* Low, high, resolution of grid */
				   NULL, NULL			/* Default data scale */
				   );
		free(dpoints);
	}
	free(ident);
	free(bident);

	return 0;
}

/* Read a calibration file from an ICC vcgt tag */
/* Return nz if this fails */
int xcal_read_icc(xcal *p, icc *c) {
	icmVideoCardGamma *vg;
	icmTextDescription *td;
	int res, i, j;

	/* See if there is a vcgt tag */
	if ((vg = (icmVideoCardGamma *)c->read_tag(c, icSigVideoCardGammaTag)) == NULL) {
		sprintf(p->err, "ICC profile has no vcgt");
		return p->errc = 1;
	}

	/* What sort of device the profile is for */
	p->devclass = c->header->deviceClass;
	p->colspace = c->header->colorSpace;

	if ((p->devmask = icx_icc_to_colorant_comb(p->colspace, p->devclass)) == 0) {
		sprintf(p->err, "Unable to determine inkmask from ICC profile");
		return p->errc = 1;
	} 
	p->devchan = icx_noofinks(p->devmask);

	/* Grab any descriptive information */
	if ((td = (icmTextDescription *)c->read_tag(c, icSigDeviceMfgDescTag)) != NULL) {
		p->xpi.deviceMfgDesc = strdup(td->desc);
	} 
	if ((td = (icmTextDescription *)c->read_tag(c, icSigDeviceModelDescTag)) != NULL) {
		p->xpi.modelDesc = strdup(td->desc);
	} 
	if ((td = (icmTextDescription *)c->read_tag(c, icSigProfileDescriptionTag)) != NULL) {
		p->xpi.profDesc = strdup(td->desc);
	} 
	if ((td = (icmTextDescription *)c->read_tag(c, icSigCopyrightTag)) != NULL) {
		p->xpi.copyright = strdup(td->desc);
	} 

	/* Decide the lut resolution */
	if (vg->tagType == icmVideoCardGammaFormulaType)
		res = 2048;
	else
		res = vg->u.table.entryCount;

	/* Read in each channels values and put them in a rspl */
	for (j = 0; j < p->devchan; j++) {
		datai low,high;
		int gres[MXDI];
		double smooth = 1.0;
		co *dpoints;

		low[0] = 0.0;
		high[0] = 1.0;
		gres[0] = res;

		if ((p->cals[j] = new_rspl(RSPL_NOFLAGS,1, 1)) == NULL) {
			sprintf(p->err,"new_rspl() failed");
			return p->errc = 2;
		}

		if ((dpoints = malloc(sizeof(co) * gres[0])) == NULL) {
			sprintf(p->err,"malloc dpoints[%d] failed",gres[0]);
			return p->errc = 2;
		}

		/* Copy the points to our array */
		for (i = 0; i < gres[0]; i++) {
			dpoints[i].p[0] = i/(double)(gres[0]-1);
			dpoints[i].v[0] = vg->lookup(vg, j, dpoints[i].p[0]);
		}

		/* Set the rspl */
		p->cals[j]->set_rspl(p->cals[j],
				   0, 
				   (void *)dpoints,		/* Read points */
				   xcal_rsplset,		/* Setting function */
				   low, high, gres,		/* Low, high, resolution of grid */
				   NULL, NULL			/* Default data scale */
				   );
		free(dpoints);
	}

	return 0;
}

/* Read a calibration file */
/* Return nz if this fails */
static int xcal_read(xcal *p, char *filename) {
	cgats *tcg;					/* .cal file */
	int table = 0;
	int rv;

	if ((tcg = new_cgats()) == NULL) {
		sprintf(p->err, "new_cgats() failed");
		return p->errc = 2;
	}

	tcg->add_other(tcg, "CAL"); 	/* our special input type is Calibration Target */

	if (tcg->read_name(tcg, filename)) {
		strcpy(p->err, tcg->err);
		p->errc = tcg->errc;
		tcg->del(tcg);
		return p->errc;
	}

	if (tcg->ntables < 1)
		return 1;

	rv = xcal_read_cgats(p, tcg, table, filename);

	tcg->del(tcg);

	return rv;
}

/* Write a calibration to a new cgats table */
/* Return nz if this fails */
static int xcal_write_cgats(xcal *p, cgats *tcg) {
	int oi;
	int table;
	int i, j, ti;
	char *ident, *bident;
	time_t clk = time(0);
	struct tm *tsp = localtime(&clk);
	char *atm = asctime(tsp);   /* Ascii time */
	char buf[100];
	cgats_set_elem *setel;		/* Array of set value elements */
	int nsetel = 0;
	int calres;

	oi = tcg->add_other(tcg, "CAL"); 	/* our special type is Calibration Target */

	table = tcg->add_table(tcg, tt_other, oi);	/* Add a table for calibration */

	tcg->add_kword(tcg, table, "DESCRIPTOR", "Argyll Device Calibration Curves",NULL);
	tcg->add_kword(tcg, table, "ORIGINATOR", "Argyll", NULL);
	atm[strlen(atm)-1] = '\000';	/* Remove \n from end */
	tcg->add_kword(tcg, table, "CREATED",atm, NULL);

	if (p->devclass == icSigInputClass)
		tcg->add_kword(tcg, table, "DEVICE_CLASS","INPUT", NULL);
	else if (p->devclass == icSigOutputClass)
		tcg->add_kword(tcg, table, "DEVICE_CLASS","OUTPUT", NULL);
	else if (p->devclass == icSigDisplayClass)
		tcg->add_kword(tcg, table, "DEVICE_CLASS","DISPLAY", NULL);
	else {
		sprintf(p->err,"Unknown device class '%s'",icm2str(icmProfileClassSignature,p->devclass));
		return p->errc = 1;
	}

	/* Colorspace */
	ident = icx_inkmask2char(p->devmask, 1); 
	bident = icx_inkmask2char(p->devmask, 0); 
	tcg->add_kword(tcg, table, "COLOR_REP", ident, NULL);

	/* Other tags */
	if (p->noramdac)
		tcg->add_kword(tcg, table, "VIDEO_LUT_CALIBRATION_POSSIBLE", "NO", NULL);

	if (p->tvenc) 
		tcg->add_kword(tcg, table, "TV_OUTPUT_ENCODING", "YES", NULL);

	/* Grab any descriptive information */
	if (p->xpi.deviceMfgDesc != NULL)
		tcg->add_kword(tcg, table, "MANUFACTURER",p->xpi.deviceMfgDesc, NULL);
	if (p->xpi.modelDesc != NULL)
		tcg->add_kword(tcg, table, "MODEL",p->xpi.modelDesc, NULL);
	if (p->xpi.profDesc != NULL)
		tcg->add_kword(tcg, table, "DESCRIPTION",p->xpi.profDesc, NULL);
	if (p->xpi.copyright != NULL)
		tcg->add_kword(tcg, table, "COPYRIGHT",p->xpi.copyright, NULL);

	sprintf(buf, "%s_I",bident);
	tcg->add_field(tcg, table, buf, r_t);
	nsetel++;
	for (j = 0; j < p->devchan; j++) {
		inkmask imask = icx_index2ink(p->devmask, j);
		sprintf(buf, "%s_%s",bident,icx_ink2char(imask));
		tcg->add_field(tcg, table, buf, r_t);
		nsetel++;
	}
	if ((setel = (cgats_set_elem *)malloc(sizeof(cgats_set_elem) * nsetel)) == NULL) {
		sprintf(p->err,"Malloc failed");
		return p->errc = 2;
	}

	calres = p->cals[0]->g.res[0];

	for (i = 0; i < calres; i++) {
		double vv = i/(calres-1.0);
		co tp;

		setel[0].d = vv;
		for (j = 0; j < p->devchan; j++) {
			tp.p[0] = vv;
			p->cals[j]->interp(p->cals[j], &tp);
			setel[j+1].d = tp.v[0];
		}

		tcg->add_setarr(tcg, table, setel);
	}

	free(setel);
	free(ident);
	free(bident);

	return 0;
}

/* Write a calibration file */
/* Return nz if this fails */
static int xcal_write(xcal *p, char *filename) {
	cgats *tcg;					/* .cal file */
	int table = 0;
	int rv;

	if ((tcg = new_cgats()) == NULL) {
		sprintf(p->err, "new_cgats() failed");
		return p->errc = 2;
	}

	if ((rv = xcal_write_cgats(p, tcg)) != 0) {
		strcpy(p->err, tcg->err);
		p->errc = tcg->errc;
		tcg->del(tcg);
		return p->errc;
	}

	if (tcg->write_name(tcg, filename)) {
		strcpy(p->err, tcg->err);
		p->errc = tcg->errc;
		tcg->del(tcg);
		return p->errc;
	}

	tcg->del(tcg);

	return rv;
}

/* Translate values through the curves. */
static void xcal_interp(xcal *p, double *out, double *in) {
	int j;
	co tp;

	for (j = 0; j < p->devchan; j++) {
		tp.p[0] = in[j];
		p->cals[j]->interp(p->cals[j], &tp);
		out[j] = tp.v[0];
	}
}

#define MAX_INVSOLN 10  /* Rspl maximum reverse solutions */

/* Translate a value backwards through the curves. */
/* Return nz if the inversion fails */ 
static int xcal_inv_interp(xcal *p, double *out, double *in) {
	int nsoln;				/* Number of solutions found */
	co pp[MAX_INVSOLN];		/* Room for all the solutions found */
	int j, k;					/* Chosen solution */
	double dir = 0.5;		/* target if multiple solutions */
	int rv = 0;

	for (j = 0; j < p->devchan; j++) {
		pp[0].v[0] = in[j];

		nsoln = p->cals[j]->rev_interp (
			p->cals[j],		 	/* this */
			RSPL_NEARCLIP,		/* Clip to nearest (faster than vector) */
			MAX_INVSOLN,		/* Maximum number of solutions allowed for */
			NULL, 				/* No auxiliary input targets */
			NULL,				/* Clip vector direction and length */
			pp);				/* Input and output values */

		nsoln &= RSPL_NOSOLNS;		/* Get number of solutions */

		if (nsoln == 1) { /* Exactly one solution */
			k = 0;
		} else if (nsoln == 0) {	/* Zero solutions. This is unexpected. */
			rv = 1;
			return -1.0;
		} else {		/* Multiple solutions */
			double bdist = 1e300;
			int bsoln = 0;
			for (k = 0; k < nsoln; k++) {
				double tt;
				tt = pp[k].p[0] - dir;
				tt *= tt;
				if (tt < bdist) {	/* Better solution */
					bdist = tt;
					bsoln = k;
				}
			}
			k = bsoln;
		}
		out[j] = pp[k].p[0];
	}

	return rv;
}

/* Translate a value through one of the curves */
static double xcal_interp_ch(xcal *p, int ch, double in) {
	co tp;

	if (ch < 0 || ch >= p->devchan)
		return -1.0;

	tp.p[0] = in;
	p->cals[ch]->interp(p->cals[ch], &tp);
	return tp.v[0];
}

/* Translate a value backwards through one of the curves */
/* Return -1.0 if the inversion fails */
static double xcal_inv_interp_ch(xcal *p, int ch, double in) {
	int nsoln;				/* Number of solutions found */
	co pp[MAX_INVSOLN];		/* Room for all the solutions found */
	int k;					/* Chosen solution */
	double dir = 0.5;		/* target if multiple solutions */

	if (ch < 0 || ch >= p->devchan)
		return -1.0;

	pp[0].v[0] = in;

	nsoln = p->cals[ch]->rev_interp (
		p->cals[ch],	 	/* this */
		RSPL_NEARCLIP,		/* Clip to nearest (faster than vector) */
		MAX_INVSOLN,		/* Maximum number of solutions allowed for */
		NULL, 				/* No auxiliary input targets */
		NULL,				/* Clip vector direction and length */
		pp);				/* Input and output values */

	nsoln &= RSPL_NOSOLNS;		/* Get number of solutions */

	if (nsoln == 1) { /* Exactly one solution */
		k = 0;
	} else if (nsoln == 0) {	/* Zero solutions. This is unexpected. */
		return -1.0;
	} else {		/* Multiple solutions */
		double bdist = 1e300;
		int bsoln = 0;
		for (k = 0; k < nsoln; k++) {
			double tt;
			tt = pp[k].p[0] - dir;
			tt *= tt;
			if (tt < bdist) {	/* Better solution */
				bdist = tt;
				bsoln = k;
			}
		}
		k = bsoln;
	}
	return pp[k].p[0];
}

/* Delete an xcal */
static void xcal_del(xcal *p) {
	int j;

	if (p->xpi.deviceMfgDesc != NULL)
		free(p->xpi.deviceMfgDesc);
	if (p->xpi.modelDesc != NULL)
		free(p->xpi.modelDesc);
	if (p->xpi.profDesc != NULL)
		free(p->xpi.profDesc);
	if (p->xpi.copyright != NULL)
		free(p->xpi.copyright);

	for (j = 0; j < p->devchan; j++) {
		if (p->cals[j] != NULL)
			p->cals[j]->del(p->cals[j]);
	}
	free(p);
}

/* Create a new, uninitialised xcal */
xcal *new_xcal(void) {
	xcal *p;

	if ((p = (xcal *)calloc(1, sizeof(xcal))) == NULL)
		return NULL;

	/* Init method pointers */
	p->del            = xcal_del;
	p->read_cgats     = xcal_read_cgats;
	p->read_icc       = xcal_read_icc;
	p->read           = xcal_read;
	p->write_cgats    = xcal_write_cgats;
	p->write          = xcal_write;
	p->interp         = xcal_interp;
	p->inv_interp     = xcal_inv_interp;
	p->interp_ch      = xcal_interp_ch;
	p->inv_interp_ch  = xcal_inv_interp_ch;

	return p;
}

