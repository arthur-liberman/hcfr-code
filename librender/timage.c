
/* 
 * Argyll Color Correction System
 *
 * RGB gamut boundary test image generator
 *
 * Author: Graeme W. Gill
 * Date:   28/12/2005
 *
 * Copyright 2005 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/*
 * Generate TIFF image with two RGB cube surface hexagons,
 * plus a rectangular grey wedges between them, on a grey
 * background, or a rectangular gamut surface test image.
 */

/*
 * TTBD:
 */

#undef DEBUG

#define verbo stdout

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include "../h/aconfig.h"
#include "../h/copyright.h"
#include "../libconv/numsup.h"
#include "render.h"

#define DEF_DPI 200
#define DITHER 0			/* 1 for test 8 bit dithering, 2 for test error diffusion */
							/* 0x8001 for dithering FG only, 0x8002 for err. diff. FG only */
#undef PNG_MEM				/* Test PNG save to memory */

void
usage(void) {
	fprintf(stderr,"Create test images, default hex RGB surface and wedge, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: timage [-options] outfile.[tif|png]\n");
//	fprintf(stderr," -v                Verbose\n");
	fprintf(stderr," -t                Generate rectangular gamut boundary test chart\n");
	fprintf(stderr," -p steps          Generate a colorspace step chart with L* steps^2\n");
	fprintf(stderr," -r res            Resolution in DPI (default %d)\n",DEF_DPI);
	fprintf(stderr," -s                Smooth blend\n");
	fprintf(stderr," -x                16 bit output\n");
	fprintf(stderr," -4                CMYK output\n");
	fprintf(stderr," -g prop           Percentage towards grey (default 0%%)\n");
	fprintf(stderr," -P                Save as PNG file (deffault TIFF)\n");
//	fprintf(stderr," -D	               Debug primitives plot */
	fprintf(stderr," outfile.[tif|png] Output TIFF or PNG file\n");
	exit(1);
	}

int main(int argc, char *argv[]) {
	int fa,nfa;				/* current argument we're looking at */
	int verb = 0;
	int rchart = 0;			/* Rectangular chart */
	int schart = 0;			/* Step chart with steps^2 */
	int smooth = 0;			/* Use smooth blending */
	rend_format fmt = tiff_file;	/* Output filr format */
	int debugchart = 0;		/* Debug chart */
	double res = DEF_DPI;
	depth2d depth = bpc8_2d;
	int cmyk = 0;			/* Do CMYK output */
	char outname[MAXNAMEL+1] = { 0 };	/* Output TIFF name */
	render2d *r;
	color2d c;
	double vv[4][2];
	color2d cc[4];
	double gbf = 1.0;		/* Grey blend factor */
	double w, h;			/* Size of page in mm */
	int i, j;

	error_program = "timage";

	if (argc <= 1)
		usage();

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {

		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-') {	/* Look for any flags */
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

			else if (argv[fa][1] == 'v')
				verb = 1;

			/* Rectangular chart */
			else if (argv[fa][1] == 't') {
				rchart = 1;
				schart = 0;

			/* Smooth blending */
			} else if (argv[fa][1] == 's')
				smooth = 1;

			/* 16 bit depth */
			else if (argv[fa][1] == 'x')
				depth = bpc16_2d;

			/* cmyk */
			else if (argv[fa][1] == '4')
				cmyk = 1;

			/* step chart */
			else if (argv[fa][1] == 'p') {
				fa = nfa;
				if (na == NULL) usage();
				schart = atoi(na);
				if (schart <= 0) usage();
				rchart = 0;
			}

			/* PNG file */
			else if (argv[fa][1] == 'P') {
				fmt = png_file;
			}

			/* debug chart */
			else if (argv[fa][1] == 'D') {
				debugchart = 1;
			}

			/* resolution */
			else if (argv[fa][1] == 'r') {
				fa = nfa;
				if (na == NULL) usage();
				res = atof(na);
				if (res <= 0.0) usage();
			}

			/* grey blend */
			else if (argv[fa][1] == 'g') {
				fa = nfa;
				if (na == NULL) usage();
				gbf = 1.0 - 0.01 * atof(na);
				if (gbf < 0.0 || gbf > 1.0) usage();
			}
			else 
				usage();
		} else
			break;
	}

	/* Get the file name arguments */
	if (fa >= argc || argv[fa][0] == '-') usage();
	strncpy(outname,argv[fa++],MAXNAMEL); outname[MAXNAMEL] = '\000';

	res /= 25.4;				/* Convert to DPmm */ 

	/* Debug chart - test each primitive in RGB space */
	if (debugchart) {
		font2d fo;

		h = 140.0;
		w = 200.0;

		if (cmyk)
			error("CMYK not supported for test chart");

		if ((r = new_render2d(w, h, NULL, res, res, rgb_2d, 0, depth, DITHER, NULL, NULL)) == NULL) {
			error("new_render2d() failed");
		}
	
		/* Set the background color */
//		c[0] = 0.5;
//		c[1] = 0.5;
//		c[2] = 0.5;
		c[0] = 1.0;
		c[1] = 1.0;
		c[2] = 1.0;
		r->set_defc(r, c);
	
		c[0] = 1.0;	/* Red rectangle, bottom left */
		c[1] = 0.0;
		c[2] = 0.0;
		r->add(r, new_rect2d(r, 5.0, 5.0, 8.0, 8.0, c)) ;

		c[0] = 0.0;	/* Green to the right of red */
		c[1] = 1.0;
		c[2] = 0.0;
		r->add(r, new_rect2d(r, 15.0, 5.0, 8.0, 8.0, c)) ;

		c[0] = 0.0;	/* Blue to the right of green */
		c[1] = 0.0;
		c[2] = 1.0;
		r->add(r, new_rect2d(r, 25.0, 5.0, 8.0, 8.0, c)) ;

		/* A vertex shaded rectangle */
		cc[0][0] = 1.0; 	/* Red */
		cc[0][1] = 0.0;
		cc[0][2] = 0.0;
		cc[1][0] = 0.0;		/* Green */
		cc[1][1] = 1.0;
		cc[1][2] = 0.0;
		cc[2][0] = 0.0;		/* Blue */
		cc[2][1] = 0.0;
		cc[2][2] = 1.0;
		cc[3][0] = 1.0;		/* Yellow */
		cc[3][1] = 1.0;
		cc[3][2] = 0.0;

		r->add(r, new_rectvs2d(r, 5.0, 20.0, 18.0, 18.0, cc));

		/* A shaded triangle to the right of the shaded rectangle */
		vv[0][0] = 30.0;
		vv[0][1] = 20.0;
		cc[0][0] = 0.0;
		cc[0][1] = 1.0;
		cc[0][2] = 1.0;
	
		vv[1][0] = 50.0;
		vv[1][1] = 20.0;
		cc[1][0] = 1.0;
		cc[1][1] = 0.0;
		cc[1][2] = 1.0;
	
		vv[2][0] = 40.0;
		vv[2][1] = 40.0;
		cc[2][0] = 1.0;
		cc[2][1] = 1.0;
		cc[2][2] = 0.0;
		r->add(r, new_trivs2d(r, vv, cc));

		/* A diagonal wide line */
		c[0] = 0.0;
		c[1] = 0.0;
		c[2] = 0.0;
		r->add(r, new_line2d(r, 10.0, 45.0, 11.0, 55.0, 3.0, 1, c));
	
		/* A dashed line */
		add_dashed_line2d(r, 20.0, 45.0, 100.0, 65.0, 1.0,  3.0, 5.0,  1, c);
	
		fo = futura_l;

		/* rectange the size the letter A should be */
		c[0] = 0.7;
		c[1] = 0.0;
		c[2] = 0.0;
		r->add(r, new_rect2d(r, 10.0, 60.0, 7.7, 10.0, c)) ;

		c[0] = 0.0;
		c[1] = 0.0;
		c[2] = 0.0;
		/* The letter A */
		add_char2d(r, NULL, NULL, fo, 'A', 10.0, 60.0, 10.0, 0, c);

		/* A test string */
		add_string2d(r, NULL, NULL, fo, "Testing 1234", 10.0, 70.0, 7.0, 3, c);

		{
			double x, y;
			char chars[33];

			x = 10.0;
			y = 125.0; 
			for (j = 0; j < 4; j++) {
				for (i = 0; i < 32; i++)
					chars[i] = j * 32 + i;
				chars[i] = '\000';
				add_string2d(r, NULL, NULL, fo, chars, x, y, 7.0, 0, c);
				y -= 10.0;
			}
		}

	/* RGB Hexagon chart */
	} else if (rchart == 0 && schart == 0) {
		double r3o2;			/* 0.866025 */
		double bb = 0.07;		/* Border proportion */
		double hh = 40.0;		/* Height of hexagon in mm */
		color2d white;
		color2d red;
		color2d green;
		color2d blue;
		color2d cyan;
		color2d magenta;
		color2d yellow;
		color2d black;
		color2d grey;
		color2d kblack;

		r3o2 = sqrt(3.0)/2.0;		/* Width to heigh of hexagon */
		h = (1.0 + 2.0 * bb) * hh;
		w = (4.0 * bb + 0.25 + 2.0 * r3o2) * hh;
	
		if ((r = new_render2d(w, h, NULL, res, res, cmyk ? cmyk_2d : rgb_2d, 0, depth, DITHER, NULL, NULL)) == NULL) {
			error("new_render2d() failed");
		}
	
		if (cmyk) {
			white[0] = 0.0;
			white[1] = 0.0;
			white[2] = 0.0;
			white[3] = 0.0;
			red[0] = 0.0;
			red[1] = 1.0;
			red[2] = 1.0;
			red[3] = 0.0;
			green[0] = 1.0;
			green[1] = 0.0;
			green[2] = 1.0;
			green[3] = 0.0;
			blue[0] = 1.0;
			blue[1] = 1.0;
			blue[2] = 0.0;
			blue[3] = 0.0;
			cyan[0] = 1.0;
			cyan[1] = 0.0;
			cyan[2] = 0.0;
			cyan[3] = 0.0;
			magenta[0] = 0.0;
			magenta[1] = 1.0;
			magenta[2] = 0.0;
			magenta[3] = 0.0;
			yellow[0] = 0.0;
			yellow[1] = 0.0;
			yellow[2] = 1.0;
			yellow[3] = 0.0;
			kblack[0] = 0.0;
			kblack[1] = 0.0;
			kblack[2] = 0.0;
			kblack[3] = 1.0;
			grey[0] = 0.0;
			grey[1] = 0.0;
			grey[2] = 0.0;
			grey[3] = 0.5;
			black[0] = 1.0;
			black[1] = 1.0;
			black[2] = 1.0;
			black[3] = 0.0;
		} else {
			white[0] = 1.0;
			white[1] = 1.0;
			white[2] = 1.0;
			red[0] = 1.0;
			red[1] = 0.0;
			red[2] = 0.0;
			green[0] = 0.0;
			green[1] = 1.0;
			green[2] = 0.0;
			blue[0] = 0.0;
			blue[1] = 0.0;
			blue[2] = 1.0;
			cyan[0] = 0.0;
			cyan[1] = 1.0;
			cyan[2] = 1.0;
			magenta[0] = 1.0;
			magenta[1] = 0.0;
			magenta[2] = 1.0;
			yellow[0] = 1.0;
			yellow[1] = 1.0;
			yellow[2] = 0.0;
			black[0] = 0.0;
			black[1] = 0.0;
			black[2] = 0.0;
			grey[0] = 0.5;
			grey[1] = 0.5;
			grey[2] = 0.5;
		}

		/* Set the default color */
		r->set_defc(r, grey);
	
		/* Left hand hex */
		vv[0][0] = hh * bb + r3o2 * 0.5 * hh;
		vv[0][1] = hh * bb + hh/2.0;
		cc[0][0] = white[0] * gbf + (1.0 - gbf) * grey[0];
		cc[0][1] = white[1] * gbf + (1.0 - gbf) * grey[1];
		cc[0][2] = white[2] * gbf + (1.0 - gbf) * grey[2];
		cc[0][3] = white[3] * gbf + (1.0 - gbf) * grey[3];
	
		vv[1][0] = hh * bb + r3o2 * 0.5 * hh;
		vv[1][1] = hh * bb;
		cc[1][0] = red[0] * gbf + (1.0 - gbf) * grey[0];
		cc[1][1] = red[1] * gbf + (1.0 - gbf) * grey[1];
		cc[1][2] = red[2] * gbf + (1.0 - gbf) * grey[2];
		cc[1][3] = red[3] * gbf + (1.0 - gbf) * grey[3];
	
		vv[2][0] = hh * bb;
		vv[2][1] = hh * bb + 0.25 * hh;
		cc[2][0] = magenta[0] * gbf + (1.0 - gbf) * grey[0];
		cc[2][1] = magenta[1] * gbf + (1.0 - gbf) * grey[1];
		cc[2][2] = magenta[2] * gbf + (1.0 - gbf) * grey[2];
		cc[2][3] = magenta[3] * gbf + (1.0 - gbf) * grey[3];
		r->add(r, new_trivs2d(r, vv, cc));
	
		vv[1][0] = hh * bb;
		vv[1][1] = hh * bb + 0.75 * hh;
		cc[1][0] = blue[0] * gbf + (1.0 - gbf) * grey[0];
		cc[1][1] = blue[1] * gbf + (1.0 - gbf) * grey[1];
		cc[1][2] = blue[2] * gbf + (1.0 - gbf) * grey[2];
		cc[1][3] = blue[3] * gbf + (1.0 - gbf) * grey[3];
		r->add(r, new_trivs2d(r, vv, cc));
	
		vv[2][0] = hh * bb + r3o2 * 0.5 * hh;
		vv[2][1] = hh * bb + 1.0 * hh;
		cc[2][0] = cyan[0] * gbf + (1.0 - gbf) * grey[0];
		cc[2][1] = cyan[1] * gbf + (1.0 - gbf) * grey[1];
		cc[2][2] = cyan[2] * gbf + (1.0 - gbf) * grey[2];
		cc[2][3] = cyan[3] * gbf + (1.0 - gbf) * grey[3];
		r->add(r, new_trivs2d(r, vv, cc));
	
		vv[1][0] = hh * bb + r3o2 * 1.0 * hh;;
		vv[1][1] = hh * bb + 0.75 * hh;
		cc[1][0] = green[0] * gbf + (1.0 - gbf) * grey[0];
		cc[1][1] = green[1] * gbf + (1.0 - gbf) * grey[1];
		cc[1][2] = green[2] * gbf + (1.0 - gbf) * grey[2];
		cc[1][3] = green[3] * gbf + (1.0 - gbf) * grey[3];
		r->add(r, new_trivs2d(r, vv, cc));
	
		vv[2][0] = hh * bb + r3o2 * 1.0 * hh;
		vv[2][1] = hh * bb + 0.25 * hh;
		cc[2][0] = yellow[0] * gbf + (1.0 - gbf) * grey[0];
		cc[2][1] = yellow[1] * gbf + (1.0 - gbf) * grey[1];
		cc[2][2] = yellow[2] * gbf + (1.0 - gbf) * grey[2];
		cc[2][3] = yellow[3] * gbf + (1.0 - gbf) * grey[3];
		r->add(r, new_trivs2d(r, vv, cc));
	
		vv[1][0] = hh * bb + r3o2 * 0.5 * hh;;
		vv[1][1] = hh * bb;
		cc[1][0] = red[0] * gbf + (1.0 - gbf) * grey[0];
		cc[1][1] = red[1] * gbf + (1.0 - gbf) * grey[1];
		cc[1][2] = red[2] * gbf + (1.0 - gbf) * grey[2];
		cc[1][3] = red[3] * gbf + (1.0 - gbf) * grey[3];
		r->add(r, new_trivs2d(r, vv, cc));
	
	
		/* Right hand hex */
		vv[0][0] = hh * (3.0 * bb + 0.25 + r3o2) + r3o2 * 0.5 * hh;
		vv[0][1] = hh * bb + hh/2.0;
		cc[0][0] = black[0] * gbf + (1.0 - gbf) * grey[0];
		cc[0][1] = black[1] * gbf + (1.0 - gbf) * grey[1];
		cc[0][2] = black[2] * gbf + (1.0 - gbf) * grey[2];
		cc[0][3] = black[3] * gbf + (1.0 - gbf) * grey[3];
	
		vv[1][0] = hh * (3.0 * bb + 0.25 + r3o2) + r3o2 * 0.5 * hh;
		vv[1][1] = hh * bb;
		cc[1][0] = red[0] * gbf + (1.0 - gbf) * grey[0];
		cc[1][1] = red[1] * gbf + (1.0 - gbf) * grey[1];
		cc[1][2] = red[2] * gbf + (1.0 - gbf) * grey[2];
		cc[1][3] = red[3] * gbf + (1.0 - gbf) * grey[3];
	
		vv[2][0] = hh * (3.0 * bb + 0.25 + r3o2);
		vv[2][1] = hh * bb + 0.25 * hh;
		cc[2][0] = magenta[0] * gbf + (1.0 - gbf) * grey[0];
		cc[2][1] = magenta[1] * gbf + (1.0 - gbf) * grey[1];
		cc[2][2] = magenta[2] * gbf + (1.0 - gbf) * grey[2];
		cc[2][3] = magenta[3] * gbf + (1.0 - gbf) * grey[3];
		r->add(r, new_trivs2d(r, vv, cc));
	
		vv[1][0] = hh * (3.0 * bb + 0.25 + r3o2);
		vv[1][1] = hh * bb + 0.75 * hh;
		cc[1][0] = blue[0] * gbf + (1.0 - gbf) * grey[0];
		cc[1][1] = blue[1] * gbf + (1.0 - gbf) * grey[1];
		cc[1][2] = blue[2] * gbf + (1.0 - gbf) * grey[2];
		cc[1][3] = blue[3] * gbf + (1.0 - gbf) * grey[3];
		r->add(r, new_trivs2d(r, vv, cc));
	
		vv[2][0] = hh * (3.0 * bb + 0.25 + r3o2) + r3o2 * 0.5 * hh;
		vv[2][1] = hh * bb + 1.0 * hh;
		cc[2][0] = cyan[0] * gbf + (1.0 - gbf) * grey[0];
		cc[2][1] = cyan[1] * gbf + (1.0 - gbf) * grey[1];
		cc[2][2] = cyan[2] * gbf + (1.0 - gbf) * grey[2];
		cc[1][3] = cyan[3] * gbf + (1.0 - gbf) * grey[3];
		r->add(r, new_trivs2d(r, vv, cc));
	
		vv[1][0] = hh * (3.0 * bb + 0.25 + r3o2) + r3o2 * 1.0 * hh;;
		vv[1][1] = hh * bb + 0.75 * hh;
		cc[1][0] = green[0] * gbf + (1.0 - gbf) * grey[0];
		cc[1][1] = green[1] * gbf + (1.0 - gbf) * grey[1];
		cc[1][2] = green[2] * gbf + (1.0 - gbf) * grey[2];
		cc[1][3] = green[3] * gbf + (1.0 - gbf) * grey[3];
		r->add(r, new_trivs2d(r, vv, cc));
	
		vv[2][0] = hh * (3.0 * bb + 0.25 + r3o2) + r3o2 * 1.0 * hh;
		vv[2][1] = hh * bb + 0.25 * hh;
		cc[2][0] = yellow[0] * gbf + (1.0 - gbf) * grey[0];
		cc[2][1] = yellow[1] * gbf + (1.0 - gbf) * grey[1];
		cc[2][2] = yellow[2] * gbf + (1.0 - gbf) * grey[2];
		cc[2][3] = yellow[3] * gbf + (1.0 - gbf) * grey[3];
		r->add(r, new_trivs2d(r, vv, cc));
	
		vv[1][0] = hh * (3.0 * bb + 0.25 + r3o2) + r3o2 * 0.5 * hh;;
		vv[1][1] = hh * bb;
		cc[1][0] = red[0] * gbf + (1.0 - gbf) * grey[0];
		cc[1][1] = red[1] * gbf + (1.0 - gbf) * grey[1];
		cc[1][2] = red[2] * gbf + (1.0 - gbf) * grey[2];
		cc[1][3] = red[3] * gbf + (1.0 - gbf) * grey[3];
		r->add(r, new_trivs2d(r, vv, cc));
	
		/* Center wedge */
		cc[0][0] = black[0] * gbf + (1.0 - gbf) * grey[0];
		cc[0][1] = black[1] * gbf + (1.0 - gbf) * grey[1];
		cc[0][2] = black[2] * gbf + (1.0 - gbf) * grey[2];
		cc[0][3] = black[3] * gbf + (1.0 - gbf) * grey[3];
		cc[1][0] = black[0] * gbf + (1.0 - gbf) * grey[0];
		cc[1][1] = black[1] * gbf + (1.0 - gbf) * grey[1];
		cc[1][2] = black[2] * gbf + (1.0 - gbf) * grey[2];
		cc[1][3] = black[3] * gbf + (1.0 - gbf) * grey[3];
		cc[2][0] = white[0] * gbf + (1.0 - gbf) * grey[0];
		cc[2][1] = white[1] * gbf + (1.0 - gbf) * grey[1];
		cc[2][2] = white[2] * gbf + (1.0 - gbf) * grey[2];
		cc[2][3] = white[3] * gbf + (1.0 - gbf) * grey[3];
		cc[3][0] = white[0] * gbf + (1.0 - gbf) * grey[0];
		cc[3][1] = white[1] * gbf + (1.0 - gbf) * grey[1];
		cc[3][2] = white[2] * gbf + (1.0 - gbf) * grey[2];
		cc[3][3] = white[3] * gbf + (1.0 - gbf) * grey[3];
		if (cmyk)
			r->add(r, new_rectvs2d(r, (2.0 * bb + r3o2) * hh, bb * hh, 0.125 * hh, hh, cc));
		else
			r->add(r, new_rectvs2d(r, (2.0 * bb + r3o2) * hh, bb * hh, 0.25 * hh, hh, cc));

		/* Center CMY wedge */
		if (cmyk) {
			cc[0][0] = kblack[0] * gbf + (1.0 - gbf) * grey[0];
			cc[0][1] = kblack[1] * gbf + (1.0 - gbf) * grey[1];
			cc[0][2] = kblack[2] * gbf + (1.0 - gbf) * grey[2];
			cc[0][3] = kblack[3] * gbf + (1.0 - gbf) * grey[3];
			cc[1][0] = kblack[0] * gbf + (1.0 - gbf) * grey[0];
			cc[1][1] = kblack[1] * gbf + (1.0 - gbf) * grey[1];
			cc[1][2] = kblack[2] * gbf + (1.0 - gbf) * grey[2];
			cc[1][3] = kblack[3] * gbf + (1.0 - gbf) * grey[3];
			cc[2][0] = white[0] * gbf + (1.0 - gbf) * grey[0];
			cc[2][1] = white[1] * gbf + (1.0 - gbf) * grey[1];
			cc[2][2] = white[2] * gbf + (1.0 - gbf) * grey[2];
			cc[2][3] = white[3] * gbf + (1.0 - gbf) * grey[3];
			cc[3][0] = white[0] * gbf + (1.0 - gbf) * grey[0];
			cc[3][1] = white[1] * gbf + (1.0 - gbf) * grey[1];
			cc[3][2] = white[2] * gbf + (1.0 - gbf) * grey[2];
			cc[3][3] = white[3] * gbf + (1.0 - gbf) * grey[3];
			r->add(r, new_rectvs2d(r, (2.0 * bb + r3o2 + 0.125) * hh, bb * hh, 0.125 * hh, hh, cc));
		}


	/* RGB Rectangular chart */
	} else if (schart == 0) {
		double bb = 0.07;		/* Border proportion */
		double hh = 50.0;		/* Height of hexagon in mm */
		double sc[6][3] = {		/* Saturated color sequence */
			{ 1, 0, 0 },
			{ 1, 0, 1 },
			{ 0, 0, 1 },
			{ 0, 1, 1 },
			{ 0, 1, 0 },
			{ 1, 1, 0 }
		};

		if (cmyk)
			error("CMYK not supported for test chart");

		h = (1.0 + 2.0 * bb) * hh;
		w = (2.0 * bb + 0.20 * 7.0) * hh;
	
		if ((r = new_render2d(w, h, NULL, res, res, rgb_2d, 0, depth, DITHER, NULL, NULL)) == NULL) {
			error("new_render2d() failed");
		}
	
		/* Set the default color */
		c[0] = 0.5;
		c[1] = 0.5;
		c[2] = 0.5;
		r->set_defc(r, c);
	
		for (i = 0; i < 7; i++) {
			prim2d *p;

			/* Top rectangle */
			cc[0][0] = sc[i % 6][0] * gbf + (1.0 - gbf) * 0.5;
			cc[0][1] = sc[i % 6][1] * gbf + (1.0 - gbf) * 0.5;
			cc[0][2] = sc[i % 6][2] * gbf + (1.0 - gbf) * 0.5;
			cc[1][0] = sc[(i+1) % 6][0] * gbf + (1.0 - gbf) * 0.5;
			cc[1][1] = sc[(i+1) % 6][1] * gbf + (1.0 - gbf) * 0.5;
			cc[1][2] = sc[(i+1) % 6][2] * gbf + (1.0 - gbf) * 0.5;
			cc[2][0] = 1.0 * gbf + (1.0 - gbf) * 0.5;
			cc[2][1] = 1.0 * gbf + (1.0 - gbf) * 0.5;
			cc[2][2] = 1.0 * gbf + (1.0 - gbf) * 0.5;
			cc[3][0] = 1.0 * gbf + (1.0 - gbf) * 0.5;
			cc[3][1] = 1.0 * gbf + (1.0 - gbf) * 0.5;
			cc[3][2] = 1.0 * gbf + (1.0 - gbf) * 0.5;
			p = new_rectvs2d(r, (bb + i * 0.2) * hh, (bb + 0.5) * hh, 0.2 * hh, 0.5 * hh, cc);
			if (smooth) {
				rectvs2d *pp = (rectvs2d *)p;
				pp->x_blend = 2;
				pp->y_blend = 3;
			}
			r->add(r, p);

			/* Bottom rectangle */
			cc[0][0] = 0.0 * gbf + (1.0 - gbf) * 0.5;
			cc[0][1] = 0.0 * gbf + (1.0 - gbf) * 0.5;
			cc[0][2] = 0.0 * gbf + (1.0 - gbf) * 0.5;
			cc[1][0] = 0.0 * gbf + (1.0 - gbf) * 0.5;
			cc[1][1] = 0.0 * gbf + (1.0 - gbf) * 0.5;
			cc[1][2] = 0.0 * gbf + (1.0 - gbf) * 0.5;
			cc[2][0] = sc[i % 6][0] * gbf + (1.0 - gbf) * 0.5;
			cc[2][1] = sc[i % 6][1] * gbf + (1.0 - gbf) * 0.5;
			cc[2][2] = sc[i % 6][2] * gbf + (1.0 - gbf) * 0.5;
			cc[3][0] = sc[(i+1) % 6][0] * gbf + (1.0 - gbf) * 0.5;
			cc[3][1] = sc[(i+1) % 6][1] * gbf + (1.0 - gbf) * 0.5;
			cc[3][2] = sc[(i+1) % 6][2] * gbf + (1.0 - gbf) * 0.5;
			p = new_rectvs2d(r, (bb + i * 0.2) * hh, bb * hh, 0.2 * hh, 0.5 * hh, cc);
			if (smooth) {
				rectvs2d *pp = (rectvs2d *)p;
				pp->x_blend = 2;
				pp->y_blend = 2;
			}
			r->add(r, p);
		}

	} else {	/* Lab step chart */
		double hh = 50.0;		/* Height of hexagon in mm */
		double bb = 0.05;		/* Border proportion */
		double ss, bs;			/* Step size, border size */

		h = hh;
		w = hh;

		if (cmyk)
			error("CMYK not supported for test chart");

		bs = (bb * hh)/(schart + 1.0);
		ss = hh * (1.0 - bb)/schart;
	
		if ((r = new_render2d(w, h, NULL, res, res, lab_2d, 0, depth, DITHER, NULL, NULL)) == NULL) {
			error("new_render2d() failed");
		}
	
		/* Set the default color */
		c[0] = 0.0;
		c[1] = 0.0;
		c[2] = 0.0;
		r->set_defc(r, c);
	
		for (i = 0; i < schart; i++) {
			for (j = 0; j < schart; j++) {
				double lv;
				
				lv = (double)(j * schart + i)/(schart * schart - 1.0) * 100.0;

				cc[0][0] = lv;
				cc[0][1] = -127.0;
				cc[0][2] = -127.0;
				cc[1][0] = lv;
				cc[1][1] =  127.0;
				cc[1][2] = -127.0;
				cc[2][0] = lv;
				cc[2][1] = -127.0;
				cc[2][2] =  127.0;
				cc[3][0] = lv;
				cc[3][1] =  127.0;
				cc[3][2] =  127.0;
				r->add(r, new_rectvs2d(r, bs + i * (bs + ss),
				                          bs + j * (bs + ss),
				                          ss, ss, cc));
			}
		}
	}

#ifdef PNG_MEM
	{
		char *nmode = "w";
		FILE *fp;
		unsigned char *buf;
		size_t len, wlen;

#if !defined(O_CREAT) && !defined(_O_CREAT)
# error "Need to #include fcntl.h!"
#endif
#if defined(O_BINARY) || defined(_O_BINARY)
		nmode = "wb";
#endif

		if (r->write(r, "MemoryBuf", 1, &buf, &len, png_mem))
			error("render->write failed");

		if ((fp = fopen(outname, nmode)) == NULL)
			error("render2d: open '%s' for writing",outname);

		if (len != (wlen = fwrite(buf, 1, len, fp)))
			error("render2d: writing %u bytes to '%s' failed (wrote %u)",len,outname,wlen);

		if (fclose(fp))
			error("render2d: failed to close after writing",outname);
	}
#else
	if (r->write(r, outname, 1, NULL, NULL, fmt))
		error("render->write failed");
#endif
	r->del(r);

	return 0;
}


