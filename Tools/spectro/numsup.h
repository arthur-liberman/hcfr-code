#ifndef NUMSUP_H

/* Numerical routine general support declarations */

/*
 * Copyright 2000-2010 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENSE Version 2 or later :-
 * see the License2.txt file for licencing details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
#include <math.h>

#ifdef __cplusplus
	extern "C" {
#endif

/* Some default math limits and constants */
#ifndef DBL_EPSILON
#define DBL_EPSILON     2.2204460492503131e-016		/* 1.0+DBL_EPSILON != 1.0 */
#endif
#ifndef DBL_MIN
#define DBL_MIN         2.2250738585072014e-308		/* IEEE 64 bit min value */
#endif
#ifndef DBL_MAX
#define DBL_MAX         1.7976931348623158e+308		/* IEEE 64 bit max value */
#endif
#ifndef DBL_PI
#define DBL_PI         3.1415926535897932384626433832795
#endif

/* Globals used to hold certain information */
extern char *exe_path;		/* Path leading to executable, not including exe name itself. */
							/* Always uses '/' separator */
							/* Malloce'd - won't be freed on exit (intended leak) */
						
extern char *error_program;
extern FILE *error_out;
extern FILE *warn_out;
extern FILE *verbose_out;
extern int verbose_level;

extern void set_exe_path(char *arg0);
extern void (*error)(char *fmt, ...);
extern void (*warning)(char *fmt, ...);
extern void (*verbose)(int level, char *fmt, ...);

extern int ret_null_on_malloc_fail;

extern void check_if_not_interactive();
extern int not_interactive;
extern char cr_char;

/* Numerical recipes vector/matrix support functions */
/* Note that the index arguments are the inclusive low and high values */

/* Double */
double *dvector(int nl,int nh);
double *dvectorz(int nl,int nh);
void free_dvector(double *v,int nl,int nh);

double **dmatrix(int nrl, int nrh, int ncl, int nch);
double **dmatrixz(int nrl, int nrh, int ncl, int nch);
void free_dmatrix(double **m, int nrl, int nrh, int ncl, int nch);

double **dhmatrix(int nrl, int nrh, int ncl, int nch);
double **dhmatrixz(int nrl, int nrh, int ncl, int nch);
void free_dhmatrix(double **m, int nrl, int nrh, int ncl, int nch);

void copy_dmatrix(double **dst, double **src, int nrl, int nrh, int ncl, int nch);
void copy_dmatrix_to3x3(double dst[3][3], double **src, int nrl, int nrh, int ncl, int nch);

double **convert_dmatrix(double *a,int nrl,int nrh,int ncl,int nch);
void free_convert_dmatrix(double **m,int nrl,int nrh,int ncl,int nch);


/* Float */
float *fvector(int nl,int nh);
float *fvectorz(int nl,int nh);
void free_fvector(float *v,int nl,int nh);

float **fmatrix(int nrl, int nrh, int ncl, int nch);
float **fmatrixz(int nrl, int nrh, int ncl, int nch);
void free_fmatrix(float **m, int nrl, int nrh, int ncl, int nch);


/* Int */
int *ivector(int nl,int nh);
int *ivectorz(int nl,int nh);
void free_ivector(int *v,int nl,int nh);

int **imatrix(int nrl,int nrh,int ncl,int nch);
int **imatrixz(int nrl,int nrh,int ncl,int nch);
void free_imatrix(int **m,int nrl,int nrh,int ncl,int nch);


/* Short */
short *svector(int nl, int nh);
short *svectorz(int nl, int nh);
void free_svector(short *v, int nl, int nh);

short **smatrix(int nrl,int nrh,int ncl,int nch);
short **smatrixz(int nrl,int nrh,int ncl,int nch);
void free_smatrix(short **m,int nrl,int nrh,int ncl,int nch);

#ifdef NT
void sleep(int seconds);
#endif

/* =========================================================== */
/* Should this go in spectro/conv.h ??                         */
/* =========================================================== */
/* Platform specific primitive defines. */
/* This really needs checking for each different platform. */
/* Using C99 and MSC covers a lot of cases, */
/* and the fallback default is pretty reliable with modern compilers and machines. */
/* (duplicated in icc.h) */ 

#if (__STDC_VERSION__ >= 199901L)	/* C99 */

#include <stdint.h> 

#define INR8   int8_t		/* 8 bit signed */
#define INR16  int16_t		/* 16 bit signed */
#define INR32  int32_t		/* 32 bit signed */
#define INR64  int64_t		/* 64 bit signed - not used in icclib */
#define ORD8   uint8_t		/* 8 bit unsigned */
#define ORD16  uint16_t		/* 16 bit unsigned */
#define ORD32  uint32_t		/* 32 bit unsigned */
#define ORD64  uint64_t		/* 64 bit unsigned - not used in icclib */

#define PNTR intptr_t

/* printf format precision specifier */
#define PF64PREC "ll"

/* Constant precision specifier */
#define CF64PREC "LL"

#else  /* !__STDC_VERSION__ */
#ifdef _MSC_VER

#define INR8   __int8				/* 8 bit signed */
#define INR16  __int16				/* 16 bit signed */
#define INR32  __int32				/* 32 bit signed */
#define INR64  __int64				/* 64 bit signed - not used in icclib */
#define ORD8   unsigned __int8		/* 8 bit unsigned */
#define ORD16  unsigned __int16		/* 16 bit unsigned */
#define ORD32  unsigned __int32		/* 32 bit unsigned */
#define ORD64  unsigned __int64		/* 64 bit unsigned - not used in icclib */

#define PNTR UINT_PTR

#define vsnprintf _vsnprintf

/* printf format precision specifier */
#define PF64PREC "I64"

/* Constant precision specifier */
#define CF64PREC "LL"

#else  /* !_MSC_VER */

/* The following works on a lot of modern systems, including */
/* LP64 model 64 bit modes */

#define INR8   signed char		/* 8 bit signed */
#define INR16  signed short		/* 16 bit signed */
#define INR32  signed int		/* 32 bit signed */
#define ORD8   unsigned char	/* 8 bit unsigned */
#define ORD16  unsigned short	/* 16 bit unsigned */
#define ORD32  unsigned int		/* 32 bit unsigned */

#ifdef __GNUC__
#define INR64  long long			/* 64 bit signed - not used in icclib */
#define ORD64  unsigned long long	/* 64 bit unsigned - not used in icclib */

/* printf format precision specifier */
#define PF64PREC "ll"

/* Constant precision specifier */
#define CF64PREC "LL"

#endif	/* __GNUC__ */

#define PNTR unsigned long 

#endif /* !_MSC_VER */
#endif /* !__STDC_VERSION__ */
/* =========================================================== */

/* Cast a native double to an IEEE754 encoded single precision value, */
/* in a platform independent fashion. */
ORD32 doubletoIEEE754(double d);

/* Cast an IEEE754 encoded single precision value to a native double, */
/* in a platform independent fashion. */
double IEEE754todouble(ORD32 ip);


/* Cast a native double to an IEEE754 encoded double precision value, */
/* in a platform independent fashion. */
ORD64 doubletoIEEE754_64(double d);

/* Cast an IEEE754 encoded double precision value to a native double, */
/* in a platform independent fashion. */
double IEEE754_64todouble(ORD64 ip);

/* Return a string representation of a 64 bit ctime. */
/* A static buffer is used. There is no \n at the end */
char *ctime_64(const ORD64 *timer);

#ifdef __cplusplus
	}
#endif

#define NUMSUP_H
#endif /* NUMSUP_H */
