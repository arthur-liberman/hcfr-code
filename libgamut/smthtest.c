
/* 
 * nearsmth test code. Test the smoothed nearpoint routine.
 *
 * Author:  Graeme W. Gill
 * Date:    17/1/2002
 * Version: 1.00
 *
 * Copyright 2002, Graeme W. Gill
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
#if defined(__IBMC__) && defined(_M_IX86)
#include <float.h>
#endif

#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "rspl.h"
#include "gamut.h"
#include "nearsmth.h"
#include "vrml.h"
#include "ui.h"

double m21po[3] = { 2.0, 1.0, 2.0 };    /* Many to 1 filter mixing power LCh (theoretically 2) */

/* Mapping weights */
gammapweights weights[] = {
	{
		gmm_default,	/* Non hue specific defaults */
		{				/* Cusp alignment control */
			{
				0.1,	/* Cusp luminance alignment weighting 0 = none, 1 = full */
				0.1,	/* Cusp chroma alignment weighting    0 = none, 1 = full */
				0.2		/* Cusp hue alignment weighting       0 = none, 1 = full */
			},
			1.00		/* Chroma expansion 1 = none */
		},
		{			/* Radial weighting */
			0.0,	/* Radial error overall weight, 0 + */
			0.5,	/* Radial hue dominance vs l+c, 0 - 1 */
			0.5		/* Radial l dominance vs, c, 0 - 1 */
		},
		{			/* Weighting of absolute error of destination from source */
			1.0,	/* Absolute error overall weight */
			0.5,	/* Hue dominance vs l+c, 0 - 1 */

			0.9,	/* Light l dominance vs, c, 0 - 1 */
			0.9,	/* Medium l dominance vs, c, 0 - 1 */
			0.9,	/* Dark l dominance vs, c, 0 - 1 */

			0.5,	/* l/c dominance breakpoint, 0 - 1 */
			0.0,	/* l dominance exageration, 0+ */
			0.0		/* c dominance exageration, 0+ */
		},
		{			/* Relative vector  smoothing */
			30.0, 20.0	/* Relative Smoothing radius L* H* */
		},
		{		/* Weighting of excessive compression error, which is */
				/* the src->dst vector length over the available dst depth. */
				/* The depth is half the distance to the intersection of the */
				/* vector to the other side of the gamut. (doesn't get triggered much ?) */
			100.0,		/* Compression depth weight */
			100.0		/* Expansion depth weight */
		}
	}
};

#define OVERSHOOT 1.0

void usage(void) {
	fprintf(stderr,"Create smoothed near mapping between two gamuts, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: smthtest [options] ingamut outgamut diag_vrml\n");
	fprintf(stderr," -v            Verbose\n");
//	fprintf(stderr," -s nearf      Absolute delta E weighting\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	char *xl;
	char in_name[100];
	char out_name[100];
	char diag_name[100];
	int verb = 0;
	double nearf = 1.0;		/* Absolute delta E weightign */
	datai il, ih;			/* rspl input range */
	datao ol, oh;			/* rspl output range */

	gamut *gin, *gout;		/* Input and Output gamuts */
	nearsmth *nsm;			/* Returned list of near smooth points */
	int nnsm;				/* Number of near smoothed points */
	int doaxes = 1;
	vrml *wrl;				/* VRML/X3D output file */

	gammapweights xweights[14];

	int i;

#if defined(__IBMC__) && defined(_M_IX86)
	_control87(EM_UNDERFLOW, EM_UNDERFLOW);
#endif

	error_program = argv[0];

	if (argc < 3)
		usage();

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')	{	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1) < argc) {
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

			/* Smoothing factor */
			else if (argv[fa][1] == 's' || argv[fa][1] == 'S') {
				fa = nfa;
				if (na == NULL) usage();
				nearf = atof(na);
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
	/* read the output device gamut */

	gout = new_gamut(0.0, 0, 0);

	if ((xl = strrchr(out_name, '.')) == NULL) { /* Add .gam extention if there isn't one */
		xl = out_name + strlen(out_name);
		strcpy(xl,".gam");
	}

	if (gout->read_gam(gout, out_name))
		error("Reading output gamut failed");

	/* - - - - - - - - - - - - - - - - - - - */

	il[0] = ol[0] = 0.0;
	il[1] = ol[1] = -128.0;
	il[2] = ol[2] = -128.0;
	ih[0] = oh[0] = 100.0;
	ih[1] = oh[1] = 128.0;
	ih[2] = oh[2] = 128.0;
	
	/* Convert from compact to explicit hextant weightings */
	expand_weights(xweights, weights);

	/* Create the near point mapping */
	nsm = near_smooth(verb, &nnsm, gin, gin, gout, 0, 0, NULL, xweights,
	           0.1, 0.1, 1, 1, 2.0, 17, 10.0, il, ih, ol, oh);
	if (nsm == NULL)
		error("Creating smoothed near points failed");

	/* Output the src to smoothed near point vectors */
	if ((xl = strrchr(diag_name, '.')) == NULL) /* Clear extension */
		xl = diag_name + strlen(diag_name);
	xl[0] = '\000';

	if ((wrl = new_vrml(diag_name, doaxes, vrml_lab)) != 0) {
		error("new_vrml failed for '%s%s'",diag_name,vrml_ext());
	}

	wrl->start_line_set(wrl, 0);

	for (i = 0; i < nnsm; i++) {
		wrl->add_vertex(wrl, 0, nsm[i].sv);			/* Source gamut point */
		wrl->add_vertex(wrl, 0, nsm[i].dv);			/* Smoother destination value */

//		add_vertex(wrl, nsm[i].drv);		/* Radial points */
	} 
	wrl->make_lines(wrl, 0, 2);

	wrl->del(wrl);		/* Write file */

	/* Clean up */
	free_nearsmth(nsm, nnsm);

	gout->del(gout);
	gin->del(gin);

	return 0;
}


