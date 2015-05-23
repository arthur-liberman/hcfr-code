
/************************************************/
/* Benchmark RSPL reverse lookup                */ 
/************************************************/

/* Author: Graeme Gill
 * Date:   31/10/96
 * Derived from tnd.c
 * Copyright 1999 - 2000 Graeme W. Gill
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include "copyright.h"
#include "aconfig.h"
#include "rspl.h"
#include "numlib.h"
//#include "ui.h"

#undef DOLIMIT			/* Define to have ink limit */
#define LIMITVAL 2.5	/* Total ink limit sum */
#undef DOCHECK

#define SHOW_OUTPUT		/* Define for printf on each conversion */

/* 11, 19 give about 60 seconds */
#define GRES 17			/* Default grid resolution */
#define RRES 43			/* Default reverse test resolution */
#define DI 4			/* Dimensions in */
#define FDI 3			/* Function (out) Dimensions */
#define NIP 10			/* Number of solutions allowed */

#define flimit(vv) ((vv) < 0.0 ? 0.0 : ((vv) > 1.0 ? 1.0 : (vv)))
#define fmin(a,b) ((a) < (b) ? (a) : (b))
#define fmin3(a,b,c)  (fmin((a), fmin((b),(c))))
#define fmax(a,b) ((a) > (b) ? (a) : (b))
#define fmax3(a,b,c)  (fmax((a), fmax((b),(c))))

/* Fwd function approximated by rspl */
/* Dummy cmyk->rgb conversion. This simulates our device */
void func(
void *cbctx,
double *out,
double *in) {
	double kk;
	double ci = in[0];
	double mi = in[1];
	double yi = in[2];
	double ki = in[3];
	double r,g,b;

	ci += ki;			/* Add black back in */
	mi += ki;
	yi += ki;
	kk = fmax3(ci,mi,yi);
	if (kk > 1.0) {
		ci /= kk;
		mi /= kk;
		yi /= kk;
	}
	r = 1.0 - ci;
	g = 1.0 - mi;
	b = 1.0 - yi;
	out[0] = flimit(r);
	out[1] = flimit(g);
	out[2] = flimit(b);
}

/* Simplex ink limit function */
double limitf(
void *lcntx,
float *in
) {
	int i;
	double ov;

	for (ov = 0.0, i = 0; i < DI; i++) {
		ov += in[i];
	} 
	return ov;
}


void usage(void) {
	fprintf(stderr,"Benchmark rspl reverse, Version %s\n",ARGYLL_VERSION_STR);
	fprintf(stderr,"usage: revbench [-f fwdres] [-r revres] [-v level] iccin iccout\n");
	fprintf(stderr," -v            Verbose\n");
	fprintf(stderr," -f res        Set forward grid res\n");
	fprintf(stderr," -r res        Set reverse test res\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	int clutres = GRES;
	int rres = RRES;
	int verb = 0;
	int gres[MXDI];
	int e;

	clock_t stime,ttime;
	rspl *rss;	/* Multi-resolution regularized spline structure */

	error_program = argv[0];

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

			/* Verbosity */
			else if (argv[fa][1] == 'v' || argv[fa][1] == 'V') {
				verb = 1;
			}
			else if (argv[fa][1] == 'f' || argv[fa][1] == 'F') {
				fa = nfa;
				if (na == NULL) usage();
				clutres = atoi(na);
			}
			else if (argv[fa][1] == 'r' || argv[fa][1] == 'R') {
				fa = nfa;
				if (na == NULL) usage();
				rres = atoi(na);
			}
			else 
				usage();
		} else
			break;
	}

	printf("Started benchmark\n");
	/* Create the object */
	rss =  new_rspl(RSPL_NOFLAGS, DI, FDI);

	for (e = 0; e < DI; e++)
		gres[e] = clutres;

	printf("Rspl allocated\n");
	rss->set_rspl(rss, 0, (void *)NULL, func,
	               NULL, NULL, gres, NULL, NULL);

	printf("Rspl set\n");

	/* Start exploring the reverse test grid */
	{
		int ops = 0;
		double secs;
		rpsh counter;
		unsigned rcount;
		int ii[10];
		int f, rgres[MXDO];

		int flags = 0;		/* rev hint flags */
		co tp[NIP];			/* Test point */
		double cvec[4];		/* Text clip vector */
		int auxm[4];		/* Auxiliary target value valid flag */
#ifdef DOCHECK
		int j;
#endif
#ifdef NEVER
		double lmin[4], lmax[4];	/* Locus min/max values */
#endif

#ifdef DOCHECK
		char *check;		/* Check that we hit every cell */
#endif /* DOCHECK */

		/* Set auxiliary target mask */
		auxm[0] = 0;
		auxm[1] = 0;
		auxm[2] = 0;
		auxm[3] = 1;

#ifdef DOLIMIT
		rss->rev_set_limit(rss,
			limitf,
			NULL,
			LIMITVAL	/* limit maximum value */
		);
#endif /* DOLIMIT */

		printf("Forward resolution %d\n",clutres);
		printf("Reverse resolution %d\n",rres);

#ifdef DOCHECK
		if ((check = (char *)calloc(1, rcount)) == NULL)
			error("Malloc of check array\n");
#endif /* DOCHECK */

		for (f = 0; f < FDI; f++)
			rgres[f] = rres;

		rcount = rpsh_init(&counter, FDI, (unsigned int *)rgres, ii);	/* Initialise counter */
		
		stime = clock();

		/* Itterate though the grid */
		for (ops = 0;; ops++) {
			int r;
			int e;			/* Table index */
	
#ifdef DOCHECK
			check[((ii[2] * rres + ii[1]) * rres) + ii[0]] = 1;
#endif /* DOCHECK */

			for (e = 0; e < FDI; e++) { 	/* Input tables */
				tp[0].v[e] = ii[e]/(rres-1.0);		/* Vertex coordinates */
			}
	
			if (verb)
				printf("Input = %f %f %f\n",tp[0].v[0], tp[0].v[1], tp[0].v[2]);

#ifdef NEVER	/* Do locus lookup explicitly ? */
			/* Lookup the locus for the auxiliary (Black) chanel */
			if ((r = rss->rev_locus(rss,
				auxm, 	/* auxm Auxiliary mask flags */
				tp,		/* Input and auxiliary values */
				lmin,	/* Locus min/max return values */
				lmax
				)) == 0) {
				/* Rev locus failed - means that it will clip ? */
				tp[0].p[3] = 0.5;
				flags = RSPL_WILLCLIP;	/* Since there was no locus, we expect to clip */
			} else {
				/* Set the auxiliary target */
				tp[0].p[3] = (lmin[3] + lmax[3])/2.0;
				flags = RSPL_EXACTAUX;	/* Since we got locus, expect exact auxiliary match */
			}
#else
				tp[0].p[3] = 0.5;
				flags = RSPL_AUXLOCUS;	/* Auxiliary target is proportion of locus */
#endif	/* NEVER */

			/* Clip vector to 0.5 */
			cvec[0] = 0.5 - tp[0].v[0];
			cvec[1] = 0.5 - tp[0].v[1];
			cvec[2] = 0.5 - tp[0].v[2];
			cvec[3] = 0.5 - tp[0].v[3];

			/* Do reverse interpolation */
			if ((r = rss->rev_interp(rss,
				flags,	/* Hint flags */
				NIP,	/* Number of solutions allowed */
				auxm, 	/* auxm Auxiliary mask flags */
				cvec, 	/* cvec Clip vector direction & length */
				tp)		/* Input and output values */
				) == 0) {
				error("rev_interp failed\n");
			}
			
			r &= RSPL_NOSOLNS;		/* Get number of solutions */

			if (verb)
				printf("Output 1 of %d: %f, %f, %f, %f%s\n",
				  r & RSPL_NOSOLNS, tp[0].p[0], tp[0].p[1], tp[0].p[2], tp[0].p[3],
			      (r & RSPL_DIDCLIP) ? " [Clipped]" : "");


			if (rpsh_inc(&counter, ii))
				break;

		}		/* Next grid point */

		ttime = clock() - stime;
		secs = (double)ttime/CLOCKS_PER_SEC;
		printf("Done - %d ops in %f seconds, rate = %f ops/sec\n",ops, secs,ops/secs);
#ifdef DOCHECK
		for (j = 0; j < rcount; j++) {
			if (check[j] != 1) {
				printf("~~CHeck error at %d\n",j);
			}
		}
#endif /* DOCHECK */
	}

	rss->del(rss);
	return 0;
}






