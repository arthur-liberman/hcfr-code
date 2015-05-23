
/* 
 * View inv fwd table device interp of an ICC file, V1.23\n");
 *
 * Author:  Graeme W. Gill
 * Date:    2000/12/8
 * Version: 1.23
 *
 * Copyright 2000 Graeme W. Gill
 * Please refer to License.txt file for details.
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
#include "icc.h"
#include "xicc.h"
#include "vrml.h"
#include "ui.h"

#define RW 0.5		/* Device Delta */ 

/* - - - - - - - - - - - - - */

/* Return maximum difference */
double maxdiff(double in1[3], double in2[3]) {
	double tt, rv = 0.0;
	if ((tt = fabs(in1[0] - in2[0])) > rv)
		rv = tt;
	if ((tt = fabs(in1[1] - in2[1])) > rv)
		rv = tt;
	if ((tt = fabs(in1[2] - in2[2])) > rv)
		rv = tt;
	return rv;
}

/* Return absolute difference */
double absdiff(double in1[3], double in2[3]) {
	double tt, rv = 0.0;
	tt = in1[0] - in2[0];
	rv += tt * tt;
	tt = in1[1] - in2[1];
	rv += tt * tt;
	tt = in1[2] - in2[2];
	rv += tt * tt;
	return sqrt(rv);
}

/* ---------------------------------------- */

void usage(void) {
	fprintf(stderr,"View inv fwd table device interp of an ICC file, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: fbtest [options] infile\n");
	fprintf(stderr," -v          verbose\n");
	fprintf(stderr," -f          Show PCS target -> reference clipped PCS vectors\n");
	fprintf(stderr," -d          Show PCS target -> average of device ref clippped PCS\n");
	fprintf(stderr," -b          Show PCS target -> B2A lookup clipped PCS\n");
	fprintf(stderr," -e          Show reference cliped PCS -> B2A lookup clipped PCS\n");
	fprintf(stderr," -r res      Resolution of test grid [Def 33]\n");
	fprintf(stderr," -g          Do full grid, not just L = 0\n");
	fprintf(stderr," -c          Do all values, not just clipped ones\n");
	fprintf(stderr," -l tlimit   set total ink limit, 0 - 400%% (estimate by default)\n");
	fprintf(stderr," -L klimit   set black ink limit, 0 - 100%% (estimate by default)\n");
	exit(1);
}

int
main(
	int argc,
	char *argv[]
) {
	int fa,nfa;				/* argument we're looking at */
	int verb = 0;
	int doaxes = 0;
	double tlimit = -1.0;	/* Total ink limit */
	double klimit = -1.0;	/* Black ink limit */
	int tres = 33;
	int doref = 0;			/* Show reference lookups */
	int dodelta = 0;		/* Show device interp delta variation */
	int dob2a = 0;			/* Show B2A lookups */
	int doeee = 0;			/* Show clipped ref PCS to B2A clipped PCS */
	int dilzero = 1;		/* Do just L = 0 plane */
	int doclip = 1;			/* Do just clipped values */
	char in_name[100];
	char *xl, out_name[100];
	icmFile *rd_fp;
	icc *rd_icco;
	int rv = 0;
	icColorSpaceSignature ins, outs;	/* Type of input and output spaces */

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
			/* Show reference */
			else if (argv[fa][1] == 'f' || argv[fa][1] == 'F') {
				doref = 1;
			}
			/* Show device delta variation */
			else if (argv[fa][1] == 'd' || argv[fa][1] == 'D') {
				dodelta = 1;
			}
			/* Show B2A table lookup */
			else if (argv[fa][1] == 'b' || argv[fa][1] == 'B') {
				dob2a = 1;
			}
			/* Show reference clipped PCS to clipped B2A PCS */
			else if (argv[fa][1] == 'e' || argv[fa][1] == 'E') {
				doeee = 1;
			}
			/* Do the full grid, not just L = 0 */
			else if (argv[fa][1] == 'g' || argv[fa][1] == 'G') {
				dilzero = 0;
			}
			/* Do all values, not just clipped ones */
			else if (argv[fa][1] == 'c' || argv[fa][1] == 'C') {
				doclip = 0;
			}
			/* Ink limit */
			else if (argv[fa][1] == 'l') {
				fa = nfa;
				if (na == NULL) usage();
				tlimit = atoi(na)/100.0;
			}
			else if (argv[fa][1] == 'L') {
				fa = nfa;
				if (na == NULL) usage();
				klimit = atoi(na)/100.0;
			}

			/* Resolution */
			else if (argv[fa][1] == 'r' || argv[fa][1] == 'R') {
				fa = nfa;
				if (na == NULL) usage();
				tres = atoi(na);
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

	strcpy(out_name, in_name);
	if ((xl = strrchr(out_name, '.')) == NULL)	/* Figure where extention is */
		xl = out_name + strlen(out_name);
	xl[0] = '\000';				/* Remove extension */

	/* Open up the file for reading */
	if ((rd_fp = new_icmFileStd_name(in_name,"r")) == NULL)
		error ("Read: Can't open file '%s'",in_name);

	if ((rd_icco = new_icc()) == NULL)
		error ("Read: Creation of ICC object failed");

	/* Read the header and tag list */
	if ((rv = rd_icco->read(rd_icco,rd_fp,0)) != 0)
		error ("Read: %d, %s",rv,rd_icco->err);

	/* Run the target Lab values through the bwd and fwd tables, */
	/* to compute the overall error. */
	{
#define GAMUT_LCENT 50.0
		xicc *xicco;
		icxLuBase *luo;
		icxInk ink;					/* Ink parameters */
		vrml *wrl;
		struct {
			double x, y, z;
			double wx, wy, wz;
			double r, g, b;
		} axes[5] = {
			{ 0, 0,  50-GAMUT_LCENT,  2, 2, 100,  .7, .7, .7 },	/* L axis */
			{ 50, 0,  0-GAMUT_LCENT,  100, 2, 2,   1,  0,  0 },	/* +a (red) axis */
			{ 0, -50, 0-GAMUT_LCENT,  2, 100, 2,   0,  0,  1 },	/* -b (blue) axis */
			{ -50, 0, 0-GAMUT_LCENT,  100, 2, 2,   0,  1,  0 },	/* -a (green) axis */
			{ 0,  50, 0-GAMUT_LCENT,  2, 100, 2,   1,  1,  0 },	/* +b (yellow) axis */
		};
		int coa[4];
		int i, j;
	
		/* Wrap with an expanded icc */
		if ((xicco = new_xicc(rd_icco)) == NULL)
			error ("Creation of xicc failed");
	
		/* Set the default ink limits if not set on command line */
		icxDefaultLimits(xicco, &ink.tlimit, tlimit, &ink.klimit, klimit);

		if (verb) {
			if (ink.tlimit >= 0.0)
				printf("Total ink limit assumed is %3.0f%%\n",100.0 * ink.tlimit);
			if (ink.klimit >= 0.0)
				printf("Black ink limit assumed is %3.0f%%\n",100.0 * ink.klimit);
		}
	
		ink.c.Ksmth = ICXINKDEFSMTH;	/* Default smoothing */
		ink.c.Kskew = ICXINKDEFSKEW;	/* default curve skew */
		ink.c.Kstle = 0.5;		/* Min K at white end */
		ink.c.Kstpo = 0.5;		/* Start of transition is at white */ 	
		ink.c.Kenle = 0.5;		/* Max K at black end */
		ink.c.Kenpo = 0.5;		/* End transition at black */
		ink.c.Kshap = 1.0;		/* Linear transition */

		/* Get a Device to PCS conversion object */
		if ((luo = xicco->get_luobj(xicco, ICX_CLIP_NEAREST, icmFwd, icAbsoluteColorimetric, icSigLabData, icmLuOrdNorm, NULL, &ink)) == NULL) {
			if ((luo = xicco->get_luobj(xicco, ICX_CLIP_NEAREST, icmFwd, icmDefaultIntent, icSigLabData, icmLuOrdNorm, NULL, &ink)) == NULL)
				error ("%d, %s",rd_icco->errc, rd_icco->err);
		}
		/* Get details of conversion */
		luo->spaces(luo, &ins, NULL, &outs, NULL, NULL, NULL, NULL, NULL);

		if (ins != icSigCmykData) {
			error("Expecting CMYK device");
		}
		
		if ((wrl = new_vrml(out_name, doaxes, vrml_lab)) == NULL) {
			fprintf(stderr,"new_vrml failed for '%s%s'\n",out_name,vrml_ext());
			return 2;
		}

		/* ---------------------------------------------- */
		/* The PCS target -> Reference clipped vectors */

		if (doref) {
			double rgb[3];

			if (verb)
				printf("Doing PCS target to reference clipped PCS Vectors\n");

			wrl->start_line_set(wrl, 0);

			i = 0;
			for (coa[0] = 0; coa[0] < tres; coa[0]++) {
				for (coa[1] = 0; coa[1] < tres; coa[1]++) {
					for (coa[2] = 0; coa[2] < tres; coa[2]++) {
						double in[4], dev[4], out[4];
						double temp[4];
						int rv1, rv2;

						temp[0] = coa[0]/(tres-1.0);
						temp[1] = coa[1]/(tres-1.0);
						temp[2] = coa[2]/(tres-1.0);

						/* PCS values */
						in[0] = temp[0] * 100.0;
						in[1] = 200.0 * temp[1] -100.0;
						in[2] = 200.0 * temp[2] -100.0;

						/* Do reference lookup */

						/* PCS -> Device */
						if ((rv2 = luo->inv_lookup(luo, dev, in)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						if (doclip && rv2 != 1)	/* Not clip */
							continue;

						/* Device -> PCS */
						if ((rv1 = luo->lookup(luo, out, dev)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						if (verb) 
							printf("."), fflush(stdout);

						/* Input PCS to ideal (Inverse AtoB) clipped PCS values */
						wrl->add_vertex(wrl, 0, in);
						wrl->add_vertex(wrl, 0, out);
						i++;
					}
				}
				if (dilzero)
					break;
			}

			if (verb)
				printf("\n");

			for (j = 0; j < i; j++) {
				int ix[2];
				ix[0] = j * 2;
				ix[1] = j * 2 +1;
				wrl->add_line(wrl, 0, ix);
			}

			rgb[0] = 1.0;
			rgb[1] = 0.1;
			rgb[2] = 0.1;
			wrl->make_lines_cc(wrl, 0, 0.0, rgb);
		}

		/* ---------------------------------------------- */
		/* The Device Delta lines */
		/* The PCS target -> clipped from average of surrounding device values, vectors */

		if (dodelta) {
			double rgb[3];

			if (verb)
				printf("Doing target PCS to average of 4 surrounding device to PCS Vectors\n");

			wrl->start_line_set(wrl, 0);

			i = 0;
			for (coa[0] = 0; coa[0] < tres; coa[0]++) {
				for (coa[1] = 0; coa[1] < tres; coa[1]++) {
					for (coa[2] = 0; coa[2] < tres; coa[2]++) {
						double in[4], dev[4], out[4];
						double in4[4], check[4];
						double temp[4], adev[4];
						double dev0[4], dev1[4], dev2[4], dev3[4];
						int rv1, rv2;

						temp[0] = coa[0]/(tres-1.0);
						temp[1] = coa[1]/(tres-1.0);
						temp[2] = coa[2]/(tres-1.0);

						in[0] = temp[0] * 100.0;
						in[1] = 200.0 * temp[1] -100.0;
						in[2] = 200.0 * temp[2] -100.0;

						/* Do reference lookup */

						/* PCS -> ideal Device */
						if ((rv2 = luo->inv_lookup(luo, dev, in)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						if (doclip && rv2 != 1)	/* Not clip */
							continue;

						/* Device -> PCS check value */
						if ((rv1 = luo->lookup(luo, check, dev)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						/* - - - - - - - - - - - - - - - */
						/* Now do average in device space of two points */
						temp[0] = coa[0]/(tres-1.0);
						temp[1] = (coa[1]-RW)/(tres-1.0);
						temp[2] = (coa[2]-RW)/(tres-1.0);

						in4[0] = temp[0] * 100.0;
						in4[1] = 200.0 * temp[1] -100.0;
						in4[2] = 200.0 * temp[2] -100.0;

						/* PCS -> Device */
						if ((rv2 = luo->inv_lookup(luo, dev0, in4)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						adev[0] = 0.25 * dev0[0];
						adev[1] = 0.25 * dev0[1];
						adev[2] = 0.25 * dev0[2];
						adev[3] = 0.25 * dev0[3];

						temp[0] = coa[0]/(tres-1.0);
						temp[1] = (coa[1]+RW)/(tres-1.0);
						temp[2] = (coa[2]-RW)/(tres-1.0);

						in4[0] = temp[0] * 100.0;
						in4[1] = 200.0 * temp[1] -100.0;
						in4[2] = 200.0 * temp[2] -100.0;

						/* PCS -> Device */
						if ((rv2 = luo->inv_lookup(luo, dev1, in4)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						adev[0] += 0.25 * dev1[0];
						adev[1] += 0.25 * dev1[1];
						adev[2] += 0.25 * dev1[2];
						adev[3] += 0.25 * dev1[3];

						temp[0] = coa[0]/(tres-1.0);
						temp[1] = (coa[1]-RW)/(tres-1.0);
						temp[2] = (coa[2]+RW)/(tres-1.0);

						in4[0] = temp[0] * 100.0;
						in4[1] = 200.0 * temp[1] -100.0;
						in4[2] = 200.0 * temp[2] -100.0;

						/* PCS -> Device */
						if ((rv2 = luo->inv_lookup(luo, dev2, in4)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						adev[0] += 0.25 * dev2[0];
						adev[1] += 0.25 * dev2[1];
						adev[2] += 0.25 * dev2[2];
						adev[3] += 0.25 * dev2[3];

						temp[0] = coa[0]/(tres-1.0);
						temp[1] = (coa[1]+RW)/(tres-1.0);
						temp[2] = (coa[2]+RW)/(tres-1.0);

						in4[0] = temp[0] * 100.0;
						in4[1] = 200.0 * temp[1] -100.0;
						in4[2] = 200.0 * temp[2] -100.0;

						/* PCS -> Device */
						if ((rv2 = luo->inv_lookup(luo, dev3, in4)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						adev[0] += 0.25 * dev3[0];
						adev[1] += 0.25 * dev3[1];
						adev[2] += 0.25 * dev3[2];
						adev[3] += 0.25 * dev3[3];

						/* Device -> PCS */
						if ((rv1 = luo->lookup(luo, out, adev)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						if (verb)
							printf("."), fflush(stdout);

						wrl->add_vertex(wrl, 0, in);
						wrl->add_vertex(wrl, 0, out);
						i++;
					}
				}
				if (dilzero)
					break;
			}

			if (verb)
				printf("\n");

			for (j = 0; j < i; j++) {
				int ix[2];
				ix[0] = j * 2;
				ix[1] = j * 2 +1;
				wrl->add_line(wrl, 0, ix);
			}

			rgb[0] = 0.9;
			rgb[1] = 0.9;
			rgb[2] = 0.9;
			wrl->make_lines_cc(wrl, 0, 0.0, rgb);
		}

		/* ---------------------------------------------- */
		/* The target PCS -> clipped PCS using B2A table vectore */

		if (dob2a) {
			double rgb[3];

			icxLuBase *luoB;

			wrl->start_line_set(wrl, 0);

			/* Get a PCS to Device conversion object */
			if ((luoB = xicco->get_luobj(xicco, ICX_CLIP_NEAREST, icmBwd, icAbsoluteColorimetric,
			                             icSigLabData, icmLuOrdNorm, NULL, &ink)) == NULL) {
				if ((luoB = xicco->get_luobj(xicco, ICX_CLIP_NEAREST, icmBwd, icmDefaultIntent,
				                             icSigLabData, icmLuOrdNorm, NULL, &ink)) == NULL)
					error ("%d, %s",rd_icco->errc, rd_icco->err);
			}

			if (verb)
				printf("Doing target PCS to B2A clipped PCS Vectors\n");

			i = 0;
			for (coa[0] = 0; coa[0] < tres; coa[0]++) {
				for (coa[1] = 0; coa[1] < tres; coa[1]++) {
					for (coa[2] = 0; coa[2] < tres; coa[2]++) {
						double in[4], dev[4], out[4];
						double temp[4];
						int rv1, rv2;

						temp[0] = coa[0]/(tres-1.0);
						temp[1] = coa[1]/(tres-1.0);
						temp[2] = coa[2]/(tres-1.0);

						in[0] = temp[0] * 100.0;
						in[1] = 200.0 * temp[1] -100.0;
						in[2] = 200.0 * temp[2] -100.0;

						/* PCS -> Device */
						if ((rv2 = luoB->lookup(luoB, dev, in)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						if (doclip && rv2 != 1)	/* Not clip */
							continue;

						/* Device -> PCS */
						if ((rv1 = luo->lookup(luo, out, dev)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						if (verb)
							printf("."), fflush(stdout);

						wrl->add_vertex(wrl, 0, in);
						wrl->add_vertex(wrl, 0, out);
						i++;
					}
				}
				if (dilzero)
					break;
			}

			if (verb)
				printf("\n");

			for (j = 0; j < i; j++) {
				int ix[2];
				ix[0] = j * 2;
				ix[1] = j * 2 +1;
				wrl->add_line(wrl, 0, ix);
			}

			rgb[0] = 0.9;
			rgb[1] = 0.9;
			rgb[2] = 0.9;
			wrl->make_lines_cc(wrl, 0, 0.0, rgb);

			luoB->del(luoB);
		}

		/* ---------------------------------------------- */
		/* The reference clipped PCS -> B2A clipped PCS vectore */

		if (doeee) {
			double rgb[3];

			icxLuBase *luoB;

			wrl->start_line_set(wrl, 0);

			/* Get a PCS to Device conversion object */
			if ((luoB = xicco->get_luobj(xicco, ICX_CLIP_NEAREST, icmBwd, icAbsoluteColorimetric,
			                             icSigLabData, icmLuOrdNorm, NULL, &ink)) == NULL) {
				if ((luoB = xicco->get_luobj(xicco, ICX_CLIP_NEAREST, icmBwd, icmDefaultIntent,
				                             icSigLabData, icmLuOrdNorm, NULL, &ink)) == NULL)
					error ("%d, %s",rd_icco->errc, rd_icco->err);
			}

			if (verb)
				printf("Doing reference clipped PCS to B2A table clipped PCS Vectors\n");

			i = 0;
			for (coa[0] = 0; coa[0] < tres; coa[0]++) {
				for (coa[1] = 0; coa[1] < tres; coa[1]++) {
					for (coa[2] = 0; coa[2] < tres; coa[2]++) {
						double in[4], dev[4], out[4];
						double check[4];
						double temp[4];
						int rv1, rv2;

						temp[0] = coa[0]/(tres-1.0);
						temp[1] = coa[1]/(tres-1.0);
						temp[2] = coa[2]/(tres-1.0);

						in[0] = temp[0] * 100.0;
						in[1] = 200.0 * temp[1] -100.0;
						in[2] = 200.0 * temp[2] -100.0;

						/* Do reference lookup */
						/* PCS -> Device */
						if ((rv1 = luo->inv_lookup(luo, dev, in)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						/* Device -> PCS */
						if (luo->lookup(luo, check, dev) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						/* Do B2A table lookup */
						/* PCS -> Device */
						if ((rv2 = luoB->lookup(luoB, dev, in)) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						/* Device -> PCS */
						if (luo->lookup(luo, out, dev) > 1)
							error ("%d, %s",rd_icco->errc,rd_icco->err);
		
						if (doclip && rv1 != 1 && rv2 != 1)	/* Not clip */
							continue;

						if (verb)
							printf("."), fflush(stdout);

						wrl->add_vertex(wrl, 0, check);
						wrl->add_vertex(wrl, 0, out);
						i++;
					}
				}
				if (dilzero)
					break;
			}

			if (verb)
				printf("\n");

			for (j = 0; j < i; j++) {
				int ix[2];
				ix[0] = j * 2;
				ix[1] = j * 2 +1;
				wrl->add_line(wrl, 0, ix);
			}

			rgb[0] = 0.9;
			rgb[1] = 0.9;
			rgb[2] = 0.9;
			wrl->make_lines_cc(wrl, 0, 0.0, rgb);

			luoB->del(luoB);
		}

		/* ---------------------------------------------- */

		if (wrl->flush(wrl) != 0) {
			fprintf(stderr,"Error closing output file '%s%s'\n",out_name,vrml_ext());
			return 2;
		}
		wrl->del(wrl);

		/* Done with lookup object */
		luo->del(luo);
		xicco->del(xicco);
	}

	rd_icco->del(rd_icco);
	rd_fp->del(rd_fp);

	return 0;
}

