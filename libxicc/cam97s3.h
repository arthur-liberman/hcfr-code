
/* 
 * cam97s3
 *
 * Color Appearance Model, based on
 * CIECAM97, "Revision for Practical Applications"
 * by Mark D. Fairchild, with the addition of the Viewing Flare
 * model described on page 487 of "Digital Color Management",
 * by Edward Giorgianni and Thomas Madden, and the
 * Helmholtz-Kohlraush effect, using the equation
 * the Bradford-Hunt 96C model as detailed in Mark Fairchilds
 * book "Color Appearance Models". 
 *
 * Author:  Graeme W. Gill
 * Date:    5/10/00
 * Version: 1.20
 *
 * Copyright 2000 Graeme W. Gill
 * Please refer to COPYRIGHT file for details.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* Algorithm tweaks */
#define CIECAM97S3_SPLINE_E			/* Use spline interpolation for eccentricity factor */

/* Definitions assumed here:

  Stimulus field is the 2 degrees field of view of the sample
	being characterised.

  Viewing/Scene/Image field is the area of the whole image or
	display surface that the stimulus is part of.

  Background field is 10-12 degree field surrounding the stimulus field.
	This may be within, overlap or encompass the Viewing/Scene/Image field.

  Surround/Adapting field is the visual field minus the background field.

  Visual field is the 130 degree angular field that is seen by the eyes.

  Illuminating field is the field that illuminates the reflective
  Scene/Image. It may be the same as the Ambient field.

  Ambient field is the whole surrounding environmental light field.

  NOTE: In "Digital Color Management", Giorgianni and Madden use the term
  "Surround" to mean the same thing as "Background" in the CIECAM97 terminology.
  The ICC standard doesn't define what it means by "Surround illumination".

*/

/* Rules of Thumb: */
/* Ambient Luminance (Lat, cd/m^2) is Ambient Illuminance (Lux) divided by PI. */
/* i.e. Lat = Iat/PI */	/* (1 foot candle = 0.0929 lux) */

/* The Adapting/Surround Luminance is often taken to be */
/* the 20% of the Ambient Luminance. (gray world) */
/* i.e. La = Lat/5 = Iat/15.7 */

/* For a reflective print, the Viewing/Scene/Image luminance (Lv, cd/m^2), */
/* will be the Illuminating Luminance (Li, cd/m^2) reflected by the */
/* media white point (Yw) */

/* If there is no special illumination for a reflective print, */
/* then the Illuminating Luminance (Li) will be the Ambient Luminance (Lat) */

/* An emisive display will have an independently determined Lv. */

/* The classification of the type of surround is */
/* determined by comparing the Adapting/Surround luminance (La, cd/m^2) */
/* with the average luminance of the Viewing/Scene/Image field (Lv, cd/m^2) */

/* La/Lv == 0%,   dark */
/* La/Lv 0 - 20%, dim */
/* La/Lv > 20%,   average */
/* special,       cut sheet */

/* The Background relative luminance Yb is typically assumed to */
/* be 0.18 .. 0.2, and is assumed to be grey. */

/* The source of flare light depends on the type of display system. */
/* For a CRT, it will be the Ambient light reflecting off the glass surface. */
/* (This implies Yf = Lat * reflectance/Lv) */
/* For a reflection print, it will be the Illuminant reflecting from the media */
/* surface. (Yf = Li * reflectance) */
/* For a projected image, it will be stray projector light, scattered by the */
/* surround, screen and air particles. (Yf = Li * reflectance_and_scattering) */

/*

  Typical Ambient Illuminance brightness
  (Lux)   La  Condition 
    11     1  Twilight
    32     2  Subdued indoor lighting
    64     4  Less than typical office light; sometimes recommended for
              display-only workplaces (sRGB)
   350    22  Typical Office (sRGB annex D)
   500    32  Practical print evaluationa (ISO-3664 P2)
  1000    64  Good Print evaluation (CIE 116-1995)
  1000    64  Overcast Outdoors
  2000   127  Critical print evaluation (ISO-3664 P1)
 10000   637  Typical outdoors, full daylight 
 50000  3185  Bright summers day 

*/

/* ---------------------------------- */
struct _cam97s3 {
/* Public: */
	void (*del)(struct _cam97s3 *s);	/* We're done with it */

	int (*set_view)(
		struct _cam97s3 *s,
		ViewingCondition Ev,	/* Enumerated Viewing Condition */
		double Wxyz[3],	/* Reference/Adapted White XYZ (Y scale 1.0) */
		double La,		/* Adapting/Surround Luminance cd/m^2 */
		double Yb,		/* Luminance of Background relative to reference white (range 0.0 .. 1.0) */
		double Lv,		/* Luminance of white in the Viewing/Scene/Image field (cd/m^2) */
						/* Ignored if Ev is set */
		double Yf,		/* Flare as a fraction of the reference white (range 0.0 .. 1.0) */
		double Fxyz[3],	/* The Flare white coordinates (typically the Ambient color) */
		int hk			/* Flag, NZ to use Helmholtz-Kohlraush effect */
	);

	/* Conversions */
	int (*XYZ_to_cam)(struct _cam97s3 *s, double *out, double *in);
	int (*cam_to_XYZ)(struct _cam97s3 *s, double *out, double *in);

/* Private: */
	/* Scene parameters */
	ViewingCondition Ev;	/* Enumerated Viewing Condition */
	double Wxyz[3];	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
	double Yb;		/* Relative Luminance of Background to reference white (Y range 0.0 .. 1.0) */
	double La;		/* Adapting/Surround Luminance cd/m^2 */
	double Yf;		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
	double Fxyz[3];	/* The Flare white coordinates (typically the Ambient color) */

	/* Internal parameters */
	double  C;		/* Surround Impact */
	double Nc;		/* Chromatic Induction */
	double  F;		/* Adaptation Degree */

	/* Pre-computed values */
	double Fsc;			/* Flare scale */
	double Fisc;		/* Inverse flare scale */
	double Fsxyz[3];	/* Scaled Flare color coordinates */
	double rgbW[3];		/* Sharpened cone response white values */
	double D;			/* Degree of chromatic adaption */
	double rgbcW[3];	/* Chromatically transformed white value */
	double rgbpW[3];	/* Hunt-Pointer-Estevez cone response space white */
	double n;			/* Background induction factor */
	double nn;			/* Precomuted function of n */
	double Fl;			/* Lightness contrast factor ?? */
	double Nbb;			/* Background brightness induction factors */
	double Ncb;			/* Chromatic brightness induction factors */
	double z;			/* Base exponential nonlinearity */
	double rgbaW[3];	/* Post-adapted cone response of white */
	double Aw;			/* Achromatic response of white */

	/* Flags */
	int hk;				/* If NZ, use Helmholtz-Kohlraush effect */
	int trace;			/* Trace values through computation */
}; typedef struct _cam97s3 cam97s3;


/* Create a cam97s3 conversion class, with default viewing conditions */
cam97s3 *new_cam97s3(void);

