
/* 
 * Compute the intersection volume of two gamuts. 
 *
 * Author:  Graeme W. Gill
 * Date:    2008/1/7
 * Version: 1.00
 *
 * Copyright 2008 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/*
 * TTBD:
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "icc.h"
#include "numlib.h"
#include "gamut.h"

/* Compute a triangles area */
static double tri_area(
double v1[3],
double v2[3],
double v3[3]) {
	int i, j;
	double sp, ss[3];		/* Triangle side lengths */
	double area;			/* Area of this triangle */
	double *vv[3];			/* Pointers to vertexes */

	vv[0] = v1;
	vv[1] = v2;
	vv[2] = v3;
	/* Compute the full triangles area */
	for (i = 0; i < 3; i++) {	/* For each edge */
		for (ss[i] = 0.0, j = 0; j < 3; j++) {
			double dd = vv[i][j] - vv[(i+1) % 3][j];
			ss[i] += dd * dd;
		}
		ss[i] = sqrt(ss[i]);
	}
	sp = 0.5 * (ss[0] + ss[1] + ss[2]);		/* semi-perimeter */
	area = sqrt(fabs(sp * (sp - ss[0]) * (sp - ss[1]) * (sp - ss[2]))); /* Area of triangle */

	return area;
}

/* See if the given edge intersects a given triangle. */
/* Return 1 if it does, 0 if it doesn't */
static int edge_tri_isect(
gamut *s,		/* Gamut the triangle is in */
double *ip,		/* return intersection point */
gtri *t,		/* Triangle in question */
gedge *e		/* edge to test (may be from another gamut) */
) {
	double rv;			/* Axis parameter value */
	double gv[3];		/* Grey axis vector */
	double ival[3];		/* Intersection value */
	double den;
	int j;

	gv[0] = e->v[1]->p[0] - e->v[0]->p[0];
	gv[1] = e->v[1]->p[1] - e->v[0]->p[1];
	gv[2] = e->v[1]->p[2] - e->v[0]->p[2];

	den = t->pe[0] * gv[0] + t->pe[1] * gv[1] + t->pe[2] * gv[2];
	if (fabs(den) < 1e-10) {
		return 0;
	}

	/* Compute the intersection of the grey axis vector with the triangle plane */
	rv = -(t->pe[0] * e->v[0]->p[0]
	     + t->pe[1] * e->v[0]->p[1]
	     + t->pe[2] * e->v[0]->p[2]
	     + t->pe[3])/den;

	/* Compute the actual intersection point */
	ival[0] = e->v[0]->p[0] + rv * gv[0];
	ival[1] = e->v[0]->p[1] + rv * gv[1];
	ival[2] = e->v[0]->p[2] + rv * gv[2];

	/* Check if the intersection is on the edge */
	if (rv < 0.0 || rv > 1.0)
		return 0;

	/* Check if the intersection point is within the triangle */
	for (j = 0; j < 3; j++) {
		double ds;
		ds = t->ee[j][0] * (ival[0] - s->cent[0])	/* Convert to relative for edge check */
		   + t->ee[j][1] * (ival[1] - s->cent[1])
	       + t->ee[j][2] * (ival[2] - s->cent[2])
		   + t->ee[j][3];
		if (ds > 1e-8) {
			return 0;		/* Not within triangle */
		}
	}

	/* Got an intersection point */
	ip[0] = ival[0];
	ip[1] = ival[1];
	ip[2] = ival[2];

	return 1;
}


/* Return the total volume of the gamut */
/* Return -1.0 if incomaptible gamuts */
double isect_volume(
gamut *s1,
gamut *s2
) {
	int i, j, k;
	gtri *tp1, *tp2;		/* Triangle pointer */
	double vol;		/* Gamut volume */

	if (s1->compatible(s1, s2) == 0)
		return -1.0;

	if IS_LIST_EMPTY(s1->tris)
		s1->triangulate(s1);
	if IS_LIST_EMPTY(s2->tris)
		s2->triangulate(s2);

	vol = 0.0;

	/* For first gamut then second gamut */
	for (k = 0; k < 2; k++) {

		if (k == 1) {			/* Swap the two gamuts roles */
			gamut *st = s1;
			s1 = s2;
			s2 = st;
printf("~1 doing second gamut inside first\n");
		} else {
printf("~1 doing first gamut inside second\n");
		}

		/* Compute the area of each triangle in the list that is within, */
		/* the other gamut, and accumulate the gamut volume. */
		tp1 = s1->tris;
		FOR_ALL_ITEMS(gtri, tp1) {
			double sp, ss[3];		/* Triangle side lengths */
			double area;			/* Area of this triangle */
			double dp;				/* Dot product of point in triangle and normal */
			int inout[3];			/* 0 = inside, 1 = outside */
			int nout;				/* Number that are out */ 
			
printf("~1 doing triangle %d from %s gamut\n",tp1->n,k == 0 ? "first" : "second");
			/* See how many verticies in the triangle are contained within */
			/* the other gamut. */
			nout = 0;
			for (i = 0; i < 3; i++) {	/* For each vertex */
				double pl;
				pl = s2->nradial(s2, NULL, tp1->v[i]->p);

				/* We add a slight hysterysis to avoid issues */
				/* with identical triangles in two gamuts. */
				if ((k == 0 && pl > (1.0 + 1e-10))
				 || (k == 1 && pl > (1.0 - 1e-10))) {
					nout++;
					inout[i] = 1;
				} else
					inout[i] = 0;
			}

printf("~1 verticies outside = %d\n",nout);

			/* If none are in, skip this triangle */
			if (nout == 3)
				continue;

			/* Compute the full triangles area */
			area = tri_area(tp1->v[0]->p, tp1->v[1]->p, tp1->v[2]->p);
printf("~1 full triangle area = %f\n",area);

			/* If the triangle is not completely in, locate all the intersections */
			/* between it and triangles in the other gamut */
			if (nout != 0) {
				gvert *opv;			/* Pointer to the one "in" or "out" vertex */
				double parea = 0.0;	/* Total partial area */

				/* Locate the odd point out of the three */
				if (nout == 2) {		/* Look for the one "in" point */
					for (j = 0; j < 3; j++) {
						if (inout[j] == 0)
							break;
					}
				} else {				/* Look for the one "out" point */
					for (j = 0; j < 3; j++) {
						if (inout[j] == 1)
							break;
					}
				}
				opv = tp1->v[j];

				tp2 = s2->tris;
				FOR_ALL_ITEMS(gtri, tp2) {		/* Other gamut triangles */
					double isps[2][3];			/* Intersection npoints */
					int nisps;					/* Number of intersection points */
					double isp[3];				/* New intersection point */
					int kk;
		
					/* Do a min/max intersection elimination test */
					for (i = 0; i < 3; i++) {
						if (tp2->mix[1][i] < tp1->mix[0][i]
						 || tp2->mix[0][i] > tp1->mix[1][i])
							break;			/* min/max don't overlap */
					}
					if (i < 3)
						continue;			/* Skip this triangle, it can't intersect */

//printf("~1 located possible intersecting triangle %d\n",tp2->n);

					/* Locate intersection of all sides of one triangle with */
					/* the plane of the other. Keep the two points of */
					/* intersection that lie within the triangles. */
					nisps = 0;
//printf("~1 initial nisps = %d\n",nisps);
					for (kk = 0; kk < 2; kk++) {
						gamut *ts;					/* Triangle gamut */
						gtri *tpa;					/* Triangle pointer */
						gtri *tpb;					/* Other triangle pointer */
						if (kk == 0) {
							ts = s1; 
							tpa = tp1;
							tpb = tp2;
						} else {
							ts = s2; 
							tpa = tp2;
							tpb = tp1;
						}

						/* For each edge */
						for (j = 0; j < 3; j++) {
							if (edge_tri_isect(ts, isp, tpa, tpb->e[j]) != 0) {
//printf("~1 isect %f %f %f\n", isp[0],isp[1],isp[2]);
								if (nisps < 2) {
//printf("~1 added at %d\n",nisps);
									icmAry2Ary(isps[nisps], isp);
									nisps++;
								} else {	/* Figure which one to replace */
									int xx;
									/* Replace the one closest to the new one, */
									/* if the new one is further from the other one */

									if (icmNorm33sq(isps[0], isp) < icmNorm33sq(isps[1], isp))
										xx = 0;
									else
										xx = 1;

									if (icmNorm33sq(isps[xx ^ 1], isp)
									  > icmNorm33sq(isps[xx ^ 1], isps[xx])) {
//printf("~1 replaced %d\n",xx);
										icmAry2Ary(isps[xx], isp);
									}
								}
							}
						}
					}
					if (nisps == 0) {
//printf("~1 no intersection\n");
						continue;

					} else if (nisps == 2)  {
						double sarea;

printf("~1 sub triangle =\n");
printf("~1 com %f %f %f\n", opv->p[0], opv->p[1], opv->p[2]);
printf("~1 1st %f %f %f\n", isps[0][0], isps[0][1], isps[0][2]);
printf("~1 2nd %f %f %f\n", isps[1][0], isps[1][1], isps[1][2]);

						/* Accumulate area of these two points + odd point */
						sarea = tri_area(opv->p, isps[0], isps[1]);
printf("~1 located intersecting triangle %d\n",tp2->n);
printf("~1 got sub area %f\n",sarea);
						parea += sarea;
					} else {	/* Hmm */
printf("~1 unexpectedly got %d intersection points in triangle\n",nisps);
					}
				} END_FOR_ALL_ITEMS(tp2);
				
				if (nout == 2) {		/* One "in" point */
					area = parea;
				} else {				/* One "out" point */
					area = area - parea;
				}
printf("~1 partial area = %f\n",area);
			}

			/* Dot product between first vertex in triangle and the unit normal vector */
			dp = tp1->v[0]->p[0] * tp1->pe[0]
			   + tp1->v[0]->p[1] * tp1->pe[1]
			   + tp1->v[0]->p[2] * tp1->pe[2];

printf("~1 vector volume = %f\n",dp * area);
			/* Accumulate gamut volume */
			vol += dp * area;

		} END_FOR_ALL_ITEMS(tp1);
	}

printf("~1 volume sum = %f\n",vol);
	vol = fabs(vol)/3.0;

printf("~1 final volume = %f\n",vol);
	return vol;
}


