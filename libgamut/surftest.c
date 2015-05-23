
/* 
 * surftest
 *
 * Do a torture test of the gamut surfacing algorithm in gamut.
 *
 * Author:  Graeme W. Gill
 * Date:    11/11/2006
 * Version: 1.00
 *
 * Copyright 2000, 2006 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "icc.h"
//#include "xicc.h"
#include "gamut.h"

#define DEF_POINTS 5
#define DEF_TPOINTS 50
#define DEF_HEIGHT 5.0

void usage(char *diag) {
	fprintf(stderr,"Do gamut surface torture test, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: surftest [options] npoints\n");
	if (diag != NULL)
		fprintf(stderr,"Diagnostic: %s\n",diag);
	fprintf(stderr," -v            Verbose\n");
	fprintf(stderr," -r            Random points\n");
	fprintf(stderr," -n            Don't add VRML axes or white/black point\n");
	fprintf(stderr," -h height     Height above sqhere (default %f)\n",DEF_HEIGHT);
	fprintf(stderr," -f            Do segemented maxima filtering (default is not)\n");
	fprintf(stderr," -t ntpoints   Number of test points (default %d)\n",DEF_TPOINTS);
	fprintf(stderr," npoints       Number of random points, default %d^3\n",DEF_POINTS);
	fprintf(stderr,"               Outputs surftest.wrl");
	fprintf(stderr,"\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	char out_name[100];
	int npoints = DEF_POINTS;
	int ntpoints = DEF_TPOINTS;
	double height = DEF_HEIGHT;
	gamut *gam;
	int rand = 0;
	int verb = 0;
	int doaxes = 1;
	int dofilt = 0;
	int i;

#ifdef NUMSUP_H
	error_program = "surftest";
#endif

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
				usage(NULL);

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}
			/* Random */
			else if (argv[fa][1] == 'r' || argv[fa][1] == 'R') {
				rand = 1;
			}
			/* No axis output */
			else if (argv[fa][1] == 'n' || argv[fa][1] == 'N') {
				doaxes = 0;
			}
			/* Segmented maxima filtering */
			else if (argv[fa][1] == 'f' || argv[fa][1] == 'F') {
				dofilt = 1;
			}
			/* Height */
			else if (argv[fa][1] == 'h' || argv[fa][1] == 'H') {
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -h");
				height = atof(na);
			}
			/* Number of test points */
			else if (argv[fa][1] == 't' || argv[fa][1] == 'T') {
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -t");
				ntpoints = atoi(na);
			}
			else 
				usage("Unknown flag");
		} else
			break;
	}

	if (fa < argc && argv[fa][0] != '-')
		npoints = atoi(argv[fa]);

	strcpy(out_name,"surftest.wrl");

	if (verb) {
		printf("Number of surface points = %d\n",npoints * npoints * npoints);
		printf("Number of test points = %d\n",ntpoints);
	}

	/* Creat a gamut object */
	if ((gam = new_gamut(0.0, 0, 0)) == NULL)
		error("Failed to create aa gamut object\n");

	if (dofilt == 0)
		gam->setnofilt(gam);

	if (rand) {
		npoints = npoints * npoints * npoints;

		/* Create and add our random test points */
		for (i = 0; i < npoints;) {
			int j;
			double pp[3], sum, rad;
	
			/* Make normalzed random direction vector on sphere radius 30  */
			for (sum = 0.0, j = 0; j < 3; j++) {
				double tt = d_rand(-1.0, 1.0);
				pp[j] = tt;
				sum += tt * tt;
			}
			if (sum < 1e-6)
				continue;
			sum = sqrt(sum);
			rad = d_rand(30.0, 30.0 + height);
			for (j = 0; j < 3; j++) {
				pp[j] /= sum;
				pp[j] *= rad;
			}
			pp[0] += 50.0;
//printf("~1 point %f %f %f\n",pp[0],pp[1],pp[2]);
//			if (verb) printf("\r%d",i+1); fflush(stdout);
			gam->expand(gam, pp);
			i++;
		}
//		if (verb) printf("\n");
	} else {
		int co[3];

		/* Create and add our gridded test points */
		for (co[0] = 0; co[0] < npoints; co[0]++) {
			for (co[1] = 0; co[1] < npoints; co[1]++) {
				for (co[2] = 0; co[2] < npoints; co[2]++) {
					double pp[3], sum, rad;
					int m, n;
	
#ifndef NEVER
					/* Make sure at least one coords are 0 & 1 */
					for (n = m = 0; m < 3; m++) {
						if (co[m] == 0 || co[m] == (npoints-1))
							n++;
					} 
					if (n < 1)
						continue;
#endif
					pp[0] = 2.0 * (co[0]/(npoints-1.0) - 0.5);
					pp[1] = 2.0 * (co[1]/(npoints-1.0) - 0.5);
					pp[2] = 2.0 * (co[2]/(npoints-1.0) - 0.5);
	
#ifdef NEVER
					/* Make normalzed random direction vector on sphere radius 30  */
					for (sum = 0.0, m = 0; m < 3; m++) {
						sum += pp[m] * pp[m];
					}
					if (sum < 1e-6)
						continue;
					sum = sqrt(sum);
#else

					sum = 1.0;
#endif /* NEVER */
					rad = d_rand(30.0, 30.0 + height);
					for (m = 0; m < 3; m++) {
						pp[m] /= sum;
						pp[m] *= rad;
					}
					pp[0] += 50.0;
//printf("~1 point %f %f %f\n",pp[0],pp[1],pp[2]);
					gam->expand(gam, pp);
				}
			}
		}
	}

	/* Write out the gamut surface */
	if (gam->write_vrml(gam, out_name, doaxes, 0))
		error ("write vrml failed on '%s'",out_name);

	if (verb)
		printf("Written out the gamut surface\n");

	/* Test the gamut surface */
	for (i = 0; i < ntpoints;) {
		int j;
		double pp[3], sum;
		double r, out[3];

		/* Make normalzed random direction vector on sphere  */
		for (sum = 0.0, j = 0; j < 3; j++) {
			double tt = d_rand(-1.0, 1.0);
			pp[j] = tt;
			sum += tt * tt;
		}
		if (sum < 1e-6)
			continue;
		sum = sqrt(sum);
		for (j = 0; j < 3; j++) {
			pp[j] /= sum;
		}

		/* Test surface */
		r = gam->radial(gam, out, pp);
		i++;
	}

	gam->del(gam);

	return 0;
}


