#ifndef ROOT_H
#define ROOT_H

/*
 * Copyright 2000 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#ifdef __cplusplus
	extern "C" {
#endif

/* 1 dimentional root finding code */

/* Bracket search function */
/* return  0 on sucess */
/*        -1 on no range */
/*        -2 on too many itterations */
int zbrac(
double *x1,			/* Input and output bracket values */
double *x2,
double (*func)(void *fdata, double tp),	/* function to evaluate */
void *fdata);							/* Opaque data pointer */

/* Root finder */
/* return  0 on sucess */
/*        -1 on root not bracketed */
/*        -2 on too many itterations */
int zbrent(
double *rv,								/* Return value */
double x1,								/* Bracket to search */
double x2,								/* (Min, Max) */
double tol,								/* Desired tollerance */
double (*func)(void *fdata, double tp),	/* function to evaluate */
void *fdata);							/* Opaque data pointer */

#ifdef __cplusplus
	}
#endif

#endif /* ROOT_H */
