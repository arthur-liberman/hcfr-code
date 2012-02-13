/* 
 * Argyll Color Correction System
 * Fake print target chart reader - use ICC or MPP profile rather than instrument
 *
 * Author: Graeme W. Gill
 * Date:   17/2/2002
 *
 * Copyright 2002 - 2007 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/*
 * TTBD: 
	
	Do we need to deterct & mark display values normalized to Y = 100 ??
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

void usage(char *diag, ...) {
	fprintf(stderr,"Fake test chart reader - lookup values in ICC/MPP profile, Version %s\n",
	               ARGYLL_VERSION_STR);
	fprintf(stderr,"Author: Graeme W. Gill, licensed under the AGPL Version 3\n");
	if (diag != NULL) {
		va_list args;
		fprintf(stderr,"  Diagnostic: ");
		va_start(args, diag);
		vfprintf(stderr, diag, args);
		va_end(args);
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"usage: fakeread [-v] [-s] [separation.icm] profile.[%s|mpp|ti3] outfile\n",ICC_FILE_EXT_ND);
	fprintf(stderr," -v                Verbose mode\n");
	fprintf(stderr," -s                Lookup MPP spectral values\n");
	fprintf(stderr," -p                Use separation profile\n");
	fprintf(stderr," -l                Output Lab rather than XYZ\n");
	fprintf(stderr," -k file.cal       Apply calibration (after sep.) and include in .ti3\n");
	fprintf(stderr," -i file.cal       Include calibration in .ti3 (but don't apply it)\n");
	fprintf(stderr," -r level          Add average random deviation of <level>%% to input device values (after sep. & cal.)\n");
	fprintf(stderr," -0 pow            Apply power to input device chanel 0-9 (after sep. cal. & rand)\n");
	fprintf(stderr," -R level          Add average random deviation of <level>%% to output PCS values\n");
	fprintf(stderr," -u                Make random deviations have uniform distributions rather than normal\n");
	fprintf(stderr," -S seed           Set random seed\n");
	fprintf(stderr," -b L,a,b          Scale black point to target Lab value\n");
	fprintf(stderr," -I intent         r = relative colorimetric, a = absolute (default)\n");
	fprintf(stderr," [separation.%s]  Device link separation profile\n",ICC_FILE_EXT_ND);
	fprintf(stderr," profile.[%s|mpp|ti3] ICC, MPP profile or TI3 to use\n",ICC_FILE_EXT_ND);
	fprintf(stderr," outfile           Base name for input[ti1]/output[ti3] file\n");
	exit(1);
	}

int main(int argc, char *argv[])
{
	int j;
	int fa,nfa;							/* current argument we're looking at */
	int verb = 0;		/* Verbose flag */
	int dosep = 0;		/* Use separation before profile */
	int dolab = 0;		/* Output Lab rather than XYZ */
	int gfudge = 0;		/* Do grey fudge, 1 = W->RGB, 2 = K->xxxK */
	double chpow[10] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
	double rdlevel = 0.0;	/* Random device average deviation level (0.0 - 1.0) */
	double rplevel = 0.0;	/* Random PCS average deviation level (0.0 - 1.0) */
	int unidist = 0;		/* Use uniform distribution of errors */
	unsigned int seed = time(NULL);	/* Random seed value */
	double tbp[3] = { -1.0, 0.0, 0.0 };	/* Target black point */
	int applycal = 0;		/* NZ to apply calibration */
	static char sepname[MAXNAMEL+1] = { 0 };	/* ICC separation profile */
	static char calname[MAXNAMEL+1] = { 0 };	/* device calibration */
	static char profname[MAXNAMEL+1] = { 0 };	/* ICC or MPP Profile name */
	static char inname[MAXNAMEL+1] = { 0 };	/* Input cgats file base name */
	static char outname[MAXNAMEL+1] = { 0 };	/* Output cgats file base name */
	cgats *icg;			/* input cgats structure */
	cgats *ocg;			/* output cgats structure */
	int nmask = 0;		/* Test chart device colorant mask */
	int nchan = 0;		/* Test chart number of device chanels */
	int npat;			/* Number of patches */
	int si;				/* Sample id index */
	int ti;				/* Temp index */
	int fi;				/* Colorspace index */
	int inti3 = 0;		/* Input is a renamed .ti3 file rather than .ti1 */

	/* ICC separation device link profile  */
	icmFile *sep_fp = NULL;		/* Color profile file */
	icc *sep_icco = NULL;		/* Profile object */
	icmLuBase *sep_luo = NULL;	/* Conversion object */
	icColorSpaceSignature sep_ins, sep_outs;	/* Type of input and output spaces */
	int sep_inn;				/* Number of input channels to separation */
	inkmask sep_nmask = 0;		/* Colorant mask for separation input */
	double wp[3], bp[3];		/* ICC profile Lab white and black points */
	double bpt[3][3];			/* Black point transform matrix (Lab->Lab) */

	/* Calibration */
	xcal *cal = NULL;			/* calibration */ 

	/* ICC profile based */
	icRenderingIntent intent = icAbsoluteColorimetric;
	icmFile *icc_fp = NULL;	/* Color profile file */
	icc *icc_icco = NULL;	/* Profile object */
	icmLuBase *icc_luo = NULL;	/* Conversion object */

	/* MPP profile based */
	mpp *mlu = NULL;	/* Conversion object */
	instType itype = instUnknown;	/* Type of instrument used */
	int dospec = 0;		/* Do spectral MPP lookup */
	int spec_n = 0;		/* Number of spectral bands */
	double spec_wl_short;/* First reading wavelength in nm (shortest) */
	double spec_wl_long; /* Last reading wavelength in nm (longest) */

	/* TI3 based fake read */
	cgats *ti3 = NULL;	/* input cgats structure */
	int ti3_npat = 0;	/* Number of patches in reference file */
	int ti3_chix[ICX_MXINKS];	/* Device chanel indexes */
	int ti3_pcsix[3];	/* Device chanel indexes */
	int ti3_spi[XSPECT_MAX_BANDS];	/* CGATS indexes for each wavelength */
	int ti3_isLab = 0;	/* Flag indicating PCS for TI3 file */

	int rv = 0;
	int inn, outn;		/* Number of channels for conversion input, output */
	icColorSpaceSignature ins, outs;	/* Type of conversion input and output spaces */
	inkmask cnv_nmask = 0;	/* Conversion input nmask */ 
	time_t clk = time(0);
	struct tm *tsp = localtime(&clk);
	char *atm = asctime(tsp); /* Ascii time */
	char *xyzfname[3] = { "XYZ_X", "XYZ_Y", "XYZ_Z" };
	char *labfname[3] = { "LAB_L", "LAB_A", "LAB_B" };

	error_program = "Fakeread";
	if (argc < 3)
		usage("Too few arguments");

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

			/* Spectral MPP lookup */
			else if (argv[fa][1] == 's')
				dospec = 1;

			/* Separation */
			else if (argv[fa][1] == 'p' || argv[fa][1] == 'P')
				dosep = 1;

			/* Lab */
			else if (argv[fa][1] == 'l' || argv[fa][1] == 'L')
				dolab = 1;

			/* Uniform distrivuted errors */
			else if (argv[fa][1] == 'u' || argv[fa][1] == 'U')
				unidist = 1;

			/* Random seed value */
			else if (argv[fa][1] == 'S') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to -S");
				if (sscanf(na, " %lu ",&seed) != 1)
					usage("Couldn't parse argument to -S");
			}

			/* calibration to device values */
			else if (argv[fa][1] == 'k' || argv[fa][1] == 'i') {
				if (argv[fa][1] == 'k')
					applycal = 1;
				else
					applycal = 0;
				fa = nfa;
				if (na == NULL) usage("Expected an argument to -%c",argv[fa][1]);
				strncpy(calname,na,MAXNAMEL); calname[MAXNAMEL] = '\000';
			}

			/* Random addition to device levels */
			else if (argv[fa][1] == 'r') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to -r");
				rdlevel = atof(na) * 0.01;
			}

			/* Power applied to device channels */
			else if (argv[fa][1] >= '0' && argv[fa][1] <= '9') {
				int ch;
				if (na == NULL) usage("Expect argument to -[0-9]");
				ch = argv[fa][1]-'0';
				if (ch < 0 || ch > 9)
					usage("Channel no. %d is out of range\n",ch);
				chpow[ch] = atof(na);
				fa = nfa;
			}

			/* Random addition to PCS levels */
			else if (argv[fa][1] == 'R') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to -R");
				rplevel = atof(na) * 0.01;
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

			/* Intent (only applies to ICC profile) */
			else if (argv[fa][1] == 'I') {
				fa = nfa;
				if (na == NULL) usage("Expect argument to -I");
    			switch (na[0]) {
					case 'r':
						intent = icRelativeColorimetric;
						break;
					case 'a':
						intent = icAbsoluteColorimetric;
						break;
					default:
						usage("Unexpected argument value '%c' to -I optyion",na[0]);
				}
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
		strncpy(sepname,argv[fa++],MAXNAMEL); sepname[MAXNAMEL] = '\000';
	}

	if (fa >= argc || argv[fa][0] == '-') usage("Missing profile filename argument");
	strncpy(profname,argv[fa++],MAXNAMEL); profname[MAXNAMEL] = '\000';

	if (fa >= argc || argv[fa][0] == '-') usage("Missing basename argument");
	strncpy(inname,argv[fa],MAXNAMEL-4); inname[MAXNAMEL-4] = '\000';
	strcat(inname,".ti1");
	strncpy(outname,argv[fa],MAXNAMEL-4); outname[MAXNAMEL-4] = '\000';
	strcat(outname,".ti3");

	rand32(seed);		/* Init seed */
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

	/* Deal with calibration */
	if (calname[0] != '\000') {
		if ((cal = new_xcal()) == NULL)
			error("new_xcal failed");
		if ((cal->read(cal, calname)) != 0)
			error("%s",cal->err);
		if (verb)
			if (applycal)
				printf("Applying calibration curves from '%s'\n",calname);
			else
				printf("Embedding calibration curves from '%s' in output\n",calname);
	}

	/* Deal with ICC profile */
	if ((icc_fp = new_icmFileStd_name(profname,"r")) == NULL)
		error ("Can't open file '%s'",profname);

	if ((icc_icco = new_icc()) == NULL)
		error ("Creation of ICC object failed");

	/* Deal with ICC profile */
	if ((rv = icc_icco->read(icc_icco,icc_fp,0)) == 0) {

		/* Embed any calibration in the output if it's present */
		if (cal == NULL) {
			applycal = 0;
			cal = xiccReadCalTag(icc_icco);

			if (verb && cal != NULL)
				printf("Embedding calibration curves from ICC profile in output\n");
		}

		/* Get a Device to PCS conversion object */
		if ((icc_luo = icc_icco->get_luobj(icc_icco, icmFwd, intent,
		                           dolab ? icSigLabData : icSigXYZData, icmLuOrdNorm)) == NULL) {
			if ((icc_luo = icc_icco->get_luobj(icc_icco, icmFwd, icmDefaultIntent,
			                       dolab ? icSigLabData : icSigXYZData, icmLuOrdNorm)) == NULL)
				error ("%d, %s",icc_icco->errc, icc_icco->err);
		}

		/* Get details of conversion */
		icc_luo->spaces(icc_luo, &ins, &inn, &outs, &outn, NULL, NULL, NULL, NULL, NULL);
		cnv_nmask = icx_icc_to_colorant_comb(ins, icc_icco->header->deviceClass);

		if (dospec)
			error("Can't lookup spectral values for ICC profile");

		if (tbp[0] >= 0.0) {
			double ss[3], tt[3];
	
			icc_luo->wh_bk_points(icc_luo, wp, bp);
			if (dolab) {
				icmXYZ2Lab(&icmD50, wp, wp);
				icmXYZ2Lab(&icmD50, bp, bp);
			} else {
				icmLab2XYZ(&icmD50, tbp, tbp);
			}

			/* Create scaling matrix */
			for (j = 0; j < 3; j++) {
				tt[j] = tbp[j] - wp[j];
				ss[j] = bp[j] - wp[j];
			}
			icmRotMat(bpt, ss, tt);

			if (verb) {
				printf("White point = %f %f %f (%s)\n",wp[0],wp[1],wp[2], dolab ? "Lab" : "XYZ");
				printf("Black point = %f %f %f (%s)\n",bp[0],bp[1],bp[2], dolab ? "Lab" : "XYZ");
				printf("Target Black point = %f %f %f (%s)\n",tbp[0],tbp[1],tbp[2], dolab ? "Lab" : "XYZ");
			}
		}

	} else {	/* Not a valid ICC */
		/* Close out the ICC profile */
		icc_icco->del(icc_icco);
		icc_icco = NULL;
		icc_fp->del(icc_fp);
		icc_fp = NULL;
		icc_luo = NULL;
	}

	/* If we don't have an ICC lookup object, look for an MPP */
	if (icc_luo == NULL) {

		if ((mlu = new_mpp()) == NULL)
			error ("Creation of MPP object failed");

		if ((rv = mlu->read_mpp(mlu, profname)) == 0) {

			/* mlu defaults to absolute XYZ lookup */
			if (dolab) {
				mlu->set_ilob(mlu, icxIT_default, NULL, icxOT_default, NULL, icSigLabData, 0);
			}

			mlu->get_info(mlu, &cnv_nmask, &inn, NULL,
			                   &spec_n, &spec_wl_short, &spec_wl_long, &itype, NULL);
	
			outn = 3;
			outs = dolab ? icSigLabData : icSigXYZData;
	
			if ((ins = icx_colorant_comb_to_icc(cnv_nmask)) == 0)
				error ("Couldn't match MPP mask to valid ICC colorspace");
	
			if (dospec && spec_n <= 0)
				error("Can't lookup spectral values for non-spectral MPP profile");
		} else {
			mlu->del(mlu);
			mlu = NULL;
		}
	}

	/* If we don't have an ICC or MPP lookup object, look for a TI3 */
	if (icc_luo == NULL && mlu == NULL) {
		char *rbuf, *outc;
		char *ti3_bident;
		int ti3_nchan;

		ti3 = new_cgats();			/* Create a CGATS structure */
		ti3->add_other(ti3, "CTI3");/* our special input type is Calibration Target Information 3 */
	
		if (ti3->read_name(ti3, profname))
			error("CGATS file read error : %s",ti3->err);
	
		if (ti3->ntables == 0 || ti3->t[0].tt != tt_other || ti3->t[0].oi != 0)
			error ("Profile file '%s' isn't a CTI3 format file",profname);
		if (ti3->ntables != 1)
			error ("Input file '%s' doesn't contain one table",profname);

		if ((ti = ti3->find_kword(ti3, 0, "COLOR_REP")) < 0)
			error("Input file doesn't contain keyword COLOR_REP");
	
		if ((rbuf = strdup(ti3->t[0].kdata[ti])) == NULL)
			error("Malloc failed");
	
		/* Split COLOR_REP into device and PCS space */
		if ((outc = strchr(rbuf, '_')) == NULL)
			error("COLOR_REP '%s' invalid", ti3->t[0].kdata[ti]);
		*outc++ = '\000';
	
		outn = 3;
		if (strcmp(outc, "XYZ") == 0) {
			ti3_isLab = 0;
			outs = icSigXYZData;
		} else if (strcmp(outc, "LAB") == 0) {
			ti3_isLab = 1;
			outs = icSigLabData;
		} else
			error("COLOR_REP '%s' invalid (Neither XYZ nor LAB)", ti3->t[0].kdata[ti]);

		if ((cnv_nmask = icx_char2inkmask(rbuf)) == 0) {
			error ("File '%s' keyword COLOR_REP has unknown device value '%s'",profname,rbuf);
		}

		free(rbuf);

		if ((ins = icx_colorant_comb_to_icc(cnv_nmask)) == 0)
			error ("Couldn't match MPP mask to valid ICC colorspace");

		if ((inn = icmCSSig2nchan(ins)) == 0)
			error ("TI3 Colorspace with unknown number of channels");

		if ((ti3_npat = ti3->t[0].nsets) <= 0)
			error ("No sets of data in reference TI3 file");

		ti3_nchan = icx_noofinks(cnv_nmask);
		ti3_bident = icx_inkmask2char(cnv_nmask, 0); 

		/* Find device fields */
		for (j = 0; j < ti3_nchan; j++) {
			int ii, imask;
			char fname[100];

			imask = icx_index2ink(cnv_nmask, j);
			sprintf(fname,"%s_%s",cnv_nmask == ICX_W || cnv_nmask == ICX_K ? "GRAY" : ti3_bident,
			                      icx_ink2char(imask));

			if ((ii = ti3->find_field(ti3, 0, fname)) < 0)
				error ("Input file doesn't contain field %s",fname);
			if (ti3->t[0].ftype[ii] != r_t)
				error ("Field %s is wrong type",fname);
			ti3_chix[j] = ii;
		}

		/* Find PCS fields */
		for (j = 0; j < 3; j++) {
			int ii;

			if ((ii = ti3->find_field(ti3, 0, ti3_isLab ? labfname[j] : xyzfname[j])) < 0)
				error ("Input file doesn't contain field %s",ti3_isLab ? labfname[j] : xyzfname[j]);
			if (ti3->t[0].ftype[ii] != r_t)
				error ("Field %s is wrong type",ti3_isLab ? labfname[j] : xyzfname[j]);
			ti3_pcsix[j] = ii;
		}

		/* Find spectral fields */
		if ((ti = ti3->find_kword(ti3, 0, "SPECTRAL_BANDS")) >= 0) {
			char buf[100];

			spec_n = atoi(ti3->t[0].kdata[ti]);
			if ((ti = ti3->find_kword(ti3, 0, "SPECTRAL_START_NM")) < 0)
				error ("Input file doesn't contain keyword SPECTRAL_START_NM");
			spec_wl_short = atof(ti3->t[0].kdata[ti]);
			if ((ti = ti3->find_kword(ti3, 0, "SPECTRAL_END_NM")) < 0)
				error ("Input file doesn't contain keyword SPECTRAL_END_NM");
			spec_wl_long = atof(ti3->t[0].kdata[ti]);
	
			/* Find the fields for spectral values */
			for (j = 0; j < spec_n; j++) {
				int nm;
		
				/* Compute nearest integer wavelength */
				nm = (int)(spec_wl_short + ((double)j/(spec_n-1.0))
				            * (spec_wl_long - spec_wl_short) + 0.5);
				
				sprintf(buf,"SPEC_%03d",nm);
	
				if ((ti3_spi[j] = ti3->find_field(ti3, 0, buf)) < 0)
					error("Input file doesn't contain field %s",buf);
			}
		}

		if (dospec && spec_n <= 0)
			error("Can't lookup spectral values for non-spectral TI3 file");

		free(ti3_bident);
	}

	/* Some sanity checking */
	if (tbp[0] >= 0.0 && icc_luo == NULL)
		error("Black scaling only works with ICC profile");

	if (verb) {
//		printf("Random seed is %u\n",seed);

		if (rdlevel > 0.0)
			printf("Adding %.3f%% average deviation %s noise to device values\n",rdlevel * 100.0,unidist ? "uniform" : "normal");
		for (j = 0; j < inn && j < 10; j++) {
			if (chpow[j] != 1.0)
				printf("Applying chan %d power %f\n",j,chpow[j]);
		}
		if (rplevel > 0.0)
			printf("Adding %.3f%% average deviation %s noise to %s PCS values\n",rplevel * 100.0,unidist ? "uniform" : "normal", dolab ? "L*a*b*": "XYZ");
		fflush(stdout);
	}

	/* Deal with input CGATS files */
	icg = new_cgats();			/* Create a CGATS structure */
	icg->add_other(icg, "CTI1"); 	/* our special input type is Calibration Target Information 1 */
	icg->add_other(icg, "CTI3"); 	/* also accept renamed .ti3 file */

	if (icg->read_name(icg, inname))
		error("CGATS file read error : %s",icg->err);

	if (icg->ntables == 0 || icg->t[0].tt != tt_other || (icg->t[0].oi != 0 && icg->t[0].oi != 1))
		error ("Input file isn't a CTI1 format file");
	if (icg->t[0].oi == 1)
		inti3 = 1;			/* It's a renamed .ti3 file */
	if (icg->ntables != 1 && icg->ntables != 2 && icg->ntables != 3)
		error ("Input file doesn't contain one, two or three tables");

	if ((npat = icg->t[0].nsets) <= 0)
		error ("No sets of data");

	/* Figure out the color space of the .ti1 */
	if ((fi = icg->find_kword(icg, 0, "COLOR_REP")) < 0)
		error ("Input file doesn't contain keyword COLOR_REP");

	if (inti3) {
		char *rbuf, *outc;

		if ((rbuf = strdup(icg->t[0].kdata[fi])) == NULL)
			error("Malloc failed");
	
		/* Split COLOR_REP into device and PCS space */
		if ((outc = strchr(rbuf, '_')) == NULL)
			error("Input file '%s' COLOR_REP '%s' invalid", inname, icg->t[0].kdata[fi]);
		*outc++ = '\000';
	
		if ((nmask = icx_char2inkmask(rbuf)) == 0) {
			error ("Input file '%s' keyword COLOR_REP has unknown device value '%s'",inname,rbuf);
		}

		free(rbuf);
	} else {
		if ((nmask = icx_char2inkmask(icg->t[0].kdata[fi])) == 0)
			error ("Input file '%s' keyword COLOR_REP has unknown value '%s'",inname, icg->t[0].kdata[fi]);
	}

	/* Setup output cgats file */
	ocg = new_cgats();				/* Create a CGATS structure */
	ocg->add_other(ocg, "CTI3"); 		/* our special type is Calibration Target Information 3 */
	ocg->add_table(ocg, tt_other, 0);	/* Start the first table */

	ocg->add_kword(ocg, 0, "DESCRIPTOR", "Argyll Calibration Target chart information 3",NULL);
	ocg->add_kword(ocg, 0, "ORIGINATOR", "Argyll fakeread", NULL);
	atm[strlen(atm)-1] = '\000';	/* Remove \n from end */
	ocg->add_kword(ocg, 0, "CREATED",atm, NULL);

	/* What sort of device this is */
	if (ti3 != NULL) {
		if ((ti = ti3->find_kword(ti3, 0, "DEVICE_CLASS")) < 0)
			error("Input TI3 doesn't contain keyword DEVICE_CLASS");
		ocg->add_kword(ocg, 0, "DEVICE_CLASS",ti3->t[0].kdata[ti], NULL);	/* Copy */
	} else if (mlu != NULL) {
		/* This is a guess. It may not be correct. */
		if ((cnv_nmask & ICX_ADDITIVE) ^ (cnv_nmask & ICX_INVERTED)) {	
			ocg->add_kword(ocg, 0, "DEVICE_CLASS","DISPLAY", NULL);
			if (nmask == ICX_IRGB)
				warning("It's unusual to have an iRGB display device !");
		} else {
			ocg->add_kword(ocg, 0, "DEVICE_CLASS","OUTPUT", NULL);
			if (nmask == ICX_RGB)
				warning("It's unusual to have an RGB printing device !");
		}
	} else {	/* Assume ICC */
		if (icc_icco->header->deviceClass == icSigDisplayClass) {
			ocg->add_kword(ocg, 0, "DEVICE_CLASS","DISPLAY", NULL);
			if (nmask == ICX_IRGB)
				warning("It's unusual to have an iRGB display device !");
		} else {
			ocg->add_kword(ocg, 0, "DEVICE_CLASS","OUTPUT", NULL);
			if (nmask == ICX_RGB)
				warning("It's unusual to have an RGB printing device !");
		}
	}

	if ((ti = icg->find_kword(icg, 0, "SINGLE_DIM_STEPS")) >= 0)
		ocg->add_kword(ocg, 0, "SINGLE_DIM_STEPS",icg->t[0].kdata[ti], NULL);

	if ((ti = icg->find_kword(icg, 0, "COMP_GREY_STEPS")) >= 0)
		ocg->add_kword(ocg, 0, "COMP_GREY_STEPS",icg->t[0].kdata[ti], NULL);

	if ((ti = icg->find_kword(icg, 0, "MULTI_DIM_STEPS")) >= 0)
		ocg->add_kword(ocg, 0, "MULTI_DIM_STEPS",icg->t[0].kdata[ti], NULL);

	if ((ti = icg->find_kword(icg, 0, "FULL_SPREAD_PATCHES")) >= 0)
		ocg->add_kword(ocg, 0, "FULL_SPREAD_PATCHES",icg->t[0].kdata[ti], NULL);
	
	if (icc_luo == NULL) {	/* If MPP profile, we know what the target instrument is */
		ocg->add_kword(ocg, 0, "TARGET_INSTRUMENT", inst_name(itype) , NULL);
	}

	/* Fields we want */
	ocg->add_field(ocg, 0, "SAMPLE_ID", nqcs_t);

	if ((si = icg->find_field(icg, 0, "SAMPLE_ID")) < 0)
		error ("Input file doesn't contain field SAMPLE_ID");

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
			if (icc_luo != NULL) {
				/* Check if icc is compatible with .ti1 */
				if (sep_outs != ins)
					error("ICC device space '%s' dosen't match Separation ICC '%s'",
					       icm2str(icmColorSpaceSignature, ins),
					       icm2str(icmColorSpaceSignature, sep_outs));
			} else if (mlu != NULL) {
				/* Check if mpp is compatible with .ti1 */
				if (icx_colorant_comb_match_icc(cnv_nmask, sep_outs) == 0)
					error("MPP device space '%s' doesn't match Separation ICC '%s'",
					       icx_inkmask2char(cnv_nmask, 1),	/* Should free(). */
					       icm2str(icmColorSpaceSignature, sep_outs));
			} else if (ti3 != NULL) {
				/* Check if .ti3 is compatible with .ti1 */
				if (icx_colorant_comb_match_icc(cnv_nmask, sep_outs) == 0)
					error("TI3 device space '%s' doesn't match Separation ICC '%s'",
					       icx_inkmask2char(cnv_nmask, 1),	/* Should free(). */
					       icm2str(icmColorSpaceSignature, sep_outs));
			}
		} else if (icc_luo != NULL) {
			/* Check if icc is compatible with .ti1 */
			if (nmask == ICX_W && ins == icSigRgbData)
				gfudge = 1;
			else if (nmask == ICX_K && ins == icSigCmykData)
				gfudge = 2;		/* Should allow for other colorant combo's that include black */
			else if (icx_colorant_comb_match_icc(nmask, ins) == 0) {
				error("ICC device space '%s' dosen't match TI1 '%s'",
				       icm2str(icmColorSpaceSignature, ins),
				       ident);	// Should free().
			}
		} else if (mlu != NULL) {
			/* Check if mpp is compatible with .ti1 */
			if (nmask == ICX_W && (cnv_nmask == ICX_RGB || cnv_nmask == ICX_IRGB))
				gfudge = 1;
			else if (nmask == ICX_K && (cnv_nmask & ICX_BLACK))
				gfudge = 2;
			else if (cnv_nmask != nmask)
				error("MPP device space '%s' doesn't match TI1 '%s'",
				       icx_inkmask2char(cnv_nmask, 1), ident);	// Should free().
		} else if (ti3 != NULL) {
			/* Check if .ti3 is compatible with .ti1 */
			if (nmask == ICX_W && (cnv_nmask == ICX_RGB || cnv_nmask == ICX_IRGB))
				gfudge = 1;
			else if (nmask == ICX_K && (cnv_nmask & ICX_BLACK))
				gfudge = 2;
			else if (cnv_nmask != nmask)
				error("TI3 device space '%s' doesn't match TI1 '%s'",
				       icx_inkmask2char(cnv_nmask, 1), ident);	// Should free().
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
			ocg->add_field(ocg, 0, dolab ? labfname[j] : xyzfname[j], r_t);
		}

		/* If we have spectral information, output it too */
		if (dospec > 0 && spec_n > 0) {
			char buf[100];

			nsetel += spec_n;		/* Spectral values */
			sprintf(buf,"%d", spec_n);
			ocg->add_kword(ocg, 0, "SPECTRAL_BANDS",buf, NULL);
			sprintf(buf,"%f", spec_wl_short);
			ocg->add_kword(ocg, 0, "SPECTRAL_START_NM",buf, NULL);
			sprintf(buf,"%f", spec_wl_long);
			ocg->add_kword(ocg, 0, "SPECTRAL_END_NM",buf, NULL);

			/* Generate fields for spectral values */
			for (i = 0; i < spec_n; i++) {
				int nm;
		
				/* Compute nearest integer wavelength */
				nm = (int)(spec_wl_short + ((double)i/(spec_n-1.0))
				            * (spec_wl_long - spec_wl_short) + 0.5);
				
				sprintf(buf,"SPEC_%03d",nm);
				ocg->add_field(ocg, 0, buf, r_t);
			}
		}

		{
			char fname[100];
			sprintf(fname, dolab ? "%s_LAB" : "%s_XYZ", ident);
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
					error ("%d, %s",icc_icco->errc,icc_icco->err);

			/* Do calibration */
			if (applycal && cal != NULL)
				cal->interp(cal, sep, sep);

			/* Add randomness and non-linearity to device values. */
			/* rdlevel = avg. dev. */
			/* Note dev/sep is 0-1.0 at this stage */
			for (j = 0; j < inn; j++) {
				double dv = sep[j];
				if (rdlevel > 0.0) {
					double rr;
					if (unidist)
						rr =  d_rand(-2.0 * rdlevel, 2.0 * rdlevel);
					else
						rr = 1.2533 * rdlevel * norm_rand();
					dv += rr;
					if (dv < 0.0)
						dv = 0.0;
					else if (dv > 1.0)
						dv = 1.0;
				}
				if (j < 10 && chpow[j] != 1.0) {
					dv = pow(dv, chpow[j]);
				}
				sep[j] = dv;
			}

			/* Do color conversion */
			if (icc_luo != NULL) {
				if (icc_luo->lookup(icc_luo, PCS, sep) > 1)
					error ("%d, %s",icc_icco->errc,icc_icco->err);

				if (tbp[0] >= 0) {	/* Doing black point scaling */

					for (j = 0; j < 3; j++)
						PCS[j] -= wp[j];
					icmMulBy3x3(PCS, bpt, PCS);
					for (j = 0; j < 3; j++)
						PCS[j] += wp[j];
				}
	
			} else if (mlu != NULL) {
				mlu->lookup(mlu, PCS, sep);
				if (dospec && spec_n > 0) {
					mlu->lookup_spec(mlu, &out, sep);
				}
			} else if (ti3 != NULL) {
				int m;
				double bdif = 1e6;
				int bix = -1;

				/* Search for the closest device values in TI3 file */
				for (m = 0; m < ti3_npat; m++) {
					double dif;

					for (dif = 0.0, j = 0; j < nchan; j++) {
						double xx;

						xx = (*((double *)ti3->t[0].fdata[m][ti3_chix[j]]) / 100.0) - sep[j];
						dif += xx * xx;
					}
					if (dif < bdif) {
						bdif = dif;
						bix = m;
					}
				}
				/* Copy best value over */
				if (!dosep)		/* Doesn't make sense for separation */
					for (j = 0; j < nchan; j++) {
						dev[j] = *((double *)ti3->t[0].fdata[bix][ti3_chix[j]]) / 100.0;
					}
				for (j = 0; j < 3; j++) {
					PCS[j] = *((double *)ti3->t[0].fdata[bix][ti3_pcsix[j]]);
				}
				if (ti3_isLab && !dolab) {	/* Convert Lab to XYZ */
					icmLab2XYZ(&icmD50, PCS, PCS);
				} else if (!ti3_isLab && dolab) {	/* Convert XYZ to Lab */
					icmXYZ2Lab(&icmD50, PCS, PCS);
				} else if (!ti3_isLab) {		/* Convert XYZ100 to XYZ1 */
					PCS[0] /= 100.0;
					PCS[1] /= 100.0;
					PCS[2] /= 100.0;
				}
				if (dospec && spec_n > 0) {
					for (j = 0; j < spec_n; j++) {
						out.spec[j] = *((double *)ti3->t[0].fdata[bix][ti3_spi[j]]);
					}
				}
			}

			setel[k++].c = id;
			
			for (j = 0; j < nchan; j++)
				setel[k++].d = 100.0 * odev[j];

			if (dolab == 0) {
				PCS[0] *= 100.0;
				PCS[1] *= 100.0;
				PCS[2] *= 100.0;
			}

			/* Add randomness. rplevel is avg. dev. */
			/* Note PCS is 0..100 XYZ or Lab at this point */
			if (rplevel > 0.0) {
				double opcs[3];
				for (j = 0; j < 3; j++) {
					double dv = PCS[j];
					double rr;
					opcs[j] = dv;
					if (unidist)
						rr = 100.0 * d_rand(-2.0 * rplevel, 2.0 * rplevel);
					else
						rr = 100.0 * 1.2533 * rplevel * norm_rand();
					dv += rr;

					/* Don't let L*, X, Y or Z go negative */
					if ((!dolab || j == 0) && dv < 0.0)
						dv = 0.0;
					PCS[j] = dv;
				}
//printf("~1 pcs %f %f %f -> %f %f %f\n", opcs[0], opcs[1], opcs[2], PCS[0], PCS[1], PCS[2]);
			}

			setel[k++].d = PCS[0];
			setel[k++].d = PCS[1];
			setel[k++].d = PCS[2];

			if (dospec && spec_n > 0) {
				for (j = 0; j < spec_n; j++) {
					setel[k++].d = 100.0 * out.spec[j];
				}
			}

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

	/* If there is a calibration, append it to the .ti3 file */
	if (cal != NULL) {
		if (cal->write_cgats(cal, ocg) != 0)
			error("Writing cal error : %s",cal->err);
	}
	
	if (cal != NULL)
		cal->del(cal);

	if (icc_luo != NULL) {
		/* Cleanup ICC profile */
		icc_luo->del(icc_luo);
		icc_icco->del(icc_icco);
		icc_fp->del(icc_fp);
	} else if (mlu != NULL) {
		mlu->del(mlu);
	} else if (ti3 != NULL) {
		ti3->del(ti3);
	}

	if (ocg->write_name(ocg, outname))
		error("Write error : %s",ocg->err);

	ocg->del(ocg);		/* Clean up */
	icg->del(icg);		/* Clean up */

	return 0;
}


