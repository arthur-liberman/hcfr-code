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

#ifdef NT
#include <basetsd.h>		/* So jpg header doesn't define INT32 */
# if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0501
#  undef _WIN32_WINNT 
#  define _WIN32_WINNT 0x0501
# endif
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif
#ifdef UNIX
# include <pthread.h>
#endif

#ifdef __cplusplus
	extern "C" {
#endif

/* =========================================================== */
/* Platform specific primitive defines. */
/* This really needs checking for each different platform. */
/* Using C99 and MSC covers a lot of cases, */
/* and the fallback default is pretty reliable with modern compilers and machines. */
/* Note that MSWin is LLP64 == 32 bit long, while OS X/Linux is LP64 == 64 bit long. */
/* so long shouldn't really be used in any code.... */
/* (duplicated in icc.h) */ 

/* Use __P64__ as cross platform 64 bit pointer #define */
#if defined(__LP64__) || defined(__ILP64__) || defined(__LLP64__) || defined(_WIN64)
# define __P64__ 1
#endif

#ifndef ORD32

#if (__STDC_VERSION__ >= 199901L)	/* C99 */		\
 || defined(_STDINT_H_) || defined(_STDINT_H)		\
 || defined(_SYS_TYPES_H)

#include <stdint.h> 

#define INR8   int8_t		/* 8 bit signed */
#define INR16  int16_t		/* 16 bit signed */
#define INR32  int32_t		/* 32 bit signed */
#define INR64  int64_t		/* 64 bit signed */
#define ORD8   uint8_t		/* 8 bit unsigned */
#define ORD16  uint16_t		/* 16 bit unsigned */
#define ORD32  uint32_t		/* 32 bit unsigned */
#define ORD64  uint64_t		/* 64 bit unsigned */

#define PNTR intptr_t

#define PF64PREC "ll"		/* printf format precision specifier */
#define CF64PREC "LL"		/* Constant precision specifier */

#ifndef ATTRIBUTE_NORETURN
# ifdef _MSC_VER
#  define ATTRIBUTE_NORETURN __declspec(noreturn)
# else
#  define ATTRIBUTE_NORETURN __attribute__((noreturn))
# endif
#endif

#ifndef INLINE
# ifdef _MSC_VER
#  define INLINE __inline
# else
#  define INLINE inline
# endif
#endif

#else  /* !__STDC_VERSION__ */

#ifdef _MSC_VER

#define INR8   __int8				/* 8 bit signed */
#define INR16  __int16				/* 16 bit signed */
#define INR32  __int32				/* 32 bit signed */
#define INR64  __int64				/* 64 bit signed */
#define ORD8   unsigned __int8		/* 8 bit unsigned */
#define ORD16  unsigned __int16		/* 16 bit unsigned */
#define ORD32  unsigned __int32		/* 32 bit unsigned */
#define ORD64  unsigned __int64		/* 64 bit unsigned */

#define PNTR UINT_PTR

#define PF64PREC "I64"				/* printf format precision specifier */
#define CF64PREC "LL"				/* Constant precision specifier */

#ifndef ATTRIBUTE_NORETURN
# define ATTRIBUTE_NORETURN __declspec(noreturn)
#endif

#ifndef INLINE
# define INLINE __inline
#endif

#else  /* !_MSC_VER */

/* The following works on a lot of modern systems, including */
/* LLP64 and LP64 models, but won't work with ILP64 which needs int32 */

#define INR8   signed char		/* 8 bit signed */
#define INR16  signed short		/* 16 bit signed */
#define INR32  signed int		/* 32 bit signed */
#define ORD8   unsigned char	/* 8 bit unsigned */
#define ORD16  unsigned short	/* 16 bit unsigned */
#define ORD32  unsigned int		/* 32 bit unsigned */

#ifdef __GNUC__
# ifdef __LP64__	/* long long could be 128 bit ? */
#  define INR64  long				/* 64 bit signed */
#  define ORD64  unsigned long		/* 64 bit unsigned */
#  define PF64PREC "l"			/* printf format precision specifier */
#  define CF64PREC "L"			/* Constant precision specifier */
# else
#  define INR64  long long			/* 64 bit signed */
#  define ORD64  unsigned long long	/* 64 bit unsigned */
#  define PF64PREC "ll"			/* printf format precision specifier */
#  define CF64PREC "LL"			/* Constant precision specifier */
# endif /* !__LP64__ */
#endif /* __GNUC__ */

#define PNTR unsigned long 

#ifndef ATTRIBUTE_NORETURN
# define ATTRIBUTE_NORETURN __attribute__((noreturn))
#endif
#ifndef INLINE
#  define INLINE inline
#endif /* INLINE */

#endif /* !_MSC_VER */
#endif /* !__STDC_VERSION__ */
#endif /* !ORD32 */

/* =========================================================== */
/* System compatibility #defines */
#if defined (NT)

#ifndef sys_stat
# define sys_stat _stat
#endif
#ifndef sys_mkdir
# define sys_mkdir _mkdir
#endif
#ifndef sys_read
# define sys_read _read
#endif
#ifndef sys_utime
# define sys_utime _utime
# define sys_utimbuf _utimbuf
#endif
#ifndef sys_access
# define sys_access _access
#endif

#ifndef snprintf
# define snprintf _snprintf
# define vsnprintf _vsnprintf
#endif
#ifndef stricmp
# define stricmp _stricmp
#endif

#endif	/* NT */

#if defined (UNIX)

#ifndef sys_stat
# define sys_stat stat
#endif
#ifndef sys_mkdir
# define sys_mkdir mkdir
#endif
#ifndef sys_read
# define sys_read read
#endif
#ifndef sys_utime
# define sys_utime utime
# define sys_utimbuf utimbuf
#endif
#ifndef sys_access
# define sys_access access
#endif

#ifndef stricmp
# define stricmp strcasecmp
#endif

#endif	/* UNIX */

/* =========================================================== */
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

/* =========================================================== */
/* General verbose, debug, warning and error logging object and functions */

/* 

	Verbose is simply informational for the end user during normal
	operation, typically on stdout or in a text information log.
	This will be logged to the verbose log stream.  Level:

	 0   = Off
	 1   = Brief progress and informational messages
	 2   = Much more verbose information
	 3+  = Very verbose detail

	Debug output is for the developer, to trace what has happened so
	as to help diagnose a fault. This will be logged to the debug log stream.
	Level:

	 0   = Off
	 1   = Brief debug info and any internal errors that may be significant
	       if something subsequently fails.
	 1-5 = Application internals at increasing level of detail
	 2-6 = Driver level.(overlaps app & coms)
	 6-7 = high level communications
	 8-9 = low level communications.
	
	Warning is a serious internal fault that is going to be ignored at the
	point it is noticed, but may explain any unexpected behaviour.
	It will be reported to the vebose, debug and error log streams.

	An error is something that is regarded as fatal at the point it
	is noticed. It will be reported to the vebose, debug and error log
	streams. The error code and error message will be latched within the
	log object if it is the first error logged. It can (theoretically) be
	treated as a warning at a higher calling level by calling
	a1logue (unlatch error) to reset the error code and message.

 */

#define A1_LOG_BUFSIZE 500


struct _a1log {
	int refc;					/* Reference count */
	char *tag;					/* Optional tag name */
	int verb;					/* Current verbosity level (public) */
	int debug;					/* Current debug level (public) */

	void *cntx;					/* Context to provide to log functions */
	void (*logv)(void *cntx, struct _a1log *p, char *fmt, va_list args);
								/* Implementation of log verbose */
	void (*logd)(void *cntx, struct _a1log *p, char *fmt, va_list args);
								/* Implementation of log debug */
	void (*loge)(void *cntx, struct _a1log *p, char *fmt, va_list args);
								/* Implementation of log warning/error */

	int errc; 					/* error code */
	char errm[A1_LOG_BUFSIZE];	/* error message (public) */

#ifdef NT
	CRITICAL_SECTION lock;
#endif
#ifdef UNIX
	pthread_mutex_t lock;
#endif
}; typedef struct _a1log a1log;
	
	

/* If log NULL, allocate a new log and return it, */
/* otherwise increment reference count and return existing log, */
/* exit() if malloc fails. */
a1log *new_a1log(
	a1log *log,						/* Existing log to reference, NULL if none */
	int verb,						/* Verbose level to set */
	int debug,						/* Debug level to set */
	void *cntx,						/* Function context value */
	/* Debug log function to call - stdout if NULL */
	void (*logv)(void *cntx, a1log *p, char *fmt, va_list args),
	/* Debug log function to call - stderr if NULL */
	void (*logd)(void *cntx, a1log *p, char *fmt, va_list args),
	/* Warning/error Log function to call - stderr if NULL */
	void (*loge)(void *cntx, a1log *p, char *fmt, va_list args)
);

/* Same as above but set default functions */
a1log *new_a1log_d(a1log *log);

/* Decrement reference count and free log. */
/* Returns NULL */
a1log *del_a1log(a1log *log);

/* Set the debug logging level. */
void a1log_debug(a1log *log, int level);

/* Set the vebosity level. */
void a1log_verb(a1log *log, int level);

/* Set the tag. Note that the tag string is NOT copied, just referenced */
void a1log_tag(a1log *log, char *tag);

/* Log a verbose message if level >= verb */
void a1logv(a1log *log, int level, char *fmt, ...);

/* Log a debug message if level >= debug */
void a1logd(a1log *log, int level, char *fmt, ...);

/* log a warning message to the error output  */
void a1logw(a1log *log, char *fmt, ...);

/* log an error message,  */
/* ecode = system, icoms or instrument error */
void a1loge(a1log *log, int ecode, char *fmt, ...);

/* Unlatch an error message. */
/* This resets errc and errm */
void a1logue(a1log *log);

/* Print bytes as hex to debug log */
/* base is the base of the displayed offset */
void adump_bytes(a1log *log, char *pfx, unsigned char *buf, int base, int len);

/* =========================================================== */
/* Globals used to hold certain information */
extern char *exe_path;		/* Path leading to executable, not including exe name itself. */
							/* Always uses '/' separator */
							/* Malloce'd - won't be freed on exit (intended leak) */
						
extern a1log *g_log;		/* Default log */

/* These legacy functions that now call through to the default log */
#define error_program g_log->tag
extern void set_exe_path(char *arg0);

extern void ATTRIBUTE_NORETURN error(char *fmt, ...);
extern void warning(char *fmt, ...);
extern void verbose(int level, char *fmt, ...);

extern int ret_null_on_malloc_fail;

extern void check_if_not_interactive();
extern int not_interactive;
extern char cr_char;

/* =========================================================== */


/* =========================================================== */

/* reallocate and clear new allocation */
void *recalloc(		/* Return new address */
void *ptr,			/* Current address */
size_t cnum,		/* Current number and unit size */
size_t csize,
size_t nnum,		/* New number and unit size */
size_t nsize
); 

/* =========================================================== */

#if defined(__APPLE__)

/* Tell App Nap that this is user initiated */
void osx_userinitiated_start();

/* Done with user initiated */
void osx_userinitiated_end();

/* Tell App Nap that this is latency critical */
void osx_latencycritical_start();

/* Done with latency critical */
void osx_latencycritical_end();

#endif	/* __APPLE__ */

/* =========================================================== */

/* Numerical recipes vector/matrix support functions */
/* Note that the index arguments are the inclusive low and high values */

/* Double */
double *dvector(int nl,int nh);
double *dvectorz(int nl,int nh);
void free_dvector(double *v,int nl,int nh);

double **dmatrix(int nrl, int nrh, int ncl, int nch);
double **dmatrixz(int nrl, int nrh, int ncl, int nch);
void free_dmatrix(double **m, int nrl, int nrh, int ncl, int nch);
void dmatrix_reset(double **m, int nrl, int nrh, int ncl, int nch);

double **dhmatrix(int nrl, int nrh, int ncl, int nch);
double **dhmatrixz(int nrl, int nrh, int ncl, int nch);
void free_dhmatrix(double **m, int nrl, int nrh, int ncl, int nch);

void copy_dmatrix(double **dst, double **src, int nrl, int nrh, int ncl, int nch);
void copy_dmatrix_to3x3(double dst[3][3], double **src, int nrl, int nrh, int ncl, int nch);

/* Convert from C matrix to matrix */
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

/* ----------------------------------------------------------- */
/* Basic matrix operations */

/* Transpose a 0 base matrix */
void matrix_trans(double **d, double **s, int nr,  int nc);

/* Transpose a 0 base symetrical matrix in place */
void sym_matrix_trans(double **m, int n);

/* Matrix multiply 0 based matricies */
int matrix_mult(
	double **d,  int nr,  int nc,
	double **s1, int nr1, int nc1,
	double **s2, int nr2, int nc2
);

/* Matrix multiply transpose of s1 by s2 */
/* 0 based matricies,  */
/* This is usefull for using results of lu_invert() */
int matrix_trans_mult(
	double **d,  int nr,  int nc,
	double **ts1, int nr1, int nc1,
	double **s2, int nr2, int nc2
);

/* Multiply a 0 based matrix by a vector */
/* d may be same as v */
int matrix_vect_mult(
	double *d, int nd,
	double **m, int nr, int nc,
	double *v, int nv
);

/* Multiply a 0 based transposed matrix by a vector */
/* d may be same as v */
int matrix_trans_vect_mult(
	double *d, int nd,
	double **m, int nr, int nc,
	double *v, int nv
);

/* Set zero based dvector */
void vect_set(double *d, double v, int len);

/* Copy zero based dvector */
void vect_cpy(double *d, double *s, int len);

/* Copy zero based dvector */
#define vect_cpy(dd, ss, len) memmove((char *)(dd), (char *)(ss), (len) * sizeof(double))

/* Negate and copy a vector, d = -v */
/* d may be same as v */
void vect_neg(double *d, double *s, int len);

/* Add two vectors, d += v */
/* d may be same as v */
void vect_add(double *d, double *v, int len);

/* Add two vectors, d = s1 + s2 */
void vect_add3(double *d, double *s1, double *s2, int len);

/* Subtract two vectors, d -= v */
/* d may be same as v */
void vect_sub(double *d, double *v, int len);

/* Subtract two vectors, d =  s1 - s2 */
void vect_sub3(double *d, double *s1, double *s2, int len);

/* Invert and copy a vector, d = 1/s */
void vect_invert(double *d, double *s, int len);

/* Multiply the elements of two vectors, d = s1 * s2 */
void vect_mul3(double *d, double *s1, double *s2, int len);

/* Blend two vectors, d = bl * s1 + (1 - bl) * s2 */
/* Blend between s0 and s1 for bl 0..1 */
void vect_blend(double *d, double *s0, double *s1, double bl, int len);

/* Scale a vector, */
/* d may be same as v */
void vect_scale(double *d, double *s, double scale, int len);

/* Scale s and add to d */
void vect_scaleadd(double *d, double *s, double scale, int len);

/* Take dot product of two vectors */
double vect_dot(double *s1, double *s2, int len);

/* Return the vectors magnitude (norm) */
double vect_mag(double *s, int len);

/* Return the magnitude (norm) of the difference between two vectors */
double vect_diffmag(double *s1, double *s2, int len);

/* Return the normalized vectors */
/* Return nz if norm is zero */
int vect_normalize(double *d, double *s, int len);

/* Return the vectors elements maximum magnitude (+ve) */
double vect_max(double *s, int len);

/* Take absolute of each element */
void vect_abs(double *d, double *s, int len);

/* Take individual elements to signed power */
void vect_spow(double *d, double *s, double pv, int len);

/* Copy zero based ivector */
#define ivect_cpy(dd, ss, len) memmove((char *)(dd), (char *)(ss), (len) * sizeof(int))

/* Set zero based ivector */
void ivect_set(int *d, int v, int len);

/* Diagnostics */
/* id identifies matrix/vector */
/* pfx used at start of each line */
/* Assumed indexed from 0 */

void adump_dmatrix(a1log *log, char *id, char *pfx, double **a, int nr,  int nc);
void adump_fmatrix(a1log *log, char *id, char *pfx, float **a, int nr,  int nc);
void adump_imatrix(a1log *log, char *id, char *pfx, int **a, int nr,  int nc);
void adump_smatrix(a1log *log, char *id, char *pfx, short **a, int nr,  int nc);

void adump_dvector(a1log *log, char *id, char *pfx, double *a, int nc);
void adump_fvector(a1log *log, char *id, char *pfx, float *a, int nc);
void adump_ivector(a1log *log, char *id, char *pfx, int *a, int nc);
void adump_svector(a1log *log, char *id, char *pfx, short *a, int nc);

/* Dump out matrix/vector as a C array to FILE */
/* id is the variable name */
/* pfx used at start of each line */
/* hb sets horizontal element limit to wrap */
/* Assumed indexed from 0 */

void acode_dmatrix(FILE *fp, char *id, char *pfx, double **a, int nr,  int nc, int hb);

/* ===================================================== */
/* C matrix support */

/* Clip a vector to the range 0.0 .. 1.0 */
/* and return any clipping margine */
double vect_ClipNmarg(int n, double *out, double *in);

/* Multiply N vector by NxN transform matrix */
/* Organization is mat[out][in] */
void vect_MulByNxN(int n, double *out, double *mat, double *in);

/* Multiply N vector by MxN transform matrix */
/* Organization is mat[out][in] */
void vect_MulByMxN(int n, int m, double *out, double *mat, double *in);

/* Multiply N vector by transposed NxM transform matrix */
/* Organization is mat[in][out] */
void vect_MulByNxM(int n, int m, double *out, double *mat, double *in);

/* Transpose an NxN matrix */
void matrix_TransposeNxN(int n, double *out, double *in);

/* Dump C type matrix */
void adump_C_dmatrix(a1log *log, char *id, char *pfx, double *a, int nr,  int nc);

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

/* Return a string representation of a 32 bit ctime. */
/* A static buffer is used. There is no \n at the end */
char *ctime_32(const INR32 *timer);

/* Return a string representation of a 64 bit ctime. */
/* A static buffer is used. There is no \n at the end */
char *ctime_64(const INR64 *timer);

/*******************************************/
/* Native to/from byte buffer functions    */
/*******************************************/

/* No overflow detection is done - numbers are clipped */

/* be = Big Endian */
/* le = Little Endian */

/* Unsigned 8 bit */
unsigned int read_ORD8(ORD8 *p);
void write_ORD8(ORD8 *p, unsigned int d);

/* Signed 8 bit */
int read_INR8(ORD8 *p);
void write_INR8(ORD8 *p, int d);

/* Unsigned 16 bit */
unsigned int read_ORD16_be(ORD8 *p);
unsigned int read_ORD16_le(ORD8 *p);
void write_ORD16_be(ORD8 *p, unsigned int d);
void write_ORD16_le(ORD8 *p, unsigned int d);

/* Signed 16 bit */
int read_INR16_be(ORD8 *p);
int read_INR16_le(ORD8 *p);
void write_INR16_be(ORD8 *p, int d);
void write_INR16_le(ORD8 *p, int d);

/* Unsigned 32 bit */
unsigned int read_ORD32_be(ORD8 *p);
unsigned int read_ORD32_le(ORD8 *p);
void write_ORD32_be(ORD8 *p, unsigned int d);
void write_ORD32_le(ORD8 *p, unsigned int d);

/* Signed 32 bit */
int read_INR32_be(ORD8 *p);
int read_INR32_le(ORD8 *p);
void write_INR32_be(ORD8 *p, int d);
void write_INR32_le(ORD8 *p, int d);

/* Unsigned 64 bit */
ORD64 read_ORD64_be(ORD8 *p);
ORD64 read_ORD64_le(ORD8 *p);
void write_ORD64_be(ORD8 *p, ORD64 d);
void write_ORD64_le(ORD8 *p, ORD64 d);

/* Signed 64 bit */
INR64 read_INR64_be(ORD8 *p);
INR64 read_INR64_le(ORD8 *p);
void write_INR64_be(ORD8 *p, INR64 d);
void write_INR64_le(ORD8 *p, INR64 d);

/*******************************************/

/* Sleep for the given number of msec */
void msec_sleep(unsigned int msec);

/* Return the current time in msec since */
/* the first invokation of msec_time() */
unsigned int msec_time();

/* Return the current time in usec */
/* (The first invokation of usec_time() returns zero) */
double usec_time();

/*******************************************/
/* Debug convenience functions (duplicated in icc) */

/* Print an int vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *debPiv(int di, int *p);

/* Print a double vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *debPdv(int di, double *p);

/* Print a double vector to a string with format. */
/* Returned static buffer is re-used every 5 calls. */
char *debPdvf(int di, char *fmt, double *p);

/* Print a float vector to a string. */
/* Returned static buffer is re-used every 5 calls. */
char *debPfv(int di, float *p);


/*******************************************/
/* Numerical diagnostics */

#ifndef isNan
#define isNan(x) ((x) != (x))
#define isNFinite(x) ( isNan(x) || (x) < DBL_MIN || DBL_MAX < (x))
#define isFinite(x) (!isNFinite(x))
#endif


#ifdef __cplusplus
	}
#endif

#define NUMSUP_H
#endif /* NUMSUP_H */
