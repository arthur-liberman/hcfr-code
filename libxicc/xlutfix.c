/* 
 * International Color Consortium color transform expanded support
 * Set Lut table values and do auxiliary chanel interpolation continuity fixups.
 *
 * Author:  Graeme W. Gill
 * Date:    17/12/00
 * Version: 1.00
 *
 * Copyright 2000 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 */

/*
 * This module provides additional xicc functionality
 * for CMYK lut based profiles.
 *
 * This is essentially a test of one approach to fixing
 * auxiliary parameter induced interpolation errors.
 */

/*
 * TTBD:
 *
 *       Remove this code when the optimised separation code is working.
 *
 *       Some of the error handling is crude. Shouldn't use
 *       error(), should return status.
 *
 */

/* Description:
	
		In all the clut based color systems, there are various
	stages where the multi-dimenional profile functions are
	resampled from one respresentation to another. As in all
	sampling, aliasing may be an issue. The standard
	methods for dealing with aliasing involve band limiting and
	over-sampling. In dealing with color, anything other than
	point sampling is often too slow to consider, meaning that
	over sampling, or on-the-fly filtering is impractical.
	
	Band limiting the function to be sampled is therefore the
	most practical approach, but there are still sever tradoffs.
	For accurately representing the sampled characteristics
	of a device, a high resolution grid, with band limited
	sample points is desirable. 3 or 4 dimension grids however,
	quickly consume memory, and generaly show an exponential
	decline in access and manipulation speed with grid resolution.
	To maintain accuracy therefore, the minimum grid resolution,
	and the minimum level of filtering is often employed.
	
	The routines in this file are to deal with an aditional
	subtlety when dealing with devices that have extra
	degrees of freedom (ie. CMYK devices). In theory, the
	aditional degrees of freedom can be set abitrarily, and
	are often chosen to follow a "rule", designed to acheive
	a goal such as minimising the amount of black used
	in the highlights of bitonal devices (to minimise
	"black dot" visibility), or to maximise black usage
	for minimum ink costs, to resduce grey axis sensitivity
	to the CMY values, to reduce the total ink loading,
	or to pass through the inking values of a similar
	input colorspace. In Argyll, these extra degrees of 
	freedom are refered to as auxiliary chanels.
	
	Because the final representation of the color correction
	transform is often a multi-dimensional interpolation lookup
	table (clut), there is an aditional hidden requirement for
	any auxiliary input chanels, and that is that there be
	a reasonable degree of interpolation continuity between
	the sampled grid points. If this continuity requirement
	is not met, then the accuracy of the interpolation within
	each grid cell can be wildly inacurate, even though the
	accuracy of the grid points themselves is good.
	
	For instance, if we have two grid points of a Lab->CMYK
	interpolation grid:
	
	1) 	50 0 0   -> .0 .0 .0 .3 -> 50 0 0
	2)  50 0 10  -> .2 .2 .4 .0 -> 50 0 10
	
	Now if an input PCS value of 50 0 5 is used to
	lookup the device values that should be used, a typical
	interpolation might return:
	
	   50 0 5 ->    .1 .1 .2 .15 ->  40 -5 6
	
	This is a small change in PCS space, but bevcause the
	two device points are at opposite extremes of the possible
	auxliary locus for each point, the device values are
	far appart in device space. The accuracy of the
	device space interpolation is therefore not guaranteed
	to be accurate, and might in this case, mean that
	the device actually reproduces an unexpectedly inaccurate PCS value.
	Even worse, at the gamut boundaries the locus shrinks to zero,
	and particularly in the dark end of the gamut, there
	may be a multitude of different Device values that overlap
	at the gamut boundary, causing abrupt or even chaotic
	device values at spacings well above the sampling spacing
	of the interpolation grid being created.

	An additional challenge is that the locus of valid auxiliary
	values may be discontinuous, (typically bifurcated), particularly
	when an ink limit is imposed - the limit often removing a segment
	of the auxiliary locus from the gamut. So ideally, a contiguous
	auxiliary region needs to be mapped out, and any holes
    patched over or removed from the gamut in a way that
	doesn't introduce discontinuities. 
	
	In Argyll, we want to maintain the freedom to set arbitrary
	auxiliary rules, yet we need to avoid the gross loss of
	accuracy abrupt transitions in auxiliary values can cause.
	
	The approach I've taken here involves a number of steps. The
	first step sets up the clut in the usual maner, but records
	various internal values for each point. In the second step, these
	grid values are examined to locate cells which are "at risk" of
	auxiliary interpolation errors. In the third step, the grid points
	around the "at risk" cells have their auxiliary target values
	adjusted to new target values, by using a simple smoothing filter
	to reduce abrupt transitions. In the fourth step, new device values
	are searched for, that have the same target PCS for the grid point,
	but the smoothed auxiliary value. In cases where there is no scope
	for meeting the new auxiliary target, because it is already at one
	extreme of its possible locus for the target PCS, a tradoff is then
	made between reduced target PCS accuracy, and an improved auxiliary
	accuracy. In the final step, the new grid values replace the old
	in the ICC.
	
*/

#include "icc.h"
#include "numlib.h"
#include "xicc.h"

/* NOTE:- that we only implement support for CMYK output here !!! */

#define CHECK_FUNCS		/* Sanity check the callback functions */
#define DO_STATS
#undef SAVE_TRACE		/* Save the values returned by the clut callback function */
#undef  USE_TRACE		/* Use the trace file instead of the clut callback function */

#define TRACENAME		"D:/usr/argyll/xicc/xlutfix.xxx"	/* So it will work in the debugger */

#define MAX_PASSES 7
#define MAX_FILTERS 20
#define THRESH 0.55		/* Fix Threshold, ratio of mean to maximum PCS point */
#define MINTHRESH 2.0	/* Set minimum interp error threshold. Don't fix if below this */
#define AUXWHT 3.0		/* Auxiliary tradeoff weight and increment */
#undef WIDEFILTER		/* Alter 4x4 neighborhood */

/* ========================================================== */

/* Return maximum difference */
static double maxdiffn(int n, double *in1, double *in2) {
	double tt, rv = 0.0;
	int i;
	for (i = 0; i < n; i++) {
		if ((tt = fabs(in1[i] - in2[i])) > rv)
			rv = tt;
	}
	return rv;
}

/* Return absolute difference */
static double absdiffn(int n, double *in1, double *in2) {
	double tt, rv = 0.0;
	int i;
	for (i = 0; i < n; i++) {
		tt = in1[i] - in2[i];
		rv += tt * tt;
	}
	return sqrt(rv);
}

/* ========================================================== */
/* Callback functions used by icc set_tables                  */
/* ========================================================== */

/* Context for set_tables callbacks */

typedef struct {
	int dofix;
	void *cbctx;
	void (*infunc)(void *cbctx, double *out, double *in);
	void (*clutfunc)(void *cbctx, double *out, double *aux, double *auxr, double *pcs, double *in);
	void (*clutpcsfunc)(void *cbctx, double *out, double *auxv, double *pcs);
	void (*clutipcsfunc)(void *cbctx, double *pcs, double *olimit, double *auxv, double *in);
	void (*outfunc)(void *cbctx, double *out, double *in);

	float *g;				/* Base of grid */
	int res;				/* Grid resolution */
	int fn;					/* Number of floats in grid */
	int n;					/* Number of entries in grid */
	int fesz;				/* Entry size in floats */
	int fci[MXDI];			/* float increment for each input dimension into latice */
	int cmin[MXDI];			/* Fixup area bounding box minimum */
	int cmax[MXDI];			/* Fixup area bounding box maximum +1 */
							/* One float for flags */
	int din;				/* Number of input (ie. grid) dimensions */
	int daux;				/* Number of auxiliary dimensions */
	int dout;				/* Number of output dimensions */
	int oauxr;				/* Offset to start of aux range entries */
	int oauxv;				/* Offset to start of aux value entries */
	int oauxvv;				/* Offset to start of aux new value entries */
	int opcs;				/* Offset to start of PCS value entries */
	int oout;				/* Offset to start of output value entries */
	int nhi;				/* Number of corners in an input grid cube */
	int *fhi;				/* nhi grid cube corner offsets in floats */

	/* Minimiser info */
	double m_auxw;			/* Auxiliary error weighting factor (ie. 5 - 100) */
	double m_auxv[MAX_CHAN];/* Auxiliary target value */
	double m_pcs[3];		/* PCS target value */

#if defined(SAVE_TRACE) || defined(USE_TRACE)
	FILE *tf;
#endif
} xifs;

/* Macros to access flag values */
#define XLF_FLAGV(fp)  (*((unsigned int *)(fp)))
#define XLF_TOFIX  0x0001		/* Grid point to be fixed flag */
#define XLF_UPDATE  0x0002		/* Grid point to be updated flag */
#define XLF_HARDER  0x0004		/* Compromise PCS to improve result */

/* Functions to pass to icc settables() to setup icc Lut */
/* Input table. input -> input' space. */
static void xif_set_input(void *cntx, double *out, double *in) {
	xifs *p = (xifs *)cntx;

	p->infunc(p->cbctx, out, in);
}


/* clut, input' -> output' space */
static void xif_set_clut(void *cntx, double *out, double *in) {
	xifs *p = (xifs *)cntx;

	if (p->dofix == 0) {			/* No fixups */
		p->clutfunc(p->cbctx, out, NULL, NULL, NULL, in);

	} else if (p->dofix == 1) {		/* First pass */
		int e, f;
		float *fp, *ep;
		double pcs[MAX_CHAN], auxv[MAX_CHAN],  auxr[MAX_CHAN * 2];

		/* the icclib set_tables() supplies us the grid indexes */
		/* as integer in the double locations at in[-e-1] */

#if defined(USE_TRACE)
		if (fread(pcs,  sizeof(double), 3, p->tf) != 3
		 || fread(auxr, sizeof(double), 2 * p->daux, p->tf) != (2 * p->daux)
		 || fread(auxv, sizeof(double), p->daux, p->tf) != p->daux
		 || fread(out,  sizeof(double), p->dout, p->tf) != p->dout) {
			fprintf(stderr,"mark_cells: read of trace failed\n");
			exit(-1);
		}
#else	/* !USE_TRACE */
		p->clutfunc(p->cbctx, out, auxv, auxr, pcs, in);

#if defined(SAVE_TRACE)
		if (fwrite(pcs,  sizeof(double), 3, p->tf) != 3
		 || fwrite(auxr, sizeof(double), 2 * p->daux, p->tf) != (2 * p->daux)
		 || fwrite(auxv, sizeof(double), p->daux, p->tf) != p->daux
		 || fwrite(out,  sizeof(double), p->dout, p->tf) != p->dout) {
			fprintf(stderr,"mark_cells: write of trace failed\n");
			exit(-1);
		}
#endif	/* SAVE_TRACE */
#endif	/* !USE_TRACE */

		/* Figure grid pointer to grid entry */
		for (fp = p->g, e = 0; e < p->din; e++)
			fp += *((int *)&in[-e-1]) * p->fci[e];

		XLF_FLAGV(fp) = 0;		/* Clear flags */

		ep = fp + p->opcs;
		for (f = 0; f < 3; f++)					/* Save PCS values */
			ep[f] = (float)pcs[f];

		ep = fp + p->oauxr;
		for (f = 0; f < (2 * p->daux); f++)		/* Save auxiliary range values */
			ep[f] = (float)auxr[f];

		ep = fp + p->oauxv;
		for (f = 0; f < p->daux; f++)			/* Save auxiliary values */
			ep[f] = (float)auxv[f];

		ep = fp + p->oout;
		for (f = 0; f < p->dout; f++)			/* Save the output values */
			ep[f] = (float)out[f];

	} else {		/* Second pass */
		int e, f;
		float *fp, *ep;

		/* Figure grid pointer to grid entry */
		for (fp = p->g, e = 0; e < p->din; e++)
			fp += *((int *)&in[-e-1]) * p->fci[e];

		ep = fp + p->oout;
		for (f = 0; f < p->dout; f++)		/* Return the fixed output values */
			out[f] = (double)ep[f];
	}
}

/* output output' -> output space */
static void xif_set_output(void *cntx, double *out, double *in) {
	xifs *p = (xifs *)cntx;

	p->outfunc(p->cbctx, out, in);
}

static int mark_cells(xifs *p);
static int filter_grid(xifs *p, int tharder);
static void fix_grid(xifs *p, double auxw);
static int comp_pcs(xifs *p, double auxw, double *auxrv, double *auxv, double *pcs, double *dev);

/* Helper function to setup the three tables contents, and the underlying icc. */
/* Only useful if there are auxiliary device output chanels to be set, */
/* as this takes care of auxiliary interpolation continuity fixups. */
int icxLut_set_tables_auxfix(
icmLut *p,								/* Pointer to icmLut object */
void   *cbctx,							/* Opaque callback context pointer value */
icColorSpaceSignature insig, 			/* Input color space */
icColorSpaceSignature outsig, 			/* Output color space */
void (*infunc)(void *cbctx, double *out, double *in),
						/* Input transfer function, inspace->inspace' (NULL = default) */
double *inmin, double *inmax,			/* Maximum range of inspace' values */
										/* (NULL = default) */
void (*clutfunc)(void *cbctx, double *out, double *aux, double *auxr, double *pcs, double *in),
						/* inspace' -> outspace' transfer function, also */
						/* return the target PCS and the  (packed) auxiliary locus range, */
						/* as [min0, max0, min1, max1...], the auxiliary chosen. */
void (*clutpcsfunc)(void *cbctx, double *out, double *aux, double *pcs),
						/* PCS + aux_target -> outspace' transfer function */
void (*clutipcsfunc)(void *cbctx, double *pcs, double *olimit, double *auxv, double *in),
						/* outspace' -> PCS + auxv check function */
double *clutmin, double *clutmax,		/* Maximum range of outspace' values */
										/* (NULL = default) */
void (*outfunc)(void *cbctx, double *out, double *in)
						/* Output transfer function, outspace'->outspace (NULL = deflt) */
) {
	int rv, g, e, jj, kk;
	double auxw;	/* Auxiliary weight factor */
	xifs xcs;		/* Our context structure */

	/* Simply pass this on to the icc set_table() */
	xcs.dofix        = 0;			/* Assume we won't attempt fix */
	xcs.cbctx        = cbctx;
	xcs.infunc       = infunc;
	xcs.clutfunc     = clutfunc;
	xcs.clutpcsfunc  = clutpcsfunc;
	xcs.clutipcsfunc = clutipcsfunc;
	xcs.outfunc      = outfunc;

	if (outsig != icSigCmykData) {	/* Don'y know how/if to fix this */
		rv = p->set_tables(p,
			ICM_CLUT_SET_APXLS,
			(void *)&xcs,
			insig, outsig,
			xif_set_input,
			inmin, inmax, 
			xif_set_clut,
			clutmin, clutmax,
			xif_set_output,
			NULL, NULL);

		return rv;
	}

#ifdef CHECK_FUNCS
	if (insig == icSigLabData) {
		double in[3], out[MAX_CHAN];
		double aux[1], auxr[2], pcs[3];
		double out_check[MAX_CHAN];
		double apcs[3], pcs_check[3];
		
		/* Pick a sample input value */
		in[0] = 50.0; in[1] = 0.0; in[2] = 0.0;

		/* Test the in->out function */
		clutfunc(cbctx, out, aux, auxr, pcs, in);

printf("~1 %f %f %f -> pcs %f %f %f,\n   auxr %f - %f, auxv %f, dev %f %f %f %f\n",
       in[0], in[1], in[2], pcs[0], pcs[1], pcs[2], auxr[0], auxr[1], aux[0],
       out[0], out[1], out[2], out[3]);

		/* Check that we get the same result for the pcs function */
		clutpcsfunc(cbctx, out_check, aux, pcs);

		if (maxdiffn(p->outputChan, out, out_check) > 1e-6) {
			fprintf(stderr,"set_tables_auxfix: pcsfunc check failed\n");
printf("~1 is %f %f %f %f, should be %f %f %f %f\n",
out_check[0], out_check[1], out_check[2], out_check[3],
out[0], out[1], out[2], out[3]);
		}
printf("~1 PCS version gives %f %f %f %f\n",
out_check[0], out_check[1], out_check[2], out_check[3]);

		/* Checkout the reverse function */
		clutipcsfunc(cbctx, apcs, NULL, NULL, out);			/* Device -> clipped PCS */

printf("~1 clipped PCS = %f %f %f\n", apcs[0], apcs[1], apcs[2]);
		clutpcsfunc(cbctx, out_check, aux, apcs);		/* clipped PCS -> Device */
		clutipcsfunc(cbctx, pcs_check, NULL, NULL, out_check);		/* Device -> PCS */
printf("~1 check PCS = %f %f %f\n", pcs_check[0], pcs_check[1], pcs_check[2]);

		if (maxdiffn(3, apcs, pcs_check) > 1e-5) {
			fprintf(stderr,"set_tables_auxfix: ipcsfunc check failed\n");
			printf("~1 is %f %f %f, should be %f %f %f\n",
			pcs_check[0], pcs_check[1], pcs_check[2],
			pcs[0], pcs[1], pcs[2]);
		}

	} else {
		fprintf(stderr,"Sanity check of %s not implemented!\n",
		        icm2str(icmColorSpaceSignature,insig));
	}
#endif /* CHECK_FUNCS */

	/* Allocate an array to hold all the results */
	xcs.res  = p->clutPoints;
	xcs.din  = p->inputChan;
	xcs.dout = p->outputChan;
	xcs.daux = xcs.dout - 3;					/* Number of auxiliary values */
	xcs.fesz = 1 + 3 + xcs.dout + 4 * xcs.daux;	/* Entry size in floats */

	/* Compute total number of entries, and offsets in each dimension */
	xcs.n = xcs.res;
	xcs.fci[0] = xcs.fesz;
	for (e = 1; e < xcs.din; e++) {
		xcs.n       *= xcs.res;
		xcs.fci[e] = xcs.fci[e-1] * xcs.res;
	}
	xcs.fn = xcs.n * xcs.fesz;

printf("~1 fci = %d %d %d\n",
xcs.fci[0], xcs.fci[1], xcs.fci[2]);

	/* Setup offset list to grid cube corners */
	xcs.nhi = 1 << xcs.din;
	if ((xcs.fhi = (int *)malloc(sizeof(int) * xcs.nhi)) == NULL) {
		sprintf(p->icp->err,"icxLut_set_tables: malloc() failed");
		return p->icp->errc = 2;
	}
	for (g = 0; g < xcs.nhi; g++) {
		xcs.fhi[g] = 0;
		for (e = 0; e < xcs.din; e++) {
			if (g & (1 << e)) 
				xcs.fhi[g] += xcs.fci[e];
		}
			
	}
printf("~1 nhi = %dd\n",xcs.nhi);
	
	/* Offsets into each entry */
	xcs.opcs = 1;							/* Allow 1 flag float */
	xcs.oout = xcs.opcs + 3;				/* dpcs floats */
	xcs.oauxr = xcs.oout + xcs.dout;		/* dout floats */
	xcs.oauxv = xcs.oauxr + xcs.daux * 2;	/* 2 daux floats */
	xcs.oauxvv = xcs.oauxv + xcs.daux;		/* daux floats */
											/* daux floats */

printf("~1 res %d, entry size = %d floats, total floats needed = %d\n",xcs.res,xcs.fesz,xcs.fn);

printf("~1 opcs = %d, oout = %d, oauxr = %d, oauxv = %d\n",
	xcs.opcs, xcs.oout, xcs.oauxr, xcs.oauxv);

	/* Allocate the grid */
	if ((xcs.g = (float *)malloc(sizeof(float) * xcs.fn)) == NULL) {
		sprintf(p->icp->err,"icxLut_set_tables: malloc() failed");
		return p->icp->errc = 2;
	}

#if defined(SAVE_TRACE) || defined(USE_TRACE)
	{
		char *tname = TRACENAME;

#if defined(SAVE_TRACE)
		if ((xcs.tf = fopen(tname,"w")) == NULL) {
#else
		if ((xcs.tf = fopen(tname,"r")) == NULL) {
#endif
			fprintf(stderr,"mark_cells: Can't open file '%s'\n",tname);
			exit(-1);
		}
#if defined(O_BINARY)
#if defined(SAVE_TRACE)
		xcs.tf = freopen(tname,"wb",xcs.tf);
#else
		xcs.tf = freopen(tname,"rb",xcs.tf);
#endif
#endif
	}
#endif	/* SAVE_TRACE || USE_TRACE */

#ifdef NEVER
// ~9 	check function
{
	int rv;
	double auxv[1], rauxv[1];
	double pcs[3], rpcs[3];
	double dev[4];

	auxv[0] = 0.5;
	pcs[0] = 60.0;
	pcs[1] = 0.0;
	pcs[2] = 0.0;

	dev[0] = 0.5;
	dev[1] = 0.1;
	dev[2] = 0.1;
	dev[3] = 0.1;

	rv = comp_pcs(&xcs, 20.0, NULL, auxv, pcs, dev);

	printf("~9 comp_pcs returned %d, device %f %f %f %f\n",rv, dev[0], dev[1], dev[2], dev[3]);

	xcs.clutipcsfunc(xcs.cbctx, rpcs, NULL, rauxv, dev);

	printf("~9 comp_pcs pcs %f %f %f, aux %f\n", rpcs[0], rpcs[1], rpcs[2], rauxv[0]);

	return 0;
}
#endif

printf("~1 doing the first pass\n");
	/* First pass */
	xcs.dofix = 1;
	rv = p->set_tables(p,
		ICM_CLUT_SET_APXLS,
		(void *)&xcs,
		insig, outsig,
		xif_set_input,
		inmin, inmax, 
		xif_set_clut,
		clutmin, clutmax,
		xif_set_output,
		NULL, NULL);

#if defined(SAVE_TRACE) || defined(USE_TRACE)
	fclose(xcs.tf);
#endif	/* SAVE_TRACE || USE_TRACE */

	if (rv != 0) {
		free(xcs.fhi);
		free(xcs.g);
		return rv;
	}

printf("~1 doing the fixups\n");

	/* Try three passes */
	for(jj = 0, auxw = AUXWHT; jj < MAX_PASSES; jj++, auxw += AUXWHT) {
		int lrv;

		/* Figure out which cells need fixing */
		rv = mark_cells(&xcs);
printf("~1 cells that need fixing = %d\n", rv);

		if (rv == 0)
			break;

		/* Filter the grid values that need fixing */
printf("~1 about to filter grid points\n");
		for (kk = 0, lrv = 0, rv = 1; kk < MAX_FILTERS && rv > 0 && rv != lrv; kk++) {
			lrv = rv;
			rv = filter_grid(&xcs, 1);
		}

		if (rv == 0)
			break;

		/* Lookup device values for grid points with changed auxiliary targets */
printf("~1 about to fix grid points\n");
		fix_grid(&xcs, auxw);
	};

rv = mark_cells(&xcs);
printf("~1 faulty cells remaining = %d\n", rv);

printf("~1 updatding the icc\n");
	/* Second pass */
	xcs.dofix = 2;
	rv = p->set_tables(p,
		ICM_CLUT_SET_APXLS,
		(void *)&xcs,
		insig, outsig,
		xif_set_input,
		inmin, inmax, 
		xif_set_clut,
		clutmin, clutmax,
		xif_set_output,
		NULL, NULL);

	free(xcs.fhi);
	free(xcs.g);

printf("~1 done\n");

	return rv;
}
		



/* ----------------------------------------- */
/* Mark cells that need fixing */
/* Return number of cells that need fixing */
static int mark_cells(xifs *p) {
	int e, f;
	int coa[MAX_CHAN], ce;	/* grid counter */
	int tcount = 0;
#ifdef DO_STATS
	double aerr = 0.0;	
	double merr = 0.0;
	double ccount = 0.0;
#endif

	/* Get ready to track fixup area bounding box */
	for (e = 0; e < p->din; e++) {
		p->cmin[e] = 99999;
		p->cmax[e] = -1;
	}

	/* Init the grid counter */
	for (ce = 0; ce < p->din; ce++)
		coa[ce] = 0;
	ce = 0;

	/* Itterate throught the PCS clut grid cells */
	while (ce < p->din) {
		int j, m;
		float *gp;				/* Grid pointer */
		float *ep, *fp;			/* Temporary grid pointers */
		double wpcsd;			/* Worst case PCS distance */
		double apcs[3];			/* Average PCS value */
		double aout[MAX_CHAN];	/* Average output value */
		double check[3];		/* Check PCS */
		double ier;				/* Interpolation error */
		int markcell = 0;		/* Mark the cell */

		/* Compute base of cell pointer */
		gp = p->g;		/* Grid pointer */
		for (e = 0; e < p->din; e++)
			gp += coa[e] * p->fci[e];
			
		/* - - - - - - - - - - - - - - - - - */
		/* Full surrounding Cell calculation */

		/* Init averaging accumulators */
		for (j = 0; j < 3; j++)
			apcs[j] = 0.0;
		for (f = 0; f < p->dout; f++)	
			aout[f] = 0.0;

		/* For each corner of the PCS grid based at the current point, */
		/* average the PCS and Device values */
		for (m = 0; m < p->nhi; m++) {
			double pcs[3];
			double dev[MAX_CHAN];

			fp = gp + p->fhi[m];

//ep = fp + p->opcs;
//printf("Input PCS %f %f %f\n", ep[0], ep[1], ep[2]);

			ep = fp + p->oout;			/* base of device values */
			for (f = 0; f < p->dout; f++) {
				double v = (double)ep[f];
				dev[f]   = v;
				aout[f] += v;
			}

			/* Device to clipped PCS */
			p->clutipcsfunc(p->cbctx, pcs, NULL, NULL, dev);

			for (j = 0; j < 3; j++)
				apcs[j] += pcs[j];

//printf("Corner PCS %f %f %f -> ", pcs[0], pcs[1], pcs[2]);
//printf("%f %f %f %f\n", dev[0], dev[1], dev[2], dev[3]);
		}

		for (j = 0; j < 3; j++)
			apcs[j] /= (double)p->nhi;

		for (f = 0; f < p->dout; f++)
			aout[f] /= (double)p->nhi;

		/* Compute worst case distance of PCS corners to average PCS */
		wpcsd = 0.0;
		for (m = 0; m < p->nhi; m++) {
			double ss;

			fp = gp + p->fhi[m] + p->opcs;
			for (ss = 0.0, j = 0; j < 3; j++) {
				double tt = (double)fp[j] - apcs[j];
				ss += tt * tt;
			}
			ss = sqrt(ss);
			if (ss > wpcsd)
				wpcsd = ss;
		}
		wpcsd *= THRESH;			/* Set threshold as proportion of most distant corner */
		if (wpcsd < MINTHRESH)	/* Set a minimum threshold */
			wpcsd = MINTHRESH;

//printf("Average PCS %f %f %f, Average Device %f %f %f %f\n",
//apcs[0], apcs[1], apcs[2], aout[0], aout[1], aout[2], aout[3]);

		/* Average Device to PCS */
		p->clutipcsfunc(p->cbctx, check, NULL, NULL, aout);

//printf("Check PCS %f %f %f\n",
//check[0], check[1], check[2]);

		/* Compute error in PCS vs. Device interpolation */
		ier = absdiffn(3, apcs, check);

//printf("Average PCS %f %f %f, Check PCS %f %f %f, error %f\n",
//apcs[0], apcs[1], apcs[2], check[0], check[1], check[2], ier);

#ifdef DO_STATS
		aerr += ier;
		ccount++;
		if (ier > merr)
			merr = ier;
#endif /* STATS */

		if (ier > wpcsd) {
			markcell = 1;
printf("~1 ier = %f, wpcsd = %f, Dev = %f %f %f %f\n", ier, wpcsd, aout[0], aout[1], aout[2], aout[3]);
		}

		/* - - - - - - - - - - - - - */
		/* Point pair calculations */

		if (markcell == 0) {	/* Don't bother testing if already marked */

			/* Get the base point values */
			ep = gp + p->oout;			/* base of device values (assumes fhi[0] == 0) */

			for (f = 0; f < p->dout; f++)
				aout[f] = (double)ep[f];

			p->clutipcsfunc(p->cbctx, apcs, NULL, NULL, aout);

			/* For each other corner of the PCS grid based at the */
			/* current point, compute the interpolation error */
			for (m = 1; m < p->nhi; m++) {
				double pcs[3];				/* Average PCS */
				double dpcs[3];				/* PCS of averaged device */
				double dev[MAX_CHAN];

				fp = gp + p->fhi[m];		/* Other point point */
				ep = fp + p->oout;			/* base of device values */

				for (f = 0; f < p->dout; f++)
					dev[f] = (double)ep[f];

				/* Device to clipped PCS */
				p->clutipcsfunc(p->cbctx, pcs, NULL, NULL, dev);

				for (j = 0; j < 3; j++)
					pcs[j] = 0.5 * (pcs[j] + apcs[j]);	/* PCS averaged value */

				for (f = 0; f < p->dout; f++)
					dev[f] = 0.5 * (aout[f] + dev[f]);	/* Average device */

				/* Average Device to PCS */
				p->clutipcsfunc(p->cbctx, dpcs, NULL, NULL, dev);

				wpcsd = THRESH * absdiffn(3, pcs, apcs);	/* Threshold value */
				if (wpcsd < MINTHRESH)	/* Set a minimum threshold */
					wpcsd = MINTHRESH;

				/* Compute error in PCS vs. Device interpolation */
				ier = absdiffn(3, pcs, dpcs);

#ifdef DO_STATS
				aerr += ier;
				ccount++;
				if (ier > merr)
					merr = ier;
#endif /* STATS */
				if (ier > wpcsd) {			/* Over threshold */
					markcell = 1;
printf("~1 ier = %f, wpcsd = %f, Dev = %f %f %f %f\n", ier, wpcsd, aout[0], aout[1], aout[2], aout[3]);
				}
			}
		}

		if (markcell) {

#ifndef WIDEFILTER
			/* Mark the whole cube around this base point */
			tcount++;
			/* Grid points that make up cell */
			for (m = 0; m < p->nhi; m++) {
				float *fp = gp + p->fhi[m];
				XLF_FLAGV(fp) |= XLF_TOFIX;
			}
			for (e = 0; e < p->din; e++) {
				if (coa[e] < p->cmin[e])
					p->cmin[e] = coa[e];
				if ((coa[e]+2) > p->cmax[e])
					p->cmax[e] = coa[e] + 2;
			}
#else
			int fo[MAX_CHAN], fe;	/* region counter */

			/* Mark the whole cube around this base point */
			tcount++;
			/* Grid points one row beyond cell */
			for (fe = 0; fe < p->din; fe++)
				fo[fe] = -1;		/* Init the neighborhood counter */
			fe = 0;

			/* For each corner of the filter region */
			while (fe < p->din) {
				float *fp = gp;

				for (e = 0; e < p->din; e++) {
					int oo = fo[e] + coa[e];
					if (oo < 0 || oo >= p->res)
						break;
					if (oo < p->cmin[e])
						p->cmin[e] = oo;
					if ((oo+1) > p->cmax[e])
						p->cmax[e] = oo + 1;
					fp += fo[e] * p->fci[e];
				}

				if (e >= p->din) {		/* Within the grid */
					XLF_FLAGV(fp) |= XLF_TOFIX;
				}
					
				/* Increment the counter */
				for (fe = 0; fe < p->din; fe++) {
					if (++fo[fe] < 3)
						break;			/* No carry */
					fo[fe] = -1;
				}
			}
#endif /* WIDEFILTER */
		}

		/* - - - - - - - - - - - - - - */


		/* Increment the main grid counter */
		for (ce = 0; ce < p->din; ce++) {
			if (++coa[ce] < (p->res-1))
				break;			/* No carry */
			coa[ce] = 0;
		}
	}

#ifdef DO_STATS
	aerr /= ccount;

	printf("~1Average interpolation error %f, maximum %f\n",aerr, merr);
	printf("~1Number outside corner radius = %f%%\n",(double)tcount * 100.0/ccount);
	printf("~1Bounding box is %d - %d, %d - %d, %d - %d\n",
		p->cmin[0], p->cmax[0], p->cmin[1], p->cmax[1], p->cmin[2], p->cmax[2]);
#endif /* STATS */

	return tcount; 
}


/* ----------------------------------------- */
/* Do one filter pass of grid aux values that need fixing */
/* If tharder is set, don't clamp new aux targets, but signal trading PCS for aux. */
/* Return the number of grid points that have a new aux target */
static int filter_grid(xifs *p, int tharder) {
	int e, f;
	int coa[MAX_CHAN], ce;	/* grid counter */
	int tcount = 0;

	/* Init the grid counter */
	for (ce = 0; ce < p->din; ce++)
		coa[ce] = p->cmin[ce];
	ce = 0;

	/* Itterate throught the PCS clut grid cells, */
	/* computing new auxiliary values */
	while (ce < p->din) {
		float *gp, *fp;			/* Grid pointer */
		int fo[MAX_CHAN], fe;	/* filter counter */
		double faux[MAX_CHAN];	/* Filtered auxiliary value */
		double nfv;				/* Number of values in filter value */

		/* Compute base of cell pointer */
		gp = p->g;		/* Grid pointer */
		for (e = 0; e < p->din; e++)
			gp += coa[e] * p->fci[e];
			
		if ((XLF_FLAGV(gp) & XLF_TOFIX) != 0) {

			for (f = 0; f < p->daux; f++)	
				faux[f] = 0.0;
			nfv = 0.0;

			/* Init the neighborhood counter */
			for (fe = 0; fe < p->din; fe++)
				fo[fe] = -1;
			fe = 0;

			/* For each corner of the filter region, */
			/* compute a filter kernel value */
			while (fe < p->din) {

				fp = gp + p->oauxv;
				for (e = 0; e < p->din; e++) {
					int oo = coa[e] + fo[e];
					if (oo < 0 || oo >= p->res)
						break;
					fp += fo[e] * p->fci[e];
				}

				if (e >= p->din) {		/* Add this neighborhood value */
					for (f = 0; f < p->daux; f++)	
						faux[f] += fp[f];
					nfv++;
				}
					
				/* Increment the counter */
				for (fe = 0; fe < p->din; fe++) {
					if (++fo[fe] < 2)
						break;			/* No carry */
					fo[fe] = -1;
				}
			}

			/* Compute average value, and save it */
			fp = gp + p->oauxvv;
			for (f = 0; f < p->daux; f++)
				fp[f] = (float)(faux[f] / nfv);

		}

		/* Increment the counter */
		for (ce = 0; ce < p->din; ce++) {
			if (++coa[ce] < p->cmax[ce])
				break;			/* No carry */
			coa[ce] = p->cmin[ce];
		}
	}

	/* Clip the new values to the valid range, and put them into place */

	/* Init the grid counter */
	for (ce = 0; ce < p->din; ce++)
		coa[ce] = p->cmin[ce];
	ce = 0;

	/* Itterate throught the PCS clut grid cells, */
	/* computing new auxiliary values */
	while (ce < p->din) {
		float *gp, *dp, *sp, *rp;		/* Grid pointer */

		/* Compute base of cell pointer */
		gp = p->g;		/* Grid pointer */
		for (e = 0; e < p->din; e++)
			gp += coa[e] * p->fci[e];
			
		if ((XLF_FLAGV(gp) & XLF_TOFIX) != 0) {
			int ud = 0;					/* Update point flag */
			int th = 0;					/* Try harder flag */
			sp = gp + p->oauxvv;		/* Source */
			dp = gp + p->oauxv;			/* Destination */
			rp = gp + p->oauxr;			/* Range */
			for (f = 0; f < p->daux; f++) {
				double v, ov;
				v = sp[f];
				if (v < rp[2 * f]) {		/* Limit new aux to valid locus range */
					if (tharder)
						th = 1;
					else
						v = rp[2 * f];
				}
				if (v > rp[2 * f + 1]) {
					if (tharder)
						th = 1;
					else
						v = rp[2 * f + 1];
				}
				ov = dp[f];					/* Old aux value */
				if (fabs(ov - v) > 0.001) {
					dp[f] = (float)v;
					ud = 1;					/* Worth updating it */
				}
			}

			if (ud != 0) {
				XLF_FLAGV(gp) |= XLF_UPDATE;
				if (th != 0)
					XLF_FLAGV(gp) |= XLF_HARDER;
			}
			if (XLF_FLAGV(gp) & XLF_UPDATE)
				tcount++;
		}

		/* Increment the counter */
		for (ce = 0; ce < p->din; ce++) {
			if (++coa[ce] < p->cmax[ce])
				break;			/* No carry */
			coa[ce] = p->cmin[ce];
		}
	}
#ifdef DO_STATS
	printf("~1 totol no. cells that will change = %d\n",tcount);
#endif

	return tcount;
}

/* ----------------------------------------- */
/* Update the grid values given the new auxiliary targets. */
/* Reset the TOFIX flags. */
static void fix_grid(
xifs *p,
double auxw		/* Compromise PCS factor */
) {
	int e, f;
	int coa[MAX_CHAN], ce;	/* grid counter */

	/* Init the grid counter */
	for (ce = 0; ce < p->din; ce++)
		coa[ce] = p->cmin[ce];
	ce = 0;

	/* Itterate throught the PCS clut grid cells, */
	/* computing new auxiliary values */
	while (ce < p->din) {
		float *gp, *ep;		/* Grid pointer */

		/* Compute base of cell pointer */
		gp = p->g;		/* Grid pointer */
		for (e = 0; e < p->din; e++)
			gp += coa[e] * p->fci[e];
			
		if ((XLF_FLAGV(gp) & XLF_TOFIX) != 0) {
			if ((XLF_FLAGV(gp) & XLF_UPDATE) != 0) {
				double out[MAX_CHAN], auxv[MAX_CHAN], pcs[MAX_CHAN];
				double auxrv[MAX_CHAN];

				ep = gp + p->opcs;
				for (f = 0; f < 3; f++)					/* Get PCS values */
					pcs[f] = (double)ep[f];

				ep = gp + p->oauxv;
				for (f = 0; f < p->daux; f++)			/* Get auxiliary values */
					auxv[f] = (double)ep[f];

				ep = gp + p->oout;

				if ((XLF_FLAGV(gp) & XLF_HARDER) != 0) {
					/* Use "try harder" PCS->devicve lookup */

					/* Set starting value */
					for (f = 0; f < p->dout; f++)
						out[f] = (double)ep[f];

					if (comp_pcs(p, auxw, auxrv, auxv, pcs, out) == 0) {
						for (f = 0; f < p->dout; f++)		/* Save the new output values */
							ep[f] = (float)out[f];
					}
#ifndef NEVER
					  else {
						printf("~9 comp_pcs failed!\n");
					}
#endif

					/* Save actual auxiliary values */
					ep = gp + p->oauxv;
					for (f = 0; f < p->daux; f++)
						ep[f] = (float)auxrv[f];

				} else {

					/* Lookup output value with different auxiliary target */
					p->clutpcsfunc(p->cbctx, out, auxv, pcs);

					for (f = 0; f < p->dout; f++)			/* Save the new output values */
						ep[f] = (float)out[f];

					/* assume auxiliary target will have been met */
				}
			}

			XLF_FLAGV(gp) &= ~(XLF_TOFIX | XLF_UPDATE | XLF_HARDER);
		}

		/* Increment the counter */
		for (ce = 0; ce < p->din; ce++) {
			if (++coa[ce] < p->cmax[ce])
				break;			/* No carry */
			coa[ce] = p->cmin[ce];
		}
	}
}

/* ----------------------------------------- */

/* minimizer callback function */
static double minfunc(	/* Return function value */
void *fdata,		/* Opaque data pointer */
double *tp			/* Device input value */
) {
	xifs *p = (xifs *)fdata;
	double pcs[3];
	double auxv[MAX_CHAN];
	double olimit;
	double fval;
	double tval;
	int e, f;

#define GWHT  1000.0

	/* Check if the device input values are outside (assumed) device range 0.0 - 1.0 */
	for (tval = 0.0, f = 0; f < p->dout; f++) {
		if (tp[f] < 0.0) {
			if (tp[0] > tval)
				tval = tp[0];
			tp[f] = 0.0;
		} else if (tp[f] > 1.0) {
			if ((tp[0] - 1.0) > tval)
				tval = (tp[0] - 1.0);
			tp[f] = 1.0;
		}
	}

	/* Figure PCS, auxiliary error, and amount over ink limit */
	p->clutipcsfunc(p->cbctx, pcs, &olimit, auxv, tp);

	if (olimit > tval)
		tval = olimit;

	fval = GWHT * tval;		/* Largest value that exceeds device/ink range */

	/* Figure auxiliary error */
 	for (tval = 0.0, e = 0; e < p->daux; e++) {
		double tt;
		tt = auxv[e] - p->m_auxv[e];
		tval += tt * tt;
	}

	fval += p->m_auxw * sqrt(tval);

	/* Figure PCS error */
 	for (tval = 0.0, e = 0; e < 3; e++) {
		double tt;
		tt = pcs[e] - p->m_pcs[e];
		tval += tt * tt;
	}

	fval += sqrt(tval);

//printf("~9 minfunc returning error %f on %f %f %f %f\n", fval, tp[0], tp[1], tp[2], tp[3]);
	return fval;
}

/* Given a PCS target, and auxiliary target, and a current */
/* device value, return a device value that is a better */
/* tradeoff between the PCS target and the auxiliary target. */
/* return non-zero on error */
static int comp_pcs(
xifs *p,			/* Aux fix structure */
double auxw,		/* Auxiliary error weighting factor (ie. 5 - 100) */
double *auxrv,		/* If not NULL, return actual auxiliary acheived */
double *auxv,		/* Auxiliary target value */
double *pcs,		/* PCS target value */
double *dev			/* Device starting value, return value */
) {
	int i;
	double rv;
	double ss[MAX_CHAN];
	double check[3];	/* Check PCS */
#ifdef NEVER	/* Diagnostic info */
double start[3];	/* Starting PCS */
double auxst[1];	/* Starting aux */
double auxch[1];	/* Auxiliary check value */
p->clutipcsfunc(p->cbctx, start, NULL, auxst, dev);
#endif

	p->m_auxw = auxw;

	for (i = 0; i < p->daux; i++)
		p->m_auxv[i] = auxv[i];

	for (i = 0; i < 3; i++)
		p->m_pcs[i] = pcs[i];

	/* Set initial search size */
	for (i = 0; i < p->dout; i++)
		ss[i] = 0.3;

	if (powell(&rv, p->dout, dev, ss, 0.001, 1000, minfunc, p, NULL, NULL) != 0) {
		return 1;
	}

	/* Sanitise device values */
	for (i = 0; i < p->dout; i++) {
		double v = dev[i];
		if (v < 0.0)
			v = 0.0;
		else if (v > 1.0)
			v = 1.0;
		dev[i] = v;
	}

	if (auxrv != NULL) {	/* Return actual auxiliary */
		p->clutipcsfunc(p->cbctx, check, NULL, auxrv, dev);
	}

#ifdef NEVER	/* Diagnostic info */
p->clutipcsfunc(p->cbctx, check, NULL, auxch, dev);
printf("~9 comp_pcs target    aux %f  PCS %f %f %f\n", auxv[0], pcs[0], pcs[1], pcs[2]);
printf("~9          returning error %f on %f %f %f %f\n", rv, dev[0], dev[1], dev[2], dev[3]);
printf("~9          PCS start  = %f %f %f (%f)\n",start[0], start[1], start[2],
                                                  absdiffn(3, start, pcs));
printf("~9          PCS finish = %f %f %f (%f)\n",check[0], check[1], check[2],
                                                  absdiffn(3, check, pcs));
printf("~9          PCS delta = %f, aux delta = %f\n", absdiffn(3, start, check),
                                                       fabs(auxst[0] - auxch[0]));
#endif /* NEVER */

	return 0;
}







































































