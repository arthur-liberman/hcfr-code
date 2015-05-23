#ifndef SOBOL_H
#define SOBOL_H

#ifdef __cplusplus
	extern "C" {
#endif

#define SOBOL_MAXBIT 30
#define SOBOL_MAXDIM 40

/* Object definition */
struct _sobol {
	/* Private: */
	int          dim;			/* dimension we're set for */
	unsigned int count;
	double       recipd;
	int          lastq[SOBOL_MAXDIM];
	int          dir[SOBOL_MAXBIT][SOBOL_MAXDIM];

	/* Public: */
	/* Methods */

	/* Get the next sobol vector, return nz if we've run out */
	/* Values are between 0.0 and 1.0 */
	int (*next)(struct _sobol *s, double *v);

	/* Rest to the begining of the sequence */
	void (*reset)(struct _sobol *s);

	/* We're done with the object */
	void (*del)(struct _sobol *s);

}; typedef struct _sobol sobol;

/* Return NULL on error */
sobol *new_sobol(int dim);

#ifdef __cplusplus
	}
#endif

#endif /* SOBOL_H */










