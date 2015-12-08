
#ifndef PLOT_H

/*
 * Simple diagnostic 2d plot function
 *
 * Copyright 1998 - 2004 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 */

/* Graph order is Black = Y1, Red = Y2, Green = Y3, Blue = Y4, Yellow = Y5, Purple = Y6 */
/* Brown = Y7, Orange = Y8, Grey = Y9, White = Y10  */

/* A plot color */
typedef struct {
	float rgb[3];
} plot_col;

/* Plot up to 3 X/Y Graphs. return when the user closes the window */
/* return 0 on success, -1 on error */
int do_plot(double *x, double *y1, double *y2, double *y3, int n);

/* Plot up to 3 graphs. */
/* if dowait > 0, wait for user key */
/* if dowait < 0, wait for no seconds */
/* If xmax > xmin, use as x scale, else auto. */
/* If ymax > ymin, use as y scale, else auto. */
/* ratio is window X / Y */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int do_plot_x(double *x, double *y1, double *y2, double *y3, int n,
int dowait, double pxmin, double pxmax, double pymin, double pymax, double ratio);

/* Plot up to 3 graphs + crosses. Wait for key */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int do_plot_p(double *x, double *y1, double *y2, double *y3, int n,
              double *x4, double *y4, int m);

/* Public routines */
/* Plot up to 6 graphs */
/* return 0 on success, -1 on error */
/* Graph order is Black = Y1, Red = Y2, Green = Y3, Blue = Y4, Yellow = Y5, Purple = Y6 */
int do_plot6(double *x, double *y1, double *y2, double *y3, double *y4, double *y5, double *y6, int n);

/* Plot up to 6 graphs + optional crosses. Wait for a key */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int do_plot6p(double *x, double *y1, double *y2, double *y3, double *y4, double *y5, double *y6,
              int n, double *x7, double *y7, int m);

/* Public routines */
/* Plot up to 10 graphs */
/* return 0 on success, -1 on error */
/* Graph order is Black = Y1, Red = Y2, Green = Y3, Blue = Y4, Yellow = Y5, Purple = Y6 */
/* Brown = Y7, Orange = Y8, Grey = Y9, White = Y10 */
/* if dozero flag, make sure y range covers zero */
int do_plot10(double *x, double *y1, double *y2, double *y3, double *y4, double *y5, double *y6,
             double *y7, double *y8, double *y9, double *y10,
             int n, int dozero);

/* Plot up to 10 graphs + optional crosses. Wait for a key */
/* return 0 on success, -1 on error */
/* If n is -ve, reverse the X axis */
int do_plot10p(double *x, double *y1, double *y2, double *y3, double *y4, double *y5, double *y6,
               double *y7, double *y8, double *y9, double *y10,
               int n, double *xp, double *yp, int m);

/* Plot a bunch of vectors + points + optional colored points & notation */
/* return 0 on success, -1 on error */
/* Vectors are x1, y1 to x2, y2 with 'X' at x2, y3, */
/* Colored annotated Crosss at x3, y3. */
int do_plot_vec(double xmin, double xmax, double ymin, double ymax,
                double *x1, double *y1, double *x2, double *y2, int n,
                int dowait,
				double *x3, double *y3, plot_col *mcols, char **mtext, int m);

/* Plot a bunch of vectors + points + optional colored points & notation */
/* + optional colored vectors */
/* return 0 on success, -1 on error */
/* Vectors are x1, y1 to x2, y2 with annotated 'X' at x2, y3, */
/* Colored annotated Crosss at x3, y3. */
/* Colored vector from x4, y4 to x5, y5 */
int do_plot_vec2(double xmin, double xmax, double ymin, double ymax,
                double *x1, double *y1, double *x2, double *y2, char **ntext, int n,
                int dowait,
				double *x3, double *y3, plot_col *mcols, char **mtext, int m,
				double *x4, double *y4, double *x5, double *y5, plot_col *ocols, int o);

#define PLOT_H
#endif /* PLOT_H */
