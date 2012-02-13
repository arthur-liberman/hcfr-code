
/*
 * Argyll Color Correction System
 * Spectral .ti3 file converter
 *
 * Copyright 2005 Gerhard Fuernkranz
 * All rights reserved.
 *
 * Copyright 2006, 2007 Graeme Gill.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * See the License2.txt file for details.
 *
 * Pursuant to the above, this file is licenced in ArgyllCMS
 * under the GNU GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * This program takes the spectral data in a .ti3 file, converts them
 * to XYZ and Lab and fills the XYZ_[XYZ] and LAB_[LAB] columns in the
 * output .ti3 file with the computed XYZ and Lab values. If the columns
 * XYZ_[XYZ] and/or LAB_[LAB] are missing in the input file, they are
 * added to the output file.
 *
 * All other colums are copied from the input to the output .ti3 file.
 *
 * If the -f option is used, the FWA corrected spectral reflectances
 * are written to the output .ti3 file, instead of simply copying the
 * spectral reflectances from the input .ti3 file. In this case, the
 * XYZ_[XYZ] and LAB_[LAB] values are computed from the FWA corrected
 * reflectances as well.
 */

/* 
	NOTE this uses hard coded space signatures, and should be converted 
	to use xcolorants instead.

	This doesn't handle display .ti3's properly, as the resulting CIE values
	need to be normalised to Y=100 or marked as not normalised.

	Calibration tables aren't being passed through either ??
 */

#define ALLOW_PLOT
#define XRES 200


#include <stdio.h>
#if defined(__IBMC__)
#include <float.h>
#endif
#include "aconfig.h"
#include "numlib.h"
#include "cgats.h"
#include "xicc.h"
#include "insttypes.h"
#include "icoms.h"
#include "conv.h"
#include "inst.h"
#ifdef ALLOW_PLOT
#include "plot.h"
#endif


void
usage (void)
{
	fprintf (stderr, "Convert spectral .ti3 file, Version %s\n", ARGYLL_VERSION_STR);
	fprintf (stderr, "Author: Gerhard Fuernkranz, licensed under the AGPL Version 3\n");
	fprintf (stderr, "\n");
	fprintf (stderr, "Usage: spec2cie [options] input.ti3 output.ti3\n");
	fprintf (stderr, " -v          Verbose mode\n");
	fprintf (stderr, " -i illum    Choose viewing illuminant:\n");
	fprintf (stderr, "             A, C, D50 (def.), D65, F5, F8, F10 or file.sp\n");
	fprintf (stderr, " -o observ   Choose CIE Observer for spectral data:\n");
	fprintf (stderr, "             1931_2 (def), 1964_10, S&B 1955_2, shaw, J&V 1978_2\n");
	fprintf (stderr, " -f          Use Fluorescent Whitening Agent compensation\n");
	fprintf (stderr, " -I illum    Override instrument illuminant in .ti3 file:\n");
	fprintf (stderr, "             A, C, D50, D65, F5, F8, F10 or file.sp\n");
	fprintf (stderr, "             (only used in conjunction with -f)\n");
	fprintf (stderr, " -n          Don't output spectral values\n"); 
#ifdef ALLOW_PLOT
	fprintf (stderr, " -p          Plot each values spectrum\n"); 
#endif
	fprintf (stderr, " input.ti3   Measurement file\n");
	fprintf (stderr, " output.ti3  Converted measurement file\n");
	exit (1);
}

int
main(int argc, char *argv[])
{
	int fa, nfa;					/* current argument we're looking at */
	int verb = 0;
	int nospec = 0;					/* NZ if not to output spectral values */
	char *in_ti3_name;
	char *out_ti3_name;
	cgats *icg;						/* input cgats structure */
	cgats *ocg;						/* output cgats structure */
	cgats_set_elem *elems;

	int isdisp = 0;					/* nz if this is a display device */
	int isdnormed = 0;				/* Has display data been normalised to 100 ? */
	icColorSpaceSignature devspace = icmSigDefaultData;	/* The device colorspace */
	int isAdditive = 0;				/* 0 if subtractive, 1 if additive colorspace */
	int isInverted = 0;				/* nz if inverted real device */
	int ci, mi, yi, ki;				/* Indexes of device values */
	int fwacomp = 0;				/* FWA compensation */
	int doplot = 0;					/* Plot each patches spectrum */
	xspect cust_illum;				/* Custom illumination spectrum */
	xspect inst_cust_illum;			/* Custom illumination spectrum */
	char* illum_str = "D50";
	icxIllumeType illum = icxIT_D50;			/* Spectral defaults */
	icxIllumeType inst_illum = icxIT_none;		/* Spectral defaults */
	icxObserverType observ = icxOT_CIE_1931_2;

	int npat;						/* Number of patches */
	char *kw;
	int i, j, jj, k;

#if defined(__IBMC__)
	_control87 (EM_UNDERFLOW, EM_UNDERFLOW);
	_control87 (EM_OVERFLOW, EM_OVERFLOW);
#endif

	if (argc < 3)
		usage ();

	/* Process the arguments */
	for (fa = 1; fa < argc; fa++) {

		nfa = fa;				/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-') {		/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa + 1) < argc) {
					if (argv[fa + 1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage ();

			/* Verbose */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V')
				verb = 1;

			/* Don't output spectral */
			else if (argv[fa][1] == 'n' || argv[fa][1] == 'N')
				nospec = 1;

			/* Plot each patch spectral value */
			else if (argv[fa][1] == 'p' || argv[fa][1] == 'P')
				doplot = 1;

			/* Instrument Illuminant type */
			else if (argv[fa][1] == 'I') {
				fa = nfa;
				if (na == NULL)
					usage ();
				if (strcmp (na, "A") == 0) {
					inst_illum = icxIT_A;
				}
				else if (strcmp (na, "C") == 0) {
					inst_illum = icxIT_C;
				}
				else if (strcmp (na, "D50") == 0) {
					inst_illum = icxIT_D50;
				}
				else if (strcmp (na, "D65") == 0) {
					inst_illum = icxIT_D65;
				}
				else if (strcmp (na, "F5") == 0) {
					inst_illum = icxIT_F5;
				}
				else if (strcmp (na, "F8") == 0) {
					inst_illum = icxIT_F8;
				}
				else if (strcmp (na, "F10") == 0) {
					inst_illum = icxIT_F10;
				}
				else {				/* Assume it's a filename */
					inst_illum = icxIT_custom;
					if (read_xspect (&inst_cust_illum, na) != 0)
						usage ();
				}
			}

			/* Spectral Illuminant type */
			else if (argv[fa][1] == 'i') {
				fa = nfa;
				if (na == NULL)
					usage ();
				illum_str = na;
				if (strcmp (na, "A") == 0) {
					illum = icxIT_A;
				}
				else if (strcmp (na, "C") == 0) {
					illum = icxIT_C;
				}
				else if (strcmp (na, "D50") == 0) {
					illum = icxIT_D50;
				}
				else if (strcmp (na, "D65") == 0) {
					illum = icxIT_D65;
				}
				else if (strcmp (na, "F5") == 0) {
					illum = icxIT_F5;
				}
				else if (strcmp (na, "F8") == 0) {
					illum = icxIT_F8;
				}
				else if (strcmp (na, "F10") == 0) {
					illum = icxIT_F10;
				}
				else {				/* Assume it's a filename */
					illum = icxIT_custom;
					if (read_xspect (&cust_illum, na) != 0)
						usage ();
				}
			}

			/* Spectral Observer type */
			else if (argv[fa][1] == 'o' || argv[fa][1] == 'O') {
				fa = nfa;
				if (na == NULL)
					usage ();
				if (strcmp (na, "1931_2") == 0) {		/* Classic 2 degree */
					observ = icxOT_CIE_1931_2;
				}
				else if (strcmp (na, "1964_10") == 0) {		/* Classic 10 degree */
					observ = icxOT_CIE_1964_10;
				}
				else if (strcmp (na, "1955_2") == 0) {		/* Stiles and Burch 1955 2 degree */
					observ = icxOT_Stiles_Burch_2;
				}
				else if (strcmp (na, "1978_2") == 0) {		/* Judd and Voss 1978 2 degree */
					observ = icxOT_Judd_Voss_2;
				}
				else if (strcmp (na, "shaw") == 0) {		/* Shaw and Fairchilds 1997 2 degree */
					observ = icxOT_Shaw_Fairchild_2;
				}
				else
					usage ();
			}

			else if (argv[fa][1] == 'f' || argv[fa][1] == 'F')
				fwacomp = 1;

			else
				usage ();
		}
		else
			break;
	}

	/* Get the file name arguments */
	if (fa >= argc || argv[fa][0] == '-')
		usage ();

	in_ti3_name = argv[fa++];

	if (fa >= argc || argv[fa][0] == '-')
		usage ();

	out_ti3_name = argv[fa++];

	/* Open and look at the .ti3 profile patches file */

	icg = new_cgats ();						/* Create a CGATS structure */
	icg->add_other (icg, "CTI3");		/* Calibration Target Information 3 */

	ocg = new_cgats ();						/* Create a CGATS structure */
	ocg->add_other (ocg, "CTI3");		/* Calibration Target Information 3 */

	if (icg->read_name (icg, in_ti3_name))
		error ("CGATS file read error: %s", icg->err);

	if (icg->ntables == 0 || icg->t[0].tt != tt_other || icg->t[0].oi != 0)
		error ("Input file isn't a CTI3 format file");
	if (icg->ntables < 1)
		error ("Input file doesn't contain at least one table");

	/* add table to output file */

	ocg->add_table(ocg, tt_other, 0);

	/* copy keywords */

	for (i = 0; i < icg->t[0].nkwords; i++) {
		kw = icg->t[0].ksym[i];
		if (fwacomp && strcmp(kw, "TARGET_INSTRUMENT") == 0) {
			/*
			 * overwrite TARGET_INSTRUMENT with the new illuminant,
			 * since the FWA corrected spectral data are no longer
			 * valid for the original instrument's illuminant
			 */
			ocg->add_kword (ocg, 0, kw, illum_str, NULL);
		}
		else {
			ocg->add_kword (ocg, 0, kw,
							icg->t[0].kdata[i], icg->t[0].kcom[i]);
		}
	}

	/* Figure out what sort of device it is */

	{
		int ti;

		if ((ti = icg->find_kword (icg, 0, "DEVICE_CLASS")) < 0)
			error ("Input file doesn't contain keyword DEVICE_CLASS");

		if (strcmp (icg->t[0].kdata[ti], "DISPLAY") == 0) {
			isdisp = 1;
		}

		/* See if the display CIE data has been normalised to Y = 100 */
		if ((ti = icg->find_kword(icg, 0, "NORMALIZED_TO_Y_100")) < 0
		 || strcmp(icg->t[0].kdata[ti],"NO") == 0) {
			isdnormed = 0;
		} else {
			isdnormed = 1;
		}

		if (isdisp && fwacomp) {
			error ("FWA compensation cannot be used for DISPLAY devices");
		}

		if ((ti = icg->find_kword(icg, 0, "COLOR_REP")) < 0)
			error("Input file doesn't contain keyword COLOR_REP");

		if (strncmp(icg->t[0].kdata[ti],"CMYK_",5) == 0)
			devspace = icSigCmykData;
		else if (strncmp(icg->t[0].kdata[ti],"CMY_",4) == 0)
			devspace = icSigCmyData;
		else if (strncmp(icg->t[0].kdata[ti],"RGB_",4) == 0)
			devspace = icSigRgbData;
		else if (strncmp(icg->t[0].kdata[ti],"iRGB_",4) == 0) {
			devspace = icSigRgbData;
			isInverted = 1;
		} else if (strncmp(icg->t[0].kdata[ti],"K_",2) == 0) {
			devspace = icSigGrayData;
			isAdditive = 0;
		} else if (strncmp(icg->t[0].kdata[ti],"W_",2) == 0) {
			devspace = icSigGrayData;
			isAdditive = 1;
		} else 
			error("Device has unhandled color representation '%s'",icg->t[0].kdata[ti]);

		if (devspace == icSigGrayData) {

			if (isAdditive) {
				if ((ci = icg->find_field(icg, 0, "GRAY_W")) < 0)
					error("Input file doesn't contain field GRAY_W");
				if (icg->t[0].ftype[ci] != r_t)
					error("Field GRAY_W is wrong type - corrupted file ?");
			} else {
				if ((ci = icg->find_field(icg, 0, "GRAY_K")) < 0)
					error("Input file doesn't contain field GRAY_K");
				if (icg->t[0].ftype[ci] != r_t)
					error("Field GRAY_K is wrong type - corrupted file ?");
			}
			mi = yi = ki = ci;

		} else if (devspace == icSigRgbData) {

			if ((ci = icg->find_field(icg, 0, "RGB_R")) < 0)
				error("Input file doesn't contain field RGB_R");
			if (icg->t[0].ftype[ci] != r_t)
				error("Field RGB_R is wrong type - corrupted file ?");
			if ((mi = icg->find_field(icg, 0, "RGB_G")) < 0)
				error("Input file doesn't contain field RGB_G");
			if (icg->t[0].ftype[mi] != r_t)
				error("Field RGB_G is wrong type - corrupted file ?");
			if ((yi = icg->find_field(icg, 0, "RGB_B")) < 0)
				error("Input file doesn't contain field RGB_B");
			if (icg->t[0].ftype[yi] != r_t)
				error("Field RGB_B is wrong type - corrupted file ?");
			ki = yi;

		} else if (devspace == icSigCmyData) {

			if ((ci = icg->find_field(icg, 0, "CMY_C")) < 0)
				error("Input file doesn't contain field CMY_C");
			if (icg->t[0].ftype[ci] != r_t)
				error("Field CMY_C is wrong type - corrupted file ?");
			if ((mi = icg->find_field(icg, 0, "CMY_M")) < 0)
				error("Input file doesn't contain field CMY_M");
			if (icg->t[0].ftype[mi] != r_t)
				error("Field CMY_M is wrong type - corrupted file ?");
			if ((yi = icg->find_field(icg, 0, "CMY_Y")) < 0)
				error("Input file doesn't contain field CMY_Y");
			if (icg->t[0].ftype[yi] != r_t)
				error("Field CMY_Y is wrong type - corrupted file ?");
			ki = yi;

		} else {	/* Assume CMYK */

			if ((ci = icg->find_field(icg, 0, "CMYK_C")) < 0)
				error("Input file doesn't contain field CMYK_C");
			if (icg->t[0].ftype[ci] != r_t)
				error("Field CMYK_C is wrong type - corrupted file ?",icg->t[0].ftype[ci],r_t);
			if ((mi = icg->find_field(icg, 0, "CMYK_M")) < 0)
				error("Input file doesn't contain field CMYK_M");
			if (icg->t[0].ftype[mi] != r_t)
				error("Field CMYK_M is wrong type - corrupted file ?");
			if ((yi = icg->find_field(icg, 0, "CMYK_Y")) < 0)
				error("Input file doesn't contain field CMYK_Y");
			if (icg->t[0].ftype[yi] != r_t)
				error("Field CMYK_Y is wrong type - corrupted file ?");
			if ((ki = icg->find_field(icg, 0, "CMYK_K")) < 0)
				error("Input file doesn't contain field CMYK_K");
			if (icg->t[0].ftype[ki] != r_t)
				error("Field CMYK_K is wrong type - corrupted file ?");
		}
	}

	if ((npat = icg->t[0].nsets) <= 0)
		error ("No sets of data");

	/* Read in the CGATs fields */

	{
		int sidx;				/* Sample ID index */
		int ti, ii, Xi, Yi, Zi, Li, ai, bi;
		int spi[XSPECT_MAX_BANDS];		/* CGATS indexes for each wavelength */
		xsp2cie *sp2cie;				/* Spectral conversion object */
		xspect sp;
		double XYZ[3];
		double Lab[3];
		char buf[100];
							/* These are only set if fwa is needed */
		xspect rmwsp;		/* Raw medium white spectrum */
		xspect mwsp;		/* FWA compensated medium white spectrum */
		double mwXYZ[3];	/* Media white XYZ */

		if ((sidx = icg->find_field (icg, 0, "SAMPLE_ID")) < 0)
			error ("Input file doesn't contain field SAMPLE_ID");
		if (icg->t[0].ftype[sidx] != nqcs_t)
			error ("Field SAMPLE_ID is wrong type");

		/* Using spectral data */

		if ((ii = icg->find_kword (icg, 0, "SPECTRAL_BANDS")) < 0)
			error ("Input file doesn't contain keyword SPECTRAL_BANDS");
		sp.spec_n = atoi (icg->t[0].kdata[ii]);
		if ((ii = icg->find_kword (icg, 0, "SPECTRAL_START_NM")) < 0)
			error ("Input file doesn't contain keyword SPECTRAL_START_NM");
		sp.spec_wl_short = atof (icg->t[0].kdata[ii]);
		if ((ii = icg->find_kword (icg, 0, "SPECTRAL_END_NM")) < 0)
			error ("Input file doesn't contain keyword SPECTRAL_END_NM");
		sp.spec_wl_long = atof (icg->t[0].kdata[ii]);
		if (!isdisp || isdnormed != 0)
			sp.norm = 100.0;
		else
			sp.norm = 1.0;

		/* Find the fields for spectral values */
		for (j = 0; j < sp.spec_n; j++) {
			int nm;

			/* Compute nearest integer wavelength */
			nm = (int) (sp.spec_wl_short +
						((double) j / (sp.spec_n - 1.0))
						* (sp.spec_wl_long - sp.spec_wl_short) + 0.5);

			sprintf (buf, "SPEC_%03d", nm);

			if ((spi[j] = icg->find_field (icg, 0, buf)) < 0)
				error ("Input file doesn't contain field %s", buf);
		}

		if (isdisp) {
			illum = icxIT_none;		/* Displays are assumed to be self luminous */
		}

		/* copy fields to output file (except spectral if nospec) */
		for (i = 0; i < icg->t[0].nfields; i++) {
			/* See if this is a input spectral field */
			for (j = 0; nospec && j < sp.spec_n; j++) {
				if (spi[j] == i)
					break;			/* Yes it is */
			}
			if (nospec == 0 || j >= sp.spec_n)
				ocg->add_field (ocg, 0, icg->t[0].fsym[i], icg->t[0].ftype[i]);
		}

		/* create field for XYZ and Lab if not present */

		if ((Xi = icg->find_field(icg, 0, "XYZ_X")) < 0)
			if ((Xi = ocg->add_field(ocg, 0, "XYZ_X", r_t)) < 0)
					error ("Cannot add field to table");

		if ((Yi = icg->find_field(icg, 0, "XYZ_Y")) < 0)
			if ((Yi = ocg->add_field(ocg, 0, "XYZ_Y", r_t)) < 0)
					error ("Cannot add field to table");

		if ((Zi = icg->find_field(icg, 0, "XYZ_Z")) < 0)
			if ((Zi = ocg->add_field(ocg, 0, "XYZ_Z", r_t)) < 0)
					error ("Cannot add field to table");

		if ((Li = icg->find_field(icg, 0, "LAB_L")) < 0)
			if ((Li = ocg->add_field(ocg, 0, "LAB_L", r_t)) < 0)
					error ("Cannot add field to table");

		if ((ai = icg->find_field(icg, 0, "LAB_A")) < 0)
			if ((ai = ocg->add_field(ocg, 0, "LAB_A", r_t)) < 0)
					error ("Cannot add field to table");

		if ((bi = icg->find_field(icg, 0, "LAB_B")) < 0)
			if ((bi = ocg->add_field(ocg, 0, "LAB_B", r_t)) < 0)
					error ("Cannot add field to table");

		/* allocate elements */

		if ((elems = (cgats_set_elem *)
			 calloc(ocg->t[0].nfields, sizeof(cgats_set_elem))) == NULL)
		{
			error("Out of memory");
		}

		/* Create a spectral conversion object */
		if ((sp2cie = new_xsp2cie (illum,
								   illum == icxIT_none ?  NULL : &cust_illum,
								   observ, NULL, icSigXYZData)) == NULL)
		{
			error ("Creation of spectral conversion object failed");
		}

		if (fwacomp) {
			double nw = 0.0;	/* Number of media white patches */

			rmwsp = sp;      	/* Initial Struct copy */

			/* Find the media white spectral reflectance */
			for (j = 0; j < rmwsp.spec_n; j++)
				rmwsp.spec[j] = 0.0;

			/* Compute the mean of all the media white patches */
			for (i = 0; i < npat; i++) {
				int use = 0;

				if (devspace == icSigGrayData) {
					if (isAdditive) {
						if (*((double *)icg->t[0].fdata[i][ci]) > (100.0 - 0.1))
							use = 1;
					} else {
						if (*((double *)icg->t[0].fdata[i][ci]) < 0.1)
							use = 1;
					}
				} else if (devspace == icSigRgbData) {
					if (*((double *)icg->t[0].fdata[i][ci]) > (100.0 - 0.1)
					 && *((double *)icg->t[0].fdata[i][mi]) > (100.0 - 0.1)
					 && *((double *)icg->t[0].fdata[i][yi]) > (100.0 - 0.1))
						use = 1;
				} else if (devspace == icSigCmyData) {
					if (*((double *)icg->t[0].fdata[i][ci]) < 0.1 
					 && *((double *)icg->t[0].fdata[i][mi]) < 0.1 
					 && *((double *)icg->t[0].fdata[i][yi]) < 0.1)
						use = 1;
				} else {	/* Assume CMYK */

					if (*((double *)icg->t[0].fdata[i][ci]) < 0.1
					 && *((double *)icg->t[0].fdata[i][mi]) < 0.1
					 && *((double *)icg->t[0].fdata[i][yi]) < 0.1
					 && *((double *)icg->t[0].fdata[i][ki]) < 0.1) {
						use = 1;
					}
				}

				if (use) {
					/* Read the spectral values for this patch */
					for (j = 0; j < rmwsp.spec_n; j++) {
						rmwsp.spec[j] += *((double *)icg->t[0].fdata[i][spi[j]]);
					}
					nw++;
				}
			}

			if (nw == 0.0) {
				warning("Can't find a media white patch to compute white reference");
				warning("Using maximum of all spectral readings instead");

				/* Track the maximum reflectance for any band to determine white. */
				/* This might give bogus results if there is no white patch... */
				for (i = 0; i < npat; i++) {
					for (j = 0; j < rmwsp.spec_n; j++) {
						double rv = *((double *)icg->t[0].fdata[i][spi[j]]);
						if (rv > rmwsp.spec[j])
							rmwsp.spec[j] = rv;
					}
				}
				nw++;
			}
			for (j = 0; j < rmwsp.spec_n; j++) {
				rmwsp.spec[j] /= nw;	/* Compute average */
			}
			mwsp = rmwsp;		/* Structure copy */
		}

		if (fwacomp) {
			instType itype;		/* Spectral instrument type */
			xspect insp;		/* Instrument illuminant */

			if (inst_illum == icxIT_none) {
					/* try to get from .ti3 file */
				if ((ti = icg->find_kword (icg, 0, "TARGET_INSTRUMENT")) < 0)
					error ("Can't find target instrument needed for FWA compensation");

				if ((itype = inst_enum (icg->t[0].kdata[ti])) == instUnknown)
					error ("Unrecognised target instrument '%s'",
						   icg->t[0].kdata[ti]);

				if (inst_illuminant (&insp, itype) != 0)
					error ("Instrument doesn't have an FWA illuminent");
			}
			else if (inst_illum == icxIT_custom) {
					insp = inst_cust_illum;		/* Structure copy */
			}
			else {
					if (standardIlluminant(&insp, inst_illum, 0) != 0)
							error ("Failed to find standard illuminant");
			}

			/* (Note that sp and mwsp.norm is set to 100.0) */
			if (sp2cie->set_fwa(sp2cie, &insp, &mwsp)) 
				error ("Set FWA on sp2cie failed");

			if (verb) {
				double FWAc;
				sp2cie->get_fwa_info(sp2cie, &FWAc);
				printf("FWA content = %f\n",FWAc);
			}

			/* Create an FWA compensated white spectrum  and XYZ value */
			sp2cie->sconvert (sp2cie, &rmwsp, mwXYZ, &mwsp);
		}

		for (i = 0; i < npat; i++) {

			xspect corr_sp;

			/* copy all input colums to output (except spectral if nospec) */

			for (jj = j = 0; j < icg->t[0].nfields; j++) {
				for (k = 0; nospec && k < sp.spec_n; k++) {
					if (spi[k] == j)
						break;
				}
				if (nospec == 0 || k >= sp.spec_n) {
					switch (icg->t[0].ftype[j]) {
						case r_t:
							elems[jj].d = *((double *) icg->t[0].fdata[i][j]);
							break;
						case i_t:
							elems[jj].i = *((int *) icg->t[0].fdata[i][j]);
							break;
						default:
							elems[jj].c = (char *) icg->t[0].fdata[i][j];
					}
					jj++;
				}
			}

			/* Read the spectral values for this patch */
			for (j = 0; j < sp.spec_n; j++) {
				sp.spec[j] = *((double *)icg->t[0].fdata[i][spi[j]]);
			}

			if (fwacomp) {
				corr_sp = sp;

				/* Convert it to CIE space */
				sp2cie->sconvert (sp2cie, &corr_sp, XYZ, &sp);

				/* Write the corrected spectral values for this patch */
				if (nospec == 0) {
					for (j = 0; j < sp.spec_n; j++) {
						elems[spi[j]].d = corr_sp.spec[j];
					}
				}
#ifdef ALLOW_PLOT
				if (doplot) {
					int ii;
					double xx[XRES];
					double y1[XRES];
					double y2[XRES];
					double lab[3];

					icmXYZ2Lab(&icmD50, lab, XYZ);
					printf("Patch %d, XYZ = %f %f %f, Lab = %f %f %f\n",i,
					XYZ[0], XYZ[1], XYZ[2], lab[0], lab[1], lab[2]);

					/* Plot spectrum out */
					for (ii = 0; ii < XRES; ii++) {
						double ww;
				
						ww = (sp.spec_wl_long - sp.spec_wl_short)
						   * ((double)ii/(XRES-1.0)) + sp.spec_wl_short;
					
						xx[ii] = ww;
						y1[ii] = value_xspect(&sp, ww);
						y2[ii] = 100.0 * value_xspect(&corr_sp, ww);
					}
					do_plot(xx,y1,y2,NULL,ii);
				}
#endif
			}
			else {
				/* Convert it to CIE space */
				sp2cie->convert (sp2cie, XYZ, &sp);
#ifdef ALLOW_PLOT
				if (doplot) {
					int ii;
					double xx[XRES];
					double y1[XRES];
					double lab[3];
		
					icmXYZ2Lab(&icmD50, lab, XYZ);
					printf("Patch %d, XYZ = %f %f %f, Lab = %f %f %f\n",i,
					XYZ[0], XYZ[1], XYZ[2], lab[0], lab[1], lab[2]);
		
					/* Plot spectrum out */
					for (ii = 0; ii < XRES; ii++) {
						double ww;
				
						ww = (sp.spec_wl_long - sp.spec_wl_short)
						   * ((double)ii/(XRES-1.0)) + sp.spec_wl_short;
					
						xx[ii] = ww;
						y1[ii] = value_xspect(&sp, ww);
					}
					do_plot(xx,y1,NULL,NULL,ii);
				}
#endif
			}

			icmXYZ2Lab(&icmD50, Lab, XYZ);

			elems[Xi].d = XYZ[0] * 100.0;
			elems[Yi].d = XYZ[1] * 100.0;
			elems[Zi].d = XYZ[2] * 100.0;

			elems[Li].d = Lab[0];
			elems[ai].d = Lab[1];
			elems[bi].d = Lab[2];

			ocg->add_setarr(ocg, 0, elems);
		}

		if (ocg->write_name(ocg, out_ti3_name)) {
			error ("Write error: %s", ocg->err);
		}

		sp2cie->del (sp2cie);		/* Done with this */

		ocg->del (ocg);				/* Clean up */
		icg->del (icg);				/* Clean up */

			free (elems);
	}

	return 0;
}

