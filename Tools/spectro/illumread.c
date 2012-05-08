
 /* Illuminant measurement utility */

/* 
 * Argyll Color Correction System
 * Author: Graeme W. Gill
 * Date:   3/10/2001
 *
 * Derived from printread.c/chartread.c
 * Was called printspot.
 *
 * Copyright 2001 - 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* This program uses an instrument capable of spectral illuminant/emissive */
/* and reflective (non-UV filtered) measurement, to measure an illuminant */
/* spectrum that includes a UV estimate. */

/* Based on spotread.c */

/*
	TTBD:
	
	Should add support for converting Spyder correction readings to ccmx.
	(see SpyderDeviceCorrections.txt)

 */

#undef DEBUG			/* Save measurements and restore them to debug_X.sp */
#define SHOWDXX			/* Plot the best matched daylight as well */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "cgats.h"
#include "xicc.h"
#include "conv.h"
#include "inst.h"
#include "icoms.h"
#include "plot.h"
#include "spyd2setup.h"			/* Enable Spyder 2 access */

#include <stdarg.h>

#if defined (NT)
#include <conio.h>
#endif

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

#if defined(__APPLE__) && defined(__POWERPC__)

/* Workaround for a ppc gcc 3.3 optimiser bug... */
static int gcc_bug_fix(int i) {
	static int nn;
	nn += i;
	return nn;
}

#define GCC_BUGFIX(XX) gcc_bug_fix(XX);
#else	/* !APPLE */
#define GCC_BUGFIX(XX)
#endif	/* !APPLE */


/* ============================================================== */
/* optimizer callback */

/* Structure to hold optimisation information */
typedef struct {
	xspect *i_sp;		/* Illuminiant measurement */
	xspect *r_sp;		/* Illuminant off paper measurement */
	xspect *p_sp;		/* Paper reflectance measurement */

	xsp2cie *pap;		/* Estimated illuminant + UV on paper lookup inc FWA */
	xsp2cie *ref;		/* Measured illuminant on paper lookup */

	xspect ill;			/* Illuminant with UV added */
	xspect cpsp;		/* FWA corrected calculated paper reflectance */
	xspect srop;		/* Scaled measured paper reflectance */

	double lab0[3], lab1[3];	/* Conversion of calculated vs. measured */
} bfinds;

/* Optimize the UV content that minimizes the (weighted) Delta E */
static double bfindfunc(void *adata, double pv[]) {
	bfinds *b = (bfinds *)adata;
	int i;
	double rv = 0.0;

	/* Add UV level to illuminant */
	b->ill = *b->i_sp;
	xsp_setUV(&b->ill, b->i_sp, pv[0]);

	/* Update the conversion */
	if (b->pap->update_fwa_custillum(b->pap, &b->ill) != 0) 
		error ("Updating FWA compensation failed");

	/* Apply FWA compensation to the paper reflectance */
	b->pap->sconvert(b->pap, &b->cpsp, b->lab0, b->p_sp);

	/* Adjust gain factor of measured illuminant off paper */
	/* and divide by illuminant measurement to give reflectance */
	b->srop = *b->r_sp;
	for (i = 0; i < b->r_sp->spec_n; i++) {
		double ww = XSPECT_XWL(b->r_sp, i);
		double ival = value_xspect(b->i_sp, ww);
		if (ival < 0.05)		/* Stop noise sending it wild */
			ival = 0.05;
		b->srop.spec[i] *= pv[1]/ival;
		if (b->srop.spec[i] < 0.0)
			b->srop.spec[i] = 0.0;
	}

	/* Compute Lab of the computed reflectance under flat spectrum */
	b->ref->convert(b->ref, b->lab0, &b->cpsp);

	/* Compute Lab value of illuminant measured reflectance */
	b->ref->convert(b->ref, b->lab1, &b->srop);

	/* Weighted Delta E (low weight on a* error) */
	rv = sqrt(      (b->lab0[0] - b->lab1[0]) * (b->lab0[0] - b->lab1[0])
	        + 0.1 * (b->lab0[1] - b->lab1[1]) * (b->lab0[1] - b->lab1[1]) 
	        +       (b->lab0[2] - b->lab1[2]) * (b->lab0[2] - b->lab1[2])); 

	/* Add a slight weight of the UV level, to minimize it */
	/* if the case is unconstrained. */  
	rv += 0.1 * fabs(pv[0]);

//printf("~1 rev = %f (%f %f %f - %f %f %f) from %f %f\n",rv, b->lab0[0], b->lab0[1], b->lab0[2], b->lab1[0], b->lab1[1], b->lab1[2], pv[0], pv[1]);
	return rv;
}

/* ============================================================== */
#ifdef SHOWDXX			/* Plot the best matched daylight as well */

/* optimizer callback for computing daylight match */

/* Structure to hold optimisation information */
typedef struct {
	xspect ill;			/* Illuminant with UV added - target */
	xspect dxx;			/* Daylight spectrum */

	xsp2cie *ref;		/* reference Lab conversion */

	double lab0[3], lab1[3];	/* Conversion of target vs. Daylight */
} cfinds;

/* Optimize the daylight color temperature and level to minimizes the Delta E */
static double cfindfunc(void *adata, double pv[]) {
	cfinds *b = (cfinds *)adata;
	int i;
	double rv = 0.0;

	/* Compute daylight with the given color temperature */
	standardIlluminant( &b->dxx, icxIT_Dtemp, pv[0]);
	b->dxx.norm = 1.0;

	/* Adjust the level of daylight spectra */
	for (i = 0; i < b->dxx.spec_n; i++) {
		b->dxx.spec[i] *= pv[1];
	}

	/* Compute Lab of target */
	b->ref->convert(b->ref, b->lab0, &b->ill);

	/* Compute Lab of Dxx */
	b->ref->convert(b->ref, b->lab1, &b->dxx);

	/* Weighted Delta E (low weight on a* error) */
	rv = icmLabDEsq(b->lab0, b->lab1);

//printf("~1 rev = %f (%f %f %f - %f %f %f) from %f %f\n",rv, b->lab0[0], b->lab0[1], b->lab0[2], b->lab1[0], b->lab1[1], b->lab1[2], pv[0], pv[1]);
	return rv;
}

#endif /* SHOWDXX */

/* ============================================================== */
void
usage(int debug) {
	icoms *icom;
	fprintf(stderr,"Measure an illuminant, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	if (setup_spyd2() == 2)
		fprintf(stderr,"WARNING: This file contains a proprietary firmware image, and may not be freely distributed !\n");
	fprintf(stderr,"usage: illumread [-options] output.sp\n");
	fprintf(stderr," -v                   Verbose mode\n");
	fprintf(stderr," -S                   Plot spectrum for each reading\n");
	fprintf(stderr," -c listno            Choose instrument from the following list (default 1)\n");
	if ((icom = new_icoms()) != NULL) {
		icompath **paths;
		if (debug)
			icom->debug = debug;
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
		} else
			fprintf(stderr,"    ** No ports found **\n");
		icom->del(icom);
	}
	fprintf(stderr," -N                   Disable auto calibration of instrument\n");
	fprintf(stderr," -H                   Use high resolution spectrum mode (if available)\n");
	fprintf(stderr," -W n|h|x             Override serial port flow control: n = none, h = HW, x = Xon/Xoff\n");
	fprintf(stderr," -D [level]           Print debug diagnostics to stderr\n");
	fprintf(stderr," illuminant.sp        File to save measurement to\n");
	exit(1);
	}

int main(int argc, char *argv[])
{
	int i,j;
	int fa, nfa, mfa;				/* current argument we're looking at */
	int verb = 0;
	int debug = 0;
	int nocal = 0;					/* Disable auto calibration */
	int pspec = 0;                  /* 2 = Plot out the spectrum for each reading */
	int highres = 0;				/* Use high res mode if available */
	char outname[MAXNAMEL+1] = "\000";  /* Spectral output file name */
#ifdef DEBUG
	char tname[MAXNAMEL+11] = "\000", *tnp;
	int rd_i = 0, rd_r = 0, rd_p = 0;
#endif

	int comno = 1;					/* Specific port suggested by user */
	inst *it = NULL;				/* Instrument object, NULL if none */
	inst_mode mode = 0;				/* Instrument reading mode */
	inst_opt_mode trigmode = inst_opt_unknown;	/* Chosen trigger mode */
	instType itype = instUnknown;	/* No default target instrument */
	inst_capability  cap = inst_unknown;	/* Instrument capabilities */
	inst2_capability cap2 = inst2_unknown;	/* Instrument capabilities 2 */
	baud_rate br = baud_38400;		/* Target baud rate */
	flow_control fc = fc_nc;		/* Default flow control */

	inst_code rv;
	int uswitch = 0;				/* Instrument switch is enabled */
	xspect i_sp;					/* Illuminant emsission spectrum */
	xspect r_sp;					/* Illuminant reflected from paper emission spectrum */
	xspect p_sp;					/* Paper reflectance spectrum */
	xspect insp;					/* Instrument illuminant (for FWA) */
	xspect ill;						/* Estimated illuminant */
	xspect aill;					/* Accumulated result */
	int    nacc = 0;				/* Number accumulated */
	ipatch val;						/* Value read */

	set_exe_path(argv[0]);			/* Set global exe_path and error_program */
	check_if_not_interactive();
	setup_spyd2();				/* Load firware if available */

	i_sp.spec_n = 0;
	r_sp.spec_n = 0;
	p_sp.spec_n = 0;
	insp.spec_n = 0;
	ill.spec_n = 0;
	aill.spec_n = 0;

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
				usage(debug);

			} else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;

			} else if (argv[fa][1] == 'S') {
				pspec = 2;

			/* COM port  */
			} else if (argv[fa][1] == 'c') {
				fa = nfa;
				if (na == NULL) usage(debug);
				comno = atoi(na);
				if (comno < 1 || comno > 40) usage(debug);

			/* No auto calibration */
			} else if (argv[fa][1] == 'N') {
				nocal = 1;

			/* High res mode */
			} else if (argv[fa][1] == 'H') {
				highres = 1;

			/* Serial port flow control */
			} else if (argv[fa][1] == 'W') {
				fa = nfa;
				if (na == NULL) usage(debug);
				if (na[0] == 'n' || na[0] == 'N')
					fc = fc_none;
				else if (na[0] == 'h' || na[0] == 'H')
					fc = fc_Hardware;
				else if (na[0] == 'x' || na[0] == 'X')
					fc = fc_XonXOff;
				else
					usage(debug);

			} else if (argv[fa][1] == 'D') {
				debug = 1;
				if (na != NULL && na[0] >= '0' && na[0] <= '9') {
					debug = atoi(na);
					fa = nfa;
				}
			} else 
				usage(debug);
		}
		else
			break;
	}

	/* Get the output spectrum file name argument */
	if (fa >= argc)
		usage(debug);

	strncpy(outname,argv[fa++],MAXNAMEL-1); outname[MAXNAMEL-1] = '\000';

#ifdef DEBUG
	strcpy(tname, outname);
	if ((tnp = strrchr(tname, '.')) == NULL)
		tnp = tname + strlen(tname);

	/* Special debug */
	strcpy(tnp, "_i.sp");
	if (read_xspect(&i_sp, tname) == 0) {
		rd_i = 1;
		printf("(Found '%s' file and loaded it)\n",tname);
	}
	strcpy(tnp, "_r.sp");
	if (read_xspect(&r_sp, tname) == 0) {
		rd_r = 1;
		printf("(Found '%s' file and loaded it)\n",tname);
	}
	strcpy(tnp, "_p.sp");
	if (read_xspect(&p_sp, tname) == 0) {
		rd_p = 1;
		/* Should read instrument type from debug_p.sp !! */
		if (inst_illuminant(&insp, instI1Pro) != 0)		/* Hack !! */
			error ("Instrument doesn't have an FWA illuminent");
		printf("(Found '%s' file and loaded it)\n",tname);
	}
#endif /* DEBUG */

	/* Until the measurements are done, or we give up */
	for (;;) {
		int c;

		/* Print the menue of adjustments */
		printf("\nPress 1 .. 7\n");
		printf("1) Measure direct illuminant%s\n",i_sp.spec_n != 0 ? " (measured)":""); 
		printf("2) Measure illuminant reflected from paper%s\n", r_sp.spec_n != 0 ? " (measured)":""); 
		printf("3) Measure paper%s\n", p_sp.spec_n != 0 ? " (measured)":""); 
		if (it == NULL) {
			icoms *icom;
			printf("4) Select another instrument, Currently %d (", comno);
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
		} else
			printf("4) Select another instrument, Currently %s\n", inst_name(itype));
		printf("5) Compute illuminant spectrum, average result with %d previous readings & save it\n",nacc);
		printf("6) Compute illuminant spectrum from this reading & save result\n");
		printf("7) Exit\n");

		empty_con_chars();
		c = next_con_char();
		printf("'%c'\n",c);

		/* Deal with doing a measurement */
		if (c == '1' || c == '2' || c == '3') {
			int ch;

			if (it == NULL) {
				/* Open the instrument */
				if ((it = new_inst(comno, instUnknown, debug, verb)) == NULL) {
					printf("!!! Unknown, inappropriate or no instrument detected !!!\n");
					continue;
				}

				if (verb)
					printf("Connecting to the instrument ..\n");

#ifdef DEBUG
				printf("About to init the comms\n");
#endif

				/* Establish communications */
				if ((rv = it->init_coms(it, comno, br, fc, 15.0)) != inst_ok) {
					printf("!!! Failed to initialise communications with instrument\n"
					       "    or wrong instrument or bad configuration !!!\n"
					       "   ('%s' + '%s')\n", it->inst_interp_error(it, rv), it->interp_error(it, rv));
					it->del(it);
					it = NULL;
					itype = instUnknown;
					continue;
				}

#ifdef DEBUG
				printf("Established comms & initing the instrument\n");
#endif

				/* Initialise the instrument */
				if ((rv = it->init_inst(it)) != inst_ok) {
					printf("!!! Instrument initialisation failed with '%s' (%s) !!!\n",
					      it->inst_interp_error(it, rv), it->interp_error(it, rv));
					it->del(it);
					it = NULL;
					itype = instUnknown;
					continue;
				}
				
				itype = it->get_itype(it);		/* get actual type of instrument */

				/* Check the instrument has the necessary capabilities */
				cap = it->capabilities(it);
				cap2 = it->capabilities2(it);

				/* Need spectral */
				if ((cap & inst_spectral) == 0) {
					printf("Instrument lacks spectral measurement capability");
				}

				/* Disable autocalibration of machine if selected */
				if (nocal != 0){
					if ((rv = it->set_opt_mode(it,inst_opt_noautocalib)) != inst_ok) {
						printf("Setting no-autocalibrate failed failed with '%s' (%s) !!!\n",
					       it->inst_interp_error(it, rv), it->interp_error(it, rv));
						printf("Disable auto-calibrate not supported\n");
					}
				}
				if (highres) {
					if (cap & inst_highres) {
						inst_code ev;
						if ((ev = it->set_opt_mode(it, inst_opt_highres)) != inst_ok) {
							printf("!!! Setting high res mode failed with error :'%s' (%s) !!!\n",
					       	       it->inst_interp_error(it, ev), it->interp_error(it, ev));
						}
					} else if (verb) {
						printf("!!! High resolution ignored - instrument doesn't support it !!!\n");
					}
				}
			}

			/* Check the instrument has the necessary capabilities for this measurement */
			cap = it->capabilities(it);
			cap2 = it->capabilities2(it);

			if (c == '1') {
				if ((cap & inst_emis_ambient) != 0) {
					mode = inst_mode_emis_ambient;
				} else if ((cap & inst_emis_spot) != 0) {
					mode = inst_mode_emis_spot;
				} else {
					printf("!!! Instrument doesn't have ambient or emissive capability !!!\n");
					continue;
				}
			}
			if (c == '2') {
				if ((cap & inst_emis_tele) != 0) {
					mode = inst_mode_emis_tele;
				} else if ((cap & inst_emis_spot) != 0) {
					mode = inst_mode_emis_spot;
				} else {
					printf("!!! Instrument doesn't have projector or emissive capability !!!\n");
					continue;
				}
			}
			if (c == '3') {
				inst_opt_filter filt;

				if ((cap & inst_ref_spot) != 0) {
					mode = inst_mode_ref_spot;
				} else {
					printf("!!! Instrument lacks reflective spot measurement capability !!!\n");
					continue;
				}

				if ((rv = it->get_status(it, inst_stat_get_filter, &filt)) == inst_ok) {
					if (filt & inst_opt_filter_UVCut) {
						printf("!!! Instrument has UV filter - can't measure FWA !!!\n");
						continue;
					}
				}
			}
			mode |= inst_mode_spectral;

			if ((rv = it->set_mode(it, mode)) != inst_ok) {
				printf("!!! Setting instrument mode failed with error :'%s' (%s) !!!\n",
			     	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
				continue;
			}
			cap = it->capabilities(it);
			cap2 = it->capabilities2(it);

			/* If it batter powered, show the status of the battery */
			if ((cap2 & inst2_has_battery)) {
				double batstat = 0.0;
				if ((rv = it->get_status(it, inst_stat_battery, &batstat)) == inst_ok)
					printf("The battery charged level is %.0f%%\n",batstat * 100.0);
			}

			/* If it's an instrument that need positioning do trigger via keyboard */
			/* in illumread, else enable switch or keyboard trigger if possible. */
			if ((cap2 & inst2_xy_locate) && (cap2 & inst2_xy_position)) {
				trigmode = inst_opt_trig_prog;

			} else if (cap2 & inst2_keyb_switch_trig) {
				trigmode = inst_opt_trig_keyb_switch;
				uswitch = 1;

			/* Or go for keyboard trigger */
			} else if (cap2 & inst2_keyb_trig) {
				trigmode = inst_opt_trig_keyb;

			/* Or something is wrong with instrument capabilities */
			} else {
				printf("!!! No reasonable trigger mode avilable for this instrument !!!\n");
				continue;
			}
			if ((rv = it->set_opt_mode(it, trigmode)) != inst_ok) {
				printf("!!! Setting trigger mode failed with error :'%s' (%s) !!!\n",
		       	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
				continue;
			}

			/* Prompt on trigger */
			if ((rv = it->set_opt_mode(it, inst_opt_trig_return)) != inst_ok) {
				printf("!!! Setting trigger mode failed with error :'%s' (%s) !!!\n",
		       	       it->inst_interp_error(it, rv), it->interp_error(it, rv));
				continue;
			}

			/* Setup the keyboard trigger to return our commands */
			it->icom->reset_uih(it->icom);
			it->icom->set_uih(it->icom, 0x0, 0xff, ICOM_TRIG);
			it->icom->set_uih(it->icom, 'q', 'q', ICOM_USER);
			it->icom->set_uih(it->icom, 'Q', 'Q', ICOM_USER);
			it->icom->set_uih(it->icom, 0x03, 0x03, ICOM_USER);		/* ^c */
			it->icom->set_uih(it->icom, 0x1b, 0x1b, ICOM_USER);		/* Esc */

			/* Hold table */
			if (cap2 & inst2_xy_holdrel) {
				for (;;) {		/* retry loop */
					if ((rv = it->xy_sheet_hold(it)) == inst_ok)
						break;

					if (ierror(it, rv)) {
						printf("!!! Setting paper hold failed with error :'%s' (%s) !!!\n",
						       it->inst_interp_error(it, rv), it->interp_error(it, rv));
						it->xy_clear(it);
						continue;
					}
				}
			}

			/* Do any needed calibration before the user places the instrument on a desired spot */
			if ((it->needs_calibration(it) & inst_calt_needs_cal_mask) != 0) {
				double lx, ly;
				inst_code ev;

				printf("\nInstrument needs a calibration before continuing\n");

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
						printf("!!! Setting calibration location failed with error :'%s' (%s) !!!\n",
							it->inst_interp_error(it, rv), it->interp_error(it, rv));
						continue;
					}
				}

				ev = inst_handle_calibrate(it, inst_calt_all, inst_calc_none, NULL, NULL);
				if (ev != inst_ok) {	/* Abort or fatal error */
					printf("!!! Calibration failed with error :'%s' (%s) !!!\n",
						it->inst_interp_error(it, rv), it->interp_error(it, rv));
					continue;
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
						printf("!!! Restoring location failed with error :'%s' (%s) !!!\n",
							it->inst_interp_error(it, rv), it->interp_error(it, rv));
						continue;
					}
				}
			}

			/* Now do the measurement: */

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
				if (rv != inst_ok)
					break;			/* Abort */

				if (c != 3) {
					printf("!!! Unexpected: XY instrument used for emissive measurements !!!\n");
					continue;
				}

				printf("Using the XY table controls, position the instrument sight\n");
				printf("so as to read the paper,\n");

			/* Purely manual instrument */
			} else {

				if (c == '1') {
					if ((cap & inst_emis_ambient) != 0) {
						printf("\n(If applicable) set instrument to ambient measurenent mode, or place\n");
						printf("ambient adapter on it, and position it so as to measure the illuminant directly.\n");
					} else {
						printf("\n(If applicable) set instrument to emissive measurenent mode,\n");
						printf("and position it so as to measure the illuminant directly.\n");
					}
				} else if (c == '2') {
					if ((cap & inst_emis_proj) != 0) {
						printf("\n(If applicable) set instrument to projector measurenent mode,\n");
						printf("position it so as to measure the illuminant reflected from the paper.\n");
					} else {
						printf("\n(If applicable) set instrument to emsissive measurenent mode,\n");
						printf("position it so as to measure the illuminant reflected from the paper.\n");
					}
				} else if (c == '3') {
					printf("\n(If applicable) set instrument to reflective measurenent mode,\n");
					printf("position it so as to measure the paper.");
				} else
					error("Unexpected choice");
			}
			if (uswitch)
				printf("Hit ESC or Q to abort, or instrument switch or any other key to take a reading: ");
			else
				printf("Hit ESC or Q to abort, any any other key to take a reading: ");
			fflush(stdout);

			if ((cap2 & inst2_xy_locate) && (cap2 & inst2_xy_position)) {
				if ((rv = it->poll_user(it, 1)) == inst_user_trig)  {
					double lx, ly;
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
						if (rv != inst_ok)
							break;			/* Abort */
			
						for (;;) {		/* retry loop */
							if ((rv = it->xy_locate_end(it)) == inst_ok)
								break;
							if (ierror(it, rv) == 0)	/* Ignore */
								continue;
							break;						/* Abort */
						}
						if (rv != inst_ok)
							break;			/* Abort */
			
						for (;;) {		/* retry loop */
							if ((rv = it->xy_position(it, 1, lx, ly)) == inst_ok)
								break;
							if (ierror(it, rv) == 0)	/* Ignore */
								continue;
							break;						/* Abort */
						}
						if (rv != inst_ok)
							break;			/* Abort */
					}
					rv = it->read_sample(it, "SPOT", &val);

					/* Restore the location the instrument to have the location */
					/* sight over the selected patch. */
					for (;;) {		/* retry loop */
						if ((ev = it->xy_position(it, 0, lx, ly)) == inst_ok)
							break;
						if (ierror(it, ev) == 0)	/* Ignore */
							continue;
						break;						/* Abort */
					}
					if (ev != inst_ok)
						break;			/* Abort */
				}
			} else {
				rv = it->read_sample(it, "SPOT", &val);
			}

			/* Release paper */
			if (cap2 & inst2_xy_holdrel) {
				it->xy_clear(it);
			}

#ifdef DEBUG
			printf("read_sample returned '%s' (%s)\n",
		       it->inst_interp_error(it, rv), it->interp_error(it, rv));
#endif /* DEBUG */

			/* Deal with a trigger or command */
			if ((rv & inst_mask) == inst_user_trig
		        || (rv & inst_mask) == inst_user_cmnd) {
			    ch = it->icom->get_uih_char(it->icom);

			/* Deal with an error or abort */
			} else if ((rv & inst_mask) == inst_user_abort) {
				printf("\nIlluminant measure aborted at user request!\n");
				continue;

			/* Deal with a needs calibration */
			} else if ((rv & inst_mask) == inst_needs_cal) {
				inst_code ev;
				printf("\nIlluminant measure failed because instruments needs calibration.\n");
				ev = inst_handle_calibrate(it, inst_calt_all, inst_calc_none, NULL, NULL);
				continue;

			/* Deal with a bad sensor position */
			} else if ((rv & inst_mask) == inst_wrong_sensor_pos) {
				printf("\nIlluminant measure failed due to the sensor being in the wrong position\n(%s)\n",it->interp_error(it, rv));
				continue;

			/* Deal with a misread */
			} else if ((rv & inst_mask) == inst_misread) {
				printf("\nIlluminant measure failed due to misread (%s)\n",it->interp_error(it, rv));
				continue;

			/* Deal with a communications error */
			} else if ((rv & inst_mask) == inst_coms_fail) {
				empty_con_chars();
				printf("\nIlluminant measure failed due to communication problem.\n");
				printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
				continue;

			/* Some other fatal error */
			} else if (rv != inst_ok) {
				printf("\nGot fatal error '%s' (%s)\n",
					it->inst_interp_error(it, rv), it->interp_error(it, rv));
				continue;
			}

			if (c == '1') {
				i_sp = val.sp;
#ifdef DEBUG
				if (rd_i == 0) {
					strcpy(tnp, "_i.sp");
					write_xspect(tname,&i_sp);
				}
#endif
			} else if (c == '2') {
				r_sp = val.sp;
#ifdef DEBUG
				if (rd_r == 0) {
					strcpy(tnp, "_r.sp");
					write_xspect(tname,&r_sp);
				}
#endif
			} else if (c == '3') {
				p_sp = val.sp;

				/* Get the illuminant spectrum too */
				if (inst_illuminant(&insp, itype) != 0)
					error ("Instrument doesn't have an FWA illuminent");

#ifdef DEBUG
				if (rd_p == 0) {
					/* Should save instrument type/instrument illuminant spectrum !!! */
					strcpy(tnp, "_p.sp");
					write_xspect(tname,&p_sp);
				}
#endif
			}

			if (pspec) {
				double xx[XSPECT_MAX_BANDS];
				double y1[XSPECT_MAX_BANDS];
				double xmin, xmax, ymin, ymax;
				int nn;

				if (val.sp.spec_n <= 0)
					error("Instrument didn't return spectral data");

				printf("Spectrum from %f to %f nm in %d steps\n",
			                val.sp.spec_wl_short, val.sp.spec_wl_long, val.sp.spec_n);

				if (val.sp.spec_n > XSPECT_MAX_BANDS)
					error("Got > %d spectral values (%d)",XSPECT_MAX_BANDS,val.sp.spec_n);

				for (j = 0; j < val.sp.spec_n; j++) {
					GCC_BUGFIX(j)
					xx[j] = val.sp.spec_wl_short
					      + j * (val.sp.spec_wl_long - val.sp.spec_wl_short)/(val.sp.spec_n-1);

					y1[j] = value_xspect(&val.sp, xx[j]);
				}
				
				xmax = val.sp.spec_wl_long;
				xmin = val.sp.spec_wl_short;
				ymin = ymax = 0.0;	/* let it scale */
				do_plot_x(xx, y1, NULL, NULL, val.sp.spec_n, 1,
				          xmin, xmax, ymin, ymax, 2.0);
			}

			continue;
		}	/* End of take a measurement */

		if (c == '4') {
			icoms *icom;
			if (it != NULL)
				it->del(it);
			it = NULL;
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
		if (c == '5' || c == '6') {		/* Compute result */
			xspect cpisp;		/* FWA corrected calculated initial paper reflectance */
			double gain;
			bfinds bf;	/* Optimization context */
			double xyz0[3], xyz1[3];
			double tt[2], sr[2];
			double rv;

			if (i_sp.spec_n == 0) {
				printf("Need to measure the direct illuminant\n");
				continue;
			}
			if (r_sp.spec_n == 0) {
				printf("Need to measure the illuminant reflected off paper\n");
				continue;
			}
			if (p_sp.spec_n == 0) {
				printf("Need to measure the paper\n");
				continue;
			}

			/* Normalize direct illumination */
			for (gain = 0.0, i = 0; i < i_sp.spec_n; i++)
				gain += i_sp.spec[i];
			gain /= i_sp.spec_n;
			for (i = 0; i < i_sp.spec_n; i++)
				i_sp.spec[i] /= gain;

			/* Normalize indirect illumination */
			for (gain = 0.0, i = 0; i < r_sp.spec_n; i++)
				gain += r_sp.spec[i];
			gain /= r_sp.spec_n;
			for (i = 0; i < r_sp.spec_n; i++)
				r_sp.spec[i] /= gain;

			/* Normalize paper reflectance to 1.0 */
			xspect_denorm(&p_sp);

			bf.i_sp = &i_sp;
			bf.r_sp = &r_sp;
			bf.p_sp = &p_sp;

			if ((bf.pap = new_xsp2cie(icxIT_custom, &i_sp, icxOT_CIE_1931_2, NULL, icSigLabData)) == NULL)
				error("new_xsp2cie pap failed");

			if (bf.pap->set_fwa(bf.pap, &insp, &p_sp) != 0) 
				error ("Setting FWA compensation failed");

			/* Setup the equal energy to Lab conversion */
			if ((bf.ref = new_xsp2cie(icxIT_E, NULL, icxOT_CIE_1931_2, NULL, icSigLabData)) == NULL)
				error("new_xsp2cie ref failed");

			/* Estimate an initial gain match */
			tt[0] = 0.0;
			tt[1] = 1.0;
			bfindfunc((void *)&bf, tt);
			icmLab2XYZ(&icmD50, xyz0, bf.lab0);
			icmLab2XYZ(&icmD50, xyz1, bf.lab1);

			gain = xyz0[1] / xyz1[1];
//printf("~1 Target XYZ %f %f %f, is %f %f %f, gain needed = %f\n",xyz0[0],xyz0[1],xyz0[2],xyz1[0],xyz1[1],xyz1[2],gain);

			for (i = 0; i < r_sp.spec_n; i++)
				r_sp.spec[i] *= gain;

			/* Check initial gain match is OK */
			bfindfunc((void *)&bf, tt);
			cpisp = bf.cpsp;			/* Copy initial match */
			icmLab2XYZ(&icmD50, xyz0, bf.lab0);
			icmLab2XYZ(&icmD50, xyz1, bf.lab1);
#ifdef NEVER
			printf("~1 Target XYZ %f %f %f, now %f %f %f\n",xyz0[0],xyz0[1],xyz0[2],xyz1[0],xyz1[1],xyz1[2]);
#endif

			tt[0] = 0.1, tt[1] = 1.0;	/* Search parameter starting values */
			sr[0] = sr[1] = 0.1;		/* Search parameter search radiuses */

			if (powell(&rv, 2, tt, sr, 0.0001, 1000,
			           bfindfunc, (void *)&bf, NULL, NULL) != 0) {
				printf("Optimization search failed\n");
				continue;
			}
			printf("(Best match DE %f, UV content = %f (gain match %f))\n", rv, tt[0], tt[1]);

			/* Compute the result */
			bfindfunc((void *)&bf, tt);
			ill = bf.ill;

			if (c == '5' && nacc > 0) {
				for (i = 0; i < ill.spec_n; i++)
					aill.spec[i] += ill.spec[i];
				nacc++;
			} else {
				aill = ill;
				nacc = 1;
			}

			/* Save the result */
			if (aill.spec_n == 0) {
				printf("Nothing to save!\n");
	 		} else {
				for (i = 0; i < ill.spec_n; i++)
					ill.spec[i] = aill.spec[i]/nacc;
				
				if(write_xspect(outname, &ill))
					printf("\nWriting file '%s' failed\n",outname);
				else
					printf("\nWriting file '%s' succeeded\n",outname);
			}

			/* Plot the result */
			if (pspec) {
				double xx[XSPECT_MAX_BANDS];
				double y1[XSPECT_MAX_BANDS];
				double y2[XSPECT_MAX_BANDS];
				double y3[XSPECT_MAX_BANDS];
				double xmin, xmax, ymin, ymax;
				int nn;

#ifdef SHOWDXX
				cfinds cf;	/* Optimization context */
				double tt[2], sr[2];
				double rv;
				xspect cpdsp;		/* FWA corrected calculated daylight paper reflectance */
				
				/* Setup the referencec comversion */
				if ((cf.ref = new_xsp2cie(icxIT_E, NULL, icxOT_CIE_1931_2, NULL, icSigLabData)) == NULL)
					error("new_xsp2cie ref failed");

				cf.ill = bf.ill; 

				/* Set starting values */
				tt[0] = 5000.0;		/* degrees */
				tt[1] = 1.0;
				cfindfunc((void *)&cf, tt);
				icmLab2XYZ(&icmD50, xyz0, cf.lab0);
				icmLab2XYZ(&icmD50, xyz1, cf.lab1);

				tt[1] = xyz0[1] / xyz1[1];

				sr[0] = 10.0;
				sr[1] = 0.1;		/* Search parameter search radiuses */

				if (powell(&rv, 2, tt, sr, 0.0001, 1000,
				           cfindfunc, (void *)&cf, NULL, NULL) != 0) {
					error("Optimization search failed\n");
				}
				printf("(Best daylight match DE %f, temp = %f (gain match %f))\n", rv, tt[0], tt[1]);

				/* Compute the result */
				cfindfunc((void *)&cf, tt);

				printf("Illuminant: Black - Measured, Red - with estimated UV, Green - daylight\n");

				if (bf.ill.spec_n > XSPECT_MAX_BANDS)
					error("Got > %d spectral values (%d)",XSPECT_MAX_BANDS,bf.ill.spec_n);

				for (j = 0; j < bf.ill.spec_n; j++) {
					GCC_BUGFIX(j)
					xx[j] = XSPECT_XWL(&bf.ill, j);
					y1[j] = value_xspect(bf.i_sp, xx[j]);		/* Measured (black) */
					y2[j] = value_xspect(&bf.ill, xx[j]);		/* Computed (red)*/
					y3[j] = value_xspect(&cf.dxx, xx[j]);		/* Daylight match (green)*/
				}
				
				xmax = bf.ill.spec_wl_long;
				xmin = bf.ill.spec_wl_short;
				ymin = ymax = 0.0;	/* let it scale */
				do_plot_x(xx, y1, y2, y3, bf.ill.spec_n, 1,
				          xmin, xmax, ymin, ymax, 2.0);

				/* Update the conversion to the matched Dayligh */
				if (bf.pap->update_fwa_custillum(bf.pap, &cf.dxx) != 0) 
					error ("Updating FWA compensation to daylight failed");

				/* Apply FWA compensation to the paper reflectance */
				bf.pap->sconvert(bf.pap, &cpdsp, NULL, bf.p_sp);

				printf("Paper Reflectance: Black - Measured, Red - Initial FWA model, Green - FWA Final FWA model\n");
//				printf("Paper Reflectance: Black - Measured, Red - FWA Modelled, Green - Daylight modelled\n");

				for (j = 0; j < bf.cpsp.spec_n; j++) {
					GCC_BUGFIX(j)
					xx[j] = XSPECT_XWL(&bf.cpsp, j);
					y1[j] = value_xspect(&bf.srop, xx[j]);		/* Measured reflectance (black) */
					y2[j] = value_xspect(&cpisp, xx[j]);		/* Computed initial reflectance (red) */
					y3[j] = value_xspect(&bf.cpsp, xx[j]);		/* Computed final reflectance (green) */
//					y3[j] = value_xspect(&cpdsp, xx[j]);		/* Computed daylight reflectance (green) */
				}
				
				xmax = bf.cpsp.spec_wl_long;
				xmin = bf.cpsp.spec_wl_short;
				ymin = ymax = 0.0;	/* let it scale */
				do_plot_x(xx, y1, y2, y3, bf.cpsp.spec_n, 1,
				          xmin, xmax, ymin, ymax, 2.0);

#else
				printf("Illuminant: Black - Measured, Red - with estimated UV\n");

				if (bf.ill.spec_n > XSPECT_MAX_BANDS)
					error("Got > %d spectral values (%d)",XSPECT_MAX_BANDS,bf.ill.spec_n);

				for (j = 0; j < bf.ill.spec_n; j++) {
					GCC_BUGFIX(j)
					xx[j] = XSPECT_XWL(&bf.ill, j);
					y1[j] = value_xspect(bf.i_sp, xx[j]);		/* Measured (black) */
					y2[j] = value_xspect(&bf.ill, xx[j]);		/* Computed (red)*/
				}
				
				xmax = bf.ill.spec_wl_long;
				xmin = bf.ill.spec_wl_short;
				ymin = ymax = 0.0;	/* let it scale */
				do_plot_x(xx, y1, y2, NULL, bf.ill.spec_n, 1,
				          xmin, xmax, ymin, ymax, 2.0);

				printf("Paper Reflectance: Black - Measured, Red - Initial FWA model, Green - FWA Final FWA model\n");

				for (j = 0; j < bf.cpsp.spec_n; j++) {
					GCC_BUGFIX(j)
					xx[j] = XSPECT_XWL(&bf.cpsp, j);
					y1[j] = value_xspect(&bf.srop, xx[j]);		/* Measured reflectance (black) */
					y2[j] = value_xspect(&cpisp, xx[j]);		/* Computed initial reflectance (red) */
					y3[j] = value_xspect(&bf.cpsp, xx[j]);		/* Computed final reflectance (green) */
				}
				
				xmax = bf.cpsp.spec_wl_long;
				xmin = bf.cpsp.spec_wl_short;
				ymin = ymax = 0.0;	/* let it scale */
				do_plot_x(xx, y1, y2, y3, bf.cpsp.spec_n, 1,
				          xmin, xmax, ymin, ymax, 2.0);

#endif
				printf("%s illuminant with UV:\n",c == '5' ? "Averaged" : "Computed");

				for (j = 0; j < ill.spec_n; j++) {
					GCC_BUGFIX(j)
					xx[j] = XSPECT_XWL(&ill, j);
					y1[j] = value_xspect(&ill, xx[j]);
				}
				
				xmax = ill.spec_wl_long;
				xmin = ill.spec_wl_short;
				ymin = ymax = 0.0;	/* let it scale */
				do_plot_x(xx, y1, NULL, NULL, ill.spec_n, 1,
				          xmin, xmax, ymin, ymax, 2.0);
			}

			/* Make sure that the illuminant is re-measured for another reading */
			i_sp.spec_n = 0;
			r_sp.spec_n = 0;

			continue;
		}

		if (c == '7' || c == 0x3) {		/* Exit */
			break;
		}

	}	/* Next command */

#ifdef DEBUG
	printf("About to exit\n");
#endif

	/* Free instrument */
	if (it != NULL)
		it->del(it);

	return 0;
}




