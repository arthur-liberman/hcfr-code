
#ifndef INSTAPPSUP_H

/* Instrument command line application support functions */

/* 
 * Argyll Color Management System
 *
 * Author: Graeme W. Gill
 * Date:   15/3/2001
 *
 * Copyright 2001 - 2013 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 */

//#include "insttypes.h"		/* libinst Includes this functionality */
//#include "icoms.h"
//#include "conv.h"

#ifdef __cplusplus
	extern "C" {
#endif

/* ============================================================================= */
/* a default user interaction callback handler */

/* User key types */
#define DUIH_NONE 	0x0000		/* No meaning */
#define DUIH_ABORT 	0x0100		/* User abort operation */
#define DUIH_TERM 	0x0200		/* User terminated operation */
#define DUIH_CMND 	0x0400		/* User command */
#define DUIH_TRIG 	0x0800		/* User trigger */

/* The default uicallback function and context. resets uih too. */
#define DUIH_FUNC_AND_CONTEXT (inst_reset_uih(), inst_get_uicallback()), inst_get_uicontext()

/* Return the default uicallback function */
inst_code (*inst_get_uicallback())(void *, inst_ui_purp);

/* Return the default uicallback context */
void *inst_get_uicontext();

/* Install the default uicallback function in the given inst */
void inst_set_uicallback(inst *p);

/* Reset user interaction handling to default (Esc, ^C, q or 'Q' = Abort) */
void inst_reset_uih();

/* Set a key range to the given handling type */
/* min & max are between 0 and 255, status is one of */
/* DUIH_OK, DUIH_USER, DUIH_TERM, DUIH_TRIG, DUIH_CMND */
void inst_set_uih(int min, int max, int status);

/* Get the character that caused the user trigger or abort */
/* + its key type in the upper 8 bits. */
/* Clear it to 0x00 after reading it. */
int inst_get_uih_char();

/* ============================================================================= */

#ifndef DISPSUP_H
/* Opaque type as far as inst.h is concerned. */
typedef struct _disp_win_info disp_win_info;
#endif

/* A default calibration user interaction handler using the console. */
/* This handles both normal and display based calibration interaction */
/* with the instrument, if a disp_setup function and pointer to disp_win_info */
/* is provided. */
inst_code inst_handle_calibrate(
	inst *p,
	inst_cal_type calt,		/* Calibration type to do */
	inst_cal_cond calc,		/* Current calibration condition */
	inst_code (*disp_setup) (inst *p, inst_cal_cond calc, disp_win_info *dwi),
							/* Callback for handling a display calibration - May be NULL */
	disp_win_info *dwi,		/* Information to be able to open a display test patch - May be NULL */
	int doimmediately		/* If nz, don't wait for user, calibrate immediatley */
);

/* ============================================================================= */

/* A helper function to display -y flag usage for each instrument type available */
/* Return accumulated capabilities2 of all the instruments. */
/* If docbib is nz, then only display the base calibration display types */
inst2_capability inst_show_disptype_options(FILE *fp, char *oline, icompaths *icmps, int docbib);

/* A helper function to turn a -y flag into a list index */
/* If docbib is nz, then only allow base calibration display types */
/* Return 0 on error */
int inst_get_disptype_index(inst *it, int ditype, int docbib);

/* Return a static string of the ditype flag char(s) */
char *inst_distr(int ditype);

#ifdef __cplusplus
	}
#endif

#define INSTAPPSUP_H
#endif /* INSTAPPSUP_H */
