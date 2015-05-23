
/* 
 * cam02
 *
 * Color Appearance Model, based on
 * CIECAM02, "The CIECAM02 Color Appearance Model"
 * by Nathan Moroney, Mark D. Fairchild, Robert W.G. Hunt, Changjun Li,
 * M. Ronnier Luo and Todd Newman, IS&T/SID Tenth Color Imaging
 * Conference, with the addition of a Viewing Flare+Glare
 * model, and the Helmholtz-Kohlraush effect, using the equation
 * the Bradford-Hunt 96C model as detailed in Mark Fairchilds
 * book "Color Appearance Models". 
 *
 * Author:  Graeme W. Gill
 * Date:    17/1/2004
 * Version: 1.00
 *
 * This file is based on cam97s3.h by Graeme Gill.
 *
 * Copyright 2004 - 2014 Graeme W. Gill
 * Please refer to COPYRIGHT file for details.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* Definitions assumed here:

  Stimulus field is the 2 degrees field of view of the sample
	being characterised.

  Viewing/Scene/Image field is the area of the whole image or
	display surface that the stimulus is part of.

  Background field is 10-12 degree field immediately surrounding the stimulus field.
	This may be within, overlap or encompass the Viewing/Scene/Image field.

  Visual field is the 130 degree angular field that is seen by the eyes.

  Surround/Adapting field is the visual field minus the background field,
  and is what is assumed to be setting the viewers light adaptation level.

  Ambient field is general illumination from the sun or general lighting,
  and is the whole surrounding environmental light field.

  Illuminating field is the field that illuminates the reflective
  Scene/Image. It may be the same as the Ambient field or it could
  be a specific light source that is directed to the viewing scene..

  NOTE: In "Digital Color Management", Giorgianni and Madden use the term
  "Surround" to mean the same thing as "Background" in the CIECAM02 terminology.
  The ICC standard doesn't define what it means by "Surround illumination".

*/

/* Rules of Thumb: */

/* Ambient Luminance (Lamb, cd/m^2) is Ambient Illuminance (Eamb, Lux) divided by PI. */
/* i.e. Lamb = Eamb/PI */	/* (1 foot candle = 0.0929 lux) */

/* Illuminating field Luminance (Li, cd/m^2) is the Illuminating field Illuminance (Ei, Lux) */
/* divide by PI. i.e. Li = Ei/PI */

/* The Adapting/Surround Luminance is La often taken to be */
/* the 20% of the Ambient Luminance (gray world, 50% perceptual) */
/* i.e. La = Lamb/5 = Eamb/15.7 */
/* If the Illuminating field covers the Adapting/surround, the it will be 20% of the */
/* Illuminating field. */

/* For a reflective print, the Viewing/Scene/Image luminance (Lv, cd/m^2), */
/* will be the Illuminating Luminance (Li, cd/m^2) or the Ambient Luminance (Lamb, cd/m^2) */
/* reflected by the media white point (Yw) */

/* If there is no special illumination for a reflective print, */
/* then the Illuminating Luminance (Li) will be the Ambient Luminance (Lamb) */

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

/* Flare is assumed to be stray light from light parts of the */
/* image illuminating dark parts of the image, and is display technology dependent. */
/* In theory reflective systems have little Flare ? */

/* Glare is assumed to be stray ambient light reflecting from the display */
/* surface, dust, or entering the observers eye directly, and as a result */
/* fogging the dark parts of the image. */
/* This is typically the major source of veiling light ? */

/* By separatedly specifiying these two, the effect can be automatically */
/* scalled according to the ambient light level, modelling the effect of */
/* reduced glare in a darkened viewing environment. */

/*

Typical adapting field luminances and white luminance in reflective setup:

E = illuminance in Lux
Lv = White luminance assuming 100% reflectance
La = Adapting field luminance in cd/m^2, assuming 20% reflectance from surround
    
    E     La     Lv   Condition 
    11     0.7    4   Twilight
    32     2     10   Subdued indoor lighting
    64     4     20   Less than typical office light; sometimes recommended for
                      display-only workplaces (sRGB)
   350    22    111   Typical Office (sRGB annex D)
   500    32    160   Practical print evaluationa (ISO-3664 P2)
  1000    64    318   Good Print evaluation (CIE 116-1995)
  1000    64    318   Television Studio lighting
  1000    64    318   Overcast Outdoors
  2000   127    637   Critical print evaluation (ISO-3664 P1)
 10000   637   3183   Typical outdoors, full daylight 
 50000  3185  15915   Bright summers day 

*/

/* ---------------------------------- */

struct _cam02 {
/* Public: */
	void (*del)(struct _cam02 *s);	/* We're done with it */

	int (*set_view)(
		struct _cam02 *s,
		ViewingCondition Ev,	/* Enumerated Viewing Condition */
		double Wxyz[3],	/* Reference/Adapted White XYZ (Y scale 1.0) */
		double La,		/* Adapting/Surround Luminance cd/m^2 */
		double Yb,		/* Luminance of Background relative to reference white (range 0.0 .. 1.0) */
		double Lv,		/* Luminance of white in the Viewing/Scene/Image field (cd/m^2) */
						/* Ignored if Ev is set */
		double Yf,		/* Flare as a fraction of the reference white (range 0.0 .. 1.0) */
		double Yg,		/* Glare as a fraction of the adapting/surround (range 0.0 .. 1.0) */
		double Gxyz[3],	/* The Glare white coordinates (ie. the Ambient color) */
						/* If <= 0 will Wxyz will be used. */
		int hk			/* Flag, NZ to use Helmholtz-Kohlraush effect */
	);

	/* Conversions. Return nz on error */
	int (*XYZ_to_cam)(struct _cam02 *s, double *out, double *in);
	int (*cam_to_XYZ)(struct _cam02 *s, double *out, double *in);

/* Private: */
	/* Scene parameters */
	ViewingCondition Ev;	/* Enumerated Viewing Condition */
	double Lv;		/* Luminance of white in the Viewing/Image cd/m^2 */
	double La;		/* Adapting/Surround Luminance cd/m^2 */
	double Wxyz[3];	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
	double Yb;		/* Relative Luminance of Background to reference white (Y range 0.0 .. 1.0) */
	double Yf;		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
	double Yg;		/* Glare as a fraction of the adapting/surround (Y range 0.0 .. 1.0) */
	double Gxyz[3];	/* The Glare white coordinates (typically the Ambient color) */

	/* Internal parameters */
	double  C;		/* Surround Impact */
	double Nc;		/* Chromatic Induction */
	double  F;		/* Adaptation Degree */

	/* Pre-computed values */
	double cc[3][3];	/* Forward cone and chromatic transform */
	double icc[3][3];	/* Reverse cone and chromatic transform */
	double crange[3];	/* ENABLE_COMPR compression range */
	double Va[3], Vb[3], VttA[3], Vttd[3];	/* rgba vectors */
	double dcomp[3];	/* Vttd in terms of VttA, Va, Vb */
	double Fsc;			/* Flare+Glare scale */
	double Fisc;		/* Inverse Flare+Glare scale */
	double Fsxyz[3];	/* Scaled Flare+Glare color coordinates */
	double rgbW[3];		/* Sharpened cone response white values */
	double D;			/* Degree of chromatic adaption */
	double Drgb[3];		/* Chromatic transformation value */
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
	double nldxval;		/* Non-linearity output value at lower crossover to linear */
	double nldxslope;	/* Non-linearity slope at lower crossover to linear */
	double nluxval;		/* Non-linearity value at upper crossover to linear */
	double nluxslope;	/* Non-linearity slope at upper crossover to linear */
	double lA;			/* JLIMIT Limited A */

	/* Option flags, code not always enabled */
	int hk;				/* Use Helmholtz-Kohlraush effect */
	int trace;			/* Trace values through computation */
	int retss;			/* Return ss rather than Jab */
	int range;			/* (for cam02ref.h) return on range error */ 

	double nldlimit;	/* value of NLDLIMIT, sets non-linearity lower limit */
	double nldicept;	/* value of NLDLICEPT, sets straight line intercept with 0.1 output */
	double nlulimit;	/* value of NLULIMIT, sets non-linearity upper limit (tangent) */
	double ddllimit;	/* value of DDLLIMIT, sets fwd k3 to k2 limit  */
	double ddulimit;	/* value of DDULIMIT, sets bwd k3 to k1 limit */
	double ssmincj;		/* value of SSMINJ, sets cJ minimum value */
	double jlimit;		/* value of JLIMIT, sets cutover to straight line for J point */
	double hklimit;		/* value of HKLIMIT, sets HK factor upper limit */

}; typedef struct _cam02 cam02;


/* Create a cam02 conversion class, with default viewing conditions */
cam02 *new_cam02(void);

