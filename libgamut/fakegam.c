
/* 
 * Gamut test code. Generate a fake gamut surface
 *
 * Author:  Graeme W. Gill
 * Date:    23/10/2008
 * Version: 1.00
 *
 * Copyright 2008, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* TTBD:
 *
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
#include "gamut.h"

int get_value(double val[3]);

#define DEF_POINTS 10

void usage(char *diag) {
	fprintf(stderr,"Generate a test gamut Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: gamtest [options] outputname\n");
	if (diag != NULL)
		fprintf(stderr,"Diagnostic: %s\n",diag);
	fprintf(stderr," -v                Verbose\n");
	fprintf(stderr," -n num            Number of points to use (default %d)\n",DEF_POINTS);
	fprintf(stderr," -c rad            Create a cube\n");
	fprintf(stderr," -p rad            Create a sphere\n");
	fprintf(stderr," -s w,d,h          Create a sphereoid\n");
	fprintf(stderr," -b w,d,h          Create a box\n");
	fprintf(stderr," -S nw,pw,nd,pd,nh,ph  Create an asymetric sphereoid\n");
	fprintf(stderr," -B nw,pw,nd,pd,nh,ph  Create an asymetric box\n");
	fprintf(stderr," -o x,y,z          Offset the shape\n");
	fprintf(stderr," -w                Create WRL\n");
	fprintf(stderr," -x                No WRL axes\n");
	fprintf(stderr,"\n");
	exit(1);
}


int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	char *xl, out_name[500];
	int npoints = DEF_POINTS;
	gamut *gam;
	int shape = 1;			/* 0 = sphereoid, 1 = box */
	double bsize[6] = { 20.0, 20.0, 20.0, 20.0, 20.0, 20.0 };	/* box/spheroid size */
	double goff[3] = { 0.0, 0.0, 0.0 };		/* Gamut position offset */
	double co[3];
	double wp[3] = { -1e6, 0.0, 0.0 }, bp[3] = { 1e6, 0.0, 0.0 };
	int verb = 0;
	int dowrl = 0;
	int doaxes = 1;
	int i;

#ifdef NUMSUP_H
	error_program = "fakegam";
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
			/* VRML */
			else if (argv[fa][1] == 'w' || argv[fa][1] == 'W') {
				dowrl = 1;
			}
			/* No axis output */
			else if (argv[fa][1] == 'x' || argv[fa][1] == 'X') {
				doaxes = 0;
			}
			/* Sphere */
			else if (argv[fa][1] == 'p') {
				double radius;
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -h");
				radius = atof(na);
				bsize[0] = bsize[2] = bsize[4] = 2.0 * radius;
				bsize[1] = bsize[3] = bsize[5] = 2.0 * radius;
				shape = 0;
			}
			/* Cube */
			else if (argv[fa][1] == 'c' || argv[fa][1] == 'C') {
				double radius;
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -h");
				radius = atof(na);
				bsize[0] = bsize[2] = bsize[4] = 2.0 * radius;
				bsize[1] = bsize[3] = bsize[5] = 2.0 * radius;
				shape = 1;
			}
			/* Sphereoid */
			else if (argv[fa][1] == 's') {
				if (na == NULL) usage("Expect parameter to -b");

				fa = nfa;
				if (sscanf(na, " %lf , %lf , %lf ",&bsize[1], &bsize[3], &bsize[5]) == 3) {
					bsize[0] = bsize[1];
					bsize[2] = bsize[3];
					bsize[4] = bsize[5];
				} else
					usage("Couldn't parse sphereoid size (-b) values");
				shape = 0;
			}
			/* Box */
			else if (argv[fa][1] == 'b') {
				if (na == NULL) usage("Expect parameter to -b");

				fa = nfa;
				if (sscanf(na, " %lf , %lf , %lf ",&bsize[1], &bsize[3], &bsize[5]) == 3) {
					bsize[0] = bsize[1];
					bsize[2] = bsize[3];
					bsize[4] = bsize[5];
				} else
					usage("Couldn't parse box size (-b) values");
				shape = 1;
			}
			/* Asymetric Sphereoid */
			else if (argv[fa][1] == 'S') {
				if (na == NULL) usage("Expect parameter to -S");

				fa = nfa;
				if (sscanf(na, " %lf , %lf , %lf , %lf , %lf , %lf ",
				     &bsize[0], &bsize[1], &bsize[2], &bsize[3], &bsize[4], &bsize[5]) == 6) {
					bsize[0] *= 2.0;
					bsize[1] *= 2.0;
					bsize[2] *= 2.0;
					bsize[3] *= 2.0;
					bsize[4] *= 2.0;
					bsize[5] *= 2.0;
				} else
					usage("Couldn't parse sphereoid size (-S) values");
				shape = 0;
			}
			/* Asymetric Box */
			else if (argv[fa][1] == 'B') {
				if (na == NULL) usage("Expect parameter to -B");

				fa = nfa;
				if (sscanf(na, " %lf , %lf , %lf , %lf , %lf , %lf ",
				     &bsize[0], &bsize[1], &bsize[2], &bsize[3], &bsize[4], &bsize[5]) == 6) {
					bsize[0] *= 2.0;
					bsize[1] *= 2.0;
					bsize[2] *= 2.0;
					bsize[3] *= 2.0;
					bsize[4] *= 2.0;
					bsize[5] *= 2.0;
				} else
					usage("Couldn't parse box size (-B) values");
				shape = 1;
			}
			/* Number of construction points */
			else if (argv[fa][1] == 'n' || argv[fa][1] == 'N') {
				fa = nfa;
				if (na == NULL) usage("No parameter after flag -t");
				npoints = atoi(na);
			}
			/* Gamut offset */
			else if (argv[fa][1] == 'o' || argv[fa][1] == 'O') {
				if (na == NULL) usage("Expect parameter to -o");

				fa = nfa;
				if (sscanf(na, " %lf , %lf , %lf ",&goff[0], &goff[1], &goff[2]) == 3) {
				} else
					usage("Couldn't parse gamut offset (-o) value");
			}
			else 
				usage("Unknown flag");
		} else
			break;
	}

	if (fa >= argc || argv[fa][0] == '-') usage("Output filename expected");
	strcpy(out_name,argv[fa++]);

	if ((xl = strrchr(out_name, '.')) == NULL) {	/* Add .gam extention if there isn't one */
		xl = out_name + strlen(out_name);
		strcpy(xl,".gam");
	}

	if (verb) {
		printf("Number of construction points = %d\n",npoints * npoints * npoints);
	}

	/* Creat a gamut object */
	if ((gam = new_gamut(5.0, 0, 0)) == NULL)
		error("Failed to create aa gamut object\n");

	/* Create and add our gridded test points */
	for (co[0] = 0; co[0] < npoints; co[0]++) {
		for (co[1] = 0; co[1] < npoints; co[1]++) {
			for (co[2] = 0; co[2] < npoints; co[2]++) {
				double pp[3], sum, rad;
				int m, n;
	
				/* Make sure at least one coords are 0 & 1 */
				for (n = m = 0; m < 3; m++) {
					if (co[m] == 0 || co[m] == (npoints-1))
						n++;
				} 
				if (n < 1)
					continue;

				if (shape == 0) {			/* Sphereoid */

					pp[0] = (co[0]/(npoints-1.0) - 0.5);
					pp[1] = (co[1]/(npoints-1.0) - 0.5);
					pp[2] = (co[2]/(npoints-1.0) - 0.5);

					/* vector length */
					for (sum = 0.0, m = 0; m < 3; m++) {
						sum += pp[m] * pp[m];
					}
					if (sum < 1e-6)
						continue;
					sum = sqrt(sum);
	
					/* Make sum of squares == 0.5 */
					for (m = 0; m < 3; m++) {
						pp[m] /= sum;
						pp[m] *= 0.5;
					}
					/* And then scale it */
					if (pp[0] < 0.0)
						pp[0] = bsize[4] * pp[0];
					else
						pp[0] = bsize[5] * pp[0];

					if (pp[1] < 0.0)
						pp[1] = bsize[0] * pp[1];
					else
						pp[1] = bsize[1] * pp[1];

					if (pp[2] < 0.0)
						pp[2] = bsize[2] * pp[2];
					else
						pp[2] = bsize[3] * pp[2];
				} else {						/* Box */
					if (co[0] < 0.0)
						pp[0] = bsize[4] * (co[0]/(npoints-1.0) - 0.5);	
					else
						pp[0] = bsize[4] * (co[0]/(npoints-1.0) - 0.5);	

					if (co[1] < 0.0)
						pp[1] = bsize[0] * (co[1]/(npoints-1.0) - 0.5);
					else
						pp[1] = bsize[1] * (co[1]/(npoints-1.0) - 0.5);

					if (co[2] < 0.0)
						pp[2] = bsize[1] * (co[2]/(npoints-1.0) - 0.5);
					else
						pp[2] = bsize[2] * (co[2]/(npoints-1.0) - 0.5);
				}

				pp[0] += 50.0 + goff[2];
				pp[1] += goff[0];
				pp[2] += goff[1];
//printf("~1 point %f %f %f\n",pp[0],pp[1],pp[2]);
				gam->expand(gam, pp);

				if (pp[0] > wp[0])
					wp[0] = pp[0];
				if (pp[0] < bp[0])
					bp[0] = pp[0];
			}
		}
	}

	gam->setwb(gam, wp, bp, NULL);

	/* Write out the gamut */
	if (gam->write_gam(gam, out_name))
		error ("write gamut failed on '%s'",out_name);

	if (dowrl) {
		strcpy(xl,".wrl");
		if (gam->write_vrml(gam, out_name, doaxes, 0))
			error ("write vrml failed on '%s'",out_name);
	}

	if (verb)
		printf("Written out the gamut surface\n");

	gam->del(gam);

	return 0;
}

