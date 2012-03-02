
/* Provide a system independent glob type function */

/*************************************************************************
 Copyright 2011 Graeme W. Gill

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

 *************************************************************************/

#if defined (NT)
# if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0501
#  if defined _WIN32_WINNT
#   undef _WIN32_WINNT
#  endif
#  define _WIN32_WINNT 0x0501
# endif
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <io.h>
#endif

#if defined (UNIX) || defined(__APPLE__)
# include <unistd.h>
# include <glob.h>
# include <pthread.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "aglob.h"

/* Create the aglob */
/* Return nz on malloc error */
int aglob_create(aglob *g, char *spath) {
#ifdef NT
	char *pp;
	int rlen;
	/* Figure out where the filename starts */
	if ((pp = strrchr(spath, '/')) == NULL
	 && (pp = strrchr(spath, '\\')) == NULL)
		rlen = 0;
	else
		rlen = pp - spath + 1;

	if ((g->base = malloc(rlen + 1)) == NULL)
		return 1;

	memmove(g->base, spath, rlen);
	g->base[rlen] = '\000';

	g->first = 1;
    g->ff = _findfirst(spath, &g->ffs);
#else /* UNIX */
	g->rv = glob(spath, GLOB_NOSORT, NULL, &g->g);
	if (g->rv == GLOB_NOSPACE)
		return 1;
	g->ix = 0;
#endif
	g->merr = 0;
	return 0;
}

/* Return an allocated string of the next match. */
/* Return NULL if no more matches */
char *aglob_next(aglob *g) {
	char *fpath;

#ifdef NT
	if (g->ff == -1L) {
		return NULL;
	}
	if (g->first == 0) {
		if (_findnext(g->ff, &g->ffs) != 0) {
			return NULL;
		}
	}
	g->first = 0;

	/* Convert match filename to full path */
	if ((fpath = malloc(strlen(g->base) + strlen(g->ffs.name) + 1)) == NULL) {
		g->merr = 1;
		return NULL;
	}
	strcpy(fpath, g->base);
	strcat(fpath, g->ffs.name);
	return fpath;
#else
	if (g->rv != 0 || g->ix >= g->g.gl_pathc)
		return NULL;
	if ((fpath = strdup(g->g.gl_pathv[g->ix])) == NULL) {
		g->merr = 1;
		return NULL;
	}
	g->ix++;
	return fpath;
#endif
}

void aglob_cleanup(aglob *g) {
#ifdef NT
	if (g->ff != -1L)
		_findclose(g->ff);
	free(g->base);
#else /* UNIX */
	if (g->rv == 0)
		globfree(&g->g);
#endif
}

