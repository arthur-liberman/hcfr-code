
/* Colorimeter Correction Matrix and */
/* Colorimeter Calibration Spectral Sample creation utility */

/* 
 * Argyll Color Correction System
 * Author: Graeme W. Gill
 * Date:   19/8/2010
 *
 * Copyright 2010, 2011 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* This program uses display measurements from a colorimeter and */
/* a spectrometer to create a correction matrix for a particular */
/* colorimeter/display combination,. */
/* or */
/* It uses display measurements from a spectrometer to create */
/* calibration samples that can be used with a Colorimeter that */
/* knowns its own spectral sensitivity curves (ie. X-Rite i1d3). */

/* Based on spotread.c, illumread.c, dispcal.c */

/* 
	TTBD:

		Would be nice to have the option of procssing a Spyder 3 correction.txt file.
		(See post from umberto.guidali@tiscali.it)

		Would be nice to have an option of providing two ICC profiles,
		instead of using .ti3 files (?? How well would it work though ?)
 */

#undef DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "cgats.h"
#include "xspect.h"
#include "insttypes.h"
#include "icoms.h"
#include "inst.h"
#include "conv.h"
#include "dispwin.h"
#include "dispsup.h"
#include "ccss.h"
#include "ccmx.h"
#include "spyd2setup.h"

#if defined (NT)
#include <conio.h>
#endif

#define DEFAULT_MSTEPS 2
#undef SHOW_WINDOW_ONFAKE	/* Display a test window up for a fake device */
#define COMPORT 1			/* Default com port 1..4 */
#define VERBOUT stdout


#if defined(__APPLE__) && defined(__POWERPC__)
/* Workaround for a ppc gcc 3.3 optimiser bug... */
static int gcc_bug_fix(int i) {
	static int nn;
	nn += i;
	return nn;
}
#endif	/* APPLE */

/* Invoke with -dfake for testing with a fake device. */
/* Invoke with -dFAKE for automatic creation of test matrix. */
/* Will use a fake.icm/.icc profile if present, or a built in fake */
/* device behaviour if not. */

void
usage(char *diag, ...) {
	disppath **dp;

	fprintf(stderr,"Create CCMX or CCSS, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
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
	fprintf(stderr,"usage: ccmxmake [-options] output.ccmx\n");
	fprintf(stderr," -v                     Verbose mode\n");
	fprintf(stderr," -S                     Create CCSS rather than CCMX\n");
	fprintf(stderr," -f file1.ti3[,file2.ti3] Create from one or two .ti3 files rather than measure.\n");
#if defined(UNIX) && !defined(__APPLE__)
	fprintf(stderr," -display displayname   Choose X11 display name\n");
	fprintf(stderr," -d n[,m]               Choose the display n from the following list (default 1)\n");
	fprintf(stderr,"                        Optionally choose different display m for VideoLUT access\n"); 
#else
	fprintf(stderr," -d n                   Choose the display from the following list (default 1)\n");
#endif
//	fprintf(stderr," -d fake                Use a fake display device for testing, fake%s if present\n",ICC_FILE_EXT);
	dp = get_displays();
	if (dp == NULL || dp[0] == NULL)
		fprintf(stderr,"    ** No displays found **\n");
	else {
		int i;
		for (i = 0; ; i++) {
			if (dp[i] == NULL)
				break;
			fprintf(stderr,"    %d name = '%s'\n",i+1,dp[i]->name);
			fprintf(stderr,"    %d = '%s'\n",i+1,dp[i]->description);
		}
	}
	free_disppaths(dp);
	fprintf(stderr," -p                     Use projector mode (if available)\n");
	fprintf(stderr," -y c|l                 Display type, c = CRT, l = LCD (CCMX)\n");
	fprintf(stderr," -P ho,vo,ss            Position test window and scale it\n");
	fprintf(stderr,"                        ho,vi: 0.0 = left/top, 0.5 = center, 1.0 = right/bottom etc.\n");
	fprintf(stderr,"                        ss: 0.5 = half, 1.0 = normal, 2.0 = double etc.\n");
	fprintf(stderr," -F                     Fill whole screen with black background\n");
#if defined(UNIX) && !defined(__APPLE__)
	fprintf(stderr," -n                     Don't set override redirect on test window\n");
#endif
	fprintf(stderr," -N                     Disable auto calibration of instrument\n");
	fprintf(stderr," -H                     Use high resolution spectrum mode (if available)\n");
	fprintf(stderr," -V                     Use adaptive measurement mode (if available)\n");
	fprintf(stderr," -C \"command\"           Invoke shell \"command\" each time a color is set\n");
	fprintf(stderr," -o observ              Choose CIE Observer for CCMX spectrometer data:\n");
	fprintf(stderr,"                        1931_2 (def), 1964_10, S&B 1955_2, shaw, J&V 1978_2\n");
	fprintf(stderr," -s steps               Override default patch sequence combination steps  (default %d)\n",DEFAULT_MSTEPS);
	fprintf(stderr," -W n|h|x               Override serial port flow control: n = none, h = HW, x = Xon/Xoff\n");
	fprintf(stderr," -D [level]             Print debug diagnostics to stderr\n");
	fprintf(stderr," -E desciption          Override the default overall description\n");
	fprintf(stderr," -I displayname         Set display make and model description\n");
	fprintf(stderr," -T displaytech         Set display technology description (ie. CRT, LCD etc.)\n");
	fprintf(stderr," correction.ccmx | calibration.ccss\n");
	fprintf(stderr,"                        File to save result to\n");
	exit(1);
	}

typedef double ary3[3];

int main(int argc, char *argv[])
{
	int i,j;
	int fa, nfa, mfa;				/* current argument we're looking at */
	disppath *disp = NULL;				/* Display being used */
	double patsize = 100.0;				/* size of displayed color patch */
	double patscale = 1.0;				/* scale factor for test patch size */
	double ho = 0.0, vo = 0.0;			/* Test window offsets, -1.0 to 1.0 */
	int blackbg = 0;            		/* NZ if whole screen should be filled with black */
	int verb = 0;
	int debug = 0;
	int doccss = 0;						/* Create CCSS rather than CCMX */
	int fake = 0;						/* Use the fake device for testing, 2 for auto */
	int faketoggle = 0;					/* Toggle fake between "colorimeter" and "spectro" */
	int fakeseq = 0;					/* Fake auto CCMX sequence */
	int spec = 0;						/* Need spectral data from spectrometer */
	icxObserverType observ = icxOT_CIE_1931_2;
	int override = 1;					/* Override redirect on X11 */
	int comno = COMPORT;				/* COM port used */
	flow_control fc = fc_nc;			/* Default flow control */
	instType itype = instUnknown;		/* Default target instrument - none */
	int highres = 0;					/* High res mode if available */
	int adaptive = 0;					/* Use adaptive mode if available */
	int dtype = 0;						/* Display kind, 0 = default, 1 = CRT, 2 = LCD */
	int proj = 0;						/* NZ if projector */
	int noautocal = 0;					/* Disable auto calibration */
	char *ccallout = NULL;				/* Change color Shell callout */
	int msteps = DEFAULT_MSTEPS;						/* Patch surface size */
	int npat = 0;						/* Number of patches/colors */
	ary3 *refs = NULL;					/* Reference XYZ values */
	int gotref = 0;
	char *refname = NULL;				/* Name of reference instrument */
	ary3 *cols = NULL;					/* Colorimeter XYZ values */
	int gotcol = 0;
	char *colname = NULL;				/* Name of colorimeter instrument */
	col *rdcols = NULL;					/* Internal storage of all the patch colors */
	int saved = 0;						/* Saved result */
	char innames[2][MAXNAMEL+1] = { "\000", "\000" };  /* .ti3 input names */
	char outname[MAXNAMEL+1] = "\000";  /* ccmx output file name */
	char *description = NULL;			/* Given overall description */
	char *displayname = NULL;			/* Given display name */
	char *displaytech = NULL;			/* Given display technology */
	int rv;

	set_exe_path(argv[0]);				/* Set global exe_path and error_program */
	check_if_not_interactive();
	setup_spyd2();					/* Load firware if available */

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

			if (argv[fa][1] == '?') {
				usage("Usage requested");

			} else if (argv[fa][1] == 'v') {
				verb = 1;

			} else if (argv[fa][1] == 'S') {
				doccss = 1;

			} else if (argv[fa][1] == 'f') {
				char *cna, *f1 = NULL;
				fa = nfa;
				if (na == NULL) usage("Expect argument to input file flag -f");

				if ((cna = strdup(na)) == NULL)
					error("Malloc failed");

				/* If got just one file - enough for CCSS */
				if ((f1 = strchr(cna, ',')) == NULL) {
					strncpy(innames[0],cna,MAXNAMEL-1); innames[0][MAXNAMEL-1] = '\000';
					free(cna);

				/* Got two files - needed for CCMX */
				} else {
					*f1++ = '\000';
					strncpy(innames[0],cna,MAXNAMEL-1); innames[0][MAXNAMEL-1] = '\000';
					strncpy(innames[1],f1,MAXNAMEL-1); innames[1][MAXNAMEL-1] = '\000';
					free(cna);
				}

			/* Display number */
			} else if (argv[fa][1] == 'd') {
#if defined(UNIX) && !defined(__APPLE__)
				int ix, iv;

				if (strcmp(&argv[fa][2], "isplay") == 0 || strcmp(&argv[fa][2], "ISPLAY") == 0) {
					if (++fa >= argc || argv[fa][0] == '-') usage("Parameter expected following -display");
					setenv("DISPLAY", argv[fa], 1);
				} else {
					if (na == NULL) usage("Parameter expected following -d");
					fa = nfa;
					if (strcmp(na,"fake") == 0 || strcmp(na,"FAKE") == 0) {
						fake = 1;
						if (strcmp(na,"FAKE") == 0)
							fakeseq = 1;
					} else {
						if (sscanf(na, "%d,%d",&ix,&iv) != 2) {
							ix = atoi(na);
							iv = 0;
						}
						if (disp != NULL)
							free_a_disppath(disp);
						if ((disp = get_a_display(ix-1)) == NULL)
							usage("-d parameter %d out of range",ix);
						if (iv > 0)
							disp->rscreen = iv-1;
					}
				}
#else
				int ix;
				if (na == NULL) usage("Parameter expected following -d");
				fa = nfa;
				if (strcmp(na,"fake") == 0 || strcmp(na,"FAKE") == 0) {
					fake = 1;
					if (strcmp(na,"FAKE") == 0)
						fakeseq = 1;
				} else {
					ix = atoi(na);
					if (disp != NULL)
						free_a_disppath(disp);
					if ((disp = get_a_display(ix-1)) == NULL)
						usage("-d parameter %d out of range",ix);
				}
#endif
#if defined(UNIX) && !defined(__APPLE__)
			} else if (argv[fa][1] == 'n') {
				override = 0;
#endif /* UNIX */

			/* COM port  */
			} else if (argv[fa][1] == 'c') {
				fa = nfa;
				if (na == NULL) usage("Paramater expected following -c");
				comno = atoi(na);
				if (comno < 1 || comno > 40) usage("-c parameter %d out of range",comno);

			/* Projector */
			} else if (argv[fa][1] == 'p') {
				proj = 1;

			/* Display type */
			} else if (argv[fa][1] == 'y' || argv[fa][1] == 'Y') {
				fa = nfa;
				if (na == NULL) usage("Parameter expected after -y");
				if (na[0] == 'c' || na[0] == 'C')
					dtype = 1;
				else if (na[0] == 'l' || na[0] == 'L')
					dtype = 2;
				else
					usage("-y parameter '%c' not recognised",na[0]);

			/* Test patch offset and size */
			} else if (argv[fa][1] == 'P') {
				fa = nfa;
				if (na == NULL) usage("Parameter expected after -P");
				if (sscanf(na, " %lf,%lf,%lf ", &ho, &vo, &patscale) != 3)
					usage("-P parameter '%s' not recognised",na);
				if (ho < 0.0 || ho > 1.0
				 || vo < 0.0 || vo > 1.0
				 || patscale <= 0.0 || patscale > 50.0)
					usage("-P parameters %f %f %f out of range",ho,vo,patscale);
				ho = 2.0 * ho - 1.0;
				vo = 2.0 * vo - 1.0;

			/* Black background */
			} else if (argv[fa][1] == 'F') {
				blackbg = 1;

			/* No auto calibration */
			} else if (argv[fa][1] == 'N') {
				noautocal = 1;

			/* High res spectral mode */
			} else if (argv[fa][1] == 'H') {
				highres = 1;

			/* Adaptive mode */
			} else if (argv[fa][1] == 'V') {
				adaptive = 1;

			/* Spectral Observer type (only relevant for CCMX) */
			} else if (argv[fa][1] == 'o' || argv[fa][1] == 'O') {
				fa = nfa;
				if (na == NULL) usage("Parameter expecte after -o");
				if (strcmp(na, "1931_2") == 0) {			/* Classic 2 degree */
					spec = 2;
					observ = icxOT_CIE_1931_2;
				} else if (strcmp(na, "1964_10") == 0) {	/* Classic 10 degree */
					spec = 2;
					observ = icxOT_CIE_1964_10;
				} else if (strcmp(na, "1955_2") == 0) {		/* Stiles and Burch 1955 2 degree */
					spec = 2;
					observ = icxOT_Stiles_Burch_2;
				} else if (strcmp(na, "1978_2") == 0) {		/* Judd and Voss 1978 2 degree */
					spec = 2;
					observ = icxOT_Judd_Voss_2;
				} else if (strcmp(na, "shaw") == 0) {		/* Shaw and Fairchilds 1997 2 degree */
					spec = 2;
					observ = icxOT_Shaw_Fairchild_2;
				} else
					usage("-o parameter '%s' not recognised",na);

			} else if (argv[fa][1] == 's') {
				fa = nfa;
				if (na == NULL) usage("Parameter expecte after -s");
				msteps = atoi(na);
				if (msteps < 2 || msteps > 16)
					usage("-s parameter value %d is outside the range 2 to 16",msteps);

			/* Change color callout */
			} else if (argv[fa][1] == 'C') {
				fa = nfa;
				if (na == NULL) usage("Parameter expected after -C");
				ccallout = na;

			/* Serial port flow control */
			} else if (argv[fa][1] == 'W') {
				fa = nfa;
				if (na == NULL) usage("Paramater expected following -W");
				if (na[0] == 'n' || na[0] == 'N')
					fc = fc_none;
				else if (na[0] == 'h' || na[0] == 'H')
					fc = fc_Hardware;
				else if (na[0] == 'x' || na[0] == 'X')
					fc = fc_XonXOff;
				else
					usage("-W parameter '%c' not recognised",na[0]);

			} else if (argv[fa][1] == 'D') {
				debug = 1;
				if (na != NULL && na[0] >= '0' && na[0] <= '9') {
					debug = atoi(na);
					fa = nfa;
				}

			} else if (argv[fa][1] == 'I') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to display description flag -I");
				displayname = strdup(na);

			} else if (argv[fa][1] == 'T') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to display technology flag -T");
				displaytech = strdup(na);

			/* Copyright string */
			} else if (argv[fa][1] == 'E') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to overall description flag -E");
				description = strdup(na);
			} else 
				usage("Flag '-%c' not recognised",argv[fa][1]);
		}
		else
			break;
	}

	/* Get the output ccmx file name argument */
	if (fa >= argc)
		usage("Output filname expected");

	strncpy(outname,argv[fa++],MAXNAMEL-1); outname[MAXNAMEL-1] = '\000';

	if (fakeseq && doccss)
		error("Fake CCSS test not implemeted");

	printf("\n");

	if (displayname == NULL && displaytech == NULL)
		error("Either the display description (-I) or technology (-T) needs to be set");

	/* CCSS: See if we're working from a .ti3 file */
	if (doccss && innames[0][0] != '\000') {
		cgats *cgf = NULL;			/* cgats file data */
		int sidx;					/* Sample ID index */
		int ii, ti;
		char buf[100];
		int  spi[XSPECT_MAX_BANDS];	/* CGATS indexes for each wavelength */
		xspect sp, *samples = NULL;
		ccss *cc;
		double bigv = -1e60;

		/* Open spectral values file */
		cgf = new_cgats();			/* Create a CGATS structure */
		cgf->add_other(cgf, ""); 	/* Allow any signature file */
	
		if (cgf->read_name(cgf, innames[0]))
			error("CGATS file '%s' read error : %s",innames[0],cgf->err);
	
		if (cgf->ntables < 1)
			error ("Input file '%s' doesn't contain at least one table",innames[0]);
	
		if ((npat = cgf->t[0].nsets) <= 0)
			error("No sets of data in file '%s'",innames[0]);

		if ((samples = (xspect *)malloc(npat * sizeof(xspect))) == NULL)
				error("malloc failed");

		if ((ii = cgf->find_kword(cgf, 0, "TARGET_INSTRUMENT")) < 0)
			error ("Can't find keyword TARGET_INSTRUMENT in '%s'",innames[0]);

		if ((ii = cgf->find_kword(cgf, 0, "SPECTRAL_BANDS")) < 0)
			error ("Input file '%s' doesn't contain keyword SPECTRAL_BANDS",innames[0]);
		sp.spec_n = atoi(cgf->t[0].kdata[ii]);
		if ((ii = cgf->find_kword(cgf, 0, "SPECTRAL_START_NM")) < 0)
			error ("Input file '%s' doesn't contain keyword SPECTRAL_START_NM",innames[0]);
		sp.spec_wl_short = atof(cgf->t[0].kdata[ii]);
		if ((ii = cgf->find_kword(cgf, 0, "SPECTRAL_END_NM")) < 0)
			error ("Input file '%s' doesn't contain keyword SPECTRAL_END_NM",innames[0]);
		sp.spec_wl_long = atof(cgf->t[0].kdata[ii]);
		sp.norm = 1.0;		/* We assume emssive */

		/* Find the fields for spectral values */
		for (j = 0; j < sp.spec_n; j++) {
			int nm;
	
			/* Compute nearest integer wavelength */
			nm = (int)(sp.spec_wl_short + ((double)j/(sp.spec_n-1.0))
			            * (sp.spec_wl_long - sp.spec_wl_short) + 0.5);
			
			sprintf(buf,"SPEC_%03d",nm);

			if ((spi[j] = cgf->find_field(cgf, 0, buf)) < 0)
				error("Input file '%s' doesn't contain field %s",innames[0],buf);
		}

		/* Transfer all the spectral values */
		for (i = 0; i < npat; i++) {

			XSPECT_COPY_INFO(&samples[i], &sp);
	
			for (j = 0; j < sp.spec_n; j++) {
				samples[i].spec[j] = *((double *)cgf->t[0].fdata[i][spi[j]]);
			}
		}
		cgf->del(cgf);		/* Clean up */
		cgf = NULL;

		if (description == NULL) {
			char *disp = displaytech != NULL ? displaytech : displayname;
			char *tt = "CCSS for ";
			if ((description = malloc(strlen(disp) + strlen(tt) + 1)) == NULL)
				error("Malloc failed");
			strcpy(description, tt);
			strcat(description, disp);
		}

		/* See what the highest value is */
		for (i = 0; i < npat; i++) {	/* For all grid points */

			for (j = 0; j < samples[i].spec_n; j++) {
				if (samples[i].spec[j] > bigv)
					bigv = samples[i].spec[j];
			}
		}
			
		/* Normalize the values */
		for (i = 0; i < npat; i++) {	/* For all grid points */
			double scale = 100.0;

			for (j = 0; j < samples[i].spec_n; j++)
				samples[i].spec[j] *= scale / bigv;
		}
			
		if ((cc = new_ccss()) == NULL)
			error("new_ccss() failed");

		if (cc->set_ccss(cc, "Argyll ccxxmake", NULL, description, displayname,
		                 displaytech, refname, samples, npat)) {
			error("set_ccss failed with '%s'\n",cc->err);
		}
		if(cc->write_ccss(cc, outname))
			printf("\nWriting file '%s' failed\n",outname);
		else
			printf("\nWriting file '%s' succeeded\n",outname);
		cc->del(cc);
		free(samples);

#ifdef DEBUG
		printf("About to exit\n");
#endif
		return 0;
	}

	/* CCMX: See if we're working from two files */
	if (!doccss && innames[0][0] != '\000') {
		int n;
		char *oname = NULL;			/* Observer name */
		ccmx *cc;

		if (innames[1][0] == '\000') {
			error("Need two .ti3 files to create CCMX");
		}

		/* Open up each CIE file in turn, target then measured, */
		/* and read in the CIE values. */
		for (n = 0; n < 2; n++) {
			cgats *cgf = NULL;			/* cgats file data */
			int isLab = 0;				/* 0 if file CIE is XYZ, 1 if is Lab */
			double wxyz[3], scale = 1.0;/* Scale factor back to absolute */
			int sidx;					/* Sample ID index */
			int xix, yix, zix;
			int cur_ref_col;			/* 1 = reference, 0 = colorimeter */
			ary3 *current = NULL;		/* Current value array */
			int ii, ti;
			instType itype;
			int instspec = 0;			/* Instrument is spectral/reference */

			/* Open CIE target values */
			cgf = new_cgats();			/* Create a CGATS structure */
			cgf->add_other(cgf, ""); 	/* Allow any signature file */
		
			if (cgf->read_name(cgf, innames[n]))
				error("CGATS file '%s' read error : %s",innames[n],cgf->err);
		
			if (cgf->ntables < 1)
				error ("Input file '%s' doesn't contain at least one table",innames[n]);
		
			/* Check if the file is suitable */
			if (cgf->find_field(cgf, 0, "LAB_L") < 0
			 && cgf->find_field(cgf, 0, "XYZ_X") < 0) {
		
				error ("No CIE data found in file '%s'",innames[n]);
			}
		
			if (cgf->find_field(cgf, 0, "LAB_L") >= 0)
				isLab = 1;
			
			if (cols == NULL) {
				if ((npat = cgf->t[0].nsets) <= 0)
					error("No sets of data in file '%s'",innames[n]);

				if ((refs = (ary3 *)malloc(npat * sizeof(ary3))) == NULL)
					error("malloc failed");
				if ((cols = (ary3 *)malloc(npat * sizeof(ary3))) == NULL)
					error("malloc failed");

			} else {
				if (npat != cgf->t[0].nsets)
					error ("Number of sets %d in file '%s' doesn't match other file %d",cgf->t[0].nsets,innames[n],npat);
			}

			if ((ii = cgf->find_kword(cgf, 0, "TARGET_INSTRUMENT")) < 0)
				error ("Can't find keyword TARGET_INSTRUMENT in '%s'",innames[n]);
	
			if ((ti = cgf->find_kword(cgf, 0, "INSTRUMENT_TYPE_SPECTRAL")) < 0)
				error ("Can't find keyword INSTRUMENT_TYPE_SPECTRAL in '%s'",innames[n]);
	
			if (strcmp(cgf->t[0].kdata[ti],"YES") == 0) {
				instspec = 1;
				cur_ref_col = 1;
				current = refs;
				refname = strdup(cgf->t[0].kdata[ii]);
				if (gotref)
					error("Need spectral reference and colorimtric file");
				gotref = 1;
				
			} else if (strcmp(cgf->t[0].kdata[ti],"NO") == 0) {
				instspec = 0;
				cur_ref_col = 0;
				current = cols;
				colname = strdup(cgf->t[0].kdata[ii]);
				if (gotcol)
					error("Need spectral reference and colorimtric file");
				gotcol = 1;
			} else {
				error ("Unknown INSTRUMENT_TYPE_SPECTRAL value '%s'",cgf->t[0].kdata[ti]);
			}

			if ((ti = cgf->find_kword(cgf, 0, "NORMALIZED_TO_Y_100")) < 0
			 || strcmp(cgf->t[0].kdata[ti],"NO") == 0) {
				scale = 1.0;		/* Leave absolute */
			} else {
				if ((ti = cgf->find_kword(cgf, 0, "LUMINANCE_XYZ_CDM2")) < 0)
					error ("Can't find keyword LUMINANCE_XYZ_CDM2 in '%s'",innames[n]);
				if (sscanf(cgf->t[0].kdata[ti],"%lf %lf %lf", &wxyz[0], &wxyz[1], &wxyz[2]) != 3)
					error ("Unable to parse LUMINANCE_XYZ_CDM2 in '%s'",innames[n]);
				scale = wxyz[1]/100.0;	/* Convert from Y = 100 normalise back to absolute */
			}

			if (instspec && spec) {
				int ii;
				xspect sp;
				char buf[100];
				int  spi[XSPECT_MAX_BANDS];	/* CGATS indexes for each wavelength */
				xsp2cie *sp2cie;	/* Spectral conversion object */

				if ((ii = cgf->find_kword(cgf, 0, "SPECTRAL_BANDS")) < 0)
					error ("Input file '%s' doesn't contain keyword SPECTRAL_BANDS",innames[n]);
				sp.spec_n = atoi(cgf->t[0].kdata[ii]);
				if ((ii = cgf->find_kword(cgf, 0, "SPECTRAL_START_NM")) < 0)
					error ("Input file '%s' doesn't contain keyword SPECTRAL_START_NM",innames[n]);
				sp.spec_wl_short = atof(cgf->t[0].kdata[ii]);
				if ((ii = cgf->find_kword(cgf, 0, "SPECTRAL_END_NM")) < 0)
					error ("Input file '%s' doesn't contain keyword SPECTRAL_END_NM",innames[n]);
				sp.spec_wl_long = atof(cgf->t[0].kdata[ii]);
				sp.norm = 1.0;		/* We assume emssive */

				/* Find the fields for spectral values */
				for (j = 0; j < sp.spec_n; j++) {
					int nm;
			
					/* Compute nearest integer wavelength */
					nm = (int)(sp.spec_wl_short + ((double)j/(sp.spec_n-1.0))
					            * (sp.spec_wl_long - sp.spec_wl_short) + 0.5);
					
					sprintf(buf,"SPEC_%03d",nm);

					if ((spi[j] = cgf->find_field(cgf, 0, buf)) < 0)
						error("Input file '%s' doesn't contain field %s",innames[n],buf);
				}

				/* Create a spectral conversion object */
				if ((sp2cie = new_xsp2cie(icxIT_none, NULL, observ, NULL, icSigXYZData)) == NULL)
					error("Creation of spectral conversion object failed");

				for (i = 0; i < npat; i++) {
	
					/* Read the spectral values for this patch */
					for (j = 0; j < sp.spec_n; j++) {
						sp.spec[j] = *((double *)cgf->t[0].fdata[i][spi[j]]) * scale;
					}
	
					/* Convert it to CIE space */
					sp2cie->convert(sp2cie, current[i], &sp);
				}
				sp2cie->del(sp2cie);        /* Done with this */

			} else {

				if (isLab) {		/* Expect Lab */
					if ((xix = cgf->find_field(cgf, 0, "LAB_L")) < 0)
						error("Input file '%s' doesn't contain field LAB_L",innames[n]);
					if (cgf->t[0].ftype[xix] != r_t)
						error("Field LAB_L is wrong type");
					if ((yix = cgf->find_field(cgf, 0, "LAB_A")) < 0)
						error("Input file '%s' doesn't contain field LAB_A",innames[n]);
					if (cgf->t[0].ftype[yix] != r_t)
						error("Field LAB_A is wrong type");
					if ((zix = cgf->find_field(cgf, 0, "LAB_B")) < 0)
						error("Input file '%s' doesn't contain field LAB_B",innames[n]);
					if (cgf->t[0].ftype[zix] != r_t)
						error("Field LAB_B is wrong type");

				} else { 		/* Expect XYZ */
					if ((xix = cgf->find_field(cgf, 0, "XYZ_X")) < 0)
						error("Input file '%s' doesn't contain field XYZ_X",innames[n]);
					if (cgf->t[0].ftype[xix] != r_t)
						error("Field XYZ_X is wrong type");
					if ((yix = cgf->find_field(cgf, 0, "XYZ_Y")) < 0)
						error("Input file '%s' doesn't contain field XYZ_Y",innames[n]);
					if (cgf->t[0].ftype[yix] != r_t)
						error("Field XYZ_Y is wrong type");
					if ((zix = cgf->find_field(cgf, 0, "XYZ_Z")) < 0)
						error("Input file '%s' doesn't contain field XYZ_Z",innames[n]);
					if (cgf->t[0].ftype[zix] != r_t)
						error("Field XYZ_Z is wrong type");
				}

				for (i = 0; i < npat; i++) {
					current[i][0] = *((double *)cgf->t[0].fdata[i][xix]);
					current[i][1] = *((double *)cgf->t[0].fdata[i][yix]);
					current[i][2] = *((double *)cgf->t[0].fdata[i][zix]);
					if (isLab) {	/* Convert test patch Lab to XYZ scale 100 */
						icmLab2XYZ(&icmD50_100, current[i], current[i]);
					}
					/* Rescale to absolute if needed */
					current[i][0] *= scale;
					current[i][1] *= scale;
					current[i][2] *= scale;
				}
			}
			cgf->del(cgf);		/* Clean up */
			cgf = NULL;
		}

		if (spec != 0 && observ != icxOT_CIE_1931_2)
			oname = standardObserverDescription(observ);

		if (oname != NULL) {
			char *tt = colname;

			if ((colname = malloc(strlen(tt) + strlen(oname) + 3)) == NULL)
				error("Malloc failed");
			strcpy(colname, tt);
			strcat(colname, " (");
			strcat(colname, oname);
			strcat(colname, ")");
		}
		if (description == NULL) {
			char *disp = displaytech != NULL ? displaytech : displayname;
			if ((description = malloc(strlen(colname) + strlen(disp) + 4)) == NULL)
				error("Malloc failed");
			strcpy(description, colname);
			strcat(description, " & ");
			strcat(description, disp);
		}
		if ((cc = new_ccmx()) == NULL)
			error("new_ccmx() failed");

		if (cc->create_ccmx(cc, description, colname, displayname, displaytech, refname,
               npat, refs, cols)) {
			error("create_ccmx failed with '%s'\n",cc->err);
		}
		if (verb) {
			printf("Fit error is max %f, avg %f DE94\n",cc->mx_err,cc->av_err);
			printf("Correction matrix is:\n");
			printf("  %f %f %f\n", cc->matrix[0][0], cc->matrix[0][1], cc->matrix[0][2]);
			printf("  %f %f %f\n", cc->matrix[1][0], cc->matrix[1][1], cc->matrix[1][2]);
			printf("  %f %f %f\n", cc->matrix[2][0], cc->matrix[2][1], cc->matrix[2][2]);
		}

		if(cc->write_ccmx(cc, outname))
			printf("\nWriting file '%s' failed\n",outname);
		else
			printf("\nWriting file '%s' succeeded\n",outname);
		cc->del(cc);

	/* Do interactive measurements */
	} else {

		if (!doccss && dtype == 0) {
			printf("**** Warning: display type (-y option) hasn't been set! ****\n");
			printf("****     The colorimeter may not work without this!     ****\n\n");
		}

		/* No explicit display has been set */
		if (
#ifndef SHOW_WINDOW_ONFAKE
		!fake && 
#endif
		 disp == NULL) {
			int ix = 0;
#if defined(UNIX) && !defined(__APPLE__)
			char *dn, *pp;

			if ((dn = getenv("DISPLAY")) != NULL) {
				if ((pp = strrchr(dn, ':')) != NULL) {
					if ((pp = strchr(pp, '.')) != NULL) {
						if (pp[1] != '\000')
							ix = atoi(pp+1);
					}
				}
			}
#endif
			if ((disp = get_a_display(ix)) == NULL)
				error("Unable to open the default display");

			if (displayname == NULL && (displayname = strdup(disp->description)) == NULL)
				error("Malloc failed");

			printf("Display description is '%s'\n",displayname);
		}
		if (fake) {
			displayname = strdup("fake display");
		}

		/* Create grid of device test values */
		{
			int j;
			int gc[3];			/* Grid coordinate */

			npat = msteps * msteps * msteps;
			if ((rdcols = (col *)malloc(npat * sizeof(col))) == NULL) {
				error("malloc failed");
			}
			if ((refs = (ary3 *)malloc(npat * sizeof(ary3))) == NULL) {
				free(rdcols);
				error("malloc failed");
			}
			if ((cols = (ary3 *)malloc(npat * sizeof(ary3))) == NULL) {
				free(rdcols);
				free(refs);
				error("malloc failed");
			}

			for (j = 0; j < 3; j++)
				gc[j] = 0;			/* init coords */
				
			for (npat = 0; ;) {	/* For all grid points */

				/* Just colors with at least one channel at 100% */
				if (gc[0] == (msteps-1)
				 || gc[1] == (msteps-1)
				 || gc[2] == (msteps-1)) 
				{
					
					rdcols[npat].r = (double)gc[0]/(msteps-1);
					rdcols[npat].g = (double)gc[1]/(msteps-1);
					rdcols[npat].b = (double)gc[2]/(msteps-1);
#ifdef DEBUG
					printf("Dev val %f %f %f\n",rdcols[npat].r,rdcols[npat].g,rdcols[npat].b);
#endif
					npat++;
				}
				
				/* Increment grid index and position */
				for (j = 0; j < 3; j++) {
					gc[j]++;
					if (gc[j] < msteps)
						break;	/* No carry */
					gc[j] = 0;
				}
				if (j >= 3)
					break;		/* Done grid */
			}
			if (verb)
				printf("Total test patches = %d\n",npat);
		}

		/* Until the measurements are done, or we give up */
		for (;;) {
			int c;

			/* Print the menue of adjustments */
			printf("\n");
			if (gotref)
				printf("[Got spectrometer readings]\n");
			if (gotcol)
				printf("[Got colorimeter readings]\n");
			printf("Press 1 .. 4:\n");
			{
				icoms *icom;
				printf("1) Select an instrument, Currently %d (", comno);
				if ((icom = new_icoms()) != NULL) {
					icompath **paths;
					if ((paths = icom->get_paths(icom)) != NULL) {
						int i;
						for (i = 0; ; i++) {
							if (paths[i] == NULL)
								break;
							if ((i+1) == comno) {
								printf(" '%s'",paths[i]->path);
								break;
							}
						}
					}
					icom->del(icom);
				}
				printf(")\n");
			}
			if (doccss)
				printf("2) Measure test patches with current (spectrometer) instrument\n");
			else
				printf("2) Measure test patches with current instrument\n");

			if (doccss) {
				if (gotref)
					printf("3) Save Colorimeter Calibration Spectral Set\n");
				else
					printf("3) [ Save Colorimeter Calibration Spectral Set ]\n");

			} else {
				if (gotref && gotcol)
					printf("3) Compute Colorimeter Correction Matrix & save it\n");
				else
					printf("3) [ Compute Colorimeter Correction Matrix & save it ]\n");
			}
			printf("4) Exit\n");

			if (fakeseq == 0) {
				empty_con_chars();
				c = next_con_char();
			} else {
				switch (fakeseq) {
					case 1:
						c = '2';
						fakeseq = 2;
						break;
					case 2:
						c = '2';
						fakeseq = 3;
						break;
					case 3:
						c = '3';
						fakeseq = 4;
						break;
					default:
						c = '4';
						break;
				}
			}
			printf("'%c'\n",c);


			/* Deal with selecting the instrument */
			if (c == '1') {
				icoms *icom;
				itype = instUnknown;

				if ((icom = new_icoms()) != NULL) {
					icompath **paths;
					if ((paths = icom->get_paths(icom)) != NULL) {
						int i;
						for (i = 0; ; i++) {
							if (paths[i] == NULL)
								break;
							if (strlen(paths[i]->path) >= 8
							  && strcmp(paths[i]->path+strlen(paths[i]->path)-8, "Spyder2)") == 0
							  && setup_spyd2() == 0)
								fprintf(stderr,"    %d = '%s' !! Disabled - no firmware !!\n",i+1,paths[i]->path);
							else
								fprintf(stderr,"    %d = '%s'\n",i+1,paths[i]->path);
						}
						printf("Select device 1 - %d: \n",i);
						empty_con_chars();
						c = next_con_char();

						if (c < '1' || c > ('0' + i)) {
							printf("'%c' is out of range - ignored !\n",c);
						} else {
							comno = c - '0'; 
						}
						
					} else {
						fprintf(stderr,"No ports to select from!\n");
					}
					icom->del(icom);
				}
				continue;
			}

			/* Deal with doing a measurement */
			if (c == '2') {
				int errc;				/* Return value from new_disprd() */
				disprd *dr;				/* Display patch read object */
				inst *it;				/* Instrument */
				inst_capability  cap = inst_unknown;	/* Instrument capabilities */
				inst2_capability cap2 = inst2_unknown;	/* Instrument capabilities 2 */

				if ((dr = new_disprd(&errc, itype, fake ? -99 : comno, fc, dtype, proj, adaptive,
				                     noautocal, highres, 1, NULL, 0, 0, disp, blackbg, override,
				                     ccallout, NULL, patsize, ho, vo, NULL, NULL, 0, 2,
				                     icxOT_default, NULL, 
				                     0, 0, verb, VERBOUT, debug, "fake" ICC_FILE_EXT)) == NULL)
					error("new_disprd failed with '%s'\n",disprd_err(errc));
			
				it = dr->it;

				if (fake) {
					if (faketoggle)
						cap = inst_spectral;
					else
						cap = inst_colorimeter;
					cap2 = 0;
				} else {
					cap = it->capabilities(it);
					cap2 = it->capabilities2(it);
				}

				if (doccss && (cap & inst_spectral) == 0) {
					printf("You have to use a spectrometer to create a CCSS!\n");
					continue;
				}

				if (faketoggle)
					dr->fake2 = 1;
				else
					dr->fake2 = -1;

				/* Test the CRT with all of the test points */
				if ((rv = dr->read(dr, rdcols, npat, 1, npat, 1, 0)) != 0) {
					dr->del(dr);
					error("disprd returned error code %d\n",rv);
				}

				if (doccss) {		/* We'll use the rdcols values */
					gotref = 1;
				} else {
					if (cap & inst_spectral) {
						xsp2cie *sp2cie = NULL;

						if (spec) {
							/* Create a spectral conversion object */
							if ((sp2cie = new_xsp2cie(icxIT_none, NULL, observ, NULL, icSigXYZData)) == NULL)
								error("Creation of spectral conversion object failed");
						}
						for (i = 0; i < npat; i++) {	/* For all grid points */
							if (spec) {
								if (rdcols[i].sp.spec_n <= 0)
									error("Didn't get spectral value");
								sp2cie->convert(sp2cie, refs[i], &rdcols[i].sp);
							} else {
								if (rdcols[i].aXYZ_v == 0)
									error("Didn't get absolute XYZ value");
								refs[i][0] = rdcols[i].aXYZ[0];
								refs[i][1] = rdcols[i].aXYZ[1];
								refs[i][2] = rdcols[i].aXYZ[2];
							}
						}
						if (fake)
							refname = "fake spectrometer";
						else
							refname = inst_name(it->itype);
						gotref = 1;
						if (sp2cie != NULL)
							sp2cie->del(sp2cie);
					} else if (cap & inst_colorimeter) {
						for (i = 0; i < npat; i++) {	/* For all grid points */
							if (rdcols[i].aXYZ_v == 0)
								error("Didn't get absolute XYZ value");
							cols[i][0] = rdcols[i].aXYZ[0];
							cols[i][1] = rdcols[i].aXYZ[1];
							cols[i][2] = rdcols[i].aXYZ[2];
						}
						if (fake)
							colname = "fake colorimeter";
						else
							colname = inst_name(it->itype);
						gotcol = 1;
					}
				}
				dr->del(dr);

				faketoggle ^= 1;

			}	/* End of take a measurement */

			if (c == '3') {		/* Compute result and save */
				/* Save the CCSS */
				if (doccss) {
					ccss *cc;
					xspect *samples = NULL;
					double bigv = -1e60;

					if (!gotref) {
						printf("You have to read the spectrometer values first!\n");
						continue;
					}

					if (description == NULL) {
						char *disp = displaytech != NULL ? displaytech : displayname;
						char *tt = "CCSS for ";
						if ((description = malloc(strlen(disp) + strlen(tt) + 1)) == NULL)
							error("Malloc failed");
						strcpy(description, tt);
						strcat(description, disp);
					}

					if ((samples = (xspect *)malloc(sizeof(xspect) * npat)) == NULL)
						error("Malloc failed");
					
					/* See what the highest value is */
					for (i = 0; i < npat; i++) {	/* For all grid points */
						if (rdcols[i].sp.spec_n <= 0)
							error("Didn't get spectral values");
						for (j = 0; j < rdcols[i].sp.spec_n; j++) {
							if (rdcols[i].sp.spec[j] > bigv)
								bigv = rdcols[i].sp.spec[j];
						}
					}
						
					/* Copy all the values and normalize them */
					for (i = 0; i < npat; i++) {	/* For all grid points */
						double scale = 100.0;

						/* Scale by half if not white, */
						/* to give color samples and white 1/4 weight each */
						if (rdcols[i].r < 0.9999999999
						 || rdcols[i].g < 0.9999999999
						 || rdcols[i].b < 0.9999999999)
							scale = 50.0;

						samples[i] = rdcols[i].sp;	/* Structure copy */
						for (j = 0; j < rdcols[i].sp.spec_n; j++)
							samples[i].spec[j] *= scale / bigv;
					}
						
					if ((cc = new_ccss()) == NULL)
						error("new_ccss() failed");
	
					if (cc->set_ccss(cc, "Argyll ccxxmake", NULL, description, displayname,
					                 displaytech, refname, samples, npat)) {
						error("set_ccss failed with '%s'\n",cc->err);
					}
					if(cc->write_ccss(cc, outname))
						printf("\nWriting file '%s' failed\n",outname);
					else
						printf("\nWriting file '%s' succeeded\n",outname);
					cc->del(cc);
					free(samples);
					saved = 1;

				/* Compute and save CCMX */
				} else {
					char *oname = NULL;		/* Observer desciption */
					ccmx *cc;
	
					if (!gotref) {
						printf("You have to read the spectrometer values first!\n");
						continue;
					}
					if (!gotcol) {
						printf("You have to read the colorimeter values first!\n");
						continue;
					}
	
					if (spec != 0 && observ != icxOT_CIE_1931_2)
						oname = standardObserverDescription(observ);
	
					if (oname != NULL) {		/* Incorporate observer name in colname */
						char *tt = colname;
	
						if ((colname = malloc(strlen(tt) + strlen(oname) + 3)) == NULL)
							error("Malloc failed");
						strcpy(colname, tt);
						strcat(colname, " (");
						strcat(colname, oname);
						strcat(colname, ")");
					}
					if (description == NULL) {
						if ((description = malloc(strlen(colname) + strlen(displayname) + 4)) == NULL)
							error("Malloc failed");
						strcpy(description, colname);
						strcat(description, " & ");
						strcat(description, displayname);
					}
					if ((cc = new_ccmx()) == NULL)
						error("new_ccmx() failed");
	
					if (cc->create_ccmx(cc, description, colname, displayname, displaytech,
					                    refname, npat, refs, cols)) {
						error("create_ccmx failed with '%s'\n",cc->err);
					}
					if (verb) {
						printf("Fit error is avg %f, max %f DE94\n",cc->av_err,cc->mx_err);
						printf("Correction matrix is:\n");
						printf("  %f %f %f\n", cc->matrix[0][0], cc->matrix[0][1], cc->matrix[0][2]);
						printf("  %f %f %f\n", cc->matrix[1][0], cc->matrix[1][1], cc->matrix[1][2]);
						printf("  %f %f %f\n", cc->matrix[2][0], cc->matrix[2][1], cc->matrix[2][2]);
					}
	
					if(cc->write_ccmx(cc, outname))
						printf("\nWriting file '%s' failed\n",outname);
					else
						printf("\nWriting file '%s' succeeded\n",outname);
					cc->del(cc);
					saved = 1;
				}
			}

			if (c == '4' || c == 0x3) {		/* Exit */
				if (!saved) {
					printf("Not saved yet, are you sure ? (y/n): "); fflush(stdout);
					empty_con_chars();
					c = next_con_char();
					printf("\n");
					if (c != 'y' && c != 'Y')
						continue;
				}
				break;
			}

		}	/* Next command */

		free(displayname);
	}

#ifdef DEBUG
	/* Do a CCMX verification */
	if (!doccss) {
		ccmx *cc;
		double av_err, mx_err;
		int wix;
		double maxy = -1e6;
		icmXYZNumber wh;

		for (i = 0; i < npat; i++) {
			if (refs[i][1] > maxy) {
				maxy = refs[i][1];
				wix = i;
			}
		}
		wh.X = refs[wix][0];
		wh.Y = refs[wix][1];
		wh.Z = refs[wix][2];

		if ((cc = new_ccmx()) == NULL)
			error("new_ccmx() failed");
		if(cc->read_ccmx(cc, outname))
			printf("Reading file '%s' failed\n",outname);

		av_err = mx_err = 0.0;
		for (i = 0; i < npat; i++) {
			double txyz[3], tlab[3], xyz[3], _xyz[3], lab[3], de;
			icmCpy3(txyz, refs[i]);
			icmXYZ2Lab(&wh, tlab, txyz);
			icmCpy3(xyz, cols[i]);
			cc->xform(cc,_xyz, xyz);
			icmXYZ2Lab(&wh, lab, _xyz);
			de = icmCIE94(tlab, lab);
			av_err += de;
			if (de > mx_err)
				mx_err = de;
			printf("%d: txyz %f %f %f\n",i,txyz[0], txyz[1], txyz[2]);
			printf("%d:  xyz %f %f %f, _xyz %f %f %f\n",i,xyz[0], xyz[1], xyz[2], _xyz[0], _xyz[1], _xyz[2]);
			printf("%d: tlab %f %f %f, lab %f %f %f\n",i,tlab[0], tlab[1], tlab[2], lab[0], lab[1], lab[2]);
			printf("%d: de %f\n",i,de);
		}
		av_err /= npat;
		printf("Avg = %f, max = %f\n",av_err,mx_err);
		cc->del(cc);
	}
#endif

#ifdef DEBUG
	printf("About to exit\n");
#endif

	return 0;
}




