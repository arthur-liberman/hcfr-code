
/*****************************************************************/
/* Smoothness factor tuning of RSPL in N Dimensions, using MPP's */
/*****************************************************************/

/* Author: Graeme Gill
 * Date:   28/11/2005
 * Derived from cmatch.c
 * Copyright 1995 - 2005 Graeme W. Gill
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Test set for tuning smoothness factor for optimal interpolation
 * with respect to dimension, number of sample points, and uncertainty
 * of the sample points.
 *
 * The reference is an RGB or CMYK .mpp profile. For 1 and 2 dimensions,
 * combinations of 1 or 2 input channels are used.
 */


#undef DEBUG

#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include "rspl.h"
#include "numlib.h"
#include "xicc.h"
#include "rspl_imp.h"
#include "counters.h"	/* Counter macros */
#include "plot.h"
#include "ui.h"

#ifdef DEBUG
#define DBG(xxxx)    printf xxxx
#else
#define DBG(xxxx)
#endif

#define MXCHPARAMS 8

#define PLOTRES 256

/* Reference convertion object */
struct _refconv {
	char *fname;	/* File name */
	mpp *mppo;		/* Underlying MPP */
	inkmask imask;	/* Device Ink mask */
	int pdi;		/* mpp input dim */
	int di;			/* effective input dim */
	int ix;			/* channel to use/not use */
	double dmedia[MXDI];	/* Media color */

	/* Do reference lookup */
	void (*lookup)(
		struct _refconv *s,	/* this */
		double *out,
		double *in);

}; typedef struct _refconv refconv;


/* Do a straight conversion */
static void refconv_default(
refconv *rco,
double *out,
double *in) {
	rco->mppo->lookup(rco->mppo, out, in);
//printf("~1 3/4D %f %f %f -> %f %f %f\n", in[0], in[1], in[2], out[0], out[1], out[2]);
}

/* Do a 1d emulation */
static void refconv_1d(
refconv *s,
double *out,
double *in
) {
	double dval[MXDI];
	int e;

	for (e = 0; e < s->pdi; e++) {
		if (e == s->ix) {
			if (s->imask & ICX_INVERTED)
				dval[e] = 1.0 - in[0];
			else
				dval[e] = in[0];
		} else {
			dval[e] = s->dmedia[e];
		}
	}
	s->mppo->lookup(s->mppo, out, dval);
//printf("~1 1D %f == %f %f %f -> %f %f %f\n", in[0], dval[0], dval[1], dval[2], out[0], out[1], out[2]);
}

/* Do a 2d emulation */
static void refconv_2d(
refconv *s,
double *out,
double *in
) {
	double dval[MXDI];
	int e, j;

	for (j = e = 0; e < s->pdi; e++) {
		if (e < 3 && e != s->ix) {
			if (s->imask & ICX_INVERTED)
				dval[e] = 1.0 - in[j++];
			else
				dval[e] = in[j++];
		} else {
			dval[e] = s->dmedia[e];
		}
	}
	
	s->mppo->lookup(s->mppo, out, dval);
//printf("~1 2D %f %f == %f %f %f -> %f %f %f\n", in[0], in[1], dval[0], dval[1], dval[2], out[0], out[1], out[2]);
}


/* Setup the reference convertion object to imitate the given dimentionality */
/* return nz if the given idex is out of range */
static int set_refconv(
	refconv *s,
	int ix,				/* Index of convertion, typicall 0-3 */
	int di				/* Chosen dimentionalty */
) {
	int e;

	s->di = di;
	s->ix = ix;

	s->lookup = NULL;

	if (di == s->pdi) {
		if (ix == 0) {
			s->lookup = refconv_default;
			return 0;
		} else {
			return 1;
		}
	}
	if (di == 3 || di == 4)
		return 1;

	/* Have to emulate a lower dimension. */

	/* Decide what the media color is */
	if (s->imask & ICX_INVERTED) {
		for (e = 0; e < s->pdi; e++)
			s->dmedia[e] = 1.0;
	} else {
		for (e = 0; e < s->pdi; e++)
			s->dmedia[e] = 0.0;
	}
	
	/* See where we're up to */
	if (di == 1) {
		if (ix < 0 || ix >= s->pdi)	/* RGB or CMYK channels */
			return 1;

		s->lookup = refconv_1d;
		return 0;

	} else if (di == 2) {
		if (ix < 0 || ix >= 3)	/* Just RGB or CMY */
			return 1;
		s->di = di;
		s->lookup = refconv_2d;
		return 0;
	}

	return 0;
}

/* ---------------------------------------------------------------------- */

/* Do one set of tests and return the results */
static void do_test(
	refconv *rco,
	double *trmse,		/* RETURN total RMS error */
	double *tmaxe,		/* RETURN total maximum error */
	double *tavge,		/* RETURN total average error */
	int verb,			/* Verbosity */
	int plot,			/* Plot graphs */
	int di,				/* Dimensions */
	int res,			/* RSPL grid resolution */
	int ntps,			/* Number of sample points */
	double noise,		/* Sample point noise volume */
	int unif,			/* nz for uniform noise, else normal */
	double smooth		/* Smoothness to test */
);

/* Compute smoothness of function */
static double do_stest(
	refconv *rco,
	int verb,			/* Verbosity */
	int di,				/* Dimensions */
	int its,			/* Number of function tests */
	int res				/* RSPL grid resolution */

);



/* ---------------------------------------------------------------------- */
/* Locate minimum of smoothness series result */

#define MXMSS 50	/* Maximum smoothness series */

/* Return the optimal smoothness value, based on the */
/* minimum RMS value. */
static double best(int n, double *rmse, double *smv) {
	int i, bi;
	rspl *curve;
	co *tps = NULL;
	int ns = 2000;			/* Number of samples */
	datai low,high;
	int gres[1];
	datai dlow,dhigh;
	double avgdev[1];
	double brmse;			/* best solution value */
	double blsmv = 0.0;		/* best solution location */
	double rv;				/* Return value */

	/* Create interpolated curve */
	if ((curve = new_rspl(RSPL_NOFLAGS,1, 1)) == NULL)
		error ("New rspl failed");

	/* Create the list of sampling points */
	if ((tps = (co *)malloc(n * sizeof(co))) == NULL)
		error ("malloc failed");

	for (i = 0; i < n; i++) {
		tps[i].p[0] = log10(smv[i]);
		tps[i].v[0] = rmse[i]; 
	}

	gres[0] = 100;
	low[0] = log10(smv[0]);
	high[0] = log10(smv[n-1]);
	dlow[0] = 0.0;
	dhigh[0] = 1.0;
	avgdev[0] = 0.0;

	curve->fit_rspl(curve,
	           0,					/* Non-mon and clip flags */
	           tps,					/* Test points */
	           n,					/* Number of test points */
	           NULL, NULL, gres,	/* Low, high, resolution of grid */
	           NULL, NULL,			/* Default data scale */
	           -0.0007,				/* Underlying smoothing */
	           avgdev,				/* Average deviation */
	           NULL);				/* iwidth */

#ifdef NEVER
	/* Check the fit */
	for (i = 0; i < n; i++) {
		co tp;

		tp.p[0] = log10(smv[i]);
		curve->interp(curve, &tp);

		printf("Point %d at %f, should be %f is %f\n",i,log10(smv[i]),rmse[i],tp.v[0]);
	}
#endif

	/* Choose a solution */
	brmse = 1e38;

	/* Find lowest rms error point */
	for (i = ns-1; i >= 0; i--) {
		co tp;
		double vi;

		vi = i/(ns-1.0);
		tp.p[0] = log10(smv[0]) + (log10(smv[n-1]) - log10(smv[0])) * vi;
		curve->interp(curve, &tp);

		if (tp.v[0] < brmse) {
			blsmv = tp.p[0];
			brmse = tp.v[0];
			bi = i;
		}
	}

	/* Then increase smoothness until fit error is 1% higher */
	for (i = bi+1; i < ns; i++) {
		co tp;
		double vi;

		vi = i/(ns-1.0);
		tp.p[0] = log10(smv[0]) + (log10(smv[n-1]) - log10(smv[0])) * vi;
		curve->interp(curve, &tp);

		if (tp.v[0] >= (1.01 * brmse)) {
			blsmv = tp.p[0];
			brmse = tp.v[0];
			break;
		}
	}
	rv = pow(10.0, blsmv);

#ifdef NEVER

#define TPRES 100
	/* Plot the result */
	{
		double xx[TPRES], yy[TPRES];

		for (i = 0; i < TPRES; i++) {
			co tp;
			double vi = i/(TPRES-1.0);
	
			tp.p[0] = log10(smv[0]) + (log10(smv[n-1]) - log10(smv[0])) * vi;
			curve->interp(curve, &tp);
			xx[i] = tp.p[0];
			yy[i] = tp.v[0];
		}
		printf("Best at %f\n",blsmv);
		do_plot(xx,yy,NULL,NULL,TPRES);
	}
#endif

	return rv;
}

/* ---------------------------------------------------------------------- */
/* Explore ideal smoothness change with test point number and noise volume */
static void do_series_1(int verb, int plot, refconv* rco, int tdi, int unif, int tntps, int tnlev) {
	int sdi = 1, edi = 4, di;
	int res = 0;
	int ntps = 0;
	double noise = 0.0;
	double smooth = 0.0;
	double trmse, tavge, tmaxe;
	int i, j, k;

	/* Resolution of grid for each dimension */
	int reses[4][4] = {
		{ 257, 129, 65, 33 }, 
		{ 128, 65, 33, 17 },
		{ 65, 33, 17, 9 },
		{ 33, 17, 9, 5 }
	};

	/* Set of smoothnesses to explore */
	double smset[4][20] = {
		{
			-0.00000001,
			-0.00000010,
			-0.00000100,
			-0.00001000,
			-0.00010000,
			-0.00100000,
			-0.01000000,
			-0.10000000,
			-1.00000000,
			0.0
		},
		{
			-0.0000001,
			-0.0000010,
			-0.0000100,
			-0.0001000,
			-0.0010000,
			-0.0100000,
			-0.1000000,
			-1.0000000,
			0.0
		},
		{
			-0.0000001,
			-0.0000010,
			-0.0000100,
			-0.0001000,
			-0.0010000,
			-0.0100000,
			-0.1000000,
			-1.0000000,
			0.0
		},
		{
			-0.0000010,
			-0.0000100,
			-0.0001000,
			-0.0010000,
			-0.0100000,
			-0.1000000,
			-1.0000000,
			-10.000000,
			0.0
		}
	};

	/* Set of sample points to explore */
	int nset[4][20] = {
		{
			5, 10, 20, 50, 100, 200, 0											/* di = 1 */
		},
		{
			25, 100, 200, 400, 1000, 2500, 10000, 40000,						/* di = 2 */
		},
		{
			10, 25, 75, 125, 250, 500, 1000, 2000, 4000, 8000, 16000, 30000, 100000		/* di = 3 */
		},
		{
			100, 200, 450, 625, 900, 1200, 1800, 3600, 10000, 200000, 500000		/* di = 4 */
		}
	};

	/* Set of noise levels to explore (average deviation * 4) */
	double noiseset[4][20] = {
		{
			0.0,		/* Perfect data */
			0.005,		/* 0.2 % */
			0.01,		/* 1.0 % */
			0.02,		/* 2.0 % */
			0.05,		/* 5.0 % */
			0.10,		/* 10.0 % */
//			0.20,		/* 20.0 % */
			-1.0
		},
		{
			0.0,		/* Perfect data */
			0.005,		/* 0.2 % */
			0.01,		/* 1.0 % */
			0.02,		/* 2.0 % */
			0.05,		/* 5.0 % */
			0.10,		/* 10.0 % */
//			0.20,		/* 20.0 % */
			-1.0
		},
		{
			0.0,		/* Perfect data */
			0.005,		/* 0.2 % */
			0.01,		/* 1.0 % */
			0.02,		/* 2.0 % */
			0.05,		/* 5.0 % */
			0.10,		/* 10.0 % */
//			0.20,		/* 20.0 % */
			-1.0
		},
		{
			0.0,		/* Perfect data */
			0.005,		/* 0.2 % */
			0.01,		/* 1.0 % */
			0.02,		/* 2.0 % */
			0.05,		/* 5.0 % */
			0.10,		/* 10.0 % */
//			0.20,		/* 20.0 % */
			-1.0
		},
	};

	printf("Testing underlying smoothness\n");

	printf("Profile is '%s'\n",rco->fname);

	/* For dimensions */
	if (tdi != 0)
		sdi = edi = tdi;
	DBG(("sdi = %d, edi = %d\n",sdi,edi));
	for (di = sdi; di <= edi; di++) {		// dimensions

		res = reses[di-1][1];				/* Just 2nd highest res */

		printf("Dimensions %d\n",di);
		printf("RSPL resolution %d\n",res);

		/* For number of sample points */
		for (i = 0; i < 20; i++) {
			ntps = nset[di-1][i]; 

			if (ntps == 0) {
				DBG(("nset[%d][%d] = %d, ntps == 0\n",di-1,i,nset[di-1][i]));
				break;
			}

			if (tntps != 0 && ntps != tntps) {		/* Skip any not requested */
				DBG(("tntps %d != 0 && ntps %d != tntps\n",tntps,ntps));
				continue;
			}
		
			printf("\nNo. Sample points %d\n",ntps);

			/* For noise levels */
			for (j = tnlev; j < 20; j++) {
				double smv[20];
				double rmse[20];
				double maxe[20];
				double bfit;

				int ix;
				double avgbest = 0.0;	/* Average best smoothness */

				if (tnlev != 0 && j != tnlev) {
					DBG(("tnlev != 0 && j != tnlev\n"));
					break;
				}

				noise = noiseset[di-1][j];
				if (noise < 0.0)
					break;
				printf("\nNoise volume %f%%\n",noise * 100.0);

				/* For each channel combination within profile */
				for (ix = 0; ; ix++) {
	
					if (set_refconv(rco, ix, di)) {
						DBG(("set_refconv returned nz with ix %f di %d\n",ix,di));
						break;
					}
	
					if (di == 1 || di == 2)
						printf("Channel %d\n",ix);

					/* For smooth factors */
					for (k = 0; k < 20; k++) {
						smooth = smset[di-1][k];
						if (smooth == 0.0) {
							DBG(("smooth == 0\n"));
							break;
						}
					
						printf("Smooth %9.8f, ",-smooth); fflush(stdout);

						do_test(rco, &trmse, &tmaxe, &tavge, verb, plot, di, res, ntps, noise,
						                                         unif, smooth);
						smv[k] = -smooth;
						rmse[k] = trmse;
						maxe[k] = tmaxe;

						printf("maxerr %f, avgerr %f, rmserr %f\n", tmaxe, tavge, trmse);
					}
					bfit = best(k, rmse, smv);		/* Best or RMS */
					printf("Best smoothness = %9.7f, log10 = %4.2f\n",bfit,log10(bfit));
					avgbest += log10(bfit);
				}
				if (ix > 0) {
					avgbest /= (double)ix;
					printf("Average best smoothness of %d = %9.7f, log10 = %4.2f\n",ix,pow(10.0,avgbest),avgbest);
				}
			}
			
		}
		printf("\n");
	}
}

/* Verify the current behaviour with test point number and noise volume */
static void do_series_2(int verb, int plot, refconv *rco, int di, int unif) {
	int res = 0;
	int ntps = 0;
	double noise = 0.0;
	double smooth = 0.0;
	double trmse, tavge, tmaxe;
	int i, j, k;

	/* Number of trials to do for each dimension */
	int trials[4] = {
		12,
		10,
		8,
		5
	};

	/* Resolution of grid for each dimension */
	int reses[4] = {
		129,
		65,
		33,
		17
	};

	/* Set of smoothnesses to explore */
	double smset[5] = {
		00.01,
		00.10,
		01.00,
		10.00,
		100.0
	};
	
#ifdef NEVER
	/* Set of sample points to explore */
	int nset[4][20] = {
		{
			5, 10, 20, 50, 100, 200, 0											/* di = 1 */
		},
		{
			25, 100, 400, 2500, 10000, 40000, 0,								/* di = 2 */
		},
		{
			25, 50, 75, 125, 250, 500, 1000, 2000, 8000, 125000, 0,				/* di = 3 */
		},
		{
			50, 100, 200, 450, 625, 900, 1800, 3600, 10000, 160000, 1000000, 0,	/* di = 4 */
		}
	};

#else
	/* Set of sample points to explore */
	int nset[4][20] = {
		{
			5, 10, 20, 50, 0
		},
		{
			25, 100, 400, 2500 , 0
		},
		{
			250, 500, 1000, 2000, 4000, 8000, 0
		},
		{
			450, 900, 1800, 3600, 0
		}
	};
#endif /* NEVER */

	/* Set of noise levels to explore */
	double noiseset[6] = {
		0.0,		/* Perfect data */
		0.01,		/* 1.0 % */
		0.02,		/* 2.0 % */
		0.05,		/* 5.0 % */
		0.10,		/* 10.0 % */
		0.20,		/* 20.0 % */
	};

	res = reses[di-1];

	printf("Verification\n");
	printf("Dimensions %d\n",di);
	printf("RSPL resolution %d\n",res);

	/* For number of sample points */
	for (i = 0; i < 20; i++) {
		ntps = nset[di-1][i]; 

		if (ntps == 0)
			break;

		printf("No. Sample points %d\n",ntps);

		/* For noise levels */
		for (j = 0; j < 6; j++) {
			noise = noiseset[j];

			printf("Noise volume %f%%\n",noise * 100.0);

			/* For smooth factors */
			for (k = 0; k < 5; k++) {
				smooth = smset[k];
			
				printf("Smooth %9.7f, ",smooth); fflush(stdout);

				do_test(rco, &trmse, &tmaxe, &tavge, verb, plot, di, res, ntps, noise, unif, smooth);

				printf("maxerr %f, avgerr %f, rmserr %f\n", tmaxe, tavge, trmse);
			}
		}
	}
	printf("\n");
}

/* ---------------------------------------------------------------------- */
void usage(void) {
	fprintf(stderr,"Test smoothness factor tuning of RSPL in N Dimensions with MPP\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: smtmpp [options] profile.mpp\n");
	fprintf(stderr," -v            Verbose\n");
	fprintf(stderr," -p            Plot graphs\n");
	fprintf(stderr," -z n          Do test series ""n""\n");
	fprintf(stderr,"               1 = underlying smoothness\n");
	fprintf(stderr,"               2 = verification of optimal smoothness\n");
	fprintf(stderr," -S            Compute smoothness factor instead\n");
	fprintf(stderr," -u            Use uniformly distributed noise\n");
	fprintf(stderr," -d n          Test ""d"" dimension, 1-4  (default 1)\n");
	fprintf(stderr," -r res        Rspl resolution (defaults 129, 65, 33, 17)\n");
	fprintf(stderr," -n no         Test ""no"" sample points (default 20, 40, 80, 100)\n");
	fprintf(stderr," -a amnt       Add total level amnt randomness (default 0.0)\n");
	fprintf(stderr," -A n          Just do the n'th noise level of series\n");
	fprintf(stderr," -s smooth     RSPL extra smoothness factor to test (default 1.0)\n");
	fprintf(stderr," -g smooth     RSPL underlying smoothness factor to test\n");
	fprintf(stderr," -X ix         Select input channel for 1/2D emulation, 0..3\n");
	fprintf(stderr," profile.mpp   MPP profile to use\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	char prof_name[500];
	refconv rco;
	char *ident = NULL;					/* Device colorspec description */
	int verb = 0;
	int plot = 0;
	int series = 0;
	int unif = 0;
	int di = 0;				/* Test input dimensions */
	int ix = 0;				/* One off test output channel */
	int its = 3;			/* Smooth test itterations */
	int res = -1;
	int ntps = 0;
	double noise = 0.0;
	int nlev = 0;
	double smooth = 1.0;
	double gsmooth = 0.0;
	int smfunc = 0;
	double trmse, tavge, tmaxe;

	int rv;

	error_program = "smtmpp";

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

			if (argv[fa][1] == '?') {
				usage();

			} else if (argv[fa][1] == 'v') {
				verb = 1;

			} else if (argv[fa][1] == 'p') {
				plot = 1;

			} else if (argv[fa][1] == 'u') {
				unif = 1;

			/* Test series */
			} else if (argv[fa][1] == 'z') {
				fa = nfa;
				if (na == NULL) usage();
				series = atoi(na);
				if (series <= 0) usage();

			/* Compute smoothness factor */
			} else if (argv[fa][1] == 'S') {
				smfunc = 1;

			/* Dimension */
			} else if (argv[fa][1] == 'd') {
				fa = nfa;
				if (na == NULL) usage();
				di = atoi(na);
				if (di <= 0 || di > 4) usage();

			/* Resolution */
			} else if (argv[fa][1] == 'r') {
				fa = nfa;
				if (na == NULL) usage();
				res = atoi(na);
				if (res <= 0) usage();

			/* Number of sample points */
			} else if (argv[fa][1] == 'n') {
				fa = nfa;
				if (na == NULL) usage();
				ntps = atoi(na);
				if (ntps <= 0) usage();

			/* Randomness */
			} else if (argv[fa][1] == 'a') {
				fa = nfa;
				if (na == NULL) usage();
				noise = atof(na);
				if (noise < 0.0) usage();

			/* Series Noise Level */
			} else if (argv[fa][1] == 'A') {
				fa = nfa;
				if (na == NULL) usage();
				nlev = atoi(na);
				if (noise < 0) usage();

			/* Extra smooth factor */
			} else if (argv[fa][1] == 's') {
				fa = nfa;
				if (na == NULL) usage();
				smooth = atof(na);
				if (smooth < 0.0) usage();

			/* Underlying smoothnes factor */
			} else if (argv[fa][1] == 'g') {
				fa = nfa;
				if (na == NULL) usage();
				gsmooth = atof(na);
				if (gsmooth < 0.0) usage();

			/* Output channel */
			} else if (argv[fa][1] == 'X') {
				fa = nfa;
				if (na == NULL) usage();
				ix = atoi(na);
				if (ix < 0 || ix > 3) usage();

			} else 
				usage();
		} else
			break;
	}

	if (fa >= argc || argv[fa][0] == '-') usage();
	strcpy(prof_name,argv[fa]);

	rco.fname = prof_name;

	if ((rco.mppo = new_mpp()) == NULL)
		error ("Creation of MPP object failed");

	if ((rv = rco.mppo->read_mpp(rco.mppo,prof_name)) != 0)
		error ("%d, %s",rv,rco.mppo->err);

	rco.mppo->get_info(rco.mppo, &rco.imask, &rco.pdi, NULL, NULL, NULL, NULL, NULL, NULL);
	ident = icx_inkmask2char(rco.imask, 1); 

	if (rco.pdi != 3 && rco.pdi != 4)
		error("Expect RGB or CMYK .mpp");

	if (verb) {
		printf("MPP profile with %d colorants, type %s\n",rco.pdi,ident);
	}

	/* Select Lab return value details */
	if ((rv = rco.mppo->set_ilob(rco.mppo, icxIT_default, NULL, icxOT_default, NULL, icSigLabData, 0)) != 0) {
		if (rv == 1)
			error("Spectral profile needed for custom illuminant, observer or FWA");
		error("Error setting illuminant, observer, or FWA");
	}

	if (series > 0) {
		if (series == 1)
			do_series_1(verb, plot, &rco, di, unif, ntps, nlev);
		else if (series == 2)
			do_series_2(verb, plot, &rco, di, unif);
		else
			error("Unknown series %d\n",series);
		return 0;
	}

	if (res < 0) {
		if (di == 1)
			res = 129;
		else if (di == 2)
			res = 65;
		else if (di == 3)
			res = 33;
		else 
			res = 17;
	}

	if (ntps < 0) {
		if (di == 1)
			ntps = 20;
		else if (di == 2)
			ntps = 40;
		else if (di == 3)
			ntps = 60;
		else 
			ntps = 80;
	}

	if (smfunc) {
		double sm;

		if (verb) {
			printf("Dimensions %d\n",di);
			printf("Tests %d\n",its);
			printf("Grid resolution %d\n",res);
		}

		sm = do_stest(&rco, verb, di, its, res);

		printf("Results: smoothness factor = %f\n",sm);

	} else {

		if (verb) {
			printf("Profile is '%s'\n",rco.fname);
			printf("Dimensions %d\n",di);
			printf("Outpu chan %d\n",ix);
			printf("RSPL resolution %d\n",res);
			printf("No. Sample points %d (norm %f)\n",ntps, pow((double)ntps, 1.0/di));
			printf("Noise volume total %f, == avg. dev. %f\n",noise, 0.25 * noise);
			if (gsmooth > 0.0)
				printf("Underlying smooth %f\n",gsmooth);
			else
				printf("Extra smooth %f\n",smooth);
		}

		ix = 0;

		if (set_refconv(&rco, ix, di)) {
			error("set_refconv returned nz with ix %f\n",ix);
		}
	
		if (gsmooth > 0.0)
			do_test(&rco, &trmse, &tmaxe, &tavge, verb, plot, di, res, ntps, noise, unif, -gsmooth);
		else
			do_test(&rco, &trmse, &tmaxe, &tavge, verb, plot, di, res, ntps, noise, unif, smooth);

		printf("Results: maxerr %f, avgerr %f, rmserr %f\n",
		       tmaxe, tavge, trmse);
	}

	rco.mppo->del(rco.mppo);

	free(ident);

	return 0;
}

/* ----------------------------------------------- */

/* Do one set of tests and return the results */
static void do_test(
	refconv *rco,
	double *trmse,		/* RETURN total RMS error */
	double *tmaxe,		/* RETURN total maximum error */
	double *tavge,		/* RETURN total average error */
	int verb,			/* Verbosity */
	int plot,			/* Plot graphs */
	int di,				/* Dimensions in */
	int res,			/* RSPL grid resolution */
	int ntps,			/* Number of sample points */
	double noise,		/* Sample point noise volume */
	int unif,			/* nz for uniform noise, else normal */
	double smooth		/* Smoothness to test, +ve for extra, -ve for underlying */
) {
	sobol *so;			/* Sobol sequence generator */
	co *tps = NULL;
	rspl *rss;	/* Multi-resolution regularized spline structure */
	datai low,high;
	int gres[MXDI];
	double avgdev[MXDO];
	int i, j, it;
	int flags = RSPL_NOFLAGS;

	*trmse = 0.0;
	*tmaxe = 0.0;
	*tavge = 0.0;

	for (j = 0; j < di; j++) {
		low[j] = 0.0;
		high[j] = 1.0;
		gres[j] = res;
	}
	
	/* Make repeatable by setting random seed before a test set. */
	rand32(0x12345678);

	if ((so = new_sobol(di)) == NULL)
		error("Creating sobol sequence generator failed");

	{
		double rmse, avge, maxe;

		/* Create the object */
		rss = new_rspl(RSPL_NOFLAGS,di, 3);

		/* Create the list of sampling points */
		tps = (co *)malloc(ntps * sizeof(co));

		so->reset(so);

		if (verb) printf("Generating the sample points\n");

		/* Random sobol test set */
		for (i = 0; i < ntps; i++) {
			double out[3];
			int f;

			so->next(so, tps[i].p);
			rco->lookup(rco, out, tps[i].p);

			/* Add randomness to the PCS values */
			/* 0.25 * converts total volume to average deviation */
			for (f = 0; f < 3; f++) {
				if (unif) {
					tps[i].v[f] = out[f] + 100.0 * d_rand(-0.5 * noise, 0.5 * noise);
				} else {
					tps[i].v[f] = out[f] + 100.0 * noise * 0.25 * 1.2533 * norm_rand();
				}
			}
//printf("~1 data %d: %f %f %f -> %f %f %f, inc noise %f %f %f\n", i, tps[i].p[0], tps[i].p[1], tps[i].p[2], out[0], out[1], out[2], tps[i].v[0], tps[i].v[1], tps[i].v[2]);
		}

		/* Average deviation of ouput % */
		avgdev[0] = 0.25 * noise;
		avgdev[1] = 0.25 * noise;
		avgdev[2] = 0.25 * noise;

		/* Fit to scattered data */
		if (verb) printf("Fitting the scattered data, smooth = %f, avgdev = %f\n",smooth,avgdev[0]);
		rss->fit_rspl(rss,
		           flags,				/* Non-mon and clip flags */
		           tps,					/* Test points */
		           ntps,				/* Number of test points */
		           low, high, gres,		/* Low, high, resolution of grid */
		           NULL, NULL,			/* Default data scale */
		           smooth,				/* Smoothing */
		           avgdev,				/* Average deviation */
		           NULL);				/* iwidth */

		/* Plot out function values */
		if (plot) {
			int slice;
			printf("L*: Black is target, Red is rspl\n");
			printf("a*: Green is target, Blue is rspl\n");
			printf("b*: Yellow is target, Purple is rspl\n");
			for (slice = 0; slice < (di+1); slice++) {
				co tp;	/* Test point */
				double x[PLOTRES];
				double yy[6][PLOTRES];
				double pp[MXDI], p1[MXDI], p2[MXDI], ss[MXDI];
				int n = PLOTRES;

				/* setup slices on each axis at 0.5 and diagonal */
				if (slice < di) {
					for (j = 0; j < di; j++)
						p1[j] = p2[j] = 0.5;
					p1[slice] = 0.0;
					p2[slice] = 1.0;
					printf("Slice along axis %d\n",slice);
				} else {
					for (j = 0; j < di; j++) {
						p1[j] = 0.0;
						p2[j] = 1.0;
					}
					printf("Slice along diagonal\n");
				}

				for (j = 0; j < di; j++) {
					ss[j] = (p2[j] - p1[j])/n;
					pp[j] = p1[j];
				}
				
				for (i = 0; i < n; i++) {
					double out[3];
					double vv = i/(n-1.0);
					x[i] = vv;

					/* Reference */
					rco->lookup(rco, out, pp);
					yy[0][i] = out[0];
					yy[2][i] = out[1];
					yy[4][i] = out[2];

					/* RSPL aproximation */
					for (j = 0; j < di; j++)
						tp.p[j] = pp[j];

					if (rss->interp(rss, &tp))
						tp.v[0] = -0.1;
					yy[1][i] = tp.v[0];
					yy[3][i] = tp.v[1];
					yy[5][i] = tp.v[2];

					/* Increment point */
					for (j = 0; j < di; j++)
						pp[j] += ss[j];
				}

				/* Plot the result */
				do_plot6(x,yy[0],yy[1],yy[2],yy[3],yy[4],yy[5],n);
//				do_plot(x,yy[0],yy[1],NULL,n);
//				do_plot(x,yy[2],yy[3],NULL,n);
//				do_plot(x,yy[4],yy[5],NULL,n);
			}
		}

		/* Compute statistics */
		rmse = 0.0;
		avge = 0.0;
		maxe = 0.0;
//		so->reset(so);


		/* Check fit to scattered data */
		if (verb) printf("Fitting the scattered data\n");
		for (i = 0; i < 100000; i++) {
//		for (i = 0; i < 100; i++) {
			double out[3];
			co tp;	/* Test point */
			double err;

			if (so->next(so, tp.p))	
				error("Ran out of pseudo radom points");

			/* Reference */
			rco->lookup(rco, out, tp.p);

			/* RSPL aproximation */
			rss->interp(rss, &tp);

			err = icmLabDE(out, tp.v);

//printf("~1 %f: point %f %f %f -> ref %f %f %f, test %f %f %f\n", err, tp.p[0], tp.p[1], tp.p[2], out[0], out[1], out[2], tp.v[0], tp.v[1], tp.v[2]);

			avge += err;
			rmse += err * err;
			if (err > maxe)
				maxe = err;
		}
		avge /= (double)i;
		rmse /= (double)i;

		if (verb)
			printf("Dim %d, res %d, noise %f, points %d, maxerr %f, rmserr %f, avgerr %f\n",
		       di, res, noise, ntps, maxe, sqrt(rmse), avge);

		*trmse += rmse;
		if (maxe > *tmaxe)
			*tmaxe = maxe;
		*tavge += avge;

		rss->del(rss);
		free(tps);
	}
	so->del(so);

	*trmse = sqrt(*trmse);
}

/* Do smoothness scaling check & return results */
static double do_stest(
	refconv *rco,
	int verb,			/* Verbosity */
	int di,				/* Dimensions */
	int its,			/* Number of function tests */
	int res				/* RSPL grid resolution */
) {
	DCOUNT(gc, MXDIDO, di, 1, 1, res-1);
	int it;
	double atse = 0.0;

	/* Make repeatable by setting random seed before a test set. */
	rand32(0x12345678);

	for (it = 0; it < its; it++) {
		double tse;
		int fdi = it % 3;		/* Rotate amongsth L, a, b */

		DC_INIT(gc)
		tse = 0.0;
		for (; !DC_DONE(gc);) {
			double out[3];
			double g[MXDI];
			int e, k;
			double y1, y2, y3;
			double del;

			for (e = 0; e < di; e++)
				g[e] = gc[e]/(res-1.0);
			rco->lookup(rco, out, g);
			y2 = 0.01 * out[fdi];

			del = 1.0/(res-1.0);

			for (k = 0 ; k < di; k++) {
				double err;

				g[k] -= del;
				rco->lookup(rco, out, g);
				y1 = 0.01 * out[fdi];
				g[k] += 2.0 * del;
				rco->lookup(rco, out, g);
				y3 = 0.01 * out[fdi];
				g[k] -= del;

				err = 0.5 * (y3 + y1) - y2;
				tse += err * err;
			}

			DC_INC(gc);
		}
		/* Apply adjustments and corrections */
		tse *= pow((res-1.0), 4.0);					/* Aprox. geometric resolution factor */
		tse /= pow((res-2.0),(double)di);			/* Average squared non-smoothness */

		if (verb)
			printf("smf for it %d = %f\n",it,tse);
		atse += tse;
	}

	return atse/(double)its;
}


