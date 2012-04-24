

/* 
 * Argyll Color Correction System
 * Common display patch reading support.
 *
 * Author: Graeme W. Gill
 * Date:   2/11/2005
 *
 * Copyright 1998 - 2007 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* 
	TTBD:

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
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "xspect.h"
#include "insttypes.h"
#include "icoms.h"
#include "inst.h"
#include "spyd2.h"
#include "conv.h"
#include "dispwin.h"
#include "dispsup.h"

#undef DEBUG

#define dbgo stderr

#ifdef DEBUG
#define DBG(xxx) fprintf xxx ;
#else
#define DBG(xxx) if (p != NULL && p->debug) fprintf xxx ;
#endif

#define DRIFT_IPERIOD	40	/* Number of samples between drift interpolation measurements */
#define DRIFT_EPERIOD	20	/* Number of samples between drift extrapolation measurements */
//#define DRIFT_IPERIOD	6	/* Test values */
//#define DRIFT_EPERIOD	3

#define FAKE_NOISE 0.01		/* Add noise to _fake_ devices XYZ */
#define FAKE_BITS 9			/* Number of bits of significance of fake device */


/* -------------------------------------------------------- */
/* A defauult callback that can be provided as an argument to */
/* inst_handle_calibrate() to handle the display part of a */
/* calibration callback. */
/* Call this again with calc = inst_calc_none to cleanup afterwards. */
inst_code setup_display_calibrate(
	inst *p,
	inst_cal_cond calc,		/* Current condition. inst_calc_none for not setup */
	disp_win_info *dwi		/* Information to be able to open a display test patch */
) {
	inst_code rv = inst_ok, ev;
	dispwin *dw;	/* Display window to display test patches on, NULL if none. */
	DBG((dbgo,"setup_display_calibrate called\n"))

	switch (calc) {
		case inst_calc_none:		/* Use this as a cleanup flag */
			if (dwi->dw == NULL && dwi->_dw != NULL) {
				dwi->_dw->del(dwi->_dw);
				dwi->_dw = NULL;
			}
			break;

		case inst_calc_disp_white:
		case inst_calc_proj_white:
			if (dwi->dw == NULL) {
				if ((dwi->_dw = new_dispwin(dwi->disp, dwi->patsize, dwi->patsize,
				                      dwi->ho, dwi->vo, 0, 0, dwi->blackbg,
				                      dwi->override, p->debug)) == NULL) {
					DBG((dbgo,"inst_handle_calibrate failed to create test window 0x%x\n",inst_other_error))
					return inst_other_error; 
				}
				printf("Frequency calibration, Place instrument on test window.\n");
				printf(" Hit any key to continue,\n"); 
				printf(" or hit Esc or Q to abort:"); 
			} else {
				dwi->_dw = dwi->dw;
			}
			p->cal_gy_level = 1.0;
			dwi->_dw->set_color(dwi->_dw, 1.0, 1.0, 1.0);
			break;

		case inst_calc_disp_grey:
		case inst_calc_disp_grey_darker: 
		case inst_calc_disp_grey_ligher:
		case inst_calc_proj_grey:
		case inst_calc_proj_grey_darker: 
		case inst_calc_proj_grey_ligher:
			if (dwi->dw == NULL) {
				if ((dwi->_dw = new_dispwin(dwi->disp, dwi->patsize, dwi->patsize,
				                      dwi->ho, dwi->vo, 0, 0, dwi->blackbg,
				                      dwi->override, p->debug)) == NULL) {
					DBG((dbgo,"inst_handle_calibrate failed to create test window 0x%x\n",inst_other_error))
					return inst_other_error; 
				}
				printf("Cell ratio calibration, Place instrument on test window.\n");
				printf(" Hit any key to continue,\n"); 
				printf(" or hit Esc or Q to abort:"); 
			} else {
				dwi->_dw = dwi->dw;
			}
			if (calc == inst_calc_disp_grey
			 || calc == inst_calc_proj_grey) {
				p->cal_gy_level = 0.6;
				p->cal_gy_count = 0;
			} else if (calc == inst_calc_disp_grey_darker
			        || calc == inst_calc_proj_grey_darker) {
				p->cal_gy_level *= 0.7;
				p->cal_gy_count++;
			} else if (calc == inst_calc_disp_grey_ligher
			        || calc == inst_calc_proj_grey_ligher) {
				p->cal_gy_level *= 1.4;
				if (p->cal_gy_level > 1.0)
					p->cal_gy_level = 1.0;
				p->cal_gy_count++;
			}
			if (p->cal_gy_count > 4) {
				printf("Cell ratio calibration failed - too many tries at setting grey level.\n");
				DBG((dbgo,"inst_handle_calibrate too many tries at setting grey level 0x%x\n",inst_internal_error))
				return inst_internal_error; 
			}
			dwi->_dw->set_color(dwi->_dw, p->cal_gy_level, p->cal_gy_level, p->cal_gy_level);
			break;

		default:
			/* Something isn't being handled */
			DBG((dbgo,"inst_handle_calibrate unhandled calc case 0x%x, err 0x%x\n",calc,inst_internal_error))
			return inst_internal_error;
	}
	return inst_ok;
}

/* -------------------------------------------------------- */
/* User requested calibration of the display instrument */
/* (Not supporting inst_crt_freq_cal, inst_calt_disp_int_time or inst_calt_proj_int_time */
/*  since these are done automatically in dispsup.) */
/* Should add an argument to be able to select type of calibration, */
/* rather than guessing what the user wants ? */
int disprd_calibration(
instType itype,			/* Instrument type (usually instUnknown) */
int comport, 			/* COM port used */
flow_control fc,		/* Serial flow control */
int dtype,				/* Display type, 0 = unknown, 1 = CRT, 2 = LCD */
int proj,				/* NZ for projector mode, falls back to display mode */
int adaptive,			/* NZ for adaptive mode */
int noautocal,				/* NZ to disable auto instrument calibration */
disppath *disp,			/* display to calibrate. */
int blackbg,			/* NZ if whole screen should be filled with black */
int override,			/* Override_redirect on X11 */
double patsize,			/* Size of dispwin */
double ho, double vo,	/* Position of dispwin */
int verb,				/* Verbosity flag */
int debug				/* Debug flag */
) {
	inst *p = NULL;
	int c;
	inst_code rv;
	baud_rate br = baud_19200;
	disp_win_info dwi;
	inst_cal_type calt = inst_calt_all;
	inst_capability  cap;
	inst2_capability cap2;
	inst_mode mode = 0;

	memset((void *)&dwi, 0, sizeof(disp_win_info));
	dwi.disp = disp; 
	dwi.blackbg = blackbg;
	dwi.override = override;
	dwi.patsize = patsize;
	dwi.ho = ho;
	dwi.vo = vo;

	if (verb)
		printf("Setting up the instrument\n");

	if ((p = new_inst(comport, itype, debug, verb, NULL)) == NULL) {
		DBG((dbgo,"new_inst failed\n"))
		return -1;
	}

	/* Establish communications */
	if ((rv = p->init_coms(p, comport, br, fc, 15.0)) != inst_ok) {
		DBG((dbgo,"init_coms returned '%s' (%s)\n",
		       p->inst_interp_error(p, rv), p->interp_error(p, rv)))
		p->del(p);
		return -1;
	}

	/* Initialise the instrument */
	if ((rv = p->init_inst(p)) != inst_ok) {
		DBG((dbgo,"init_inst returned '%s' (%s)\n",
		       p->inst_interp_error(p, rv), p->interp_error(p, rv)))
		p->del(p);
		return -1;
	}

	itype = p->get_itype(p);			/* Actual type */
	cap  = p->capabilities(p);
	cap2 = p->capabilities2(p);

	if (proj && (cap & inst_emis_proj) == 0) {
		printf("Want projection measurement capability but instrument doesn't support it\n");
		printf("so falling back to display mode.\n");
		proj = 0;
	}
	
	/* Set to emission mode to read a display */
	if (proj) {
		if (adaptive)
			mode = inst_mode_emis_tele;
		else
			mode = inst_mode_emis_proj;
	} else {
		if (adaptive)
			mode = inst_mode_emis_spot;
		else
			mode = inst_mode_emis_disp;
	}
	
	/* (We're assuming spectral doesn't affect calibration ?) */

	if ((rv = p->set_mode(p, mode)) != inst_ok) {
		DBG((dbgo,"Set_mode failed with '%s' (%s)\n",
		       p->inst_interp_error(p, rv), p->interp_error(p, rv)))
		return -1;
	}
	cap  = p->capabilities(p);
	cap2 = p->capabilities2(p);

	/* Set CRT or LCD mode */
	if ((cap & (inst_emis_disp_crt | inst_emis_disp_lcd | inst_emis_proj_crt | inst_emis_proj_lcd))
	 && (dtype == 1 || dtype == 2)) {
		inst_opt_mode om;

		if (proj) {
			if (dtype == 1)
				om = inst_opt_proj_crt;
			else
				om = inst_opt_proj_lcd;
		} else {
			if (dtype == 1)
				om = inst_opt_disp_crt;
			else
				om = inst_opt_disp_lcd;
		}

		if ((rv = p->set_opt_mode(p,om)) != inst_ok) {
			DBG((dbgo,"Setting display type failed failed with '%s' (%s)\n",
			       p->inst_interp_error(p, rv), p->interp_error(p, rv)))
			p->del(p);
			return -1;
		}
	} else if (cap & (inst_emis_disp_crt | inst_emis_disp_lcd | inst_emis_proj_crt | inst_emis_proj_lcd)) {
		printf("Either CRT or LCD must be selected\n");
		p->del(p);
		return -1;
	}

	/* Disable autocalibration of machine if selected */
	if (noautocal != 0) {
		if ((rv = p->set_opt_mode(p,inst_opt_noautocalib)) != inst_ok) {
			DBG((dbgo,"Setting no-autocalibrate failed failed with '%s' (%s)\n",
			       p->inst_interp_error(p, rv), p->interp_error(p, rv)))
			printf("Disable auto-calibrate not supported\n");
		}
	}

	/* Guess a calibration type */
	if (cap2 & inst2_cal_ref_white) {
		/* White or emmission mode calibration (ie. Spectrolino) */
		calt = inst_calt_ref_white;

	} else if (cap2 & inst2_cal_disp_offset) {
		/* Offset calibration */
		calt = inst_calt_disp_offset;
	} else if (cap2 & inst2_cal_disp_ratio) {
		/* Cell ratio calibration */
		calt = inst_calt_disp_ratio;
	} else if (cap2 & inst2_cal_disp_int_time) {
		/* Integration time calibration for display mode */
		calt = inst_calt_disp_int_time;

	} else if (cap2 & inst2_cal_proj_offset) {
		/* Offset calibration */
		calt = inst_calt_proj_offset;
	} else if (cap2 & inst2_cal_proj_ratio) {
		/* Cell ratio calibration */
		calt = inst_calt_proj_ratio;
	} else if (cap2 & inst2_cal_proj_int_time) {
		/* Integration time calibration for projector mode */
		calt = inst_calt_proj_int_time;

	} else if (cap2 & inst2_cal_crt_freq) {
		/* Frequency calibration for CRT */
		calt = inst_calt_crt_freq;
	}
	
	/* Do the calibration */
	rv = inst_handle_calibrate(p, calt, inst_calc_none, setup_display_calibrate, &dwi);
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
	if (verb) printf("Closing the instrument\n");

	return 0;
}

/* Take a series of readings from the display - implementation */
/* Return nz on fail/abort */
/* 1 = user aborted */
/* 2 = instrument access failed */
/* 3 = window access failed */ 
/* 4 = user hit terminate key */
/* 5 = system error */
/* Use disprd_err() to interpret it */
static int disprd_read_imp(
	disprd *p,
	col *cols,		/* Array of patch colors to be tested */
	int npat, 		/* Number of patches to be tested */
	int spat,		/* Start patch index for "verb", 0 if not used */
	int tpat,		/* Total patch index for "verb", 0 if not used */
	int acr,		/* If nz, do automatic final carriage return */
	int noinc,		/* If nz, don't increment the count */
	int tc			/* If nz, termination key */
) {
	int j, rv;
	int patch;
	ipatch val;		/* Return value */
	int ch;			/* Character */
	int cal_type;
	char id[CALIDLEN];

	/* Setup user termination character */
	p->it->icom->set_uih(p->it->icom, tc, tc, ICOM_TERM);

	/* See if we should do a frequency calibration or display intergaation time cal. first */
	if ((p->it->capabilities2(p->it) & inst2_cal_crt_freq) != 0
	 && p->it->needs_calibration(p->it) == inst_calt_crt_freq
	 && npat > 0
	 && (cols[0].r != 1.0 || cols[0].g != 1.0 || cols[0].b != 1.0)) {
		col tc;
		inst_cal_cond calc;

		if (p->proj)
			calc = inst_calc_proj_white;
		else
			calc = inst_calc_disp_white;

		if ((rv = p->dw->set_color(p->dw, 1.0, 1.0, 1.0)) != 0) {
			DBG((dbgo,"set_color() returned %s\n",rv))
			return 3;
		}
		/* Do calibrate, but ignore return code. Press on regardless. */
		if ((rv = p->it->calibrate(p->it, inst_calt_crt_freq, &calc, id)) != inst_ok) {
			DBG((dbgo,"warning, frequency calibrate failed with '%s' (%s)\n",
				      p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
		}
	}

	if (p->proj) {
		/* See if we should do a projector integration calibration first */
		if ((p->it->capabilities2(p->it) & inst2_cal_proj_int_time) != 0
		 && p->it->needs_calibration(p->it) == inst_calt_proj_int_time) {
			col tc;
			inst_cal_cond calc = inst_calc_proj_white;
	
			if ((rv = p->dw->set_color(p->dw, 1.0, 1.0, 1.0)) != 0) {
				DBG((dbgo,"set_color() returned %s\n",rv))
				return 3;
			}
			/* Do calibrate, but ignore return code. Press on regardless. */
			if ((rv = p->it->calibrate(p->it, inst_calt_proj_int_time, &calc, id)) != inst_ok) {
				DBG((dbgo,"warning, projector integration calibrate failed with '%s' (%s)\n",
					      p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
			}
		}
	} else {
		/* See if we should do a display integration calibration first */
		if ((p->it->capabilities2(p->it) & inst2_cal_disp_int_time) != 0
		 && p->it->needs_calibration(p->it) == inst_calt_disp_int_time) {
			col tc;
			inst_cal_cond calc = inst_calc_disp_white;
	
			if ((rv = p->dw->set_color(p->dw, 1.0, 1.0, 1.0)) != 0) {
				DBG((dbgo,"set_color() returned %s\n",rv))
				return 3;
			}
			/* Do calibrate, but ignore return code. Press on regardless. */
			if ((rv = p->it->calibrate(p->it, inst_calt_disp_int_time, &calc, id)) != inst_ok) {
				DBG((dbgo,"warning, display integration calibrate failed with '%s' (%s)\n",
					      p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
			}
		}
	}

	for (patch = 0; patch < npat; patch++) {
		col *scb = &cols[patch];
		double rgb[3];

		scb->XYZ_v = 0;		/* No readings are valid */
		scb->aXYZ_v = 0;
		scb->sp.spec_n = 0;
		scb->duration = 0.0;

		if (p->verb && spat != 0 && tpat != 0) {
			fprintf(p->df,"%cpatch %d of %d",cr_char,spat + (noinc != 0 ? 0 : patch),tpat);
			fflush(p->df);
		}
		DBG((dbgo,"About to read patch %d\n",patch))

		rgb[0] = scb->r;
		rgb[1] = scb->g;
		rgb[2] = scb->b;

		/* If we are doing a soft cal, apply it to the test color */
		if (p->softcal && p->cal[0][0] >= 0.0) {
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
			DBG((dbgo,"set_color() returned %s\n",rv))
			return 3;
		}

		/* Until we give up retrying */
		for (;;) {
			val.XYZ_v = 0;		/* No readings are valid */
			val.aXYZ_v = 0;
			val.sp.spec_n = 0;
			val.duration = 0.0;
	
			if ((rv = p->it->read_sample(p->it, scb->id, &val)) != inst_ok
			     && (rv & inst_mask) != inst_user_trig) {
				DBG((dbgo,"read_sample returned '%s' (%s)\n",
			       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))

				/* Deal with a user terminate */
				if ((rv & inst_mask) == inst_user_term) {
					return 4;

				/* Deal with a user abort */
				} else if ((rv & inst_mask) == inst_user_abort) {
					empty_con_chars();
					printf("\nSample read stopped at user request!\n");
					printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
					if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
						printf("\n");
						return 1;
					}
					printf("\n");
					continue;

				/* Deal with needing calibration */
				} else if ((rv & inst_mask) == inst_needs_cal) {
					disp_win_info dwi;
					dwi.dw = p->dw;		/* Set window to use */
					printf("\nSample read failed because instruments needs calibration\n");
					rv = inst_handle_calibrate(p->it, inst_calt_all, inst_calc_none,
					                                        setup_display_calibrate, &dwi);
					setup_display_calibrate(p->it, inst_calc_none, &dwi); 
					if (rv != inst_ok) {	/* Abort or fatal error */
						return 1;
					}
					continue;

				/* Deal with a bad sensor position */
				} else if ((rv & inst_mask) == inst_wrong_sensor_pos) {
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
						int tt = p->it->last_comerr(p->it);
						if (tt & (ICOM_BRK | ICOM_FER | ICOM_PER | ICOM_OER)) {
							if (p->br == baud_19200) p->br = baud_9600;
							else if (p->br == baud_9600) p->br = baud_4800;
							else if (p->br == baud_2400) p->br = baud_1200;
							else p->br = baud_1200;
						}
						if ((rv = p->it->init_coms(p->it, p->comport, p->br, p->fc, 15.0)) != inst_ok) {
							DBG((dbgo,"init_coms returned '%s' (%s)\n",
						       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
							return 2;
						}
					}
				continue;
				}
			} else {
				break;		/* Sucesful reading */
			}
		}
		/* We only fall through with a valid reading */
		scb->serno = p->serno++;
		scb->msec = msec_time();

		DBG((dbgo, "got reading abs. %f %f %f, transfering to col\n",
		                val.aXYZ[0], val.aXYZ[1], val.aXYZ[2]))

		/* Copy relative XYZ */
		if (val.XYZ_v != 0) {
			for (j = 0; j < 3; j++)
				scb->XYZ[j] = val.XYZ[j];
			scb->XYZ_v = 1;
		}

		/* Copy absolute XYZ */
		if (val.aXYZ_v != 0) {
			for (j = 0; j < 3; j++)
				scb->aXYZ[j] = val.aXYZ[j];
			scb->aXYZ_v = 1;
		}

		/* Copy spectral */
		if (p->spectral && val.sp.spec_n > 0) {
			scb->sp = val.sp;
		}
		DBG((dbgo,"on to next reading\n"))
	}
	/* Final return. */
	if (acr && p->verb && spat != 0 && tpat != 0 && (spat+patch-1) == tpat)
		fprintf(p->df,"\n");
	return 0;
}

/* A structure to hold info about drift samples */
typedef struct {
	col dcols[2];	/* Black and white readings */

	int off, count;	/* Offset and count into cols */

} dsamples;

/* Take a series of readings from the display - drift compensation */
/* Return nz on fail/abort */
/* 1 = user aborted */
/* 2 = instrument access failed */
/* 3 = window access failed */ 
/* 4 = user hit terminate key */
/* 5 = system error */
/* Use disprd_err() to interpret it */
static int disprd_read_drift(
	disprd *p,
	col *cols,		/* Array of patch colors to be tested */
	int npat, 		/* Number of patches to be tested */
	int spat,		/* Start patch index for "verb", 0 if not used */
	int tpat,		/* Total patch index for "verb", 0 if not used */
	int acr,		/* If nz, do automatic final carriage return */
	int tc			/* If nz, termination key */
) {
	int rv, i, j, e;
	double fper, foff;
	int off, poff;
	int boff, eoff, dno;	/* b&w offsets and count */

//printf("~1 DRIFT_EPERIOD %d, DRIFT_IPERIOD %d, npat %d\n",DRIFT_EPERIOD,DRIFT_IPERIOD,npat);

	/* Figure number and offset for b&w */
	if (p->bdrift == 0) {	/* Must be just wdrift */
		boff = eoff = 1;
		dno = 1;
	} else if (p->bdrift == 0) {	/* Must be just bdrift */
		boff = eoff = 0;
		dno = 1;
	} else {				/* Must be both */
		boff = eoff = 0;
		dno = 2;
	}

	/* If the last readings are invalid or too old, */
	/* or if we will use interpolation, read b&w */
#ifdef NEVER
	printf("last_bw_v = %d (%d)\n",p->last_bw_v,p->last_bw_v == 0);
	printf("npat = %d > %d, serno %d, last serno %d (%d)\n",npat, DRIFT_EPERIOD, p->serno,p->last_bw[eoff].serno,(npat > DRIFT_EPERIOD && p->serno > p->last_bw[eoff].serno));
	printf("serno - lastserno %d > %d (%d)\n",p->serno - p->last_bw[eoff].serno, DRIFT_EPERIOD,(p->serno - p->last_bw[eoff].serno) > DRIFT_EPERIOD);
	printf("msec %d - last %d = %d > 10000 (%d)\n",msec_time(),p->last_bw[boff].msec,msec_time() - p->last_bw[boff].msec,(msec_time() - p->last_bw[eoff].msec) > 10000);
#endif /* NEVER */
	if (p->last_bw_v == 0											/* There are none */
	 || (npat > DRIFT_EPERIOD && p->serno > p->last_bw[eoff].serno)	/* We will interpolate */
	 || (p->serno - p->last_bw[eoff].serno - dno) > DRIFT_EPERIOD	/* extrapolate would be too far */
	 || (msec_time() - p->last_bw[eoff].msec) > 10000) {	/* The current is too long ago */

//printf("~1 refreshing last bw\n");
		/* Read the black and/or white drift patch */
		p->last_bw[0].r = 
		p->last_bw[0].g = 
		p->last_bw[0].b = 0.0; 
		p->last_bw[1].r = 
		p->last_bw[1].g = 
		p->last_bw[1].b = 1.0; 
		if ((rv = disprd_read_imp(p, &p->last_bw[boff], dno, spat, tpat, 0, 1, tc)) != 0) {
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

	if (npat > DRIFT_EPERIOD) {
		int ndrift = 2;			/* Number of drift records */
		dsamples *dss;

		/* Figure out the number of drift samples we need */
		ndrift += (npat-1)/DRIFT_IPERIOD;
//printf("~1 spat %d, npat = %d, tpat %d, ndrift = %d\n",spat,npat,tpat,ndrift);

		if ((dss = (dsamples *)calloc(sizeof(dsamples), ndrift)) == NULL) {
			DBG((dbgo, "malloc of %d dsamples failed\n",ndrift))
			return 5;
		}
	
		/* Set up bookeeping */
		fper = (double)npat/(ndrift-1.0);
//printf("~1 fper = %f\n",fper);
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
//printf("~1 dss[%d] off = %d, count = %d\n",i, dss[i].off,dss[i].count);

			dss[i].dcols[0].r = 
			dss[i].dcols[0].g = 
			dss[i].dcols[0].b = 0.0; 
			dss[i].dcols[1].r = 
			dss[i].dcols[1].g = 
			dss[i].dcols[1].b = 1.0; 
		}

		for (i = 0; i < (ndrift-1); i++) {

			if (i == 0) {
				/* Use last b&w */
				dss[i].dcols[0] = p->last_bw[0];
				dss[i].dcols[1] = p->last_bw[1];
			} else {
				/* Read the black and/or white drift patch */
				if ((rv = disprd_read_imp(p, &dss[i].dcols[boff], dno, spat+dss[i].off, tpat, 0, 1, tc)) != 0) {
					free(dss);
					return rv;
				}
			}
			/* Read this batch of patches */
			if ((rv = disprd_read_imp(p, &cols[dss[i].off], dss[i].count,spat+dss[i].off,tpat,0,0,tc)) != 0) {
				free(dss);
				return rv;
			}
		}
		/* Read the last black and/or white drift patch */
		if ((rv = disprd_read_imp(p, &dss[i].dcols[boff], dno, spat+dss[i].off-1, tpat, 0, 1, tc)) != 0) {
			free(dss);
			return rv;
		}
		/* Remember the last */
		p->last_bw[0] = dss[i].dcols[0];
		p->last_bw[1] = dss[i].dcols[1];
		p->last_bw_v = 1;

		/* Set the white drift reference to be the last one */
		p->targ_w = p->last_bw[1];
		p->targ_w_v = 1;

//printf("~1 ref b = %f %f %f\n", p->ref_bw[0].aXYZ[0], p->ref_bw[0].aXYZ[1], p->ref_bw[0].aXYZ[2]);
//printf("~1 ref w = %f %f %f\n", p->ref_bw[1].aXYZ[0], p->ref_bw[1].aXYZ[1], p->ref_bw[1].aXYZ[2]);
//printf("~1 ndrift-1 b = %f %f %f\n", dss[ndrift-1].dcols[0].aXYZ[0], dss[ndrift-1].dcols[0].aXYZ[1], dss[ndrift-1].dcols[0].aXYZ[2]);
//printf("~1 ndrift-1 w = %f %f %f\n", dss[ndrift-1].dcols[1].aXYZ[0], dss[ndrift-1].dcols[1].aXYZ[1], dss[ndrift-1].dcols[1].aXYZ[2]);

		/* Apply the drift compensation using interpolation */
		for (i = 0; i < (ndrift-1); i++) {

			for (j = 0; j < dss[i].count; j++) {
				int k = dss[i].off + j;
				double we;		/* Interpolation weight of eairlier value */
				col bb, ww;		/* Interpolated black and white */
//double uXYZ[3];
//icmCpy3(uXYZ, cols[k].aXYZ);
//printf("~1 patch %d = %f %f %f\n", k, cols[k].aXYZ[0], cols[k].aXYZ[1], cols[k].aXYZ[2]);
				if (p->bdrift) {
					we = (double)(dss[i+1].dcols[0].msec - cols[k].msec)/
					     (double)(dss[i+1].dcols[0].msec - dss[i].dcols[0].msec);

					if (cols[k].XYZ_v) {
						for (e = 0; e < 3; e++) {
							bb.XYZ[e] =        we  * dss[i].dcols[0].XYZ[e]
							          + (1.0 - we) * dss[i+1].dcols[0].XYZ[e];
						}
					}
					if (cols[k].aXYZ_v) {
						for (e = 0; e < 3; e++) {
							bb.aXYZ[e] =        we  * dss[i].dcols[0].aXYZ[e]
							          + (1.0 - we) * dss[i+1].dcols[0].aXYZ[e];
						}
//printf("~1 bb = %f %f %f\n", bb.aXYZ[0], bb.aXYZ[1], bb.aXYZ[2]);
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
					}
					if (cols[k].aXYZ_v) {
						for (e = 0; e < 3; e++) {
							ww.aXYZ[e] =        we  * dss[i].dcols[1].aXYZ[e]
							          + (1.0 - we) * dss[i+1].dcols[1].aXYZ[e];
						}
//printf("~1 ww = %f %f %f\n", ww.aXYZ[0], ww.aXYZ[1], ww.aXYZ[2]);
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
						}
						if (cols[k].aXYZ_v) {
							for (e = 0; e < 3; e++) {
								bb.aXYZ[e] *= p->ref_bw[1].aXYZ[e]/ww.aXYZ[e];
							}
//printf("~1 wcomp bb = %f %f %f\n", bb.aXYZ[0], bb.aXYZ[1], bb.aXYZ[2]);
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
					}
					if (cols[k].aXYZ_v) {
						for (e = 0; e < 3; e++) {
							cols[k].aXYZ[e] += p->ref_bw[0].aXYZ[e] - bb.aXYZ[e];
						}
//printf("~1 bcomp patch = %f %f %f\n",  cols[k].aXYZ[0], cols[k].aXYZ[1], cols[k].aXYZ[2]);
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
					}
					if (cols[k].aXYZ_v) {
						for (e = 0; e < 3; e++) {
							cols[k].aXYZ[e] *= p->targ_w.aXYZ[e]/ww.aXYZ[e];
						}
//printf("~1 wcomp patch = %f %f %f\n",  cols[k].aXYZ[0], cols[k].aXYZ[1], cols[k].aXYZ[2]);
					}
					if (cols[k].sp.spec_n > 0) {
						for (e = 0; e < cols[k].sp.spec_n; e++) {
							cols[k].sp.spec[e] *= p->targ_w.sp.spec[e]/ww.sp.spec[e];
						}
					}
				}
//printf("~1 %d: drift change %f DE\n",k,icmXYZLabDE(&icmD50, uXYZ, cols[k].aXYZ));
			}
		}
		free(dss);

	/* Else use extrapolation from the last b&w */
	} else {

//printf("~1 doing small number of readings\n");
		/* Read the small number of patches */
		if ((rv = disprd_read_imp(p, cols,npat,spat,tpat,acr,0,tc)) != 0)
			return rv;

		if (p->targ_w_v == 0) {
			/* Set the white drift reference to be the last one */
			p->targ_w = p->last_bw[1];
			p->targ_w_v = 1;
//printf("~1 set white drift target\n");
		}

//printf("~1 ref b = %f %f %f\n", p->ref_bw[0].aXYZ[0], p->ref_bw[0].aXYZ[1], p->ref_bw[0].aXYZ[2]);
//printf("~1 ref w = %f %f %f\n", p->ref_bw[1].aXYZ[0], p->ref_bw[1].aXYZ[1], p->ref_bw[1].aXYZ[2]);
//printf("~1 last b = %f %f %f\n", p->last_bw[0].aXYZ[0], p->last_bw[0].aXYZ[1], p->last_bw[0].aXYZ[2]);
//printf("~1 last w = %f %f %f\n", p->last_bw[1].aXYZ[0], p->last_bw[1].aXYZ[1], p->last_bw[1].aXYZ[2]);
//printf("~1 target w = %f %f %f\n", p->targ_w.aXYZ[0], p->targ_w.aXYZ[1], p->targ_w.aXYZ[2]);
		/* Apply the drift compensation using extrapolation */
		/* Note that white compensation can't work in this case */
		for (j = 0; j < npat; j++) {
			double we;		/* Interpolation weight of eairlier value */
			col bb, ww;		/* Interpolated black and white */

//printf("~1 patch %d = %f %f %f\n", j, cols[j].aXYZ[0], cols[j].aXYZ[1], cols[j].aXYZ[2]);
//double uXYZ[3];
//icmCpy3(uXYZ, cols[j].aXYZ);

			if (p->bdrift) {
				bb = p->last_bw[0];
//printf("~1 bb = %f %f %f\n", bb.aXYZ[0], bb.aXYZ[1], bb.aXYZ[2]);
			}
			if (p->wdrift) {
				ww = p->last_bw[1];
//printf("~1 ww = %f %f %f\n", ww.aXYZ[0], ww.aXYZ[1], ww.aXYZ[2]);
			}

			if (p->bdrift) {
				/* Compensate interpolated black for any white change from ref. */
				if (p->wdrift) {
					if (cols[j].XYZ_v) {
						for (e = 0; e < 3; e++) {
							bb.XYZ[e] *= p->ref_bw[1].XYZ[e]/ww.XYZ[e];
						}
					}
					if (cols[j].aXYZ_v) {
						for (e = 0; e < 3; e++) {
							bb.aXYZ[e] *= p->ref_bw[1].aXYZ[e]/ww.aXYZ[e];
						}
//printf("~1 wcomp bb = %f %f %f\n", bb.aXYZ[0], bb.aXYZ[1], bb.aXYZ[2]);
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
				}
				if (cols[j].aXYZ_v) {
					for (e = 0; e < 3; e++) {
						cols[j].aXYZ[e] += p->ref_bw[0].aXYZ[e] - bb.aXYZ[e];
					}
//printf("~1 bcomp patch = %f %f %f\n",  cols[j].aXYZ[0], cols[j].aXYZ[1], cols[j].aXYZ[2]);
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
				}
				if (cols[j].aXYZ_v) {
					for (e = 0; e < 3; e++) {
						cols[j].aXYZ[e] *= p->targ_w.aXYZ[e]/ww.aXYZ[e];
					}
//printf("~1 wcomp patch = %f %f %f\n",  cols[j].aXYZ[0], cols[j].aXYZ[1], cols[j].aXYZ[2]);
				}
				if (cols[j].sp.spec_n > 0) {
					for (e = 0; e < cols[j].sp.spec_n; e++) {
						cols[j].sp.spec[e] *= p->targ_w.sp.spec[e]/ww.sp.spec[e];
					}
				}
			}
//printf("~1 %d: drift change %f DE\n",j,icmXYZLabDE(&icmD50, uXYZ, cols[j].aXYZ));
		}
	}

	if (acr && p->verb && spat != 0 && tpat != 0 && (spat+npat-1) == tpat)
		fprintf(p->df,"\n");

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
/* Return nz on fail/abort */
/* 1 = user aborted */
/* 2 = instrument access failed */
/* 3 = window access failed */ 
/* 4 = user hit terminate key */
/* 5 = system error */
/* Use disprd_err() to interpret it */
static int disprd_read(
	disprd *p,
	col *cols,		/* Array of patch colors to be tested */
	int npat, 		/* Number of patches to be tested */
	int spat,		/* Start patch index for "verb", 0 if not used */
	int tpat,		/* Total patch index for "verb", 0 if not used */
	int acr,		/* If nz, do automatic final carriage return */
	int tc			/* If nz, termination key */
) {
	int rv, i;

	if (p->bdrift || p->wdrift) {
		if ((rv = disprd_read_drift(p, cols,npat,spat,tpat,acr,tc)) != 0)
			return rv;
	} else {
		if ((rv = disprd_read_imp(p, cols,npat,spat,tpat,acr,0,tc)) != 0)
			return rv;
	}

	/* Convert spectral to XYZ. */
	/* Since this is a display, assume that this is absolute spectral */
	if (p->sp2cie != NULL) {
		for (i = 0; i < npat; i++) {
			if (cols[i].sp.spec_n > 0) {
				p->sp2cie->convert(p->sp2cie, cols[i].aXYZ, &cols[i].sp);
				cols[i].aXYZ_v = 1;
			}
		}
	}

	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static int config_inst_displ(disprd *p);

/* Take an ambient reading if the instrument has the capability. */
/* return nz on fail/abort */
/* 1 = user aborted */
/* 2 = instrument access failed */
/* 4 = user hit terminate key */
/* 5 = system error */
/* 8 = no ambient capability */ 
/* Use disprd_err() to interpret it */
int disprd_ambient(struct _disprd *p,
	double *ambient,		/* return ambient in cd/m^2 */
	int tc					/* If nz, termination key */
) {
	inst_capability cap = 0;
	inst2_capability cap2 = 0;
	inst_opt_mode trigmode = inst_opt_unknown;  /* Chosen trigger mode */
	int uswitch = 0;                /* Instrument switch is enabled */
	ipatch val;
	int verb = p->verb;
	int rv;

	if (p->it != NULL) { /* Not fake */
		cap = p->it->capabilities(p->it);
		cap2 = p->it->capabilities2(p->it);
	}
	
	if ((cap & inst_emis_ambient) == 0) {
		printf("Need ambient measurement capability,\n");
		printf("but instrument doesn't support it\n");
		return 8;
	}

	printf("\nPlease make sure the instrument is fitted with\n");
	printf("the appropriate ambient light measuring head.\n");
	
	if ((rv = p->it->set_mode(p->it, inst_mode_emis_ambient)) != inst_ok) {
		DBG((dbgo,"set_mode returned '%s' (%s)\n",
		       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
		return 2;
	}
	if (p->it != NULL) { /* Not fake */
		cap  = p->it->capabilities(p->it);
		cap2 = p->it->capabilities2(p->it);
	}
	
	/* Select a reasonable trigger mode */
	if (cap2 & inst2_keyb_switch_trig) {
		trigmode = inst_opt_trig_keyb_switch;
		uswitch = 1;

	/* Or go for keyboard trigger */
	} else if (cap2 & inst2_keyb_trig) {
		trigmode = inst_opt_trig_keyb;

	/* Or something is wrong with instrument capabilities */
	} else {
		printf("No reasonable trigger mode avilable for this instrument\n");
		return 2;
	}

	if ((rv = p->it->set_opt_mode(p->it, trigmode)) != inst_ok) {
		printf("\nSetting trigger mode failed with error :'%s' (%s)\n",
       	       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
		return 2;
	}

	/* Prompt on trigger */
	if ((rv = p->it->set_opt_mode(p->it, inst_opt_trig_return)) != inst_ok) {
		printf("Setting trigger mode failed with error :'%s' (%s)\n",
       	       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
		return 2;
	}

	/* Setup the keyboard trigger to return our commands */
	p->it->icom->reset_uih(p->it->icom);
	p->it->icom->set_uih(p->it->icom, 0x0, 0xff, ICOM_TRIG);
	p->it->icom->set_uih(p->it->icom, 'q', 'q', ICOM_USER);
	p->it->icom->set_uih(p->it->icom, 'Q', 'Q', ICOM_USER);
	p->it->icom->set_uih(p->it->icom, 0x03, 0x03, ICOM_USER);		/* ^c */
	p->it->icom->set_uih(p->it->icom, 0x1b, 0x1b, ICOM_USER);		/* Esc */

	/* Setup user termination character */
	p->it->icom->set_uih(p->it->icom, tc, tc, ICOM_TERM);

	/* Until we give up retrying */
	for (;;) {
		char ch;
		val.XYZ_v = 0;		/* No readings are valid */
		val.aXYZ_v = 0;
		val.sp.spec_n = 0;
		val.duration = 0.0;

		printf("\nPlace the instrument so as to measure ambient upwards, beside the display,\n");
		if (uswitch)
			printf("Hit ESC or Q to exit, instrument switch or any other key to take a reading: ");
		else
			printf("Hit ESC or Q to exit, any other key to take a reading: ");
		fflush(stdout);

		if ((rv = p->it->read_sample(p->it, "AMBIENT", &val)) != inst_ok
		     && (rv & inst_mask) != inst_user_trig) {
			DBG((dbgo,"read_sample returned '%s' (%s)\n",
		       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))

			/* Deal with a user terminate */
			if ((rv & inst_mask) == inst_user_term) {
				return 4;

			/* Deal with a user abort */
			} else if ((rv & inst_mask) == inst_user_abort) {
				empty_con_chars();
				printf("\nMeasure stopped at user request!\n");
				printf("Hit Esc or Q to give up, any other key to retry:"); fflush(stdout);
				if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
					printf("\n");
					return 1;
				}
				printf("\n");
				continue;

			/* Deal with needs calibration */
			} else if ((rv & inst_mask) == inst_needs_cal) {
				disp_win_info dwi;
				dwi.dw = p->dw;		/* Set window to use */
				printf("\nSample read failed because instruments needs calibration\n");
				rv = inst_handle_calibrate(p->it, inst_calt_all, inst_calc_none,
				                                        setup_display_calibrate, &dwi);
				setup_display_calibrate(p->it,inst_calc_none, &dwi); 
				if (rv != inst_ok) {	/* Abort or fatal error */
					return 1;
				}
				continue;

			/* Deal with a bad sensor position */
			} else if ((rv & inst_mask) == inst_wrong_sensor_pos) {
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
					int tt = p->it->last_comerr(p->it);
					if (tt & (ICOM_BRK | ICOM_FER | ICOM_PER | ICOM_OER)) {
						if (p->br == baud_19200) p->br = baud_9600;
						else if (p->br == baud_9600) p->br = baud_4800;
						else if (p->br == baud_2400) p->br = baud_1200;
						else p->br = baud_1200;
					}
					if ((rv = p->it->init_coms(p->it, p->comport, p->br, p->fc, 15.0)) != inst_ok) {
						DBG((dbgo,"init_coms returned '%s' (%s)\n",
					       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
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
		icmCpy3(val.aXYZ, val.XYZ);
		val.aXYZ_v = 1;
	}

	if (val.aXYZ_v == 0) {
		printf("Unexpected failure to get measurement\n");
		return 2;
	}

	DBG((dbgo,"Measured ambient of %f\n",val.aXYZ[1]));
	if (ambient != NULL)
		*ambient = val.aXYZ[1];
	
	/* Configure the instrument mode back to reading the display */
	if ((rv = config_inst_displ(p)) != 0)
		return rv;

	printf("\nPlace the instrument back on the test window\n");
	fflush(stdout);

	return 0;
}


/* Test without a spectrometer using a fake device */
/* Return nz on error */
static int disprd_fake_read(disprd *p,
	col *cols,		/* Array of patch colors to be tested */
	int npat, 		/* Number of patches to be tested */
	int spat,		/* Start patch index for "verb", 0 if not used */
	int tpat,		/* Total patch index for "verb", 0 if not used */
	int acr,		/* If nz, do automatic final carriage return */
	int tc			/* If nz, termination key */
) {
	icmXYZNumber white;		/* White point */
	icmXYZNumber red;		/* Red colorant */
	icmXYZNumber green;		/* Green colorant */
	icmXYZNumber blue;		/* Blue colorant */
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

	if (ttpat < npat)
		ttpat = npat;

	/* Setup fake device */
	white.X = br * 0.955;		/* Somewhere between D50 and D65 */
	white.Y = br * 1.00;
	white.Z = br * 0.97;
	red.X = br * 0.41;
	red.Y = br * 0.21;
	red.Z = br * 0.02;
	green.X = br * 0.30;
	green.Y = br * 0.55;
	green.Z = br * 0.15;
	blue.X = br * 0.15;
	blue.Y = br * 0.10;
	blue.Z = br * 0.97;
	/* Input offset, equivalent to RGB offsets having various values */
	doff[0] = 0.10;
	doff[1] = 0.06;
	doff[2] = 0.08;
	/* Output offset - equivalent to flare [range 0.0 - 1.0] */
	ooff[0] = 0.03;
	ooff[1] = 0.04;
	ooff[2] = 0.09;

	if (icmRGBprim2matrix(white, red, green, blue, mat))
		error("Fake read unexpectedly got singular matrix\n");

	icmSetUnity3x3(xmat);

	if (p->fake2 == 1) {
		/* Target matrix */
		xmat[0][0] = 1.0; xmat[0][1] = 0.01; xmat[0][2] = 0.02;
		xmat[1][0] = 0.1; xmat[1][1] = 1.11; xmat[1][2] = 0.12;
		xmat[2][0] = 0.2; xmat[2][1] = 0.2;  xmat[2][2] = 1.22;
	}

	for (patch = 0; patch < npat; patch++) {
		if (p->verb && spat != 0 && tpat != 0) {
			fprintf(p->df,"%cpatch %d of %d",cr_char,spat+patch,tpat);
			fflush(p->df);
		}
		crgb[0] = rgb[0] = cols[patch].r;
		crgb[1] = rgb[1] = cols[patch].g;
		crgb[2] = rgb[2] = cols[patch].b;

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
		}

		/* If we have a test window, display the patch color */
		if (p->dw) {
			inst_code rv;
			if (p->softcal) {	/* Display value with calibration */
				if ((rv = p->dw->set_color(p->dw, crgb[0], crgb[1], crgb[2])) != 0) {
					DBG((dbgo,"set_color() returned %s\n",rv))
					return 3;
				}
			} else {			/* Hardware will apply calibration */
				if ((rv = p->dw->set_color(p->dw, rgb[0], rgb[1], rgb[2])) != 0) {
					DBG((dbgo,"set_color() returned %s\n",rv))
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
		icmMulBy3x3(cols[patch].aXYZ, mat, crgb);

		/* Apply XYZ offset */
		for (j = 0; j < 3; j++)
			cols[patch].aXYZ[j] += ooff[j];

		/* Apply extra matrix */
		icmMulBy3x3(cols[patch].aXYZ, xmat, cols[patch].aXYZ);

#ifdef FAKE_NOISE
		if (p->fake2 == 0) {
			cols[patch].aXYZ[0] += 2.0 * FAKE_NOISE * d_rand(-1.0, 1.0);
			cols[patch].aXYZ[1] += FAKE_NOISE * d_rand(-1.0, 1.0);
			cols[patch].aXYZ[2] += 4.0 * FAKE_NOISE * d_rand(-1.0, 1.0);
			for (j = 0; j < 3; j++) { 
				if (cols[patch].aXYZ[j] < 0.0)
					cols[patch].aXYZ[j] = 0.0;
			}
		}
#endif
		cols[patch].aXYZ_v = 1;
	}
	if (acr && p->verb && spat != 0 && tpat != 0 && (spat+patch-1) == tpat)
		fprintf(p->df,"\n");
	return 0;
}

/* Test without a spectrometer using a fake ICC profile device */
static int disprd_fake_read_lu(disprd *p,
	col *cols,		/* Array of patch colors to be tested */
	int npat, 		/* Number of patches to be tested */
	int spat,		/* Start patch index for "verb", 0 if not used */
	int tpat,		/* Total patch index for "verb", 0 if not used */
	int acr,		/* If nz, do automatic final carriage return */
	int tc			/* If nz, termination key */
) {
	int patch, j;
	int ttpat = tpat;
	double br = 120.4;		/* Overall brightness */

	if (ttpat < npat)
		ttpat = npat;

	for (patch = 0; patch < npat; patch++) {
		double rgb[3];;
		if (p->verb && spat != 0 && tpat != 0) {
			fprintf(p->df,"%cpatch %d of %d",cr_char,spat+patch,tpat);
			fflush(p->df);
		}
		rgb[0] = cols[patch].r;
		rgb[1] = cols[patch].g;
		rgb[2] = cols[patch].b;

		/* If we have a test window, display the patch color */
		if (p->dw) {
			inst_code rv;
			if ((rv = p->dw->set_color(p->dw, rgb[0], rgb[1], rgb[2])) != 0) {
				DBG((dbgo,"set_color() returned %s\n",rv))
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

		p->fake_lu->lookup(p->fake_lu, cols[patch].aXYZ, rgb); 
		for (j = 0; j < 3; j++) 
			cols[patch].aXYZ[j] *= br;
#ifdef FAKE_NOISE
		cols[patch].aXYZ[0] += 2.0 * FAKE_NOISE * d_rand(-1.0, 1.0);
		cols[patch].aXYZ[1] += FAKE_NOISE * d_rand(-1.0, 1.0);
		cols[patch].aXYZ[2] += 4.0 * FAKE_NOISE * d_rand(-1.0, 1.0);
		for (j = 0; j < 3; j++) { 
			if (cols[patch].aXYZ[j] < 0.0)
				cols[patch].aXYZ[j] = 0.0;
		}
#endif
		cols[patch].aXYZ_v = 1;
	}
	if (acr && p->verb && spat != 0 && tpat != 0 && (spat+patch-1) == tpat)
		fprintf(p->df,"\n");
	return 0;
}

/* Use without a direct connect spectrometer using shell callout */
static int disprd_fake_read_co(disprd *p,
	col *cols,		/* Array of patch colors to be tested */
	int npat, 		/* Number of patches to be tested */
	int spat,		/* Start patch index for "verb", 0 if not used */
	int tpat,		/* Total patch index for "verb", 0 if not used */
	int acr,		/* If nz, do automatic final carriage return */
	int tc			/* If nz, termination key */
) {
	int patch, j;
	int ttpat = tpat;

	if (ttpat < npat)
		ttpat = npat;

	for (patch = 0; patch < npat; patch++) {
		double rgb[3];
		int rv;
		char *cmd;
		FILE *fp;

		if (p->verb && spat != 0 && tpat != 0) {
			fprintf(p->df,"%cpatch %d of %d",cr_char,spat+patch,tpat);
			fflush(p->df);
		}
		rgb[0] = cols[patch].r;
		rgb[1] = cols[patch].g;
		rgb[2] = cols[patch].b;

		/* If we have a test window, display the patch color */
		if (p->dw) {
			inst_code rv;
			if ((rv = p->dw->set_color(p->dw, rgb[0], rgb[1], rgb[2])) != 0) {
				DBG((dbgo,"set_color() returned %s\n",rv))
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

		if (fscanf(fp, " %lf %lf %lf", &cols[patch].aXYZ[0], &cols[patch].aXYZ[1],
		                               &cols[patch].aXYZ[2]) != 3)
			error("Unable to parse measurement value file '%s'",cmd);
		fclose(fp);
		free(cmd);

		if (p->verb > 1)
			printf("Read XYZ %f %f %f from '%s'\n", cols[patch].aXYZ[0],
			            cols[patch].aXYZ[1],cols[patch].aXYZ[2], cmd);

		cols[patch].aXYZ_v = 1;
	}
	if (acr && p->verb && spat != 0 && tpat != 0 && (spat+patch-1) == tpat)
		fprintf(p->df,"\n");
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
			return "Instrument Access Failed (No PLD Pattern - have you run spyd2en ?)";
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
	}
	return "Unknown";
}

static void disprd_del(disprd *p) {

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
		if (p->or != NULL)
			p->dw->set_ramdac(p->dw,p->or, 0);
		p->dw->del(p->dw);
	}
	if (p->sp2cie != NULL)
		p->sp2cie->del(p->sp2cie); 
	free(p);
}

/* Helper to configure the instrument mode ready */
/* for reading the display */
/* return new_disprd() error code */
static int config_inst_displ(disprd *p) {
	inst_capability cap;
	inst2_capability cap2;
	inst_mode mode = 0;
	int verb = p->verb;
	int rv;
	
	cap = p->it->capabilities(p->it);
	cap2 = p->it->capabilities2(p->it);
	
	if (p->proj && (cap & inst_emis_proj) == 0) {
		printf("Want projection measurement capability but instrument doesn't support it\n");
		printf("so falling back to display mode.\n");
		DBG((dbgo,"No projection mode so falling back to display mode.\n"));
		p->proj = 0;
	}
	
	if (( p->proj && (cap & inst_emis_proj) == 0)
	 || (!p->proj && p->adaptive && (cap & inst_emis_spot) == 0)
	 || (!p->proj && !p->adaptive && (cap & inst_emis_disp) == 0)) {
		printf("Need %s measurement capability,\n",
		       p->proj ? "projection" : p->adaptive ? "emission" : "display");
		printf("but instrument doesn't support it\n");
		DBG((dbgo,"Need %s measurement capability but device doesn't support it,\n",
		       p->proj ? "projection" : p->adaptive ? "emission" : "display"));
		return 2;
	}
	
	if (p->obType != icxOT_none
	 && p->obType != icxOT_default) {
		if ((cap & inst_spectral) == 0 && (cap & inst_ccss) == 0) {
			printf("A non-standard observer was requested,\n");
			printf("but instrument doesn't support spectral or CCSS\n");
			DBG((dbgo,"A non-standard observer was requested,\n"
			          "but instrument doesn't support spectral or CCSS\n"))
			return 2;
		}
		/* Make sure spectral is turned on if an observer is requested */
		if (!p->spectral && (cap & inst_ccss) == 0)
			p->spectral = 1;
	}

	if (p->spectral && (cap & inst_spectral) == 0) {
		if (p->spectral != 2) {		/* Not soft */
			printf("Spectral information was requested,\n");
			printf("but instrument doesn't support it\n");
			DBG((dbgo,"Spectral information was requested,\nbut instrument doesn't support it\n"))
			return 2;
		}
		p->spectral = 0;
	}
	
	if (p->proj) {
		mode = inst_mode_emis_proj;
	} else {
		if (p->adaptive)
			mode = inst_mode_emis_spot;
		else
			mode = inst_mode_emis_disp;
	}
	
	if (p->spectral) {
		mode |= inst_mode_spectral;
		p->spectral = 1;
	} else {
		p->spectral = 0;
	}
	
	/* Set CRT or LCD mode */
	if ((cap & (inst_emis_disp_crt | inst_emis_disp_lcd | inst_emis_proj_crt | inst_emis_proj_lcd))
	 && (p->dtype == 1 || p->dtype == 2)) {
		inst_opt_mode om;
	
		if (p->proj) {
			if (p->dtype == 1)
				om = inst_opt_proj_crt;
			else
				om = inst_opt_proj_lcd;
		} else {
			if (p->dtype == 1)
				om = inst_opt_disp_crt;
			else
				om = inst_opt_disp_lcd;
		}
		
		if ((rv = p->it->set_opt_mode(p->it, om)) != inst_ok) {
			DBG((dbgo,"Setting display type failed failed with '%s' (%s)\n",
		       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
			return 2;
		}
	} else if (cap & (inst_emis_disp_crt | inst_emis_disp_lcd | inst_emis_proj_crt | inst_emis_proj_lcd)) {
		printf("Either CRT or LCD must be selected\n");
		return 7;
	}
	
	/* Disable autocalibration of machine if selected */
	if (p->noautocal != 0) {
		if ((rv = p->it->set_opt_mode(p->it,inst_opt_noautocalib)) != inst_ok) {
			DBG((dbgo,"Setting no-autocalibrate failed failed with '%s' (%s)\n",
		       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
			printf("Disable auto-calibrate not supported\n");
		}
	}
	
	if (p->highres) {
		if (cap & inst_highres) {
			inst_code ev;
			if ((ev = p->it->set_opt_mode(p->it, inst_opt_highres)) != inst_ok) {
				DBG((dbgo,"\nSetting high res mode failed with error :'%s' (%s)\n",
		       	       p->it->inst_interp_error(p->it, ev), p->it->interp_error(p->it, ev)))
				return 2;
			}
		} else if (p->verb) {
			printf("high resolution ignored - instrument doesn't support high res. mode\n");
		}
	}
	if ((rv = p->it->set_mode(p->it, mode)) != inst_ok) {
		DBG((dbgo,"set_mode returned '%s' (%s)\n",
		       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
		return 2;
	}
	cap  = p->it->capabilities(p->it);
	cap2 = p->it->capabilities2(p->it);

	if (p->ccmtx != NULL) {
		if ((cap & inst_ccmx) == 0) {
			DBG((dbgo,"Instrument doesn't support ccmx correction\n"))
			return 10;
		}
		if ((rv = p->it->col_cor_mat(p->it, p->ccmtx)) != inst_ok) {
			DBG((dbgo,"col_cor_mat returned '%s' (%s)\n",
			       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
			return 2;
		}
	}
	
	if (p->sets != NULL
	 || ((cap & inst_ccss) != 0 && p->obType != icxOT_default)) {

		if ((cap & inst_ccss) == 0) {
			DBG((dbgo,"Instrument doesn't support ccss calibration\n"))
			return 11;
		}
		if ((rv = p->it->col_cal_spec_set(p->it, p->obType, p->custObserver,
		                                         p->sets, p->no_sets)) != inst_ok) {
			DBG((dbgo,"col_cal_spec_set returned '%s' (%s)\n",
			       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
			return 2;
		}
	}
	
	/* Set the trigger mode to program triggered */
	if ((rv = p->it->set_opt_mode(p->it,inst_opt_trig_prog)) != inst_ok) {
		DBG((dbgo,"Setting program trigger mode failed failed with '%s' (%s)\n",
	       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
		return 2;
	}

	/* No prompt on trigger */
	if ((rv = p->it->set_opt_mode(p->it, inst_opt_trig_no_return)) != inst_ok) {
		DBG((dbgo,"\nSetting trigger mode failed with error :'%s' (%s)\n",
	       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
		return 2;
	}

	/* Reset key meanings */
	p->it->icom->reset_uih(p->it->icom);

	DBG((dbgo,"config_inst_displ suceeded\n"));
	return 0;
}

/* Create a display reading object. */
/* Return NULL if error */
/* Set *errc to code: */
/* 0 = no error */
/* 1 = user aborted */
/* 2 = instrument access failed */
/* 22 = instrument access failed  - no PLD pattern */
/* 3 = window access failed */ 
/* 4 = RAMDAC access failed */ 
/* 5 = user hit terminate key */
/* 6 = system error */
/* 7 = CRT or LCD must be selected */
/* 9 = spectral conversion failed */
/* 10 = no ccmx support */
/* 11 = no ccss support */
/* Use disprd_err() to interpret *errc */
disprd *new_disprd(
int *errc,          /* Error code. May be NULL */
instType itype,		/* Nominal instrument type (usually instUnknown) */
int comport, 		/* COM port used. -99 == fake Display */
flow_control fc,	/* Flow control */
int dtype,			/* Display type, 0 = unknown, 1 = CRT, 2 = LCD */
int proj,			/* NZ for projector mode. Falls back to display mode */
int adaptive,		/* NZ for adaptive mode */
int noautocal,			/* No automatic instrument calibration */
int highres,		/* Use high res mode if available */
int native,			/* 0 = use current current or given calibration curve */
					/* 1 = set native linear output and use ramdac high prec'n */
					/* 2 = set native linear output */
double cal[3][MAX_CAL_ENT],	/* Calibration set/return (cal[0][0] < 0.0 or NULL if not used) */
int ncal,			/* Number of cal[] entries */
int softcal,		/* NZ if apply cal to readings rather than hardware */
disppath *disp,		/* Display to calibrate. NULL if fake and no dispwin */
int blackbg,		/* NZ if whole screen should be filled with black */
int override,		/* Override_redirect on X11 */
char *ccallout,		/* Shell callout on set color */
char *mcallout,		/* Shell callout on measure color (forced fake) */
double patsize,		/* Size of dispwin */
double ho,			/* Horizontal offset */
double vo,			/* Vertical offset */
double ccmtx[3][3],	/* Colorimeter Correction matrix, NULL if none */
xspect *sets,		/* CCSS Set of sample spectra, NULL if none  */
int no_sets,	 	/* CCSS Number on set, 0 if none */
int spectral,		/* Generate spectral info flag */
icxObserverType obType,	/* Use alternate observer if spectral or CCSS and != icxOT_none */
xspect custObserver[3],	/* Optional custom observer */
int bdrift,			/* Flag, nz for black drift compensation */
int wdrift,			/* Flag, nz for white drift compensation */
int verb,			/* Verbosity flag */
FILE *df,			/* Verbose output - NULL = stdout */
int debug,			/* Debug flag */
char *fake_name		/* Name of profile to use as a fake device */
) {
	disprd *p = NULL;
	int ch;
	inst_code rv;

	if (errc != NULL) *errc = 0;		/* default return code = no error */

	/* Allocate a disprd */
	if ((p = (disprd *)calloc(sizeof(disprd), 1)) == NULL) {
		if (debug) fprintf(stderr,"new_disprd failed due to malloc failure\n");
		if (errc != NULL) *errc = 6;
		return NULL;
	}
	p->del = disprd_del;
	p->read = disprd_read;
	p->reset_targ_w = disprd_reset_targ_w;
	p->change_drift_comp = disprd_change_drift_comp;
	p->ambient = disprd_ambient;
	p->fake_name = fake_name;

	p->verb = verb;
	p->debug = debug;
	p->itype = itype;
	p->ccmtx = ccmtx;
	p->sets = sets;
	p->no_sets = no_sets;		/* CCSS */
	p->spectral = spectral;
	p->obType = obType;			/* CCSS or spectral */
	p->custObserver = custObserver;
	p->bdrift = bdrift;
	p->wdrift = wdrift;
	p->dtype = dtype;
	p->proj = proj;
	p->adaptive = adaptive;
	p->noautocal = noautocal;
	p->highres = highres;
	if (df)
		p->df = df;
	else
		p->df = stdout;
	if (mcallout != NULL)
		comport = -99;			/* Force fake device */
	p->mcallout = mcallout;
	p->comport = comport;
	p->br = baud_19200;
	p->fc = fc;

	/* Save this in case we are using a fake device */
	if (cal != NULL && cal[0][0] >= 0.0) {
		int j, i;
		for (j = 0; j < 3; j++) {
			for (i = 0; i < ncal; i++) {
				p->cal[j][i] = cal[j][i];
			}
		}
		p->ncal = ncal;
		p->softcal = softcal;
	} else {
		p->cal[0][0] =  -1.0;
		p->ncal = 0;
		p->softcal = 0;
	}

	if (comport == -99) {
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
			if (verb)
				printf("Using profile '%s' rather than real device\n",p->fake_name);
			p->read = disprd_fake_read_lu;
		} else if (p->mcallout != NULL) {
			if (verb)
				printf("Using shell callout '%s' rather than real device\n",p->mcallout);
			p->read = disprd_fake_read_co;
		} else
			p->read = disprd_fake_read;

		if (disp == NULL) {
			DBG((dbgo,"new_disprd returning fake device\n"));
			return p;
		}

	/* Setup the instrument */
	} else {
	
		if (verb)
			fprintf(p->df,"Setting up the instrument\n");
	
		if ((p->it = new_inst(comport, p->itype, debug, verb, NULL)) == NULL) {
			DBG((stderr,"new_disprd failed because new_inst failed\n"));
			p->del(p);
			if (errc != NULL) *errc = 2;
			return NULL;
		}
	
		/* Establish communications */
		if ((rv = p->it->init_coms(p->it, p->comport, p->br, p->fc, 15.0)) != inst_ok) {
			DBG((dbgo,"init_coms returned '%s' (%s)\n",
			       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
			DBG((dbgo,"new_disprd failed because init_coms failed\n"));
			p->del(p);
			if (errc != NULL) *errc = 2;
			return NULL;
		}
	
		/* Initialise the instrument */
		if ((rv = p->it->init_inst(p->it)) != inst_ok) {
			DBG((dbgo,"init_inst returned '%s' (%s)\n",
			       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)))
			DBG((dbgo,"new_disprd failed because init_inst failed\n"));
			p->del(p);
			if (errc != NULL) {
				*errc = 2;
				if ((rv & inst_imask) == SPYD2_NO_PLD_PATTERN)
					*errc = 22;
			}
			return NULL;
		}
		p->itype = p->it->get_itype(p->it);			/* Actual type */
	
		/* Configure the instrument mode for reading the display */
		if ((rv = config_inst_displ(p)) != 0) {
			DBG((dbgo,"new_disprd failed because config_inst_displ failed\n"));
			p->del(p);
			if (errc != NULL) *errc = rv;
			return NULL;
		}
	}

	/* Create a spectral conversion object if needed */
	if (p->spectral && p->obType != icxOT_none) {
		if ((p->sp2cie = new_xsp2cie(icxIT_none, NULL, p->obType, custObserver, icSigXYZData))
		                                                                                 == NULL) {
			DBG((dbgo,"new_disprd failed because creation of spectral conversion object failed\n"))
			p->del(p);
			if (errc != NULL) *errc = 9;
			return NULL;
		}
	}

	/* Open display window for positioning (no blackbg) */
	if ((p->dw = new_dispwin(disp, patsize, patsize, ho, vo, 0, native, 0,
	                                                        override, debug)) == NULL) {
		DBG((dbgo,"new_disprd failed because new_dispwin failed\n"))
		p->del(p);
		if (errc != NULL) *errc = 3;
		return NULL;
	}

	if (p->it != NULL) {
		/* Do a calibration up front, so as not to get in the users way, */
		/* but ignore a CRT frequency or display integration calibration, */
		/* since these will be done automatically. */
		if ((p->it->needs_calibration(p->it) & inst_calt_needs_cal_mask) != 0
		 && p->it->needs_calibration(p->it) != inst_calt_crt_freq
		 && p->it->needs_calibration(p->it) != inst_calt_disp_int_time
		 && p->it->needs_calibration(p->it) != inst_calt_proj_int_time) {
			disp_win_info dwi;
			dwi.dw = p->dw;		/* Set window to use */
	
			rv = inst_handle_calibrate(p->it, inst_calt_all, inst_calc_none,
			                                  setup_display_calibrate, &dwi);
			setup_display_calibrate(p->it,inst_calc_none, &dwi); 
			printf("\n");
			if (rv != inst_ok) {	/* Abort or fatal error */
				DBG((dbgo,"new_disprd failed because calibrate failed with '%s' (%s)\n",
				       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv)));
				printf("Calibrate failed with '%s' (%s)\n",
				       p->it->inst_interp_error(p->it, rv), p->it->interp_error(p->it, rv));
				p->del(p);
				if (errc != NULL) *errc = 2;
				return NULL;
			}
		}
	}

	/* Ask user to put instrument on screen */
	empty_con_chars();
	printf("Place instrument on test window.\n");
	printf("Hit Esc or Q to give up, any other key to continue:"); fflush(stdout);
	if ((ch = next_con_char()) == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
		printf("\n");
		DBG((dbgo,"new_disprd failed because user aborted when placing device\n"));
		p->del(p);
		if (errc != NULL) *errc = 1;
		return NULL;
	}
	printf("\n");

	/* Close the positioning window */
	if (p->dw != NULL) {
		if (p->or != NULL)
			p->dw->set_ramdac(p->dw,p->or, 0);
		p->dw->del(p->dw);
	}

	/* Open display window again for measurement */
	if ((p->dw = new_dispwin(disp, patsize, patsize, ho, vo, 0, native, blackbg,
	                                                        override, debug)) == NULL) {
		DBG((dbgo,"new_disprd failed new_dispwin failed\n"))
		p->del(p);
		if (errc != NULL) *errc = 3;
		return NULL;
	}

	/* Set color change callout */
	if (ccallout) {
		p->dw->set_callout(p->dw, ccallout);
	}

	/* Save current RAMDAC so that we can restore it */
	if (!softcal && (p->or = p->dw->get_ramdac(p->dw)) == NULL) {
		warning("Unable to read or set display RAMDAC");
	}

	/* Set the given RAMDAC so we can characterise through it */
	if (!softcal && cal != NULL && cal[0][0] >= 0.0 && p->or != NULL) {
		ramdac *r;
		int j, i;
		
		r = p->or->clone(p->or);

		/* Set the ramdac contents. */
		/* We linearly interpolate from cal[ncal] to RAMDAC[nent] resolution */
		for (i = 0; i < r->nent; i++) {
			double val, w;
			unsigned int ix;

			val = (ncal-1.0) * i/(r->nent-1.0);
			ix = (unsigned int)floor(val);		/* Coordinate */
			if (ix > (ncal-2))
				ix = (ncal-2);
			w = val - (double)ix;		/* weight */
			for (j = 0; j < 3; j++) {
				val = cal[j][ix];
				r->v[j][i] = val + w * (cal[j][ix+1] - val);
			}
		}
		if (p->dw->set_ramdac(p->dw, r, 0)) {
			DBG((dbgo,"new_disprd failed becayse set_ramdac failed\n"))
			if (p->verb) {
				fprintf(p->df,"Failed to set RAMDAC to desired calibration.\n");
				fprintf(p->df,"Perhaps the operating system is being fussy ?\n");
			}
			r->del(r);
			p->del(p);
			if (errc != NULL) *errc = 4;
			return NULL;
		}
		r->del(r);
	}

	/* Return the ramdac being used */
	if (!softcal && p->or != NULL && cal != NULL) {
		ramdac *r;
		int j, i;
		
		if ((r = p->dw->get_ramdac(p->dw)) == NULL) {
			DBG((dbgo,"new_disprd failed becayse get_ramdac failed\n"))
			if (p->verb)
				fprintf(p->df,"Failed to read current RAMDAC");
			p->del(p);
			if (errc != NULL) *errc = 4;
			return NULL;
		}
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
	
	DBG((dbgo,"new_disprd succeeded\n"))
	return p;
}

