
/* 
 * Argyll Color Correction System
 * Create a linear display calibration file.
 *
 * Author: Graeme W. Gill
 * Date:   15/11/2005
 *
 * Copyright 1996 - 2007 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#undef DEBUG

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

#include "sort.h"

#include <stdarg.h>

#if defined (NT)
#include <conio.h>
#endif

void
usage(int level) {
	int i;
	fprintf(stderr,"Create a synthetic calibration file, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: synthcal [-options] outfile\n");
	fprintf(stderr," -t N            i = input, o = output, d = display (default)\n");
	fprintf(stderr," -d col_comb     choose colorant combination from the following (default 3):\n");
	for (i = 0; ; i++) {
		char *desc; 
		if (icx_enum_colorant_comb(i, &desc) == 0)
			break;
		fprintf(stderr,"                 %d: %s\n",i,desc);
	}
	fprintf(stderr," -D colorant     Add or delete colorant from combination:\n");
	if (level == 0)
		fprintf(stderr,"                 (Use -?? to list known colorants)\n");
	else {
		fprintf(stderr,"                 %d: %s\n",0,"Additive");
		for (i = 0; ; i++) {
			char *desc; 
			if (icx_enum_colorant(i, &desc) == 0)
				break;
			fprintf(stderr,"                 %d: %s\n",i+1,desc);
		}
	}
	fprintf(stderr," -o o1,o2,o3,    Set non-linear curve offset (default 0.0)\n");
	fprintf(stderr," -s s1,s2,s3,    Set non-linear curve scale (default 1.0)\n");
	fprintf(stderr," -p p1,p2,p3,    Set non-linear curve powers (default 1.0)\n");
	fprintf(stderr," -E description  Set the profile dEscription string\n");
	fprintf(stderr," outfile         Base name for output .cal file\n");
	exit(1);
	}

int main(int argc, char *argv[])
{
	int fa,nfa;							/* current argument we're looking at */
	int j;
	int verb = 0;
	char outname[MAXNAMEL+1] = { 0 };	/* Output cgats file base name */
	char *profDesc = NULL;		/* Description */ 
	int devtype = 2;			/* debice type, 0 = in, 1 = out, 2 = display */
	inkmask devmask = 0;		/* ICX ink mask of device space */
	int devchan;				/* Number of chanels in device space */
	char *ident;				/* Ink combination identifier (includes possible leading 'i') */
	char *bident;				/* Base ink combination identifier */
	double off[MAX_CHAN];		/* Output offset */
	double sca[MAX_CHAN];		/* Output scale */
	double gam[MAX_CHAN];		/* Gamma applied */

	for (j = 0; j < MAX_CHAN; j++)
		off[j] = 0.0, sca[j] = gam[j] = 1.0;

	error_program = "synthcal";
	if (argc <= 1)
		usage(0);

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')		/* Look for any flags */
			{
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1) < argc) {
					if (argv[fa+1][0] != '-')
						{
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?' || argv[fa][1] == '-') {
				if (argv[fa][2] == '?' || argv[fa][2] == '-')
					usage(1);
				usage(0);
			}

			else if (argv[fa][1] == 'v')
				verb = 1;

			/* Select the device type */
			else if (argv[fa][1] == 't' || argv[fa][1] == 'T') {
				fa = nfa;
				if (na == NULL) usage(0);
				if (na[0] == 'i' || na[0] == 'I')
					devtype = 0;
				else if (na[0] == 'o' || na[0] == 'O')
					devtype = 1;
				else if (na[0] == 'd' || na[0] == 'D')
					devtype = 2;
				else
					usage(0);
			}

			/* Select the ink enumeration */
			else if (argv[fa][1] == 'd') {
				int ix;
				fa = nfa;
				if (na == NULL) usage(0);
				ix = atoi(na);
				if (ix == 0 && na[0] != '0')
					usage(0);
				if ((devmask = icx_enum_colorant_comb(ix, NULL)) == 0)
					usage(0);
			}

			/* Toggle the colorant in ink combination */
			else if (argv[fa][1] == 'D') {
				int ix, tmask;
				fa = nfa;
				if (na == NULL) usage(0);
				ix = atoi(na);
				if (ix == 0 && na[0] != '0')
					usage(0);
				if (ix == 0)
					tmask = ICX_ADDITIVE;
				else
					if ((tmask = icx_enum_colorant(ix-1, NULL)) == 0)
						usage(0);
				devmask ^= tmask;
			}

			/* curve offset */
			else if (argv[fa][1] == 'o' || argv[fa][1] == 'O') {
				fa = nfa;
				if (na == NULL) usage(0);
				for (j = 0; j < MAX_CHAN; j++) {
					int i;
					for (i = 0; ; i++) {
						if (na[i] == ','){
							na[i] = '\000';
							break;
						}
						if (na[i] == '\000') {
							i = 0;
							break;
						}
					}
					off[j] = atof(na);
					if (i == 0)
						break;
					na += i+1;
				}
			}

			/* curve scale */
			else if (argv[fa][1] == 's' || argv[fa][1] == 'S') {
				fa = nfa;
				if (na == NULL) usage(0);
				for (j = 0; j < MAX_CHAN; j++) {
					int i;
					for (i = 0; ; i++) {
						if (na[i] == ','){
							na[i] = '\000';
							break;
						}
						if (na[i] == '\000') {
							i = 0;
							break;
						}
					}
					sca[j] = atof(na);
					if (i == 0)
						break;
					na += i+1;
				}
			}

			/* curve power */
			else if (argv[fa][1] == 'p' || argv[fa][1] == 'P') {
				fa = nfa;
				if (na == NULL) usage(0);
				for (j = 0; j < MAX_CHAN; j++) {
					int i;
					for (i = 0; ; i++) {
						if (na[i] == ','){
							na[i] = '\000';
							break;
						}
						if (na[i] == '\000') {
							i = 0;
							break;
						}
					}
					gam[j] = atof(na);
					if (i == 0)
						break;
					na += i+1;
				}
			}

			/* Profile Description */
			else if (argv[fa][1] == 'E') {
				fa = nfa;
				if (na == NULL) usage(0);
				profDesc = na;
			}


			else 
				usage(0);
		} else
			break;
	}

	/* Get the file name argument */
	if (fa >= argc || argv[fa][0] == '-') usage(0);
	strcpy(outname,argv[fa]);
	if (strlen(outname) < 4 || strcmp(".cal",outname + strlen(outname)-4) != 0)
		strcat(outname,".cal");

	/* Implement defaults */
	if (devmask == 0) {
		if (devtype == 0 || devtype == 2)
			devmask = ICX_RGB;
		else
			devmask = ICX_CMYK;
	}

	ident = icx_inkmask2char(devmask, 1); 
	bident = icx_inkmask2char(devmask, 0); 
	devchan = icx_noofinks(devmask);

	if (verb) {
		if (devtype == 0)
			printf("Device type: input\n");
		else if (devtype == 1)
			printf("Device type: output\n");
		else
			printf("Device type: display\n");

		printf("Colorspace: %s\n", ident);

		printf("Curve parameters:\n");
		for (j = 0; j < devchan; j++)
			printf("off[%d] = %f, sca[%d] = %f, gam[%d] = %f\n",j,off[j],j,sca[j],j, gam[j]);
	}

	/* Write out the resulting calibration file */
	{
		int i, j, calres = 256;	/* 256 steps in calibration */
		cgats *ocg;			/* output cgats structure */
		time_t clk = time(0);
		struct tm *tsp = localtime(&clk);
		char *atm = asctime(tsp);	/* Ascii time */
		cgats_set_elem *setel;		/* Array of set value elements */
		int nsetel = 0;
		char buf[200];

		ocg = new_cgats();				/* Create a CGATS structure */
		ocg->add_other(ocg, "CAL"); 	/* our special type is Calibration file */

		ocg->add_table(ocg, tt_other, 0);	/* Add a table for RAMDAC values */
		ocg->add_kword(ocg, 0, "DESCRIPTOR", "Argyll Device Calibration Curves",NULL);
		ocg->add_kword(ocg, 0, "ORIGINATOR", "Argyll synthcal", NULL);
		atm[strlen(atm)-1] = '\000';	/* Remove \n from end */
		ocg->add_kword(ocg, 0, "CREATED",atm, NULL);

		if (devtype == 0)
			ocg->add_kword(ocg, 0, "DEVICE_CLASS","INPUT", NULL);
		else if (devtype == 1)
			ocg->add_kword(ocg, 0, "DEVICE_CLASS","OUTPUT", NULL);
		else
			ocg->add_kword(ocg, 0, "DEVICE_CLASS","DISPLAY", NULL);

		ocg->add_kword(ocg, 0, "COLOR_REP", ident, NULL);

		if (profDesc != NULL)
			ocg->add_kword(ocg, 0, "DESCRIPTION", profDesc, NULL);

		sprintf(buf, "%s_I",bident);
		ocg->add_field(ocg, 0, buf, r_t);
		nsetel++;
		for (j = 0; j < devchan; j++) {
			inkmask imask = icx_index2ink(devmask, j);
			sprintf(buf, "%s_%s",bident,icx_ink2char(imask));
			ocg->add_field(ocg, 0, buf, r_t);
			nsetel++;
		}
		if ((setel = (cgats_set_elem *)malloc(sizeof(cgats_set_elem) * nsetel)) == NULL)
			error("Malloc failed!");

		for (i = 0; i < calres; i++) {
			double vv = i/(calres-1.0);

			setel[0].d = vv;
			for (j = 0; j < devchan; j++) {
				setel[j+1].d = off[j] + sca[j] * pow(vv, gam[j]);
				if (setel[j+1].d < 0.0)
					setel[j+1].d = 0.0;
				else if (setel[j+1].d > 1.0)
					setel[j+1].d = 1.0;
			}

			ocg->add_setarr(ocg, 0, setel);
		}

		free(setel);

		if (ocg->write_name(ocg, outname))
			error("Write error : %s",ocg->err);

		ocg->del(ocg);		/* Clean up */
	}
	return 0;
}


