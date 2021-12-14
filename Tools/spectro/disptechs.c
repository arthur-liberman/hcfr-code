
 /* Standardized display types */

 /* Standardized display types for use with libinst */

/* 
 * Argyll Color Management System
 *
 * Author: Graeme W. Gill
 * Date:   14/5/2014
 *
 * Copyright 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "numsup.h"
#ifndef SALONEINSTLIB
#include "copyright.h"
#include "aconfig.h"
#include "icc.h"
#else
#include "sa_config.h"
#include "sa_conv.h"
#endif /* !SALONEINSTLIB */
#include "conv.h"
#include "disptechs.h"

/* Other selection characters used,
   that shouldn't be used in the disptech_info_array[] entries :

	"n"		Non-refresh (Generic)
	"r"		Refresh (Generic)
	"F"		Factory base calibration
	"R"		Raw sensor values
	"g"		Generic

  oemarch:
	"C"		CMF
	"U"		Custom
  kleink10:
	"P"		DLP projector using ambient
	"E"		SMPTE C
	"P"		Klein DLP Lux
	"d"		Klein LED Bk LCD
	"O"		Sony EL OLED
	"z"		Eizo CG LCD
 */

/* We deliberately duplicate the selection characters, */
/* because it's not usual to offer the whole list, just */
/* a sub-set which may not clash. */
/* disptechs_set_sel() should be used to present */
/* unique selectors. */
static disptech_info disptech_info_array[] = {
	{
		disptech_none,				/* Not applicable entry. Must be first */
		"None",						/* because disptech_get_list() assumes so */
		"None",
		0,
		0.001,
		0.001,
		NULL
	},

	{
		disptech_crt,
		"CRT",
		"CRT",
		1,
		DISPTECH_CRT_RISE,
		DISPTECH_CRT_FALL,
		"c"
	},
	{
		disptech_plasma,
		"Plasma",
		"Plasma",
		1,
		DISPTECH_CRT_RISE,
		DISPTECH_CRT_FALL,
		"m"
	},

	{
		disptech_lcd,
		"LCD",
		"LCD",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"l"
	},
	{
		disptech_lcd_ccfl,
		"LCD CCFL",
		"LCD CCFL",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"l"
	},
	{
		disptech_lcd_ccfl_ips,
		"LCD CCFL IPS",
		"LCD CCFL IPS",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"l"
	},
	{	disptech_lcd_ccfl_pva,
		"LCD CCFL PVA",
		"LCD CCFL PVA",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"l"
	},
	{	disptech_lcd_ccfl_tft,
		"LCD CCFL TFT",
		"LCD CCFL TFT",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"l"

	},
	{	disptech_lcd_ccfl_wg,
		"LCD CCFL Wide Gamut",
		"LCD CCFL Wide Gamut",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"L"
	},
	{	disptech_lcd_ccfl_wg_ips,
		"LCD CCFL Wide Gamut IPS",
		"LCD CCFL Wide Gamut IPS",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"L"
	},
	{	disptech_lcd_ccfl_wg_pva,
		"LCD CCFL Wide Gamut PVA",
		"LCD CCFL Wide Gamut PVA",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"L"
	},
	{	disptech_lcd_ccfl_wg_tft,
		"LCD CCFL Wide Gamut TFT",
		"LCD CCFL Wide Gamut TFT",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"L"
	},
	{	disptech_lcd_wled,
		"LCD White LED",
		"LCD White LED",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"e"
	},
	{	disptech_lcd_wled_ips,
		"LCD White LED IPS",
		"LCD White LED IPS",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"e"
	},
	{	disptech_lcd_wled_pva,
		"LCD White LED PVA",
		"LCD White LED PVA",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"e"
	},
	{	disptech_lcd_wled_tft,
		"LCD White LED TFT",
		"LCD White LED TFT",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"e"
	},
	{	disptech_lcd_rgbled,
		"LCD RGB LED",
		"LCD RGB LED",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"b"
	},
	{	disptech_lcd_rgbled_ips,
		"LCD RGB LED IPS",
		"LCD RGB LED IPS",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"b"
	},
	{	disptech_lcd_rgbled_pva,
		"LCD RGB LED PVA",
		"LCD RGB LED PVA",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"b"
	},
	{	disptech_lcd_rgbled_tft,
		"LCD RGB LED TFT",
		"LCD RGB LED TFT",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"b"

	},

	{	disptech_lcd_rgledp,
		"LCD RG Phosphor",
		"LCD RG Phosphor",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"h"
	},
	{	disptech_lcd_rgledp_ips,
		"LCD RG Phosphor IPS",
		"LCD RG Phosphor IPS",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"h"
	},
	{	disptech_lcd_rgledp_pva,
		"LCD RG Phosphor PVA",
		"LCD RG Phosphor PVA",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"h"
	},
	{	disptech_lcd_rgledp_tft,
		"LCD RG Phosphor TFT",
		"LCD RG Phosphor TFT",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"h"
	},

	{	disptech_lcd_nrgledp,
		"LCD PFS Phosphor",
		"LCD PFS Phosphor",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"h"
	},
	{	disptech_lcd_nrgledp_ips,
		"LCD PFS Phosphor IPS",
		"LCD PFS Phosphor IPS",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"h"
	},
	{	disptech_lcd_nrgledp_pva,
		"LCD PFS Phosphor PVA",
		"LCD PFS Phosphor PVA",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"h"
	},
	{	disptech_lcd_nrgledp_tft,
		"LCD PFS Phosphor TFT",
		"LCD PFS Phosphor TFT",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"h"
	},

	{	disptech_lcd_gbrledp,
		"LCD GB-R Phosphor",
		"LCD GB-R Phosphor",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"i"
	},
	{	disptech_lcd_gbrledp_ips,
		"LCD GB-R Phosphor IPS",
		"LCD GB-R Phosphor IPS",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"i"
	},
	{	disptech_lcd_gbrledp_pva,
		"LCD GB-R Phosphor PVA",
		"LCD GB-R Phosphor PVA",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"i"
	},
	{	disptech_lcd_gbrledp_tft,
		"LCD GB-R Phosphor TFT",
		"LCD GB-R Phosphor TFT",
		0,
		DISPTECH_LCD_RISE,
		DISPTECH_LCD_FALL,
		"i"
	},

	{	disptech_oled,
		"LED OLED",
		"LED OLED",
		0,
		DISPTECH_LED_RISE,
		DISPTECH_LED_FALL,
		"o"
	},
	{	disptech_amoled,
		"LED AMOLED",
		"LED AMOLED",
		0,
		DISPTECH_LED_RISE,
		DISPTECH_LED_FALL,
		"a"
	},
	{	disptech_woled,
		"LED WOLED",
		"LED WOLED",
		0,
		DISPTECH_LED_RISE,
		DISPTECH_LED_FALL,
		"w"
	},

	{	disptech_dlp,
		"Projector",
		"DLP Projector",
		1,
		DISPTECH_DLP_RISE,
		DISPTECH_DLP_FALL,
		"p"
	},
	{	disptech_dlp_rgb,
		"Projector RGB Filter Wheel",
		"DLP Projector RGB Filter Wheel",
		1,
		DISPTECH_DLP_RISE,
		DISPTECH_DLP_FALL,
		"p"
	},
	{	disptech_dlp_rgbw,
		"Projector RGBW Filter Wheel",
		"DPL Projector RGBW Filter Wheel",
		1,
		DISPTECH_DLP_RISE,
		DISPTECH_DLP_FALL,
		"p"
	},
	{	disptech_dlp_rgbcmy,
		"Projector RGBCMY Filter Wheel",
		"DLP Projector RGBCMY Filter Wheel",
		1,
		DISPTECH_DLP_RISE,
		DISPTECH_DLP_FALL,
		"p"
	},

	{
		disptech_unknown,
		"Unknown",
		"Unknown",
		1,
		DISPTECH_WORST_RISE,
		DISPTECH_WORST_FALL,
		"u"
	},

	{
		disptech_end				/* End marker */
	}
};


static int unknown_ix = -1;			/* Set to actual index by find_unknown() */

static void find_unknown() {
	int i;

	for (i = 0; disptech_info_array[i].dtech != disptech_end; i++) {
		if (disptech_info_array[i].dtech == disptech_unknown) {
			unknown_ix = i;
			break;
		}
	} 
}

/* Given the enum id, return the matching disptech_info entry */
/* Return the disptech_unknown entry if not matched */
disptech_info *disptech_get_id(disptech id) {
	int i;

	for (i = 0; disptech_info_array[i].dtech != disptech_end; i++) {
		if (disptech_info_array[i].dtech == id)
			return &disptech_info_array[i]; 
	} 

	if (unknown_ix < 0)
		find_unknown();
	return &disptech_info_array[unknown_ix];
}

/* Given the string id, return the matching disptech_info entry */
/* Return the disptech_unknown entry if not matched */
disptech_info *disptech_get_strid(char *strid) {
	int i;
	char *bp;

	/* Backwards compatibility hack - replace " VPA" with " PVA" */
	if ((bp = strstr(strid, " VPA")) != NULL) {
		bp[1] = 'P';
		bp[2] = 'V';
	}

	for (i = 0; disptech_info_array[i].dtech != disptech_end; i++) {

		if (strcmp(disptech_info_array[i].strid, strid) == 0)
			return &disptech_info_array[i]; 
	} 

	if (unknown_ix < 0)
		find_unknown();
	return &disptech_info_array[unknown_ix];
}

/* For each selector we need to:

	check each selector char
	if already used,
		remove it.
	if no selector remain,
		allocate a free one from the fallback list.
	mark all used selectors

	We treat the first selector as more important
	than any aliases that come after it, and the
	aliases as more important than the fallback list,
	so we need to do three passes through all the selections.
*/

/* Append the selection characters. */
/* If a selector is set, its index will be set in usels[]. */
/* Remove any used selectors from isel[]. */
/* If flag == 0, set from just first suggested selector. */
/* If flag == 1, set from just suggested selectors. */
/* If flag == 2, set from suggested and fallback selectors */
/* If flag == 3, append from suggested selectors */
void disptechs_set_sel(
	int flag,			/* See above */
	int ix,				/* Index of entry being set */
	char *osel,			/* Append unique selectors to this string. */
	char *isel,			/* Pointer to string list of suggested selectors, */
	char *usels,		/* char[256] initially -1, to track used selector entry index */
	int *k,				/* Index of next available selector in asels */
	char *asels			/* String list of fallback selectors to choose from, in order. */
) {
	char *iisel = isel, i;

//a1logd(g_log, 1,"disptechs_set_sel: flag %d, ix %d, osel '%s', isel '%s', k %d\n",flag, ix,osel,isel,*k);

	if (flag != 3) {
		/* See if we already have a selecor character */
		if (osel[0] != '\000') {
//a1logd(g_log, 1," already set OK\n");
			return;
		}
	} else {
		if (isel[0] == '\000') {
//a1logd(g_log, 1," nothing to set from\n");
			return;				/* Nothing to set from */
		}

		/* Get ready to append */
		osel += strlen(osel);
	}

	/* Set or add from the first unsed suggested selectors */ 
	for (i = 0; *isel != '\000'; isel++, i++) {
		if (flag == 0 && i > 0) {
//a1logd(g_log, 1," run out of primaries\n");
			break;					/* Looked at primary */
		}
		if (usels[*isel] == ((char)-1)) {		/* If this selector is not currently used */
//a1logd(g_log, 1," added to '%c' from %d\n", *isel, i);
			osel[0] = *isel;			/* Use it */
			osel[1] = '\000';
			usels[osel[0]] = ix;

			/* Remove all used/discarded from isel, in case we are called again. */
			for (isel++; ;isel++, iisel++) {
				*iisel = *isel; 
				if (*isel == '\000')
					break;
			}
			return;
		}
//a1logd(g_log, 1," sel '%c' at %d is used by ix %d\n", *isel, i, usels[*isel]);
	}

	/* If we get here, we haven't managed to add anything from the remaining */
	/* selectors, so mark the candidate list as empty: */
	iisel[0] = '\000';

	if (flag != 2) {
//a1logd(g_log, 1," returning without add\n");
		return;
	}

	/* Get the next unused char in fallback list */
	for (;asels[*k] != '\000'; (*k)++) {
		if (usels[asels[*k]] == ((char)-1))	/* Unused */
			break;
	}
	if (asels[*k] != '\000') { 
//a1logd(g_log, 1," set int to fallback '%c' at %d\n", asels[*k], *k);
		osel[0] = asels[*k]; 
		osel[1] = '\000';
		usels[osel[0]] = ix;
		(*k)++;
		return;
	}

//a1logd(g_log, 1," returning after fallback without add\n");
	/* If we got here, we failed to add a selector */
	return;
}

/* Return the display tech list with unique lsel lectors */
disptech_info *disptech_get_list() {
	disptech_info *list = &disptech_info_array[1];	/* skip disptech_none entry */

	int i, k;
	char usels[256];			/* Used selectors */
	static char *asels = "123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

	for (i = 0; i < 256; i++)
		usels[i] = ((char)-1);
	k = 0;		/* Next selector index */

	/* Add entries from the static list and their primary selectors */
	for (i = 0; list[i].dtech != disptech_end; i++) {
//a1logd(g_log,1,"tech[%d] '%s' sels = '%s'\n",i,list[i].desc,list[i].sel);
		strcpy(list[i].isel, list[i].sel);
		list[i].lsel[0] = '\000';
		disptechs_set_sel(0, i, list[i].lsel, list[i].isel, usels, &k, asels);
	}

	/* Set selectors from secondary */
	for (i = 0; list[i].dtech != disptech_end; i++)
		disptechs_set_sel(1, i, list[i].lsel, list[i].isel, usels, &k, asels);

	/* Set remainder from fallback */
	for (i = 0; list[i].dtech != disptech_end; i++)
		disptechs_set_sel(2, i, list[i].lsel, list[i].isel, usels, &k, asels);

	return list;
}

/* Locate the display list item that matches the given selector. */
/* Return NULL if not found */
disptech_info *disptech_select(disptech_info *list, char c) {
	int i;

	for (i = 0; list[i].dtech != disptech_end; i++) {
		if (c == list[i].lsel[0])
			return &list[i];
	}

	return NULL;
}


/* ------------------------------------------- */

/*
	Display settling time model. This is primarily tailored
	to phosphor type response (ie. CRT or Plasma), but LCD
	should be somewhat similar

	Outline:

	Use sRGB as the device model.

	For the target RGB, compute the partial derivative of
	delta E with respect to R, G & B, and then multiply
	that by desired dE accuracy to get the target R, G & B
	delta from the target, and then put that into 
	the exponential rise/fall model to compute settling time.
	Choose the worst of the 3.

	Should really change the code to compute partial derivative
	directly, to speed code up.

	We assume the phosphor stimulus is the proportional to the
	light output required (ie. that the CRT/encoding non-linearity
	is in the electron gun, not the electron->phoshor->light mechanism. )
*/

/* Convert gamma encoded rgb to linear light rgb */
static void rgb2rgbl(double *rgbl, double *rgb) {
	int i;
	for (i = 0; i < 3; i++) {
		if (rgb[i] < 0.04045)
			rgbl[i] = rgb[i]/12.92;
		else
			rgbl[i] = pow((rgb[i] + 0.055)/1.055, 2.4);
	}
}

/* Convert linear light rgb to L*a*b* */
static void rgbl2lab(double *lab, double *rgbl) {
	int i;
	double xyz[3];
	static double mat[3][3] = { 
		{ 0.412391, 0.212639, 0.019331 },	/* Red */
		{ 0.357584, 0.715169, 0.119195 },	/* Green */
		{ 0.180481, 0.072192, 0.950532 }	/* Blue */
	};

	icmMulBy3x3(xyz, mat, rgbl);
	icmXYZ2Lab(&icmD65, lab, xyz);
}

#ifdef NEVER
/* Convert linear light rgb to L*a*b* with partial derivatives */
static void ddrgbl2lab(double *lab, double dout[3][3], double *rgbl) {
	int i, j, k;
	double xyz[3];
	static double mat[9] = { 
		0.412391, 0.212639, 0.019331,	/* Red */
		0.357584, 0.715169, 0.119195,	/* Green */
		0.180481, 0.072192, 0.950532	/* Blue */
	};
    double dxyzlab[3][3];	/* Part. Deriv of [xyz] with respect to [rgb] */
	double dlabxyz[3][3];	/* Part. Deriv of [lab] from [xyz] */

	icxdpdiiMulBy3x3Parm(xyz, dxyzlab, mat, rgbl);
	icxdXYZ2Lab(&icmD65, lab, dlabxyz, xyz);

	/* Compute the partial derivative of Lab from rgb */
	for (k = 0; k < 3; k++) {
		for (j = 0; j < 3; j++) {
			dout[k][j] = 0.0;
			for (i = 0; i < 3; i++) {
				dout[k][j] += dlabxyz[k][i] * dxyzlab[i][j];
			}
		}
	}
}
#endif /* NEVER */

/* Convert linear light rgb to L*a*b* delta E partial derivatives */
static void drgbl2lab(/* double *lab, */double deout[3], double *rgbl) {
	int i, j, k;
	static double mat[3][3] = { 
		{ 0.412391, 0.212639, 0.019331 },	/* Red */
		{ 0.357584, 0.715169, 0.119195 },	/* Green */
		{ 0.180481, 0.072192, 0.950532 }	/* Blue */
	};
	double xyz[3];
	double wp[3], tin[3], dtin[3];
	double dlabxyz[3][3];	/* Part. Deriv of [lab] from [xyz] */
	double dout[3][3];		/* Part. Deriv of [lab] from [rgb] */

	/* rgb to XYZ */
	for (i = 0; i < 3; i++) {
		xyz[i] = 0.0;
		for (j = 0; j < 3; j++) {
			xyz[i] += mat[i][j] * rgbl[j];
		}
	}

	/* XYZ to perceptual Lab with partial derivatives. */
	wp[0] = icmD65.X, wp[1] = icmD65.Y, wp[2] = icmD65.Z;

	for (i = 0; i < 3; i++) {
		tin[i] = xyz[i]/wp[i];
		dtin[i] = 1.0/wp[i];

		if (tin[i] > 0.008856451586) {
			dtin[i] *= pow(tin[i], -2.0/3.0) / 3.0;
			tin[i] = pow(tin[i],1.0/3.0);
		} else {
			dtin[i] *= 7.787036979;
			tin[i] = 7.787036979 * tin[i] + 16.0/116.0;
		}
	}

/*	lab[0] = 116.0 * tin[1] - 16.0; */
	dlabxyz[0][0] = 0.0;
	dlabxyz[0][1] = 116.0 * dtin[1];
	dlabxyz[0][2] = 0.0;

/*	lab[1] = 500.0 * (tin[0] - tin[1]); */
	dlabxyz[1][0] = 500.0 * dtin[0];
	dlabxyz[1][1] = 500.0 * -dtin[1];
	dlabxyz[1][2] = 0.0;

/*	lab[2] = 200.0 * (tin[1] - tin[2]); */
	dlabxyz[2][0] = 0.0 * mat[0][1];
	dlabxyz[2][1] = 200.0 * dtin[1];
	dlabxyz[2][2] = 200.0 * -dtin[2];

	/* Compute the partial derivative of delta E from rgb */
	for (j = 0; j < 3; j++) {
		deout[j] = 0.0;
		for (k = 0; k < 3; k++) {
			dout[k][j] = 0.0;
			for (i = 0; i < 3; i++) {
				dout[k][j] += dlabxyz[k][i] * mat[i][j];
			}
			deout[j] += dout[k][j] * dout[k][j];
		}
		deout[j] = sqrt(deout[j]);
	}
}

double disp_settle_time(double *orgb, double *nrgb, double rise, double fall, double dE) {
	int i, j;
	double orgbl[3], nrgbl[3];		/* Linear light RGB */
	double drgb[3];		/* Partial derivative of RGB wrt to dE at new rgb */
	double argbl[3];	/* Acceptable RGB */
	double stime[3];	/* Settling time */
	double kr, kf;
	double xtime = 0.0;
	
	/* Convert rgb's to linear light rgb */
	rgb2rgbl(orgbl, orgb);
	rgb2rgbl(nrgbl, nrgb);

//printf("orgb = %f %f %f\n", orgb[0], orgb[1], orgb[2]);
//printf("nrgb = %f %f %f\n", nrgb[0], nrgb[1], nrgb[2]);
//printf("orgbl = %f %f %f\n", orgbl[0], orgbl[1], orgbl[2]);
//printf("nrgbl = %f %f %f\n", nrgbl[0], nrgbl[1], nrgbl[2]);
//printf("dE = %f\n", dE);

	/* Compute partial derivative */
	drgbl2lab(drgb, nrgbl);
//printf("drgb = %f %f %f\n", drgb[0], drgb[1], drgb[2]);

#ifdef NEVER
	/* Calculate partial derivative explicitely to check */
	{
		double rlab[3], lab[3];
		double xdrgb[3];		/* Partial derivative of RGB wrt to dE at new rgb */

		/* Reference Lab */
		rgbl2lab(rlab, nrgbl);
//printf("rlab = %f %f %f\n", rlab[0], rlab[1], rlab[2]);

		for (j = 0; j < 3; j++) {
			double del;
	
			if (nrgbl[j] > 0.5)
				del = -1e-6;
			else
				del = 1e-6;
	
			nrgbl[j] += del;
			rgbl2lab(lab, nrgbl);
			nrgbl[j] -= del;
//printf("check pde of in %d = lab %f, %f, %f\n",j, (lab[0] - rlab[0])/del, (lab[1] - rlab[1])/del, (lab[2] - rlab[2])/del);
			xdrgb[j] = icmLabDE(rlab, lab)/fabs(del);
		}
printf("chk drgb = %f %f %f\n", xdrgb[0], xdrgb[1], xdrgb[2]);
	}
#endif /* NEVER */

	/* Compute rgb value that would give targ delta E */
	for (j = 0; j < 3; j++) {
		double del;

		del = dE/drgb[j];

		if (orgbl[j] < nrgbl[j]) {
			argbl[j] = nrgbl[j] - del;
			if (argbl[j] < orgbl[j])
				argbl[j] = orgbl[j];
		} else {
			argbl[j] = nrgbl[j] + del;
			if (argbl[j] > orgbl[j])
				argbl[j] = orgbl[j];
		}
	}

//{ double rlab[3], lab[3];
//rgbl2lab(lab, argbl);
//rgbl2lab(rlab, nrgbl);
//printf("argbl = %f %f %f\n", argbl[0], argbl[1], argbl[2]);
//printf("dE for argbl = %f\n",icmLabDE(rlab, lab));
//}

	/* Compute the modelled time from orgbl to argbl */
	kr = rise/log(1.0 - 0.9);	/* Exponent constant for 90% change*/
	kf = fall/log(1.0 - 0.9);	/* Exponent constant for 90% change*/

	for (j = 0; j < 3; j++) {
		double el, dl, n, t;

		dl = (argbl[j] - orgbl[j])/(nrgbl[j] - orgbl[j]);

		if (fabs(dl) < 1e-6) {
			stime[j] = 0.0;
			continue;
		}

		if (nrgbl[j] > orgbl[j])
			stime[j] = kr * log(1.0 - dl);
		else
			stime[j] = kf * log(1.0 - dl);

		if (stime[j] > xtime && stime[j] < 5.0)
			xtime = stime[j];
	}
//printf("stime = %f %f %f\n", stime[0], stime[1], stime[2]);
//printf("returning = %f\n",xtime);

	return xtime;
}





