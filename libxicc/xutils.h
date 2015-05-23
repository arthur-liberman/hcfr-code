#ifndef XUTILS_H
#define XUTILS_H

/* 
 * xicc standalone utilities.
 *
 * Author:  Graeme W. Gill
 * Date:    28/6/00
 * Version: 1.00
 *
 * Copyright 2000 - 2006 Graeme W. Gill
 * All rights reserved.
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License.txt file for licencing details.
 *
 * Based on the old iccXfm class.
 */

/*
 * These utilities, while living in xicc, are meant to
 * not depend on other modules.
 */

/* Return a lut resolution given the input dimesion and quality */
/* Input dimension [0-8], quality: low, medium, high, very high. */
/* A returned value of 0 indicates illegal.  */
int dim_to_clutres(int dim, int quality);


/* Open an ICC file or a TIFF or JPEG  file with an embedded ICC profile for reading. */
/* Return NULL on error */
icc *read_embedded_icc(char *file_name);

#endif /* XUTILS_H */



































