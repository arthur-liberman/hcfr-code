
/* Test smoothness scaling behaviour for 1D */

#include <stdio.h>
#include <math.h>

double trans(double *v, int luord, double vv);

double f(double x) {
	double fp[5] = { +1.0, 0.7, -0.3, 0.0, 0.0 };
	double y;

	y = trans(fp, 5, x);

	return y;
}

int main() {
	double min = 0.0;
	double max = 1.0;
	int i, res = 4;
	int di = 1;

#define LOC(xx) (min + (max-min) * (xx)/(res-1.0))

	/* For each resolution */
	for (i = 0; i < 10; i++, res *= 2) {
		double tse = 0.0;		/* Total squared error */
		int j;

		/* For each grid point with neigbors */
		for (j = 1; j < (res-1); j++) {
			double err;
			double y1, y2, y3;

			y1 = f(LOC(j-1));
			y2 = f(LOC(j));
			y3 = f(LOC(j+1));
			err = 0.5 * (y3 + y1) - y2;
			tse += err * err;
//			tse += fabs(err);
		}
		/* Apply adjustments and corrections to error squared */
		tse *= pow((res-1.0), 4.0);					/* Aprox. geometric resolution factor */
		tse /= pow((res-2.0),(double)di);			/* Average squared non-smoothness */

//		tse /= (di * pow((res-2.0),(double)di));	/* Average squared non-smoothness */
		printf("Res %d, tse = %f\n",res,tse);
	}


	return 0;
}

/* Transfer function */
double trans(
double *v,			/* Pointer to first parameter */
int    luord,		/* Number of parameters */
double vv			/* Source of value */
) {
	double g;
	int ord;

	for (ord = 0; ord < luord; ord++) {
		int nsec;			/* Number of sections */
		double sec;			/* Section */

		g = v[ord];			/* Parameter */

		nsec = ord + 1;		/* Increase sections for each order */

		vv *= (double)nsec;

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
		vv /= (double)nsec;
	}

	return vv;
} 
