#ifndef RSPL_GAM_H
#define RSPL_GAM_H

/* 
 * Argyll Color Correction System
 * Multi-dimensional regularized spline data structure
 *
 * Precise gamut surface, gamut pruning, ink limiting and K min/max
 * support routine.s
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

/* In practice the gamut is only ever computed for 2D or 3D output */
/* dimensions. The gamut surface is composed of simplexes ("triangles") */
/* of dimensions fdi-1, with fdi verticies */

#include "llist.h"

#define MXNE 16			/* Maximum number of edges per triangle allowed */

#define VHASHSIZE 6863	/* Vertex hash index size */
#define EHASHSIZE 2659	/* Edge hash index size */
#define THASHSIZE 2659	/* Triangle hash index size */

/* ----------------------------------------- */

/* Vertex node - all vertex nodes are part of the grid */
struct _rvert {
	struct _rvert *next;	/* Hash linked list */
	int n;					/* Index number of vertex */
	int gix;				/* Grid index - used to identify and order nodes */ 
	float *fg;				/* Pointer to grid data */

//	double p[MXDO];			/* Poistion of node */
	double v[MXDO];			/* Output value of node */
	double r[MXDO];			/* Radial coordinates */
	struct _rvert *list;	/* Next in linked list */
}; typedef struct _rvert rvert;

/* ------------------------------------ */

/* An edge shared by one or more triangle in the mesh */
struct _redge {
	struct _redge *next;	/* Hash linked list */
	int n;					/* Serial number */
//	float *f[MXDO-1];		/* fdi-1 grid verticies of edge in base simplex order. */
	struct _rvert *v[MXDO-1];	/* fdi-1 Verticies of edge in base simplex order. */
	double pe[MXDO+1];		/* Plane equation for edge for side of edge testing. */
							/* fdi for normal + constant */
	int nt;					/* Total number of triangles that share this edge */
	int npt;				/* Positive side triangles that share this edge */
	int nnt;				/* Negative side triangles that share this edge */
	struct _rtri  *t[MXNE];	/* nt triangles edge is part of */

	struct _redge *list;	/* Next in linked list */
}; typedef struct _redge redge;

/* ------------------------------------ */

/* A "triangle" (simplex dimension fdi-1) in the surface mesh */
struct _rtri {
	struct _rtri *next;			/* Hash linked list */
	int n;						/* Serial number */
//	float *f[MXDO];				/* fdi grid verticies in gix order */
	struct _rvert *v[MXDO];		/* fdi verticies in gix order */

//	struct _redge *e[((MXDO+1) * MXDO)/2];		/* Edges in vertex sorted order */
//	double mix[2][MXDO];	/* nn: Bounding box min and max */

	struct _rtri *list;		/* Next in linked list */
}; typedef struct _rtri rtri;

/* ----------------------------------------- */
/* Gamut info stored in main rspl function */
struct _gam_struct {
	int inited;

	double cent[MXDO];	/* Center of radial distance calculation */
	double scale[MXDO];	/* Scale of radial distance calculation */

	void (*outf)(void *cntxf, double *out, double *in);	/* Optional rspl val -> output value */
	void *cntxf;					/* Context for function */
	void (*outb)(void *cntxb, double *out, double *in);	/* Optional output value -> rspl val */
	void *cntxb;					/* Context for function */

	ssxinfo ssi[MXDO-1];	/* Sub-simplex information for sdi from 0..fdi-1 */

	int rvert_no;		/* Number of rverts allocated */
	int vhsize;			/* Vertex hash list size */
	rvert **verts;		/* Hash list, NULL if not allocated */
	rvert *vtop;      	/* Top of list of verticies */
	rvert *vbot;      	/* Bottom of list of verticies */
	
	int redge_no;		/* Number of redges allocated */
	int ehsize;			/* Edge hash list size */
	redge **edges;      /* Edges between the triangles linked list */
	redge *etop;      	/* Top of list of edges */
	redge *ebot;      	/* Bottom of list of edges */

	int rtri_no;		/* Number of rtris allocated */
	int thsize;			/* Triangle hash list size */
	rtri **tris;        /* Hash list, NULL if not allocated */
	rtri *ttop;         /* Surface triangles linked list */

}; typedef struct _gam_struct gam_struct;

#endif /* RSPL_GAM_H */














