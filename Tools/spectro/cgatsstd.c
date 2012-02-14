
/* 
 * cgats library stdio and malloc utility classes.
 * Version 2.05
 *
 * Author: Graeme W. Gill
 * Date:   2002/10/24
 *
 * Copyright 2002, Graeme W. Gill
 * All rights reserved.
 *
 * This material is licensed with an "MIT" free use license:-
 * see the License.txt file in this directory for licensing details.
 *
 * These are kept in a separate file to allow them to be
 * selectively ommitted from the cgats library.
 *
 */

#ifndef COMBINED_STD

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "cgats.h"

#ifdef NEVER	/* Not sure if this is needed with some combinations */

/* Implimentation function - register an error */
/* Return the error number */
/* [Note that this is a duplicate of the one in cgats.c] */
static int
err(cgats *p, int errc, const char *fmt, ...) {
	va_list args;

	p->errc = errc;
	va_start(args, fmt);
	vsprintf(p->err, fmt, args);
	va_end(args);

	/* If this is the first registered error */
	if (p->ferrc != 0) {
		p->ferrc = p->errc;
		strcpy(p->ferr, p->err);
	}

	return errc;
}

#endif /* NEVER */

#endif /* !COMBINED_STD */

#if defined(SEPARATE_STD) || defined(COMBINED_STD)

/* Create an empty cgats object, default standard allocator */
cgats *new_cgats(void) {
	cgats *p;
	cgatsAlloc *al;			/* memory allocator */

	if ((al = new_cgatsAllocStd()) == NULL)
		return NULL;

	if ((p = new_cgats_al(al)) == NULL) {
		al->del(al);
		return NULL;
	}

	p->del_al = 1;			/* Get cgets->del to cleanup allocator */
	return p;
}

/* Read a cgats file into structure */
/* Return non-zero if there was an error */
/* and set p->errc * p->err */
CGATS_STATIC
int
cgats_read_name(cgats *p, const char *filename) {
	int rv;
	cgatsFile *fp;

	p->errc = 0;
	p->err[0] = '\000';

	if ((fp = new_cgatsFileStd_name(filename, "r")) == NULL)
		return err(p,-1,"Unable to open file '%s' for reading",filename);
	rv = p->read(p, fp);
	fp->del(fp);

	return rv;
}

/* Write a cgats file into structure */
/* Return -ve, errc & err if there was an error */
CGATS_STATIC
int
cgats_write_name(cgats *p, const char *filename) {
	int rv;
	cgatsFile *fp;

	if ((fp = new_cgatsFileStd_name(filename, "w")) == NULL)
		return err(p,-1,"Unable to open file '%s' for writing",filename);
	rv = p->write(p, fp);
	fp->del(fp);

	return rv;
}

#endif /* defined(SEPARATE_STD) || defined(COMBINED_STD) */
