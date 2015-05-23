
/************************************************/
/* Test RSPL in 3/4D */
/************************************************/

/* Author: Graeme Gill
 * Date:   22/4/96
 * Derived from cmatch.c
 * Copyright 1995 - 2000 Graeme W. Gill
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */


#undef DEBUG
#undef DETAILED

#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include "rspl.h"
#include "numlib.h"
#include "tiffio.h"
#include "plot.h"

#ifdef NEVER
FILE *verbose_out = stdout;
int verbose_level = 6;			/* Current verbosity level */
								/* 0 = none */
								/* !0 = diagnostics */
#endif /* NEVER */

#define spline_interp interp

/* rspl flags */
#define FLAGS (0 /* */)

#define TEST_FWD_2D
#define TEST_REV_LOOKUP
#undef TEST_SLICE
#undef TEST_RANDOM_POINTS

#ifdef TEST_SLICE
# include "ui.h"
#endif

#define MAX_ITS 500
#define IT_TOL 0.0005
#define GRES 17			/* Grid resolution */
#define DI 4			/* Dimensions in */
#define FDI 4			/* Function (out) Dimensions */
#undef NEVER
#define ALWAYS

/* Arbitrary values */
static co test_points[] = {
	{{ 0.1,0.1,0.5,0.0 },{ 0.6, 0.2, 0.3, 0.99 }},	/* 0 */
	{{ 0.2,0.7,0.1,0.3 },{ 0.3, 0.1, 0.1, 0.45 }},	/* 1 */
	{{ 0.8,0.8,0.8,0.2 },{ 0.1, 0.7, 0.7, 0.7  }}, 	/* 2 */
	{{ 0.5,0.6,0.4,0.9 },{ 0.7, 0.6, 0.5, 0.4 }},	/* 3 */
	{{ 0.2,0.5,0.2,0.7 },{ 0.2, 0.3, 0.2, 0.2 }},	/* 4 */
	{{ 0.3,0.7,0.2,0.8 },{ 0.8, 0.9, 0.3, 0.5 }},	/* 5 */
	{{ 0.5,0.4,0.9,0.3 },{ 0.6, 0.4, 0.2, 0.01 }},	/* 6 */
	{{ 0.1,0.9,0.7,0.4 },{ 1.0, 0.9, 0.3, 0.6 }},	/* 7 */
	{{ 0.7,0.2,0.1,0.3 },{ 0.2, 0.3, 0.7, 0.3 }},	/* 8 */
	{{ 0.8,0.4,0.3,0.7 },{ 0.4, 0.5, 0.6, 0.2 }},	/* 9 */
	{{ 0.3,0.3,0.4,0.1 },{ 0.8, 0.6, 0.8, 0.1 }}	/* 10 */
	};

#ifdef NEVER
/* Inverting table */
static co test_points[] = {
	{{ 0.1,0.1,0.5,0.0 },{ 0.9, 0.9, 0.5, 1.0 }},	/* 0 */
	{{ 0.2,0.7,0.1,0.3 },{ 0.8, 0.3, 0.9, 0.7 }},	/* 1 */
	{{ 0.8,0.8,0.8,0.2 },{ 0.2, 0.2, 0.2, 0.8 }}, 	/* 2 */
	{{ 0.5,0.6,0.4,0.9 },{ 0.5, 0.4, 0.6, 0.1 }},	/* 3 */
	{{ 0.2,0.5,0.2,0.7 },{ 0.8, 0.5, 0.8, 0.3 }},	/* 4 */
	{{ 0.3,0.7,0.2,0.8 },{ 0.7, 0.3, 0.8, 0.2 }},	/* 5 */
	{{ 0.5,0.4,0.9,0.3 },{ 0.5, 0.6, 0.1, 0.7 }},	/* 6 */
	{{ 0.1,0.9,0.7,0.4 },{ 0.9, 0.1, 0.3, 0.6 }},	/* 7 */
	{{ 0.7,0.2,0.1,0.3 },{ 0.3, 0.8, 0.9, 0.7 }},	/* 8 */
	{{ 0.8,0.4,0.3,0.7 },{ 0.2, 0.6, 0.7, 0.3 }},	/* 9 */
	{{ 0.3,0.3,0.4,0.1 },{ 0.7, 0.7, 0.6, 0.9 }}	/* 10 */
	};
#endif /* NEVER */

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

int main(int argc, char *argv[]) {
	co *tps = NULL;
	int ntps = 0;
	rspl *rss;	/* Multi-resolution regularized spline structure */
	datai low,high;
	int gres[MXDI];
	double avgdev[MXDO];
	low[0] = 0.0;
	low[1] = 0.0;
	low[2] = 0.0;
	low[3] = 0.0;
	high[0] = 1.0;
	high[1] = 1.0;
	high[2] = 1.0;
	high[3] = 1.0;
	gres[0] = GRES;
	gres[1] = GRES;
	gres[2] = GRES;
	gres[3] = GRES;
	avgdev[0] = 0.0;
	avgdev[1] = 0.0;
	avgdev[2] = 0.0;
	avgdev[3] = 0.0;

	/* Create the object */
	rss =  new_rspl(RSPL_NOFLAGS, DI,				/* di */
	                  FDI);				/* fdi */

#ifdef TEST_RANDOM_POINTS
	{
	int i;
	ntps = i_rand(30,150);
	tps = (co *)malloc(ntps * sizeof(co));
	for (i = 0; i < ntps; i++) {
		tps[i].p[0] = d_rand(0.0,1.0);
		tps[i].p[1] = d_rand(0.0,1.0);
		tps[i].p[2] = d_rand(0.0,1.0);
		tps[i].p[3] = d_rand(0.0,1.0);
		tps[i].v[0] = d_rand(0.0,1.0);
		tps[i].v[1] = d_rand(0.0,1.0);
		tps[i].v[2] = d_rand(0.0,1.0);
		tps[i].v[3] = d_rand(0.0,1.0);
	}
	}
#else
	tps = test_points;
	ntps = sizeof(test_points)/sizeof(co);
#endif 

	/* Fit to scattered data */
	rss->fit_rspl(rss,
	           FLAGS,				/* Non-mon and clip flags */
	           tps,					/* Test points */
	           ntps,				/* Number of test points */
	           low, high, gres,		/* Low, high, resolution of grid */
	           NULL, NULL,			/* Default data scale */
	           1.0,					/* Smoothing */
	           avgdev,				/* Average deviation */
	           NULL);				/* iwidth */
               /* IT_TOL, MAX_ITS); */

/*	verbose(1,"Regular spline fit error = %f\n",rss->efactor(rss,0)); */

	/* Do a quick check */
	{
		co tco;	/* Test point */
	 	int i,j;
		double df,sm;
		for (i = 0; i < ntps; i++) {
			for (j = 0; j < DI; j++)
				tco.p[j] = tps[i].p[j];

			rss->spline_interp(rss, &tco);
			sm = 0.0;
			for (j = 0; j < DI; j++) {
				df = tco.v[j] - tps[i].v[j];
				sm += df * df;
			}
			printf("Error at data point %d = %f\n",i,sqrt(sm));
		}
	}

#ifdef TEST_REV_LOOKUP
	{
#define NIP 10
	int i, r;
	double v[MXDO];		/* Target output value */
	co tp[NIP], chp;	/* Test point, check point */
	double cvec[4];		/* Text clip vector */
	int auxm[4];		/* Auxiliary target value valid flag */

	tp[0].v[0] = v[0] = 0.5;
	tp[0].v[1] = v[1] = 0.5;
	tp[0].v[2] = v[2] = 0.5;
	tp[0].v[3] = v[3] = 0.5;

	/* Set auxiliary target */
	auxm[0] = 0;
	auxm[1] = 0;
	auxm[2] = 1;
	auxm[3] = 0;
	tp[0].p[0] = -1.0;
	tp[0].p[1] = -1.0;
	tp[0].p[2] =  0.5;
	tp[0].p[3] = -1.0;

	for (i = 1; i < NIP; i++) {	/* Make sure we can see changes */
		tp[i].p[0] = -1.0;
		tp[i].p[1] = -1.0;
		tp[i].p[2] = -1.0;
		tp[i].p[3] = -1.0;
	}

	/* Clip center */
	cvec[0] = 0.0 - tp[0].v[0];
	cvec[1] = 0.0 - tp[0].v[1];
	cvec[2] = 0.0 - tp[0].v[2];
	cvec[3] = 0.0 - tp[0].v[3];

	/* Do reverse interpolation ~~~1 */
	if ((r = rss->rev_interp(rss, 0, NIP, auxm, NULL /*cvec*/, tp)) > 0) {
		printf("Total of %d Results\n",r);
		for (i = 0; i < r; i++)
			printf("Result %d = %f, %f, %f, %f\n",i, tp[i].p[0],tp[i].p[1],tp[i].p[2],tp[i].p[3]);

		/* Check test result */
		for (i = 0; i < r; i++) {
			chp.p[0] = tp[i].p[0];
			chp.p[1] = tp[i].p[1];
			chp.p[2] = tp[i].p[2];
			chp.p[3] = tp[i].p[3];
			chp.v[0] = -1.0;
			chp.v[1] = -1.0;
			chp.v[2] = -1.0;
			chp.v[3] = -1.0;
			if (rss->interp(rss, &chp))
				printf("Fwd check %d failed!\n",i);
			else {
				int p;
				double er = 0.0;
				for (p = 0; p < FDI; p++)
					er += (v[p] - chp.v[p]) *  (v[p] - chp.v[p]);
				printf("Fwd check error %d = %f\n",i,er);
			}
		}
	} else
		printf("Rev lookup result returned none\n");
	}
#endif /* TEST_REV_LOOKUP */

#ifdef TEST_SLICE
	/* Test the interpolation */
	{
	co tp;	/* Test point */
	double x[50000];
	double y[50000];
	double ya[50000];
	double xx,yy;
	double x1,x2,y1,y2;
	double sx,sy;
	int i,j,n;

	/* Set up slice to plot */
	x1 = 0.1; y1 = 0.5;		/* ~4 */
	x2 = 0.9; y2 = 0.5;
	n = 100;

	sx = (x2 - x1)/n;
	sy = (y2 - y1)/n;
	
	xx = x1;
	yy = y1;
	for (j = i = 0; i < n; i++)
		{
		tp.p[0] = xx;
		tp.p[1] = yy;
		if (rss->spline_interp(rss, &tp))
			{
			tp.v[0] = -0.1;
			}
		x[j] = xx;
		y[j] = tp.v[0];
		j++;
		xx += sx;
		yy += sy;
		}

	/* Plot the result */
	do_plot(x,y,NULL,NULL,j);
	}
#endif /* TEST_SLICE */

#ifdef TEST_FWD_2D
	/* Test the interpolation in 2D */
	{
#define WIDTH 200
#define HEIGHT 200
	double x1 = -0.2;
	double x2 = 1.2;
	double y1 = -0.2;
	double y2 = 1.2;
	double min = -0.0;
	double max = 1.0;

	unsigned char pa[HEIGHT][WIDTH][3];
	co tco;	/* Test point */
	double sx,sy;
	int i,j,k;

	sx = (x2 - x1)/(double)WIDTH;
	sy = (y2 - y1)/(double)HEIGHT;
	
	tco.p[2] = 0.5;		/* Set slice */
	tco.p[3] = 0.5;
	for (j=0; j < HEIGHT; j++)
		{
		tco.p[1] = (double)((HEIGHT-1) - j) * sy + y1;
		for (i=0; i < WIDTH; i++)
			{
			tco.p[0] = (double)i * sx + x1;
			if (rss->spline_interp(rss, &tco))
				{
				pa[j][i][0] = 0;	/* Out of bounds in green */
				pa[j][i][1] = 100;
				pa[j][i][2] = 0;
				}
			else
				{
				int m;
/* printf("%d %d, %f %f returned %f\n",i,j,tco.p[0],tco.p[1],tco.v[0]); */
				m = (int)((255.0 * (tco.v[0] - min)/(max - min)) + 0.5);
				if (m < 0)
					{
					pa[j][i][0] = 0;		/* Dark blue */
					pa[j][i][1] = 0;
					pa[j][i][2] = 40;
					}
				else if (m > 255)
					{
					pa[j][i][0] = 220;		/* Light blue */
					pa[j][i][1] = 220;
					pa[j][i][2] = 255;
					}
				else
					{
					pa[j][i][0] = m;	/* Level in grey */
					pa[j][i][1] = m;
					pa[j][i][2] = m;
					}
				}
			}
		}

	/* Mark verticies in red */
	for(k = 0; k < ntps; k++)
		{
		j = (int)((HEIGHT * (y2 - tps[k].p[1])/(y2 - y1)) + 0.5);
		i = (int)((WIDTH * (tps[k].p[0] - x1)/(x2 - x1)) + 0.5);
		pa[j][i][0] = 255;
		pa[j][i][1] = 0;
		pa[j][i][2] = 0;
		}

	write_rgb_tiff("tnd.tif",WIDTH,HEIGHT,(unsigned char *)pa);
	}
#endif /* TEST_FWD_2D */
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
