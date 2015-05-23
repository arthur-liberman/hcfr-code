
/* 
 * Argyll.
 * 
 * Check for B2A table PCS->Device interpolation faults
 *
 * Author:  Graeme W. Gill
 * Date:    2000/12/11
 * Version: 1.00
 *
 * Copyright 2000 Graeme W. Gill
 * Please refer to License.txt file for details.
 */

/*
	Estimate PCS->device interpolation error by comparing
	the interpolated PCS value in the cell to the PCS computed
	from the corners of the cell.
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
#include "numlib.h"
#include "copyright.h"
#include "aconfig.h"
#include "icc.h"
#include "vrml.h"

void usage(void) {
	fprintf(stderr,"Check PCS->Device Interpolation faults of ICC file, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: icheck [-v] [-w] infile\n");
	fprintf(stderr," -v        verbose\n");
	fprintf(stderr," -a        Show all interpolation errors, not just worst\n");
	fprintf(stderr," -w        create %s visualisation\n",vrml_format());
	fprintf(stderr," -x        Use %s axies\n",vrml_format());
	exit(1);
}

int
main(
	int argc,
	char *argv[]
) {
	int fa,nfa;				/* argument we're looking at */
	int verb = 0;
	int doall = 0;
	int dovrml = 0;
	int doaxes = 0;
	char in_name[100];
	char out_name[100], *xl;
	icmFile *rd_fp;
	icc *rd_icco;
	int rv = 0;

	/* Check variables */
	icmLuBase *luof, *luob;		/* A2B and B2A table lookups */
	icmLuLut *lluof, *lluob;	/* Lookup Lut type object */
	int gres;					/* Grid resolution of B2A */
	icColorSpaceSignature ins, outs;	/* Type of input and output spaces */
	int inn;							/* Number of input chanels */
	icmLuAlgType alg;
	vrml *wrl = NULL;
	
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
			/* VRML/X3D */
			else if (argv[fa][1] == 'w' || argv[fa][1] == 'W') {
				dovrml = 1;
			}
			/* Axes */
			else if (argv[fa][1] == 'x' || argv[fa][1] == 'X') {
				doaxes = 1;
			}
			else if (argv[fa][1] == 'a') {
				doall = 1;
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

	/* Get a Device to PCS conversion object */
	if ((luof = rd_icco->get_luobj(rd_icco, icmFwd, icAbsoluteColorimetric, icSigLabData, icmLuOrdNorm)) == NULL) {
		if ((luof = rd_icco->get_luobj(rd_icco, icmFwd, icmDefaultIntent, icSigLabData, icmLuOrdNorm)) == NULL)
			error ("%d, %s",rd_icco->errc, rd_icco->err);
	}

	/* Get a PCS to Device conversion object */
	if ((luob = rd_icco->get_luobj(rd_icco, icmBwd, icAbsoluteColorimetric, icSigLabData, icmLuOrdNorm)) == NULL) {
		if ((luob = rd_icco->get_luobj(rd_icco, icmBwd, icmDefaultIntent, icSigLabData, icmLuOrdNorm)) == NULL)
			error ("%d, %s",rd_icco->errc, rd_icco->err);
	}

	/* Get details of conversion (for B2A direction) */
	luob->spaces(luob, &outs, NULL, &ins, &inn, &alg, NULL, NULL, NULL, NULL);

	if (alg != icmLutType) {
		error("Expecting Lut based profile");
	}

	if (outs != icSigLabData) {
		error("Expecting Lab PCS");
	}

	lluof = (icmLuLut *)luof;	/* Lookup Lut type object */
	lluob = (icmLuLut *)luob;	/* Lookup Lut type object */

	gres = lluob->lut->clutPoints;

	if (dovrml) {
		wrl = new_vrml(out_name, doaxes, vrml_lab);
		if (wrl == NULL)
			error("new_vrml failed for '%s%s'",out_name,vrml_ext());
		wrl->start_line_set(wrl, 0);
	}

	{
		double aerr = 0.0;
		double ccount = 0.0;
		double merr = 0.0;
		double tcount = 0.0;
		int co[3];	/* PCS grid counter */

		/* Itterate throught the PCS clut grid cells */
		for (co[2] = 0; co[2] < (gres-1); co[2]++) {
			for (co[1] = 0; co[1] < (gres-1); co[1]++) {
				for (co[0] = 0; co[0] < (gres-1); co[0]++) {
					int j, k, m;
					int cc[3];				/* Cube corner offsets */
					double pcs[8][3], wpcsd;
					double apcs[3];
					double adev[MAX_CHAN];
					double check[3];		/* Check PCS */
					double ier;				/* Interpolation error */

					apcs[0] = apcs[1] = apcs[2] = 0.0;
					for (k = 0; k < inn; k++)
						adev[k] = 0.0;

					/* For each corner of the PCS grid based at the current point, */
					/* average the PCS and Device values */
					m = 0;
					for (cc[2] = 0; cc[2] < 2; cc[2]++) {
						for (cc[1] = 0; cc[1] < 2; cc[1]++) {
							for (cc[0] = 0; cc[0] < 2; cc[0]++, m++) {
								double dev[MAX_CHAN];

								pcs[m][0] = (co[0] + cc[0])/(gres - 1.0);
								pcs[m][1] = (co[1] + cc[1])/(gres - 1.0);
								pcs[m][2] = (co[2] + cc[2])/(gres - 1.0);

								/* Match icclib settable() range */
								pcs[m][0] = pcs[m][0] * 100.0;
								pcs[m][1] = (pcs[m][1] * 254.0) - 127.0;
								pcs[m][2] = (pcs[m][2] * 254.0) - 127.0;

//printf("Input PCS %f %f %f\n", pcs[m][0], pcs[m][1], pcs[m][2]);

								/* PCS to (cliped) Device */
								if ((rv = lluob->clut(lluob, dev, pcs[m])) > 1)
									error ("%d, %s",rd_icco->errc,rd_icco->err);

								/* (clipped) Device to (clipped) PCS */
								if ((rv = lluof->clut(lluof, pcs[m], dev)) > 1)
									error ("%d, %s",rd_icco->errc,rd_icco->err);

								apcs[0] += pcs[m][0];
								apcs[1] += pcs[m][1];
								apcs[2] += pcs[m][2];

//printf("Corner PCS %f %f %f -> %f %f %f %f\n",
//pcs[m][0], pcs[m][1], pcs[m][2], dev[0], dev[1], dev[2], dev[3]);

								for (k = 0; k < inn; k++)
									adev[k] += dev[k];
							}
						}
					}

					for (j = 0; j < 3; j++)
						apcs[j] /= 8.0;

					for (k = 0; k < inn; k++)
						adev[k] /= 8.0;
	
					/* Compute worst case distance of PCS corners to PCS of average dev */
					wpcsd = 0.0;
					for (m = 0; m < 8; m++) {
						double ss;
						for (ss = 0.0, j = 0; j < 3; j++) {
							double tt = pcs[m][j] - apcs[j];
							ss += tt * tt;
						}
						ss = sqrt(ss);
						if (ss > wpcsd)
							wpcsd = ss;
					}
					wpcsd *= 0.15;		/* Set threshold at 75% of most distant corner */

//printf("~1 wpcsd = %f\n",wpcsd);

					/* Set a worst case */
					if (wpcsd < 1.0)
						wpcsd = 1.0;

//					else if (wpcsd > 3.0)
//						wpcsd = 3.0;


//printf("Average PCS %f %f %f, Average Device %f %f %f %f\n",
//apcs[0], apcs[1], apcs[2], adev[0], adev[1], adev[2], adev[3]);

					/* Average Device to PCS */
					if ((rv = lluof->clut(lluof, check, adev)) > 1)
						error ("%d, %s",rd_icco->errc,rd_icco->err);

//printf("Check PCS %f %f %f\n",
//check[0], check[1], check[2]);

					/* Compute error in PCS vs. Device interpolation */
					for (ier = 0.0, j = 0; j < 3; j++) {
						double tt = apcs[j] - check[j];
						ier += tt * tt;
					}
					ier = sqrt(ier);

//printf("Average PCS %f %f %f, Check PCS %f %f %f, error %f\n",
//apcs[0], apcs[1], apcs[2], check[0], check[1], check[2], ier);

					aerr += ier;
					ccount++;
					if (ier > merr)
						merr = ier;

					if (doall || ier > wpcsd) {
						tcount++;

						printf("ier = %f, Dev = %f %f %f %f\n",
						       ier, adev[0], adev[1], adev[2], adev[3]);
						if (dovrml) {
							wrl->add_vertex(wrl, 0, apcs);
							wrl->add_vertex(wrl, 0, check);
						}
					}

//printf("~1 ier = %f\n",ier);
//printf("\n");


					if (verb)
						printf("."), fflush(stdout);
				}
			}
		}

		if (dovrml) {
			wrl->make_lines(wrl, 0, 2);
			wrl->del(wrl);
		}
	
		aerr /= ccount;
	
		printf("Average interpolation error %f, maximum %f\n",aerr, merr);
		printf("Number outside corner radius = %f%%\n",tcount * 100.0/ccount);
	}

	/* Done with lookup objects */
	luof->del(luof);
	luob->del(luob);

	rd_icco->del(rd_icco);
	rd_fp->del(rd_fp);

	return 0;
}

