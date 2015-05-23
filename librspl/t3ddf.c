/************************************************/
/* Test RSPL in 3D with weak default function */
/************************************************/

/* Author: Graeme Gill
 * Date:   20/11/2005
 * Derived from cmatch.c
 * Copyright 1995, 2005 Graeme W. Gill
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */


#define DEBUG
#define DETAILED

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include "rspl.h"
#include "tiffio.h"
#include "plot.h"
#include "ui.h"

#ifdef NEVER
#define INTERP spline_interp
#else
#define INTERP interp
#endif

#ifdef NEVER
FILE *verbose_out = stdout;
int verbose_level = 6;			/* Current verbosity level */
								/* 0 = none */
								/* !0 = diagnostics */
#endif /* NEVER */

#define PLOTRES 256
#define WIDTH 400			/* Raster size */
#define HEIGHT 400

#define MAX_ITS 500
#define IT_TOL 0.0005
#define GRES0 33			/* Default rspl resolutions */
#define GRES1 33
#define GRES2 33
#undef NEVER
#define ALWAYS

/* two correction points along x = y = 0.5 */
co test_points1[] = {
//	{{ 0.5, 0.5, 0.325 },{ 0.4 }},	/* 0 */
//	{{ 0.5, 0.5, 0.625 },{ 0.70 }}	/* 1 */

	{{ 0.4, 0.4, 0.325 },{ 0.5 }},	/* 0 */
	{{ 0.4, 0.4, 0.625 },{ 0.8 }},	/* 1 */
	{{ 0.5, 0.4, 0.325 },{ 0.5 }},	/* 0 */
	{{ 0.5, 0.4, 0.625 },{ 0.8 }},	/* 1 */
	{{ 0.6, 0.4, 0.325 },{ 0.5 }},	/* 0 */
	{{ 0.6, 0.4, 0.625 },{ 0.8 }},	/* 1 */

	{{ 0.4, 0.5, 0.325 },{ 0.5 }},	/* 0 */
	{{ 0.4, 0.5, 0.625 },{ 0.8 }},	/* 1 */
	{{ 0.5, 0.5, 0.325 },{ 0.5 }},	/* 0 */
	{{ 0.5, 0.5, 0.625 },{ 0.8 }},	/* 1 */
	{{ 0.6, 0.5, 0.325 },{ 0.5 }},	/* 0 */
	{{ 0.6, 0.5, 0.625 },{ 0.8 }},	/* 1 */

	{{ 0.4, 0.6, 0.325 },{ 0.5 }},	/* 0 */
	{{ 0.4, 0.6, 0.625 },{ 0.8 }},	/* 1 */
	{{ 0.5, 0.6, 0.325 },{ 0.5 }},	/* 0 */
	{{ 0.5, 0.6, 0.625 },{ 0.8 }},	/* 1 */
	{{ 0.6, 0.6, 0.325 },{ 0.5 }},	/* 0 */
	{{ 0.6, 0.6, 0.625 },{ 0.8 }}	/* 1 */
};

#ifdef NEVER
#ifdef	__STDC__
#include <stdarg.h>
void error(char *fmt, ...), warning(char *fmt, ...), verbose(int level, char *fmt, ...);
#else
#include <varargs.h>
void error(), warning(), verbose();
#endif
#endif /* NEVER */

void write_rgb_tiff(char *name, int width, int height, unsigned char *data);

/* Weak default function */
/* Linear along z, independent of x & y */
static void wfunc(void *cbntx, double *out, double *in) {
	out[0] = in[2];
}

void usage(void) {
	fprintf(stderr,"Test 3D rspl interpolation with weak default function\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: t2d [options]\n");
	fprintf(stderr," -t n          Test set:\n");
	fprintf(stderr,"             * 1 = two points along x & y\n");
	fprintf(stderr," -w wweight    Set weak default function weight (default 1.0)\n");
	fprintf(stderr," -r resx,resy,resz  Set grid resolutions (def %d %d %d)\n",GRES0,GRES1,GRES2);
	fprintf(stderr," -h            Test half scale resolution too\n");
	fprintf(stderr," -q            Test quarter scale resolution too\n");
	fprintf(stderr," -x            Use auto smoothing\n");
	fprintf(stderr," -s            Test symetric smoothness (set asymetric -r !)\n");
	fprintf(stderr," -s            Test symetric smoothness\n");
	fprintf(stderr," -p            plot 4 slices, xy = 0.5, yz = 0.5, xz = 0.5,  x=y=z\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	rspl *rss;			/* Regularized spline structure */
	rspl *rss2 = NULL;	/* Regularized spline structure at half/quarter resolution */
	datai low,high;
	int gres[MXDI];
	int gres2[MXDI];
	double avgdev[MXDO];
	co *test_points = test_points1;
	int npoints = sizeof(test_points1)/sizeof(co);
	double wweight = 1.0;
	int autosm = 0;
	int dosym = 0;
	int doplot = 0;
	int doh = 0;
	int doq = 0;
	int rsv;
	int flags = RSPL_NOFLAGS;

	low[0] = 0.0;
	low[1] = 0.0;
	low[2] = 0.0;
	high[0] = 1.0;
	high[1] = 1.0;
	high[2] = 1.0;
	gres[0] = GRES0;
	gres[1] = GRES1;
	gres[2] = GRES2;
	avgdev[0] = 0.0;
	avgdev[1] = 0.0;
	avgdev[2] = 0.0;

	/* Process the arguments */
	for(fa = 1;fa < argc;fa++) {
		nfa = fa;					/* skip to nfa if next argument is used */
		if (argv[fa][0] == '-')	{	/* Look for any flags */
			char *na = NULL;		/* next argument after flag, null if none */

			if (argv[fa][2] != '\000')
				na = &argv[fa][2];		/* next is directly after flag */
			else {
				if ((fa+1) < argc) {
					if (argv[fa+1][0] != '-') {
						nfa = fa + 1;
						na = argv[nfa];		/* next is seperate non-flag argument */
					}
				}
			}

			if (argv[fa][1] == '?')
				usage();

			/* test set */
			else if (argv[fa][1] == 't' || argv[fa][1] == 'T') {
				int ix;
				fa = nfa;
				if (na == NULL) usage();
				ix = atoi(na);
    			switch (ix) {
					case 1:
						test_points = test_points1;
						npoints = sizeof(test_points1)/sizeof(co);
						break;
					default:
						usage();
				}

			} else if (argv[fa][1] == 'w' || argv[fa][1] == 'W') {
				fa = nfa;
				if (na == NULL) usage();
				wweight = atof(na);

			} else if (argv[fa][1] == 'r' || argv[fa][1] == 'R') {
				fa = nfa;
				if (na == NULL) usage();
				if (sscanf(na, " %d,%d,%d ", &gres[0], &gres[1], &gres[2]) != 2)
					usage();

			} else if (argv[fa][1] == 'h' || argv[fa][1] == 'H') {
				doh = 1;

			} else if (argv[fa][1] == 'q' || argv[fa][1] == 'Q') {
				doh = 1;
				doq = 1;

			} else if (argv[fa][1] == 'p' || argv[fa][1] == 'P') {
				doplot = 1;

			} else if (argv[fa][1] == 's' || argv[fa][1] == 'S') {
				dosym = 1;

			} else if (argv[fa][1] == 'x') {
				autosm = 1;

			} else if (argv[fa][1] == 's') {
				dosym = 1;

			} else 
				usage();
		} else
			break;
	}


	if (autosm)
		flags |= RSPL_AUTOSMOOTH;

	if (dosym)
		flags |= RSPL_SYMDOMAIN;

	/* Create the object */
	rss =  new_rspl(RSPL_NOFLAGS, 3, 1);

	/* Fit to scattered data */
	rss->fit_rspl_df(rss,
	           flags,				/* Non-mon and clip flags */
	           test_points,			/* Test points */
	           npoints,				/* Number of test points */
	           low, high, gres,		/* Low, high, resolution of grid */
	           low, high,			/* Default data scale */
	           1.0,					/* Smoothing */
	           avgdev,				/* Average Deviation */
	           NULL,				/* iwidth */
               wweight,				/* weak function weight */
	           NULL,				/* No context */
	           wfunc				/* Weak function */
	);
	if (doh) {

		if (doq) {
			gres2[0] = gres[0]/4;
			gres2[1] = gres[1]/4;
			gres2[2] = gres[2]/4;
		} else {
			gres2[0] = gres[0]/2;
			gres2[1] = gres[1]/2;
			gres2[2] = gres[2]/2;
		}

		rss2 =  new_rspl(RSPL_NOFLAGS, 3, 1);

		/* Fit to scattered data */
		rss2->fit_rspl_df(rss2,
		           flags,				/* Non-mon and clip flags */
		           test_points,			/* Test points */
		           npoints,				/* Number of test points */
		           low, high, gres2,	/* Low, high, resolution of grid */
		           low, high,			/* Default data scale */
		           1.0, 				/* Smoothing */
		           avgdev,				/* Average Deviation */
		           NULL,				/* iwidth */
                   wweight,				/* weak function weight */
	               NULL,				/* No context */
	               wfunc				/* Weak function */
	);
	}

	/* Test the interpolation with a slice in 2D */
	for (rsv = 0; rsv <= doh; rsv++) {
		double z[2][2] = { { 0.1, 0.5 }, { 0.5, 0.9 } }; 
		double x1 = -0.2;
		double x2 = 1.2;
		double y1 = -0.2;
		double y2 = 1.2;
		double min = -0.0;
		double max = 1.0;
		rspl *rs;
		unsigned char pa[HEIGHT][WIDTH][3];
		co tco;	/* Test point */
		double sx,sy;
		int i,j,k;
	
		if (rsv == 0)
			rs = rss;
		else
			rs = rss2;

		sx = (x2 - x1)/(double)WIDTH;
		sy = (y2 - y1)/(double)HEIGHT;
		
		for (j=0; j < HEIGHT; j++) {
			double jj = j/(HEIGHT-1.0);
			tco.p[1] = (double)((HEIGHT-1) - j) * sy + y1;
			for (i = 0; i < WIDTH; i++) {
			double ii = j/(HEIGHT-1.0);
				tco.p[0] = (double)i * sx + x1;

				tco.p[2] = (1.0-ii) * (1.0-jj) * z[0][0]
				         + (1.0-ii) *      jj  * z[0][1]
				         +      ii  * (1.0-jj) * z[1][0]
				         +      ii  *      jj  * z[1][1];

				if (rs->INTERP(rs, &tco)) {
					pa[j][i][0] = 0;	/* Out of bounds in green */
					pa[j][i][1] = 100;
					pa[j][i][2] = 0;
				} else {
					int m;
	/* printf("%d %d, %f %f returned %f\n",i,j,tco.p[0],tco.p[1],tco.v[0]); */
					m = (int)((255.0 * (tco.v[0] - min)/(max - min)) + 0.5);
					if (m < 0) {
						pa[j][i][0] = 20;		/* Dark blue */
						pa[j][i][1] = 20;
						pa[j][i][2] = 50;
					} else if (m > 255) {
						pa[j][i][0] = 230;		/* Light blue */
						pa[j][i][1] = 230;
						pa[j][i][2] = 255;
					} else {
						pa[j][i][0] = m;	/* Level in grey */
						pa[j][i][1] = m;
						pa[j][i][2] = m;
					}
				}
			}
		}
	
		/* Mark verticies in red */
		for(k = 0; k < npoints; k++) {
			j = (int)((HEIGHT * (y2 - test_points[k].p[1])/(y2 - y1)) + 0.5);
			i = (int)((WIDTH * (test_points[k].p[0] - x1)/(x2 - x1)) + 0.5);
			pa[j][i][0] = 255;
			pa[j][i][1] = 0;
			pa[j][i][2] = 0;
		}
		write_rgb_tiff(rsv == 0 ? "t3d.tif" : "t3dh.tif" ,WIDTH,HEIGHT,(unsigned char *)pa);
	}

	/* Plot out 4 slices */
	if (doplot) {
		int slice;
		
		for (slice = 0; slice < 4; slice++) {
			co tp;	/* Test point */
			double x[PLOTRES];
			double ya[PLOTRES];
			double yb[PLOTRES];
			double xx,yy,zz;
			double x1,x2,y1,y2,z1,z2;
			double sx,sy,sz;
			int i,n;
			
			/* Set up slice to plot */
			if (slice == 0) {
				x1 = 0.5; y1 = 0.5, z1 = 0.0;
				x2 = 0.5; y2 = 0.5, z2 = 1.0;
				printf("Plot along z at x = y = 0.5\n");
				n = PLOTRES;
			} else if (slice == 1) {
				x1 = 0.0; y1 = 0.5, z1 = 0.5;
				x2 = 1.0; y2 = 0.5, z2 = 0.5;
				printf("Plot along x at y = z = 0.5\n");
				n = PLOTRES;
			} else if (slice == 2) {
				x1 = 0.5; y1 = 0.0, z1 = 0.5;
				x2 = 0.5; y2 = 1.0, z2 = 0.5;
				printf("Plot along y at x = z = 0.5\n");
				n = PLOTRES;
			} else {
				x1 = 0.0; y1 = 0.0, z1 = 0.0;
				x2 = 1.0; y2 = 1.0, z2 = 1.0;
				printf("Plot along x = y = z\n");
				n = PLOTRES;
			}

			sx = (x2 - x1)/n;
			sy = (y2 - y1)/n;
			sz = (z2 - z1)/n;
			
			xx = x1;
			yy = y1;
			zz = z1;
			for (i = 0; i < n; i++) {
				double vv = i/(n-1.0);
				x[i] = vv;
				tp.p[0] = xx;
				tp.p[1] = yy;
				tp.p[2] = zz;

				if (rss->INTERP(rss, &tp))
					tp.v[0] = -0.1;
				ya[i] = tp.v[0];

				if (doh) {
					if (rss2->INTERP(rss2, &tp))
						tp.v[0] = -0.1;
					yb[i] = tp.v[0];
				}

				xx += sx;
				yy += sy;
				zz += sz;
			}

			/* Plot the result */
			if (doh)
				do_plot(x,ya,yb,NULL,n);
			else
				do_plot(x,ya,NULL,NULL,n);
		}
	}

	/* Report the fit */
	{
		co tco;	/* Test point */
		int k;
		double avg = 0;
		double max = 0.0;
	
		for(k = 0; k < npoints; k++) {
			double err;
			tco.p[0] = test_points[k].p[0];
			tco.p[1] = test_points[k].p[1];
			tco.p[2] = test_points[k].p[2];
			rss->INTERP(rss, &tco);

			err = tco.v[0] - test_points[k].v[0];
			err = fabs(err);

			avg += err;
			if (err > max)
				max = err;
		}
		avg /= (double)npoints;
		printf("Max error %f%%, average %f%%\n",100.0 * max, 100.0 * avg);
	}
	return 0;
}

/* ---------------------- */
/* Tiff diagnostic output */

void
write_rgb_tiff(
char *name,
int width,
int height,
unsigned char *data
) {
	int y;
	unsigned char *dp;
	TIFF *tif;

	if ((tif = TIFFOpen(name, "w")) == NULL) {
		fprintf(stderr,"Failed to open output TIFF file '%s'\n",name);
		exit (-1);
		}

	TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,  width);
	TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
	TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);

	for (dp = data, y = 0; y < height; y++, dp += 3 * width) {
		if (TIFFWriteScanline(tif, (tdata_t)dp, y, 0) < 0) {
			fprintf(stderr,"WriteScanline Failed at line %d\n",y);
			exit (-1);
		}
	}
	(void) TIFFClose(tif);
}

#ifdef NEVER
/******************************************************************/
/* Error/debug output routines */
/******************************************************************/

/* Basic printf type error() and warning() routines */

#ifdef	__STDC__
void
error(char *fmt, ...)
#else
void
error(va_alist) 
va_dcl
#endif
{
	va_list args;
#ifndef	__STDC__
	char *fmt;
#endif

	fprintf(stderr,"cmatch: Error - ");
#ifdef	__STDC__
	va_start(args, fmt);
#else
	va_start(args);
	fmt = va_arg(args, char *);
#endif
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	fflush(stdout);
	exit (-1);
}

#ifdef	__STDC__
void
warning(char *fmt, ...)
#else
void
warning(va_alist) 
va_dcl
#endif
{
	va_list args;
#ifndef	__STDC__
	char *fmt;
#endif

	fprintf(stderr,"cmatch: Warning - ");
#ifdef	__STDC__
	va_start(args, fmt);
#else
	va_start(args);
	fmt = va_arg(args, char *);
#endif
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

#ifdef	__STDC__
void
verbose(int level, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
#else
verbose(va_alist) 
va_dcl
{
	va_list args;
	int   level;
	char *fmt;
	va_start(args);
	level = va_arg(args, int);
	fmt = va_arg(args, char *);
#endif
	if (verbose_level >= level)
		{
		fprintf(verbose_out,"cmatch: ");
		vfprintf(verbose_out, fmt, args);
		fprintf(verbose_out, "\n");
		fflush(verbose_out);
		}
	va_end(args);
}
#endif /* NEVER */
