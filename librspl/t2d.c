/************************************************/
/* Test RSPL in 2D */
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
#define GRES0 25			/* Default resolutions */
#define GRES1 25
#undef NEVER
#define ALWAYS

//double t1xa[PNTS] = { 0.2, 0.25, 0.30, 0.35,  0.40,  0.44, 0.48, 0.51,  0.64,  0.75  };
//double t1ya[PNTS] = { 0.3, 0.35, 0.4,  0.41,  0.42,  0.46, 0.5,  0.575, 0.48,  0.75  };

/* 1D test function repeated 3 times along x = 0.5 */
co test_points1[] = {
	{{ 0.4,0.20 },{ 0.30  }},	/* 0 */
	{{ 0.4,0.25 },{ 0.35  }},	/* 1 */
	{{ 0.4,0.30 },{ 0.40  }},	/* 2 */
	{{ 0.4,0.35 },{ 0.41  }},	/* 3 */
	{{ 0.4,0.40 },{ 0.42  }},	/* 4 */
	{{ 0.4,0.44 },{ 0.46  }},	/* 5 */
	{{ 0.4,0.48 },{ 0.50  }},	/* 6 */
	{{ 0.4,0.51 },{ 0.575 }},	/* 7 */
	{{ 0.4,0.64 },{ 0.48  }},	/* 8 */
	{{ 0.4,0.75 },{ 0.75  }},	/* 9 */

	{{ 0.5,0.20 },{ 0.30  }},	/* 0 */
	{{ 0.5,0.25 },{ 0.35  }},	/* 1 */
	{{ 0.5,0.30 },{ 0.40  }},	/* 2 */
	{{ 0.5,0.35 },{ 0.41  }},	/* 3 */
	{{ 0.5,0.40 },{ 0.42  }},	/* 4 */
	{{ 0.5,0.44 },{ 0.46  }},	/* 5 */
	{{ 0.5,0.48 },{ 0.50  }},	/* 6 */
	{{ 0.5,0.51 },{ 0.575 }},	/* 7 */
	{{ 0.5,0.64 },{ 0.48  }},	/* 8 */
	{{ 0.5,0.75 },{ 0.75  }},	/* 9 */

	{{ 0.6,0.20 },{ 0.30  }},	/* 0 */
	{{ 0.6,0.25 },{ 0.35  }},	/* 1 */
	{{ 0.6,0.30 },{ 0.40  }},	/* 2 */
	{{ 0.6,0.35 },{ 0.41  }},	/* 3 */
	{{ 0.6,0.40 },{ 0.42  }},	/* 4 */
	{{ 0.6,0.44 },{ 0.46  }},	/* 5 */
	{{ 0.6,0.48 },{ 0.50  }},	/* 6 */
	{{ 0.6,0.51 },{ 0.575 }},	/* 7 */
	{{ 0.6,0.64 },{ 0.48  }},	/* 8 */
	{{ 0.6,0.75 },{ 0.75  }}	/* 9 */
};

/* x + y^2 function with one non-monotonic point */
co test_points2[] = {
	{{ 0.1,0.1,0.5,0.0 },{ 0.11 }},	/* 0 */
	{{ 0.2,0.7,0.1,0.3 },{ 0.69 }},	/* 1 */
	{{ 0.8,0.8,0.8,0.2 },{ 1.44 }}, /* 2 */
	{{ 0.5,0.6,0.4,0.9 },{ 0.86 }},	/* 3 */
	{{ 0.2,0.5,0.2,0.7 },{ 0.45 }},	/* 4 */
//	{{ 0.3,0.7,0.2,0.8 },{ 0.35 }},	/* nm 5 */
	{{ 0.5,0.5,0.2,0.8 },{ 0.30 }},	/* nm 5 */
	{{ 0.5,0.4,0.9,0.3 },{ 0.66 }},	/* 6 */
	{{ 0.1,0.9,0.7,0.4 },{ 0.91 }},	/* 7 */
	{{ 0.7,0.2,0.1,0.3 },{ 0.74 }},	/* 8 */
	{{ 0.8,0.4,0.3,0.7 },{ 0.96 }},	/* 9 */
	{{ 0.3,0.3,0.4,0.1 },{ 0.39 }}	/* 10 */
	};

/* x + y^2 function */
co test_points3[] = {
	{{ 0.1,0.1,0.5,0.0 },{ 0.11 }},	/* 0 */
	{{ 0.2,0.7,0.1,0.3 },{ 0.69 }},	/* 1 */
	{{ 0.8,0.8,0.8,0.2 },{ 1.44 }}, /* 2 */
	{{ 0.5,0.6,0.4,0.9 },{ 0.86 }},	/* 3 */
	{{ 0.2,0.5,0.2,0.7 },{ 0.45 }},	/* 4 */
	{{ 0.3,0.7,0.2,0.8 },{ 0.79 }},	/* 5 */
	{{ 0.5,0.4,0.9,0.3 },{ 0.66 }},	/* 6 */
	{{ 0.1,0.9,0.7,0.4 },{ 0.91 }},	/* 7 */
	{{ 0.7,0.2,0.1,0.3 },{ 0.74 }},	/* 8 */
	{{ 0.8,0.4,0.3,0.7 },{ 0.96 }},	/* 9 */
	{{ 0.3,0.3,0.4,0.1 },{ 0.39 }}	/* 10 */
	};

/* Arbitrary values */
co test_points4[] = {
	{{ 0.1,0.1,0.5,0.0 },{ 0.0 }},	/* 0 */
	{{ 0.2,0.7,0.1,0.3 },{ 0.1 }},	/* 1 */
	{{ 0.8,0.8,0.8,0.2 },{ 0.2 }}, 	/* 2 */
	{{ 0.5,0.6,0.4,0.9 },{ 0.3 }},	/* 3 */
	{{ 0.2,0.5,0.2,0.7 },{ 0.4 }},	/* 4 */
	{{ 0.3,0.7,0.2,0.8 },{ 0.5 }},	/* 5 */
	{{ 0.5,0.4,0.9,0.3 },{ 0.6 }},	/* 6 */
	{{ 0.1,0.9,0.7,0.4 },{ 0.7 }},	/* 7 */
	{{ 0.7,0.2,0.1,0.3 },{ 0.8 }},	/* 8 */
	{{ 0.8,0.4,0.3,0.7 },{ 0.9 }},	/* 9 */
	{{ 0.3,0.3,0.4,0.1 },{ 1.0 }}	/* 10 */
	};

/* single dimension line */
co test_points5[] = {
	{{ 0.4,0.2 },{ 0.10 }},	/* 0 */
	{{ 0.5,0.4 },{ 0.50 }},	/* 1 */
	{{ 0.6,0.6 },{ 0.90 }}	/* 2 */
	};

/* Single value, many points */
co test_points6[] = {
	{{ 0.5,0.6  },{ 0.4 }},	/* 0 */
	{{ 0.4,0.4  },{ 0.4 }},	/* 1 */
	{{ 0.6,0.4  },{ 0.4 }},	/* 2 */
	{{ 0.5,0.8  },{ 0.4 }},	/* 3 */
	{{ 0.8,0.6  },{ 0.4 }},	/* 4 */
	{{ 0.8,0.3  },{ 0.4 }},	/* 5 */
	{{ 0.5,0.15 },{ 0.4 }},	/* 6 */
	{{ 0.2,0.3  },{ 0.4 }},	/* 7 */
	{{ 0.2,0.6  },{ 0.4 }}	/* 8 */
	};

/* single value triangle */
co test_points7[] = {
	{{ 0.2,0.5  },{ 0.4 }},	/* 0 */
	{{ 0.6,0.2  },{ 0.4 }},	/* 1 */
	{{ 0.6,0.8  },{ 0.4 }}	/* 2 */
	};

/* 3dap C+M L* values */
co test_points8[] = {
	{{ 0.0,     0.0 }, { 0.96433 }},
	{{ 1.0,     0.0 }, { 0.54532 }},
	{{ 0.0,     1.0 }, { 0.13844 }},
	{{ 1.0,     1.0 }, { 0.10533 }},
	{{ 0.52015, 0.51226 }, { 0.44379 }},
	{{ 0.016882, 0.4899 }, { 0.55744 }},
	{{ 0.48229, 0.014288 }, { 0.71457 }},
	{{ 0.32501, 0.99594 }, { 0.14023 }},
	{{ 0.9429, 0.39117 }, { 0.40529 }},
	{{ 0.25639, 0.25624 }, { 0.65594 }},
	{{ 0.80601, 0.62945 }, { 0.32338 }},
	{{ 0.74113, 0.20699 }, { 0.528 }},
	{{ 0.21766, 0.72345 }, { 0.37442 }},
	{{ 0.64679, 0.94379 }, { 0.16175 }},
	{{ 0.24217, 0.012757 }, { 0.81892 }},
	{{ 0.013911, 0.25268 }, { 0.72574 }},
	{{ 0.54723, 0.74872 }, { 0.30302 }},
	{{ 0.016488, 0.77535 }, { 0.33234 }},
	{{ 0.82135, 0.044133 }, { 0.57191 }},
	{{ 0.52377, 0.2403 }, { 0.58315 }},
	{{ 0.27798, 0.51935 }, { 0.49399 }},
	{{ 0.1186, 0.93321 }, { 0.19912 }},
	{{ 0.95827, 0.18481 }, { 0.48037 }},
	{{ 0.7714, 0.48664 }, { 0.39652 }},
	{{ 0.94209, 0.7863 }, { 0.22555 }},
	{{ 0.9429, 0.58967 }, { 0.32235 }},
	{{ 0.12251, 0.12897 }, { 0.78743 }},
	{{ 0.14563, 0.38536 }, { 0.60355 }},
	{{ 0.41616, 0.65055 }, { 0.3876 }},
	{{ 0.38091, 0.14221 }, { 0.68198 }},
	{{ 0.388, 0.38098 }, { 0.54479 }},
	{{ 0.74149, 0.77517 }, { 0.25597 }},
	{{ 0.11267, 0.62485 }, { 0.45976 }},
	{{ 0.62755, 0.37228 }, { 0.48943 }},
	{{ 0.6502, 0.59695 }, { 0.37031 }},
	{{ 0.32033, 0.81669 }, { 0.28831 }},
	{{ 0.664, 0.096944 }, { 0.60575 }},
	{{ 0.82772, 0.33242 }, { 0.45496 }},
	{{ 0.49121, 0.89468 }, { 0.20858 }},
	{{ 0.84729, 0.94866 }, { 0.1453 }}
};

/* 3dap C+M a* values */
co test_points9[] = {
	{{ 0.0,     0.0 }, { 0.504266 }},
	{{ 1.0,     0.0 }, { 0.14962 }},
	{{ 0.0,     1.0 }, { 0.538792 }},
	{{ 1.0,     1.0 }, { 0.481662 }},
	{{ 0.52015, 0.51226 }, { 0.39446 }},
	{{ 0.016882, 0.4899 }, { 0.5038104 }},
	{{ 0.48229, 0.014288 }, { 0.32387 }},
	{{ 0.32501, 0.99594 }, { 0.5028343 }},
	{{ 0.9429, 0.39117 }, { 0.24127 }},
	{{ 0.25639, 0.25624 }, { 0.43018 }},
	{{ 0.80601, 0.62945 }, { 0.34684 }},
	{{ 0.74113, 0.20699 }, { 0.274 }},
	{{ 0.21766, 0.72345 }, { 0.480491 }},
	{{ 0.64679, 0.94379 }, { 0.472241 }},
	{{ 0.24217, 0.012757 }, { 0.39938 }},
	{{ 0.013911, 0.25268 }, { 0.5035042 }},
	{{ 0.54723, 0.74872 }, { 0.433005 }},
	{{ 0.016488, 0.77535 }, { 0.513262 }},
	{{ 0.82135, 0.044133 }, { 0.21553 }},
	{{ 0.52377, 0.2403 }, { 0.35082 }},
	{{ 0.27798, 0.51935 }, { 0.451019 }},
	{{ 0.1186, 0.93321 }, { 0.513378 }},
	{{ 0.95827, 0.18481 }, { 0.19309 }},
	{{ 0.7714, 0.48664 }, { 0.32019 }},
	{{ 0.94209, 0.7863 }, { 0.37573 }},
	{{ 0.9429, 0.58967 }, { 0.29975 }},
	{{ 0.12251, 0.12897 }, { 0.460094 }},
	{{ 0.14563, 0.38536 }, { 0.471575 }},
	{{ 0.41616, 0.65055 }, { 0.439078 }},
	{{ 0.38091, 0.14221 }, { 0.37965 }},
	{{ 0.388, 0.38098 }, { 0.409296 }},
	{{ 0.74149, 0.77517 }, { 0.403494 }},
	{{ 0.11267, 0.62485 }, { 0.4905027 }},
	{{ 0.62755, 0.37228 }, { 0.34157 }},
	{{ 0.6502, 0.59695 }, { 0.37896 }},
	{{ 0.32033, 0.81669 }, { 0.477444 }},
	{{ 0.664, 0.096944 }, { 0.28 }},
	{{ 0.82772, 0.33242 }, {0.26821 }},
	{{ 0.49121, 0.89468 }, { 0.47241 }},
	{{ 0.84729, 0.94866 }, { 0.459256 }}
};

/* 3dap C+M b* values */
co test_points10[] = {
	{{ 0.0,     0.0 }, { 0.4918362 }},
	{{ 1.0,     0.0 }, { 0.0 }},
	{{ 0.0,     1.0 }, { 0.457067 }},
	{{ 1.0,     1.0 }, { 0.407021 }},
	{{ 0.52015, 0.51226 }, { 0.34029 }},
	{{ 0.016882, 0.4899 }, { 0.48672 }},
	{{ 0.48229, 0.014288 }, { 0.19406 }},
	{{ 0.32501, 0.99594 }, { 0.453116 }},
	{{ 0.9429, 0.39117 }, { 0.1766 }},
	{{ 0.25639, 0.25624 }, { 0.37533 }},
	{{ 0.80601, 0.62945 }, { 0.29851 }},
	{{ 0.74113, 0.20699 }, { 0.16808 }},
	{{ 0.21766, 0.72345 }, { 0.448783 }},
	{{ 0.64679, 0.94379 }, { 0.415526 }},
	{{ 0.24217, 0.012757 }, { 0.32194 }},
	{{ 0.013911, 0.25268 }, { 0.486404 }},
	{{ 0.54723, 0.74872 }, { 0.38695 }},
	{{ 0.016488, 0.77535 }, { 0.484031 }},
	{{ 0.82135, 0.044133 }, { 0.07184 }},
	{{ 0.52377, 0.2403 }, { 0.26192 }},
	{{ 0.27798, 0.51935 }, { 0.412787 }},
	{{ 0.1186, 0.93321 }, { 0.463822 }},
	{{ 0.95827, 0.18481 }, { 0.09177 }},
	{{ 0.7714, 0.48664 }, { 0.26049 }},
	{{ 0.94209, 0.7863 }, { 0.3249 }},
	{{ 0.9429, 0.58967 }, { 0.25115 }},
	{{ 0.12251, 0.12897 }, { 0.419966 }},
	{{ 0.14563, 0.38536 }, { 0.436771 }},
	{{ 0.41616, 0.65055 }, { 0.39638 }},
	{{ 0.38091, 0.14221 }, { 0.29133 }},
	{{ 0.388, 0.38098 }, { 0.35442 }},
	{{ 0.74149, 0.77517 }, { 0.35501 }},
	{{ 0.11267, 0.62485 }, { 0.466548 }},
	{{ 0.62755, 0.37228 }, { 0.2675 }},
	{{ 0.6502, 0.59695 }, { 0.32833 }},
	{{ 0.32033, 0.81669 }, { 0.438066 }},
	{{ 0.664, 0.096944 }, { 0.15598 }},
	{{ 0.82772, 0.33242 }, { 0.18834 }},
	{{ 0.49121, 0.89468 }, { 0.423113 }},
	{{ 0.84729, 0.94866 }, { 0.3979 }}
};

/* Values at edges test */
double test_f11(double x, double y) {		/* Function that computes values */
	double val = 0.0;
	
	val = (x * x - 0.5) * 0.6
	    + (y * y - 0.5) * 0.4
	    + 0.5;

	return val;
}
co test_points11[] = {
	{{ 0.0,     0.0 }},
	{{ 0.0,     0.25 }},
	{{ 0.0,     0.5 }},
	{{ 0.0,     0.75 }},
	{{ 0.0,     1.0 }},

	{{ 0.25,     0.0 }},
	{{ 0.25,     0.25 }},
	{{ 0.25,     0.5 }},
	{{ 0.25,     0.75 }},
	{{ 0.25,     1.0 }},

	{{ 0.5,     0.0 }},
	{{ 0.5,     0.25 }},
	{{ 0.5,     0.5 }},
	{{ 0.5,     0.75 }},
	{{ 0.5,     1.0 }},

	{{ 0.75,     0.0 }},
	{{ 0.75,     0.25 }},
	{{ 0.75,     0.5 }},
	{{ 0.75,     0.75 }},
	{{ 0.75,     1.0 }},

	{{ 1.0,     0.0 }},
	{{ 1.0,     0.25 }},
	{{ 1.0,     0.5 }},
	{{ 1.0,     0.75 }},
	{{ 1.0,     1.0 }},


	{{ 0.2,     0.7 }},
	{{ 0.2,     0.8 }},
	{{ 0.2,     0.9 }},
	{{ 0.2,     1.0 }},

	{{ 0.1,     0.7 }},
	{{ 0.1,     0.8 }},
	{{ 0.1,     0.9 }},
	{{ 0.1,     1.0 }},

	{{ 0.0,     0.7 }},
	{{ 0.0,     0.8 }},
	{{ 0.0,     0.9 }}
};

/* Points consistent with a matrix interpolation */
co test_points12[] = {
	{{ 0.1,     0.1 }, { 0.1 }},
	{{ 0.5,     0.1 }, { 0.5 }},
	{{ 0.9,     0.1 }, { 0.9 }},
	{{ 0.1,     0.5 }, { 0.1 }},
	{{ 0.5,     0.5 }, { 0.35 }},
	{{ 0.9,     0.5 }, { 0.6 }},
	{{ 0.1,     0.9 }, { 0.1 }},
	{{ 0.5,     0.9 }, { 0.2 }},
	{{ 0.9,     0.9 }, { 0.3 }}
};

/* Points down the "neutral axis" extrapolation test */
co test_points13[] = {
	{{ 0.0069, 0.0071 }, { 0.0726 }},
	{{ 0.0068, 0.0071 }, { 0.0704 }},
	{{ 0.0069, 0.0072 }, { 0.0720 }},
	{{ 0.0069, 0.0072 }, { 0.0734 }},
	{{ 0.0069, 0.0072 }, { 0.0750 }},
	{{ 0.0070, 0.0072 }, { 0.0779 }},
	{{ 0.0070, 0.0072 }, { 0.0741 }},
	{{ 0.0069, 0.0072 }, { 0.0745 }},
	{{ 0.0069, 0.0072 }, { 0.0747 }},
	{{ 0.0071, 0.0073 }, { 0.0760 }},
	{{ 0.0070, 0.0073 }, { 0.0751 }},
	{{ 0.0070, 0.0073 }, { 0.0759 }},
	{{ 0.0071, 0.0074 }, { 0.0693 }},
	{{ 0.0071, 0.0074 }, { 0.0740 }},
	{{ 0.0072, 0.0075 }, { 0.0741 }},
	{{ 0.0199, 0.0209 }, { 0.1019 }},
	{{ 0.0296, 0.0306 }, { 0.1213 }},
	{{ 0.0627, 0.0651 }, { 0.1779 }},
	{{ 0.0831, 0.0863 }, { 0.2095 }},
	{{ 0.1091, 0.1134 }, { 0.2487 }},
	{{ 0.1442, 0.1497 }, { 0.2949 }},
	{{ 0.1745, 0.1814 }, { 0.3360 }},
	{{ 0.1747, 0.1816 }, { 0.3367 }},
	{{ 0.1747, 0.1816 }, { 0.3364 }},
	{{ 0.1748, 0.1816 }, { 0.3355 }},
	{{ 0.1749, 0.1817 }, { 0.3344 }},
	{{ 0.1748, 0.1817 }, { 0.3356 }},
	{{ 0.1748, 0.1817 }, { 0.3354 }},
	{{ 0.1749, 0.1817 }, { 0.3361 }},
	{{ 0.1749, 0.1818 }, { 0.3368 }},
	{{ 0.1749, 0.1818 }, { 0.3335 }},
	{{ 0.1750, 0.1818 }, { 0.3367 }},
	{{ 0.1750, 0.1819 }, { 0.3362 }},
	{{ 0.1750, 0.1819 }, { 0.3359 }},
	{{ 0.1751, 0.1820 }, { 0.3354 }},
	{{ 0.1752, 0.1821 }, { 0.3355 }},
	{{ 0.1754, 0.1823 }, { 0.3369 }},
	{{ 0.1756, 0.1824 }, { 0.3360 }},
	{{ 0.2743, 0.2842 }, { 0.4381 }},
	{{ 0.3289, 0.3411 }, { 0.4922 }},
	{{ 0.4036, 0.4184 }, { 0.5617 }},
	{{ 0.4689, 0.4854 }, { 0.6147 }},
	{{ 0.5379, 0.5567 }, { 0.6709 }},
	{{ 0.7137, 0.7420 }, { 0.8045 }},
	{{ 0.8730, 0.9105 }, { 0.9150 }},
	{{ 0.8738, 0.9113 }, { 0.9141 }},
	{{ 0.8741, 0.9116 }, { 0.9120 }},
	{{ 0.8744, 0.9118 }, { 0.9173 }},
	{{ 0.8748, 0.9123 }, { 0.9219 }},
	{{ 0.8748, 0.9123 }, { 0.9133 }},
	{{ 0.8748, 0.9124 }, { 0.9210 }},
	{{ 0.8751, 0.9127 }, { 0.9207 }},
	{{ 0.8751, 0.9127 }, { 0.9225 }},
	{{ 0.8754, 0.9130 }, { 0.9137 }},
	{{ 0.8757, 0.9133 }, { 0.9219 }},
	{{ 0.8759, 0.9135 }, { 0.9166 }},
	{{ 0.8761, 0.9137 }, { 0.9162 }},
	{{ 0.8759, 0.9137 }, { 0.9151 }},
	{{ 0.8765, 0.9141 }, { 0.9167 }}
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
	fprintf(stderr,"Test 2D rspl interpolation\n");
	fprintf(stderr,"Author: Graeme W. Gill\n");
	fprintf(stderr,"usage: t2d [options]\n");
	fprintf(stderr," -t n            Test set:\n");
	fprintf(stderr,"             *   1 = 1D curve along x = 0.5\n");
	fprintf(stderr,"                 2 = x + y^2 with nonmon point\n");
	fprintf(stderr,"                 3 = x + y^2\n");
	fprintf(stderr,"                 4 = arbitrary11\n");
	fprintf(stderr,"                 5 = 1D line of 3 points\n");
	fprintf(stderr,"                 6 = same value 11 points\n");
	fprintf(stderr,"                 7 = same value 3 points\n");
	fprintf(stderr,"                 8 = C + M printer L* values\n");
	fprintf(stderr,"                 9 = C + M printer a* values\n");
	fprintf(stderr,"                 10 = C + M printer b* values\n");
	fprintf(stderr,"                 11 = Points up to edge test\n");
	fprintf(stderr,"                 12 = Four points with high smoothing\n");
	fprintf(stderr,"                 13 = Neutral axis extrapolation\n");
	fprintf(stderr," -r resx,resy    Set grid resolutions (def %d %d)\n",GRES0,GRES1);
	fprintf(stderr," -h              Test half scale resolution too\n");
	fprintf(stderr," -q              Test quarter scale resolution too\n");
	fprintf(stderr," -x              Use auto smoothing\n");
	fprintf(stderr," -s              Test symetric smoothness (set asymetric -r !)\n");
	fprintf(stderr," -S              Test spline interpolation\n");
	fprintf(stderr," -p              plot 3 slices, x = 0.5, y = 0.5, x = y\n");
	fprintf(stderr," -P x1:y1:x2:y2  plot a slice from x1,y1 to x2,y2\n");
	fprintf(stderr," -m              No red point markers in TIFF\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	int fa,nfa;				/* argument we're looking at */
	rspl *rss;			/* Regularized spline structure */
	rspl *rss2 = NULL;	/* Regularized spline structure at half resolution */
	datai low,high;
	int gres[MXDI];
	int gres2[MXDI];
	double avgdev[MXDO];
	co *test_points = test_points1;
	int npoints = sizeof(test_points1)/sizeof(co);
	int dospline = 0;
	int autosm = 0;
	int dosym = 0;
	int doplot = 0;
	double plotpts[2][2];		/* doplot == 2 start/end points */
	int doh = 0;				/* half scale */
	int doq = 0;
	int rsv;
	int flags = RSPL_NOFLAGS;
	int markers = 1;
	double smooth = 1.0;

	low[0] = 0.0;
	low[1] = 0.0;
	high[0] = 1.0;
	high[1] = 1.0;
	gres[0] = GRES0;
	gres[1] = GRES1;
	avgdev[0] = 0.0;
	avgdev[1] = 0.0;

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
					case 7:
						test_points = test_points7;
						npoints = sizeof(test_points7)/sizeof(co);
						break;
					case 8:
						test_points = test_points8;
						npoints = sizeof(test_points8)/sizeof(co);
						break;
					case 9:
						test_points = test_points9;
						npoints = sizeof(test_points9)/sizeof(co);
						break;
					case 10:
						test_points = test_points10;
						npoints = sizeof(test_points10)/sizeof(co);
						break;
					case 11: {
						int i;
						test_points = test_points11;
						npoints = sizeof(test_points11)/sizeof(co);
						for (i = 0; i < npoints; i++) {
							test_points[i].v[0] = test_f11(test_points[i].p[0],test_points[i].p[1]);
						}
						break;
					case 12:
						test_points = test_points12;
						npoints = sizeof(test_points12)/sizeof(co);
//						smooth = -100000.0;
//						smooth = 1e-4;
						avgdev[0] = 0.1;
						avgdev[1] = 0.1;
						break;
					case 13:
						test_points = test_points13;
						npoints = sizeof(test_points13)/sizeof(co);
						break;
					}
					default:
						usage();
				}

			} else if (argv[fa][1] == 'r' || argv[fa][1] == 'R') {
				fa = nfa;
				if (na == NULL) usage();
				if (sscanf(na, " %d,%d ", &gres[0], &gres[1]) != 2)
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
				if (sscanf(na,"%lf:%lf:%lf:%lf",&plotpts[0][0],&plotpts[0][1],&plotpts[1][0],&plotpts[1][1]) != 4) {
					usage();
				}

			} else if (argv[fa][1] == 'S') {
				dospline = 1;

			} else if (argv[fa][1] == 'x') {
				autosm = 1;

			} else if (argv[fa][1] == 's') {
				dosym = 1;

			} else if (argv[fa][1] == 'm') {
				markers = 0;

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
	rss =  new_rspl(RSPL_NOFLAGS, 2, 1);

	/* Fit to scattered data */
	rss->fit_rspl(rss,
	           flags,				/* Non-mon and clip flags */
	           test_points,			/* Test points */
	           npoints,				/* Number of test points */
	           low, high, gres,		/* Low, high, resolution of grid */
	           NULL, NULL,			/* Default data scale */
	           smooth,				/* Smoothing */
	           avgdev,				/* Average deviation */
	           NULL);				/* iwidth */
	if (doh) {

		if (doq) {
			gres2[0] = gres[0]/4;
			gres2[1] = gres[1]/4;
		} else {
			gres2[0] = gres[0]/2;
			gres2[1] = gres[1]/2;
		}

		rss2 =  new_rspl(RSPL_NOFLAGS, 2, 1);

		/* Fit to scattered data */
		rss2->fit_rspl(rss2,
		           flags,				/* Non-mon and clip flags */
		           test_points,			/* Test points */
		           npoints,				/* Number of test points */
		           low, high, gres2,	/* Low, high, resolution of grid */
		           NULL, NULL,			/* Default data scale */
		           1.0,					/* Smoothing */
		           avgdev,				/* Average deviation */
		           NULL);				/* iwidth */
	}

	/* Plot the interpolation in 2D */
	for (rsv = 0; rsv <= doh; rsv++) {
		double x1 = -0.2;		/* Plot range */
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
			tco.p[1] = (double)((HEIGHT-1) - j) * sy + y1;
			for (i=0; i < WIDTH; i++) {
				tco.p[0] = (double)i * sx + x1;
				if ((dospline && rs->spline_interp(rs, &tco))
				 || (!dospline && rs->interp(rs, &tco))) {
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
	
		if (markers) {
			/* Mark verticies in red */
			for(k = 0; k < npoints; k++) {
				j = (int)((HEIGHT * (y2 - test_points[k].p[1])/(y2 - y1)) + 0.5);
				i = (int)((WIDTH * (test_points[k].p[0] - x1)/(x2 - x1)) + 0.5);
				pa[j][i][0] = 255;
				pa[j][i][1] = 0;
				pa[j][i][2] = 0;
			}
		}
		write_rgb_tiff(rsv == 0 ? "t2d.tif" : "t2dh.tif" ,WIDTH,HEIGHT,(unsigned char *)pa);
	}

	/* Plot out 3 slices */
	if (doplot == 1) {
		int slice;
		
		for (slice = 0; slice < 3; slice++) {
			co tp;	/* Test point */
			double x[PLOTRES];
			double ya[PLOTRES];
			double yb[PLOTRES];
			double xx,yy;
			double x1,x2,y1,y2;
			double sx,sy;
			int i,n;
			
			/* Set up slice to plot */
			if (slice == 0) {
				printf("Slice along x = 0.5\n");
				x1 = 0.5; y1 = 0.0;
				x2 = 0.5; y2 = 1.0;
				n = PLOTRES;
			} else if (slice == 1) {
				printf("Slice along y = 0.5\n");
				x1 = 0.0; y1 = 0.5;
				x2 = 1.0; y2 = 0.5;
				n = PLOTRES;
			} else {
				printf("Slice along x = y\n");
				x1 = 0.0; y1 = 0.0;
				x2 = 1.0; y2 = 1.0;
				n = PLOTRES;
			}

			sx = (x2 - x1)/n;
			sy = (y2 - y1)/n;
			
			xx = x1;
			yy = y1;
			for (i = 0; i < n; i++) {
				double vv = i/(n-1.0);
				x[i] = vv;
				tp.p[0] = xx;
				tp.p[1] = yy;

				if ((dospline && rss->spline_interp(rss, &tp))
				 || (!dospline && rss->interp(rss, &tp)))
					tp.v[0] = -0.1;
				ya[i] = tp.v[0];

				if (doh) {
					if ((dospline && rss2->spline_interp(rss2, &tp))
					 || (!dospline && rss2->interp(rss2, &tp)))
						tp.v[0] = -0.1;
					yb[i] = tp.v[0];
				}

				xx += sx;
				yy += sy;
			}

			/* Plot the result */
			if (doh)
				do_plot(x,ya,yb,NULL,n);
			else
				do_plot(x,ya,NULL,NULL,n);
		}
	} else if (doplot == 2) {		/* Plot a given slice */
		co tp;	/* Test point */
		double x[PLOTRES];
		double ya[PLOTRES];
		double yb[PLOTRES];
		double xx,yy;
		double x1,x2,y1,y2;
		double sx,sy;
		int i,n;
		
		x1 = plotpts[0][0];
		y1 = plotpts[0][1];
		x2 = plotpts[1][0];
		y2 = plotpts[1][1];

		printf("Slice from %f,%f to %f,%f\n",x1,y1,x2,y2);
		n = PLOTRES;

		sx = (x2 - x1)/n;
		sy = (y2 - y1)/n;
		
		xx = x1;
		yy = y1;
		for (i = 0; i < n; i++) {
			double vv = i/(n-1.0);
			x[i] = vv;
			tp.p[0] = xx;
			tp.p[1] = yy;

			if ((dospline && rss->spline_interp(rss, &tp))
			 || (!dospline && rss->interp(rss, &tp)))
				tp.v[0] = -0.1;
			ya[i] = tp.v[0];

			if (doh) {
				if ((dospline && rss2->spline_interp(rss2, &tp))
				 || (!dospline && rss2->interp(rss2, &tp)))
					tp.v[0] = -0.1;
				yb[i] = tp.v[0];
			}

			xx += sx;
			yy += sy;
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
			if (dospline)
				rss->spline_interp(rss, &tco);
			else
				rss->interp(rss, &tco);

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
