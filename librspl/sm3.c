
/* Test smoothness scaling behaviour for 1D */

#include <stdio.h>
#include <math.h>

double trans(double *v, int luord, double vv);

double f(double x, double y, double z) {
	double fp[5] = { 1.0, 0.7, -0.3, 0.0, 0.0 };
	double v;

#ifdef NEVER
	/* 1D function */
	v = trans(fp, 5, x);
#else
	/* 1D on angle */
	v = trans(fp, 5, (x+y+z)/3.0);
	v *= 3.0 * sqrt(3.0);		// ?????
#endif

	return v;
}

int main() {
	double min = 0.0;
	double max = 1.0;
	int di = 3;
	int i, res = 4;

#define LOC(xx) (min + (max-min) * (xx)/(res-1.0))

	/* For each resolution */
	for (i = 0; i < 10; i++, res *= 2) {
		double tse = 0.0;		/* Total squared error */
		int j, k, m;

		/* For each grid point with neigbors */
		for (j = 1; j < (res-1); j++) {
			for (k = 1; k < (res-1); k++) {
				for (m = 1; m < (res-1); m++) {
					double err;
					double y1, y2, y3;
		
					y1 = f(LOC(j-1), LOC(k), LOC(m));
					y2 = f(LOC(j+0), LOC(k), LOC(m));
					y3 = f(LOC(j+1), LOC(k), LOC(m));
					err = 0.5 * (y3 + y1) - y2;
					tse += err * err;
	
					y1 = f(LOC(j), LOC(k-1), LOC(m));
					y2 = f(LOC(j), LOC(k+0), LOC(m));
					y3 = f(LOC(j), LOC(k+1), LOC(m));
					err = 0.5 * (y3 + y1) - y2;
					tse += err * err;

					y1 = f(LOC(j), LOC(k), LOC(m-1));
					y2 = f(LOC(j), LOC(k), LOC(m+0));
					y3 = f(LOC(j), LOC(k), LOC(m+1));
					err = 0.5 * (y3 + y1) - y2;
					tse += err * err;
				}
			}
		}
		/* Apply adjustments and corrections */
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
