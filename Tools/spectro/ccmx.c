
/* 
 * Argyll Color Correction System
 * Colorimeter Correction Matrix
 *
 * Author: Graeme W. Gill
 * Date:   19/8/2010
 *
 * Copyright 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 * NOTE though that if SALONEINSTLIB is not defined, that this file depends
 * on other libraries that are licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3.
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
#include "ccmx.h"

#ifdef NT       /* You'd think there might be some standards.... */
# ifndef __BORLANDC__
#  define stricmp _stricmp
# endif
#else
# define stricmp strcasecmp
#endif

/* Forward declarations */

/* Utilities */

/* Method implimentations */

/* Write out the ccmx to a CGATS format .ccmx file */
/* Return nz on error */
static int create_ccmx_cgats(
ccmx *p,			/* This */
cgats **pocg        /* return CGATS structure */
) {
	int i, j, n;
	time_t clk = time(0);
	struct tm *tsp = localtime(&clk);
	char *atm = asctime(tsp); /* Ascii time */
	cgats *ocg;				/* CGATS structure */
	char buf[100];

	atm[strlen(atm)-1] = '\000';	/* Remove \n from end */

	/* Setup output cgats file */
	ocg = new_cgats();	/* Create a CGATS structure */
	ocg->add_other(ocg, "CCMX"); 		/* our special type is Colorimeter Correction Matrix */
	ocg->add_table(ocg, tt_other, 0);	/* Start the first table */

	if (p->desc != NULL)
		ocg->add_kword(ocg, 0, "DESCRIPTOR", p->desc,NULL);
	ocg->add_kword(ocg, 0, "INSTRUMENT",p->inst, NULL);
	if (p->disp != NULL)
		ocg->add_kword(ocg, 0, "DISPLAY",p->disp, NULL);
	if (p->tech != NULL)
		ocg->add_kword(ocg, 0, "TECHNOLOGY",p->tech, NULL);
	if (p->disp == NULL && p->tech == NULL) {
#ifdef DEBUG
		fprintf(stdout, "write_ccmx: ccmx doesn't contain display or techology strings");
#endif
		sprintf(p->err, "write_ccmx: ccmx doesn't contain display or techology strings");
		ocg->del(ocg);
		return 1;
	}
	if (p->cbid != 0) {
		sprintf(buf, "%d", p->cbid);
		ocg->add_kword(ocg, 0, "DISPLAY_TYPE_BASE_ID", buf, NULL);
	}
	if (p->refrmode >= 0)
		ocg->add_kword(ocg, 0, "DISPLAY_TYPE_REFRESH", p->refrmode ? "YES" : "NO", NULL);
	if (p->sel != NULL)
		ocg->add_kword(ocg, 0, "UI_SELECTORS", p->sel, NULL);
	if (p->ref != NULL)
		ocg->add_kword(ocg, 0, "REFERENCE",p->ref, NULL);

	ocg->add_kword(ocg, 0, "ORIGINATOR", "Argyll ccmx", NULL);
	ocg->add_kword(ocg, 0, "CREATED",atm, NULL);

	ocg->add_kword(ocg, 0, "COLOR_REP", "XYZ", NULL);

	/* Add fields for the matrix */
	ocg->add_field(ocg, 0, "XYZ_X", r_t);
	ocg->add_field(ocg, 0, "XYZ_Y", r_t);
	ocg->add_field(ocg, 0, "XYZ_Z", r_t);

	/* Write out the matrix values */
	for (i = 0; i < 3; i++) {
		ocg->add_set(ocg, 0, p->matrix[i][0], p->matrix[i][1], p->matrix[i][2]);
	}

    if (pocg != NULL)
        *pocg = ocg;

	return 0;
}

/* Write out the ccmx to a CGATS format .ccmx file */
/* Return nz on error */
static int write_ccmx(
ccmx *p,		/* This */
char *outname	/* Filename to write to */
) {
	int rv;
	cgats *ocg;				/* CGATS structure */

	/* Create CGATS elements */
	if ((rv = create_ccmx_cgats(p, &ocg)) != 0) {
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

/* write to a CGATS .ccmx file to a memory buffer. */
/* return nz on error, with message in err[] */
static int buf_write_ccmx(
ccmx *p,
unsigned char **buf,		/* Return allocated buffer */
int *len					/* Return length */
) {
	int rv;
	cgats *ocg;				/* CGATS structure */
	cgatsFile *fp;

	/* Create CGATS elements */
	if ((rv = create_ccmx_cgats(p, &ocg)) != 0) {
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

	/* Get the buffer the ccmx has been written to */
	if (fp->get_buf(fp, buf, (size_t *)len)) {
		strcpy(p->err, "cgatsFileMem get_buf failed");
		return 2;
	}

	ocg->del(ocg);		/* Clean up */
	fp->del(fp);

	return 0;
}

/* Read in the ccmx CGATS .ccmx file */
/* Return nz on error */
static int read_ccmx_cgats(
ccmx *p,			/* This */
cgats *icg			/* input cgats structure */
) {
	int i, j, n, ix;
	int ti;				/* Temporary CGATs index */
	int  spi[3];		/* CGATS indexes for each band */
	char *xyzfname[3] = { "XYZ_X", "XYZ_Y", "XYZ_Z" };

	if (icg->ntables == 0 || icg->t[0].tt != tt_other || icg->t[0].oi != 0) {
		sprintf(p->err, "read_ccmx: Input file isn't a CCMX format file");
		icg->del(icg);
		return 1;
	}
	if (icg->ntables != 1) {
		sprintf(p->err, "Input file doesn't contain exactly one table");
		icg->del(icg);
		return 1;
	}
	if ((ti = icg->find_kword(icg, 0, "COLOR_REP")) < 0) {
		sprintf(p->err, "read_ccmx: Input file doesn't contain keyword COLOR_REP");
		icg->del(icg);
		return 1;
	}

	if (strcmp(icg->t[0].kdata[ti],"XYZ") != 0) { 
		sprintf(p->err, "read_ccmx: Input file doesn't have COLOR_REP of XYZ");
		icg->del(icg);
		return 1;
	}

	if ((ti = icg->find_kword(icg, 0, "DESCRIPTOR")) >= 0) {
		if ((p->desc = strdup(icg->t[0].kdata[ti])) == NULL) {
			sprintf(p->err, "read_ccmx: malloc failed");
			icg->del(icg);
			return 2;
		}
	}

	if ((ti = icg->find_kword(icg, 0, "INSTRUMENT")) < 0) {
		sprintf(p->err, "read_ccmx: Input file doesn't contain keyword INSTRUMENT");
		icg->del(icg);
		return 1;
	}
	if ((p->inst = strdup(icg->t[0].kdata[ti])) == NULL) {
		sprintf(p->err, "read_ccmx: malloc failed");
		icg->del(icg);
		return 2;
	}

	if ((ti = icg->find_kword(icg, 0, "DISPLAY")) >= 0) {
		if ((p->disp = strdup(icg->t[0].kdata[ti])) == NULL) {
			sprintf(p->err, "read_ccmx: malloc failed");
			icg->del(icg);
			return 2;
		}
	}
	if ((ti = icg->find_kword(icg, 0, "TECHNOLOGY")) >= 0) {
		if ((p->tech = strdup(icg->t[0].kdata[ti])) == NULL) {
			sprintf(p->err, "read_ccmx: malloc failed");
			icg->del(icg);
			return 2;
		}
	}
	if (p->disp == NULL && p->tech == NULL) {
		sprintf(p->err, "read_ccmx: Input file doesn't contain keyword DISPLAY or TECHNOLOGY");
		icg->del(icg);
		return 1;
	}
	if ((ti = icg->find_kword(icg, 0, "DISPLAY_TYPE_REFRESH")) >= 0) {
		if (stricmp(icg->t[0].kdata[ti], "YES") == 0)
			p->refrmode = 1;
		else if (stricmp(icg->t[0].kdata[ti], "NO") == 0)
			p->refrmode = 0;
	} else {
		p->refrmode = -1;
	}
	if ((ti = icg->find_kword(icg, 0, "DISPLAY_TYPE_BASE_ID")) >= 0) {
		p->cbid = atoi(icg->t[0].kdata[ti]);
	} else {
	}

	if ((ti = icg->find_kword(icg, 0, "UI_SELECTORS")) >= 0) {
		if ((p->sel = strdup(icg->t[0].kdata[ti])) == NULL) {
			sprintf(p->err, "read_ccmx: malloc failed");
			icg->del(icg);
			return 2;
		}
	}

	if ((ti = icg->find_kword(icg, 0, "REFERENCE")) >= 0) {
		if ((p->ref = strdup(icg->t[0].kdata[ti])) == NULL) {
			sprintf(p->err, "read_ccmx: malloc failed");
			icg->del(icg);
			return 2;
		}
	}

	/* Locate the fields */
	for (i = 0; i < 3; i++) {	/* XYZ fields */
		if ((spi[i] = icg->find_field(icg, 0, xyzfname[i])) < 0) {
			sprintf(p->err, "read_ccmx: Input file doesn't contain field %s", xyzfname[i]);
			icg->del(icg);
			return 1;
		}
		if (icg->t[0].ftype[spi[i]] != r_t) {
			sprintf(p->err, "read_ccmx: Input file field %s is wrong type", xyzfname[i]);
			icg->del(icg);
			return 1;
		}
	}

	if (icg->t[0].nsets != 3) {
		sprintf(p->err, "read_ccmx: Input file doesn't have exactly 3 sets");
		icg->del(icg);
		return 1;
	}

	/* Read the matrix values */
	for (ix = 0; ix < icg->t[0].nsets; ix++) {
		p->matrix[ix][0] = *((double *)icg->t[0].fdata[ix][spi[0]]);
		p->matrix[ix][1] = *((double *)icg->t[0].fdata[ix][spi[1]]);
		p->matrix[ix][2] = *((double *)icg->t[0].fdata[ix][spi[2]]);
	}

	return 0;
}

/* Read in the ccmx CGATS .ccmx file */
/* Return nz on error */
static int read_ccmx(
ccmx *p,			/* This */
char *inname	/* Filename to read from */
) {
	int rv;
	cgats *icg;			/* input cgats structure */

	/* Open and look at the .ccmx */
	if ((icg = new_cgats()) == NULL) {		/* Create a CGATS structure */
		sprintf(p->err, "read_ccmx: new_cgats() failed");
		return 2;
	}
	icg->add_other(icg, "CCMX");		/* our special type is Model Printer Profile */

	if (icg->read_name(icg, inname)) {
		strcpy(p->err, icg->err);
		icg->del(icg);
		return 1;
	}

	if ((rv = read_ccmx_cgats(p, icg)) != 0) {
		icg->del(icg);
		return rv;
	}

	icg->del(icg);		/* Clean up */

	return 0;
}

/* Read in the ccmx CGATS .ccmx file from a memory buffer */
/* Return nz on error */
static int buf_read_ccmx(
ccmx *p,		/* This */
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

	/* Open and look at the .ccmx file */
	if ((icg = new_cgats()) == NULL) {		/* Create a CGATS structure */
		sprintf(p->err, "read_ccmx: new_cgats() failed");
		fp->del(fp);
		return 2;
	}
	icg->add_other(icg, "CCMX");		/* our special type is Model Printer Profile */
	
	if (icg->read(icg, fp)) {
		strcpy(p->err, icg->err);
		icg->del(icg);
		fp->del(fp);
		return 1;
	}
	fp->del(fp);

	if ((rv = read_ccmx_cgats(p, icg)) != 0) {
		icg->del(icg);		/* Clean up */
		return rv;
	}

	icg->del(icg);		/* Clean up */

	return 0;
}

/* Lookup an XYZ or Lab color */
static void xform(
ccmx *p,						/* This */
double *out,				/* Output XYZ */
double *in					/* Input XYZ */
) {
	icmMulBy3x3(out, p->matrix, in);
}


/* Set the contents of the ccmx. return nz on error. */
static int set_ccmx(ccmx *p,
char *desc,			/* General description (optional) */
char *inst,			/* Instrument description to copy from */
char *disp,			/* Display make and model (optional if tech) */
char *tech,			/* Display technology description (optional if disp) */
int refrmode,		/* Display refresh mode, -1 = unknown, 0 = n, 1 = yes */
int cbid,			/* Display type calibration base ID, 0 = unknown */
char *sel,			/* UI selector characters - NULL for none */
char *refd,			/* Reference spectrometer description (optional) */
double mtx[3][3]	/* Transform matrix to copy from */
) {
	if ((p->desc = desc) != NULL && (p->desc = strdup(desc)) == NULL) {
		sprintf(p->err, "set_ccmx: malloc failed");
		return 2;
	}
	if ((p->inst = inst) != NULL && (p->inst = strdup(inst)) == NULL) {
		sprintf(p->err, "set_ccmx: malloc failed");
		return 2;
	}
	if ((p->disp = disp) != NULL && (p->disp = strdup(disp)) == NULL) {
		sprintf(p->err, "set_ccmx: malloc failed");
		return 2;
	}
	if ((p->tech = tech) != NULL && (p->tech = strdup(tech)) == NULL) {
		sprintf(p->err, "set_ccmx: malloc failed");
		return 2;
	}

	p->refrmode = refrmode;
	p->cbid = cbid;

	if (sel != NULL) {
		if ((p->sel = strdup(sel)) == NULL) {
			sprintf(p->err, "set_ccmx: malloc sel failed");
			return 2;
		}
	}
	if ((p->ref = refd) != NULL && (p->ref = strdup(refd)) == NULL) {
		sprintf(p->err, "set_ccmx: malloc failed");
		return 2;
	}

	icmCpy3x3(p->matrix, mtx);

	return 0;
}

#ifndef SALONEINSTLIB

/* ------------------------------------------- */
/* Modified version that de-weights Luminance errors by 5: */
/* Return the CIE94 Delta E color difference measure, squared */
static double wCIE94sq(double Lab0[3], double Lab1[3]) {
	double desq, dhsq;
	double dlsq, dcsq;
	double c12;

	{
		double dl, da, db;
		dl = Lab0[0] - Lab1[0];
		dlsq = dl * dl;		/* dl squared */
		da = Lab0[1] - Lab1[1];
		db = Lab0[2] - Lab1[2];

		/* Compute normal Lab delta E squared */
		desq = dlsq + da * da + db * db;
	}

	{
		double c1, c2, dc;

		/* Compute chromanance for the two colors */
		c1 = sqrt(Lab0[1] * Lab0[1] + Lab0[2] * Lab0[2]);
		c2 = sqrt(Lab1[1] * Lab1[1] + Lab1[2] * Lab1[2]);
		c12 = sqrt(c1 * c2);	/* Symetric chromanance */

		/* delta chromanance squared */
		dc = c2 - c1;
		dcsq = dc * dc;
	}

	/* Compute delta hue squared */
	if ((dhsq = desq - dlsq - dcsq) < 0.0)
		dhsq = 0.0;
	{
		double sc, sh;

		/* Weighting factors for delta chromanance & delta hue */
		sc = 1.0 + 0.048 * c12;
		sh = 1.0 + 0.014 * c12;
		return 0.2 * 0.2 * dlsq + dcsq/(sc * sc) + dhsq/(sh * sh);
	}
}

/* ------------------------------------------- */

/* Context for optimisation callback */
typedef struct {
	int npat;
	double (*refs)[3];	/* Array of XYZ values from spectrometer */
	double (*cols)[3];	/* Array of XYZ values from colorimeter */
	int wix;			/* White index */
	icmXYZNumber wh;	/* White reference */
} cntx;


/* Optimisation function */
/* Compute the sum of delta E's squared for the */
/* tp will be the 9 matrix values */
/* It's not clear if the white weighting is advantagous or not. */
double optf(void *fdata, double *tp) {
	cntx *cx = (cntx *)fdata;
	int i;
	double de;
	double m[3][3];

	m[0][0] = tp[0];
	m[0][1] = tp[1];
	m[0][2] = tp[2];
	m[1][0] = tp[3];
	m[1][1] = tp[4];
	m[1][2] = tp[5];
	m[2][0] = tp[6];
	m[2][1] = tp[7];
	m[2][2] = tp[8];

	for (de = 0.0, i = 0; i < cx->npat; i++) {
		double tlab[3], xyz[3], lab[3];
		icmXYZ2Lab(&cx->wh, tlab, cx->refs[i]);

		icmMulBy3x3(xyz, m, cx->cols[i]);
		icmXYZ2Lab(&cx->wh, lab, xyz);

		if (i == cx->wix)
			de += cx->npat/4.0 * wCIE94sq(tlab, lab);	/* Make white weight = 1/4 all others */
		else
			de += wCIE94sq(tlab, lab);
//printf("~1 %d: txyz %f %f %f, tlab %f %f %f\n", i,cx->refs[i][0], cx->refs[i][1], cx->refs[i][2], tlab[0], tlab[1], tlab[2]);
//printf("~1 %d: xyz %f %f %f\n", i,cx->cols[i][0], cx->cols[i][1], cx->cols[i][2]);
//printf("~1 %d: mxyz %f %f %f, lab %f %f %f\n", i,xyz[0], xyz[1], xyz[2], lab[0], lab[1], lab[2]);
//printf("~1 %d: de %f\n", i,wCIE94(tlab, lab));
	}

	de /= cx->npat;
#ifdef DEBUG
//	printf("~1 return values = %f\n",de);
#endif

	return de;
}

/* Create a ccmx from measurements. return nz on error. */
static int create_ccmx(ccmx *p,
char *desc,			/* General description (optional) */
char *inst,			/* Instrument description to copy from */
char *disp,			/* Display make and model (optional if tech) */
char *tech,			/* Display technology description (optional if disp) */
int refrmode,		/* Display refresh mode, -1 = unknown, 0 = n, 1 = yes */
int cbid,			/* Display type calibration base index, 0 = unknown */
char *sel,			/* UI selector characters - NULL for none */
char *refd,			/* Reference spectrometer description (optional) */
int npat,			/* Number of samples in following arrays */
double (*refs)[3],	/* Array of XYZ values from spectrometer */
double (*cols)[3]		/* Array of XYZ values from colorimeter */
) {
	int i, mxix;
	double maxy = -1e6;
	cntx cx;
	double cp[9], sa[9];

	if ((p->desc = desc) != NULL && (p->desc = strdup(desc)) == NULL) {
		sprintf(p->err, "create_ccmx: malloc failed");
		return 2;
	}
	if ((p->inst = inst) != NULL && (p->inst = strdup(inst)) == NULL) {
		sprintf(p->err, "create_ccmx: malloc failed");
		return 2;
	}
	if ((p->disp = disp) != NULL && (p->disp = strdup(disp)) == NULL) {
		sprintf(p->err, "create_ccmx: malloc failed");
		return 2;
	}
	if ((p->tech = tech) != NULL && (p->tech = strdup(tech)) == NULL) {
		sprintf(p->err, "create_ccmx: malloc failed");
		return 2;
	}
	p->refrmode = refrmode;
	p->cbid = cbid;

	if (sel != NULL) {
		if ((p->sel = strdup(sel)) == NULL) {
			sprintf(p->err, "create_ccmx: malloc sel failed");
			return 2;
		}
	}
	if ((p->ref = refd) != NULL && (p->ref = strdup(refd)) == NULL) {
		sprintf(p->err, "create_ccmx: malloc failed");
		return 2;
	}

	/* Find the white patch */

	cx.npat = npat;
	cx.refs = refs;
	cx.cols = cols;

	for (i = 0; i < npat; i++) {
		if (refs[i][1] > maxy) {
			maxy = refs[i][1];
			cx.wix = i;
		}
	}
#ifdef DEBUG
	printf("white = %f %f %f\n",refs[cx.wix][0],refs[cx.wix][1],refs[cx.wix][1]);
#endif
	cx.wh.X = refs[cx.wix][0];
	cx.wh.Y = refs[cx.wix][1];
	cx.wh.Z = refs[cx.wix][2];

	/* Starting matrix */
	cp[0] = 1.0; cp[1] = 0.0; cp[2] = 0.0;
	cp[3] = 0.0; cp[4] = 1.0; cp[5] = 0.0;
	cp[6] = 0.0; cp[7] = 0.0; cp[8] = 1.0;

	for (i = 0; i < 9; i++)
		sa[i] = 0.1;

	if (powell(NULL, 9, cp, sa, 1e-6, 2000, optf, &cx, NULL, NULL) < 0.0) {
		sprintf(p->err, "create_ccmx: powell() failed");
		return 1;
	}

	p->matrix[0][0] = cp[0];
	p->matrix[0][1] = cp[1];
	p->matrix[0][2] = cp[2];
	p->matrix[1][0] = cp[3];
	p->matrix[1][1] = cp[4];
	p->matrix[1][2] = cp[5];
	p->matrix[2][0] = cp[6];
	p->matrix[2][1] = cp[7];
	p->matrix[2][2] = cp[8];

	/* Compute the average and max errors */
	p->av_err = p->mx_err = 0.0;
	for (i = 0; i < npat; i++) {
		double tlab[3], xyz[3], lab[3], de;
		icmXYZ2Lab(&cx.wh, tlab, cx.refs[i]);
		icmMulBy3x3(xyz, p->matrix, cx.cols[i]);
		icmXYZ2Lab(&cx.wh, lab, xyz);
		de = icmCIE94(tlab, lab);
		p->av_err += de;
		if (de > p->mx_err) {
			p->mx_err = de;
			mxix = i;
		}
//printf("~1 %d: de %f, tlab %f %f %f, lab %f %f %f\n",i,de,tlab[0], tlab[1], tlab[2], lab[0], lab[1], lab[2]);
	}
	p->av_err /= (double)npat;

//printf("~1 max error is index %d\n",mxix);

#ifdef DEBUG
	printf("Average error %f, max %f\n",p->av_err,p->mx_err);
	printf("Correction matrix is:\n");
	printf("  %f %f %f\n", cp[0], cp[1], cp[2]);
	printf("  %f %f %f\n", cp[3], cp[4], cp[5]);
	printf("  %f %f %f\n", cp[6], cp[7], cp[8]);
#endif

	return 0;
}

#else /* SALONEINSTLIB */

/* Create a ccmx from measurements. return nz on error. */
static int create_ccmx(ccmx *p,
char *desc,			/* General description (optional) */
char *inst,			/* Instrument description to copy from */
char *disp,			/* Display make and model (optional if tech) */
char *tech,			/* Display technology description (optional if disp) */
int refrmode,		/* Display refresh mode, -1 = unknown, 0 = n, 1 = yes */
int cbid,			/* Display type calibration base ID, 0 = unknown */
char *sel,			/* UI selector characters - NULL for none */
char *refd,			/* Reference spectrometer description (optional) */
int npat,			/* Number of samples in following arrays */
double (*refs)[3],	/* Array of XYZ values from spectrometer */
double (*cols)[3]		/* Array of XYZ values from colorimeter */
) {
	sprintf(p->err, "create_ccmx: not implemented in ccmx.c");
	return 1;
}

#endif /* SALONEINSTLIB */

/* Delete it */
static void del_ccmx(ccmx *p) {
	if (p != NULL) {
		if (p->desc != NULL)
			free(p->desc);
		if (p->inst != NULL)
			free(p->inst);
		if (p->disp != NULL)
			free(p->disp);
		if (p->tech != NULL)
			free(p->tech);
		if (p->sel != NULL)
			free(p->sel);
		if (p->ref != NULL)
			free(p->ref);
		free(p);
	}
}

/* Allocate a new, uninitialised ccmx */
/* Note thate black and white points aren't allocated */
ccmx *new_ccmx(void) {
	ccmx *p;

	if ((p = (ccmx *)calloc(1, sizeof(ccmx))) == NULL)
		return NULL;

	p->refrmode = -1;
	p->cbid = 0;

	/* Init method pointers */
	p->del            = del_ccmx;
	p->set_ccmx       = set_ccmx;
	p->create_ccmx    = create_ccmx;
	p->write_ccmx     = write_ccmx;
	p->buf_write_ccmx = buf_write_ccmx;
	p->read_ccmx      = read_ccmx;
	p->buf_read_ccmx  = buf_read_ccmx;
	p->xform          = xform;

	return p;
}


























