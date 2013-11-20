
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
#include "xspect.h"
#include "insttypes.h"
#include "conv.h"
#include "icoms.h"

/* NOTE NOTE NOTE: */
/* Need to add a new instrument to new_inst() in */
/* inst.c as well !!! */

/* Utility functions */

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
		case instSpyder2:
			return "Spyder2";
		case instSpyder3:
			return "Spyder3";
		case instSpyder4:
			return "Spyder4";
		case instHuey:
			return "Huey";
		case instSmile:
			return "Smile";
		case instSpecbos1201:
			return "specbos 1201";
		case instSpecbos:
			return "specbos";
		case instColorHug:
			return "ColorHug";
		default:
			break;
	}
	return "Unknown";
}

/* Return the long instrument identification name (static string) */
char *inst_name(instType itype) {
	switch (itype) {
		case instDTP20:
			return "Xrite DTP20";
		case instDTP22:
			return "Xrite DTP22";
		case instDTP41:
			return "Xrite DTP41";
		case instDTP51:
			return "Xrite DTP51";
		case instDTP92:
			return "Xrite DTP92";
		case instDTP94:
			return "Xrite DTP94";
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
			return "Xrite i1 DisplayPro, ColorMunki Display";
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
		case instSpyder2:
			return "ColorVision Spyder2";
		case instSpyder3:
			return "Datacolor Spyder3";
		case instSpyder4:
			return "Datacolor Spyder4";
		case instHuey:
			return "GretagMacbeth Huey";
		case instSmile:
			return "ColorMunki Smile";
		case instSpecbos1201:
			return "JETI specbos 1201";
		case instSpecbos:
			return "JETI specbos";
		case instColorHug:
			return "Hughski ColorHug";
		default:
			break;
	}
	return "Unknown Instrument";
}

/* Given a long instrument identification name, return the matching */
/* instType, or instUnknown if not matched */
instType inst_enum(char *name) {

	if (strcmp(name, "Xrite DTP20") == 0)
		return instDTP20;
	else if (strcmp(name, "Xrite DTP22") == 0)
		return instDTP22;
	else if (strcmp(name, "Xrite DTP41") == 0)
		return instDTP41;
	else if (strcmp(name, "Xrite DTP51") == 0)
		return instDTP51;
	else if (strcmp(name, "Xrite DTP92") == 0)
		return instDTP92;
	else if (strcmp(name, "Xrite DTP94") == 0)
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
	      || strcmp(name, "Xrite i1 Display") == 0)
		return instI1Disp2;
	else if (strcmp(name, "Xrite i1 DisplayPro") == 0
	      || strcmp(name, "ColorMunki Display") == 0)
		return instI1Disp3;
	else if (strcmp(name, "GretagMacbeth i1 Monitor") == 0)
		return instI1Monitor;
	else if (strcmp(name, "GretagMacbeth i1 Pro") == 0
	      || strcmp(name, "Xrite i1 Pro") == 0)
		return instI1Pro;
	else if (strcmp(name, "Xrite i1 Pro 2") == 0)
		return instI1Pro2;
	else if (strcmp(name, "X-Rite ColorMunki") == 0)
		return instColorMunki;
	else if (strcmp(name, "Colorimtre HCFR") == 0)
		return instHCFR;
	else if (strcmp(name, "ColorVision Spyder2") == 0)
		return instSpyder2;
	else if (strcmp(name, "Datacolor Spyder3") == 0)
		return instSpyder3;
	else if (strcmp(name, "Datacolor Spyder4") == 0)
		return instSpyder4;
	else if (strcmp(name, "GretagMacbeth Huey") == 0)
		return instHuey;
	else if (strcmp(name, "ColorMunki Smile") == 0)
		return instSmile;
	else if (strcmp(name, "JETI specbos 1201") == 0)
		return instSpecbos1201;
	else if (strcmp(name, "JETI specbos") == 0)
		return instSpecbos;
	else if (strcmp(name, "Hughski ColorHug") == 0)
		return instColorHug;


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
		if (idProduct == 0xD020)	/* DTP20 */
			return instDTP20;
		if (idProduct == 0xD092)	/* DTP92Q */
			return instDTP92;
	  	if (idProduct == 0xD094)	/* DTP94 */
			return instDTP94;
	}

	if (idVendor == 0x085C) {		/* ColorVision */
		if (idProduct == 0x0100)	/* ColorVision Spyder1 */
			return instSpyder2;		/* Alias to Spyder 2 */
		if (idProduct == 0x0200)	/* ColorVision Spyder2 */
			return instSpyder2;
		if (idProduct == 0x0300)	/* ColorVision Spyder3 */
			return instSpyder3;
		if (idProduct == 0x0400)	/* ColorVision Spyder4 */
			return instSpyder4;
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

	if ((idVendor == 0x04d8 && idProduct == 0xf8da)			/* Microchip & Hughski ColorHug (old) */
	 || (idVendor == 0x273f && idProduct == 0x1001)) {		/* Hughski & ColorHug Fmw. >= 0.1.20 */
		return instColorHug;
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

		case instSpyder2:
			return 1;										/* Not applicable */

		case instSpyder3:
			return 1;										/* Not applicable */

		case instSpyder4:
			return 1;										/* Not applicable */

		case instHuey:
			return 1;										/* Not applicable */

		case instSmile:
			return 1;										/* Not applicable */

		case instSpecbos1201:
		case instSpecbos:
			return 1;										/* Not applicable */

		case instColorHug:
			return 1;										/* Not applicable */


		default:
			break;
	}
	return 1;
}


