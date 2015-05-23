
/* 
 * Argyll Gamut Mapping Library
 *
 * Author:  Graeme W. Gill
 * Date:    1/10/00
 * Version: 2.00
 *
 * Copyright 2000 - 2006 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * For a discussion of gamut mapping strategy used,
 * see gammap.txt
 */

/*
 * TTBD:
 *       Improve error handling.
 *
 *	There is a general expectation (especially in comparing products)
 *  that the profile colorimetric intent be not strictly minimum delta E,
 *  but that it correct neutral axis, luminence range and keep hue
 *  proportionality. Ideally there should be an intent that matches
 *  this, that can be selected for the colorimetric table (or perhaps be default).
 *
 *	It might be good to offer the black mapping method as an option (icx_BPmap),
 *  as well as offering different profile (xicc/xlut.c) black point options
 *  (neutral, K hue max density, CMY max density, any max density).
 *  Perhaps print RGB & CMY should default to neutral black, rather than 0,0,0 ??
 *
 *  The gamut mapping code here and the near smooth code don't actually mesh
 *  very well. For instance, the black point bend approach in < V1.3.4
 *  means that the dest gamut isn't actually contained within the source,
 *  messing up the guide vector mappings. Even if this is fixed, the
 *  actual neutral aim point within nearsmooth is Jab 0,0, while
 *  the mapping in gammap is from the source neutral to the chosen
 *  ??????
 */



#define VERBOSE			/* [Def] Print out extra interesting information when verbose is set */
#undef PLOT_DIAG_WRL	/* [Und] Always plot "gammap.wrl" */

	/* What do display when user requests disgnostic VRML/X3D */
#define PLOT_SRC_GMT	/* [Def] Plot the source surface to "gammap.wrl" as well */
#define PLOT_DST_GMT	/* [Def] Plot the dest surface to "gammap.wrl" as well */
#undef PLOT_SRC_CUSPS	/* [Und] Plot the source surface cusps to "gammap.wrl" as well */
#undef PLOT_DST_CUSPS	/* [Und] Plot the dest surface cusps to "gammap.wrl" as well */
#undef PLOT_TRANSSRC_CUSPS	/* [Und] Plot the gamut mapped source surface cusps to "gammap.wrl" */
#define PLOT_AXES		/* [Und] Plot the axes to "gammap.wrl" as well */
#undef SHOW_VECTOR_INDEXES	/* [Und] Show the mapping vector index numbers */
#define SHOW_MAP_VECTORS	/* [Def] Show the mapping vectors - yellow to red, green to red */
							/*       if no clear direction. */
#undef SHOW_SUB_SURF	/* [Und] Show the sub-surface mapping vector - grey to purple. */
#undef SHOW_SUB_PNTS	/* [Und] Show the sub-surface sv2 (red), div2 (green), sd3 (yellow) pnts */
#undef SHOW_CUSPMAP		/* [Und] Show the cusp mapped vectors rather than final vectors */
#undef SHOW_ACTUAL_VECTORS		/* [Und] Show how the source vectors actually map thought xform */
#undef SHOW_ACTUAL_VEC_DIFF		/* [Und] Show how the difference between guide and actual vectors */

	/* Other diagnostics */
#undef PLOT_LMAP		/* [Und] Plot L map */
#undef PLOT_GAMUTS		/* [Und] Save (part mapped) input and output gamuts as */
						/* src.wrl, img.wrl, dst.wrl, gmsrc.wrl */
#undef PLOT_3DKNEES		/* [Und] Plot the 3D compression knees */
#undef CHECK_NEARMAP	/* [Und] Check how accurately near map vectors are represented by rspl */

#define USE_GLUMKNF		/* [Define] Enable luminence knee function points */
#define USE_GREYMAP		/* [Define] Enable 3D->3D mapping points down the grey axis */
#define USE_GAMKNF		/* [Define] Enable 3D knee function points */
#define USE_BOUND		/* [Define] Enable grid boundary anchor points */

#undef SHOW_NEIGBORS	/* [Und] Show nearsmth neigbors in gammap.wrl */

#undef PLOT_DIGAM		/* [Und] Rather than DST_GMT - don't free it (#def in nearsmth.c too) */


#define XRES 100		/* [100] Res of plots */

/* The locus.ts file can contain source locus(es) that will be plotted */
/* as cones in red, with the destination plotted in white. They can */
/* be created from .tif files using xicc/tiffgmts utility. */

/* Optional marker points for gamut mapping diagnosotic */
struct {
	int type;		/* 1 = src point (xlate), 2 = dst point (no xlate) */
					/* 0 = end marker */
	double pos[3];	/* Position, (usually in Jab space) */
	double col[3];	/* RGB color */
} markers[] = {
	{ 0, },								/* End marker */
	{ 1, { 37.18, 17.78, 20.28 }, { 0.545, 0.357, 0.256 } },	/* Dark Baby skin */
	{ 1, { 12.062, -0.87946, 0.97008 }, { 1.0, 0.3, 0.3 } },	/* Black point */
	{ 1, { 67.575411, -37.555250, -36.612862 }, { 1.0, 0.3, 0.3 } },	/* bad source in red (Red) */
	{ 1, { 61.003078, -44.466554, 1.922585 }, { 0.0, 1.0, 0.3 } },	/* good source in green */
	{ 2, { 49.294793, 50.749543, -51.383167 }, { 1.0, 0.0, 0.0 } },	
	{ 2, { 42.783425, 49.089363, -37.823712 }, { 0.0, 1.0, 0.0 } },	
	{ 2, { 41.222695, 63.911823, 37.695310 }, { 0.0, 1.0, 0.3 } },	/* destination in green */
	{ 1, { 41.951770, 60.220284, 34.788195 }, { 1.0, 0.3, 0.3 } },	/* source in red (Red) */
	{ 2, { 41.222695, 63.911823, 37.695310 }, { 0.3, 1.3, 0.3 } },		/* Dest in green */
	{ 1, { 85.117353, -60.807580, -22.195118 }, { 0.3, 0.3, 1 } },		/* Cyan Source (Blue) */
	{ 2, { 61.661622, -38.164411, -18.090824 }, { 1.0, 0.3, 0.3 } },	/* CMYK destination (Red) */
	{ 0 }								/* End marker */
};

/* Optional marker rings for gamut mapping diagnosotic */
struct {
	int type;		/* 1 = src ring point, 2 = ignore, */
					/* 0 = end marker */
	double ppoint[3];	/* Location of a point on the plane in source space */
	double pnorm[3];	/* Plane normal direction in source space */
	int    nverts;		/* Number of points to make ring */
	double rad;			/* Relative Radius from neutral to source surface (0.0 - 1.0) */
	double scol[3];	/* Source RGB color */
	double dcol[3];	/* Destination RGB color */
} rings[] = {
	{ 0 },								/* End marker */
	{ 1,
		{ 60.0, 0.0, 0.0 }, { 1.0, 0.8, 0.0 },		/* plane point and normal */
		100, 1.0,									/* 20 vertexes at source radius */
		{ 0.0, 1.0, 0.0 },			/* Green source */
		{ 1.0, 0.0, 0.0 }			/* Red destination */
	},
	{ 1,
		{ 60.0, 0.0, 0.0 }, { 1.0, 0.8, 0.0 },		/* plane point and normal */
		100, 0.9,									/* 20 vertexes at source radius */
		{ 0.0, 1.0, 0.0 },			/* Green source */
		{ 1.0, 0.0, 0.0 }			/* Red destination */
	},
	{ 1,
		{ 60.0, 0.0, 0.0 }, { 1.0, 0.8, 0.0 },		/* plane point and normal */
		100, 0.8,									/* 20 vertexes at source radius */
		{ 0.0, 1.0, 0.0 },			/* Green source */
		{ 1.0, 0.0, 0.0 }			/* Red destination */
	},
	{ 0 }								/* End marker */
};

/* Degree to which the hue & saturation of the black point axes should be aligned: */
#define GREYBPHSMF 0.0

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "counters.h"
#include "icc.h"
#include "numlib.h"
#include "xicc.h"
#include "gamut.h"
#include "rspl.h"
#include "gammap.h"
#include "nearsmth.h"
#include "vrml.h"
#ifdef PLOT_LMAP
#include "plot.h"
#endif

/* Callback context for enhancing the saturation of the clut values */
typedef struct {
	gamut *dst;			/* Destination colorspace gamut */
	double wp[3], bp[3];/* Destination colorspace white and black points */
	double satenh;		/* Saturation engancement value */
} adjustsat;

/* Callback context for making clut relative to white and black points */
typedef struct {
	double mat[3][4];
} adjustwb;

static void inv_grey_func(void *pp, double *out, double *in);
static void adjust_wb_func(void *pp, double *out, double *in);
static void adjust_sat_func(void *pp, double *out, double *in);

#define XVRA 3.0	/* [3.0] Extra mapping vertex ratio over no. tri verts from gamut */

/* The smoothed near weighting control values. */
/* These weightings setup the detailed behaviour of the */
/* gamut mapping for the fully perceptual and saturation intents. */
/* They are ordered here by increasing priority. A -ve value is ignored */

/* Perceptual mapping weights, where smoothness and proportionality are important.. */
gammapweights pweights[] = {
	{
		gmm_default,	/* Non hue specific defaults */
		{				/* Cusp alignment control */
			{
				0.1,	/* Cusp luminance alignment weighting 0 = none, 1 = full */
				0.0,	/* Cusp chroma alignment weighting    0 = none, 1 = full */
				0.3		/* Cusp hue alignment weighting       0 = none, 1 = full */
			},
			2.0,		/* Alignment twist power, 0 = linear, 1 = curve, 2+ late curve */
			1.00		/* Chroma expansion 1 = none */
		},
		{			/* Radial weighting (currently broken - need to fix) */
			0.0,	/* Radial error overall weight, 0 + */
			0.5,	/* Radial hue dominance vs l+c, 0 - 1 */
			0.5		/* Radial l dominance vs, c, 0 - 1 */
		},
		{			/* Weighting of absolute error of destination from source */
			1.0,	/* Absolute error overall weight */
			0.8,	/* Hue dominance vs l+c, 0 - 1 */

			0.8,	/* White l dominance vs, c, 0 - 1 */
			0.5,	/* Grey l dominance vs, c, 0 - 1 */
			0.97,	/* Black l dominance vs, c, 0 - 1 */

			0.4,	/* White l blend start radius, 0 - 1, at white = 0 */
			0.7,	/* Black l blend power, linear = 1.0, enhance < 1.0 */

			1.5,	/* L error extra power with size, none = 1.0 */
			10.0	/* L error extra xover threshold in DE */
		},
		{			/* Relative vector  smoothing */
			25.0, 35.0	/* Relative Smoothing radius L* H* */
		},
		{		/* Weighting of excessive compression error, which is */
				/* the src->dst vector length over the available dst depth. */
				/* The depth is half the distance to the intersection of the */
				/* vector to the other side of the gamut. (doesn't get triggered much ?) */
			10.0,		/* Compression depth weight */
			10.0		/* Expansion depth weight */
		},
		{
			0.0			/* Fine tuning expansion weight, 0 - 1 */
		}
	},
	{
		gmm_light_yellow,		/* Treat yellow differently, to get purer result. */
		{
			{
				0.9,	/* Cusp luminance alignment weighting 0 = none, 1 = full */
				0.8,	/* Cusp chroma alignment weighting    0 = none, 1 = full */
				0.7		/* Cusp hue alignment weighting       0 = none, 1 = full */
			},
			4.0,		/* Alignment twist power, 0 = linear, 1 = curve, 2+ late curve */
			1.20		/* Chroma expansion 1 = none */
		},
		{			/* Radial weighting */
			-1.0,	/* Radial error overall weight, 0 + */
			-1.0,	/* Radial hue dominance vs l+c, 0 - 1 */
			-1.0	/* Radial l dominance vs, c, 0 - 1 */
		},
		{			/* Weighting of absolute error of destination from source */
			-1.0,	/* Absolute error overall weight */
			-1.0,	/* Hue dominance vs l+c, 0 - 1 */

			-1.0,	/* White l dominance vs, c, 0 - 1 */
			-1.0,	/* Grey l dominance vs, c, 0 - 1 */
			-1.0,	/* Black l dominance vs, c, 0 - 1 */

			-1.0,	/* White l threshold ratio to grey distance, 0 - 1 */
			-1.0,	/* Black l threshold ratio to grey distance, 0 - 1 */

			-1.0,	/* L error extra power, none = 1.0 */
			-1.0	/* L error xover threshold in DE */
		},
		{			/* Relative error preservation using smoothing */
			20.0, 10.0	/* Relative Smoothing radius L* H* */
		},
		{		/* Weighting of excessive compression error, which is */
				/* the src->dst vector length over the available dst depth. */
				/* The depth is half the distance to the intersection of the */
				/* vector to the other side of the gamut. (doesn't get triggered much ?) */
			-1.0,		/* Compression depth weight */
			-1.0		/* Expansion depth weight */
		},
		{
			0.5			/* Fine tuning expansion weight, 0 - 1 */
		}
	},
#ifdef NEVER
	{
		gmm_l_d_blue,		/* Increase maintaining hue importance for blue */
		{
			{
				-1.0,	/* Cusp luminance alignment weighting 0 = none, 1 = full */
				-1.0,	/* Cusp chroma alignment weighting    0 = none, 1 = full */
				 0.0	/* Cusp hue alignment weighting       0 = none, 1 = full */
			},
			-1.0,		/* 2.0 Alignment twist power, 0 = linear, 1 = curve, 2+ late curve */
			-1.0		/* Chroma expansion 1 = none */
		},
		{			/* Radial weighting */
			-1.0,	/* Radial error overall weight, 0 + */
			-1.0,	/* Radial hue dominance vs l+c, 0 - 1 */
			-1.0	/* Radial l dominance vs, c, 0 - 1 */
		},
		{			/* Weighting of absolute error of destination from source */
			-1.0,	/* Absolute error overall weight */
			 0.8,	/* Hue dominance vs l+c, 0 - 1 */

			-1.0,	/* White l dominance vs, c, 0 - 1 */
			-1.0,	/* Grey l dominance vs, c, 0 - 1 */
			-1.0,	/* Black l dominance vs, c, 0 - 1 */

			-1.0,	/* White l threshold ratio to grey distance, 0 - 1 */
			-1.0,	/* Black l threshold ratio to grey distance, 0 - 1 */

			-1.0,	/* L error extra power, none = 1.0 */
			-1.0	/* L error xover threshold in DE */
		},
		{			/* Relative error preservation using smoothing */
			-1.0, 15.0	/* Relative Smoothing radius L* H* */
		},
		{		/* Weighting of excessive compression error, which is */
				/* the src->dst vector length over the available dst depth. */
				/* The depth is half the distance to the intersection of the */
				/* vector to the other side of the gamut. (doesn't get triggered much ?) */
			-1.0,		/* Compression depth weight */
			-1.0		/* Expansion depth weight */
		},
		{
			-1.0		/* Fine tuning expansion weight, 0 - 1 */
		}
	},
#endif /* NEVER */
	{
		gmm_end,
	}
};
double psmooth = 4.0;		/* [5.0] Level of RSPL smoothing for perceptual, 1 = nominal */

/* Saturation mapping weights, where saturation has priority over smoothness */
gammapweights sweights[] = {
	{
		gmm_default,	/* Non hue specific defaults */
		{				/* Cusp alignment control */
			{
				0.6,	/* Cusp luminance alignment weighting 0 = none, 1 = full */
				0.5,	/* Cusp chroma alignment weighting    0 = none, 1 = full */
				0.6		/* Cusp hue alignment weighting       0 = none, 1 = full */
			},
			1.0,		/* Alignment twist power, 0 = linear, 1 = curve, 2+ late curve */
			1.05		/* Chroma expansion 1 = none */
		},
		{			/* Radial weighting */
			0.0,	/* Radial error overall weight, 0 + */
			0.5,	/* Radial hue dominance vs l+c, 0 - 1 */
			0.5		/* Radial l dominance vs, c, 0 - 1 */
		},
		{			/* Weighting of absolute error of destination from source */
			1.0,	/* Absolute error overall weight */
			0.4,	/* Hue dominance vs l+c, 0 - 1 */

			0.6,	/* White l dominance vs, c, 0 - 1 */
			0.4,	/* Grey l dominance vs, c, 0 - 1 */
			0.6,	/* Black l dominance vs, c, 0 - 1 */

			0.5,	/* wl blend start radius, 0 - 1 */
			1.0,	/* bl blend power, linear = 1.0, enhance < 1.0 */

			1.0,	/* L error extra power with size, none = 1.0 */
			10.0	/* L error extra xover threshold in DE */
		},
		{			/* Relative vector  smoothing */
			20.0, 25.0	/* Relative Smoothing radius L* H* */
		},
		{		/* Weighting of excessive compression error, which is */
				/* the src->dst vector length over the available dst depth. */
				/* The depth is half the distance to the intersection of the */
				/* vector to the other side of the gamut. (doesn't get triggered much ?) */
			10.0,		/* Compression depth weight */
			10.0		/* Expansion depth weight */
		},
		{
			0.5			/* Fine tuning expansion weight, 0 - 1 */
		}
	},
	{
		gmm_light_yellow,		/* Treat yellow differently, to get purer result. */
		{
			{
				1.0,	/* Cusp luminance alignment weighting 0 = none, 1 = full */
				1.0,	/* Cusp chroma alignment weighting    0 = none, 1 = full */
				1.0		/* Cusp hue alignment weighting       0 = none, 1 = full */
			},
			1.0,		/* Alignment twist power, 0 = linear, 1 = curve, 2+ late curve */
			1.20		/* Chroma expansion 1 = none */
		},
		{			/* Radial weighting */
			-1.0,	/* Radial error overall weight, 0 + */
			-1.0,	/* Radial hue dominance vs l+c, 0 - 1 */
			-1.0	/* Radial l dominance vs, c, 0 - 1 */
		},
		{			/* Weighting of absolute error of destination from source */
			1.0,	/* Absolute error overall weight */
			0.3,	/* Hue dominance vs l+c, 0 - 1 */

			-1.0,	/* White l dominance vs, c, 0 - 1 */
			-1.0,	/* Grey l dominance vs, c, 0 - 1 */
			-1.0,	/* Black l dominance vs, c, 0 - 1 */

			-1.0,	/* White l threshold ratio to grey distance, 0 - 1 */
			-1.0,	/* Black l threshold ratio to grey distance, 0 - 1 */

			-1.0,	/* L error extra power, none = 1.0 */
			-1.0	/* L error xover threshold in DE */
		},
		{			/* Relative error preservation using smoothing */
			10.0, 15.0	/* Relative smoothing radius */
		},
		{		/* Weighting of excessive compression error, which is */
				/* the src->dst vector length over the available dst depth. */
				/* The depth is half the distance to the intersection of the */
				/* vector to the other side of the gamut. (doesn't get triggered much ?) */
			-1.0,		/* Compression depth weight */
			-1.0		/* Expansion depth weight */
		},
		{
			-1.0		/* Fine tuning expansion weight, 0 - 1 */
		}
	},
	{
		gmm_end
	}
};
double ssmooth = 2.0;		/* Level of RSPL smoothing for saturation */

/*
 * Notes:
 *       The "knee" shape produced by the rspl (regular spline) code
 *       is not what one would expect for expansion. It is not
 *       symetrical with compression, and is less "sharp". This
 *       is due to the rspl "smoothness" criteria being based on
 *       grid value difference rather than smoothness being measured,
 *       as curvature. This means that the spline gets "stiffer" as
 *       it increases in slope.
 *       Possibly rspl could be improved in this respect ???
 *       (Doesn't matter for L compression now, because rspl is
 *       being inverted for expansion).
 */

static void del_gammap(gammap *s);
static void domap(gammap *s, double *out, double *in);
static void dopartialmap1(gammap *s, double *out, double *in);
static void dopartialmap2(gammap *s, double *out, double *in);
static gamut *parttransgamut(gammap *s, gamut *src);
static void invdomap1(gammap *s, double *out, double *in);
#ifdef PLOT_GAMUTS
static void map_trans(void *cntx, double out[3], double in[3]);
#endif

/* Return a gammap to map from the input space to the output space */
/* Return NULL on error. */
gammap *new_gammap(
	int verb,			/* Verbose flag */
	gamut *sc_gam,		/* Source colorspace gamut */
	gamut *si_gam,		/* Source image gamut (NULL if none) */
	gamut *d_gam,		/* Destination colorspace gamut */
	icxGMappingIntent *gmi,	/* Gamut mapping specification */
	int src_kbp,		/* Use K only black point as src gamut black point */
	int dst_kbp,		/* Use K only black point as dst gamut black point */
	int dst_cmymap,		/* masks C = 1, M = 2, Y = 4 to force 100% cusp map */
	int rel_oride,		/* 0 = normal, 1 = clip like, 2 = max relative */
	int    mapres,		/* Gamut map resolution, typically 9 - 33 */
	double *mn,			/* If not NULL, set minimum mapping input range */
	double *mx,			/* for rspl grid. */
	char *diagname		/* If non-NULL, write a gamut mapping diagnostic WRL */
) {
	gammap *s;			/* This */
	gamut *scl_gam;		/* Source colorspace gamut with rotation and L mapping applied */
	gamut *sil_gam;		/* Source image gamut with rotation and L mapping applied */

	double s_cs_wp[3];	/* Source colorspace white point */
	double s_cs_bp[3];	/* Source colorspace black point */
	double s_ga_wp[3];	/* Source (image) gamut white point */
	double s_ga_bp[3];	/* Source (image) gamut black point */
	double d_cs_wp[3];	/* Destination colorspace white point */
	double d_cs_bp[3];	/* Destination colorspace black point */

	double sr_cs_wp[3];	/* Source rotated colorspace white point */
	double sr_cs_bp[3];	/* Source rotated colorspace black point */
	double sr_ga_wp[3];	/* Source rotated (image) gamut white point */
	double sr_ga_bp[3];	/* Source rotated (image) gamut black point */
	double dr_cs_wp[3];	/* Target (gmi->greymf aligned) white point */
	double dr_cs_bp[3];	/* Target (gmi->greymf aligned) black point */
	double dr_be_bp[3];	/* Bend at start in source neutral axis direction */
						/* Target black point (Same as dr_cs_bp[] otherwise) */

	double sl_cs_wp[3];	/* Source rotated and L mapped colorspace white point */
	double sl_cs_bp[3];	/* Source rotated and L mapped colorspace black point */

	double s_mt_wp[3];	/* Overall source mapping target white point (used for finetune) */
	double s_mt_bp[3];	/* Overall source mapping target black point (used for finetune) */
	double d_mt_wp[3];	/* Overall destination mapping white point (used for finetune) */
	double d_mt_bp[3];	/* Overall destination mapping black point (used for finetune) */

	int defrgrid = 6;	/* mapping range surface default anchor point resolution */
	int nres = 512;		/* Neutral axis resolution */
	cow lpnts[10];		/* Mapping points to create grey axis map */
	int revrspl = 0;	/* Reverse grey axis rspl construction */
	int ngreyp = 0;		/* Number of grey axis mapping points */
	int ngamp = 0;		/* Number of gamut mapping points */
	double xvra = XVRA;	/* Extra ss vertex ratio to src gamut vertex count */
	int j;

#if defined(PLOT_LMAP) || defined(PLOT_GAMUTS) || defined(PLOT_3DKNEES)
# pragma message("################ A gammap.c PLOT is #defined #########################")
#endif

	if (verb) {
		xicc_dump_gmi(gmi);
		printf("Gamut map resolution: %d\n",mapres);
		if (si_gam != NULL)
			printf("Image gamut supplied\n");
	}

	/* Allocate the object */
	if ((s = (gammap *)calloc(1, sizeof(gammap))) == NULL)
		error("gammap: calloc failed on gammap object");

	/* Setup methods */
	s->del = del_gammap;
	s->domap = domap;
	s->invdomap1 = invdomap1;

	/* Now create everything */

	/* Grab the white and black points */
	if (src_kbp) {
		// ~~99 Hmm. Shouldn't this be colorspace rather than gamut ????
		if (sc_gam->getwb(sc_gam, NULL, NULL, NULL, s_cs_wp, NULL, s_cs_bp)) {
//		if (sc_gam->getwb(sc_gam, s_cs_wp, NULL, s_cs_bp, NULL, NULL, NULL))
			fprintf(stderr,"gamut map: Unable to read source colorspace white and black points\n");
			free(s);
			return NULL;
		}
	} else {
		if (sc_gam->getwb(sc_gam, NULL, NULL, NULL, s_cs_wp, s_cs_bp, NULL)) {
//		if (sc_gam->getwb(sc_gam, s_cs_wp, s_cs_bp, NULL, NULL, NULL, NULL))
			fprintf(stderr,"gamut map: Unable to read source colorspace white and black points\n");
			free(s);
			return NULL;
		}
	}

	/* If source space is source gamut */
	if (si_gam == NULL) {
		si_gam = sc_gam;
		for (j = 0; j < 3; j++) {
			s_ga_wp[j] = s_cs_wp[j];
			s_ga_bp[j] = s_cs_bp[j];
		}

	/* Else have explicit sourcegamut */
	} else {

		if (src_kbp) {
			if (si_gam->getwb(si_gam, NULL, NULL, NULL, s_ga_wp, NULL, s_ga_bp)) {
				fprintf(stderr,"gamut map: Unable to read source gamut white and black points\n");
				free(s);
				return NULL;
			}
		} else {
			if (si_gam->getwb(si_gam, NULL, NULL, NULL, s_ga_wp, s_ga_bp, NULL)) {
				fprintf(stderr,"gamut map: Unable to read source gamut white and black points\n");
				free(s);
				return NULL;
			}
		}

		/* Guard against silliness. Image must be within colorspace */
		if (s_ga_wp[0] > s_cs_wp[0]) {
			int j;
			double t;
#ifdef VERBOSE
			if (verb)
				printf("Fixing wayward image white point\n");
#endif
			t = (s_cs_wp[0] - s_ga_bp[0])/(s_ga_wp[0] - s_ga_bp[0]);
			for (j = 0; j < 3; j++)
				s_ga_wp[j] = s_ga_bp[j] + t * (s_ga_wp[j] - s_ga_bp[j]);

		}
		if (s_ga_bp[0] < s_cs_bp[0]) {
			int j;
			double t;
#ifdef VERBOSE
			if (verb)
				printf("Fixing wayward image black point\n");
#endif
			t = (s_cs_bp[0] - s_ga_wp[0])/(s_ga_bp[0] - s_ga_wp[0]);
			for (j = 0; j < 3; j++)
				s_ga_bp[j] = s_ga_wp[j] + t * (s_ga_bp[j] - s_ga_wp[j]);
		}
	}

	if (dst_kbp) {
		if (d_gam->getwb(d_gam, NULL, NULL, NULL, d_cs_wp, NULL, d_cs_bp)) {
			fprintf(stderr,"gamut map: Unable to read destination white and black points\n");
			free(s);
			return NULL;
		}
	} else {
		if (d_gam->getwb(d_gam, NULL, NULL, NULL, d_cs_wp, d_cs_bp, NULL)) {
			fprintf(stderr,"gamut map: Unable to read destination white and black points\n");
			free(s);
			return NULL;
		}
	}

#ifdef VERBOSE
	if (verb) {
		if (src_kbp)
			printf("Using Src K only black point\n");

		if (dst_kbp)
			printf("Using Dst K only black point\n");

		printf("Src colorspace white/black are %f %f %f, %f %f %f\n",
		s_cs_wp[0], s_cs_wp[1], s_cs_wp[2], s_cs_bp[0], s_cs_bp[1], s_cs_bp[2]);
	
		printf("Src gamut white/black are      %f %f %f, %f %f %f\n",
		s_ga_wp[0], s_ga_wp[1], s_ga_wp[2], s_ga_bp[0], s_ga_bp[1], s_ga_bp[2]);
	
		printf("Dst colorspace white/black are %f %f %f, %f %f %f\n",
		d_cs_wp[0], d_cs_wp[1], d_cs_wp[2], d_cs_bp[0], d_cs_bp[1], d_cs_bp[2]);
	}
#endif /* VERBOSE */

	/* ------------------------------------ */
	/* Figure out the destination grey axis alignment */
	/* This is all done using colorspace white & black points */
	{
		double t, svl, dvl;
		double wrot[3][3];			/* Rotation about 0,0,0 to match white points */
		double sswp[3], ssbp[3];	/* Temporary source white & black points */
		double fawp[3], fabp[3];	/* Fully adapted destination white & black */
		double hawp[3], habp[3];	/* Half (full white, not black) adapted destination w & b */

		/* - - - - - - - - - - - - - - - - - - - - - - - - - - - */
		/* The first task is to decide what our target destination */
		/* white and black points are going to be. */

		/* Figure out what our initial target destination white point is going to be: */

		/* Compute source white and black points with same L value as the destination */
		t = (d_cs_wp[0] - s_cs_bp[0])/(s_cs_wp[0] - s_cs_bp[0]);
		for (j = 0; j < 3; j++)
			sswp[j] = s_cs_bp[j] + t * (s_cs_wp[j] - s_cs_bp[j]);

		t = (d_cs_bp[0] - s_cs_wp[0])/(s_cs_bp[0] - s_cs_wp[0]);
		for (j = 0; j < 3; j++)
			ssbp[j] = s_cs_wp[j] + t * (s_cs_bp[j] - s_cs_wp[j]);

		/* The raw grey axis alignment target is a blend between the */
		/* source colorspace (NOT gamut) and the destination */
		/* colorspace. */

		for (j = 0; j < 3; j++) {
			dr_cs_wp[j] = gmi->greymf * d_cs_wp[j] + (1.0 - gmi->greymf) * sswp[j];
			dr_cs_bp[j] = gmi->greymf * d_cs_bp[j] + (1.0 - gmi->greymf) * ssbp[j];
		}

#ifdef VERBOSE
		if (verb) {
			printf("Target (blended) dst wp/bp   = %f %f %f, %f %f %f\n",
				dr_cs_wp[0], dr_cs_wp[1], dr_cs_wp[2], dr_cs_bp[0], dr_cs_bp[1], dr_cs_bp[2]);
		}
#endif /* VERBOSE */

		/* Compute full adaptation target destinations */
		for (j = 0; j < 3; j++) {
			fawp[j] = dr_cs_wp[j];			/* White fully adapted */
			fabp[j] = dr_cs_bp[j];			/* Black fully adapted */
		}

		/* Clip the target grey axis to the destination gamut */
		if (d_gam->vector_isect(d_gam, fabp, fawp, fabp, fawp, NULL, NULL, NULL, NULL) == 0)
			error("gamut: vector_isect failed!");

		/* To work around the problem that vector_isect() is not entirely accurate, */
		/* special case the situation where gmi->greymf == 1.0 */
		if (gmi->greymf > 0.99) {
			for (j = 0; j < 3; j++) {
				fawp[j] = d_cs_wp[j];
				fabp[j] = d_cs_bp[j];
			}
		}

		/* If dst_kbp is set, then clipping to the dest gamut doesn't do what we want, */
		/* since it extends the black to a full composite black point. */
		/* A "K only" gamut is hard to define, so do a hack: */
		/* scale fabp[] towards fawp[] so that it has the same L as */
		/* the destination K only black point. */
		if (dst_kbp && fabp[0] < d_cs_bp[0]) {
			t = (d_cs_bp[0] - fawp[0])/(fabp[0] - fawp[0]);

			for (j = 0; j < 3; j++)
				fabp[j] = fawp[j] + t * (fabp[j] - fawp[j]);
		}

		/* Compute half adapted (full white, not black) target destinations */
		for (j = 0; j < 3; j++)
			hawp[j] = dr_cs_wp[j];			/* White fully adapted */

		/* Compute the rotation matrix that maps the source white point */
		/* onto the target white point. */
		icmRotMat(wrot, sswp, dr_cs_wp);

		/* Compute the target black point as the rotated source black point */ 
		icmMulBy3x3(habp, wrot, s_cs_bp);
		
		/* Now intersect the target white and black points with the destination */
		/* colorspace gamut to arrive at the best possible in gamut values for */
		/* the target white and black points. */
		if (d_gam->vector_isect(d_gam, habp, hawp, habp, hawp, NULL, NULL, NULL, NULL) == 0)
			error("gamut: vector_isect failed!");

		/* To work around the problem that vector_isect() is not entirely accurate, */
		/* special case the situation where gmi->greymf == 1.0 */
		if (gmi->greymf > 0.99) {
			for (j = 0; j < 3; j++) {
				hawp[j] = d_cs_wp[j];
			}
		}

		/* If dst_kbp is set, then clipping to the dest gamut doesn't do what we want, */
		/* since it extends the black to a full composite black point. */
		/* A "K only" gamut is hard to define, so do a hack: */
		/* scale habp[] towards hawp[] so that it has the same L as */
		/* the destination K only black point. */
		if (dst_kbp && habp[0] < d_cs_bp[0]) {
			t = (d_cs_bp[0] - hawp[0])/(habp[0] - hawp[0]);

			for (j = 0; j < 3; j++)
				habp[j] = hawp[j] + t * (habp[j] - hawp[j]);
		}

		/* Now decide the detail of the white and black alignment */
		if (gmi->bph == gmm_BPadpt || gmi->bph == gmm_bendBP) {
														/* Adapt to destination white and black */

			/* Use the fully adapted white and black points */
			for (j = 0; j < 3; j++) {
				dr_cs_wp[j] = fawp[j];
				dr_cs_bp[j] = fabp[j];
			}

			if (gmi->bph == gmm_bendBP) {

				/* Extend the half adapted (white = dst, black = src) black point */
				/* to the same L as the target (dst), to use as the initial (bent) black point */
				t = (dr_cs_bp[0] - dr_cs_wp[0])/(habp[0] - dr_cs_wp[0]);
				for (j = 0; j < 3; j++)
					dr_be_bp[j] = dr_cs_wp[j] + t * (habp[j] - dr_cs_wp[j]);

			} else {

				/* Set bent black point target to be the same as our actual */
				/* black point target, so that the "bend" code does nothing. */ 
				for (j = 0; j < 3; j++)
					dr_be_bp[j] = dr_cs_bp[j];
			}

		} else {					/* Adapt to destination white but not black */

			/* Use the half adapted (white = dst, black = src) white and black points */
			for (j = 0; j < 3; j++) {
				dr_cs_wp[j] = hawp[j];
				dr_cs_bp[j] = habp[j];
			}

#ifdef VERBOSE
			if (verb) {
				printf("Adapted target wp/bp         = %f %f %f, %f %f %f\n",
					dr_cs_wp[0], dr_cs_wp[1], dr_cs_wp[2], dr_cs_bp[0], dr_cs_bp[1], dr_cs_bp[2]);
			}
#endif
			if (gmi->bph == gmm_clipBP) {

				/* Extend the target black point to accomodate the */
				/* bent or clipped destination space L* range */
				if (fabp[0] < dr_cs_bp[0]) {
					t = (fabp[0] - dr_cs_wp[0])/(dr_cs_bp[0] - dr_cs_wp[0]);
					for (j = 0; j < 3; j++)
						dr_cs_bp[j] = dr_cs_wp[j] + t * (dr_cs_bp[j] - d_cs_wp[j]);
				}
			}
		
			/* Set the bent black point target to be the same as our actual */
			/* black point target, so that the "bend" code does nothing. */ 
			for (j = 0; j < 3; j++)
				dr_be_bp[j] = dr_cs_bp[j];
		}

#ifdef VERBOSE
		if (verb) {
			printf("Adapted & extended tgt wp/bp = %f %f %f, %f %f %f\n",
				dr_cs_wp[0], dr_cs_wp[1], dr_cs_wp[2], dr_cs_bp[0], dr_cs_bp[1], dr_cs_bp[2]);
		}
#endif /* VERBOSE */

		/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
		/* Now we need to figure out what origin alignment is needed, as well as */
		/* making sure the vectors are the same length to avoid rescaling. */
		/* (Scaling is meant to be done with the L curve though.) */

		/* Create temporary source white point that has the same L as the */
		/* target destination white point. */
		t = (dr_cs_wp[0] - s_cs_bp[0])/(s_cs_wp[0] - s_cs_bp[0]);
		for (j = 0; j < 3; j++)
			sswp[j] = s_cs_bp[j] + t * (s_cs_wp[j] - s_cs_bp[j]);

		/* Create temporary source black point that will form a vector to the src white */
		/* point with same length as the target destination black->white vector. */
		for (svl = dvl = 0.0, j = 0; j < 3; j++) {
			double tt;
			tt = sswp[j] - s_cs_bp[j]; 
			svl += tt * tt;
			tt = dr_cs_wp[j] - dr_cs_bp[j];
			dvl += tt * tt;
		}
		svl = sqrt(svl);
		dvl = sqrt(dvl);
		for (j = 0; j < 3; j++)
			ssbp[j] = sswp[j] + dvl/svl * (s_cs_bp[j] - sswp[j]); 

#ifdef VERBOSE
		if (verb) {
			printf("Rotate matrix src wp/bp      = %f %f %f, %f %f %f\n",
				sswp[0], sswp[1], sswp[2], ssbp[0], ssbp[1], ssbp[2]);
			printf("Rotate matrix dst wp/bp      = %f %f %f, %f %f %f\n",
				dr_cs_wp[0], dr_cs_wp[1], dr_cs_wp[2], dr_cs_bp[0], dr_cs_bp[1], dr_cs_bp[2]);
		}
#endif /* VERBOSE */

		/* Now create the general rotation and translation to map the source grey */
		/* axis to our destination grey axis. */
		icmVecRotMat(s->grot, sswp, ssbp, dr_cs_wp, dr_cs_bp);

		/* And create the inverse as well: */
		icmVecRotMat(s->igrot, dr_cs_wp, dr_cs_bp, sswp, ssbp);

		/* Create rotated versions of source colorspace & image white and */
		/* black points for use from now on, given that rotation will */
		/* be applied first to all source points. */
		icmMul3By3x4(sr_cs_wp, s->grot, s_cs_wp);
		icmMul3By3x4(sr_cs_bp, s->grot, s_cs_bp);
		icmMul3By3x4(sr_ga_wp, s->grot, s_ga_wp);
		icmMul3By3x4(sr_ga_bp, s->grot, s_ga_bp);

#ifdef VERBOSE
		if (verb) {
			printf("Bend target                                              bp = %f %f %f\n",
		        dr_be_bp[0], dr_be_bp[1], dr_be_bp[2]);
			printf("Rotated source grey axis wp/bp %f %f %f, %f %f %f\n",
				sr_cs_wp[0], sr_cs_wp[1], sr_cs_wp[2], sr_cs_bp[0], sr_cs_bp[1], sr_cs_bp[2]);
			printf("Rotated gamut grey axis wp/bp  %f %f %f, %f %f %f\n",
				sr_ga_wp[0], sr_ga_wp[1], sr_ga_wp[2], sr_ga_bp[0], sr_ga_bp[1], sr_ga_bp[2]);
			printf("Destination axis target wp/bp  %f %f %f, %f %f %f\n",
				dr_cs_wp[0], dr_cs_wp[1], dr_cs_wp[2], dr_cs_bp[0], dr_cs_bp[1], dr_cs_bp[2]);
		}
#endif
	}

#ifdef NEVER
sr_cs_wp[0] = 100.0;
sr_cs_bp[0] = 30.0;
dr_cs_wp[0] = 80.0;
dr_cs_bp[0] = 10.0;
glumknf	= 1.0;
#endif /* NEVER */

	/* Create the mapping points needed to build the 1D L mapping rspl. */
	/* If we have a gamut (ie. image) range that is smaller than the */
	/* L range of the colorspace, then use its white and black L values */
	/* as the source to be compressed to the destination L range. */
	/* We expand only a colorspace range, not a gamut/image range. */
	{
		double swL, dwL;			/* Source and destination white point L */
		double sbL, dbL;			/* Source and destination black point L */
		int j;
		double t;

		/* Setup white point mapping */
		if (sr_cs_wp[0] <= dr_cs_wp[0]) {	/* Needs possible expansion */
			swL = sr_cs_wp[0];
			dwL = gmi->glumwexf * dr_cs_wp[0] + (1.0 - gmi->glumwexf) * sr_cs_wp[0];

		} else {
			if (sr_ga_wp[0] > dr_cs_wp[0]) {	/* Gamut or colorspace needs compression */
				
				swL = (1.0 - gmi->glumwcpf) * dr_cs_wp[0] + gmi->glumwcpf * sr_ga_wp[0];
				dwL = dr_cs_wp[0];

			} else {	/* Neither needed */
				swL = sr_ga_wp[0];
				dwL = sr_ga_wp[0];
			}
		}

		/* Setup black point mapping */
		if (sr_cs_bp[0] >= dr_cs_bp[0]) {	/* Needs possible expansion */
			sbL = sr_cs_bp[0];
			dbL = gmi->glumbexf * dr_cs_bp[0] + (1.0 - gmi->glumbexf) * sr_cs_bp[0];

		} else {
			if (sr_ga_bp[0] < dr_cs_bp[0]) {	/* Gamut or colorspace needs compression */
				
				sbL = (1.0 - gmi->glumbcpf) * dr_cs_bp[0] + gmi->glumbcpf * sr_ga_bp[0];
				dbL = dr_cs_bp[0];

			} else {	/* Neither needed */
				sbL = sr_ga_bp[0];
				dbL = sr_ga_bp[0];
			}
		}

#ifdef PLOT_LMAP
		printf("sbL = %f, swL = %f\n",sbL,swL);
		printf("dbL = %f, dwL = %f\n",dbL,dwL);
#endif

		/* Remember our source and destination mapping targets */
		/* so that we can use them for fine tuning later. */

		/* We scale the source and target white and black */
		/* points to match the L values of the source and destination */
		/* L curve mapping, as this is how we have chosen the */
		/* white and black point mapping for the link. */
		/* Put them back in pre-rotated space, so that we can */
		/* check the overall transform of the white and black points. */
		t = (swL - sr_cs_bp[0])/(sr_cs_wp[0] - sr_cs_bp[0]);
		for (j = 0; j < 3; j++)
			s_mt_wp[j] = sr_cs_bp[j] + t * (sr_cs_wp[j] - sr_cs_bp[j]);
		icmMul3By3x4(s_mt_wp, s->igrot, s_mt_wp);

		t = (sbL - sr_cs_wp[0])/(sr_cs_bp[0] - sr_cs_wp[0]);
		for (j = 0; j < 3; j++)
			s_mt_bp[j] = sr_cs_wp[j] + t * (sr_cs_bp[j] - sr_cs_wp[j]);
//printf("~1 check black point rotated = %f %f %f\n",s_mt_bp[0],s_mt_bp[1],s_mt_bp[2]);
		icmMul3By3x4(s_mt_bp, s->igrot, s_mt_bp);
//printf("~1 check black point prerotated = %f %f %f\n",s_mt_bp[0],s_mt_bp[1],s_mt_bp[2]);

		t = (dwL - dr_cs_bp[0])/(dr_cs_wp[0] - dr_cs_bp[0]);
		for (j = 0; j < 3; j++)
			d_mt_wp[j] = dr_cs_bp[j] + t * (dr_cs_wp[j] - dr_cs_bp[j]);

		for (j = 0; j < 3; j++)
			d_mt_bp[j] = dr_cs_wp[j] + t * (dr_cs_bp[j] - dr_cs_wp[j]);

		/* To ensure symetry between compression and expansion, always create RSPL for */
		/* overall compression and its inverse, and then swap grey and igrey rspl to compensate. */
		/* We swap the source and desitination white and black points to achieve this. */
		/* Note that we could still have expansion at one end or the other, depending */
		/* on the center point location, so we need to allow for this in the rspl setup. */
		if ((dwL - dbL) > (swL - sbL)) {
			double tt;
			tt = swL; swL = dwL; dwL = tt;
			tt = sbL; sbL = dbL; dbL = tt;
			revrspl = 1;
		}

		/* White point end */
		lpnts[ngreyp].p[0] = swL;
		lpnts[ngreyp].v[0] = dwL;
		lpnts[ngreyp++].w  = 10.0;		/* Must go through here */

		/* Black point end */
		lpnts[ngreyp].p[0] = sbL;
		lpnts[ngreyp].v[0] = dbL;
		lpnts[ngreyp++].w  = 10.0;		/* Must go through here */

#ifdef USE_GLUMKNF
		if (gmi->glumknf < 0.05)
#endif /* USE_GLUMKNF */
		{			/* make sure curve is firmly anchored */
			lpnts[ngreyp].p[0] = 0.3 * lpnts[ngreyp-1].p[0] + 0.7 * lpnts[ngreyp-2].p[0];
			lpnts[ngreyp].v[0] = 0.3 * lpnts[ngreyp-1].v[0] + 0.7 * lpnts[ngreyp-2].v[0];
			lpnts[ngreyp++].w  = 1.0;	

			lpnts[ngreyp].p[0] = 0.7 * lpnts[ngreyp-2].p[0] + 0.3 * lpnts[ngreyp-3].p[0];
			lpnts[ngreyp].v[0] = 0.7 * lpnts[ngreyp-2].v[0] + 0.3 * lpnts[ngreyp-3].v[0];
			lpnts[ngreyp++].w  = 1.0;	
		}
#ifdef USE_GLUMKNF
		else {		/* There is at least some weight in knee points */
			double cppos = 0.50;		/* Center point ratio between black and white */
			double cpll, cplv;			/* Center point location and value */
			double kpwpos = 0.30;		/* White knee point location prop. towards center */
			double kpbpos = 0.20;		/* Black knee point location prop. towards center */
			double kwl, kbl, kwv, kbv;	/* Knee point values and locations */
			double kwx, kbx;			/* Knee point extra  */
			
#ifdef PLOT_LMAP
			printf("%ssbL = %f, swL = %f\n", revrspl ? "(swapped) ": "", sbL,swL);
			printf("%sdbL = %f, dwL = %f\n", revrspl ? "(swapped) ": "", dbL,dwL);
#endif

			/* Center point location */
			cpll = cppos * (swL - sbL) + sbL;
			// ~~?? would this be better if the output
			// was scaled by dwL/swL ?
			cplv = cppos * (swL - sbL) + sbL;
#ifdef PLOT_LMAP
			printf("cpll = %f, cplv = %f\n",cpll, cplv);
#endif

#ifdef NEVER	/* Don't use a center point */
			lpnts[ngreyp].p[0] = cpll;
			lpnts[ngreyp].v[0] = cplv;
			lpnts[ngreyp++].w  = 5.0;	
#endif
	
//printf("~1 black half diff = %f\n",dbL - sbL); 
//printf("~1 white half diff = %f\n",dwL - swL); 

			/* Knee point locations */
			kwl = kpwpos * (cplv - swL) + swL;
			kbl = kpbpos * (cplv - sbL) + sbL;
			
			/* Extra compression for white and black knees */
			// ~~ ie move knee point level beyond 45 degree line
			// ~~ weigting of black point and white point differences
			kwx = 0.6 * (dbL - sbL) + 1.0 * (swL - dwL);
			kbx = 1.0 * (dbL - sbL) + 0.6 * (swL - dwL);

//kwx = 0.0;
//kbx = 0.0;
//glumknf = 0.0;

			/* Knee point values */
			kwv = (dwL + kwx - cplv) * (kwl - cplv)/(swL - cplv) + cplv;
			if (kwv > dwL)		/* Sanity check */
				kwv = dwL;

			kbv = (dbL - kbx - cplv) * (kbl - cplv)/(sbL - cplv) + cplv;
			if (kbv < dbL)		/* Sanity check */
				kbv = dbL;


#ifdef PLOT_LMAP
			printf("using kbl = %f, kbv = %f\n",kbl, kbv);
			printf("using kwl = %f, kwv = %f\n",kwl, kwv);
#endif

			/* Emphasise points to cause white "knee" curve */
			lpnts[ngreyp].p[0] = kwl;
			lpnts[ngreyp].v[0] = kwv;
			lpnts[ngreyp++].w  = gmi->glumknf * gmi->glumknf;	
		
			/* Emphasise points to cause black "knee" curve */
			lpnts[ngreyp].p[0] = kbl;
			lpnts[ngreyp].v[0] = kbv;
			lpnts[ngreyp++].w  = 1.5 * gmi->glumknf * 1.5 * gmi->glumknf;	
		}
#endif /* USE_GLUMKNF */
	}

	/* We now create the 1D rspl L map, that compresses or expands the luminence */
	/* range, independent of grey axis alignment, or gamut compression. */
	/* Because the rspl isn't symetrical when we swap X & Y, and we would */
	/* like a conversion from profile A to B to be the inverse of profile B to A */
	/* (as much as possible), we contrive here to always create a compression */
	/* RSPL, and create an inverse for it, and swap the two of them so that */
	/* the transform is correct and has an accurate inverse available. */
	{
		datai il, ih;
		datao ol, oh;
		double avgdev[MXDO];
		int gres = 256;

		/* Create a 1D rspl, that is used to */
		/* form the overall L compression mapping. */
		if ((s->grey = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL)	/* Allocate 1D -> 1D */
			error("gamut: grey new_rspl failed");
	
		il[0] = -1.0;		/* Set possible input range */
		ih[0] = 101.0;
		ol[0] = 0.0;		/* Set normalisation output range */
		oh[0] = 100.0;

#ifdef NEVER		/* Dump out the L mapping points */
		{
			int i;
			printf("1D rspl L mapping points:\n");
			for (i = 0; i < ngreyp; i++)
				printf("%d %f -> %f (w %f)\n",i,lpnts[i].p[0],lpnts[i].v[0],lpnts[i].w);
		}
#endif
		/* Create spline from the data points, with appropriate smoothness. */
		avgdev[0] = GAMMAP_RSPLAVGDEV;
		if (s->grey->fit_rspl_w(s->grey, GAMMAP_RSPLFLAGS, lpnts, ngreyp, il, ih, &gres, ol, oh, 5.0, avgdev, NULL)) {
			fprintf(stderr,"Warning: Grey axis mapping is non-monotonic - may not be very smooth ?\n");
		}

		/* Create an inverse mapping too, for reverse gamut and/or expansion. */
		il[0] = -1.0;		/* Set possible input range */
		ih[0] = 101.0;
		ol[0] = 0.0;		/* Set normalisation output range */
		oh[0] = 100.0;

		if ((s->igrey = new_rspl(RSPL_NOFLAGS, 1, 1)) == NULL)	/* Allocate 1D -> 1D */
			error("gamut: igrey new_rspl failed");
			
		/* Create it from inverse lookups of s->grey */
		s->igrey->set_rspl(s->igrey, 0, (void *)s->grey, inv_grey_func, il, ih, &gres, ol, oh);

		if (revrspl) {		/* Swap to compensate for swapping of white and black points */
			rspl *tt = s->grey;
			s->grey = s->igrey;
			s->igrey = tt;
		}
	}

#ifdef PLOT_LMAP
	{	/* Plot the 1D mapping */
		double xx[XRES];
		double y1[XRES];
		int i;

		for (i = 0; i < XRES; i++) {
			double x;
			co cp;		/* Conversion point */
			x = sr_cs_bp[0] + (i/(double)(XRES-1)) * (sr_cs_wp[0] - sr_cs_bp[0]);
			xx[i] = x;
			cp.p[0] = x;
			s->grey->interp(s->grey, &cp);
			y1[i] = cp.v[0];
		}
		do_plot(xx,y1,NULL,NULL,XRES);
	}
#endif /* PLOT_LMAP */

	{
		/* We want to rotate and then map L independently of everything else, */
		/* so transform source cs & image gamuts through the rotation and L mapping */
		/* before we create the surface 3D mapping from them */

		/* Create L mapped versions of rotated src colorspace white/black points */
		dopartialmap1(s, sl_cs_wp, s_cs_wp);
		dopartialmap1(s, sl_cs_bp, s_cs_bp);

#ifdef VERBOSE
		if (verb) {
			printf("Mapped source grey axis wp/bp  %f %f %f, %f %f %f\n",
				sl_cs_wp[0], sl_cs_wp[1], sl_cs_wp[2], sl_cs_bp[0], sl_cs_bp[1], sl_cs_bp[2]);
		}
#endif

		if ((scl_gam = parttransgamut(s, sc_gam)) == NULL) {
			fprintf(stderr,"gamut map: parttransgamut failed\n");
			free(s);
			return NULL;
		}

		if (sc_gam == si_gam)
			sil_gam = scl_gam;

		else {
			if ((sil_gam = parttransgamut(s, si_gam)) == NULL) {
				fprintf(stderr,"gamut map: parttransgamut failed\n");
				free(s);
				return NULL;
			}
		}
	}

	/* Create all the 3D->3D gamut mapping points and 3D rspl, */
	/* if there is any compression or expansion to do. */
	if (gmi->gamcpf > 1e-6 || gmi->gamexf > 1e-6) {
		cow *gpnts = NULL;	/* Mapping points to create gamut mapping */
		int max_gpnts;
		int nspts;		/* Number of source gamut surface points */
		int rgridpts;	/* Number of range surface grid points */
		int i, j;
		datai il, ih;
		datao ol, oh;
		int gres[MXDI];
		double avgdev[MXDO];
		nearsmth *nsm = NULL;	/* Returned list of near smooth points */
		int nnsm;				/* Number of near smoothed points */
		double brad = 0.0;		/* Black bend radius */
		gammapweights xpweights[14], xsweights[14];	/* Explicit perceptial and sat. weights */
		gammapweights xwh[14]; 	/* Structure holding blended weights */
		double smooth = 1.0;	/* Level of 3D RSPL smoothing, blend of psmooth and ssmooth */
		vrml *wrl = NULL;		/* Gamut mapping illustration (hulls + guide vectors) */
		cgats *locus = NULL;	/* Diagnostic locus to plot in wrl, NULL if none */

#ifdef PLOT_3DKNEES
typedef struct {
	double v0[3], v1[3];
} p3dk_lpoint;
		p3dk_lpoint *p3dk_locus;
		int p3dk_ix = 0;
#endif /* PLOT_3DKNEES */

		/* Get the maximum number of points that will be created */
		nspts = near_smooth_np(scl_gam, sil_gam, d_gam, xvra);
		
		rgridpts = 0;
#ifdef USE_BOUND
		if (defrgrid >= 2) {
			rgridpts = defrgrid * defrgrid * defrgrid
			         - (defrgrid -2) * (defrgrid -2) * (defrgrid -2);
		}
#endif

		max_gpnts = nres + 3 * nspts + rgridpts;
		if ((gpnts = (cow *)malloc(max_gpnts * sizeof(cow))) == NULL) { 
			fprintf(stderr,"gamut map: Malloc of mapping setup points failed\n");
			s->grey->del(s->grey);
			s->igrey->del(s->igrey);
			if (sil_gam != scl_gam)
				sil_gam->del(sil_gam);
			scl_gam->del(scl_gam);
			free(s);
			return NULL;
		}

#ifdef PLOT_3DKNEES
		if ((p3dk_locus = (p3dk_lpoint *)malloc((2 * nspts) * sizeof(p3dk_lpoint))) == NULL)
			error("gamut: Diagnostic array p3dk_locus malloc failed");
#endif /* PLOT_3DKNEES */

		/* ------------------------------------------- */
		/* Finish off the grey axis mapping by creating the */
		/* grey axis 3D->3D mapping points */
		/* We use 4 times the grid density, and create */
		/* points that span the source colorspace (this may exceed) */
		/* the source image gamut, and map to points outside the */
		/* destination gamut) */

		/* See how much to bend the black - compute the color difference */
		/* We start out in the direction of dr_be_bp at white, and at */
		/* the end we bend towards the overall bp dr_cs_bp */
		/* (brad will be 0 for non gmm_bendBP because dr_be_bp dr_cs_bp */
		for (brad = 0.0, i = 1; i < 3; i++) {
			double tt = dr_be_bp[i] - dr_cs_bp[i];
			brad += tt * tt;
		}
		brad = sqrt(brad);

//printf("~1 brad = %f, Bend target = %f %f %f, straight = %f %f %f\n",
//brad, dr_be_bp[0], dr_be_bp[1], dr_be_bp[2], dr_cs_bp[0], dr_cs_bp[1], dr_cs_bp[2]);

#ifdef USE_GREYMAP
		for (i = 0; i < nres; i++) {	/* From black to white */
			double t;
			double bv[3];		/* Bent (initial) destination value */
			double dv[3];		/* Straight (final) destination value */
			double wt = 1.0;	/* Default grey axis point weighting */

			/* Create source grey axis point */
			t = i/(nres - 1.0);

			/* Cover L = 0.0 to 100.0 */
			t = ((100.0 * t) - sl_cs_bp[0])/(sl_cs_wp[0] - sl_cs_bp[0]);
			for (j = 0; j < 3; j++)
				gpnts[ngamp].p[j] = sl_cs_bp[j] + t * (sl_cs_wp[j] - sl_cs_bp[j]);

			/* L values are the same, as they have been mapped prior to 3D */
			gpnts[ngamp].v[0] = gpnts[ngamp].p[0];

			/* Figure destination point on initial bent grey axis */
			t = (gpnts[ngamp].v[0] - dr_cs_wp[0])/(dr_be_bp[0] - dr_cs_wp[0]);
			for (j = 0; j < 3; j++)
				bv[j] = dr_cs_wp[j] + t * (dr_be_bp[j] - dr_cs_wp[j]);
//printf("~1 t = %f, bent dest     %f %f %f\n",t, bv[0], bv[1],bv[2]);
			
			/* Figure destination point on final straight grey axis */
			t = (gpnts[ngamp].v[0] - dr_cs_wp[0])/(dr_cs_bp[0] - dr_cs_wp[0]);
			for (j = 0; j < 3; j++)
				dv[j] = dr_cs_wp[j] + t * (dr_cs_bp[j] - dr_cs_wp[j]);
//printf("~1 t = %f, straight dest %f %f %f\n",t, dv[0], dv[1],dv[2]);
			
			/* Figure out a blend value between the bent value */
			/* and the straight value, so that it curves smoothly from */
			/* one to the other. */
			if (brad > 0.001) {
				double ty;
				t = ((dr_cs_bp[0] + brad)  - gpnts[ngamp].v[0])/brad;
				if (t < 0.0)
					t = 0.0;
				else if (t > 1.0)
					t = 1.0;
				/* Make it a spline ? */
				t = t * t * (3.0 - 2.0 * t);
				ty = t * t * (3.0 - 2.0 * t);	/* spline blend value */
				t = (1.0 - t) * ty + t * t;		/* spline at t == 0, linear at t == 1 */

				wt *= (1.0 + t * brad);	/* Increase weigting with the bend */

			} else {
				t = 0.0;	/* stick to straight, it will be close anyway. */
			}

			for (j = 0; j < 3; j++)		/* full straight when t == 1 */
				gpnts[ngamp].v[j] = t * dv[j] + (1.0 - t) * bv[j];
			gpnts[ngamp].w = wt;
//printf("~1 t = %f,      blended  %f %f %f\n",t, gpnts[ngamp].v[0], gpnts[ngamp].v[1],gpnts[ngamp].v[2]);

#ifdef NEVER
			printf("Grey axis %d maps %f %f %f -> %f %f %f wit %f\n",ngamp,
			gpnts[ngamp].p[0], gpnts[ngamp].p[1], gpnts[ngamp].p[2],
			gpnts[ngamp].v[0], gpnts[ngamp].v[1], gpnts[ngamp].v[2],
			gpnts[ngamp].w);
#endif
			ngamp++;
			if (ngamp >= max_gpnts)
				error("gammap: internal, not enough space for mapping points A (%d > %d)\n",ngamp, max_gpnts); 
		}
#endif /* USE_GREYMAP */

		/* ---------------------------------------------------- */
		/* Do preliminary computation of the rspl input and output bounding values */
		for (j = 0; j < 3; j++) {
			il[j] = ol[j] =  1e60;
			ih[j] = oh[j] = -1e60;
		}

		/* From grey axis points */
		for (i = 0; i < ngamp; i++) {
			for (j = 0; j < 3; j++) {
				if (gpnts[i].p[j] < il[j])
					il[j] = gpnts[i].p[j];
				if (gpnts[i].p[j] > ih[j])
					ih[j] = gpnts[i].p[j];
			}
		}

		/* From the source gamut */
		{
			double tmx[3], tmn[3];
			scl_gam->getrange(scl_gam, tmn, tmx);
			for (j = 0; j < 3; j++) {
				if (tmn[j] < il[j])
					il[j] = tmn[j];
				if (tmx[j] > ih[j])
					ih[j] = tmx[j];
			}
		}

		/* from input arguments override */ 
		if (mn != NULL && mx != NULL) {

			for (j = 0; j < 3; j++) {
				if (mn[j] < il[j])
					il[j] = mn[j];
				if (mx[j] > ih[j])
					ih[j] = mx[j];
			}
		}

		/* From the destination gamut */
		{
			double tmx[3], tmn[3];
			d_gam->getrange(d_gam, tmn, tmx);
			for (j = 0; j < 3; j++) {
				if (tmn[j] < ol[j])
					ol[j] = tmn[j];
				if (tmx[j] > oh[j])
					oh[j] = tmx[j];
			}
		}

		/* ---------------------------------------------------- */
		/* Deal with gamut hull guide vector creation. */

		/* For compression, create a mapping for each vertex of */
		/* the source gamut (image) surface towards the destination gamut */
		/* For expansion, do the opposite. */

		/* Convert from compact to explicit hextant weightings */
		if (expand_weights(xpweights, pweights)
		 || expand_weights(xsweights, sweights)) {
			fprintf(stderr,"gamut map: expand_weights() failed\n");
			s->grey->del(s->grey);
			s->igrey->del(s->igrey);
			if (sil_gam != scl_gam)
				sil_gam->del(sil_gam);
			scl_gam->del(scl_gam);
			free(s);
			return NULL;
		}
		/* Create weights as blend between perceptual and saturation */
		near_xwblend(xwh, xpweights, gmi->gampwf, xsweights, gmi->gamswf);
		if ((gmi->gampwf + gmi->gamswf) > 0.1)
			smooth = (gmi->gampwf * psmooth) + (gmi->gamswf * ssmooth);

		/* Tweak gamut mappings according to extra cmy cusp flags or rel override */ 
		if (dst_cmymap != 0 || rel_oride != 0) {
			tweak_weights(xwh, dst_cmymap, rel_oride); 
		}

		/* Create the near point mapping, which is our fundamental gamut */
		/* hull to gamut hull mapping. */
		nsm = near_smooth(verb, &nnsm, scl_gam, sil_gam, d_gam, src_kbp, dst_kbp,
		                  dr_cs_bp, xwh, gmi->gamcknf, gmi->gamxknf,
		                  gmi->gamcpf > 1e-6, gmi->gamexf > 1e-6,
		                  xvra, mapres, smooth, il, ih, ol, oh);
		if (nsm == NULL) {
			fprintf(stderr,"Creating smoothed near points failed\n");
			s->grey->del(s->grey);
			s->igrey->del(s->igrey);
			if (sil_gam != scl_gam)
				sil_gam->del(sil_gam);
			scl_gam->del(scl_gam);
			free(s);
			return NULL;
		}
		/* --------------------------- */

		/* Make sure the input range to encompasss the guide vectors. */
		for (i = 0; i < nnsm; i++) {
			for (j = 0; j < 3; j++) {
				if (nsm[i].sv[j] < il[j])
					il[j] = nsm[i].sv[j];;
				if (nsm[i].sv[j] > ih[j])
					ih[j] = nsm[i].sv[j];
			}
		}

#ifdef NEVER
		if (verb) {
			fprintf(stderr,"Input bounding box:\n");
			fprintfstderr,("%f -> %f, %f -> %f, %f -> %f\n",
			il[0], ih[0], il[1], ih[1], il[2], ih[2]);
		}
#endif

		/* Now expand the bounding box by aprox 5% margin, but scale grid res */
		/* to match, so that the natural or given boundary still lies on the grid. */
		{
			int xmapres;
			double scale;

			xmapres = (int) ((mapres-1) * 0.05 + 0.5);
			if (xmapres < 1)
				xmapres = 1;

			scale = (double)(mapres-1 + xmapres)/(double)(mapres-1);

			for (j = 0; j < 3; j++) {
				double low, high;
				high = ih[j];
				low = il[j];
				ih[j] = (scale * (high - low)) + low;
				il[j] = (scale * (low - high)) + high;
			}

			mapres += 2 * xmapres;
#ifdef NEVER
			if (verb) {
				fprintf(stderr,"After incresing mapres to %d, input bounding box for 3D gamut mapping is:\n",mapres);
				fprintf(stderr,"%f -> %f, %f -> %f, %f -> %f\n",
				il[0], ih[0], il[1], ih[1], il[2], ih[2]);
			}
#endif
		}

		/* ---------------------------------------------------- */
		/* Setup for diagnostic plot, that will have elements added */
		/* as we create the final 3D gamut mapping rspl */
		/* (The plot is of the already rotated and L mapped source space) */
		{
			int doaxes = 0;

#ifdef PLOT_AXES
			doaxes = 1;
#endif
			if (diagname != NULL)
				wrl = new_vrml(diagname, doaxes, vrml_lab);
#ifdef PLOT_DIAG_WRL
			else
				wrl = new_vrml("gammap", doaxes, vrml_lab);
#endif
		}

		if (wrl != NULL) {
			/* See if there is a diagnostic locus to plot too */
			if ((locus = new_cgats()) == NULL)
				error("Failed to create cgats object");

			locus->add_other(locus, "TS");

			if (locus->read_name(locus, "locus.ts")) {
				locus->del(locus);
				locus = NULL;
			} else {
				if (verb)
					printf("!! Found diagnostic locus.ts file !!\n");
				/* locus will be added later */
			}

			/* Add diagnostic markers from markers structure */
			for (i = 0; ; i++) {
				double pp[3];
				co cp;
				if (markers[i].type == 0)
					break;
	
				if (markers[i].type == 1) {		/* Src point - do luminance mapping */
					dopartialmap1(s, pp, markers[i].pos);
				} else {
					pp[0] = markers[i].pos[0];
					pp[1] = markers[i].pos[1];
					pp[2] = markers[i].pos[2];
				}
				wrl->add_marker(wrl, pp, markers[i].col, 1.0);
			}
		}

		/* --------------------------- */
		/* Now computue our 3D mapping points from the near point mapping. */
		for (i = 0; i < nnsm; i++) {
			double cpexf;			/* The effective compression or expansion factor */

			if (nsm[i].vflag == 0) {			/* Unclear whether compression or expansion */
				/* Use larger to the the two factors */
				cpexf = gmi->gamcpf > gmi->gamexf ? gmi->gamcpf : gmi->gamexf;
				
			} else if (nsm[i].vflag == 1) {		/* Compression */
				cpexf = gmi->gamcpf;

			} else if (nsm[i].vflag == 2) {		/* Expansion */
				cpexf = gmi->gamexf;

			} else {
				error("gammap: internal, unknown guide point flag");
			}

			/* Compute destination value which is a blend */
			/* between the source value and the fully mapped destination value. */
			icmBlend3(nsm[i].div, nsm[i].sv, nsm[i].dv, cpexf);

#ifdef NEVER
			printf("%s mapping:\n",nsm[i].vflag == 0 ? "Unclear" : nsm[i].vflag == 1 ? "Compression" : "Expansion");
			printf("Src point = %f %f %f radius %f\n",nsm[i].sv[0], nsm[i].sv[1], nsm[i].sv[2], nsm[i].sr);
			printf("Dst point = %f %f %f radius %f\n",nsm[i].dv[0], nsm[i].dv[1], nsm[i].dv[2], nsm[i].dr);
			printf("Blended dst point = %f %f %f\n",nsm[i].div[0], nsm[i].div[1], nsm[i].div[2]);
#endif	/* NEVER */
			/* Set the main gamut hull mapping point */
			for (j = 0; j < 3; j++) {
				gpnts[ngamp].p[j] = nsm[i].sv[j];
				gpnts[ngamp].v[j] = nsm[i].div[j];
			}
			gpnts[ngamp++].w = 1.01;		/* Main gamut surface mapping point */
											/* (Use 1.01 as a marker value) */
			if (ngamp >= max_gpnts)
				error("gammap: internal, not enough space for mapping points B (%d > %d)\n",ngamp, max_gpnts); 

#ifdef USE_GAMKNF
			/* Add sub surface mapping point if available */
			if (nsm[i].vflag != 0) {	/* Sub surface point is available */

				/* Compute destination value which is a blend */
				/* between the source value and the knee adjusted destination */
				icmBlend3(nsm[i].div2, nsm[i].sv2, nsm[i].dv2, cpexf);

#ifdef NEVER
				printf("Src2 point = %f %f %f radius %f\n",nsm[i].sv2[0], nsm[i].sv2[1], nsm[i].sv2[2], nsm[i].sr);
				printf("Dst2 point = %f %f %f radius %f\n",nsm[i].dv2[0], nsm[i].dv2[1], nsm[i].dv2[2], nsm[i].dr);
				printf("Blended dst2 point = %f %f %f\n",nsm[i].div2[0], nsm[i].div2[1], nsm[i].div2[2]);
				printf("Src/Dst3 point = %f %f %f w %f\n",nsm[i].sd2[0], nsm[i].sd2[1], nsm[i].sd2[2]);
				printf("\n");
#endif	/* NEVER */

				/* Set the sub-surface gamut hull mapping point */
				for (j = 0; j < 3; j++) {
					gpnts[ngamp].p[j] = nsm[i].sv2[j];
					gpnts[ngamp].v[j] = nsm[i].div2[j];
				}
				gpnts[ngamp++].w = nsm[i].w2;		/* Sub-suface mapping points */

				if (ngamp >= max_gpnts)
					error("gammap: internal, not enough space for mapping points C (%d > %d)\n",ngamp, max_gpnts); 

				/* Set the sub-surface gamut hull mapping point */
				for (j = 0; j < 3; j++) {
					gpnts[ngamp].p[j] = nsm[i].sd3[j];
					gpnts[ngamp].v[j] = nsm[i].sd3[j];
				}
				gpnts[ngamp++].w = nsm[i].w3;		/* Sub-suface mapping points */

				if (ngamp >= max_gpnts)
					error("gammap: internal, not enough space for mapping points D (%d > %d)\n",ngamp, max_gpnts); 
			}
#endif /* USE_GAMKNF */
		}

		if (ngamp >= max_gpnts)
			error("gammap: internal, not enough space for mapping points (%d > %d)\n",ngamp, max_gpnts); 

		/* Create preliminary gamut mapping rspl, without grid boundary values. */
		/* We use this to lookup the mapping for points on the source space gamut */
		/* that result from clipping our grid boundary points */
#ifdef USE_BOUND
		for (j = 0; j < 3; j++) {		/* Set resolution for all axes */
			gres[j] = (mapres+1)/2;
			avgdev[j] = GAMMAP_RSPLAVGDEV;
		}
		s->map = new_rspl(RSPL_NOFLAGS, 3, 3);	/* Allocate 3D -> 3D */
		s->map->fit_rspl_w(s->map, GAMMAP_RSPLFLAGS, gpnts, ngamp, il, ih, gres, ol, oh, smooth, avgdev, NULL);

		/* Add input range grid surface anchor points to improve clipping behaviour. */
		if (defrgrid >= 2) {
			DCOUNT(gc, 3, 3, 0, 0, defrgrid);
			double cent[3];

			sc_gam->getcent(d_gam, cent);

			DC_INIT(gc);
			for (;;) {
				/* If point is on the grid surface */
				if (   gc[0] == 0 || gc[0] == (defrgrid-1)
					|| gc[1] == 0 || gc[1] == (defrgrid-1)
					|| gc[2] == 0 || gc[2] == (defrgrid-1)) {
					double grid2gamut, gamut2cent, ww;
					co cp;

					/* Clip the point to the closest location on the source */
					/* colorspace gamut. */
					for (j = 0; j < 3; j++)
						gpnts[ngamp].p[j] = il[j] + gc[j]/(defrgrid-1.0) * (ih[j] - il[j]);
					sc_gam->nearest(sc_gam, cp.p, gpnts[ngamp].p);

					/* Then lookup the equivalent gamut mapped value */
					s->map->interp(s->map, &cp);

					for (j = 0; j < 3; j++)
						gpnts[ngamp].v[j] = cp.v[j];

					/* Compute the distance of the grid surface point to the to the */
					/* source colorspace gamut, as well as the distance from there */
					/* to the gamut center point. */
					for (grid2gamut = gamut2cent = 0.0, j = 0; j < 3; j++) {
						double tt;
						tt = gpnts[ngamp].p[j] - cp.p[j];
						grid2gamut += tt * tt;
						tt = cp.p[j] - cent[j];
						gamut2cent += tt * tt;
					}
					grid2gamut = sqrt(grid2gamut);
					gamut2cent = sqrt(gamut2cent);
					
					/* Make the weighting inversely related to distance, */
					/* to reduce influence on in gamut mapping shape, */
					/* while retaining some influence at the edge of the */
					/* grid. */
					ww = grid2gamut / gamut2cent;
					if (ww > 1.0)
						ww = 1.0;
					
					/* A low weight seems to be enough ? */
					/* the lower the better in terms of geting best hull mapping fidelity */
					gpnts[ngamp++].w = 0.05 * ww;

					if (ngamp >= max_gpnts)
						error("gammap: internal, not enough space for mapping points E (%d > %d)\n",ngamp, max_gpnts); 
				}
				DC_INC(gc);
				if (DC_DONE(gc))
					break;
			}
		}
#else /* !USE_BOUND */
		printf("!!!! Warning - gammap boundary points disabled !!!!\n");
#endif /* !USE_BOUND */

		/* --------------------------- */
		/* Compute the output bounding values, and check input range hasn't changed */
		for (i = 0; i < ngamp; i++) {
			for (j = 0; j < 3; j++) {
				if (gpnts[i].p[j] < (il[j]-1e-5) || gpnts[i].p[j] > (ih[j]+1e-5))
					warning("gammap internal: input bounds has changed! %f <> %f <> %f",il[j],gpnts[i].p[j],ih[j]);
				if (gpnts[i].v[j] < ol[j])
					ol[j] = gpnts[i].v[j];
				if (gpnts[i].v[j] > oh[j])
					oh[j] = gpnts[i].v[j];
			}
		}

		/* --------------------------- */

#ifdef NEVER		/* Dump out all the mapping points */
		{
			for (i = 0; i < ngamp; i++) {
				printf("%d: %f %f %f -> %f %f %f\n",i,
					gpnts[i].p[0], gpnts[i].p[1], gpnts[i].p[2],
					gpnts[i].v[0], gpnts[i].v[1], gpnts[i].v[2]);
			}
		}
#endif

		/* Create the final gamut mapping rspl. */
		/* [ The smoothing is not as useful as it should be, because */
		/*   if it is increased it tends to push colors out of gamut */
		/*   where they get clipped. Some cleverer scheme which makes */
		/*   sure that smoothness errs on the side of more compression */
		/*   is needed. - Addressed in nearsmth now ? ] */
		/* How about converting to a delta filer ? ie. */
		/* create curren filter, then create point list of delta from */
		/* smoothed value, filtering that and then un-deltering it ?? */
		if (s->map != NULL)
			s->map->del(s->map);
		if (verb)
			printf("Creating rspl..\n");
		for (j = 0; j < 3; j++) {		/* Set resolution for all axes */
			gres[j] = mapres;
			avgdev[j] = GAMMAP_RSPLAVGDEV;
		}
		s->map = new_rspl(RSPL_NOFLAGS, 3, 3);	/* Allocate 3D -> 3D */
		if (s->map->fit_rspl_w(s->map, GAMMAP_RSPLFLAGS, gpnts, ngamp, il, ih, gres, ol, oh, smooth, avgdev, NULL)) {
			if (verb)
				fprintf(stderr,"Warning: Gamut mapping is non-monotonic - may not be very smooth !\n");
		}
		/* return the min and max of the input values valid in the grid */
		s->map->get_in_range(s->map, s->imin, s->imax); 

#ifdef CHECK_NEARMAP
		/* Check how accurate gamut shell mapping is against nsm */
		/* (This isn't a good indication now that vectors have been adjusted */
		/*  to counteract the rspl smoothing at the edges.) */
		if (verb) {
			double de, avgde = 0.0, maxde = 0.0;		/* DE stats */
			
			for (i = 0; i < nnsm; i++) {
				double av[3];
	
				/* Compute the mapping error */
				dopartialmap2(s, av, nsm[i].sv);		/* Just the rspl */
	
				de = icmLabDE(nsm[i].div, av);
				avgde += de;
				if (de > maxde)
					maxde = de;
			}
			printf("Gamut hull fit to guides: = avg %f, max %f\n",avgde/nnsm,maxde);
		}
#endif /* CHECK_NEARMAP */

		/* If requested, enhance the saturation of the output values. */
		if (gmi->satenh > 0.0) {
			adjustsat cx;		/* Adjustment context */

			/* Compute what our source white and black points actually maps to */
			s->domap(s, cx.wp, s_mt_wp);
			s->domap(s, cx.bp, s_mt_bp);

			cx.dst = d_gam; 
			cx.satenh = gmi->satenh; 

			/* Saturation enhance the output values */
			s->map->re_set_rspl(
				s->map,				/* this */
				0,					/* Combination of flags */
				(void *)&cx,		/* Opaque function context */
				adjust_sat_func /* Function to set from */
			);
		}

		/* Test the gamut white and black point mapping, and "fine tune" */
		/* the mapping, to ensure an accurate transform of the white */
		/* and black points to the destination colorspace. */
		/* This compensates for any inacuracy introduced in the */
		/* various rspl mappings. */
 		{
			adjustwb cx;		/* Adjustment context */
			double a_wp[3];		/* actual white point */
			double a_bp[3];		/* actual black point */

			if (verb)
				printf("Fine tuning white and black point mapping\n");

			/* Check what the source white and black points actually maps to */
			s->domap(s, a_wp, s_mt_wp);
			s->domap(s, a_bp, s_mt_bp);

#ifdef VERBOSE
			if (verb) {
				printf("White is %f %f %f, should be %f %f %f\n",
				a_wp[0], a_wp[1], a_wp[2], d_mt_wp[0], d_mt_wp[1], d_mt_wp[2]);
				printf("Black is %f %f %f, should be %f %f %f\n",
				a_bp[0], a_bp[1], a_bp[2], d_mt_bp[0], d_mt_bp[1], d_mt_bp[2]);
			}
#endif /* VERBOSE */

			/* Setup the fine tune transform */

			/* We've decided not to fine tune the black point if we're */
			/* bending to the destination black, as the bend is not */
			/* followed perfectly (too sharp, or in conflict with */
			/* the surface mapping ?) and we don't want to shift */
			/* mid neutrals due to this. */
			/* We do fine tune it if dst_kbp is set though, since */
			/* we would like perfect K only out. */

			/* Compute rotation/scale relative white point matrix */
			icmVecRotMat(cx.mat, a_wp, a_bp, d_mt_wp, d_mt_bp);		/* wp & bp */

			/* Fine tune the 3D->3D mapping */
			s->map->re_set_rspl(
				s->map,				/* this */
				0,					/* Combination of flags */
				(void *)&cx,		/* Opaque function context */
				adjust_wb_func 		/* Function to set from */
			);

#ifdef VERBOSE
			if (verb) {
				/* Check what the source white and black points actually maps to */
				s->domap(s, a_wp, s_mt_wp);
				s->domap(s, a_bp, s_mt_bp);
	
				printf("After fine tuning:\n");
				printf("White is %f %f %f, should be %f %f %f\n",
				a_wp[0], a_wp[1], a_wp[2], d_mt_wp[0], d_mt_wp[1], d_mt_wp[2]);
				printf("Black is %f %f %f, should be %f %f %f\n",
				a_bp[0], a_bp[1], a_bp[2], d_mt_bp[0], d_mt_bp[1], d_mt_bp[2]);
			}
#endif /* VERBOSE */
		}

		if (wrl != NULL) {
			int arerings = 0;
			double cc[3] = { 0.7, 0.7, 0.7 }; 
			double nc[3] = { 1.0, 0.4, 0.7 }; 	/* Pink for neighbors */
			int nix = -1;						/* Index of point to show neighbour */

#ifdef SHOW_NEIGBORS
#ifdef NEVER
			/* Show all neighbours */
			wrl->start_line_set(wrl, 0);
			for (i = 0; i < nnsm; i++) {
				for (j = 0; j < XNNB; j++) {
					nearsmth *np = nsm[i].n[j];		/* Pointer to neighbor */
			
					if (np == NULL)
						break;

					wrl->add_col_vertex(wrl, 0, nsm[i].sv, nc);	/* Source value */
					wrl->add_col_vertex(wrl, 0, np->sv, nc);	/* Neighbpor value */
				}
			}
			wrl->make_lines(wrl, 0, 2);
#else
			/* Show neighbours of points near source markers */
			for (i = 0; ; i++) {	/* Add diagnostic markers */
				double pp[3];
				co cp;
				int ix, bix;
				double bdist = 1e6;

				if (markers[i].type == 0)
					break;
	
				if (markers[i].type != 1)
					continue;
				
				/* Rotate and map marker point the same as the src gamuts */
				icmMul3By3x4(pp, s->grot, markers[i].pos);
				cp.p[0] = pp[0];			/* L value */
				s->grey->interp(s->grey, &cp);
				pp[0] = cp.v[0];
//printf("~1 looking for closest point to marker %d at %f %f %f\n",i,pp[0],pp[1],pp[2]);

				/* Locate the nearest source point */
				for (ix = 0; ix < nnsm; ix++) {
					double dist = icmNorm33(pp, nsm[ix].sv);
					if (dist < bdist) {
						bdist = dist;
						bix = ix;
					}
				}
//printf("~1 closest src point ix %d at %f %f %f\n",bix,nsm[bix].sv[0],nsm[bix].sv[1],nsm[bix].sv[2]);
//printf("~1 there are %d neighbours\n",nsm[bix].nnb);

				wrl->start_line_set(wrl, 0);
				for (j = 0; j < nsm[bix].nnb; j++) {
					nearsmth *np = nsm[bix].n[j].n;	/* Pointer to neighbor */

					wrl->add_col_vertex(wrl, 0, nsm[bix].sv, nc);	/* Source value */
					wrl->add_col_vertex(wrl, 0, np->sv, nc);		/* Neighbpor value */
				}
				wrl->make_lines(wrl, 0, 2);
			}
#endif
#endif /* SHOW_NEIGBORS */

			/* Add the source and dest gamut surfaces */
#ifdef PLOT_SRC_GMT
			wrl->make_gamut_surface_2(wrl, sil_gam, 0.6, 0, cc);	/* Grey */
#endif /* PLOT_SRC_GMT */
#ifdef PLOT_DST_GMT
			cc[0] = -1.0;
			wrl->make_gamut_surface(wrl, d_gam, 0.3, cc);			/* Natural color */
#endif /* PLOT_DST_GMT */
#ifdef PLOT_DIGAM
			if (nsm[0].dgam == NULL)
				error("Need to #define PLOT_DIGAM in nearsmth.c!");
			cc[0] = -1.0;
			wrl->make_gamut_surface(wrl, nsm[0].dgam, 0.2, cc);
#endif /* PLOT_DIGAM */
#ifdef PLOT_SRC_CUSPS
			wrl->add_cusps(wrl, sil_gam, 0.6, NULL);
#endif /* PLOT_SRC_CUSPS */
#ifdef PLOT_DST_CUSPS
			wrl->add_cusps(wrl, d_gam, 0.3, NULL);
#endif /* PLOT_DST_CUSPS */

#ifdef PLOT_TRANSSRC_CUSPS
			/* Add transformed source cusp markers */
			{
				int i;
				double cusps[6][3];
				double ccolors[6][3] = {
					{ 1.0, 0.1, 0.1 },		/* Red */
					{ 1.0, 1.0, 0.1 },		/* Yellow */
					{ 0.1, 1.0, 0.1 },		/* Green */
					{ 0.1, 1.0, 1.0 },		/* Cyan */
					{ 0.1, 0.1, 1.0 },		/* Blue */
					{ 1.0, 0.1, 1.0 }		/* Magenta */
				};
			
				if (sc_gam->getcusps(sc_gam, cusps) == 0) {

					for (i = 0; i < 6; i++) {
						double val[3];

						s->domap(s, val, cusps[i]);
						wrl->add_marker(wrl, val, ccolors[i], 2.5);
					}
				}
			}
#endif

#if defined(SHOW_MAP_VECTORS) || defined(SHOW_SUB_SURF) || defined(SHOW_ACTUAL_VECTORS) || defined(SHOW_ACTUAL_VEC_DIFF)
			/* Start of guide vector plot */
			wrl->start_line_set(wrl, 0);

			for (i = 0; i < nnsm; i++) {
				double cpexf;			/* The effective compression or expansion factor */
				double yellow[3] = { 1.0, 1.0, 0.0 };
				double red[3]    = { 1.0, 0.0, 0.0 };
				double green[3]  = { 0.0, 1.0, 0.0 };
				double lgrey[3]  = { 0.8, 0.8, 0.8 };
				double purp[3]   = { 0.6, 0.0, 1.0 };
				double blue[3]   = { 0.2, 0.2, 1.0 };
				double *ccc;
				double mdst[3];

#if defined(SHOW_ACTUAL_VECTORS) || defined(SHOW_ACTUAL_VEC_DIFF)
# ifdef SHOW_ACTUAL_VECTORS
				wrl->add_col_vertex(wrl, 0, nsm[i].sv, yellow);
# else	/* SHOW_ACTUAL_VEC_DIFF */
				wrl->add_col_vertex(wrl, 0, nsm[i].div, yellow);
# endif
				dopartialmap2(s, mdst, nsm[i].sv);
				wrl->add_col_vertex(wrl, 0, mdst, red);

#else
# ifdef SHOW_MAP_VECTORS
				ccc = yellow;

				if (nsm[i].gflag == 0)
					ccc = green;			/* Mark "no clear direction" vectors in green->red */
#  ifdef SHOW_CUSPMAP
				wrl->add_col_vertex(wrl, 0, nsm[i].csv, ccc);	/* Cusp mapped source value */
#  else
				wrl->add_col_vertex(wrl, 0, nsm[i].sv, ccc);	/* Source value */
#  endif
				wrl->add_col_vertex(wrl, 0, nsm[i].div, red);	/* Blended destination value */
# endif /* SHOW_MAP_VECTORS */

# ifdef SHOW_SUB_SURF
				if (nsm[i].vflag != 0) {	/* Sub surface point is available */
					wrl->add_col_vertex(wrl, 0, nsm[i].sv2, lgrey); /* Subs-surf Source value */
					wrl->add_col_vertex(wrl, 0, nsm[i].div2, purp); /* Blended destination value */
				}
# endif /* SHOW_SUB_SURF */
#endif	/* !SHOW_ACTUAL_VECTORS */
			}
			wrl->make_lines(wrl, 0, 2);			/* Guide vectors */
#endif	/* Show vectors */

#if defined(SHOW_VECTOR_INDEXES) || defined(SHOW_SUB_PNTS)
			for (i = 0; i < nnsm; i++) {
#ifdef SHOW_VECTOR_INDEXES
				{
					double cream[3] = { 0.7, 0.7, 0.5 };
					char buf[100];
					sprintf(buf, "%d", i);
					wrl->add_text(wrl, buf, nsm[i].sv, cream, 0.5);
				}
#endif /* SHOW_VECTOR_INDEXES */
# ifdef SHOW_SUB_PNTS
				if (nsm[i].vflag != 0) {	/* Sub surface point is available */
					double red[3]    = { 1.0, 0.0, 0.0 };
					double green[3]  = { 0.0, 1.0, 0.0 };
					double yellow[3] = { 1.0, 1.0, 0.0 };

					wrl->add_marker(wrl, nsm[i].sv2, red, 1.0); /* Subs-surf Source value */
					wrl->add_marker(wrl, nsm[i].div2, green, 1.0); /* Blended destination value */
					wrl->add_marker(wrl, nsm[i].sd3, yellow, 1.0); /* Deep sub-surface point */
				}
# endif /* SHOW_SUB_PNTS */
			}
#endif

			/* add the locus from locus.ts file */ 
			if (locus != NULL) {
				int table, npoints;
				char *fnames[3] = { "LAB_L", "LAB_A", "LAB_B" };
				int ix[3];
				double v0[3], v1[3];
				double rgb[3];

				/* Each table holds a separate locus */
				for (table = 0; table < locus->ntables; table++) {

				    if ((npoints = locus->t[table].nsets) <= 0)
				        error("No sets of data in diagnostic locus");

					for (j = 0; j < 3; j++) {
						if ((ix[j] = locus->find_field(locus, 0, fnames[j])) < 0)
							error ("Locus file doesn't contain field %s",fnames[j]);
						if (locus->t[table].ftype[ix[j]] != r_t)
							error ("Field %s is wrong type",fnames[j]);
					}

					/* Source locus */
					rgb[0] = 1.0;
					rgb[1] = 0.5;
					rgb[2] = 0.5;
					for (i = 0; i < npoints; i++) {
						co cp;

						for (j = 0; j < 3; j++)
							v1[j] = *((double *)locus->t[table].fdata[i][ix[j]]);

						/* Rotate and locus verticies the same as the src gamuts */
						dopartialmap1(s, v1, v1);
						if (i > 0 )
							wrl->add_cone(wrl, v0, v1, rgb, 0.5);
						icmAry2Ary(v0,v1);
					}

					/* Gamut mapped locus */
					rgb[0] = 1.0;
					rgb[1] = 1.0;
					rgb[2] = 1.0;
					for (i = 0; i < npoints; i++) {
						co cp;

						for (j = 0; j < 3; j++)
							v1[j] = *((double *)locus->t[table].fdata[i][ix[j]]);

						s->domap(s, v1, v1);
						if (i > 0 )
							wrl->add_cone(wrl, v0, v1, rgb, 0.5);
						icmAry2Ary(v0,v1);
					}
				}
				
				locus->del(locus);
				locus = NULL;
			}

			/* Add any ring mapping diagnostics */
			for (i = 0; ; i++) {
				if (rings[i].type == 0)
					break;

				if (rings[i].type == 2)
					continue;
	
				if (rings[i].type == 1) {
					double pconst;
					double cpoint[3];
					double mat[3][4];		/* translate to our plane */
					double imat[3][4];		/* translate from our plane */
					double s1[3], s0[3], t1[3];
					int j;
					double maxa, mina;
					double maxb, minb;

					if (arerings == 0) {
						arerings = 1;
						wrl->start_line_set(wrl, 1);	/* Source ring */
						wrl->start_line_set(wrl, 2);	/* Destination ring */
					}

					if (icmNormalize3(rings[i].pnorm, rings[i].pnorm, 1.0))
						error("Ring %d diagnostic plane normal failed",i);

					pconst = -icmDot3(rings[i].ppoint, rings[i].pnorm);

					/* Locate intersection of source neautral axis and plane */
					if (icmVecPlaneIsect(cpoint, pconst, rings[i].pnorm, s_cs_wp, s_cs_bp))
						error("Ring %d diagnostic center point intersection failed",i);

					/* Compute the rotation and translation between */
					/* a plane in ab and the plane we are using */
					s0[0] = s0[1] = s0[2] = 0.0;
					s1[0] = 1.0, s1[1] = s1[2] = 0.0;
					t1[0] = cpoint[0] + rings[i].pnorm[0];
					t1[1] = cpoint[1] + rings[i].pnorm[1];
					t1[2] = cpoint[2] + rings[i].pnorm[2];
					icmVecRotMat(mat, s1, s0, t1, cpoint);
					icmVecRotMat(imat, t1, cpoint, s1, s0);

					/* Do a min/max of a circle of vectors so as to */
					/* establish an offset to the centroid for this slice */
					maxa = maxb = -1e60;
					mina = minb = 1e60;
					for (j = 0; j < 20; j++) {
						double ang = 2 * 3.1415926 * j/(20 - 1.0);
						double vec[3], isect[3];
						double pp[3];
						co cp;
						int k;

						vec[0] = 0.0;
						vec[1] = sin(ang);
						vec[2] = cos(ang);
						icmMul3By3x4(vec, mat, vec);

						/* Intersect it with the source gamut */
						if (si_gam->vector_isect(si_gam, vec, cpoint, isect,
						                 NULL, NULL, NULL, NULL, NULL) == 0) {
							continue;
						}

						/* Translate back to plane */
						icmMul3By3x4(pp, imat, isect);

						if (pp[1] > maxa)
							maxa = pp[1];
						if (pp[1] < mina)
							mina = pp[1];
						if (pp[2] > maxb)
							maxb = pp[2];
						if (pp[2] < minb)
							minb = pp[2];
					}
					/* Move center to centroid of min/max box */
					t1[0] = 0.0;
					t1[1] = (maxa + mina) * 0.5;
					t1[2] = (maxb + minb) * 0.5;
					if (t1[1] < -200.0 || t1[1] > 200.0
					 || t1[2] < -200.0 || t1[2] > 200.0)
						error("Failed to locate centroid of slice");
					icmMul3By3x4(cpoint, mat, t1);
					
//printf("~1 ring centroid point = %f %f %f\n", cpoint[0],cpoint[1],cpoint[2]);
		
					/* Recompute the rotation and translation between */
					/* a plane in ab and the plane we are using */
					s0[0] = s0[1] = s0[2] = 0.0;
					s1[0] = 1.0, s1[1] = s1[2] = 0.0;
					t1[0] = cpoint[0] + rings[i].pnorm[0];
					t1[1] = cpoint[1] + rings[i].pnorm[1];
					t1[2] = cpoint[2] + rings[i].pnorm[2];
					icmVecRotMat(mat, s1, s0, t1, cpoint);
					icmVecRotMat(imat, t1, cpoint, s1, s0);

//printf("~1 generating %d ring verts\n",rings[i].nverts);
					/* Create a circle of vectors in the plane from the center */
					/* point, to intersect with the source gamut surface. */
					/* (Duplicate start and end vertex) */
					for (j = 0; j <= rings[i].nverts; j++) {
						double ang = 2 * 3.1415926 * j/((double) rings[i].nverts);
						double vec[3], isect[3];
						double pp[3];
						co cp;
						int k;

						vec[0] = 0.0;
						vec[1] = sin(ang);
						vec[2] = cos(ang);
						icmMul3By3x4(vec, mat, vec);

						/* Intersect it with the source gamut */
						if (si_gam->vector_isect(si_gam, vec, cpoint, isect,
						                 NULL, NULL, NULL, NULL, NULL) == 0) {
							warning("Ring %d vect %d diagnostic vector intersect failed",i,j);
							continue;
						}

//printf("~1 vec %d = %f %f %f\n",j,isect[0],isect[1],isect[2]);

						/* Scale them to the ratio */
						for (k = 0; k < 3; k++)
							vec[k] = isect[k] * rings[i].rad + (1.0 - rings[i].rad) * cpoint[k];

//printf("~1 rad vec %d = %f %f %f\n",j,vec[0],vec[1],vec[2]);

						/* Transform them into rotated and scaled destination space */
						dopartialmap1(s, vec, vec);
//printf("~1 trans vec %d = %f %f %f\n",j,vec[0],vec[1],vec[2]);

						/* Add to plot */
						wrl->add_col_vertex(wrl, 1, vec, rings[i].scol);
//printf("~1 src vec %d = %f %f %f\n",j,vec[0],vec[1],vec[2]);

						/* Gamut map and add to plot */
						s->domap(s, vec, vec);
//printf("~1 dst vec %d = %f %f %f\n",j,vec[0],vec[1],vec[2]);
						wrl->add_col_vertex(wrl, 2, vec, rings[i].dcol);
					}
					wrl->make_last_vertex(wrl, 1);		/* Source ring */
					wrl->make_last_vertex(wrl, 2);		/* Destination ring */
				}
				if (arerings) {
					wrl->make_lines(wrl, 1, 1000000);		/* Source ring */
					wrl->make_lines(wrl, 2, 1000000);		/* Destination ring */
				}
			}

			wrl->del(wrl);		/* Write and delete */
			wrl = NULL;
		}

#ifdef PLOT_3DKNEES
		/* Plot one graph per 3D gamut boundary mapping point */
		for (j = 0; j < p3dk_ix; j++) {
			double xx[XRES];
			double yy[XRES];

			printf("Vector %f %f %f -> %f %f %f\n", p3dk_locus[j].v0[0], p3dk_locus[j].v0[1], p3dk_locus[j].v0[2], p3dk_locus[j].v1[0], p3dk_locus[j].v1[1], p3dk_locus[j].v1[2]);

			for (i = 0; i < XRES; i++) {
				double v;
				co cp;		/* Conversion point */
				v = (i/(double)(XRES-1.0));
				cp.p[0] = p3dk_locus[j].v0[0] + v * (p3dk_locus[j].v1[0] - p3dk_locus[j].v0[0]);
				cp.p[1] = p3dk_locus[j].v0[1] + v * (p3dk_locus[j].v1[1] - p3dk_locus[j].v0[1]);
				cp.p[2] = p3dk_locus[j].v0[2] + v * (p3dk_locus[j].v1[2] - p3dk_locus[j].v0[2]);
				xx[i] = sqrt(cp.p[1] * cp.p[1] + cp.p[2] * cp.p[2]);
				s->map->interp(s->map, &cp);
				yy[i] = sqrt(cp.v[1] * cp.v[1] + cp.v[2] * cp.v[2]);
			}
			do_plot(xx,yy,NULL,NULL,XRES);
		}
		free(p3dk_locus);
#endif /* PLOT_3DKNEES */


		free(gpnts);
		free_nearsmth(nsm, nnsm);

	} else if (diagname != NULL && verb) {
		printf("Warning: Won't create '%s' because there is no 3D gamut mapping\n",diagname);
	}

#ifdef PLOT_GAMUTS
	scl_gam->write_vrml(scl_gam, "src", 1, 0);
	sil_gam->write_vrml(sil_gam, "img", 1, 0);
	d_gam->write_vrml(d_gam, "dst", 1, 0);
	sc_gam->write_trans_vrml(sc_gam, "gmsrc", 1, 0, map_trans, s);
#endif

	if (sil_gam != scl_gam)
		sil_gam->del(sil_gam);
	scl_gam->del(scl_gam);

	return s;
}

#ifdef PLOT_GAMUTS

/* Debug */
static void map_trans(void *cntx, double out[3], double in[3]) {
	gammap *map = (gammap *)cntx;

	map->domap(map, out, in);
}

#endif

/* Object methods */
static void del_gammap(
gammap *s
) {
	if (s->grey != NULL)
		s->grey->del(s->grey);
	if (s->igrey != NULL)
		s->igrey->del(s->igrey);
	if (s->map != NULL)
		s->map->del(s->map);

	free(s);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Apply the gamut mapping to the given color value */
static void domap(
gammap *s,
double *out,
double *in
) {
	double rin[3];
	co cp;

	if (s->dbg) printf("domap: got input %f %f %f\n",in[0],in[1],in[2]);
	icmMul3By3x4(rin, s->grot, in);		/* Rotate */

	if (s->dbg) printf("domap: after rotate %f %f %f\n",rin[0],rin[1],rin[2]);
	cp.p[0] = rin[0];
	s->grey->interp(s->grey, &cp);		/* L map */

	if (s->dbg) printf("domap: after L map %f %f %f\n",cp.v[0],rin[1],rin[2]);

	/* If there is a 3D->3D mapping */
	if (s->map != NULL) {
		int e;

		/* Clip out of range a, b proportionately */
		if (rin[1] < s->imin[1] || rin[1] > s->imax[1]
		 || rin[2] < s->imin[2] || rin[2] > s->imax[2]) {
			double as = 1.0, bs = 1.0;
			if (rin[1] < s->imin[1])
				as = s->imin[1]/rin[1];
			else if (rin[1] > s->imax[1])
				as = s->imax[1]/rin[1];
			if (rin[2] < s->imin[2])
				bs = s->imin[2]/rin[2];
			else if (rin[2] > s->imax[2])
				bs = s->imax[2]/rin[2];
			if (bs < as)
				as = bs;
			rin[1] *= as;
			rin[2] *= as;
		}
	
		cp.p[0] = cp.v[0];					/* 3D map */
		cp.p[1] = rin[1];
		cp.p[2] = rin[2];
		s->map->interp(s->map, &cp);
	
		for (e = 0; e < s->map->fdi; e++)
			out[e] = cp.v[e];
	
		if (s->dbg) printf("domap: after 3D map %s\n\n",icmPdv(s->map->fdi, out));
	} else {
		out[0] = cp.v[0];
		out[1] = rin[1];
		out[2] = rin[2];
	}
}

/* Apply the matrix and grey mapping to the given color value */
static void dopartialmap1(
gammap *s,
double *out,
double *in
) {
	double rin[3];
	co cp;

	icmMul3By3x4(rin, s->grot, in);		/* Rotate */
	cp.p[0] = rin[0];
	s->grey->interp(s->grey, &cp);		/* L map */
	out[0] = cp.v[0];
	out[1] = rin[1];
	out[2] = rin[2];
}

/* Apply just the rspl mapping to the given color value */
/* (ie. to a color already rotated and L mapped) */
static void dopartialmap2(
gammap *s,
double *out,
double *in
) {
	co cp;

	/* If there is a 3D->3D mapping */
	if (s->map != NULL) {
		int e;

		icmCpy3(cp.p, in);

		/* Clip out of range a, b proportionately */
		if (cp.p[1] < s->imin[1] || cp.p[1] > s->imax[1]
		 || cp.p[2] < s->imin[2] || cp.p[2] > s->imax[2]) {
			double as = 1.0, bs = 1.0;
			if (cp.p[1] < s->imin[1])
				as = s->imin[1]/cp.p[1];
			else if (cp.p[1] > s->imax[1])
				as = s->imax[1]/cp.p[1];
			if (cp.p[2] < s->imin[2])
				bs = s->imin[2]/cp.p[2];
			else if (cp.p[2] > s->imax[2])
				bs = s->imax[2]/cp.p[2];
			if (bs < as)
				as = bs;
			cp.p[1] *= as;
			cp.p[2] *= as;
		}
	
		s->map->interp(s->map, &cp);
	
		icmCpy3(out, cp.v);
	} else {
		icmCpy3(out, in);
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* powell function - minimise error to target */
static double invgmfunc(
void *fdata,
double *tp
) {
	gammap *s = (gammap *)fdata;
	int i;
	double gmv[3];
	double tt, rv = 0.0;

	domap(s, gmv, tp);
	for (i = 0; i < 3; i++) {
		double tt = gmv[i] - s->tv[i];
		rv += tt * tt;
	}

	return rv;
}

/* Invert a gamut mapping using powell */
static void invdomap1(
gammap *s,
double *out,
double *in
) {
	double ss[3] = { 20.0, 20.0, 20.0 };		/* search area */
	double tp[3], rv;

	s->tv[0] = tp[0] = in[0];
	s->tv[1] = tp[1] = in[1];
	s->tv[2] = tp[2] = in[2];

	if (powell(&rv, 3, tp, ss, 1e-7, 5000, invgmfunc, (void *)s, NULL, NULL) != 0) {
		warning("gamut invdomap1 failed on %f %f %f\n", in[0], in[1], in[2]);
	}

	out[0] = tp[0];
	out[1] = tp[1];
	out[2] = tp[2];
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Function to pass to rspl to invert grey curve */
static void inv_grey_func(
	void *cntx,
	double *out,
	double *in
) {
	rspl *fwd = (rspl *)cntx;
	int nsoln;		/* Number of solutions found */
	co pp[2];		/* Room for all the solutions found */

	pp[0].p[0] = 
	pp[0].v[0] = in[0];

	nsoln = fwd->rev_interp(
		fwd,
		RSPL_NEARCLIP,		/* Clip to nearest (faster than vector) */
		2,					/* Maximum number of solutions allowed for */
		NULL, 				/* No auxiliary input targets */
		NULL,				/* Clip vector direction and length */
		pp);				/* Input and output values */

	nsoln &= RSPL_NOSOLNS;		/* Get number of solutions */

	if (nsoln != 1)
		error("gammap: Unexpected failure to find reverse solution for grey axis lookup");

	out[0] = pp[0].p[0];
}

/* Function to pass to rspl to alter output values, */
/* to enhance the saturation. */
static void
adjust_sat_func(
	void *pp,			/* adjustsat structure */
	double *out,		/* output value to be adjusted */
	double *in			/* corresponding input value */
) {
	adjustsat *p = (adjustsat *)pp;
	double cp[3];		/* Center point */
	double rr, t1[3], p1;
	double t2[3], p2;

	/* Locate center point on the white/black axis corresponding to this color */
	cp[0] = out[0];
	rr = (out[0] - p->bp[0])/(p->wp[0] - p->bp[0]);	/* Relative location on the white/black axis */
	cp[1] = p->bp[1] + rr * (p->wp[1] - p->bp[1]);
	cp[2] = p->bp[2] + rr * (p->wp[2] - p->bp[2]);
	
	/* Locate the point on the destination gamut surface in the direction */
	/* from the center point to the point being processed. */
	if (p->dst->vector_isect(p->dst, cp, out, t2, t1, &p2, &p1, NULL, NULL) != 0) {

		if (p1 > 1.0) {		/* If this point is within gamut */
			double ep1, bf;

//printf("\n");
//printf("~1 cp %f %f %f input %f %f %f\n",cp[0],cp[1],cp[2], out[0], out[1], out[2]);
//printf("~1 min %f %f %f mint %f\n",t2[0],t2[1],t2[2],p2);
//printf("~1 max %f %f %f maxt %f\n",t1[0],t1[1],t1[2],p1);

			p1 = 1.0/p1;		/* Position of out from cp to t1 */

#ifdef NEVER
			/* Enhanced parameter value */
			ep1 = (p1 + p->satenh * p1)/(1.0 + p->satenh * p1);
			/* Make blend between linear p1 and enhanced p1, */
			/* to reduce effects on near neutrals. */
			p1 = (1.0 - p1) * p1 + p1 * ep1;
#else
			/* Compute Enhanced p1 */
			ep1 = (p1 + p->satenh * p1)/(1.0 + p->satenh * p1);

			/* Make blend factor between linear p1 and enhanced p1, */
			/* to reduce effects on near neutrals. */
			{
				double pp = 4.0;		/* Sets where the 50% transition is */
				double g = 2.0;			/* Sets rate of transition */
				double sec, vv = p1;
		
				vv = vv/(pp - pp * vv + 1.0);

				vv *= 2.0;
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
				vv *= 0.5;

				bf = (vv + pp * vv)/(1.0 + pp * vv);
			}
			/* Do the blend */
			p1 = (1.0 - bf) * p1 + bf * ep1;
#endif
			/* Compute enhanced values position */
			out[0] = cp[0] + (t1[0] - cp[0]) * p1;
			out[1] = cp[1] + (t1[1] - cp[1]) * p1;
			out[2] = cp[2] + (t1[2] - cp[2]) * p1;
//printf("~1 output %f %f %f, param %f\n",out[0],out[1],out[2],p1);
		}
	}
}

/* Function to pass to rspl to re-set output values, */
/* to adjust the white and black points */
static void
adjust_wb_func(
	void *pp,			/* adjustwb structure */
	double *out,		/* output value to be adjusted */
	double *in			/* corresponding input value */
) {
	adjustwb *p = (adjustwb *)pp;

	/* Do a linear mapping from swp -> dwp and sbp -> dbp, */
	/* to compute the adjusted value. */
	icmMul3By3x4(out, p->mat, out);
}


/* Create a new gamut that the the given gamut transformed by the */
/* gamut mappings rotation and grey curve mapping. Return NULL on error. */
static gamut *parttransgamut(gammap *s, gamut *src) {
	gamut *dst;
	double cusps[6][3];
	double wp[3], bp[3], kp[3];
	double p[3];
	int i;

	if ((dst = new_gamut(src->getsres(src), src->getisjab(src), src->getisrast(src))) == NULL)
		return NULL;

	dst->setnofilt(dst);

	/* Translate all the surface nodes */
	for (i = 0;;) {
		if ((i = src->getrawvert(src, p, i)) < 0)
			break;

		dopartialmap1(s, p, p); 
		dst->expand(dst, p);
	}
	/* Translate cusps */
	if (src->getcusps(src, cusps) == 0) {
		dst->setcusps(dst, 0, NULL);
		for (i = 0; i < 6; i++) {
			dopartialmap1(s, p, cusps[i]); 
			dst->setcusps(dst, 1, p);
		}
		dst->setcusps(dst, 2, NULL);
	}
	/* Translate white and black points */
	if (src->getwb(src, wp, bp, kp, NULL, NULL, NULL) == 0) {
		dopartialmap1(s, wp, wp); 
		dopartialmap1(s, bp, bp); 
		dopartialmap1(s, kp, kp); 
		dst->setwb(dst, wp, bp, kp);
	}

	return dst;
}




















