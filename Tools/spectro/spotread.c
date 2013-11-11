
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
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "cgats.h"
#include "xicc.h"
#include "conv.h"
#include "plot.h"
#else /* SALONEINSTLIB */
#include "sa_config.h"
#include "numsup.h"
#include "xspect.h"
#include "conv.h"
#endif /* SALONEINSTLIB */
#include "ccss.h"
#include "ccmx.h"
#include "inst.h"
#include "icoms.h"
#include "instappsup.h"
#include "spyd2setup.h"
#ifdef EN_SPYD2
#include "spyd2setup.h"
#endif

#if defined (NT)
#include <conio.h>
#endif

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

/* A color structure */
/* This can hold all representations simultaniously */
typedef struct {
	double gy;
	double r,g,b;
	double cmyk[4];
	double XYZ[4];		/* Colorimeter readings */
	double eXYZ[4];		/* Expected XYZ values */
	xspect sp;			/* Spectral reading */
	
	char *id;			/* Id string */
	char *loc;			/* Location string */
	int loci;			/* Location integer = pass * 256 + step */
} col;

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

/*

  Flags used:

         ABCDEFGHIJKLMNOPQRSTUVWXYZ
  upper     ... .  .  .  . .. ...  
  lower  . .... ..      .  .. . .. 

*/

void
usage(char *diag, ...) {
	int i;
	icompaths *icmps;
	inst2_capability cap2 = 0;
	fprintf(stderr,"Measure spot values, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the GPL Version 2 or later\n");
	if (setup_spyd2() == 2)
		fprintf(stderr,"WARNING: This file contains a proprietary firmware image, and may not be freely distributed !\n");
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
	fprintf(stderr," -c listno            Set communication port from the following list (default %d)\n",COMPORT);
	if ((icmps = new_icompaths(g_log)) != NULL) {
		icompath **paths;
		if ((paths = icmps->paths) != NULL) {
			int i;
			for (i = 0; ; i++) {
				if (paths[i] == NULL)
					break;
				if (paths[i]->itype == instSpyder2 && setup_spyd2() == 0)
					fprintf(stderr,"    %d = '%s' !! Disabled - no firmware !!\n",i+1,paths[i]->name);
				else
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
	fprintf(stderr," -i illum             Choose illuminant for computation of CIE XYZ from spectral data & FWA:\n");
#ifndef SALONEINSTLIB
	fprintf(stderr,"                       A, C, D50 (def.), D50M2, D65, F5, F8, F10 or file.sp\n");
#else
	fprintf(stderr,"                       A, C, D50 (def.), D65\n");
#endif
	fprintf(stderr," -Q observ            Choose CIE Observer for spectral data or CCSS instrument:\n");
#ifndef SALONEINSTLIB
	fprintf(stderr,"                      1931_2 (def), 1964_10, S&B 1955_2, shaw, J&V 1978_2\n");
#else
	fprintf(stderr,"                      1931_2 (def), 1964_10\n");
#endif
#ifndef SALONEINSTLIB
	fprintf(stderr,"                      (Choose FWA during operation)\n");
#endif
	fprintf(stderr," -F filter            Set filter configuration (if aplicable):\n");
	fprintf(stderr,"    n                  None\n");
	fprintf(stderr,"    p                  Polarising filter\n");
	fprintf(stderr,"    6                  D65\n");
	fprintf(stderr,"    u                  U.V. Cut\n");
	fprintf(stderr," -E extrafilterfile   Apply extra filter compensation file\n");
	fprintf(stderr," -x                   Display Yxy instead of Lab\n");
	fprintf(stderr," -h                   Display LCh instead of Lab\n");
	fprintf(stderr," -V                   Show running average and std. devation from ref.\n");
#ifndef SALONEINSTLIB
	fprintf(stderr," -T                   Display correlated color temperatures and CRI\n");
#endif /* !SALONEINSTLIB */
//	fprintf(stderr," -K type              Run instrument calibration first\n");
	fprintf(stderr," -N                   Disable auto calibration of instrument\n");
	fprintf(stderr," -H                   Start in high resolution spectrum mode (if available)\n");
	if (cap2 & inst2_ccmx)
		fprintf(stderr," -X file.ccmx         Apply Colorimeter Correction Matrix\n");
	if (cap2 & inst2_ccss) {
		fprintf(stderr," -X file.ccss         Use Colorimeter Calibration Spectral Samples for calibration\n");
	}
	fprintf(stderr," -Y r|n               Override refresh, non-refresh display mode\n");
	fprintf(stderr," -Y R:rate            Override measured refresh rate with rate Hz\n");
	fprintf(stderr," -Y A                 Use non-adaptive integration time mode (if available).\n");
//	fprintf(stderr," -Y U                 Test i1pro2 UV measurement mode\n");
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
	int pspec = 0;					/* 1 = Print out the spectrum for each reading */
									/* 2 = Plot out the spectrum for each reading */
	int trans = 0;					/* Use transmissioin mode */
	int emiss = 0;					/* 1 = Use emissive mode, 2 = display bright rel. */
	                                /* 3 = display white rel. */
	int tele = 0;					/* 1 = Use telephoto emissive sub-mode. */
	int ambient = 0;				/* 1 = Use ambient emissive mode, 2 = ambient flash mode */
	int highres = 0;				/* Use high res mode if available */
	int uvmode = 0;					/* ~~~ i1pro2 test mode ~~~ */
	int refrmode = -1;				/* -1 = default, 0 = non-refresh mode, 1 = refresh mode */
	double refrate = 0.0;			/* 0.0 = default, > 0.0 = override refresh rate */ 
	int nadaptive = 0;				/* Use non-apative mode if available */
	int doYxy= 0;					/* Display Yxy instead of Lab */
	int doLCh= 0;					/* Display LCh instead of Lab */
	int doCCT= 0;					/* Display correlated color temperatures */
	inst_mode mode = 0, smode = 0;	/* Normal mode and saved readings mode */
	inst_opt_type trigmode = inst_opt_unknown;	/* Chosen trigger mode */
	inst_opt_filter fe = inst_opt_filter_unknown;
									/* Filter configuration */
	char outname[MAXNAMEL+1] = "\000";  /* Output logfile name */
	char ccxxname[MAXNAMEL+1] = "\000";  /* Colorimeter Correction/Colorimeter Calibration name */
	char filtername[MAXNAMEL+1] = "\000";  /* Filter compensation */
	FILE *fp = NULL;				/* Logfile */
	icompaths *icmps = NULL;
	int comport = COMPORT;				/* COM port used */
	icompath *ipath = NULL;
	int dtype = 0;					/* Display type selection charater */
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
	icxIllumeType tillum = icxIT_none;	/* Target/simulated instrument illuminant */ 
	xspect cust_tillum, *tillump = NULL; /* Custom target/simulated illumination spectrum */
	icxIllumeType illum = icxIT_D50;	/* Spectral defaults */
	xspect cust_illum;				/* Custom illumination spectrum */
	icxObserverType obType = icxOT_default;
	xspect sp;						/* Last spectrum read */
	xspect rsp;						/* Reference spectrum */
	xsp2cie *sp2cie = NULL;			/* default conversion */
	xsp2cie *sp2cief[26];			/* FWA corrected conversions */
	double wXYZ[3] = { -10.0, 0, 0 };/* White XYZ for display white relative */
	double chmat[3][3];				/* Chromatic adapation matrix */
	double Lab[3] = { -10.0, 0, 0};	/* Last Lab */
	double rXYZ[3] = { 0.0, -10.0, 0};	/* Reference XYZ */
	double rLab[3] = { -10.0, 0, 0};	/* Reference Lab */
	double Yxy[3] = { 0.0, 0, 0};	/* Yxy value */
	double LCh[3] = { 0.0, 0, 0};	/* LCh value */
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
	setup_spyd2();				/* Load firware if available */

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
				comport = atoi(na);
				if (comport < 1 || comport > 40) usage("-c parameter %d out of range",comport);

			/* Display type */
			} else if (argv[fa][1] == 'y') {
				fa = nfa;
				if (na == NULL) usage("Paramater expected following -y");
				dtype = na[0];

#ifndef SALONEINSTLIB
			/* Simulated instrument illumination (FWA) */
			} else if (argv[fa][1] == 'I') {

				fa = nfa;
				if (na == NULL) usage("Paramater expected following -I");
				if (strcmp(na, "A") == 0
				 || strcmp(na, "M0") == 0) {
					spec = 1;
					tillum = icxIT_A;
				} else if (strcmp(na, "C") == 0) {
					spec = 1;
					tillum = icxIT_C;
				} else if (strcmp(na, "D50") == 0
				        || strcmp(na, "M1") == 0) {
					spec = 1;
					tillum = icxIT_D50;
				} else if (strcmp(na, "D50M2") == 0
				        || strcmp(na, "M2") == 0) {
					spec = 1;
					tillum = icxIT_D50M2;
				} else if (strcmp(na, "D65") == 0) {
					spec = 1;
					tillum = icxIT_D65;
				} else if (strcmp(na, "F5") == 0) {
					spec = 1;
					tillum = icxIT_F5;
				} else if (strcmp(na, "F8") == 0) {
					spec = 1;
					tillum = icxIT_F8;
				} else if (strcmp(na, "F10") == 0) {
					spec = 1;
					tillum = icxIT_F10;
				} else {	/* Assume it's a filename */
					spec = 1;
					tillum = icxIT_custom;
					if (read_xspect(&cust_tillum, na) != 0)
						usage("Failed to read custom target illuminant spectrum in file '%s'",na);
				}
#endif /* SALONEINSTLIB */

			/* Spectral Illuminant type for XYZ computation */
			} else if (argv[fa][1] == 'i') {
				fa = nfa;
				if (na == NULL) usage("Paramater expected following -i");
				if (strcmp(na, "A") == 0) {
					spec = 1;
					illum = icxIT_A;
				} else if (strcmp(na, "C") == 0) {
					spec = 1;
					illum = icxIT_C;
				} else if (strcmp(na, "D50") == 0) {
					spec = 1;
					illum = icxIT_D50;
				} else if (strcmp(na, "D50M2") == 0) {
					spec = 1;
					illum = icxIT_D50M2;
				} else if (strcmp(na, "D65") == 0) {
					spec = 1;
					illum = icxIT_D65;
#ifndef SALONEINSTLIB
				} else if (strcmp(na, "F5") == 0) {
					spec = 1;
					illum = icxIT_F5;
				} else if (strcmp(na, "F8") == 0) {
					spec = 1;
					illum = icxIT_F8;
				} else if (strcmp(na, "F10") == 0) {
					spec = 1;
					illum = icxIT_F10;
				} else {	/* Assume it's a filename */
					spec = 1;
					illum = icxIT_custom;
					if (read_xspect(&cust_illum, na) != 0)
						usage("Unable to read custom illuminant file '%s'",na);
				}
#else /* SALONEINSTLIB */
				} else
					usage("Unrecognised illuminant '%s'",na);
#endif /* SALONEINSTLIB */

			/* Spectral Observer type */
			} else if (argv[fa][1] == 'Q') {
				fa = nfa;
				if (na == NULL) usage("Paramater expected following -Q");
				if (strcmp(na, "1931_2") == 0) {			/* Classic 2 degree */
					obType = icxOT_CIE_1931_2;
				} else if (strcmp(na, "1964_10") == 0) {	/* Classic 10 degree */
					obType = icxOT_CIE_1964_10;
#ifndef SALONEINSTLIB
				} else if (strcmp(na, "1955_2") == 0) {		/* Stiles and Burch 1955 2 degree */
					obType = icxOT_Stiles_Burch_2;
				} else if (strcmp(na, "1978_2") == 0) {		/* Judd and Vos 1978 2 degree */
					obType = icxOT_Judd_Vos_2;
				} else if (strcmp(na, "shaw") == 0) {		/* Shaw and Fairchilds 1997 2 degree */
					obType = icxOT_Shaw_Fairchild_2;
#endif /* !SALONEINSTLIB */
				} else
					usage("Spectral observer type '%s' not recognised",na);

			/* Request transmission measurement */
			} else if (argv[fa][1] == 't') {
				emiss = 0;
				trans = 1;
				tele = 0;
				ambient = 0;

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
						emiss = 2;
					else if (argv[fa][2] == 'w' || argv[fa][2] == 'W')
						emiss = 3;
					else
						usage("-p modifier '%c' not recognised",argv[fa][2]);
				}

			/* Request ambient measurement */
			} else if (argv[fa][1] == 'a') {
				emiss = 1;
				trans = 0;
				tele = 0;
				ambient = 1;

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

			/* Show Yxy */
			} else if (argv[fa][1] == 'x') {
				doYxy = 1;
				doLCh = 0;

			/* Show LCh */
			} else if (argv[fa][1] == 'h') {
				doYxy = 0;
				doLCh = 1;

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

			/* High res mode */
			} else if (argv[fa][1] == 'H') {
				highres = 1;

			/* Colorimeter Correction Matrix or */
			/* Colorimeter Calibration Spectral Samples */
			} else if (argv[fa][1] == 'X') {
				int ix;
				fa = nfa;
				if (na == NULL) usage("Parameter expected after -K");
				strncpy(ccxxname,na,MAXNAMEL-1); ccxxname[MAXNAMEL-1] = '\000';

			/* Extra flags */
			} else if (argv[fa][1] == 'Y') {
				if (na == NULL)
					usage("Parameter expected after -Y");
			
				if (na[0] == 'A') {
					nadaptive = 1;
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
					fc = fc_none;
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

	/* - - - - - - - - - - - - - - - - - - -  */
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
			if (!IMODETST(cap, inst_mode_trans_spot)
			 || it->check_mode(it, inst_mode_trans_spot) != inst_ok) {
				printf("Need transmission spot capability,\n");
				printf("and instrument doesn't support it\n");
				it->del(it);
				return -1;
			}

		} else if (ambient == 1) {
			if (!IMODETST(cap, inst_mode_emis_ambient)
			 || it->check_mode(it, inst_mode_emis_ambient) != inst_ok) {
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
			if (!IMODETST(cap, inst_mode_emis_ambient_flash)
			 || it->check_mode(it, inst_mode_emis_ambient_flash) != inst_ok) {
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
			if (!IMODETST(cap, inst_mode_emis_spot)
			 && IMODETST(cap, inst_mode_emis_tele)) {
				tele = 1;
			}

			if (tele) {
				if (!IMODETST(cap, inst_mode_emis_tele)
			 || it->check_mode(it, inst_mode_emis_tele) != inst_ok) {
					printf("Need telephoto spot capability\n");
					printf("and instrument doesn't support it\n");
					it->del(it);
					return -1;
				}
			} else {
				if (!IMODETST(cap, inst_mode_emis_spot)
			 || it->check_mode(it, inst_mode_emis_spot) != inst_ok) {
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
			if (refrmode >= 0 && !IMODETST(cap, inst_mode_emis_refresh_ovd)
			                  && !IMODETST(cap, inst_mode_emis_norefresh_ovd)) {
				if (verb) {
					printf("Requested refresh mode override and instrument doesn't support it (ignored)\n");
					refrmode = -1;
				}
			}
			/* Set display type */
			if (dtype != 0) {

				if (cap2 & inst2_disptype) {
					int ix;
					if ((ix = inst_get_disptype_index(it, dtype, 0)) < 0) {
						it->del(it);
						usage("Failed to locate display type matching '%c'",dtype);
					}
		
					if ((rv = it->set_disptype(it, ix)) != inst_ok) {
						printf("Setting display type ix %d not supported by instrument\n",ix);
						it->del(it);
						return -1;
					}
				} else
					printf("Display type ignored - instrument doesn't support display type\n");
			}

		} else {
			if (!IMODETST(cap, inst_mode_ref_spot)
			 || it->check_mode(it, inst_mode_ref_spot) != inst_ok) {
				printf("Need reflection spot reading capability,\n");
				printf("and instrument doesn't support it\n");
				it->del(it);
				return -1;
			}
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
		if (highres) {
			if (IMODETST(cap, inst_mode_highres)) {
				inst_code ev;
				if ((ev = it->get_set_opt(it, inst_opt_highres)) != inst_ok) {
					printf("\nSetting high res mode failed with error :'%s' (%s)\n",
			       	       it->inst_interp_error(it, ev), it->interp_error(it, ev));
					it->del(it);
					return -1;
				}
				highres = 1;
			} else if (verb) {
				printf("high resolution ignored - instrument doesn't support high res. mode\n");
			}
		}

		/* Set it to the appropriate mode */

		/* Should look at instrument type & user spec ??? */
		if (trans)
			smode = mode = inst_mode_trans_spot;
		else if (ambient == 1 && IMODETST(cap, inst_mode_emis_ambient)
			 && it->check_mode(it, inst_mode_emis_ambient) == inst_ok)
			smode = mode = inst_mode_emis_ambient;
		else if (ambient == 2 && IMODETST(cap, inst_mode_emis_ambient_flash)
			 && it->check_mode(it, inst_mode_emis_ambient_flash) == inst_ok)
			smode = mode = inst_mode_emis_ambient_flash;
		else if (tele)							// Hmm. What about tele flash ?
			smode = mode = inst_mode_emis_tele;
		else if (emiss || ambient)
			smode = mode = inst_mode_emis_spot;
		else {
			smode = mode = inst_mode_ref_spot;
			if (IMODETST(cap, inst_mode_s_ref_spot)
			 && it->check_mode(it, inst_mode_s_ref_spot) == inst_ok)
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

		/* Apply Extra filter compensation */
		if (filtername[0] != '\000') {
			if ((rv = it->comp_filter(it, filtername)) != inst_ok) {
				printf("\nSetting filter compensation failed with error :'%s' (%s)\n",
			     	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
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
				if ((rv = it->col_cor_mat(it, cx->matrix)) != inst_ok) {
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
				if ((rv = it->get_set_opt(it, inst_opt_set_ccss_obs, obType, NULL)) != inst_ok) {
					printf("\nSetting CCS Observer failed with error :'%s' (%s)\n",
				     	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
					cs->del(cs);
					it->del(it);
					return -1;
				}
				if ((rv = it->col_cal_spec_set(it, cs->samples, cs->no_samp)) != inst_ok) {
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
			if ((rv = it->get_set_opt(it, inst_opt_set_ccss_obs, obType, 0)) != inst_ok) {
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

		/* If it batter powered, show the status of the battery */
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

#ifdef DEBUG
	printf("About to enter read loop\n");
#endif

	if (verb)
		printf("Init instrument success !\n");

	if (spec) {
		/* Any non-illuminated mode has no illuminant */
		if (emiss || tele || ambient)
			illum = icxIT_none;

		/* Create a spectral conversion object */
		if ((sp2cie = new_xsp2cie(illum, &cust_illum, obType, NULL, icSigXYZData,
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
		double XYZ[3];			/* XYZ scaled 0..100 or absolute */
		double tXYZ[3];
#ifndef SALONEINSTLIB
		double cct, vct, vdt;
		double cct_de, vct_de, vdt_de;
#endif /* !SALONEINSTLIB */
		int ch = '0';		/* Character */
		int sufwa = 0;		/* Setup for FWA compensation */
		int dofwa = 0;		/* Do FWA compensation */
		int fidx = -1;		/* FWA compensated index, default = none */
	
		if (savdrd != -1 && IMODETST(cap, inst_mode_s_ref_spot)
			 && it->check_mode(it, inst_mode_s_ref_spot) == inst_ok) {
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

		/* Read a stored value */
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

				ev = inst_handle_calibrate(it, inst_calt_needed, inst_calc_none, NULL, NULL);
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
		printf("read_sample returned '%s' (%s)\n",
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
			ev = inst_handle_calibrate(it, inst_calt_needed, inst_calc_none, NULL, NULL);
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
			if (it->icom->port_type(it->icom) == icomt_serial) {
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
				printf("\nEnter filename (ie. xxxx.sp): "); fflush(stdout);
				if (getns(buf, 500) != NULL && strlen(buf) > 0) {
					if(write_xspect(buf, &sp))
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

			ev = inst_handle_calibrate(it, inst_calt_available, inst_calc_none, NULL, NULL);
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

				if ((ev = it->get_set_opt(it, inst_opt_get_dtinfo, &refrmode, NULL)) != inst_ok) {
					printf("Can't get curretn refresh mode from instrument\n");
					--ix;
					continue;
				}
				if (!refrmode) {
					printf("Instrument isn't set to a refresh display type\n");
					--ix;
					continue;
				}

				ev = inst_handle_calibrate(it, inst_calt_ref_freq, inst_calc_none, NULL, NULL);

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

		/* Setup FWA compensation */
		if (sufwa) {
			double FWAc;
			xspect insp;			/* Instrument illuminant */

			if (val.sp.spec_n <= 0) {
				error("Instrument didn't return spectral data");
			}

			if (inst_illuminant(&insp, it->get_itype(it)) != 0)
				error ("Instrument doesn't have an FWA illuminent");

			/* Creat the base conversion object */
			if (sp2cief[fidx] == NULL) {
				if ((sp2cief[fidx] = new_xsp2cie(illum, &cust_illum, obType,
				            NULL, icSigXYZData, refstats ? icxNoClamp : icxClamp)) == NULL)
					error("Creation of spectral conversion object failed");
			}

			if (sp2cief[fidx]->set_fwa(sp2cief[fidx], &insp, tillump, &val.sp)) 
				error ("Set FWA on sp2cie failed");

			sp2cief[fidx]->get_fwa_info(sp2cief[fidx], &FWAc);
			printf("FWA content = %f\n",FWAc);
		}
#else /* !SALONEINSTLIB */
		if (sufwa) {
			error ("FWA compensation not supported in this version");
		}
#endif /* !SALONEINSTLIB */

		/* Print and/or plot out spectrum, */
		/* even if it's an FWA setup */
		if (pspec) {
			xspect tsp;

			if (val.sp.spec_n <= 0)
				error("Instrument didn't return spectral data");

			tsp = val.sp;		/* Temp. save spectral reading */

			/* Compute FWA corrected spectrum */
			if (dofwa != 0) {
				sp2cief[fidx]->sconvert(sp2cief[fidx], &tsp, NULL, &tsp);
			}

			printf("Spectrum from %f to %f nm in %d steps\n",
		                tsp.spec_wl_short, tsp.spec_wl_long, tsp.spec_n);

			for (j = 0; j < tsp.spec_n; j++)
				printf("%s%8.3f",j > 0 ? ", " : "", tsp.spec[j]);
			printf("\n");

#ifndef SALONEINSTLIB
			/* Plot the spectrum */
			if (pspec == 2) {
				double xx[XSPECT_MAX_BANDS];
				double yy[XSPECT_MAX_BANDS];
				double yr[XSPECT_MAX_BANDS];
				double xmin, xmax, ymin, ymax;
				xspect *ss;		/* Spectrum range to use */
				int nn;

				if (rsp.spec_n > 0) {
					if ((tsp.spec_wl_long - tsp.spec_wl_short) > 
					    (rsp.spec_wl_long - rsp.spec_wl_short))
						ss = &tsp;
					else
						ss = &rsp;
				} else 
					ss = &tsp;

				if (tsp.spec_n > rsp.spec_n)
					nn = tsp.spec_n;
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

					yy[j] = value_xspect(&tsp, xx[j]);

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
				do_plot_x(xx, yy, rLab[0] >= -1.0 ? yr : NULL, NULL, nn, 1,
				          xmin, xmax, ymin, ymax, 2.0);
			}
#endif /* !SALONEINSTLIB */
		}

		if (sufwa == 0) {		/* Not setting up fwa, so show reading */

			sp = val.sp;		/* Save as last spectral reading */

			/* Compute the XYZ & Lab */
			if (dofwa == 0 && spec == 0) {
				if (val.XYZ_v == 0)
					error("Instrument didn't return XYZ value");
				
				for (j = 0; j < 3; j++)
					XYZ[j] = val.XYZ[j];
	
			} else {
				/* Compute XYZ given spectral */
	
				if (sp.spec_n <= 0) {
					error("Instrument didn't return spectral data");
				}
	
				if (dofwa == 0) {
					/* Convert it to XYZ space using uncompensated */
					sp2cie->convert(sp2cie, XYZ, &sp);
				} else {
					/* Convert using compensated conversion */
					sp2cief[fidx]->sconvert(sp2cief[fidx], &sp, XYZ, &sp);
				}
				if (!(emiss || tele || ambient)) {
					for (j = 0; j < 3; j++)
						XYZ[j] *= 100.0;		/* 0..100 scale */
				}
			}

			/* XYZ is 0 .. 100 for reflective/transmissive, and absolute for emissibe here */
			/* XYZ is 0 .. 1 for reflective/transmissive, and absolute for emissibe here */

#ifndef SALONEINSTLIB
			/* Compute color temperatures */
			if (ambient || doCCT) {
				icmXYZNumber wp;
				double nxyz[3], axyz[3];
				double lab[3], alab[3];

				nxyz[0] = XYZ[0] / XYZ[1];
				nxyz[2] = XYZ[2] / XYZ[1];
				nxyz[1] = XYZ[1] / XYZ[1];

				/* Compute CCT */
				if ((cct = icx_XYZ2ill_ct(axyz, icxIT_Ptemp, obType, NULL, XYZ, NULL, 0)) < 0)
					error ("Got bad cct\n");
				axyz[0] /= axyz[1];
				axyz[2] /= axyz[1];
				axyz[1] /= axyz[1];
				icmAry2XYZ(wp, axyz);
				icmXYZ2Lab(&wp, lab, nxyz);
				icmXYZ2Lab(&wp, alab, axyz);
				cct_de = icmLabDE(lab, alab);
			
				/* Compute VCT */
				if ((vct = icx_XYZ2ill_ct(axyz, icxIT_Ptemp, obType, NULL, XYZ, NULL, 1)) < 0)
					error ("Got bad vct\n");
				axyz[0] /= axyz[1];
				axyz[2] /= axyz[1];
				axyz[1] /= axyz[1];
				icmAry2XYZ(wp, axyz);
				icmXYZ2Lab(&wp, lab, nxyz);
				icmXYZ2Lab(&wp, alab, axyz);
				vct_de = icmLabDE(lab, alab);

				/* Compute VDT */
				if ((vdt = icx_XYZ2ill_ct(axyz, icxIT_Dtemp, obType, NULL, XYZ, NULL, 1)) < 0)
					error ("Got bad vct\n");
				axyz[0] /= axyz[1];
				axyz[2] /= axyz[1];
				axyz[1] /= axyz[1];
				icmAry2XYZ(wp, axyz);
				icmXYZ2Lab(&wp, lab, nxyz);
				icmXYZ2Lab(&wp, alab, axyz);
				vdt_de = icmLabDE(lab, alab);
			}

			/* Compute D50 Lab from XYZ */
			icmXYZ2Lab(&icmD50_100, Lab, XYZ);

			/* Compute Yxy from XYZ */
			icmXYZ2Yxy(Yxy, XYZ);

			/* Compute LCh from Lab */
			icmLab2LCh(LCh, Lab);

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
					printf("\n Making result XYZ: %f %f %f, D50 Lab: %f %f %f white reference.\n",
					XYZ[0], XYZ[1], XYZ[2], Lab[0], Lab[1], Lab[2]);
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
//					XYZ[0] = XYZ[0] * icmD50_100.X / wXYZ[0];
//					XYZ[1] = XYZ[1] * icmD50_100.Y / wXYZ[1];
//					XYZ[2] = XYZ[2] * icmD50_100.Z / wXYZ[2];
					icmMulBy3x3(XYZ, chmat, XYZ);
					icmScale3(XYZ, XYZ, 100.0);
				}

				/* recompute Lab */
				icmXYZ2Lab(&icmD50_100, Lab, XYZ);

				/* recompute Yxy from XYZ */
				icmXYZ2Yxy(Yxy, XYZ);

				/* recompute LCh from Lab */
				icmLab2LCh(LCh, Lab);
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
				} else {
					/* Print out the XYZ and Lab */
					printf("\n Result is XYZ: %f %f %f, D50 Lab: %f %f %f\n",
					XYZ[0], XYZ[1], XYZ[2], Lab[0], Lab[1], Lab[2]);
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
					printf(" Ambient = %.1f Lux%s, CCT = %.0fK (Delta E %f)\n",
					       XYZ[1], ambient == 2 ? "-Seconds" : "",
					       cct, cct_de);
					if (ambient != 2) 
						printf(" Suggested EV @ ISO100 for %.1f Lux incident light = %.1f\n",
							       XYZ[1],
						           log(XYZ[1]/2.5)/log(2.0));
					printf(" Closest Planckian temperature = %.0fK (Delta E %f)\n",vct, vct_de);
					printf(" Closest Daylight temperature  = %.0fK (Delta E %f)\n",vdt, vdt_de);
#else /* SALONEINSTLIB */
					printf(" Ambient = %.1f Lux%s\n",
					       XYZ[1], ambient == 2 ? "-Seconds" : "");
#endif /* SALONEINSTLIB */
				}
#ifndef SALONEINSTLIB
			} else if (doCCT) {
				printf("                           CCT = %.0fK (Delta E %f)\n",cct, cct_de);
				printf(" Closest Planckian temperature = %.0fK (Delta E %f)\n",vct, vct_de);
				printf(" Closest Daylight temperature  = %.0fK (Delta E %f)\n",vdt, vdt_de);
#endif
			}
#ifndef SALONEINSTLIB
			if (val.sp.spec_n > 0 && (ambient || doCCT)) {
				int invalid = 0;
				double cri;
				cri = icx_CIE1995_CRI(&invalid, &sp);
				printf(" Color Rendering Index (Ra) = %.1f%s\n",cri,invalid ? " (Invalid)" : "");
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
							fprintf(fp,"\t%3f",wvl);
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
						fprintf(fp,"\t%8.3f",sp.spec[j]);
				}
				fprintf(fp,"\n");
			}
		}
	}	/* Next reading */

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




