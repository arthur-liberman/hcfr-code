
/***************************************************/
/* Sobol sub-random vector sequence generator      */
/***************************************************/

/* Code is an expression of the algorithm decsribed in */
/* the SSOBOL.F fortran source file, with additional */
/* guidance from "Numerical Recipes in C", by W.H.Press, B.P.Flannery, */
/* S.A.Teukolsky & W.T.Vetterling. */

/*
 * Copyright 2002 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#include "numsup.h"
#include "sobol.h"

/*
 * The array poly gives successive primitive
 * polynomials coded in binary, e.g.
          45 = 100101
 * has bits 5, 2, and 0 set (counting from the
 * right) and therefore represents
        X**5 + X**2 + X**0

 * These polynomials are in the order used by
 * sobol in ussr comput. maths. math. phys. 16 (1977),
 * 236-242. 
 */

static int sobol_poly[SOBOL_MAXDIM] = {
	  1,   3,   7,  11,  13,  19,  25,  37,  59,  47,
	 61,  55,  41,  67,  97,  91, 109, 103, 115, 131,
	193, 137, 145, 143, 241, 157, 185, 167, 229, 171,
	213, 191, 253, 203, 211, 239, 247, 285, 369, 299
};

/*
 * The initialization of the array vinit is from 
 * Sobol and Levitan, the production of points uniformly
 * distributed in a multidimensional cube (in Russian),
 * preprint ipm akad. nauk sssr, no. 40, moscow 1976.
 * For a polynomial of degree m, m initial
 * values are needed : these are the values given here.
 * subsequent values are calculated during initialisation.
 */

static int vinit[8][SOBOL_MAXDIM] = {
	{
		0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1 
	},
	{
		0, 0, 1, 3, 1, 3, 1, 3, 3, 1,
		3, 1, 3, 1, 3, 1, 1, 3, 1, 3, 
		1, 3, 1, 3, 3, 1, 3, 1, 3, 1, 
		3, 1, 1, 3, 1, 3, 1, 3, 1, 3
	},
	{
		0, 0, 0, 7, 5, 1, 3, 3, 7, 5, 
		5, 7, 7, 1, 3, 3, 7, 5, 1, 1, 
		5, 3, 3, 1, 7, 5, 1, 3, 3, 7, 
		5, 1, 1, 5, 7, 7, 5, 1, 3, 3
	},
	{
		0, 0, 0, 0, 0, 1, 7, 9, 13, 11, 
		1, 3, 7, 9, 5, 13, 13, 11, 3, 15, 
		5, 3, 15, 7, 9, 13, 9, 1, 11, 7, 
		5, 15, 1, 15, 11, 5, 3, 1, 7, 9
	},
	{
		0, 0, 0, 0, 0, 0, 0, 9, 3, 27, 
		15, 29, 21, 23, 19, 11, 25, 7, 13, 17, 
		1, 25, 29, 3, 31, 11, 5, 23, 27, 19, 
		21, 5, 1, 17, 13, 7, 15, 9, 31, 9
	},
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
		0, 0, 0, 37, 33, 7, 5, 11, 39, 63, 
		27, 17, 15, 23, 29, 3, 21, 13, 31, 25, 
		9, 49, 33, 19, 29, 11, 19, 27, 15, 25
	},
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
		0, 0, 0, 0, 0, 0, 0, 0, 0, 13, 
		33, 115, 41, 79, 17, 29, 119, 75, 73, 105, 
		7, 59, 65, 21, 3, 113, 61, 89, 45, 107
	},
	{
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
		0, 0, 0, 0, 0, 0, 0, 7, 23, 39
	}
};

/* Get the next sobol vector */
/* return nz if we've run out */
static int next_sobol(sobol *s, double * v)
{
	int i, p;
	unsigned int c;

	s->count++;

	/* Find the position of the right-hand zero in count */
	for (c = s->count, p = 0; (c & 1) == 0; p++, c >>= 1)
		;

	if(p > SOBOL_MAXBIT)
		return 1;	/* Run out */

	for (i = 0; i < s->dim; i++) {
		s->lastq[i] ^= s->dir[p][i];
		v[i] = s->lastq[i] * s->recipd;
	}

	return 0;
}

/* Free up the object */
static void del_sobol(sobol *s) {
	if (s != NULL)
		free(s);
}

/* reset the count */
static void reset_sobol(sobol *s) {
	int i;

	/* Set up first vector and values */
	s->count = 0;
	for (i = 0; i < s->dim; i++)
		s->lastq[i] = 0;
}

/* Return NULL on error */
sobol *new_sobol(int dim) {
	sobol *s = NULL;
	int i, j, p;

	if (dim < 1 || dim > SOBOL_MAXDIM) {
		return NULL;
	}

	if ((s = (sobol *)malloc(sizeof(sobol))) == NULL) {
		return NULL;
	}

	s->dim  = dim;
	s->next  = next_sobol;
	s->reset = reset_sobol;
	s->del   = del_sobol;

	/* Initialize the direction table */
	for (i = 0; i < dim; i++) {

		if (i == 0) {
			for (j = 0; j < SOBOL_MAXBIT; j++)
				s->dir[j][i] = 1;
		} else {
			int m;				/* Degree */
			int pm;				/* Polinomial mask */
	
			/* Find degree of polynomial from binary encoding */
			for (m = 0, pm = sobol_poly[i] >> 1; pm != 0; m++, pm >>= 1)
				; 
	
			/* The leading elements of row i come from vinit[][] */
			for (j = 0; j < m; j++) {
				s->dir[j][i] = vinit[j][i];
			}
	
			/* Calculate remaining elements of row i as explained */
			/* in bratley and fox, section 2 */
			pm = sobol_poly[i];
			for (j = m; j < SOBOL_MAXBIT; j++) {
				int k;
				int newv = s->dir[j-m][i];
				for (k = 0; k < m; k++) {
					if (pm & (1 << (m-k-1))) {
						newv ^= s->dir[j-k-1][i] << (k+1);
					}
				}
				s->dir[j][i] = newv;
			}
		}
	}
	/* Multiply columns of v by appropriate power of 2 */
	for (p = 2, j = SOBOL_MAXBIT-2; j >= 0; j--, p <<= 1) {
		for (i = 0; i < dim; i++)
			s->dir[j][i] *= p;
	}

	/* recipd is 1/(common denominator of the elements in v) */
	s->recipd = 1.0/(1 << SOBOL_MAXBIT);

	/* Set up first vector and values */
	s->count = 0;
	for (i = 0; i < dim; i++)
		s->lastq[i] = 0;

	return s;
}

