/* 
 * Argyll Color Correction System
 * Average one or more .ti3 (or other CGATS like) file values together.
 *
 * Author: Graeme W. Gill
 * Date:   18/1/2011
 *
 * Copyright 2005, 2010, 2011 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * (based on splitti3.c)
 */

/*
 * TTBD:
	
	Should probably re-index SAMPLE_ID field

 */

#define DEBUG

#define verbo stdout

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "cgats.h"
#include "xicc.h"
#include "insttypes.h"

void usage(char *diag, ...) {
	int i;
	fprintf(stderr,"Average or merge values in .ti3 like files, Version %s\n",ARGYLL_VERSION_STR);
	if (diag != NULL) {
		va_list args;
		fprintf(stderr,"  Diagnostic: ");
		va_start(args, diag);
		vfprintf(stderr, diag, args);
		va_end(args);
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	fprintf(stderr,"usage: average [-options] input1.ti3 input2.ti3 ... output.ti3\n");
	fprintf(stderr," -v              Verbose\n");
	fprintf(stderr," -m              Merge rather than average\n");
	fprintf(stderr," input1.ti3      First input file\n");
	fprintf(stderr," input2.ti3      Second input file\n");
	fprintf(stderr," ...             etc.\n");
	fprintf(stderr," output.ti3      Resulting averaged or merged output file\n");
	exit(1);
}

/* Information about each file */
struct _inpinfo {
	char name[MAXNAMEL+1];
	cgats *c;
}; typedef struct _inpinfo inpinfo;

int main(int argc, char *argv[]) {
	int fa,nfa;					/* current argument we're looking at */
	int verb = 0;
	int domerge = 0;			/* Merge rather than average */

	int ninps = 0;				/* Number of input files */
	inpinfo *inps;				/* Input file info. inp[ninp] == output file */
	cgats *ocg;					/* Copy of output cgats * */

	cgats_set_elem *setel;		/* Array of set value elements */
	int *flags;					/* Point to destination of set */

	int nchan;					/* Number of device channels */
	int chix[ICX_MXINKS];		/* Device chanel indexes */
	int pcsix[3];				/* PCS chanel indexes */
	int isLab = 0;

	int i, j, n;

	error_program = "average";

	if (argc <= 3)
		usage("Too few arguments (%d, minimum is 2)",argc-1);

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
				usage("Usage requested");

			/* Merge */
			else if (argv[fa][1] == 'm' || argv[fa][1] == 'M') {
				domerge = 1;
			}

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}

			else  {
				usage("Unknown flag '%c'",argv[fa][1]);
			}

		} else if (argv[fa][0] != '\000') {
			/* Get the next filename */
		
			if (ninps == 0)
				inps = (inpinfo *)malloc(sizeof(inpinfo));
			else
				inps = (inpinfo *)realloc(inps, (ninps+1) * sizeof(inpinfo));
			if (inps == NULL)
				error("Malloc failed in allocating space for file info.");

			memset((void *)&inps[ninps], 0, sizeof(inpinfo));
			strncpy(inps[ninps].name,argv[fa],MAXNAMEL);
			inps[ninps].name[MAXNAMEL] = '\000';

			ninps++;
		} else {
			break;
		}
	}

	if (ninps < 2)
		error("Must be at least one input and one output file specified");

	ninps--;	/* Number of inputs */

	/* Open and read each input file */
	for (n = 0; n <= ninps; n++) {

		if ((inps[n].c = new_cgats()) == NULL)
			error("Failed to create cgats object for file '%s'",inps[n].name);
	
		if (n < ninps) {		/* If input file, read it */
			inps[n].c->add_other(inps[n].c, ""); 	/* Allow any signature file */
		
			if (inps[n].c->read_name(inps[n].c, inps[n].name))
				error("CGATS file '%s' read error : %s",inps[n].name,inps[n].c->err);
		
			if (inps[n].c->ntables < 1)
				error ("Input file '%s' doesn't contain at least one table",inps[n].name);
		}
	}
	ocg = inps[ninps].c;		/* Alias for output file */

	/* Duplicate everything from the first input file into the output file. */
	for (n = 0; n < inps[0].c->ntables; n++) {

		if (inps[0].c->t[n].tt == cgats_X) {
			ocg->add_other(ocg, inps[0].c->cgats_type);
			ocg->add_table(ocg, tt_other, 0);
		} else if (inps[0].c->t[n].tt == tt_other) {
			int oi;
			oi = ocg->add_other(ocg, inps[0].c->others[inps[0].c->t[n].oi]);
			ocg->add_table(ocg, tt_other, oi);
		} else {
			ocg->add_table(ocg, inps[0].c->t[n].tt, 0);
		}

		/* Duplicate all the keywords */
		for (i = 0; i < inps[0].c->t[n].nkwords; i++) {
//printf("~1 table %d, adding keyword '%s'\n",n,inps[0].c->t[n].ksym[i]);
			ocg->add_kword(ocg, n, inps[0].c->t[n].ksym[i], inps[0].c->t[n].kdata[i], NULL);
		}

		/* Duplicate all of the fields */
		for (i = 0; i < inps[0].c->t[n].nfields; i++) {
			ocg->add_field(ocg, n, inps[0].c->t[n].fsym[i], inps[0].c->t[n].ftype[i]);
		}

		/* Duplicate all of the data */
		if ((setel = (cgats_set_elem *)malloc(
		     sizeof(cgats_set_elem) * inps[0].c->t[n].nfields)) == NULL)
			error("Malloc failed!");

		for (i = 0; i < inps[0].c->t[n].nsets; i++) {
			inps[0].c->get_setarr(inps[0].c, n, i, setel);
			ocg->add_setarr(ocg, n, setel);
		}
		free(setel);
	}

	/* Figure out the indexes of the device channels */
	{
		int ti;
		char *buf;
		char *xyzfname[3] = { "XYZ_X", "XYZ_Y", "XYZ_Z" };
		char *labfname[3] = { "LAB_L", "LAB_A", "LAB_B" };
		char *outc;
		int nmask;
		char *bident;

		if ((ti = inps[0].c->find_kword(inps[0].c, 0, "COLOR_REP")) < 0)
			error("Input file '%s' doesn't contain keyword COLOR_REP", inps[0].name);
		
		if ((buf = strdup(inps[0].c->t[0].kdata[ti])) == NULL)
			error("Malloc failed");
		
		/* Split COLOR_REP into device and PCS space */
		if ((outc = strchr(buf, '_')) == NULL)
			error("COLOR_REP '%s' invalid", inps[0].c->t[0].kdata[ti]);
		*outc++ = '\000';
		
		if (strcmp(outc, "XYZ") == 0) {
			isLab = 0;
		} else if (strcmp(outc, "LAB") == 0) {
			isLab = 1;
		} else
			error("COLOR_REP '%s' invalid (Neither XYZ nor LAB)", inps[0].c->t[0].kdata[ti]);
	
		if ((nmask = icx_char2inkmask(buf)) == 0) {
			error ("File '%s' keyword COLOR_REP has unknown device value '%s'",inps[0].name,buf);
		}

		nchan = icx_noofinks(nmask);
		bident = icx_inkmask2char(nmask, 0); 

		/* Find device fields */
		for (j = 0; j < nchan; j++) {
			int ii, imask;
			char fname[100];

			imask = icx_index2ink(nmask, j);
			sprintf(fname,"%s_%s",nmask == ICX_W || nmask == ICX_K ? "GRAY" : bident,
			                      icx_ink2char(imask));

			if ((ii = inps[0].c->find_field(inps[0].c, 0, fname)) < 0)
				error ("Input file doesn't contain field %s",fname);
			if (inps[0].c->t[0].ftype[ii] != r_t)
				error ("Field %s is wrong type",fname);
			chix[j] = ii;
		}

		/* Find PCS fields */
		for (j = 0; j < 3; j++) {
			int ii;

			if ((ii = inps[0].c->find_field(inps[0].c, 0, isLab ? labfname[j] : xyzfname[j])) < 0)
				error ("Input file doesn't contain field %s",isLab ? labfname[j] : xyzfname[j]);
			if (inps[0].c->t[0].ftype[ii] != r_t)
				error ("Field %s is wrong type",isLab ? labfname[j] : xyzfname[j]);
			pcsix[j] = ii;
		}
		free(bident);
	}
	
	if (!domerge && verb) {
		printf("Averaging the following fields:");
		for (j = 0; j < inps[0].c->t[0].nfields; j++) {
			int jj;

			/* Only real types */
			if (inps[0].c->t[0].ftype[j] != r_t) {
				continue;
			}

			/* Not device channels */
			for (jj = 0; jj < nchan; jj++) {
				if (chix[jj] == j)
					break;
			}
			if (jj < nchan) {
				continue;
			}

			printf(" %s",inps[0].c->t[0].fsym[j]);
		}
		printf("\n");
	}

	/* Get ready to add more values to output */
	if ((setel = (cgats_set_elem *)malloc(
	     sizeof(cgats_set_elem) * inps[0].c->t[0].nfields)) == NULL)
		error("Malloc failed!");

	/* Process all the other input files */
	for (n = 1; n < ninps; n++) {

		/* Check all the fields match */
		if (inps[0].c->t[0].nfields != inps[n].c->t[0].nfields)
			error ("File '%s' has %d fields, file '%s has %d",
			       inps[n].name, inps[n].c->t[0].nfields, inps[0].name, inps[0].c->t[0].nfields);
		for (j = 0; j < inps[0].c->t[0].nfields; j++) {
			if (inps[0].c->t[0].ftype[j] != inps[n].c->t[0].ftype[j])
				error ("File '%s' field no. %d named '%s' doesn't match file '%s' field '%s'",
				       inps[n].name, j, inps[n].c->t[0].fsym[j], inps[0].name, inps[0].c->t[0].fsym[j]);
		}

		/* If merging, append all the values */
		if (domerge) {
			for (i = 0; i < inps[n].c->t[0].nsets; i++) {
				inps[n].c->get_setarr(inps[0].c, 0, i, setel);
				ocg->add_setarr(ocg, 0, setel);
			}

		} else {	/* Averaging */
			/* Check the number of values matches */
			if (inps[0].c->t[0].nsets != inps[n].c->t[0].nsets)
				error ("File '%s' has %d sets, file '%s has %d",
				       inps[n].name, inps[n].c->t[0].nsets, inps[0].name, inps[0].c->t[0].nsets);
	
			/* Add the numeric field values to corresponding output */
			for (i = 0; i < inps[n].c->t[0].nsets; i++) {

				/* Check that the device values match */
				for (j = 0; j < nchan; j++) {
					double diff;
					diff = *((double *)inps[0].c->t[0].fdata[i][chix[j]])
					     - *((double *)inps[n].c->t[0].fdata[i][chix[j]]);

					if (diff > 0.001)
						error ("File '%s' set %d has field '%s' value that differs from '%s'",
				       inps[n].name, i+1, inps[n].c->t[0].fsym[j], inps[0].name);
				}

				/* Add all the non-device real field values */
				for (j = 0; j < inps[0].c->t[0].nfields; j++) {
					int jj;

					/* Only real types */
					if (inps[0].c->t[0].ftype[j] != r_t)
						continue;

					/* Not device channels */
					for (jj = 0; jj < nchan; jj++) {
						if (chix[jj] == j)
							break;
					}
					if (jj < nchan)
						continue;

					*((double *)ocg->t[0].fdata[i][j])
					+= *((double *)inps[n].c->t[0].fdata[i][j]);
				}
			}
		}
	}

	/* If averaging, divide out the number of files */
	if (!domerge) {

		for (i = 0; i < inps[n].c->t[0].nsets; i++) {

			for (j = 0; j < inps[0].c->t[0].nfields; j++) {
				int jj;

				/* Only real types */
				if (inps[0].c->t[0].ftype[j] != r_t)
					continue;

				/* Not device channels */
				for (jj = 0; jj < nchan; jj++) {
					if (chix[jj] == j)
						break;
				}
				if (jj < nchan)
					continue;

				*((double *)ocg->t[0].fdata[i][j]) /= (double)ninps;
			}
		}
	}

	/* Write out the output and free the cgats * */
	for (n = 0; n <= ninps; n++) {

		if (n >= ninps) {		/* If ouput file, write it */
			if (inps[n].c->write_name(inps[n].c, inps[ninps].name))
				error("CGATS file '%s' write error : %s",inps[n].name,inps[n].c->err);
		}
		inps[n].c->del(inps[n].c);
	}

	free(setel);
	free(inps);
	
	return 0;
}





