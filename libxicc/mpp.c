
/* 
 * Argyll Color Correction System
 * Model Printer Profile object.
 *
 * Author: Graeme W. Gill
 * Date:   24/2/2002
 *
 * Copyright 2003 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* This version (based on mpp_x1) has n * 2^(n-1) shape params, */
/* used for linear interpolation of the shaping correction. */

/*
 * This program takes in the scattered test chart
 * points, and creates a model based forward printer profile
 * (Device -> CIE + spectral), based on heuristic extensions to
 * the Neugenbauer model.
 * It is designed to handle an arbitrary number of colorants,
 * and in the future, (optionaly) create an aproximate ink overlap/mixing model
 * to allow synthesis of a forward model for a hyperthetical
 * similar printing process with additional inks.
 *
 * The model is as follows:
 *
 *   A per input channel transfer curve, that models
 *   per channel dot gain, transfer curve processing etc.
 *
 *   A per input channel shape modification model.
 *   Each transfer curve adjusted value is further
 *   adjusted in a way that depends on the combination
 *   of value of all the other input channels.
 *   This allows for ink interaction effects in a way
 *   that should allow good conformance to all combinations
 *   of input values at 50% values.
 *
 *   An n-linear interpolation between all combination of
 *   the 0 and 100% colorant primary combinations (Neugenbauer).
 *
 * The model making can be rather slow, particularly if high
 * quality, large number of colorants, large number of sample
 * points.
 *
 * This code is based on profile.c, sprof.c and xlut.c
 *
 */

/*
 * TTBD:
 * 		
 *		!!!! Should change device transfer model to include offset & scale, 		
 * 		to better match display & other devices !!!!
 * 		
 *		Should add Jab pcs mode, so that Jab gamuts can be
 *      written.
 *
 *      Remove #ifndef DEBUG & replace with verbose progress bars.
 *
 *      Add support for extra profile details to create() to support
 *      profxinf like stuff.
 * 		
 *      Add ink order and overlay modeling stuff back in, with
 *      new ink overlay model (see mpprof0.c). 		
 * 		
 *      Rather than computing XYZ based versios of the print model
 *      and ink mixing models, should compute spectrally sharpened
 *      equivalents to XYZ ?? (The spectral CIE base values don't
 *		seem much more accurate than XYZ, so perhaps remove SHARPEN code ?). 		
 * 		
 *      Need to cleanup error handling. 		
 */

#define VERSION "1.0"

#undef DEBUG
#undef TESTDFUNC		/* Check delta functions */
#undef NODDV			/* Use (very slow) non d/dv powell */
#undef DOPLOT			/* Plot the device curves */

#undef NOPROCESS		/* Define to skip all fitting */
#define MULTIPASS		/* Fit passes by parts (normal mode) */
#undef BIGBANG			/* Define to fit all parameters at once */
#undef ISHAPE			/* define to try SVD init of shape parameters */
#undef SHARPEN			/* use sharpened XYZ values for modeling */
						/* (Wrecks derivative lookup at the moment, and makes */
						/* accuracy worse!) */

						/* Transfer curve parameter (wiggle) minimisation weight */
//#define TRANS_BASE  0.2	/* 0 & 1 harmonic parameter weight */
//#define TRANS_HBASE 0.8	/* 2nd harmonic and above base parameter weight */
#define TRANS_HW01		  0.01		/* 0 & 1 harmonic weights */
#define TRANS_HBREAK	  3			/* Harmonic that has HWBR */
//#define TRANS_HWBR        0.5		/* Base weight of harmonics HBREAK up */
//#define TRANS_HWINC       0.5		/* Increase in weight for each harmonic above HWBR */
#define TRANS_HWBR        0.5		/* Base weight of harmonics HBREAK up */
#define TRANS_HWINC       0.5		/* Increase in weight for each harmonic above HWBR */

//#define SHAPE_PMW 0.2	/* Shape parameter (wiggle) minimisation weight */
#define SHAPE_PMW 10.0	/* Shape parameter (wiggle) minimisation weight */
#define COMB_PMW 0.008	/* Primary combination anchor point distance weight */

#define verbo stdout

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#if defined(__IBMC__) && defined(_M_IX86)
#include <float.h>
#endif
#include "numlib.h"
#include "cgats.h"
#include "icc.h"
#include "xicc.h"		/* Spectral support */
#include "xspect.h"		/* Spectral support */
#include "xcolorants.h"	/* Known colorants support */
#include "insttypes.h"
#include "gamut.h"
#include "mpp.h"
#ifdef DOPLOT
#include "plot.h"
#endif /* DOPLOT */

/* Forward declarations */
static double bandval(mpp *p, int band, double *dev);
static double dbandval(mpp *p, double *dv, int band, double *dev);
static void forward(mpp *p, double *spec, double *Lab, double *XYZ, double *dev);
static int create(mpp *p, int verb, int quality, int display, double limit, inkmask devmask,
                  int spec_n, double spec_wl_short, double spec_wl_long,
                  double norm, instType itype, int nodp, mppcol *points);
static void compute_wb(mpp *p);
static void init_shape(mpp *p);

/* Utilities */

#ifdef SHARPEN
/* Convert from XYZ to spectrally sharpened response */
/* (Can this generate -ve values for real colors ??) */
static void XYZ2sharp(double *a, double *b, double *c) {
	double xyz[3], rgb[3];

	xyz[0] = *a;
	xyz[1] = *b;
	xyz[2] = *c;

	rgb[0] =  0.8562 * xyz[0] + 0.3372 * xyz[1] - 0.1934 * xyz[2];
	rgb[1] = -0.8360 * xyz[0] + 1.8327 * xyz[1] + 0.0033 * xyz[2];
	rgb[2] =  0.0357 * xyz[0] - 0.0469 * xyz[1] + 1.0112 * xyz[2];

	*a = rgb[0];
	*b = rgb[1];
	*c = rgb[2];
}
	
/* Convert from spectrally sharpened response to XYZ */
static void sharp2XYZ(double *a, double *b, double *c) {
	double xyz[3], rgb[3];

	rgb[0] = *a;
	rgb[1] = *b;
	rgb[2] = *c;

	xyz[0] =  0.9873999149199270 * rgb[0]
	       -  0.1768250198556842 * rgb[1]
	       +  0.1894251049357572 * rgb[2];
	xyz[1] =  0.4504351090445316 * rgb[0]
	       +  0.4649328977527109 * rgb[1]
	       +  0.0846319932027575 * rgb[2];
	xyz[2] = -0.0139683251072516 * rgb[0]
	       +  0.0278065725014340 * rgb[1]
	       +  0.9861617526058175 * rgb[2];

	*a = xyz[0];
	*b = xyz[1];
	*c = xyz[2];
}
#endif /* SHARPEN */


/* Method implimentations */

/* Write out the mpp to a CGATS format .mpp file */
/* Return nz on error */
static int write_mpp(
mpp *p,			/* This */
char *outname,	/* Filename to write to */
int dolab		/* If NZ, write Lab values rather than XYZ */
) {
	int i, j, n;
	time_t clk = time(0);
	struct tm *tsp = localtime(&clk);
	char *atm = asctime(tsp); /* Ascii time */
	cgats *ocg;		/* CGATS structure */
	char *ident = icx_inkmask2char(p->imask, 1); 
	int nsetel = 0;
	cgats_set_elem *setel;	/* Array of set value elements */
	char buf[100];

	atm[strlen(atm)-1] = '\000';	/* Remove \n from end */

	/* Setup output cgats file */
	ocg = new_cgats();	/* Create a CGATS structure */
	ocg->add_other(ocg, "MPP"); 		/* our special type is Model Printer Profile */
	ocg->add_table(ocg, tt_other, 0);	/* Start the first table */

	ocg->add_kword(ocg, 0, "DESCRIPTOR", "Argyll Model Printer Profile, Colorant linearisation",NULL);
	ocg->add_kword(ocg, 0, "ORIGINATOR", "Argyll mpp", NULL);
	ocg->add_kword(ocg, 0, "CREATED",atm, NULL);
	if (p->display)
		ocg->add_kword(ocg, 0, "DEVICE_CLASS","DISPLAY", NULL);	/* What sort of device this is */
	else {
		ocg->add_kword(ocg, 0, "DEVICE_CLASS","OUTPUT", NULL);	/* What sort of device this is */

		/* Note what instrument the chart was read with, in case we */
		/* want to apply FWA when converting spectral data to CIE */
		ocg->add_kword(ocg, 0, "TARGET_INSTRUMENT", inst_name(p->itype) , NULL);
	
		sprintf(buf,"%5.1f",p->limit * 100.0);
		ocg->add_kword(ocg, 0, "TOTAL_INK_LIMIT", buf, NULL);
	}

	ocg->add_kword(ocg, 0, "COLOR_REP", ident, NULL);

	/* Record how many factors are used in device channel transfer curve */
	sprintf(buf,"%d",p->cord);
	ocg->add_kword(ocg, 0, "TRANSFER_ORDERS", buf, NULL);

	/* Record if shaper parameters are being used */
	if (p->useshape) {
		ocg->add_kword(ocg, 0, "USE_SHAPER", "YES", NULL);
	} else {
		ocg->add_kword(ocg, 0, "USE_SHAPER", "NO", NULL);
	}

	/* Setup the table, which holds all the model parameters. */
	/* There is always a parameter per X Y Z or spectral band */
	ocg->add_field(ocg, 0, "PARAMETER", nqcs_t);
	if (dolab) {
		ocg->add_field(ocg, 0, "LAB_L", r_t);
		ocg->add_field(ocg, 0, "LAB_A", r_t);
		ocg->add_field(ocg, 0, "LAB_B", r_t);
	} else {
		ocg->add_field(ocg, 0, "XYZ_X", r_t);
		ocg->add_field(ocg, 0, "XYZ_Y", r_t);
		ocg->add_field(ocg, 0, "XYZ_Z", r_t);
	}
	nsetel = 1 + 3;

	/* Add fields for spectral values */
	if (p->spec_n > 0) {

		nsetel += p->spec_n;		/* Spectral values */
		sprintf(buf,"%d", p->spec_n);
		ocg->add_kword(ocg, 0, "SPECTRAL_BANDS",buf, NULL);
		sprintf(buf,"%f", p->spec_wl_short);
		ocg->add_kword(ocg, 0, "SPECTRAL_START_NM",buf, NULL);
		sprintf(buf,"%f", p->spec_wl_long);
		ocg->add_kword(ocg, 0, "SPECTRAL_END_NM",buf, NULL);
		sprintf(buf,"%f", p->norm * 100.0);
		ocg->add_kword(ocg, 0, "SPECTRAL_NORM",buf, NULL);

		/* Generate fields for spectral values */
		for (i = 0; i < p->spec_n; i++) {
			int nm;
	
			/* Compute nearest integer wavelength */
			nm = (int)(p->spec_wl_short + ((double)i/(p->spec_n-1.0))
			            * (p->spec_wl_long - p->spec_wl_short) + 0.5);
			
			sprintf(buf,"SPEC_%03d",nm);
			ocg->add_field(ocg, 0, buf, r_t);
		}
	}

	if ((setel = (cgats_set_elem *)malloc(sizeof(cgats_set_elem) * nsetel)) == NULL) {
		free(ident);
		sprintf(p->err,"write_mpp: malloc of setel failed");
		return 1;
	}

	/* Write out the transfer curve values */
	for (i = 0; i < p->n; i++) {		/* each colorant */
		for (j = 0; j < p->cord; j++) {	/* curve order values */
			
			sprintf(buf,"t_%d_%d",i,j);
			setel[0].c = buf;		/* Parameter identifier */
				
			for (n = 0; n < (3+p->spec_n); n++)
				setel[1+n].d = p->tc[i][n][j];
			ocg->add_setarr(ocg, 0, setel);
		}
	}

	if (p->useshape) {

		/* Write out the shaper values */
		for (i = 0; i < p->nnn2; i++) {	/* For all shaper values */
			int m = p->c2f[i].ink;
			int k = p->c2f[i].comb;

			sprintf(buf,"s_%d_%d",m, k);
			setel[0].c = buf;		/* Parameter identifier */
					
			for (n = 0; n < (3+p->spec_n); n++)
				setel[1+n].d = p->shape[m][k][n];
		
			ocg->add_setarr(ocg, 0, setel);
		}
	}

	/* Write out the colorant combination values */
	for (i = 0; i < p->nn; i++) {

		sprintf(buf,"c_%d",i);
		setel[0].c = buf;		/* Parameter identifier */
				
		for (n = 0; n < (3+p->spec_n); n++)
			setel[1+n].d = p->pc[i][n];
		
#ifdef SHARPEN
		sharp2XYZ(&setel[1+0].d, &setel[1+1].d, &setel[1+2].d);
#endif
		if (dolab) {
			double ttt[3];
			ttt[0] = setel[1+0].d, ttt[1] = setel[1+1].d, ttt[2] = setel[1+2].d;
			icmXYZ2Lab(&icmD50, ttt, ttt);
			setel[1+0].d = ttt[0], setel[1+1].d = ttt[1], setel[1+2].d = ttt[2];
		}
		ocg->add_setarr(ocg, 0, setel);
	}
	free(setel);
	free(ident);

	/* Write it */
	if (ocg->write_name(ocg, outname)) {
		strcpy(p->err, ocg->err);
		return 1;
	}

	ocg->del(ocg);		/* Clean up */

	return 0;
}

/* Read in the mpp CGATS .mpp file */
/* Return nz on error */
static int read_mpp(
mpp *p,			/* This */
char *inname	/* Filename to read from */
) {
	int i, j, n, ix;
	cgats *icg;			/* input cgats structure */
	int ti;				/* Temporary CGATs index */
	int islab = 0;		/* nz if Lab parameters */

	/* Open and look at the .mpp model printer profile */
	if ((icg = new_cgats()) == NULL) {		/* Create a CGATS structure */
		sprintf(p->err, "read_mpp: new_cgats() failed");
		return 2;
	}
	icg->add_other(icg, "MPP");		/* our special type is Model Printer Profile */

	if (icg->read_name(icg, inname)) {
		strcpy(p->err, icg->err);
		icg->del(icg);
		return 1;
	}

	if (icg->ntables == 0 || icg->t[0].tt != tt_other || icg->t[0].oi != 0) {
		sprintf(p->err, "read_mpp: Input file '%s' isn't a MPP format file",inname);
		icg->del(icg);
		return 1;
	}
	if (icg->ntables != 1) {
		sprintf(p->err, "Input file '%s' doesn't contain exactly one table",inname);
		icg->del(icg);
		return 1;
	}
	if ((ti = icg->find_kword(icg, 0, "COLOR_REP")) < 0) {
		sprintf(p->err, "read_mpp: Input file '%s' doesn't contain keyword COLOR_REP",inname);
		icg->del(icg);
		return 1;
	}

	p->imask = icx_char2inkmask(icg->t[0].kdata[ti]); 
	p->n = icx_noofinks(p->imask);
	p->nn = 1 << p->n;
	p->nnn2 = p->n * p->nn/2;

	if (p->n == 0) {
		sprintf(p->err, "read_mpp: COLOR_REP '%s' invalid from file '%s' (No matching devmask)",
		        icg->t[0].kdata[ti], inname);
		icg->del(icg);
		return 1;
	}

	/* See if it is the expected device class */
	if ((ti = icg->find_kword(icg, 0, "DEVICE_CLASS")) < 0) {
		sprintf(p->err, "read_mpp: Input file '%s' doesn't contain keyword DEVICE_CLASS",inname);
		icg->del(icg);
		return 1;
	}
	if (strcmp(icg->t[0].kdata[ti],"OUTPUT") == 0) {

		if ((ti = icg->find_kword(icg, 0, "TOTAL_INK_LIMIT")) >= 0) {
			double imax;
			imax = atof(icg->t[0].kdata[ti]);
			p->limit = imax/100.0;
		} else {
			p->limit = 0.0;				/* Don't use ink limit */
		}

		if ((ti = icg->find_kword(icg, 0, "TARGET_INSTRUMENT")) < 0) {
			sprintf(p->err, "read_mpp: Can't find keyword TARGET_INSTRUMENT in file '%s'", inname);
			icg->del(icg);
			return 1;
		}
	
		if ((p->itype = inst_enum(icg->t[0].kdata[ti])) == instUnknown
		 &&  icg->find_kword(icg, 0, "SPECTRAL_BANDS") >= 0) {
			sprintf(p->err, "read_mpp: Unrecognised target instrument '%s' in file '%s'",
			        icg->t[0].kdata[ti], inname);
			icg->del(icg);
			return 1;
		}

		p->display = 0;

	} else if (strcmp(icg->t[0].kdata[ti],"DISPLAY") == 0) {

		p->display = 1;
		p->itype = instUnknown;
		p->limit = p->n;

	} else {
		/* Don't know anything else at the moment */
		sprintf(p->err, "read_mpp: Input file '%s' has unknown DEVICE_CLASS '%s'",
		        inname, icg->t[0].kdata[ti]);
		icg->del(icg);
		return 1;
	}

	/* Read the number of device linearisation orders */
	if ((ti = icg->find_kword(icg, 0, "TRANSFER_ORDERS")) < 0) {
		sprintf(p->err, "read_mpp: Input file '%s' doesn't contain keyword TRANSFER_ORDERS",
		        inname);
		icg->del(icg);
		return 1;
	}
	p->cord = atoi(icg->t[0].kdata[ti]);
	if (p->cord < 1 || p->cord > MPP_MXTCORD) {
		sprintf(p->err, "read_mpp: Input file '%s' has out of range TRANSFER_ORDERS %d",
		        inname, p->cord);
		icg->del(icg);
		return 1;
	}

	/* See if shaper parameters are used */
	p->useshape = 0;
	if ((ti = icg->find_kword(icg, 0, "USE_SHAPER")) >= 0) {
		if(strcmp(icg->t[0].kdata[ti], "YES") == 0)
			p->useshape = 1;
	}

	/* Read the model parameters */
	{
		int ci;						/* Parameter dentified index */
		int  spi[3+MPP_MXBANDS];	/* CGATS indexes for each band */
		char *xyzfname[3] = { "XYZ_X", "XYZ_Y", "XYZ_Z" };
		char *labfname[3] = { "LAB_L", "LAB_A", "LAB_B" };
		char buf[100];

		/* See if we have spectral information available */
		if (icg->find_kword(icg, 0, "SPECTRAL_BANDS") < 0) {
			p->spec_n = 0;			/* None */
		} else {
			if ((ti = icg->find_kword(icg, 0, "SPECTRAL_BANDS")) < 0)
				error ("Input file doesn't contain keyword SPECTRAL_BANDS");
			p->spec_n = atoi(icg->t[0].kdata[ti]);
			if ((ti = icg->find_kword(icg, 0, "SPECTRAL_START_NM")) < 0)
				error ("Input file doesn't contain keyword SPECTRAL_START_NM");
			p->spec_wl_short = atof(icg->t[0].kdata[ti]);
			if ((ti = icg->find_kword(icg, 0, "SPECTRAL_END_NM")) < 0)
				error ("Input file doesn't contain keyword SPECTRAL_END_NM");
			p->spec_wl_long = atof(icg->t[0].kdata[ti]);
			if ((ti = icg->find_kword(icg, 0, "SPECTRAL_NORM")) < 0)
				error ("Input file doesn't contain keyword SPECTRAL_NORM");
			p->norm = atof(icg->t[0].kdata[ti])/100.0;
		}

		if ((new_mppcol(&p->white, p->n, p->spec_n)) != 0) {
			error("Malloc failed!");
		}
		if ((new_mppcol(&p->black, p->n, p->spec_n)) != 0) {
			error("Malloc failed!");
		}
		if ((new_mppcol(&p->kblack, p->n, p->spec_n)) != 0) {
			error("Malloc failed!");
		}
		init_shape(p); /* Allocate and init shape related parameter space */

		/* Get the field indexes */
		if ((ci = icg->find_field(icg, 0, "PARAMETER")) < 0) {
			sprintf(p->err, "read_mpp: Input file '%s' doesn't contain field PARAMETER",
			        inname);
			icg->del(icg);
			return 1;
		}
		if (icg->t[0].ftype[ci] != nqcs_t) {
			sprintf(p->err, "read_mpp: Input file '%s' field PARAMETER is wrong type",
			        inname);
			icg->del(icg);
			return 1;
		}

		for (i = 0; i < 3; i++) {	/* XYZ fields */
			if ((spi[i] = icg->find_field(icg, 0, xyzfname[i])) < 0) {
				break;
			}
			if (icg->t[0].ftype[spi[i]] != r_t) {
				sprintf(p->err, "read_mpp: Input file '%s' field %s is wrong type",
				        inname, buf);
				icg->del(icg);
				return 1;
			}
		}

		if (i < 3) {
			islab = 1;
			for (i = 0; i < 3; i++) {	/* XYZ fields */
				if ((spi[i] = icg->find_field(icg, 0, labfname[i])) < 0) {
					sprintf(p->err, "read_mpp: Input file '%s' doesn't contain field %s or %s",
					        inname, xyzfname[i], labfname[i]);
					icg->del(icg);
					return 1;
				}
				if (icg->t[0].ftype[spi[i]] != r_t) {
					sprintf(p->err, "read_mpp: Input file '%s' field %s is wrong type",
					        inname, buf);
					icg->del(icg);
					return 1;
				}
			}
		}

		/* Find the fields for spectral values */
		if (p->spec_n > 0) {
			for (j = 0; j < p->spec_n; j++) {
				int nm;
		
				/* Compute nearest integer wavelength */
				nm = (int)(p->spec_wl_short + ((double)j/(p->spec_n-1.0))
				            * (p->spec_wl_long - p->spec_wl_short) + 0.5);
				
				sprintf(buf,"SPEC_%03d",nm);
	
				if ((spi[3+j] = icg->find_field(icg, 0, buf)) < 0) {
					sprintf(p->err, "read_mpp: Input file '%s' doesn't contain field %s",
					        buf,inname);
					icg->del(icg);
					return 1;
				}
				if (icg->t[0].ftype[spi[3+j]] != r_t) {
					sprintf(p->err, "read_mpp: Input file '%s' field %s is wrong type",
					        inname, buf);
					icg->del(icg);
					return 1;
				}
			}
		}

		/* Read the transfer curve values */
		for (i = 0; i < p->n; i++) {		/* each colorant */
			for (j = 0; j < p->cord; j++) {	/* curve order values */
				
				sprintf(buf,"t_%d_%d",i,j);
					
				/* Find the right parameter */
				for (ix = 0; ix < icg->t[0].nsets; ix++) {

					if (strcmp((char *)icg->t[0].fdata[ix][ci], buf) == 0) { 
						for (n = 0; n < (3+p->spec_n); n++)
							p->tc[i][n][j] = *((double *)icg->t[0].fdata[ix][spi[n]]);
						break;
					}
				}
			}
		}

		if (p->useshape) {

			/* Read the shaper values */
			for (i = 0; i < p->nnn2; i++) {	/* For all shaper values */
				int m = p->c2f[i].ink;
				int k = p->c2f[i].comb;

				sprintf(buf,"s_%d_%d",m, k);
						
				/* Find the right parameter */
				for (ix = 0; ix < icg->t[0].nsets; ix++) {

					if (strcmp((char *)icg->t[0].fdata[ix][ci], buf) == 0) { 
						for (n = 0; n < (3+p->spec_n); n++)
							p->shape[m][k][n] = *((double *)icg->t[0].fdata[ix][spi[n]]);

						break;
					}
				}
			}
		}

		/* Read the combination values */
		for (i = 0; i < p->nn; i++) {

			sprintf(buf,"c_%d",i);
					
			
			/* Find the right parameter */
			for (ix = 0; ix < icg->t[0].nsets; ix++) {

				if (strcmp((char *)icg->t[0].fdata[ix][ci], buf) == 0) { 
					for (n = 0; n < (3+p->spec_n); n++)
						p->pc[i][n] = *((double *)icg->t[0].fdata[ix][spi[n]]);
					if (islab) {
						double tt[3];
						tt[0] = p->pc[i][0], tt[1] = p->pc[i][1], tt[2] = p->pc[i][2];
						icmLab2XYZ(&icmD50, tt, tt);
						p->pc[i][0] = tt[0], p->pc[i][1] = tt[1], p->pc[i][2] = tt[2];
					}
#ifdef SHARPEN
					XYZ2sharp(&p->pc[i][0], &p->pc[i][1], &p->pc[i][2]);
#endif
					break;
				}
			}
		}
	}
	
	icg->del(icg);		/* Clean up */

	/* Figure out the white and black points */
	compute_wb(p);

	return 0;
}

/* Get various types of information about the mpp */
static void get_info(
mpp *p,					/* This */
inkmask *imask,			/* Inkmask, describing device colorspace */
int *nodchan,			/* Number of device channels */
double *limit,			/* Total ink limit (0.0 .. devchan) */
int *spec_n,			/* Number of spectral bands, 0 if none */
double *spec_wl_short,	/* First reading wavelength in nm (shortest) */
double *spec_wl_long,	/* Last reading wavelength in nm (longest) */
instType *itype,		/* Instrument type */
int *display			/* Return nz if display type */
) {
	if (imask != NULL)
		*imask = p->imask;
	if (nodchan != NULL)
		*nodchan = p->n;
	if (limit != NULL)
		*limit = p->limit;
	if (spec_n != NULL)
		*spec_n = p->spec_n;
	if (spec_wl_short != NULL)
		*spec_wl_short = p->spec_wl_short;
	if (spec_wl_long != NULL)
		*spec_wl_long = p->spec_wl_long;
	if (itype != NULL)
		*itype = p->itype;
	if (display != NULL)
		*display = p->display;
}

/* Set an illuminant and observer to use spectral model */
/* for CIE lookup with optional FWA. Set both to default for XYZ mpp model. */
/* return 0 on OK, 1 on spectral not supported, 2 on other error */ 
/* If the model is for a display, the illuminant will be ignored. */
static int set_ilob(
mpp *p,
icxIllumeType ilType,			/* Illuminant type (icxIT_default for none) */
xspect        *custIllum,		/* Custom illuminant (NULL for none) */
icxObserverType obType,			/* Observer type (icxOT_default for none) */	
xspect        custObserver[3],	/* Custom observer (NULL for none)  */
icColorSpaceSignature  rcs,		/* Return color space, icSigXYZData or icSigLabData */
int           use_fwa			/* NZ to involke FWA. */
) {

	/* Get rid of any existing conversion object */
	if (p->spc != NULL) {
		p->spc->del(p->spc);
		p->spc = NULL;
	}

	p->pcs = rcs;

	if (ilType == icxIT_default && obType == icxOT_default && use_fwa == 0)
		return 0;
		
	if (p->spec_n == 0) {
		p->errc = 1;
		sprintf(p->err,"No Spectral Data in MPP");
		return 1;
	}

	if (p->display) {
		ilType = icxIT_none;
		custIllum = NULL;
	}

	if ((p->spc = new_xsp2cie(ilType, custIllum, obType, custObserver, rcs, 1)) == NULL)
		error("mpp->set_ilob, new_xsp2cie failed");

	if (use_fwa) {
		int j;
		xspect white, inst;

		white.norm = p->norm; 
		white.spec_n = p->spec_n; 
		white.spec_wl_short = p->spec_wl_short; 
		white.spec_wl_long = p->spec_wl_long; 
		for (j = 0; j < p->spec_n; j++)
			white.spec[j] = p->white.band[3+j];
			
		if (inst_illuminant(&inst, p->itype) != 0)
			error ("mpp->set_ilob, instrument doesn't have an FWA illuminent");

		if (p->spc->set_fwa(p->spc, &inst, NULL, &white))
			error ("mpp->set_ilob, set_fwa faild");
	}

	return 0;
}

/* Lookup an XYZ or Lab color */
static void lookup(
mpp *p,						/* This */
double *out,				/* Returned XYZ or Lab */
double *in					/* Input device values */
) {
	if (p->spc == NULL) {
		double *Lab;
		double *XYZ;
	
		if (p->pcs == icSigLabData) {
			Lab = out;
			XYZ = NULL;
		} else {
			Lab = NULL;
			XYZ = out;
		}
	
		forward(p, NULL, Lab, XYZ, in);

		return;
	} else {
		xspect tspec;

		p->lookup_spec(p, &tspec, in);
		p->spc->convert(p->spc, out, &tspec);
	}
}

/* Lookup an XYZ or Lab color, plus the partial derivative. */
/* This is useful if the lookup is being used within */
/* a minimisation routine */
static void dlookup(
mpp *p,
double *out,		/* Return the XYZ or Lab */
double **dout,		/* Return the partial derivative dout[3][n] */
double *dev) {
	int i, j, k;

#ifdef SHARPEN
	/* Can't handle this without converting from sharpened derivative */
	/* to XYZ derivative. ~~~~~~ */
#endif
	/* We can't handle derivative using FWA, so */
	/* always compute from the XYZ model values. */
	for (j = 0; j < 3; j++) {	/* Compute each bands value */
		out[j] = dbandval(p, dout[j], j, dev);
	}
	if (p->pcs == icSigLabData) {
		double dlab[3][3];

		icxdXYZ2Lab(&icmD50, out, dlab, out);

		/* Apply Lab deriv to dout */
		for (i = 0; i < p->n; i++) {
			double tt[3];

			for (k = 0; k < 3; k++)
				tt[k] = dout[k][i];

			for (j = 0; j < 3; j++) {
				dout[j][i] = 0.0;

				for (k = 0; k < 3; k++) {
					dout[j][i] += dlab[j][k] * tt[k]; 
				}
			}
		}
	}
}


/* Get the white and black points */
static void get_wb(
mpp *p,						/* This */
double *white,
double *black,	
double *kblack				/* K only black */
) {
	if (white != NULL) {
		if (p->spc == NULL) {
			white[0] = p->white.band[0];
			white[1] = p->white.band[1];
			white[2] = p->white.band[2];
#ifdef SHARPEN
			sharp2XYZ(&white[0], &white[1], &white[2]);
#endif
			if (p->pcs == icSigLabData)
				icmXYZ2Lab(&icmD50, white, white);
		} else {
			int j;
			xspect ispect;
	
			ispect.norm = p->norm; 
			ispect.spec_n = p->spec_n; 
			ispect.spec_wl_short = p->spec_wl_short; 
			ispect.spec_wl_long = p->spec_wl_long; 
			for (j = 0; j < p->spec_n; j++)
				ispect.spec[j] = p->white.band[3+j];
			
			p->spc->convert(p->spc, white, &ispect);
		}
	}
	if (black != NULL) {
		if (p->spc == NULL) {
			black[0] = p->black.band[0];
			black[1] = p->black.band[1];
			black[2] = p->black.band[2];
#ifdef SHARPEN
			sharp2XYZ(&black[0], &black[1], &black[2]);
#endif
			if (p->pcs == icSigLabData)
				icmXYZ2Lab(&icmD50, black, black);
		} else {
			int j;
			xspect ispect;
	
			ispect.norm = p->norm; 
			ispect.spec_n = p->spec_n; 
			ispect.spec_wl_short = p->spec_wl_short; 
			ispect.spec_wl_long = p->spec_wl_long; 
			for (j = 0; j < p->spec_n; j++)
				ispect.spec[j] = p->black.band[3+j];
			
			p->spc->convert(p->spc, black, &ispect);
		}
	}

	if (kblack != NULL) {
		if (p->spc == NULL) {
			kblack[0] = p->kblack.band[0];
			kblack[1] = p->kblack.band[1];
			kblack[2] = p->kblack.band[2];
#ifdef SHARPEN
			sharp2XYZ(&kblack[0], &kblack[1], &kblack[2]);
#endif
			if (p->pcs == icSigLabData)
				icmXYZ2Lab(&icmD50, kblack, kblack);
		} else {
			int j;
			xspect ispect;
	
			ispect.norm = p->norm; 
			ispect.spec_n = p->spec_n; 
			ispect.spec_wl_short = p->spec_wl_short; 
			ispect.spec_wl_long = p->spec_wl_long; 
			for (j = 0; j < p->spec_n; j++)
				ispect.spec[j] = p->kblack.band[3+j];
			
			p->spc->convert(p->spc, kblack, &ispect);
		}
	}
}

/* Lookup an XYZ value. */
/* (Note that this is never FWA compensated) */
static void lookup_xyz(
mpp *p,						/* This */
double *out,				/* Returned XYZ value */
double *in					/* Input device values */
) {
	forward(p, NULL, NULL, out, in);
}

/* Lookup a spectral value. */
/* (Note that this is never FWA compensated) */
static void lookup_spec(
mpp *p,						/* This */
xspect *out,				/* Returned spectral value */
double *in					/* Input device values */
) {
	int j;

	out->norm = p->norm; 
	out->spec_n = p->spec_n; 
	out->spec_wl_short = p->spec_wl_short; 
	out->spec_wl_long = p->spec_wl_long; 

	forward(p, out->spec, NULL, NULL, in);

	for (j = 0; j < p->spec_n; j++)
		out->spec[j] *= out->norm;
}

/* Macros for an arbitrary dimensional counter */
/* Declare the counter name nn, dimensions di, & count */

#define DCOUNT(nn, di, start, reset, count) 				\
	int nn[MAX_CHAN];	/* counter value */						\
	int nn##_di = (di);		/* Number of dimensions */		\
	int nn##_stt = (start);	/* start count value */			\
	int nn##_rst = (reset);	/* reset on carry value */		\
	int nn##_res = (count);	/* last count +1 */				\
	int nn##_e				/* dimension index */

/* Set the counter value to 0 */
#define DC_INIT(nn) 								\
{													\
	for (nn##_e = 0; nn##_e < nn##_di; nn##_e++)	\
		nn[nn##_e] = nn##_stt;						\
	nn##_e = 0;										\
}

/* Increment the counter value */
#define DC_INC(nn)									\
{													\
	for (nn##_e = 0; nn##_e < nn##_di; nn##_e++) {	\
		nn[nn##_e]++;								\
		if (nn[nn##_e] < nn##_res)					\
			break;	/* No carry */					\
		nn[nn##_e] = nn##_rst;						\
	}												\
}

/* After increment, expression is TRUE if counter is done */
#define DC_DONE(nn)									\
	(nn##_e >= nn##_di)
	
/* Create a gamut */
/* (This will be in the current PCS) */
static gamut *mpp_gamut(
mpp *p,				/* This */
double       detail		/* gamut detail level, 0.0 = def */
) {
	gamut *gam;
	int res;		/* Sample point resolution */
	double white[3], black[3], kblack[3];
	DCOUNT(co, p->n, 0, 0, 2);

	if (detail == 0.0)
		detail = 10.0;

	gam = new_gamut(detail, 0, 0);		/* Lab only at the moment */

	/* Explore the gamut by itterating through */
	/* it with sample points in device space. */

	res = (int)(100.0/detail);	/* Establish an appropriate sampling density */

	if (res < 3)
		res = 3;

	DC_INIT(co);

	/* Itterate over all the 2 faces in the device space. */
	/* We're assuming in practice that only colors lying */
	/* on such faces contribute to the gamut volume. */
	/* The total number of faces explored will be */
	/* (dim * (dim-1))/2 * 2 ^ (dim-2) */
	/* It seems possible that a number of these faces could */
	/* be cheaply culled by examining the PCS corner values */
	/* for each 2^(dim-2) combinations, and skiping those. */
	/* that cannot possibly expand the gamut ? */
	/* ie. that they are clearly contained by other face combinations. */
	while(!DC_DONE(co)) {
		int e, m1, m2;
		double in[MAX_CHAN];
		double out[3];
		double sum;

		/* Check the ink limit */
		for (sum = 0.0, e = 0; e < p->n; e++)
			sum += (double)co[e];

		if (p->limit > 1e-4 && (sum - 1.0) > p->limit) {
			DC_INC(co);
			continue;		/* Skip points really over limit */
		}

		/* Scan only device surface */
		for (m1 = 0; m1 < p->n; m1++) {
			if (co[m1] != 0)
				continue;
			for (m2 = m1 + 1; m2 < p->n; m2++) {
				int x, y;

				if (co[m2] != 0)
					continue;

				for (e = 0; e < p->n; e++)
					in[e] = (double)co[e];		/* Base value */

				for (x = 0; x < res; x++) {				/* step over surface */
					in[m1] = x/(res - 1.0);
					for (y = 0; y < res; y++) {
						double ssum, iin[MAX_CHAN];
						in[m2] = y/(res - 1.0);
						ssum = sum + in[m1] + in[m2];
						if (p->limit > 1e-4 && (ssum - 1.0) > p->limit) {
							continue;
						}

						for (e = 0; e < p->n; e++)
							iin[e] = in[e];				/* Scalable copy */

						/* Apply ink limit by simple scaling */
						if (p->limit > 1e-4 && ssum > p->limit) {
							for (e = 0; e < p->n; e++)
								iin[e] *= p->limit/ssum;
						}
						
						forward(p, NULL, out, NULL, iin);
						gam->expand(gam, out);
					}
				}
			}
		}

		/* Increment index within block */
		DC_INC(co);
	}

	/* set the white and black points */
	p->get_wb(p, white, black, kblack);
	gam->setwb(gam, white, black, kblack);

	/* Set the cusp points */
	/* Do this by scanning just 0 & 100% colorant combinations */

	res = 2;
	gam->setcusps(gam, 0, NULL);
	DC_INIT(co);

	/* Itterate over all the faces in the device space */
	while(!DC_DONE(co)) {
		int e, m1, m2;
		double in[MAX_CHAN];
		double out[3];
		double sum;

		/* Check the ink limit */
		for (sum = 0.0, e = 0; e < p->n; e++)
			sum += (double)co[e];

		if (p->limit > 1e-4 && (sum - 1.0) > p->limit) {
			DC_INC(co);
			continue;		/* Skip points really over limit */
		}

		/* Scan only device surface */
		for (m1 = 0; m1 < p->n; m1++) {
			if (co[m1] != 0)
				continue;
			for (m2 = m1 + 1; m2 < p->n; m2++) {
				int x, y;

				if (co[m2] != 0)
					continue;

				for (e = 0; e < p->n; e++)
					in[e] = (double)co[e];		/* Base value */

				for (x = 0; x < res; x++) {				/* step over surface */
					in[m1] = x/(res - 1.0);
					for (y = 0; y < res; y++) {
						double ssum, iin[MAX_CHAN];
						in[m2] = y/(res - 1.0);
						ssum = sum + in[m1] + in[m2];
						if (p->limit > 1e-4 && (ssum - 1.0) > p->limit) {
							continue;
						}

						for (e = 0; e < p->n; e++)
							iin[e] = in[e];				/* Scalable copy */

						/* Apply ink limit by simple scaling */
						if (p->limit > 1e-4 && ssum > p->limit) {
							for (e = 0; e < p->n; e++)
								iin[e] *= p->limit/ssum;
						}
						
						forward(p, NULL, out, NULL, iin);
						gam->setcusps(gam, 1, out);
					}
				}
			}
		}

		/* Increment index within block */
		DC_INC(co);
	}
	gam->setcusps(gam, 2, NULL);

	return gam;
}

/* Delete it */
static void del_mpp(mpp *p) {
	if (p != NULL) {
		del_mppcol(&p->white, p->n, p->spec_n);	
		del_mppcol(&p->black, p->n, p->spec_n);	
		del_mppcol(&p->kblack, p->n, p->spec_n);	
		del_mppcols(p->cols, p->nodp, p->n, p->spec_n);	/* Delete array of target points */
		if (p->spc != NULL)
			p->spc->del(p->spc);

		/* Delete shape parameters */
		if (p->shape != NULL) {
			int i, j;
			for (j = 0; j < p->n; j++) {
				if (p->shape[j] != NULL) {
					for (i = 0; i < p->nn; i++) {
						if (p->shape[j][i] != NULL)
							free(p->shape[j][i]);
					}
					free(p->shape[j]);
				}
			}
			free(p->shape);
		}
		free(p);
	}
}

/* Allocate a new, uninitialised mpp */
/* Note thate black and white points aren't allocated */
mpp *new_mpp(void) {
	mpp *p;

	if ((p = (mpp *)calloc(1, sizeof(mpp))) == NULL)
		return NULL;

	p->pcs = icSigXYZData;

	/* Init method pointers */
	p->del         = del_mpp;
	p->create      = create;
	p->get_gamut   = mpp_gamut;
	p->write_mpp   = write_mpp;
	p->read_mpp    = read_mpp;
	p->get_info    = get_info;
	p->set_ilob    = set_ilob;
	p->get_wb      = get_wb;
	p->lookup      = lookup;
	p->dlookup     = dlookup;
	p->lookup_xyz  = lookup_xyz;
	p->lookup_spec = lookup_spec;

	return p;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Utility functions */

/* Allocate the data portion of an mppcol */
/* Return NZ if malloc error */
int new_mppcol(
mppcol *p,			/* mppcol to allocate */
int n,				/* Number of inks */
int nb				/* Number of spectral bands */
) {
	int nn = (1 << n);	/* Number of ink combinations */

	if ((p->nv = (double *)malloc(sizeof(double) * n)) == NULL) {
		del_mppcol(p, n, nb);
		return 1;
	}
	if ((p->band = (double *)malloc(sizeof(double) * (3 + nb))) == NULL) {
		del_mppcol(p, n, nb);
		return 1;
	}
	if ((p->lband = (double *)malloc(sizeof(double) * (3 + nb))) == NULL) {
		del_mppcol(p, n, nb);
		return 1;
	}
	if ((p->tcnv = (double *)calloc(n, sizeof(double))) == NULL) {
		del_mppcol(p, n, nb);
		return 1;
	}
	if ((p->scnv = (double *)calloc(n, sizeof(double))) == NULL) {
		del_mppcol(p, n, nb);
		return 1;
	}
	if ((p->pcnv = (double *)malloc(nn * sizeof(double))) == NULL) {
		del_mppcol(p, n, nb);
		return 1;
	}
	if ((p->fcnv = (double *)malloc(n * nn/2 * sizeof(double))) == NULL) {
		del_mppcol(p, n, nb);
		return 1;
	}
	return 0;
}

/* Copy the contents of one mppcol to another */
void copy_mppcol(
mppcol *d,			/* Destination */
mppcol *s,			/* Source */
int n,				/* Number of inks */
int nb				/* Number of spectral bands */
) {
	mppcol al;
	int nn = (1 << n);	/* Number of ink combinations */
	int i, j;

	al = *d;		/* Copy destinations allocations */
	*d = *s;		/* Copy source contents & pointers */

	/* Now fixup allocation addresses  */
	d->nv = al.nv;
	d->band = al.band;
	d->lband = al.lband;
	d->tcnv = al.tcnv;
	d->scnv = al.scnv;
	d->pcnv = al.pcnv;
	d->fcnv = al.fcnv;

	/* Copy contents of allocated data */
	for (j = 0; j < n; j++)
		d->nv[j] = s->nv[j];

	for (j = 0; j < (3+nb); j++)
		d->band[j] = s->band[j];

	for (j = 0; j < (3+nb); j++)
		d->lband[j] = s->lband[j];

	for (i = 0; i < n; i++)
		d->tcnv[i] = s->tcnv[i];

	for (i = 0; i < n; i++)
		d->scnv[i] = s->scnv[i];

	for (i = 0; i < nn; i++)
		d->pcnv[i] = s->pcnv[i];

	for (i = 0; i < (n * nn/2); i++)
		d->fcnv[i] = s->fcnv[i];
}


/* Free the data allocation of an mppcol */
void del_mppcol(
mppcol *p,
int n,				/* Number of inks */
int nb				/* Number of spectral bands */
) {
	if (p != NULL) {
		if (p->nv != NULL)
			free (p->nv);

		if (p->band != NULL)
			free (p->band);

		if (p->lband != NULL)
			free (p->lband);

		if (p->tcnv != NULL)
			free (p->tcnv);

		if (p->scnv != NULL)
			free (p->scnv);

		if (p->pcnv != NULL)
			free (p->pcnv);

		if (p->fcnv != NULL)
			free (p->fcnv);
	}
}

/* Allocate an array of mppcol */
/* Return NULL if malloc error */
mppcol *new_mppcols(
int no,				/* Number in array */
int n,				/* Number of inks */
int nb				/* Number of spectral bands */
) {
	mppcol *p;
	int i;

	if ((p = (mppcol *)calloc(no, sizeof(mppcol))) == NULL) {
		return NULL;
	}

	for (i = 0; i < no; i++) {
		if (new_mppcol(&p[i], n, nb) != 0) {
			del_mppcols(p, no, n, nb);
			return NULL;
		}
	}
	
	return p;
}

/* Free an array of mppcol */
void del_mppcols(
mppcol *p,
int no,				/* Number in array */
int n,				/* Number of inks */
int nb				/* Number of spectral bands */
) {
	if (p != NULL) {
		int i;
	
		for (i = 0; i < no; i++)
			del_mppcol(&p[i], n, nb);
		free (p);
	}
}

/* Allocate and setup the extra shape parameters combinations */
static void init_shape(mpp *p) {
	int i, j, k;
	int ix[MPP_MXINKS];

	/* First allocate the shape parameter array */
	
	if ((p->shape = (double ***)malloc(p->n * sizeof(double **))) == NULL)
		error("Malloc failed (mpp shape)!");
	for (j = 0; j < p->n; j++) {
		if ((p->shape[j] = (double **)malloc(p->nn * sizeof(double *))) == NULL)
			error("Malloc failed (mpp shape)!");
		for (i = 0; i < p->nn; i++) {
			if ((i & (1 << j)) == 0) {		/* Valid combo for this ink */
				if ((p->shape[j][i] = (double *)malloc((3+p->spec_n) * sizeof(double))) == NULL)
					error("Malloc failed (mpp shape)!");
				for (k = 0; k < (3+p->spec_n); k++)
					p->shape[j][i][k] = 0.0;	/* Initial shape value */
			} else {
				p->shape[j][i] = NULL;
			}
		}
	}
	
	/* Setup sparse to full and back indexing lookup */
	for (j = 0; j < p->n; j++)
		ix[j] = 0;

	for (i = 0; i < p->nn; i++) {
		for (j = 0; j < p->n; j++) {
			p->f2c[j][i] = j * p->nn/2 + ix[j]; 
			if ((i & (1 << j)) == 0) {
				p->c2f[j * p->nn/2 + ix[j]].ink = j;
				p->c2f[j * p->nn/2 + ix[j]].comb = i;
				ix[j]++;
			}
		}
	}

#ifdef NEVER
	/* Print result */
	for (j = 0; j < p->n; j++) {
		for (i = 0; i < p->nn; i++) {
			printf("f2c[%d][%d] = %d\n", j,i,p->f2c[j][i]);
		}
		for (i = 0; i < p->nn/2; i++) {
			printf("c2f[%d].ink,comb = %d, %d\n",
			j * p->nn/2 + i,p->c2f[j * p->nn/2 + i].ink,
			p->c2f[j * p->nn/2 + i].comb);
		}
	}
#endif /* NEVER */
}



/* - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Creation helper functions */

/* Per band error metric */

/* Convert argument to L* like scale */
#define lDE(aa) ((aa) > 0.008856451586 ? 116.0 * pow((aa),1.0/3.0) - 16.0 : (aa) * 903.2962896)

/* Function to approximate visible difference in an XYZ or spectral difference */
static double DEsq(double aa, double bb) {
	double dd;

	/* Convert input to L* like value */
	aa = lDE(aa);
	bb = lDE(bb);

	dd = aa - bb;

	return dd * dd;
}

/* Convert argument to the derivative of the DEsq function at that value */
#define dDE(aa) ((aa) > 0.008856451586 ? 38.666667 * pow((aa), -2.0/3.0) : 903.2962896)


/* Given a device value, return the given bands value */
/* according to the current model. */
static double bandval(mpp *p, int band, double *dev) {
	double tcnv[MPP_MXINKS];	/* Transfer curve corrected device values */
	double tcnv1[MPP_MXINKS];	/* 1.0 - Transfer curve corrected device values */
	double ww[MPP_MXINKS];		/* Interpolated tweak params for each channel */
	int m, k;
	double ov;
	int j = band;

	/* Compute the tranfer corrected device values */
	for (m = 0; m < p->n; m++) {
		tcnv[m] = icxTransFunc(p->tc[m][j], p->cord, dev[m]);
		tcnv1[m] = 1.0 - tcnv[m];
	}

	if (p->useshape) {

		for (m = 0; m < p->n; m++)
			ww[m] = 0.0;

		/* Lookup the shape values */
		for (k = 0; k < p->nn; k++) {		/* For each interp vertex */
			double vv;
	 		for (vv = 1.0, m = 0; m < p->n; m++) {	/* Compute weighting */
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			for (m = 0; m < p->n; m++) {
				ww[m] += p->shape[m][k & ~(1<<m)][j] * vv;
									/* Apply weighting to shape vertex value */
			}
		}

		/* Apply the shape values to adjust the primaries */
		for (m = 0; m < p->n; m++) {
			double gg = ww[m];			/* Curve adjustment */
			double vv = tcnv[m];		/* Input value to be tweaked */
			if (gg >= 0.0) {
				vv = vv/(gg - gg * vv + 1.0);
			} else {
				vv = (vv - gg * vv)/(1.0 - gg * vv);
			}
			tcnv[m] = vv;
			tcnv1[m] = 1.0 - vv;
		}
	}

	/* Compute the primary combination values */
	for (ov = 0.0, k = 0; k < p->nn; k++) {
		double vv = p->pc[k][j];
		for (m = 0; m < p->n; m++) {
			if (k & (1 << m))
				vv *= tcnv[m];
			else
				vv *= tcnv1[m];
		}
		ov += vv;
	}

	return ov;
}

/* Given a device value, return the given bands value */
/* according to the current model, as well as the partial */
/* derivative wrt the device values. */
static double dbandval(mpp *p, double *dv, int band, double *dev) {

	double dtcnv_ddev[MPP_MXINKS];				/* Del in tcnv[m] due to del dev[m] */
	double dww_dtcnv[MPP_MXINKS][MPP_MXINKS];	/* Del in ww[m] due to del in tcnv[m] */
	double dtcnv_tc[MPP_MXINKS];				/* Del in tcnv'[m] due to del in tcnv[m] */
	double dtcnv_ww[MPP_MXINKS];				/* Del in tcnv'[m] due to del in ww[m] */
	double dov[MPP_MXINKS];						/* Del of ov due to del in tcnv'[m] */

	double tcnv[MPP_MXINKS];	/* Transfer curve corrected device values */
	double tcnv1[MPP_MXINKS];	/* 1.0 - Transfer curve corrected device values */
	double ww[MPP_MXINKS];		/* Interpolated tweak params for each channel */
	int m, k;
	double ov;
	int j = band;

	/* Compute the tranfer corrected device values */
	for (m = 0; m < p->n; m++) {
		tcnv[m] = icxdiTransFunc(p->tc[m][j], &dtcnv_ddev[m], p->cord, dev[m]);
		tcnv1[m] = 1.0 - tcnv[m];
		ww[m] = 0.0;
	}

	/* Lookup the shape values */
	for (k = 0; k < p->nn; k++) {		/* For each interp vertex */
		double vv;
 		for (vv = 1.0, m = 0; m < p->n; m++) {	/* Compute weighting */
			if (k & (1 << m))
				vv *= tcnv[m];
			else
				vv *= tcnv1[m];
		}
		for (m = 0; m < p->n; m++) {
			ww[m] += p->shape[m][k & ~(1<<m)][j] * vv;
								/* Apply weighting to shape vertex value */
		}
	}

	/* Compute del ww[m][m] for del tcnv[m] */
	for (m = 0; m < p->n; m++) {			/* For each input channel were doing del of */
		int mo;
		for (mo = 0; mo < p->n; mo++)
			dww_dtcnv[mo][m] = 0.0;
		for (k = 0; k < p->nn; k++) {
			int mm;
			double vv = 1.0;
			for (mm = 0; mm < p->n; mm++) {	/* Compute weight for node k */
				if (m == mm)
					continue;
				if (k & (1 << mm))
					vv *= tcnv[mm];
				else
					vv *= tcnv1[mm];
			}
			for (mo = 0; mo < p->n; mo++) {				/* For each output channel */
				double vvv = vv * p->shape[mo][k & ~(1<<mo)][j];
				if (k & (1 << m))
					dww_dtcnv[mo][m] += vvv;
				else
					dww_dtcnv[mo][m] -= vvv;
			}
		}
	}

	/* Apply the shape values to adjust the primaries */
	for (m = 0; m < p->n; m++) {
		double tt;
		double gg = ww[m];				/* Curve adjustment */
		double sv, dsv, vv = tcnv[m];		/* Input value to be tweaked */
		if (gg >= 0.0) {
			tt = gg - gg * vv + 1.0;
			sv = vv/tt;
			dsv = (gg + 1.0)/(tt * tt);
		} else {
			tt = 1.0 - gg * vv;
			sv = (vv - gg * vv)/tt;
			dsv = (1.0 - gg)/(tt * tt);
		}
		tcnv[m] = sv;
		tcnv1[m] = 1.0 - sv;
		dtcnv_tc[m] = dsv;						/* del in tcnv[m] due to del in tcnv[m] */
		dtcnv_ww[m] = (vv * vv - vv)/(tt * tt);	/* del in tcnv[m] due to del in ww[m] */
	}

	/* Compute the primary combination values */
	for (ov = 0.0, k = 0; k < p->nn; k++) {
		double vv = p->pc[k][j];
 		for (m = 0; m < p->n; m++) {
			if (k & (1 << m))
				vv *= tcnv[m];
			else
				vv *= tcnv1[m];
		}
		ov += vv;
	}

	/* Compute del ov for del tcnv[m] */
	for (m = 0; m < p->n; m++) {
		for (dov[m] = 0.0, k = 0; k < p->nn; k++) {
			int mm;
			double vv = p->pc[k][j];
			for (mm = 0; mm < p->n; mm++) {
				if (m == mm)
					continue;
				if (k & (1 << mm))
					vv *= tcnv[mm];
				else
					vv *= tcnv1[mm];
			}
			if (k & (1 << m))
				dov[m] += vv;
			else
				dov[m] -= vv;
		}
	}

	/* Accumulate delta from input value to output */
	for (m = 0;  m < p->n; m++) {
		double ttt;
		int mm;

		/* delta via dww */
		for (ttt = 0.0, mm = 0; mm < p->n; mm++)
			ttt += dov[mm] * dtcnv_ww[mm] * dww_dtcnv[mm][m] * dtcnv_ddev[m];

		/* delta direct */
		dv[m] = ttt + dov[m] * dtcnv_tc[m] * dtcnv_ddev[m];
	}

	return ov;
}

/* Given a device value, return the XYZ, Lab and spectral */
/* values according to the current model. */
/* Pointer may be NULL if result is not needed */
static void forward(mpp *p, double *spec, double *Lab, double *XYZ, double *dev) {
	double tXYZ[3];
	int sb = 3, eb = 3;			/* Start and end bands to compute */
	int j;

	if (XYZ != NULL || Lab != NULL)
		sb = 0;
	if (spec != NULL)
		eb = 3 + p->spec_n;

	for (j = sb; j < eb; j++) {	/* Compute each bands value */
		double ov;

		ov = bandval(p, j, dev);

		if (j < 3)
			tXYZ[j] = ov;
		else
			spec[j-3] = ov;
	}

#ifdef SHARPEN
	if (sb == 0)
		sharp2XYZ(&tXYZ[0], &tXYZ[1], &tXYZ[2]);
#endif
	if (XYZ != NULL) {
		XYZ[0] = tXYZ[0];
		XYZ[1] = tXYZ[1];
		XYZ[2] = tXYZ[2];
	}
	if (Lab != NULL) {
		icmXYZ2Lab(&icmD50, Lab, tXYZ); /* Convert D50 Lab */
	}
}

/* Return the specified bands current average error */
static void banderr(
mpp *p,
double *avese,		/* Return average Error from spectral bands */
double *maxse,		/* Return maximum Error from spectral bands */
int band
) {
	double serr = 0.0, smax = 0.0;
	int i;

	/* For each test point */
	for (i = 0; i < p->nodp; i++) {
		double spv;
		mppcol *c = &p->cols[i];

		/* Compute models output */
		spv = bandval(p, band, c->nv);
		spv = sqrt(DEsq(c->band[band], spv));

		if (spv > smax)
			smax = spv;
		serr += spv;
	}

	if (avese != NULL)
		*avese = serr/((double)p->nodp);
	if (maxse != NULL)
		*maxse = smax;
}

/* Figure out what the current model errors are and return them */
static void deltae(
mpp *p,
double *avede,		/* Return average Delta E from XYZ bands */
double *maxde,		/* Return maximum Delta E from XYZ bands */
double *avese,		/* Return average Error from spectral bands */
double *maxse		/* Return maximum Error from spectral bands */
) {
	double spec[MPP_MXBANDS];	/* spectral value for each point */
	double Lab[3];
	double err = 0.0, max = 0.0;	/* Delta E */
	double serr = 0.0, smax = 0.0;	/* Delta S */
	int i, j;

	/* For each test point */
	for (i = 0; i < p->nodp; i++) {
		double ee;
		mppcol *c = &p->cols[i];

		/* Compute models output */
		forward(p, p->spec_n > 0 ? spec : NULL, Lab, c->cXYZ, c->nv);

		/* Track the Delta E */
		c->err = icmLabDEsq(Lab, c->Lab);
		ee = sqrt(c->err);
		if (ee > max)
			max = ee;
		err += ee;

		/* Track the spectral error */
		for (j = 0; j < p->spec_n; j++) {
			ee = DEsq(c->band[3+j], spec[j]);
			ee = sqrt(ee);
			if (ee > smax)
				smax = ee;
			serr += ee;
		}
	}

	if (avede != NULL)
		*avede = err/((double)p->nodp);
	if (maxde != NULL)
		*maxde = max;

	if (p->spec_n > 0 && avese != NULL)
		*avese = serr/(((double)p->nodp) * ((double)p->spec_n));
	if (maxse != NULL)
		*maxse = smax;
}

/* - - - - - - - - - - - - - - - */
/* Powell optimisation callbacks */

/* Progress reporter for mpp powell/conjgrad  */
static void mppprog(void *pdata, int perc) {
	mpp *p = (mpp *)pdata;

	if (p->verb) {
		printf("%c% 3d%%",cr_char,perc); 
		if (perc == 100)
			printf("\n");
		fflush(stdout);
	}
}

#ifdef NEVER		// Skip efunc1 passes for now.

/* Setup test point data ready for efunc1 on the given och and oba */
static void sfunc1(mpp *p) {
	double tcnv[MPP_MXINKS];	/* Transfer curve corrected device values */
	double tcnv1[MPP_MXINKS];	/* 1.0 - Transfer curve corrected device values */
	int k = p->och;			/* Channel being optimised */
	int j = p->oba;			/* Band being optimised */
	int i, m;

	/* Pre-compute combination weighting and colorant combination values */
	/* for this band, without this device channels contribution. */
	for (i = 0; i < p->nodp; i++) {
		mppcol *c = &p->cols[i];
		int kk;

		/* Compute the tranfer corrected device values */
		for (m = 0; m < p->n; m++) {
			tcnv[m] = icxTransFunc(p->tc[m][j], p->cord, c->nv[m]);
			tcnv1[m] = 1.0 - tcnv[m];
		}

		/* Compute the test point primary combination values */
		c->tpcnv = c->tpcnv1 = 0.0;
		for (kk = 0; kk < p->nn; kk++) {		/* Combination value */
			double vv;
			for (vv = 1.0, m = 0; m < p->n; m++) {
				if (m == k)
					continue;			/* Multiply this in efunc1 */
				if (kk & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			if (kk & (1 << k))
				c->tpcnv += vv * p->pc[kk][j];
			else
				c->tpcnv1 += vv * p->pc[kk][j];
		}
	}
}

#endif /* NEVER */

#ifdef NEVER		// Skip efunc1 passes for now.
/* Optimise a device transfer curve to minimise a particular bands error */
/* (Assume no shape parameters) */
static double efunc1(void *adata, double pv[]) {
	mpp *p = (mpp *)adata;
	double smv, rv = 0.0;
	double tcnv;				/* Transfer curve corrected device values */
	int k = p->och;			/* Channel being optimised */
	int j = p->oba;			/* Band being optimised */
	int pc, m, i;

	/* For each test point */
	for (pc = m = 0; m < p->nodp; m++) {
		mppcol *c = &p->cols[m];
		double vv;

		if (c->w < 1e-6)
			continue;		/* Skip zero weighted points */

		/* Compute the tranfer corrected device values */
		tcnv = icxTransFunc(pv, p->cord, c->nv[k]);

		/* Compute band value */
		vv = tcnv * c->tpcnv + (1.0 - tcnv) * c->tpcnv1;

		/* Compute the band value error */
		vv = lDE(vv) - c->lband[j];
		rv += c->w * vv * vv;
		pc++;
	}
	rv /= (double)pc;

	/* Compute average magnitude of shaper parameters squared */
	/* to minimise unconstrained "wiggles" */
	for (smv = 0.0, m = 0; m < p->cord; m++) {
		smv += pv[m] * pv[m];
	}
	smv /= (double)(p->cord);
	rv += 0.01 * smv;

#ifdef DEBUG
	printf("efunc1 itt %d/%d chan %d band %d k0 %f returning %f\n",p->oit,p->ott,k,j,pv[0],rv);
#endif
	return rv;
}
#endif /* NEVER */

/* Optimise all transfer curves simultaniously to minimise a particular bands error */
static double efunc2(void *adata, double pv[]) {
	mpp *p = (mpp *)adata;
	double smv, rv = 0.0;
	double tcnv[MPP_MXINKS];	/* Transfer curve corrected device values */
	double tcnv1[MPP_MXINKS];	/* 1.0 - Transfer curve corrected device values */
	double ww[MPP_MXINKS];		/* Interpolated tweak params for each channel */
	int j = p->oba;				/* Band being optimised */
	int i, m, k;

	/* For each test point */
	for (i = 0; i < p->nodp; i++) {
		mppcol *c = &p->cols[i];
		double ov;

		/* Compute the tranfer corrected device values */
		for (m = 0; m < p->n; m++) {
			tcnv[m] = icxTransFunc(&pv[m * p->cord], p->cord, c->nv[m]);
			tcnv1[m] = 1.0 - tcnv[m];
		}

		if (p->useshape) {

			for (m = 0; m < p->n; m++)
				ww[m] = 0.0;

			/* Lookup the shape values */
			for (k = 0; k < p->nn; k++) {		/* For each interp vertex */
				double vv;
		 		for (vv = 1.0, m = 0; m < p->n; m++) {	/* Compute weighting */
					if (k & (1 << m))
						vv *= tcnv[m];
					else
						vv *= tcnv1[m];
				}
				for (m = 0; m < p->n; m++) {
					ww[m] += p->shape[m][k & ~(1<<m)][j] * vv;
										/* Apply weighting to shape vertex value */
				}
			}

			/* Apply the shape values to adjust the primaries */
			for (m = 0; m < p->n; m++) {
				double gg = ww[m];			/* Curve adjustment */
				double vv = tcnv[m];		/* Input value to be tweaked */
				if (gg >= 0.0) {
					vv = vv/(gg - gg * vv + 1.0);
				} else {
					vv = (vv - gg * vv)/(1.0 - gg * vv);
				}
				tcnv[m] = vv;
				tcnv1[m] = 1.0 - vv;
			}
		}

		/* Compute the primary combination values */
		for (ov = 0.0, k = 0; k < p->nn; k++) {
			double vv = p->pc[k][j];
	 		for (m = 0; m < p->n; m++) {
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			ov += vv;
		}

		/* Compute the band value error */
		ov = lDE(ov) - c->lband[j];
		rv += ov * ov;
	}

	rv /= (double)p->nodp;

	/* Compute weighted magnitude of shaper parameters squared */
	/* to minimise unconstrained "wiggles" */
	for (smv = 0.0, m = 0; m < p->n; m++) {
		double w;
		for (k = 0; k < p->cord; k++) {
			i = m * p->cord + k;

#ifdef NEVER
			w = (k < 2) ? TRANS_BASE : k * TRANS_HBASE; 	/* Increase weight with harmonics */
#else
			/* Weigh to suppress ripples */
			if (k <= 1) {						/* Use TRANS_HW01 */
				w = TRANS_HW01;
			} else if (k <= TRANS_HBREAK) {	/* Blend from TRANS_HW01 to TRANS_HWBR */
				double bl = (k - 1.0)/(TRANS_HBREAK - 1.0);
				w = (1.0 - bl) * TRANS_HW01 + bl * TRANS_HWBR;
			} else {				/* Use TRANS_HWBR */
				w = TRANS_HWBR + (k-TRANS_HBREAK) * TRANS_HWINC;
			}
#endif
			smv += w * pv[i] * pv[i];
		}
	}
	smv /= (double)(p->n);
	rv += smv;

#ifdef DEBUG
	printf("efunc2 itt %d/%d band %d returning %f\n",p->oit,p->ott,j,rv);
#endif
	return rv;
}

/* Return the gradient of the minimisation function at the given location, */
/* as well as the function value at this location. */
static double dfunc2(void *adata, double dv[], double pv[]) {
	mpp *p = (mpp *)adata;
	double smv, tt, rv = 0.0;
	double dtcnv_dpv[MPP_MXINKS][MPP_MXTCORD];	/* Del in tcnv[m] due to del in parameter */
	double dww_dtcnv[MPP_MXINKS][MPP_MXINKS];	/* Del in ww[m] due to del in tcnv[m] */
	double dtcnv_tc[MPP_MXINKS];				/* Del in tcnv'[m] due to del in tcnv[m] */
	double dtcnv_ww[MPP_MXINKS];				/* Del in tcnv'[m] due to del in ww[m] */
	double dov[MPP_MXINKS];						/* Del of ov due to del in tcnv'[m] */
	double ddov; 								/* Del in final ov due to del in raw ov */
	double tcnv[MPP_MXINKS];	/* Transfer curve corrected device values */
	double tcnv1[MPP_MXINKS];	/* 1.0 - Transfer curve corrected device values */
	double ww[MPP_MXINKS];		/* Interpolated tweak params for each channel */
	int j = p->oba;			/* Band being optimised */
	int i, m, k;

	for (k = 0; k < (p->n * p->cord); k++)
		dv[k] = 0.0;

	/* For each test point */
	for (i = 0; i < p->nodp; i++) {
		int mo;
		mppcol *c = &p->cols[i];
		double ov;

		/* Compute the tranfer corrected device values */
		/* and del in these values due to del in input parameters */
		for (m = 0; m < p->n; m++) {
			tcnv[m] = icxdpTransFunc(&pv[m * p->cord], dtcnv_dpv[m], p->cord, c->nv[m]);
			tcnv1[m] = 1.0 - tcnv[m];
		}

#ifdef NEVER
{
for (m = 0; m < p->n; m++) {
	int ii;
	double ttt;

	for (ii = 0; ii < p->cord; ii++) {
		pv[m * p->cord + ii] += 1e-6;
		ttt = (icxTransFunc(&pv[m * p->cord], p->cord, c->nv[m]) - tcnv[m])/1e-6;
		pv[m * p->cord + ii] -= 1e-6;
		printf("~1 chan %d cord %d is %f ref %f\n",m,ii,dtcnv_dpv[m][ii],ttt);
	}
}
}
#endif
		for (m = 0; m < p->n; m++)
			ww[m] = 0.0;

		/* Lookup the shape values */
		for (k = 0; k < p->nn; k++) {		/* For each interp vertex */
			double vv;
	 		for (vv = 1.0, m = 0; m < p->n; m++) {	/* Compute weighting */
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			for (m = 0; m < p->n; m++) {
				ww[m] += p->shape[m][k & ~(1<<m)][j] * vv;
									/* Apply weighting to shape vertex value */
			}
		}

		/* Compute del ww[m][m] for del tcnv[m] */
		for (m = 0; m < p->n; m++) {			/* For each input channel were doing del of */
			for (mo = 0; mo < p->n; mo++)
				dww_dtcnv[mo][m] = 0.0;
			for (k = 0; k < p->nn; k++) {
				int mm;
				double vv = 1.0;
				for (mm = 0; mm < p->n; mm++) {	/* Compute weight for node k */
					if (m == mm)
						continue;
					if (k & (1 << mm))
						vv *= tcnv[mm];
					else
						vv *= tcnv1[mm];
				}
				for (mo = 0; mo < p->n; mo++) {				/* For each output channel */
					double vvv = vv * p->shape[mo][k & ~(1<<mo)][j];
					if (k & (1 << m))
						dww_dtcnv[mo][m] += vvv;
					else
						dww_dtcnv[mo][m] -= vvv;
				}
			}
		}

#ifdef NEVER
{
	int mm;
	double tww[MPP_MXINKS][MPP_MXINKS];

	for (mm = 0; mm < p->n; mm++) {		/* For del in each input value */
		tcnv[mm] += 1e-6;

		for (m = 0; m < p->n; m++)
			tww[m][mm] = 0.0;

		/* Lookup the shape values */
		for (k = 0; k < p->nn; k++) {		/* For each interp vertex */
			double vv;
	 		for (vv = 1.0, m = 0; m < p->n; m++) {	/* Compute weighting */
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= (1.0 - tcnv[m]);
			}
			for (m = 0; m < p->n; m++) {
				tww[m][mm] += p->shape[m][k & ~(1<<m)][j] * vv;
									/* Apply weighting to shape vertex value */
			}
		}

		tcnv[mm] -= 1e-6;
		for (m = 0; m < p->n; m++) {
			tww[m][mm] = (tww[m][mm] - ww[m])/1e-6;
		}
	}

	for (mm = 0; mm < p->n; mm++) {
		for (m = 0; m < p->n; m++) {
			printf("~1 ww[%d][%d] %f should be %f\n",mm, m, dww_dtcnv[mm][m],tww[mm][m]);
		}
	}
}
#endif /* NEVER */

#ifdef NEVER
{
	double itc[MPP_MXINKS];
	double ttc[MPP_MXINKS];
	double tww[MPP_MXINKS];

	for (m = 0; m < p->n; m++) {
		itc[m] = tcnv[m];
	}
#endif /* NEVER */


		/* Apply the shape values to adjust the primaries */
		for (m = 0; m < p->n; m++) {
			double gg = ww[m];				/* Curve adjustment */
			double sv, dsv, vv = tcnv[m];		/* Input value to be tweaked */
			if (gg >= 0.0) {
				tt = gg - gg * vv + 1.0;
				sv = vv/tt;
				dsv = (gg + 1.0)/(tt * tt);
			} else {
				tt = 1.0 - gg * vv;
				sv = (vv - gg * vv)/tt;
				dsv = (1.0 - gg)/(tt * tt);
			}
			tcnv[m] = sv;
			tcnv1[m] = 1.0 - sv;
			dtcnv_tc[m] = dsv;						/* del in tcnv[m] due to del in tcnv[m] */
			dtcnv_ww[m] = (vv * vv - vv)/(tt * tt);	/* del in tcnv[m] due to del in ww[m] */
		}


#ifdef NEVER
	/* Check derivatives */
	for (m = 0; m < p->n; m++) {
		double gg = ww[m];			/* Curve adjustment */
		double vv = itc[m];		/* Input value to be tweaked */

		vv += 1e-6;
		if (gg >= 0.0) {
			vv = vv/(gg - gg * vv + 1.0);
		} else {
			vv = (vv - gg * vv)/(1.0 - gg * vv);
		}
		ttc[m] = (vv - tcnv[m])/1e-6;
	}

	for (m = 0; m < p->n; m++) {
		double gg = ww[m];			/* Curve adjustment */
		double vv = itc[m];		/* Input value to be tweaked */

		gg += 1e-6;
		if (gg >= 0.0) {
			vv = vv/(gg - gg * vv + 1.0);
		} else {
			vv = (vv - gg * vv)/(1.0 - gg * vv);
		}
		tww[m] = (vv - tcnv[m])/1e-6;
	}


	for (m = 0; m < p->n; m++) {
		printf("~1 dtcnv_tc[%d] is %f should be %f\n",m, dtcnv_tc[m], ttc[m]);
	}
	for (m = 0; m < p->n; m++) {
		printf("~1 dtcnv_ww[%d] is %f should be %f\n",m, dtcnv_ww[m], tww[m]);
	}
}
#endif /* NEVER */

		/* Compute the primary combination values */
		for (ov = 0.0, k = 0; k < p->nn; k++) {
			double vv = p->pc[k][j];
	 		for (m = 0; m < p->n; m++) {
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			ov += vv;
		}

		/* Compute del ov for del tcnv[m] */
		for (m = 0; m < p->n; m++) {
			for (dov[m] = 0.0, k = 0; k < p->nn; k++) {
				int mm;
				double vv = p->pc[k][j];
				for (mm = 0; mm < p->n; mm++) {
					if (m == mm)
						continue;
					if (k & (1 << mm))
						vv *= tcnv[mm];
					else
						vv *= tcnv1[mm];
				}
				if (k & (1 << m))
					dov[m] += vv;
				else
					dov[m] -= vv;
			}
		}

#ifdef NEVER
{
	int mm;
	double tdov[MPP_MXINKS];

	for (mm = 0; mm < p->n; mm++) {
		tcnv[mm] += 1e-6;

		for (tdov[mm] = 0.0, k = 0; k < p->nn; k++) {
			double vv = p->pc[k][j];
	 		for (m = 0; m < p->n; m++) {
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= (1.0 - tcnv[m]);
			}
			tdov[mm] += vv;
		}
		tcnv[mm] -= 1e-6;
		tdov[mm] = (tdov[mm] - ov)/1e-6;
	}

	for (m = 0; m < p->n; m++) {
		printf("~1 dov[%d] is %f should be %f\n",m, tdov[m], dov[m]);
	}
}
#endif /* NEVER */

		/* Compute the band value error */
		ddov = dDE(ov);
		ov = lDE(ov) - c->lband[j];
	
		ddov *= 2.0 * ov; 	/* del in final ov due to del in raw ov */
		rv += ov * ov;
	
		/* Accumulate delta from input params to output */
		for (m = 0;  m < p->n; m++) {
			for (k = 0; k < p->cord; k++) {
				double ttt;
				int mm;
	
				/* delta via dww */
				for (ttt = 0.0, mm = 0; mm < p->n; mm++)
					ttt += dov[mm] * dtcnv_ww[mm] * dww_dtcnv[mm][m] * dtcnv_dpv[m][k];
	
				/* delta direct */
				dv[m * p->cord + k] += ddov * (ttt + dov[m] * dtcnv_tc[m] * dtcnv_dpv[m][k]);
			}
		}
	}

	rv /= (double)p->nodp;
	for (k = 0; k < (p->n * p->cord); k++)
		dv[k] /= (double)p->nodp;

	/* Compute weighted magnitude of shaper parameters squared */
	/* to minimise unconstrained "wiggles" */
	tt = 2.0/(double)(p->n);		/* Common factor */
	for (smv = 0.0, m = 0; m < p->n; m++) {
		double w;
		for (k = 0; k < p->cord; k++) {
			i = m * p->cord + k;
#ifdef NEVER
			w = (k < 2) ? TRANS_BASE : k * TRANS_HBASE; 	/* Increase weight with harmonics */
#else
			/* Weigh to suppress ripples */
			if (k <= 1) {						/* Use TRANS_HW01 */
				w = TRANS_HW01;
			} else if (k <= TRANS_HBREAK) {	/* Blend from TRANS_HW01 to TRANS_HWBR */
				double bl = (k - 1.0)/(TRANS_HBREAK - 1.0);
				w = (1.0 - bl) * TRANS_HW01 + bl * TRANS_HWBR;
			} else {				/* Use TRANS_HWBR */
				w = TRANS_HWBR + (k-TRANS_HBREAK) * TRANS_HWINC;
			}
#endif
			dv[i] += w * tt * pv[i];				/* Del in rv due to del in pv */
			smv += w * pv[i] * pv[i];
		}
	}
	smv /= (double)(p->n);
	rv += smv;

#ifdef DEBUG
	printf("dfunc2 itt %d/%d band %d returning %f\n",p->oit,p->ott,j,rv);
#endif
	return rv;
}

#ifdef TESTDFUNC
/* Check that dfunc2 returns the right values */
static void test_dfunc2(
mpp *p,
int nparms,
double *pv
) {
	int i, j, k;
	double refov;
	double refdv[MPP_MXINKS * MPP_MXTCORD];
	double ov;
	double dv[MPP_MXINKS * MPP_MXTCORD];

	/* Create reference dvs */
	refov = efunc2((void *)p, pv);
	for (i = 0; i < nparms; i++) {
		pv[i] += 1e-6;
		refdv[i] = (efunc2((void *)p, pv) - refov)/1e-6;
		pv[i] -= 1e-6;
	}

	/* Check dfunc2 */
	ov = dfunc2((void *)p, dv, pv);

	printf("~#############################################\n");
	printf("~! Check dfunc2, ov %f, refov %f\n",ov, refov);
	for (i = 0; i < nparms; i++) {
		printf("~1 Parm %d val %f, ref %f\n",i,dv[i],refdv[i]);
	}
}
#endif /* TESTDFUNC */


/* Setup test point data ready for efunc3 on the given oba */
static void sfunc3(mpp *p) {
	double pcnv[MPP_MXCCOMB];	/* Interpolation combination values */
	int j = p->oba;				/* Band being optimised */
	int i, k, m;

	/* Setup the correct per patch value fcnv values */
	for (i = 0; i < p->nodp; i++) {
		mppcol *c = &p->cols[i];

		/* Compute the tranfer corrected device values */
		for (m = 0; m < p->n; m++)
			c->tcnv[m] = icxTransFunc(p->tc[m][j], p->cord, c->nv[m]);

		/* Compute combination values */
		for (k = 0; k < p->nn; k++) {		/* For each interp vertex */
			double vv;
	 		for (vv = 1.0, m = 0; m < p->n; m++) {	/* Compute weighting */
				if (k & (1 << m))
					vv *= c->tcnv[m];
				else
					vv *= (1.0 - c->tcnv[m]);
			}
			pcnv[k] = vv;
		}

		/* Compute shape weighting values */
		for (k = 0; k < p->nnn2; k++) {
			int m = p->c2f[k].ink;
			int n = p->c2f[k].comb;
			/* Compress full interpolation one dimension to */
			/* exclude ink value being tweaked by shape */
			c->fcnv[k] = pcnv[n & ~(1<<m)] + pcnv[n | (1<<m)];
		}
	}
}


/* Optimise all shape parameters simultaniously to minimise a particular bands error */
/* Assume test point tcnv and pcnv are setup for pre-shape values */
static double efunc3(void *adata, double pv[]) {
	mpp *p = (mpp *)adata;
	double smv, rv = 0.0;
	double tcnv[MPP_MXINKS];	/* Transfer curve corrected device values */
	double tcnv1[MPP_MXINKS];	/* 1.0 - Transfer curve corrected device values */
	double ww[MPP_MXINKS];		/* Interpolated tweak params for each channel */
	int j = p->oba;			/* Band being optimised */
	int n1 = p->n - 1;
	int i, m, k;

	/* For each test point */
	for (i = 0; i < p->nodp; i++) {
		mppcol *c = &p->cols[i];
		double ov;

		for (m = 0; m < p->n; m++)
			ww[m] = 0.0;

		/* Interpolate the per ink shape values for this set of input values */
		for (k = 0; k < p->nnn2; k++) {		/* For each ink & interp vertex */
			m = k >> n1; 					/* Corresponding ink channel */
			ww[m] += pv[k] * c->fcnv[k]; /* Apply weighting to shape vertex value */
		}

		/* Apply the shape values to adjust the primaries */
		for (m = 0; m < p->n; m++) {
			double gg = ww[m];			/* Curve adjustment */
			double vv = c->tcnv[m];		/* Input value to be tweaked */
			if (gg >= 0.0) {
				vv = vv/(gg - gg * vv + 1.0);
			} else {
				vv = (vv - gg * vv)/(1.0 - gg * vv);
			}
			tcnv[m] = vv;
			tcnv1[m] = 1.0 - vv;
		}

		/* Compute the primary combination values */
		for (ov = 0.0, k = 0; k < p->nn; k++) {
			double vv = p->pc[k][j];
			for (m = 0; m < p->n; m++) {
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			ov += vv;
		}

		/* Compute the band value error */
		ov = lDE(ov) - c->lband[j];
		rv += ov * ov;
	}

	rv /= (double)p->nodp;

	/* Compute average magnitude of shaper parameters squared */
	/* to minimise unconstrained "wiggles" */
	for (smv = 0.0, m = 0; m < p->nnn2; m++) {
		smv += pv[m] * pv[m];
	}
	smv /= (double)(p->nnn2);
	rv += SHAPE_PMW * smv;		

#ifdef DEBUG
	printf("efunc3 itt %d/%d band %d (smv %f) returning %f\n",p->oit,p->ott,j,smv,rv);
#endif
	return rv;
}

/* Return the gradient of the minimisation function at the given location, */
/* as well as the function value at this location. */
static double dfunc3(void *adata, double dv[], double pv[]) {
	mpp *p = (mpp *)adata;
	double smv, tt, rv = 0.0;
	double dtcnv[MPP_MXINKS];	/* Derivative of transfer curve corrected device values */
	double dov[MPP_MXINKS];		/* Derivative of output interpolation device values */
	double tcnv[MPP_MXINKS];	/* Transfer curve corrected device values */
	double tcnv1[MPP_MXINKS];	/* 1.0 - Transfer curve corrected device values */
	double ww[MPP_MXINKS];		/* Interpolated tweak params for each channel */
	int j = p->oba;			/* Band being optimised */
	int n1 = p->n - 1;
	int i, m, k;

	for (k = 0; k < p->nnn2; k++)
		dv[k] = 0.0;					/* Start with 0 delta */

	/* For each test point */
	for (i = 0; i < p->nodp; i++) {
		mppcol *c = &p->cols[i];
		double ov, ddov;

		for (m = 0; m < p->n; m++)
			ww[m] = 0.0;

		/* Interpolate the per ink shape values for this set of input values */
		for (k = 0; k < p->nnn2; k++) {
			m = k >> n1; 					/* Corresponding ink channel */
			ww[m] += pv[k] * c->fcnv[k]; /* Apply weighting to shape vertex value */
		}

		/* Apply the shape values to adjust the primaries */
		for (m = 0; m < p->n; m++) {
			double gg = ww[m];				/* Curve adjustment */
			double sv, vv = c->tcnv[m];		/* Input value to be tweaked */
			if (gg >= 0.0) {
				tt = gg - gg * vv + 1.0;
				sv = vv/tt;
			} else {
				tt = 1.0 - gg * vv;
				sv = (vv - gg * vv)/tt;
			}
			tcnv[m] = sv;
			tcnv1[m] = 1.0 - sv;
			dtcnv[m] = (vv * vv - vv)/(tt * tt);	/* del in tcnv[m] due to del in ww[m] */
		}

		/* Compute the primary combination values */
		for (ov = 0.0, k = 0; k < p->nn; k++) {
			double vv = p->pc[k][j];
			for (m = 0; m < p->n; m++) {
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			ov += vv;
		}

		/* Compute del ov[m] for del tcnv[m] */
		for (m = 0; m < p->n; m++) {
			for (dov[m] = 0.0, k = 0; k < p->nn; k++) {
				int mm;
				double vv = p->pc[k][j];
				for (mm = 0; mm < p->n; mm++) {
					if (m == mm)
						continue;
					if (k & (1 << mm))
						vv *= tcnv[mm];
					else
						vv *= tcnv1[mm];
				}
				if (k & (1 << m))
					dov[m] += vv;
				else
					dov[m] -= vv;
			}
			dov[m] *= dtcnv[m];		/* del ov[m] due to del in ww[m] */
		}

		/* Compute the band value error */
		ddov = dDE(ov);
		ov = lDE(ov) - c->lband[j];

		ddov *= 2.0 * ov; 	/* del in final ov due to del in raw ov */
		rv += ov * ov;

		for (k = 0; k < p->nnn2; k++) {
			m = k >> n1; 					/* Corresponding ink channel */
			dv[k] += ddov * dov[m] * c->fcnv[k];
		}
	}

	rv /= ((double)p->nodp);

	for (k = 0; k < p->nnn2; k++)
		dv[k] /= ((double)p->nodp);

	/* Compute average magnitude of shaper parameters squared */
	/* to minimise unconstrained "wiggles" */
	tt = SHAPE_PMW * 2.0/(double)(p->nnn2);			/* Common factor */
	for (smv = 0.0, k = 0; k < p->nnn2; k++) {
		dv[k] += tt * pv[k];		/* del rv due to del pv[k] */
		smv += pv[k] * pv[k];
	}
	smv /= (double)(p->nnn2);
	rv += SHAPE_PMW * smv;		

#ifdef DEBUG
	printf("dfunc3 itt %d/%d band %d (smv %f) returning %f\n",p->oit,p->ott,j,smv,rv);
#endif
	return rv;
}

#ifdef TESTDFUNC
/* Check that dfunc3 returns the right values */
static void test_dfunc3(
mpp *p,
int nparms,
double *pv
) {
	int i, j, k;
	double refov;
	double refdv[MPP_MXINKS * MPP_MXCCOMB/2];
	double ov;
	double dv[MPP_MXINKS * MPP_MXCCOMB/2];

	/* Create reference dvs */
	refov = efunc3((void *)p, pv);
	for (k = 0; k < nparms; k++) {
		pv[k] += 1e-6;
		dv[k] = (efunc3((void *)p, pv) - refov)/1e-6;
		pv[k] -= 1e-6;
	}

	/* Check dfunc3 */
	ov = dfunc3((void *)p, dv, pv);

	printf("~#############################################\n");
	printf("~! Check dfunc3, ov %f, refov %f\n",ov, refov);
	for (i = 0; i < nparms; i++) {
		printf("~1 Parm %d val %f, ref %f\n",i,dv[i],refdv[i]);
	}
}
#endif	/* TESTDFUNC */


/* Setup test point data ready for efunc4 on the given oba */
static void sfunc4(mpp *p) {
	double tcnv[MPP_MXINKS];	/* Transfer curve corrected device values */
	double tcnv1[MPP_MXINKS];	/* 1.0 - Transfer curve corrected device values */
	double ww[MPP_MXINKS];		/* Interpolated tweak params for each channel */
	int j = p->oba;			/* Band being optimised */
	int i, k, m;

	/* Setup the correct per patch value fcnv values */
	for (i = 0; i < p->nodp; i++) {
		mppcol *c = &p->cols[i];

		/* Compute the tranfer corrected device values */
		for (m = 0; m < p->n; m++) {
			tcnv[m] = icxTransFunc(p->tc[m][j], p->cord, c->nv[m]);
			tcnv1[m] = 1.0 - tcnv[m];
		}

		for (m = 0; m < p->n; m++)
			ww[m] = 0.0;

		/* Lookup the interpolated shape values */
		for (k = 0; k < p->nn; k++) {		/* For each interp vertex */
			double vv;
	 		for (vv = 1.0, m = 0; m < p->n; m++) {	/* Compute weighting */
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			for (m = 0; m < p->n; m++) {
				ww[m] += p->shape[m][k & ~(1<<m)][j] * vv;
									/* Apply weighting to shape vertex value */
			}
		}

		/* Apply the shape values to adjust the primaries */
		for (m = 0; m < p->n; m++) {
			double gg = ww[m];			/* Curve adjustment */
			double vv = tcnv[m];		/* Input value to be tweaked */
			if (gg >= 0.0) {
				vv = vv/(gg - gg * vv + 1.0);
			} else {
				vv = (vv - gg * vv)/(1.0 - gg * vv);
			}
			tcnv[m] = vv;
			tcnv1[m] = 1.0 - vv;
		}

		/* Compute the shape adjusted primary combination values */
		for (k = 0; k < p->nn; k++) {
			double vv;
			for (vv = 1.0, m = 0; m < p->n; m++) {
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			c->pcnv[k] = vv;
		}
	}
}

/* Optimise all vertex values simultaniously to minimise a particular bands error */
/* Assume test point tcnv and pcnv are setup for post-shape values */
static double efunc4(void *adata, double pv[]) {
	mpp *p = (mpp *)adata;
	double smv = 0.0, rv = 0.0;
	int j = p->oba;				/* Band being optimised */
	int i, k;

	/* For each test point */
	for (i = 0; i < p->nodp; i++) {
		mppcol *c = &p->cols[i];
		double ov = 0.0;

		for (k = 0; k < p->nn; k++) {
			double pp = pv[k];

			if (pp < 0.0)			/* Stop non-real values */
				rv += -5000.0 * pp; 
			ov += c->pcnv[k] * pp;
		}
			
		/* Compute the band value error */
		ov = lDE(ov) - c->lband[j];
		rv += ov * ov;
	}

	rv /= p->nodp;

	/* Compute anchor point error */
	for (smv = 0.0, i = 0; i < p->nn; i++) {
		double tt = lDE(pv[i]) - p->lpca[i][j];
		smv += tt * tt;
	}
	smv /= (double)p->nn;

//printf("~1 anchor error = %f\n",smv);
	rv += COMB_PMW * smv;

#ifdef DEBUG
	printf("efunc4 itt %d/%d band %d returning %f\n",p->oit,p->ott,j,rv);
#endif
	return rv;
}

/* Return the gradient of the minimisation function at the given location, */
/* as well as the function value at this location. */
static double dfunc4(void *adata, double dv[], double pv[]) {
	mpp *p = (mpp *)adata;
	double smv = 0.0, rv = 0.0;
	double drv[MPP_MXCCOMB];	/* Delta in rv */
	int j = p->oba;				/* Band being optimised */
	int i, k;

	for (k = 0; k < p->nn; k++) {
		dv[k] = 0.0;
		drv[k] = 0.0;
	}

	/* For each test point */
	for (i = 0; i < p->nodp; i++) {
		mppcol *c = &p->cols[i];
		double ov = 0.0, ddov;

		for (k = 0; k < p->nn; k++) {
			double pp = pv[k];

			if (pp < 0.0) {			/* Stop non-real values */
				rv += -5000.0 * pp; 
				drv[k] += -5000.0;
			}
			ov += c->pcnv[k] * pp;
		}
			
		/* Compute the band value error */
		ddov = dDE(ov);
		ov = lDE(ov) - c->lband[j];

		ddov *= 2.0 * ov; 	/* del in final ov due to del in raw ov */
		rv += ov * ov;

		for (k = 0; k < p->nn; k++) {
			dv[k] += ddov * c->pcnv[k];
		}
	}

	rv /= p->nodp;
	for (k = 0; k < p->nn; k++)
		dv[k] /= ((double)p->nodp);

	/* Compute anchor point error */
	for (smv = 0.0, k = 0; k < p->nn; k++) {
		double tt, ddov;
		ddov = dDE(pv[k]);
		tt = lDE(pv[k]) - p->lpca[k][j];
		ddov *= COMB_PMW * 2.0 * tt/(double)p->nn;
		dv[k] += ddov;
		smv += tt * tt;
	}
	smv /= (double)p->nn;

//printf("~1 anchor error = %f\n",smv);
	rv += COMB_PMW * smv;

	for (k = 0; k < p->nn; k++)		/* Add any < 0 contribution */
		dv[k] += drv[k];

#ifdef DEBUG
	printf("dfunc4 itt %d/%d band %d returning %f\n",p->oit,p->ott,j,rv);
#endif
	return rv;
}

#ifdef TESTDFUNC
/* Check that dfunc4 returns the right values */
static void test_dfunc4(
mpp *p,
int nparms,
double *pv
) {
	int i, j, k;
	double refov;
	double refdv[MPP_MXCCOMB];
	double ov;
	double dv[MPP_MXCCOMB];

	/* Create reference dvs */
	refov = efunc4((void *)p, pv);
	for (i = 0; i < nparms; i++) {
		pv[i] += 1e-9;
		refdv[i] = (efunc4((void *)p, pv) - refov)/1e-9;
		pv[i] -= 1e-9;
	}

	/* Check dfunc4 */
	ov = dfunc4((void *)p, dv, pv);

	printf("~#############################################\n");
	printf("~! Check dfunc4, ov %f, refov %f\n",ov, refov);
	for (i = 0; i < nparms; i++) {
		printf("~1 Parm %d val %f, ref %f\n",i,dv[i],refdv[i]);
	}
}
#endif /* TESTDFUNC */

/* ---------------------------------------------------- */
/* Optimise whole model in one go */

#ifdef BIGBANG

/* Optimise all parameters simultaniously to minimise a particular bands error */
static double efunc0(void *adata, double pv[]) {
	mpp *p = (mpp *)adata;
	double *pv2, *pv3, *pv4;	/* Pointers to each group of parameters */
	double smv, rv = 0.0;
	double tcnv[MPP_MXINKS];	/* Transfer curve corrected device values */
	double tcnv1[MPP_MXINKS];	/* 1.0 - Transfer curve corrected device values */
	double ww[MPP_MXINKS];		/* Interpolated tweak params for each channel */
	int j = p->oba;			/* Band being optimised */
	int i, m, k;

	pv2 = pv;
	pv3 = pv + (p->n * p->cord);
	pv4 = pv + (p->n * p->cord) + p->nnn2;

	/* For each test point */
	for (i = 0; i < p->nodp; i++) {
		mppcol *c = &p->cols[i];
		double ov;

		/* Compute the tranfer corrected device values */
		for (m = 0; m < p->n; m++) {
			tcnv[m] = icxTransFunc(&pv2[m * p->cord], p->cord, c->nv[m]);
			tcnv1[m] = 1.0 - tcnv[m];
		}

		for (m = 0; m < p->n; m++)
			ww[m] = 0.0;

		/* Lookup the shape values */
		for (k = 0; k < p->nn; k++) {		/* For each interp vertex */
			double vv;
	 		for (vv = 1.0, m = 0; m < p->n; m++) {	/* Compute weighting */
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			for (m = 0; m < p->n; m++) {
				ww[m] += pv3[p->f2c[m][k & ~(1<<m)]] * vv;
									/* Apply weighting to shape vertex value */
			}
		}

		/* Apply the shape values to adjust the primaries */
		for (m = 0; m < p->n; m++) {
			double gg = ww[m];			/* Curve adjustment */
			double vv = tcnv[m];		/* Input value to be tweaked */
			if (gg >= 0.0) {
				vv = vv/(gg - gg * vv + 1.0);
			} else {
				vv = (vv - gg * vv)/(1.0 - gg * vv);
			}
			tcnv[m] = vv;
			tcnv1[m] = 1.0 - vv;
		}

		/* Compute the primary combination values */
		for (ov = 0.0, k = 0; k < p->nn; k++) {
			double vv = pv4[k];
			if (vv < 0.0)			/* Stop non-real values */
				rv += -5000.0 * vv; 
	 		for (m = 0; m < p->n; m++) {
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			ov += vv;
		}

		/* Compute the band value error */
		ov = lDE(ov) - c->lband[j];
		rv += ov * ov;
	}

	rv /= (double)p->nodp;

	/* Compute weighted magnitude of shaper parameters squared */
	/* to minimise unconstrained "wiggles" */
	for (smv = 0.0, m = 0; m < p->n; m++) {
		double w;
		for (k = 0; k < p->cord; k++) {
			i = m * p->cord + k;
			w = (k < 2) ? TRANS_BASE : k * TRANS_HBASE; 	/* Increase weight with harmonics */
			smv += w * pv2[i] * pv2[i];
		}
	}
	smv /= (double)(p->n);
	rv += smv;

	/* Compute average magnitude of shaper parameters squared */
	/* to minimise unconstrained "wiggles" */
	for (smv = 0.0, m = 0; m < p->nnn2; m++) {
		smv += pv3[m] * pv3[m];
	}
	smv /= (double)(p->nnn2);
	rv += SHAPE_PMW * smv;		

	/* Compute anchor point error */
	for (smv = 0.0, i = 0; i < p->nn; i++) {
		double tt = lDE(pv4[i]) - p->lpca[i][j];
		smv += tt * tt;
	}
	smv /= (double)p->nn;
	rv += COMB_PMW * smv;

#ifdef DEBUG
	printf("efunc0 itt %d/%d band %d returning %f\n",p->oit,p->ott,j,rv);
#endif
	return rv;
}

/* Return the gradient of the minimisation function at the given location, */
/* as well as the function value at this location. */
static double dfunc0(void *adata, double dv[], double pv[]) {
	mpp *p = (mpp *)adata;
	double *pv2, *pv3, *pv4;	/* Pointers to each group of parameters */
	double *dv2, *dv3, *dv4;	/* Pointers to each group of derivatives */
	double smv, tt, rv = 0.0;

	double dtcnv_dpv2[MPP_MXINKS][MPP_MXTCORD];	/* Del in tcnv[m] due to del in pv2 */
	double dww_dpv3[MPP_MXINKS][MPP_MXINKS * MPP_MXCCOMB/2]; /* Del in ww[m] due to del in pv3 */
	double dov_dpv4[MPP_MXCCOMB];				/* Delta in ov due to delta in pv4 */
	double drv4[MPP_MXCCOMB];					/* Delta in rv due to error in pv4 */

	double dww_dtcnv[MPP_MXINKS][MPP_MXINKS];	/* Del in ww[m] due to del in tcnv[m] */
	double dtcnv_tc[MPP_MXINKS];				/* Del in tcnv'[m] due to del in tcnv[m] */
	double dtcnv_ww[MPP_MXINKS];				/* Del in tcnv'[m] due to del in ww[m] */
	double dov_dcnv[MPP_MXINKS];				/* Del of ov due to del in tcnv'[m] */
	double ddov; 								/* Del in final ov due to del in raw ov */

	double tcnv[MPP_MXINKS];	/* Transfer curve corrected device values */
	double tcnv1[MPP_MXINKS];	/* 1.0 - Transfer curve corrected device values */
	double ww[MPP_MXINKS];		/* Interpolated tweak params for each channel */
	int j = p->oba;				/* Band being optimised */
	int i, m, k;

	pv2 = pv;
	pv3 = pv + (p->n * p->cord);
	pv4 = pv + (p->n * p->cord) + p->nnn2;
	dv2 = dv;
	dv3 = dv + (p->n * p->cord);
	dv4 = dv + (p->n * p->cord) + p->nnn2;

	for (k = 0; k < (p->n * p->cord); k++)
		dv2[k] = 0.0;
	for (k = 0; k < p->nnn2; k++)
		dv3[k] = 0.0;
	for (k = 0; k < p->nn; k++)
		dv4[k] += 0.0;

	/* For each test point */
	for (i = 0; i < p->nodp; i++) {
		int mo;
		mppcol *c = &p->cols[i];
		double ov;


		/* Compute the tranfer corrected device values */
		/* and del in these values due to del in input parameters */
		for (m = 0; m < p->n; m++) {
			tcnv[m] = icxdpTransFunc(&pv2[m * p->cord], dtcnv_dpv2[m], p->cord, c->nv[m]);
			tcnv1[m] = 1.0 - tcnv[m];
		}

		for (m = 0; m < p->n; m++) {
			ww[m] = 0.0;
			for (k = 0; k < p->nnn2; k++)
				dww_dpv3[m][k] = 0.0;
		}

		/* Lookup the shape values */
		for (k = 0; k < p->nn; k++) {		/* For each interp vertex */
			double vv;
	 		for (vv = 1.0, m = 0; m < p->n; m++) {	/* Compute weighting */
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			for (m = 0; m < p->n; m++) {
				int pix = p->f2c[m][k & ~(1<<m)];
				ww[m] += pv3[pix] * vv; /* Apply weighting to shape vertex value */
				dww_dpv3[m][pix] += vv;	/* Delta to ww[m] from del in pv3[pix] */
			}
		}

		/* Compute del ww[m][m] for del tcnv[m] */
		for (m = 0; m < p->n; m++) {			/* For each input channel were doing del of */
			for (mo = 0; mo < p->n; mo++)
				dww_dtcnv[mo][m] = 0.0;
			for (k = 0; k < p->nn; k++) {
				int mm;
				double vv = 1.0;
				for (mm = 0; mm < p->n; mm++) {	/* Compute weight for node k */
					if (m == mm)
						continue;
					if (k & (1 << mm))
						vv *= tcnv[mm];
					else
						vv *= tcnv1[mm];
				}
				for (mo = 0; mo < p->n; mo++) {				/* For each output channel */
					double vvv = vv * pv3[p->f2c[mo][k & ~(1<<mo)]];
					if (k & (1 << m))
						dww_dtcnv[mo][m] += vvv;
					else
						dww_dtcnv[mo][m] -= vvv;
				}
			}
		}

		/* Apply the shape values to adjust the primaries */
		for (m = 0; m < p->n; m++) {
			double gg = ww[m];					/* Curve adjustment */
			double sv, dsv, vv = tcnv[m];		/* Input value to be tweaked */
			if (gg >= 0.0) {
				tt = gg - gg * vv + 1.0;
				sv = vv/tt;
				dsv = (gg + 1.0)/(tt * tt);
			} else {
				tt = 1.0 - gg * vv;
				sv = (vv - gg * vv)/tt;
				dsv = (1.0 - gg)/(tt * tt);
			}
			tcnv[m] = sv;
			tcnv1[m] = 1.0 - sv;
			dtcnv_tc[m] = dsv;						/* del in tcnv[m] due to del in tcnv[m] */
			dtcnv_ww[m] = (vv * vv - vv)/(tt * tt);	/* del in tcnv[m] due to del in ww[m] */
		}

		/* Compute the primary combination values */
		for (ov = 0.0, k = 0; k < p->nn; k++) {
			double pp, vv;
	 		for (vv = 1.0, m = 0; m < p->n; m++) {
				if (k & (1 << m))
					vv *= tcnv[m];
				else
					vv *= tcnv1[m];
			}
			dov_dpv4[k] = vv;

			pp = pv4[k];
			if (pp < 0.0) {			/* Stop non-real values */
				rv += -5000.0 * pp; 
				drv4[k] += -5000.0;
			}
			ov += vv * pp;
		}

		/* Compute del ov for del tcnv[m] */
		for (m = 0; m < p->n; m++) {
			for (dov_dcnv[m] = 0.0, k = 0; k < p->nn; k++) {
				int mm;
				double vv = p->pc[k][j];
				for (mm = 0; mm < p->n; mm++) {
					if (m == mm)
						continue;
					if (k & (1 << mm))
						vv *= tcnv[mm];
					else
						vv *= tcnv1[mm];
				}
				if (k & (1 << m))
					dov_dcnv[m] += vv;
				else
					dov_dcnv[m] -= vv;
			}
		}

		/* Compute the band value error */
		ddov = dDE(ov);
		ov = lDE(ov) - c->lband[j];
	
		ddov *= 2.0 * ov; 	/* del in final ov due to del in raw ov */
		rv += ov * ov;
	
		/* Accumulate delta from input params to output */
		for (m = 0;  m < p->n; m++) {
			for (k = 0; k < p->cord; k++) {
				double ttt;
				int mm;
	
				/* delta via dww */
				for (ttt = 0.0, mm = 0; mm < p->n; mm++)
					ttt += dov_dcnv[mm] * dtcnv_ww[mm] * dww_dtcnv[mm][m] * dtcnv_dpv2[m][k];
	
				/* delta direct */
				dv2[m * p->cord + k] += ddov * (ttt + dov_dcnv[m] * dtcnv_tc[m] * dtcnv_dpv2[m][k]);
			}
		}
		for (k = 0; k < p->nnn2; k++) {
			double ttt;

			/* delta via dww */
			for (ttt = 0.0, m = 0; m < p->n; m++)
				ttt += dov_dcnv[m] * dtcnv_ww[m] * dww_dpv3[m][k];
			dv3[k] += ddov * ttt;
		}
		for (k = 0; k < p->nn; k++) {
			dv4[k] += ddov * dov_dpv4[k];
		}
	}

	rv /= (double)p->nodp;
	for (k = 0; k < (p->n * p->cord); k++)
		dv2[k] /= (double)p->nodp;
	for (k = 0; k < p->nnn2; k++)
		dv3[k] /= ((double)p->nodp);
	for (k = 0; k < p->nn; k++)
		dv4[k] /= ((double)p->nodp);

	/* Compute weighted magnitude of shaper parameters squared */
	/* to minimise unconstrained "wiggles" */
	tt = 2.0/(double)(p->n);		/* Common factor */
	for (smv = 0.0, m = 0; m < p->n; m++) {
		double w;
		for (k = 0; k < p->cord; k++) {
			i = m * p->cord + k;
			w = (k < 2) ? TRANS_BASE : k * TRANS_HBASE; 	/* Increase weight with harmonics */
			dv2[i] += w * tt * pv2[i];				/* Del in rv due to del in pv */
			smv += w * pv2[i] * pv2[i];
		}
	}
	smv /= (double)(p->n);
	rv += smv;

	/* Compute average magnitude of shaper parameters squared */
	/* to minimise unconstrained "wiggles" */
	tt = SHAPE_PMW * 2.0/(double)(p->nnn2);			/* Common factor */
	for (smv = 0.0, k = 0; k < p->nnn2; k++) {
		dv3[k] += tt * pv3[k];		/* del rv due to del pv[k] */
		smv += pv3[k] * pv3[k];
	}
	smv /= (double)(p->nnn2);
	rv += SHAPE_PMW * smv;		

	/* Compute anchor point error */
	for (smv = 0.0, k = 0; k < p->nn; k++) {
		double tt, ddov;
		ddov = dDE(pv4[k]);
		tt = lDE(pv4[k]) - p->lpca[k][j];
		ddov *= COMB_PMW * 2.0 * tt/(double)p->nn;
		dv4[k] += ddov;
		smv += tt * tt;
	}
	smv /= (double)p->nn;
	rv += COMB_PMW * smv;

	for (k = 0; k < p->nn; k++)		/* Add any < 0 contribution */
		dv4[k] += drv4[k];

#ifdef DEBUG
	printf("dfunc0 itt %d/%d band %d returning %f\n",p->oit,p->ott,j,rv);
#endif
	return rv;
}

#ifdef TESTDFUNC
/* Check that dfunc0 returns the right values */
static void test_dfunc0(
mpp *p,
int nparms,
double *pv
) {
	int i, j, k;
	double refov;
	double refdv[MPP_MXPARMS];
	double ov;
	double dv[MPP_MXPARMS];

	/* Create reference dvs */
	refov = efunc0((void *)p, pv);
	for (i = 0; i < nparms; i++) {
		pv[i] += 1e-9;
		refdv[i] = (efunc0((void *)p, pv) - refov)/1e-9;
		pv[i] -= 1e-9;
	}

	/* Check dfunc0 */
	ov = dfunc0((void *)p, dv, pv);

	printf("~#############################################\n");
	printf("~! Check dfunc0, ov %f, refov %f\n",ov, refov);
	for (i = 0; i < nparms; i++) {
		printf("~1 Parm %d val %f, ref %f\n",i,dv[i],refdv[i]);
	}
}
#endif /* TESTDFUNC */
#endif /* BIGBANG */

#ifdef ISHAPE
/* ----------------------------------------------------- */
/* Create an initial solution for shape parameters */
/* by solving least squares using SVD */

/* Optimise device values to match test points Lab */
/* while minimising the difference between the device */
/* values and the current model device values in tcnv */
static double efuncS(void *adata, double pv[]) {
	double tcnv[MPP_MXINKS];	/* Shape tweaked input values */
	double tcnv1[MPP_MXINKS];
	mpp *p = (mpp *)adata;
	double smv, rv = 0.0;
	mppcol *c = p->otp;
	int j = p->oba;
	int k, m;

	double ov;

	for (k = 0; k < p->n; k++) {
		double gg = pv[k];			/* Curve adjustment */
		double vv = c->tcnv[k];		/* Input value to be tweaked */
		if (gg >= 0.0) {
			vv = vv/(gg - gg * vv + 1.0);
		} else {
			vv = (vv - gg * vv)/(1.0 - gg * vv);
		}
		tcnv[k] = vv;
		tcnv1[k] = 1.0 - vv;
	}

	/* Compute the primary combination values */
	for (ov = 0.0, k = 0; k < p->nn; k++) {
		double vv = p->pc[k][j];
 		for (m = 0; m < p->n; m++) {
			if (k & (1 << m))
				vv *= tcnv[m];
			else
				vv *= tcnv1[m];
		}
			ov += vv;
	}

	/* Compute the band value error */
	ov = lDE(ov) - c->lband[j];
	rv += ov * ov;

	/* Compute magnitude of shape params */
	for (smv = 0.0, k = 0; k < p->n; k++) {
		smv += pv[k] * pv[k];
	}
	rv += SHAPE_PMW * 0.1 * smv/(double)p->n;	/* Don't worry about this here - SVD will cope */

//printf("~1 efuncS %f %f %f %f returning %f\n",pv[0],pv[1],pv[2],pv[3],rv);
	return rv;
}


static void ishape(
mpp *p,
int j			/* Band being initialised */
) {
	int i, k, m;
	double **a;		/* A[0..m-1][0..n-1] input A[][], will return U[][] */
	double *b;		/* B[0..m-1]  Right hand side of equation, return solution */

	p->oba = j;

	if (p->nodp < (p->nn/2))		/* Nicer to return error ?? */
		error ("Not enough data points to solve shaper parameters");

//printf("~1 ishape called for band %d\n",j);

	/* First step is to compute the ideal shape values */
	/* for each test point. */
	for (i = 0; i < p->nodp; i++) {
		mppcol *c = &p->cols[i];
		double pv[MPP_MXINKS];		/* Parameter values */
		double sr[MPP_MXINKS];		/* search radius */

		p->otp = c;

//printf("~1 setting up point %d\n",i);

		/* Compute the tranfer corrected device values */
		for (m = 0; m < p->n; m++)
			c->tcnv[m] = icxTransFunc(p->tc[m][j], p->cord, c->nv[m]);

		/* Compute pre-shape combination values */
		for (k = 0; k < p->nn; k++) {		/* For each interp vertex */
			double vv;
	 		for (vv = 1.0, m = 0; m < p->n; m++) {	/* Compute weighting */
				if (k & (1 << m))
					vv *= c->tcnv[m];
				else
					vv *= (1.0 - c->tcnv[m]);
			}
			c->pcnv[k] = vv;
		}

		/* Do reverse lookup to find ideal post transfer device */
		/* shape values that are minimal, but will give the desired */
		/* band value */
		for (k = 0; k < p->n; k++) {
			pv[k] = 0.0;
			sr[k] = 0.5;
		}

		if (powell(NULL, p->n, pv, sr, 0.001, 300, efuncS, (void *)p, NULL, NULL) != 0) {
//printf("~1 ishape Powell failed at point %d\n",i);
			for (k = 0; k < p->n; k++)
				pv[k] = 0.0;
		}

//printf("~1 got uncorrected device vals %f %f %f %f\n",c->nv[0],c->nv[1],c->nv[2],c->nv[3]);
//printf("~1 got shape target values     %f %f %f %f\n",pv[0],pv[1],pv[2],pv[3]);

		/* put them leave them in scnv */
		for (k = 0; k < p->n; k++) {
			c->scnv[k] = pv[k];
		}
	}

	/* The second step is to use SVD to compute the per colorant */
	/* interpolation corner values that are the best least squares */
	/* solution to providing those shape parameters at each point. */

	/* Allocate our SVD matricies */
	a = dmatrix(0, p->nodp-1 + p->nn/2, 0, p->nn/2-1);
	b = dvector(0, p->nodp-1 + p->nn/2);

	for (m = 0; m < p->n; m++) {	/* For each channel */
		int kk; 

		/* Setup our matricies for the test point */
		for (i = 0; i < p->nodp; i++) {
			mppcol *c = &p->cols[i];
			double ss;			/* Weighting factor */

			/* The theoretically correct weighting to get */
			/* better alighment with DEsq() measure is */
			/* ss = dDE(c->band[j]);, but this doesn't seem */
			/* to work well in practice. */

			/* I don't quite understand why this weighting */
			/* seems to give a good result. */
			ss = c->tcnv[m] * (1.0 - c->tcnv[m]);	/* Don't understand why this is so good */

			for (kk = k = 0; k < p->nn; k++) {
				if (k & (1<<m))
					continue;			/* Invalid comb for this channel */
				
				a[i][kk++] = ss * (c->pcnv[k] + c->pcnv[k | (1<<m)]);
			}
			b[i] = ss * c->scnv[m];
		}

		/* Add extra fake "data points" to weight the minimisation of */
		/* the shape parameter values */
		{
			int ii;
			double ss;

			/* We want the relative weight to be similar to that */
			/* aimed for in efunc3 */

			// ~~~ 0.1 seems about right for 1150 ???
			//     1.0 for roland8 ??
//			ss = (double)p->nodp/((double)(p->nn/2)) * 1.0 * SHAPE_PMW;
			ss = (double)p->nodp * 0.05 * SHAPE_PMW;

			for (ii = 0; ii < p->nn/2; ii++) {
				for (kk = k = 0; k < p->nn; k++) {
					if (k & (1<<m))
						continue;			/* Invalid comb for this channel */
						
					if (kk == ii)			/* Weights only on diagonals */
						a[i+ii][kk] = ss;
					else
						a[i+ii][kk] = 0.0;
					kk++;
				}
				b[i+ii] = 0.0;
			}
		}

		/* Solve the equation A.x = b using SVD */
		if (svdsolve(a, b, p->nodp + p->nn/2, p->nn/2) != 0)
			error("ishape: SVD failed");

		/* Copy the solution back */
		for (kk = k = 0; k < p->nn; k++) {
			if (k & (1<<m))
				continue;			/* Invalid comb for this channel */
			
//printf("~1 Calculated shape[%d][%d] = %f\n",m,k,b[kk]);
			p->shape[m][k][j] = b[kk++];
		}
	}

	free_dvector(b, 0, p->nodp-1 + p->nn/2);
	free_dmatrix(a, 0, p->nodp-1 + p->nn/2, 0, p->nn/2-1);
//printf("~1 ishape is done for band %d\n",j);

}
#endif /* ISHAPE */


/* ----------------------------------------------------- */
/* Routine to figure out a suitable black point */

/* Structure to hold optimisation information */
typedef struct {
	mpp *p;					/* Lookup object */
	int n;					/* Number of device channels */
	double ilimit;			/* Ink limit */
	double p1[3];			/* white pivot point */
	double p2[3];			/* Point on vector towards black */
} bfinds;

/* Optimise device values to minimise L, while remaining */
/* within the ink limit, and staying in line between p1 and p2 */
static double efunc6(void *adata, double pv[]) {
	bfinds *b = (bfinds *)adata;
	double rv = 0.0;
	double dv[MPP_MXINKS];
	double Lab[3];
	double lr, ta, tb, terr;	/* L ratio, target a, target b, target error */
	double sum, ovr;
	int j;

	/* See if over ink limit or outside device range */
	for (ovr = sum = 0.0, j = 0; j < b->n; j++) {
		dv[j] = pv[j];
		if (dv[j] < 0.0) {
			if (-dv[j] > ovr)
				ovr = -dv[j];
			dv[j] = 0.0;
		} else if (dv[j] > 1.0) {
			if ((dv[j] - 1.0) > ovr)
				ovr = dv[j] - 1.0;
			dv[j] = 1.0;
		}
		sum += dv[j];
	}

	if (b->ilimit > 1e-4) {
		sum -= b->ilimit;
		if (sum < 0.0)
			sum = 0.0;
	} else {
		sum = 0.0;
	}

	/* Compute Lab value */
	forward(b->p, NULL, Lab, NULL, dv);

#ifdef DEBUG
	printf("p1 =  %f %f %f, p2 = %f %f %f\n",b->p1[0],b->p1[1],b->p1[2],b->p2[0],b->p2[1],b->p2[2]);
	printf("device value %f %f %f %f, Lab = %f %f %f\n",dv[0],dv[1],dv[2],dv[3],Lab[0],Lab[1],Lab[2]);
#endif

	/* Primary is to minimise L value */
	rv = Lab[0];

	/* See how out of line from p1 to p2 we are */
	lr = (Lab[0] - b->p1[0])/(b->p2[0] - b->p1[0]);		/* Distance towards p2 from p1 */
	ta = lr * (b->p2[1] - b->p1[1]) + b->p1[1];			/* Target a value */
	tb = lr * (b->p2[2] - b->p1[2]) + b->p1[2];			/* Target b value */

	terr = (ta - Lab[1]) * (ta - Lab[1])
	     + (tb - Lab[2]) * (tb - Lab[2]);
	
#ifdef DEBUG
	printf("target error %f\n",terr);
#endif
	rv += 100.0 * terr;

#ifdef DEBUG
	printf("out of range error %f\n",ovr);
	printf("over limit error %f\n",sum);
#endif
	rv += 200 * (ovr + sum);

#ifdef DEBUG
	printf("black find tc ret %f\n",rv);
#endif
	return rv;
}

static void compute_wb(
mpp *p
) { 
	int j;

	/* Figure out the white and black points */
	if (p->imask & ICX_ADDITIVE) {	/* Assume additive doesn't have an ink limit */

		/* Simply lookup values from min and max device. */
		/* If we have ICX_WHITE, should this be treated the same as */
		/* subtractive black ???? */
		for (j = 0; j < p->n; j++)
			p->white.nv[j] = 1.0;
		forward(p, &p->white.band[3], NULL, p->white.band, p->white.nv);

		for (j = 0; j < p->n; j++)
			p->black.nv[j] = 0.0;
		forward(p, &p->black.band[3], NULL, p->black.band, p->black.nv);

		for (j = 0; j < p->n; j++)
			p->kblack.nv[j] = 0.0;
		forward(p, &p->kblack.band[3], NULL, p->kblack.band, p->kblack.nv);

	} else {		/* Subtractive */
		bfinds bfs;
		double sr[MPP_MXINKS];		/* search radius */
		double tt[MPP_MXINKS];		/* temporary */
		int trial;
		double rv, brv;
		int kbset = 0;

		/* Lookup white directly */
		for (j = 0; j < p->n; j++)
			p->white.nv[j] = 0.0;
		forward(p, &p->white.band[3], bfs.p1, p->white.band, p->white.nv);

		/* Choose a black direction */
		if (p->imask & ICX_BLACK) {
			int bix;
			bix = icx_ink2index(p->imask, ICX_BLACK);

			for (j = 0; j < p->n; j++) {
				if (j == bix)
					p->kblack.nv[j] = 1.0;
				else
					p->kblack.nv[j] = 0.0;
			}
			forward(p, &p->kblack.band[3], bfs.p2, p->kblack.band, p->kblack.nv);
			kbset = 1;

		} else if ((p->imask & (ICX_CYAN | ICX_MAGENTA | ICX_YELLOW))
		                    == (ICX_CYAN | ICX_MAGENTA | ICX_YELLOW)) {
			int cix, mix, yix;
			cix = icx_ink2index(p->imask, ICX_CYAN);
			mix = icx_ink2index(p->imask, ICX_MAGENTA);
			yix = icx_ink2index(p->imask, ICX_YELLOW);

			for (j = 0; j < p->n; j++) {
				if (j == cix)
					p->kblack.nv[j] = 1.0;
				else if (j == mix)
					p->kblack.nv[j] = 1.0;
				else if (j == yix)
					p->kblack.nv[j] = 1.0;
				else
					p->kblack.nv[j] = 0.0;
			}
			forward(p, NULL, bfs.p2, NULL, p->kblack.nv);

		} else {	/* Make direction parallel to L axis */
			bfs.p2[0] = 0.0;
			bfs.p2[1] = bfs.p1[1];
			bfs.p2[2] = bfs.p1[2];
		}
		bfs.p = p;
		bfs.n = p->n;
		bfs.ilimit = p->limit;
		
		/* Find the black point */
		/* Do several trials to avoid local minima */
		for (j = 0; j < p->n; j++) { 
			tt[j] = p->black.nv[j] = 0.5;		/* Starting point */
			sr[j] = 0.1;
		}
		brv = 1e38;
		for (trial = 0; trial < 20; trial++) {

			if (powell(&rv, p->n, tt, sr, 0.00001, 500, efunc6, (void *)&bfs, NULL, NULL) == 0) {
				if (rv < brv) {
					brv = rv;
					for (j = 0; j < p->n; j++)
						p->black.nv[j] = tt[j];
				}
			}
			for (j = 0; j < p->n; j++) {
				tt[j] = p->black.nv[j] + d_rand(-0.3, 0.3);
				if (tt[j] < 0.0)
					tt[j] = 0.0;
				else if (tt[j] > 1.0)
					tt[j] = 1.0;
			}
		}
		if (brv > 1000.0)
			error ("mpp: Black point powell failed");

		for (j = 0; j < p->n; j++) { 	/* Make sure device values are in range */
			if (p->black.nv[j] < 0.0)
				p->black.nv[j] = 0.0;
			else if (p->black.nv[j] > 1.0)
				p->black.nv[j] = 1.0;
		}

		/* Set black value */
		forward(p, &p->black.band[3], NULL, p->black.band, p->black.nv);

		/* Set K only if device doesn't have a K channel */
		if (kbset == 0) {
			for (j = 0; j < p->n; j++)
				p->kblack.nv[j] = p->black.nv[j];
			forward(p, &p->kblack.band[3], NULL, p->kblack.band, p->kblack.nv);
		}

		if (p->verb) {
			double Lab[3];

			icmXYZ2Lab(&icmD50, Lab, p->white.band);
			printf("White point %f %f %f [Lab %f %f %f]\n",
			p->white.band[0],p->white.band[1],p->white.band[2],
			Lab[0], Lab[1], Lab[2]);

			icmXYZ2Lab(&icmD50, Lab, p->black.band);
			printf("Black point %f %f %f [Lab %f %f %f]\n",
			p->black.band[0],p->black.band[1],p->black.band[2],
			Lab[0], Lab[1], Lab[2]);

			icmXYZ2Lab(&icmD50, Lab, p->kblack.band);
			printf("K only Black point %f %f %f [Lab %f %f %f]\n",
			p->kblack.band[0],p->kblack.band[1],p->kblack.band[2],
			Lab[0], Lab[1], Lab[2]);
		}
	}
}

/* ===================================== */

/* Create the mpp from scattered data points */
/* Returns nz on error */
static int create(
	mpp *p,					/* This */
	int verb,				/* Vebosity level, 0 = none */
	int quality,			/* Profile quality, 0..3 */
	int display,			/* non-zero if display device */
	double limit,			/* Total ink limit (if not display) */
	inkmask devmask,		/* Inkmask describing device colorspace */
	int    spec_n,			/* Number of spectral bands, 0 if not valid */
	double spec_wl_short,	/* First reading wavelength in nm (shortest) */
	double spec_wl_long,	/* Last reading wavelength in nm (longest) */
	double norm,			/* Normalising scale value for spectral values */
	instType itype,			/* Spectral instrument type (if not display) */
	int nodp,				/* Number of points */
	mppcol *points			/* Array of input points */
) {
	int it, i, j, k;
	double de, mxde;		/* Average Delta E and maximum Delta E */
	double sde, mxsde;		/* Average Spectral error and maximum spectral error */
	int mxtcord;			/* maximum transfer curve order */
	int maxit;				/* Maximum number of tuning itterations */
	double thr;				/* Powell threshold multiplier at each tuning pass */
	int useshape;			/* Make use of shaping parameters */
	int mode;				/* Band scanning mode */

	/* Convert quality into operation counts */
	switch (quality) {
		case 0:				/* Low */
			useshape = 0;
			mxtcord = 8;
			maxit = 3;
			break;
		case 1:
		default:			/* Medium */
			useshape = 1;
			mxtcord = 10;
			maxit = 4;
			break;
		case 2:				/* High */
			useshape = 1;
			mxtcord = 14;
			maxit = 5;
			break;
		case 3:				/* Ultra high */
			useshape = 1;
			mxtcord = 20;	/* Is more actually better ? */
			maxit = 10;
			break;
		case 99:				/* Special, simple model */
			useshape = 0;
			mxtcord = 1;
			maxit = 4;
			break;
	}

	/* Setup the basic mpp information */
	p->verb = verb;
	p->imask = devmask;
	p->n = icx_noofinks(devmask);
	p->nn = 1 << p->n;
	p->nnn2 = p->n * p->nn/2;

	if (display) {
		p->display = 1;
		p->limit = p->n;
		p->itype = instUnknown;
	} else {
		p->display = 0;
		p->limit = limit;			/* Record it here */
		p->itype = itype;
	}
	p->spec_n = spec_n;
	p->spec_wl_short = spec_wl_short;
	p->spec_wl_long  = spec_wl_long;
	p->nodp = nodp;

	/* MPP limit is less than XICC */
	if (p->n > MPP_MXINKS) {
		p->errc = 1;
		sprintf(p->err,"MPP Can't handle %d colorants",p->n);
		return 1;
	}

	/* MPP limit is less than XSPECT */
	if (spec_n > MPP_MXBANDS) {
		p->errc = 1;
		sprintf(p->err,"MPP Can't handle %d spectral bands",spec_n);
		return 1;
	}

	/* Take a copy of the data points */
	if ((p->cols = new_mppcols(p->nodp, p->n, p->spec_n)) == NULL) {
		error("Malloc failed!");
	}
	if ((new_mppcol(&p->white, p->n, p->spec_n)) != 0) {
		error("Malloc failed!");
	}
	if ((new_mppcol(&p->black, p->n, p->spec_n)) != 0) {
		error("Malloc failed!");
	}
	if ((new_mppcol(&p->kblack, p->n, p->spec_n)) != 0) {
		error("Malloc failed!");
	}

	p->spmax = -1e6;
	for (i = 0; i < p->nodp; i++) {
		copy_mppcol(&p->cols[i], &points[i], p->n, p->spec_n);	/* Copy structure */

		/* Create Lab version */
		icmXYZ2Lab(&icmD50, p->cols[i].Lab, p->cols[i].band);

#ifdef SHARPEN
		XYZ2sharp(&p->cols[i].band[0], &p->cols[i].band[1], &p->cols[i].band[2]);
#endif
		/* Normalise spectral values */
		for (j = 0; j < p->spec_n; j++) {
			p->cols[i].band[3+j] /= norm;		/* Normalise spectral value to range 0..1 */

			if (p->cols[i].band[3+j] > p->spmax)	/* Track maximum value */
				p->spmax = p->cols[i].band[3+j];
		}

		/* Compute L* type band target values */
		for (j = 0; j < (3+p->spec_n); j++) {
			p->cols[i].lband[j] = lDE(p->cols[i].band[j]);
		}
	}
	p->norm = 1.0;		/* Internal norm is 1.0 */

	/* Compute L* type band target values */
	for (i = 0; i < p->nodp; i++) {
		for (j = 0; j < (3+p->spec_n); j++) {
			p->cols[i].lband[j] = lDE(p->cols[i].band[j]);
		}
	}
	/* Init transfer curve parameters of model */
	for (k = 0; k < p->n; k++) {				/* For each ink */
		for (j = 0; j < (p->spec_n+3); j++) {	/* For each band */
			for (i = 0; i < mxtcord; i++) {		/* For each curve order */
				if (i == 0)
					p->tc[k][j][i] = -1.6;		/* Typical starting value */
				else
					p->tc[k][j][i] = 0.0;		/* Straight transfer curve */
			}
		}
	}


	/* Initialise the primary colorant values */
	for (i = -1; i < p->n; i++) {
		int ii, bk = 0;
		double bdif = 1e6;
		
		if (i < 0)
			ii = 0;
		else
			ii = 1 << i;

		/* Search the patch list to find the one closest to this colorant combination */
		for (k = 0; k < p->nodp; k++) {
			double dif = 0.0;
			for (j = 0; j < p->n; j++) {
				double tt;
				if (i == j)
					tt = 1.0 - p->cols[k].nv[j];
				else
					tt = p->cols[k].nv[j];
				dif += tt * tt;
			}
			if (dif < bdif) {		/* best so far */
				bdif = dif;
				bk = k;
				if (dif < 0.001)
					break;			/* Don't bother looking further */
			}
		}
		
		/* Put that sample patch in place as initial value */
		for (j = 0; j < (3+p->spec_n); j++)
			p->pc[ii][j] = p->cols[bk].band[j];
#ifdef DEBUG
	printf("comb 0x%x XYZ is %f %f %f\n", ii, p->pc[ii][0], p->pc[ii][1], p->pc[ii][2]);
#endif
	}

	/* Estimate primary combination values from primary values */
	{
		double sm[3+MPP_MXBANDS];		/* Smallest reflection sample in this band */
		double ink[3+MPP_MXBANDS];

		/* Search the patch list to find the smallest values for each band */
		/* we'll use this as a guide to the freznell reflection from the surface */
		for (j = 0; j < (3+p->spec_n); j++) {

			sm[j] = 1e6;
			for (k = 0; k < p->nodp; k++) {
				if (sm[j] > p->cols[k].band[j]) {
					int m;
					sm[j] = p->cols[k].band[j];
					ink[j] = 0.0;
					for (m = 0; m < p->n; m++)
						ink[j] += p->cols[k].nv[m];
				}
			}
			/* Adjust for error in freznell was estimated from */
			/* a low ink coverage */
			if (ink[j] < 4.0) {
				sm[j] *= pow(0.775, 4.0 - ink[j]);
			}
#ifdef DEBUG
	printf("smallest value in band %d = %f from total ink %f\n",j,sm[j],ink[j]);
#endif
		}

		for (i = 3; i < p->nn; i++) {

			if ((i & (i-1)) == 0)
				continue;			/* Skip primaries */
			for (j = 0; j < (3+p->spec_n); j++) {
				int k;
				double wh = p->pc[0][j];		/* white */
				double tr = 1.0;				/* Trapping coefficient */
				if (wh < 0.01)
					wh = 0.01;			/* Guard against silliness */
				p->pc[i][j] = wh;		/* Start with white */
				for (k = 0; k < p->n; k++) {
					if (i & (1<<k)) {
						double co = p->pc[1 << k][j];		/* colorant + paper reflectance */
						co = (co - sm[j])/wh;				/* Colorant reflectance */
						co /= tr;				/* Trapping reduction */
						if (co > 1.0)
							co = 1.0;
						else if (co < 0.0)
							co = 0.0;
						p->pc[i][j] *= co;		/* Estimated combined reflectivity */

						tr *= 0.90;				/* Next inks effectiveness due to trapping */
					}
				}
				p->pc[i][j] = p->pc[i][j];
				p->pc[i][j] += sm[j];			/* Add freznell back in */
			}
#ifdef DEBUG
	printf("comb 0x%x estimated XYZ = %f %f %f\n",i, p->pc[i][0], p->pc[i][1], p->pc[i][2]);
#endif
		}
	}
		
	/* Override the estimated primary combination values, if actual readings are available */
	for (i = 3; i < p->nn; i++) {
		int k, bk = 0;
		double bdif = 1e6;
		
		if ((i & (i-1)) == 0)
			continue;			/* Skip primaries */

		/* Search the patch list to find the one closest to this colorant combination */
		for (k = 0; k < p->nodp; k++) {
			double dif = 0.0;
			for (j = 0; j < p->n; j++) {
				double tt;
				if (i & (1<<j))
					tt = 1.0 - p->cols[k].nv[j];
				else
					tt = p->cols[k].nv[j];
				dif += tt * tt;
			}
			if (dif < bdif) {		/* best so far */
				bdif = dif;
				bk = k;
				if (dif < 0.001)
					break;			/* Don't bother looking further */
			}
		}
		
		if (bdif < 0.02) {

			/* Override the estimated combination values with the real values */
			for (j = 0; j < (3+p->spec_n); j++) {
#ifdef DEBUG
	printf("comb 0x%x band %d was %f ",i, j, p->pc[i][j]);
#endif
				p->pc[i][j] = p->cols[bk].band[j];
#ifdef DEBUG
	printf("best %d now %f\n",bk, p->pc[i][j]);
#endif
			}
		}
	}

	/* These initial primary combination values become the anchors during optimisation */
	for (i = 0; i < p->nn; i++) {
		for (j = 0; j < (3+p->spec_n); j++)
			p->lpca[i][j] = lDE(p->pc[i][j]);
	}

	/* Allocate and init shape related parameter space */
	init_shape(p);

	p->cord = 1;		/* Start with only 1 order */

#ifndef DEBUG
	if (p->verb) 
#endif /* !DEBUG */
	{
		deltae(p, &de, &mxde, &sde, &mxsde);
		printf("Before optimising model have average dE of %f, max %f\n", de, mxde);
		if (p->spec_n > 0) printf("and average spectral E of %f, max %f\n",sde, mxsde);
	}

	/* - - - - - - - - - - - - - - - - - */

#ifndef NOPROCESS
#ifdef NEVER		// Skip efunc1 passes for now.
	/* Do initial fast pass of optimisations */
	/* using only first transfer curve order */
	for (it = 0, p->ott = 3; it < p->ott; it++) {
		double resid;

		p->oit = it+1;

		/* First optimise each input channels transfer curve */
		for (k = 0; k < p->n; k++) {				/* For each input channel */
			double tw = 0.0;						/* Total weight */

			p->och = k;			/* device channel being optimised */

			/* Setup the appropriate weights */
			for (i = 0; i < p->nodp; i++) {
				mppcol *c = &p->cols[i];
				double ww; 
				int kk;

				for (ww = 0.0, kk = 0; kk < p->n; kk++) {
					if (kk == k)
						continue;		/* Channel of interest can have any value */
					ww += c->nv[kk] * c->nv[kk];
				}
				c->w = 1.0 - ww;
				if (c->w < 0.0)
					c->w = 0.0;
				tw += c->w;

//printf("~1 chan %d point %d weight %f\n",k,i,c->w);
			}

			if (tw < 3.0)
				error ("MPP - not enough weighted point");
			
			for (j = 0; j < (3+p->spec_n); j++) {	/* For all bands */
				double pv[MPP_MXTCORD];		/* Parameter values */
				double sr[MPP_MXTCORD];		/* search radius */

				p->oba = j;					/* Band being optimised */

				if (p->verb) {
					printf("Optimising device transfer curves channel %d band %d:\n",k,j);
				}
	
				/* Get the current values */
				for (i = 0; i < p->cord; i++) { 
					pv[i] = p->tc[k][j][i];
					sr[i] = 0.05;
				}

				sfunc1(p);	/* Setup test point values for this chan and band */

				if (powell(&resid, p->cord, pv, sr, 0.001, 100, efunc1, (void *)p, mppprog, (void *)p) != 0)
					error ("Powell failed");
	
				/* Put results back into place */
				for (i = 0; i < p->cord; i++) { 
					p->tc[k][j][i] = pv[i];
				}
			}
#ifndef DEBUG
		if (p->verb)
#endif /* !DEBUG */
			{
				deltae(p, &de, &mxde, &sde, &mxsde);
				printf("\nNow got avg dE of %f, max %f\n",de, mxde);
				if (p->spec_n > 0)
					printf("and avgerage spectral E of %f, max %f\n",sde, mxsde);
			}
		}
	}
#endif /* NEVER */

	p->cord = mxtcord;

	/* - - - - - - - - - - - - - - - - - */
	/* Fine tune all parameters in the model */
	for (mode = 0;;) {
		double pv[MPP_MXPARMS];		/* Parameter values */
		double sr[MPP_MXPARMS];		/* search radius */
		int lj, yj = 0;				/* Last band, peak Y band */
	
		/* Decide which band to do next */
		if (mode == 0) {				/* Start at the beginning */
			lj = -1;
			if (p->spec_n == 0) {
				mode = 3;				/* Switch to doing Y values */
				j = 1;
			} else {
				/* Start at the peak Y bandwidth */
				j = (int)(p->spec_n * (555.0 - p->spec_wl_short)
				        /(p->spec_wl_long - p->spec_wl_short) + 0.5);
				if (j < 0)
					j = 0;
				else if (j >= p->spec_n)
					j = p->spec_n-1;
				j += 3;
				yj = j;
				mode = 1;				/* Switch to incrementing j */
			}
		} else if (mode == 1) {			/* Increment j */
			lj = j;
			j++;
			if (j >= (3+p->spec_n)) {	/* We're finished moving up */
				lj = yj;
				j = yj-1;
				mode = 2;
				if (j < 3) {			/* Just in case */
					lj = yj;
					j = 1;
					mode = 3;
				}
			}
		} else if (mode == 2) {			/* Decrement j */
			lj = j;
			j--;
			if (j < 3) {
				lj = yj;				/* We know that spect is valid */
				j = 1;
				mode = 3;				/* Switch to Y mode */
			}
		} else if (mode == 3) {			/* Doing Y mode */
			if (spec_n > 0) {
				/* Use spec at the peak X bandwidth */
				lj = (int)(p->spec_n * (600.0 - p->spec_wl_short)
				        /(p->spec_wl_long - p->spec_wl_short) + 0.5);
				if (lj < 0)
					lj = 0;
				else if (lj >= p->spec_n)
					lj = p->spec_n-1;
				lj += 3;
			} else {
				lj = 1;
			}
			j = 0;						/* Doing X mode */
			mode = 4;
		} else if (mode == 4) {			/* Doing X mode */
			if (spec_n > 0) {
				/* Use spec at the peak Z bandwidth */
				lj = (int)(p->spec_n * (445.0 - p->spec_wl_short)
				        /(p->spec_wl_long - p->spec_wl_short) + 0.5);
				if (lj < 0)
					lj = 0;
				else if (lj >= p->spec_n)
					lj = p->spec_n-1;
				lj += 3;
			} else {
				lj = 1;
			}
			j = 2;						/* Doing Z mode */
			mode = 5;
		} else {
			break;						/* we're now done */
		}

		p->oba = j;					/* Band being optimised */

		if (p->verb)
			printf("Doing band %d, last band %d\n",j,lj);

		/* See if the last bands values are a good place to start */
		if (lj >= 0) {
			double cval, pval, p0val;

			banderr(p, &cval, NULL, j);		/* Current error */

			/* Copy previous band transfer and shape into current band */
			for (k = 0; k < p->n; k++)
				for (i = 0; i < p->cord; i++) {
					pv[k * p->cord + i] = p->tc[k][j][i];	/* Save current for restore */
					p->tc[k][j][i] = p->tc[k][lj][i];
				}
			for (i = 0; i < p->nnn2; i++) { 
				int m = p->c2f[i].ink;
				int n = p->c2f[i].comb;
				
				sr[i] = p->shape[m][n][j];
				p->shape[m][n][j] = p->shape[m][n][lj];
			}
			banderr(p, &pval, NULL, j);

			/* Try out the urrent order 0 transfer values with rest of transfer and shape */
			for (k = 0; k < p->n; k++)
				p->tc[k][j][0] = pv[k * p->cord];
			banderr(p, &p0val, NULL, j);

			/* See which was best out of the three */
			if (pval >= cval && p0val >= pval) {			/* Original was the best */
				for (k = 0; k < p->n; k++)
					for (i = 0; i < p->cord; i++)
						p->tc[k][j][i] = pv[k * p->cord + i];	/* Restore previous values */
				for (i = 0; i < p->nnn2; i++) { 
					int m = p->c2f[i].ink;
					int n = p->c2f[i].comb;
					p->shape[m][n][j] = sr[i];	/* Restore previous value */
				}
//printf("~1 Starting values were best (%f && %f > %f)\n",pval,p0val,cval);
			} else if (p0val >= pval) {						/* Copying all was best */

				for (k = 0; k < p->n; k++)
					p->tc[k][j][0] = p->tc[k][lj][0];		/* Back to order 0 values */

//printf("~1 copying all previous bands values was best (%f < %f && %f)\n",pval,cval,p0val);
			} else {
//printf("~1 copying except order 0 values was best (%f < %f && %f)\n",p0val,cval,pval);
			}
		}

#ifdef MULTIPASS	/* Multipass in parts */
		for (it = 0, p->ott = maxit, thr = 1.0; it < maxit; it++, thr *= 0.2) {
			double sde, mxsde;
			double resid;
	
			p->oit = it+1;
	
			/* Optimise main transfer curve to minimise each bands error */
			/* Initially using only first transfer curve order */

			if (p->verb)
				printf("Fine tuning device transfer curves itteration %d\n",it);
	
			/* Get the current values */
			for (k = 0; k < p->n; k++) {
				for (i = 0; i < p->cord; i++) { 
					pv[k * p->cord + i] = p->tc[k][j][i];
					sr[k * p->cord + i] = 0.05;
				}
			}
#ifdef TESTDFUNC
			test_dfunc2(p, p->n * p->cord, pv);
#endif /* TESTDFUNC */
#ifdef NODDV
			if (powell(&resid, p->n * p->cord, pv, sr, thr * 0.01, 200,
			                          efunc2, (void *)p, mppprog, (void *)p) != 0)
				error ("Powell failed");
#else /* !NODDV */
			if (conjgrad(&resid, p->n * p->cord, pv, sr, thr * 0.01, 200,
			                   efunc2, dfunc2, (void *)p, mppprog, (void *)p)!= 0)
				error ("ConjGrad failed");
#endif /* !NODDV */

			/* Put results back into place */
			for (k = 0; k < p->n; k++) {
				for (i = 0; i < p->cord; i++) 
					p->tc[k][j][i] = pv[k * p->cord + i];
			}

#ifndef DEBUG
			if (p->verb)
#endif /* !DEBUG */
			{
				banderr(p, &sde, &mxsde, j);		/* Current error */
				printf("\nNow got avg E of %f, max %f for this band\n",sde, mxsde);
			}

			p->cord = mxtcord;			/* maximum transfer curve order after very first run */

			/* Tune the shaping parameters */
			if (useshape) {

				if (p->verb)
					printf("Tuning detailed shaping parameters itteration %d\n",it);
		
				sfunc3(p);	/* Setup test point values for this band */

				/* Get the current values */
				for (i = 0; i < p->nnn2; i++) { 
					int m = p->c2f[i].ink;
					int n = p->c2f[i].comb;
					
					pv[i] = p->shape[m][n][j];
					sr[i] = 0.01;
				}

#ifdef TESTDFUNC
				test_dfunc3(p, p->nnn2, pv);
#endif /* TESTDFUNC */
#ifdef NODDV
				if (powell(&resid, p->nnn2, pv, sr, thr * 0.05, 2000,
				           efunc3, (void *)p, mppprog, (void *)p) != 0)
					error ("Powell failed");

#else /* !NODDV */
				if (conjgrad(&resid, p->nnn2, pv, sr, thr * 0.05, 2000,
				          efunc3, dfunc3, (void *)p, mppprog, (void *)p) != 0.0)
					error ("ConjGrad failed");
#endif /* !NODDV */

				/* Put results back into place */
				for (i = 0; i < p->nnn2; i++) { 
					int m = p->c2f[i].ink;
					int n = p->c2f[i].comb;
					
					p->shape[m][n][j] = pv[i];
//printf("~1 shape[%d][%d] = %f\n",m,n,pv[i]);
				}

#ifndef DEBUG
				if (p->verb)
#endif /* !DEBUG */
				{
					banderr(p, &sde, &mxsde, j);		/* Current error */
					printf("\nNow got avg E of %f, max %f for this band\n",sde, mxsde);
				}
#ifdef DEBUG
				dump_shape(p, 0, "After efunc3:");
#endif /* DEBUG */

				p->useshape = 1;		/* Would be nice to flag this on a per band basis */
			}

			/* Tune the vertex parameters */

			if (p->verb)
				printf("Optimising device combination values itteration %d\n",it);
		
			sfunc4(p);	/* Setup test point values for this band */

			/* Get the current values */
			for (k = 0; k < p->nn; k++) {
				pv[k] = p->pc[k][j];
				sr[k] = 0.01;
			}

#ifdef TESTDFUNC
			test_dfunc4(p, p->nn, pv);
#endif /* TESTDFUNC */
#ifdef NODDV
			if (powell(&resid, p->nn, pv, sr, thr * 0.01, 500,
			                 efunc4, (void *)p, mppprog, (void *)p) != 0)
				error ("Powell failed");
#else /* !NODDV */
			if (conjgrad(&resid, p->nn, pv, sr, thr * 0.01, 500,
			          efunc4, dfunc4, (void *)p, mppprog, (void *)p) != 0)
				error ("ConjGrad failed");
#endif /* !NODDV */

			/* Put results back into place */
			for (k = 0; k < p->nn; k++) {
				double pp = pv[k];
				if (pp < 0.0)
					pp = 0.0;
				p->pc[k][j] = pp;
			}

#ifndef DEBUG
			if (p->verb)
#endif /* !DEBUG */
			{
				banderr(p, &sde, &mxsde, j);		/* Current error */
				printf("\nNow got avg E of %f, max %f for this band\n",sde, mxsde);
			}
		}
#endif /* MULTIPASS	*/

#ifdef BIGBANG
		/* Do optimisation with one big bang */
		{
			double resid;
			double *pv2, *pv3, *pv4;	/* Pointers to each group of parameters */
			double *sr2, *sr3, *sr4;	/* Pointers to each group of search radius */
			int tparms = p->n * p->cord + p->nnn2 + p->nn;

			pv2 = pv;
			pv3 = pv + (p->n * p->cord);
			pv4 = pv + (p->n * p->cord) + p->nnn2;
			sr2 = sr;
			sr3 = sr + (p->n * p->cord);
			sr4 = sr + (p->n * p->cord) + p->nnn2;

			/* Get the current transfer values */
			for (k = 0; k < p->n; k++) {
				for (i = 0; i < p->cord; i++) { 
					pv2[k * p->cord + i] = p->tc[k][j][i];
					sr2[k * p->cord + i] = 0.005;
				}
			}
			/* Get the current shaper values */
			for (i = 0; i < p->nnn2; i++) { 
				int m = p->c2f[i].ink;
				int n = p->c2f[i].comb;
				
				pv3[i] = p->shape[m][n][j];
				sr3[i] = 0.005;
			}
			/* Get the current device combination values */
			for (k = 0; k < p->nn; k++) {
				pv4[k] = p->pc[k][j];
				sr4[k] = 0.005;
			}

#ifdef TESTDFUNC
			test_dfunc0(p, tparms, pv);
#endif /* TESTDFUNC */

			if (conjgrad(&resid, tparms, pv, sr, 0.001, 4000, efunc0, dfunc0, (void *)p,
			                                                     mppprog, (void *)p) != 0)
				error ("ConjGrad failed");

			/* Put results back into place */
			for (k = 0; k < p->n; k++) {
				for (i = 0; i < p->cord; i++) 
					p->tc[k][j][i] = pv2[k * p->cord + i];
			}
			for (i = 0; i < p->nnn2; i++) { 
				int m = p->c2f[i].ink;
				int n = p->c2f[i].comb;
				
				p->shape[m][n][j] = pv3[i];
			}
			for (k = 0; k < p->nn; k++) {
				double pp = pv4[k];
				if (pp < 0.0)
					pp = 0.0;
				p->pc[k][j] = pp;
			}

#ifndef DEBUG
			if (p->verb)
#endif /* !DEBUG */
			{
				banderr(p, &sde, &mxsde, j);		/* Current error */
				printf("\nNow got avg E of %f, max %f for this band\n",sde, mxsde);
			}
		}
#endif /* BIGBANG */
	}
#endif /* NOPROCESS */

#ifdef DEBUG
	if (p->verb)
#endif /* !DEBUG */
	{
		double de, mxde;
		deltae(p, &de, &mxde, &sde, &mxsde);
		printf("\nNow got avg dE of %f, max %f\n",de, mxde);
		if (p->spec_n > 0)
			printf("and avgerage spectral E of %f, max %f\n",sde, mxsde);
	}

#ifdef DOPLOT			/* Plot the device curves */
	{
#define	XRES 100
		double xx[XRES];
		double y1[XRES];

		for (i = 0; i < (3+p->spec_n); i++) {
			printf("Band %d:\n",i);
			for (j = 0; j < p->n; j++) {
				printf("Ink %d:\n",j);
	
				for (k = 0; k < XRES; k++) {
					double x;
					x = k/(double)(XRES-1);
					xx[k] = x;
					y1[k] = icxTransFunc(p->tc[j][i], p->cord, x);
				}
				do_plot(xx,y1,NULL,NULL,XRES);
			}
		}
	}
#endif /* DOPLOT */

	/* Figure out the white and black points */
	compute_wb(p);

	/* Done with our copy of the input points */
	free (p->cols);
	p->nodp = 0;
	p->cols = NULL;

	return 0;
}


#ifdef DEBUG

/* Dump current shape params to a file */
static void dump_shape(mpp *p, int first, char *title) {
	int i,j,k;
	FILE *df;
	/* Some debug code */

	if (first) {
		if ((df = fopen("debug.txt","w")) == NULL)
			error ("Failed to open debug.txt");
	} else {
		if ((df = fopen("debug.txt","a")) == NULL)
			error ("Failed to open debug.txt");
	}
	
	fprintf(df,"%s\n",title);

	for (i = 0; i < (3+p->spec_n); i++) {
		fprintf(df,"Band %d:\n",i);
		for (j = 0; j < p->n; j++) {
			fprintf(df,"Ink %d:\n",j);

			for (k = 0; k < p->nn; k++) {
				if ((k & (1<<j)) == 0) {
					int m, n;
					double val = 0.0;
					fprintf(df,"Comb %d = %f\n", k, p->shape[j][k][i]);
				}
			}
			fprintf(df,"\n");
		}
		fprintf(df,"\n");
	}
	fclose(df);
}

/* Combine two first order shapers coefficients together */
static double comb(double n1, double n2) {
	double nn;
	if (n1 > 0.0)
		n1 = (n1+1.0);
	else
		n1 = 1.0/(1.0-n1);

	if (n2 > 0.0)
		n2 = (n2+1.0);
	else
		n2 = 1.0/(1.0-n2);

	nn = n1 * n2;

	if (nn >= 1.0)
		nn -= 1.0;
	else
		nn = 1.0-(1.0/nn);

	return nn;
}

#endif /* DEBUG */

























