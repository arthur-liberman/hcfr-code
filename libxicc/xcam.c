
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "xcam.h"
#include "cam97s3.h"
#include "cam02.h"

static void icx_cam_free(icxcam *s);
static int icx_set_view(icxcam *s, ViewingCondition Ev, double Wxyz[3],
                        double La, double Yb, double Lv, double Yf, double Yg, double Gxyz[3],
						int hk);
static int icx_XYZ_to_cam(struct _icxcam *s, double Jab[3], double XYZ[3]);
static int icx_cam_to_XYZ(struct _icxcam *s, double XYZ[3], double Jab[3]);
static void settrace(struct _icxcam *s, int tracev);

/* Return the default CAM */
icxCAM icxcam_default(void)	{
//	return cam_CIECAM97s3;
	return cam_CIECAM02;
}

/* Return a string describing the given CAM */
char *icxcam_description(icxCAM which) {
	if (which == cam_default)
		which = icxcam_default();

	switch(which) {
		case cam_CIECAM97s3:
			return "CIECAM97s3";
		case cam_CIECAM02:
			return "CIECAM02";
		default:
			break;
	}
	return "Unknown CAM";
}

/* Create a cam conversion object */
icxcam *new_icxcam(icxCAM which) {
	icxcam *s;

	if ((s = (icxcam *)calloc(1, sizeof(icxcam))) == NULL) {
		fprintf(stderr,"icxcam: malloc failed allocating object\n");
		return NULL;
	}
	
	/* Initialise methods */
	s->del        = icx_cam_free;
	s->set_view   = icx_set_view;
	s->XYZ_to_cam = icx_XYZ_to_cam;
	s->cam_to_XYZ = icx_cam_to_XYZ;
	s->settrace   = settrace;

	/* We set the default CAM here */
	if (which == cam_default)
		which = icxcam_default();

	s->tag = which;

	switch(which) {
		case cam_CIECAM97s3:
			if ((s->p = (void *)new_cam97s3()) == NULL) {
				fprintf(stderr,"icxcam: malloc failed allocating object\n");
				free(s);
				return NULL;
			}
			break;
		case cam_CIECAM02:
			if ((s->p = (void *)new_cam02()) == NULL) {
				fprintf(stderr,"icxcam: malloc failed allocating object\n");
				free(s);
				return NULL;
			}
			break;

		default:
			fprintf(stderr,"icxcam: unknown CAM type\n");
			free(s);
			return NULL;
	}
	return s;
}

static void icx_cam_free(icxcam *s) {
	if (s != NULL) {
		switch(s->tag) {
			case cam_CIECAM97s3: {
				cam97s3 *pp = (cam97s3 *)s->p;
				pp->del(pp);
				break;
			}
			case cam_CIECAM02: {
				cam02 *pp = (cam02 *)s->p;
				pp->del(pp);
				break;
			}
		default:
			break;
		}
		free(s);
	}
}

static int icx_set_view(
icxcam *s,
ViewingCondition Ev,	/* Enumerated Viewing Condition */
double Wxyz[3],	/* Reference/Adapted White XYZ (Y range 0.0 .. 1.0) */
double La,		/* Adapting/Surround Luminance cd/m^2 */
double Yb,		/* Relative Luminance of Background to reference white */
double Lv,		/* Luminance of white in the Viewing/Scene/Image field (cd/m^2) */
				/* Ignored if Ev is set to other than vc_none */
double Yf,		/* Flare as a fraction of the reference white (Y range 0.0 .. 1.0) */
double Yg,		/* Glare as a fraction of the adapting/surround (Y range 0.0 .. 1.0) */
double Gxyz[3],	/* The Glare white coordinates (typically the Ambient color) */
int hk			/* Flag, NZ to use Helmholtz-Kohlraush effect */
) {
	s->Wxyz[0] = Wxyz[0];
	s->Wxyz[1] = Wxyz[1];
	s->Wxyz[2] = Wxyz[2];

	switch(s->tag) {
		case cam_CIECAM97s3: {
			cam97s3 *pp = (cam97s3 *)s->p;
			return pp->set_view(pp, Ev, Wxyz, La, Yb, Lv, 0.2 * Yg, Gxyz, hk);
		}
		case cam_CIECAM02: {
			cam02 *pp = (cam02 *)s->p;
			return pp->set_view(pp, Ev, Wxyz, La, Yb, Lv, Yf, Yg, Gxyz, hk);
		}
		default:
			break;
	}
	return 0;
}

/* Conversions */
static int icx_XYZ_to_cam(
struct _icxcam *s,
double Jab[3],
double XYZ[3]
) {
	switch(s->tag) {
		case cam_CIECAM97s3: {
			cam97s3 *pp = (cam97s3 *)s->p;
			return pp->XYZ_to_cam(pp, Jab, XYZ);
		}
		case cam_CIECAM02: {
			cam02 *pp = (cam02 *)s->p;
			return pp->XYZ_to_cam(pp, Jab, XYZ);
		}
		default:
			break;
	}
	return 0;
}

static int icx_cam_to_XYZ(
struct _icxcam *s,
double XYZ[3],
double Jab[3]
) {
	switch(s->tag) {
		case cam_CIECAM97s3: {
			cam97s3 *pp = (cam97s3 *)s->p;
			return pp->cam_to_XYZ(pp, XYZ, Jab);
		}
		case cam_CIECAM02: {
			cam02 *pp = (cam02 *)s->p;
			return pp->cam_to_XYZ(pp, XYZ, Jab);
		}
		default:
			break;
	}
	return 0;
}

/* Debug */
static void settrace(
struct _icxcam *s,
int tracev
) {
	switch(s->tag) {
		case cam_CIECAM97s3: {
			cam97s3 *pp = (cam97s3 *)s->p;
			pp->trace = tracev;
		}
		case cam_CIECAM02: {
			cam02 *pp = (cam02 *)s->p;
			pp->trace = tracev;
		}
		default:
			break;
	}
}





