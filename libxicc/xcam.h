
#ifndef _XCAM_H_

/* 
 * Abstract interface to color appearance model transforms.
 * 
 * This is to allow the rest of Argyll to use a default CAM.
 * 
 * Author:  Graeme W. Gill
 * Date:    25/7/2004
 * Version: 1.00
 *
 * Copyright 2004 Graeme W. Gill
 * Please refer to COPYRIGHT file for details.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* ---------------------------------- */

/* The range of CAMs supported */
typedef enum {
	cam_default    = 0,		/* Default CAM */
	cam_CIECAM97s3 = 1,		/* CIECAM97, version 3 */
	cam_CIECAM02   = 2		/* CIECAM02 */
} icxCAM;

/* The enumerated viewing conditions: */
typedef enum {
	vc_notset    = -1,
	vc_none      = 0,	/* Figure out from Lv and La */
	vc_dark      = 1,
	vc_dim       = 2,
	vc_average   = 3,
	vc_cut_sheet = 4	/* Transparencies on a Light Box */
} ViewingCondition;

struct _icxcam {
/* Public: */
	void (*del)(struct _icxcam *s);	/* We're done with it */

	/* Always returns 0 */
	int (*set_view)(
		struct _icxcam *s,
		ViewingCondition Ev,	/* Enumerated Viewing Condition */
		double Wxyz[3],	/* Reference/Adapted White XYZ (Y scale 1.0) */
		double La,		/* Adapting/Surround Luminance cd/m^2 */
		double Yb,		/* Luminance of Background relative to reference white (range 0.0 .. 1.0) */
		double Lv,		/* Luminance of white in the Viewing/Scene/Image field (cd/m^2) */
						/* Ignored if Ev is set */
		double Yf,		/* Flare as a fraction of the reference white (range 0.0 .. 1.0) */
		double Yg,		/* Glare as a fraction of the adapting/surround (range 0.0 .. 1.0) */
		double Gxyz[3],	/* The Glare white coordinates (typically the Ambient color) */
		int hk			/* Flag, NZ to use Helmholtz-Kohlraush effect */
	);

	/* Conversions */
	int (*XYZ_to_cam)(struct _icxcam *s, double *out, double *in);
	int (*cam_to_XYZ)(struct _icxcam *s, double *out, double *in);

	/* Debug */
	void (*settrace)(struct _icxcam *s, int tracev);

/* Private: */
	icxCAM tag;			/* Type */
	void *p;			/* Pointer to implementation */
	double Wxyz[3];		/* Copy of Wxyz */

}; typedef struct _icxcam icxcam;

/* Create a new CAM conversion object */
icxcam *new_icxcam(icxCAM which);

/* Return the default CAM */
icxCAM icxcam_default();	

/* Return a string describing the given CAM */
char *icxcam_description(icxCAM ct); 

#define _XCAM_H_
#endif /* _XCAM_H_ */


