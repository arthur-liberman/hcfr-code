
/* 
 * Argyll Color Correction System
 * Colorimeter Correction Matrix
 *
 * Author: Graeme W. Gill
 * Date:   9/8/2010
 *
 * Copyright 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/*
 * TTBD:
 */

#undef DEBUG

#define verbo stdout

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#ifndef SALONEINSTLIB
#include "numlib.h"
#include "icc.h"
#else
#include "numsup.h"
#include "conv.h"
#endif
#include "cgats.h"
#include "xspect.h"
#include "disptechs.h"
#include "ccss.h"

#ifdef NT       /* You'd think there might be some standards.... */
# ifndef __BORLANDC__
#  define stricmp _stricmp
# endif
#else
# define stricmp strcasecmp
#endif

/* Forward declarations */
static void free_ccss(ccss *p);

/* Utilities */

/* Method implimentations */

/* Write out the ccss to a CGATS format object */
/* Return nz on error */
static int create_ccss_cgats(
ccss *p,			/* This */
cgats **pocg		/* return CGATS structure */
) {
	int i, j;
	time_t clk = time(0);
	struct tm *tsp = localtime(&clk);
	char *atm = asctime(tsp); /* Ascii time */
	cgats *ocg;				/* CGATS structure */
	int nsetel = 0;
	cgats_set_elem *setel;	/* Array of set value elements */
	char buf[100];

	atm[strlen(atm)-1] = '\000';	/* Remove \n from end */

	/* Setup output cgats file */
	ocg = new_cgats();	/* Create a CGATS structure */
	ocg->add_other(ocg, "CCSS"); 		/* Type is Colorimeter Calibration Spectral Set */
	ocg->add_table(ocg, tt_other, 0);	/* Start the first table */

	if (p->desc != NULL)
		ocg->add_kword(ocg, 0, "DESCRIPTOR", p->desc,NULL);

	if (p->orig != NULL)
		ocg->add_kword(ocg, 0, "ORIGINATOR", p->orig, NULL);
	else
		ocg->add_kword(ocg, 0, "ORIGINATOR", "Argyll ccss", NULL);
	if (p->crdate != NULL)
		ocg->add_kword(ocg, 0, "CREATED",p->crdate, NULL);
	else
		ocg->add_kword(ocg, 0, "CREATED",atm, NULL);

	if (p->disp)
		ocg->add_kword(ocg, 0, "DISPLAY",p->disp, NULL);

	ocg->add_kword(ocg, 0, "TECHNOLOGY", disptech_get_id(p->dtech)->strid,NULL);

	if (p->disp == NULL && p->tech == NULL) {
		sprintf(p->err, "write_ccss: ccss doesn't contain display or techology strings");
		ocg->del(ocg);
		return 1;
	}
	if (p->refrmode >= 0)
		ocg->add_kword(ocg, 0, "DISPLAY_TYPE_REFRESH", p->refrmode ? "YES" : "NO", NULL);
	if (p->sel != NULL)
		ocg->add_kword(ocg, 0, "UI_SELECTORS", p->sel, NULL);
	if (p->ref != NULL)
		ocg->add_kword(ocg, 0, "REFERENCE",p->ref, NULL);

	sprintf(buf,"%d", p->samples[0].spec_n);
	ocg->add_kword(ocg, 0, "SPECTRAL_BANDS",buf, NULL);
	sprintf(buf,"%f", p->samples[0].spec_wl_short);
	ocg->add_kword(ocg, 0, "SPECTRAL_START_NM",buf, NULL);
	sprintf(buf,"%f", p->samples[0].spec_wl_long);
	ocg->add_kword(ocg, 0, "SPECTRAL_END_NM",buf, NULL);
	sprintf(buf,"%f", p->samples[0].norm);
	ocg->add_kword(ocg, 0, "SPECTRAL_NORM",buf, NULL);

	/* Fields we want */
	if (ocg->add_field(ocg, 0, "SAMPLE_ID", nqcs_t) < 0) {
		sprintf(p->err, "cgats add_field SAMPLE_ID failed with '%s'!",ocg->err);
		ocg->del(ocg);		/* Clean up */
		return 2;
	}
	nsetel += 1;		/* For id */

	/* Generate fields for spectral values */
	for (i = 0; i < p->samples[0].spec_n; i++) {
		int nm;

		/* Compute nearest integer wavelength */
		nm = (int)(p->samples[0].spec_wl_short + ((double)i/(p->samples[0].spec_n-1.0))
		            * (p->samples[0].spec_wl_long - p->samples[0].spec_wl_short) + 0.5);
		
		sprintf(buf,"SPEC_%03d",nm);
		if (ocg->add_field(ocg, 0, buf, r_t) < 0) {
			sprintf(p->err, "cgats add_field %s failed with '%s'",buf,ocg->err);
			ocg->del(ocg);		/* Clean up */
			return 2;
		}
	}
	nsetel += p->samples[0].spec_n;		/* Spectral values */

	if ((setel = (cgats_set_elem *)malloc(sizeof(cgats_set_elem) * nsetel)) == NULL) {
		strcpy(p->err, "Malloc failed!");
		ocg->del(ocg);		/* Clean up */
		return 2;
	}

	/* Write out the patch info to the output CGATS file */
	for (i = 0; i < p->no_samp; i++) {
		int k = 0;

		sprintf(buf,"%d",i+1);
		setel[k++].c = buf;

		for (j = 0; j < p->samples[i].spec_n; j++)
			setel[k++].d = p->samples[i].spec[j];

		ocg->add_setarr(ocg, 0, setel);
	}
	free(setel);

	if (pocg != NULL)
		*pocg = ocg;

	return 0;
}

/* Write out the ccss to a CGATS format .ccss file */
/* Return nz on error */
static int write_ccss(
ccss *p,		/* This */
char *outname	/* Filename to write to */
) {
	int rv;
	cgats *ocg;				/* CGATS structure */

	if (p->no_samp < 3) {
		strcpy(p->err, "Need at least three spectral samples");
		return 1;
	}

	/* Create CGATS elements */
	if ((rv = create_ccss_cgats(p, &ocg)) != 0) {
		return rv;
	}

	/* Write it to file */
	if (ocg->write_name(ocg, outname)) {
		strcpy(p->err, ocg->err);
		ocg->del(ocg);		/* Clean up */
		return 1;
	}
	ocg->del(ocg);		/* Clean up */

	return 0;
}

/* write to a CGATS .ccss file to a memory buffer. */
/* return nz on error, with message in err[] */
static int buf_write_ccss(
ccss *p,
unsigned char **buf,		/* Return allocated buffer */
int *len					/* Return length */
) {
	int rv;
	cgats *ocg;				/* CGATS structure */
	cgatsFile *fp;

	if (p->no_samp < 3) {
		strcpy(p->err, "Need at least three spectral samples");
		return 1;
	}

	/* Create CGATS elements */
	if ((rv = create_ccss_cgats(p, &ocg)) != 0) {
		return rv;
	}

	if ((fp = new_cgatsFileMem(NULL, 0)) == NULL) {
		strcpy(p->err, "new_cgatsFileMem failed");
		return 2;
	}

	/* Write it to file */
	if (ocg->write(ocg, fp)) {
		strcpy(p->err, ocg->err);
		ocg->del(ocg);		/* Clean up */
		fp->del(fp);
		return 1;
	}

	/* Get the buffer the ccss has been written to */
	if (fp->get_buf(fp, buf, (size_t *)len)) {
		strcpy(p->err, "cgatsFileMem get_buf failed");
		return 2;
	}

	ocg->del(ocg);		/* Clean up */
	fp->del(fp);

	return 0;
}

/* Read in the ccss CGATS .ccss file from cgats */
/* Return nz on error */
static int read_ccss_cgats(
ccss *p,		/* This */
cgats *icg		/* input cgats structure */
) {
	int i, j;
	int ti, ii;			/* Temporary CGATs index */
	int  spi[XSPECT_MAX_BANDS];	/* CGATS indexes for each wavelength */
	xspect sp;

	if (icg->ntables == 0 || icg->t[0].tt != tt_other || icg->t[0].oi != 0) {
		sprintf(p->err, "read_ccss: Input file isn't a CCSS format file");
		return 1;
	}
	if (icg->ntables != 1) {
		sprintf(p->err, "Input file doesn't contain exactly one table");
		return 1;
	}

	free_ccss(p);

	if ((ti = icg->find_kword(icg, 0, "DESCRIPTOR")) >= 0) {
		if ((p->desc = strdup(icg->t[0].kdata[ti])) == NULL) {
			sprintf(p->err, "read_ccss: malloc failed");
			return 2;
		}
	}
	if ((ti = icg->find_kword(icg, 0, "ORIGINATOR")) >= 0) {
		if ((p->orig = strdup(icg->t[0].kdata[ti])) == NULL) {
			sprintf(p->err, "read_ccss: malloc failed");
			return 2;
		}
	}
	if ((ti = icg->find_kword(icg, 0, "CREATED")) >= 0) {
		if ((p->crdate = strdup(icg->t[0].kdata[ti])) == NULL) {
			sprintf(p->err, "read_ccss: malloc failed");
			return 2;
		}
	}

	if ((ti = icg->find_kword(icg, 0, "DISPLAY")) >= 0) {
		if ((p->disp = strdup(icg->t[0].kdata[ti])) == NULL) {
			sprintf(p->err, "read_ccss: malloc failed");
			return 2;
		}
	}
	if ((ti = icg->find_kword(icg, 0, "TECHNOLOGY")) >= 0) {
		if ((p->tech = strdup(icg->t[0].kdata[ti])) == NULL) {
			sprintf(p->err, "read_ccss: malloc failed");
			return 2;
		}
		/* Get disptech enum from standard TECHNOLOGY string */
		p->dtech = disptech_get_strid(p->tech)->dtech;
	}
	if (p->disp == NULL && p->tech == NULL) {
		sprintf(p->err, "read_ccss: Input file doesn't contain keyword DISPLAY or TECHNOLOGY");
		return 1;
	}
	if ((ti = icg->find_kword(icg, 0, "DISPLAY_TYPE_REFRESH")) >= 0) {
		if (stricmp(icg->t[0].kdata[ti], "YES") == 0)
			p->refrmode = 1;
		else if (stricmp(icg->t[0].kdata[ti], "NO") == 0)
			p->refrmode = 0;
	}

	if ((ti = icg->find_kword(icg, 0, "UI_SELECTORS")) >= 0) {
		if ((p->sel = strdup(icg->t[0].kdata[ti])) == NULL) {
			sprintf(p->err, "read_ccss: malloc failed");
			return 2;
		}
	}

	if ((ti = icg->find_kword(icg, 0, "REFERENCE")) >= 0) {
		if ((p->ref = strdup(icg->t[0].kdata[ti])) == NULL) {
			sprintf(p->err, "read_ccss: malloc failed");
			return 2;
		}
	}

	if ((ii = icg->find_kword(icg, 0, "SPECTRAL_BANDS")) < 0) {
		sprintf(p->err,"Input file doesn't contain keyword SPECTRAL_BANDS");
		return 1;
	}
	sp.spec_n = atoi(icg->t[0].kdata[ii]);
	if ((ii = icg->find_kword(icg, 0, "SPECTRAL_START_NM")) < 0) {
		sprintf(p->err,"Input file doesn't contain keyword SPECTRAL_START_NM");
		return 1;
	}
	sp.spec_wl_short = atof(icg->t[0].kdata[ii]);
	if ((ii = icg->find_kword(icg, 0, "SPECTRAL_END_NM")) < 0) {
		sprintf(p->err,"Input file doesn't contain keyword SPECTRAL_END_NM");
		return 1;
	}
	sp.spec_wl_long = atof(icg->t[0].kdata[ii]);

	if ((ii = icg->find_kword(icg, 0, "SPECTRAL_NORM")) < 0) {
		sp.norm = 1.0;			/* Older versions don't have SPECTRAL_NORM */
	} else {
		sp.norm = atof(icg->t[0].kdata[ii]);
	}

	/* Find the fields for spectral values */
	for (j = 0; j < sp.spec_n; j++) {
		char buf[100];
		int nm;

		/* Compute nearest integer wavelength */
		nm = (int)(sp.spec_wl_short + ((double)j/(sp.spec_n-1.0))
		            * (sp.spec_wl_long - sp.spec_wl_short) + 0.5);
		
		sprintf(buf,"SPEC_%03d",nm);

		if ((spi[j] = icg->find_field(icg, 0, buf)) < 0) {
			sprintf(p->err,"Input file doesn't contain field %s",buf);
			return 1;
		}
	}

	if ((p->no_samp = icg->t[0].nsets) < 3) {
		sprintf(p->err, "Input file doesn't contain at least three spectral samples");
		p->no_samp = 0;
		return 1;
	}

	/* Allocate spectral array */
	if ((p->samples = (xspect *)malloc(sizeof(xspect) * p->no_samp)) == NULL) {
		strcpy(p->err, "Malloc failed!");
		p->no_samp = 0;
		return 2;
	}

	/* Fill sample array with the data */
	for (i = 0; i < p->no_samp; i++) {

		XSPECT_COPY_INFO(&p->samples[i], &sp);

		/* Read the spectral values for this patch */
		for (j = 0; j < sp.spec_n; j++) {
			p->samples[i].spec[j] = *((double *)icg->t[0].fdata[i][spi[j]]);
		}
	}

	return 0;
}

/* Read in the ccss CGATS .ccss file */
/* Return nz on error */
static int read_ccss(
ccss *p,		/* This */
char *inname	/* Filename to read from */
) {
	int rv;
	cgats *icg;			/* input cgats structure */

	/* Open and look at the .ccss file */
	if ((icg = new_cgats()) == NULL) {		/* Create a CGATS structure */
		sprintf(p->err, "read_ccss: new_cgats() failed");
		return 2;
	}
	icg->add_other(icg, "CCSS");		/* our special type is Model Printer Profile */

	if (icg->read_name(icg, inname)) {
		strcpy(p->err, icg->err);
		icg->del(icg);
		return 1;
	}

	if ((rv = read_ccss_cgats(p, icg)) != 0) {
		icg->del(icg);		/* Clean up */
		return rv;
	}

	icg->del(icg);		/* Clean up */

	return 0;
}

/* Read in the ccss CGATS .ccss file from a memory buffer */
/* Return nz on error */
static int buf_read_ccss(
ccss *p,		/* This */
unsigned char *buf,
int len
) {
	int rv;
	cgatsFile *fp;
	cgats *icg;			/* input cgats structure */

	if ((fp = new_cgatsFileMem(buf, len)) == NULL) {
		strcpy(p->err, "new_cgatsFileMem failed");
		return 2;
	}

	/* Open and look at the .ccss file */
	if ((icg = new_cgats()) == NULL) {		/* Create a CGATS structure */
		sprintf(p->err, "read_ccss: new_cgats() failed");
		fp->del(fp);
		return 2;
	}
	icg->add_other(icg, "CCSS");		/* our special type is Model Printer Profile */
	
	if (icg->read(icg, fp)) {
		strcpy(p->err, icg->err);
		icg->del(icg);
		fp->del(fp);
		return 1;
	}
	fp->del(fp);

	if ((rv = read_ccss_cgats(p, icg)) != 0) {
		icg->del(icg);		/* Clean up */
		return rv;
	}

	icg->del(icg);		/* Clean up */

	return 0;
}

/* Set the contents of the ccss. return nz on error. */
/* (Copy all the values) */
static int set_ccss(ccss *p,
char *orig,			/* Originator (May be NULL) */
char *crdate,		/* Creation date in ctime() format (May be NULL) */
char *desc,			/* General description (optional) */
char *disp,			/* Display make and model (optional) */
disptech dtech,		/* Display technology enum */
int refrmode,		/* Display refresh mode, -1 = unknown, 0 = n, 1 = yes */
char *sel,			/* UI selector characters - NULL for none */
char *refd,			/* Reference spectrometer description (optional) */
xspect *samples,	/* Arry of spectral samples. All assumed to be same dim as first */
int no_samp			/* Number of spectral samples */
) {
	int i;

	free_ccss(p);
	if (orig != NULL) {
		if ((p->orig = strdup(orig)) == NULL) {
			sprintf(p->err, "set_ccss: malloc orig failed");
			return 2;
		}
	}
	if (desc != NULL) {
		if ((p->desc = strdup(desc)) == NULL) {
			sprintf(p->err, "set_ccss: malloc desc failed");
			return 2;
		}
	}
	if (crdate != NULL) {
		if ((p->crdate = strdup(crdate)) == NULL) {
			sprintf(p->err, "set_ccss: malloc crdate failed");
			return 2;
		}
	}
	if (disp != NULL) {
		if ((p->disp = strdup(disp)) == NULL) {
			sprintf(p->err, "set_ccss: malloc disp failed");
			return 2;
		}
	}
	p->dtech = dtech;
	p->refrmode = refrmode;
	if (sel != NULL) {
		if ((p->sel = strdup(sel)) == NULL) {
			sprintf(p->err, "set_ccss: malloc sel failed");
			return 2;
		}
	}
	if (refd != NULL) {
		if ((p->ref = strdup(refd)) == NULL) {
			sprintf(p->err, "set_ccss: malloc ref failed");
			return 2;
		}
	}

	if (p->samples != NULL) {
		free(p->samples);
		p->samples = NULL;
	}

	if ((p->no_samp = no_samp) < 3) {
		strcpy(p->err, "Must be at least three spectral samples");
		p->no_samp = 0;
		return 1;
	}

	/* Allocate spectral array */
	if ((p->samples = (xspect *)malloc(sizeof(xspect) * p->no_samp)) == NULL) {
		strcpy(p->err, "Malloc failed!");
		p->no_samp = 0;
		return 2;
	}

	/* And copy all the values */
	for (i = 0; i < p->no_samp; i++) {
		p->samples[i] = samples[i];
	}

	return 0;
}

/* Free the contents */
static void free_ccss(ccss *p) {
	if (p != NULL) {
		if (p->desc != NULL)
			free(p->desc);
		p->desc = NULL;
		if (p->orig != NULL)
			free(p->orig);
		p->orig = NULL;
		if (p->crdate != NULL)
			free(p->crdate);
		p->crdate = NULL;
		if (p->disp != NULL)
			free(p->disp);
		p->disp = NULL;
		if (p->tech != NULL)
			free(p->tech);
		p->tech = NULL;
		if (p->sel != NULL)
			free(p->sel);
		p->sel = NULL;
		if (p->ref != NULL)
			free(p->ref);
		p->ref = NULL;
		if (p->samples != NULL)
			free(p->samples);
		p->samples = NULL;
		p->no_samp = 0;
	}
}

/* Delete it */
static void del_ccss(ccss *p) {
	if (p != NULL) {
		free_ccss(p);
		free(p);
	}
}

/* Allocate a new, uninitialised ccss */
/* Note thate black and white points aren't allocated */
ccss *new_ccss(void) {
	ccss *p;

	if ((p = (ccss *)calloc(1, sizeof(ccss))) == NULL)
		return NULL;

	/* Init method pointers */
	p->del             = del_ccss;
	p->set_ccss        = set_ccss;
	p->write_ccss      = write_ccss;
	p->buf_write_ccss  = buf_write_ccss;
	p->read_ccss       = read_ccss;
	p->buf_read_ccss   = buf_read_ccss;

	return p;
}


























