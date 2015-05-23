
/* 
 * Check various aspects of RGB or CMYK device link, 
 * and RGB/CMYK profile transfer characteristics.
 *
 * Author:  Graeme W. Gill
 * Date:    2003/9/7
 * Version: 1.0
 *
 * Copyright 2000 - 2003 Graeme W. Gill
 * Please refer to License.txt file for details.
 */

/* TTBD:
 *
 * Make general device input, not just CMYK
 *
 * Allow specifying a vector in the range space.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "icc.h"
#include "numlib.h"
#include "plot.h"
#include "ui.h"

void usage(void) {
	fprintf(stderr,"Check CMYK/RGB/PCS->PCS/CMYK/RGB transfer response\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: transplot [-v] infile\n");
	fprintf(stderr," -v        verbose\n");
	fprintf(stderr," -i intent     p = perceptual, r = relative colorimetric,\n");
	fprintf(stderr,"               s = saturation, a = absolute\n");
	fprintf(stderr," -o order      n = normal (priority: lut > matrix > monochrome)\n");
	fprintf(stderr,"               r = reverse (priority: monochrome > matrix > lut)\n");
	fprintf(stderr," -c -m -y -k  Check Cyan and/or Magenta and/or Yellow and/or Black input\n");
	fprintf(stderr," -r -g -b  Check Red and/or Green and/or Blue input\n");
	fprintf(stderr," -L -A -B  Check L and/or a* and/or b* input\n");
	exit(1);
}

#define XRES 256
//#define XRES 11

int
main(
	int argc,
	char *argv[]
) {
	int fa,nfa;				/* argument we're looking at */
	int verb = 0;
	int chans[4] = { 0, 0, 0, 0 };	/* Flags indicating which channels to plot against */
	char in_name[100];
	icmFile *rd_fp;
	icc *rd_icco;		/* Keep object separate */
	int rv = 0;

	/* Lookup parameters */
	icRenderingIntent intent = icmDefaultIntent;	/* Default */
	icmLookupOrder    order  = icmLuOrdNorm;		/* Default */

	/* Check variables */
	icmLuBase *luo;
	icmLuLut *luluto;	/* Lookup xLut type object */
	icColorSpaceSignature ins, outs;	/* Type of input and output spaces */
	int inn;							/* Number of input chanels */
	icmLuAlgType alg;
	int labin = 0;		/* Flag */
	int rgbin = 0;		/* Flag */
	int labout = 0;		/* Flag */
	int rgbout = 0;		/* Flag */
	
	error_program = argv[0];

	if (argc < 2)
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

			/* Verbosity */
			if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}

			/* Intent */
			else if (argv[fa][1] == 'i' || argv[fa][1] == 'I') {
				fa = nfa;
				if (na == NULL) usage();
    			switch (na[0]) {
					case 'p':
						intent = icPerceptual;
						break;
					case 'r':
						intent = icRelativeColorimetric;
						break;
					case 's':
						intent = icSaturation;
						break;
					case 'a':
						intent = icAbsoluteColorimetric;
						break;
					/* Special function icclib intents */
					case 'P':
						intent = icmAbsolutePerceptual;
						break;
					case 'S':
						intent = icmAbsoluteSaturation;
						break;
					default:
						usage();
				}
			}

			/* Search order */
			else if (argv[fa][1] == 'o' || argv[fa][1] == 'O') {
				fa = nfa;
				if (na == NULL) usage();
    			switch (na[0]) {
					case 'n':
					case 'N':
						order = icmLuOrdNorm;
						break;
					case 'r':
					case 'R':
						order = icmLuOrdRev;
						break;
					default:
						usage();
				}
			}

			/* Cyan, Red */
			else if (argv[fa][1] == 'c' || argv[fa][1] == 'C'
			      || argv[fa][1] == 'r' || argv[fa][1] == 'R') {
				chans[0] = 1;
			}
			/* Magenta, Green, a* */
			else if (argv[fa][1] == 'm' || argv[fa][1] == 'M'
			      || argv[fa][1] == 'g' || argv[fa][1] == 'G') {
				chans[1] = 1;
			}
			/* Yellow, Blue, b* */
			else if (argv[fa][1] == 'y' || argv[fa][1] == 'Y'
			      || argv[fa][1] == 'b') {
				chans[2] = 1;
			}
			/* L* */
			else if (argv[fa][1] == 'L') {
				chans[0] = 1;
				labin = 1;
			}
			/* a* */
			else if (argv[fa][1] == 'A') {
				chans[1] = 1;
				labin = 1;
			}
			/* Yellow, Blue, b* */
			else if (argv[fa][1] == 'B') {
				chans[2] = 1;
				labin = 1;
			}
			/* Black */
			else if (argv[fa][1] == 'k' || argv[fa][1] == 'K') {
				chans[3] = 1;
			}
			else if (argv[fa][1] == '?')
				usage();
			else 
				usage();
		}
		else
			break;
	}

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(in_name,argv[fa]);

	/* Open up the file for reading */
	if ((rd_fp = new_icmFileStd_name(in_name,"r")) == NULL)
		error ("Read: Can't open file '%s'",in_name);

	if ((rd_icco = new_icc()) == NULL)
		error ("Read: Creation of ICC object failed");

	/* Read the header and tag list */
	if ((rv = rd_icco->read(rd_icco,rd_fp,0)) != 0)
		error ("Read: %d, %s",rv,rd_icco->err);

	if (labin) {
		/* Get a Device to PCS conversion object */
		if ((luo = rd_icco->get_luobj(rd_icco, icmBwd, intent, icSigLabData, order)) == NULL)
			error ("%d, %s",rd_icco->errc, rd_icco->err);
	} else {
		/* Get a PCS to Device conversion object */
		if ((luo = rd_icco->get_luobj(rd_icco, icmFwd, intent, icSigLabData, order)) == NULL) {
			if ((luo = rd_icco->get_luobj(rd_icco, icmFwd, intent, icmSigDefaultData, order)) == NULL) {
				error ("%d, %s",rd_icco->errc, rd_icco->err);
			}
		}
	}

	/* Get details of conversion */
	luo->spaces(luo, &ins, &inn, &outs, NULL, &alg, NULL, NULL, NULL, NULL);

	if (labin) {
		chans[3] = 0;

		if (outs != icSigCmykData && outs != icSigRgbData) {
			error("Expecting CMYK or RGB output space");
		}

		if (outs == icSigRgbData) {
			rgbout = 1;
		}
	
	} else {
		if (ins != icSigCmykData && ins != icSigRgbData) {
			error("Expecting CMYK or RGB input space");
		}

		if (ins == icSigRgbData) {
			rgbin = 1;
			chans[3] = 0;
		}
	
		if (outs != icSigCmykData && outs != icSigLabData && outs != icSigRgbData) {
			error("Expecting Lab or CMYK or RGB output space");
		}

		if (outs == icSigLabData)
			labout = 1;
	}


	luluto = (icmLuLut *)luo;	/* Lookup xLut type object */

	{
		int i, j;
		double xx[XRES];
		double y0[XRES];
		double y1[XRES];
		double y2[XRES];
		double y3[XRES];

		for (i = 0; i < XRES; i++) {
			double ival = (double)i/(XRES-1.0);
			double in[4];
			double out[4];

			for (j = 0; j < 4; j++)
				in[j] = 0.0;

			if (labin) {
				in[0] = 0.5;
				in[1] = 0.5;
				in[2] = 0.5;
			}

			if (chans[0])
				in[0] = ival;
			if (chans[1])
				in[1] = ival;
			if (chans[2])
				in[2] = ival;
			if (chans[3])
				in[3] = ival;

			if (labin) {
				in[0] = in[0] * 100.0;
				in[1] = (in[1] - 0.5) * 254.0;
				in[2] = (in[2] - 0.5) * 254.0;
			}

			/* Do the conversion */
			if ((rv = luo->lookup(luo, out, in)) > 1)
				error ("%d, %s",rd_icco->errc,rd_icco->err);

			if (labout) {
//printf("~1 in %f %f %f %f, out %f %f %f\n",in[0],in[1],in[2],in[3],out[0],out[1],out[2]);
				xx[i] = ival;
				y0[i] = out[0]/1.0;
				y1[i] = 50.0 + out[1]/2.0;
				y2[i] = 50.0 + out[2]/2.0;
				if (y1[i] < 0.0)
					y1[i] = 0.0;
				if (y1[i] > 100.0)
					y1[i] = 100.0;
				if (y2[i] < 0.0)
					y2[i] = 0.0;
				if (y2[i] > 100.0)
					y2[i] = 100.0;
			} else {
				xx[i] = ival;
				y0[i] = out[0];
				y1[i] = out[1];
				y2[i] = out[2];
				y3[i] = out[3];
			}

		}
		if (labout)
			do_plot6(xx,y0,y1,NULL,NULL,y2,NULL,XRES);
		else {
			if (outs == icSigCmykData)
				do_plot6(xx,y3,y1,NULL,y0,y2,NULL,XRES);
			else 	/* Assume RGB */
				do_plot6(xx,NULL,y0,y1,y2,NULL,NULL,XRES);
		}
	}

	/* Done with lookup object */
	luo->del(luo);

	rd_icco->del(rd_icco);
	rd_fp->del(rd_fp);

	return 0;
}
