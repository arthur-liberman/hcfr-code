
/* 
 * Argyll Color Correction System
 * Multi-dimensional regularized spline data structure
 *
 * Precice gamut surface, gamut pruning, ink limiting and K min/max
 * support routines.
 *
 * Author: Graeme W. Gill
 * Date:   2008/11/21
 *
 * Copyright 1999 - 2008 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Latest simplex/linear equation version.
 */

/* TTBD:

	Add ouutput curve lookup callback support.
	Cache these values in the vertex structures ?

	Add ink limit support. This be done by breaking
	a cell into a fixed geometry of smaller simplexes
	by dividing the cell into two on each axis.

	Need to then add scan that detects areas to prune,
	that then ties in with rev code to mark such
	areas out of gamut.

 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <memory.h>
#include <time.h>

#include "rspl_imp.h"
#include "numlib.h"
#include "sort.h"		/* Heap sort */
#include "counters.h"	/* Counter macros */

#include "vrml.h"		/* If there is VRML/X3D stuff here */	

/* Print a vectors value */
#define DBGVI(text, dim, out, vec, end)			\
{	int pveci;									\
	printf("%s",text);							\
	for (pveci = 0 ; pveci < (dim); pveci++)		\
		printf(out,(vec)[pveci]);				\
	printf(end);								\
}

/* Print a matrix value */
#define DBGMI(text, rows, cols, out, mat, end)		\
{	int pveci, pvecr;								\
	printf("%s",text);								\
	for (pvecr = 0 ; pvecr < (rows); pvecr++) {		\
		for (pveci = 0 ; pveci < (cols); pveci++)		\
			printf(out,(mat)[pvecr][pveci]);		\
	if ((pvecr+1) < (rows))							\
		printf("\n");								\
	}												\
	printf(end);									\
}

#undef VRML_TRACE			/* Save a vrml at each step */

/* Do an arbitrary printf */
#define DBGI(text) printf text ;

#define DEBUG1
#undef DBG
#undef DBGV
#undef DBGM

#undef NEVER
#define ALWAYS

#ifdef DEBUG1
#undef DBGS
#undef DBG
#undef DBGV
#undef DBGM
#define DEBUG
#define DBGS(xxx) xxx
#define DBG(xxx) DBGI(xxx)
#define DBGV(xxx) DBGVI xxx
#define DBGM(xxx) DBGMI xxx
#else
#undef DEBUG
#undef DBGS
#undef DBG
#undef DBGV
#undef DBGM
#define DBGS(xxx) 
#define DBG(xxx) 
#define DBGV(xxx) 
#define DBGM(xxx) 
#endif

/* Convention is to use:
   i to index grid points u.a
   n to index data points d.a
   e to index position dimension di
   f to index output function dimension fdi
   j misc and cube corners
   k misc
 */

#define	EPS (1e-10)			/* Allowance for numeric error */

/* ====================================================== */
/* Support functions */

/* Compute the norm (length) squared of a vector define by two points */
static double norm33sq(double in1[3], double in0[3]) {
	int j;
	double rv;
	for (rv = 0.0, j = 0; j < 3; j++) {
		double tt = in1[j] - in0[j];
		rv += tt * tt;
	}
	return rv;
}


static rvert *get_vert(rspl *s, int gix);

/* Given an output value, return the gamut radius */
static double gvprad(rspl *s, double *v) {
	int f, fdi = s->fdi;
	double rr = 0.0;

	for (f = 0; f < fdi; f++) {
		double tt;
		tt = s->gam.scale[f] * (v[f] - s->gam.cent[f]);
		rr += tt * tt;
	}
	rr = sqrt(rr);
	return rr;
}


/* Given an output value, create the radial coordinate */
static void radcoord(rspl *s, double *rad, double *v) {
	int f, fdi = s->fdi;
	double rr = 0.0;

	for (f = 0; f < fdi; f++) {
		double tt;
		tt = s->gam.scale[f] * (v[f] - s->gam.cent[f]);
		rr += tt * tt;
	}
	rr = sqrt(rr);

	rad[0] = rr;
}

/* Given an output value and an edge, return the side, */
/* 0 = -vem 1 = +ve */
static int eside(rspl *s, redge *ep, double *v) {
	int f, fdi = s->fdi;
	double tt;

	for (tt = 0.0, f = 0; f < fdi; f++) {
		tt += v[f] * ep->pe[f];
	}
	tt += ep->pe[f];
	return (tt >= 0.0 ? 1 : 0);
}


/* Given a list of nodes that form an sdi-1 sub-simplex, */
/* return a list of nodes that could be added to make */
/* an sdi sub-simplex. */
/* Nodes are identified by their grid index. */
/* All possible nodes are returned, even if they are already */
/* in our surface triangulation. */
/* NOTE that inodes will be sorted into sub-simplex order! */
// ~~99 we aren't currently taking ink limit into account

/* return nz on fatal error */
static int get_ssimplex_nodes(
rspl *s,
int sdi, 		/* Dimensionality of target sub-simplex */
rvert **inodes,	/* sdi input nodes that form an sdi-1 dimensional input sub-simplex */
int aonodes,	/* Number of output nodes allowed for */
int *nonodes,	/* Number of output nodes set */
rvert **onodes	/* Space for up to 3^MXDI-1 output nodes */
) {
	int e, di = s->di;
	int i, j, k;

	*nonodes = 0;

//printf("\n~1 get_ssimplex_nodes called with sdi = %d\n",sdi);
	/* Sort the input nodes into normal sub-simplex order */
	/* (We are assuming all the nodes are in the grid) */
	for (i = 0; i < sdi-1; i++) {
		for (j = i+1; j < sdi; j++) {
			if (inodes[i]->gix < inodes[j]->gix) {
				rvert *tt;
				tt = inodes[i]; inodes[i] = inodes[j]; inodes[j] = tt; 
			}
		}
	}
//printf("~2 input nodes = %d %d\n", inodes[0]-gix, inodes[1]->gix);

	/* For each sub-simplex */
	for (i = 0; i < s->gam.ssi[sdi].nospx; i++) {
		int kk;
//printf("~1 sub simplex %d\n",i);
		/* For leaving out one node of the ssimplex in turn, */
		/* check the inodes match the remaining ssimplex nodes */
		for (kk = 0; kk <= sdi; kk++) {		/* ssimplex node being left out */
			int bix = 0;	/* ssimplex base node */

			if (kk == 0)
				bix++;

//printf("~1 anchor node %d = %d\n",j,s->gam.ssi[sdi].spxi[i].goffs[bix]);
//printf("~2 candidate node offsets = %d %d %d\n",
//s->gam.ssi[sdi].spxi[i].offs[0],
//s->gam.ssi[sdi].spxi[i].offs[1],
//s->gam.ssi[sdi].spxi[i].offs[2]);

			for (j = k = 0; j < sdi; j++, k++) {		/* Check all inodes[] */
				if (k == kk)
					k++;			/* Skip the ssimplex node being left out */
				if (inodes[j]->gix != (inodes[0]->gix + s->gam.ssi[sdi].spxi[i].goffs[k]
				                                      - s->gam.ssi[sdi].spxi[i].goffs[bix])) {
//printf("~1 not a match\n");
					break;
				}
//printf("~1 matched node %d\n",inodes[k]->gix);
			}
			if (j >= sdi) { /* they all match */
//printf("~1 all match\n");
				/* Check if kk offset to the base is still in the grid */
				for (e = 0; e < di; e++) {
					int doff = ((s->gam.ssi[sdi].spxi[i].offs[kk] >> e) & 1)
					         - ((s->gam.ssi[sdi].spxi[i].offs[bix] >> e) & 1);
					int eflags = G_FL(inodes[0]->fg,e);
//printf("~1 checking dim %d, doff = %d, eflags = %d\n",e,doff,eflags);
					if ((doff < 0 && (eflags & 4) && (eflags & 3) < 1)
					 || (doff > 0 && !(eflags & 4) && (eflags & 3) < 1)) {
//printf("~1 outside grid\n");
						break;		/* Offset will take us past edge */
					}
				}
				if (e >= di) {		/* Remaining point is within grid */
					int gix;

					/* Add the remaining node to the onodes */
					if (*nonodes >= aonodes)
						return 1;			/* Oops - ran out of return space! */

					gix = inodes[0]->gix + s->gam.ssi[sdi].spxi[i].goffs[kk]
				                         - s->gam.ssi[sdi].spxi[i].goffs[bix];
					onodes[*nonodes] = get_vert(s, gix);

//printf("~1 Returning node %d\n",(onodes[*nonodes]->gix);
//printf("~1 Value %f %f %f\n", onodes[*nonodes]->p[0], onodes[*nonodes]->p[1], onodes[*nonodes]->p[2]);
					(*nonodes)++;
				}
			}
		}
	}
//printf("~1 onodes = %d\n",*nonodes);
	return 0;
}

/* ====================================================== */
/* Search for an existing vertex, and return it. */
/* If there is no existing edge, create it. */
/* (This is where we apply the output functions to the grid values) */
static rvert *get_vert(rspl *s, int gix) {
	int f, fdi = s->fdi;
	int hash;
	rvert *vp = NULL;

	if (gix < 0 || gix >= s->g.no) {	/* Assert */
		error("rspl_gam: get_vert got out of range gix %d\n",gix);
	}

	/* See if it is in our hash list */
	hash = gix % s->gam.vhsize;

	for (vp = s->gam.verts[hash]; vp != NULL; vp = vp->next) {
		if (vp->gix == gix)
			break;
	}
	if (vp == NULL) {	/* No such vertex */
		float *fg;

		if ((vp = calloc(1, sizeof(rvert))) == NULL)
			error("rspl_gam: get_vert calloc failed");

		fg = s->g.a + gix * s->g.pss;
		vp->n = s->gam.rvert_no++;		/* serial number */
		vp->fg = fg;					/* Pointer to node float data */
		vp->gix = gix;					/* grid index */
		for (f = 0; f < fdi; f++)		/* Node output value */
			vp->v[f] = (double)fg[f];
		if (s->gam.outf != NULL)
			s->gam.outf(s->gam.cntxf, vp->v, vp->v);	/* Apply output lookup */

		radcoord(s, vp->r, vp->v);	/* Compute radial coordinate */

		/* Add vertex to hash */
		vp->next = s->gam.verts[hash];
		s->gam.verts[hash] = vp;

		/* Add the vertex to the bottom of the list of vertex */
		if (s->gam.vbot == NULL) {
			s->gam.vtop = s->gam.vbot = vp;
		} else {
			s->gam.vbot->list = vp;
			s->gam.vbot = vp;
		}
	}

	return vp;
}

/* Search for an existing edge containing the given verticies, */
/* and return it. If there is no existing edge, create it. */
/* Note that edges have fdi-2 dimensions == fdi-1 verticies. */
static redge *get_edge(rspl *s, rvert **_vv) {
	int i, j, f, fdi = s->fdi;
	rvert *vv[MXDO-1];		/* Sorted verticies */
	int gix, hash;
	redge *ep = NULL;

	/* Sort the input nodes into normal sub-simplex order */
	/* (We are assuming all the nodes are in the grid) */
	for (i = 0; i < (fdi-1); i++)
		vv[i] = _vv[i];
	for (i = 0; i < (fdi-2); i++) {
		for (j = i+1; j < (fdi-1); j++) {
			if (vv[i]->gix < vv[j]->gix) {
				rvert *tt;
				tt = vv[i]; vv[i] = vv[j]; vv[j] = tt; 
			}
		}
	}
	for (gix = i = 0; i < (fdi-1); i++)
		gix += vv[i]->gix;
//printf("~1 get edge with nodes = %d %d\n", vv[0]->gix, vv[1]->gix);

	/* See if it is in our hash list */
	hash = gix % s->gam.ehsize;
//printf("~1 get edge gix = %d, hash = %d\n", gix, hash);

	for (ep = s->gam.edges[hash]; ep != NULL; ep = ep->next) {
		for (i = 0; i < (fdi-1); i++) {
			if (ep->v[i] != vv[i]) {
//printf("~1 verticies don't match\n");
				break;			/* No match */
			}
		}
		if (i >= (fdi-1)) {
//printf("~1 all verticies match\n");
			break;				/* All match */
		}
	}
	if (ep == NULL) {	/* No such edge */
		int sm, lg;

		if ((ep = calloc(1, sizeof(redge))) == NULL)
			error("rspl_gam: get_edge calloc failed");

		ep->n = s->gam.redge_no++;		/* serial number */
		for (i = 0; i < (fdi-1); i++)
			ep->v[i] = vv[i];			/* Vertex */
printf("~1 new edge %d with nodes = %d %d\n", ep->n, ep->v[0]->gix, ep->v[1]->gix);

		/* Compute plane equation to center of gamut, so */
		/* that we can quickly determine which side a triangle lies on. */

		/* Compute plane equation */
		if (fdi < 2 || fdi > 3)
			error("rspl_gam: plane equation for out dimensions other than 2 or 3 not supported!");
		if (fdi == 2) {
			
		} else {
			double v1[3], v2[3];

			/* Compute two vectors from the three points */
			for (f = 0; f < fdi; f++) {
				v1[f] = ep->v[0]->v[f] - s->gam.cent[f];
				v2[f] = ep->v[1]->v[f] - s->gam.cent[f];
			}

			/* Compute normal to the plane using the cross product */
			ep->pe[0] = ep->v[0]->v[1] * (ep->v[1]->v[2] - s->gam.cent[2])
			          + ep->v[1]->v[1] * (s->gam.cent[2] - ep->v[0]->v[2])
			          + s->gam.cent[1] * (ep->v[0]->v[2] - ep->v[1]->v[2]);
			ep->pe[1] = ep->v[0]->v[2] * (ep->v[1]->v[0] - s->gam.cent[0])
			          + ep->v[1]->v[2] * (s->gam.cent[0] - ep->v[0]->v[0])
			          + s->gam.cent[2] * (ep->v[0]->v[0] - ep->v[1]->v[0]);
			ep->pe[2] = ep->v[0]->v[0] * (ep->v[1]->v[1] - s->gam.cent[1])
			          + ep->v[1]->v[0] * (s->gam.cent[1] - ep->v[0]->v[1])
			          + s->gam.cent[0] * (ep->v[0]->v[1] - ep->v[1]->v[1]);
			ep->pe[3] = - (ep->v[0]->v[0] * (ep->v[1]->v[1] * s->gam.cent[2] - s->gam.cent[1] * ep->v[1]->v[2])
			             + ep->v[1]->v[0] * (s->gam.cent[1] * ep->v[0]->v[2] - ep->v[0]->v[1] * s->gam.cent[2])
			             + s->gam.cent[0] * (ep->v[0]->v[1] * ep->v[1]->v[2] - ep->v[1]->v[1] * ep->v[0]->v[2]));
			
		}

		/* Add edge to hash */
		ep->next = s->gam.edges[hash];
		s->gam.edges[hash] = ep;

		/* Add the edge to the bottom of the list of edges */
		if (s->gam.ebot == NULL) {
			s->gam.etop = s->gam.ebot = ep;
		} else {
			s->gam.ebot->list = ep;
			s->gam.ebot = ep;
		}
	}

printf("~1 returning edge no %d\n",ep->n);
	return ep;
}

/* Check whether a triangle like this already exists */
/* Return NULL if it doesn't */
static rtri *check_tri(rspl *s, redge *ep, rvert *_vv) {
	int i, j, k, f, fdi = s->fdi;
	rvert *vv[MXDO];		/* Sorted verticies */
	int gix, hash;
	rtri *tp = NULL;

	/* Copy edge verticies from edge */
	for (i = 0; i < (fdi-1); i++)
		vv[i] = ep->v[i];
	vv[i] = _vv;			/* And new vertex */

	/* Sort verticies */
	for (i = 0; i < (fdi-1); i++) {
		for (j = i+1; j < fdi; j++) {
			if (vv[i]->gix < vv[j]->gix) {
				rvert *tv;
				tv = vv[i]; vv[i] = vv[j]; vv[j] = tv; 
			}
		}
	}

	/* Create hash */
	for (gix = i = 0; i < fdi; i++)
		gix += vv[i]->gix;

	/* See if it is in our hash list */
	hash = gix % s->gam.thsize;
//printf("~1 make tri gix = %d, hash = %d\n", gix, hash);

	for (tp = s->gam.tris[hash]; tp != NULL; tp = tp->next) {
		for (i = 0; i < fdi; i++) {
			if (tp->v[i] != vv[i]) {
//printf("~1 verticies don't match\n");
				break;			/* No match */
			}
		}
		if (i >= fdi) {
//printf("~1 all verticies match\n");
			break;				/* All match */
		}
	}

	return tp;
}

/* Create a triangle given an edge with fdi-1 verticies and a grid vertex. */
/* if inv is set, triangle is upside down wrt to center point, */
/* and so all the oppositive verticies should be placed on the */
/* opposite to their natural side. */
static rtri *make_tri(rspl *s, redge *ep, rvert *_vv, int inv) {
	int i, j, k, f, fdi = s->fdi;
	rvert *vv[MXDO];		/* Sorted verticies */
	int gix, hash;
	rtri *tp = NULL;

printf("~1 make_tri called\n");

	/* Copy edge verticies from edge */
	for (i = 0; i < (fdi-1); i++)
		vv[i] = ep->v[i];
	vv[i] = _vv;			/* And new vertex */

	/* Sort verticies */
	for (i = 0; i < (fdi-1); i++) {
		for (j = i+1; j < fdi; j++) {
			if (vv[i]->gix < vv[j]->gix) {
				rvert *tv;
				tv = vv[i]; vv[i] = vv[j]; vv[j] = tv; 
			}
		}
	}

	/* Create hash */
	for (gix = i = 0; i < fdi; i++)
		gix += vv[i]->gix;

	/* See if it is in our hash list */
	hash = gix % s->gam.thsize;
printf("~1 make tri gix = %d, hash = %d\n", gix, hash);

	for (tp = s->gam.tris[hash]; tp != NULL; tp = tp->next) {
		for (i = 0; i < fdi; i++) {
			if (tp->v[i] != vv[i]) {
//printf("~1 verticies don't match\n");
				break;			/* No match */
			}
		}
		if (i >= fdi) {
//printf("~1 all verticies match\n");
			break;				/* All match */
		}
	}

	if (tp == NULL) {			/* No such triangle */

//printf("~1 creating a new triangle\n");
		/* Create triangle */
		if ((tp = calloc(1, sizeof(rtri))) == NULL)
			error("rspl_gam: make_tri calloc failed");

		tp->n = s->gam.rtri_no++;		/* serial number */

		/* Copy edge verticies to triangle */
		for (i = 0; i < fdi; i++)
			tp->v[i] = vv[i];

		/* Link all the triangles edges to this triangle */
		printf("~1 triangle nodes = %d %d %d\n", tp->v[0]->gix, tp->v[1]->gix, tp->v[2]->gix);
//printf("~2 triangle vert 0 = %f %f %f\n", tp->v[0]->v[0], tp->f[0]->v[1], tp->f[0]->v[2]);
//printf("~2 triangle vert 1 = %f %f %f\n", tp->v[1]->v[0], tp->f[1]->v[1], tp->f[1]->v[2]);
//printf("~2 triangle vert 2 = %f %f %f\n", tp->v[2]->v[0], tp->f[2]->v[1], tp->f[2]->v[2]);
		for (i = 0; i < fdi; i++) {			/* For each tri verticy being odd one out */
			rvert *ov, *rr[MXDO-1];			/* Odd verticy, remaining edge verticies */
			int ss;							/* Side */

			ov = tp->v[i];					/* Odd node */
			for (k = j = 0; j < fdi; j++) {
				if (i == j)
					continue;
				rr[k++] = tp->v[j];			/* Remaining nodes */
			}
//printf("~1 edge nodes = %d %d, odd = %d\n", rr[0]->gix, rr[1]->gix, ov->gix);
			ep = get_edge(s, rr);

			if (ep->nt >= MXNE)
				error("rspl_gam: make_tri run out of triangle space %d in edge",MXNE);
			ep->t[ep->nt++] = tp;

			/* See which side of the edge the remaining vertex is */
			ss = eside(s, ep, ov->v);
//printf("~1 node gix %d has side %d to edge %d %d\n", ov->gix, ss, ep->v[0]->gix, ep->v[1]->gix);
			if (inv)
				ss = 1-ss;
			if (ss) {	/* +ve side */
				ep->t[ep->npt++] = tp;
			} else {
				ep->t[ep->nnt++] = tp;
			}
		}

		/* Add triangle to hash */
		tp->next = s->gam.tris[hash];
		s->gam.tris[hash] = tp;

		/* Add them to the linked list */
		tp->list = s->gam.ttop;
		s->gam.ttop = tp;

	}

	return tp;
}


/* ====================================================== */

/* Create a surface gamut representation. */
/* Return NZ on error */
/* Could add more flexibility with:
	optional function instead of default radial distance function
	option use sub set of output dimensions (ie allow for CMYK->LabK etc. ? )
*/
static int
gam_comp_gamut(
rspl *s,
double *cent,		/* Optional center of gamut [fdi], default center of out range */
double *scale,		/* Optional Scale of output values in vector to center [fdi], def. 1.0 */
void (*outf)(void *cntxf, double *out, double *in),	/* Optional rspl val -> output value */
void *cntxf,					/* Context for function */
void (*outb)(void *cntxb, double *out, double *in),	/* Optional output value -> rspl val */
void *cntxb						/* Context for function */
) {
	int e, f, di = s->di, fdi = s->fdi;
	int i, j, ssdi;
	int maxp[MXDO];			/* Grid indexes of maxium function values */
	rvert *fedge[MXDO-1];	/* The first "edge" containing fdi-1 verticies */
	rvert *onodes[50];		/* float pointers of canditate nodes */
	int aonodes = 50;		/* Allocated out nodes */
	int nonodes;			/* Number of nodes returned */

	rvert *cnodes[2][50];	/* Negative, positive candidate nodes */
	double rcnodes[2][50];	/* Radius of negative, positive candidate nodes */ 
	int ncnodes[2];			/* Number of negative, positive candidate nodes */
	
	if (fdi < 2 || fdi > di) {
		DBG(("gam: gam_comp_gamut called for di = %d, fdi = %d\n", di, fdi));
		return 2;
	}

	/* Save output value conversion functions */
	s->gam.outf = outf;
	s->gam.cntxf = cntxf;
	s->gam.outb = outb;
	s->gam.cntxb = cntxb;

	/* Deal with gamut center point, and scale */
	if (cent == NULL) {
		double min[MXDO], max[MXDO];

		s->get_out_range(s, min, max);
		if (s->gam.outf != NULL) {
			s->gam.outf(s->gam.cntxf, min, min);	/* Apply output lookup */
			s->gam.outf(s->gam.cntxf, max, max);	/* Apply output lookup */
		}
		for (f = 0; f < fdi; f++)
			s->gam.cent[f] = 0.5 * (min[f] + max[f]);
	} else {
		for (f = 0; f < fdi; f++)
			s->gam.cent[f] = cent[f];
	}
	DBGVI("Gamut center is ", fdi, "%f ", s->gam.cent, "\n")

	if (scale == NULL) {
		for (f = 0; f < fdi; f++)
			s->gam.scale[f] = 1.0;
	} else {
		for (f = 0; f < fdi; f++)
			s->gam.scale[f] = scale[f];
	}
	DBGVI("Gamut scale is ", fdi, "%f ", s->gam.scale, "\n")

	for (ssdi = 1; ssdi <= fdi-1; ssdi++) {
		int i, j;

		/* Compute gamut surface sub-simplex geometry info */
		rspl_init_ssimplex_info(s, &s->gam.ssi[ssdi], ssdi); 
	
		/* Filter the sub-simplex geometry to only include subsimplexes that */
		/* use the base vertex, so that there are no duplicates. */
		for (i = j = 0; i < s->gam.ssi[ssdi].nospx; i++) {
			if (s->gam.ssi[ssdi].spxi[i].offs[s->gam.ssi[ssdi].sdi] == 0) {
				s->gam.ssi[ssdi].spxi[j] = s->gam.ssi[ssdi].spxi[i];	/* Structure copy */
				j++;
			}
		}
		s->gam.ssi[ssdi].nospx = j;

#ifdef DEBUG
		printf("Sub-simplex dim %d out of input %d\n",s->gam.ssi[ssdi].sdi,di);
		printf("Number of subsimplex = %d\n",s->gam.ssi[ssdi].nospx);
		for (i = 0; i < s->gam.ssi[ssdi].nospx; i++) {
			printf("Cube Offset = ");
			for (e = 0; e <= s->gam.ssi[ssdi].sdi; e++)
				printf("%d ",s->gam.ssi[ssdi].spxi[i].offs[e]);
			printf("\n");

			printf("Grid Offset = ");
			for (e = 0; e <= s->gam.ssi[ssdi].sdi; e++)
				printf("%d ",s->gam.ssi[ssdi].spxi[i].foffs[e]);
			printf("\n");
			printf("\n");
		}
#endif /* DEBUG */
	}

	/* Allocate the vertex hash array */
	if ((s->gam.verts = calloc(VHASHSIZE, sizeof(rvert *))) == NULL) {
		DBG(("gam: allocating vertex hash array failed\n"));
		return 1;
	}
	s->gam.vhsize = VHASHSIZE;

	/* Allocate the edge hash array */
	if ((s->gam.edges = calloc(EHASHSIZE, sizeof(redge *))) == NULL) {
		DBG(("gam: allocating edge hash array failed\n"));
		return 1;
	}
	s->gam.ehsize = EHASHSIZE;

	/* Allocate the triangle hash array */
	if ((s->gam.tris = calloc(THASHSIZE, sizeof(rtri *))) == NULL) {
		DBG(("gam: allocating tris hash array failed\n"));
		return 1;
	}
	s->gam.thsize = THASHSIZE;

	/* Get a starting gid point for surface */
	// ~~99 this isn't right if the point we get is over the ink limit!!!. */
	s->get_out_range_points(s, NULL, maxp);		/* Maximum */
//	s->get_out_range_points(s, maxp, NULL);		/* Minium */

	DBG(("Starting point = gix %d/%d\n",maxp[0], s->g.no));

	/* Now work it up to an fdi-2 sub-simplex, so that we have a */
	/* "triangle edge", and can enter the main loop. */
	// ~~~9 this needs fixing to switch to "maximum angle" calculation 
	fedge[0] = get_vert(s, maxp[0]); /* grid index of first vertex */
	
	if (fdi == 2) {
		/* We just need one point, and we've got it */

	} else if (fdi == 3) {		/* Usual case */
		double ba = -1.0;		/* Bigest angle */
		/* We just need two points, so find the other points */
		/* that makes the greatest angle to the center */
 
		if (get_ssimplex_nodes(s, 1, fedge, aonodes, &nonodes, onodes)) {
			error("rspl_gam: get_ssimplex_nodes fatal error - too many nodes?");
		}
		if (nonodes == 0)
			error("rspl_gam: get_ssimplex_nodes fatal error - retrurned no nodes?");

printf("~1 get_ssimplex_nodes returned %d nodes\n",nonodes);
for(i = 0; i < nonodes; i++)
printf(" ~1 %d: %d\n",i,onodes[i]->gix);

		/* Evaluate the choice of verticies to choose the one that */
		/* will best enclose the gamut. (We use a really dumb criteria - maximum radius) */
		for (i = 0; i < nonodes; i++) {
			double tt, a, b, c;		/* Lenght of sides of triangle squared */
			a = norm33sq(onodes[i]->v, s->gam.cent);
			b = norm33sq(onodes[i]->v, fedge[0]->v);
			c = norm33sq(fedge[0]->v, s->gam.cent);
			tt = acos((b + c - a)/(2.0 * sqrt(b * c)));
printf("~1 node %d angle = %f\n",onodes[i]->gix,tt);
			if (tt > ba) {
				ba = tt;
				fedge[1] = onodes[i];	/* Use candidate with largest gamut as next base */
			}
		}
printf("~1 chosen node %d\n",fedge[1]->gix);

	} else if (fdi > 3) {		/* General case */
		/* This isn't a correct approach ... */

		for (ssdi = 1; ssdi <= fdi-2; ssdi++) {
			double br = -1.0;		/* Bigest radius */

			DBG(("Working up ssdim %d -> %d\n",ssdi-1, ssdi));

			if (get_ssimplex_nodes(s, ssdi, fedge, aonodes, &nonodes, onodes)) {
				error("rspl_gam: get_ssimplex_nodes fatal error - too many nodes?");
			}
			if (nonodes == 0)
				error("rspl_gam: get_ssimplex_nodes fatal error - retrurned no nodes?");

printf("~1 get_ssimplex_nodes returned %d nodes\n",nonodes);
for(i = 0; i < nonodes; i++)
printf(" ~1 %d: %d\n",i,onodes[i]->gix);

			/* Evaluate the choice of verticies to choose the one that */
			/* will best enclose the gamut. (We use a really dumb criteria - maximum radius) */
			for (i = 0; i < nonodes; i++) {
				double tt;
				tt = gvprad(s, onodes[i]->v);		/* Compute radius */
				if (tt > br) {
					br = tt;
					fedge[ssdi] = onodes[i];	/* Use candidate with largest gamut as next base */
				}
			}
printf("~1 chosen node %d\n",fedge[ssdi]->gix);
		}
	}

	/* Creat the initial "edge" */
	get_edge(s, fedge);

printf("~1 Created initial edge\n");

	{
		redge *ep;

		double **A;		/* lu value -> baricentric matrix */
		double *B;
		int *pivx;
					
		pivx = ivector(0, fdi-1);				/* pixv[fdi] */
		A = dmatrix(0, fdi-1, 0, fdi-1);		/* A[fdi][fdi] */
		B = dvector(0, fdi-1);					/* B[fdi] */

		/* The main loop: */
		/* We start with any "edges" that have less than two associated "triangles", */
		/* and evaluate the vertex nodes that can make "triangles" with the "edge". */
		/* We choose the node that will give the greatest slope, and make a "triangle". */
		/* Loop until there are no "edges" with less than two associated "triangles". */
		for (ep = s->gam.etop; ep != NULL; ep = ep->list) {

printf("~1 expanding from edge no %d\n",ep->n);
printf("~1 edge v1 = %d = %f %f %f\n", ep->v[0]->gix, ep->v[0]->v[0], ep->v[0]->v[1], ep->v[0]->v[2]);
printf("~1 edge v2 = %d = %f %f %f\n", ep->v[1]->gix, ep->v[1]->v[0], ep->v[1]->v[1], ep->v[1]->v[2]);

//			if (ep->npt < 1 || ep->nnt < 1)
			{
				int ss;
	
				if (get_ssimplex_nodes(s, fdi-1, ep->v, aonodes, &nonodes, onodes)) {
					error("rspl_gam: get_ssimplex_nodes fatal error - too many nodes?");
				}

				/* Clasify the returned nodes as positive or negative side */
				ncnodes[0] = ncnodes[1] = 0;
				for (i = 0; i < nonodes; i++) {
					double tt;
					tt = gvprad(s, onodes[i]->v);		/* Compute radius */
					ss = eside(s, ep, onodes[i]->v);	/* Side */
printf("~1 node gix %d has rad %f side %d to edge %d %d\n", onodes[i]->gix, tt, ss, ep->v[0]->gix, ep->v[1]->gix);

#ifdef NEVER	/* This messes things up ? */
					/* Check if this node is already in a triangle with this edge, */
					/* to avoid costly matrix solution check below. */
					if (check_tri(s, ep, onodes[i]) == NULL)
#endif
					{
						cnodes[ss][ncnodes[ss]] = onodes[i];
						rcnodes[ss][ncnodes[ss]++] = tt;
					}
				}

				/* Sort them */
				for (ss = 0; ss < 2; ss++) {		/* Negative side then positive */
					for (i = 0; i < (ncnodes[ss]-1); i++) {
						for (j = i+1; j < ncnodes[ss]; j++) {
							if (rcnodes[ss][i] < rcnodes[ss][j]) {
								rvert *tt;
								double tr;
								tt = cnodes[ss][i]; cnodes[ss][i] = cnodes[ss][j];
								cnodes[ss][j] = tt; 
								tr = rcnodes[ss][i]; rcnodes[ss][i] = rcnodes[ss][j];
								rcnodes[ss][j] = tr; 
							}
						}
					}
				}

// ~~1
for (ss = 0; ss < 2; ss++) {		/* Negative side then positive */
	if (ss == 0)
		printf("~1 -ve nodes:\n");
	else
		printf("~1 +ve nodes:\n");
	for (i = 0; i < ncnodes[ss]; i++) {
		printf("~1 node %d, rad %f\n",cnodes[ss][i]->gix,rcnodes[ss][i]);
	}
}

#ifdef VRML_TRACE
{
	vrml *wrl;
	int i, j, k;
	ECOUNT(gc, MXDIDO, di, 0, s->g.res, 0);/* coordinates */
	DCOUNT(cc, MXDIDO, s->di, 0, 0, 2);   /* Surrounding cube counter */
	float *gp;			/* Grid point pointer */
	rvert *vp;
	rtri *tp;
	double col[3], pos[3];

	if ((wrl = new_vrml("gam_diag", 1, vrml_lab)) == NULL)
		error("new_vrml failed for 'gam_diag%s\n",vrml_ext());

	wrl->start_line_set(wrl, 0);

	/* Display the grid */
	EC_INIT(gc);
	for (gp = s->g.a; !EC_DONE(gc); gp += s->g.pss) {
		/* Itterate cube from this base */
		DC_INIT(cc)
		for (i = 0; !DC_DONE(cc); i++ ) {
			float *sp = s->g.a;

			for (e = 0; e < s->di; e++) { 				/* Input tables */
				int j;
				j = gc[e] + cc[e];
				if (j < 0 || j >= s->g.res[e]) {
					sp = NULL;			/* outside grid */
					break;
				}
				sp += s->g.fci[e] * j;	/* Compute pointer to surrounder */
			}
			if (i> 0 && e >= s->di) {
				/* Create vector from base to surrounder */
				pos[0] = (double)gp[0];
				pos[1] = (double)gp[1];
				pos[2] = (double)gp[2];
				if (s->gam.outf != NULL)
					s->gam.outf(s->gam.cntxf, pos, pos);	/* Apply output lookup */
				wrl->add_vertex(wrl, 0, pos);
				pos[0] = (double)sp[0];
				pos[1] = (double)sp[1];
				pos[2] = (double)sp[2];
				if (s->gam.outf != NULL)
					s->gam.outf(s->gam.cntxf, pos, pos);	/* Apply output lookup */
				wrl->add_vertex(wrl, 0, pos);
			}
			DC_INC(cc);
		}
		EC_INC(gc);
	}
	wrl->make_lines(wrl, 0, 2);
	wrl->start_line_set(wrl, 0);

	/* Display the current triangles transparently */
	if (s->gam.ttop != NULL) {
		for (vp = s->gam.vtop; vp != NULL; vp = vp->list) {
			wrl->add_vertex(wrl, 0, vp->v);
		}
	
		/* Set the triangles */
		for (tp = s->gam.ttop; tp != NULL; tp = tp->list) {
			int ix[3];
			ix[0] = tp->v[0]->n;
			ix[1] = tp->v[1]->n;
			ix[2] = tp->v[2]->n;
			wrl->add_triangle(wrl, 0, ix);
		}
		
//		wrl->make_triangles(wrl, 0, 0.3, NULL);
		wrl->make_triangles(wrl, 0, 0.0, NULL);
		wrl->start_line_set(wrl, 0);
	}

	/* Show the active edge as a red cone */
	col[0] = 1.0; col[1] = 0.0; col[2] = 0.0;	/* Red */
	wrl->add_cone(wrl, ep->v[0]->v, ep->v[1]->v, col, 1.0);
//printf("~1 edge v1 = %d = %f %f %f\n", ep->v[0]->gix, ep->v[0]->v[0], ep->v[0]->v[1], ep->v[0]->v[2]);
//printf("~1 edge v2 = %d = %f %f %f\n", ep->v[1]->gix, ep->v[1]->v[0], ep->v[1]->v[1], ep->v[1]->v[2]);

	/* Show the candidate verticies */
	for (ss = 0; ss < 2; ss++) {
		for (i = 0; i < ncnodes[ss]; i++) {		/* +ve */
			if (ss) {
				col[0] = 0.0; col[1] = 1.0; col[2] = 0.0;	/* Green  +ve */
			} else {
				col[0] = 0.0; col[1] = 0.0; col[2] = 1.0;	/* Blue -ve */
			}
			wrl->add_marker(wrl, cnodes[ss][i]->v, col, 1.0);
		}
	}
	wrl->del(wrl);
	printf("Waiting for return after writing gam_diag%s\n",vrml_ext()); 
	getchar();
}
#endif
				/* See if we can make a triangle */
				for (ss = 0; ss < 2; ss++) {	/* For -ve and +ve sides */
					int ii, si;
					int sta, end, inc;
					rvert **nods;
					double rip;		/* Row interchange parity */

printf("~1 direction = %d\n",ss);
#ifdef NEVER
					if (( ss && ep->npt >= 1) 		/* Not looking for a positive node */
					 || (!ss && ep->nnt >= 1)) {  	/* Not looking for a negative node */
printf("~1 no need to look for node in this direction\n");
						continue;
					}
#endif

					if (ncnodes[ss] > 0)	{ 	/* There are nodes in wanted direction */
						si = ss;			/* use wanted direction nodes */
						sta = 0;			/* Start at max radius node */
						end = ncnodes[si];	/* End and least radius node */
						inc = 1;			/* Increment */
						nods = cnodes[si];	/* +ve nodes */
printf("~1 Looking for biggest angle, inc = %d\n",inc);
#ifdef NEVER		/* Convex tracing ? */
					} else if (ncnodes[1-ss] > 0) {	/* There are opposited direction nodes */
						si = 1-ss;				/* use opposite direction nodes */
						sta = ncnodes[si]-1;	/* Start at min radius node */
						end = -1;			/* end and max radius node */
						inc = -1;			/* Decrement */
						nods = cnodes[si];
printf("~1 Looking for smallest angle, inc = %d\n",-1);
#endif	/* NEVER */
					} else {
printf("~1 No points to search\n");
						continue;
					}
					ii = 0;
					if (ncnodes[si] > 1) {		/* If there is more than one to choose from */
						/* Go through each candidate in the most likely order */
						for (ii = sta; ii != end; ii += inc) {
							
printf("~1 Candidate %d: node %d\n",ii,nods[ii]->gix);
							/* Create the baricentric conversion for this candidate */
							for (f = 0; f < fdi; f++)		/* The center point */
								A[f][0] = s->gam.cent[f] - nods[ii]->v[f];
							for (j = 0; j < (fdi-1); j++) {		/* The edge points */
								for (f = 0; f < fdi; f++)
									A[f][j+1] = ep->v[j]->v[f] - nods[ii]->v[f];
							}
							
							if (lu_decomp(A, fdi, pivx, &rip)) {
printf("~1 lu_decomp failed\n");
for (f = 0; f < fdi; f++)		/* The center point */
	A[f][0] = s->gam.cent[f] - nods[ii]->v[f];
for (j = 0; j < (fdi-1); j++) {		/* The edge points */
	for (f = 0; f < fdi; f++)
		A[f][j+1] = ep->v[j]->v[f] - nods[ii]->v[f];
}
printf("~1 A = \n");
printf("~1    %f %f %f\n", A[0][0], A[0][1], A[0][2]);
printf("~1    %f %f %f\n", A[1][0], A[1][1], A[1][2]);
printf("~1    %f %f %f\n", A[2][0], A[2][1], A[2][2]);
								warning("lu_decomp failed");
								continue;
							}

							/* Test the remainder candidates against this simplex */
							for (i = sta; i != end; i += inc) {
								if (i == ii)
									continue;

printf("~1 Test %d: node %d\n",i,nods[i]->gix);
								for (f = 0; f < fdi; f++)			/* The candidate point */
									B[f] = nods[i]->v[f] - nods[ii]->v[f];
								lu_backsub(A, fdi, pivx, B);

#ifdef NEVER
printf("~1 baricentric = %f %f %f %f\n",B[0],B[1],B[2],1.0-B[0]-B[1]-B[2]);
{
double tt[3], B3;
/* Check baricentric */
B3 = 1.0-B[0]-B[1]-B[2];
for (f = 0; f < fdi; f++)
	tt[f] = 0.0;
for (f = 0; f < fdi; f++)
	tt[f] += B[0] * s->gam.cent[f];
for (f = 0; f < fdi; f++)
	tt[f] += B[1] * ep->v[0]->v[f];
for (f = 0; f < fdi; f++)
	tt[f] += B[2] * ep->v[1]->v[f];
for (f = 0; f < fdi; f++)
	tt[f] += B3 * nods[ii]->v[f];
printf("~1 target point %f %f %f\n", (double)nods[i][0], (double)nods[i][1], (double)nods[i][2]);
printf("~1 barice check %f %f %f\n", tt[0],tt[1],tt[2]);
}
#endif /* NEVER */
								if ((inc == 1 && B[0] < -EPS)   /* other point is at higher angle */
								 || (inc == -1 && B[0] > EPS)) { /* other point is at lower angle */
printf("~1 candidate isn't best\n");
									break;
								}
							}
							if (i == end) {
printf("~1 candidate IS the best\n");
								break;				/* Candidate is at highest angle */
							}
						}
						if (ii == end) {
printf("~1Inconsistent candidate ordering\n");
							error("Inconsistent candidate ordering");
						}
					} else {
printf("~1 there are only %d nodes, so don't search them\n",ncnodes[si]);
					}
printf("~1 Making triangle with %d: node %d\n",ii,nods[ii]->gix);
					make_tri(s, ep, nods[ii], inc == -1);
				}
				if (ep->npt < 1 || ep->nnt < 1) {
					if (ep->npt < 1)
						warning("###### Unable to locate +ve triangles for edge %d\n",ep->n);
					if (ep->nnt < 1)
						warning("###### Unable to locate -ve triangles for edge %d\n",ep->n);
				}
			}
		}

		free_ivector(pivx, 0, fdi-1);
		free_dmatrix(A, 0, fdi-1, 0, fdi-1);
		free_dvector(B, 0, fdi-1);

		/* Dump out the edges */
		for (ep = s->gam.etop; ep != NULL; ep = ep->list) {

printf("~1 edge no %d, npt = %d, nnt = %d\n",ep->n, ep->npt, ep->nnt);
		}
	}
	return 0;
}

/* ====================================================== */
/* Gamut rspl setup functions                           */

/* Called by rspl initialisation */
void
init_gam(rspl *s) {

	/* Methods */
	s->comp_gamut = gam_comp_gamut;
}

/* Free up all the gamut info */
void free_gam(
rspl *s		/* Pointer to rspl grid */
) {
	int i;
	int ssdi;
	rvert *vp, *nvp;
	redge *ep, *nep;
	rtri *tp, *ntp;

	for (ssdi = 1; ssdi <= s->fdi-1; ssdi++)
		rspl_free_ssimplex_info(s, &s->gam.ssi[ssdi]);

	/* Free the verticies */
	for (vp = s->gam.vtop; vp != NULL; vp = nvp) {
		nvp = vp->list;
		free(vp);
	}
	free(s->gam.verts);

	/* Free the edges */
	for (ep = s->gam.etop; ep != NULL; ep = nep) {
		nep = ep->list;
		free(ep);
	}
	free(s->gam.edges);

	/* Free the triangles */
	for (tp = s->gam.ttop; tp != NULL; tp = ntp) {
		ntp = tp->list;
		free(tp);
	}
	free(s->gam.tris);
}


/* Create the gamut surface structure. */
/* Current this is:

	not rationalized to be non-overlapping
	culled to eliminate overlaps
	Have ink limits applied

*/

/* ~~~ we need space ing gam to:

	store radial coordinates of output values
	mark nodes as being visited.
	store surface triangle structures 
	hold tadial output bounding acceleration structures


	There is a lot in common with rev here.
	we need to know input channel significance (what's black)
	plus ink limit info.
	we're going to create black generation information used by rev.
*/

/* ---------------------- */
/* rspl gam diagnostic */

/* Diagnostic */
void rspl_gam_plot(rspl *s, char *name) {
	int i;
	double col[3] = { 0.7, 0.7, 0.7 };
	rvert *vp, *nvp;
	rtri *tp, *ntp;

	vrml *wrl;

	if ((wrl = new_vrml(name, 1, 0)) == NULL)
		error("new_vrml failed for '%s%s'\n",name,vrml_ext());

	/* Set the verticies */
	for (vp = s->gam.vtop; vp != NULL; vp = vp->list) {
		wrl->add_vertex(wrl, 0, vp->v);
	}

	/* Set the triangles */
	for (tp = s->gam.ttop; tp != NULL; tp = ntp) {
		int ix[3];
		ntp = tp->list;
		ix[0] = tp->v[0]->n;
		ix[1] = tp->v[1]->n;
		ix[2] = tp->v[2]->n;
		wrl->add_triangle(wrl, 0, ix);
	}
	
	wrl->make_triangles(wrl, 0, 0.0, NULL);
//	wrl->make_triangles(wrl, 0, 0.0, col);

	wrl->del(wrl);
}

#undef DEBUG
#undef DBGV
#undef DBG
#define DBGV(xxx)
#define DBG(xxx) 






