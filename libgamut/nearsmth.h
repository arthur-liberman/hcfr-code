#ifndef NEARSMTH_H
#define NEARSMTH_H

/* 
 * nearsmth
 *
 * Gamut mapping support routine that creates a list of
 * guide vectors from the source to destination
 * gamut, smoothed to retain reasonably even spacing.
 *
 * Author:  Graeme W. Gill
 * Date:    17/1/2002
 * Version: 1.00
 *
 * Copyright 2002 - 2006 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */


/* Settings for nearsmth & gammap */

#define GAMMAP_RSPLFLAGS (0)		/* Default rspl flags */
#define GAMMAP_RSPLAVGDEV 0.005		/* Default average deviation */

/* Structures to hold weightings */

/* Color x light & dark = 14 combinations */
typedef enum {
	/* Combinations in prioroty order */
	gmm_light_red     = 0x101,
	gmm_light_yellow  = 0x102,
	gmm_light_green   = 0x104,
	gmm_light_cyan    = 0x108,
	gmm_light_blue    = 0x110,
	gmm_light_magenta = 0x120,
	gmm_light_neutral = 0x140,

	gmm_dark_red      = 0x201,
	gmm_dark_yellow   = 0x202,
	gmm_dark_green    = 0x204,
	gmm_dark_cyan     = 0x208,
	gmm_dark_blue     = 0x210,
	gmm_dark_magenta  = 0x220,
	gmm_dark_neutral  = 0x240,

	gmm_l_d_red       = 0x301,
	gmm_l_d_yellow    = 0x302,
	gmm_l_d_green     = 0x304,
	gmm_l_d_cyan      = 0x308,
	gmm_l_d_blue      = 0x310,
	gmm_l_d_magenta   = 0x320,
	gmm_l_d_neutral   = 0x340,

	gmm_light_colors  = 0x17f,	/* All light colors */
	gmm_dark_colors   = 0x27f,	/* All dark colors */

	gmm_default       = 0x37f,	/* All light and dark colors */ 

	gmm_end     = 0x0000,		/* Mark the end of the list */
	gmm_ignore  = 0x1001,		/* Ignore this entry */

	/* Components */
	gmc_light   = 0x100,
	gmc_dark    = 0x200,
	gmc_l_d     = 0x300,	/* Both Light and dark */
	gmc_red     = 0x001,
	gmc_yellow  = 0x002,
	gmc_green   = 0x004,
	gmc_cyan    = 0x008,
	gmc_blue    = 0x010,
	gmc_magenta = 0x020,
	gmc_neutral = 0x040,
	gmc_colors  = 0x07f		/* All colors */

} gmm_chex;

/* Single weight */
typedef struct {
	double l,c,h;
} iweight;

/* Group of complete weights */
/* (Remeber to alter near_wcopy() and near_wblend() !!) */
typedef struct {

	gmm_chex ch;			/* Color hextant this applies to */

	/* Cusp alignment control */
	struct {
		iweight w;			/* Component alignment weights, 0 - 1 */
		double tw;			/* Alignment twist power, 0 = linear, 1 = curve, 2+ late curve */ 
							/* Use > 0 to make cusp alignment only affect surface colors. */
		double cx;			/* Chroma expansion, 1 = none, > 1 = more */
	} c;

	/* Radial weighting (This seems to be not working at the moment ?) */
	/* Weight to give to minimizing delta E to source mapped radially */
	struct {
		double o;			/* Overall Radial weight */
		double h;			/* Hue dominance vs l+c, 0 - 1 */
		double l;			/* l dominance vs, c, 0 - 1 */
	} l;

	/* Absolute error weighting. */
	/* Weight given to minimizing delta E to destination closest point */
	struct {
		double o;			/* Overall Absolute weight */

		double h;			/* Hue dominance vs l+c, 0 - 1 */

		double wl;			/* White l dominance vs, c, 0 - 1 */
		double gl;			/* Grey l dominance vs, c, 0 - 1 */
		double bl;			/* Black l dominance vs, c, 0 - 1 */

		double wlth;		/* White l blend start radius, 0 - 1, at white = 0 */
		double blpow;		/* Black l blend power, linear = 1.0, enhance < 1.0 */

		double lxpow;		/* L error extra power, none = 1.0 */
		double lxthr;		/* L error xover threshold in DE */
	} a;

	/* Relative vector smoothing. */
	struct {
		double rdl;			/* Direction smoothing radius L* dir. (delta E radius at src point)*/
		double rdh;			/* Direction smoothing radius H* (delta E radius at src point)*/
	} r;

	/* depth weighting */
	/* Weighing to give to minimizing depth ratio by mapping to/from adequate dest/src depth */
	struct {
		double co;			/* Overall compression weighting */
		double xo;			/* Overall expansion weighting */
	} d;

	/* Fine tuning destination gamut mapping */
	struct {
		double x;			/* Final expansion weight, 0 - 1 */
	} f;					/* Weights fine tuning to expand rather than just compress */

	/* Internal use */
	iweight rl;				/* Resolved radial weight */
	iweight ra;				/* Resolved absolute weight */

	int set;				/* Whether this has been set */
} gammapweights;


/* Blend a two groups of weights into one, given two weightings */
void near_wblend(gammapweights *dst,
	             gammapweights *src1, double wgt1, gammapweights *src2, double wgt2);


/* A neighbour and it's weight for relative error */
typedef struct {
	struct _nearsmth *n;	/* Pointer to neigbor */
	double rw;				/* Raw neighbor interpolation weight (1.0 at point) */
	double w;				/* Neighbor interpolation weight (sums to 1.0) */
} neighb;

/* Returned point values. */
/* Note that source == input point, destination == output point */
/* (All the source values are in rotated L mapped partial gamut mapped space) */
struct _nearsmth {

  /* Public: */
	int    gflag;		/* Gamut direction flag. 0 = not determinable, 1 = comp., 2 = exp. */
	int    vflag;		/* Vector direction flag. 0 = not determinable, 1 = comp., 2 = exp. */
						/* sv2, dv2, sd3 etc. are valid if vflag != 0 */

	/* Gamut surface mapping guide point */
	double sv[3];		/* Source value (input, cusp aligned during fwd optimization) */
	double sr;			/* Corresponding source radius from center */
	double drv[3];		/* Destination radial value (starting point & reference) */
	double drr;			/* Destination radial value radius from center */
	double dv[3];		/* Output destination value */
	double dr;			/* Output destination value radius from center */
	double div[3];		/* gam[cx]pf moderated dv[] value */

	/* Gamut sub-surface mapping guide point (knee shape controlled by gamcknf & gamxknf) */
	double sv2[3];		/* Sub-surface source value */
	double dv2[3];		/* Sub-surface knee'd adjusted destination value */
	double div2[3];		/* gam[cx]pf moderated dv2[] value */
	double w2;			/* Sub-surface weight (fixed in nearsmth) */

	double sd3[3];		/* Deep sub-surface source & destination value */
	double w3;			/* Deep sub-surface weight */

	/* Diagnostic points */
	double csv[3];		/* Non-cusp mapped source value */

 /* Private to nearsmth: */
	gammapweights wt;	/* Weight for this point determined from original source location */
	int swap;			/* Swapped during nearest finding */
	double mL, fmL;		/* Mid point L value, filtered mid point L value */
	double m2d[3][4];   /* Tangent alignment rotation for sv[] 3D -> 2D */
	double m3d[3][4];   /* Tangent alignment rotation for 2D -> sv[] 3D */
	double _sv[3];		/* Original (non cusp aligned) source value (input) */
	double _sr;			/* Original source radius */
	double naxbf;		/* Blend factor that goes to 0.0 at white & black points. */
	double aodv[3];		/* Absolute error optimized destination value */
	double nrdv[3];		/* No relative weight optimized destination value */
	double anv[3];		/* Average neighborhood target point (relative target) */

	double tdst[3];		/* Target destination on gamut */
	double evect[3];	/* Accumulated extension vector direction */
	double clen;		/* Current correction length needed */
	double coff[3];		/* Correction offset */
	double rext;

	double temp[3];		/* General temporary */
	gamut *sgam;		/* Source gamut sci_gam = intersection of src and img gamut gamut */
	gamut *dgam;		/* Destination gamut di_gam */

	int nnd, _nnd;		/* No & size of direction neighbour list */
	neighb *nd;			/* Allocated list of neighbours */

	double dcratio;		/* Depth compression ratio */ 
	double dxratio;		/* Depth expansion ratio */ 

	int mapres;			/* Target grid res for 3D RSPL */

	int debug;
	double dbgv[4];		/* Error components va, vr, vl, vd on last itteration */

}; typedef struct _nearsmth nearsmth;


/* Return the upper bound on the number of points that will be generated */
int near_smooth_np(
	gamut *sc_gam,		/* Source colorspace gamut */
	gamut *s_gam,		/* Source image gamut (== sc_gam if none) */
	gamut *d_gam,		/* Destination colorspace gamut */
	double xvra			/* Extra vertex ratio */
);

/* Return a list of points. Call free_nearsmth() after use */
/* Return NULL on error */
nearsmth *near_smooth(
	int verb,			/* Verbose flag */
	int *npp,			/* Return the number of points returned */
	gamut *sc_gam,		/* Source colorspace gamut */
	gamut *s_gam,		/* Source image gamut (== sc_gam if none) */
	gamut *d_gam,		/* Destination colorspace gamut */
	int src_kbp,		/* Use K only black point as src gamut black point */
	int dst_kbp,		/* Use K only black point as dst gamut black point */
	double d_bp[3],		/* Destination target black point - may be NULL */
	gammapweights *wh,  /* Structure holding weights */
	double gamcknf,		/* Gamut compression knee factor, 0.0 - 1.0 */
	double gamxknf,		/* Gamut expansion knee factor, 0.0 - 1.0 */
	int    usecomp,		/* Flag indicating whether smoothed compressed value will be used */
	int    useexp,		/* Flag indicating whether smoothed expanded value will be used */
	double xvra,		/* Extra vertex ratio */
	int    mapres,		/* Target grid res for 3D RSPL */
	double mapsmooth,	/* Target smoothing for 3D RSPL */
	datai map_il,		/* Preliminary rspl input range */
	datai map_ih,
	datao map_ol,		/* Preliminary rspl output range */
	datao map_oh 
);

/* Free the list of points that was returned */
void free_nearsmth(nearsmth *smp, int npp);

/* Expand the compact form of weights into the explicit form. */
int expand_weights(gammapweights out[14], gammapweights *in);
	
/* Blend a two expanded groups of individual weights into one */
void near_xwblend(
gammapweights *dst,
gammapweights *src1, double wgt1,
gammapweights *src2, double wgt2
);

/* Tweak weights acording to extra cmy cusp flags or rel override */
void tweak_weights(gammapweights out[14], int dst_cmymap, int rel_oride);

#endif /* NEARSMTH_H */

