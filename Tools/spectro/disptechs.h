
#ifndef DISPTYPES_H

 /* Standardized display types for use with libinst */

/* 
 * Argyll Color Correction System
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

#ifdef __cplusplus
	extern "C" {
#endif

/* See <http://www.tftcentral.co.uk/> for some numbers on response times */

/* These are intended to be conservative numbers for 90% transition times */
/* The settling model will compute the time necessary for a 0.1 dE error */
/* from this using an exponenial decay model. */ 

#define DISPTECH_WORST_RISE 0.100
#define DISPTECH_WORST_FALL 0.250

#define DISPTECH_CRT_RISE 0.040
#define DISPTECH_CRT_FALL 0.250

#define DISPTECH_LCD_RISE 0.100
#define DISPTECH_LCD_FALL 0.050

#define DISPTECH_LED_RISE 0.001
#define DISPTECH_LED_FALL 0.001

#define DISPTECH_DLP_RISE 0.002
#define DISPTECH_DLP_FALL 0.002

/* Type of display technology */
typedef enum {

	disptech_unknown            = 0x0000,		/* Unknown dislay type */

	disptech_none               = 0x0001,		/* display type is inaplicable (ie. ambient) */

	disptech_crt                = 0x1000,		/* Generic CRT */

	disptech_plasma             = 0x2000,		/* Generic Plasma */

	disptech_lcd                = 0x3000,		/* Generic LCD */

	disptech_lcd_ccfl           = 0x3100,		/* LCD with CCFL */
	disptech_lcd_ccfl_ips       = 0x3110,		/* IPS LCD with CCFL */
	disptech_lcd_ccfl_vpa       = 0x3120,		/* VPA LCD with CCFL */
	disptech_lcd_ccfl_tft       = 0x3130,		/* TFT LCD with CCFL */

	disptech_lcd_ccfl_wg        = 0x3200,		/* Wide gamut LCD with CCFL */
	disptech_lcd_ccfl_wg_ips    = 0x3210,		/* IPS wide gamut LCD with CCFL */
	disptech_lcd_ccfl_wg_vpa    = 0x3220,		/* VPA wide gamut LCD with CCFL */
	disptech_lcd_ccfl_wg_tft    = 0x3230,		/* TFT wide gamut LCD with CCFL */
	
	disptech_lcd_wled           = 0x3300,		/* LCD with white LED */
	disptech_lcd_wled_ips       = 0x3310,		/* IPS LCD with white LED */
	disptech_lcd_wled_vpa       = 0x3320,		/* VPA LCD with white LED */
	disptech_lcd_wled_tft       = 0x3330,		/* TFT LCD with white LED */

	disptech_lcd_rgbled         = 0x3400,		/* LCD with RGB LED */
	disptech_lcd_rgbled_ips     = 0x3410,		/* IPS LCD with RGB LED */
	disptech_lcd_rgbled_vpa     = 0x3420,		/* VPA LCD with RGB LED */
	disptech_lcd_rgbled_tft     = 0x3430,		/* TFT LCD with RGB LED */

	disptech_lcd_rgledp         = 0x3500,		/* IPS LCD with RG LED + Phosphor */
	disptech_lcd_rgledp_ips     = 0x3510,		/* IPS LCD with RG LED + Phosphor */
	disptech_lcd_rgledp_vpa     = 0x3520,		/* VPA LCD with RG LED + Phosphor */
	disptech_lcd_rgledp_tft     = 0x3530,		/* TFT LCD with RG LED + Phosphor */

	disptech_oled               = 0x4000,		/* Organic LED */
	disptech_amoled             = 0x4010,		/* Active Matrix Organic LED */

	disptech_dlp                = 0x5000,		/* Generic Digital Light Processing projector */
	disptech_dlp_rgb            = 0x5010,		/* DLP projector with RGB filter */
	disptech_dlp_rgbw           = 0x5020,		/* DLP projector with RGBW filter */
	disptech_dlp_rgbcmy         = 0x5030,		/* DLP projector with RGBCMY filter */

	disptech_end                = 0xffffffff	/* List end marker */
	
} disptech;

#ifdef __cplusplus
	}
#endif

/* Information defined by instrument type */
struct _disptech_info {

	disptech dtech;			/* Enumeration */

	char *strid;			/* String ID */
	char *desc;				/* Desciption and identification string */

	int  refr;				/* Refresh mode flag */

	double rise_time;		/* rise time to 90% in seconds */
	double fall_time;		/* fall time to 90% in seconds */

	char *sel;				/* Default command line selector (may be NULL) */

  /* Private: */

	char lsel[10];			/* Unique list selector for ui */

}; typedef struct _disptech_info disptech_info;


/* Given the enum id, return the matching disptech_info entry */
/* Return the disptech_unknown entry if not matched */
disptech_info *disptech_get_id(disptech id);

/* Given the string id, return the matching disptech_info entry */
/* Return the disptech_unknown entry if not matched */
disptech_info *disptech_get_strid(char *strid);

/* Return the display tech list with unique lsel lectors */
disptech_info *disptech_get_list();

/* Locate the display list item that matches the given selector. */
/* Return NULL if not found */
disptech_info *disptech_select(disptech_info *list, char c);

/* - - - - - - - - - - - */

/* utility function, used by disptech_get_list & inst_creat_disptype_list()  */
int disptechs_set_sel(int flag, char *sel, char *usels, int *k, char *asels);

/* - - - - - - - - - - */
/* Display settling time model */

double disp_settle_time(double *orgb, double *nrgb, double rise, double fall, double dE);


#define DISPTYPES_H
#endif /* DISPTYPES_H */

