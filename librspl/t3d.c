/************************************************/
/* Test RSPL in 3D */
/************************************************/

/* Author: Graeme Gill
 * Date:   22/4/96
 * Derived from cmatch.c
 * Copyright 1995 Graeme W. Gill
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */


#define DEBUG
#define DETAILED

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include "rspl.h"
#include "tiffio.h"
#include "plot.h"
#include "ui.h"

#ifdef NEVER
#define INTERP spline_interp
#else
#define INTERP interp
#endif

#ifdef NEVER
FILE *verbose_out = stdout;
int verbose_level = 6;			/* Current verbosity level */
								/* 0 = none */
								/* !0 = diagnostics */
#endif /* NEVER */

#define PLOTRES 256
#define WIDTH 400			/* Raster size */
#define HEIGHT 400

#define MAX_ITS 500
#define IT_TOL 0.0005
#define GRES0 33			/* Default rspl resolutions */
#define GRES1 33
#define GRES2 33
#undef NEVER
#define ALWAYS

//double t1xa[PNTS] = { 0.2, 0.25, 0.30, 0.35,  0.40,  0.44, 0.48, 0.51,  0.64,  0.75  };
//double t1ya[PNTS] = { 0.3, 0.35, 0.4,  0.41,  0.42,  0.46, 0.5,  0.575, 0.48,  0.75  };

/* 1D test function repeated 3 times along x = y = 0.5 */
co test_points1[] = {
	{{ 0.4,0.4,0.20 },{ 0.30  }},	/* 0 */
	{{ 0.4,0.4,0.25 },{ 0.35  }},	/* 1 */
	{{ 0.4,0.4,0.30 },{ 0.40  }},	/* 2 */
	{{ 0.4,0.4,0.35 },{ 0.41  }},	/* 3 */
	{{ 0.4,0.4,0.40 },{ 0.42  }},	/* 4 */
	{{ 0.4,0.4,0.44 },{ 0.46  }},	/* 5 */
	{{ 0.4,0.4,0.48 },{ 0.50  }},	/* 6 */
	{{ 0.4,0.4,0.51 },{ 0.575 }},	/* 7 */
	{{ 0.4,0.4,0.64 },{ 0.48  }},	/* 8 */
	{{ 0.4,0.4,0.75 },{ 0.75  }},	/* 9 */

	{{ 0.5,0.4,0.20 },{ 0.30  }},	/* 0 */
	{{ 0.5,0.4,0.25 },{ 0.35  }},	/* 1 */
	{{ 0.5,0.4,0.30 },{ 0.40  }},	/* 2 */
	{{ 0.5,0.4,0.35 },{ 0.41  }},	/* 3 */
	{{ 0.5,0.4,0.40 },{ 0.42  }},	/* 4 */
	{{ 0.5,0.4,0.44 },{ 0.46  }},	/* 5 */
	{{ 0.5,0.4,0.48 },{ 0.50  }},	/* 6 */
	{{ 0.5,0.4,0.51 },{ 0.575 }},	/* 7 */
	{{ 0.5,0.4,0.64 },{ 0.48  }},	/* 8 */
	{{ 0.5,0.4,0.75 },{ 0.75  }},	/* 9 */

	{{ 0.6,0.4,0.20 },{ 0.30  }},	/* 0 */
	{{ 0.6,0.4,0.25 },{ 0.35  }},	/* 1 */
	{{ 0.6,0.4,0.30 },{ 0.40  }},	/* 2 */
	{{ 0.6,0.4,0.35 },{ 0.41  }},	/* 3 */
	{{ 0.6,0.4,0.40 },{ 0.42  }},	/* 4 */
	{{ 0.6,0.4,0.44 },{ 0.46  }},	/* 5 */
	{{ 0.6,0.4,0.48 },{ 0.50  }},	/* 6 */
	{{ 0.6,0.4,0.51 },{ 0.575 }},	/* 7 */
	{{ 0.6,0.4,0.64 },{ 0.48  }},	/* 8 */
	{{ 0.6,0.4,0.75 },{ 0.75  }},	/* 9 */


	{{ 0.4,0.5,0.20 },{ 0.30  }},	/* 0 */
	{{ 0.4,0.5,0.25 },{ 0.35  }},	/* 1 */
	{{ 0.4,0.5,0.30 },{ 0.40  }},	/* 2 */
	{{ 0.4,0.5,0.35 },{ 0.41  }},	/* 3 */
	{{ 0.4,0.5,0.40 },{ 0.42  }},	/* 4 */
	{{ 0.4,0.5,0.44 },{ 0.46  }},	/* 5 */
	{{ 0.4,0.5,0.48 },{ 0.50  }},	/* 6 */
	{{ 0.4,0.5,0.51 },{ 0.575 }},	/* 7 */
	{{ 0.4,0.5,0.64 },{ 0.48  }},	/* 8 */
	{{ 0.4,0.5,0.75 },{ 0.75  }},	/* 9 */

	{{ 0.5,0.5,0.20 },{ 0.30  }},	/* 0 */
	{{ 0.5,0.5,0.25 },{ 0.35  }},	/* 1 */
	{{ 0.5,0.5,0.30 },{ 0.40  }},	/* 2 */
	{{ 0.5,0.5,0.35 },{ 0.41  }},	/* 3 */
	{{ 0.5,0.5,0.40 },{ 0.42  }},	/* 4 */
	{{ 0.5,0.5,0.44 },{ 0.46  }},	/* 5 */
	{{ 0.5,0.5,0.48 },{ 0.50  }},	/* 6 */
	{{ 0.5,0.5,0.51 },{ 0.575 }},	/* 7 */
	{{ 0.5,0.5,0.64 },{ 0.48  }},	/* 8 */
	{{ 0.5,0.5,0.75 },{ 0.75  }},	/* 9 */

	{{ 0.6,0.5,0.20 },{ 0.30  }},	/* 0 */
	{{ 0.6,0.5,0.25 },{ 0.35  }},	/* 1 */
	{{ 0.6,0.5,0.30 },{ 0.40  }},	/* 2 */
	{{ 0.6,0.5,0.35 },{ 0.41  }},	/* 3 */
	{{ 0.6,0.5,0.40 },{ 0.42  }},	/* 4 */
	{{ 0.6,0.5,0.44 },{ 0.46  }},	/* 5 */
	{{ 0.6,0.5,0.48 },{ 0.50  }},	/* 6 */
	{{ 0.6,0.5,0.51 },{ 0.575 }},	/* 7 */
	{{ 0.6,0.5,0.64 },{ 0.48  }},	/* 8 */
	{{ 0.6,0.5,0.75 },{ 0.75  }},	/* 9 */


	{{ 0.4,0.6,0.20 },{ 0.30  }},	/* 0 */
	{{ 0.4,0.6,0.25 },{ 0.35  }},	/* 1 */
	{{ 0.4,0.6,0.30 },{ 0.40  }},	/* 2 */
	{{ 0.4,0.6,0.35 },{ 0.41  }},	/* 3 */
	{{ 0.4,0.6,0.40 },{ 0.42  }},	/* 4 */
	{{ 0.4,0.6,0.44 },{ 0.46  }},	/* 5 */
	{{ 0.4,0.6,0.48 },{ 0.50  }},	/* 6 */
	{{ 0.4,0.6,0.51 },{ 0.575 }},	/* 7 */
	{{ 0.4,0.6,0.64 },{ 0.48  }},	/* 8 */
	{{ 0.4,0.6,0.75 },{ 0.75  }},	/* 9 */

	{{ 0.5,0.6,0.20 },{ 0.30  }},	/* 0 */
	{{ 0.5,0.6,0.25 },{ 0.35  }},	/* 1 */
	{{ 0.5,0.6,0.30 },{ 0.40  }},	/* 2 */
	{{ 0.5,0.6,0.35 },{ 0.41  }},	/* 3 */
	{{ 0.5,0.6,0.40 },{ 0.42  }},	/* 4 */
	{{ 0.5,0.6,0.44 },{ 0.46  }},	/* 5 */
	{{ 0.5,0.6,0.48 },{ 0.50  }},	/* 6 */
	{{ 0.5,0.6,0.51 },{ 0.575 }},	/* 7 */
	{{ 0.5,0.6,0.64 },{ 0.48  }},	/* 8 */
	{{ 0.5,0.6,0.75 },{ 0.75  }},	/* 9 */

	{{ 0.6,0.6,0.20 },{ 0.30  }},	/* 0 */
	{{ 0.6,0.6,0.25 },{ 0.35  }},	/* 1 */
	{{ 0.6,0.6,0.30 },{ 0.40  }},	/* 2 */
	{{ 0.6,0.6,0.35 },{ 0.41  }},	/* 3 */
	{{ 0.6,0.6,0.40 },{ 0.42  }},	/* 4 */
	{{ 0.6,0.6,0.44 },{ 0.46  }},	/* 5 */
	{{ 0.6,0.6,0.48 },{ 0.50  }},	/* 6 */
	{{ 0.6,0.6,0.51 },{ 0.575 }},	/* 7 */
	{{ 0.6,0.6,0.64 },{ 0.48  }},	/* 8 */
	{{ 0.6,0.6,0.75 },{ 0.75  }}	/* 9 */
};

/* function  */
co test_points2[] = {
	{{ 0.50915, 0.50936, 0.57048 },{ 0.36209169635 }},
	{{ 0.85943, 0.84331, 0.81487 },{ 0.91571493013 }},
	{{ 0.11381, 0.80378, 0.82951 },{ 0.16052707023 }},
	{{ 0.79087, 0.11157, 0.83913 },{ 0.30641388193 }},
	{{ 0.16297, 0.23090, 0.96417 },{ 0.15005479047 }},
	{{ 0.78181, 0.80097, 0.00192 },{ 0.32179402798 }},
	{{ 0.16141, 0.84321, 0.16561 },{ 0.14446082013 }},
	{{ 0.79859, 0.11111, 0.15547 },{ -0.1308162293 }},
	{{ 0.12959, 0.16184, 0.21825 },{ 0.03520247555 }},
	{{ 1.00000, 0.46395, 0.84399 },{ 0.63914200000 }},
	{{ 0.48724, 0.76024, 1.00000 },{ 0.64150992880 }},
	{{ 0.40744, 0.00000, 0.65533 },{ 0.13060244736 }},
	{{ 0.10931, 0.43216, 0.25195 },{ 0.06329759515 }},
	{{ 0.69401, 0.99412, 0.55335 },{ 0.75632862795 }},
	{{ 0.51759, 0.30372, 0.13622 },{ 0.07965761859 }},
	{{ 0.98628, 0.40857, 0.47688 },{ 0.29286006552 }},
	{{ 0.44474, 0.26024, 1.00000 },{ 0.37263430380 }},
	{{ 0.15229, 0.52694, 0.75101 },{ 0.16014862087 }},
	{{ 0.50968, 0.74522, 0.14058 },{ 0.30725752992 }},
	{{ 0.82291, 0.10614, 0.49956 },{ 0.07762756903 }},
	{{ 0.15674, 0.78167, 0.50766 },{ 0.17389174472 }},
	{{ 0.12961, 0.17234, 0.65990 },{ 0.08236132255 }},
	{{ 1.00000, 0.83202, 0.35206 },{ 0.61366800000 }},
	{{ 0.36633, 0.89555, 0.76014 },{ 0.48373766601 }},
	{{ 0.85641, 0.39194, 0.18691 },{ 0.09699956583 }},
	{{ 0.50918, 0.23923, 0.42132 },{ 0.16380116928 }},
	{{ 0.65513, 0.40585, 0.83832 },{ 0.49065371733 }},
	{{ 0.66726, 0.75472, 0.25419 },{ 0.41666516892 }},
	{{ 0.22193, 0.58555, 0.13920 },{ 0.13003877385 }},
	{{ 0.41507, 0.89581, 0.25539 },{ 0.37048608609 }},
	{{ 0.43231, 0.12362, 0.17297 },{ 0.01981752271 }},
	{{ 0.78191, 0.63297, 0.72639 },{ 0.64361123257 }}
};

co test_points3[] = {
	{{ 0.50915, 0.50936, 0.57048 },{ -0.00010 }},
	{{ 0.85943, 0.84331, 0.81487 },{ 0.46573 }},
	{{ 0.11381, 0.80378, 0.82951 },{ 0.56085 }},
	{{ 0.79087, 0.11157, 0.83913 },{ 0.33378 }},
	{{ 0.16297, 0.23090, 0.96417 },{ 0.38872 }},
	{{ 0.78181, 0.80097, 0.00192 },{ 0.28654 }},
	{{ 0.16141, 0.84321, 0.16561 },{ 0.43410 }},
	{{ 0.79859, 0.11111, 0.15547 },{ 0.35140 }},
	{{ 0.12959, 0.16184, 0.21825 },{ 0.46711 }},
	{{ 1.00000, 0.46395, 0.84399 },{ 0.94213 }},
	{{ 0.48724, 0.76024, 1.00000 },{ 0.00058 }},
	{{ 0.40744, 0.00000, 0.65533 },{ 0.00859 }},
	{{ 0.10931, 0.43216, 0.25195 },{ 0.54679 }},
	{{ 0.69401, 0.99412, 0.55335 },{ 0.12494 }},
	{{ 0.51759, 0.30372, 0.13622 },{ -0.00126 }},
	{{ 0.98628, 0.40857, 0.47688 },{ 0.89573 }},
	{{ 0.44474, 0.26024, 1.00000 },{ 0.00280 }},
	{{ 0.15229, 0.52694, 0.75101 },{ 0.43807 }},
	{{ 0.50968, 0.74522, 0.14058 },{ 0.00004 }},
	{{ 0.82291, 0.10614, 0.49956 },{ 0.40990 }},
	{{ 0.15674, 0.78167, 0.50766 },{ 0.44271 }},
	{{ 0.12961, 0.17234, 0.65990 },{ 0.46804 }},
	{{ 1.00000, 0.83202, 0.35206 },{ 0.90938 }},
	{{ 0.36633, 0.89555, 0.76014 },{ 0.07020 }},
	{{ 0.85641, 0.39194, 0.18691 },{ 0.48559 }},
	{{ 0.50918, 0.23923, 0.42132 },{ -0.00391 }},
	{{ 0.65513, 0.40585, 0.83832 },{ 0.09449 }},
	{{ 0.66726, 0.75472, 0.25419 },{ 0.10065 }},
	{{ 0.22193, 0.58555, 0.13920 },{ 0.28186 }},
	{{ 0.41507, 0.89581, 0.25539 },{ 0.02877 }},
	{{ 0.43231, 0.12362, 0.17297 },{ 0.00237 }},
	{{ 0.78191, 0.63297, 0.72639 },{ 0.29573 }}
};


/* x + y^2 + z^1/3 function with one non-monotonic point */
co test_points4[] = {
	{{ 0.1,0.1,0.5 },{ 0.11 }},	/* 0 */
	{{ 0.2,0.7,0.1 },{ 0.69 }},	/* 1 */
	{{ 0.8,0.8,0.8 },{ 1.44 }}, /* 2 */
	{{ 0.5,0.6,0.4 },{ 0.86 }},	/* 3 */
	{{ 0.2,0.5,0.2 },{ 0.45 }},	/* 4 */
	{{ 0.3,0.7,0.2 },{ 0.35 }},	/* nm 5 */
	{{ 0.5,0.4,0.9 },{ 0.66 }},	/* 6 */
	{{ 0.1,0.9,0.7 },{ 0.91 }},	/* 7 */
	{{ 0.7,0.2,0.1 },{ 0.74 }},	/* 8 */
	{{ 0.8,0.4,0.3 },{ 0.96 }},	/* 9 */
	{{ 0.3,0.3,0.4 },{ 0.39 }}	/* 10 */
	};

/* doubled up x + y^2 + z^1/3 function with one non-monotonic point */
co test_points5[] = {
	{{ 0.1,0.1,0.5 },{ 0.11 }},	/* 0 */
	{{ 0.101,0.101,0.501 },{ 0.11 }},	/* 0d */
	{{ 0.2,0.7,0.1 },{ 0.69 }},	/* 1 */
	{{ 0.201,0.701,0.101 },{ 0.69 }},	/* 1d */
	{{ 0.8,0.8,0.8 },{ 1.44 }}, /* 2 */
	{{ 0.801,0.801,0.801 },{ 1.44 }}, /* 2d */
	{{ 0.5,0.6,0.4 },{ 0.86 }},	/* 3 */
	{{ 0.501,0.601,0.401 },{ 0.86 }},	/* 3d */
	{{ 0.2,0.5,0.2 },{ 0.45 }},	/* 4 */
	{{ 0.201,0.501,0.201 },{ 0.45 }},	/* 4d */
	{{ 0.3,0.7,0.2 },{ 0.35 }},	/* nm 5 */
	{{ 0.301,0.701,0.201 },{ 0.35 }},	/* nm 5d */
	{{ 0.5,0.4,0.9 },{ 0.66 }},	/* 6 */
	{{ 0.501,0.401,0.901 },{ 0.66 }},	/* 6d */
	{{ 0.1,0.9,0.7 },{ 0.91 }},	/* 7 */
	{{ 0.101,0.901,0.701 },{ 0.91 }},	/* 7d */
	{{ 0.7,0.2,0.1 },{ 0.74 }},	/* 8 */
	{{ 0.701,0.201,0.101 },{ 0.74 }},	/* 8d */
	{{ 0.8,0.4,0.3 },{ 0.96 }},	/* 9 */
	{{ 0.801,0.401,0.301 },{ 0.96 }},	/* 9d */
	{{ 0.3,0.3,0.4 },{ 0.39 }},	/* 10 */
	{{ 0.301,0.301,0.401 },{ 0.39 }}	/* 10d */
	};

co test_points6[] = {
	{{ 0.0069, 0.0071, 0.0061 },{ 0.0726 }},
	{{ 0.0068, 0.0071, 0.0060 },{ 0.0704 }},
	{{ 0.0069, 0.0072, 0.0062 },{ 0.0720 }},
	{{ 0.0069, 0.0072, 0.0061 },{ 0.0734 }},
	{{ 0.0069, 0.0072, 0.0063 },{ 0.0750 }},
	{{ 0.0070, 0.0072, 0.0062 },{ 0.0779 }},
	{{ 0.0070, 0.0072, 0.0063 },{ 0.0741 }},
	{{ 0.0069, 0.0072, 0.0061 },{ 0.0745 }},
	{{ 0.0069, 0.0072, 0.0061 },{ 0.0747 }},
	{{ 0.0071, 0.0073, 0.0063 },{ 0.0760 }},
	{{ 0.0070, 0.0073, 0.0063 },{ 0.0751 }},
	{{ 0.0070, 0.0073, 0.0062 },{ 0.0759 }},
	{{ 0.0071, 0.0074, 0.0062 },{ 0.0693 }},
	{{ 0.0071, 0.0074, 0.0064 },{ 0.0740 }},
	{{ 0.0072, 0.0075, 0.0064 },{ 0.0741 }},
	{{ 0.0199, 0.0209, 0.0184 },{ 0.1019 }},
	{{ 0.0296, 0.0306, 0.0257 },{ 0.1213 }},
	{{ 0.0627, 0.0651, 0.0548 },{ 0.1779 }},
	{{ 0.0831, 0.0863, 0.0718 },{ 0.2095 }},
	{{ 0.1091, 0.1134, 0.0946 },{ 0.2487 }},
	{{ 0.1442, 0.1497, 0.1227 },{ 0.2949 }},
	{{ 0.1745, 0.1814, 0.1495 },{ 0.3360 }},
	{{ 0.1747, 0.1816, 0.1498 },{ 0.3367 }},
	{{ 0.1747, 0.1816, 0.1496 },{ 0.3364 }},
	{{ 0.1748, 0.1816, 0.1497 },{ 0.3355 }},
	{{ 0.1749, 0.1817, 0.1497 },{ 0.3344 }},
	{{ 0.1748, 0.1817, 0.1498 },{ 0.3356 }},
	{{ 0.1748, 0.1817, 0.1498 },{ 0.3354 }},
	{{ 0.1749, 0.1817, 0.1496 },{ 0.3361 }},
	{{ 0.1749, 0.1818, 0.1498 },{ 0.3368 }},
	{{ 0.1749, 0.1818, 0.1498 },{ 0.3335 }},
	{{ 0.1750, 0.1818, 0.1499 },{ 0.3367 }},
	{{ 0.1750, 0.1819, 0.1500 },{ 0.3362 }},
	{{ 0.1750, 0.1819, 0.1498 },{ 0.3359 }},
	{{ 0.1751, 0.1820, 0.1500 },{ 0.3354 }},
	{{ 0.1752, 0.1821, 0.1501 },{ 0.3355 }},
	{{ 0.1754, 0.1823, 0.1502 },{ 0.3369 }},
	{{ 0.1756, 0.1824, 0.1504 },{ 0.3360 }},
	{{ 0.2743, 0.2842, 0.2367 },{ 0.4381 }},
	{{ 0.3289, 0.3411, 0.2834 },{ 0.4922 }},
	{{ 0.4036, 0.4184, 0.3475 },{ 0.5617 }},
	{{ 0.4689, 0.4854, 0.4020 },{ 0.6147 }},
	{{ 0.5379, 0.5567, 0.4606 },{ 0.6709 }},
	{{ 0.7137, 0.7420, 0.6169 },{ 0.8045 }},
	{{ 0.8730, 0.9105, 0.7433 },{ 0.9150 }},
	{{ 0.8738, 0.9113, 0.7435 },{ 0.9141 }},
	{{ 0.8741, 0.9116, 0.7445 },{ 0.9120 }},
	{{ 0.8744, 0.9118, 0.7443 },{ 0.9173 }},
	{{ 0.8748, 0.9123, 0.7457 },{ 0.9219 }},
	{{ 0.8748, 0.9123, 0.7450 },{ 0.9133 }},
	{{ 0.8748, 0.9124, 0.7445 },{ 0.9210 }},
	{{ 0.8751, 0.9127, 0.7462 },{ 0.9207 }},
	{{ 0.8751, 0.9127, 0.7457 },{ 0.9225 }},
	{{ 0.8754, 0.9130, 0.7454 },{ 0.9137 }},
	{{ 0.8757, 0.9133, 0.7456 },{ 0.9219 }},
	{{ 0.8759, 0.9135, 0.7470 },{ 0.9166 }},
	{{ 0.8761, 0.9137, 0.7469 },{ 0.9162 }},
	{{ 0.8759, 0.9137, 0.7469 },{ 0.9151 }},
	{{ 0.8765, 0.9141, 0.7470 },{ 0.9167 }},
};


#ifdef NEVER
#ifdef	__STDC__
#include <stdarg.h>
void error(char *fmt, ...), warning(char *fmt, ...), verbose(int level, char *fmt, ...);
#else
#include <varargs.h>
void error(), warning(), verbose();
#endif
#endif /* NEVER */

void write_rgb_tiff(char *name, int width, int height, unsigned char *data);

void usage(void) {
	fprintf(stderr,"Test 3D rspl interpolation\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: t2d [options]\n");
	fprintf(stderr," -t n                  Test set:\n");
	fprintf(stderr,"                     * 1 = 1D test set along x = y = 0.5\n");
	fprintf(stderr,"                       2 = Test set 1\n");
	fprintf(stderr,"                       3 = Test set 2\n");
	fprintf(stderr,"                       4 = x + y^2 + z^1/3 with nonmon point\n");
	fprintf(stderr,"                       5 = doubled up x + y^2 + z^1/3 with nonmon point\n");
	fprintf(stderr,"                       6 = neutral axis extrapolation\n");
	fprintf(stderr," -r resx,resy,resz  Set grid resolutions (def %d %d %d)\n",GRES0,GRES1,GRES2);
	fprintf(stderr," -h                    Test half scale resolution too\n");
	fprintf(stderr," -q                    Test quarter scale resolution too\n");
	fprintf(stderr," -x                    Use auto smoothing\n");
	fprintf(stderr," -s                    Test symetric smoothness\n");
	fprintf(stderr," -p                    plot 4 slices, xy = 0.5, yz = 0.5, xz = 0.5,  x=y=z\n");
	fprintf(stderr," -P x1:y1:z1:x2:y2:z2  plot slice from x1,y1,z1,x2,y2,z2\n");
	fprintf(stderr," -S factor             smoothing factor (default 1.0)\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	int fa,nfa;			/* argument we're looking at */
	rspl *rss;			/* Regularized spline structure */
	rspl *rss2 = NULL;	/* Regularized spline structure at half/quarter resolution */
	datai low,high;
	int gres[MXDI];
	int gres2[MXDI];
	double avgdev[MXDO];
	co *test_points = test_points1;
	int npoints = sizeof(test_points1)/sizeof(co);
	int dosym = 0;
	int autosm = 0;
	int doplot = 0;
	double plotpts[2][3];       /* doplot == 2 start/end points */
	int doh = 0;				/* half scale */
	int doq = 0;
	int rsv;
	double smoothf = 1.0;
	int flags = RSPL_NOFLAGS;

	low[0] = 0.0;
	low[1] = 0.0;
	low[2] = 0.0;
	high[0] = 1.0;
	high[1] = 1.0;
	high[2] = 1.0;
	gres[0] = GRES0;
	gres[1] = GRES1;
	gres[2] = GRES2;
	avgdev[0] = 0.0;
	avgdev[1] = 0.0;
	avgdev[2] = 0.0;

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

			/* test set */
			else if (argv[fa][1] == 't' || argv[fa][1] == 'T') {
				int ix;
				fa = nfa;
				if (na == NULL) usage();
				ix = atoi(na);
    			switch (ix) {
					case 1:
						test_points = test_points1;
						npoints = sizeof(test_points1)/sizeof(co);
						break;
					case 2:
						test_points = test_points2;
						npoints = sizeof(test_points2)/sizeof(co);
						break;
					case 3:
						test_points = test_points3;
						npoints = sizeof(test_points3)/sizeof(co);
						break;
					case 4:
						test_points = test_points4;
						npoints = sizeof(test_points4)/sizeof(co);
						break;
					case 5:
						test_points = test_points5;
						npoints = sizeof(test_points5)/sizeof(co);
						break;
					case 6:
						test_points = test_points6;
						npoints = sizeof(test_points6)/sizeof(co);
						break;
					default:
						usage();
				}

			} else if (argv[fa][1] == 'r' || argv[fa][1] == 'R') {
				fa = nfa;
				if (na == NULL) usage();
				if (sscanf(na, " %d,%d,%d ", &gres[0], &gres[1], &gres[2]) != 3)
					usage();

			} else if (argv[fa][1] == 'h' || argv[fa][1] == 'H') {
				doh = 1;

			} else if (argv[fa][1] == 'q' || argv[fa][1] == 'Q') {
				doh = 1;
				doq = 1;

			} else if (argv[fa][1] == 'p') {
				doplot = 1;

			} else if (argv[fa][1] == 'P') {
				doplot = 2;
				fa = nfa;
				if (na == NULL) usage();
				if (sscanf(na,"%lf:%lf:%lf:%lf:%lf:%lf",&plotpts[0][0],&plotpts[0][1],&plotpts[0][2],&plotpts[1][0],&plotpts[1][1],&plotpts[1][2]) != 6) {
					usage();
				}

			} else if (argv[fa][1] == 's') {
				dosym = 1;

			} else if (argv[fa][1] == 'x') {
				autosm = 1;

			/* smoothing factor */
			} else if (argv[fa][1] == 'S') {
				int ix;
				fa = nfa;
				if (na == NULL) usage();
				smoothf = atof(na);

			} else 
				usage();
		} else
			break;
	}


	if (autosm)
		flags |= RSPL_AUTOSMOOTH;

	if (dosym)
		flags |= RSPL_SYMDOMAIN;


	/* Create the object */
	rss =  new_rspl(RSPL_NOFLAGS, 3, 1);

	/* Fit to scattered data */
	rss->fit_rspl(rss,
	           flags,				/* Non-mon and clip flags */
	           test_points,			/* Test points */
	           npoints,				/* Number of test points */
	           low, high, gres,		/* Low, high, resolution of grid */
	           NULL, NULL,			/* Default data scale */
	           smoothf,				/* Smoothing */
	           avgdev,				/* Average deviation */
	           NULL);				/* iwidth */
	if (doh) {

		if (doq) {
			gres2[0] = gres[0]/4;
			gres2[1] = gres[1]/4;
			gres2[2] = gres[2]/4;
		} else {
			gres2[0] = gres[0]/2;
			gres2[1] = gres[1]/2;
			gres2[2] = gres[2]/2;
		}

		rss2 =  new_rspl(RSPL_NOFLAGS, 3, 1);

		/* Fit to scattered data */
		rss2->fit_rspl(rss2,
		           flags,				/* Non-mon and clip flags */
		           test_points,			/* Test points */
		           npoints,				/* Number of test points */
		           low, high, gres2,	/* Low, high, resolution of grid */
		           NULL, NULL,			/* Default data scale */
		           smoothf,				/* Smoothing */
		           avgdev,				/* Average deviation */
		           NULL);				/* iwidth */
	}

	/* Test the interpolation with a slice in 2D */
	for (rsv = 0; rsv <= doh; rsv++) {
		double z[2][2] = { { 0.1, 0.5 } , { 0.5, 0.9 } }; 
		double x1 = -0.2;
		double x2 = 1.2;
		double y1 = -0.2;
		double y2 = 1.2;
		double min = -0.0;
		double max = 1.0;
		rspl *rs;
		unsigned char pa[HEIGHT][WIDTH][3];
		co tco;	/* Test point */
		double sx,sy;
		int i,j,k;
	
		if (rsv == 0)
			rs = rss;
		else
			rs = rss2;

		sx = (x2 - x1)/(double)WIDTH;
		sy = (y2 - y1)/(double)HEIGHT;
		
		for (j=0; j < HEIGHT; j++) {
			double jj = j/(HEIGHT-1.0);
			tco.p[1] = (double)((HEIGHT-1) - j) * sy + y1;
			for (i = 0; i < WIDTH; i++) {
			double ii = j/(HEIGHT-1.0);
				tco.p[0] = (double)i * sx + x1;

				tco.p[2] = (1.0-ii) * (1.0-jj) * z[0][0]
				         + (1.0-ii) *      jj  * z[0][1]
				         +      ii  * (1.0-jj) * z[1][0]
				         +      ii  *      jj  * z[1][1];

				if (rs->INTERP(rs, &tco)) {
					pa[j][i][0] = 0;	/* Out of bounds in green */
					pa[j][i][1] = 100;
					pa[j][i][2] = 0;
				} else {
					int m;
	/* printf("%d %d, %f %f returned %f\n",i,j,tco.p[0],tco.p[1],tco.v[0]); */
					m = (int)((255.0 * (tco.v[0] - min)/(max - min)) + 0.5);
					if (m < 0) {
						pa[j][i][0] = 20;		/* Dark blue */
						pa[j][i][1] = 20;
						pa[j][i][2] = 50;
					} else if (m > 255) {
						pa[j][i][0] = 230;		/* Light blue */
						pa[j][i][1] = 230;
						pa[j][i][2] = 255;
					} else {
						pa[j][i][0] = m;	/* Level in grey */
						pa[j][i][1] = m;
						pa[j][i][2] = m;
					}
				}
			}
		}
	
		/* Mark verticies in red */
		for(k = 0; k < npoints; k++) {
			j = (int)((HEIGHT * (y2 - test_points[k].p[1])/(y2 - y1)) + 0.5);
			i = (int)((WIDTH * (test_points[k].p[0] - x1)/(x2 - x1)) + 0.5);
			pa[j][i][0] = 255;
			pa[j][i][1] = 0;
			pa[j][i][2] = 0;
		}
		write_rgb_tiff(rsv == 0 ? "t3d.tif" : "t3dh.tif" ,WIDTH,HEIGHT,(unsigned char *)pa);
	}

	/* Plot out 4 slices */
	if (doplot == 1) {
		int slice;
		
		for (slice = 0; slice < 4; slice++) {
			co tp;	/* Test point */
			double x[PLOTRES];
			double ya[PLOTRES];
			double yb[PLOTRES];
			double xx,yy,zz;
			double x1,x2,y1,y2,z1,z2;
			double sx,sy,sz;
			int i,n;
			
			/* Set up slice to plot */
			if (slice == 0) {
				x1 = 0.5; y1 = 0.5, z1 = 0.0;
				x2 = 0.5; y2 = 0.5, z2 = 1.0;
				printf("Plot along z at x = y = 0.5\n");
				n = PLOTRES;
			} else if (slice == 1) {
				x1 = 0.0; y1 = 0.5, z1 = 0.5;
				x2 = 1.0; y2 = 0.5, z2 = 0.5;
				printf("Plot along x at y = z = 0.5\n");
				n = PLOTRES;
			} else if (slice == 2) {
				x1 = 0.5; y1 = 0.0, z1 = 0.5;
				x2 = 0.5; y2 = 1.0, z2 = 0.5;
				printf("Plot along y at x = z = 0.5\n");
				n = PLOTRES;
			} else {
				x1 = 0.0; y1 = 0.0, z1 = 0.0;
				x2 = 1.0; y2 = 1.0, z2 = 1.0;
				printf("Plot along x = y = z\n");
				n = PLOTRES;
			}

			sx = (x2 - x1)/n;
			sy = (y2 - y1)/n;
			sz = (z2 - z1)/n;
			
			xx = x1;
			yy = y1;
			zz = z1;
			for (i = 0; i < n; i++) {
				double vv = i/(n-1.0);
				x[i] = vv;
				tp.p[0] = xx;
				tp.p[1] = yy;
				tp.p[2] = zz;

				if (rss->INTERP(rss, &tp))
					tp.v[0] = -0.1;
				ya[i] = tp.v[0];

				if (doh) {
					if (rss2->INTERP(rss2, &tp))
						tp.v[0] = -0.1;
					yb[i] = tp.v[0];
				}

				xx += sx;
				yy += sy;
				zz += sz;
			}

			/* Plot the result */
			if (doh)
				do_plot(x,ya,yb,NULL,n);
			else
				do_plot(x,ya,NULL,NULL,n);
		}
	} else if (doplot == 2) {
		co tp;	/* Test point */
		double x[PLOTRES];
		double ya[PLOTRES];
		double yb[PLOTRES];
		double xx,yy,zz;
		double x1,x2,y1,y2,z1,z2;
		double sx,sy,sz;
		int i,n;
		

		x1 = plotpts[0][0];
		y1 = plotpts[0][1];
		z1 = plotpts[0][2];
		x2 = plotpts[1][0];
		y2 = plotpts[1][1];
		z2 = plotpts[1][2];

		printf("Plot along z at x = y = 0.5\n");
		n = PLOTRES;

		sx = (x2 - x1)/n;
		sy = (y2 - y1)/n;
		sz = (z2 - z1)/n;
		
		xx = x1;
		yy = y1;
		zz = z1;
		for (i = 0; i < n; i++) {
			double vv = i/(n-1.0);
			x[i] = vv;
			tp.p[0] = xx;
			tp.p[1] = yy;
			tp.p[2] = zz;

			if (rss->INTERP(rss, &tp))
				tp.v[0] = -0.1;
			ya[i] = tp.v[0];

			if (doh) {
				if (rss2->INTERP(rss2, &tp))
					tp.v[0] = -0.1;
				yb[i] = tp.v[0];
			}

			xx += sx;
			yy += sy;
			zz += sz;
		}

		/* Plot the result */
		if (doh)
			do_plot(x,ya,yb,NULL,n);
		else
			do_plot(x,ya,NULL,NULL,n);
	}

	/* Report the fit */
	{
		co tco;	/* Test point */
		int k;
		double avg = 0;
		double max = 0.0;
	
		for(k = 0; k < npoints; k++) {
			double err;
			tco.p[0] = test_points[k].p[0];
			tco.p[1] = test_points[k].p[1];
			tco.p[2] = test_points[k].p[2];
			rss->INTERP(rss, &tco);

			err = tco.v[0] - test_points[k].v[0];
			err = fabs(err);

			avg += err;
			if (err > max)
				max = err;
		}
		avg /= (double)npoints;
		printf("Max error %f%%, average %f%%\n",100.0 * max, 100.0 * avg);
	}
	return 0;
}

/* ---------------------- */
/* Tiff diagnostic output */

void
write_rgb_tiff(
char *name,
int width,
int height,
unsigned char *data
) {
	int y;
	unsigned char *dp;
	TIFF *tif;

	if ((tif = TIFFOpen(name, "w")) == NULL) {
		fprintf(stderr,"Failed to open output TIFF file '%s'\n",name);
		exit (-1);
		}

	TIFFSetField(tif, TIFFTAG_IMAGEWIDTH,  width);
	TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
	TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
	TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);

	for (dp = data, y = 0; y < height; y++, dp += 3 * width) {
		if (TIFFWriteScanline(tif, (tdata_t)dp, y, 0) < 0) {
			fprintf(stderr,"WriteScanline Failed at line %d\n",y);
			exit (-1);
		}
	}
	(void) TIFFClose(tif);
}

#ifdef NEVER
/******************************************************************/
/* Error/debug output routines */
/******************************************************************/

/* Basic printf type error() and warning() routines */

#ifdef	__STDC__
void
error(char *fmt, ...)
#else
void
error(va_alist) 
va_dcl
#endif
{
	va_list args;
#ifndef	__STDC__
	char *fmt;
#endif

	fprintf(stderr,"cmatch: Error - ");
#ifdef	__STDC__
	va_start(args, fmt);
#else
	va_start(args);
	fmt = va_arg(args, char *);
#endif
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	fflush(stdout);
	exit (-1);
}

#ifdef	__STDC__
void
warning(char *fmt, ...)
#else
void
warning(va_alist) 
va_dcl
#endif
{
	va_list args;
#ifndef	__STDC__
	char *fmt;
#endif

	fprintf(stderr,"cmatch: Warning - ");
#ifdef	__STDC__
	va_start(args, fmt);
#else
	va_start(args);
	fmt = va_arg(args, char *);
#endif
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

#ifdef	__STDC__
void
verbose(int level, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
#else
verbose(va_alist) 
va_dcl
{
	va_list args;
	int   level;
	char *fmt;
	va_start(args);
	level = va_arg(args, int);
	fmt = va_arg(args, char *);
#endif
	if (verbose_level >= level)
		{
		fprintf(verbose_out,"cmatch: ");
		vfprintf(verbose_out, fmt, args);
		fprintf(verbose_out, "\n");
		fflush(verbose_out);
		}
	va_end(args);
}
#endif /* NEVER */
