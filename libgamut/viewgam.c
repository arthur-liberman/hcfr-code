
/* 
 * viewgam
 *
 * Gamut support routines.
 *
 * Author:  Graeme W. Gill
 * Date:    4/10/00
 * Version: 1.00
 *
 * Copyright 2000 Graeme W. Gill
 * All rights reserved.
 *
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
#include "gamut.h"
#include "cgats.h"
#include "vrml.h"

/* 
	This program reads one or more CGATS format triangular gamut
	surface descriptions, and combines them into a VRML file,
	so that the gamuts can be visually compared.

 */

/* TTBD:
 *
 */

#undef DEBUG

#undef HALF_HACK /* 27.0 */		/* Crude cutting plane */

void usage(char *diag, ...) {
	fprintf(stderr,"View gamuts Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	if (diag != NULL) {
		va_list args;
		fprintf(stderr,"Diagnostic: ");
		va_start(args, diag);
		vfprintf(stderr, diag, args);
		va_end(args);
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"usage: viewgam { [-c color] [-t trans] [-w|s] infile.gam } ... outfile%s\n",vrml_ext());
	fprintf(stderr," -c color       Color to make gamut, r = red, g = green, b = blue\n"); 
	fprintf(stderr,"                c = cyan, m = magenta, y = yellow, e = grey, w = white\n"); 
	fprintf(stderr,"                n = natural color\n"); 
	fprintf(stderr," -t trans       Set transparency from 0.0 (opaque) to 1.0 (invisible)\n"); 
	fprintf(stderr," -w             Show as a wireframe\n");
	fprintf(stderr," -s             Show as a solid surace\n");
	fprintf(stderr," infile.gam     Name of .gam file\n");
	fprintf(stderr,"                Repeat above for each input file\n\n");
	fprintf(stderr," -n             Don't add Lab axes\n");
	fprintf(stderr," -k             Add markers for prim. & sec. \"cusp\" points\n");
	fprintf(stderr," -i             Compute and print intersecting volume of first 2 gamuts\n");
	fprintf(stderr," -I isect.gam   Same as -i, but save intersection gamut to isect.gam\n");
	fprintf(stderr,"                (Set env. ARGYLL_3D_DISP_FORMAT to VRML, X3D or X3DOM to change format)\n");
	fprintf(stderr," outfile        Base name of output %s file\n",vrml_ext());
	fprintf(stderr,"\n");
	exit(1);
}

typedef enum {
	gam_red       = 0,
	gam_green     = 1,
	gam_blue      = 2,
	gam_cyan      = 3,
	gam_magenta   = 4,	
	gam_yellow    = 5,
	gam_grey      = 6,
	gam_white     = 7,
	gam_natural   = 8
} gam_colors;

struct {
	double rgb[3];
} color_rgb[8] = {
	{ 1, 0, 0 },	/* gam_red */
	{ 0, 1, 0 },	/* gam_green */
	{ 0, 0, 1 },	/* gam_blue */
	{ 0, 1, 1 },	/* gam_cyan */
	{ 1, 0, 1 },	/* gam_magenta */
	{ 1, 1, 0 },	/* gam_yellow */
	{ .1, .1, .1 },	/* gam_grey */
	{ .7, .7, .7 }	/* gam_white */
};

typedef enum {
	gam_solid     = 0,
	gam_wire      = 1,
	gam_points    = 2
} gam_reps;

struct _gamdisp {
	char in_name[MAXNAMEL+1];
	gam_colors in_colors;		/* Color enum for each input */
	double in_trans;			/* Transparency for each input */
	gam_reps in_rep;			/* Representation enum for each input */
}; typedef struct _gamdisp gamdisp;  


/* Set a default for a given gamut */
static void set_default(gamdisp *gds, int n) {
	gds[n].in_name[0] = '\000';
	switch(n) {
		case 0:
			gds[n].in_colors = gam_natural;
			gds[n].in_rep   = gam_solid;
			gds[n].in_trans  = 0.0;
			break;
		case 1:
			gds[n].in_colors = gam_white;
			gds[n].in_rep   = gam_wire;
			gds[n].in_trans  = 0.0;
			break;
		case 2:
			gds[n].in_colors = gam_red;
			gds[n].in_rep   = gam_wire;
			gds[n].in_trans  = 0.0;
			break;
		case 3:
			gds[n].in_colors = gam_cyan;
			gds[n].in_rep   = gam_wire;
			gds[n].in_trans  = 0.0;
			break;
		case 4:
			gds[n].in_colors = gam_yellow;
			gds[n].in_rep   = gam_wire;
			gds[n].in_trans  = 0.2;
			break;
		case 5:
			gds[n].in_colors = gam_green;
			gds[n].in_rep   = gam_wire;
			gds[n].in_trans  = 0.3;
			break;
		case 6:
			gds[n].in_colors = gam_blue;
			gds[n].in_rep   = gam_wire;
			gds[n].in_trans  = 0.4;
			break;
		case 7:
			gds[n].in_colors = gam_magenta;
			gds[n].in_rep   = gam_wire;
			gds[n].in_trans  = 0.5;
			break;
		default:
			gds[n].in_colors = n % 6;
			gds[n].in_rep   = gam_wire;
			gds[n].in_trans  = 0.6;
			break;
	}
}

int
main(int argc, char *argv[]) {
	int fa, nfa, mfa;		/* argument we're looking at */
	int n, ng = 0;			/* Current allocation, number of input gamuts */
	gamdisp *gds;			/* Definition of each gamut */
	int doaxes = 1;
	int docusps = 0;
	int isect = 0;
	vrml *wrl;
	char out_name[MAXNAMEL+1+10];
	char iout_name[MAXNAMEL+1] = "\000";;
	if (argc < 3)
		usage("Too few arguments, got %d expect at least 2",argc-1);

	mfa = 1;		/* Minimum final arguments */

	if ((gds = (gamdisp *)malloc((ng+1) * sizeof(gamdisp))) == NULL)
		error("Malloc failed on gamdisp");
	set_default(gds, 0);

	/* Process the arguments */
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
				usage("Usage requested");

			/* Color */
			else if (argv[fa][1] == 'c' || argv[fa][1] == 'C') {
				fa = nfa;
				if (na == NULL) usage("Expect argument after flag -c");
    			switch (na[0]) {
					case 'r':
					case 'R':
						gds[ng].in_colors = gam_red;
						break;
					case 'g':
					case 'G':
						gds[ng].in_colors = gam_green;
						break;
					case 'b':
					case 'B':
						gds[ng].in_colors = gam_blue;
						break;
					case 'c':
					case 'C':
						gds[ng].in_colors = gam_cyan;
						break;
					case 'm':
					case 'M':
						gds[ng].in_colors = gam_magenta;
						break;
					case 'y':
					case 'Y':
						gds[ng].in_colors = gam_yellow;
						break;
					case 'e':
					case 'E':
						gds[ng].in_colors = gam_grey;
						break;
					case 'w':
					case 'W':
						gds[ng].in_colors = gam_white;
						break;
					case 'n':
					case 'N':
						gds[ng].in_colors = gam_natural;
						break;
					default:
						usage("Unknown argument after flag -c '%c'",na[0]);
				}
			}

			/* Transparency */
			else if (argv[fa][1] == 't' || argv[fa][1] == 'T') {
				double v;
				fa = nfa;
				if (na == NULL) usage("Expect argument after flag -t");
				v = atof(na);
				if (v < 0.0)
					v = 0.0;
				else if (v > 1.0)
					v = 1.0;
				gds[ng].in_trans = v;
			}

			/* Solid output */
			else if (argv[fa][1] == 's' || argv[fa][1] == 'S') {
				gds[ng].in_rep = gam_solid;
			}

			/* Wireframe output */
			else if (argv[fa][1] == 'w' || argv[fa][1] == 'W') {
				gds[ng].in_rep = gam_wire;
			}

			/* No axis output */
			else if (argv[fa][1] == 'n' || argv[fa][1] == 'N') {
				doaxes = 0;
			}

			/* Add cusp markers */
			else if (argv[fa][1] == 'k' || argv[fa][1] == 'K') {
				docusps = 1;
			}

			/* Print intersecting volume */
			else if (argv[fa][1] == 'i' || argv[fa][1] == 'I') {
				isect = 1;

				/* There is an intersection output gamut file */
				if (argv[fa][1] == 'I' && na != NULL) {
					fa = nfa;
					strncpy(iout_name, na, MAXNAMEL); iout_name[MAXNAMEL] = '\000';
				}
			}

			else 
				usage("Unknown flag '%c'",argv[fa][1]);

		} else if (argv[fa][0] != '\000') { /* Got a non-flag */
			strncpy(gds[ng].in_name,argv[fa],MAXNAMEL); gds[ng].in_name[MAXNAMEL] = '\000';

			ng++;
			if ((gds = (gamdisp *)realloc(gds, (ng+1) * sizeof(gamdisp))) == NULL)
				error("Realloc failed on gamdisp");
			set_default(gds, ng);
		} else {
			break;
		}
	}

	/* The last "gamut" is actually the output VRML filename, */
	/* so unwind it. */

	if (ng < 2)
		usage("Not enough arguments to specify output %s files",vrml_format());

	strncpy(out_name, gds[--ng].in_name,MAXNAMEL); out_name[MAXNAMEL] = '\000';

#ifdef DEBUG
	for (n = 0; n < ng; n++) {
		printf("Input file %d is '%s'\n",n,gds[n].in_name);
		printf("Input file %d has color %d\n",n,gds[n].in_colors);
		printf("Input file %d has rep %d\n",n,gds[n].in_rep);
		printf("Input file %d has trans %f\n",n,gds[n].in_trans);
		
	}
	printf("Output file is '%s'\n",out_name);
#endif	/* DEBUG */

	/* Create the VRML object */
	if ((wrl = new_vrml(out_name, doaxes, vrml_lab)) == NULL)
		error("Error creating %s object '%s%s'\n",vrml_format(),out_name,vrml_ext());
	
	/* Read each input in turn */
	for (n = 0; n < ng; n++) {
		int i;
		cgats *pp;
		int nverts;
		int ntris;
		int Lf, af, bf;			/* Fields holding L, a & b data */
		int v0f, v1f, v2f;		/* Fields holding verticies 0, 1 & 2 */

		pp = new_cgats();	/* Create a CGATS structure */
	
		/* Setup to cope with a gamut file */
		pp->add_other(pp, "GAMUT");
	
		if (pp->read_name(pp, gds[n].in_name))
			error("Input file '%s' error : %s",gds[n].in_name, pp->err);
	
		if (pp->t[0].tt != tt_other || pp->t[0].oi != 0)
			error("Input file isn't a GAMUT format file");
		if (pp->ntables != 2)
			error("Input file doesn't contain exactly two tables");

		if ((nverts = pp->t[0].nsets) <= 0)
			error("No verticies");
		if ((ntris = pp->t[1].nsets) <= 0)
			error("No triangles");

		if ((Lf = pp->find_field(pp, 0, "LAB_L")) < 0)
			error("Input file doesn't contain field LAB_L");
		if (pp->t[0].ftype[Lf] != r_t)
			error("Field LAB_L is wrong type");
		if ((af = pp->find_field(pp, 0, "LAB_A")) < 0)
			error("Input file doesn't contain field LAB_A");
		if (pp->t[0].ftype[af] != r_t)
			error("Field LAB_A is wrong type");
		if ((bf = pp->find_field(pp, 0, "LAB_B")) < 0)
			error("Input file doesn't contain field LAB_B");
		if (pp->t[0].ftype[bf] != r_t)
			error("Field LAB_B is wrong type");

		wrl->start_line_set(wrl, 0);

		/* Spit out the point values, in order. */
		/* Note that a->x, b->y, L->z */
		for (i = 0; i < nverts; i++) {
			double pos[3];
			pos[0] = *((double *)pp->t[0].fdata[i][Lf]);
			pos[1] = *((double *)pp->t[0].fdata[i][af]);
			pos[2] = *((double *)pp->t[0].fdata[i][bf]);
			
			wrl->add_vertex(wrl, 0, pos);
		}

		/* Write the triangles/wires out */
		if ((v0f = pp->find_field(pp, 1, "VERTEX_0")) < 0)
			error("Input file doesn't contain field VERTEX_0");
		if (pp->t[1].ftype[v0f] != i_t)
			error("Field VERTEX_0 is wrong type");
		if ((v1f = pp->find_field(pp, 1, "VERTEX_1")) < 0)
			error("Input file doesn't contain field VERTEX_1");
		if (pp->t[1].ftype[v1f] != i_t)
			error("Field VERTEX_1 is wrong type");
		if ((v2f = pp->find_field(pp, 1, "VERTEX_2")) < 0)
			error("Input file doesn't contain field VERTEX_2");
		if (pp->t[1].ftype[v2f] != i_t)
			error("Field VERTEX_2 is wrong type");

		for (i = 0; i < ntris; i++) {
			int v0, v1, v2;
			v0 = *((int *)pp->t[1].fdata[i][v0f]);
			v1 = *((int *)pp->t[1].fdata[i][v1f]);
			v2 = *((int *)pp->t[1].fdata[i][v2f]);

#ifdef HALF_HACK 
			if (*((double *)pp->t[0].fdata[v0][Lf]) < HALF_HACK
			 || *((double *)pp->t[0].fdata[v1][Lf]) < HALF_HACK
			 || *((double *)pp->t[0].fdata[v2][Lf]) < HALF_HACK)
				continue;
#endif /* HALF_HACK */

			if (gds[n].in_rep == gam_wire) {
				int ix[2];
				if (v0 < v1) {				/* Only output 1 wire of two on an edge */
					ix[0] = v0;
					ix[1] = v1;
					wrl->add_line(wrl, 0, ix);
				}
				if (v1 < v2) {
					ix[0] = v1;
					ix[1] = v2;
					wrl->add_line(wrl, 0, ix);
				}
				if (v2 < v0) {
					ix[0] = v2;
					ix[1] = v0;
					wrl->add_line(wrl, 0, ix);
				}
			} else {
				int ix[3];
				ix[0] = v0;
				ix[1] = v1;
				ix[2] = v2;
				wrl->add_triangle(wrl, 0, ix);
			}
		}

		/* Write the wires or triangles out */
		if (gds[n].in_rep == gam_wire) {
			if (gds[n].in_colors == gam_natural)
				wrl->make_lines_vc(wrl, 0, gds[n].in_trans);
			else
				wrl->make_lines_cc(wrl, 0, gds[n].in_trans, color_rgb[gds[n].in_colors].rgb);
		} else {
			if (gds[n].in_colors == gam_natural)
				wrl->make_triangles_vc(wrl, 0, gds[n].in_trans);
			else
				wrl->make_triangles(wrl, 0, gds[n].in_trans, color_rgb[gds[n].in_colors].rgb);
		}

		/* See if there are cusp values */
		if (docusps) {
			int kk;
			double rgb[3], Lab[3];
			char buf1[50];
			char *cnames[6] = { "RED", "YELLOW", "GREEN", "CYAN", "BLUE", "MAGENTA" };
	
			for (i = 0; i < 6; i++) {
				sprintf(buf1,"CUSP_%s", cnames[i]);
				if ((kk = pp->find_kword(pp, 0, buf1)) < 0)
					break;
	
				if (sscanf(pp->t[0].kdata[kk], "%lf %lf %lf",
			           &Lab[0], &Lab[1], &Lab[2]) != 3) {
					break;
				}

				if (gds[n].in_colors != gam_natural)
					wrl->add_marker(wrl, Lab, color_rgb[gds[n].in_colors].rgb, 2.0);
				else
					wrl->add_marker(wrl, Lab, NULL, 2.0);
			}
		}
		pp->del(pp);		/* Clean up */
	}

	/* Write the file out */
	if (wrl->flush(wrl))
		error("Closing output file '%s%s'\n",out_name,vrml_ext());
	wrl->del(wrl);

	if (isect && ng >= 2) {
		gamut *s, *s1, *s2;
		double v1, v2, vi;

		if ((s = new_gamut(0.0, 0, 0)) == NULL)
			error("Creating gamut object failed");
		
		if ((s1 = new_gamut(0.0, 0, 0)) == NULL)
			error("Creating gamut object failed");
		
		if ((s2 = new_gamut(0.0, 0, 0)) == NULL)
			error("Creating gamut object failed");
		
		if (s1->read_gam(s1, gds[0].in_name))
			error("Input file '%s' read failed",gds[n].in_name[0]);

		if (s2->read_gam(s2, gds[1].in_name))
			error("Input file '%s' read failed",gds[n].in_name[1]);

		v1 = s1->volume(s1);
		v2 = s2->volume(s2);

		if (s->intersect(s, s1, s2))
			error("Gamuts are not compatible! (Colorspace, gamut center ?)");
		vi = s->volume(s);

		if (iout_name[0] != '\000') {
			if (s->write_gam(s, iout_name))
				error("Writing intersection gamut to '%s' failed",iout_name);
		}

		printf("Intersecting volume = %.1f cubic units\n",vi);
		printf("'%s' volume = %.1f cubic units, intersect = %.2f%%\n",gds[0].in_name,v1,100.0 * vi/v1);
		printf("'%s' volume = %.1f cubic units, intersect = %.2f%%\n",gds[1].in_name,v2,100.0 * vi/v2);

		s1->del(s1);
		s2->del(s2);
	}

	if (ng > 0)
		free(gds);

	return 0;
}

#ifdef NEVER
/* Basic printf type error() and warning() routines */

void
error(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"icclu: Error - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit (-1);
}

void
warning(char *fmt, ...)
{
	va_list args;

	fprintf(stderr,"icclu: Warning - ");
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

#endif /* NEVER */
