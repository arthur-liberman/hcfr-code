
#ifndef INSTTYPES_H

 /* Instrument suported types utilities. */

/* 
 * Argyll Color Correction System
 *
 * Author: Graeme W. Gill
 * Date:   15/3/2001
 *
 * Copyright 2001 - 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 */

#ifdef __cplusplus
	extern "C" {
#endif

/* ----------------------------- */
/* Possible types of instruments */
typedef enum {
    instUnknown      = -1,		/* Undefined Instrument */
    instDTP20        = 0,		/* Xrite DTP20 (Pulse)  */
    instDTP22,					/* Xrite DTP22 (Digital Swatchbook)  */
    instDTP41,       			/* Xrite DTP41 */
    instDTP51, 					/* Xrite DTP51 */
    instDTP92, 					/* Xrite DTP92 */
    instDTP94, 					/* Xrite DTP94 (Optix) */
    instSpectrolino, 			/* GretagMacbeth Spectrolino */
    instSpectroScan, 			/* GretagMacbeth SpectroScan */
    instSpectroScanT, 			/* GretagMacbeth SpectroScanT */
	instSpectrocam,				/* Avantes Spectrocam */
	instI1Display,				/* GretagMacbeth i1 Display */
	instI1Monitor,				/* GretagMacbeth i1 Monitor */
	instI1Pro,					/* GretagMacbeth i1 Pro */
	instI1Disp3,				/* Xrite i1 DisplayPro, ColorMunki Display */
	instColorMunki,				/* X-Rite ColorMunki */
	instHCFR,					/* Colorimtre HCFR */
	instSpyder2,				/* Datacolor/ColorVision Spyder2 */
	instSpyder3,				/* Datacolor Spyder3 */
	instHuey,					/* GretagMacbeth Huey */

} instType;

/* Utility functions in libinsttypes */

/* Given its instrument type, return the matching */
/* instrument identification name (static string), */
extern char *inst_name(instType itype);


/* Given an instrument identification name, return the matching */
/* instType, or instUnknown if not matched */
extern instType inst_enum(char *name);


#ifdef ENABLE_USB
/* Given a USB vendor and product ID, */
/* return the matching instrument type, or */
/* instUnknown if none match. */
extern instType inst_usb_match(
unsigned short idVendor,
unsigned short idProduct);
#endif /* ENABLE_USB */


/* Should deprecate the following. It should be replaced with a */
/* method in the instrument class that returns its configured spectrum, */
/* and the spectrum should be embedded in the .ti3 file, not the instrument */
/* name. */

/* Fill in an instruments illuminant spectrum. */
/* Return 0 on sucess, 1 if not not applicable. */
extern int inst_illuminant(xspect *sp, instType itype);


#ifdef __cplusplus
	extern "C" {
#endif

#define INSTTYPES_H
#endif /* INSTTYPES_H */

