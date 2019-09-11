
/* Instrument supported types utilities */

/* 
 * Argyll Color Correction System
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
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"

/* NOTE NOTE NOTE: */
/* Need to add a new instrument to new_inst() in */
/* inst.c as well !!! */

/* Utility functions */

/* Given a device type, return the corrsponding */
/* category */
extern icom_type dev_category(devType dtype) {
	switch (dtype) {

	/* Color measurement instruments */
	case instDTP22:
	case instDTP41:
	case instDTP51:
	case instSpectrolino:
	case instSpectroScan:
	case instSpectroScanT:
	case instSpectrocam:
	case instSpecbos1201:
	case instSpecbos:
	case instSpectraval:
	case instKleinK10:
	case instSMCube:
	case instDTP20:
	case instDTP92:
	case instDTP94:
	case instI1Disp1:
	case instI1Disp2:
	case instI1Disp3:
	case instI1Monitor:
	case instI1Pro:
	case instI1Pro2:
	case instColorMunki:
	case instHCFR:
	case instSpyder1:
	case instSpyder2:
	case instSpyder3:
	case instSpyder4:
	case instSpyder5:
	case instSpyderX:
	case instHuey:
	case instSmile:
	case instEX1:
	case instColorHug:
	case instColorHug2:


		return icomt_instrument;

#ifdef ENABLE_VTPGLUT
	/* 3D cLUT box */
	case devRadiance:
		return icomt_v3dlut | icomt_vtpg;
	case devPrisma:
		return icomt_v3dlut;

	/* Video test patern generator box */

#endif



	case instFakeDisp:
		return icomt_unknown;		// ???


	case devUnknown:
		return icomt_unknown;		// ???
	}
	return icomt_cat_any;
}

/* Return the short instrument identification name (static string) */
char *inst_sname(instType itype) {
	switch (itype) {
		case instDTP20:
			return "DTP20";
		case instDTP22:
			return "DTP22";
		case instDTP41:
			return "DTP41";
		case instDTP51:
			return "DTP51";
		case instDTP92:
			return "DTP92";
		case instDTP94:
			return "DTP94";
		case instSpectrolino:
			return "Spectrolino";
		case instSpectroScan:
			return "SpectroScan";
		case instSpectroScanT:
			return "SpectroScanT";
		case instSpectrocam:
			return "Spectrocam";
		case instI1Disp1:
			return "i1D1";
		case instI1Disp2:
			return "i1D2";
		case instI1Disp3:
			return "i1D3";
		case instI1Monitor:
			return "i1 Monitor";
		case instI1Pro:
			return "i1 Pro";
		case instI1Pro2:
			return "i1 Pro 2";
		case instColorMunki:
			return "ColorMunki";
		case instHCFR:
			return "HCFR";
		case instSpyder1:
			return "Spyder1";
		case instSpyder2:
			return "Spyder2";
		case instSpyder3:
			return "Spyder3";
		case instSpyder4:
			return "Spyder4";
		case instSpyder5:
			return "Spyder5";
		case instSpyderX:
			return "SpyderX";
		case instHuey:
			return "Huey";
		case instSmile:
			return "Smile";
		case instSpecbos1201:
			return "specbos 1201";
		case instSpecbos:
			return "specbos";
		case instSpectraval:
			return "spectraval";
		case instKleinK10:
			return "K-10";
		case instEX1:
			return "EX1";
		case instSMCube:
			return "Cube";
		case instColorHug:
			return "ColorHug";
		case instColorHug2:
			return "ColorHug2";


#ifdef ENABLE_VTPGLUT
		case devRadiance:
			return "Radiance";
		case devPrisma:
			return "Prisma";
#endif



		default:
			break;
	}
	return "Unknown";
}

/* Return the long instrument identification name (static string) */
char *inst_name(instType itype) {
	switch (itype) {
		case instDTP20:
			return "X-Rite DTP20";
		case instDTP22:
			return "X-Rite DTP22";
		case instDTP41:
			return "X-Rite DTP41";
		case instDTP51:
			return "X-Rite DTP51";
		case instDTP92:
			return "X-Rite DTP92";
		case instDTP94:
			return "X-Rite DTP94";
		case instSpectrolino:
			return "GretagMacbeth Spectrolino";
		case instSpectroScan:
			return "GretagMacbeth SpectroScan";
		case instSpectroScanT:
			return "GretagMacbeth SpectroScanT";
		case instSpectrocam:
			return "Spectrocam";
		case instI1Disp1:
			return "GretagMacbeth i1 Display 1";
		case instI1Disp2:
			return "GretagMacbeth i1 Display 2";
		case instI1Disp3:
			return "X-Rite i1 DisplayPro, ColorMunki Display";
		case instI1Monitor:
			return "GretagMacbeth i1 Monitor";
		case instI1Pro:
			return "GretagMacbeth i1 Pro";
		case instI1Pro2:
			return "X-Rite i1 Pro 2";
		case instColorMunki:
			return "X-Rite ColorMunki";
		case instHCFR:
			return "Colorimtre HCFR";
		case instSpyder1:
			return "ColorVision Spyder1";
		case instSpyder2:
			return "ColorVision Spyder2";
		case instSpyder3:
			return "Datacolor Spyder3";
		case instSpyder4:
			return "Datacolor Spyder4";
		case instSpyder5:
			return "Datacolor Spyder5";
		case instSpyderX:
			return "Datacolor SpyderX";
		case instHuey:
			return "GretagMacbeth Huey";
		case instSmile:
			return "ColorMunki Smile";
		case instSpecbos1201:
			return "JETI specbos 1201";
		case instSpecbos:
			return "JETI specbos";
		case instSpectraval:
			return "JETI spectraval";
		case instKleinK10:
			return "Klein K-10";
		case instEX1:
			return "Image Engineering EX1";
		case instSMCube:
			return "SwatchMate Cube";
		case instColorHug:
			return "Hughski ColorHug";
		case instColorHug2:
			return "Hughski ColorHug2";


#ifdef ENABLE_VTPGLUT
		case devRadiance:
			return "Lumagen Radiance";
		case devPrisma:
			return "Q, Inc Prisma";
#endif



		default:
			break;
	}
	return "Unknown Instrument";
}

/* Given a long instrument identification name, return the matching */
/* instType, or instUnknown if not matched */
instType inst_enum(char *name) {

	if (strcmp(name, "Xrite DTP20") == 0
	 || strcmp(name, "X-Rite DTP20") == 0)
		return instDTP20;
	else if (strcmp(name, "Xrite DTP22") == 0
	      || strcmp(name, "X-Rite DTP22") == 0)
		return instDTP22;
	else if (strcmp(name, "Xrite DTP41") == 0
	     ||  strcmp(name, "X-Rite DTP41") == 0)
		return instDTP41;
	else if (strcmp(name, "Xrite DTP51") == 0
	     ||  strcmp(name, "X-Rite DTP51") == 0)
		return instDTP51;
	else if (strcmp(name, "Xrite DTP92") == 0
	     ||  strcmp(name, "X-Rite DTP92") == 0)
		return instDTP92;
	else if (strcmp(name, "Xrite DTP94") == 0
	     ||  strcmp(name, "X-Rite DTP94") == 0)
		return instDTP94;
	else if (strcmp(name, "GretagMacbeth Spectrolino") == 0)
		return instSpectrolino;
	else if (strcmp(name, "GretagMacbeth SpectroScan") == 0)
		return instSpectroScan;
	else if (strcmp(name, "GretagMacbeth SpectroScanT") == 0)
		return instSpectroScanT;
	else if (strcmp(name, "Spectrocam") == 0)
		return instSpectrocam;
	else if (strcmp(name, "GretagMacbeth i1 Display 1") == 0)
		return instI1Disp1;
	else if (strcmp(name, "GretagMacbeth i1 Display 2") == 0
		  || strcmp(name, "GretagMacbeth i1 Display") == 0
	      || strcmp(name, "Xrite i1 Display") == 0
	      || strcmp(name, "X-Rite i1 Display") == 0)
		return instI1Disp2;
	else if (strcmp(name, "Xrite i1 DisplayPro") == 0
	      || strcmp(name, "X-Rite i1 DisplayPro") == 0
	      || strcmp(name, "ColorMunki Display") == 0
	      || strcmp(name, "X-Rite i1 DisplayPro, ColorMunki Display") == 0
	      || strcmp(name, "Xrite i1 DisplayPro, ColorMunki Display") == 0)
		return instI1Disp3;
	else if (strcmp(name, "GretagMacbeth i1 Monitor") == 0)
		return instI1Monitor;
	else if (strcmp(name, "GretagMacbeth i1 Pro") == 0
	      || strcmp(name, "Xrite i1 Pro") == 0
	      || strcmp(name, "X-Rite i1 Pro") == 0)
		return instI1Pro;
	else if (strcmp(name, "Xrite i1 Pro 2") == 0
		  || strcmp(name, "X-Rite i1 Pro 2") == 0)
		return instI1Pro2;
	else if (strcmp(name, "XRite ColorMunki") == 0
		  || strcmp(name, "X-Rite ColorMunki") == 0)
		return instColorMunki;
	else if (strcmp(name, "Colorimtre HCFR") == 0)
		return instHCFR;
	else if (strcmp(name, "ColorVision Spyder1") == 0)
		return instSpyder1;
	else if (strcmp(name, "ColorVision Spyder2") == 0)
		return instSpyder2;
	else if (strcmp(name, "Datacolor Spyder3") == 0)
		return instSpyder3;
	else if (strcmp(name, "Datacolor Spyder4") == 0)
		return instSpyder4;
	else if (strcmp(name, "Datacolor Spyder5") == 0)
		return instSpyder5;
	else if (strcmp(name, "Datacolor SpyderX") == 0)
		return instSpyderX;
	else if (strcmp(name, "GretagMacbeth Huey") == 0)
		return instHuey;
	else if (strcmp(name, "ColorMunki Smile") == 0)
		return instSmile;
	else if (strcmp(name, "JETI specbos 1201") == 0)
		return instSpecbos1201;
	else if (strcmp(name, "JETI specbos") == 0)
		return instSpecbos;
	else if (strcmp(name, "JETI spectraval") == 0)
		return instSpectraval;
	else if (strcmp(name, "Klein K-10") == 0)
		return instKleinK10;
	else if (strcmp(name, "Image Engineering EX1") == 0)
		return instEX1;
	else if (strcmp(name, "SwatchMate Cube") == 0)
		return instSMCube;
	else if (strcmp(name, "Hughski ColorHug") == 0)
		return instColorHug;
	else if (strcmp(name, "Hughski ColorHug2") == 0)
		return instColorHug2;


#ifdef ENABLE_VTPGLUT
	if (strcmp(name, "Lumagen Radiance") == 0)
		return devRadiance;

	if (strcmp(name, "Q, Inc Prisma") == 0)
		return devPrisma;
#endif



	return instUnknown;
}

#ifdef ENABLE_USB

/* Given a USB vendor and product ID, */
/* return the matching instrument type, or */
/* instUnknown if none match. */
/* If nep == 0, do preliminary match just on vid & pid */
instType inst_usb_match(
unsigned int idVendor,
unsigned int idProduct,
int nep) {					/* Number of end points */

	if (idVendor == 0x04DB) {
		if (idProduct == 0x005B)	/* Colorimtre HCFR */
			return instHCFR;
	}

	if (idVendor == 0x0670) {		/* Sequel Imaging */
		if (idProduct == 0x0001)	/* Monaco Optix / i1 Display 1 */
									/* Sequel Chroma 4 / i1 Display 1 */
			return instI1Disp1;
	}

	if (idVendor == 0x0765) {		/* X-Rite */
	  	if (idProduct == 0x5001)	/* HueyL (Lenovo W70DS Laptop ?) */
			return instHuey;
	  	if (idProduct == 0x5010)	/* HueyL (Lenovo W530 Laptop ?) */
			return instHuey;
	  	if (idProduct == 0x5020)	/* i1DisplayPro, ColorMunki Display (HID) */
			return instI1Disp3;
	  	if (idProduct == 0x6003)	/* ColorMinki Smile (aka. ColorMunki Display Lite) */
			return instSmile;
		if (idProduct == 0x6008)	/* ColorMunki i1Studio */
			return instColorMunki;
		if (idProduct == 0xD020)	/* DTP20 */
			return instDTP20;
		if (idProduct == 0xD092)	/* DTP92Q */
			return instDTP92;
	  	if (idProduct == 0xD094)	/* DTP94 */
			return instDTP94;
	}

	if (idVendor == 0x085C) {		/* ColorVision */
		if (idProduct == 0x0100)	/* ColorVision Spyder1 */
			return instSpyder1;
		if (idProduct == 0x0200)	/* ColorVision Spyder2 */
			return instSpyder2;
		if (idProduct == 0x0300)	/* DataColor Spyder3 */
			return instSpyder3;
		if (idProduct == 0x0400)	/* DataColor Spyder4 */
			return instSpyder4;
		if (idProduct == 0x0500)	/* DataColor Spyder5 */
			return instSpyder5;
		if (idProduct == 0x0A00)	/* DataColor SpyderX */
			return instSpyderX;
	}

	if (idVendor == 0x0971) {		/* Gretag Macbeth */
		if (idProduct == 0x2000) {	/* i1 Pro or i1 Pro 2*/
			
			/* The i1pro2/rev E has 5 pipes - it has EP 85 */
			if (nep >= 5)
				return instI1Pro2;
			else
				return instI1Pro;
		}
		if (idProduct == 0x2001)	/* i1 Monitor */
			return instI1Monitor;
		if (idProduct == 0x2003)	/* i1 Display 2 */
			return instI1Disp2;
		if (idProduct == 0x2005)	/* Huey (HID) */
			return instHuey;
		if (idProduct == 0x2007)	/* ColorMunki */
			return instColorMunki;
	}

	if (idVendor == 0x2457) {		/* Image Engineering */
		if (idProduct == 0x4000) 	/* EX1 */
			return instEX1;
	}

	if ((idVendor == 0x04d8 && idProduct == 0xf8da)			/* Microchip & Hughski ColorHug (old) */
	 || (idVendor == 0x273f && idProduct == 0x1001)) {		/* Hughski & ColorHug Fmw. >= 0.1.20 */
		return instColorHug;
	}
	if (idVendor == 0x273f && idProduct == 0x1004) {		/* Hughski & ColorHug2 */
		return instColorHug2;
	}
	/* Add other instruments here */





	return instUnknown;
}

#endif /* ENABLE_USB */

/* Should deprecate the following. It should be replaced with a */
/* method in the instrument class that returns its configured spectrum, */
/* and the spectrum should be embedded in the .ti3 file, not the instrument */
/* name. */

/* Fill in an instruments illuminant spectrum. */
/* Return 0 on sucess, 1 if not not applicable. */
int inst_illuminant(xspect *sp, instType itype) {

	switch (itype) {
		case instDTP20:
			return standardIlluminant(sp, icxIT_A, 0);		/* 2850K */

		case instDTP22:
			return standardIlluminant(sp, icxIT_A, 0);		/* 2850K */

		case instDTP41:
			return standardIlluminant(sp, icxIT_A, 0);		/* 2850K */

		case instDTP51:
			return standardIlluminant(sp, icxIT_A, 0);		/* 2850K */

		case instDTP92:
		case instDTP94:
			return 1;										/* Not applicable */

		/* Strictly the Spectrolino could have the UV or D65 filter on, */
		/* but since we're not currently passing this inform through, we */
		/* are simply assuming the default A type illuminant. */

		case instSpectrolino:
			return standardIlluminant(sp, icxIT_A, 0);		/* Standard A type assumed */

		case instSpectroScan:
			return standardIlluminant(sp, icxIT_A, 0);		/* Standard A type assumed */

		case instSpectroScanT:
			return standardIlluminant(sp, icxIT_A, 0);		/* Standard A type assumed */

#ifndef SALONEINSTLIB
		case instSpectrocam:
			return standardIlluminant(sp, icxIT_Spectrocam, 0);   /* Spectrocam Xenon Lamp */
#endif
		case instI1Disp1:
			return 1;										/* Not applicable */

		case instI1Disp2:
			return 1;										/* Not applicable */

		case instI1Disp3:
			return 1;										/* Not applicable */

		case instI1Monitor:
			return 1;										/* Not applicable */

		case instI1Pro:
		case instI1Pro2:
			return standardIlluminant(sp, icxIT_A, 0);		/* Standard A type assumed */

		case instColorMunki:
			return 1;										/* No U.V. */

		case instHCFR:
			return 1;										/* Not applicable */

		case instSpyder1:
		case instSpyder2:
		case instSpyder3:
		case instSpyder4:
		case instSpyder5:
		case instSpyderX:
			return 1;										/* Not applicable */

		case instHuey:
			return 1;										/* Not applicable */

		case instSmile:
			return 1;										/* Not applicable */

		case instSpecbos1201:
		case instSpecbos:
		case instSpectraval:
			return 1;										/* Not applicable */

		case instKleinK10:
			return 1;										/* Not applicable */

		case instEX1:
			return 1;										/* Not applicable */

		case instSMCube:
			return 1;										/* Not applicable */


		case instColorHug:
		case instColorHug2:
			return 1;										/* Not applicable */


		default:
			break;
	}
	return 1;
}

/* ============================================================= */
/* XRGA support */

/* Return a string for the xcalstd enum */
char *xcalstd2str(xcalstd std) {
	switch(std) {
		case xcalstd_native:
			return "NATIVE";
		case xcalstd_xrdi:
			return "XRDI";
		case xcalstd_gmdi:
			return "GMDI";
		case xcalstd_xrga:
			return "XRGA";
		default:
			break;
	}
	return "None";
}

/* Parse a string to xcalstd, */
/* return xcalstd_none if not recognized */
xcalstd str2xcalstd(char *str) {
	if (strcmp(str, "NATIVE") == 0)
		return xcalstd_native;
	if (strcmp(str, "XRDI") == 0)
		return xcalstd_xrdi;
	if (strcmp(str, "GMDI") == 0)
		return xcalstd_gmdi;
	if (strcmp(str, "XRGA") == 0)
		return xcalstd_xrga;

	return xcalstd_none;
}



