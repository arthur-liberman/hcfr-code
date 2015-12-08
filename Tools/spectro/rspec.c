
/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   20015
 *
 * Copyright 2006 - 2015 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 * 
 * Derived from i1pro_imp.c & munki_imp.c
 */

/*
 * A library for processing raw spectrometer values.
 *
 * Currently this is setup for the EX1 spectrometer,
 * but the longer term plan is to expand the functionality
 * so that it becomes more generic, and can replace a lot 
 * of common code in i1pro_imp.c & munki_imp.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#if defined(UNIX)
# include <utime.h>
#else
# include <sys/utime.h>
#endif
#include <sys/stat.h>
#include <stdarg.h>
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#else	/* !SALONEINSTLIB */
#include "sa_config.h"
#include "numsup.h"
#endif /* !SALONEINSTLIB */
#include "plot.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "inst.h"
#include "rspec.h"

/* -------------------------------------------------- */
#if defined(__APPLE__) && defined(__POWERPC__)

/* Workaround for a ppc gcc 3.3 optimiser bug... */
static int gcc_bug_fix(int i) {
	static int nn;
	nn += i;
	return nn;
}
#endif	/* APPLE */

/* -------------------------------------------------- */
/* Setup code */

/* Fit a wavelength polynomial to a set of mapping points */
// ~~~~9999

/* Completely clear an rspec_inf. */
void clear_rspec_inf(rspec_inf *inf) {
	memset(inf, 0, sizeof(rspec_inf));
}

/* Completely free contesnt of rspec_inf. */
void free_rspec_inf(rspec_inf *inf) {

	if (inf != NULL) {
		if (inf->straylight != NULL) {
			error("rspec_inf: help - don't know how to free straylight!");
		}
	
		if (inf->wlcal)
			free(inf->wlcal);
	
		if (inf->findex != NULL)
			free(inf->findex);
		if (inf->fnocoef != NULL)
			free(inf->fnocoef);
		if (inf->fcoef != NULL)
			free(inf->fcoef);
	
		if (inf->lin != NULL)
			free(inf->lin);
	
		if (inf->idark[0] != NULL)
			del_rspec(inf->idark[0]);
		if (inf->idark[1] != NULL)
			del_rspec(inf->idark[1]);
		
		if (inf->ecal != NULL)
			free(inf->ecal);
	
		clear_rspec_inf(inf);		/* In case it gets reused */
	}
}


/* return the number of samples for the given spectral type */
int rspec_typesize(rspec_inf *inf, rspec_type ty) {
	int no;
	if (ty == rspec_sensor)
		no = inf->nsen;
	else if (ty == rspec_raw)
		no = inf->nraw;
	else if (ty == rspec_wav)
		no = inf->nwav;
	else
		error("rspec_typesize type %d unknown",ty); 
	return no;
}

/* Compute the valid raw range from the calibration information */
void rspec_comp_raw_range_from_ecal(rspec_inf *inf) {
	int i;

	if (inf->ecaltype != rspec_raw)
		error("rspec_comp_raw_range_from_ecal: ecaltype not raw"); 

	for (i = 0; i < inf->nraw; i++) {
		if (inf->ecal[i] != 0.0) {
			inf->rawrange.off = i;
			break;
		}
	}
	if (i >= inf->nraw)
		error("rspec_comp_raw_range_from_ecal: ecal is zero");

	for (i = inf->rawrange.off; i < inf->nraw; i++) {
		if (inf->ecal[i] == 0.0) {
			break;
		}
	}
	inf->rawrange.num = i - inf->rawrange.off;
}

/* Convert a raw index to nm using polynomial */
double rspec_raw2nm(rspec_inf *inf, double rix) {
	int k;
	double wl;

	if (inf->nwlcal == 0)
		error("rspec_raw2nm: nwlcal == 0");

	/* Compute polinomial */
	for (wl = inf->wlcal[inf->nwlcal-1], k = inf->nwlcal-2; k >= 0; k--)
		wl = wl * rix + inf->wlcal[k];

	return wl;
}

/* Convert a cooked index to nm */
double rspec_wav2nm(rspec_inf *inf, double ix) {
	return inf->wl_short + ix * inf->wl_space;
}

/* -------------------------------------------------- */

/* Create a new rspec from scratch. */
/* Don't allocate samp if nmeas == 0 */
/* This always succeeds (i.e. application bombs if malloc fails) */
rspec *new_rspec(rspec_inf *inf, rspec_type ty, int nmeas) {
	rspec *p;
	int no;

	if ((p = (rspec *)calloc(1, sizeof(rspec))) == NULL) {
		error("Malloc failure in rspec()");
	}
	p->inf = inf;
	p->stype = ty;

	p->nmeas = nmeas;
	p->nsamp = rspec_typesize(inf, p->stype);
	if (nmeas > 0)
		p->samp = dmatrix(0, p->nmeas-1, 0, p->nsamp-1);

	return p;
}

/* Create a new rspec based on an existing prototype */ 
/* If nmeas == 0, create space for the same number or measurements */
rspec *new_rspec_proto(rspec *rs, int nmeas) {
	rspec *p;

	if ((p = (rspec *)calloc(1, sizeof(rspec))) == NULL) {
		error("Malloc failure in rspec()");
	}
	p->inf = rs->inf;
	p->stype = rs->stype;
	p->mtype = rs->mtype;
	p->state = rs->state;
	p->inttime = rs->inttime;

	if (nmeas == 0)
		p->nmeas = rs->nmeas;
	else
		p->nmeas = nmeas;
	p->nsamp = rs->nsamp;
	p->samp = dmatrix(0, p->nmeas-1, 0, p->nsamp-1);

	return p;
}

/* Create a new rspec by cloning an existing one */
rspec *new_rspec_clone(rspec *rs) {
	rspec *p;
	int i, j;

	if ((p = (rspec *)calloc(1, sizeof(rspec))) == NULL) {
		error("Malloc failure in rspec()");
	}
	p->inf = rs->inf;
	p->stype = rs->stype;
	p->mtype = rs->mtype;
	p->state = rs->state;
	p->inttime = rs->inttime;

	p->nmeas = rs->nmeas;
	p->nsamp = rs->nsamp;
	p->samp = dmatrix(0, p->nmeas-1, 0, p->nsamp-1);

	for (i = 0; i < p->nmeas; i++) {
		for (j = 0; j < p->nsamp; j++) {
			p->samp[i][j] = rs->samp[i][j];
		}
	}

	return p;
}

/* Free a rspec */
void del_rspec(rspec *p) {
	if (p != NULL) {
		if (p->samp != NULL)
			free_dmatrix(p->samp, 0, p->nmeas-1, 0, p->nsamp-1); 
		free(p);
	}
}

/* Plot the first rspec */
void plot_rspec1(rspec *p) {
	int i, no;
	double xx[RSPEC_MAXSAMP];
	double yy[RSPEC_MAXSAMP];

	no = rspec_typesize(p->inf, p->stype);
	
	for (i = 0; i < no; i++) {
		if (p->stype == rspec_wav)
			xx[i] = rspec_wav2nm(p->inf, (double)i);
		else
			xx[i] = (double)i;
		yy[i] = p->samp[0][i];
	}
	do_plot(xx, yy, NULL, NULL, no);
}

/* Plot the first rspec of 2 */
void plot_rspec2(rspec *p1, rspec *p2) {
	int i, no;
	double xx[RSPEC_MAXSAMP];
	double y1[RSPEC_MAXSAMP];
	double y2[RSPEC_MAXSAMP];

	// Should check p1 & p2 are compatible ??

	no = rspec_typesize(p1->inf, p1->stype);
	
	for (i = 0; i < no; i++) {
		if (p1->stype == rspec_wav)
			xx[i] = rspec_wav2nm(p1->inf, (double)i);
		else
			xx[i] = (double)i;
		y1[i] = p1->samp[0][i];
		y2[i] = p2->samp[0][i];
	}
	do_plot(xx, y1, y2, NULL, no);
}

void plot_ecal(rspec_inf *inf) {
	int i, no;
	double xx[RSPEC_MAXSAMP];
	double yy[RSPEC_MAXSAMP];

	no = rspec_typesize(inf, inf->ecaltype);
	
	for (i = 0; i < no; i++) {
		if (inf->ecaltype == rspec_wav)
			xx[i] = rspec_wav2nm(inf, (double)i);
		else
			xx[i] = (double)i;
		yy[i] = inf->ecal[i];
	}
	do_plot(xx, yy, NULL, NULL, no);
}


/* -------------------------------------------------- */

/* Return the largest value */
/* Optionally return the measurement and sample idex of that sample */
double largest_val_rspec(int *pmix, int *psix, rspec *raw) {
	double mx = -1e38;
	int mi = -1, mj = -1;
	int i, j;

	if (raw->nmeas <= 0)
		error("largest_val_rspec: raw has zero measurements");

	for (i = 0; i < raw->nmeas; i++) {
		for (j = 0; j < raw->nsamp; j++) {
			if (raw->samp[i][j] > mx) {
				mx = raw->samp[i][j];
				mi = i;
				mj = j;
			}
		}
	}
	if (pmix != NULL)
		*pmix = mi;
	if (psix != NULL)
		*psix = mj;

	return mx;
}

/* return a raw rspec from a sensor rspec */
/* (This does not make any adjustments to the values) */
rspec *extract_raw_from_sensor_rspec(rspec *sens) {
	rspec *raw;
	int off, i, j;

	if (sens->stype != rspec_sensor)
		error("extract_raw_from_sensor_rspec: input is not sensor type");

	raw = new_rspec(sens->inf, rspec_raw, sens->nmeas);

	raw->mtype = sens->mtype;
	raw->state = sens->state;
	raw->inttime = sens->inttime;

	off = sens->inf->lightrange.off;
	for (i = 0; i < raw->nmeas; i++) {
		for (j = 0; j < raw->nsamp; j++) {
			raw->samp[i][j] = sens->samp[i][off + j];
		}
	}

	return raw;
}

/* Return an interpolated dark reference value from idark */
double ex1_interp_idark_val(rspec_inf *inf, int mix, int six, double inttime) {
	double idv;
	double w0, w1;
	int i, j;

	w1 = (inttime - inf->idark[0]->inttime)/(inf->idark[1]->inttime - inf->idark[0]->inttime);
	w0 = 1.0 - w1;

	idv = w0 * inf->idark[0]->samp[mix][six] + w1 * inf->idark[1]->samp[mix][six];

	return idv;
}

/* Return an interpolated dark reference from idark */
rspec *ex1_interp_idark(rspec_inf *inf, double inttime) {
	double w0, w1;
	int i, j;
	rspec *dark;

	w1 = (inttime - inf->idark[0]->inttime)/(inf->idark[1]->inttime - inf->idark[0]->inttime);
	w0 = 1.0 - w1;

	dark = new_rspec_proto(inf->idark[0], 0);

	for (i = 0; i < inf->idark[0]->nmeas; i++) {
		for (j = 0; j < inf->idark[0]->nsamp; j++)
			dark->samp[i][j] = w0 * inf->idark[0]->samp[i][j] + w1 * inf->idark[1]->samp[i][j];
	}

	return dark;
}

/* Subtract the adaptive black */
void subtract_idark_rspec(rspec *raw) {
	rspec_inf *inf = raw->inf;
	int i, j;
	rspec *dark;

	if (raw->state & rspec_dcal)
		error("subtract_idark_rspec: already done");

	if (raw->stype != inf->idark[0]->stype)
		error("subtract_idark_rspect: idark does not match rspec type");

	dark = ex1_interp_idark(inf, raw->inttime);

	for (i = 0; i < raw->nmeas; i++) {
		for (j = 0; j < raw->nsamp; j++) {
			raw->samp[i][j] -= dark->samp[0][j];
		}
	}

	raw->state |= rspec_dcal;
}

/* Apply non-linearity */
double linearize_val_rspec(rspec_inf *inf, double ival) {
	double oval = ival;
	int k;

	if (ival >= 0.0) {
		for (oval = inf->lin[inf->nlin-1], k = inf->nlin-2; k >= 0; k--) {
			oval = oval * ival + inf->lin[k];
	 	}

		if (inf->lindiv)		/* EX1 divides */
			oval = ival/oval;
	}
	return oval;
}

/* Invert non-linearity. */
/* Since the linearisation is nearly a straight line, */
/* a simple Newton inversion will suffice. */
double inv_linearize_val_rspec(rspec_inf *inf, double targv) {
	double oval, ival = targv, del = 100.0;
	int i, k;

	for (i = 0; i < 200 && fabs(del) > 1e-7; i++) {
		for (oval = inf->lin[inf->nlin-1], k = inf->nlin-2; k >= 0; k--)
			oval = oval * ival + inf->lin[k];
		if (inf->lindiv)		/* EX1 divides */
			oval = ival/oval;

		del = (targv - oval);
		ival += 0.99 * del;
	}
	return ival;
}

/* Correct non-linearity */
void linearize_rspec(rspec *raw) {
	rspec_inf *inf = raw->inf;
	int i, j;
	rspec *dark;

	if (raw->state & rspec_lin)
		error("linearize_rspec: already done");

	if (raw->state & rspec_int)
		error("linearize_rspec: can't be integration time adjusted");

	if (!(raw->state & rspec_dcal))
		error("linearize_rspec: needs black subtract");

	if (inf->nlin > 0) {
		for (i = 0; i < raw->nmeas; i++) {
			for (j = 0; j < raw->nsamp; j++) {
				raw->samp[i][j] = linearize_val_rspec(inf, raw->samp[i][j]);
			}
		}
	}
	raw->state |= rspec_lin;
}

/* Apply the emsissive calibration */
void emis_calibrate_rspec(rspec *raw) {
	rspec_inf *inf = raw->inf;
	int i, j;

	if (raw->state & rspec_cal)
		error("emis_calibrate_rspec: already done");

	if (raw->stype != raw->inf->ecaltype)
		error("emis_calibrate_rspec: ecaltype does not match rspec type");

	for (i = 0; i < raw->nmeas; i++) {
		for (j = 0; j < raw->nsamp; j++) {
			raw->samp[i][j] *= inf->ecal[j];
		}
	}
	raw->state |= rspec_cal;
}

/* Scale to the integration time */
void inttime_calibrate_rspec(rspec *raw) {
	rspec_inf *inf = raw->inf;
	int i, j;

	if (raw->state & rspec_int)
		error("inttime_calibrate_rspec: already done");

	for (i = 0; i < raw->nmeas; i++) {
		for (j = 0; j < raw->nsamp; j++) {
			raw->samp[i][j] /= raw->inttime;
		}
	}

	raw->inttime = 1.0;

	raw->state |= rspec_int;
}

/* return a wav rspec from a raw rspec */
/* (This does not make any adjustments to the values) */
rspec *convert_wav_from_raw_rspec(rspec *raw) {
	rspec_inf *inf = raw->inf;
	rspec *wav;
	int cx, sx, i, j, k;

	if (raw->stype != rspec_raw)
		error("extract_raw_from_sensor_rspec: input is not raw type");

	wav = new_rspec(raw->inf, rspec_wav, raw->nmeas);

	wav->mtype = raw->mtype;
	wav->state = raw->state;
	wav->inttime = raw->inttime;

	for (i = 0; i < wav->nmeas; i++) {				/* For each measurement */
		for (cx = j = 0; j < inf->nwav; j++) {		/* For each wav sample */
			double oval = 0.0;
	
			sx = inf->findex[j];		/* Starting index */
			for (k = 0; k < inf->fnocoef[j]; k++, cx++, sx++)	/* For each matrix value */
				oval += inf->fcoef[cx] * raw->samp[i][sx];
			wav->samp[i][j] = oval;
		}
	}

	return wav;
}

/* -------------------------------------------------- */


/* Filter code in i1pro_imp is in:

	i1pro_compute_wav_filters()		X-Rite way
	i1pro_create_hr()				Using gausian

 */

/* Resampling kernels. (There are more in i1pro_imp.c) */

/* They aren't expected to be unity area, as they will be */
/* normalized anyway. */
/* wi is the width of the filter */

static double triangle(double wi, double x) {
	double y = 0.0;

	x = fabs(x/wi);
	y = 1.0 - x;
	if (y < 0.0)
		y = 0.0;

	return y;
}

static double gausian(double wi, double x) {
	double y = 0.0;

	wi = wi/(sqrt(2.0 * log(2.0)));	/* Convert width at half max to std. dev. */
    x = x/wi;
    y = exp(-(x * x));											/* Center at 1.0 */

	return y;
}

static double lanczos2(double wi, double x) {
	double y = 0.0;

	wi *= 1.05;			// Improves smoothness. Why ?

	x = fabs(1.0 * x/wi);
	if (x >= 2.0)
		return 0.0; 
	if (x < 1e-6)
		return 1.0;
	y = sin(DBL_PI * x)/(DBL_PI * x) * sin(DBL_PI * x/2.0)/(DBL_PI * x/2.0);

	return y;
}

static double lanczos3(double wi, double x) {
	double y = 0.0;

	x = fabs(1.0 * x/wi);
	if (x >= 3.0)
		return 0.0; 
	if (x < 1e-6)
		return 1.0;
	y = sin(DBL_PI * x)/(DBL_PI * x) * sin(DBL_PI * x/3.0)/(DBL_PI * x/3.0);

	return y;
}

static double cubicspline(double wi, double x) {
	double y = 0.0;
	double xx = x;
	double bb, cc;

	xx = fabs(1.0 * x/wi);

//	bb = cc = 1.0/3.0;		/* Mitchell */
	bb = 0.5;
	cc = 0.5;

	if (xx < 1.0) {
		y = ( 12.0 -  9.0 * bb - 6.0 * cc) * xx * xx * xx
		  + (-18.0 + 12.0 * bb + 6.0 * cc) * xx * xx
		  + (  6.0 -  2.0 * bb);
		y /= (6.0 - 2.0 * bb);
	} else if (xx < 2.0) {
		y = ( -1.0 * bb -  6.0 * cc) * xx * xx * xx
		  + (  6.0 * bb + 30.0 * cc) * xx * xx
		  + (-12.0 * bb - 48.0 * cc) * xx 
		  + (  8.0 * bb + 24.0 * cc);
		y /= (6.0 - 2.0 * bb);
	} else {
		y = 0.0;
	}

	return y;
}

/* Create the wavelength resampling filters */
void rspec_make_resample_filters(rspec_inf *inf) {
	double twidth = inf->wl_space;
	double rawspace;				/* Average raw band spacing wl */
	double fshmax;					/* filter shape max wavelength from center */
	double finc;					/* Integration step size */
	int maxcoeffs;					/* Maximum coefficients per filter */
	int    **coeff_ix;				/* [band][coef] Raw index */
	double **coeff_we;				/* [band][coef] Weighting */
	double (*kernel)(double wi, double x) = NULL;	/* Filter kernel */
	int xcount;
	int i, j, k;
	
	if (inf->ktype == rspec_triangle)
		kernel = triangle; 
	else if (inf->ktype == rspec_gausian)
		kernel = gausian; 
	else if (inf->ktype == rspec_lanczos2)
		kernel = lanczos2; 
	else if (inf->ktype == rspec_lanczos3)
		kernel = lanczos3; 
	else if (inf->ktype == rspec_cubicspline)
		kernel = cubicspline; 
	else
		error("rspec_make_resample_filters: unknown kernel %d",inf->ktype); 
	
#ifdef NEVER		// Check kernel sums to 1.0
{
	double x, y;

	for (x = 0.0; x < 5.0; x += 0.1) {
		y = kernel(1.0, x - 4.0)
		  + kernel(1.0, x - 3.0)
		  + kernel(1.0, x - 2.0)
		  + kernel(1.0, x - 1.0)
		  + kernel(1.0, x)
		  + kernel(1.0, x + 1.0)
		  + kernel(1.0, x + 2.0);
		  + kernel(1.0, x + 3.0);
		  + kernel(1.0, x + 4.0);

		printf("Offset %f sum %f\n",x,y);
	}

}
#endif 	// NEVER

	/* Aproximate raw value spacing in nm */
	rawspace = (inf->wl_long - inf->wl_short)/inf->rawrange.num;
//printf("~1 rawspace = %f\n",rawspace);

	/* Figure the extent of the filter kernel. We assume they */
	/* all have a finite extent. */
	for (fshmax = 50.0; fshmax >= 0.0; fshmax -= 0.01) {
		if (fabs(kernel(twidth, fshmax)) > 1e-6) {
			fshmax += 0.01;
			break;
		}
	}
//printf("~1 fshmax = %f\n",fshmax);

	if (fshmax <= 0.0)
		error("rspec_make_resample_filters: fshmax search failed\n");

	a1logd(inf->log, 4,"rspec_make_resample_filters: fshmax = %f\n",fshmax);

	/* Figure number of raw samples over kernel extent. */
	/* (Allow generous factor for non-linearity) */
	maxcoeffs = (int)floor(2.0 * 1.4 * fshmax/rawspace + 3.0);

	a1logd(inf->log, 4,"rspec_make_resample_filters: maxcoeffs = %d\n",maxcoeffs);

	/* Figure out integration step size */
#ifdef FAST_HIGH_RES_SETUP
	finc = twidth/50.0;
	if (rawspace/finc < 10.0)
		finc = rawspace/10.0;
#else
	finc = twidth/15.0;
	if (rawspace/finc < 4.0)
		finc = rawspace/4.0;
#endif

	a1logd(inf->log, 4,"rspec_make_resample_filters: integration step = %f\n",finc);

	if (inf->fnocoef != NULL)
		free(inf->fnocoef);
	if ((inf->fnocoef = (int *)calloc(inf->nwav, sizeof(int))) == NULL)
		error("rspec_make_resample_filters: malloc failure");

	/* Space to build filter coeficients */
	coeff_ix = imatrix(0, inf->nwav-1, 0, maxcoeffs-1);
	coeff_we = dmatrix(0, inf->nwav-1, 0, maxcoeffs-1);

	/* For all the usable raw bands */
	for (i = inf->rawrange.off+1; i < (inf->rawrange.off+inf->rawrange.num-1); i++) {
		double w1, wl, w2;

		/* Translate CCD center and boundaries to calibrated wavelength */
		wl = rspec_raw2nm(inf, (double)i);
		w1 = rspec_raw2nm(inf, (double)i - 0.5);
		w2 = rspec_raw2nm(inf, (double)i + 0.5);

//		printf("~1 CCD %d, w1 %f, wl %f, w2 %f\n",i,w1,wl,w2);

		/* For each output filter */
		for (j = 0; j < inf->nwav; j++) {
			double cwl, rwl;		/* center, relative wavelegth */
			double we;

			cwl = rspec_wav2nm(inf, (double)j);
			rwl = wl - cwl;			/* raw relative wavelength to filter */

			if (fabs(w1 - cwl) > fshmax && fabs(w2 - cwl) > fshmax)
				continue;		/* Doesn't fall into this filter */

			/* Integrate in finc nm increments from filter shape */
			/* using triangular integration. */
			{
				int nn;
				double lw, ll;

				nn = (int)(fabs(w2 - w1)/finc + 0.5);		/* Number to integrate over */
		
				lw = w1;				/* start at lower boundary of CCD cell */
				ll = kernel(twidth, w1 - cwl);
				we = 0.0;
				for (k = 0; k < nn; k++) { 
					double cw, cl;

#if defined(__APPLE__) && defined(__POWERPC__)
					gcc_bug_fix(k);
#endif
					cw = w1 + (k+1.0)/(nn + 1.0) * fabs(w2 - w1);	/* wl to sample */
					cl = kernel(twidth, cw - cwl);
					we += 0.5 * (cl + ll) * fabs(lw - cw);			/* Area under triangle */
					ll = cl;
					lw = cw;
				}
			}

			if (inf->fnocoef[j] >= maxcoeffs)
				error("rspec_make_resample_filters: run out of high res filter space\n");

			coeff_ix[j][inf->fnocoef[j]] = i;
			coeff_we[j][inf->fnocoef[j]++] = we; 
//			printf("~1 filter %d, cwl %f, rwl %f, ix %d, we %f, nocoefs %d\n",j,cwl,rwl,i,we,info->fnocoef[j]);
		}
	}

	/* Convert hires filters into runtime format: */

	/* Allocate or reallocate high res filter tables */
	if (inf->findex != NULL)
		free(inf->findex);
	if (inf->fcoef != NULL)
		free(inf->fcoef);

	if ((inf->findex = (int *)calloc(inf->nraw, sizeof(int))) == NULL)
		error("rspec_make_resample_filters: malloc index failed!\n");

	/* Count the total number of coefficients */
	for (xcount = j = 0; j < inf->nwav; j++) {
		inf->findex[j] = coeff_ix[j][0];		/* raw starting index */ 
		xcount += inf->fnocoef[j];
	}

//printf("~1 total coefs = %d\n",xcount);

	/* Allocate space for them */
	if ((inf->fcoef = (double *)calloc(xcount, sizeof(double))) == NULL)
		error("rspec_make_resample_filters: malloc index failed!\n");

	/* Normalize the weight * nm to 1.0, and pack them into the run-time format */
	for (i = j = 0; j < inf->nwav; j++) {
		int sx;
		double rwi, twe = 0.0;

		sx = inf->findex[j];		/* raw starting index */

		for (k = 0; k < inf->fnocoef[j]; sx++, k++) {
			/* Width of raw band in nm */
			rwi = fabs(rspec_raw2nm(inf, (double)sx - 0.5)
			         - rspec_raw2nm(inf, (double)sx + 0.5));
			twe += rwi * coeff_we[j][k];
		}

		if (twe > 0.0)
			twe = 1.0/twe;
		else
			twe = 1.0;

//		printf("Output %d, nocoefs %d, norm weight %f:\n",j,inf->fnocoef[j],twe);

		for (k = 0; k < inf->fnocoef[j]; k++, i++) {
			inf->fcoef[i] = coeff_we[j][k] * twe;
//			printf("  coef %d packed %d from raw %d val %f\n",k,i,inf->findex[j]+k,inf->fcoef[i]);
		}
	}

	free_imatrix(coeff_ix, 0, inf->nwav-1, 0, maxcoeffs-1);
	free_dmatrix(coeff_we, 0, inf->nwav-1, 0, maxcoeffs-1);
}

//printf("~1 line %d\n",__LINE__);

/* Plot the wave resampling filters */
void plot_resample_filters(rspec_inf *inf) {
	double *xx, *ss;
	double **yy;
	int i, j, k, sx;

//printf("~1 nraw = %d\n",inf->nraw);

	xx = dvectorz(0, inf->nraw-1);		/* X index */
	yy = dmatrixz(0, 5, 0, inf->nraw-1);	/* Curves distributed amongst 5 graphs */
									/* with 6th holding sum */
	for (i = 0; i < inf->nraw; i++)
		xx[i] = i;

	/* For each output wavelength */
	for (i = j = 0; j < inf->nwav; j++) {

		sx = inf->findex[j];		/* raw starting index */
//printf("Output %d raw sx %d\n",j,sx);

		/* For each matrix value */
		for (k = 0; k < inf->fnocoef[j]; k++, sx++, i++) {
			yy[5][sx] += 0.5 * inf->fcoef[i];
			yy[j % 5][sx] = inf->fcoef[i];
//printf(" filter %d six %d weight = %e\n",k,sx,inf->fcoef[i]);
		}
	}

	printf("Wavelength re-sampling curves:\n");
//	do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], 150);
	do_plot6(xx, yy[0], yy[1], yy[2], yy[3], yy[4], yy[5], inf->nraw);
	free_dvector(xx, 0, inf->nraw-1);
	free_dmatrix(yy, 0, 2, 0, inf->nraw-1);
}

/* ================================================== */
/* Calibration file support */

/* Open the file. nz wr for write mode, else read */
/* Return nz on error */
int calf_open(calf *x, a1log *log, char *fname, int wr) {
	char nmode[10];
	char cal_name[200];
	char **cal_paths = NULL;
	int no_paths = 0;

	memset((void *)x, 0, sizeof(calf));
	x->log = log;

	if (wr)
		strcpy(nmode, "w");
	else
		strcpy(nmode, "r");

#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
	strcat(nmode, "b");
#endif

	/* Create the file name */
	if (wr)
		sprintf(cal_name, "ArgyllCMS/%s", fname);
	else
		sprintf(cal_name, "ArgyllCMS/%s" SSEPS "color/%s", fname, fname);
	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_write, xdg_user, cal_name)) < 1) {
		a1logd(x->log,1,"calf_open: xdg_bds returned no paths\n");
		return 1;
	}

	a1logd(x->log,2,"calf_open: %s file '%s'\n",cal_paths[0], wr ? "saving to" : "restoring from");

	/* Check the last modification time */
	if (!wr) {
		struct sys_stat sbuf;

		if (sys_stat(cal_paths[0], &sbuf) == 0) {
			x->lo_secs = time(NULL) - sbuf.st_mtime;
			a1logd(x->log,2,"calf_open:: %d secs from instrument last open\n",x->lo_secs);
		} else {
			a1logd(x->log,2,"calf_open:: stat on file failed\n");
		}
	}

	if ((wr && create_parent_directories(cal_paths[0]))
	 || (x->fp = fopen(cal_paths[0], nmode)) == NULL) {
		a1logd(x->log,2,"calf_open: failed to open file for %s\n",wr ? "writing" : "reading");
		xdg_free(cal_paths, no_paths);
		return 1;
	}
	xdg_free(cal_paths, no_paths);

	a1logd(x->log,2,"calf_open: suceeded\n");

	return 0;
}

/* Update the modification time */
/* Return nz on error */
int calf_touch(a1log *log, char *fname) {
	char cal_name[200];
	char **cal_paths = NULL;
	int no_paths = 0;
	int rv;

	/* Locate the file name */
	sprintf(cal_name, "ArgyllCMS/%s" SSEPS "color/%s", fname, fname);

	if ((no_paths = xdg_bds(NULL, &cal_paths, xdg_cache, xdg_read, xdg_user, cal_name)) < 1) {
		a1logd(log,2,"calf_touch: xdg_bds failed to locate file'\n");
		return 1;
	}

	a1logd(log,2,"calf_touch: touching file '%s'\n",cal_paths[0]);

	if ((rv = sys_utime(cal_paths[0], NULL)) != 0) {
		a1logd(log,2,"calf_touch: failed with %d\n",rv);
		xdg_free(cal_paths, no_paths);
		return 1;
	}
	xdg_free(cal_paths, no_paths);

	return 0;
}

/* Rewind and reset for another read */
void calf_rewind(calf *x) {
	x->ef = 0;
	x->chsum = 0;
	x->nbytes = 0;
	rewind(x->fp);
}

/* Close the file and free any memory */
/* return nz on error */
int calf_done(calf *x) {
	int rv = 0;

	if (x->fp != NULL) {
		if (fclose(x->fp)) {
			a1logd(x->log,2,"calf_done: closing file failed\n");
			rv = 1;
		}
	}

	if (x->buf != NULL)
		free(x->buf);
	x->buf = NULL;
	return rv;
}

static void sizebuf(calf *x, size_t size) {
	if (x->bufsz < size)
		x->buf = realloc(x->buf, size);
	if (x->buf == NULL)
		error("calf: sizebuf malloc failed");
}

static void update_chsum(calf *x, unsigned char *p, int nn) {
	int i;
	for (i = 0; i < nn; i++, p++)
		x->chsum = ((x->chsum << 13) | (x->chsum >> (32-13))) + *p;
	x->nbytes += nn;
}

/* Write an array of ints to the file. Set the error flag to nz on error */
void calf_wints(calf *x, int *dp, int n) {
	if (x->ef)
		return;

	if (fwrite((void *)dp, sizeof(int), n, x->fp) != n) {
		x->ef = 1;
		a1logd(x->log,2,"calf_wints: write failed for %d ints at offset %d\n",n,x->nbytes);
	} else {
		update_chsum(x, (unsigned char *)dp, sizeof(int) * n);
	}
}

/* Write an array of doubles to the file. Set the error flag to nz on error */
void calf_wdoubles(calf *x, double *dp, int n) {
	if (x->ef)
		return;

	if (fwrite((void *)dp, sizeof(double), n, x->fp) != n) {
		x->ef = 1;
		a1logd(x->log,2,"calf_wdoubles: write failed for %d doubles at offset %d\n",n,x->nbytes);
	} else {
		update_chsum(x, (unsigned char *)dp, sizeof(double) * n);
	}
}

/* Write an array of time_t's to the file. Set the error flag to nz on error */
/* (This will cause file checksum fail if different executables on the same */
/*  system have different time_t values) */
void calf_wtime_ts(calf *x, time_t *dp, int n) {
	if (x->ef)
		return;

	if (fwrite((void *)dp, sizeof(time_t), n, x->fp) != n) {
		x->ef = 1;
		a1logd(x->log,2,"calf_wtime_ts: write failed for %d time_ts at offset %d\n",n,x->nbytes);
	} else {
		update_chsum(x, (unsigned char *)dp, sizeof(time_t) * n);
	}
}

/* Write a zero terminated string */
void calf_wstrz(calf *x, char *dp) {
	int n;

	if (x->ef)
		return;

	n = strlen(dp) + 1;

	calf_wints(x, &n, 1);

	if (fwrite((void *)dp, sizeof(char), n, x->fp) != n) {
		x->ef = 1;
		a1logd(x->log,2,"calf_wstrz: write failed for %d long string at offset %d\n",n,x->nbytes);
	} else {
		update_chsum(x, (unsigned char *)dp, sizeof(char) * n);
	}
}


/* Read an array of ints from the file. Set the error flag to nz on error */
/* Always read (ignore rd flag) */
void calf_rints2(calf *x, int *dp, int n) {
	if (x->ef)
		return;

	if (fread((void *)dp, sizeof(int), n, x->fp) != n) {
		x->ef = 1;
		a1logd(x->log,2,"calf_rints2: read failed for %d ints at offset %d\n",n,x->nbytes);
	} else {
		update_chsum(x, (unsigned char *)dp, sizeof(int) * n);
	}
}


/* Read an array of ints from the file. Set the error flag to nz on error */
void calf_rints(calf *x, int *dp, int n) {
	size_t nbytes = n * sizeof(int);
	void *dest = (void *)dp;

	if (x->ef)
		return;

	if (x->rd == 0) {		/* Dummy read */
		sizebuf(x, nbytes);
		dest = x->buf;
	}

	if (fread(dest, 1, nbytes, x->fp) != nbytes) {
		x->ef = 1;
		a1logd(x->log,2,"calf_rints: read failed for %d ints at offset %d\n",n,x->nbytes);
	} else {
		update_chsum(x, dest, nbytes);
	}
}

/* Read an array of doubles from the file. Set the error flag to nz on error */
void calf_rdoubles(calf *x, double *dp, int n) {
	size_t nbytes = n * sizeof(double);
	void *dest = (void *)dp;

	if (x->ef)
		return;

	if (x->rd == 0) {		/* Dummy read */
		sizebuf(x, nbytes);
		dest = x->buf;
	}

	if (fread(dest, 1, nbytes, x->fp) != nbytes) {
		x->ef = 1;
		a1logd(x->log,2,"calf_rdoubles: read failed for %d ints at offset %d\n",n,x->nbytes);
	} else {
		update_chsum(x, dest, nbytes);
	}
}

/* Read an array of time_t's from the file. Set the error flag to nz on error */
/* (This will cause file checksum fail if different executables on the same */
/*  system have different time_t values) */
void calf_rtime_ts(calf *x, time_t *dp, int n) {
	size_t nbytes = n * sizeof(time_t);
	void *dest = (void *)dp;

	if (x->ef)
		return;

	if (x->rd == 0) {		/* Dummy read */
		sizebuf(x, nbytes);
		dest = x->buf;
	}

	if (fread(dest, 1, nbytes, x->fp) != nbytes) {
		x->ef = 1;
		a1logd(x->log,2,"calf_rtime_ts: read failed for %d ints at offset %d\n",n,x->nbytes);
	} else {
		update_chsum(x, dest, nbytes);
	}
}

/* Read a zero terminated string. */
void calf_rstrz(calf *x, char **dp) {
	int n;
	size_t nbytes = 0;
	char *dest = NULL;

	if (x->ef)
		return;

	calf_rints2(x, &n, 1);
	nbytes = sizeof(char) * n;

	if (x->ef || n == 0)
		return;

	if (x->rd != 0) {	/* Reading for real */
		if (*dp != NULL)
			free(*dp);
		if ((*dp = dest = malloc(nbytes)) == NULL)
			error("calf: calf_rstrz malloc failed");
	} else {
		sizebuf(x, nbytes);
		dest = x->buf;
	}
	if (fread(dest, 1, nbytes, x->fp) != nbytes) {
		x->ef = 1;
		a1logd(x->log,2,"calf_rstrz: read failed for %d long string at offset %d\n",n,x->nbytes);
	} else {
		update_chsum(x, (unsigned char*)dest, nbytes);
	}
}

void calf_rstrz2(calf *x, char **dp) {
	int rd = x->rd;
	x->rd = 1;
	calf_rstrz(x, dp);
	x->rd = rd;
}

/* ================================================== */

/* Save a rspec to a calibration file */
void calf_wrspec(calf *x, rspec *s) {
	int i;

	if (x->ef)
		return;

	calf_wints(x, (int *)&s->stype, 1);
	calf_wints(x, (int *)&s->mtype, 1);
	calf_wints(x, (int *)&s->state, 1);
	calf_wdoubles(x, &s->inttime, 1);
	calf_wints(x, &s->nmeas, 1);
	calf_wints(x, &s->nsamp, 1);

	for (i = 0; i < s->nmeas; i++) {
		calf_wdoubles(x, s->samp[i], s->nsamp);
	}
}

/* Create a rspec from a calibration file */
void calf_rrspec(calf *x, rspec **dp, rspec_inf *inf) {
	rspec *s, dumy;
	int no, i;

	if (x->ef)
		return;

	if (x->rd != 0) {
		if (*dp != NULL)
			del_rspec(*dp);
		*dp = s = new_rspec(inf, rspec_sensor, 0);	
	} else {
		s = &dumy;
	}
	
	calf_rints2(x, (int *)&s->stype, 1);
	calf_rints2(x, (int *)&s->mtype, 1);
	calf_rints2(x, (int *)&s->state, 1);
	calf_rdoubles(x, &s->inttime, 1);
	calf_rints2(x, &s->nmeas, 1);
	calf_rints2(x, &s->nsamp, 1);

	/* Sanity check. */
	no = rspec_typesize(inf, s->stype);
	if (no != s->nsamp) {
		a1logd(inf->log, 4,"calf_rrspec: unexpected nsamp %d (expect %d)\n",s->nsamp,no);
		x->ef = 1;
		return;
	}

	if (x->rd != 0) {
		s->samp = dmatrix(0, s->nmeas-1, 0, s->nsamp-1);
		for (i = 0; i < s->nmeas; i++) {
			calf_rdoubles(x, s->samp[i], s->nsamp);
		}
	} else {
		for (i = 0; i < s->nmeas; i++) {
			calf_rdoubles(x, NULL, s->nsamp);
		}
	}
}

