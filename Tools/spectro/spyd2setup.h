#ifndef SPYD2SETUP_H

/* 
 * Argyll Color Correction System
 *
 * ColorVision Spyder 2 related software.
 *
 * Author: Graeme W. Gill
 * Date:   19/10/2006
 *
 * Copyright 2006 - 2013, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

/* This file is only included in top utilities that need to */
/* be able to access the Spyder 2 colorimeter. This provides */
/* a mechanism for ensuring that only such utilities load the */
/* proprietary Spyder firmware, as well as providing a means to */
/* detect if the spyder driver is going to be funcional. */

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int *spyder2_pld_size;			/* in spyd2.c */
extern unsigned char *spyder2_pld_bytes;

#ifdef __cplusplus
}
#endif

/* Return 0 if Spyder 2 firmware is not available */
/* Return 1 if Spyder 2 firmware is available from an external file */
/* Return 2 if Spyder 2 firmware is part of this executable */
int setup_spyd2() {
#ifdef ENABLE_USB
	static int loaded = 0;		/* Was loaded from a file */
	char **bin_paths = NULL;
	int no_paths = 0;
	unsigned int size;//, rsize;
	FILE *fp;
//	int i;

	/* Spyder 2 Colorimeter Xilinx XCS05XL firmware pattern. */
	/* This is a placeholder in the distributed files. */
	/* It could be replaced with the actual end users firmware */
	/* by using the spyd2trans utility, but normally the spyd2PLD.bin */
	/* file is loaded instead. */

#include "spyd2PLD.h"

	spyder2_pld_size = &pld_size; 
	spyder2_pld_bytes = pld_bytes; 

	/* If no firmware compiled in, see if there is a file to load from. */
	if ((pld_size == 0 || pld_size == 0x11223344) && loaded == 0) {
		

		for (;;) {	/* So we can break out */
			if ((no_paths = xdg_bds(NULL, &bin_paths, xdg_data, xdg_read, xdg_user,
	                   "ArgyllCMS/spyd2PLD.bin" XDG_FUDGE "color/spyd2PLD.bin"
)) < 1) {
				a1logd(g_log, 1, "setup_spyd2: failed to find PLD file\n");
				break;
		}

			/* open binary file */
#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
			if ((fp = fopen(bin_paths[0],"rb")) == NULL)
#else
			if ((fp = fopen(bin_paths[0],"r")) == NULL)
#endif
				break;
			xdg_free(bin_paths, no_paths);

			/* Figure out how file it is */
			if (fseek(fp, 0, SEEK_END)) {
				fclose(fp);
				break;
			}
			size = (unsigned long)ftell(fp);

			if (size > pld_space) 
				size = pld_space;
		
			if (fseek(fp, 0, SEEK_SET)) {
				fclose(fp);
				break;
			}
		
			if (fread(pld_bytes, 1, size, fp) != size) {
				fclose(fp);
				break;
			}
			pld_size = size;
			loaded = 1;			/* We've loaded it from a file */
//			a1logd(g_log,0,"Spyder2 pld bytes = 0x%x 0x%x 0x%x 0x%x\n",pld_bytes[0], pld_bytes[1], pld_bytes[2], pld_bytes[3]);
			fclose(fp);
			break;
		}
	}

	if (pld_size != 0 && pld_size != 0x11223344) {
		if (loaded)
			return 1;			/* Was loaded from a file */
		return 2;				/* Was compiled in */
	}
#endif /* ENABLE_USB */
	return 0;					/* Not available */
}

#define SPYD2SETUP_H
#endif /* SPYD2SETUP_H */
