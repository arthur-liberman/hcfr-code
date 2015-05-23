

/* 
 * Argyll Color Correction System
 * Common display patch reading support.
 *
 * Author: Graeme W. Gill
 * Date:   2/11/2005
 *
 * Copyright 1998 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* 
	TTBD:

		If verb, report white/black drift

		Should add option to ask for a black cal every N readings, for spectro's
 */

#ifdef __MINGW32__
# define WINVER 0x0500
#endif
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include "aconfig.h"
#include "numlib.h"
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"
#include "inst.h"
#include "spyd2.h"
#include "dispwin.h"
#include "webwin.h"
#include "ccast.h"
#include "ccwin.h"
#include "icc.h"
#ifdef NT
# include "madvrwin.h"
#endif

#include "dispsup.h"
#include "instappsup.h"

#undef DEBUG

#undef SIMPLE_MODEL		/* Make fake device well behaved */
						/* else has offsets, quantization, noise etc. */

#define DRIFT_IPERIOD	40	/* Number of samples between drift interpolation measurements */
#define DRIFT_EPERIOD	20	/* Number of samples between drift extrapolation measurements */
#define DRIFT_MAXSECS	60	/* Number of seconds to time out previous drift value */
//#define DRIFT_IPERIOD	6	/* Test values */
//#define DRIFT_EPERIOD	3

#ifdef SIMPLE_MODEL
# undef FAKE_NOISE			/* Add noise to _fake_ devices XYZ */
# undef FAKE_UNPREDIC		/* Initialise random unpredictably */
# undef FAKE_BITS 			/* Number of bits of significance of fake device */
#else
# define FAKE_NOISE 0.01		/* Add noise to _fake_ devices XYZ */
# define FAKE_UNPREDIC			/* Initialise random unpredictably */
# define FAKE_BITS 9			/* Number of bits of significance of fake device */
#endif


#if defined(DEBUG)

#define DBG(xxx) fprintf xxx ;
#define dbgo stderr
#else
#define DBG(xxx) 
#endif	/* DEBUG */

/* -------------------------------------------------------- */
/* A default callback that can be provided as an argument to */
/* inst_handle_calibrate() to handle the display part of a */
/* calibration callback. */
/* Call this again with calc = inst_calc_none to cleanup afterwards. */
inst_code setup_display_calibrate(
	inst *p,
	inst_cal_cond calc,		/* Current condition. inst_calc_none for not setup */
	disp_win_info *dwi		/* Information to be able to open a display test patch */
) {
	inst_code rv = inst_ok;//, ev;
//	dispwin *dw;	/* Display window to display test patches on, NULL if none. */
	a1logd(p->log,1,"setup_display_calibrate called with calc = 0x%x\n",calc);

	switch (calc) {
		case inst_calc_none:		/* Use this as a cleanup flag */
			if (dwi->dw == NULL && dwi->_dw != NULL) {
				dwi->_dw->del(dwi->_dw);
				dwi->_dw = NULL;
			}
			break;

		case inst_calc_emis_white:
		case inst_calc_emis_80pc:
		case inst_calc_emis_grey:
		case inst_calc_emis_grey_darker: 
		case inst_calc_emis_grey_ligher:
			if (dwi->dw == NULL) {
				if (dwi->webdisp != 0) {
					if ((dwi->_dw = new_webwin(dwi->webdisp, dwi->hpatsize, dwi->vpatsize,
						         dwi->ho, dwi->vo, 0, 0, NULL, NULL, dwi->out_tvenc, dwi->blackbg,
					             p->log->verb, p->log->debug)) == NULL) {
						a1logd(p->log,1,"inst_handle_calibrate failed to create test window 0x%x\n",inst_other_error);
						return inst_other_error; 
					}
				} else if (dwi->ccid != NULL) {
					if ((dwi->_dw = new_ccwin(dwi->ccid, dwi->hpatsize, dwi->vpatsize,
						         dwi->ho, dwi->vo, 0, 0, NULL, NULL, dwi->out_tvenc, dwi->blackbg,
					             p->log->verb, p->log->debug)) == NULL) {
						a1logd(p->log,1,"inst_handle_calibrate failed to create test window 0x%x\n",inst_other_error);
						return inst_other_error; 
					}
#ifdef NT
				} else if (dwi->madvrdisp != 0) {
					if ((dwi->_dw = new_madvrwin(dwi->hpatsize, dwi->vpatsize,
					             dwi->ho, dwi->vo, 0, 0, NULL, NULL, dwi->out_tvenc, dwi->blackbg,
					             p->log->verb, p->log->debug)) == NULL) {
						a1logd(p->log,1,"inst_handle_calibrate failed to create test window 0x%x\n",inst_other_error);
						return inst_other_error; 
					}
#endif /* NT */
				} else {
					if ((dwi->_dw = new_dispwin(dwi->disp, dwi->hpatsize, dwi->vpatsize,
					             dwi->ho, dwi->vo, 0, 0, NULL, NULL, dwi->out_tvenc, dwi->blackbg,
					             dwi->override, p->log->debug)) == NULL) {
						a1logd(p->log,1,"inst_handle_calibrate failed to create test window 0x%x\n",inst_other_error);
						return inst_other_error; 
					}
				}
				printf("Calibration: Place instrument on test window.\n");
				printf(" Hit any key to continue,\n"); 
				printf(" or hit Esc or Q to abort:"); 
			} else {
				dwi->_dw = dwi->dw;
			}

			/* Set display rise & fall time more optimally */
			{
				disptech dtech;
				disptech_info *tinfo;
				p->get_disptechi(p, &dtech, NULL, NULL);
				tinfo = disptech_get_id(dtech);
				dwi->dw->set_settling_delay(dwi->dw, tinfo->rise_time, tinfo->fall_time, -1.0);
			}

			if (calc == inst_calc_emis_white) {
				p->cal_gy_level = 1.0;
				dwi->_dw->set_color(dwi->_dw, 1.0, 1.0, 1.0);

			} else if (calc == inst_calc_emis_80pc) {
				p->cal_gy_level = 0.8;
				dwi->_dw->set_color(dwi->_dw, 0.8, 0.8, 0.8);

			} else  {
				if (calc == inst_calc_emis_grey) {
					p->cal_gy_level = 0.6;
					p->cal_gy_count = 0;
				} else if (calc == inst_calc_emis_grey_darker) {
					p->cal_gy_level *= 0.7;
					p->cal_gy_count++;
				} else if (calc == inst_calc_emis_grey_ligher) {
					p->cal_gy_level *= 1.4;
					if (p->cal_gy_level > 1.0)
						p->cal_gy_level = 1.0;
					p->cal_gy_count++;
				}
				if (p->cal_gy_count > 4) {
					printf("Cell ratio calibration failed - too many tries at setting grey level.\n");
					a1logd(p->log,1,"inst_handle_calibrate too many tries at setting grey level 0x%x\n",inst_internal_error);
					return inst_internal_error; 
				}
				dwi->_dw->set_color(dwi->_dw, p->cal_gy_level, p->cal_gy_level, p->cal_gy_level);
			}
			break;

		default:
			/* Something isn't being handled */
			a1logd(p->log,1,"inst_handle_calibrate unhandled calc case 0x%x, err 0x%x\n",calc,inst_internal_error);
			return inst_internal_error;
	}
	return inst_ok;
}

/* -------------------------------------------------------- */
/* User requested calibration of the display instrument */
/* Do all available non-deferrable calibrations */
/* Should add an argument to be able to select type of calibration, */
/* rather than guessing what the user wants ? */
int disprd_calibration(
icompath *ipath,		/* Instrument path to open, &icomFakeDevice == fake */
flow_control fc,		/* Serial flow control */
int dtype,				/* Display type selection character */
int sdtype,				/* Spectro dtype, use dtype if -1 */
int docbid,				/* NZ to only allow cbid dtypes */
int tele,				/* NZ for tele mode, falls back to spot mode */
int nadaptive,			/* NZ for non-adaptive mode */ 
int noinitcal,			/* NZ to disable initial instrument calibration */
disppath *disp,			/* display to calibrate. */
int webdisp,			/* If nz, port number for web display */
ccast_id *ccid,	 		/* non-NULL for ChromeCast */
#ifdef NT
int madvrdisp,		/* NZ for MadVR display */
#endif
int out_tvenc,			/* 1 = use RGB Video Level encoding */
int blackbg,			/* NZ if whole screen should be filled with black */
int override,			/* Override_redirect on X11 */
double hpatsize,		/* Size of dispwin */
double vpatsize,
double ho, double vo,	/* Position of dispwin */
a1log *log				/* Verb, debug & error log */
) {
	instType itype = instUnknown;
	inst *p = NULL;
//	int c;
	inst_code rv;
	baud_rate br = baud_19200;
	disp_win_info dwi;
	inst_cal_type calt = inst_calt_needed;
	inst_mode  cap;
	inst2_capability cap2;
	inst3_capability cap3;
	inst_mode mode = 0;

	memset((void *)&dwi, 0, sizeof(disp_win_info));
	dwi.webdisp = webdisp; 
	dwi.ccid = ccid; 
#ifdef NT
	dwi.madvrdisp = madvrdisp; 
#endif
	dwi.disp = disp; 
	dwi.out_tvenc = out_tvenc;
	dwi.blackbg = blackbg;
	dwi.override = override;
	dwi.hpatsize = hpatsize;
	dwi.vpatsize = vpatsize;
	dwi.ho = ho;
	dwi.vo = vo;

	a1logv(log, 1, "Setting up the instrument\n");

	if ((p = new_inst(ipath, 0, log, DUIH_FUNC_AND_CONTEXT)) == NULL) {
		a1logd(log, 1, "new_inst failed\n");
		return -1;
	}

	p->log = new_a1log_d(log);

	/* Establish communications */
	if ((rv = p->init_coms(p, br, fc, 15.0)) != inst_ok) {
		a1logd(p->log, 1, "init_coms returned '%s' (%s)\n",
		       p->inst_interp_error(p, rv), p->interp_error(p, rv));
		p->del(p);
		return -1;
	}

	/* Initialise the instrument */
	if ((rv = p->init_inst(p)) != inst_ok) {
		a1logd(log,1,"init_inst returned '%s' (%s)\n",
		       p->inst_interp_error(p, rv), p->interp_error(p, rv));
		p->del(p);
		return -1;
	}

// ~~~~9999 should we call config_inst_displ(p) instead of badly duplicating
//  the instrument setup here ???

	itype = p->get_itype(p);			/* Actual type */
	p->capabilities(p, &cap, &cap2, &cap3);

	if (tele && !IMODETST(cap, inst_mode_emis_tele)) {
		printf("Want telephoto measurement capability but instrument doesn't support it\n");
		printf("so falling back to emissive spot mode.\n");
		tele = 0;
	}
	
	if (!tele && !IMODETST(cap, inst_mode_emis_spot)) {
		printf("Want emissive spot measurement capability but instrument doesn't support it\n");
		printf("so switching to telephoto spot mode.\n");
		tele = 1;
	}
	
	/* Set to emission mode to read a display */
	if (tele)
		mode = inst_mode_emis_tele;
	else
		mode = inst_mode_emis_spot;
	if (nadaptive)
		mode |= inst_mode_emis_nonadaptive;
	
//	if (p->highres)
//		mode |= inst_mode_highres;

	/* (We're assuming spectral doesn't affect calibration ?) */

	if ((rv = p->set_mode(p, mode)) != inst_ok) {
		a1logd(log,1,"Set_mode failed with '%s' (%s)\n",
		       p->inst_interp_error(p, rv), p->interp_error(p, rv));
		return -1;
	}
	p->capabilities(p, &cap, &cap2, &cap3);

	/* If this is a spectral instrument, and a different */
	/* spectral inst. dtype is supplied, then use it */
	if (IMODETST(cap, inst_mode_spectral) && sdtype >= 0)
		dtype = sdtype;

	/* Set the display type */
	if (dtype != 0) {		/* Given selection character */
		if (cap2 & inst2_disptype) {
			int ix;
			if ((ix = inst_get_disptype_index(p, dtype, docbid)) < 0) {
				a1logd(log,1,"Display type selection '%c' is not valid for instrument\n",dtype);
				p->del(p);
				return -1;
			}
		
			if ((rv = p->set_disptype(p, ix)) != inst_ok) {
				a1logd(log,1,"Setting display type failed failed with '%s' (%s)\n",
				       p->inst_interp_error(p, rv), p->interp_error(p, rv));
				p->del(p);
				return -1;
			}
		} else
			printf("Display type ignored - instrument doesn't support display type selection\n");
	}

	/* Disable initial calibration of machine if selected */
	if (noinitcal != 0) {
		if ((rv = p->get_set_opt(p,inst_opt_noinitcalib, 0)) != inst_ok) {
			a1logd(log,1,"Setting no-initail calibrate failed with '%s' (%s)\n",
			       p->inst_interp_error(p, rv), p->interp_error(p, rv));
			printf("Disable initial-calibrate not supported\n");
		}
	}

	/* Do the calibration */
	rv = inst_handle_calibrate(p, inst_calt_available, inst_calc_none, setup_display_calibrate, &dwi, 0);
	setup_display_calibrate(p,inst_calc_none, &dwi); 
	if (rv == inst_unsupported) {
		printf("No calibration available for instrument in this mode\n");
	} else if (rv != inst_ok) {	/* Abort or fatal error */
		printf("Calibrate failed with '%s' (%s)\n",
	       p->inst_interp_error(p, rv), p->interp_error(p, rv));
		p->del(p);
		return -1;
	}

	/* clean up */
	p->del(p);
	a1logv(log, 1, "Finished setting up the instrument\n");

	return 0;
}

/* The i1pro/munki seems to get into trouble if we do a meas_delay */
/* on another thread - some conflict with the button monotitor thread. */

/* Set color to white after 200 msec delay */
int del_set_white(void *cx) {
	disprd *p = (disprd *)cx;
	inst_code ev;
	int rv;

	msec_sleep(200);

	/* Start the patch change */
	/* This function may return some time before or after */
	/* the change actually arrives at the instrument. */
	if ((rv = p->dw->set_color(p->dw, 1.0, 1.0, 1.0)) != 0) {
		a1logd(p->log,1,"set_color() returned %d\n",rv);
		return 3;
	}

	/* Signal instrument that we've returned from the patch change function */
	/* (We're interested in any delay from this point in time until the */
	/*  change arrives at the instrument, since this is the point that we */
	/*  add extra delay at the end of the set_color() functions) */
	if ((ev = p->it->white_change(p->it, 0)) != inst_ok) {
		a1logd(p->log,1,"white_change() returned 0x%x\n",ev);
		return 3;
	}

	return 0;
}

#ifdef NEVER
/* Implement scallout, which reports the XYZ measured */
static void do_scallout(disprd *p, double *xyz) {
	char *cmd;

	if (p->scallout == NULL)
		return;

	if ((cmd = malloc(strlen(p->scallout) + 200)) == NULL)
		error("Malloc of command string failed");

	sprintf(cmd, "%s %f %f %f",p->scallout, xyz[0], xyz[1], xyz[2]);
	if ((rv = system(cmd)) != 0)
		error("System command '%s' failed with %d",cmd,rv); 
	free(cmd);
}
#endif

/* Take a series of readings from the display - implementation */
/* Return nz on fail/abort - see dispsup.h */
/* Use disprd_err() to interpret it */
static int disprd_read_imp(
	disprd *p,
	col *cols,		/* Array of patch colors to be tested */
	int npat, 		/* Number of patches to be tested */
	int spat,		/* Start patch index for "verb", 0 if not used */
	int tpat,		/* Total patch index for "verb", 0 if not used */
	int acr,		/* If nz, do automatic final carriage return */
	int noinc,		/* If nz, don't increment the count */
	int tc,			/* If nz, termination key */
	instClamping clamp	/* NZ if clamp XYZ/Lab to be +ve */
) {
	int j, rv;
	int patch;
	ipatch val;		/* Return value */
	int ch;			/* Character */
//	int cal_type;
	char id[CALIDLEN];
	inst2_capability cap2;

	/* Setup the keyboard trigger to return our commands */
	inst_set_uih(0x0, 0xff,  DUIH_TRIG);
	inst_set_uih('q', 'q',   DUIH_ABORT);
	inst_set_uih('Q', 'Q',   DUIH_ABORT);
	inst_set_uih(0x03, 0x03, DUIH_ABORT);		/* ^c */
	inst_set_uih(0x1b, 0x1b, DUIH_ABORT);		/* Esc */

	/* Setup user termination character */
	inst_set_uih(tc, tc, DUIH_TERM);

	p->it->capabilities(p->it, NULL, &cap2, NULL);

	/* See if we should calibrate the display update */
	if (!p->update_delay_set && (cap2 & inst2_meas_disp_update) != 0) {
		athread *th;
//		inst_code ev;
		int mdelay;			/* Display update delay*/
		int idelay = 0;		/* Instrument reaction time */
	
		/* Disable the update delay */
		p->dw->enable_update_delay(p->dw, 0);

		/* Set black */
		if ((rv = p->dw->set_color(p->dw, 0.0, 0.0, 0.0)) != 0) {
			a1logd(p->log,1,"set_color() returned %d\n",rv);
			p->dw->enable_update_delay(p->dw, 1);
			return 3;
		}
		/* Wait for it to settle */
		msec_sleep(600);
		
		/* Init the white change stamp */
		p->it->white_change(p->it, 1);

		/* Set black 200 msec after this call and call white_change() */
		if ((th = new_athread(del_set_white, (void *)p)) == NULL) {
			a1logd(p->log,1,"failed to create thread to set_color()\n");
			p->dw->enable_update_delay(p->dw, 1);
			return 3;
		}

		/* If the test window is adding extra known delay after the patch */
		/* update, don't start looking for the transition too soon, */
		/* or we may exaust our 2.0 second scan window */
		if (p->dw->extra_update_delay > 0.2) {
			a1logd(p->log,1,"update delay cal waiting %f secs to start scan\n",
			                                       p->dw->extra_update_delay - 0.2);
			msec_sleep((int)((p->dw->extra_update_delay - 0.2) * 1000));
		}
		
		/* Measure the delay */
		if ((rv = p->it->meas_delay(p->it, &mdelay, &idelay)) != inst_ok) {
			a1logd(p->log,1,"warning, measure display update delay failed with '%s' (%s)\n",
				      p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
			mdelay = PATCH_UPDATE_DELAY;
			/* idelay will be set to a default by meas_delay() */
		} else {
			a1logv(p->log, 1, "Measured display update delay of %d msec",mdelay);
			mdelay += mdelay/3 + 100;		/* Margin - +30% + 100msec */
		}
		p->dw->set_update_delay(p->dw, mdelay, idelay);
		a1logv(p->log, 1, ", using delay of %d msec & %d msec inst reaction\n",mdelay, idelay);
		p->update_delay_set = 1;

		/* Re-enable update delay */
		p->dw->enable_update_delay(p->dw, 1);

		th->del(th);
	}

	/* See if we should do a frequency calibration or display integration time cal. first */
	if (p->it->needs_calibration(p->it) & inst_calt_ref_freq
	 && npat > 0
	 && (cols[0].r != 0.8 || cols[0].g != 0.8 || cols[0].b != 0.8)) {
//		col tc;
		inst_cal_type calt = inst_calt_ref_freq;
		inst_cal_cond calc = inst_calc_emis_80pc;

		/* Hmm. Should really ask the instrument what sort of calc it needs !!! */
		if ((rv = p->dw->set_color(p->dw, 0.8, 0.8, 0.8)) != 0) {
			a1logd(p->log,1,"set_color() returned %d\n",rv);
			return 3;
		}
		/* Do calibrate, but ignore return code. Press on regardless. */
		if ((rv = p->it->calibrate(p->it, &calt, &calc, id)) != inst_ok) {
			a1logd(p->log,1,"warning, frequency calibrate failed with '%s' (%s)\n",
				      p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
		}
	}

	/* See if we should do a display integration calibration first */
	if (p->it->needs_calibration(p->it) & inst_calt_emis_int_time) {
//		col tc;
		inst_cal_type calt = inst_calt_emis_int_time;
		inst_cal_cond calc = inst_calc_emis_white;

		if ((rv = p->dw->set_color(p->dw, 1.0, 1.0, 1.0)) != 0) {
			a1logd(p->log,1,"set_color() returned %d\n",rv);
			return 3;
		}
		/* Do calibrate, but ignore return code. Press on regardless. */
		if ((rv = p->it->calibrate(p->it, &calt, &calc, id)) != inst_ok) {
			a1logd(p->log,1,"warning, display integration calibrate failed with '%s' (%s)\n",
				      p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
		}
	}

	for (patch = 0; patch < npat; patch++) {
		col *scb = &cols[patch];
		double rgb[3];

		scb->mtype = inst_mrt_none;
		scb->XYZ_v = 0;		/* No readings are valid */
		scb->sp.spec_n = 0;
		scb->duration = 0.0;

		if (spat != 0 && tpat != 0) {
			a1logv(p->log, 1, "%cpatch %d of %d",cr_char,spat + (noinc != 0 ? 0 : patch),tpat);
			if (p->dw->set_pinfo != NULL)
				p->dw->set_pinfo(p->dw, spat + (noinc != 0 ? 0 : patch),tpat);
		}
		a1logd(p->log,1,"About to read patch %d\n",patch);

		rgb[0] = scb->r;
		rgb[1] = scb->g;
		rgb[2] = scb->b;

		/* If we are doing a soft cal, apply it to the test color */
		/* (dispwin will apply any tvenc needed) */
		if ((p->native & 1) && p->cal[0][0] >= 0.0) {
			int j;
			double inputEnt_1 = (double)(p->ncal-1);

			for (j = 0; j < 3; j++) {
				unsigned int ix;
				double val, w;
				val = rgb[j] * inputEnt_1;
				if (val < 0.0) {
					val = 0.0;
				} else if (val > inputEnt_1) {
					val = inputEnt_1;
				}
				ix = (unsigned int)floor(val);		/* Coordinate */
				if (ix > (p->ncal-2))
					ix = (p->ncal-2);
				w = val - (double)ix;		/* weight */
				val = p->cal[j][ix];
				rgb[j] = val + w * (p->cal[j][ix+1] - val);
			}
		}
		if ((rv = p->dw->set_color(p->dw, rgb[0], rgb[1], rgb[2])) != 0) {
			a1logd(p->log,1,"set_color() returned %d\n",rv);
			return 3;
		}

		/* Until we give up retrying */
		for (;;) {
			val.mtype = inst_mrt_none;
			val.XYZ_v = 0;		/* No readings are valid */
			val.sp.spec_n = 0;
			val.duration = 0.0;
	
			if ((rv = p->it->read_sample(p->it, scb->id, &val, 1)) != inst_ok
			     && (rv & inst_mask) != inst_user_trig) {
				a1logd(p->log,1,"read_sample returned '%s' (%s)\n",
			       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));

				/* deal with a user terminate or abort */
				if ((rv & inst_mask) == inst_user_abort) {
					int keyc = inst_get_uih_char();

					/* Deal with a user terminate */
					if (keyc & DUIH_TERM) {
						return 4;

					/* Deal with a user abort */
					} else if (keyc & DUIH_ABORT) {
						empty_con_chars();
						printf("\nSample read stopped at user request!\n");
						printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
						if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
							printf("\n");
							return 1;
						}
						printf("\n");
						continue;
					}

				/* Deal with needing calibration */
				} else if ((rv & inst_mask) == inst_needs_cal) {
					disp_win_info dwi;
					dwi.dw = p->dw;		/* Set window to use */
					printf("\nSample read failed because instruments needs calibration\n");
					rv = inst_handle_calibrate(p->it, inst_calt_needed, inst_calc_none,
					                                        setup_display_calibrate, &dwi, 0);
					setup_display_calibrate(p->it, inst_calc_none, &dwi); 
					if (rv != inst_ok) {	/* Abort or fatal error */
						return 1;
					}

					printf("Place instrument back on test window.\n");
					printf("Hit Esc or Q to give up, any other key to continue:"); fflush(stdout);
					if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
						printf("\n");
						return 1;
					}
					printf("\n");
					continue;

				/* Deal with a bad sensor position */
				} else if ((rv & inst_mask) == inst_wrong_config) {
					empty_con_chars();
					printf("\n\nSpot read failed due to the sensor being in the wrong position\n");
					printf("(%s)\n",p->it->interp_error(p->it, rv));
					printf("Correct position then hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
					if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
						printf("\n");
						return 1;
					}
					printf("\n");
					continue;

				/* Deal with a misread */
				} else if ((rv & inst_mask) == inst_misread) {
					empty_con_chars();
					printf("\nSample read failed due to misread\n");
					printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
					if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
						printf("\n");
						return 1;
					}
					printf("\n");
					continue;

				/* Deal with a communications error */
				} else if ((rv & inst_mask) == inst_coms_fail) {
					empty_con_chars();
					printf("\nSample read failed due to communication problem.\n");
					printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
					if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
						printf("\n");
						return 1;
					}
					printf("\n");
					if (p->it->icom->port_type(p->it->icom) == icomt_serial) {
						/* Allow retrying at a lower baud rate */
						int tt = p->it->last_scomerr(p->it);
						if (tt & (ICOM_BRK | ICOM_FER | ICOM_PER | ICOM_OER)) {
							if (p->br == baud_19200) p->br = baud_9600;
							else if (p->br == baud_9600) p->br = baud_4800;
							else if (p->br == baud_2400) p->br = baud_1200;
							else p->br = baud_1200;
						}
						if ((rv = p->it->init_coms(p->it, p->br, p->fc, 15.0)) != inst_ok) {
							a1logd(p->log,1,"init_coms returned '%s' (%s)\n",
						       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
							return 2;
						}
					}
				}
			    printf("\nSample read failed with unhandled error.\n");
				return 2;
			} else {
				break;		/* Sucesful reading */
			}
		}
		/* We only fall through with a valid reading */
		scb->serno = p->serno++;
		scb->msec = msec_time();

		a1logd(p->log,1, "got reading %f %f %f, transfering to col\n",
		                val.XYZ[0], val.XYZ[1], val.XYZ[2]);

		scb->mtype = val.mtype;

		/* Copy XYZ */
		if (val.XYZ_v != 0) {
			for (j = 0; j < 3; j++)
				scb->XYZ[j] = val.XYZ[j];
			scb->XYZ_v = 1;
		}

		/* Copy spectral */
		if (p->spectral && val.sp.spec_n > 0) {
			scb->sp = val.sp;
		}
		a1logd(p->log,1,"on to next reading\n");
	}
	/* Final return. */
	if (acr && spat != 0 && tpat != 0 && (spat+patch-1) == tpat)
		a1logv(p->log, 1, "\n");
	return 0;
}

/* A structure to hold info about drift samples */
typedef struct {
	col dcols[2];	/* Black and white readings */

	int off, count;	/* Offset and count into cols */

} dsamples;

/* Take a series of readings from the display - drift compensation */
/* Return nz on fail/abort - see dispsup.h */
/* Use disprd_err() to interpret it */
static int disprd_read_drift(
	disprd *p,
	col *cols,		/* Array of patch colors to be tested */
	int npat, 		/* Number of patches to be tested */
	int spat,		/* Start patch index for "verb", 0 if not used */
	int tpat,		/* Total patch index for "verb", 0 if not used */
	int acr,		/* If nz, do automatic final carriage return */
	int tc,			/* If nz, termination key */
	instClamping clamp	/* NZ if clamp XYZ/Lab to be +ve */
) {
	int rv, i, j, e;
	double fper, foff;
	int off, poff;
	int boff, eoff, dno;	/* b&w offsets and count */

	DBG((dbgo,"DRIFT_EPERIOD %d, DRIFT_IPERIOD %d, npat %d\n",DRIFT_EPERIOD,DRIFT_IPERIOD,npat))

	/* Figure number and offset for b&w */
	if (p->bdrift == 0) {			/* Must be just wdrift */
		boff = eoff = 1;
		dno = 1;
	} else if (p->wdrift == 0) {	/* Must be just bdrift */
		boff = eoff = 0;
		dno = 1;
	} else {						/* Must be both */
		boff = 0;
		eoff = 1;
		dno = 2;
	}

	/* Make sure these jave been initialised */
	p->last_bw[0].r = 
	p->last_bw[0].g = 
	p->last_bw[0].b = 0.0; 
	p->last_bw[1].r = 
	p->last_bw[1].g = 
	p->last_bw[1].b = 1.0; 

	/* If the last drift readings are invalid or too old, */
	/* or if we will use interpolation, read b&w */
#ifdef DEBUG
	DBG((dbgo,"last_bw_v = %d (%d)\n",p->last_bw_v,p->last_bw_v == 0))
	DBG((dbgo,"npat = %d > %d, serno %d, last serno %d (%d)\n",npat, DRIFT_EPERIOD, p->serno,p->last_bw[eoff].serno,(npat > DRIFT_EPERIOD && p->serno > p->last_bw[eoff].serno)))
	DBG((dbgo,"serno - lastserno %d > %d (%d)\n",p->serno - p->last_bw[eoff].serno, DRIFT_EPERIOD,(p->serno - p->last_bw[eoff].serno) > DRIFT_EPERIOD))
	DBG((dbgo,"msec %d - last %d = %d > %d (%d)\n",msec_time(),p->last_bw[boff].msec,msec_time() - p->last_bw[boff].msec,DRIFT_MAXSECS * 1000, (msec_time() - p->last_bw[eoff].msec) > (DRIFT_MAXSECS * 1000)))
#endif /* DEBUG */
	if (p->last_bw_v == 0											/* There are none */
	 || (npat > DRIFT_EPERIOD && p->serno > p->last_bw[eoff].serno)	/* We will interpolate */
	 || (p->serno - p->last_bw[eoff].serno - dno) > DRIFT_EPERIOD	/* extrapolate would be too far */
	 || (msec_time() - p->last_bw[eoff].msec) > (DRIFT_MAXSECS * 1000)) {	/* The current is too long ago */

		DBG((dbgo,"Reading a beginning set of %d b/w drift compensation patches\n",dno))
		a1logd(p->log,2, "Reading a beginning set of %d b/w drift compensation patches\n",dno);

		/* Read the black and/or white drift patch */
		if ((rv = disprd_read_imp(p, &p->last_bw[boff], dno, spat, tpat, 0, 1, tc, 0)) != 0) {
			return rv;
		}
		p->last_bw_v = 1;

		/* If there is no reference b&w, set them from this first b&w */
		if (p->ref_bw_v == 0) {
			p->ref_bw[0] = p->last_bw[0];
			p->ref_bw[1] = p->last_bw[1];
			p->ref_bw_v = 1;
		}
	}

	/* If there are enough patches to bracket with drift readings */ 
	if (npat > DRIFT_EPERIOD) {
		int ndrift = 2;			/* Number of drift records */
		dsamples *dss;

		/* Figure out the number of drift samples we need */
		ndrift += (npat-1)/DRIFT_IPERIOD;
		DBG((dbgo,"spat %d, npat = %d, tpat %d, ndrift = %d\n",spat,npat,tpat,ndrift))

		if ((dss = (dsamples *)calloc(sizeof(dsamples), ndrift)) == NULL) {
			a1logd(p->log,1, "malloc of %d dsamples failed\n",ndrift);
			return 5;
		}
	
		/* Set up bookeeping */
		fper = (double)npat/(ndrift-1.0);
		DBG((dbgo,"fper = %f\n",fper))
		foff = 0.0;
		for (poff = off = i = 0; i < ndrift; i++) {
			dss[i].off = off;
			foff += fper;
			off = (int)floor(foff + 0.5);
			if (i < (ndrift-1))
				dss[i].count = off - poff;
			else
				dss[i].count = 0;
			poff = off;
			DBG((dbgo,"dss[%d] off = %d, count = %d\n",i, dss[i].off,dss[i].count))

			dss[i].dcols[0].r = 
			dss[i].dcols[0].g = 
			dss[i].dcols[0].b = 0.0; 
			dss[i].dcols[1].r = 
			dss[i].dcols[1].g = 
			dss[i].dcols[1].b = 1.0; 
		}

		/* For each batch of patches bracketed by drift patch readings */
		for (i = 0; i < (ndrift-1); i++) {

			if (i == 0) {	/* Already done very first drift patches, so use them */
				/* Use last b&w */
				dss[i].dcols[0] = p->last_bw[0];
				dss[i].dcols[1] = p->last_bw[1];
			} else {
				/* Read the black and/or white drift patchs before next batch */
				DBG((dbgo,"Reading another set of %d b/w drift compensation patches\n",dno))
				a1logd(p->log,2, "Reading another set of %d b/w drift compensation patches\n",dno);
				if ((rv = disprd_read_imp(p, &dss[i].dcols[boff], dno, spat+dss[i].off, tpat, 0, 1, tc, 0)) != 0) {
					free(dss);
					return rv;
				}
			}
			/* Read this batch of patches */
			if ((rv = disprd_read_imp(p, &cols[dss[i].off], dss[i].count,spat+dss[i].off,tpat,0,0,tc, 0)) != 0) {
				free(dss);
				return rv;
			}
		}
		/* Read the black and/or white drift patchs after last batch */
		DBG((dbgo,"Reading an end set of %d b/w drift compensation patches\n",dno))
		a1logd(p->log,2, "Reading an end set of %d b/w drift compensation patches\n",dno);
		if ((rv = disprd_read_imp(p, &dss[i].dcols[boff], dno, spat+dss[i].off-1, tpat, 0, 1, tc, 0)) != 0) {
			free(dss);
			return rv;
		}
		/* Remember the last for next time */
		p->last_bw[0] = dss[i].dcols[0];
		p->last_bw[1] = dss[i].dcols[1];
		p->last_bw_v = 1;

		/* Set the white drift target to be the last one for batch */
		p->targ_w = p->last_bw[1];
		p->targ_w_v = 1;

		DBG((dbgo,"ref b = %f %f %f\n", p->ref_bw[0].XYZ[0], p->ref_bw[0].XYZ[1], p->ref_bw[0].XYZ[2]))
		DBG((dbgo,"ref w = %f %f %f\n", p->ref_bw[1].XYZ[0], p->ref_bw[1].XYZ[1], p->ref_bw[1].XYZ[2]))
		DBG((dbgo,"ndrift-1 b = %f %f %f\n", dss[ndrift-1].dcols[0].XYZ[0], dss[ndrift-1].dcols[0].XYZ[1], dss[ndrift-1].dcols[0].XYZ[2]))
		DBG((dbgo,"ndrift-1 w = %f %f %f\n", dss[ndrift-1].dcols[1].XYZ[0], dss[ndrift-1].dcols[1].XYZ[1], dss[ndrift-1].dcols[1].XYZ[2]))

		/* Apply the drift compensation using interpolation */
		for (i = 0; i < (ndrift-1); i++) {

			for (j = 0; j < dss[i].count; j++) {
				int k = dss[i].off + j;
				double we;		/* Interpolation weight of eairlier value */
				col bb, ww;		/* Interpolated black and white */
#ifdef DEBUG
				double uXYZ[3];
				icmCpy3(uXYZ, cols[k].XYZ);
				DBG((dbgo,"patch %d = %f %f %f\n", k, cols[k].XYZ[0], cols[k].XYZ[1], cols[k].XYZ[2]))
#endif
				if (p->bdrift) {
					we = (double)(dss[i+1].dcols[0].msec - cols[k].msec)/
					     (double)(dss[i+1].dcols[0].msec - dss[i].dcols[0].msec);

					if (cols[k].XYZ_v) {
						for (e = 0; e < 3; e++) {
							bb.XYZ[e] =        we  * dss[i].dcols[0].XYZ[e]
							          + (1.0 - we) * dss[i+1].dcols[0].XYZ[e];
						}
						DBG((dbgo,"bb = %f %f %f\n", bb.XYZ[0], bb.XYZ[1], bb.XYZ[2]))
					}
					if (cols[k].sp.spec_n > 0) {
						for (e = 0; e < cols[k].sp.spec_n; e++) {
							bb.sp.spec[e] =        we  * dss[i].dcols[0].sp.spec[e]
							          + (1.0 - we) * dss[i+1].dcols[0].sp.spec[e];
						}
					}
				}
				if (p->wdrift) {
					we = (double)(dss[i+1].dcols[1].msec - cols[k].msec)/
					     (double)(dss[i+1].dcols[1].msec - dss[i].dcols[1].msec);

					if (cols[k].XYZ_v) {
						for (e = 0; e < 3; e++) {
							ww.XYZ[e] =        we  * dss[i].dcols[1].XYZ[e]
							          + (1.0 - we) * dss[i+1].dcols[1].XYZ[e];
						}
						DBG((dbgo,"ww = %f %f %f\n", ww.XYZ[0], ww.XYZ[1], ww.XYZ[2]))
					}
					if (cols[k].sp.spec_n > 0) {
						for (e = 0; e < cols[k].sp.spec_n; e++) {
							ww.sp.spec[e] =        we  * dss[i].dcols[1].sp.spec[e]
							          + (1.0 - we) * dss[i+1].dcols[1].sp.spec[e];
						}
					}
				}

				if (p->bdrift) {
					/* Compensate interpolated black for any white change from ref. */
					if (p->wdrift) {
						if (cols[k].XYZ_v) {
							for (e = 0; e < 3; e++) {
								bb.XYZ[e] *= p->ref_bw[1].XYZ[e]/ww.XYZ[e];
							}
							DBG((dbgo,"wcomp bb = %f %f %f\n", bb.XYZ[0], bb.XYZ[1], bb.XYZ[2]))
						}
						if (cols[k].sp.spec_n > 0) {
							for (e = 0; e < cols[k].sp.spec_n; e++) {
								bb.sp.spec[e] *= p->ref_bw[1].sp.spec[e]/ww.sp.spec[e];
							}
						}
					}

					/* Compensate the reading for any black drift from ref. */
					if (cols[k].XYZ_v) {
						for (e = 0; e < 3; e++) {
							cols[k].XYZ[e] += p->ref_bw[0].XYZ[e] - bb.XYZ[e];
						}
						DBG((dbgo,"bcomp patch = %f %f %f\n",  cols[k].XYZ[0], cols[k].XYZ[1], cols[k].XYZ[2]))
					}
					if (cols[k].sp.spec_n > 0) {
						for (e = 0; e < cols[k].sp.spec_n; e++) {
							cols[k].sp.spec[e] += p->ref_bw[0].sp.spec[e] - bb.sp.spec[e];
						}
					}
				}
				/* Compensate the reading for any white drift to target white */
				if (p->wdrift) {
					if (cols[k].XYZ_v) {
						for (e = 0; e < 3; e++) {
							cols[k].XYZ[e] *= p->targ_w.XYZ[e]/ww.XYZ[e];
						}
						DBG((dbgo,"wcomp patch = %f %f %f\n",  cols[k].XYZ[0], cols[k].XYZ[1], cols[k].XYZ[2]))
					}
					if (cols[k].sp.spec_n > 0) {
						for (e = 0; e < cols[k].sp.spec_n; e++) {
							cols[k].sp.spec[e] *= p->targ_w.sp.spec[e]/ww.sp.spec[e];
						}
					}
				}
				DBG((dbgo,"%d: drift change %f DE\n",k,icmXYZLabDE(&icmD50, uXYZ, cols[k].XYZ)))
			}
		}
		free(dss);

	/* Else too small a batch, use extrapolation from the last b&w */
	} else {

		DBG((dbgo,"doing small number of readings\n"))
		/* Read the small number of patches */
		if ((rv = disprd_read_imp(p, cols,npat,spat,tpat,acr,0,tc,0)) != 0)
			return rv;

		if (p->targ_w_v == 0) {
			/* Set the white drift reference to be the last one */
			p->targ_w = p->last_bw[1];
			p->targ_w_v = 1;
			DBG((dbgo,"set white drift target\n"))
		}

		DBG((dbgo,"ref b = %f %f %f\n", p->ref_bw[0].XYZ[0], p->ref_bw[0].XYZ[1], p->ref_bw[0].XYZ[2]))
		DBG((dbgo,"ref w = %f %f %f\n", p->ref_bw[1].XYZ[0], p->ref_bw[1].XYZ[1], p->ref_bw[1].XYZ[2]))
		DBG((dbgo,"last b = %f %f %f\n", p->last_bw[0].XYZ[0], p->last_bw[0].XYZ[1], p->last_bw[0].XYZ[2]))
		DBG((dbgo,"last w = %f %f %f\n", p->last_bw[1].XYZ[0], p->last_bw[1].XYZ[1], p->last_bw[1].XYZ[2]))
		DBG((dbgo,"target w = %f %f %f\n", p->targ_w.XYZ[0], p->targ_w.XYZ[1], p->targ_w.XYZ[2]))
		/* Apply the drift compensation using extrapolation */
		for (j = 0; j < npat; j++) {
//			double we;		/* Interpolation weight of eairlier value */
			col bb, ww;		/* Interpolated black and white */

#ifdef DEBUG
			double uXYZ[3];
			DBG((dbgo,"patch %d = %f %f %f\n", j, cols[j].XYZ[0], cols[j].XYZ[1], cols[j].XYZ[2]))
			icmCpy3(uXYZ, cols[j].XYZ);
#endif

			if (p->bdrift) {
				bb = p->last_bw[0];
				DBG((dbgo,"bb = %f %f %f\n", bb.XYZ[0], bb.XYZ[1], bb.XYZ[2]))
			}
			if (p->wdrift) {
				ww = p->last_bw[1];
				DBG((dbgo,"ww = %f %f %f\n", ww.XYZ[0], ww.XYZ[1], ww.XYZ[2]))
			}

			if (p->bdrift) {
				/* Compensate interpolated black for any white change from ref. */
				if (p->wdrift) {
					if (cols[j].XYZ_v) {
						for (e = 0; e < 3; e++) {
							bb.XYZ[e] *= p->ref_bw[1].XYZ[e]/ww.XYZ[e];
						}
						DBG((dbgo,"wcomp bb = %f %f %f\n", bb.XYZ[0], bb.XYZ[1], bb.XYZ[2]))
					}
					if (cols[j].sp.spec_n > 0) {
						for (e = 0; e < cols[j].sp.spec_n; e++) {
							bb.sp.spec[e] *= p->ref_bw[1].sp.spec[e]/ww.sp.spec[e];
						}
					}
				}

				/* Compensate the reading for any black drift from ref. */
				if (cols[j].XYZ_v) {
					for (e = 0; e < 3; e++) {
						cols[j].XYZ[e] += p->ref_bw[0].XYZ[e] - bb.XYZ[e];
					}
					DBG((dbgo,"bcomp patch = %f %f %f\n",  cols[j].XYZ[0], cols[j].XYZ[1], cols[j].XYZ[2]))
				}
				if (cols[j].sp.spec_n > 0) {
					for (e = 0; e < cols[j].sp.spec_n; e++) {
						cols[j].sp.spec[e] += p->ref_bw[0].sp.spec[e] - bb.sp.spec[e];
					}
				}
			}
			/* Compensate the reading for any white drift to target white */
			if (p->wdrift) {
				if (cols[j].XYZ_v) {
					for (e = 0; e < 3; e++) {
						cols[j].XYZ[e] *= p->targ_w.XYZ[e]/ww.XYZ[e];
					}
					DBG((dbgo,"wcomp patch = %f %f %f\n",  cols[j].XYZ[0], cols[j].XYZ[1], cols[j].XYZ[2]))
				}
				if (cols[j].sp.spec_n > 0) {
					for (e = 0; e < cols[j].sp.spec_n; e++) {
						cols[j].sp.spec[e] *= p->targ_w.sp.spec[e]/ww.sp.spec[e];
					}
				}
			}
			DBG((dbgo,"%d: drift change %f DE\n",j,icmXYZLabDE(&icmD50, uXYZ, cols[j].XYZ)))
		}
	}

	if (acr && spat != 0 && tpat != 0 && (spat+npat-1) == tpat)
		a1logv(p->log, 1, "\n");

	if (clamp) {
		for (j = 0; j < npat; j++) {
			if (cols[j].XYZ_v)
				icmClamp3(cols[j].XYZ, cols[j].XYZ);
		}
	}
	return rv;
}

/* Reset the white drift target white value, for non-batch */
/* readings when white drift comp. is enabled */
static void disprd_reset_targ_w(disprd *p) {
	p->targ_w_v = 0;
}

/* Change the black/white drift compensation options. */
/* Note that this simply invalidates any reference readings, */
/* and therefore will not make for good black compensation */
/* if it is done a long time since the instrument calibration. */
static void disprd_change_drift_comp(disprd *p,
	int bdrift,			/* Flag, nz for black drift compensation */
	int wdrift			/* Flag, nz for white drift compensation */
) {
	if (p->bdrift && !bdrift) {		/* Turning black drift off */
		p->bdrift = 0;
		p->ref_bw_v = 0;
		p->last_bw_v = 0;
		p->targ_w_v = 0;

	} else if (!p->bdrift && bdrift) {	/* Turning black drift on */
		p->bdrift = 1;
		p->ref_bw_v = 0;
		p->last_bw_v = 0;
		p->targ_w_v = 0;
	}

	if (p->wdrift && !wdrift) {		/* Turning white drift off */
		p->wdrift = 0;
		p->ref_bw_v = 0;
		p->last_bw_v = 0;
		p->targ_w_v = 0;

	} else if (!p->wdrift && wdrift) {	/* Turning white drift on */
		p->wdrift = 1;
		p->ref_bw_v = 0;
		p->last_bw_v = 0;
		p->targ_w_v = 0;
	}
}

/* Take a series of readings from the display */
/* Return nz on fail/abort - see dispsup.h */
/* Use disprd_err() to interpret it */
static int disprd_read(
	disprd *p,
	col *cols,		/* Array of patch colors to be tested */
	int npat, 		/* Number of patches to be tested */
	int spat,		/* Start patch index for "verb", 0 if not used */
	int tpat,		/* Total patch index for "verb", 0 if not used */
	int acr,		/* If nz, do automatic final carriage return */
	int tc,			/* If nz, termination key */
	instClamping clamp	/* NZ if clamp XYZ/Lab to be +ve */
) {
	int rv, i;

	if (p->bdrift || p->wdrift) {
		if ((rv = disprd_read_drift(p, cols,npat,spat,tpat,acr,tc,clamp)) != 0)
			return rv;
	} else {
		if ((rv = disprd_read_imp(p, cols,npat,spat,tpat,acr,0,tc,clamp)) != 0)
			return rv;
	}

	/* Use spectral if available. */
	/* Since this is a display, assume that this is absolute spectral */
	if (p->sp2cie != NULL) {
		for (i = 0; i < npat; i++) {
			if (cols[i].sp.spec_n > 0) {
				p->sp2cie->convert(p->sp2cie, cols[i].XYZ, &cols[i].sp);
				if (clamp)
					icmClamp3(cols[i].XYZ, cols[i].XYZ);
				cols[i].XYZ_v = 1;
			}
		}
	}

	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* Return the refresh mode and cbid */
static void disprd_get_disptype(disprd *p, int *refrmode, int *cbid) {
	if (refrmode != NULL)
		*refrmode = p->refrmode;
	if (cbid != NULL)
		*cbid = p->cbid;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static int config_inst_displ(disprd *p);

/* Take an ambient reading if the instrument has the capability. */
/* return nz on fail/abort - see dispsup.h */
/* Use disprd_err() to interpret it */
int disprd_ambient(
	struct _disprd *p,
	double *ambient,		/* return ambient in cd/m^2 */
	int tc					/* If nz, termination key */
) {
	inst_mode cap = 0;
	inst2_capability cap2 = 0;
	inst3_capability cap3 = 0;
	inst_opt_type trigmode = inst_opt_unknown;  /* Chosen trigger mode */
	int uswitch = 0;                /* Instrument switch is enabled */
	ipatch val;
	int rv;

	if (p->it != NULL) { /* Not fake */
		p->it->capabilities(p->it, &cap, &cap2, &cap3);
	}
	
	if (!IMODETST(cap, inst_mode_emis_ambient)) {
		printf("Need ambient measurement capability,\n");
		printf("but instrument doesn't support it\n");
		return 8;
	}

	printf("\nPlease make sure the instrument is fitted with\n");
	printf("the appropriate ambient light measuring head.\n");
	
	if ((rv = p->it->set_mode(p->it, inst_mode_emis_ambient)) != inst_ok) {
		a1logd(p->log,1,"set_mode returned '%s' (%s)\n",
		       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
		return 2;
	}
	if (p->it != NULL) { /* Not fake */
		p->it->capabilities(p->it, &cap, &cap2, &cap3);
	}
	
	/* Select a reasonable trigger mode */
	if (cap2 & inst2_user_switch_trig) {
		trigmode = inst_opt_trig_user_switch;
		uswitch = 1;

	/* Or go for user via uicallback trigger */
	} else if (cap2 & inst2_user_trig) {
		trigmode = inst_opt_trig_user;

	/* Or something is wrong with instrument capabilities */
	} else {
		printf("No reasonable trigger mode avilable for this instrument\n");
		return 2;
	}

	if ((rv = p->it->get_set_opt(p->it, trigmode)) != inst_ok) {
		printf("\nSetting trigger mode failed with error :'%s' (%s)\n",
       	       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
		return 2;
	}

	/* Setup the keyboard trigger to return our commands */
	inst_set_uih(0x0, 0xff,  DUIH_TRIG);
	inst_set_uih('q', 'q',   DUIH_ABORT);
	inst_set_uih('Q', 'Q',   DUIH_ABORT);
	inst_set_uih(0x03, 0x03, DUIH_ABORT);		/* ^c */
	inst_set_uih(0x1b, 0x1b, DUIH_ABORT);		/* Esc */

	/* Setup user termination character */
	inst_set_uih(tc, tc, DUIH_TERM);

	/* Until we give up retrying */
	for (;;) {
		char ch;
		val.mtype = inst_mrt_none;
		val.XYZ_v = 0;		/* No readings are valid */
		val.sp.spec_n = 0;
		val.duration = 0.0;

		printf("\nPlace the instrument so as to measure ambient upwards, beside the display,\n");
		if (uswitch)
			printf("Hit ESC or Q to exit, instrument switch or any other key to take a reading: ");
		else
			printf("Hit ESC or Q to exit, any other key to take a reading: ");
		fflush(stdout);

		if ((rv = p->it->read_sample(p->it, "AMBIENT", &val, 1)) != inst_ok
		     && (rv & inst_mask) != inst_user_trig) {
			a1logd(p->log,1,"read_sample returned '%s' (%s)\n",
		       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));

			/* deal with a user terminate or abort */
			if ((rv & inst_mask) == inst_user_abort) {
				int keyc = inst_get_uih_char();

				/* Deal with a user terminate */
				if (keyc & DUIH_TERM) {
					return 4;

				/* Deal with a user abort */
				} else if (keyc & DUIH_ABORT) {
					empty_con_chars();
					printf("\nMeasure stopped at user request!\n");
					printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
					if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
						printf("\n");
						return 1;
					}
					printf("\n");
					continue;
				}

			/* Deal with needs calibration */
			} else if ((rv & inst_mask) == inst_needs_cal) {
				disp_win_info dwi;
				dwi.dw = p->dw;		/* Set window to use */
				printf("\nSample read failed because instruments needs calibration\n");
				rv = inst_handle_calibrate(p->it, inst_calt_needed, inst_calc_none,
				                                        setup_display_calibrate, &dwi, 0);
				setup_display_calibrate(p->it,inst_calc_none, &dwi); 
				if (rv != inst_ok) {	/* Abort or fatal error */
					return 1;
				}
				continue;

			/* Deal with a bad sensor position */
			} else if ((rv & inst_mask) == inst_wrong_config) {
				empty_con_chars();
				printf("\n\nSpot read failed due to the sensor being in the wrong position\n");
				printf("(%s)\n",p->it->interp_error(p->it, rv));
				printf("Correct position then hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
				if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
					printf("\n");
					return 1;
				}
				printf("\n");
				continue;

			/* Deal with a misread */
			} else if ((rv & inst_mask) == inst_misread) {
				empty_con_chars();
				printf("\nMeasurement failed due to misread\n");
				printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
				if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
					printf("\n");
					return 1;
				}
				printf("\n");
				continue;

			/* Deal with a communications error */
			} else if ((rv & inst_mask) == inst_coms_fail) {
				empty_con_chars();
				printf("\nMeasurement read failed due to communication problem.\n");
				printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
				if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
					printf("\n");
					return 1;
				}
				printf("\n");
				if (p->it->icom->port_type(p->it->icom) == icomt_serial) {
					/* Allow retrying at a lower baud rate */
					int tt = p->it->last_scomerr(p->it);
					if (tt & (ICOM_BRK | ICOM_FER | ICOM_PER | ICOM_OER)) {
						if (p->br == baud_19200) p->br = baud_9600;
						else if (p->br == baud_9600) p->br = baud_4800;
						else if (p->br == baud_2400) p->br = baud_1200;
						else p->br = baud_1200;
					}
					if ((rv = p->it->init_coms(p->it, p->br, p->fc, 15.0)) != inst_ok) {
						a1logd(p->log,1,"init_coms returned '%s' (%s)\n",
					       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
						return 2;
					}
				}
			continue;
			}
		} else {
			break;		/* Sucesful reading */
		}
	}
	/* Convert spectral to XYZ */
	if (p->sp2cie != NULL && val.sp.spec_n > 0) {
		p->sp2cie->convert(p->sp2cie, val.XYZ, &val.sp);
		icmClamp3(val.XYZ, val.XYZ);
		val.XYZ_v = 1;
	}

	if (val.XYZ_v == 0) {
		printf("Unexpected failure to get measurement\n");
		return 2;
	}

	a1logd(p->log,1,"Measured ambient of %f\n",val.XYZ[1]);
	if (ambient != NULL)
		*ambient = val.XYZ[1];
	
	/* Configure the instrument mode back to reading the display */
	if ((rv = config_inst_displ(p)) != 0)
		return rv;

	printf("\nPlace the instrument back on the test window\n");
	fflush(stdout);

	return 0;
}


/* Test without a spectrometer using a fake device */
/* Return nz on fail/abort  - see dispsup.h */
/* Use disprd_err() to interpret it */
static int disprd_fake_read(
	disprd *p,
	col *cols,		/* Array of patch colors to be tested */
	int npat, 		/* Number of patches to be tested */
	int spat,		/* Start patch index for "verb", 0 if not used */
	int tpat,		/* Total patch index for "verb", 0 if not used */
	int acr,		/* If nz, do automatic final carriage return */
	int tc,			/* If nz, termination key */
	instClamping clamp	/* NZ if clamp XYZ/Lab to be +ve */
) {
	double white[3];		/* White point */
	double red[3];			/* Red colorant */
	double green[3];		/* Green colorant */
	double blue[3];			/* Blue colorant */
	double doff[3];			/* device offsets */
	double mat[3][3];		/* Destination matrix */
	double xmat[3][3];		/* Extra matrix */
	double ooff[3];			/* XYZ offsets */
	double rgb[3];			/* Given test values */
	double crgb[3];			/* Calibrated test values */
//	double br = 35.4;		/* Overall brightness */
	double br = 120.0;		/* Overall brightness */
	int patch, j;
	int ttpat = tpat;
	inst_code (*uicallback)(void *, inst_ui_purp) = NULL;
	void *uicontext = NULL;

	if (ttpat < npat)
		ttpat = npat;

	/* Setup fake device */
	white[0] = br * 0.955;		/* Somewhere between D50 and D65 */
	white[1] = br * 1.00;
	white[2] = br * 0.97;
	red[0] = br * 0.41;
	red[1] = br * 0.21;
	red[2] = br * 0.02;
	green[0] = br * 0.30;
	green[1] = br * 0.55;
	green[2] = br * 0.15;
	blue[0] = br * 0.15;
	blue[1] = br * 0.10;
	blue[2] = br * 0.97;
#ifdef SIMPLE_MODEL
	doff[0] = doff[1] = doff[2] = 0.0; /* Input offset */
	ooff[0] = ooff[1] = ooff[2] = 0.0; /* Output offset */
#else
	/* Input offset added to RGB, equivalent to RGB offsets having various values */
	doff[0] = -0.03;
	doff[1] = 0.07;
	doff[2] = -0.08;

	/* Output offset - equivalent to flare [range 0.0 - 1.0] */
	ooff[0] = 0.03;
	ooff[1] = 0.04;
	ooff[2] = 0.09;
#endif

	if (icmRGBXYZprim2matrix(red, green, blue, white, mat))
		error("Fake read unexpectedly got singular matrix\n");

	icmSetUnity3x3(xmat);

	if (p->fake2 == 1) {
		/* Target matrix */
		xmat[0][0] = 1.0; xmat[0][1] = 0.01; xmat[0][2] = 0.02;
		xmat[1][0] = 0.1; xmat[1][1] = 1.11; xmat[1][2] = 0.12;
		xmat[2][0] = 0.2; xmat[2][1] = 0.2;  xmat[2][2] = 1.22;
	}

	uicallback = inst_get_uicallback();
	uicontext = inst_get_uicontext();

	/* Setup the keyboard trigger to return our commands */
	inst_set_uih(0x0, 0xff,  DUIH_TRIG);
	inst_set_uih('q', 'q',   DUIH_ABORT);
	inst_set_uih('Q', 'Q',   DUIH_ABORT);
	inst_set_uih(0x03, 0x03, DUIH_ABORT);		/* ^c */
	inst_set_uih(0x1b, 0x1b, DUIH_ABORT);		/* Esc */

	/* Setup user termination character */
	inst_set_uih(tc, tc, DUIH_TERM);

	for (patch = 0; patch < npat; patch++) {
		if (spat != 0 && tpat != 0)
			a1logv(p->log, 1, "%cpatch %d of %d",cr_char,spat+patch,tpat);
		crgb[0] = rgb[0] = cols[patch].r;
		crgb[1] = rgb[1] = cols[patch].g;
		crgb[2] = rgb[2] = cols[patch].b;

		/* deal with a user terminate or abort */
		if (uicallback(uicontext, inst_measuring) == inst_user_abort) {
			int ch, keyc = inst_get_uih_char();

			/* Deal with a user terminate */
			if (keyc & DUIH_TERM) {
				return 4;

			/* Deal with a user abort */
			} else if (keyc & DUIH_ABORT) {
				empty_con_chars();
				printf("\nSample read stopped at user request!\n");
				printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
				if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
					printf("\n");
					return 1;
				}
				printf("\n");
			}
		}

//printf("~1 patch %d RGB %f %f %f\n",patch,rgb[0], rgb[1], rgb[2]);
		/* If we have a calibration, apply it to the color */
		if (p->cal[0][0] >= 0.0) {
			double inputEnt_1 = (double)(p->ncal-1);

			for (j = 0; j < 3; j++) {
				unsigned int ix;
				double val, w;
				val = rgb[j] * inputEnt_1;
				if (val < 0.0) {
					val = 0.0;
				} else if (val > inputEnt_1) {
					val = inputEnt_1;
				}
				ix = (unsigned int)floor(val);		/* Coordinate */
				if (ix > (p->ncal-2))
					ix = (p->ncal-2);
				w = val - (double)ix;		/* weight */
				val = p->cal[j][ix];
				crgb[j] = val + w * (p->cal[j][ix+1] - val);
			}
//printf("~1 patch %d cal RGB %f %f %f\n",patch, crgb[0], crgb[1], crgb[2]);
		}

		/* If we have a test window, display the patch color */
		if (p->dw) {
			inst_code rv;
			if (p->native & 1) {	/* Apply soft calibration to display value */
				if ((rv = p->dw->set_color(p->dw, crgb[0], crgb[1], crgb[2])) != 0) {
					a1logd(p->log,1,"set_color() returned %d\n",rv);
					return 3;
				}
			} else {			/* Hardware will apply calibration */
				if ((rv = p->dw->set_color(p->dw, rgb[0], rgb[1], rgb[2])) != 0) {
					a1logd(p->log,1,"set_color() returned %d\n",rv);
					return 3;
				}
			}
		}

		/* Apply the fake devices level of quantization */
#ifdef FAKE_BITS
		if (p->fake2 == 0) {
			for (j = 0; j < 3; j++) {
				int vv;
				vv = (int) (crgb[j] * ((1 << FAKE_BITS) - 1.0) + 0.5);
				crgb[j] =  vv/((1 << FAKE_BITS) - 1.0);
			}
		}
#endif

		/* Apply device offset */
		for (j = 0; j < 3; j++) {
			if (crgb[j] < 0.0)
				crgb[j] = 0.0;
			else if (crgb[j] > 1.0)
				crgb[j] = 1.0;
			crgb[j] = doff[j] + (1.0 - doff[j]) * crgb[j];
		}

		/* Apply gamma */
		for (j = 0; j < 3; j++) {
			if (crgb[j] >= 0.0)
				crgb[j] = pow(crgb[j], 2.5);
			else
				crgb[j] = 0.0;
		}

		/* Convert to XYZ */
		icmMulBy3x3(cols[patch].XYZ, mat, crgb);

		/* Apply XYZ offset */
		for (j = 0; j < 3; j++)
			cols[patch].XYZ[j] += ooff[j];

		/* Apply extra matrix */
		icmMulBy3x3(cols[patch].XYZ, xmat, cols[patch].XYZ);

#ifdef FAKE_NOISE
		if (p->fake2 == 0) {
			cols[patch].XYZ[0] += 2.0 * FAKE_NOISE * d_rand(-1.0, 1.0);
			cols[patch].XYZ[1] += FAKE_NOISE * d_rand(-1.0, 1.0);
			cols[patch].XYZ[2] += 4.0 * FAKE_NOISE * d_rand(-1.0, 1.0);
		}
#endif
		if (clamp)
			icmClamp3(cols[patch].XYZ, cols[patch].XYZ);
//printf("~1 patch %d XYZ %f %f %f\n",patch,cols[patch].XYZ[0], cols[patch].XYZ[1], cols[patch].XYZ[2]);
		cols[patch].XYZ_v = 1;
		cols[patch].sp.spec_n = 0;
		cols[patch].mtype = inst_mrt_emission;
	}
	if (acr && spat != 0 && tpat != 0 && (spat+patch-1) == tpat)
		a1logv(p->log, 1, "\n");
	return 0;
}

/* Test without a spectrometer using a fake ICC profile device */
/* Return nz on fail/abort - see dispsup.h */
/* Use disprd_err() to interpret it */
static int disprd_fake_read_lu(
	disprd *p,
	col *cols,		/* Array of patch colors to be tested */
	int npat, 		/* Number of patches to be tested */
	int spat,		/* Start patch index for "verb", 0 if not used */
	int tpat,		/* Total patch index for "verb", 0 if not used */
	int acr,		/* If nz, do automatic final carriage return */
	int tc,			/* If nz, termination key */
	instClamping clamp	/* NZ if clamp XYZ/Lab to be +ve */
) {
	int patch, j;
	int ttpat = tpat;
	double br = 120.4;		/* Overall brightness */
	inst_code (*uicallback)(void *, inst_ui_purp) = NULL;
	void *uicontext = NULL;

	if (ttpat < npat)
		ttpat = npat;

	uicallback = inst_get_uicallback();
	uicontext = inst_get_uicontext();

	/* Setup the keyboard trigger to return our commands */
	inst_set_uih(0x0, 0xff,  DUIH_TRIG);
	inst_set_uih('q', 'q',   DUIH_ABORT);
	inst_set_uih('Q', 'Q',   DUIH_ABORT);
	inst_set_uih(0x03, 0x03, DUIH_ABORT);		/* ^c */
	inst_set_uih(0x1b, 0x1b, DUIH_ABORT);		/* Esc */

	/* Setup user termination character */
	inst_set_uih(tc, tc, DUIH_TERM);

	for (patch = 0; patch < npat; patch++) {
		double rgb[3];;
		if (spat != 0 && tpat != 0)
			a1logv(p->log, 1, "%cpatch %d of %d",cr_char,spat+patch,tpat);
		rgb[0] = cols[patch].r;
		rgb[1] = cols[patch].g;
		rgb[2] = cols[patch].b;

		/* deal with a user terminate or abort */
		if (uicallback(uicontext, inst_measuring) == inst_user_abort) {
			int ch, keyc = inst_get_uih_char();

			/* Deal with a user terminate */
			if (keyc & DUIH_TERM) {
				return 4;

			/* Deal with a user abort */
			} else if (keyc & DUIH_ABORT) {
				empty_con_chars();
				printf("\nSample read stopped at user request!\n");
				printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
				if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
					printf("\n");
					return 1;
				}
				printf("\n");
			}
		}

		/* If we have a test window, display the patch color */
		if (p->dw) {
			inst_code rv;
			if ((rv = p->dw->set_color(p->dw, rgb[0], rgb[1], rgb[2])) != 0) {
				a1logd(p->log,1,"set_color() returned %d\n",rv);
				return 3;
			}
		}

		/* If we have a RAMDAC, apply it to the color */
		if (p->cal[0][0] >= 0.0) {
			double inputEnt_1 = (double)(p->ncal-1);

			for (j = 0; j < 3; j++) {
				unsigned int ix;
				double val, w;

				val = rgb[j] * inputEnt_1;
				if (val < 0.0) {
					val = 0.0;
				} else if (val > inputEnt_1) {
					val = inputEnt_1;
				}
				ix = (unsigned int)floor(val);		/* Coordinate */
				if (ix > (p->ncal-2))
					ix = (p->ncal-2);
				w = val - (double)ix;		/* weight */
				val = p->cal[j][ix];
				rgb[j] = val + w * (p->cal[j][ix+1] - val);
			}
		}

		p->fake_lu->lookup(p->fake_lu, cols[patch].XYZ, rgb); 
		for (j = 0; j < 3; j++) 
			cols[patch].XYZ[j] *= br;
#ifdef FAKE_NOISE
		cols[patch].XYZ[0] += 2.0 * FAKE_NOISE * d_rand(-1.0, 1.0);
		cols[patch].XYZ[1] += FAKE_NOISE * d_rand(-1.0, 1.0);
		cols[patch].XYZ[2] += 4.0 * FAKE_NOISE * d_rand(-1.0, 1.0);
#endif
		if (clamp)
			icmClamp3(cols[patch].XYZ, cols[patch].XYZ);
		cols[patch].XYZ_v = 1;
		cols[patch].sp.spec_n = 0;
		cols[patch].mtype = inst_mrt_emission;
	}
	if (acr && spat != 0 && tpat != 0 && (spat+patch-1) == tpat)
		a1logv(p->log, 1, "\n");
	return 0;
}

/* Use without a direct connect spectrometer using shell callout */
/* Return nz on fail/abort - see duspsup.h */
/* Use disprd_err() to interpret it */
static int disprd_fake_read_co(disprd *p,
	col *cols,		/* Array of patch colors to be tested */
	int npat, 		/* Number of patches to be tested */
	int spat,		/* Start patch index for "verb", 0 if not used */
	int tpat,		/* Total patch index for "verb", 0 if not used */
	int acr,		/* If nz, do automatic final carriage return */
	int tc,			/* If nz, termination key */
	instClamping clamp	/* NZ if clamp XYZ/Lab to be +ve */
) {
	int patch, j;
	int ttpat = tpat;
	inst_code (*uicallback)(void *, inst_ui_purp) = NULL;
	void *uicontext = NULL;

	if (ttpat < npat)
		ttpat = npat;

	uicallback = inst_get_uicallback();
	uicontext = inst_get_uicontext();

	/* Setup the keyboard trigger to return our commands */
	inst_set_uih(0x0, 0xff,  DUIH_TRIG);
	inst_set_uih('q', 'q',   DUIH_ABORT);
	inst_set_uih('Q', 'Q',   DUIH_ABORT);
	inst_set_uih(0x03, 0x03, DUIH_ABORT);		/* ^c */
	inst_set_uih(0x1b, 0x1b, DUIH_ABORT);		/* Esc */

	/* Setup user termination character */
	inst_set_uih(tc, tc, DUIH_TERM);

	for (patch = 0; patch < npat; patch++) {
		double rgb[3];
		int rv;
		char *cmd;
		FILE *fp;

		/* deal with a user terminate or abort */
		if (uicallback(uicontext, inst_measuring) == inst_user_abort) {
			int ch, keyc = inst_get_uih_char();

			/* Deal with a user terminate */
			if (keyc & DUIH_TERM) {
				return 4;

			/* Deal with a user abort */
			} else if (keyc & DUIH_ABORT) {
				empty_con_chars();
				printf("\nSample read stopped at user request!\n");
				printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
				if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
					printf("\n");
					return 1;
				}
				printf("\n");
			}
		}

		if (spat != 0 && tpat != 0)
			a1logv(p->log, 1, "%cpatch %d of %d",cr_char,spat+patch,tpat);
		rgb[0] = cols[patch].r;
		rgb[1] = cols[patch].g;
		rgb[2] = cols[patch].b;

		/* If we have a test window, display the patch color */
		if (p->dw) {
			inst_code rv;
			if ((rv = p->dw->set_color(p->dw, rgb[0], rgb[1], rgb[2])) != 0) {
				a1logd(p->log,1,"set_color() returned %d\n",rv);
				return 3;
			}
		}

		/* If we have a RAMDAC, apply it to the color */
		if (p->cal[0][0] >= 0.0) {
			double inputEnt_1 = (double)(p->ncal-1);

			for (j = 0; j < 3; j++) {
				unsigned int ix;
				double val, w;

				val = rgb[j] * inputEnt_1;
				if (val < 0.0) {
					val = 0.0;
				} else if (val > inputEnt_1) {
					val = inputEnt_1;
				}
				ix = (unsigned int)floor(val);		/* Coordinate */
				if (ix > (p->ncal-2))
					ix = (p->ncal-2);
				w = val - (double)ix;		/* weight */
				val = p->cal[j][ix];
				rgb[j] = val + w * (p->cal[j][ix+1] - val);
			}
		}

		if ((cmd = malloc(strlen(p->mcallout) + 200)) == NULL)
			error("Malloc of command string failed");

		sprintf(cmd, "%s %d %d %d %f %f %f",p->mcallout,
			        (int)(rgb[0] * 255.0 + 0.5),
		            (int)(rgb[1] * 255.0 + 0.5),
		            (int)(rgb[2] * 255.0 + 0.5), rgb[0], rgb[1], rgb[2]);
		if ((rv = system(cmd)) != 0)
			error("System command '%s' failed with %d",cmd,rv); 

		/* Now read the XYZ result from the mcallout.meas file */
		sprintf(cmd, "%s.meas",p->mcallout);
		if ((fp = fopen(cmd,"r")) == NULL)
			error("Unable to open measurement value file '%s'",cmd);

		if (fscanf(fp, " %lf %lf %lf", &cols[patch].XYZ[0], &cols[patch].XYZ[1],
		                               &cols[patch].XYZ[2]) != 3)
			error("Unable to parse measurement value file '%s'",cmd);
		fclose(fp);
		free(cmd);

		if (clamp)
			icmClamp3(cols[patch].XYZ, cols[patch].XYZ);
		cols[patch].XYZ_v = 1;
		cols[patch].mtype = inst_mrt_emission;

		a1logv(p->log, 2, "Read XYZ %f %f %f from '%s'\n", cols[patch].XYZ[0],
			                     cols[patch].XYZ[1],cols[patch].XYZ[2], cmd);

	}
	if (acr && spat != 0 && tpat != 0 && (spat+patch-1) == tpat)
		a1logv(p->log, 1, "\n");
	return 0;
}

/* Return a string describing the error code */
char *disprd_err(int en) {
	switch(en) {
		case 1:
			return "User Aborted";
		case 2:
			return "Instrument Access Failed";
		case 22:
			return "Instrument Access Failed (No PLD Pattern - have you run oeminst ?)";
		case 3:
			return "Window Access Failed";
		case 4:
			return "VideoLUT Access Failed";
		case 5:
			return "User Terminated";
		case 6:
			return "System Error";
		case 7:
			return "Either CRT or LCD must be selected";
		case 8:
			return "Instrument has no ambient measurement capability";
		case 9:
			return "Creating spectral conversion object failed";
		case 10:
			return "Instrument has no CCMX capability";
		case 11:
			return "Instrument has no CCSS capability";
		case 12:
			return "Video encoding requested using nonlinear current calibration curves";
		case 13:
			return "Video encoding requested for MadVR display - use MadVR to set video encoding";
		case 14:
			return "Instrument has no set refresh rate capability";
		case 15:
			return "Unknown calibration display type selection";
		case 16:
			return "Must use BASE calibration display type selection";
	}
	return "Unknown";
}

static void disprd_del(disprd *p) {

	if (p->log->verb >= 1 && p->bdrift && p->ref_bw_v && p->last_bw_v) {
		icmXYZNumber w;
		double de;
		icmAry2XYZ(w, p->ref_bw[1].XYZ);
		de = icmXYZLabDE(&w, p->ref_bw[0].XYZ, p->last_bw[0].XYZ);
		a1logv(p->log, 1, "Black drift was %f DE\n",de);
	}
	if (p->log->verb >= 1 && p->wdrift && p->ref_bw_v && p->last_bw_v) {
		icmXYZNumber w;
		double de;
		icmAry2XYZ(w, p->ref_bw[1].XYZ);
		de = icmXYZLabDE(&w, p->ref_bw[1].XYZ, p->last_bw[1].XYZ);
		a1logv(p->log, 1, "White drift was %f DE\n",de);
	}

	/* The user may remove the instrument */
	if (p->dw != NULL)
		printf("The instrument can be removed from the screen.\n");

	if (p->fake_lu != NULL)
		p->fake_lu->del(p->fake_lu);
	if (p->fake_icc != NULL)
		p->fake_icc->del(p->fake_icc);
	if (p->fake_fp != NULL)
		p->fake_fp->del(p->fake_fp);

	if (p->it != NULL)
		p->it->del(p->it);
	if (p->dw != NULL) {
		p->dw->del(p->dw);
	}
	if (p->sp2cie != NULL)
		p->sp2cie->del(p->sp2cie); 
	p->log = del_a1log(p->log);
	free(p);
}

/* Helper to configure the instrument mode ready */
/* for reading the display */
/* return new_disprd() error code */
static int config_inst_displ(disprd *p) {
	inst_mode cap;
	inst2_capability cap2;
	inst3_capability cap3;
	inst_mode mode = 0;
	int dtype = p->dtype;
	int rv;
	
	p->it->capabilities(p->it, &cap, &cap2, &cap3);
	
	if (p->tele && !IMODETST(cap, inst_mode_emis_tele)) {
		printf("Want telephoto measurement capability but instrument doesn't support it\n");
		printf("so falling back to spot mode.\n");
		a1logd(p->log,1,"No telephoto mode so falling back to spot mode.\n");
		p->tele = 0;
	}
	
	if (!p->tele && !IMODETST(cap, inst_mode_emis_spot)) {
		printf("Want emissive spot measurement capability but instrument doesn't support it\n");
		printf("so switching to telephoto spot mode.\n");
		p->tele = 1;
	}
	
	if (( p->tele && !IMODETST(cap, inst_mode_emis_tele))
	 || (!p->tele && !IMODETST(cap, inst_mode_emis_spot))) {
		printf("Need %s emissive measurement capability,\n", p->tele ? "telephoto" : "spot");
		printf("but instrument doesn't support it\n");
		a1logd(p->log,1,"Need %s emissive measurement capability but device doesn't support it,\n",
		       p->tele ? "telephoto" : "spot");
		return 2;
	}
	
	if (p->nadaptive && !IMODETST(cap, inst_mode_emis_nonadaptive)) {
		printf("Need non-adaptives measurement mode, but instrument doesn't support it\n");
		a1logd(p->log,1, "Need non-adaptives measurement mode, but instrument doesn't support it\n");
		return 2;
	}
	
	if (p->obType != icxOT_none
	 && p->obType != icxOT_default) {
		if (!IMODETST(cap, inst_mode_spectral) && (cap2 & inst2_ccss) == 0) {
			printf("A non-standard observer was requested,\n");
			printf("but instrument doesn't support spectral or CCSS\n");
			a1logd(p->log,1,"A non-standard observer was requested,\n"
			          "but instrument doesn't support spectral or CCSS\n");
			return 2;
		}
		/* Make sure spectral is turned on if an observer is requested */
		if (!p->spectral && (cap2 & inst2_ccss) == 0)
			p->spectral = 1;
	}

	if (p->spectral && !IMODETST(cap,inst_mode_spectral)) {
		if (p->spectral != 2) {		/* Not soft */
			printf("Spectral information was requested,\n");
			printf("but instrument doesn't support it\n");
			a1logd(p->log,1,"Spectral information was requested,\nbut instrument doesn't support it\n");
			return 2;
		}
		p->spectral = 0;
	}
	
	if (p->tele) {
		mode = inst_mode_emis_tele;
	} else {
		mode = inst_mode_emis_spot;
	}
	if (p->nadaptive)
		mode |= inst_mode_emis_nonadaptive;
	
	if (p->spectral) {
		mode |= inst_mode_spectral;
		p->spectral = 1;
	} else {
		p->spectral = 0;
	}
	
	/* If this is a spectral instrument, and a different */
	/* spectral inst. dtype is supplied, then use it */
	if (IMODETST(cap, inst_mode_spectral) && p->sdtype >= 0)
		dtype = p->sdtype;

	/* Set the display type */
	if (dtype != 0) {
		if (cap2 & inst2_disptype) {
			int ix;
			if ((ix = inst_get_disptype_index(p->it, dtype, p->docbid)) < 0) {
				a1logd(p->log,1,"Display type selection '%c' is not valid for instrument\n",dtype);
				if (p->docbid)
					return 16;
				return 15;
			}
			if ((rv = p->it->set_disptype(p->it, ix)) != inst_ok) {
				a1logd(p->log,1,"Setting display type failed with '%s' (%s)\n",
				       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
				return 15;
			}
		} else
			printf("Display type ignored - instrument doesn't support display type selection\n");
	}

	/* Disable initcalibration of machine if selected */
	if (p->noinitcal != 0) {
		if ((rv = p->it->get_set_opt(p->it,inst_opt_noinitcalib, 0)) != inst_ok) {
			a1logd(p->log,1,"Setting no-initial calibrate failed failed with '%s' (%s)\n",
		       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
			printf("Disable initial-calibrate not supported\n");
		}
	}
	
	if (p->highres) {
		if (IMODETST(cap, inst_mode_highres)) {
			mode |= inst_mode_highres;
		} else {
			a1logv(p->log, 1, "high resolution ignored - instrument doesn't support high res. mode\n");
		}
	}
	if ((rv = p->it->set_mode(p->it, mode)) != inst_ok) {
		a1logd(p->log,1,"set_mode returned '%s' (%s)\n",
		       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
		return 2;
	}
	p->it->capabilities(p->it, &cap, &cap2, &cap3);

	/* Set calibration matrix */
	if (p->ccmtx != NULL) {
		if ((cap2 & inst2_ccmx) == 0) {
			a1logd(p->log,1,"Instrument doesn't support ccmx correction\n");
			return 10;
		}
		if ((rv = p->it->col_cor_mat(p->it, p->cc_dtech, p->cc_cbid, p->ccmtx)) != inst_ok) {
			a1logd(p->log,1,"col_cor_mat returned '%s' (%s)\n",
			       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
			return 2;
		}
	}

	/* Get the refresh mode and cbid */
	p->it->get_disptechi(p->it, NULL, &p->refrmode, &p->cbid);
	
	/* Observer */
	if ((cap2 & inst2_ccss) != 0 && p->obType != icxOT_default) {

		if ((cap2 & inst2_ccss) == 0) {
			a1logd(p->log,1,"Instrument doesn't support ccss calibration and we need it\n");
			return 11;
		}
		if ((rv = p->it->get_set_opt(p->it, inst_opt_set_ccss_obs, p->obType, p->custObserver))
			                                                                      != inst_ok) {
			a1logd(p->log,1,"inst_opt_set_ccss_obs returned '%s' (%s)\n",
			       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
			return 2;
		}
	}

	/* Custom CCSS */
	if (p->sets != NULL) {

		if ((cap2 & inst2_ccss) == 0) {
			a1logd(p->log,1,"Instrument doesn't support ccss calibration and we need it\n");
			return 11;
		}

		if ((rv = p->it->col_cal_spec_set(p->it, p->cc_dtech, p->sets, p->no_sets)) != inst_ok) {
			a1logd(p->log,1,"col_cal_spec_set returned '%s' (%s)\n",
			       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
			return 2;
		}
	}
	
	if (p->refrate > 0.0) {
		if (!(cap2 & inst2_set_refresh_rate)) {
			a1logd(p->log,1,"Instrument doesn't support setting refresh rate\n");
			return 11;
		} else {
			if ((rv = p->it->set_refr_rate(p->it, p->refrate)) != inst_ok) {
				a1logd(p->log,1,"set_refr_rate %f Hz returned '%s' (%s)\n",
			     p->refrate, p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
				return 2;
			}
		}
	}

	/* Set the trigger mode to program triggered */
	if ((rv = p->it->get_set_opt(p->it,inst_opt_trig_prog)) != inst_ok) {
		a1logd(p->log,1,"Setting program trigger mode failed failed with '%s' (%s)\n",
	       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
		return 2;
	}

	/* Reset key meanings */
	inst_reset_uih();

	a1logd(p->log,1,"config_inst_displ suceeded\n");
	return 0;
}

/* Create a display reading object. */
/* Return NULL if error */
/* Set *errc to code. See dispsup.h */
/* Use disprd_err() to interpret *errc */
disprd *new_disprd(
int *errc,          /* Error code. May be NULL (could use log for this instead?) */
icompath *ipath,	/* Instrument path to open, &icomFakeDevice == fake */
flow_control fc,	/* Flow control */
int dtype,			/* Display type selection character */
int sdtype,			/* Spectro dtype, use dtype if -1 */
int docbid,			/* NZ to only allow cbid dtypes */
int tele,			/* NZ for tele mode. Falls back to display mode */
int nadaptive,		/* NZ for non-adaptive mode */
int noinitcal,		/* No initial instrument calibration */
int noinitplace,	/* Don't wait for user to place instrument on screen */
int highres,		/* Use high res mode if available */
double refrate,		/* If != 0.0, set display refresh rate calibration */
int native,			/* X0 = use current per channel calibration curve */
					/* X1 = set native linear output and use ramdac high precn. */
					/* 0X = use current color management cLut (MadVR) */
					/* 1X = disable color management cLUT (MadVR) */
int *noramdac,		/* Return nz if no ramdac access. native is set to X0 */
int *nocm,			/* Return nz if no CM cLUT access. native is set to 0X */
double cal[3][MAX_CAL_ENT],	/* Calibration (cal == NULL or cal[0][0] < 0.0 if not valid) */
int ncal,			/* Number of cal[] entries */
disppath *disp,		/* Display to calibrate. NULL if fake and no dispwin */
int out_tvenc,		/* 1 = use RGB Video Level encoding */
int blackbg,		/* NZ if whole screen should be filled with black */
int override,		/* Override_redirect on X11 */
int webdisp,        /* If nz, port number for web color display */
ccast_id *ccid,	 	/* non-NULL for ChromeCast */
#ifdef NT
int madvrdisp,		/* NZ for MadVR display */
#endif
char *ccallout,		/* Shell callout on set color */
char *mcallout,		/* Shell callout on measure color (forced fake) */
//char *scallout,		/* Shell callout on results of measure color */
double hpatsize,	/* Size of dispwin */
double vpatsize,
double ho,			/* Horizontal offset */
double vo,			/* Vertical offset */
disptech cc_dtech,	/* Display tech to go with ccmtx or sets */
int cc_cbid,		/* cbid to go with ccmtx or sets */
double ccmtx[3][3],	/* Colorimeter Correction matrix, NULL if none */
xspect *sets,		/* CCSS Set of sample spectra, NULL if none  */
int no_sets,	 	/* CCSS Number on set, 0 if none */
int spectral,		/* Generate spectral info flag */
icxObserverType obType,	/* Use alternate observer if spectral or CCSS and != icxOT_none */
xspect custObserver[3],	/* Optional custom observer */
int bdrift,			/* Flag, nz for black drift compensation */
int wdrift,			/* Flag, nz for white drift compensation */
char *fake_name,	/* Name of profile to use as a fake device */
a1log *log      	/* Verb, debug & error log */
) {
	disprd *p = NULL;
	int ch;
	inst_code rv;
	int uout_tvenc = out_tvenc;			/* tvenc to use with dispwin value setting */

	if (errc != NULL) *errc = 0;		/* default return code = no error */

	/* Allocate a disprd */
	if ((p = (disprd *)calloc(sizeof(disprd), 1)) == NULL) {
		a1logd(log, 1, "new_disprd failed due to malloc failure\n");
		if (errc != NULL) *errc = 6;
		return NULL;
	}
	p->log = new_a1log_d(log);
	p->del = disprd_del;
	p->read = disprd_read;
	p->get_disptype = disprd_get_disptype;
	p->reset_targ_w = disprd_reset_targ_w;
	p->change_drift_comp = disprd_change_drift_comp;
	p->ambient = disprd_ambient;
	p->fake_name = fake_name;

	p->cc_dtech = cc_dtech;
	p->cc_cbid = cc_cbid;
	p->ccmtx = ccmtx;			/* CCMX */
	p->sets = sets;
	p->no_sets = no_sets;		/* CCSS */
	p->spectral = spectral;
	p->obType = obType;			/* CCSS or spectral */
	p->custObserver = custObserver;
	p->bdrift = bdrift;
	p->wdrift = wdrift;
	p->dtype = dtype;
	p->sdtype = sdtype;
	p->docbid = docbid;
	p->refrmode = -1;			/* Unknown */
	p->cbid = 0;				/* Unknown */
	p->tele = tele;
	p->nadaptive = nadaptive;
	p->noinitcal = noinitcal;
	p->noinitplace = noinitplace;
	p->highres = highres;
	p->refrate = refrate;
	if (mcallout != NULL)
		ipath = &icomFakeDevice;	/* Force fake device */
	p->mcallout = mcallout;
//	p->scallout = scallout;
	p->ipath = ipath;
	p->br = baud_19200;
	p->fc = fc;

	p->native = native;

#ifdef FAKE_NOISE 
# ifdef FAKE_UNPREDIC
	rand32(time(NULL));
# endif
#endif

	/* Save this so we can return current cal, or */
	/* in case we are using a fake device */
	if (cal != NULL && cal[0][0] >= 0.0) {
		int j, i;
		for (j = 0; j < 3; j++) {
			for (i = 0; i < ncal; i++) {
				p->cal[j][i] = cal[j][i];
			}
		}
		p->ncal = ncal;
	} else {
		p->cal[0][0] =  -1.0;
		p->ncal = 0;
	}

	/* If non-real instrument */
	if (ipath == &icomFakeDevice) {
		p->fake = 1;

		p->fake_fp = NULL;
		p->fake_icc = NULL;
		p->fake_lu = NULL;

		/* See if there is a profile we should use as the fake device */
		if (p->mcallout == NULL && p->fake_name != NULL
		 && (p->fake_fp = new_icmFileStd_name(p->fake_name,"r")) != NULL) {
			if ((p->fake_icc = new_icc()) != NULL) {
				if (p->fake_icc->read(p->fake_icc,p->fake_fp,0) == 0) {
					icColorSpaceSignature ins;
					p->fake_lu = p->fake_icc->get_luobj(p->fake_icc, icmFwd, icAbsoluteColorimetric,
					                            icSigXYZData, icmLuOrdNorm);
					p->fake_lu->spaces(p->fake_lu, &ins, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
					if (ins != icSigRgbData) {
						p->fake_lu->del(p->fake_lu);
						p->fake_lu = NULL;
					}
				}
			}
		}

		if (p->fake_lu != NULL) {
			a1logv(p->log, 1, "Using profile '%s' rather than real device\n",p->fake_name);
			p->read = disprd_fake_read_lu;
		} else if (p->mcallout != NULL) {
			a1logv(p->log, 1, "Using shell callout '%s' rather than real device\n",p->mcallout);
			p->read = disprd_fake_read_co;
		} else
			p->read = disprd_fake_read;

		if (disp == NULL) {
			a1logd(log,1,"new_disprd returning fake device\n");
			return p;
		}

	/* Setup the instrument */
	} else {
	
		a1logv(p->log, 1, "Setting up the instrument\n");
	
		if ((p->it = new_inst(ipath, 0, log, DUIH_FUNC_AND_CONTEXT)) == NULL) {
			a1logd(p->log,1,"new_disprd failed because new_inst failed\n");
			p->del(p);
			if (errc != NULL) *errc = 2;
			return NULL;
		}
	
		/* Establish communications */
		if ((rv = p->it->init_coms(p->it, p->br, p->fc, 15.0)) != inst_ok) {
			a1logd(log,1,"init_coms returned '%s' (%s)\n",
			       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
			a1logd(log,1,"new_disprd failed because init_coms failed\n");
			p->del(p);
			if (errc != NULL) *errc = 2;
			return NULL;
		}
	
		/* Initialise the instrument */
		if ((rv = p->it->init_inst(p->it)) != inst_ok) {
			a1logd(log,1,"init_inst returned '%s' (%s)\n",
			       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
			a1logd(log,1,"new_disprd failed because init_inst failed\n");
			p->del(p);
			if (errc != NULL) {
				*errc = 2;
				if ((rv & inst_imask) == SPYD2_NO_PLD_PATTERN)
					*errc = 22;
			}
			return NULL;
		}
	
		/* Configure the instrument mode for reading the display */
		if ((rv = config_inst_displ(p)) != 0) {
			a1logd(log,1,"new_disprd failed because config_inst_displ failed\n");
			p->del(p);
			if (errc != NULL) *errc = rv;
			return NULL;
		}
	}

	/* Create a spectral conversion object if needed */
	if (p->spectral && p->obType != icxOT_none) {
		if ((p->sp2cie = new_xsp2cie(icxIT_none, NULL, p->obType, custObserver, icSigXYZData, icxNoClamp))
		                                                                                 == NULL) {
			a1logd(log,1,"new_disprd failed because creation of spectral conversion object failed\n");
			p->del(p);
			if (errc != NULL) *errc = 9;
			return NULL;
		}
	}

	if (webdisp != 0) {
		/* Open web display - no black bg since we assume window only */
		if ((p->dw = new_webwin(webdisp, hpatsize, vpatsize, ho, vo, 0, native, noramdac, nocm,
			                    uout_tvenc, 0, p->log->verb, p->log->debug)) == NULL) {
			a1logd(log,1,"new_disprd failed because new_webwin failed\n");
			p->del(p);
			if (errc != NULL) *errc = 3;
			return NULL;
		}
	} else if (ccid != NULL) {
		if ((p->dw = new_ccwin(ccid, hpatsize, vpatsize, ho, vo, 0, native, noramdac, nocm,
			                    uout_tvenc, 0, p->log->verb, p->log->debug)) == NULL) {
			a1logd(log,1,"new_disprd failed because new_ccwin('%s') failed\n",ccid->name);
			p->del(p);
			if (errc != NULL) *errc = 3;
			return NULL;
		}
#ifdef NT
	} else if (madvrdisp != 0) {
		if (out_tvenc) {
			a1logd(log,1,"new_disprd failed because tv_enc & MadVR window\n");
			p->del(p);
			if (errc != NULL) *errc = 13;
			return NULL;
		}
			
		if ((p->dw = new_madvrwin(hpatsize, vpatsize, ho, vo, 0, native, noramdac, nocm, out_tvenc,
			                                  0, p->log->verb, p->log->debug)) == NULL) {
			a1logd(log,1,"new_disprd failed because new_madvrwin failed\n");
			p->del(p);
			if (errc != NULL) *errc = 3;
			return NULL;
		}
#endif
	} else {
		/* Don't do tvenc on test values if we are going to do it with RAMDAC curve */
		if (out_tvenc && (p->native & 1) == 0 && p->cal[0][0] >= 0.0) {
			uout_tvenc = 0;
		}

		/* Open display window for positioning (no blackbg) */
		if ((p->dw = new_dispwin(disp, hpatsize, vpatsize, ho, vo, 0, native, noramdac, nocm,
			                               uout_tvenc, 0, override, p->log->debug)) == NULL) {
			a1logd(log,1,"new_disprd failed because new_dispwin failed\n");
			p->del(p);
			if (errc != NULL) *errc = 3;
			return NULL;
		}

		/* If TV encoding using existing RAMDAC and it is not linear, */
		/* error out, as TV encoding probably won't work properly. */
		if (out_tvenc && (native & 1) == 0 && p->cal[0][0] < 0.0) {
			ramdac *cr;
			if ((cr = p->dw->get_ramdac(p->dw)) != NULL) {
				int i, j;
				for (i = 0; i < cr->nent; i++) {
					double val = i/(cr->nent-1.0);
					for (j = 0; j < 3; j++) {
						if (fabs(val - cr->v[j][i]) > 1e-5)
							break;
					}
					if (j < 3)
						break;
				}
				if (i < cr->nent) {
					a1logd(log,1,"new_disprd failed because tvenc and nonlinear RAMDAC");
					cr->del(cr);
					p->del(p);
					if (errc != NULL) *errc = 12;
					return NULL;
				}
				cr->del(cr);
			}
		}

#ifdef NEVER		// Don't do this - dispread doesn't save calibration when using existing.
		/* If using exiting RAMDAC, return it */
		if ((native & 1) == 0 && p->cal[0][0] < 0.0 && cal != NULL) {
			ramdac *r;
			
			if ((r = p->dw->get_ramdac(p->dw)) != NULL) {
				int j, i;

				/* Get the ramdac contents. */
				/* We linearly interpolate from RAMDAC[nent] to cal[ncal] resolution */
				for (i = 0; i < ncal; i++) {
					double val, w;
					unsigned int ix;
	
					val = (r->nent-1.0) * i/(ncal-1.0);
					ix = (unsigned int)floor(val);		/* Coordinate */
					if (ix > (r->nent-2))
						ix = (r->nent-2);
					w = val - (double)ix;		/* weight */
					for (j = 0; j < 3; j++) {
						val = r->v[j][ix];
						cal[j][i] = val + w * (r->v[j][ix+1] - val);
					}
				}
				r->del(r);
			}
		}
#endif
	}

	if (p->it != NULL) {
		/* Do an instrument calibration up front, so as not to get in the users way, */
		/* but ignore a CRT frequency or display integration calibration, */
		/* since these will be done automatically. */
		if (p->it->needs_calibration(p->it) & inst_calt_n_dfrble_mask) {
			disp_win_info dwi;
			dwi.dw = p->dw;		/* Set window to use */
	
			rv = inst_handle_calibrate(p->it, inst_calt_needed, inst_calc_none,
			                                  setup_display_calibrate, &dwi, 0);
			setup_display_calibrate(p->it,inst_calc_none, &dwi); 
			printf("\n");
			if (rv != inst_ok) {	/* Abort or fatal error */
				a1logd(log,1,"new_disprd failed because calibrate failed with '%s' (%s)\n",
				       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
				printf("Calibrate failed with '%s' (%s)\n",
				       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
				p->del(p);
				if (errc != NULL) *errc = 2;
				return NULL;
			}
		}
	}

	if (p->it != NULL && !p->noinitplace) {
		inst2_capability cap2;

		p->it->capabilities(p->it, NULL, &cap2, NULL);

		/* Turn target on */
		if (cap2 & inst2_has_target)
			p->it->get_set_opt(p->it, inst_opt_set_target_state, 1);

		/* Ask user to put instrument on screen */
		empty_con_chars();

		printf("Place instrument on test window.\n");
		printf("Hit Esc or Q to give up, any other key to continue:"); fflush(stdout);
		if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
			printf("\n");
			a1logd(log,1,"new_disprd failed because user aborted when placing device\n");
			if (cap2 & inst2_has_target) /* Turn target off */
				p->it->get_set_opt(p->it, inst_opt_set_target_state, 0);
			p->del(p);
			if (errc != NULL) *errc = 1;
			return NULL;
		}

		/* Turn target off */
		if (cap2 & inst2_has_target)
			p->it->get_set_opt(p->it, inst_opt_set_target_state, 0);

		printf("\n");
	}

	/* All except web disp can have a black backround when running tests */
	if (webdisp == 0 && blackbg) {

		if (p->dw->set_bg(p->dw, blackbg)) {		/* Have to re-create window */

			/* Close the positioning window */
			if (p->dw != NULL) {
				p->dw->del(p->dw);
			}
		
			/* We happen to know that only the default dispwin needs re-creating */

			/* Open display window again for measurement */
			if ((p->dw = new_dispwin(disp, hpatsize, vpatsize, ho, vo, 0, native, noramdac, nocm,
				                     uout_tvenc, blackbg, override, p->log->debug)) == NULL) {
				a1logd(log,1,"new_disprd failed new_dispwin failed\n");
				p->del(p);
				if (errc != NULL) *errc = 3;
				return NULL;
			}
		}
	}

	/* Set display rise & fall time more optimally */
	if (p->it != NULL) {
		disptech dtech;
		disptech_info *tinfo;
		p->it->get_disptechi(p->it, &dtech, NULL, NULL);
		tinfo = disptech_get_id(dtech);
		p->dw->set_settling_delay(p->dw, tinfo->rise_time, tinfo->fall_time, -1.0);
	}

	/* Set color change callout */
	if (ccallout) {
		p->dw->set_callout(p->dw, ccallout);
	}

	/* If we have a calibration to set using the RAMDAC */
	/* (This is only typically the case for disread) */
	if ((p->native & 1) == 0 && p->cal[0][0] >= 0.0) {

		/* Save current RAMDAC so that we can restore it */
		if (p->dw->r == NULL) {
			warning("Unable to read or set display RAMDAC - switching to softcal");
			p->native |= 1;

		/* Set the given RAMDAC so we can characterise through it */
		} else {
			int j, i;
			
			/* Set the ramdac contents. */
			/* We linearly interpolate from cal[ncal] to RAMDAC[nent] resolution */
			for (i = 0; i < p->dw->r->nent; i++) {
				double val, w;
				unsigned int ix;

				val = (ncal-1.0) * i/(p->dw->r->nent-1.0);
				ix = (unsigned int)floor(val);		/* Coordinate */
				if (ix > (ncal-2))
					ix = (ncal-2);
				w = val - (double)ix;		/* weight */
				for (j = 0; j < 3; j++) {
					val = p->cal[j][ix];
					p->dw->r->v[j][i] = val + w * (p->cal[j][ix+1] - val);
					/* Implement tvenc here rather than dispwin, so that the order is correct */
					if (out_tvenc) {
						p->dw->r->v[j][i] = ((235.0 - 16.0) * p->dw->r->v[j][i] + 16.0)/255.0;

						/* For video encoding the extra bits of precision are created by bit */
						/* shifting rather than scaling, so we need to scale the fp value to */
						/* account for this. */
						if (p->dw->edepth > 8)
							p->dw->r->v[j][i] =
							           (p->dw->r->v[j][i] * 255 * (1 << (p->dw->edepth - 8)))
							           /((1 << p->dw->edepth) - 1.0); 	
					}
				}
			}
			if (p->dw->set_ramdac(p->dw, p->dw->r, 0)) {
				a1logd(log,1,"new_disprd failed becayse set_ramdac failed\n");
				a1logv(p->log, 1, "Failed to set RAMDAC to desired calibration.\n");
				a1logv(p->log, 1, "Perhaps the operating system is being fussy ?\n");
				if (errc != NULL) *errc = 4;
				return NULL;
			}
		}
	}

	a1logd(log,1,"new_disprd succeeded\n");
	return p;
}

