
 /* Spectrometer/Colorimeter color spot reader utility */

/* 
 * Argyll Color Correction System
 * Author: Graeme W. Gill
 * Date:   3/10/2001
 *
 * Derived from printread.c/chartread.c
 * Was called printspot.
 *
 * Copyright 2001 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* This program reads a spot reflection/transmission/emission value using */
/* a spectrometer or colorimeter. */

/* TTBD
 *
 *  Add option to automatically read continuously, until stopped. (A bit like -O)

 *	Make -V average the spectrum too (if present), and allow it to
 *  be saved to a .sp file.
 *
 *  Should fix plot so that it is a separate object running its own thread,
 *  so that it can be sent a graph without needing to be clicked in all the time.
 *
 *  Should add option to show reflective/tranmsission density values.
 *
 *  Should add option for Y u' v' values.
 */

#undef DEBUG
#undef TEST_EVENT_CALLBACK		/* Report async event callbacks */

#define COMPORT 1		/* Default com port 1..4 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#ifndef SALONEINSTLIB
# include "copyright.h"
# include "aconfig.h"
# include "numlib.h"
# include "cgats.h"
# include "xicc.h"
# include "conv.h"
# include "plot.h"
# include "ui.h"
#else /* SALONEINSTLIB */
# include "sa_config.h"
# include "numsup.h"
# include "xspect.h"
# include "conv.h"
#endif /* SALONEINSTLIB */
#include "inst.h"
#include "icoms.h"
#include "ccss.h"
#include "ccmx.h"
#include "instappsup.h"
#if !defined(NOT_ALLINSTS) || defined(EN_SPYD2)
# include "spyd2.h"
#endif

#if defined (NT)
#include <conio.h>
#endif

#undef DO_TM3015_PLOT	/* Diagnostic */

/* ----------------------------------------------------------------- */

#ifdef DO_TM3015_PLOT

#pragma message("#### spectro/spotread.c DO_TM3015_PLOT is enabled ####")

#define SSAMP 4

// Note that bins is modified
static void tm3015_plot(double bins[IES_TM_30_15_BINS][2][3]) {
	double rvecs[SSAMP * 16][2];	// DEBUG ref light circle vectors
	double tvecs[SSAMP * 16][2];	// test light circle vectors
	double shvec[2 * 16][2];		// Shift vectors
	int i, j, k;
	double maxr = 0.0;
	plot_g gg = { 0 };
	float lblack[3] = { 0.5, 0.5, 0.5 };
	float lred[3]   = { 1.0, 0.5, 0.5 };
	float black[3] = { 0.0, 0.0, 0.0 };
	float red[3]   = { 1.0, 0.0, 0.0 };
	float blue[3] = { 0.2, 0.2, 1.0 };

#ifdef NEVER
	clear_g(&gg);

	for (i = 0; i < 16; i++) {
		int ip1 = i < 15 ? i+1 : 0; 
		
		add_vec_g(&gg, bins[i][0][1], bins[i][0][2], bins[ip1][0][1], bins[ip1][0][2], lblack);
		add_vec_g(&gg, bins[i][1][1], bins[i][1][2], bins[ip1][1][1], bins[ip1][1][2], lred);
	}
	do_plot_g(&gg, 0.0, 0.0, 0.0, 0.0, 1.0, 0, 1);
#endif

	clear_g(&gg);

	/* Copy raw shift vectors */
	for (i = 0; i < 16; i++) {
		shvec[2 * i + 0][0] = bins[i][0][1];
		shvec[2 * i + 0][1] = bins[i][0][2];
		shvec[2 * i + 1][0] = bins[i][1][1];
		shvec[2 * i + 1][1] = bins[i][1][2];
	}

	// Convert to angle & radius 
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 2; j++) {
			double x = bins[i][j][1];
			double y = bins[i][j][2];

			// atan2 returns -pi < val <= +pi
			bins[i][j][1] = atan2(y, x);
			bins[i][j][2] = sqrt(x * x + y * y);
		}
	}

	// Interpolate test points, and normalize them by interp. reference points */
	for (i = 0; i < 16; i++) {
		int ip1 = i < 15 ? i+1 : 0; 
		int k = i, kp1 = ip1;
		for (j = 0; j < SSAMP; j++) {	// 4 x interpolation
			double ra, rr, ta, tr;
			double ra0, ra1, ta0, ta1;
			double bl = (double)j/SSAMP;
			double nf = 1.0;

//printf("vec %d i %d j %d bl %f\n",SSAMP * i + j, i, j, bl);

			ra0 = bins[i][0][1];
			ra1 = bins[ip1][0][1];
			ta0 = bins[i][1][1];
			ta1 = bins[ip1][1][1];

//printf("raw ra0 %f ra1 %f ta0 %f ta1 %f\n", ra0, ra1, ta0, ta1);

			// Make sure pairs are ordered
			if ((ra1 - ra0) > (1.0 * DBL_PI))
				ra0 += 2.0 * DBL_PI;
			else if ((ra1 - ra0) < -(1.0 * DBL_PI))
				ra0 -= 2.0 * DBL_PI;

			if ((ta1 - ta0) > (1.0 * DBL_PI))
				ta0 += 2.0 * DBL_PI;
			else if ((ta1 - ta0) < -(1.0 * DBL_PI))
				ta0 -= 2.0 * DBL_PI;

//printf("fix ra0 %f ra1 %f ta0 %f ta1 %f\n", ra0, ra1, ta0, ta1);

			// Interpolated values
			ra = (1.0 - bl) * ra0 + bl * ra1;
			rr = (1.0 - bl) * bins[i][0][2] + bl * bins[ip1][0][2];
			ta = (1.0 - bl) * ta0 + bl * ta1;
			tr = (1.0 - bl) * bins[i][1][2] + bl * bins[ip1][1][2];

//printf(" raw  ra %f rr %f ta %f tr %f\n",ra, rr, ta, tr);

			nf = rr;		// Normalization factor

			tr /= nf;
			rr /= nf;

			if (tr > maxr)
				maxr = tr;

//printf(" norm ra %f rr %f ta %f tr %f\n",ra, rr, ta, tr);

			rvecs[SSAMP * i + j][0] = ra;
			rvecs[SSAMP * i + j][1] = rr; 

			tvecs[SSAMP * i + j][0] = ta;
			tvecs[SSAMP * i + j][1] = tr; 

			// Set shift vectors
			if (j == 0) {
				shvec[2 * i + 0][0] = ra;
				shvec[2 * i + 0][1] = rr;
				shvec[2 * i + 1][0] = ta; 
				shvec[2 * i + 1][1] = tr; 
//printf(" shft ra %f rr %f ta %f tr %f\n",ra, rr, ta, tr);
			}
//printf("\n");
		}
	}

//printf("Done all, convert back\n");

	// Convert back to cartesian
	for (i = 0; i < (SSAMP * 16); i++) {
		double a = tvecs[i][0];
		double r = tvecs[i][1];

		tvecs[i][0] = r * cos(a); 
		tvecs[i][1] = r * sin(a);
//printf(" tvecs[%d] a %f r %f x %f y %f\n",i,a,r,tvecs[i][0],tvecs[i][1]);
	}

	for (i = 0; i < (SSAMP * 16); i++) {
		double a = rvecs[i][0];
		double r = rvecs[i][1];

		rvecs[i][0] = r * cos(a); 
		rvecs[i][1] = r * sin(a);
//printf(" rvecs[%d] a %f r %f x %f y %f\n",i,a,r,rvecs[i][0],rvecs[i][1]);
	}

	for (i = 0; i < (2 * 16); i++) {
		double a = shvec[i][0];
		double r = shvec[i][1];

		shvec[i][0] = r * cos(a); 
		shvec[i][1] = r * sin(a);
//printf(" shvec[%d] a %f r %f x %f y %f\n",i,a,r,shvec[i][0],shvec[i][1]);
	}

//printf("About to plot\n");

	for (i = 0; i < (SSAMP * 16); i++) {
		int ip1 = i < (SSAMP * 16 -1) ? i+1 : 0; 
		double x0, y0, x1, y1, r0, r1;

		x0 = tvecs[i][0];
		y0 = tvecs[i][1];
		x1 = tvecs[ip1][0];
		y1 = tvecs[ip1][1];

		r0 = sqrt(x0 * x0 + y0 * y0);
		r1 = sqrt(x1 * x1 + y1 * y1);

		add_vec_g(&gg, x0/r0, y0/r0, x1/r1, y1/r1, black);
//		add_vec_g(&gg, rvecs[i][0], rvecs[i][1], rvecs[ip1][0], rvecs[ip1][1], black);

		add_vec_g(&gg, x0, y0, x1, y1, red);

	}

	for (i = 0; i < 16; i++) {
		add_vec_g(&gg, shvec[2 * i + 0][0], shvec[2 * i + 0][1],
		               shvec[2 * i + 1][0], shvec[2 * i + 1][1], blue);
	}

	do_plot_g(&gg, 0.0, 0.0, 0.0, 0.0, 1.0, 0, 1);
	
}

#undef SSAMP

#endif /* DO_TM3015_PLOT */

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#ifdef NEVER	/* Not currently used */
/* Convert control chars to ^[A-Z] notation in a string */
static char *
fix_asciiz(char *s) {
	static char buf [200];
	char *d;
	for(d = buf; ;) {
		if (*s < ' ' && *s > '\000') {
			*d++ = '^';
			*d++ = *s++ + '@';
		} else
		*d++ = *s++;
		if (s[-1] == '\000')
			break;
	}
	return buf;
}
#endif

#ifdef SALONEINSTLIB

#define D50_X_100  96.42
#define D50_Y_100 100.00
#define D50_Z_100  82.49

/* CIE XYZ 0..100 to perceptual D50 CIE 1976 L*a*b* */
static void
XYZ2Lab(double *out, double *in) {
	double X = in[0], Y = in[1], Z = in[2];
	double x,y,z,fx,fy,fz;

	x = X/D50_X_100;
	y = Y/D50_Y_100;
	z = Z/D50_Z_100;

	if (x > 0.008856451586)
		fx = pow(x,1.0/3.0);
	else
		fx = 7.787036979 * x + 16.0/116.0;

	if (y > 0.008856451586)
		fy = pow(y,1.0/3.0);
	else
		fy = 7.787036979 * y + 16.0/116.0;

	if (z > 0.008856451586)
		fz = pow(z,1.0/3.0);
	else
		fz = 7.787036979 * z + 16.0/116.0;

	out[0] = 116.0 * fy - 16.0;
	out[1] = 500.0 * (fx - fy);
	out[2] = 200.0 * (fy - fz);
}

/* Return the normal Delta E given two Lab values */
static double LabDE(double *Lab0, double *Lab1) {
	double rv = 0.0, tt;

	tt = Lab0[0] - Lab1[0];
	rv += tt * tt;
	tt = Lab0[1] - Lab1[1];
	rv += tt * tt;
	tt = Lab0[2] - Lab1[2];
	rv += tt * tt;
	
	return sqrt(rv);
}

/* Lab to LCh */
void Lab2LCh(double *out, double *in) {
	double C, h;

	C = sqrt(in[1] * in[1] + in[2] * in[2]);

    h = (180.0/3.14159265359) * atan2(in[2], in[1]);
	h = (h < 0.0) ? h + 360.0 : h;

	out[0] = in[0];
	out[1] = C;
	out[2] = h;
}

/* XYZ to Yxy */
static void XYZ2Yxy(double *out, double *in) {
	double sum = in[0] + in[1] + in[2];
	double Y, x, y;

	if (sum < 1e-9) {
		Y = 0.0;
		y = 0.0;
		x = 0.0;
	} else {
		Y = in[1];
		x = in[0]/sum;
		y = in[1]/sum;
	}
	out[0] = Y;
	out[1] = x;
	out[2] = y;
}

#endif /* SALONEINSTLIB */

/* Replacement for gets */
char *getns(char *buf, int len) {
	int i;
	if (fgets(buf, len, stdin) == NULL)
		return NULL;

	for (i = 0; i < len; i++) {
		if (buf[i] == '\n') {
			buf[i] = '\000';
			return buf;
		}
	}
	buf[len-1] = '\000';
	return buf;
}

/* Deal with an instrument error. */
/* Return 0 to retry, 1 to abort */
static int ierror(inst *it, inst_code ic) {
	int ch;
	empty_con_chars();
	printf("Got '%s' (%s) error.\nHit Esc or Q to give up, any other key to retry:",
	       it->inst_interp_error(it, ic), it->interp_error(it, ic));
	fflush(stdout);
	ch = next_con_char();
	printf("\n");
	if (ch == 0x03 || ch == 0x1b || ch == 'q' || ch == 'Q')	/* Escape, ^C or Q */
		return 1;
	return 0;
}

#ifdef TEST_EVENT_CALLBACK
	void test_event_callback(void *cntx, inst_event_type event) {
		a1logd(g_log,0,"Got event_callback with 0x%x\n",event);
	}
#endif

#if defined(__APPLE__) && defined(__POWERPC__)

/* Workaround for a ppc gcc 3.3 optimiser bug... */
static int gcc_bug_fix(int i) {
	static int nn;
	nn += i;
	return nn;
}
#endif	/* APPLE */

/* A user callback to trigger for -O option */
static inst_code uicallback(void *cntx, inst_ui_purp purp) {

	if (purp == inst_armed)
		return inst_user_trig;
	return inst_ok;
}

/*

  Flags used:

         ABCDEFGHIJKLMNOPQRSTUVWXYZ
  upper     ... .  .  .. . .. ...  
  lower  . .... ..      .  .. .... 

*/

void
usage(char *diag, ...) {
//	int i;
	icompaths *icmps;
	inst2_capability cap2 = 0;
	fprintf(stderr,"Measure spot values, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the GPL Version 2 or later\n");
	if (diag != NULL) {
		va_list args;
		fprintf(stderr,"Diagnostic: ");
		va_start(args, diag);
		vfprintf(stderr, diag, args);
		va_end(args);
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"usage: spotread [-options] [logfile]\n");
	fprintf(stderr," -v                   Verbose mode\n");
	fprintf(stderr," -s                   Print spectrum for each reading\n");
#ifndef SALONEINSTLIB
	fprintf(stderr," -S                   Plot spectrum for each reading\n");
#endif /* !SALONEINSTLIB */
	fprintf(stderr," -c listno            Set instrument port from the following list (default %d)\n",COMPORT);
	if ((icmps = new_icompaths(g_log)) != NULL) {
		icompath **paths;
		if ((paths = icmps->paths) != NULL) {
			int i;
			for (i = 0; ; i++) {
				if (paths[i] == NULL)
					break;
#if !defined(NOT_ALLINSTS) || defined(EN_SPYD2)
				if ((paths[i]->dtype == instSpyder1 && setup_spyd2(0) == 0)
				 || (paths[i]->dtype == instSpyder2 && setup_spyd2(1) == 0))
					fprintf(stderr,"    %d = '%s' !! Disabled - no firmware !!\n",i+1,paths[i]->name);
				else
#endif
					fprintf(stderr,"    %d = '%s'\n",i+1,paths[i]->name);
			}
		} else
			fprintf(stderr,"    ** No ports found **\n");
	}
	fprintf(stderr," -t                   Use transmission measurement mode\n");
	fprintf(stderr," -e                   Use emissive measurement mode (absolute results)\n");
	fprintf(stderr," -eb                  Use display white brightness relative measurement mode\n");
	fprintf(stderr," -ew                  Use display white point relative chromatically adjusted mode\n");
	fprintf(stderr," -p                   Use telephoto measurement mode (absolute results)\n");
	fprintf(stderr," -pb                  Use projector white brightness relative measurement mode\n");
	fprintf(stderr," -pw                  Use projector white point relative chromatically adjusted mode\n");
	fprintf(stderr," -a                   Use ambient measurement mode (absolute results)\n");
	fprintf(stderr," -f                   Use ambient flash measurement mode (absolute results)\n");
	cap2 = inst_show_disptype_options(stderr, " -y                   ", icmps, 0);
#ifndef SALONEINSTLIB
	fprintf(stderr," -I illum             Set simulated instrument illumination using FWA (def -i illum):\n");
	fprintf(stderr,"                       M0, M1, M2, A, C, D50, D50M2, D65, F5, F8, F10 or file.sp]\n");
#endif
	fprintf(stderr," -i illum             Choose illuminant for computation of CIE XYZ from spectral reflectance & FWA:\n");
#ifndef SALONEINSTLIB
	fprintf(stderr,"                       A, C, D50 (def.), D50M2, D65, F5, F8, F10 or file.sp\n");
#else
	fprintf(stderr,"                       A, C, D50 (def.), D65\n");
#endif
	fprintf(stderr," -Q observ            Choose CIE Observer for spectral data or CCSS instrument:\n");
#ifndef SALONEINSTLIB
	fprintf(stderr,"                      1931_2 (def), 1964_10, 2012_2, 2012_10, S&B 1955_2, shaw, J&V 1978_2 or file.cmf\n");
#else
	fprintf(stderr,"                      1931_2 (def), 1964_10, 2012_2, 2012_10 or file.cmf\n");
#endif
#ifndef SALONEINSTLIB
	fprintf(stderr,"                      (Choose FWA during operation)\n");
#endif
	fprintf(stderr," -F filter            Set filter configuration (if aplicable):\n");
	fprintf(stderr,"    n                  None\n");
	fprintf(stderr,"    p                  Polarising filter\n");
	fprintf(stderr,"    6                  D65\n");
	fprintf(stderr,"    u                  U.V. Cut\n");
	fprintf(stderr," -E customfilter.sp   Compensate for emission measurement filter\n");
	fprintf(stderr," -A N|A|X|G           XRGA conversion (default N)\n");
#ifndef SALONEINSTLIB
	fprintf(stderr," -w                   Use -i param. illuminant for comuting L*a*b*\n");
#endif
	fprintf(stderr," -x                   Display Yxy instead of Lab\n");
	fprintf(stderr," -h                   Display LCh instead of Lab\n");
#ifndef SALONEINSTLIB
	fprintf(stderr," -u                   Display Yuv instead of Lab\n");
#endif
	fprintf(stderr," -V                   Show running average and std. devation from ref.\n");
#ifndef SALONEINSTLIB
	fprintf(stderr," -T                   Display correlated color temperatures, CRI, TLCI & IES TM-30-15\n");
#endif /* !SALONEINSTLIB */
//	fprintf(stderr," -K type              Run instrument calibration first\n");
	fprintf(stderr," -N                   Disable auto calibration of instrument\n");
#ifndef SALONEINSTLIB
	fprintf(stderr," -O [fname.sp]        Do one cal. or measure and exit [save spectrum to file]\n");
#else
	fprintf(stderr," -O                   Do one cal. or measure and exit\n");
#endif
	fprintf(stderr," -H                   Start in high resolution spectrum mode (if available)\n");
	if (cap2 & inst2_ccmx)
		fprintf(stderr," -X file.ccmx         Apply Colorimeter Correction Matrix\n");
	if (cap2 & inst2_ccss) {
		fprintf(stderr," -X file.ccss         Use Colorimeter Calibration Spectral Samples for calibration\n");
	}
#ifndef SALONEINSTLIB
	fprintf(stderr," -R fname.sp          Preset reference to spectrum\n");
#endif
	fprintf(stderr," -Y r|n               Override refresh, non-refresh display mode\n");
	fprintf(stderr," -Y R:rate            Override measured refresh rate with rate Hz\n");
	fprintf(stderr," -Y A                 Use non-adaptive integration time mode (if available).\n");
	fprintf(stderr," -Y l|L               Test for i1Pro Lamp Drift (l), and remediate it (L)\n");
	fprintf(stderr," -Y a                 Use Averaging mode (if available).\n");
//	fprintf(stderr," -Y U                 Test i1pro2 UV measurement mode\n");
#ifndef SALONEINSTLIB
	fprintf(stderr," -Y W:fname.sp        Save white tile ref. spectrum to file\n");
#endif	/* !SALONEINSTLIB */
	fprintf(stderr," -W n|h|x             Override serial port flow control: n = none, h = HW, x = Xon/Xoff\n");
	fprintf(stderr," -D [level]           Print debug diagnostics to stderr\n");
	fprintf(stderr," logfile              Optional file to save reading results as text\n");

	if (icmps != NULL)
		icmps->del(icmps);
	exit(1);
}

int main(int argc, char *argv[]) {
	int i, j;
	int fa, nfa, mfa;				/* current argument we're looking at */
	int verb = 0;
	int debug = 0;
	int docalib = 0;				/* Do a manual instrument calibration */
	int nocal = 0;					/* Disable auto calibration */
	int doone = 0;					/* 1 = Do one calibration or measure and exit */
									/* 2 = + also save result to outspname */
	int pspec = 0;					/* 1 = Print out the spectrum for each reading */
									/* 2 = Plot out the spectrum for each reading */
	int trans = 0;					/* Use transmissioin mode */
	int emiss = 0;					/* 1 = Use emissive mode, 2 = display bright rel. */
	                                /* 3 = display white rel. */
	int tele = 0;					/* 1 = Use telephoto emissive sub-mode. */
	int ambient = 0;				/* 1 = Use ambient emissive mode, 2 = ambient flash mode */
	int highres = 0;				/* Use high res mode if available */
	int lampdrift = 0;				/* i1Pro Lamp Drift test (1) & fix (2) */
	int uvmode = 0;					/* ~~~ i1pro2 test mode ~~~ */
	xcalstd calstd = xcalstd_none;	/* X-Rite calibration standard */
	int refrmode = -1;				/* -1 = default, 0 = non-refresh mode, 1 = refresh mode */
	double refrate = 0.0;			/* 0.0 = default, > 0.0 = override refresh rate */ 
	int nadaptive = 0;				/* Use non-apative mode if available */
	int averagemode = 0;			/* Use averaging mode if available */
	int doYxy= 0;					/* Display Yxy instead of Lab */
	int doLCh= 0;					/* Display LCh instead of Lab */
#ifndef SALONEINSTLIB
	int doYuv= 0;					/* Display Yuv instead of Lab */
#endif
	int doCCT= 0;					/* Display correlated color temperatures */
	inst_mode mode = 0, smode = 0;	/* Normal mode and saved readings mode */
	inst_opt_type trigmode = inst_opt_unknown;	/* Chosen trigger mode */
	inst_opt_filter fe = inst_opt_filter_unknown;
									/* Filter configuration */
	char outname[MAXNAMEL+1] = "\000";  /* Output logfile name */
	char ccxxname[MAXNAMEL+1] = "\000";  /* Colorimeter Correction/Colorimeter Calibration name */
	char filtername[MAXNAMEL+1] = "\000";  /* Filter compensation */
	char wtilename[MAXNAMEL+1] = "\000";  /* White file spectrum */
	char psetrefname[MAXNAMEL+1] = "\000";  /* Preset reference spectrum */
	char outspname[MAXNAMEL+1] = "\000";  /* Save doone spectrum file */
	FILE *fp = NULL;				/* Logfile */
	icompaths *icmps = NULL;
	int comport = COMPORT;				/* COM port used */
	icompath *ipath = NULL;
	int ditype = 0;					/* Display type selection character(s) */
	inst_mode cap = inst_mode_none;		/* Instrument mode capabilities */
	inst2_capability cap2 = inst2_none;	/* Instrument capabilities 2 */
	inst3_capability cap3 = inst3_none;	/* Instrument capabilities 3 */
	double lx, ly;					/* Read location on xy table */
	baud_rate br = baud_38400;		/* Target baud rate */
	flow_control fc = fc_nc;		/* Default flow control */
	inst *it;						/* Instrument object */
	inst_code rv;
	int uswitch = 0;				/* Instrument switch is enabled */
	int spec = 0;					/* Need spectral data for observer/illuminant flag */
	int tillum_set = 0;				/* User asked for custom target illuminant spectrum */
	icxIllumeType tillum = icxIT_none;	/* Target/simulated instrument illuminant */ 
	xspect cust_tillum, *tillump = NULL; /* Custom target/simulated illumination spectrum */
	int illum_set = 0;				/* User asked for custom illuminant spectrum */
	icxIllumeType illum = icxIT_D50;	/* Spectral defaults */
	xspect cust_illum;				/* Custom illumination spectrum */
	int labwpillum = 0;				/* nz to use illum WP for L*a*b* conversion */
	icmXYZNumber labwp = { icmD50_100.X, icmD50_100.Y, icmD50_100.Z };	/* Lab conversion wp */
	char labwpname[100] = "D50";	/* Name of Lab conversion wp */
	icxObserverType obType = icxOT_default;		/* Default is 1931_2 */
	xspect custObserver[3];			/* If obType = icxOT_custom */
	xspect sp;						/* Last spectrum read (fwa adjusted) */
	xspect rsp;						/* Reference spectrum (fwa adjusted) */
	xsp2cie *sp2cie = NULL;			/* default conversion */
	xsp2cie *sp2cief[26];			/* FWA corrected conversions */
	double wXYZ[3] = { -10.0, 0, 0 };/* White XYZ for display white relative */
//	double chmat[3][3];				/* Chromatic adapation matrix for white point relative */
	double XYZ[3] = { 0.0, 0.0, 0.0 };		/* Last XYZ scaled 0..100 or absolute */
	double Lab[3] = { -10.0, 0, 0};	/* Last Lab */
	double rXYZ[3] = { 0.0, -10.0, 0};	/* Reference XYZ */
	double rLab[3] = { -10.0, 0, 0};	/* Reference Lab */
	double Yxy[3] = { 0.0, 0, 0};	/* Yxy value */
	double LCh[3] = { 0.0, 0, 0};	/* LCh value */
#ifndef SALONEINSTLIB
	double Yuv[3] = { 0.0, 0, 0};	/* Yuv value */
#endif
	double refstats = 0;			/* Print running avg & stddev against ref */
	double rstat_n;					/* Stats N */
	double rstat_XYZ[3];			/* Stats sum of XYZ's */
	double rstat_XYZsq[3];			/* Stats sum of XYZ's squared */
	double rstat_Lab[3];			/* Stats sum of Lab's */
	double rstat_Labsq[3];			/* Stats sum of Lab's squared */
	int savdrd = 0;					/* At least one saved reading is available */
	int ix;							/* Reading index number */
	int loghead = 0;				/* NZ if log file heading has been printed */

	set_exe_path(argv[0]);			/* Set global exe_path and error_program */
	check_if_not_interactive();

	for (i = 0; i < 26; i++)
		sp2cief[i] = NULL;

	sp.spec_n = 0;
	rsp.spec_n = 0;

	/* Process the arguments */
	mfa = 0;        /* Minimum final arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-') {	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1+mfa) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == 'D') {
				debug = 1;
				if (na != NULL && na[0] >= '0' && na[0] <= '9') {
					debug = atoi(na);
					fa = nfa;
				}
				g_log->debug = debug;
			} else if (argv[fa][1] == '?') {
				usage("Usage requested");

			} else if (argv[fa][1] == 'v') {
				verb = 1;
				g_log->verb = verb;

			} else if (argv[fa][1] == 's') {
				pspec = 1;

#ifndef SALONEINSTLIB
			} else if (argv[fa][1] == 'S') {
				pspec = 2;
#endif /* !SALONEINSTLIB */

			/* COM port  */
			} else if (argv[fa][1] == 'c') {
				fa = nfa;
				if (na == NULL) usage("Paramater expected following -c");
				{
					comport = atoi(na);
					if (comport < 1 || comport > 40) usage("-c parameter %d out of range",comport);
				}

			/* Display type */
			} else if (argv[fa][1] == 'y') {
				fa = nfa;
				if (na == NULL) usage("Paramater expected following -y");
				ditype = na[0];
				if (ditype == '_' && na[1] != '\000')
					ditype = ditype << 8 | na[1];

#ifndef SALONEINSTLIB
			/* Simulated instrument illumination (FWA) */
			} else if (argv[fa][1] == 'I') {

				fa = nfa;
				if (na == NULL) usage("Paramater expected following -I");
				if (strcmp(na, "A") == 0
				 || strcmp(na, "M0") == 0) {
					tillum_set = spec = 1;
					tillum = icxIT_A;
				} else if (strcmp(na, "C") == 0) {
					tillum_set = spec = 1;
					tillum = icxIT_C;
				} else if (strcmp(na, "D50") == 0
				        || strcmp(na, "M1") == 0) {
					tillum_set = spec = 1;
					tillum = icxIT_D50;
				} else if (strcmp(na, "D50M2") == 0
				        || strcmp(na, "M2") == 0) {
					tillum_set = spec = 1;
					tillum = icxIT_D50M2;
				} else if (strcmp(na, "D65") == 0) {
					tillum_set = spec = 1;
					tillum = icxIT_D65;
				} else if (strcmp(na, "F5") == 0) {
					tillum_set = spec = 1;
					tillum = icxIT_F5;
				} else if (strcmp(na, "F8") == 0) {
					tillum_set = spec = 1;
					tillum = icxIT_F8;
				} else if (strcmp(na, "F10") == 0) {
					tillum_set = spec = 1;
					tillum = icxIT_F10;
				} else {	/* Assume it's a filename */
					inst_meas_type mt;

					tillum_set = spec = 1;
					tillum = icxIT_custom;
					if (read_xspect(&cust_tillum, &mt, na) != 0)
						usage("Failed to read custom target illuminant spectrum in file '%s'",na);

					if (mt != inst_mrt_none
					 && mt != inst_mrt_emission
					 && mt != inst_mrt_ambient
					 && mt != inst_mrt_emission_flash
					 && mt != inst_mrt_ambient_flash)
						error("Target illuminant '%s' is wrong measurement type",na);
				}
#endif /* !SALONEINSTLIB */

			/* Spectral Illuminant type for XYZ computation */
			} else if (argv[fa][1] == 'i') {
				fa = nfa;
				if (na == NULL) usage("Paramater expected following -i");
				if (strcmp(na, "A") == 0) {
					illum_set = spec = 1;
					illum = icxIT_A;
				} else if (strcmp(na, "C") == 0) {
					illum_set = spec = 1;
					illum = icxIT_C;
				} else if (strcmp(na, "D50") == 0) {
					illum_set = spec = 1;
					illum = icxIT_D50;
				} else if (strcmp(na, "D50M2") == 0) {
					illum_set = spec = 1;
					illum = icxIT_D50M2;
				} else if (strcmp(na, "D65") == 0) {
					illum_set = spec = 1;
					illum = icxIT_D65;
#ifndef SALONEINSTLIB
				} else if (strcmp(na, "F5") == 0) {
					illum_set = spec = 1;
					illum = icxIT_F5;
				} else if (strcmp(na, "F8") == 0) {
					illum_set = spec = 1;
					illum = icxIT_F8;
				} else if (strcmp(na, "F10") == 0) {
					illum_set = spec = 1;
					illum = icxIT_F10;
				} else {	/* Assume it's a filename */
					inst_meas_type mt;

					illum_set = spec = 1;
					illum = icxIT_custom;
					if (read_xspect(&cust_illum, &mt, na) != 0)
						usage("Unable to read custom illuminant file '%s'",na);

					if (mt != inst_mrt_none
					 && mt != inst_mrt_emission
					 && mt != inst_mrt_ambient
					 && mt != inst_mrt_emission_flash
					 && mt != inst_mrt_ambient_flash)
						error("Custom illuminant '%s' is wrong measurement type",na);
				}
#else /* SALONEINSTLIB */
				} else
					usage("Unrecognised illuminant '%s'",na);
#endif /* SALONEINSTLIB */

#ifndef SALONEINSTLIB
			/* Use -i illuminant for L*a*b* conversion */
			} else if (argv[fa][1] == 'w') {
				labwpillum = 1;
#endif /* !SALONEINSTLIB */

			/* Spectral Observer type */
			} else if (argv[fa][1] == 'Q') {
				fa = nfa;
				if (na == NULL) usage("Paramater expected following -Q");
				if (strcmp(na, "1931_2") == 0) {			/* Classic 2 degree */
					obType = icxOT_CIE_1931_2;
				} else if (strcmp(na, "1964_10") == 0) {	/* Classic 10 degree */
					obType = icxOT_CIE_1964_10;
				} else if (strcmp(na, "2012_2") == 0) {		/* Latest 2 degree */
					obType = icxOT_CIE_2012_2;
				} else if (strcmp(na, "2012_10") == 0) {	/* Latest 10 degree */
					obType = icxOT_CIE_2012_10;
#ifndef SALONEINSTLIB
				} else if (strcmp(na, "1955_2") == 0) {		/* Stiles and Burch 1955 2 degree */
					obType = icxOT_Stiles_Burch_2;
				} else if (strcmp(na, "1978_2") == 0) {		/* Judd and Voss 1978 2 degree */
					obType = icxOT_Judd_Voss_2;
				} else if (strcmp(na, "shaw") == 0) {		/* Shaw and Fairchilds 1997 2 degree */
					obType = icxOT_Shaw_Fairchild_2;
#endif /* !SALONEINSTLIB */
				} else {
					obType = icxOT_custom;
					if (read_cmf(custObserver, na) != 0)
						usage(0,"Failed to read custom observer CMF from -Q file '%s'",na);
				}

			/* Request transmission measurement */
			} else if (argv[fa][1] == 't') {
				emiss = 0;
				trans = 1;
				tele = 0;
				ambient = 0;		/* Default normal diffuse/90 geometry trans. */

			/* Request emissive measurement */
			} else if (argv[fa][1] == 'e' || argv[fa][1] == 'd') {

				if (argv[fa][1] == 'd')
					warning("spotread -d flag is deprecated");

				emiss = 1;
				trans = 0;
				tele = 0;
				ambient = 0;

				if (argv[fa][2] != '\000') {
					if (argv[fa][2] == 'b' || argv[fa][2] == 'B')
						emiss = 2;
					else if (argv[fa][2] == 'w' || argv[fa][2] == 'W')
						emiss = 3;
					else
						usage("-d modifier '%c' not recognised",argv[fa][2]);
				}

			/* Request telephoto measurement */
			} else if (argv[fa][1] == 'p') {

				emiss = 1;
				trans = 0;
				tele = 1;
				ambient = 0;

				if (argv[fa][2] != '\000') {
					fa = nfa;
					if (argv[fa][2] == 'b' || argv[fa][2] == 'B')
						emiss = 2;		/* Display brightness relative */
					else if (argv[fa][2] == 'w' || argv[fa][2] == 'W')
						emiss = 3;		/* Display white point relative */
					else
						usage("-p modifier '%c' not recognised",argv[fa][2]);
				}

			/* Request ambient measurement */
			} else if (argv[fa][1] == 'a') {
				if (trans) {
					ambient = 1;		/* Alternate 90/diffuse geometry */
				} else {
					emiss = 1;
					trans = 0;
					tele = 0;
					ambient = 1;
				}

			/* Request ambient flash measurement */
			} else if (argv[fa][1] == 'f') {
				emiss = 1;
				trans = 0;
				tele = 0;
				ambient = 2;

			/* Filter configuration */
			} else if (argv[fa][1] == 'F') {
				fa = nfa;
				if (na == NULL) usage("Paramater expected following -F");
				if (na[0] == 'n' || na[0] == 'N')
					fe = inst_opt_filter_none;
				else if (na[0] == 'p' || na[0] == 'P')
					fe = inst_opt_filter_pol;
				else if (na[0] == '6')
					fe = inst_opt_filter_D65;
				else if (na[0] == 'u' || na[0] == 'U')
					fe = inst_opt_filter_UVCut;
				else
					usage("-F type '%c' not recognised",na[0]);

			/* Extra filter compensation file */
			} else if (argv[fa][1] == 'E') {
				fa = nfa;
				if (na == NULL) usage("Paramater expected following -E");
				strncpy(filtername,na,MAXNAMEL-1); filtername[MAXNAMEL-1] = '\000';

			/* XRGA conversion */
			} else if (argv[fa][1] == 'A') {
				fa = nfa;
				if (na == NULL) usage("Paramater expected following -A");
				if (na[0] == 'N')
					calstd = xcalstd_none;
				else if (na[0] == 'A')
					calstd = xcalstd_xrga;
				else if (na[0] == 'X')
					calstd = xcalstd_xrdi;
				else if (na[0] == 'G')
					calstd = xcalstd_gmdi;
				else
					usage("Paramater after -A '%c' not recognized",na[0]);

			/* Show Yxy */
			} else if (argv[fa][1] == 'x') {
				doYxy = 1;
				doLCh = 0;
#ifndef SALONEINSTLIB
				doYuv = 0;
#endif

			/* Show LCh */
			} else if (argv[fa][1] == 'h') {
				doYxy = 0;
				doLCh = 1;
#ifndef SALONEINSTLIB
				doYuv = 0;
#endif

#ifndef SALONEINSTLIB
			/* Show Yuv */
			} else if (argv[fa][1] == 'u') {
				doYxy = 0;
				doLCh = 0;
				doYuv = 1;
#endif

			/* Compute running average and standard deviation from ref. */
			/* Also turns off clamping */
			} else if (argv[fa][1] == 'V') {
				refstats = 1;
#ifndef SALONEINSTLIB

			/* Show CCT etc. */
			} else if (argv[fa][1] == 'T') {
				doCCT = 1;
#endif /* !SALONEINSTLIB */

			/* Manual calibration */
			} else if (argv[fa][1] == 'K') {

				if (na != NULL && na[0] >= '0' && na[0] <= '9') {
					docalib = atoi(na);
					fa = nfa;
				} else
					usage("-K parameter '%c' not recognised",na[0]);

			/* No auto calibration */
			} else if (argv[fa][1] == 'N') {
				nocal = 1;

			/* Do one cal. or measure and exit */
			} else if (argv[fa][1] == 'O') {
				doone = 1;
#ifndef SALONEINSTLIB
				if (na != NULL) {
					strncpy(outspname,na,MAXNAMEL-1); outspname[MAXNAMEL-1] = '\000';
					doone = 2;
					fa = nfa;
				}
#endif

			/* High res mode */
			} else if (argv[fa][1] == 'H') {
				highres = 1;

			/* Colorimeter Correction Matrix or */
			/* Colorimeter Calibration Spectral Samples */
			} else if (argv[fa][1] == 'X') {
//				int ix;
				fa = nfa;
				if (na == NULL) usage("Parameter expected after -K");
				strncpy(ccxxname,na,MAXNAMEL-1); ccxxname[MAXNAMEL-1] = '\000';

#ifndef SALONEINSTLIB
			/* Preset reference spectrum */
			} else if (argv[fa][1] == 'R') {
				fa = nfa;
				if (na == NULL)
					usage("-R fname.sp syntax incorrect");
				strncpy(psetrefname,na,MAXNAMEL-1); psetrefname[MAXNAMEL-1] = '\000';
#endif

			/* Extra flags */
			} else if (argv[fa][1] == 'Y') {
				if (na == NULL)
					usage("Parameter expected after -Y");
			
				if (na[0] == 'A') {
					nadaptive = 1;
				} else if (na[0] == 'a') {
					averagemode = 1;
				} else if (na[0] == 'r') {
					refrmode = 1;
				} else if (na[0] == 'n') {
					refrmode = 0;
				} else if (na[0] == 'R') {
					if (na[1] != ':')
						usage("-Y R:rate syntax incorrect");
					refrate = atof(na+2);
					if (refrate < 5.0 || refrate > 150.0)
						usage("-Y R:rate %f Hz not in valid range",refrate);

#ifndef SALONEINSTLIB
				/* Save white tile reference spectrum to a file */
				} else if (na[0] == 'W') {
					if (na[1] != ':')
						usage("-Y W:fname.sp syntax incorrect");
						strncpy(wtilename,&na[2],MAXNAMEL-1); wtilename[MAXNAMEL-1] = '\000';
#endif	/* !SALONEINSTLIB */

				/* i1Pro lamp drift test & fix */
				} else if (na[0] == 'l') {
					lampdrift = 1;
				} else if (na[0] == 'L') {
					lampdrift = 2;

				/* ~~~ i1pro2 test code ~~~ */
				} else if (na[0] == 'U') {
					uvmode = 1;

				} else {
					usage("-Y parameter '%c' not recognised",na[0]);
				}
				fa = nfa;

			/* Serial port flow control */
			} else if (argv[fa][1] == 'W') {
				fa = nfa;
				if (na == NULL) usage("Parameter expected after -W");
				if (na[0] == 'n' || na[0] == 'N')
					fc = fc_None;
				else if (na[0] == 'h' || na[0] == 'H')
					fc = fc_Hardware;
				else if (na[0] == 'x' || na[0] == 'X')
					fc = fc_XonXOff;
				else
					usage("-W parameter '%c' not recognised",na[0]);

			} else 
				usage("Flag -%c not recognised",argv[fa][1]);
		}
		else
			break;
	}

	/* Get the optional file name argument */
	if (fa < argc) {
		strncpy(outname,argv[fa++],MAXNAMEL-1); outname[MAXNAMEL-1] = '\000';
		if ((fp = fopen(outname, "w")) == NULL)
			error("Unable to open logfile '%s' for writing\n",outname);
	}


	/* See if there is an environment variable ccxx */
	if (ccxxname[0] == '\000') {
		char *na;
		if ((na = getenv("ARGYLL_COLMTER_CAL_SPEC_SET")) != NULL) {
			strncpy(ccxxname,na,MAXNAMEL-1); ccxxname[MAXNAMEL-1] = '\000';

		} else if ((na = getenv("ARGYLL_COLMTER_COR_MATRIX")) != NULL) {
			strncpy(ccxxname,na,MAXNAMEL-1); ccxxname[MAXNAMEL-1] = '\000';
		}
	}

	/* Check for some user mistakes */
	if ((tillum_set || illum_set) && emiss )
		warning("-I or -i parameter makes no sense with emissive or ambient measurement!");

	if (illum_set && labwpillum && emiss) {
		warning("-w for emissive is ignored!");
		labwpillum = 0;
	}

	/* - - - - - - - - - - - - - - - - - - -  */
	/* Setup Lab conversion wp if not D50 */
	if (illum_set && labwpillum && !emiss) {
		double xyz[3];

		strcpy(labwpname, standardIlluminant_name(illum, 0.0));

		if (icx_ill_sp2XYZ(xyz, obType, custObserver, illum, 0.0, &cust_illum, 0))     
			error("Looking up W.P. of illuminant failed");

		icmScale3(xyz, xyz, 100.0);
		icmAry2XYZ(labwp, xyz);

	}

	if ((icmps = new_icompaths(g_log)) == NULL)
		error("Finding instrument paths failed");
	if ((ipath = icmps->get_path(icmps, comport)) == NULL)
		error("No instrument at port %d",comport);

	/* Setup the instrument ready to do reads */
	if ((it = new_inst(ipath, 0, g_log, DUIH_FUNC_AND_CONTEXT)) == NULL) {
		usage("Unknown, inappropriate or no instrument detected");
	}

#ifdef TEST_EVENT_CALLBACK
	it->set_event_callback(it, test_event_callback, (void *)it);
#endif

	if (verb)
		printf("Connecting to the instrument ..\n");

#ifdef DEBUG
	printf("About to init the comms\n");
#endif

	/* Establish communications */
	if ((rv = it->init_coms(it, br, fc, 15.0)) != inst_ok) {
		printf("Failed to initialise communications with instrument\n"
		       "or wrong instrument or bad configuration!\n"
		       "('%s' + '%s')\n", it->inst_interp_error(it, rv), it->interp_error(it, rv));
		it->del(it);
		return -1;
	}

#ifdef DEBUG
	printf("Established comms\n");
#endif

#ifdef DEBUG
	printf("About to init the instrument\n");
#endif

	/* set filter configuration before initialising/calibrating */
	if (fe != inst_opt_filter_unknown) {
		if ((rv = it->get_set_opt(it, inst_opt_set_filter, fe)) != inst_ok) {
			printf("Setting filter configuration not supported by instrument\n");
			it->del(it);
			return -1;
		}
	}

	/* Initialise the instrument */
	if ((rv = it->init_inst(it)) != inst_ok) {
		printf("Instrument initialisation failed with '%s' (%s)!\n",
		      it->inst_interp_error(it, rv), it->interp_error(it, rv));
		it->del(it);
		return -1;
	}
	
	/* Check i1Pro lamp drift, and remediate if it is too large */
	/* (Hmm. If cooltime is too long, drift appears to be worse
	    after remediation. It's not clear why, but it returns
	    to expected values once instrument has truly cooled down (i.e. 2+min ?)
	 */
	if (lampdrift) {
		int pass = 0;
		double remtime = 0.0;
		int cooltime = 30;

		if (it->dtype != instI1Pro
		 && it->dtype != instI1Pro2) {
			printf("LampDrift is only applicable to i1Pro instrument");
		}

		/* Disable initial calibration of machine if selected */
		if (nocal != 0) {
			if ((rv = it->get_set_opt(it,inst_opt_noinitcalib, 0)) != inst_ok) {
				printf("Setting no-initial calibrate failed with '%s' (%s)\n",
			       it->inst_interp_error(it, rv), it->interp_error(it, rv));
				printf("Disable initial-calibrate not supported\n");
			}
		}

		if ((rv = it->get_set_opt(it, inst_opt_trig_prog)) != inst_ok)
			error("Setting trigger mode failed with error :'%s' (%s)",
	       	       it->inst_interp_error(it, rv), it->interp_error(it, rv));

		for (pass = 0; pass < 2; pass++) {
			ipatch val;
			double dl, maxdl = -100.0, de, maxde = -100.0;
			int ii;

			printf("\nDoing Lamp Drift check - place instrument on calibration tile\n");

			/* Do any needed calibration before the user places the instrument on a desired spot */
			if (it->needs_calibration(it) & inst_calt_n_dfrble_mask) {
				inst_code ev;

				printf("\nNeed a calibration before continuing\n");

				ev = inst_handle_calibrate(it, inst_calt_needed, inst_calc_none, NULL, NULL, doone);
				if (ev != inst_ok) {	/* Abort or fatal error */
					error("Got abort or error from calibration");
				}
			}

#ifndef NEVER
			/* Ensure lamp has cooled down */
			printf("\nAllowing lamp to cool\n");
			for (i = cooltime; i > 0; i--) {
				msec_sleep(1000);
				printf("\r%d ",i); fflush(stdout);
			}
			printf("\r0 \n");
#endif
	
			printf("\nChecking Lamp Drift\n");
			/* Measure 1 spot and save as ref */
			if ((rv = it->read_sample(it, "SPOT", &val, instNoClamp)) != inst_ok) {
				error("Read sample failed with '%s' (%s)",
				       it->inst_interp_error(it, rv), it->interp_error(it, rv));
			}
			if (val.XYZ_v == 0) error("Instrument didn't return XYZ value");
			icmXYZ2Lab(&icmD50_100, rLab, val.XYZ);
	
			// Until measurement is stable, or 30 measurements
			// Read and save max DE

			/* 30 trials, or no new biggest in 4 measurements */
			for (ii = 100, i = 0; i < 40 && (i < 10 || (i - ii) < 6) ; i++) {
				if ((rv = it->read_sample(it, "SPOT", &val, instNoClamp)) != inst_ok) {
					error("Read sample failed with '%s' (%s)",
					       it->inst_interp_error(it, rv), it->interp_error(it, rv));
				}
				if (val.XYZ_v == 0) error("Instrument didn't return XYZ value");
				icmXYZ2Lab(&icmD50_100, Lab, val.XYZ);

				dl = Lab[0] - rLab[0]; 
				de = icmLabDE(Lab, rLab);

				/* Keep going while dl is rising */
				if (dl > maxdl) {
					maxdl = dl;
					ii = i;
				}
				if (de > maxde) {
					maxde = de;
					printf("\r%1.3f DE",de); fflush(stdout);
				}
			}
	
			// Print DE
//			printf("\nLamp Drift Delta E = %f\n",maxde);

			if (lampdrift == 1) {
				printf("\nDrift test complete - %s\n", maxde >= 0.09 ? "Needs Fixing!" : "OK");
				break;
			}

			if (pass > 0) {
				printf("\nDrift test & fix complete\n");
				break;
			}
	
			if (maxde >= 0.09) 
				remtime = 60.0;
			else if (maxde >= 0.12)
				remtime = 90.0;
			else if (maxde > 0.20)
				remtime = 120.0;
	
			if (remtime > 0.0) {
				printf("\nDoing %.0f seconds of remediation\n",remtime);
				// Do remediation */
				if ((rv = it->get_set_opt(it, inst_opt_lamp_remediate, remtime)) != inst_ok) {
					error("Remediating Lamp Drift failed with error :'%s' (%s)\n",
			       	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
				}
				cooltime = 45;
			} else {
				printf("\nDrift is OK\n");
				break;
			}
		}
		
		goto done;
	}

	/* Configure the instrument mode */
	{
		int ccssset = 0;
		it->capabilities(it, &cap, &cap2, &cap3);

		/* Don't fail if the instrument has only one mode, and */
		/* it's not been selected */

		if (trans == 0 && emiss == 0 && !IMODETST(cap, inst_mode_reflection)) {
			/* This will fail. Switch to a mode the instrument has */
			if (IMODETST(cap, inst_mode_emission)) {
				if (verb)
					printf("Defaulting to emission measurement\n");
				emiss = 1;

			} else if (IMODETST(cap, inst_mode_transmission)) {
				if (verb)
					printf("Defaulting to transmission measurement\n");
				trans = 1;
			}
		}

		if (trans) {
			/* Alternate geometry - 90/diffuse */
			if (ambient) {
				if (it->check_mode(it, inst_mode_trans_spot_a) != inst_ok) {
					printf("Need transmission spot (alt) capability,\n");
					printf("and instrument doesn't support it\n");
					it->del(it);
					return -1;
				}
			/* Normal geometry - diffuce/90 */
			} else {
				if (it->check_mode(it, inst_mode_trans_spot) != inst_ok) {
					printf("Need transmission spot capability,\n");
					printf("and instrument doesn't support it\n");
					it->del(it);
					return -1;
				}
			}

		} else if (ambient == 1) {
			if (it->check_mode(it, inst_mode_emis_ambient) != inst_ok) {
				printf("Requested ambient light capability,\n");
				printf("and instrument doesn't support it.\n");
				return -1;
			} else {
				if (verb) {
					printf("Please make sure the instrument is fitted with\n");
					printf("the appropriate ambient light measuring head\n");
				}
			}

		} else if (ambient == 2) {
			if (it->check_mode(it, inst_mode_emis_ambient_flash) != inst_ok) {
				printf("Requested ambient flash capability,\n");
				printf("and instrument doesn't support it.\n");
				it->del(it);
				return -1;
			} else {
				if (verb) {
					printf("Please make sure the instrument is fitted with\n");
					printf("the appropriate ambient light measuring head, and that\n");
					printf("you are ready to trigger the flash.\n");
				}
			}

		} else if (emiss || tele) {

			/* If there is a tele mode but no emission, use tele */
			if (it->check_mode(it, inst_mode_emis_spot) != inst_ok
			 && it->check_mode(it, inst_mode_emis_tele) == inst_ok) {
				tele = 1;
			}

			if (tele) {
				if (it->check_mode(it, inst_mode_emis_tele) != inst_ok) {
					printf("Need telephoto spot capability\n");
					printf("and instrument doesn't support it\n");
					it->del(it);
					return -1;
				}
			} else {
				if (it->check_mode(it, inst_mode_emis_spot) != inst_ok) {
					printf("Need emissive spot capability\n");
					printf("and instrument doesn't support it\n");
					it->del(it);
					return -1;
				}
			}

			if (nadaptive && !IMODETST(cap, inst_mode_emis_nonadaptive)) {
				if (verb) {
					printf("Requested non-adaptive mode and instrument doesn't support it (ignored)\n");
					nadaptive = 0;
				}
			}
			if (refrmode >= 0 && it->check_mode(it, inst_mode_emis_refresh_ovd) != inst_ok
			                  && it->check_mode(it, inst_mode_emis_norefresh_ovd) != inst_ok) {
				if (verb) {
					printf("Requested refresh mode override and instrument doesn't support it (ignored)\n");
					refrmode = -1;
				}
			}
		} else {
			if (it->check_mode(it, inst_mode_ref_spot) != inst_ok) {
				printf("Need reflection spot reading capability,\n");
				printf("and instrument doesn't support it\n");
				it->del(it);
				return -1;
			}
		}

		/* Set displaytype or calibration mode */
		if (ditype != 0) {
			if (cap2 & inst2_disptype) {
				int ix;
				if ((ix = inst_get_disptype_index(it, ditype, 0)) < 0) {
					it->del(it);
					usage("Failed to locate display type matching '%s'",inst_distr(ditype));
				}
	
				if ((rv = it->set_disptype(it, ix)) != inst_ok) {
					printf("Setting display type ix %d not supported by instrument\n",ix);
					it->del(it);
					return -1;
				}
			} else
				printf("Display/calibration type ignored - instrument doesn't support it\n");
		}

		/* If we have non-standard observer we need spectral or CCSS */
		if (obType != icxOT_default && !IMODETST(cap, inst_mode_spectral) && !(cap2 & inst2_ccss)) {
			printf("Non standard observer needs spectral information or CCSS capability\n");
			printf("and instrument doesn't support either.\n");
			it->del(it);
			return -1;
		}

		/* If we don't have CCSS then we need spectral for non-standard observer */
		if (obType != icxOT_default && (cap2 & inst2_ccss) == 0) {
			spec = 1;
		}

		if ((spec || pspec) && !IMODETST(cap, inst_mode_spectral)) {
			printf("Need spectral information for custom illuminant or observer\n");
			printf("and instrument doesn't support it\n");
			it->del(it);
			return -1;
		}

		/* Disable initial calibration of machine if selected */
		if (nocal != 0) {
			if ((rv = it->get_set_opt(it,inst_opt_noinitcalib, 0)) != inst_ok) {
				printf("Setting no-initial calibrate failed with '%s' (%s)\n",
			       it->inst_interp_error(it, rv), it->interp_error(it, rv));
				printf("Disable initial-calibrate not supported\n");
			}
		}
		/* Set it to the appropriate mode */

		/* Should look at instrument type & user spec ??? */
		if (trans) {
			if (ambient)
				smode = mode = inst_mode_trans_spot_a;
			else
				smode = mode = inst_mode_trans_spot;
		} else if (ambient == 1
			 && it->check_mode(it, inst_mode_emis_ambient) == inst_ok)
			smode = mode = inst_mode_emis_ambient;
		else if (ambient == 2
			 && it->check_mode(it, inst_mode_emis_ambient_flash) == inst_ok)
			smode = mode = inst_mode_emis_ambient_flash;
		else if (tele)							// Hmm. What about tele flash ?
			smode = mode = inst_mode_emis_tele;
		else if (emiss || ambient)
			smode = mode = inst_mode_emis_spot;
		else {
			smode = mode = inst_mode_ref_spot;
			if (it->check_mode(it, inst_mode_s_ref_spot) == inst_ok)
				smode = inst_mode_s_ref_spot;
		}

		/* Mode dependent extra modes */
		if (emiss || tele) {
			if (nadaptive) {
				mode  |= inst_mode_emis_nonadaptive;
				smode |= inst_mode_emis_nonadaptive;
			}
			if (refrmode == 0) {
				mode  |= inst_mode_emis_norefresh_ovd;
				smode |= inst_mode_emis_norefresh_ovd;
			}
			else if (refrmode == 1) {
				mode  |= inst_mode_emis_refresh_ovd;
				smode |= inst_mode_emis_refresh_ovd;
			}
		}

		/* Use spec if requested or if available in case of CRI or FWA */
		if (spec || pspec || IMODETST(cap, inst_mode_spectral)) {
			mode  |= inst_mode_spectral;
			smode |= inst_mode_spectral;
		}

		if (highres) {
			if (IMODETST(cap, inst_mode_highres)) {
				mode  |= inst_mode_highres;
				smode |= inst_mode_highres;
			} else if (verb) {
				printf("high resolution ignored - instrument doesn't support high res. mode\n");
				highres = 0;
			}
		}

		// ~~~ i1pro2 test code ~~~ */
		if (uvmode) {
			if (!IMODETST(cap, inst_mode_ref_uv)) {
				printf("UV measurement mode requested, but instrument doesn't support this mode\n");
				it->del(it);
				return -1;
			}
			mode  |= inst_mode_ref_uv;
			smode |= inst_mode_ref_uv;
		}

		if ((rv = it->set_mode(it, mode)) != inst_ok) {
			printf("\nSetting instrument mode failed with error :'%s' (%s)\n",
		     	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
			it->del(it);
			return -1;
		}
		it->capabilities(it, &cap, &cap2, &cap3);

		/* If requested, average several readings (i.e. JETI 1211) */
		if (averagemode) {
			if (!IMODETST(cap3, inst3_average)) {
				if (verb)
					printf("Requested averaging mode and instrument doesn't support it (ignored)\n");
				averagemode = 0;
			} else if ((rv = it->get_set_opt(it, inst_opt_set_averages, 10)) != inst_ok) {
				printf("Setting no of averages to 10 failed with '%s' (%s) !!!\n",
			       it->inst_interp_error(it, rv), it->interp_error(it, rv));
			}
		}

		/* Apply emission filter compensation */
		if (filtername[0] != '\000') {
			xspect sp;
			if (read_xspect(&sp, NULL, filtername) != 0)
				error("Failed to emission filter compensation file '%s'",filtername);

			if ((rv = it->get_set_opt(it, inst_opt_set_custom_filter, &sp)) != inst_ok) {
				printf("Setting emission filter compensation failed with '%s' (%s) !!!\n",
			       it->inst_interp_error(it, rv), it->interp_error(it, rv));
			}
		}

		/* set XRGA conversion */
		if (calstd != xcalstd_none) {
			if ((rv = it->get_set_opt(it, inst_opt_set_xcalstd, calstd)) != inst_ok) {
				printf("Setting calibration standard not supported by instrument\n");
				it->del(it);
				return -1;
			}
		}

		/* Colorimeter Correction Matrix */
		if (ccxxname[0] != '\000') {
			ccss *cs = NULL;
			ccmx *cx = NULL;

			if ((cx = new_ccmx()) == NULL) {
				printf("\nnew_ccmx failed\n");
				it->del(it);
				return -1;
			}
			if (cx->read_ccmx(cx,ccxxname) == 0) {
				if ((cap2 & inst2_ccmx) == 0) {
					printf("\nInstrument doesn't have Colorimeter Correction Matrix capability\n");
					it->del(it);
					return -1;
				}
				if ((rv = it->col_cor_mat(it, cx->dtech, cx->cc_cbid, cx->matrix)) != inst_ok) {
					printf("\nSetting Colorimeter Correction Matrix failed with error :'%s' (%s)\n",
				     	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
					cx->del(cx);
					it->del(it);
					return -1;
				}
				cx->del(cx);

			} else {
				cx->del(cx);
				cx = NULL;
				
				/* CCMX failed, try CCSS */
				if ((cs = new_ccss()) == NULL) {
					printf("\nnew_ccss failed\n");
					it->del(it);
					return -1;
				}
				if (cs->read_ccss(cs,ccxxname)) {
					printf("\nReading CCMX/CCSS File '%s' failed with error %d:'%s'\n",
				     	       ccxxname, cs->errc, cs->err);
					cs->del(cs);
					it->del(it);
					return -1;
				}
				if ((cap2 & inst2_ccss) == 0) {
					printf("\nInstrument doesn't have Colorimeter Calibration Spectral Sample capability\n");
					cs->del(cs);
					it->del(it);
					return -1;
				}
				if ((rv = it->get_set_opt(it, inst_opt_set_ccss_obs, obType, custObserver)) != inst_ok) {
					printf("\nSetting CCS Observer failed with error :'%s' (%s)\n",
				     	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
					cs->del(cs);
					it->del(it);
					return -1;
				}
				if ((rv = it->col_cal_spec_set(it, cs->dtech, cs->samples, cs->no_samp)) != inst_ok) {
					printf("\nSetting Colorimeter Calibration Spectral Samples failed with error :'%s' (%s)\n",
				     	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
					cs->del(cs);
					it->del(it);
					return -1;
				}
				ccssset = 1;
				cs->del(cs);
			}
		}

		if (refrate > 0.0) {
			if (!(cap2 & inst2_set_refresh_rate)) {
				if (verb)
					printf("Attempted to set refresh rate and instrument doesn't support setting it (ignored)\n");
				refrate = 0.0;
			} else {
				if ((rv = it->set_refr_rate(it, refrate)) != inst_ok) {
					printf("\nSetting instrument refresh rate to %f Hz failed with error :'%s' (%s)\n",
				     	       refrate, it->inst_interp_error(it, rv), it->interp_error(it, rv));
					it->del(it);
					return -1;
				}
			}
		}

		/* If non-standard observer wasn't set by a CCSS file above */
		if (obType != icxOT_default && (cap2 & inst2_ccss) && ccssset == 0) {
			if ((rv = it->get_set_opt(it, inst_opt_set_ccss_obs, obType, custObserver)) != inst_ok) {
				printf("\nSetting CCSS Observer failed with error :'%s' (%s)\n",
			     	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
				it->del(it);
				return -1;
			}
		}

		/* Warm the user that they should do a frequency calibration */
		if (it->needs_calibration(it) & inst_calt_ref_freq) {
			printf("Please read an 80%% white patch first to calibrate refresh frequency\n");
		}

		/* If it battery powered, show the status of the battery */
		if ((cap2 & inst2_has_battery)) {
			double batstat = 0.0;
			if ((rv = it->get_set_opt(it, inst_stat_battery, &batstat)) != inst_ok) {
				printf("\nGetting instrument battery status failed with error :'%s' (%s)\n",
		       	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
				it->del(it);
				return -1;
			}
			printf("The battery charged level is %.0f%%\n",batstat * 100.0);
		}

		/* If it's an instrument that need positioning let user trigger via uicallback */
		/* in spotread, else enable switch or user via uicallback trigger if possible. */
		if ((cap2 & inst2_xy_locate) && (cap2 & inst2_xy_position)) {
			trigmode = inst_opt_trig_prog;

		} else if (cap2 & inst2_user_switch_trig) {
			trigmode = inst_opt_trig_user_switch;
			uswitch = 1;

		/* Or go for keyboard trigger */
		} else if (cap2 & inst2_user_trig) {
			trigmode = inst_opt_trig_user;

		/* Or something is wrong with instrument capabilities */
		} else {
			printf("\nNo reasonable trigger mode avilable for this instrument\n");
			it->del(it);
			return -1;
		}
		if ((rv = it->get_set_opt(it, trigmode)) != inst_ok) {
			printf("\nSetting trigger mode failed with error :'%s' (%s)\n",
	       	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
			it->del(it);
			return -1;
		}

		/* Setup the keyboard trigger to return our commands */
		inst_set_uih(0x0, 0xff,  DUIH_TRIG);
		inst_set_uih('r', 'r',   DUIH_CMND);
		inst_set_uih('R', 'R',   DUIH_CMND);
		inst_set_uih('h', 'h',   DUIH_CMND);
		inst_set_uih('H', 'H',   DUIH_CMND);
		inst_set_uih('k', 'k',   DUIH_CMND);
		inst_set_uih('K', 'K',   DUIH_CMND);
		inst_set_uih('s', 's',   DUIH_CMND);
		inst_set_uih('S', 'S',   DUIH_CMND);
		if (cap2 & inst2_has_target)
			inst_set_uih('t', 't',   DUIH_CMND);
		inst_set_uih('f', 'f',   DUIH_CMND);
		inst_set_uih('F', 'F',   DUIH_CMND);
		inst_set_uih('q', 'q',   DUIH_ABORT);
		inst_set_uih('Q', 'Q',   DUIH_ABORT);
		inst_set_uih(0x03, 0x03, DUIH_ABORT);		/* ^c */
		inst_set_uih(0x1b, 0x1b, DUIH_ABORT);		/* Esc */
	}

#ifndef SALONEINSTLIB
	/* Save reference white tile reflectance spectrum */
	if (wtilename[0] != '\000') {
		xspect sp;

		if ((rv = it->get_set_opt(it, inst_opt_get_cal_tile_sp, &sp)) != inst_ok) {
			printf("\nGetting reference white tile spectrum failed with error :'%s' (%s)\n",
	       	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
			it->del(it);
			return -1;
		}

		if (write_xspect(wtilename, inst_mrt_reflective, &sp) != 0)
			error("Failed to save spectrum to file '%s'",wtilename);

		if (verb)
			printf("Saved reference white tile spectrum to '%s'\n",wtilename);
	}
#endif	/* !SALONEINSTLIB */

#ifdef DEBUG
	printf("About to enter read loop\n");
#endif

	if (verb)
		printf("Init instrument success !\n");

	if (doone) {	/* Set to trigger immediately */
		it->set_uicallback(it, uicallback, NULL);
	}

	if (spec || psetrefname[0] != '\000') {
		/* Any non-illuminated mode has no illuminant */
		if (emiss || ambient)
			illum = icxIT_none;

		/* Create a spectral conversion object */
		if ((sp2cie = new_xsp2cie(illum, 0.0, &cust_illum, obType, custObserver, icSigXYZData,
			                           refstats ? icxNoClamp : icxClamp)) == NULL)
			error("Creation of spectral conversion object failed");

		/* If we are setting a specific simulated instrument illuminant */
		if (tillum != icxIT_none) {
			tillump = &cust_tillum;
			if (tillum != icxIT_custom) {
				if (standardIlluminant(tillump, tillum, 0.0)) {
					error("simulated inst. illum. not recognised");
				}
			}
		}
	}

#ifndef SALONEINSTLIB
	/* Load preset reference spectrum */
	if (psetrefname[0] != '\000') {
		inst_meas_type mt;

		if (read_xspect(&rsp, &mt, psetrefname) != 0)
			error("Failed to read spectrum from file '%s'",psetrefname);

		if (!emiss && !ambient) {
			if (mt != inst_mrt_none
			 && mt != inst_mrt_transmissive
			 && mt != inst_mrt_reflective)
				error("Reference reflectance spectrum '%s' is wrong measurement type",psetrefname);
		} else {
			if (mt != inst_mrt_none
			 && mt != inst_mrt_emission
			 && mt != inst_mrt_ambient
			 && mt != inst_mrt_emission_flash
			 && mt != inst_mrt_ambient_flash)
				error("Reference reflectance spectrum '%s' is wrong measurement type",psetrefname);
		}

		if (verb)
			printf("Loaded reference spectrum from '%s'\n",psetrefname);

		sp2cie->convert(sp2cie, rXYZ, &rsp);

		if (!(emiss || tele || ambient)) {
			for (j = 0; j < 3; j++)
				rXYZ[j] *= 100.0;		/* 0..100 scale */
		}
		icmXYZ2Lab(&labwp, rLab, rXYZ);
		if (verb)
			printf("Preset ref. XYZ %f %f %f, %s Lab %f %f %f\n", rXYZ[0], rXYZ[1], rXYZ[2], labwpname, rLab[0], rLab[1], rLab[2]);
	}
#endif

	/* Hold table */
	if (cap2 & inst2_xy_holdrel) {
		for (;;) {		/* retry loop */
			if ((rv = it->xy_sheet_hold(it)) == inst_ok)
				break;

			if (ierror(it, rv)) {
				it->xy_clear(it);
				it->del(it);
				return -1;
			}
		}
	}

	/* Read spots until the user quits */
	for (ix = 1;; ix++) {
		ipatch val;
//		double tXYZ[3];
#ifndef SALONEINSTLIB
		double cct, vct, vdt;
		double cct_de, vct_de, vdt_de;
		double cct_sn, vct_sn, vdt_sn;			/* Sign: 1 + above, -1 = below */
#endif /* !SALONEINSTLIB */
		int ch = '0';		/* Character */
		int sufwa = 0;		/* Setup for FWA compensation */
		int dofwa = 0;		/* Do FWA compensation */
		int fidx = -1;		/* FWA compensated index, default = none */
	
#ifdef NEVER 	// test i1d3 min_int_time code
		{
			double cval;
			char *cp;

			if ((rv = it->get_set_opt(it, inst_opt_get_min_int_time, &cval)) != inst_ok) {
				printf("\nGetting min_int)time failed with error :'%s' (%s)\n",
		       	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
				it->del(it);
				return -1;
			}
			printf("Current min int time = %f\n",cval);
			
			if ((cp = getenv("I1D3_MIN_INT_TIME")) != NULL) {
				cval = atof(cp);

				printf("Setting int time %f\n",cval);
				if ((rv = it->get_set_opt(it, inst_opt_set_min_int_time, cval)) != inst_ok) {
					printf("\nSetting min_int_time failed with error :'%s' (%s)\n",
			       	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
					it->del(it);
					return -1;
				}
			
				if ((rv = it->get_set_opt(it, inst_opt_get_min_int_time, &cval)) != inst_ok) {
					printf("\nGetting min_int)time failed with error :'%s' (%s)\n",
			       	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
					it->del(it);
					return -1;
				}
			
				printf("Check current min int time = %f\n",cval);
			}
		}

#endif // NEVER

		if (savdrd != -1 && it->check_mode(it, inst_mode_s_ref_spot) == inst_ok) {
			inst_stat_savdrd sv;

			savdrd = 0;
			if ((rv = it->get_set_opt(it, inst_stat_saved_readings, &sv)) != inst_ok) {
				printf("\nGetting saved reading status failed with error :'%s' (%s)\n",
		       	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
				it->del(it);
				return -1;
			}
			if (sv & inst_stat_savdrd_spot)
				savdrd = 1;
		}

		/* We now wait for a user to trigger a measurement with a key, */
		/* or issue a command using a key. */

		/* Read a stored value from the instrument */
		if (savdrd == 1) {
			inst_code ev;

			/* Set to saved spot mode */
			if ((ev = it->set_mode(it, smode)) != inst_ok) {
				printf("\nSetting instrument mode failed with error :'%s' (%s)\n",
			     	       it->inst_interp_error(it, ev), it->interp_error(it, ev));
				it->del(it);
				return -1;
			}
			it->capabilities(it, &cap, &cap2, &cap3);

			/* Set N and n to be a command */
			inst_set_uih('N', 'N', DUIH_CMND);
			inst_set_uih('n', 'n', DUIH_CMND);

			/* Set to keyboard only trigger */
			if ((ev = it->get_set_opt(it, inst_opt_trig_user)) != inst_ok) {
				printf("\nSetting trigger mode failed with error :'%s' (%s)\n",
		       	       it->inst_interp_error(it, ev), it->interp_error(it, ev));
				it->del(it);
				return -1;
			}

			printf("\nThere are saved spot readings in the instrument.\n");
#ifndef SALONEINSTLIB
			printf("Hit [A-Z] to use reading for white and setup FWA compensation (keyed to letter)\n");
			printf("[a-z] to use reading for FWA compensated value from keyed reference\n");
			printf("'r' to set reference, 's' to save spectrum,\n");
#else /* SALONEINSTLIB */
			printf("Hit 'r' to set reference\n");
#endif /* SALONEINSTLIB */
			printf("Hit ESC or Q to exit, N to not read saved reading,\n");
			printf("any other key to use reading: ");
			fflush(stdout);
			
			/* Read the sample or get a command */
			rv = it->read_sample(it, "SPOT", &val, refstats ? instNoClamp : instClamp);
			
			ch = inst_get_uih_char();

			/* Restore the trigger mode */
			if ((ev = it->get_set_opt(it, trigmode)) != inst_ok) {
				printf("\nSetting trigger mode failed with error :'%s' (%s)\n",
		       	       it->inst_interp_error(it, ev), it->interp_error(it, ev));
				it->del(it);
				return -1;
			}

			/* Set N and n to be a trigger */
			inst_set_uih('N', 'N', DUIH_TRIG);
			inst_set_uih('n', 'n', DUIH_TRIG);

			/* Set back to read spot mode */
			if ((ev = it->set_mode(it, mode)) != inst_ok) {
				printf("\nSetting instrument mode failed with error :'%s' (%s)\n",
			     	       it->inst_interp_error(it, ev), it->interp_error(it, ev));
				it->del(it);
				return -1;
			}
			it->capabilities(it, &cap, &cap2, &cap3);

			/* If user said no to reading stored values */
			if ((rv & inst_mask) == inst_user_abort
			 && (ch & DUIH_CMND) && ((ch & 0xff) == 'N' || (ch & 0xff) == 'n')) {
				printf("\n");
				savdrd = -1;
				continue;
			}

		/* Do a normal read */
		} else {

			/* Do any needed calibration before the user places the instrument on a desired spot */
			if (it->needs_calibration(it) & inst_calt_n_dfrble_mask) {
				inst_code ev;

				printf("\nSpot read needs a calibration before continuing\n");

				/* save current location */
				if ((cap2 & inst2_xy_locate) && (cap2 & inst2_xy_position)) {
					for (;;) {		/* retry loop */
						if ((ev = it->xy_get_location(it, &lx, &ly)) == inst_ok)
							break;
						if (ierror(it, ev) == 0)	/* Ignore */
							continue;
						break;						/* Abort */
					}
					if (ev != inst_ok) {
						printf("\nSpot read got abort or error from xy_get_location\n");
						break;			/* Abort */
					}
				}

				ev = inst_handle_calibrate(it, inst_calt_needed, inst_calc_none, NULL, NULL, doone);
				if (ev != inst_ok) {	/* Abort or fatal error */
					printf("\nSpot read got abort or error from calibration\n");
					break;
				}

				/* restore location */
				if ((cap2 & inst2_xy_locate) && (cap2 & inst2_xy_position)) {
					for (;;) {		/* retry loop */
						if ((ev = it->xy_position(it, 0, lx, ly)) == inst_ok)
							break;
						if (ierror(it, ev) == 0)	/* Ignore */
							continue;
						break;						/* Abort */
					}
					if (ev != inst_ok) {
						printf("\nSpot read got abort or error from xy_get_location\n");
						break;			/* Abort */
					}
				}

				if (doone)
					break;
			}

			if (ambient == 2) {	/* Flash ambient */
				printf("\nConfigure for ambient, press and hold button, trigger flash then release button,\n");
#ifndef SALONEINSTLIB
				printf("or hit 'r' to set reference, 's' to save spectrum,\n");
#else /* SALONEINSTLIB */
				printf("or hit 'r' to set reference\n");
#endif /* SALONEINSTLIB */
				printf("'h' to toggle high res., 'k' to do a calibration\n");

			} else {
				/* If this is an xy instrument: */
				if ((cap2 & inst2_xy_locate) && (cap2 & inst2_xy_position)) {

					/* Allow the user to position the instrument */
					for (;;) {		/* retry loop */
						if ((rv = it->xy_locate_start(it)) == inst_ok)
							break;
						if (ierror(it, rv) == 0)	/* Ignore */
							continue;
						break;						/* Abort */
					}
					if (rv != inst_ok) {
						printf("\nSpot read got abort or error from xy_locate_start\n");
						break;			/* Abort */
					}

					printf("\nUsing the XY table controls, locate the point to measure with the sight,\n");

				/* Purely manual instrument */
				} else {

					/* If this is display white brightness relative, read the white */
					if ((emiss > 1  || tele > 1) && wXYZ[0] < 0.0)
						printf("\nPlace instrument on white reference spot,\n");
					else {
						printf("\nPlace instrument on spot to be measured,\n");
					}
				}
#ifndef SALONEINSTLIB
				printf("and hit [A-Z] to read white and setup FWA compensation (keyed to letter)\n");
				printf("[a-z] to read and make FWA compensated reading from keyed reference\n");
				printf("'r' to set reference, 's' to save spectrum,\n");
				printf("'f' to report cal. refresh rate, 'F' to measure refresh rate\n");
#else /* SALONEINSTLIB */
				printf("Hit 'r' to set reference\n");
#endif /* SALONEINSTLIB */
				printf("'h' to toggle high res., 'k' to do a calibration\n");
				if (cap2 & inst2_has_target)
					printf("'t' to toggle laser target\n");
			}
			if (uswitch)
				printf("Hit ESC or Q to exit, instrument switch or any other key to take a reading: ");
			else
				printf("Hit ESC or Q to exit, any other key to take a reading: ");
			fflush(stdout);

			if ((cap2 & inst2_xy_locate) && (cap2 & inst2_xy_position)) {
				/* Wait for the user to hit a key */
				for (;;) {
					if ((rv = inst_get_uicallback()(inst_get_uicontext(), inst_armed)) != inst_ok)
						break;
				}

				if (rv == inst_user_abort) {
					break;				/* Abort */

				} else if (rv == inst_user_trig) {
					inst_code ev;

					/* Take the location set on the sight, and move the instrument */
					/* to take the measurement there. */ 
					if ((cap2 & inst2_xy_locate) && (cap2 & inst2_xy_position)) {
						for (;;) {		/* retry loop */
							if ((rv = it->xy_get_location(it, &lx, &ly)) == inst_ok)
								break;
							if (ierror(it, rv) == 0)	/* Ignore */
								continue;
							break;						/* Abort */
						}
						if (rv != inst_ok) {
							printf("\nSpot read got abort or error from xy_get_location\n");
							break;			/* Abort */
						}
			
						for (;;) {		/* retry loop */
							if ((rv = it->xy_locate_end(it)) == inst_ok)
								break;
							if (ierror(it, rv) == 0)	/* Ignore */
								continue;
							break;						/* Abort */
						}
						if (rv != inst_ok) {
							printf("\nSpot read got abort or error from xy_locate_end\n");
							break;			/* Abort */
						}
			
						for (;;) {		/* retry loop */
							if ((rv = it->xy_position(it, 1, lx, ly)) == inst_ok)
								break;
							if (ierror(it, rv) == 0)	/* Ignore */
								continue;
							break;						/* Abort */
						}
						if (rv != inst_ok) {
							printf("\nSpot read got abort or error from xy_position\n");
							break;			/* Abort */
						}
					}
					rv = it->read_sample(it, "SPOT", &val, refstats ? instNoClamp : instClamp);

					/* Restore the location the instrument to have the location */
					/* sight over the selected patch. */
					for (;;) {		/* retry loop */
						if ((ev = it->xy_position(it, 0, lx, ly)) == inst_ok)
							break;
						if (ierror(it, ev) == 0)	/* Ignore */
							continue;
						break;						/* Abort */
					}
					if (ev != inst_ok) {
						printf("\nSpot read got abort or error from xy_position\n");
						break;			/* Abort */
					}
				}
				/* else what ? */
			} else {
				rv = it->read_sample(it, "SPOT", &val, refstats ? instNoClamp : instClamp);
			}
		}

#ifdef DEBUG
		printf("\nread_sample returned '%s' (%s)\n",
	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
#endif /* DEBUG */

		/* Get any command or trigger character */
		if ((rv & inst_mask) == inst_user_trig
	     || (rv & inst_mask) == inst_user_abort)
			ch = inst_get_uih_char();
		else
			ch = '0';

		/* Do return after command */
		if ((rv & inst_mask) == inst_user_abort && (ch & DUIH_CMND))
			printf("\n");

		/* Deal with a user abort */
		if ((rv & inst_mask) == inst_user_abort && (ch & DUIH_ABORT)) {
			printf("\n\nSpot read stopped at user request!\n");
			printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
			ch = next_con_char();
			if (ch == 0x1b || ch == 0x03 || ch == 'q' || ch == 'Q') {
				printf("\n");
				break;
			}
			printf("\n");
			continue;

		/* Deal with a needs calibration */
		} else if ((rv & inst_mask) == inst_needs_cal) {
			inst_code ev;
			printf("\n\nSpot read failed because instruments needs calibration.\n");
			ev = inst_handle_calibrate(it, inst_calt_needed, inst_calc_none, NULL, NULL, doone);
			if (ev != inst_ok) {	/* Abort or fatal error */
				printf("\nSpot read got abort or error from calibrate\n");
				break;
			}
			continue;

		/* Deal with a bad sensor position */
		} else if ((rv & inst_mask) == inst_wrong_config) {
			printf("\n\nSpot read failed due to the sensor being in the wrong position\n(%s)\n",it->interp_error(it, rv));
			continue;

		/* Deal with a misread */
		} else if ((rv & inst_mask) == inst_misread) {
			empty_con_chars();
			printf("\n\nSpot read failed due to misread (%s)\n",it->interp_error(it, rv));
			printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
			ch = next_con_char();
			printf("\n");
			if (ch == 0x1b || ch == 0x03 || ch == 'q' || ch == 'Q') {
				break;
			}
			continue;

		/* Deal with a communications error */
		} else if ((rv & inst_mask) == inst_coms_fail) {
			empty_con_chars();
			printf("\n\nSpot read failed due to communication problem.\n");
			printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
			ch = next_con_char();
			if (ch == 0x1b || ch == 0x03 || ch == 'q' || ch == 'Q') {
				printf("\n");
				break;
			}
			printf("\n");
			if ((it->icom->port_type(it->icom) & icomt_serial)
			 && !(it->icom->port_attr(it->icom) & icomt_fastserial)) {
				/* Allow retrying at a lower baud rate */
				int tt = it->last_scomerr(it);
				if (tt & (ICOM_BRK | ICOM_FER | ICOM_PER | ICOM_OER)) {
					if (br == baud_57600) br = baud_38400;
					else if (br == baud_38400) br = baud_9600;
					else if (br == baud_9600) br = baud_4800;
					else if (br == baud_9600) br = baud_4800;
					else if (br == baud_2400) br = baud_1200;
					else br = baud_1200;
				}
				if ((rv = it->init_coms(it, br, fc, 15.0)) != inst_ok) {
					printf("init_coms returned '%s' (%s)\n",
				       it->inst_interp_error(it, rv), it->interp_error(it, rv));
					break;
				}
			}
			continue;

		/* Some fatal error */
		} else if ((rv & inst_mask) != inst_ok
		        && (rv & inst_mask) != inst_user_trig
		        && (rv & inst_mask) != inst_user_abort) {
			printf("\n\nGot fatal error '%s' (%s)\n",
				it->inst_interp_error(it, rv), it->interp_error(it, rv));
			break;
		}

		/* Process command or reading */
		ch &= 0xff;
		if (ch == 0x1b || ch == 0x03 || ch == 'q' || ch == 'Q') {	/* Or ^C */
			break;
		}
		if ((cap2 & inst2_has_target) && ch == 't') {	// Toggle target
			inst_code ev;
			if ((ev = it->get_set_opt(it, inst_opt_set_target_state, 2)) != inst_ok) {
				printf("\nToggling target failed with error :'%s' (%s)\n",
		       	       it->inst_interp_error(it, ev), it->interp_error(it, ev));
				it->del(it);
				return -1;
			}
			--ix;
			continue;
		}
		if (ch == 'H' || ch == 'h') {	/* Toggle high res mode */
			if (IMODETST(cap, inst_mode_highres)) {
				inst_code ev;
				/* Hmm. Could simply re-do set_mode() here instead */
				if (highres) {
					if ((ev = it->get_set_opt(it, inst_opt_stdres)) != inst_ok) {
						printf("\nSetting std res mode failed with error :'%s' (%s)\n",
				       	       it->inst_interp_error(it, ev), it->interp_error(it, ev));
						it->del(it);
						return -1;
					}
					highres = 0;
					if (pspec) 
						loghead = 0;
					printf("\n Instrument set to standard resolution spectrum mode\n");
				} else {
					if ((ev = it->get_set_opt(it, inst_opt_highres)) != inst_ok) {
						printf("\nSetting high res mode failed with error :'%s' (%s)\n",
				       	       it->inst_interp_error(it, ev), it->interp_error(it, ev));
						it->del(it);
						return -1;
					}
					highres = 1;
					if (pspec) 
						loghead = 0;
					printf("\n Instrument set to high resolution spectrum mode\n");
				}
			} else {
				printf("\n 'H' Command ignored - instrument doesn't support high res. mode\n");
			}
			--ix;
			continue;
		}
		if (ch == 'R' || ch == 'r') {	/* Make last reading the reference */
			if (Lab[0] >= -9.0) {
				rXYZ[0] = XYZ[0];
				rXYZ[1] = XYZ[1];
				rXYZ[2] = XYZ[2];
				rLab[0] = Lab[0];
				rLab[1] = Lab[1];
				rLab[2] = Lab[2];
				if (pspec) {
					rsp = sp;		/* Save spectral reference too */
				}
				if (refstats) {
					rstat_n = 1;
					for (j = 0; j < 3; j++) {
						rstat_XYZ[j] = XYZ[j];
						rstat_XYZsq[j] = XYZ[j] * XYZ[j];
						rstat_Lab[j] = Lab[j];
						rstat_Labsq[j] = Lab[j] * Lab[j];
					}
				}
				printf("\n Reference is now XYZ: %f %f %f Lab: %f %f %f\n", rXYZ[0], rXYZ[1], rXYZ[2],rLab[0], rLab[1], rLab[2]);
			} else {
				printf("\n No previous reading to use as reference\n");
			}
			--ix;
			continue;
		}
#ifndef SALONEINSTLIB
		if (ch == 'S' || ch == 's') {	/* Save last spectral into file */
			if (sp.spec_n > 0) {
				char buf[500];

				if (sp.spec_n <= 0)
					error("Save: Instrument didn't return spectral data");

				printf("\nEnter filename (ie. xxxx.sp): "); fflush(stdout);
				if (getns(buf, 500) != NULL && strlen(buf) > 0) {
					if(write_xspect(buf, val.mtype, &sp))
						printf("\nWriting file '%s' failed\n",buf);
					else
						printf("\nWriting file '%s' succeeded\n",buf);
				} else {
					printf("\nNo filename, nothing saved\n");
				}
			} else {
				printf("\nNo previous spectral reading to save to file (Use -s flag ?)\n");
			}
			--ix;
			continue;
		}
#endif /* !SALONEINSTLIB */
		if (ch == 'K' || ch == 'k') {	/* Do a calibration */
			inst_code ev;

			/* Should we do a get_n_a_cals() first, so we can abort */
			/* if no calibrations are available ?? */

			printf("\nDoing a calibration\n");

			/* save current location */
			if ((cap2 & inst2_xy_locate) && (cap2 & inst2_xy_position)) {
				for (;;) {		/* retry loop */
					if ((ev = it->xy_get_location(it, &lx, &ly)) == inst_ok)
						break;
					if (ierror(it, ev) == 0) {	/* Ignore */
						--ix;
						continue;
					}
					break;						/* Abort */
				}
				if (ev != inst_ok) {
					printf("\nSpot read got abort or error from xy_get_location\n");
					break;			/* Abort */
				}
			}

			ev = inst_handle_calibrate(it, inst_calt_available, inst_calc_none, NULL, NULL, doone);
			if (ev != inst_ok) {	/* Abort or fatal error */
				printf("\nSpot read got abort or error from calibrate\n");
				break;
			}

			/* restore location */
			if ((cap2 & inst2_xy_locate) && (cap2 & inst2_xy_position)) {
				for (;;) {		/* retry loop */
					if ((ev = it->xy_position(it, 0, lx, ly)) == inst_ok)
						break;
					if (ierror(it, ev) == 0) {	/* Ignore */
						--ix;
						continue;
					}
					break;						/* Abort */
				}
				if (ev != inst_ok) {
					printf("\nSpot read got abort or error from xy_position");
					break;			/* Abort */
				}
			}
			--ix;
			continue;
		}

		/* Measure refresh rate */
		if (ch == 'F') {
			double refr;
			inst_code ev;

			if (!(cap2 & inst2_emis_refr_meas)) {
				printf("\nInstrument isn't capable of refresh rate measurement in current mode\n");
				--ix;
				continue;
			}

			ev = it->read_refrate(it, &refr);
			if (ev == inst_unsupported) {
				printf("\nInstrument isn't capable of refresh rate measurement in current mode\n");
				--ix;
				continue;
			}

			if (ev == inst_misread) {
				printf("\nNo refresh detectable, or measurement failed\n");
				--ix;
				continue;

			} else if (ev != inst_ok) {
				if (ierror(it, ev) == 0) {	/* Ignore */
					--ix;
					continue;
				}
				break;						/* Abort */
			} else {
				printf("\nRefresh rate = %f Hz\n",refr);
			}
			--ix;
			continue;
		}

		/* Report calibrated refresh rate */
		if (ch == 'f') {
			double refr;
			inst_code ev;

			if (!(cap2 & inst2_get_refresh_rate)) {
				printf("\nInstrument isn't capable of refresh rate calibration\n");
				--ix;
				continue;
			}

			ev = it->get_refr_rate(it, &refr);
			if (ev == inst_unsupported) {
				printf("\nInstrument isn't capable of refresh rate calibration\n");
				--ix;
				continue;

			} else if (ev == inst_needs_cal) {
				int refrmode;

				printf("\nRefresh rate hasn't been calibrated\n");

				/* (refrmode may be the default disptype_unknown refrmode) */
				if ((ev = it->get_disptechi(it, NULL, &refrmode, NULL)) != inst_ok) {
					printf("Can't get current refresh mode from instrument\n");
					--ix;
					continue;
				}
				if (!refrmode) {
					printf("Instrument isn't set to a refresh display type\n");
					--ix;
					continue;
				}

				ev = inst_handle_calibrate(it, inst_calt_ref_freq, inst_calc_none, NULL, NULL, doone);

				if (ev != inst_ok) {	/* Abort or fatal error */
					printf("\nSpot read got abort or error from calibrate\n");
					if (ierror(it, ev) == 0) {	/* Ignore */
						--ix;
						continue;
					}
					break;
				}
				ev = it->get_refr_rate(it, &refr);
			}

			if (ev == inst_misread) {
				printf("\nNo refresh, or wasn't able to measure one\n");
				--ix;
				continue;

			} else if (ev != inst_ok) {
				if (ierror(it, ev) == 0) {	/* Ignore */
					--ix;
					continue;
				}
				break;						/* Abort */
			} else {
				printf("\nRefresh rate = %f Hz\n",refr);
			}
			--ix;
			continue;
		}

#ifndef SALONEINSTLIB
		if (ch >= 'A' && ch <= 'Z') {
			printf("\nMeasured media to setup FWA compensation slot '%c'\n",ch);
			sufwa = 1;
			fidx = ch - 'A';
		}
		if (ch >= 'a' && ch <= 'z') {
			fidx = ch - 'a';
			if (fidx < 0 ||  sp2cief[fidx] == NULL) {
				printf("\nUnable to apply FWA compensation because it wasn't set up\n");
				fidx = -1;
			} else {
				dofwa = 1;
			}

		}

		sp = val.sp;		/* Save as last spectral reading */

		/* Setup FWA compensation */
		if (sufwa) {
			double FWAc;
			xspect insp;			/* Instrument illuminant */

			if (sp.spec_n <= 0) {
				error("FWA Setup: Instrument didn't return spectral data");
			}

			if (inst_illuminant(&insp, it->get_itype(it)) != 0)
				error ("Instrument doesn't have an FWA illuminent");

			/* Creat the base conversion object */
			if (sp2cief[fidx] == NULL) {
				if ((sp2cief[fidx] = new_xsp2cie(illum, 0.0, &cust_illum, obType,
				            custObserver, icSigXYZData, refstats ? icxNoClamp : icxClamp)) == NULL)
					error("Creation of spectral conversion object failed");
			}

			if (sp2cief[fidx]->set_fwa(sp2cief[fidx], &insp, tillump, &sp)) 
				error ("Set FWA on sp2cie failed");

			sp2cief[fidx]->get_fwa_info(sp2cief[fidx], &FWAc);
			printf("FWA content = %f\n",FWAc);
		}
#else /* !SALONEINSTLIB */
		if (sufwa) {
			error ("FWA compensation not supported in this version");
		}
#endif /* !SALONEINSTLIB */

		if (sufwa == 0) {		/* Not setting up fwa, so process reading */

			/* Compute the XYZ & Lab */
			if (dofwa == 0 && spec == 0) {
				if (val.XYZ_v == 0)
					error("Instrument didn't return XYZ value");
				
				for (j = 0; j < 3; j++)
					XYZ[j] = val.XYZ[j];
	
			} else {
				/* Compute XYZ given spectral */
	
				if (sp.spec_n <= 0) {
					error("FAW Convert: Instrument didn't return spectral data");
				}
	
				if (dofwa == 0) {
					/* Convert it to XYZ space using uncompensated */
					sp2cie->convert(sp2cie, XYZ, &sp);
				} else {
					/* Convert using FWA compensated conversion */
					sp2cief[fidx]->sconvert(sp2cief[fidx], &sp, XYZ, &sp);
				}
				if (!(emiss || tele || ambient)) {
					for (j = 0; j < 3; j++)
						XYZ[j] *= 100.0;		/* 0..100 scale */
				}
			}

			/* XYZ is 0 .. 100 for reflective/transmissive, and absolute for emissibe here */
			/* XYZ is 0 .. 1 for reflective/transmissive, and absolute for emissibe here */
		}

		/* Print and/or plot out spectrum, */
		/* even if it's an FWA setup */
		if (pspec) {

			if (sp.spec_n <= 0)
				error("Print: Instrument didn't return spectral data");

			printf("Spectrum from %.3f to %.3f nm in %d steps\n",
		                sp.spec_wl_short, sp.spec_wl_long, sp.spec_n);

			for (j = 0; j < sp.spec_n; j++)
				printf("%s%g",j > 0 ? ", " : "", sp.spec[j]);
			printf("\n");

#ifndef SALONEINSTLIB
			/* Plot the spectrum */
			if (pspec == 2) {
				double xx[XSPECT_MAX_BANDS];
				double yy[XSPECT_MAX_BANDS];
				double yr[XSPECT_MAX_BANDS];
				double xmin, xmax, ymin, ymax;
				xspect *ss;				/* Spectrum range to use */
				int nn;

				if (rsp.spec_n > 0) {
					if ((sp.spec_wl_long - sp.spec_wl_short) > 
					    (rsp.spec_wl_long - rsp.spec_wl_short))
						ss = &sp;
					else
						ss = &rsp;
				} else 
					ss = &sp;

				if (sp.spec_n > rsp.spec_n)
					nn = sp.spec_n;
				else
					nn = rsp.spec_n;

				if (nn > XSPECT_MAX_BANDS)
					error("Got > %d spectral values (%d)",XSPECT_MAX_BANDS,nn);

				for (j = 0; j < nn; j++) {
#if defined(__APPLE__) && defined(__POWERPC__)
					gcc_bug_fix(j);
#endif
					xx[j] = ss->spec_wl_short
					      + j * (ss->spec_wl_long - ss->spec_wl_short)/(nn-1);

					yy[j] = value_xspect(&sp, xx[j]);

					if (rLab[0] >= -1.0) {	/* If there is a reference */
						yr[j] = value_xspect(&rsp, xx[j]);
					}
				}
				
				xmax = ss->spec_wl_long;
				xmin = ss->spec_wl_short;
				if (emiss || uvmode) {
					ymin = ymax = 0.0;	/* let it scale */
				} else {
					ymin = 0.0;
					ymax = 120.0;
				}
				do_plot_x(xx, yy, NULL, rLab[0] >= -1.0 ? yr : NULL, nn, 0,
				          xmin, xmax, ymin, ymax, 2.0);
			}
#endif /* !SALONEINSTLIB */
		}

		if (sufwa == 0) {		/* Not setting up fwa, so show reading */

			/* XYZ is 0 .. 100 for reflective/transmissive, and absolute for emissibe here */
			/* XYZ is 0 .. 1 for reflective/transmissive, and absolute for emissibe here */

#ifndef SALONEINSTLIB
			/* Compute color temperatures */
			if (ambient || doCCT) {
				icmXYZNumber wp;
				double nxyz[3], axyz[3];
				double lab[3], alab[3];
				double yxy[3], ayxy[3];

				/* Y normalized sample */
				nxyz[0] = XYZ[0] / XYZ[1];
				nxyz[2] = XYZ[2] / XYZ[1];
				nxyz[1] = XYZ[1] / XYZ[1];

				/* Compute CCT */
				if ((cct = icx_XYZ2ill_ct(axyz, icxIT_Ptemp, obType, custObserver, nxyz, NULL, 0)) < 0)
					error ("Got bad cct\n");
				axyz[0] /= axyz[1];
				axyz[2] /= axyz[1];
				axyz[1] /= axyz[1];
				icmXYZ21960UCS(lab, nxyz);
				icmXYZ21960UCS(alab, axyz);
				cct_de = sqrt((lab[1] - alab[1]) * (lab[1] - alab[1])
				            + (lab[2] - alab[2]) * (lab[2] - alab[2]));

				
				cct_sn = 1.0;
				icmXYZ2Yxy(yxy, nxyz);
				icmXYZ2Yxy(ayxy, axyz);
				/* Dot product of vector from aprox. locus curve "center" */
				/* xy 0.5, 0.25 with vector from	*/
				if ((yxy[1] - ayxy[1]) * (ayxy[1] - 0.5)
				  + (yxy[2] - ayxy[2]) * (ayxy[2] - 0.25) < 0.0)
					cct_sn = -1.0;
			
				/* Compute VCT */
				if ((vct = icx_XYZ2ill_ct(axyz, icxIT_Ptemp, obType, custObserver, nxyz, NULL, 1)) < 0)
					error ("Got bad vct\n");
				axyz[0] /= axyz[1];
				axyz[2] /= axyz[1];
				axyz[1] /= axyz[1];
				icmAry2XYZ(wp, axyz);
				icmXYZ2Lab(&wp, lab, nxyz);
				icmXYZ2Lab(&wp, alab, axyz);
				vct_de = icmCIE2K(lab, alab);

				vct_sn = 1.0;
				icmXYZ2Yxy(yxy, nxyz);
				icmXYZ2Yxy(ayxy, axyz);
				if ((yxy[1] - ayxy[1]) * (ayxy[1] - 0.5)
				  + (yxy[2] - ayxy[2]) * (ayxy[2] - 0.25) < 0.0)
					vct_sn = -1.0;
			
				/* Compute VDT */
				if ((vdt = icx_XYZ2ill_ct(axyz, icxIT_Dtemp, obType, custObserver, nxyz, NULL, 1)) < 0)
					error ("Got bad vct\n");
				axyz[0] /= axyz[1];
				axyz[2] /= axyz[1];
				axyz[1] /= axyz[1];
				icmAry2XYZ(wp, axyz);
				icmXYZ2Lab(&wp, lab, nxyz);
				icmXYZ2Lab(&wp, alab, axyz);
				vdt_de = icmCIE2K(lab, alab);

				vdt_sn = 1.0;
				icmXYZ2Yxy(yxy, nxyz);
				icmXYZ2Yxy(ayxy, axyz);
				if ((yxy[1] - ayxy[1]) * (ayxy[1] - 0.5)
				  + (yxy[2] - ayxy[2]) * (ayxy[2] - 0.25) < 0.0)
					vdt_sn = -1.0;
			}

			/* Compute D50 (or other) Lab from XYZ */
			icmXYZ2Lab(&labwp, Lab, XYZ);

			/* Compute Yxy from XYZ */
			icmXYZ2Yxy(Yxy, XYZ);

			/* Compute LCh from Lab */
			icmLab2LCh(LCh, Lab);

			/* Compute Yuv from XYZ */
			icmXYZ21976UCS(Yuv, XYZ);

#else /* SALONEINSTLIB */
			/* Compute D50 Lab from XYZ */
			XYZ2Lab(Lab, XYZ);

			/* Compute Yxy from XYZ */
			XYZ2Yxy(Yxy, XYZ);

			/* Compute LCh from Lab */
			Lab2LCh(LCh, Lab);
#endif /* SALONEINSTLIB */

			if (emiss > 1 || tele > 1) {
				if (wXYZ[0] < 0.0) {		/* If we haven't save a white ref. yet */
					if (XYZ[1] < 10.0)
						error ("White of XYZ %f %f %f doesn't seem reasonable",XYZ[0], XYZ[1], XYZ[2]);
					printf("\n Making result XYZ: %f %f %f, %s Lab: %f %f %f white reference.\n",
					XYZ[0], XYZ[1], XYZ[2], labwpname, Lab[0], Lab[1], Lab[2]);
					wXYZ[0] = XYZ[0];
					wXYZ[1] = XYZ[1];
					wXYZ[2] = XYZ[2];
#ifndef SALONEINSTLIB
					{	/* Compute a Chromatic adapation matrix to D50 */
						icmXYZNumber s_wp;
						icmAry2XYZ(s_wp, wXYZ);
						icmChromAdaptMatrix(ICM_CAM_BRADFORD, icmD50, s_wp, chmat);
					}
#endif /* !SALONEINSTLIB */
					continue;
				}
				if (emiss == 2 || tele == 2) {
					/* Normalize to white Y value and scale to 0..100 */
					XYZ[0] = 100.0 * XYZ[0] / wXYZ[1];
					XYZ[1] = 100.0 * XYZ[1] / wXYZ[1];
					XYZ[2] = 100.0 * XYZ[2] / wXYZ[1];
				} 
#ifndef SALONEINSTLIB
				  else {	/* emiss == 3, white point relative */

					/* Normalize to white and scale to 0..100 */
					icmMulBy3x3(XYZ, chmat, XYZ);
					icmScale3(XYZ, XYZ, 100.0);
				}

				/* recompute Lab */
				icmXYZ2Lab(&labwp, Lab, XYZ);

				/* recompute Yxy from XYZ */
				icmXYZ2Yxy(Yxy, XYZ);

				/* recompute LCh from Lab */
				icmLab2LCh(LCh, Lab);

				/* recompute Yuv */
				icmXYZ21976UCS(Yuv, XYZ);
#else /* SALONEINSTLIB */
				  else {

					/* Normalize to white */
					XYZ[0] = XYZ[0] * D50_X_100 / wXYZ[0];
					XYZ[1] = XYZ[1] * D50_Y_100 / wXYZ[1];
					XYZ[2] = XYZ[2] * D50_Z_100 / wXYZ[2];
				}

				/* recompute Lab */
				XYZ2Lab(Lab, XYZ);

				/* recompute Yxy from XYZ */
				XYZ2Yxy(Yxy, XYZ);

				/* recompute LCh from Lab */
				Lab2LCh(LCh, Lab);
#endif /* SALONEINSTLIB */
			}

			if (ambient && (cap2 & inst2_ambient_mono)) {
				printf("\n Result is Y: %f, L*: %f\n",XYZ[1], Lab[0]);
			} else {
				if (doYxy) {
					/* Print out the XYZ and Yxy */
					printf("\n Result is XYZ: %f %f %f, Yxy: %f %f %f\n",
					XYZ[0], XYZ[1], XYZ[2], Yxy[0], Yxy[1], Yxy[2]);
				} else if (doLCh) {
					/* Print out the XYZ and LCh */
					printf("\n Result is XYZ: %f %f %f, LCh: %f %f %f\n",
					XYZ[0], XYZ[1], XYZ[2], LCh[0], LCh[1], LCh[2]);
#ifndef SALONEINSTLIB
				} else if (doYuv) {
					/* Print out the XYZ and Yuv */
					printf("\n Result is XYZ: %f %f %f, Yuv: %f %f %f\n",
					XYZ[0], XYZ[1], XYZ[2], Yuv[0], Yuv[1], Yuv[2]);
#endif
				} else {
					/* Print out the XYZ and Lab */
					printf("\n Result is XYZ: %f %f %f, %s Lab: %f %f %f\n",
					XYZ[0], XYZ[1], XYZ[2], labwpname, Lab[0], Lab[1], Lab[2]);
				}
			}

			if (rLab[0] >= -9.0) {
				if (refstats) {
					double avg[3], sdev[3];
					rstat_n++;

					for (j = 0; j < 3; j++) {
						rstat_XYZ[j] += XYZ[j];
						rstat_XYZsq[j] += XYZ[j] * XYZ[j];

						avg[j] = rstat_XYZ[j]/rstat_n;
						sdev[j] = sqrt(rstat_n * rstat_XYZsq[j] - rstat_XYZ[j] * rstat_XYZ[j])/rstat_n;
					}
					printf(" XYZ stats %.0f: Avg %f %f %f, S.Dev %f %f %f\n",
					rstat_n, avg[0], avg[1], avg[2], sdev[0], sdev[1], sdev[2]);

					for (j = 0; j < 3; j++) {
						rstat_Lab[j] += Lab[j];
						rstat_Labsq[j] += Lab[j] * Lab[j];

						avg[j] = rstat_Lab[j]/rstat_n;
						sdev[j] = sqrt(rstat_n * rstat_Labsq[j] - rstat_Lab[j] * rstat_Lab[j])/rstat_n;
					}
					printf(" Lab stats %.0f: Avg %f %f %f, S.Dev %f %f %f\n",
					rstat_n, avg[0], avg[1], avg[2], sdev[0], sdev[1], sdev[2]);
				}
#ifndef SALONEINSTLIB
				printf(" Delta E to reference is %f %f %f (%f, CIE94 %f)\n",
				Lab[0] - rLab[0], Lab[1] - rLab[1], Lab[2] - rLab[2],
				icmLabDE(Lab, rLab), icmCIE94(Lab, rLab));
#else
				printf(" Delta E to reference is %f %f %f (%f)\n",
				Lab[0] - rLab[0], Lab[1] - rLab[1], Lab[2] - rLab[2],
				LabDE(Lab, rLab));
#endif
			}
			if (ambient) {
				if (ambient == 2)
					printf(" Apparent flash duration = %f seconds\n",val.duration);
				if (cap2 & inst2_ambient_mono) {
					printf(" Ambient = %.1f Lux%s\n",
						       XYZ[1], ambient == 2 ? "-Seconds" : "");
					if (ambient != 2) 
						printf(" Suggested EV @ ISO100 for %.1f Lux incident light = %.1f\n",
							       XYZ[1],
						           log(XYZ[1]/2.5)/log(2.0));
				} else {
#ifndef SALONEINSTLIB
					printf(" Ambient = %.1f Lux%s, CCT = %.0fK (Duv %.4f)\n",
					       XYZ[1], ambient == 2 ? "-Seconds" : "",
					       cct, cct_sn * cct_de);
					if (ambient != 2) 
						printf(" Suggested EV @ ISO100 for %.1f Lux incident light = %.1f\n",
							       XYZ[1],
						           log(XYZ[1]/2.5)/log(2.0));
					printf(" Closest Planckian temperature = %.0fK (DE2K %.1f)\n",vct, vct_sn * vct_de);
					printf(" Closest Daylight temperature  = %.0fK (DE2K %.1f)\n",vdt, vdt_sn * vdt_de);
#else /* SALONEINSTLIB */
					printf(" Ambient = %.1f Lux%s\n",
					       XYZ[1], ambient == 2 ? "-Seconds" : "");
#endif /* SALONEINSTLIB */
				}
#ifndef SALONEINSTLIB
			} else if (doCCT) {
				printf("                           CCT = %.0fK (Duv %.4f)\n",cct, cct_sn * cct_de);
				printf(" Closest Planckian temperature = %.0fK (DE2K %.1f)\n",vct, vct_sn * vct_de);
				printf(" Closest Daylight temperature  = %.0fK (DE2K %.1f)\n",vdt, vdt_sn * vdt_de);
#endif
			}
#ifndef SALONEINSTLIB
			if (sp.spec_n > 0 && (ambient || doCCT)) {
				int i, invalid = 0;
				double RR[14];
				double cri;
				cri = icx_CIE1995_CRI(&invalid, RR, &sp);
				printf(" Color Rendering Index (Ra) = %.1f [ R9 = %.1f ]%s\n",
				                       cri, RR[9-1], invalid ? " (Caution)" : "");
				for (i = 0; i < 14; i++) {
					printf("  R%d%s = %.1f", i+1, i < 9 ? " " : "", RR[i]);
					if (i == 6)
						printf("\n");
				}
				printf("\n");
			}
			if (sp.spec_n > 0 && (ambient || doCCT)) {
				int invalid = 0;
				double tlci;
				tlci = icx_EBU2012_TLCI(&invalid, &sp);
				printf(" Television Lighting Consistency Index 2012 (Qa) = %.1f%s\n",tlci,invalid ? " (Caution)" : "");
			}
			if (sp.spec_n > 0 && (ambient || doCCT)) {
				int invalid;
				double Rf, Rg, cct, dc;
				double bins[IES_TM_30_15_BINS][2][3];

				invalid = icx_IES_TM_30_15(&Rf, &Rg, &cct, &dc, bins, &sp);

				printf(" IES TM-30-15 Rf = %.2f Rg = %.2f CCT = %.0f Duv = %f%s\n", Rf, Rg, cct, dc, invalid ? " (Caution)" : "");

#ifdef DO_TM3015_PLOT
				tm3015_plot(bins);
#endif
			}
#endif

			/* Save reading to the log file */
			if (fp != NULL) {
				/* Print some column titles */
				if (loghead == 0) {
					fprintf(fp,"Reading\tX\tY\tZ\tL*\ta*\tb*");
					if (pspec) {	/* Print or plot out spectrum */
						for (j = 0; j < sp.spec_n; j++) {
							double wvl = sp.spec_wl_short 
								      + j * (sp.spec_wl_long - sp.spec_wl_short)/(sp.spec_n-1);
							fprintf(fp,"\t%.3f",wvl);
						}
					}
					fprintf(fp,"\n");
					loghead = 1;
				}
			
				/* Print results */
				fprintf(fp,"%d\t%f\t%f\t%f\t%f\t%f\t%f",
				       ix, XYZ[0], XYZ[1], XYZ[2], Lab[0], Lab[1], Lab[2]);
	
				if (pspec != 0) {
					for (j = 0; j < sp.spec_n; j++)
						fprintf(fp,"\t%g",sp.spec[j]);
				}
				fprintf(fp,"\n");
			}
		}

#ifndef SALONEINSTLIB
		if (doone == 2) {
			if (sp.spec_n <= 0)
				error("Save: Instrument didn't return spectral data");

			if (write_xspect(outspname, val.mtype, &sp))
				printf("Writing file '%s' failed\n",outspname);
			else
				printf("Writing file '%s' succeeded\n",outspname);
		}
#endif

		if (doone)
			break;

	}	/* Next reading */

done:;

	/* Release paper */
	if (cap2 & inst2_xy_holdrel) {
		it->xy_clear(it);
	}

#ifdef DEBUG
	printf("About to exit\n");
#endif

	if (sp2cie != NULL)
		sp2cie->del(sp2cie);
	for (i = 0; i < 26; i++)
		if (sp2cief[i] != NULL)
			sp2cief[i]->del(sp2cief[i]);

	/* Free instrument */
	it->del(it);

	icmps->del(icmps);

	if (fp != NULL)
		fclose(fp);

	return 0;
}




