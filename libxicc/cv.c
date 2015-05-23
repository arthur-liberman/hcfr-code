
/**********************************************************/
/* Investigate Graphics GEMS transfer curve function parameters */
/**********************************************************/

/* Author: Graeme Gill
 * Date:   2003/12/1
 *
 * Copyright 2003 Graeme W. Gill
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <numlib.h>
#include "plot.h"
#include "ui.h"

void usage(void);

#define XRES 100		/* Plot resolution */

/* Per transfer function */
static double tfunc(
double *v,			/* Pointer to first parameter */
int    luord,		/* Curve order n..MPP_MXTCORD */
double vv			/* Source of value */
) {
	double g;
	int ord;

	if (vv < 0.0)
		vv = 0.0;
	else if (vv > 1.0)
		vv = 1.0;

	/* Process all the shaper orders from high to low. */
	/* [These shapers were inspired by a Graphics Gem idea */
	/* (Gems IV, VI.3, "Fast Alternatives to Perlin's Bias and */
	/*  Gain Functions, pp 401). */
	/*  They have the nice properties that they are smooth, and */
	/*  can't be non-monotonic. The control parameter has been */
	/*  altered to have a range from -oo to +oo rather than 0.0 to 1.0 */
	/*  so that the search space is less non-linear. ] */
	for (ord = luord-1; ord >= 0; ord--) {
		int nsec;			/* Number of sections */
		double sec;			/* Section */

		g = v[ord];			/* Parameter */

//		nsec = 1 << ord;	/* Double sections for each order */
		nsec = ord + 1;		/* Sections for each order */

		vv *= (double)nsec;

		sec = floor(vv);
		if (((int)sec) & 1)
			g = -g;				/* Alternate action in each section */
		vv -= sec;
		if (g >= 0.0) {
			vv = vv/(g - g * vv + 1.0);
		} else {
			vv = (vv - g * vv)/(1.0 - g * vv);
		}
		vv += sec;
		vv /= (double)nsec;
	}

	return vv;
}


#define MAX_PARM 10

int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	int np = 0;			/* Current number of input parameters */
	double params[MAX_PARM] =  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int i;
	double x;
	double xx[XRES];
	double y1[XRES];

	error_program = "cv";

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (np >= MAX_PARM)
			break;

		params[np++] = atof(argv[fa]);
	}

	if (np == 0)
		np = 1;

	printf("There are %d parameters:\n",np); fflush(stdout);
	for (i = 0; i < np; i++) {
		printf("Paramter %d = %f\n",i, params[i]); fflush(stdout);
	}

	/* Display the result */
	for (i = 0; i < XRES; i++) {
		x = i/(double)(XRES-1);

		xx[i] = x;
		y1[i] = tfunc(params, np, x);

		if (y1[i] < -0.2)
			y1[i] = -0.2;
		else if (y1[i] > 1.2)
			y1[i] = 1.2;
	}
	do_plot(xx,y1,NULL,NULL,XRES);

	return 0;
}

/******************************************************************/
/* Error/debug output routines */
/******************************************************************/

void
usage(void) {
	puts("usage: cv param0 [param1] [param2] ... ");
	exit(1);
}





