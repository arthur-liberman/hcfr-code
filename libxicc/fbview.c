
/* 
 * International Color Consortium Format Library (icclib)
 * View the gamut clipping implemented by the bwd profile.
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
#include "vrml.h"

#define TRES 43

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
	fprintf(stderr,"View bwd table clipping of an ICC file, , Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: fbtest [-v] infile\n");
	fprintf(stderr," -v        verbose\n");
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
	char in_name[100];
	char *xl, out_name[100];
	icmFile *rd_fp;
	icc *rd_icco;
	int rv = 0;
	icColorSpaceSignature ins, outs;	/* Type of input and output spaces */

	/* Check variables */
	int co[4];
	double in[4], out[4], check[4], temp[4];
	
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
	xl[0] = '\000';			/* Remove extension */

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
		double merr = 0.0;
		double aerr = 0.0;
		double nsamps = 0.0;
		icmLuBase *luo1, *luo2;
		int doaxes = 1;
		vrml *wrl;
		int i, j;
	

		/* Get a Device to PCS conversion object */
		if ((luo1 = rd_icco->get_luobj(rd_icco, icmFwd, icAbsoluteColorimetric, icSigLabData, icmLuOrdNorm)) == NULL) {
			if ((luo1 = rd_icco->get_luobj(rd_icco, icmFwd, icmDefaultIntent, icSigLabData, icmLuOrdNorm)) == NULL)
				error ("%d, %s",rd_icco->errc, rd_icco->err);
		}
		/* Get details of conversion */
		luo1->spaces(luo1, &ins, NULL, &outs, NULL, NULL, NULL, NULL, NULL, NULL);

		if (ins != icSigCmykData) {
			error("Expecting CMYK device");
		}
		
		/* Get a PCS to Device conversion object */
		if ((luo2 = rd_icco->get_luobj(rd_icco, icmBwd, icAbsoluteColorimetric,
		                               icSigLabData, icmLuOrdNorm)) == NULL) {
			if ((luo2 = rd_icco->get_luobj(rd_icco, icmBwd, icmDefaultIntent,
			                               icSigLabData, icmLuOrdNorm)) == NULL)
				error ("%d, %s",rd_icco->errc, rd_icco->err);
		}

		if ((wrl = new_vrml(out_name, doaxes, vrml_lab)) == NULL) {
			error("new_vrml for '%s%s' failed\n",out_name,vrml_ext());
		}

		wrl->start_line_set(wrl, 0);

		i = 0;
//		for (co[0] = 0; co[0] < TRES; co[0]++) {
			for (co[1] = 0; co[1] < TRES; co[1]++) {
				for (co[2] = 0; co[2] < TRES; co[2]++) {
					int rv1, rv2;
					double mxd, absd;

//					temp[0] = co[0]/(TRES-1.0);
					temp[1] = co[1]/(TRES-1.0);
					temp[2] = co[2]/(TRES-1.0);

//					in[0] = temp[0] * 100.0;
					in[0] = 0.0;
					in[1] = 200.0 * temp[1] -100.0;
					in[2] = 200.0 * temp[2] -100.0;


					/* PCS -> Device */
					if ((rv2 = luo2->lookup(luo2, out, in)) > 1)
						error ("%d, %s",rd_icco->errc,rd_icco->err);
	
					/* Device -> PCS */
					if ((rv1 = luo1->lookup(luo1, check, out)) > 1)
						error ("%d, %s",rd_icco->errc,rd_icco->err);
	
					if (verb) {
						printf("%f %f %f -> %f %f %f %f -> %f %f %f [%f]\n",
						in[0],in[1],in[2],out[0],out[1],out[2],out[3],check[0],check[1],check[2],
						maxdiff(check, in));
					}

					/* Check the result */
					mxd = maxdiff(check, in);
					if (mxd > merr)
						merr = mxd;

					nsamps++;
					absd = absdiff(check, in);
					aerr += absd;

					if (absd > 3.0) {
						wrl->add_vertex(wrl, 0, in);
						wrl->add_vertex(wrl, 0, check);
						i++;
					}
				}
			}
//		}

		wrl->make_lines(wrl, 0, 2);

		if (wrl->flush(wrl) != 0) {
			fprintf(stderr,"Error writint output file '%s%s'\n",out_name,vrml_ext());
			return 2;
		}
		wrl->del(wrl);

		printf("bwd to fwd check complete, peak err = %f, avg err = %f\n",merr,aerr/nsamps);

		/* Done with lookup object */
		luo1->del(luo1);
		luo2->del(luo2);
	}

	rd_icco->del(rd_icco);
	rd_fp->del(rd_fp);

	return 0;
}

