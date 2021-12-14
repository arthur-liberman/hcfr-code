
/* Instrument command line application support functions */

/* 
 * Argyll Color Management System
 *
 * Author: Graeme W. Gill
 * Date:   10/3/2001
 *
 * Copyright 2001 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#else
#include "sa_config.h"
#endif /* !SALONEINSTLIB */
#include "numsup.h"
#include "cgats.h"
#include "xspect.h"
#include "conv.h"
#include "insttypes.h"

#include "icoms.h"
#include "inst.h"
#include "rspec.h"
#include "insttypeinst.h"
#include "instappsup.h"

/* ================================================================= */
/* a default user interaction handler */

typedef struct _uicontext {
	int emit_ret;				/* Emit \n on inst_triggered */
	int cut;					/* The character that caused the termination */
	int uih[256];				/* User interrupt handling key table. Value can be: */
								/* DUIH_OK, DUIH_ABORT, DUIH_TERM, DUIH_TRIG, DUIH_CMND */
} uicontext;

static uicontext def_uicntx = { 1, 0, { 0 } };

static inst_code def_uicallback(void *cntx, inst_ui_purp purp) {
	uicontext *p = (uicontext *)cntx;

	if (purp == inst_triggered) {
		if (p->emit_ret)
			printf("\n");
		return inst_ok;

	} else if (purp == inst_negcoms
	        || purp == inst_armed
	        || purp == inst_measuring) {
		int c;

		c = poll_con_char();
		if (c != 0) {
			p->cut = c;
			c = p->uih[c];
			if (c & (DUIH_ABORT | DUIH_TERM | DUIH_CMND))
				return inst_user_abort;
			if (c & DUIH_TRIG)
				return inst_user_trig;
		}

	} else if (purp == inst_measuring) {
		return inst_ok;
	}
	return inst_ok;
}

/* Return the default uicallback function */
inst_code (*inst_get_uicallback())(void *, inst_ui_purp) {
	return &def_uicallback;
}

/* Return the default uicallback context */
void *inst_get_uicontext() {
	return (void *)&def_uicntx;
}

/* Install the default uicallback function in the given inst */
void inst_set_uicallback(inst *p) {
	p->set_uicallback(p, def_uicallback, (void *)&def_uicntx);
}

/* Set the return on trigger to true or false */
void inst_set_uicb_trigret(int set) {
	uicontext *p = &def_uicntx;
	p->emit_ret = set;
}

/* Reset user interaction handling to default (Esc, ^C, q or 'Q' = Abort) */
void inst_reset_uih() {
	uicontext *p = &def_uicntx;
	int i;

	for (i = 0; i < 255; i++)
		p->uih[i] = DUIH_NONE;

	p->uih[0x1b] = DUIH_ABORT;	/* Escape */
	p->uih['q']  = DUIH_ABORT;	/* q */
	p->uih['Q']  = DUIH_ABORT;	/* Q */
	p->uih[0x03] = DUIH_ABORT;	/* ^C */
}

/* Set a key range to the given handling type */
/* min & max are between 0 and 255, status is one of */
/* DUIH_OK, DUIH_USER, DUIH_TERM, DUIH_TRIG, DUIH_CMND */
void inst_set_uih(
int min,		/* Start key code */
int max,		/* End key code (inclusive) */
int status		/* ICOM_OK, ICOM_USER, ICOM_TERM, ICOM_TRIG, ICOM_CMND */
) {
	uicontext *p = &def_uicntx;
	int i;

	if (min < 0)
		min = 0;
	else if (min > 255)
		min = 255;
	if (max < 0)
		max = 0;
	else if (max > 255)
		max = 255;

	if (status != DUIH_NONE
	 && status != DUIH_ABORT
	 && status != DUIH_TERM
	 && status != DUIH_CMND
	 && status != DUIH_TRIG)
		status = DUIH_NONE;

	for (i = min; i <= max; i++) {
		p->uih[i] = status;
	}
}

/* Get the character that caused the user interrupt */
/* + its key type in the upper 8 bits. */
/* Clear it to 0x00 after reading it. */
int inst_get_uih_char() {
	uicontext *p = &def_uicntx;
	int c = p->cut;
	c |= p->uih[c];
	p->cut = 0;
	return c;
}

/* ================================================================= */

/* A default calibration user interaction handler using the console. */
/* This handles both normal and display based calibration interaction */
/* with the instrument, if a disp_setup function and pointer to disp_win_info */
/* is provided. */
inst_code inst_handle_calibrate(
	inst *p,
	inst_cal_type calt,		/* Calibration type to do */
	inst_cal_cond calc,		/* Current current condition */
	inst_code (*disp_setup) (inst *p,inst_cal_cond calc, disp_win_info *dwi),
							/* Callback for handling a display calibration - May be NULL */
	disp_win_info *dwi,		/* Information to be able to open a display test patch - May be NULL */
	int doimmediately		/* If nz, don't wait for user, calibrate immediatley */
) {
	inst_code rv = inst_ok, ev;
	int usermes = 0;			/* User was given a message */
	inst_calc_id_type idtype;	/* Condition identifier type */
	char id[200];				/* Condition identifier */
	int ch;

	a1logd(p->log,1,"inst_handle_calibrate called\n");

	/* Untill we're done with the calibration */
	for (;;) {

		a1logd(p->log,1,"About to call calibrate at top of loop\n");
	    ev = p->calibrate(p, &calt, &calc, &idtype, id);
		a1logd(p->log,1,"Calibrate returned calt 0x%x, calc 0x%x, ev 0x%x\n",calt,calc,ev);

		/* We're done */
		if ((ev & inst_mask) == inst_ok) {
			if ((calc & inst_calc_cond_mask) == inst_calc_message) {
				/* (Or could create our own message text based on value of idtype) */
				printf("%s\n",id);
			}
			if (usermes)
				printf("Calibration complete\n");
			fflush(stdout);
			a1logd(p->log,1,"inst_handle_calibrate done 0x%x\n",ev);
			return ev;
		}

		/* User aborted */
		if ((ev & inst_mask) == inst_user_abort) {
			a1logd(p->log,1,"inst_handle_calibrate user aborted 0x%x\n",ev);
			return ev;
		}

		/* Retry on an error */
		if ((ev & inst_mask) != inst_cal_setup) {
			if ((ev & inst_mask) == inst_unsupported) {
				a1logd(p->log,1,"inst_handle_calibrate err 0x%x, calibration type 0x%x not supported\n",ev, calt);
				return inst_unsupported;
			}

			printf("Calibration failed with '%s' (%s)\n",
				       p->inst_interp_error(p, ev), p->interp_error(p, ev));

			if (doimmediately)
				return inst_user_abort;

			printf("Hit any key to retry, or Esc or Q to abort:\n");

			empty_con_chars();
			ch = next_con_char();
			printf("\n");
			if (ch == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
				a1logd(p->log,1,"inst_handle_calibrate user aborted 0x%x\n",inst_user_abort);
				fflush(stdout);
				return inst_user_abort;
			}

		} else {

			printf("\n");

			/* Get user to do/setup calibration */
			switch (calc & inst_calc_cond_mask) {
				case inst_calc_uop_ref_white:
					printf("Do a reflective white calibration,\n");
					printf(" and then hit any key to continue,\n"); 
					break;

				case inst_calc_uop_trans_white:
					printf("Do a transmissive white calibration,\n");
					printf(" and then hit any key to continue,\n"); 
					break;
			
				case inst_calc_uop_trans_dark:
					printf("Do a transmissive dark calibration,\n");
					printf(" and then hit any key to continue,\n"); 
					break;
			
				case inst_calc_man_ref_white:
					printf("Place the instrument on its reflective white reference S/N %s,\n",id);
					printf(" and then hit any key to continue,\n"); 
					break;

				case inst_calc_man_ref_whitek:
					printf("Click the instrument on its reflective white reference %s,\n",id);
					break;

				case inst_calc_man_ref_dark:
					printf("Place the instrument on light trap, or in the dark,\n");
					printf("and distant from any surface,\n");
					printf(" and then hit any key to continue,\n"); 
					break;

				case inst_calc_man_dark_gloss:
					printf("Place the instrument on black gloss reference\n");
					printf(" and then hit any key to continue,\n"); 
					break;

				case inst_calc_man_em_dark:
					printf("Place cap on the instrument, or place on a dark surface,\n");
					printf("or place on the calibration reference,\n");
					printf(" and then hit any key to continue,\n"); 
					break;

				case inst_calc_man_am_dark:
					printf("Place ambient adapter and cap on the instrument,\n");
					printf("or place on the calibration reference,\n");
					printf(" and then hit any key to continue,\n"); 
					break;

				case inst_calc_man_cal_smode:
					printf("Set instrument sensor to calibration position,\n");
					printf(" and then hit any key to continue,\n"); 
					break;

				case inst_calc_man_trans_white:
					printf("Place the instrument on its transmissive white source,\n");
					printf(" and then hit any key to continue,\n"); 
					break;

				case inst_calc_man_trans_dark:
					printf("Use the appropriate tramissive blocking to block the transmission path,\n");
					printf(" and then hit any key to continue,\n"); 
					break;

				case inst_calc_change_filter:
					printf("Change filter on instrument to %s,\n",id);
					printf(" and then hit any key to continue,\n"); 
					break;

				case inst_calc_message:
					printf("%s\n",id);
					printf(" Hit any key to continue,\n"); 
					break;

				case inst_calc_emis_white:
					if (disp_setup == NULL || dwi == NULL) { /* No way of creating a test window */
						printf("Place the instrument on a 100%% white test patch,\n");
						printf(" and then hit any key to continue,\n"); 
					} else {
						/* We need to display a 100% white patch to proceed with this */
						/* type of calibration */
						if ((rv = disp_setup(p, calc, dwi)) != inst_ok)
							return rv; 
					}
					break;

				case inst_calc_emis_80pc:
					if (disp_setup == NULL || dwi == NULL) { /* No way of creating a test window */
						printf("Place the instrument on a 80%% white test patch,\n");
						printf(" and then hit any key to continue,\n"); 
					} else {
						/* We need to display a 80% white patch to proceed with this */
						/* type of calibration */
						if ((rv = disp_setup(p, calc, dwi)) != inst_ok)
							return rv; 
					}
					break;

				case inst_calc_emis_grey:
				case inst_calc_emis_grey_darker: 
				case inst_calc_emis_grey_ligher:
					if (dwi == NULL) {	/* No way of creating a test window */
						if ((calc & inst_calc_cond_mask) == inst_calc_emis_grey) {
							p->cal_gy_level = 0.6;
							p->cal_gy_count = 0;
						} else if ((calc & inst_calc_cond_mask) == inst_calc_emis_grey_darker) {
							p->cal_gy_level *= 0.7;
							p->cal_gy_count++;
						} else if ((calc & inst_calc_cond_mask) == inst_calc_emis_grey_ligher) {
							p->cal_gy_level *= 1.4;
							if (p->cal_gy_level > 1.0)
								p->cal_gy_level = 1.0;
							p->cal_gy_count++;
						}
						if (p->cal_gy_count > 4) {
							printf("Cell ratio calibration failed - too many tries at setting grey level.\n");
							a1logd(p->log,1,"inst_handle_calibrate too many tries at setting grey level 0x%x\n",inst_internal_error);
							return inst_internal_error; 
						} else {
							printf("Place the instrument on a %d%% white test patch,\n", (int)(p->cal_gy_level * 100.0 + 0.5));
							printf(" and then hit any key to continue,\n"); 
						}
					} else {

						/* We need to display a test patch to proceed with this
						 * type of calibration. Typically this will be:
						 *
						 * inst_calc_xxxx_grey:
						 *		set p->cal_gy_level = 0.6
						 *		set p->cal_gy_count = 0;
						 *
						 * inst_calc_xxxx_grey_darker:
						 *		set p->cal_gy_level *= 0.7
						 *		set p->cal_gy_count++
						 *
						 * inst_calc_xxxx_grey_ligher:
						 *		set p->cal_gy_level *= 1.4
						 *		set p->cal_gy_count++
						 *
						 * and return failure if p->cal_gy_count > 4
						 */

						if ((rv = disp_setup(p, calc, dwi)) != inst_ok)
							return rv; 
					}
					break;

				default:
					/* Something isn't being handled */
					a1logd(p->log,1,"inst_handle_calibrate unhandled calc case 0x%x, err 0x%x\n",calc,inst_internal_error);
					return inst_internal_error;
			}
			if (calc & inst_calc_optional_flag)
				printf(" or hit Esc or Q to abort, or S to skip: "); 
			else
				printf(" or hit Esc or Q to abort: "); 
			fflush(stdout);

			usermes = 1;

			/* If we should wait for user to say we're in the right condition, */
			/* and this isn't a calibration that requires clicking the instrument */
			/* on the calibration tile, then wait for the an OK or abort or skip. */
			if (!doimmediately
			 && (calc & inst_calc_cond_mask) != inst_calc_man_ref_whitek) {
				empty_con_chars();
				ch = next_con_char();
				printf("\n");
				/* If optional calib. and user wants to skip it */
				/* Loop back to calibrate() with inst_calc_optional_flag still set */
				if ((calc & inst_calc_optional_flag) != 0 && (ch == 's' || ch == 'S')) {
					printf("Skipped\n");
					goto oloop;
				}
				if (ch == 0x1b || ch == 0x3 || ch == 'q' || ch == 'Q') {
					a1logd(p->log,1,"inst_handle_calibrate user aborted 0x%x\n",inst_user_abort);
					return inst_user_abort;
				}
			}
			/* Remove any skip flag and continue with the calibration */
			calc &= inst_calc_cond_mask;
		}
 oloop:;
	}
}

/* ============================================================================= */

/* A helper function to display -y flag usage for each instrument type available */
/* Return accumulated capabilities2 of all the instruments */
/* Return all possible capabilities if there are no instruments */
/* If docbib is nz, then only display the base calibration display types */
inst2_capability inst_show_disptype_options(FILE *fp, char *oline, icompaths *icmps, int docbib) { 
	int i, j;
	char buf[200], *bp;
	char extra[40];
	int olen, pstart;
	int notall = 0;				/* Not all instruments are USB */
	int gotone = 0;				/* Found at least one USB instrument */
	inst2_capability acap = 0;	/* Accumulate capabilities */

	if (icmps == NULL)
		return 0;

	/* Locate the end of the option */
	for (bp = oline; *bp != '\000' && *bp == ' '; bp++)
		;
	for (; *bp != '\000' && *bp != ' '; bp++)
		;
	pstart = bp - oline;
	if (pstart > 10)
		pstart = 10;
	strncpy(buf, oline, pstart); 
	buf[pstart++] = ' ';

	olen = strlen(oline);		/* lenth of option part of line */

	for (i = 0; icmps != NULL && i < icmps->ndpaths[dtix_inst]; i++) {
		inst *it;
		inst2_capability cap;
		int k;

		if ((it = new_inst(icmps->dpaths[dtix_inst][i], 1, g_log, NULL, NULL)) == NULL) {
			notall = 1;
			continue;
		}
		gotone = 1;

		it->capabilities(it, NULL, &cap, NULL);
		acap |= cap;

		if (cap & inst2_disptype) {
			int nsel;
			inst_disptypesel *sels;

			if (it->get_disptypesel(it, &nsel, &sels, 1, 0) != inst_ok) {
				it->del(it);
				continue;
			}
			for (j = 0; j < nsel; j++) {
				int m;

				if (docbib && sels[j].cbid == 0)
					continue;			/* Skip non cbid type */

				/* Break up selector chars/pairs with '|' */
				m = pstart;
				for (k = 0; k < (INST_DTYPE_SEL_LEN-1); k++) {
					if (sels[j].sel[k] == '\000')
						break;
					if (m > pstart)
						buf[m++] = '|';
					buf[m++] = sels[j].sel[k];
					if (sels[j].sel[k] == '_')
						buf[m++] = sels[j].sel[++k];
				}
				while (m < (olen+1))	/* Indent it by 1 */
					buf[m++] = ' ';
				buf[m++] = '\000';
				
				extra[0] = '\000';
				if ((sels[j].flags & inst_dtflags_default) || sels[j].cbid != 0) {
					strcat(extra, " [");
			        if (sels[j].flags & inst_dtflags_default) {
						strcat(extra, "Default");
						if (sels[j].cbid != 0)
							strcat(extra, ",");
					}
					if (sels[j].cbid != 0) {
						sprintf(extra + strlen(extra), "CB%d",sels[j].cbid);
					}
					strcat(extra, "]");
				}

				fprintf(fp, "%s%s: %s%s\n",buf, inst_sname(it->dtype), sels[j].desc, extra);

				if (j == 0) {
					for (m = 0; m < pstart; m++)
						buf[m] = ' ';
				}
			}
		}
		it->del(it);
	}
	/* Output a default desciption if not all instruments are USB */
	if (notall) {
		int m = pstart;
		buf[m++] = 'l';
		buf[m++] = '|';
		buf[m++] = 'c';
		while (m < olen)
			buf[m++] = ' ';
		buf[m++] = '\000';
		fprintf(fp, "%s%s\n",buf, " Other: l = LCD, c = CRT");
	}
	if (!gotone)
		acap = ~0;

	return acap;
}

/* A helper function to turn a -y flag into a selection index */
/* If docbib is nz, then only allow base calibration display types */
/* c will be 16 bits ('_' + 'X') if a 2 char selector */
/* Return -1 on error */
int inst_get_disptype_index(inst *it, int ditype, int docbib) {
	inst2_capability cap;
	int j, k;

	it->capabilities(it, NULL, &cap, NULL);

	if (cap & inst2_disptype) {
		int nsel;
		inst_disptypesel *sels;

		if (it->get_disptypesel(it, &nsel, &sels, 1, 0) != inst_ok) {
			return -1;
		}
		for (j = 0; j < nsel; j++) {
			if (docbib && sels[j].cbid == 0)
				continue;			/* Skip non cbid type */

			for (k = 0; k < (INST_DTYPE_SEL_LEN-1); k++) {
				if (sels[j].sel[k] == '\000')
					break;
				if (sels[j].sel[k] == '_') {	/* 2 char selector */
					k++;
					if (sels[j].sel[k-1] == (0xff & (ditype >> 8))
					 && sels[j].sel[k] == (0xff & ditype)) {
						return j;
					}
				}
				if (sels[j].sel[k] == ditype) {
					return j;
				}
			}
		}
	}
	return -1;
}

/* Return a static string of the ditype flag char(s) */
char *inst_distr(int ditype) {
	static char buf[5];
	
	if ((ditype >> 8) & 0xff) {
		buf[0] = (ditype >> 8) & 0xff;
		buf[1] = ditype & 0xff;
		buf[2] = '\000';
	} else {
		buf[0] = ditype;
		buf[1] = '\000';
	}

	return buf;
}
/* ================================================================= */




















