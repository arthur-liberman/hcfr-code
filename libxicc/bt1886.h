#ifndef BT1886_H
#define BT1886_H

/* 
 * Author:  Graeme W. Gill
 * Date:    16/8/13
 * Version: 1.00
 *
 * Copyright 2013, 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUB LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/* BT.1886 stype input offset transfer curve, */
/* + general gamma + input + output offset curve support. */


typedef struct {
	icmXYZNumber w;				/* White point for Lab conversion */
	double ingo;				/* input Y offset for bt1886 */
	double outsc;				/* output Y scale for bt1886 */
	double outo;				/* output Y offset */
	double outL;				/* output black point L value */
	double tab[2];				/* Target ab offset value at zero input for bt1886 */
	double gamma;				/* bt.1886 technical gamma to apply */
} bt1886_info;

/* Set the bt1886_info to a default do nothing state */
void bt1886_setnop(bt1886_info *p);

/* Setup the bt1886_info for the given target */
void bt1886_setup(
bt1886_info *p,
icmXYZNumber *w,	/* wp used for L*a*b* conversion */
double *XYZbp,		/* normalised bp used for black offset and black point hue "bend" */
double outoprop,	/* 0..1 proportion of output black point compensation */
double gamma,		/* technical or effective gamma */
int effg			/* nz if effective gamma, z if technical gamma */
);

/* Apply BT.1886 eotf curve to the device RGB value */
/* to produce a linear light RGB. We pass xvYCC out of range values through. */
void bt1886_fwd_curve(bt1886_info *p, double *out, double *in);

/* Apply BT.1886 processing black point hue adjustment to the XYZ value */
void bt1886_wp_adjust(bt1886_info *p, double *out, double *in);


/* Apply inverse BT.1886 eotf curve to the device RGB value */
/* to produce a linear light RGB. We pass xvYCC out of range values through. */
void bt1886_bwd_curve(bt1886_info *p, double *out, double *in);

/* Apply inverse BT.1886 processing black point hue adjustment to the XYZ value */
void bt1886_inv_wp_adjust(bt1886_info *p, double *out, double *in);

#endif /* BT1886_H */



































