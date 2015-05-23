#ifndef RAND_H
#define RAND_H

/*
 * Copyright 1998 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

#ifdef __cplusplus
	extern "C" {
#endif

/* Return a random number between 0 and 4294967294 */
unsigned int
rand32(						/* Return 32 bit random number */
unsigned int seed);			/* Optional seed. Non-zero re-initialized with that seed */

/* Return a random integer in the range min to max inclusive */
int i_rand(int min, int max);

/* Return a random double in the range min to max inclusive */
double d_rand(double min, double max);

/* Return a random floating point number with a gausian/normal */
/* distribution, centered about 0.0, with standard deviation 1.0 */
/* and an average deviation of 0.564 */
double norm_rand(void);

#ifdef __cplusplus
	}
#endif

#endif /* RAND_H */
