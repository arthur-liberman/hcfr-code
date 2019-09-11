
#ifndef INSTTYPES_H

/* Device and Instrument suported types definitions. */

/* 
 * Argyll Color Correction System
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

#ifdef __cplusplus
	extern "C" {
#endif


/* ----------------------------- */
/* Possible types of devices and instruments */
typedef enum {
    devUnknown      = 0,		/* Undefined Device */

	/* Color measurement instruments */
    instDTP22,					/* Xrite DTP22 (Digital Swatchbook)  */
    instDTP41,       			/* Xrite DTP41 */
    instDTP51, 					/* Xrite DTP51 */
    instSpectrolino, 			/* GretagMacbeth Spectrolino */
    instSpectroScan, 			/* GretagMacbeth SpectroScan */
    instSpectroScanT, 			/* GretagMacbeth SpectroScanT */
	instSpectrocam,				/* Avantes Spectrocam */
	instSpecbos1201,			/* JETI specbos 1201 */
	instSpecbos,				/* JETI specbos XXXX */
	instSpectraval,				/* JETI spectraval 1501, 1511 */
	instKleinK10,				/* Klein K10-A */
	instSMCube,					/* SwatchMate Cube */
    instDTP20,					/* Xrite DTP20 (Pulse)  */
    instDTP92, 					/* Xrite DTP92 */
    instDTP94, 					/* Xrite DTP94 (Optix) */
	instI1Disp1,				/* GretagMacbeth i1 Display 1 */
	instI1Disp2,				/* GretagMacbeth i1 Display 2 */
	instI1Disp3,				/* Xrite i1 DisplayPro, ColorMunki Display */
	instI1Monitor,				/* GretagMacbeth i1 Monitor */
	instI1Pro,					/* GretagMacbeth i1 Pro */
	instI1Pro2,					/* X-Rite i1 Pro2 */
	instColorMunki,				/* X-Rite ColorMunki */
	instHCFR,					/* Colorimtre HCFR */
	instSpyder1,				/* Datacolor/ColorVision Spyder1 */
	instSpyder2,				/* Datacolor/ColorVision Spyder2 */
	instSpyder3,				/* Datacolor Spyder3 */
	instSpyder4,				/* Datacolor Spyder4 */
	instSpyder5,				/* Datacolor Spyder5 */
	instSpyderX,				/* Datacolor SpyderX */
	instHuey,					/* GretagMacbeth Huey */
	instSmile,					/* X-rite Colormunki Smile */
	instEX1,					/* Image Engineering EX1 */
	instColorHug,				/* Hughski ColorHug */
	instColorHug2,				/* Hughski ColorHug2 */


	instFakeDisp = 9998,		/* Fake display & instrument device id */

#ifdef ENABLE_VTPGLUT
	/* 3D cLUT box */
	devRadiance = 20000,		/* Lumagen Radiance v3dlut & vtpg */
	devPrisma,					/* Q, Inc Prisma v3dlut */

	/* Video test patern generator box */
	// 30000
#endif



} devType;

/* Aliases for backwards compatibility */
#define instUnknown devUnknown
typedef devType instType;
typedef devType vcLUTType;
typedef devType vtpgType;
typedef devType printerType;

struct _icoms;					/* Forward declarations */
enum _icom_type;

/* Utility functions in libinsttypes */

/* Given a device type, return the corrsponding */
/* category */
//extern _icom_type dev_category(instType itype);

/* Given its instrument type, return the matching */
/* short instrument name (static string), */
extern char *inst_sname(instType itype);

/* Given its instrument type, return the matching */
/* long instrument identification name (static string), */
extern char *inst_name(instType itype);


/* Given an instrument long identification name, return the matching */
/* instType, or instUnknown if not matched */
extern instType inst_enum(char *name);


#ifdef ENABLE_USB
/* Given a USB vendor and product ID, */
/* return the matching instrument type, or */
/* instUnknown if none match. */
extern instType inst_usb_match(
unsigned int idVendor,
unsigned int idProduct,
int nep);					/* Number of end points (0 for prelim match) */
#endif /* ENABLE_USB */


/* Should deprecate the following. It should be replaced with a */
/* method in the instrument class that returns its configured spectrum, */
/* and the spectrum should be embedded in the .ti3 file, not the instrument */
/* name. */

/* Fill in an instruments illuminant spectrum. */
/* Return 0 on sucess, 1 if not not applicable. */
extern int inst_illuminant(xspect *sp, instType itype);


/* ------------------------------------------------------ */
/* Gretag/X-Rite specific reflective measurement standard */

typedef enum {
	xcalstd_none = -2,		/* Not set */
	xcalstd_native = -1,	/* No conversion */
	xcalstd_xrdi = 0,		/* Historical X-Rite */
	xcalstd_gmdi = 1,		/* Historical Gretag-Macbeth */
	xcalstd_xrga = 2,		/* Current X-Rite */
} xcalstd;

/* Return a string for the xcalstd enum */
char *xcalstd2str(xcalstd std);

/* Parse a string to xcalstd, */
/* return xcalstd_none if not recognized */
xcalstd str2xcalstd(char *str);

#ifdef __cplusplus
	}
#endif

#define INSTTYPES_H
#endif /* INSTTYPES_H */

