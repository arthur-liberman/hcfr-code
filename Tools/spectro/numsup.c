
/* General Numerical routines support functions, */
/* and common Argyll support functions. */
/* (Perhaps these should be moved out of numlib at some stange ?) */

/*
 * Copyright 1997 - 2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#if defined (NT)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef UNIX
#include <unistd.h>
#include <sys/param.h>
#endif

#include "numsup.h"

/* 
 * TODO: Should probably break all the non-numlib stuff out into
 *       a separate library, so that it can be ommitted.
 *       Or enhance it so that numerical callers of error()
 *       can get a callback on out of memory etc. ???
 *
 */

/* Globals */

char *exe_path = "\000";			/* Directory executable resides in ('/' dir separator) */
char *error_program = "Unknown";	/* Name to report as responsible for an error */
#define ERROR_OUT_DEFAULT stderr
#define WARN_OUT_DEFAULT stderr
#define VERBOSE_OUT_DEFAULT stdout
int verbose_level = 6;			/* Current verbosity level */
								/* 0 = none */
								/* !0 = diagnostics */

/* Should Vector/Matrix Support functions return NULL on error, */
/* or call error() ? */
int ret_null_on_malloc_fail = 0;	/* Call error() */

/******************************************************************/
/* Executable path routine. Sets default error_program too. */
/******************************************************************/

/* Pass in argv[0] from main() */
/* Sets the error_program name too */
void set_exe_path(char *argv0) {
	int i;

	error_program = argv0;
	i = strlen(argv0);
	if ((exe_path = malloc(i + 5)) == NULL)
		error("set_exe_path: malloc %d bytes failed",i+5);
	strcpy(exe_path, argv0);

#ifdef NT	/* CMD.EXE doesn't give us the full path in argv[0] :-( */
			/* so we need to fix this */
	{
		HMODULE mh;
		char *tpath = NULL;
		int pl;

		/* Add an .exe extension if it is missing */
		if (i < 4 || _stricmp(exe_path +i -4, ".exe") != 0)
			strcat(exe_path, ".exe");

		if ((mh = GetModuleHandle(exe_path)) == NULL)
			error("set_exe_path: GetModuleHandle '%s' failed",exe_path);
		
		/* Retry until we don't truncate the returned path */
		for (pl = 100; ; pl *= 2) {
			if (tpath != NULL)
				free(tpath);
			if ((tpath = malloc(pl)) == NULL)
				error("set_exe_path: malloc %d bytes failed",pl);
			if ((i = GetModuleFileName(mh, tpath, pl)) == 0)
				error("set_exe_path: GetModuleFileName failed");
			if (i < pl)		/* There was enough space */
				break;
		}
		free(exe_path);
		exe_path = tpath;

		/* Convert from MSWindows to UNIX file separator convention */
		for (i = 0; ;i++) {
			if (exe_path[i] == '\000')
				break;
			if (exe_path[i] == '\\')
				exe_path[i] = '/';
		}
	}
#else		/* Neither does UNIX */

	if (*exe_path != '/') {			/* Not already absolute */
		char *p, *cp;
		if (strchr(exe_path, '/') != 0) {	/* relative path */
			cp = ".:";				/* Fake a relative PATH */
		} else  {
			cp = getenv("PATH");
		}
		if (cp != NULL) {
			int found = 0;
			while((p = strchr(cp,':')) != NULL
			 || *cp != '\000') {
				char b1[PATH_MAX], b2[PATH_MAX];
 				int ll;
				if (p == NULL)
					ll = strlen(cp);
				else
					ll = p - cp;
				if ((ll + 1 + strlen(exe_path) + 1) > PATH_MAX)
					error("set_exe_path: Search path exceeds PATH_MAX");
				strncpy(b1, cp, ll);		/* Element of path to search */
				b1[ll] = '\000';
				strcat(b1, "/");
				strcat(b1, exe_path);		/* Construct path */
				if (realpath(b1, b2)) {
					if (access(b2, 0) == 0) {	/* See if exe exits */
						found = 1;
						free(exe_path);
						if ((exe_path = malloc(strlen(b2)+1)) == NULL)
							error("set_exe_path: malloc %d bytes failed",strlen(b2)+1);
						strcpy(exe_path, b2);
						break;
					}
				}
				if (p == NULL)
					break;
				cp = p + 1;
			}
			if (found == 0)
				exe_path[0] = '\000';
		}
	}
#endif
	/* strip the executable path to the base */
	for (i = strlen(exe_path)-1; i >= 0; i--) {
		if (exe_path[i] == '/') {
			char *tpath;
			if ((tpath = malloc(strlen(exe_path + i))) == NULL)
				error("set_exe_path: malloc %d bytes failed",strlen(exe_path + i));
			strcpy(tpath, exe_path + i + 1);
			error_program = tpath;				/* Set error_program to base name */
			exe_path[i+1] = '\000';				/* (The malloc never gets free'd) */
			break;
		}
	}
	/* strip off any .exe from the error_program to be more readable */
	i = strlen(error_program);
	if (i >= 4
	 && error_program[i-4] == '.'
	 && (error_program[i-3] == 'e' || error_program[i-3] == 'E')
	 && (error_program[i-2] == 'x' || error_program[i-2] == 'X')
	 && (error_program[i-1] == 'e' || error_program[i-1] == 'E'))
		error_program[i-4] = '\000';

//printf("exe_path = '%s'\n",exe_path);
//printf("error_program = '%s'\n",error_program);
}

/******************************************************************/
/* Check "ARGYLL_NOT_INTERACTIVE" environment variable.           */
/******************************************************************/

/* Check if the "ARGYLL_NOT_INTERACTIVE" environment variable is */
/* set, and set cr_char to '\n' if it is. */

int not_interactive = 0;
char cr_char = '\r';

void check_if_not_interactive() {
	char *ev;

	if ((ev = getenv("ARGYLL_NOT_INTERACTIVE")) != NULL) {
		not_interactive = 1;
		cr_char = '\n';
	} else {
		not_interactive = 0;
		cr_char = '\r';
	}
}

/******************************************************************/
/* Default error/debug output routines */
/******************************************************************/

/* Globals - can be changed on the fly */
FILE *error_out = NULL;
FILE *warn_out = NULL;
FILE *verbose_out = NULL;

/* Basic printf type error() and warning() routines */
static void
error_imp(char *fmt, ...) {
	va_list args;

	fflush(stdout);
	if (error_out == NULL)
		error_out = ERROR_OUT_DEFAULT;
	fprintf(error_out,"%s: Error - ",error_program);
	va_start(args, fmt);
	vfprintf(error_out, fmt, args);
	va_end(args);
	fprintf(error_out, "\n");
	fflush(error_out);
	exit (1);
}

static void
warning_imp(char *fmt, ...) {
	va_list args;

	fflush(stdout);
	if (warn_out == NULL)
		warn_out = WARN_OUT_DEFAULT;
	fprintf(warn_out,"%s: Warning - ",error_program);
	va_start(args, fmt);
	vfprintf(warn_out, fmt, args);
	va_end(args);
	fprintf(warn_out, "\n");
	fflush(warn_out);
}

static void
verbose_imp(int level, char *fmt, ...) {
	va_list args;

	fflush(stdout);
	va_start(args, fmt);
	if (verbose_level >= level)
		{
		if (verbose_out == NULL)
			verbose_out = VERBOSE_OUT_DEFAULT;
		fprintf(verbose_out,"%s: ",error_program);
		vfprintf(verbose_out, fmt, args);
		fprintf(verbose_out, "\n");
		fflush(verbose_out);
		}
	va_end(args);
}

/* Globals - can be changed on the fly */
void (*error)(char *fmt, ...) = error_imp;
void (*warning)(char *fmt, ...) = warning_imp;
void (*verbose)(int level, char *fmt, ...) = verbose_imp;

/******************************************************************/
/* Numerical Recipes Vector/Matrix Support functions              */
/******************************************************************/
/* Note the z suffix versions return zero'd vectors/matricies */

/* Double Vector malloc/free */
double *dvector(
int nl,		/* Lowest index */
int nh		/* Highest index */
)	{
	double *v;

	if ((v = (double *) malloc((nh-nl+1) * sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dvector()");
	}
	return v-nl;
}

double *dvectorz(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	double *v;

	if ((v = (double *) calloc(nh-nl+1, sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dvector()");
	}
	return v-nl;
}

void free_dvector(
double *v,
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	if (v == NULL)
		return;

	free((char *) (v+nl));
}

/* --------------------- */
/* 2D Double vector malloc/free */
double **dmatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (double **) malloc((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) malloc(rows * cols * sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

double **dmatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (double **) malloc((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) calloc(rows * cols, sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

void free_dmatrix(
double **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/* --------------------- */
/* 2D diagonal half matrix vector malloc/free */
/* A half matrix must have equal rows and columns, */
/* and the column address must always be >= than the row. */
double **dhmatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if (rows != cols) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("dhmatrix() given unequal rows and columns");
	}

	if ((m = (double **) malloc((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dhmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) malloc((rows * rows + rows)/2 * sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dhmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1, j = 1; i <= nrh; i++, j++) /* Set subsequent row addresses */
		m[i] = m[i-1] + j;			/* Start with 1 entry and increment */

	return m;
}

double **dhmatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j;
	int rows, cols;
	double **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if (rows != cols) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("dhmatrix() given unequal rows and columns");
	}

	if ((m = (double **) malloc((rows + 1) * sizeof(double *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dhmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (double *) calloc((rows * rows + rows)/2, sizeof(double))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dhmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1, j = 1; i <= nrh; i++, j++) /* Set subsequent row addresses */
		m[i] = m[i-1] + j;			/* Start with 1 entry and increment */

	return m;
}

void free_dhmatrix(
double **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/* --------------------- */
/* 2D vector copy */
void copy_dmatrix(
double **dst,
double **src,
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j;
	for (j = nrl; j <= nrh; j++)
		for (i = ncl; i <= nch; i++)
			dst[j][i] = src[j][i];
}

/* Copy a matrix to a 3x3 standard C array */
void copy_dmatrix_to3x3(
double dst[3][3],
double **src,
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j;
	if ((nrh - nrl) > 2)
		nrh = nrl + 2;
	if ((nch - ncl) > 2)
		nch = ncl + 2;
	for (j = nrl; j <= nrh; j++)
		for (i = ncl; i <= nch; i++)
			dst[j][i] = src[j][i];
}

/* -------------------------------------------------------------- */
/* Convert standard C type 2D array into an indirect referenced array */
double **convert_dmatrix(
double *a,	/* base address of normal C array, ie &a[0][0] */
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i, j, nrow = nrh-nrl+1, ncol = nch-ncl+1;
	double **m;

	/* Allocate pointers to rows */
	if ((m = (double **) malloc(nrow * sizeof(double*))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in convert_dmatrix()");
	}

	m -= nrl;

	m[nrl] = a - ncl;
	for(i=1, j = nrl+1; i < nrow; i++, j++)
		m[j] = m[j-1] + ncol;
	/* return pointer to array of pointers */
	return m;
}

/* Free the indirect array reference (but not array) */
void free_convert_dmatrix(
double **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	free((char*) (m+nrl));
}

/* -------------------------- */
/* Float vector malloc/free */
float *fvector(
int nl,		/* Lowest index */
int nh		/* Highest index */
)	{
	float *v;

	if ((v = (float *) malloc((nh-nl+1) * sizeof(float))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in fvector()");
	}
	return v-nl;
}

float *fvectorz(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	float *v;

	if ((v = (float *) calloc(nh-nl+1, sizeof(float))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in fvector()");
	}
	return v-nl;
}

void free_fvector(
float *v,
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	if (v == NULL)
		return;

	free((char *) (v+nl));
}

/* --------------------- */
/* 2D Float matrix malloc/free */
float **fmatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	float **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (float **) malloc((rows + 1) * sizeof(float *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (float *) malloc(rows * cols * sizeof(float))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

float **fmatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	float **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (float **) malloc((rows + 1) * sizeof(float *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (float *) calloc(rows * cols, sizeof(float))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in dmatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

void free_fmatrix(
float **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/* ------------------ */
/* Integer vector malloc/free */
int *ivector(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	int *v;

	if ((v = (int *) malloc((nh-nl+1) * sizeof(int))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in ivector()");
	}
	return v-nl;
}

int *ivectorz(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	int *v;

	if ((v = (int *) calloc(nh-nl+1, sizeof(int))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in ivector()");
	}
	return v-nl;
}

void free_ivector(
int *v,
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	if (v == NULL)
		return;

	free((char *) (v+nl));
}


/* ------------------------------ */
/* 2D integer matrix malloc/free */

int **imatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	int **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (int **) malloc((rows + 1) * sizeof(int *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in imatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (int *) malloc(rows * cols * sizeof(int))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in imatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

int **imatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	int **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (int **) malloc((rows + 1) * sizeof(int *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in imatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (int *) calloc(rows * cols, sizeof(int))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in imatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

void free_imatrix(
int **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/* ------------------ */
/* Short vector malloc/free */
short *svector(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	short *v;

	if ((v = (short *) malloc((nh-nl+1) * sizeof(short))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in svector()");
	}
	return v-nl;
}

short *svectorz(
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	short *v;

	if ((v = (short *) calloc(nh-nl+1, sizeof(short))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in svector()");
	}
	return v-nl;
}

void free_svector(
short *v,
int nl,		/* Lowest index */
int nh		/* Highest index */
) {
	if (v == NULL)
		return;

	free((char *) (v+nl));
}


/* ------------------------------ */
/* 2D short vector malloc/free */

short **smatrix(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	short **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (short **) malloc((rows + 1) * sizeof(short *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in smatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (short *) malloc(rows * cols * sizeof(short))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in smatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

short **smatrixz(
int nrl,	/* Row low index */
int nrh,	/* Row high index */
int ncl,	/* Col low index */
int nch		/* Col high index */
) {
	int i;
	int rows, cols;
	short **m;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	rows = nrh - nrl + 1;
	cols = nch - ncl + 1;

	if ((m = (short **) malloc((rows + 1) * sizeof(short *))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in smatrix(), pointers");
	}
	m -= nrl;	/* Offset to nrl */
	m += 1;		/* Make nrl-1 pointer to main allocation, in case rows get swaped */

	if ((m[nrl-1] = (short *) calloc(rows * cols, sizeof(short))) == NULL) {
		if (ret_null_on_malloc_fail)
			return NULL;
		else
			error("Malloc failure in smatrix(), array");
	}

	m[nrl] = m[nrl-1] - ncl;		/* Set first row address, offset to ncl */
	for(i = nrl+1; i <= nrh; i++)	/* Set subsequent row addresses */
		m[i] = m[i-1] + cols;

	return m;
}

void free_smatrix(
short **m,
int nrl,
int nrh,
int ncl,
int nch
) {
	if (m == NULL)
		return;

	if (nrh < nrl)	/* Prevent failure for 0 dimension */
		nrh = nrl;
	if (nch < ncl)
		nch = ncl;

	free((char *)(m[nrl-1]));
	free((char *)(m+nrl-1));
}

/***************************/
/* Basic matrix operations */
/***************************/

/* Transpose a 0 base matrix */
void matrix_trans(double **d, double **s, int nr,  int nc) {
	int i, j;

	for (i = 0; i < nr; i++) {
		for (j = 0; j < nc; j++) {
			d[j][i] = s[i][j];
		}
	}
}

/* Multiply two 0 based matricies */
/* Return nz on matching error */
int matrix_mult(
	double **d,  int nr,  int nc,
	double **s1, int nr1, int nc1,
	double **s2, int nr2, int nc2
) {
	int i, j, k;

	/* s1 and s2 must mesh */
	if (nc1 != nr2)
		return 1;

	/* Output rows = s1 rows */
	if (nr != nr1)
		return 2;

	/* Output colums = s2 columns */
	if (nc != nc2)
		return 2;

	for (i = 0; i < nr1; i++) {
		for (j = 0; j < nc2; j++) { 
			d[i][j] = 0.0;  
			for (k = 0; k < nc1; k++) {
				d[i][j] += s1[i][k] * s2[k][j];
			}
		}
	}

	return 0;
}

/* Diagnostic */
void matrix_print(char *c, double **a, int nr,  int nc) {
	int i, j;
	printf("%s, %d x %d\n",c,nr,nc);

	for (j = 0; j < nr; j++) {
		printf(" ");
		for (i = 0; i < nc; i++) {
			printf(" %.2f",a[j][i]);
		}
		printf("\n");
	}
}


/*******************************************/
/* Platform independent IEE754 conversions */
/*******************************************/

/* Cast a native double to an IEEE754 encoded single precision value, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
ORD32 doubletoIEEE754(double d) {
	ORD32 sn = 0, ep = 0, ma;
	ORD32 id;

	/* Convert double to IEEE754 single precision. */
	/* This would be easy if we're running on an IEEE754 architecture, */
	/* but isn't generally portable, so we use ugly code: */

	if (d < 0.0) {
		sn = 1;
		d = -d;
	}
	if (d != 0.0) {
		int ee;
		ee = (int)floor(log(d)/log(2.0));
		if (ee < -126)			/* Allow for denormalized */
			ee = -126;
		d *= pow(0.5, (double)(ee - 23));
		ee += 127;
		if (ee < 1)				/* Too small */
			ee = 0;				/* Zero or denormalised */
		else if (ee > 254) {	/* Too large */
			ee = 255;			/* Infinity */
			d = 0.0;
		}
		ep = ee;
	} else {
		ep = 0;					/* Zero */
	}
	ma = ((ORD32)d) & ((1 << 23)-1);
	id = (sn << 31) | (ep << 23) | ma;

	return id;
}

/* Cast a an IEEE754 encoded single precision value to a native double, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
double IEEE754todouble(ORD32 ip) {
	double op;
	ORD32 sn = 0, ep = 0, ma;

	sn = (ip >> 31) & 0x1;
	ep = (ip >> 23) & 0xff;
	ma = ip & 0x7fffff;

	if (ep == 0) { 		/* Zero or denormalised */
		op = (double)ma/(double)(1 << 23);
		op *= pow(2.0, (-126.0));
	} else {
		op = (double)(ma | (1 << 23))/(double)(1 << 23);
		op *= pow(2.0, (((int)ep)-127.0));
	}
	if (sn)
		op = -op;
	return op;
}

/* Cast a native double to an IEEE754 encoded double precision value, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
ORD64 doubletoIEEE754_64(double d) {
	ORD32 sn = 0, ep = 0;
	ORD64 ma, id;

	/* Convert double to IEEE754 double precision. */
	/* This would be easy if we know we're running on an IEEE754 architecture, */
	/* but isn't generally portable, so we use ugly code: */

	if (d < 0.0) {
		sn = 1;
		d = -d;
	}
	if (d != 0.0) {
		int ee;
		ee = (int)floor(log(d)/log(2.0));
		if (ee < -1022)			/* Allow for denormalized */
			ee = -1022;
		d *= pow(0.5, (double)(ee - 52));
		ee += 1023;				/* Exponent bias */
		if (ee < 1)				/* Too small */
			ee = 0;				/* Zero or denormalised */
		else if (ee > 2046) {	/* Too large */
			ee = 2047;			/* Infinity */
			d = 0.0;
		}
		ep = ee;
	} else {
		ep = 0;					/* Zero */
	}
	ma = ((ORD64)d) & (((ORD64)1 << 52)-1);
	id = ((ORD64)sn << 63) | ((ORD64)ep << 52) | ma;

	return id;
}

/* Cast a an IEEE754 encode double precision value to a native double, */
/* in a platform independent fashion. (ie. This works even */
/* on the rare platforms that don't use IEEE 754 floating */
/* point for their C implementation) */
double IEEE754_64todouble(ORD64 ip) {
	double op;
	ORD32 sn = 0, ep = 0;
	INR64 ma;

	sn = (ip >> 63) & 0x1;
	ep = (ip >> 52) & 0x7ff;
	ma = ip & (((INR64)1 << 52)-1);

	if (ep == 0) { 		/* Zero or denormalised */
		op = (double)ma/(double)((INR64)1 << 52);
		op *= pow(2.0, -1022.0);
	} else {
		op = (double)(ma | ((INR64)1 << 52))/(double)((INR64)1 << 52);
		op *= pow(2.0, (((int)ep)-1023.0));
	}
	if (sn)
		op = -op;
	return op;
}

/* Return a string representation of a 32 bit ctime. */
/* A static buffer is used. There is no \n at the end */
char *ctime_32(const INR32 *timer) {
	char *rv;
#if defined(_MSC_VER) && __MSVCRT_VERSION__ >= 0x0601
	rv = _ctime32((const __time32_t *)timer);
#else
	time_t timerv = (time_t) *timer;		/* May case to 64 bit */
	rv = ctime(&timerv);
#endif

	if (rv != NULL)
		rv[strlen(rv)-1] = '\000';

	return rv;
}

/* Return a string representation of a 64 bit ctime. */
/* A static buffer is used. There is no \n at the end */
char *ctime_64(const INR64 *timer) {
	char *rv;
#if defined(_MSC_VER) && __MSVCRT_VERSION__ >= 0x0601
	rv = _ctime64((const __time64_t *)timer);
#else
	time_t timerv;

	if (sizeof(time_t) == 4 && *timer > 0x7fffffff)
		return NULL;
	timerv = (time_t) *timer;			/* May truncate to 32 bits */
	rv = ctime(&timerv);
#endif

	if (rv != NULL)
		rv[strlen(rv)-1] = '\000';

	return rv;
}
