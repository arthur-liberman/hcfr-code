
/* 
 * Gamut mapping test code. Test the gamut mapping library.
 *
 * Author:  Graeme W. Gill
 * Date:    29/10/00
 * Version: 1.00
 *
 * Copyright 2000, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* TTBD:
 *
 */

#undef DEBUG		/* test a single value out */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "xicc.h"
#include "gamut.h"
#include "rspl.h"
#include "gammap.h"
#include "vrml.h"
#include "ui.h"

void usage(void) {
	fprintf(stderr,"Map bteween two gamuts, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: maptest [options] ingamut outgamut diag_gamut\n");
	fprintf(stderr," -v                Verbose\n");
	fprintf(stderr," -s                Do saturation style expand/compress\n");
	fprintf(stderr," -i imggamut.gam   Use an image gamut\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa, nfa, mfa;
	char *xl;
	char in_name[100];
	char img_name[MAXNAMEL+1] = "";
	char out_name[100];
	char diag_name[100];
	int sat = 0;
	int verb = 0;
	gammap *map;			/* Regular split gamut mapping */
	icxGMappingIntent gmi;
	char gammapwrl[30] = "gammap";	/* Diagnostic file name */

	gamut *gin, *gout;		/* Input and Output gamuts */
	gamut *gimg = NULL;		/* Optional image gamut */

	error_program = argv[0];

	strcat(gammapwrl, vrml_ext());

	if (argc < 3)
		usage();

	/* Process the arguments */
	mfa = 3;        /* Minimum final arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')	{	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1+mfa) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage();

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}

			/* Saturation */
			else if (argv[fa][1] == 's' || argv[fa][1] == 'S') {
				sat = 1;
			}

			/* Image gamut */
			else if (argv[fa][1] == 'i' || argv[fa][1] == 'I') {
				if (na == NULL) usage();
				fa = nfa;
				strncpy(img_name,na,MAXNAMEL); img_name[MAXNAMEL] = '\000';
			}

			else 
				usage();
		} else
			break;
	}

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(in_name,argv[fa++]);

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(out_name,argv[fa++]);

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(diag_name,argv[fa++]);

	/* - - - - - - - - - - - - - - - - - - - */
	/* read the input device gamut */

	gin = new_gamut(0.0, 0, 0);

	if ((xl = strrchr(in_name, '.')) == NULL) {	/* Add .gam extention if there isn't one */
		xl = in_name + strlen(in_name);
		strcpy(xl,".gam");
	}

	if (gin->read_gam(gin, in_name))
		error("Reading input gamut failed");

	/* - - - - - - - - - - - - - - - - - - - */
	/* read the optional image gamut */

	if (img_name[0] != '\000') {

		gimg = new_gamut(0.0, 0, 0);

		if ((xl = strrchr(img_name, '.')) == NULL) { /* Add .gam extention if there isn't one */
			xl = img_name + strlen(img_name);
			strcpy(xl,".gam");
		}
	
		if (gimg->read_gam(gimg, img_name))
			error("Reading image gamut failed");
	} else {
		gimg = gin;
	}

	/* - - - - - - - - - - - - - - - - - - - */
	/* read the output device gamut */

	gout = new_gamut(0.0, 0, 0);

	if ((xl = strrchr(out_name, '.')) == NULL) { /* Add .gam extention if there isn't one */
		xl = out_name + strlen(out_name);
		strcpy(xl,".gam");
	}

	if (gout->read_gam(gout, out_name))
		error("Reading output gamut failed");

	/* - - - - - - - - - - - - - - - - - - - */

	/* Create the gamut mapping */
	gmi.usecas = 1;			/* Abs. L*a*b* */
	gmi.usemap = 1;
	gmi.greymf = 1.0;		/* Gray axis hue matching factor, 0.0 - 1.0 */
	gmi.glumwcpf = 1.0;		/* Grey axis luminance white compression factor, 0.0 - 1.0 */
	gmi.glumwexf = 1.0;		/* Grey axis luminance white expansion factor, 0.0 - 1.0 */
	gmi.glumbcpf = 1.0;		/* Grey axis luminance black compression factor, 0.0 - 1.0 */
	gmi.glumbexf = 1.0;		/* Grey axis luminance black expansion factor, 0.0 - 1.0 */
	gmi.glumknf = 0.7;		/* Gray axis luminance knee factor, 0.0 - 1.0 */
	gmi.bph = gmm_bendBP;	/* Extend and bend */
	gmi.gamcpf = 1.0;		/* Gamut compression factor, 0.0 - 1.0 */
	if (sat)
		gmi.gamexf = 1.0;		/* Gamut expansion factor, 0.0 - 1.0 */
	else
		gmi.gamexf = 0.0;		/* Gamut expansion factor, 0.0 - 1.0 */
	gmi.gamcknf = 0.1;		/* Gamut comp. knee factor, 0.0 - 1.0 */
	gmi.gamxknf = 0.1;		/* Gamut exp. knee factor, 0.0 - 1.0 */
	if (sat) {
		gmi.gampwf = 0.0;		/* Gamut Perceptual Map weighting factor, 0.0 - 1.0 */
		gmi.gamswf = 1.0;		/* Gamut Saturation Map weighting factor, 0.0 - 1.0 */
	} else {
		gmi.gampwf = 1.0;		/* Gamut Perceptual Map weighting factor, 0.0 - 1.0 */
		gmi.gamswf = 0.0;		/* Gamut Saturation Map weighting factor, 0.0 - 1.0 */
	}
	gmi.satenh = 0.0;		/* Saturation enhancement factor */
	gmi.desc = "mapetest";
	gmi.icci = 0;

	map = new_gammap(
		verb,
		gin,			/* Source gamut */
		gimg,			/* Image gamut */
		gout,			/* Destination gamut */
		&gmi,
		0, 0, 			/* Normal black points */
		0,				/* Normal CMY cusp mapping */
		0,				/* No relative weighting override */
		17,				/* rspl resolution of 17 */
		NULL,			/* No input range override */
		NULL,
		gammapwrl		/* Diagnostic plot */
	);

	if (map == NULL) {
		error("new_gammap() failed\n");
	}

	if (verb)
		printf("Got gamut mapping\n");

	/* Transform the input gamut by the gamut mapping */
	{	/* Bad - delving inside gamut! */
		int i;
		for (i = 0; i < gin->nv; i++) {

			/* transform the input gamutboundary points */
			map->domap(map, gin->verts[i]->p, gin->verts[i]->p);
		}
	}
	
	/* Output a transformed gamut */
	if ((xl = strrchr(diag_name, '.')) == NULL) { /* Add .gam extention if there isn't one */
		xl = diag_name + strlen(diag_name);
		strcpy(xl,".gam");
	}
	gin->write_gam(gin, diag_name);

	/* Clean up */
	gout->del(gout);
	gin->del(gin);
	if (gimg != gin)
		gimg->del(gimg);

	return 0;
}

