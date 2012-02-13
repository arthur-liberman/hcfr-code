
/* 
 * Argyll Color Correction System
 * Synthetic device target chart reader
 *
 * Author: Graeme W. Gill
 * Date:   10/7/2007
 *
 * Copyright 2002 - 2007 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Based on fakeread.c
 */

/*
	Implements a synthetic RGB device response based on sRGB like
	primaries.

 */

/*
 * TTBD: 
 *
 *	Add non-linear mixing model
 *
 */


#undef DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include "copyright.h"
#include "aconfig.h"
#include "numlib.h"
#include "cgats.h"
#include "xicc.h"
#include "icc.h"

void
usage(char *mes) {
	fprintf(stderr,"Synthetic device model test chart reader - Version %s\n",
	               ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	if (mes != NULL)
		fprintf(stderr,"Error '%s'\n",mes);
	fprintf(stderr,"usage: synthread [-v] [-s] [separation.icm] profile.[icc|mpp|ti3] outfile\n");
	fprintf(stderr," -v                Verbose mode\n");
	fprintf(stderr," -p                Use separation profile\n");
	fprintf(stderr," -l                Construct and output in Lab rather than XYZ\n");
	fprintf(stderr," -i p1,p2,p3,      Set input channel curve powers (default 1.0)\n");
	fprintf(stderr," -k x1:y1,x2:y2,x3:y2  Set input channel inflection points (default 0.5,0.5)\n");
	fprintf(stderr," -o p1,p2,p3,      Set output channel curve powers (default 1.0)\n");
	fprintf(stderr," -r level          Add average random deviation of <level>%% to input device values (after sep.)\n");
	fprintf(stderr," -R level          Add average random deviation of <level>%% to output PCS values\n");
	fprintf(stderr," -u                Make random deviations have uniform distributions rather than normal\n");
	fprintf(stderr," -b L,a,b          Scale black point to target Lab value\n");
	fprintf(stderr," [separation.icm]  Device link separation profile\n");
	fprintf(stderr," profile.[icc|mpp|ti3] ICC, MPP profile or TI3 to use\n");
	fprintf(stderr," outfile           Base name for input[ti1]/output[ti3] file\n");
	exit(1);
	}


typedef struct {
	int dolab;			/* Combine and output in Lab space */
	double ipow[3];		/* Input power applied */
	double ibpp[3];		/* Input breakpoint location, -ve if none */
	double ibpv[3];		/* Input breakpoint value, -ve if none */
	double col[3][3];	/* sRGB additive colorant values in XYZ :- [out][in]  */
	double wnf[3];		/* White normalization factor */
	double opow[3];		/* Output power */
	double omax[3];		/* Output maximum that power operates into */
} synthmodel;

/* Symetrical power function */
double spow(double val, double pp) {
	if (val < 0.0)
		return -pow(-val, pp);
	else
		return pow(val, pp);
}

/* Execute the device model */
static void domodel(synthmodel *p, double *out, double *in) {
	double tmp[3];
	int i, j;

	/* Input power */
	for (j = 0; j < 3; j++)
		tmp[j] = pow(in[j], p->ipow[j]);

	/* Input breakpoint */
	for (j = 0; j < 3; j++) {
		if (p->ibpp[j] >= 0.0 && p->ibpv[j] >= 0.0) {
			double b;
			if (tmp[j] <= p->ibpp[j]) {
				b = (tmp[j] - 0.0)/(p->ibpp[j] - 0.0);
				tmp[j] = b * p->ibpv[j] + (1.0 - b) * 0.0;
			} else {
				b = (tmp[j] - p->ibpp[j])/(1.0 - p->ibpp[j]);
				tmp[j] = b * 1.0 + (1.0 - b) * p->ibpv[j];
			}
		}
	}
	
	/* Lookup primary values, sum them, and then */
	/* apply output power */
	/* (We're not allowing for non-linear mixing yet) */
	for (j = 0; j < 3; j++) {
		out[j] = 0.0;
		for (i = 0; i < 3; i++)
			out[j] += p->col[j][i] * tmp[i];
		out[j] = spow(out[j]/p->omax[j], p->opow[j]) * p->omax[j];
	}
}

int main(int argc, char *argv[])
{
	int i, j, rv = 0;
	int fa,nfa;							/* current argument we're looking at */
	int verb = 0;		/* Verbose flag */
	int dosep = 0;		/* Use separation before profile */
	int gfudge = 0;		/* Do grey fudge, 1 = W->RGB, 2 = K->xxxK */
	double rdlevel = 0.0;	/* Random device average deviation level (0.0 - 1.0) */
	double rplevel = 0.0;	/* Random PCS average deviatio level (0.0 - 1.0) */
	int unidist = 0;		/* Use uniform distribution of errors */
	double tbp[3] = { -1.0, 0.0, 0.0 };	/* Target black point */
	static char sepname[500] = { 0 };	/* ICC separation profile */
	static char inname[500] = { 0 };	/* Input cgats file base name */
	static char outname[500] = { 0 };	/* Output cgats file base name */
	cgats *icg;			/* input cgats structure */
	cgats *ocg;			/* output cgats structure */
	int nmask = 0;		/* Test chart device colorant mask */
	int nchan = 0;		/* Test chart number of device chanels */
	int npat;			/* Number of patches */
	int si;				/* Sample id index */
	int ti;				/* Temp index */
	int fi;				/* Colorspace index */

	synthmodel md;		/* Synthetic model */

	/* ICC separation device link profile  */
	icmFile *sep_fp = NULL;		/* Color profile file */
	icc *sep_icco = NULL;		/* Profile object */
	icmLuBase *sep_luo = NULL;	/* Conversion object */
	icColorSpaceSignature sep_ins, sep_outs;	/* Type of input and output spaces */
	int sep_inn;				/* Number of input channels to separation */
	inkmask sep_nmask = 0;		/* Colorant mask for separation input */
	double wp[3], bp[3];		/* ICC profile Lab white and black points */
	double bpt[3][3];			/* Black point transform matrix (Lab->Lab) */


	int inn, outn;		/* Number of channels for conversion input, output */
	icColorSpaceSignature ins, outs;	/* Type of conversion input and output spaces */
	int cnv_nmask = 0;	/* Conversion input nmask */ 
	time_t clk = time(0);
	struct tm *tsp = localtime(&clk);
	char *atm = asctime(tsp); /* Ascii time */
	char *xyzfname[3] = { "XYZ_X", "XYZ_Y", "XYZ_Z" };
	char *labfname[3] = { "LAB_L", "LAB_A", "LAB_B" };

	error_program = "Synthread";
	if (argc < 1)
		usage("Too few arguments");

	/* Initialise the default model */
	inn = 3;
	ins = icSigRgbData;
	outn = 3;
	outs = icSigXYZData;

	md.dolab = 0;

	md.ipow[0] = 1.0;
	md.ipow[1] = 1.0;
	md.ipow[2] = 1.0;

	md.ibpp[0] = -1.0;
	md.ibpv[0] = -1.0;
	md.ibpp[1] = -1.0;
	md.ibpv[1] = -1.0;
	md.ibpp[2] = -1.0;
	md.ibpv[2] = -1.0;

	md.col[0][0] = 0.412424;	/* X from R */
	md.col[0][1] = 0.357579;	/* X from G */
	md.col[0][2] = 0.180464;	/* X from B */
	md.col[1][0] = 0.212656;	/* Y from R */
	md.col[1][1] = 0.715158;	/* Y from G */
	md.col[1][2] = 0.0721856;	/* Y from B */
	md.col[2][0] = 0.0193324;	/* Z from R */
	md.col[2][1] = 0.119193;	/* Z from G */
	md.col[2][2] = 0.950444;	/* Z from B */

	md.opow[0] = 1.0;
	md.opow[1] = 1.0;
	md.opow[2] = 1.0;

	md.omax[0] = 1.0;
	md.omax[1] = 1.0;
	md.omax[2] = 1.0;

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-') {	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else
				{
				if ((fa+1) < argc)
					{
					if (argv[fa+1][0] != '-')
						{
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
						}
					}
				}

			if (argv[fa][1] == '?')
				usage("Usage requested");

			/* Verbose */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V')
				verb = 1;

			/* Separation */
			else if (argv[fa][1] == 'p' || argv[fa][1] == 'P')
				dosep = 1;

			/* Lab */
			else if (argv[fa][1] == 'l' || argv[fa][1] == 'L')
				md.dolab = 1;

			/* Input curve power */
			else if (argv[fa][1] == 'i' || argv[fa][1] == 'I') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to -i");
				if (sscanf(na, " %lf,%lf,%lf ", &md.ipow[0], &md.ipow[1], &md.ipow[2]) != 3)
					usage("Argument to -i does not parse");
			}

			/* Input curve inflexction point */
			else if (argv[fa][1] == 'k' || argv[fa][1] == 'K') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to -k");
				if (sscanf(na, " %lf:%lf,%lf:%lf,%lf:%lf ",
				    &md.ibpp[0], &md.ibpv[0], &md.ibpp[1],
				    &md.ibpv[1], &md.ibpp[2], &md.ibpv[2]) != 6)
					usage("Argument to -k does not parse");
			}

			/* Output curve power */
			else if (argv[fa][1] == 'o' || argv[fa][1] == 'O') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to -o");
				if (sscanf(na, " %lf,%lf,%lf ", &md.opow[0], &md.opow[1], &md.opow[2]) != 3)
					usage("Argument to -o does not parse");
			}

			/* Uniform distrivuted errors */
			else if (argv[fa][1] == 'u' || argv[fa][1] == 'U')
				unidist = 1;

			/* Random addition to device levels */
			else if (argv[fa][1] == 'r') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to -r");
				rdlevel = 0.01 * atof(na);
				rand32(time(NULL));		/* Init seed randomly */
			}

			/* Random addition to PCS levels */
			else if (argv[fa][1] == 'R') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to -R");
				rplevel = 0.01 * atof(na);
				rand32(time(NULL));		/* Init seed randomly */
			}

			/* Black point scale */
			else if (argv[fa][1] == 'b' || argv[fa][1] == 'B') {
				if (na == NULL) usage("Expect argument to -b");
				fa = nfa;
				if (sscanf(na, " %lf , %lf , %lf ",&tbp[0], &tbp[1], &tbp[2]) != 3)
					usage("Couldn't parse argument to -b");
				if (tbp[0] < 0.0 || tbp[0] > 100.0) usage("-b L* value out of range");
				if (tbp[1] < -128.0 || tbp[1] > 128.0) usage("-b a* value out of range");
				if (tbp[2] < -128.0 || tbp[2] > 128.0) usage("-b b* value out of range");
			}

			else
				usage("Unrecognised flag");
		}
		else
			break;
	}

	/* Get the file name argument */
	if (dosep) {
		if (fa >= argc || argv[fa][0] == '-') usage("Missing separation profile filename argument");
		strcpy(sepname,argv[fa++]);
	}

	if (fa >= argc || argv[fa][0] == '-') usage("Missing basename argument");
	strcpy(inname,argv[fa]);
	strcat(inname,".ti1");
	strcpy(outname,argv[fa]);
	strcat(outname,".ti3");

	/* Convert colorants to Lab, and scale white point */
	if (md.dolab) {
		double white[3] = { 100.0, 0, 0 };
		double rot[3][3];

printf("~1 switching to Lab\n");

		for (i = 0; i < 3; i++) {
			double val[3];

			val[0] = md.col[0][i];
			val[1] = md.col[1][i];
			val[2] = md.col[2][i];
printf("~1 prim XYZ %f %f %f -> ", val[0], val[1], val[2]);
			icmXYZ2Lab(&icmD50, val, val);
printf("Lab %f %f %f\n", val[0], val[1], val[2]);
			md.col[0][i] = val[0];
			md.col[1][i] = val[1];
			md.col[2][i] = val[2];
		}

		/* Compute white sum */
		for (i = 0; i < 3; i++) {
			md.omax[i] = 0.0;
			for (j = 0; j < 3; j++)
				md.omax[i] += md.col[i][j];
		}
printf("~1 sum = %f %f %f\n", md.omax[0], md.omax[1], md.omax[2]);

		/* Compute rotate and scale to map to white target */
		icmRotMat(rot, md.omax, white);

		/* Rotate and primaries to sum to white */
		for (i = 0; i < 3; i++) {
			double val[3];

			val[0] = md.col[0][i];
			val[1] = md.col[1][i];
			val[2] = md.col[2][i];
			icmMulBy3x3(val, rot, val);
printf("~1 Scaled primary %f %f %f\n", val[0], val[1], val[2]);
			md.col[0][i] = val[0];
			md.col[1][i] = val[1];
			md.col[2][i] = val[2];
		}

		/* Compute output maximum factors to set out power range */
		for (i = 0; i < 3; i++) {
			md.omax[i] = 0.0;
			for (j = 0; j < 3; j++) {
				if (i == 0)
					md.omax[i] += md.col[i][j];
				else {
					if (fabs(md.col[i][j]) > md.omax[i])
						md.omax[i] = fabs(md.col[i][j]);
					
				}
			}
		}
	} else {

		/* Compute output maximum factors to set out power range */
		for (i = 0; i < 3; i++) {
			md.omax[i] = 0.0;
			for (j = 0; j < 3; j++)
				md.omax[i] += md.col[i][j];
		}
	}
printf("~1 omax = %f %f %f\n", md.omax[0], md.omax[1], md.omax[2]);

	/* Deal with separation */
	if (dosep) {
		if ((sep_fp = new_icmFileStd_name(sepname,"r")) == NULL)
			error ("Can't open file '%s'",sepname);
	
		if ((sep_icco = new_icc()) == NULL)
			error ("Creation of ICC object failed");
	
		/* Deal with ICC separation */
		if ((rv = sep_icco->read(sep_icco,sep_fp,0)) == 0) {
	
			/* Get a conversion object */
			if ((sep_luo = sep_icco->get_luobj(sep_icco, icmFwd, icmDefaultIntent, icmSigDefaultData, icmLuOrdNorm)) == NULL) {
					error ("%d, %s",sep_icco->errc, sep_icco->err);
			}
	
			/* Get details of conversion */
			sep_luo->spaces(sep_luo, &sep_ins, &sep_inn, &sep_outs, NULL, NULL, NULL, NULL, NULL, NULL);
			sep_nmask = icx_icc_to_colorant_comb(sep_ins, sep_icco->header->deviceClass);
		}
	}

	/* Deal with input CGATS files */
	icg = new_cgats();			/* Create a CGATS structure */
	icg->add_other(icg, "CTI1"); 	/* our special input type is Calibration Target Information 1 */

	if (icg->read_name(icg, inname))
		error("CGATS file read error : %s",icg->err);

	if (icg->ntables == 0 || icg->t[0].tt != tt_other || icg->t[0].oi != 0)
		error ("Input file isn't a CTI1 format file");
	if (icg->ntables != 1 && icg->ntables != 2 && icg->ntables != 3)
		error ("Input file doesn't contain one, two or three tables");

	if ((npat = icg->t[0].nsets) <= 0)
		error ("No sets of data");

	/* Setup output cgats file */
	ocg = new_cgats();				/* Create a CGATS structure */
	ocg->add_other(ocg, "CTI3"); 		/* our special type is Calibration Target Information 3 */
	ocg->add_table(ocg, tt_other, 0);	/* Start the first table */

	ocg->add_kword(ocg, 0, "DESCRIPTOR", "Argyll Calibration Target chart information 3",NULL);
	ocg->add_kword(ocg, 0, "ORIGINATOR", "Argyll synthread", NULL);
	atm[strlen(atm)-1] = '\000';	/* Remove \n from end */
	ocg->add_kword(ocg, 0, "CREATED",atm, NULL);

	/* Assume a general output type device */
	ocg->add_kword(ocg, 0, "DEVICE_CLASS","OUTPUT", NULL);

	if ((ti = icg->find_kword(icg, 0, "SINGLE_DIM_STEPS")) >= 0)
		ocg->add_kword(ocg, 0, "SINGLE_DIM_STEPS",icg->t[0].kdata[ti], NULL);

	if ((ti = icg->find_kword(icg, 0, "COMP_GREY_STEPS")) >= 0)
		ocg->add_kword(ocg, 0, "COMP_GREY_STEPS",icg->t[0].kdata[ti], NULL);

	if ((ti = icg->find_kword(icg, 0, "MULTI_DIM_STEPS")) >= 0)
		ocg->add_kword(ocg, 0, "MULTI_DIM_STEPS",icg->t[0].kdata[ti], NULL);

	if ((ti = icg->find_kword(icg, 0, "FULL_SPREAD_PATCHES")) >= 0)
		ocg->add_kword(ocg, 0, "FULL_SPREAD_PATCHES",icg->t[0].kdata[ti], NULL);
	
	/* Fields we want */
	ocg->add_field(ocg, 0, "SAMPLE_ID", nqcs_t);

	if ((si = icg->find_field(icg, 0, "SAMPLE_ID")) < 0)
		error ("Input file doesn't contain field SAMPLE_ID");

	/* Figure out the color space */
	if ((fi = icg->find_kword(icg, 0, "COLOR_REP")) < 0)
		error ("Input file doesn't contain keyword COLOR_REP");

	if ((nmask = icx_char2inkmask(icg->t[0].kdata[fi])) == 0)
		error ("Input file keyword COLOR_REP has unknown value");

	{
		int i, j, ii;
		int chix[ICX_MXINKS];	/* Device chanel indexes */
		char *ident, *bident;
		int nsetel = 0;
		cgats_set_elem *setel;	/* Array of set value elements */

		nchan = icx_noofinks(nmask);
		ident = icx_inkmask2char(nmask, 1); 
		bident = icx_inkmask2char(nmask, 0); 

		/* Sanity check what we're going to do */
		if (dosep) {

			/* Check if sep ICC input is compatible with .ti1 */
			if (nmask == ICX_W && sep_ins == icSigRgbData)
				gfudge = 1;
			else if (nmask == ICX_K && sep_ins == icSigCmykData)
				gfudge = 2;
			else if (icx_colorant_comb_match_icc(nmask, sep_ins) == 0) {
				error("Separation ICC device space '%s' dosen't match TI1 '%s'",
				       icm2str(icmColorSpaceSignature, sep_ins),
				       ident);	/* Should free(). */
			}

			/* Check if separation ICC output is compatible with ICC/MPP/TI3 conversion */ 
			if (sep_outs != ins)
				error("Synthetic device space '%s' dosen't match Separation ICC '%s'",
				       icm2str(icmColorSpaceSignature, ins),
				       icm2str(icmColorSpaceSignature, sep_outs));
		} else {
			/* Check if synthetic device is compatible with .ti1 */
			if (nmask == ICX_W && ins == icSigRgbData)
				gfudge = 1;
			else if (nmask == ICX_K && ins == icSigCmykData)
				gfudge = 2;		/* Should allow for other colorant combo's that include black */
			else if (icx_colorant_comb_match_icc(nmask, ins) == 0) {
				error("Synthetic device space '%s' dosen't match TI1 '%s'",
				       icm2str(icmColorSpaceSignature, ins),
				       ident);	// Should free().
			}
		}

		if ((ii = icg->find_kword(icg, 0, "TOTAL_INK_LIMIT")) >= 0)
			ocg->add_kword(ocg, 0, "TOTAL_INK_LIMIT",icg->t[0].kdata[ii], NULL);

		nsetel += 1;		/* For id */
		nsetel += nchan;	/* For device values */
		nsetel += 3;		/* For XYZ/Lab */

		for (j = 0; j < nchan; j++) {
			int imask;
			char fname[100];

			imask = icx_index2ink(nmask, j);
			sprintf(fname,"%s_%s",nmask == ICX_W || nmask == ICX_K ? "GRAY" : bident,
			                      icx_ink2char(imask));

			if ((ii = icg->find_field(icg, 0, fname)) < 0)
				error ("Input file doesn't contain field %s",fname);
			if (icg->t[0].ftype[ii] != r_t)
				error ("Field %s is wrong type",fname);
	
			ocg->add_field(ocg, 0, fname, r_t);
			chix[j] = ii;
		}

		/* Add PCS fields */
		for (j = 0; j < 3; j++) {
			ocg->add_field(ocg, 0, md.dolab ? labfname[j] : xyzfname[j], r_t);
		}

		{
			char fname[100];
			sprintf(fname, md.dolab ? "%s_LAB" : "%s_XYZ", ident);
			ocg->add_kword(ocg, 0, "COLOR_REP", fname, NULL);
		}

		if ((setel = (cgats_set_elem *)malloc(sizeof(cgats_set_elem) * nsetel)) == NULL)
			error("Malloc failed!");

		/* Read all the test patches in, convert them, */
		/* and write them out. */
		for (i = 0; i < npat; i++) {
			int k = 0;
			char *id;
			double odev[ICX_MXINKS], dev[ICX_MXINKS], sep[ICX_MXINKS], PCS[3];
			xspect out;

			id = ((char *)icg->t[0].fdata[i][si]);
			for (j = 0; j < nchan; j++) {
				double dv = *((double *)icg->t[0].fdata[i][chix[j]]) / 100.0;
				odev[j] = dev[j] = sep[j] = dv;
			}

			if (gfudge) {
				int nch;

				if (dosep)		/* Figure number of channels into conversion */
					nch = sep_inn;
				else
					nch = inn;

				if (gfudge == 1) { 	/* Convert W -> RGB */
					double wval = dev[0];
					for (j = 0; j < nch; j++)
						dev[j] = sep[j] = wval; 

				} else {			/* Convert K->xxxK */
					int kch;
					int inmask;
					double kval = dev[0];

					if (dosep)		/* Figure number of channels into conversion */
						inmask = sep_nmask;
					else
						inmask = cnv_nmask;

					if (inmask == 0)
						error("Input colorspace ambiguous - can't determine if it has black");

					if ((kch = icx_ink2index(inmask, ICX_BLACK)) == -1)
						error("Can't find black colorant for K fudge");
					for (j = 0; j < nch; j++) {
						if (j == kch)
							dev[j] = sep[j] = kval; 
						else
							dev[j] = sep[j] = 0.0; 
					}
				}
			}

			if (dosep)
				if (sep_luo->lookup(sep_luo, sep, dev) > 1)
					error ("%d, %s",sep_icco->errc,sep_icco->err);

			/* Add randomness and non-linearity (rdlevel is avg. dev.) */
			/* Note dev/sep is 0-1.0 at this stage */
			for (j = 0; j < inn; j++) {
				double dv = sep[j];
				if (rdlevel > 0.0) {
					double rr;
					if (unidist)
						rr = d_rand(-2.0 * rdlevel, 2.0 * rdlevel);
					else
						rr = 1.2533 * rdlevel * norm_rand();
					dv += rr;
					if (dv < 0.0)
						dv = 0.0;
					else if (dv > 1.0)
						dv = 1.0;
				}
				sep[j] = dv;
			}

			/* Do color conversion */
			domodel(&md, PCS, sep);

			if (tbp[0] >= 0) {	/* Doing black point scaling */

				for (j = 0; j < 3; j++)
					PCS[j] -= wp[j];
				icmMulBy3x3(PCS, bpt, PCS);
				for (j = 0; j < 3; j++)
					PCS[j] += wp[j];
			}

			setel[k++].c = id;
			
			for (j = 0; j < nchan; j++)
				setel[k++].d = 100.0 * odev[j];

			if (md.dolab == 0) {
				PCS[0] *= 100.0;
				PCS[1] *= 100.0;
				PCS[2] *= 100.0;
			}

			/* Add randomness (rplevel is avg. dev.) */
			/* Note PCS is 0..100 XYZ or Lab at this point */
			if (rplevel > 0.0) {
				for (j = 0; j < 3; j++) {
					double dv = PCS[j];
					double rr;
					if (unidist)
						rr = 100.0 * d_rand(-2.0 * rplevel, 2.0 * rplevel);
					else
						rr = 100.0 * 1.2533 * rplevel * norm_rand();
					dv += rr;

					/* Don't let L*, X, Y or Z go negative */
					if ((!md.dolab || j == 0) && dv < 0.0)
						dv = 0.0;
					PCS[j] = dv;
				}
			}

			setel[k++].d = PCS[0];
			setel[k++].d = PCS[1];
			setel[k++].d = PCS[2];

			ocg->add_setarr(ocg, 0, setel);
		}

		free(setel);
		free(ident);
		free(bident);
	}

	if (sep_luo != NULL) {
		sep_luo->del(sep_luo);
		sep_icco->del(sep_icco);
		sep_fp->del(sep_fp);
	}

	if (ocg->write_name(ocg, outname))
		error("Write error : %s",ocg->err);

	ocg->del(ocg);		/* Clean up */
	icg->del(icg);		/* Clean up */

	return 0;
}


